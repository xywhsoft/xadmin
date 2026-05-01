#ifndef XADMIN_PLUGIN_SYSTEM_RESOURCE_H
#define XADMIN_PLUGIN_SYSTEM_RESOURCE_H

#include "ps_types.h"

#define PS_PLUGIN_STATIC_PREFIX "/plugin-static/"
#define PS_PLUGIN_STATIC_MAX_FILE_SIZE (16u * 1024u * 1024u)
#define PS_PLUGIN_STATIC_VERSIONED_CACHE_SECONDS (31536000u)

bool PS_ResourceIsSafeRelativePath(const char* sRelPath)
{
	if ( (sRelPath == NULL) || (sRelPath[0] == '\0') ) {
		return FALSE;
	}
	if ( (sRelPath[0] == '/') || (sRelPath[0] == '\\') ) {
		return FALSE;
	}
	if ( strchr(sRelPath, ':') != NULL ) {
		return FALSE;
	}
	if ( strstr(sRelPath, "..") != NULL ) {
		return FALSE;
	}
	if ( strchr(sRelPath, '%') != NULL ) {
		return FALSE;
	}
	for ( const char* p = sRelPath; *p; p++ ) {
		if ( (*p == '\\') || (((unsigned char)*p) < 32) ) {
			return FALSE;
		}
	}
	return TRUE;
}

PluginSystemPackage* PS_ResourceFindPackage(str sXid)
{
	if ( (G_PluginSystem == NULL) || (G_PluginSystem->lstPackages == NULL) || (sXid == NULL) || (sXid[0] == '\0') ) {
		return NULL;
	}

	for ( int i = 0; i < xrtListCount(G_PluginSystem->lstPackages); i++ ) {
		PluginSystemPackage* pPackage = xrtListGetPtr(G_PluginSystem->lstPackages, i);
		if ( pPackage == NULL ) {
			continue;
		}
		if ( (pPackage->sXid && strcmp(pPackage->sXid, sXid) == 0) || (pPackage->sName && strcmp(pPackage->sName, sXid) == 0) ) {
			return pPackage;
		}
	}
	return NULL;
}

int PS_ResourceAsciiLower(int ch)
{
	if ( (ch >= 'A') && (ch <= 'Z') ) {
		return ch + ('a' - 'A');
	}
	return ch;
}

bool PS_ResourceExtEquals(const char* sLeft, const char* sRight)
{
	if ( (sLeft == NULL) || (sRight == NULL) ) {
		return FALSE;
	}
	while ( *sLeft && *sRight ) {
		if ( PS_ResourceAsciiLower((unsigned char)*sLeft) != PS_ResourceAsciiLower((unsigned char)*sRight) ) {
			return FALSE;
		}
		sLeft++;
		sRight++;
	}
	return (*sLeft == '\0') && (*sRight == '\0');
}

str PS_ResourceBuildPath(PluginSystemPackage* pPackage, const char* sBaseDir, const char* sRelPath)
{
	if ( (pPackage == NULL) || (pPackage->sRootPath == NULL) || (sBaseDir == NULL) || !PS_ResourceIsSafeRelativePath(sBaseDir) || !PS_ResourceIsSafeRelativePath(sRelPath) ) {
		return NULL;
	}
	return xrtPathJoin(3, pPackage->sRootPath, (str)sBaseDir, (str)sRelPath);
}

const char* PS_ResourceMimeByPath(const char* sPath)
{
	const char* sExt;

	if ( sPath == NULL ) {
		return NULL;
	}

	sExt = strrchr(sPath, '.');
	if ( sExt == NULL ) {
		return NULL;
	}

	if ( PS_ResourceExtEquals(sExt, ".css") ) return "text/css; charset=utf-8";
	if ( PS_ResourceExtEquals(sExt, ".html") || PS_ResourceExtEquals(sExt, ".htm") ) return "text/html; charset=utf-8";
	if ( PS_ResourceExtEquals(sExt, ".js") ) return "application/javascript; charset=utf-8";
	if ( PS_ResourceExtEquals(sExt, ".mjs") ) return "application/javascript; charset=utf-8";
	if ( PS_ResourceExtEquals(sExt, ".svg") ) return "image/svg+xml";
	if ( PS_ResourceExtEquals(sExt, ".png") ) return "image/png";
	if ( PS_ResourceExtEquals(sExt, ".jpg") || PS_ResourceExtEquals(sExt, ".jpeg") ) return "image/jpeg";
	if ( PS_ResourceExtEquals(sExt, ".gif") ) return "image/gif";
	if ( PS_ResourceExtEquals(sExt, ".webp") ) return "image/webp";
	if ( PS_ResourceExtEquals(sExt, ".ico") ) return "image/x-icon";
	if ( PS_ResourceExtEquals(sExt, ".woff") ) return "font/woff";
	if ( PS_ResourceExtEquals(sExt, ".woff2") ) return "font/woff2";
	if ( PS_ResourceExtEquals(sExt, ".ttf") ) return "font/ttf";
	if ( PS_ResourceExtEquals(sExt, ".eot") ) return "application/vnd.ms-fontobject";
	if ( PS_ResourceExtEquals(sExt, ".map") ) return "application/json; charset=utf-8";
	return NULL;
}

bool PS_ResourceIsSourceMapPath(const char* sPath)
{
	const char* sExt;

	if ( sPath == NULL ) {
		return FALSE;
	}
	sExt = strrchr(sPath, '.');
	return PS_ResourceExtEquals(sExt, ".map");
}

bool PS_ResourceStaticAllowSourceMap(PluginSystemPackage* pPackage)
{
	xvalue tblResources;

	if ( (pPackage == NULL) || (pPackage->tblManifest == NULL) ) {
		return FALSE;
	}
	tblResources = xvoTableGetValue(pPackage->tblManifest, "resources", 9);
	if ( (tblResources == NULL) || (xvoType(tblResources) != XVO_DT_TABLE) ) {
		return FALSE;
	}
	return xvoTableGetBool(tblResources, "allowSourceMap", 14) || xvoTableGetBool(tblResources, "staticAllowSourceMap", 20);
}

bool PS_ResourceLooksVersioned(const char* sRelPath)
{
	const char* sDot;
	const char* sExt;
	int iHex = 0;

	if ( sRelPath == NULL ) {
		return FALSE;
	}
	sExt = strrchr(sRelPath, '.');
	if ( sExt == NULL ) {
		return FALSE;
	}
	sDot = sExt;
	while ( (sDot > sRelPath) && (*(sDot - 1) != '/') ) {
		sDot--;
	}
	for ( const char* p = sDot; p < sExt; p++ ) {
		if ( (*p == '.') || (*p == '-') || (*p == '_') ) {
			iHex = 0;
			continue;
		}
		if ( ((*p >= '0') && (*p <= '9')) || ((*p >= 'a') && (*p <= 'f')) || ((*p >= 'A') && (*p <= 'F')) ) {
			iHex++;
			if ( iHex >= 8 ) {
				return TRUE;
			}
		} else {
			iHex = 0;
		}
	}
	return FALSE;
}

str PS_ResourceBuildStaticHeader(const char* sContentType, str sPath, const char* sRelPath)
{
	size_t iSize;
	int64 iModified;
	str sModifiedText;
	str sHeader;
	const char* sCacheControl;

	iSize = xrtFileGetSize(sPath);
	iModified = xrtFileGetChangeTime(sPath);
	sModifiedText = xrtTimeToStr(iModified, XRT_TIME_FORMAT_DATETIME);
	sCacheControl = PS_ResourceLooksVersioned(sRelPath)
		? "public, max-age=31536000, immutable"
		: "no-cache";

	sHeader = xrtFormat(
		"Content-Type: %s\r\nCache-Control: %s\r\nETag: \"p-%lld-%llu\"\r\nLast-Modified: %s\r\n",
		sContentType ? sContentType : "application/octet-stream",
		sCacheControl,
		(long long)iModified,
		(unsigned long long)iSize,
		sModifiedText ? (const char*)sModifiedText : "1970-01-01 00:00:00"
	);
	if ( sModifiedText ) {
		xrtFree(sModifiedText);
	}
	return sHeader;
}

bool PS_ResourceReplyFileWithHeader(XS_ResponseObject objResp, int iCode, const char* sHeader, str sPath)
{
	size_t iSize = 0;
	str sData;

	if ( (objResp == NULL) || (sPath == NULL) || !xrtFileExists(sPath) ) {
		return FALSE;
	}

	sData = xrtFileGetAll(sPath, &iSize);
	if ( sData == NULL ) {
		return FALSE;
	}
	if ( iSize > PS_PLUGIN_STATIC_MAX_FILE_SIZE ) {
		xrtFree(sData);
		return FALSE;
	}

	if ( sHeader == NULL ) {
		xrtFree(sData);
		return FALSE;
	}

	xsHttpReplyAuto(objResp, iCode, sHeader, sData, iSize);
	xrtFree(sData);
	return TRUE;
}

bool PS_ResourceReplyFile(XS_ResponseObject objResp, int iCode, const char* sContentType, str sPath)
{
	str sHeader = PS_ResourceBuildStaticHeader(sContentType, sPath, NULL);
	bool bOK = PS_ResourceReplyFileWithHeader(objResp, iCode, sHeader, sPath);
	if ( sHeader ) {
		xrtFree(sHeader);
	}
	return bOK;
}

bool PS_ResourceLoadPluginPage(PluginSystemGeneration* pGeneration, XS_ResponseObject objResp, int iCode, const char* sHeader, const char* sPage)
{
	str sPath;
	bool bOK;

	if ( (pGeneration == NULL) || (pGeneration->pPackage == NULL) || (objResp == NULL) || (sPage == NULL) ) {
		return FALSE;
	}

	sPath = PS_ResourceBuildPath(pGeneration->pPackage, pGeneration->pPackage->sPageDir ? (const char*)pGeneration->pPackage->sPageDir : "page", sPage);
	if ( sPath == NULL ) {
		return FALSE;
	}

	bOK = PS_ResourceReplyFile(objResp, iCode, sHeader ? sHeader : (const char*)HTTP_CT_HTML, sPath);
	xrtFree(sPath);
	if ( !bOK ) {
		xsHttpReplyAuto(objResp, 404, HTTP_CT_HTML, "<h1>404</h1>", 0);
	}
	return TRUE;
}

typedef struct PS_PluginTemplateCacheContext
{
	str sBasePath;
	xdict hCache;
	xlist lstOwnedTemplates;
	uint32 iLoaded;
	uint32 iFailed;
} PS_PluginTemplateCacheContext;

static str PS_PluginTemplateNormalizeKey(str sBasePath, str sPath)
{
	str sRel;
	str sKey;
	size_t iBaseLen;

	if ( (sBasePath == NULL) || (sPath == NULL) ) {
		return NULL;
	}
	iBaseLen = strlen(sBasePath);
	if ( xrtStrComp(sPath, sBasePath, (uint32)iBaseLen, FALSE) == 0 ) {
		sRel = sPath + iBaseLen;
		while ( (*sRel == '\\') || (*sRel == '/') ) {
			sRel++;
		}
	} else {
		sRel = sPath;
	}
	sKey = xrtCopyStr(sRel, 0);
	if ( sKey == NULL ) {
		return NULL;
	}
	for ( size_t i = 0; sKey[i] != '\0'; i++ ) {
		if ( sKey[i] == '\\' ) {
			sKey[i] = '/';
		}
	}
	return sKey;
}

static bool PS_PluginTemplateCopyGlobalProc(Dict_Key* pKey, xtetemplate* phTemplate, ptr pArg)
{
	xdict hDst = (xdict)pArg;
	if ( (pKey == NULL) || (phTemplate == NULL) || (*phTemplate == NULL) || (hDst == NULL) ) {
		return FALSE;
	}
	xrtDictSetPtr(hDst, pKey->Key, pKey->KeyLen, *phTemplate, NULL);
	return FALSE;
}

static int PS_PluginTemplateLoadProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
{
	PS_PluginTemplateCacheContext* pCtx = (PS_PluginTemplateCacheContext*)Param;
	str sKey;
	XTE_Error tError = { 0 };
	xtetemplate hTemplate;
	xtetemplate hOldTemplate = NULL;

	(void)iSize;
	(void)pData;

	if ( (pCtx == NULL) || (pCtx->hCache == NULL) || (sPath == NULL) || (bDir != 0) ) {
		return FALSE;
	}

	sKey = PS_PluginTemplateNormalizeKey(pCtx->sBasePath, sPath);
	if ( (sKey == NULL) || (sKey[0] == '\0') ) {
		if ( sKey ) xrtFree(sKey);
		return FALSE;
	}

	hTemplate = Template_ParseTemplateFile(sPath, sKey, &tError);
	if ( hTemplate == NULL ) {
		pCtx->iFailed++;
		xrtFree(sKey);
		return FALSE;
	}

	if ( !xrtDictSetPtr(pCtx->hCache, sKey, (uint32)strlen(sKey), hTemplate, (ptr*)&hOldTemplate) ) {
		xteDestroyTemplate(hTemplate);
		pCtx->iFailed++;
		xrtFree(sKey);
		return FALSE;
	}
	if ( hOldTemplate != NULL ) {
		/* Old value may be a global template pointer. Do not destroy it here. */
	}
	if ( pCtx->lstOwnedTemplates ) {
		xrtListSetPtr(pCtx->lstOwnedTemplates, xrtListCount(pCtx->lstOwnedTemplates), hTemplate, NULL);
	}
	pCtx->iLoaded++;
	xrtFree(sKey);
	return FALSE;
}

static void PS_PluginTemplateDestroyOwnedList(xlist lstOwnedTemplates)
{
	if ( lstOwnedTemplates == NULL ) {
		return;
	}
	for ( int i = 0; i < xrtListCount(lstOwnedTemplates); i++ ) {
		xtetemplate hTemplate = (xtetemplate)xrtListGetPtr(lstOwnedTemplates, i);
		if ( hTemplate != NULL ) {
			xteDestroyTemplate(hTemplate);
		}
	}
	xrtListDestroy(lstOwnedTemplates);
}

static xdict PS_PluginTemplateBuildIncludeMap(PluginSystemPackage* pPackage, str sTemplateBasePath, xlist* plstOwnedTemplates)
{
	PS_PluginTemplateCacheContext tCtx;
	xdict hCache;

	if ( plstOwnedTemplates ) {
		*plstOwnedTemplates = NULL;
	}
	if ( (pPackage == NULL) || (sTemplateBasePath == NULL) ) {
		return NULL;
	}

	hCache = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	if ( hCache == NULL ) {
		return NULL;
	}
	xrtOwnerActivateShared(&hCache->Owner);
	xrtOwnerActivateShared(&hCache->AVLT.Owner);

	if ( G_Template != NULL ) {
		xrtDictWalk(G_Template, (Dict_EachProc)PS_PluginTemplateCopyGlobalProc, hCache);
	}

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.sBasePath = sTemplateBasePath;
	tCtx.hCache = hCache;
	tCtx.lstOwnedTemplates = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	if ( plstOwnedTemplates ) {
		*plstOwnedTemplates = tCtx.lstOwnedTemplates;
	}
	if ( xrtDirExists(sTemplateBasePath) ) {
		xrtDirScan(sTemplateBasePath, TRUE, PS_PluginTemplateLoadProc, &tCtx);
	}

	return hCache;
}

str PS_PluginRenderTemplateFile(PluginSystemPackage* pPackage, const char* sTemplate, xvalue tblData, size_t* pRetSize, str* psError)
{
	str sPath;
	str sTemplateBasePath = NULL;
	xdict hIncludeMap = NULL;
	xlist lstOwnedTemplates = NULL;
	xtetemplate hTemplate = NULL;
	XTE_Error tError = { 0 };
	str sOutput = NULL;
	str sRenderError = NULL;

	if ( psError ) {
		*psError = NULL;
	}
	if ( pRetSize ) {
		*pRetSize = 0;
	}
	if ( (pPackage == NULL) || (sTemplate == NULL) || (sTemplate[0] == '\0') ) {
		if ( psError ) *psError = xrtCopyStr("template name required", 0);
		return NULL;
	}

	sTemplateBasePath = xrtPathJoin(2, pPackage->sRootPath, pPackage->sTemplateDir ? pPackage->sTemplateDir : (str)"template");
	hIncludeMap = PS_PluginTemplateBuildIncludeMap(pPackage, sTemplateBasePath, &lstOwnedTemplates);
	sPath = PS_ResourceBuildPath(pPackage, pPackage->sTemplateDir ? pPackage->sTemplateDir : (str)"template", sTemplate);
	if ( sPath && xrtFileExists(sPath) ) {
		if ( G_TemplateLock ) {
			xrtMutexLock(G_TemplateLock);
		}
		hTemplate = Template_ParseTemplateFile(sPath, (str)sTemplate, &tError);
		if ( hTemplate ) {
			sOutput = Template_RenderCompiledTemplateWithIncludeMap(hTemplate, hIncludeMap, tblData, pRetSize, &sRenderError);
			xteDestroyTemplate(hTemplate);
		}
		if ( G_TemplateLock ) {
			xrtMutexUnlock(G_TemplateLock);
		}
		xrtFree(sPath);
		if ( sOutput ) {
			if ( hIncludeMap ) xrtDictDestroy(hIncludeMap);
			PS_PluginTemplateDestroyOwnedList(lstOwnedTemplates);
			if ( sTemplateBasePath ) xrtFree(sTemplateBasePath);
			return sOutput;
		}
		if ( psError ) {
			*psError = sRenderError ? sRenderError : xrtFormat("plugin template parse failed: %s", tError.sDesc ? tError.sDesc : "unknown");
		} else if ( sRenderError ) {
			xrtFree(sRenderError);
		}
		if ( hIncludeMap ) xrtDictDestroy(hIncludeMap);
		PS_PluginTemplateDestroyOwnedList(lstOwnedTemplates);
		if ( sTemplateBasePath ) xrtFree(sTemplateBasePath);
		return NULL;
	}
	if ( sPath ) {
		xrtFree(sPath);
	}

	sOutput = MakeTextWithTemplate((char*)sTemplate, tblData, pRetSize);
	if ( sOutput == NULL && psError ) {
		*psError = xrtCopyStr("template render failed", 0);
	}
	if ( hIncludeMap ) xrtDictDestroy(hIncludeMap);
	PS_PluginTemplateDestroyOwnedList(lstOwnedTemplates);
	if ( sTemplateBasePath ) xrtFree(sTemplateBasePath);
	return sOutput;
}

xvalue PS_PluginOptionLoadFile(const char* sXid, const char* sFileName)
{
	PluginSystemPackage* pPackage;
	str sPath;
	str sDataDir;
	str sDataPath;
	xvalue tblConfig;

	if ( !PS_ResourceIsSafeRelativePath(sXid) || !PS_ResourceIsSafeRelativePath(sFileName) ) {
		return NULL;
	}

	pPackage = PS_ResourceFindPackage((str)sXid);
	if ( pPackage == NULL ) {
		return NULL;
	}
	if ( !pPackage->bEnabled || (pPackage->iStatus != PS_PACKAGE_STATUS_ACTIVE) ) {
		return NULL;
	}

	if ( pPackage->sDataPath ) {
		sDataDir = xrtPathJoin(2, pPackage->sDataPath, "option");
		sDataPath = sDataDir ? xrtPathJoin(2, sDataDir, (str)sFileName) : NULL;
		if ( sDataPath && xrtFileExists(sDataPath) ) {
			tblConfig = xrtParseJSON_File(sDataPath);
			xrtFree(sDataPath);
			if ( sDataDir ) {
				xrtFree(sDataDir);
			}
			return tblConfig;
		}
		if ( sDataPath ) {
			xrtFree(sDataPath);
		}
		if ( sDataDir ) {
			xrtFree(sDataDir);
		}
	}

	sPath = PS_ResourceBuildPath(pPackage, pPackage->sOptionDir ? pPackage->sOptionDir : (str)"option", sFileName);
	if ( sPath == NULL ) {
		return NULL;
	}

	tblConfig = xrtParseJSON_File(sPath);
	xrtFree(sPath);
	return tblConfig;
}

bool PS_PluginOptionSaveFile(const char* sXid, const char* sFileName, xvalue tblFormData)
{
	PluginSystemPackage* pPackage;
	str sPath;
	str sDataDir;
	str sDataPath;
	xvalue tblConfig;
	xvalue arrClassList;
	bool bRet;

	if ( (tblFormData == NULL) || !PS_ResourceIsSafeRelativePath(sXid) || !PS_ResourceIsSafeRelativePath(sFileName) ) {
		return FALSE;
	}

	pPackage = PS_ResourceFindPackage((str)sXid);
	if ( pPackage == NULL ) {
		return FALSE;
	}
	if ( !pPackage->bEnabled || (pPackage->iStatus != PS_PACKAGE_STATUS_ACTIVE) ) {
		return FALSE;
	}

	sDataDir = pPackage->sDataPath ? xrtPathJoin(2, pPackage->sDataPath, "option") : NULL;
	sDataPath = sDataDir ? xrtPathJoin(2, sDataDir, (str)sFileName) : NULL;
	if ( sDataPath && xrtFileExists(sDataPath) ) {
		sPath = xrtCopyStr(sDataPath, 0);
	} else {
		sPath = PS_ResourceBuildPath(pPackage, pPackage->sOptionDir ? pPackage->sOptionDir : (str)"option", sFileName);
	}
	if ( sPath == NULL ) {
		if ( sDataDir ) xrtFree(sDataDir);
		if ( sDataPath ) xrtFree(sDataPath);
		return FALSE;
	}

	tblConfig = xrtParseJSON_File(sPath);
	xrtFree(sPath);
	if ( tblConfig == NULL ) {
		if ( sDataDir ) xrtFree(sDataDir);
		if ( sDataPath ) xrtFree(sDataPath);
		return FALSE;
	}

	arrClassList = xvoTableGetValue(tblConfig, "classList", 9);
	if ( (arrClassList != NULL) && (xvoType(arrClassList) == XVO_DT_ARRAY) ) {
		uint32 iClassCount = xvoArrayItemCount(arrClassList);
		for ( uint32 i = 0; i < iClassCount; i++ ) {
			xvalue tblClass = xvoArrayGetValue(arrClassList, i);
			xvalue arrOptions;
			if ( (tblClass == NULL) || (xvoType(tblClass) != XVO_DT_TABLE) ) {
				continue;
			}
			arrOptions = xvoTableGetValue(tblClass, "options", 7);
			if ( (arrOptions == NULL) || (xvoType(arrOptions) != XVO_DT_ARRAY) ) {
				continue;
			}
			for ( uint32 j = 0; j < xvoArrayItemCount(arrOptions); j++ ) {
				xvalue tblOpt = xvoArrayGetValue(arrOptions, j);
				str sName;
				xvalue varNewValue;
				if ( (tblOpt == NULL) || (xvoType(tblOpt) != XVO_DT_TABLE) ) {
					continue;
				}
				sName = xvoTableGetText(tblOpt, "name", 4);
				if ( sName == NULL ) {
					continue;
				}
				varNewValue = xvoTableGetValue(tblFormData, sName, 0);
				if ( varNewValue != NULL ) {
					xvoAddRef(varNewValue);
					xvoTableSetValue(tblOpt, "value", 5, varNewValue, TRUE);
				}
			}
		}
	}

	if ( sDataDir ) {
		xrtDirCreateAll(sDataDir);
	}
	bRet = (sDataPath != NULL) ? xrtStringifyJSON_File(sDataPath, tblConfig, TRUE) : FALSE;
	xvoUnref(tblConfig);
	if ( sDataDir ) xrtFree(sDataDir);
	if ( sDataPath ) xrtFree(sDataPath);
	return bRet;
}

bool PS_TryServePluginStatic(XS_RequestObject objReq, XS_ResponseObject objResp, const char* sPath)
{
	const char* sCursor;
	const char* sSlash;
	char sXid[128];
	size_t iXidLen;
	const char* sRelPath;
	PluginSystemPackage* pPackage;
	str sFilePath;
	const char* sMime;
	bool bOK;

	if ( (objReq == NULL) || (objResp == NULL) || (sPath == NULL) ) {
		return FALSE;
	}
	if ( strncmp(sPath, PS_PLUGIN_STATIC_PREFIX, strlen(PS_PLUGIN_STATIC_PREFIX)) != 0 ) {
		return FALSE;
	}
	if ( (xsReqMethodID(objReq) != XHTTPD_METHOD_GET) && (xsReqMethodID(objReq) != XHTTPD_METHOD_HEAD) ) {
		xsHttpReplyAuto(objResp, 404, HTTP_CT_HTML, "<h1>404</h1>", 0);
		return TRUE;
	}

	sCursor = sPath + strlen(PS_PLUGIN_STATIC_PREFIX);
	sSlash = strchr(sCursor, '/');
	if ( sSlash == NULL ) {
		xsHttpReplyAuto(objResp, 404, HTTP_CT_HTML, "<h1>404</h1>", 0);
		return TRUE;
	}

	iXidLen = (size_t)(sSlash - sCursor);
	if ( (iXidLen <= 0) || (iXidLen >= sizeof(sXid)) ) {
		xsHttpReplyAuto(objResp, 404, HTTP_CT_HTML, "<h1>404</h1>", 0);
		return TRUE;
	}

	memcpy(sXid, sCursor, iXidLen);
	sXid[iXidLen] = '\0';
	sRelPath = sSlash + 1;
	if ( !PS_ResourceIsSafeRelativePath(sXid) || !PS_ResourceIsSafeRelativePath(sRelPath) ) {
		xsHttpReplyAuto(objResp, 404, HTTP_CT_HTML, "<h1>404</h1>", 0);
		return TRUE;
	}

	pPackage = PS_ResourceFindPackage((str)sXid);
	if ( (pPackage == NULL) || !pPackage->bEnabled || (pPackage->iStatus != PS_PACKAGE_STATUS_ACTIVE) ) {
		xsHttpReplyAuto(objResp, 404, HTTP_CT_HTML, "<h1>404</h1>", 0);
		return TRUE;
	}

	sFilePath = PS_ResourceBuildPath(pPackage, pPackage->sStaticDir ? pPackage->sStaticDir : (str)"static", sRelPath);
	if ( sFilePath == NULL ) {
		xsHttpReplyAuto(objResp, 404, HTTP_CT_HTML, "<h1>404</h1>", 0);
		return TRUE;
	}

	sMime = PS_ResourceMimeByPath(sFilePath);
	if ( (sMime == NULL) || (PS_ResourceIsSourceMapPath(sFilePath) && !PS_ResourceStaticAllowSourceMap(pPackage)) ) {
		xrtFree(sFilePath);
		xsHttpReplyAuto(objResp, 404, HTTP_CT_HTML, "<h1>404</h1>", 0);
		return TRUE;
	}
	str sHeader = PS_ResourceBuildStaticHeader(sMime, sFilePath, sRelPath);
	if ( xsReqMethodID(objReq) == XHTTPD_METHOD_HEAD ) {
		xsHttpReplyAuto(objResp, 200, sHeader ? sHeader : HTTP_CT_TEXT, "", 0);
		if ( sHeader ) xrtFree(sHeader);
		xrtFree(sFilePath);
		return TRUE;
	}
	bOK = PS_ResourceReplyFileWithHeader(objResp, 200, sHeader, sFilePath);
	if ( sHeader ) xrtFree(sHeader);
	xrtFree(sFilePath);
	if ( !bOK ) {
		xsHttpReplyAuto(objResp, 404, HTTP_CT_HTML, "<h1>404</h1>", 0);
	}
	return TRUE;
}

#endif
