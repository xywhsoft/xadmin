



// 后台管理前台用户的路由处理 - /admin/member/*



// ==================== 前台用户管理 ====================

void Request_View_Member_User(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		LoadPage(objResp, 200, HTTP_CT_HTML, "member/user.html");
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_View_Member_User_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		xvalue* tblInfo = ValueObject();
		ValueSetRef(tblInfo, "groupList", G_CACHE_MemberGroup);
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("member/user_add.html", tblInfo, &iSize);
		xrtValueRelease(tblInfo);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_View_Member_User_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		char sID[24];
		xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		int64 id = Util_ParseI64(sID);
		
		xvalue* tblInfo = ValueObject();
		int bRow = false;
		sqlite3_bind_int64(stmt_member_get, 1, id);
		if ( sqlite3_step(stmt_member_get) == SQLITE_ROW ) {
			bRow = true;
			ValueSetInt(tblInfo, "id", sqlite3_column_int64(stmt_member_get, 0));
			ValueSetText(tblInfo, "username", (str)sqlite3_column_text(stmt_member_get, 1));
			ValueSetInt(tblInfo, "groupId", sqlite3_column_int64(stmt_member_get, 2));
			ValueSetInt(tblInfo, "authLevel", sqlite3_column_int64(stmt_member_get, 3));
			ValueSetInt(tblInfo, "balance", sqlite3_column_int64(stmt_member_get, 4));
			ValueSetText(tblInfo, "nickname", (str)sqlite3_column_text(stmt_member_get, 5));
			ValueSetText(tblInfo, "email", (str)sqlite3_column_text(stmt_member_get, 6));
			ValueSetText(tblInfo, "phone", (str)sqlite3_column_text(stmt_member_get, 7));
			ValueSetInt(tblInfo, "status", sqlite3_column_int64(stmt_member_get, 9));
		}
		sqlite3_reset(stmt_member_get);
		if ( !bRow ) {
			xrtValueRelease(tblInfo);
			LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
			return;
		}
		ValueSetRef(tblInfo, "groupList", G_CACHE_MemberGroup);
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("member/user_edit.html", tblInfo, &iSize);
		xrtValueRelease(tblInfo);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_View_Member_User_Balance(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		char sID[24];
		xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		int64 id = Util_ParseI64(sID);
		
		xvalue* tblInfo = ValueObject();
		sqlite3_bind_int64(stmt_member_get, 1, id);
		if ( sqlite3_step(stmt_member_get) == SQLITE_ROW ) {
			ValueSetInt(tblInfo, "id", sqlite3_column_int64(stmt_member_get, 0));
			ValueSetText(tblInfo, "username", (str)sqlite3_column_text(stmt_member_get, 1));
			int64 balance = sqlite3_column_int64(stmt_member_get, 4);
			ValueSetInt(tblInfo, "balance", balance);
			// 转换为元，保留两位小数
			char sBalanceYuan[32];
			snprintf(sBalanceYuan, sizeof(sBalanceYuan), "%.2f", balance / 100.0);
			ValueSetText(tblInfo, "balanceYuan", sBalanceYuan);
		}
		sqlite3_reset(stmt_member_get);
		if ( sqlite3_column_count(stmt_member_get) >= 0 && ValueCount(tblInfo) == 0 ) {
			/* L6：查无记录渲染 404（与 edit 视图守卫同款，原为空表单 200） */
			xrtValueRelease(tblInfo);
			LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
			return;
		}
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("member/user_balance.html", tblInfo, &iSize);
		xrtValueRelease(tblInfo);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_Member_User(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		char sParam[64];
		xsReqQueryValue(objReq, "page", sParam, sizeof(sParam));
		int64 iPage = Util_ParseI64(sParam);
		if ( iPage <= 0 ) iPage = 1;
		xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam));
		int64 iLimit = Util_ParseI64(sParam);
		if ( iLimit <= 0 ) iLimit = 10;
		if ( iLimit > 100 ) iLimit = 100;	/* F9 */
		int64 iOffset = (iPage - 1) * iLimit;
		
		xvalue* data = ValueArray();
		int64 iCount = 0;
		/* v1 契约：search 为 LIKE 模式串（前端预包 %kw%），按原样绑定 */
		int iSearch = xsReqQueryValue(objReq, "search", sParam, sizeof(sParam));

		if ( iSearch > 0 ) {
			sqlite3_bind_text(stmt_member_sel, 1, sParam, -1, NULL);
			sqlite3_bind_text(stmt_member_sel, 2, sParam, -1, NULL);
			sqlite3_bind_text(stmt_member_sel, 3, sParam, -1, NULL);
			sqlite3_bind_int64(stmt_member_sel, 4, iLimit);
			sqlite3_bind_int64(stmt_member_sel, 5, iOffset);
			while ( sqlite3_step(stmt_member_sel) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject();
				ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_member_sel, 0));
				ValueSetText(tblRow, "username", (str)sqlite3_column_text(stmt_member_sel, 1));
				ValueSetInt(tblRow, "groupId", sqlite3_column_int64(stmt_member_sel, 2));
				ValueSetInt(tblRow, "authLevel", sqlite3_column_int64(stmt_member_sel, 3));
				ValueSetInt(tblRow, "balance", sqlite3_column_int64(stmt_member_sel, 4));
				ValueSetText(tblRow, "nickname", (str)sqlite3_column_text(stmt_member_sel, 5));
				ValueSetInt(tblRow, "status", sqlite3_column_int64(stmt_member_sel, 9));
				xtime iTime = sqlite3_column_int64(stmt_member_sel, 10);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_member_sel);
			sqlite3_bind_text(stmt_member_count_sel, 1, sParam, -1, NULL);
			if ( sqlite3_step(stmt_member_count_sel) == SQLITE_ROW ) iCount = sqlite3_column_int64(stmt_member_count_sel, 0);
			sqlite3_reset(stmt_member_count_sel);
		} else {
		sqlite3_bind_int64(stmt_member_all, 1, iLimit);
		sqlite3_bind_int64(stmt_member_all, 2, iOffset);
		while ( sqlite3_step(stmt_member_all) == SQLITE_ROW ) {
			xvalue* tblRow = ValueObject();
			ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_member_all, 0));
			ValueSetText(tblRow, "username", (str)sqlite3_column_text(stmt_member_all, 1));
			ValueSetInt(tblRow, "groupId", sqlite3_column_int64(stmt_member_all, 2));
			ValueSetInt(tblRow, "authLevel", sqlite3_column_int64(stmt_member_all, 3));
			ValueSetInt(tblRow, "balance", sqlite3_column_int64(stmt_member_all, 4));
			ValueSetText(tblRow, "nickname", (str)sqlite3_column_text(stmt_member_all, 5));
			ValueSetInt(tblRow, "status", sqlite3_column_int64(stmt_member_all, 9));
			xtime iTime = sqlite3_column_int64(stmt_member_all, 10);
			ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
			ValueArrayOwn(data, tblRow);
		}
		sqlite3_reset(stmt_member_all);
			if ( sqlite3_step(stmt_member_count_all) == SQLITE_ROW ) iCount = sqlite3_column_int64(stmt_member_count_all, 0);
			sqlite3_reset(stmt_member_count_all);
		}
		
		xvalue* tblRet = ValueObject();
		ValueSetBool(tblRet, "result", true);
		ValueSetInt(tblRet, "code", 0);
		ValueSetInt(tblRet, "count", iCount);
		ValueSetOwn(tblRet, "data", data);
		size_t iRetSize = 0;
		char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xrtValueRelease(tblRet);
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xrtValueRelease(tblForm); return; }
		str username = ValueText(tblForm, "username");
		str password = ValueText(tblForm, "password");
		xstrview account = {0}; char accountKey[65];
		if (!xrtValueGetString(ValueGet(tblForm,"username"), &account) || !XA_AccountKey(account.Data, account.Size, accountKey)) { xsHttpReplyAuto(objResp,400,HTTP_CT_JSON,"{\"result\":false,\"message\":\"账号格式无效，须含字母，3至64个ASCII字母、数字、点、下划线或连字符\"}",0); xrtValueRelease(tblForm); return; }
		if (ValueHas(tblForm,"email") || ValueHas(tblForm,"phone") || ValueHas(tblForm,"email_verified_at") || ValueHas(tblForm,"phone_verified_at")) { xsHttpReplyAuto(objResp,400,HTTP_CT_JSON,"{\"result\":false,\"message\":\"联系方式只能通过验证码绑定流程修改\"}",0); xrtValueRelease(tblForm); return; }
		str nickname = ValueText(tblForm, "nickname");
		int64 groupId = ValueInt(tblForm, "groupId");
		int64 authLevel = ValueInt(tblForm, "authLevel");
		int64 status = ValueInt(tblForm, "status");
		if ( !username || strlen(username) == 0 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名不能为空！\"}", 0); xrtValueRelease(tblForm); return; }
		if ( !password || strlen(password) == 0 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"密码不能为空！\"}", 0); xrtValueRelease(tblForm); return; }
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( strlen(username) > 64 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名最多64个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		if ( strlen(password) > 128 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"密码最多128个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		if ( nickname && strlen(nickname) > 64 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"昵称最多64个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		if ( groupId < 1 ) groupId = 1;
		if ( status != 0 && status != 1 ) status = 1;
		
		sqlite3_bind_text(stmt_member_chk, 1, username, -1, NULL);
		int iCount = 0;
		if ( sqlite3_step(stmt_member_chk) == SQLITE_ROW ) iCount = sqlite3_column_int(stmt_member_chk, 0);
		sqlite3_reset(stmt_member_chk);
		if ( iCount > 0 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名已存在！\"}", 0); xrtValueRelease(tblForm); return; }
		
		if (!XA_PasswordPolicy(password)) { xsHttpReplyAuto(objResp,400,HTTP_CT_JSON,"{\"result\":false,\"message\":\"密码须为8至128字节\"}",0); xrtValueRelease(tblForm); return; }
		char record[257]={0}; xrtMutexUnlock(G_RequestLock); bool hashed=XA_PasswordHash(password,record); xrtMutexLock(G_RequestLock);
		if (!hashed) { xsHttpReplyAuto(objResp,500,HTTP_CT_JSON,"{\"result\":false,\"message\":\"密码服务不可用\"}",0); xrtValueRelease(tblForm); return; }
		str sSalt=xrtStrDup(""); str sPwdHash=xrtStrDup(record); xrtSecureZero(record,sizeof(record));
		if (!sSalt||!sPwdHash) { xrtFree(sSalt);xrtFree(sPwdHash);xrtValueRelease(tblForm); xsHttpReplyAuto(objResp,500,HTTP_CT_JSON,"{\"result\":false,\"message\":\"密码服务不可用\"}",0);return; }
		xtime now = xrtNow();
		sqlite3_bind_text(stmt_member_add, 1, username, -1, NULL);
		sqlite3_bind_text(stmt_member_add, 2, sSalt, -1, NULL);
		sqlite3_bind_text(stmt_member_add, 3, sPwdHash, -1, NULL);
		sqlite3_bind_int64(stmt_member_add, 4, groupId);
		sqlite3_bind_int64(stmt_member_add, 5, authLevel);
		sqlite3_bind_int64(stmt_member_add, 6, 0);
		sqlite3_bind_text(stmt_member_add, 7, nickname && strlen(nickname) > 0 ? nickname : username, -1, NULL);
		sqlite3_bind_text(stmt_member_add, 8, "", -1, NULL);
		sqlite3_bind_text(stmt_member_add, 9, "", -1, NULL);
		sqlite3_bind_text(stmt_member_add, 10, "", -1, NULL);
		sqlite3_bind_int(stmt_member_add, 11, status);
		sqlite3_bind_int64(stmt_member_add, 12, now);
		sqlite3_bind_int64(stmt_member_add, 13, now);
		bool written = DB_Write(stmt_member_add, true);
		int64 newId = written ? sqlite3_last_insert_rowid(G_DB) : 0;
		xrtFree(sSalt); xrtFree(sPwdHash); xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"前台用户添加成功！\", \"data\": {\"id\": %lld}}", newId);
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_PUT) ) {
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xrtValueRelease(tblForm); return; }
		int64 id = ValueInt(tblForm, "id");
		int64 groupId = ValueInt(tblForm, "groupId");
		int64 authLevel = ValueInt(tblForm, "authLevel");
		str nickname = ValueText(tblForm, "nickname");
		if (ValueHas(tblForm,"email") || ValueHas(tblForm,"phone") || ValueHas(tblForm,"email_verified_at") || ValueHas(tblForm,"phone_verified_at")) { xsHttpReplyAuto(objResp,400,HTTP_CT_JSON,"{\"result\":false,\"message\":\"联系方式只能通过验证码绑定流程修改\"}",0); xrtValueRelease(tblForm); return; }
		str avatar = ValueText(tblForm, "avatar");
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( nickname && strlen(nickname) > 64 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"昵称最多64个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		if ( avatar && strlen(avatar) > 512 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"头像地址最多512个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		int64 status = ValueInt(tblForm, "status");
		if ( groupId < 1 ) groupId = 1;
		if ( status != 0 && status != 1 ) status = 1; /* L7：与 POST 同款钳位 */
		/* 记录权限相关旧值：组/权限级别变更须撤销会话（会话内是登录快照） */
		int64 oldGroupId = 0, oldAuthLevel = 0;
		sqlite3_bind_int64(stmt_member_get, 1, id);
		if ( sqlite3_step(stmt_member_get) == SQLITE_ROW ) {
			oldGroupId = sqlite3_column_int64(stmt_member_get, 2);
			oldAuthLevel = sqlite3_column_int64(stmt_member_get, 3);
		}
		sqlite3_reset(stmt_member_get);
		xtime now = xrtNow();
		sqlite3_bind_int64(stmt_member_put, 1, groupId);
		sqlite3_bind_int64(stmt_member_put, 2, authLevel);
		sqlite3_bind_text(stmt_member_put, 3, nickname ? nickname : (str)"", -1, NULL);
		sqlite3_bind_text(stmt_member_put, 4, avatar ? avatar : (str)"", -1, NULL);
		sqlite3_bind_int(stmt_member_put, 5, status);
		sqlite3_bind_int64(stmt_member_put, 6, now);
		sqlite3_bind_int64(stmt_member_put, 7, id);
		bool revoke=status==0||oldGroupId!=groupId||oldAuthLevel!=authLevel;
		bool begun=DB_BeginWrite();bool written=begun&&DB_Write(stmt_member_put,true);
		if(!begun)DB_ResetWrite(stmt_member_put);
		if(written&&revoke)written=XA_SessionRevokeAccount(id,NULL);
		if(begun)written=DB_EndWrite(written);
		xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		if ( status == 0 || oldGroupId != groupId || oldAuthLevel != authLevel ) {
			/* F1：禁用即时生效；组/权限级别变更同样撤销（会话内是登录快照）。 */
			Session_RevokeAccount(false, id);
		}
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"前台用户更新成功！\"}", 0);
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_DELETE) ) {
		char sID[24];
		xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		int64 id = Util_ParseI64(sID);
		if ( id > 0 ) {
			xtime now = xrtNow();
			sqlite3_bind_int64(stmt_member_del, 1, now);
			sqlite3_bind_int64(stmt_member_del, 2, id);
			bool written = DB_Write(stmt_member_del, true);
			if (ReplyIfWriteFailed(objResp, written)) return;
			Session_RevokeAccount(false, id);
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"前台用户删除成功！\"}", 0);
		} else {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的用户ID\"}", 0);
		}
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_Member_User_Repwd(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xrtValueRelease(tblForm); return; }
		int64 id = ValueInt(tblForm, "id");
		str username = ValueText(tblForm, "username");
		str password = ValueText(tblForm, "password");
		if ( !username || strlen(username) == 0 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名不能为空！\"}", 0); xrtValueRelease(tblForm); return; }
		if ( !password || strlen(password) == 0 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"密码不能为空！\"}", 0); xrtValueRelease(tblForm); return; }
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( strlen(username) > 64 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名最多64个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		if ( strlen(password) > 128 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"密码最多128个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		if (!XA_PasswordPolicy(password)) { xsHttpReplyAuto(objResp,400,HTTP_CT_JSON,"{\"result\":false,\"message\":\"密码须为8至128字节\"}",0);xrtValueRelease(tblForm);return; }
		char record[257]={0};xrtMutexUnlock(G_RequestLock);bool hashed=XA_PasswordHash(password,record);xrtMutexLock(G_RequestLock);
		if(!hashed){xsHttpReplyAuto(objResp,500,HTTP_CT_JSON,"{\"result\":false,\"message\":\"密码服务不可用\"}",0);xrtValueRelease(tblForm);return;}
		str sSalt=xrtStrDup("");str sPwdHash=xrtStrDup(record);xrtSecureZero(record,sizeof(record));
		if(!sSalt||!sPwdHash){xrtFree(sSalt);xrtFree(sPwdHash);xrtValueRelease(tblForm);xsHttpReplyAuto(objResp,500,HTTP_CT_JSON,"{\"result\":false,\"message\":\"密码服务不可用\"}",0);return;}
		xtime now = xrtNow();
		sqlite3_bind_text(stmt_member_pwd, 1, sSalt, -1, NULL);
		sqlite3_bind_text(stmt_member_pwd, 2, sPwdHash, -1, NULL);
		sqlite3_bind_int64(stmt_member_pwd, 3, now);
		sqlite3_bind_int64(stmt_member_pwd, 4, id);
		bool begun=DB_BeginWrite();bool written=begun&&DB_Write(stmt_member_pwd,true);
		if(!begun)DB_ResetWrite(stmt_member_pwd);
		if(written)written=XA_SessionRevokeAccount(id,NULL);
		if(begun)written=DB_EndWrite(written);
		xrtFree(sSalt); xrtFree(sPwdHash); xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		/* R3：重置密码成功后撤销该账号全部会话（管理员代重置，无当前会话需保留）。 */
		Session_RevokeAccount(false, id);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"密码重置成功！\"}", 0);
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_Member_User_Balance(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xrtValueRelease(tblForm); return; }
		int64 memberId = ValueInt(tblForm, "id");
		int type = ValueInt(tblForm, "type");
		int64 amount = ValueInt(tblForm, "amount");
		str remark = ValueText(tblForm, "remark");
		str operator = ValueText(objSession, "user");
		bool bOK = Member_ChangeBalance(memberId, type, amount, remark, operator);
		xrtValueRelease(tblForm);
		if ( bOK ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"余额调整成功！\"}", 0);
		} else {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"余额调整失败！\"}", 0);
		}
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}



// ==================== 前台用户组管理 ====================

void Request_View_Member_Group(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) { LoadPage(objResp, 200, HTTP_CT_HTML, "member/group.html"); }
	else { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); }
}

void Request_View_Member_Group_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		xvalue* tblInfo = ValueObject();
		ValueSetRef(tblInfo, "authGroups", G_CACHE_MemberAuthGroup);
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("member/group_add.html", tblInfo, &iSize);
		xrtValueRelease(tblInfo);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
	} else { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); }
}

void Request_View_Member_Group_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		char sID[24];
		xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		int64 id = Util_ParseI64(sID);
		xvalue* tblInfo = ValueObject();
		xvalue* listAuth = xrtValueIntMap();
		int bRow = false;
		sqlite3_bind_int64(stmt_mgroup_get, 1, id);
		if ( sqlite3_step(stmt_mgroup_get) == SQLITE_ROW ) {
			bRow = true;
			ValueSetInt(tblInfo, "id", id);
			ValueSetText(tblInfo, "name", (str)sqlite3_column_text(stmt_mgroup_get, 1));
			ValueSetText(tblInfo, "desc", (str)sqlite3_column_text(stmt_mgroup_get, 2));
			ValueSetInt(tblInfo, "authLevel", sqlite3_column_int64(stmt_mgroup_get, 4));
			str sAuthList = (str)sqlite3_column_text(stmt_mgroup_get, 3);
			if ( sAuthList && strlen(sAuthList) > 2 ) {
				xvalue* arrAuth = JsonParseN(sAuthList, 0);
				if ( arrAuth && xrtValueType(arrAuth) == XVALUE_ARRAY ) {
					for ( int i = 0; i < xrtValueCount(arrAuth); i++ ) {
						int64 authID = ValueArrayInt(arrAuth, i);
						if ( authID > 0 ) ValueMapSetBool(listAuth, authID, true);
					}
				}
				xrtValueRelease(arrAuth);
			}
		}
		sqlite3_reset(stmt_mgroup_get);
		if ( !bRow ) {
			xrtValueRelease(tblInfo);
			LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
			return;
		}
		xvalue* arrAuthGroups = xrtValueDeepClone(G_CACHE_MemberAuthGroup);
		for ( int g = 1; g <= xrtValueCount(arrAuthGroups); g++ ) {
			xvalue* tblGroup = xrtValueArrayGet(arrAuthGroups, (g) - 1);
			xvalue* arrAuths = ValueGet(tblGroup, "auths");
			if ( arrAuths && xrtValueType(arrAuths) == XVALUE_ARRAY ) {
				for ( int a = 1; a <= xrtValueCount(arrAuths); a++ ) {
					xvalue* tblAuth = xrtValueArrayGet(arrAuths, (a) - 1);
					int64 authID = ValueInt(tblAuth, "id");
					bool bCheck = ValueMapBool(listAuth, authID);
					ValueSetText(tblAuth, "checked", bCheck ? (str)" checked" : (str)"");
				}
			}
		}
		xrtValueRelease(listAuth);
		ValueSetOwn(tblInfo, "authGroups", arrAuthGroups);
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("member/group_edit.html", tblInfo, &iSize);
		xrtValueRelease(tblInfo);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
	} else { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); }
}

void Request_Member_Group(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		char sParam[64];
		xsReqQueryValue(objReq, "page", sParam, sizeof(sParam));
		int64 iPage = Util_ParseI64(sParam); if ( iPage <= 0 ) iPage = 1;
		xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam));
		int64 iLimit = Util_ParseI64(sParam); if ( iLimit <= 0 ) iLimit = 10;
		if ( iLimit > 100 ) iLimit = 100;	/* F9 */
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = xsReqQueryValue(objReq, "search", sParam, sizeof(sParam));
		
		xvalue* data = ValueArray(); int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_int64(stmt_mgroup_all, 1, iLimit);
			sqlite3_bind_int64(stmt_mgroup_all, 2, iOffset);
			while ( sqlite3_step(stmt_mgroup_all) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject(); int64 rowId = sqlite3_column_int64(stmt_mgroup_all, 0);
				ValueSetInt(tblRow, "id", rowId);
				ValueSetText(tblRow, "name", (str)sqlite3_column_text(stmt_mgroup_all, 1));
				ValueSetText(tblRow, "desc", (str)sqlite3_column_text(stmt_mgroup_all, 2));
				// 解析 authList 获取权限数量
				str authList = (str)sqlite3_column_text(stmt_mgroup_all, 3);
				int64 authCount = 0;
				if ( authList && strlen(authList) > 2 ) {
					xvalue* arrAuth = JsonParseN(authList, strlen(authList));
					if ( (arrAuth) && (xrtValueType(arrAuth) == XVALUE_ARRAY) ) authCount = ValueCount(arrAuth);
					if ( arrAuth ) xrtValueRelease(arrAuth);
				}
				ValueSetInt(tblRow, "authCount", authCount);
				ValueSetInt(tblRow, "authLevel", sqlite3_column_int64(stmt_mgroup_all, 4));
				xtime iTime = sqlite3_column_int64(stmt_mgroup_all, 5);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				iTime = sqlite3_column_int64(stmt_mgroup_all, 6);
				ValueSetOwnedText(tblRow, "updateTime", TimeText(iTime, TIME_TEXT_DATETIME));
				sqlite3_bind_int64(stmt_mgroup_sum, 1, rowId);
				if ( sqlite3_step(stmt_mgroup_sum) == SQLITE_ROW ) ValueSetInt(tblRow, "userCount", sqlite3_column_int64(stmt_mgroup_sum, 0));
				sqlite3_reset(stmt_mgroup_sum);
				if ( iCount <= 0 ) iCount = sqlite3_column_int64(stmt_mgroup_all, 7);
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_mgroup_all);
		} else {
			// 筛选查询
			sqlite3_bind_text(stmt_mgroup_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_mgroup_sel, 2, iLimit);
			sqlite3_bind_int64(stmt_mgroup_sel, 3, iOffset);
			while ( sqlite3_step(stmt_mgroup_sel) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject(); int64 rowId = sqlite3_column_int64(stmt_mgroup_sel, 0);
				ValueSetInt(tblRow, "id", rowId);
				ValueSetText(tblRow, "name", (str)sqlite3_column_text(stmt_mgroup_sel, 1));
				ValueSetText(tblRow, "desc", (str)sqlite3_column_text(stmt_mgroup_sel, 2));
				// 解析 authList 获取权限数量
				str authList = (str)sqlite3_column_text(stmt_mgroup_sel, 3);
				int64 authCount = 0;
				if ( authList && strlen(authList) > 2 ) {
					xvalue* arrAuth = JsonParseN(authList, strlen(authList));
					if ( (arrAuth) && (xrtValueType(arrAuth) == XVALUE_ARRAY) ) authCount = ValueCount(arrAuth);
					if ( arrAuth ) xrtValueRelease(arrAuth);
				}
				ValueSetInt(tblRow, "authCount", authCount);
				ValueSetInt(tblRow, "authLevel", sqlite3_column_int64(stmt_mgroup_sel, 4));
				xtime iTime = sqlite3_column_int64(stmt_mgroup_sel, 5);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				iTime = sqlite3_column_int64(stmt_mgroup_sel, 6);
				ValueSetOwnedText(tblRow, "updateTime", TimeText(iTime, TIME_TEXT_DATETIME));
				sqlite3_bind_int64(stmt_mgroup_sum, 1, rowId);
				if ( sqlite3_step(stmt_mgroup_sum) == SQLITE_ROW ) ValueSetInt(tblRow, "userCount", sqlite3_column_int64(stmt_mgroup_sum, 0));
				sqlite3_reset(stmt_mgroup_sum);
				if ( iCount <= 0 ) iCount = sqlite3_column_int64(stmt_mgroup_sel, 7);
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_mgroup_sel);
		}
		
		xvalue* tblRet = ValueObject();
		ValueSetBool(tblRet, "result", true);
		ValueSetInt(tblRet, "code", 0);
		ValueSetInt(tblRet, "count", iCount);
		ValueSetOwn(tblRet, "data", data);
		size_t iRetSize = 0;
		char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet); xrtValueRelease(tblRet);
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xrtValueRelease(tblForm); return; }
		str name = ValueText(tblForm, "name");
		str desc = ValueText(tblForm, "desc");
		str authList = ValueText(tblForm, "authList");
		int64 authLevel = ValueInt(tblForm, "authLevel");
		if ( !authList || strlen(authList) == 0 ) authList = "[]";
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( name && strlen(name) > 64 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"名称最多64个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		if ( desc && strlen(desc) > 1024 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"描述最多1024个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		if ( strlen(authList) > 4096 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"权限列表最多4096个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		xtime now = xrtNow();
		sqlite3_bind_text(stmt_mgroup_add, 1, name, -1, NULL);
		sqlite3_bind_text(stmt_mgroup_add, 2, desc ? desc : (str)"", -1, NULL);
		sqlite3_bind_text(stmt_mgroup_add, 3, authList, -1, NULL);
		sqlite3_bind_int64(stmt_mgroup_add, 4, authLevel);
		sqlite3_bind_int64(stmt_mgroup_add, 5, now);
		sqlite3_bind_int64(stmt_mgroup_add, 6, now);
		bool written = DB_Write(stmt_mgroup_add, true);
		int64 newId = written ? sqlite3_last_insert_rowid(G_DB) : 0;
		xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"用户组添加成功！\", \"data\": {\"id\": %lld}}", newId);
		MemberAuth_ReloadCache();
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_PUT) ) {
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xrtValueRelease(tblForm); return; }
		int64 id = ValueInt(tblForm, "id");
		str name = ValueText(tblForm, "name");
		str desc = ValueText(tblForm, "desc");
		str authList = ValueText(tblForm, "authList");
		int64 authLevel = ValueInt(tblForm, "authLevel");
		if ( !authList || strlen(authList) == 0 ) authList = "[]";
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( name && strlen(name) > 64 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"名称最多64个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		if ( desc && strlen(desc) > 1024 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"描述最多1024个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		if ( strlen(authList) > 4096 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"权限列表最多4096个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		xtime now = xrtNow();
		sqlite3_bind_text(stmt_mgroup_put, 1, name, -1, NULL);
		sqlite3_bind_text(stmt_mgroup_put, 2, desc ? desc : (str)"", -1, NULL);
		sqlite3_bind_text(stmt_mgroup_put, 3, authList, -1, NULL);
		sqlite3_bind_int64(stmt_mgroup_put, 4, authLevel);
		sqlite3_bind_int64(stmt_mgroup_put, 5, now);
		sqlite3_bind_int64(stmt_mgroup_put, 6, id);
		bool written = DB_Write(stmt_mgroup_put, true);
		xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"用户组更新成功！\"}", 0);
		MemberAuth_ReloadCache();
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_DELETE) ) {
		char sID[24];
		xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		int64 id = Util_ParseI64(sID);
		if ( id > 1 ) {
			sqlite3_bind_int64(stmt_mgroup_sum, 1, id);
			int64 userCount = 0;
			if ( sqlite3_step(stmt_mgroup_sum) == SQLITE_ROW ) userCount = sqlite3_column_int64(stmt_mgroup_sum, 0);
			sqlite3_reset(stmt_mgroup_sum);
			if ( userCount > 0 ) {
				xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无法删除此用户组，因为它关联了用户！\"}", 0);
			} else {
				xtime now = xrtNow();
				sqlite3_bind_int64(stmt_mgroup_del, 1, now);
				sqlite3_bind_int64(stmt_mgroup_del, 2, id);
				bool written = DB_Write(stmt_mgroup_del, true);
				if (ReplyIfWriteFailed(objResp, written)) return;
				xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"用户组删除成功！\"}", 0);
				MemberAuth_ReloadCache();
			}
		} else if ( id == 1 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"不能删除默认用户组！\"}", 0); }
		else { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的用户组ID\"}", 0); }
	} else { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); }
}



// ==================== 前台权限分类管理 ====================

void Request_View_Member_AuthGroup(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{ if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) LoadPage(objResp, 200, HTTP_CT_HTML, "member/authgroup.html"); else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); }

void Request_View_Member_AuthGroup_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{ if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) LoadPage(objResp, 200, HTTP_CT_HTML, "member/authgroup_add.html"); else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); }

void Request_View_Member_AuthGroup_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		char sID[24]; xsReqQueryValue(objReq, "id", sID, sizeof(sID)); int64 id = Util_ParseI64(sID);
		xvalue* tblInfo = ValueObject();
		int bRow = false;
		sqlite3_bind_int64(stmt_magroup_get, 1, id);
		if ( sqlite3_step(stmt_magroup_get) == SQLITE_ROW ) {
			bRow = true;
			ValueSetInt(tblInfo, "id", id);
			ValueSetText(tblInfo, "name", (str)sqlite3_column_text(stmt_magroup_get, 1));
			ValueSetText(tblInfo, "desc", (str)sqlite3_column_text(stmt_magroup_get, 2));
			ValueSetInt(tblInfo, "sort", sqlite3_column_int64(stmt_magroup_get, 3));
		}
		sqlite3_reset(stmt_magroup_get);
		if ( !bRow ) {
			xrtValueRelease(tblInfo);
			LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
			return;
		}
		size_t iSize = 0; str sPage = MakePageWithTemplate("member/authgroup_edit.html", tblInfo, &iSize);
		xrtValueRelease(tblInfo); xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iSize); xrtFree(sPage);
	} else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

void Request_Member_AuthGroup(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		char sParam[64]; xsReqQueryValue(objReq, "page", sParam, sizeof(sParam)); int64 iPage = Util_ParseI64(sParam); if ( iPage <= 0 ) iPage = 1;
		xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam)); int64 iLimit = Util_ParseI64(sParam); if ( iLimit <= 0 ) iLimit = 10;
		if ( iLimit > 100 ) iLimit = 100;	/* F9 */
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = xsReqQueryValue(objReq, "search", sParam, sizeof(sParam));
		
		xvalue* data = ValueArray(); int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_int64(stmt_magroup_all, 1, iLimit); sqlite3_bind_int64(stmt_magroup_all, 2, iOffset);
			while ( sqlite3_step(stmt_magroup_all) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject(); int64 rowId = sqlite3_column_int64(stmt_magroup_all, 0);
				ValueSetInt(tblRow, "id", rowId);
				ValueSetText(tblRow, "name", (str)sqlite3_column_text(stmt_magroup_all, 1));
				ValueSetText(tblRow, "desc", (str)sqlite3_column_text(stmt_magroup_all, 2));
				ValueSetInt(tblRow, "sort", sqlite3_column_int64(stmt_magroup_all, 3));
				xtime iTime = sqlite3_column_int64(stmt_magroup_all, 4);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				iTime = sqlite3_column_int64(stmt_magroup_all, 5);
				ValueSetOwnedText(tblRow, "updateTime", TimeText(iTime, TIME_TEXT_DATETIME));
				sqlite3_bind_int64(stmt_magroup_sum, 1, rowId);
				if ( sqlite3_step(stmt_magroup_sum) == SQLITE_ROW ) ValueSetInt(tblRow, "authCount", sqlite3_column_int64(stmt_magroup_sum, 0));
				sqlite3_reset(stmt_magroup_sum);
				if ( iCount <= 0 ) iCount = sqlite3_column_int64(stmt_magroup_all, 6);
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_magroup_all);
		} else {
			// 筛选查询
			sqlite3_bind_text(stmt_magroup_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_magroup_sel, 2, iLimit); sqlite3_bind_int64(stmt_magroup_sel, 3, iOffset);
			while ( sqlite3_step(stmt_magroup_sel) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject(); int64 rowId = sqlite3_column_int64(stmt_magroup_sel, 0);
				ValueSetInt(tblRow, "id", rowId);
				ValueSetText(tblRow, "name", (str)sqlite3_column_text(stmt_magroup_sel, 1));
				ValueSetText(tblRow, "desc", (str)sqlite3_column_text(stmt_magroup_sel, 2));
				ValueSetInt(tblRow, "sort", sqlite3_column_int64(stmt_magroup_sel, 3));
				xtime iTime = sqlite3_column_int64(stmt_magroup_sel, 4);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				iTime = sqlite3_column_int64(stmt_magroup_sel, 5);
				ValueSetOwnedText(tblRow, "updateTime", TimeText(iTime, TIME_TEXT_DATETIME));
				sqlite3_bind_int64(stmt_magroup_sum, 1, rowId);
				if ( sqlite3_step(stmt_magroup_sum) == SQLITE_ROW ) ValueSetInt(tblRow, "authCount", sqlite3_column_int64(stmt_magroup_sum, 0));
				sqlite3_reset(stmt_magroup_sum);
				if ( iCount <= 0 ) iCount = sqlite3_column_int64(stmt_magroup_sel, 6);
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_magroup_sel);
		}
		
		xvalue* tblRet = ValueObject(); ValueSetBool(tblRet, "result", true); ValueSetInt(tblRet, "code", 0);
		ValueSetInt(tblRet, "count", iCount); ValueSetOwn(tblRet, "data", data);
		size_t iRetSize = 0; char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize); xrtFree(sRet); xrtValueRelease(tblRet);
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xrtValueRelease(tblForm); return; }
		str name = ValueText(tblForm, "name"); str desc = ValueText(tblForm, "desc"); int64 sort = ValueInt(tblForm, "sort");
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( name && strlen(name) > 64 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"名称最多64个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		if ( desc && strlen(desc) > 1024 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"描述最多1024个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		xtime now = xrtNow(); sqlite3_bind_text(stmt_magroup_add, 1, name, -1, NULL); sqlite3_bind_text(stmt_magroup_add, 2, desc ? desc : (str)"", -1, NULL);
		sqlite3_bind_int64(stmt_magroup_add, 3, sort); sqlite3_bind_int64(stmt_magroup_add, 4, now); sqlite3_bind_int64(stmt_magroup_add, 5, now);
		bool written = DB_Write(stmt_magroup_add, true);
		int64 newId = written ? sqlite3_last_insert_rowid(G_DB) : 0;
		xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分类添加成功！\", \"data\": {\"id\": %lld}}", newId);
		ReloadCache_MemberAuthGroup();
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_PUT) ) {
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xrtValueRelease(tblForm); return; }
		int64 id = ValueInt(tblForm, "id"); str name = ValueText(tblForm, "name"); str desc = ValueText(tblForm, "desc"); int64 sort = ValueInt(tblForm, "sort");
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( name && strlen(name) > 64 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"名称最多64个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		if ( desc && strlen(desc) > 1024 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"描述最多1024个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		xtime now = xrtNow(); sqlite3_bind_text(stmt_magroup_put, 1, name, -1, NULL); sqlite3_bind_text(stmt_magroup_put, 2, desc ? desc : (str)"", -1, NULL);
		sqlite3_bind_int64(stmt_magroup_put, 3, sort); sqlite3_bind_int64(stmt_magroup_put, 4, now); sqlite3_bind_int64(stmt_magroup_put, 5, id);
		bool written = DB_Write(stmt_magroup_put, true);
		xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分类更新成功！\"}", 0); ReloadCache_MemberAuthGroup();
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_DELETE) ) {
		char sID[24]; xsReqQueryValue(objReq, "id", sID, sizeof(sID)); int64 id = Util_ParseI64(sID);
		if ( id > 1 ) {
			xtime now = xrtNow();
			sqlite3_bind_int64(stmt_magroup_mov, 1, now);
			sqlite3_bind_int64(stmt_magroup_mov, 2, id);
			sqlite3_bind_int64(stmt_magroup_del, 1, now);
			sqlite3_bind_int64(stmt_magroup_del, 2, id);
			bool written = DB_MoveAndDelete(stmt_magroup_mov, stmt_magroup_del);
			if (ReplyIfWriteFailed(objResp, written)) return;
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分类删除成功！\"}", 0); ReloadCache_MemberAuthGroup();
		} else if ( id == 1 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"不能删除默认权限分类！\"}", 0); }
		else { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的权限分类ID\"}", 0); }
	} else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}



// ==================== 前台权限分组管理 ====================

void Request_View_Member_Auth(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{ if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) LoadPage(objResp, 200, HTTP_CT_HTML, "member/auth.html"); else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); }

void Request_View_Member_Auth_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		xvalue* tblInfo = ValueObject(); ValueSetRef(tblInfo, "groupList", G_CACHE_MemberAuthGroup);
		size_t iSize = 0; str sPage = MakePageWithTemplate("member/auth_add.html", tblInfo, &iSize);
		xrtValueRelease(tblInfo); xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iSize); xrtFree(sPage);
	} else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

void Request_View_Member_Auth_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		char sID[24]; xsReqQueryValue(objReq, "id", sID, sizeof(sID)); int64 id = Util_ParseI64(sID);
		xvalue* tblInfo = ValueObject();
		int bRow = false;
		sqlite3_bind_int64(stmt_mauth_get, 1, id);
		if ( sqlite3_step(stmt_mauth_get) == SQLITE_ROW ) {
			bRow = true;
			ValueSetInt(tblInfo, "id", id);
			ValueSetInt(tblInfo, "groupID", sqlite3_column_int64(stmt_mauth_get, 1));
			ValueSetText(tblInfo, "name", (str)sqlite3_column_text(stmt_mauth_get, 2));
			ValueSetText(tblInfo, "desc", (str)sqlite3_column_text(stmt_mauth_get, 3));
			ValueSetInt(tblInfo, "sort", sqlite3_column_int64(stmt_mauth_get, 4));
		}
		sqlite3_reset(stmt_mauth_get);
		if ( !bRow ) {
			xrtValueRelease(tblInfo);
			LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
			return;
		}
		ValueSetRef(tblInfo, "groupList", G_CACHE_MemberAuthGroup);
		size_t iSize = 0; str sPage = MakePageWithTemplate("member/auth_edit.html", tblInfo, &iSize);
		xrtValueRelease(tblInfo); xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iSize); xrtFree(sPage);
	} else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

void Request_Member_Auth(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		char sParam[64]; xsReqQueryValue(objReq, "page", sParam, sizeof(sParam)); int64 iPage = Util_ParseI64(sParam); if ( iPage <= 0 ) iPage = 1;
		xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam)); int64 iLimit = Util_ParseI64(sParam); if ( iLimit <= 0 ) iLimit = 10;
		if ( iLimit > 100 ) iLimit = 100;	/* F9 */
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = xsReqQueryValue(objReq, "search", sParam, sizeof(sParam));
		
		xvalue* data = ValueArray(); int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_int64(stmt_mauth_all, 1, iLimit); sqlite3_bind_int64(stmt_mauth_all, 2, iOffset);
			while ( sqlite3_step(stmt_mauth_all) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject();
				ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_mauth_all, 0));
				ValueSetInt(tblRow, "groupID", sqlite3_column_int64(stmt_mauth_all, 1));
				ValueSetText(tblRow, "name", (str)sqlite3_column_text(stmt_mauth_all, 2));
				ValueSetText(tblRow, "desc", (str)sqlite3_column_text(stmt_mauth_all, 3));
				ValueSetInt(tblRow, "sort", sqlite3_column_int64(stmt_mauth_all, 4));
				xtime iTime = sqlite3_column_int64(stmt_mauth_all, 5);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				iTime = sqlite3_column_int64(stmt_mauth_all, 6);
				ValueSetOwnedText(tblRow, "updateTime", TimeText(iTime, TIME_TEXT_DATETIME));
				ValueSetText(tblRow, "groupName", (str)sqlite3_column_text(stmt_mauth_all, 7));
				if ( iCount <= 0 ) iCount = sqlite3_column_int64(stmt_mauth_all, 8);
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_mauth_all);
		} else {
			// 筛选查询
			sqlite3_bind_text(stmt_mauth_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_mauth_sel, 2, iLimit); sqlite3_bind_int64(stmt_mauth_sel, 3, iOffset);
			while ( sqlite3_step(stmt_mauth_sel) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject();
				ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_mauth_sel, 0));
				ValueSetInt(tblRow, "groupID", sqlite3_column_int64(stmt_mauth_sel, 1));
				ValueSetText(tblRow, "name", (str)sqlite3_column_text(stmt_mauth_sel, 2));
				ValueSetText(tblRow, "desc", (str)sqlite3_column_text(stmt_mauth_sel, 3));
				ValueSetInt(tblRow, "sort", sqlite3_column_int64(stmt_mauth_sel, 4));
				xtime iTime = sqlite3_column_int64(stmt_mauth_sel, 5);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				iTime = sqlite3_column_int64(stmt_mauth_sel, 6);
				ValueSetOwnedText(tblRow, "updateTime", TimeText(iTime, TIME_TEXT_DATETIME));
				ValueSetText(tblRow, "groupName", (str)sqlite3_column_text(stmt_mauth_sel, 7));
				if ( iCount <= 0 ) iCount = sqlite3_column_int64(stmt_mauth_sel, 8);
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_mauth_sel);
		}
		
		xvalue* tblRet = ValueObject(); ValueSetBool(tblRet, "result", true); ValueSetInt(tblRet, "code", 0);
		ValueSetInt(tblRet, "count", iCount); ValueSetOwn(tblRet, "data", data);
		size_t iRetSize = 0; char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize); xrtFree(sRet); xrtValueRelease(tblRet);
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xrtValueRelease(tblForm); return; }
		int64 groupID = ValueInt(tblForm, "groupID"); str name = ValueText(tblForm, "name");
		str desc = ValueText(tblForm, "desc"); int64 sort = ValueInt(tblForm, "sort");
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( name && strlen(name) > 64 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"名称最多64个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		if ( desc && strlen(desc) > 1024 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"描述最多1024个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		xtime now = xrtNow(); sqlite3_bind_int64(stmt_mauth_add, 1, groupID); sqlite3_bind_text(stmt_mauth_add, 2, name, -1, NULL);
		sqlite3_bind_text(stmt_mauth_add, 3, desc ? desc : (str)"", -1, NULL); sqlite3_bind_int64(stmt_mauth_add, 4, sort);
		sqlite3_bind_int64(stmt_mauth_add, 5, now); sqlite3_bind_int64(stmt_mauth_add, 6, now);
		bool written = DB_Write(stmt_mauth_add, true);
		int64 newId = written ? sqlite3_last_insert_rowid(G_DB) : 0;
		xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分组添加成功！\", \"data\": {\"id\": %lld}}", newId);
		ReloadCache_MemberAuth(); ReloadCache_MemberAuthGroup(); MemberAuth_ReloadCache();
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_PUT) ) {
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xrtValueRelease(tblForm); return; }
		int64 id = ValueInt(tblForm, "id"); int64 groupID = ValueInt(tblForm, "groupID");
		str name = ValueText(tblForm, "name"); str desc = ValueText(tblForm, "desc"); int64 sort = ValueInt(tblForm, "sort");
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( name && strlen(name) > 64 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"名称最多64个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		if ( desc && strlen(desc) > 1024 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"描述最多1024个字符！\"}", 0); xrtValueRelease(tblForm); return; }
		xtime now = xrtNow(); sqlite3_bind_int64(stmt_mauth_put, 1, groupID); sqlite3_bind_text(stmt_mauth_put, 2, name, -1, NULL);
		sqlite3_bind_text(stmt_mauth_put, 3, desc ? desc : (str)"", -1, NULL); sqlite3_bind_int64(stmt_mauth_put, 4, sort);
		sqlite3_bind_int64(stmt_mauth_put, 5, now); sqlite3_bind_int64(stmt_mauth_put, 6, id);
		bool written = DB_Write(stmt_mauth_put, true);
		xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分组更新成功！\"}", 0);
		ReloadCache_MemberAuth(); ReloadCache_MemberAuthGroup(); MemberAuth_ReloadCache();
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_DELETE) ) {
		char sID[24]; xsReqQueryValue(objReq, "id", sID, sizeof(sID)); int64 id = Util_ParseI64(sID);
		if ( id > 1 ) {
			xtime now = xrtNow();
			sqlite3_bind_int64(stmt_mauth_mov, 1, now);
			sqlite3_bind_int64(stmt_mauth_mov, 2, id);
			sqlite3_bind_int64(stmt_mauth_del, 1, now);
			sqlite3_bind_int64(stmt_mauth_del, 2, id);
			bool written = DB_MoveAndDelete(stmt_mauth_mov, stmt_mauth_del);
			if (ReplyIfWriteFailed(objResp, written)) return;
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分组删除成功！\"}", 0);
			ReloadCache_MemberAuth(); ReloadCache_MemberAuthGroup(); MemberAuth_ReloadCache();
		} else if ( id == 1 ) { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"不能删除默认权限分组！\"}", 0); }
		else { xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的权限分组ID\"}", 0); }
	} else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}



