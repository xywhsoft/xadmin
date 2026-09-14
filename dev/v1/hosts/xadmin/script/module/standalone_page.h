#ifndef XADMIN_STANDALONE_PAGE_H
#define XADMIN_STANDALONE_PAGE_H

typedef struct StandalonePageItem {
	int64 id;
	str title;
	str status;
	str uris;
	str header;
	str content;
	bool useTemplate;
	int64 cacheSeconds;
	str cachedBody;
	size_t cachedSize;
	xtime cacheExpire;
} StandalonePageItem;

xlist G_StandalonePages = NULL;
void Request_StandalonePage(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);

static const char* Standalone_CStr(str sText)
{
	return sText ? (const char*)sText : "";
}

static bool Standalone_IsEnabled(StandalonePageItem* pPage)
{
	return pPage && pPage->status && strcmp((const char*)pPage->status, "enabled") == 0;
}

static str Standalone_CopyColumn(sqlite3_stmt* stmt, int iCol)
{
	const unsigned char* sText = sqlite3_column_text(stmt, iCol);
	return xrtCopyStr((str)(sText ? sText : (const unsigned char*)""), 0);
}

static char* Standalone_TrimInPlace(char* sText)
{
	char* sStart = sText;
	char* sEnd;
	if ( sText == NULL ) return NULL;
	while ( *sStart == ' ' || *sStart == '\t' || *sStart == '\r' || *sStart == '\n' ) sStart++;
	sEnd = sStart + strlen(sStart);
	while ( sEnd > sStart && (sEnd[-1] == ' ' || sEnd[-1] == '\t' || sEnd[-1] == '\r' || sEnd[-1] == '\n') ) sEnd--;
	*sEnd = '\0';
	return sStart;
}

static bool Standalone_ForEachUri(const char* sUris, bool (*proc)(const char* sUri, void* pCtx), void* pCtx)
{
	str sCopy;
	char* sLine;
	char* sNext;
	bool bStop = FALSE;

	if ( sUris == NULL || proc == NULL ) return FALSE;
	sCopy = xrtCopyStr((str)sUris, 0);
	if ( sCopy == NULL ) return FALSE;
	sLine = (char*)sCopy;
	while ( sLine && *sLine ) {
		sNext = strchr(sLine, '\n');
		if ( sNext ) {
			*sNext = '\0';
			sNext++;
		}
		sLine = Standalone_TrimInPlace(sLine);
		if ( sLine[0] != '\0' ) {
			if ( proc(sLine, pCtx) ) {
				bStop = TRUE;
				break;
			}
		}
		sLine = sNext;
	}
	xrtFree(sCopy);
	return bStop;
}

static void Standalone_FreePage(StandalonePageItem* pPage)
{
	if ( pPage == NULL ) return;
	if ( pPage->title ) xrtFree(pPage->title);
	if ( pPage->status ) xrtFree(pPage->status);
	if ( pPage->uris ) xrtFree(pPage->uris);
	if ( pPage->header ) xrtFree(pPage->header);
	if ( pPage->content ) xrtFree(pPage->content);
	if ( pPage->cachedBody ) xrtFree(pPage->cachedBody);
	xrtFree(pPage);
}

static str Standalone_NormalizeHeader(str sHeader, str* psError)
{
	size_t iLen;
	str sOut;
	size_t iOut = 0;
	if ( psError ) *psError = NULL;
	if ( sHeader == NULL || sHeader[0] == '\0' ) {
		return xrtCopyStr(HTTP_CT_HTML, 0);
	}
	for ( size_t i = 0; sHeader[i] != '\0'; i++ ) {
		if ( sHeader[i] == '\r' && sHeader[i + 1] != '\n' ) {
			if ( psError ) *psError = xrtCopyStr("invalid header newline", 0);
			return NULL;
		}
	}
	iLen = strlen((const char*)sHeader);
	sOut = xrtMalloc((uint32)(iLen * 2 + 4));
	if ( sOut == NULL ) {
		if ( psError ) *psError = xrtCopyStr("header alloc failed", 0);
		return NULL;
	}
	for ( size_t i = 0; i < iLen; i++ ) {
		if ( sHeader[i] == '\n' && (i == 0 || sHeader[i - 1] != '\r') ) {
			sOut[iOut++] = '\r';
			sOut[iOut++] = '\n';
		 } else {
			sOut[iOut++] = sHeader[i];
		}
	}
	if ( iOut < 2 || sOut[iOut - 2] != '\r' || sOut[iOut - 1] != '\n' ) {
		sOut[iOut++] = '\r';
		sOut[iOut++] = '\n';
	}
	sOut[iOut] = '\0';
	return sOut;
}

static void Standalone_ClearPageCache(StandalonePageItem* pPage)
{
	if ( pPage == NULL ) return;
	if ( pPage->cachedBody ) {
		xrtFree(pPage->cachedBody);
		pPage->cachedBody = NULL;
	}
	pPage->cachedSize = 0;
	pPage->cacheExpire = 0;
}

static bool Standalone_RenderPage(StandalonePageItem* pPage, XS_ServerObject objServer, XS_HostObject objHost, str* psBody, size_t* piSize, str* psError)
{
	xvalue tblData = NULL;
	xtetemplate hTemplate = NULL;
	XTE_Error tError = {0};
	str sBody = NULL;
	size_t iSize = 0;
	xtime tNow = xrtNow();
	bool bDebug = (xsServerDebug(objServer) || xsHostDebug(objHost) || xsHostDevMode(objHost)) ? TRUE : FALSE;

	(void)bDebug;
	if ( psBody ) *psBody = NULL;
	if ( piSize ) *piSize = 0;
	if ( psError ) *psError = NULL;
	if ( pPage == NULL ) return FALSE;

	if ( pPage->cachedBody && (pPage->cacheSeconds <= 0 || pPage->cacheExpire > tNow) ) {
		if ( psBody ) *psBody = xrtCopyStr(pPage->cachedBody, pPage->cachedSize);
		if ( piSize ) *piSize = pPage->cachedSize;
		return TRUE;
	}

	if ( !pPage->useTemplate ) {
		sBody = xrtCopyStr(pPage->content ? pPage->content : (str)"", 0);
		iSize = sBody ? strlen((const char*)sBody) : 0;
	} else {
		tblData = xvoCreateTable();
		hTemplate = xteParseEx(G_TemplateEngine, (const char*)Standalone_CStr(pPage->content), strlen(Standalone_CStr(pPage->content)), &G_TemplateParseOptions, &tError);
		if ( hTemplate == NULL ) {
			if ( psError ) {
				*psError = xrtFormat("template parse failed: code=%d desc=%s line=%u col=%u", tError.iCode, tError.sDesc ? tError.sDesc : "unknown", tError.iLine, tError.iColumn);
			}
			if ( tblData ) xvoUnref(tblData);
			return FALSE;
		}
		sBody = Template_RenderCompiledTemplate(hTemplate, tblData, &iSize, psError);
		xteDestroyTemplate(hTemplate);
		xvoUnref(tblData);
		if ( sBody == NULL ) return FALSE;
	}

	if ( pPage->cacheSeconds > 0 || !pPage->useTemplate ) {
		Standalone_ClearPageCache(pPage);
		pPage->cachedBody = xrtCopyStr(sBody, iSize);
		pPage->cachedSize = iSize;
		pPage->cacheExpire = pPage->cacheSeconds > 0 ? (tNow + pPage->cacheSeconds) : 0;
	}
	if ( psBody ) *psBody = sBody;
	else if ( sBody ) xrtFree(sBody);
	if ( piSize ) *piSize = iSize;
	return TRUE;
}

static bool Standalone_UriBelongsToPage(RouteInfo* pInfo, int64 iPageId)
{
	StandalonePageItem* pPage = pInfo ? (StandalonePageItem*)pInfo->pPageRouteToken : NULL;
	return pPage && pPage->id == iPageId;
}

static bool Standalone_CheckUriConflict(const char* sUri, int64 iPageId, str* psError)
{
	RouteInfo* pInfo;
	if ( sUri == NULL || sUri[0] != '/' ) {
		if ( psError ) *psError = xrtCopyStr("uri must start with /", 0);
		return FALSE;
	}
	pInfo = (RouteInfo*)xrtDictGet(G_StaticRouteTableHTTP, (ptr)sUri, (uint32)strlen(sUri));
	if ( pInfo && !Standalone_UriBelongsToPage(pInfo, iPageId) ) {
		if ( psError ) *psError = xrtFormat("uri already exists: %s", sUri);
		return FALSE;
	}
	if ( FindDynamicRouteHTTP((str)sUri) != NULL ) {
		if ( psError ) *psError = xrtFormat("uri already exists: %s", sUri);
		return FALSE;
	}
	return TRUE;
}

typedef struct StandaloneCheckCtx {
	int64 id;
	str error;
} StandaloneCheckCtx;

static bool Standalone_CheckUriProc(const char* sUri, void* pCtx)
{
	StandaloneCheckCtx* pCheck = (StandaloneCheckCtx*)pCtx;
	return !Standalone_CheckUriConflict(sUri, pCheck->id, &pCheck->error);
}

static bool Standalone_RegisterUriProc(const char* sUri, void* pCtx)
{
	StandalonePageItem* pPage = (StandalonePageItem*)pCtx;
	RouteInfo* pInfo;
	AddStaticRouteHTTP((str)sUri, Request_StandalonePage);
	pInfo = (RouteInfo*)xrtDictGet(G_StaticRouteTableHTTP, (ptr)sUri, (uint32)strlen(sUri));
	if ( pInfo ) {
		pInfo->bAuth = FALSE;
		pInfo->bAdmin = FALSE;
		pInfo->pPageRouteToken = pPage;
	}
	return FALSE;
}

static bool Standalone_UnregisterUriProc(const char* sUri, void* pCtx)
{
	StandalonePageItem* pPage = (StandalonePageItem*)pCtx;
	RouteInfo* pInfo = (RouteInfo*)xrtDictGet(G_StaticRouteTableHTTP, (ptr)sUri, (uint32)strlen(sUri));
	if ( pInfo && pInfo->pPageRouteToken == pPage ) {
		xrtDictRemove(G_StaticRouteTableHTTP, (ptr)sUri, (uint32)strlen(sUri));
	}
	return FALSE;
}

static void Standalone_RegisterPageRoutes(StandalonePageItem* pPage)
{
	if ( Standalone_IsEnabled(pPage) ) {
		Standalone_ForEachUri((const char*)pPage->uris, Standalone_RegisterUriProc, pPage);
	}
}

static void Standalone_UnregisterPageRoutes(StandalonePageItem* pPage)
{
	if ( pPage && pPage->uris ) {
		Standalone_ForEachUri((const char*)pPage->uris, Standalone_UnregisterUriProc, pPage);
	}
}

static bool Standalone_EnsureUriRowProc(const char* sUri, void* pCtx)
{
	sqlite3_stmt* stmt = NULL;
	xtime tNow = xrtNow();
	(void)pCtx;
	if ( sqlite3_prepare_v3(G_DB, "UPDATE uris SET isPersistent = 1, namespace = 'page', isBackend = 0, updateTime = ? WHERE uri = ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, tNow);
		sqlite3_bind_text(stmt, 2, sUri, -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	if ( sqlite3_changes(G_DB) <= 0 && sqlite3_prepare_v3(G_DB, "INSERT INTO uris (authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime, isPersistent, namespace) VALUES (1, ?, 'Standalone page', 0, 0, 0, 0, 0, ?, ?, 1, 'page')", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sUri, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, tNow);
		sqlite3_bind_int64(stmt, 3, tNow);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	return FALSE;
}

static void Standalone_SyncUriRows(str sUris)
{
	Standalone_ForEachUri((const char*)sUris, Standalone_EnsureUriRowProc, NULL);
}

static bool Standalone_EnsureSchema()
{
	return sqlite3_exec(G_DB,
		"CREATE TABLE IF NOT EXISTS standalone_page ("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"title TEXT NOT NULL DEFAULT '',"
		"status TEXT NOT NULL DEFAULT 'draft',"
		"uris TEXT NOT NULL DEFAULT '',"
		"header TEXT NOT NULL DEFAULT '',"
		"useTemplate INTEGER NOT NULL DEFAULT 0,"
		"content TEXT NOT NULL DEFAULT '',"
		"cacheSeconds INTEGER NOT NULL DEFAULT 0,"
		"createTime INTEGER NOT NULL DEFAULT 0,"
		"updateTime INTEGER NOT NULL DEFAULT 0"
		")", NULL, NULL, NULL) == SQLITE_OK;
}

static StandalonePageItem* Standalone_LoadPageFromStmt(sqlite3_stmt* stmt)
{
	StandalonePageItem* pPage = xrtMalloc(sizeof(StandalonePageItem));
	if ( pPage == NULL ) return NULL;
	memset(pPage, 0, sizeof(StandalonePageItem));
	pPage->id = sqlite3_column_int64(stmt, 0);
	pPage->title = Standalone_CopyColumn(stmt, 1);
	pPage->status = Standalone_CopyColumn(stmt, 2);
	pPage->uris = Standalone_CopyColumn(stmt, 3);
	pPage->header = Standalone_CopyColumn(stmt, 4);
	pPage->useTemplate = sqlite3_column_int(stmt, 5) ? TRUE : FALSE;
	pPage->content = Standalone_CopyColumn(stmt, 6);
	pPage->cacheSeconds = sqlite3_column_int64(stmt, 7);
	return pPage;
}

static void Standalone_ClearAllPages()
{
	if ( G_StandalonePages ) {
		for ( uint32 i = 0; i < xrtListCount(G_StandalonePages); i++ ) {
			StandalonePageItem* pPage = (StandalonePageItem*)xrtListGetPtr(G_StandalonePages, i);
			Standalone_UnregisterPageRoutes(pPage);
			Standalone_FreePage(pPage);
			xrtListSetPtr(G_StandalonePages, i, NULL, NULL);
		}
		xrtListDestroy(G_StandalonePages);
		G_StandalonePages = NULL;
	}
}

bool StandalonePage_LoadAll()
{
	sqlite3_stmt* stmt = NULL;
	Standalone_ClearAllPages();
	G_StandalonePages = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	if ( G_StandalonePages == NULL ) return FALSE;
	if ( sqlite3_prepare_v3(G_DB, "SELECT id,title,status,uris,header,useTemplate,content,cacheSeconds FROM standalone_page WHERE status <> 'deleted' ORDER BY id ASC", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		StandalonePageItem* pPage = Standalone_LoadPageFromStmt(stmt);
		if ( pPage ) {
			xrtListSetPtr(G_StandalonePages, xrtListCount(G_StandalonePages), pPage, NULL);
			Standalone_RegisterPageRoutes(pPage);
			Standalone_SyncUriRows(pPage->uris);
		}
	}
	sqlite3_finalize(stmt);
	return TRUE;
}

bool StandalonePage_Init()
{
	printf("        StandalonePage_Init \n");
	if ( !Standalone_EnsureSchema() ) return FALSE;
	return StandalonePage_LoadAll();
}

void StandalonePage_Unit()
{
	printf("        StandalonePage_Unit \n");
	Standalone_ClearAllPages();
}

void Request_StandalonePage(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	const char* sPath = xsReqPath(objReq);
	RouteInfo* pInfo = sPath ? (RouteInfo*)xrtDictGet(G_StaticRouteTableHTTP, (ptr)sPath, (uint32)strlen(sPath)) : NULL;
	StandalonePageItem* pPage = pInfo ? (StandalonePageItem*)pInfo->pPageRouteToken : NULL;
	str sBody = NULL;
	str sError = NULL;
	size_t iSize = 0;
	bool bDebug = (xsServerDebug(objServer) || xsHostDebug(objHost) || xsHostDevMode(objHost)) ? TRUE : FALSE;
	(void)objSession;
	if ( xsReqMethodID(objReq) != XHTTPD_METHOD_GET || pPage == NULL ) {
		xsHttpReplyAuto(objResp, 404, HTTP_CT_HTML, "<h1>404</h1>", 0);
		return;
	}
	if ( !Standalone_RenderPage(pPage, objServer, objHost, &sBody, &iSize, &sError) ) {
		if ( bDebug ) {
			str sMsg = xrtFormat("standalone page render failed\n\n%s", Standalone_CStr(sError));
			xsHttpReplyAuto(objResp, 500, HTTP_CT_TEXT, sMsg, 0);
			if ( sMsg ) xrtFree(sMsg);
		} else {
			xsHttpReplyAuto(objResp, 500, HTTP_CT_TEXT, "Internal Server Error", 0);
		}
		if ( sError ) xrtFree(sError);
		return;
	}
	xsHttpReplyAuto(objResp, 200, pPage->header && pPage->header[0] ? pPage->header : HTTP_CT_HTML, sBody, iSize);
	if ( sBody ) xrtFree(sBody);
}

#endif
