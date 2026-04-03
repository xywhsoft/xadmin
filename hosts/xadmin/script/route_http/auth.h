


// 获取 user 管理页面视图
void Request_View_Auth_User(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 登录页面
		LoadPage(objResp, 200, HTTP_CT_HTML, "auth/user.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 user 添加页面视图
void Request_View_Auth_User_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 构建模板�?- 角色列表
		xvalue tblInfo = xvoCreateTable();
		xvoTableSetValue(tblInfo, "roleList", 8, G_CACHE_Role, FALSE);
		
		// 构建页面并返�?
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("auth/user_add.html", tblInfo, &iSize);
		xvoUnref(tblInfo);
		http_reply(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 user 编辑页面视图
void Request_View_Auth_User_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 获取参数
		char sID[24];
		HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
		int64 id = xrtStrToI64(sID);
		
		// 读取数据库中的记录，构建模板�?
		xvalue tblInfo = xvoCreateTable();
		int bRow = FALSE;
		sqlite3_bind_int64(stmt_user_get, 1, id);
		while ( sqlite3_step(stmt_user_get) == SQLITE_ROW ) {
			xvoTableSetInt(tblInfo, "id", 2, id);
			xvoTableSetInt(tblInfo, "role", 4, sqlite3_column_int64(stmt_user_get, 4));
			xvoTableSetInt(tblInfo, "authLevel", 9, sqlite3_column_int64(stmt_user_get, 5));
			xvoTableSetText(tblInfo, "user", 4, (str)sqlite3_column_text(stmt_user_get, 1), 0, FALSE);
			bRow = TRUE;
		}
		sqlite3_reset(stmt_user_get);
		
		// 添加角色列表
		xvoTableSetValue(tblInfo, "roleList", 8, G_CACHE_Role, FALSE);
		
		// 构建页面并返�?
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("auth/user_edit.html", tblInfo, &iSize);
		xvoUnref(tblInfo);
		http_reply(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// user 主接�?
void Request_Auth_User(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// �?URL 查询字符串中提取参数
		char sParam[64];
		HttpGetQueryVar(objReq, "page", sParam, sizeof(sParam));
		int64 iPage = xrtStrToI64(sParam);
		if ( iPage <= 0 ) { iPage = 1; }
		HttpGetQueryVar(objReq, "limit", sParam, sizeof(sParam));
		int64 iLimit = xrtStrToI64(sParam);
		if ( iLimit <= 0 ) { iLimit = 10; }
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = HttpGetQueryVar(objReq, "search", sParam, sizeof(sParam));
		
		// 从数据库中查询数�?
		xvalue data = xvoCreateArray();
		int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_int64(stmt_user_all, 1, iLimit);
			sqlite3_bind_int64(stmt_user_all, 2, iOffset);
			while ( sqlite3_step(stmt_user_all) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_user_all, 0));
				xvoTableSetInt(tblRow, "role", 4, sqlite3_column_int64(stmt_user_all, 4));
				xvoTableSetInt(tblRow, "authLevel", 9, sqlite3_column_int64(stmt_user_all, 5));
				xvoTableSetText(tblRow, "user", 4, (str)sqlite3_column_text(stmt_user_all, 1), 0, FALSE);
				// 不返回密码字�?
				xtime iTime = sqlite3_column_int64(stmt_user_all, 6);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iTime = sqlite3_column_int64(stmt_user_all, 7);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				xvoTableSetText(tblRow, "roleName", 8, (str)sqlite3_column_text(stmt_user_all, 9), 0, FALSE);
				if ( iCount <= 0 ) {
					iCount = sqlite3_column_int64(stmt_user_all, 10);
				}
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_user_all);
		} else {
			// 筛�?
			sqlite3_bind_text(stmt_user_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_user_sel, 2, iLimit);
			sqlite3_bind_int64(stmt_user_sel, 3, iOffset);
			while ( sqlite3_step(stmt_user_sel) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_user_sel, 0));
				xvoTableSetInt(tblRow, "role", 4, sqlite3_column_int64(stmt_user_sel, 4));
				xvoTableSetInt(tblRow, "authLevel", 9, sqlite3_column_int64(stmt_user_sel, 5));
				xvoTableSetText(tblRow, "user", 4, (str)sqlite3_column_text(stmt_user_sel, 1), 0, FALSE);
				// 不返回密码字�?
				xtime iTime = sqlite3_column_int64(stmt_user_sel, 6);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iTime = sqlite3_column_int64(stmt_user_sel, 7);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				xvoTableSetText(tblRow, "roleName", 8, (str)sqlite3_column_text(stmt_user_sel, 9), 0, FALSE);
				if ( iCount <= 0 ) {
					iCount = sqlite3_column_int64(stmt_user_sel, 10);
				}
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_user_sel);
		}
		
		// 构建返回�?
		xvalue tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetInt(tblRet, "code", 4, 0);
		xvoTableSetInt(tblRet, "count", 5, iCount);
		xvoTableSetText(tblRet, "message", 7, "�û����ݻ�ȡ�ɹ���", 0, FALSE);
		xvoTableSetValue(tblRet, "data", 4, data, TRUE);
		
		// 生成 JSON
		size_t iRetSize = 0;
		char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
		http_reply(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xvoUnref(tblRet);
		
	} else if ( HttpMethodIs(objReq, "POST") ) {
		
		// 添加用户
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		int64 role = xvoTableGetInt(tblForm, "role", 4);
		if ( role < 1 ) {
			role = 1;
		}
		str user = xvoTableGetText(tblForm, "username", 8);
		if ( !user || (strlen(user) == 0) ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名不能为空！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		str password = xvoTableGetText(tblForm, "password", 8);
		if ( !password || (strlen(password) == 0) ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"密码不能为空！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		
		// 检查用户名是否已存�?
		bool bExists = FALSE;
		sqlite3_bind_text(stmt_user_chk, 1, user, strlen(user), SQLITE_STATIC);
		sqlite3_bind_int64(stmt_user_chk, 2, 0);
		while ( sqlite3_step(stmt_user_chk) == SQLITE_ROW ) {
			bExists = TRUE;
			break;
		}
		sqlite3_reset(stmt_user_chk);
		if ( bExists ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名已存在！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		
		// 服务端二�?SHA-256 哈希
		str sSalt = xrtMakeXIDS();
		str sPwdHash = ServerHashPassword(user, sSalt, password);
		
		// 写入数据�?
		xtime now = xrtNow();
		sqlite3_bind_text(stmt_user_add, 1, user, strlen(user), SQLITE_STATIC);
		sqlite3_bind_text(stmt_user_add, 2, sSalt, strlen(sSalt), SQLITE_STATIC);
		sqlite3_bind_text(stmt_user_add, 3, sPwdHash, strlen(sPwdHash), SQLITE_STATIC);
		sqlite3_bind_int64(stmt_user_add, 4, role);
		sqlite3_bind_int64(stmt_user_add, 5, 0); // authLevel 默认�?0
		sqlite3_bind_int64(stmt_user_add, 6, now);
		sqlite3_bind_int64(stmt_user_add, 7, now);
		sqlite3_step(stmt_user_add);
		int64 newId = sqlite3_last_insert_rowid(G_DB);
		sqlite3_reset(stmt_user_add);
		xrtFree(sSalt);
		xrtFree(sPwdHash);
		xvoUnref(tblForm);
		
		// 返回成功信息和新创建的ID
		xvalue tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetText(tblRet, "message", 7, "�û����ӳɹ���", 0, FALSE);
		xvalue dataRet = xvoCreateTable();
		xvoTableSetInt(dataRet, "id", 2, newId);
		xvoTableSetValue(tblRet, "data", 4, dataRet, TRUE);
		size_t iRetSize = 0;
		char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
		http_reply(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xvoUnref(tblRet);
		
	} else if ( HttpMethodIs(objReq, "PUT") ) {
		
		// 更新用户 - 不更新用户名，只更新角色
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		int64 id = xvoTableGetInt(tblForm, "id", 2);
		int64 role = xvoTableGetInt(tblForm, "role", 4);
		int64 authLevel = xvoTableGetInt(tblForm, "authLevel", 9);
		if ( role < 1 ) {
			role = 1;
		}
								
		// 写入数据�?
		xtime now = xrtNow();
		sqlite3_bind_int64(stmt_user_put, 1, role);
		sqlite3_bind_int64(stmt_user_put, 2, authLevel);
		sqlite3_bind_int64(stmt_user_put, 3, now);
		sqlite3_bind_int64(stmt_user_put, 4, id);
		sqlite3_step(stmt_user_put);
		sqlite3_reset(stmt_user_put);
		xvoUnref(tblForm);
		
		// 返回成功信息
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"用户更新成功！\"}", 0);
		
	} else if ( HttpMethodIs(objReq, "DELETE") ) {
		
		// 删除用户（软删除�?
		// 从URL查询字符串中提取ID
		char sID[24];
		HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
		int64 id = xrtStrToI64(sID);
		if ( id > 0 ) {
			// 执行软删�?
			sqlite3_bind_int64(stmt_user_del, 1, id);
			sqlite3_step(stmt_user_del);
			sqlite3_reset(stmt_user_del);
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"用户删除成功！\"}", 0);
		} else {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的用户ID\"}", 0);
		}
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 重置密码接口
void Request_Auth_User_Repwd(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "POST") ) {
		
		// 重置密码
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		int64 id = xvoTableGetInt(tblForm, "id", 2);
		str username = xvoTableGetText(tblForm, "username", 8);
		if ( !username || (strlen(username) == 0) ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名不能为空！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		str password = xvoTableGetText(tblForm, "password", 8);
		if ( !password || (strlen(password) == 0) ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"密码不能为空！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		
		// 生成新的随机 salt（使�?XID�?
		str sSalt = xrtMakeXIDS();
		
		// 服务端二�?SHA-256 哈希
		str sPwdHash = ServerHashPassword(username, sSalt, password);
		
		// 更新 salt 和密�?
		xtime now = xrtNow();
		sqlite3_bind_text(stmt_user_pwd, 1, sSalt, strlen(sSalt), SQLITE_STATIC);
		sqlite3_bind_text(stmt_user_pwd, 2, sPwdHash, strlen(sPwdHash), SQLITE_STATIC);
		sqlite3_bind_int64(stmt_user_pwd, 3, now);
		sqlite3_bind_int64(stmt_user_pwd, 4, id);
		sqlite3_step(stmt_user_pwd);
		sqlite3_reset(stmt_user_pwd);
		
		xrtFree(sSalt);
		xrtFree(sPwdHash);
		xvoUnref(tblForm);
		
		// 返回成功信息
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"密码重置成功！\"}", 0);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}





// 获取 role 管理页面视图
void Request_View_Auth_Role(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 角色管理页面
		LoadPage(objResp, 200, HTTP_CT_HTML, "auth/role.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 role 添加页面视图
void Request_View_Auth_Role_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 构建模板�?- 使用分组缓存
		xvalue tblInfo = xvoCreateTable();
		xvoTableSetValue(tblInfo, "authGroups", 10, G_CACHE_Group, FALSE);
		
		// 构建页面并返�?
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("auth/role_add.html", tblInfo, &iSize);
		xvoUnref(tblInfo);
		http_reply(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 role 编辑页面视图
void Request_View_Auth_Role_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 获取参数
		char sID[24];
		HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
		int64 id = xrtStrToI64(sID);
		
		// 读取数据库中的记录，构建模板�?
		xvalue tblInfo = xvoCreateTable();
		xvalue listAuth = xvoCreateList();
		int bRow = FALSE;
		sqlite3_bind_int64(stmt_role_get, 1, id);
		while ( sqlite3_step(stmt_role_get) == SQLITE_ROW ) {
			xvoTableSetInt(tblInfo, "id", 2, id);
			xvoTableSetText(tblInfo, "name", 4, (str)sqlite3_column_text(stmt_role_get, 1), 0, FALSE);
			xvoTableSetText(tblInfo, "desc", 4, (str)sqlite3_column_text(stmt_role_get, 2), 0, FALSE);
			xvoTableSetInt(tblInfo, "authLevel", 9, sqlite3_column_int64(stmt_role_get, 4));
			// 解析权限分组列表 - 转换�?list 方便按ID索引
			str sAuthList = (str)sqlite3_column_text(stmt_role_get, 3);
			if ( sAuthList && (strlen(sAuthList) > 2) ) {
				xvalue arrAuth = xrtParseJSON(sAuthList, 0);
				if ( arrAuth && (arrAuth->Type == XVO_DT_ARRAY) && (arrAuth->vArray->Count > 0) ) {
					for ( int i = 0; i < arrAuth->vArray->Count; i++ ) {
						int64 authID = xvoArrayGetInt(arrAuth, i);
						if ( authID > 0 ) {
							xvoListSetBool(listAuth, authID, TRUE);
						}
					}
				}
				xvoUnref(arrAuth);
			}
			bRow = TRUE;
		}
		sqlite3_reset(stmt_role_get);
		
		// 标记已选中的权�?- 这里创建深拷贝副本，避免多线程写入同步问�?
		xvalue arrAuthGroups = xvoDeepCopy(G_CACHE_Group);
		for ( int g = 1; g <= arrAuthGroups->vArray->Count; g++ ) {
			xvalue tblGroup = xrtPtrArrayGet_Inline(arrAuthGroups->vArray, g);
			xvalue arrAuths = xvoTableGetValue(tblGroup, "auths", 5);
			if ( arrAuths && (arrAuths->Type == XVO_DT_ARRAY) && (arrAuths->vArray->Count > 0) ) {
				for ( int a = 1; a <= arrAuths->vArray->Count; a++ ) {
					xvalue tblAuth = xrtPtrArrayGet_Inline(arrAuths->vArray, a);
					int64 authID = xvoTableGetInt(tblAuth, "id", 2);
					bool bCheck = xvoListGetBool(listAuth, authID);
					xvoTableSetText(tblAuth, "checked", 7, bCheck ? (str)" checked" : (str)"", 0, FALSE);
				}
			}
		}
		xvoUnref(listAuth);
		xvoTableSetValue(tblInfo, "authGroups", 10, arrAuthGroups, TRUE);
		
		// 构建页面并返�?
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("auth/role_edit.html", tblInfo, &iSize);
		xvoUnref(tblInfo);
		http_reply(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// role 主接�?
void Request_Auth_Role(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// �?URL 查询字符串中提取参数
		char sParam[64];
		HttpGetQueryVar(objReq, "page", sParam, sizeof(sParam));
		int64 iPage = xrtStrToI64(sParam);
		if ( iPage <= 0 ) { iPage = 1; }
		HttpGetQueryVar(objReq, "limit", sParam, sizeof(sParam));
		int64 iLimit = xrtStrToI64(sParam);
		if ( iLimit <= 0 ) { iLimit = 10; }
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = HttpGetQueryVar(objReq, "search", sParam, sizeof(sParam));
		
		// 从数据库中查询数�?
		xvalue data = xvoCreateArray();
		int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_int64(stmt_role_all, 1, iLimit);
			sqlite3_bind_int64(stmt_role_all, 2, iOffset);
			while ( sqlite3_step(stmt_role_all) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_role_all, 0));
				xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt_role_all, 1), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt_role_all, 2), 0, FALSE);
				xvoTableSetText(tblRow, "authList", 8, (str)sqlite3_column_text(stmt_role_all, 3), 0, FALSE);
				xvoTableSetInt(tblRow, "authLevel", 9, sqlite3_column_int64(stmt_role_all, 4));
				xtime iTime = sqlite3_column_int64(stmt_role_all, 5);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iTime = sqlite3_column_int64(stmt_role_all, 6);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				xvoTableSetInt(tblRow, "authCount", 9, sqlite3_column_int64(stmt_role_all, 8));
				xvoTableSetInt(tblRow, "userCount", 9, sqlite3_column_int64(stmt_role_all, 9));
				if ( iCount <= 0 ) {
					iCount = sqlite3_column_int64(stmt_role_all, 10);
				}
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_role_all);
		} else {
			// 筛�?
			sqlite3_bind_text(stmt_role_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_text(stmt_role_sel, 2, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_role_sel, 3, iLimit);
			sqlite3_bind_int64(stmt_role_sel, 4, iOffset);
			while ( sqlite3_step(stmt_role_sel) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_role_sel, 0));
				xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt_role_sel, 1), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt_role_sel, 2), 0, FALSE);
				xvoTableSetText(tblRow, "authList", 8, (str)sqlite3_column_text(stmt_role_sel, 3), 0, FALSE);
				xvoTableSetInt(tblRow, "authLevel", 9, sqlite3_column_int64(stmt_role_sel, 4));
				xtime iTime = sqlite3_column_int64(stmt_role_sel, 5);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iTime = sqlite3_column_int64(stmt_role_sel, 6);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				xvoTableSetInt(tblRow, "authCount", 9, sqlite3_column_int64(stmt_role_sel, 8));
				xvoTableSetInt(tblRow, "userCount", 9, sqlite3_column_int64(stmt_role_sel, 9));
				if ( iCount <= 0 ) {
					iCount = sqlite3_column_int64(stmt_role_sel, 10);
				}
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_role_sel);
		}
		
		// 构建返回�?
		xvalue tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetInt(tblRet, "code", 4, 0);
		xvoTableSetInt(tblRet, "count", 5, iCount);
		xvoTableSetText(tblRet, "message", 7, "��ɫ���ݻ�ȡ�ɹ���", 0, FALSE);
		xvoTableSetValue(tblRet, "data", 4, data, TRUE);
		
		// 生成 JSON
		size_t iRetSize = 0;
		char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
		http_reply(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xvoUnref(tblRet);
		
	} else if ( HttpMethodIs(objReq, "POST") ) {
		
		// 添加角色
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		str name = xvoTableGetText(tblForm, "name", 4);
		str desc = xvoTableGetText(tblForm, "desc", 4);
		str authList = xvoTableGetText(tblForm, "authList", 8);
		int64 authLevel = xvoTableGetInt(tblForm, "authLevel", 9);
		if ( !authList || (strlen(authList) == 0) ) {
			authList = "[]";
		}
		
		// 添加数据库记�?
		xtime now = xrtNow();
		sqlite3_bind_text(stmt_role_add, 1, name, strlen(name), SQLITE_STATIC);
		sqlite3_bind_text(stmt_role_add, 2, desc, strlen(desc), SQLITE_STATIC);
		sqlite3_bind_text(stmt_role_add, 3, authList, strlen(authList), SQLITE_STATIC);
		sqlite3_bind_int64(stmt_role_add, 4, authLevel);
		sqlite3_bind_int64(stmt_role_add, 5, now);
		sqlite3_bind_int64(stmt_role_add, 6, now);
		sqlite3_step(stmt_role_add);
		int64 newId = sqlite3_last_insert_rowid(G_DB);
		sqlite3_reset(stmt_role_add);
		xvoUnref(tblForm);
		
		// 返回成功信息和新创建的ID
		HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"角色添加成功！\", \"data\": {\"id\": %lld}}", newId);
		
		// 刷新角色缓存
		Auth_ReloadCache();
		
	} else if ( HttpMethodIs(objReq, "PUT") ) {
		
		// 更新角色
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		int64 id = xvoTableGetInt(tblForm, "id", 2);
		str name = xvoTableGetText(tblForm, "name", 4);
		str desc = xvoTableGetText(tblForm, "desc", 4);
		str authList = xvoTableGetText(tblForm, "authList", 8);
		int64 authLevel = xvoTableGetInt(tblForm, "authLevel", 9);
		if ( !authList || (strlen(authList) == 0) ) {
			authList = "[]";
		}
		
		// 更新数据库记�?
		xtime now = xrtNow();
		sqlite3_bind_text(stmt_role_put, 1, name, strlen(name), SQLITE_STATIC);
		sqlite3_bind_text(stmt_role_put, 2, desc, strlen(desc), SQLITE_STATIC);
		sqlite3_bind_text(stmt_role_put, 3, authList, strlen(authList), SQLITE_STATIC);
		sqlite3_bind_int64(stmt_role_put, 4, authLevel);
		sqlite3_bind_int64(stmt_role_put, 5, now);
		sqlite3_bind_int64(stmt_role_put, 6, id);
		sqlite3_step(stmt_role_put);
		sqlite3_reset(stmt_role_put);
		xvoUnref(tblForm);
		
		// 返回成功信息
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"角色更新成功！\"}", 0);
		
		// 刷新角色缓存
		Auth_ReloadCache();
		
	} else if ( HttpMethodIs(objReq, "DELETE") ) {
		
		// 删除角色（软删除�?
		char sID[24];
		HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
		int64 id = xrtStrToI64(sID);
		if ( id > 1 ) {
			
			// 检查是否有关联的用�?
			int64 iCount = 0;
			sqlite3_bind_int64(stmt_role_sum, 1, id);
			while ( sqlite3_step(stmt_role_sum) == SQLITE_ROW ) {
				iCount = sqlite3_column_int64(stmt_role_sum, 0);
			}
			sqlite3_reset(stmt_role_sum);
			if ( iCount > 0 ) {
				// 有关联的用户，不能删�?
				http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无法删除此角色，因为它关联了用户！\"}", 0);
			} else {
				// 没有关联的用户，可以删除
				sqlite3_bind_int64(stmt_role_del, 1, id);
				sqlite3_step(stmt_role_del);
				sqlite3_reset(stmt_role_del);
				http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"角色删除成功！\"}", 0);
			}
			
			// 刷新角色缓存
			Auth_ReloadCache();
			
		} else if ( id == 1 ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"不能删除默认角色！\"}", 0);
		} else {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的角色ID\"}", 0);
		}
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}





// 获取 group 管理页面视图
void Request_View_Auth_Group(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 登录页面
		LoadPage(objResp, 200, HTTP_CT_HTML, "auth/group.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 authGroup 添加页面视图
void Request_View_Auth_Group_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		LoadPage(objResp, 200, HTTP_CT_HTML, "auth/group_add.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 authGroup 编辑页面视图
void Request_View_Auth_Group_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 获取参数
		char sID[24];
		HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
		int64 id = xrtStrToI64(sID);
		
		// 读取数据库中的记录，构建模板�?
		xvalue tblInfo = xvoCreateTable();
		int bRow = FALSE;
		sqlite3_bind_int64(stmt_group_get, 1, id);
		while ( sqlite3_step(stmt_group_get) == SQLITE_ROW ) {
			xvoTableSetInt(tblInfo, "id", 2, id);
			xvoTableSetText(tblInfo, "name", 4, (str)sqlite3_column_text(stmt_group_get, 1), 0, FALSE);
			xvoTableSetText(tblInfo, "desc", 4, (str)sqlite3_column_text(stmt_group_get, 2), 0, FALSE);
			xvoTableSetInt(tblInfo, "sort", 4, sqlite3_column_int64(stmt_group_get, 3));
			bRow = TRUE;
		}
		sqlite3_reset(stmt_group_get);
		
		// 构建页面并返�?
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("auth/group_edit.html", tblInfo, &iSize);
		xvoUnref(tblInfo);
		http_reply(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// group 主接�?
void Request_Auth_Group(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// �?URL 查询字符串中提取参数
		char sParam[64];
		HttpGetQueryVar(objReq, "page", sParam, sizeof(sParam));
		int64 iPage = xrtStrToI64(sParam);
		if ( iPage <= 0 ) { iPage = 1; }
		HttpGetQueryVar(objReq, "limit", sParam, sizeof(sParam));
		int64 iLimit = xrtStrToI64(sParam);
		if ( iLimit <= 0 ) { iLimit = 10; }
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = HttpGetQueryVar(objReq, "search", sParam, sizeof(sParam));
		
		// 从数据库中查询数�?
		xvalue data = xvoCreateArray();
		int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_int64(stmt_group_all, 1, iLimit);
			sqlite3_bind_int64(stmt_group_all, 2, iOffset);
			while ( sqlite3_step(stmt_group_all) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_group_all, 0));
				xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt_group_all, 1), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt_group_all, 2), 0, FALSE);
				xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int64(stmt_group_all, 3));
				xtime iTime = sqlite3_column_int64(stmt_group_all, 4);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iTime = sqlite3_column_int64(stmt_group_all, 5);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				xvoTableSetInt(tblRow, "authCount", 9, sqlite3_column_int64(stmt_group_all, 7));
				if ( iCount <= 0 ) {
					iCount = sqlite3_column_int64(stmt_group_all, 8);
				}
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_group_all);
		} else {
			// 筛�?
			sqlite3_bind_text(stmt_group_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_text(stmt_group_sel, 2, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_group_sel, 3, iLimit);
			sqlite3_bind_int64(stmt_group_sel, 4, iOffset);
			while ( sqlite3_step(stmt_group_sel) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_group_sel, 0));
				xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt_group_sel, 1), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt_group_sel, 2), 0, FALSE);
				xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int64(stmt_group_sel, 3));
				xtime iTime = sqlite3_column_int64(stmt_group_sel, 4);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iTime = sqlite3_column_int64(stmt_group_sel, 5);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				xvoTableSetInt(tblRow, "authCount", 9, sqlite3_column_int64(stmt_group_sel, 7));
				if ( iCount <= 0 ) {
					iCount = sqlite3_column_int64(stmt_group_sel, 8);
				}
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_group_sel);
		}
		
		// 构建返回�?
		xvalue tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetInt(tblRet, "code", 4, 0);
		xvoTableSetInt(tblRet, "count", 5, iCount);
		xvoTableSetText(tblRet, "message", 7, "Ȩ�޷������ݻ�ȡ�ɹ���", 0, FALSE);
		xvoTableSetValue(tblRet, "data", 4, data, TRUE);
		
		// 生成 JSON
		size_t iRetSize = 0;
		char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
		http_reply(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xvoUnref(tblRet);
		
	} else if ( HttpMethodIs(objReq, "POST") ) {
		
		// 添加权限分类
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		str name = xvoTableGetText(tblForm, "name", 4);
		str desc = xvoTableGetText(tblForm, "desc", 4);
		int64 sort = xvoTableGetInt(tblForm, "sort", 4);
		xtime now = xrtNow();
		
		sqlite3_bind_text(stmt_group_add, 1, name, strlen(name), SQLITE_STATIC);
		sqlite3_bind_text(stmt_group_add, 2, desc, strlen(desc), SQLITE_STATIC);
		sqlite3_bind_int64(stmt_group_add, 3, sort);
		sqlite3_bind_int64(stmt_group_add, 4, now);
		sqlite3_bind_int64(stmt_group_add, 5, now);
		sqlite3_step(stmt_group_add);
		int64 newId = sqlite3_last_insert_rowid(G_DB);
		sqlite3_reset(stmt_group_add);
		xvoUnref(tblForm);
		
		// 返回成功信息和新创建的ID
		HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分类添加成功！\", \"data\": {\"id\": %lld}}", newId);
		
		// 刷新权限分类缓存
		ReloadCache_Auth_Group();
		
	} else if ( HttpMethodIs(objReq, "PUT") ) {
		
		// 更新权限分类
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		int64 id = xvoTableGetInt(tblForm, "id", 2);
		str name = xvoTableGetText(tblForm, "name", 4);
		str desc = xvoTableGetText(tblForm, "desc", 4);
		int64 sort = xvoTableGetInt(tblForm, "sort", 4);
		xtime now = xrtNow();
		
		sqlite3_bind_text(stmt_group_put, 1, name, strlen(name), SQLITE_STATIC);
		sqlite3_bind_text(stmt_group_put, 2, desc, strlen(desc), SQLITE_STATIC);
		sqlite3_bind_int64(stmt_group_put, 3, sort);
		sqlite3_bind_int64(stmt_group_put, 4, now);
		sqlite3_bind_int64(stmt_group_put, 5, id);
		sqlite3_step(stmt_group_put);
		sqlite3_reset(stmt_group_put);
		xvoUnref(tblForm);
		
		// 返回成功信息
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分类更新成功！\"}", 0);
		
		// 刷新权限分类缓存
		ReloadCache_Auth_Group();
		
	} else if ( HttpMethodIs(objReq, "DELETE") ) {
		
		// 删除权限分类（软删除�?
		char sID[24];
		HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
		int64 id = xrtStrToI64(sID);
		if ( id > 1 ) {
			
			// 有关联的URI权限，将他们移入未分�?
			int64 iCount = 0;
			sqlite3_bind_int64(stmt_group_sum, 1, id);
			while ( sqlite3_step(stmt_group_sum) == SQLITE_ROW ) {
				iCount = sqlite3_column_int64(stmt_group_sum, 0);
			}
			sqlite3_reset(stmt_group_sum);
			if ( iCount > 0 ) {
				sqlite3_bind_int64(stmt_group_mov, 1, id);
				sqlite3_step(stmt_group_mov);
				sqlite3_reset(stmt_group_mov);
			}
			
			// 删除权限分类
			sqlite3_bind_int64(stmt_group_del, 1, id);
			sqlite3_step(stmt_group_del);
			sqlite3_reset(stmt_group_del);
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分类删除成功！\"}", 0);
			
			// 刷新权限分类缓存
			ReloadCache_Auth_Group();
			
		} else if ( id == 1 ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"不能删除默认权限分类！\"}", 0);
		} else {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的权限分类ID\"}", 0);
		}
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}





// 获取 auth 管理页面视图
void Request_View_Auth_Auth(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 登录页面
		LoadPage(objResp, 200, HTTP_CT_HTML, "auth/auth.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 auth 添加页面视图
void Request_View_Auth_Auth_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 构建模板�?
		xvalue tblInfo = xvoCreateTable();
		xvoTableSetValue(tblInfo, "groupList", 9, G_CACHE_Group, FALSE);
		
		// 构建页面并返�?
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("auth/auth_add.html", tblInfo, &iSize);
		xvoUnref(tblInfo);
		http_reply(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 auth 编辑页面视图
void Request_View_Auth_Auth_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 获取参数
		char sID[24];
		HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
		int64 id = xrtStrToI64(sID);
		
		// 读取数据库中的记录，构建模板�?
		xvalue tblInfo = xvoCreateTable();
		int bRow = FALSE;
		sqlite3_bind_int64(stmt_auth_get, 1, id);
		while ( sqlite3_step(stmt_auth_get) == SQLITE_ROW ) {
			xvoTableSetInt(tblInfo, "id", 2, id);
			xvoTableSetInt(tblInfo, "groupID", 7, sqlite3_column_int64(stmt_auth_get, 1));
			xvoTableSetText(tblInfo, "name", 4, (str)sqlite3_column_text(stmt_auth_get, 2), 0, FALSE);
			xvoTableSetText(tblInfo, "desc", 4, (str)sqlite3_column_text(stmt_auth_get, 3), 0, FALSE);
			xvoTableSetInt(tblInfo, "sort", 4, sqlite3_column_int64(stmt_auth_get, 4));
			xvoTableSetValue(tblInfo, "groupList", 9, G_CACHE_Group, FALSE);
			bRow = TRUE;
		}
		sqlite3_reset(stmt_auth_get);
		
		// 构建页面并返�?
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("auth/auth_edit.html", tblInfo, &iSize);
		xvoUnref(tblInfo);
		http_reply(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// auth 主接�?
void Request_Auth_Auth(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// �?URL 查询字符串中提取参数
		char sParam[64];
		HttpGetQueryVar(objReq, "page", sParam, sizeof(sParam));
		int64 iPage = xrtStrToI64(sParam);
		if ( iPage <= 0 ) { iPage = 1; }
		HttpGetQueryVar(objReq, "limit", sParam, sizeof(sParam));
		int64 iLimit = xrtStrToI64(sParam);
		if ( iLimit <= 0 ) { iLimit = 10; }
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = HttpGetQueryVar(objReq, "search", sParam, sizeof(sParam));
		
		// 从数据库中查询数�?
		xvalue data = xvoCreateArray();
		int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_int64(stmt_auth_all, 1, iLimit);
			sqlite3_bind_int64(stmt_auth_all, 2, iOffset);
			while ( sqlite3_step(stmt_auth_all) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_auth_all, 0));
				xvoTableSetInt(tblRow, "groupID", 7, sqlite3_column_int64(stmt_auth_all, 1));
				xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt_auth_all, 2), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt_auth_all, 3), 0, FALSE);
				xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int64(stmt_auth_all, 4));
				xtime iTime = sqlite3_column_int64(stmt_auth_all, 5);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iTime = sqlite3_column_int64(stmt_auth_all, 6);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				xvoTableSetText(tblRow, "groupName", 9, (str)sqlite3_column_text(stmt_auth_all, 7), 0, FALSE);
				if ( iCount <= 0 ) {
					iCount = sqlite3_column_int64(stmt_auth_all, 8);
				}
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_auth_all);
		} else {
			// 筛�?
			sqlite3_bind_text(stmt_auth_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_text(stmt_auth_sel, 2, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_auth_sel, 3, iLimit);
			sqlite3_bind_int64(stmt_auth_sel, 4, iOffset);
			while ( sqlite3_step(stmt_auth_sel) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_auth_sel, 0));
				xvoTableSetInt(tblRow, "groupID", 7, sqlite3_column_int64(stmt_auth_sel, 1));
				xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt_auth_sel, 2), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt_auth_sel, 3), 0, FALSE);
				xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int64(stmt_auth_sel, 4));
				xtime iTime = sqlite3_column_int64(stmt_auth_sel, 5);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iTime = sqlite3_column_int64(stmt_auth_sel, 6);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				xvoTableSetText(tblRow, "groupName", 9, (str)sqlite3_column_text(stmt_auth_sel, 7), 0, FALSE);
				if ( iCount <= 0 ) {
					iCount = sqlite3_column_int64(stmt_auth_sel, 8);
				}
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_auth_sel);
		}
		
		// 构建返回�?
		xvalue tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetInt(tblRet, "code", 4, 0);
		xvoTableSetInt(tblRet, "count", 5, iCount);
		xvoTableSetText(tblRet, "message", 7, "Ȩ�������ݻ�ȡ�ɹ���", 0, FALSE);
		xvoTableSetValue(tblRet, "data", 4, data, TRUE);
		
		// 生成 JSON
		size_t iRetSize = 0;
		char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
		http_reply(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xvoUnref(tblRet);
		
	} else if ( HttpMethodIs(objReq, "POST") ) {
		
		// 添加权限�?
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		int64 groupID = xvoTableGetInt(tblForm, "groupID", 7);
		str name = xvoTableGetText(tblForm, "name", 4);
		str desc = xvoTableGetText(tblForm, "desc", 4);
		int64 sort = xvoTableGetInt(tblForm, "sort", 4);
		xtime now = xrtNow();
		
		sqlite3_bind_int64(stmt_auth_add, 1, groupID);
		sqlite3_bind_text(stmt_auth_add, 2, name, strlen(name), SQLITE_STATIC);
		sqlite3_bind_text(stmt_auth_add, 3, desc, strlen(desc), SQLITE_STATIC);
		sqlite3_bind_int64(stmt_auth_add, 4, sort);
		sqlite3_bind_int64(stmt_auth_add, 5, now);
		sqlite3_bind_int64(stmt_auth_add, 6, now);
		sqlite3_step(stmt_auth_add);
		int64 newId = sqlite3_last_insert_rowid(G_DB);
		sqlite3_reset(stmt_auth_add);
		xvoUnref(tblForm);
		
		// 返回成功信息和新创建的ID
		HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限组添加成功！\", \"data\": {\"id\": %lld}}", newId);
		
		// 刷新权限分组 + 权限分类缓存
		ReloadCache_Auth_Auth();
		ReloadCache_Auth_Group();
		
	} else if ( HttpMethodIs(objReq, "PUT") ) {
		
		// 更新权限�?
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		int64 id = xvoTableGetInt(tblForm, "id", 2);
		int64 groupID = xvoTableGetInt(tblForm, "groupID", 7);
		str name = xvoTableGetText(tblForm, "name", 4);
		str desc = xvoTableGetText(tblForm, "desc", 4);
		int64 sort = xvoTableGetInt(tblForm, "sort", 4);
		xtime now = xrtNow();
		
		sqlite3_bind_int64(stmt_auth_put, 1, groupID);
		sqlite3_bind_text(stmt_auth_put, 2, name, strlen(name), SQLITE_STATIC);
		sqlite3_bind_text(stmt_auth_put, 3, desc, strlen(desc), SQLITE_STATIC);
		sqlite3_bind_int64(stmt_auth_put, 4, sort);
		sqlite3_bind_int64(stmt_auth_put, 5, now);
		sqlite3_bind_int64(stmt_auth_put, 6, id);
		sqlite3_step(stmt_auth_put);
		sqlite3_reset(stmt_auth_put);
		xvoUnref(tblForm);
		
		// 返回成功信息
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限组更新成功！\"}", 0);
		
		// 刷新权限分组 + 权限分类缓存
		ReloadCache_Auth_Auth();
		ReloadCache_Auth_Group();
		
	} else if ( HttpMethodIs(objReq, "DELETE") ) {
		
		// 删除权限组（软删除）
		char sID[24];
		HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
		int64 id = xrtStrToI64(sID);
		if ( id > 1 ) {
			
			// 有关联的URI权限，将他们移入未分�?
			int64 iCount = 0;
			sqlite3_bind_int64(stmt_auth_sum, 1, id);
			while ( sqlite3_step(stmt_auth_sum) == SQLITE_ROW ) {
				iCount = sqlite3_column_int64(stmt_auth_sum, 0);
			}
			sqlite3_reset(stmt_auth_sum);
			if ( iCount > 0 ) {
				sqlite3_bind_int64(stmt_auth_mov, 1, id);
				sqlite3_step(stmt_auth_mov);
				sqlite3_reset(stmt_auth_mov);
			}
			
			// 删除权限�?
			sqlite3_bind_int64(stmt_auth_del, 1, id);
			sqlite3_step(stmt_auth_del);
			sqlite3_reset(stmt_auth_del);
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限组删除成功！\"}", 0);
			
			// 刷新权限分组 + 权限分类缓存
			ReloadCache_Auth_Auth();
			ReloadCache_Auth_Group();
			
		} else if ( id == 1 ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"不能删除默认权限组！\"}", 0);
		} else {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的权限组ID\"}", 0);
		}
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}





// 获取 uris 管理页面视图
void Request_View_Auth_URIs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 登录页面
		LoadPage(objResp, 200, HTTP_CT_HTML, "auth/uris.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}

// 获取 uris 编辑页面视图
void Request_View_Auth_URIs_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 获取参数
		char sID[24];
		HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
		int64 id = xrtStrToI64(sID);
		
		// 读取数据库中的记录，构建模板�?
		xvalue tblInfo = xvoCreateTable();
		int bRow = FALSE;
		sqlite3_bind_int64(stmt_uris_get, 1, id);
		while ( sqlite3_step(stmt_uris_get) == SQLITE_ROW ) {
			xvoTableSetInt(tblInfo, "id", 2, id);
			xvoTableSetInt(tblInfo, "authID", 6, sqlite3_column_int64(stmt_uris_get, 1));
			xvoTableSetText(tblInfo, "uri", 3, (str)sqlite3_column_text(stmt_uris_get, 2), 0, FALSE);
			xvoTableSetText(tblInfo, "desc", 4, (str)sqlite3_column_text(stmt_uris_get, 3), 0, FALSE);
			// �?个字�? isBackend(4), needAuth(5), needLog(6), keepActive(7)
			xvoTableSetInt(tblInfo, "isBackend", 9, sqlite3_column_int64(stmt_uris_get, 4));
			xvoTableSetInt(tblInfo, "needAuth", 8, sqlite3_column_int64(stmt_uris_get, 5));
			xvoTableSetInt(tblInfo, "needLog", 7, sqlite3_column_int64(stmt_uris_get, 6));
			xvoTableSetInt(tblInfo, "keepActive", 10, sqlite3_column_int64(stmt_uris_get, 7));
			xvoTableSetInt(tblInfo, "sort", 4, sqlite3_column_int64(stmt_uris_get, 8));
			xvoTableSetValue(tblInfo, "authList", 8, G_CACHE_Auth, FALSE);
			xvoTableSetValue(tblInfo, "memberAuthList", 14, G_CACHE_MemberAuth, FALSE);
			bRow = TRUE;
		}
		sqlite3_reset(stmt_uris_get);
		
		// 构建页面并返�?
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("auth/uris_edit.html", tblInfo, &iSize);
		xvoUnref(tblInfo);
		http_reply(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// uris 主接�?
void Request_Auth_URIs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// �?URL 查询字符串中提取参数
		char sParam[64];
		HttpGetQueryVar(objReq, "page", sParam, sizeof(sParam));
		int64 iPage = xrtStrToI64(sParam);
		if ( iPage <= 0 ) { iPage = 1; }
		HttpGetQueryVar(objReq, "limit", sParam, sizeof(sParam));
		int64 iLimit = xrtStrToI64(sParam);
		if ( iLimit <= 0 ) { iLimit = 10; }
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = HttpGetQueryVar(objReq, "search", sParam, sizeof(sParam));
		
		// 从数据库中查询数�?
		xvalue data = xvoCreateArray();
		int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_int64(stmt_uris_all, 1, iLimit);
			sqlite3_bind_int64(stmt_uris_all, 2, iOffset);
			while ( sqlite3_step(stmt_uris_all) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_uris_all, 0));
				xvoTableSetInt(tblRow, "authID", 6, sqlite3_column_int64(stmt_uris_all, 1));
				xvoTableSetText(tblRow, "uri", 3, (str)sqlite3_column_text(stmt_uris_all, 2), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt_uris_all, 3), 0, FALSE);
				// �?个字�? isBackend(4), needAuth(5), needLog(6), keepActive(7)
				xvoTableSetInt(tblRow, "isBackend", 9, sqlite3_column_int64(stmt_uris_all, 4));
				xvoTableSetInt(tblRow, "needAuth", 8, sqlite3_column_int64(stmt_uris_all, 5));
				xvoTableSetInt(tblRow, "needLog", 7, sqlite3_column_int64(stmt_uris_all, 6));
				xvoTableSetInt(tblRow, "keepActive", 10, sqlite3_column_int64(stmt_uris_all, 7));
				xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int64(stmt_uris_all, 8));
				xtime iTime = sqlite3_column_int64(stmt_uris_all, 9);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iTime = sqlite3_column_int64(stmt_uris_all, 10);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				xvoTableSetText(tblRow, "authName", 8, (str)sqlite3_column_text(stmt_uris_all, 11), 0, FALSE);
				xvoTableSetText(tblRow, "memberAuthName", 14, (str)sqlite3_column_text(stmt_uris_all, 12), 0, FALSE);
				if ( iCount <= 0 ) {
					iCount = sqlite3_column_int64(stmt_uris_all, 13);
				}
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_uris_all);
		} else {
			// 筛�?
			sqlite3_bind_text(stmt_uris_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_text(stmt_uris_sel, 2, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_uris_sel, 3, iLimit);
			sqlite3_bind_int64(stmt_uris_sel, 4, iOffset);
			while ( sqlite3_step(stmt_uris_sel) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_uris_sel, 0));
				xvoTableSetInt(tblRow, "authID", 6, sqlite3_column_int64(stmt_uris_sel, 1));
				xvoTableSetText(tblRow, "uri", 3, (str)sqlite3_column_text(stmt_uris_sel, 2), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt_uris_sel, 3), 0, FALSE);
				// �?个字�? isBackend(4), needAuth(5), needLog(6), keepActive(7)
				xvoTableSetInt(tblRow, "isBackend", 9, sqlite3_column_int64(stmt_uris_sel, 4));
				xvoTableSetInt(tblRow, "needAuth", 8, sqlite3_column_int64(stmt_uris_sel, 5));
				xvoTableSetInt(tblRow, "needLog", 7, sqlite3_column_int64(stmt_uris_sel, 6));
				xvoTableSetInt(tblRow, "keepActive", 10, sqlite3_column_int64(stmt_uris_sel, 7));
				xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int64(stmt_uris_sel, 8));
				xtime iTime = sqlite3_column_int64(stmt_uris_sel, 9);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iTime = sqlite3_column_int64(stmt_uris_sel, 10);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				xvoTableSetText(tblRow, "authName", 8, (str)sqlite3_column_text(stmt_uris_sel, 11), 0, FALSE);
				xvoTableSetText(tblRow, "memberAuthName", 14, (str)sqlite3_column_text(stmt_uris_sel, 12), 0, FALSE);
				if ( iCount <= 0 ) {
					iCount = sqlite3_column_int64(stmt_uris_sel, 13);
				}
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_uris_sel);
		}
		
		// 构建返回�?
		xvalue tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetInt(tblRet, "code", 4, 0);
		xvoTableSetInt(tblRet, "count", 5, iCount);
		xvoTableSetText(tblRet, "message", 7, "�ӿ����ݻ�ȡ�ɹ���", 0, FALSE);
		xvoTableSetValue(tblRet, "data", 4, data, TRUE);
		
		// 生成 JSON
		size_t iRetSize = 0;
		char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
		http_reply(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xvoUnref(tblRet);
		
	} else if ( HttpMethodIs(objReq, "PUT") ) {
		
		// 更新接口信息
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0);
			xvoUnref(tblForm);
			return;
		}
		int64 id = xvoTableGetInt(tblForm, "id", 2);
		int64 authID = xvoTableGetInt(tblForm, "authID", 6);
		str uri = xvoTableGetText(tblForm, "uri", 3);
		str desc = xvoTableGetText(tblForm, "desc", 4);
		int64 sort = xvoTableGetInt(tblForm, "sort", 4);
		// �?个字�?
		int64 isBackend = xvoTableGetInt(tblForm, "isBackend", 9);
		int64 needAuth = xvoTableGetInt(tblForm, "needAuth", 8);
		int64 needLog = xvoTableGetInt(tblForm, "needLog", 7);
		int64 keepActive = xvoTableGetInt(tblForm, "keepActive", 10);
		
		// UPDATE uris SET authID=?, desc=?, sort=?, isBackend=?, needAuth=?, needLog=?, keepActive=?, updateTime=? WHERE id=?
		sqlite3_bind_int64(stmt_uris_put, 1, authID);
		sqlite3_bind_text(stmt_uris_put, 2, desc, strlen(desc), SQLITE_STATIC);
		sqlite3_bind_int64(stmt_uris_put, 3, sort);
		sqlite3_bind_int64(stmt_uris_put, 4, isBackend);
		sqlite3_bind_int64(stmt_uris_put, 5, needAuth);
		sqlite3_bind_int64(stmt_uris_put, 6, needLog);
		sqlite3_bind_int64(stmt_uris_put, 7, keepActive);
		sqlite3_bind_int64(stmt_uris_put, 8, xrtNow());
		sqlite3_bind_int64(stmt_uris_put, 9, id);
		sqlite3_step(stmt_uris_put);
		sqlite3_reset(stmt_uris_put);
		xvoUnref(tblForm);
		
		// 更新路由表和权限缓存
		RouteInfo* pInfo = xrtDictGet(G_StaticRouteTableHTTP, uri, 0);
		if ( pInfo ) {
			pInfo->AuthID = authID;
			pInfo->bAdmin = isBackend ? TRUE : FALSE;
			pInfo->bAuth = needAuth ? TRUE : FALSE;
			pInfo->bPutLog = needLog ? TRUE : FALSE;
			pInfo->bActive = keepActive ? TRUE : FALSE;
		}
		Auth_ReloadCache();
		
		// 响应请求
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"接口信息修改成功！\"}", 0);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}


