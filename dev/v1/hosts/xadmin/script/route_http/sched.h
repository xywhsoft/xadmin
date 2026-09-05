static bool Sched_ReadIdFromBody(XS_RequestObject objReq, int64* pID, int* pEnabled)
{
	xvalue tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));

	if ( pID ) *pID = 0;
	if ( pEnabled ) *pEnabled = 0;
	if ( tblBody == NULL ) {
		return FALSE;
	}

	if ( pID ) *pID = xvoTableGetInt(tblBody, "id", 2);
	if ( pEnabled ) *pEnabled = xvoTableGetInt(tblBody, "enabled", 7);
	xvoUnref(tblBody);
	return TRUE;
}

static bool Sched_ReadBatchBody(XS_RequestObject objReq, xvalue* pArrIDs, int* pEnabled)
{
	xvalue tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	xvalue arrIDs = NULL;
	xvalue arrCopy = NULL;
	uint32 i;

	if ( pArrIDs ) *pArrIDs = NULL;
	if ( pEnabled ) *pEnabled = 0;
	if ( tblBody == NULL || xvoType(tblBody) != XVO_DT_TABLE ) {
		if ( tblBody ) xvoUnref(tblBody);
		return FALSE;
	}

	arrIDs = xvoTableGetValue(tblBody, "ids", 3);
	if ( arrIDs == NULL || xvoType(arrIDs) != XVO_DT_ARRAY || xvoArrayItemCount(arrIDs) <= 0 ) {
		xvoUnref(tblBody);
		return FALSE;
	}

	arrCopy = xvoCreateArray();
	for ( i = 0; i < xvoArrayItemCount(arrIDs); i++ ) {
		xvoArrayAppendInt(arrCopy, xvoArrayGetInt(arrIDs, i));
	}

	if ( pEnabled ) *pEnabled = xvoTableGetInt(tblBody, "enabled", 7);
	if ( pArrIDs ) {
		*pArrIDs = arrCopy;
		arrCopy = NULL;
	}
	if ( arrCopy ) xvoUnref(arrCopy);
	xvoUnref(tblBody);
	return TRUE;
}

static str RequestSched_StrOrEmpty(str sText)
{
	return sText ? sText : (str)"";
}

static int64 RequestSched_ParseTimeText(const char* sText)
{
	if ( sText == NULL || sText[0] == '\0' ) {
		return 0;
	}
	return xrtStrToTime((str)sText, strlen(sText));
}

void Request_View_Sched(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "sched/list.html");
}

void Request_View_Sched_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "sched/edit.html");
}

void Request_View_Sched_Log(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "sched/log.html");
}

void Request_View_Sched_Dashboard(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "sched/dashboard.html");
}

void Request_Sched_Tasks(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sParam[128];
	int64 iPage = 1;
	int64 iLimit = 20;
	char sSearchRaw[128] = {0};
	str sSearch = NULL;
	xvalue tblRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	xsReqQueryValue(objReq, "page", sParam, sizeof(sParam));
	if ( sParam[0] ) iPage = xrtStrToI64(sParam);
	xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam));
	if ( sParam[0] ) iLimit = xrtStrToI64(sParam);
	xsReqQueryValue(objReq, "search", sSearchRaw, sizeof(sSearchRaw));
	if ( sSearchRaw[0] ) {
		sSearch = xrtFormat("%%%s%%", sSearchRaw);
	}

	tblRet = Sched_ListTasks(iPage, iLimit, sSearch);
	if ( sSearch ) xrtFree(sSearch);
	Sched_SendJson(objResp, tblRet);
}

void Request_Sched_Task(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		char sID[32];
		int64 iID;
		xvalue tblTask;
		xvalue tblRet;

		xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		iID = xrtStrToI64(sID);
		tblTask = Sched_GetTask(iID);
		if ( tblTask == NULL ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Task not found\"}", 0);
			return;
		}

		tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetValue(tblRet, "data", 4, tblTask, TRUE);
		Sched_SendJson(objResp, tblRet);
		return;
	}

	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_POST) || (xsReqMethodID(objReq) == XHTTPD_METHOD_PUT) ) {
		xvalue tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		xvalue tblRet;
		str sMessage = NULL;
		bool bRet;
		int64 iTaskId = 0;

		if ( tblBody == NULL ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid request body\"}", 0);
			return;
		}

		bRet = Sched_SaveTaskRequest(tblBody, (xsReqMethodID(objReq) == XHTTPD_METHOD_PUT), &sMessage, &iTaskId);
		xvoUnref(tblBody);

		tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, bRet);
		xvoTableSetText(tblRet, "message", 7, RequestSched_StrOrEmpty(sMessage), 0, FALSE);
		xvoTableSetInt(tblRet, "id", 2, iTaskId);
		if ( sMessage ) xrtFree(sMessage);
		Sched_SendJson(objResp, tblRet);
		return;
	}

	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_DELETE) ) {
		char sID[32];
		int64 iID;
		xvalue tblRet;
		str sMessage = NULL;
		bool bRet;

		xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		iID = xrtStrToI64(sID);
		bRet = Sched_DeleteTask(iID, &sMessage);
		tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, bRet);
		xvoTableSetText(tblRet, "message", 7, RequestSched_StrOrEmpty(sMessage), 0, FALSE);
		if ( sMessage ) xrtFree(sMessage);
		Sched_SendJson(objResp, tblRet);
		return;
	}

	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
}

void Request_Sched_Task_Enable(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int64 iID = 0;
	int iEnabled = 0;
	xvalue tblRet;
	str sMessage = NULL;
	bool bRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) || !Sched_ReadIdFromBody(objReq, &iID, &iEnabled) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid request body\"}", 0);
		return;
	}

	bRet = Sched_SetEnabled(iID, iEnabled ? TRUE : FALSE, &sMessage);
	tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, bRet);
	xvoTableSetText(tblRet, "message", 7, RequestSched_StrOrEmpty(sMessage), 0, FALSE);
	if ( sMessage ) xrtFree(sMessage);
	Sched_SendJson(objResp, tblRet);
}

void Request_Sched_Task_Run(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int64 iID = 0;
	xvalue tblRet;
	str sMessage = NULL;
	bool bRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) || !Sched_ReadIdFromBody(objReq, &iID, NULL) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid request body\"}", 0);
		return;
	}

	bRet = Sched_RunNow(iID, &sMessage);
	tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, bRet);
	xvoTableSetText(tblRet, "message", 7, RequestSched_StrOrEmpty(sMessage), 0, FALSE);
	if ( sMessage ) xrtFree(sMessage);
	Sched_SendJson(objResp, tblRet);
}

void Request_Sched_Task_Copy(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int64 iID = 0;
	xvalue tblRet;
	str sMessage = NULL;
	int64 iTaskId = 0;
	bool bRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) || !Sched_ReadIdFromBody(objReq, &iID, NULL) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid request body\"}", 0);
		return;
	}

	bRet = Sched_CopyTask(iID, &sMessage, &iTaskId);
	tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, bRet);
	xvoTableSetText(tblRet, "message", 7, RequestSched_StrOrEmpty(sMessage), 0, FALSE);
	xvoTableSetInt(tblRet, "id", 2, iTaskId);
	if ( sMessage ) xrtFree(sMessage);
	Sched_SendJson(objResp, tblRet);
}

void Request_Sched_Task_Batch_Enable(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue arrIDs = NULL;
	xvalue tblRet;
	str sMessage = NULL;
	int iEnabled = 0;
	int64 iAffected = 0;
	bool bRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) || !Sched_ReadBatchBody(objReq, &arrIDs, &iEnabled) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid request body\"}", 0);
		return;
	}

	bRet = Sched_SetEnabledBatch(arrIDs, iEnabled ? TRUE : FALSE, &iAffected, &sMessage);
	xvoUnref(arrIDs);

	tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, bRet);
	xvoTableSetText(tblRet, "message", 7, RequestSched_StrOrEmpty(sMessage), 0, FALSE);
	xvoTableSetInt(tblRet, "affected", 8, iAffected);
	if ( sMessage ) xrtFree(sMessage);
	Sched_SendJson(objResp, tblRet);
}

void Request_Sched_Task_Batch_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue arrIDs = NULL;
	xvalue tblRet;
	str sMessage = NULL;
	int64 iAffected = 0;
	bool bRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) || !Sched_ReadBatchBody(objReq, &arrIDs, NULL) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid request body\"}", 0);
		return;
	}

	bRet = Sched_DeleteTaskBatch(arrIDs, &iAffected, &sMessage);
	xvoUnref(arrIDs);

	tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, bRet);
	xvoTableSetText(tblRet, "message", 7, RequestSched_StrOrEmpty(sMessage), 0, FALSE);
	xvoTableSetInt(tblRet, "affected", 8, iAffected);
	if ( sMessage ) xrtFree(sMessage);
	Sched_SendJson(objResp, tblRet);
}

void Request_Sched_Task_Batch_Run(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue arrIDs = NULL;
	xvalue tblRet;
	str sMessage = NULL;
	int64 iQueued = 0;
	int64 iSkipped = 0;
	int64 iFailed = 0;
	bool bRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) || !Sched_ReadBatchBody(objReq, &arrIDs, NULL) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid request body\"}", 0);
		return;
	}

	bRet = Sched_RunNowBatch(arrIDs, &iQueued, &iSkipped, &iFailed, &sMessage);
	xvoUnref(arrIDs);

	tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, bRet);
	xvoTableSetText(tblRet, "message", 7, RequestSched_StrOrEmpty(sMessage), 0, FALSE);
	xvoTableSetInt(tblRet, "queued", 6, iQueued);
	xvoTableSetInt(tblRet, "skipped", 7, iSkipped);
	xvoTableSetInt(tblRet, "failed", 6, iFailed);
	if ( sMessage ) xrtFree(sMessage);
	Sched_SendJson(objResp, tblRet);
}

void Request_Sched_Logs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sParam[64];
	char sStatus[64] = {0};
	char sKeywordRaw[128] = {0};
	char sStartFrom[32] = {0};
	char sStartTo[32] = {0};
	int64 iPage = 1;
	int64 iLimit = 20;
	int64 iTaskId = 0;
	int64 iStartFromTime = 0;
	int64 iStartToTime = 0;
	str sKeyword = NULL;
	xvalue tblRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	xsReqQueryValue(objReq, "page", sParam, sizeof(sParam));
	if ( sParam[0] ) iPage = xrtStrToI64(sParam);
	xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam));
	if ( sParam[0] ) iLimit = xrtStrToI64(sParam);
	xsReqQueryValue(objReq, "taskId", sParam, sizeof(sParam));
	if ( sParam[0] ) iTaskId = xrtStrToI64(sParam);
	xsReqQueryValue(objReq, "status", sStatus, sizeof(sStatus));
	if ( strcmp(sStatus, "all") == 0 ) {
		sStatus[0] = '\0';
	}
	xsReqQueryValue(objReq, "keyword", sKeywordRaw, sizeof(sKeywordRaw));
	if ( sKeywordRaw[0] ) {
		sKeyword = xrtCopyStr(sKeywordRaw, 0);
	}
	xsReqQueryValue(objReq, "startFrom", sStartFrom, sizeof(sStartFrom));
	xsReqQueryValue(objReq, "startTo", sStartTo, sizeof(sStartTo));
	iStartFromTime = RequestSched_ParseTimeText(sStartFrom);
	iStartToTime = RequestSched_ParseTimeText(sStartTo);

	tblRet = Sched_ListLogs(iTaskId, iPage, iLimit, sStatus, sKeyword, iStartFromTime, iStartToTime);
	if ( sKeyword ) xrtFree(sKeyword);
	Sched_SendJson(objResp, tblRet);
}

void Request_Sched_Logs_Export(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sParam[64];
	char sStatus[64] = {0};
	char sKeywordRaw[128] = {0};
	char sStartFrom[32] = {0};
	char sStartTo[32] = {0};
	int64 iTaskId = 0;
	int64 iStartFromTime = 0;
	int64 iStartToTime = 0;
	str sKeyword = NULL;
	xvalue tblRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}

	xsReqQueryValue(objReq, "taskId", sParam, sizeof(sParam));
	if ( sParam[0] ) iTaskId = xrtStrToI64(sParam);
	xsReqQueryValue(objReq, "status", sStatus, sizeof(sStatus));
	if ( strcmp(sStatus, "all") == 0 ) {
		sStatus[0] = '\0';
	}
	xsReqQueryValue(objReq, "keyword", sKeywordRaw, sizeof(sKeywordRaw));
	if ( sKeywordRaw[0] ) {
		sKeyword = xrtCopyStr(sKeywordRaw, 0);
	}
	xsReqQueryValue(objReq, "startFrom", sStartFrom, sizeof(sStartFrom));
	xsReqQueryValue(objReq, "startTo", sStartTo, sizeof(sStartTo));
	iStartFromTime = RequestSched_ParseTimeText(sStartFrom);
	iStartToTime = RequestSched_ParseTimeText(sStartTo);

	tblRet = Sched_ExportLogs(iTaskId, sStatus, sKeyword, iStartFromTime, iStartToTime);
	if ( sKeyword ) xrtFree(sKeyword);
	Sched_SendJson(objResp, tblRet);
}

void Request_Sched_Logs_Clear(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody;
	xvalue tblRet;
	int64 iTaskId = 0;
	int64 iAffected = 0;
	int64 iStartFromTime = 0;
	int64 iStartToTime = 0;
	char sStatus[64] = {0};
	str sKeyword = NULL;
	str sStartFrom = NULL;
	str sStartTo = NULL;
	str sMessage = NULL;
	bool bRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}

	tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( tblBody == NULL ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid request body\"}", 0);
		return;
	}

	iTaskId = xvoTableGetInt(tblBody, "taskId", 6);
	snprintf(sStatus, sizeof(sStatus), "%s", RequestSched_StrOrEmpty((str)xvoTableGetText(tblBody, "status", 6)));
	if ( strcmp(sStatus, "all") == 0 ) {
		sStatus[0] = '\0';
	}
	if ( xvoTableGetText(tblBody, "keyword", 7) ) {
		sKeyword = xrtCopyStr(xvoTableGetText(tblBody, "keyword", 7), 0);
	}
	if ( xvoTableGetText(tblBody, "startFrom", 9) ) {
		sStartFrom = xrtCopyStr(xvoTableGetText(tblBody, "startFrom", 9), 0);
	}
	if ( xvoTableGetText(tblBody, "startTo", 7) ) {
		sStartTo = xrtCopyStr(xvoTableGetText(tblBody, "startTo", 7), 0);
	}
	iStartFromTime = RequestSched_ParseTimeText(sStartFrom);
	iStartToTime = RequestSched_ParseTimeText(sStartTo);

	bRet = Sched_ClearLogs(iTaskId, sStatus, sKeyword, iStartFromTime, iStartToTime, &iAffected, &sMessage);
	xvoUnref(tblBody);
	if ( sKeyword ) xrtFree(sKeyword);
	if ( sStartFrom ) xrtFree(sStartFrom);
	if ( sStartTo ) xrtFree(sStartTo);

	tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, bRet);
	xvoTableSetText(tblRet, "message", 7, RequestSched_StrOrEmpty(sMessage), 0, FALSE);
	xvoTableSetInt(tblRet, "affected", 8, iAffected);
	if ( sMessage ) xrtFree(sMessage);
	Sched_SendJson(objResp, tblRet);
}

void Request_Sched_Export(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sID[32] = {0};
	char sSearchRaw[128] = {0};
	int64 iTaskId = 0;
	str sSearch = NULL;
	xvalue tblRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}

	xsReqQueryValue(objReq, "id", sID, sizeof(sID));
	if ( sID[0] ) iTaskId = xrtStrToI64(sID);
	xsReqQueryValue(objReq, "search", sSearchRaw, sizeof(sSearchRaw));
	if ( sSearchRaw[0] ) {
		sSearch = xrtFormat("%%%s%%", sSearchRaw);
	}

	tblRet = Sched_ExportTasks(iTaskId, sSearch);
	if ( sSearch ) xrtFree(sSearch);
	Sched_SendJson(objResp, tblRet);
}

void Request_Sched_Export_Selected(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue arrIDs = NULL;
	xvalue tblRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) || !Sched_ReadBatchBody(objReq, &arrIDs, NULL) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid request body\"}", 0);
		return;
	}

	tblRet = Sched_ExportTasksSelected(arrIDs);
	xvoUnref(arrIDs);
	Sched_SendJson(objResp, tblRet);
}

void Request_Sched_Import(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody;
	xvalue tblRet;
	str sMessage = NULL;
	int64 iImported = 0;
	int64 iSkipped = 0;
	int64 iFailed = 0;
	bool bRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}

	tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( tblBody == NULL ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid request body\"}", 0);
		return;
	}

	bRet = Sched_ImportTasks(tblBody, &iImported, &iSkipped, &iFailed, &sMessage);
	xvoUnref(tblBody);

	tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, bRet);
	xvoTableSetText(tblRet, "message", 7, RequestSched_StrOrEmpty(sMessage), 0, FALSE);
	xvoTableSetInt(tblRet, "count", 5, iImported);
	xvoTableSetInt(tblRet, "skipped", 7, iSkipped);
	xvoTableSetInt(tblRet, "failed", 6, iFailed);
	if ( sMessage ) xrtFree(sMessage);
	Sched_SendJson(objResp, tblRet);
}

void Request_Sched_Preview(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody;
	xvalue tblRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}

	tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( tblBody == NULL ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid request body\"}", 0);
		return;
	}

	tblRet = Sched_PreviewTask(tblBody, 5, NULL);
	xvoUnref(tblBody);
	Sched_SendJson(objResp, tblRet);
}

void Request_Sched_Dashboard(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}

	tblRet = Sched_DashboardData();
	Sched_SendJson(objResp, tblRet);
}

void Request_Sched_Task_Example(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody;
	xvalue tblRet;
	str sKind;
	str sMessage = NULL;
	int64 iTaskId = 0;
	bool bRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}

	tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( tblBody == NULL ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid request body\"}", 0);
		return;
	}

	sKind = xvoTableGetText(tblBody, "kind", 4);
	bRet = Sched_CreateExampleTask(sKind, &sMessage, &iTaskId);
	xvoUnref(tblBody);

	tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, bRet);
	xvoTableSetText(tblRet, "message", 7, RequestSched_StrOrEmpty(sMessage), 0, FALSE);
	xvoTableSetInt(tblRet, "id", 2, iTaskId);
	if ( sMessage ) xrtFree(sMessage);
	Sched_SendJson(objResp, tblRet);
}
