#include "xs_plugin.h"
#include "value_util.h"
#include "util.h"
#include <sqlite3.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* 上传文件名净化：只取 basename，拒绝路径逃逸（v1 未净化，..\ 可逃出上传目录） */
static size_t FM_SanitizeFileName(const char* sName, size_t iNameLen, char* sOut, size_t iOutSize)
{
	size_t iStart = 0, i, n;
	if (!sName || iNameLen == 0 || !sOut || iOutSize == 0) return 0;
	for (i = 0; i < iNameLen; i++) {
		if (sName[i] == '/' || sName[i] == '\\') iStart = i + 1;
	}
	n = iNameLen - iStart;
	if (n == 0 || n >= iOutSize) return 0;
	memcpy(sOut, sName + iStart, n);
	sOut[n] = '\0';
	if (strcmp(sOut, ".") == 0 || strcmp(sOut, "..") == 0) return 0;
	return n;
}
/* 路径式文件信息（v3 原生为句柄/Stat 式，这里按路径包装） */
static int64 FM_FileSize(const char* sPath)
{
	xfileinfo info;
	if (!sPath || !xrtPathStat(sPath, false, &info)) return 0;
	return (int64)info.Size;
}
static xtime FM_FileMTime(const char* sPath)
{
	xfileinfo info;
	if (!sPath || !xrtPathStat(sPath, false, &info)) return 0;
	return info.Modified;
}

typedef struct {
	char sRootPath[512];
	char sToolPath[512];
	char sTempPath[512];
	char sThumbPath[512];
} FMConfigState;

static XAdminPluginHandle G_FMHandle = NULL;
static sqlite3* G_FMMainDb = NULL;
static const char* G_FMXid = NULL;
static const char* G_FMRootPath = NULL;
static const char* G_FMDataPath = NULL;
static const char* G_FMPrivateDbPath = NULL;
static FMConfigState G_FMConfig;

static void FM_NormalizePath(char* path);

static bool FM_IsFileSystemMode(void)
{
	return (strcmp(G_FMConfig.sRootPath, "*") == 0);
}

static bool FM_IsAbsolutePath(const char* sPath)
{
	if ( (sPath == NULL) || (sPath[0] == '\0') ) return false;
	if ( sPath[0] == '/' || sPath[0] == '\\' ) return true;
#if defined(_WIN32) || defined(_WIN64)
	if ( ((sPath[0] >= 'A' && sPath[0] <= 'Z') || (sPath[0] >= 'a' && sPath[0] <= 'z')) && sPath[1] == ':' ) {
		return true;
	}
#endif
	return false;
}

static void FM_AssignResolvedPath(char* sDest, size_t iCap, const char* sValue, const char* sBasePath, const char* sFallback)
{
	str sResolved = NULL;
	char sLocal[512];
	const char* sInput = NULL;

	if ( (sDest == NULL) || (iCap == 0) ) return;
	memset(sLocal, 0, sizeof(sLocal));
	if ( sValue && sValue[0] ) {
		snprintf(sLocal, sizeof(sLocal), "%s", sValue);
		sInput = sLocal;
	} else {
		sInput = sFallback;
	}
	sDest[0] = '\0';
	if ( (sInput == NULL) || (sInput[0] == '\0') ) return;

	if ( FM_IsAbsolutePath(sInput) || (sBasePath == NULL) || (sBasePath[0] == '\0') ) {
		snprintf(sDest, iCap, "%s", sInput);
		FM_NormalizePath(sDest);
		return;
	}

	sResolved = xrtPathJoin((str)sBasePath, (str)sInput);
	if ( sResolved ) {
		FM_NormalizePath(sResolved);
		snprintf(sDest, iCap, "%s", (char*)sResolved);
		xrtFree(sResolved);
	}
}

static void FM_PrepareRuntimePaths(void)
{
	const char* sAppPath = xsAppPath();

	/* Backward compatibility: historical configs used "data", but the current
	 * plugin is intended to expose the full filesystem. */
	if ( strcmp(G_FMConfig.sRootPath, "data") == 0 ) {
		snprintf(G_FMConfig.sRootPath, sizeof(G_FMConfig.sRootPath), "%s", "*");
	}
	if ( G_FMConfig.sRootPath[0] == '\0' || strcmp(G_FMConfig.sRootPath, "*") == 0 ) {
		snprintf(G_FMConfig.sRootPath, sizeof(G_FMConfig.sRootPath), "%s", "*");
	} else {
		FM_AssignResolvedPath(G_FMConfig.sRootPath, sizeof(G_FMConfig.sRootPath), G_FMConfig.sRootPath, sAppPath, "*");
		if ( G_FMConfig.sRootPath[0] == '\0' ) {
			snprintf(G_FMConfig.sRootPath, sizeof(G_FMConfig.sRootPath), "%s", "*");
		}
	}
	FM_AssignResolvedPath(G_FMConfig.sToolPath, sizeof(G_FMConfig.sToolPath), G_FMConfig.sToolPath, G_FMRootPath, "tools");

	if ( G_FMDataPath && G_FMDataPath[0] ) {
		str sTemp = xrtPathJoin((str)G_FMDataPath, "temp");
		str sThumb = xrtPathJoin((str)G_FMDataPath, "thumbnail");
		if ( sTemp ) {
			FM_NormalizePath(sTemp);
			snprintf(G_FMConfig.sTempPath, sizeof(G_FMConfig.sTempPath), "%s", (char*)sTemp);
			xrtFree(sTemp);
		}
		if ( sThumb ) {
			FM_NormalizePath(sThumb);
			snprintf(G_FMConfig.sThumbPath, sizeof(G_FMConfig.sThumbPath), "%s", (char*)sThumb);
			xrtFree(sThumb);
		}
	}

	if ( G_FMConfig.sRootPath[0] && !FM_IsFileSystemMode() ) xrtDirCreateAll(G_FMConfig.sRootPath);
	if ( G_FMConfig.sTempPath[0] ) xrtDirCreateAll(G_FMConfig.sTempPath);
	if ( G_FMConfig.sThumbPath[0] ) xrtDirCreateAll(G_FMConfig.sThumbPath);
}

XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int idx, void* ptr)
{
	if ( idx == XADMIN_GLOBAL_MAIN_DB ) {
		G_FMMainDb = (sqlite3*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_XID ) {
		G_FMXid = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_ROOT_PATH ) {
		G_FMRootPath = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_DATA_PATH ) {
		G_FMDataPath = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_PRIVATE_DB_PATH ) {
		G_FMPrivateDbPath = (const char*)ptr;
	}
}

static void FM_NormalizePath(char* path)
{
	int i;
	for ( i = 0; path[i]; i++ ) {
#if defined(_WIN32) || defined(_WIN64)
		if ( path[i] == '/' ) path[i] = '\\';
#else
		if ( path[i] == '\\' ) path[i] = '/';
#endif
	}
}

static bool FM_IsPathSafe(const char* relPath)
{
	if ( relPath == NULL ) return false;
	if ( strstr(relPath, "..") != NULL ) return false;
	if ( FM_IsFileSystemMode() ) {
#if defined(_WIN32) || defined(_WIN64)
		if ( relPath[0] == '\0' ) return true;
		if ( relPath[0] == '/' || relPath[0] == '\\' ) return false;
		if ( strchr(relPath, ':') != NULL ) {
			if ( !(((relPath[0] >= 'A' && relPath[0] <= 'Z') || (relPath[0] >= 'a' && relPath[0] <= 'z')) && relPath[1] == ':') ) {
				return false;
			}
			if ( strchr(relPath + 2, ':') != NULL ) return false;
		}
		return true;
#else
		return (relPath[0] == '\0' || relPath[0] == '/');
#endif
	}
	if ( relPath[0] == '/' || relPath[0] == '\\' ) return false;
	if ( strchr(relPath, ':') != NULL ) return false;
	return true;
}

static str FM_ResolvePath(const char* relPath)
{
	char sBuf[512];
	str sPath;
	const char* sPart = relPath ? relPath : "";
	if ( FM_IsFileSystemMode() ) {
#if defined(_WIN32) || defined(_WIN64)
		if ( sPart[0] == '\0' ) return NULL;
		memset(sBuf, 0, sizeof(sBuf));
		snprintf(sBuf, sizeof(sBuf), "%s", sPart);
		FM_NormalizePath(sBuf);
		if ( ((sBuf[0] >= 'A' && sBuf[0] <= 'Z') || (sBuf[0] >= 'a' && sBuf[0] <= 'z')) && sBuf[1] == ':' && sBuf[2] == '\0' ) {
			sBuf[2] = '\\';
			sBuf[3] = '\0';
		}
		return xrtStrDup(sBuf);
#else
		if ( sPart[0] == '\0' ) return xrtStrDup("/");
		return xrtStrDup((str)sPart);
#endif
	}
	if ( relPath && (relPath[0] == '/' || relPath[0] == '\\') ) {
		sPart = relPath + 1;
	}
	sPath = xrtPathJoin(G_FMConfig.sRootPath, (str)sPart);
	if ( sPath ) FM_NormalizePath(sPath);
	return sPath;
}

void FM_SendJsonCode(XS_ResponseObject objResp, int iCode, xvalue* tblData)
{
	size_t iSize = 0;
	str sJson = xrtJsonStringify(tblData, false, &iSize);
	if ( sJson ) {
		xsHttpReplyAuto(objResp, iCode, "Content-Type: application/json; charset=utf-8\r\n", sJson, iSize);
		xrtFree(sJson);
	}
	xrtValueRelease(tblData);
}

void FM_SendJson(XS_ResponseObject objResp, xvalue* tblData)
{
	FM_SendJsonCode(objResp, 200, tblData);
}

xvalue* FM_Ok(const char* sMessage)
{
	xvalue* tblRet = ValueObject();
	ValueSetBool(tblRet, "result", true);
	if ( sMessage ) ValueSetText(tblRet, "message", (str)sMessage);
	return tblRet;
}

xvalue* FM_Fail(const char* sMessage)
{
	xvalue* tblRet = ValueObject();
	ValueSetBool(tblRet, "result", false);
	if ( sMessage ) ValueSetText(tblRet, "message", (str)sMessage);
	return tblRet;
}

void FM_SendError(XS_ResponseObject objResp, int iCode, const char* sMessage)
{
	FM_SendJsonCode(objResp, iCode, FM_Fail(sMessage));
}

bool FM_SendAssetHtml(XS_ResponseObject objResp, const char* sFileName)
{
	str sPath;
	ptr pData;
	size_t iSize = 0;
	if ( (G_FMRootPath == NULL) || (sFileName == NULL) ) return false;
	sPath = xrtPathJoin(G_FMRootPath, (str)sFileName);
	if ( (sPath == NULL) || !xrtFileExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		return false;
	}
	pData = xrtFileReadAll(sPath, &iSize);
	xrtFree(sPath);
	if ( pData == NULL ) return false;
	xsHttpReplyAuto(objResp, 200, "Content-Type: text/html; charset=utf-8\r\n", pData, iSize);
	xrtFree(pData);
	return true;
}

xvalue* FM_ParseJsonBody(XS_RequestObject objReq)
{
	xvalue* tblForm = JsonParseN((str)XAdmin_ReqBody(objReq), XAdmin_ReqBodyLen(objReq));
	if ( (tblForm == NULL) || (xrtValueType(tblForm) != XVALUE_OBJECT) ) {
		if ( tblForm ) xrtValueRelease(tblForm);
		return NULL;
	}
	return tblForm;
}

str FM_ReadQuery(XS_RequestObject objReq, const char* sName)
{
	char sBuf[512];
	memset(sBuf, 0, sizeof(sBuf));
	xsReqQueryValue(objReq, sName, sBuf, sizeof(sBuf));
	if ( sBuf[0] == '\0' ) return NULL;
	return xrtStrDup(sBuf);
}

static const char* FM_GetImageContentType(const char* sPath)
{
	const char* ext = strrchr(sPath, '.');
	if ( !ext ) return "application/octet-stream";
	if ( strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0 ) return "image/jpeg";
	if ( strcasecmp(ext, ".png") == 0 ) return "image/png";
	if ( strcasecmp(ext, ".gif") == 0 ) return "image/gif";
	if ( strcasecmp(ext, ".bmp") == 0 ) return "image/bmp";
	if ( strcasecmp(ext, ".webp") == 0 ) return "image/webp";
	return "application/octet-stream";
}

static bool FM_IsImageExt(const char* sPath)
{
	const char* ext = strrchr(sPath, '.');
	if ( !ext ) return false;
	return (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0 ||
		strcasecmp(ext, ".png") == 0 || strcasecmp(ext, ".gif") == 0 ||
		strcasecmp(ext, ".bmp") == 0 || strcasecmp(ext, ".webp") == 0);
}

static unsigned long FM_SimpleHash(const char* str)
{
	unsigned long hash = 5381;
	int c;
	while ( (c = *str++) ) {
		hash = ((hash << 5) + hash) + c;
	}
	return hash;
}

static void FM_AppendVirtualRootEntry(xvalue* arrFile, const char* sName)
{
	xvalue* tblInfo = ValueObject();
	ValueSetInt(tblInfo, "id", (int)ValueCount(arrFile));
	ValueSetOwnedText(tblInfo, "name", xrtStrDup((str)sName));
	ValueSetText(tblInfo, "time", (str)"-");
	ValueSetText(tblInfo, "ext", (str)"");
	ValueSetText(tblInfo, "type", (str)"drive");
	ValueSetBool(tblInfo, "isdir", true);
	ValueSetBool(tblInfo, "isroot", true);
	ValueSetInt(tblInfo, "size", 0);
	ValueSetText(tblInfo, "access", (str)"-");
	ValueArrayOwn(arrFile, tblInfo);
}

static void FM_ListVirtualRoots(xvalue* arrFile)
{
	xdirroots roots;
	size_t i;

	if ( !xrtDirRoots(&roots) ) return;
	for ( i = 0; i < roots.Count; i++ ) {
		str sItem = roots.Items[i];
		size_t iLen;
		if ( !sItem || !sItem[0] ) continue;
		/* 展示名去掉尾部分隔符（"C:\" → "C:"；POSIX 根 "/" 保留） */
		iLen = strlen(sItem);
		while ( iLen > 1 && (sItem[iLen - 1] == '/' || sItem[iLen - 1] == '\\') ) {
			sItem[--iLen] = '\0';
		}
		FM_AppendVirtualRootEntry(arrFile, sItem);
	}
	xrtDirRootsFree(&roots);
}

static int FM_ListProc(const char* sPath, size_t iSize, bool bDir, void* pParam)
{
	str sName;
	str sExt;
	xvalue* tblInfo;
	xtime iTime;
	str sTime;
	xvalue* arrFile = (xvalue*)pParam;

	sName = xrtPathName(sPath);
	sExt = Util_ExtNoDot(sPath);
	tblInfo = ValueObject();
	ValueSetInt(tblInfo, "id", (int)ValueCount(arrFile));
	ValueSetOwnedText(tblInfo, "name", sName ? sName : (str)"");
	iTime = FM_FileMTime(sPath);
	sTime = TimeText(iTime, TIME_TEXT_DATETIME);
	ValueSetOwnedText(tblInfo, "time", sTime ? sTime : (str)"");
	if ( sExt && sExt[0] ) {
		ValueSetOwnedText(tblInfo, "ext", sExt);
	} else {
		ValueSetText(tblInfo, "ext", (str)"");
	}
	ValueSetText(tblInfo, "type", (str)"-");
	if ( bDir ) {
		ValueSetBool(tblInfo, "isdir", true);
		ValueSetInt(tblInfo, "size", 0);
	} else {
		ValueSetBool(tblInfo, "isdir", false);
		ValueSetInt(tblInfo, "size", (int64)iSize);
	}
	ValueSetText(tblInfo, "access", (str)"-");
	ValueArrayOwn(arrFile, tblInfo);
	return false;
}

void FM_Req_ViewPage(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !FM_SendAssetHtml(objResp, "page/filemanager.html") ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain\r\n", "page not found", 0);
	}
}

void FM_Req_ApiList(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	str sRelPath;
	str sPath;
	xvalue* tblRet, *arrFile;
	bool bIsRootList = false;

	(void)objServer; (void)objHost; (void)objSession;

	sRelPath = FM_ReadQuery(objReq, "path");
	if ( sRelPath && !FM_IsPathSafe((char*)sRelPath) ) {
		xrtFree(sRelPath);
		FM_SendError(objResp, 400, "invalid path");
		return;
	}
	bIsRootList = (sRelPath == NULL || sRelPath[0] == '\0');
	sPath = FM_ResolvePath(sRelPath ? (char*)sRelPath : "");
	if ( sRelPath ) xrtFree(sRelPath);

	tblRet = ValueObject();
	ValueSetInt(tblRet, "code", 0);
	ValueSetText(tblRet, "msg", (str)"ok");
	arrFile = ValueArray();
	ValueSetOwn(tblRet, "data", arrFile);

	if ( FM_IsFileSystemMode() && bIsRootList ) {
		FM_ListVirtualRoots(arrFile);
	} else if ( sPath && xrtDirExists(sPath) ) {
		DirScan(sPath, false, FM_ListProc, arrFile);
	}
	ValueSetInt(tblRet, "count", (int64)ValueCount(arrFile));

	FM_SendJson(objResp, tblRet);
	if ( sPath ) xrtFree(sPath);
}

void FM_Req_ApiCreate(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* tblForm;
	str sName, sFolderPath, sIsFileStr;
	bool isFile;
	str sPath, sPathName;

	(void)objServer; (void)objHost; (void)objSession;

	tblForm = FM_ParseJsonBody(objReq);
	if ( !tblForm ) { FM_SendError(objResp, 400, "invalid json"); return; }

	sName = ValueText(tblForm, "fileName");
	sFolderPath = ValueText(tblForm, "folderPath");
	if ( !sName || !sName[0] || !sFolderPath ) {
		xrtValueRelease(tblForm);
		FM_SendError(objResp, 400, "fileName and folderPath required");
		return;
	}

	if ( !FM_IsPathSafe((char*)sFolderPath) || !FM_IsPathSafe((char*)sName) ) {
		xrtValueRelease(tblForm);
		FM_SendError(objResp, 400, "invalid path");
		return;
	}

	sIsFileStr = ValueText(tblForm, "isfile");
	isFile = sIsFileStr ? atoi((char*)sIsFileStr) : false;

	sPath = FM_ResolvePath((char*)sFolderPath);
	if ( !sPath || !xrtDirExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		xrtValueRelease(tblForm);
		FM_SendError(objResp, 400, "folderPath not found");
		return;
	}
	sPathName = xrtPathJoin(sPath, sName);
	if ( sPathName ) FM_NormalizePath(sPathName);

	if ( isFile ) {
		FILE* file = fopen(sPathName, "w");
		if ( !file ) {
			xrtFree(sPath);
			xrtFree(sPathName);
			xrtValueRelease(tblForm);
			FM_SendError(objResp, 500, "cannot create file");
			return;
		}
		fclose(file);
	} else {
		xrtDirCreate(sPathName);
	}

	FM_SendJson(objResp, FM_Ok("created"));
	xrtFree(sPath);
	xrtFree(sPathName);
	xrtValueRelease(tblForm);
}

void FM_Req_ApiDelete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* tblForm, *items;
	str sFolderPath;
	str sPath;
	int iCount, i;
	bool allOk = true;

	(void)objServer; (void)objHost; (void)objSession;

	tblForm = FM_ParseJsonBody(objReq);
	if ( !tblForm ) { FM_SendError(objResp, 400, "invalid json"); return; }

	sFolderPath = ValueText(tblForm, "folderPath");
	if ( !sFolderPath || !FM_IsPathSafe((char*)sFolderPath) ) {
		xrtValueRelease(tblForm);
		FM_SendError(objResp, 400, "invalid folderPath");
		return;
	}

	items = ValueGet(tblForm, "items");
	if ( !items || xrtValueType(items) != XVALUE_ARRAY ) {
		xrtValueRelease(tblForm);
		FM_SendError(objResp, 400, "items must be array");
		return;
	}

	sPath = FM_ResolvePath((char*)sFolderPath);
	if ( !sPath || !xrtDirExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		xrtValueRelease(tblForm);
		FM_SendError(objResp, 400, "folderPath not found");
		return;
	}
	iCount = (int)ValueCount(items);

	for ( i = 0; i < iCount; i++ ) {
		xvalue* tblItem = xrtValueArrayGet(items, i);
		if ( !tblItem || xrtValueType(tblItem) != XVALUE_OBJECT ) continue;
		str sName = ValueText(tblItem, "name");
		str sIsDirStr = ValueText(tblItem, "isdir");
		bool isDir = sIsDirStr ? atoi((char*)sIsDirStr) : false;

		if ( !sName || !FM_IsPathSafe((char*)sName) ) { allOk = false; continue; }
		str sFull = xrtPathJoin(sPath, sName);
		if ( isDir ) {
			if ( xrtDirExists(sFull) ) xrtDirRemoveAll(sFull); else allOk = false;
		} else {
			if ( xrtFileExists(sFull) ) xrtFileDelete(sFull); else allOk = false;
		}
		xrtFree(sFull);
	}

	FM_SendJson(objResp, allOk ? FM_Ok("deleted") : FM_Fail("some items failed to delete"));
	xrtFree(sPath);
	xrtValueRelease(tblForm);
}

void FM_Req_ApiCopy(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* tblForm, *items;
	str sTargetPath;
	str sTarget;
	int iCount, i;

	(void)objServer; (void)objHost; (void)objSession;

	tblForm = FM_ParseJsonBody(objReq);
	if ( !tblForm ) { FM_SendError(objResp, 400, "invalid json"); return; }

	sTargetPath = ValueText(tblForm, "targetPath");
	if ( !sTargetPath || !FM_IsPathSafe((char*)sTargetPath) ) {
		xrtValueRelease(tblForm);
		FM_SendError(objResp, 400, "invalid targetPath");
		return;
	}

	str sIsCutStr = ValueText(tblForm, "isCut");
	bool isCut = sIsCutStr ? atoi((char*)sIsCutStr) : false;

	items = ValueGet(tblForm, "items");
	if ( !items || xrtValueType(items) != XVALUE_ARRAY ) {
		xrtValueRelease(tblForm);
		FM_SendError(objResp, 400, "items must be array");
		return;
	}

	sTarget = FM_ResolvePath((char*)sTargetPath);
	if ( !sTarget || !xrtDirExists(sTarget) ) {
		if ( sTarget ) xrtFree(sTarget);
		xrtValueRelease(tblForm);
		FM_SendError(objResp, 400, "targetPath not found");
		return;
	}
	iCount = (int)ValueCount(items);

	for ( i = 0; i < iCount; i++ ) {
		xvalue* tblItem = xrtValueArrayGet(items, i);
		if ( !tblItem || xrtValueType(tblItem) != XVALUE_OBJECT ) continue;
		str sSourcePath = ValueText(tblItem, "sourcePath");
		if ( !sSourcePath || !FM_IsPathSafe((char*)sSourcePath) ) continue;

		char* fileName = strrchr((char*)sSourcePath, '/');
		fileName = fileName ? fileName + 1 : (char*)sSourcePath;
		str sSource = FM_ResolvePath((char*)sSourcePath);
		str sTargetFull = xrtPathJoin(sTarget, fileName);
		if ( sTargetFull ) FM_NormalizePath(sTargetFull);

		str sIsDirStr = ValueText(tblItem, "isdir");
		bool isDir = sIsDirStr ? atoi((char*)sIsDirStr) : false;

		if ( isDir ) {
			if ( isCut ) xrtDirMove(sSource, sTargetFull, true);
			else xrtDirCopy(sSource, sTargetFull, true);
		} else {
			if ( isCut ) xrtFileMove(sSource, sTargetFull, true);
			else xrtFileCopy(sSource, sTargetFull, true);
		}
		xrtFree(sSource);
		xrtFree(sTargetFull);
	}

	FM_SendJson(objResp, FM_Ok("operation completed"));
	xrtFree(sTarget);
	xrtValueRelease(tblForm);
}

void FM_Req_ApiContent(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	str sRelPath, sPath;
	str sContent;
	xvalue* tblRet;

	(void)objServer; (void)objHost; (void)objSession;

	sRelPath = FM_ReadQuery(objReq, "path");
	if ( !sRelPath || !FM_IsPathSafe((char*)sRelPath) ) {
		if ( sRelPath ) xrtFree(sRelPath);
		FM_SendError(objResp, 400, "invalid path");
		return;
	}

	sPath = FM_ResolvePath((char*)sRelPath);
	xrtFree(sRelPath);

	if ( !sPath || !xrtFileExists(sPath) || xrtDirExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		FM_SendError(objResp, 400, "file not found or is directory");
		return;
	}

	sContent = (str)xrtFileReadAll(sPath, NULL);
	xrtFree(sPath);
	if ( !sContent ) { FM_SendError(objResp, 500, "read failed"); return; }

	tblRet = FM_Ok("ok");
	ValueSetOwnedText(tblRet, "content", sContent);
	FM_SendJson(objResp, tblRet);
}

void FM_Req_ApiSave(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* tblForm;
	str sRelPath, sContent, sPath;

	(void)objServer; (void)objHost; (void)objSession;

	tblForm = FM_ParseJsonBody(objReq);
	if ( !tblForm ) { FM_SendError(objResp, 400, "invalid json"); return; }

	sRelPath = ValueText(tblForm, "path");
	sContent = ValueText(tblForm, "content");
	if ( !sRelPath || !sContent || !FM_IsPathSafe((char*)sRelPath) ) {
		xrtValueRelease(tblForm);
		FM_SendError(objResp, 400, "invalid path or content");
		return;
	}

	sPath = FM_ResolvePath((char*)sRelPath);
	if ( !sPath || !xrtFileExists(sPath) || xrtDirExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		xrtValueRelease(tblForm);
		FM_SendError(objResp, 400, "file not found or is directory");
		return;
	}

	{
		size_t iSize = strlen((char*)sContent);
		bool bWrite = xrtFileWriteAll(sPath, (xbytesview){(cbytes)sContent, iSize});
		xrtFree(sPath);
		xrtValueRelease(tblForm);
		FM_SendJson(objResp, bWrite ? FM_Ok("saved") : FM_Fail("write failed"));
	}
}

void FM_Req_ApiUpload(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	const char* sBody;
	size_t iBodyLen;
	const char* sContentType;
	char sBoundary[76];
	size_t iOffset;
	str sRelPath, sUploadDir;
	xvalue* tblRet;

	(void)objServer; (void)objHost; (void)objSession;

	sBody = XAdmin_ReqBody(objReq);
	iBodyLen = XAdmin_ReqBodyLen(objReq);
	sContentType = XAdmin_PluginReqHeader(objReq, "Content-Type");

	if ( !sContentType || !XAdmin_MultipartBoundary(sContentType, sBoundary, sizeof(sBoundary)) ) {
		FM_SendError(objResp, 400, "invalid multipart request");
		return;
	}

	sRelPath = FM_ReadQuery(objReq, "path");
	if ( !sRelPath && strcmp(G_FMConfig.sRootPath, "*") == 0 ) {
		/* 文件系统模式没有默认目录（字面 "*" 是无效路径），必须显式指定 */
		FM_SendError(objResp, 400, "path required in filesystem mode");
		return;
	}
	sUploadDir = sRelPath ? FM_ResolvePath((char*)sRelPath) : xrtStrDup(G_FMConfig.sRootPath);
	if ( sRelPath ) xrtFree(sRelPath);

	if ( sUploadDir && !xrtDirExists(sUploadDir) ) {
		xrtDirCreateAll(sUploadDir);
	}

	iOffset = 0;
	tblRet = NULL;

	{
		XAdminMultipartPart part;
		memset(&part, 0, sizeof(part));
		while ( XAdmin_MultipartNext(sBody, iBodyLen, sBoundary, strlen(sBoundary), &iOffset, &part) ) {
			if ( part.filename && part.filenameLen > 0 ) {
				char sFileName[256];
				if ( FM_SanitizeFileName(part.filename, part.filenameLen, sFileName, sizeof(sFileName)) > 0 ) {
					str sFilePath = xrtPathJoin(sUploadDir, sFileName);
					if ( sFilePath ) {
						if ( xrtFileWriteAll(sFilePath, (xbytesview){(cbytes)part.data, part.size}) ) {
							xvalue* r = FM_Ok("uploaded");
							ValueSetText(r, "name", sFileName);
							ValueSetText(r, "url", sFileName);
							if ( tblRet ) xrtValueRelease(tblRet);
							tblRet = r;
						}
						xrtFree(sFilePath);
					}
				}
			}
		}
	}

	xrtFree(sUploadDir);

	if ( tblRet ) {
		FM_SendJson(objResp, tblRet);
	} else {
		FM_SendError(objResp, 400, "no file found in request");
	}
}

void FM_Req_ApiDownload(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	str sRelPath, sPath, sName;
	ptr pData;
	size_t iSize = 0;
	char sHeader[512];
	const char* sContentType;

	(void)objServer; (void)objHost; (void)objSession;

	sRelPath = FM_ReadQuery(objReq, "path");
	if ( !sRelPath || !FM_IsPathSafe((char*)sRelPath) ) {
		if ( sRelPath ) xrtFree(sRelPath);
		FM_SendError(objResp, 400, "invalid path");
		return;
	}

	sPath = FM_ResolvePath((char*)sRelPath);
	xrtFree(sRelPath);

	if ( !sPath || !xrtFileExists(sPath) || xrtDirExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		FM_SendError(objResp, 404, "file not found");
		return;
	}

	pData = xrtFileReadAll(sPath, &iSize);
	if ( !pData ) {
		xrtFree(sPath);
		FM_SendError(objResp, 500, "read failed");
		return;
	}

	sName = xrtPathName(sPath);
	sContentType = FM_GetImageContentType(sPath);
	snprintf(sHeader, sizeof(sHeader),
		"Content-Type: %s\r\n"
		"Content-Disposition: attachment; filename=\"%s\"\r\n"
		"Content-Length: %llu\r\n",
		sContentType, sName ? (char*)sName : "download", (unsigned long long)iSize);

	xsHttpReplyAuto(objResp, 200, sHeader, pData, iSize);

	xrtFree(pData);
	xrtFree(sPath);
	if ( sName ) xrtFree(sName);
}

void FM_Req_ApiImage(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	str sRelPath, sPath;
	ptr pData;
	size_t iSize = 0;
	char sHeader[256];
	const char* sContentType;

	(void)objServer; (void)objHost; (void)objSession;

	sRelPath = FM_ReadQuery(objReq, "path");
	if ( !sRelPath || !FM_IsPathSafe((char*)sRelPath) ) {
		if ( sRelPath ) xrtFree(sRelPath);
		FM_SendError(objResp, 400, "invalid path");
		return;
	}

	sPath = FM_ResolvePath((char*)sRelPath);
	xrtFree(sRelPath);

	if ( !sPath || !xrtFileExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		FM_SendError(objResp, 404, "not found");
		return;
	}

	pData = xrtFileReadAll(sPath, &iSize);
	if ( !pData ) {
		xrtFree(sPath);
		FM_SendError(objResp, 500, "read failed");
		return;
	}

	sContentType = FM_GetImageContentType(sPath);
	snprintf(sHeader, sizeof(sHeader),
		"Content-Type: %s\r\nCache-Control: max-age=86400\r\n", sContentType);

	xsHttpReplyAuto(objResp, 200, sHeader, pData, iSize);
	xrtFree(pData);
	xrtFree(sPath);
}

void FM_Req_ApiThumbnail(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	str sRelPath, sPath;
	str sSizeStr;
	int thumbSize = 64;
	char sThumbPath[4096];
	const char* sFilename;
	unsigned long hash;
	bool needGen;
	char sCmd[1024];
	ptr pData;
	size_t iSize = 0;
	char sHeader[256];
	const char* sContentType;

	(void)objServer; (void)objHost; (void)objSession;

	sRelPath = FM_ReadQuery(objReq, "path");
	if ( !sRelPath || !FM_IsPathSafe((char*)sRelPath) ) {
		if ( sRelPath ) xrtFree(sRelPath);
		FM_SendError(objResp, 400, "invalid path");
		return;
	}

	sSizeStr = FM_ReadQuery(objReq, "size");
	if ( sSizeStr ) { thumbSize = atoi((char*)sSizeStr); xrtFree(sSizeStr); }
	if ( thumbSize < 64 ) thumbSize = 64;
	if ( thumbSize > 512 ) thumbSize = 512;

	sPath = FM_ResolvePath((char*)sRelPath);
	xrtFree(sRelPath);

	if ( !sPath || !xrtFileExists(sPath) || !FM_IsImageExt(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		FM_SendError(objResp, 400, "not an image or not found");
		return;
	}

	sFilename = strrchr(sPath, '/');
	if ( !sFilename ) sFilename = strrchr(sPath, '\\');
	sFilename = sFilename ? sFilename + 1 : (char*)sPath;
	hash = FM_SimpleHash(sPath);

#if defined(_WIN32) || defined(_WIN64)
	snprintf(sThumbPath, sizeof(sThumbPath), "%s\\thumb_%08lx_%dx%d_%s",
		G_FMConfig.sThumbPath, hash, thumbSize, thumbSize, sFilename);
#else
	snprintf(sThumbPath, sizeof(sThumbPath), "%s/thumb_%08lx_%dx%d_%s",
		G_FMConfig.sThumbPath, hash, thumbSize, thumbSize, sFilename);
#endif

	needGen = true;
	if ( xrtFileExists(sThumbPath) ) {
		xtime tOrig = FM_FileMTime(sPath);
		xtime tThumb = FM_FileMTime(sThumbPath);
		needGen = (tOrig != tThumb);
	}

	if ( needGen ) {
#if defined(_WIN32) || defined(_WIN64)
		snprintf(sCmd, sizeof(sCmd),
			"%s\\magick \"%s\" -thumbnail %dx%d -background white -gravity center -extent %dx%d \"%s\"",
			G_FMConfig.sToolPath, sPath, thumbSize, thumbSize, thumbSize, thumbSize, sThumbPath);
#else
		snprintf(sCmd, sizeof(sCmd),
			"convert \"%s\" -thumbnail %dx%d -background white -gravity center -extent %dx%d \"%s\"",
			sPath, thumbSize, thumbSize, thumbSize, thumbSize, sThumbPath);
#endif
		system(sCmd);
	}

	pData = xrtFileReadAll(sThumbPath, &iSize);
	if ( !pData ) {
		pData = xrtFileReadAll(sPath, &iSize);
		if ( !pData ) {
			xrtFree(sPath);
			FM_SendError(objResp, 500, "thumbnail failed");
			return;
		}
	}

	sContentType = FM_GetImageContentType(sPath);
	snprintf(sHeader, sizeof(sHeader),
		"Content-Type: %s\r\nCache-Control: max-age=86400\r\n", sContentType);

	xsHttpReplyAuto(objResp, 200, sHeader, pData, iSize);
	xrtFree(pData);
	xrtFree(sPath);
}

void FM_Req_ApiCompress(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* tblForm, *items;
	str sFormat, sFilename, sSavePath, sSourcePath;
	str sSrc, sSave, sArchivePath, sTempList;
	FILE* fp;
	int iCount, i, ret = 0;
	char sCmd[2048];
	char sArcName[256];

	(void)objServer; (void)objHost; (void)objSession;

	tblForm = FM_ParseJsonBody(objReq);
	if ( !tblForm ) { FM_SendError(objResp, 400, "invalid json"); return; }

	sFormat = ValueText(tblForm, "format");
	sFilename = ValueText(tblForm, "filename");
	sSavePath = ValueText(tblForm, "savePath");
	sSourcePath = ValueText(tblForm, "sourcePath");
	items = ValueGet(tblForm, "items");

	if ( !sFormat || !sFilename || !sSavePath || !sSourcePath || !items ||
		xrtValueType(items) != XVALUE_ARRAY ||
		!FM_IsPathSafe((char*)sSavePath) || !FM_IsPathSafe((char*)sSourcePath) ) {
		xrtValueRelease(tblForm);
		FM_SendError(objResp, 400, "invalid parameters");
		return;
	}

	sSrc = FM_ResolvePath((char*)sSourcePath);
	sSave = FM_ResolvePath((char*)sSavePath);
	snprintf(sArcName, sizeof(sArcName), "%s.%s", (char*)sFilename, (char*)sFormat);
	sArchivePath = xrtPathJoin(sSave, sArcName);

	if ( !xrtDirExists(sSave) ) xrtDirCreateAll(sSave);

	sTempList = xrtFormat("%s/_filelist-%u.txt", (char*)G_FMConfig.sTempPath, (unsigned)XAdmin_UnixNowUs() % 1000000u);
	fp = fopen(sTempList, "w");
	if ( !fp ) {
		FM_SendError(objResp, 500, "cannot create temp file list");
		goto compress_cleanup;
	}

	iCount = (int)ValueCount(items);
	for ( i = 0; i < iCount; i++ ) {
		xvalue* tblItem = xrtValueArrayGet(items, i);
		if ( !tblItem || xrtValueType(tblItem) != XVALUE_OBJECT ) continue;
		str sName = ValueText(tblItem, "name");
		if ( sName ) fprintf(fp, "%s\n", (char*)sName);
	}
	fclose(fp);

#if defined(_WIN32) || defined(_WIN64)
	xrtPathSetCwd(sSrc);
	if ( strcmp((char*)sFormat, "tar.gz") == 0 || strcmp((char*)sFormat, "tgz") == 0 ) {
		char sTempTar[1024];
		snprintf(sTempTar, sizeof(sTempTar), "%s\\temp_%u.tar", (char*)sSave, (unsigned)(XAdmin_UnixNowUs() % 1000000));
		snprintf(sCmd, sizeof(sCmd), "%s\\7z.exe a -ttar \"%s\" @\"%s\"", (char*)G_FMConfig.sToolPath, sTempTar, (char*)sTempList);
		ret = system(sCmd);
		if ( ret == 0 ) {
			snprintf(sCmd, sizeof(sCmd), "%s\\7z.exe a -tgzip \"%s\" \"%s\"", (char*)G_FMConfig.sToolPath, (char*)sArchivePath, sTempTar);
			ret = system(sCmd);
			xrtFileDelete(sTempTar);
		}
	} else {
		snprintf(sCmd, sizeof(sCmd), "%s\\7z.exe a -t%s \"%s\" @\"%s\"", (char*)G_FMConfig.sToolPath, (char*)sFormat, (char*)sArchivePath, (char*)sTempList);
		ret = system(sCmd);
	}
#else
	xrtPathSetCwd(sSrc);
	if ( strcmp((char*)sFormat, "tar.gz") == 0 || strcmp((char*)sFormat, "tgz") == 0 ) {
		snprintf(sCmd, sizeof(sCmd), "tar -czf \"%s\" -T \"%s\"", (char*)sArchivePath, (char*)sTempList);
	} else if ( strcmp((char*)sFormat, "zip") == 0 ) {
		snprintf(sCmd, sizeof(sCmd), "zip -r \"%s\" -@ < \"%s\"", (char*)sArchivePath, (char*)sTempList);
	} else if ( strcmp((char*)sFormat, "tar") == 0 ) {
		snprintf(sCmd, sizeof(sCmd), "tar -cf \"%s\" -T \"%s\"", (char*)sArchivePath, (char*)sTempList);
	} else {
		ret = -1;
	}
	if ( ret == 0 ) ret = system(sCmd);
#endif

	remove(sTempList);

	if ( ret == 0 && xrtFileExists(sArchivePath) ) {
		FM_SendJson(objResp, FM_Ok("compressed"));
	} else {
		FM_SendJson(objResp, FM_Fail("compression failed"));
	}

compress_cleanup:
	xrtFree(sTempList);
	xrtFree(sSrc);
	xrtFree(sSave);
	xrtFree(sArchivePath);
	xrtValueRelease(tblForm);
}

void FM_Req_ApiExtract(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* tblForm;
	str sArchivePath, sExtractPath;
	str sArc, sExtract;
	const char* ext;
	char sCmd[2048];
	int ret = 0;

	(void)objServer; (void)objHost; (void)objSession;

	tblForm = FM_ParseJsonBody(objReq);
	if ( !tblForm ) { FM_SendError(objResp, 400, "invalid json"); return; }

	sArchivePath = ValueText(tblForm, "archivePath");
	sExtractPath = ValueText(tblForm, "extractPath");
	if ( !sArchivePath || !sExtractPath ||
		!FM_IsPathSafe((char*)sArchivePath) || !FM_IsPathSafe((char*)sExtractPath) ) {
		xrtValueRelease(tblForm);
		FM_SendError(objResp, 400, "invalid parameters");
		return;
	}

	sArc = FM_ResolvePath((char*)sArchivePath);
	sExtract = FM_ResolvePath((char*)sExtractPath);
	xrtDirCreateAll(sExtract);

	ext = strrchr((char*)sArchivePath, '.');
	if ( ext ) ext++;

#if defined(_WIN32) || defined(_WIN64)
		snprintf(sCmd, sizeof(sCmd), "%s\\7z.exe x \"%s\" -o\"%s\" -y", (char*)G_FMConfig.sToolPath, (char*)sArc, (char*)sExtract);
		ret = system(sCmd);
#else
	if ( ext && (strcmp(ext, "gz") == 0 || strcmp(ext, "tgz") == 0) ) {
		snprintf(sCmd, sizeof(sCmd), "tar -xzf \"%s\" -C \"%s\"", (char*)sArc, (char*)sExtract);
	} else if ( ext && strcmp(ext, "zip") == 0 ) {
		snprintf(sCmd, sizeof(sCmd), "unzip -o \"%s\" -d \"%s\"", (char*)sArc, (char*)sExtract);
	} else if ( ext && strcmp(ext, "tar") == 0 ) {
		snprintf(sCmd, sizeof(sCmd), "tar -xf \"%s\" -C \"%s\"", (char*)sArc, (char*)sExtract);
	} else {
		ret = -1;
	}
	if ( ret == 0 ) ret = system(sCmd);
#endif

	FM_SendJson(objResp, ret == 0 ? FM_Ok("extracted") : FM_Fail("extraction failed"));
	xrtFree(sArc);
	xrtFree(sExtract);
	xrtValueRelease(tblForm);
}

int FM_OnLoad(XAdminPluginHandle* out_handle)
{
	if ( out_handle ) G_FMHandle = *out_handle;
	return 0;
}

int FM_OnInstall(XAdminPluginHandle handle)
{
	(void)handle;
	return 0;
}

int FM_OnStart(XAdminPluginHandle handle)
{
	XAdminMenuDecl menu;
	XAdminRouteDecl route;
	int iAuthGroupId = 0;
	int iAuthId = 0;
	XAdminAuthGroupDecl authGroup;
	XAdminAuthDecl auth;

	G_FMHandle = handle;

	memset(&authGroup, 0, sizeof(authGroup));
	authGroup.scope = XADMIN_AUTH_SCOPE_ADMIN;
	authGroup.name = "File Manager";
	authGroup.description = "File management permissions";
	authGroup.sort = 400000;
	if ( XAdmin_RegisterAuthGroup(handle, &authGroup, &iAuthGroupId, NULL) != 0 ) return -1;

	memset(&auth, 0, sizeof(auth));
	auth.scope = XADMIN_AUTH_SCOPE_ADMIN;
	auth.group_id = iAuthGroupId;
	auth.name = "filemanager.manage";
	auth.description = "Manage files and directories";
	auth.sort = 400001;
	if ( XAdmin_RegisterAuth(handle, &auth, &iAuthId, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/filemanager";
	route.proc = FM_Req_ViewPage;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/list";
	route.proc = FM_Req_ApiList;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/upload";
	route.proc = FM_Req_ApiUpload;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/download";
	route.proc = FM_Req_ApiDownload;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/create";
	route.proc = FM_Req_ApiCreate;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/delete";
	route.proc = FM_Req_ApiDelete;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/copy";
	route.proc = FM_Req_ApiCopy;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/content";
	route.proc = FM_Req_ApiContent;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/save";
	route.proc = FM_Req_ApiSave;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/image";
	route.proc = FM_Req_ApiImage;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/thumbnail";
	route.proc = FM_Req_ApiThumbnail;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/compress";
	route.proc = FM_Req_ApiCompress;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/extract";
	route.proc = FM_Req_ApiExtract;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&menu, 0, sizeof(menu));
	menu.title = "File Manager";
	menu.icon = "layui-icon layui-icon-file";
	menu.type = 1;
	menu.open_type = "_iframe";
	menu.href = "/admin/view/plugin/filemanager";
	menu.sort = 400;
	menu.visible = true;
	menu.remark = "File management";
	if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) return -1;

	printf("[filemanager] started\n");
	return 0;
}

int FM_OnConfigChanged(XAdminPluginHandle handle, xvalue* new_cfg)
{
	(void)handle;

	memset(&G_FMConfig, 0, sizeof(G_FMConfig));

	if ( new_cfg && xrtValueType(new_cfg) == XVALUE_OBJECT ) {
		str sVal = ValueText(new_cfg, "rootPath");
		if ( sVal && sVal[0] ) {
			strncpy(G_FMConfig.sRootPath, (char*)sVal, sizeof(G_FMConfig.sRootPath) - 1);
		}
		sVal = ValueText(new_cfg, "toolPath");
		if ( sVal && sVal[0] ) {
			strncpy(G_FMConfig.sToolPath, (char*)sVal, sizeof(G_FMConfig.sToolPath) - 1);
		}
	}

	if ( G_FMConfig.sRootPath[0] == '\0' ) {
		strcpy(G_FMConfig.sRootPath, "*");
	}
	if ( G_FMConfig.sToolPath[0] == '\0' ) {
		strcpy(G_FMConfig.sToolPath, "tools");
	}

	FM_PrepareRuntimePaths();

	return 0;
}

int FM_OnHealthCheck(XAdminPluginHandle handle, XAdminHealthReport* out_report)
{
	(void)handle;
	if ( out_report ) {
		out_report->status_code = 0;
		out_report->message = "ok";
	}
	return 0;
}

void FM_OnStop(XAdminPluginHandle handle)
{
	(void)handle;
	G_FMHandle = NULL;
	printf("[filemanager] stopped\n");
}

void FM_OnUnload(XAdminPluginHandle handle)
{
	(void)handle;
	G_FMHandle = NULL;
	G_FMMainDb = NULL;
	G_FMXid = NULL;
	G_FMRootPath = NULL;
	G_FMDataPath = NULL;
	G_FMPrivateDbPath = NULL;
}

static XAdminPluginDescriptor G_FMPlugin = {
	XADMIN_ABI_VERSION,
	sizeof(XAdminPluginDescriptor),
	"filemanager",
	"1.0.0",
	"File Manager",
	FM_OnLoad,
	FM_OnInstall,
	FM_OnStart,
	FM_OnConfigChanged,
	FM_OnHealthCheck,
	FM_OnStop,
	FM_OnUnload
};

XADMIN_DECLARE_PLUGIN(G_FMPlugin)
