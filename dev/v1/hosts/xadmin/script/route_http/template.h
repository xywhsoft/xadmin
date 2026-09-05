static void TemplateReplyJSON(XS_ResponseObject objResp, xvalue tblRet)
{
	xsHttpJsonValueTake(objResp, 200, tblRet);
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

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) && !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
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
}

void Request_View_Template_Form_Demo(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	size_t iSize = 0;
	xvalue tblData;
	str sPage;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	tblData = xvoCreateTable();
	sPage = MakePageWithTemplate("form/block_demo.html", tblData, &iSize);
	xvoUnref(tblData);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iSize);
	xrtFree(sPage);
}
