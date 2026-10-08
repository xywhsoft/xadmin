

// prepared sql
sqlite3_stmt* stmt_logs_all = NULL;
sqlite3_stmt* stmt_logs_sel = NULL;
sqlite3_stmt* stmt_logs_add = NULL;
sqlite3_stmt* stmt_logs_clear = NULL;
sqlite3_stmt* stmt_logs_count = NULL;
sqlite3_stmt* stmt_logs_count_sel = NULL;
static xmutex* G_LogsConfigLock;
static bool G_LogsCleanupEnabled;
static int G_LogsRetentionDays = 7;
static bool G_LogsCleanupSyncOK;
static int64 Logs_ExpiryCutoff(int days)
{
	return XAdmin_UnixNowUs() - (int64)days * 86400 * 1000000;
}
static void Logs_SyncCleanupConfig(void)
{
	bool enabled = Global_Int("admin_log_auto_cleanup") != 0;
	xrtMutexLock(G_LogsConfigLock);
	G_LogsCleanupEnabled = enabled;
	G_LogsRetentionDays = Global_Int("admin_log_retention_days");
	xrtMutexUnlock(G_LogsConfigLock);
	G_LogsCleanupSyncOK = Sched_EnsureLogCleanupTask(enabled, Global_Int("admin_log_cleanup_hour"));
	if (!G_LogsCleanupSyncOK) printf("[logs][error] failed to synchronize cleanup task: %s\n", sqlite3_errmsg(G_DB));
}
/* Worker 只用独立连接和日志配置锁，不访问请求锁或配置缓存。
 * Sched_Unit 在 Logs_Unit 前等待 worker，保证重载时的锁与回调寿命。 */
static bool Logs_RunScheduledCleanup(sqlite3* db, str* message)
{
	bool enabled; int days; sqlite3_stmt* stmt = NULL;
	xrtMutexLock(G_LogsConfigLock);
	enabled = G_LogsCleanupEnabled; days = G_LogsRetentionDays;
	xrtMutexUnlock(G_LogsConfigLock);
	if (!enabled) { *message = xrtStrDup("自动清理已关闭，本次跳过"); return true; }
	sqlite3_busy_timeout(db, 5000);
	bool ok = sqlite3_prepare_v3(db, "DELETE FROM logs WHERE createTime < ?", -1, 0, &stmt, NULL) == SQLITE_OK;
	if (ok) { sqlite3_bind_int64(stmt, 1, Logs_ExpiryCutoff(days)); ok = sqlite3_step(stmt) == SQLITE_DONE; }
	*message = ok ? xrtFormat("已清理 %d 天前的后台操作日志，共 %d 条", days, sqlite3_changes(db)) :
		xrtFormat("后台日志清理失败：%s", sqlite3_errmsg(db));
	sqlite3_finalize(stmt);
	return ok;
}



// init logs module
void Logs_Init()
{
	printf("        Logs_Init \n");
	G_LogsConfigLock = xrtMutexCreate();

	int iRet = sqlite3_prepare_v3(G_DB, "SELECT id, user, ip, uri, method, param, body, createTime FROM logs ORDER BY id DESC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_logs_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Logs_Init [stmt_logs_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT id, user, ip, uri, method, param, body, createTime FROM logs WHERE uri LIKE ? ORDER BY id DESC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_logs_sel, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Logs_Init [stmt_logs_sel] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "INSERT INTO logs (user, ip, uri, method, param, body, createTime) VALUES (?, ?, ?, ?, ?, ?, ?);", -1, SQL_PREPARE_DEFAULT, &stmt_logs_add, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Logs_Init [stmt_logs_add] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "DELETE FROM logs WHERE createTime < ?;", -1, SQL_PREPARE_DEFAULT, &stmt_logs_clear, NULL);
/* 攻击评审后的查询结构修复：窗口函数 COUNT(*) OVER() 迫使全表宽行物化
 * （10 万行实测 387ms）；计数独立成窄扫描，分页只读本页行。 */
sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM logs;", -1, SQL_PREPARE_DEFAULT, &stmt_logs_count, NULL);
sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM logs WHERE uri LIKE ?;", -1, SQL_PREPARE_DEFAULT, &stmt_logs_count_sel, NULL);
sqlite3_exec(G_DB, "CREATE INDEX IF NOT EXISTS idx_logs_createTime ON logs(createTime);", NULL, NULL, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Logs_Init [stmt_logs_clear] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
}



// add access log
/* F2：bMaskBody 路由（口令类接口）不记录请求体，其余 POST/PUT 原样记录。 */
void Logs_Add(XS_RequestObject objReq, xvalue* objSession, bool bMaskBody)
{
	const char* sUser = "(guest)";
	const char* sIP = xsReqRemote(objReq);
	const char* sURI = xsReqPath(objReq);
	const char* sQuery = xsReqQuery(objReq);
	const char* sMethod = xsReqMethod(objReq);
	const char* pBody = NULL;
	size_t iBodyLen = 0;
	xtime now = XAdmin_UnixNowUs();

	if ( objSession && (xrtValueType(objSession) == XVALUE_OBJECT) ) {
		sUser = ValueText(objSession, "user");
		if ( !sUser ) {
			sUser = "(unknown)";
		}
	}
	if ( sIP == NULL || sIP[0] == '\0' ) {
		sIP = "(unknown)";
	}
	if ( sURI == NULL ) {
		sURI = "";
	}
	if ( sQuery == NULL ) {
		sQuery = "";
	}
	if ( sMethod == NULL ) {
		sMethod = "";
	}
	if ( !bMaskBody && ((xsReqMethodID(objReq) == XHTTP_METHOD_POST) || (xsReqMethodID(objReq) == XHTTP_METHOD_PUT)) ) {
		pBody = (str)xsReqBody(objReq);
		iBodyLen = xsReqBodyLen(objReq);
	}
	if ( pBody == NULL ) {
		pBody = "";
		iBodyLen = 0;
	}

	sqlite3_bind_text(stmt_logs_add, 1, sUser, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt_logs_add, 2, sIP, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt_logs_add, 3, sURI, (int)strlen(sURI), SQLITE_STATIC);
	sqlite3_bind_text(stmt_logs_add, 4, sMethod, (int)strlen(sMethod), SQLITE_STATIC);
	sqlite3_bind_text(stmt_logs_add, 5, sQuery, (int)strlen(sQuery), SQLITE_STATIC);
	sqlite3_bind_text(stmt_logs_add, 6, pBody, (int)iBodyLen, SQLITE_STATIC);
	sqlite3_bind_int64(stmt_logs_add, 7, now);
	sqlite3_step(stmt_logs_add);
	sqlite3_reset(stmt_logs_add);
}



// free logs module
void Logs_Unit()
{
	xrtMutexDestroy(G_LogsConfigLock); G_LogsConfigLock = NULL;
	printf("        Logs_Unit \n");
	sqlite3_finalize(stmt_logs_all);
	sqlite3_finalize(stmt_logs_sel);
	sqlite3_finalize(stmt_logs_add);
	sqlite3_finalize(stmt_logs_clear);
}
