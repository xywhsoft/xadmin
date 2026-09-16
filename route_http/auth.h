


// 获取 user 管理页面视图
void Request_View_Auth_User(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 登录页面
		LoadPage(objResp, 200, HTTP_CT_HTML, "auth/user.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 user 添加页面视图
void Request_View_Auth_User_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 构建模板数据 - 角色列表
		xvalue* tblInfo = ValueObject();
		ValueSetRef(tblInfo, "roleList", G_CACHE_Role);
		
		// 构建页面并返回
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("auth/user_add.html", tblInfo, &iSize);
		xrtValueRelease(tblInfo);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 user 编辑页面视图
void Request_View_Auth_User_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 获取参数
		char sID[24];
		xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		int64 id = Util_ParseI64(sID);
		
		// 读取数据库中的记录，构建模板数据
		xvalue* tblInfo = ValueObject();
		int bRow = false;
		sqlite3_bind_int64(stmt_user_get, 1, id);
		while ( sqlite3_step(stmt_user_get) == SQLITE_ROW ) {
			ValueSetInt(tblInfo, "id", id);
			ValueSetInt(tblInfo, "role", sqlite3_column_int64(stmt_user_get, 4));
			ValueSetInt(tblInfo, "authLevel", sqlite3_column_int64(stmt_user_get, 5));
			ValueSetText(tblInfo, "user", (str)sqlite3_column_text(stmt_user_get, 1));
			bRow = true;
		}
		sqlite3_reset(stmt_user_get);
		if ( !bRow ) {
			xrtValueRelease(tblInfo);
			LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
			return;
		}
		
		// 添加角色列表
		ValueSetRef(tblInfo, "roleList", G_CACHE_Role);
		
		// 构建页面并返回
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("auth/user_edit.html", tblInfo, &iSize);
		xrtValueRelease(tblInfo);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// user 主接口
void Request_Auth_User(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 从 URL 查询字符串中提取参数
		char sParam[64];
		xsReqQueryValue(objReq, "page", sParam, sizeof(sParam));
		int64 iPage = Util_ParseI64(sParam);
		if ( iPage <= 0 ) { iPage = 1; }
		xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam));
		int64 iLimit = Util_ParseI64(sParam);
		if ( iLimit <= 0 ) { iLimit = 10; }
		if ( iLimit > 100 ) { iLimit = 100; }	/* F9 */
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = xsReqQueryValue(objReq, "search", sParam, sizeof(sParam));
		
		// 从数据库中查询数据
		xvalue* data = ValueArray();
		int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_int64(stmt_user_all, 1, iLimit);
			sqlite3_bind_int64(stmt_user_all, 2, iOffset);
			while ( sqlite3_step(stmt_user_all) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject();
				ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_user_all, 0));
				ValueSetInt(tblRow, "role", sqlite3_column_int64(stmt_user_all, 2));
				ValueSetInt(tblRow, "authLevel", sqlite3_column_int64(stmt_user_all, 3));
				ValueSetText(tblRow, "user", (str)sqlite3_column_text(stmt_user_all, 1));
				// 不返回密码字段
				xtime iTime = sqlite3_column_int64(stmt_user_all, 4);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				iTime = sqlite3_column_int64(stmt_user_all, 5);
				ValueSetOwnedText(tblRow, "updateTime", TimeText(iTime, TIME_TEXT_DATETIME));
				ValueSetText(tblRow, "roleName", (str)sqlite3_column_text(stmt_user_all, 6));
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_user_all);
			if ( sqlite3_step(stmt_user_count_all) == SQLITE_ROW ) iCount = sqlite3_column_int64(stmt_user_count_all, 0);
			sqlite3_reset(stmt_user_count_all);
		} else {
			// 筛选
			sqlite3_bind_text(stmt_user_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_user_sel, 2, iLimit);
			sqlite3_bind_int64(stmt_user_sel, 3, iOffset);
			while ( sqlite3_step(stmt_user_sel) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject();
				ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_user_sel, 0));
				ValueSetInt(tblRow, "role", sqlite3_column_int64(stmt_user_sel, 2));
				ValueSetInt(tblRow, "authLevel", sqlite3_column_int64(stmt_user_sel, 3));
				ValueSetText(tblRow, "user", (str)sqlite3_column_text(stmt_user_sel, 1));
				// 不返回密码字段
				xtime iTime = sqlite3_column_int64(stmt_user_sel, 4);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				iTime = sqlite3_column_int64(stmt_user_sel, 5);
				ValueSetOwnedText(tblRow, "updateTime", TimeText(iTime, TIME_TEXT_DATETIME));
				ValueSetText(tblRow, "roleName", (str)sqlite3_column_text(stmt_user_sel, 6));
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_user_sel);
			sqlite3_bind_text(stmt_user_count_sel, 1, sParam, -1, NULL);
			if ( sqlite3_step(stmt_user_count_sel) == SQLITE_ROW ) iCount = sqlite3_column_int64(stmt_user_count_sel, 0);
			sqlite3_reset(stmt_user_count_sel);
		}
		
		// 构建返回值
		xvalue* tblRet = ValueObject();
		ValueSetBool(tblRet, "result", true);
		ValueSetInt(tblRet, "code", 0);
		ValueSetInt(tblRet, "count", iCount);
		ValueSetText(tblRet, "message", "用户数据获取成功！");
		ValueSetOwn(tblRet, "data", data);
		
		// 生成 JSON
		size_t iRetSize = 0;
		char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xrtValueRelease(tblRet);
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		
		// 添加用户
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		int64 role = ValueInt(tblForm, "role");
		if ( role < 1 ) {
			role = 1;
		}
		str user = ValueText(tblForm, "username");
		if ( !user || (strlen(user) == 0) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名不能为空！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		str password = ValueText(tblForm, "password");
		if ( !password || (strlen(password) == 0) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"密码不能为空！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( strlen(user) > 64 ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名最多64个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		if ( strlen(password) > 128 ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"密码最多128个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		// 检查用户名是否已存在
		bool bExists = false;
		sqlite3_bind_text(stmt_user_chk, 1, user, strlen(user), SQLITE_STATIC);
		sqlite3_bind_int64(stmt_user_chk, 2, 0);
		while ( sqlite3_step(stmt_user_chk) == SQLITE_ROW ) {
			bExists = true;
			break;
		}
		sqlite3_reset(stmt_user_chk);
		if ( bExists ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名已存在！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		
		// 服务端二次 SHA-256 哈希
		str sSalt = Util_Token();
		str sPwdHash = ServerHashPassword(user, sSalt, password);
		
		// 写入数据库
		xtime now = xrtNow();
		sqlite3_bind_text(stmt_user_add, 1, user, strlen(user), SQLITE_STATIC);
		sqlite3_bind_text(stmt_user_add, 2, sSalt, strlen(sSalt), SQLITE_STATIC);
		sqlite3_bind_text(stmt_user_add, 3, sPwdHash, strlen(sPwdHash), SQLITE_STATIC);
		sqlite3_bind_int64(stmt_user_add, 4, role);
		sqlite3_bind_int64(stmt_user_add, 5, 0); // authLevel 默认为 0
		sqlite3_bind_int64(stmt_user_add, 6, now);
		sqlite3_bind_int64(stmt_user_add, 7, now);
		bool written = DB_Write(stmt_user_add, true);
		int64 newId = written ? sqlite3_last_insert_rowid(G_DB) : 0;
		xrtFree(sSalt);
		xrtFree(sPwdHash);
		xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		
		// 返回成功信息和新创建的ID
		xvalue* tblRet = ValueObject();
		ValueSetBool(tblRet, "result", true);
		ValueSetText(tblRet, "message", "用户添加成功！");
		xvalue* dataRet = ValueObject();
		ValueSetInt(dataRet, "id", newId);
		ValueSetOwn(tblRet, "data", dataRet);
		size_t iRetSize = 0;
		char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xrtValueRelease(tblRet);
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_PUT) ) {
		
		// 更新用户 - 不更新用户名，只更新角色
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		int64 id = ValueInt(tblForm, "id");
		int64 role = ValueInt(tblForm, "role");
		int64 authLevel = ValueInt(tblForm, "authLevel");
		if ( role < 1 ) {
			role = 1;
		}
		/* L7：与 role 族 0..999 约定一致钳位（原可负/任意大并参与登录 max()） */
		if ( authLevel < 0 ) authLevel = 0;
		if ( authLevel > 999 ) authLevel = 999;
								
		// 写入数据库
		xtime now = xrtNow();
		sqlite3_bind_int64(stmt_user_put, 1, role);
		sqlite3_bind_int64(stmt_user_put, 2, authLevel);
		sqlite3_bind_int64(stmt_user_put, 3, now);
		sqlite3_bind_int64(stmt_user_put, 4, id);
		bool written = DB_Write(stmt_user_put, true);
		xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;

		/* 角色/权限级别已变更：撤销该账户既有会话（会话内 roleID 是登录快照，
		 * 缓存重建救不了"换角色"这层——与 repwd/删除同一撤销语义）。 */
		Session_RevokeAccount(true, id);

		// 返回成功信息
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"用户更新成功！\"}", 0);

	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_DELETE) ) {

		// 删除用户（软删除）
		// 从URL查询字符串中提取ID
		char sID[24];
		xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		int64 id = Util_ParseI64(sID);
		if ( id <= 0 ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的用户ID\"}", 0);
			return;
		}
		/* 自锁保护：id=1 为内置超管不可删（role 删除已有同款保护，补齐一致性）。
		 * 自删是既有受测契约（自删安全完成并当场撤销自身会话），不拦。 */
		if ( id == 1 ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"不能删除内置超级管理员！\"}", 0);
			return;
		}
		{
			// 执行软删除
			sqlite3_bind_int64(stmt_user_del, 1, id);
			bool written = DB_Write(stmt_user_del, true);
			if (ReplyIfWriteFailed(objResp, written)) return;
			Session_RevokeAccount(true, id);
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"用户删除成功！\"}", 0);
		}

	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 重置密码接口
void Request_Auth_User_Repwd(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		
		// 重置密码
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		int64 id = ValueInt(tblForm, "id");
		str username = ValueText(tblForm, "username");
		if ( !username || (strlen(username) == 0) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名不能为空！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		str password = ValueText(tblForm, "password");
		if ( !password || (strlen(password) == 0) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"密码不能为空！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( strlen(username) > 64 ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名最多64个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		if ( strlen(password) > 128 ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"密码最多128个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		// 生成新的随机 salt（使用 XID）
		str sSalt = Util_Token();
		
		// 服务端二次 SHA-256 哈希
		str sPwdHash = ServerHashPassword(username, sSalt, password);
		
		// 更新 salt 和密码
		xtime now = xrtNow();
		sqlite3_bind_text(stmt_user_pwd, 1, sSalt, strlen(sSalt), SQLITE_STATIC);
		sqlite3_bind_text(stmt_user_pwd, 2, sPwdHash, strlen(sPwdHash), SQLITE_STATIC);
		sqlite3_bind_int64(stmt_user_pwd, 3, now);
		sqlite3_bind_int64(stmt_user_pwd, 4, id);
		bool written = DB_Write(stmt_user_pwd, true);
		
		xrtFree(sSalt);
		xrtFree(sPwdHash);
		xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		
		// 返回成功信息
		/* R3：重置密码成功后撤销该账号全部会话（管理员代重置，无当前会话需保留）。 */
		Session_RevokeAccount(true, id);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"密码重置成功！\"}", 0);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}





// 获取 role 管理页面视图
void Request_View_Auth_Role(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 角色管理页面
		LoadPage(objResp, 200, HTTP_CT_HTML, "auth/role.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 role 添加页面视图
void Request_View_Auth_Role_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 构建模板数据 - 使用分组缓存
		xvalue* tblInfo = ValueObject();
		ValueSetRef(tblInfo, "authGroups", G_CACHE_Group);
		
		// 构建页面并返回
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("auth/role_add.html", tblInfo, &iSize);
		xrtValueRelease(tblInfo);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 role 编辑页面视图
void Request_View_Auth_Role_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 获取参数
		char sID[24];
		xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		int64 id = Util_ParseI64(sID);
		
		// 读取数据库中的记录，构建模板数据
		xvalue* tblInfo = ValueObject();
		xvalue* listAuth = xrtValueIntMap();
		int bRow = false;
		sqlite3_bind_int64(stmt_role_get, 1, id);
		while ( sqlite3_step(stmt_role_get) == SQLITE_ROW ) {
			ValueSetInt(tblInfo, "id", id);
			ValueSetText(tblInfo, "name", (str)sqlite3_column_text(stmt_role_get, 1));
			ValueSetText(tblInfo, "desc", (str)sqlite3_column_text(stmt_role_get, 2));
			ValueSetInt(tblInfo, "authLevel", sqlite3_column_int64(stmt_role_get, 4));
			// 解析权限分组列表 - 转换为 list 方便按ID索引
			str sAuthList = (str)sqlite3_column_text(stmt_role_get, 3);
			if ( sAuthList && (strlen(sAuthList) > 2) ) {
				xvalue* arrAuth = JsonParseN(sAuthList, 0);
				if ( arrAuth && (xrtValueType(arrAuth) == XVALUE_ARRAY) && (xrtValueCount(arrAuth) > 0) ) {
					for ( int i = 0; i < xrtValueCount(arrAuth); i++ ) {
						int64 authID = ValueArrayInt(arrAuth, i);
						if ( authID > 0 ) {
							ValueMapSetBool(listAuth, authID, true);
						}
					}
				}
				xrtValueRelease(arrAuth);
			}
			bRow = true;
		}
		sqlite3_reset(stmt_role_get);
		if ( !bRow ) {
			xrtValueRelease(tblInfo);
			LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
			return;
		}
		
		// 标记已选中的权限 - 这里创建深拷贝副本，避免多线程写入同步问题
		xvalue* arrAuthGroups = xrtValueDeepClone(G_CACHE_Group);
		for ( int g = 1; g <= xrtValueCount(arrAuthGroups); g++ ) {
			xvalue* tblGroup = xrtValueArrayGet(arrAuthGroups, (g) - 1);
			xvalue* arrAuths = ValueGet(tblGroup, "auths");
			if ( arrAuths && (xrtValueType(arrAuths) == XVALUE_ARRAY) && (xrtValueCount(arrAuths) > 0) ) {
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
		
		// 构建页面并返回
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("auth/role_edit.html", tblInfo, &iSize);
		xrtValueRelease(tblInfo);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// role 主接口
void Request_Auth_Role(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 从 URL 查询字符串中提取参数
		char sParam[64];
		xsReqQueryValue(objReq, "page", sParam, sizeof(sParam));
		int64 iPage = Util_ParseI64(sParam);
		if ( iPage <= 0 ) { iPage = 1; }
		xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam));
		int64 iLimit = Util_ParseI64(sParam);
		if ( iLimit <= 0 ) { iLimit = 10; }
		if ( iLimit > 100 ) { iLimit = 100; }	/* F9 */
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = xsReqQueryValue(objReq, "search", sParam, sizeof(sParam));
		
		// 从数据库中查询数据
		xvalue* data = ValueArray();
		int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_int64(stmt_role_all, 1, iLimit);
			sqlite3_bind_int64(stmt_role_all, 2, iOffset);
			while ( sqlite3_step(stmt_role_all) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject();
				ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_role_all, 0));
				ValueSetText(tblRow, "name", (str)sqlite3_column_text(stmt_role_all, 1));
				ValueSetText(tblRow, "desc", (str)sqlite3_column_text(stmt_role_all, 2));
				ValueSetText(tblRow, "authList", (str)sqlite3_column_text(stmt_role_all, 3));
				ValueSetInt(tblRow, "authLevel", sqlite3_column_int64(stmt_role_all, 4));
				xtime iTime = sqlite3_column_int64(stmt_role_all, 5);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				iTime = sqlite3_column_int64(stmt_role_all, 6);
				ValueSetOwnedText(tblRow, "updateTime", TimeText(iTime, TIME_TEXT_DATETIME));
				ValueSetInt(tblRow, "authCount", sqlite3_column_int64(stmt_role_all, 8));
				ValueSetInt(tblRow, "userCount", sqlite3_column_int64(stmt_role_all, 9));
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_role_all);
			if ( sqlite3_step(stmt_role_count_all) == SQLITE_ROW ) iCount = sqlite3_column_int64(stmt_role_count_all, 0);
			sqlite3_reset(stmt_role_count_all);
		} else {
			// 筛选
			sqlite3_bind_text(stmt_role_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_text(stmt_role_sel, 2, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_role_sel, 3, iLimit);
			sqlite3_bind_int64(stmt_role_sel, 4, iOffset);
			while ( sqlite3_step(stmt_role_sel) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject();
				ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_role_sel, 0));
				ValueSetText(tblRow, "name", (str)sqlite3_column_text(stmt_role_sel, 1));
				ValueSetText(tblRow, "desc", (str)sqlite3_column_text(stmt_role_sel, 2));
				ValueSetText(tblRow, "authList", (str)sqlite3_column_text(stmt_role_sel, 3));
				ValueSetInt(tblRow, "authLevel", sqlite3_column_int64(stmt_role_sel, 4));
				xtime iTime = sqlite3_column_int64(stmt_role_sel, 5);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				iTime = sqlite3_column_int64(stmt_role_sel, 6);
				ValueSetOwnedText(tblRow, "updateTime", TimeText(iTime, TIME_TEXT_DATETIME));
				ValueSetInt(tblRow, "authCount", sqlite3_column_int64(stmt_role_sel, 8));
				ValueSetInt(tblRow, "userCount", sqlite3_column_int64(stmt_role_sel, 9));
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_role_sel);
			sqlite3_bind_text(stmt_role_count_sel, 1, sParam, -1, NULL);
			sqlite3_bind_text(stmt_role_count_sel, 2, sParam, -1, NULL);
			if ( sqlite3_step(stmt_role_count_sel) == SQLITE_ROW ) iCount = sqlite3_column_int64(stmt_role_count_sel, 0);
			sqlite3_reset(stmt_role_count_sel);
		}
		
		// 构建返回值
		xvalue* tblRet = ValueObject();
		ValueSetBool(tblRet, "result", true);
		ValueSetInt(tblRet, "code", 0);
		ValueSetInt(tblRet, "count", iCount);
		ValueSetText(tblRet, "message", "角色数据获取成功！");
		ValueSetOwn(tblRet, "data", data);
		
		// 生成 JSON
		size_t iRetSize = 0;
		char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xrtValueRelease(tblRet);
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		
		// 添加角色
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		str name = ValueText(tblForm, "name");
		str desc = ValueText(tblForm, "desc");
		str authList = ValueText(tblForm, "authList");
		int64 authLevel = ValueInt(tblForm, "authLevel");
		if ( !authList || (strlen(authList) == 0) ) {
			authList = "[]";
		}
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( name && (strlen(name) > 64) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"名称最多64个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		if ( desc && (strlen(desc) > 1024) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"描述最多1024个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		if ( strlen(authList) > 4096 ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"权限列表最多4096个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		
		// 添加数据库记录
		xtime now = xrtNow();
		sqlite3_bind_text(stmt_role_add, 1, name, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt_role_add, 2, desc, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt_role_add, 3, authList, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_role_add, 4, authLevel);
		sqlite3_bind_int64(stmt_role_add, 5, now);
		sqlite3_bind_int64(stmt_role_add, 6, now);
		bool written = DB_Write(stmt_role_add, true);
		int64 newId = written ? sqlite3_last_insert_rowid(G_DB) : 0;
		xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		
		// 返回成功信息和新创建的ID
		xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"角色添加成功！\", \"data\": {\"id\": %lld}}", newId);
		
		// 刷新角色缓存
		Auth_ReloadCache();
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_PUT) ) {
		
		// 更新角色
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		int64 id = ValueInt(tblForm, "id");
		str name = ValueText(tblForm, "name");
		str desc = ValueText(tblForm, "desc");
		str authList = ValueText(tblForm, "authList");
		int64 authLevel = ValueInt(tblForm, "authLevel");
		if ( !authList || (strlen(authList) == 0) ) {
			authList = "[]";
		}
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( name && (strlen(name) > 64) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"名称最多64个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		if ( desc && (strlen(desc) > 1024) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"描述最多1024个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		if ( strlen(authList) > 4096 ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"权限列表最多4096个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		
		// 更新数据库记录
		xtime now = xrtNow();
		sqlite3_bind_text(stmt_role_put, 1, name, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt_role_put, 2, desc, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt_role_put, 3, authList, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_role_put, 4, authLevel);
		sqlite3_bind_int64(stmt_role_put, 5, now);
		sqlite3_bind_int64(stmt_role_put, 6, id);
		bool written = DB_Write(stmt_role_put, true);
		xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		
		// 返回成功信息
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"角色更新成功！\"}", 0);
		
		// 刷新角色缓存
		Auth_ReloadCache();

		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_DELETE) ) {
		
		// 删除角色（软删除）
		char sID[24];
		xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		int64 id = Util_ParseI64(sID);
		if ( id > 1 ) {
			
			// 检查是否有关联的用户
			int64 iCount = 0;
			sqlite3_bind_int64(stmt_role_sum, 1, id);
			while ( sqlite3_step(stmt_role_sum) == SQLITE_ROW ) {
				iCount = sqlite3_column_int64(stmt_role_sum, 0);
			}
			sqlite3_reset(stmt_role_sum);
			if ( iCount > 0 ) {
				// 有关联的用户，不能删除
				xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无法删除此角色，因为它关联了用户！\"}", 0);
			} else {
				// 没有关联的用户，可以删除
				sqlite3_bind_int64(stmt_role_del, 1, id);
				bool written = DB_Write(stmt_role_del, true);
				if (ReplyIfWriteFailed(objResp, written)) return;
				xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"角色删除成功！\"}", 0);
			}
			
			// 刷新角色缓存
			Auth_ReloadCache();
			
		} else if ( id == 1 ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"不能删除默认角色！\"}", 0);
		} else {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的角色ID\"}", 0);
		}
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}





// 获取 group 管理页面视图
void Request_View_Auth_Group(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 登录页面
		LoadPage(objResp, 200, HTTP_CT_HTML, "auth/group.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 authGroup 添加页面视图
void Request_View_Auth_Group_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		LoadPage(objResp, 200, HTTP_CT_HTML, "auth/group_add.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 authGroup 编辑页面视图
void Request_View_Auth_Group_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 获取参数
		char sID[24];
		xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		int64 id = Util_ParseI64(sID);
		
		// 读取数据库中的记录，构建模板数据
		xvalue* tblInfo = ValueObject();
		int bRow = false;
		sqlite3_bind_int64(stmt_group_get, 1, id);
		while ( sqlite3_step(stmt_group_get) == SQLITE_ROW ) {
			ValueSetInt(tblInfo, "id", id);
			ValueSetText(tblInfo, "name", (str)sqlite3_column_text(stmt_group_get, 1));
			ValueSetText(tblInfo, "desc", (str)sqlite3_column_text(stmt_group_get, 2));
			ValueSetInt(tblInfo, "sort", sqlite3_column_int64(stmt_group_get, 3));
			bRow = true;
		}
		sqlite3_reset(stmt_group_get);
		if ( !bRow ) {
			xrtValueRelease(tblInfo);
			LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
			return;
		}
		
		// 构建页面并返回
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("auth/group_edit.html", tblInfo, &iSize);
		xrtValueRelease(tblInfo);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// group 主接口
void Request_Auth_Group(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 从 URL 查询字符串中提取参数
		char sParam[64];
		xsReqQueryValue(objReq, "page", sParam, sizeof(sParam));
		int64 iPage = Util_ParseI64(sParam);
		if ( iPage <= 0 ) { iPage = 1; }
		xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam));
		int64 iLimit = Util_ParseI64(sParam);
		if ( iLimit <= 0 ) { iLimit = 10; }
		if ( iLimit > 100 ) { iLimit = 100; }	/* F9 */
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = xsReqQueryValue(objReq, "search", sParam, sizeof(sParam));
		
		// 从数据库中查询数据
		xvalue* data = ValueArray();
		int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_int64(stmt_group_all, 1, iLimit);
			sqlite3_bind_int64(stmt_group_all, 2, iOffset);
			while ( sqlite3_step(stmt_group_all) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject();
				ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_group_all, 0));
				ValueSetText(tblRow, "name", (str)sqlite3_column_text(stmt_group_all, 1));
				ValueSetText(tblRow, "desc", (str)sqlite3_column_text(stmt_group_all, 2));
				ValueSetInt(tblRow, "sort", sqlite3_column_int64(stmt_group_all, 3));
				xtime iTime = sqlite3_column_int64(stmt_group_all, 4);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				iTime = sqlite3_column_int64(stmt_group_all, 5);
				ValueSetOwnedText(tblRow, "updateTime", TimeText(iTime, TIME_TEXT_DATETIME));
				ValueSetInt(tblRow, "authCount", sqlite3_column_int64(stmt_group_all, 7));
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_group_all);
			if ( sqlite3_step(stmt_group_count_all) == SQLITE_ROW ) iCount = sqlite3_column_int64(stmt_group_count_all, 0);
			sqlite3_reset(stmt_group_count_all);
		} else {
			// 筛选
			sqlite3_bind_text(stmt_group_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_text(stmt_group_sel, 2, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_group_sel, 3, iLimit);
			sqlite3_bind_int64(stmt_group_sel, 4, iOffset);
			while ( sqlite3_step(stmt_group_sel) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject();
				ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_group_sel, 0));
				ValueSetText(tblRow, "name", (str)sqlite3_column_text(stmt_group_sel, 1));
				ValueSetText(tblRow, "desc", (str)sqlite3_column_text(stmt_group_sel, 2));
				ValueSetInt(tblRow, "sort", sqlite3_column_int64(stmt_group_sel, 3));
				xtime iTime = sqlite3_column_int64(stmt_group_sel, 4);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				iTime = sqlite3_column_int64(stmt_group_sel, 5);
				ValueSetOwnedText(tblRow, "updateTime", TimeText(iTime, TIME_TEXT_DATETIME));
				ValueSetInt(tblRow, "authCount", sqlite3_column_int64(stmt_group_sel, 7));
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_group_sel);
			sqlite3_bind_text(stmt_group_count_sel, 1, sParam, -1, NULL);
			sqlite3_bind_text(stmt_group_count_sel, 2, sParam, -1, NULL);
			if ( sqlite3_step(stmt_group_count_sel) == SQLITE_ROW ) iCount = sqlite3_column_int64(stmt_group_count_sel, 0);
			sqlite3_reset(stmt_group_count_sel);
		}
		
		// 构建返回值
		xvalue* tblRet = ValueObject();
		ValueSetBool(tblRet, "result", true);
		ValueSetInt(tblRet, "code", 0);
		ValueSetInt(tblRet, "count", iCount);
		ValueSetText(tblRet, "message", "权限分类数据获取成功！");
		ValueSetOwn(tblRet, "data", data);
		
		// 生成 JSON
		size_t iRetSize = 0;
		char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xrtValueRelease(tblRet);
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		
		// 添加权限分类
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		str name = ValueText(tblForm, "name");
		str desc = ValueText(tblForm, "desc");
		int64 sort = ValueInt(tblForm, "sort");
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( name && (strlen(name) > 64) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"名称最多64个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		if ( desc && (strlen(desc) > 1024) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"描述最多1024个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		xtime now = xrtNow();
		
		sqlite3_bind_text(stmt_group_add, 1, name, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt_group_add, 2, desc, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_group_add, 3, sort);
		sqlite3_bind_int64(stmt_group_add, 4, now);
		sqlite3_bind_int64(stmt_group_add, 5, now);
		bool written = DB_Write(stmt_group_add, true);
		int64 newId = written ? sqlite3_last_insert_rowid(G_DB) : 0;
		xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		
		// 返回成功信息和新创建的ID
		xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分类添加成功！\", \"data\": {\"id\": %lld}}", newId);
		
		// 刷新权限分类缓存
		ReloadCache_Auth_Group();
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_PUT) ) {
		
		// 更新权限分类
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		int64 id = ValueInt(tblForm, "id");
		str name = ValueText(tblForm, "name");
		str desc = ValueText(tblForm, "desc");
		int64 sort = ValueInt(tblForm, "sort");
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( name && (strlen(name) > 64) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"名称最多64个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		if ( desc && (strlen(desc) > 1024) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"描述最多1024个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		xtime now = xrtNow();
		
		sqlite3_bind_text(stmt_group_put, 1, name, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt_group_put, 2, desc, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_group_put, 3, sort);
		sqlite3_bind_int64(stmt_group_put, 4, now);
		sqlite3_bind_int64(stmt_group_put, 5, id);
		bool written = DB_Write(stmt_group_put, true);
		xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		
		// 返回成功信息
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分类更新成功！\"}", 0);
		
		// 刷新权限分类缓存
		ReloadCache_Auth_Group();
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_DELETE) ) {
		
		// 删除权限分类（软删除）
		char sID[24];
		xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		int64 id = Util_ParseI64(sID);
		if ( id > 1 ) {
			
			// 有关联的URI权限，将他们移入未分组
			sqlite3_bind_int64(stmt_group_mov, 1, id);
			sqlite3_bind_int64(stmt_group_del, 1, id);
			bool written = DB_MoveAndDelete(stmt_group_mov, stmt_group_del);
			if (ReplyIfWriteFailed(objResp, written)) return;
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分类删除成功！\"}", 0);
			
			// 刷新权限分类缓存
			ReloadCache_Auth_Group();
			
		} else if ( id == 1 ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"不能删除默认权限分类！\"}", 0);
		} else {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的权限分类ID\"}", 0);
		}
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}





// 获取 auth 管理页面视图
void Request_View_Auth_Auth(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 登录页面
		LoadPage(objResp, 200, HTTP_CT_HTML, "auth/auth.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 auth 添加页面视图
void Request_View_Auth_Auth_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 构建模板数据
		xvalue* tblInfo = ValueObject();
		ValueSetRef(tblInfo, "groupList", G_CACHE_Group);
		
		// 构建页面并返回
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("auth/auth_add.html", tblInfo, &iSize);
		xrtValueRelease(tblInfo);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 auth 编辑页面视图
void Request_View_Auth_Auth_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 获取参数
		char sID[24];
		xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		int64 id = Util_ParseI64(sID);
		
		// 读取数据库中的记录，构建模板数据
		xvalue* tblInfo = ValueObject();
		int bRow = false;
		sqlite3_bind_int64(stmt_auth_get, 1, id);
		while ( sqlite3_step(stmt_auth_get) == SQLITE_ROW ) {
			ValueSetInt(tblInfo, "id", id);
			ValueSetInt(tblInfo, "groupID", sqlite3_column_int64(stmt_auth_get, 1));
			ValueSetText(tblInfo, "name", (str)sqlite3_column_text(stmt_auth_get, 2));
			ValueSetText(tblInfo, "desc", (str)sqlite3_column_text(stmt_auth_get, 3));
			ValueSetInt(tblInfo, "sort", sqlite3_column_int64(stmt_auth_get, 4));
			ValueSetRef(tblInfo, "groupList", G_CACHE_Group);
			bRow = true;
		}
		sqlite3_reset(stmt_auth_get);
		if ( !bRow ) {
			xrtValueRelease(tblInfo);
			LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
			return;
		}
		
		// 构建页面并返回
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("auth/auth_edit.html", tblInfo, &iSize);
		xrtValueRelease(tblInfo);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// auth 主接口
void Request_Auth_Auth(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 从 URL 查询字符串中提取参数
		char sParam[64];
		xsReqQueryValue(objReq, "page", sParam, sizeof(sParam));
		int64 iPage = Util_ParseI64(sParam);
		if ( iPage <= 0 ) { iPage = 1; }
		xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam));
		int64 iLimit = Util_ParseI64(sParam);
		if ( iLimit <= 0 ) { iLimit = 10; }
		if ( iLimit > 100 ) { iLimit = 100; }	/* F9 */
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = xsReqQueryValue(objReq, "search", sParam, sizeof(sParam));
		
		// 从数据库中查询数据
		xvalue* data = ValueArray();
		int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_int64(stmt_auth_all, 1, iLimit);
			sqlite3_bind_int64(stmt_auth_all, 2, iOffset);
			while ( sqlite3_step(stmt_auth_all) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject();
				ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_auth_all, 0));
				ValueSetInt(tblRow, "groupID", sqlite3_column_int64(stmt_auth_all, 1));
				ValueSetText(tblRow, "name", (str)sqlite3_column_text(stmt_auth_all, 2));
				ValueSetText(tblRow, "desc", (str)sqlite3_column_text(stmt_auth_all, 3));
				ValueSetInt(tblRow, "sort", sqlite3_column_int64(stmt_auth_all, 4));
				xtime iTime = sqlite3_column_int64(stmt_auth_all, 5);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				iTime = sqlite3_column_int64(stmt_auth_all, 6);
				ValueSetOwnedText(tblRow, "updateTime", TimeText(iTime, TIME_TEXT_DATETIME));
				ValueSetText(tblRow, "groupName", (str)sqlite3_column_text(stmt_auth_all, 7));
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_auth_all);
			if ( sqlite3_step(stmt_auth_count_all) == SQLITE_ROW ) iCount = sqlite3_column_int64(stmt_auth_count_all, 0);
			sqlite3_reset(stmt_auth_count_all);
		} else {
			// 筛选
			sqlite3_bind_text(stmt_auth_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_text(stmt_auth_sel, 2, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_auth_sel, 3, iLimit);
			sqlite3_bind_int64(stmt_auth_sel, 4, iOffset);
			while ( sqlite3_step(stmt_auth_sel) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject();
				ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_auth_sel, 0));
				ValueSetInt(tblRow, "groupID", sqlite3_column_int64(stmt_auth_sel, 1));
				ValueSetText(tblRow, "name", (str)sqlite3_column_text(stmt_auth_sel, 2));
				ValueSetText(tblRow, "desc", (str)sqlite3_column_text(stmt_auth_sel, 3));
				ValueSetInt(tblRow, "sort", sqlite3_column_int64(stmt_auth_sel, 4));
				xtime iTime = sqlite3_column_int64(stmt_auth_sel, 5);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				iTime = sqlite3_column_int64(stmt_auth_sel, 6);
				ValueSetOwnedText(tblRow, "updateTime", TimeText(iTime, TIME_TEXT_DATETIME));
				ValueSetText(tblRow, "groupName", (str)sqlite3_column_text(stmt_auth_sel, 7));
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_auth_sel);
			sqlite3_bind_text(stmt_auth_count_sel, 1, sParam, -1, NULL);
			sqlite3_bind_text(stmt_auth_count_sel, 2, sParam, -1, NULL);
			if ( sqlite3_step(stmt_auth_count_sel) == SQLITE_ROW ) iCount = sqlite3_column_int64(stmt_auth_count_sel, 0);
			sqlite3_reset(stmt_auth_count_sel);
		}
		
		// 构建返回值
		xvalue* tblRet = ValueObject();
		ValueSetBool(tblRet, "result", true);
		ValueSetInt(tblRet, "code", 0);
		ValueSetInt(tblRet, "count", iCount);
		ValueSetText(tblRet, "message", "权限分组数据获取成功！");
		ValueSetOwn(tblRet, "data", data);
		
		// 生成 JSON
		size_t iRetSize = 0;
		char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xrtValueRelease(tblRet);
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		
		// 添加权限组
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		int64 groupID = ValueInt(tblForm, "groupID");
		str name = ValueText(tblForm, "name");
		str desc = ValueText(tblForm, "desc");
		int64 sort = ValueInt(tblForm, "sort");
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( name && (strlen(name) > 64) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"名称最多64个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		if ( desc && (strlen(desc) > 1024) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"描述最多1024个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		xtime now = xrtNow();
		
		sqlite3_bind_int64(stmt_auth_add, 1, groupID);
		sqlite3_bind_text(stmt_auth_add, 2, name, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt_auth_add, 3, desc, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_auth_add, 4, sort);
		sqlite3_bind_int64(stmt_auth_add, 5, now);
		sqlite3_bind_int64(stmt_auth_add, 6, now);
		bool written = DB_Write(stmt_auth_add, true);
		int64 newId = written ? sqlite3_last_insert_rowid(G_DB) : 0;
		xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		
		// 返回成功信息和新创建的ID
		xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限组添加成功！\", \"data\": {\"id\": %lld}}", newId);
		
		// 刷新权限分组 + 权限分类缓存
		ReloadCache_Auth_Auth();
		ReloadCache_Auth_Group();
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_PUT) ) {
		
		// 更新权限组
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		int64 id = ValueInt(tblForm, "id");
		int64 groupID = ValueInt(tblForm, "groupID");
		str name = ValueText(tblForm, "name");
		str desc = ValueText(tblForm, "desc");
		int64 sort = ValueInt(tblForm, "sort");
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( name && (strlen(name) > 64) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"名称最多64个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		if ( desc && (strlen(desc) > 1024) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"描述最多1024个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		xtime now = xrtNow();
		
		sqlite3_bind_int64(stmt_auth_put, 1, groupID);
		sqlite3_bind_text(stmt_auth_put, 2, name, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt_auth_put, 3, desc, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_auth_put, 4, sort);
		sqlite3_bind_int64(stmt_auth_put, 5, now);
		sqlite3_bind_int64(stmt_auth_put, 6, id);
		bool written = DB_Write(stmt_auth_put, true);
		xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		
		// 返回成功信息
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限组更新成功！\"}", 0);
		
		// 刷新权限分组 + 权限分类缓存
		ReloadCache_Auth_Auth();
		ReloadCache_Auth_Group();
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_DELETE) ) {
		
		// 删除权限组（软删除）
		char sID[24];
		xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		int64 id = Util_ParseI64(sID);
		if ( id > 1 ) {
			
			// 有关联的URI权限，将他们移入未分组
			sqlite3_bind_int64(stmt_auth_mov, 1, id);
			sqlite3_bind_int64(stmt_auth_del, 1, id);
			bool written = DB_MoveAndDelete(stmt_auth_mov, stmt_auth_del);
			if (ReplyIfWriteFailed(objResp, written)) return;
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限组删除成功！\"}", 0);
			
			// 刷新权限分组 + 权限分类缓存
			ReloadCache_Auth_Auth();
			ReloadCache_Auth_Group();
			
		} else if ( id == 1 ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"不能删除默认权限组！\"}", 0);
		} else {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的权限组ID\"}", 0);
		}
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}





// 获取 uris 管理页面视图
void Request_View_Auth_URIs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 登录页面
		LoadPage(objResp, 200, HTTP_CT_HTML, "auth/uris.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 uris 编辑页面视图
void Request_View_Auth_URIs_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 获取参数
		char sID[24];
		xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		int64 id = Util_ParseI64(sID);
		
		// 读取数据库中的记录，构建模板数据
		xvalue* tblInfo = ValueObject();
		int bRow = false;
		sqlite3_bind_int64(stmt_uris_get, 1, id);
		while ( sqlite3_step(stmt_uris_get) == SQLITE_ROW ) {
			ValueSetInt(tblInfo, "id", id);
			ValueSetInt(tblInfo, "authID", sqlite3_column_int64(stmt_uris_get, 1));
			ValueSetText(tblInfo, "uri", (str)sqlite3_column_text(stmt_uris_get, 2));
			ValueSetText(tblInfo, "desc", (str)sqlite3_column_text(stmt_uris_get, 3));
			// 几个字段: isBackend(4), needAuth(5), needLog(6), keepActive(7)
			ValueSetInt(tblInfo, "isBackend", sqlite3_column_int64(stmt_uris_get, 4));
			ValueSetInt(tblInfo, "needAuth", sqlite3_column_int64(stmt_uris_get, 5));
			ValueSetInt(tblInfo, "needLog", sqlite3_column_int64(stmt_uris_get, 6));
			ValueSetInt(tblInfo, "keepActive", sqlite3_column_int64(stmt_uris_get, 7));
			ValueSetInt(tblInfo, "sort", sqlite3_column_int64(stmt_uris_get, 8));
			{
				/* isPersistent(16), namespace(17), plugin_xid(19), plugin_generation(20), routeActive(21) */
				ValueSetInt(tblInfo, "isPersistent", sqlite3_column_int64(stmt_uris_get, 16));
				ValueSetText(tblInfo, "namespace", (str)sqlite3_column_text(stmt_uris_get, 17));
				ValueSetText(tblInfo, "pluginXid", (str)sqlite3_column_text(stmt_uris_get, 19));
				ValueSetInt(tblInfo, "pluginGeneration", sqlite3_column_int64(stmt_uris_get, 20));
				ValueSetBool(tblInfo, "routeActive", sqlite3_column_int(stmt_uris_get, 21) != 0);
			}
			ValueSetRef(tblInfo, "authList", G_CACHE_Auth);
			ValueSetRef(tblInfo, "memberAuthList", G_CACHE_MemberAuth);
			bRow = true;
		}
		sqlite3_reset(stmt_uris_get);
		if ( !bRow ) {
			xrtValueRelease(tblInfo);
			LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
			return;
		}
		
		// 构建页面并返回
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("auth/uris_edit.html", tblInfo, &iSize);
		xrtValueRelease(tblInfo);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// uris 主接口
void Request_Auth_URIs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 从 URL 查询字符串中提取参数
		char sParam[64];
		xsReqQueryValue(objReq, "page", sParam, sizeof(sParam));
		int64 iPage = Util_ParseI64(sParam);
		if ( iPage <= 0 ) { iPage = 1; }
		xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam));
		int64 iLimit = Util_ParseI64(sParam);
		if ( iLimit <= 0 ) { iLimit = 10; }
		if ( iLimit > 100 ) { iLimit = 100; }	/* F9 */
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = xsReqQueryValue(objReq, "search", sParam, sizeof(sParam));
		/* 插件筛选：__core__=xAdmin 本体（无插件归属）、__all__/缺省=全部（兼容旧调用）、其余=插件 xid */
		char sPlugin[96];
		if ( !xsReqQueryValue(objReq, "plugin", sPlugin, sizeof(sPlugin)) || !sPlugin[0] ) {
			snprintf(sPlugin, sizeof(sPlugin), "__all__");
		}

		// 从数据库中查询数据
		xvalue* data = ValueArray();
		int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_text(stmt_uris_all, 1, sPlugin, -1, NULL);
			sqlite3_bind_int64(stmt_uris_all, 2, iLimit);
			sqlite3_bind_int64(stmt_uris_all, 3, iOffset);
			while ( sqlite3_step(stmt_uris_all) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject();
				ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_uris_all, 0));
				ValueSetInt(tblRow, "authID", sqlite3_column_int64(stmt_uris_all, 1));
				ValueSetText(tblRow, "uri", (str)sqlite3_column_text(stmt_uris_all, 2));
				ValueSetText(tblRow, "desc", (str)sqlite3_column_text(stmt_uris_all, 3));
				// 几个字段: isBackend(4), needAuth(5), needLog(6), keepActive(7)
				ValueSetInt(tblRow, "isBackend", sqlite3_column_int64(stmt_uris_all, 4));
				ValueSetInt(tblRow, "needAuth", sqlite3_column_int64(stmt_uris_all, 5));
				ValueSetInt(tblRow, "needLog", sqlite3_column_int64(stmt_uris_all, 6));
				ValueSetInt(tblRow, "keepActive", sqlite3_column_int64(stmt_uris_all, 7));
				ValueSetInt(tblRow, "sort", sqlite3_column_int64(stmt_uris_all, 8));
				xtime iTime = sqlite3_column_int64(stmt_uris_all, 9);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				iTime = sqlite3_column_int64(stmt_uris_all, 10);
				ValueSetOwnedText(tblRow, "updateTime", TimeText(iTime, TIME_TEXT_DATETIME));
				ValueSetText(tblRow, "authName", (str)sqlite3_column_text(stmt_uris_all, 11));
				ValueSetText(tblRow, "memberAuthName", (str)sqlite3_column_text(stmt_uris_all, 12));
				ValueSetInt(tblRow, "isPersistent", sqlite3_column_int64(stmt_uris_all, 13));
				ValueSetText(tblRow, "namespace", (str)sqlite3_column_text(stmt_uris_all, 14));
				ValueSetText(tblRow, "pluginXid", (str)sqlite3_column_text(stmt_uris_all, 15));
				ValueSetInt(tblRow, "pluginGeneration", sqlite3_column_int64(stmt_uris_all, 16));
				ValueSetBool(tblRow, "routeActive", sqlite3_column_int(stmt_uris_all, 17) != 0);
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_uris_all);
			sqlite3_bind_text(stmt_uris_count_all, 1, sPlugin, -1, NULL);
			if ( sqlite3_step(stmt_uris_count_all) == SQLITE_ROW ) iCount = sqlite3_column_int64(stmt_uris_count_all, 0);
			sqlite3_reset(stmt_uris_count_all);
		} else {
			// 筛选
			sqlite3_bind_text(stmt_uris_sel, 1, sPlugin, -1, NULL);
			sqlite3_bind_text(stmt_uris_sel, 2, sParam, iSize, NULL);
			sqlite3_bind_text(stmt_uris_sel, 3, sParam, iSize, NULL);
			sqlite3_bind_text(stmt_uris_sel, 4, sParam, iSize, NULL);
			sqlite3_bind_text(stmt_uris_sel, 5, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_uris_sel, 6, iLimit);
			sqlite3_bind_int64(stmt_uris_sel, 7, iOffset);
			while ( sqlite3_step(stmt_uris_sel) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject();
				ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_uris_sel, 0));
				ValueSetInt(tblRow, "authID", sqlite3_column_int64(stmt_uris_sel, 1));
				ValueSetText(tblRow, "uri", (str)sqlite3_column_text(stmt_uris_sel, 2));
				ValueSetText(tblRow, "desc", (str)sqlite3_column_text(stmt_uris_sel, 3));
				// 几个字段: isBackend(4), needAuth(5), needLog(6), keepActive(7)
				ValueSetInt(tblRow, "isBackend", sqlite3_column_int64(stmt_uris_sel, 4));
				ValueSetInt(tblRow, "needAuth", sqlite3_column_int64(stmt_uris_sel, 5));
				ValueSetInt(tblRow, "needLog", sqlite3_column_int64(stmt_uris_sel, 6));
				ValueSetInt(tblRow, "keepActive", sqlite3_column_int64(stmt_uris_sel, 7));
				ValueSetInt(tblRow, "sort", sqlite3_column_int64(stmt_uris_sel, 8));
				xtime iTime = sqlite3_column_int64(stmt_uris_sel, 9);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				iTime = sqlite3_column_int64(stmt_uris_sel, 10);
				ValueSetOwnedText(tblRow, "updateTime", TimeText(iTime, TIME_TEXT_DATETIME));
				ValueSetText(tblRow, "authName", (str)sqlite3_column_text(stmt_uris_sel, 11));
				ValueSetText(tblRow, "memberAuthName", (str)sqlite3_column_text(stmt_uris_sel, 12));
				ValueSetInt(tblRow, "isPersistent", sqlite3_column_int64(stmt_uris_sel, 13));
				ValueSetText(tblRow, "namespace", (str)sqlite3_column_text(stmt_uris_sel, 14));
				ValueSetText(tblRow, "pluginXid", (str)sqlite3_column_text(stmt_uris_sel, 15));
				ValueSetInt(tblRow, "pluginGeneration", sqlite3_column_int64(stmt_uris_sel, 16));
				ValueSetBool(tblRow, "routeActive", sqlite3_column_int(stmt_uris_sel, 17) != 0);
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_uris_sel);
			sqlite3_bind_text(stmt_uris_count_sel, 1, sPlugin, -1, NULL);
			sqlite3_bind_text(stmt_uris_count_sel, 2, sParam, -1, NULL);
			sqlite3_bind_text(stmt_uris_count_sel, 3, sParam, -1, NULL);
			sqlite3_bind_text(stmt_uris_count_sel, 4, sParam, -1, NULL);
			sqlite3_bind_text(stmt_uris_count_sel, 5, sParam, -1, NULL);
			if ( sqlite3_step(stmt_uris_count_sel) == SQLITE_ROW ) iCount = sqlite3_column_int64(stmt_uris_count_sel, 0);
			sqlite3_reset(stmt_uris_count_sel);
		}

		// 构建返回值
		xvalue* tblRet = ValueObject();
		ValueSetBool(tblRet, "result", true);
		ValueSetInt(tblRet, "code", 0);
		ValueSetInt(tblRet, "count", iCount);
		ValueSetText(tblRet, "message", "接口数据获取成功！");
		ValueSetOwn(tblRet, "data", data);
		/* 筛选下拉框数据源：已注册 URI 的插件清单（column_text 借用视图，构造即拷贝） */
		{
			xvalue* arrPlugins = ValueArray();
			while ( sqlite3_step(stmt_uris_plugins) == SQLITE_ROW ) {
				str sXid = (str)sqlite3_column_text(stmt_uris_plugins, 0);
				ValueArrayOwn(arrPlugins, xrtValueString(xrtStrView(sXid ? sXid : (str)"")));
			}
			sqlite3_reset(stmt_uris_plugins);
			ValueSetOwn(tblRet, "plugins", arrPlugins);
		}
		
		// 生成 JSON
		size_t iRetSize = 0;
		char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xrtValueRelease(tblRet);
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_PUT) ) {
		
		// 更新接口信息
		xvalue* tblForm = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( xrtValueType(tblForm) != XVALUE_OBJECT ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		int64 id = ValueInt(tblForm, "id");
		int64 authID = ValueInt(tblForm, "authID");
		str desc = ValueText(tblForm, "desc");
		/* E1：管理写接口字段长度上限（防异常长载荷膨胀行与缓存内存） */
		if ( desc && (strlen(desc) > 1024) ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"描述最多1024个字符！\"}", 0);
			xrtValueRelease(tblForm);
			return;
		}
		int64 sort = ValueInt(tblForm, "sort");
		// 几个字段:
		int64 isBackend = ValueInt(tblForm, "isBackend");
		int64 needAuth = ValueInt(tblForm, "needAuth");
		int64 needLog = ValueInt(tblForm, "needLog");
		int64 keepActive = ValueInt(tblForm, "keepActive");
		
		// UPDATE uris SET authID=?, desc=?, sort=?, isBackend=?, needAuth=?, needLog=?, keepActive=?, updateTime=? WHERE id=?
		sqlite3_bind_int64(stmt_uris_put, 1, authID);
		sqlite3_bind_text(stmt_uris_put, 2, desc, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_uris_put, 3, sort);
		sqlite3_bind_int64(stmt_uris_put, 4, isBackend);
		sqlite3_bind_int64(stmt_uris_put, 5, needAuth);
		sqlite3_bind_int64(stmt_uris_put, 6, needLog);
		sqlite3_bind_int64(stmt_uris_put, 7, keepActive);
		sqlite3_bind_int64(stmt_uris_put, 8, ValueInt(tblForm, "isPersistent"));
		{
			str ns = ValueText(tblForm, "namespace");
			sqlite3_bind_text(stmt_uris_put, 9, ns && ns[0] ? ns : "auto", -1, SQLITE_TRANSIENT);
		}
		sqlite3_bind_int64(stmt_uris_put, 10, xrtNow());
		sqlite3_bind_int64(stmt_uris_put, 11, id);
		bool written = DB_Write(stmt_uris_put, true);
		xrtValueRelease(tblForm);
		if (ReplyIfWriteFailed(objResp, written)) return;
		
		// 更新路由表和权限缓存
		Auth_UpdateURIS();
		Auth_ReloadCache();
		MemberAuth_ReloadCache();
		
		// 响应请求
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"接口信息修改成功！\"}", 0);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}


