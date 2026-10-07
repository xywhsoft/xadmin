/* 独立页面管理路由（v1 语义：CRUD + 视图）+ 前台站点路由。 */

static void Standalone_ReplyJson(XS_ResponseObject resp, xvalue* ret)
{
	size_t size = 0;
	char* json = xrtJsonStringify(ret, false, &size);
	xsHttpReplyAuto(resp, 200, "application/json; charset=utf-8", json, 0);
	xrtFree(json);
	xrtValueRelease(ret);
}

static void Standalone_ReplyError(XS_ResponseObject resp, const char* message)
{
	xvalue* ret = ValueObject();
	ValueSetBool(ret, "result", false);
	ValueSetText(ret, "message", message ? message : "error");
	Standalone_ReplyJson(resp, ret);
}

static void Standalone_ReplySuccess(XS_ResponseObject resp, xvalue* data)
{
	xvalue* ret = ValueObject();
	ValueSetBool(ret, "result", true);
	if (data) ValueSetOwn(ret, "data", data);
	Standalone_ReplyJson(resp, ret);
}

static xvalue* Standalone_RowToValue(sqlite3_stmt* stmt)
{
	xvalue* row = ValueObject();
	ValueSetInt(row, "id", sqlite3_column_int64(stmt, 0));
	ValueSetText(row, "title", (const char*)sqlite3_column_text(stmt, 1));
	ValueSetText(row, "status", (const char*)sqlite3_column_text(stmt, 2));
	ValueSetText(row, "uris", (const char*)sqlite3_column_text(stmt, 3));
	ValueSetText(row, "header", (const char*)sqlite3_column_text(stmt, 4));
	ValueSetInt(row, "useTemplate", sqlite3_column_int(stmt, 5));
	ValueSetText(row, "content", (const char*)sqlite3_column_text(stmt, 6));
	ValueSetInt(row, "cacheSeconds", sqlite3_column_int64(stmt, 7));
	ValueSetOwnedText(row, "createTime", TimeText(sqlite3_column_int64(stmt, 8), TIME_TEXT_DATETIME));
	ValueSetOwnedText(row, "updateTime", TimeText(sqlite3_column_int64(stmt, 9), TIME_TEXT_DATETIME));
	return row;
}

// 独立页面管理列表页
void Request_View_Content_Page(XS_ServerObject s, XS_HostObject h, XS_RequestObject r, XS_ResponseObject w, xvalue* sess)
{
	(void)s; (void)h; (void)r; (void)sess;
	LoadPage(w, 200, HTTP_CT_HTML, "content/page.html");
}

// 独立页面列表 API
void Request_Content_Pages(XS_ServerObject s, XS_HostObject h, XS_RequestObject r, XS_ResponseObject w, xvalue* sess)
{
	sqlite3_stmt* stmt = NULL;
	xvalue* data = ValueArray();

	(void)s; (void)h; (void)r; (void)sess;
	if (sqlite3_prepare_v3(G_DB,
		"SELECT id,title,status,uris,header,useTemplate,content,cacheSeconds,createTime,updateTime "
		"FROM standalone_page WHERE status <> 'deleted' ORDER BY id DESC",
		-1, 0, &stmt, NULL) != SQLITE_OK) {
		Standalone_ReplyError(w, "query pages failed");
		xrtValueRelease(data);
		return;
	}
	while (sqlite3_step(stmt) == SQLITE_ROW)
		ValueArrayOwn(data, Standalone_RowToValue(stmt));
	sqlite3_finalize(stmt);
	Standalone_ReplySuccess(w, data);
}

// 独立页面详情
void Request_Content_Page(XS_ServerObject s, XS_HostObject h, XS_RequestObject r, XS_ResponseObject w, xvalue* sess)
{
	char idText[32] = {0};
	int64 id;
	sqlite3_stmt* stmt = NULL;

	(void)s; (void)h; (void)sess;
	xsReqQueryValue(r, "id", idText, sizeof(idText));
	id = Util_ParseI64(idText);
	if (id <= 0) {
		Standalone_ReplyError(w, "invalid page id");
		return;
	}
	if (sqlite3_prepare_v3(G_DB,
		"SELECT id,title,status,uris,header,useTemplate,content,cacheSeconds,createTime,updateTime "
		"FROM standalone_page WHERE id = ? AND status <> 'deleted'",
		-1, 0, &stmt, NULL) != SQLITE_OK) {
		Standalone_ReplyError(w, "query page failed");
		return;
	}
	sqlite3_bind_int64(stmt, 1, id);
	if (sqlite3_step(stmt) == SQLITE_ROW) {
		xvalue* row = Standalone_RowToValue(stmt);
		sqlite3_finalize(stmt);
		Standalone_ReplySuccess(w, row);
		return;
	}
	sqlite3_finalize(stmt);
	Standalone_ReplyError(w, "page not found");
}

// 独立页面保存（POST）
void Request_Content_Page_Save(XS_ServerObject s, XS_HostObject h, XS_RequestObject r, XS_ResponseObject w, xvalue* sess)
{
	xvalue* body = NULL;
	int64 id;
	str title, status, uris, header, content;
	str normalizedHeader = NULL;
	int64 useTemplate, cacheSeconds;
	sqlite3_stmt* stmt = NULL;
	xtime now = XAdmin_UnixNowUs();

	(void)s; (void)h; (void)sess;
	if (xsReqMethodID(r) != XHTTP_METHOD_POST) {
		Standalone_ReplyError(w, "method not allowed");
		return;
	}
	body = JsonParseN(XAdmin_ReqBody(r), XAdmin_ReqBodyLen(r));
	if (!body || xrtValueType(body) != XVALUE_OBJECT) {
		if (body) xrtValueRelease(body);
		Standalone_ReplyError(w, "invalid json body");
		return;
	}
	id = ValueInt(body, "id");
	title = ValueText(body, "title");
	status = ValueText(body, "status");
	uris = ValueText(body, "uris");
	header = ValueText(body, "header");
	content = ValueText(body, "content");
	useTemplate = ValueInt(body, "useTemplate");
	cacheSeconds = ValueInt(body, "cacheSeconds");

	if (!title || !title[0]) {
		xrtValueRelease(body);
		Standalone_ReplyError(w, "title required");
		return;
	}
	if (!status || (strcmp(status, "enabled") != 0 && strcmp(status, "draft") != 0))
		status = "draft";

	normalizedHeader = Standalone_NormalizeHeader(header, NULL);
	if (!normalizedHeader) {
		xrtValueRelease(body);
		Standalone_ReplyError(w, "header normalize failed");
		return;
	}

	if (id > 0) {
		if (sqlite3_prepare_v3(G_DB,
			"UPDATE standalone_page SET title=?,status=?,uris=?,header=?,useTemplate=?,content=?,cacheSeconds=?,updateTime=? WHERE id=?",
			-1, 0, &stmt, NULL) != SQLITE_OK) {
			xrtValueRelease(body); xrtFree(normalizedHeader);
			Standalone_ReplyError(w, "prepare update failed");
			return;
		}
	} else {
		if (sqlite3_prepare_v3(G_DB,
			"INSERT INTO standalone_page (title,status,uris,header,useTemplate,content,cacheSeconds,createTime,updateTime) VALUES (?,?,?,?,?,?,?,?,?)",
			-1, 0, &stmt, NULL) != SQLITE_OK) {
			xrtValueRelease(body); xrtFree(normalizedHeader);
			Standalone_ReplyError(w, "prepare insert failed");
			return;
		}
	}
	sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, status, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, uris ? uris : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 4, normalizedHeader, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 5, useTemplate ? 1 : 0);
	sqlite3_bind_text(stmt, 6, content ? content : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 7, cacheSeconds > 0 ? cacheSeconds : 0);
	if (id > 0) {
		sqlite3_bind_int64(stmt, 8, now);
		sqlite3_bind_int64(stmt, 9, id);
	} else {
		sqlite3_bind_int64(stmt, 8, now);
		sqlite3_bind_int64(stmt, 9, now);
	}
	if (sqlite3_step(stmt) != SQLITE_DONE) {
		sqlite3_finalize(stmt);
		xrtValueRelease(body); xrtFree(normalizedHeader);
		Standalone_ReplyError(w, "save page failed");
		return;
	}
	if (id <= 0) id = sqlite3_last_insert_rowid(G_DB);
	sqlite3_finalize(stmt);
	xrtFree(normalizedHeader);
	xrtValueRelease(body);

	/* 重载页面路由 */
	Standalone_LoadAll();
	{
		xvalue* ret = ValueObject();
		ValueSetInt(ret, "id", id);
		Standalone_ReplySuccess(w, ret);
	}
}

// 独立页面删除（POST）
void Request_Content_Page_Delete(XS_ServerObject s, XS_HostObject h, XS_RequestObject r, XS_ResponseObject w, xvalue* sess)
{
	xvalue* body = NULL;
	int64 id;
	sqlite3_stmt* stmt = NULL;

	(void)s; (void)h; (void)sess;
	if (xsReqMethodID(r) != XHTTP_METHOD_POST) {
		Standalone_ReplyError(w, "method not allowed");
		return;
	}
	body = JsonParseN(XAdmin_ReqBody(r), XAdmin_ReqBodyLen(r));
	if (!body || xrtValueType(body) != XVALUE_OBJECT) {
		if (body) xrtValueRelease(body);
		Standalone_ReplyError(w, "invalid json body");
		return;
	}
	id = ValueInt(body, "id");
	xrtValueRelease(body);
	if (id <= 0) {
		Standalone_ReplyError(w, "invalid page id");
		return;
	}
	if (sqlite3_prepare_v3(G_DB, "UPDATE standalone_page SET status='deleted', updateTime=? WHERE id=?",
		-1, 0, &stmt, NULL) != SQLITE_OK) {
		Standalone_ReplyError(w, "prepare delete failed");
		return;
	}
	sqlite3_bind_int64(stmt, 1, XAdmin_UnixNowUs());
	sqlite3_bind_int64(stmt, 2, id);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	Standalone_LoadAll();
	Standalone_ReplySuccess(w, NULL);
}

// ---- 前台站点路由 ----

#define SITE_ROUTE(name, page) \
	void name(XS_ServerObject s, XS_HostObject h, XS_RequestObject r, XS_ResponseObject w, xvalue* sess) \
	{ \
		(void)s; (void)h; (void)sess; \
		if (xsReqMethodID(r) == XHTTP_METHOD_GET) \
			Site_LoadPage(w, 200, HTTP_CT_HTML, page); \
		else \
			Site_LoadPage(w, 404, HTTP_CT_HTML, "404.html"); \
	}

SITE_ROUTE(Request_Site_Home, "index.html")
SITE_ROUTE(Request_Site_Features, "features.html")
SITE_ROUTE(Request_Site_Plugins, "plugins.html")
SITE_ROUTE(Request_Site_Capabilities, "capabilities.html")
SITE_ROUTE(Request_Site_ContentSystem, "content-system.html")
SITE_ROUTE(Request_Site_Docs, "docs.html")
SITE_ROUTE(Request_Site_Download, "download.html")
SITE_ROUTE(Request_Site_Demo, "demo.html")
