/* Reserve all budgets in one private DB transaction before a billable attempt.
 * Failed/uncertain submissions still consume an attempt. No query/key history
 * is persisted. Unix UTC minute/day windows survive plugin/server restarts. */
static bool Search_ReserveOne(int64 owner, int kind, int64 window, int limit)
{
    sqlite3_stmt* stmt = NULL;
    bool ok = sqlite3_prepare_v2(G_DB,"INSERT OR IGNORE INTO search_budget(owner,kind,window,used) VALUES(?,?,?,0)",-1,&stmt,NULL) == SQLITE_OK;
    if (ok) { sqlite3_bind_int64(stmt,1,owner); sqlite3_bind_int(stmt,2,kind); sqlite3_bind_int64(stmt,3,window); ok = sqlite3_step(stmt) == SQLITE_DONE; }
    sqlite3_finalize(stmt); stmt = NULL;
    if (!ok || sqlite3_prepare_v2(G_DB,"UPDATE search_budget SET used=used+1 WHERE owner=? AND kind=? AND window=? AND used<?",-1,&stmt,NULL) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt,1,owner); sqlite3_bind_int(stmt,2,kind); sqlite3_bind_int64(stmt,3,window); sqlite3_bind_int(stmt,4,limit);
    ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(G_DB) == 1; sqlite3_finalize(stmt); return ok;
}
static int Search_Reserve(int64 owner, const SearchConfig* config)
{
    int64 now = (int64)time(NULL);
    if (now < 0 || sqlite3_exec(G_DB,"BEGIN IMMEDIATE",NULL,NULL,NULL) != SQLITE_OK) return 503;
    bool ok = Search_ReserveOne(owner,0,now/60,config->minute_limit) &&
        Search_ReserveOne(owner,1,now/86400,config->daily_limit) &&
        Search_ReserveOne(0,1,now/86400,config->global_daily_limit);
    int error = sqlite3_errcode(G_DB);
    if (!ok) { sqlite3_exec(G_DB,"ROLLBACK",NULL,NULL,NULL); return error == SQLITE_OK ? 429 : 503; }
    sqlite3_stmt* stmt = NULL;
    ok = sqlite3_prepare_v2(G_DB,"DELETE FROM search_budget WHERE (kind=0 AND window<?) OR (kind=1 AND window<?)",-1,&stmt,NULL) == SQLITE_OK;
    if (ok) { sqlite3_bind_int64(stmt,1,now/60-2); sqlite3_bind_int64(stmt,2,now/86400-2); ok = sqlite3_step(stmt) == SQLITE_DONE; }
    sqlite3_finalize(stmt);
    if (ok && sqlite3_exec(G_DB,"COMMIT",NULL,NULL,NULL) == SQLITE_OK) return 0;
    sqlite3_exec(G_DB,"ROLLBACK",NULL,NULL,NULL); return 503;
}
static int Search_Used(int64 owner, int kind, int64 window)
{
    sqlite3_stmt* stmt = NULL; int used = -1;
    if (sqlite3_prepare_v2(G_DB,"SELECT used FROM search_budget WHERE owner=? AND kind=? AND window=?",-1,&stmt,NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt,1,owner); sqlite3_bind_int(stmt,2,kind); sqlite3_bind_int64(stmt,3,window);
        int result = sqlite3_step(stmt); if (result == SQLITE_ROW) used = sqlite3_column_int(stmt,0); else if (result == SQLITE_DONE) used = 0;
    }
    sqlite3_finalize(stmt); return used;
}
