#ifndef XADMIN_CONTENT_DB_H
#define XADMIN_CONTENT_DB_H

bool ContentDB_Exec(const char* sSQL)
{
	char* sErr = NULL;

	if ( (G_DB == NULL) || (sSQL == NULL) ) {
		return FALSE;
	}

	if ( sqlite3_exec(G_DB, sSQL, NULL, NULL, &sErr) != SQLITE_OK ) {
		printf("[content] db exec failed: %s\n", sErr ? sErr : "(unknown)");
		if ( sErr ) {
			sqlite3_free(sErr);
		}
		return FALSE;
	}
	return TRUE;
}

bool ContentDB_BeginImmediate()
{
	return ContentDB_Exec("BEGIN IMMEDIATE");
}

bool ContentDB_Commit()
{
	return ContentDB_Exec("COMMIT");
}

bool ContentDB_Rollback()
{
	return ContentDB_Exec("ROLLBACK");
}

bool ContentDB_Init()
{
	if ( !ContentDB_Exec(
		"CREATE TABLE IF NOT EXISTS content_schema_version ("
		"module TEXT PRIMARY KEY,"
		"version INTEGER NOT NULL,"
		"description TEXT NOT NULL DEFAULT '',"
		"update_time INTEGER NOT NULL"
		");"
		"CREATE TABLE IF NOT EXISTS content_model ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"xid TEXT NOT NULL UNIQUE,"
		"name TEXT NOT NULL,"
		"namespace TEXT NOT NULL DEFAULT '',"
		"title TEXT NOT NULL,"
		"description TEXT NOT NULL DEFAULT '',"
		"icon TEXT NOT NULL DEFAULT '',"
		"table_name TEXT NOT NULL DEFAULT '',"
		"status TEXT NOT NULL DEFAULT 'active',"
		"field_count INTEGER NOT NULL DEFAULT 0,"
		"current_revision INTEGER NOT NULL DEFAULT 0,"
		"applied_revision INTEGER NOT NULL DEFAULT 0,"
		"generated_plugin_xid TEXT NOT NULL DEFAULT '',"
		"spec_json TEXT NOT NULL,"
		"spec_hash TEXT NOT NULL DEFAULT '',"
		"create_time INTEGER NOT NULL,"
		"update_time INTEGER NOT NULL"
		");"
		"CREATE INDEX IF NOT EXISTS idx_content_model_update_time ON content_model(update_time DESC);"
		"CREATE INDEX IF NOT EXISTS idx_content_model_status ON content_model(status);"
		"CREATE INDEX IF NOT EXISTS idx_content_model_generated_plugin ON content_model(generated_plugin_xid);"
		"CREATE TABLE IF NOT EXISTS content_model_revision ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"model_id INTEGER NOT NULL,"
		"revision INTEGER NOT NULL,"
		"spec_json TEXT NOT NULL,"
		"spec_hash TEXT NOT NULL DEFAULT '',"
		"note TEXT NOT NULL DEFAULT '',"
		"generator_version TEXT NOT NULL DEFAULT '',"
		"create_time INTEGER NOT NULL,"
		"UNIQUE(model_id, revision)"
		");"
		"CREATE INDEX IF NOT EXISTS idx_content_model_revision_model_rev ON content_model_revision(model_id, revision DESC);"
		"CREATE TABLE IF NOT EXISTS content_generation ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"model_id INTEGER NOT NULL,"
		"target_revision INTEGER NOT NULL,"
		"plugin_xid TEXT NOT NULL,"
		"status TEXT NOT NULL DEFAULT 'pending',"
		"output_json TEXT NOT NULL DEFAULT '{}',"
		"advisor_json TEXT NOT NULL DEFAULT '{}',"
		"error_message TEXT NOT NULL DEFAULT '',"
		"create_time INTEGER NOT NULL,"
		"finish_time INTEGER NOT NULL DEFAULT 0"
		");"
		"CREATE INDEX IF NOT EXISTS idx_content_generation_model_time ON content_generation(model_id, create_time DESC);"
		"CREATE INDEX IF NOT EXISTS idx_content_generation_status ON content_generation(status);"
		"CREATE TABLE IF NOT EXISTS content_pack ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"pack_id TEXT NOT NULL UNIQUE,"
		"name TEXT NOT NULL DEFAULT '',"
		"title TEXT NOT NULL DEFAULT '',"
		"description TEXT NOT NULL DEFAULT '',"
		"version TEXT NOT NULL DEFAULT '',"
		"author TEXT NOT NULL DEFAULT '',"
		"source TEXT NOT NULL DEFAULT '',"
		"install_type TEXT NOT NULL DEFAULT '',"
		"status TEXT NOT NULL DEFAULT 'active',"
		"path TEXT NOT NULL DEFAULT '',"
		"manifest_json TEXT NOT NULL DEFAULT '{}',"
		"global_form_json TEXT NOT NULL DEFAULT '{}',"
		"instance_form_json TEXT NOT NULL DEFAULT '{}',"
		"effects_json TEXT NOT NULL DEFAULT '{}',"
		"hooks_json TEXT NOT NULL DEFAULT '{}',"
		"symbols_json TEXT NOT NULL DEFAULT '{}',"
		"patches_json TEXT NOT NULL DEFAULT '{}',"
		"contracts_json TEXT NOT NULL DEFAULT '{}',"
		"update_channel TEXT NOT NULL DEFAULT '',"
		"update_package_id TEXT NOT NULL DEFAULT '',"
		"can_update INTEGER NOT NULL DEFAULT 0,"
		"can_uninstall INTEGER NOT NULL DEFAULT 0,"
		"readonly INTEGER NOT NULL DEFAULT 0,"
		"system INTEGER NOT NULL DEFAULT 0,"
		"sort INTEGER NOT NULL DEFAULT 0,"
		"create_time INTEGER NOT NULL,"
		"update_time INTEGER NOT NULL"
		");"
		"CREATE INDEX IF NOT EXISTS idx_content_pack_status_sort ON content_pack(status, sort);"
		"CREATE TABLE IF NOT EXISTS content_pack_option ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"pack_id TEXT NOT NULL UNIQUE,"
		"options_json TEXT NOT NULL DEFAULT '{}',"
		"create_time INTEGER NOT NULL,"
		"update_time INTEGER NOT NULL"
		");"
		"CREATE TABLE IF NOT EXISTS content_model_pack ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"model_id INTEGER NOT NULL,"
		"pack_id TEXT NOT NULL,"
		"enabled INTEGER NOT NULL DEFAULT 1,"
		"instance_options_json TEXT NOT NULL DEFAULT '{}',"
		"mount_json TEXT NOT NULL DEFAULT '{}',"
		"sort INTEGER NOT NULL DEFAULT 0,"
		"create_time INTEGER NOT NULL,"
		"update_time INTEGER NOT NULL,"
		"UNIQUE(model_id, pack_id)"
		");"
		"CREATE INDEX IF NOT EXISTS idx_content_model_pack_model ON content_model_pack(model_id);"
		"CREATE INDEX IF NOT EXISTS idx_content_model_pack_pack ON content_model_pack(pack_id);"
	) ) {
		return FALSE;
	}

	sqlite3_stmt* stmt = NULL;
	int iNow = (int)time(NULL);
	if ( sqlite3_prepare_v3(G_DB, "INSERT INTO content_schema_version (module,version,description,update_time) VALUES ('content',1,'initial builtin content schema',?) ON CONFLICT(module) DO UPDATE SET version=excluded.version,description=excluded.description,update_time=excluded.update_time", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}
	sqlite3_bind_int(stmt, 1, iNow);
	bool bOK = (sqlite3_step(stmt) == SQLITE_DONE);
	sqlite3_finalize(stmt);
	return bOK;
}

#endif
