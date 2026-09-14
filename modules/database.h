static bool DB_Init(void)
{
	char* file = xrtPathJoin(DBPath, "main.db");
	int result;
	/* 禁止路径错误时静默创建空库；初始迁移由工具显式执行。 */
	if (!file || !xrtFileExists(file)) { xrtFree(file); return false; }
	result = sqlite3_open_v2(file, &G_DB, SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, NULL);
	xrtFree(file);
	if (result != SQLITE_OK) return false;
	sqlite3_busy_timeout(G_DB, 5000);
	return true;
}
static void DB_Unit(void)
{
	if (G_DB) sqlite3_close_v2(G_DB);
	G_DB = NULL;
}

/* 攻击评审 F2：v1 库的 uris 表没有 maskBody 列，首次启动幂等补列。
 * 列仅作记录（新路由 INSERT 写入初始值），运行时以注册参数为权威。 */

/* 时间单位迁移：v1 库以"公元零年起算的本地日历秒"记录时间；原生应用统一
 * 使用 xtime（Unix 微秒）。首次启动一次性换算全部时间列，以 PRAGMA
 * user_version=2 为记号保证只执行一次；v1 值域约 6.2e10，换算后约 1.7e15，
 * WHERE 区间同时防止对已换算行重复执行。历史行的本地时差按当前时区近似。 */
static bool DB_MigrateTimeUnits(void)
{
	static const struct { const char* table; const char* columns[8]; } tables[] = {
		{"attachment", {"createTime", NULL}},
		{"attachmentOrder", {"createTime", NULL}},
		{"auth", {"createTime", "updateTime", NULL}},
		{"authGroup", {"createTime", "updateTime", NULL}},
		{"logs", {"createTime", NULL}},
		{"member", {"createTime", "updateTime", NULL}},
		{"memberAuth", {"createTime", "updateTime", NULL}},
		{"memberAuthGroup", {"createTime", "updateTime", NULL}},
		{"memberBalanceLog", {"createTime", NULL}},
		{"memberGroup", {"createTime", "updateTime", NULL}},
		{"menu", {"createTime", "updateTime", NULL}},
		{"plugin_generation", {"load_time", "start_time", "stop_time", NULL}},
		{"plugin_instance", {"create_time", "update_time", NULL}},
		{"plugin_migration_log", {"exec_time", NULL}},
		{"plugin_package", {"install_time", NULL}},
		{"plugin_resource", {"create_time", NULL}},
		{"plugin_runtime", {"create_time", "update_time", NULL}},
		{"role", {"createTime", "updateTime", NULL}},
		{"uris", {"createTime", "updateTime", NULL}},
		{"user", {"createTime", "updateTime", NULL}},
		{"sched_task", {"onceAt", "startAt", "nextRunAt", "lastRunAt", "lastFinishAt", "createTime", "updateTime", NULL}},
		{"sched_run_log", {"startTime", "finishTime", NULL}},
		{"notify_message", {"createTime", NULL}},
		{"notify_recipient", {"readTime", "deleteTime", NULL}},
		{"member_notify_setting", {"updateTime", NULL}},
		{"mail_task", {"nextRetryAt", "createTime", NULL}},
		{"mail_log", {"createTime", NULL}},
	};
	xdatetime local;
	int64 offset = 0;
	char sql[512];
	size_t i, c;
	int version = 0;
	sqlite3_stmt* stmt = NULL;

	if (sqlite3_prepare_v2(G_DB, "PRAGMA user_version;", -1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW) version = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}
	if (version >= 2) return true;
	if (xrtTimeLocal(xrtNow(), &local)) offset = (int64)local.Offset;
	if (sqlite3_exec(G_DB, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) return false;
	for (i = 0; i < sizeof(tables) / sizeof(tables[0]); i++) {
		for (c = 0; c < 8 && tables[i].columns[c]; c++) {
			snprintf(sql, sizeof(sql),
				"UPDATE %s SET %s = (%s - 62167219200 - %lld) * 1000000 "
				"WHERE %s > 62167219200 AND %s < 64000000000;",
				tables[i].table, tables[i].columns[c], tables[i].columns[c],
				(long long)offset, tables[i].columns[c], tables[i].columns[c]);
			if (sqlite3_exec(G_DB, sql, NULL, NULL, NULL) != SQLITE_OK) {
				sqlite3_exec(G_DB, "ROLLBACK", NULL, NULL, NULL);
				return false;
			}
		}
	}
	snprintf(sql, sizeof(sql), "PRAGMA user_version = 2;");
	if (sqlite3_exec(G_DB, sql, NULL, NULL, NULL) != SQLITE_OK) {
		sqlite3_exec(G_DB, "ROLLBACK", NULL, NULL, NULL);
		return false;
	}
	return sqlite3_exec(G_DB, "COMMIT", NULL, NULL, NULL) == SQLITE_OK;
}


/* 浸泡实测：plugin_resource 无索引时台账清理全表扫描，20 万行后 p95 达 5.2s。
 * (xid, generation, status) 精确覆盖 CleanupResources 的 WHERE 子句。 */
static bool DB_EnsurePluginResourceIndex(void)
{
	sqlite3_stmt* stmt; bool bHas = false;
	if (sqlite3_prepare_v2(G_DB, "SELECT name FROM sqlite_master WHERE type='index' AND tbl_name='plugin_resource' AND name='idx_plugin_resource_lookup';", -1, &stmt, NULL) != SQLITE_OK) return false;
	if (sqlite3_step(stmt) == SQLITE_ROW) bHas = true;
	sqlite3_finalize(stmt);
	if (bHas) return true;
	return sqlite3_exec(G_DB, "CREATE INDEX IF NOT EXISTS idx_plugin_resource_lookup ON plugin_resource(xid, generation, status);", NULL, NULL, NULL) == SQLITE_OK;
}

static bool DB_EnsureUrisMaskColumn(void)
{
	sqlite3_stmt* stmt; bool bHas = false;
	if (sqlite3_prepare_v2(G_DB, "PRAGMA table_info(uris);", -1, &stmt, NULL) != SQLITE_OK) return false;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char* sName = (const char*)sqlite3_column_text(stmt, 1);
		if (sName && strcmp(sName, "maskBody") == 0) { bHas = true; break; }
	}
	sqlite3_finalize(stmt);
	if (bHas) return true;
	return sqlite3_exec(G_DB, "ALTER TABLE uris ADD COLUMN maskBody INTEGER NOT NULL DEFAULT 0;",
		NULL, NULL, NULL) == SQLITE_OK;
}

/* 调用方持有请求锁；在下一条 SQL 前读取影响行数，并释放借用的绑定值。
 * 按 ID 修改/删除及新增必须实际写入；批量清理和关联迁移允许零行。 */
static void DB_ResetWrite(sqlite3_stmt* stmt)
{
	if (!stmt) return;
	sqlite3_reset(stmt);
	sqlite3_clear_bindings(stmt);
}
static bool DB_Write(sqlite3_stmt* stmt, bool require_row)
{
	int result = stmt ? sqlite3_step(stmt) : SQLITE_MISUSE;
	bool written = result == SQLITE_DONE && (!require_row || sqlite3_changes(G_DB) > 0);
	DB_ResetWrite(stmt);
	return written;
}
static bool DB_BeginWrite(void)
{
	return sqlite3_exec(G_DB, "SAVEPOINT xadmin_write", NULL, NULL, NULL) == SQLITE_OK;
}
static bool DB_EndWrite(bool written)
{
	if (written && sqlite3_exec(G_DB, "RELEASE xadmin_write", NULL, NULL, NULL) == SQLITE_OK) return true;
	sqlite3_exec(G_DB, "ROLLBACK TO xadmin_write", NULL, NULL, NULL);
	sqlite3_exec(G_DB, "RELEASE xadmin_write", NULL, NULL, NULL);
	return false;
}
/* 两个语句在调用前完成绑定；迁移关联和删除主记录必须一起提交。 */
static bool DB_MoveAndDelete(sqlite3_stmt* move, sqlite3_stmt* remove)
{
	bool written;
	if (!DB_BeginWrite()) { DB_ResetWrite(move); DB_ResetWrite(remove); return false; }
	written = DB_Write(move, false);
	if (written) written = DB_Write(remove, true);
	else DB_ResetWrite(remove);
	return DB_EndWrite(written);
}
