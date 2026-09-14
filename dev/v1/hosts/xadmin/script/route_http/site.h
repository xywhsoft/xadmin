static void Request_Site_Page(XS_RequestObject objReq, XS_ResponseObject objResp, const char* sPage)
{
	if ( xsReqMethodID(objReq) == XHTTPD_METHOD_GET ) {
		LoadSitePage(objResp, 200, HTTP_CT_HTML, (str)sPage);
	} else {
		LoadSitePage(objResp, 404, HTTP_CT_HTML, "404.html");
	}
}

void Request_Site_Home(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	Request_Site_Page(objReq, objResp, "index.html");
}

void Request_Site_Features(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	Request_Site_Page(objReq, objResp, "features.html");
}

void Request_Site_Plugins(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	Request_Site_Page(objReq, objResp, "plugins.html");
}

void Request_Site_Capabilities(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	Request_Site_Page(objReq, objResp, "capabilities.html");
}

void Request_Site_ContentSystem(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	Request_Site_Page(objReq, objResp, "content-system.html");
}

void Request_Site_Docs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	Request_Site_Page(objReq, objResp, "docs.html");
}

void Request_Site_Download(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	Request_Site_Page(objReq, objResp, "download.html");
}

void Request_Site_Demo(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	Request_Site_Page(objReq, objResp, "demo.html");
}
