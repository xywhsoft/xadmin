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
