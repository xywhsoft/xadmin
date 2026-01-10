


// 后台请求鉴权处理
void AdminRequestAuth(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm, RouteInfo* pInfo)
{
	if ( hm->session->Type == XVO_DT_TABLE ) {
		// 已登录先验证鉴权
		int64 iRoleID = xvoTableGetInt(hm->session, "roleID", 6);
		xvalue tblRole = xvoListGetValue(G_CACHE_RoleAuth, iRoleID);
		if ( tblRole && (tblRole->Type == XVO_DT_TABLE) ) {
			bool bOK = xvoTableGetBool(tblRole, hm->uri.buf, hm->uri.len);
			if ( bOK ) {
				// 活跃路由自动延长 Session 有效期
				if ( pInfo->bActive ) {
					Session_ExtendAdmin(hm->session);
				}
				pInfo->Proc(objServer, objHost, c, hm);
			} else {
				LoadPage(c, 403, HTTP_CT_HTML, "status/403.html");
			}
		} else {
			LoadPage(c, 403, HTTP_CT_HTML, "status/403.html");
		}
	} else {
		// 未登录，跳转到登录页面（不鉴权的页面可正常访问）
		http_reply(c, 302, "Location: /admin/login\r\n", NULL, 0);
	}
}



// 前台请求鉴权处理
void MemberRequestAuth(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm, RouteInfo* pInfo)
{
	if ( hm->session->Type == XVO_DT_TABLE ) {
		// 已登录先验证鉴权
		int64 iGroupID = xvoTableGetInt(hm->session, "groupId", 7);
		xvalue tblGroup = xvoListGetValue(G_CACHE_MemberGroupAuth, iGroupID);
		if ( tblGroup && (tblGroup->Type == XVO_DT_TABLE) ) {
			bool bOK = xvoTableGetBool(tblGroup, hm->uri.buf, hm->uri.len);
			if ( bOK ) {
				// 活跃路由自动延长 Session 有效期
				if ( pInfo->bActive ) {
					Session_ExtendMember(hm->session);
				}
				pInfo->Proc(objServer, objHost, c, hm);
			} else {
				// 前台接口返回 JSON 格式的权限不足
				http_reply(c, 403, HTTP_CT_JSON, "{\"code\":403,\"msg\":\"权限不足\"}", 0);
			}
		} else {
			http_reply(c, 403, HTTP_CT_JSON, "{\"code\":403,\"msg\":\"权限不足\"}", 0);
		}
	} else {
		// 未登录，返回 JSON 格式的未登录响应（不鉴权的接口可正常访问）
		http_reply(c, 401, HTTP_CT_JSON, "{\"code\":401,\"msg\":\"未登录\"}", 0);
	}
}



// HTTP 请求处理
void RequestProc(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	RouteInfo* pInfo = xrtDictGet(G_StaticRouteTableHTTP, hm->uri.buf, hm->uri.len);
	if ( pInfo ) {
		
		// 没有安装过，则跳转到安装页面
		if ( G_Install == FALSE ) {
			Request_Install(objServer, objHost, c, hm);
			return;
		}
		
		// 解析 cookies、获取 Session
		xdict tblCookies = ParseCookies(hm);
		str sSessionID;
		if ( pInfo->bAdmin ) {
			sSessionID = xrtDictGetPtr(tblCookies, "XSID", 4);
			hm->session = xvoTableGetValue(G_AdminSession, sSessionID, 0);
		} else {
			sSessionID = xrtDictGetPtr(tblCookies, "MSID", 4);
			hm->session = xvoTableGetValue(G_MemberSession, sSessionID, 0);
		}
		
		// 检查 Session 是否过期
		if ( hm->session->Type == XVO_DT_TABLE && Session_IsExpired(hm->session) ) {
			// Session 已过期，删除并视为未登录
			if ( pInfo->bAdmin ) {
				xvoTableRemove(G_AdminSession, sSessionID, 0);
			} else {
				xvoTableRemove(G_MemberSession, sSessionID, 0);
			}
			hm->session = xvoCreateNull();
		}
		
		if ( pInfo->bAuth ) {
			if ( pInfo->bAdmin ) {
				
				// 记录访问日志
				if ( pInfo->bPutLog ) {
					Logs_Add(c, hm);
				}
				
				// 后台请求处理
				AdminRequestAuth(objServer, objHost, c, hm, pInfo);
				
			} else {
				
				// 前台请求处理
				MemberRequestAuth(objServer, objHost, c, hm, pInfo);
				
			}
		} else {
			
			// 不需要鉴权的请求处理
			pInfo->Proc(objServer, objHost, c, hm);
			
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



