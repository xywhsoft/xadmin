#ifndef XADMIN_ROUTE_HTTP_STANDALONE_PAGE_H
#define XADMIN_ROUTE_HTTP_STANDALONE_PAGE_H

static void StandaloneAdmin_RowToValue(sqlite3_stmt* stmt, xvalue tblRow)
{
	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetText(tblRow, "title", 5, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
	xvoTableSetText(tblRow, "status", 6, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetText(tblRow, "uris", 4, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetText(tblRow, "header", 6, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetInt(tblRow, "useTemplate", 11, sqlite3_column_int64(stmt, 5));
	xvoTableSetText(tblRow, "content", 7, (str)sqlite3_column_text(stmt, 6), 0, FALSE);
	xvoTableSetInt(tblRow, "cacheSeconds", 12, sqlite3_column_int64(stmt, 7));
	xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 8));
	xvoTableSetInt(tblRow, "updateTime", 10, sqlite3_column_int64(stmt, 9));
}

void Request_View_Content_Page(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "content/page.html");
}

void Request_Content_Pages(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3_stmt* stmt = NULL;
	xvalue arrRows = xvoCreateArray();
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( sqlite3_prepare_v3(G_DB, "SELECT id,title,status,uris,header,useTemplate,content,cacheSeconds,createTime,updateTime FROM standalone_page WHERE status <> 'deleted' ORDER BY id DESC", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		Content_ReplyError(objResp, "query pages failed");
		xvoUnref(arrRows);
		return;
	}
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		StandaloneAdmin_RowToValue(stmt, tblRow);
		xvoArrayAppendValue(arrRows, tblRow, TRUE);
	}
	sqlite3_finalize(stmt);
	Content_ReplySuccess(objResp, arrRows);
}

void Request_Content_Page(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sID[32] = {0};
	int64 id;
	sqlite3_stmt* stmt = NULL;
	(void)objServer; (void)objHost; (void)objSession;
	xsReqQueryValue(objReq, "id", sID, sizeof(sID));
	id = xrtStrToI64(sID);
	if ( id <= 0 ) {
		Content_ReplyError(objResp, "invalid page id");
		return;
	}
	if ( sqlite3_prepare_v3(G_DB, "SELECT id,title,status,uris,header,useTemplate,content,cacheSeconds,createTime,updateTime FROM standalone_page WHERE id = ? AND status <> 'deleted'", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		Content_ReplyError(objResp, "query page failed");
		return;
	}
	sqlite3_bind_int64(stmt, 1, id);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		StandaloneAdmin_RowToValue(stmt, tblRow);
		sqlite3_finalize(stmt);
		Content_ReplySuccess(objResp, tblRow);
		return;
	}
	sqlite3_finalize(stmt);
	Content_ReplyError(objResp, "page not found");
}

typedef struct StandaloneContainsCtx {
	const char* sUri;
	bool bFound;
} StandaloneContainsCtx;

static bool StandaloneAdmin_ContainsUriProc(const char* sItem, void* pCtx)
{
	StandaloneContainsCtx* p = (StandaloneContainsCtx*)pCtx;
	if ( strcmp(sItem, p->sUri) == 0 ) {
		p->bFound = TRUE;
		return TRUE;
	}
	return FALSE;
}

static bool StandaloneAdmin_ContainsUri(const char* sUris, const char* sUri)
{
	StandaloneContainsCtx ctx = { sUri, FALSE };
	Standalone_ForEachUri(sUris, StandaloneAdmin_ContainsUriProc, &ctx);
	return ctx.bFound;
}

static bool StandaloneAdmin_CheckPageUriConflict(const char* sUri, int64 iPageId, str* psError)
{
	sqlite3_stmt* stmt = NULL;
	bool bOK = TRUE;
	if ( sqlite3_prepare_v3(G_DB, "SELECT id,uris FROM standalone_page WHERE status <> 'deleted' AND id <> ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		if ( psError ) *psError = xrtCopyStr("check page uri failed", 0);
		return FALSE;
	}
	sqlite3_bind_int64(stmt, 1, iPageId);
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		int64 iOtherId = sqlite3_column_int64(stmt, 0);
		const char* sOtherUris = (const char*)sqlite3_column_text(stmt, 1);
		if ( StandaloneAdmin_ContainsUri(sOtherUris, sUri) ) {
			if ( psError ) *psError = xrtFormat("uri already exists: %s (page %lld)", sUri, iOtherId);
			bOK = FALSE;
			break;
		}
	}
	sqlite3_finalize(stmt);
	return bOK;
}

typedef struct StandaloneDeleteOldCtx {
	const char* sNewUris;
} StandaloneDeleteOldCtx;

static bool StandaloneAdmin_DeleteOldUriProc(const char* sUri, void* pCtx)
{
	StandaloneDeleteOldCtx* pDel = (StandaloneDeleteOldCtx*)pCtx;
	sqlite3_stmt* stmt = NULL;
	if ( StandaloneAdmin_ContainsUri(pDel->sNewUris, sUri) ) return FALSE;
	if ( sqlite3_prepare_v3(G_DB, "DELETE FROM uris WHERE namespace = 'page' AND uri = ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sUri, -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	return FALSE;
}

static bool StandaloneAdmin_CheckUriProc(const char* sUri, void* pCtx)
{
	StandaloneCheckCtx* pCheck = (StandaloneCheckCtx*)pCtx;
	if ( !Standalone_CheckUriConflict(sUri, pCheck->id, &pCheck->error) ) return TRUE;
	if ( !StandaloneAdmin_CheckPageUriConflict(sUri, pCheck->id, &pCheck->error) ) return TRUE;
	return FALSE;
}

static str StandaloneAdmin_LoadOldUris(int64 id)
{
	sqlite3_stmt* stmt = NULL;
	str sUris = NULL;
	if ( id <= 0 ) return NULL;
	if ( sqlite3_prepare_v3(G_DB, "SELECT uris FROM standalone_page WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, id);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			sUris = Standalone_CopyColumn(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}
	return sUris;
}

void Request_Content_Page_Save(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = NULL;
	int64 id;
	str title;
	str status;
	str uris;
	str header;
	str content;
	str normalizedHeader = NULL;
	str oldUris = NULL;
	str error = NULL;
	int64 useTemplate;
	int64 cacheSeconds;
	sqlite3_stmt* stmt = NULL;
	xtime now = xrtNow();
	StandaloneCheckCtx checkCtx;
	(void)objServer; (void)objHost; (void)objSession;

	if ( xsReqMethodID(objReq) != XHTTPD_METHOD_POST ) {
		Content_ReplyError(objResp, "method not allowed");
		return;
	}
	tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( tblBody == NULL || xvoType(tblBody) != XVO_DT_TABLE ) {
		if ( tblBody ) xvoUnref(tblBody);
		Content_ReplyError(objResp, "invalid json body");
		return;
	}
	id = xvoTableGetInt(tblBody, "id", 2);
	title = xvoTableGetText(tblBody, "title", 5);
	status = xvoTableGetText(tblBody, "status", 6);
	uris = xvoTableGetText(tblBody, "uris", 4);
	header = xvoTableGetText(tblBody, "header", 6);
	content = xvoTableGetText(tblBody, "content", 7);
	useTemplate = xvoTableGetInt(tblBody, "useTemplate", 11);
	cacheSeconds = xvoTableGetInt(tblBody, "cacheSeconds", 12);
	if ( title == NULL || title[0] == '\0' ) {
		xvoUnref(tblBody);
		Content_ReplyError(objResp, "title required");
		return;
	}
	if ( status == NULL || (strcmp((const char*)status, "enabled") != 0 && strcmp((const char*)status, "draft") != 0) ) {
		status = "draft";
	}
	normalizedHeader = Standalone_NormalizeHeader(header, &error);
	if ( normalizedHeader == NULL ) {
		xvoUnref(tblBody);
		Content_ReplyError(objResp, Standalone_CStr(error));
		if ( error ) xrtFree(error);
		return;
	}
	checkCtx.id = id;
	checkCtx.error = NULL;
	if ( Standalone_ForEachUri((const char*)Standalone_CStr(uris), StandaloneAdmin_CheckUriProc, &checkCtx) ) {
		xvoUnref(tblBody);
		xrtFree(normalizedHeader);
		Content_ReplyError(objResp, Standalone_CStr(checkCtx.error));
		if ( checkCtx.error ) xrtFree(checkCtx.error);
		return;
	}
	oldUris = StandaloneAdmin_LoadOldUris(id);
	if ( id > 0 ) {
		if ( sqlite3_prepare_v3(G_DB, "UPDATE standalone_page SET title=?,status=?,uris=?,header=?,useTemplate=?,content=?,cacheSeconds=?,updateTime=? WHERE id=?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			xvoUnref(tblBody); xrtFree(normalizedHeader); if ( oldUris ) xrtFree(oldUris);
			Content_ReplyError(objResp, "prepare update failed");
			return;
		}
		sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, status, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, Standalone_CStr(uris), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, normalizedHeader, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 5, useTemplate ? 1 : 0);
		sqlite3_bind_text(stmt, 6, Standalone_CStr(content), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 7, cacheSeconds > 0 ? cacheSeconds : 0);
		sqlite3_bind_int64(stmt, 8, now);
		sqlite3_bind_int64(stmt, 9, id);
	} else {
		if ( sqlite3_prepare_v3(G_DB, "INSERT INTO standalone_page (title,status,uris,header,useTemplate,content,cacheSeconds,createTime,updateTime) VALUES (?,?,?,?,?,?,?,?,?)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			xvoUnref(tblBody); xrtFree(normalizedHeader);
			Content_ReplyError(objResp, "prepare insert failed");
			return;
		}
		sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, status, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, Standalone_CStr(uris), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, normalizedHeader, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 5, useTemplate ? 1 : 0);
		sqlite3_bind_text(stmt, 6, Standalone_CStr(content), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 7, cacheSeconds > 0 ? cacheSeconds : 0);
		sqlite3_bind_int64(stmt, 8, now);
		sqlite3_bind_int64(stmt, 9, now);
	}
	if ( sqlite3_step(stmt) != SQLITE_DONE ) {
		sqlite3_finalize(stmt);
		xvoUnref(tblBody); xrtFree(normalizedHeader); if ( oldUris ) xrtFree(oldUris);
		Content_ReplyError(objResp, "save page failed");
		return;
	}
	if ( id <= 0 ) id = sqlite3_last_insert_rowid(G_DB);
	sqlite3_finalize(stmt);
	if ( oldUris ) {
		StandaloneDeleteOldCtx delCtx = { (const char*)Standalone_CStr(uris) };
		Standalone_ForEachUri((const char*)oldUris, StandaloneAdmin_DeleteOldUriProc, &delCtx);
		xrtFree(oldUris);
	}
	Standalone_SyncUriRows(uris);
	StandalonePage_LoadAll();
	Auth_SyncURIS();
	{
		xvalue tblRet = xvoCreateTable();
		xvoTableSetInt(tblRet, "id", 2, id);
		Content_ReplySuccess(objResp, tblRet);
	}
	xvoUnref(tblBody);
	xrtFree(normalizedHeader);
}

void Request_Content_Page_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = NULL;
	int64 id;
	str oldUris = NULL;
	sqlite3_stmt* stmt = NULL;
	(void)objServer; (void)objHost; (void)objSession;
	if ( xsReqMethodID(objReq) != XHTTPD_METHOD_POST ) {
		Content_ReplyError(objResp, "method not allowed");
		return;
	}
	tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( tblBody == NULL || xvoType(tblBody) != XVO_DT_TABLE ) {
		if ( tblBody ) xvoUnref(tblBody);
		Content_ReplyError(objResp, "invalid json body");
		return;
	}
	id = xvoTableGetInt(tblBody, "id", 2);
	oldUris = StandaloneAdmin_LoadOldUris(id);
	if ( sqlite3_prepare_v3(G_DB, "UPDATE standalone_page SET status='deleted', updateTime=? WHERE id=?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		xvoUnref(tblBody); if ( oldUris ) xrtFree(oldUris);
		Content_ReplyError(objResp, "prepare delete failed");
		return;
	}
	sqlite3_bind_int64(stmt, 1, xrtNow());
	sqlite3_bind_int64(stmt, 2, id);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if ( oldUris ) {
		StandaloneDeleteOldCtx delCtx = { "" };
		Standalone_ForEachUri((const char*)oldUris, StandaloneAdmin_DeleteOldUriProc, &delCtx);
		xrtFree(oldUris);
	}
	StandalonePage_LoadAll();
	Auth_SyncURIS();
	Content_ReplySuccess(objResp, NULL);
	xvoUnref(tblBody);
}

#endif
