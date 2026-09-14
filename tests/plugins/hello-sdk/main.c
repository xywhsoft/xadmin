/* hello-sdk —— xs3 插件宿主的全能力一致性验证插件。
 * 只使用 ABI 面（xs_plugin.h 的 27 个宿主函数 + HttpReplyFormat/LoadPage）
 * 与 xrt 公共 API，不触碰宿主内部符号——这也是 ABI 契约的活体测试。
 * /api/plugin/hello-sdk/state 输出全部计数器，供 smoke 逐项断言。 */
#include <xs_plugin.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

static XAdminPluginHandle G_Handle;
static char G_ConfigMessage[128];

XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int idx, void* ptr)
{
	/* 一致性插件不消费宿主全局；按 ABI v4 契约必须提供该入口。 */
	(void)idx; (void)ptr;
}
static int G_Starts;
static int G_Events;
static int G_HookCalls;
static int G_LastHookRet = -999;
static int G_ServiceCalls;
static int G_UnregisterWorks = -1;
static XAdminRouteToken G_TempRouteToken;

typedef struct {
	const char* (*get_message)(void);
} HelloSdkGreeterVTable;

static const char* HelloSdk_ServiceMessage(void)
{
	G_ServiceCalls++;
	return G_ConfigMessage[0] ? G_ConfigMessage : "hello-sdk-service";
}

static void HelloSdk_ReplyJson(XS_ResponseObject objResp, xvalue* tbl)
{
	size_t size = 0;
	str json = xrtJsonStringify(tbl, false, &size);
	HttpReplyFormat(objResp, 200, "Content-Type: application/json; charset=utf-8\r\n", "%s", json ? json : "{}");
	xrtFree(json);
	xrtValueRelease(tbl);
}

static void HelloSdk_OnEvent(const char* event_name, void* payload, size_t payload_size)
{
	(void)payload; (void)payload_size;
	if (event_name && strcmp(event_name, "hello-sdk.ping") == 0) G_Events++;
}

static int HelloSdk_OnHook(const char* hook_name, void* payload, size_t payload_size)
{
	if (hook_name && strcmp(hook_name, "hello-sdk.greet") == 0) {
		G_HookCalls++;
		if (payload && payload_size >= sizeof(int)) *(int*)payload = G_HookCalls;
		return XADMIN_HOOK_CONTINUE;
	}
	return XADMIN_HOOK_CONTINUE;
}

static void HelloSdk_RequestPing(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	xvalue* tbl = xrtValueObject();
	(void)s; (void)h; (void)req; (void)session;
	xrtValueObjectSetNew(tbl, xrtStrView("result"), xrtValueBool(true));
	xrtValueObjectSetNew(tbl, xrtStrView("message"), xrtValueString(xrtStrView(G_ConfigMessage)));
	xrtValueObjectSetNew(tbl, xrtStrView("starts"), xrtValueInt(G_Starts));
	HelloSdk_ReplyJson(resp, tbl);
}

static void HelloSdk_RequestState(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	xvalue* tbl = xrtValueObject();
	(void)s; (void)h; (void)req; (void)session;
	xrtValueObjectSetNew(tbl, xrtStrView("result"), xrtValueBool(true));
	xrtValueObjectSetNew(tbl, xrtStrView("starts"), xrtValueInt(G_Starts));
	xrtValueObjectSetNew(tbl, xrtStrView("events"), xrtValueInt(G_Events));
	xrtValueObjectSetNew(tbl, xrtStrView("hookCalls"), xrtValueInt(G_HookCalls));
	xrtValueObjectSetNew(tbl, xrtStrView("lastHookRet"), xrtValueInt(G_LastHookRet));
	xrtValueObjectSetNew(tbl, xrtStrView("serviceCalls"), xrtValueInt(G_ServiceCalls));
	xrtValueObjectSetNew(tbl, xrtStrView("unregisterWorks"), xrtValueInt(G_UnregisterWorks));
	xrtValueObjectSetNew(tbl, xrtStrView("message"), xrtValueString(xrtStrView(G_ConfigMessage)));
	HelloSdk_ReplyJson(resp, tbl);
}

static void HelloSdk_RequestEmit(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	int counter = 1;
	xvalue* tbl = xrtValueObject();
	(void)s; (void)h; (void)req; (void)session;
	XAdmin_EmitEvent(G_Handle, "hello-sdk.ping", &counter, sizeof(counter));
	xrtValueObjectSetNew(tbl, xrtStrView("result"), xrtValueBool(true));
	xrtValueObjectSetNew(tbl, xrtStrView("events"), xrtValueInt(G_Events));
	HelloSdk_ReplyJson(resp, tbl);
}

static void HelloSdk_RequestHook(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	int payload = 0;
	xvalue* tbl = xrtValueObject();
	(void)s; (void)h; (void)req; (void)session;
	G_LastHookRet = XAdmin_InvokeHook(G_Handle, "hello-sdk.greet", &payload, sizeof(payload));
	xrtValueObjectSetNew(tbl, xrtStrView("result"), xrtValueBool(true));
	xrtValueObjectSetNew(tbl, xrtStrView("ret"), xrtValueInt(G_LastHookRet));
	xrtValueObjectSetNew(tbl, xrtStrView("payload"), xrtValueInt(payload));
	HelloSdk_ReplyJson(resp, tbl);
}

static void HelloSdk_RequestService(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	XAdminServiceLease lease = NULL;
	const void* vtable = NULL;
	const HelloSdkGreeterVTable* greeter;
	xvalue* tbl = xrtValueObject();
	int rc = XAdmin_AcquireService(G_Handle, "helloSdk.greeter", 1, &lease, &vtable);
	(void)s; (void)h; (void)req; (void)session;
	greeter = (const HelloSdkGreeterVTable*)vtable;
	xrtValueObjectSetNew(tbl, xrtStrView("acquire"), xrtValueInt(rc));
	xrtValueObjectSetNew(tbl, xrtStrView("result"), xrtValueBool(rc == 0 && greeter && greeter->get_message));
	if (rc == 0 && greeter && greeter->get_message)
		xrtValueObjectSetNew(tbl, xrtStrView("message"), xrtValueString(xrtStrView(greeter->get_message())));
	if (lease) XAdmin_ReleaseService(lease);
	HelloSdk_ReplyJson(resp, tbl);
}

static void HelloSdk_RequestUnregister(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	xvalue* tbl = xrtValueObject();
	(void)s; (void)h; (void)req; (void)session;
	G_UnregisterWorks = XAdmin_UnregisterRoute(G_TempRouteToken) == 0 ? 1 : 0;
	G_TempRouteToken = 0;
	xrtValueObjectSetNew(tbl, xrtStrView("result"), xrtValueBool(G_UnregisterWorks == 1));
	HelloSdk_ReplyJson(resp, tbl);
}

static void HelloSdk_RequestTemp(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	(void)s; (void)h; (void)req; (void)session;
	HttpReplyFormat(resp, 200, "Content-Type: text/plain\r\n", "temp");
}

static void HelloSdk_RequestAdminEcho(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	xvalue* tbl = xrtValueObject();
	(void)s; (void)h; (void)req;
	xrtValueObjectSetNew(tbl, xrtStrView("result"), xrtValueBool(true));
	{
		int64 id = 0;
		xvalue* v = session ? xrtValueObjectGet(session, xrtStrView("id")) : NULL;
		if (v) (void)xrtValueGetInt(v, &id);
		xrtValueObjectSetNew(tbl, xrtStrView("adminId"), xrtValueInt(id));
	}
	HelloSdk_ReplyJson(resp, tbl);
}

static int HelloSdk_OnLoad(XAdminPluginHandle* out_handle)
{
	G_Starts++;
	if (out_handle) G_Handle = *out_handle;
	return 0;
}

static int HelloSdk_OnStart(XAdminPluginHandle handle)
{
	XAdminRouteDecl route;
	XAdminMenuDecl menu;
	XAdminAuthGroupDecl authGroup;
	XAdminAuthDecl auth;
	XAdminUriAuthDecl uriAuth;
	XAdminEventDecl event;
	XAdminHookDecl hook;
	XAdminServiceDecl service;
	static const HelloSdkGreeterVTable vtable = { HelloSdk_ServiceMessage };
	int iGroupId = 0, iAuthId = 0;

	G_Handle = handle;
	memset(&uriAuth, 0, sizeof(uriAuth));
	uriAuth.scope = XADMIN_AUTH_SCOPE_MEMBER;
	uriAuth.key = "hello-sdk/ping";
	uriAuth.auth_id = 0;
	uriAuth.uri = "/api/plugin/hello-sdk/ping";
	uriAuth.description = "hello-sdk public ping";
	uriAuth.sort = 900100;
	uriAuth.need_auth = false;
	uriAuth.keep_active = false;
	if (XAdmin_RegisterUriAuth(handle, &uriAuth, NULL, NULL) != 0) return -1;

	memset(&authGroup, 0, sizeof(authGroup));
	authGroup.scope = XADMIN_AUTH_SCOPE_ADMIN;
	authGroup.key = "hello-sdk";
	authGroup.name = "Hello SDK";
	authGroup.description = "hello-sdk conformance";
	authGroup.sort = 900200;
	if (XAdmin_RegisterAuthGroup(handle, &authGroup, &iGroupId, NULL) != 0) return -1;

	memset(&auth, 0, sizeof(auth));
	auth.scope = XADMIN_AUTH_SCOPE_ADMIN;
	auth.key = "hello-sdk/echo";
	auth.group_id = iGroupId;
	auth.name = "Hello SDK Echo";
	auth.description = "hello-sdk admin echo";
	auth.sort = 900201;
	if (XAdmin_RegisterAuth(handle, &auth, &iAuthId, NULL) != 0) return -1;

	uriAuth.scope = XADMIN_AUTH_SCOPE_ADMIN;
	uriAuth.key = "hello-sdk/echo";
	uriAuth.auth_id = iAuthId;
	uriAuth.uri = "/api/plugin/hello-sdk/admin-echo";
	uriAuth.need_auth = true;
	if (XAdmin_RegisterUriAuth(handle, &uriAuth, NULL, NULL) != 0) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/api/plugin/hello-sdk/ping";
	route.proc = HelloSdk_RequestPing;
	if (XAdmin_RegisterRoute(handle, &route, NULL) != 0) return -1;
	route.path = "/api/plugin/hello-sdk/state";
	route.proc = HelloSdk_RequestState;
	if (XAdmin_RegisterRoute(handle, &route, NULL) != 0) return -1;
	route.path = "/api/plugin/hello-sdk/emit";
	route.proc = HelloSdk_RequestEmit;
	if (XAdmin_RegisterRoute(handle, &route, NULL) != 0) return -1;
	route.path = "/api/plugin/hello-sdk/hook";
	route.proc = HelloSdk_RequestHook;
	if (XAdmin_RegisterRoute(handle, &route, NULL) != 0) return -1;
	route.path = "/api/plugin/hello-sdk/service";
	route.proc = HelloSdk_RequestService;
	if (XAdmin_RegisterRoute(handle, &route, NULL) != 0) return -1;
	route.path = "/api/plugin/hello-sdk/unregister";
	route.proc = HelloSdk_RequestUnregister;
	if (XAdmin_RegisterRoute(handle, &route, NULL) != 0) return -1;
	route.path = "/api/plugin/hello-sdk/temp";
	route.proc = HelloSdk_RequestTemp;
	if (XAdmin_RegisterRoute(handle, &route, &G_TempRouteToken) != 0) return -1;
	route.path = "/api/plugin/hello-sdk/admin-echo";
	route.proc = HelloSdk_RequestAdminEcho;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if (XAdmin_RegisterRoute(handle, &route, NULL) != 0) return -1;

	memset(&menu, 0, sizeof(menu));
	menu.title = "Hello SDK 插件";
	menu.icon = "layui-icon layui-icon-component";
	menu.type = 1;
	menu.open_type = "_iframe";
	menu.href = "/api/plugin/hello-sdk/ping";
	menu.sort = 990002;
	menu.visible = true;
	menu.remark = "hello-sdk conformance";
	if (XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0) return -1;

	memset(&event, 0, sizeof(event));
	event.event_name = "hello-sdk.ping";
	event.proc = HelloSdk_OnEvent;
	if (XAdmin_ListenEvent(handle, &event, NULL) != 0) return -1;

	memset(&hook, 0, sizeof(hook));
	hook.hook_name = "hello-sdk.greet";
	hook.sort = 10;
	hook.proc = HelloSdk_OnHook;
	if (XAdmin_RegisterHook(handle, &hook, NULL) != 0) return -1;

	memset(&service, 0, sizeof(service));
	service.service_name = "helloSdk.greeter";
	service.major_version = 1;
	service.minor_version = 0;
	service.provider_xid = "hello-sdk";
	service.lifecycle_scope = 0;
	service.vtable_size = sizeof(vtable);
	if (XAdmin_RegisterService(handle, &service, &vtable) != 0) return -1;

	printf("[hello-sdk] started (%d)\n", G_Starts);
	return 0;
}

static int HelloSdk_OnConfigChanged(XAdminPluginHandle handle, xvalue* new_cfg)
{
	(void)handle;
	G_ConfigMessage[0] = '\0';
	if (new_cfg) {
		xvalue* v = xrtValueObjectGet(new_cfg, xrtStrView("welcomeMessage"));
		xstrview text = {0};
		if (v && xrtValueGetString(v, &text) && text.Size) {
			size_t n = text.Size < sizeof(G_ConfigMessage) - 1 ? text.Size : sizeof(G_ConfigMessage) - 1;
			memcpy(G_ConfigMessage, text.Data, n);
			G_ConfigMessage[n] = '\0';
		}
	}
	if (!G_ConfigMessage[0]) snprintf(G_ConfigMessage, sizeof(G_ConfigMessage), "hello-sdk ok");
	return 0;
}

static int HelloSdk_OnHealthCheck(XAdminPluginHandle handle, XAdminHealthReport* out_report)
{
	(void)handle;
	if (out_report) {
		out_report->status_code = 0;
		out_report->message = "ok";
	}
	return 0;
}

static void HelloSdk_OnStop(XAdminPluginHandle handle)
{
	(void)handle;
	printf("[hello-sdk] stopping\n");
}

static void HelloSdk_OnUnload(XAdminPluginHandle handle)
{
	(void)handle;
	G_Handle = NULL;
	G_TempRouteToken = 0;
	printf("[hello-sdk] unloaded\n");
}

static const XAdminPluginDescriptor G_HelloSdkPlugin = {
	XADMIN_ABI_VERSION,
	sizeof(XAdminPluginDescriptor),
	"hello-sdk",
	"1.0.0",
	"Hello SDK Conformance Plugin",
	HelloSdk_OnLoad,
	NULL,
	HelloSdk_OnStart,
	HelloSdk_OnConfigChanged,
	HelloSdk_OnHealthCheck,
	HelloSdk_OnStop,
	HelloSdk_OnUnload
};

XADMIN_DECLARE_PLUGIN(G_HelloSdkPlugin)
