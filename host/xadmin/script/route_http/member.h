



// 后台管理前台用户的路由处�?- /admin/member/*



// ==================== 前台用户管理 ====================

void Request_View_Member_User(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		LoadPage(objResp, 200, HTTP_CT_HTML, "member/user.html");
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_View_Member_User_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		xvalue tblInfo = xvoCreateTable();
		xvoTableSetValue(tblInfo, "groupList", 9, G_CACHE_MemberGroup, FALSE);
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("member/user_add.html", tblInfo, &iSize);
		xvoUnref(tblInfo);
		http_reply(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_View_Member_User_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		char sID[24];
		HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
		int64 id = xrtStrToI64(sID);
		
		xvalue tblInfo = xvoCreateTable();
		sqlite3_bind_int64(stmt_member_get, 1, id);
		if ( sqlite3_step(stmt_member_get) == SQLITE_ROW ) {
			xvoTableSetInt(tblInfo, "id", 2, sqlite3_column_int64(stmt_member_get, 0));
			xvoTableSetText(tblInfo, "username", 8, (str)sqlite3_column_text(stmt_member_get, 1), 0, FALSE);
			xvoTableSetInt(tblInfo, "groupId", 7, sqlite3_column_int64(stmt_member_get, 2));
			xvoTableSetInt(tblInfo, "authLevel", 9, sqlite3_column_int64(stmt_member_get, 3));
			xvoTableSetInt(tblInfo, "balance", 7, sqlite3_column_int64(stmt_member_get, 4));
			xvoTableSetText(tblInfo, "nickname", 8, (str)sqlite3_column_text(stmt_member_get, 5), 0, FALSE);
			xvoTableSetText(tblInfo, "email", 5, (str)sqlite3_column_text(stmt_member_get, 6), 0, FALSE);
			xvoTableSetText(tblInfo, "phone", 5, (str)sqlite3_column_text(stmt_member_get, 7), 0, FALSE);
			xvoTableSetInt(tblInfo, "status", 6, sqlite3_column_int64(stmt_member_get, 9));
		}
		sqlite3_reset(stmt_member_get);
		xvoTableSetValue(tblInfo, "groupList", 9, G_CACHE_MemberGroup, FALSE);
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("member/user_edit.html", tblInfo, &iSize);
		xvoUnref(tblInfo);
		http_reply(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_View_Member_User_Balance(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		char sID[24];
		HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
		int64 id = xrtStrToI64(sID);
		
		xvalue tblInfo = xvoCreateTable();
		sqlite3_bind_int64(stmt_member_get, 1, id);
		if ( sqlite3_step(stmt_member_get) == SQLITE_ROW ) {
			xvoTableSetInt(tblInfo, "id", 2, sqlite3_column_int64(stmt_member_get, 0));
			xvoTableSetText(tblInfo, "username", 8, (str)sqlite3_column_text(stmt_member_get, 1), 0, FALSE);
			int64 balance = sqlite3_column_int64(stmt_member_get, 4);
			xvoTableSetInt(tblInfo, "balance", 7, balance);
			// 转换为元，保�?位小�?
			char sBalanceYuan[32];
			snprintf(sBalanceYuan, sizeof(sBalanceYuan), "%.2f", balance / 100.0);
			xvoTableSetText(tblInfo, "balanceYuan", 11, sBalanceYuan, 0, FALSE);
		}
		sqlite3_reset(stmt_member_get);
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("member/user_balance.html", tblInfo, &iSize);
		xvoUnref(tblInfo);
		http_reply(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_Member_User(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		char sParam[64];
		HttpGetQueryVar(objReq, "page", sParam, sizeof(sParam));
		int64 iPage = xrtStrToI64(sParam);
		if ( iPage <= 0 ) iPage = 1;
		HttpGetQueryVar(objReq, "limit", sParam, sizeof(sParam));
		int64 iLimit = xrtStrToI64(sParam);
		if ( iLimit <= 0 ) iLimit = 10;
		int64 iOffset = (iPage - 1) * iLimit;
		
		xvalue data = xvoCreateArray();
		int64 iCount = 0;
		
		sqlite3_bind_int64(stmt_member_all, 1, iLimit);
		sqlite3_bind_int64(stmt_member_all, 2, iOffset);
		while ( sqlite3_step(stmt_member_all) == SQLITE_ROW ) {
			xvalue tblRow = xvoCreateTable();
			xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_member_all, 0));
			xvoTableSetText(tblRow, "username", 8, (str)sqlite3_column_text(stmt_member_all, 1), 0, FALSE);
			xvoTableSetInt(tblRow, "groupId", 7, sqlite3_column_int64(stmt_member_all, 2));
			xvoTableSetInt(tblRow, "authLevel", 9, sqlite3_column_int64(stmt_member_all, 3));
			xvoTableSetInt(tblRow, "balance", 7, sqlite3_column_int64(stmt_member_all, 4));
			xvoTableSetText(tblRow, "nickname", 8, (str)sqlite3_column_text(stmt_member_all, 5), 0, FALSE);
			xvoTableSetInt(tblRow, "status", 6, sqlite3_column_int64(stmt_member_all, 9));
			xtime iTime = sqlite3_column_int64(stmt_member_all, 10);
			xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
			xvoArrayAppendValue(data, tblRow, TRUE);
		}
		sqlite3_reset(stmt_member_all);
		
		sqlite3_stmt* stmt_count;
		sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM member WHERE isDelete = 0", -1, 0, &stmt_count, NULL);
		if ( sqlite3_step(stmt_count) == SQLITE_ROW ) iCount = sqlite3_column_int64(stmt_count, 0);
		sqlite3_finalize(stmt_count);
		
		xvalue tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetInt(tblRet, "code", 4, 0);
		xvoTableSetInt(tblRet, "count", 5, iCount);
		xvoTableSetValue(tblRet, "data", 4, data, TRUE);
		size_t iRetSize = 0;
		char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
		http_reply(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xvoUnref(tblRet);
		
	} else if ( HttpMethodIs(objReq, "POST") ) {
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xvoUnref(tblForm); return; }
		str username = xvoTableGetText(tblForm, "username", 8);
		str password = xvoTableGetText(tblForm, "password", 8);
		str nickname = xvoTableGetText(tblForm, "nickname", 8);
		int64 groupId = xvoTableGetInt(tblForm, "groupId", 7);
		int64 authLevel = xvoTableGetInt(tblForm, "authLevel", 9);
		int64 status = xvoTableGetInt(tblForm, "status", 6);
		if ( !username || strlen(username) == 0 ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名不能为空！\"}", 0); xvoUnref(tblForm); return; }
		if ( !password || strlen(password) == 0 ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"密码不能为空！\"}", 0); xvoUnref(tblForm); return; }
		if ( groupId < 1 ) groupId = 1;
		if ( status != 0 && status != 1 ) status = 1;
		
		sqlite3_bind_text(stmt_member_chk, 1, username, -1, NULL);
		int iCount = 0;
		if ( sqlite3_step(stmt_member_chk) == SQLITE_ROW ) iCount = sqlite3_column_int(stmt_member_chk, 0);
		sqlite3_reset(stmt_member_chk);
		if ( iCount > 0 ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名已存在！\"}", 0); xvoUnref(tblForm); return; }
		
		str sSalt = xrtMakeXIDS();
		str sPwdHash = ServerHashPassword(username, sSalt, password);
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
		sqlite3_step(stmt_member_add);
		int64 newId = sqlite3_last_insert_rowid(G_DB);
		sqlite3_reset(stmt_member_add);
		xrtFree(sSalt); xrtFree(sPwdHash); xvoUnref(tblForm);
		mg_http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"前台用户添加成功！\", \"data\": {\"id\": %lld}}", newId);
		
	} else if ( HttpMethodIs(objReq, "PUT") ) {
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xvoUnref(tblForm); return; }
		int64 id = xvoTableGetInt(tblForm, "id", 2);
		int64 groupId = xvoTableGetInt(tblForm, "groupId", 7);
		int64 authLevel = xvoTableGetInt(tblForm, "authLevel", 9);
		str nickname = xvoTableGetText(tblForm, "nickname", 8);
		str email = xvoTableGetText(tblForm, "email", 5);
		str phone = xvoTableGetText(tblForm, "phone", 5);
		str avatar = xvoTableGetText(tblForm, "avatar", 6);
		int64 status = xvoTableGetInt(tblForm, "status", 6);
		if ( groupId < 1 ) groupId = 1;
		xtime now = xrtNow();
		sqlite3_bind_int64(stmt_member_put, 1, groupId);
		sqlite3_bind_int64(stmt_member_put, 2, authLevel);
		sqlite3_bind_text(stmt_member_put, 3, nickname ? nickname : (str)"", -1, NULL);
		sqlite3_bind_text(stmt_member_put, 4, email ? email : (str)"", -1, NULL);
		sqlite3_bind_text(stmt_member_put, 5, phone ? phone : (str)"", -1, NULL);
		sqlite3_bind_text(stmt_member_put, 6, avatar ? avatar : (str)"", -1, NULL);
		sqlite3_bind_int(stmt_member_put, 7, status);
		sqlite3_bind_int64(stmt_member_put, 8, now);
		sqlite3_bind_int64(stmt_member_put, 9, id);
		sqlite3_step(stmt_member_put); sqlite3_reset(stmt_member_put); xvoUnref(tblForm);
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"前台用户更新成功！\"}", 0);
		
	} else if ( HttpMethodIs(objReq, "DELETE") ) {
		char sID[24];
		HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
		int64 id = xrtStrToI64(sID);
		if ( id > 0 ) {
			xtime now = xrtNow();
			sqlite3_bind_int64(stmt_member_del, 1, now);
			sqlite3_bind_int64(stmt_member_del, 2, id);
			sqlite3_step(stmt_member_del); sqlite3_reset(stmt_member_del);
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"前台用户删除成功！\"}", 0);
		} else {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的用户ID\"}", 0);
		}
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_Member_User_Repwd(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "POST") ) {
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xvoUnref(tblForm); return; }
		int64 id = xvoTableGetInt(tblForm, "id", 2);
		str username = xvoTableGetText(tblForm, "username", 8);
		str password = xvoTableGetText(tblForm, "password", 8);
		if ( !username || strlen(username) == 0 ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"用户名不能为空！\"}", 0); xvoUnref(tblForm); return; }
		if ( !password || strlen(password) == 0 ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"密码不能为空！\"}", 0); xvoUnref(tblForm); return; }
		str sSalt = xrtMakeXIDS();
		str sPwdHash = ServerHashPassword(username, sSalt, password);
		xtime now = xrtNow();
		sqlite3_bind_text(stmt_member_pwd, 1, sSalt, -1, NULL);
		sqlite3_bind_text(stmt_member_pwd, 2, sPwdHash, -1, NULL);
		sqlite3_bind_int64(stmt_member_pwd, 3, now);
		sqlite3_bind_int64(stmt_member_pwd, 4, id);
		sqlite3_step(stmt_member_pwd); sqlite3_reset(stmt_member_pwd);
		xrtFree(sSalt); xrtFree(sPwdHash); xvoUnref(tblForm);
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"密码重置成功！\"}", 0);
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_Member_User_Balance(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "POST") ) {
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xvoUnref(tblForm); return; }
		int64 memberId = xvoTableGetInt(tblForm, "id", 2);
		int type = xvoTableGetInt(tblForm, "type", 4);
		int64 amount = xvoTableGetInt(tblForm, "amount", 6);
		str remark = xvoTableGetText(tblForm, "remark", 6);
		str operator = xvoTableGetText(objSession, "user", 4);
		bool bOK = Member_ChangeBalance(memberId, type, amount, remark, operator);
		xvoUnref(tblForm);
		if ( bOK ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"余额调整成功！\"}", 0);
		} else {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"余额调整失败！\"}", 0);
		}
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}



// ==================== 前台用户组管�?====================

void Request_View_Member_Group(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) { LoadPage(objResp, 200, HTTP_CT_HTML, "member/group.html"); }
	else { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); }
}

void Request_View_Member_Group_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		xvalue tblInfo = xvoCreateTable();
		xvoTableSetValue(tblInfo, "authGroups", 10, G_CACHE_MemberAuthGroup, FALSE);
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("member/group_add.html", tblInfo, &iSize);
		xvoUnref(tblInfo);
		http_reply(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
	} else { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); }
}

void Request_View_Member_Group_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		char sID[24];
		HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
		int64 id = xrtStrToI64(sID);
		xvalue tblInfo = xvoCreateTable();
		xvalue listAuth = xvoCreateList();
		sqlite3_bind_int64(stmt_mgroup_get, 1, id);
		if ( sqlite3_step(stmt_mgroup_get) == SQLITE_ROW ) {
			xvoTableSetInt(tblInfo, "id", 2, id);
			xvoTableSetText(tblInfo, "name", 4, (str)sqlite3_column_text(stmt_mgroup_get, 1), 0, FALSE);
			xvoTableSetText(tblInfo, "desc", 4, (str)sqlite3_column_text(stmt_mgroup_get, 2), 0, FALSE);
			xvoTableSetInt(tblInfo, "authLevel", 9, sqlite3_column_int64(stmt_mgroup_get, 4));
			str sAuthList = (str)sqlite3_column_text(stmt_mgroup_get, 3);
			if ( sAuthList && strlen(sAuthList) > 2 ) {
				xvalue arrAuth = xrtParseJSON(sAuthList, 0);
				if ( arrAuth && arrAuth->Type == XVO_DT_ARRAY ) {
					for ( int i = 0; i < arrAuth->vArray->Count; i++ ) {
						int64 authID = xvoArrayGetInt(arrAuth, i);
						if ( authID > 0 ) xvoListSetBool(listAuth, authID, TRUE);
					}
				}
				xvoUnref(arrAuth);
			}
		}
		sqlite3_reset(stmt_mgroup_get);
		xvalue arrAuthGroups = xvoDeepCopy(G_CACHE_MemberAuthGroup);
		for ( int g = 1; g <= arrAuthGroups->vArray->Count; g++ ) {
			xvalue tblGroup = xrtPtrArrayGet_Inline(arrAuthGroups->vArray, g);
			xvalue arrAuths = xvoTableGetValue(tblGroup, "auths", 5);
			if ( arrAuths && arrAuths->Type == XVO_DT_ARRAY ) {
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
		size_t iSize = 0;
		str sPage = MakePageWithTemplate("member/group_edit.html", tblInfo, &iSize);
		xvoUnref(tblInfo);
		http_reply(objResp, 200, HTTP_CT_HTML, sPage, iSize);
		xrtFree(sPage);
	} else { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); }
}

void Request_Member_Group(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		char sParam[64];
		HttpGetQueryVar(objReq, "page", sParam, sizeof(sParam));
		int64 iPage = xrtStrToI64(sParam); if ( iPage <= 0 ) iPage = 1;
		HttpGetQueryVar(objReq, "limit", sParam, sizeof(sParam));
		int64 iLimit = xrtStrToI64(sParam); if ( iLimit <= 0 ) iLimit = 10;
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = HttpGetQueryVar(objReq, "search", sParam, sizeof(sParam));
		
		xvalue data = xvoCreateArray(); int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_int64(stmt_mgroup_all, 1, iLimit);
			sqlite3_bind_int64(stmt_mgroup_all, 2, iOffset);
			while ( sqlite3_step(stmt_mgroup_all) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable(); int64 rowId = sqlite3_column_int64(stmt_mgroup_all, 0);
				xvoTableSetInt(tblRow, "id", 2, rowId);
				xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt_mgroup_all, 1), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt_mgroup_all, 2), 0, FALSE);
				// 解析 authList 获取权限数量
				str authList = (str)sqlite3_column_text(stmt_mgroup_all, 3);
				int64 authCount = 0;
				if ( authList && strlen(authList) > 2 ) {
					xvalue arrAuth = xrtParseJSON(authList, strlen(authList));
					if ( (arrAuth) && (arrAuth->Type == XVO_DT_ARRAY) ) authCount = xvoArrayItemCount(arrAuth);
					if ( arrAuth ) xvoUnref(arrAuth);
				}
				xvoTableSetInt(tblRow, "authCount", 9, authCount);
				xvoTableSetInt(tblRow, "authLevel", 9, sqlite3_column_int64(stmt_mgroup_all, 4));
				xtime iTime = sqlite3_column_int64(stmt_mgroup_all, 5);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iTime = sqlite3_column_int64(stmt_mgroup_all, 6);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				sqlite3_bind_int64(stmt_mgroup_sum, 1, rowId);
				if ( sqlite3_step(stmt_mgroup_sum) == SQLITE_ROW ) xvoTableSetInt(tblRow, "userCount", 9, sqlite3_column_int64(stmt_mgroup_sum, 0));
				sqlite3_reset(stmt_mgroup_sum);
				if ( iCount <= 0 ) iCount = sqlite3_column_int64(stmt_mgroup_all, 7);
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_mgroup_all);
		} else {
			// 筛选查�?
			sqlite3_bind_text(stmt_mgroup_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_mgroup_sel, 2, iLimit);
			sqlite3_bind_int64(stmt_mgroup_sel, 3, iOffset);
			while ( sqlite3_step(stmt_mgroup_sel) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable(); int64 rowId = sqlite3_column_int64(stmt_mgroup_sel, 0);
				xvoTableSetInt(tblRow, "id", 2, rowId);
				xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt_mgroup_sel, 1), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt_mgroup_sel, 2), 0, FALSE);
				// 解析 authList 获取权限数量
				str authList = (str)sqlite3_column_text(stmt_mgroup_sel, 3);
				int64 authCount = 0;
				if ( authList && strlen(authList) > 2 ) {
					xvalue arrAuth = xrtParseJSON(authList, strlen(authList));
					if ( (arrAuth) && (arrAuth->Type == XVO_DT_ARRAY) ) authCount = xvoArrayItemCount(arrAuth);
					if ( arrAuth ) xvoUnref(arrAuth);
				}
				xvoTableSetInt(tblRow, "authCount", 9, authCount);
				xvoTableSetInt(tblRow, "authLevel", 9, sqlite3_column_int64(stmt_mgroup_sel, 4));
				xtime iTime = sqlite3_column_int64(stmt_mgroup_sel, 5);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iTime = sqlite3_column_int64(stmt_mgroup_sel, 6);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				sqlite3_bind_int64(stmt_mgroup_sum, 1, rowId);
				if ( sqlite3_step(stmt_mgroup_sum) == SQLITE_ROW ) xvoTableSetInt(tblRow, "userCount", 9, sqlite3_column_int64(stmt_mgroup_sum, 0));
				sqlite3_reset(stmt_mgroup_sum);
				if ( iCount <= 0 ) iCount = sqlite3_column_int64(stmt_mgroup_sel, 7);
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_mgroup_sel);
		}
		
		xvalue tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetInt(tblRet, "code", 4, 0);
		xvoTableSetInt(tblRet, "count", 5, iCount);
		xvoTableSetValue(tblRet, "data", 4, data, TRUE);
		size_t iRetSize = 0;
		char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
		http_reply(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet); xvoUnref(tblRet);
	} else if ( HttpMethodIs(objReq, "POST") ) {
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xvoUnref(tblForm); return; }
		str name = xvoTableGetText(tblForm, "name", 4);
		str desc = xvoTableGetText(tblForm, "desc", 4);
		str authList = xvoTableGetText(tblForm, "authList", 8);
		int64 authLevel = xvoTableGetInt(tblForm, "authLevel", 9);
		if ( !authList || strlen(authList) == 0 ) authList = "[]";
		xtime now = xrtNow();
		sqlite3_bind_text(stmt_mgroup_add, 1, name, -1, NULL);
		sqlite3_bind_text(stmt_mgroup_add, 2, desc ? desc : (str)"", -1, NULL);
		sqlite3_bind_text(stmt_mgroup_add, 3, authList, -1, NULL);
		sqlite3_bind_int64(stmt_mgroup_add, 4, authLevel);
		sqlite3_bind_int64(stmt_mgroup_add, 5, now);
		sqlite3_bind_int64(stmt_mgroup_add, 6, now);
		sqlite3_step(stmt_mgroup_add);
		int64 newId = sqlite3_last_insert_rowid(G_DB);
		sqlite3_reset(stmt_mgroup_add); xvoUnref(tblForm);
		mg_http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"用户组添加成功！\", \"data\": {\"id\": %lld}}", newId);
		MemberAuth_ReloadCache();
	} else if ( HttpMethodIs(objReq, "PUT") ) {
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xvoUnref(tblForm); return; }
		int64 id = xvoTableGetInt(tblForm, "id", 2);
		str name = xvoTableGetText(tblForm, "name", 4);
		str desc = xvoTableGetText(tblForm, "desc", 4);
		str authList = xvoTableGetText(tblForm, "authList", 8);
		int64 authLevel = xvoTableGetInt(tblForm, "authLevel", 9);
		if ( !authList || strlen(authList) == 0 ) authList = "[]";
		xtime now = xrtNow();
		sqlite3_bind_text(stmt_mgroup_put, 1, name, -1, NULL);
		sqlite3_bind_text(stmt_mgroup_put, 2, desc ? desc : (str)"", -1, NULL);
		sqlite3_bind_text(stmt_mgroup_put, 3, authList, -1, NULL);
		sqlite3_bind_int64(stmt_mgroup_put, 4, authLevel);
		sqlite3_bind_int64(stmt_mgroup_put, 5, now);
		sqlite3_bind_int64(stmt_mgroup_put, 6, id);
		sqlite3_step(stmt_mgroup_put); sqlite3_reset(stmt_mgroup_put); xvoUnref(tblForm);
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"用户组更新成功！\"}", 0);
		MemberAuth_ReloadCache();
	} else if ( HttpMethodIs(objReq, "DELETE") ) {
		char sID[24];
		HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
		int64 id = xrtStrToI64(sID);
		if ( id > 1 ) {
			sqlite3_bind_int64(stmt_mgroup_sum, 1, id);
			int64 userCount = 0;
			if ( sqlite3_step(stmt_mgroup_sum) == SQLITE_ROW ) userCount = sqlite3_column_int64(stmt_mgroup_sum, 0);
			sqlite3_reset(stmt_mgroup_sum);
			if ( userCount > 0 ) {
				http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无法删除此用户组，因为它关联了用户！\"}", 0);
			} else {
				xtime now = xrtNow();
				sqlite3_bind_int64(stmt_mgroup_del, 1, now);
				sqlite3_bind_int64(stmt_mgroup_del, 2, id);
				sqlite3_step(stmt_mgroup_del); sqlite3_reset(stmt_mgroup_del);
				http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"用户组删除成功！\"}", 0);
				MemberAuth_ReloadCache();
			}
		} else if ( id == 1 ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"不能删除默认用户组！\"}", 0); }
		else { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的用户组ID\"}", 0); }
	} else { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); }
}



// ==================== 前台权限分类管理 ====================

void Request_View_Member_AuthGroup(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{ if ( HttpMethodIs(objReq, "GET") ) LoadPage(objResp, 200, HTTP_CT_HTML, "member/authgroup.html"); else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); }

void Request_View_Member_AuthGroup_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{ if ( HttpMethodIs(objReq, "GET") ) LoadPage(objResp, 200, HTTP_CT_HTML, "member/authgroup_add.html"); else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); }

void Request_View_Member_AuthGroup_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		char sID[24]; HttpGetQueryVar(objReq, "id", sID, sizeof(sID)); int64 id = xrtStrToI64(sID);
		xvalue tblInfo = xvoCreateTable();
		sqlite3_bind_int64(stmt_magroup_get, 1, id);
		if ( sqlite3_step(stmt_magroup_get) == SQLITE_ROW ) {
			xvoTableSetInt(tblInfo, "id", 2, id);
			xvoTableSetText(tblInfo, "name", 4, (str)sqlite3_column_text(stmt_magroup_get, 1), 0, FALSE);
			xvoTableSetText(tblInfo, "desc", 4, (str)sqlite3_column_text(stmt_magroup_get, 2), 0, FALSE);
			xvoTableSetInt(tblInfo, "sort", 4, sqlite3_column_int64(stmt_magroup_get, 3));
		}
		sqlite3_reset(stmt_magroup_get);
		size_t iSize = 0; str sPage = MakePageWithTemplate("member/authgroup_edit.html", tblInfo, &iSize);
		xvoUnref(tblInfo); http_reply(objResp, 200, HTTP_CT_HTML, sPage, iSize); xrtFree(sPage);
	} else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

void Request_Member_AuthGroup(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		char sParam[64]; HttpGetQueryVar(objReq, "page", sParam, sizeof(sParam)); int64 iPage = xrtStrToI64(sParam); if ( iPage <= 0 ) iPage = 1;
		HttpGetQueryVar(objReq, "limit", sParam, sizeof(sParam)); int64 iLimit = xrtStrToI64(sParam); if ( iLimit <= 0 ) iLimit = 10;
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = HttpGetQueryVar(objReq, "search", sParam, sizeof(sParam));
		
		xvalue data = xvoCreateArray(); int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_int64(stmt_magroup_all, 1, iLimit); sqlite3_bind_int64(stmt_magroup_all, 2, iOffset);
			while ( sqlite3_step(stmt_magroup_all) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable(); int64 rowId = sqlite3_column_int64(stmt_magroup_all, 0);
				xvoTableSetInt(tblRow, "id", 2, rowId);
				xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt_magroup_all, 1), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt_magroup_all, 2), 0, FALSE);
				xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int64(stmt_magroup_all, 3));
				xtime iTime = sqlite3_column_int64(stmt_magroup_all, 4);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iTime = sqlite3_column_int64(stmt_magroup_all, 5);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				sqlite3_bind_int64(stmt_magroup_sum, 1, rowId);
				if ( sqlite3_step(stmt_magroup_sum) == SQLITE_ROW ) xvoTableSetInt(tblRow, "authCount", 9, sqlite3_column_int64(stmt_magroup_sum, 0));
				sqlite3_reset(stmt_magroup_sum);
				if ( iCount <= 0 ) iCount = sqlite3_column_int64(stmt_magroup_all, 6);
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_magroup_all);
		} else {
			// 筛选查�?
			sqlite3_bind_text(stmt_magroup_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_magroup_sel, 2, iLimit); sqlite3_bind_int64(stmt_magroup_sel, 3, iOffset);
			while ( sqlite3_step(stmt_magroup_sel) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable(); int64 rowId = sqlite3_column_int64(stmt_magroup_sel, 0);
				xvoTableSetInt(tblRow, "id", 2, rowId);
				xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt_magroup_sel, 1), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt_magroup_sel, 2), 0, FALSE);
				xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int64(stmt_magroup_sel, 3));
				xtime iTime = sqlite3_column_int64(stmt_magroup_sel, 4);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iTime = sqlite3_column_int64(stmt_magroup_sel, 5);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				sqlite3_bind_int64(stmt_magroup_sum, 1, rowId);
				if ( sqlite3_step(stmt_magroup_sum) == SQLITE_ROW ) xvoTableSetInt(tblRow, "authCount", 9, sqlite3_column_int64(stmt_magroup_sum, 0));
				sqlite3_reset(stmt_magroup_sum);
				if ( iCount <= 0 ) iCount = sqlite3_column_int64(stmt_magroup_sel, 6);
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_magroup_sel);
		}
		
		xvalue tblRet = xvoCreateTable(); xvoTableSetBool(tblRet, "result", 6, TRUE); xvoTableSetInt(tblRet, "code", 4, 0);
		xvoTableSetInt(tblRet, "count", 5, iCount); xvoTableSetValue(tblRet, "data", 4, data, TRUE);
		size_t iRetSize = 0; char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
		http_reply(objResp, 200, HTTP_CT_JSON, sRet, iRetSize); xrtFree(sRet); xvoUnref(tblRet);
	} else if ( HttpMethodIs(objReq, "POST") ) {
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xvoUnref(tblForm); return; }
		str name = xvoTableGetText(tblForm, "name", 4); str desc = xvoTableGetText(tblForm, "desc", 4); int64 sort = xvoTableGetInt(tblForm, "sort", 4);
		xtime now = xrtNow(); sqlite3_bind_text(stmt_magroup_add, 1, name, -1, NULL); sqlite3_bind_text(stmt_magroup_add, 2, desc ? desc : (str)"", -1, NULL);
		sqlite3_bind_int64(stmt_magroup_add, 3, sort); sqlite3_bind_int64(stmt_magroup_add, 4, now); sqlite3_bind_int64(stmt_magroup_add, 5, now);
		sqlite3_step(stmt_magroup_add); int64 newId = sqlite3_last_insert_rowid(G_DB); sqlite3_reset(stmt_magroup_add); xvoUnref(tblForm);
		mg_http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分类添加成功！\", \"data\": {\"id\": %lld}}", newId);
		ReloadCache_MemberAuthGroup();
	} else if ( HttpMethodIs(objReq, "PUT") ) {
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xvoUnref(tblForm); return; }
		int64 id = xvoTableGetInt(tblForm, "id", 2); str name = xvoTableGetText(tblForm, "name", 4); str desc = xvoTableGetText(tblForm, "desc", 4); int64 sort = xvoTableGetInt(tblForm, "sort", 4);
		xtime now = xrtNow(); sqlite3_bind_text(stmt_magroup_put, 1, name, -1, NULL); sqlite3_bind_text(stmt_magroup_put, 2, desc ? desc : (str)"", -1, NULL);
		sqlite3_bind_int64(stmt_magroup_put, 3, sort); sqlite3_bind_int64(stmt_magroup_put, 4, now); sqlite3_bind_int64(stmt_magroup_put, 5, id);
		sqlite3_step(stmt_magroup_put); sqlite3_reset(stmt_magroup_put); xvoUnref(tblForm);
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分类更新成功！\"}", 0); ReloadCache_MemberAuthGroup();
	} else if ( HttpMethodIs(objReq, "DELETE") ) {
		char sID[24]; HttpGetQueryVar(objReq, "id", sID, sizeof(sID)); int64 id = xrtStrToI64(sID);
		if ( id > 1 ) {
			sqlite3_bind_int64(stmt_magroup_sum, 1, id); int64 authCount = 0;
			if ( sqlite3_step(stmt_magroup_sum) == SQLITE_ROW ) authCount = sqlite3_column_int64(stmt_magroup_sum, 0); sqlite3_reset(stmt_magroup_sum);
			if ( authCount > 0 ) { xtime now = xrtNow(); sqlite3_bind_int64(stmt_magroup_mov, 1, now); sqlite3_bind_int64(stmt_magroup_mov, 2, id); sqlite3_step(stmt_magroup_mov); sqlite3_reset(stmt_magroup_mov); }
			xtime now = xrtNow(); sqlite3_bind_int64(stmt_magroup_del, 1, now); sqlite3_bind_int64(stmt_magroup_del, 2, id); sqlite3_step(stmt_magroup_del); sqlite3_reset(stmt_magroup_del);
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分类删除成功！\"}", 0); ReloadCache_MemberAuthGroup();
		} else if ( id == 1 ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"不能删除默认权限分类！\"}", 0); }
		else { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的权限分类ID\"}", 0); }
	} else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}



// ==================== 前台权限分组管理 ====================

void Request_View_Member_Auth(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{ if ( HttpMethodIs(objReq, "GET") ) LoadPage(objResp, 200, HTTP_CT_HTML, "member/auth.html"); else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); }

void Request_View_Member_Auth_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		xvalue tblInfo = xvoCreateTable(); xvoTableSetValue(tblInfo, "groupList", 9, G_CACHE_MemberAuthGroup, FALSE);
		size_t iSize = 0; str sPage = MakePageWithTemplate("member/auth_add.html", tblInfo, &iSize);
		xvoUnref(tblInfo); http_reply(objResp, 200, HTTP_CT_HTML, sPage, iSize); xrtFree(sPage);
	} else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

void Request_View_Member_Auth_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		char sID[24]; HttpGetQueryVar(objReq, "id", sID, sizeof(sID)); int64 id = xrtStrToI64(sID);
		xvalue tblInfo = xvoCreateTable();
		sqlite3_bind_int64(stmt_mauth_get, 1, id);
		if ( sqlite3_step(stmt_mauth_get) == SQLITE_ROW ) {
			xvoTableSetInt(tblInfo, "id", 2, id);
			xvoTableSetInt(tblInfo, "groupID", 7, sqlite3_column_int64(stmt_mauth_get, 1));
			xvoTableSetText(tblInfo, "name", 4, (str)sqlite3_column_text(stmt_mauth_get, 2), 0, FALSE);
			xvoTableSetText(tblInfo, "desc", 4, (str)sqlite3_column_text(stmt_mauth_get, 3), 0, FALSE);
			xvoTableSetInt(tblInfo, "sort", 4, sqlite3_column_int64(stmt_mauth_get, 4));
		}
		sqlite3_reset(stmt_mauth_get);
		xvoTableSetValue(tblInfo, "groupList", 9, G_CACHE_MemberAuthGroup, FALSE);
		size_t iSize = 0; str sPage = MakePageWithTemplate("member/auth_edit.html", tblInfo, &iSize);
		xvoUnref(tblInfo); http_reply(objResp, 200, HTTP_CT_HTML, sPage, iSize); xrtFree(sPage);
	} else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

void Request_Member_Auth(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		char sParam[64]; HttpGetQueryVar(objReq, "page", sParam, sizeof(sParam)); int64 iPage = xrtStrToI64(sParam); if ( iPage <= 0 ) iPage = 1;
		HttpGetQueryVar(objReq, "limit", sParam, sizeof(sParam)); int64 iLimit = xrtStrToI64(sParam); if ( iLimit <= 0 ) iLimit = 10;
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = HttpGetQueryVar(objReq, "search", sParam, sizeof(sParam));
		
		xvalue data = xvoCreateArray(); int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_int64(stmt_mauth_all, 1, iLimit); sqlite3_bind_int64(stmt_mauth_all, 2, iOffset);
			while ( sqlite3_step(stmt_mauth_all) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_mauth_all, 0));
				xvoTableSetInt(tblRow, "groupID", 7, sqlite3_column_int64(stmt_mauth_all, 1));
				xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt_mauth_all, 2), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt_mauth_all, 3), 0, FALSE);
				xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int64(stmt_mauth_all, 4));
				xtime iTime = sqlite3_column_int64(stmt_mauth_all, 5);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iTime = sqlite3_column_int64(stmt_mauth_all, 6);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				xvoTableSetText(tblRow, "groupName", 9, (str)sqlite3_column_text(stmt_mauth_all, 7), 0, FALSE);
				if ( iCount <= 0 ) iCount = sqlite3_column_int64(stmt_mauth_all, 8);
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_mauth_all);
		} else {
			// 筛选查�?
			sqlite3_bind_text(stmt_mauth_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_mauth_sel, 2, iLimit); sqlite3_bind_int64(stmt_mauth_sel, 3, iOffset);
			while ( sqlite3_step(stmt_mauth_sel) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_mauth_sel, 0));
				xvoTableSetInt(tblRow, "groupID", 7, sqlite3_column_int64(stmt_mauth_sel, 1));
				xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt_mauth_sel, 2), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt_mauth_sel, 3), 0, FALSE);
				xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int64(stmt_mauth_sel, 4));
				xtime iTime = sqlite3_column_int64(stmt_mauth_sel, 5);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iTime = sqlite3_column_int64(stmt_mauth_sel, 6);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				xvoTableSetText(tblRow, "groupName", 9, (str)sqlite3_column_text(stmt_mauth_sel, 7), 0, FALSE);
				if ( iCount <= 0 ) iCount = sqlite3_column_int64(stmt_mauth_sel, 8);
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_mauth_sel);
		}
		
		xvalue tblRet = xvoCreateTable(); xvoTableSetBool(tblRet, "result", 6, TRUE); xvoTableSetInt(tblRet, "code", 4, 0);
		xvoTableSetInt(tblRet, "count", 5, iCount); xvoTableSetValue(tblRet, "data", 4, data, TRUE);
		size_t iRetSize = 0; char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
		http_reply(objResp, 200, HTTP_CT_JSON, sRet, iRetSize); xrtFree(sRet); xvoUnref(tblRet);
	} else if ( HttpMethodIs(objReq, "POST") ) {
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xvoUnref(tblForm); return; }
		int64 groupID = xvoTableGetInt(tblForm, "groupID", 7); str name = xvoTableGetText(tblForm, "name", 4);
		str desc = xvoTableGetText(tblForm, "desc", 4); int64 sort = xvoTableGetInt(tblForm, "sort", 4);
		xtime now = xrtNow(); sqlite3_bind_int64(stmt_mauth_add, 1, groupID); sqlite3_bind_text(stmt_mauth_add, 2, name, -1, NULL);
		sqlite3_bind_text(stmt_mauth_add, 3, desc ? desc : (str)"", -1, NULL); sqlite3_bind_int64(stmt_mauth_add, 4, sort);
		sqlite3_bind_int64(stmt_mauth_add, 5, now); sqlite3_bind_int64(stmt_mauth_add, 6, now);
		sqlite3_step(stmt_mauth_add); int64 newId = sqlite3_last_insert_rowid(G_DB); sqlite3_reset(stmt_mauth_add); xvoUnref(tblForm);
		mg_http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分组添加成功！\", \"data\": {\"id\": %lld}}", newId);
		ReloadCache_MemberAuth(); ReloadCache_MemberAuthGroup(); MemberAuth_ReloadCache();
	} else if ( HttpMethodIs(objReq, "PUT") ) {
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblForm->Type != XVO_DT_TABLE ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据！\"}", 0); xvoUnref(tblForm); return; }
		int64 id = xvoTableGetInt(tblForm, "id", 2); int64 groupID = xvoTableGetInt(tblForm, "groupID", 7);
		str name = xvoTableGetText(tblForm, "name", 4); str desc = xvoTableGetText(tblForm, "desc", 4); int64 sort = xvoTableGetInt(tblForm, "sort", 4);
		xtime now = xrtNow(); sqlite3_bind_int64(stmt_mauth_put, 1, groupID); sqlite3_bind_text(stmt_mauth_put, 2, name, -1, NULL);
		sqlite3_bind_text(stmt_mauth_put, 3, desc ? desc : (str)"", -1, NULL); sqlite3_bind_int64(stmt_mauth_put, 4, sort);
		sqlite3_bind_int64(stmt_mauth_put, 5, now); sqlite3_bind_int64(stmt_mauth_put, 6, id);
		sqlite3_step(stmt_mauth_put); sqlite3_reset(stmt_mauth_put); xvoUnref(tblForm);
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分组更新成功！\"}", 0);
		ReloadCache_MemberAuth(); ReloadCache_MemberAuthGroup(); MemberAuth_ReloadCache();
	} else if ( HttpMethodIs(objReq, "DELETE") ) {
		char sID[24]; HttpGetQueryVar(objReq, "id", sID, sizeof(sID)); int64 id = xrtStrToI64(sID);
		if ( id > 1 ) {
			sqlite3_bind_int64(stmt_mauth_sum, 1, id); int64 urisCount = 0;
			if ( sqlite3_step(stmt_mauth_sum) == SQLITE_ROW ) urisCount = sqlite3_column_int64(stmt_mauth_sum, 0); sqlite3_reset(stmt_mauth_sum);
			if ( urisCount > 0 ) { xtime now = xrtNow(); sqlite3_bind_int64(stmt_mauth_mov, 1, now); sqlite3_bind_int64(stmt_mauth_mov, 2, id); sqlite3_step(stmt_mauth_mov); sqlite3_reset(stmt_mauth_mov); }
			xtime now = xrtNow(); sqlite3_bind_int64(stmt_mauth_del, 1, now); sqlite3_bind_int64(stmt_mauth_del, 2, id); sqlite3_step(stmt_mauth_del); sqlite3_reset(stmt_mauth_del);
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"权限分组删除成功！\"}", 0);
			ReloadCache_MemberAuth(); ReloadCache_MemberAuthGroup(); MemberAuth_ReloadCache();
		} else if ( id == 1 ) { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"不能删除默认权限分组！\"}", 0); }
		else { http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的权限分组ID\"}", 0); }
	} else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}



