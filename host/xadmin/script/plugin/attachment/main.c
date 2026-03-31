#include "plugin.h"

PluginContext* ctx;
str AttachmentPluginPath = NULL;
xdict G_HotlinkExact = NULL;
xdict G_HotlinkSuffix = NULL;
int g_iMenuId = 0;

typedef struct {
	str ext;
	str mime;
	int isInline;
} MimeMapping;

MimeMapping G_MimeTable[] = {
	{"jpg", "image/jpeg", 1},
	{"jpeg", "image/jpeg", 1},
	{"png", "image/png", 1},
	{"gif", "image/gif", 1},
	{"webp", "image/webp", 1},
	{"bmp", "image/bmp", 1},
	{"svg", "image/svg+xml", 1},
	{"ico", "image/x-icon", 1},
	{"txt", "text/plain; charset=utf-8", 1},
	{"html", "text/html; charset=utf-8", 1},
	{"htm", "text/html; charset=utf-8", 1},
	{"css", "text/css; charset=utf-8", 1},
	{"js", "application/javascript; charset=utf-8", 1},
	{"json", "application/json; charset=utf-8", 1},
	{"xml", "application/xml; charset=utf-8", 1},
	{"md", "text/markdown; charset=utf-8", 1},
	{"pdf", "application/pdf", 1},
	{"mp4", "video/mp4", 1},
	{"webm", "video/webm", 1},
	{"ogg", "video/ogg", 1},
	{"mp3", "audio/mpeg", 1},
	{"wav", "audio/wav", 1},
	{"flac", "audio/flac", 1},
	{"zip", "application/zip", 0},
	{"rar", "application/x-rar-compressed", 0},
	{"7z", "application/x-7z-compressed", 0},
	{"tar", "application/x-tar", 0},
	{"gz", "application/gzip", 0},
	{"doc", "application/msword", 0},
	{"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document", 0},
	{"xls", "application/vnd.ms-excel", 0},
	{"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", 0},
	{"ppt", "application/vnd.ms-powerpoint", 0},
	{"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation", 0},
	{NULL, NULL, 0}
};

void LoadHotlinkWhitelist();
void Plugin_attachment_Upload(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);
void Plugin_attachment_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);
void Plugin_attachment_Get(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);
void Plugin_attachment_Save(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);
void Plugin_attachment_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);
void Plugin_attachment_Stats(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);
void Plugin_attachment_Purchase(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);
void Plugin_attachment_MyList(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);
void Plugin_attachment_Purchased(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);
void Plugin_attachment_Access(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);
void Plugin_attachment_ViewList(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);

void Plugin_SetGlobalData(int idx, void* ptr)
{
	if ( idx == 1 ) {
		ctx = (PluginContext*)ptr;
	}
}

void Plugin_attachment_Init()
{
	ctx->Log(LOG_INFO, "Attachment Plugin initializing...");

	AttachmentPluginPath = xrtPathJoin(2, ctx->sDataPath, "uploads_plugin");
	xrtDirCreateAll(AttachmentPluginPath);

	LoadHotlinkWhitelist();

	ctx->AddRoute("/api/plugin/attachment/upload", Plugin_attachment_Upload, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/api/plugin/attachment/list", Plugin_attachment_List, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/api/plugin/attachment/get", Plugin_attachment_Get, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/api/plugin/attachment/save", Plugin_attachment_Save, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/api/plugin/attachment/delete", Plugin_attachment_Delete, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/api/plugin/attachment/stats", Plugin_attachment_Stats, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/api/plugin/attachment/purchase", Plugin_attachment_Purchase, FALSE, FALSE, 0, 0);
	ctx->AddRoute("/api/plugin/attachment/my", Plugin_attachment_MyList, TRUE, FALSE, 0, 0);
	ctx->AddRoute("/api/plugin/attachment/purchased", Plugin_attachment_Purchased, TRUE, FALSE, 0, 0);
	ctx->AddRoute("/plugin/attachment/access", Plugin_attachment_Access, FALSE, FALSE, 0, 0);
	ctx->AddRoute("/admin/view/plugin/attachment/list", Plugin_attachment_ViewList, TRUE, TRUE, 0, 0);

	int iAuthGroupId = ctx->AddAuthGroup("attachment.manage", "attachment upload, download, manage", 10);

	ctx->AddAuth(iAuthGroupId, "attachment.upload", "涓婁紶闄勪欢", 1);
	ctx->AddAuth(iAuthGroupId, "attachment.manage", "绠＄悊闄勪欢", 2);
	ctx->AddAuth(iAuthGroupId, "attachment.delete", "鍒犻櫎闄勪欢", 3);
	ctx->AddAuth(iAuthGroupId, "attachment.stats", "鏌ョ湅缁熻", 4);

	g_iMenuId = ctx->AddMenu(0, "闄勪欢绠＄悊", "layui-icon layui-icon-file", 1, "_component", "/admin/view/plugin/attachment/list", 10, TRUE);

	ctx->Log(LOG_INFO, "Attachment Plugin initialized!");
}

void Plugin_attachment_ViewList(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	LOAD_SEND_PAGE(objResp, "list.html");
}

void Plugin_attachment_Unit()
{
	ctx->Log(LOG_INFO, "Attachment Plugin unloaded!");

	if ( AttachmentPluginPath ) {
		xrtFree(AttachmentPluginPath);
		AttachmentPluginPath = NULL;
	}
	if ( G_HotlinkExact ) {
		xrtDictDestroy(G_HotlinkExact);
		G_HotlinkExact = NULL;
	}
	if ( G_HotlinkSuffix ) {
		xrtDictDestroy(G_HotlinkSuffix);
		G_HotlinkSuffix = NULL;
	}
}

MimeMapping* GetMime(str sExt)
{
	if ( !sExt ) return NULL;
	for ( int i = 0; G_MimeTable[i].ext != NULL; i++ ) {
		if ( xrtStrComp(sExt, G_MimeTable[i].ext, 0, FALSE) == 0 ) {
			return &G_MimeTable[i];
		}
	}
	return NULL;
}

void LoadHotlinkWhitelist()
{
	if ( G_HotlinkExact ) {
		xrtDictDestroy(G_HotlinkExact);
	}
	if ( G_HotlinkSuffix ) {
		xrtDictDestroy(G_HotlinkSuffix);
	}
	G_HotlinkExact = xrtDictCreate(0, XRT_OBJMODE_SHARED);
	G_HotlinkSuffix = xrtDictCreate(0, XRT_OBJMODE_SHARED);
	xrtOwnerActivateShared(&G_HotlinkExact->Owner);
	xrtOwnerActivateShared(&G_HotlinkExact->AVLT.Owner);
	xrtOwnerActivateShared(&G_HotlinkSuffix->Owner);
	xrtOwnerActivateShared(&G_HotlinkSuffix->AVLT.Owner);

	xvalue tblAttachment = ctx->GetOption("attachment", "attachment");
	if ( !tblAttachment || tblAttachment->Type != XVO_DT_TABLE ) return;
	str sWhitelist = xvoTableGetText(tblAttachment, "hotlinkWhitelist", 16);
	if ( !sWhitelist || !*sWhitelist ) return;
	str sCopy = xrtCopyStr(sWhitelist, 0);
	str sLine = strtok(sCopy, "\r\n");
	while ( sLine ) {
		while ( *sLine == ' ' || *sLine == '\t' ) sLine++;
		if ( *sLine ) {
			size_t iLen = strlen(sLine);
			while ( iLen > 0 && (sLine[iLen-1] == ' ' || sLine[iLen-1] == '\t') ) {
				iLen--;
			}
			if ( iLen > 0 ) {
				if ( sLine[0] == '*' && sLine[1] == '.' ) {
					xrtDictSet(G_HotlinkSuffix, sLine + 1, iLen - 1, (ptr)1);
				} else {
					xrtDictSet(G_HotlinkExact, sLine, iLen, (ptr)1);
				}
			}
		}
		sLine = strtok(NULL, "\r\n");
	}
	xrtFree(sCopy);
}

bool CheckHotlinkWhitelist(str sHost, size_t iLen)
{
	if ( !sHost || iLen == 0 ) return FALSE;
	if ( xrtDictGet(G_HotlinkExact, sHost, iLen) ) {
		return TRUE;
	}
	str p = sHost;
	while ( (p = strchr(p, '.')) != NULL ) {
		size_t iSuffixLen = iLen - (p - sHost);
		if ( xrtDictGet(G_HotlinkSuffix, p, iSuffixLen) ) {
			return TRUE;
		}
		p++;
	}
	return FALSE;
}

str ExtractHost(str sReferer, size_t* pLen)
{
	if ( !sReferer ) return NULL;
	str p = strstr(sReferer, "://");
	if ( p ) {
		p += 3;
	} else {
		p = sReferer;
	}
	str pEnd = p;
	while ( *pEnd && *pEnd != '/' && *pEnd != ':' && *pEnd != '?' ) {
		pEnd++;
	}
	*pLen = pEnd - p;
	return p;
}

bool CheckHotlink(XS_RequestObject objReq, bool bAllowHotlink)
{
	if ( bAllowHotlink ) return TRUE;
	const char* sReferer = xsReqHeader(objReq, "Referer");
	if ( sReferer == NULL || sReferer[0] == '\0' ) {
		return TRUE;
	}
	size_t iHostLen = 0;
	str sHost = ExtractHost((str)sReferer, &iHostLen);
	if ( !sHost || iHostLen == 0 ) return FALSE;
	if ( ((iHostLen == 9) && (strncmp(sHost, "localhost", 9) == 0)) ||
		 ((iHostLen == 9) && (strncmp(sHost, "127.0.0.1", 9) == 0)) ) {
		return TRUE;
	}
	return CheckHotlinkWhitelist(sHost, iHostLen);
}

bool IsExtAllowed(str sExt)
{
	if ( !sExt ) return FALSE;
	xvalue tblAttachment = ctx->GetOption("attachment", "attachment");
	if ( !tblAttachment ) return TRUE;
	str sAllowed = xvoTableGetText(tblAttachment, "allowedExts", 11);
	if ( !sAllowed || !*sAllowed ) return TRUE;
	str sCopy = xrtCopyStr(sAllowed, 0);
	str sToken = strtok(sCopy, ",");
	bool bFound = FALSE;
	while ( sToken ) {
		while ( *sToken == ' ' ) sToken++;
		if ( xrtStrComp(sToken, sExt, 0, FALSE) == 0 ) {
			bFound = TRUE;
			break;
		}
		sToken = strtok(NULL, ",");
	}
	xrtFree(sCopy);
	return bFound;
}

int64 GetMaxSize()
{
	xvalue tblAttachment = ctx->GetOption("attachment", "attachment");
	if ( !tblAttachment ) return 0;
	int64 iMaxMB = xvoTableGetInt(tblAttachment, "maxSize", 7);
	if ( iMaxMB <= 0 ) return 0;
	return iMaxMB * 1024 * 1024;
}

int64 GetUserQuota()
{
	xvalue tblAttachment = ctx->GetOption("attachment", "attachment");
	if ( !tblAttachment ) return 0;
	int64 iQuotaMB = xvoTableGetInt(tblAttachment, "userQuota", 9);
	if ( iQuotaMB <= 0 ) return 0;
	return iQuotaMB * 1024 * 1024;
}

int64 GetUserUsage(int64 iUploaderId, int iUploaderType)
{
	xvalue tblResult = ctx->QuerySQL(xrtFormat(
		"SELECT COALESCE(SUM(size), 0) FROM attachment WHERE uploaderId = %lld AND uploaderType = %d AND isDelete = 0",
		iUploaderId, iUploaderType));
	if ( !tblResult || tblResult->Type != XVO_DT_ARRAY ) return 0;
	xvalue arr = (xvalue)tblResult;
	if ( xvoArrayItemCount(arr) == 0 ) {
		xvoUnref(tblResult);
		return 0;
	}
	xvalue row = xvoArrayGetValue(arr, 0);
	int64 iUsage = xvoTableGetInt(row, "SUM(size)", 9);
	xvoUnref(tblResult);
	return iUsage;
}

str GeneratePath(str sModelName, str sXID, str sExt)
{
	xtime now = ctx->TimeNow();
	str sDate = xrtTimeToStr(now, XRT_TIME_FORMAT_DATE);
	char sYearMonth[8];
	memcpy(sYearMonth, sDate, 4);
	sYearMonth[4] = '/';
	memcpy(sYearMonth + 5, sDate + 5, 2);
	sYearMonth[7] = '\0';
	ctx->Free(sDate);
	str sSubDir = (sModelName && *sModelName) ? sModelName : (str)"global";
	str sPath = xrtFormat("%s/%s/%s.%s", sSubDir, sYearMonth, sXID, sExt);
	return sPath;
}

bool EnsureDir(str sPath)
{
	str sFullPath = xrtPathJoin(2, AttachmentPluginPath, sPath);
	str sDir = xrtPathGetDir(sFullPath, 0);
	bool bRet = xrtDirCreate(sDir);
	ctx->Free(sDir);
	ctx->Free(sFullPath);
	return bRet;
}

void Plugin_attachment_Upload(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	CHECK_METHOD_POST(objResp, hm);

	if ( !objSession || objSession->Type != XVO_DT_TABLE ) {
		SEND_JSON_ERR(objResp, "Please login first");
		return;
	}

	int64 iUploaderId = xvoTableGetInt(objSession, "id", 2);
	int iUploaderType = 1;

	HttpMultipartPart part;
	size_t ofs = 0;
	str sFilename = NULL;
	str sExt = NULL;
	str sModelName = NULL;
	int64 iRecordId = 0;
	ptr pFileData = NULL;
	size_t iFileSize = 0;
	int iAllowHotlink = 1;
	int iAccessType = 0;
	int iAccessLevel = 0;
	int64 iPrice = 0;
	int iPriceType = 0;

	while ( HttpMultipartNext(objReq, &ofs, &part) ) {
		if ( HttpMultipartNameIs(&part, "file") ) {
			pFileData = (ptr)part.pBody;
			iFileSize = part.iBodyLen;
			if ( part.iFileNameLen > 0 ) {
				sFilename = xrtCopyStr((str)part.sFileName, part.iFileNameLen);
				sExt = xrtPathGetExt(sFilename, 0);
				if ( sExt ) {
					for ( str p = sExt; *p; p++ ) {
						if ( *p >= 'A' && *p <= 'Z' ) *p += 32;
					}
				}
			}
		} else if ( HttpMultipartNameIs(&part, "modelName") ) {
			sModelName = xrtCopyStr((str)part.pBody, part.iBodyLen);
		} else if ( HttpMultipartNameIs(&part, "recordId") ) {
			char sTmp[24] = {0};
			size_t iLen = part.iBodyLen < 23 ? part.iBodyLen : 23;
			memcpy(sTmp, part.pBody, iLen);
			iRecordId = xrtStrToI64(sTmp);
		} else if ( HttpMultipartNameIs(&part, "allowHotlink") ) {
			iAllowHotlink = (part.iBodyLen > 0 && part.pBody[0] == '1') ? 1 : 0;
		} else if ( HttpMultipartNameIs(&part, "accessType") ) {
			char sTmp[8] = {0};
			size_t iLen = part.iBodyLen < 7 ? part.iBodyLen : 7;
			memcpy(sTmp, part.pBody, iLen);
			iAccessType = atoi(sTmp);
		} else if ( HttpMultipartNameIs(&part, "accessLevel") ) {
			char sTmp[8] = {0};
			size_t iLen = part.iBodyLen < 7 ? part.iBodyLen : 7;
			memcpy(sTmp, part.pBody, iLen);
			iAccessLevel = atoi(sTmp);
		} else if ( HttpMultipartNameIs(&part, "price") ) {
			char sTmp[24] = {0};
			size_t iLen = part.iBodyLen < 23 ? part.iBodyLen : 23;
			memcpy(sTmp, part.pBody, iLen);
			iPrice = xrtStrToI64(sTmp);
		} else if ( HttpMultipartNameIs(&part, "priceType") ) {
			char sTmp[8] = {0};
			size_t iLen = part.iBodyLen < 7 ? part.iBodyLen : 7;
			memcpy(sTmp, part.pBody, iLen);
			iPriceType = atoi(sTmp);
		}
	}

	if ( !pFileData || iFileSize == 0 || !sFilename || !sExt ) {
		if ( sFilename ) ctx->Free(sFilename);
		if ( sExt ) ctx->Free(sExt);
		if ( sModelName ) ctx->Free(sModelName);
		SEND_JSON_ERR(objResp, "No file uploaded");
		return;
	}

	if ( !IsExtAllowed(sExt) ) {
		ctx->Free(sFilename);
		ctx->Free(sExt);
		if ( sModelName ) ctx->Free(sModelName);
		SEND_JSON_ERR(objResp, "File type not allowed");
		return;
	}

	int64 iMaxSize = GetMaxSize();
	if ( iMaxSize > 0 && (int64)iFileSize > iMaxSize ) {
		ctx->Free(sFilename);
		ctx->Free(sExt);
		if ( sModelName ) ctx->Free(sModelName);
		SEND_JSON_ERR(objResp, "File too large");
		return;
	}

	int64 iQuota = GetUserQuota();
	if ( iQuota > 0 ) {
		int64 iUsage = GetUserUsage(iUploaderId, iUploaderType);
		if ( iUsage + (int64)iFileSize > iQuota ) {
			ctx->Free(sFilename);
			ctx->Free(sExt);
			if ( sModelName ) ctx->Free(sModelName);
			SEND_JSON_ERR(objResp, "Storage quota exceeded");
			return;
		}
	}

	str sXID = xrtMakeXIDS();
	str sPath = GeneratePath(sModelName, sXID, sExt);
	EnsureDir(sPath);

	str sFullPath = xrtPathJoin(2, AttachmentPluginPath, sPath);
	FILE* fp = fopen(sFullPath, "wb");
	if ( !fp ) {
		ctx->Free(sFullPath);
		ctx->Free(sPath);
		ctx->Free(sXID);
		ctx->Free(sFilename);
		ctx->Free(sExt);
		if ( sModelName ) ctx->Free(sModelName);
		SEND_JSON_ERR(objResp, "Failed to save file");
		return;
	}
	fwrite(pFileData, 1, iFileSize, fp);
	fclose(fp);
	ctx->Free(sFullPath);

	MimeMapping* pMime = GetMime(sExt);
	str sMime = pMime ? pMime->mime : (str)"application/octet-stream";

	str sSQL = xrtFormat(
		"INSERT INTO attachment (xid, filename, ext, mime, size, path, modelName, recordId, "
		"uploaderId, uploaderType, allowHotlink, accessType, accessLevel, price, priceType, createTime) "
		"VALUES ('%s', '%s', '%s', '%s', %lld, '%s', '%s', %lld, %lld, %d, %d, %d, %lld, %d, %lld)",
		sXID, sFilename, sExt, sMime, (int64)iFileSize, sPath,
		sModelName ? sModelName : (str)"", iRecordId, iUploaderId, iUploaderType,
		iAllowHotlink, iAccessType, iAccessLevel, iPrice, iPriceType, ctx->TimeNow());
	bool bOK = ctx->ExecuteSQL(sSQL);
	ctx->Free(sSQL);

	if ( bOK ) {
		str sJson = xrtFormat("{\"result\":true,\"data\":{\"xid\":\"%s\",\"filename\":\"%s\",\"ext\":\"%s\",\"size\":%lld,\"url\":\"/plugin/attachment/access?xid=%s\"}}",
							  sXID, sFilename, sExt, (int64)iFileSize, sXID);
		ctx->SendJson(objResp, 200, sJson, 0);
		ctx->Free(sJson);
	} else {
		SEND_JSON_ERR(objResp, "Failed to save record");
	}

	ctx->Free(sPath);
	ctx->Free(sXID);
	ctx->Free(sFilename);
	ctx->Free(sExt);
	if ( sModelName ) ctx->Free(sModelName);
}

void Plugin_attachment_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sPage[12], sLimit[12], sModelName[64], sExt[16], sAccessType[8];
	HttpGetQueryVar(objReq, "page", sPage, sizeof(sPage));
	HttpGetQueryVar(objReq, "limit", sLimit, sizeof(sLimit));
	HttpGetQueryVar(objReq, "modelName", sModelName, sizeof(sModelName));
	HttpGetQueryVar(objReq, "ext", sExt, sizeof(sExt));
	HttpGetQueryVar(objReq, "accessType", sAccessType, sizeof(sAccessType));

	int iPage = atoi(sPage);
	int iLimit = atoi(sLimit);
	if ( iPage < 1 ) iPage = 1;
	if ( iLimit < 1 ) iLimit = 20;
	if ( iLimit > 100 ) iLimit = 100;
	int iOffset = (iPage - 1) * iLimit;

	xbuffer_struct bufSQL;
	xrtBufferInit(&bufSQL, 256);
	xrtBufferAppend(&bufSQL, "SELECT xid, filename, ext, mime, size, modelName, recordId, "
		"uploaderId, uploaderType, allowHotlink, accessType, accessLevel, price, priceType, salesCount, downloadCount, createTime "
		"FROM attachment WHERE isDelete = 0", 0, XBUF_ANSI);
	if ( sModelName[0] ) {
		xrtBufferAppend(&bufSQL, " AND modelName = '", 0, XBUF_ANSI);
		xrtBufferAppend(&bufSQL, sModelName, 0, XBUF_ANSI);
		xrtBufferAppend(&bufSQL, "'", 0, XBUF_ANSI);
	}
	if ( sExt[0] ) {
		xrtBufferAppend(&bufSQL, " AND ext = '", 0, XBUF_ANSI);
		xrtBufferAppend(&bufSQL, sExt, 0, XBUF_ANSI);
		xrtBufferAppend(&bufSQL, "'", 0, XBUF_ANSI);
	}
	if ( sAccessType[0] ) {
		xrtBufferAppend(&bufSQL, " AND accessType = ", 0, XBUF_ANSI);
		xrtBufferAppend(&bufSQL, sAccessType, 0, XBUF_ANSI);
	}
	xrtBufferAppend(&bufSQL, " ORDER BY createTime DESC LIMIT ", 0, XBUF_ANSI);
	char sLimitStr[24];
	sprintf(sLimitStr, "%d OFFSET %d", iLimit, iOffset);
	xrtBufferAppend(&bufSQL, sLimitStr, 0, XBUF_ANSI);

	xvalue arrData = ctx->QuerySQL(bufSQL.Buffer);
	xrtBufferUnit(&bufSQL);

	if ( !arrData ) {
		SEND_JSON_ERR(objResp, "Query failed");
		return;
	}

	str sModelClause = sModelName[0] ? xrtFormat(" AND modelName = '%s'", sModelName) : (str)"";
	str sExtClause = sExt[0] ? xrtFormat(" AND ext = '%s'", sExt) : (str)"";
	str sAccessTypeClause = sAccessType[0] ? xrtFormat(" AND accessType = %s", sAccessType) : (str)"";
	xvalue tblCount = ctx->QuerySQL(xrtFormat(
		"SELECT COUNT(*) as count FROM attachment WHERE isDelete = 0%s%s%s",
		sModelClause, sExtClause, sAccessTypeClause));

	int64 iCount = 0;
	if ( tblCount && tblCount->Type == XVO_DT_ARRAY && xvoArrayItemCount(tblCount) > 0 ) {
		xvalue row = xvoArrayGetValue(tblCount, 0);
		iCount = xvoTableGetInt(row, "count", 5);
	}

	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);

	size_t iSize = 0;
	str sJson = ctx->JsonStringify(tblRet, &iSize);
	ctx->SendJson(objResp, 200, sJson, iSize);
	ctx->Free(sJson);
	xvoUnref(tblRet);
	if ( tblCount ) xvoUnref(tblCount);
}

void Plugin_attachment_Get(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sXID[48];
	HttpGetQueryVar(objReq, "xid", sXID, sizeof(sXID));
	if ( !sXID[0] ) {
		SEND_JSON_ERR(objResp, "Missing xid");
		return;
	}

	xvalue arrResult = ctx->QuerySQL(xrtFormat(
		"SELECT xid, filename, ext, mime, size, path, modelName, recordId, uploaderId, uploaderType, "
		"allowHotlink, accessType, accessLevel, price, priceType, salesCount, downloadCount, remark, createTime "
		"FROM attachment WHERE xid = '%s' AND isDelete = 0", sXID));

	if ( !arrResult || arrResult->Type != XVO_DT_ARRAY || xvoArrayItemCount(arrResult) == 0 ) {
		if ( arrResult ) xvoUnref(arrResult);
		SEND_JSON_ERR(objResp, "Not found");
		return;
	}

	xvalue tblData = xvoArrayGetValue(arrResult, 0);
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);

	size_t iSize = 0;
	str sJson = ctx->JsonStringify(tblRet, &iSize);
	ctx->SendJson(objResp, 200, sJson, iSize);
	ctx->Free(sJson);
	xvoUnref(tblRet);
	xvoUnref(arrResult);
}

void Plugin_attachment_Save(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	CHECK_METHOD_POST(objResp, hm);

	xvalue tblForm = ctx->JsonParse((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		if ( tblForm ) xvoUnref(tblForm);
		SEND_JSON_ERR(objResp, "Invalid data");
		return;
	}

	str sXID = xvoTableGetText(tblForm, "xid", 3);
	if ( !sXID || !*sXID ) {
		xvoUnref(tblForm);
		SEND_JSON_ERR(objResp, "Missing xid");
		return;
	}

	str sSQL = xrtFormat(
		"UPDATE attachment SET allowHotlink=%d, accessType=%d, accessLevel=%d, price=%lld, priceType=%d, remark='%s' WHERE xid='%s'",
		xvoTableGetBool(tblForm, "allowHotlink", 12) ? 1 : 0,
		(int)xvoTableGetInt(tblForm, "accessType", 10),
		(int)xvoTableGetInt(tblForm, "accessLevel", 11),
		xvoTableGetInt(tblForm, "price", 5),
		(int)xvoTableGetInt(tblForm, "priceType", 9),
		xvoTableGetText(tblForm, "remark", 6) ? xvoTableGetText(tblForm, "remark", 6) : (str)"",
		sXID);
	bool bOK = ctx->ExecuteSQL(sSQL);
	ctx->Free(sSQL);
	xvoUnref(tblForm);

	if ( bOK ) {
		SEND_JSON_OK(objResp, "淇濆瓨鎴愬姛");
	} else {
		SEND_JSON_ERR(objResp, "淇濆瓨澶辫触");
	}
}

void Plugin_attachment_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sXID[48];
	HttpGetQueryVar(objReq, "xid", sXID, sizeof(sXID));
	if ( !sXID[0] ) {
		SEND_JSON_ERR(objResp, "Missing xid");
		return;
	}

	xvalue arrResult = ctx->QuerySQL(xrtFormat("SELECT path FROM attachment WHERE xid = '%s' AND isDelete = 0", sXID));
	if ( arrResult && arrResult->Type == XVO_DT_ARRAY && xvoArrayItemCount(arrResult) > 0 ) {
		xvalue tblRow = xvoArrayGetValue(arrResult, 0);
		str sPath = xvoTableGetText(tblRow, "path", 4);
		if ( sPath ) {
			str sFullPath = xrtPathJoin(2, AttachmentPluginPath, sPath);
			xrtFileDelete(sFullPath);
			ctx->Free(sFullPath);
		}
	}
	if ( arrResult ) xvoUnref(arrResult);

	str sSQL = xrtFormat("UPDATE attachment SET isDelete = 1 WHERE xid = '%s'", sXID);
	bool bOK = ctx->ExecuteSQL(sSQL);
	ctx->Free(sSQL);

	if ( bOK ) {
		SEND_JSON_OK(objResp, "鍒犻櫎鎴愬姛");
	} else {
		SEND_JSON_ERR(objResp, "鍒犻櫎澶辫触");
	}
}

void Plugin_attachment_Stats(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue arrTotal = ctx->QuerySQL("SELECT COUNT(*) as count, COALESCE(SUM(size), 0) as size FROM attachment WHERE isDelete = 0");
	xvalue arrByModel = ctx->QuerySQL("SELECT modelName, COUNT(*) as count, COALESCE(SUM(size), 0) as size FROM attachment WHERE isDelete = 0 GROUP BY modelName");
	xvalue arrByExt = ctx->QuerySQL("SELECT ext, COUNT(*) as count, COALESCE(SUM(size), 0) as size FROM attachment WHERE isDelete = 0 GROUP BY ext ORDER BY SUM(size) DESC");

	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvalue tblData = xvoCreateTable();

	if ( arrTotal && arrTotal->Type == XVO_DT_ARRAY && xvoArrayItemCount(arrTotal) > 0 ) {
		xvoTableSetValue(tblData, "total", 5, xvoArrayGetValue(arrTotal, 0), TRUE);
	}
	if ( arrByModel ) {
		xvoTableSetValue(tblData, "byModel", 7, arrByModel, TRUE);
	}
	if ( arrByExt ) {
		xvoTableSetValue(tblData, "byExt", 5, arrByExt, TRUE);
	}

	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);

	size_t iSize = 0;
	str sJson = ctx->JsonStringify(tblRet, &iSize);
	ctx->SendJson(objResp, 200, sJson, iSize);
	ctx->Free(sJson);
	xvoUnref(tblRet);
	if ( arrTotal ) xvoUnref(arrTotal);
	if ( arrByModel ) xvoUnref(arrByModel);
	if ( arrByExt ) xvoUnref(arrByExt);
}

void Plugin_attachment_Purchase(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	CHECK_METHOD_POST(objResp, hm);

	if ( !objSession || objSession->Type != XVO_DT_TABLE ) {
		SEND_JSON_ERR(objResp, "Please login first");
		return;
	}

	int64 iMemberId = xvoTableGetInt(objSession, "id", 2);

	xvalue tblForm = ctx->JsonParse((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		if ( tblForm ) xvoUnref(tblForm);
		SEND_JSON_ERR(objResp, "Invalid data");
		return;
	}

	str sXID = xvoTableGetText(tblForm, "xid", 3);
	if ( !sXID || !*sXID ) {
		xvoUnref(tblForm);
		SEND_JSON_ERR(objResp, "Missing xid");
		return;
	}

	xvalue arrResult = ctx->QuerySQL(xrtFormat(
		"SELECT uploaderId, uploaderType, accessType, price, priceType FROM attachment WHERE xid = '%s' AND isDelete = 0", sXID));

	if ( !arrResult || arrResult->Type != XVO_DT_ARRAY || xvoArrayItemCount(arrResult) == 0 ) {
		if ( arrResult ) xvoUnref(arrResult);
		xvoUnref(tblForm);
		SEND_JSON_ERR(objResp, "Attachment not found");
		return;
	}

	xvalue tblRow = xvoArrayGetValue(arrResult, 0);
	int64 iUploaderId = xvoTableGetInt(tblRow, "uploaderId", 10);
	int iUploaderType = xvoTableGetInt(tblRow, "uploaderType", 12);
	int iAccessType = xvoTableGetInt(tblRow, "accessType", 10);
	int64 iPrice = xvoTableGetInt(tblRow, "price", 5);
	int iPriceType = xvoTableGetInt(tblRow, "priceType", 9);

	xvoUnref(arrResult);

	if ( iAccessType != 2 ) {
		xvoUnref(tblForm);
		SEND_JSON_ERR(objResp, "Attachment is free");
		return;
	}

	if ( iUploaderType == 2 && iUploaderId == iMemberId ) {
		xvoUnref(tblForm);
		SEND_JSON_ERR(objResp, "Cannot purchase own attachment");
		return;
	}

	xvalue arrCheck = ctx->QuerySQL(xrtFormat(
		"SELECT 1 FROM attachmentOrder WHERE attachmentXid = '%s' AND memberId = %lld", sXID, iMemberId));
	bool bPurchased = arrCheck && arrCheck->Type == XVO_DT_ARRAY && xvoArrayItemCount(arrCheck) > 0;
	if ( arrCheck ) xvoUnref(arrCheck);

	if ( bPurchased ) {
		xvoUnref(tblForm);
		SEND_JSON_OK(objResp, "Already purchased");
		return;
	}

	if ( iPriceType == 0 ) {
		xvalue arrBalance = ctx->QuerySQL(xrtFormat("SELECT balance FROM member WHERE id = %lld", iMemberId));
		int64 iBalance = 0;
		if ( arrBalance && arrBalance->Type == XVO_DT_ARRAY && xvoArrayItemCount(arrBalance) > 0 ) {
			xvalue row = xvoArrayGetValue(arrBalance, 0);
			iBalance = xvoTableGetInt(row, "balance", 7);
		}
		if ( arrBalance ) xvoUnref(arrBalance);

		if ( iBalance < iPrice ) {
			xvoUnref(tblForm);
			SEND_JSON_ERR(objResp, "Insufficient balance");
			return;
		}

		xvalue tblAttachment = ctx->GetOption("attachment", "attachment");
		int iPlatformFeeRate = tblAttachment ? (int)xvoTableGetInt(tblAttachment, "platformFeeRate", 15) : 10;
		int64 iSellerIncome = iPrice * (100 - iPlatformFeeRate) / 100;

		ctx->ExecuteSQL("BEGIN TRANSACTION");
		ctx->ExecuteSQL(xrtFormat("UPDATE member SET balance = balance - %lld WHERE id = %lld", iPrice, iMemberId));
		if ( iUploaderType == 2 && iSellerIncome > 0 ) {
			ctx->ExecuteSQL(xrtFormat("UPDATE member SET balance = balance + %lld WHERE id = %lld", iSellerIncome, iUploaderId));
		}
		ctx->ExecuteSQL(xrtFormat(
			"INSERT INTO attachmentOrder (attachmentXid, memberId, price, priceType, sellerId, sellerIncome, createTime) "
			"VALUES ('%s', %lld, %lld, %d, %lld, %lld, %lld)",
			sXID, iMemberId, iPrice, iPriceType,
			(iUploaderType == 2) ? iUploaderId : 0, iSellerIncome, ctx->TimeNow()));
		ctx->ExecuteSQL(xrtFormat("UPDATE attachment SET salesCount = salesCount + 1 WHERE xid = '%s'", sXID));
		ctx->ExecuteSQL("COMMIT");

		SEND_JSON_OK(objResp, "Purchase successful");
	} else {
		xvoUnref(tblForm);
		SEND_JSON_ERR(objResp, "Currency type not supported");
		return;
	}
	xvoUnref(tblForm);
}

void Plugin_attachment_MyList(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !objSession || objSession->Type != XVO_DT_TABLE ) {
		SEND_JSON_ERR(objResp, "Please login first");
		return;
	}

	int64 iMemberId = xvoTableGetInt(objSession, "id", 2);

	char sPage[12], sLimit[12];
	HttpGetQueryVar(objReq, "page", sPage, sizeof(sPage));
	HttpGetQueryVar(objReq, "limit", sLimit, sizeof(sLimit));
	int iPage = atoi(sPage);
	int iLimit = atoi(sLimit);
	if ( iPage < 1 ) iPage = 1;
	if ( iLimit < 1 ) iLimit = 20;
	if ( iLimit > 100 ) iLimit = 100;
	int iOffset = (iPage - 1) * iLimit;

	xvalue arrData = ctx->QuerySQL(xrtFormat(
		"SELECT xid, filename, ext, size, accessType, price, priceType, salesCount, downloadCount, createTime "
		"FROM attachment WHERE uploaderId = %lld AND uploaderType = 2 AND isDelete = 0 "
		"ORDER BY createTime DESC LIMIT %d OFFSET %d",
		iMemberId, iLimit, iOffset));

	xvalue arrCount = ctx->QuerySQL(xrtFormat(
		"SELECT COUNT(*) as count FROM attachment WHERE uploaderId = %lld AND uploaderType = 2 AND isDelete = 0", iMemberId));

	int64 iCount = 0;
	if ( arrCount && arrCount->Type == XVO_DT_ARRAY && xvoArrayItemCount(arrCount) > 0 ) {
		xvalue row = xvoArrayGetValue(arrCount, 0);
		iCount = xvoTableGetInt(row, "count", 5);
	}

	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);

	size_t iSize = 0;
	str sJson = ctx->JsonStringify(tblRet, &iSize);
	ctx->SendJson(objResp, 200, sJson, iSize);
	ctx->Free(sJson);
	xvoUnref(tblRet);
	if ( arrCount ) xvoUnref(arrCount);
}

void Plugin_attachment_Purchased(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !objSession || objSession->Type != XVO_DT_TABLE ) {
		SEND_JSON_ERR(objResp, "Please login first");
		return;
	}

	int64 iMemberId = xvoTableGetInt(objSession, "id", 2);

	char sPage[12], sLimit[12];
	HttpGetQueryVar(objReq, "page", sPage, sizeof(sPage));
	HttpGetQueryVar(objReq, "limit", sLimit, sizeof(sLimit));
	int iPage = atoi(sPage);
	int iLimit = atoi(sLimit);
	if ( iPage < 1 ) iPage = 1;
	if ( iLimit < 1 ) iLimit = 20;
	if ( iLimit > 100 ) iLimit = 100;
	int iOffset = (iPage - 1) * iLimit;

	xvalue arrData = ctx->QuerySQL(xrtFormat(
		"SELECT o.attachmentXid, a.filename, a.ext, a.size, o.price, o.priceType, o.createTime "
		"FROM attachmentOrder o LEFT JOIN attachment a ON o.attachmentXid = a.xid "
		"WHERE o.memberId = %lld ORDER BY o.createTime DESC LIMIT %d OFFSET %d",
		iMemberId, iLimit, iOffset));

	xvalue arrCount = ctx->QuerySQL(xrtFormat("SELECT COUNT(*) as count FROM attachmentOrder WHERE memberId = %lld", iMemberId));

	int64 iCount = 0;
	if ( arrCount && arrCount->Type == XVO_DT_ARRAY && xvoArrayItemCount(arrCount) > 0 ) {
		xvalue row = xvoArrayGetValue(arrCount, 0);
		iCount = xvoTableGetInt(row, "count", 5);
	}

	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);

	size_t iSize = 0;
	str sJson = ctx->JsonStringify(tblRet, &iSize);
	ctx->SendJson(objResp, 200, sJson, iSize);
	ctx->Free(sJson);
	xvoUnref(tblRet);
	if ( arrCount ) xvoUnref(arrCount);
}

void Plugin_attachment_Access(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sXID[64];
	int iLen = HttpGetQueryVar(objReq, "xid", sXID, sizeof(sXID));
	if ( iLen <= 0 ) {
		mg_http_reply(objResp, 400, HTTP_CT_JSON, "{\"error\":\"Missing xid parameter\"}");
		return;
	}

	xvalue arrResult = ctx->QuerySQL(xrtFormat(
		"SELECT filename, ext, mime, path, uploaderId, uploaderType, allowHotlink, "
		"accessType, accessLevel, price, priceType FROM attachment WHERE xid = '%s' AND isDelete = 0", sXID));

	if ( !arrResult || arrResult->Type != XVO_DT_ARRAY || xvoArrayItemCount(arrResult) == 0 ) {
		if ( arrResult ) xvoUnref(arrResult);
		mg_http_reply(objResp, 404, HTTP_CT_JSON, "{\"error\":\"Not found\"}");
		return;
	}

	xvalue tblRow = xvoArrayGetValue(arrResult, 0);
	str sFilename = xvoTableGetText(tblRow, "filename", 8);
	str sExt = xvoTableGetText(tblRow, "ext", 3);
	str sMime = xvoTableGetText(tblRow, "mime", 4);
	str sPath = xvoTableGetText(tblRow, "path", 4);
	int64 iUploaderId = xvoTableGetInt(tblRow, "uploaderId", 10);
	int iUploaderType = xvoTableGetInt(tblRow, "uploaderType", 12);
	int iAllowHotlink = xvoTableGetInt(tblRow, "allowHotlink", 12);
	int iAccessType = xvoTableGetInt(tblRow, "accessType", 10);
	int iAccessLevel = xvoTableGetInt(tblRow, "accessLevel", 11);
	int64 iPrice = xvoTableGetInt(tblRow, "price", 5);
	int iPriceType = xvoTableGetInt(tblRow, "priceType", 9);

	xvoUnref(arrResult);

	if ( !CheckHotlink(objReq, iAllowHotlink) ) {
		mg_http_reply(objResp, 403, HTTP_CT_JSON, "{\"error\":\"Hotlink not allowed\"}");
		return;
	}

	if ( iAccessType == 1 ) {
		if ( !objSession || objSession->Type != XVO_DT_TABLE ) {
			mg_http_reply(objResp, 401, HTTP_CT_JSON, "{\"error\":\"Login required\"}");
			return;
		}
	} else if ( iAccessType == 2 ) {
		if ( !objSession || objSession->Type != XVO_DT_TABLE ) {
			mg_http_reply(objResp, 401, HTTP_CT_JSON, "{\"error\":\"Login required\"}");
			return;
		}

		int64 iMemberId = xvoTableGetInt(objSession, "id", 2);
		bool bIsOwner = (iUploaderType == 2 && iUploaderId == iMemberId);

		if ( !bIsOwner ) {
			xvalue arrCheck = ctx->QuerySQL(xrtFormat(
				"SELECT 1 FROM attachmentOrder WHERE attachmentXid = '%s' AND memberId = %lld", sXID, iMemberId));
			bool bPurchased = arrCheck && arrCheck->Type == XVO_DT_ARRAY && xvoArrayItemCount(arrCheck) > 0;
			if ( arrCheck ) xvoUnref(arrCheck);

			if ( !bPurchased ) {
				str sJson = xrtFormat("{\"error\":\"Payment required\",\"price\":%lld,\"priceType\":%d}", iPrice, iPriceType);
				mg_http_reply(objResp, 402, HTTP_CT_JSON, sJson);
				ctx->Free(sJson);
				return;
			}
		}
	} else if ( iAccessType == 3 ) {
		if ( !objSession || objSession->Type != XVO_DT_TABLE ) {
			mg_http_reply(objResp, 401, HTTP_CT_JSON, "{\"error\":\"Login required\"}");
			return;
		}
		int iUserLevel = xvoTableGetInt(objSession, "authLevel", 9);

		if ( iUserLevel < iAccessLevel ) {
			str sJson = xrtFormat("{\"error\":\"Insufficient permission level\",\"required\":%d,\"current\":%d}", iAccessLevel, iUserLevel);
			mg_http_reply(objResp, 403, HTTP_CT_JSON, sJson);
			ctx->Free(sJson);
			return;
		}
	}

	str sFullPath = xrtPathJoin(2, AttachmentPluginPath, sPath);
	if ( !xrtFileExists(sFullPath) ) {
		ctx->Free(sFullPath);
		mg_http_reply(objResp, 404, HTTP_CT_JSON, "{\"error\":\"File not found\"}");
		return;
	}

	MimeMapping* pMime = GetMime(sExt);
	int bIsInline = pMime ? pMime->isInline : 0;

	if ( !bIsInline ) {
		ctx->ExecuteSQL(xrtFormat("UPDATE attachment SET downloadCount = downloadCount + 1 WHERE xid = '%s'", sXID));
	}

	char sHeader[512];
	if ( bIsInline ) {
		sprintf(sHeader, "Content-Type: %s\r\n", sMime);
	} else {
		sprintf(sHeader, "Content-Type: %s\r\nContent-Disposition: attachment; filename=\"%s\"\r\n", sMime, sFilename);
	}

	size_t iFileSize = 0;
	str sFileData = xrtFileGetAll(sFullPath, &iFileSize);
	if ( sFileData == NULL ) {
		ctx->Free(sFullPath);
		mg_http_reply(objResp, 500, HTTP_CT_JSON, "{\"error\":\"Failed to read file\"}");
		return;
	}

	http_reply(objResp, 200, sHeader, sFileData, iFileSize);
	xrtFree(sFileData);
	ctx->Free(sFullPath);
}

