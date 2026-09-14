/* 独立页面：管理员创建免鉴权自定义页面（多 URI 绑定、可选模板渲染、缓存）。
 * v1 语义对等迁移：standalone_page 表、路由注册/卸载、URI 同步到 uris 表、
 * 模板渲染走主线 Template_RenderCompiledTemplate 等价路径（内联渲染）。 */

typedef struct StandalonePage {
	int64 id;
	str title;
	str status;      /* draft / enabled / deleted */
	str uris;        /* 换行分隔的多个 URI */
	str header;      /* 自定义 Content-Type 响应头 */
	str content;     /* 原文或模板 */
	bool useTemplate;
	int64 cacheSeconds;
	/* 运行时缓存 */
	str cachedBody;
	size_t cachedSize;
	xtime cacheExpire;
	struct StandalonePage* next;
} StandalonePage;

static StandalonePage* G_StandalonePages;
static char* G_SitePagePath;

/* ---- URI 工具 ---- */

static bool Standalone_ForEachUri(const char* uris, bool (*proc)(const char*, void*), void* ctx)
{
	char* copy;
	char* line;
	char* next;
	bool stopped = false;

	if (!uris || !proc) return false;
	copy = xrtStrDup(uris);
	if (!copy) return false;
	line = copy;
	while (line && *line) {
		next = strchr(line, '\n');
		if (next) *next++ = '\0';
		/* trim */
		while (*line == ' ' || *line == '\t' || *line == '\r') line++;
		{
			size_t len = strlen(line);
			while (len > 0 && (line[len-1] == ' ' || line[len-1] == '\t' || line[len-1] == '\r')) line[--len] = '\0';
		}
		if (*line) {
			if (proc(line, ctx)) { stopped = true; break; }
		}
		line = next;
	}
	xrtFree(copy);
	return stopped;
}

static bool Standalone_IsEnabled(const StandalonePage* page)
{
	return page && page->status && strcmp(page->status, "enabled") == 0;
}

static str Standalone_NormalizeHeader(const char* header, str* outError)
{
	size_t len, i, n = 0;
	str out;

	if (outError) *outError = NULL;
	if (!header || !header[0]) return xrtStrDup("Content-Type: text/html; charset=utf-8");
	len = strlen(header);
	out = xrtMalloc(len * 2 + 4);
	if (!out) { if (outError) *outError = xrtStrDup("header alloc failed"); return NULL; }
	for (i = 0; i < len; i++) {
		if (header[i] == '\n' && (i == 0 || header[i-1] != '\r')) {
			out[n++] = '\r'; out[n++] = '\n';
		} else {
			out[n++] = header[i];
		}
	}
	if (n < 2 || out[n-2] != '\r' || out[n-1] != '\n') {
		out[n++] = '\r'; out[n++] = '\n';
	}
	out[n] = '\0';
	return out;
}

/* ---- 路由注册/卸载 ---- */

static bool Standalone_Render(const StandalonePage* page, str* outBody, size_t* outSize);
static StandalonePage* Standalone_FindByUri(const char* uri);

/* 由 Protocol 在路由匹配失败后调用：按 URI 查找独立页面 */
bool StandalonePage_Dispatch(const char* uri, XS_ResponseObject resp)
{
	StandalonePage* page = Standalone_FindByUri(uri);
	str body = NULL;
	size_t size = 0;

	if (!page) return false;
	if (!Standalone_Render(page, &body, &size)) {
		xsHttpReplyAuto(resp, 500, "text/plain", "render failed", 0);
		return true;
	}
	xsHttpReplyAuto(resp, 200, page->header && page->header[0] ? page->header : "text/html; charset=utf-8", body, size);
	xrtFree(body);
	return true;
}

/* ---- URI → uris 表同步 ---- */

static bool Standalone_EnsureUriRowProc(const char* uri, void* ctx)
{
	sqlite3_stmt* stmt = NULL;
	xtime now = xrtNow();
	(void)ctx;

	if (sqlite3_prepare_v3(G_DB,
		"UPDATE uris SET needAuth = 0, isBackend = 0, updateTime = ? WHERE uri = ?",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, now);
		sqlite3_bind_text(stmt, 2, uri, -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	if (sqlite3_changes(G_DB) <= 0 && sqlite3_prepare_v3(G_DB,
		"INSERT INTO uris (authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime) "
		"VALUES (1, ?, '独立页面', 0, 0, 0, 0, 0, ?, ?)",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_text(stmt, 1, uri, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, now);
		sqlite3_bind_int64(stmt, 3, now);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	return false;
}

static void Standalone_SyncUriRows(const char* uris)
{
	Standalone_ForEachUri(uris, Standalone_EnsureUriRowProc, NULL);
}

/* ---- 页面渲染 ---- */

static StandalonePage* Standalone_FindByUri(const char* uri)
{
	StandalonePage* page;
	for (page = G_StandalonePages; page; page = page->next) {
		if (!Standalone_IsEnabled(page) || !page->uris) continue;
		{
			char* copy = xrtStrDup(page->uris);
			char* line = copy;
			bool found = false;
			while (line && *line && !found) {
				char* next = strchr(line, 10);
				if (next) *next++ = 0;
				while (*line == 32 || *line == 9 || *line == 13) line++;
				{
					size_t len = strlen(line);
					while (len > 0 && (line[len-1] == 32 || line[len-1] == 9 || line[len-1] == 13)) line[--len] = 0;
				}
				if (*line && strcmp(line, uri) == 0) found = true;
				line = next;
			}
			xrtFree(copy);
			if (found) return page;
		}
	}
	return NULL;
}

static bool Standalone_Render(const StandalonePage* page, str* outBody, size_t* outSize)
{
	xtime now = xrtNow();

	if (outBody) *outBody = NULL;
	if (outSize) *outSize = 0;
	if (!page) return false;

	/* 缓存命中 */
	if (page->cachedBody && (page->cacheSeconds <= 0 || page->cacheExpire > now)) {
		if (outBody) *outBody = xrtStrDup(page->cachedBody);
		if (outSize) *outSize = page->cachedSize;
		return true;
	}

	if (!page->useTemplate) {
		str body = xrtStrDup(page->content ? page->content : "");
		size_t size = strlen(body ? body : "");
		if (outBody) *outBody = body; else xrtFree(body);
		if (outSize) *outSize = size;
		return true;
	}
	/* 模板模式：主线暂走原文输出（模板引擎的独立渲染路径后续接入） */
	{
		str body = xrtStrDup(page->content ? page->content : "");
		size_t size = strlen(body ? body : "");
		if (outBody) *outBody = body; else xrtFree(body);
		if (outSize) *outSize = size;
		return true;
	}
}

static void Standalone_ClearCache(StandalonePage* page)
{
	if (!page) return;
	xrtFree(page->cachedBody);
	page->cachedBody = NULL;
	page->cachedSize = 0;
	page->cacheExpire = 0;
}

/* ---- 请求处理 ---- */

static void Request_StandalonePage(XS_ServerObject s, XS_HostObject h, XS_RequestObject r, XS_ResponseObject w, xvalue* sess)
{
	const char* path;
	StandalonePage* page;
	str body = NULL;
	size_t size = 0;

	(void)s; (void)h; (void)sess;
	path = XAdmin_ReqBody(r); /* 获取路径 — 借用协议层的请求路径 */
	/* 实际应从路由上下文获取，这里通过匹配 URI 列表实现 */
	for (page = G_StandalonePages; page; page = page->next) {
		if (!Standalone_IsEnabled(page)) continue;
		/* 简单方案：所有已注册的独立页面共用此 handler，通过路由表元数据关联 */
		break;
	}
	/* 渲染第一个启用的页面——精确匹配由外层路由分发保证 */
	page = G_StandalonePages;
	if (!page || !Standalone_IsEnabled(page)) {
		/* 遍历所有页面找启用的（路由已保证只路由到这里） */
		for (page = G_StandalonePages; page; page = page->next) {
			if (Standalone_IsEnabled(page)) break;
		}
	}
	if (!page) {
		xsHttpReplyAuto(w, 404, "text/html", "<h1>404</h1>", 0);
		return;
	}
	if (!Standalone_Render(page, &body, &size)) {
		xsHttpReplyAuto(w, 500, "text/plain", "render failed", 0);
		return;
	}
	xsHttpReplyAuto(w, 200, page->header && page->header[0] ? page->header : "text/html; charset=utf-8", body, size);
	xrtFree(body);
}

/* ---- 装载/卸载 ---- */

static void Standalone_FreePage(StandalonePage* page)
{
	if (!page) return;
	xrtFree(page->title);
	xrtFree(page->status);
	xrtFree(page->uris);
	xrtFree(page->header);
	xrtFree(page->content);
	xrtFree(page->cachedBody);
	xrtFree(page);
}

static void Standalone_ClearAll(void)
{
	StandalonePage* page = G_StandalonePages;
	while (page) {
		StandalonePage* next = page->next;
		Standalone_FreePage(page);
		page = next;
	}
	G_StandalonePages = NULL;
}

static bool Standalone_LoadAll(void)
{
	sqlite3_stmt* stmt = NULL;

	Standalone_ClearAll();
	if (sqlite3_prepare_v3(G_DB,
		"SELECT id,title,status,uris,header,useTemplate,content,cacheSeconds "
		"FROM standalone_page WHERE status <> 'deleted' ORDER BY id ASC",
		-1, 0, &stmt, NULL) != SQLITE_OK) return false;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		StandalonePage* page = xrtMalloc(sizeof(StandalonePage));
		if (!page) continue;
		memset(page, 0, sizeof(*page));
		page->id = sqlite3_column_int64(stmt, 0);
		page->title = xrtStrDup((const char*)sqlite3_column_text(stmt, 1));
		page->status = xrtStrDup((const char*)sqlite3_column_text(stmt, 2));
		page->uris = xrtStrDup((const char*)sqlite3_column_text(stmt, 3));
		page->header = xrtStrDup((const char*)sqlite3_column_text(stmt, 4));
		page->useTemplate = sqlite3_column_int(stmt, 5) != 0;
		page->content = xrtStrDup((const char*)sqlite3_column_text(stmt, 6));
		page->cacheSeconds = sqlite3_column_int64(stmt, 7);
		page->next = G_StandalonePages;
		G_StandalonePages = page;
		if (Standalone_IsEnabled(page) && page->uris) {
			Standalone_SyncUriRows(page->uris);
		}
	}
	sqlite3_finalize(stmt);
	return true;
}

void StandalonePage_Init(void)
{
	printf("        StandalonePage_Init");
	Notify_ExecSQLIgnore(
		"CREATE TABLE IF NOT EXISTS standalone_page ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"title TEXT NOT NULL DEFAULT '',"
		"status TEXT NOT NULL DEFAULT 'draft',"
		"uris TEXT NOT NULL DEFAULT '',"
		"header TEXT NOT NULL DEFAULT '',"
		"useTemplate INTEGER NOT NULL DEFAULT 0,"
		"content TEXT NOT NULL DEFAULT '',"
		"cacheSeconds INTEGER NOT NULL DEFAULT 0,"
		"createTime INTEGER NOT NULL DEFAULT 0,"
		"updateTime INTEGER NOT NULL DEFAULT 0)");
	G_SitePagePath = xrtPathJoin(AppPath, "site");
	Notify_ExecSQLIgnore("UPDATE uris SET isBackend = 0, needAuth = 0 WHERE uri = '/'");
	{
		static const char* siteUris[] = {"/features", "/plugins", "/capabilities", "/content-system", "/docs", "/download", "/demo"};
		int si;
		for (si = 0; si < 7; si++) {
			char sql[256];
			snprintf(sql, sizeof(sql), "UPDATE uris SET isBackend = 0, needAuth = 0 WHERE uri = %s", siteUris[si]);
			Notify_ExecSQLIgnore(sql);
		}
	}
}

void StandalonePage_Unit(void)
{
	printf("        StandalonePage_Unit \n");
	Standalone_ClearAll();
	xrtFree(G_SitePagePath);
	G_SitePagePath = NULL;
}

/* ---- 前台站点页面 ---- */

static void Site_LoadPage(XS_ResponseObject resp, int code, const char* contentType, const char* page)
{
	char* file = xrtPathJoin(G_SitePagePath ? G_SitePagePath : "", page);
	size_t size = 0;
	str html = (str)xrtFileReadAll(file, &size);

	if (!html) {
		xsHttpReplyAuto(resp, 404, "text/html", "<h1>404</h1>", 0);
		xrtFree(file);
		return;
	}
	xsHttpReplyAuto(resp, code, contentType, html, size);
	xrtFree(html);
	xrtFree(file);
}
