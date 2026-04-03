void PluginRoute_SendJson(XS_ResponseObject objResp, xvalue tblRet)
{
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(objResp, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}

int PluginRoute_FindMenuIdByHref(const char* sHref)
{
	sqlite3_stmt* stmt = NULL;
	int iMenuId = 0;

	if ( (G_DB == NULL) || (sHref == NULL) || (sHref[0] == '\0') ) {
		return 0;
	}
	if ( sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE href = ? AND isDelete = 0 ORDER BY id DESC LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return 0;
	}

	sqlite3_bind_text(stmt, 1, sHref, -1, NULL);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iMenuId = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return iMenuId;
}

int PluginRoute_FindMenuIdByTitleParent(const char* sTitle, int iParentId)
{
	sqlite3_stmt* stmt = NULL;
	int iMenuId = 0;

	if ( (G_DB == NULL) || (sTitle == NULL) || (sTitle[0] == '\0') ) {
		return 0;
	}
	if ( sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE parent = ? AND title = ? AND isDelete = 0 ORDER BY id DESC LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return 0;
	}

	sqlite3_bind_int(stmt, 1, iParentId);
	sqlite3_bind_text(stmt, 2, sTitle, -1, NULL);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iMenuId = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return iMenuId;
}

bool PluginRoute_SaveMenu(int iMenuId, int iParentId, const char* sTitle, const char* sIcon, int iType, const char* sOpenType, const char* sHref, int iSort, int iVisible, const char* sRemark)
{
	sqlite3_stmt* stmt = NULL;
	int64 iNow = xrtNow();

	if ( (G_DB == NULL) || (sTitle == NULL) || (sTitle[0] == '\0') ) {
		return FALSE;
	}

	if ( iMenuId > 0 ) {
		if ( sqlite3_prepare_v3(G_DB, "UPDATE menu SET parent = ?, title = ?, icon = ?, type = ?, openType = ?, href = ?, sort = ?, visible = ?, remark = ?, updateTime = ?, isDelete = 0 WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return FALSE;
		}
		sqlite3_bind_int(stmt, 1, iParentId);
		sqlite3_bind_text(stmt, 2, sTitle, -1, NULL);
		sqlite3_bind_text(stmt, 3, sIcon ? sIcon : "", -1, NULL);
		sqlite3_bind_int(stmt, 4, iType);
		sqlite3_bind_text(stmt, 5, sOpenType ? sOpenType : "", -1, NULL);
		sqlite3_bind_text(stmt, 6, sHref ? sHref : "", -1, NULL);
		sqlite3_bind_int(stmt, 7, iSort);
		sqlite3_bind_int(stmt, 8, iVisible ? 1 : 0);
		sqlite3_bind_text(stmt, 9, sRemark ? sRemark : "", -1, NULL);
		sqlite3_bind_int64(stmt, 10, iNow);
		sqlite3_bind_int(stmt, 11, iMenuId);
	} else {
		if ( sqlite3_prepare_v3(G_DB, "INSERT INTO menu (parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return FALSE;
		}
		sqlite3_bind_int(stmt, 1, iParentId);
		sqlite3_bind_text(stmt, 2, sTitle, -1, NULL);
		sqlite3_bind_text(stmt, 3, sIcon ? sIcon : "", -1, NULL);
		sqlite3_bind_int(stmt, 4, iType);
		sqlite3_bind_text(stmt, 5, sOpenType ? sOpenType : "", -1, NULL);
		sqlite3_bind_text(stmt, 6, sHref ? sHref : "", -1, NULL);
		sqlite3_bind_int(stmt, 7, iSort);
		sqlite3_bind_int(stmt, 8, iVisible ? 1 : 0);
		sqlite3_bind_text(stmt, 9, sRemark ? sRemark : "", -1, NULL);
		sqlite3_bind_int64(stmt, 10, iNow);
		sqlite3_bind_int64(stmt, 11, iNow);
	}

	if ( sqlite3_step(stmt) != SQLITE_DONE ) {
		sqlite3_finalize(stmt);
		return FALSE;
	}

	sqlite3_finalize(stmt);
	return TRUE;
}

void PluginRoute_SoftDeleteMenuByHrefExcept(const char* sHref, int iKeepId)
{
	sqlite3_stmt* stmt = NULL;

	if ( (G_DB == NULL) || (sHref == NULL) || (sHref[0] == '\0') ) {
		return;
	}
	if ( sqlite3_prepare_v3(G_DB, "UPDATE menu SET isDelete = 1, updateTime = ? WHERE href = ? AND isDelete = 0 AND id <> ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return;
	}

	sqlite3_bind_int64(stmt, 1, xrtNow());
	sqlite3_bind_text(stmt, 2, sHref, -1, NULL);
	sqlite3_bind_int(stmt, 3, iKeepId);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

void PluginRoute_EnsureMenus(void)
{
	int iRootMenuId = 0;
	int iStoreMenuId = 0;
	int iInstalledMenuId = 0;

	if ( G_DB == NULL ) {
		return;
	}

	iRootMenuId = PluginRoute_FindMenuIdByTitleParent("插件管理", 0);
	if ( !PluginRoute_SaveMenu(iRootMenuId, 0, "插件管理", "layui-icon layui-icon-app", 0, "", "", 550000, 1, "插件管理目录") ) {
		return;
	}
	if ( iRootMenuId <= 0 ) {
		iRootMenuId = PluginRoute_FindMenuIdByTitleParent("插件管理", 0);
	}
	if ( iRootMenuId <= 0 ) {
		return;
	}

	iStoreMenuId = PluginRoute_FindMenuIdByHref("/admin/view/plugin/store");
	if ( iStoreMenuId <= 0 ) {
		iStoreMenuId = PluginRoute_FindMenuIdByTitleParent("插件商店", iRootMenuId);
	}
	PluginRoute_SaveMenu(iStoreMenuId, iRootMenuId, "插件商店", "layui-icon layui-icon-cart-simple", 1, "_component", "/admin/view/plugin/store", 550100, 1, "浏览远程插件商店");

	iInstalledMenuId = PluginRoute_FindMenuIdByHref("/admin/view/plugin/installed");
	if ( iInstalledMenuId <= 0 ) {
		iInstalledMenuId = PluginRoute_FindMenuIdByHref("/admin/view/plugin");
	}
	if ( iInstalledMenuId <= 0 ) {
		iInstalledMenuId = PluginRoute_FindMenuIdByTitleParent("已安装插件", iRootMenuId);
	}
	if ( PluginRoute_SaveMenu(iInstalledMenuId, iRootMenuId, "已安装插件", "layui-icon layui-icon-component", 1, "_component", "/admin/view/plugin/installed", 550200, 1, "查看和管理已安装插件") ) {
		if ( iInstalledMenuId <= 0 ) {
			iInstalledMenuId = PluginRoute_FindMenuIdByHref("/admin/view/plugin/installed");
		}
	}

	PluginRoute_SoftDeleteMenuByHrefExcept("/admin/view/plugin", 0);
	PluginRoute_SoftDeleteMenuByHrefExcept("/admin/view/plugin/installed", iInstalledMenuId);
}

bool PluginRoute_ReadNameFromBody(XS_RequestObject objReq, char* sName, size_t iCap)
{
	xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	str sValue;

	if ( sName && (iCap > 0) ) {
		sName[0] = '\0';
	}
	if ( tblForm == NULL ) {
		return FALSE;
	}

	sValue = xvoTableGetText(tblForm, "name", 4);
	if ( sValue && sName && (iCap > 0) ) {
		snprintf(sName, iCap, "%s", sValue);
	}
	xvoUnref(tblForm);
	return (sName && (sName[0] != '\0'));
}

void Request_Plugin_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue arrList = PluginSystem_GetList();
	xvalue tblRet = xvoCreateTable();

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	xvoTableSetInt(tblRet, "code", 4, 0);
	xvoTableSetText(tblRet, "msg", 3, "", 0, FALSE);
	xvoTableSetInt(tblRet, "count", 5, xvoArrayItemCount(arrList));
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	PluginRoute_SendJson(objResp, tblRet);
}

void Request_Plugin_Get(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sName[128];
	xvalue tblData;
	xvalue tblRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	HttpGetQueryVar(objReq, "name", sName, sizeof(sName));
	if ( sName[0] == '\0' ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing plugin xid\"}", 0);
		return;
	}

	tblData = PluginSystem_GetPackageData(sName);
	if ( tblData == NULL ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Plugin not found\"}", 0);
		return;
	}

	tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	PluginRoute_SendJson(objResp, tblRet);
}

void Request_Plugin_Enable(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sName[128] = {0};
	bool bResult;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !HttpMethodIs(objReq, "POST") ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	if ( !PluginRoute_ReadNameFromBody(objReq, sName, sizeof(sName)) ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing plugin xid\"}", 0);
		return;
	}

	bResult = PluginSystem_Enable(sName);
	http_reply(objResp, 200, HTTP_CT_JSON, bResult ? "{\"result\":true,\"message\":\"Plugin enabled\"}" : "{\"result\":false,\"message\":\"Failed to enable plugin\"}", 0);
}

void Request_Plugin_Disable(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sName[128] = {0};
	bool bResult;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !HttpMethodIs(objReq, "POST") ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	if ( !PluginRoute_ReadNameFromBody(objReq, sName, sizeof(sName)) ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing plugin xid\"}", 0);
		return;
	}

	bResult = PluginSystem_Disable(sName);
	http_reply(objResp, 200, HTTP_CT_JSON, bResult ? "{\"result\":true,\"message\":\"Plugin disabled\"}" : "{\"result\":false,\"message\":\"Failed to disable plugin\"}", 0);
}

void Request_Plugin_Reload(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sName[128] = {0};
	bool bResult;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !HttpMethodIs(objReq, "POST") ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	if ( !PluginRoute_ReadNameFromBody(objReq, sName, sizeof(sName)) ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing plugin xid\"}", 0);
		return;
	}

	bResult = PluginSystem_Reload(sName);
	http_reply(objResp, 200, HTTP_CT_JSON, bResult ? "{\"result\":true,\"message\":\"Plugin reloaded\"}" : "{\"result\":false,\"message\":\"Failed to reload plugin\"}", 0);
}

void Request_Plugin_Settings(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( HttpMethodIs(objReq, "GET") ) {
		char sName[128] = {0};
		xvalue tblRet;
		xvalue tblSettings;

		HttpGetQueryVar(objReq, "name", sName, sizeof(sName));
		if ( sName[0] == '\0' ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing plugin xid\"}", 0);
			return;
		}

		tblSettings = PluginSystem_GetSettings(sName);
		tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetValue(tblRet, "data", 4, tblSettings, TRUE);
		PluginRoute_SendJson(objResp, tblRet);
		return;
	}

	if ( HttpMethodIs(objReq, "POST") ) {
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		str sName;
		xvalue tblSettings;
		bool bResult;

		if ( tblForm == NULL ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
			return;
		}

		sName = xvoTableGetText(tblForm, "name", 4);
		tblSettings = xvoTableGetValue(tblForm, "settings", 8);
		if ( (sName == NULL) || (sName[0] == '\0') ) {
			xvoUnref(tblForm);
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing plugin xid\"}", 0);
			return;
		}

		bResult = PluginSystem_SaveSettings(sName, tblSettings);
		xvoUnref(tblForm);
		http_reply(objResp, 200, HTTP_CT_JSON, bResult ? "{\"result\":true,\"message\":\"Settings saved\"}" : "{\"result\":false,\"message\":\"Failed to save settings\"}", 0);
		return;
	}

	http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
}

void Request_View_Plugin_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	LoadPage(objResp, 200, HTTP_CT_HTML, "plugin/list.html");
}

void Request_View_Plugin_Store(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	LoadPage(objResp, 200, HTTP_CT_HTML, "plugin/store.html");
}
