


// ============================================
// 闁圭粯甯婂▎銏㈢不閿涘嫭鍊為柛?
// ============================================

#ifndef PLUGIN_MGR_H
#define PLUGIN_MGR_H

#include "plugin_ctx.h"



// 闁告挸绉撮幃婊勭珶閻楀牊顫栭柨娑欑鑶╅柡澶庢硶濞村宕楅崘鎻掑伎閻忕偐鍋撻柛娆愶耿閸?
extern xvalue tblENV;
extern xdict G_Template;


// ==================== 闁轰胶澧楀畵浣虹磼閹惧鈧垳鈧鐭粻?====================

// 闁圭粯甯婂▎銏⑩偓鍦仒缁躲儳绱掗幘瀵糕偓顖涙媴?
typedef struct {
	
	// ===== 闁糕晞娅ｉ、鍛┍閳╁啩绱?=====
	str sName;                  // 闁圭粯甯婂▎銏ゅ冀閸ヮ亞妲曢柨娑樼墢濞叉媽銇愰弴鐐村€抽柨?
	str sTitle;                 // 闁哄嫬澧介妵姘跺触瀹ュ泦?
	str sDesc;                  // 闁硅绻楅崼?
	str sVersion;               // 闁绘鐗婂﹢浼村矗?
	str sAuthor;                // 濞达絾绮忛埀?
	int iSort;                  // 闁圭儤甯掔花顓㈡晬閸繂顫ｉ弶鐐扮矙閵嗗孩鎯旇箛銉х
	
	// ===== 閻犱警鍨扮欢?=====
	str sPath;                  // 闁圭粯甯婂▎銏ゆ儎椤旇偐绉块悹渚灠缁?
	str sConfigPath;            // 闂佹澘绉堕悿鍡涘棘閸ワ附顐介悹渚灠缁?
	str sCodePath;              // 濞戞捁顔婇崬顒勬儘娴ｈ鐎ù鐘冲劶閻儳顕?
	str sDataPath;              // 闁轰胶澧楀畵渚€鎯勯鑲╃Э閻犱警鍨扮欢?
	
	// ===== 闁绘鍩栭埀?=====
	bool bEnabled;              // 闁哄嫷鍨伴幆渚€宕ラ婊勬殢
	bool bLoaded;               // 闁哄嫷鍨伴幆浣割啅閹绘帒顫ｉ弶?
	int iLoadOrder;             // 閻庡湱鍋ゅ顖炲礉閻樼儤绁板銈呮惈缁?
	
	// ===== TCC 闁绘鍩栭埀顑跨劍濠р偓 =====
	TCCState* pTccState;
	
	// ===== 资源跟踪 =====
	xlist lstDependencies;      // 依赖的其他插件
	xlist lstDependents;        // 被依赖列表（反向依赖，用于依赖追踪）
	xlist lstRoutes;            // 注册的路由URI列表
	xlist lstMenuIds;           // 注册的菜单ID列表
	xlist lstAuthGroupIds;      // 注册的权限分类ID列表
	xlist lstAuthIds;           // 注册的权限分组ID列表
	
	// ===== 闂佹澘绉堕悿?=====
	xvalue tblSettings;         // 闁圭粯甯婂▎銏ゆ嚊椤忓嫮鏆板☉鏂款樀閸樸倗绱?
	xvalue arrExports;          // 閻庣數鍘ч崵顓㈠箳閵夈儱缍撻柛姘Ф琚ㄩ柛鎺擃殙閵?
	
	// ===== 闁哄啫鐖煎Λ鍧楀箣?=====
	int64 iCreateTime;
	int64 iUpdateTime;
	int64 iEnableTime;
	
} PluginInstance;



// 闁圭粯甯婂▎銏㈢不閿涘嫭鍊為柛锝冨妿缁劑寮搁崟顏嗙Ъ
typedef struct {
	
	xdict tblPlugins;           // 闁圭粯甯婂▎銏⑩偓鍦仒缁躲儳鎮伴…鎺旂key: name闁?
	xlist lstLoadedPlugins;     // 鐎瑰憡褰冩慨鐐存姜閻ｅ本鐣遍柟缁樺笂濞嗐垽宕氬Δ鍕┾偓鍐晬閸喎鐦婚柛鏃傚Ь濞村洦銇勯崫鍕闁?
	xlist lstEventListeners;    // 濞存粌顑勫▎銏ゆ儎閹存繃鍎旈柛锝冨妼閸亞鎮?
	xdict tblExports;           // 闁圭粯甯婂▎銏⑩偓鐢靛帶閸ゎ厾鎮伴…鎺旂key: pluginName:exportName闁?
	
} PluginManager;



// 闁稿繈鍔岄惇顒勫箵閹哄秵顐界紒鐙呯磿閹﹪宕?
PluginManager* G_PluginMgr = NULL;

// 闁稿繈鍔岄惇顒勫箵閹哄秵顐藉☉鎾筹梗缁楀懘寮?
PluginContext* G_PluginCtx = NULL;

// 闁圭粯甯婂▎銏ゆ儎椤旇偐绉块悹渚灠缁?
str PluginPath = NULL;
str PluginDataPath = NULL;

// 鐟滅増鎸告晶鐘差潰閿濆懏韬璺哄閹﹪鎯冮崟顒€绲诲ù?
str G_CurrentPluginId = NULL;
PluginInstance* G_CurrentPlugin = NULL;



// ==================== 闁告挸绉撮幃婊勭珶閻楀牊顫?====================

void PluginCtx_Log(int level, str format, ...);
void PluginCtx_LoadPage(XS_ResponseObject objResp, int code, str head, str pagePath);
void PluginCtx_SendJson(XS_ResponseObject objResp, int code, str json, size_t len);
void PluginCtx_SendHtml(XS_ResponseObject objResp, int code, str html);
void PluginCtx_SendPage(XS_ResponseObject objResp, str pagePath, xvalue data);
void PluginCtx_SendFile(XS_ResponseObject objResp, str filePath, str mimeType);
void PluginCtx_SendError(XS_ResponseObject objResp, int code, str message);
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

// 依赖管理相关函数
int Plugin_CompareVersion(str v1, str v2);
bool Plugin_CheckVersionRequirement(str actualVersion, str minVersion, str maxVersion);
void Plugin_BuildDependentsGraph();
bool Plugin_TopologicalSort(xlist* pResult);
bool Plugin_ValidateDependencies();



// ==================== 濞戞挸锕ｇ粭鍛村棘閸ャ劌澶嶉柛娆欑到閻ゅ嫰鎮?====================

// 閻犱警鍨抽弫閬嶅箼瀹ュ嫮绋?
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
			sqlite3_prepare_v3(G_DB, sCheckSQL, -1, 0, &stmtCheck, NULL);
			xrtFree(sCheckSQL);

			if ( sqlite3_step(stmtCheck) == SQLITE_ROW ) {
				int iId = sqlite3_column_int(stmtCheck, 0);
				sqlite3_finalize(stmtCheck);
				str sUpdateSQL = xrtFormat(
					"UPDATE uris SET plugin_id = '%s', updateTime = %lld WHERE id = %d",
					G_CurrentPluginId, iNow, iId
				);
				sqlite3_exec(G_DB, sUpdateSQL, NULL, NULL, NULL);
				xrtFree(sUpdateSQL);
			} else {
				sqlite3_finalize(stmtCheck);
				str sInsertSQL = xrtFormat(
					"INSERT INTO uris (authID, uri, desc, isBackend, needAuth, needLog, putLog, putData, createTime, updateTime, plugin_id) "
					"VALUES (%d, '%s', '', %d, %d, %d, 0, 0, %lld, %lld, '%s')",
					authId, uri, bAdmin ? 1 : 0, bAuth ? 1 : 0, 0, iNow, iNow, G_CurrentPluginId
				);
				sqlite3_exec(G_DB, sInsertSQL, NULL, NULL, NULL);
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


// 闁兼寧绮屽畷鐔煎箼瀹ュ嫮绋?
int PluginCtx_AddMenu(int parent, str title, str icon, int type, str openType, str href, int sort, bool visible)
{
	str sSQL = NULL;
	sqlite3_stmt* stmt = NULL;
	int iExistingId = 0;

	if ( G_CurrentPluginId ) {
		sSQL = "SELECT id FROM menu WHERE href = ? AND (plugin_id = ? OR plugin_id IS NULL) AND isDelete = 0 ORDER BY id DESC LIMIT 1";
		if ( sqlite3_prepare_v3(G_DB, sSQL, -1, 0, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, href ? href : (str)"", -1, NULL);
			sqlite3_bind_text(stmt, 2, G_CurrentPluginId, -1, NULL);
			if ( sqlite3_step(stmt) == SQLITE_ROW ) {
				iExistingId = sqlite3_column_int(stmt, 0);
			}
			sqlite3_finalize(stmt);
		}
	}

	if ( iExistingId > 0 ) {
		sSQL = "UPDATE menu SET title = ?, icon = ?, type = ?, openType = ?, parent = ?, sort = ?, visible = ?, updateTime = ?, plugin_id = ?, isDelete = 0 WHERE id = ?";
		if ( sqlite3_prepare_v3(G_DB, sSQL, -1, 0, &stmt, NULL) == SQLITE_OK ) {
			int64 iNow = xrtNow();
			sqlite3_bind_text(stmt, 1, title, -1, NULL);
			sqlite3_bind_text(stmt, 2, icon ? icon : (str)"", -1, NULL);
			sqlite3_bind_int(stmt, 3, type);
			sqlite3_bind_text(stmt, 4, openType ? openType : (str)"_component", -1, NULL);
			sqlite3_bind_int(stmt, 5, parent);
			sqlite3_bind_int(stmt, 6, sort);
			sqlite3_bind_int(stmt, 7, visible ? 1 : 0);
			sqlite3_bind_int64(stmt, 8, iNow);
			sqlite3_bind_text(stmt, 9, G_CurrentPluginId ? G_CurrentPluginId : (str)"", -1, NULL);
			sqlite3_bind_int(stmt, 10, iExistingId);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
			if ( href && href[0] ) {
				str sCleanupSQL = xrtFormat(
					"UPDATE menu SET isDelete = 1, updateTime = %lld "
					"WHERE href = '%s' AND (plugin_id = '%s' OR plugin_id IS NULL) AND id <> %d AND isDelete = 0",
					iNow, href, G_CurrentPluginId ? G_CurrentPluginId : (str)"", iExistingId
				);
				sqlite3_exec(G_DB, sCleanupSQL, NULL, NULL, NULL);
				xrtFree(sCleanupSQL);
			}
			printf("        [Plugin] Menu updated: %s (id=%d, plugin_id=%s)\n", title, iExistingId, G_CurrentPluginId ? G_CurrentPluginId : (str)"NULL");
			return iExistingId;
		}
	}

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
	int iMenuId = sqlite3_last_insert_rowid(G_DB);
	sqlite3_reset(stmt_menu_add);
	printf("        [Plugin] Menu added: %s (id=%d, plugin_id=%s)\n", title, iMenuId, G_CurrentPluginId ? G_CurrentPluginId : (str)"NULL");
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
	sqlite3_exec(G_DB, sSQL, NULL, NULL, &sErr);
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
	sqlite3_exec(G_DB, sSQL, NULL, NULL, &sErr);
	xrtFree(sSQL);
	if ( sErr ) {
		sqlite3_free(sErr);
		return FALSE;
	}
	return TRUE;
}


// 闁哄鍟村娲箼瀹ュ嫮绋?
int PluginCtx_AddAuthGroup(str name, str desc, int sort)
{
	int64 iNow = xrtNow();
	sqlite3_bind_text(stmt_group_add, 1, name, -1, NULL);
	sqlite3_bind_text(stmt_group_add, 2, desc ? desc : (str)"", -1, NULL);
	sqlite3_bind_int(stmt_group_add, 3, sort);
	sqlite3_bind_int64(stmt_group_add, 4, iNow);
	sqlite3_bind_int64(stmt_group_add, 5, iNow);
	sqlite3_step(stmt_group_add);
	int iGroupId = sqlite3_last_insert_rowid(G_DB);
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
	int iAuthId = sqlite3_last_insert_rowid(G_DB);
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
	// 婵☆偀鍋撻柡宀婃珔RI闁哄嫷鍨伴幆浣衡偓娑櫭﹢?
	sqlite3_stmt* stmt_check;
	sqlite3_prepare_v3(G_DB, "SELECT id FROM uris WHERE uri = ?", -1, 0, &stmt_check, NULL);
	sqlite3_bind_text(stmt_check, 1, uri, -1, NULL);
	
	int64 iNow = xrtNow();
	if ( sqlite3_step(stmt_check) == SQLITE_ROW ) {
		// 闁哄洤鐡ㄩ弻?
		int iId = sqlite3_column_int(stmt_check, 0);
		sqlite3_finalize(stmt_check);
		
		char* sErr = NULL;
		str sSQL = xrtFormat(
			"UPDATE uris SET authID = %d, desc = '%s', isBackend = %d, needAuth = %d, needLog = %d, updateTime = %lld WHERE id = %d",
			authId, desc ? desc : (str)"", isBackend ? 1 : 0, needAuth ? 1 : 0, needLog ? 1 : 0, iNow, iId
		);
		sqlite3_exec(G_DB, sSQL, NULL, NULL, &sErr);
		xrtFree(sSQL);
		if ( sErr ) sqlite3_free(sErr);
	} else {
		// 闁圭粯甯掗崣?
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


// Session 闁瑰灝绉崇紞?
xvalue PluginCtx_GetAdminSession(str token)
{
	return Session_GetAdminByID(token);
}

xvalue PluginCtx_GetMemberSession(str token)
{
	return Session_GetMemberByID(token);
}

str PluginCtx_CreateAdminSession(int64 userId, str userName, int roleId, int timeout)
{
	str sToken = xrtMakeXIDS();
	xvalue tblSession = Session_CreateAdmin(sToken);
	xvoTableSetInt(tblSession, "id", 2, userId);
	xvoTableSetText(tblSession, "user", 4, userName, 0, FALSE);
	xvoTableSetInt(tblSession, "role", 4, roleId);
	if ( timeout > 0 ) {
		xvoTableSetInt(tblSession, "_expireTime", 11, xrtNow() + timeout);
	}
	Session_StoreAdmin(sToken, tblSession);
	xvoUnref(tblSession);
	return sToken;
}

str PluginCtx_CreateMemberSession(int64 userId, str userName, int groupId, int timeout)
{
	str sToken = xrtMakeXIDS();
	xvalue tblSession = Session_CreateMember(sToken);
	xvoTableSetInt(tblSession, "id", 2, userId);
	xvoTableSetText(tblSession, "username", 8, userName, 0, FALSE);
	xvoTableSetInt(tblSession, "groupId", 7, groupId);
	if ( timeout > 0 ) {
		xvoTableSetInt(tblSession, "_expireTime", 11, xrtNow() + timeout);
	}
	Session_StoreMember(sToken, tblSession);
	xvoUnref(tblSession);
	return sToken;
}

void PluginCtx_DestroyAdminSession(str token)
{
	Session_RemoveAdminByID(token);
}

void PluginCtx_DestroyMemberSession(str token)
{
	Session_RemoveMemberByID(token);
}

void PluginCtx_ExtendSession(bool isAdmin, str token, int timeout)
{
	xvalue tblSession = isAdmin ? 
		Session_GetAdminByID(token) :
		Session_GetMemberByID(token);
	if ( tblSession ) {
		int64 iNow = xrtNow();
		xvoTableSetInt(tblSession, "_activeTime", 11, iNow);
		xvoTableSetInt(tblSession, "_expireTime", 11, iNow + timeout);
	}
}


// HTTP 闁告繂绉寸花?
void PluginCtx_SendJson(XS_ResponseObject objResp, int code, str json, size_t len)
{
	http_reply(objResp, code, HTTP_CT_JSON, json, len);
}

void PluginCtx_SendHtml(XS_ResponseObject objResp, int code, str html)
{
	http_reply(objResp, code, HTTP_CT_HTML, html, strlen(html));
}

void PluginCtx_SendPage(XS_ResponseObject objResp, str pagePath, xvalue data)
{
	str sPluginId = G_CurrentPlugin ? G_CurrentPlugin->sName : NULL;
	size_t iRetSize = 0;
	str sHtml = NULL;

	if ( sPluginId ) {
		sHtml = Plugin_MakePageWithTemplate(sPluginId, pagePath, data, &iRetSize);
	} else {
		sHtml = MakePageWithTemplate(pagePath, data, &iRetSize);
	}

	if ( sHtml ) {
		http_reply(objResp, 200, HTTP_CT_HTML, sHtml, iRetSize > 0 ? iRetSize : strlen(sHtml));
		xrtFree(sHtml);
	} else {
		http_reply(objResp, 500, HTTP_CT_HTML, "Page render failed", 0);
	}
}


void PluginCtx_LoadPage(XS_ResponseObject objResp, int code, str head, str pagePath)
{
	str sPluginId = G_CurrentPlugin ? G_CurrentPlugin->sName : NULL;

	if ( sPluginId ) {
		Plugin_LoadPage(objResp, code, head, sPluginId, pagePath);
	} else {
		LoadPage(objResp, code, head, pagePath);
	}
}

void PluginCtx_SendFile(XS_ResponseObject objResp, str filePath, str mimeType)
{
	size_t iFileSize = 0;
	str sData = xrtFileGetAll(filePath, &iFileSize);
	str sHead = NULL;

	if ( sData == NULL ) {
		PluginCtx_SendError(objResp, 404, "File not found");
		return;
	}

	sHead = xrtFormat("Content-Type: %s\r\n", (mimeType && mimeType[0]) ? mimeType : (str)"application/octet-stream");
	http_reply(objResp, 200, sHead, sData, iFileSize);
	xrtFree(sHead);
	xrtFree(sData);
}

void PluginCtx_SendError(XS_ResponseObject objResp, int code, str message)
{
	str sJson = xrtFormat("{\"result\":false,\"message\":\"%s\"}", message);
	http_reply(objResp, code, HTTP_CT_JSON, sJson, strlen(sJson));
	xrtFree(sJson);
}


// 闂佹澘绉堕悿鍡涘箼瀹ュ嫮绋?
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


// JSON 闁瑰灝绉崇紞?
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


// 鐎规悶鍎遍崣鍧楀礄閼恒儲娈?
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


// 闁哄啨鍎辩换?
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
	// 缂佺姭鍋撻柛鏍ㄧ墪閻ゅ嫰鎮崇敮顔剧獥闁瑰灚鎸稿畵鍐媼閸ф锛栭柡鍐﹀劚缁?
	printf("[ACCESS] user=%s uri=%s method=%s\n", user ? user : (str)"", uri, method);
}


// 闁圭粯甯婂▎銏ゆ⒒閹绢喒鍋撳顐＄箚
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


// 濞存粌顑勫▎銏㈠寲閼姐倗鍩?
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
	// 闁哄被鍎叉竟姗€寮伴姘剨鐎圭寮跺﹢浣烘嫚閵夈倗鐨戝ù鐘插濞堟垿鎯勯幋婵囧剶闁?
	int iCount = xrtListCount(G_PluginMgr->lstEventListeners);
	for ( int i = 0; i < iCount; i++ ) {
		EventListener* pListener = xrtListGetPtr(G_PluginMgr->lstEventListeners, i);
		if ( pListener && strcmp(pListener->sEventName, eventName) == 0 ) {
			int iIdx = xrtListCount(pListener->lstCallbacks);
			xrtListSetPtr(pListener->lstCallbacks, iIdx, callback, NULL);
			return TRUE;
		}
	}
	
	// 闁告帗绋戠紓鎾诲棘閹殿喗鐣遍柣鈺傚灥閹宕?
	EventListener* pListener = xrtMalloc(sizeof(EventListener));
	pListener->sEventName = xrtCopyStr(eventName, 0);
	pListener->lstCallbacks = xrtListCreate(sizeof(ptr), 0);
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



// ==================== 闁圭粯甯婂▎銏⑩偓鍦仒缁躲儳绮婚敍鍕€?====================

// 闁告帗绋戠紓鎾诲箵閹哄秵顐介悗鍦仒缁?
PluginInstance* Plugin_Create(str sName)
{
	PluginInstance* pPlugin = xrtMalloc(sizeof(PluginInstance));
	memset(pPlugin, 0, sizeof(PluginInstance));

	pPlugin->sName = xrtCopyStr(sName, 0);
	pPlugin->lstRoutes = xrtListCreate(sizeof(ptr), 0);
	pPlugin->lstMenuIds = xrtListCreate(sizeof(int), 0);
	pPlugin->lstAuthGroupIds = xrtListCreate(sizeof(int), 0);
	pPlugin->lstAuthIds = xrtListCreate(sizeof(int), 0);
	pPlugin->lstDependencies = xrtListCreate(sizeof(ptr), 0);
	pPlugin->lstDependents = xrtListCreate(sizeof(ptr), 0);
	pPlugin->bEnabled = FALSE;
	pPlugin->bLoaded = FALSE;

	return pPlugin;
}


// 闂佸簱鍋撴慨锝勭劍瑜板啯绂掔捄铏规澖濞?
void Plugin_Destroy(PluginInstance* pPlugin)
{
	if ( !pPlugin ) return;
	
	// 濠碘€冲€归悘澶婎啅閹绘帒顫ｉ弶鐐存灮缁辨繈宕楅崼婵嗙セ閺?
	if ( pPlugin->bLoaded ) {
		Plugin_Disable(pPlugin);
	}
	
	// 闁告鐡曞ù鍥箵閹哄秵顐介柡鍐啇缁辨繄娑甸鈧崹褰掓⒔閵堝棗绲诲ù鐘冲劶缁侇偄鈹冮幇鍓佺濞戞挸娴烽々锕傛偨閵婏附顦ч柣銊ュ閽傚宕氶悩缁樼彑濞戞挸绉撮幃鎾绘晬?
	str sSQL;
	sSQL = xrtFormat("DELETE FROM menu WHERE plugin_id = '%s'", pPlugin->sName);
	sqlite3_exec(G_DB, sSQL, NULL, NULL, NULL);
	xrtFree(sSQL);
	
	sSQL = xrtFormat("DELETE FROM uris WHERE plugin_id = '%s'", pPlugin->sName);
	sqlite3_exec(G_DB, sSQL, NULL, NULL, NULL);
	xrtFree(sSQL);
	
	sSQL = xrtFormat("DELETE FROM authGroup WHERE plugin_id = '%s'", pPlugin->sName);
	sqlite3_exec(G_DB, sSQL, NULL, NULL, NULL);
	xrtFree(sSQL);
	
	sSQL = xrtFormat("DELETE FROM auth WHERE plugin_id = '%s'", pPlugin->sName);
	sqlite3_exec(G_DB, sSQL, NULL, NULL, NULL);
	xrtFree(sSQL);
	
	// 闁告帞濞€濞呭酣骞撻幒宥嗩偨闁告帗绋戠紓鎾绘儍閸曨剚娈堕柟璇″枦閵?
	sSQL = xrtFormat("SELECT table_name FROM plugin_table WHERE plugin_id = '%s'", pPlugin->sName);
	sqlite3_stmt* stmt;
	sqlite3_prepare_v3(G_DB, sSQL, -1, 0, &stmt, NULL);
	xrtFree(sSQL);
	
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		str sTableName = (str)sqlite3_column_text(stmt, 0);
		str sDropSQL = xrtFormat("DROP TABLE IF EXISTS %s", sTableName);
		sqlite3_exec(G_DB, sDropSQL, NULL, NULL, NULL);
		xrtFree(sDropSQL);
	}
	sqlite3_finalize(stmt);
	
	// 闁告帞濞€濞呭酣骞撻幒宥嗩偨閻炴稏鍔忛鍥亹?
	sSQL = xrtFormat("DELETE FROM plugin_table WHERE plugin_id = '%s'", pPlugin->sName);
	sqlite3_exec(G_DB, sSQL, NULL, NULL, NULL);
	xrtFree(sSQL);
	
	// 闂佹彃锕ラ弬?TCC 闁绘鍩栭埀顑跨劍濠р偓
	if ( pPlugin->pTccState ) {
		tcc_delete(pPlugin->pTccState);
		pPlugin->pTccState = NULL;
	}
	
	// 闂佹彃锕ラ弬渚€宕氬Δ鍕┾偓?
	if ( pPlugin->lstRoutes ) xrtListDestroy(pPlugin->lstRoutes);
	if ( pPlugin->lstMenuIds ) xrtListDestroy(pPlugin->lstMenuIds);
	if ( pPlugin->lstAuthGroupIds ) xrtListDestroy(pPlugin->lstAuthGroupIds);
	if ( pPlugin->lstAuthIds ) xrtListDestroy(pPlugin->lstAuthIds);
	if ( pPlugin->lstDependencies ) xrtListDestroy(pPlugin->lstDependencies);
	if ( pPlugin->lstDependents ) xrtListDestroy(pPlugin->lstDependents);
	
	// 闂佹彃锕ラ弬渚€鏌婂鍥╂瀭
	if ( pPlugin->tblSettings ) xvoUnref(pPlugin->tblSettings);
	if ( pPlugin->arrExports ) xvoUnref(pPlugin->arrExports);
	
	// 闂佹彃锕ラ弬浣衡偓娑欘殘椤戜焦绋?
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


// 濞寸姴閰ｉ崢銈囩磾椤旇姤鐎ù鐘烘硾婵偞娼懞銉ョ祷濞寸姵婀规穱濠囧箒?
bool Plugin_LoadConfig(PluginInstance* pPlugin)
{
	xvalue tblConfig = xrtParseJSON_File(pPlugin->sConfigPath);
	if ( !tblConfig ) {
		printf("        [Plugin] Failed to load config: %s\n", pPlugin->sConfigPath);
		return FALSE;
	}
	
	// 閻犲洩顕цぐ鍥春閾忚鏀ㄥǎ鍥ｅ墲娴?
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
	
	// 閻犲洩顕цぐ鍥嚊椤忓嫮鏆板☉鏂款樀閸樸倗绱?
	xvalue tblSettings = xvoTableGetValue(tblConfig, "settings", 8);
	if ( tblSettings ) {
		xvoAddRef(tblSettings);
		pPlugin->tblSettings = tblSettings;
	}
	
	// 閻犲洩顕цぐ鍥┾偓鐢靛帶閸ゎ參宕氬Δ鍕┾偓?
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


// 濞ｅ洦绻傞悺銊╁箵閹哄秵顐介梺鏉跨Ф閻?
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


// TCC 闂佹寧鐟ㄩ銈夊炊閻愬墎娈?
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
	
	if ( (strlen(sCode) >= 3) && (((unsigned char)sCode[0]) == 0xEF) && (((unsigned char)sCode[1]) == 0xBB) && (((unsigned char)sCode[2]) == 0xBF) ) {
		memmove(sCode, sCode + 3, strlen(sCode + 3) + 1);
	}

	// 闁告帗绋戠紓?TCC 闁绘鍩栭埀顑跨劍濠р偓

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

	tcc_add_symbol(pTcc, "HttpMethodIs", HttpMethodIs);
	tcc_add_symbol(pTcc, "HttpGetQueryVar", HttpGetQueryVar);
	tcc_add_symbol(pTcc, "HttpMultipartNext", HttpMultipartNext);
	tcc_add_symbol(pTcc, "HttpMultipartNameIs", HttpMultipartNameIs);
	tcc_add_symbol(pTcc, "http_reply", http_reply);
	tcc_add_symbol(pTcc, "HttpReplyFormat", HttpReplyFormat);

	// 闁革附婢樺鍐煂瀹ュ懐鏆伴柛?
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


// 闁告鐡曞ù鍥箵閹哄秵顐?TCC 闁绘鍩栭埀顑跨劍濠р偓
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


// 闁告凹鍨抽弫銈夊箵閹哄秵顐?
bool Plugin_Enable(PluginInstance* pPlugin)
{
	printf("        [Plugin] Enabling %s...\n", pPlugin->sName);

	if ( pPlugin->bLoaded ) {
		printf("        [Plugin] Plugin is already loaded\n");
		return TRUE;
	}

	G_CurrentPlugin = pPlugin;
	G_CurrentPluginId = pPlugin->sName;

	Plugin_LoadTemplates(pPlugin->sName);

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


// 缂佸倷鑳堕弫銈夊箵閹哄秵顐?
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

	Plugin_UnloadTemplates(pPlugin->sName);

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


// 婵炴挸鎳愰幃濠囧箵閹哄秵顐介柛蹇撶枃娴犲牓鎯冮崟顔俱偒婵?
void Plugin_CleanupResources(str pluginId)
{
	if ( !pluginId ) return;

	printf("        [Plugin] Cleaning up resources for plugin_id: %s\n", pluginId);

	str sSQL = xrtFormat(
		"UPDATE menu SET isDelete = 1, updateTime = %lld WHERE plugin_id = '%s'",
		xrtNow(), pluginId
	);
	sqlite3_exec(G_DB, sSQL, NULL, NULL, NULL);
	xrtFree(sSQL);

	sSQL = xrtFormat("DELETE FROM uris WHERE plugin_id = '%s'", pluginId);
	sqlite3_exec(G_DB, sSQL, NULL, NULL, NULL);
	xrtFree(sSQL);

	sSQL = xrtFormat("UPDATE authGroup SET isDelete = 1 WHERE plugin_id = '%s'", pluginId);
	sqlite3_exec(G_DB, sSQL, NULL, NULL, NULL);
	xrtFree(sSQL);

	sSQL = xrtFormat("UPDATE auth SET isDelete = 1 WHERE plugin_id = '%s'", pluginId);
	sqlite3_exec(G_DB, sSQL, NULL, NULL, NULL);
	xrtFree(sSQL);

	sSQL = xrtFormat("SELECT table_name FROM plugin_table WHERE plugin_id = '%s'", pluginId);
	sqlite3_stmt* stmt;
	sqlite3_prepare_v3(G_DB, sSQL, -1, 0, &stmt, NULL);
	xrtFree(sSQL);

	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		str sTableName = (str)sqlite3_column_text(stmt, 0);
		str sDropSQL = xrtFormat("DROP TABLE IF EXISTS %s", sTableName);
		sqlite3_exec(G_DB, sDropSQL, NULL, NULL, NULL);
		xrtFree(sDropSQL);
		printf("        [Plugin] Dropped table: %s\n", sTableName);
	}
	sqlite3_finalize(stmt);

	sSQL = xrtFormat("DELETE FROM plugin_table WHERE plugin_id = '%s'", pluginId);
	sqlite3_exec(G_DB, sSQL, NULL, NULL, NULL);
	xrtFree(sSQL);

	sSQL = xrtFormat("DELETE FROM plugin_dependency WHERE plugin_name = '%s'", pluginId);
	sqlite3_exec(G_DB, sSQL, NULL, NULL, NULL);
	xrtFree(sSQL);

	printf("        [Plugin] Cleanup completed for plugin_id: %s\n", pluginId);
}



// ==================== 闁圭粯甯婂▎銏㈢不閿涘嫭鍊為柛锝冨妼閸ㄥ灚鎱ㄧ€ｎ亜顕?====================

// 闁告帗绻傞～鎰板礌閺嶃劌绲诲ù鐘虫构缁楀倹绋夌€ｎ偅鐎?
void PluginCtx_Init()
{
	G_PluginCtx = xrtMalloc(sizeof(PluginContext));
	memset(G_PluginCtx, 0, sizeof(PluginContext));
	
	// 闁哄秶顭堢缓楣冨极閻楀牆绁?
	G_PluginCtx->pDB = G_DB;
	G_PluginCtx->pAdminSession = &G_AdminSession;
	G_PluginCtx->pMemberSession = &G_MemberSession;
	G_PluginCtx->pOption = &G_Option;
	
	// 閻犱警鍨扮欢鐐寸┍閳╁啩绱?
	G_PluginCtx->sAppPath = AppPath;
	G_PluginCtx->sWebPath = WebPath;
	G_PluginCtx->sDataPath = xrtPathJoin(2, AppPath, "data");
	G_PluginCtx->sPluginPath = PluginPath;
	G_PluginCtx->sPagePath = PagePath;
	
	// 閻犱警鍨抽弫閬嶅箼瀹ュ嫮绋?
	G_PluginCtx->AddRoute = PluginCtx_AddRoute;
	G_PluginCtx->RemoveRoute = PluginCtx_RemoveRoute;
	G_PluginCtx->GetRoute = PluginCtx_GetRoute;
	
	// 闁兼寧绮屽畷鐔煎箼瀹ュ嫮绋?
	G_PluginCtx->AddMenu = PluginCtx_AddMenu;
	G_PluginCtx->RemoveMenu = PluginCtx_RemoveMenu;
	G_PluginCtx->ShowMenu = PluginCtx_ShowMenu;
	G_PluginCtx->HideMenu = PluginCtx_HideMenu;
	
	// 闁哄鍟村娲箼瀹ュ嫮绋?
	G_PluginCtx->AddAuthGroup = PluginCtx_AddAuthGroup;
	G_PluginCtx->AddAuth = PluginCtx_AddAuth;
	G_PluginCtx->RemoveAuthGroup = PluginCtx_RemoveAuthGroup;
	G_PluginCtx->RemoveAuth = PluginCtx_RemoveAuth;
	G_PluginCtx->SyncUriAuth = PluginCtx_SyncUriAuth;
	G_PluginCtx->ReloadAuthCache = PluginCtx_ReloadAuthCache;
	
	// Session 闁瑰灝绉崇紞?
	G_PluginCtx->GetAdminSession = PluginCtx_GetAdminSession;
	G_PluginCtx->GetMemberSession = PluginCtx_GetMemberSession;
	G_PluginCtx->CreateAdminSession = PluginCtx_CreateAdminSession;
	G_PluginCtx->CreateMemberSession = PluginCtx_CreateMemberSession;
	G_PluginCtx->DestroyAdminSession = PluginCtx_DestroyAdminSession;
	G_PluginCtx->DestroyMemberSession = PluginCtx_DestroyMemberSession;
	G_PluginCtx->ExtendSession = PluginCtx_ExtendSession;
	
	// HTTP 闁告繂绉寸花?
	G_PluginCtx->SendJson = PluginCtx_SendJson;
	G_PluginCtx->SendHtml = PluginCtx_SendHtml;
	G_PluginCtx->SendPage = PluginCtx_SendPage;
	G_PluginCtx->LoadPage = PluginCtx_LoadPage;
	G_PluginCtx->SendFile = PluginCtx_SendFile;
	G_PluginCtx->SendError = PluginCtx_SendError;
	
	// 闂佹澘绉堕悿鍡涘箼瀹ュ嫮绋?
	G_PluginCtx->GetOption = PluginCtx_GetOption;
	G_PluginCtx->SetOption = PluginCtx_SetOption;
	G_PluginCtx->ReloadOption = PluginCtx_ReloadOption;
	
	// JSON 闁瑰灝绉崇紞?
	G_PluginCtx->JsonParse = PluginCtx_JsonParse;
	G_PluginCtx->JsonStringify = PluginCtx_JsonStringify;
	G_PluginCtx->JsonFree = PluginCtx_JsonFree;
	
	// 鐎规悶鍎遍崣鍧楀礄閼恒儲娈?
	G_PluginCtx->TimeNow = PluginCtx_TimeNow;
	G_PluginCtx->Format = PluginCtx_Format;
	G_PluginCtx->Free = PluginCtx_Free;
	G_PluginCtx->HashPassword = PluginCtx_HashPassword;
	G_PluginCtx->GenerateSalt = PluginCtx_GenerateSalt;
	G_PluginCtx->GenerateToken = PluginCtx_GenerateToken;
	
	// 闁哄啨鍎辩换?
	G_PluginCtx->Log = PluginCtx_Log;
	G_PluginCtx->LogAccess = PluginCtx_LogAccess;
	
	// 闁圭粯甯婂▎銏ゆ⒒閹绢喒鍋撳顐＄箚
	G_PluginCtx->GetPluginExport = PluginCtx_GetPluginExport;
	G_PluginCtx->SetPluginExport = PluginCtx_SetPluginExport;
	
	// 濞存粌顑勫▎銏㈠寲閼姐倗鍩?
	G_PluginCtx->EmitEvent = PluginCtx_EmitEvent;
	G_PluginCtx->OnEvent = PluginCtx_OnEvent;
	G_PluginCtx->OffEvent = PluginCtx_OffEvent;

	// 闁圭粯甯婂▎銏ゆ嚊椤忓洭鐓╁ǎ鍥ｅ墲娴?
	G_PluginCtx->GetPluginId = PluginCtx_GetPluginId;
	G_PluginCtx->GetPluginName = PluginCtx_GetPluginName;
	G_PluginCtx->GetPluginPath = PluginCtx_GetPluginPath;

	// 闁哄倸娲ｅ▎銏ゅ箼瀹ュ嫮绋?
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

	// xPack 闂傚棗妫欓崹?
	G_PluginCtx->CreateXpkg = PluginCtx_CreateXpkg;
	G_PluginCtx->ExtractXpkg = PluginCtx_ExtractXpkg;
	G_PluginCtx->GetXpkgInfo = PluginCtx_GetXpkgInfo;

	// 濞寸媴绲块悥婊堟偨閻旂鐏?
	G_PluginCtx->GenerateModel = PluginCtx_GenerateModel;
	G_PluginCtx->CompilePlugin = PluginCtx_CompilePlugin;
	G_PluginCtx->ReloadPlugin = PluginCtx_ReloadPlugin;
	G_PluginCtx->GetPluginConfig = PluginCtx_GetPluginConfig;
	G_PluginCtx->SetPluginConfig = PluginCtx_SetPluginConfig;

	// 闁轰胶澧楀畵浣规償閹惧瓨鎯欏ù?
	G_PluginCtx->CreateTable = PluginCtx_CreateTable;
	G_PluginCtx->DropTable = PluginCtx_DropTable;
	G_PluginCtx->ExecuteSQL = PluginCtx_ExecuteSQL;
	G_PluginCtx->QuerySQL = PluginCtx_QuerySQL;
	G_PluginCtx->PrepareSQL = PluginCtx_PrepareSQL;
	G_PluginCtx->ExecuteStmt = PluginCtx_ExecuteStmt;
	G_PluginCtx->FinalizeStmt = PluginCtx_FinalizeStmt;

	// 闁圭粯甯婂▎銏㈢不閿涘嫭鍊?
	G_PluginCtx->InstallPlugin = PluginCtx_InstallPlugin;
	G_PluginCtx->UninstallPlugin = PluginCtx_UninstallPlugin;
	G_PluginCtx->UpgradePlugin = PluginCtx_UpgradePlugin;

	// 婵☆垪鍓濆妯恒€掗崣澶屽帬
	G_PluginCtx->RenderTemplate = PluginCtx_RenderTemplate;
	G_PluginCtx->RenderString = PluginCtx_RenderString;
}


// 闁规鍋呭鍧楀箵閹哄秵顐介柣鈺婂枛缂嶅秹鎯冮崟顐ｇ閻犲鍟崵閬嶅极?
int PluginMgr_ScanDirProc(str sPath, size_t iSize, int bDir, ptr pData, size_t iPathSize)
{
	// 闁告瑯浜滈ˇ鈺呮偠閸℃瑦绐楃憸鐗堟穿缁辨瑦娼诲☉妯哄汲闁哄啳顔愮槐婕汥ir=1闁挎稑顧€缁辨繄鎹勭€圭姷绠栭柡鍌氭矗濞?0)闁告粌鐬奸‖鍥ь嚕閳ь剟鎯勯鑲╃Э(2)
	if ( bDir != 1 ) return FALSE;
	
	// 闁兼儳鍢茶ぐ鍥儎椤旇偐绉块柛?
	str sName = xrtPathGetName(sPath, 0);
	if ( !sName ) return FALSE;
	
	// 閻犲搫鐤囩换鍐⒕閹邦垱顥戦柣鈺婂枛缂嶅秹宕畝鈧竟鎺戔枔婵犲嫭绐楃憸?
	if ( sName[0] == '.' || sName[0] == '_' ) {
		xrtFree(sName);
		return FALSE;
	}
	
	// 閻犲搫鐤囩换?plugin.h 缂佹稑顦崣鏇㈠礂鏉堛劍鐎ù?
	if ( strcmp(sName, "plugin.h") == 0 ) {
		xrtFree(sName);
		return FALSE;
	}
	
	// 婵☆偀鍋撻柡灞诲劜濡叉悂宕ラ敂鑺ョ畳闂佹澘绉堕悿鍡涘棘閸ワ附顐?
	str sConfigPath = xrtFormat("%s/config.json", sPath);
	if ( !xrtFileExists(sConfigPath) ) {
		xrtFree(sConfigPath);
		xrtFree(sName);
		return FALSE;
	}
	
	// 婵☆偀鍋撻柡灞诲劜濡叉悂宕ラ敂鑺ョ畳濞寸媴绲块悥婊堝棘閸ワ附顐?
	str sCodePath = xrtFormat("%s/main.c", sPath);
	if ( !xrtFileExists(sCodePath) ) {
		xrtFree(sConfigPath);
		xrtFree(sCodePath);
		xrtFree(sName);
		return FALSE;
	}
	
	printf("        [Plugin] Found plugin: %s\n", sName);
	
	// 闁告帗绋戠紓鎾诲箵閹哄秵顐介悗鍦仒缁?
	PluginInstance* pPlugin = Plugin_Create(sName);
	pPlugin->sPath = xrtCopyStr(sPath, 0);
	pPlugin->sConfigPath = sConfigPath;
	pPlugin->sCodePath = sCodePath;
	pPlugin->sDataPath = xrtPathJoin(2, PluginDataPath, sName);
	
	// 缁绢収鍠曠换姘跺极閻楀牆绁﹂柣鈺婂枛缂嶅秶鈧稒锚濠€?
	xrtDirCreate(pPlugin->sDataPath);
	
	// 闁告梻濮惧ù鍥煀瀹ュ洨鏋?
	if ( !Plugin_LoadConfig(pPlugin) ) {
		Plugin_Destroy(pPlugin);
		xrtFree(sName);
		return FALSE;
	}
	
	// 婵烇綀顕ф慨鐐哄礆閻楀牆绲诲ù鐘冲劶閵?
	xrtDictSet(G_PluginMgr->tblPlugins, sName, strlen(sName), NULL);
	PluginInstance** ppPlugin = xrtDictGet(G_PluginMgr->tblPlugins, sName, strlen(sName));
	*ppPlugin = pPlugin;
	
	xrtFree(sName);
	return FALSE;  // 缂備綀鍛暰闂侇剙绉村?
}

// 闁规鍋呭鍧楃嵁鐠哄搫顫ｉ弶鐐跺Г婢у秹寮垫径瀣祷濞?
void PluginMgr_ScanPlugins()
{
	printf("        [Plugin] Scanning plugins in: %s\n", PluginPath);
	xrtDirScan(PluginPath, FALSE, PluginMgr_ScanDirProc, NULL);
}


// 闁衡偓閸洘鑲犵€瑰憡褰冮幆搴ㄦ偨閵婏箑绲诲ù鐘插濞堟垿宕堕悙鍓佹闁告垼濮ら弳?
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
	return FALSE;  // 缂備綀鍛暰闂侇剙绉村?
}

// 闁圭顦扮敮鎾存償韫囨挴鍋撻悡搴☆潱閺夌偠妫勯崙锟犲触椤栨粍鏆忛柣銊ュ瑜板啯绂?
void PluginMgr_LoadEnabledPlugins()
{
	printf("        [Plugin] Loading enabled plugins...\n");
	fflush(stdout);

	xlist lstEnabled = xrtListCreate(sizeof(ptr), 0);
	printf("[xadmin:plugin] collect enabled begin\n");
	fflush(stdout);
	xrtDictWalk(G_PluginMgr->tblPlugins, PluginMgr_CollectEnabledProc, lstEnabled);
	printf("[xadmin:plugin] collect enabled done\n");
	fflush(stdout);

	int iCount = xrtListCount(lstEnabled);
	printf("[xadmin:plugin] enabled count=%d\n", iCount);
	fflush(stdout);
	if ( iCount <= 0 ) {
		xrtListDestroy(lstEnabled);
		return;
	}

	if ( !Plugin_ValidateDependencies() ) {
		printf("        [Plugin] ERROR: Dependency validation failed\n");
		xrtListDestroy(lstEnabled);
		return;
	}

	xlist loadOrder = xrtListCreate(sizeof(ptr), 0);
	if ( !Plugin_TopologicalSort(&loadOrder) ) {
		printf("        [Plugin] ERROR: Circular dependency detected\n");
		xrtListDestroy(loadOrder);
		xrtListDestroy(lstEnabled);
		return;
	}

	printf("[xadmin:plugin] sort done\n");
	fflush(stdout);

	int iLoadCount = xrtListCount(loadOrder);
	for ( int i = 0; i < iLoadCount; i++ ) {
		PluginInstance* pPlugin = xrtListGetPtr(loadOrder, i);
		printf("[xadmin:plugin] enabling index=%d name=%s\n", i, pPlugin ? pPlugin->sName : (str)"(null)");
		fflush(stdout);
		if ( pPlugin && !Plugin_Enable(pPlugin) ) {
			printf("        [Plugin] ERROR: Failed to enable plugin '%s'\n", pPlugin->sName);
		}
		printf("[xadmin:plugin] enable return index=%d name=%s\n", i, pPlugin && pPlugin->sName ? pPlugin->sName : (str)"(null)");
		fflush(stdout);
	}

	xrtListDestroy(loadOrder);
	xrtListDestroy(lstEnabled);
}
void PluginMgr_EnsureMenu()
{
	// 婵☆偀鍋撻柡灞诲劜瑜板啯绂掗崜渚囧悁闁荤偛妫滆ぐ宥夊础閺囩喐笑闁告熬绠戦悺銊╁捶?
	str sSQL = "SELECT COUNT(*) FROM menu WHERE href = '/admin/view/plugin' AND isDelete = 0";
	sqlite3_stmt* stmt;
	int iRet = sqlite3_prepare_v3(G_DB, sSQL, -1, 0, &stmt, NULL);
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
	
	// 闁哄被鍎叉竟妯兼媼閸撗呮瀭闁兼寧绮屽畷鐔兼儍閸戭毆闁挎稑娼穉rent闁?
	sSQL = "SELECT id FROM menu WHERE title = '閻犱礁澧介悿? AND parent = 0 AND isDelete = 0";
	iRet = sqlite3_prepare_v3(G_DB, sSQL, -1, 0, &stmt, NULL);
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
	
	// 闁告帗绋戠紓鎾诲箵閹哄秵顐界紒鐙呯磿閹﹪鎳ｅ鍐ㄧ
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
	int iMenuId = sqlite3_last_insert_rowid(G_DB);
	sqlite3_reset(stmt_menu_add);
	printf("        [Plugin] Menu created: 插件管理 (id=%d)\n", iMenuId);
}



void PluginMgr_Init()
{
	printf("        PluginMgr_Init \n");
	fflush(stdout);

	printf("[xadmin:plugin] path begin\n");
	fflush(stdout);
	PluginPath = xrtPathJoin(2, AppPath, "script/plugin");
	PluginDataPath = xrtPathJoin(2, AppPath, "data/plugin");
	printf("[xadmin:plugin] path done\n");
	fflush(stdout);

	printf("[xadmin:plugin] mkdir begin\n");
	fflush(stdout);
	xrtDirCreate(PluginPath);
	xrtDirCreate(PluginDataPath);
	printf("[xadmin:plugin] mkdir done\n");
	fflush(stdout);

	printf("[xadmin:plugin] manager alloc begin\n");
	fflush(stdout);
	G_PluginMgr = xrtMalloc(sizeof(PluginManager));
	memset(G_PluginMgr, 0, sizeof(PluginManager));
	G_PluginMgr->tblPlugins = xrtDictCreate(sizeof(ptr), 0);
	G_PluginMgr->lstLoadedPlugins = xrtListCreate(sizeof(ptr), 0);
	G_PluginMgr->lstEventListeners = xrtListCreate(sizeof(ptr), 0);
	G_PluginMgr->tblExports = xrtDictCreate(sizeof(PluginExport), 0);
	printf("[xadmin:plugin] manager alloc done\n");
	fflush(stdout);

	printf("[xadmin:plugin] ctx init begin\n");
	fflush(stdout);
	PluginCtx_Init();
	printf("[xadmin:plugin] ctx init done\n");
	fflush(stdout);

	printf("[xadmin:plugin] ensure menu begin\n");
	fflush(stdout);
	PluginMgr_EnsureMenu();
	printf("[xadmin:plugin] ensure menu done\n");
	fflush(stdout);

	printf("[xadmin:plugin] scan begin\n");
	fflush(stdout);
	PluginMgr_ScanPlugins();
	printf("[xadmin:plugin] scan done\n");
	fflush(stdout);

	printf("[xadmin:plugin] load enabled begin\n");
	fflush(stdout);
	PluginMgr_LoadEnabledPlugins();
	printf("[xadmin:plugin] load enabled done\n");
	fflush(stdout);

	printf("[xadmin:plugin] emit ready begin\n");
	fflush(stdout);
	PluginCtx_EmitEvent(EVENT_SYSTEM_READY, NULL);
	printf("[xadmin:plugin] emit ready done\n");
	fflush(stdout);
}


// 闂佸簱鍋撴慨锝勭劍瑜板啯绂掔捄铏规澖濞撴艾顑囧▓鎴﹀炊閻愬墎娈堕柛鎴ｅГ閺?
bool PluginMgr_DestroyWalkProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	PluginInstance** ppPlugin = (PluginInstance**)pVal;
	if ( ppPlugin && *ppPlugin ) {
		Plugin_Destroy(*ppPlugin);
	}
	return FALSE;  // 缂備綀鍛暰闂侇剙绉村?
}

// 闁告鐡曞ù鍥箵閹哄秵顐界紒鐙呯磿閹﹪宕?
void PluginMgr_Unit()
{
	printf("        PluginMgr_Unit \n");
	
	// 閻熸瑱绠戣ぐ鍌滃寲閼姐倗鍩犻柛蹇斿▕濡瓨绂嶇€ｂ晜顐?
	PluginCtx_EmitEvent(EVENT_SYSTEM_SHUTDOWN, NULL);
	
	// 闁圭顦慨鐐存姜娴犲鈧孩鎯旇箛鏇熺暠闂侇偄妫楃花顓㈠础濮濆本绁伴柟缁樺笂濞?
	int iCount = xrtListCount(G_PluginMgr->lstLoadedPlugins);
	for ( int i = iCount - 1; i >= 0; i-- ) {
		PluginInstance* pPlugin = xrtListGetPtr(G_PluginMgr->lstLoadedPlugins, i);
		if ( pPlugin ) {
			Plugin_TccUnload(pPlugin);
		}
	}
	
	// 闂佸簱鍋撴慨锝勭劍婢у秹寮垫径瀣祷濞寸姾娉涢悿鍕瑹?
	xrtDictWalk(G_PluginMgr->tblPlugins, PluginMgr_DestroyWalkProc, NULL);
	
	// 闂佹彃锕ラ弬浣圭鐎ｂ晜顐介柣鈺傚灥閹宕?
	int iListenerCount = xrtListCount(G_PluginMgr->lstEventListeners);
	for ( int i = 0; i < iListenerCount; i++ ) {
		EventListener* pListener = xrtListGetPtr(G_PluginMgr->lstEventListeners, i);
		if ( pListener ) {
			if ( pListener->sEventName ) xrtFree(pListener->sEventName);
			if ( pListener->lstCallbacks ) xrtListDestroy(pListener->lstCallbacks);
			xrtFree(pListener);
		}
	}
	
	// 闂佹彃锕ラ弬渚€骞撻幒宥嗩偨缂佺媴绱曢幃濠囧闯?
	xrtDictDestroy(G_PluginMgr->tblPlugins);
	xrtListDestroy(G_PluginMgr->lstLoadedPlugins);
	xrtListDestroy(G_PluginMgr->lstEventListeners);
	xrtDictDestroy(G_PluginMgr->tblExports);
	xrtFree(G_PluginMgr);
	G_PluginMgr = NULL;
	
	// 闂佹彃锕ラ弬浣圭▔婵犱胶鐟撻柡?
	if ( G_PluginCtx ) {
		if ( G_PluginCtx->sDataPath ) xrtFree(G_PluginCtx->sDataPath);
		xrtFree(G_PluginCtx);
		G_PluginCtx = NULL;
	}
	
	// 闂佹彃锕ラ弬浣烘崉椤栨氨绐?
	if ( PluginPath ) xrtFree(PluginPath);
	if ( PluginDataPath ) xrtFree(PluginDataPath);
}



// ==================== 闁圭粯甯婂▎銏㈢不閿涘嫭鍊?API ====================

// 闁衡偓閸洘鑲犻柟缁樺笂濞嗐垽宕氬Δ鍕┾偓鍐儍閸曨偅绀€閻犲鍟崵閬嶅极?
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
	return FALSE;  // 缂備綀鍛暰闂侇剙绉村?
}

// 闁兼儳鍢茶ぐ鍥箵閹哄秵顐介柛鎺擃殙閵?
xvalue PluginMgr_GetList()
{
	xvalue arrList = xvoCreateArray();
	xrtDictWalk(G_PluginMgr->tblPlugins, PluginMgr_ListWalkProc, arrList);
	return arrList;
}


// 闁哄秷顫夊畵渚€宕ュ鍥嗙偤鎳㈠畡鏉跨悼闁圭粯甯婂▎?
PluginInstance* PluginMgr_GetPlugin(str sName)
{
	PluginInstance** ppPlugin = xrtDictGet(G_PluginMgr->tblPlugins, sName, strlen(sName));
	return ppPlugin ? *ppPlugin : NULL;
}


// 闁告凹鍨抽弫銈夊箰閸パ呮毎闁圭粯甯婂▎?
bool PluginMgr_EnablePlugin(str sName)
{
	PluginInstance* pPlugin = PluginMgr_GetPlugin(sName);
	if ( !pPlugin ) {
		return FALSE;
	}
	return Plugin_Enable(pPlugin);
}


// 缂佸倷鑳堕弫銈夊箰閸パ呮毎闁圭粯甯婂▎?
bool PluginMgr_DisablePlugin(str sName)
{
	PluginInstance* pPlugin = PluginMgr_GetPlugin(sName);
	if ( !pPlugin ) {
		return FALSE;
	}
	return Plugin_Disable(pPlugin);
}


// 闂佹彃绉峰ù鍥箰閸パ呮毎闁圭粯甯婂▎?
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



// ==================== 依赖管理功能 =============

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

// ==================== 闁圭鏅涢惈宥夊箳閵夈儱缍撻悗鍦仧楠?====================

// ===== 闁圭粯甯婂▎銏ゆ嚊椤忓洭鐓╁ǎ鍥ｅ墲娴?=====

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


// ===== 闁哄倸娲ｅ▎銏ゅ箼瀹ュ嫮绋?=====

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


// ===== xPack 闂傚棗妫欓崹?=====

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


// ===== 濞寸媴绲块悥婊堟偨閻旂鐏?=====

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


// ===== 闁轰胶澧楀畵浣规償閹惧瓨鎯欏ù?=====

bool PluginCtx_CreateTable(str tableName, str sql)
{
	if ( !tableName || !sql ) {
		return FALSE;
	}

	char* sErr = NULL;
	int iRet = sqlite3_exec(G_DB, sql, NULL, NULL, &sErr);

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
		sqlite3_exec(G_DB, sInsertSQL, NULL, NULL, NULL);
		xrtFree(sInsertSQL);
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

bool Plugin_LoadOrderLessThan(PluginInstance* pLeft, PluginInstance* pRight)
{
	if ( !pLeft ) {
		return FALSE;
	}
	if ( !pRight ) {
		return TRUE;
	}
	if ( pLeft->iSort != pRight->iSort ) {
		return pLeft->iSort < pRight->iSort;
	}
	if ( !pLeft->sName ) {
		return FALSE;
	}
	if ( !pRight->sName ) {
		return TRUE;
	}
	return strcmp(pLeft->sName, pRight->sName) < 0;
}

bool Plugin_DependenciesResolvedForSort(PluginInstance* pPlugin, xdict processed)
{
	if ( !pPlugin ) {
		return FALSE;
	}

	int iDepCount = pPlugin->lstDependencies ? xrtListCount(pPlugin->lstDependencies) : 0;
	for ( int i = 0; i < iDepCount; i++ ) {
		PluginDependency* pDep = xrtListGetPtr(pPlugin->lstDependencies, i);
		if ( !pDep || !pDep->sPluginName || strlen(pDep->sPluginName) == 0 ) {
			continue;
		}

		PluginInstance* pDepPlugin = PluginMgr_GetPlugin(pDep->sPluginName);
		if ( pDepPlugin && pDepPlugin->bEnabled ) {
			if ( !xrtDictExists(processed, pDepPlugin->sName, strlen(pDepPlugin->sName)) ) {
				return FALSE;
			}
		}
	}

	return TRUE;
}

bool Plugin_TopologicalSort(xlist* pResult)
{
	if ( !pResult || !(*pResult) ) {
		return FALSE;
	}

	xlist pending = xrtListCreate(sizeof(ptr), 0);
	xdict processed = xrtDictCreate(sizeof(char), 0);
	xrtDictWalk(G_PluginMgr->tblPlugins, PluginMgr_CollectEnabledProc, pending);

	while ( xrtListCount(pending) > 0 ) {
		int iBestIdx = -1;
		PluginInstance* pBest = NULL;
		int iPendingCount = xrtListCount(pending);

		for ( int i = 0; i < iPendingCount; i++ ) {
			PluginInstance* pPlugin = xrtListGetPtr(pending, i);
			if ( !Plugin_DependenciesResolvedForSort(pPlugin, processed) ) {
				continue;
			}
			if ( Plugin_LoadOrderLessThan(pPlugin, pBest) ) {
				pBest = pPlugin;
				iBestIdx = i;
			}
		}

		if ( iBestIdx < 0 || !pBest ) {
			xrtDictDestroy(processed);
			xrtListDestroy(pending);
			return FALSE;
		}

		xrtListSetPtr(*pResult, xrtListCount(*pResult), pBest, NULL);

		char* pDone = xrtDictSet(processed, pBest->sName, strlen(pBest->sName), NULL);
		if ( pDone ) {
			*pDone = 1;
		}

		xrtListRemove(pending, iBestIdx);
	}

	xrtDictDestroy(processed);
	xrtListDestroy(pending);
	return TRUE;
}

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

bool PluginCtx_DropTable(str tableName)
{
	if ( !tableName ) {
		return FALSE;
	}

	str sSQL = xrtFormat("DROP TABLE IF EXISTS %s", tableName);
	char* sErr = NULL;
	int iRet = sqlite3_exec(G_DB, sSQL, NULL, NULL, &sErr);
	xrtFree(sSQL);

	if ( iRet != SQLITE_OK ) {
		printf("        [Plugin] DropTable failed: %s\n", sErr);
		if ( sErr ) sqlite3_free(sErr);
		return FALSE;
	}

	str sDeleteSQL = xrtFormat("DELETE FROM plugin_table WHERE table_name = '%s'", tableName);
	sqlite3_exec(G_DB, sDeleteSQL, NULL, NULL, NULL);
	xrtFree(sDeleteSQL);

	return TRUE;
}

bool PluginCtx_ExecuteSQL(str sql)
{
	if ( !sql ) {
		return FALSE;
	}

	char* sErr = NULL;
	int iRet = sqlite3_exec(G_DB, sql, NULL, NULL, &sErr);

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
	int iRet = sqlite3_prepare_v3(G_DB, sql, -1, 0, &stmt, NULL);
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
	int iRet = sqlite3_prepare_v3(G_DB, sql, -1, 0, &stmt, NULL);

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


// ===== 闁圭粯甯婂▎銏㈢不閿涘嫭鍊?=====

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


// ===== 婵☆垪鍓濆妯恒€掗崣澶屽帬 =====

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
	xtetemplate hTemplate;
	XTE_Error tError = { 0 };
	size_t iRetSize = 0;

	if ( !templateString || !data ) {
		return NULL;
	}

	hTemplate = xteParseEx(NULL, templateString, strlen(templateString), NULL, &tError);
	if ( hTemplate == NULL ) {
		return xrtFormat("Template parse error");
	}

	str sResult = xteMake(hTemplate, data, tblENV, G_Template, &iRetSize);
	xteDestroyTemplate(hTemplate);
	return sResult;
}

#endif // PLUGIN_MGR_H


