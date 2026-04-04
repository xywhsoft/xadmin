


// 登录
void Request_Login(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 登录页面
		if ( objSession->Type == XVO_DT_TABLE ) {
			http_reply(objResp, 302, "Location: /admin\r\n", NULL, 0);
		} else {
			LoadPage(objResp, 200, HTTP_CT_HTML, "admin/login.html");
		}
		
	} else if ( HttpMethodIs(objReq, "POST") ) {
		
		// 登录请求
		
		// step 1 : 暴力破解防火�?
		xtime tCD = Guard_Check((str)xsReqRemote(objReq));
		if ( tCD ) {
			str sTime = xrtTimeToStr(tCD, XRT_TIME_FORMAT_DATETIME);
			HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"登录失败尝试次数过多，请�?%s 后再试！\"}", sTime);
			xrtFree(sTime);
			return;
		}
		
		// step 2 : 根据用户名查询用户信息（获取 salt �?pwd�?
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		str sUser = xvoTableGetText(tblForm, "username", 8);
		if ( !sUser || (strlen(sUser) == 0) ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名不能为空！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		str sClientHash = xvoTableGetText(tblForm, "password", 8);
		if ( !sClientHash || (strlen(sClientHash) == 0) ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"密码不能为空！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		
		// step 3 : 查询用户并验证密�?
		bool bOK = FALSE;
		str XID = NULL;
		xvalue tblSession = NULL;
		sqlite3_bind_text(stmt_login_get, 1, sUser, -1, NULL);
		while ( sqlite3_step(stmt_login_get) == SQLITE_ROW ) {
			// 获取用户�?salt �?pwd
			str sSalt = (str)sqlite3_column_text(stmt_login_get, 2);
			str sStoredPwd = (str)sqlite3_column_text(stmt_login_get, 3);
			
			// 服务端二�?SHA-256 哈希
			str sPwdHash = ServerHashPassword(sUser, sSalt, sClientHash);
			
			// 比对密码
			if ( strcmp(sPwdHash, sStoredPwd) == 0 ) {
				
				// step 4 : 检查是否有对应�?role 权限�?
				int64 iRoleID = sqlite3_column_int64(stmt_login_get, 4);
				int64 iLvRole = -1;
				if ( Auth_DBRoleGetAccess(iRoleID, 0, &iLvRole) ) {
					bOK = TRUE;
					
					// step 5 : 创建用户 Session 表（使用新的创建函数，自动设置过期时间）
					XID = xrtMakeXIDS();
					tblSession = Session_CreateAdmin(XID);
					
					// 获取 authLevel
					int64 iLvUser = sqlite3_column_int64(stmt_login_get, 5);
					int64 iAuthLevel = iLvUser > iLvRole ? iLvUser : iLvRole;
					
					// step 6 : 将用户信息填入用�?Session �?
					xvoTableSetText(tblSession, "xid", 3, XID, 0, TRUE);
					xvoTableSetInt(tblSession, "id", 2, sqlite3_column_int64(stmt_login_get, 0));
					xvoTableSetInt(tblSession, "roleID", 6, iRoleID);
					xvoTableSetInt(tblSession, "authLevel", 9, iAuthLevel);
					xvoTableSetText(tblSession, "user", 4, sUser, 0, FALSE);
					Session_StoreAdmin(XID, tblSession);
					
				} else {
					http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户没有被分配到正确的角色！\"}", 0);
					xvoUnref(tblForm);
					xrtFree(sPwdHash);
					return;
				}
			}
			xrtFree(sPwdHash);
		}
		sqlite3_reset(stmt_login_get);
		
		if ( bOK ) {
			
			// step 7 : 重置防护模块信息
			Guard_Reset((str)xsReqRemote(objReq));
			
			// step 8 : 返回响应，附�?cookie 信息（remember 字段在勾�?[记住登录状态] 时传递为字符�?on，不勾选时不传递参数）
			str sHeader;
			if ( xvoTableItemType(tblForm, "remember", 8) == XVO_DT_TEXT ) {
				sHeader = xrtFormat("%sSet-Cookie: XSID=%s; Path=/; HttpOnly; Max-Age=604800\r\n", HTTP_CT_JSON, XID);
			} else {
				sHeader = xrtFormat("%sSet-Cookie: XSID=%s; Path=/; HttpOnly\r\n", HTTP_CT_JSON, XID);
			}
			http_reply(objResp, 200, sHeader, "{\"result\": true, \"message\": \"登录成功，即将跳转到后台管理页面！\"}", 0);
			xrtFree(sHeader);
			
		} else {
			// 登录失败
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"登录失败，请检查用户名密码是否正确！\"}", 0);
			Guard_Failed((str)xsReqRemote(objReq));
		}
		
		// step 9 : 释放表单
		xvoUnref(tblForm);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 注销登录
void Request_Logout(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( (HttpMethodIs(objReq, "GET")) || (HttpMethodIs(objReq, "POST")) ) {
		
		// 删除 Session
		if ( objSession->Type == XVO_DT_TABLE ) {
			str sID = xvoTableGetText(objSession, "xid", 3);
			Session_RemoveAdminByID(sID);
		}
		
		// 清除 Cookie 并跳转到登录�?
		str sHeader = xrtFormat("%sSet-Cookie: XSID=; Path=/; HttpOnly; Max-Age=0\r\nLocation: %s\r\n", HTTP_CT_JSON, Option_GetAdminLoginPath());
		http_reply(objResp, 302, sHeader, "{\"result\": true, \"message\": \"注销成功！\"}", 0);
		xrtFree(sHeader);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}


