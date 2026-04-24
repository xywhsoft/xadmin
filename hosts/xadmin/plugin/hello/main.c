#include <xs_plugin.h>

static XAdminPluginHandle G_HelloHandle = NULL;
static sqlite3* G_HelloMainDb = NULL;
static xvalue G_HelloOptionTable = NULL;
static const char* G_HelloXid = NULL;
static const char* G_HelloRootPath = NULL;
static const char* G_HelloDataPath = NULL;
static const char* G_HelloPrivateDbPath = NULL;

typedef struct {
	char sWelcomeMessage[256];
	bool bShowTime;
} HelloConfigState;

typedef struct {
	char sMessage[256];
	bool bShowTime;
} HelloGreetingData;

typedef struct {
	const char* sRoute;
	const char* sMessage;
} HelloGreetingEvent;

typedef struct {
	const char* (*get_message)(void);
	void (*fill_data)(HelloGreetingData* pData);
} HelloGreeterServiceVTable;

static HelloConfigState G_HelloConfig = {
	"Hello from xAdmin plugin system",
	FALSE
};
static int G_HelloEventCount = 0;
static int G_HelloHookCount = 0;
static char G_HelloLastEventMessage[256] = "";
static const char* G_HelloGeneratedSchema =
	"{\n"
	"\t\"type\": \"object\",\n"
	"\t\"properties\": {\n"
	"\t\t\"message\": {\n"
	"\t\t\t\"type\": \"string\"\n"
	"\t\t}\n"
	"\t},\n"
	"\t\"additionalProperties\": false\n"
	"}\n";

bool Hello_IsValidGeneratedXid(const char* sXid)
{
	size_t iLen;

	if ( (sXid == NULL) || (sXid[0] == '\0') ) {
		return FALSE;
	}
	iLen = strlen(sXid);
	if ( (iLen <= 0) || (iLen > 96) ) {
		return FALSE;
	}

	for ( size_t i = 0; i < iLen; i++ ) {
		char ch = sXid[i];
		if ( ((ch >= 'a') && (ch <= 'z'))
			|| ((ch >= 'A') && (ch <= 'Z'))
			|| ((ch >= '0') && (ch <= '9'))
			|| (ch == '.')
			|| (ch == '_')
			|| (ch == '-') ) {
			continue;
		}
		return FALSE;
	}
	return TRUE;
}

str Hello_BuildGeneratedMainSource(const char* sXid)
{
	return xrtFormat(
		"#include <xs_plugin.h>\n"
		"\n"
		"static char G_Message[256] = \"Hello from generated plugin %s\";\n"
		"\n"
		"void Generated_SendJson(XS_ResponseObject objResp)\n"
		"{\n"
		"\txvalue tblRet = xvoCreateTable();\n"
		"\tsize_t iSize = 0;\n"
		"\tstr sJson;\n"
		"\txvoTableSetBool(tblRet, \"result\", 6, TRUE);\n"
		"\txvoTableSetText(tblRet, \"xid\", 3, \"%s\", 0, FALSE);\n"
		"\txvoTableSetText(tblRet, \"message\", 7, G_Message, 0, FALSE);\n"
		"\tsJson = xrtStringifyJSON(tblRet, FALSE, &iSize);\n"
		"\tif ( sJson ) {\n"
		"\t\txsHttpReplyAuto(objResp, 200, \"Content-Type: application/json\\r\\n\", sJson, iSize);\n"
		"\t\txrtFree(sJson);\n"
		"\t}\n"
		"\txvoUnref(tblRet);\n"
		"}\n"
		"\n"
		"void Generated_RequestPing(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)\n"
		"{\n"
		"\t(void)objServer;\n"
		"\t(void)objHost;\n"
		"\t(void)objSession;\n"
		"\tif ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {\n"
		"\t\txsHttpReplyAuto(objResp, 405, \"Content-Type: application/json\\r\\n\", \"{\\\"result\\\":false,\\\"message\\\":\\\"method not allowed\\\"}\", 0);\n"
		"\t\treturn;\n"
		"\t}\n"
		"\tGenerated_SendJson(objResp);\n"
		"}\n"
		"\n"
		"int Generated_OnLoad(XAdminPluginHandle* out_handle)\n"
		"{\n"
		"\t(void)out_handle;\n"
		"\treturn 0;\n"
		"}\n"
		"\n"
		"int Generated_OnStart(XAdminPluginHandle handle)\n"
		"{\n"
		"\tXAdminRouteDecl route;\n"
		"\tmemset(&route, 0, sizeof(route));\n"
		"\troute.path = \"/api/plugin/generated/%s/ping\";\n"
		"\troute.proc = Generated_RequestPing;\n"
		"\treturn XAdmin_RegisterRoute(handle, &route, NULL);\n"
		"}\n"
		"\n"
		"int Generated_OnConfigChanged(XAdminPluginHandle handle, xvalue new_cfg)\n"
		"{\n"
		"\tstr sMessage = NULL;\n"
		"\t(void)handle;\n"
		"\tsnprintf(G_Message, sizeof(G_Message), \"Hello from generated plugin %s\");\n"
		"\tif ( new_cfg ) {\n"
		"\t\tsMessage = xvoTableGetText(new_cfg, \"message\", 7);\n"
		"\t\tif ( sMessage && sMessage[0] ) {\n"
		"\t\t\tsnprintf(G_Message, sizeof(G_Message), \"%%s\", sMessage);\n"
		"\t\t}\n"
		"\t}\n"
		"\treturn 0;\n"
		"}\n"
		"\n"
		"void Generated_OnStop(XAdminPluginHandle handle)\n"
		"{\n"
		"\t(void)handle;\n"
		"}\n"
		"\n"
		"void Generated_OnUnload(XAdminPluginHandle handle)\n"
		"{\n"
		"\t(void)handle;\n"
		"}\n"
		"\n"
		"static XAdminPluginDescriptor G_Plugin = {\n"
		"\tXADMIN_ABI_VERSION,\n"
		"\tsizeof(XAdminPluginDescriptor),\n"
		"\t\"%s\",\n"
		"\t\"1.0.0\",\n"
		"\t\"%s\",\n"
		"\tGenerated_OnLoad,\n"
		"\tNULL,\n"
		"\tGenerated_OnStart,\n"
		"\tGenerated_OnConfigChanged,\n"
		"\tNULL,\n"
		"\tGenerated_OnStop,\n"
		"\tGenerated_OnUnload\n"
		"};\n"
		"\n"
		"XADMIN_DECLARE_PLUGIN(G_Plugin)\n",
		sXid,
		sXid,
		sXid,
		sXid,
		sXid,
		sXid
	);
}

const char* Hello_ServiceGetMessage(void)
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
	if ( idx == XADMIN_GLOBAL_MAIN_DB ) {
		G_HelloMainDb = (sqlite3*)ptr;
	} else if ( idx == XADMIN_GLOBAL_OPTION_TABLE ) {
		G_HelloOptionTable = (xvalue)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_XID ) {
		G_HelloXid = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_ROOT_PATH ) {
		G_HelloRootPath = (const char*)ptr;
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
		xsHttpReplyAuto(objResp, 200, "Content-Type: application/json\r\n", sJson, iSize);
		xrtFree(sJson);
	}
	xvoUnref(tblData);
}

void Hello_RequestGenerate(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	XAdminGeneratedFile files[3];
	XAdminGeneratedPluginSpec spec;
	xvalue tblForm = NULL;
	xvalue tblRet;
	str sXid = NULL;
	str sTitle = NULL;
	str sMainSource = NULL;
	str sDefaultConfig = NULL;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	tblRet = xvoCreateTable();
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xvoTableSetBool(tblRet, "result", 6, FALSE);
		xvoTableSetText(tblRet, "message", 7, "method not allowed", 0, FALSE);
		Hello_SendTableJson(objResp, tblRet);
		return;
	}

	tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( (tblForm == NULL) || (xvoType(tblForm) != XVO_DT_TABLE) ) {
		if ( tblForm ) {
			xvoUnref(tblForm);
		}
		xvoTableSetBool(tblRet, "result", 6, FALSE);
		xvoTableSetText(tblRet, "message", 7, "invalid json body", 0, FALSE);
		Hello_SendTableJson(objResp, tblRet);
		return;
	}

	if ( xvoTableGetText(tblForm, "xid", 3) ) {
		sXid = xrtCopyStr(xvoTableGetText(tblForm, "xid", 3), 0);
	}
	if ( xvoTableGetText(tblForm, "title", 5) ) {
		sTitle = xrtCopyStr(xvoTableGetText(tblForm, "title", 5), 0);
	}
	if ( !Hello_IsValidGeneratedXid(sXid) ) {
		xvoUnref(tblForm);
		if ( sXid ) {
			xrtFree(sXid);
		}
		if ( sTitle ) {
			xrtFree(sTitle);
		}
		xvoTableSetBool(tblRet, "result", 6, FALSE);
		xvoTableSetText(tblRet, "message", 7, "invalid xid", 0, FALSE);
		Hello_SendTableJson(objResp, tblRet);
		return;
	}

	sMainSource = Hello_BuildGeneratedMainSource(sXid);
	sDefaultConfig = xrtFormat("{\n\t\"message\": \"Hello from generated plugin %s\"\n}\n", sXid);
	if ( (sMainSource == NULL) || (sDefaultConfig == NULL) ) {
		xvoUnref(tblForm);
		if ( sXid ) xrtFree(sXid);
		if ( sTitle ) xrtFree(sTitle);
		if ( sMainSource ) xrtFree(sMainSource);
		if ( sDefaultConfig ) xrtFree(sDefaultConfig);
		xvoTableSetBool(tblRet, "result", 6, FALSE);
		xvoTableSetText(tblRet, "message", 7, "generate source failed", 0, FALSE);
		Hello_SendTableJson(objResp, tblRet);
		return;
	}

	memset(files, 0, sizeof(files));
	files[0].relative_path = "main.c";
	files[0].data = sMainSource;
	files[1].relative_path = "config.defaults.json";
	files[1].data = sDefaultConfig;
	files[2].relative_path = "config.schema.json";
	files[2].data = G_HelloGeneratedSchema;

	memset(&spec, 0, sizeof(spec));
	spec.xid = sXid;
	spec.title = (sTitle && sTitle[0]) ? sTitle : sXid;
	spec.version = "1.0.0";
	spec.entry = "main.c";
	spec.auto_enable = 1;
	spec.file_count = 3;
	spec.files = files;

	if ( XAdmin_GeneratePlugin(G_HelloHandle, &spec) != 0 ) {
		xvoTableSetBool(tblRet, "result", 6, FALSE);
		xvoTableSetText(tblRet, "message", 7, "generate plugin failed", 0, FALSE);
	} else {
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetText(tblRet, "message", 7, "plugin generated", 0, FALSE);
		xvoTableSetText(tblRet, "xid", 3, sXid, 0, FALSE);
		xvoTableSetText(tblRet, "route", 5, xrtFormat("/api/plugin/generated/%s/ping", sXid), 0, TRUE);
	}

	xvoUnref(tblForm);
	if ( sXid ) xrtFree(sXid);
	if ( sTitle ) xrtFree(sTitle);
	if ( sMainSource ) xrtFree(sMainSource);
	if ( sDefaultConfig ) xrtFree(sDefaultConfig);
	Hello_SendTableJson(objResp, tblRet);
}

void Hello_RequestGreeting(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	XAdminServiceLease hLease = NULL;
	const HelloGreeterServiceVTable* pGreeter = NULL;
	HelloGreetingData data;
	HelloGreetingEvent eventData;
	int iHookResult;
	xvalue tblRet;

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	memset(&data, 0, sizeof(data));
	memset(&eventData, 0, sizeof(eventData));
	tblRet = xvoCreateTable();
	if ( (G_HelloHandle == NULL)
		|| (XAdmin_AcquireService(G_HelloHandle, "hello.greeter", 1, &hLease, (const void**)&pGreeter) != 0)
		|| (pGreeter == NULL) ) {
		if ( hLease ) {
			XAdmin_ReleaseService(hLease);
		}
		xvoTableSetBool(tblRet, "result", 6, FALSE);
		xvoTableSetText(tblRet, "message", 7, "hello.greeter unavailable", 0, FALSE);
		Hello_SendTableJson(objResp, tblRet);
		return;
	}

	pGreeter->fill_data(&data);
	XAdmin_ReleaseService(hLease);
	iHookResult = XAdmin_InvokeHook(G_HelloHandle, "hello.greeting.decorate", &data, sizeof(data));
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
		xvoTableSetInt(tblRet, "time", 4, xrtNow());
	}

	eventData.sRoute = "/api/plugin/hello/greeting";
	eventData.sMessage = data.sMessage;
	XAdmin_EmitEvent(G_HelloHandle, "hello.greeting.served", &eventData, sizeof(eventData));
	Hello_SendTableJson(objResp, tblRet);
}

void Hello_RequestInfo(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblRet;
	xvalue tblGlobal = NULL;

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetText(tblRet, "xid", 3, (str)(G_HelloXid ? G_HelloXid : "hello"), 0, FALSE);
	xvoTableSetText(tblRet, "title", 5, "Hello World Demo Plugin", 0, FALSE);
	xvoTableSetText(tblRet, "version", 7, "4.0.0", 0, FALSE);
	xvoTableSetBool(tblRet, "dbInjected", 10, G_HelloMainDb != NULL);
	xvoTableSetBool(tblRet, "optionsInjected", 15, G_HelloOptionTable != NULL);
	xvoTableSetText(tblRet, "rootPath", 8, (str)(G_HelloRootPath ? G_HelloRootPath : ""), 0, FALSE);
	xvoTableSetText(tblRet, "dataPath", 8, (str)(G_HelloDataPath ? G_HelloDataPath : ""), 0, FALSE);
	xvoTableSetText(tblRet, "privateDbPath", 13, (str)(G_HelloPrivateDbPath ? G_HelloPrivateDbPath : ""), 0, FALSE);
	xvoTableSetInt(tblRet, "eventCount", 10, G_HelloEventCount);
	xvoTableSetInt(tblRet, "hookCount", 9, G_HelloHookCount);
	xvoTableSetText(tblRet, "lastEventMessage", 16, G_HelloLastEventMessage, 0, FALSE);
	if ( G_HelloOptionTable ) {
		tblGlobal = xvoTableGetValue(G_HelloOptionTable, "global", 6);
		if ( tblGlobal ) {
			xvoTableSetValue(tblRet, "globalOptions", 13, tblGlobal, FALSE);
		}
	}
	Hello_SendTableJson(objResp, tblRet);
}

void Hello_RequestView(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	const char* sHtml =
		"<div style='padding:32px;text-align:center;'>"
		"<h1 style='margin-bottom:12px;'>Hello Plugin</h1>"
		"<p style='color:#666;'>This page is served by xs_plugin.h.</p>"
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

	xsHttpReplyAuto(objResp, 200, "Content-Type: text/html\r\n", sHtml, 0);
}

void Hello_OnGreetingEvent(const char* event_name, void* payload, size_t payload_size)
{
	HelloGreetingEvent* pEvent = (HelloGreetingEvent*)payload;

	G_HelloEventCount++;
	if ( (payload_size >= sizeof(HelloGreetingEvent)) && pEvent && pEvent->sMessage ) {
		snprintf(G_HelloLastEventMessage, sizeof(G_HelloLastEventMessage), "%s", pEvent->sMessage);
	}
	printf("[hello] event observed: %s\n", event_name ? event_name : "(null)");
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

int Hello_OnLoad(XAdminPluginHandle* out_handle)
{
	if ( out_handle ) {
		G_HelloHandle = *out_handle;
	}
	G_HelloEventCount = 0;
	G_HelloHookCount = 0;
	G_HelloLastEventMessage[0] = '\0';
	printf("[hello] loaded: db=%d options=%d xid=%s data=%s\n",
		G_HelloMainDb != NULL,
		G_HelloOptionTable != NULL,
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
	if ( XAdmin_RegisterAuthGroup(handle, &authGroup, &iAuthGroupId, NULL) != 0 ) {
		return -1;
	}

	memset(&auth, 0, sizeof(auth));
	auth.scope = XADMIN_AUTH_SCOPE_ADMIN;
	auth.group_id = iAuthGroupId;
	auth.name = "hello.plugin.view";
	auth.description = "Access the hello plugin admin page";
	auth.sort = 990001;
	if ( XAdmin_RegisterAuth(handle, &auth, &iAuthId, NULL) != 0 ) {
		return -1;
	}

	memset(&serviceDecl, 0, sizeof(serviceDecl));
	serviceDecl.service_name = "hello.greeter";
	serviceDecl.major_version = 1;
	serviceDecl.minor_version = 0;
	serviceDecl.vtable_size = sizeof(G_HelloGreeterService);
	if ( XAdmin_RegisterService(handle, &serviceDecl, &G_HelloGreeterService) != 0 ) {
		return -1;
	}

	memset(&eventDecl, 0, sizeof(eventDecl));
	eventDecl.event_name = "hello.greeting.served";
	eventDecl.proc = Hello_OnGreetingEvent;
	if ( XAdmin_ListenEvent(handle, &eventDecl, NULL) != 0 ) {
		return -1;
	}

	memset(&hookDecl, 0, sizeof(hookDecl));
	hookDecl.hook_name = "hello.greeting.decorate";
	hookDecl.sort = 100;
	hookDecl.proc = Hello_OnGreetingHook;
	if ( XAdmin_RegisterHook(handle, &hookDecl, NULL) != 0 ) {
		return -1;
	}

	memset(&route, 0, sizeof(route));
	route.path = "/api/plugin/hello/greeting";
	route.proc = Hello_RequestGreeting;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
		return -1;
	}

	memset(&route, 0, sizeof(route));
	route.path = "/api/plugin/hello/info";
	route.proc = Hello_RequestInfo;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
		return -1;
	}

	memset(&route, 0, sizeof(route));
	route.path = "/api/plugin/hello/generate";
	route.proc = Hello_RequestGenerate;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
		return -1;
	}

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/hello";
	route.proc = Hello_RequestView;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
		return -1;
	}

	memset(&uriAuth, 0, sizeof(uriAuth));
	uriAuth.scope = XADMIN_AUTH_SCOPE_ADMIN;
	uriAuth.auth_id = iAuthId;
	uriAuth.uri = "/admin/view/plugin/hello";
	uriAuth.description = "Hello plugin admin page";
	uriAuth.sort = 990001;
	uriAuth.need_auth = TRUE;
	if ( XAdmin_RegisterUriAuth(handle, &uriAuth, NULL, NULL) != 0 ) {
		return -1;
	}

	memset(&uriAuth, 0, sizeof(uriAuth));
	uriAuth.scope = XADMIN_AUTH_SCOPE_ADMIN;
	uriAuth.auth_id = iAuthId;
	uriAuth.uri = "/api/plugin/hello/generate";
	uriAuth.description = "Hello plugin generator api";
	uriAuth.sort = 990002;
	uriAuth.need_auth = TRUE;
	uriAuth.need_log = TRUE;
	if ( XAdmin_RegisterUriAuth(handle, &uriAuth, NULL, NULL) != 0 ) {
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
	if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) {
		return -1;
	}

	printf("[hello] started\n");
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
	G_HelloHandle = NULL;
	printf("[hello] stopping\n");
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
	G_HelloDataPath = NULL;
	G_HelloPrivateDbPath = NULL;
	printf("[hello] unloaded\n");
}

static XAdminPluginDescriptor G_HelloPlugin = {
	XADMIN_ABI_VERSION,
	sizeof(XAdminPluginDescriptor),
	"hello",
	"4.0.0",
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
