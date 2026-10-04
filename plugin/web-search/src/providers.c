typedef struct SearchQuery {
    char query[1025], freshness[16]; int count; bool summary;
} SearchQuery;
typedef struct SearchProvider {
    const char* id; const char* title; const char* url;
    bool freshness;
    xvalue* (*build)(const SearchQuery*, const char* request_id);
    xvalue* (*parse)(const xvalue*);
} SearchProvider;
static xvalue* Search_BochaBuild(const SearchQuery* query, const char* request_id)
{
    (void)request_id; xvalue* value = ValueObject();
    ValueSetText(value,"query",query->query); ValueSetText(value,"freshness",query->freshness);
    ValueSetBool(value,"summary",query->summary); ValueSetInt(value,"count",query->count); return value;
}
static xvalue* Search_BochaParse(const xvalue* value)
{
    if (ValueInt(value,"code") != 200) return NULL;
    return ValueGet(ValueGet(ValueGet(value,"data"),"webPages"),"value");
}
static xvalue* Search_ZaiBuild(const SearchQuery* query, const char* request_id)
{
    xvalue* value = ValueObject();
    ValueSetText(value,"search_engine","search-prime"); ValueSetText(value,"search_query",query->query);
    ValueSetInt(value,"count",query->count); ValueSetText(value,"request_id",request_id); return value;
}
static xvalue* Search_ZaiParse(const xvalue* value) { return ValueGet(value,"search_result"); }
static xvalue* Search_GlmBuild(const SearchQuery* query, const char* request_id)
{
    xvalue* value = Search_ZaiBuild(query,request_id);
    if (value) ValueSetText(value,"search_engine","search_pro");
    return value;
}
static const SearchProvider G_Providers[] = {
    {"bocha","博查","https://api.bocha.cn/v1/web-search",true,Search_BochaBuild,Search_BochaParse},
    {"zai","z.ai","https://api.z.ai/api/paas/v4/web_search",false,Search_ZaiBuild,Search_ZaiParse}
};
/* Region is an administrator-controlled allowlist, not a client-supplied URL.
 * The stable zai provider ID covers either regional account, never a fallback. */
static const SearchProvider G_GlmProvider = {
    "zai","智谱 GLM（国内）","https://open.bigmodel.cn/api/paas/v4/web_search",false,Search_GlmBuild,Search_ZaiParse
};
static const SearchProvider* Search_Provider(const char* id, const SearchConfig* config)
{
    size_t i; for (i = 0; i < 2; i++) if (id && !strcmp(id,G_Providers[i].id))
        return i == 1 && !strcmp(config->zai_region,"cn") ? &G_GlmProvider : &G_Providers[i];
    return NULL;
}
static bool Search_Enabled(const SearchProvider* provider, const SearchConfig* config)
{ return !strcmp(provider->id,"bocha") ? config->bocha_enabled : config->zai_enabled; }
static bool Search_ValidURL(const char* url)
{
    if (!url || (strncmp(url,"https://",8) && strncmp(url,"http://",7)) || strpbrk(url,"\r\n\t \\")) return false;
    const char* authority = strchr(url,':') + 3; size_t n = strcspn(authority,"/?#");
    if (!n || memchr(authority,'@',n)) return false;
    const unsigned char* p = (const unsigned char*)url;
    for (; *p; p++) if (*p < 32 || *p == 127) return false;
    return true;
}
static bool Search_CopyText(xvalue* out, const char* name, const char* text, size_t limit)
{
    if (!text) text = ""; size_t n = strlen(text);
    if (n > limit) { n = limit; while (n && ((unsigned char)text[n] & 0xc0) == 0x80) n--; }
    char* copy = xrtStrDupN(text,n); bool ok = copy && ValueSetText(out,name,copy);
    xrtFree(copy); return ok;
}
static xvalue* Search_Normalize(const SearchProvider* provider, const SearchQuery* query,
    const xvalue* raw, bool* truncated)
{
    xvalue* items = provider->parse(raw); *truncated = false;
    if (!items || xrtValueType(items) != XVALUE_ARRAY || ValueCount(items) > 500) return NULL;
    xvalue* results = ValueArray(); if (!results) return NULL; uint32 i;
    bool bocha = !strcmp(provider->id,"bocha");
    for (i = 0; i < ValueCount(items); i++) {
        xvalue* item = xrtValueArrayGet(items,i);
        const char* url = Search_Text(item,bocha?"url":"link",2048);
        const char* title = Search_Text(item,bocha?"name":"title",16384);
        if (!Search_ValidURL(url) || !title || !*title) continue;
        uint32 j; bool duplicate = false;
        for (j = 0; j < ValueCount(results); j++)
            if (!strcmp(url,ValueText(xrtValueArrayGet(results,j),"url"))) { duplicate = true; break; }
        if (duplicate) continue;
        if (ValueCount(results) >= (uint32)query->count) { *truncated = true; break; }
        const char* snippet = Search_Text(item,bocha?"snippet":"content",524288);
        if (bocha && query->summary) {
            const char* summary = Search_Text(item,"summary",524288); if (summary && *summary) snippet = summary;
        }
        if (snippet && strlen(snippet) > 2048) *truncated = true;
        xvalue* row = ValueObject();
        bool ok = row && Search_CopyText(row,"title",title,512) && ValueSetText(row,"url",url) &&
            Search_CopyText(row,"snippet",snippet,2048) &&
            Search_CopyText(row,"site",Search_Text(item,bocha?"siteName":"media",16384),256) &&
            Search_CopyText(row,"published_at",Search_Text(item,bocha?"datePublished":"publish_date",1024),64);
        if (!ok) { xrtValueRelease(row); xrtValueRelease(results); return NULL; }
        if (!ValueArrayOwn(results,row)) { xrtValueRelease(results); return NULL; }
    }
    return results;
}
