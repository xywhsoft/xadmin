static void TemplateReplyJSON(XS_ResponseObject objResp, xvalue tblRet)
{
	size_t iRetSize = 0;
	char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
	http_reply(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
}

void Request_Template_Rebuild(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
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
	xvoTableSetText(tblRet, "message", 7, bOK ? (str)"template cache rebuilt" : (str)"template cache rebuild failed", 0, FALSE);
	xvoTableSetInt(tblData, "loaded", 6, (int64)iLoaded);
	xvoTableSetInt(tblData, "failed", 6, (int64)iFailed);
	xvoTableSetInt(tblData, "total", 5, (int64)(iLoaded + iFailed));
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	TemplateReplyJSON(objResp, tblRet);
	xvoUnref(tblRet);
}
