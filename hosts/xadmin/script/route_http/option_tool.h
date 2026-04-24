
static void OptionToolReplyJSON(XS_ResponseObject objResp, xvalue tblRet)
{
	size_t iRetSize = 0;
	char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
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

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
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

void Request_Option_SMTPTest(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblReq = NULL;
	xvalue tblRet = NULL;
	xvalue tblData = NULL;
	xvalue tblConfig = NULL;
	str sToEmail = NULL;
	str sMessage = NULL;
	bool bOK;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	tblReq = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( tblReq == NULL || xvoType(tblReq) != XVO_DT_TABLE ) {
		if ( tblReq ) xvoUnref(tblReq);
		xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"%s\"}", "Invalid request body");
		return;
	}

	sToEmail = xvoTableGetText(tblReq, "toEmail", 7);
	tblConfig = xvoTableGetValue(tblReq, "data", 4);
	bOK = MemberMessage_TestSMTPWithData(tblConfig, sToEmail, &sMessage);

	tblRet = xvoCreateTable();
	tblData = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, bOK);
	xvoTableSetText(tblRet, "message", 7, sMessage ? sMessage : (str)"", 0, FALSE);
	xvoTableSetText(tblData, "toEmail", 7, sToEmail ? sToEmail : (str)"", 0, FALSE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	OptionToolReplyJSON(objResp, tblRet);

	if ( sMessage ) {
		xrtFree(sMessage);
	}
	xvoUnref(tblReq);
	xvoUnref(tblRet);
}
