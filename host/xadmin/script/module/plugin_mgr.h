


// ============================================
// 插件管理器
// ============================================


// 前向声明：模板相关全局变量
extern xvalue tblENV;
extern xdict G_Template;


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

// 当前正在处理的插件
str G_CurrentPluginId = NULL;
PluginInstance* G_CurrentPlugin = NULL;



// ==================== 前向声明 ====================

void PluginCtx_Log(int level, str format, ...);
bool Plugin_Enable(PluginInstance* pPlugin);
bool Plugin_Disable(PluginInstance* pPlugin);
void Plugin_CleanupResources(str pluginId);

str PluginCtx_GetPluginId();
str PluginCtx_GetPluginName();
str PluginCtx_GetPluginPath();

bool PluginCtx_WriteFile(str filePath, str content, size_t len);
bool PluginCtx_ReadFile(str filePath, str* outContent, size_t* outLen);
bool PluginCtx_DeleteFile(str filePath);
bool PluginCtx_FileExists(str filePath);
bool PluginCtx_CreateDir(str dirPath);
bool PluginCtx_DeleteDir(str dirPath, bool bRecursive);
bool PluginCtx_DirExists(str dirPath);
bool PluginCtx_ScanDir(str dirPath, bool bRecursive, DirScanCallback callback, ptr userData);
bool PluginCtx_CopyFile(str srcPath, str destPath);
bool PluginCtx_MoveFile(str srcPath, str destPath);

int PluginCtx_CreateXpkg(str outputPath, str* fileList, int fileCount, int compressLevel);
int PluginCtx_ExtractXpkg(str xpkgPath, str outputDir);
xvalue PluginCtx_GetXpkgInfo(str xpkgPath);

bool PluginCtx_GenerateModel(str modelName, xvalue modelConfig);
bool PluginCtx_CompilePlugin(str pluginName);
bool PluginCtx_ReloadPlugin(str pluginName);
xvalue PluginCtx_GetPluginConfig(str pluginName);
bool PluginCtx_SetPluginConfig(str pluginName, xvalue config);

bool PluginCtx_CreateTable(str tableName, str sql);
bool PluginCtx_DropTable(str tableName);
bool PluginCtx_ExecuteSQL(str sql);
xvalue PluginCtx_QuerySQL(str sql);
sqlite3_stmt* PluginCtx_PrepareSQL(str sql);
bool PluginCtx_ExecuteStmt(sqlite3_stmt* stmt);
void PluginCtx_FinalizeStmt(sqlite3_stmt* stmt);

bool PluginCtx_InstallPlugin(str xpkgPath);
bool PluginCtx_UninstallPlugin(str pluginName);
bool PluginCtx_UpgradePlugin(str pluginName, str newXpkgPath);

str PluginCtx_RenderTemplate(str templatePath, xvalue data);
str PluginCtx_RenderString(str templateString, xvalue data);



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

		if ( G_CurrentPluginId ) {
			int64 iNow = xrtNow();
			str sCheckSQL = xrtFormat("SELECT id FROM uris WHERE uri = '%s'", uri);
			sqlite3_stmt* stmtCheck;
			sqlite3_prepare_v3(G_DB->objDB, sCheckSQL, -1, 0, &stmtCheck, NULL);
			xrtFree(sCheckSQL);

			if ( sqlite3_step(stmtCheck) == SQLITE_ROW ) {
				int iId = sqlite3_column_int(stmtCheck, 0);
				sqlite3_finalize(stmtCheck);
				str sUpdateSQL = xrtFormat(
					"UPDATE uris SET plugin_id = '%s', updateTime = %lld WHERE id = %d",
					G_CurrentPluginId, iNow, iId
				);
				sqlite3_exec(G_DB->objDB, sUpdateSQL, NULL, NULL, NULL);
				xrtFree(sUpdateSQL);
			} else {
				sqlite3_finalize(stmtCheck);
				str sInsertSQL = xrtFormat(
					"INSERT INTO uris (authID, uri, desc, isBackend, needAuth, needLog, putLog, putData, createTime, updateTime, plugin_id) "
					"VALUES (%d, '%s', '', %d, %d, %d, 0, 0, %lld, %lld, '%s')",
					authId, uri, bAdmin ? 1 : 0, bAuth ? 1 : 0, 0, iNow, iNow, G_CurrentPluginId
				);
				sqlite3_exec(G_DB->objDB, sInsertSQL, NULL, NULL, NULL);
				xrtFree(sInsertSQL);
			}
			printf("        [Plugin] Route added: %s (plugin_id=%s)\n", uri, G_CurrentPluginId);
		} else {
			printf("        [Plugin] Route added: %s\n", uri);
		}
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

	if ( G_CurrentPluginId ) {
		sqlite3_bind_text(stmt_menu_add, 12, G_CurrentPluginId, -1, NULL);
	} else {
		sqlite3_bind_null(stmt_menu_add, 12);
	}

	sqlite3_step(stmt_menu_add);
	int iMenuId = sqlite3_last_insert_rowid(G_DB->objDB);
	sqlite3_reset(stmt_menu_add);
	printf("        [Plugin] Menu added: %s (id=%d, plugin_id=%s)\n", title, iMenuId, G_CurrentPluginId ? G_CurrentPluginId : "NULL");
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
	
	// 创建 TCC 状态机
	TCCState* pTcc = xsCreateTCC(PluginPath);
	if ( !pTcc ) {
		printf("        [Plugin] Failed to create TCC state\n");
		xrtFree(sCode);
		return FALSE;
	}
	
	// 设置错误回调
	tcc_set_error_func(pTcc, stderr, Plugin_TccErrorFunc);
	
	// 编译代码
	if ( tcc_compile_string(pTcc, sCode) < 0 ) {
		printf("        [Plugin] Compile failed\n");
		xsDestroyTCC(pTcc);
		xrtFree(sCode);
		return FALSE;
	}
	xrtFree(sCode);
	
	// 地址重定向
	if ( tcc_relocate(pTcc) < 0 ) {
		printf("        [Plugin] Relocate failed\n");
		xsDestroyTCC(pTcc);
		return FALSE;
	}
	
	// 保存 TCC 状态机
	pPlugin->pTccState = pTcc;
	
	// 获取并调用全局数据传递函数
	void (*procSetGlobalData)(int, void*) = tcc_get_symbol(pTcc, "Plugin_SetGlobalData");
	if ( procSetGlobalData ) {
		procSetGlobalData(1, G_PluginCtx);
	}
	
	// 获取初始化函数
	str sInitFuncName = xrtFormat("Plugin_%s_Init", pPlugin->sName);
	void (*procInit)() = tcc_get_symbol(pTcc, sInitFuncName);
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
	str sUnitFuncName = xrtFormat("Plugin_%s_Unit", pPlugin->sName);
	void (*procUnit)() = tcc_get_symbol(pPlugin->pTccState, sUnitFuncName);
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

	G_CurrentPlugin = pPlugin;
	G_CurrentPluginId = pPlugin->sName;

	if ( !Plugin_TccLoad(pPlugin) ) {
		printf("        [Plugin] Failed to load plugin with TCC\n");
		G_CurrentPlugin = NULL;
		G_CurrentPluginId = NULL;
		return FALSE;
	}

	int iIdx = xrtListCount(G_PluginMgr->lstLoadedPlugins);
	xrtListSetPtr(G_PluginMgr->lstLoadedPlugins, iIdx, pPlugin, NULL);
	pPlugin->iLoadOrder = iIdx;

	pPlugin->bLoaded = TRUE;
	pPlugin->bEnabled = TRUE;
	pPlugin->iEnableTime = xrtNow();

	Plugin_SaveConfig(pPlugin);

	G_CurrentPlugin = NULL;
	G_CurrentPluginId = NULL;

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

	G_CurrentPlugin = pPlugin;
	G_CurrentPluginId = pPlugin->sName;

	Plugin_TccUnload(pPlugin);

	Plugin_CleanupResources(pPlugin->sName);

	int iCount = xrtListCount(G_PluginMgr->lstLoadedPlugins);
	for ( int i = 0; i < iCount; i++ ) {
		if ( xrtListGetPtr(G_PluginMgr->lstLoadedPlugins, i) == pPlugin ) {
			xrtListRemove(G_PluginMgr->lstLoadedPlugins, i);
			break;
		}
	}

	pPlugin->bLoaded = FALSE;
	pPlugin->bEnabled = FALSE;

	Plugin_SaveConfig(pPlugin);

	G_CurrentPlugin = NULL;
	G_CurrentPluginId = NULL;

	printf("        [Plugin] Plugin disabled: %s\n", pPlugin->sName);
	return TRUE;
}


// 清理插件关联的资源
void Plugin_CleanupResources(str pluginId)
{
	if ( !pluginId ) return;

	printf("        [Plugin] Cleaning up resources for plugin_id: %s\n", pluginId);

	str sSQL = xrtFormat(
		"UPDATE menu SET isDelete = 1, updateTime = %lld WHERE plugin_id = '%s'",
		xrtNow(), pluginId
	);
	sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, NULL);
	xrtFree(sSQL);

	sSQL = xrtFormat("DELETE FROM uris WHERE plugin_id = '%s'", pluginId);
	sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, NULL);
	xrtFree(sSQL);

	sSQL = xrtFormat("UPDATE authGroup SET isDelete = 1 WHERE plugin_id = '%s'", pluginId);
	sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, NULL);
	xrtFree(sSQL);

	sSQL = xrtFormat("UPDATE auth SET isDelete = 1 WHERE plugin_id = '%s'", pluginId);
	sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, NULL);
	xrtFree(sSQL);

	sSQL = xrtFormat("SELECT table_name FROM plugin_table WHERE plugin_id = '%s'", pluginId);
	sqlite3_stmt* stmt;
	sqlite3_prepare_v3(G_DB->objDB, sSQL, -1, 0, &stmt, NULL);
	xrtFree(sSQL);

	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		str sTableName = (str)sqlite3_column_text(stmt, 0);
		str sDropSQL = xrtFormat("DROP TABLE IF EXISTS %s", sTableName);
		sqlite3_exec(G_DB->objDB, sDropSQL, NULL, NULL, NULL);
		xrtFree(sDropSQL);
		printf("        [Plugin] Dropped table: %s\n", sTableName);
	}
	sqlite3_finalize(stmt);

	sSQL = xrtFormat("DELETE FROM plugin_table WHERE plugin_id = '%s'", pluginId);
	sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, NULL);
	xrtFree(sSQL);

	sSQL = xrtFormat("DELETE FROM plugin_dependency WHERE plugin_name = '%s'", pluginId);
	sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, NULL);
	xrtFree(sSQL);

	printf("        [Plugin] Cleanup completed for plugin_id: %s\n", pluginId);
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

	// 插件自身信息
	G_PluginCtx->GetPluginId = PluginCtx_GetPluginId;
	G_PluginCtx->GetPluginName = PluginCtx_GetPluginName;
	G_PluginCtx->GetPluginPath = PluginCtx_GetPluginPath;

	// 文件操作
	G_PluginCtx->WriteFile = PluginCtx_WriteFile;
	G_PluginCtx->ReadFile = PluginCtx_ReadFile;
	G_PluginCtx->DeleteFile = PluginCtx_DeleteFile;
	G_PluginCtx->FileExists = PluginCtx_FileExists;
	G_PluginCtx->CreateDir = PluginCtx_CreateDir;
	G_PluginCtx->DeleteDir = PluginCtx_DeleteDir;
	G_PluginCtx->DirExists = PluginCtx_DirExists;
	G_PluginCtx->ScanDir = PluginCtx_ScanDir;
	G_PluginCtx->CopyFile = PluginCtx_CopyFile;
	G_PluginCtx->MoveFile = PluginCtx_MoveFile;

	// xPack 集成
	G_PluginCtx->CreateXpkg = PluginCtx_CreateXpkg;
	G_PluginCtx->ExtractXpkg = PluginCtx_ExtractXpkg;
	G_PluginCtx->GetXpkgInfo = PluginCtx_GetXpkgInfo;

	// 代码生成
	G_PluginCtx->GenerateModel = PluginCtx_GenerateModel;
	G_PluginCtx->CompilePlugin = PluginCtx_CompilePlugin;
	G_PluginCtx->ReloadPlugin = PluginCtx_ReloadPlugin;
	G_PluginCtx->GetPluginConfig = PluginCtx_GetPluginConfig;
	G_PluginCtx->SetPluginConfig = PluginCtx_SetPluginConfig;

	// 数据库操作
	G_PluginCtx->CreateTable = PluginCtx_CreateTable;
	G_PluginCtx->DropTable = PluginCtx_DropTable;
	G_PluginCtx->ExecuteSQL = PluginCtx_ExecuteSQL;
	G_PluginCtx->QuerySQL = PluginCtx_QuerySQL;
	G_PluginCtx->PrepareSQL = PluginCtx_PrepareSQL;
	G_PluginCtx->ExecuteStmt = PluginCtx_ExecuteStmt;
	G_PluginCtx->FinalizeStmt = PluginCtx_FinalizeStmt;

	// 插件管理
	G_PluginCtx->InstallPlugin = PluginCtx_InstallPlugin;
	G_PluginCtx->UninstallPlugin = PluginCtx_UninstallPlugin;
	G_PluginCtx->UpgradePlugin = PluginCtx_UpgradePlugin;

	// 模板渲染
	G_PluginCtx->RenderTemplate = PluginCtx_RenderTemplate;
	G_PluginCtx->RenderString = PluginCtx_RenderString;
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

// 按排序值加载已启用的插件
void PluginMgr_LoadEnabledPlugins()
{
	printf("        [Plugin] Loading enabled plugins...\n");
	
	// 收集所有已启用的插件
	xlist lstEnabled = xrtListCreate(sizeof(ptr));
	xrtDictWalk(G_PluginMgr->tblPlugins, PluginMgr_CollectEnabledProc, lstEnabled);
	
	// 简单冒泡排序（按 iSort）
	int iCount = xrtListCount(lstEnabled);
	for ( int i = 0; i < iCount - 1; i++ ) {
		for ( int j = 0; j < iCount - i - 1; j++ ) {
			PluginInstance* p1 = xrtListGetPtr(lstEnabled, j);
			PluginInstance* p2 = xrtListGetPtr(lstEnabled, j + 1);
			if ( p1->iSort > p2->iSort ) {
				xrtListSetPtr(lstEnabled, j, p2, NULL);
				xrtListSetPtr(lstEnabled, j + 1, p1, NULL);
			}
		}
	}
	
	// 按顺序加载
	for ( int i = 0; i < iCount; i++ ) {
		PluginInstance* pPlugin = xrtListGetPtr(lstEnabled, i);
		if ( pPlugin ) {
			Plugin_Enable(pPlugin);
		}
	}
	
	xrtListDestroy(lstEnabled);
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


// ==================== 扩展接口实现 ====================

// ===== 插件自身信息 =====

str PluginCtx_GetPluginId()
{
	if ( G_CurrentPlugin ) {
		return G_CurrentPlugin->sName;
	}
	return NULL;
}

str PluginCtx_GetPluginName()
{
	if ( G_CurrentPlugin ) {
		return G_CurrentPlugin->sTitle;
	}
	return NULL;
}

str PluginCtx_GetPluginPath()
{
	if ( G_CurrentPlugin ) {
		return G_CurrentPlugin->sPath;
	}
	return PluginPath;
}


// ===== 文件操作 =====

bool PluginCtx_WriteFile(str filePath, str content, size_t len)
{
	if ( !filePath || !content ) {
		return FALSE;
	}

	str sDir = xrtPathGetDir(filePath, 0);
	if ( sDir ) {
		xrtDirCreate(sDir);
		xrtFree(sDir);
	}

	size_t iLen = len > 0 ? len : strlen(content);
	int iResult = xrtFileWriteAll(filePath, content, iLen, XRT_CP_UTF8);
	return iResult >= 0;
}

bool PluginCtx_ReadFile(str filePath, str* outContent, size_t* outLen)
{
	if ( !filePath || !outContent ) {
		return FALSE;
	}

	str sContent = xrtFileReadAll(filePath, XRT_CP_UTF8, outLen);
	if ( !sContent ) {
		return FALSE;
	}

	*outContent = sContent;
	return TRUE;
}

bool PluginCtx_DeleteFile(str filePath)
{
	if ( !xrtFileExists(filePath) ) {
		return FALSE;
	}
	return xrtFileDelete(filePath);
}

bool PluginCtx_FileExists(str filePath)
{
	return xrtFileExists(filePath);
}

bool PluginCtx_CreateDir(str dirPath)
{
	if ( xrtDirExists(dirPath) ) {
		return TRUE;
	}
	return xrtDirCreate(dirPath);
}

bool PluginCtx_DeleteDir(str dirPath, bool bRecursive)
{
	if ( !xrtDirExists(dirPath) ) {
		return FALSE;
	}
	return xrtDirDelete(dirPath) >= 0;
}

bool PluginCtx_DirExists(str dirPath)
{
	return xrtDirExists(dirPath);
}

bool PluginCtx_ScanDir(str dirPath, bool bRecursive, DirScanCallback callback, ptr userData)
{
	return xrtDirScan(dirPath, bRecursive, callback, userData);
}

bool PluginCtx_CopyFile(str srcPath, str destPath)
{
	if ( !xrtFileExists(srcPath) ) {
		return FALSE;
	}

	str sDestDir = xrtPathGetDir(destPath, 0);
	if ( sDestDir ) {
		xrtDirCreate(sDestDir);
		xrtFree(sDestDir);
	}

	size_t iLen = 0;
	str sContent = xrtFileReadAll(srcPath, XRT_CP_UTF8, &iLen);
	if ( !sContent ) {
		return FALSE;
	}

	int iResult = xrtFileWriteAll(destPath, sContent, iLen, XRT_CP_UTF8);
	xrtFree(sContent);
	return iResult >= 0;
}

bool PluginCtx_MoveFile(str srcPath, str destPath)
{
	if ( !xrtFileExists(srcPath) ) {
		return FALSE;
	}

	str sDestDir = xrtPathGetDir(destPath, 0);
	if ( sDestDir ) {
		xrtDirCreate(sDestDir);
		xrtFree(sDestDir);
	}

	bool bResult = xrtFileMove(srcPath, destPath, TRUE);
	if ( !bResult ) {
		bResult = PluginCtx_CopyFile(srcPath, destPath);
		if ( bResult ) {
			xrtFileDelete(srcPath);
		}
	}
	return bResult;
}


// ===== xPack 集成 =====

int PluginCtx_CreateXpkg(str outputPath, str* fileList, int fileCount, int compressLevel)
{
	printf("        [Plugin] CreateXpkg: NOT IMPLEMENTED\n");
	return -1;
}

int PluginCtx_ExtractXpkg(str xpkgPath, str outputDir)
{
	printf("        [Plugin] ExtractXpkg: NOT IMPLEMENTED\n");
	return -1;
}

xvalue PluginCtx_GetXpkgInfo(str xpkgPath)
{
	printf("        [Plugin] GetXpkgInfo: NOT IMPLEMENTED\n");
	return NULL;
}


// ===== 代码生成 =====

bool PluginCtx_GenerateModel(str modelName, xvalue modelConfig)
{
	printf("        [Plugin] GenerateModel: %s - NOT FULLY IMPLEMENTED\n", modelName);
	return FALSE;
}

bool PluginCtx_CompilePlugin(str pluginName)
{
	return PluginMgr_ReloadPlugin(pluginName);
}

bool PluginCtx_ReloadPlugin(str pluginName)
{
	return PluginMgr_ReloadPlugin(pluginName);
}

xvalue PluginCtx_GetPluginConfig(str pluginName)
{
	PluginInstance* pPlugin = PluginMgr_GetPlugin(pluginName);
	if ( !pPlugin || !pPlugin->tblSettings ) {
		return NULL;
	}
	xvoAddRef(pPlugin->tblSettings);
	return pPlugin->tblSettings;
}

bool PluginCtx_SetPluginConfig(str pluginName, xvalue config)
{
	PluginInstance* pPlugin = PluginMgr_GetPlugin(pluginName);
	if ( !pPlugin || !config ) {
		return FALSE;
	}

	if ( pPlugin->tblSettings ) {
		xvoUnref(pPlugin->tblSettings);
	}

	xvoAddRef(config);
	pPlugin->tblSettings = config;
	Plugin_SaveConfig(pPlugin);
	return TRUE;
}


// ===== 数据库操作 =====

bool PluginCtx_CreateTable(str tableName, str sql)
{
	if ( !tableName || !sql ) {
		return FALSE;
	}

	char* sErr = NULL;
	int iRet = sqlite3_exec(G_DB->objDB, sql, NULL, NULL, &sErr);

	if ( iRet != SQLITE_OK ) {
		printf("        [Plugin] CreateTable failed: %s, error: %s\n", tableName, sErr);
		if ( sErr ) sqlite3_free(sErr);
		return FALSE;
	}

	if ( G_CurrentPluginId ) {
		str sInsertSQL = xrtFormat(
			"INSERT INTO plugin_table (plugin_id, table_name, table_type, description, create_time) "
			"VALUES ('%s', '%s', 'data', '', %lld)",
			G_CurrentPluginId, tableName, xrtNow()
		);
		sqlite3_exec(G_DB->objDB, sInsertSQL, NULL, NULL, NULL);
		xrtFree(sInsertSQL);
	}

	return TRUE;
}

bool PluginCtx_DropTable(str tableName)
{
	if ( !tableName ) {
		return FALSE;
	}

	str sSQL = xrtFormat("DROP TABLE IF EXISTS %s", tableName);
	char* sErr = NULL;
	int iRet = sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, &sErr);
	xrtFree(sSQL);

	if ( iRet != SQLITE_OK ) {
		printf("        [Plugin] DropTable failed: %s\n", sErr);
		if ( sErr ) sqlite3_free(sErr);
		return FALSE;
	}

	str sDeleteSQL = xrtFormat("DELETE FROM plugin_table WHERE table_name = '%s'", tableName);
	sqlite3_exec(G_DB->objDB, sDeleteSQL, NULL, NULL, NULL);
	xrtFree(sDeleteSQL);

	return TRUE;
}

bool PluginCtx_ExecuteSQL(str sql)
{
	if ( !sql ) {
		return FALSE;
	}

	char* sErr = NULL;
	int iRet = sqlite3_exec(G_DB->objDB, sql, NULL, NULL, &sErr);

	if ( iRet != SQLITE_OK ) {
		printf("        [Plugin] ExecuteSQL failed: %s\n", sErr);
		if ( sErr ) sqlite3_free(sErr);
		return FALSE;
	}

	return TRUE;
}

xvalue PluginCtx_QuerySQL(str sql)
{
	if ( !sql ) {
		return NULL;
	}

	sqlite3_stmt* stmt;
	int iRet = sqlite3_prepare_v3(G_DB->objDB, sql, -1, 0, &stmt, NULL);
	if ( iRet != SQLITE_OK ) {
		return NULL;
	}

	xvalue arrResult = xvoCreateArray();

	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		int iColCount = sqlite3_column_count(stmt);
		xvalue tblRow = xvoCreateTable();

		for ( int i = 0; i < iColCount; i++ ) {
			str sColName = (str)sqlite3_column_name(stmt, i);
			int iType = sqlite3_column_type(stmt, i);

			switch ( iType ) {
				case SQLITE_INTEGER:
					xvoTableSetValue(tblRow, sColName, strlen(sColName), xvoCreateInt(sqlite3_column_int64(stmt, i)), TRUE);
					break;
				case SQLITE_FLOAT:
					xvoTableSetValue(tblRow, sColName, strlen(sColName), xvoCreateFloat(sqlite3_column_double(stmt, i)), TRUE);
					break;
				case SQLITE_TEXT:
					xvoTableSetText(tblRow, sColName, strlen(sColName), (str)sqlite3_column_text(stmt, i), 0, FALSE);
					break;
				case SQLITE_NULL:
					xvoTableSetNull(tblRow, sColName, strlen(sColName));
					break;
			}
		}

		xvoArrayAppendValue(arrResult, tblRow, TRUE);
	}

	sqlite3_finalize(stmt);
	return arrResult;
}

sqlite3_stmt* PluginCtx_PrepareSQL(str sql)
{
	if ( !sql ) return NULL;

	sqlite3_stmt* stmt;
	int iRet = sqlite3_prepare_v3(G_DB->objDB, sql, -1, 0, &stmt, NULL);

	if ( iRet != SQLITE_OK ) {
		return NULL;
	}

	return stmt;
}

bool PluginCtx_ExecuteStmt(sqlite3_stmt* stmt)
{
	if ( !stmt ) return FALSE;

	int iRet = sqlite3_step(stmt);
	sqlite3_reset(stmt);

	return iRet == SQLITE_DONE || iRet == SQLITE_ROW;
}

void PluginCtx_FinalizeStmt(sqlite3_stmt* stmt)
{
	if ( stmt ) {
		sqlite3_finalize(stmt);
	}
}


// ===== 插件管理 =====

bool PluginCtx_InstallPlugin(str xpkgPath)
{
	printf("        [Plugin] InstallPlugin: NOT IMPLEMENTED\n");
	return FALSE;
}

bool PluginCtx_UninstallPlugin(str pluginName)
{
	PluginInstance* pPlugin = PluginMgr_GetPlugin(pluginName);
	if ( !pPlugin ) {
		return FALSE;
	}

	if ( pPlugin->bLoaded ) {
		Plugin_Disable(pPlugin);
	}

	str sPluginPath = pPlugin->sPath;
	if ( xrtDirExists(sPluginPath) ) {
		xrtDirDelete(sPluginPath);
	}

	xrtDictRemove(G_PluginMgr->tblPlugins, pluginName, strlen(pluginName));
	Plugin_Destroy(pPlugin);

	return TRUE;
}

bool PluginCtx_UpgradePlugin(str pluginName, str newXpkgPath)
{
	printf("        [Plugin] UpgradePlugin: NOT IMPLEMENTED\n");
	return FALSE;
}


// ===== 模板渲染 =====

str PluginCtx_RenderTemplate(str templatePath, xvalue data)
{
	if ( !templatePath ) {
		return NULL;
	}

	size_t iRetSize = 0;
	str sHtml = MakePageWithTemplate(templatePath, data, &iRetSize);
	return sHtml;
}

str PluginCtx_RenderString(str templateString, xvalue data)
{
	if ( !templateString || !data ) {
		return NULL;
	}

	XTE_LiteObject objTemplate = xteParse(templateString, strlen(templateString), NULL);
	if ( !objTemplate || !objTemplate->Success ) {
		return xrtFormat("Template parse error");
	}

	size_t iRetSize = 0;
	str sResult = xteMake(objTemplate, data, tblENV, G_Template, &iRetSize);
	xteParseFree(objTemplate);
	return sResult;
}


