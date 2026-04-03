


// 菜单管理页面视图
void Request_View_Option_Menu(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		LoadPage(objResp, 200, HTTP_CT_HTML, "option/menu.html");
		
	} else {
		
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 添加分类页面视图
void Request_View_Option_Menu_Add_Category(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		LoadPage(objResp, 200, HTTP_CT_HTML, "option/menu_add_category.html");
		
	} else {
		
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 添加菜单页面视图
void Request_View_Option_Menu_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		LoadPage(objResp, 200, HTTP_CT_HTML, "option/menu_add.html");
		
	} else {
		
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 编辑菜单/分类页面视图
void Request_View_Option_Menu_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 获取菜单 ID
		char sID[32];
		int iSize = HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
		
		if ( iSize <= 0 ) {
			LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
			return;
		}
		
		int iID = atoi(sID);
		
		// 查询菜单数据
		sqlite3_bind_int(stmt_menu_get, 1, iID);
		if ( sqlite3_step(stmt_menu_get) != SQLITE_ROW ) {
			sqlite3_reset(stmt_menu_get);
			LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
			return;
		}
		
		// 获取类型
		int iType = sqlite3_column_int(stmt_menu_get, 4);
		
		// 构建数据�?
		xvalue tblMenu = xvoCreateTable();
		xvoTableSetInt(tblMenu, "id", 2, sqlite3_column_int(stmt_menu_get, 0));
		xvoTableSetInt(tblMenu, "parent", 6, sqlite3_column_int(stmt_menu_get, 1));
		xvoTableSetText(tblMenu, "title", 5, (char*)sqlite3_column_text(stmt_menu_get, 2), 0, FALSE);
		xvoTableSetText(tblMenu, "icon", 4, (char*)sqlite3_column_text(stmt_menu_get, 3), 0, FALSE);
		xvoTableSetInt(tblMenu, "type", 4, iType);
		xvoTableSetText(tblMenu, "openType", 8, (char*)sqlite3_column_text(stmt_menu_get, 5), 0, FALSE);
		xvoTableSetText(tblMenu, "href", 4, (char*)sqlite3_column_text(stmt_menu_get, 6), 0, FALSE);
		xvoTableSetInt(tblMenu, "sort", 4, sqlite3_column_int(stmt_menu_get, 7));
		xvoTableSetInt(tblMenu, "visible", 7, sqlite3_column_int(stmt_menu_get, 8));
		xvoTableSetText(tblMenu, "remark", 6, (char*)sqlite3_column_text(stmt_menu_get, 9), 0, FALSE);
		sqlite3_reset(stmt_menu_get);
		
		// 根据类型选择模板
		str sTemplate = (iType == 0) ? "option/menu_edit_category.html" : "option/menu_edit.html";
		
		// 构建页面并返�?
		size_t iRetSize = 0;
		str sPage = MakePageWithTemplate(sTemplate, tblMenu, &iRetSize);
		xvoUnref(tblMenu);
		http_reply(objResp, 200, HTTP_CT_HTML, sPage, iRetSize);
		xrtFree(sPage);
		
	} else {
		
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 菜单数据接口
void Request_Option_Menu(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 获取所有菜单数�?
		xvalue arrData = xvoCreateArray();
		
		while ( sqlite3_step(stmt_menu_all) == SQLITE_ROW ) {
			xvalue tblRow = xvoCreateTable();
			xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int(stmt_menu_all, 0));
			xvoTableSetInt(tblRow, "parent", 6, sqlite3_column_int(stmt_menu_all, 1));
			xvoTableSetText(tblRow, "title", 5, (char*)sqlite3_column_text(stmt_menu_all, 2), 0, FALSE);
			xvoTableSetText(tblRow, "icon", 4, (char*)sqlite3_column_text(stmt_menu_all, 3), 0, FALSE);
			xvoTableSetInt(tblRow, "type", 4, sqlite3_column_int(stmt_menu_all, 4));
			xvoTableSetText(tblRow, "openType", 8, (char*)sqlite3_column_text(stmt_menu_all, 5), 0, FALSE);
			xvoTableSetText(tblRow, "href", 4, (char*)sqlite3_column_text(stmt_menu_all, 6), 0, FALSE);
			xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int(stmt_menu_all, 7));
			xvoTableSetInt(tblRow, "visible", 7, sqlite3_column_int(stmt_menu_all, 8));
			xvoTableSetText(tblRow, "remark", 6, (char*)sqlite3_column_text(stmt_menu_all, 9), 0, FALSE);
			xvoArrayAppendValue(arrData, tblRow, TRUE);
		}
		sqlite3_reset(stmt_menu_all);
		
		// 构建返回�?
		xvalue tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetText(tblRet, "message", 7, "菜单数据获取成功", 0, FALSE);
		xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
		
		size_t iRetSize = 0;
		char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
		http_reply(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xvoUnref(tblRet);
		
	} else if ( HttpMethodIs(objReq, "POST") ) {
		
		// 解析请求�?
		xvalue tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblBody == NULL ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"请求数据格式错误\"}", 0);
			return;
		}
		
		// 获取参数
		int iParent = xvoTableGetInt(tblBody, "parent", 6);
		str sTitle = xvoTableGetText(tblBody, "title", 5);
		str sIcon = xvoTableGetText(tblBody, "icon", 4);
		int iType = xvoTableGetInt(tblBody, "type", 4);
		str sOpenType = xvoTableGetText(tblBody, "openType", 8);
		str sHref = xvoTableGetText(tblBody, "href", 4);
		int iSort = xvoTableGetInt(tblBody, "sort", 4);
		int iVisible = xvoTableGetInt(tblBody, "visible", 7);
		str sRemark = xvoTableGetText(tblBody, "remark", 6);
		
		// 验证必填参数
		if ( !sTitle || (strlen(sTitle) == 0) ) {
			xvoUnref(tblBody);
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"菜单标题不能为空\"}", 0);
			return;
		}
		
		// 添加菜单
		xtime now = xrtNow();
		sqlite3_bind_int(stmt_menu_add, 1, iParent);
		sqlite3_bind_text(stmt_menu_add, 2, sTitle, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt_menu_add, 3, sIcon ? (ptr)sIcon : (str)"", -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt_menu_add, 4, iType);
		sqlite3_bind_text(stmt_menu_add, 5, sOpenType ? (ptr)sOpenType : (str)"_component", -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt_menu_add, 6, sHref ? (ptr)sHref : (str)"", -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt_menu_add, 7, iSort);
		sqlite3_bind_int(stmt_menu_add, 8, iVisible);
		sqlite3_bind_text(stmt_menu_add, 9, sRemark ? (ptr)sRemark : (str)"", -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_menu_add, 10, now);
		sqlite3_bind_int64(stmt_menu_add, 11, now);
		
		int iRet = sqlite3_step(stmt_menu_add);
		sqlite3_reset(stmt_menu_add);
		xvoUnref(tblBody);
		
		if ( iRet == SQLITE_DONE ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"菜单添加成功\"}", 0);
		} else {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"菜单添加失败\"}", 0);
		}
		
	} else if ( HttpMethodIs(objReq, "PUT") ) {
		
		// 解析请求�?
		xvalue tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblBody == NULL ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"请求数据格式错误\"}", 0);
			return;
		}
		
		// 获取参数
		int iID = xvoTableGetInt(tblBody, "id", 2);
		int iParent = xvoTableGetInt(tblBody, "parent", 6);
		str sTitle = xvoTableGetText(tblBody, "title", 5);
		str sIcon = xvoTableGetText(tblBody, "icon", 4);
		int iType = xvoTableGetInt(tblBody, "type", 4);
		str sOpenType = xvoTableGetText(tblBody, "openType", 8);
		str sHref = xvoTableGetText(tblBody, "href", 4);
		int iSort = xvoTableGetInt(tblBody, "sort", 4);
		int iVisible = xvoTableGetInt(tblBody, "visible", 7);
		str sRemark = xvoTableGetText(tblBody, "remark", 6);
		
		// 验证参数
		if ( iID <= 0 ) {
			xvoUnref(tblBody);
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"菜单ID无效\"}", 0);
			return;
		}
		
		if ( !sTitle || (strlen(sTitle) == 0) ) {
			xvoUnref(tblBody);
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"菜单标题不能为空\"}", 0);
			return;
		}
		
		// 防止将菜单设置为自己的子菜单
		if ( iParent == iID ) {
			xvoUnref(tblBody);
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"不能将菜单设置为自己的子菜单\"}", 0);
			return;
		}
		
		// 更新菜单
		xtime now = xrtNow();
		sqlite3_bind_int(stmt_menu_put, 1, iParent);
		sqlite3_bind_text(stmt_menu_put, 2, sTitle, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt_menu_put, 3, sIcon ? (ptr)sIcon : (str)"", -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt_menu_put, 4, iType);
		sqlite3_bind_text(stmt_menu_put, 5, sOpenType ? (ptr)sOpenType : (str)"_component", -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt_menu_put, 6, sHref ? (ptr)sHref : (str)"", -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt_menu_put, 7, iSort);
		sqlite3_bind_int(stmt_menu_put, 8, iVisible);
		sqlite3_bind_text(stmt_menu_put, 9, sRemark ? (ptr)sRemark : (str)"", -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_menu_put, 10, now);
		sqlite3_bind_int(stmt_menu_put, 11, iID);
		
		int iRet = sqlite3_step(stmt_menu_put);
		sqlite3_reset(stmt_menu_put);
		xvoUnref(tblBody);
		
		if ( iRet == SQLITE_DONE ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"菜单更新成功\"}", 0);
		} else {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"菜单更新失败\"}", 0);
		}
		
	} else if ( HttpMethodIs(objReq, "DELETE") ) {
		
		// 获取菜单 ID
		char sID[32];
		int iSize = HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
		
		if ( iSize <= 0 ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"缺少菜单ID参数\"}", 0);
			return;
		}
		
		int iID = atoi(sID);
		
		// 检查是否有子菜�?
		if ( Menu_HasChildren(iID) > 0 ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"该菜单下存在子菜单，请先删除子菜单\"}", 0);
			return;
		}
		
		// 删除菜单
		xtime now = xrtNow();
		sqlite3_bind_int64(stmt_menu_del, 1, now);
		sqlite3_bind_int(stmt_menu_del, 2, iID);
		
		int iRet = sqlite3_step(stmt_menu_del);
		sqlite3_reset(stmt_menu_del);
		
		if ( iRet == SQLITE_DONE ) {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"菜单删除成功\"}", 0);
		} else {
			http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"菜单删除失败\"}", 0);
		}
		
	} else {
		
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}


