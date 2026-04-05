// template runtime env
xvalue tblENV = NULL;

// shared include table (kept for compatibility)
xdict G_Template = NULL;
xmutex G_TemplateLock = NULL;
typedef struct TemplateRetiredCacheNode
{
	xdict hCache;
	struct TemplateRetiredCacheNode* pNext;
} TemplateRetiredCacheNode;
TemplateRetiredCacheNode* G_TemplateRetired = NULL;

#define XADMIN_TEMPLATE_BRACKET "{{}}"
XTE_ParseOptions G_TemplateParseOptions = { XADMIN_TEMPLATE_BRACKET, 0 };

typedef struct TemplateCacheLoadContext
{
	uint32 iLoaded;
	uint32 iFailed;
	xdict hCache;
} TemplateCacheLoadContext;



static str Template_NormalizeTemplateKey(str sPath)
{
	str sKey;
	size_t iBaseLen;
	str sRel;
	size_t i;

	if ( sPath == NULL ) {
		return NULL;
	}

	iBaseLen = strlen(TemplatePath);
	sRel = sPath;
	if ( xrtStrComp(sPath, TemplatePath, (uint32)iBaseLen, FALSE) == 0 ) {
		sRel = sPath + iBaseLen;
		while ( (*sRel == '\\') || (*sRel == '/') ) {
			sRel++;
		}
	}

	sKey = xrtCopyStr(sRel, 0);
	if ( sKey == NULL ) {
		return NULL;
	}

	for ( i = 0; sKey[i] != '\0'; i++ ) {
		if ( sKey[i] == '\\' ) {
			sKey[i] = '/';
		}
	}

	return sKey;
}

static xtetemplate Template_ParseTemplateFile(str sFilePath, str sTemplateKey, XTE_Error* pError)
{
	str sText;
	size_t iSize;
	xtetemplate hTemplate;

	sText = xrtFileReadAll(sFilePath, XRT_CP_BINARY, NULL);
	if ( sText == NULL ) {
		if ( pError ) {
			memset(pError, 0, sizeof(*pError));
		}
		return NULL;
	}

	iSize = strlen(sText);
	hTemplate = xteParseEx(NULL, sText, iSize, &G_TemplateParseOptions, pError);
	xrtFree(sText);
	if ( hTemplate == NULL ) {
		printf("[template] parse failed: %s (%s at %u:%u)\n",
			sTemplateKey ? sTemplateKey : sFilePath,
			(pError && pError->sDesc) ? pError->sDesc : "unknown error",
			pError ? pError->iLine : 0,
			pError ? pError->iColumn : 0);
	}

	return hTemplate;
}

static int Template_LoadCacheProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
{
	TemplateCacheLoadContext* pCtx = (TemplateCacheLoadContext*)Param;
	str sKey;
	XTE_Error tError = { 0 };
	xtetemplate hTemplate;
	xtetemplate hOldTemplate = NULL;

	(void)iSize;
	(void)pData;

	if ( (sPath == NULL) || (bDir != 0) ) {
		return FALSE;
	}

	sKey = Template_NormalizeTemplateKey(sPath);
	if ( (sKey == NULL) || (sKey[0] == '\0') ) {
		xrtFree(sKey);
		return FALSE;
	}

	hTemplate = Template_ParseTemplateFile(sPath, sKey, &tError);
	if ( hTemplate == NULL ) {
		if ( pCtx ) {
			pCtx->iFailed++;
		}
		xrtFree(sKey);
		return FALSE;
	}

	if ( (pCtx == NULL) || (pCtx->hCache == NULL) ) {
		xteDestroyTemplate(hTemplate);
		xrtFree(sKey);
		return FALSE;
	}

	if ( !xrtDictSetPtr(pCtx->hCache, sKey, (uint32)strlen(sKey), hTemplate, (ptr*)&hOldTemplate) ) {
		printf("[template] cache insert failed: %s\n", sKey);
		xteDestroyTemplate(hTemplate);
		if ( pCtx ) {
			pCtx->iFailed++;
		}
		xrtFree(sKey);
		return FALSE;
	}

	if ( hOldTemplate != NULL ) {
		xteDestroyTemplate(hOldTemplate);
	}

	if ( pCtx ) {
		pCtx->iLoaded++;
	}

	xrtFree(sKey);
	return FALSE;
}

static bool Template_DestroyCacheItemProc(Dict_Key* pKey, xtetemplate* phTemplate, ptr pArg)
{
	(void)pKey;
	(void)pArg;

	if ( (phTemplate != NULL) && (*phTemplate != NULL) ) {
		xteDestroyTemplate(*phTemplate);
		*phTemplate = NULL;
	}

	return FALSE;
}

static xdict Template_CreateCacheDict()
{
	xdict hDict = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	if ( hDict != NULL ) {
		xrtOwnerActivateShared(&hDict->Owner);
		xrtOwnerActivateShared(&hDict->AVLT.Owner);
	}
	return hDict;
}

static void Template_ClearCacheDict(xdict hDict)
{
	if ( hDict == NULL ) {
		return;
	}

	xrtDictWalk(hDict, (Dict_EachProc)Template_DestroyCacheItemProc, NULL);
	xrtDictClear(hDict);
}

static void Template_DestroyCacheDict(xdict hDict)
{
	if ( hDict == NULL ) {
		return;
	}

	Template_ClearCacheDict(hDict);
	xrtDictDestroy(hDict);
}

static void Template_PushRetiredCache(xdict hDict)
{
	TemplateRetiredCacheNode* pNode;

	if ( hDict == NULL ) {
		return;
	}

	pNode = xrtMalloc(sizeof(TemplateRetiredCacheNode));
	if ( pNode == NULL ) {
		return;
	}

	pNode->hCache = hDict;
	pNode->pNext = G_TemplateRetired;
	G_TemplateRetired = pNode;
}

static void Template_DestroyRetiredCaches()
{
	TemplateRetiredCacheNode* pNode = G_TemplateRetired;
	while ( pNode != NULL ) {
		TemplateRetiredCacheNode* pNext = pNode->pNext;
		Template_DestroyCacheDict(pNode->hCache);
		xrtFree(pNode);
		pNode = pNext;
	}
	G_TemplateRetired = NULL;
}

static xtetemplate Template_GetCompiledTemplate(str sTemplate, str* psTemplateKey)
{
	str sKey;
	xtetemplate hTemplate;

	if ( (sTemplate == NULL) || (sTemplate[0] == '\0') ) {
		return NULL;
	}

	sKey = Template_NormalizeTemplateKey(sTemplate);
	if ( sKey == NULL ) {
		return NULL;
	}

	hTemplate = (xtetemplate)xrtDictGetPtr(G_Template, sKey, (uint32)strlen(sKey));
	if ( psTemplateKey != NULL ) {
		*psTemplateKey = sKey;
	} else {
		xrtFree(sKey);
	}

	return hTemplate;
}

static void Template_LoadCache(TemplateCacheLoadContext* pCtx, xdict hCache)
{
	if ( pCtx != NULL ) {
		pCtx->iLoaded = 0;
		pCtx->iFailed = 0;
		pCtx->hCache = hCache;
	}

	if ( xrtDirExists(TemplatePath) ) {
		xrtDirScan(TemplatePath, TRUE, Template_LoadCacheProc, pCtx);
	}
}

bool Template_RebuildCache(uint32* piLoaded, uint32* piFailed)
{
	TemplateCacheLoadContext tCtx;
	xdict hNewCache;
	xdict hOldCache;

	tCtx.iLoaded = 0;
	tCtx.iFailed = 0;
	tCtx.hCache = NULL;
	if ( G_Template == NULL ) {
		if ( piLoaded ) *piLoaded = 0;
		if ( piFailed ) *piFailed = 0;
		return FALSE;
	}

	hNewCache = Template_CreateCacheDict();
	if ( hNewCache == NULL ) {
		if ( piLoaded ) *piLoaded = 0;
		if ( piFailed ) *piFailed = 0;
		return FALSE;
	}

	Template_LoadCache(&tCtx, hNewCache);
	if ( (tCtx.iLoaded == 0) && (tCtx.iFailed > 0) ) {
		Template_DestroyCacheDict(hNewCache);
		if ( piLoaded ) *piLoaded = 0;
		if ( piFailed ) *piFailed = tCtx.iFailed;
		return FALSE;
	}

	hOldCache = NULL;
	if ( G_TemplateLock ) {
		xrtMutexLock(G_TemplateLock);
	}

	hOldCache = G_Template;
	G_Template = hNewCache;
	Template_PushRetiredCache(hOldCache);

	if ( G_TemplateLock ) {
		xrtMutexUnlock(G_TemplateLock);
	}

	if ( piLoaded ) *piLoaded = tCtx.iLoaded;
	if ( piFailed ) *piFailed = tCtx.iFailed;
	return TRUE;
}



xvalue TemplateProc_Project_MakeXID(xvalue varENV, xvalue varParam)
{
	(void)varENV;
	(void)varParam;

	return xvoCreateText(xrtMakeXIDS(), 0, TRUE);
}



char* MakePageWithTemplate(char* sTemplate, xvalue tblData, size_t* pRetSize)
{
	xtetemplate hTemplate;
	str sTemplateKey = NULL;
	char* sPage;

	if ( (sTemplate == NULL) || (sTemplate[0] == '\0') ) {
		return xrtCopyStr("<!DOCTYPE html><html><body><p>template name required</p></body></html>", 0);
	}

	if ( G_TemplateLock ) {
		xrtMutexLock(G_TemplateLock);
	}
	hTemplate = Template_GetCompiledTemplate(sTemplate, &sTemplateKey);
	if ( hTemplate == NULL ) {
		char* sError = xrtFormat("<!DOCTYPE html><html><body><p>template not found in cache: %s</p></body></html>", sTemplateKey ? (char*)sTemplateKey : sTemplate);
		if ( G_TemplateLock ) {
			xrtMutexUnlock(G_TemplateLock);
		}
		xrtFree(sTemplateKey);
		return sError;
	}

	sPage = xteMake(hTemplate, tblData, tblENV, G_Template, pRetSize);
	if ( G_TemplateLock ) {
		xrtMutexUnlock(G_TemplateLock);
	}
	xrtFree(sTemplateKey);
	if ( sPage == NULL ) {
		return xrtFormat("<!DOCTYPE html><html><body><p>template render failed: %s</p></body></html>", sTemplate);
	}

	return sPage;
}

char* MakeTextWithTemplate(char* sTemplate, xvalue tblData, size_t* pRetSize)
{
	xtetemplate hTemplate;
	str sTemplateKey = NULL;
	char* sOutput;

	if ( (sTemplate == NULL) || (sTemplate[0] == '\0') ) {
		return xrtCopyStr("template name required", 0);
	}

	if ( G_TemplateLock ) {
		xrtMutexLock(G_TemplateLock);
	}
	hTemplate = Template_GetCompiledTemplate(sTemplate, &sTemplateKey);
	if ( hTemplate == NULL ) {
		char* sError = xrtFormat("template not found in cache: %s", sTemplateKey ? (char*)sTemplateKey : sTemplate);
		if ( G_TemplateLock ) {
			xrtMutexUnlock(G_TemplateLock);
		}
		xrtFree(sTemplateKey);
		return sError;
	}

	sOutput = xteMake(hTemplate, tblData, tblENV, G_Template, pRetSize);
	if ( G_TemplateLock ) {
		xrtMutexUnlock(G_TemplateLock);
	}
	xrtFree(sTemplateKey);
	if ( sOutput == NULL ) {
		return xrtFormat("template render failed: %s", sTemplate);
	}

	return sOutput;
}



void Template_Init()
{
	TemplateCacheLoadContext tCtx;

	printf("        Template_Init \n");

	G_TemplateLock = xrtMutexCreate();
	G_Template = Template_CreateCacheDict();
	tblENV = xvoCreateTable();
	xvoTableSetFunc(tblENV, "MakeXID", 7, TemplateProc_Project_MakeXID);
	xrtOwnerActivateShared(&tblENV->vTable->AVLT.Owner);
	xrtOwnerActivateShared(&tblENV->vTable->Owner);
	xvoSetShared_Inline(tblENV);

	Template_LoadCache(&tCtx, G_Template);
	printf("        Template cache ready: loaded=%u failed=%u\n", tCtx.iLoaded, tCtx.iFailed);
}



void Template_Unit()
{
	printf("        Template_Unit \n");
	fflush(stdout);

	if ( G_Template ) {
		printf("        Template_Unit: destroy active cache begin\n");
		fflush(stdout);
		Template_DestroyCacheDict(G_Template);
		G_Template = NULL;
		printf("        Template_Unit: destroy active cache done\n");
		fflush(stdout);
	}

	printf("        Template_Unit: destroy retired caches begin\n");
	fflush(stdout);
	Template_DestroyRetiredCaches();
	printf("        Template_Unit: destroy retired caches done\n");
	fflush(stdout);

	if ( G_TemplateLock ) {
		printf("        Template_Unit: destroy mutex begin\n");
		fflush(stdout);
		xrtMutexDestroy(G_TemplateLock);
		G_TemplateLock = NULL;
		printf("        Template_Unit: destroy mutex done\n");
		fflush(stdout);
	}

	if ( tblENV ) {
		printf("        Template_Unit: unref env begin\n");
		fflush(stdout);
		xvoUnref(tblENV);
		tblENV = NULL;
		printf("        Template_Unit: unref env done\n");
		fflush(stdout);
	}

	printf("        Template_Unit: done\n");
	fflush(stdout);
}
