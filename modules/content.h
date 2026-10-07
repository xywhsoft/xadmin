/* 内容模型系统（v1 script/content 对齐，方案 A 原生方言）。
 * 阶段 1：DB 7 表 + spec 校验/指纹 + 模型 CRUD/修订 + advisor 体检 +
 * 能力包装载器 + 菜单装配。生成器与模板在 content_generator/generation
 * （阶段 2，generate 路由暂以明确错误占位）。
 * 时间列统一 Unix 微秒（xrtNow），与 v1 的秒不互换——内容表为 v3 新表。 */

/* ==================== DB 基础 ==================== */

static bool ContentDB_Exec(const char* sql)
{
	char* err = NULL;
	if (!G_DB || !sql) return false;
	if (sqlite3_exec(G_DB, sql, NULL, NULL, &err) != SQLITE_OK) {
		printf("[content] db exec failed: %s\n", err ? err : "(unknown)");
		if (err) sqlite3_free(err);
		return false;
	}
	return true;
}

static bool ContentDB_BeginImmediate(void) { return ContentDB_Exec("BEGIN IMMEDIATE"); }
static bool ContentDB_Commit(void) { return ContentDB_Exec("COMMIT"); }
static bool ContentDB_Rollback(void) { return ContentDB_Exec("ROLLBACK"); }

static bool ContentDB_Init(void)
{
	if (!ContentDB_Exec(
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
	)) return false;
	{
		sqlite3_stmt* stmt = NULL;
		bool ok = false;
		if (sqlite3_prepare_v2(G_DB,
			"INSERT INTO content_schema_version (module,version,description,update_time) VALUES ('content',1,'initial builtin content schema',?) "
			"ON CONFLICT(module) DO UPDATE SET version=excluded.version,description=excluded.description,update_time=excluded.update_time",
			-1, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_int64(stmt, 1, XAdmin_UnixNowUs());
			ok = sqlite3_step(stmt) == SQLITE_DONE;
			sqlite3_finalize(stmt);
		}
		return ok;
	}
}

/* ==================== 能力包装载器（content_pack） ==================== */

static const char* ContentPack_TextOr(const char* text, const char* fallback)
{
	return (text && text[0]) ? text : fallback;
}

static char* ContentPack_ReadTextOrDefault(const char* dir, const char* name, const char* fallbackJson)
{
	char* path = xrtPathJoin((char*)dir, (char*)name);
	char* text = NULL;
	size_t size = 0;
	if (path) {
		if (xrtPathExists(path)) text = (char*)xrtFileReadAll(path, &size);
		xrtFree(path);
	}
	return text ? text : xrtStrDup(ContentPack_TextOr(fallbackJson, "{}"));
}

static char* ContentPack_ReadManifestFile(xvalue* pack, const char* dir, const char* key, const char* defaultName, const char* fallbackJson)
{
	str name = ValueText(pack, key);
	return ContentPack_ReadTextOrDefault(dir, ContentPack_TextOr(name, defaultName), fallbackJson);
}

static bool ContentPack_UpsertManifest(xvalue* pack, const char* dir)
{
	sqlite3_stmt* stmt = NULL;
	str packId, name, title, description, version, author, source, installType, status;
	char* manifestJson = NULL; char* globalFormJson = NULL; char* instanceFormJson = NULL;
	char* effectsJson = NULL; char* hooksJson = NULL; char* symbolsJson = NULL;
	char* patchesJson = NULL; char* contractsJson = NULL;
	xvalue* update; xvalue* protection; xvalue* config;
	str updateChannel = NULL, updatePackageId = NULL;
	int canUpdate = 0, canUninstall = 0, readonly = 0, system = 0, sort;
	int64 now = XAdmin_UnixNowUs();
	bool ok = false;

	if (!G_DB || !pack || xrtValueType(pack) != XVALUE_OBJECT) return false;
	packId = ValueText(pack, "packId");
	name = ValueText(pack, "name");
	title = ValueText(pack, "title");
	description = ValueText(pack, "description");
	version = ValueText(pack, "version");
	author = ValueText(pack, "author");
	source = ValueText(pack, "source");
	installType = ValueText(pack, "installType");
	status = ValueText(pack, "status");
	sort = (int)ValueInt(pack, "sort");
	update = ValueGet(pack, "update");
	protection = ValueGet(pack, "protection");
	config = ValueGet(pack, "config");
	if (!packId || !packId[0] || !title || !title[0]) return false;
	if (sort <= 0) sort = 1000;
	if (update && xrtValueType(update) == XVALUE_OBJECT) {
		updateChannel = ValueText(update, "channel");
		updatePackageId = ValueText(update, "packageId");
		canUpdate = ValueBool(update, "canUpdate") ? 1 : 0;
		canUninstall = ValueBool(update, "canUninstall") ? 1 : 0;
	}
	if (protection && xrtValueType(protection) == XVALUE_OBJECT) {
		readonly = ValueBool(protection, "readonly") ? 1 : 0;
		system = ValueBool(protection, "system") ? 1 : 0;
	}
	{
		size_t size = 0;
		manifestJson = xrtJsonStringify(pack, false, &size);
	}
	if (config && xrtValueType(config) == XVALUE_OBJECT) {
		str globalForm = ValueText(config, "globalForm");
		str instanceForm = ValueText(config, "instanceForm");
		globalFormJson = ContentPack_ReadTextOrDefault(dir, ContentPack_TextOr(globalForm, "global.xform.json"), "{}");
		instanceFormJson = ContentPack_ReadTextOrDefault(dir, ContentPack_TextOr(instanceForm, "instance.xform.json"), "{}");
	} else {
		globalFormJson = ContentPack_ReadTextOrDefault(dir, "global.xform.json", "{}");
		instanceFormJson = ContentPack_ReadTextOrDefault(dir, "instance.xform.json", "{}");
	}
	effectsJson = ContentPack_ReadManifestFile(pack, dir, "effects", "effects.json", "{}");
	hooksJson = ContentPack_ReadManifestFile(pack, dir, "hooks", "hooks.json", "{}");
	symbolsJson = ContentPack_ReadManifestFile(pack, dir, "symbols", "symbols.json", "{}");
	patchesJson = ContentPack_ReadManifestFile(pack, dir, "patches", "patches.json", "{}");
	contractsJson = ContentPack_ReadManifestFile(pack, dir, "contracts", "contracts.json", "{}");

	if (sqlite3_prepare_v2(G_DB,
		"INSERT INTO content_pack (pack_id,name,title,description,version,author,source,install_type,status,path,manifest_json,global_form_json,instance_form_json,effects_json,hooks_json,symbols_json,patches_json,contracts_json,update_channel,update_package_id,can_update,can_uninstall,readonly,system,sort,create_time,update_time) "
		"VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
		"ON CONFLICT(pack_id) DO UPDATE SET name=excluded.name,title=excluded.title,description=excluded.description,version=excluded.version,author=excluded.author,source=excluded.source,install_type=excluded.install_type,status=excluded.status,path=excluded.path,manifest_json=excluded.manifest_json,global_form_json=excluded.global_form_json,instance_form_json=excluded.instance_form_json,effects_json=excluded.effects_json,hooks_json=excluded.hooks_json,symbols_json=excluded.symbols_json,patches_json=excluded.patches_json,contracts_json=excluded.contracts_json,update_channel=excluded.update_channel,update_package_id=excluded.update_package_id,can_update=excluded.can_update,can_uninstall=excluded.can_uninstall,readonly=excluded.readonly,system=excluded.system,sort=excluded.sort,update_time=excluded.update_time",
		-1, &stmt, NULL) == SQLITE_OK) {
		Plugin_BindText(stmt, 1, packId);
		Plugin_BindText(stmt, 2, ContentPack_TextOr(name, packId));
		Plugin_BindText(stmt, 3, title);
		Plugin_BindText(stmt, 4, ContentPack_TextOr(description, ""));
		Plugin_BindText(stmt, 5, ContentPack_TextOr(version, ""));
		Plugin_BindText(stmt, 6, ContentPack_TextOr(author, ""));
		Plugin_BindText(stmt, 7, ContentPack_TextOr(source, "local"));
		Plugin_BindText(stmt, 8, ContentPack_TextOr(installType, "local"));
		Plugin_BindText(stmt, 9, ContentPack_TextOr(status, "active"));
		Plugin_BindText(stmt, 10, ContentPack_TextOr(dir, ""));
		Plugin_BindText(stmt, 11, ContentPack_TextOr(manifestJson, "{}"));
		Plugin_BindText(stmt, 12, ContentPack_TextOr(globalFormJson, "{}"));
		Plugin_BindText(stmt, 13, ContentPack_TextOr(instanceFormJson, "{}"));
		Plugin_BindText(stmt, 14, ContentPack_TextOr(effectsJson, "{}"));
		Plugin_BindText(stmt, 15, ContentPack_TextOr(hooksJson, "{}"));
		Plugin_BindText(stmt, 16, ContentPack_TextOr(symbolsJson, "{}"));
		Plugin_BindText(stmt, 17, ContentPack_TextOr(patchesJson, "{}"));
		Plugin_BindText(stmt, 18, ContentPack_TextOr(contractsJson, "{}"));
		Plugin_BindText(stmt, 19, ContentPack_TextOr(updateChannel, ""));
		Plugin_BindText(stmt, 20, ContentPack_TextOr(updatePackageId, ""));
		sqlite3_bind_int(stmt, 21, canUpdate);
		sqlite3_bind_int(stmt, 22, canUninstall);
		sqlite3_bind_int(stmt, 23, readonly);
		sqlite3_bind_int(stmt, 24, system);
		sqlite3_bind_int(stmt, 25, sort);
		sqlite3_bind_int64(stmt, 26, now);
		sqlite3_bind_int64(stmt, 27, now);
		ok = sqlite3_step(stmt) == SQLITE_DONE;
		sqlite3_finalize(stmt);
	}
	xrtFree(manifestJson); xrtFree(globalFormJson); xrtFree(instanceFormJson);
	xrtFree(effectsJson); xrtFree(hooksJson); xrtFree(symbolsJson);
	xrtFree(patchesJson); xrtFree(contractsJson);
	return ok;
}

static int ContentPack_LoadDirProc(const char* path, size_t size, bool dir, void* param)
{
	char* manifestPath; char* json; size_t jsonSize = 0;
	xvalue* pack;
	bool* pOK = (bool*)param;
	(void)size;
	if (!path || !dir) return 0;
	manifestPath = xrtPathJoin((char*)path, "pack.json");
	if (!manifestPath || !xrtPathExists(manifestPath)) { xrtFree(manifestPath); return 0; }
	json = (char*)xrtFileReadAll(manifestPath, &jsonSize);
	xrtFree(manifestPath);
	if (!json) { if (pOK) *pOK = false; return 1; }
	/* 跳过 UTF-8 BOM */
	{
		char* p = json;
		size_t n = jsonSize;
		if (n >= 3 && (unsigned char)p[0] == 0xEF && (unsigned char)p[1] == 0xBB && (unsigned char)p[2] == 0xBF) { p += 3; n -= 3; }
		pack = n ? xrtJsonParse(xrtStrViewN(p, n)) : NULL;
	}
	xrtFree(json);
	if (!pack || xrtValueType(pack) != XVALUE_OBJECT) {
		xrtValueRelease(pack);
		if (pOK) *pOK = false;
		return 1;
	}
	if (!ContentPack_UpsertManifest(pack, path) && pOK) *pOK = false;
	xrtValueRelease(pack);
	return 0;
}

static bool ContentPack_Init(void)
{
	bool ok = true;
	char* packDir = xrtPathJoin(AppPath, "capability-pack");
	if (packDir) {
		xrtDirCreateAll(packDir);
		if (xrtDirExists(packDir)) DirScan(packDir, false, ContentPack_LoadDirProc, &ok);
		xrtFree(packDir);
	}
	return ok;
}

static xvalue* ContentPack_ListAll(void)
{
	sqlite3_stmt* stmt = NULL;
	xvalue* rows = ValueArray();
	if (!rows) return NULL;
	if (sqlite3_prepare_v2(G_DB, "SELECT pack_id,name,title,description,version,author,source,install_type,status,path,update_channel,update_package_id,can_update,can_uninstall,readonly,system,sort,effects_json,instance_form_json FROM content_pack ORDER BY sort ASC,id ASC", -1, &stmt, NULL) == SQLITE_OK) {
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			xvalue* row = ValueObject();
			ValueSetText(row, "packId", (const char*)sqlite3_column_text(stmt, 0));
			ValueSetText(row, "name", (const char*)sqlite3_column_text(stmt, 1));
			ValueSetText(row, "title", (const char*)sqlite3_column_text(stmt, 2));
			ValueSetText(row, "description", (const char*)sqlite3_column_text(stmt, 3));
			ValueSetText(row, "version", (const char*)sqlite3_column_text(stmt, 4));
			ValueSetText(row, "author", (const char*)sqlite3_column_text(stmt, 5));
			ValueSetText(row, "source", (const char*)sqlite3_column_text(stmt, 6));
			ValueSetText(row, "installType", (const char*)sqlite3_column_text(stmt, 7));
			ValueSetText(row, "status", (const char*)sqlite3_column_text(stmt, 8));
			ValueSetText(row, "path", (const char*)sqlite3_column_text(stmt, 9));
			ValueSetText(row, "updateChannel", (const char*)sqlite3_column_text(stmt, 10));
			ValueSetText(row, "updatePackageId", (const char*)sqlite3_column_text(stmt, 11));
			ValueSetBool(row, "canUpdate", sqlite3_column_int(stmt, 12) != 0);
			ValueSetBool(row, "canUninstall", sqlite3_column_int(stmt, 13) != 0);
			ValueSetBool(row, "readonly", sqlite3_column_int(stmt, 14) != 0);
			ValueSetBool(row, "system", sqlite3_column_int(stmt, 15) != 0);
			ValueSetInt(row, "sort", sqlite3_column_int(stmt, 16));
			ValueSetText(row, "effectsJson", (const char*)sqlite3_column_text(stmt, 17));
			ValueSetText(row, "instanceFormJson", (const char*)sqlite3_column_text(stmt, 18));
			ValueArrayOwn(rows, row);
		}
		sqlite3_finalize(stmt);
	}
	return rows;
}

static xvalue* ContentPack_GetDetail(const char* packId)
{
	sqlite3_stmt* stmt = NULL;
	xvalue* row = NULL;
	if (!packId || !packId[0]) return NULL;
	if (sqlite3_prepare_v2(G_DB, "SELECT pack_id,name,title,description,version,author,source,install_type,status,path,manifest_json,global_form_json,instance_form_json,effects_json,hooks_json,symbols_json,patches_json,contracts_json,update_channel,update_package_id,can_update,can_uninstall,readonly,system,sort FROM content_pack WHERE pack_id=? LIMIT 1", -1, &stmt, NULL) != SQLITE_OK)
		return NULL;
	Plugin_BindText(stmt, 1, packId);
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		row = ValueObject();
		ValueSetText(row, "packId", (const char*)sqlite3_column_text(stmt, 0));
		ValueSetText(row, "name", (const char*)sqlite3_column_text(stmt, 1));
		ValueSetText(row, "title", (const char*)sqlite3_column_text(stmt, 2));
		ValueSetText(row, "description", (const char*)sqlite3_column_text(stmt, 3));
		ValueSetText(row, "version", (const char*)sqlite3_column_text(stmt, 4));
		ValueSetText(row, "author", (const char*)sqlite3_column_text(stmt, 5));
		ValueSetText(row, "source", (const char*)sqlite3_column_text(stmt, 6));
		ValueSetText(row, "installType", (const char*)sqlite3_column_text(stmt, 7));
		ValueSetText(row, "status", (const char*)sqlite3_column_text(stmt, 8));
		ValueSetText(row, "path", (const char*)sqlite3_column_text(stmt, 9));
		ValueSetText(row, "manifestJson", (const char*)sqlite3_column_text(stmt, 10));
		ValueSetText(row, "globalFormJson", (const char*)sqlite3_column_text(stmt, 11));
		ValueSetText(row, "instanceFormJson", (const char*)sqlite3_column_text(stmt, 12));
		ValueSetText(row, "effectsJson", (const char*)sqlite3_column_text(stmt, 13));
		ValueSetText(row, "hooksJson", (const char*)sqlite3_column_text(stmt, 14));
		ValueSetText(row, "symbolsJson", (const char*)sqlite3_column_text(stmt, 15));
		ValueSetText(row, "patchesJson", (const char*)sqlite3_column_text(stmt, 16));
		ValueSetText(row, "contractsJson", (const char*)sqlite3_column_text(stmt, 17));
		ValueSetText(row, "updateChannel", (const char*)sqlite3_column_text(stmt, 18));
		ValueSetText(row, "updatePackageId", (const char*)sqlite3_column_text(stmt, 19));
		ValueSetBool(row, "canUpdate", sqlite3_column_int(stmt, 20) != 0);
		ValueSetBool(row, "canUninstall", sqlite3_column_int(stmt, 21) != 0);
		ValueSetBool(row, "readonly", sqlite3_column_int(stmt, 22) != 0);
		ValueSetBool(row, "system", sqlite3_column_int(stmt, 23) != 0);
		ValueSetInt(row, "sort", sqlite3_column_int(stmt, 24));
	}
	sqlite3_finalize(stmt);
	return row;
}

/* 收集单个表单字段的默认值（defaultValue/value/default 三键兼容） */
static void ContentPack_FieldDefault(const xvalue* field, xvalue* defaults)
{
	str name;
	const xvalue* dv;
	if (!field || xrtValueType(field) != XVALUE_OBJECT) return;
	name = ValueText(field, "name");
	if (!name || !name[0]) return;
	dv = ValueGet(field, "defaultValue");
	if (!dv) dv = ValueGet(field, "value");
	if (!dv) dv = ValueGet(field, "default");
	if (dv) ValueSetRef(defaults, name, dv);
}

/* 遍历表单 schema（groups[]/fields[] 两种形态）收集字段默认值 */
static xvalue* ContentPack_FormDefaults(const xvalue* schema)
{
	xvalue* defaults = ValueObject();
	const xvalue* groups = ValueGet(schema, "groups");
	const xvalue* fields;
	uint32 i, j;
	if (groups && xrtValueType(groups) == XVALUE_ARRAY) {
		for (i = 0; i < ValueCount(groups); i++) {
			const xvalue* group = xrtValueArrayGet(groups, i);
			fields = group ? ValueGet(group, "fields") : NULL;
			if (!fields || xrtValueType(fields) != XVALUE_ARRAY) continue;
			for (j = 0; j < ValueCount(fields); j++) ContentPack_FieldDefault(xrtValueArrayGet(fields, j), defaults);
		}
	}
	fields = ValueGet(schema, "fields");
	if (fields && xrtValueType(fields) == XVALUE_ARRAY) {
		for (j = 0; j < ValueCount(fields); j++) ContentPack_FieldDefault(xrtValueArrayGet(fields, j), defaults);
	}
	return defaults;
}

static bool ContentPack_SaveOptions(const char* packId, const char* optionsJson)
{
	sqlite3_stmt* stmt = NULL;
	int64 now = XAdmin_UnixNowUs();
	bool ok = false;
	if (!G_DB || !packId || !packId[0]) return false;
	if (sqlite3_prepare_v2(G_DB, "INSERT INTO content_pack_option (pack_id,options_json,create_time,update_time) VALUES (?,?,?,?) ON CONFLICT(pack_id) DO UPDATE SET options_json=excluded.options_json,update_time=excluded.update_time", -1, &stmt, NULL) == SQLITE_OK) {
		Plugin_BindText(stmt, 1, packId);
		Plugin_BindText(stmt, 2, optionsJson ? optionsJson : "{}");
		sqlite3_bind_int64(stmt, 3, now);
		sqlite3_bind_int64(stmt, 4, now);
		ok = sqlite3_step(stmt) == SQLITE_DONE;
		sqlite3_finalize(stmt);
	}
	return ok;
}

static const char* ContentPack_NormalizeId(const char* packId)
{
	if (!packId) return NULL;
	if (!strcmp(packId, "comment")) return "content.comment";
	if (!strcmp(packId, "tag")) return "content.tag";
	if (!strcmp(packId, "topic")) return "content.topic";
	if (!strcmp(packId, "sensitive")) return "content.sensitive";
	if (!strcmp(packId, "static")) return "content.static";
	if (!strcmp(packId, "like")) return "content.like";
	if (!strcmp(packId, "view") || !strcmp(packId, "view-stat")) return "content.view-stat";
	return packId;
}

static bool ContentPack_Exists(const char* packId)
{
	sqlite3_stmt* stmt = NULL;
	bool exists = false;
	const char* normalized;
	if (!G_DB || !packId || !packId[0]) return false;
	normalized = ContentPack_NormalizeId(packId);
	if (sqlite3_prepare_v2(G_DB, "SELECT 1 FROM content_pack WHERE pack_id=? AND status='active' LIMIT 1", -1, &stmt, NULL) != SQLITE_OK)
		return false;
	Plugin_BindText(stmt, 1, normalized);
	exists = sqlite3_step(stmt) == SQLITE_ROW;
	sqlite3_finalize(stmt);
	return exists;
}

/* ==================== spec 校验与指纹（content_spec） ==================== */

static bool Content_SpecIsValidIdent(const char* text, bool allowDot)
{
	size_t i, len;
	if (!text || !text[0]) return false;
	len = strlen(text);
	if (len > 96) return false;
	for (i = 0; i < len; i++) {
		char ch = text[i];
		if (((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) || ((ch >= '0') && (ch <= '9')) || ch == '_' || ch == '-' || (allowDot && ch == '.'))
			continue;
		return false;
	}
	return true;
}

static bool Content_IsValidXid(const char* xid)
{
	return Content_SpecIsValidIdent(xid, true);
}

static bool Content_SpecValidate(xvalue* spec, str* error)
{
	xvalue* fields; xvalue* capabilities; xvalue* pages; xvalue* policies;
	str xid, generatedPluginXid;
	if (error) *error = NULL;
	if (!spec || xrtValueType(spec) != XVALUE_OBJECT) {
		if (error) *error = xrtStrDup("spec: invalid object");
		return false;
	}
	xid = ValueText(spec, "xid");
	if (!Content_SpecIsValidIdent(xid, true)) {
		if (error) *error = xrtStrDup("xid: invalid content model xid");
		return false;
	}
	generatedPluginXid = ValueText(spec, "generatedPluginXid");
	if (generatedPluginXid && generatedPluginXid[0] && !Content_SpecIsValidIdent(generatedPluginXid, true)) {
		if (error) *error = xrtStrDup("generatedPluginXid: invalid plugin xid");
		return false;
	}
	fields = ValueGet(spec, "fields");
	if (!fields || xrtValueType(fields) != XVALUE_ARRAY || ValueCount(fields) == 0) {
		if (error) *error = xrtStrDup("fields: at least one field is required");
		return false;
	}
	{
		uint32 i, j;
		for (i = 0; i < ValueCount(fields); i++) {
			xvalue* field = xrtValueArrayGet(fields, i);
			str name;
			if (!field || xrtValueType(field) != XVALUE_OBJECT) {
				if (error) *error = xrtFormat("fields[%u]: invalid field object", i);
				return false;
			}
			name = ValueText(field, "name");
			if (!Content_SpecIsValidIdent(name, false)) {
				if (error) *error = xrtFormat("fields[%u].name: invalid field name", i);
				return false;
			}
			for (j = i + 1; j < ValueCount(fields); j++) {
				xvalue* other = xrtValueArrayGet(fields, j);
				str otherName = (other && xrtValueType(other) == XVALUE_OBJECT) ? ValueText(other, "name") : NULL;
				if (otherName && !strcmp(name, otherName)) {
					if (error) *error = xrtFormat("fields[%u].name: duplicate field name '%s'", j, name);
					return false;
				}
			}
		}
	}
	capabilities = ValueGet(spec, "capabilities");
	if (capabilities) {
		uint32 i, j;
		if (xrtValueType(capabilities) != XVALUE_ARRAY) {
			if (error) *error = xrtStrDup("capabilities: invalid capability list");
			return false;
		}
		for (i = 0; i < ValueCount(capabilities); i++) {
			xvalue* cap = xrtValueArrayGet(capabilities, i);
			xvalue* config;
			str key, rawKey;
			if (!cap || xrtValueType(cap) != XVALUE_OBJECT) {
				if (error) *error = xrtFormat("capabilities[%u]: invalid capability object", i);
				return false;
			}
			rawKey = ValueText(cap, "key");
			key = (str)ContentPack_NormalizeId(rawKey);
			if (key && rawKey && strcmp(key, rawKey) != 0)
				ValueSetText(cap, "key", key);
			if (!Content_SpecIsValidIdent(key, true)) {
				if (error) *error = xrtFormat("capabilities[%u].key: invalid capability key", i);
				return false;
			}
			if (!ContentPack_Exists(key)) {
				if (error) *error = xrtFormat("capabilities[%u].key: unknown capability '%s'", i, key);
				return false;
			}
			for (j = i + 1; j < ValueCount(capabilities); j++) {
				xvalue* other = xrtValueArrayGet(capabilities, j);
				str otherKey = (other && xrtValueType(other) == XVALUE_OBJECT) ? ValueText(other, "key") : NULL;
				otherKey = (str)ContentPack_NormalizeId(otherKey);
				if (otherKey && !strcmp(key, otherKey)) {
					if (error) *error = xrtFormat("capabilities[%u].key: duplicate capability '%s'", j, key);
					return false;
				}
			}
			config = ValueGet(cap, "config");
			if (config && xrtValueType(config) != XVALUE_OBJECT) {
				if (error) *error = xrtFormat("capabilities[%u].config: invalid config object", i);
				return false;
			}
		}
	}
	pages = ValueGet(spec, "pages");
	if (pages && xrtValueType(pages) != XVALUE_OBJECT) {
		if (error) *error = xrtStrDup("pages: invalid pages object");
		return false;
	}
	if (pages) {
		xvalue* displayGroups = ValueGet(pages, "displayGroups");
		xvalue* fieldGroups = ValueGet(pages, "fieldGroups");
		if (displayGroups && xrtValueType(displayGroups) != XVALUE_ARRAY) {
			if (error) *error = xrtStrDup("pages.displayGroups: invalid group list");
			return false;
		}
		if (fieldGroups && xrtValueType(fieldGroups) != XVALUE_ARRAY) {
			if (error) *error = xrtStrDup("pages.fieldGroups: invalid group list");
			return false;
		}
	}
	policies = ValueGet(spec, "policies");
	if (policies && xrtValueType(policies) != XVALUE_OBJECT) {
		if (error) *error = xrtStrDup("policies: invalid policies object");
		return false;
	}
	return true;
}

static uint64 Content_SpecHash64(const char* text)
{
	uint64 h = 1469598103934665603ULL;
	const unsigned char* p = (const unsigned char*)(text ? text : "");
	while (*p) {
		h ^= (uint64)(*p++);
		h *= 1099511628211ULL;
	}
	return h;
}

static str Content_SpecHashText(const char* specJson)
{
	uint64 h = Content_SpecHash64(specJson);
	return xrtFormat("%08x%08x", (uint32)(h >> 32), (uint32)(h & 0xffffffffu));
}

/* ==================== 模型 CRUD（content_model / revision） ==================== */

static void Content_BindText(sqlite3_stmt* stmt, int index, const char* text)
{
	sqlite3_bind_text(stmt, index, text ? text : "", -1, SQLITE_TRANSIENT);
}

static int Content_CountFields(xvalue* spec)
{
	xvalue* fields;
	if (!spec || xrtValueType(spec) != XVALUE_OBJECT) return 0;
	fields = ValueGet(spec, "fields");
	if (fields && xrtValueType(fields) == XVALUE_ARRAY) return (int)ValueCount(fields);
	return 0;
}

static bool Content_SyncModelCapabilities(int modelId, xvalue* spec, int64 now)
{
	sqlite3_stmt* stmt = NULL;
	xvalue* capabilities;
	if (!G_DB || modelId <= 0) return false;
	if (sqlite3_prepare_v2(G_DB, "DELETE FROM content_model_pack WHERE model_id=?", -1, &stmt, NULL) != SQLITE_OK)
		return false;
	sqlite3_bind_int(stmt, 1, modelId);
	if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); return false; }
	sqlite3_finalize(stmt);
	if (!spec || xrtValueType(spec) != XVALUE_OBJECT) return true;
	capabilities = ValueGet(spec, "capabilities");
	if (!capabilities || xrtValueType(capabilities) != XVALUE_ARRAY) return true;
	{
		uint32 i;
		for (i = 0; i < ValueCount(capabilities); i++) {
			xvalue* item = xrtValueArrayGet(capabilities, i);
			str key; char* configJson = NULL; char* mountJson = NULL;
			xvalue* config; xvalue* mount;
			bool enabled = true;
			if (!item || xrtValueType(item) != XVALUE_OBJECT) continue;
			key = ValueText(item, "key");
			key = (str)ContentPack_NormalizeId(key);
			if (!key || !key[0]) continue;
			if (ValueHas(item, "enabled")) enabled = ValueBool(item, "enabled");
			if (!enabled) continue;
			config = ValueGet(item, "config");
			mount = ValueGet(item, "mount");
			{
				size_t size = 0;
				configJson = config ? xrtJsonStringify(config, false, &size) : xrtStrDup("{}");
				mountJson = mount ? xrtJsonStringify(mount, false, &size) : xrtStrDup("{}");
			}
			if (sqlite3_prepare_v2(G_DB, "INSERT INTO content_model_pack (model_id,pack_id,enabled,instance_options_json,mount_json,sort,create_time,update_time) VALUES (?,?,1,?,?,?,?,?)", -1, &stmt, NULL) != SQLITE_OK) {
				xrtFree(configJson); xrtFree(mountJson);
				return false;
			}
			sqlite3_bind_int(stmt, 1, modelId);
			Content_BindText(stmt, 2, key);
			Content_BindText(stmt, 3, configJson);
			Content_BindText(stmt, 4, mountJson);
			sqlite3_bind_int(stmt, 5, (int)i);
			sqlite3_bind_int64(stmt, 6, now);
			sqlite3_bind_int64(stmt, 7, now);
			if (sqlite3_step(stmt) != SQLITE_DONE) {
				sqlite3_finalize(stmt);
				xrtFree(configJson); xrtFree(mountJson);
				return false;
			}
			sqlite3_finalize(stmt);
			xrtFree(configJson); xrtFree(mountJson);
		}
	}
	return true;
}

static xvalue* Content_RowToModel(sqlite3_stmt* stmt)
{
	xvalue* row = ValueObject();
	ValueSetInt(row, "id", sqlite3_column_int(stmt, 0));
	ValueSetText(row, "xid", (const char*)sqlite3_column_text(stmt, 1));
	ValueSetText(row, "name", (const char*)sqlite3_column_text(stmt, 2));
	ValueSetText(row, "namespace", (const char*)sqlite3_column_text(stmt, 3));
	ValueSetText(row, "title", (const char*)sqlite3_column_text(stmt, 4));
	ValueSetText(row, "description", (const char*)sqlite3_column_text(stmt, 5));
	ValueSetText(row, "icon", (const char*)sqlite3_column_text(stmt, 6));
	ValueSetText(row, "tableName", (const char*)sqlite3_column_text(stmt, 7));
	ValueSetText(row, "status", (const char*)sqlite3_column_text(stmt, 8));
	ValueSetInt(row, "fieldCount", sqlite3_column_int(stmt, 9));
	ValueSetInt(row, "currentRevision", sqlite3_column_int(stmt, 10));
	ValueSetInt(row, "appliedRevision", sqlite3_column_int(stmt, 11));
	ValueSetText(row, "generatedPluginXid", (const char*)sqlite3_column_text(stmt, 12));
	ValueSetText(row, "specJson", (const char*)sqlite3_column_text(stmt, 13));
	ValueSetText(row, "specHash", (const char*)sqlite3_column_text(stmt, 14));
	{
		xtime t = sqlite3_column_int64(stmt, 15);
		ValueSetOwnedText(row, "createTime", TimeText(t, TIME_TEXT_DATETIME));
		t = sqlite3_column_int64(stmt, 16);
		ValueSetOwnedText(row, "updateTime", TimeText(t, TIME_TEXT_DATETIME));
	}
	return row;
}

static bool Content_FindModelIdAndRevision(const char* xid, int* pModelId, int* pRevision)
{
	sqlite3_stmt* stmt = NULL;
	bool found = false;
	if (!G_DB || !xid) return false;
	if (sqlite3_prepare_v2(G_DB, "SELECT id,current_revision FROM content_model WHERE xid = ? AND status <> 'deleted' LIMIT 1", -1, &stmt, NULL) != SQLITE_OK)
		return false;
	Content_BindText(stmt, 1, xid);
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		if (pModelId) *pModelId = sqlite3_column_int(stmt, 0);
		if (pRevision) *pRevision = sqlite3_column_int(stmt, 1);
		found = true;
	}
	sqlite3_finalize(stmt);
	return found;
}

static xvalue* Content_ListModels(int page, int limit)
{
	sqlite3_stmt* stmt = NULL;
	xvalue* data = ValueObject();
	xvalue* rows = ValueArray();
	int offset, total = 0;
	if (page <= 0) page = 1;
	if (limit <= 0 || limit > 100) limit = 20;
	offset = (page - 1) * limit;
	if (sqlite3_prepare_v2(G_DB, "SELECT COUNT(*) FROM content_model WHERE status <> 'deleted'", -1, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW) total = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}
	if (sqlite3_prepare_v2(G_DB, "SELECT id,xid,name,namespace,title,description,icon,table_name,status,field_count,current_revision,applied_revision,generated_plugin_xid,spec_json,spec_hash,create_time,update_time FROM content_model WHERE status <> 'deleted' ORDER BY update_time DESC,id DESC LIMIT ? OFFSET ?", -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int(stmt, 1, limit);
		sqlite3_bind_int(stmt, 2, offset);
		while (sqlite3_step(stmt) == SQLITE_ROW) ValueArrayOwn(rows, Content_RowToModel(stmt));
		sqlite3_finalize(stmt);
	}
	ValueSetInt(data, "count", total);
	ValueSetOwn(data, "items", rows);
	return data;
}

static xvalue* Content_GetModelByXid(const char* xid)
{
	sqlite3_stmt* stmt = NULL;
	xvalue* model = NULL;
	if (!Content_IsValidXid(xid)) return NULL;
	if (sqlite3_prepare_v2(G_DB, "SELECT id,xid,name,namespace,title,description,icon,table_name,status,field_count,current_revision,applied_revision,generated_plugin_xid,spec_json,spec_hash,create_time,update_time FROM content_model WHERE xid = ? AND status <> 'deleted' LIMIT 1", -1, &stmt, NULL) != SQLITE_OK)
		return NULL;
	Content_BindText(stmt, 1, xid);
	if (sqlite3_step(stmt) == SQLITE_ROW) model = Content_RowToModel(stmt);
	sqlite3_finalize(stmt);
	return model;
}

static bool Content_DeleteModelByXid(const char* xid, str* error)
{
	sqlite3_stmt* stmt = NULL;
	int modelId = 0;
	int64 now = XAdmin_UnixNowUs();
	bool ok;
	if (error) *error = NULL;
	if (!Content_IsValidXid(xid)) {
		if (error) *error = xrtStrDup("invalid xid");
		return false;
	}
	if (!Content_FindModelIdAndRevision(xid, &modelId, NULL) || modelId <= 0) {
		if (error) *error = xrtStrDup("model not found");
		return false;
	}
	if (!ContentDB_BeginImmediate()) {
		if (error) *error = xrtStrDup("begin transaction failed");
		return false;
	}
	if (sqlite3_prepare_v2(G_DB, "UPDATE content_model SET status='deleted', update_time=? WHERE id=? AND status <> 'deleted'", -1, &stmt, NULL) != SQLITE_OK) {
		ContentDB_Rollback();
		if (error) *error = xrtStrDup("prepare delete failed");
		return false;
	}
	sqlite3_bind_int64(stmt, 1, now);
	sqlite3_bind_int(stmt, 2, modelId);
	ok = (sqlite3_step(stmt) == SQLITE_DONE) && (sqlite3_changes(G_DB) > 0);
	sqlite3_finalize(stmt);
	if (!ok) {
		ContentDB_Rollback();
		if (error) *error = xrtStrDup("delete model failed");
		return false;
	}
	if (sqlite3_prepare_v2(G_DB, "DELETE FROM content_model_pack WHERE model_id=?", -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int(stmt, 1, modelId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	ContentDB_Commit();
	return true;
}

static xvalue* Content_SaveModelSpec(xvalue* spec, str* error)
{
	char* specJson; str specHash; char* oldSpecHash = NULL;
	size_t specSize = 0;
	str xid, name, title, ns, description, icon, tableName, generatedPluginXid, note;
	int fieldCount, modelId = 0, currentRevision = 0, newRevision;
	int64 now = XAdmin_UnixNowUs();
	sqlite3_stmt* stmt = NULL;
	xvalue* data;
	if (error) *error = NULL;
	if (!spec || xrtValueType(spec) != XVALUE_OBJECT) {
		if (error) *error = xrtStrDup("invalid json body");
		return NULL;
	}
	xid = ValueText(spec, "xid");
	name = ValueText(spec, "name");
	title = ValueText(spec, "title");
	ns = ValueText(spec, "namespace");
	description = ValueText(spec, "description");
	if (!description) description = ValueText(spec, "desc");
	icon = ValueText(spec, "icon");
	tableName = ValueText(spec, "tableName");
	if (!tableName) tableName = ValueText(spec, "table_name");
	generatedPluginXid = ValueText(spec, "generatedPluginXid");
	note = ValueText(spec, "note");
	if (!Content_IsValidXid(xid)) {
		if (error) *error = xrtStrDup("invalid xid");
		return NULL;
	}
	if (!name || !name[0]) name = xid;
	if (!title || !title[0]) {
		if (error) *error = xrtStrDup("title is required");
		return NULL;
	}
	if (!Content_SpecValidate(spec, error)) return NULL;
	fieldCount = Content_CountFields(spec);
	specJson = xrtJsonStringify(spec, false, &specSize);
	if (!specJson) {
		if (error) *error = xrtStrDup("stringify spec failed");
		return NULL;
	}
	specHash = Content_SpecHashText(specJson);
	Content_FindModelIdAndRevision(xid, &modelId, &currentRevision);
	newRevision = currentRevision + 1;
	if (modelId > 0) {
		if (sqlite3_prepare_v2(G_DB, "SELECT spec_hash FROM content_model WHERE id=? LIMIT 1", -1, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_int(stmt, 1, modelId);
			if (sqlite3_step(stmt) == SQLITE_ROW) oldSpecHash = xrtStrDup((const char*)sqlite3_column_text(stmt, 0));
			sqlite3_finalize(stmt);
		}
		if (oldSpecHash && !strcmp(oldSpecHash, specHash)) {
			data = ValueObject();
			ValueSetInt(data, "id", modelId);
			ValueSetText(data, "xid", xid);
			ValueSetInt(data, "revision", currentRevision);
			ValueSetInt(data, "fieldCount", fieldCount);
			ValueSetInt(data, "updateTime", now);
			ValueSetBool(data, "unchanged", true);
			xrtFree(oldSpecHash); xrtFree(specHash); xrtFree(specJson);
			return data;
		}
		xrtFree(oldSpecHash);
	}
	if (!ContentDB_BeginImmediate()) {
		xrtFree(specHash); xrtFree(specJson);
		if (error) *error = xrtStrDup("begin transaction failed");
		return NULL;
	}
	if (modelId > 0) {
		if (sqlite3_prepare_v2(G_DB, "UPDATE content_model SET name=?,namespace=?,title=?,description=?,icon=?,table_name=?,field_count=?,current_revision=?,generated_plugin_xid=?,spec_json=?,spec_hash=?,update_time=? WHERE id=?", -1, &stmt, NULL) != SQLITE_OK) {
			ContentDB_Rollback(); xrtFree(specHash); xrtFree(specJson);
			if (error) *error = xrtStrDup("prepare update failed");
			return NULL;
		}
		Content_BindText(stmt, 1, name);
		Content_BindText(stmt, 2, ns);
		Content_BindText(stmt, 3, title);
		Content_BindText(stmt, 4, description);
		Content_BindText(stmt, 5, icon);
		Content_BindText(stmt, 6, tableName);
		sqlite3_bind_int(stmt, 7, fieldCount);
		sqlite3_bind_int(stmt, 8, newRevision);
		Content_BindText(stmt, 9, generatedPluginXid);
		Content_BindText(stmt, 10, specJson);
		Content_BindText(stmt, 11, specHash);
		sqlite3_bind_int64(stmt, 12, now);
		sqlite3_bind_int(stmt, 13, modelId);
	} else {
		if (sqlite3_prepare_v2(G_DB, "INSERT INTO content_model (xid,name,namespace,title,description,icon,table_name,status,field_count,current_revision,applied_revision,generated_plugin_xid,spec_json,spec_hash,create_time,update_time) VALUES (?,?,?,?,?,?,?,'active',?,1,0,?,?,?,?,?)", -1, &stmt, NULL) != SQLITE_OK) {
			ContentDB_Rollback(); xrtFree(specHash); xrtFree(specJson);
			if (error) *error = xrtStrDup("prepare insert failed");
			return NULL;
		}
		Content_BindText(stmt, 1, xid);
		Content_BindText(stmt, 2, name);
		Content_BindText(stmt, 3, ns);
		Content_BindText(stmt, 4, title);
		Content_BindText(stmt, 5, description);
		Content_BindText(stmt, 6, icon);
		Content_BindText(stmt, 7, tableName);
		sqlite3_bind_int(stmt, 8, fieldCount);
		Content_BindText(stmt, 9, generatedPluginXid);
		Content_BindText(stmt, 10, specJson);
		Content_BindText(stmt, 11, specHash);
		sqlite3_bind_int64(stmt, 12, now);
		sqlite3_bind_int64(stmt, 13, now);
	}
	if (sqlite3_step(stmt) != SQLITE_DONE) {
		sqlite3_finalize(stmt);
		ContentDB_Rollback(); xrtFree(specHash); xrtFree(specJson);
		if (error) *error = xrtStrDup("save model failed");
		return NULL;
	}
	sqlite3_finalize(stmt);
	if (modelId <= 0) modelId = (int)sqlite3_last_insert_rowid(G_DB);
	if (sqlite3_prepare_v2(G_DB, "INSERT INTO content_model_revision (model_id,revision,spec_json,spec_hash,note,generator_version,create_time) VALUES (?,?,?,?,?,'builtin-0.1',?)", -1, &stmt, NULL) != SQLITE_OK) {
		ContentDB_Rollback(); xrtFree(specHash); xrtFree(specJson);
		if (error) *error = xrtStrDup("prepare revision failed");
		return NULL;
	}
	sqlite3_bind_int(stmt, 1, modelId);
	sqlite3_bind_int(stmt, 2, newRevision);
	Content_BindText(stmt, 3, specJson);
	Content_BindText(stmt, 4, specHash);
	Content_BindText(stmt, 5, note);
	sqlite3_bind_int64(stmt, 6, now);
	if (sqlite3_step(stmt) != SQLITE_DONE) {
		sqlite3_finalize(stmt);
		ContentDB_Rollback(); xrtFree(specHash); xrtFree(specJson);
		if (error) *error = xrtStrDup("save revision failed");
		return NULL;
	}
	sqlite3_finalize(stmt);
	if (!Content_SyncModelCapabilities(modelId, spec, now)) {
		ContentDB_Rollback(); xrtFree(specHash); xrtFree(specJson);
		if (error) *error = xrtStrDup("sync capabilities failed");
		return NULL;
	}
	ContentDB_Commit();
	data = ValueObject();
	ValueSetInt(data, "id", modelId);
	ValueSetText(data, "xid", xid);
	ValueSetInt(data, "revision", newRevision);
	ValueSetInt(data, "fieldCount", fieldCount);
	ValueSetInt(data, "updateTime", now);
	ValueSetText(data, "specHash", specHash);
	xrtFree(specHash);
	xrtFree(specJson);
	return data;
}

static xvalue* Content_ListRevisions(int modelId)
{
	sqlite3_stmt* stmt = NULL;
	xvalue* rows = ValueArray();
	if (modelId <= 0) return rows;
	if (sqlite3_prepare_v2(G_DB, "SELECT revision,note,generator_version,create_time FROM content_model_revision WHERE model_id=? ORDER BY revision DESC", -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int(stmt, 1, modelId);
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			xvalue* row = ValueObject();
			ValueSetInt(row, "revision", sqlite3_column_int(stmt, 0));
			ValueSetText(row, "note", (const char*)sqlite3_column_text(stmt, 1));
			ValueSetText(row, "generatorVersion", (const char*)sqlite3_column_text(stmt, 2));
			ValueSetOwnedText(row, "createTime", TimeText(sqlite3_column_int64(stmt, 3), TIME_TEXT_DATETIME));
			ValueArrayOwn(rows, row);
		}
		sqlite3_finalize(stmt);
	}
	return rows;
}

static xvalue* Content_ListGenerations(int modelId)
{
	sqlite3_stmt* stmt = NULL;
	xvalue* rows = ValueArray();
	if (modelId <= 0) return rows;
	if (sqlite3_prepare_v2(G_DB, "SELECT target_revision,plugin_xid,status,output_json,error_message,create_time,finish_time FROM content_generation WHERE model_id=? ORDER BY create_time DESC,id DESC LIMIT 20", -1, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int(stmt, 1, modelId);
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			xvalue* row = ValueObject();
			ValueSetInt(row, "targetRevision", sqlite3_column_int(stmt, 0));
			ValueSetText(row, "pluginXid", (const char*)sqlite3_column_text(stmt, 1));
			ValueSetText(row, "status", (const char*)sqlite3_column_text(stmt, 2));
			ValueSetText(row, "outputJson", (const char*)sqlite3_column_text(stmt, 3));
			ValueSetText(row, "errorMessage", (const char*)sqlite3_column_text(stmt, 4));
			ValueSetOwnedText(row, "createTime", TimeText(sqlite3_column_int64(stmt, 5), TIME_TEXT_DATETIME));
			ValueSetOwnedText(row, "finishTime", TimeText(sqlite3_column_int64(stmt, 6), TIME_TEXT_DATETIME));
			ValueArrayOwn(rows, row);
		}
		sqlite3_finalize(stmt);
	}
	return rows;
}

/* ==================== advisor 体检（content_advisor） ==================== */

static xvalue* Content_CreateAdvisorItem(const char* level, const char* message, const char* field)
{
	xvalue* item = ValueObject();
	ValueSetText(item, "level", (level && level[0]) ? level : "info");
	ValueSetText(item, "message", message ? message : "");
	if (field && field[0]) ValueSetText(item, "field", field);
	return item;
}

static xvalue* Content_CreateCapabilityAcceptanceItem(const char* packId, const char* apiPath, const char* viewPath)
{
	xvalue* item = Content_CreateAdvisorItem("info", "能力包验收路径已注册", packId);
	ValueSetText(item, "kind", "acceptance");
	ValueSetText(item, "packId", packId ? packId : "");
	ValueSetText(item, "apiPath", apiPath ? apiPath : "");
	ValueSetText(item, "viewPath", viewPath ? viewPath : "");
	return item;
}

static char* Content_CopyAbilityManifestText(const char* packId, const char* key)
{
	xvalue* pack = NULL;
	str manifestJson;
	xvalue* manifest = NULL;
	str value = NULL;
	char* ret = NULL;
	if (!packId || !key) return NULL;
	pack = ContentPack_GetDetail(packId);
	if (!pack) return NULL;
	manifestJson = ValueText(pack, "manifestJson");
	if (manifestJson && manifestJson[0]) {
		manifest = xrtJsonParse(xrtStrView(manifestJson));
		if (manifest && xrtValueType(manifest) == XVALUE_OBJECT) {
			value = ValueText(manifest, key);
			if (value && value[0]) ret = xrtStrDup(value);
		}
	}
	xrtValueRelease(manifest);
	xrtValueRelease(pack);
	return ret;
}

static void Content_AppendCapabilityAdvisorItems(xvalue* items, xvalue* capabilities)
{
	uint32 i;
	if (!items || !capabilities || xrtValueType(capabilities) != XVALUE_ARRAY) return;
	for (i = 0; i < ValueCount(capabilities); i++) {
		xvalue* cap = xrtValueArrayGet(capabilities, i);
		const char* packId = (cap && xrtValueType(cap) == XVALUE_OBJECT) ? ValueText(cap, "key") : NULL;
		const char* normalized = ContentPack_NormalizeId(packId);
		char* manifestMessage = Content_CopyAbilityManifestText(normalized, "advisorMessage");
		char* manifestApiPath = Content_CopyAbilityManifestText(normalized, "acceptanceApiPath");
		char* manifestViewPath = Content_CopyAbilityManifestText(normalized, "acceptanceViewPath");
		const char* message = manifestMessage ? manifestMessage : "能力包已启用：生产发布前请检查生成路由、数据结构、权限和运行成本。";
		const char* apiPath = manifestApiPath ? manifestApiPath : "/admin/api/plugin/{pluginXid}/contracts";
		const char* viewPath = manifestViewPath ? manifestViewPath : "/admin/view/plugin/{pluginXid}";
		ValueArrayOwn(items, Content_CreateAdvisorItem("warning", message, normalized));
		ValueArrayOwn(items, Content_CreateCapabilityAcceptanceItem(normalized, apiPath, viewPath));
		xrtFree(manifestMessage);
		xrtFree(manifestApiPath);
		xrtFree(manifestViewPath);
	}
}

static xvalue* Content_BuildAdvisor(xvalue* spec)
{
	xvalue* ret = ValueObject();
	xvalue* items = ValueArray();
	xvalue* fields = NULL; xvalue* pages = NULL; xvalue* capabilities = NULL;
	str xid = NULL, title = NULL;
	int errorCount = 0;
	if (spec && xrtValueType(spec) == XVALUE_OBJECT) {
		xid = ValueText(spec, "xid");
		title = ValueText(spec, "title");
		fields = ValueGet(spec, "fields");
		pages = ValueGet(spec, "pages");
		capabilities = ValueGet(spec, "capabilities");
	}
	if (!Content_IsValidXid(xid)) {
		ValueArrayOwn(items, Content_CreateAdvisorItem("error", "模型 XID 缺失或不合法", NULL));
		errorCount++;
	}
	if (!title || !title[0]) {
		ValueArrayOwn(items, Content_CreateAdvisorItem("error", "模型标题不能为空", NULL));
		errorCount++;
	}
	if (!fields || xrtValueType(fields) != XVALUE_ARRAY || ValueCount(fields) == 0) {
		ValueArrayOwn(items, Content_CreateAdvisorItem("error", "至少需要一个字段", NULL));
		errorCount++;
	} else {
		uint32 i, j;
		for (i = 0; i < ValueCount(fields); i++) {
			xvalue* field = xrtValueArrayGet(fields, i);
			str name = (field && xrtValueType(field) == XVALUE_OBJECT) ? ValueText(field, "name") : NULL;
			if (!name || !name[0]) {
				ValueArrayOwn(items, Content_CreateAdvisorItem("error", "字段名不能为空", NULL));
				errorCount++;
				continue;
			}
			for (j = i + 1; j < ValueCount(fields); j++) {
				xvalue* other = xrtValueArrayGet(fields, j);
				str otherName = (other && xrtValueType(other) == XVALUE_OBJECT) ? ValueText(other, "name") : NULL;
				if (otherName && !strcmp(name, otherName)) {
					ValueArrayOwn(items, Content_CreateAdvisorItem("error", "字段名重复", name));
					errorCount++;
				}
			}
		}
	}
	if (fields && xrtValueType(fields) == XVALUE_ARRAY) {
		char* message = xrtFormat("字段：%u 个字段会生成到 payload 和后台视图中", (unsigned)ValueCount(fields));
		ValueArrayOwn(items, Content_CreateAdvisorItem("info", message, "fields"));
		xrtFree(message);
	}
	if (pages && xrtValueType(pages) == XVALUE_OBJECT)
		ValueArrayOwn(items, Content_CreateAdvisorItem("info", "页面：后台、公开列表和详情页开关会影响生成路由和页面文件", "pages"));
	if (capabilities && xrtValueType(capabilities) == XVALUE_ARRAY && ValueCount(capabilities) > 0) {
		char* message = xrtFormat("能力包：%u 个能力项会写入生成契约", (unsigned)ValueCount(capabilities));
		ValueArrayOwn(items, Content_CreateAdvisorItem("info", message, "capabilities"));
		xrtFree(message);
		Content_AppendCapabilityAdvisorItems(items, capabilities);
	}
	ValueArrayOwn(items, Content_CreateAdvisorItem("info", "数据库：业务记录会保存在生成插件的私有数据库中", "database"));
	ValueSetText(ret, "status", errorCount > 0 ? "error" : "ok");
	ValueSetInt(ret, "errorCount", errorCount);
	ValueSetOwn(ret, "items", items);
	return ret;
}

/* ==================== 菜单装配与入口（content_init） ==================== */

static bool Content_EnsureMenuItem(int parent, const char* title, const char* href, const char* icon, int sort, const char* remark)
{
	sqlite3_stmt* stmt = NULL;
	int menuId = 0;
	int64 now = XAdmin_UnixNowUs();
	bool ok;
	if (sqlite3_prepare_v2(G_DB, "SELECT id FROM menu WHERE isDelete = 0 AND href = ? LIMIT 1", -1, &stmt, NULL) == SQLITE_OK) {
		Content_BindText(stmt, 1, href);
		if (sqlite3_step(stmt) == SQLITE_ROW) menuId = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}
	if (menuId > 0) {
		if (sqlite3_prepare_v2(G_DB, "UPDATE menu SET parent=?,title=?,icon=?,type=1,openType='_component',href=?,sort=?,visible=1,remark=?,updateTime=? WHERE id=?", -1, &stmt, NULL) != SQLITE_OK)
			return false;
		sqlite3_bind_int(stmt, 1, parent);
		Content_BindText(stmt, 2, title);
		Content_BindText(stmt, 3, icon);
		Content_BindText(stmt, 4, href);
		sqlite3_bind_int(stmt, 5, sort);
		Content_BindText(stmt, 6, remark);
		sqlite3_bind_int64(stmt, 7, now);
		sqlite3_bind_int(stmt, 8, menuId);
	} else {
		if (sqlite3_prepare_v2(G_DB, "INSERT INTO menu (parent,title,icon,type,openType,href,sort,visible,remark,createTime,updateTime,isDelete) VALUES (?,?,?,1,'_component',?,?,1,?,?,?,0)", -1, &stmt, NULL) != SQLITE_OK)
			return false;
		sqlite3_bind_int(stmt, 1, parent);
		Content_BindText(stmt, 2, title);
		Content_BindText(stmt, 3, icon);
		Content_BindText(stmt, 4, href);
		sqlite3_bind_int(stmt, 5, sort);
		Content_BindText(stmt, 6, remark);
		sqlite3_bind_int64(stmt, 7, now);
		sqlite3_bind_int64(stmt, 8, now);
	}
	ok = sqlite3_step(stmt) == SQLITE_DONE;
	sqlite3_finalize(stmt);
	return ok;
}

static bool Content_EnsureMenu(void)
{
	sqlite3_stmt* stmt = NULL;
	int menuId = 0;
	int64 now = XAdmin_UnixNowUs();
	if (sqlite3_prepare_v2(G_DB, "SELECT id FROM menu WHERE isDelete = 0 AND (href = '/admin/view/content' OR title = ? OR title = ? OR title = 'Content Model') ORDER BY id ASC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK) {
		Content_BindText(stmt, 1, "内容管理");
		Content_BindText(stmt, 2, "内容模型");
		if (sqlite3_step(stmt) == SQLITE_ROW) menuId = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}
	if (menuId > 0) {
		if (sqlite3_prepare_v2(G_DB, "UPDATE menu SET parent=0,title=?,icon='layui-icon layui-icon-template-1',type=0,openType='',href='',sort=540100,visible=1,remark=?,updateTime=? WHERE id=?", -1, &stmt, NULL) != SQLITE_OK)
			return false;
		Content_BindText(stmt, 1, "内容管理");
		Content_BindText(stmt, 2, "内容管理与能力包系统");
		sqlite3_bind_int64(stmt, 3, now);
		sqlite3_bind_int(stmt, 4, menuId);
	} else {
		if (sqlite3_prepare_v2(G_DB, "INSERT INTO menu (parent,title,icon,type,openType,href,sort,visible,remark,createTime,updateTime,isDelete) VALUES (0,?,'layui-icon layui-icon-template-1',0,'','',540100,1,?,?,?,0)", -1, &stmt, NULL) != SQLITE_OK)
			return false;
		Content_BindText(stmt, 1, "内容管理");
		Content_BindText(stmt, 2, "内容管理与能力包系统");
		sqlite3_bind_int64(stmt, 3, now);
		sqlite3_bind_int64(stmt, 4, now);
	}
	if (sqlite3_step(stmt) != SQLITE_DONE) {
		sqlite3_finalize(stmt);
		return false;
	}
	sqlite3_finalize(stmt);
	if (menuId <= 0) {
		if (sqlite3_prepare_v2(G_DB, "SELECT id FROM menu WHERE isDelete = 0 AND title = ? ORDER BY id ASC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK) {
			Content_BindText(stmt, 1, "内容管理");
			if (sqlite3_step(stmt) == SQLITE_ROW) menuId = sqlite3_column_int(stmt, 0);
			sqlite3_finalize(stmt);
		}
	}
	if (menuId > 0) {
		if (sqlite3_prepare_v2(G_DB, "UPDATE menu SET isDelete=1,updateTime=? WHERE isDelete=0 AND type=0 AND id<>? AND (title=? OR title='Content Model')", -1, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_int64(stmt, 1, now);
			sqlite3_bind_int(stmt, 2, menuId);
			Content_BindText(stmt, 3, "内容模型");
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}
	return Content_EnsureMenuItem(menuId, "独立页面", "/admin/view/content/page", "layui-icon layui-icon-template", 540105, "独立页面管理")
		&& Content_EnsureMenuItem(menuId, "模型管理", "/admin/view/content", "layui-icon layui-icon-list", 540110, "内容模型管理")
		&& Content_EnsureMenuItem(menuId, "能力包商店", "/admin/view/content/pack-store", "layui-icon layui-icon-cart", 540120, "能力包商店")
		&& Content_EnsureMenuItem(menuId, "能力包管理", "/admin/view/content/packs", "layui-icon layui-icon-component", 540130, "本机能力包管理");
}

static bool Content_Init(void)
{
	printf("        Content_Init \n");
	if (!ContentDB_Init()) {
		printf("[content] database init failed\n");
		return false;
	}
	if (!ContentPack_Init()) {
		printf("[content] capability pack init failed\n");
		return false;
	}
	if (!Content_EnsureMenu()) {
		printf("[content] menu init failed\n");
		return false;
	}
	return true;
}
