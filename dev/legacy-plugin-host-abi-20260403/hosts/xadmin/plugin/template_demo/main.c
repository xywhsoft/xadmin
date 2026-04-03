#include "plugin.h"

PluginContext* ctx;

void Plugin_template_demo_Hello(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);
void Plugin_template_demo_Page(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);

void Plugin_SetGlobalData(int idx, void* ptr)
{
	if ( idx == 1 ) {
		ctx = (PluginContext*)ptr;
	}
}

void Plugin_template_demo_Init()
{
	ctx->Log(LOG_INFO, "Template Demo Plugin initializing...");

	ctx->AddRoute("/admin/api/template_demo/hello", Plugin_template_demo_Hello, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/admin/view/template_demo/page", Plugin_template_demo_Page, TRUE, TRUE, 0, 0);

	ctx->Log(LOG_INFO, "Template Demo Plugin initialized!");
}

void Plugin_template_demo_Unit()
{
	ctx->Log(LOG_INFO, "Template Demo Plugin unloaded!");
}

void Plugin_template_demo_Hello(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue objData = xvoCreateTable();
	xvoTableSetText(objData, "title", 5, "Template Demo", 0, TRUE);
	xvoTableSetText(objData, "message", 7, "Welcome to xAdmin template demo.", 0, TRUE);
	xvoTableSetInt(objData, "time", 4, ctx->TimeNow());

	RENDER_SEND_TEMPLATE(objResp, "hello.html", objData);

	xvoUnref(objData);
}

void Plugin_template_demo_Page(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	LOAD_SEND_PAGE(objResp, "static.html");
}
