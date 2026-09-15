


// 登录
void Request_Login(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 登录页面
		if ( xrtValueType(objSession) == XVALUE_OBJECT ) {
			xsHttpReplyAuto(objResp, 302, "Location: /admin\r\n", NULL, 0);
		} else {
			LoadPage(objResp, 200, HTTP_CT_HTML, "admin/login.html");
		}
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		
		// 登录请求
		
		// step 1 : 暴力破解防火墙
		xtime tCD = Guard_Check(G_GuardAdmin, (str)xsReqRemote(objReq));
		if ( tCD ) {
			str sTime = TimeText(tCD, TIME_TEXT_DATETIME);
		xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"登录失败尝试次数过多，请在 %s 后再试！\"}", sTime);
			xrtFree(sTime);
			return;
		}
		
		// F7：guard 通过即留痕（洪泛被 guard 拒绝，不产生日志行）
		Logs_Add(objReq, NULL, true);
		// step 2 : 根据用户名查询用户信息（获取 salt 和 pwd）
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		str sUser = ValueText(tblForm, "username");
		if ( !sUser || (strlen(sUser) == 0) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名不能为空！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		str sClientHash = ValueText(tblForm, "password");
		if ( !sClientHash || (strlen(sClientHash) == 0) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"密码不能为空！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		
		// step 3 : 查询用户并验证密码
		bool bOK = false;
		str XID = NULL;
		xvalue* tblSession = NULL;
		sqlite3_bind_text(stmt_login_get, 1, sUser, -1, NULL);
		while ( sqlite3_step(stmt_login_get) == SQLITE_ROW ) {
			// 获取用户的 salt 和 pwd
			str sSalt = (str)sqlite3_column_text(stmt_login_get, 2);
			str sStoredPwd = (str)sqlite3_column_text(stmt_login_get, 3);
			
			// 服务端二次 SHA-256 哈希
			str sPwdHash = ServerHashPassword(sUser, sSalt, sClientHash);
			
			// 比对密码
			if ( sPwdHash && sStoredPwd && strcmp(sPwdHash, sStoredPwd) == 0 ) {
				
				// step 4 : 检查是否有对应的 role 权限配置
				int64 iRoleID = sqlite3_column_int64(stmt_login_get, 4);
				int64 iLvRole = -1;
				if ( Auth_DBRoleGetAccess(iRoleID, 0, &iLvRole) ) {
					bOK = true;
					
					// step 5 : 创建用户 Session 表（使用新的创建函数，自动设置过期时间）
					XID = Util_Token();
					tblSession = Session_CreateAdmin(XID);
					if (!XID || !tblSession) {
						xrtFree(XID); xrtValueRelease(tblSession); xrtFree(sPwdHash); xrtValueRelease(tblForm);
						sqlite3_reset(stmt_login_get);
						xsHttpReplyAuto(objResp, 500, HTTP_CT_JSON, "{\"result\":false,\"message\":\"session unavailable\"}", 0);
						return;
					}
					
					// 获取 authLevel
					int64 iLvUser = sqlite3_column_int64(stmt_login_get, 5);
					int64 iAuthLevel = iLvUser > iLvRole ? iLvUser : iLvRole;
					
					// step 6 : 将用户信息填入用户 Session 中
					ValueSetOwnedText(tblSession, "xid", XID);
					/* 转交字符串后重新借用值内地址，不继续使用旧分配的指针。 */
					XID = ValueText(tblSession, "xid");
					ValueSetInt(tblSession, "id", sqlite3_column_int64(stmt_login_get, 0));
					ValueSetInt(tblSession, "roleID", iRoleID);
					ValueSetInt(tblSession, "authLevel", iAuthLevel);
					ValueSetText(tblSession, "user", sUser);
					if (!Session_StoreAdmin(XID, tblSession)) {
						xrtValueRelease(tblSession); xrtFree(sPwdHash); xrtValueRelease(tblForm);
						sqlite3_reset(stmt_login_get);
						xsHttpReplyAuto(objResp, 500, HTTP_CT_JSON, "{\"result\":false,\"message\":\"session unavailable\"}", 0);
						return;
					}
					
				} else {
					xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户没有被分配到正确的角色！\"}", 0);
					xrtValueRelease(tblForm);
					xrtFree(sPwdHash);
					sqlite3_reset(stmt_login_get);
					return;
				}
			}
			xrtFree(sPwdHash);
		}
		sqlite3_reset(stmt_login_get);
		
		if ( bOK ) {
			
			// step 7 : 重置防护模块信息
			Guard_Reset(G_GuardAdmin, (str)xsReqRemote(objReq));
			
			// step 8 : 返回响应，附带 cookie 信息（remember 字段在勾选 [记住登录状态] 时传递为字符串 on，不勾选时不传递参数）
			str sHeader;
			if ( xrtValueType(ValueGet(tblForm, "remember")) == XVALUE_STRING ) {
				sHeader = Session_AdminHeaders(objReq, XID, 604800, NULL);
			} else {
				sHeader = Session_AdminHeaders(objReq, XID, -1, NULL);
			}
			xsHttpReplyAuto(objResp, 200, sHeader, "{\"result\": true, \"message\": \"登录成功，即将跳转到后台管理页面！\"}", 0);
			xrtFree(sHeader);
			
		} else {
			// 登录失败
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"登录失败，请检查用户名密码是否正确！\"}", 0);
			Guard_Failed(G_GuardAdmin, (str)xsReqRemote(objReq));
		}
		
		// step 9 : 释放表单
		if ( tblSession ) {
			xrtValueRelease(tblSession);
			tblSession = NULL;
		}
		xrtValueRelease(tblForm);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 注销登录
void Request_Logout(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	/* R1：仅接受 POST——SameSite=Lax 不拦顶级 GET 导航，GET 注销可被跨站触发。 */
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		
		// 删除 Session
		if ( xrtValueType(objSession) == XVALUE_OBJECT ) {
			str sID = ValueText(objSession, "xid");
			Session_RemoveAdminByID(sID);
		}
		
		// 清除 Cookie 并跳转到登录页
		str sHeader = Session_AdminHeaders(objReq, "", 0, Option_GetAdminLoginPath());
		xsHttpReplyAuto(objResp, 302, sHeader, "{\"result\": true, \"message\": \"注销成功！\"}", 0);
		xrtFree(sHeader);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}


