



// 前台 API 路由处理 - /api/v1/*
// Cookie 名称: MSID
// Session 表: G_MemberSession
// 权限缓存: G_CACHE_MemberGroupAuth



// ==================== 前台登录接口 ====================

// POST /api/v1/login - 前台用户登录
void API_Login(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	
	// step 1 : 暴力破解防火墙
	xtime tCD = Guard_Check(&c->rem);
	if ( tCD ) {
		str sTime = xrtTimeToStr(tCD, XRT_TIME_FORMAT_DATETIME);
		mg_http_reply(c, 200, HTTP_CT_JSON, "{\"code\":429,\"msg\":\"登录失败尝试次数过多，请于 %s 后再试\"}", sTime);
		xrtFree(sTime);
		return;
	}
	
	// step 2 : 解析请求数据
	xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( tblForm->Type != XVO_DT_TABLE ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"无效的请求数据\"}", 0);
		xvoUnref(tblForm);
		return;
	}
	str sUsername = xvoTableGetText(tblForm, "username", 8);
	if ( !sUsername || (strlen(sUsername) == 0) ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"用户名不能为空\"}", 0);
		xvoUnref(tblForm);
		return;
	}
	str sClientHash = xvoTableGetText(tblForm, "password", 8);
	if ( !sClientHash || (strlen(sClientHash) == 0) ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"密码不能为空\"}", 0);
		xvoUnref(tblForm);
		return;
	}
	
	// step 3 : 查询用户并验证密码
	bool bOK = FALSE;
	str MSID = NULL;
	xvalue tblSession = NULL;
	sqlite3_bind_text(stmt_member_login, 1, sUsername, -1, NULL);
	while ( sqlite3_step(stmt_member_login) == SQLITE_ROW ) {
		// id, username, salt, pwd, groupId, authLevel, balance, nickname, status
		int64 iStatus = sqlite3_column_int64(stmt_member_login, 8);
		if ( iStatus != 1 ) {
			http_reply(c, 200, HTTP_CT_JSON, "{\"code\":403,\"msg\":\"账号已被禁用\"}", 0);
			sqlite3_reset(stmt_member_login);
			xvoUnref(tblForm);
			return;
		}
		
		// 获取用户的 salt 和 pwd
		str sSalt = (str)sqlite3_column_text(stmt_member_login, 2);
		str sStoredPwd = (str)sqlite3_column_text(stmt_member_login, 3);
		
		// 服务端二次 SHA-256 哈希
		str sPwdHash = ServerHashPassword(sUsername, sSalt, sClientHash);
		
		// 比对密码
		if ( strcmp(sPwdHash, sStoredPwd) == 0 ) {
			
			// step 4 : 检查用户组权限
			int64 iGroupId = sqlite3_column_int64(stmt_member_login, 4);
			xvalue tblGroup = xvoListGetValue(G_CACHE_MemberGroupAuth, iGroupId);
			if ( tblGroup && (tblGroup->Type == XVO_DT_TABLE) ) {
				bOK = TRUE;
				
				// step 5 : 创建用户 Session 表（使用新的创建函数，自动设置过期时间）
				MSID = xrtMakeXIDS();
				tblSession = Session_CreateMember(MSID);
				
				// 获取 authLevel（用户级别 > 用户组级别取较大值）
				int64 iLvUser = sqlite3_column_int64(stmt_member_login, 5);
				int64 iLvGroup = xvoTableGetInt(tblGroup, "__authLevel__", 13);
				int64 iAuthLevel = iLvUser > iLvGroup ? iLvUser : iLvGroup;
				
				// step 6 : 将用户信息填入 Session 表
				xvoTableSetText(tblSession, "msid", 4, MSID, 0, TRUE);
				xvoTableSetInt(tblSession, "id", 2, sqlite3_column_int64(stmt_member_login, 0));
				xvoTableSetInt(tblSession, "groupId", 7, iGroupId);
				xvoTableSetInt(tblSession, "authLevel", 9, iAuthLevel);
				xvoTableSetInt(tblSession, "balance", 7, sqlite3_column_int64(stmt_member_login, 6));
				xvoTableSetText(tblSession, "username", 8, sUsername, 0, FALSE);
				xvoTableSetText(tblSession, "nickname", 8, (str)sqlite3_column_text(stmt_member_login, 7), 0, FALSE);
				
			} else {
				http_reply(c, 200, HTTP_CT_JSON, "{\"code\":403,\"msg\":\"用户组配置异常，请联系管理员\"}", 0);
				xvoUnref(tblForm);
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
		Guard_Reset(&c->rem);
		
		// step 8 : 返回响应，附带 cookie 信息
		str sHeader;
		if ( xvoTableItemType(tblForm, "remember", 8) == XVO_DT_TEXT ) {
			sHeader = xrtFormat("%sSet-Cookie: MSID=%s; Path=/; HttpOnly; Max-Age=604800\r\n", HTTP_CT_JSON, MSID);
		} else {
			sHeader = xrtFormat("%sSet-Cookie: MSID=%s; Path=/; HttpOnly\r\n", HTTP_CT_JSON, MSID);
		}
		
		// 构建用户信息响应
		mg_http_reply(c, 200, sHeader, 
			"{\"code\":0,\"msg\":\"登录成功\",\"data\":{\"id\":%lld,\"username\":\"%s\",\"nickname\":\"%s\",\"balance\":%lld}}",
			xvoTableGetInt(tblSession, "id", 2),
			xvoTableGetText(tblSession, "username", 8),
			xvoTableGetText(tblSession, "nickname", 8),
			xvoTableGetInt(tblSession, "balance", 7)
		);
		xrtFree(sHeader);
		
	} else {
		// 登录失败
		http_reply(c, 200, HTTP_CT_JSON, "{\"code\":401,\"msg\":\"用户名或密码错误\"}", 0);
		Guard_Failed(&c->rem);
	}
	
	// step 9 : 释放表单
	xvoUnref(tblForm);
}



// ==================== 前台注册接口 ====================

// POST /api/v1/register - 前台用户注册
void API_Register(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	
	// step 1 : 解析请求数据
	xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( tblForm->Type != XVO_DT_TABLE ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"无效的请求数据\"}", 0);
		xvoUnref(tblForm);
		return;
	}
	
	str sUsername = xvoTableGetText(tblForm, "username", 8);
	str sPassword = xvoTableGetText(tblForm, "password", 8);
	str sNickname = xvoTableGetText(tblForm, "nickname", 8);
	
	// step 2 : 验证必填字段
	if ( !sUsername || (strlen(sUsername) < 3) ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"用户名至少3个字符\"}", 0);
		xvoUnref(tblForm);
		return;
	}
	if ( strlen(sUsername) > 32 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"用户名最多32个字符\"}", 0);
		xvoUnref(tblForm);
		return;
	}
	if ( !sPassword || (strlen(sPassword) == 0) ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"密码不能为空\"}", 0);
		xvoUnref(tblForm);
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
		http_reply(c, 200, HTTP_CT_JSON, "{\"code\":409,\"msg\":\"用户名已存在\"}", 0);
		xvoUnref(tblForm);
		return;
	}
	
	// step 4 : 生成 salt 和密码哈希
	str sSalt = xrtMakeXIDS();
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
	
	int rc = sqlite3_step(stmt_member_add);
	sqlite3_reset(stmt_member_add);
	
	xrtFree(sSalt);
	xrtFree(sPwdHash);
	
	if ( rc == SQLITE_DONE ) {
		int64 newId = sqlite3_last_insert_rowid(G_DB->objDB);
		mg_http_reply(c, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"注册成功\",\"data\":{\"id\":%lld}}", newId);
	} else {
		http_reply(c, 200, HTTP_CT_JSON, "{\"code\":500,\"msg\":\"注册失败，请稍后重试\"}", 0);
	}
	
	xvoUnref(tblForm);
}



// ==================== 前台登出接口 ====================

// POST /api/v1/logout - 前台用户登出
void API_Logout(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	// 删除 Session
	if ( hm->session->Type == XVO_DT_TABLE ) {
		str sID = xvoTableGetText(hm->session, "msid", 4);
		xvoTableRemove(G_MemberSession, sID, 0);
	}
	
	// 清除 Cookie
	str sHeader = xrtFormat("%sSet-Cookie: MSID=; Path=/; HttpOnly; Max-Age=0\r\n", HTTP_CT_JSON);
	http_reply(c, 200, sHeader, "{\"code\":0,\"msg\":\"登出成功\"}", 0);
	xrtFree(sHeader);
}



// ==================== 个人中心接口 ====================

// GET /api/v1/profile - 获取当前用户信息
// PUT /api/v1/profile - 更新当前用户信息
void API_Profile(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode == HTTP_GET ) {
		// 获取当前用户信息
		int64 iMemberId = xvoTableGetInt(hm->session, "id", 2);
		
		sqlite3_bind_int64(stmt_member_get, 1, iMemberId);
		if ( sqlite3_step(stmt_member_get) == SQLITE_ROW ) {
			// id, username, groupId, authLevel, balance, nickname, email, phone, avatar, status, createTime, updateTime
			mg_http_reply(c, 200, HTTP_CT_JSON, 
				"{\"code\":0,\"msg\":\"success\",\"data\":{"
				"\"id\":%lld,"
				"\"username\":\"%s\","
				"\"groupId\":%lld,"
				"\"authLevel\":%lld,"
				"\"balance\":%lld,"
				"\"nickname\":\"%s\","
				"\"email\":\"%s\","
				"\"phone\":\"%s\","
				"\"avatar\":\"%s\","
				"\"status\":%lld,"
				"\"createTime\":%lld"
				"}}",
				sqlite3_column_int64(stmt_member_get, 0),
				(str)sqlite3_column_text(stmt_member_get, 1),
				sqlite3_column_int64(stmt_member_get, 2),
				sqlite3_column_int64(stmt_member_get, 3),
				sqlite3_column_int64(stmt_member_get, 4),
				(str)sqlite3_column_text(stmt_member_get, 5),
				(str)sqlite3_column_text(stmt_member_get, 6),
				(str)sqlite3_column_text(stmt_member_get, 7),
				(str)sqlite3_column_text(stmt_member_get, 8),
				sqlite3_column_int64(stmt_member_get, 9),
				sqlite3_column_int64(stmt_member_get, 10)
			);
		} else {
			http_reply(c, 200, HTTP_CT_JSON, "{\"code\":404,\"msg\":\"用户不存在\"}", 0);
		}
		sqlite3_reset(stmt_member_get);
		
	} else if ( hm->methodCode == HTTP_PUT ) {
		// 更新当前用户信息（仅允许修改昵称、邮箱、电话、头像）
		xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
		if ( tblForm->Type != XVO_DT_TABLE ) {
			http_reply(c, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"无效的请求数据\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		
		int64 iMemberId = xvoTableGetInt(hm->session, "id", 2);
		int64 iGroupId = xvoTableGetInt(hm->session, "groupId", 7);
		int64 iAuthLevel = xvoTableGetInt(hm->session, "authLevel", 9);
		int64 now = xrtNow();
		
		str sNickname = xvoTableGetText(tblForm, "nickname", 8);
		str sEmail = xvoTableGetText(tblForm, "email", 5);
		str sPhone = xvoTableGetText(tblForm, "phone", 5);
		str sAvatar = xvoTableGetText(tblForm, "avatar", 6);
		
		// stmt_member_put: UPDATE member SET groupId=?, authLevel=?, nickname=?, email=?, phone=?, avatar=?, status=?, updateTime=? WHERE id=?
		sqlite3_bind_int64(stmt_member_put, 1, iGroupId);
		sqlite3_bind_int64(stmt_member_put, 2, iAuthLevel);
		sqlite3_bind_text(stmt_member_put, 3, sNickname ? sNickname : (str)"", -1, NULL);
		sqlite3_bind_text(stmt_member_put, 4, sEmail ? sEmail : (str)"", -1, NULL);
		sqlite3_bind_text(stmt_member_put, 5, sPhone ? sPhone : (str)"", -1, NULL);
		sqlite3_bind_text(stmt_member_put, 6, sAvatar ? sAvatar : (str)"", -1, NULL);
		sqlite3_bind_int(stmt_member_put, 7, 1);  // status 保持不变
		sqlite3_bind_int64(stmt_member_put, 8, now);
		sqlite3_bind_int64(stmt_member_put, 9, iMemberId);
		
		int rc = sqlite3_step(stmt_member_put);
		sqlite3_reset(stmt_member_put);
		
		if ( rc == SQLITE_DONE ) {
			// 更新 Session 中的昵称
			if ( sNickname ) {
				xvoTableSetText(hm->session, "nickname", 8, sNickname, 0, FALSE);
			}
			http_reply(c, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"更新成功\"}", 0);
		} else {
			http_reply(c, 200, HTTP_CT_JSON, "{\"code\":500,\"msg\":\"更新失败\"}", 0);
		}
		
		xvoUnref(tblForm);
		
	} else {
		http_reply(c, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
	}
}



// ==================== 修改密码接口 ====================

// POST /api/v1/profile/password - 修改当前用户密码
void API_Password(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( tblForm->Type != XVO_DT_TABLE ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"无效的请求数据\"}", 0);
		xvoUnref(tblForm);
		return;
	}
	
	str sOldPassword = xvoTableGetText(tblForm, "oldPassword", 11);
	str sNewPassword = xvoTableGetText(tblForm, "newPassword", 11);
	
	if ( !sOldPassword || (strlen(sOldPassword) == 0) ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"原密码不能为空\"}", 0);
		xvoUnref(tblForm);
		return;
	}
	if ( !sNewPassword || (strlen(sNewPassword) == 0) ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"新密码不能为空\"}", 0);
		xvoUnref(tblForm);
		return;
	}
	
	int64 iMemberId = xvoTableGetInt(hm->session, "id", 2);
	str sUsername = xvoTableGetText(hm->session, "username", 8);
	
	// 验证原密码
	sqlite3_bind_text(stmt_member_login, 1, sUsername, -1, NULL);
	bool bOldPwdOK = FALSE;
	while ( sqlite3_step(stmt_member_login) == SQLITE_ROW ) {
		str sSalt = (str)sqlite3_column_text(stmt_member_login, 2);
		str sStoredPwd = (str)sqlite3_column_text(stmt_member_login, 3);
		str sOldPwdHash = ServerHashPassword(sUsername, sSalt, sOldPassword);
		if ( strcmp(sOldPwdHash, sStoredPwd) == 0 ) {
			bOldPwdOK = TRUE;
		}
		xrtFree(sOldPwdHash);
	}
	sqlite3_reset(stmt_member_login);
	
	if ( !bOldPwdOK ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"code\":401,\"msg\":\"原密码错误\"}", 0);
		xvoUnref(tblForm);
		return;
	}
	
	// 生成新 salt 和密码哈希
	str sNewSalt = xrtMakeXIDS();
	str sNewPwdHash = ServerHashPassword(sUsername, sNewSalt, sNewPassword);
	
	// 更新密码
	int64 now = xrtNow();
	sqlite3_bind_text(stmt_member_pwd, 1, sNewSalt, -1, NULL);
	sqlite3_bind_text(stmt_member_pwd, 2, sNewPwdHash, -1, NULL);
	sqlite3_bind_int64(stmt_member_pwd, 3, now);
	sqlite3_bind_int64(stmt_member_pwd, 4, iMemberId);
	
	int rc = sqlite3_step(stmt_member_pwd);
	sqlite3_reset(stmt_member_pwd);
	
	xrtFree(sNewSalt);
	xrtFree(sNewPwdHash);
	
	if ( rc == SQLITE_DONE ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"密码修改成功\"}", 0);
	} else {
		http_reply(c, 200, HTTP_CT_JSON, "{\"code\":500,\"msg\":\"密码修改失败\"}", 0);
	}
	
	xvoUnref(tblForm);
}



// ==================== 余额查询接口 ====================

// GET /api/v1/balance - 获取当前用户余额
void API_Balance(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_GET ) {
		http_reply(c, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	
	int64 iMemberId = xvoTableGetInt(hm->session, "id", 2);
	
	// 从数据库获取最新余额
	sqlite3_bind_int64(stmt_member_get, 1, iMemberId);
	if ( sqlite3_step(stmt_member_get) == SQLITE_ROW ) {
		int64 iBalance = sqlite3_column_int64(stmt_member_get, 4);
		// 更新 Session 中的余额
		xvoTableSetInt(hm->session, "balance", 7, iBalance);
		mg_http_reply(c, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"success\",\"data\":{\"balance\":%lld}}", iBalance);
	} else {
		http_reply(c, 200, HTTP_CT_JSON, "{\"code\":404,\"msg\":\"用户不存在\"}", 0);
	}
	sqlite3_reset(stmt_member_get);
}



// GET /api/v1/balance/log - 获取余额变动日志
void API_BalanceLog(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_GET ) {
		http_reply(c, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	
	int64 iMemberId = xvoTableGetInt(hm->session, "id", 2);
	
	// 解析分页参数
	int64 iPage = 1;
	int64 iLimit = 20;
	char sParam[64];
	if ( mg_http_get_var(&hm->query, "page", sParam, sizeof(sParam)) > 0 ) {
		iPage = atoll(sParam);
	}
	if ( mg_http_get_var(&hm->query, "limit", sParam, sizeof(sParam)) > 0 ) {
		iLimit = atoll(sParam);
	}
	if ( iPage < 1 ) iPage = 1;
	if ( iLimit < 1 ) iLimit = 20;
	if ( iLimit > 100 ) iLimit = 100;
	
	int64 iOffset = (iPage - 1) * iLimit;
	
	// 构建返回数据
	xvalue arrRet = xvoCreateArray();
	
	sqlite3_bind_int64(stmt_mbalance_all, 1, iMemberId);
	sqlite3_bind_int64(stmt_mbalance_all, 2, iLimit);
	sqlite3_bind_int64(stmt_mbalance_all, 3, iOffset);
	
	while ( sqlite3_step(stmt_mbalance_all) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_mbalance_all, 0));
		xvoTableSetInt(tblRow, "type", 4, sqlite3_column_int(stmt_mbalance_all, 2));
		xvoTableSetInt(tblRow, "amount", 6, sqlite3_column_int64(stmt_mbalance_all, 3));
		xvoTableSetInt(tblRow, "balance", 7, sqlite3_column_int64(stmt_mbalance_all, 4));
		xvoTableSetText(tblRow, "remark", 6, (str)sqlite3_column_text(stmt_mbalance_all, 5), 0, FALSE);
		xvoTableSetText(tblRow, "operator", 8, (str)sqlite3_column_text(stmt_mbalance_all, 6), 0, FALSE);
		xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt_mbalance_all, 7));
		xvoArrayAppendValue(arrRet, tblRow, TRUE);
	}
	sqlite3_reset(stmt_mbalance_all);
	
	// 返回 JSON 响应
	size_t iJSONSize = 0;
	str sJSON = xrtStringifyJSON(arrRet, FALSE, &iJSONSize);
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"success\",\"data\":%s}", sJSON);
	xrtFree(sJSON);
	xvoUnref(arrRet);
}


