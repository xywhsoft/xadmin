/* 受控页面不进 wwwroot；通过 xroot 拒绝越界路径和链接跳转。 */
static xroot G_PageRoot;
static void LoadPage(XS_ResponseObject response, int code, const char* head, const char* page)
{
	size_t size = 0; bytes data = RootFileReadAll(G_PageRoot, page, &size);
	if (!data) { xsHttpReplyAuto(response, 404, HTTP_CT_HTML, "<h1>404</h1>", 0); return; }
	xsHttpReplyAuto(response, code, head, data, size);
	xrtFree(data);
}
