#include "../content/content_generator.h"
#include "../content/content_model.h"
#include "../content/content_advisor.h"
#include "../content/content_revision.h"
#include "../content/content_generation.h"

void Content_ReplyJsonValue(XS_ResponseObject objResp, xvalue tblData)
{
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblData, FALSE, &iSize);
	if ( sJson ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sJson, iSize);
		xrtFree(sJson);
	}
	if ( tblData ) {
		xvoUnref(tblData);
	}
}

void Content_ReplyError(XS_ResponseObject objResp, const char* sMessage)
{
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, FALSE);
	xvoTableSetText(tblRet, "message", 7, (str)(sMessage ? sMessage : "error"), 0, FALSE);
	xvoTableSetText(tblRet, "errorCode", 9, "CONTENT_ERROR", 0, FALSE);
	xvoTableSetValue(tblRet, "data", 4, xvoCreateNull(), TRUE);
	Content_ReplyJsonValue(objResp, tblRet);
}

void Content_ReplySuccess(XS_ResponseObject objResp, xvalue tblData)
{
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetText(tblRet, "message", 7, "ok", 0, FALSE);
	xvoTableSetText(tblRet, "errorCode", 9, "", 0, FALSE);
	if ( tblData ) {
		xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	} else {
		xvoTableSetValue(tblRet, "data", 4, xvoCreateNull(), TRUE);
	}
	Content_ReplyJsonValue(objResp, tblRet);
}

void Request_View_Content_Index(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "content/index.html");
}

void Request_View_Content_Editor(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "content/editor.html");
}

void Request_Content_Capabilities(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	Content_ReplySuccess(objResp, ContentCapability_ListActive());
}

void Request_Content_Types(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblData;
	char sPage[32] = {0};
	char sLimit[32] = {0};
	int iPage = 1;
	int iLimit = 20;

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	xsReqQueryValue(objReq, "page", sPage, sizeof(sPage));
	xsReqQueryValue(objReq, "limit", sLimit, sizeof(sLimit));
	iPage = (int)xrtStrToI64(sPage);
	iLimit = (int)xrtStrToI64(sLimit);

	tblData = Content_ListModels(iPage, iLimit);
	Content_ReplySuccess(objResp, tblData);
}

void Request_Content_Type(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sXid[128] = {0};
	xvalue tblModel;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		Content_ReplyError(objResp, "method not allowed");
		return;
	}
	xsReqQueryValue(objReq, "xid", sXid, sizeof(sXid));
	if ( !Content_IsValidXid(sXid) ) {
		Content_ReplyError(objResp, "invalid xid");
		return;
	}

	tblModel = Content_GetModelByXid(sXid);
	if ( tblModel ) {
		Content_ReplySuccess(objResp, tblModel);
		return;
	}
	Content_ReplyError(objResp, "model not found");
}

void Request_Content_Save(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblSpec;
	xvalue tblData;
	char* sError = NULL;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		Content_ReplyError(objResp, "method not allowed");
		return;
	}

	tblSpec = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( (tblSpec == NULL) || (xvoType(tblSpec) != XVO_DT_TABLE) ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		Content_ReplyError(objResp, "invalid json body");
		return;
	}

	tblData = Content_SaveModelSpec(tblSpec, &sError);
	if ( tblData == NULL ) {
		xvoUnref(tblSpec);
		Content_ReplyError(objResp, sError ? sError : "save model failed");
		if ( sError ) xrtFree(sError);
		return;
	}
	Content_ReplySuccess(objResp, tblData);
	xvoUnref(tblSpec);
}

void Request_Content_Revisions(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sXid[128] = {0};
	int iModelId = 0;
	int iRevision = 0;
	xvalue arrRows;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	xsReqQueryValue(objReq, "xid", sXid, sizeof(sXid));
	if ( !Content_IsValidXid(sXid) || !Content_FindModelIdAndRevision(sXid, &iModelId, &iRevision) ) {
		Content_ReplyError(objResp, "model not found");
		return;
	}
	arrRows = Content_ListRevisions(iModelId);
	Content_ReplySuccess(objResp, arrRows);
}

void Request_Content_Generations(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sXid[128] = {0};
	int iModelId = 0;
	int iRevision = 0;
	xvalue arrRows;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	xsReqQueryValue(objReq, "xid", sXid, sizeof(sXid));
	if ( !Content_IsValidXid(sXid) || !Content_FindModelIdAndRevision(sXid, &iModelId, &iRevision) ) {
		Content_ReplyError(objResp, "model not found");
		return;
	}
	arrRows = Content_ListGenerations(iModelId);
	Content_ReplySuccess(objResp, arrRows);
}

void Request_Content_Advisor(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = NULL;
	xvalue tblRet;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( xsReqMethodID(objReq) == XHTTPD_METHOD_POST ) {
		tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	}
	tblRet = Content_BuildAdvisor(tblBody);
	Content_ReplySuccess(objResp, tblRet);
	if ( tblBody ) {
		xvoUnref(tblBody);
	}
}

void Request_Content_Generate(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = NULL;
	str sXid = NULL;
	char sQueryXid[128] = {0};
	xvalue tblRet;
	char* sError = NULL;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( xsReqMethodID(objReq) == XHTTPD_METHOD_POST ) {
		tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblBody && (xvoType(tblBody) == XVO_DT_TABLE) ) {
			sXid = xvoTableGetText(tblBody, "xid", 3);
		}
	} else {
		xsReqQueryValue(objReq, "xid", sQueryXid, sizeof(sQueryXid));
		sXid = sQueryXid;
	}

	if ( !Content_IsValidXid(sXid) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Content_ReplyError(objResp, "invalid xid");
		return;
	}

	tblRet = Content_GeneratePluginForModel(sXid, &sError);
	if ( tblRet ) {
		Content_ReplySuccess(objResp, tblRet);
	} else {
		Content_ReplyError(objResp, sError ? sError : "generate plugin failed");
	}

	if ( tblBody ) xvoUnref(tblBody);
	if ( sError ) xrtFree(sError);
}

void Request_Content_Templates(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue arrRows = xvoCreateArray();
	const char* aTemplates[] = {
		"plugin.main.c.tpl",
		"plugin.admin.html.tpl",
		"plugin.public.html.tpl",
		"plugin.managed.json.tpl",
		"plugin.contracts.json.tpl"
	};

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	for ( int i = 0; i < 5; i++ ) {
		xvalue tblRow = xvoCreateTable();
		xvoTableSetText(tblRow, "name", 4, (str)aTemplates[i], 0, FALSE);
		xvoTableSetText(tblRow, "path", 4, (str)"content", 0, FALSE);
		xvoArrayAppendValue(arrRows, tblRow, TRUE);
	}
	Content_ReplySuccess(objResp, arrRows);
}
