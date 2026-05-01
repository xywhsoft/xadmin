#include "xs_plugin.h"

typedef struct {
	int iPageSize;
} ManagedConfigState;

typedef struct {
	xvalue arrProviders;
	str sCurrentPluginXid;
} ManagedProviderScanContext;

static XAdminPluginHandle G_Handle = NULL;
static const char* G_RootPath = NULL;
static const char* G_PrivateDbPath = NULL;
static ManagedConfigState G_Config = {
	20
};

static const char* G_SchemaSql =
	"CREATE TABLE IF NOT EXISTS content_item ("
	"id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"title TEXT NOT NULL DEFAULT '',"
	"status INTEGER NOT NULL DEFAULT 0,"
	"payload_json TEXT NOT NULL DEFAULT '{}',"
	"category_id INTEGER NOT NULL DEFAULT 0,"
	"is_draft INTEGER NOT NULL DEFAULT 0,"
	"create_time INTEGER NOT NULL,"
	"update_time INTEGER NOT NULL DEFAULT 0,"
	"delete_time INTEGER NOT NULL DEFAULT 0"
	");"
	"CREATE TABLE IF NOT EXISTS content_category ("
	"id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"parent_id INTEGER NOT NULL DEFAULT 0,"
	"title TEXT NOT NULL DEFAULT '',"
	"slug TEXT NOT NULL DEFAULT '',"
	"path TEXT NOT NULL DEFAULT '',"
	"level INTEGER NOT NULL DEFAULT 0,"
	"sort INTEGER NOT NULL DEFAULT 0,"
	"status INTEGER NOT NULL DEFAULT 1,"
	"seo_title TEXT NOT NULL DEFAULT '',"
	"seo_keywords TEXT NOT NULL DEFAULT '',"
	"seo_description TEXT NOT NULL DEFAULT '',"
	"create_time INTEGER NOT NULL DEFAULT 0,"
	"update_time INTEGER NOT NULL DEFAULT 0,"
	"delete_time INTEGER NOT NULL DEFAULT 0"
	");"
	"CREATE INDEX IF NOT EXISTS idx_content_item_public ON content_item(delete_time, is_draft, status, update_time DESC);"
	"CREATE INDEX IF NOT EXISTS idx_content_item_admin ON content_item(delete_time, update_time DESC);"
	"CREATE INDEX IF NOT EXISTS idx_content_category_parent_sort ON content_category(parent_id, sort, id);"
	"CREATE UNIQUE INDEX IF NOT EXISTS idx_content_category_parent_slug ON content_category(parent_id, slug) WHERE delete_time = 0;";

static const char* G_PostMigrationIndexSql =
	"CREATE INDEX IF NOT EXISTS idx_content_item_category_status ON content_item(category_id, status, is_draft, delete_time);";

XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int idx, void* ptr)
{
	if ( idx == XADMIN_GLOBAL_PLUGIN_ROOT_PATH ) {
		G_RootPath = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_PRIVATE_DB_PATH ) {
		G_PrivateDbPath = (const char*)ptr;
	}
}

void Managed_ConfigReset(void)
{
	G_Config.iPageSize = 20;
}

bool Managed_IsBlank(const char* sText)
{
	const unsigned char* p = (const unsigned char*)sText;

	if ( p == NULL ) {
		return TRUE;
	}
	while ( *p ) {
		if ( (*p != ' ') && (*p != '\t') && (*p != '\r') && (*p != '\n') ) {
			return FALSE;
		}
		p++;
	}
	return TRUE;
}

int Managed_ReadIntQuery(XS_RequestObject objReq, const char* sName, int iDefault)
{
	char sValue[32];

	memset(sValue, 0, sizeof(sValue));
	xsReqQueryValue(objReq, sName, sValue, sizeof(sValue));
	if ( sValue[0] == '\0' ) {
		return iDefault;
	}
	return atoi(sValue);
}

void Managed_ReadTextQuery(XS_RequestObject objReq, const char* sName, char* sBuf, int iBufSize)
{
	if ( (sBuf == NULL) || (iBufSize <= 0) ) {
		return;
	}
	memset(sBuf, 0, (size_t)iBufSize);
	if ( (objReq == NULL) || Managed_IsBlank(sName) ) {
		return;
	}
	xsReqQueryValue(objReq, sName, sBuf, iBufSize);
}

char Managed_ToLowerAscii(char c)
{
	return ((c >= 'A') && (c <= 'Z')) ? (char)(c + ('a' - 'A')) : c;
}

bool Managed_TextContainsIgnoreCase(const char* sText, const char* sNeedle)
{
	size_t iTextLen;
	size_t iNeedleLen;

	if ( Managed_IsBlank(sNeedle) ) {
		return TRUE;
	}
	if ( Managed_IsBlank(sText) ) {
		return FALSE;
	}
	iTextLen = strlen(sText);
	iNeedleLen = strlen(sNeedle);
	if ( iNeedleLen > iTextLen ) {
		return FALSE;
	}
	for ( size_t i = 0; i <= (iTextLen - iNeedleLen); i++ ) {
		size_t j = 0;
		while ( j < iNeedleLen ) {
			if ( Managed_ToLowerAscii(sText[i + j]) != Managed_ToLowerAscii(sNeedle[j]) ) {
				break;
			}
			j++;
		}
		if ( j == iNeedleLen ) {
			return TRUE;
		}
	}
	return FALSE;
}

bool Managed_OpenDb(sqlite3** ppDb)
{
	sqlite3* pDb = NULL;
	int iRet;

	if ( ppDb ) {
		*ppDb = NULL;
	}
	if ( (ppDb == NULL) || (G_PrivateDbPath == NULL) || (G_PrivateDbPath[0] == '\0') ) {
		return FALSE;
	}
	iRet = sqlite3_open_v2(
		G_PrivateDbPath,
		&pDb,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
		NULL);
	if ( iRet != SQLITE_OK ) {
		if ( pDb ) sqlite3_close(pDb);
		return FALSE;
	}
	sqlite3_busy_timeout(pDb, 3000);
	*ppDb = pDb;
	return TRUE;
}

void Managed_CloseDb(sqlite3* pDb)
{
	if ( pDb ) {
		sqlite3_close(pDb);
	}
}

bool Managed_ExecSql(sqlite3* pDb, const char* sSql)
{
	char* sError = NULL;
	int iRet;

	if ( (pDb == NULL) || (sSql == NULL) ) {
		return FALSE;
	}
	iRet = sqlite3_exec(pDb, sSql, NULL, NULL, &sError);
	if ( iRet != SQLITE_OK ) {
		printf("        [ManagedPlugin] sqlite exec failed: xid={{PLUGIN_XID}} code=%d error=%s\n",
			iRet,
			sError ? sError : "(null)");
	}
	if ( sError ) sqlite3_free(sError);
	return iRet == SQLITE_OK;
}

bool Managed_TableColumnExists(sqlite3* pDb, const char* sTable, const char* sColumn)
{
	sqlite3_stmt* stmt = NULL;
	str sSql = NULL;
	bool bExists = FALSE;

	if ( (pDb == NULL) || Managed_IsBlank(sTable) || Managed_IsBlank(sColumn) ) {
		return FALSE;
	}
	sSql = xrtFormat("PRAGMA table_info(%s)", sTable);
	if ( sSql == NULL ) {
		return FALSE;
	}
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			const char* sName = (const char*)sqlite3_column_text(stmt, 1);
			if ( (sName != NULL) && (strcmp(sName, sColumn) == 0) ) {
				bExists = TRUE;
				break;
			}
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	xrtFree(sSql);
	return bExists;
}

bool Managed_EnsureSchema(void)
{
	sqlite3* pDb = NULL;
	bool bOK = FALSE;

	if ( !Managed_OpenDb(&pDb) ) {
		return FALSE;
	}
	bOK = Managed_ExecSql(pDb, G_SchemaSql);
	if ( bOK && !Managed_TableColumnExists(pDb, "content_item", "category_id") ) {
		bOK = Managed_ExecSql(pDb, "ALTER TABLE content_item ADD COLUMN category_id INTEGER NOT NULL DEFAULT 0");
	}
	if ( bOK ) {
		bOK = Managed_ExecSql(pDb, G_PostMigrationIndexSql);
	}
	Managed_CloseDb(pDb);
	return bOK;
}

xvalue Managed_CreateResult(bool bResult, const char* sMessage)
{
	xvalue tblRet = xvoCreateTable();

	if ( tblRet == NULL ) {
		return NULL;
	}
	xvoTableSetBool(tblRet, "result", 6, bResult);
	if ( sMessage ) {
		xvoTableSetText(tblRet, "message", 7, (str)sMessage, 0, FALSE);
	}
	return tblRet;
}

void Managed_SendJsonValue(XS_ResponseObject objResp, xvalue objValue)
{
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(objValue, FALSE, &iSize);

	if ( sJson ) {
		xsHttpReplyAuto(objResp, 200, "Content-Type: application/json\r\n", sJson, iSize);
		xrtFree(sJson);
	} else {
		xsHttpReplyAuto(objResp, 500, "Content-Type: application/json\r\n", "{\"result\":false,\"message\":\"json encode failed\"}", 0);
	}
	xvoUnref(objValue);
}

void Managed_SendError(XS_ResponseObject objResp, const char* sMessage)
{
	xvalue tblRet = Managed_CreateResult(FALSE, sMessage ? sMessage : "request failed");

	if ( tblRet == NULL ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: application/json\r\n", "{\"result\":false,\"message\":\"request failed\"}", 0);
		return;
	}
	Managed_SendJsonValue(objResp, tblRet);
}

xvalue Managed_ParseJsonBody(XS_RequestObject objReq)
{
	xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));

	if ( (tblForm == NULL) || (xvoType(tblForm) != XVO_DT_TABLE) ) {
		if ( tblForm ) xvoUnref(tblForm);
		return NULL;
	}
	return tblForm;
}

void Managed_SetTimeText(xvalue tblItem, const char* sKey, int iKeyLen, xtime iTime)
{
	str sValue = (iTime > 0) ? xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME) : xrtCopyStr("", 0);
	xvoTableSetText(tblItem, sKey, iKeyLen, sValue ? sValue : (str)"", 0, TRUE);
}

bool Managed_SendAssetHtml(XS_ResponseObject objResp, const char* sFileName)
{
	str sPath;
	ptr pData;
	size_t iSize = 0;

	if ( (G_RootPath == NULL) || (sFileName == NULL) ) {
		return FALSE;
	}
	sPath = xrtPathJoin(2, G_RootPath, (str)sFileName);
	if ( (sPath == NULL) || !xrtFileExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		return FALSE;
	}
	pData = xrtFileGetAll(sPath, &iSize);
	xrtFree(sPath);
	if ( pData == NULL ) {
		return FALSE;
	}
	xsHttpReplyAuto(objResp, 200, "Content-Type: text/html; charset=utf-8\r\n", pData, iSize);
	xrtFree(pData);
	return TRUE;
}

xvalue Managed_LoadJsonFile(const char* sRelPath)
{
	str sPath;
	ptr pData;
	size_t iSize = 0;
	xvalue objValue = NULL;

	if ( (G_RootPath == NULL) || (sRelPath == NULL) ) {
		return NULL;
	}
	sPath = xrtPathJoin(2, G_RootPath, (str)sRelPath);
	if ( (sPath == NULL) || !xrtFileExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		return NULL;
	}
	pData = xrtFileGetAll(sPath, &iSize);
	xrtFree(sPath);
	if ( pData == NULL ) {
		return NULL;
	}
	objValue = xrtParseJSON((str)pData, iSize);
	xrtFree(pData);
	return objValue;
}

xvalue Managed_LoadJsonPath(const char* sPath)
{
	ptr pData;
	size_t iSize = 0;
	xvalue objValue = NULL;

	if ( (sPath == NULL) || !xrtFileExists((str)sPath) ) {
		return NULL;
	}
	pData = xrtFileGetAll((str)sPath, &iSize);
	if ( pData == NULL ) {
		return NULL;
	}
	objValue = xrtParseJSON((str)pData, iSize);
	xrtFree(pData);
	return objValue;
}

xvalue Managed_LoadSpec(void)
{
	return Managed_LoadJsonFile("generated/spec.json");
}

xvalue Managed_LoadManagedMeta(void)
{
	return Managed_LoadJsonFile("runtime/managed.json");
}

xvalue Managed_LoadContractsMeta(void)
{
	return Managed_LoadJsonFile("runtime/contracts.json");
}

xvalue Managed_LoadCustomMounts(void)
{
	return Managed_LoadJsonFile("custom/capability.mounts.json");
}

xvalue Managed_LoadMountSample(void)
{
	return Managed_LoadJsonFile("runtime/capability.mounts.example.json");
}

xvalue Managed_GetTableValue(xvalue tblData, const char* sKey)
{
	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || (sKey == NULL) ) {
		return NULL;
	}
	return xvoTableGetValue(tblData, sKey, (int)strlen(sKey));
}

xvalue Managed_GetMountArray(xvalue objMounts)
{
	if ( objMounts == NULL ) {
		return NULL;
	}
	if ( xvoType(objMounts) == XVO_DT_ARRAY ) {
		return objMounts;
	}
	if ( xvoType(objMounts) == XVO_DT_TABLE ) {
		return xvoTableGetValue(objMounts, "mounts", 6);
	}
	return NULL;
}

xvalue Managed_FindMountBySlotKey(xvalue arrMounts, const char* sSlotKey)
{
	if ( (arrMounts == NULL) || (xvoType(arrMounts) != XVO_DT_ARRAY) || Managed_IsBlank(sSlotKey) ) {
		return NULL;
	}
	for ( uint32 i = 0; i < xvoArrayItemCount(arrMounts); i++ ) {
		xvalue tblMount = xvoArrayGetValue(arrMounts, i);
		const char* sKey;

		if ( (tblMount == NULL) || (xvoType(tblMount) != XVO_DT_TABLE) ) {
			continue;
		}
		sKey = xvoTableGetText(tblMount, "slotKey", 7);
		if ( Managed_IsBlank(sKey) ) {
			sKey = xvoTableGetText(tblMount, "key", 3);
		}
		if ( sKey && (strcmp(sKey, sSlotKey) == 0) ) {
			return tblMount;
		}
	}
	return NULL;
}

bool Managed_IsValidXid(const char* sXid)
{
	size_t iLen;

	if ( Managed_IsBlank(sXid) ) {
		return FALSE;
	}
	iLen = strlen(sXid);
	if ( (iLen == 0) || (iLen > 96) ) {
		return FALSE;
	}
	for ( size_t i = 0; i < iLen; i++ ) {
		char c = sXid[i];
		bool bOK = ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '.') || (c == '_') || (c == '-');
		if ( !bOK ) {
			return FALSE;
		}
	}
	return TRUE;
}

bool Managed_IsValidRoutePath(const char* sPath)
{
	size_t iLen;

	if ( Managed_IsBlank(sPath) ) {
		return FALSE;
	}
	iLen = strlen(sPath);
	if ( (iLen == 0) || (iLen > 128) || (sPath[0] != '/') ) {
		return FALSE;
	}
	for ( size_t i = 1; i < iLen; i++ ) {
		char c = sPath[i];
		bool bOK = ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '/') || (c == '-') || (c == '_') || (c == '.') || (c == '?') || (c == '=') || (c == '&');
		if ( !bOK ) {
			return FALSE;
		}
	}
	return TRUE;
}

void Managed_BumpStatusCount(xvalue tblCounts, const char* sStatus)
{
	int iLen;
	int iCount;

	if ( (tblCounts == NULL) || (xvoType(tblCounts) != XVO_DT_TABLE) || Managed_IsBlank(sStatus) ) {
		return;
	}
	iLen = (int)strlen(sStatus);
	iCount = xvoTableGetInt(tblCounts, sStatus, iLen);
	xvoTableSetInt(tblCounts, sStatus, iLen, iCount + 1);
}

void Managed_CountMountRegistryStatus(
	const char* sStatus,
	int* piMounted,
	int* piConfigured,
	int* piUnresolved,
	int* piInvalid,
	xvalue tblStatusCounts)
{
	if ( Managed_IsBlank(sStatus) ) {
		return;
	}

	Managed_BumpStatusCount(tblStatusCounts, sStatus);
	if ( strcmp(sStatus, "mounted") == 0 ) {
		if ( piMounted ) (*piMounted)++;
	} else if ( strcmp(sStatus, "configured") == 0 ) {
		if ( piConfigured ) (*piConfigured)++;
	} else if ( strcmp(sStatus, "unresolved") == 0 ) {
		if ( piUnresolved ) (*piUnresolved)++;
	} else {
		if ( piInvalid ) (*piInvalid)++;
	}
}

const char* Managed_GetNestedText(xvalue tblData, const char* sKey1, const char* sKey2)
{
	xvalue tblInner = Managed_GetTableValue(tblData, sKey1);
	if ( (tblInner == NULL) || (xvoType(tblInner) != XVO_DT_TABLE) ) {
		return NULL;
	}
	return xvoTableGetText(tblInner, sKey2, (int)strlen(sKey2));
}

xvalue Managed_CreateEmptyMountRoot(void)
{
	xvalue tblRoot = xvoCreateTable();
	xvoTableSetValue(tblRoot, "mounts", 6, xvoCreateArray(), TRUE);
	return tblRoot;
}

xvalue Managed_CopyMountRoot(xvalue objMounts)
{
	if ( (objMounts != NULL) && (xvoType(objMounts) == XVO_DT_TABLE) ) {
		return xvoCopy(objMounts);
	}
	if ( (objMounts != NULL) && (xvoType(objMounts) == XVO_DT_ARRAY) ) {
		xvalue tblRoot = Managed_CreateEmptyMountRoot();
		xvoTableSetValue(tblRoot, "mounts", 6, xvoCopy(objMounts), TRUE);
		return tblRoot;
	}
	return Managed_CreateEmptyMountRoot();
}

bool Managed_IsValidMountSurface(const char* sSurface)
{
	if ( Managed_IsBlank(sSurface) ) {
		return TRUE;
	}
	return strcmp(sSurface, "admin.record-list") == 0
		|| strcmp(sSurface, "admin.record-detail") == 0
		|| strcmp(sSurface, "admin.record-form") == 0
		|| strcmp(sSurface, "public.record-list") == 0
		|| strcmp(sSurface, "public.record-detail") == 0;
}

bool Managed_IsValidMountMode(const char* sMode)
{
	if ( Managed_IsBlank(sMode) ) {
		return TRUE;
	}
	return strcmp(sMode, "optional") == 0 || strcmp(sMode, "required") == 0;
}

bool Managed_ValidateMountEntry(xvalue tblMount, int iIndex, str* psError)
{
	const char* sSlotKey;
	const char* sProvider;
	const char* sServiceXid;
	const char* sStatus;
	const char* sSurface;
	const char* sMode;
	const char* sAdminHref;
	const char* sPublicHref;
	xvalue tblMountConfig;
	xvalue tblAdminEntry;
	xvalue tblPublicEntry;
	xvalue tblOptions;

	if ( psError ) *psError = NULL;
	if ( (tblMount == NULL) || (xvoType(tblMount) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtFormat("mounts[%d] must be an object", iIndex);
		return FALSE;
	}
	sSlotKey = xvoTableGetText(tblMount, "slotKey", 7);
	if ( Managed_IsBlank(sSlotKey) ) {
		if ( psError ) *psError = xrtFormat("mounts[%d].slotKey is required", iIndex);
		return FALSE;
	}
	sProvider = xvoTableGetText(tblMount, "providerPlugin", 14);
	if ( !Managed_IsBlank(sProvider) && !Managed_IsValidXid(sProvider) ) {
		if ( psError ) *psError = xrtFormat("mounts[%d].providerPlugin is invalid", iIndex);
		return FALSE;
	}
	sServiceXid = xvoTableGetText(tblMount, "serviceXid", 10);
	if ( !Managed_IsBlank(sServiceXid) && !Managed_IsValidXid(sServiceXid) ) {
		if ( psError ) *psError = xrtFormat("mounts[%d].serviceXid is invalid", iIndex);
		return FALSE;
	}
	sStatus = xvoTableGetText(tblMount, "status", 6);
	if ( !Managed_IsBlank(sStatus)
		&& strcmp(sStatus, "mounted") != 0
		&& strcmp(sStatus, "configured") != 0
		&& strcmp(sStatus, "unresolved") != 0 ) {
		if ( psError ) *psError = xrtFormat("mounts[%d].status is invalid", iIndex);
		return FALSE;
	}

	tblMountConfig = Managed_GetTableValue(tblMount, "mount");
	if ( (tblMountConfig != NULL) && (xvoType(tblMountConfig) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtFormat("mounts[%d].mount must be an object", iIndex);
		return FALSE;
	}
	sSurface = tblMountConfig ? xvoTableGetText(tblMountConfig, "surface", 7) : xvoTableGetText(tblMount, "surface", 7);
	if ( !Managed_IsValidMountSurface(sSurface) ) {
		if ( psError ) *psError = xrtFormat("mounts[%d].mount.surface is invalid", iIndex);
		return FALSE;
	}
	sMode = tblMountConfig ? xvoTableGetText(tblMountConfig, "mode", 4) : xvoTableGetText(tblMount, "mode", 4);
	if ( !Managed_IsValidMountMode(sMode) ) {
		if ( psError ) *psError = xrtFormat("mounts[%d].mount.mode is invalid", iIndex);
		return FALSE;
	}

	tblAdminEntry = Managed_GetTableValue(tblMount, "adminEntry");
	if ( (tblAdminEntry != NULL) && (xvoType(tblAdminEntry) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtFormat("mounts[%d].adminEntry must be an object", iIndex);
		return FALSE;
	}
	tblPublicEntry = Managed_GetTableValue(tblMount, "publicEntry");
	if ( (tblPublicEntry != NULL) && (xvoType(tblPublicEntry) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtFormat("mounts[%d].publicEntry must be an object", iIndex);
		return FALSE;
	}
	tblOptions = Managed_GetTableValue(tblMount, "options");
	if ( (tblOptions != NULL) && (xvoType(tblOptions) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtFormat("mounts[%d].options must be an object", iIndex);
		return FALSE;
	}
	sAdminHref = tblAdminEntry ? xvoTableGetText(tblAdminEntry, "href", 4) : NULL;
	sPublicHref = tblPublicEntry ? xvoTableGetText(tblPublicEntry, "href", 4) : NULL;
	if ( !Managed_IsBlank(sAdminHref) && !Managed_IsValidRoutePath(sAdminHref) ) {
		if ( psError ) *psError = xrtFormat("mounts[%d].adminEntry.href is invalid", iIndex);
		return FALSE;
	}
	if ( !Managed_IsBlank(sPublicHref) && !Managed_IsValidRoutePath(sPublicHref) ) {
		if ( psError ) *psError = xrtFormat("mounts[%d].publicEntry.href is invalid", iIndex);
		return FALSE;
	}
	return TRUE;
}

bool Managed_ValidateCustomMounts(xvalue tblRoot, str* psError)
{
	xvalue arrMounts;

	if ( psError ) *psError = NULL;
	if ( (tblRoot == NULL) || (xvoType(tblRoot) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("mount payload must be an object", 0);
		return FALSE;
	}
	arrMounts = Managed_GetMountArray(tblRoot);
	if ( (arrMounts == NULL) || (xvoType(arrMounts) != XVO_DT_ARRAY) ) {
		if ( psError ) *psError = xrtCopyStr("mounts must be an array", 0);
		return FALSE;
	}
	for ( uint32 i = 0; i < xvoArrayItemCount(arrMounts); i++ ) {
		if ( !Managed_ValidateMountEntry(xvoArrayGetValue(arrMounts, i), (int)i, psError) ) {
			return FALSE;
		}
	}
	return TRUE;
}

bool Managed_SaveCustomMountsFile(xvalue tblRoot)
{
	str sCustomDir = NULL;
	str sFilePath = NULL;
	str sJson = NULL;
	size_t iSize = 0;
	bool bOK = FALSE;

	if ( (G_RootPath == NULL) || (tblRoot == NULL) || (xvoType(tblRoot) != XVO_DT_TABLE) ) {
		return FALSE;
	}
	sCustomDir = xrtPathJoin(2, G_RootPath, "custom");
	if ( sCustomDir == NULL ) {
		return FALSE;
	}
	if ( !xrtDirCreateAll(sCustomDir) ) {
		xrtFree(sCustomDir);
		return FALSE;
	}
	sFilePath = xrtPathJoin(2, G_RootPath, "custom/capability.mounts.json");
	xrtFree(sCustomDir);
	if ( sFilePath == NULL ) {
		return FALSE;
	}
	sJson = xrtStringifyJSON(tblRoot, TRUE, &iSize);
	if ( sJson ) {
		bOK = xrtFilePutAll(sFilePath, sJson, iSize) == 0;
		xrtFree(sJson);
	}
	xrtFree(sFilePath);
	return bOK;
}

bool Managed_TextInArray(xvalue arrValues, const char* sNeedle)
{
	if ( (arrValues == NULL) || (xvoType(arrValues) != XVO_DT_ARRAY) || Managed_IsBlank(sNeedle) ) {
		return FALSE;
	}
	for ( uint32 i = 0; i < xvoArrayItemCount(arrValues); i++ ) {
		xvalue objItem = xvoArrayGetValue(arrValues, i);
		const char* sValue = xvoGetText(objItem);
		if ( sValue && (strcmp(sValue, sNeedle) == 0) ) {
			return TRUE;
		}
	}
	return FALSE;
}

xvalue Managed_NormalizeProviderSpec(xvalue tblProvider, const char* sPluginXid, const char* sPluginTitle, const char* sVersion)
{
	xvalue tblRet;
	xvalue tblSupports;
	xvalue arrServiceXid;
	xvalue arrSurface;
	xvalue arrEntries;
	const char* sKey;
	const char* sTitle;
	const char* sDesc;

	if ( (tblProvider == NULL) || (xvoType(tblProvider) != XVO_DT_TABLE) || Managed_IsBlank(sPluginXid) ) {
		return NULL;
	}
	tblRet = xvoCreateTable();
	sKey = xvoTableGetText(tblProvider, "key", 3);
	sTitle = xvoTableGetText(tblProvider, "title", 5);
	sDesc = xvoTableGetText(tblProvider, "desc", 4);
	tblSupports = Managed_GetTableValue(tblProvider, "supports");
	arrServiceXid = tblSupports ? xvoTableGetValue(tblSupports, "serviceXid", 10) : NULL;
	arrSurface = tblSupports ? xvoTableGetValue(tblSupports, "surface", 7) : NULL;
	arrEntries = xvoTableGetValue(tblProvider, "entries", 7);

	xvoTableSetText(tblRet, "pluginXid", 9, (str)sPluginXid, 0, FALSE);
	xvoTableSetText(tblRet, "pluginTitle", 11, (str)(Managed_IsBlank(sPluginTitle) ? sPluginXid : sPluginTitle), 0, FALSE);
	xvoTableSetText(tblRet, "pluginVersion", 13, (str)(Managed_IsBlank(sVersion) ? "" : sVersion), 0, FALSE);
	xvoTableSetText(tblRet, "key", 3, (str)(Managed_IsBlank(sKey) ? sPluginXid : sKey), 0, FALSE);
	xvoTableSetText(tblRet, "title", 5, (str)(Managed_IsBlank(sTitle) ? (Managed_IsBlank(sPluginTitle) ? sPluginXid : sPluginTitle) : sTitle), 0, FALSE);
	xvoTableSetText(tblRet, "desc", 4, (str)(sDesc ? sDesc : ""), 0, FALSE);
	xvoTableSetValue(tblRet, "supports", 8, tblSupports ? xvoCopy(tblSupports) : xvoCreateTable(), TRUE);
	xvoTableSetValue(tblRet, "serviceXidList", 14, ((arrServiceXid != NULL) && (xvoType(arrServiceXid) == XVO_DT_ARRAY)) ? xvoCopy(arrServiceXid) : xvoCreateArray(), TRUE);
	xvoTableSetValue(tblRet, "surfaceList", 11, ((arrSurface != NULL) && (xvoType(arrSurface) == XVO_DT_ARRAY)) ? xvoCopy(arrSurface) : xvoCreateArray(), TRUE);
	xvoTableSetValue(tblRet, "adminEntry", 10, xvoCopy(Managed_GetTableValue(tblProvider, "adminEntry")), TRUE);
	xvoTableSetValue(tblRet, "publicEntry", 11, xvoCopy(Managed_GetTableValue(tblProvider, "publicEntry")), TRUE);
	xvoTableSetValue(tblRet, "defaults", 8, xvoCopy(Managed_GetTableValue(tblProvider, "defaults")), TRUE);
	xvoTableSetValue(tblRet, "optionSchema", 12, xvoCopy(Managed_GetTableValue(tblProvider, "optionSchema")), TRUE);
	xvoTableSetValue(tblRet, "entries", 7, ((arrEntries != NULL) && (xvoType(arrEntries) == XVO_DT_ARRAY)) ? xvoCopy(arrEntries) : xvoCreateArray(), TRUE);
	return tblRet;
}

void Managed_AppendProviderSpecs(xvalue arrProviders, xvalue tblProviderFile, const char* sPluginXid, const char* sPluginTitle, const char* sVersion)
{
	xvalue arrItems;

	if ( (arrProviders == NULL) || (xvoType(arrProviders) != XVO_DT_ARRAY) || (tblProviderFile == NULL) || (xvoType(tblProviderFile) != XVO_DT_TABLE) ) {
		return;
	}
	arrItems = xvoTableGetValue(tblProviderFile, "providers", 9);
	if ( (arrItems != NULL) && (xvoType(arrItems) == XVO_DT_ARRAY) ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrItems); i++ ) {
			xvalue tblProvider = Managed_NormalizeProviderSpec(xvoArrayGetValue(arrItems, i), sPluginXid, sPluginTitle, sVersion);
			if ( tblProvider ) {
				xvoArrayAppendValue(arrProviders, tblProvider, TRUE);
			}
		}
		return;
	}
	{
		xvalue tblProvider = Managed_NormalizeProviderSpec(tblProviderFile, sPluginXid, sPluginTitle, sVersion);
		if ( tblProvider ) {
			xvoArrayAppendValue(arrProviders, tblProvider, TRUE);
		}
	}
}

int Managed_ScanProviderPluginProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
{
	ManagedProviderScanContext* pCtx = (ManagedProviderScanContext*)Param;
	str sDirName = NULL;
	str sProviderPath = NULL;
	str sManifestPath = NULL;
	xvalue tblProviderFile = NULL;
	xvalue tblManifest = NULL;
	const char* sPluginXid = NULL;
	const char* sPluginTitle = NULL;
	const char* sVersion = NULL;

	(void)iSize;
	(void)pData;

	if ( (pCtx == NULL) || (pCtx->arrProviders == NULL) || (bDir == 0) || (sPath == NULL) ) {
		return FALSE;
	}

	sDirName = xrtPathGetNameExt(sPath, 0);
	if ( Managed_IsBlank((const char*)sDirName) ) {
		if ( sDirName ) xrtFree(sDirName);
		return FALSE;
	}
	if ( pCtx->sCurrentPluginXid && (strcmp((const char*)sDirName, (const char*)pCtx->sCurrentPluginXid) == 0) ) {
		xrtFree(sDirName);
		return FALSE;
	}

	sProviderPath = xrtPathJoin(2, sPath, "capability.provider.json");
	if ( (sProviderPath == NULL) || !xrtFileExists(sProviderPath) ) {
		if ( sProviderPath ) xrtFree(sProviderPath);
		xrtFree(sDirName);
		return FALSE;
	}
	tblProviderFile = Managed_LoadJsonPath(sProviderPath);
	xrtFree(sProviderPath);
	if ( (tblProviderFile == NULL) || (xvoType(tblProviderFile) != XVO_DT_TABLE) ) {
		if ( tblProviderFile ) xvoUnref(tblProviderFile);
		xrtFree(sDirName);
		return FALSE;
	}

	sManifestPath = xrtPathJoin(2, sPath, "plugin.json");
	if ( sManifestPath ) {
		tblManifest = Managed_LoadJsonPath(sManifestPath);
		xrtFree(sManifestPath);
	}
	sPluginXid = tblManifest ? xvoTableGetText(tblManifest, "xid", 3) : NULL;
	if ( Managed_IsBlank(sPluginXid) ) {
		sPluginXid = (const char*)sDirName;
	}
	sPluginTitle = tblManifest ? xvoTableGetText(tblManifest, "title", 5) : NULL;
	sVersion = tblManifest ? xvoTableGetText(tblManifest, "version", 7) : NULL;

	Managed_AppendProviderSpecs(pCtx->arrProviders, tblProviderFile, sPluginXid, sPluginTitle, sVersion);

	if ( tblManifest ) xvoUnref(tblManifest);
	xvoUnref(tblProviderFile);
	xrtFree(sDirName);
	return FALSE;
}

xvalue Managed_ScanCapabilityProviders(void)
{
	ManagedProviderScanContext ctx;
	str sPluginDir;
	str sCurrentPluginXid;

	if ( G_RootPath == NULL ) {
		return xvoCreateArray();
	}
	sPluginDir = xrtPathGetDir((str)G_RootPath, 0);
	if ( sPluginDir == NULL ) {
		return xvoCreateArray();
	}
	sCurrentPluginXid = xrtPathGetNameExt((str)G_RootPath, 0);
	ctx.arrProviders = xvoCreateArray();
	ctx.sCurrentPluginXid = sCurrentPluginXid;
	xrtDirScan(sPluginDir, FALSE, Managed_ScanProviderPluginProc, &ctx);
	xrtFree(sPluginDir);
	if ( sCurrentPluginXid ) xrtFree(sCurrentPluginXid);
	return ctx.arrProviders;
}

xvalue Managed_BuildProviderSuggestions(xvalue tblContracts, xvalue arrProviders)
{
	xvalue tblRet = xvoCreateTable();
	xvalue arrSuggestions = xvoCreateArray();
	xvalue arrSlots = NULL;

	if ( tblContracts && (xvoType(tblContracts) == XVO_DT_TABLE) ) {
		arrSlots = xvoTableGetValue(tblContracts, "capabilitySlots", 15);
	}
	xvoTableSetValue(tblRet, "providers", 9, ((arrProviders != NULL) && (xvoType(arrProviders) == XVO_DT_ARRAY)) ? xvoCopy(arrProviders) : xvoCreateArray(), TRUE);
	xvoTableSetValue(tblRet, "slots", 5, arrSuggestions, TRUE);

	if ( (arrSlots == NULL) || (xvoType(arrSlots) != XVO_DT_ARRAY) || (arrProviders == NULL) || (xvoType(arrProviders) != XVO_DT_ARRAY) ) {
		return tblRet;
	}
	for ( uint32 i = 0; i < xvoArrayItemCount(arrSlots); i++ ) {
		xvalue tblSlot = xvoArrayGetValue(arrSlots, i);
		const char* sSlotKey;
		const char* sServiceXid;
		const char* sSurface;
		xvalue tblMount;
		xvalue tblSuggestion;
		xvalue arrCandidates;

		if ( (tblSlot == NULL) || (xvoType(tblSlot) != XVO_DT_TABLE) ) {
			continue;
		}
		sSlotKey = xvoTableGetText(tblSlot, "key", 3);
		if ( Managed_IsBlank(sSlotKey) ) {
			continue;
		}
		tblMount = Managed_GetTableValue(tblSlot, "mount");
		sServiceXid = xvoTableGetText(tblSlot, "serviceXid", 10);
		sSurface = tblMount ? xvoTableGetText(tblMount, "surface", 7) : NULL;
		tblSuggestion = xvoCreateTable();
		arrCandidates = xvoCreateArray();
		xvoTableSetText(tblSuggestion, "slotKey", 7, (str)sSlotKey, 0, FALSE);
		xvoTableSetValue(tblSuggestion, "slot", 4, xvoCopy(tblSlot), TRUE);
		xvoTableSetValue(tblSuggestion, "providers", 9, arrCandidates, TRUE);

		for ( uint32 j = 0; j < xvoArrayItemCount(arrProviders); j++ ) {
			xvalue tblProvider = xvoArrayGetValue(arrProviders, j);
			xvalue arrServiceList;
			xvalue arrSurfaceList;
			bool bServiceMatch;
			bool bSurfaceMatch;

			if ( (tblProvider == NULL) || (xvoType(tblProvider) != XVO_DT_TABLE) ) {
				continue;
			}
			arrServiceList = xvoTableGetValue(tblProvider, "serviceXidList", 14);
			arrSurfaceList = xvoTableGetValue(tblProvider, "surfaceList", 11);
			bServiceMatch = Managed_IsBlank(sServiceXid) || (Managed_TextInArray(arrServiceList, sServiceXid));
			bSurfaceMatch = Managed_IsBlank(sSurface) || (Managed_TextInArray(arrSurfaceList, sSurface));
			if ( bServiceMatch && bSurfaceMatch ) {
				xvoArrayAppendValue(arrCandidates, xvoCopy(tblProvider), TRUE);
			}
		}
		xvoArrayAppendValue(arrSuggestions, tblSuggestion, TRUE);
	}
	return tblRet;
}

xvalue Managed_GetIdentity(xvalue tblSpec)
{
	return Managed_GetTableValue(tblSpec, "identity");
}

xvalue Managed_GetEntity(xvalue tblSpec)
{
	return Managed_GetTableValue(tblSpec, "entity");
}

xvalue Managed_GetFields(xvalue tblSpec)
{
	xvalue tblEntity = Managed_GetEntity(tblSpec);
	if ( (tblEntity == NULL) || (xvoType(tblEntity) != XVO_DT_TABLE) ) {
		return NULL;
	}
	return xvoTableGetValue(tblEntity, "fields", 6);
}

const char* Managed_GetFieldSemanticRole(xvalue tblField)
{
	xvalue tblSemantic = Managed_GetTableValue(tblField, "semantic");
	return ((tblSemantic != NULL) && (xvoType(tblSemantic) == XVO_DT_TABLE))
		? xvoTableGetText(tblSemantic, "role", 4)
		: NULL;
}

xvalue Managed_FindFieldByRole(xvalue tblSpec, const char* sRole)
{
	xvalue arrFields = Managed_GetFields(tblSpec);

	if ( (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) || Managed_IsBlank(sRole) ) {
		return NULL;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
		xvalue tblField = xvoArrayGetValue(arrFields, i);
		const char* sFieldRole = Managed_GetFieldSemanticRole(tblField);
		if ( !Managed_IsBlank(sFieldRole) && (strcmp(sFieldRole, sRole) == 0) ) {
			return tblField;
		}
	}
	return NULL;
}

const char* Managed_GetEntityFieldNameByKey(xvalue tblSpec, const char* sKey)
{
	xvalue tblEntity = Managed_GetEntity(tblSpec);
	return (tblEntity && !Managed_IsBlank(sKey)) ? xvoTableGetText(tblEntity, sKey, (int)strlen(sKey)) : NULL;
}

const char* Managed_GetEntityFieldOrRole(xvalue tblSpec, const char* sEntityKey, const char* sRole)
{
	const char* sFieldName = Managed_GetEntityFieldNameByKey(tblSpec, sEntityKey);
	xvalue tblField;

	if ( !Managed_IsBlank(sFieldName) ) {
		return sFieldName;
	}
	tblField = Managed_FindFieldByRole(tblSpec, sRole);
	return tblField ? xvoTableGetText(tblField, "name", 4) : NULL;
}

const char* Managed_GetTitleField(xvalue tblSpec)
{
	return Managed_GetEntityFieldOrRole(tblSpec, "titleField", "title");
}

const char* Managed_GetStatusField(xvalue tblSpec)
{
	return Managed_GetEntityFieldOrRole(tblSpec, "statusField", "status");
}

const char* Managed_GetSlugField(xvalue tblSpec)
{
	const char* sField = Managed_GetEntityFieldOrRole(tblSpec, "slugField", "slug");
	return Managed_IsBlank(sField) ? "slug" : sField;
}

const char* Managed_GetSummaryField(xvalue tblSpec)
{
	return Managed_GetEntityFieldOrRole(tblSpec, "summaryField", "summary");
}

const char* Managed_GetCoverField(xvalue tblSpec)
{
	return Managed_GetEntityFieldOrRole(tblSpec, "coverField", "cover");
}

const char* Managed_GetPublishedAtField(xvalue tblSpec)
{
	return Managed_GetEntityFieldOrRole(tblSpec, "publishedAtField", "publishedAt");
}

const char* Managed_GetPluginTitle(xvalue tblSpec)
{
	xvalue tblIdentity = Managed_GetIdentity(tblSpec);
	const char* sTitle = tblIdentity ? xvoTableGetText(tblIdentity, "title", 5) : NULL;
	return (!Managed_IsBlank(sTitle)) ? sTitle : "{{PLUGIN_TITLE_C}}";
}

bool Managed_DraftEnabled(xvalue tblSpec)
{
	xvalue tblCore = Managed_GetTableValue(tblSpec, "coreFeatures");
	xvalue tblDraft = Managed_GetTableValue(tblCore, "draft");
	return tblDraft ? xvoTableGetBool(tblDraft, "enabled", 7) : FALSE;
}

bool Managed_AdminCrudEnabled(xvalue tblSpec)
{
	xvalue tblCore = Managed_GetTableValue(tblSpec, "coreFeatures");
	xvalue objValue = Managed_GetTableValue(tblCore, "adminCrud");
	return (objValue && (xvoType(objValue) == XVO_DT_BOOL)) ? xvoGetBool(objValue) : TRUE;
}

bool Managed_PublicApiEnabled(xvalue tblSpec)
{
	xvalue tblCore = Managed_GetTableValue(tblSpec, "coreFeatures");
	xvalue objValue = Managed_GetTableValue(tblCore, "publicApi");
	return (objValue && (xvoType(objValue) == XVO_DT_BOOL)) ? xvoGetBool(objValue) : TRUE;
}

const char* Managed_GetStatusFlow(xvalue tblSpec)
{
	xvalue tblPolicies = Managed_GetTableValue(tblSpec, "policies");
	const char* sFlow = tblPolicies ? xvoTableGetText(tblPolicies, "statusFlow", 10) : NULL;
	return Managed_IsBlank(sFlow) ? "draft-published" : sFlow;
}

bool Managed_StatusFlowNeedsReview(xvalue tblSpec)
{
	return strcmp(Managed_GetStatusFlow(tblSpec), "draft-review-published") == 0;
}

int Managed_PublicStatusThreshold(xvalue tblSpec)
{
	return Managed_StatusFlowNeedsReview(tblSpec) ? 2 : 1;
}

bool Managed_FieldIsStatusField(xvalue tblField, xvalue tblSpec)
{
	const char* sFieldName;
	const char* sStatusField = Managed_GetStatusField(tblSpec);
	const char* sRole = Managed_GetFieldSemanticRole(tblField);

	if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
		return FALSE;
	}
	sFieldName = xvoTableGetText(tblField, "name", 4);
	return (!Managed_IsBlank(sRole) && (strcmp(sRole, "status") == 0))
		|| (!Managed_IsBlank(sFieldName) && !Managed_IsBlank(sStatusField) && (strcmp(sFieldName, sStatusField) == 0));
}

xvalue Managed_CreateStatusOption(const char* sLabel, int iValue)
{
	xvalue tblOption = xvoCreateTable();
	xvoTableSetInt(tblOption, "value", 5, iValue);
	xvoTableSetText(tblOption, "label", 5, (str)(Managed_IsBlank(sLabel) ? "" : sLabel), 0, FALSE);
	return tblOption;
}

xvalue Managed_BuildDefaultStatusList(xvalue tblSpec)
{
	xvalue arrList = xvoCreateArray();

	xvoArrayAppendValue(arrList, Managed_CreateStatusOption("Hidden", 0), TRUE);
	if ( Managed_StatusFlowNeedsReview(tblSpec) ) {
		xvoArrayAppendValue(arrList, Managed_CreateStatusOption("In Review", 1), TRUE);
		xvoArrayAppendValue(arrList, Managed_CreateStatusOption("Published", 2), TRUE);
	} else {
		xvoArrayAppendValue(arrList, Managed_CreateStatusOption("Published", 1), TRUE);
	}
	return arrList;
}

xvalue Managed_GetUiListConfig(xvalue tblSpec)
{
	xvalue tblUi = Managed_GetTableValue(tblSpec, "ui");
	return Managed_GetTableValue(tblUi, "list");
}

xvalue Managed_GetUiFormConfig(xvalue tblSpec)
{
	xvalue tblUi = Managed_GetTableValue(tblSpec, "ui");
	return Managed_GetTableValue(tblUi, "form");
}

const char* Managed_GetUiFormLayout(xvalue tblSpec)
{
	xvalue tblForm = Managed_GetUiFormConfig(tblSpec);
	const char* sLayout = tblForm ? xvoTableGetText(tblForm, "layout", 6) : NULL;
	return Managed_IsBlank(sLayout) ? "single-column" : sLayout;
}

bool Managed_UiFormIsTwoColumn(xvalue tblSpec)
{
	return strcmp(Managed_GetUiFormLayout(tblSpec), "two-column") == 0;
}

int Managed_GetUiListPageSize(xvalue tblSpec, int iFallback)
{
	xvalue tblList = Managed_GetUiListConfig(tblSpec);
	xvalue objValue = Managed_GetTableValue(tblList, "pageSize");
	int iPageSize = (objValue && (xvoType(objValue) == XVO_DT_INT)) ? (int)xvoGetInt(objValue) : iFallback;

	if ( iPageSize < 1 ) iPageSize = iFallback;
	if ( iPageSize < 1 ) iPageSize = 20;
	if ( iPageSize > 200 ) iPageSize = 200;
	return iPageSize;
}

const char* Managed_NormalizeListSortField(const char* sField)
{
	if ( Managed_IsBlank(sField) ) {
		return NULL;
	}
	if ( strcmp(sField, "id") == 0 ) return "id";
	if ( strcmp(sField, "title") == 0 ) return "title";
	if ( strcmp(sField, "status") == 0 ) return "status";
	if ( strcmp(sField, "createTime") == 0 ) return "create_time";
	if ( strcmp(sField, "create_time") == 0 ) return "create_time";
	if ( strcmp(sField, "updateTime") == 0 ) return "update_time";
	if ( strcmp(sField, "update_time") == 0 ) return "update_time";
	return NULL;
}

const char* Managed_GetUiListSortField(xvalue tblSpec)
{
	xvalue tblList = Managed_GetUiListConfig(tblSpec);
	xvalue objSort = Managed_GetTableValue(tblList, "defaultSort");

	if ( (objSort != NULL) && (xvoType(objSort) == XVO_DT_ARRAY) && (xvoArrayItemCount(objSort) >= 1) ) {
		xvalue objField = xvoArrayGetValue(objSort, 0);
		if ( (objField != NULL) && (xvoType(objField) == XVO_DT_TEXT) ) {
			return xvoGetText(objField);
		}
	}
	return NULL;
}

bool Managed_GetUiListSortAsc(xvalue tblSpec)
{
	xvalue tblList = Managed_GetUiListConfig(tblSpec);
	xvalue objSort = Managed_GetTableValue(tblList, "defaultSort");

	if ( (objSort != NULL) && (xvoType(objSort) == XVO_DT_ARRAY) && (xvoArrayItemCount(objSort) >= 2) ) {
		xvalue objDir = xvoArrayGetValue(objSort, 1);
		if ( (objDir != NULL) && (xvoType(objDir) == XVO_DT_TEXT) ) {
			return strcmp(xvoGetText(objDir), "asc") == 0;
		}
	}
	return FALSE;
}

str Managed_SelectListSql(xvalue tblSpec, bool bAdmin, const char* sSortField, bool bAsc)
{
	const char* sOrderBy = bAsc ? "update_time ASC, id ASC" : "update_time DESC, id DESC";
	str sWhere = NULL;
	str sSql = NULL;

	if ( (sSortField != NULL) && (strcmp(sSortField, "id") == 0) ) {
		sOrderBy = bAsc ? "id ASC" : "id DESC";
	} else if ( (sSortField != NULL) && (strcmp(sSortField, "title") == 0) ) {
		sOrderBy = bAsc ? "title ASC, id ASC" : "title DESC, id DESC";
	} else if ( (sSortField != NULL) && (strcmp(sSortField, "status") == 0) ) {
		sOrderBy = bAsc ? "status ASC, id ASC" : "status DESC, id DESC";
	} else if ( (sSortField != NULL) && (strcmp(sSortField, "create_time") == 0) ) {
		sOrderBy = bAsc ? "create_time ASC, id ASC" : "create_time DESC, id DESC";
	}

	if ( bAdmin ) {
		sWhere = xrtCopyStr("delete_time = 0", 0);
	} else {
		sWhere = xrtFormat("delete_time = 0 AND is_draft = 0 AND status >= %d", Managed_PublicStatusThreshold(tblSpec));
	}
	if ( sWhere == NULL ) {
		return NULL;
	}
	sSql = xrtFormat(
		"SELECT id, title, status, payload_json, is_draft, create_time, update_time, category_id FROM content_item WHERE %s ORDER BY %s",
		(const char*)sWhere,
		sOrderBy);
	xrtFree(sWhere);
	return sSql;
}

bool Managed_ValueIsEmpty(xvalue objValue)
{
	if ( objValue == NULL ) {
		return TRUE;
	}
	switch ( xvoType(objValue) ) {
		case XVO_DT_NULL:
			return TRUE;
		case XVO_DT_TEXT:
			return Managed_IsBlank(xvoGetText(objValue));
		case XVO_DT_ARRAY:
			return xvoArrayItemCount(objValue) == 0;
		default:
			return FALSE;
	}
}

str Managed_ValueToTextDup(xvalue objValue)
{
	if ( objValue == NULL ) {
		return xrtCopyStr("", 0);
	}
	switch ( xvoType(objValue) ) {
		case XVO_DT_TEXT:
			return xrtCopyStr(xvoGetText(objValue), 0);
		case XVO_DT_BOOL:
			return xrtCopyStr(xvoGetBool(objValue) ? "true" : "false", 0);
		case XVO_DT_INT:
			return xrtFormat("%lld", (long long)xvoGetInt(objValue));
		case XVO_DT_FLOAT:
			return xrtFormat("%g", xvoGetFloat(objValue));
		case XVO_DT_NULL:
			return xrtCopyStr("", 0);
		default:
			return xrtStringifyJSON(objValue, FALSE, NULL);
	}
}

const char* Managed_GetFieldText(xvalue tblField, const char* sKey, const char* sFallbackKey)
{
	const char* sValue = xvoTableGetText(tblField, sKey, (int)strlen(sKey));
	if ( Managed_IsBlank(sValue) && sFallbackKey ) {
		sValue = xvoTableGetText(tblField, sFallbackKey, (int)strlen(sFallbackKey));
	}
	return sValue;
}

const char* Managed_GetComponentText(xvalue tblField, const char* sKey)
{
	return Managed_GetNestedText(tblField, "component", sKey);
}

const char* Managed_GetComponentPropsText(xvalue tblField, const char* sKey)
{
	xvalue tblComponent = Managed_GetTableValue(tblField, "component");
	xvalue tblProps = Managed_GetTableValue(tblComponent, "props");
	return tblProps ? xvoTableGetText(tblProps, sKey, (int)strlen(sKey)) : NULL;
}

xvalue Managed_CopyValueIfPresent(xvalue tblSource, const char* sKey)
{
	xvalue objValue;

	if ( (tblSource == NULL) || (xvoType(tblSource) != XVO_DT_TABLE) ) {
		return NULL;
	}
	objValue = xvoTableGetValue(tblSource, sKey, (int)strlen(sKey));
	return objValue ? xvoCopy(objValue) : NULL;
}

void Managed_SetCopiedValue(xvalue tblTarget, const char* sKey, xvalue objValue)
{
	if ( (tblTarget == NULL) || (objValue == NULL) ) {
		return;
	}
	xvoTableSetValue(tblTarget, sKey, (int)strlen(sKey), objValue, TRUE);
}

xvalue Managed_NormalizeOptionList(xvalue objList)
{
	xvalue arrRet = xvoCreateArray();

	if ( (objList == NULL) || (xvoType(objList) != XVO_DT_ARRAY) ) {
		return arrRet;
	}

	for ( uint32 i = 0; i < xvoArrayItemCount(objList); i++ ) {
		xvalue objItem = xvoArrayGetValue(objList, i);
		xvalue tblOption = xvoCreateTable();
		xvalue objValue = NULL;
		xvalue objLabel = NULL;
		str sLabel = NULL;

		if ( objItem == NULL ) {
			xvoUnref(tblOption);
			continue;
		}

		if ( xvoType(objItem) == XVO_DT_TABLE ) {
			objValue = Managed_CopyValueIfPresent(objItem, "value");
			if ( objValue == NULL ) {
				objValue = Managed_CopyValueIfPresent(objItem, "key");
			}
			objLabel = Managed_CopyValueIfPresent(objItem, "label");
			if ( objLabel == NULL ) {
				objLabel = Managed_CopyValueIfPresent(objItem, "title");
			}
			if ( objLabel == NULL ) {
				objLabel = Managed_CopyValueIfPresent(objItem, "text");
			}
		} else {
			objValue = xvoCopy(objItem);
		}

		if ( objValue == NULL ) {
			xvoUnref(tblOption);
			if ( objLabel ) xvoUnref(objLabel);
			continue;
		}

		if ( objLabel == NULL ) {
			sLabel = Managed_ValueToTextDup(objValue);
			objLabel = xvoCreateText(sLabel ? sLabel : (str)"", 0, FALSE);
			if ( sLabel ) xrtFree(sLabel);
		}

		xvoTableSetValue(tblOption, "value", 5, objValue, TRUE);
		xvoTableSetValue(tblOption, "label", 5, objLabel, TRUE);
		xvoArrayAppendValue(arrRet, tblOption, TRUE);
	}

	return arrRet;
}

xvalue Managed_ResolveFieldList(xvalue tblField)
{
	xvalue tblComponent;
	xvalue tblProps;
	xvalue objList;

	if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
		return xvoCreateArray();
	}

	objList = xvoTableGetValue(tblField, "list", 4);
	if ( objList != NULL && xvoType(objList) == XVO_DT_ARRAY ) {
		return Managed_NormalizeOptionList(objList);
	}

	objList = xvoTableGetValue(tblField, "options", 7);
	if ( objList != NULL && xvoType(objList) == XVO_DT_ARRAY ) {
		return Managed_NormalizeOptionList(objList);
	}

	tblComponent = Managed_GetTableValue(tblField, "component");
	objList = xvoTableGetValue(tblComponent, "list", 4);
	if ( objList != NULL ) {
		return Managed_NormalizeOptionList(objList);
	}

	objList = xvoTableGetValue(tblComponent, "options", 7);
	if ( objList != NULL ) {
		return Managed_NormalizeOptionList(objList);
	}

	tblProps = Managed_GetTableValue(tblComponent, "props");
	objList = xvoTableGetValue(tblProps, "list", 4);
	if ( objList != NULL ) {
		return Managed_NormalizeOptionList(objList);
	}

	objList = xvoTableGetValue(tblProps, "options", 7);
	if ( objList != NULL ) {
		return Managed_NormalizeOptionList(objList);
	}

	return xvoCreateArray();
}

const char* Managed_MapFieldType(xvalue tblField)
{
	const char* sComponentType = Managed_GetComponentText(tblField, "type");
	const char* sStorageType = Managed_GetNestedText(tblField, "storage", "type");

	if ( Managed_IsBlank(sComponentType) ) {
		sComponentType = "";
	}
	if ( Managed_IsBlank(sStorageType) ) {
		sStorageType = "";
	}

	if ( strcmp(sComponentType, "textarea") == 0 ) return "textarea";
	if ( strcmp(sComponentType, "number") == 0 ) return "number";
	if ( strcmp(sComponentType, "int") == 0 ) return "int";
	if ( strcmp(sComponentType, "image") == 0 ) return "image";
	if ( strcmp(sComponentType, "images") == 0 ) return "images";
	if ( strcmp(sComponentType, "file") == 0 ) return "file";
	if ( strcmp(sComponentType, "files") == 0 ) return "files";
	if ( strcmp(sComponentType, "switch") == 0 ) return "switch";
	if ( strcmp(sComponentType, "select") == 0 ) return "select";
	if ( strcmp(sComponentType, "combobox") == 0 ) return "combobox";
	if ( strcmp(sComponentType, "radio") == 0 ) return "radio";
	if ( strcmp(sComponentType, "checkbox") == 0 ) return "checkbox";
	if ( strcmp(sComponentType, "checklist") == 0 ) return "checklist";
	if ( strcmp(sComponentType, "date") == 0 ) return "date";
	if ( strcmp(sComponentType, "datetime") == 0 ) return "datetime";
	if ( strcmp(sComponentType, "time") == 0 ) return "time";
	if ( strcmp(sComponentType, "richtext") == 0 ) return "editor_html";
	if ( strcmp(sComponentType, "markdown") == 0 ) return "editor_md";
	if ( strcmp(sComponentType, "code") == 0 ) return "editor_code";
	if ( strcmp(sComponentType, "icon") == 0 ) return "icon_picker";
	if ( strcmp(sComponentType, "editor_html") == 0 ) return "editor_html";
	if ( strcmp(sComponentType, "editor_md") == 0 ) return "editor_md";
	if ( strcmp(sComponentType, "editor_code") == 0 ) return "editor_code";
	if ( strcmp(sComponentType, "icon_picker") == 0 ) return "icon_picker";
	if ( strcmp(sComponentType, "intrange") == 0 ) return "intrange";
	if ( strcmp(sComponentType, "numrange") == 0 ) return "numrange";
	if ( strcmp(sComponentType, "daterange") == 0 ) return "daterange";
	if ( strcmp(sComponentType, "timerange") == 0 ) return "timerange";
	if ( strcmp(sComponentType, "datetimerange") == 0 ) return "datetimerange";
	if ( strcmp(sComponentType, "password") == 0 ) return "password";
	if ( strcmp(sComponentType, "badge_picker") == 0 ) return "badge_picker";

	if ( strcmp(sStorageType, "integer") == 0 ) return "int";
	if ( strcmp(sStorageType, "float") == 0 ) return "number";
	if ( strcmp(sStorageType, "boolean") == 0 ) return "switch";
	if ( strcmp(sStorageType, "datetime") == 0 ) return "datetime";
	if ( strcmp(sStorageType, "date") == 0 ) return "date";
	if ( strcmp(sStorageType, "time") == 0 ) return "time";
	if ( strcmp(sStorageType, "json") == 0 ) return "editor_code";
	return "text";
}

xvalue Managed_BuildFieldProps(xvalue tblField, const char* sFormType)
{
	xvalue tblProps = xvoCreateTable();
	xvalue tblComponent = Managed_GetTableValue(tblField, "component");
	xvalue tblComponentProps = Managed_GetTableValue(tblComponent, "props");
	const char* sPlaceholder = NULL;
	xvalue objValue = NULL;
	const char* sKeys[] = { "min", "max", "step", "mode", "theme", "readonly", "disabled", "height", "uploadUrl", "accept", "acceptMime", "buttonText" };

	if ( tblComponentProps && (xvoType(tblComponentProps) == XVO_DT_TABLE) ) {
		for ( uint32 i = 0; i < (sizeof(sKeys) / sizeof(sKeys[0])); i++ ) {
			objValue = xvoTableGetValue(tblComponentProps, sKeys[i], (int)strlen(sKeys[i]));
			if ( objValue ) {
				xvoTableSetValue(tblProps, sKeys[i], (int)strlen(sKeys[i]), xvoCopy(objValue), TRUE);
			}
		}
	}

	for ( uint32 i = 0; i < (sizeof(sKeys) / sizeof(sKeys[0])); i++ ) {
		if ( xvoTableGetValue(tblProps, sKeys[i], (int)strlen(sKeys[i])) != NULL ) {
			continue;
		}
		objValue = xvoTableGetValue(tblComponent, sKeys[i], (int)strlen(sKeys[i]));
		if ( objValue ) {
			xvoTableSetValue(tblProps, sKeys[i], (int)strlen(sKeys[i]), xvoCopy(objValue), TRUE);
		}
	}

	sPlaceholder = Managed_GetComponentPropsText(tblField, "placeholder");
	if ( Managed_IsBlank(sPlaceholder) ) {
		sPlaceholder = Managed_GetComponentText(tblField, "placeholder");
	}
	if ( Managed_IsBlank(sPlaceholder) ) {
		sPlaceholder = Managed_GetFieldText(tblField, "placeholder", "desc");
	}
	if ( !Managed_IsBlank(sPlaceholder) ) {
		xvoTableSetText(tblProps, "placeholder", 11, (str)sPlaceholder, 0, FALSE);
	}

	if ( xvoTableGetValue(tblProps, "height", 6) == NULL ) {
		if ( strcmp(sFormType, "textarea") == 0 ) {
			xvoTableSetInt(tblProps, "height", 6, 160);
		} else if ( strcmp(sFormType, "editor_html") == 0 ) {
			xvoTableSetInt(tblProps, "height", 6, 480);
		} else if ( strcmp(sFormType, "editor_md") == 0 ) {
			xvoTableSetInt(tblProps, "height", 6, 420);
		} else if ( strcmp(sFormType, "editor_code") == 0 ) {
			xvoTableSetInt(tblProps, "height", 6, 360);
		}
	}

	if ( (strcmp(sFormType, "editor_code") == 0) && (xvoTableGetValue(tblProps, "mode", 4) == NULL) ) {
		xvoTableSetText(tblProps, "mode", 4, "javascript", 0, FALSE);
	}

	return tblProps;
}

bool Managed_FieldShouldSpanFullWidth(xvalue tblField, const char* sFormType)
{
	const char* sComponentType;

	if ( Managed_IsBlank(sFormType) ) {
		return FALSE;
	}
	if ( strcmp(sFormType, "textarea") == 0
		|| strcmp(sFormType, "image") == 0
		|| strcmp(sFormType, "images") == 0
		|| strcmp(sFormType, "file") == 0
		|| strcmp(sFormType, "files") == 0
		|| strcmp(sFormType, "editor_html") == 0
		|| strcmp(sFormType, "editor_md") == 0
		|| strcmp(sFormType, "editor_code") == 0 ) {
		return TRUE;
	}
	sComponentType = Managed_GetComponentText(tblField, "type");
	return !Managed_IsBlank(sComponentType)
		&& (strcmp(sComponentType, "textarea") == 0
			|| strcmp(sComponentType, "image") == 0
			|| strcmp(sComponentType, "richtext") == 0
			|| strcmp(sComponentType, "markdown") == 0
			|| strcmp(sComponentType, "code") == 0
			|| strcmp(sComponentType, "images") == 0
			|| strcmp(sComponentType, "file") == 0
			|| strcmp(sComponentType, "files") == 0);
}

int Managed_GetFieldLayoutSpan(xvalue tblField)
{
	xvalue objValue;

	if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
		return 0;
	}
	objValue = xvoTableGetValue(tblField, "layoutSpan", 10);
	if ( objValue == NULL ) {
		return 0;
	}
	switch ( xvoType(objValue) ) {
		case XVO_DT_INT:
			return (int)xvoGetInt(objValue);
		case XVO_DT_FLOAT:
			return (int)xvoGetFloat(objValue);
		case XVO_DT_TEXT:
			return atoi(xvoGetText(objValue));
		default:
			return 0;
	}
}

xvalue Managed_BuildFormField(xvalue tblField, xvalue tblSpec)
{
	xvalue tblRet;
	xvalue arrList;
	const char* sName;
	const char* sLabel;
	const char* sDesc;
	const char* sType;
	int iLayoutSpan;

	if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
		return NULL;
	}
	if ( !xvoTableGetBool(tblField, "showInForm", 10) && (xvoTableGetValue(tblField, "showInForm", 10) != NULL) ) {
		return NULL;
	}

	sName = xvoTableGetText(tblField, "name", 4);
	if ( Managed_IsBlank(sName) ) {
		return NULL;
	}

	sLabel = Managed_GetFieldText(tblField, "title", "name");
	sDesc = Managed_GetFieldText(tblField, "desc", "description");
	sType = Managed_MapFieldType(tblField);
	tblRet = xvoCreateTable();

	xvoTableSetText(tblRet, "name", 4, (str)sName, 0, FALSE);
	xvoTableSetText(tblRet, "type", 4, (str)sType, 0, FALSE);
	xvoTableSetText(tblRet, "label", 5, (str)(Managed_IsBlank(sLabel) ? sName : sLabel), 0, FALSE);
	xvoTableSetText(tblRet, "desc", 4, (str)(sDesc ? sDesc : ""), 0, FALSE);
	xvoTableSetBool(tblRet, "required", 8, xvoTableGetBool(tblField, "required", 8));
	xvoTableSetBool(tblRet, "readonly", 8, xvoTableGetBool(tblField, "readonly", 8));
	xvoTableSetBool(tblRet, "disabled", 8, xvoTableGetBool(tblField, "disabled", 8));
	iLayoutSpan = Managed_GetFieldLayoutSpan(tblField);
	if ( Managed_UiFormIsTwoColumn(tblSpec) ) {
		if ( iLayoutSpan >= 2 ) {
			xvoTableSetInt(tblRet, "layoutSpan", 10, 2);
		} else if ( Managed_FieldShouldSpanFullWidth(tblField, sType) ) {
			xvoTableSetInt(tblRet, "layoutSpan", 10, 2);
		}
	}
	xvoTableSetValue(tblRet, "props", 5, Managed_BuildFieldProps(tblField, sType), TRUE);

	arrList = Managed_ResolveFieldList(tblField);
	if ( (xvoArrayItemCount(arrList) == 0) && Managed_FieldIsStatusField(tblField, tblSpec) ) {
		xvoUnref(arrList);
		arrList = Managed_BuildDefaultStatusList(tblSpec);
	}
	if ( xvoArrayItemCount(arrList) > 0 ) {
		xvoTableSetValue(tblRet, "list", 4, arrList, TRUE);
	} else {
		xvoUnref(arrList);
	}

	return tblRet;
}

xvalue Managed_FindSchemaGroup(xvalue arrGroups, const char* sKey)
{
	if ( (arrGroups == NULL) || (xvoType(arrGroups) != XVO_DT_ARRAY) || Managed_IsBlank(sKey) ) {
		return NULL;
	}
	for ( uint32 i = 0; i < xvoArrayItemCount(arrGroups); i++ ) {
		xvalue tblGroup = xvoArrayGetValue(arrGroups, i);
		const char* sGroupKey = xvoTableGetText(tblGroup, "key", 3);
		if ( sGroupKey && (strcmp(sGroupKey, sKey) == 0) ) {
			return tblGroup;
		}
	}
	return NULL;
}

xvalue Managed_EnsureSchemaGroup(xvalue arrGroups, const char* sKey, const char* sTitle, const char* sDesc)
{
	xvalue tblGroup = Managed_FindSchemaGroup(arrGroups, sKey);

	if ( tblGroup ) {
		return tblGroup;
	}

	tblGroup = xvoCreateTable();
	xvoTableSetText(tblGroup, "key", 3, (str)(Managed_IsBlank(sKey) ? "content" : sKey), 0, FALSE);
	xvoTableSetText(tblGroup, "title", 5, (str)(Managed_IsBlank(sTitle) ? "Content Fields" : sTitle), 0, FALSE);
	xvoTableSetText(tblGroup, "desc", 4, (str)(sDesc ? sDesc : ""), 0, FALSE);
	xvoTableSetValue(tblGroup, "fields", 6, xvoCreateArray(), TRUE);
	xvoArrayAppendValue(arrGroups, tblGroup, TRUE);
	return tblGroup;
}

xvalue Managed_GetPresentationGroups(xvalue tblSpec)
{
	xvalue tblPresentation = Managed_GetTableValue(tblSpec, "presentation");
	return tblPresentation ? xvoTableGetValue(tblPresentation, "groups", 6) : NULL;
}

xvalue Managed_GetPresentationGroupByKey(xvalue tblSpec, const char* sKey)
{
	xvalue arrGroups = Managed_GetPresentationGroups(tblSpec);

	if ( (arrGroups == NULL) || (xvoType(arrGroups) != XVO_DT_ARRAY) || Managed_IsBlank(sKey) ) {
		return NULL;
	}
	for ( uint32 i = 0; i < xvoArrayItemCount(arrGroups); i++ ) {
		xvalue tblGroup = xvoArrayGetValue(arrGroups, i);
		const char* sGroupKey = xvoTableGetText(tblGroup, "key", 3);
		if ( sGroupKey && (strcmp(sGroupKey, sKey) == 0) ) {
			return tblGroup;
		}
	}
	return NULL;
}

xvalue Managed_BuildMountRegistry(xvalue tblContracts, xvalue tblCustomMounts)
{
	xvalue tblRet = xvoCreateTable();
	xvalue arrRet = xvoCreateArray();
	xvalue tblStatusCounts = xvoCreateTable();
	xvalue arrSlots = NULL;
	xvalue arrMounts = Managed_GetMountArray(tblCustomMounts);
	int iMounted = 0;
	int iConfigured = 0;
	int iUnresolved = 0;
	int iInvalid = 0;
	int iDeclared = 0;

	if ( tblContracts && (xvoType(tblContracts) == XVO_DT_TABLE) ) {
		arrSlots = xvoTableGetValue(tblContracts, "capabilitySlots", 15);
	}

	if ( (arrSlots != NULL) && (xvoType(arrSlots) == XVO_DT_ARRAY) ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrSlots); i++ ) {
			xvalue tblSlot = xvoArrayGetValue(arrSlots, i);
			xvalue tblItem;
			xvalue tblMount;
			const char* sKey;
			const char* sStatus;
			const char* sExpectedProvider;
			const char* sProvider;
			const char* sExpectedSurface;
			const char* sMountSurface;
			const char* sAdminHref;
			const char* sPublicHref;
			xvalue tblSlotMount;
			const char* sResolvedStatus;
			const char* sMessage = NULL;

			if ( (tblSlot == NULL) || (xvoType(tblSlot) != XVO_DT_TABLE) ) {
				continue;
			}
			sKey = xvoTableGetText(tblSlot, "key", 3);
			if ( Managed_IsBlank(sKey) ) {
				continue;
			}
			iDeclared++;
			tblItem = xvoCreateTable();
			xvoTableSetValue(tblItem, "slot", 4, xvoCopy(tblSlot), TRUE);
			xvoTableSetText(tblItem, "slotKey", 7, (str)sKey, 0, FALSE);
			tblSlotMount = Managed_GetTableValue(tblSlot, "mount");
			sExpectedProvider = xvoTableGetText(tblSlot, "serviceXid", 10);
			sExpectedSurface = tblSlotMount ? xvoTableGetText(tblSlotMount, "surface", 7) : NULL;

			tblMount = Managed_FindMountBySlotKey(arrMounts, sKey);
			if ( tblMount ) {
				sStatus = xvoTableGetText(tblMount, "status", 6);
				sProvider = xvoTableGetText(tblMount, "providerPlugin", 14);
				if ( Managed_IsBlank(sProvider) ) {
					sProvider = xvoTableGetText(tblMount, "serviceXid", 10);
				}
				sMountSurface = Managed_GetNestedText(tblMount, "mount", "surface");
				if ( Managed_IsBlank(sMountSurface) ) {
					sMountSurface = xvoTableGetText(tblMount, "surface", 7);
				}
				sAdminHref = Managed_GetNestedText(tblMount, "adminEntry", "href");
				sPublicHref = Managed_GetNestedText(tblMount, "publicEntry", "href");
				sResolvedStatus = "mounted";
				if ( !Managed_IsBlank(sProvider) && !Managed_IsValidXid(sProvider) ) {
					sResolvedStatus = "invalid-provider";
					sMessage = "providerPlugin/serviceXid is invalid.";
				} else if ( !Managed_IsBlank(sExpectedProvider) && !Managed_IsBlank(sProvider) && (strcmp(sExpectedProvider, sProvider) != 0) ) {
					sResolvedStatus = "provider-mismatch";
					sMessage = "Provider plugin does not match the slot contract.";
				} else if ( Managed_IsBlank(sProvider) ) {
					sResolvedStatus = "configured";
					sMessage = "Mount file exists but providerPlugin is missing.";
				} else if ( !Managed_IsBlank(sExpectedSurface) && Managed_IsBlank(sMountSurface) ) {
					sResolvedStatus = "configured";
					sMessage = "Mount file exists but mount.surface is missing.";
				} else if ( !Managed_IsBlank(sExpectedSurface) && !Managed_IsBlank(sMountSurface) && (strcmp(sExpectedSurface, sMountSurface) != 0) ) {
					sResolvedStatus = "surface-mismatch";
					sMessage = "Mount surface does not match the slot contract.";
				} else if ( (!Managed_IsBlank(sAdminHref) && !Managed_IsValidRoutePath(sAdminHref))
					|| (!Managed_IsBlank(sPublicHref) && !Managed_IsValidRoutePath(sPublicHref)) ) {
					sResolvedStatus = "invalid-route";
					sMessage = "Mounted entry href is invalid.";
				} else if ( !Managed_IsBlank(sStatus) && (strcmp(sStatus, "mounted") != 0) ) {
					sResolvedStatus = sStatus;
				}
				xvoTableSetBool(tblItem, "mounted", 7, strcmp(sResolvedStatus, "mounted") == 0);
				xvoTableSetText(tblItem, "status", 6, (str)sResolvedStatus, 0, FALSE);
				if ( sMessage ) {
					xvoTableSetText(tblItem, "message", 7, (str)sMessage, 0, FALSE);
				}
				xvoTableSetValue(tblItem, "mount", 5, xvoCopy(tblMount), TRUE);
				Managed_CountMountRegistryStatus(sResolvedStatus, &iMounted, &iConfigured, &iUnresolved, &iInvalid, tblStatusCounts);
			} else {
				xvoTableSetBool(tblItem, "mounted", 7, FALSE);
				xvoTableSetText(tblItem, "status", 6, "unresolved", 0, FALSE);
				xvoTableSetText(tblItem, "message", 7, "No provider mount file entry matched this slot.", 0, FALSE);
				Managed_CountMountRegistryStatus("unresolved", &iMounted, &iConfigured, &iUnresolved, &iInvalid, tblStatusCounts);
			}
			xvoArrayAppendValue(arrRet, tblItem, TRUE);
		}
	}

	if ( (arrMounts != NULL) && (xvoType(arrMounts) == XVO_DT_ARRAY) ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrMounts); i++ ) {
			xvalue tblMount = xvoArrayGetValue(arrMounts, i);
			const char* sSlotKey;

			if ( (tblMount == NULL) || (xvoType(tblMount) != XVO_DT_TABLE) ) {
				continue;
			}
			sSlotKey = xvoTableGetText(tblMount, "slotKey", 7);
			if ( Managed_IsBlank(sSlotKey) ) {
				sSlotKey = xvoTableGetText(tblMount, "key", 3);
			}
			if ( Managed_IsBlank(sSlotKey) ) {
				xvalue tblItem = xvoCreateTable();
				xvoTableSetText(tblItem, "slotKey", 7, "(missing)", 0, FALSE);
				xvoTableSetBool(tblItem, "mounted", 7, FALSE);
				xvoTableSetText(tblItem, "status", 6, "invalid-slot", 0, FALSE);
				xvoTableSetText(tblItem, "message", 7, "Mount entry is missing slotKey.", 0, FALSE);
				xvoTableSetValue(tblItem, "mount", 5, xvoCopy(tblMount), TRUE);
				xvoArrayAppendValue(arrRet, tblItem, TRUE);
				Managed_CountMountRegistryStatus("invalid-slot", &iMounted, &iConfigured, &iUnresolved, &iInvalid, tblStatusCounts);
				continue;
			}
			if ( Managed_FindMountBySlotKey(arrSlots, sSlotKey) == NULL ) {
				xvalue tblItem = xvoCreateTable();
				xvoTableSetText(tblItem, "slotKey", 7, (str)sSlotKey, 0, FALSE);
				xvoTableSetBool(tblItem, "mounted", 7, FALSE);
				xvoTableSetText(tblItem, "status", 6, "invalid-slot", 0, FALSE);
				xvoTableSetText(tblItem, "message", 7, "Mount entry references a slot that is not declared by this managed plugin.", 0, FALSE);
				xvoTableSetValue(tblItem, "mount", 5, xvoCopy(tblMount), TRUE);
				xvoArrayAppendValue(arrRet, tblItem, TRUE);
				Managed_CountMountRegistryStatus("invalid-slot", &iMounted, &iConfigured, &iUnresolved, &iInvalid, tblStatusCounts);
			}
		}
	}

	xvoTableSetValue(tblRet, "mounts", 6, arrRet, TRUE);
	xvoTableSetInt(tblRet, "total", 5, xvoArrayItemCount(arrRet));
	xvoTableSetInt(tblRet, "declaredCount", 13, iDeclared);
	xvoTableSetInt(tblRet, "resolvedCount", 13, iMounted + iConfigured);
	xvoTableSetInt(tblRet, "mountedCount", 12, iMounted);
	xvoTableSetInt(tblRet, "configuredCount", 15, iConfigured);
	xvoTableSetInt(tblRet, "unresolvedCount", 15, iUnresolved);
	xvoTableSetInt(tblRet, "invalidCount", 12, iInvalid);
	xvoTableSetValue(tblRet, "statusCounts", 12, tblStatusCounts, TRUE);
	return tblRet;
}

xvalue Managed_BuildFormSchema(xvalue tblSpec)
{
	xvalue tblSchema = xvoCreateTable();
	xvalue arrGroups = xvoCreateArray();
	xvalue arrFields = Managed_GetFields(tblSpec);
	xvalue tblIdentity = Managed_GetIdentity(tblSpec);
	const char* sTitle = tblIdentity ? xvoTableGetText(tblIdentity, "title", 5) : NULL;
	const char* sDesc = tblIdentity ? xvoTableGetText(tblIdentity, "description", 11) : NULL;

	xvoTableSetText(tblSchema, "title", 5, (str)(Managed_IsBlank(sTitle) ? "{{PLUGIN_TITLE_C}}" : sTitle), 0, FALSE);
	xvoTableSetText(tblSchema, "desc", 4, (str)(sDesc ? sDesc : ""), 0, FALSE);
	xvoTableSetText(tblSchema, "layout", 6, (str)Managed_GetUiFormLayout(tblSpec), 0, FALSE);
	xvoTableSetValue(tblSchema, "groups", 6, arrGroups, TRUE);

	if ( (arrFields != NULL) && (xvoType(arrFields) == XVO_DT_ARRAY) ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
			xvalue tblField = xvoArrayGetValue(arrFields, i);
			xvalue tblFormField = Managed_BuildFormField(tblField, tblSpec);
			const char* sGroupKey = xvoTableGetText(tblField, "group", 5);
			xvalue tblPresentationGroup = Managed_GetPresentationGroupByKey(tblSpec, sGroupKey);
			xvalue tblGroup;
			xvalue arrFormFields;

			if ( tblFormField ) {
				tblGroup = Managed_EnsureSchemaGroup(
					arrGroups,
					Managed_IsBlank(sGroupKey) ? "content" : sGroupKey,
					tblPresentationGroup ? (const char*)xvoTableGetText(tblPresentationGroup, "title", 5) : "内容表单",
					tblPresentationGroup ? (const char*)xvoTableGetText(tblPresentationGroup, "desc", 4) : "");
				arrFormFields = xvoTableGetValue(tblGroup, "fields", 6);
				xvoArrayAppendValue(arrFormFields, tblFormField, TRUE);
			}
		}
	}
	if ( xvoArrayItemCount(arrGroups) == 0 ) {
		Managed_EnsureSchemaGroup(arrGroups, "content", "内容表单", "");
	}
	return tblSchema;
}

bool Managed_ValueEquals(xvalue objLeft, xvalue objRight)
{
	if ( (objLeft == NULL) || (objRight == NULL) ) {
		return objLeft == objRight;
	}
	if ( xvoType(objLeft) == xvoType(objRight) ) {
		switch ( xvoType(objLeft) ) {
			case XVO_DT_BOOL:
				return xvoGetBool(objLeft) == xvoGetBool(objRight);
			case XVO_DT_INT:
				return xvoGetInt(objLeft) == xvoGetInt(objRight);
			case XVO_DT_FLOAT:
				return xvoGetFloat(objLeft) == xvoGetFloat(objRight);
			case XVO_DT_TEXT:
				return strcmp(
					xvoGetText(objLeft) ? (const char*)xvoGetText(objLeft) : "",
					xvoGetText(objRight) ? (const char*)xvoGetText(objRight) : "") == 0;
			default:
				break;
		}
	}

	{
		str sLeft = Managed_ValueToTextDup(objLeft);
		str sRight = Managed_ValueToTextDup(objRight);
		bool bEqual = strcmp(sLeft ? (const char*)sLeft : "", sRight ? (const char*)sRight : "") == 0;
		if ( sLeft ) xrtFree(sLeft);
		if ( sRight ) xrtFree(sRight);
		return bEqual;
	}
}

void Managed_AppendFieldValueLabel(xvalue arrList, xvalue objValue, str* psOut)
{
	for ( uint32 i = 0; i < xvoArrayItemCount(arrList); i++ ) {
		xvalue tblOption = xvoArrayGetValue(arrList, i);
		xvalue objOptionValue;

		if ( (tblOption == NULL) || (xvoType(tblOption) != XVO_DT_TABLE) ) {
			continue;
		}
		objOptionValue = xvoTableGetValue(tblOption, "value", 5);
		if ( Managed_ValueEquals(objOptionValue, objValue) ) {
			const char* sLabel = xvoTableGetText(tblOption, "label", 5);
			if ( !Managed_IsBlank(sLabel) ) {
				if ( *psOut == NULL ) {
					*psOut = xrtCopyStr((str)sLabel, 0);
				} else {
					str sNext = xrtFormat("%s, %s", *psOut, sLabel);
					xrtFree(*psOut);
					*psOut = sNext;
				}
			}
			return;
		}
	}
}

str Managed_BuildListFieldSummary(xvalue tblField, xvalue tblData)
{
	const char* sName;
	xvalue objValue;
	xvalue arrList;
	str sText = NULL;

	if ( (tblField == NULL) || (tblData == NULL) ) {
		return NULL;
	}
	sName = xvoTableGetText(tblField, "name", 4);
	if ( Managed_IsBlank(sName) ) {
		return NULL;
	}
	objValue = xvoTableGetValue(tblData, sName, 0);
	if ( Managed_ValueIsEmpty(objValue) ) {
		return NULL;
	}

	arrList = Managed_ResolveFieldList(tblField);
	if ( xvoArrayItemCount(arrList) > 0 ) {
		if ( xvoType(objValue) == XVO_DT_ARRAY ) {
			for ( uint32 i = 0; i < xvoArrayItemCount(objValue); i++ ) {
				xvalue objItem = xvoArrayGetValue(objValue, i);
				Managed_AppendFieldValueLabel(arrList, objItem, &sText);
			}
		} else {
			Managed_AppendFieldValueLabel(arrList, objValue, &sText);
		}
	}
	xvoUnref(arrList);

	if ( sText == NULL ) {
		sText = Managed_ValueToTextDup(objValue);
	}
	return sText;
}

void Managed_ApplyMissingFieldDefault(xvalue tblData, xvalue tblField)
{
	const char* sName;
	xvalue objCurrent;
	xvalue objDefault;

	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
		return;
	}
	sName = xvoTableGetText(tblField, "name", 4);
	if ( Managed_IsBlank(sName) ) {
		return;
	}
	objCurrent = xvoTableGetValue(tblData, sName, 0);
	if ( objCurrent != NULL ) {
		return;
	}
	objDefault = xvoTableGetValue(tblField, "defaultValue", 12);
	if ( objDefault == NULL ) {
		return;
	}
	xvoTableSetValue(tblData, sName, (int)strlen(sName), xvoCopy(objDefault), TRUE);
}

void Managed_ApplyMissingDefaults(xvalue tblData, xvalue tblSpec)
{
	xvalue arrFields = Managed_GetFields(tblSpec);

	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) ) {
		return;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
		Managed_ApplyMissingFieldDefault(tblData, xvoArrayGetValue(arrFields, i));
	}
}

void Managed_NormalizeNullableField(xvalue tblData, xvalue tblField)
{
	const char* sName;
	xvalue objValue;

	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
		return;
	}
	if ( !xvoTableGetBool(tblField, "nullable", 8) ) {
		return;
	}
	sName = xvoTableGetText(tblField, "name", 4);
	if ( Managed_IsBlank(sName) ) {
		return;
	}
	objValue = xvoTableGetValue(tblData, sName, 0);
	if ( (objValue == NULL) || !Managed_ValueIsEmpty(objValue) ) {
		return;
	}
	xvoTableSetNull(tblData, sName, (int)strlen(sName));
}

void Managed_NormalizeNullableFields(xvalue tblData, xvalue tblSpec)
{
	xvalue arrFields = Managed_GetFields(tblSpec);

	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) ) {
		return;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
		Managed_NormalizeNullableField(tblData, xvoArrayGetValue(arrFields, i));
	}
}

bool Managed_IsAsciiSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool Managed_TextEqualsIgnoreCase(const char* sLeft, const char* sRight)
{
	size_t i = 0;

	if ( (sLeft == NULL) || (sRight == NULL) ) {
		return FALSE;
	}
	while ( sLeft[i] && sRight[i] ) {
		if ( Managed_ToLowerAscii(sLeft[i]) != Managed_ToLowerAscii(sRight[i]) ) {
			return FALSE;
		}
		i++;
	}
	return sLeft[i] == '\0' && sRight[i] == '\0';
}

bool Managed_TextIsInteger(const char* sText)
{
	const char* sPtr = sText;

	if ( Managed_IsBlank(sText) ) {
		return FALSE;
	}
	while ( Managed_IsAsciiSpace(*sPtr) ) sPtr++;
	if ( (*sPtr == '+') || (*sPtr == '-') ) sPtr++;
	if ( (*sPtr < '0') || (*sPtr > '9') ) {
		return FALSE;
	}
	while ( (*sPtr >= '0') && (*sPtr <= '9') ) sPtr++;
	while ( Managed_IsAsciiSpace(*sPtr) ) sPtr++;
	return *sPtr == '\0';
}

bool Managed_TextIsNumber(const char* sText)
{
	char* sEnd = NULL;

	if ( Managed_IsBlank(sText) ) {
		return FALSE;
	}
	strtod(sText, &sEnd);
	if ( sEnd == sText ) {
		return FALSE;
	}
	while ( sEnd && Managed_IsAsciiSpace(*sEnd) ) sEnd++;
	return (sEnd != NULL) && (*sEnd == '\0');
}

bool Managed_FloatIsInteger(double fValue)
{
	int64 iValue = (int64)fValue;
	return ((double)iValue) == fValue;
}

const char* Managed_NormalizeStorageTypeName(const char* sStorageType)
{
	if ( Managed_IsBlank(sStorageType) ) return "text";
	if ( strcmp(sStorageType, "int") == 0 ) return "integer";
	if ( strcmp(sStorageType, "integer") == 0 ) return "integer";
	if ( strcmp(sStorageType, "real") == 0 ) return "float";
	if ( strcmp(sStorageType, "float") == 0 ) return "float";
	if ( strcmp(sStorageType, "number") == 0 ) return "float";
	if ( strcmp(sStorageType, "bool") == 0 ) return "boolean";
	if ( strcmp(sStorageType, "boolean") == 0 ) return "boolean";
	if ( strcmp(sStorageType, "json") == 0 ) return "json";
	if ( strcmp(sStorageType, "date") == 0 ) return "date";
	if ( strcmp(sStorageType, "time") == 0 ) return "time";
	if ( strcmp(sStorageType, "datetime") == 0 ) return "datetime";
	return sStorageType;
}

bool Managed_ValueMatchesStorageType(xvalue objValue, const char* sStorageType)
{
	const char* sType = Managed_NormalizeStorageTypeName(sStorageType);

	if ( Managed_ValueIsEmpty(objValue) ) {
		return TRUE;
	}
	if ( strcmp(sType, "json") == 0 ) {
		return TRUE;
	}
	if ( strcmp(sType, "text") == 0 ) {
		return (xvoType(objValue) == XVO_DT_TEXT)
			|| (xvoType(objValue) == XVO_DT_BOOL)
			|| (xvoType(objValue) == XVO_DT_INT)
			|| (xvoType(objValue) == XVO_DT_FLOAT);
	}
	if ( strcmp(sType, "integer") == 0 ) {
		if ( xvoType(objValue) == XVO_DT_INT ) return TRUE;
		if ( xvoType(objValue) == XVO_DT_FLOAT ) return Managed_FloatIsInteger(xvoGetFloat(objValue));
		if ( xvoType(objValue) == XVO_DT_TEXT ) return Managed_TextIsInteger(xvoGetText(objValue));
		return FALSE;
	}
	if ( strcmp(sType, "float") == 0 ) {
		if ( xvoType(objValue) == XVO_DT_INT ) return TRUE;
		if ( xvoType(objValue) == XVO_DT_FLOAT ) return TRUE;
		if ( xvoType(objValue) == XVO_DT_TEXT ) return Managed_TextIsNumber(xvoGetText(objValue));
		return FALSE;
	}
	if ( strcmp(sType, "boolean") == 0 ) {
		if ( xvoType(objValue) == XVO_DT_BOOL ) return TRUE;
		if ( xvoType(objValue) == XVO_DT_INT ) {
			int64 iValue = xvoGetInt(objValue);
			return (iValue == 0) || (iValue == 1);
		}
		if ( xvoType(objValue) == XVO_DT_FLOAT ) {
			double fValue = xvoGetFloat(objValue);
			return (fValue == 0.0) || (fValue == 1.0);
		}
		if ( xvoType(objValue) == XVO_DT_TEXT ) {
			const char* sText = xvoGetText(objValue);
			return Managed_TextEqualsIgnoreCase(sText, "true")
				|| Managed_TextEqualsIgnoreCase(sText, "false")
				|| strcmp(sText, "1") == 0
				|| strcmp(sText, "0") == 0;
		}
		return FALSE;
	}
	if ( strcmp(sType, "date") == 0 || strcmp(sType, "time") == 0 || strcmp(sType, "datetime") == 0 ) {
		return (xvoType(objValue) == XVO_DT_TEXT)
			|| (xvoType(objValue) == XVO_DT_INT)
			|| (xvoType(objValue) == XVO_DT_FLOAT);
	}
	return TRUE;
}

void Managed_CoerceFieldValue(xvalue tblData, xvalue tblField)
{
	const char* sName;
	const char* sStorageType;
	const char* sType;
	xvalue objValue;

	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
		return;
	}
	sName = xvoTableGetText(tblField, "name", 4);
	if ( Managed_IsBlank(sName) ) {
		return;
	}
	objValue = xvoTableGetValue(tblData, sName, 0);
	if ( Managed_ValueIsEmpty(objValue) ) {
		return;
	}
	sStorageType = Managed_GetNestedText(tblField, "storage", "type");
	sType = Managed_NormalizeStorageTypeName(sStorageType);

	if ( strcmp(sType, "text") == 0 ) {
		if ( xvoType(objValue) != XVO_DT_TEXT ) {
			str sText = Managed_ValueToTextDup(objValue);
			xvoTableSetText(tblData, sName, (int)strlen(sName), sText ? sText : (str)"", 0, TRUE);
			if ( sText ) xrtFree(sText);
		}
		return;
	}
	if ( strcmp(sType, "integer") == 0 ) {
		if ( xvoType(objValue) == XVO_DT_FLOAT ) {
			xvoTableSetInt(tblData, sName, (int)strlen(sName), (int64)xvoGetFloat(objValue));
		} else if ( xvoType(objValue) == XVO_DT_TEXT ) {
			xvoTableSetInt(tblData, sName, (int)strlen(sName), atoll(xvoGetText(objValue)));
		}
		return;
	}
	if ( strcmp(sType, "float") == 0 ) {
		if ( xvoType(objValue) == XVO_DT_INT ) {
			xvoTableSetFloat(tblData, sName, (int)strlen(sName), (double)xvoGetInt(objValue));
		} else if ( xvoType(objValue) == XVO_DT_TEXT ) {
			xvoTableSetFloat(tblData, sName, (int)strlen(sName), strtod(xvoGetText(objValue), NULL));
		}
		return;
	}
	if ( strcmp(sType, "boolean") == 0 ) {
		if ( xvoType(objValue) == XVO_DT_INT ) {
			xvoTableSetBool(tblData, sName, (int)strlen(sName), xvoGetInt(objValue) != 0);
		} else if ( xvoType(objValue) == XVO_DT_FLOAT ) {
			xvoTableSetBool(tblData, sName, (int)strlen(sName), xvoGetFloat(objValue) != 0.0);
		} else if ( xvoType(objValue) == XVO_DT_TEXT ) {
			const char* sText = xvoGetText(objValue);
			xvoTableSetBool(
				tblData,
				sName,
				(int)strlen(sName),
				Managed_TextEqualsIgnoreCase(sText, "true") || (strcmp(sText, "1") == 0));
		}
	}
}

void Managed_CoerceFieldValues(xvalue tblData, xvalue tblSpec)
{
	xvalue arrFields = Managed_GetFields(tblSpec);

	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) ) {
		return;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
		Managed_CoerceFieldValue(tblData, xvoArrayGetValue(arrFields, i));
	}
}

bool Managed_ValidateData(xvalue tblSpec, xvalue tblData, bool bSkipRequired, str* psError)
{
	xvalue arrFields = Managed_GetFields(tblSpec);

	if ( psError ) *psError = NULL;
	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("data must be an object", 0);
		return FALSE;
	}
	if ( (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) ) {
		return TRUE;
	}

	for ( int i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
		xvalue tblField = xvoArrayGetValue(arrFields, i);
		const char* sName;
		const char* sTitle;
		const char* sStorageType;
		xvalue objValue;

		if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
			continue;
		}
		sName = xvoTableGetText(tblField, "name", 4);
		sTitle = xvoTableGetText(tblField, "title", 5);
		sStorageType = Managed_GetNestedText(tblField, "storage", "type");
		if ( Managed_IsBlank(sName) ) {
			continue;
		}
		objValue = xvoTableGetValue(tblData, sName, 0);
		if ( !Managed_ValueIsEmpty(objValue) ) {
			if ( !Managed_ValueMatchesStorageType(objValue, sStorageType) ) {
				if ( psError ) {
					*psError = xrtFormat(
						"%s must match storage.type = %s",
						(!Managed_IsBlank(sTitle) ? sTitle : sName),
						Managed_NormalizeStorageTypeName(sStorageType));
				}
				return FALSE;
			}
			continue;
		}
		if ( bSkipRequired ) {
			continue;
		}
		if ( !xvoTableGetBool(tblField, "required", 8) ) {
			continue;
		}
		if ( psError ) {
			*psError = xrtFormat("%s is required", (!Managed_IsBlank(sTitle) ? sTitle : sName));
		}
		return FALSE;
	}
	return TRUE;
}

str Managed_ExtractTitle(xvalue tblData, xvalue tblSpec)
{
	const char* sTitleField = Managed_GetTitleField(tblSpec);
	xvalue objValue = NULL;
	str sTitle = NULL;

	if ( !Managed_IsBlank(sTitleField) ) {
		objValue = xvoTableGetValue(tblData, sTitleField, 0);
	}
	sTitle = Managed_ValueToTextDup(objValue);
	if ( (sTitle == NULL) || Managed_IsBlank((const char*)sTitle) ) {
		if ( sTitle ) xrtFree(sTitle);
		return xrtCopyStr("", 0);
	}
	return sTitle;
}

str Managed_ExtractTextField(xvalue tblData, const char* sFieldName)
{
	xvalue objValue = NULL;
	str sText = NULL;

	if ( Managed_IsBlank(sFieldName) ) {
		return xrtCopyStr("", 0);
	}
	objValue = xvoTableGetValue(tblData, sFieldName, 0);
	sText = Managed_ValueToTextDup(objValue);
	if ( (sText == NULL) || Managed_IsBlank((const char*)sText) ) {
		if ( sText ) xrtFree(sText);
		return xrtCopyStr("", 0);
	}
	return sText;
}

int Managed_ExtractStatus(xvalue tblData, xvalue tblSpec)
{
	const char* sStatusField = Managed_GetStatusField(tblSpec);
	xvalue objValue = NULL;

	if ( Managed_IsBlank(sStatusField) ) {
		return 0;
	}
	objValue = xvoTableGetValue(tblData, sStatusField, 0);
	if ( objValue == NULL ) {
		return 0;
	}
	switch ( xvoType(objValue) ) {
		case XVO_DT_BOOL:
			return xvoGetBool(objValue) ? 1 : 0;
		case XVO_DT_INT:
			return (int)xvoGetInt(objValue);
		case XVO_DT_FLOAT:
			return (int)xvoGetFloat(objValue);
		case XVO_DT_TEXT:
			return atoi(xvoGetText(objValue));
		default:
			return 0;
	}
}

int Managed_NormalizeStatusForSave(xvalue tblSpec, bool bDraft, int iStatus)
{
	const char* sStatusField = Managed_GetStatusField(tblSpec);

	if ( bDraft ) {
		return 0;
	}
	if ( Managed_IsBlank(sStatusField) ) {
		return Managed_PublicStatusThreshold(tblSpec);
	}
	if ( iStatus < 0 ) {
		return 0;
	}
	if ( Managed_StatusFlowNeedsReview(tblSpec) ) {
		if ( iStatus > 2 ) {
			return 2;
		}
		return iStatus;
	}
	return iStatus > 0 ? 1 : 0;
}

void Managed_StoreStatusValue(xvalue tblData, xvalue tblSpec, int iStatus)
{
	const char* sStatusField = Managed_GetStatusField(tblSpec);

	if ( Managed_IsBlank(sStatusField) || (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) ) {
		return;
	}
	xvoTableSetInt(tblData, sStatusField, (int)strlen(sStatusField), iStatus);
}

int64 Managed_ValueToInt64(xvalue objValue)
{
	if ( objValue == NULL ) {
		return 0;
	}
	switch ( xvoType(objValue) ) {
		case XVO_DT_BOOL:
			return xvoGetBool(objValue) ? 1 : 0;
		case XVO_DT_INT:
			return xvoGetInt(objValue);
		case XVO_DT_FLOAT:
			return (int64)xvoGetFloat(objValue);
		case XVO_DT_TEXT:
			return (int64)atoll(xvoGetText(objValue));
		default:
			return 0;
	}
}

void Managed_EnsurePublishedAtValue(xvalue tblData, xvalue tblSpec, bool bDraft, int iStatus, xtime iNow)
{
	const char* sPublishedAtField = Managed_GetPublishedAtField(tblSpec);
	xvalue objValue = NULL;

	if ( bDraft || (iStatus < Managed_PublicStatusThreshold(tblSpec)) || Managed_IsBlank(sPublishedAtField) || (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) ) {
		return;
	}
	objValue = xvoTableGetValue(tblData, sPublishedAtField, 0);
	if ( !Managed_ValueIsEmpty(objValue) && (Managed_ValueToInt64(objValue) > 0) ) {
		return;
	}
	xvoTableSetInt(tblData, sPublishedAtField, (int)strlen(sPublishedAtField), iNow);
}

void Managed_AppendPayload(xvalue tblItem, const char* sPayloadJson)
{
	xvalue tblPayload = NULL;

	if ( sPayloadJson && sPayloadJson[0] ) {
		tblPayload = xrtParseJSON((str)sPayloadJson, strlen(sPayloadJson));
	}
	if ( (tblPayload == NULL) || (xvoType(tblPayload) != XVO_DT_TABLE) ) {
		if ( tblPayload ) xvoUnref(tblPayload);
		tblPayload = xvoCreateTable();
	}
	xvoTableSetValue(tblItem, "data", 4, tblPayload, TRUE);
}

void Managed_AppendDerivedFields(xvalue tblItem, xvalue tblSpec)
{
	xvalue tblData;
	str sSlug = NULL;
	str sSummary = NULL;
	str sCover = NULL;
	const char* sPublishedAtField;
	xvalue objPublishedAt;
	int64 iPublishedAt;

	if ( (tblItem == NULL) || (xvoType(tblItem) != XVO_DT_TABLE) ) {
		return;
	}
	tblData = xvoTableGetValue(tblItem, "data", 4);
	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) ) {
		return;
	}

	sSlug = Managed_ExtractTextField(tblData, Managed_GetSlugField(tblSpec));
	if ( (sSlug != NULL) && !Managed_IsBlank((const char*)sSlug) ) {
		xvoTableSetText(tblItem, "slug", 4, sSlug, 0, TRUE);
	} else if ( sSlug ) {
		xrtFree(sSlug);
	}

	sSummary = Managed_ExtractTextField(tblData, Managed_GetSummaryField(tblSpec));
	if ( (sSummary != NULL) && !Managed_IsBlank((const char*)sSummary) ) {
		xvoTableSetText(tblItem, "summary", 7, sSummary, 0, TRUE);
	} else if ( sSummary ) {
		xrtFree(sSummary);
	}

	sCover = Managed_ExtractTextField(tblData, Managed_GetCoverField(tblSpec));
	if ( (sCover != NULL) && !Managed_IsBlank((const char*)sCover) ) {
		xvoTableSetText(tblItem, "cover", 5, sCover, 0, TRUE);
	} else if ( sCover ) {
		xrtFree(sCover);
	}

	sPublishedAtField = Managed_GetPublishedAtField(tblSpec);
	if ( Managed_IsBlank(sPublishedAtField) ) {
		return;
	}
	objPublishedAt = xvoTableGetValue(tblData, sPublishedAtField, 0);
	iPublishedAt = Managed_ValueToInt64(objPublishedAt);
	if ( iPublishedAt > 0 ) {
		xvoTableSetInt(tblItem, "publishedAt", 11, iPublishedAt);
		Managed_SetTimeText(tblItem, "publishedAtText", 15, iPublishedAt);
	}
}

void Managed_AppendRow(xvalue arrList, sqlite3_stmt* stmt, xvalue tblSpec)
{
	xvalue tblItem = xvoCreateTable();
	const char* sPayload = (const char*)sqlite3_column_text(stmt, 3);

	xvoTableSetInt(tblItem, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetText(tblItem, "title", 5, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
	xvoTableSetInt(tblItem, "status", 6, sqlite3_column_int(stmt, 2));
	Managed_AppendPayload(tblItem, sPayload);
	xvoTableSetBool(tblItem, "isDraft", 7, sqlite3_column_int(stmt, 4) ? TRUE : FALSE);
	Managed_SetTimeText(tblItem, "createTimeText", 14, sqlite3_column_int64(stmt, 5));
	Managed_SetTimeText(tblItem, "updateTimeText", 14, sqlite3_column_int64(stmt, 6));
	xvoTableSetInt(tblItem, "categoryId", 10, sqlite3_column_int(stmt, 7));
	Managed_AppendDerivedFields(tblItem, tblSpec);
	xvoArrayAppendValue(arrList, tblItem, TRUE);
}

bool Managed_ItemMatchesSlug(xvalue tblItem, const char* sSlug)
{
	const char* sItemSlug;

	if ( (tblItem == NULL) || (xvoType(tblItem) != XVO_DT_TABLE) || Managed_IsBlank(sSlug) ) {
		return FALSE;
	}
	sItemSlug = xvoTableGetText(tblItem, "slug", 4);
	return (!Managed_IsBlank(sItemSlug)) && (strcmp(sItemSlug, sSlug) == 0);
}

bool Managed_FieldIsSearchable(xvalue tblField)
{
	return (tblField != NULL) && (xvoType(tblField) == XVO_DT_TABLE) && xvoTableGetBool(tblField, "searchable", 10);
}

bool Managed_FieldIsSortable(xvalue tblField)
{
	return (tblField != NULL) && (xvoType(tblField) == XVO_DT_TABLE) && xvoTableGetBool(tblField, "sortable", 8);
}

bool Managed_FieldIsFilterable(xvalue tblField)
{
	return (tblField != NULL) && (xvoType(tblField) == XVO_DT_TABLE) && xvoTableGetBool(tblField, "filterable", 10);
}

xvalue Managed_GetFieldStorage(xvalue tblField)
{
	return Managed_GetTableValue(tblField, "storage");
}

const char* Managed_GetFieldStorageType(xvalue tblField)
{
	xvalue tblStorage = Managed_GetFieldStorage(tblField);
	return tblStorage ? xvoTableGetText(tblStorage, "type", 4) : NULL;
}

const char* Managed_GetFieldComponentType(xvalue tblField)
{
	xvalue tblComponent = Managed_GetTableValue(tblField, "component");
	return tblComponent ? xvoTableGetText(tblComponent, "type", 4) : NULL;
}

xvalue Managed_FindFieldByName(xvalue tblSpec, const char* sFieldName)
{
	xvalue arrFields = Managed_GetFields(tblSpec);

	if ( (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) || Managed_IsBlank(sFieldName) ) {
		return NULL;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
		xvalue tblField = xvoArrayGetValue(arrFields, i);
		const char* sName = xvoTableGetText(tblField, "name", 4);
		if ( !Managed_IsBlank(sName) && (strcmp(sName, sFieldName) == 0) ) {
			return tblField;
		}
	}
	return NULL;
}

int Managed_CompareTextIgnoreCase(const char* sLeft, const char* sRight)
{
	size_t i = 0;
	const char* a = sLeft ? sLeft : "";
	const char* b = sRight ? sRight : "";

	while ( a[i] && b[i] ) {
		char cLeft = Managed_ToLowerAscii(a[i]);
		char cRight = Managed_ToLowerAscii(b[i]);
		if ( cLeft < cRight ) return -1;
		if ( cLeft > cRight ) return 1;
		i++;
	}
	if ( a[i] == b[i] ) return 0;
	return a[i] ? 1 : -1;
}

int Managed_CompareInt64(int64 iLeft, int64 iRight)
{
	if ( iLeft < iRight ) return -1;
	if ( iLeft > iRight ) return 1;
	return 0;
}

bool Managed_FieldFilterUsesExactMatch(xvalue tblField)
{
	const char* sStorageType = Managed_NormalizeStorageTypeName(Managed_GetFieldStorageType(tblField));
	const char* sComponentType = Managed_GetFieldComponentType(tblField);
	xvalue arrList = Managed_ResolveFieldList(tblField);
	bool bHasList = xvoArrayItemCount(arrList) > 0;

	xvoUnref(arrList);
	if ( bHasList ) {
		return TRUE;
	}
	return (sStorageType && (
			strcmp(sStorageType, "boolean") == 0
			|| strcmp(sStorageType, "integer") == 0
			|| strcmp(sStorageType, "float") == 0
			|| strcmp(sStorageType, "date") == 0
			|| strcmp(sStorageType, "time") == 0
			|| strcmp(sStorageType, "datetime") == 0))
		|| (sComponentType && (
			strcmp(sComponentType, "switch") == 0
			|| strcmp(sComponentType, "select") == 0
			|| strcmp(sComponentType, "radio") == 0
			|| strcmp(sComponentType, "checkbox") == 0
			|| strcmp(sComponentType, "checklist") == 0
			|| strcmp(sComponentType, "date") == 0
			|| strcmp(sComponentType, "time") == 0
			|| strcmp(sComponentType, "datetime") == 0));
}

bool Managed_ObjectMatchesFilterValue(xvalue objValue, const char* sFilterValue, bool bExact)
{
	if ( Managed_IsBlank(sFilterValue) ) {
		return TRUE;
	}
	if ( Managed_ValueIsEmpty(objValue) ) {
		return FALSE;
	}
	if ( xvoType(objValue) == XVO_DT_ARRAY ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(objValue); i++ ) {
			if ( Managed_ObjectMatchesFilterValue(xvoArrayGetValue(objValue, i), sFilterValue, bExact) ) {
				return TRUE;
			}
		}
		return FALSE;
	}
	{
		str sText = Managed_ValueToTextDup(objValue);
		bool bMatch = FALSE;
		if ( sText != NULL ) {
			if ( bExact ) {
				size_t iLeftLen = strlen((const char*)sText);
				size_t iRightLen = strlen(sFilterValue);
				bMatch = (iLeftLen == iRightLen) && Managed_TextContainsIgnoreCase((const char*)sText, sFilterValue);
			} else {
				bMatch = Managed_TextContainsIgnoreCase((const char*)sText, sFilterValue);
			}
			xrtFree(sText);
		}
		return bMatch;
	}
}

bool Managed_FieldValueMatchesFilter(xvalue tblField, xvalue tblData, const char* sFilterValue)
{
	const char* sName;
	xvalue objValue;
	bool bExact;

	if ( Managed_IsBlank(sFilterValue) ) {
		return TRUE;
	}
	if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) || (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) ) {
		return FALSE;
	}
	sName = xvoTableGetText(tblField, "name", 4);
	if ( Managed_IsBlank(sName) ) {
		return FALSE;
	}
	objValue = xvoTableGetValue(tblData, sName, 0);
	bExact = Managed_FieldFilterUsesExactMatch(tblField);
	return Managed_ObjectMatchesFilterValue(objValue, sFilterValue, bExact);
}

int64 Managed_GetSystemSortInt(xvalue tblItem, const char* sSortField)
{
	if ( strcmp(sSortField, "id") == 0 ) return xvoTableGetInt(tblItem, "id", 2);
	if ( strcmp(sSortField, "status") == 0 ) return xvoTableGetInt(tblItem, "status", 6);
	if ( strcmp(sSortField, "createTime") == 0 ) return xvoTableGetInt(tblItem, "createTime", 10);
	if ( strcmp(sSortField, "updateTime") == 0 ) return xvoTableGetInt(tblItem, "updateTime", 10);
	if ( strcmp(sSortField, "publishedAt") == 0 ) return xvoTableGetInt(tblItem, "publishedAt", 11);
	return 0;
}

const char* Managed_GetSystemSortText(xvalue tblItem, const char* sSortField)
{
	if ( strcmp(sSortField, "title") == 0 ) return xvoTableGetText(tblItem, "title", 5);
	if ( strcmp(sSortField, "slug") == 0 ) return xvoTableGetText(tblItem, "slug", 4);
	if ( strcmp(sSortField, "summary") == 0 ) return xvoTableGetText(tblItem, "summary", 7);
	return "";
}

int Managed_CompareItemsBySortField(xvalue tblLeft, xvalue tblRight, xvalue tblSpec, const char* sSortField)
{
	xvalue tblField;

	if ( Managed_IsBlank(sSortField) ) {
		return Managed_CompareInt64(
			xvoTableGetInt(tblLeft, "updateTime", 10),
			xvoTableGetInt(tblRight, "updateTime", 10));
	}
	if ( strcmp(sSortField, "id") == 0
		|| strcmp(sSortField, "status") == 0
		|| strcmp(sSortField, "createTime") == 0
		|| strcmp(sSortField, "updateTime") == 0
		|| strcmp(sSortField, "publishedAt") == 0 ) {
		return Managed_CompareInt64(
			Managed_GetSystemSortInt(tblLeft, sSortField),
			Managed_GetSystemSortInt(tblRight, sSortField));
	}
	if ( strcmp(sSortField, "title") == 0
		|| strcmp(sSortField, "slug") == 0
		|| strcmp(sSortField, "summary") == 0 ) {
		return Managed_CompareTextIgnoreCase(
			Managed_GetSystemSortText(tblLeft, sSortField),
			Managed_GetSystemSortText(tblRight, sSortField));
	}

	tblField = Managed_FindFieldByName(tblSpec, sSortField);
	if ( Managed_FieldIsSortable(tblField) ) {
		xvalue tblLeftData = xvoTableGetValue(tblLeft, "data", 4);
		xvalue tblRightData = xvoTableGetValue(tblRight, "data", 4);
		xvalue objLeft = tblLeftData ? xvoTableGetValue(tblLeftData, sSortField, 0) : NULL;
		xvalue objRight = tblRightData ? xvoTableGetValue(tblRightData, sSortField, 0) : NULL;

		if ( objLeft && objRight ) {
			bool bNumericLeft = (xvoType(objLeft) == XVO_DT_BOOL) || (xvoType(objLeft) == XVO_DT_INT) || (xvoType(objLeft) == XVO_DT_FLOAT);
			bool bNumericRight = (xvoType(objRight) == XVO_DT_BOOL) || (xvoType(objRight) == XVO_DT_INT) || (xvoType(objRight) == XVO_DT_FLOAT);
			if ( bNumericLeft && bNumericRight ) {
				return Managed_CompareInt64(Managed_ValueToInt64(objLeft), Managed_ValueToInt64(objRight));
			}
		}
		{
			str sLeft = tblLeftData ? Managed_BuildListFieldSummary(tblField, tblLeftData) : NULL;
			str sRight = tblRightData ? Managed_BuildListFieldSummary(tblField, tblRightData) : NULL;
			int iCmp = Managed_CompareTextIgnoreCase(
				sLeft ? (const char*)sLeft : "",
				sRight ? (const char*)sRight : "");
			if ( sLeft ) xrtFree(sLeft);
			if ( sRight ) xrtFree(sRight);
			return iCmp;
		}
	}
	return Managed_CompareInt64(
		xvoTableGetInt(tblLeft, "updateTime", 10),
		xvoTableGetInt(tblRight, "updateTime", 10));
}

void Managed_SortItems(xvalue arrItems, xvalue tblSpec, const char* sSortField, bool bAsc)
{
	uint32 iCount;

	if ( (arrItems == NULL) || (xvoType(arrItems) != XVO_DT_ARRAY) ) {
		return;
	}
	iCount = xvoArrayItemCount(arrItems);
	if ( iCount <= 1 ) {
		return;
	}
	for ( uint32 i = 0; i < iCount; i++ ) {
		for ( uint32 j = i + 1; j < iCount; j++ ) {
			xvalue tblLeft = xvoArrayGetValue(arrItems, i);
			xvalue tblRight = xvoArrayGetValue(arrItems, j);
			int iCmp = Managed_CompareItemsBySortField(tblLeft, tblRight, tblSpec, sSortField);
			bool bSwap = bAsc ? (iCmp > 0) : (iCmp < 0);
			if ( (!bSwap) && (iCmp == 0) ) {
				int iTimeCmp = Managed_CompareInt64(
					xvoTableGetInt(tblLeft, "updateTime", 10),
					xvoTableGetInt(tblRight, "updateTime", 10));
				bSwap = bAsc ? (iTimeCmp > 0) : (iTimeCmp < 0);
			}
			if ( bSwap ) {
				xvoArraySwap(arrItems, i, j);
			}
		}
	}
}

bool Managed_RowMatchesQuery(xvalue tblItem, xvalue tblSpec, const char* sQuery)
{
	xvalue tblData;
	xvalue arrFields;
	const char* sTitle;
	const char* sSlug;
	const char* sSummary;

	if ( Managed_IsBlank(sQuery) ) {
		return TRUE;
	}
	if ( (tblItem == NULL) || (xvoType(tblItem) != XVO_DT_TABLE) ) {
		return FALSE;
	}
	sTitle = xvoTableGetText(tblItem, "title", 5);
	sSlug = xvoTableGetText(tblItem, "slug", 4);
	sSummary = xvoTableGetText(tblItem, "summary", 7);
	if ( Managed_TextContainsIgnoreCase(sTitle, sQuery)
		|| Managed_TextContainsIgnoreCase(sSlug, sQuery)
		|| Managed_TextContainsIgnoreCase(sSummary, sQuery) ) {
		return TRUE;
	}
	tblData = xvoTableGetValue(tblItem, "data", 4);
	arrFields = Managed_GetFields(tblSpec);
	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) ) {
		return FALSE;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
		xvalue tblField = xvoArrayGetValue(arrFields, i);
		const char* sName;
		xvalue objValue;
		str sText = NULL;

		if ( !Managed_FieldIsSearchable(tblField) ) {
			continue;
		}
		sName = xvoTableGetText(tblField, "name", 4);
		if ( Managed_IsBlank(sName) ) {
			continue;
		}
		objValue = xvoTableGetValue(tblData, sName, 0);
		if ( Managed_ValueIsEmpty(objValue) ) {
			continue;
		}
		sText = Managed_BuildListFieldSummary(tblField, tblData);
		if ( (sText != NULL) && Managed_TextContainsIgnoreCase((const char*)sText, sQuery) ) {
			xrtFree(sText);
			return TRUE;
		}
		if ( sText ) {
			xrtFree(sText);
		}
	}
	return FALSE;
}

bool Managed_RowMatchesFieldFilters(xvalue tblItem, xvalue tblSpec, XS_RequestObject objReq)
{
	xvalue arrFields;
	xvalue tblData;

	if ( (tblItem == NULL) || (xvoType(tblItem) != XVO_DT_TABLE) ) {
		return FALSE;
	}
	arrFields = Managed_GetFields(tblSpec);
	tblData = xvoTableGetValue(tblItem, "data", 4);
	if ( (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) || (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) ) {
		return TRUE;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
		xvalue tblField = xvoArrayGetValue(arrFields, i);
		const char* sName;
		str sQueryKey;
		char sFilterValue[160];

		if ( !Managed_FieldIsFilterable(tblField) ) {
			continue;
		}
		sName = xvoTableGetText(tblField, "name", 4);
		if ( Managed_IsBlank(sName) ) {
			continue;
		}
		sQueryKey = xrtFormat("f_%s", sName);
		if ( sQueryKey == NULL ) {
			continue;
		}
		Managed_ReadTextQuery(objReq, sQueryKey, sFilterValue, sizeof(sFilterValue));
		xrtFree(sQueryKey);
		if ( Managed_IsBlank(sFilterValue) ) {
			continue;
		}
		if ( !Managed_FieldValueMatchesFilter(tblField, tblData, sFilterValue) ) {
			return FALSE;
		}
	}
	return TRUE;
}

bool Managed_RowMatchesFilters(xvalue tblItem, xvalue tblSpec, XS_RequestObject objReq, const char* sQuery, bool bAdmin, int iStatusFilter, int iDraftFilter)
{
	char sCategoryId[32];
	int iCategoryId = 0;

	if ( (tblItem == NULL) || (xvoType(tblItem) != XVO_DT_TABLE) ) {
		return FALSE;
	}
	memset(sCategoryId, 0, sizeof(sCategoryId));
	if ( objReq ) {
		xsReqQueryValue(objReq, "categoryId", sCategoryId, sizeof(sCategoryId));
	}
	if ( sCategoryId[0] != '\0' ) {
		iCategoryId = atoi(sCategoryId);
		if ( (iCategoryId > 0) && (xvoTableGetInt(tblItem, "categoryId", 10) != iCategoryId) ) {
			return FALSE;
		}
	}
	if ( (iStatusFilter != 0x7fffffff) && (xvoTableGetInt(tblItem, "status", 6) != iStatusFilter) ) {
		return FALSE;
	}
	if ( bAdmin && (iDraftFilter >= 0) ) {
		bool bDraft = xvoTableGetBool(tblItem, "isDraft", 7);
		if ( (iDraftFilter == 1) && !bDraft ) return FALSE;
		if ( (iDraftFilter == 0) && bDraft ) return FALSE;
	}
	if ( !Managed_RowMatchesFieldFilters(tblItem, tblSpec, objReq) ) {
		return FALSE;
	}
	return Managed_RowMatchesQuery(tblItem, tblSpec, sQuery);
}

int Managed_QueryCount(sqlite3* pDb, xvalue tblSpec, bool bAdmin)
{
	sqlite3_stmt* stmt = NULL;
	int iCount = 0;
	str sSql = bAdmin
		? xrtCopyStr("SELECT COUNT(*) FROM content_item WHERE delete_time = 0", 0)
		: xrtFormat("SELECT COUNT(*) FROM content_item WHERE delete_time = 0 AND is_draft = 0 AND status >= %d", Managed_PublicStatusThreshold(tblSpec));

	if ( (pDb == NULL) || (sSql == NULL) || (sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) != SQLITE_OK) ) {
		if ( sSql ) xrtFree(sSql);
		return 0;
	}
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iCount = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	if ( sSql ) xrtFree(sSql);
	return iCount;
}

void Managed_RequestMeta(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue tblData = xvoCreateTable();
	xvalue tblSpec = Managed_LoadSpec();
	xvalue tblManaged = Managed_LoadManagedMeta();
	xvalue tblContracts = Managed_LoadContractsMeta();
	xvalue tblCustomMounts = Managed_LoadCustomMounts();
	xvalue tblMounts = Managed_BuildMountRegistry(tblContracts, tblCustomMounts);

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	xvoTableSetText(tblData, "pluginXid", 9, "{{PLUGIN_XID}}", 0, FALSE);
	xvoTableSetText(tblData, "title", 5, (str)Managed_GetPluginTitle(tblSpec), 0, FALSE);
	xvoTableSetBool(tblData, "draftEnabled", 12, Managed_DraftEnabled(tblSpec));
	if ( tblSpec ) xvoTableSetValue(tblData, "spec", 4, tblSpec, TRUE);
	if ( tblManaged ) xvoTableSetValue(tblData, "managed", 7, tblManaged, TRUE);
	if ( tblContracts ) xvoTableSetValue(tblData, "contracts", 9, tblContracts, TRUE);
	if ( tblMounts ) xvoTableSetValue(tblData, "mounts", 6, tblMounts, TRUE);
	if ( tblCustomMounts ) xvoUnref(tblCustomMounts);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestContractsPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue tblContracts = Managed_LoadContractsMeta();
	xvalue tblCustomMounts = Managed_LoadCustomMounts();
	xvalue tblMounts = Managed_BuildMountRegistry(tblContracts, tblCustomMounts);

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	if ( (tblContracts == NULL) && (tblMounts == NULL) ) {
		Managed_SendError(objResp, "contracts.json is missing");
		return;
	}
	if ( tblContracts ) {
		xvoTableSetValue(tblRet, "data", 4, tblContracts, TRUE);
	}
	if ( tblMounts ) {
		xvoTableSetValue(tblRet, "mounts", 6, tblMounts, TRUE);
	}
	if ( tblCustomMounts ) xvoUnref(tblCustomMounts);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestContractsAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblRet = NULL;
	xvalue tblContracts = NULL;
	xvalue tblCustomMounts = NULL;
	xvalue tblMounts = NULL;
	xvalue tblMountSample = NULL;
	xvalue arrProviders = NULL;
	xvalue tblProviderDiscovery = NULL;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xvalue tblForm = Managed_ParseJsonBody(objReq);
		str sError = NULL;

		if ( tblForm == NULL ) {
			Managed_SendError(objResp, "invalid json body");
			return;
		}
		if ( !Managed_ValidateCustomMounts(tblForm, &sError) ) {
			xvoUnref(tblForm);
			Managed_SendError(objResp, sError ? (const char*)sError : "mount validation failed");
			if ( sError ) xrtFree(sError);
			return;
		}
		if ( !Managed_SaveCustomMountsFile(tblForm) ) {
			xvoUnref(tblForm);
			Managed_SendError(objResp, "failed to save capability.mounts.json");
			return;
		}
		xvoUnref(tblForm);
		tblRet = Managed_CreateResult(TRUE, "mount registry saved");
	} else {
		tblRet = Managed_CreateResult(TRUE, NULL);
	}

	tblContracts = Managed_LoadContractsMeta();
	tblCustomMounts = Managed_LoadCustomMounts();
	tblMounts = Managed_BuildMountRegistry(tblContracts, tblCustomMounts);
	tblMountSample = Managed_LoadMountSample();
	arrProviders = Managed_ScanCapabilityProviders();
	tblProviderDiscovery = Managed_BuildProviderSuggestions(tblContracts, arrProviders);

	if ( (tblContracts == NULL) && (tblMounts == NULL) ) {
		if ( tblRet ) xvoUnref(tblRet);
		Managed_SendError(objResp, "contracts.json is missing");
		return;
	}
	if ( tblContracts ) {
		xvoTableSetValue(tblRet, "data", 4, tblContracts, TRUE);
	}
	if ( tblMounts ) {
		xvoTableSetValue(tblRet, "mounts", 6, tblMounts, TRUE);
	}
	xvoTableSetValue(tblRet, "customMounts", 12, Managed_CopyMountRoot(tblCustomMounts), TRUE);
	if ( tblMountSample ) {
		xvoTableSetValue(tblRet, "mountSample", 11, tblMountSample, TRUE);
	}
	if ( tblProviderDiscovery ) {
		xvoTableSetValue(tblRet, "providerDiscovery", 17, tblProviderDiscovery, TRUE);
	}
	if ( arrProviders ) xvoUnref(arrProviders);
	if ( tblCustomMounts ) xvoUnref(tblCustomMounts);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestFormMetaAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue tblData = xvoCreateTable();
	xvalue tblSpec = Managed_LoadSpec();
	xvalue tblManaged = Managed_LoadManagedMeta();
	xvalue tblContracts = Managed_LoadContractsMeta();
	xvalue tblCustomMounts = Managed_LoadCustomMounts();
	xvalue tblMounts = Managed_BuildMountRegistry(tblContracts, tblCustomMounts);
	xvalue tblMountSample = Managed_LoadMountSample();
	xvalue arrProviders = Managed_ScanCapabilityProviders();
	xvalue tblProviderDiscovery = Managed_BuildProviderSuggestions(tblContracts, arrProviders);

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	xvoTableSetText(tblData, "pluginXid", 9, "{{PLUGIN_XID}}", 0, FALSE);
	xvoTableSetText(tblData, "title", 5, (str)Managed_GetPluginTitle(tblSpec), 0, FALSE);
	xvoTableSetBool(tblData, "draftEnabled", 12, Managed_DraftEnabled(tblSpec));
	xvoTableSetValue(tblData, "fieldTypes", 10, xvoCreateTable(), TRUE);
	if ( tblSpec ) {
		xvoTableSetValue(tblData, "spec", 4, xvoCopy(tblSpec), TRUE);
		xvoTableSetValue(tblData, "schema", 6, Managed_BuildFormSchema(tblSpec), TRUE);
		xvoUnref(tblSpec);
	}
	if ( tblManaged ) xvoTableSetValue(tblData, "managed", 7, tblManaged, TRUE);
	if ( tblContracts ) xvoTableSetValue(tblData, "contracts", 9, tblContracts, TRUE);
	if ( tblMounts ) xvoTableSetValue(tblData, "mounts", 6, tblMounts, TRUE);
	xvoTableSetValue(tblData, "customMounts", 12, Managed_CopyMountRoot(tblCustomMounts), TRUE);
	if ( tblMountSample ) xvoTableSetValue(tblData, "mountSample", 11, tblMountSample, TRUE);
	if ( tblProviderDiscovery ) xvoTableSetValue(tblData, "providerDiscovery", 17, tblProviderDiscovery, TRUE);
	if ( arrProviders ) xvoUnref(arrProviders);
	if ( tblCustomMounts ) xvoUnref(tblCustomMounts);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestListCommon(XS_ResponseObject objResp, XS_RequestObject objReq, bool bAdmin, int iForcedDraftFilter)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	xvalue arrList = NULL;
	xvalue arrMatched = NULL;
	xvalue tblSpec = Managed_LoadSpec();
	char sQuery[160];
	char sStatus[32];
	char sDraft[32];
	char sSortBy[32];
	char sSortDir[16];
	int iPage = Managed_ReadIntQuery(objReq, "page", 1);
	int iLimit = Managed_ReadIntQuery(objReq, "limit", Managed_GetUiListPageSize(tblSpec, G_Config.iPageSize > 0 ? G_Config.iPageSize : 20));
	int iOffset;
	int iCount;
	int iStatusFilter = 0x7fffffff;
	int iDraftFilter = iForcedDraftFilter;
	const char* sRequestedSortField = NULL;
	const char* sSqlSortField = NULL;
	bool bSortAsc = FALSE;
	str sSql = NULL;

	if ( iPage < 1 ) iPage = 1;
	if ( iLimit < 1 ) iLimit = 20;
	if ( iLimit > 200 ) iLimit = 200;
	iOffset = (iPage - 1) * iLimit;
	Managed_ReadTextQuery(objReq, "q", sQuery, sizeof(sQuery));
	Managed_ReadTextQuery(objReq, "status", sStatus, sizeof(sStatus));
	Managed_ReadTextQuery(objReq, "draft", sDraft, sizeof(sDraft));
	Managed_ReadTextQuery(objReq, "sortBy", sSortBy, sizeof(sSortBy));
	Managed_ReadTextQuery(objReq, "sortDir", sSortDir, sizeof(sSortDir));
	if ( sStatus[0] != '\0' ) {
		iStatusFilter = atoi(sStatus);
	}
	if ( bAdmin && (iForcedDraftFilter < 0) && (sDraft[0] != '\0') ) {
		iDraftFilter = atoi(sDraft);
	}
	sRequestedSortField = !Managed_IsBlank(sSortBy) ? sSortBy : Managed_GetUiListSortField(tblSpec);
	sSqlSortField = Managed_NormalizeListSortField(sRequestedSortField);
	if ( Managed_IsBlank(sSortBy) ) {
		bSortAsc = Managed_GetUiListSortAsc(tblSpec);
	} else {
		bSortAsc = strcmp(sSortDir, "asc") == 0;
	}
	sSql = Managed_SelectListSql(tblSpec, bAdmin, sSqlSortField, bSortAsc);

	if ( (sSql == NULL) || !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( sSql ) xrtFree(sSql);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	iCount = 0;
	tblRet = Managed_CreateResult(TRUE, NULL);
	arrList = xvoCreateArray();
	arrMatched = xvoCreateArray();
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue arrRow = xvoCreateArray();
			xvalue tblItem;
			Managed_AppendRow(arrRow, stmt, tblSpec);
			tblItem = (xvoArrayItemCount(arrRow) > 0) ? xvoArrayGetValue(arrRow, 0) : NULL;
			if ( Managed_RowMatchesFilters(tblItem, tblSpec, objReq, sQuery, bAdmin, iStatusFilter, iDraftFilter) ) {
				xvoArrayAppendValue(arrMatched, xvoCopy(tblItem), TRUE);
			}
			xvoUnref(arrRow);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	if ( sSql ) xrtFree(sSql);
	Managed_SortItems(arrMatched, tblSpec, sRequestedSortField, bSortAsc);
	iCount = xvoArrayItemCount(arrMatched);
	for ( uint32 i = (uint32)iOffset; (i < xvoArrayItemCount(arrMatched)) && (xvoArrayItemCount(arrList) < (uint32)iLimit); i++ ) {
		xvoArrayAppendValue(arrList, xvoCopy(xvoArrayGetValue(arrMatched, i)), TRUE);
	}
	xvoUnref(arrMatched);
	if ( tblSpec ) xvoUnref(tblSpec);

	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetInt(tblRet, "page", 4, iPage);
	xvoTableSetInt(tblRet, "pageSize", 8, iLimit);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestListPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;
	Managed_RequestListCommon(objResp, objReq, FALSE, 0);
}

void Managed_RequestListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;
	Managed_RequestListCommon(objResp, objReq, TRUE, 0);
}

void Managed_RequestDraftsAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;
	Managed_RequestListCommon(objResp, objReq, TRUE, 1);
}

void Managed_RequestDetailCommon(XS_ResponseObject objResp, XS_RequestObject objReq, bool bAdmin)
{
	char sId[32];
	char sSlug[160];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	xvalue tblData = NULL;
	xvalue tblSpec = Managed_LoadSpec();
	str sSqlById = NULL;
	str sSqlScan = NULL;

	memset(sId, 0, sizeof(sId));
	memset(sSlug, 0, sizeof(sSlug));
	sSqlById = bAdmin
		? xrtCopyStr("SELECT id, title, status, payload_json, is_draft, create_time, update_time, category_id FROM content_item WHERE id = ? AND delete_time = 0", 0)
		: xrtFormat("SELECT id, title, status, payload_json, is_draft, create_time, update_time, category_id FROM content_item WHERE id = ? AND delete_time = 0 AND is_draft = 0 AND status >= %d", Managed_PublicStatusThreshold(tblSpec));
	sSqlScan = bAdmin
		? xrtCopyStr("SELECT id, title, status, payload_json, is_draft, create_time, update_time, category_id FROM content_item WHERE delete_time = 0 ORDER BY update_time DESC, id DESC", 0)
		: xrtFormat("SELECT id, title, status, payload_json, is_draft, create_time, update_time, category_id FROM content_item WHERE delete_time = 0 AND is_draft = 0 AND status >= %d ORDER BY update_time DESC, id DESC", Managed_PublicStatusThreshold(tblSpec));
	xsReqQueryValue(objReq, "id", sId, sizeof(sId));
	xsReqQueryValue(objReq, "slug", sSlug, sizeof(sSlug));
	if ( (sId[0] == '\0') && (sSlug[0] == '\0') ) {
		if ( sSqlById ) xrtFree(sSqlById);
		if ( sSqlScan ) xrtFree(sSqlScan);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "id or slug is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( sSqlById ) xrtFree(sSqlById);
		if ( sSqlScan ) xrtFree(sSqlScan);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, (sId[0] != '\0') ? sSqlById : sSqlScan, -1, &stmt, NULL) != SQLITE_OK ) {
		Managed_CloseDb(pDb);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "failed to prepare query");
		return;
	}
	if ( sId[0] != '\0' ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)atoll(sId));
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue arrData = xvoCreateArray();
			Managed_AppendRow(arrData, stmt, tblSpec);
			if ( xvoArrayItemCount(arrData) > 0 ) {
				tblData = xvoCopy(xvoArrayGetValue(arrData, 0));
			}
			xvoUnref(arrData);
		}
	} else {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue arrData = xvoCreateArray();
			Managed_AppendRow(arrData, stmt, tblSpec);
			if ( (xvoArrayItemCount(arrData) > 0) && Managed_ItemMatchesSlug(xvoArrayGetValue(arrData, 0), sSlug) ) {
				tblData = xvoCopy(xvoArrayGetValue(arrData, 0));
				xvoUnref(arrData);
				break;
			}
			xvoUnref(arrData);
		}
	}
	sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	if ( sSqlById ) xrtFree(sSqlById);
	if ( sSqlScan ) xrtFree(sSqlScan);
	if ( tblSpec ) xvoUnref(tblSpec);
	if ( tblData == NULL ) {
		Managed_SendError(objResp, "content item not found");
		return;
	}
	tblRet = Managed_CreateResult(TRUE, NULL);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestDetailPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;
	Managed_RequestDetailCommon(objResp, objReq, FALSE);
}

void Managed_RequestGetAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;
	Managed_RequestDetailCommon(objResp, objReq, TRUE);
}

void Managed_RequestSave(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm = NULL;
	xvalue tblData = NULL;
	xvalue tblSpec = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	str sPayloadJson = NULL;
	str sTitle = NULL;
	str sError = NULL;
	int64 iId = 0;
	int iCategoryId = 0;
	int iStatus = 0;
	bool bDraft = FALSE;
	xtime iNow = xrtNow();
	xvalue tblRet = NULL;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		Managed_SendError(objResp, "method not allowed");
		return;
	}
	tblForm = Managed_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		Managed_SendError(objResp, "invalid json body");
		return;
	}
	tblData = xvoTableGetValue(tblForm, "data", 4);
	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) ) {
		xvoUnref(tblForm);
		Managed_SendError(objResp, "data is required");
		return;
	}
	tblSpec = Managed_LoadSpec();
	if ( tblSpec == NULL ) {
		xvoUnref(tblForm);
		Managed_SendError(objResp, "spec.json is missing");
		return;
	}
	Managed_ApplyMissingDefaults(tblData, tblSpec);
	Managed_NormalizeNullableFields(tblData, tblSpec);
	Managed_CoerceFieldValues(tblData, tblSpec);
	bDraft = Managed_DraftEnabled(tblSpec) ? xvoTableGetBool(tblForm, "isDraft", 7) : FALSE;
	if ( !Managed_ValidateData(tblSpec, tblData, bDraft, &sError) ) {
		xvoUnref(tblSpec);
		xvoUnref(tblForm);
		Managed_SendError(objResp, sError ? (const char*)sError : "validation failed");
		if ( sError ) xrtFree(sError);
		return;
	}

	iId = xvoTableGetInt(tblForm, "id", 2);
	iCategoryId = (int)xvoTableGetInt(tblForm, "categoryId", 10);
	if ( iCategoryId <= 0 ) {
		iCategoryId = (int)xvoTableGetInt(tblData, "categoryId", 10);
	}
	sTitle = Managed_ExtractTitle(tblData, tblSpec);
	iStatus = Managed_NormalizeStatusForSave(tblSpec, bDraft, Managed_ExtractStatus(tblData, tblSpec));
	Managed_StoreStatusValue(tblData, tblSpec, iStatus);
	Managed_EnsurePublishedAtValue(tblData, tblSpec, bDraft, iStatus, iNow);
	sPayloadJson = xrtStringifyJSON(tblData, FALSE, NULL);

	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblSpec);
		xvoUnref(tblForm);
		if ( sTitle ) xrtFree(sTitle);
		if ( sPayloadJson ) xrtFree(sPayloadJson);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}

	if ( iId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE content_item SET title = ?, status = ?, payload_json = ?, category_id = ?, is_draft = ?, update_time = ? WHERE id = ? AND delete_time = 0", -1, &stmt, NULL) != SQLITE_OK ) {
			Managed_CloseDb(pDb);
			xvoUnref(tblSpec);
			xvoUnref(tblForm);
			if ( sTitle ) xrtFree(sTitle);
			if ( sPayloadJson ) xrtFree(sPayloadJson);
			Managed_SendError(objResp, "failed to prepare update");
			return;
		}
		sqlite3_bind_text(stmt, 1, sTitle ? (const char*)sTitle : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 2, iStatus);
		sqlite3_bind_text(stmt, 3, sPayloadJson ? (const char*)sPayloadJson : "{}", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 4, iCategoryId > 0 ? iCategoryId : 0);
		sqlite3_bind_int(stmt, 5, bDraft ? 1 : 0);
		sqlite3_bind_int64(stmt, 6, iNow);
		sqlite3_bind_int64(stmt, 7, (sqlite3_int64)iId);
		sqlite3_step(stmt);
	} else {
		if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_item(title, status, payload_json, category_id, is_draft, create_time, update_time, delete_time) VALUES(?, ?, ?, ?, ?, ?, ?, 0)", -1, &stmt, NULL) != SQLITE_OK ) {
			Managed_CloseDb(pDb);
			xvoUnref(tblSpec);
			xvoUnref(tblForm);
			if ( sTitle ) xrtFree(sTitle);
			if ( sPayloadJson ) xrtFree(sPayloadJson);
			Managed_SendError(objResp, "failed to prepare insert");
			return;
		}
		sqlite3_bind_text(stmt, 1, sTitle ? (const char*)sTitle : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 2, iStatus);
		sqlite3_bind_text(stmt, 3, sPayloadJson ? (const char*)sPayloadJson : "{}", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 4, iCategoryId > 0 ? iCategoryId : 0);
		sqlite3_bind_int(stmt, 5, bDraft ? 1 : 0);
		sqlite3_bind_int64(stmt, 6, iNow);
		sqlite3_bind_int64(stmt, 7, iNow);
		sqlite3_step(stmt);
		iId = sqlite3_last_insert_rowid(pDb);
	}

	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoUnref(tblSpec);
	xvoUnref(tblForm);
	if ( sTitle ) xrtFree(sTitle);
	if ( sPayloadJson ) xrtFree(sPayloadJson);

	tblRet = Managed_CreateResult(TRUE, "saved");
	xvoTableSetInt(tblRet, "id", 2, iId);
	xvoTableSetInt(tblRet, "status", 6, iStatus);
	xvoTableSetInt(tblRet, "categoryId", 10, iCategoryId > 0 ? iCategoryId : 0);
	xvoTableSetBool(tblRet, "isDraft", 7, bDraft);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestDelete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = 0;
	xvalue tblRet = NULL;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		Managed_SendError(objResp, "method not allowed");
		return;
	}
	tblForm = Managed_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		Managed_SendError(objResp, "invalid json body");
		return;
	}
	iId = xvoTableGetInt(tblForm, "id", 2);
	if ( iId <= 0 ) {
		xvoUnref(tblForm);
		Managed_SendError(objResp, "id is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblForm);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_item SET delete_time = ?, update_time = ? WHERE id = ? AND delete_time = 0", -1, &stmt, NULL) == SQLITE_OK ) {
		xtime iNow = xrtNow();
		sqlite3_bind_int64(stmt, 1, iNow);
		sqlite3_bind_int64(stmt, 2, iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoUnref(tblForm);

	tblRet = Managed_CreateResult(TRUE, "deleted");
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_AppendCategoryRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblItem = xvoCreateTable();

	xvoTableSetInt(tblItem, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblItem, "parentId", 8, sqlite3_column_int(stmt, 1));
	xvoTableSetText(tblItem, "title", 5, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetText(tblItem, "slug", 4, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetText(tblItem, "path", 4, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetInt(tblItem, "level", 5, sqlite3_column_int(stmt, 5));
	xvoTableSetInt(tblItem, "sort", 4, sqlite3_column_int(stmt, 6));
	xvoTableSetInt(tblItem, "status", 6, sqlite3_column_int(stmt, 7));
	Managed_SetTimeText(tblItem, "createTimeText", 14, sqlite3_column_int64(stmt, 8));
	Managed_SetTimeText(tblItem, "updateTimeText", 14, sqlite3_column_int64(stmt, 9));
	xvoTableSetInt(tblItem, "contentCount", 12, sqlite3_column_int(stmt, 10));
	xvoArrayAppendValue(arrList, tblItem, TRUE);
}

void Managed_RequestCategoryListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();
	int iCount = 0;
	const char* sSql =
		"SELECT c.id, c.parent_id, c.title, c.slug, c.path, c.level, c.sort, c.status, c.create_time, c.update_time, "
		"(SELECT COUNT(*) FROM content_item i WHERE i.category_id = c.id AND i.delete_time = 0) AS content_count "
		"FROM content_category c WHERE c.delete_time = 0 ORDER BY c.parent_id ASC, c.sort ASC, c.id ASC";

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendCategoryRow(arrList, stmt);
			iCount++;
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestCategoryGetAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	xvalue arrList = xvoCreateArray();
	char sId[32];
	int64 iId = 0;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	memset(sId, 0, sizeof(sId));
	xsReqQueryValue(objReq, "id", sId, sizeof(sId));
	iId = atoll(sId);
	if ( iId <= 0 ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "id is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb,
		"SELECT c.id, c.parent_id, c.title, c.slug, c.path, c.level, c.sort, c.status, c.create_time, c.update_time, "
		"(SELECT COUNT(*) FROM content_item i WHERE i.category_id = c.id AND i.delete_time = 0) AS content_count "
		"FROM content_category c WHERE c.id = ? AND c.delete_time = 0",
		-1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendCategoryRow(arrList, stmt);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	if ( xvoArrayItemCount(arrList) <= 0 ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "category not found");
		return;
	}
	tblRet = Managed_CreateResult(TRUE, NULL);
	xvoTableSetValue(tblRet, "data", 4, xvoArrayGetValue(arrList, 0), TRUE);
	xvoUnref(arrList);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestCategorySaveAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm = NULL;
	xvalue tblRet = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = 0;
	int iParentId = 0;
	int iSort = 0;
	int iStatus = 1;
	xtime iNow = xrtNow();
	const char* sTitle;
	const char* sSlug;
	str sPath = NULL;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		Managed_SendError(objResp, "method not allowed");
		return;
	}
	tblForm = Managed_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		Managed_SendError(objResp, "invalid json body");
		return;
	}
	sTitle = xvoTableGetText(tblForm, "title", 5);
	sSlug = xvoTableGetText(tblForm, "slug", 4);
	if ( Managed_IsBlank(sTitle) || Managed_IsBlank(sSlug) ) {
		xvoUnref(tblForm);
		Managed_SendError(objResp, "title and slug are required");
		return;
	}
	iId = xvoTableGetInt(tblForm, "id", 2);
	iParentId = (int)xvoTableGetInt(tblForm, "parentId", 8);
	iSort = (int)xvoTableGetInt(tblForm, "sort", 4);
	iStatus = (int)xvoTableGetInt(tblForm, "status", 6);
	if ( iStatus <= 0 ) iStatus = 1;
	sPath = xrtFormat("/%s/", sSlug);
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblForm);
		if ( sPath ) xrtFree(sPath);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( iId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE content_category SET parent_id = ?, title = ?, slug = ?, path = ?, level = 0, sort = ?, status = ?, update_time = ? WHERE id = ? AND delete_time = 0", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int(stmt, 1, iParentId > 0 ? iParentId : 0);
			sqlite3_bind_text(stmt, 2, sTitle, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sSlug, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, sPath ? (const char*)sPath : "/", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 5, iSort);
			sqlite3_bind_int(stmt, 6, iStatus);
			sqlite3_bind_int64(stmt, 7, iNow);
			sqlite3_bind_int64(stmt, 8, (sqlite3_int64)iId);
			sqlite3_step(stmt);
		}
	} else {
		if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_category(parent_id, title, slug, path, level, sort, status, create_time, update_time, delete_time) VALUES(?, ?, ?, ?, 0, ?, ?, ?, ?, 0)", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int(stmt, 1, iParentId > 0 ? iParentId : 0);
			sqlite3_bind_text(stmt, 2, sTitle, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sSlug, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, sPath ? (const char*)sPath : "/", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 5, iSort);
			sqlite3_bind_int(stmt, 6, iStatus);
			sqlite3_bind_int64(stmt, 7, iNow);
			sqlite3_bind_int64(stmt, 8, iNow);
			sqlite3_step(stmt);
			iId = sqlite3_last_insert_rowid(pDb);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoUnref(tblForm);
	if ( sPath ) xrtFree(sPath);
	tblRet = Managed_CreateResult(TRUE, "saved");
	xvoTableSetInt(tblRet, "id", 2, iId);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestCategoryDeleteAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm = NULL;
	xvalue tblRet = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = 0;
	xtime iNow = xrtNow();

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		Managed_SendError(objResp, "method not allowed");
		return;
	}
	tblForm = Managed_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		Managed_SendError(objResp, "invalid json body");
		return;
	}
	iId = xvoTableGetInt(tblForm, "id", 2);
	if ( iId <= 0 ) {
		xvoUnref(tblForm);
		Managed_SendError(objResp, "id is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblForm);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_category SET delete_time = ?, update_time = ? WHERE id = ? AND delete_time = 0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, iNow);
		sqlite3_bind_int64(stmt, 2, iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoUnref(tblForm);
	tblRet = Managed_CreateResult(TRUE, "deleted");
	xvoTableSetInt(tblRet, "id", 2, iId);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestCategorySortAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm = NULL;
	xvalue arrItems = NULL;
	xvalue tblRet = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xtime iNow = xrtNow();
	int iUpdated = 0;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		Managed_SendError(objResp, "method not allowed");
		return;
	}
	tblForm = Managed_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		Managed_SendError(objResp, "invalid json body");
		return;
	}
	arrItems = xvoTableGetValue(tblForm, "items", 5);
	if ( (arrItems == NULL) || (xvoType(arrItems) != XVO_DT_ARRAY) ) {
		xvoUnref(tblForm);
		Managed_SendError(objResp, "items is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblForm);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_category SET sort = ?, update_time = ? WHERE id = ? AND delete_time = 0", -1, &stmt, NULL) == SQLITE_OK ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrItems); i++ ) {
			xvalue tblItem = xvoArrayGetValue(arrItems, i);
			int64 iId;
			int iSort;
			if ( (tblItem == NULL) || (xvoType(tblItem) != XVO_DT_TABLE) ) {
				continue;
			}
			iId = xvoTableGetInt(tblItem, "id", 2);
			iSort = (int)xvoTableGetInt(tblItem, "sort", 4);
			if ( iId <= 0 ) {
				continue;
			}
			sqlite3_reset(stmt);
			sqlite3_clear_bindings(stmt);
			sqlite3_bind_int(stmt, 1, iSort);
			sqlite3_bind_int64(stmt, 2, iNow);
			sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
			if ( sqlite3_step(stmt) == SQLITE_DONE ) {
				iUpdated++;
			}
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoUnref(tblForm);
	tblRet = Managed_CreateResult(TRUE, "saved");
	xvoTableSetInt(tblRet, "updated", 7, iUpdated);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestAdminView(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	if ( !Managed_SendAssetHtml(objResp, "generated/admin.html") ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "managed admin page missing", 0);
	}
}

void Managed_RequestDraftsView(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	if ( !Managed_SendAssetHtml(objResp, "generated/drafts.html") ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "managed drafts page missing", 0);
	}
}

void Managed_RequestEditorView(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	if ( !Managed_SendAssetHtml(objResp, "generated/editor.html") ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "managed editor page missing", 0);
	}
}

void Managed_RequestCategoriesView(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	if ( !Managed_SendAssetHtml(objResp, "generated/categories.html") ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "managed categories page missing", 0);
	}
}

void Managed_RequestPublicView(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	if ( !Managed_SendAssetHtml(objResp, "generated/public.html") ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "managed public page missing", 0);
	}
}

int Managed_OnLoad(XAdminPluginHandle* out_handle)
{
	if ( out_handle ) {
		G_Handle = *out_handle;
	}
	return 0;
}

int Managed_OnStart(XAdminPluginHandle handle)
{
	XAdminRouteDecl route;
	XAdminMenuDecl menu;
	int iRootMenuId = 0;
	xvalue tblSpec = Managed_LoadSpec();
	bool bAdminCrud = Managed_AdminCrudEnabled(tblSpec);
	bool bPublicApi = Managed_PublicApiEnabled(tblSpec);

	if ( !Managed_EnsureSchema() ) {
		printf("        [ManagedPlugin] start failed during schema ensure: xid={{PLUGIN_XID}}\n");
		if ( tblSpec ) xvoUnref(tblSpec);
		return -1;
	}

	if ( bPublicApi ) {
		memset(&route, 0, sizeof(route));
		route.path = "/api/plugin/{{PLUGIN_XID}}/meta";
		route.proc = Managed_RequestMeta;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/api/plugin/{{PLUGIN_XID}}/list";
		route.proc = Managed_RequestListPublic;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/api/plugin/{{PLUGIN_XID}}/detail";
		route.proc = Managed_RequestDetailPublic;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/api/plugin/{{PLUGIN_XID}}/contracts";
		route.proc = Managed_RequestContractsPublic;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/plugin/{{PLUGIN_XID}}";
		route.proc = Managed_RequestPublicView;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}
	}

	if ( bAdminCrud ) {
		memset(&route, 0, sizeof(route));
		route.path = "/admin/api/plugin/{{PLUGIN_XID}}/list";
		route.proc = Managed_RequestListAdmin;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/api/plugin/{{PLUGIN_XID}}/drafts";
		route.proc = Managed_RequestDraftsAdmin;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/api/plugin/{{PLUGIN_XID}}/category/list";
		route.proc = Managed_RequestCategoryListAdmin;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/api/plugin/{{PLUGIN_XID}}/category/get";
		route.proc = Managed_RequestCategoryGetAdmin;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/api/plugin/{{PLUGIN_XID}}/category/save";
		route.proc = Managed_RequestCategorySaveAdmin;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/api/plugin/{{PLUGIN_XID}}/category/delete";
		route.proc = Managed_RequestCategoryDeleteAdmin;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/api/plugin/{{PLUGIN_XID}}/category/sort";
		route.proc = Managed_RequestCategorySortAdmin;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/api/plugin/{{PLUGIN_XID}}/contracts";
		route.proc = Managed_RequestContractsAdmin;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/api/plugin/{{PLUGIN_XID}}/form-meta";
		route.proc = Managed_RequestFormMetaAdmin;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/api/plugin/{{PLUGIN_XID}}/get";
		route.proc = Managed_RequestGetAdmin;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/api/plugin/{{PLUGIN_XID}}/save";
		route.proc = Managed_RequestSave;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/api/plugin/{{PLUGIN_XID}}/delete";
		route.proc = Managed_RequestDelete;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/view/plugin/{{PLUGIN_XID}}";
		route.proc = Managed_RequestAdminView;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/view/plugin/{{PLUGIN_XID}}/articles";
		route.proc = Managed_RequestAdminView;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/view/plugin/{{PLUGIN_XID}}/drafts";
		route.proc = Managed_RequestDraftsView;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/view/plugin/{{PLUGIN_XID}}/editor";
		route.proc = Managed_RequestEditorView;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/view/plugin/{{PLUGIN_XID}}/categories";
		route.proc = Managed_RequestCategoriesView;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&menu, 0, sizeof(menu));
		menu.key = "{{PLUGIN_XID}}.root";
		menu.title = "{{PLUGIN_TITLE_C}}";
		menu.icon = "layui-icon layui-icon-template";
		menu.type = 0;
		menu.open_type = "_component";
		menu.href = "";
		menu.sort = 990200;
		menu.visible = TRUE;
		menu.remark = "Managed content plugin root";
		if ( XAdmin_RegisterMenu(handle, &menu, &iRootMenuId, NULL) != 0 ) {
			printf("        [ManagedPlugin] menu register failed: xid={{PLUGIN_XID}} href=%s\n", menu.href);
			goto failed;
		}

		memset(&menu, 0, sizeof(menu));
		menu.key = "{{PLUGIN_XID}}.categories";
		menu.parent_id = iRootMenuId;
		menu.title = "栏目管理";
		menu.icon = "layui-icon layui-icon-tabs";
		menu.type = 1;
		menu.open_type = "_component";
		menu.href = "/admin/view/plugin/{{PLUGIN_XID}}/categories";
		menu.sort = 10;
		menu.visible = TRUE;
		menu.remark = "Managed content categories";
		if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) {
			printf("        [ManagedPlugin] menu register failed: xid={{PLUGIN_XID}} href=%s\n", menu.href);
			goto failed;
		}

		memset(&menu, 0, sizeof(menu));
		menu.key = "{{PLUGIN_XID}}.articles";
		menu.parent_id = iRootMenuId;
		menu.title = "文章列表";
		menu.icon = "layui-icon layui-icon-list";
		menu.type = 1;
		menu.open_type = "_component";
		menu.href = "/admin/view/plugin/{{PLUGIN_XID}}/articles";
		menu.sort = 20;
		menu.visible = TRUE;
		menu.remark = "Managed content articles";
		if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) {
			printf("        [ManagedPlugin] menu register failed: xid={{PLUGIN_XID}} href=%s\n", menu.href);
			goto failed;
		}

		memset(&menu, 0, sizeof(menu));
		menu.key = "{{PLUGIN_XID}}.drafts";
		menu.parent_id = iRootMenuId;
		menu.title = "草稿箱";
		menu.icon = "layui-icon layui-icon-file-b";
		menu.type = 1;
		menu.open_type = "_component";
		menu.href = "/admin/view/plugin/{{PLUGIN_XID}}/drafts";
		menu.sort = 30;
		menu.visible = TRUE;
		menu.remark = "Managed content drafts";
		if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) {
			printf("        [ManagedPlugin] menu register failed: xid={{PLUGIN_XID}} href=%s\n", menu.href);
			goto failed;
		}
	}

	if ( tblSpec ) xvoUnref(tblSpec);
	return 0;

failed:
	printf("        [ManagedPlugin] start aborted: xid={{PLUGIN_XID}}\n");
	if ( tblSpec ) xvoUnref(tblSpec);
	return -1;
}

int Managed_OnConfigChanged(XAdminPluginHandle handle, xvalue new_cfg)
{
	(void)handle;
	Managed_ConfigReset();
	if ( new_cfg && (xvoType(new_cfg) == XVO_DT_TABLE) ) {
		int iPageSize = (int)xvoTableGetInt(new_cfg, "pageSize", 8);
		if ( iPageSize > 0 ) {
			G_Config.iPageSize = iPageSize;
		}
	}
	return 0;
}

void Managed_OnStop(XAdminPluginHandle handle)
{
	(void)handle;
}

void Managed_OnUnload(XAdminPluginHandle handle)
{
	(void)handle;
}

static XAdminPluginDescriptor G_Plugin = {
	XADMIN_ABI_VERSION,
	sizeof(XAdminPluginDescriptor),
	"{{PLUGIN_XID}}",
	"{{PLUGIN_VERSION}}",
	"{{PLUGIN_TITLE_C}}",
	Managed_OnLoad,
	NULL,
	Managed_OnStart,
	Managed_OnConfigChanged,
	NULL,
	Managed_OnStop,
	Managed_OnUnload
};

XADMIN_DECLARE_PLUGIN(G_Plugin)
