// frontend notify apis

static xvalue API_NotifyParseIDArray(xvalue tblForm)
{
	xvalue arrIDs = xvoCreateArray();

	if ( tblForm == NULL || tblForm->Type != XVO_DT_TABLE ) {
		return arrIDs;
	}

	if ( xvoTableItemType(tblForm, "ids", 3) == XVO_DT_ARRAY ) {
		xvalue arrInput = xvoTableGetValue(tblForm, "ids", 3);
		for ( uint32 i = 0; i < xvoArrayItemCount(arrInput); i++ ) {
			int64 id = xvoArrayGetInt(arrInput, i);
			if ( id > 0 && !XAdminIDArrayContainsInt(arrIDs, id) ) {
				xvoArrayAppendInt(arrIDs, id);
			}
		}
		return arrIDs;
	}

	if ( xvoTableItemType(tblForm, "id", 2) != XVO_DT_NULL ) {
		int64 id = xvoTableGetInt(tblForm, "id", 2);
		if ( id > 0 ) {
			xvoArrayAppendInt(arrIDs, id);
		}
		return arrIDs;
	}

	MemberMessage_ParseIDText(xvoTableGetText(tblForm, "ids", 3), arrIDs);
	return arrIDs;
}

void API_Notify_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sParam[64];
	int64 iPage = 1;
	int64 iLimit = 20;
	int64 iCount = 0;
	xvalue arrRet;
	size_t iJSONSize = 0;
	str sJSON;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}

	if ( xsReqQueryValue(objReq, "page", sParam, sizeof(sParam)) > 0 ) {
		iPage = xrtStrToI64(sParam);
	}
	if ( xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam)) > 0 ) {
		iLimit = xrtStrToI64(sParam);
	}

	arrRet = MemberMessage_MemberListNotify(xvoTableGetInt(objSession, "id", 2), iPage, iLimit, &iCount);
	sJSON = xrtStringifyJSON(arrRet, FALSE, &iJSONSize);
	xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"success\",\"count\":%lld,\"data\":%s}", iCount, sJSON);
	xrtFree(sJSON);
	xvoUnref(arrRet);
}

void API_Notify_UnreadCount(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int64 iCount;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}

	iCount = MemberMessage_MemberUnreadCount(xvoTableGetInt(objSession, "id", 2));
	xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"success\",\"data\":{\"unread\":%lld}}", iCount);
}

void API_Notify_Detail(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sParam[64];
	int64 iID = 0;
	xvalue tblRet;
	size_t iJSONSize = 0;
	str sJSON;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}

	if ( xsReqQueryValue(objReq, "id", sParam, sizeof(sParam)) > 0 ) {
		iID = xrtStrToI64(sParam);
	}
	if ( iID <= 0 ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"缺少有效的消息 ID\"}", 0);
		return;
	}

	tblRet = MemberMessage_MemberNotifyDetail(xvoTableGetInt(objSession, "id", 2), iID);
	if ( tblRet == NULL ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":404,\"msg\":\"消息不存在\"}", 0);
		return;
	}

	sJSON = xrtStringifyJSON(tblRet, FALSE, &iJSONSize);
	xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"success\",\"data\":%s}", sJSON);
	xrtFree(sJSON);
	xvoUnref(tblRet);
}

void API_Notify_Read(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm;
	xvalue arrIDs;
	bool bOK;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}

	tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( tblForm == NULL || tblForm->Type != XVO_DT_TABLE ) {
		if ( tblForm ) xvoUnref(tblForm);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"无效的请求数据\"}", 0);
		return;
	}

	arrIDs = API_NotifyParseIDArray(tblForm);
	bOK = MemberMessage_MarkNotifyRead(xvoTableGetInt(objSession, "id", 2), arrIDs, FALSE);
	xvoUnref(arrIDs);
	xvoUnref(tblForm);

	if ( bOK ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"success\"}", 0);
	} else {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"没有可标记的消息\"}", 0);
	}
}

void API_Notify_ReadAll(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}

	MemberMessage_MarkNotifyRead(xvoTableGetInt(objSession, "id", 2), NULL, TRUE);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"success\"}", 0);
}

void API_Notify_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm;
	xvalue arrIDs;
	bool bOK;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}

	tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( tblForm == NULL || tblForm->Type != XVO_DT_TABLE ) {
		if ( tblForm ) xvoUnref(tblForm);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"无效的请求数据\"}", 0);
		return;
	}

	arrIDs = API_NotifyParseIDArray(tblForm);
	bOK = MemberMessage_DeleteNotify(xvoTableGetInt(objSession, "id", 2), arrIDs);
	xvoUnref(arrIDs);
	xvoUnref(tblForm);

	if ( bOK ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"success\"}", 0);
	} else {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"没有可删除的消息\"}", 0);
	}
}
