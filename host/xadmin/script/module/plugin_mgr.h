


// ============================================
// 插件管理器
// ============================================

#ifndef PLUGIN_MGR_H
#define PLUGIN_MGR_H

#include "plugin_ctx.h"




// ==================== 数据结构定义 ====================

// 插件实例结构体
typedef struct {
	
	// ===== 基础信息 =====
	str sName;                  // 插件标识（目录名）
	str sTitle;                 // 显示名称
	str sDesc;                  // 描述
	str sVersion;               // 版本号
	str sAuthor;                // 作者
	int iSort;                  // 排序（加载顺序）
	
	// ===== 路径 =====
	str sPath;                  // 插件目录路径
	str sConfigPath;            // 配置文件路径
	str sCodePath;              // 主代码文件路径
	str sDataPath;              // 数据目录路径
	
	// ===== 状态 =====
	bool bEnabled;              // 是否启用
	bool bLoaded;               // 是否已加载
	int iLoadOrder;             // 实际加载顺序
	
	// ===== TCC 状态机 =====
	TCCState* pTccState;
	
	// ===== 资源跟踪 =====
	xlist lstDependencies;      // 依赖的其他插件
	xlist lstDependents;        // 被依赖列表（反向依赖，用于依赖追踪）
	xlist lstRoutes;            // 注册的路由URI列表
	xlist lstMenuIds;           // 注册的菜单ID列表
	xlist lstAuthGroupIds;      // 注册的权限分类ID列表
	xlist lstAuthIds;           // 注册的权限分组ID列表
	
	// ===== 配置 =====
	xvalue tblSettings;         // 插件自定义配置
	xvalue arrExports;          // 导出接口名称列表
	
	// ===== 时间戳 =====
	int64 iCreateTime;
	int64 iUpdateTime;
	int64 iEnableTime;
	
} PluginInstance;



// 插件管理器结构体
typedef struct {
	
	xdict tblPlugins;           // 插件实例表（key: name）
	xlist lstLoadedPlugins;     // 已加载的插件列表（按加载顺序）
	xlist lstEventListeners;    // 事件监听器列表
	xdict tblExports;           // 插件导出表（key: pluginName:exportName）
	
} PluginManager;



// 全局插件管理器
PluginManager* G_PluginMgr = NULL;

// 全局插件上下文
PluginContext* G_PluginCtx = NULL;

// 插件目录路径
str PluginPath = NULL;
str PluginDataPath = NULL;



// ==================== 前向声明 ====================

void PluginCtx_Log(int level, str format, ...);
bool Plugin_Enable(PluginInstance* pPlugin);
bool Plugin_Disable(PluginInstance* pPlugin);

// 依赖管理相关函数
int Plugin_CompareVersion(str v1, str v2);
bool Plugin_CheckVersionRequirement(str actualVersion, str minVersion, str maxVersion);
void Plugin_BuildDependentsGraph();
bool Plugin_TopologicalSort(xlist* pResult);
bool Plugin_ValidateDependencies();



// ==================== 上下文接口实现 ====================

// 路由操作
RouteInfo* PluginCtx_AddRoute(str uri, void* proc, bool bAuth, bool bAdmin, int authId, int authLevel)
{
	RouteInfo* pInfo = xrtDictSet(G_StaticRouteTableHTTP, uri, strlen(uri), NULL);
	if ( pInfo ) {
		pInfo->Proc = proc;
		pInfo->bAuth = bAuth;
		pInfo->bAdmin = bAdmin;
		pInfo->bPutLog = FALSE;
		pInfo->bActive = FALSE;
		pInfo->AuthID = authId;
		pInfo->AuthLevel = authLevel;
		printf("        [Plugin] Route added: %s\n", uri);
	}
	return pInfo;
}

void PluginCtx_RemoveRoute(str uri)
{
	xrtDictRemove(G_StaticRouteTableHTTP, uri, strlen(uri));
	printf("        [Plugin] Route removed: %s\n", uri);
}

RouteInfo* PluginCtx_GetRoute(str uri)
{
	return xrtDictGet(G_StaticRouteTableHTTP, uri, strlen(uri));
}


// 菜单操作
int PluginCtx_AddMenu(int parent, str title, str icon, int type, str openType, str href, int sort, bool visible)
{
	int64 iNow = xrtNow();
	sqlite3_bind_int(stmt_menu_add, 1, parent);
	sqlite3_bind_text(stmt_menu_add, 2, title, -1, NULL);
	sqlite3_bind_text(stmt_menu_add, 3, icon ? icon : (str)"", -1, NULL);
	sqlite3_bind_int(stmt_menu_add, 4, type);
	sqlite3_bind_text(stmt_menu_add, 5, openType ? openType : (str)"_component", -1, NULL);
	sqlite3_bind_text(stmt_menu_add, 6, href ? href : (str)"", -1, NULL);
	sqlite3_bind_int(stmt_menu_add, 7, sort);
	sqlite3_bind_int(stmt_menu_add, 8, visible ? 1 : 0);
	sqlite3_bind_text(stmt_menu_add, 9, "", -1, NULL);
	sqlite3_bind_int64(stmt_menu_add, 10, iNow);
	sqlite3_bind_int64(stmt_menu_add, 11, iNow);
	sqlite3_step(stmt_menu_add);
	int iMenuId = sqlite3_last_insert_rowid(G_DB->objDB);
	sqlite3_reset(stmt_menu_add);
	printf("        [Plugin] Menu added: %s (id=%d)\n", title, iMenuId);
	return iMenuId;
}

bool PluginCtx_RemoveMenu(int menuId)
{
	int64 iNow = xrtNow();
	sqlite3_bind_int64(stmt_menu_del, 1, iNow);
	sqlite3_bind_int(stmt_menu_del, 2, menuId);
	sqlite3_step(stmt_menu_del);
	sqlite3_reset(stmt_menu_del);
	printf("        [Plugin] Menu removed: id=%d\n", menuId);
	return TRUE;
}

bool PluginCtx_ShowMenu(int menuId)
{
	char* sErr = NULL;
	str sSQL = xrtFormat("UPDATE menu SET visible = 1, updateTime = %lld WHERE id = %d", xrtNow(), menuId);
	sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, &sErr);
	xrtFree(sSQL);
	if ( sErr ) {
		sqlite3_free(sErr);
		return FALSE;
	}
	return TRUE;
}

bool PluginCtx_HideMenu(int menuId)
{
	char* sErr = NULL;
	str sSQL = xrtFormat("UPDATE menu SET visible = 0, updateTime = %lld WHERE id = %d", xrtNow(), menuId);
	sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, &sErr);
	xrtFree(sSQL);
	if ( sErr ) {
		sqlite3_free(sErr);
		return FALSE;
	}
	return TRUE;
}


// 权限操作
int PluginCtx_AddAuthGroup(str name, str desc, int sort)
{
	int64 iNow = xrtNow();
	sqlite3_bind_text(stmt_group_add, 1, name, -1, NULL);
	sqlite3_bind_text(stmt_group_add, 2, desc ? desc : (str)"", -1, NULL);
	sqlite3_bind_int(stmt_group_add, 3, sort);
	sqlite3_bind_int64(stmt_group_add, 4, iNow);
	sqlite3_bind_int64(stmt_group_add, 5, iNow);
	sqlite3_step(stmt_group_add);
	int iGroupId = sqlite3_last_insert_rowid(G_DB->objDB);
	sqlite3_reset(stmt_group_add);
	printf("        [Plugin] AuthGroup added: %s (id=%d)\n", name, iGroupId);
	return iGroupId;
}

int PluginCtx_AddAuth(int groupId, str name, str desc, int sort)
{
	int64 iNow = xrtNow();
	sqlite3_bind_int(stmt_auth_add, 1, groupId);
	sqlite3_bind_text(stmt_auth_add, 2, name, -1, NULL);
	sqlite3_bind_text(stmt_auth_add, 3, desc ? desc : (str)"", -1, NULL);
	sqlite3_bind_int(stmt_auth_add, 4, sort);
	sqlite3_bind_int64(stmt_auth_add, 5, iNow);
	sqlite3_bind_int64(stmt_auth_add, 6, iNow);
	sqlite3_step(stmt_auth_add);
	int iAuthId = sqlite3_last_insert_rowid(G_DB->objDB);
	sqlite3_reset(stmt_auth_add);
	printf("        [Plugin] Auth added: %s (id=%d)\n", name, iAuthId);
	return iAuthId;
}

bool PluginCtx_RemoveAuthGroup(int groupId)
{
	int64 iNow = xrtNow();
	sqlite3_bind_int64(stmt_group_del, 1, iNow);
	sqlite3_bind_int(stmt_group_del, 2, groupId);
	sqlite3_step(stmt_group_del);
	sqlite3_reset(stmt_group_del);
	return TRUE;
}

bool PluginCtx_RemoveAuth(int authId)
{
	int64 iNow = xrtNow();
	sqlite3_bind_int64(stmt_auth_del, 1, iNow);
	sqlite3_bind_int(stmt_auth_del, 2, authId);
	sqlite3_step(stmt_auth_del);
	sqlite3_reset(stmt_auth_del);
	return TRUE;
}

void PluginCtx_SyncUriAuth(str uri, int authId, str desc, bool isBackend, bool needAuth, bool needLog)
{
	// 检查URI是否存在
	sqlite3_stmt* stmt_check;
	sqlite3_prepare_v3(G_DB->objDB, "SELECT id FROM uris WHERE uri = ?", -1, 0, &stmt_check, NULL);
	sqlite3_bind_text(stmt_check, 1, uri, -1, NULL);
	
	int64 iNow = xrtNow();
	if ( sqlite3_step(stmt_check) == SQLITE_ROW ) {
		// 更新
		int iId = sqlite3_column_int(stmt_check, 0);
		sqlite3_finalize(stmt_check);
		
		char* sErr = NULL;
		str sSQL = xrtFormat(
			"UPDATE uris SET authID = %d, desc = '%s', isBackend = %d, needAuth = %d, needLog = %d, updateTime = %lld WHERE id = %d",
			authId, desc ? desc : (str)"", isBackend ? 1 : 0, needAuth ? 1 : 0, needLog ? 1 : 0, iNow, iId
		);
		sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, &sErr);
		xrtFree(sSQL);
		if ( sErr ) sqlite3_free(sErr);
	} else {
		// 插入
		sqlite3_finalize(stmt_check);
		
		sqlite3_bind_int(stmt_uris_add, 1, authId);
		sqlite3_bind_text(stmt_uris_add, 2, uri, -1, NULL);
		sqlite3_bind_text(stmt_uris_add, 3, desc ? desc : (str)"", -1, NULL);
		sqlite3_bind_int(stmt_uris_add, 4, isBackend ? 1 : 0);
		sqlite3_bind_int(stmt_uris_add, 5, needAuth ? 1 : 0);
		sqlite3_bind_int(stmt_uris_add, 6, needLog ? 1 : 0);
		sqlite3_bind_int(stmt_uris_add, 7, 0);
		sqlite3_bind_int(stmt_uris_add, 8, 0);
		sqlite3_bind_int64(stmt_uris_add, 9, iNow);
		sqlite3_bind_int64(stmt_uris_add, 10, iNow);
		sqlite3_step(stmt_uris_add);
		sqlite3_reset(stmt_uris_add);
	}
}

void PluginCtx_ReloadAuthCache()
{
	ReloadCache_Auth_Auth();
	ReloadCache_Auth_Group();
	Auth_ReloadCache();
}


// Session 操作
xvalue PluginCtx_GetAdminSession(str token)
{
	return xvoTableGetValue(G_AdminSession, token, strlen(token));
}

xvalue PluginCtx_GetMemberSession(str token)
{
	return xvoTableGetValue(G_MemberSession, token, strlen(token));
}

str PluginCtx_CreateAdminSession(int64 userId, str userName, int roleId, int timeout)
{
	str sToken = xrtMakeXIDS();
	xvalue tblSession = xvoCreateTable();
	xvoTableSetInt(tblSession, "id", 2, userId);
	xvoTableSetText(tblSession, "user", 4, userName, 0, FALSE);
	xvoTableSetInt(tblSession, "role", 4, roleId);
	xvoTableSetInt(tblSession, "expire", 6, xrtNow() + timeout);
	xvoTableSetValue(G_AdminSession, sToken, 32, tblSession, TRUE);
	return sToken;
}

str PluginCtx_CreateMemberSession(int64 userId, str userName, int groupId, int timeout)
{
	str sToken = xrtMakeXIDS();
	xvalue tblSession = xvoCreateTable();
	xvoTableSetInt(tblSession, "id", 2, userId);
	xvoTableSetText(tblSession, "username", 8, userName, 0, FALSE);
	xvoTableSetInt(tblSession, "groupId", 7, groupId);
	xvoTableSetInt(tblSession, "expire", 6, xrtNow() + timeout);
	xvoTableSetValue(G_MemberSession, sToken, 32, tblSession, TRUE);
	return sToken;
}

void PluginCtx_DestroyAdminSession(str token)
{
	xvoTableRemove(G_AdminSession, token, strlen(token));
}

void PluginCtx_DestroyMemberSession(str token)
{
	xvoTableRemove(G_MemberSession, token, strlen(token));
}

void PluginCtx_ExtendSession(bool isAdmin, str token, int timeout)
{
	xvalue tblSession = isAdmin ? 
		xvoTableGetValue(G_AdminSession, token, strlen(token)) :
		xvoTableGetValue(G_MemberSession, token, strlen(token));
	if ( tblSession ) {
		xvoTableSetInt(tblSession, "expire", 6, xrtNow() + timeout);
	}
}


// HTTP 响应
void PluginCtx_SendJson(struct mg_connection* c, int code, str json, size_t len)
{
	http_reply(c, code, HTTP_CT_JSON, json, len);
}

void PluginCtx_SendHtml(struct mg_connection* c, int code, str html)
{
	http_reply(c, code, HTTP_CT_HTML, html, strlen(html));
}

void PluginCtx_SendPage(struct mg_connection* c, str pagePath, xvalue data)
{
	size_t iRetSize = 0;
	str sHtml = MakePageWithTemplate(pagePath, data, &iRetSize);
	if ( sHtml ) {
		http_reply(c, 200, HTTP_CT_HTML, sHtml, iRetSize > 0 ? iRetSize : strlen(sHtml));
		xrtFree(sHtml);
	} else {
		http_reply(c, 500, HTTP_CT_HTML, "Page render failed", 0);
	}
}

void PluginCtx_SendFile(struct mg_connection* c, str filePath, str mimeType)
{
	struct mg_http_serve_opts opts = { .mime_types = mimeType };
	mg_http_serve_file(c, NULL, filePath, &opts);
}

void PluginCtx_SendError(struct mg_connection* c, int code, str message)
{
	str sJson = xrtFormat("{\"result\":false,\"message\":\"%s\"}", message);
	http_reply(c, code, HTTP_CT_JSON, sJson, strlen(sJson));
	xrtFree(sJson);
}


// 配置操作
xvalue PluginCtx_GetOption(str group, str key)
{
	xvalue tblGroup = xvoTableGetValue(G_Option, group, strlen(group));
	if ( tblGroup && key ) {
		return xvoTableGetValue(tblGroup, key, strlen(key));
	}
	return tblGroup;
}

bool PluginCtx_SetOption(str group, str key, xvalue value)
{
	xvalue tblGroup = xvoTableGetValue(G_Option, group, strlen(group));
	if ( !tblGroup ) {
		tblGroup = xvoCreateTable();
		xvoTableSetValue(G_Option, group, strlen(group), tblGroup, TRUE);
	}
	xvoTableSetValue(tblGroup, key, strlen(key), value, FALSE);
	return TRUE;
}

void PluginCtx_ReloadOption(str group)
{
	str sPath = xrtPathJoin(2, OptionPath, xrtFormat("%s.json", group));
	xvalue tblOption = xrtParseJSON_File(sPath);
	xrtFree(sPath);
	if ( tblOption ) {
		xvoTableSetValue(G_Option, group, strlen(group), tblOption, TRUE);
	}
}


// JSON 操作
xvalue PluginCtx_JsonParse(str json, size_t len)
{
	return xrtParseJSON(json, len);
}

str PluginCtx_JsonStringify(xvalue val, size_t* outLen)
{
	return xrtStringifyJSON(val, FALSE, outLen);
}

void PluginCtx_JsonFree(xvalue val)
{
	xvoUnref(val);
}


// 工具函数
int64 PluginCtx_TimeNow()
{
	return xrtNow();
}

str PluginCtx_Format(str fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char buffer[4096];
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	return xrtCopyStr(buffer, 0);
}

void PluginCtx_Free(void* ptr)
{
	xrtFree(ptr);
}

str PluginCtx_HashPassword(str user, str salt, str clientHash)
{
	return ServerHashPassword(user, salt, clientHash);
}

str PluginCtx_GenerateSalt()
{
	return xrtMakeXIDS();
}

str PluginCtx_GenerateToken(int length)
{
	return xrtMakeXIDS();
}


// 日志
void PluginCtx_Log(int level, str format, ...)
{
	str sLevel = "DEBUG";
	if ( level == LOG_INFO ) sLevel = "INFO";
	else if ( level == LOG_WARN ) sLevel = "WARN";
	else if ( level == LOG_ERROR ) sLevel = "ERROR";
	
	va_list args;
	va_start(args, format);
	printf("[%s] ", sLevel);
	vprintf(format, args);
	printf("\n");
	va_end(args);
}

void PluginCtx_LogAccess(str user, str uri, str method, str param, str body)
{
	// 简化实现：打印访问日志
	printf("[ACCESS] user=%s uri=%s method=%s\n", user ? user : (str)"", uri, method);
}


// 插件间通信
void* PluginCtx_GetPluginExport(str pluginName, str exportName)
{
	str sKey = xrtFormat("%s:%s", pluginName, exportName);
	PluginExport* pExport = xrtDictGet(G_PluginMgr->tblExports, sKey, strlen(sKey));
	xrtFree(sKey);
	return pExport ? pExport->pPtr : NULL;
}

bool PluginCtx_SetPluginExport(str pluginName, str exportName, void* ptr)
{
	str sKey = xrtFormat("%s:%s", pluginName, exportName);
	PluginExport* pExport = xrtDictSet(G_PluginMgr->tblExports, sKey, strlen(sKey), NULL);
	if ( pExport ) {
		pExport->sPluginName = xrtCopyStr(pluginName, 0);
		pExport->sExportName = xrtCopyStr(exportName, 0);
		pExport->pPtr = ptr;
		printf("        [Plugin] Export registered: %s\n", sKey);
	}
	xrtFree(sKey);
	return pExport != NULL;
}


// 事件系统
bool PluginCtx_EmitEvent(str eventName, xvalue eventData)
{
	int iCount = xrtListCount(G_PluginMgr->lstEventListeners);
	for ( int i = 0; i < iCount; i++ ) {
		EventListener* pListener = xrtListGetPtr(G_PluginMgr->lstEventListeners, i);
		if ( pListener && strcmp(pListener->sEventName, eventName) == 0 ) {
			int iCbCount = xrtListCount(pListener->lstCallbacks);
			for ( int j = 0; j < iCbCount; j++ ) {
				PluginEventCallback cb = xrtListGetPtr(pListener->lstCallbacks, j);
				if ( cb ) {
					cb(eventName, eventData);
				}
			}
			return TRUE;
		}
	}
	return FALSE;
}

bool PluginCtx_OnEvent(str eventName, void* callback)
{
	// 查找是否已有该事件的监听器
	int iCount = xrtListCount(G_PluginMgr->lstEventListeners);
	for ( int i = 0; i < iCount; i++ ) {
		EventListener* pListener = xrtListGetPtr(G_PluginMgr->lstEventListeners, i);
		if ( pListener && strcmp(pListener->sEventName, eventName) == 0 ) {
			int iIdx = xrtListCount(pListener->lstCallbacks);
			xrtListSetPtr(pListener->lstCallbacks, iIdx, callback, NULL);
			return TRUE;
		}
	}
	
	// 创建新的监听器
	EventListener* pListener = xrtMalloc(sizeof(EventListener));
	pListener->sEventName = xrtCopyStr(eventName, 0);
	pListener->lstCallbacks = xrtListCreate(sizeof(ptr));
	xrtListSetPtr(pListener->lstCallbacks, 0, callback, NULL);
	
	int iIdx = xrtListCount(G_PluginMgr->lstEventListeners);
	xrtListSetPtr(G_PluginMgr->lstEventListeners, iIdx, pListener, NULL);
	return TRUE;
}

void PluginCtx_OffEvent(str eventName, void* callback)
{
	int iCount = xrtListCount(G_PluginMgr->lstEventListeners);
	for ( int i = 0; i < iCount; i++ ) {
		EventListener* pListener = xrtListGetPtr(G_PluginMgr->lstEventListeners, i);
		if ( pListener && strcmp(pListener->sEventName, eventName) == 0 ) {
			int iCbCount = xrtListCount(pListener->lstCallbacks);
			for ( int j = 0; j < iCbCount; j++ ) {
				if ( xrtListGetPtr(pListener->lstCallbacks, j) == callback ) {
					xrtListRemove(pListener->lstCallbacks, j);
					return;
				}
			}
		}
	}
}



// ==================== 插件实例管理 ====================

// 创建插件实例
PluginInstance* Plugin_Create(str sName)
{
	PluginInstance* pPlugin = xrtMalloc(sizeof(PluginInstance));
	memset(pPlugin, 0, sizeof(PluginInstance));

	pPlugin->sName = xrtCopyStr(sName, 0);
	pPlugin->lstRoutes = xrtListCreate(sizeof(ptr));
	pPlugin->lstMenuIds = xrtListCreate(sizeof(int));
	pPlugin->lstAuthGroupIds = xrtListCreate(sizeof(int));
	pPlugin->lstAuthIds = xrtListCreate(sizeof(int));
	pPlugin->lstDependencies = xrtListCreate(sizeof(ptr));
	pPlugin->lstDependents = xrtListCreate(sizeof(ptr));
	pPlugin->bEnabled = FALSE;
	pPlugin->bLoaded = FALSE;

	return pPlugin;
}


// 销毁插件实例
void Plugin_Destroy(PluginInstance* pPlugin)
{
	if ( !pPlugin ) return;
	
	// 如果已加载，先卸载
	if ( pPlugin->bLoaded ) {
		Plugin_Disable(pPlugin);
	}
	
	// 释放 TCC 状态机
	if ( pPlugin->pTccState ) {
		tcc_delete(pPlugin->pTccState);
		pPlugin->pTccState = NULL;
	}
	
	// 释放列表
	if ( pPlugin->lstRoutes ) xrtListDestroy(pPlugin->lstRoutes);
	if ( pPlugin->lstMenuIds ) xrtListDestroy(pPlugin->lstMenuIds);
	if ( pPlugin->lstAuthGroupIds ) xrtListDestroy(pPlugin->lstAuthGroupIds);
	if ( pPlugin->lstAuthIds ) xrtListDestroy(pPlugin->lstAuthIds);
	if ( pPlugin->lstDependencies ) xrtListDestroy(pPlugin->lstDependencies);
	if ( pPlugin->lstDependents ) xrtListDestroy(pPlugin->lstDependents);
	
	// 释放配置
	if ( pPlugin->tblSettings ) xvoUnref(pPlugin->tblSettings);
	if ( pPlugin->arrExports ) xvoUnref(pPlugin->arrExports);
	
	// 释放字符串
	if ( pPlugin->sName ) xrtFree(pPlugin->sName);
	if ( pPlugin->sTitle ) xrtFree(pPlugin->sTitle);
	if ( pPlugin->sDesc ) xrtFree(pPlugin->sDesc);
	if ( pPlugin->sVersion ) xrtFree(pPlugin->sVersion);
	if ( pPlugin->sAuthor ) xrtFree(pPlugin->sAuthor);
	if ( pPlugin->sPath ) xrtFree(pPlugin->sPath);
	if ( pPlugin->sConfigPath ) xrtFree(pPlugin->sConfigPath);
	if ( pPlugin->sCodePath ) xrtFree(pPlugin->sCodePath);
	if ( pPlugin->sDataPath ) xrtFree(pPlugin->sDataPath);
	
	xrtFree(pPlugin);
}


// 从配置文件加载插件信息
bool Plugin_LoadConfig(PluginInstance* pPlugin)
{
	xvalue tblConfig = xrtParseJSON_File(pPlugin->sConfigPath);
	if ( !tblConfig ) {
		printf("        [Plugin] Failed to load config: %s\n", pPlugin->sConfigPath);
		return FALSE;
	}
	
	// 读取基础信息
	str sTitle = xvoTableGetText(tblConfig, "title", 5);
	str sDesc = xvoTableGetText(tblConfig, "desc", 4);
	str sVersion = xvoTableGetText(tblConfig, "version", 7);
	str sAuthor = xvoTableGetText(tblConfig, "author", 6);
	
	pPlugin->sTitle = sTitle ? xrtCopyStr(sTitle, 0) : xrtCopyStr(pPlugin->sName, 0);
	pPlugin->sDesc = sDesc ? xrtCopyStr(sDesc, 0) : xrtCopyStr("", 0);
	pPlugin->sVersion = sVersion ? xrtCopyStr(sVersion, 0) : xrtCopyStr("1.0.0", 0);
	pPlugin->sAuthor = sAuthor ? xrtCopyStr(sAuthor, 0) : xrtCopyStr("", 0);
	pPlugin->iSort = xvoTableGetInt(tblConfig, "sort", 4);
	pPlugin->bEnabled = xvoTableGetBool(tblConfig, "enabled", 7);
	
	// 读取自定义配置
	xvalue tblSettings = xvoTableGetValue(tblConfig, "settings", 8);
	if ( tblSettings ) {
		xvoAddRef(tblSettings);
		pPlugin->tblSettings = tblSettings;
	}
	
	// 读取导出列表
	xvalue arrExports = xvoTableGetValue(tblConfig, "exports", 7);
	if ( arrExports ) {
		xvoAddRef(arrExports);
		pPlugin->arrExports = arrExports;
	}

	// 读取依赖列表（支持新旧两种格式）
	xvalue arrDependencies = xvoTableGetValue(tblConfig, "dependencies", 12);
	if ( arrDependencies ) {
		// 尝试作为数组处理
		int iDepCount = xvoArrayItemCount(arrDependencies);
		if ( iDepCount > 0 ) {
			// 是数组格式
			for ( int i = 0; i < iDepCount; i++ ) {
				xvalue depItem = xvoArrayGetValue(arrDependencies, i);

				// 尝试读取 table 的 plugin 字段来判断格式
				str sPluginName = xvoTableGetText(depItem, "plugin", 6);

				if ( sPluginName && strlen(sPluginName) > 0 ) {
					// 对象格式: {"plugin": "name", "minVersion": "1.0.0"}
					PluginDependency* pDep = xrtMalloc(sizeof(PluginDependency));
					memset(pDep, 0, sizeof(PluginDependency));

					str sMinVersion = xvoTableGetText(depItem, "minVersion", 11);
					str sMaxVersion = xvoTableGetText(depItem, "maxVersion", 11);

					pDep->sPluginName = xrtCopyStr(sPluginName, 0);
					pDep->sMinVersion = sMinVersion ? xrtCopyStr(sMinVersion, 0) : NULL;
					pDep->sMaxVersion = sMaxVersion ? xrtCopyStr(sMaxVersion, 0) : NULL;

					int iIdx = xrtListCount(pPlugin->lstDependencies);
					xrtListSetPtr(pPlugin->lstDependencies, iIdx, pDep, NULL);
				} else {
					// 尝试作为字符串处理（向后兼容）: "plugin_name"
					str sName = xvoTableGetText(depItem, 0, 0);
					if ( sName && strlen(sName) > 0 ) {
						PluginDependency* pDep = xrtMalloc(sizeof(PluginDependency));
						memset(pDep, 0, sizeof(PluginDependency));

						pDep->sPluginName = xrtCopyStr(sName, 0);
						pDep->sMinVersion = NULL;
						pDep->sMaxVersion = NULL;

						int iIdx = xrtListCount(pPlugin->lstDependencies);
						xrtListSetPtr(pPlugin->lstDependencies, iIdx, pDep, NULL);
					}
				}
			}
		}
	}

	xvoUnref(tblConfig);
	return TRUE;
}


// 保存插件配置
bool Plugin_SaveConfig(PluginInstance* pPlugin)
{
	xvalue tblConfig = xvoCreateTable();
	
	xvoTableSetText(tblConfig, "name", 4, pPlugin->sName, 0, FALSE);
	xvoTableSetText(tblConfig, "title", 5, pPlugin->sTitle, 0, FALSE);
	xvoTableSetText(tblConfig, "desc", 4, pPlugin->sDesc, 0, FALSE);
	xvoTableSetText(tblConfig, "version", 7, pPlugin->sVersion, 0, FALSE);
	xvoTableSetText(tblConfig, "author", 6, pPlugin->sAuthor, 0, FALSE);
	xvoTableSetInt(tblConfig, "sort", 4, pPlugin->iSort);
	xvoTableSetBool(tblConfig, "enabled", 7, pPlugin->bEnabled);
	
	if ( pPlugin->tblSettings ) {
		xvoTableSetValue(tblConfig, "settings", 8, pPlugin->tblSettings, FALSE);
	}
	if ( pPlugin->arrExports ) {
		xvoTableSetValue(tblConfig, "exports", 7, pPlugin->arrExports, FALSE);
	}
	
	bool bResult = xrtStringifyJSON_File(pPlugin->sConfigPath, tblConfig, TRUE);
	xvoUnref(tblConfig);
	return bResult;
}


// TCC 错误回调
void Plugin_TccErrorFunc(void* opaque, const char* msg)
{
	printf("        [Plugin TCC] %s\n", msg);
}


// 为插件代码添加命名空间前缀
str Plugin_AddNamespacePrefix(str sCode, str sPluginName)
{
	// 生成前缀宏定义
	str sPrefix = xrtFormat("_plugin_%s_", sPluginName);
	size_t iPrefixLen = strlen(sPrefix);

	// 估算新代码大小（预留更多空间）
	size_t iCodeLen = strlen(sCode);
	size_t iNewSize = iCodeLen * 3 + 4096;
	str sNewCode = xrtMalloc(iNewSize);
	if ( !sNewCode ) return NULL;

	// 添加宏定义
	sprintf(sNewCode, "#define PLUGIN_NS(name) %s##name\n", sPrefix);
	strcat(sNewCode, "#define PLUGIN_API(name) PLUGIN_NS(API_##name)\n");
	strcat(sNewCode, "#define PLUGIN_FUNC(name) PLUGIN_NS(name)\n");

	// 添加 Plugin_ 函数前缀（自动重命名 Plugin_xxx 为 _plugin_name_Plugin_xxx）
	strcat(sNewCode, "#define Plugin_SetGlobalData PLUGIN_NS(Plugin_SetGlobalData)\n");
	strcat(sNewCode, "#define Plugin_Init PLUGIN_NS(Plugin_Init)\n");
	strcat(sNewCode, "#define Plugin_Unit PLUGIN_NS(Plugin_Unit)\n");

	strcat(sNewCode, "\n");
	strcat(sNewCode, sCode);

	xrtFree(sPrefix);
	return sNewCode;
}


// 使用 TCC 加载插件代码
bool Plugin_TccLoad(PluginInstance* pPlugin)
{
	printf("        [Plugin] Loading plugin: %s\n", pPlugin->sName);

	// 读取代码文件
	str sCode = xrtFileReadAll(pPlugin->sCodePath, XRT_CP_UTF8, NULL);
	if ( !sCode ) {
		printf("        [Plugin] Failed to read code: %s\n", pPlugin->sCodePath);
		return FALSE;
	}

	// 添加命名空间前缀
	str sPrefixedCode = Plugin_AddNamespacePrefix(sCode, pPlugin->sName);
	if ( !sPrefixedCode ) {
		printf("        [Plugin] Failed to add namespace prefix\n");
		xrtFree(sCode);
		return FALSE;
	}

	// 创建 TCC 状态机
	TCCState* pTcc = xsCreateTCC(PluginPath);
	if ( !pTcc ) {
		printf("        [Plugin] Failed to create TCC state\n");
		xrtFree(sCode);
		xrtFree(sPrefixedCode);
		return FALSE;
	}

	// 设置错误回调
	tcc_set_error_func(pTcc, stderr, Plugin_TccErrorFunc);

	// 编译代码（使用带前缀的代码）
	if ( tcc_compile_string(pTcc, sPrefixedCode) < 0 ) {
		printf("        [Plugin] Compile failed\n");
		xsDestroyTCC(pTcc);
		xrtFree(sCode);
		xrtFree(sPrefixedCode);
		return FALSE;
	}
	xrtFree(sCode);
	xrtFree(sPrefixedCode);

	// 地址重定向
	if ( tcc_relocate(pTcc) < 0 ) {
		printf("        [Plugin] Relocate failed\n");
		xsDestroyTCC(pTcc);
		return FALSE;
	}

	// 保存 TCC 状态机
	pPlugin->pTccState = pTcc;

	// 获取并调用全局数据传递函数
	// 旧插件使用 Plugin_SetGlobalData，新插件使用 PLUGIN_NS(Plugin_SetGlobalData)
	str sGlobalDataFuncName = xrtFormat("Plugin_SetGlobalData");
	void (*procSetGlobalData)(int, void*) = tcc_get_symbol(pTcc, sGlobalDataFuncName);
	if ( !procSetGlobalData ) {
		// 尝试新格式
		xrtFree(sGlobalDataFuncName);
		sGlobalDataFuncName = xrtFormat("_plugin_%s_Plugin_SetGlobalData", pPlugin->sName);
		procSetGlobalData = tcc_get_symbol(pTcc, sGlobalDataFuncName);
	}
	if ( procSetGlobalData ) {
		procSetGlobalData(1, G_PluginCtx);
	}
	xrtFree(sGlobalDataFuncName);

	// 获取初始化函数
	// 旧插件使用 Plugin_{name}_Init，新插件使用 PLUGIN_NS(Init)
	str sInitFuncName = xrtFormat("Plugin_%s_Init", pPlugin->sName);
	void (*procInit)() = tcc_get_symbol(pTcc, sInitFuncName);
	if ( !procInit ) {
		// 尝试新格式
		xrtFree(sInitFuncName);
		sInitFuncName = xrtFormat("_plugin_%s_Init", pPlugin->sName);
		procInit = tcc_get_symbol(pTcc, sInitFuncName);
	}
	xrtFree(sInitFuncName);

	if ( !procInit ) {
		printf("        [Plugin] Init function not found\n");
		xsDestroyTCC(pTcc);
		pPlugin->pTccState = NULL;
		return FALSE;
	}

	// 调用初始化函数
	procInit();

	printf("        [Plugin] Plugin loaded: %s\n", pPlugin->sName);
	return TRUE;
}


// 卸载插件 TCC 状态机
bool Plugin_TccUnload(PluginInstance* pPlugin)
{
	printf("        [Plugin] Unloading plugin: %s\n", pPlugin->sName);

	if ( !pPlugin->pTccState ) {
		return TRUE;
	}

	// 获取卸载函数
	// 旧插件使用 Plugin_{name}_Unit，新插件使用 PLUGIN_NS(Unit)
	str sUnitFuncName = xrtFormat("Plugin_%s_Unit", pPlugin->sName);
	void (*procUnit)() = tcc_get_symbol(pPlugin->pTccState, sUnitFuncName);
	if ( !procUnit ) {
		// 尝试新格式
		xrtFree(sUnitFuncName);
		sUnitFuncName = xrtFormat("_plugin_%s_Unit", pPlugin->sName);
		procUnit = tcc_get_symbol(pPlugin->pTccState, sUnitFuncName);
	}
	xrtFree(sUnitFuncName);

	// 调用卸载函数
	if ( procUnit ) {
		procUnit();
	}

	// 释放 TCC 状态机
	xsDestroyTCC(pPlugin->pTccState);
	pPlugin->pTccState = NULL;

	printf("        [Plugin] Plugin unloaded: %s\n", pPlugin->sName);
	return TRUE;
}


// 启用插件
bool Plugin_Enable(PluginInstance* pPlugin)
{
	printf("        [Plugin] Enabling %s...\n", pPlugin->sName);
	
	if ( pPlugin->bLoaded ) {
		printf("        [Plugin] Plugin is already loaded\n");
		return TRUE;
	}
	
	// 使用 TCC 加载插件代码
	if ( !Plugin_TccLoad(pPlugin) ) {
		printf("        [Plugin] Failed to load plugin with TCC\n");
		return FALSE;
	}
	
	// 添加到已加载列表
	int iIdx = xrtListCount(G_PluginMgr->lstLoadedPlugins);
	xrtListSetPtr(G_PluginMgr->lstLoadedPlugins, iIdx, pPlugin, NULL);
	pPlugin->iLoadOrder = iIdx;
	
	// 更新状态
	pPlugin->bLoaded = TRUE;
	pPlugin->bEnabled = TRUE;
	pPlugin->iEnableTime = xrtNow();
	
	// 保存配置
	Plugin_SaveConfig(pPlugin);
	
	printf("        [Plugin] Plugin enabled: %s\n", pPlugin->sName);
	return TRUE;
}


// 禁用插件
bool Plugin_Disable(PluginInstance* pPlugin)
{
	printf("        [Plugin] Disabling %s...\n", pPlugin->sName);
	
	if ( !pPlugin->bLoaded ) {
		printf("        [Plugin] Plugin is not loaded\n");
		return TRUE;
	}
	
	// 使用 TCC 卸载插件
	Plugin_TccUnload(pPlugin);
	
	// 从已加载列表移除
	int iCount = xrtListCount(G_PluginMgr->lstLoadedPlugins);
	for ( int i = 0; i < iCount; i++ ) {
		if ( xrtListGetPtr(G_PluginMgr->lstLoadedPlugins, i) == pPlugin ) {
			xrtListRemove(G_PluginMgr->lstLoadedPlugins, i);
			break;
		}
	}
	
	// 更新状态
	pPlugin->bLoaded = FALSE;
	pPlugin->bEnabled = FALSE;
	
	// 保存配置
	Plugin_SaveConfig(pPlugin);
	
	printf("        [Plugin] Plugin disabled: %s\n", pPlugin->sName);
	return TRUE;
}



// ==================== 插件管理器初始化 ====================

// 初始化插件上下文
void PluginCtx_Init()
{
	G_PluginCtx = xrtMalloc(sizeof(PluginContext));
	memset(G_PluginCtx, 0, sizeof(PluginContext));
	
	// 核心数据
	G_PluginCtx->pDB = G_DB;
	G_PluginCtx->pAdminSession = &G_AdminSession;
	G_PluginCtx->pMemberSession = &G_MemberSession;
	G_PluginCtx->pOption = &G_Option;
	
	// 路径信息
	G_PluginCtx->sAppPath = AppPath;
	G_PluginCtx->sWebPath = WebPath;
	G_PluginCtx->sDataPath = xrtPathJoin(2, AppPath, "data");
	G_PluginCtx->sPluginPath = PluginPath;
	G_PluginCtx->sPagePath = PagePath;
	
	// 路由操作
	G_PluginCtx->AddRoute = PluginCtx_AddRoute;
	G_PluginCtx->RemoveRoute = PluginCtx_RemoveRoute;
	G_PluginCtx->GetRoute = PluginCtx_GetRoute;
	
	// 菜单操作
	G_PluginCtx->AddMenu = PluginCtx_AddMenu;
	G_PluginCtx->RemoveMenu = PluginCtx_RemoveMenu;
	G_PluginCtx->ShowMenu = PluginCtx_ShowMenu;
	G_PluginCtx->HideMenu = PluginCtx_HideMenu;
	
	// 权限操作
	G_PluginCtx->AddAuthGroup = PluginCtx_AddAuthGroup;
	G_PluginCtx->AddAuth = PluginCtx_AddAuth;
	G_PluginCtx->RemoveAuthGroup = PluginCtx_RemoveAuthGroup;
	G_PluginCtx->RemoveAuth = PluginCtx_RemoveAuth;
	G_PluginCtx->SyncUriAuth = PluginCtx_SyncUriAuth;
	G_PluginCtx->ReloadAuthCache = PluginCtx_ReloadAuthCache;
	
	// Session 操作
	G_PluginCtx->GetAdminSession = PluginCtx_GetAdminSession;
	G_PluginCtx->GetMemberSession = PluginCtx_GetMemberSession;
	G_PluginCtx->CreateAdminSession = PluginCtx_CreateAdminSession;
	G_PluginCtx->CreateMemberSession = PluginCtx_CreateMemberSession;
	G_PluginCtx->DestroyAdminSession = PluginCtx_DestroyAdminSession;
	G_PluginCtx->DestroyMemberSession = PluginCtx_DestroyMemberSession;
	G_PluginCtx->ExtendSession = PluginCtx_ExtendSession;
	
	// HTTP 响应
	G_PluginCtx->SendJson = PluginCtx_SendJson;
	G_PluginCtx->SendHtml = PluginCtx_SendHtml;
	G_PluginCtx->SendPage = PluginCtx_SendPage;
	G_PluginCtx->SendFile = PluginCtx_SendFile;
	G_PluginCtx->SendError = PluginCtx_SendError;
	
	// 配置操作
	G_PluginCtx->GetOption = PluginCtx_GetOption;
	G_PluginCtx->SetOption = PluginCtx_SetOption;
	G_PluginCtx->ReloadOption = PluginCtx_ReloadOption;
	
	// JSON 操作
	G_PluginCtx->JsonParse = PluginCtx_JsonParse;
	G_PluginCtx->JsonStringify = PluginCtx_JsonStringify;
	G_PluginCtx->JsonFree = PluginCtx_JsonFree;
	
	// 工具函数
	G_PluginCtx->TimeNow = PluginCtx_TimeNow;
	G_PluginCtx->Format = PluginCtx_Format;
	G_PluginCtx->Free = PluginCtx_Free;
	G_PluginCtx->HashPassword = PluginCtx_HashPassword;
	G_PluginCtx->GenerateSalt = PluginCtx_GenerateSalt;
	G_PluginCtx->GenerateToken = PluginCtx_GenerateToken;
	
	// 日志
	G_PluginCtx->Log = PluginCtx_Log;
	G_PluginCtx->LogAccess = PluginCtx_LogAccess;
	
	// 插件间通信
	G_PluginCtx->GetPluginExport = PluginCtx_GetPluginExport;
	G_PluginCtx->SetPluginExport = PluginCtx_SetPluginExport;
	
	// 事件系统
	G_PluginCtx->EmitEvent = PluginCtx_EmitEvent;
	G_PluginCtx->OnEvent = PluginCtx_OnEvent;
	G_PluginCtx->OffEvent = PluginCtx_OffEvent;
}


// 扫描插件目录的回调函数
int PluginMgr_ScanDirProc(str sPath, size_t iSize, int bDir, ptr pData, size_t iPathSize)
{
	// 只处理目录（进入时，bDir=1），跳过文件(0)和离开目录(2)
	if ( bDir != 1 ) return FALSE;
	
	// 获取目录名
	str sName = xrtPathGetName(sPath, 0);
	if ( !sName ) return FALSE;
	
	// 跳过隐藏目录和特殊目录
	if ( sName[0] == '.' || sName[0] == '_' ) {
		xrtFree(sName);
		return FALSE;
	}
	
	// 跳过 plugin.h 等公共文件
	if ( strcmp(sName, "plugin.h") == 0 ) {
		xrtFree(sName);
		return FALSE;
	}
	
	// 检查是否有配置文件
	str sConfigPath = xrtFormat("%s/config.json", sPath);
	if ( !xrtFileExists(sConfigPath) ) {
		xrtFree(sConfigPath);
		xrtFree(sName);
		return FALSE;
	}
	
	// 检查是否有代码文件
	str sCodePath = xrtFormat("%s/main.c", sPath);
	if ( !xrtFileExists(sCodePath) ) {
		xrtFree(sConfigPath);
		xrtFree(sCodePath);
		xrtFree(sName);
		return FALSE;
	}
	
	printf("        [Plugin] Found plugin: %s\n", sName);
	
	// 创建插件实例
	PluginInstance* pPlugin = Plugin_Create(sName);
	pPlugin->sPath = xrtCopyStr(sPath, 0);
	pPlugin->sConfigPath = sConfigPath;
	pPlugin->sCodePath = sCodePath;
	pPlugin->sDataPath = xrtPathJoin(2, PluginDataPath, sName);
	
	// 确保数据目录存在
	xrtDirCreate(pPlugin->sDataPath);
	
	// 加载配置
	if ( !Plugin_LoadConfig(pPlugin) ) {
		Plugin_Destroy(pPlugin);
		xrtFree(sName);
		return FALSE;
	}
	
	// 添加到插件表
	xrtDictSet(G_PluginMgr->tblPlugins, sName, strlen(sName), NULL);
	PluginInstance** ppPlugin = xrtDictGet(G_PluginMgr->tblPlugins, sName, strlen(sName));
	*ppPlugin = pPlugin;
	
	xrtFree(sName);
	return FALSE;  // 继续遍历
}

// 扫描并加载所有插件
void PluginMgr_ScanPlugins()
{
	printf("        [Plugin] Scanning plugins in: %s\n", PluginPath);
	xrtDirScan(PluginPath, FALSE, PluginMgr_ScanDirProc, NULL);
}


// 收集已启用插件的回调函数
bool PluginMgr_CollectEnabledProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	xlist lstEnabled = (xlist)pArg;
	PluginInstance** ppPlugin = (PluginInstance**)pVal;
	if ( !ppPlugin || !(*ppPlugin) ) return FALSE;
	
	PluginInstance* pPlugin = *ppPlugin;
	if ( pPlugin->bEnabled ) {
		int iIdx = xrtListCount(lstEnabled);
		xrtListSetPtr(lstEnabled, iIdx, pPlugin, NULL);
	}
	return FALSE;  // 继续遍历
}

// 按依赖关系加载已启用的插件
void PluginMgr_LoadEnabledPlugins()
{
	printf("        [Plugin] Loading enabled plugins...\n");

	// 收集所有已启用的插件
	xlist lstEnabled = xrtListCreate(sizeof(ptr));
	xrtDictWalk(G_PluginMgr->tblPlugins, PluginMgr_CollectEnabledProc, lstEnabled);

	// 构建反向依赖图
	// Plugin_BuildDependentsGraph();  // 暂时禁用

	// 验证依赖关系
	// if ( !Plugin_ValidateDependencies() ) {  // 暂时禁用
	// 	printf("        [Plugin] ERROR: Dependency validation failed!\n");
	// 	xrtListDestroy(lstEnabled);
	// 	return;
	// }
	printf("        [Plugin] Dependency validation temporarily disabled\n");

	// 使用拓扑排序确定加载顺序
	// xlist loadOrder = xrtListCreate(sizeof(ptr));  // 暂时禁用
	// if ( !Plugin_TopologicalSort(&loadOrder) ) {  // 暂时禁用
	// 	printf("        [Plugin] ERROR: Circular dependency detected!\n");
	// 	xrtListDestroy(lstEnabled);
	// 	xrtListDestroy(loadOrder);
	// 	return;
	// }

	// 按拓扑顺序加载
	// int iLoadCount = xrtListCount(loadOrder);  // 暂时禁用
	// for ( int i = 0; i < iLoadCount; i++ ) {  // 暂时禁用
	// 	PluginInstance* pPlugin = xrtListGetPtr(loadOrder, i);
	// 	if ( pPlugin && pPlugin->bEnabled ) {
	// 		Plugin_Enable(pPlugin);
	// 	}
	// }

	// 暂时使用原来的简单加载
	int iLoadCount = xrtListCount(lstEnabled);
	for ( int i = 0; i < iLoadCount; i++ ) {
		PluginInstance* pPlugin = xrtListGetPtr(lstEnabled, i);
		if ( pPlugin ) {
			Plugin_Enable(pPlugin);
		}
	}

	xrtListDestroy(lstEnabled);
	// xrtListDestroy(loadOrder);  // 暂时禁用
}


// 初始化插件管理器
// 检查并创建插件管理菜单（对于已安装的系统）
void PluginMgr_EnsureMenu()
{
	// 检查插件管理菜单是否存在
	str sSQL = "SELECT COUNT(*) FROM menu WHERE href = '/admin/view/plugin' AND isDelete = 0";
	sqlite3_stmt* stmt;
	int iRet = sqlite3_prepare_v3(G_DB->objDB, sSQL, -1, 0, &stmt, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! PluginMgr_EnsureMenu prepare error\n");
		return;
	}
	
	int iCount = 0;
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iCount = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	
	if ( iCount > 0 ) {
		printf("        [Plugin] Menu already exists\n");
		return;
	}
	
	// 查找设置菜单的ID（parent）
	sSQL = "SELECT id FROM menu WHERE title = '设置' AND parent = 0 AND isDelete = 0";
	iRet = sqlite3_prepare_v3(G_DB->objDB, sSQL, -1, 0, &stmt, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! PluginMgr_EnsureMenu find parent error\n");
		return;
	}
	
	int iParentId = 0;
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iParentId = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	
	if ( iParentId == 0 ) {
		printf("        [Plugin] Setting menu not found, skip menu creation\n");
		return;
	}
	
	// 创建插件管理菜单
	int64 iNow = xrtNow();
	sqlite3_bind_int(stmt_menu_add, 1, iParentId);
	sqlite3_bind_text(stmt_menu_add, 2, "插件管理", -1, NULL);
	sqlite3_bind_text(stmt_menu_add, 3, "layui-icon layui-icon-app", -1, NULL);
	sqlite3_bind_int(stmt_menu_add, 4, 1);
	sqlite3_bind_text(stmt_menu_add, 5, "_component", -1, NULL);
	sqlite3_bind_text(stmt_menu_add, 6, "/admin/view/plugin", -1, NULL);
	sqlite3_bind_int(stmt_menu_add, 7, 500600);
	sqlite3_bind_int(stmt_menu_add, 8, 1);
	sqlite3_bind_text(stmt_menu_add, 9, "管理系统插件", -1, NULL);
	sqlite3_bind_int64(stmt_menu_add, 10, iNow);
	sqlite3_bind_int64(stmt_menu_add, 11, iNow);
	sqlite3_step(stmt_menu_add);
	int iMenuId = sqlite3_last_insert_rowid(G_DB->objDB);
	sqlite3_reset(stmt_menu_add);
	printf("        [Plugin] Menu created: 插件管理 (id=%d)\n", iMenuId);
}



void PluginMgr_Init()
{
	printf("        PluginMgr_Init \n");
	
	// 初始化路径
	PluginPath = xrtPathJoin(2, AppPath, "script/plugin");
	PluginDataPath = xrtPathJoin(2, AppPath, "data/plugin");
	
	// 创建目录
	xrtDirCreate(PluginPath);
	xrtDirCreate(PluginDataPath);
	
	// 创建插件管理器
	G_PluginMgr = xrtMalloc(sizeof(PluginManager));
	memset(G_PluginMgr, 0, sizeof(PluginManager));
	G_PluginMgr->tblPlugins = xrtDictCreate(sizeof(ptr));
	G_PluginMgr->lstLoadedPlugins = xrtListCreate(sizeof(ptr));
	G_PluginMgr->lstEventListeners = xrtListCreate(sizeof(ptr));
	G_PluginMgr->tblExports = xrtDictCreate(sizeof(PluginExport));
	
	// 初始化插件上下文
	PluginCtx_Init();
	
	// 检查并创建插件管理菜单
	PluginMgr_EnsureMenu();
	
	// 扫描插件
	PluginMgr_ScanPlugins();
	
	// 加载已启用的插件
	PluginMgr_LoadEnabledPlugins();
	
	// 触发系统就绪事件
	PluginCtx_EmitEvent(EVENT_SYSTEM_READY, NULL);
}


// 销毁插件实例的回调函数
bool PluginMgr_DestroyWalkProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	PluginInstance** ppPlugin = (PluginInstance**)pVal;
	if ( ppPlugin && *ppPlugin ) {
		Plugin_Destroy(*ppPlugin);
	}
	return FALSE;  // 继续遍历
}

// 卸载插件管理器
void PluginMgr_Unit()
{
	printf("        PluginMgr_Unit \n");
	
	// 触发系统关闭事件
	PluginCtx_EmitEvent(EVENT_SYSTEM_SHUTDOWN, NULL);
	
	// 按加载顺序的逆序卸载插件
	int iCount = xrtListCount(G_PluginMgr->lstLoadedPlugins);
	for ( int i = iCount - 1; i >= 0; i-- ) {
		PluginInstance* pPlugin = xrtListGetPtr(G_PluginMgr->lstLoadedPlugins, i);
		if ( pPlugin ) {
			Plugin_TccUnload(pPlugin);
		}
	}
	
	// 销毁所有插件实例
	xrtDictWalk(G_PluginMgr->tblPlugins, PluginMgr_DestroyWalkProc, NULL);
	
	// 释放事件监听器
	int iListenerCount = xrtListCount(G_PluginMgr->lstEventListeners);
	for ( int i = 0; i < iListenerCount; i++ ) {
		EventListener* pListener = xrtListGetPtr(G_PluginMgr->lstEventListeners, i);
		if ( pListener ) {
			if ( pListener->sEventName ) xrtFree(pListener->sEventName);
			if ( pListener->lstCallbacks ) xrtListDestroy(pListener->lstCallbacks);
			xrtFree(pListener);
		}
	}
	
	// 释放插件管理器
	xrtDictDestroy(G_PluginMgr->tblPlugins);
	xrtListDestroy(G_PluginMgr->lstLoadedPlugins);
	xrtListDestroy(G_PluginMgr->lstEventListeners);
	xrtDictDestroy(G_PluginMgr->tblExports);
	xrtFree(G_PluginMgr);
	G_PluginMgr = NULL;
	
	// 释放上下文
	if ( G_PluginCtx ) {
		if ( G_PluginCtx->sDataPath ) xrtFree(G_PluginCtx->sDataPath);
		xrtFree(G_PluginCtx);
		G_PluginCtx = NULL;
	}
	
	// 释放路径
	if ( PluginPath ) xrtFree(PluginPath);
	if ( PluginDataPath ) xrtFree(PluginDataPath);
}



// ==================== 插件管理 API ====================

// 收集插件列表的回调函数
bool PluginMgr_ListWalkProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	xvalue arrList = (xvalue)pArg;
	PluginInstance** ppPlugin = (PluginInstance**)pVal;
	if ( !ppPlugin || !(*ppPlugin) ) return FALSE;
	
	PluginInstance* pPlugin = *ppPlugin;
	
	xvalue tblPlugin = xvoCreateTable();
	xvoTableSetText(tblPlugin, "name", 4, pPlugin->sName, 0, FALSE);
	xvoTableSetText(tblPlugin, "title", 5, pPlugin->sTitle, 0, FALSE);
	xvoTableSetText(tblPlugin, "desc", 4, pPlugin->sDesc, 0, FALSE);
	xvoTableSetText(tblPlugin, "version", 7, pPlugin->sVersion, 0, FALSE);
	xvoTableSetText(tblPlugin, "author", 6, pPlugin->sAuthor, 0, FALSE);
	xvoTableSetInt(tblPlugin, "sort", 4, pPlugin->iSort);
	xvoTableSetBool(tblPlugin, "enabled", 7, pPlugin->bEnabled);
	xvoTableSetBool(tblPlugin, "loaded", 6, pPlugin->bLoaded);
	
	xvoArrayAppendValue(arrList, tblPlugin, TRUE);
	return FALSE;  // 继续遍历
}

// 获取插件列表
xvalue PluginMgr_GetList()
{
	xvalue arrList = xvoCreateArray();
	xrtDictWalk(G_PluginMgr->tblPlugins, PluginMgr_ListWalkProc, arrList);
	return arrList;
}


// 根据名称获取插件
PluginInstance* PluginMgr_GetPlugin(str sName)
{
	PluginInstance** ppPlugin = xrtDictGet(G_PluginMgr->tblPlugins, sName, strlen(sName));
	return ppPlugin ? *ppPlugin : NULL;
}


// 启用指定插件
bool PluginMgr_EnablePlugin(str sName)
{
	PluginInstance* pPlugin = PluginMgr_GetPlugin(sName);
	if ( !pPlugin ) {
		return FALSE;
	}
	return Plugin_Enable(pPlugin);
}


// 禁用指定插件
bool PluginMgr_DisablePlugin(str sName)
{
	PluginInstance* pPlugin = PluginMgr_GetPlugin(sName);
	if ( !pPlugin ) {
		return FALSE;
	}
	return Plugin_Disable(pPlugin);
}


// 重载指定插件
bool PluginMgr_ReloadPlugin(str sName)
{
	PluginInstance* pPlugin = PluginMgr_GetPlugin(sName);
	if ( !pPlugin ) {
		return FALSE;
	}

	if ( pPlugin->bLoaded ) {
		Plugin_Disable(pPlugin);
	}
	return Plugin_Enable(pPlugin);
}



// ==================== 依赖管理功能 ====================

// 版本比较函数
// 返回值: -1(v1 < v2), 0(v1 == v2), 1(v1 > v2)
int Plugin_CompareVersion(str v1, str v2)
{
	if ( !v1 || !v2 ) return 0;

	int major1 = 0, minor1 = 0, patch1 = 0;
	int major2 = 0, minor2 = 0, patch2 = 0;

	// 安全解析，避免 sscanf 崩溃
	sscanf(v1, "%d.%d.%d", &major1, &minor1, &patch1);
	sscanf(v2, "%d.%d.%d", &major2, &minor2, &patch2);

	if ( major1 != major2 ) return major1 < major2 ? -1 : 1;
	if ( minor1 != minor2 ) return minor1 < minor2 ? -1 : 1;
	if ( patch1 != patch2 ) return patch1 < patch2 ? -1 : 1;

	return 0;
}

// 检查版本是否满足要求
bool Plugin_CheckVersionRequirement(str actualVersion, str minVersion, str maxVersion)
{
	if ( !actualVersion ) return FALSE;

	// 检查最小版本
	if ( minVersion && strlen(minVersion) > 0 ) {
		if ( Plugin_CompareVersion(actualVersion, minVersion) < 0 ) {
			printf("        [Plugin] Version check failed: %s < %s\n", actualVersion, minVersion);
			return FALSE;
		}
	}

	// 检查最大版本
	if ( maxVersion && strlen(maxVersion) > 0 ) {
		if ( Plugin_CompareVersion(actualVersion, maxVersion) > 0 ) {
			printf("        [Plugin] Version check failed: %s > %s\n", actualVersion, maxVersion);
			return FALSE;
		}
	}

	return TRUE;
}

// 构建反向依赖关系的回调函数
bool Plugin_BuildDependentsWalkProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	PluginInstance** ppPlugin = (PluginInstance**)pVal;
	if ( !ppPlugin || !(*ppPlugin) ) return FALSE;

	PluginInstance* pPlugin = *ppPlugin;

	// 遍历该插件的依赖列表
	int iDepCount = xrtListCount(pPlugin->lstDependencies);
	for ( int j = 0; j < iDepCount; j++ ) {
		PluginDependency* pDep = xrtListGetPtr(pPlugin->lstDependencies, j);

		// 找到被依赖的插件
		PluginInstance* pDepPlugin = PluginMgr_GetPlugin(pDep->sPluginName);
		if ( pDepPlugin ) {
			// 在被依赖插件的 dependents 列表中添加当前插件
			int iDependentIdx = xrtListCount(pDepPlugin->lstDependents);
			xrtListSetPtr(pDepPlugin->lstDependents, iDependentIdx, pPlugin, NULL);
		}
	}

	return FALSE;
}

// 构建反向依赖关系
void Plugin_BuildDependentsGraph()
{
	// 遍历所有插件，构建反向依赖
	xrtDictWalk(G_PluginMgr->tblPlugins, Plugin_BuildDependentsWalkProc, NULL);
}

// 拓扑排序（Kahn算法）
// 初始化入度的回调函数
bool Plugin_TopologicalSortInitProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	xdict inDegree = (xdict)pArg;
	PluginInstance** ppPlugin = (PluginInstance**)pVal;
	if ( !ppPlugin || !(*ppPlugin) ) return FALSE;

	PluginInstance* pPlugin = *ppPlugin;

	// 计算入度（依赖数量）
	int iInDegree = xrtListCount(pPlugin->lstDependencies);
	int* pDegree = xrtDictSet(inDegree, pPlugin->sName, strlen(pPlugin->sName), NULL);
	if ( pDegree ) {
		*pDegree = iInDegree;
	}

	return FALSE;
}

// 将入度为0的节点加入队列的回调函数
typedef struct {
	xlist queue;
	xdict inDegree;
} TopologicalSortContext;

bool Plugin_TopologicalSortEnqueueProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	TopologicalSortContext* pCtx = (TopologicalSortContext*)pArg;
	PluginInstance** ppPlugin = (PluginInstance**)pVal;
	if ( !ppPlugin || !(*ppPlugin) ) return FALSE;

	PluginInstance* pPlugin = *ppPlugin;
	int* pDegree = xrtDictGet(pCtx->inDegree, pPlugin->sName, strlen(pPlugin->sName));
	if ( pDegree && *pDegree == 0 ) {
		xrtListSetPtr(pCtx->queue, xrtListCount(pCtx->queue), pPlugin, NULL);
	}

	return FALSE;
}

// 统计插件数量的回调函数
typedef struct {
	int* piCount;
} PluginCountContext;

bool Plugin_CountPluginsProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	PluginCountContext* pCtx = (PluginCountContext*)pArg;
	(*pCtx->piCount)++;
	return FALSE;
}

// 拓扑排序（Kahn算法）
// 返回值: TRUE=成功, FALSE=失败（循环依赖）
bool Plugin_TopologicalSort(xlist* pResult)
{
	// 创建入度表
	xdict inDegree = xrtDictCreate(sizeof(int));

	// 初始化入度
	xrtDictWalk(G_PluginMgr->tblPlugins, Plugin_TopologicalSortInitProc, inDegree);

	// 创建队列
	xlist queue = xrtListCreate(sizeof(ptr));

	// 统计插件总数
	int iCount = 0;
	PluginCountContext countCtx = {&iCount};
	xrtDictWalk(G_PluginMgr->tblPlugins, Plugin_CountPluginsProc, &countCtx);

	// 将入度为0的节点加入队列
	TopologicalSortContext ctx = {queue, inDegree};
	xrtDictWalk(G_PluginMgr->tblPlugins, Plugin_TopologicalSortEnqueueProc, &ctx);

	// 拓扑排序
	while ( xrtListCount(queue) > 0 ) {
		// 取出队首
		PluginInstance* pPlugin = xrtListGetPtr(queue, 0);
		xrtListRemove(queue, 0);

		// 加入结果
		xrtListSetPtr(*pResult, xrtListCount(*pResult), pPlugin, NULL);

		// 减少依赖此节点的节点的入度
		int iDependentCount = xrtListCount(pPlugin->lstDependents);
		for ( int i = 0; i < iDependentCount; i++ ) {
			PluginInstance* pDependent = xrtListGetPtr(pPlugin->lstDependents, i);
			if ( pDependent ) {
				int* pDegree = xrtDictGet(inDegree, pDependent->sName, strlen(pDependent->sName));
				if ( pDegree ) {
					(*pDegree)--;
					if ( *pDegree == 0 ) {
						xrtListSetPtr(queue, xrtListCount(queue), pDependent, NULL);
					}
				}
			}
		}
	}

	// 检查是否有环
	int iSortedCount = xrtListCount(*pResult);
	bool bHasCycle = (iSortedCount != iCount);

	// 清理
	xrtDictDestroy(inDegree);
	xrtListDestroy(queue);

	return !bHasCycle;
}

// 验证依赖关系的回调函数
typedef struct {
	bool* pbValid;
} ValidateContext;

bool Plugin_ValidateWalkProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	ValidateContext* pCtx = (ValidateContext*)pArg;
	PluginInstance** ppPlugin = (PluginInstance**)pVal;
	if ( !ppPlugin || !(*ppPlugin) ) return FALSE;

	PluginInstance* pPlugin = *ppPlugin;

	// 检查插件版本是否有效
	if ( !pPlugin->sVersion || strlen(pPlugin->sVersion) == 0 ) {
		// 没有版本号，跳过版本检查
		return FALSE;
	}

	// 检查每个依赖
	int iDepCount = xrtListCount(pPlugin->lstDependencies);
	for ( int j = 0; j < iDepCount; j++ ) {
		PluginDependency* pDep = xrtListGetPtr(pPlugin->lstDependencies, j);

		// 检查依赖配置是否有效
		if ( !pDep || !pDep->sPluginName || strlen(pDep->sPluginName) == 0 ) {
			continue;
		}

		// 查找依赖的插件是否存在
		PluginInstance* pDepPlugin = PluginMgr_GetPlugin(pDep->sPluginName);
		if ( !pDepPlugin ) {
			printf("        [Plugin] ERROR: Plugin '%s' depends on missing plugin '%s'\n",
			       pPlugin->sName, pDep->sPluginName);
			*(pCtx->pbValid) = FALSE;
			// 不返回 FALSE，继续检查其他依赖
			continue;
		}

		// 检查依赖插件版本是否有效
		if ( !pDepPlugin->sVersion || strlen(pDepPlugin->sVersion) == 0 ) {
			// 依赖插件没有版本号，跳过版本检查
			continue;
		}

		// 检查版本是否满足
		if ( !Plugin_CheckVersionRequirement(pDepPlugin->sVersion,
		                                      pDep->sMinVersion,
		                                      pDep->sMaxVersion) ) {
			printf("        [Plugin] ERROR: Plugin '%s' version %s does not meet requirements of '%s'\n",
			       pDep->sPluginName, pDepPlugin->sVersion, pPlugin->sName);
			*(pCtx->pbValid) = FALSE;
			// 不返回 FALSE，继续检查其他依赖
		}
	}

	return FALSE;
}

// 验证依赖关系
bool Plugin_ValidateDependencies()
{
	printf("        [Plugin] Validating dependencies...\n");

	bool bValid = TRUE;
	ValidateContext ctx = {&bValid};

	xrtDictWalk(G_PluginMgr->tblPlugins, Plugin_ValidateWalkProc, &ctx);

	return bValid;
}


#endif // PLUGIN_MGR_H


