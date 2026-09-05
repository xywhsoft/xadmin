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
