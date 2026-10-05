/* Administrator primary password authentication; no session exists until MFA passes. */
void Request_Login(XS_ServerObject server,XS_HostObject host,XS_RequestObject req,XS_ResponseObject resp,xvalue* session)
{
    (void)server;(void)host;(void)resp;
    if(xsReqMethodID(req)==XHTTP_METHOD_GET){
        if(xrtValueType(session)==XVALUE_OBJECT)xsHttpReplyAuto(req,302,"Location: /admin\r\n",NULL,0);
        else LoadPage(req,200,HTTP_CT_HTML,"admin/login.html");return;
    }
    if(xsReqMethodID(req)!=XHTTP_METHOD_POST){LoadPage(req,404,HTTP_CT_HTML,"status/404.html");return;}
    if(!XA_SameOrigin(req)){XA_Reply(req,403,"origin is not allowed",NULL,NULL);return;}
    if(Guard_Check(G_GuardAdmin,req->remote)){xsHttpReplyAuto(req,200,HTTP_CT_JSON,"{\"result\":false,\"message\":\"登录尝试过多，请稍后再试\"}",0);return;}
    Logs_Add(req,NULL,true);xvalue* body=XA_Body(req);
    if(!body){xsHttpReplyAuto(req,200,HTTP_CT_JSON,"{\"result\":false,\"message\":\"无效的请求数据\"}",0);return;}
    if(ValueHas(body,"challenge_id")){const char* action=XA_Text(body,"action",16);if(action&&!strcmp(action,"cancel"))XA_MFACancelLogin(req,"admin",body);else XA_MFALoginComplete(req,"admin",body);xrtValueRelease(body);return;}
    const char* username=XA_Text(body,"username",256);const char* password=XA_Text(body,"password",64);
    int64 owner=0,role=0,role_level=-1;bool valid=false;
    if(username&&*username&&XA_IsHex(password,64)){
        sqlite3_stmt* s=XA_SQL("SELECT id,salt,pwd,role FROM user WHERE user=? AND isDelete=0");XA_BindText(s,1,username);
        if(s&&sqlite3_step(s)==SQLITE_ROW){
            char* computed=ServerHashPassword(username,(const char*)sqlite3_column_text(s,1),password);const char* stored=(const char*)sqlite3_column_text(s,2);
            valid=computed&&stored&&strlen(computed)==strlen(stored)&&xrtConstTimeEqual(computed,stored,strlen(stored));
            if(valid){owner=sqlite3_column_int64(s,0);role=sqlite3_column_int64(s,3);}
            if(computed)xrtSecureZero(computed,strlen(computed));xrtFree(computed);
        }sqlite3_finalize(s);
    }
    valid=valid&&Auth_DBRoleGetAccess(role,0,&role_level);
    if(!valid){Guard_Failed(G_GuardAdmin,req->remote);xsHttpReplyAuto(req,200,HTTP_CT_JSON,"{\"result\":false,\"message\":\"登录失败，请检查用户名和密码\"}",0);xrtValueRelease(body);return;}
    XAMFAFactor factor;bool remember=xrtValueType(ValueGet(body,"remember"))==XVALUE_STRING;
    if(!XA_MFAFactorRead("admin",owner,&factor)){XA_Reply(req,500,"MFA unavailable",NULL,NULL);xrtValueRelease(body);return;}
    if(factor.enabled){
        char challenge[65];bool ok=XA_MFAChallengeCreate(req,"admin",owner,remember,challenge);
        if(ok)xsHttpReplyFormat(req,200,"Content-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\n","{\"result\":false,\"mfa_required\":true,\"challenge_id\":\"%s\",\"message\":\"请输入验证器验证码或恢复码\"}",challenge);
        else XA_Reply(req,factor.locked_until>XA_Now()?429:503,"MFA challenge unavailable; retry later",NULL,NULL);
    }else{
        xvalue* created=NULL;char* headers=NULL;bool ok=XA_MFAAdminSession(req,owner,factor.version,0,remember,&created,&headers);
        if(ok){Guard_Reset(G_GuardAdmin,req->remote);xsHttpReplyAuto(req,200,headers,"{\"result\":true,\"message\":\"登录成功\"}",0);}
        else XA_Reply(req,500,"session unavailable",NULL,NULL);
        xrtValueRelease(created);xrtFree(headers);
    }
    xrtValueRelease(body);
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


