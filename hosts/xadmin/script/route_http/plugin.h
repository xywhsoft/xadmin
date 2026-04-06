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
	const char* sRootTitle = "\xE6\x8F\x92\xE4\xBB\xB6\xE7\xAE\xA1\xE7\x90\x86";
	const char* sRootRemark = "\xE6\x8F\x92\xE4\xBB\xB6\xE7\xAE\xA1\xE7\x90\x86\xE7\x9B\xAE\xE5\xBD\x95";
	const char* sStoreTitle = "\xE6\x8F\x92\xE4\xBB\xB6\xE5\x95\x86\xE5\xBA\x97";
	const char* sStoreRemark = "\xE6\xB5\x8F\xE8\xA7\x88\xE8\xBF\x9C\xE7\xA8\x8B\xE6\x8F\x92\xE4\xBB\xB6\xE5\x95\x86\xE5\xBA\x97";
	const char* sInstalledTitle = "\xE5\xB7\xB2\xE5\xAE\x89\xE8\xA3\x85\xE6\x8F\x92\xE4\xBB\xB6";
	const char* sInstalledRemark = "\xE6\x9F\xA5\xE7\x9C\x8B\xE5\x92\x8C\xE7\xAE\xA1\xE7\x90\x86\xE5\xB7\xB2\xE5\xAE\x89\xE8\xA3\x85\xE6\x8F\x92\xE4\xBB\xB6";

	if ( G_DB == NULL ) {
		return;
	}

	iRootMenuId = PluginRoute_FindMenuIdByTitleParent(sRootTitle, 0);
	if ( !PluginRoute_SaveMenu(iRootMenuId, 0, sRootTitle, "layui-icon layui-icon-app", 0, "", "", 550000, 1, sRootRemark) ) {
		return;
	}
	if ( iRootMenuId <= 0 ) {
		iRootMenuId = PluginRoute_FindMenuIdByTitleParent(sRootTitle, 0);
	}
	if ( iRootMenuId <= 0 ) {
		return;
	}

	iStoreMenuId = PluginRoute_FindMenuIdByHref("/admin/view/plugin/store");
	if ( iStoreMenuId <= 0 ) {
		iStoreMenuId = PluginRoute_FindMenuIdByTitleParent(sStoreTitle, iRootMenuId);
	}
	PluginRoute_SaveMenu(iStoreMenuId, iRootMenuId, sStoreTitle, "layui-icon layui-icon-cart-simple", 1, "_component", "/admin/view/plugin/store", 550100, 1, sStoreRemark);

	iInstalledMenuId = PluginRoute_FindMenuIdByHref("/admin/view/plugin/installed");
	if ( iInstalledMenuId <= 0 ) {
		iInstalledMenuId = PluginRoute_FindMenuIdByHref("/admin/view/plugin");
	}
	if ( iInstalledMenuId <= 0 ) {
		iInstalledMenuId = PluginRoute_FindMenuIdByTitleParent(sInstalledTitle, iRootMenuId);
	}
	if ( PluginRoute_SaveMenu(iInstalledMenuId, iRootMenuId, sInstalledTitle, "layui-icon layui-icon-component", 1, "_component", "/admin/view/plugin/installed", 550200, 1, sInstalledRemark) ) {
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

bool PluginRoute_TextIsTrue(const char* sText, size_t iLen)
{
	char sValue[16];
	size_t iCopyLen;

	if ( sText == NULL || iLen == 0 ) {
		return FALSE;
	}

	iCopyLen = (iLen < sizeof(sValue) - 1) ? iLen : (sizeof(sValue) - 1);
	memcpy(sValue, sText, iCopyLen);
	sValue[iCopyLen] = '\0';
	for ( size_t i = 0; i < iCopyLen; i++ ) {
		if ( sValue[i] >= 'A' && sValue[i] <= 'Z' ) {
			sValue[i] = (char)(sValue[i] + 32);
		}
	}

	return strcmp(sValue, "1") == 0
		|| strcmp(sValue, "true") == 0
		|| strcmp(sValue, "yes") == 0
		|| strcmp(sValue, "on") == 0;
}

void PluginRoute_SendResult(XS_ResponseObject objResp, bool bResult, const char* sMessage, const char* sXid)
{
	xvalue tblRet = xvoCreateTable();

	xvoTableSetBool(tblRet, "result", 6, bResult);
	xvoTableSetText(tblRet, "message", 7, (str)(sMessage ? sMessage : ""), 0, FALSE);
	if ( sXid && sXid[0] ) {
		xvoTableSetText(tblRet, "xid", 3, (str)sXid, 0, FALSE);
	}
	PluginRoute_SendJson(objResp, tblRet);
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
		PluginRoute_SendResult(objResp, FALSE, "\xE8\xAF\xB7\xE6\xB1\x82\xE6\x96\xB9\xE6\xB3\x95\xE4\xB8\x8D\xE5\x85\x81\xE8\xAE\xB8", NULL);
		return;
	}
	if ( !PluginRoute_ReadNameFromBody(objReq, sName, sizeof(sName)) ) {
		PluginRoute_SendResult(objResp, FALSE, "\xE7\xBC\xBA\xE5\xB0\x91\xE6\x8F\x92\xE4\xBB\xB6 xid", NULL);
		return;
	}

	bResult = PluginSystem_Enable(sName);
	PluginRoute_SendResult(objResp, bResult, bResult ? "\xE6\x8F\x92\xE4\xBB\xB6\xE5\xB7\xB2\xE5\x90\xAF\xE7\x94\xA8" : "\xE5\x90\xAF\xE7\x94\xA8\xE6\x8F\x92\xE4\xBB\xB6\xE5\xA4\xB1\xE8\xB4\xA5", NULL);
}

void Request_Plugin_Disable(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sName[128] = {0};
	bool bResult;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !HttpMethodIs(objReq, "POST") ) {
		PluginRoute_SendResult(objResp, FALSE, "\xE8\xAF\xB7\xE6\xB1\x82\xE6\x96\xB9\xE6\xB3\x95\xE4\xB8\x8D\xE5\x85\x81\xE8\xAE\xB8", NULL);
		return;
	}
	if ( !PluginRoute_ReadNameFromBody(objReq, sName, sizeof(sName)) ) {
		PluginRoute_SendResult(objResp, FALSE, "\xE7\xBC\xBA\xE5\xB0\x91\xE6\x8F\x92\xE4\xBB\xB6 xid", NULL);
		return;
	}

	bResult = PluginSystem_Disable(sName);
	PluginRoute_SendResult(objResp, bResult, bResult ? "\xE6\x8F\x92\xE4\xBB\xB6\xE5\xB7\xB2\xE7\xA6\x81\xE7\x94\xA8" : "\xE7\xA6\x81\xE7\x94\xA8\xE6\x8F\x92\xE4\xBB\xB6\xE5\xA4\xB1\xE8\xB4\xA5", NULL);
}

void Request_Plugin_Reload(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sName[128] = {0};
	bool bResult;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !HttpMethodIs(objReq, "POST") ) {
		PluginRoute_SendResult(objResp, FALSE, "\xE8\xAF\xB7\xE6\xB1\x82\xE6\x96\xB9\xE6\xB3\x95\xE4\xB8\x8D\xE5\x85\x81\xE8\xAE\xB8", NULL);
		return;
	}
	if ( !PluginRoute_ReadNameFromBody(objReq, sName, sizeof(sName)) ) {
		PluginRoute_SendResult(objResp, FALSE, "\xE7\xBC\xBA\xE5\xB0\x91\xE6\x8F\x92\xE4\xBB\xB6 xid", NULL);
		return;
	}

	bResult = PluginSystem_Reload(sName);
	PluginRoute_SendResult(objResp, bResult, bResult ? "\xE6\x8F\x92\xE4\xBB\xB6\xE5\xB7\xB2\xE9\x87\x8D\xE8\xBD\xBD" : "\xE9\x87\x8D\xE8\xBD\xBD\xE6\x8F\x92\xE4\xBB\xB6\xE5\xA4\xB1\xE8\xB4\xA5", NULL);
}

void Request_Plugin_Export(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sName[128] = {0};
	ptr pData = NULL;
	size_t iSize = 0;
	str sFileName = NULL;
	str sError = NULL;
	str sDisposition = NULL;
	const char* sErrorText;
	const char* sFileNameText;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !HttpMethodIs(objReq, "GET") ) {
		PluginRoute_SendResult(objResp, FALSE, "\xE8\xAF\xB7\xE6\xB1\x82\xE6\x96\xB9\xE6\xB3\x95\xE4\xB8\x8D\xE5\x85\x81\xE8\xAE\xB8", NULL);
		return;
	}

	HttpGetQueryVar(objReq, "name", sName, sizeof(sName));
	if ( sName[0] == '\0' ) {
		PluginRoute_SendResult(objResp, FALSE, "\xE7\xBC\xBA\xE5\xB0\x91\xE6\x8F\x92\xE4\xBB\xB6 xid", NULL);
		return;
	}

	if ( !PluginSystem_ExportPackageArchive(sName, &pData, &iSize, &sFileName, &sError) ) {
		sErrorText = sError ? (const char*)sError : "\xE5\xAF\xBC\xE5\x87\xBA\xE6\x8F\x92\xE4\xBB\xB6\xE5\xA4\xB1\xE8\xB4\xA5";
		PluginRoute_SendResult(objResp, FALSE, sErrorText, NULL);
		if ( sError ) {
			xrtFree(sError);
		}
		if ( sFileName ) {
			xrtFree(sFileName);
		}
		return;
	}

	sFileNameText = sFileName ? (const char*)sFileName : "plugin.xpk";
	sDisposition = xrtFormat("attachment; filename=\"%s\"", sFileNameText);
	xsHttpStatus(objResp, 200, "OK");
	if ( sDisposition ) {
		xsHttpHeader(objResp, "Content-Disposition", sDisposition);
	}
	xsHttpHeader(objResp, "Cache-Control", "no-store");
	xsHttpBody(objResp, pData, iSize, "application/octet-stream");

	if ( sDisposition ) {
		xrtFree(sDisposition);
	}
	if ( sFileName ) {
		xrtFree(sFileName);
	}
	if ( pData ) {
		xrtFree(pData);
	}
}

void Request_Plugin_Import(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	HttpMultipartPart part;
	size_t iOffset = 0;
	ptr pFileData = NULL;
	size_t iFileSize = 0;
	bool bAutoEnable = FALSE;
	str sImportedXid = NULL;
	str sError = NULL;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !HttpMethodIs(objReq, "POST") ) {
		PluginRoute_SendResult(objResp, FALSE, "\xE8\xAF\xB7\xE6\xB1\x82\xE6\x96\xB9\xE6\xB3\x95\xE4\xB8\x8D\xE5\x85\x81\xE8\xAE\xB8", NULL);
		return;
	}

	while ( HttpMultipartNext(objReq, &iOffset, &part) ) {
		if ( HttpMultipartNameIs(&part, "file") ) {
			pFileData = (ptr)part.pBody;
			iFileSize = part.iBodyLen;
		} else if ( HttpMultipartNameIs(&part, "autoEnable") ) {
			bAutoEnable = PluginRoute_TextIsTrue((const char*)part.pBody, part.iBodyLen);
		}
	}

	if ( pFileData == NULL || iFileSize == 0 ) {
		PluginRoute_SendResult(objResp, FALSE, "\xE6\x9C\xAA\xE4\xB8\x8A\xE4\xBC\xA0\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85", NULL);
		return;
	}

	if ( !PluginSystem_ImportPackageBuffer(pFileData, iFileSize, bAutoEnable, &sImportedXid, &sError) ) {
		const char* sErrorText = sError ? (const char*)sError : "\xE5\xAF\xBC\xE5\x85\xA5\xE6\x8F\x92\xE4\xBB\xB6\xE5\xA4\xB1\xE8\xB4\xA5";
		PluginRoute_SendResult(objResp, FALSE, sErrorText, sImportedXid ? (const char*)sImportedXid : NULL);
		if ( sError ) {
			xrtFree(sError);
		}
		if ( sImportedXid ) {
			xrtFree(sImportedXid);
		}
		return;
	}

	PluginRoute_SendResult(objResp, TRUE, "\xE6\x8F\x92\xE4\xBB\xB6\xE5\xAF\xBC\xE5\x85\xA5\xE6\x88\x90\xE5\x8A\x9F", sImportedXid ? (const char*)sImportedXid : NULL);

	if ( sImportedXid ) {
		xrtFree(sImportedXid);
	}
	if ( sError ) {
		xrtFree(sError);
	}
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
