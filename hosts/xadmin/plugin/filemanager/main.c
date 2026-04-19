#include "xs_plugin.h"

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
	if ( relPath[0] == '/' || relPath[0] == '\\' ) return FALSE;
	if ( strstr(relPath, "..") != NULL ) return FALSE;
	return TRUE;
}

static str FM_ResolvePath(const char* relPath)
{
	str sPath;
	if ( relPath[0] == '/' || relPath[0] == '\\' ) {
		relPath++;
	}
	sPath = xrtPathJoin(2, G_FMConfig.sRootPath, (str)relPath);
	if ( sPath ) FM_NormalizePath(sPath);
	return sPath;
}

void FM_SendJson(XS_ResponseObject objResp, xvalue tblData)
{
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblData, FALSE, &iSize);
	if ( sJson ) {
		http_reply(objResp, 200, "Content-Type: application/json\r\n", sJson, iSize);
		xrtFree(sJson);
	}
	xvoUnref(tblData);
}

xvalue FM_Ok(const char* sMessage)
{
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	if ( sMessage ) xvoTableSetText(tblRet, "message", 7, (str)sMessage, 0, FALSE);
	return tblRet;
}

xvalue FM_Fail(const char* sMessage)
{
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, FALSE);
	if ( sMessage ) xvoTableSetText(tblRet, "message", 7, (str)sMessage, 0, FALSE);
	return tblRet;
}

void FM_SendError(XS_ResponseObject objResp, int iCode, const char* sMessage)
{
	FM_SendJson(objResp, FM_Fail(sMessage));
}

bool FM_SendAssetHtml(XS_ResponseObject objResp, const char* sFileName)
{
	str sPath;
	ptr pData;
	size_t iSize = 0;
	if ( (G_FMRootPath == NULL) || (sFileName == NULL) ) return FALSE;
	sPath = xrtPathJoin(2, G_FMRootPath, (str)sFileName);
	if ( (sPath == NULL) || !xrtFileExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		return FALSE;
	}
	pData = xrtFileGetAll(sPath, &iSize);
	xrtFree(sPath);
	if ( pData == NULL ) return FALSE;
	http_reply(objResp, 200, "Content-Type: text/html; charset=utf-8\r\n", pData, iSize);
	xrtFree(pData);
	return TRUE;
}

xvalue FM_ParseJsonBody(XS_RequestObject objReq)
{
	xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( (tblForm == NULL) || (xvoType(tblForm) != XVO_DT_TABLE) ) {
		if ( tblForm ) xvoUnref(tblForm);
		return NULL;
	}
	return tblForm;
}

str FM_ReadQuery(XS_RequestObject objReq, const char* sName)
{
	char sBuf[512];
	memset(sBuf, 0, sizeof(sBuf));
	HttpGetQueryVar(objReq, sName, sBuf, sizeof(sBuf));
	if ( sBuf[0] == '\0' ) return NULL;
	return xrtCopyStr(sBuf, 0);
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
	if ( !ext ) return FALSE;
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

typedef struct {
	str sPath;
	size_t iSize;
	int bDir;
	ptr pData;
	xvalue arrFile;
} FM_ScanCtx;

static int FM_ListProc(str sPath, size_t iSize, int bDir, ptr pData, xvalue arrFile)
{
	str sName;
	str sExt;
	xvalue tblInfo;
	xtime iTime;
	str sTime;

	if ( bDir == 2 ) return FALSE;

	sName = xrtPathGetNameExt(sPath, iSize);
	sExt = xrtPathGetExt(sPath, iSize);
	tblInfo = xvoCreateTable();
	xvoTableSetInt(tblInfo, "id", 2, (int)xvoArrayLength(arrFile));
	xvoTableSetText(tblInfo, "name", 4, sName, 0, FALSE);
	iTime = xrtFileGetChangeTime(sPath);
	sTime = xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME);
	xvoTableSetText(tblInfo, "time", 4, sTime, 0, TRUE);
	xvoTableSetText(tblInfo, "ext", 3, sExt ? sExt : (str)"", 0, FALSE);
	xvoTableSetText(tblInfo, "type", 4, (str)"-", 0, FALSE);
	if ( bDir == 1 ) {
		xvoTableSetBool(tblInfo, "isdir", 5, TRUE);
		xvoTableSetInt(tblInfo, "size", 4, 0);
	} else {
		xvoTableSetBool(tblInfo, "isdir", 5, FALSE);
		xvoTableSetInt(tblInfo, "size", 4, (int64)xrtFileGetSize(sPath));
	}
	xvoTableSetText(tblInfo, "access", 6, (str)"-", 0, FALSE);
	xvoArrayAppendValue(arrFile, tblInfo, TRUE);
	return FALSE;
}

void FM_Req_ViewPage(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !FM_SendAssetHtml(objResp, "page/filemanager.html") ) {
		http_reply(objResp, 500, "Content-Type: text/plain\r\n", "page not found", 0);
	}
}

void FM_Req_ApiList(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	str sRelPath;
	str sPath;
	xvalue tblRet, arrFile;

	(void)objServer; (void)objHost; (void)objSession;

	sRelPath = FM_ReadQuery(objReq, "path");
	sPath = FM_ResolvePath(sRelPath ? (char*)sRelPath : "");
	if ( sRelPath ) xrtFree(sRelPath);

	tblRet = xvoCreateTable();
	xvoTableSetInt(tblRet, "code", 4, 0);
	xvoTableSetText(tblRet, "msg", 3, (str)"ok", 0, FALSE);
	arrFile = xvoCreateArray();
	xvoTableSetValue(tblRet, "data", 4, arrFile, TRUE);

	if ( sPath && xrtDirExists(sPath) ) {
		xrtDirScan(sPath, FALSE, FM_ListProc, arrFile);
	}
	xvoTableSetInt(tblRet, "count", 5, (int64)xvoArrayLength(arrFile));

	FM_SendJson(objResp, tblRet);
	if ( sPath ) xrtFree(sPath);
}

void FM_Req_ApiCreate(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm;
	str sName, sFolderPath, sIsFileStr;
	bool isFile;
	str sPath, sPathName;

	(void)objServer; (void)objHost; (void)objSession;

	tblForm = FM_ParseJsonBody(objReq);
	if ( !tblForm ) { FM_SendError(objResp, 400, "invalid json"); return; }

	sName = xvoTableGetText(tblForm, "fileName", 8);
	sFolderPath = xvoTableGetText(tblForm, "folderPath", 10);
	if ( !sName || !sName[0] || !sFolderPath ) {
		xvoUnref(tblForm);
		FM_SendError(objResp, 400, "fileName and folderPath required");
		return;
	}

	if ( !FM_IsPathSafe((char*)sFolderPath) || !FM_IsPathSafe((char*)sName) ) {
		xvoUnref(tblForm);
		FM_SendError(objResp, 400, "invalid path");
		return;
	}

	sIsFileStr = xvoTableGetText(tblForm, "isfile", 6);
	isFile = sIsFileStr ? atoi((char*)sIsFileStr) : FALSE;

	sPath = FM_ResolvePath((char*)sFolderPath);
	sPathName = xrtPathJoin(2, sPath, sName);

	if ( isFile ) {
		FILE* file = fopen(sPathName, "w");
		if ( !file ) {
			xrtFree(sPath);
			xrtFree(sPathName);
			xvoUnref(tblForm);
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
	xvoUnref(tblForm);
}

void FM_Req_ApiDelete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm, items;
	str sFolderPath;
	str sPath;
	int iCount, i;
	bool allOk = TRUE;

	(void)objServer; (void)objHost; (void)objSession;

	tblForm = FM_ParseJsonBody(objReq);
	if ( !tblForm ) { FM_SendError(objResp, 400, "invalid json"); return; }

	sFolderPath = xvoTableGetText(tblForm, "folderPath", 10);
	if ( !sFolderPath || !FM_IsPathSafe((char*)sFolderPath) ) {
		xvoUnref(tblForm);
		FM_SendError(objResp, 400, "invalid folderPath");
		return;
	}

	items = xvoTableGetValue(tblForm, "items", 5);
	if ( !items || xvoType(items) != XVO_DT_ARRAY ) {
		xvoUnref(tblForm);
		FM_SendError(objResp, 400, "items must be array");
		return;
	}

	sPath = FM_ResolvePath((char*)sFolderPath);
	iCount = (int)xvoArrayLength(items);

	for ( i = 0; i < iCount; i++ ) {
		xvalue tblItem = xvoArrayGetValue(items, i);
		if ( !tblItem || xvoType(tblItem) != XVO_DT_TABLE ) continue;
		str sName = xvoTableGetText(tblItem, "name", 4);
		str sIsDirStr = xvoTableGetText(tblItem, "isdir", 5);
		bool isDir = sIsDirStr ? atoi((char*)sIsDirStr) : FALSE;

		if ( !sName || !FM_IsPathSafe((char*)sName) ) { allOk = FALSE; continue; }
		str sFull = xrtPathJoin(2, sPath, sName);
		if ( isDir ) {
			if ( xrtDirExists(sFull) ) xrtDirDelete(sFull); else allOk = FALSE;
		} else {
			if ( xrtFileExists(sFull) ) xrtFileDelete(sFull); else allOk = FALSE;
		}
		xrtFree(sFull);
	}

	FM_SendJson(objResp, allOk ? FM_Ok("deleted") : FM_Fail("some items failed to delete"));
	xrtFree(sPath);
	xvoUnref(tblForm);
}

void FM_Req_ApiCopy(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm, items;
	str sTargetPath;
	str sTarget;
	int iCount, i;

	(void)objServer; (void)objHost; (void)objSession;

	tblForm = FM_ParseJsonBody(objReq);
	if ( !tblForm ) { FM_SendError(objResp, 400, "invalid json"); return; }

	sTargetPath = xvoTableGetText(tblForm, "targetPath", 10);
	if ( !sTargetPath || !FM_IsPathSafe((char*)sTargetPath) ) {
		xvoUnref(tblForm);
		FM_SendError(objResp, 400, "invalid targetPath");
		return;
	}

	str sIsCutStr = xvoTableGetText(tblForm, "isCut", 5);
	bool isCut = sIsCutStr ? atoi((char*)sIsCutStr) : FALSE;

	items = xvoTableGetValue(tblForm, "items", 5);
	if ( !items || xvoType(items) != XVO_DT_ARRAY ) {
		xvoUnref(tblForm);
		FM_SendError(objResp, 400, "items must be array");
		return;
	}

	sTarget = FM_ResolvePath((char*)sTargetPath);
	iCount = (int)xvoArrayLength(items);

	for ( i = 0; i < iCount; i++ ) {
		xvalue tblItem = xvoArrayGetValue(items, i);
		if ( !tblItem || xvoType(tblItem) != XVO_DT_TABLE ) continue;
		str sSourcePath = xvoTableGetText(tblItem, "sourcePath", 10);
		if ( !sSourcePath || !FM_IsPathSafe((char*)sSourcePath) ) continue;

		char* fileName = strrchr((char*)sSourcePath, '/');
		fileName = fileName ? fileName + 1 : (char*)sSourcePath;
		str sSource = FM_ResolvePath((char*)sSourcePath);
		str sTargetFull = xrtPathJoin(2, sTarget, fileName);
		if ( sTargetFull ) FM_NormalizePath(sTargetFull);

		str sIsDirStr = xvoTableGetText(tblItem, "isdir", 5);
		bool isDir = sIsDirStr ? atoi((char*)sIsDirStr) : FALSE;

		if ( isDir ) {
			if ( isCut ) xrtDirMove(sSource, sTargetFull, TRUE);
			else xrtDirCopy(sSource, sTargetFull, TRUE);
		} else {
			if ( isCut ) xrtFileMove(sSource, sTargetFull, TRUE);
			else xrtFileCopy(sSource, sTargetFull, TRUE);
		}
		xrtFree(sSource);
		xrtFree(sTargetFull);
	}

	FM_SendJson(objResp, FM_Ok("operation completed"));
	xrtFree(sTarget);
	xvoUnref(tblForm);
}

void FM_Req_ApiContent(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	str sRelPath, sPath;
	str sContent;
	xvalue tblRet;

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

	sContent = xrtFileReadAll(sPath, XRT_CP_BINARY);
	xrtFree(sPath);
	if ( !sContent ) { FM_SendError(objResp, 500, "read failed"); return; }

	tblRet = FM_Ok("ok");
	xvoTableSetText(tblRet, "content", 7, sContent, 0, TRUE);
	FM_SendJson(objResp, tblRet);
}

void FM_Req_ApiSave(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm;
	str sRelPath, sContent, sPath;

	(void)objServer; (void)objHost; (void)objSession;

	tblForm = FM_ParseJsonBody(objReq);
	if ( !tblForm ) { FM_SendError(objResp, 400, "invalid json"); return; }

	sRelPath = xvoTableGetText(tblForm, "path", 4);
	sContent = xvoTableGetText(tblForm, "content", 7);
	if ( !sRelPath || !sContent || !FM_IsPathSafe((char*)sRelPath) ) {
		xvoUnref(tblForm);
		FM_SendError(objResp, 400, "invalid path or content");
		return;
	}

	sPath = FM_ResolvePath((char*)sRelPath);
	if ( !sPath || !xrtFileExists(sPath) || xrtDirExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		xvoUnref(tblForm);
		FM_SendError(objResp, 400, "file not found or is directory");
		return;
	}

	{
		size_t iSize = strlen((char*)sContent);
		size_t iWrite = xrtFileWriteAll(sPath, (str)sContent, iSize, XRT_CP_BINARY);
		xrtFree(sPath);
		xvoUnref(tblForm);
		FM_SendJson(objResp, iWrite == iSize ? FM_Ok("saved") : FM_Fail("write failed"));
	}
}

void FM_Req_ApiUpload(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	const char* sBody;
	size_t iBodyLen;
	const char* sContentType;
	xrtstrview tBoundary;
	size_t iOffset;
	str sRelPath, sUploadDir;
	xvalue tblRet;

	(void)objServer; (void)objHost; (void)objSession;

	sBody = xsReqBody(objReq);
	iBodyLen = xsReqBodyLen(objReq);
	sContentType = xsReqHeader(objReq, "Content-Type");

	if ( !sContentType || !xrtMultipartBoundaryFromContentType(sContentType, &tBoundary) ) {
		FM_SendError(objResp, 400, "invalid multipart request");
		return;
	}

	sRelPath = FM_ReadQuery(objReq, "path");
	sUploadDir = sRelPath ? FM_ResolvePath((char*)sRelPath) : xrtCopyStr(G_FMConfig.sRootPath, 0);
	if ( sRelPath ) xrtFree(sRelPath);

	if ( sUploadDir && !xrtDirExists(sUploadDir) ) {
		xrtDirCreate(sUploadDir);
	}

	iOffset = 0;
	tblRet = NULL;

	{
		xrtmultipartpartview part;
		memset(&part, 0, sizeof(part));
		while ( xrtMultipartNextN(sBody, iBodyLen, tBoundary.Ptr, tBoundary.Len, &iOffset, &part) ) {
			if ( part.tName.Len == 4 && part.tName.Ptr && memcmp(part.tName.Ptr, "file", 4) == 0 ) {
				char sFileName[256] = {0};
				size_t iNameLen = 0;
				if ( xrtMultipartDecodeFileNameTo(&part, sFileName, sizeof(sFileName), &iNameLen) && iNameLen > 0 ) {
					str sFilePath = xrtPathJoin(2, sUploadDir, sFileName);
					if ( sFilePath ) {
						xrtFilePutAll(sFilePath, (ptr)part.tBody.Ptr, part.tBody.Len);
						{
							xvalue r = FM_Ok("uploaded");
							xvoTableSetText(r, "name", 4, xrtCopyStr(sFileName, iNameLen), 0, TRUE);
							xvoTableSetText(r, "url", 3, xrtCopyStr(sFileName, iNameLen), 0, TRUE);
							if ( tblRet ) xvoUnref(tblRet);
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

void FM_Req_ApiDownload(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
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

	pData = xrtFileGetAll(sPath, &iSize);
	if ( !pData ) {
		xrtFree(sPath);
		FM_SendError(objResp, 500, "read failed");
		return;
	}

	sName = xrtPathGetNameExt(sPath, strlen(sPath));
	sContentType = FM_GetImageContentType(sPath);
	snprintf(sHeader, sizeof(sHeader),
		"Content-Type: %s\r\n"
		"Content-Disposition: attachment; filename=\"%s\"\r\n"
		"Content-Length: %llu\r\n",
		sContentType, sName ? sName : "download", (unsigned long long)iSize);

	http_reply(objResp, 200, sHeader, pData, iSize);

	xrtFree(pData);
	xrtFree(sPath);
	if ( sName ) xrtFree(sName);
}

void FM_Req_ApiImage(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
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

	pData = xrtFileGetAll(sPath, &iSize);
	if ( !pData ) {
		xrtFree(sPath);
		FM_SendError(objResp, 500, "read failed");
		return;
	}

	sContentType = FM_GetImageContentType(sPath);
	snprintf(sHeader, sizeof(sHeader),
		"Content-Type: %s\r\nCache-Control: max-age=86400\r\n", sContentType);

	http_reply(objResp, 200, sHeader, pData, iSize);
	xrtFree(pData);
	xrtFree(sPath);
}

void FM_Req_ApiThumbnail(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
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
	sFilename = sFilename ? sFilename + 1 : sPath;
	hash = FM_SimpleHash(sPath);

#if defined(_WIN32) || defined(_WIN64)
	snprintf(sThumbPath, sizeof(sThumbPath), "%s\\thumb_%08lx_%dx%d_%s",
		G_FMConfig.sThumbPath, hash, thumbSize, thumbSize, sFilename);
#else
	snprintf(sThumbPath, sizeof(sThumbPath), "%s/thumb_%08lx_%dx%d_%s",
		G_FMConfig.sThumbPath, hash, thumbSize, thumbSize, sFilename);
#endif

	needGen = TRUE;
	if ( xrtFileExists(sThumbPath) ) {
		xtime tOrig = xrtFileGetChangeTime(sPath);
		xtime tThumb = xrtFileGetChangeTime(sThumbPath);
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

	pData = xrtFileGetAll(sThumbPath, &iSize);
	if ( !pData ) {
		pData = xrtFileGetAll(sPath, &iSize);
		if ( !pData ) {
			xrtFree(sPath);
			FM_SendError(objResp, 500, "thumbnail failed");
			return;
		}
	}

	sContentType = FM_GetImageContentType(sPath);
	snprintf(sHeader, sizeof(sHeader),
		"Content-Type: %s\r\nCache-Control: max-age=86400\r\n", sContentType);

	http_reply(objResp, 200, sHeader, pData, iSize);
	xrtFree(pData);
	xrtFree(sPath);
}

void FM_Req_ApiCompress(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm, items;
	str sFormat, sFilename, sSavePath, sSourcePath;
	str sSrc, sSave, sArchivePath, sTempList;
	FILE* fp;
	int iCount, i, ret = 0;
	char sCmd[2048];
	char sArcName[256];

	(void)objServer; (void)objHost; (void)objSession;

	tblForm = FM_ParseJsonBody(objReq);
	if ( !tblForm ) { FM_SendError(objResp, 400, "invalid json"); return; }

	sFormat = xvoTableGetText(tblForm, "format", 6);
	sFilename = xvoTableGetText(tblForm, "filename", 8);
	sSavePath = xvoTableGetText(tblForm, "savePath", 8);
	sSourcePath = xvoTableGetText(tblForm, "sourcePath", 10);
	items = xvoTableGetValue(tblForm, "items", 5);

	if ( !sFormat || !sFilename || !sSavePath || !sSourcePath || !items ||
		xvoType(items) != XVO_DT_ARRAY ||
		!FM_IsPathSafe((char*)sSavePath) || !FM_IsPathSafe((char*)sSourcePath) ) {
		xvoUnref(tblForm);
		FM_SendError(objResp, 400, "invalid parameters");
		return;
	}

	sSrc = FM_ResolvePath((char*)sSourcePath);
	sSave = FM_ResolvePath((char*)sSavePath);
	snprintf(sArcName, sizeof(sArcName), "%s.%s", (char*)sFilename, (char*)sFormat);
	sArchivePath = xrtPathJoin(2, sSave, sArcName);

	if ( !xrtDirExists(sSave) ) xrtDirCreate(sSave);

	sTempList = xrtPathRandom(G_FMConfig.sTempPath, 0, "_filelist.txt", 4, 32);
	fp = fopen(sTempList, "w");
	if ( !fp ) {
		FM_SendError(objResp, 500, "cannot create temp file list");
		goto compress_cleanup;
	}

	iCount = (int)xvoArrayLength(items);
	for ( i = 0; i < iCount; i++ ) {
		xvalue tblItem = xvoArrayGetValue(items, i);
		if ( !tblItem || xvoType(tblItem) != XVO_DT_TABLE ) continue;
		str sName = xvoTableGetText(tblItem, "name", 4);
		if ( sName ) fprintf(fp, "%s\n", (char*)sName);
	}
	fclose(fp);

#if defined(_WIN32) || defined(_WIN64)
	SetCurrentDirectoryA(sSrc);
	if ( strcmp((char*)sFormat, "tar.gz") == 0 || strcmp((char*)sFormat, "tgz") == 0 ) {
		char sTempTar[MAX_PATH];
		snprintf(sTempTar, MAX_PATH, "%s\\temp_%d.tar", (char*)sSave, GetCurrentProcessId());
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
	chdir((char*)sSrc);
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
	xvoUnref(tblForm);
}

void FM_Req_ApiExtract(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm;
	str sArchivePath, sExtractPath;
	str sArc, sExtract;
	const char* ext;
	char sCmd[2048];
	int ret = 0;

	(void)objServer; (void)objHost; (void)objSession;

	tblForm = FM_ParseJsonBody(objReq);
	if ( !tblForm ) { FM_SendError(objResp, 400, "invalid json"); return; }

	sArchivePath = xvoTableGetText(tblForm, "archivePath", 11);
	sExtractPath = xvoTableGetText(tblForm, "extractPath", 11);
	if ( !sArchivePath || !sExtractPath ||
		!FM_IsPathSafe((char*)sArchivePath) || !FM_IsPathSafe((char*)sExtractPath) ) {
		xvoUnref(tblForm);
		FM_SendError(objResp, 400, "invalid parameters");
		return;
	}

	sArc = FM_ResolvePath((char*)sArchivePath);
	sExtract = FM_ResolvePath((char*)sExtractPath);
	xrtDirCreate(sExtract);

	ext = strrchr((char*)sArchivePath, '.');
	if ( ext ) ext++;

#if defined(_WIN32) || defined(_WIN64)
	if ( ext && (strcmp(ext, "gz") == 0 || strcmp(ext, "tgz") == 0) ) {
		snprintf(sCmd, sizeof(sCmd), "%s\\7z.exe x \"%s\" -o\"%s\" -y", (char*)G_FMConfig.sToolPath, (char*)sArc, (char*)sExtract);
		ret = system(sCmd);
	} else {
		snprintf(sCmd, sizeof(sCmd), "%s\\7z.exe x \"%s\" -o\"%s\" -y", (char*)G_FMConfig.sToolPath, (char*)sArc, (char*)sExtract);
		ret = system(sCmd);
	}
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
	xvoUnref(tblForm);
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
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/list";
	route.proc = FM_Req_ApiList;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/upload";
	route.proc = FM_Req_ApiUpload;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/download";
	route.proc = FM_Req_ApiDownload;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/create";
	route.proc = FM_Req_ApiCreate;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/delete";
	route.proc = FM_Req_ApiDelete;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/copy";
	route.proc = FM_Req_ApiCopy;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/content";
	route.proc = FM_Req_ApiContent;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/save";
	route.proc = FM_Req_ApiSave;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/image";
	route.proc = FM_Req_ApiImage;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/thumbnail";
	route.proc = FM_Req_ApiThumbnail;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/compress";
	route.proc = FM_Req_ApiCompress;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/filemanager/extract";
	route.proc = FM_Req_ApiExtract;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&menu, 0, sizeof(menu));
	menu.title = "File Manager";
	menu.icon = "layui-icon layui-icon-file";
	menu.type = 1;
	menu.open_type = "_iframe";
	menu.href = "/admin/view/plugin/filemanager";
	menu.sort = 400;
	menu.visible = TRUE;
	menu.remark = "File management";
	if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) return -1;

	printf("[filemanager] started\n");
	return 0;
}

int FM_OnConfigChanged(XAdminPluginHandle handle, xvalue new_cfg)
{
	(void)handle;

	memset(&G_FMConfig, 0, sizeof(G_FMConfig));

	if ( new_cfg && xvoType(new_cfg) == XVO_DT_TABLE ) {
		str sVal = xvoTableGetText(new_cfg, "rootPath", 8);
		if ( sVal && sVal[0] ) {
			strncpy(G_FMConfig.sRootPath, (char*)sVal, sizeof(G_FMConfig.sRootPath) - 1);
		}
		sVal = xvoTableGetText(new_cfg, "toolPath", 8);
		if ( sVal && sVal[0] ) {
			strncpy(G_FMConfig.sToolPath, (char*)sVal, sizeof(G_FMConfig.sToolPath) - 1);
		}
	}

	if ( G_FMConfig.sRootPath[0] == '\0' ) {
		strcpy(G_FMConfig.sRootPath, "data");
	}
	if ( G_FMConfig.sToolPath[0] == '\0' ) {
		strcpy(G_FMConfig.sToolPath, "tools");
	}

	if ( G_FMDataPath ) {
		str sTemp = xrtPathJoin(2, G_FMDataPath, "temp");
		strncpy(G_FMConfig.sTempPath, (char*)sTemp, sizeof(G_FMConfig.sTempPath) - 1);
		xrtFree(sTemp);
		sTemp = xrtPathJoin(2, G_FMDataPath, "thumbnail");
		strncpy(G_FMConfig.sThumbPath, (char*)sTemp, sizeof(G_FMConfig.sThumbPath) - 1);
		xrtFree(sTemp);
	}

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