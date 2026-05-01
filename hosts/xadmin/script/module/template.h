// template runtime env
xvalue tblENV = NULL;
xteengine G_TemplateEngine = NULL;
static char* Form_RenderTemplateBlockHTML(xvalue tblSpec, str* psError);
static char* Template_RenderCompiledTemplate(xtetemplate hTemplate, xvalue tblData, size_t* pRetSize, str* psError);

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

typedef struct TemplateFormStmtData
{
	str sHTML;
} TemplateFormStmtData;

static const char* Template_CStrOr(str sText, const char* sDefault)
{
	return sText ? (const char*)sText : sDefault;
}

static int TemplateStmt_Form_Parse(XTE_StmtParseCtx* pCtx, void** ppData)
{
	str sJSON = NULL;
	xvalue tblSpec = NULL;
	str sError = NULL;
	char* sHTML = NULL;
	TemplateFormStmtData* pData = NULL;
	str sPreview = NULL;

	if ( (pCtx == NULL) || (ppData == NULL) ) {
		return xteStmtParseSetError(pCtx, -1, "invalid form stmt context");
	}

	if ( (pCtx->sRawBody == NULL) || (pCtx->iRawBodySize == 0) ) {
		return xteStmtParseSetError(pCtx, -1, "form block json required");
	}

	sJSON = xrtMalloc((uint32)pCtx->iRawBodySize + 1);
	if ( sJSON == NULL ) {
		return xteStmtParseSetError(pCtx, -1, "form block alloc failed");
	}
	memcpy(sJSON, pCtx->sRawBody, pCtx->iRawBodySize);
	sJSON[pCtx->iRawBodySize] = '\0';
	sPreview = xrtCopyStr(sJSON, 160);
	printf("[template][form] parse begin: json=%s\n", Template_CStrOr(sPreview, "(null)"));
	fflush(stdout);
	if ( sPreview ) {
		xrtFree(sPreview);
		sPreview = NULL;
	}

	tblSpec = xrtParseJSON(sJSON, pCtx->iRawBodySize);
	xrtFree(sJSON);
	if ( tblSpec == NULL ) {
		printf("[template][form] json parse failed\n");
		fflush(stdout);
		return xteStmtParseSetError(pCtx, -1, "form block json parse failed");
	}

	sHTML = Form_RenderTemplateBlockHTML(tblSpec, &sError);
	xvoUnref(tblSpec);
	if ( sHTML == NULL ) {
		printf("[template][form] html render failed: error=%s\n",
			Template_CStrOr(sError, "(null)"));
		fflush(stdout);
		const char* sDesc = sError ? (const char*)sError : "form block render failed";
		int iRet = xteStmtParseSetError(pCtx, -1, sDesc);
		if ( sError ) {
			xrtFree(sError);
		}
		return iRet;
	}
	if ( sError ) {
		xrtFree(sError);
	}
	printf("[template][form] parse success: html=%u bytes\n", (uint32)strlen(sHTML));
	fflush(stdout);

	pData = xrtMalloc(sizeof(TemplateFormStmtData));
	if ( pData == NULL ) {
		xrtFree(sHTML);
		return xteStmtParseSetError(pCtx, -1, "form block data alloc failed");
	}
	memset(pData, 0, sizeof(*pData));
	pData->sHTML = sHTML;
	*ppData = pData;
	return 1;
}

static XTE_Flow TemplateStmt_Form_Render(XTE_StmtRenderCtx* pCtx)
{
	TemplateFormStmtData* pData = (TemplateFormStmtData*)pCtx->pData;
	if ( (pData == NULL) || (pData->sHTML == NULL) ) {
		printf("[template][form] render missing cached html\n");
		fflush(stdout);
		return xteStmtSetError(pCtx, -1, "form block cached html missing");
	}
	if ( !xteStmtWrite(pCtx, pData->sHTML, strlen(pData->sHTML)) ) {
		printf("[template][form] render write failed\n");
		fflush(stdout);
		return xteStmtSetError(pCtx, -1, "form block write failed");
	}
	printf("[template][form] render success: html=%u bytes\n", (uint32)strlen(pData->sHTML));
	fflush(stdout);
	return XTE_FLOW_OK;
}

static void TemplateStmt_Form_FreeData(void* pData)
{
	TemplateFormStmtData* pStmtData = (TemplateFormStmtData*)pData;
	if ( pStmtData == NULL ) {
		return;
	}
	if ( pStmtData->sHTML ) {
		xrtFree(pStmtData->sHTML);
		pStmtData->sHTML = NULL;
	}
	xrtFree(pStmtData);
}

static bool Template_RegisterCustomStatements(xteengine hEngine)
{
	static XTE_StatementDef s_FormStatement = {
		"form",
		XTE_STMT_BLOCK | XTE_STMT_RAW_BODY,
		0,
		0,
		NULL,
		TemplateStmt_Form_Parse,
		TemplateStmt_Form_Render,
		TemplateStmt_Form_FreeData
	};

	if ( hEngine == NULL ) {
		return FALSE;
	}

	return xteRegisterStatement(hEngine, &s_FormStatement) >= 0;
}

static int Template_BufferWriterProc(void* pUserData, const char* sText, size_t iSize)
{
	xbuffer pBuf = (xbuffer)pUserData;
	if ( (pBuf == NULL) || (sText == NULL) ) {
		return 0;
	}
	return xrtBufferAppend(pBuf, (ptr)sText, (uint32)iSize, XBUF_BINARY) ? 1 : 0;
}

static char* Template_RenderCompiledTemplateWithIncludeMap(xtetemplate hTemplate, xdict hIncludeMap, xvalue tblData, size_t* pRetSize, str* psError)
{
	XTE_RenderOptions tOptions = { 0 };
	XTE_Writer tWriter = { 0 };
	XTE_Error tError = { 0 };
	xbuffer_struct tBuf = { 0 };
	char chZero = 0;
	char* sOutput = NULL;

	if ( psError ) {
		*psError = NULL;
	}

	xrtBufferInit(&tBuf, 0);
	tWriter.procWrite = Template_BufferWriterProc;
	tWriter.pUserData = &tBuf;
	tOptions.pCurrent = tblData;
	tOptions.pRoot = tblData;
	tOptions.pGlobal = tblENV;
	tOptions.pIncludeMap = hIncludeMap ? hIncludeMap : G_Template;
	tOptions.pWriter = &tWriter;

	if ( !xteRenderEx(hTemplate, &tOptions, &tError) ) {
		if ( psError != NULL ) {
			*psError = xrtFormat(
				"xte render failed: code=%d desc=%s line=%u col=%u",
				tError.iCode,
				tError.sDesc ? tError.sDesc : "unknown",
				tError.iLine,
				tError.iColumn
			);
		}
		xrtBufferUnit(&tBuf);
		if ( pRetSize ) {
			*pRetSize = 0u;
		}
		return NULL;
	}

	if ( !xrtBufferAppend(&tBuf, &chZero, 1, XBUF_BINARY) ) {
		if ( psError != NULL ) {
			*psError = xrtCopyStr("template output buffer append failed", 0);
		}
		xrtBufferUnit(&tBuf);
		if ( pRetSize ) {
			*pRetSize = 0u;
		}
		return NULL;
	}

	if ( pRetSize ) {
		*pRetSize = tBuf.Length - 1u;
	}
	sOutput = (char*)tBuf.Buffer;
	return sOutput;
}



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
	hTemplate = xteParseEx(G_TemplateEngine, sText, iSize, &G_TemplateParseOptions, pError);
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
	if ( strcmp((const char*)sKey, "form/block_demo.html") == 0 ) {
		printf("[template] cache loaded key=form/block_demo.html\n");
		fflush(stdout);
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

	xrtDictWalk(hDict, (Dict_EachProc)Template_DestroyCacheItemProc, NULL);
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
	if ( strcmp((const char*)sKey, "form/block_demo.html") == 0 ) {
		printf("[template] lookup key=form/block_demo.html\n");
		fflush(stdout);
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
	str sError = NULL;

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

	sPage = Template_RenderCompiledTemplate(hTemplate, tblData, pRetSize, &sError);
	if ( G_TemplateLock ) {
		xrtMutexUnlock(G_TemplateLock);
	}
	xrtFree(sTemplateKey);
	if ( sPage == NULL ) {
		printf("[template] render failed: %s (%s)\n", sTemplate, Template_CStrOr(sError, "unknown"));
		fflush(stdout);
		sPage = xrtFormat(
			"<!DOCTYPE html><html><body><p>template render failed: %s</p><pre>%s</pre></body></html>",
			sTemplate,
			Template_CStrOr(sError, "unknown")
		);
		if ( sError ) {
			xrtFree(sError);
		}
		return sPage;
	}

	return sPage;
}

char* MakeTextWithTemplate(char* sTemplate, xvalue tblData, size_t* pRetSize)
{
	xtetemplate hTemplate;
	str sTemplateKey = NULL;
	char* sOutput;
	str sError = NULL;

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

	sOutput = Template_RenderCompiledTemplate(hTemplate, tblData, pRetSize, &sError);
	if ( G_TemplateLock ) {
		xrtMutexUnlock(G_TemplateLock);
	}
	xrtFree(sTemplateKey);
	if ( sOutput == NULL ) {
		sOutput = xrtFormat("template render failed: %s (%s)", sTemplate, Template_CStrOr(sError, "unknown"));
		if ( sError ) {
			xrtFree(sError);
		}
		return sOutput;
	}

	return sOutput;
}

static char* Template_RenderCompiledTemplate(xtetemplate hTemplate, xvalue tblData, size_t* pRetSize, str* psError)
{
	return Template_RenderCompiledTemplateWithIncludeMap(hTemplate, G_Template, tblData, pRetSize, psError);
}



void Template_Init()
{
	TemplateCacheLoadContext tCtx;

	printf("        Template_Init \n");

	G_TemplateLock = xrtMutexCreate();
	G_TemplateEngine = xteCreateEngine();
	if ( G_TemplateEngine ) {
		xteRegisterBuiltinStatements(G_TemplateEngine);
		if ( !Template_RegisterCustomStatements(G_TemplateEngine) ) {
			printf("        Template custom statements register failed\n");
		}
	}
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

	if ( G_TemplateEngine ) {
		printf("        Template_Unit: destroy engine begin\n");
		fflush(stdout);
		xteDestroyEngine(G_TemplateEngine);
		G_TemplateEngine = NULL;
		printf("        Template_Unit: destroy engine done\n");
		fflush(stdout);
	}

	printf("        Template_Unit: done\n");
	fflush(stdout);
}
