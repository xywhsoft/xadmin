#include "plugin.h"

static const XAdminHostAPI* G_HelloHost = NULL;
static XAdminPluginHandle G_HelloHandle = NULL;
static sqlite3* G_HelloMainDb = NULL;
static const char* G_HelloXid = NULL;
static const char* G_HelloRootPath = NULL;
static const char* G_HelloInstanceId = NULL;
static const char* G_HelloInstanceName = NULL;
static const char* G_HelloMountPath = NULL;
static const char* G_HelloDataPath = NULL;
static const char* G_HelloPrivateDbPath = NULL;

typedef struct {
	char sWelcomeMessage[256];
	bool bShowTime;
} HelloConfigState;

static HelloConfigState G_HelloConfig = {
	"Hello from xAdmin plugin system",
	FALSE
};

typedef struct {
	char sMessage[256];
	bool bShowTime;
} HelloGreetingData;

typedef struct {
	const char* sRoute;
	const char* sMessage;
} HelloGreetingEvent;

static int G_HelloEventCount = 0;
static int G_HelloHookCount = 0;
static char G_HelloLastEventMessage[256] = "";

typedef struct {
	const char* (*get_message)();
	void (*fill_data)(HelloGreetingData* pData);
} HelloGreeterServiceVTable;

const char* Hello_ServiceGetMessage()
{
	return G_HelloConfig.sWelcomeMessage[0] ? G_HelloConfig.sWelcomeMessage : "Hello from xAdmin plugin system";
}

void Hello_ServiceFillData(HelloGreetingData* pData)
{
	if ( pData == NULL ) {
		return;
	}

	memset(pData, 0, sizeof(HelloGreetingData));
	snprintf(pData->sMessage, sizeof(pData->sMessage), "%s", Hello_ServiceGetMessage());
	pData->bShowTime = G_HelloConfig.bShowTime;
}

static HelloGreeterServiceVTable G_HelloGreeterService = {
	Hello_ServiceGetMessage,
	Hello_ServiceFillData
};

XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int idx, void* ptr)
{
	if ( idx == XADMIN_GLOBAL_HOST_API ) {
		G_HelloHost = (const XAdminHostAPI*)ptr;
	} else if ( idx == XADMIN_GLOBAL_MAIN_DB ) {
		G_HelloMainDb = (sqlite3*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_XID ) {
		G_HelloXid = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_ROOT_PATH ) {
		G_HelloRootPath = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_INSTANCE_ID ) {
		G_HelloInstanceId = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_INSTANCE_NAME ) {
		G_HelloInstanceName = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_MOUNT_PATH ) {
		G_HelloMountPath = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_DATA_PATH ) {
		G_HelloDataPath = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_PRIVATE_DB_PATH ) {
		G_HelloPrivateDbPath = (const char*)ptr;
	}
}

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
	XAdminServiceLease hLease = NULL;
	const HelloGreeterServiceVTable* pGreeter = NULL;
	HelloGreetingData data;
	HelloGreetingEvent eventData;
	int iHookResult = XADMIN_HOOK_CONTINUE;
	xvalue tblRet;

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	memset(&data, 0, sizeof(data));
	memset(&eventData, 0, sizeof(eventData));
	tblRet = xvoCreateTable();
	if ( (G_HelloHandle == NULL)
		|| (G_HelloHost->service.acquire_service(G_HelloHandle, "hello.greeter", 1, &hLease, (const void**)&pGreeter) != 0)
		|| (pGreeter == NULL) ) {
		if ( hLease ) {
			G_HelloHost->service.release_service(hLease);
		}
		xvoTableSetBool(tblRet, "result", 6, FALSE);
		xvoTableSetText(tblRet, "message", 7, "hello.greeter unavailable", 0, FALSE);
		Hello_SendTableJson(objResp, tblRet);
		return;
	}

	pGreeter->fill_data(&data);
	G_HelloHost->service.release_service(hLease);
	iHookResult = G_HelloHost->hook.invoke(G_HelloHandle, "hello.greeting.decorate", &data, sizeof(data));
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	if ( iHookResult == XADMIN_HOOK_STOP ) {
		xvoTableSetBool(tblRet, "result", 6, FALSE);
		xvoTableSetText(tblRet, "message", 7, "greeting stopped by hook", 0, FALSE);
		Hello_SendTableJson(objResp, tblRet);
		return;
	}
	if ( iHookResult == XADMIN_HOOK_ERROR ) {
		xvoTableSetBool(tblRet, "result", 6, FALSE);
		xvoTableSetText(tblRet, "message", 7, "greeting hook failed", 0, FALSE);
		Hello_SendTableJson(objResp, tblRet);
		return;
	}

	xvoTableSetText(tblRet, "message", 7, data.sMessage, 0, FALSE);
	xvoTableSetText(tblRet, "service", 7, "hello.greeter", 0, FALSE);
	xvoTableSetInt(tblRet, "hookCount", 9, G_HelloHookCount);
	if ( data.bShowTime ) {
		xvoTableSetInt(tblRet, "time", 4, G_HelloHost->core.time_now());
	}

	eventData.sRoute = "/api/plugin/hello/greeting";
	eventData.sMessage = data.sMessage;
	G_HelloHost->event.emit(G_HelloHandle, "hello.greeting.served", &eventData, sizeof(eventData));
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
	xvoTableSetText(tblRet, "id", 2, (str)(G_HelloXid ? G_HelloXid : "hello"), 0, FALSE);
	xvoTableSetText(tblRet, "xid", 3, (str)(G_HelloXid ? G_HelloXid : "hello"), 0, FALSE);
	xvoTableSetText(tblRet, "title", 5, "Hello World Demo Plugin", 0, FALSE);
	xvoTableSetText(tblRet, "version", 7, "3.0.0", 0, FALSE);
	xvoTableSetBool(tblRet, "dbInjected", 10, G_HelloMainDb != NULL);
	xvoTableSetBool(tblRet, "hostInjected", 12, G_HelloHost != NULL);
	xvoTableSetText(tblRet, "rootPath", 8, (str)(G_HelloRootPath ? G_HelloRootPath : ""), 0, FALSE);
	xvoTableSetText(tblRet, "instanceId", 10, (str)(G_HelloInstanceId ? G_HelloInstanceId : ""), 0, FALSE);
	xvoTableSetText(tblRet, "instanceName", 12, (str)(G_HelloInstanceName ? G_HelloInstanceName : ""), 0, FALSE);
	xvoTableSetText(tblRet, "mountPath", 9, (str)(G_HelloMountPath ? G_HelloMountPath : ""), 0, FALSE);
	xvoTableSetText(tblRet, "dataPath", 8, (str)(G_HelloDataPath ? G_HelloDataPath : ""), 0, FALSE);
	xvoTableSetText(tblRet, "privateDbPath", 13, (str)(G_HelloPrivateDbPath ? G_HelloPrivateDbPath : ""), 0, FALSE);
	xvoTableSetInt(tblRet, "eventCount", 10, G_HelloEventCount);
	xvoTableSetInt(tblRet, "hookCount", 9, G_HelloHookCount);
	xvoTableSetText(tblRet, "lastEventMessage", 16, G_HelloLastEventMessage, 0, FALSE);
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

void Hello_OnGreetingEvent(const char* event_name, void* payload, size_t payload_size)
{
	HelloGreetingEvent* pEvent = (HelloGreetingEvent*)payload;

	G_HelloEventCount++;
	if ( (payload_size >= sizeof(HelloGreetingEvent)) && pEvent && pEvent->sMessage ) {
		snprintf(G_HelloLastEventMessage, sizeof(G_HelloLastEventMessage), "%s", pEvent->sMessage);
	}
	if ( G_HelloHost ) {
		G_HelloHost->core.log(LOG_INFO, "hello event observed: %s", event_name ? event_name : "(null)");
	}
}

int Hello_OnGreetingHook(const char* hook_name, void* payload, size_t payload_size)
{
	HelloGreetingData* pData = (HelloGreetingData*)payload;
	char sBuffer[256];

	(void)hook_name;

	if ( (pData == NULL) || (payload_size < sizeof(HelloGreetingData)) ) {
		return XADMIN_HOOK_ERROR;
	}

	G_HelloHookCount++;
	snprintf(sBuffer, sizeof(sBuffer), "%s [hook]", pData->sMessage[0] ? pData->sMessage : "Hello");
	snprintf(pData->sMessage, sizeof(pData->sMessage), "%s", sBuffer);
	return XADMIN_HOOK_CONTINUE;
}

int Hello_OnLoad(const XAdminHostAPI* host, XAdminPluginHandle* out_handle)
{
	(void)out_handle;
	G_HelloHost = host;
	G_HelloEventCount = 0;
	G_HelloHookCount = 0;
	G_HelloLastEventMessage[0] = '\0';
	G_HelloHost->core.log(LOG_INFO, "hello plugin loaded: dbInjected=%d pluginctl=%d xid=%s data=%s",
		G_HelloMainDb != NULL,
		(host && host->pluginctl.generate_plugin != NULL) ? 1 : 0,
		G_HelloXid ? G_HelloXid : "(null)",
		G_HelloDataPath ? G_HelloDataPath : "(null)");
	return 0;
}

int Hello_OnStart(XAdminPluginHandle handle)
{
	XAdminAuthGroupDecl authGroup;
	XAdminAuthDecl auth;
	XAdminUriAuthDecl uriAuth;
	XAdminMenuDecl menu;
	XAdminRouteDecl route;
	XAdminServiceDecl serviceDecl;
	XAdminEventDecl eventDecl;
	XAdminHookDecl hookDecl;
	int iAuthGroupId = 0;
	int iAuthId = 0;

	G_HelloHandle = handle;

	memset(&authGroup, 0, sizeof(authGroup));
	authGroup.scope = XADMIN_AUTH_SCOPE_ADMIN;
	authGroup.name = "Plugin Demo";
	authGroup.description = "Permissions for demo plugins";
	authGroup.sort = 990000;
	if ( G_HelloHost->auth.register_auth_group(handle, &authGroup, &iAuthGroupId, NULL) != 0 ) {
		G_HelloHost->core.log(LOG_ERROR, "hello auth group register failed");
		return -1;
	}

	memset(&auth, 0, sizeof(auth));
	auth.scope = XADMIN_AUTH_SCOPE_ADMIN;
	auth.group_id = iAuthGroupId;
	auth.name = "hello.plugin.view";
	auth.description = "Access the hello plugin admin page";
	auth.sort = 990001;
	if ( G_HelloHost->auth.register_auth(handle, &auth, &iAuthId, NULL) != 0 ) {
		G_HelloHost->core.log(LOG_ERROR, "hello auth register failed");
		return -1;
	}

	memset(&serviceDecl, 0, sizeof(serviceDecl));
	serviceDecl.service_name = "hello.greeter";
	serviceDecl.major_version = 1;
	serviceDecl.minor_version = 0;
	serviceDecl.vtable_size = sizeof(G_HelloGreeterService);
	if ( G_HelloHost->service.register_service(handle, &serviceDecl, &G_HelloGreeterService) != 0 ) {
		G_HelloHost->core.log(LOG_ERROR, "hello service register failed");
		return -1;
	}

	memset(&eventDecl, 0, sizeof(eventDecl));
	eventDecl.event_name = "hello.greeting.served";
	eventDecl.proc = Hello_OnGreetingEvent;
	if ( G_HelloHost->event.listen(handle, &eventDecl, NULL) != 0 ) {
		G_HelloHost->core.log(LOG_ERROR, "hello event listen failed");
		return -1;
	}

	memset(&hookDecl, 0, sizeof(hookDecl));
	hookDecl.hook_name = "hello.greeting.decorate";
	hookDecl.sort = 100;
	hookDecl.proc = Hello_OnGreetingHook;
	if ( G_HelloHost->hook.register_hook(handle, &hookDecl, NULL) != 0 ) {
		G_HelloHost->core.log(LOG_ERROR, "hello hook register failed");
		return -1;
	}

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
	route.auth_id = iAuthId;
	G_HelloHost->http.register_route(handle, &route, NULL);

	memset(&uriAuth, 0, sizeof(uriAuth));
	uriAuth.scope = XADMIN_AUTH_SCOPE_ADMIN;
	uriAuth.auth_id = iAuthId;
	uriAuth.uri = "/admin/view/plugin/hello";
	uriAuth.description = "Hello plugin admin page";
	uriAuth.sort = 990001;
	uriAuth.need_auth = TRUE;
	if ( G_HelloHost->auth.register_uri_auth(handle, &uriAuth, NULL, NULL) != 0 ) {
		G_HelloHost->core.log(LOG_ERROR, "hello uri auth register failed");
		return -1;
	}

	memset(&menu, 0, sizeof(menu));
	menu.title = "Hello Plugin";
	menu.icon = "layui-icon layui-icon-face-smile";
	menu.type = 1;
	menu.open_type = "_iframe";
	menu.href = "/admin/view/plugin/hello";
	menu.sort = 990001;
	menu.visible = TRUE;
	menu.remark = "Hello demo plugin";
	if ( G_HelloHost->ui.register_menu(handle, &menu, NULL, NULL) != 0 ) {
		G_HelloHost->core.log(LOG_ERROR, "hello menu register failed");
		return -1;
	}

	G_HelloHost->core.log(LOG_INFO, "hello plugin started");
	return 0;
}

int Hello_OnConfigChanged(XAdminPluginHandle handle, xvalue new_cfg)
{
	str sCustomMessage = NULL;

	(void)handle;

	memset(&G_HelloConfig, 0, sizeof(G_HelloConfig));
	snprintf(G_HelloConfig.sWelcomeMessage, sizeof(G_HelloConfig.sWelcomeMessage), "%s", "Hello from xAdmin plugin system");

	if ( new_cfg ) {
		sCustomMessage = xvoTableGetText(new_cfg, "welcomeMessage", 14);
		if ( sCustomMessage && sCustomMessage[0] ) {
			snprintf(G_HelloConfig.sWelcomeMessage, sizeof(G_HelloConfig.sWelcomeMessage), "%s", sCustomMessage);
		}
		G_HelloConfig.bShowTime = xvoTableGetBool(new_cfg, "showTime", 8);
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
	G_HelloHandle = NULL;
}

void Hello_OnUnload(XAdminPluginHandle handle)
{
	(void)handle;

	G_HelloHandle = NULL;
	G_HelloEventCount = 0;
	G_HelloHookCount = 0;
	G_HelloLastEventMessage[0] = '\0';
	G_HelloXid = NULL;
	G_HelloRootPath = NULL;
	G_HelloInstanceId = NULL;
	G_HelloInstanceName = NULL;
	G_HelloMountPath = NULL;
	G_HelloDataPath = NULL;
	G_HelloPrivateDbPath = NULL;
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
