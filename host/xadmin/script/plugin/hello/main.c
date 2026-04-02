#include "plugin.h"

static const XAdminHostAPI* G_HelloHost = NULL;
static xvalue G_HelloConfig = NULL;

void Hello_SendTableJson(XS_ResponseObject objResp, xvalue tblData)
{
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblData, FALSE, &iSize);
	if ( sJson ) {
		G_HelloHost->http.reply_json(objResp, 200, sJson, iSize);
		G_HelloHost->core.free((void*)sJson);
	}
	xvoUnref(tblData);
}

void Hello_RequestGreeting(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	str sMessage = "Hello from xAdmin plugin system";
	bool bShowTime = FALSE;
	xvalue tblRet;

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	if ( G_HelloConfig ) {
		str sCustomMessage = xvoTableGetText(G_HelloConfig, "welcomeMessage", 14);
		if ( sCustomMessage && sCustomMessage[0] ) {
			sMessage = sCustomMessage;
		}
		bShowTime = xvoTableGetBool(G_HelloConfig, "showTime", 8);
	}

	tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetText(tblRet, "message", 7, sMessage, 0, FALSE);
	if ( bShowTime ) {
		xvoTableSetInt(tblRet, "time", 4, G_HelloHost->core.time_now());
	}

	Hello_SendTableJson(objResp, tblRet);
}

void Hello_RequestInfo(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblRet;

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetText(tblRet, "id", 2, "hello", 0, FALSE);
	xvoTableSetText(tblRet, "title", 5, "Hello World Demo Plugin", 0, FALSE);
	xvoTableSetText(tblRet, "version", 7, "3.0.0", 0, FALSE);
	Hello_SendTableJson(objResp, tblRet);
}

void Hello_RequestView(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	const char* sHtml =
		"<div style='padding:32px;text-align:center;'>"
		"<h1 style='margin-bottom:12px;'>Hello Plugin</h1>"
		"<p style='color:#666;'>This page is served by the new plugin ABI.</p>"
		"<button class='layui-btn' onclick='helloPluginTest()'>Call API</button>"
		"<pre id='hello_plugin_result' style='margin:24px auto 0;max-width:720px;text-align:left;background:#f7f7f7;padding:16px;border-radius:8px;'></pre>"
		"</div>"
		"<script>"
		"function helloPluginTest(){"
		"fetch('/api/plugin/hello/greeting')"
		".then(function(r){return r.json();})"
		".then(function(data){document.getElementById('hello_plugin_result').innerText=JSON.stringify(data,null,2);});"
		"}"
		"</script>";

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	G_HelloHost->http.reply_html(objResp, 200, sHtml);
}

int Hello_OnLoad(const XAdminHostAPI* host, XAdminPluginHandle* out_handle)
{
	(void)out_handle;
	G_HelloHost = host;
	G_HelloHost->core.log(LOG_INFO, "hello plugin loaded");
	return 0;
}

int Hello_OnStart(XAdminPluginHandle handle)
{
	XAdminRouteDecl route;

	memset(&route, 0, sizeof(route));
	route.path = "/api/plugin/hello/greeting";
	route.proc = Hello_RequestGreeting;
	G_HelloHost->http.register_route(handle, &route, NULL);

	memset(&route, 0, sizeof(route));
	route.path = "/api/plugin/hello/info";
	route.proc = Hello_RequestInfo;
	G_HelloHost->http.register_route(handle, &route, NULL);

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/hello";
	route.proc = Hello_RequestView;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	G_HelloHost->http.register_route(handle, &route, NULL);

	G_HelloHost->core.log(LOG_INFO, "hello plugin started");
	return 0;
}

int Hello_OnConfigChanged(XAdminPluginHandle handle, xvalue new_cfg)
{
	(void)handle;

	if ( G_HelloConfig ) {
		xvoUnref(G_HelloConfig);
		G_HelloConfig = NULL;
	}

	if ( new_cfg ) {
		xvoAddRef(new_cfg);
		G_HelloConfig = new_cfg;
	}

	return 0;
}

int Hello_OnHealthCheck(XAdminPluginHandle handle, XAdminHealthReport* out_report)
{
	(void)handle;

	if ( out_report ) {
		out_report->status_code = 0;
		out_report->message = "ok";
	}
	return 0;
}

void Hello_OnStop(XAdminPluginHandle handle)
{
	(void)handle;
	G_HelloHost->core.log(LOG_INFO, "hello plugin stopping");
}

void Hello_OnUnload(XAdminPluginHandle handle)
{
	(void)handle;

	if ( G_HelloConfig ) {
		xvoUnref(G_HelloConfig);
		G_HelloConfig = NULL;
	}

	G_HelloHost->core.log(LOG_INFO, "hello plugin unloaded");
	G_HelloHost = NULL;
}

static XAdminPluginDescriptor G_HelloPlugin = {
	XADMIN_ABI_VERSION,
	sizeof(XAdminPluginDescriptor),
	"hello",
	"3.0.0",
	"Hello World Demo Plugin",
	Hello_OnLoad,
	NULL,
	Hello_OnStart,
	Hello_OnConfigChanged,
	Hello_OnHealthCheck,
	Hello_OnStop,
	Hello_OnUnload
};

XADMIN_DECLARE_PLUGIN(G_HelloPlugin)
