


// 初始化安装模�?
void Install_Init()
{
	printf("        Install_Init \n");
	str sFile = xrtPathJoin(2, AppPath, "install.lock");
	G_Install = xrtFileExists(sFile);
	xrtFree(sFile);
}



// 卸载安装模块
void Install_Unit()
{
	printf("        Install_Unit \n");
	
}



// 安装 xPanel 请求
void Request_Install(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		
		// 安装页面
		LoadPage(objResp, 200, HTTP_CT_HTML, "install.html");
		
	} else if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		
		// 安装请求
		G_Install = TRUE;
		
		// step 1 : 复制数据�?+ 创建连接
		str sSrcDB = xrtPathJoin(2, InstallPath, "main.db");
		str sDstDB = xrtPathJoin(2, DBPath, "main.db");
		void DB_Unit();
		DB_Unit();
		xrtFileCopy(sSrcDB, sDstDB, TRUE);
		xrtFree(sSrcDB);
		xrtFree(sDstDB);
		void DB_Init();
		DB_Init();
		
		// step 2 : 创建超管账号（接收客户端哈希，生成随�?salt，进行服务端二次哈希后存储）
		xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		str sUser = xvoTableGetText(tblForm, "username", 8);
		str sClientHash = xvoTableGetText(tblForm, "password", 8);
		
		// 服务端二�?SHA-256 哈希
		str sSalt = xrtMakeXIDS();
		str sPwdHash = ServerHashPassword(sUser, sSalt, sClientHash);
		
		// 写入数据�?
		xtime now = xrtNow();
		sqlite3_bind_text(stmt_user_add, 1, sUser, strlen(sUser), SQLITE_STATIC);
		sqlite3_bind_text(stmt_user_add, 2, sSalt, strlen(sSalt), SQLITE_STATIC);
		sqlite3_bind_text(stmt_user_add, 3, sPwdHash, strlen(sPwdHash), SQLITE_STATIC);
		sqlite3_bind_int64(stmt_user_add, 4, 1);
		sqlite3_bind_int64(stmt_user_add, 5, 0);
		sqlite3_bind_int64(stmt_user_add, 6, now);
		sqlite3_bind_int64(stmt_user_add, 7, now);
		sqlite3_step(stmt_user_add);
		int64 newId = sqlite3_last_insert_rowid(G_DB);
		sqlite3_reset(stmt_user_add);
		xrtFree(sSalt);
		xrtFree(sPwdHash);
		xvoUnref(tblForm);
		
		// step 3 : 创建安装锁定文件
		str sFile = xrtPathJoin(2, AppPath, "install.lock");
		xrtFilePutAll(sFile, "xLogServer installed", 16);
		xrtFree(sFile);
		
		// step 4 : 返回安装成功响应
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"xLogServer 服务器管理面板安装成功！\"}", 0);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}


