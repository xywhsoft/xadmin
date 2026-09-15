


// 框架主页
void Request_Index(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 后台主页
		LoadPage(objResp, 200, HTTP_CT_HTML, "admin/index.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 主页视图
void Request_View_Home(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 加载主页页面
		LoadPage(objResp, 200, HTTP_CT_HTML, "admin/home.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 动态菜单接口（L8 契约标注：返回全量菜单树、不按角色过滤——v1 语义；
// 无权限者可见入口名，点入由路由层 403 拦截）
void Request_Menu(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 从数据库动态获取菜单树
		xvalue* arrMenu = Menu_BuildTree();
		
		// 生成 JSON 并返回
		size_t iRetSize = 0;
		char* sRet = xrtJsonStringify(arrMenu, false, &iRetSize);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xrtValueRelease(arrMenu);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}


