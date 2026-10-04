/* Only non-secret policy is stored in the generic plugin options. Keys live
 * in process environment or the private data directory, never static assets. */
static bool Search_Int(const xvalue* value, const char* key, int min, int max, int* out)
{
    int64 number;
    if (!xrtValueGetInt(ValueGet(value, key), &number) || number < min || number > max) return false;
    *out = (int)number; return true;
}
static int Search_ConfigChanged(XAdminPluginHandle handle, xvalue* value)
{
    (void)handle; SearchConfig config = {0};
    const char* fields[] = {"default_provider", "verification", "minute_limit", "daily_limit",
        "global_daily_limit", "max_results", "timeout_ms", "max_concurrent", "bocha_enabled", "zai_enabled", "zai_region"};
    const char* provider = Search_Text(value, "default_provider", 15);
    const char* verification = Search_Text(value, "verification", 15);
    /* Existing policy files remain international unless explicitly switched.
     * Never infer the destination from a secret or forward a key to both APIs. */
    const char* region = ValueHas(value,"zai_region") ? Search_Text(value,"zai_region",15) : "global";
    if (!Search_Fields(value, fields, 11) || !region || (strcmp(region,"global") && strcmp(region,"cn")) ||
        !provider || (strcmp(provider,"bocha") && strcmp(provider,"zai")) ||
        !verification || (strcmp(verification,"phone") && strcmp(verification,"any") && strcmp(verification,"none")) ||
        !Search_Int(value,"minute_limit",1,60,&config.minute_limit) ||
        !Search_Int(value,"daily_limit",1,10000,&config.daily_limit) ||
        !Search_Int(value,"global_daily_limit",1,1000000,&config.global_daily_limit) ||
        !Search_Int(value,"max_results",1,50,&config.max_results) ||
        !Search_Int(value,"timeout_ms",1000,60000,&config.timeout_ms) ||
        !Search_Int(value,"max_concurrent",1,16,&config.max_concurrent) ||
        !xrtValueGetBool(ValueGet(value,"bocha_enabled"),&config.bocha_enabled) ||
        !xrtValueGetBool(ValueGet(value,"zai_enabled"),&config.zai_enabled)) return -1;
    strcpy(config.provider,provider); strcpy(config.verification,verification); strcpy(config.zai_region,region);
    G_Config = config; return 0;
}
static bool Search_ValidKey(const char* text)
{
    if (!text || strlen(text) > 1000) return false;
    for (; *text; text++) if ((unsigned char)*text < 33 || (unsigned char)*text > 126) return false;
    return true;
}
static xvalue* Search_ReadCredentials(bool* invalid)
{
    *invalid = false;
    if (!G_CredentialPath) { *invalid = true; return NULL; }
    if (!xrtFileExists(G_CredentialPath)) return ValueObject();
    xfileinfo info;
    if (!xrtPathStat(G_CredentialPath, true, &info) || info.Size > 4096) { *invalid = true; return NULL; }
    size_t n = 0; char* bytes = xrtFileReadAllLimit(G_CredentialPath, 4096, &n);
    xvalue* value = bytes && n <= 4096 && !memchr(bytes,0,n) ? xrtJsonParse(xrtStrViewN(bytes,n)) : NULL;
    if (bytes) xrtSecureZero(bytes,n); xrtFree(bytes);
    const char* fields[] = {"bocha","zai"};
    bool ok = Search_Fields(value,fields,2); size_t i;
    for (i = 0; ok && i < 2; i++) if (ValueHas(value,fields[i])) {
        const char* key = Search_Text(value,fields[i],1000); ok = Search_ValidKey(key);
    }
    if (!ok) { xrtValueRelease(value); *invalid = true; return NULL; }
    return value;
}
static void Search_WipeCredentials(xvalue* value)
{
    const char* names[] = {"bocha","zai"}; size_t i;
    for (i = 0; i < 2; i++) {
        const char* text = ValueText(value,names[i]);
        if (text) xrtSecureZero((void*)text,strlen(text));
    }
    xrtValueRelease(value);
}
static bool Search_Key(const char* provider, char out[1001])
{
    memset(out,0,1001);
    const char* env = getenv(!strcmp(provider,"bocha") ? "BOCHA_API_KEY" : "ZAI_API_KEY");
    if (env && *env) {
        if (!Search_ValidKey(env)) return false;
        strcpy(out,env); return true;
    }
    bool invalid; xvalue* credentials = Search_ReadCredentials(&invalid);
    const char* key = Search_Text(credentials,provider,1000);
    bool ok = !invalid && key && *key;
    if (ok) strcpy(out,key);
    Search_WipeCredentials(credentials); return ok;
}
