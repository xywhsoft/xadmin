
static void BrandReplyJSON(XS_ResponseObject objResp, xvalue tblRet)
{
	size_t iRetSize = 0;
	char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
	http_reply(objResp, 200, "Content-Type: application/json\r\nCache-Control: no-store, no-cache, must-revalidate\r\nPragma: no-cache\r\nExpires: 0\r\n", sRet, iRetSize);
	xrtFree(sRet);
}

void Request_Admin_Brand(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblRet;
	xvalue tblData;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !HttpMethodIs(objReq, "GET") ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	tblRet = xvoCreateTable();
	tblData = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetText(tblRet, "message", 7, "ok", 0, FALSE);
	xvoTableSetText(tblData, "adminTitle", 10, Option_GetGlobalText("adminTitle", "xAdmin"), 0, FALSE);
	xvoTableSetText(tblData, "siteTitle", 9, Option_GetGlobalText("siteTitle", "xAdmin"), 0, FALSE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	BrandReplyJSON(objResp, tblRet);
	xvoUnref(tblRet);
}
