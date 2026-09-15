


// 菜单管理页面视图
void Request_View_Option_Menu(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		LoadPage(objResp, 200, HTTP_CT_HTML, "option/menu.html");
		
	} else {
		
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 添加分类页面视图
void Request_View_Option_Menu_Add_Category(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		LoadPage(objResp, 200, HTTP_CT_HTML, "option/menu_add_category.html");
		
	} else {
		
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 添加菜单页面视图
void Request_View_Option_Menu_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		LoadPage(objResp, 200, HTTP_CT_HTML, "option/menu_add.html");
		
	} else {
		
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 编辑菜单/分类页面视图
void Request_View_Option_Menu_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 获取菜单 ID
		char sID[32];
		int iSize = xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		
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
		
		// 构建数据表
		xvalue* tblMenu = ValueObject();
		ValueSetInt(tblMenu, "id", sqlite3_column_int(stmt_menu_get, 0));
		ValueSetInt(tblMenu, "parent", sqlite3_column_int(stmt_menu_get, 1));
		ValueSetText(tblMenu, "title", (char*)sqlite3_column_text(stmt_menu_get, 2));
		ValueSetText(tblMenu, "icon", (char*)sqlite3_column_text(stmt_menu_get, 3));
		ValueSetInt(tblMenu, "type", iType);
		ValueSetText(tblMenu, "openType", (char*)sqlite3_column_text(stmt_menu_get, 5));
		ValueSetText(tblMenu, "href", (char*)sqlite3_column_text(stmt_menu_get, 6));
		ValueSetInt(tblMenu, "sort", sqlite3_column_int(stmt_menu_get, 7));
		ValueSetInt(tblMenu, "visible", sqlite3_column_int(stmt_menu_get, 8));
		ValueSetText(tblMenu, "remark", (char*)sqlite3_column_text(stmt_menu_get, 9));
		sqlite3_reset(stmt_menu_get);
		
		// 根据类型选择模板
		str sTemplate = (iType == 0) ? "option/menu_edit_category.html" : "option/menu_edit.html";
		
		// 构建页面并返回
		size_t iRetSize = 0;
		str sPage = MakePageWithTemplate(sTemplate, tblMenu, &iRetSize);
		xrtValueRelease(tblMenu);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_HTML, sPage, iRetSize);
		xrtFree(sPage);
		
	} else {
		
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 菜单数据接口
void Request_Option_Menu(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 获取所有菜单数据
		xvalue* arrData = ValueArray();
		int iCount = 0;

		while ( sqlite3_step(stmt_menu_all) == SQLITE_ROW ) {
			xvalue* tblRow = ValueObject();
			ValueSetInt(tblRow, "id", sqlite3_column_int(stmt_menu_all, 0));
			ValueSetInt(tblRow, "parent", sqlite3_column_int(stmt_menu_all, 1));
			ValueSetText(tblRow, "title", (char*)sqlite3_column_text(stmt_menu_all, 2));
			ValueSetText(tblRow, "icon", (char*)sqlite3_column_text(stmt_menu_all, 3));
			ValueSetInt(tblRow, "type", sqlite3_column_int(stmt_menu_all, 4));
			ValueSetText(tblRow, "openType", (char*)sqlite3_column_text(stmt_menu_all, 5));
			ValueSetText(tblRow, "href", (char*)sqlite3_column_text(stmt_menu_all, 6));
			ValueSetInt(tblRow, "sort", sqlite3_column_int(stmt_menu_all, 7));
			ValueSetInt(tblRow, "visible", sqlite3_column_int(stmt_menu_all, 8));
			ValueSetText(tblRow, "remark", (char*)sqlite3_column_text(stmt_menu_all, 9));
			ValueArrayOwn(arrData, tblRow);
			iCount++;
		}
		sqlite3_reset(stmt_menu_all);

		// 构建返回值
		xvalue* tblRet = ValueObject();
		ValueSetBool(tblRet, "result", true);
		ValueSetText(tblRet, "message", "菜单数据获取成功");
		ValueSetInt(tblRet, "count", iCount);
		ValueSetOwn(tblRet, "data", arrData);
		
		size_t iRetSize = 0;
		char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xrtValueRelease(tblRet);
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		
		// 解析请求体
		xvalue* tblBody = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblBody == NULL ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"请求数据格式错误\"}", 0);
			return;
		}
		
		// 获取参数
		int iParent = ValueInt(tblBody, "parent");
		str sTitle = ValueText(tblBody, "title");
		str sIcon = ValueText(tblBody, "icon");
		int iType = ValueInt(tblBody, "type");
		str sOpenType = ValueText(tblBody, "openType");
		str sHref = ValueText(tblBody, "href");
		int iSort = ValueInt(tblBody, "sort");
		int iVisible = ValueInt(tblBody, "visible");
		str sRemark = ValueText(tblBody, "remark");
		
		// 验证必填参数
		if ( !sTitle || (strlen(sTitle) == 0) ) {
			xrtValueRelease(tblBody);
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"菜单标题不能为空\"}", 0);
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
		
		bool written = DB_Write(stmt_menu_add, true);
		xrtValueRelease(tblBody);
		
		if ( written ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"菜单添加成功\"}", 0);
		} else {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"菜单添加失败\"}", 0);
		}
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_PUT) ) {
		
		// 解析请求体
		xvalue* tblBody = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( tblBody == NULL ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"请求数据格式错误\"}", 0);
			return;
		}
		
		// 获取参数
		int iID = ValueInt(tblBody, "id");
		int iParent = ValueInt(tblBody, "parent");
		str sTitle = ValueText(tblBody, "title");
		str sIcon = ValueText(tblBody, "icon");
		int iType = ValueInt(tblBody, "type");
		str sOpenType = ValueText(tblBody, "openType");
		str sHref = ValueText(tblBody, "href");
		int iSort = ValueInt(tblBody, "sort");
		int iVisible = ValueInt(tblBody, "visible");
		str sRemark = ValueText(tblBody, "remark");
		
		// 验证参数
		if ( iID <= 0 ) {
			xrtValueRelease(tblBody);
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"菜单ID无效\"}", 0);
			return;
		}
		
		if ( !sTitle || (strlen(sTitle) == 0) ) {
			xrtValueRelease(tblBody);
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"菜单标题不能为空\"}", 0);
			return;
		}
		
		// 防止将菜单设置为自己的子菜单；并沿父链上溯防间接环（L4：A→B→A 会让
		// 两菜单从 parent=0 的树构建中静默消失）
		if ( iParent == iID ) {
			xrtValueRelease(tblBody);
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"不能将菜单设置为自己的子菜单\"}", 0);
			return;
		}
		{
			/* 上溯父链至根，深度上限 64（数据库本身不该超菜单树深） */
			int64 iWalk = iParent;
			int iDepth = 0;
			bool bCycle = false;
			while ( iWalk > 0 && iDepth++ < 64 ) {
				if ( iWalk == iID ) { bCycle = true; break; }
				sqlite3_bind_int(stmt_menu_get, 1, (int)iWalk);
				if ( sqlite3_step(stmt_menu_get) == SQLITE_ROW ) {
					iWalk = sqlite3_column_int(stmt_menu_get, 1); /* parent 列 */
				} else {
					iWalk = 0;
				}
				sqlite3_reset(stmt_menu_get);
			}
			if ( bCycle ) {
				xrtValueRelease(tblBody);
				xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"不能将菜单挂到自己的子菜单下\"}", 0);
				return;
			}
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
		
		bool written = DB_Write(stmt_menu_put, true);
		xrtValueRelease(tblBody);
		
		if ( written ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"菜单更新成功\"}", 0);
		} else {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"菜单更新失败\"}", 0);
		}
		
	} else if ( (xsReqMethodID(objReq) == XHTTP_METHOD_DELETE) ) {
		
		// 获取菜单 ID
		char sID[32];
		int iSize = xsReqQueryValue(objReq, "id", sID, sizeof(sID));
		
		if ( iSize <= 0 ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"缺少菜单ID参数\"}", 0);
			return;
		}
		
		int iID = atoi(sID);
		
		// 检查是否有子菜单
		if ( Menu_HasChildren(iID) > 0 ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"该菜单下存在子菜单，请先删除子菜单\"}", 0);
			return;
		}
		
		// 删除菜单
		xtime now = xrtNow();
		sqlite3_bind_int64(stmt_menu_del, 1, now);
		sqlite3_bind_int(stmt_menu_del, 2, iID);
		
		bool written = DB_Write(stmt_menu_del, true);
		
		if ( written ) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"菜单删除成功\"}", 0);
		} else {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"菜单删除失败\"}", 0);
		}
		
	} else {
		
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}


