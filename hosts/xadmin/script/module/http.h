
#include <stdarg.h>

static str HttpDupSpan(const char* sText, size_t iLen)
{
	str sOut;

	sOut = xrtMalloc(iLen + 1);
	if ( sOut == NULL ) {
		return NULL;
	}
	if ( (sText != NULL) && (iLen > 0) ) {
		memcpy(sOut, sText, iLen);
	}
	sOut[iLen] = '\0';
	return sOut;
}

static int HttpHexDigitValue(char cHex)
{
	if ( (cHex >= '0') && (cHex <= '9') ) {
		return cHex - '0';
	}
	if ( (cHex >= 'A') && (cHex <= 'F') ) {
		return cHex - 'A' + 10;
	}
	if ( (cHex >= 'a') && (cHex <= 'f') ) {
		return cHex - 'a' + 10;
	}
	return -1;
}

static str HttpUrlDecodeDup(const char* sText, size_t iLen)
{
	str sOut;
	size_t iRead;
	size_t iWrite = 0;
	
	if ( sText == NULL ) {
		return NULL;
	}
	
	sOut = xrtMalloc(iLen + 1);
	if ( sOut == NULL ) {
		return NULL;
	}
	
	for ( iRead = 0; iRead < iLen; iRead++ ) {
		if ( (sText[iRead] == '%') && ((iRead + 2) < iLen) ) {
			int iHigh = HttpHexDigitValue(sText[iRead + 1]);
			int iLow = HttpHexDigitValue(sText[iRead + 2]);
			if ( (iHigh >= 0) && (iLow >= 0) ) {
				sOut[iWrite++] = (char)((iHigh << 4) | iLow);
				iRead += 2;
				continue;
			}
		}
		
		if ( sText[iRead] == '+' ) {
			sOut[iWrite++] = ' ';
		} else {
			sOut[iWrite++] = sText[iRead];
		}
	}
	
	sOut[iWrite] = '\0';
	return sOut;
}

static const char* HttpReasonText(uint32 iCode)
{
	switch ( iCode ) {
		case 200: return "OK";
		case 302: return "Found";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 500: return "Internal Server Error";
		default: return "OK";
	}
}

bool HttpMethodIs(XS_RequestObject objReq, const char* sMethod)
{
	const char* sReqMethod = xsReqMethod(objReq);

	if ( sReqMethod == NULL || sMethod == NULL ) {
		return FALSE;
	}

	return strcmp(sReqMethod, sMethod) == 0;
}

int HttpGetQueryVar(XS_RequestObject objReq, const char* sName, char* sOut, size_t iOutCap)
{
	const char* sQuery = xsReqQuery(objReq);
	xrtquerypair tPair;
	str sDecoded = NULL;
	size_t iDecodedLen;

	if ( sOut == NULL || iOutCap == 0 ) {
		return -1;
	}

	sOut[0] = '\0';
	if ( sQuery == NULL || sName == NULL ) {
		return -1;
	}
	if ( !xrtQueryFind(sQuery, sName, &tPair) ) {
		return -1;
	}

	sDecoded = HttpUrlDecodeDup(tPair.tValue.sPtr, tPair.tValue.iLen);
	if ( sDecoded == NULL ) {
		sDecoded = HttpDupSpan("", 0);
		if ( sDecoded == NULL ) {
			return -1;
		}
	}

	iDecodedLen = strlen(sDecoded);
	if ( iDecodedLen >= iOutCap ) {
		iDecodedLen = iOutCap - 1;
	}
	if ( iDecodedLen > 0 ) {
		memcpy(sOut, sDecoded, iDecodedLen);
	}
	sOut[iDecodedLen] = '\0';
	xrtFree(sDecoded);
	return (int)iDecodedLen;
}

bool HttpMultipartNameIs(const HttpMultipartPart* pPart, const char* sName)
{
	size_t iNameLen;

	if ( pPart == NULL || sName == NULL || pPart->sName == NULL ) {
		return FALSE;
	}

	iNameLen = strlen(sName);
	if ( pPart->iNameLen != iNameLen ) {
		return FALSE;
	}

	return memcmp(pPart->sName, sName, iNameLen) == 0;
}

bool HttpMultipartNext(XS_RequestObject objReq, size_t* pOffset, HttpMultipartPart* pPart)
{
	const char* sContentType;
	xrtstrview tBoundary;
	xrtmultipartpartview tPart;

	if ( pOffset == NULL || pPart == NULL ) {
		return FALSE;
	}

	memset(pPart, 0, sizeof(HttpMultipartPart));
	memset(&tBoundary, 0, sizeof(tBoundary));
	memset(&tPart, 0, sizeof(tPart));

	sContentType = xsReqHeader(objReq, "Content-Type");
	if ( sContentType == NULL || sContentType[0] == '\0' ) {
		return FALSE;
	}
	if ( !xrtMultipartBoundaryFromContentType(sContentType, &tBoundary) ) {
		return FALSE;
	}
	if ( !xrtMultipartNextN((str)xsReqBody(objReq), xsReqBodyLen(objReq), tBoundary.sPtr, tBoundary.iLen, pOffset, &tPart) ) {
		return FALSE;
	}

	pPart->sName = tPart.tName.sPtr;
	pPart->iNameLen = tPart.tName.iLen;
	pPart->sFileName = tPart.tFileName.sPtr;
	pPart->iFileNameLen = tPart.tFileName.iLen;
	pPart->pBody = tPart.tBody.sPtr;
	pPart->iBodyLen = tPart.tBody.iLen;
	return TRUE;
}

static str HttpGetCookieDup(XS_RequestObject objReq, const char* sName)
{
	const char* sCookie = xsReqHeader(objReq, "Cookie");
	xrtcookiepair tCookie;

	if ( sCookie == NULL || sName == NULL ) {
		return NULL;
	}
	if ( !xrtCookieFind(sCookie, sName, &tCookie) ) {
		return NULL;
	}

	return HttpUrlDecodeDup(tCookie.tValue.sPtr, tCookie.tValue.iLen);
}

static void HttpApplyHeaderLine(XS_ResponseObject objResp, const char* sLine, size_t iLineLen, const char** psContentType, str* psContentTypeMem)
{
	const char* sColon;
	size_t iNameLen;
	size_t iValueOff;
	size_t iValueLen;

	sColon = (const char*)memchr(sLine, ':', iLineLen);
	if ( sColon == NULL ) {
		return;
	}

	iNameLen = (size_t)(sColon - sLine);
	iValueOff = iNameLen + 1;
	while ( iValueOff < iLineLen && (sLine[iValueOff] == ' ' || sLine[iValueOff] == '\t') ) {
		iValueOff++;
	}
	iValueLen = iLineLen - iValueOff;
	while ( iValueLen > 0 && (sLine[iValueOff + iValueLen - 1] == ' ' || sLine[iValueOff + iValueLen - 1] == '\t') ) {
		iValueLen--;
	}

	if ( iNameLen == 12 && strncmp(sLine, "Content-Type", 12) == 0 ) {
		if ( *psContentTypeMem ) {
			xrtFree(*psContentTypeMem);
			*psContentTypeMem = NULL;
		}
		*psContentTypeMem = HttpDupSpan(sLine + iValueOff, iValueLen);
		if ( *psContentTypeMem ) {
			*psContentType = *psContentTypeMem;
		}
		return;
	}

	str sName = HttpDupSpan(sLine, iNameLen);
	str sValue = HttpDupSpan(sLine + iValueOff, iValueLen);
	if ( sName && sValue ) {
		xsHttpHeader(objResp, sName, sValue);
	}
	if ( sName ) {
		xrtFree(sName);
	}
	if ( sValue ) {
		xrtFree(sValue);
	}
}

static void HttpApplyHeaders(XS_ResponseObject objResp, const char* sHead, const char** psContentType, str* psContentTypeMem)
{
	const char* sLine;

	if ( sHead == NULL || sHead[0] == '\0' ) {
		return;
	}

	sLine = sHead;
	while ( *sLine ) {
		const char* sNext = strstr(sLine, "\r\n");
		size_t iLineLen;

		if ( sNext ) {
			iLineLen = (size_t)(sNext - sLine);
		} else {
			iLineLen = strlen(sLine);
		}

		if ( iLineLen == 0 ) {
			break;
		}

		HttpApplyHeaderLine(objResp, sLine, iLineLen, psContentType, psContentTypeMem);
		if ( sNext == NULL ) {
			break;
		}
		sLine = sNext + 2;
	}
}

int http_reply(XS_ResponseObject objResp, int iCode, str sHead, const void* pBody, size_t iLen)
{
	const char* sContentType = "text/plain; charset=utf-8";
	str sContentTypeMem = NULL;
	const char* pOutBody = (const char*)pBody;

	if ( objResp == NULL ) {
		return 0;
	}

	xsHttpStatus(objResp, (uint32)iCode, HttpReasonText((uint32)iCode));
	HttpApplyHeaders(objResp, sHead, &sContentType, &sContentTypeMem);

	if ( pOutBody == NULL ) {
		pOutBody = "";
		iLen = 0;
	} else if ( iLen == 0 ) {
		iLen = strlen(pOutBody);
	}

	int iRet = xsHttpBody(objResp, pOutBody, iLen, sContentType);
	if ( sContentTypeMem ) {
		xrtFree(sContentTypeMem);
	}
	return iRet;
}

int HttpReplyFormat(XS_ResponseObject objResp, int iCode, str sHead, str sFormat, ...)
{
	va_list objArgs;
	int iLen;
	str sText;
	int iRet;

	if ( sFormat == NULL ) {
		return http_reply(objResp, iCode, sHead, "", 0);
	}

	va_start(objArgs, sFormat);
#if defined(_WIN32) || defined(_WIN64)
	iLen = _vscprintf(sFormat, objArgs);
#else
	iLen = vsnprintf(NULL, 0, sFormat, objArgs);
#endif
	va_end(objArgs);
	if ( iLen < 0 ) {
		return http_reply(objResp, 500, HTTP_CT_TEXT, "format failed", 0);
	}

	sText = xrtMalloc((size_t)iLen + 1);
	if ( sText == NULL ) {
		return http_reply(objResp, 500, HTTP_CT_TEXT, "alloc failed", 0);
	}

	va_start(objArgs, sFormat);
	vsnprintf(sText, (size_t)iLen + 1, sFormat, objArgs);
	va_end(objArgs);

	iRet = http_reply(objResp, iCode, sHead, sText, (size_t)iLen);
	xrtFree(sText);
	return iRet;
}

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
		http_reply(objResp, 302, "Location: /admin/login\r\n", NULL, 0);
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
			http_reply(objResp, 403, HTTP_CT_JSON, "{\"code\":403,\"msg\":\"forbidden\"}", 0);
		}
	} else {
		http_reply(objResp, 401, HTTP_CT_JSON, "{\"code\":401,\"msg\":\"unauthorized\"}", 0);
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
	RouteInfo* pInfo = xrtDictGet(G_StaticRouteTableHTTP, sPath, strlen(sPath));
	if ( pInfo == NULL ) {
		return FALSE;
	}
	
	
	RouteInfo tInfo;
	xvalue objSession = NULL;
	bool bOwnSession = FALSE;
	str sSessionID = NULL;
	tInfo = *pInfo;
	pInfo = &tInfo;

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
		sSessionID = HttpGetCookieDup(objReq, "XSID");
		if ( sSessionID ) {
			objSession = Session_GetAdminByID(sSessionID);
		}
	} else {
		sSessionID = HttpGetCookieDup(objReq, "MSID");
		if ( sSessionID ) {
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

	if ( sSessionID ) {
		xrtFree(sSessionID);
	}
	if ( bOwnSession && objSession ) {
		xvoUnref(objSession);
	}
	return TRUE;
}
