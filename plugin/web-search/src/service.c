static bool Search_Member(XS_RequestObject req, XS_ResponseObject resp, xvalue* session, int method)
{
    if (xsReqMethodID(req) != method) { Search_Reply(resp,405,"Method not allowed",NULL); return false; }
    if (!session || ValueInt(session,"id") <= 0) { Search_Reply(resp,401,"Member login required",NULL); return false; }
    int verified = XAdmin_MemberContactStatus(session);
    if (verified < 0) { Search_Reply(resp,401,"Account unavailable",NULL); return false; }
    if ((!strcmp(G_Config.verification,"phone") && !(verified&1)) ||
        (!strcmp(G_Config.verification,"any") && !verified)) {
        Search_Reply(resp,403,"Verified contact required",NULL); return false;
    }
    return true;
}
static bool Search_Query(const xvalue* body, SearchQuery* query, const SearchConfig* config)
{
    const char* fields[] = {"query","provider","count","freshness","summary"};
    if (!Search_Fields(body,fields,5)) return false;
    const char* text = Search_Text(body,"query",1024); const char* freshness = Search_Text(body,"freshness",15);
    if (!text || !*text) return false; bool visible = false; const unsigned char* p;
    for (p = (const unsigned char*)text; *p; p++) { if (*p < 32 || *p == 127) return false; if (*p != ' ') visible = true; }
    if (!visible) return false;
    memset(query,0,sizeof(*query)); strcpy(query->query,text); query->count = config->max_results < 10 ? config->max_results : 10; query->summary = true;
    if (ValueHas(body,"count") && !Search_Int(body,"count",1,config->max_results,&query->count)) return false;
    if (ValueHas(body,"summary") && !xrtValueGetBool(ValueGet(body,"summary"),&query->summary)) return false;
    if (ValueHas(body,"freshness") && !freshness) return false;
    if (!freshness) freshness = "noLimit";
    if (strcmp(freshness,"noLimit") && strcmp(freshness,"oneDay") && strcmp(freshness,"oneWeek") &&
        strcmp(freshness,"oneMonth") && strcmp(freshness,"oneYear")) return false;
    strcpy(query->freshness,freshness); return true;
}
static void Search_RequestWork(XS_ServerObject server, XS_HostObject host, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
    (void)server; (void)host;
    if (!Search_Member(req,resp,session,XHTTP_METHOD_POST)) return;
    SearchConfig config = G_Config; SearchQuery query;
    xvalue* body = Search_Body(req,8192);
    const char* id = ValueHas(body,"provider") ? Search_Text(body,"provider",15) : config.provider;
    const SearchProvider* provider = Search_Provider(id,&config);
    bool valid = Search_Query(body,&query,&config);
    xrtValueRelease(body);
    if (!valid || !provider) { Search_Reply(resp,400,"Invalid search request",NULL); return; }
    if (!provider->freshness && strcmp(query.freshness,"noLimit")) { Search_Reply(resp,400,"Provider does not support freshness filter",NULL); return; }
    char key[1001];
    if (!Search_Enabled(provider,&config) || !Search_Key(provider->id,key)) { Search_Reply(resp,503,"Search provider unavailable",NULL); return; }
    int64 owner = ValueInt(session,"id"); int slot = -1, active = 0; size_t i;
    for (i = 0; i < 16; i++) { if (G_Active[i] == owner) { active = 16; break; } if (G_Active[i]) active++; else slot = (int)i; }
    int status = active >= config.max_concurrent || slot < 0 ? 429 : Search_Reserve(owner,&config);
    if (status) { xrtSecureZero(key,sizeof(key)); Search_Reply(resp,status,status==429?"Search quota or concurrency limit reached":"Search budget unavailable",NULL); return; }
    G_Active[slot] = owner;
    unsigned char random[16] = {0}; char request_id[33] = {0};
    bool random_ok = xrtSecureRandom(random,sizeof(random));
    if (random_ok) for (i = 0; i < 16; i++) snprintf(request_id+i*2,3,"%02x",random[i]);
    xrtSecureZero(random,sizeof(random));
    xvalue* outbound = random_ok ? provider->build(&query,request_id) : NULL;
    size_t length = 0; char* json = outbound ? xrtJsonStringify(outbound,false,&length) : NULL;
    xrtValueRelease(outbound);
    int upstream_status = 0; char* raw = NULL;
    int transport = json ? XAdmin_HttpPostJson(G_Handle,req,provider->url,key,json,
        config.timeout_ms,524288,&upstream_status,&raw) : -1;
    xrtSecureZero(key,sizeof(key)); if (json) xrtSecureZero(json,length); xrtFree(json);
    G_Active[slot] = 0;
    if (transport || upstream_status < 200 || upstream_status >= 300) {
        if (raw) xrtSecureZero(raw,strlen(raw)); xrtFree(raw);
        Search_Reply(resp,transport==-2?504:502,transport==-2?"Search provider timed out":"Search provider request failed",NULL); return;
    }
    xvalue* upstream = xrtJsonParse(xrtStrView(raw)); bool truncated = false;
    xvalue* results = Search_Normalize(provider,&query,upstream,&truncated);
    xrtValueRelease(upstream); xrtSecureZero(raw,strlen(raw)); xrtFree(raw);
    if (!results) { Search_Reply(resp,502,"Invalid search provider response",NULL); return; }
    xvalue* data = ValueObject();
    ValueSetText(data,"provider",provider->id); ValueSetText(data,"request_id",request_id);
    ValueSetInt(data,"count",ValueCount(results)); ValueSetBool(data,"truncated",truncated);
    ValueSetOwn(data,"results",results); Search_Reply(resp,200,"",data);
}
static void Search_Request(XS_ServerObject server, XS_HostObject host, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
    (void)server; (void)host;
    if (!Search_Member(req,resp,session,XHTTP_METHOD_POST)) return;
    if (XAdmin_ReqBodyLen(req) > 8192) { Search_Reply(resp,400,"Search body too large",NULL); return; }
    if (XAdmin_DeferRoute(G_Handle,req,session,Search_RequestWork))
        Search_Reply(resp,503,"Search workers unavailable",NULL);
}
static void Search_Providers(XS_ServerObject server, XS_HostObject host, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
    (void)server; (void)host; if (!Search_Member(req,resp,session,XHTTP_METHOD_GET)) return;
    xvalue* providers = ValueArray(); size_t i;
    for (i = 0; i < 2; i++) {
        const SearchProvider* provider = Search_Provider(G_Providers[i].id,&G_Config); char key[1001];
        xvalue* row = ValueObject(); ValueSetText(row,"id",provider->id); ValueSetText(row,"title",provider->title);
        ValueSetBool(row,"available",Search_Enabled(provider,&G_Config) && Search_Key(provider->id,key));
        xrtSecureZero(key,sizeof(key)); ValueSetBool(row,"freshness_filter",provider->freshness);
        ValueArrayOwn(providers,row);
    }
    xvalue* data = ValueObject(); ValueSetText(data,"default_provider",G_Config.provider);
    ValueSetInt(data,"max_results",G_Config.max_results); ValueSetOwn(data,"providers",providers);
    Search_Reply(resp,200,"",data);
}
static void Search_Usage(XS_ServerObject server, XS_HostObject host, XS_RequestObject req, XS_ResponseObject resp, xvalue* session)
{
    (void)server; (void)host; if (!Search_Member(req,resp,session,XHTTP_METHOD_GET)) return;
    int64 now = (int64)time(NULL), owner = ValueInt(session,"id");
    int minute = Search_Used(owner,0,now/60), day = Search_Used(owner,1,now/86400);
    if (minute < 0 || day < 0) { Search_Reply(resp,503,"Search budget unavailable",NULL); return; }
    xvalue* data = ValueObject(); ValueSetInt(data,"minute_used",minute); ValueSetInt(data,"minute_limit",G_Config.minute_limit);
    ValueSetInt(data,"daily_used",day); ValueSetInt(data,"daily_limit",G_Config.daily_limit);
    ValueSetInt(data,"daily_reset_at",(now/86400+1)*86400); Search_Reply(resp,200,"",data);
}
