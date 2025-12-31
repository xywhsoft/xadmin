


// 框架主页
void Request_Index(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode == HTTP_GET ) {
		
		// 后台主页
		LoadPage(c, 200, HTTP_CT_HTML, "index.html");
		
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
		LoadPage(c, 200, HTTP_CT_HTML, "home.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 动态菜单接口
void Request_Menu(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode == HTTP_GET ) {
		
		// 构建菜单数组
		xvalue arrMenu = xvoCreateArray();
		
		// 1. 主页
		xvalue itemHome = xvoCreateTable();
		xvoTableSetText(itemHome, "id", 2, "home", 0, FALSE);
		xvoTableSetText(itemHome, "title", 5, "主页", 0, FALSE);
		xvoTableSetText(itemHome, "icon", 4, "layui-icon layui-icon-home", 0, FALSE);
		xvoTableSetInt(itemHome, "type", 4, 1);
		xvoTableSetText(itemHome, "openType", 8, "_iframe", 0, FALSE);
		xvoTableSetText(itemHome, "href", 4, "view/home", 0, FALSE);
		xvoArrayAppendValue(arrMenu, itemHome, TRUE);
		
		// 4. 系统日志
		xvalue itemLogs = xvoCreateTable();
		xvoTableSetText(itemLogs, "id", 2, "logs", 0, FALSE);
		xvoTableSetText(itemLogs, "title", 5, "系统日志", 0, FALSE);
		xvoTableSetText(itemLogs, "icon", 4, "layui-icon layui-icon-log", 0, FALSE);
		xvoTableSetInt(itemLogs, "type", 4, 1);
		xvoTableSetText(itemLogs, "openType", 8, "_component", 0, FALSE);
		xvoTableSetText(itemLogs, "href", 4, "view/logs", 0, FALSE);
		xvoArrayAppendValue(arrMenu, itemLogs, TRUE);
		
		// 5. 权限管理分类
		xvalue itemAuth = xvoCreateTable();
		xvoTableSetText(itemAuth, "id", 2, "class_Auth", 0, FALSE);
		xvoTableSetText(itemAuth, "title", 5, "权限管理", 0, FALSE);
		xvoTableSetText(itemAuth, "icon", 4, "layui-icon layui-icon-auz", 0, FALSE);
		xvoTableSetInt(itemAuth, "type", 4, 0);
		
		xvalue arrAuthChildren = xvoCreateArray();
		
		// 5.1 用户管理
		xvalue itemUser = xvoCreateTable();
		xvoTableSetText(itemUser, "id", 2, "user", 0, FALSE);
		xvoTableSetText(itemUser, "title", 5, "用户管理", 0, FALSE);
		xvoTableSetText(itemUser, "icon", 4, "layui-icon layui-icon-username", 0, FALSE);
		xvoTableSetInt(itemUser, "type", 4, 1);
		xvoTableSetText(itemUser, "openType", 8, "_component", 0, FALSE);
		xvoTableSetText(itemUser, "href", 4, "view/auth/user", 0, FALSE);
		xvoArrayAppendValue(arrAuthChildren, itemUser, TRUE);
		
		// 5.2 角色管理
		xvalue itemRole = xvoCreateTable();
		xvoTableSetText(itemRole, "id", 2, "role", 0, FALSE);
		xvoTableSetText(itemRole, "title", 5, "角色管理", 0, FALSE);
		xvoTableSetText(itemRole, "icon", 4, "layui-icon layui-icon-user", 0, FALSE);
		xvoTableSetInt(itemRole, "type", 4, 1);
		xvoTableSetText(itemRole, "openType", 8, "_component", 0, FALSE);
		xvoTableSetText(itemRole, "href", 4, "view/auth/role", 0, FALSE);
		xvoArrayAppendValue(arrAuthChildren, itemRole, TRUE);
		
		// 5.3 权限分类
		xvalue itemAuthGroup = xvoCreateTable();
		xvoTableSetText(itemAuthGroup, "id", 2, "authGroup", 0, FALSE);
		xvoTableSetText(itemAuthGroup, "title", 5, "权限分类", 0, FALSE);
		xvoTableSetText(itemAuthGroup, "icon", 4, "layui-icon layui-icon-tabs", 0, FALSE);
		xvoTableSetInt(itemAuthGroup, "type", 4, 1);
		xvoTableSetText(itemAuthGroup, "openType", 8, "_component", 0, FALSE);
		xvoTableSetText(itemAuthGroup, "href", 4, "view/auth/group", 0, FALSE);
		xvoArrayAppendValue(arrAuthChildren, itemAuthGroup, TRUE);
		
		// 5.4 权限分组
		xvalue itemAuthAuth = xvoCreateTable();
		xvoTableSetText(itemAuthAuth, "id", 2, "auth", 0, FALSE);
		xvoTableSetText(itemAuthAuth, "title", 5, "权限分组", 0, FALSE);
		xvoTableSetText(itemAuthAuth, "icon", 4, "layui-icon layui-icon-template", 0, FALSE);
		xvoTableSetInt(itemAuthAuth, "type", 4, 1);
		xvoTableSetText(itemAuthAuth, "openType", 8, "_component", 0, FALSE);
		xvoTableSetText(itemAuthAuth, "href", 4, "view/auth/auth", 0, FALSE);
		xvoArrayAppendValue(arrAuthChildren, itemAuthAuth, TRUE);
		
		// 5.5 接口管理
		xvalue itemUris = xvoCreateTable();
		xvoTableSetText(itemUris, "id", 2, "uris", 0, FALSE);
		xvoTableSetText(itemUris, "title", 5, "接口管理", 0, FALSE);
		xvoTableSetText(itemUris, "icon", 4, "layui-icon layui-icon-website", 0, FALSE);
		xvoTableSetInt(itemUris, "type", 4, 1);
		xvoTableSetText(itemUris, "openType", 8, "_component", 0, FALSE);
		xvoTableSetText(itemUris, "href", 4, "view/auth/uris", 0, FALSE);
		xvoArrayAppendValue(arrAuthChildren, itemUris, TRUE);
		
		xvoTableSetValue(itemAuth, "children", 8, arrAuthChildren, TRUE);
		xvoArrayAppendValue(arrMenu, itemAuth, TRUE);
		
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


