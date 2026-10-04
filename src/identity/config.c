#include <xjwt.h>
#include <xoauth2.h>

typedef struct XAProviderConfig {
    bool enabled;
    char id[257], secret[513], callback[513], mode[32];
} XAProviderConfig;
typedef struct XAIdentityConfig {
    char issuer[257], audience[129], origin[513], country[5];
    bool registration, oauth_create, secure_cookie;
    char sms_url[1025], sms_token[513];
    XAProviderConfig github, wechat;
    char cors[8][513];
    size_t cors_count;
} XAIdentityConfig;
static XAIdentityConfig G_Identity;

static bool XA_ConfigString(const xvalue* v, const char* name, char* out, size_t size)
{
    if (!ValueHas(v, name)) return true;
    const char* text = XA_Text(v, name, size - 1);
    if (!text) return false;
    strcpy(out, text); return true;
}
static bool XA_ConfigBool(const xvalue* v, const char* name, bool* out)
{
    return !ValueHas(v, name) || xrtValueGetBool(ValueGet(v, name), out);
}
static bool XA_ConfigFields(const xvalue* v, const char* const* names, size_t count)
{
    xvalueiter it = {0}; xvaluekey key; xvalue* value; bool ok = true;
    if (xrtValueIterBegin(v, &it)) {
        while ((value = xrtValueIterNext(&it, &key))) {
            size_t i;
            for (i = 0; i < count; i++)
                if (key.Type == XVALUE_KEY_STRING && xrtStrEqual(key.String, xrtStrView(names[i]))) break;
            if (i == count) ok = false;
        }
        xrtValueIterEnd(&it);
    }
    return ok;
}
static bool XA_ConfigProvider(xvalue* v, XAProviderConfig* p)
{
    const char* const names[] = {"enabled", "client_id", "client_secret", "callback", "mode"};
    strcpy(p->mode, "website");
    if (!v) return true;
    return xrtValueType(v) == XVALUE_OBJECT && XA_ConfigFields(v, names, 5) &&
        XA_ConfigBool(v, "enabled", &p->enabled) &&
        XA_ConfigString(v, "client_id", p->id, sizeof(p->id)) &&
        XA_ConfigString(v, "client_secret", p->secret, sizeof(p->secret)) &&
        XA_ConfigString(v, "callback", p->callback, sizeof(p->callback)) &&
        XA_ConfigString(v, "mode", p->mode, sizeof(p->mode));
}
/* Trusted scheme and authority, never a request-controlled forwarded header.
 * HTTP origins are accepted only for local development. */
static bool XA_ConfigOrigin(const char* origin)
{
    if (!origin || !*origin) return false;
    size_t prefix = !strncmp(origin, "https://", 8) ? 8 : 7;
    if (prefix == 7 && strncmp(origin, "http://127.0.0.1:", 17) &&
        strncmp(origin, "http://localhost:", 17)) return false;
    return strlen(origin) > prefix && !strpbrk(origin + prefix, "/@?#\\ \r\n\t");
}
/* Pure decoding: rejected edits never modify a live script generation. */
static bool XA_ConfigDecode(xvalue* v, XAIdentityConfig* out)
{
    const char* const names[] = {"issuer", "audience", "public_origin", "default_country_code",
        "registration", "oauth_create_member", "sms_webhook_url", "sms_webhook_token",
        "github", "wechat", "cors_origins"};
    memset(out, 0, sizeof(*out));
    strcpy(out->issuer, "xadmin:member"); strcpy(out->audience, "member");
    out->registration = out->oauth_create = true;
    if (!v) { strcpy(out->github.mode, "website"); strcpy(out->wechat.mode, "website"); return true; }
    if (xrtValueType(v) != XVALUE_OBJECT || !XA_ConfigFields(v, names, 11) ||
        !XA_ConfigString(v, "issuer", out->issuer, sizeof(out->issuer)) ||
        !XA_ConfigString(v, "audience", out->audience, sizeof(out->audience)) ||
        !XA_ConfigString(v, "public_origin", out->origin, sizeof(out->origin)) ||
        !XA_ConfigString(v, "default_country_code", out->country, sizeof(out->country)) ||
        !XA_ConfigString(v, "sms_webhook_url", out->sms_url, sizeof(out->sms_url)) ||
        !XA_ConfigString(v, "sms_webhook_token", out->sms_token, sizeof(out->sms_token)) ||
        !XA_ConfigBool(v, "registration", &out->registration) ||
        !XA_ConfigBool(v, "oauth_create_member", &out->oauth_create) ||
        !XA_ConfigProvider(ValueGet(v, "github"), &out->github) ||
        !XA_ConfigProvider(ValueGet(v, "wechat"), &out->wechat)) return false;
    if (!out->issuer[0] || !out->audience[0] || (out->origin[0] && !XA_ConfigOrigin(out->origin))) return false;
    out->secure_cookie = !strncmp(out->origin, "https://", 8);
    if (out->country[0]) {
        size_t n = strlen(out->country), i;
        if (n < 2 || n > 4 || out->country[0] != '+' || out->country[1] < '1' || out->country[1] > '9') return false;
        for (i = 1; i < n; i++) if (out->country[i] < '0' || out->country[i] > '9') return false;
    }
    if (out->sms_url[0] && (strncmp(out->sms_url, "https://", 8) || strpbrk(out->sms_url, "\r\n"))) return false;
    if (strpbrk(out->sms_token, "\r\n")) return false;
    XAProviderConfig* providers[] = {&out->github, &out->wechat};
    const char* ids[] = {"github", "wechat"}; size_t i;
    for (i = 0; i < 2; i++) if (providers[i]->enabled) {
        char expected[600];
        snprintf(expected, sizeof(expected), "%s/api/v1/auth/oauth/%s/callback", out->origin, ids[i]);
        if (!out->origin[0] || !providers[i]->id[0] || !providers[i]->secret[0] || strcmp(expected, providers[i]->callback)) return false;
    }
    if (strcmp(out->wechat.mode, "website") && strcmp(out->wechat.mode, "web")) return false;
    xvalue* cors = ValueGet(v, "cors_origins");
    if (cors) {
        if (xrtValueType(cors) != XVALUE_ARRAY || ValueCount(cors) > 8) return false;
        for (i = 0; i < ValueCount(cors); i++) {
            xstrview origin;
            if (!xrtValueGetString(xrtValueArrayGet(cors, i), &origin) || origin.Size > 512 ||
                memchr(origin.Data, 0, origin.Size)) return false;
            memcpy(out->cors[i], origin.Data, origin.Size); out->cors[i][origin.Size] = 0;
            if (!XA_ConfigOrigin(out->cors[i])) return false;
        }
        out->cors_count = ValueCount(cors);
    }
    return true;
}
static bool XA_ConfigLoad(XAIdentityConfig* out)
{
    char* path = xrtPathJoin(DBPath, "identity.json"); xvalue* v = NULL; bool ok;
    if (!path) return false;
    if (xrtFileExists(path)) {
        size_t n = 0; char* bytes = xrtFileReadAll(path, &n);
        xjsonreadconfig limits; xrtJsonReadConfigInit(&limits);
        limits.MaxInputBytes = 16384; limits.MaxDepth = 4; limits.MaxValues = 128;
        if (bytes && n <= 16384) v = xrtJsonRead(xrtStrViewN(bytes, n), &limits);
        if (bytes) xrtSecureZero(bytes, n); xrtFree(bytes);
        if (xrtValueType(v) != XVALUE_OBJECT) { xrtFree(path); xrtValueRelease(v); return false; }
    }
    ok = XA_ConfigDecode(v, out); xrtValueRelease(v); xrtFree(path); return ok;
}
static bool XA_ConfigInit(void) { return XA_ConfigLoad(&G_Identity); }
static xvalue* XA_ConfigValue(const XAIdentityConfig* c, bool redacted)
{
    xvalue* v = ValueObject(); size_t i; const char* ids[] = {"github", "wechat"};
    if (!v || !ValueSetText(v, "issuer", c->issuer) || !ValueSetText(v, "audience", c->audience) ||
        !ValueSetText(v, "public_origin", c->origin) || !ValueSetText(v, "default_country_code", c->country) ||
        !ValueSetBool(v, "registration", c->registration) || !ValueSetBool(v, "oauth_create_member", c->oauth_create) ||
        !ValueSetText(v, "sms_webhook_url", c->sms_url)) goto failed;
    if (redacted) { if (!ValueSetBool(v, "sms_token_configured", c->sms_token[0] != 0)) goto failed; }
    else if (!ValueSetText(v, "sms_webhook_token", c->sms_token)) goto failed;
    const XAProviderConfig* providers[] = {&c->github, &c->wechat};
    for (i = 0; i < 2; i++) {
        xvalue* p = ValueObject();
        bool ok = p && ValueSetBool(p, "enabled", providers[i]->enabled) &&
            ValueSetText(p, "client_id", providers[i]->id) && ValueSetText(p, "callback", providers[i]->callback) &&
            ValueSetText(p, "mode", providers[i]->mode) && (redacted ?
                ValueSetBool(p, "secret_configured", providers[i]->secret[0] != 0) :
                ValueSetText(p, "client_secret", providers[i]->secret));
        if (!ok) { xrtValueRelease(p); goto failed; }
        if (!ValueSetOwn(v, ids[i], p)) goto failed;
    }
    xvalue* cors = ValueArray();
    if (!cors) goto failed;
    for (i = 0; i < c->cors_count; i++) {
        xvalue* origin = xrtValueString(xrtStrView(c->cors[i]));
        if (!origin || !ValueArrayOwn(cors, origin)) { xrtValueRelease(cors); goto failed; }
    }
    if (!ValueSetOwn(v, "cors_origins", cors)) goto failed;
    return v;
failed:
    xrtValueRelease(v); return NULL;
}
static bool XA_SameOrigin(XAdminRequest* req)
{
    bool bad = false; const xhttpfield* origin = XA_Header(req, "Origin", &bad);
    if (bad) return false;
    if (!origin) return true;
    if (G_Identity.origin[0]) return xrtStrEqual(origin->Value, xrtStrView(G_Identity.origin));
    const xhttpfield* host = XA_Header(req, "Host", &bad); char expected[600];
    if (bad || !host || host->Value.Size > 500) return false;
    snprintf(expected, sizeof(expected), "%s://%.*s", req->raw->tls ? "https" : "http", (int)host->Value.Size, host->Value.Data);
    return xrtStrEqual(origin->Value, xrtStrView(expected));
}
/* Exact allowlist for Bearer clients. Credentialed CORS is never enabled. */
static xstrview XA_CORSOrigin(XS_HttpReq* raw)
{
    xstrview result = {0}; size_t i, j; const xhttpfield* origin = NULL;
    if (!raw || !raw->head || raw->head->Target.Size < 8 || memcmp(raw->head->Target.Data, "/api/v1/", 8)) return result;
    for (i = 0; i < raw->head->FieldCount; i++) {
        const xhttpfield* f = &raw->head->Fields[i];
        if (xrtStrCaseEqual(f->Name, XRT_STR_LITERAL("Origin"))) { if (origin) return result; origin = f; }
    }
    if (origin) for (j = 0; j < G_Identity.cors_count; j++)
        if (xrtStrEqual(origin->Value, xrtStrView(G_Identity.cors[j]))) return origin->Value;
    return result;
}
static bool XA_OriginAllowed(XAdminRequest* req)
{
    return XA_SameOrigin(req) || XA_CORSOrigin(req->raw).Size != 0;
}
