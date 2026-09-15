



// 前台 API 路由处理 - /api/v1/*
// Cookie 名称: MSID
// Session 名称: G_MemberSession
// 权限缓存: G_CACHE_MemberGroupAuth



// ==================== 前台登录接口 ====================

// POST /api/v1/login - 前台用户登录
void API_Login(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( !(xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	
	// step 1 : 暴力破解防火墙
	xtime tCD = Guard_Check(G_GuardMember, (str)xsReqRemote(objReq));
	if ( tCD ) {
		str sTime = TimeText(tCD, TIME_TEXT_DATETIME);
		xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"code\":429,\"msg\":\"登录失败尝试次数过多，请在 %s 后再试\"}", sTime);
		xrtFree(sTime);
		return;
	}
	
	// step 2 : 解析请求数据
	xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( xrtValueType(tblForm) != XVALUE_OBJECT ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"无效的请求数据\"}", 0);
		xrtValueRelease(tblForm);
		return;
	}
	str sUsername = ValueText(tblForm, "username");
	if ( !sUsername || (strlen(sUsername) == 0) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"用户名不能为空\"}", 0);
		xrtValueRelease(tblForm);
		return;
	}
	str sClientHash = ValueText(tblForm, "password");
	if ( !sClientHash || (strlen(sClientHash) == 0) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"密码不能为空\"}", 0);
		xrtValueRelease(tblForm);
		return;
	}
	
	// step 3 : 查询用户并验证密码
	bool bOK = false;
	str MSID = NULL;
	xvalue* tblSession = NULL;
	sqlite3_bind_text(stmt_member_login, 1, sUsername, -1, NULL);
	while ( sqlite3_step(stmt_member_login) == SQLITE_ROW ) {
		// id, username, salt, pwd, groupId, authLevel, balance, nickname, status
		int64 iStatus = sqlite3_column_int64(stmt_member_login, 8);
		if ( iStatus != 1 ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":403,\"msg\":\"账号已被禁用\"}", 0);
			sqlite3_reset(stmt_member_login);
			xrtValueRelease(tblForm);
			return;
		}
		
		// 获取用户的 salt 和 pwd
		str sSalt = (str)sqlite3_column_text(stmt_member_login, 2);
		str sStoredPwd = (str)sqlite3_column_text(stmt_member_login, 3);
		
		// 服务端二次 SHA-256 哈希
		str sPwdHash = ServerHashPassword(sUsername, sSalt, sClientHash);
		
		// 比对密码
		if ( sPwdHash && sStoredPwd && strcmp(sPwdHash, sStoredPwd) == 0 ) {
			
			// step 4 : 检查用户组权限
			int64 iGroupId = sqlite3_column_int64(stmt_member_login, 4);
			int64 iLvGroup = -1;
			if ( MemberAuth_DBGroupGetAccess(iGroupId, 0, &iLvGroup) ) {
				bOK = true;
				
				// step 5 : 创建用户 Session 表（使用新的创建函数，自动设置过期时间）
				MSID = Util_Token();
				tblSession = Session_CreateMember(MSID);
				if (!MSID || !tblSession) {
					xrtFree(MSID); xrtValueRelease(tblSession); xrtFree(sPwdHash); xrtValueRelease(tblForm);
					sqlite3_reset(stmt_member_login);
					xsHttpReplyAuto(objResp, 500, HTTP_CT_JSON, "{\"code\":500,\"msg\":\"session unavailable\"}", 0);
					return;
				}
				
				// 获取 authLevel（用户级别 > 用户组级别取较大值）
				int64 iLvUser = sqlite3_column_int64(stmt_member_login, 5);
				int64 iAuthLevel = iLvUser > iLvGroup ? iLvUser : iLvGroup;
				
				// step 6 : 将用户信息填入 Session 中
				ValueSetOwnedText(tblSession, "msid", MSID);
				MSID = ValueText(tblSession, "msid");
				ValueSetInt(tblSession, "id", sqlite3_column_int64(stmt_member_login, 0));
				ValueSetInt(tblSession, "groupId", iGroupId);
				ValueSetInt(tblSession, "authLevel", iAuthLevel);
				ValueSetInt(tblSession, "balance", sqlite3_column_int64(stmt_member_login, 6));
				ValueSetText(tblSession, "username", sUsername);
				ValueSetText(tblSession, "nickname", (str)sqlite3_column_text(stmt_member_login, 7));
				if (!Session_StoreMember(MSID, tblSession)) {
					xrtValueRelease(tblSession); xrtFree(sPwdHash); xrtValueRelease(tblForm);
					sqlite3_reset(stmt_member_login);
					xsHttpReplyAuto(objResp, 500, HTTP_CT_JSON, "{\"code\":500,\"msg\":\"session unavailable\"}", 0);
					return;
				}
				
			} else {
				xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":403,\"msg\":\"用户组配置异常，请联系管理员\"}", 0);
				xrtValueRelease(tblForm);
				xrtFree(sPwdHash);
				sqlite3_reset(stmt_member_login);
				return;
			}
		}
		xrtFree(sPwdHash);
	}
	sqlite3_reset(stmt_member_login);
	
	if ( bOK ) {
		// step 7 : 重置防护模块信息
		Guard_Reset(G_GuardMember, (str)xsReqRemote(objReq));

		// step 8 : 返回响应，附带 cookie 信息（F3：与后台同级的 SameSite/Secure 属性）
		str sHeader;
		if ( xrtValueType(ValueGet(tblForm, "remember")) == XVALUE_STRING ) {
			sHeader = Session_MemberHeaders(objReq, MSID, 604800, NULL);
		} else {
			sHeader = Session_MemberHeaders(objReq, MSID, -1, NULL);
		}

		// F8：值对象构造响应，杜绝昵称经 sprintf 直拼 JSON 的结构注入
		xvalue* tblRet = ValueObject();
		xvalue* tblData = ValueObject();
		char* sBody;
		size_t iBodySize = 0;
		ValueSetInt(tblRet, "code", 0);
		ValueSetText(tblRet, "msg", "登录成功");
		ValueSetInt(tblData, "id", ValueInt(tblSession, "id"));
		ValueSetText(tblData, "username", ValueText(tblSession, "username"));
		ValueSetText(tblData, "nickname", ValueText(tblSession, "nickname"));
		ValueSetInt(tblData, "balance", ValueInt(tblSession, "balance"));
		ValueSetOwn(tblRet, "data", tblData);
		sBody = xrtJsonStringify(tblRet, false, &iBodySize);
		xsHttpReplyAuto(objResp, 200, sHeader, sBody, iBodySize);
		xrtFree(sBody);
		xrtValueRelease(tblRet);
		xrtFree(sHeader);

	} else {
		// 登录失败
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":401,\"msg\":\"用户名或密码错误\"}", 0);
		Guard_Failed(G_GuardMember, (str)xsReqRemote(objReq));
	}
	
	// step 9 : 释放表单
	if ( tblSession ) {
		xrtValueRelease(tblSession);
		tblSession = NULL;
	}
	xrtValueRelease(tblForm);
}



// ==================== 前台注册接口 ====================

// POST /api/v1/register - 前台用户注册
void API_Register(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( !(xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	
		/* R2：注册限速——每 IP 一个间隔窗口（默认 60 秒，registerIntervalSecond 可配，
	 * 成功注册才计数，失败尝试不占用窗口）。 */
	{
		str sInterval = Option_GetGlobalText("registerIntervalSecond", "60");
		int iInterval = sInterval ? atoi(sInterval) : 60;
		if ( iInterval > 0 ) {
			int iWait = Register_WaitSeconds((str)xsReqRemote(objReq), iInterval);
			if ( iWait > 0 ) {
				xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":429,\"msg\":\"注册操作过于频繁，请稍后再试\"}", 0);
				return;
			}
		}
	}

// step 1 : 解析请求数据
	xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( xrtValueType(tblForm) != XVALUE_OBJECT ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"无效的请求数据\"}", 0);
		xrtValueRelease(tblForm);
		return;
	}
	
	str sUsername = ValueText(tblForm, "username");
	str sPassword = ValueText(tblForm, "password");
	str sNickname = ValueText(tblForm, "nickname");
	
	// step 2 : 验证必填字段
	if ( !sUsername || (strlen(sUsername) < 3) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"用户名至少3个字符\"}", 0);
		xrtValueRelease(tblForm);
		return;
	}
	if ( strlen(sUsername) > 32 ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"用户名最多32个字符\"}", 0);
		xrtValueRelease(tblForm);
		return;
	}
	if ( !sPassword || (strlen(sPassword) == 0) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"密码不能为空\"}", 0);
		xrtValueRelease(tblForm);
		return;
	}
	
	// step 3 : 检查用户名是否已存在
	sqlite3_bind_text(stmt_member_chk, 1, sUsername, -1, NULL);
	int iCount = 0;
	if ( sqlite3_step(stmt_member_chk) == SQLITE_ROW ) {
		iCount = sqlite3_column_int(stmt_member_chk, 0);
	}
	sqlite3_reset(stmt_member_chk);
	
	if ( iCount > 0 ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":409,\"msg\":\"用户名已存在\"}", 0);
		xrtValueRelease(tblForm);
		return;
	}
	
	// step 4 : 生成 salt 和密码哈希
	str sSalt = Util_Token();
	str sPwdHash = ServerHashPassword(sUsername, sSalt, sPassword);
	
	// step 5 : 插入新用户
	int64 now = xrtNow();
	sqlite3_bind_text(stmt_member_add, 1, sUsername, -1, NULL);
	sqlite3_bind_text(stmt_member_add, 2, sSalt, -1, NULL);
	sqlite3_bind_text(stmt_member_add, 3, sPwdHash, -1, NULL);
	sqlite3_bind_int64(stmt_member_add, 4, 1);  // 默认用户组 ID = 1
	sqlite3_bind_int64(stmt_member_add, 5, 0);  // authLevel = 0
	sqlite3_bind_int64(stmt_member_add, 6, 0);  // balance = 0
	sqlite3_bind_text(stmt_member_add, 7, sNickname && strlen(sNickname) > 0 ? sNickname : sUsername, -1, NULL);
	sqlite3_bind_text(stmt_member_add, 8, "", -1, NULL);  // email
	sqlite3_bind_text(stmt_member_add, 9, "", -1, NULL);  // phone
	sqlite3_bind_text(stmt_member_add, 10, "", -1, NULL); // avatar
	sqlite3_bind_int(stmt_member_add, 11, 1);  // status = 1 (启用)
	sqlite3_bind_int64(stmt_member_add, 12, now);
	sqlite3_bind_int64(stmt_member_add, 13, now);
	
	bool written = DB_Write(stmt_member_add, true);
	
	xrtFree(sSalt);
	xrtFree(sPwdHash);
	
	if ( written ) {
		int64 newId = sqlite3_last_insert_rowid(G_DB);
		Register_Note((str)xsReqRemote(objReq));
		xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"注册成功\",\"data\":{\"id\":%lld}}", newId);
	} else {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":500,\"msg\":\"注册失败，请稍后重试\"}", 0);
	}
	
	xrtValueRelease(tblForm);
}



// ==================== 前台登出接口 ====================

// POST /api/v1/logout - 前台用户登出
void API_Logout(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	/* R1：仅接受 POST，防止跨站 GET 触发注销。 */
	if ( !(xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}

	// 删除 Session
	if ( xrtValueType(objSession) == XVALUE_OBJECT ) {
		str sID = ValueText(objSession, "msid");
		Session_RemoveMemberByID(sID);
	}
	
	// 清除 Cookie
	str sHeader = Session_MemberHeaders(objReq, "", 0, NULL);
	xsHttpReplyAuto(objResp, 200, sHeader, "{\"code\":0,\"msg\":\"登出成功\"}", 0);
	xrtFree(sHeader);
}



// ==================== 个人中心接口 ====================

// GET /api/v1/profile - 获取当前用户信息
// PUT /api/v1/profile - 更新当前用户信息
void API_Profile(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		// 获取当前用户信息
		int64 iMemberId = ValueInt(objSession, "id");
		
		sqlite3_bind_int64(stmt_member_get, 1, iMemberId);
		if ( sqlite3_step(stmt_member_get) == SQLITE_ROW ) {
			// id, username, groupId, authLevel, balance, nickname, email, phone, avatar, status, createTime, updateTime
			// F8：值对象构造响应，杜绝 sprintf 直拼 JSON
			xvalue* tblRet = ValueObject();
			xvalue* tblData = ValueObject();
			char* sBody;
			size_t iBodySize = 0;
			ValueSetInt(tblRet, "code", 0);
			ValueSetText(tblRet, "msg", "success");
			ValueSetInt(tblData, "id", sqlite3_column_int64(stmt_member_get, 0));
			ValueSetText(tblData, "username", (str)sqlite3_column_text(stmt_member_get, 1));
			ValueSetInt(tblData, "groupId", sqlite3_column_int64(stmt_member_get, 2));
			ValueSetInt(tblData, "authLevel", sqlite3_column_int64(stmt_member_get, 3));
			ValueSetInt(tblData, "balance", sqlite3_column_int64(stmt_member_get, 4));
			ValueSetText(tblData, "nickname", (str)sqlite3_column_text(stmt_member_get, 5));
			ValueSetText(tblData, "email", (str)sqlite3_column_text(stmt_member_get, 6));
			ValueSetText(tblData, "phone", (str)sqlite3_column_text(stmt_member_get, 7));
			ValueSetText(tblData, "avatar", (str)sqlite3_column_text(stmt_member_get, 8));
			ValueSetInt(tblData, "status", sqlite3_column_int64(stmt_member_get, 9));
			ValueSetInt(tblData, "createTime", sqlite3_column_int64(stmt_member_get, 10));
			ValueSetOwn(tblRet, "data", tblData);
			sBody = xrtJsonStringify(tblRet, false, &iBodySize);
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sBody, iBodySize);
			xrtFree(sBody);
			xrtValueRelease(tblRet);
		} else {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":404,\"msg\":\"用户不存在\"}", 0);
		}
		sqlite3_reset(stmt_member_get);
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_PUT) ) {
		// 更新当前用户信息（仅允许修改昵称、邮箱、电话、头像）
		// F1：专用语句不再触碰 groupId/authLevel/status——被禁用会员无法借改资料翻回 status=1
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"无效的请求数据\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}

		int64 iMemberId = ValueInt(objSession, "id");

		str sNickname = ValueText(tblForm, "nickname");
		str sEmail = ValueText(tblForm, "email");
		str sPhone = ValueText(tblForm, "phone");
		str sAvatar = ValueText(tblForm, "avatar");

		sqlite3_bind_text(stmt_member_profile, 1, sNickname ? sNickname : (str)"", -1, NULL);
		sqlite3_bind_text(stmt_member_profile, 2, sEmail ? sEmail : (str)"", -1, NULL);
		sqlite3_bind_text(stmt_member_profile, 3, sPhone ? sPhone : (str)"", -1, NULL);
		sqlite3_bind_text(stmt_member_profile, 4, sAvatar ? sAvatar : (str)"", -1, NULL);
		sqlite3_bind_int64(stmt_member_profile, 5, xrtNow());
		sqlite3_bind_int64(stmt_member_profile, 6, iMemberId);

		bool written = DB_Write(stmt_member_profile, true);

		if ( written ) {
			// 更新 Session 中的昵称
			if ( sNickname ) {
				ValueSetText(objSession, "nickname", sNickname);
			}
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"更新成功\"}", 0);
		} else {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":500,\"msg\":\"更新失败\"}", 0);
		}

		xrtValueRelease(tblForm);

	} else {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
	}
}



// ==================== 修改密码接口 ====================

// POST /api/v1/profile/password - 修改当前用户密码
void API_Password(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( !(xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	
	xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( xrtValueType(tblForm) != XVALUE_OBJECT ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"无效的请求数据\"}", 0);
		xrtValueRelease(tblForm);
		return;
	}
	
	str sOldPassword = ValueText(tblForm, "oldPassword");
	str sNewPassword = ValueText(tblForm, "newPassword");
	
	if ( !sOldPassword || (strlen(sOldPassword) == 0) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"原密码不能为空\"}", 0);
		xrtValueRelease(tblForm);
		return;
	}
	if ( !sNewPassword || (strlen(sNewPassword) == 0) ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"新密码不能为空\"}", 0);
		xrtValueRelease(tblForm);
		return;
	}
	
	int64 iMemberId = ValueInt(objSession, "id");
	str sUsername = ValueText(objSession, "username");
	
	// 验证原密码
	sqlite3_bind_text(stmt_member_login, 1, sUsername, -1, NULL);
	bool bOldPwdOK = false;
	while ( sqlite3_step(stmt_member_login) == SQLITE_ROW ) {
		str sSalt = (str)sqlite3_column_text(stmt_member_login, 2);
		str sStoredPwd = (str)sqlite3_column_text(stmt_member_login, 3);
		str sOldPwdHash = ServerHashPassword(sUsername, sSalt, sOldPassword);
		if ( strcmp(sOldPwdHash, sStoredPwd) == 0 ) {
			bOldPwdOK = true;
		}
		xrtFree(sOldPwdHash);
	}
	sqlite3_reset(stmt_member_login);
	
	if ( !bOldPwdOK ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":401,\"msg\":\"原密码错误\"}", 0);
		xrtValueRelease(tblForm);
		return;
	}
	
	// 生成新 salt 和密码哈希
	str sNewSalt = Util_Token();
	str sNewPwdHash = ServerHashPassword(sUsername, sNewSalt, sNewPassword);
	
	// 更新密码
	int64 now = xrtNow();
	sqlite3_bind_text(stmt_member_pwd, 1, sNewSalt, -1, NULL);
	sqlite3_bind_text(stmt_member_pwd, 2, sNewPwdHash, -1, NULL);
	sqlite3_bind_int64(stmt_member_pwd, 3, now);
	sqlite3_bind_int64(stmt_member_pwd, 4, iMemberId);
	
	bool written = DB_Write(stmt_member_pwd, true);
	
	xrtFree(sNewSalt);
	xrtFree(sNewPwdHash);
	
	if ( written ) {
		/* R3：改密成功后撤销该账号其他会话，仅保留当前请求会话。 */
		Session_RevokeAccountExcept(false, iMemberId, objSession);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"密码修改成功\"}", 0);
	} else {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":500,\"msg\":\"密码修改失败\"}", 0);
	}
	
	xrtValueRelease(tblForm);
}



// ==================== 余额查询接口 ====================

// GET /api/v1/balance - 获取当前用户余额
void API_Balance(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( !(xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	
	int64 iMemberId = ValueInt(objSession, "id");
	
	// 从数据库获取最新余额
	sqlite3_bind_int64(stmt_member_get, 1, iMemberId);
	if ( sqlite3_step(stmt_member_get) == SQLITE_ROW ) {
		int64 iBalance = sqlite3_column_int64(stmt_member_get, 4);
		// 更新 Session 中的余额
		ValueSetInt(objSession, "balance", iBalance);
		xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"success\",\"data\":{\"balance\":%lld}}", iBalance);
	} else {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":404,\"msg\":\"用户不存在\"}", 0);
	}
	sqlite3_reset(stmt_member_get);
}



// GET /api/v1/balance/log - 获取余额变动日志
void API_BalanceLog(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( !(xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	
	int64 iMemberId = ValueInt(objSession, "id");
	
	// 解析分页参数
	int64 iPage = 1;
	int64 iLimit = 20;
	char sParam[64];
	if ( xsReqQueryValue(objReq, "page", sParam, sizeof(sParam)) > 0 ) {
		iPage = atoll(sParam);
	}
	if ( xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam)) > 0 ) {
		iLimit = atoll(sParam);
	}
	if ( iPage < 1 ) iPage = 1;
	if ( iLimit < 1 ) iLimit = 20;
	if ( iLimit > 100 ) iLimit = 100;
	
	int64 iOffset = (iPage - 1) * iLimit;
	
	// 构建返回数据
	xvalue* arrRet = ValueArray();
	
	sqlite3_bind_int64(stmt_mbalance_all, 1, iMemberId);
	sqlite3_bind_int64(stmt_mbalance_all, 2, iLimit);
	sqlite3_bind_int64(stmt_mbalance_all, 3, iOffset);
	
	while ( sqlite3_step(stmt_mbalance_all) == SQLITE_ROW ) {
		xvalue* tblRow = ValueObject();
		ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_mbalance_all, 0));
		ValueSetInt(tblRow, "type", sqlite3_column_int(stmt_mbalance_all, 2));
		ValueSetInt(tblRow, "amount", sqlite3_column_int64(stmt_mbalance_all, 3));
		ValueSetInt(tblRow, "balance", sqlite3_column_int64(stmt_mbalance_all, 4));
		ValueSetText(tblRow, "remark", (str)sqlite3_column_text(stmt_mbalance_all, 5));
		ValueSetText(tblRow, "operator", (str)sqlite3_column_text(stmt_mbalance_all, 6));
		ValueSetInt(tblRow, "createTime", sqlite3_column_int64(stmt_mbalance_all, 7));
		ValueArrayOwn(arrRet, tblRow);
	}
	sqlite3_reset(stmt_mbalance_all);
	
	// 返回 JSON 响应
	size_t iJSONSize = 0;
	str sJSON = xrtJsonStringify(arrRet, false, &iJSONSize);
	xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"success\",\"data\":%s}", sJSON);
	xrtFree(sJSON);
	xrtValueRelease(arrRet);
}


