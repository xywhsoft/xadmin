
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



size_t HttpMethodLen(XS_RequestObject objReq)
{
	const char* sText = xsReqMethod(objReq);

	return sText ? strlen(sText) : 0;
}



size_t HttpPathLen(XS_RequestObject objReq)
{
	const char* sText = xsReqPath(objReq);

	return sText ? strlen(sText) : 0;
}



size_t HttpQueryLen(XS_RequestObject objReq)
{
	const char* sText = xsReqQuery(objReq);

	return sText ? strlen(sText) : 0;
}



static int HttpCopyQueryValue(const char* sQuery, const char* sName, char* sOut, size_t iOutCap)
{
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



int HttpGetQueryVar(XS_RequestObject objReq, const char* sName, char* sOut, size_t iOutCap)
{
	return HttpCopyQueryValue(xsReqQuery(objReq), sName, sOut, iOutCap);
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
static bool HttpCookieFreeWalkProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	char** psText = (char**)pVal;

	(void)pKey;
	(void)pArg;

	if ( psText && *psText ) {
		xrtFree(*psText);
	}

	return TRUE;
}



xdict ParseCookies(XS_RequestObject objReq)
{
	const char* sCookie = xsReqHeader(objReq, "Cookie");
	xdict tblCookie = xrtDictCreate(sizeof(char*), 0);
	size_t iOffset = 0;
	xrtcookiepair tCookie;

	if ( sCookie == NULL ) {
		return tblCookie;
	}

	while ( xrtCookieNext(sCookie, &iOffset, &tCookie) ) {
		char** psValue;
		str sDecoded;

		psValue = xrtDictSet(tblCookie, (ptr)tCookie.tName.sPtr, tCookie.tName.iLen, NULL);
		if ( psValue == NULL ) {
			continue;
		}

		sDecoded = HttpUrlDecodeDup(tCookie.tValue.sPtr, tCookie.tValue.iLen);
		if ( sDecoded == NULL ) {
			sDecoded = HttpDupSpan("", 0);
		}
		*psValue = sDecoded;
	}

	return tblCookie;
}



void FreeCookies(xdict tblCookies)
{
	if ( tblCookies == NULL ) {
		return;
	}

	xrtDictWalk(tblCookies, HttpCookieFreeWalkProc, NULL);
	xrtDictDestroy(tblCookies);
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
		xvalue tblRole = xvoListGetValue(G_CACHE_RoleAuth, iRoleID);
		if ( tblRole && (tblRole->Type == XVO_DT_TABLE) ) {
			bool bOK = xvoTableGetBool(tblRole, xsReqPath(objReq), (int)HttpPathLen(objReq));
			if ( bOK ) {
				if ( pInfo->bActive ) {
					Session_ExtendAdmin(objSession);
				}
				pInfo->Proc(objServer, objHost, objReq, objResp, objSession);
			} else {
				LoadPage(objResp, 403, HTTP_CT_HTML, "status/403.html");
			}
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
		xvalue tblGroup = xvoListGetValue(G_CACHE_MemberGroupAuth, iGroupID);
		if ( tblGroup && (tblGroup->Type == XVO_DT_TABLE) ) {
			bool bOK = xvoTableGetBool(tblGroup, xsReqPath(objReq), (int)HttpPathLen(objReq));
			if ( bOK ) {
				if ( pInfo->bActive ) {
					Session_ExtendMember(objSession);
				}
				pInfo->Proc(objServer, objHost, objReq, objResp, objSession);
			} else {
				http_reply(objResp, 403, HTTP_CT_JSON, "{\"code\":403,\"msg\":\"閺夊啴妾烘稉宥堝喕\"}", 0);
			}
		} else {
			http_reply(objResp, 403, HTTP_CT_JSON, "{\"code\":403,\"msg\":\"閺夊啴妾烘稉宥堝喕\"}", 0);
		}
	} else {
		http_reply(objResp, 401, HTTP_CT_JSON, "{\"code\":401,\"msg\":\"unauthorized\"}", 0);
	}
}



// current http request dispatcher
bool RequestProc(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp)
{
	const char* sPath = xsReqPath(objReq);
	RouteInfo* pInfo;
	xdict tblCookies = NULL;
	xvalue objSession = NULL;
	bool bOwnSession = FALSE;
	str sSessionID;

	printf("[xadmin:req] path=%s method=%s\n", sPath ? sPath : "(null)", xsReqMethod(objReq) ? xsReqMethod(objReq) : "(null)");
	fflush(stdout);

	if ( sPath == NULL || sPath[0] == '\0' ) {
		printf("[xadmin:req] empty path\n");
		fflush(stdout);
		return FALSE;
	}

	pInfo = xrtDictGet(G_StaticRouteTableHTTP, sPath, strlen(sPath));
	if ( pInfo == NULL ) {
		printf("[xadmin:req] route miss: %s\n", sPath);
		fflush(stdout);
		return FALSE;
	}

	printf("[xadmin:req] route hit: %s auth=%d admin=%d\n", sPath, pInfo->bAuth ? 1 : 0, pInfo->bAdmin ? 1 : 0);
	fflush(stdout);

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

	tblCookies = ParseCookies(objReq);
	if ( pInfo->bAdmin ) {
		sSessionID = xrtDictGetPtr(tblCookies, "XSID", 4);
		if ( sSessionID ) {
			objSession = xvoTableGetValue(G_AdminSession, sSessionID, 0);
		}
	} else {
		sSessionID = xrtDictGetPtr(tblCookies, "MSID", 4);
		if ( sSessionID ) {
			objSession = xvoTableGetValue(G_MemberSession, sSessionID, 0);
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
			xvoTableRemove(G_AdminSession, sSessionID, 0);
		} else {
			xvoTableRemove(G_MemberSession, sSessionID, 0);
		}
		if ( bOwnSession && objSession ) {
			xvoUnref(objSession);
		}
		objSession = xvoCreateNull();
		bOwnSession = TRUE;
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
		pInfo->Proc(objServer, objHost, objReq, objResp, objSession);
	}

	FreeCookies(tblCookies);
	if ( bOwnSession && objSession ) {
		xvoUnref(objSession);
	}
	return TRUE;
}
