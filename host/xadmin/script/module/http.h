


// HTTP 请求处理
void RequestProc(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	RouteInfo* pInfo = xrtDictGet(G_StaticRouteTableHTTP, hm->uri.buf, hm->uri.len);
	if ( pInfo ) {
		// C 语言静态路由
		
		// 没有安装过，则跳转到安装页面
		if ( G_Install == FALSE ) {
			Request_Install(objServer, objHost, c, hm);
			return;
		}
		
		// 解析 cookies 获取 Session ID 和 Session 对象
		xdict tblCookies = ParseCookies(hm);
		str sSessionID = xrtDictGetPtr(tblCookies, "XSID", 4);
		hm->session = xvoTableGetValue(G_Session, sSessionID, 0);
		
		// 记录访问日志
		if ( pInfo->bPutLog ) {
			Logs_Add(c, hm);
		}
		
		if ( hm->session->Type == XVO_DT_TABLE ) {
			
			// 已登录先验证鉴权，验证通过则调用对应的接口处理函数，否则加载权限不足页面
			int64 iRoleID = xvoTableGetInt(hm->session, "roleID", 6);
			xvalue tblRole = xvoListGetValue(G_CACHE_RoleAuth, iRoleID);
			if ( tblRole && (tblRole->Type == XVO_DT_TABLE) ) {
				bool bOK = xvoTableGetBool(tblRole, hm->uri.buf, hm->uri.len);
				if ( bOK ) {
					pInfo->Proc(objServer, objHost, c, hm);
				} else {
					LoadPage(c, 403, HTTP_CT_HTML, "status/403.html");
				}
			} else {
				LoadPage(c, 403, HTTP_CT_HTML, "status/403.html");
			}
			
		} else {
			
			// 如果没有登录，跳转到登录页面 ( 不进行鉴权的页面可以正常访问 )
			if ( pInfo->bAuth ) {
				http_reply(c, 302, "Location: /login\r\n", NULL, 0);
			} else {
				pInfo->Proc(objServer, objHost, c, hm);
			}
			
		}
		
		// 释放资源
		FreeCookies(tblCookies);
		
	} else {
		// 访问服务器静态资源
		struct mg_http_serve_opts opts = {0};
		opts.root_dir = objHost->Path;
		mg_http_serve_dir(c, hm, &opts);
	}
}


