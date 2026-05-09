


// 跟踪接口 - 用于调试和监控全局缓存数据



// 获取全局缓存概览
void Request_Trace_Overview(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	
	xvalue tblData = xvoCreateTable();
	
	// Session 缓存信息 - 后台管理�?
	xvalue tblSession = xvoCreateTable();
	xvoTableSetBool(tblSession, "exists", 6, G_AdminSession != NULL);
	if ( G_AdminSession != NULL ) {
		xvoTableSetInt(tblSession, "count", 5, xvoTableItemCount(G_AdminSession));
	}
	xvoTableSetValue(tblData, "adminSession", 12, tblSession, TRUE);
	
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
		if ( G_CACHE_RoleAuth->Type == XVO_DT_ARRAY ) {
			xvoTableSetInt(tblAuth, "roleAuth_count", 14, xvoArrayItemCount(G_CACHE_RoleAuth));
		} else if ( G_CACHE_RoleAuth->Type == XVO_DT_TABLE ) {
			xvoTableSetInt(tblAuth, "roleAuth_count", 14, xvoTableItemCount(G_CACHE_RoleAuth));
		} else if ( G_CACHE_RoleAuth->Type == XVO_DT_LIST ) {
			xvoTableSetInt(tblAuth, "roleAuth_count", 14, xvoListItemCount(G_CACHE_RoleAuth));
		}
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
	
	// 路由表信�?
	xvalue tblRoute = xvoCreateTable();
	xvoTableSetBool(tblRoute, "exists", 6, G_StaticRouteTableHTTP != NULL);
	if ( G_StaticRouteTableHTTP != NULL ) {
		xvoTableSetInt(tblRoute, "count", 5, xrtDictCount(G_StaticRouteTableHTTP));
	}
	xvoTableSetBool(tblRoute, "dynamicExists", 13, G_DynamicRouteTableHTTP.lstRoutes != NULL);
	xvoTableSetInt(tblRoute, "dynamicCount", 12, G_DynamicRouteTableHTTP.lstRoutes ? xrtListCount(G_DynamicRouteTableHTTP.lstRoutes) : 0);
	xvoTableSetInt(tblRoute, "dynamicCompiled", 15, G_DynamicRouteTableHTTP.iCompiledCount);
	xvoTableSetInt(tblRoute, "dynamicGeneration", 17, G_DynamicRouteTableHTTP.iGeneration);
	if ( G_DynamicRouteTableHTTP.sLastError ) {
		xvoTableSetText(tblRoute, "dynamicLastError", 16, G_DynamicRouteTableHTTP.sLastError, 0, FALSE);
	}
	if ( G_DynamicRouteTableHTTP.sLastWarning ) {
		xvoTableSetText(tblRoute, "dynamicLastWarning", 18, G_DynamicRouteTableHTTP.sLastWarning, 0, FALSE);
	}
	xvoTableSetValue(tblData, "route", 5, tblRoute, TRUE);
	
	// 数据库连接信�?
	xvalue tblDB = xvoCreateTable();
	xvoTableSetBool(tblDB, "connected", 9, G_DB != NULL);
	xvoTableSetValue(tblData, "database", 8, tblDB, TRUE);
	
	// 安装状�?
	xvoTableSetBool(tblData, "installed", 9, G_Install);
	
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	
	size_t iRetSize = 0;
	char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
	xvoUnref(tblRet);
}



// 获取 Session 缓存数据
void Request_Trace_Session(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	
	xvalue tblRet = xvoCreateTable();
	
	if ( G_AdminSession == NULL ) {
		xvoTableSetBool(tblRet, "result", 6, FALSE);
		xvoTableSetText(tblRet, "message", 7, "AdminSession ���治����", 0, FALSE);
	} else {
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoAddRef(G_AdminSession);
		xvoTableSetValue(tblRet, "data", 4, G_AdminSession, TRUE);
	}
	
	size_t iRetSize = 0;
	char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
	xvoUnref(tblRet);
}



// 获取 Option 缓存数据
void Request_Trace_Option(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	
	xvalue tblRet = xvoCreateTable();
	
	if ( G_Option == NULL ) {
		xvoTableSetBool(tblRet, "result", 6, FALSE);
		xvoTableSetText(tblRet, "message", 7, "Option ���治����", 0, FALSE);
	} else {
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoAddRef(G_Option);
		xvoTableSetValue(tblRet, "data", 4, G_Option, TRUE);
	}
	
	size_t iRetSize = 0;
	char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
	xvoUnref(tblRet);
}



// 获取权限相关缓存数据
void Request_Trace_Auth(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	
	// 获取类型参数
	char sType[32];
	int iSize = xsReqQueryValue(objReq, "type", sType, sizeof(sType));
	if ( iSize <= 0 ) {
		strcpy(sType, "all");
	}
	
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	
	xvalue tblData = xvoCreateTable();
	
	if ( (strcmp(sType, "all") == 0) || (strcmp(sType, "roleAuth") == 0) ) {
		if ( G_CACHE_RoleAuth != NULL ) {
			xvalue tblRoleAuth = xvoCreateTable();
			xvalue objRole1 = XAdminIDCacheGetValue(G_CACHE_RoleAuth, 1);
			xvalue objRole2 = XAdminIDCacheGetValue(G_CACHE_RoleAuth, 2);
			xvalue objItem0 = (G_CACHE_RoleAuth->Type == XVO_DT_ARRAY && G_CACHE_RoleAuth->vArray->Count > 0) ? xvoArrayGetValue(G_CACHE_RoleAuth, 0) : NULL;
			xvalue objItem1 = (G_CACHE_RoleAuth->Type == XVO_DT_ARRAY && G_CACHE_RoleAuth->vArray->Count > 1) ? xvoArrayGetValue(G_CACHE_RoleAuth, 1) : NULL;
			xvoTableSetInt(tblRoleAuth, "cacheType", 9, G_CACHE_RoleAuth->Type);
			if ( G_CACHE_RoleAuth->Type == XVO_DT_ARRAY ) {
				xvoTableSetInt(tblRoleAuth, "count", 5, xvoArrayItemCount(G_CACHE_RoleAuth));
			} else if ( G_CACHE_RoleAuth->Type == XVO_DT_TABLE ) {
				xvoTableSetInt(tblRoleAuth, "count", 5, xvoTableItemCount(G_CACHE_RoleAuth));
			} else if ( G_CACHE_RoleAuth->Type == XVO_DT_LIST ) {
				xvoTableSetInt(tblRoleAuth, "count", 5, xvoListItemCount(G_CACHE_RoleAuth));
			}
			xvoTableSetBool(tblRoleAuth, "role1_exists", 12, objRole1 != NULL);
			xvoTableSetBool(tblRoleAuth, "item0_exists", 12, objItem0 != NULL);
			xvoTableSetBool(tblRoleAuth, "item1_exists", 12, objItem1 != NULL);
			xvoTableSetBool(tblRoleAuth, "role2_exists", 12, objRole2 != NULL);
			if ( objRole1 != NULL ) {
				xvoTableSetInt(tblRoleAuth, "role1_type", 10, objRole1->Type);
				if ( objRole1->Type == XVO_DT_TABLE ) {
					xvoTableSetInt(tblRoleAuth, "role1_id", 8, xvoTableGetInt(objRole1, "id", 2));
				}
			}
			if ( objRole2 != NULL ) {
				xvoTableSetInt(tblRoleAuth, "role2_type", 10, objRole2->Type);
				if ( objRole2->Type == XVO_DT_TABLE ) {
					xvoTableSetInt(tblRoleAuth, "role2_id", 8, xvoTableGetInt(objRole2, "id", 2));
				}
			}
			if ( objItem0 != NULL ) {
				xvoTableSetInt(tblRoleAuth, "item0_type", 10, objItem0->Type);
				if ( objItem0->Type == XVO_DT_TABLE ) {
					xvoTableSetInt(tblRoleAuth, "item0_id", 8, xvoTableGetInt(objItem0, "id", 2));
				}
			}
			if ( objItem1 != NULL ) {
				xvoTableSetInt(tblRoleAuth, "item1_type", 10, objItem1->Type);
				if ( objItem1->Type == XVO_DT_TABLE ) {
					xvoTableSetInt(tblRoleAuth, "item1_id", 8, xvoTableGetInt(objItem1, "id", 2));
				}
			}
			xvoTableSetValue(tblData, "roleAuth", 8, tblRoleAuth, TRUE);
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
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
	xvoUnref(tblRet);
}



// 路由表遍历回调函�?
bool TraceRouteWalkProc(Dict_Key* pKey, RouteInfo* pInfo, xvalue arrRoutes)
{
	if ( pInfo ) {
		xvalue tblRoute = xvoCreateTable();
		xvoTableSetText(tblRoute, "uri", 3, pKey->Key, pKey->KeyLen, FALSE);
		xvoTableSetText(tblRoute, "type", 4, "static", 6, FALSE);
		xvoTableSetBool(tblRoute, "log", 3, pInfo->bPutLog);
		xvoTableSetBool(tblRoute, "auth", 4, pInfo->bAuth);
		xvoTableSetInt(tblRoute, "authId", 6, pInfo->AuthID);
		xvoArrayAppendValue(arrRoutes, tblRoute, TRUE);
	}
	return FALSE;
}

void TraceDynamicRoutes(xvalue arrRoutes)
{
	if ( (arrRoutes == NULL) || (G_DynamicRouteTableHTTP.lstRoutes == NULL) ) {
		return;
	}
	for ( uint32 i = 0; i < xrtListCount(G_DynamicRouteTableHTTP.lstRoutes); i++ ) {
		DynamicRouteInfo* pRoute = (DynamicRouteInfo*)xrtListGetPtr(G_DynamicRouteTableHTTP.lstRoutes, i);
		if ( pRoute ) {
			xvalue tblRoute = xvoCreateTable();
			xvoTableSetText(tblRoute, "uri", 3, pRoute->sUri ? pRoute->sUri : "", 0, FALSE);
			xvoTableSetText(tblRoute, "type", 4, "dynamic", 7, FALSE);
			xvoTableSetText(tblRoute, "pattern", 7, pRoute->sPattern ? pRoute->sPattern : "", 0, FALSE);
			xvoTableSetInt(tblRoute, "priority", 8, pRoute->iPriority);
			xvoTableSetInt(tblRoute, "method", 6, pRoute->iMethod);
			xvoTableSetInt(tblRoute, "patternIndex", 12, pRoute->iPatternIndex);
			xvoTableSetInt(tblRoute, "captureCount", 12, pRoute->iCaptureCount);
			xvoTableSetBool(tblRoute, "log", 3, pRoute->Info.bPutLog);
			xvoTableSetBool(tblRoute, "auth", 4, pRoute->Info.bAuth);
			xvoTableSetInt(tblRoute, "authId", 6, pRoute->Info.AuthID);
			xvoArrayAppendValue(arrRoutes, tblRoute, TRUE);
		}
	}
}

// 获取路由表数�?
void Request_Trace_Route(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	
	xvalue tblRet = xvoCreateTable();
	
	if ( G_StaticRouteTableHTTP == NULL ) {
		xvoTableSetBool(tblRet, "result", 6, FALSE);
		xvoTableSetText(tblRet, "message", 7, "路由表不存在", 0, FALSE);
	} else {
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		
		// 遍历路由�?
		xvalue arrRoutes = xvoCreateArray();
		xrtDictWalk(G_StaticRouteTableHTTP, (Dict_EachProc)TraceRouteWalkProc, arrRoutes);
		TraceDynamicRoutes(arrRoutes);
		xvoTableSetValue(tblRet, "data", 4, arrRoutes, TRUE);
	}
	
	size_t iRetSize = 0;
	char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
	xvoUnref(tblRet);
}


