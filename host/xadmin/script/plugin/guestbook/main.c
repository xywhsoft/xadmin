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
	if ( idx == 1 ) ctx = (PluginContext*)ptr;
	else if ( idx == 2 ) g_tblSettings = (xvalue)ptr;
}

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
	if ( !pInst ) return;

	if ( pInst->stmtAdd ) sqlite3_finalize(pInst->stmtAdd);
	if ( pInst->stmtList ) sqlite3_finalize(pInst->stmtList);

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

	xrtFree(sTableName);
	xrtFree(sSQL);

	if ( !bResult ) return FALSE;

	str sAddSQL = xrtFormat(
		"INSERT INTO guestbook_%s (memberId, username, content, status, createTime) VALUES (?, ?, ?, 0, ?)",
		pInst->sInstanceId
	);
	sqlite3_prepare_v3(ctx->pDB->objDB, sAddSQL, -1, 0, &pInst->stmtAdd, NULL);
	xrtFree(sAddSQL);

	str sListSQL = xrtFormat(
		"SELECT id, memberId, username, content, status, reply, replyTime, createTime FROM guestbook_%s ORDER BY id DESC LIMIT ?",
		pInst->sInstanceId
	);
	sqlite3_prepare_v3(ctx->pDB->objDB, sListSQL, -1, 0, &pInst->stmtList, NULL);
	xrtFree(sListSQL);

	return TRUE;
}

void API_Guestbook_List(void* s, void* h, struct mg_connection* c, struct mg_http_message* hm)
{
	xvalue form = ctx->JsonParse(hm->body.buf, hm->body.len);
	if ( !form ) {
		ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"参数错误\"}", 0);
		return;
	}

	str sInstanceId = xvoTableGetText(form, "instanceId", 11);
	xvoUnref(form);

	if ( !sInstanceId ) {
		ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"缺少 instanceId\"}", 0);
		return;
	}

	GuestbookInstance** ppInst = xrtDictGet(g_tblInstances, sInstanceId, strlen(sInstanceId));
	if ( !ppInst || !(*ppInst) ) {
		ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"留言板不存在\"}", 0);
		return;
	}

	GuestbookInstance* pInst = *ppInst;

	xvalue arrList = xvoCreateArray();
	sqlite3_bind_int(pInst->stmtList, 1, 100);

	while ( sqlite3_step(pInst->stmtList) == SQLITE_ROW ) {
		xvalue item = xvoCreateTable();
		xvoTableSetInt(item, "id", 2, sqlite3_column_int(pInst->stmtList, 0));
		xvoTableSetInt(item, "memberId", 8, sqlite3_column_int(pInst->stmtList, 1));
		xvoTableSetText(item, "username", 8, (str)sqlite3_column_text(pInst->stmtList, 2), 0, FALSE);
		xvoTableSetText(item, "content", 7, (str)sqlite3_column_text(pInst->stmtList, 3), 0, FALSE);
		xvoTableSetInt(item, "status", 6, sqlite3_column_int(pInst->stmtList, 4));
		xvoTableSetText(item, "reply", 5, (str)sqlite3_column_text(pInst->stmtList, 5), 0, FALSE);
		xvoTableSetInt(item, "replyTime", 8, sqlite3_column_int64(pInst->stmtList, 6));
		xvoTableSetInt(item, "createTime", 10, sqlite3_column_int64(pInst->stmtList, 7));
		xvoArrayAppendValue(arrList, item, TRUE);
	}
	sqlite3_reset(pInst->stmtList);

	xvalue ret = xvoCreateTable();
	xvoTableSetBool(ret, "result", 6, TRUE);
	xvoTableSetValue(ret, "data", 4, arrList, TRUE);

	size_t iLen = 0;
	str sJson = ctx->JsonStringify(ret, &iLen);
	ctx->SendJson(c, 200, sJson, iLen);
	ctx->Free(sJson);
	xvoUnref(ret);
}

void API_Guestbook_Add(void* s, void* h, struct mg_connection* c, struct mg_http_message* hm)
{
	xvalue form = ctx->JsonParse(hm->body.buf, hm->body.len);
	if ( !form ) {
		ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"参数错误\"}", 0);
		return;
	}

	str sInstanceId = xvoTableGetText(form, "instanceId", 11);
	str sUsername = xvoTableGetText(form, "username", 8);
	str sContent = xvoTableGetText(form, "content", 7);

	if ( !sInstanceId || !sUsername || !sContent ) {
		xvoUnref(form);
		ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"缺少必需参数\"}", 0);
		return;
	}

	GuestbookInstance** ppInst = xrtDictGet(g_tblInstances, sInstanceId, strlen(sInstanceId));
	if ( !ppInst || !(*ppInst) ) {
		xvoUnref(form);
		ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"留言板不存在\"}", 0);
		return;
	}

	GuestbookInstance* pInst = *ppInst;

	if ( pInst->bRequireLogin ) {
		xvoUnref(form);
		ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"该留言板需要登录后留言\"}", 0);
		return;
	}

	sqlite3_bind_int(pInst->stmtAdd, 1, 0);
	sqlite3_bind_text(pInst->stmtAdd, 2, sUsername, -1, NULL);
	sqlite3_bind_text(pInst->stmtAdd, 3, sContent, -1, NULL);
	sqlite3_bind_int64(pInst->stmtAdd, 4, ctx->TimeNow());
	sqlite3_step(pInst->stmtAdd);
	sqlite3_reset(pInst->stmtAdd);

	xvoUnref(form);
	ctx->SendJson(c, 200, "{\"result\":true,\"message\":\"留言成功\"}", 0);
}

void API_Guestbook_CreateInstance(void* s, void* h, struct mg_connection* c, struct mg_http_message* hm)
{
	xvalue form = ctx->JsonParse(hm->body.buf, hm->body.len);
	if ( !form ) {
		ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"参数错误\"}", 0);
		return;
	}

	str sInstanceId = xvoTableGetText(form, "instanceId", 11);
	str sName = xvoTableGetText(form, "name", 4);
	str sDescription = xvoTableGetText(form, "description", 11);

	if ( !sInstanceId ) {
		xvoUnref(form);
		ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"缺少 instanceId\"}", 0);
		return;
	}

	if ( xrtDictGet(g_tblInstances, sInstanceId, strlen(sInstanceId)) ) {
		xvoUnref(form);
		ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"实例 ID 已存在\"}", 0);
		return;
	}

	GuestbookInstance* pInst = Guestbook_CreateInstance(sInstanceId, sName, sDescription);

	if ( !Guestbook_InitTable(pInst) ) {
		Guestbook_DestroyInstance(pInst);
		xvoUnref(form);
		ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"初始化失败\"}", 0);
		return;
	}

	xrtDictSet(g_tblInstances, sInstanceId, strlen(sInstanceId), NULL);
	GuestbookInstance** ppInst = xrtDictGet(g_tblInstances, sInstanceId, strlen(sInstanceId));
	*ppInst = pInst;

	xvoUnref(form);
	ctx->SendJson(c, 200, "{\"result\":true,\"message\":\"创建成功\"}", 0);
}

void API_Guestbook_DeleteInstance(void* s, void* h, struct mg_connection* c, struct mg_http_message* hm)
{
	xvalue form = ctx->JsonParse(hm->body.buf, hm->body.len);
	if ( !form ) {
		ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"参数错误\"}", 0);
		return;
	}

	str sInstanceId = xvoTableGetText(form, "instanceId", 11);
	xvoUnref(form);

	if ( !sInstanceId ) {
		ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"缺少 instanceId\"}", 0);
		return;
	}

	GuestbookInstance** ppInst = xrtDictGet(g_tblInstances, sInstanceId, strlen(sInstanceId));
	if ( !ppInst || !(*ppInst) ) {
		ctx->SendJson(c, 200, "{\"result\":false,\"message\":\"留言板不存在\"}", 0);
		return;
	}

	GuestbookInstance* pInst = *ppInst;

	str sTableName = xrtFormat("guestbook_%s", pInst->sInstanceId);
	ctx->DropTable(sTableName);
	xrtFree(sTableName);

	Guestbook_DestroyInstance(pInst);

	xrtDictRemove(g_tblInstances, sInstanceId, strlen(sInstanceId));

	ctx->SendJson(c, 200, "{\"result\":true,\"message\":\"删除成功\"}", 0);
}

void Plugin_guestbook_Init()
{
	ctx->Log(LOG_INFO, "[Guestbook] Initializing...");

	g_tblInstances = xrtDictCreate(sizeof(ptr));

	ctx->AddRoute("/admin/api/guestbook/list", API_Guestbook_List, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/admin/api/guestbook/add", API_Guestbook_Add, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/admin/api/guestbook/create_instance", API_Guestbook_CreateInstance, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/admin/api/guestbook/delete_instance", API_Guestbook_DeleteInstance, TRUE, TRUE, 0, 0);

	g_iMenuId = ctx->AddMenu(0, "留言板", "layui-icon layui-icon-dialogue",
	                         1, "_component", "/admin/view/guestbook", 100, TRUE);

	ctx->Log(LOG_INFO, "[Guestbook] Initialized!");
}

void Plugin_guestbook_Unit()
{
	if ( g_iMenuId > 0 ) {
		ctx->RemoveMenu(g_iMenuId);
	}

	ctx->RemoveRoute("/admin/api/guestbook/list");
	ctx->RemoveRoute("/admin/api/guestbook/add");
	ctx->RemoveRoute("/admin/api/guestbook/create_instance");
	ctx->RemoveRoute("/admin/api/guestbook/delete_instance");

	if ( g_tblInstances ) {
		xrtDictDestroy(g_tblInstances);
		g_tblInstances = NULL;
	}

	ctx->Log(LOG_INFO, "[Guestbook] Unloaded!");
}
