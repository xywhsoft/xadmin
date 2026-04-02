void PluginRoute_SendJson(XS_ResponseObject objResp, xvalue tblRet)
{
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(objResp, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
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
