/* Android's standard public association document. Exact route and fixed file
 * keep xs's general dotfile denial intact; no arbitrary hidden path is served.
 * This is generic website infrastructure, independent of any app/package. */
static void XA_AssetLinksHandler(XS_ServerObject server, XS_HostObject host,
    XAdminRequest* req, XAdminRequest* resp, xvalue* session)
{
    (void)server; (void)host; (void)resp; (void)session;
    int method = xsReqMethodID(req);
    if (method != XHTTP_METHOD_GET && method != XHTTP_METHOD_HEAD) {
        xsHttpReplyAuto(req, 405, "Allow: GET, HEAD\r\nCache-Control: no-store\r\n", "", 0); return;
    }
    /* Bounded reading precedes parsing; administrator-owned files are still
     * checked before becoming a public JSON response. */
    char* path = xrtPathJoin(AppPath, "wwwroot");
    xroot root = path ? xrtRootOpen(path) : NULL; xrtFree(path);
    xfileoptions options; xrtFileOptionsInit(&options);
    options.Flags = XFILE_READ; options.Share = XFILE_SHARE_READ;
    xfile file = root ? xrtRootFileOpen(root, ".well-known/assetlinks.json", &options) : NULL;
    xfileinfo info; size_t size = 0; bytes body = NULL; xvalue* value = NULL;
    if (file && xrtFileStat(file, &info) && info.Type == XFILE_TYPE_FILE && info.Size && info.Size <= 65536) {
        size = (size_t)info.Size; body = xrtMalloc(size);
        if (!body || !xrtReadFull(file, body, size, NULL)) { xrtFree(body); body = NULL; }
    }
    if (file) xrtClose(file);
    if (root) xrtRootClose(root);
    if (body && xrtUtf8Valid((xstrview){(cstr)body,size}, NULL)) {
        xjsonreadconfig limits; xrtJsonReadConfigInit(&limits);
        limits.MaxInputBytes = 65536; limits.MaxDepth = 6; limits.MaxValues = 512;
        value = xrtJsonRead((xstrview){(cstr)body,size}, &limits);
    }
    if (xrtValueType(value) == XVALUE_ARRAY) xsHttpReplyAuto(req, 200,
        "Content-Type: application/json; charset=utf-8\r\nCache-Control: public, max-age=300\r\nX-Content-Type-Options: nosniff\r\n",
        body, size);
    else xsHttpReplyAuto(req, 404, "Cache-Control: no-store\r\n", "", 0);
    xrtValueRelease(value); xrtFree(body);
}
static void XA_AssetLinksRegister(void)
{
    RouteInfo* route = AddStaticRouteHTTP("/.well-known/assetlinks.json", XHTTP_METHOD_ANY, XA_AssetLinksHandler, false);
    if (route) { route->bAdmin = false; route->bAuth = false; }
}
