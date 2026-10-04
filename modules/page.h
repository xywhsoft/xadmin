/* 受控页面不进 wwwroot；通过 xroot 拒绝越界路径和链接跳转。 */
static xroot G_PageRoot;
static void LoadPage(XS_ResponseObject response, int code, const char* head, const char* page)
{
	size_t size = 0; bytes data = RootFileReadAll(G_PageRoot, page, &size);
	if (!data) { xsHttpReplyAuto(response, 404, HTTP_CT_HTML, "<h1>404</h1>", 0); return; }
	/* 组件页禁缓存：页面热更新后浏览器 iframe 常驻旧副本（普通 F5 不刷新
	 * 子 frame 缓存），附 no-cache 头让浏览器每次回源校验 */
	char* headers=xrtFormat("%sCache-Control: no-cache\r\n",head?head:HTTP_CT_HTML);
	if(headers)xsHttpReplyAuto(response,code,headers,data,size);
	else xsHttpReplyAuto(response,500,HTTP_CT_TEXT,"page response unavailable",0);
	xrtFree(headers);
	xrtFree(data);
}
