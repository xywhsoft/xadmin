static void ToolReloadReplyJSON(XS_ResponseObject objResp, xvalue tblRet)
{
	size_t iRetSize = 0;
	char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
	http_reply(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
}

void Request_View_Tool_Reload(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	LoadPage(objResp, 200, HTTP_CT_HTML, "tool/reload.html");
}

void Request_Tool_Reload_Template(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblRet;
	xvalue tblData;
	uint32 iLoaded = 0;
	uint32 iFailed = 0;
	bool bOK;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !HttpMethodIs(objReq, "POST") && !HttpMethodIs(objReq, "GET") ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	bOK = Template_RebuildCache(&iLoaded, &iFailed);
	tblRet = xvoCreateTable();
	tblData = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, bOK);
	xvoTableSetText(tblRet, "message", 7, bOK ? "template cache rebuilt" : "template cache rebuild failed", 0, FALSE);
	xvoTableSetInt(tblData, "loaded", 6, (int64)iLoaded);
	xvoTableSetInt(tblData, "failed", 6, (int64)iFailed);
	xvoTableSetInt(tblData, "total", 5, (int64)(iLoaded + iFailed));
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	ToolReloadReplyJSON(objResp, tblRet);
	xvoUnref(tblRet);
}

void Request_Tool_Reload_Host(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblRet;
	xvalue tblData;
	int iRet;
	bool bOK;

	(void)objSession;

	if ( !HttpMethodIs(objReq, "POST") && !HttpMethodIs(objReq, "GET") ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	iRet = xsRequestReloadCurrentHost(objServer, objHost, 1);
	bOK = (iRet == 0);
	tblRet = xvoCreateTable();
	tblData = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, bOK);
	xvoTableSetText(tblRet, "message", 7, bOK ? "current host reload queued" : "current host reload queue failed", 0, FALSE);
	xvoTableSetInt(tblData, "code", 4, (int64)iRet);
	xvoTableSetBool(tblData, "queued", 6, bOK);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	ToolReloadReplyJSON(objResp, tblRet);
	xvoUnref(tblRet);
}

void Request_Tool_Reload_Server(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblRet;
	xvalue tblData;
	int iRet;
	bool bOK;

	(void)objHost;
	(void)objSession;

	if ( !HttpMethodIs(objReq, "POST") && !HttpMethodIs(objReq, "GET") ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	iRet = xsRequestReloadServer(objServer, 1);
	bOK = (iRet == 0);
	tblRet = xvoCreateTable();
	tblData = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, bOK);
	xvoTableSetText(tblRet, "message", 7, bOK ? "current server reload queued" : "current server reload queue failed", 0, FALSE);
	xvoTableSetInt(tblData, "reloadCount", 11, 0);
	xvoTableSetInt(tblData, "code", 4, (int64)iRet);
	xvoTableSetBool(tblData, "queued", 6, bOK);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	ToolReloadReplyJSON(objResp, tblRet);
	xvoUnref(tblRet);
}

void Request_Tool_Reload_XS(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblRet;
	xvalue tblData;
	int iRet;
	bool bOK;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !HttpMethodIs(objReq, "POST") && !HttpMethodIs(objReq, "GET") ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	iRet = xsRequestReloadAllServer(1);
	bOK = (iRet == 0);

	tblRet = xvoCreateTable();
	tblData = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, bOK);
	xvoTableSetText(tblRet, "message", 7, bOK ? "xs reload queued" : "xs reload queue failed", 0, FALSE);
	xvoTableSetInt(tblData, "reloadCount", 11, 0);
	xvoTableSetInt(tblData, "code", 4, (int64)iRet);
	xvoTableSetBool(tblData, "queued", 6, bOK);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	ToolReloadReplyJSON(objResp, tblRet);
	xvoUnref(tblRet);
}
