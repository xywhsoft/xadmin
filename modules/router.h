/* 复用 demo-single 的路由规则：静态优先，pattern 次之，最后选择方法槽。
 * 不改业务 URI；ANY 让旧处理函数继续按原来的方法分支工作。
 * 新处理函数可通过 xsReqRouteValue 取得路径参数。注册只在初始化阶段进行。
 */
#define ROUTE_DYNAMIC_MAX 64
#define ROUTE_PARAM_MAX 8
static xpattern* G_DynamicPattern;
static RouteInfo G_DynamicRoutes[ROUTE_DYNAMIC_MAX];
static size_t G_DynamicCount;
/* 当前请求的动态路由参数镜像（XAdmin_RouteParam ABI 用；请求锁内读写）。
 * v1 的 RouteParam 无请求参数，依赖当前请求上下文——v3 分发全程持
 * G_RequestLock，等价安全。 */
static xstrview G_PluginRouteParams[ROUTE_PARAM_MAX];
static size_t G_PluginRouteParamCount;
static const char* G_MethodNames[10] = {
	"GET", "HEAD", "POST", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE", "PATCH", "OTHER"
};

static bool RouteMethodsValid(xhttpmethod methods)
{
	return methods && !((uint32)methods & ~(uint32)XHTTP_METHOD_ANY);
}
static void RouteSetMethods(RouteInfo* route, xhttpmethod methods, XAdminRouteProc proc)
{
	int i;
	for (i = 0; i < 10; i++) if (methods & G_Methods[i]) {
		if (route->Proc[i]) printf("[route][warn] duplicate %s %s; callback overwritten\n", G_MethodNames[i], route->Path);
		route->Proc[i] = proc;
	}
}
static RouteInfo* AddStaticRouteHTTP(const char* path, xhttpmethod methods, XAdminRouteProc proc, bool bMaskBody)
{
	bool added = false; RouteInfo* route;
	if (G_Ready || !path || *path != '/' || !proc || !RouteMethodsValid(methods)) return NULL;
	route = xrtMapGetOrAdd(G_StaticRouteTableHTTP, KeyView(path), &added);
	if (!route) return NULL;
	if (added) {
		memset(route, 0, sizeof(*route)); route->Path = path;
		route->bAdmin = route->bAuth = true;
		route->bMaskBody = bMaskBody;
	}
	RouteSetMethods(route, methods, proc);
	return route;
}
static RouteInfo* AddDynamicRouteHTTP(const char* pattern, xhttpmethod methods, XAdminRouteProc proc, bool bMaskBody)
{
	size_t i; RouteInfo* route = NULL;
	if (G_Ready || !pattern || *pattern != '/' || !proc || !RouteMethodsValid(methods)) return NULL;
	for (i = 0; i < G_DynamicCount; i++) if (!strcmp(G_DynamicRoutes[i].Path, pattern)) route = &G_DynamicRoutes[i];
	if (!route) {
		char* copy;
		if (G_DynamicCount == ROUTE_DYNAMIC_MAX || !(copy = xrtStrDup(pattern))) return NULL;
		route = &G_DynamicRoutes[G_DynamicCount++];
		memset(route, 0, sizeof(*route)); route->Path = copy;
		route->bAdmin = route->bAuth = true;
		route->bMaskBody = bMaskBody;
	}
	RouteSetMethods(route, methods, proc);
	return route;
}
static bool RouteHTTP_RecompileDynamic(void)
{
	xpatternspec specs[ROUTE_DYNAMIC_MAX]; xpattern* pattern; size_t i;
	if (!G_DynamicCount) {
		xrtPatternRelease(G_DynamicPattern); G_DynamicPattern = NULL;
		return true;
	}
	/* Priority/Flags 未使用，必须显式清零：重载线程的栈上是垃圾值。 */
	memset(specs, 0, sizeof(specs));
	for (i = 0; i < G_DynamicCount; i++) {
		specs[i].Pattern = xrtStrView(G_DynamicRoutes[i].Path);
		specs[i].Value = &G_DynamicRoutes[i];
	}
	pattern = xrtPatternCompileMany(specs, G_DynamicCount);
	if (!pattern) {
		const xerror* err = xrtGetError();
		printf("[route][error] dynamic patterns conflict or are invalid: %s\n",
			err && xrtErrorMessage(err) ? xrtErrorMessage(err) : "(no error)");
		return false;
	}
	if (xrtPatternMaxCaptureCount(pattern) > ROUTE_PARAM_MAX) { xrtPatternRelease(pattern); return false; }
	xrtPatternRelease(G_DynamicPattern); G_DynamicPattern = pattern;
	return true;
}
static bool RouteHTTP_Compile(void)
{
	return RouteHTTP_RecompileDynamic();
}
/* 读取配置时查注册键，不把 pattern 当一次真实 HTTP 请求来匹配。 */
static RouteInfo* RouteHTTP_Registered(const char* path)
{
	size_t i; RouteInfo* route = xrtMapGet(G_StaticRouteTableHTTP, KeyView(path));
	if (route) return route;
	for (i = 0; i < G_DynamicCount; i++) if (!strcmp(G_DynamicRoutes[i].Path, path)) return &G_DynamicRoutes[i];
	return NULL;
}
static RouteInfo* RouteHTTP_Match(const char* path, XAdminRequest* req)
{
	RouteInfo* route = xrtMapGet(G_StaticRouteTableHTTP, KeyView(path));
	xpatternmatch match; size_t i;
	G_PluginRouteParamCount = 0; /* 静态命中/未命中都不得残留上一请求参数 */
	if (route || !G_DynamicPattern) return route;
	if (xrtPatternMatch(G_DynamicPattern, xrtStrView(path), req->param_value, ROUTE_PARAM_MAX, &match) != XPATTERN_MATCH) return NULL;
	req->param_count = match.CaptureCount;
	for (i = 0; i < req->param_count; i++) {
		xrtPatternCaptureName(G_DynamicPattern, match.PatternIndex, i, &req->param_name[i]);
		G_PluginRouteParams[i] = req->param_value[i];
	}
	G_PluginRouteParamCount = req->param_count;
	return (RouteInfo*)match.Value;
}
static void RouteHTTP_Reply405(XAdminRequest* req, const RouteInfo* route)
{
	char allow[128] = {0}; size_t used = 0; int i; char* headers;
	/* OTHER 不代表一种可枚举的 token，所以不写进 Allow。 */
	for (i = 0; i < 9; i++) if (route->Proc[i]) {
		used += snprintf(allow + used, sizeof(allow) - used, "%s%s", used ? ", " : "", G_MethodNames[i]);
	}
	headers = xrtFormat("Content-Type: application/json; charset=utf-8\r\nAllow: %s\r\n", allow);
	xsHttpReplyAuto(req, 405, headers, "{\"code\":405,\"msg\":\"method not allowed\"}", 0);
	xrtFree(headers);
}
static void RouteHTTP_Unit(void)
{
	size_t i;
	xrtMapDestroy(G_StaticRouteTableHTTP); G_StaticRouteTableHTTP = NULL;
	xrtPatternRelease(G_DynamicPattern); G_DynamicPattern = NULL;
	for (i = 0; i < G_DynamicCount; i++) xrtFree((void*)G_DynamicRoutes[i].Path);
	G_DynamicCount = 0;
}
