


// 框架主页
void Request_Index(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode == HTTP_GET ) {
		
		// 后台主页
		LoadPage(c, 200, HTTP_CT_HTML, "admin/index.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 主页视图
void Request_View_Home(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode == HTTP_GET ) {
		
		// 加载主页页面
		LoadPage(c, 200, HTTP_CT_HTML, "admin/home.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 动态菜单接口
void Request_Menu(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode == HTTP_GET ) {
		
		// 从数据库动态获取菜单树
		xvalue arrMenu = Menu_BuildTree();
		
		// 生成 JSON 并返回
		size_t iRetSize = 0;
		char* sRet = xrtStringifyJSON(arrMenu, FALSE, &iRetSize);
		http_reply(c, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xvoUnref(arrMenu);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}


