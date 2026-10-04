/* Included by config.c. Limits keep parsing and generation memory bounded. */
static bool XA_ApplicationId(const char* id)
{
    size_t i, n = id ? strlen(id) : 0;
    if (!n || n > 64) return false;
    for (i = 0; i < n; i++)
        if (!((id[i] >= 'a' && id[i] <= 'z') ||
              (id[i] >= '0' && id[i] <= '9') || id[i] == '-' || id[i] == '_')) return false;
    return true;
}
static bool XA_ApplicationRedirectSyntax(const char* uri)
{
    const char* authority;
    const char* path;
    if (!uri || strlen(uri) > 512 || strpbrk(uri, "@?#\\ \r\n\t") ||
        !strncmp(uri, "https:///", 9)) return false;
    if (!strncmp(uri, "http://127.0.0.1:{port}/", strlen("http://127.0.0.1:{port}/")) ||
        !strncmp(uri, "http://[::1]:{port}/", strlen("http://[::1]:{port}/"))) return true;
    if (strncmp(uri, "https://", 8) || strchr(uri, '{') || strchr(uri, '}')) return false;
    authority = uri + 8;
    path = strchr(authority, '/');
    return path && path > authority && path[1] != '\0';
}
static bool XA_ApplicationsDecode(const xvalue* list, XAIdentityConfig* out)
{
    const char* const fields[] = {"client_id", "name", "redirect_uris"};
    size_t i, j, k;
    if (!list) return true;
    if (xrtValueType(list) != XVALUE_ARRAY || ValueCount(list) > 8) return false;
    for (i = 0; i < ValueCount(list); i++) {
        xvalue* item = xrtValueArrayGet(list, i);
        xvalue* redirects = ValueGet(item, "redirect_uris");
        XAApplicationConfig* app = &out->applications[i];
        if (xrtValueType(item) != XVALUE_OBJECT || !XA_ConfigFields(item, fields, 3) ||
            !XA_ConfigString(item, "client_id", app->id, sizeof(app->id)) ||
            !XA_ConfigString(item, "name", app->name, sizeof(app->name)) ||
            !XA_ApplicationId(app->id) || !app->name[0] ||
            strpbrk(app->name, "\r\n\t") || xrtValueType(redirects) != XVALUE_ARRAY ||
            !ValueCount(redirects) || ValueCount(redirects) > 4) return false;
        for (k = 0; k < i; k++) if (!strcmp(app->id, out->applications[k].id)) return false;
        for (j = 0; j < ValueCount(redirects); j++) {
            xstrview text;
            if (!xrtValueGetString(xrtValueArrayGet(redirects, j), &text) ||
                text.Size >= sizeof(app->redirects[j]) || memchr(text.Data, 0, text.Size)) return false;
            memcpy(app->redirects[j], text.Data, text.Size);
            if (!XA_ApplicationRedirectSyntax(app->redirects[j])) return false;
            for (k = 0; k < j; k++) if (!strcmp(app->redirects[k], app->redirects[j])) return false;
        }
        app->redirect_count = ValueCount(redirects);
    }
    out->application_count = ValueCount(list);
    return true;
}
static xvalue* XA_ApplicationsValue(const XAIdentityConfig* config)
{
    size_t i, j;
    xvalue* list = ValueArray();
    if (!list) return NULL;
    for (i = 0; i < config->application_count; i++) {
        const XAApplicationConfig* app = &config->applications[i];
        xvalue* item = ValueObject();
        xvalue* redirects = ValueArray();
        bool ok = item && redirects && ValueSetText(item, "client_id", app->id) &&
            ValueSetText(item, "name", app->name);
        for (j = 0; ok && j < app->redirect_count; j++)
            ok = ValueArrayOwn(redirects, xrtValueString(xrtStrView(app->redirects[j])));
        if (!ok) { xrtValueRelease(item); xrtValueRelease(redirects); goto failed; }
        if (!ValueSetOwn(item, "redirect_uris", redirects)) { xrtValueRelease(item); goto failed; }
        if (!ValueArrayOwn(list, item)) goto failed;
    }
    return list;
failed:
    xrtValueRelease(list);
    return NULL;
}
