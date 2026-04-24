
// admin request auth
static void AdminRequestAuth(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession, RouteInfo* pInfo)
{
	if ( objSession->Type == XVO_DT_TABLE ) {
		int64 iRoleID = xvoTableGetInt(objSession, "roleID", 6);
		xvalue tblRole = XAdminIDCacheGetValue(G_CACHE_RoleAuth, iRoleID);
		const char* sPath = xsReqPath(objReq);
		size_t iPathLen = sPath ? strlen(sPath) : 0;
		bool bOK = FALSE;
		if ( tblRole && (tblRole->Type == XVO_DT_TABLE) ) {
			bOK = xvoTableGetBool(tblRole, sPath, (int)iPathLen);
		}
		if ( !bOK ) {
			bOK = Auth_DBRoleGetAccess(iRoleID, pInfo->AuthID, NULL);
		}
		if ( bOK ) {
			if ( pInfo->bActive ) {
				Session_ExtendAdmin(objSession);
			}
			PS_HostInvokeRoute(pInfo, objServer, objHost, objReq, objResp, objSession);
		} else {
			LoadPage(objResp, 403, HTTP_CT_HTML, "status/403.html");
		}
	} else {
		str sHeader = xrtFormat("Content-Type: text/plain\r\nLocation: %s\r\n", Option_GetAdminLoginPath());
		if ( sHeader != NULL ) {
			xsHttpReplyAuto(objResp, 302, sHeader, "", 0);
			xrtFree(sHeader);
		} else {
			xsHttpReplyAuto(objResp, 302, "Content-Type: text/plain\r\nLocation: /admin/login\r\n", "", 0);
		}
	}
}

// member request auth
static void MemberRequestAuth(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession, RouteInfo* pInfo)
{
	if ( objSession->Type == XVO_DT_TABLE ) {
		int64 iGroupID = xvoTableGetInt(objSession, "groupId", 7);
		xvalue tblGroup = XAdminIDCacheGetValue(G_CACHE_MemberGroupAuth, iGroupID);
		const char* sPath = xsReqPath(objReq);
		size_t iPathLen = sPath ? strlen(sPath) : 0;
		bool bOK = FALSE;
		if ( tblGroup && (tblGroup->Type == XVO_DT_TABLE) ) {
			bOK = xvoTableGetBool(tblGroup, sPath, (int)iPathLen);
		}
		if ( !bOK ) {
			bOK = MemberAuth_DBGroupGetAccess(iGroupID, pInfo->AuthID, NULL);
		}
		if ( bOK ) {
			if ( pInfo->bActive ) {
				Session_ExtendMember(objSession);
			}
			PS_HostInvokeRoute(pInfo, objServer, objHost, objReq, objResp, objSession);
		} else {
			xsHttpReplyAuto(objResp, 403, HTTP_CT_JSON, "{\"code\":403,\"msg\":\"forbidden\"}", 0);
		}
	} else {
		xsHttpReplyAuto(objResp, 401, HTTP_CT_JSON, "{\"code\":401,\"msg\":\"unauthorized\"}", 0);
	}
}



// 处理 HTTP 请求的回调函数
bool RequestProc(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp)
{
	
	// 安全检查
	if ( (objReq == NULL) || (objResp == NULL) ) {
		return FALSE;
	}
	
	// 获取 URI
	const char* sPath = xsReqPath(objReq);
	if ( (sPath == NULL) || (sPath[0] == '\0') ) {
		return FALSE;
	}
	
	// 查询路由表
	bool bAdminEntryAlias = Option_AdminEntryIsMatch(sPath);
	const char* sLookupPath = bAdminEntryAlias ? "/admin/login" : sPath;
	const RouteInfo* pRoute = (const RouteInfo*)xrtDictGet(G_StaticRouteTableHTTP, (str)sLookupPath, strlen(sLookupPath));
	if ( pRoute == NULL ) {
		return FALSE;
	}
	
	
	RouteInfo tInfo;
	RouteInfo* pInfo = &tInfo;
	xvalue objSession = NULL;
	bool bOwnSession = FALSE;
	char sSessionID[128] = {0};
	tInfo = *pRoute;

	if ( G_Install == FALSE ) {
		objSession = xvoCreateNull();
		if ( objSession ) {
			bOwnSession = TRUE;
		}
		Request_Install(objServer, objHost, objReq, objResp, objSession);
		if ( bOwnSession && objSession ) {
			xvoUnref(objSession);
		}
		return TRUE;
	}

	if ( pInfo->bAdmin ) {
		if ( xsReqCookieValue(objReq, "XSID", sSessionID, sizeof(sSessionID)) >= 0 ) {
			objSession = Session_GetAdminByID(sSessionID);
		}
	} else {
		if ( xsReqCookieValue(objReq, "MSID", sSessionID, sizeof(sSessionID)) >= 0 ) {
			objSession = Session_GetMemberByID(sSessionID);
		}
	}

	if ( objSession == NULL ) {
		objSession = xvoCreateNull();
		if ( objSession ) {
			bOwnSession = TRUE;
		}
	}

	if ( objSession && (objSession->Type == XVO_DT_TABLE) && Session_IsExpired(objSession) ) {
		if ( pInfo->bAdmin ) {
			Session_RemoveAdminByID(sSessionID);
		} else {
			Session_RemoveMemberByID(sSessionID);
		}
		if ( bOwnSession && objSession ) {
			xvoUnref(objSession);
		}
		objSession = xvoCreateNull();
		bOwnSession = (objSession != NULL);
	}

	if ( Option_AdminEntryEnabled() && pInfo->bAdmin && (objSession->Type != XVO_DT_TABLE) ) {
		if ( !bAdminEntryAlias ) {
			LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
			if ( bOwnSession && objSession ) {
				xvoUnref(objSession);
			}
			return TRUE;
		}
	}

	if ( pInfo->bAuth ) {
		if ( pInfo->bAdmin ) {
			if ( pInfo->bPutLog ) {
				Logs_Add(objReq, objSession);
			}
			AdminRequestAuth(objServer, objHost, objReq, objResp, objSession, pInfo);
		} else {
			MemberRequestAuth(objServer, objHost, objReq, objResp, objSession, pInfo);
		}
	} else {
		PS_HostInvokeRoute(pInfo, objServer, objHost, objReq, objResp, objSession);
	}

	if ( bOwnSession && objSession ) {
		xvoUnref(objSession);
	}
	return TRUE;
}
