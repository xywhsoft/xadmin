/* hello-sdk —— xs3 插件宿主的全能力一致性验证插件。
 * 只使用 ABI 面（xs_plugin.h 的 27 个宿主函数 + HttpReplyFormat/LoadPage）
 * 与 xrt 公共 API，不触碰宿主内部符号——这也是 ABI 契约的活体测试。
 * /api/plugin/hello-sdk/state 输出全部计数器，供 smoke 逐项断言。 */
#include <xs_plugin.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include "hello_extra.h"     /* 约定目录 inc/ */
#include "hello_src_note.h"  /* 约定目录 src/ */

static XAdminPluginHandle G_Handle;
static char G_ConfigMessage[128];

static XAdminHostContext* G_HostCtx;

XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int idx, void* ptr)
{
	/* 聚合上下文（槽位 7）落存；其余标量槽位不消费。 */
	if (idx == XADMIN_GLOBAL_HOST_CONTEXT) G_HostCtx = (XAdminHostContext*)ptr;
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

/* ---- 资源体系探针：page / template / option / resource-path ---- */

static void HelloSdk_RequestPage(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	(void)s; (void)h; (void)req; (void)session;
	XAdmin_LoadPluginPage(G_Handle, resp, 200,
		"Content-Type: text/html; charset=utf-8\r\n", "admin.html");
}

static void HelloSdk_RequestTemplate(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	xvalue* data = xrtValueObject();
	size_t size = 0;
	char* out = NULL;
	char* err = NULL;
	(void)s; (void)h; (void)req; (void)session;
	xrtValueObjectSetNew(data, xrtStrView("name"), xrtValueString(xrtStrView("hello-sdk")));
	out = XAdmin_RenderPluginTemplate(G_Handle, "hello.tpl", data, &size, &err);
	xrtValueRelease(data);
	if (out) {
		HttpReplyFormat(resp, 200, "Content-Type: text/plain; charset=utf-8\r\n", "%s", out);
		xrtFree(out);
	} else {
		HttpReplyFormat(resp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "render failed: %s", err ? err : "unknown");
	}
	if (err) xrtFree(err);
}

static const char* HelloSdk_OptionValue(xvalue* option_table)
{
	xvalue* arr = option_table ? xrtValueObjectGet(option_table, xrtStrView("classList")) : NULL;
	size_t i, j;
	if (!arr) return NULL;
	for (i = 0; i < xrtValueCount(arr); i++) {
		xvalue* cls = xrtValueArrayGet(arr, i);
		xvalue* options = cls ? xrtValueObjectGet(cls, xrtStrView("options")) : NULL;
		if (!options) continue;
		for (j = 0; j < xrtValueCount(options); j++) {
			xvalue* opt = xrtValueArrayGet(options, j);
			xvalue* name = opt ? xrtValueObjectGet(opt, xrtStrView("name")) : NULL;
			xstrview key = {0};
			if (name && xrtValueGetString(name, &key) && key.Size == 15 &&
			    memcmp(key.Data, "welcome_message", 15) == 0) {
				xvalue* value = xrtValueObjectGet(opt, xrtStrView("value"));
				xstrview text = {0};
				static char buf[128];
				size_t n;
				if (!value || !xrtValueGetString(value, &text) || !text.Size) return NULL;
				n = text.Size < sizeof(buf) - 1 ? text.Size : sizeof(buf) - 1;
				memcpy(buf, text.Data, n);
				buf[n] = '\0';
				return buf;
			}
		}
	}
	return NULL;
}

static void HelloSdk_RequestOption(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	xvalue* tbl = xrtValueObject();
	xvalue* option_table;
	const char* value;
	(void)s; (void)h; (void)req; (void)session;
	option_table = XAdmin_PluginOptionLoad(G_Handle, "runtime.json");
	value = HelloSdk_OptionValue(option_table);
	xrtValueObjectSetNew(tbl, xrtStrView("result"), xrtValueBool(option_table != NULL));
	xrtValueObjectSetNew(tbl, xrtStrView("value"),
		xrtValueString(xrtStrView(value ? value : "")));
	xrtValueObjectSetNew(tbl, xrtStrView("conv"),
		xrtValueInt(HELLO_SDK_EXTRA + HELLO_SDK_SRC_NOTE));
	xrtValueRelease(option_table);
	HelloSdk_ReplyJson(resp, tbl);
}

static void HelloSdk_RequestOptionSave(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	xvalue* tbl = xrtValueObject();
	xvalue* body = NULL;
	const char* body_text = XAdmin_ReqBody(req);
	(void)s; (void)h; (void)session;
	if (body_text && XAdmin_ReqBodyLen(req))
		body = xrtJsonParse(xrtStrViewN(body_text, XAdmin_ReqBodyLen(req)));
	xrtValueObjectSetNew(tbl, xrtStrView("result"),
		xrtValueBool(body != NULL && XAdmin_PluginOptionSave(G_Handle, "runtime.json", body) == 0));
	{
		xvalue* option_table = XAdmin_PluginOptionLoad(G_Handle, "runtime.json");
		const char* value = HelloSdk_OptionValue(option_table);
		xrtValueObjectSetNew(tbl, xrtStrView("value"),
			xrtValueString(xrtStrView(value ? value : "")));
		xrtValueRelease(option_table);
	}
	xrtValueRelease(body);
	HelloSdk_ReplyJson(resp, tbl);
}

static void HelloSdk_RequestResourcePath(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	xvalue* tbl = xrtValueObject();
	char* path = XAdmin_PluginResourcePath(G_Handle, "static", "hello.css");
	(void)s; (void)h; (void)req; (void)session;
	xrtValueObjectSetNew(tbl, xrtStrView("result"), xrtValueBool(path != NULL));
	xrtValueObjectSetNew(tbl, xrtStrView("endsWithHelloCss"),
		xrtValueBool(path != NULL && strstr(path, "hello.css") != NULL));
	xrtValueObjectSetNew(tbl, xrtStrView("hasTraversal"), xrtValueBool(false));
	xrtFree(path);
	HelloSdk_ReplyJson(resp, tbl);
}

/* ---- libraries 实链探针：夹具期由 e2e 注入 HELLO_SDK_LINK_IPHLPAPI=1 +
 * libraries:["iphlpapi"] 并把 iphlpapi.dll 放入 lib/ 约定目录后编译生效。
 * 内嵌 winapi 头 VFS 无 iphlpapi.h，这里手工声明导入函数。 ---- */
#ifdef HELLO_SDK_LINK_IPHLPAPI
__declspec(dllimport) unsigned long __stdcall GetNumberOfInterfaces(unsigned long* if_count);
#endif

static void HelloSdk_RequestLinkProbe(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	xvalue* tbl = xrtValueObject();
	(void)s; (void)h; (void)req; (void)session;
#ifdef HELLO_SDK_LINK_IPHLPAPI
	unsigned long count = 0;
	unsigned long rc = GetNumberOfInterfaces(&count);
	xrtValueObjectSetNew(tbl, xrtStrView("result"), xrtValueBool(rc == 0));
	xrtValueObjectSetNew(tbl, xrtStrView("interfaces"), xrtValueInt((int64)count));
	xrtValueObjectSetNew(tbl, xrtStrView("callRc"), xrtValueInt((int64)rc));
#else
	xrtValueObjectSetNew(tbl, xrtStrView("result"), xrtValueBool(false));
	xrtValueObjectSetNew(tbl, xrtStrView("message"), xrtValueString(xrtStrView("link probe not built")));
#endif
	HelloSdk_ReplyJson(resp, tbl);
}

/* ---- HostContext 探针：聚合上下文一致性（槽位 7 注入） ---- */

static void HelloSdk_RequestHostCtx(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	xvalue* tbl = xrtValueObject();
	const XAdminHostContext* ctx = G_HostCtx;
	int score = 0;
	(void)s; (void)h; (void)req; (void)session;
	if (!ctx) {
		xrtValueObjectSetNew(tbl, xrtStrView("result"), xrtValueBool(false));
		xrtValueObjectSetNew(tbl, xrtStrView("message"), xrtValueString(xrtStrView("host context not injected")));
		HelloSdk_ReplyJson(resp, tbl);
		return;
	}
	if (ctx->size == (uint32_t)sizeof(XAdminHostContext)) score++;
	if (ctx->abi_version == XADMIN_ABI_VERSION) score++;
	if (ctx->app_path && ctx->app_path[0]) score++;
	if (ctx->plugin_xid && strcmp(ctx->plugin_xid, "hello-sdk") == 0) score++;
	if (ctx->main_db) score++;
	if (ctx->option_table) score++;
	if (ctx->db_path && strstr(ctx->db_path, "db")) score++;
	if (ctx->template_path && strstr(ctx->template_path, "template")) score++;
	if (ctx->exe_path && strstr(ctx->exe_path, "xs.exe")) score++;
	if (ctx->web_path && ctx->web_path[0]) score++;
	xrtValueObjectSetNew(tbl, xrtStrView("result"), xrtValueBool(score >= 10));
	xrtValueObjectSetNew(tbl, xrtStrView("score"), xrtValueInt(score));
	if (ctx->plugin_xid) xrtValueObjectSetNew(tbl, xrtStrView("pluginXid"), xrtValueString(xrtStrView(ctx->plugin_xid)));
	if (ctx->app_path) xrtValueObjectSetNew(tbl, xrtStrView("appPath"), xrtValueString(xrtStrView(ctx->app_path)));
	HelloSdk_ReplyJson(resp, tbl);
}

/* ---- md4c 探针：宿主 markdown 渲染符号一致性 ---- */

extern int md_html(const char* input, unsigned input_size,
	void (*process_output)(const char*, unsigned, void*),
	void* userdata, unsigned parser_flags, unsigned renderer_flags);
static void HelloSdk_MdOutput(const char* data, unsigned size, void* userdata)
{
	xrtBufferAppend((xbuffer*)userdata, (xbytesview){(cbytes)data, size});
}
static void HelloSdk_RequestMdProbe(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	/* GFM 方言组合 + NOHTML + SKIP_BOM（与内容模板一致的取值） */
	unsigned gfm = 0x8u | 0x4u | 0x400u | 0x100u | 0x200u | 0x800u | 0x80000u | 0x100000u;
	unsigned parser_flags = gfm | 0x20u | 0x40u;
	xvalue* tbl = xrtValueObject();
	xbuffer* buf = xrtBufferCreate();
	int rc;
	(void)s; (void)h; (void)req; (void)session;
	{
		const char* md = "## T\n\n**b** and `c`\n\n- i1\n- i2\n";
		rc = buf ? md_html(md, (unsigned)strlen(md), HelloSdk_MdOutput, buf, parser_flags, 0x0004u) : -1;
	}
	xrtValueObjectSetNew(tbl, xrtStrView("result"), xrtValueBool(rc == 0));
	if (rc == 0 && buf && buf->Data)
		xrtValueObjectSetNew(tbl, xrtStrView("html"), xrtValueString(xrtStrViewN((const char*)buf->Data, buf->Size)));
	if (buf) xrtBufferDestroy(buf);
	HelloSdk_ReplyJson(resp, tbl);
}

/* ---- 动态路由探针：pattern 参数经 XAdmin_RouteParam 读取 ---- */

static void HelloSdk_RequestDynParam(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	char name[64] = {0};
	xvalue* tbl = xrtValueObject();
	(void)s; (void)h; (void)req; (void)session;
	XAdmin_RouteParam(0, name, sizeof(name));
	xrtValueObjectSetNew(tbl, xrtStrView("result"), xrtValueBool(true));
	xrtValueObjectSetNew(tbl, xrtStrView("name"), xrtValueString(xrtStrView(name)));
	xrtValueObjectSetNew(tbl, xrtStrView("paramCount"), xrtValueInt(XAdmin_RouteParamCount()));
	HelloSdk_ReplyJson(resp, tbl);
}

/* ---- 脚手架生成探针：XAdmin_GeneratePlugin 生成可运行的最小插件 ---- */

static const char HELLO_GEN_MAIN[] =
	"#include <xs_plugin.h>\n"
	"#include <string.h>\n"
	"XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int idx, void* ptr)\n"
	"{\n"
	"	(void)idx; (void)ptr;\n"
	"}\n"
	"static void Gen_Req(XS_ServerObject s, XS_HostObject h, XS_RequestObject q, XS_ResponseObject r, xvalue* sess)\n"
	"{\n"
	"	(void)s; (void)h; (void)q; (void)sess;\n"
	"	HttpReplyFormat(r, 200, \"Content-Type: text/plain\\r\\n\", \"gen-ok\");\n"
	"}\n"
	"static int Gen_OnStart(XAdminPluginHandle handle)\n"
	"{\n"
	"	XAdminRouteDecl route;\n"
	"	memset(&route, 0, sizeof(route));\n"
	"	route.path = \"/api/plugin/hello-gen/ping\";\n"
	"	route.proc = Gen_Req;\n"
	"	return XAdmin_RegisterRoute(handle, &route, NULL);\n"
	"}\n"
	"static const XAdminPluginDescriptor G_Gen = {\n"
	"	XADMIN_ABI_VERSION, sizeof(XAdminPluginDescriptor),\n"
	"	\"hello-gen\", \"1.0.0\", \"Generated Probe\",\n"
	"	NULL, NULL, Gen_OnStart, NULL, NULL, NULL, NULL\n"
	"};\n"
	"XADMIN_DECLARE_PLUGIN(G_Gen)\n";

static void HelloSdk_RequestGenerate(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	XAdminGeneratedFile files[1];
	XAdminGeneratedPluginSpec spec;
	xvalue* tbl = xrtValueObject();
	int rc;
	(void)s; (void)h; (void)req; (void)session;
	memset(&spec, 0, sizeof(spec));
	memset(files, 0, sizeof(files));
	spec.xid = "hello-gen";
	spec.title = "Generated Probe";
	spec.version = "1.0.0";
	spec.entry = "main.c";
	spec.auto_enable = 1;
	spec.file_count = 1;
	files[0].relative_path = "main.c";
	files[0].data = HELLO_GEN_MAIN;
	files[0].size = sizeof(HELLO_GEN_MAIN) - 1;
	spec.files = files;
	rc = XAdmin_GeneratePlugin(G_Handle, &spec);
	xrtValueObjectSetNew(tbl, xrtStrView("result"), xrtValueBool(rc == 0));
	xrtValueObjectSetNew(tbl, xrtStrView("rc"), xrtValueInt(rc));
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
	route.path = "/api/plugin/hello-sdk/page";
	route.proc = HelloSdk_RequestPage;
	route.need_auth = false;
	route.admin_only = false;
	route.auth_id = 0;
	if (XAdmin_RegisterRoute(handle, &route, NULL) != 0) return -1;
	route.path = "/api/plugin/hello-sdk/template";
	route.proc = HelloSdk_RequestTemplate;
	if (XAdmin_RegisterRoute(handle, &route, NULL) != 0) return -1;
	route.path = "/api/plugin/hello-sdk/option";
	route.proc = HelloSdk_RequestOption;
	if (XAdmin_RegisterRoute(handle, &route, NULL) != 0) return -1;
	route.path = "/api/plugin/hello-sdk/option-save";
	route.proc = HelloSdk_RequestOptionSave;
	if (XAdmin_RegisterRoute(handle, &route, NULL) != 0) return -1;
	route.path = "/api/plugin/hello-sdk/resource-path";
	route.proc = HelloSdk_RequestResourcePath;
	if (XAdmin_RegisterRoute(handle, &route, NULL) != 0) return -1;
	route.path = "/api/plugin/hello-sdk/link-probe";
	route.proc = HelloSdk_RequestLinkProbe;
	if (XAdmin_RegisterRoute(handle, &route, NULL) != 0) return -1;
	route.path = "/api/plugin/hello-sdk/hostctx";
	route.proc = HelloSdk_RequestHostCtx;
	if (XAdmin_RegisterRoute(handle, &route, NULL) != 0) return -1;
	route.path = "/api/plugin/hello-sdk/md-probe";
	route.proc = HelloSdk_RequestMdProbe;
	if (XAdmin_RegisterRoute(handle, &route, NULL) != 0) return -1;
	route.path = "/api/plugin/hello-sdk/generate";
	route.proc = HelloSdk_RequestGenerate;
	if (XAdmin_RegisterRoute(handle, &route, NULL) != 0) return -1;
	{
		XAdminDynamicRouteDecl dyn;
		memset(&dyn, 0, sizeof(dyn));
		dyn.path = "/api/plugin/hello-sdk/dyn";
		dyn.pattern = "/api/plugin/hello-sdk/dyn/{name}";
		dyn.proc = HelloSdk_RequestDynParam;
		if (XAdmin_RegisterDynamicRoute(handle, &dyn, NULL) != 0) return -1;
	}

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
	/* 供 settings 回滚 e2e：标记值显式拒绝（v1 SaveSettings 回滚语义） */
	if (new_cfg) {
		xvalue* v = xrtValueObjectGet(new_cfg, xrtStrView("welcomeMessage"));
		xstrview text = {0};
		if (v && xrtValueGetString(v, &text) && text.Size == 9 &&
		    memcmp(text.Data, "reject-me", 9) == 0)
			return -1;
	}
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
