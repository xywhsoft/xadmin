
// ============================================
// 模型: 文章 (article)
// 自动生成代码 - 请勿手动修改
// 生成时间: 2026-01-09 11:37:09
// TCC独立状态机编译
// ============================================

// 引入 xserver 基础库
#include <xsbase.h>

// 全局变量（通过 Model_SetGlobalData 传入）
XDO_Connect G_DB;

// 模型管理器特有函数声明
// 注意：Model_AddRoute 实际返回 RouteInfo*，但模型代码不需要使用返回值
extern void* Model_AddRoute(str, void*, bool, bool, int, int);
extern void Model_RemoveRoute(str);

// HTTP 响应常量
#define HTTP_CT_JSON "Content-Type: application/json\r\n"

// 接收主系统传递的全局数据
void Model_SetGlobalData(int idx, void* ptr)
{
	if ( idx == 1 ) {
		G_DB = (XDO_Connect)ptr;
	}
}

// ==================== 模型代码 ====================

// 预编译语句
sqlite3_stmt* stmt_article_all;
sqlite3_stmt* stmt_article_get;
sqlite3_stmt* stmt_article_add;
sqlite3_stmt* stmt_article_put;
sqlite3_stmt* stmt_article_del;
sqlite3_stmt* stmt_article_count;

// 初始化预编译语句
void Model_article_InitStmt()
{
	sqlite3* db = G_DB->objDB;
	
	// 分页获取列表
	sqlite3_prepare_v3(db,
		"SELECT id, title, createTime, updateTime FROM model_cms_article "
		"WHERE isDelete = 0 ORDER BY id DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt_article_all, NULL);
	
	// 根据ID获取单条记录
	sqlite3_prepare_v3(db,
		"SELECT id, title, createTime, updateTime FROM model_cms_article WHERE id = ? AND isDelete = 0",
		-1, 0, &stmt_article_get, NULL);
	
	// 添加记录
	sqlite3_prepare_v3(db,
		"INSERT INTO model_cms_article (title, createTime, updateTime, isDelete) VALUES (?, ?, ?, 0)",
		-1, 0, &stmt_article_add, NULL);
	
	// 更新记录
	sqlite3_prepare_v3(db,
		"UPDATE model_cms_article SET title = ?, updateTime = ? WHERE id = ?",
		-1, 0, &stmt_article_put, NULL);
	
	// 删除记录（软删除）
	sqlite3_prepare_v3(db,
		"UPDATE model_cms_article SET isDelete = 1, updateTime = ? WHERE id = ?",
		-1, 0, &stmt_article_del, NULL);
	
	// 统计总数
	sqlite3_prepare_v3(db,
		"SELECT COUNT(*) FROM model_cms_article WHERE isDelete = 0",
		-1, 0, &stmt_article_count, NULL);
}

// 销毁预编译语句
void Model_article_FreeStmt()
{
	if ( stmt_article_all ) sqlite3_finalize(stmt_article_all);
	if ( stmt_article_get ) sqlite3_finalize(stmt_article_get);
	if ( stmt_article_add ) sqlite3_finalize(stmt_article_add);
	if ( stmt_article_put ) sqlite3_finalize(stmt_article_put);
	if ( stmt_article_del ) sqlite3_finalize(stmt_article_del);
	if ( stmt_article_count ) sqlite3_finalize(stmt_article_count);
}




// ==================== 前台 API ====================

// 获取列表
void Api_article_List(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_GET ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	char sParam[64];
	mg_http_get_var(&hm->query, "page", sParam, sizeof(sParam));
	int64 iPage = xrtStrToI64(sParam);
	if ( iPage <= 0 ) iPage = 1;
	mg_http_get_var(&hm->query, "limit", sParam, sizeof(sParam));
	int64 iLimit = xrtStrToI64(sParam);
	if ( iLimit <= 0 ) iLimit = 20;
	if ( iLimit > 100 ) iLimit = 100;
	int64 iOffset = (iPage - 1) * iLimit;
	
	// 获取总数
	int64 iCount = 0;
	if ( sqlite3_step(stmt_article_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_article_count, 0);
	}
	sqlite3_reset(stmt_article_count);
	
	// 获取数据
	xvalue arrData = xvoCreateArray();
	sqlite3_bind_int64(stmt_article_all, 1, iLimit);
	sqlite3_bind_int64(stmt_article_all, 2, iOffset);
	while ( sqlite3_step(stmt_article_all) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_article_all, iCol++));
		xvoTableSetText(tblRow, "title", 5, (str)sqlite3_column_text(stmt_article_all, iCol++), 0, FALSE);

		xtime iTime = sqlite3_column_int64(stmt_article_all, iCol++);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_article_all, iCol++);
		xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_reset(stmt_article_all);
	
	// 构建响应
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}

// 获取详情
void Api_article_Get(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sID[24];
	mg_http_get_var(&hm->query, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xvalue tblData = NULL;
	sqlite3_bind_int64(stmt_article_get, 1, iID);
	if ( sqlite3_step(stmt_article_get) == SQLITE_ROW ) {
		tblData = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblData, "id", 2, sqlite3_column_int64(stmt_article_get, iCol++));
		xvoTableSetText(tblData, "title", 5, (str)sqlite3_column_text(stmt_article_get, iCol++), 0, FALSE);

		xtime iTime = sqlite3_column_int64(stmt_article_get, iCol++);
		xvoTableSetText(tblData, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_article_get, iCol++);
		xvoTableSetText(tblData, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
	}
	sqlite3_reset(stmt_article_get);
	
	if ( !tblData ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Not found\"}", 0);
		return;
	}
	
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}




/* SUBMIT DISABLED

// ==================== 前台投稿 ====================

// 投稿内容
void Api_article_Submit(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		if ( tblForm ) xvoUnref(tblForm);
		return;
	}
	
	xtime now = xrtNow();
	int iIdx = 1;
	{ str sVal = xvoTableGetText(tblForm, "title", 5); sqlite3_bind_text(stmt_article_add, iIdx++, sVal ? sVal : "", -1, NULL); }

	sqlite3_bind_int64(stmt_article_add, iIdx++, now);
	sqlite3_bind_int64(stmt_article_add, iIdx++, now);
	sqlite3_step(stmt_article_add);
	int64 newId = sqlite3_last_insert_rowid(G_DB->objDB);
	sqlite3_reset(stmt_article_add);
	xvoUnref(tblForm);
	
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"id\": %lld}}", newId);
}
*/




// ==================== 后台管理 API ====================

// 后台列表
void Admin_article_List(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_GET ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	char sParam[64];
	mg_http_get_var(&hm->query, "page", sParam, sizeof(sParam));
	int64 iPage = xrtStrToI64(sParam);
	if ( iPage <= 0 ) iPage = 1;
	mg_http_get_var(&hm->query, "limit", sParam, sizeof(sParam));
	int64 iLimit = xrtStrToI64(sParam);
	if ( iLimit <= 0 ) iLimit = 20;
	int64 iOffset = (iPage - 1) * iLimit;
	
	// 获取总数
	int64 iCount = 0;
	if ( sqlite3_step(stmt_article_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_article_count, 0);
	}
	sqlite3_reset(stmt_article_count);
	
	// 获取数据
	xvalue arrData = xvoCreateArray();
	sqlite3_bind_int64(stmt_article_all, 1, iLimit);
	sqlite3_bind_int64(stmt_article_all, 2, iOffset);
	while ( sqlite3_step(stmt_article_all) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_article_all, iCol++));
		xvoTableSetText(tblRow, "title", 5, (str)sqlite3_column_text(stmt_article_all, iCol++), 0, FALSE);

		xtime iTime = sqlite3_column_int64(stmt_article_all, iCol++);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_article_all, iCol++);
		xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_reset(stmt_article_all);
	
	// layui table 格式响应
	xvalue tblRet = xvoCreateTable();
	xvoTableSetInt(tblRet, "code", 4, 0);
	xvoTableSetText(tblRet, "msg", 3, "", 0, FALSE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}

// 后台获取单条
void Admin_article_Get(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sID[24];
	mg_http_get_var(&hm->query, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xvalue tblData = NULL;
	sqlite3_bind_int64(stmt_article_get, 1, iID);
	if ( sqlite3_step(stmt_article_get) == SQLITE_ROW ) {
		tblData = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblData, "id", 2, sqlite3_column_int64(stmt_article_get, iCol++));
		xvoTableSetText(tblData, "title", 5, (str)sqlite3_column_text(stmt_article_get, iCol++), 0, FALSE);

		xtime iTime = sqlite3_column_int64(stmt_article_get, iCol++);
		xvoTableSetText(tblData, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_article_get, iCol++);
		xvoTableSetText(tblData, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
	}
	sqlite3_reset(stmt_article_get);
	
	if ( !tblData ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Not found\"}", 0);
		return;
	}
	
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}

// 后台添加
void Admin_article_Add(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		if ( tblForm ) xvoUnref(tblForm);
		return;
	}
	
	xtime now = xrtNow();
	int iIdx = 1;
	{ str sVal = xvoTableGetText(tblForm, "title", 5); sqlite3_bind_text(stmt_article_add, iIdx++, sVal ? sVal : "", -1, NULL); }

	sqlite3_bind_int64(stmt_article_add, iIdx++, now);
	sqlite3_bind_int64(stmt_article_add, iIdx++, now);
	sqlite3_step(stmt_article_add);
	int64 newId = sqlite3_last_insert_rowid(G_DB->objDB);
	sqlite3_reset(stmt_article_add);
	xvoUnref(tblForm);
	
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"id\": %lld}}", newId);
}

// 后台更新
void Admin_article_Save(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		if ( tblForm ) xvoUnref(tblForm);
		return;
	}
	
	int64 iID = xvoTableGetInt(tblForm, "id", 2);
	if ( iID <= 0 ) {
		xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xtime now = xrtNow();
	int iIdx = 1;
	{ str sVal = xvoTableGetText(tblForm, "title", 5); sqlite3_bind_text(stmt_article_put, iIdx++, sVal ? sVal : "", -1, NULL); }

	sqlite3_bind_int64(stmt_article_put, iIdx++, now);
	sqlite3_bind_int64(stmt_article_put, iIdx++, iID);
	sqlite3_step(stmt_article_put);
	sqlite3_reset(stmt_article_put);
	xvoUnref(tblForm);
	
	http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}

// 后台删除
void Admin_article_Delete(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sID[24];
	mg_http_get_var(&hm->query, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xtime now = xrtNow();
	sqlite3_bind_int64(stmt_article_del, 1, now);
	sqlite3_bind_int64(stmt_article_del, 2, iID);
	sqlite3_step(stmt_article_del);
	sqlite3_reset(stmt_article_del);
	
	http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}




// ==================== 路由注册 ====================

void Model_article_RegisterRoutes()
{
	printf("        [Model] Registering routes for article...\n");
	

	// 前台 API 路由
	Model_AddRoute("/api/v1/cms/article/list", Api_article_List, FALSE, FALSE, 0, 0);
	Model_AddRoute("/api/v1/cms/article/get", Api_article_Get, FALSE, FALSE, 0, 0);


/* SUBMIT DISABLED

	// 前台投稿路由
	Model_AddRoute("/api/v1/cms/article/submit", Api_article_Submit, TRUE, FALSE, 0, 0);
*/


	// 后台管理路由
	Model_AddRoute("/admin/cms/article/list", Admin_article_List, TRUE, TRUE, 38, 0);
	Model_AddRoute("/admin/cms/article/get", Admin_article_Get, TRUE, TRUE, 38, 0);
	Model_AddRoute("/admin/cms/article/add", Admin_article_Add, TRUE, TRUE, 38, 0);
	Model_AddRoute("/admin/cms/article/save", Admin_article_Save, TRUE, TRUE, 38, 0);
	Model_AddRoute("/admin/cms/article/delete", Admin_article_Delete, TRUE, TRUE, 38, 0);

}

void Model_article_UnregisterRoutes()
{
	printf("        [Model] Unregistering routes for article...\n");
	

	Model_RemoveRoute("/api/v1/cms/article/list");
	Model_RemoveRoute("/api/v1/cms/article/get");


/* SUBMIT DISABLED

	Model_RemoveRoute("/api/v1/cms/article/submit");
*/


	Model_RemoveRoute("/admin/cms/article/list");
	Model_RemoveRoute("/admin/cms/article/get");
	Model_RemoveRoute("/admin/cms/article/add");
	Model_RemoveRoute("/admin/cms/article/save");
	Model_RemoveRoute("/admin/cms/article/delete");

}



// ==================== 模型入口 ====================

void Model_article_Init()
{
	printf("    [Model] Init article...\n");
	Model_article_InitStmt();
	Model_article_RegisterRoutes();
}

void Model_article_Unit()
{
	printf("    [Model] Unit article...\n");
	Model_article_UnregisterRoutes();
	Model_article_FreeStmt();
}
