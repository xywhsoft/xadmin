/* Test-only composition: real plugin + real host, no provider credentials,
 * billing or remote network. Native HTTPS is tested separately by sms_unit. */
#define ServiceInit XAdmin_ServiceInit
#include "../main.c"
#undef ServiceInit
static char G_SearchTestMarker[65];
static void Search_TestState(XS_ServerObject server, XS_HostObject host, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
    (void)server; (void)req; (void)session;
    XS_ReloadId id = xsReqMethodID(req) == XHTTP_METHOD_POST ? xsReloadHostSubmit((XS_HostInfo*)host) : 0;
    HttpReplyFormat(resp,200,HTTP_CT_JSON,"{\"marker\":\"%s\",\"reload_id\":%llu}",G_SearchTestMarker,(unsigned long long)id);
}

static bool Search_TestTransport(void* engine, const XAHttpRequest* request,
    unsigned timeout_ms, size_t max_body, int* status, char** response)
{
    (void)engine; (void)timeout_ms;
    xvalue* body = xrtJsonParse(xrtStrView(request->body));
    bool bocha = !strcmp(request->url,"https://api.bocha.cn/v1/web-search");
    bool zai = !strcmp(request->url,"https://api.z.ai/api/paas/v4/web_search");
    bool glm = !strcmp(request->url,"https://open.bigmodel.cn/api/paas/v4/web_search");
    const char* query = ValueText(body,bocha?"query":"search_query");
    bool contract = query && (bocha || zai || glm) && max_body == 524288 &&
        request->header_count == 1 && !strcmp(request->names[0],"Authorization") &&
        !strcmp(request->values[0],bocha?"Bearer test-bocha-secret":"Bearer test-zai-secret") &&
        !strcmp(request->content_type,"application/json") && ValueInt(body,"count") > 0;
    bool summary;
    if (bocha) contract = contract && ValueText(body,"freshness") && xrtValueGetBool(ValueGet(body,"summary"),&summary);
    if (zai || glm) contract = contract && !strcmp(ValueText(body,"search_engine"),glm?"search_pro":"search-prime") &&
        XA_IsHex(ValueText(body,"request_id"),32) && !ValueHas(body,"user_id") && !ValueHas(body,"search_recency_filter");
    if (!contract) { xrtValueRelease(body); return false; }
    char* native_path = xrtPathJoin(AppPath,"temp/search-native-url");
    char* native_url = xrtFileReadAllLimit(native_path,2000,NULL); xrtFree(native_path);
    if (native_url) {
        XAHttpRequest native = *request;
        snprintf(native.url,sizeof(native.url),"%s/%s",native_url,bocha?"bocha":"zai");
        char* ca_path = xrtPathJoin(AppPath,"temp/search-native-ca.pem");
        char* ca = xrtFileReadAllLimit(ca_path,8192,NULL); xrtFree(ca_path);
        bool result = XA_HttpsHttp(engine,!strcmp(query,"native-untrusted")?NULL:ca,&native,timeout_ms,max_body,status,response);
        xrtFree(ca); xrtFree(native_url); xrtValueRelease(body); return result;
    }
    *status = 200;
    if (!strcmp(query,"slow")) {
        char* path = xrtPathJoin(AppPath,"temp/search-entered");
        xrtFileWriteAll(path,(xbytesview){(cbytes)"1",1}); xrtFree(path);
        path = xrtPathJoin(AppPath,"temp/search-release");
        xfuture* wait = NULL; xpromise* promise = xrtPromiseCreate(&wait,NULL);
        int attempts = 0;
        while (!xrtFileExists(path) && attempts++ < 400) xrtFutureWaitFor(wait,20000);
        xrtPromiseDestroy(promise); xrtFutureDestroy(wait);
        xrtFree(path);
    }
    if (!strcmp(query,"upstream401")) { *status = 401; *response = xrtStrDup("{\"message\":\"secret upstream body test-bocha-secret\"}"); }
    else if (!strcmp(query,"upstream429")) { *status = 429; *response = xrtStrDup("{}"); }
    else if (!strcmp(query,"redirect")) { *status = 302; *response = xrtStrDup("{}"); }
    else if (!strcmp(query,"malformed")) *response = xrtStrDup("{bad}");
    else if (!strcmp(query,"application_error")) *response = xrtStrDup("{\"code\":401,\"message\":\"test-bocha-secret\"}");
    else if (!strcmp(query,"empty")) *response = xrtStrDup(bocha?"{\"code\":200,\"data\":{\"webPages\":{\"value\":[]}}}":"{\"search_result\":[]}");
    else if (!strcmp(query,"transport")) { xrtValueRelease(body); return false; }
    else if (bocha) *response = xrtStrDup("{\"code\":200,\"data\":{\"webPages\":{\"value\":[{\"name\":\"测试标题\",\"url\":\"https://example.com/1\",\"snippet\":\"short\",\"summary\":\"完整摘要\",\"siteName\":\"Example\",\"datePublished\":\"2026-10-04\"},{\"name\":\"duplicate\",\"url\":\"https://example.com/1\"},{\"name\":\"unsafe\",\"url\":\"javascript:alert(1)\"},{\"name\":\"second\",\"url\":\"https://example.com/2\",\"snippet\":\"second snippet\"}]}}}");
    else *response = xrtStrDup("{\"id\":\"task\",\"search_result\":[{\"title\":\"测试标题\",\"link\":\"https://example.com/1\",\"content\":\"完整摘要\",\"media\":\"Example\",\"publish_date\":\"2026-10-04\"},{\"title\":\"second\",\"link\":\"https://example.com/2\"}]}");
    xrtValueRelease(body); return *response != NULL;
}
void ServiceInit(XS_HostInfo* host)
{
    G_PluginHttpTransport = Search_TestTransport; XAdmin_ServiceInit(host);
    if (!G_Ready) return;
    XA_Random(G_SearchTestMarker); G_Ready = false;
    RouteInfo* route = AddStaticRouteHTTP("/__test/search-state",XHTTP_METHOD_ANY,Search_TestState,false);
    if (route) { route->bAuth = false; route->bAdmin = false; }
    G_Ready = RouteHTTP_Compile();
}
