// admin routes for member messages

static void MemberMessageReplyAdminTable(XS_ResponseObject objResp, xvalue arrData, int64 iCount)
{
	xvalue tblRet = xvoCreateTable();
	size_t iRetSize = 0;
	char* sRet;

	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetInt(tblRet, "code", 4, 0);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
	xvoUnref(tblRet);
}

void Request_View_Member_Notify(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		LoadPage(objResp, 200, HTTP_CT_HTML, "member/notify.html");
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_View_Member_Notify_Send(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		LoadPage(objResp, 200, HTTP_CT_HTML, "member/notify_send.html");
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_View_Member_Mail(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		LoadPage(objResp, 200, HTTP_CT_HTML, "member/mail_task.html");
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_View_Member_Mail_Send(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		LoadPage(objResp, 200, HTTP_CT_HTML, "member/mail_send.html");
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_Member_Notify(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		char sParam[128];
		int64 iPage = 1;
		int64 iLimit = 20;
		int64 iCount = 0;
		int64 iTaskID = 0;
		xvalue arrData;
		const char* sSearch = NULL;

		if ( xsReqQueryValue(objReq, "page", sParam, sizeof(sParam)) > 0 ) {
			iPage = xrtStrToI64(sParam);
		}
		if ( xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam)) > 0 ) {
			iLimit = xrtStrToI64(sParam);
		}
		if ( xsReqQueryValue(objReq, "search", sParam, sizeof(sParam)) > 0 ) {
			sSearch = sParam;
		}

		arrData = MemberMessage_AdminListNotify(iPage, iLimit, sSearch, &iCount);
		MemberMessageReplyAdminTable(objResp, arrData, iCount);
		return;
	}

	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		str sTitle;
		str sContent;
		str sActionURL;
		str sSendType;
		str sMemberIDs;
		str sGroupIDs;
		str sMessage = NULL;
		int64 iMessageID = 0;
		int64 iRecipientCount = 0;
		bool bOK;

		if ( tblForm == NULL || tblForm->Type != XVO_DT_TABLE ) {
			if ( tblForm ) xvoUnref(tblForm);
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据\"}", 0);
			return;
		}

		sTitle = xvoTableGetText(tblForm, "title", 5);
		sContent = xvoTableGetText(tblForm, "content", 7);
		sActionURL = xvoTableGetText(tblForm, "actionUrl", 9);
		sSendType = xvoTableGetText(tblForm, "sendType", 8);
		sMemberIDs = xvoTableGetText(tblForm, "memberIds", 9);
		sGroupIDs = xvoTableGetText(tblForm, "groupIds", 8);

		bOK = MemberMessage_SendNotify(
			sSendType ? sSendType : (str)"users",
			sMemberIDs,
			sGroupIDs,
			sTitle,
			sContent,
			sActionURL,
			xvoTableGetInt(objSession, "id", 2),
			&sMessage,
			&iMessageID,
			&iRecipientCount
		);
		xvoUnref(tblForm);

		if ( bOK ) {
			xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"%s\", \"data\": {\"id\": %lld, \"recipientCount\": %lld}}",
				MemberMessage_StrOrEmpty(sMessage), iMessageID, iRecipientCount);
		} else {
			xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"%s\"}", MemberMessage_StrOrEmpty(sMessage));
		}
		if ( sMessage ) {
			xrtFree(sMessage);
		}
		return;
	}

	LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

void Request_Member_Mail(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		char sParam[128];
		int64 iPage = 1;
		int64 iLimit = 20;
		int64 iCount = 0;
		int64 iTaskID = 0;
		xvalue arrData;
		const char* sSearch = NULL;

		if ( xsReqQueryValue(objReq, "page", sParam, sizeof(sParam)) > 0 ) {
			iPage = xrtStrToI64(sParam);
		}
		if ( xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam)) > 0 ) {
			iLimit = xrtStrToI64(sParam);
		}
		if ( xsReqQueryValue(objReq, "search", sParam, sizeof(sParam)) > 0 ) {
			sSearch = sParam;
		}
		if ( xsReqQueryValue(objReq, "taskId", sParam, sizeof(sParam)) > 0 ) {
			iTaskID = xrtStrToI64(sParam);
		}

		arrData = MemberMessage_AdminListMailTasks(iPage, iLimit, sSearch, iTaskID, &iCount);
		MemberMessageReplyAdminTable(objResp, arrData, iCount);
		return;
	}

	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		str sSubject;
		str sContent;
		str sSendType;
		str sMemberIDs;
		str sGroupIDs;
		str sMessage = NULL;
		int64 iCreateCount = 0;
		int64 iSkipCount = 0;
		bool bOK;

		if ( tblForm == NULL || tblForm->Type != XVO_DT_TABLE ) {
			if ( tblForm ) xvoUnref(tblForm);
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据\"}", 0);
			return;
		}

		sSubject = xvoTableGetText(tblForm, "subject", 7);
		sContent = xvoTableGetText(tblForm, "content", 7);
		sSendType = xvoTableGetText(tblForm, "sendType", 8);
		sMemberIDs = xvoTableGetText(tblForm, "memberIds", 9);
		sGroupIDs = xvoTableGetText(tblForm, "groupIds", 8);

		bOK = MemberMessage_CreateMailTasks(
			sSendType ? sSendType : (str)"users",
			sMemberIDs,
			sGroupIDs,
			sSubject,
			sContent,
			&sMessage,
			&iCreateCount,
			&iSkipCount
		);
		xvoUnref(tblForm);

		if ( bOK ) {
			xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"%s\", \"data\": {\"created\": %lld, \"skipped\": %lld}}",
				MemberMessage_StrOrEmpty(sMessage), iCreateCount, iSkipCount);
		} else {
			xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"%s\"}", MemberMessage_StrOrEmpty(sMessage));
		}
		if ( sMessage ) {
			xrtFree(sMessage);
		}
		return;
	}

	LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

void Request_Member_Mail_Status(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblStatus;
	char* sJson;
	size_t iJsonSize = 0;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	tblStatus = xvoCreateTable();
	xvoTableSetBool(tblStatus, "result", 6, TRUE);
	xvoTableSetValue(tblStatus, "data", 4, MemberMessage_GetMailQueueStatus(), TRUE);
	sJson = xrtStringifyJSON(tblStatus, FALSE, &iJsonSize);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sJson, iJsonSize);
	xrtFree(sJson);
	xvoUnref(tblStatus);
}

void Request_Member_Mail_RunPending(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	const char* sMessage = NULL;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	if ( !MemberMessage_GetGlobalBool("mail_enabled", FALSE) ) {
		xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON,
			"{\"result\": false, \"message\": \"%s\"}",
			"邮件功能未启用");
		return;
	}
	if ( !MemberMessage_GetGlobalBool("mail_queue_enabled", TRUE) ) {
		xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON,
			"{\"result\": false, \"message\": \"%s\"}",
			"邮件队列未启用");
		return;
	}

	MemberMessage_RequestMailQueueRun();
	if ( MemberMessage_IsMailQueueRunning() ) {
		sMessage = "邮件队列正在后台执行，请稍后刷新列表";
	} else {
		sMessage = "已触发邮件队列执行，请稍后刷新列表";
	}
	xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON,
		"{\"result\": true, \"message\": \"%s\"}",
		sMessage);
}

void Request_Member_Mail_Retry(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sParam[64];
	int64 iTaskID = 0;
	str sMessage = NULL;
	bool bOK;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	if ( xsReqQueryValue(objReq, "id", sParam, sizeof(sParam)) > 0 ) {
		iTaskID = xrtStrToI64(sParam);
	}
	if ( iTaskID <= 0 ) {
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm && tblForm->Type == XVO_DT_TABLE ) {
			iTaskID = xvoTableGetInt(tblForm, "id", 2);
		}
		if ( tblForm ) {
			xvoUnref(tblForm);
		}
	}

	bOK = MemberMessage_RetryMailTask(iTaskID, &sMessage);
	xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON,
		"{\"result\": %s, \"message\": \"%s\"}",
		bOK ? "true" : "false",
		MemberMessage_StrOrEmpty(sMessage));
	if ( sMessage ) {
		xrtFree(sMessage);
	}
}

void Request_Member_Mail_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sParam[64];
	int64 iTaskID = 0;
	str sMessage = NULL;
	bool bOK;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	if ( xsReqQueryValue(objReq, "id", sParam, sizeof(sParam)) > 0 ) {
		iTaskID = xrtStrToI64(sParam);
	}
	if ( iTaskID <= 0 ) {
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm && tblForm->Type == XVO_DT_TABLE ) {
			iTaskID = xvoTableGetInt(tblForm, "id", 2);
		}
		if ( tblForm ) {
			xvoUnref(tblForm);
		}
	}

	bOK = MemberMessage_DeleteMailTask(iTaskID, &sMessage);
	xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON,
		"{\"result\": %s, \"message\": \"%s\"}",
		bOK ? "true" : "false",
		MemberMessage_StrOrEmpty(sMessage));
	if ( sMessage ) {
		xrtFree(sMessage);
	}
}
