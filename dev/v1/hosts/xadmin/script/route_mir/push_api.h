


void Request_MIR_Push(XS_ServerObject objServer, XS_HostObject objHost,
	XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objSession;

	const char* sBody = (const char*)xsReqBody(objReq);
	if ( sBody == NULL ) {
		xsHttpReplyAuto(objResp, 400, HTTP_CT_TEXT, "missing body", 11);
		return;
	}

	size_t iBodyLen = xsReqBodyLen(objReq);

	xvalue objRoot = xrtParseJSON(sBody, iBodyLen);
	if ( objRoot == NULL || objRoot->Type != XVO_DT_TABLE ) {
		xsHttpReplyAuto(objResp, 400, HTTP_CT_TEXT, "invalid json", 12);
		if ( objRoot ) xvoUnref(objRoot);
		return;
	}

	const char* sCmd = xvoTableGetText(objRoot, "cmd", 3);
	const char* sAccount = xvoTableGetText(objRoot, "account", 7);
	const char* sPushBody = xvoTableGetText(objRoot, "body", 4);

	if ( sCmd == NULL || sAccount == NULL ) {
		xsHttpReplyAuto(objResp, 400, HTTP_CT_TEXT, "missing cmd or account", 22);
		xvoUnref(objRoot);
		return;
	}

	void* pStream = NULL;
	int64_t dataId = xsDataFindFirst("mir", "m2stream");
	if ( dataId > 0 ) {
		xvalue val = xsDataGet(dataId);
		if ( val && val->Type == XVO_DT_INT ) {
			pStream = (void*)(long long)xvoGetInt(val);
		}
	}

	if ( pStream == NULL ) {
		xsHttpReplyAuto(objResp, 503, HTTP_CT_TEXT, "m2 stream not connected", 23);
		xvoUnref(objRoot);
		return;
	}

	const char* arrKeys[2] = { "account", "body" };
	const char* arrVals[2] = { sAccount, sPushBody ? sPushBody : "" };

	int iRet = xsXtpSendPush(pStream, sCmd, strlen(sCmd), 2, arrKeys, arrVals, NULL, 0);
	if ( iRet == 0 ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_TEXT, "OK", 2);
	} else {
		char sErr[64];
		snprintf(sErr, sizeof(sErr), "push failed: %d", iRet);
		xsHttpReplyAuto(objResp, 500, HTTP_CT_TEXT, sErr, strlen(sErr));
	}

	xvoUnref(objRoot);
}
