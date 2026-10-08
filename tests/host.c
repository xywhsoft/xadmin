/* 仅测试配置使用此入口。测试 URI 不编入正式 main.c，也不写入 URI 权限库。 */
#define ServiceInit XAdmin_ServiceInit
#include "../main.c"
#undef ServiceInit

static void ProbeGet(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	xstrview id = {0}; (void)s; (void)h; (void)session;
	if (xsReqRouteValue(req, "id", &id)) xsHttpReplyFormat(resp, 200, HTTP_CT_TEXT, "get:%.*s", (int)id.Size, id.Data);
	else xsHttpReplyAuto(resp, 200, HTTP_CT_TEXT, "get:static", 0);
}
static void ProbePost(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	(void)s; (void)h; (void)req; (void)session;
	xsHttpReplyAuto(resp, 200, HTTP_CT_TEXT, "post", 0);
}
static void ProbeReplacement(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	(void)s; (void)h; (void)req; (void)session;
	xsHttpReplyAuto(resp, 200, HTTP_CT_TEXT, "replacement", 0);
}
static void ProbeExpire(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	char id[128]; xvalue* value; (void)s; (void)h; (void)session;
	xsReqCookieValue(req, "XSID", id, sizeof(id));
	value = Session_Acquire(true, id);
	ValueSetInt(value, "_expireTime", XAdmin_UnixNowUs() - 1);
	xrtValueRelease(value);
	Session_Prune(G_AdminSessions);
	xsHttpReplyAuto(resp, 200, HTTP_CT_TEXT, "expired", 0);
}

static void ProbeCrash(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	(void)s; (void)h; (void)req; (void)session; (void)resp;
	*(volatile int*)0 = 1;   /* 崩溃捕获链路验证专用 */
}
/* 隔离夹具可调整会话时钟，验证分钟级到期而无需真实等待。 */
static void ProbeAdminSession(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	char id[128] = {0}; xvalue* value; (void)s; (void)h; (void)session;
	xsReqCookieValue(req, "XSID", id, sizeof(id));
	value = ValueGet(G_AdminSessions, id);
	if (!value) { xsHttpReplyAuto(resp, 404, HTTP_CT_JSON, "{}", 0); return; }
	if (xsReqMethodID(req) == XHTTP_METHOD_POST) {
		xvalue* body = JsonParseN(req->body, req->body_size);
		int64 active = XAdmin_UnixNowUs() - ValueInt(body, "idle_seconds") * 1000000;
		ValueSetInt(value, "_activeTime", active);
		if (ValueBool(body, "remember_elapsed")) ValueSetInt(value, "_createTime", active);
		ValueSetInt(value, "_expireTime", ValueBool(body, "revoke") ? -1 :
			ValueBool(body, "expire") ? XAdmin_UnixNowUs() - 1 :
			ValueBool(value, "_remember") ? ValueInt(value, "_createTime") + (int64)Session_AdminRememberSeconds() * 1000000 : active + (int64)Session_AdminTimeoutSeconds() * 1000000);
		xrtValueRelease(body);
	}
	xsHttpReplyFormat(resp, 200, HTTP_CT_JSON,
		"{\"active\":%lld,\"expires\":%lld,\"timeout\":%d,\"remember\":%d,\"created\":%lld,\"remembered\":%s}",
		(long long)ValueInt(value, "_activeTime"), (long long)ValueInt(value, "_expireTime"),
		Session_AdminTimeoutSeconds(), Session_AdminRememberSeconds(), (long long)ValueInt(value,"_createTime"), ValueBool(value,"_remember") ? "true" : "false");
}
static void ProbeReload(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	XS_ReloadId id; (void)s; (void)req; (void)session;
	id = xsReloadHostSubmit((XS_HostInfo*)h);
	xsHttpReplyFormat(resp, id ? 202 : 500, HTTP_CT_JSON, "{\"id\":%llu}", (unsigned long long)id);
}
static void ProbeEcho(XS_ServerObject s, XS_HostObject h, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
	(void)s; (void)h; (void)session;
	xsHttpReplyAuto(resp, 200, HTTP_CT_TEXT, req->body, req->body_size);
}
static void Public(RouteInfo* route)
{
	if (route) { route->bAuth = false; route->bAdmin = false; }
}
void ServiceInit(XS_HostInfo* host)
{
	XAdmin_ServiceInit(host);
	if (!G_Ready) return;
	G_Ready = false; /* 正式运行前增加测试节点。运行中的注册被拒绝。 */
	Public(AddStaticRouteHTTP("/__test/method", XHTTP_METHOD_GET, ProbeGet, false));
	AddStaticRouteHTTP("/__test/method", XHTTP_METHOD_POST, ProbePost, false);
	AddStaticRouteHTTP("/__test/method", XHTTP_METHOD_GET, ProbeReplacement, false);
	Public(AddStaticRouteHTTP("/__test/item/new", XHTTP_METHOD_GET, ProbeGet, false));
	Public(AddDynamicRouteHTTP("/__test/item/{id}", XHTTP_METHOD_GET, ProbeGet, false));
	AddDynamicRouteHTTP("/__test/item/{id}", XHTTP_METHOD_POST, ProbePost, false);
	AddDynamicRouteHTTP("/__test/item/{id}", XHTTP_METHOD_POST, ProbeReplacement, false);
	Public(AddStaticRouteHTTP("/__test/crud", XHTTP_METHOD_CRUD, ProbeGet, false));
	Public(AddStaticRouteHTTP("/__test/expire", XHTTP_METHOD_POST, ProbeExpire, false));
	Public(AddStaticRouteHTTP("/__test/admin-session", XHTTP_METHOD_GET, ProbeAdminSession, false));
	AddStaticRouteHTTP("/__test/admin-session", XHTTP_METHOD_POST, ProbeAdminSession, false);
	Public(AddStaticRouteHTTP("/__test/crash", XHTTP_METHOD_GET, ProbeCrash, false));
	Public(AddStaticRouteHTTP("/__test/reload", XHTTP_METHOD_POST, ProbeReload, false));
	Public(AddStaticRouteHTTP("/__test/echo", XHTTP_METHOD_POST, ProbeEcho, false));
	G_Ready = RouteHTTP_Compile();
}
