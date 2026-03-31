#include "plugin.h"

PluginContext* ctx;
xvalue g_tblSettings = NULL;
xdict g_tblInstances = NULL;
int g_iMenuId = 0;

typedef struct {
	str sInstanceId;
	str sName;
	str sDescription;
	int iMaxMessages;
	bool bRequireLogin;
	bool bRequireApproval;
	str sTheme;
	sqlite3_stmt* stmtAdd;
	sqlite3_stmt* stmtList;
} GuestbookInstance;

void Plugin_SetGlobalData(int idx, void* ptr)
{
	if ( idx == 1 ) {
		ctx = (PluginContext*)ptr;
	} else if ( idx == 2 ) {
		g_tblSettings = (xvalue)ptr;
	}
}

void Plugin_guestbook_View(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);

GuestbookInstance* Guestbook_CreateInstance(str sInstanceId, str sName, str sDescription)
{
	GuestbookInstance* pInst = xrtMalloc(sizeof(GuestbookInstance));
	memset(pInst, 0, sizeof(GuestbookInstance));

	pInst->sInstanceId = xrtCopyStr(sInstanceId, 0);
	pInst->sName = xrtCopyStr(sName ? sName : sInstanceId, 0);
	pInst->sDescription = xrtCopyStr(sDescription ? sDescription : "", 0);
	pInst->iMaxMessages = 100;
	pInst->bRequireLogin = FALSE;
	pInst->bRequireApproval = FALSE;
	pInst->sTheme = xrtCopyStr("default", 0);

	return pInst;
}

void Guestbook_DestroyInstance(GuestbookInstance* pInst)
{
	if ( !pInst ) {
		return;
	}

	if ( pInst->stmtAdd ) {
		sqlite3_finalize(pInst->stmtAdd);
	}
	if ( pInst->stmtList ) {
		sqlite3_finalize(pInst->stmtList);
	}

	xrtFree(pInst->sInstanceId);
	xrtFree(pInst->sName);
	xrtFree(pInst->sDescription);
	xrtFree(pInst->sTheme);
	xrtFree(pInst);
}

bool Guestbook_InitTable(GuestbookInstance* pInst)
{
	str sTableName = xrtFormat("guestbook_%s", pInst->sInstanceId);
	str sSQL = xrtFormat(
		"CREATE TABLE IF NOT EXISTS %s ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"memberId INTEGER DEFAULT 0,"
		"username TEXT NOT NULL,"
		"content TEXT NOT NULL,"
		"status INTEGER DEFAULT 0,"
		"reply TEXT,"
		"replyTime INTEGER,"
		"createTime INTEGER NOT NULL"
		")",
		sTableName
	);

	bool bResult = ctx->CreateTable(sTableName, sSQL);
	if ( !bResult ) {
		xrtFree(sTableName);
		xrtFree(sSQL);
		return FALSE;
	}

	xrtFree(sTableName);
	xrtFree(sSQL);

	str sAddSQL = xrtFormat(
		"INSERT INTO guestbook_%s (memberId, username, content, status, createTime) VALUES (?, ?, ?, 0, ?)",
		pInst->sInstanceId
	);
	if ( sqlite3_prepare_v3(ctx->pDB, sAddSQL, -1, 0, &pInst->stmtAdd, NULL) != SQLITE_OK ) {
		xrtFree(sAddSQL);
		return FALSE;
	}
	xrtFree(sAddSQL);

	str sListSQL = xrtFormat(
		"SELECT id, memberId, username, content, status, reply, replyTime, createTime FROM guestbook_%s ORDER BY id DESC LIMIT ?",
		pInst->sInstanceId
	);
	if ( sqlite3_prepare_v3(ctx->pDB, sListSQL, -1, 0, &pInst->stmtList, NULL) != SQLITE_OK ) {
		xrtFree(sListSQL);
		return FALSE;
	}
	xrtFree(sListSQL);

	return TRUE;
}

void API_Guestbook_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue objForm = ctx->JsonParse((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !objForm ) {
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"invalid request\"}", 0);
		return;
	}

	str sInstanceId = xvoTableGetText(objForm, "instanceId", 11);
	if ( !sInstanceId ) {
		xvoUnref(objForm);
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"missing instanceId\"}", 0);
		return;
	}

	GuestbookInstance** ppInst = xrtDictGet(g_tblInstances, sInstanceId, strlen(sInstanceId));
	if ( !ppInst || !(*ppInst) ) {
		xvoUnref(objForm);
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"guestbook not found\"}", 0);
		return;
	}

	GuestbookInstance* pInst = *ppInst;
	xvalue objArrList = xvoCreateArray();

	sqlite3_bind_int(pInst->stmtList, 1, 100);
	while ( sqlite3_step(pInst->stmtList) == SQLITE_ROW ) {
		xvalue objItem = xvoCreateTable();
		xvoTableSetInt(objItem, "id", 2, sqlite3_column_int(pInst->stmtList, 0));
		xvoTableSetInt(objItem, "memberId", 8, sqlite3_column_int(pInst->stmtList, 1));
		xvoTableSetText(objItem, "username", 8, (str)sqlite3_column_text(pInst->stmtList, 2), 0, FALSE);
		xvoTableSetText(objItem, "content", 7, (str)sqlite3_column_text(pInst->stmtList, 3), 0, FALSE);
		xvoTableSetInt(objItem, "status", 6, sqlite3_column_int(pInst->stmtList, 4));
		xvoTableSetText(objItem, "reply", 5, (str)sqlite3_column_text(pInst->stmtList, 5), 0, FALSE);
		xvoTableSetInt(objItem, "replyTime", 8, sqlite3_column_int64(pInst->stmtList, 6));
		xvoTableSetInt(objItem, "createTime", 10, sqlite3_column_int64(pInst->stmtList, 7));
		xvoArrayAppendValue(objArrList, objItem, TRUE);
	}
	sqlite3_reset(pInst->stmtList);
	sqlite3_clear_bindings(pInst->stmtList);

	xvalue objRet = xvoCreateTable();
	xvoTableSetBool(objRet, "result", 6, TRUE);
	xvoTableSetValue(objRet, "data", 4, objArrList, TRUE);

	size_t iLen = 0;
	str sJson = ctx->JsonStringify(objRet, &iLen);
	ctx->SendJson(objResp, 200, sJson, iLen);
	ctx->Free(sJson);
	xvoUnref(objRet);
	xvoUnref(objForm);
}

void API_Guestbook_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue objForm = ctx->JsonParse((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !objForm ) {
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"invalid request\"}", 0);
		return;
	}

	str sInstanceId = xvoTableGetText(objForm, "instanceId", 11);
	str sUsername = xvoTableGetText(objForm, "username", 8);
	str sContent = xvoTableGetText(objForm, "content", 7);
	if ( !sInstanceId || !sUsername || !sContent ) {
		xvoUnref(objForm);
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"missing required fields\"}", 0);
		return;
	}

	GuestbookInstance** ppInst = xrtDictGet(g_tblInstances, sInstanceId, strlen(sInstanceId));
	if ( !ppInst || !(*ppInst) ) {
		xvoUnref(objForm);
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"guestbook not found\"}", 0);
		return;
	}

	GuestbookInstance* pInst = *ppInst;
	if ( pInst->bRequireLogin ) {
		xvoUnref(objForm);
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"login required\"}", 0);
		return;
	}

	sqlite3_bind_int(pInst->stmtAdd, 1, 0);
	sqlite3_bind_text(pInst->stmtAdd, 2, sUsername, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(pInst->stmtAdd, 3, sContent, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(pInst->stmtAdd, 4, ctx->TimeNow());
	sqlite3_step(pInst->stmtAdd);
	sqlite3_reset(pInst->stmtAdd);
	sqlite3_clear_bindings(pInst->stmtAdd);

	xvoUnref(objForm);
	ctx->SendJson(objResp, 200, "{\"result\":true,\"message\":\"ok\"}", 0);
}

void API_Guestbook_CreateInstance(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue objForm = ctx->JsonParse((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !objForm ) {
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"invalid request\"}", 0);
		return;
	}

	str sInstanceId = xvoTableGetText(objForm, "instanceId", 11);
	str sName = xvoTableGetText(objForm, "name", 4);
	str sDescription = xvoTableGetText(objForm, "description", 11);
	if ( !sInstanceId ) {
		xvoUnref(objForm);
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"missing instanceId\"}", 0);
		return;
	}

	if ( xrtDictGet(g_tblInstances, sInstanceId, strlen(sInstanceId)) ) {
		xvoUnref(objForm);
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"instance already exists\"}", 0);
		return;
	}

	GuestbookInstance* pInst = Guestbook_CreateInstance(sInstanceId, sName, sDescription);
	if ( !Guestbook_InitTable(pInst) ) {
		Guestbook_DestroyInstance(pInst);
		xvoUnref(objForm);
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"init failed\"}", 0);
		return;
	}

	xrtDictSet(g_tblInstances, sInstanceId, strlen(sInstanceId), NULL);
	GuestbookInstance** ppInst = xrtDictGet(g_tblInstances, sInstanceId, strlen(sInstanceId));
	*ppInst = pInst;

	xvoUnref(objForm);
	ctx->SendJson(objResp, 200, "{\"result\":true,\"message\":\"created\"}", 0);
}

void API_Guestbook_DeleteInstance(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue objForm = ctx->JsonParse((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !objForm ) {
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"invalid request\"}", 0);
		return;
	}

	str sInstanceId = xvoTableGetText(objForm, "instanceId", 11);
	if ( !sInstanceId ) {
		xvoUnref(objForm);
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"missing instanceId\"}", 0);
		return;
	}

	GuestbookInstance** ppInst = xrtDictGet(g_tblInstances, sInstanceId, strlen(sInstanceId));
	if ( !ppInst || !(*ppInst) ) {
		xvoUnref(objForm);
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"guestbook not found\"}", 0);
		return;
	}

	GuestbookInstance* pInst = *ppInst;
	str sTableName = xrtFormat("guestbook_%s", pInst->sInstanceId);
	ctx->DropTable(sTableName);
	xrtFree(sTableName);

	Guestbook_DestroyInstance(pInst);
	xrtDictRemove(g_tblInstances, sInstanceId, strlen(sInstanceId));

	xvoUnref(objForm);
	ctx->SendJson(objResp, 200, "{\"result\":true,\"message\":\"deleted\"}", 0);
}

void Plugin_guestbook_Init()
{
	ctx->Log(LOG_INFO, "[Guestbook] Initializing...");

	g_tblInstances = xrtDictCreate(sizeof(ptr), 0);

	ctx->AddRoute("/admin/api/guestbook/list", API_Guestbook_List, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/admin/api/guestbook/add", API_Guestbook_Add, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/admin/api/guestbook/create_instance", API_Guestbook_CreateInstance, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/admin/api/guestbook/delete_instance", API_Guestbook_DeleteInstance, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/admin/view/guestbook", Plugin_guestbook_View, TRUE, TRUE, 0, 0);

	g_iMenuId = ctx->AddMenu(0, "Guestbook", "layui-icon layui-icon-dialogue", 1, "_component", "/admin/view/guestbook", 100, TRUE);

	ctx->Log(LOG_INFO, "[Guestbook] Initialized!");
}

void Plugin_guestbook_View(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	LOAD_SEND_PAGE(objResp, "index.html");
}

void Plugin_guestbook_Unit()
{
	ctx->RemoveRoute("/admin/api/guestbook/list");
	ctx->RemoveRoute("/admin/api/guestbook/add");
	ctx->RemoveRoute("/admin/api/guestbook/create_instance");
	ctx->RemoveRoute("/admin/api/guestbook/delete_instance");
	ctx->RemoveRoute("/admin/view/guestbook");

	if ( g_tblInstances ) {
		xrtDictDestroy(g_tblInstances);
		g_tblInstances = NULL;
	}

	ctx->Log(LOG_INFO, "[Guestbook] Unloaded!");
}
