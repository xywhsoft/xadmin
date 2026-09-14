/* v1 设置文件管理：列表/结构编辑/删除/加入菜单。
 * 模块层 Option_ListFiles/SaveDefinition/DeleteFile 已随配置模块迁入；
 * 本文件是路由接线与菜单联动。v1 的逐请求调试 printf 不随迁移。
 * 写入按本批纪律检查影响行数（stmt_menu_add 复用菜单模块预编译语句）。 */
static void OptionFile_ReplyJSON(XS_ResponseObject objResp, xvalue* tblRet)
{
	size_t iRetSize = 0;
	char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
}

static str OptionFile_StrOrEmpty(str sText)
{
	return sText ? sText : (str)"";
}

static void OptionFile_ReplyMessage(XS_ResponseObject objResp, bool bResult, str sMessage)
{
	xvalue* tblRet = ValueObject();
	ValueSetBool(tblRet, "result", bResult);
	ValueSetText(tblRet, "message", sMessage ? sMessage : (bResult ? (str)"操作成功" : (str)"操作失败"));
	OptionFile_ReplyJSON(objResp, tblRet);
	xrtValueRelease(tblRet);
}

static void OptionFile_ReplyData(XS_ResponseObject objResp, xvalue* objData, str sMessage)
{
	xvalue* tblRet = ValueObject();
	ValueSetBool(tblRet, "result", true);
	ValueSetText(tblRet, "message", sMessage ? sMessage : (str)"获取成功");
	ValueSetOwn(tblRet, "data", objData);
	OptionFile_ReplyJSON(objResp, tblRet);
	xrtValueRelease(tblRet);
}

/* 列表行附带菜单挂载信息（href = /admin/view/option?file=xxx）。 */
static void OptionFile_AttachMenuInfo(xvalue* arrData)
{
	sqlite3_stmt* stmt = NULL;
	int iRet;

	if ( (arrData == NULL) || (xrtValueType(arrData) != XVALUE_ARRAY) ) {
		return;
	}

	iRet = sqlite3_prepare_v3(G_DB, "SELECT id, parent, sort, visible, title FROM menu WHERE href = ? AND isDelete = 0 ORDER BY sort ASC, id ASC LIMIT 1;", -1, SQL_PREPARE_DEFAULT, &stmt, NULL);
	if ( iRet != SQLITE_OK ) {
		return;
	}

	for ( uint32 i = 0; i < ValueCount(arrData); i++ ) {
		xvalue* tblItem = xrtValueArrayGet(arrData, i);
		str sFileName;
		str sHref;

		if ( (tblItem == NULL) || (xrtValueType(tblItem) != XVALUE_OBJECT) ) {
			continue;
		}

		sFileName = ValueText(tblItem, "file");
		sHref = Option_BuildViewHref(sFileName);
		sqlite3_bind_text(stmt, 1, (const char*)OptionFile_StrOrEmpty(sHref), -1, SQLITE_TRANSIENT);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			ValueSetBool(tblItem, "inMenu", true);
			ValueSetInt(tblItem, "menuId", sqlite3_column_int(stmt, 0));
			ValueSetInt(tblItem, "menuParent", sqlite3_column_int(stmt, 1));
			ValueSetInt(tblItem, "menuSort", sqlite3_column_int(stmt, 2));
			ValueSetInt(tblItem, "menuVisible", sqlite3_column_int(stmt, 3));
			ValueSetText(tblItem, "menuTitle", (str)sqlite3_column_text(stmt, 4));
		} else {
			ValueSetBool(tblItem, "inMenu", false);
		}
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
		xrtFree(sHref);
	}

	sqlite3_finalize(stmt);
}

static int OptionFile_FindMenuParent()
{
	int iParent = 0;
	sqlite3_stmt* stmt = NULL;
	int iRet;

	iRet = sqlite3_prepare_v3(G_DB, "SELECT parent FROM menu WHERE href = '/admin/view/option?file=global.json' AND isDelete = 0 ORDER BY id ASC LIMIT 1;", -1, SQL_PREPARE_DEFAULT, &stmt, NULL);
	if ( iRet == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iParent = sqlite3_column_int(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}

	if ( iParent > 0 ) {
		return iParent;
	}

	iRet = sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE title = '设置' AND type = 0 AND isDelete = 0 ORDER BY sort ASC, id ASC LIMIT 1;", -1, SQL_PREPARE_DEFAULT, &stmt, NULL);
	if ( iRet == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iParent = sqlite3_column_int(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}

	return iParent;
}

/* 新配置页排在 global.json 与下一个固定条目之间，占满则排到末尾。 */
static int OptionFile_FindMenuSort(int iParent)
{
	sqlite3_stmt* stmt = NULL;
	int iRet;
	int iGlobalSort = 600300;
	int iBarrierSort = 0;
	int iMaxCustomSort = 0;
	int iMaxSort = 0;

	iRet = sqlite3_prepare_v3(G_DB, "SELECT sort FROM menu WHERE href = '/admin/view/option?file=global.json' AND isDelete = 0 ORDER BY id ASC LIMIT 1;", -1, SQL_PREPARE_DEFAULT, &stmt, NULL);
	if ( iRet == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iGlobalSort = sqlite3_column_int(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}

	iRet = sqlite3_prepare_v3(G_DB, "SELECT sort FROM menu WHERE parent = ? AND isDelete = 0 AND sort > ? ORDER BY sort ASC LIMIT 1;", -1, SQL_PREPARE_DEFAULT, &stmt, NULL);
	if ( iRet == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iParent);
		sqlite3_bind_int(stmt, 2, iGlobalSort);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iBarrierSort = sqlite3_column_int(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}
	if ( iBarrierSort <= iGlobalSort ) {
		iBarrierSort = iGlobalSort + 100;
	}

	iRet = sqlite3_prepare_v3(G_DB, "SELECT MAX(sort) FROM menu WHERE parent = ? AND isDelete = 0 AND sort > ? AND sort < ?;", -1, SQL_PREPARE_DEFAULT, &stmt, NULL);
	if ( iRet == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iParent);
		sqlite3_bind_int(stmt, 2, iGlobalSort);
		sqlite3_bind_int(stmt, 3, iBarrierSort);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iMaxCustomSort = sqlite3_column_int(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}
	if ( iMaxCustomSort > iGlobalSort ) {
		int iNextSort = iMaxCustomSort + 10;
		if ( iNextSort < iBarrierSort ) {
			return iNextSort;
		}
	}

	iRet = sqlite3_prepare_v3(G_DB, "SELECT MAX(sort) FROM menu WHERE parent = ? AND isDelete = 0 AND sort >= ?;", -1, SQL_PREPARE_DEFAULT, &stmt, NULL);
	if ( iRet == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iParent);
		sqlite3_bind_int(stmt, 2, iGlobalSort);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iMaxSort = sqlite3_column_int(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}

	if ( iMaxSort < iGlobalSort ) {
		return iGlobalSort + 10;
	}

	return iMaxSort + 10;
}

static void OptionFile_DeleteMenusByFile(str sFileName)
{
	sqlite3_stmt* stmt = NULL;
	int iRet;
	str sHref = Option_BuildViewHref(sFileName);

	iRet = sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE href = ? AND isDelete = 0 ORDER BY id ASC;", -1, SQL_PREPARE_DEFAULT, &stmt, NULL);
	if ( iRet != SQLITE_OK ) {
		xrtFree(sHref);
		return;
	}

	sqlite3_bind_text(stmt, 1, (const char*)OptionFile_StrOrEmpty(sHref), -1, SQLITE_TRANSIENT);
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		int iMenuID = sqlite3_column_int(stmt, 0);
		xtime now = xrtNow();
		sqlite3_bind_int64(stmt_menu_del, 1, now);
		sqlite3_bind_int(stmt_menu_del, 2, iMenuID);
		DB_Write(stmt_menu_del, true);
	}
	sqlite3_finalize(stmt);
	xrtFree(sHref);
}

static bool OptionFile_AddMenu(str sFileName, str* psError)
{
	xvalue* tblConfig;
	str sHref;
	str sTitle;
	str sDesc;
	int iParent;
	int iSort;

	if ( !Option_IsValidFileName(sFileName) ) {
		if ( psError ) *psError = xrtStrDup("非法的文件名");
		return false;
	}

	tblConfig = Option_LoadFile(sFileName);
	if ( tblConfig == NULL ) {
		if ( psError ) *psError = xrtStrDup("配置文件不存在或解析失败");
		return false;
	}

	sHref = Option_BuildViewHref(sFileName);
	{
		sqlite3_stmt* stmt = NULL;
		int iRet = sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE href = ? AND isDelete = 0 LIMIT 1;", -1, SQL_PREPARE_DEFAULT, &stmt, NULL);
		if ( iRet == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, (const char*)OptionFile_StrOrEmpty(sHref), -1, SQLITE_TRANSIENT);
			if ( sqlite3_step(stmt) == SQLITE_ROW ) {
				sqlite3_finalize(stmt);
				xrtFree(sHref);
				xrtValueRelease(tblConfig);
				if ( psError ) *psError = xrtStrDup("该配置已添加到菜单中");
				return false;
			}
			sqlite3_finalize(stmt);
		}
	}

	iParent = OptionFile_FindMenuParent();
	iSort = OptionFile_FindMenuSort(iParent);
	sTitle = ValueText(tblConfig, "title");
	sDesc = ValueText(tblConfig, "desc");

	sqlite3_bind_int(stmt_menu_add, 1, iParent);
	sqlite3_bind_text(stmt_menu_add, 2, (sTitle && sTitle[0]) ? sTitle : sFileName, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt_menu_add, 3, "layui-icon layui-icon-set", -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt_menu_add, 4, 1);
	sqlite3_bind_text(stmt_menu_add, 5, "_iframe", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt_menu_add, 6, (const char*)OptionFile_StrOrEmpty(sHref), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt_menu_add, 7, iSort);
	sqlite3_bind_int(stmt_menu_add, 8, 1);
	sqlite3_bind_text(stmt_menu_add, 9, (const char*)((sDesc && sDesc[0]) ? sDesc : (str)"自定义配置页面"), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt_menu_add, 10, xrtNow());
	sqlite3_bind_int64(stmt_menu_add, 11, xrtNow());

	if ( !DB_Write(stmt_menu_add, true) ) {
		xrtFree(sHref);
		xrtValueRelease(tblConfig);
		if ( psError ) *psError = xrtStrDup("添加菜单失败");
		return false;
	}

	xrtFree(sHref);
	xrtValueRelease(tblConfig);
	return true;
}

// 设置文件管理列表页面
void Request_View_Option_Files(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		LoadPage(objResp, 200, HTTP_CT_HTML, "option/files.html");
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

// 设置文件编辑页面
void Request_View_Option_File(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		LoadPage(objResp, 200, HTTP_CT_HTML, "option/file_edit.html");
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

// 设置文件列表接口
void Request_Option_Files(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		xvalue* arrData = Option_ListFiles();
		OptionFile_AttachMenuInfo(arrData);
		OptionFile_ReplyData(objResp, arrData, "设置文件列表获取成功");
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

// 设置文件 CRUD 接口
void Request_Option_File(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		char sFileName[128];
		int iSize = xsReqQueryValue(objReq, "file", sFileName, sizeof(sFileName));
		xvalue* tblConfig;

		if ( iSize <= 0 ) {
			OptionFile_ReplyMessage(objResp, false, "缺少 file 参数");
			return;
		}
		if ( !Option_IsValidFileName(sFileName) ) {
			OptionFile_ReplyMessage(objResp, false, "非法的文件名");
			return;
		}

		tblConfig = Option_LoadFile(sFileName);
		if ( tblConfig == NULL ) {
			OptionFile_ReplyMessage(objResp, false, "配置文件不存在或解析失败");
			return;
		}

		OptionFile_ReplyData(objResp, tblConfig, "设置文件获取成功");
		return;
	}

	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) || (xsReqMethodID(objReq) == XHTTP_METHOD_PUT) ) {
		xvalue* tblBody = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		xvalue* tblData;
		str sFileName;
		str sError = NULL;
		bool bCreate = (xsReqMethodID(objReq) == XHTTP_METHOD_POST);
		bool bOK;

		if ( (tblBody == NULL) || (xrtValueType(tblBody) != XVALUE_OBJECT) ) {
			OptionFile_ReplyMessage(objResp, false, "请求数据格式错误");
			return;
		}

		sFileName = ValueText(tblBody, "file");
		tblData = ValueGet(tblBody, "data");
		if ( (sFileName == NULL) || (sFileName[0] == '\0') ) {
			xrtValueRelease(tblBody);
			OptionFile_ReplyMessage(objResp, false, "缺少 file 参数");
			return;
		}
		if ( (tblData == NULL) || (xrtValueType(tblData) != XVALUE_OBJECT) ) {
			xrtValueRelease(tblBody);
			OptionFile_ReplyMessage(objResp, false, "缺少 data 参数");
			return;
		}

		bOK = Option_SaveDefinition(sFileName, tblData, bCreate, &sError);
		xrtValueRelease(tblBody);
		if ( !bOK ) {
			OptionFile_ReplyMessage(objResp, false, sError ? sError : (str)"保存失败");
			if ( sError ) xrtFree(sError);
			return;
		}

		if ( sError ) xrtFree(sError);
		OptionFile_ReplyMessage(objResp, true, bCreate ? "设置文件创建成功" : "设置文件保存成功");
		return;
	}

	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_DELETE) ) {
		char sFileName[128];
		int iSize = xsReqQueryValue(objReq, "file", sFileName, sizeof(sFileName));
		str sError = NULL;

		if ( iSize <= 0 ) {
			OptionFile_ReplyMessage(objResp, false, "缺少 file 参数");
			return;
		}

		if ( !Option_DeleteFile(sFileName, &sError) ) {
			OptionFile_ReplyMessage(objResp, false, sError ? sError : (str)"删除失败");
			if ( sError ) xrtFree(sError);
			return;
		}

		OptionFile_DeleteMenusByFile(sFileName);
		OptionFile_ReplyMessage(objResp, true, "设置文件删除成功");
		return;
	}

	LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

// 将设置文件加入菜单
void Request_Option_File_Menu(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	xvalue* tblBody = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	str sFileName;
	str sError = NULL;

	if ( (tblBody == NULL) || (xrtValueType(tblBody) != XVALUE_OBJECT) ) {
		OptionFile_ReplyMessage(objResp, false, "请求数据格式错误");
		return;
	}

	sFileName = ValueText(tblBody, "file");
	if ( (sFileName == NULL) || (sFileName[0] == '\0') ) {
		xrtValueRelease(tblBody);
		OptionFile_ReplyMessage(objResp, false, "缺少 file 参数");
		return;
	}

	if ( !OptionFile_AddMenu(sFileName, &sError) ) {
		xrtValueRelease(tblBody);
		OptionFile_ReplyMessage(objResp, false, sError ? sError : (str)"添加菜单失败");
		if ( sError ) xrtFree(sError);
		return;
	}

	xrtValueRelease(tblBody);
	if ( sError ) xrtFree(sError);
	OptionFile_ReplyMessage(objResp, true, "已加入菜单");
}
