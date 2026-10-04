/* Generic browser-to-native login. The browser proves the member identity;
 * the public client proves possession of its PKCE verifier. No member token
 * or password appears in a redirect. All access runs under G_RequestLock. */
static const char XA_ApplicationSchema1[] =
    "CREATE TABLE identity_application(request_id TEXT PRIMARY KEY,client_id TEXT NOT NULL,redirect_uri TEXT NOT NULL,state TEXT NOT NULL,challenge TEXT NOT NULL,browser_hash TEXT NOT NULL,remote TEXT NOT NULL,created_at INTEGER NOT NULL,expires_at INTEGER NOT NULL,source_sid TEXT NOT NULL DEFAULT '',code_hash TEXT NOT NULL DEFAULT '',decision INTEGER NOT NULL DEFAULT 0 CHECK(decision IN(0,1,2)),consumed_at INTEGER NOT NULL DEFAULT 0);"
    "CREATE UNIQUE INDEX identity_application_code ON identity_application(code_hash) WHERE code_hash<>'';"
    "CREATE INDEX identity_application_expiry ON identity_application(expires_at);";

typedef struct XAApplicationRequest {
    char id[65], client[65], redirect[513], state[129], challenge[44];
    char browser[65], sid[65], code_hash[65];
    int64 expires, consumed;
    int decision;
} XAApplicationRequest;

static const XAApplicationConfig* XA_ApplicationFind(const char* id)
{
    size_t i;
    if (!id) return NULL;
    for (i = 0; i < G_Identity.application_count; i++)
        if (!strcmp(id, G_Identity.applications[i].id)) return &G_Identity.applications[i];
    return NULL;
}
static bool XA_ApplicationRedirect(const XAApplicationConfig* app, const char* uri)
{
    size_t i;
    if (!app || !uri || strlen(uri) > 512 || strpbrk(uri, "@?#\\ \r\n\t")) return false;
    for (i = 0; i < app->redirect_count; i++) {
        const char* pattern = app->redirects[i];
        const char* marker = strstr(pattern, "{port}");
        if (!marker) { if (!strcmp(pattern, uri)) return true; continue; }
        size_t prefix = (size_t)(marker - pattern);
        if (strncmp(pattern, uri, prefix)) continue;
        const char* port = uri + prefix;
        const char* end = port;
        unsigned value = 0;
        while (*end >= '0' && *end <= '9' && (size_t)(end - port) < 5) value = value * 10 + (unsigned)(*end++ - '0');
        if (end > port && port[0] != '0' && value && value <= 65535 && !strcmp(end, marker + 6)) return true;
    }
    return false;
}
static bool XA_ApplicationAscii(const char* value, size_t min, size_t max)
{
    size_t i, n = value ? strlen(value) : 0;
    if (n < min || n > max) return false;
    for (i = 0; i < n; i++)
        if (!((value[i] >= 'a' && value[i] <= 'z') || (value[i] >= 'A' && value[i] <= 'Z') ||
            (value[i] >= '0' && value[i] <= '9') || strchr("-._~", value[i]))) return false;
    return true;
}
static bool XA_ApplicationChallenge(const char* verifier, char out[44])
{
    unsigned char digest[32]; char encoded[45]; size_t n = 0, i;
    if (!XA_ApplicationAscii(verifier, 43, 128) || !xrtSha256(verifier, strlen(verifier), digest) ||
        !xrtBase64Encode(digest, sizeof(digest), encoded, sizeof(encoded), &n, NULL) || n != 44) return false;
    for (i = 0; i < 43; i++) out[i] = encoded[i] == '+' ? '-' : encoded[i] == '/' ? '_' : encoded[i];
    out[43] = 0;
    xrtSecureZero(digest, sizeof(digest));
    return true;
}
/* Decode both names and values, reject duplicates and truncation. The legacy
 * query helper returns the first occurrence and is unsuitable for auth. */
static xvalue* XA_ApplicationQuery(const char* query)
{
    xvalue* result = ValueObject();
    const char* cursor = query;
    if (!result || !query || strlen(query) > 4096) goto failed;
    while (*cursor) {
        const char* end = strchr(cursor, '&');
        const char* equal;
        size_t kn = 0, vn = 0;
        char* key = NULL; char* value = NULL;
        if (!end) end = cursor + strlen(cursor);
        equal = memchr(cursor, '=', (size_t)(end - cursor));
        if (!equal) goto failed;
        key = (char*)xrtPercentDecodeNew(xrtStrViewN(cursor, (size_t)(equal - cursor)), &kn);
        value = (char*)xrtPercentDecodeNew(xrtStrViewN(equal + 1, (size_t)(end - equal - 1)), &vn);
        bool ok = key && value && kn && kn <= 64 && vn <= 1024 && !memchr(key, 0, kn) &&
            !memchr(value, 0, vn) && !ValueHas(result, key) && ValueSetText(result, key, value);
        xrtFree(key); xrtFree(value);
        if (!ok) goto failed;
        cursor = *end ? end + 1 : end;
        if (*end && !*cursor) goto failed;
    }
    return result;
failed:
    xrtValueRelease(result); return NULL;
}
static bool XA_ApplicationRead(const char* id, const char* code_hash, XAApplicationRequest* out)
{
    sqlite3_stmt* s = XA_SQL(id ?
        "SELECT request_id,client_id,redirect_uri,state,challenge,browser_hash,source_sid,code_hash,expires_at,decision,consumed_at FROM identity_application WHERE request_id=?" :
        "SELECT request_id,client_id,redirect_uri,state,challenge,browser_hash,source_sid,code_hash,expires_at,decision,consumed_at FROM identity_application WHERE code_hash=?");
    memset(out, 0, sizeof(*out)); XA_BindText(s, 1, id ? id : code_hash);
    bool ok = s && sqlite3_step(s) == SQLITE_ROW &&
        XA_CopyColumn(s, 0, out->id, sizeof(out->id)) && XA_CopyColumn(s, 1, out->client, sizeof(out->client)) &&
        XA_CopyColumn(s, 2, out->redirect, sizeof(out->redirect)) && XA_CopyColumn(s, 3, out->state, sizeof(out->state)) &&
        XA_CopyColumn(s, 4, out->challenge, sizeof(out->challenge)) && XA_CopyColumn(s, 5, out->browser, sizeof(out->browser)) &&
        XA_CopyColumn(s, 6, out->sid, sizeof(out->sid)) && XA_CopyColumn(s, 7, out->code_hash, sizeof(out->code_hash));
    if (ok) { out->expires = sqlite3_column_int64(s, 8); out->decision = sqlite3_column_int(s, 9); out->consumed = sqlite3_column_int64(s, 10); }
    sqlite3_finalize(s); return ok;
}
static bool XA_ApplicationBrowser(XAdminRequest* req, const XAApplicationRequest* app)
{
    char cookie[65], hash[65];
    return XA_Cookie(req, "XAPP", cookie, sizeof(cookie)) == 64 && XA_IsHex(cookie, 64) &&
        XA_Hash(cookie, hash) && xrtConstTimeEqual(hash, app->browser, 64);
}
static char* XA_ApplicationReturn(const XAApplicationRequest* app, const char* code, const char* error)
{
    char* state = xrtPercentEncodeNew(app->state, strlen(app->state), xrtStrView(""), NULL);
    char* result = state ? xrtFormat("%s?%s=%s&state=%s", app->redirect,
        code ? "code" : "error", code ? code : error, state) : NULL;
    xrtFree(state); return result;
}
