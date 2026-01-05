


// 跟踪接口 - 用于调试和监控全局缓存数据



// 获取全局缓存概览
void Request_Trace_Overview(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_GET ) {
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	
	xvalue tblData = xvoCreateTable();
	
	// Session 缓存信息
	xvalue tblSession = xvoCreateTable();
	xvoTableSetBool(tblSession, "exists", 6, G_Session != NULL);
	if ( G_Session != NULL ) {
		xvoTableSetInt(tblSession, "count", 5, xvoTableItemCount(G_Session));
	}
	xvoTableSetValue(tblData, "session", 7, tblSession, TRUE);
	
	// Option 缓存信息
	xvalue tblOption = xvoCreateTable();
	xvoTableSetBool(tblOption, "exists", 6, G_Option != NULL);
	if ( G_Option != NULL ) {
		xvoTableSetInt(tblOption, "count", 5, xvoTableItemCount(G_Option));
	}
	xvoTableSetValue(tblData, "option", 6, tblOption, TRUE);
	
	// 权限缓存信息
	xvalue tblAuth = xvoCreateTable();
	xvoTableSetBool(tblAuth, "roleAuth_exists", 15, G_CACHE_RoleAuth != NULL);
	xvoTableSetBool(tblAuth, "auth_exists", 11, G_CACHE_Auth != NULL);
	xvoTableSetBool(tblAuth, "group_exists", 12, G_CACHE_Group != NULL);
	xvoTableSetBool(tblAuth, "role_exists", 11, G_CACHE_Role != NULL);
	if ( G_CACHE_RoleAuth != NULL ) {
		xvoTableSetInt(tblAuth, "roleAuth_count", 14, xvoTableItemCount(G_CACHE_RoleAuth));
	}
	if ( G_CACHE_Auth != NULL ) {
		xvoTableSetInt(tblAuth, "auth_count", 10, xvoArrayItemCount(G_CACHE_Auth));
	}
	if ( G_CACHE_Group != NULL ) {
		xvoTableSetInt(tblAuth, "group_count", 11, xvoArrayItemCount(G_CACHE_Group));
	}
	if ( G_CACHE_Role != NULL ) {
		xvoTableSetInt(tblAuth, "role_count", 10, xvoArrayItemCount(G_CACHE_Role));
	}
	xvoTableSetValue(tblData, "auth", 4, tblAuth, TRUE);
	
	// 路由表信息
	xvalue tblRoute = xvoCreateTable();
	xvoTableSetBool(tblRoute, "exists", 6, G_StaticRouteTableHTTP != NULL);
	if ( G_StaticRouteTableHTTP != NULL ) {
		xvoTableSetInt(tblRoute, "count", 5, xrtDictCount(G_StaticRouteTableHTTP));
	}
	xvoTableSetValue(tblData, "route", 5, tblRoute, TRUE);
	
	// 数据库连接信息
	xvalue tblDB = xvoCreateTable();
	xvoTableSetBool(tblDB, "connected", 9, G_DB != NULL);
	xvoTableSetValue(tblData, "database", 8, tblDB, TRUE);
	
	// 安装状态
	xvoTableSetBool(tblData, "installed", 9, G_Install);
	
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	
	size_t iRetSize = 0;
	char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
	http_reply(c, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
	xvoUnref(tblRet);
}



// 获取 Session 缓存数据
void Request_Trace_Session(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_GET ) {
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	
	xvalue tblRet = xvoCreateTable();
	
	if ( G_Session == NULL ) {
		xvoTableSetBool(tblRet, "result", 6, FALSE);
		xvoTableSetText(tblRet, "message", 7, "Session 缓存不存在", 0, FALSE);
	} else {
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoAddRef(G_Session);
		xvoTableSetValue(tblRet, "data", 4, G_Session, TRUE);
	}
	
	size_t iRetSize = 0;
	char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
	http_reply(c, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
	xvoUnref(tblRet);
}



// 获取 Option 缓存数据
void Request_Trace_Option(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_GET ) {
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	
	xvalue tblRet = xvoCreateTable();
	
	if ( G_Option == NULL ) {
		xvoTableSetBool(tblRet, "result", 6, FALSE);
		xvoTableSetText(tblRet, "message", 7, "Option 缓存不存在", 0, FALSE);
	} else {
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoAddRef(G_Option);
		xvoTableSetValue(tblRet, "data", 4, G_Option, TRUE);
	}
	
	size_t iRetSize = 0;
	char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
	http_reply(c, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
	xvoUnref(tblRet);
}



// 获取权限相关缓存数据
void Request_Trace_Auth(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_GET ) {
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	
	// 获取类型参数
	char sType[32];
	int iSize = mg_http_get_var(&hm->query, "type", sType, sizeof(sType));
	if ( iSize <= 0 ) {
		strcpy(sType, "all");
	}
	
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	
	xvalue tblData = xvoCreateTable();
	
	if ( (strcmp(sType, "all") == 0) || (strcmp(sType, "roleAuth") == 0) ) {
		if ( G_CACHE_RoleAuth != NULL ) {
			xvoAddRef(G_CACHE_RoleAuth);
			xvoTableSetValue(tblData, "roleAuth", 8, G_CACHE_RoleAuth, TRUE);
		}
	}
	
	if ( (strcmp(sType, "all") == 0) || (strcmp(sType, "auth") == 0) ) {
		if ( G_CACHE_Auth != NULL ) {
			xvoAddRef(G_CACHE_Auth);
			xvoTableSetValue(tblData, "auth", 4, G_CACHE_Auth, TRUE);
		}
	}
	
	if ( (strcmp(sType, "all") == 0) || (strcmp(sType, "group") == 0) ) {
		if ( G_CACHE_Group != NULL ) {
			xvoAddRef(G_CACHE_Group);
			xvoTableSetValue(tblData, "group", 5, G_CACHE_Group, TRUE);
		}
	}
	
	if ( (strcmp(sType, "all") == 0) || (strcmp(sType, "role") == 0) ) {
		if ( G_CACHE_Role != NULL ) {
			xvoAddRef(G_CACHE_Role);
			xvoTableSetValue(tblData, "role", 4, G_CACHE_Role, TRUE);
		}
	}
	
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	
	size_t iRetSize = 0;
	char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
	http_reply(c, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
	xvoUnref(tblRet);
}



// 路由表遍历回调函数
bool TraceRouteWalkProc(Dict_Key* pKey, RouteInfo* pInfo, xvalue arrRoutes)
{
	if ( pInfo ) {
		xvalue tblRoute = xvoCreateTable();
		xvoTableSetText(tblRoute, "uri", 3, pKey->Key, pKey->KeyLen, FALSE);
		xvoTableSetBool(tblRoute, "log", 3, pInfo->bPutLog);
		xvoTableSetBool(tblRoute, "auth", 4, pInfo->bAuth);
		xvoTableSetInt(tblRoute, "authId", 6, pInfo->AuthID);
		xvoArrayAppendValue(arrRoutes, tblRoute, TRUE);
	}
	return FALSE;
}

// 获取路由表数据
void Request_Trace_Route(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_GET ) {
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	
	xvalue tblRet = xvoCreateTable();
	
	if ( G_StaticRouteTableHTTP == NULL ) {
		xvoTableSetBool(tblRet, "result", 6, FALSE);
		xvoTableSetText(tblRet, "message", 7, "路由表不存在", 0, FALSE);
	} else {
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		
		// 遍历路由表
		xvalue arrRoutes = xvoCreateArray();
		xrtDictWalk(G_StaticRouteTableHTTP, (Dict_EachProc)TraceRouteWalkProc, arrRoutes);
		xvoTableSetValue(tblRet, "data", 4, arrRoutes, TRUE);
	}
	
	size_t iRetSize = 0;
	char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
	http_reply(c, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
	xvoUnref(tblRet);
}


