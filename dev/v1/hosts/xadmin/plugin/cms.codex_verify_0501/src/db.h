#ifndef CONTENT_PLUGIN_DB_H
#define CONTENT_PLUGIN_DB_H

static const char* CONTENT_PLUGIN_SCHEMA_SQL =
	"CREATE TABLE IF NOT EXISTS cms_codex_verify_0501 ("
	"id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"status INTEGER NOT NULL DEFAULT 0,"
	"payload_json TEXT NOT NULL DEFAULT '{}',"
	"create_time INTEGER NOT NULL,"
	"update_time INTEGER NOT NULL DEFAULT 0,"
	"delete_time INTEGER NOT NULL DEFAULT 0"
	");";

static bool ContentPlugin_EnsureSchema(const char* sPrivateDbPath)
{
	sqlite3* pDb = NULL;
	char* sError = NULL;
	bool bOK = FALSE;
	if ( (sPrivateDbPath == NULL) || (sPrivateDbPath[0] == '\0') ) return FALSE;
	if ( sqlite3_open_v2(sPrivateDbPath, &pDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL) != SQLITE_OK ) {
		if ( pDb ) sqlite3_close(pDb);
		return FALSE;
	}
	sqlite3_busy_timeout(pDb, 3000);
	bOK = sqlite3_exec(pDb, CONTENT_PLUGIN_SCHEMA_SQL, NULL, NULL, &sError) == SQLITE_OK;
	if ( sError ) sqlite3_free(sError);
	sqlite3_close(pDb);
	return bOK;
}

#endif
