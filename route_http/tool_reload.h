/* v1 重载工具页面与四个动作。页面与 JSON 契约原样保留（fetch 只消费 message/展示整包）。
 * Submit 契约换成 xs3 期望状态协调器：id 非零即已受理，终态可用 xsReloadQuery 查询；
 * data.id 为新增字段，code/queued/reloadCount 维持 v1 形状。 */
static void ToolReloadReplyJSON(XS_ResponseObject objResp, xvalue* tblRet)
{
	size_t iRetSize = 0;
	char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
}

/* v1 动作同时接受 GET/POST（页面按钮走 POST）。 */
static bool ToolReloadActionAllowed(XS_RequestObject objReq)
{
	return (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ||
	       (xsReqMethodID(objReq) == XHTTP_METHOD_POST);
}

static void ToolReloadReplySubmit(XS_ResponseObject objResp, XS_ReloadId id, const char* sOK, const char* sFail, bool with_reload_count)
{
	xvalue* tblRet = ValueObject();
	xvalue* tblData = ValueObject();
	ValueSetBool(tblRet, "result", id != 0);
	ValueSetText(tblRet, "message", (str)(id ? sOK : sFail));
	ValueSetInt(tblData, "code", id ? 0 : 1);
	ValueSetBool(tblData, "queued", id != 0);
	ValueSetInt(tblData, "id", (int64)id);
	if ( with_reload_count ) {
		ValueSetInt(tblData, "reloadCount", 0);
	}
	ValueSetOwn(tblRet, "data", tblData);
	ToolReloadReplyJSON(objResp, tblRet);
	xrtValueRelease(tblRet);
}

void Request_View_Tool_Reload(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	LoadPage(objResp, 200, HTTP_CT_HTML, "tool/reload.html");
}

void Request_Tool_Reload_Template(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* tblRet;
	xvalue* tblData;
	uint32 iLoaded = 0;
	uint32 iFailed = 0;
	bool bOK;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !ToolReloadActionAllowed(objReq) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	bOK = Template_RebuildCache(&iLoaded, &iFailed);
	tblRet = ValueObject();
	tblData = ValueObject();
	ValueSetBool(tblRet, "result", bOK);
	ValueSetText(tblRet, "message", bOK ? "template cache rebuilt" : "template cache rebuild failed");
	ValueSetInt(tblData, "loaded", (int64)iLoaded);
	ValueSetInt(tblData, "failed", (int64)iFailed);
	ValueSetInt(tblData, "total", (int64)(iLoaded + iFailed));
	ValueSetOwn(tblRet, "data", tblData);
	ToolReloadReplyJSON(objResp, tblRet);
	xrtValueRelease(tblRet);
}

void Request_Tool_Reload_Host(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	XS_ReloadId iId;

	(void)objServer;
	(void)objSession;

	if ( !ToolReloadActionAllowed(objReq) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	iId = xsReloadHostSubmit((XS_HostInfo*)objHost);
	ToolReloadReplySubmit(objResp, iId, "current host reload queued", "current host reload queue failed", false);
}

void Request_Tool_Reload_Server(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	XS_ReloadId iId;

	(void)objServer;
	(void)objSession;

	if ( !ToolReloadActionAllowed(objReq) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	iId = (objHost && objHost->Server) ? xsReloadServerSubmit(objHost->Server->Name) : 0;
	ToolReloadReplySubmit(objResp, iId, "current server reload queued", "current server reload queue failed", true);
}

void Request_Tool_Reload_XS(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	XS_ReloadId iId;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !ToolReloadActionAllowed(objReq) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	iId = xsReloadAllSubmit();
	ToolReloadReplySubmit(objResp, iId, "xs reload queued", "xs reload queue failed", true);
}
