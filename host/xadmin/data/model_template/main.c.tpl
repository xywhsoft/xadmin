// ============================================
// 模型: {{MODEL_TITLE}} ({{MODEL_NAME}})
// 命名空间: {{MODEL_NAMESPACE}}
// 自动生成，请勿手动修改
// 生成时间: {{GENERATE_TIME}}
// ============================================



// ==================== 预编译SQL语句 ====================

sqlite3_stmt* stmt_{{MODEL_NAME}}_all = NULL;
sqlite3_stmt* stmt_{{MODEL_NAME}}_get = NULL;
sqlite3_stmt* stmt_{{MODEL_NAME}}_add = NULL;
sqlite3_stmt* stmt_{{MODEL_NAME}}_put = NULL;
sqlite3_stmt* stmt_{{MODEL_NAME}}_del = NULL;
sqlite3_stmt* stmt_{{MODEL_NAME}}_count = NULL;



// ==================== SQL预编译 ====================

void Model_{{MODEL_NAME}}_CompileSQL()
{
	int iRet;
	
	// 分页获取所有数据
	iRet = sqlite3_prepare_v3(G_ModelCtx->pDB->objDB, 
		"SELECT {{SQL_SELECT_FIELDS}} FROM {{TABLE_NAME}} WHERE isDelete = 0 ORDER BY id DESC LIMIT ? OFFSET ?", 
		-1, SQL_PREPARE_DEFAULT, &stmt_{{MODEL_NAME}}_all, NULL);
	if ( iRet != SQLITE_OK ) {
		G_ModelCtx->Log("[Model:{{MODEL_NAME}}] Compile stmt_all failed: %d", iRet);
	}
	
	// 根据ID获取单条数据
	iRet = sqlite3_prepare_v3(G_ModelCtx->pDB->objDB, 
		"SELECT {{SQL_SELECT_FIELDS}} FROM {{TABLE_NAME}} WHERE id = ? AND isDelete = 0", 
		-1, SQL_PREPARE_DEFAULT, &stmt_{{MODEL_NAME}}_get, NULL);
	if ( iRet != SQLITE_OK ) {
		G_ModelCtx->Log("[Model:{{MODEL_NAME}}] Compile stmt_get failed: %d", iRet);
	}
	
	// 添加数据
	iRet = sqlite3_prepare_v3(G_ModelCtx->pDB->objDB, 
		"INSERT INTO {{TABLE_NAME}} ({{SQL_INSERT_FIELDS}}, createTime, updateTime, isDelete) VALUES ({{SQL_INSERT_VALUES}}, ?, ?, 0)", 
		-1, SQL_PREPARE_DEFAULT, &stmt_{{MODEL_NAME}}_add, NULL);
	if ( iRet != SQLITE_OK ) {
		G_ModelCtx->Log("[Model:{{MODEL_NAME}}] Compile stmt_add failed: %d", iRet);
	}
	
	// 修改数据
	iRet = sqlite3_prepare_v3(G_ModelCtx->pDB->objDB, 
		"UPDATE {{TABLE_NAME}} SET {{SQL_UPDATE_FIELDS}}, updateTime = ? WHERE id = ?", 
		-1, SQL_PREPARE_DEFAULT, &stmt_{{MODEL_NAME}}_put, NULL);
	if ( iRet != SQLITE_OK ) {
		G_ModelCtx->Log("[Model:{{MODEL_NAME}}] Compile stmt_put failed: %d", iRet);
	}
	
	// 删除数据（软删除）
	iRet = sqlite3_prepare_v3(G_ModelCtx->pDB->objDB, 
		"UPDATE {{TABLE_NAME}} SET isDelete = 1, updateTime = ? WHERE id = ?", 
		-1, SQL_PREPARE_DEFAULT, &stmt_{{MODEL_NAME}}_del, NULL);
	if ( iRet != SQLITE_OK ) {
		G_ModelCtx->Log("[Model:{{MODEL_NAME}}] Compile stmt_del failed: %d", iRet);
	}
	
	// 统计总数
	iRet = sqlite3_prepare_v3(G_ModelCtx->pDB->objDB, 
		"SELECT COUNT(*) FROM {{TABLE_NAME}} WHERE isDelete = 0", 
		-1, SQL_PREPARE_DEFAULT, &stmt_{{MODEL_NAME}}_count, NULL);
	if ( iRet != SQLITE_OK ) {
		G_ModelCtx->Log("[Model:{{MODEL_NAME}}] Compile stmt_count failed: %d", iRet);
	}
}



// ==================== 前台API处理 ====================

{{#ENABLE_API}}
// API: 获取列表
void API_{{API_PREFIX}}_List(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	// TODO: 实现列表接口
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"ok\",\"data\":[]}");
}

// API: 获取详情
void API_{{API_PREFIX}}_Get(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	// TODO: 实现详情接口
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"ok\",\"data\":{}}");
}

// API: 添加数据
void API_{{API_PREFIX}}_Add(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	// TODO: 实现添加接口
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"ok\"}");
}

// API: 修改数据
void API_{{API_PREFIX}}_Put(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	// TODO: 实现修改接口
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"ok\"}");
}

// API: 删除数据
void API_{{API_PREFIX}}_Del(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	// TODO: 实现删除接口
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"ok\"}");
}
{{/ENABLE_API}}



// ==================== 后台管理处理 ====================

{{#ENABLE_ADMIN}}
// 后台: 数据接口
void Request_Model_{{ADMIN_PREFIX}}(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	// TODO: 实现后台数据接口
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"ok\",\"data\":[]}");
}

// 后台: 列表页面
void Request_View_Model_{{ADMIN_PREFIX}}(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	// TODO: 渲染列表页面
	str sPagePath = G_ModelCtx->Format("%s/{{MODEL_NAME}}/page/list.html", ModelPath);
	str sHtml = xrtFileRead(sPagePath);
	if ( sHtml ) {
		mg_http_reply(c, 200, HTTP_CT_HTML, "%s", sHtml);
		xrtFree(sHtml);
	} else {
		mg_http_reply(c, 404, HTTP_CT_HTML, "Page not found");
	}
	G_ModelCtx->Free(sPagePath);
}

// 后台: 添加页面
void Request_View_Model_{{ADMIN_PREFIX}}_Add(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	// TODO: 渲染添加页面
	str sPagePath = G_ModelCtx->Format("%s/{{MODEL_NAME}}/page/add.html", ModelPath);
	str sHtml = xrtFileRead(sPagePath);
	if ( sHtml ) {
		mg_http_reply(c, 200, HTTP_CT_HTML, "%s", sHtml);
		xrtFree(sHtml);
	} else {
		mg_http_reply(c, 404, HTTP_CT_HTML, "Page not found");
	}
	G_ModelCtx->Free(sPagePath);
}

// 后台: 编辑页面
void Request_View_Model_{{ADMIN_PREFIX}}_Edit(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	// TODO: 渲染编辑页面
	str sPagePath = G_ModelCtx->Format("%s/{{MODEL_NAME}}/page/edit.html", ModelPath);
	str sHtml = xrtFileRead(sPagePath);
	if ( sHtml ) {
		mg_http_reply(c, 200, HTTP_CT_HTML, "%s", sHtml);
		xrtFree(sHtml);
	} else {
		mg_http_reply(c, 404, HTTP_CT_HTML, "Page not found");
	}
	G_ModelCtx->Free(sPagePath);
}
{{/ENABLE_ADMIN}}



// ==================== 路由注册 ====================

void Model_{{MODEL_NAME}}_RegisterRoutes()
{
{{#ENABLE_API}}
	// 前台API路由
	G_ModelCtx->AddRoute("{{API_LIST_URI}}", API_{{API_PREFIX}}_List, {{API_AUTH}}, FALSE, {{API_AUTH_ID}}, {{API_AUTH_LEVEL}});
	G_ModelCtx->AddRoute("{{API_GET_URI}}", API_{{API_PREFIX}}_Get, {{API_AUTH}}, FALSE, {{API_AUTH_ID}}, {{API_AUTH_LEVEL}});
	G_ModelCtx->AddRoute("{{API_ADD_URI}}", API_{{API_PREFIX}}_Add, {{API_AUTH}}, FALSE, {{API_AUTH_ID}}, {{API_AUTH_LEVEL}});
	G_ModelCtx->AddRoute("{{API_PUT_URI}}", API_{{API_PREFIX}}_Put, {{API_AUTH}}, FALSE, {{API_AUTH_ID}}, {{API_AUTH_LEVEL}});
	G_ModelCtx->AddRoute("{{API_DEL_URI}}", API_{{API_PREFIX}}_Del, {{API_AUTH}}, FALSE, {{API_AUTH_ID}}, {{API_AUTH_LEVEL}});
{{/ENABLE_API}}

{{#ENABLE_ADMIN}}
	// 后台管理路由
	G_ModelCtx->AddRoute("{{ADMIN_DATA_URI}}", Request_Model_{{ADMIN_PREFIX}}, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	G_ModelCtx->AddRoute("{{ADMIN_LIST_URI}}", Request_View_Model_{{ADMIN_PREFIX}}, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	G_ModelCtx->AddRoute("{{ADMIN_ADD_URI}}", Request_View_Model_{{ADMIN_PREFIX}}_Add, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	G_ModelCtx->AddRoute("{{ADMIN_EDIT_URI}}", Request_View_Model_{{ADMIN_PREFIX}}_Edit, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
{{/ENABLE_ADMIN}}
}



// ==================== 模型初始化/卸载 ====================

void Model_{{MODEL_NAME}}_Init()
{
	G_ModelCtx->Log("[Model:{{MODEL_NAME}}] Init");
	
	// 预编译SQL
	Model_{{MODEL_NAME}}_CompileSQL();
	
	// 注册路由
	Model_{{MODEL_NAME}}_RegisterRoutes();
}

void Model_{{MODEL_NAME}}_Unit()
{
	G_ModelCtx->Log("[Model:{{MODEL_NAME}}] Unit");
	
	// 释放预编译SQL
	if ( stmt_{{MODEL_NAME}}_all ) sqlite3_finalize(stmt_{{MODEL_NAME}}_all);
	if ( stmt_{{MODEL_NAME}}_get ) sqlite3_finalize(stmt_{{MODEL_NAME}}_get);
	if ( stmt_{{MODEL_NAME}}_add ) sqlite3_finalize(stmt_{{MODEL_NAME}}_add);
	if ( stmt_{{MODEL_NAME}}_put ) sqlite3_finalize(stmt_{{MODEL_NAME}}_put);
	if ( stmt_{{MODEL_NAME}}_del ) sqlite3_finalize(stmt_{{MODEL_NAME}}_del);
	if ( stmt_{{MODEL_NAME}}_count ) sqlite3_finalize(stmt_{{MODEL_NAME}}_count);
}


