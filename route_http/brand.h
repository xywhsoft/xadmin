
static void BrandReplyJSON(XS_ResponseObject objResp, xvalue* tblRet)
{
	size_t iRetSize = 0;
	char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
	xsHttpReplyAuto(objResp, 200, "Content-Type: application/json\r\nCache-Control: no-store, no-cache, must-revalidate\r\nPragma: no-cache\r\nExpires: 0\r\n", sRet, iRetSize);
	xrtFree(sRet);
}

void Request_Admin_Brand(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* tblRet;
	xvalue* tblData;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	tblRet = ValueObject();
	tblData = ValueObject();
	ValueSetBool(tblRet, "result", true);
	ValueSetText(tblRet, "message", "ok");
	ValueSetText(tblData, "adminTitle", Option_GetGlobalText("adminTitle", "xAdmin"));
	ValueSetText(tblData, "siteTitle", Option_GetGlobalText("siteTitle", "xAdmin"));
	ValueSetOwn(tblRet, "data", tblData);
	BrandReplyJSON(objResp, tblRet);
	xrtValueRelease(tblRet);
}
