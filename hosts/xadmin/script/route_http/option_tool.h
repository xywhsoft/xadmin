
static void OptionToolReplyJSON(XS_ResponseObject objResp, xvalue tblRet)
{
	size_t iRetSize = 0;
	char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
	http_reply(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
}

void Request_Option_AdminEntryGenerate(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblRet;
	xvalue tblData;
	str sPath;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !HttpMethodIs(objReq, "GET") ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	sPath = Option_GenerateAdminEntryPath();
	tblRet = xvoCreateTable();
	tblData = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetText(tblRet, "message", 7, "ok", 0, FALSE);
	xvoTableSetText(tblData, "path", 4, sPath ? sPath : (str)"", 0, FALSE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	OptionToolReplyJSON(objResp, tblRet);
	if ( sPath ) {
		xrtFree(sPath);
	}
	xvoUnref(tblRet);
}
