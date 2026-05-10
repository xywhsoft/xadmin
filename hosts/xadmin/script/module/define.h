


// 全局定义
#define SQL_PREPARE_DEFAULT		SQLITE_PREPARE_PERSISTENT | SQLITE_PREPARE_DONT_LOG



// 补充 API 定义



// 全局路径
str ExePath;
str AppPath;
str WebPath;
str DBPath;
str LogPath;
str TempPath;
str PagePath;
str SitePagePath;
str ToolPath;
str OptionPath;
str InstallPath;
str TemplatePath;



// HTML 载荷类型
str HTTP_CT_HTML = "Content-Type: text/html\r\n";
str HTTP_CT_TEXT = "Content-Type: text/plain\r\n";
str HTTP_CT_JSON = "Content-Type: application/json\r\n";



// 服务端二次 SHA-256 哈希函数
// 计算：SHA256(用户名 + salt + 客户端密码哈希值
// salt 为每个用户独立的随机盐（存储在数据库 user.salt 字段）
// 返回动态分配的哈希字符串，调用方需要 xrtFree 释放
str ServerHashPassword(str user, str salt, str clientHash)
{
	str sCombined = xrtFormat("%s%s%s", user, salt, clientHash);
	uint8 arrHash[32];
	str sPwdHash;

	xrtSHA256((const ptr)sCombined, strlen(sCombined), arrHash);
	sPwdHash = xrtHexEncode(arrHash, sizeof(arrHash));
	xrtFree(sCombined);
	return sPwdHash;
}



// 是否已安装
bool G_Install = FALSE;



// 全局 Session - 后台管理
xvalue G_AdminSession = NULL;
xdict G_AdminSessionMap = NULL;

// 全局 Session - 前台用户
xvalue G_MemberSession = NULL;
xdict G_MemberSessionMap = NULL;



// 全局配置
xvalue G_Option = NULL;
bool G_AdminEntryEnabled = FALSE;
str G_AdminEntryPath = NULL;



// 全局数据库对象
sqlite3* G_DB = NULL;



// 全局服务缓存
xlist G_Services = NULL;



// 全局静态路由表 - HTTP
typedef struct {
	
	// 对应 URI 的处理函数
	void (*Proc)(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);
	
	// 对应插件
	void* pPluginRouteToken;
	void* pPageRouteToken;
	
	// 是否必须鉴权才能访问
	bool bAuth;
	
	// 是否是后台 URI（TRUE为后台、FALSE为前台）
	bool bAdmin;
	
	// 是否记录访问日志（后台选项）
	bool bPutLog;
	
	// 是否保持活跃（访问了保持活跃的链接，会自动延伸 session 寿命）
	bool bActive;
	
	// 所属权限组ID
	uint32 AuthID;
	
	// 权限级别 0 为不限制，否则必须用户组具备大于等于这个数字的权限级别才能访问）
	uint32 AuthLevel;

	bool bDynamic;
	void* pDynamicRoute;
	
} RouteInfo;
xdict G_StaticRouteTableHTTP;
xdict G_DynamicRouteInvokeContext = NULL;

// Keep the request hot path bounded while allowing one route group to expose a practical candidate set.
#define XADMIN_DYNAMIC_ROUTE_MAX_MATCHES 256
#define XADMIN_DYNAMIC_ROUTE_MAX_CAPTURES 16

typedef struct {
	RouteInfo Info;
	str sUri;
	str sPattern;
	xregex* pRegex;
	int iPriority;
	int iMethod;
	uint32 iPatternIndex;
	uint32 iCaptureCount;
} DynamicRouteInfo;

typedef struct {
	DynamicRouteInfo* pRoute;
	const char* sPath;
	size_t iPathLen;
	xregexspan arrCaptures[XADMIN_DYNAMIC_ROUTE_MAX_CAPTURES];
	uint32 iCaptureCount;
} DynamicRouteInvokeContext;

typedef struct {
	xlist lstRoutes;
	xregexset* pRegexSet;
	uint32 iCompiledCount;
	uint32 iGeneration;
	str sLastError;
	str sLastWarning;
} DynamicRouteTableHTTP;

DynamicRouteTableHTTP G_DynamicRouteTableHTTP = {0};

// 添加全局静态路由表 - HTTP
void AddStaticRouteHTTP(str uri, void* proc)
{
	RouteInfo* pInfo = xrtDictSet(G_StaticRouteTableHTTP, uri, strlen(uri), NULL);
	if ( pInfo ) {
		pInfo->Proc = proc;
		pInfo->pPluginRouteToken = NULL;
		pInfo->pPageRouteToken = NULL;
		pInfo->bAuth = TRUE;		// 默认需要鉴权（安全优先）
		pInfo->bAdmin = TRUE;		// 默认后台接口
		pInfo->bPutLog = FALSE;		// 默认不记录日志
		pInfo->bActive = FALSE;		// 默认不保持活跃
		pInfo->AuthID = 0;
		pInfo->AuthLevel = 0;
		pInfo->bDynamic = FALSE;
		pInfo->pDynamicRoute = NULL;
	} else {
		printf("add static http route failed : %s.\n", uri);
	}
}

static void DynamicRoute_ContextKey(char sKey[32])
{
	snprintf(sKey, 32, "%llu", (unsigned long long)xrtThreadGetCurrentId());
}

static void DynamicRoute_BeginInvoke(RouteInfo* pInfo, const char* sPath)
{
	DynamicRouteInvokeContext* pCtx;
	DynamicRouteInvokeContext* pOldCtx = NULL;
	DynamicRouteInfo* pRoute;
	char sKey[32];

	if ( pInfo == NULL || !pInfo->bDynamic || pInfo->pDynamicRoute == NULL || sPath == NULL || G_DynamicRouteInvokeContext == NULL ) {
		return;
	}
	pRoute = (DynamicRouteInfo*)pInfo->pDynamicRoute;
	pCtx = (DynamicRouteInvokeContext*)xrtMalloc(sizeof(DynamicRouteInvokeContext));
	if ( pCtx == NULL ) {
		return;
	}
	memset(pCtx, 0, sizeof(DynamicRouteInvokeContext));
	pCtx->pRoute = pRoute;
	pCtx->sPath = sPath;
	pCtx->iPathLen = strlen(sPath);
	pCtx->iCaptureCount = pRoute->iCaptureCount;
	if ( pCtx->iCaptureCount > XADMIN_DYNAMIC_ROUTE_MAX_CAPTURES ) {
		pCtx->iCaptureCount = XADMIN_DYNAMIC_ROUTE_MAX_CAPTURES;
	}
	if ( pCtx->iCaptureCount == 0 || xrtRegexCaptures(pRoute->pRegex, sPath, pCtx->iPathLen, pCtx->arrCaptures, pCtx->iCaptureCount) != 1 ) {
		pCtx->iCaptureCount = 0;
	}
	DynamicRoute_ContextKey(sKey);
	xrtDictSetPtr(G_DynamicRouteInvokeContext, sKey, (uint32)strlen(sKey), pCtx, (ptr*)&pOldCtx);
	if ( pOldCtx ) {
		xrtFree(pOldCtx);
	}
}

static void DynamicRoute_EndInvoke()
{
	DynamicRouteInvokeContext* pCtx;
	char sKey[32];

	if ( G_DynamicRouteInvokeContext == NULL ) {
		return;
	}
	DynamicRoute_ContextKey(sKey);
	pCtx = (DynamicRouteInvokeContext*)xrtDictRemovePtr(G_DynamicRouteInvokeContext, sKey, (uint32)strlen(sKey));
	if ( pCtx ) {
		xrtFree(pCtx);
	}
}

int DynamicRoute_GetParam(int iIndex, char* sOut, size_t iOutCap)
{
	DynamicRouteInvokeContext* pCtx;
	xregexspan* pSpan;
	char sKey[32];
	uint32 iCaptureIndex;
	size_t iLen;

	if ( sOut && iOutCap > 0 ) {
		sOut[0] = '\0';
	}
	if ( G_DynamicRouteInvokeContext == NULL || iIndex < 0 || sOut == NULL || iOutCap == 0 ) {
		return -1;
	}
	DynamicRoute_ContextKey(sKey);
	pCtx = (DynamicRouteInvokeContext*)xrtDictGetPtr(G_DynamicRouteInvokeContext, sKey, (uint32)strlen(sKey));
	iCaptureIndex = (uint32)iIndex + 1;
	if ( pCtx == NULL || iCaptureIndex >= pCtx->iCaptureCount ) {
		return -1;
	}
	pSpan = &pCtx->arrCaptures[iCaptureIndex];
	if ( pSpan->iEnd < pSpan->iBegin || pSpan->iEnd > pCtx->iPathLen ) {
		return -1;
	}
	iLen = pSpan->iEnd - pSpan->iBegin;
	if ( iLen >= iOutCap ) {
		iLen = iOutCap - 1;
	}
	memcpy(sOut, pCtx->sPath + pSpan->iBegin, iLen);
	sOut[iLen] = '\0';
	return (int)(pSpan->iEnd - pSpan->iBegin);
}

int DynamicRoute_GetParamCount()
{
	DynamicRouteInvokeContext* pCtx;
	char sKey[32];

	if ( G_DynamicRouteInvokeContext == NULL ) {
		return 0;
	}
	DynamicRoute_ContextKey(sKey);
	pCtx = (DynamicRouteInvokeContext*)xrtDictGetPtr(G_DynamicRouteInvokeContext, sKey, (uint32)strlen(sKey));
	return (pCtx && pCtx->iCaptureCount > 0) ? (int)(pCtx->iCaptureCount - 1) : 0;
}

static int DynamicRoute_Compare(const void* pLeft, const void* pRight)
{
	const DynamicRouteInfo* pA = *(const DynamicRouteInfo* const*)pLeft;
	const DynamicRouteInfo* pB = *(const DynamicRouteInfo* const*)pRight;
	if ( pA == NULL && pB == NULL ) return 0;
	if ( pA == NULL ) return 1;
	if ( pB == NULL ) return -1;
	if ( pA->iPriority != pB->iPriority ) return (pA->iPriority < pB->iPriority) ? -1 : 1;
	return strcmp(pA->sUri ? pA->sUri : "", pB->sUri ? pB->sUri : "");
}

static void DynamicRoute_SortMatchIndexes(uint32* arrMatches, uint32 iMatchCount)
{
	for ( uint32 i = 1; i < iMatchCount; i++ ) {
		uint32 iValue = arrMatches[i];
		uint32 j = i;
		while ( j > 0 && arrMatches[j - 1] > iValue ) {
			arrMatches[j] = arrMatches[j - 1];
			j--;
		}
		arrMatches[j] = iValue;
	}
}

static void DynamicRoute_Free(DynamicRouteInfo* pRoute)
{
	if ( pRoute == NULL ) return;
	if ( pRoute->pRegex ) xrtRegexDestroy(pRoute->pRegex);
	if ( pRoute->sUri ) xrtFree(pRoute->sUri);
	if ( pRoute->sPattern ) xrtFree(pRoute->sPattern);
	xrtFree(pRoute);
}

static void DynamicRoute_SetError(const char* sMessage)
{
	if ( G_DynamicRouteTableHTTP.sLastError ) {
		xrtFree(G_DynamicRouteTableHTTP.sLastError);
		G_DynamicRouteTableHTTP.sLastError = NULL;
	}
	if ( sMessage ) {
		G_DynamicRouteTableHTTP.sLastError = xrtCopyStr((str)sMessage, 0);
	}
}

static void DynamicRoute_SetWarning(const char* sMessage)
{
	if ( G_DynamicRouteTableHTTP.sLastWarning ) {
		xrtFree(G_DynamicRouteTableHTTP.sLastWarning);
		G_DynamicRouteTableHTTP.sLastWarning = NULL;
	}
	if ( sMessage ) {
		G_DynamicRouteTableHTTP.sLastWarning = xrtCopyStr((str)sMessage, 0);
	}
}

static void DynamicRoute_AppendWarning(const char* sMessage)
{
	str sNext;

	if ( (sMessage == NULL) || (sMessage[0] == '\0') ) return;
	if ( G_DynamicRouteTableHTTP.sLastWarning && G_DynamicRouteTableHTTP.sLastWarning[0] ) {
		sNext = xrtFormat("%s; %s", (const char*)G_DynamicRouteTableHTTP.sLastWarning, sMessage);
	} else {
		sNext = xrtCopyStr((str)sMessage, 0);
	}
	if ( sNext ) {
		DynamicRoute_SetWarning((const char*)sNext);
		xrtFree(sNext);
	}
}

const char* DynamicRoute_GetLastErrorHTTP()
{
	return G_DynamicRouteTableHTTP.sLastError ? (const char*)G_DynamicRouteTableHTTP.sLastError : "";
}

const char* DynamicRoute_GetLastWarningHTTP()
{
	return G_DynamicRouteTableHTTP.sLastWarning ? (const char*)G_DynamicRouteTableHTTP.sLastWarning : "";
}

static void DynamicRoute_RecordPatternRisk(str uri, str pattern)
{
	str sWarning = NULL;

	if ( pattern == NULL ) {
		return;
	}
	if ( strcmp(pattern, "^/.*$") == 0 || strcmp(pattern, "^/(.*)$") == 0 || strcmp(pattern, "^/([^/]+)$") == 0 ) {
		sWarning = xrtFormat("broad pattern: uri=%s pattern=%s", uri ? (const char*)uri : "", pattern);
	}
	if ( strstr(pattern, "/css") || strstr(pattern, "/js") || strstr(pattern, "/img") || strstr(pattern, "/uploads") || strstr(pattern, "/res") ) {
		str sNext = xrtFormat("%s%sstatic asset overlap risk: uri=%s pattern=%s",
			sWarning ? (const char*)sWarning : "",
			sWarning ? "; " : "",
			uri ? (const char*)uri : "",
			pattern);
		if ( sWarning ) xrtFree(sWarning);
		sWarning = sNext;
	}
	if ( strstr(pattern, "^/admin") || strstr(pattern, "^/api") || strstr(pattern, "/admin") || strstr(pattern, "/api") ) {
		str sNext = xrtFormat("%s%sadmin/API prefix overlap risk: uri=%s pattern=%s",
			sWarning ? (const char*)sWarning : "",
			sWarning ? "; " : "",
			uri ? (const char*)uri : "",
			pattern);
		if ( sWarning ) xrtFree(sWarning);
		sWarning = sNext;
	}
	if ( sWarning ) {
		DynamicRoute_SetWarning((const char*)sWarning);
		printf("[dynamic_route:warn] %s\n", (const char*)sWarning);
		xrtFree(sWarning);
	}
}

static bool DynamicRoute_RebuildHTTP()
{
	uint32 iCount = G_DynamicRouteTableHTTP.lstRoutes ? xrtListCount(G_DynamicRouteTableHTTP.lstRoutes) : 0;
	DynamicRouteInfo** arrRoutes = NULL;
	xregexsetbuilder* pBuilder = NULL;
	xregexset* pNewSet = NULL;
	xregexset* pOldSet = G_DynamicRouteTableHTTP.pRegexSet;
	uint32 iOldCompiledCount = G_DynamicRouteTableHTTP.iCompiledCount;
	uint32 iActive = 0;
	int iRet;

	DynamicRoute_SetError(NULL);

	if ( iCount == 0 ) {
		G_DynamicRouteTableHTTP.pRegexSet = NULL;
		G_DynamicRouteTableHTTP.iCompiledCount = 0;
		if ( pOldSet ) {
			xrtRegexSetDestroy(pOldSet);
		}
		G_DynamicRouteTableHTTP.iGeneration++;
		return TRUE;
	}

	arrRoutes = (DynamicRouteInfo**)xrtMalloc(sizeof(DynamicRouteInfo*) * iCount);
	if ( arrRoutes == NULL ) {
		G_DynamicRouteTableHTTP.pRegexSet = pOldSet;
		G_DynamicRouteTableHTTP.iCompiledCount = iOldCompiledCount;
		DynamicRoute_SetError("dynamic route rebuild out of memory");
		return FALSE;
	}
	memset(arrRoutes, 0, sizeof(DynamicRouteInfo*) * iCount);
	for ( uint32 i = 0; i < iCount; i++ ) {
		arrRoutes[i] = (DynamicRouteInfo*)xrtListGetPtr(G_DynamicRouteTableHTTP.lstRoutes, i);
	}
	qsort(arrRoutes, iCount, sizeof(DynamicRouteInfo*), DynamicRoute_Compare);

	iRet = xrtRegexSetBuilderCreate(&pBuilder, NULL);
	if ( iRet != 0 || pBuilder == NULL ) {
		xrtFree(arrRoutes);
		G_DynamicRouteTableHTTP.pRegexSet = pOldSet;
		G_DynamicRouteTableHTTP.iCompiledCount = iOldCompiledCount;
		DynamicRoute_SetError("dynamic route regex set builder create failed");
		return FALSE;
	}
	for ( uint32 i = 0; i < iCount; i++ ) {
		DynamicRouteInfo* pRoute = arrRoutes[i];
		if ( pRoute == NULL || pRoute->pRegex == NULL ) continue;
		iRet = xrtRegexSetBuilderAdd(pBuilder, pRoute->pRegex);
		if ( iRet != 0 ) {
			xrtRegexSetBuilderDestroy(pBuilder);
			xrtFree(arrRoutes);
			G_DynamicRouteTableHTTP.pRegexSet = pOldSet;
			G_DynamicRouteTableHTTP.iCompiledCount = iOldCompiledCount;
			DynamicRoute_SetError("dynamic route regex set add failed");
			return FALSE;
		}
		iActive++;
	}
	if ( iActive > 0 ) {
		iRet = xrtRegexSetCreateFromBuilder(&pNewSet, pBuilder, NULL);
		if ( iRet != 0 || pNewSet == NULL ) {
			xrtRegexSetBuilderDestroy(pBuilder);
			xrtFree(arrRoutes);
			G_DynamicRouteTableHTTP.pRegexSet = pOldSet;
			G_DynamicRouteTableHTTP.iCompiledCount = iOldCompiledCount;
			DynamicRoute_SetError("dynamic route regex set compile failed");
			return FALSE;
		}
	}
	xrtRegexSetBuilderDestroy(pBuilder);
	iActive = 0;
	for ( uint32 i = 0; i < iCount; i++ ) {
		DynamicRouteInfo* pRoute = arrRoutes[i];
		xrtListSetPtr(G_DynamicRouteTableHTTP.lstRoutes, i, pRoute, NULL);
		if ( pRoute == NULL || pRoute->pRegex == NULL ) continue;
		pRoute->iPatternIndex = iActive++;
	}
	xrtFree(arrRoutes);
	G_DynamicRouteTableHTTP.pRegexSet = pNewSet;
	G_DynamicRouteTableHTTP.iCompiledCount = iActive;
	if ( iActive > XADMIN_DYNAMIC_ROUTE_MAX_MATCHES ) {
		str sCapacityWarning = xrtFormat("dynamic route candidate limit warning: compiled=%u limit=%u", iActive, XADMIN_DYNAMIC_ROUTE_MAX_MATCHES);
		if ( sCapacityWarning ) {
			DynamicRoute_AppendWarning((const char*)sCapacityWarning);
			printf("[dynamic_route:warn] %s\n", (const char*)sCapacityWarning);
			xrtFree(sCapacityWarning);
		}
	}
	if ( pOldSet ) {
		xrtRegexSetDestroy(pOldSet);
	}
	G_DynamicRouteTableHTTP.iGeneration++;
	return TRUE;
}

bool AddDynamicRouteHTTPEx(str uri, str pattern, void* proc, int iPriority, int iMethod)
{
	DynamicRouteInfo* pRoute;
	DynamicRouteInfo* pOldRoute = NULL;
	xregex* pRegex;
	const char* sRegexError;
	int iOldIndex = -1;

	if ( (uri == NULL) || (uri[0] == '\0') || (pattern == NULL) || (pattern[0] == '\0') || (proc == NULL) ) {
		DynamicRoute_SetError("dynamic route requires uri, pattern and proc");
		return FALSE;
	}
	if ( G_DynamicRouteTableHTTP.lstRoutes == NULL ) {
		G_DynamicRouteTableHTTP.lstRoutes = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
		if ( G_DynamicRouteTableHTTP.lstRoutes ) {
			xrtOwnerActivateShared(&G_DynamicRouteTableHTTP.lstRoutes->Owner);
			xrtOwnerActivateShared(&G_DynamicRouteTableHTTP.lstRoutes->AVLT.Owner);
		}
	}
	if ( G_DynamicRouteTableHTTP.lstRoutes == NULL ) {
		DynamicRoute_SetError("dynamic route list create failed");
		return FALSE;
	}
	if ( G_StaticRouteTableHTTP && xrtDictGet(G_StaticRouteTableHTTP, uri, (uint32)strlen(uri)) != NULL ) {
		DynamicRoute_SetError("dynamic route uri conflicts with static route");
		return FALSE;
	}
	DynamicRoute_SetWarning(NULL);
	DynamicRoute_RecordPatternRisk(uri, pattern);

	pRegex = xrtRegexCreate(pattern);
	sRegexError = pRegex ? xrtRegexGetErrorMsg(pRegex) : NULL;
	if ( pRegex == NULL || (sRegexError && sRegexError[0]) ) {
		if ( pRegex ) xrtRegexDestroy(pRegex);
		DynamicRoute_SetError(sRegexError && sRegexError[0] ? sRegexError : "dynamic route regex compile failed");
		return FALSE;
	}

	for ( uint32 i = 0; i < xrtListCount(G_DynamicRouteTableHTTP.lstRoutes); i++ ) {
		DynamicRouteInfo* pOld = (DynamicRouteInfo*)xrtListGetPtr(G_DynamicRouteTableHTTP.lstRoutes, i);
		if ( pOld && pOld->sUri && strcmp(pOld->sUri, uri) == 0 ) {
			pOldRoute = (DynamicRouteInfo*)xrtListRemovePtr(G_DynamicRouteTableHTTP.lstRoutes, i);
			iOldIndex = (int)i;
			break;
		}
	}

	pRoute = (DynamicRouteInfo*)xrtMalloc(sizeof(DynamicRouteInfo));
	if ( pRoute == NULL ) {
		xrtRegexDestroy(pRegex);
		if ( pOldRoute ) {
			xrtListSetPtr(G_DynamicRouteTableHTTP.lstRoutes, iOldIndex >= 0 ? (uint32)iOldIndex : xrtListCount(G_DynamicRouteTableHTTP.lstRoutes), pOldRoute, NULL);
			DynamicRoute_RebuildHTTP();
		}
		DynamicRoute_SetError("dynamic route alloc failed");
		return FALSE;
	}
	memset(pRoute, 0, sizeof(DynamicRouteInfo));
	pRoute->sUri = xrtCopyStr(uri, 0);
	pRoute->sPattern = xrtCopyStr(pattern, 0);
	pRoute->pRegex = pRegex;
	pRoute->iPriority = iPriority;
	pRoute->iMethod = iMethod;
	pRoute->iCaptureCount = xrtRegexCaptureCount(pRegex);
	if ( pRoute->iCaptureCount > XADMIN_DYNAMIC_ROUTE_MAX_CAPTURES ) {
		str sCaptureWarning = xrtFormat("dynamic route capture count exceeds route param limit: captures=%u limit=%u", pRoute->iCaptureCount, XADMIN_DYNAMIC_ROUTE_MAX_CAPTURES);
		if ( sCaptureWarning ) {
			DynamicRoute_AppendWarning((const char*)sCaptureWarning);
			printf("[dynamic_route:warn] %s\n", (const char*)sCaptureWarning);
			xrtFree(sCaptureWarning);
		}
	}
	pRoute->Info.Proc = proc;
	pRoute->Info.bAuth = TRUE;
	pRoute->Info.bAdmin = TRUE;
	pRoute->Info.bDynamic = TRUE;
	pRoute->Info.pDynamicRoute = pRoute;
	if ( (pRoute->sUri == NULL) || (pRoute->sPattern == NULL) ) {
		DynamicRoute_Free(pRoute);
		if ( pOldRoute ) {
			xrtListSetPtr(G_DynamicRouteTableHTTP.lstRoutes, iOldIndex >= 0 ? (uint32)iOldIndex : xrtListCount(G_DynamicRouteTableHTTP.lstRoutes), pOldRoute, NULL);
			DynamicRoute_RebuildHTTP();
		}
		DynamicRoute_SetError("dynamic route string alloc failed");
		return FALSE;
	}

	xrtListSetPtr(G_DynamicRouteTableHTTP.lstRoutes, xrtListCount(G_DynamicRouteTableHTTP.lstRoutes), pRoute, NULL);
	if ( !DynamicRoute_RebuildHTTP() ) {
		xrtListRemovePtr(G_DynamicRouteTableHTTP.lstRoutes, xrtListCount(G_DynamicRouteTableHTTP.lstRoutes) - 1);
		DynamicRoute_Free(pRoute);
		if ( pOldRoute ) {
			xrtListSetPtr(G_DynamicRouteTableHTTP.lstRoutes, iOldIndex >= 0 ? (uint32)iOldIndex : xrtListCount(G_DynamicRouteTableHTTP.lstRoutes), pOldRoute, NULL);
		}
		DynamicRoute_RebuildHTTP();
		return FALSE;
	}
	if ( pOldRoute ) {
		DynamicRoute_Free(pOldRoute);
	}
	return TRUE;
}

bool AddDynamicRouteHTTP(str uri, str pattern, void* proc)
{
	return AddDynamicRouteHTTPEx(uri, pattern, proc, 1000, 0);
}

bool RemoveDynamicRouteHTTP(str uri)
{
	if ( (uri == NULL) || (G_DynamicRouteTableHTTP.lstRoutes == NULL) ) return FALSE;
	for ( uint32 i = 0; i < xrtListCount(G_DynamicRouteTableHTTP.lstRoutes); i++ ) {
		DynamicRouteInfo* pRoute = (DynamicRouteInfo*)xrtListGetPtr(G_DynamicRouteTableHTTP.lstRoutes, i);
		if ( pRoute && pRoute->sUri && strcmp(pRoute->sUri, uri) == 0 ) {
			DynamicRouteInfo* pRemoved = (DynamicRouteInfo*)xrtListRemovePtr(G_DynamicRouteTableHTTP.lstRoutes, i);
			if ( DynamicRoute_RebuildHTTP() ) {
				DynamicRoute_Free(pRemoved);
				return TRUE;
			}
			xrtListSetPtr(G_DynamicRouteTableHTTP.lstRoutes, i, pRemoved, NULL);
			DynamicRoute_RebuildHTTP();
			return FALSE;
		}
	}
	return FALSE;
}

RouteInfo* FindDynamicRouteHTTP(str uri)
{
	if ( (uri == NULL) || (G_DynamicRouteTableHTTP.lstRoutes == NULL) ) return NULL;
	for ( uint32 i = 0; i < xrtListCount(G_DynamicRouteTableHTTP.lstRoutes); i++ ) {
		DynamicRouteInfo* pRoute = (DynamicRouteInfo*)xrtListGetPtr(G_DynamicRouteTableHTTP.lstRoutes, i);
		if ( pRoute && pRoute->sUri && strcmp(pRoute->sUri, uri) == 0 ) {
			return &pRoute->Info;
		}
	}
	return NULL;
}

RouteInfo* MatchDynamicRouteHTTP(const char* sPath, int iMethod)
{
	uint32 arrMatches[XADMIN_DYNAMIC_ROUTE_MAX_MATCHES];
	uint32 iMatchCount = 0;
	size_t iPathLen;

	if ( (sPath == NULL) || (G_DynamicRouteTableHTTP.pRegexSet == NULL) || (G_DynamicRouteTableHTTP.iCompiledCount == 0) ) return NULL;
	iPathLen = strlen(sPath);
	if ( xrtRegexSetMatches(G_DynamicRouteTableHTTP.pRegexSet, sPath, iPathLen, arrMatches, XADMIN_DYNAMIC_ROUTE_MAX_MATCHES, &iMatchCount) != 0 || iMatchCount == 0 ) return NULL;
	DynamicRoute_SortMatchIndexes(arrMatches, iMatchCount);

	for ( uint32 i = 0; i < iMatchCount; i++ ) {
		uint32 iPatternIndex = arrMatches[i];
		DynamicRouteInfo* pRoute = NULL;
		if ( iPatternIndex < xrtListCount(G_DynamicRouteTableHTTP.lstRoutes) ) {
			pRoute = (DynamicRouteInfo*)xrtListGetPtr(G_DynamicRouteTableHTTP.lstRoutes, iPatternIndex);
			if ( pRoute && pRoute->iPatternIndex != iPatternIndex ) pRoute = NULL;
		}
		if ( pRoute == NULL || pRoute->pRegex == NULL ) continue;
		if ( pRoute->iMethod != 0 && pRoute->iMethod != iMethod ) continue;
		{
			xregexspan span[1];
			if ( xrtRegexCaptures(pRoute->pRegex, sPath, iPathLen, span, 1) == 1 && span[0].iBegin == 0 && span[0].iEnd == iPathLen ) {
				pRoute->Info.pDynamicRoute = pRoute;
				return &pRoute->Info;
			}
		}
	}
	return NULL;
}



// 后台全局权限

// xadmin page helper api
void LoadPage(XS_ResponseObject objResp, int iCode, str sHead, str sPage);
void LoadSitePage(XS_ResponseObject objResp, int iCode, str sHead, str sPage);
void XS_ImportScriptAPI(TCCState* s);

static void XAdminTCCErrorHandler(void* pOpaque, const char* sMsg)
{
	(void)pOpaque;
	fprintf(stderr, "[TCC] %s\n", sMsg);
}

static void XAdminTCCAddIncludePathEx(TCCState* s, const char* sBasePath, const char* sRelPath)
{
	str sPath;
	
	if ( (s == NULL) || (sRelPath == NULL) || (sRelPath[0] == '\0') ) {
		return;
	}
	
	if ( (sBasePath != NULL) && (sBasePath[0] != '\0') ) {
		sPath = xrtPathJoin(2, (str)sBasePath, (str)sRelPath);
		if ( sPath ) {
			if ( xrtDirExists(sPath) ) {
				tcc_add_include_path(s, sPath);
			}
			xrtFree(sPath);
		}
	}
	
	tcc_add_include_path(s, sRelPath);
}

static void XAdminTCCAddLibraryPathEx(TCCState* s, const char* sBasePath, const char* sRelPath)
{
	str sPath;
	
	if ( (s == NULL) || (sRelPath == NULL) || (sRelPath[0] == '\0') ) {
		return;
	}
	
	if ( (sBasePath != NULL) && (sBasePath[0] != '\0') ) {
		sPath = xrtPathJoin(2, (str)sBasePath, (str)sRelPath);
		if ( sPath ) {
			if ( xrtDirExists(sPath) ) {
				tcc_add_library_path(s, sPath);
			}
			xrtFree(sPath);
		}
	}
	
	tcc_add_library_path(s, sRelPath);
}

TCCState* xsCreateTCC(const char* sWorkPath)
{
	TCCState* s = tcc_new();
	const char* sAppPath = xsAppPath();
	
	if ( s == NULL ) {
		return NULL;
	}
	
	tcc_set_error_func(s, stderr, XAdminTCCErrorHandler);
	
	#if defined(_WIN32) || defined(_WIN64)
		XAdminTCCAddIncludePathEx(s, sAppPath, "tcc/include_win/winapi");
		XAdminTCCAddIncludePathEx(s, sAppPath, "tcc/include_win");
	#else
		XAdminTCCAddIncludePathEx(s, sAppPath, "tcc/include_linux");
		tcc_add_include_path(s, "/usr/include");
		tcc_add_include_path(s, "/usr/include/i386-linux-gnu");
		tcc_add_include_path(s, "/usr/include/i386-linux-gnu/sys");
		tcc_add_include_path(s, "/usr/include/x86_64-linux-gnu");
		tcc_add_include_path(s, "/usr/include/x86_64-linux-gnu/sys");
		tcc_add_library_path(s, "/usr/lib");
		tcc_add_library_path(s, "/usr/lib/i386-linux-gnu");
		tcc_add_library_path(s, "/usr/lib/x86_64-linux-gnu");
	#endif
	
	XAdminTCCAddIncludePathEx(s, sAppPath, "tcc/inc_xs");
	XAdminTCCAddIncludePathEx(s, sAppPath, "tcc/include");
	XAdminTCCAddLibraryPathEx(s, sAppPath, "tcc/lib");
	
	if ( (sWorkPath != NULL) && (sWorkPath[0] != '\0') ) {
		tcc_add_include_path(s, sWorkPath);
		tcc_add_library_path(s, sWorkPath);
	}
	
	tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
	XS_ImportScriptAPI(s);
	return s;
}

void xsDestroyTCC(TCCState* s)
{
	if ( s ) {
		tcc_delete(s);
	}
}

xvalue G_CACHE_RoleAuth = NULL;					// 角色权限缓存 - 后端鉴权查表�?
xvalue G_CACHE_Auth = NULL;						// 权限分组缓存 - 前端列表渲染�?
xvalue G_CACHE_Group = NULL;					// 权限分类缓存 - 前端列表渲染�?
xvalue G_CACHE_Role = NULL;						// 角色列表缓存 - 前端列表渲染�?
xvalue G_CACHE_RoleAuthIndex = NULL;			// role id -> permission array index
xvalue G_CACHE_RoleAuthLevel = NULL;			// role id -> authLevel

// 前台全局权限�?
xvalue G_CACHE_MemberGroupAuth = NULL;			// 前台用户组权限缓�?- 鉴权查表�?
xvalue G_CACHE_MemberAuth = NULL;				// 前台权限分组缓存
xvalue G_CACHE_MemberAuthGroup = NULL;			// 前台权限分类缓存
xvalue G_CACHE_MemberGroup = NULL;				// 前台用户组列表缓�?
xvalue G_CACHE_MemberGroupAuthIndex = NULL;	// group id -> permission array index
xvalue G_CACHE_MemberGroupAuthLevel = NULL;	// group id -> authLevel



// 预编译的 SQL 语句 - uris
sqlite3_stmt* stmt_uris_all = NULL;				// 分页获取所�?URI 数据
sqlite3_stmt* stmt_uris_sel = NULL;				// 分页条件查询 URI 数据
sqlite3_stmt* stmt_uris_add = NULL;				// 添加 URI 记录
sqlite3_stmt* stmt_uris_del = NULL;				// 删除 URI 记录
sqlite3_stmt* stmt_uris_put = NULL;				// 修改 URI 记录
sqlite3_stmt* stmt_uris_get = NULL;				// 根据 ID 获取 URI 记录

// 预编译的 SQL 语句 - auth
sqlite3_stmt* stmt_auth_all = NULL;				// 分页获取所有权限组数据
sqlite3_stmt* stmt_auth_sel = NULL;				// 分页条件查询权限组数�?
sqlite3_stmt* stmt_auth_add = NULL;				// 添加权限组记�?
sqlite3_stmt* stmt_auth_del = NULL;				// 删除权限组记录（软删除）
sqlite3_stmt* stmt_auth_put = NULL;				// 修改权限组记�?
sqlite3_stmt* stmt_auth_get = NULL;				// 根据 ID 获取权限组记�?
sqlite3_stmt* stmt_auth_sum = NULL;				// 统计关联�?URI 权限数量
sqlite3_stmt* stmt_auth_mov = NULL;				// 移动权限组下�?URI 权限到默认分�?

// 预编译的 SQL 语句 - authGroup
sqlite3_stmt* stmt_group_all = NULL;			// 分页获取所有权限分类数�?
sqlite3_stmt* stmt_group_sel = NULL;			// 分页条件查询权限分类数据
sqlite3_stmt* stmt_group_add = NULL;			// 添加权限分类记录
sqlite3_stmt* stmt_group_del = NULL;			// 删除权限分类记录（软删除�?
sqlite3_stmt* stmt_group_put = NULL;			// 修改权限分类记录
sqlite3_stmt* stmt_group_get = NULL;			// 根据 ID 获取权限分类记录
sqlite3_stmt* stmt_group_sum = NULL;			// 统计关联的权限组数量
sqlite3_stmt* stmt_group_mov = NULL;			// 移动权限分类下的权限组到默认分类

// 预编译的 SQL 语句 - role
sqlite3_stmt* stmt_role_all = NULL;				// 分页获取所有角色数�?
sqlite3_stmt* stmt_role_sel = NULL;				// 分页条件查询角色数据
sqlite3_stmt* stmt_role_get = NULL;				// 根据 ID 获取角色记录
sqlite3_stmt* stmt_role_add = NULL;				// 添加角色记录
sqlite3_stmt* stmt_role_put = NULL;				// 修改角色记录
sqlite3_stmt* stmt_role_del = NULL;				// 删除角色记录（软删除�?
sqlite3_stmt* stmt_role_sum = NULL;				// 统计关联的用户数�?

// 预编译的 SQL 语句 - user
sqlite3_stmt* stmt_user_all = NULL;				// 分页获取所有用户数�?
sqlite3_stmt* stmt_user_sel = NULL;				// 分页条件查询用户数据
sqlite3_stmt* stmt_user_get = NULL;				// 根据 ID 获取用户记录
sqlite3_stmt* stmt_user_add = NULL;				// 添加用户记录
sqlite3_stmt* stmt_user_put = NULL;				// 修改用户记录
sqlite3_stmt* stmt_user_del = NULL;				// 删除用户记录（软删除�?
sqlite3_stmt* stmt_user_chk = NULL;				// 检查用户名是否已存�?
sqlite3_stmt* stmt_user_pwd = NULL;				// 修改用户密码

// 预编译的 SQL 语句 - 登录相关
sqlite3_stmt* stmt_login_get = NULL;			// 根据用户名和密码获取用户信息

// 预编译的 SQL 语句 - 缓存相关
sqlite3_stmt* stmt_cache_auth = NULL;			// 获取所有权限组数据（缓存用�?
sqlite3_stmt* stmt_cache_group = NULL;			// 获取所有权限分类数据（缓存用）
sqlite3_stmt* stmt_cache_role = NULL;			// 获取所有角色数据（缓存用）
sqlite3_stmt* stmt_cache_uris = NULL;			// 获取所有URI记录（用于更新URI表）

// ==================== 前台用户系统预编译SQL ====================

// 预编译的 SQL 语句 - member
sqlite3_stmt* stmt_member_all = NULL;			// 分页获取所有前台用户数据
sqlite3_stmt* stmt_member_sel = NULL;			// 分页条件查询前台用户数据
sqlite3_stmt* stmt_member_get = NULL;			// 根据 ID 获取前台用户记录
sqlite3_stmt* stmt_member_add = NULL;			// 添加前台用户记录
sqlite3_stmt* stmt_member_put = NULL;			// 修改前台用户记录
sqlite3_stmt* stmt_member_del = NULL;			// 删除前台用户记录（软删除）
sqlite3_stmt* stmt_member_chk = NULL;			// 检查用户名是否已存在
sqlite3_stmt* stmt_member_pwd = NULL;			// 修改用户密码
sqlite3_stmt* stmt_member_balance = NULL;		// 修改用户余额

// 预编译的 SQL 语句 - memberGroup
sqlite3_stmt* stmt_mgroup_all = NULL;			// 分页获取所有前台用户组数据
sqlite3_stmt* stmt_mgroup_sel = NULL;			// 分页条件查询前台用户组数据
sqlite3_stmt* stmt_mgroup_get = NULL;			// 根据 ID 获取前台用户组记录
sqlite3_stmt* stmt_mgroup_add = NULL;			// 添加前台用户组记录
sqlite3_stmt* stmt_mgroup_put = NULL;			// 修改前台用户组记录
sqlite3_stmt* stmt_mgroup_del = NULL;			// 删除前台用户组记录（软删除）
sqlite3_stmt* stmt_mgroup_sum = NULL;			// 统计关联的用户数量

// 预编译的 SQL 语句 - memberAuthGroup
sqlite3_stmt* stmt_magroup_all = NULL;			// 分页获取所有前台权限分类数据
sqlite3_stmt* stmt_magroup_sel = NULL;			// 分页条件查询前台权限分类数据
sqlite3_stmt* stmt_magroup_get = NULL;			// 根据 ID 获取前台权限分类记录
sqlite3_stmt* stmt_magroup_add = NULL;			// 添加前台权限分类记录
sqlite3_stmt* stmt_magroup_put = NULL;			// 修改前台权限分类记录
sqlite3_stmt* stmt_magroup_del = NULL;			// 删除前台权限分类记录（软删除）
sqlite3_stmt* stmt_magroup_sum = NULL;			// 统计关联的权限分组数量
sqlite3_stmt* stmt_magroup_mov = NULL;			// 移动权限分类下的权限分组到默认分类

// 预编译的 SQL 语句 - memberAuth
sqlite3_stmt* stmt_mauth_all = NULL;			// 分页获取所有前台权限分组数据
sqlite3_stmt* stmt_mauth_sel = NULL;			// 分页条件查询前台权限分组数据
sqlite3_stmt* stmt_mauth_get = NULL;			// 根据 ID 获取前台权限分组记录
sqlite3_stmt* stmt_mauth_add = NULL;			// 添加前台权限分组记录
sqlite3_stmt* stmt_mauth_put = NULL;			// 修改前台权限分组记录
sqlite3_stmt* stmt_mauth_del = NULL;			// 删除前台权限分组记录（软删除）
sqlite3_stmt* stmt_mauth_sum = NULL;			// 统计关联的 URI 权限数量
sqlite3_stmt* stmt_mauth_mov = NULL;			// 移动权限分组下的 URI 权限到默认分类


// 预编译的 SQL 语句 - memberBalanceLog
sqlite3_stmt* stmt_mbalance_all = NULL;		// 分页获取余额变动日志
sqlite3_stmt* stmt_mbalance_add = NULL;		// 添加余额变动日志

// 预编译的 SQL 语句 - 前台登录相关
sqlite3_stmt* stmt_member_login = NULL;		// 根据用户名获取前台用户信息

// 预编译的 SQL 语句 - 前台缓存相关
sqlite3_stmt* stmt_cache_mauth = NULL;			// 获取所有前台权限分组数据（缓存用）
sqlite3_stmt* stmt_cache_magroup = NULL;		// 获取所有前台权限分类数据（缓存用）
sqlite3_stmt* stmt_cache_mgroup = NULL;		// 获取所有前台用户组数据（缓存用）
sqlite3_stmt* stmt_cache_muris = NULL;			// 获取所有前台URI记录（从uris表筛选isBackend=0）



// 初始化全局定义
void Define_Init(XS_ServerObject objServer, XS_HostObject objHost)
{
	printf("        Define_Init \n");
	ExePath = (str)xsAppPath();
	WebPath = (str)xsHostPath(objHost);
	AppPath = xrtPathGetDir(WebPath, 0);
	DBPath = xrtPathJoin(3, AppPath, "data", "db");
	LogPath = xrtPathJoin(3, AppPath, "data", "logs");
	TempPath = xrtPathJoin(3, AppPath, "data", "temp");
	PagePath = xrtPathJoin(3, AppPath, "data", "page");
	SitePagePath = xrtPathJoin(4, AppPath, "data", "site", "page");
	ToolPath = xrtPathJoin(2, ExePath, "tools");
	OptionPath = xrtPathJoin(3, AppPath, "data", "options");
	InstallPath = xrtPathJoin(3, AppPath, "data", "install");
	TemplatePath = xrtPathJoin(3, AppPath, "data", "template");
	
	// 自动创建目录
	xrtDirCreate(LogPath);
	xrtDirCreate(TempPath);
}



// 卸载全局数据
void Define_Unit()
{
	printf("        Define_Unit \n");
	xrtFree(AppPath);
	xrtFree(DBPath);
	xrtFree(LogPath);
	xrtFree(TempPath);
	xrtFree(PagePath);
	xrtFree(SitePagePath);
	xrtFree(ToolPath);
	xrtFree(OptionPath);
	xrtFree(InstallPath);
	xrtFree(TemplatePath);
}


