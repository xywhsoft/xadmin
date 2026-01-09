
// ============================================
// 模型: {{MODEL_TITLE}} ({{MODEL_NAME}})
// 自动生成代码 - 请勿手动修改
// 生成时间: {{GEN_TIME}}
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
sqlite3_stmt* stmt_{{MODEL_NAME}}_all;
sqlite3_stmt* stmt_{{MODEL_NAME}}_get;
sqlite3_stmt* stmt_{{MODEL_NAME}}_add;
sqlite3_stmt* stmt_{{MODEL_NAME}}_put;
sqlite3_stmt* stmt_{{MODEL_NAME}}_del;
sqlite3_stmt* stmt_{{MODEL_NAME}}_count;

// 初始化预编译语句
void Model_{{MODEL_NAME}}_InitStmt()
{
	sqlite3* db = G_DB->objDB;
	
	// 分页获取列表
	sqlite3_prepare_v3(db,
		"SELECT id, {{FIELD_NAMES}}, createTime, updateTime FROM {{TABLE_NAME}} "
		"WHERE isDelete = 0 ORDER BY id DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt_{{MODEL_NAME}}_all, NULL);
	
	// 根据ID获取单条记录
	sqlite3_prepare_v3(db,
		"SELECT id, {{FIELD_NAMES}}, createTime, updateTime FROM {{TABLE_NAME}} WHERE id = ? AND isDelete = 0",
		-1, 0, &stmt_{{MODEL_NAME}}_get, NULL);
	
	// 添加记录
	sqlite3_prepare_v3(db,
		"INSERT INTO {{TABLE_NAME}} ({{FIELD_NAMES}}, createTime, updateTime, isDelete) VALUES ({{FIELD_PLACEHOLDERS}}, ?, ?, 0)",
		-1, 0, &stmt_{{MODEL_NAME}}_add, NULL);
	
	// 更新记录
	sqlite3_prepare_v3(db,
		"UPDATE {{TABLE_NAME}} SET {{FIELD_UPDATE_SET}}, updateTime = ? WHERE id = ?",
		-1, 0, &stmt_{{MODEL_NAME}}_put, NULL);
	
	// 删除记录（软删除）
	sqlite3_prepare_v3(db,
		"UPDATE {{TABLE_NAME}} SET isDelete = 1, updateTime = ? WHERE id = ?",
		-1, 0, &stmt_{{MODEL_NAME}}_del, NULL);
	
	// 统计总数
	sqlite3_prepare_v3(db,
		"SELECT COUNT(*) FROM {{TABLE_NAME}} WHERE isDelete = 0",
		-1, 0, &stmt_{{MODEL_NAME}}_count, NULL);
}

// 销毁预编译语句
void Model_{{MODEL_NAME}}_FreeStmt()
{
	if ( stmt_{{MODEL_NAME}}_all ) sqlite3_finalize(stmt_{{MODEL_NAME}}_all);
	if ( stmt_{{MODEL_NAME}}_get ) sqlite3_finalize(stmt_{{MODEL_NAME}}_get);
	if ( stmt_{{MODEL_NAME}}_add ) sqlite3_finalize(stmt_{{MODEL_NAME}}_add);
	if ( stmt_{{MODEL_NAME}}_put ) sqlite3_finalize(stmt_{{MODEL_NAME}}_put);
	if ( stmt_{{MODEL_NAME}}_del ) sqlite3_finalize(stmt_{{MODEL_NAME}}_del);
	if ( stmt_{{MODEL_NAME}}_count ) sqlite3_finalize(stmt_{{MODEL_NAME}}_count);
}



{{#IF_ENABLE_API}}
// ==================== 前台 API ====================

// 获取列表
void Api_{{MODEL_NAME}}_List(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
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
	if ( sqlite3_step(stmt_{{MODEL_NAME}}_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_{{MODEL_NAME}}_count, 0);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_count);
	
	// 获取数据
	xvalue arrData = xvoCreateArray();
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_all, 1, iLimit);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_all, 2, iOffset);
	while ( sqlite3_step(stmt_{{MODEL_NAME}}_all) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_{{MODEL_NAME}}_all, iCol++));
{{FIELD_READ_CODE}}
		xtime iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_all, iCol++);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_all, iCol++);
		xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_all);
	
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
void Api_{{MODEL_NAME}}_Get(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sID[24];
	mg_http_get_var(&hm->query, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xvalue tblData = NULL;
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_get, 1, iID);
	if ( sqlite3_step(stmt_{{MODEL_NAME}}_get) == SQLITE_ROW ) {
		tblData = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblData, "id", 2, sqlite3_column_int64(stmt_{{MODEL_NAME}}_get, iCol++));
{{FIELD_READ_CODE_GET}}
		xtime iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_get, iCol++);
		xvoTableSetText(tblData, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_get, iCol++);
		xvoTableSetText(tblData, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_get);
	
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
{{#ENDIF_ENABLE_API}}



{{#IF_ENABLE_SUBMIT}}
// ==================== 前台投稿 ====================

// 投稿内容
void Api_{{MODEL_NAME}}_Submit(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
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
{{FIELD_BIND_ADD_CODE}}
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, now);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, now);
	sqlite3_step(stmt_{{MODEL_NAME}}_add);
	int64 newId = sqlite3_last_insert_rowid(G_DB->objDB);
	sqlite3_reset(stmt_{{MODEL_NAME}}_add);
	xvoUnref(tblForm);
	
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"id\": %lld}}", newId);
}
{{#ENDIF_ENABLE_SUBMIT}}



{{#IF_ENABLE_ADMIN}}
// ==================== 后台管理 API ====================

// 后台列表
void Admin_{{MODEL_NAME}}_List(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
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
	if ( sqlite3_step(stmt_{{MODEL_NAME}}_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_{{MODEL_NAME}}_count, 0);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_count);
	
	// 获取数据
	xvalue arrData = xvoCreateArray();
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_all, 1, iLimit);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_all, 2, iOffset);
	while ( sqlite3_step(stmt_{{MODEL_NAME}}_all) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_{{MODEL_NAME}}_all, iCol++));
{{FIELD_READ_CODE}}
		xtime iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_all, iCol++);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_all, iCol++);
		xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_all);
	
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
void Admin_{{MODEL_NAME}}_Get(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sID[24];
	mg_http_get_var(&hm->query, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xvalue tblData = NULL;
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_get, 1, iID);
	if ( sqlite3_step(stmt_{{MODEL_NAME}}_get) == SQLITE_ROW ) {
		tblData = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblData, "id", 2, sqlite3_column_int64(stmt_{{MODEL_NAME}}_get, iCol++));
{{FIELD_READ_CODE_GET}}
		xtime iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_get, iCol++);
		xvoTableSetText(tblData, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_get, iCol++);
		xvoTableSetText(tblData, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_get);
	
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
void Admin_{{MODEL_NAME}}_Add(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
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
{{FIELD_BIND_ADD_CODE}}
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, now);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, now);
	sqlite3_step(stmt_{{MODEL_NAME}}_add);
	int64 newId = sqlite3_last_insert_rowid(G_DB->objDB);
	sqlite3_reset(stmt_{{MODEL_NAME}}_add);
	xvoUnref(tblForm);
	
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"id\": %lld}}", newId);
}

// 后台更新
void Admin_{{MODEL_NAME}}_Save(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
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
{{FIELD_BIND_UPDATE_CODE}}
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_put, iIdx++, now);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_put, iIdx++, iID);
	sqlite3_step(stmt_{{MODEL_NAME}}_put);
	sqlite3_reset(stmt_{{MODEL_NAME}}_put);
	xvoUnref(tblForm);
	
	http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}

// 后台删除
void Admin_{{MODEL_NAME}}_Delete(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sID[24];
	mg_http_get_var(&hm->query, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xtime now = xrtNow();
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_del, 1, now);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_del, 2, iID);
	sqlite3_step(stmt_{{MODEL_NAME}}_del);
	sqlite3_reset(stmt_{{MODEL_NAME}}_del);
	
	http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}
{{#ENDIF_ENABLE_ADMIN}}



// ==================== 路由注册 ====================

void Model_{{MODEL_NAME}}_RegisterRoutes()
{
	printf("        [Model] Registering routes for {{MODEL_NAME}}...\n");
	
{{#IF_ENABLE_API}}
	// 前台 API 路由
	Model_AddRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/list", Api_{{MODEL_NAME}}_List, FALSE, FALSE, 0, {{API_AUTH_LEVEL}});
	Model_AddRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/get", Api_{{MODEL_NAME}}_Get, FALSE, FALSE, 0, {{API_AUTH_LEVEL}});
{{#ENDIF_ENABLE_API}}

{{#IF_ENABLE_SUBMIT}}
	// 前台投稿路由
	Model_AddRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/submit", Api_{{MODEL_NAME}}_Submit, TRUE, FALSE, 0, {{API_AUTH_LEVEL}});
{{#ENDIF_ENABLE_SUBMIT}}

{{#IF_ENABLE_ADMIN}}
	// 后台管理路由
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/list", Admin_{{MODEL_NAME}}_List, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/get", Admin_{{MODEL_NAME}}_Get, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/add", Admin_{{MODEL_NAME}}_Add, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/save", Admin_{{MODEL_NAME}}_Save, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/delete", Admin_{{MODEL_NAME}}_Delete, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
{{#ENDIF_ENABLE_ADMIN}}
}

void Model_{{MODEL_NAME}}_UnregisterRoutes()
{
	printf("        [Model] Unregistering routes for {{MODEL_NAME}}...\n");
	
{{#IF_ENABLE_API}}
	Model_RemoveRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/list");
	Model_RemoveRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/get");
{{#ENDIF_ENABLE_API}}

{{#IF_ENABLE_SUBMIT}}
	Model_RemoveRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/submit");
{{#ENDIF_ENABLE_SUBMIT}}

{{#IF_ENABLE_ADMIN}}
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/list");
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/get");
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/add");
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/save");
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/delete");
{{#ENDIF_ENABLE_ADMIN}}
}



// ==================== 模型入口 ====================

void Model_{{MODEL_NAME}}_Init()
{
	printf("    [Model] Init {{MODEL_NAME}}...\n");
	Model_{{MODEL_NAME}}_InitStmt();
	Model_{{MODEL_NAME}}_RegisterRoutes();
}

void Model_{{MODEL_NAME}}_Unit()
{
	printf("    [Model] Unit {{MODEL_NAME}}...\n");
	Model_{{MODEL_NAME}}_UnregisterRoutes();
	Model_{{MODEL_NAME}}_FreeStmt();
}
