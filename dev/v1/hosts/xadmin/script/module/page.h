

// load static page from data/page
void LoadPage(XS_ResponseObject objResp, int iCode, str sHead, str sPage)
{
	str sFile = xrtPathJoin(2, PagePath, sPage);
	size_t iRetSize = 0;
	str sHTML = xrtFileGetAll(sFile, &iRetSize);

	if ( sHTML == NULL ) {
		xsHttpReplyAuto(objResp, 404, HTTP_CT_HTML, "<h1>404</h1>", 0);
		xrtFree(sFile);
		return;
	}

	xsHttpReplyAuto(objResp, iCode, sHead, sHTML, iRetSize);
	xrtFree(sHTML);
	xrtFree(sFile);
}

// load public site page from data/site/page
void LoadSitePage(XS_ResponseObject objResp, int iCode, str sHead, str sPage)
{
	str sFile = xrtPathJoin(2, SitePagePath, sPage);
	size_t iRetSize = 0;
	str sHTML = xrtFileGetAll(sFile, &iRetSize);

	if ( sHTML == NULL ) {
		xsHttpReplyAuto(objResp, 404, HTTP_CT_HTML, "<h1>404</h1>", 0);
		xrtFree(sFile);
		return;
	}

	xsHttpReplyAuto(objResp, iCode, sHead, sHTML, iRetSize);
	xrtFree(sHTML);
	xrtFree(sFile);
}
