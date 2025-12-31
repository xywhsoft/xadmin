


// 加载独立页面 ( 独立页面就是无法直接通过 URL 访问的页面，只能通过代码返回，和模板类似，但是没有渲染过程 )
void LoadPage(struct mg_connection* c, int iCode, str sHead, str sPage)
{
	str sFile = xrtPathJoin(2, PagePath, sPage);
	size_t iRetSize = 0;
	str sHTML = xrtFileGetAll(sFile, &iRetSize);
	http_reply(c, iCode, sHead, sHTML, iRetSize);
	xrtFree(sHTML);
	xrtFree(sFile);
}


