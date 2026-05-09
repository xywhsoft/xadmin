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
	"description TEXT NOT NULL DEFAULT '',"
	"cover_url TEXT NOT NULL DEFAULT '',"
	"template_key TEXT NOT NULL DEFAULT '',"
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
	"CREATE UNIQUE INDEX IF NOT EXISTS idx_content_category_parent_slug ON content_category(parent_id, slug) WHERE delete_time = 0;"
{{ABILITY_PACK_SCHEMA_SQL}}
	;

static const char* G_PostMigrationIndexSql =
	"CREATE INDEX IF NOT EXISTS idx_content_item_category_status ON content_item(category_id, status, is_draft, delete_time);";

#define MANAGED_STATIC_URL_PREFIX "/plugin-static/{{PLUGIN_XID}}/"

bool Managed_AbilityPackMounted(const char* sPackId);

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
	if ( bOK && !Managed_TableColumnExists(pDb, "content_category", "description") ) {
		bOK = Managed_ExecSql(pDb, "ALTER TABLE content_category ADD COLUMN description TEXT NOT NULL DEFAULT ''");
	}
	if ( bOK && !Managed_TableColumnExists(pDb, "content_category", "cover_url") ) {
		bOK = Managed_ExecSql(pDb, "ALTER TABLE content_category ADD COLUMN cover_url TEXT NOT NULL DEFAULT ''");
	}
	if ( bOK && !Managed_TableColumnExists(pDb, "content_category", "template_key") ) {
		bOK = Managed_ExecSql(pDb, "ALTER TABLE content_category ADD COLUMN template_key TEXT NOT NULL DEFAULT ''");
	}
	if ( bOK && Managed_AbilityPackMounted("content.workflow") && !Managed_TableColumnExists(pDb, "content_workflow_log", "assignee_id") ) {
		bOK = Managed_ExecSql(pDb, "ALTER TABLE content_workflow_log ADD COLUMN assignee_id INTEGER NOT NULL DEFAULT 0");
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

bool Managed_SendAbilityAssetHtml(XS_RequestObject objReq, XS_ResponseObject objResp)
{
	str sPath;
	str sHtml;
	str sRendered;
	const char* sReqPath;
	const char* sPageKey;
	size_t iSize = 0;

	if ( G_RootPath == NULL ) {
		return FALSE;
	}
	sPath = xrtPathJoin(2, G_RootPath, "generated/ability.html");
	if ( (sPath == NULL) || !xrtFileExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		return FALSE;
	}
	sHtml = xrtFileGetAll(sPath, &iSize);
	xrtFree(sPath);
	if ( sHtml == NULL ) {
		return FALSE;
	}

	sReqPath = xsReqPath(objReq);
	sPageKey = sReqPath ? strrchr(sReqPath, '/') : NULL;
	sPageKey = (sPageKey && sPageKey[1]) ? (sPageKey + 1) : "overview";
	sRendered = xrtReplace(sHtml, 0, "@@ABILITY_PAGE_KEY@@", 0, (str)sPageKey, 0, NULL);
	xrtFree(sHtml);
	if ( sRendered == NULL ) {
		return FALSE;
	}
	xsHttpReplyAuto(objResp, 200, "Content-Type: text/html; charset=utf-8\r\n", sRendered, 0);
	xrtFree(sRendered);
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

xvalue Managed_CreateItemFromStmt(sqlite3_stmt* stmt, xvalue tblSpec)
{
	xvalue tblItem = xvoCreateTable();
	const char* sPayload = (const char*)sqlite3_column_text(stmt, 3);

	if ( (stmt == NULL) || (tblItem == NULL) ) {
		if ( tblItem ) xvoUnref(tblItem);
		return NULL;
	}
	xvoTableSetInt(tblItem, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetText(tblItem, "title", 5, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
	xvoTableSetInt(tblItem, "status", 6, sqlite3_column_int(stmt, 2));
	Managed_AppendPayload(tblItem, sPayload);
	xvoTableSetBool(tblItem, "isDraft", 7, sqlite3_column_int(stmt, 4) ? TRUE : FALSE);
	xvoTableSetInt(tblItem, "createTime", 10, sqlite3_column_int64(stmt, 5));
	xvoTableSetInt(tblItem, "updateTime", 10, sqlite3_column_int64(stmt, 6));
	Managed_SetTimeText(tblItem, "createTimeText", 14, sqlite3_column_int64(stmt, 5));
	Managed_SetTimeText(tblItem, "updateTimeText", 14, sqlite3_column_int64(stmt, 6));
	xvoTableSetInt(tblItem, "categoryId", 10, sqlite3_column_int(stmt, 7));
	Managed_AppendDerivedFields(tblItem, tblSpec);
	return tblItem;
}

xvalue Managed_LoadContentItemById(sqlite3* pDb, xvalue tblSpec, int64 iContentId)
{
	sqlite3_stmt* stmt = NULL;
	xvalue tblItem = NULL;

	if ( (pDb == NULL) || (tblSpec == NULL) || (iContentId <= 0) ) return NULL;
	if ( sqlite3_prepare_v2(pDb, "SELECT id,title,status,payload_json,is_draft,create_time,update_time,category_id FROM content_item WHERE id=? AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			tblItem = Managed_CreateItemFromStmt(stmt, tblSpec);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return tblItem;
}

const char* Managed_TableTextOrEmpty(xvalue tblData, const char* sKey)
{
	const char* sValue;

	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || Managed_IsBlank(sKey) ) {
		return "";
	}
	sValue = xvoTableGetText(tblData, sKey, (int)strlen(sKey));
	return sValue ? sValue : "";
}

const char* Managed_ItemTextOrEmpty(xvalue tblItem, const char* sKey)
{
	const char* sValue;

	if ( (tblItem == NULL) || (xvoType(tblItem) != XVO_DT_TABLE) || Managed_IsBlank(sKey) ) {
		return "";
	}
	sValue = xvoTableGetText(tblItem, sKey, (int)strlen(sKey));
	return sValue ? sValue : "";
}

bool Managed_AbilityPackMounted(const char* sPackId);

xvalue Managed_BuildSeoMeta(xvalue tblItem)
{
	xvalue tblMeta = xvoCreateTable();
	xvalue tblData = tblItem ? xvoTableGetValue(tblItem, "data", 4) : NULL;
	const char* sTitle = Managed_TableTextOrEmpty(tblData, "seo_title");
	const char* sKeywords = Managed_TableTextOrEmpty(tblData, "seo_keywords");
	const char* sDescription = Managed_TableTextOrEmpty(tblData, "seo_description");
	const char* sSlug = Managed_ItemTextOrEmpty(tblItem, "slug");

	if ( Managed_IsBlank(sTitle) ) {
		sTitle = Managed_ItemTextOrEmpty(tblItem, "title");
	}
	if ( Managed_IsBlank(sDescription) ) {
		sDescription = Managed_ItemTextOrEmpty(tblItem, "summary");
	}
	xvoTableSetText(tblMeta, "title", 5, (str)(sTitle ? sTitle : ""), 0, FALSE);
	xvoTableSetText(tblMeta, "keywords", 8, (str)(sKeywords ? sKeywords : ""), 0, FALSE);
	xvoTableSetText(tblMeta, "description", 11, (str)(sDescription ? sDescription : ""), 0, FALSE);
	xvoTableSetText(tblMeta, "slug", 4, (str)(sSlug ? sSlug : ""), 0, FALSE);
	if ( !Managed_IsBlank(sSlug) ) {
		str sCanonical = xrtFormat("/plugin/{{PLUGIN_XID}}?slug=%s", sSlug);
		xvoTableSetText(tblMeta, "canonical", 9, sCanonical ? sCanonical : (str)"", 0, TRUE);
	}
	return tblMeta;
}

void Managed_ApplySeoMetaRow(xvalue tblMeta, sqlite3_stmt* stmt)
{
	const char* sTitle = (const char*)sqlite3_column_text(stmt, 0);
	const char* sKeywords = (const char*)sqlite3_column_text(stmt, 1);
	const char* sDescription = (const char*)sqlite3_column_text(stmt, 2);
	const char* sCanonical = (const char*)sqlite3_column_text(stmt, 3);

	if ( tblMeta == NULL ) return;
	if ( !Managed_IsBlank(sTitle) ) xvoTableSetText(tblMeta, "title", 5, (str)sTitle, 0, FALSE);
	if ( !Managed_IsBlank(sKeywords) ) xvoTableSetText(tblMeta, "keywords", 8, (str)sKeywords, 0, FALSE);
	if ( !Managed_IsBlank(sDescription) ) xvoTableSetText(tblMeta, "description", 11, (str)sDescription, 0, FALSE);
	if ( !Managed_IsBlank(sCanonical) ) xvoTableSetText(tblMeta, "canonical", 9, (str)sCanonical, 0, FALSE);
}

xvalue Managed_LoadSeoMeta(sqlite3* pDb, xvalue tblItem)
{
	sqlite3_stmt* stmt = NULL;
	xvalue tblMeta = Managed_BuildSeoMeta(tblItem);
	int64 iContentId = tblItem ? xvoTableGetInt(tblItem, "id", 2) : 0;

	if ( !Managed_AbilityPackMounted("content.seo") || (pDb == NULL) || (iContentId <= 0) ) return tblMeta;
	if ( sqlite3_prepare_v2(pDb, "SELECT seo_title,seo_keywords,seo_description,canonical FROM content_seo_meta WHERE content_id=? AND status=1 AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) Managed_ApplySeoMetaRow(tblMeta, stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return tblMeta;
}

void Managed_AttachSeoMetaField(sqlite3* pDb, xvalue tblItem)
{
	xvalue tblMeta = NULL;

	if ( !Managed_AbilityPackMounted("content.seo") || (pDb == NULL) || (tblItem == NULL) || (xvoType(tblItem) != XVO_DT_TABLE) ) {
		return;
	}
	tblMeta = Managed_LoadSeoMeta(pDb, tblItem);
	if ( tblMeta ) {
		xvoTableSetValue(tblItem, "seoMeta", 7, tblMeta, TRUE);
	}
}

void Managed_SeoSyncData(sqlite3* pDb, int64 iContentId, const char* sTitle, xvalue tblData, int64 iNow)
{
	sqlite3_stmt* stmt = NULL;
	const char* sSeoTitle = Managed_TableTextOrEmpty(tblData, "seo_title");
	const char* sSeoKeywords = Managed_TableTextOrEmpty(tblData, "seo_keywords");
	const char* sSeoDescription = Managed_TableTextOrEmpty(tblData, "seo_description");
	const char* sFinalTitle = Managed_IsBlank(sSeoTitle) ? sTitle : sSeoTitle;

	if ( !Managed_AbilityPackMounted("content.seo") || (pDb == NULL) || (iContentId <= 0) || (tblData == NULL) ) return;
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_seo_meta(content_id,seo_title,seo_keywords,seo_description,canonical,status,create_time,update_time,delete_time) VALUES(?,?,?,?,?,1,?,?,0) ON CONFLICT(content_id) DO UPDATE SET seo_title=excluded.seo_title,seo_keywords=excluded.seo_keywords,seo_description=excluded.seo_description,canonical=excluded.canonical,status=1,update_time=excluded.update_time,delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		str sCanonical = xrtFormat("/plugin/{{PLUGIN_XID}}?id=%lld", (long long)iContentId);
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_text(stmt, 2, sFinalTitle ? sFinalTitle : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, sSeoKeywords ? sSeoKeywords : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, sSeoDescription ? sSeoDescription : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 5, sCanonical ? (const char*)sCanonical : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 6, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 7, (sqlite3_int64)iNow);
		sqlite3_step(stmt);
		if ( sCanonical ) xrtFree(sCanonical);
	}
	if ( stmt ) sqlite3_finalize(stmt);
}

void Managed_SeoDelete(sqlite3* pDb, int64 iContentId)
{
	sqlite3_stmt* stmt = NULL;

	if ( !Managed_AbilityPackMounted("content.seo") || (pDb == NULL) || (iContentId <= 0) ) return;
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_seo_meta SET status=0,delete_time=?,update_time=? WHERE content_id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		int64 iNow = xrtNow();
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iContentId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
}

bool Managed_NormalizeRedirectPath(const char* sInput, char* sBuf, int iBufSize)
{
	if ( (sBuf == NULL) || (iBufSize <= 0) ) {
		return FALSE;
	}
	memset(sBuf, 0, (size_t)iBufSize);
	if ( Managed_IsBlank(sInput) ) {
		return FALSE;
	}
	if ( sInput[0] == '/' ) {
		snprintf(sBuf, (size_t)iBufSize, "%s", sInput);
	} else {
		snprintf(sBuf, (size_t)iBufSize, "/%s", sInput);
	}
	return sBuf[0] != '\0';
}

bool Managed_RedirectLocationSafe(const char* sUrl)
{
	if ( Managed_IsBlank(sUrl) ) {
		return FALSE;
	}
	for ( int i = 0; sUrl[i]; i++ ) {
		if ( (sUrl[i] == '\r') || (sUrl[i] == '\n') ) {
			return FALSE;
		}
	}
	return TRUE;
}

void Managed_AppendTaxonomyText(char* sBuffer, size_t iCap, const char* sText)
{
	size_t iLen;

	if ( (sBuffer == NULL) || (iCap <= 0) || Managed_IsBlank(sText) ) {
		return;
	}
	iLen = strlen(sBuffer);
	if ( iLen >= (iCap - 1) ) {
		return;
	}
	if ( iLen > 0 ) {
		strncat(sBuffer, ", ", iCap - iLen - 1);
		iLen = strlen(sBuffer);
		if ( iLen >= (iCap - 1) ) {
			return;
		}
	}
	strncat(sBuffer, sText, iCap - iLen - 1);
}

void Managed_AttachTagFields(sqlite3* pDb, xvalue tblItem, bool bAttachTag)
{
	sqlite3_stmt* stmt = NULL;
	xvalue arrIds = xvoCreateArray();
	xvalue arrNames = xvoCreateArray();
	char sNames[1024] = {0};
	int64 iContentId = xvoTableGetInt(tblItem, "id", 2);

	if ( (pDb == NULL) || (tblItem == NULL) || (iContentId <= 0) || !bAttachTag ) {
		xvoUnref(arrIds);
		xvoUnref(arrNames);
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT t.id,t.name FROM content_tag ct INNER JOIN tag t ON t.id=ct.tag_id WHERE ct.content_id=? AND t.delete_time=0 ORDER BY ct.sort ASC,t.id ASC", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			const char* sName = (const char*)sqlite3_column_text(stmt, 1);
			xvoArrayAppendValue(arrIds, xvoCreateInt(sqlite3_column_int64(stmt, 0)), TRUE);
			xvoArrayAppendValue(arrNames, xvoCreateText((str)(sName ? sName : ""), 0, FALSE), TRUE);
			Managed_AppendTaxonomyText(sNames, sizeof(sNames), sName);
		}
		sqlite3_finalize(stmt);
	}
	xvoTableSetValue(tblItem, "tagIds", 6, arrIds, TRUE);
	xvoTableSetValue(tblItem, "tagNames", 8, arrNames, TRUE);
	xvoTableSetText(tblItem, "tagNamesText", 12, sNames, 0, FALSE);
}

void Managed_AttachTopicFields(sqlite3* pDb, xvalue tblItem, bool bAttachTopic)
{
	sqlite3_stmt* stmt = NULL;
	xvalue arrIds = xvoCreateArray();
	xvalue arrTitles = xvoCreateArray();
	char sTitles[1024] = {0};
	int64 iContentId = xvoTableGetInt(tblItem, "id", 2);

	if ( (pDb == NULL) || (tblItem == NULL) || (iContentId <= 0) || !bAttachTopic ) {
		xvoUnref(arrIds);
		xvoUnref(arrTitles);
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT t.id,t.title FROM topic_content tc INNER JOIN topic t ON t.id=tc.topic_id WHERE tc.content_id=? AND t.delete_time=0 ORDER BY tc.sort ASC,t.id ASC", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			const char* sTitle = (const char*)sqlite3_column_text(stmt, 1);
			xvoArrayAppendValue(arrIds, xvoCreateInt(sqlite3_column_int64(stmt, 0)), TRUE);
			xvoArrayAppendValue(arrTitles, xvoCreateText((str)(sTitle ? sTitle : ""), 0, FALSE), TRUE);
			Managed_AppendTaxonomyText(sTitles, sizeof(sTitles), sTitle);
		}
		sqlite3_finalize(stmt);
	}
	xvoTableSetValue(tblItem, "topicIds", 8, arrIds, TRUE);
	xvoTableSetValue(tblItem, "topicTitles", 11, arrTitles, TRUE);
	xvoTableSetText(tblItem, "topicTitlesText", 15, sTitles, 0, FALSE);
}

void Managed_AttachMetricFields(sqlite3* pDb, xvalue tblItem, bool bAttachComment, bool bAttachLike, bool bAttachView)
{
	sqlite3_stmt* stmt = NULL;
	int64 iContentId = xvoTableGetInt(tblItem, "id", 2);

	if ( (pDb == NULL) || (tblItem == NULL) || (iContentId <= 0) ) {
		return;
	}
	if ( bAttachComment && (sqlite3_prepare_v2(pDb, "SELECT comment_count,visible_count FROM comment_thread WHERE content_id=? LIMIT 1", -1, &stmt, NULL) == SQLITE_OK) ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvoTableSetInt(tblItem, "commentCount", 12, sqlite3_column_int64(stmt, 0));
			xvoTableSetInt(tblItem, "visibleCommentCount", 19, sqlite3_column_int64(stmt, 1));
		} else {
			xvoTableSetInt(tblItem, "commentCount", 12, 0);
			xvoTableSetInt(tblItem, "visibleCommentCount", 19, 0);
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if ( bAttachLike && (sqlite3_prepare_v2(pDb, "SELECT like_count FROM like_counter WHERE content_id=? LIMIT 1", -1, &stmt, NULL) == SQLITE_OK) ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		xvoTableSetInt(tblItem, "likeCount", 9, (sqlite3_step(stmt) == SQLITE_ROW) ? sqlite3_column_int64(stmt, 0) : 0);
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if ( bAttachView && (sqlite3_prepare_v2(pDb, "SELECT view_count,unique_view_count FROM view_counter WHERE content_id=? LIMIT 1", -1, &stmt, NULL) == SQLITE_OK) ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvoTableSetInt(tblItem, "viewCount", 9, sqlite3_column_int64(stmt, 0));
			xvoTableSetInt(tblItem, "uniqueViewCount", 15, sqlite3_column_int64(stmt, 1));
		} else {
			xvoTableSetInt(tblItem, "viewCount", 9, 0);
			xvoTableSetInt(tblItem, "uniqueViewCount", 15, 0);
		}
		sqlite3_finalize(stmt);
	}
}

void Managed_AttachMediaFields(sqlite3* pDb, xvalue tblItem, bool bAttachMedia)
{
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	int64 iContentId = xvoTableGetInt(tblItem, "id", 2);

	if ( (pDb == NULL) || (tblItem == NULL) || (iContentId <= 0) || !bAttachMedia ) {
		xvoUnref(arrList);
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT m.id,m.title,m.url,m.mime,m.size,r.ref_type FROM content_media_ref r INNER JOIN content_media m ON m.id=r.media_id WHERE r.content_id=? AND m.delete_time=0 ORDER BY r.id ASC", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblMedia = xvoCreateTable();
			xvoTableSetInt(tblMedia, "id", 2, sqlite3_column_int64(stmt, 0));
			xvoTableSetText(tblMedia, "title", 5, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
			xvoTableSetText(tblMedia, "url", 3, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
			xvoTableSetText(tblMedia, "mime", 4, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
			xvoTableSetInt(tblMedia, "size", 4, sqlite3_column_int64(stmt, 4));
			xvoTableSetText(tblMedia, "refType", 7, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
			xvoArrayAppendValue(arrList, tblMedia, TRUE);
			if ( strcmp((const char*)sqlite3_column_text(stmt, 5), "cover") == 0 ) {
				xvoTableSetInt(tblItem, "coverMediaId", 12, sqlite3_column_int64(stmt, 0));
				xvoTableSetText(tblItem, "coverMediaUrl", 13, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
			}
		}
		sqlite3_finalize(stmt);
	}
	xvoTableSetValue(tblItem, "mediaList", 9, arrList, TRUE);
}

void Managed_AttachRelatedFields(sqlite3* pDb, xvalue tblItem, xvalue tblSpec, bool bAttachRelated)
{
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	int64 iContentId = tblItem ? xvoTableGetInt(tblItem, "id", 2) : 0;

	if ( (pDb == NULL) || (tblItem == NULL) || (iContentId <= 0) || !bAttachRelated ) {
		xvoUnref(arrList);
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT i.id,i.title,i.status,i.payload_json,i.is_draft,i.create_time,i.update_time,i.category_id,r.relation_type,r.weight FROM content_related r INNER JOIN content_item i ON i.id=r.related_content_id WHERE r.source_content_id=? AND r.status=1 AND r.delete_time=0 AND i.delete_time=0 AND i.is_draft=0 AND i.status>=? ORDER BY r.weight DESC,r.update_time DESC,r.id DESC LIMIT 20", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_int(stmt, 2, Managed_PublicStatusThreshold(tblSpec));
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblRelated = Managed_CreateItemFromStmt(stmt, tblSpec);
			xvoTableSetText(tblRelated, "relationType", 12, (str)sqlite3_column_text(stmt, 8), 0, FALSE);
			xvoTableSetInt(tblRelated, "weight", 6, sqlite3_column_int(stmt, 9));
			xvoArrayAppendValue(arrList, tblRelated, TRUE);
			xvoUnref(tblRelated);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	xvoTableSetValue(tblItem, "relatedList", 11, arrList, TRUE);
}

void Managed_AttachAbilityListFields(sqlite3* pDb, xvalue tblItem, bool bAttachTag, bool bAttachTopic, bool bAttachComment, bool bAttachLike, bool bAttachView, bool bAttachMedia)
{
	if ( bAttachTag ) {
		Managed_AttachTagFields(pDb, tblItem, TRUE);
	}
	if ( bAttachTopic ) {
		Managed_AttachTopicFields(pDb, tblItem, TRUE);
	}
	if ( bAttachComment || bAttachLike || bAttachView ) {
		Managed_AttachMetricFields(pDb, tblItem, bAttachComment, bAttachLike, bAttachView);
	}
	if ( bAttachMedia ) {
		Managed_AttachMediaFields(pDb, tblItem, TRUE);
	}
}

bool Managed_ArrayHasIntValue(xvalue arrList, int64 iNeedle)
{
	if ( (arrList == NULL) || (xvoType(arrList) != XVO_DT_ARRAY) || (iNeedle <= 0) ) {
		return FALSE;
	}
	for ( uint32 i = 0; i < xvoArrayItemCount(arrList); i++ ) {
		xvalue objValue = xvoArrayGetValue(arrList, i);
		if ( xvoGetInt(objValue) == iNeedle ) {
			return TRUE;
		}
	}
	return FALSE;
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

bool Managed_AbilityPackMounted(const char* sPackId);

str Managed_ExtractSlugValue(xvalue tblData, xvalue tblSpec)
{
	if ( !Managed_AbilityPackMounted("content.slug") || (tblData == NULL) || (tblSpec == NULL) ) {
		return NULL;
	}
	return Managed_ExtractTextField(tblData, Managed_GetSlugField(tblSpec));
}

str Managed_LoadContentSlug(sqlite3* pDb, xvalue tblSpec, int64 iId)
{
	sqlite3_stmt* stmt = NULL;
	xvalue tblItem = NULL;
	str sSlug = NULL;

	if ( (pDb == NULL) || (tblSpec == NULL) || (iId <= 0) ) {
		return NULL;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id,title,status,payload_json,is_draft,create_time,update_time,category_id FROM content_item WHERE id=? AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			tblItem = Managed_CreateItemFromStmt(stmt, tblSpec);
			sSlug = Managed_ExtractSlugValue(tblItem, tblSpec);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	if ( tblItem ) xvoUnref(tblItem);
	return sSlug;
}

bool Managed_SlugExists(sqlite3* pDb, xvalue tblSpec, const char* sSlug, int64 iExcludeId, int64* pConflictId)
{
	sqlite3_stmt* stmt = NULL;
	bool bFound = FALSE;

	if ( pConflictId ) *pConflictId = 0;
	if ( !Managed_AbilityPackMounted("content.slug") || (pDb == NULL) || (tblSpec == NULL) || Managed_IsBlank(sSlug) ) {
		return FALSE;
	}

	/* Slug is stored in payload during the current capability phase, so conflict detection scans content rows only when the pack is enabled. */
	if ( sqlite3_prepare_v2(pDb, "SELECT id,title,status,payload_json,is_draft,create_time,update_time,category_id FROM content_item WHERE delete_time=0 AND id<>? ORDER BY id DESC", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iExcludeId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblCandidate = Managed_CreateItemFromStmt(stmt, tblSpec);
			if ( Managed_ItemMatchesSlug(tblCandidate, sSlug) ) {
				bFound = TRUE;
				if ( pConflictId ) *pConflictId = sqlite3_column_int64(stmt, 0);
				if ( tblCandidate ) xvoUnref(tblCandidate);
				break;
			}
			if ( tblCandidate ) xvoUnref(tblCandidate);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return bFound;
}

str Managed_NormalizeSlugCandidate(const char* sValue, int64 iContentId)
{
	char sBuf[192];
	int iPos = 0;
	bool bDash = FALSE;

	memset(sBuf, 0, sizeof(sBuf));
	if ( !Managed_IsBlank(sValue) ) {
		for ( int i = 0; sValue[i] && (iPos < (int)sizeof(sBuf) - 1); i++ ) {
			unsigned char ch = (unsigned char)sValue[i];
			if ( (ch >= 'A') && (ch <= 'Z') ) {
				sBuf[iPos++] = (char)(ch - 'A' + 'a');
				bDash = FALSE;
			} else if ( ((ch >= 'a') && (ch <= 'z')) || ((ch >= '0') && (ch <= '9')) || (ch == '_') ) {
				sBuf[iPos++] = (char)ch;
				bDash = FALSE;
			} else if ( (ch == '-') || (ch == ' ') || (ch == '.') || (ch == '/') || (ch == '\\') ) {
				if ( (iPos > 0) && !bDash ) {
					sBuf[iPos++] = '-';
					bDash = TRUE;
				}
			}
		}
		while ( (iPos > 0) && (sBuf[iPos - 1] == '-') ) {
			sBuf[--iPos] = 0;
		}
	}
	if ( iPos <= 0 ) {
		return xrtFormat("item-%lld", iContentId);
	}
	return xrtCopyStr(sBuf, 0);
}

str Managed_BuildRepairSlug(sqlite3* pDb, xvalue tblSpec, const char* sBase, int64 iContentId, bool bForceSuffix)
{
	str sBaseSlug = Managed_NormalizeSlugCandidate(sBase, iContentId);
	str sCandidate = NULL;
	int64 iConflictId = 0;

	if ( Managed_IsBlank((const char*)sBaseSlug) ) {
		if ( sBaseSlug ) xrtFree(sBaseSlug);
		sBaseSlug = xrtFormat("item-%lld", iContentId);
	}
	if ( !bForceSuffix && !Managed_SlugExists(pDb, tblSpec, (const char*)sBaseSlug, iContentId, &iConflictId) ) {
		return sBaseSlug;
	}
	sCandidate = xrtFormat("%s-%lld", (const char*)sBaseSlug, iContentId);
	for ( int i = 2; Managed_SlugExists(pDb, tblSpec, (const char*)sCandidate, iContentId, &iConflictId) && (i < 10000); i++ ) {
		if ( sCandidate ) xrtFree(sCandidate);
		sCandidate = xrtFormat("%s-%lld-%d", (const char*)sBaseSlug, iContentId, i);
	}
	if ( sBaseSlug ) xrtFree(sBaseSlug);
	return sCandidate;
}

bool Managed_UpdateContentSlugPayload(sqlite3* pDb, int64 iContentId, const char* sPayloadJson, const char* sSlugField, const char* sNewSlug, int64 iNow)
{
	sqlite3_stmt* stmt = NULL;
	xvalue tblPayload = NULL;
	str sNextJson = NULL;
	bool bOK = FALSE;

	if ( (pDb == NULL) || (iContentId <= 0) || Managed_IsBlank(sSlugField) || Managed_IsBlank(sNewSlug) ) {
		return FALSE;
	}
	if ( !Managed_IsBlank(sPayloadJson) ) {
		tblPayload = xrtParseJSON((str)sPayloadJson, strlen(sPayloadJson));
	}
	if ( (tblPayload == NULL) || (xvoType(tblPayload) != XVO_DT_TABLE) ) {
		if ( tblPayload ) xvoUnref(tblPayload);
		tblPayload = xvoCreateTable();
	}
	xvoTableSetText(tblPayload, sSlugField, (int)strlen(sSlugField), (str)sNewSlug, 0, FALSE);
	sNextJson = xrtStringifyJSON(tblPayload, FALSE, NULL);
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_item SET payload_json=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, (const char*)(sNextJson ? sNextJson : (str)"{}"), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iContentId);
		bOK = (sqlite3_step(stmt) == SQLITE_DONE);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	if ( sNextJson ) xrtFree(sNextJson);
	if ( tblPayload ) xvoUnref(tblPayload);
	return bOK;
}

void Managed_SlugHistoryInsert(sqlite3* pDb, int64 iContentId, const char* sOldSlug, const char* sNewSlug, int64 iNow)
{
	sqlite3_stmt* stmt = NULL;

	if ( !Managed_AbilityPackMounted("content.slug") || (pDb == NULL) || (iContentId <= 0) || Managed_IsBlank(sOldSlug) ) {
		return;
	}
	if ( !Managed_IsBlank(sNewSlug) && (strcmp(sOldSlug, sNewSlug) == 0) ) {
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_slug_history(content_id,old_slug,new_slug,status,create_time) VALUES(?,?,?,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_text(stmt, 2, sOldSlug, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, sNewSlug ? sNewSlug : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 4, 1);
		sqlite3_bind_int64(stmt, 5, (sqlite3_int64)iNow);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
}

void Managed_SlugRedirectSync(sqlite3* pDb, int64 iContentId, const char* sOldSlug, const char* sNewSlug, int64 iNow)
{
	sqlite3_stmt* stmt = NULL;
	char sSource[768];
	str sRawSource = NULL;
	str sTarget = NULL;

	if ( !Managed_AbilityPackMounted("content.slug") || !Managed_AbilityPackMounted("content.redirect") || (pDb == NULL) || (iContentId <= 0) ) return;
	if ( Managed_IsBlank(sOldSlug) || Managed_IsBlank(sNewSlug) || (strcmp(sOldSlug, sNewSlug) == 0) ) return;
	sRawSource = xrtFormat("/%s", sOldSlug);
	sTarget = xrtFormat("/plugin/{{PLUGIN_XID}}?slug=%s", sNewSlug);
	if ( !Managed_NormalizeRedirectPath(sRawSource ? (const char*)sRawSource : sOldSlug, sSource, sizeof(sSource)) || Managed_IsBlank((const char*)sTarget) ) {
		if ( sRawSource ) xrtFree(sRawSource);
		if ( sTarget ) xrtFree(sTarget);
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_redirect SET target_url=?,status_code=301,status=1,update_time=?,delete_time=0 WHERE source_path=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, (const char*)sTarget, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iNow);
		sqlite3_bind_text(stmt, 3, sSource, -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( sqlite3_changes(pDb) <= 0 ) {
		if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_redirect(source_path,target_url,status_code,status,create_time,update_time,delete_time) VALUES(?,?,301,1,?,?,0)", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sSource, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, (const char*)sTarget, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iNow);
			sqlite3_bind_int64(stmt, 4, (sqlite3_int64)iNow);
			sqlite3_step(stmt);
		}
		if ( stmt ) sqlite3_finalize(stmt);
	}
	if ( sRawSource ) xrtFree(sRawSource);
	if ( sTarget ) xrtFree(sTarget);
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
	char sTagId[32];
	char sTopicId[32];
	int iCategoryId = 0;
	int iTagId = 0;
	int iTopicId = 0;

	if ( (tblItem == NULL) || (xvoType(tblItem) != XVO_DT_TABLE) ) {
		return FALSE;
	}
	memset(sCategoryId, 0, sizeof(sCategoryId));
	memset(sTagId, 0, sizeof(sTagId));
	memset(sTopicId, 0, sizeof(sTopicId));
	if ( objReq ) {
		xsReqQueryValue(objReq, "categoryId", sCategoryId, sizeof(sCategoryId));
		xsReqQueryValue(objReq, "tagId", sTagId, sizeof(sTagId));
		xsReqQueryValue(objReq, "topicId", sTopicId, sizeof(sTopicId));
	}
	if ( sCategoryId[0] != '\0' ) {
		iCategoryId = atoi(sCategoryId);
		if ( (iCategoryId > 0) && (xvoTableGetInt(tblItem, "categoryId", 10) != iCategoryId) ) {
			return FALSE;
		}
	}
	if ( sTagId[0] != '\0' ) {
		iTagId = atoi(sTagId);
		if ( (iTagId > 0) && !Managed_ArrayHasIntValue(xvoTableGetValue(tblItem, "tagIds", 6), iTagId) ) {
			return FALSE;
		}
	}
	if ( sTopicId[0] != '\0' ) {
		iTopicId = atoi(sTopicId);
		if ( (iTopicId > 0) && !Managed_ArrayHasIntValue(xvoTableGetValue(tblItem, "topicIds", 8), iTopicId) ) {
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

bool Managed_AbilityPackMounted(const char* sPackId);
str ServerHashPassword(str user, str salt, str clientHash);
const char* Managed_AuditSessionIp(xvalue objSession);
const char* Managed_AuditRequestIp(XS_RequestObject objReq);
void Managed_AuditLogCore(sqlite3* pDb, const char* sTargetType, int64 iTargetId, const char* sAction, const char* sSummary, const char* sDetailJson, const char* sRequestIp, xvalue objSession);
#define Managed_AuditLog(pDb, sTargetType, iTargetId, sAction, sSummary, sDetailJson, objSession) Managed_AuditLogCore(pDb, sTargetType, iTargetId, sAction, sSummary, sDetailJson, NULL, objSession)
#define Managed_AuditLogWithRequest(pDb, sTargetType, iTargetId, sAction, sSummary, sDetailJson, objReq, objSession) Managed_AuditLogCore(pDb, sTargetType, iTargetId, sAction, sSummary, sDetailJson, Managed_AuditRequestIp(objReq), objSession)
void Managed_MediaSyncRefs(sqlite3* pDb, int64 iContentId, xvalue tblData, int64 iNow);
void Managed_RevisionSnapshot(sqlite3* pDb, int64 iContentId, const char* sTitle, int iStatus, int iCategoryId, bool bDraft, const char* sPayloadJson, const char* sAction, int64 iNow);

int64 Managed_RequestReadLevel(XS_RequestObject objReq, xvalue objSession)
{
	char sReadLevel[32];
	int64 iLevel = 0;

	memset(sReadLevel, 0, sizeof(sReadLevel));
	if ( objSession && (xvoType(objSession) == XVO_DT_TABLE) ) {
		iLevel = xvoTableGetInt(objSession, "authLevel", 9);
		if ( iLevel <= 0 ) iLevel = xvoTableGetInt(objSession, "__authLevel__", 13);
	}
	if ( objReq ) {
		xsReqQueryValue(objReq, "readLevel", sReadLevel, sizeof(sReadLevel));
		if ( sReadLevel[0] != '\0' ) iLevel = atoll(sReadLevel);
	}
	return iLevel;
}

bool Managed_RequestHasMemberSession(xvalue objSession)
{
	if ( (objSession == NULL) || (xvoType(objSession) != XVO_DT_TABLE) ) {
		return FALSE;
	}
	return (xvoTableGetInt(objSession, "memberID", 8) > 0)
		|| (xvoTableGetInt(objSession, "memberId", 8) > 0)
		|| (xvoTableGetInt(objSession, "id", 2) > 0);
}

int64 Managed_RequestMemberGroupId(XS_RequestObject objReq, xvalue objSession)
{
	char sGroupId[32];
	int64 iGroupId = 0;

	memset(sGroupId, 0, sizeof(sGroupId));
	if ( objSession && (xvoType(objSession) == XVO_DT_TABLE) ) {
		iGroupId = xvoTableGetInt(objSession, "memberGroupId", 13);
		if ( iGroupId <= 0 ) iGroupId = xvoTableGetInt(objSession, "groupId", 7);
		if ( iGroupId <= 0 ) iGroupId = xvoTableGetInt(objSession, "roleId", 6);
	}
	if ( objReq ) {
		xsReqQueryValue(objReq, "memberGroupId", sGroupId, sizeof(sGroupId));
		if ( sGroupId[0] != '\0' ) iGroupId = atoll(sGroupId);
	}
	return iGroupId;
}

bool Managed_AccessGroupAllowed(const char* sGroupIds, int64 iGroupId)
{
	str sHaystack = NULL;
	str sNeedle = NULL;
	bool bAllowed = FALSE;

	if ( Managed_IsBlank(sGroupIds) ) return TRUE;
	if ( iGroupId <= 0 ) return FALSE;
	sHaystack = xrtFormat(",%s,", sGroupIds);
	sNeedle = xrtFormat(",%lld,", (long long)iGroupId);
	if ( sHaystack && sNeedle && strstr((const char*)sHaystack, (const char*)sNeedle) ) {
		bAllowed = TRUE;
	}
	if ( sNeedle ) xrtFree(sNeedle);
	if ( sHaystack ) xrtFree(sHaystack);
	return bAllowed;
}

str Managed_AccessPasswordEncodeLegacyXrt64(int64 iTargetId, const char* sPassword)
{
	unsigned long long uHash;

	if ( Managed_IsBlank(sPassword) ) return xrtCopyStr((str)"", 0);
	uHash = (unsigned long long)xrtHash64((str)sPassword, (uint32)strlen(sPassword));
	uHash ^= (unsigned long long)iTargetId * 11400714819323198485ull;
	return xrtFormat("xrt64:%lld:%016llx", (long long)iTargetId, uHash);
}

str Managed_AccessPasswordEncode(int64 iTargetId, const char* sPassword)
{
	str sSalt = NULL;
	str sUser = NULL;
	str sHash = NULL;
	str sStored = NULL;

	if ( Managed_IsBlank(sPassword) ) return xrtCopyStr((str)"", 0);
	if ( strncmp(sPassword, "xsha256:", 8) == 0 ) return xrtCopyStr((str)sPassword, 0);
	if ( strncmp(sPassword, "xrt64:", 6) == 0 ) return xrtCopyStr((str)sPassword, 0);
	sSalt = xrtMakeXIDS();
	sUser = xrtFormat("content.access.%lld", (long long)iTargetId);
	sHash = ServerHashPassword(sUser ? (str)sUser : (str)"content.access", sSalt ? (str)sSalt : (str)"", (str)sPassword);
	sStored = xrtFormat("xsha256:%lld:%s:%s", (long long)iTargetId, sSalt ? (const char*)sSalt : "", sHash ? (const char*)sHash : "");
	if ( sSalt ) xrtFree(sSalt);
	if ( sUser ) xrtFree(sUser);
	if ( sHash ) xrtFree(sHash);
	return sStored;
}

bool Managed_AccessPasswordMatches(int64 iTargetId, const char* sStored, const char* sInput)
{
	str sEncoded = NULL;
	bool bMatch = FALSE;

	if ( Managed_IsBlank(sStored) || Managed_IsBlank(sInput) ) return FALSE;
	if ( strncmp(sStored, "xsha256:", 8) == 0 ) {
		const char* sTargetPart = sStored + 8;
		const char* sSaltPart = strchr(sTargetPart, ':');
		const char* sHashPart = sSaltPart ? strchr(sSaltPart + 1, ':') : NULL;
		str sSalt = NULL;
		str sUser = NULL;
		str sHash = NULL;
		if ( sSaltPart && sHashPart && (sHashPart > sSaltPart + 1) ) {
			sSalt = xrtFormat("%.*s", (int)(sHashPart - sSaltPart - 1), sSaltPart + 1);
			sUser = xrtFormat("content.access.%lld", (long long)iTargetId);
			sHash = ServerHashPassword(sUser ? (str)sUser : (str)"content.access", sSalt ? (str)sSalt : (str)"", (str)sInput);
			bMatch = sHash && (strcmp((const char*)sHash, sHashPart + 1) == 0);
		}
		if ( sSalt ) xrtFree(sSalt);
		if ( sUser ) xrtFree(sUser);
		if ( sHash ) xrtFree(sHash);
		return bMatch;
	}
	if ( strncmp(sStored, "xrt64:", 6) == 0 ) {
		sEncoded = Managed_AccessPasswordEncodeLegacyXrt64(iTargetId, sInput);
		bMatch = sEncoded && (strcmp((const char*)sEncoded, sStored) == 0);
		if ( sEncoded ) xrtFree(sEncoded);
		return bMatch;
	}
	return strcmp(sStored, sInput) == 0;
}

bool Managed_AccessLoadRule(sqlite3* pDb, int64 iRuleTargetId, str* psMode, int64* pRequiredLevel, str* psPassword, str* psGroupIds, int64* pPrice)
{
	sqlite3_stmt* stmt = NULL;
	bool bFound = FALSE;

	if ( psMode ) *psMode = NULL;
	if ( pRequiredLevel ) *pRequiredLevel = 0;
	if ( psPassword ) *psPassword = NULL;
	if ( psGroupIds ) *psGroupIds = NULL;
	if ( pPrice ) *pPrice = 0;
	if ( pDb == NULL || iRuleTargetId == 0 ) return FALSE;
	if ( sqlite3_prepare_v2(pDb, "SELECT access_mode,required_read_level,password_hash,member_group_ids,price FROM content_access_rule WHERE content_id=? AND status=1 AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iRuleTargetId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			if ( psMode ) *psMode = xrtCopyStr((str)sqlite3_column_text(stmt, 0), 0);
			if ( pRequiredLevel ) *pRequiredLevel = sqlite3_column_int64(stmt, 1);
			if ( psPassword ) *psPassword = xrtCopyStr((str)sqlite3_column_text(stmt, 2), 0);
			if ( psGroupIds ) *psGroupIds = xrtCopyStr((str)sqlite3_column_text(stmt, 3), 0);
			if ( pPrice ) *pPrice = sqlite3_column_int64(stmt, 4);
			bFound = TRUE;
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return bFound;
}

int64 Managed_AccessLoadContentCategory(sqlite3* pDb, int64 iContentId)
{
	sqlite3_stmt* stmt = NULL;
	int64 iCategoryId = 0;

	if ( (pDb == NULL) || (iContentId <= 0) ) return 0;
	if ( sqlite3_prepare_v2(pDb, "SELECT category_id FROM content_item WHERE id=? AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) iCategoryId = sqlite3_column_int64(stmt, 0);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return iCategoryId;
}

int64 Managed_AccessLoadParentCategory(sqlite3* pDb, int64 iCategoryId)
{
	sqlite3_stmt* stmt = NULL;
	int64 iParentId = 0;

	if ( (pDb == NULL) || (iCategoryId <= 0) ) return 0;
	if ( sqlite3_prepare_v2(pDb, "SELECT parent_id FROM content_category WHERE id=? AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iCategoryId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) iParentId = sqlite3_column_int64(stmt, 0);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return iParentId;
}

bool Managed_AccessCheckRule(sqlite3* pDb, int64 iContentId, XS_RequestObject objReq, xvalue objSession, bool bAdmin, xvalue tblAccessOut)
{
	str sMode = NULL;
	str sRequiredPassword = NULL;
	str sGroupIds = NULL;
	int64 iRequiredLevel = 0;
	int64 iGroupId = 0;
	int64 iPrice = 0;
	int64 iRuleTargetId = iContentId;
	int64 iInheritedCategoryId = 0;
	int iDepth = 0;
	bool bAllowed = TRUE;
	bool bHasRule = FALSE;

	if ( bAdmin || !Managed_AbilityPackMounted("content.access") || (pDb == NULL) || (iContentId <= 0) ) {
		return TRUE;
	}
	bHasRule = Managed_AccessLoadRule(pDb, iContentId, &sMode, &iRequiredLevel, &sRequiredPassword, &sGroupIds, &iPrice);
	if ( !bHasRule && Managed_AbilityPackMounted("content.category") ) {
		int64 iCategoryId = Managed_AccessLoadContentCategory(pDb, iContentId);
		while ( iCategoryId > 0 && iDepth < 64 ) {
			if ( Managed_AccessLoadRule(pDb, -iCategoryId, &sMode, &iRequiredLevel, &sRequiredPassword, &sGroupIds, &iPrice) ) {
				iRuleTargetId = -iCategoryId;
				iInheritedCategoryId = iCategoryId;
				bHasRule = TRUE;
				break;
			}
			iCategoryId = Managed_AccessLoadParentCategory(pDb, iCategoryId);
			iDepth++;
		}
	}
	iGroupId = Managed_RequestMemberGroupId(objReq, objSession);
	if ( !bHasRule || Managed_IsBlank((const char*)sMode) || (strcmp((const char*)sMode, "public") == 0) ) {
		bAllowed = TRUE;
	} else if ( strcmp((const char*)sMode, "login") == 0 ) {
		bAllowed = Managed_RequestHasMemberSession(objSession);
	} else if ( strcmp((const char*)sMode, "level") == 0 ) {
		bAllowed = Managed_RequestReadLevel(objReq, objSession) >= iRequiredLevel;
	} else if ( strcmp((const char*)sMode, "group") == 0 ) {
		bAllowed = Managed_RequestHasMemberSession(objSession) && Managed_AccessGroupAllowed((const char*)sGroupIds, iGroupId);
	} else if ( strcmp((const char*)sMode, "password") == 0 ) {
		char sPassword[160];
		memset(sPassword, 0, sizeof(sPassword));
		if ( objReq ) xsReqQueryValue(objReq, "accessPassword", sPassword, sizeof(sPassword));
		bAllowed = Managed_AccessPasswordMatches(iRuleTargetId, (const char*)sRequiredPassword, sPassword);
	} else if ( strcmp((const char*)sMode, "paid") == 0 ) {
		/* No order subsystem is wired yet; expose price and deny by default. */
		bAllowed = FALSE;
	} else {
		bAllowed = FALSE;
	}
	if ( tblAccessOut && (xvoType(tblAccessOut) == XVO_DT_TABLE) ) {
		xvoTableSetBool(tblAccessOut, "allowed", 7, bAllowed);
		xvoTableSetText(tblAccessOut, "mode", 4, sMode ? sMode : (str)"public", 0, FALSE);
		xvoTableSetInt(tblAccessOut, "requiredReadLevel", 17, iRequiredLevel);
		xvoTableSetText(tblAccessOut, "memberGroupIds", 14, sGroupIds ? sGroupIds : (str)"", 0, FALSE);
		xvoTableSetInt(tblAccessOut, "memberGroupId", 13, iGroupId);
		xvoTableSetInt(tblAccessOut, "price", 5, iPrice);
		xvoTableSetBool(tblAccessOut, "payRequired", 11, sMode && (strcmp((const char*)sMode, "paid") == 0));
		xvoTableSetInt(tblAccessOut, "ruleTargetId", 12, iRuleTargetId);
		xvoTableSetInt(tblAccessOut, "inheritedCategoryId", 19, iInheritedCategoryId);
		xvoTableSetBool(tblAccessOut, "inherited", 9, iInheritedCategoryId > 0);
	}
	if ( sMode ) xrtFree(sMode);
	if ( sRequiredPassword ) xrtFree(sRequiredPassword);
	if ( sGroupIds ) xrtFree(sGroupIds);
	return bAllowed;
}

void Managed_AttachAccessInfo(sqlite3* pDb, xvalue tblItem, XS_RequestObject objReq, xvalue objSession, bool bAdmin)
{
	xvalue tblAccess = xvoCreateTable();
	int64 iContentId = tblItem ? xvoTableGetInt(tblItem, "id", 2) : 0;

	if ( tblItem == NULL ) {
		xvoUnref(tblAccess);
		return;
	}
	Managed_AccessCheckRule(pDb, iContentId, objReq, objSession, bAdmin, tblAccess);
	xvoTableSetValue(tblItem, "access", 6, tblAccess, TRUE);
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

const char* Managed_AbilityPackPrimaryTable(const char* sPackId)
{
	if ( sPackId == NULL ) return NULL;
	if ( strcmp(sPackId, "content.comment") == 0 ) return "comment_item";
	if ( strcmp(sPackId, "content.tag") == 0 ) return "tag";
	if ( strcmp(sPackId, "content.topic") == 0 ) return "topic";
	if ( strcmp(sPackId, "content.sensitive") == 0 ) return "sensitive_word";
	if ( strcmp(sPackId, "content.static") == 0 ) return "static_rule";
	if ( strcmp(sPackId, "content.like") == 0 ) return "like_counter";
	if ( strcmp(sPackId, "content.view-stat") == 0 ) return "view_counter";
	return NULL;
}

xvalue Managed_FindAbilityPackContract(xvalue tblContracts, const char* sPackId)
{
	xvalue arrPacks = tblContracts ? xvoTableGetValue(tblContracts, "abilityPacks", 12) : NULL;
	if ( (sPackId == NULL) || (arrPacks == NULL) || (xvoType(arrPacks) != XVO_DT_ARRAY) ) {
		return NULL;
	}
	for ( uint32 i = 0; i < xvoArrayItemCount(arrPacks); i++ ) {
		xvalue tblPack = xvoArrayGetValue(arrPacks, i);
		str sCurrent = (tblPack && (xvoType(tblPack) == XVO_DT_TABLE)) ? xvoTableGetText(tblPack, "packId", 6) : NULL;
		if ( sCurrent && strcmp((const char*)sCurrent, sPackId) == 0 ) {
			return tblPack;
		}
	}
	return NULL;
}

xvalue Managed_AbilityPackConfig(xvalue tblContracts, const char* sPackId)
{
	xvalue tblPack = Managed_FindAbilityPackContract(tblContracts, sPackId);
	xvalue tblConfig = tblPack ? xvoTableGetValue(tblPack, "instanceConfig", 14) : NULL;
	return (tblConfig && (xvoType(tblConfig) == XVO_DT_TABLE)) ? tblConfig : NULL;
}

bool Managed_AbilityPackConfigBool(const char* sPackId, const char* sName, bool bDefault)
{
	xvalue tblContracts = Managed_LoadContractsMeta();
	xvalue tblConfig = Managed_AbilityPackConfig(tblContracts, sPackId);
	xvalue objValue = (tblConfig && sName) ? xvoTableGetValue(tblConfig, sName, (int)strlen(sName)) : NULL;
	bool bRet = bDefault;
	if ( objValue != NULL ) {
		if ( xvoType(objValue) == XVO_DT_BOOL ) {
			bRet = xvoGetBool(objValue) ? TRUE : FALSE;
		} else if ( xvoType(objValue) == XVO_DT_INT ) {
			bRet = xvoGetInt(objValue) != 0;
		} else if ( xvoType(objValue) == XVO_DT_TEXT ) {
			const char* sText = xvoGetText(objValue);
			bRet = Managed_TextEqualsIgnoreCase(sText, "true") || (sText && strcmp(sText, "1") == 0) || Managed_TextEqualsIgnoreCase(sText, "on");
		}
	}
	if ( tblContracts ) xvoUnref(tblContracts);
	return bRet;
}

str Managed_AbilityPackConfigTextDup(const char* sPackId, const char* sName, const char* sDefault)
{
	xvalue tblContracts = Managed_LoadContractsMeta();
	xvalue tblConfig = Managed_AbilityPackConfig(tblContracts, sPackId);
	xvalue objValue = (tblConfig && sName) ? xvoTableGetValue(tblConfig, sName, (int)strlen(sName)) : NULL;
	const char* sText = NULL;
	str sRet;
	if ( objValue != NULL ) {
		if ( xvoType(objValue) == XVO_DT_TEXT ) {
			sText = xvoGetText(objValue);
		} else if ( xvoType(objValue) == XVO_DT_BOOL ) {
			sText = xvoGetBool(objValue) ? "true" : "false";
		}
	}
	sRet = xrtCopyStr((str)(Managed_IsBlank(sText) ? (sDefault ? sDefault : "") : sText), 0);
	if ( tblContracts ) xvoUnref(tblContracts);
	return sRet;
}

int Managed_AbilityPackConfigInt(const char* sPackId, const char* sName, int iDefault)
{
	xvalue tblContracts = Managed_LoadContractsMeta();
	xvalue tblConfig = Managed_AbilityPackConfig(tblContracts, sPackId);
	xvalue objValue = (tblConfig && sName) ? xvoTableGetValue(tblConfig, sName, (int)strlen(sName)) : NULL;
	int iRet = iDefault;
	if ( objValue != NULL ) {
		if ( xvoType(objValue) == XVO_DT_INT ) {
			iRet = (int)xvoGetInt(objValue);
		} else if ( xvoType(objValue) == XVO_DT_FLOAT ) {
			iRet = (int)xvoGetFloat(objValue);
		} else if ( xvoType(objValue) == XVO_DT_TEXT ) {
			iRet = atoi(xvoGetText(objValue));
		}
	}
	if ( tblContracts ) xvoUnref(tblContracts);
	return iRet;
}

xvalue Managed_AbilityPackConfigArrayDup(const char* sPackId, const char* sName)
{
	xvalue tblContracts = Managed_LoadContractsMeta();
	xvalue tblConfig = Managed_AbilityPackConfig(tblContracts, sPackId);
	xvalue objValue = (tblConfig && sName) ? xvoTableGetValue(tblConfig, sName, (int)strlen(sName)) : NULL;
	xvalue arrRet = NULL;
	if ( objValue && (xvoType(objValue) == XVO_DT_ARRAY) ) {
		arrRet = xvoCopy(objValue);
	}
	if ( tblContracts ) xvoUnref(tblContracts);
	return arrRet;
}

bool Managed_ArrayContainsText(xvalue arrList, const char* sText)
{
	if ( Managed_IsBlank(sText) || (arrList == NULL) || (xvoType(arrList) != XVO_DT_ARRAY) ) {
		return FALSE;
	}
	for ( uint32 i = 0; i < xvoArrayItemCount(arrList); i++ ) {
		xvalue objItem = xvoArrayGetValue(arrList, i);
		const char* sItem = (objItem && (xvoType(objItem) == XVO_DT_TEXT)) ? xvoGetText(objItem) : NULL;
		if ( !Managed_IsBlank(sItem) && (strcmp(sItem, sText) == 0) ) {
			return TRUE;
		}
	}
	return FALSE;
}

void Managed_SetSqliteColumnValue(xvalue tblRow, sqlite3_stmt* stmt, int iCol)
{
	const char* sName = sqlite3_column_name(stmt, iCol);
	int iType = sqlite3_column_type(stmt, iCol);

	if ( Managed_IsBlank(sName) ) {
		return;
	}
	if ( iType == SQLITE_INTEGER ) {
		xvoTableSetInt(tblRow, sName, (int)strlen(sName), sqlite3_column_int64(stmt, iCol));
	} else if ( iType == SQLITE_FLOAT ) {
		xvoTableSetFloat(tblRow, sName, (int)strlen(sName), sqlite3_column_double(stmt, iCol));
	} else if ( iType == SQLITE_NULL ) {
		xvoTableSetNull(tblRow, sName, (int)strlen(sName));
	} else {
		xvoTableSetText(tblRow, sName, (int)strlen(sName), (str)sqlite3_column_text(stmt, iCol), 0, FALSE);
	}
}

void Managed_RequestAbilityPackMetaAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sPackId[128];
	xvalue tblContracts = Managed_LoadContractsMeta();
	xvalue tblPack = NULL;
	xvalue tblRet = NULL;
	xvalue tblData = NULL;
	const char* sTable = NULL;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	Managed_ReadTextQuery(objReq, "pack", sPackId, sizeof(sPackId));
	if ( Managed_IsBlank(sPackId) ) {
		if ( tblContracts ) xvoUnref(tblContracts);
		Managed_SendError(objResp, "missing pack");
		return;
	}
	tblPack = Managed_FindAbilityPackContract(tblContracts, sPackId);
	if ( tblPack == NULL ) {
		if ( tblContracts ) xvoUnref(tblContracts);
		Managed_SendError(objResp, "ability pack is not mounted");
		return;
	}
	sTable = Managed_AbilityPackPrimaryTable(sPackId);
	tblRet = Managed_CreateResult(TRUE, "ok");
	tblData = xvoCreateTable();
	xvoTableSetText(tblData, "packId", 6, sPackId, 0, FALSE);
	xvoTableSetText(tblData, "table", 5, (str)(sTable ? sTable : ""), 0, FALSE);
	xvoTableSetValue(tblData, "pack", 4, xvoCopy(tblPack), TRUE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	if ( tblContracts ) xvoUnref(tblContracts);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestAbilityPackListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sPackId[128];
	char sStatus[32];
	const char* sTable = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	str sSql = NULL;
	xvalue tblContracts = NULL;
	xvalue tblPack = NULL;
	xvalue tblRet = NULL;
	xvalue arrList = xvoCreateArray();
	int iPage = Managed_ReadIntQuery(objReq, "page", 1);
	int iLimit = Managed_ReadIntQuery(objReq, "limit", 20);
	int iOffset;
	int iCount = 0;
	int iStatus = 0;
	bool bFilterStatus = FALSE;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	Managed_ReadTextQuery(objReq, "pack", sPackId, sizeof(sPackId));
	Managed_ReadTextQuery(objReq, "status", sStatus, sizeof(sStatus));
	if ( iPage < 1 ) iPage = 1;
	if ( iLimit < 1 ) iLimit = 20;
	if ( iLimit > 200 ) iLimit = 200;
	iOffset = (iPage - 1) * iLimit;

	if ( Managed_IsBlank(sPackId) ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "missing pack");
		return;
	}
	sTable = Managed_AbilityPackPrimaryTable(sPackId);
	if ( sTable == NULL ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "unsupported ability pack table");
		return;
	}
	tblContracts = Managed_LoadContractsMeta();
	tblPack = Managed_FindAbilityPackContract(tblContracts, sPackId);
	if ( tblPack == NULL ) {
		if ( tblContracts ) xvoUnref(tblContracts);
		xvoUnref(arrList);
		Managed_SendError(objResp, "ability pack is not mounted");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( tblContracts ) xvoUnref(tblContracts);
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( !Managed_IsBlank(sStatus) && Managed_TableColumnExists(pDb, sTable, "status") ) {
		iStatus = atoi(sStatus);
		bFilterStatus = TRUE;
	}
	sSql = bFilterStatus ? xrtFormat("SELECT COUNT(*) FROM %s WHERE status=?", sTable) : xrtFormat("SELECT COUNT(*) FROM %s", sTable);
	if ( sSql && (sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK) ) {
		if ( bFilterStatus ) sqlite3_bind_int(stmt, 1, iStatus);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iCount = sqlite3_column_int(stmt, 0);
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if ( sSql ) xrtFree(sSql);
	sSql = bFilterStatus ? xrtFormat("SELECT * FROM %s WHERE status=? ORDER BY id DESC LIMIT ? OFFSET ?", sTable) : xrtFormat("SELECT * FROM %s ORDER BY id DESC LIMIT ? OFFSET ?", sTable);
	if ( sSql && (sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK) ) {
		if ( bFilterStatus ) {
			sqlite3_bind_int(stmt, 1, iStatus);
			sqlite3_bind_int(stmt, 2, iLimit);
			sqlite3_bind_int(stmt, 3, iOffset);
		} else {
			sqlite3_bind_int(stmt, 1, iLimit);
			sqlite3_bind_int(stmt, 2, iOffset);
		}
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblRow = xvoCreateTable();
			int iCols = sqlite3_column_count(stmt);
			for ( int i = 0; i < iCols; i++ ) {
				Managed_SetSqliteColumnValue(tblRow, stmt, i);
			}
			xvoArrayAppendValue(arrList, tblRow, TRUE);
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if ( sSql ) xrtFree(sSql);
	Managed_CloseDb(pDb);
	if ( tblContracts ) xvoUnref(tblContracts);

	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetText(tblRet, "packId", 6, sPackId, 0, FALSE);
	xvoTableSetText(tblRet, "table", 5, (str)sTable, 0, FALSE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetInt(tblRet, "page", 4, iPage);
	xvoTableSetInt(tblRet, "pageSize", 8, iLimit);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

bool Managed_AbilityPackMounted(const char* sPackId)
{
	xvalue tblContracts = Managed_LoadContractsMeta();
	xvalue tblPack = Managed_FindAbilityPackContract(tblContracts, sPackId);
	bool bMounted = tblPack != NULL;
	if ( tblContracts ) xvoUnref(tblContracts);
	return bMounted;
}

void Managed_AppendCommentRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();
	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblRow, "contentId", 9, sqlite3_column_int64(stmt, 1));
	xvoTableSetInt(tblRow, "parentId", 8, sqlite3_column_int64(stmt, 2));
	xvoTableSetText(tblRow, "authorName", 10, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetText(tblRow, "body", 4, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetInt(tblRow, "status", 6, sqlite3_column_int(stmt, 5));
	xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 6));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

xvalue Managed_CopyCommentNodeWithChildren(xvalue arrList, xvalue tblRow)
{
	xvalue tblNode = xvoCopy(tblRow);
	xvalue arrChildren = xvoCreateArray();
	int64 iId = xvoTableGetInt(tblRow, "id", 2);
	if ( (arrList != NULL) && (xvoType(arrList) == XVO_DT_ARRAY) ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrList); i++ ) {
			xvalue tblChild = xvoArrayGetValue(arrList, i);
			if ( (tblChild == NULL) || (xvoType(tblChild) != XVO_DT_TABLE) ) continue;
			if ( xvoTableGetInt(tblChild, "parentId", 8) == iId ) {
				xvoArrayAppendValue(arrChildren, Managed_CopyCommentNodeWithChildren(arrList, tblChild), TRUE);
			}
		}
	}
	xvoTableSetValue(tblNode, "children", 8, arrChildren, TRUE);
	return tblNode;
}

xvalue Managed_BuildCommentTree(xvalue arrList)
{
	xvalue arrTree = xvoCreateArray();
	if ( (arrList == NULL) || (xvoType(arrList) != XVO_DT_ARRAY) ) return arrTree;
	for ( uint32 i = 0; i < xvoArrayItemCount(arrList); i++ ) {
		xvalue tblRow = xvoArrayGetValue(arrList, i);
		if ( (tblRow == NULL) || (xvoType(tblRow) != XVO_DT_TABLE) ) continue;
		if ( xvoTableGetInt(tblRow, "parentId", 8) <= 0 ) {
			xvoArrayAppendValue(arrTree, Managed_CopyCommentNodeWithChildren(arrList, tblRow), TRUE);
		}
	}
	return arrTree;
}

void Managed_RefreshCommentThreadCounts(sqlite3* pDb, int64 iContentId)
{
	sqlite3_stmt* stmt = NULL;
	int64 iNow = xrtNow();

	if ( (pDb == NULL) || (iContentId <= 0) ) {
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE comment_thread SET comment_count=(SELECT COUNT(*) FROM comment_item WHERE content_id=? AND delete_time=0), visible_count=(SELECT COUNT(*) FROM comment_item WHERE content_id=? AND status=1 AND delete_time=0), update_time=? WHERE content_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iContentId);
		sqlite3_bind_int64(stmt, 3, iNow);
		sqlite3_bind_int64(stmt, 4, (sqlite3_int64)iContentId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
}

void Managed_RequestCommentListPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sContentId[32];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	xvalue arrList = xvoCreateArray();
	int64 iContentId = 0;

	(void)objServer;
	(void)objHost;

	if ( !Managed_AbilityPackMounted("content.comment") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "comment ability pack is not enabled");
		return;
	}
	Managed_ReadTextQuery(objReq, "contentId", sContentId, sizeof(sContentId));
	iContentId = atoll(sContentId);
	if ( iContentId <= 0 ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "missing contentId");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id,content_id,parent_id,author_name,body,status,create_time FROM comment_item WHERE content_id=? AND status=1 AND delete_time=0 ORDER BY create_time ASC,id ASC", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendCommentRow(arrList, stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	xvoTableSetValue(tblRet, "tree", 4, Managed_BuildCommentTree(arrList), TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestCommentCountPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sContentId[32];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	int64 iContentId = 0;
	int64 iCommentCount = 0;
	int64 iVisibleCount = 0;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !Managed_AbilityPackMounted("content.comment") ) {
		Managed_SendError(objResp, "comment ability pack is not enabled");
		return;
	}
	Managed_ReadTextQuery(objReq, "contentId", sContentId, sizeof(sContentId));
	iContentId = atoll(sContentId);
	if ( iContentId <= 0 ) {
		Managed_SendError(objResp, "missing contentId");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	Managed_RefreshCommentThreadCounts(pDb, iContentId);
	if ( sqlite3_prepare_v2(pDb, "SELECT comment_count,visible_count FROM comment_thread WHERE content_id=? LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iCommentCount = sqlite3_column_int64(stmt, 0);
			iVisibleCount = sqlite3_column_int64(stmt, 1);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetInt(tblRet, "contentId", 9, iContentId);
	xvoTableSetInt(tblRet, "commentCount", 12, iCommentCount);
	xvoTableSetInt(tblRet, "visibleCount", 12, iVisibleCount);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestCommentCreatePublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	str sAuthorName = NULL;
	str sBody = NULL;
	int64 iContentId = 0;
	int64 iParentId = 0;
	int64 iThreadId = 0;
	int64 iNow = xrtNow();
	int iStatus = 0;
	str sModeration = NULL;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !Managed_AbilityPackMounted("content.comment") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "comment ability pack is not enabled");
		return;
	}
	if ( !Managed_AbilityPackConfigBool("content.comment", "allowPublicPost", TRUE) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "public comment posting is disabled");
		return;
	}
	if ( (tblBody == NULL) || (xvoType(tblBody) != XVO_DT_TABLE) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "invalid request body");
		return;
	}
	sModeration = Managed_AbilityPackConfigTextDup("content.comment", "moderation", "manual");
	if ( sModeration && strcmp((const char*)sModeration, "auto") == 0 ) {
		iStatus = 1;
	}
	iContentId = xvoTableGetInt(tblBody, "contentId", 9);
	iParentId = xvoTableGetInt(tblBody, "parentId", 8);
	sAuthorName = xvoTableGetText(tblBody, "authorName", 10);
	sBody = xvoTableGetText(tblBody, "body", 4);
	if ( (iContentId <= 0) || Managed_IsBlank(sBody) ) {
		if ( sModeration ) xrtFree(sModeration);
		xvoUnref(tblBody);
		Managed_SendError(objResp, "contentId and body are required");
		return;
	}
	if ( Managed_IsBlank(sAuthorName) ) {
		sAuthorName = "Guest";
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( sModeration ) xrtFree(sModeration);
		xvoUnref(tblBody);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "INSERT OR IGNORE INTO comment_thread(content_id,comment_count,visible_count,status,create_time,update_time) VALUES(?,0,0,1,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_int64(stmt, 2, iNow);
		sqlite3_bind_int64(stmt, 3, iNow);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id FROM comment_thread WHERE content_id=? LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iThreadId = sqlite3_column_int64(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}
	stmt = NULL;
	if ( iParentId > 0 ) {
		bool bParentOk = FALSE;
		if ( sqlite3_prepare_v2(pDb, "SELECT id FROM comment_item WHERE id=? AND content_id=? AND status=1 AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iParentId);
			sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iContentId);
			bParentOk = (sqlite3_step(stmt) == SQLITE_ROW);
			sqlite3_finalize(stmt);
			stmt = NULL;
		}
		if ( !bParentOk ) {
			Managed_CloseDb(pDb);
			if ( sModeration ) xrtFree(sModeration);
			xvoUnref(tblBody);
			Managed_SendError(objResp, "parent comment not found");
			return;
		}
	}
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO comment_item(content_id,thread_id,parent_id,author_name,author_id,body,ip,user_agent,status,create_time,update_time,delete_time) VALUES(?,?,?,?, '', ?, '', '', ?, ?, ?, 0)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iThreadId);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)(iParentId > 0 ? iParentId : 0));
		sqlite3_bind_text(stmt, 4, sAuthorName ? (const char*)sAuthorName : "Guest", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 5, sBody ? (const char*)sBody : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 6, iStatus);
		sqlite3_bind_int64(stmt, 7, iNow);
		sqlite3_bind_int64(stmt, 8, iNow);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	Managed_RefreshCommentThreadCounts(pDb, iContentId);
	Managed_AuditLogWithRequest(pDb, "comment", sqlite3_last_insert_rowid(pDb), "comment.create", "create comment", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	if ( sModeration ) xrtFree(sModeration);
	xvoUnref(tblBody);
	tblRet = Managed_CreateResult(TRUE, "comment submitted");
	xvoTableSetInt(tblRet, "status", 6, iStatus);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestCommentHidePublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = 0;
	int64 iContentId = 0;
	int64 iNow = xrtNow();

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !Managed_AbilityPackMounted("content.comment") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "comment ability pack is not enabled");
		return;
	}
	if ( (tblBody == NULL) || (xvoType(tblBody) != XVO_DT_TABLE) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "invalid request body");
		return;
	}
	iId = xvoTableGetInt(tblBody, "id", 2);
	if ( iId <= 0 ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "missing id");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		xvoUnref(tblBody);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT content_id FROM comment_item WHERE id=? AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iContentId = sqlite3_column_int64(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE comment_item SET status=2, update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, iNow);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	Managed_RefreshCommentThreadCounts(pDb, iContentId);
	Managed_AuditLogWithRequest(pDb, "comment", iId, "comment.hide", "hide comment", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "hidden"));
}

void Managed_RequestCommentStatusAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = 0;
	int64 iContentId = 0;
	int iStatus = 0;
	int64 iNow = xrtNow();

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !Managed_AbilityPackMounted("content.comment") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "comment ability pack is not enabled");
		return;
	}
	if ( (tblBody == NULL) || (xvoType(tblBody) != XVO_DT_TABLE) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "invalid request body");
		return;
	}
	iId = xvoTableGetInt(tblBody, "id", 2);
	iStatus = (int)xvoTableGetInt(tblBody, "status", 6);
	if ( iId <= 0 ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "missing id");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		xvoUnref(tblBody);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT content_id FROM comment_item WHERE id=? LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iContentId = sqlite3_column_int64(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE comment_item SET status=?, update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iStatus);
		sqlite3_bind_int64(stmt, 2, iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	Managed_RefreshCommentThreadCounts(pDb, iContentId);
	Managed_AuditLogWithRequest(pDb, "comment", iId, "comment.status", "update comment status", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "ok"));
}

void Managed_RequestCommentDeleteAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = 0;
	int64 iContentId = 0;
	int64 iNow = xrtNow();

	(void)objServer;
	(void)objHost;

	if ( !Managed_AbilityPackMounted("content.comment") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "comment ability pack is not enabled");
		return;
	}
	if ( (tblBody == NULL) || (xvoType(tblBody) != XVO_DT_TABLE) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "invalid request body");
		return;
	}
	iId = xvoTableGetInt(tblBody, "id", 2);
	if ( iId <= 0 ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "missing id");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		xvoUnref(tblBody);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT content_id FROM comment_item WHERE id=? AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iContentId = sqlite3_column_int64(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE comment_item SET delete_time=?, update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, iNow);
		sqlite3_bind_int64(stmt, 2, iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	Managed_RefreshCommentThreadCounts(pDb, iContentId);
	Managed_AuditLogWithRequest(pDb, "comment", iId, "comment.delete", "delete comment", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "deleted"));
}

void Managed_AppendTagRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();
	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
	xvoTableSetText(tblRow, "slug", 4, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetText(tblRow, "description", 11, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int(stmt, 4));
	xvoTableSetInt(tblRow, "status", 6, sqlite3_column_int(stmt, 5));
	xvoTableSetInt(tblRow, "contentCount", 12, sqlite3_column_int(stmt, 6));
	xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 7));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_UpdateTagCounters(sqlite3* pDb)
{
	sqlite3_stmt* stmt = NULL;
	if ( pDb == NULL ) return;
	if ( sqlite3_prepare_v2(pDb, "UPDATE tag SET content_count=(SELECT COUNT(*) FROM content_tag WHERE content_tag.tag_id=tag.id) WHERE delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
}

void Managed_AppendTagContentRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();
	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblRow, "tagId", 5, sqlite3_column_int64(stmt, 1));
	xvoTableSetText(tblRow, "tagName", 7, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetInt(tblRow, "contentId", 9, sqlite3_column_int64(stmt, 3));
	xvoTableSetText(tblRow, "contentTitle", 12, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int(stmt, 5));
	xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 6));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_RequestTagListPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	xvalue arrList = xvoCreateArray();

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	if ( !Managed_AbilityPackMounted("content.tag") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "tag ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	Managed_UpdateTagCounters(pDb);
	if ( sqlite3_prepare_v2(pDb, "SELECT id,name,slug,description,sort,status,content_count,create_time FROM tag WHERE status=1 AND delete_time=0 ORDER BY sort ASC,id ASC", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendTagRow(arrList, stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestTagDetailPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sId[32];
	char sSlug[160];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblRow = NULL;
	xvalue tblRet = NULL;
	int64 iId = 0;

	(void)objServer;
	(void)objHost;

	if ( !Managed_AbilityPackMounted("content.tag") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "tag ability pack is not enabled");
		return;
	}
	memset(sId, 0, sizeof(sId));
	memset(sSlug, 0, sizeof(sSlug));
	Managed_ReadTextQuery(objReq, "id", sId, sizeof(sId));
	Managed_ReadTextQuery(objReq, "slug", sSlug, sizeof(sSlug));
	iId = atoll(sId);
	if ( (iId <= 0) && Managed_IsBlank(sSlug) ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "id or slug is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	Managed_UpdateTagCounters(pDb);
	if ( sqlite3_prepare_v2(pDb, (iId > 0) ? "SELECT id,name,slug,description,sort,status,content_count,create_time FROM tag WHERE id=? AND status=1 AND delete_time=0 LIMIT 1" : "SELECT id,name,slug,description,sort,status,content_count,create_time FROM tag WHERE slug=? AND status=1 AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( iId > 0 ) sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		else sqlite3_bind_text(stmt, 1, sSlug, -1, SQLITE_TRANSIENT);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendTagRow(arrList, stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	if ( xvoArrayItemCount(arrList) > 0 ) {
		tblRow = xvoCopy(xvoArrayGetValue(arrList, 0));
	}
	xvoUnref(arrList);
	if ( tblRow == NULL ) {
		Managed_SendError(objResp, "tag not found");
		return;
	}
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, tblRow, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestTagContentListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sTagId[32] = {0};
	char sContentId[32] = {0};
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblRet = NULL;
	int64 iTagId = 0;
	int64 iContentId = 0;
	const char* sSql = NULL;

	(void)objServer; (void)objHost;
	Managed_ReadTextQuery(objReq, "tagId", sTagId, sizeof(sTagId));
	Managed_ReadTextQuery(objReq, "contentId", sContentId, sizeof(sContentId));
	iTagId = atoll(sTagId);
	iContentId = atoll(sContentId);
	if ( !Managed_AbilityPackMounted("content.tag") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "tag ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( iContentId > 0 ) {
		sSql = "SELECT ct.id,ct.tag_id,t.name,ct.content_id,c.title,ct.sort,ct.create_time FROM content_tag ct LEFT JOIN tag t ON t.id=ct.tag_id LEFT JOIN content_item c ON c.id=ct.content_id WHERE ct.content_id=? ORDER BY ct.create_time DESC,ct.id DESC";
	} else if ( iTagId > 0 ) {
		sSql = "SELECT ct.id,ct.tag_id,t.name,ct.content_id,c.title,ct.sort,ct.create_time FROM content_tag ct LEFT JOIN tag t ON t.id=ct.tag_id LEFT JOIN content_item c ON c.id=ct.content_id WHERE ct.tag_id=? ORDER BY ct.create_time DESC,ct.id DESC";
	} else {
		sSql = "SELECT ct.id,ct.tag_id,t.name,ct.content_id,c.title,ct.sort,ct.create_time FROM content_tag ct LEFT JOIN tag t ON t.id=ct.tag_id LEFT JOIN content_item c ON c.id=ct.content_id ORDER BY ct.create_time DESC,ct.id DESC";
	}
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
		if ( iContentId > 0 ) sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		else if ( iTagId > 0 ) sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTagId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendTagContentRow(arrList, stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestTagUnbindAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	(void)objServer; (void)objHost; (void)objReq;
	if ( !Managed_AbilityPackMounted("content.tag") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "tag ability pack is not enabled");
		return;
	}
	if ( iId <= 0 ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "missing id");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( tblBody ) xvoUnref(tblBody);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM content_tag WHERE id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	Managed_UpdateTagCounters(pDb);
	Managed_AuditLogWithRequest(pDb, "tag", iId, "tag.unbind", "unbind tag content", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	if ( tblBody ) xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "unbound"));
}

void Managed_RequestTagContentsPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sTagId[32];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblSpec = Managed_LoadSpec();
	int64 iTagId = 0;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !Managed_AbilityPackMounted("content.tag") ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(arrList);
		Managed_SendError(objResp, "tag ability pack is not enabled");
		return;
	}
	Managed_ReadTextQuery(objReq, "tagId", sTagId, sizeof(sTagId));
	iTagId = atoll(sTagId);
	if ( iTagId <= 0 ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(arrList);
		Managed_SendError(objResp, "missing tagId");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(arrList);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT c.id,c.title,c.status,c.payload_json,c.is_draft,c.create_time,c.update_time,c.category_id FROM content_item c INNER JOIN content_tag ct ON ct.content_id=c.id WHERE ct.tag_id=? AND c.delete_time=0 AND c.is_draft=0 AND c.status>=1 ORDER BY c.update_time DESC,c.id DESC", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTagId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendRow(arrList, stmt, tblSpec);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	if ( tblSpec ) xvoUnref(tblSpec);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestTagSaveAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	const char* sName = NULL;
	const char* sSlug = NULL;
	const char* sDescription = NULL;
	int64 iId = 0;
	int iSort = 0;
	int iStatus = 1;
	int64 iNow = xrtNow();
	bool bInsert = FALSE;

	(void)objServer;
	(void)objHost;

	if ( !Managed_AbilityPackMounted("content.tag") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "tag ability pack is not enabled");
		return;
	}
	if ( tblBody == NULL ) {
		Managed_SendError(objResp, "invalid request body");
		return;
	}
	iId = xvoTableGetInt(tblBody, "id", 2);
	bInsert = iId <= 0;
	sName = xvoTableGetText(tblBody, "name", 4);
	sSlug = xvoTableGetText(tblBody, "slug", 4);
	sDescription = xvoTableGetText(tblBody, "description", 11);
	iSort = (int)xvoTableGetInt(tblBody, "sort", 4);
	iStatus = (int)xvoTableGetInt(tblBody, "status", 6);
	if ( Managed_IsBlank(sName) ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "name is required");
		return;
	}
	if ( Managed_IsBlank(sSlug) ) {
		sSlug = sName;
	}
	if ( iStatus <= 0 ) iStatus = 1;
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( iId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE tag SET name=?,slug=?,description=?,sort=?,status=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sName, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, sSlug, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sDescription ? sDescription : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 4, iSort);
			sqlite3_bind_int(stmt, 5, iStatus);
			sqlite3_bind_int64(stmt, 6, iNow);
			sqlite3_bind_int64(stmt, 7, (sqlite3_int64)iId);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	} else {
		if ( sqlite3_prepare_v2(pDb, "INSERT INTO tag(name,slug,description,sort,status,content_count,create_time,update_time,delete_time) VALUES(?,?,?,?,?,0,?,?,0)", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sName, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, sSlug, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sDescription ? sDescription : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 4, iSort);
			sqlite3_bind_int(stmt, 5, iStatus);
			sqlite3_bind_int64(stmt, 6, iNow);
			sqlite3_bind_int64(stmt, 7, iNow);
			sqlite3_step(stmt);
			iId = sqlite3_last_insert_rowid(pDb);
			sqlite3_finalize(stmt);
		}
	}
	Managed_AuditLogWithRequest(pDb, "tag", iId, bInsert ? "tag.create" : "tag.update", sName ? sName : "", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	xvoUnref(tblBody);
	tblRet = Managed_CreateResult(TRUE, "saved");
	xvoTableSetInt(tblRet, "id", 2, iId);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestTagDeleteAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = 0;
	int64 iNow = xrtNow();

	(void)objServer;
	(void)objHost;

	if ( !Managed_AbilityPackMounted("content.tag") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "tag ability pack is not enabled");
		return;
	}
	if ( tblBody == NULL ) {
		Managed_SendError(objResp, "invalid request body");
		return;
	}
	iId = xvoTableGetInt(tblBody, "id", 2);
	if ( iId <= 0 ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "missing id");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE tag SET delete_time=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, iNow);
		sqlite3_bind_int64(stmt, 2, iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM content_tag WHERE tag_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	Managed_AuditLogWithRequest(pDb, "tag", iId, "tag.delete", "delete tag", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "deleted"));
}

int64 Managed_EnsureTagByName(sqlite3* pDb, const char* sName, int64 iNow)
{
	sqlite3_stmt* stmt = NULL;
	int64 iTagId = 0;
	if ( (pDb == NULL) || Managed_IsBlank(sName) ) {
		return 0;
	}
	if ( sqlite3_prepare_v2(pDb, "INSERT OR IGNORE INTO tag(name,slug,description,sort,status,content_count,create_time,update_time,delete_time) VALUES(?,?, '',0,1,0,?,?,0)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sName, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, sName, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 3, iNow);
		sqlite3_bind_int64(stmt, 4, iNow);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( sqlite3_prepare_v2(pDb, "SELECT id FROM tag WHERE slug=? AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sName, -1, SQLITE_TRANSIENT);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iTagId = sqlite3_column_int64(stmt, 0);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return iTagId;
}

void Managed_RequestTagBindAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	xvalue arrTagIds = NULL;
	xvalue arrTagNames = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iContentId = 0;
	int64 iNow = xrtNow();
	int iMaxTags = Managed_AbilityPackConfigInt("content.tag", "maxTags", 0);
	int iTagCount = 0;
	bool bOwnTagIds = FALSE;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !Managed_AbilityPackMounted("content.tag") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "tag ability pack is not enabled");
		return;
	}
	if ( tblBody == NULL ) {
		Managed_SendError(objResp, "invalid request body");
		return;
	}
	iContentId = xvoTableGetInt(tblBody, "contentId", 9);
	arrTagIds = xvoTableGetValue(tblBody, "tagIds", 6);
	arrTagNames = xvoTableGetValue(tblBody, "tagNames", 8);
	if ( (arrTagNames == NULL) || (xvoType(arrTagNames) != XVO_DT_ARRAY) ) {
		arrTagNames = xvoTableGetValue(tblBody, "tags", 4);
	}
	if ( iContentId <= 0 ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "contentId is required");
		return;
	}
	if ( (arrTagIds == NULL) || (xvoType(arrTagIds) != XVO_DT_ARRAY) ) {
		arrTagIds = xvoCreateArray();
		bOwnTagIds = TRUE;
	}
	for ( uint32 i = 0; i < xvoArrayItemCount(arrTagIds); i++ ) {
		xvalue objTagId = xvoArrayGetValue(arrTagIds, i);
		if ( xvoGetInt(objTagId) > 0 ) iTagCount++;
	}
	if ( (arrTagNames != NULL) && (xvoType(arrTagNames) == XVO_DT_ARRAY) ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrTagNames); i++ ) {
			xvalue objName = xvoArrayGetValue(arrTagNames, i);
			const char* sName = xvoGetText(objName);
			if ( !Managed_IsBlank(sName) ) iTagCount++;
		}
	}
	if ( (iMaxTags > 0) && (iTagCount > iMaxTags) ) {
		if ( bOwnTagIds ) xvoUnref(arrTagIds);
		xvoUnref(tblBody);
		Managed_SendError(objResp, "tag count exceeds maxTags");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( bOwnTagIds ) xvoUnref(arrTagIds);
		xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( (arrTagNames != NULL) && (xvoType(arrTagNames) == XVO_DT_ARRAY) ) {
		if ( !Managed_AbilityPackConfigBool("content.tag", "allowCreateInline", TRUE) ) {
			Managed_CloseDb(pDb);
			if ( bOwnTagIds ) xvoUnref(arrTagIds);
			xvoUnref(tblBody);
			Managed_SendError(objResp, "inline tag creation is disabled");
			return;
		}
		for ( uint32 i = 0; i < xvoArrayItemCount(arrTagNames); i++ ) {
			xvalue objName = xvoArrayGetValue(arrTagNames, i);
			const char* sName = xvoGetText(objName);
			int64 iTagId = Managed_EnsureTagByName(pDb, sName, iNow);
			if ( iTagId > 0 ) {
				xvoArrayAppendValue(arrTagIds, xvoCreateInt(iTagId), TRUE);
			}
		}
	}
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM content_tag WHERE content_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	if ( sqlite3_prepare_v2(pDb, "INSERT OR IGNORE INTO content_tag(content_id,tag_id,sort,create_time) VALUES(?,?,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrTagIds); i++ ) {
			xvalue objTagId = xvoArrayGetValue(arrTagIds, i);
			int64 iTagId = xvoGetInt(objTagId);
			if ( iTagId <= 0 ) continue;
			sqlite3_reset(stmt);
			sqlite3_clear_bindings(stmt);
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
			sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iTagId);
			sqlite3_bind_int(stmt, 3, (int)i);
			sqlite3_bind_int64(stmt, 4, iNow);
			sqlite3_step(stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_UpdateTagCounters(pDb);
	Managed_AuditLogWithRequest(pDb, "tag", iContentId, "tag.bind", "bind content tags", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	if ( bOwnTagIds ) xvoUnref(arrTagIds);
	xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "bound"));
}

void Managed_AppendTopicRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();
	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetText(tblRow, "title", 5, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
	xvoTableSetText(tblRow, "slug", 4, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetText(tblRow, "summary", 7, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetText(tblRow, "cover", 5, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int(stmt, 5));
	xvoTableSetInt(tblRow, "status", 6, sqlite3_column_int(stmt, 6));
	xvoTableSetInt(tblRow, "contentCount", 12, sqlite3_column_int(stmt, 7));
	xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 8));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_UpdateTopicCounters(sqlite3* pDb)
{
	sqlite3_stmt* stmt = NULL;
	if ( pDb == NULL ) return;
	if ( sqlite3_prepare_v2(pDb, "UPDATE topic SET content_count=(SELECT COUNT(*) FROM topic_content WHERE topic_content.topic_id=topic.id) WHERE delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
}

void Managed_AppendTopicContentRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();
	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblRow, "topicId", 7, sqlite3_column_int64(stmt, 1));
	xvoTableSetText(tblRow, "topicTitle", 10, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetInt(tblRow, "contentId", 9, sqlite3_column_int64(stmt, 3));
	xvoTableSetText(tblRow, "contentTitle", 12, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int(stmt, 5));
	xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 6));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_RequestTopicListPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	xvalue arrList = xvoCreateArray();

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	if ( !Managed_AbilityPackMounted("content.topic") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "topic ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	Managed_UpdateTopicCounters(pDb);
	if ( sqlite3_prepare_v2(pDb, "SELECT id,title,slug,summary,cover,sort,status,content_count,create_time FROM topic WHERE status=1 AND delete_time=0 ORDER BY sort ASC,id ASC", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendTopicRow(arrList, stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestTopicDetailPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sId[32];
	char sSlug[160];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblRow = NULL;
	xvalue tblRet = NULL;
	int64 iId = 0;

	(void)objServer;
	(void)objHost;

	if ( !Managed_AbilityPackMounted("content.topic") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "topic ability pack is not enabled");
		return;
	}
	memset(sId, 0, sizeof(sId));
	memset(sSlug, 0, sizeof(sSlug));
	Managed_ReadTextQuery(objReq, "id", sId, sizeof(sId));
	Managed_ReadTextQuery(objReq, "slug", sSlug, sizeof(sSlug));
	iId = atoll(sId);
	if ( (iId <= 0) && Managed_IsBlank(sSlug) ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "id or slug is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	Managed_UpdateTopicCounters(pDb);
	if ( sqlite3_prepare_v2(pDb, (iId > 0) ? "SELECT id,title,slug,summary,cover,sort,status,content_count,create_time FROM topic WHERE id=? AND status=1 AND delete_time=0 LIMIT 1" : "SELECT id,title,slug,summary,cover,sort,status,content_count,create_time FROM topic WHERE slug=? AND status=1 AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( iId > 0 ) sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		else sqlite3_bind_text(stmt, 1, sSlug, -1, SQLITE_TRANSIENT);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendTopicRow(arrList, stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	if ( xvoArrayItemCount(arrList) > 0 ) {
		tblRow = xvoCopy(xvoArrayGetValue(arrList, 0));
	}
	xvoUnref(arrList);
	if ( tblRow == NULL ) {
		Managed_SendError(objResp, "topic not found");
		return;
	}
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, tblRow, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestTopicContentListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sTopicId[32] = {0};
	char sContentId[32] = {0};
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblRet = NULL;
	int64 iTopicId = 0;
	int64 iContentId = 0;
	const char* sSql = NULL;

	(void)objServer; (void)objHost;
	Managed_ReadTextQuery(objReq, "topicId", sTopicId, sizeof(sTopicId));
	Managed_ReadTextQuery(objReq, "contentId", sContentId, sizeof(sContentId));
	iTopicId = atoll(sTopicId);
	iContentId = atoll(sContentId);
	if ( !Managed_AbilityPackMounted("content.topic") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "topic ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( iContentId > 0 ) {
		sSql = "SELECT tc.id,tc.topic_id,t.title,tc.content_id,c.title,tc.sort,tc.create_time FROM topic_content tc LEFT JOIN topic t ON t.id=tc.topic_id LEFT JOIN content_item c ON c.id=tc.content_id WHERE tc.content_id=? ORDER BY tc.sort ASC,tc.create_time DESC,tc.id DESC";
	} else if ( iTopicId > 0 ) {
		sSql = "SELECT tc.id,tc.topic_id,t.title,tc.content_id,c.title,tc.sort,tc.create_time FROM topic_content tc LEFT JOIN topic t ON t.id=tc.topic_id LEFT JOIN content_item c ON c.id=tc.content_id WHERE tc.topic_id=? ORDER BY tc.sort ASC,tc.create_time DESC,tc.id DESC";
	} else {
		sSql = "SELECT tc.id,tc.topic_id,t.title,tc.content_id,c.title,tc.sort,tc.create_time FROM topic_content tc LEFT JOIN topic t ON t.id=tc.topic_id LEFT JOIN content_item c ON c.id=tc.content_id ORDER BY tc.sort ASC,tc.create_time DESC,tc.id DESC";
	}
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
		if ( iContentId > 0 ) sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		else if ( iTopicId > 0 ) sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTopicId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendTopicContentRow(arrList, stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestTopicUnbindAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	(void)objServer; (void)objHost; (void)objReq;
	if ( !Managed_AbilityPackMounted("content.topic") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "topic ability pack is not enabled");
		return;
	}
	if ( iId <= 0 ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "missing id");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( tblBody ) xvoUnref(tblBody);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM topic_content WHERE id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	Managed_UpdateTopicCounters(pDb);
	Managed_AuditLogWithRequest(pDb, "topic", iId, "topic.unbind", "unbind topic content", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	if ( tblBody ) xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "unbound"));
}

void Managed_RequestTopicContentsPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sTopicId[32];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblSpec = Managed_LoadSpec();
	int64 iTopicId = 0;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !Managed_AbilityPackMounted("content.topic") ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(arrList);
		Managed_SendError(objResp, "topic ability pack is not enabled");
		return;
	}
	Managed_ReadTextQuery(objReq, "topicId", sTopicId, sizeof(sTopicId));
	iTopicId = atoll(sTopicId);
	if ( iTopicId <= 0 ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(arrList);
		Managed_SendError(objResp, "missing topicId");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(arrList);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT c.id,c.title,c.status,c.payload_json,c.is_draft,c.create_time,c.update_time,c.category_id FROM content_item c INNER JOIN topic_content tc ON tc.content_id=c.id WHERE tc.topic_id=? AND c.delete_time=0 AND c.is_draft=0 AND c.status>=1 ORDER BY tc.sort ASC,c.update_time DESC,c.id DESC", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTopicId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendRow(arrList, stmt, tblSpec);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	if ( tblSpec ) xvoUnref(tblSpec);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestTopicSaveAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	const char* sTitle = NULL;
	const char* sSlug = NULL;
	const char* sSummary = NULL;
	const char* sCover = NULL;
	int64 iId = 0;
	int iSort = 0;
	int iStatus = 1;
	int64 iNow = xrtNow();
	bool bInsert = FALSE;

	(void)objServer;
	(void)objHost;

	if ( !Managed_AbilityPackMounted("content.topic") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "topic ability pack is not enabled");
		return;
	}
	if ( tblBody == NULL ) {
		Managed_SendError(objResp, "invalid request body");
		return;
	}
	iId = xvoTableGetInt(tblBody, "id", 2);
	bInsert = iId <= 0;
	sTitle = xvoTableGetText(tblBody, "title", 5);
	sSlug = xvoTableGetText(tblBody, "slug", 4);
	sSummary = xvoTableGetText(tblBody, "summary", 7);
	sCover = xvoTableGetText(tblBody, "cover", 5);
	iSort = (int)xvoTableGetInt(tblBody, "sort", 4);
	iStatus = (int)xvoTableGetInt(tblBody, "status", 6);
	if ( Managed_IsBlank(sTitle) ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "title is required");
		return;
	}
	if ( Managed_IsBlank(sSlug) ) sSlug = sTitle;
	if ( iStatus <= 0 ) iStatus = 1;
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( iId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE topic SET title=?,slug=?,summary=?,cover=?,sort=?,status=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sTitle, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, sSlug, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sSummary ? sSummary : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, sCover ? sCover : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 5, iSort);
			sqlite3_bind_int(stmt, 6, iStatus);
			sqlite3_bind_int64(stmt, 7, iNow);
			sqlite3_bind_int64(stmt, 8, (sqlite3_int64)iId);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	} else {
		if ( sqlite3_prepare_v2(pDb, "INSERT INTO topic(title,slug,summary,cover,sort,status,content_count,create_time,update_time,delete_time) VALUES(?,?,?,?,?,?,0,?,?,0)", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sTitle, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, sSlug, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sSummary ? sSummary : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, sCover ? sCover : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 5, iSort);
			sqlite3_bind_int(stmt, 6, iStatus);
			sqlite3_bind_int64(stmt, 7, iNow);
			sqlite3_bind_int64(stmt, 8, iNow);
			sqlite3_step(stmt);
			iId = sqlite3_last_insert_rowid(pDb);
			sqlite3_finalize(stmt);
		}
	}
	Managed_AuditLogWithRequest(pDb, "topic", iId, bInsert ? "topic.create" : "topic.update", sTitle ? sTitle : "", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	xvoUnref(tblBody);
	tblRet = Managed_CreateResult(TRUE, "saved");
	xvoTableSetInt(tblRet, "id", 2, iId);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestTopicDeleteAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = 0;
	int64 iNow = xrtNow();

	(void)objServer;
	(void)objHost;

	if ( !Managed_AbilityPackMounted("content.topic") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "topic ability pack is not enabled");
		return;
	}
	if ( tblBody == NULL ) {
		Managed_SendError(objResp, "invalid request body");
		return;
	}
	iId = xvoTableGetInt(tblBody, "id", 2);
	if ( iId <= 0 ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "missing id");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE topic SET delete_time=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, iNow);
		sqlite3_bind_int64(stmt, 2, iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM topic_content WHERE topic_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	Managed_AuditLogWithRequest(pDb, "topic", iId, "topic.delete", "delete topic", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "deleted"));
}

void Managed_RequestTopicBindAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	xvalue arrContentIds = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iTopicId = 0;
	int64 iNow = xrtNow();
	str sMode = NULL;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !Managed_AbilityPackMounted("content.topic") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "topic ability pack is not enabled");
		return;
	}
	if ( tblBody == NULL ) {
		Managed_SendError(objResp, "invalid request body");
		return;
	}
	iTopicId = xvoTableGetInt(tblBody, "topicId", 7);
	arrContentIds = xvoTableGetValue(tblBody, "contentIds", 10);
	if ( (iTopicId <= 0) || (arrContentIds == NULL) || (xvoType(arrContentIds) != XVO_DT_ARRAY) ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "topicId and contentIds are required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	sMode = Managed_AbilityPackConfigTextDup("content.topic", "mode", "single");
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM topic_content WHERE topic_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTopicId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	if ( sMode && strcmp((const char*)sMode, "single") == 0 ) {
		if ( sqlite3_prepare_v2(pDb, "DELETE FROM topic_content WHERE content_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
			for ( uint32 i = 0; i < xvoArrayItemCount(arrContentIds); i++ ) {
				xvalue objContentId = xvoArrayGetValue(arrContentIds, i);
				int64 iContentId = xvoGetInt(objContentId);
				if ( iContentId <= 0 ) continue;
				sqlite3_reset(stmt);
				sqlite3_clear_bindings(stmt);
				sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
				sqlite3_step(stmt);
			}
			sqlite3_finalize(stmt);
		}
	}
	if ( sqlite3_prepare_v2(pDb, "INSERT OR IGNORE INTO topic_content(topic_id,content_id,sort,create_time) VALUES(?,?,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrContentIds); i++ ) {
			xvalue objContentId = xvoArrayGetValue(arrContentIds, i);
			int64 iContentId = xvoGetInt(objContentId);
			if ( iContentId <= 0 ) continue;
			sqlite3_reset(stmt);
			sqlite3_clear_bindings(stmt);
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTopicId);
			sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iContentId);
			sqlite3_bind_int(stmt, 3, (int)i);
			sqlite3_bind_int64(stmt, 4, iNow);
			sqlite3_step(stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_UpdateTopicCounters(pDb);
	Managed_AuditLogWithRequest(pDb, "topic", iTopicId, "topic.bind", "bind topic contents", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	if ( sMode ) xrtFree(sMode);
	xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "bound"));
}

void Managed_RequestTopicBindContentAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	xvalue arrTopicIds = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iContentId = 0;
	int64 iNow = xrtNow();

	(void)objServer;
	(void)objHost;

	if ( !Managed_AbilityPackMounted("content.topic") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "topic ability pack is not enabled");
		return;
	}
	if ( tblBody == NULL ) {
		Managed_SendError(objResp, "invalid request body");
		return;
	}
	iContentId = xvoTableGetInt(tblBody, "contentId", 9);
	arrTopicIds = xvoTableGetValue(tblBody, "topicIds", 8);
	if ( iContentId <= 0 ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "contentId is required");
		return;
	}
	if ( (arrTopicIds != NULL) && (xvoType(arrTopicIds) != XVO_DT_ARRAY) ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "topicIds must be an array");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM topic_content WHERE content_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	if ( (arrTopicIds != NULL) && (sqlite3_prepare_v2(pDb, "INSERT OR IGNORE INTO topic_content(topic_id,content_id,sort,create_time) VALUES(?,?,?,?)", -1, &stmt, NULL) == SQLITE_OK) ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrTopicIds); i++ ) {
			xvalue objTopicId = xvoArrayGetValue(arrTopicIds, i);
			int64 iTopicId = xvoGetInt(objTopicId);
			if ( iTopicId <= 0 ) continue;
			sqlite3_reset(stmt);
			sqlite3_clear_bindings(stmt);
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTopicId);
			sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iContentId);
			sqlite3_bind_int(stmt, 3, (int)i);
			sqlite3_bind_int64(stmt, 4, iNow);
			sqlite3_step(stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_UpdateTopicCounters(pDb);
	Managed_AuditLogWithRequest(pDb, "topic", iContentId, "topic.bindContent", "bind content topics", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "bound"));
}

void Managed_AppendSensitiveWordRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();
	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetText(tblRow, "word", 4, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
	xvoTableSetInt(tblRow, "level", 5, sqlite3_column_int(stmt, 2));
	xvoTableSetText(tblRow, "scope", 5, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetText(tblRow, "replacement", 11, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetInt(tblRow, "status", 6, sqlite3_column_int(stmt, 5));
	xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 6));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

bool Managed_TextContainsWord(const char* sText, const char* sWord)
{
	if ( Managed_IsBlank(sText) || Managed_IsBlank(sWord) ) {
		return FALSE;
	}
	return strstr(sText, sWord) != NULL;
}

bool Managed_FieldListContainsName(xvalue arrFields, const char* sName)
{
	if ( Managed_IsBlank(sName) || (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) ) {
		return FALSE;
	}
	for ( uint32 i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
		xvalue tblField = xvoArrayGetValue(arrFields, i);
		const char* sField = ((tblField != NULL) && (xvoType(tblField) == XVO_DT_TABLE)) ? xvoTableGetText(tblField, "name", 4) : NULL;
		if ( !Managed_IsBlank(sField) && (strcmp(sField, sName) == 0) ) {
			return TRUE;
		}
	}
	return FALSE;
}

void Managed_AppendStaticRuleRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();
	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
	xvoTableSetText(tblRow, "pathPattern", 11, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetText(tblRow, "templateName", 12, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetInt(tblRow, "status", 6, sqlite3_column_int(stmt, 4));
	xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 5));
	xvoTableSetInt(tblRow, "updateTime", 10, sqlite3_column_int64(stmt, 6));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_AppendStaticTaskRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();
	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblRow, "ruleId", 6, sqlite3_column_int64(stmt, 1));
	xvoTableSetText(tblRow, "targetType", 10, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetInt(tblRow, "targetId", 8, sqlite3_column_int64(stmt, 3));
	xvoTableSetInt(tblRow, "status", 6, sqlite3_column_int(stmt, 4));
	xvoTableSetText(tblRow, "message", 7, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
	xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 6));
	xvoTableSetInt(tblRow, "finishTime", 10, sqlite3_column_int64(stmt, 7));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_AppendStaticArtifactRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();
	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblRow, "ruleId", 6, sqlite3_column_int64(stmt, 1));
	xvoTableSetText(tblRow, "targetType", 10, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetInt(tblRow, "targetId", 8, sqlite3_column_int64(stmt, 3));
	xvoTableSetText(tblRow, "path", 4, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetText(tblRow, "hash", 4, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
	xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 6));
	xvoTableSetInt(tblRow, "updateTime", 10, sqlite3_column_int64(stmt, 7));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

str Managed_EscapeHtmlText(const char* sText)
{
	xbuffer_struct tBuf = {0};
	char chZero = 0;

	xrtBufferInit(&tBuf, 0);
	if ( sText ) {
		for ( const unsigned char* p = (const unsigned char*)sText; *p; p++ ) {
			const char* sEsc = NULL;
			if ( *p == '&' ) sEsc = "&amp;";
			else if ( *p == '<' ) sEsc = "&lt;";
			else if ( *p == '>' ) sEsc = "&gt;";
			else if ( *p == '"' ) sEsc = "&quot;";
			else if ( *p == '\'' ) sEsc = "&#39;";
			if ( sEsc ) {
				xrtBufferAppend(&tBuf, (ptr)sEsc, (uint32)strlen(sEsc), XBUF_BINARY);
			} else {
				xrtBufferAppend(&tBuf, (ptr)p, 1, XBUF_BINARY);
			}
		}
	}
	xrtBufferAppend(&tBuf, &chZero, 1, XBUF_BINARY);
	return (str)tBuf.Buffer;
}

str Managed_StaticNormalizeRelPath(const char* sPath)
{
	const char* sRel = sPath;
	str sRet;

	if ( Managed_IsBlank(sPath) ) {
		return NULL;
	}
	if ( strncmp(sRel, MANAGED_STATIC_URL_PREFIX, strlen(MANAGED_STATIC_URL_PREFIX)) == 0 ) {
		sRel += strlen(MANAGED_STATIC_URL_PREFIX);
	}
	while ( (*sRel == '/') || (*sRel == '\\') ) {
		sRel++;
	}
	if ( strncmp(sRel, "static/", 7) == 0 ) {
		sRel += 7;
	}
	if ( Managed_IsBlank(sRel) || strstr(sRel, "..") || strchr(sRel, ':') || strchr(sRel, '\\') || strchr(sRel, '%') ) {
		return NULL;
	}
	sRet = xrtCopyStr((str)sRel, 0);
	return sRet;
}

str Managed_StaticNormalizeOutputDir(str sOutputDir)
{
	const char* sDir = (const char*)sOutputDir;
	str sRet;

	if ( Managed_IsBlank(sDir) || strstr(sDir, "..") || strchr(sDir, ':') || strchr(sDir, '%') ) {
		sDir = "content";
	}
	while ( (*sDir == '/') || (*sDir == '\\') ) {
		sDir++;
	}
	if ( strncmp(sDir, "static/", 7) == 0 ) {
		sDir += 7;
	}
	if ( Managed_IsBlank(sDir) ) {
		sDir = "content";
	}
	sRet = xrtCopyStr((str)sDir, 0);
	return sRet;
}

bool Managed_StaticPathCharSafe(char ch)
{
	return ((ch >= 'a') && (ch <= 'z'))
		|| ((ch >= 'A') && (ch <= 'Z'))
		|| ((ch >= '0') && (ch <= '9'))
		|| (ch == '-') || (ch == '_') || (ch == '.');
}

str Managed_StaticSanitizeSegment(const char* sText)
{
	xbuffer_struct tBuf = {0};
	char chZero = 0;
	bool bLastDash = FALSE;

	xrtBufferInit(&tBuf, 0);
	if ( sText ) {
		for ( const unsigned char* p = (const unsigned char*)sText; *p; p++ ) {
			char ch = (char)*p;
			if ( Managed_StaticPathCharSafe(ch) ) {
				xrtBufferAppend(&tBuf, &ch, 1, XBUF_BINARY);
				bLastDash = FALSE;
			} else if ( !bLastDash ) {
				ch = '-';
				xrtBufferAppend(&tBuf, &ch, 1, XBUF_BINARY);
				bLastDash = TRUE;
			}
		}
	}
	if ( tBuf.Length == 0 ) {
		xrtBufferAppend(&tBuf, "item", 4, XBUF_BINARY);
	}
	xrtBufferAppend(&tBuf, &chZero, 1, XBUF_BINARY);
	return (str)tBuf.Buffer;
}

str Managed_StaticFieldTextDup(xvalue tblItem, const char* sField)
{
	xvalue tblData;
	xvalue objValue;

	if ( Managed_IsBlank(sField) || (tblItem == NULL) || (xvoType(tblItem) != XVO_DT_TABLE) ) {
		return xrtCopyStr("", 0);
	}
	objValue = xvoTableGetValue(tblItem, sField, (int)strlen(sField));
	if ( objValue == NULL ) {
		tblData = xvoTableGetValue(tblItem, "data", 4);
		objValue = (tblData && (xvoType(tblData) == XVO_DT_TABLE)) ? xvoTableGetValue(tblData, sField, (int)strlen(sField)) : NULL;
	}
	return Managed_ValueToTextDup(objValue);
}

str Managed_StaticReplaceToken(str sInput, const char* sToken, const char* sValue)
{
	str sSafe = Managed_StaticSanitizeSegment(sValue ? sValue : "");
	str sNext = xrtReplace(sInput, 0, (str)sToken, 0, sSafe ? sSafe : (str)"", 0, NULL);
	if ( sSafe ) xrtFree(sSafe);
	if ( sInput ) xrtFree(sInput);
	return sNext;
}

str Managed_StaticApplyPathPattern(const char* sPattern, int64 iTargetId, xvalue tblItem)
{
	str sPath = xrtCopyStr((str)(Managed_IsBlank(sPattern) ? "/content/{id}.html" : sPattern), 0);
	char sId[32];
	str sTitle = NULL;
	str sSlug = NULL;
	str sCategoryId = NULL;

	snprintf(sId, sizeof(sId), "%lld", (long long)iTargetId);
	sTitle = Managed_StaticFieldTextDup(tblItem, "title");
	sSlug = Managed_StaticFieldTextDup(tblItem, "slug");
	sCategoryId = Managed_StaticFieldTextDup(tblItem, "categoryId");
	sPath = Managed_StaticReplaceToken(sPath, "{id}", sId);
	sPath = Managed_StaticReplaceToken(sPath, "{title}", sTitle ? (const char*)sTitle : "");
	sPath = Managed_StaticReplaceToken(sPath, "{slug}", !Managed_IsBlank((const char*)sSlug) ? (const char*)sSlug : sId);
	sPath = Managed_StaticReplaceToken(sPath, "{categoryId}", sCategoryId ? (const char*)sCategoryId : "0");
	if ( sTitle ) xrtFree(sTitle);
	if ( sSlug ) xrtFree(sSlug);
	if ( sCategoryId ) xrtFree(sCategoryId);
	return sPath;
}

str Managed_StaticBuildRelPath(int64 iTargetId, const char* sPath, const char* sPathPattern, xvalue tblItem)
{
	str sExplicit = Managed_StaticNormalizeRelPath(sPath);
	str sOutputDir = NULL;
	str sDir = NULL;
	str sRaw = NULL;
	str sRel = NULL;

	if ( sExplicit ) {
		return sExplicit;
	}
	if ( !Managed_IsBlank(sPathPattern) ) {
		sRaw = Managed_StaticApplyPathPattern(sPathPattern, iTargetId, tblItem);
		sRel = Managed_StaticNormalizeRelPath(sRaw);
		if ( sRaw ) xrtFree(sRaw);
		if ( sRel ) return sRel;
	}
	sOutputDir = Managed_AbilityPackConfigTextDup("content.static", "outputDir", "content");
	sDir = Managed_StaticNormalizeOutputDir(sOutputDir);
	if ( iTargetId > 0 ) {
		sRel = xrtFormat("%s/%lld.html", sDir ? (const char*)sDir : "content", (long long)iTargetId);
	} else {
		sRel = xrtFormat("%s/index.html", sDir ? (const char*)sDir : "content");
	}
	if ( sOutputDir ) xrtFree(sOutputDir);
	if ( sDir ) xrtFree(sDir);
	return sRel;
}

str Managed_StaticBuildArtifactUrl(const char* sRelPath)
{
	if ( Managed_IsBlank(sRelPath) ) {
		return NULL;
	}
	return xrtFormat("%s%s", MANAGED_STATIC_URL_PREFIX, sRelPath);
}

str Managed_StaticNormalizeTemplateName(const char* sTemplateName)
{
	const char* sName = Managed_IsBlank(sTemplateName) ? "detail" : sTemplateName;

	if ( strstr(sName, "..") || strchr(sName, ':') || strchr(sName, '\\') || strchr(sName, '%') ) {
		sName = "detail";
	}
	if ( strchr(sName, '/') == NULL ) {
		if ( strstr(sName, ".html") == NULL ) {
			return xrtFormat("static/%s.html", sName);
		}
		return xrtFormat("static/%s", sName);
	}
	if ( strstr(sName, ".html") == NULL ) {
		return xrtFormat("%s.html", sName);
	}
	return xrtCopyStr((str)sName, 0);
}

void Managed_StaticPrepareFieldHtml(xvalue tblRender, xvalue tblData, xvalue tblField)
{
	const char* sName;
	const char* sType;
	xvalue objValue;
	str sRaw = NULL;
	str sHtml = NULL;
	str sHtmlName = NULL;

	if ( (tblRender == NULL) || (tblData == NULL) || (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
		return;
	}
	sName = xvoTableGetText(tblField, "name", 4);
	if ( Managed_IsBlank(sName) ) {
		return;
	}
	objValue = xvoTableGetValue(tblData, sName, (int)strlen(sName));
	if ( objValue ) {
		xvoTableSetValue(tblRender, sName, (int)strlen(sName), xvoCopy(objValue), TRUE);
	}
	sRaw = Managed_ValueToTextDup(objValue);
	sType = Managed_MapFieldType(tblField);
	if ( strcmp(sType, "editor_md") == 0 ) {
		sHtml = xsMarkdownToHtmlEx(sRaw ? (const char*)sRaw : "", MD_DIALECT_GITHUB | MD_FLAG_NOHTML, MD_HTML_FLAG_SKIP_UTF8_BOM);
		if ( sHtml == NULL ) {
			sHtml = Managed_EscapeHtmlText(sRaw ? (const char*)sRaw : "");
		}
	} else if ( strcmp(sType, "editor_html") == 0 ) {
		sHtml = xrtCopyStr(sRaw ? sRaw : (str)"", 0);
	} else {
		sHtml = Managed_EscapeHtmlText(sRaw ? (const char*)sRaw : "");
	}
	sHtmlName = xrtFormat("%s_html", sName);
	if ( sHtmlName ) {
		xvoTableSetText(tblRender, sHtmlName, (int)strlen(sHtmlName), sHtml ? sHtml : (str)"", 0, TRUE);
	}
	if ( sHtmlName ) xrtFree(sHtmlName);
	if ( sRaw ) xrtFree(sRaw);
}

xvalue Managed_StaticPrepareRenderData(xvalue tblItem, xvalue tblSpec)
{
	xvalue tblRender = xvoCreateTable();
	xvalue tblData = tblItem ? xvoTableGetValue(tblItem, "data", 4) : NULL;
	xvalue arrFields = Managed_GetFields(tblSpec);

	if ( tblRender == NULL ) {
		return NULL;
	}
	if ( tblItem ) {
		xvalue obj;
		const char* sKeys[] = { "id", "title", "status", "slug", "summary", "cover", "categoryId", "createTime", "updateTime", "createTimeText", "updateTimeText", "publishedAt", "publishedAtText" };
		for ( uint32 i = 0; i < sizeof(sKeys) / sizeof(sKeys[0]); i++ ) {
			obj = xvoTableGetValue(tblItem, sKeys[i], (int)strlen(sKeys[i]));
			if ( obj ) {
				xvoTableSetValue(tblRender, sKeys[i], (int)strlen(sKeys[i]), xvoCopy(obj), TRUE);
			}
		}
	}
	if ( tblData && xvoType(tblData) == XVO_DT_TABLE ) {
		xvoTableSetValue(tblRender, "data", 4, xvoCopy(tblData), TRUE);
	}
	xvoTableSetValue(tblRender, "item", 4, tblItem ? xvoCopy(tblItem) : xvoCreateTable(), TRUE);
	xvoTableSetValue(tblRender, "spec", 4, tblSpec ? xvoCopy(tblSpec) : xvoCreateTable(), TRUE);
	if ( arrFields && xvoType(arrFields) == XVO_DT_ARRAY && tblData && xvoType(tblData) == XVO_DT_TABLE ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
			Managed_StaticPrepareFieldHtml(tblRender, tblData, xvoArrayGetValue(arrFields, i));
		}
	}
	return tblRender;
}

bool Managed_StaticLoadRule(sqlite3* pDb, int64 iRuleId, str* psPathPattern, str* psTemplateName)
{
	sqlite3_stmt* stmt = NULL;
	bool bFound = FALSE;

	if ( psPathPattern ) *psPathPattern = NULL;
	if ( psTemplateName ) *psTemplateName = NULL;
	if ( (pDb == NULL) || (iRuleId <= 0) ) {
		return FALSE;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT path_pattern,template_name FROM static_rule WHERE id=? AND status=1 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iRuleId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			if ( psPathPattern ) *psPathPattern = xrtCopyStr((str)sqlite3_column_text(stmt, 0), 0);
			if ( psTemplateName ) *psTemplateName = xrtCopyStr((str)sqlite3_column_text(stmt, 1), 0);
			bFound = TRUE;
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return bFound;
}

xvalue Managed_StaticLoadContentItem(sqlite3* pDb, int64 iTargetId, xvalue tblSpec)
{
	sqlite3_stmt* stmt = NULL;
	xvalue tblItem = NULL;

	if ( (pDb == NULL) || (iTargetId <= 0) ) {
		return NULL;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id, title, status, payload_json, is_draft, create_time, update_time, category_id FROM content_item WHERE id=? AND delete_time=0 AND is_draft=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTargetId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			tblItem = Managed_CreateItemFromStmt(stmt, tblSpec);
			Managed_AttachAbilityListFields(pDb, tblItem,
				Managed_AbilityPackMounted("content.tag"),
				Managed_AbilityPackMounted("content.topic"),
				Managed_AbilityPackMounted("content.comment"),
				Managed_AbilityPackMounted("content.like"),
				Managed_AbilityPackMounted("content.view-stat"),
				Managed_AbilityPackMounted("content.media"));
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return tblItem;
}

bool Managed_StaticWriteFile(const char* sRelPath, const char* sHtml, size_t iHtmlSize)
{
	str sFilePath;
	str sDirPath;
	bool bOK;

	if ( Managed_IsBlank(sRelPath) || (sHtml == NULL) || (G_Handle == NULL) ) {
		return FALSE;
	}
	sFilePath = XAdmin_PluginResourcePath(G_Handle, "static", sRelPath);
	if ( sFilePath == NULL ) {
		return FALSE;
	}
	sDirPath = xrtPathGetDir(sFilePath, 0);
	if ( sDirPath ) {
		xrtDirCreateAll(sDirPath);
		xrtFree(sDirPath);
	}
	bOK = xrtFilePutAll(sFilePath, (ptr)sHtml, (uint32)iHtmlSize) >= 0;
	xrtFree(sFilePath);
	return bOK;
}

str Managed_StaticReplaceRenderValue(str sHtml, xvalue tblRender, const char* sToken, const char* sKey)
{
	xvalue objValue;
	str sValue = NULL;
	str sNext = NULL;

	if ( sHtml == NULL || tblRender == NULL || Managed_IsBlank(sToken) || Managed_IsBlank(sKey) ) {
		return sHtml;
	}
	objValue = xvoTableGetValue(tblRender, sKey, (int)strlen(sKey));
	sValue = Managed_ValueToTextDup(objValue);
	sNext = xrtReplace(sHtml, 0, (str)sToken, 0, sValue ? sValue : (str)"", 0, NULL);
	if ( sValue ) xrtFree(sValue);
	if ( sNext ) {
		xrtFree(sHtml);
		return sNext;
	}
	return sHtml;
}

str Managed_StaticApplyRenderTokens(str sHtml, xvalue tblRender, xvalue tblSpec)
{
	xvalue arrFields = Managed_GetFields(tblSpec);

	sHtml = Managed_StaticReplaceRenderValue(sHtml, tblRender, "@@title@@", "title");
	sHtml = Managed_StaticReplaceRenderValue(sHtml, tblRender, "@@summary@@", "summary");
	sHtml = Managed_StaticReplaceRenderValue(sHtml, tblRender, "@@summary_html@@", "summary_html");
	sHtml = Managed_StaticReplaceRenderValue(sHtml, tblRender, "@@content@@", "content");
	sHtml = Managed_StaticReplaceRenderValue(sHtml, tblRender, "@@content_html@@", "content_html");
	sHtml = Managed_StaticReplaceRenderValue(sHtml, tblRender, "@@updateTimeText@@", "updateTimeText");
	sHtml = Managed_StaticReplaceRenderValue(sHtml, tblRender, "@@createTimeText@@", "createTimeText");

	if ( arrFields && xvoType(arrFields) == XVO_DT_ARRAY ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
			xvalue tblField = xvoArrayGetValue(arrFields, i);
			const char* sName = tblField ? xvoTableGetText(tblField, "name", 4) : NULL;
			str sToken = NULL;
			str sHtmlKey = NULL;
			str sHtmlToken = NULL;
			if ( Managed_IsBlank(sName) ) {
				continue;
			}
			sToken = xrtFormat("@@%s@@", sName);
			sHtmlKey = xrtFormat("%s_html", sName);
			sHtmlToken = xrtFormat("@@%s_html@@", sName);
			if ( sToken ) {
				sHtml = Managed_StaticReplaceRenderValue(sHtml, tblRender, (const char*)sToken, sName);
				xrtFree(sToken);
			}
			if ( sHtmlKey && sHtmlToken ) {
				sHtml = Managed_StaticReplaceRenderValue(sHtml, tblRender, (const char*)sHtmlToken, (const char*)sHtmlKey);
			}
			if ( sHtmlKey ) xrtFree(sHtmlKey);
			if ( sHtmlToken ) xrtFree(sHtmlToken);
		}
	}
	return sHtml;
}

bool Managed_StaticRenderToFile(sqlite3* pDb, int64 iTargetId, int64 iRuleId, const char* sPath, const char* sPathPattern, const char* sTemplateName, str* psRelPath, str* psArtifactUrl, str* psHash, str* psError)
{
	xvalue tblSpec = NULL;
	xvalue tblItem = NULL;
	xvalue tblRender = NULL;
	str sTemplate = NULL;
	char* sHtml = NULL;
	char* sError = NULL;
	size_t iHtmlSize = 0;
	str sRelPath = NULL;
	str sArtifactUrl = NULL;
	bool bOK = FALSE;
	(void)iRuleId;

	if ( psRelPath ) *psRelPath = NULL;
	if ( psArtifactUrl ) *psArtifactUrl = NULL;
	if ( psHash ) *psHash = NULL;
	if ( psError ) *psError = NULL;
	if ( (pDb == NULL) || (iTargetId <= 0) ) {
		if ( psError ) *psError = xrtCopyStr("target content is required", 0);
		return FALSE;
	}
	tblSpec = Managed_LoadSpec();
	if ( tblSpec == NULL ) {
		if ( psError ) *psError = xrtCopyStr("spec.json is missing", 0);
		return FALSE;
	}
	tblItem = Managed_StaticLoadContentItem(pDb, iTargetId, tblSpec);
	if ( tblItem == NULL ) {
		if ( psError ) *psError = xrtCopyStr("content item not found or not publishable", 0);
		xvoUnref(tblSpec);
		return FALSE;
	}
	sRelPath = Managed_StaticBuildRelPath(iTargetId, sPath, sPathPattern, tblItem);
	if ( sRelPath == NULL ) {
		if ( psError ) *psError = xrtCopyStr("invalid static output path", 0);
		goto cleanup;
	}
	tblRender = Managed_StaticPrepareRenderData(tblItem, tblSpec);
	sTemplate = Managed_StaticNormalizeTemplateName(sTemplateName);
	sHtml = XAdmin_RenderPluginTemplate(G_Handle, sTemplate ? (const char*)sTemplate : "static/detail.html", tblRender, &iHtmlSize, &sError);
	if ( sHtml == NULL ) {
		if ( psError ) *psError = xrtFormat("template render failed: %s", sError ? sError : "unknown");
		goto cleanup;
	}
	{
		str sApplied = Managed_StaticApplyRenderTokens(xrtCopyStr((str)sHtml, (uint32)iHtmlSize), tblRender, tblSpec);
		XAdmin_Free(sHtml);
		sHtml = sApplied;
		iHtmlSize = sHtml ? strlen(sHtml) : 0;
	}
	if ( !Managed_StaticWriteFile((const char*)sRelPath, sHtml, iHtmlSize) ) {
		if ( psError ) *psError = xrtCopyStr("failed to write static html file", 0);
		goto cleanup;
	}
	sArtifactUrl = Managed_StaticBuildArtifactUrl((const char*)sRelPath);
	if ( psRelPath ) {
		*psRelPath = sRelPath;
		sRelPath = NULL;
	}
	if ( psArtifactUrl ) {
		*psArtifactUrl = sArtifactUrl;
		sArtifactUrl = NULL;
	}
	if ( psHash ) {
		*psHash = xrtFormat("%016llx", (unsigned long long)xrtHash64(sHtml, iHtmlSize));
	}
	bOK = TRUE;

cleanup:
	if ( sRelPath ) xrtFree(sRelPath);
	if ( sArtifactUrl ) xrtFree(sArtifactUrl);
	if ( sTemplate ) xrtFree(sTemplate);
	if ( sHtml ) xrtFree(sHtml);
	if ( sError ) XAdmin_Free(sError);
	if ( tblRender ) xvoUnref(tblRender);
	if ( tblItem ) xvoUnref(tblItem);
	if ( tblSpec ) xvoUnref(tblSpec);
	return bOK;
}

bool Managed_StaticCreateTask(sqlite3* pDb, int64 iRuleId, const char* sTargetType, int64 iTargetId, const char* sPath, int64* piTaskId, str* psArtifactPath)
{
	sqlite3_stmt* stmt = NULL;
	str sPathPattern = NULL;
	str sTemplateName = NULL;
	str sRelPath = NULL;
	str sArtifactPath = NULL;
	str sHash = NULL;
	str sError = NULL;
	int64 iNow = xrtNow();
	bool bOK = FALSE;
	bool bTaskOK = FALSE;

	if ( piTaskId ) *piTaskId = 0;
	if ( psArtifactPath ) *psArtifactPath = NULL;
	if ( pDb == NULL ) {
		return FALSE;
	}
	if ( ((sTargetType == NULL) || (strcmp(sTargetType, "content") == 0)) && (iTargetId > 0) && !Managed_AccessCheckRule(pDb, iTargetId, NULL, NULL, FALSE, NULL) ) {
		return FALSE;
	}
	if ( iRuleId > 0 ) {
		Managed_StaticLoadRule(pDb, iRuleId, &sPathPattern, &sTemplateName);
	}
	bOK = Managed_StaticRenderToFile(pDb, iTargetId, iRuleId, sPath, sPathPattern, sTemplateName, &sRelPath, &sArtifactPath, &sHash, &sError);
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO static_task(rule_id,target_type,target_id,status,message,create_time,finish_time) VALUES(?,?,?,?,?,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iRuleId);
		sqlite3_bind_text(stmt, 2, sTargetType ? sTargetType : "content", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iTargetId);
		sqlite3_bind_int(stmt, 4, bOK ? 1 : -1);
		sqlite3_bind_text(stmt, 5, bOK ? "generated" : (sError ? (const char*)sError : "failed"), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 6, iNow);
		sqlite3_bind_int64(stmt, 7, iNow);
		bTaskOK = (sqlite3_step(stmt) == SQLITE_DONE);
		if ( bTaskOK && piTaskId ) *piTaskId = sqlite3_last_insert_rowid(pDb);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( bOK ) {
		if ( sqlite3_prepare_v2(pDb, "DELETE FROM static_artifact WHERE (rule_id=? AND target_type=? AND target_id=?) OR path=?", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iRuleId);
			sqlite3_bind_text(stmt, 2, sTargetType ? sTargetType : "content", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iTargetId);
			sqlite3_bind_text(stmt, 4, sArtifactPath ? (const char*)sArtifactPath : "", -1, SQLITE_TRANSIENT);
			sqlite3_step(stmt);
		}
		if ( stmt ) sqlite3_finalize(stmt);
		stmt = NULL;
		if ( sqlite3_prepare_v2(pDb, "INSERT OR REPLACE INTO static_artifact(rule_id,target_type,target_id,path,hash,create_time,update_time) VALUES(?,?,?,?,?,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iRuleId);
			sqlite3_bind_text(stmt, 2, sTargetType ? sTargetType : "content", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iTargetId);
			sqlite3_bind_text(stmt, 4, (const char*)sArtifactPath, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 5, sHash ? (const char*)sHash : "managed", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 6, iNow);
			sqlite3_bind_int64(stmt, 7, iNow);
			bOK = (sqlite3_step(stmt) == SQLITE_DONE);
		}
		if ( stmt ) sqlite3_finalize(stmt);
	}
	if ( bOK && psArtifactPath ) {
		*psArtifactPath = sArtifactPath;
		sArtifactPath = NULL;
	}
	if ( sPathPattern ) xrtFree(sPathPattern);
	if ( sTemplateName ) xrtFree(sTemplateName);
	if ( sRelPath ) xrtFree(sRelPath);
	if ( sArtifactPath ) xrtFree(sArtifactPath);
	if ( sHash ) xrtFree(sHash);
	if ( sError ) xrtFree(sError);
	return bOK && bTaskOK;
}

void Managed_RequestStaticGeneratePublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	xvalue tblRet;
	int64 iTaskId = 0;
	int64 iTargetId = tblBody ? xvoTableGetInt(tblBody, "targetId", 8) : 0;
	int64 iRuleId = tblBody ? xvoTableGetInt(tblBody, "ruleId", 6) : 0;
	const char* sPath = tblBody ? xvoTableGetText(tblBody, "path", 4) : NULL;
	str sArtifactPath = NULL;
	(void)objServer; (void)objHost; (void)objReq;
	if ( !Managed_AbilityPackMounted("content.static") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "static ability pack is not enabled");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( (iTargetId > 0) && !Managed_AccessCheckRule(pDb, iTargetId, NULL, NULL, FALSE, NULL) ) {
		Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "restricted content cannot generate public static output");
		return;
	}
	if ( !Managed_StaticCreateTask(pDb, iRuleId, "content", iTargetId, sPath, &iTaskId, &sArtifactPath) ) {
		Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to create static generation task");
		return;
	}
	Managed_AuditLogWithRequest(pDb, "static_task", iTaskId, "static.generate", sPath ? sPath : "", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "generated");
	xvoTableSetInt(tblRet, "taskId", 6, iTaskId);
	xvoTableSetText(tblRet, "path", 4, sArtifactPath, 0, FALSE);
	if ( tblBody ) xvoUnref(tblBody);
	if ( sArtifactPath ) xrtFree(sArtifactPath);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestStaticPreviewPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sTargetId[32] = {0};
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet;
	xvalue tblData = NULL;
	int64 iTargetId;
	(void)objServer; (void)objHost; (void)objSession;
	xsReqQueryValue(objReq, "targetId", sTargetId, sizeof(sTargetId));
	iTargetId = atoll(sTargetId);
	if ( !Managed_AbilityPackMounted("content.static") ) {
		Managed_SendError(objResp, "static ability pack is not enabled");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id,rule_id,target_type,target_id,path,hash,create_time,update_time FROM static_artifact WHERE target_id=? ORDER BY update_time DESC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTargetId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			tblData = xvoCreateTable();
			xvoTableSetInt(tblData, "id", 2, sqlite3_column_int64(stmt, 0));
			xvoTableSetInt(tblData, "ruleId", 6, sqlite3_column_int64(stmt, 1));
			xvoTableSetText(tblData, "targetType", 10, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
			xvoTableSetInt(tblData, "targetId", 8, sqlite3_column_int64(stmt, 3));
			xvoTableSetText(tblData, "path", 4, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
			xvoTableSetText(tblData, "hash", 4, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
			xvoTableSetInt(tblData, "createTime", 10, sqlite3_column_int64(stmt, 6));
			xvoTableSetInt(tblData, "updateTime", 10, sqlite3_column_int64(stmt, 7));
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	if ( tblData ) xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_StaticMaybeAutoGenerate(sqlite3* pDb, int64 iContentId, bool bDraft, int iStatus)
{
	sqlite3_stmt* stmt = NULL;
	str sArtifactPath = NULL;
	bool bGeneratedByRule = FALSE;
	(void)iStatus;
	if ( (pDb == NULL) || (iContentId <= 0) || bDraft ) {
		return;
	}
	if ( !Managed_AbilityPackMounted("content.static") ) {
		return;
	}
	if ( !Managed_AbilityPackConfigBool("content.static", "autoGenerate", TRUE) ) {
		return;
	}
	if ( !Managed_AccessCheckRule(pDb, iContentId, NULL, NULL, FALSE, NULL) ) {
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id FROM static_rule WHERE status=1 ORDER BY id ASC", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			int64 iRuleId = sqlite3_column_int64(stmt, 0);
			Managed_StaticCreateTask(pDb, iRuleId, "content", iContentId, NULL, NULL, &sArtifactPath);
			if ( sArtifactPath ) {
				xrtFree(sArtifactPath);
				sArtifactPath = NULL;
			}
			bGeneratedByRule = TRUE;
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	if ( !bGeneratedByRule ) {
		Managed_StaticCreateTask(pDb, 0, "content", iContentId, NULL, NULL, &sArtifactPath);
	}
	if ( sArtifactPath ) xrtFree(sArtifactPath);
}

void Managed_StaticMaybeAutoClean(sqlite3* pDb, int64 iContentId)
{
	sqlite3_stmt* stmt = NULL;
	if ( (pDb == NULL) || (iContentId <= 0) ) {
		return;
	}
	if ( !Managed_AbilityPackMounted("content.static") ) {
		return;
	}
	if ( !Managed_AbilityPackConfigBool("content.static", "autoClean", TRUE) ) {
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT path FROM static_artifact WHERE target_type='content' AND target_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			str sRelPath = Managed_StaticNormalizeRelPath((const char*)sqlite3_column_text(stmt, 0));
			if ( sRelPath ) {
				str sFilePath = XAdmin_PluginResourcePath(G_Handle, "static", sRelPath);
				if ( sFilePath ) {
					xrtFileDelete(sFilePath);
					xrtFree(sFilePath);
				}
				xrtFree(sRelPath);
			}
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM static_artifact WHERE target_type='content' AND target_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
}

str Managed_StaticRuleBuildRiskWarning(const char* sPathPattern)
{
	if ( Managed_IsBlank(sPathPattern) ) return NULL;
	if ( strcmp(sPathPattern, "/") == 0 || strcmp(sPathPattern, "/*") == 0 ) {
		return xrtCopyStr("static pathPattern covers site root; submit-time warning only, request hot path is unchanged", 0);
	}
	if ( strncmp(sPathPattern, "/admin", 6) == 0 || strncmp(sPathPattern, "/api", 4) == 0 ) {
		return xrtCopyStr("static pathPattern overlaps admin/API prefix; verify permission and route ownership before publishing", 0);
	}
	if ( strncmp(sPathPattern, "/css", 4) == 0 || strncmp(sPathPattern, "/js", 3) == 0 || strncmp(sPathPattern, "/img", 4) == 0 || strncmp(sPathPattern, "/res", 4) == 0 || strncmp(sPathPattern, "/static", 7) == 0 || strncmp(sPathPattern, "/uploads", 8) == 0 || strncmp(sPathPattern, "/plugin-static", 14) == 0 ) {
		return xrtCopyStr("static pathPattern looks like a static resource path; verify rule ownership before enabling pseudo-static output", 0);
	}
	if ( (strncmp(sPathPattern, "/{", 2) == 0) || (strncmp(sPathPattern, "/*", 2) == 0) ) {
		return xrtCopyStr("static pathPattern is very broad; prefer a fixed prefix before dynamic placeholders", 0);
	}
	return NULL;
}

void Managed_RequestStaticRuleSaveAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet;
	str sWarning = NULL;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	const char* sName = tblBody ? xvoTableGetText(tblBody, "name", 4) : NULL;
	const char* sPathPattern = tblBody ? xvoTableGetText(tblBody, "pathPattern", 11) : NULL;
	const char* sTemplateName = tblBody ? xvoTableGetText(tblBody, "templateName", 12) : NULL;
	int iStatus = tblBody ? xvoTableGetInt(tblBody, "status", 6) : 1;
	int64 iNow = xrtNow();
	bool bInsert = iId <= 0;
	(void)objServer; (void)objHost; (void)objReq;
	if ( !Managed_AbilityPackMounted("content.static") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "static ability pack is not enabled");
		return;
	}
	if ( Managed_IsBlank(sName) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "name is required");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( iId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE static_rule SET name=?,path_pattern=?,template_name=?,status=?,update_time=? WHERE id=?", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sName, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, sPathPattern ? sPathPattern : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sTemplateName ? sTemplateName : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 4, iStatus);
			sqlite3_bind_int64(stmt, 5, iNow);
			sqlite3_bind_int64(stmt, 6, (sqlite3_int64)iId);
			sqlite3_step(stmt);
		}
	} else {
		if ( sqlite3_prepare_v2(pDb, "INSERT INTO static_rule(name,path_pattern,template_name,status,create_time,update_time) VALUES(?,?,?,?,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sName, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, sPathPattern ? sPathPattern : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sTemplateName ? sTemplateName : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 4, iStatus);
			sqlite3_bind_int64(stmt, 5, iNow);
			sqlite3_bind_int64(stmt, 6, iNow);
			if ( sqlite3_step(stmt) == SQLITE_DONE ) iId = sqlite3_last_insert_rowid(pDb);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_AuditLogWithRequest(pDb, "static_rule", iId, bInsert ? "static.rule.create" : "static.rule.update", sName ? sName : "", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "saved");
	xvoTableSetInt(tblRet, "id", 2, iId);
	sWarning = Managed_StaticRuleBuildRiskWarning(sPathPattern);
	if ( sWarning ) {
		xvoTableSetText(tblRet, "warning", 7, sWarning, 0, FALSE);
		xrtFree(sWarning);
	}
	xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestStaticRuleDeleteAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	(void)objServer; (void)objHost; (void)objReq;
	if ( !Managed_AbilityPackMounted("content.static") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "static ability pack is not enabled");
		return;
	}
	if ( iId <= 0 ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "missing id");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM static_rule WHERE id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_AuditLogWithRequest(pDb, "static_rule", iId, "static.rule.delete", "delete static rule", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "deleted"));
}

void Managed_RequestStaticRuleListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblRet = NULL;
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.static") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "static ability pack is not enabled");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id,name,path_pattern,template_name,status,create_time,update_time FROM static_rule ORDER BY status DESC,id DESC", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendStaticRuleRow(arrList, stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestStaticTaskListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sLimit[16] = {0};
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblRet = NULL;
	int iLimit = 50;
	(void)objServer; (void)objHost; (void)objSession;
	Managed_ReadTextQuery(objReq, "limit", sLimit, sizeof(sLimit));
	if ( atoi(sLimit) > 0 ) {
		iLimit = atoi(sLimit);
		if ( iLimit > 200 ) iLimit = 200;
	}
	if ( !Managed_AbilityPackMounted("content.static") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "static ability pack is not enabled");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id,rule_id,target_type,target_id,status,message,create_time,finish_time FROM static_task ORDER BY create_time DESC,id DESC LIMIT ?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iLimit);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendStaticTaskRow(arrList, stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestStaticTaskStatusAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sId[32] = {0};
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblRet = NULL;
	int64 iTaskId = 0;
	(void)objServer; (void)objHost; (void)objSession;
	Managed_ReadTextQuery(objReq, "id", sId, sizeof(sId));
	iTaskId = atoll(sId);
	if ( iTaskId <= 0 ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "invalid task id");
		return;
	}
	if ( !Managed_AbilityPackMounted("content.static") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "static ability pack is not enabled");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id,rule_id,target_type,target_id,status,message,create_time,finish_time FROM static_task WHERE id=? LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTaskId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendStaticTaskRow(arrList, stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	if ( xvoArrayItemCount(arrList) <= 0 ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "static task not found");
		return;
	}
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, xvoCopy(xvoArrayGetValue(arrList, 0)), TRUE);
	xvoUnref(arrList);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestStaticArtifactListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sTargetId[32] = {0};
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblRet = NULL;
	int64 iTargetId = 0;
	(void)objServer; (void)objHost; (void)objSession;
	Managed_ReadTextQuery(objReq, "targetId", sTargetId, sizeof(sTargetId));
	iTargetId = atoll(sTargetId);
	if ( !Managed_AbilityPackMounted("content.static") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "static ability pack is not enabled");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, (iTargetId > 0) ? "SELECT id,rule_id,target_type,target_id,path,hash,create_time,update_time FROM static_artifact WHERE target_type='content' AND target_id=? ORDER BY update_time DESC,id DESC" : "SELECT id,rule_id,target_type,target_id,path,hash,create_time,update_time FROM static_artifact ORDER BY update_time DESC,id DESC", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( iTargetId > 0 ) sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTargetId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendStaticArtifactRow(arrList, stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestStaticCleanAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iTargetId = tblBody ? xvoTableGetInt(tblBody, "targetId", 8) : 0;
	int64 iRuleId = tblBody ? xvoTableGetInt(tblBody, "ruleId", 6) : 0;
	(void)objServer; (void)objHost; (void)objReq;
	if ( !Managed_AbilityPackMounted("content.static") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "static ability pack is not enabled");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT path FROM static_artifact WHERE (? <= 0 OR target_id = ?) AND (? <= 0 OR rule_id = ?)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTargetId);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iTargetId);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iRuleId);
		sqlite3_bind_int64(stmt, 4, (sqlite3_int64)iRuleId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			str sRelPath = Managed_StaticNormalizeRelPath((const char*)sqlite3_column_text(stmt, 0));
			if ( sRelPath ) {
				str sFilePath = XAdmin_PluginResourcePath(G_Handle, "static", sRelPath);
				if ( sFilePath ) {
					xrtFileDelete(sFilePath);
					xrtFree(sFilePath);
				}
				xrtFree(sRelPath);
			}
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM static_artifact WHERE (? <= 0 OR target_id = ?) AND (? <= 0 OR rule_id = ?)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTargetId);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iTargetId);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iRuleId);
		sqlite3_bind_int64(stmt, 4, (sqlite3_int64)iRuleId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	Managed_AuditLogWithRequest(pDb, "static_artifact", iTargetId, "static.clean", "clean static artifacts", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	if ( tblBody ) xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "cleaned"));
}

void Managed_UpdateLikeCounter(sqlite3* pDb, int64 iContentId)
{
	sqlite3_stmt* stmt = NULL;
	int iCount = 0;
	int64 iNow = xrtNow();
	if ( (pDb == NULL) || (iContentId <= 0) ) {
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT COUNT(*) FROM like_record WHERE content_id=? AND status=1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) iCount = sqlite3_column_int(stmt, 0);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM like_counter WHERE content_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO like_counter(content_id,like_count,update_time) VALUES(?,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_int(stmt, 2, iCount);
		sqlite3_bind_int64(stmt, 3, iNow);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
}

const char* Managed_LikeActorKey(xvalue tblBody)
{
	str sDedup = Managed_AbilityPackConfigTextDup("content.like", "dedup", "ip");
	const char* sActorKey = tblBody ? xvoTableGetText(tblBody, "actorKey", 8) : NULL;
	const char* sActorId = tblBody ? xvoTableGetText(tblBody, "actorId", 7) : NULL;
	const char* sIp = tblBody ? xvoTableGetText(tblBody, "ip", 2) : NULL;
	const char* sCookie = tblBody ? xvoTableGetText(tblBody, "cookieKey", 9) : NULL;
	const char* sRet = NULL;
	if ( sDedup && strcmp((const char*)sDedup, "member") == 0 ) {
		sRet = !Managed_IsBlank(sActorId) ? sActorId : sActorKey;
	} else if ( sDedup && strcmp((const char*)sDedup, "cookie") == 0 ) {
		sRet = !Managed_IsBlank(sCookie) ? sCookie : sActorKey;
	} else {
		sRet = !Managed_IsBlank(sIp) ? sIp : sActorKey;
	}
	if ( Managed_IsBlank(sRet) && !Managed_IsBlank(sActorId) ) sRet = sActorId;
	if ( Managed_IsBlank(sRet) ) sRet = "anonymous";
	if ( sDedup ) xrtFree(sDedup);
	return sRet;
}

void Managed_LikeActorKeyFromQuery(XS_RequestObject objReq, char* sOut, size_t iCap)
{
	char sActorKey[128] = {0};
	char sActorId[128] = {0};
	char sIp[128] = {0};
	char sCookie[128] = {0};
	xvalue tblBody = xvoCreateTable();
	const char* sKey;
	if ( sOut && (iCap > 0) ) sOut[0] = '\0';
	xsReqQueryValue(objReq, "actorKey", sActorKey, sizeof(sActorKey));
	xsReqQueryValue(objReq, "actorId", sActorId, sizeof(sActorId));
	xsReqQueryValue(objReq, "ip", sIp, sizeof(sIp));
	xsReqQueryValue(objReq, "cookieKey", sCookie, sizeof(sCookie));
	xvoTableSetText(tblBody, "actorKey", 8, sActorKey, 0, FALSE);
	xvoTableSetText(tblBody, "actorId", 7, sActorId, 0, FALSE);
	xvoTableSetText(tblBody, "ip", 2, sIp, 0, FALSE);
	xvoTableSetText(tblBody, "cookieKey", 9, sCookie, 0, FALSE);
	sKey = Managed_LikeActorKey(tblBody);
	if ( sOut && (iCap > 0) ) {
		snprintf(sOut, iCap, "%s", Managed_IsBlank(sKey) ? "anonymous" : sKey);
	}
	xvoUnref(tblBody);
}

const char* Managed_LikeActorId(xvalue tblBody)
{
	const char* sActorId = tblBody ? xvoTableGetText(tblBody, "actorId", 7) : NULL;
	if ( !Managed_IsBlank(sActorId) ) return sActorId;
	return "anonymous";
}

void Managed_RequestLikeStatusPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sContentId[32] = {0};
	char sActorKey[128] = {0};
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet;
	int64 iContentId;
	int iLiked = 0;
	int iCount = 0;
	(void)objServer; (void)objHost; (void)objSession;
	xsReqQueryValue(objReq, "contentId", sContentId, sizeof(sContentId));
	Managed_LikeActorKeyFromQuery(objReq, sActorKey, sizeof(sActorKey));
	iContentId = atoll(sContentId);
	if ( !Managed_AbilityPackMounted("content.like") ) {
		Managed_SendError(objResp, "like ability pack is not enabled");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT like_count FROM like_counter WHERE content_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) iCount = sqlite3_column_int(stmt, 0);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( sqlite3_prepare_v2(pDb, "SELECT status FROM like_record WHERE content_id=? AND actor_key=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_text(stmt, 2, sActorKey[0] ? sActorKey : "anonymous", -1, SQLITE_TRANSIENT);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) iLiked = sqlite3_column_int(stmt, 0) == 1 ? 1 : 0;
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetInt(tblRet, "contentId", 9, iContentId);
	xvoTableSetInt(tblRet, "likeCount", 9, iCount);
	xvoTableSetBool(tblRet, "liked", 5, iLiked ? TRUE : FALSE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestLikeCreatePublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet;
	int64 iContentId = tblBody ? xvoTableGetInt(tblBody, "contentId", 9) : 0;
	const char* sActorKey = Managed_LikeActorKey(tblBody);
	const char* sActorId = tblBody ? xvoTableGetText(tblBody, "actorId", 7) : NULL;
	const char* sIp = tblBody ? xvoTableGetText(tblBody, "ip", 2) : NULL;
	int64 iNow = xrtNow();
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.like") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "like ability pack is not enabled");
		return;
	}
	if ( !Managed_AbilityPackConfigBool("content.like", "allowGuest", TRUE) && Managed_IsBlank(sActorId) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "guest likes are disabled");
		return;
	}
	if ( iContentId <= 0 ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "missing contentId");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM like_record WHERE content_id=? AND actor_key=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_text(stmt, 2, sActorKey, -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO like_record(content_id,actor_id,actor_key,ip,status,create_time,update_time) VALUES(?,?,?,?,1,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_text(stmt, 2, sActorId ? sActorId : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, sActorKey, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, sIp ? sIp : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 5, iNow);
		sqlite3_bind_int64(stmt, 6, iNow);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_UpdateLikeCounter(pDb, iContentId);
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "liked");
	xvoTableSetInt(tblRet, "contentId", 9, iContentId);
	xvoTableSetBool(tblRet, "liked", 5, TRUE);
	if ( tblBody ) xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestLikeCancelPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet;
	int64 iContentId = tblBody ? xvoTableGetInt(tblBody, "contentId", 9) : 0;
	const char* sActorKey = Managed_LikeActorKey(tblBody);
	int64 iNow = xrtNow();
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.like") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "like ability pack is not enabled");
		return;
	}
	if ( iContentId <= 0 ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "missing contentId");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE like_record SET status=0,update_time=? WHERE content_id=? AND actor_key=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, iNow);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iContentId);
		sqlite3_bind_text(stmt, 3, sActorKey, -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_UpdateLikeCounter(pDb, iContentId);
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "cancelled");
	xvoTableSetInt(tblRet, "contentId", 9, iContentId);
	xvoTableSetBool(tblRet, "liked", 5, FALSE);
	if ( tblBody ) xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_AppendLikeRecordRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();
	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblRow, "contentId", 9, sqlite3_column_int64(stmt, 1));
	xvoTableSetText(tblRow, "actorId", 7, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetText(tblRow, "actorKey", 8, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetText(tblRow, "ip", 2, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetInt(tblRow, "status", 6, sqlite3_column_int(stmt, 5));
	xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 6));
	xvoTableSetInt(tblRow, "updateTime", 10, sqlite3_column_int64(stmt, 7));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_AppendLikeCounterRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();
	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblRow, "contentId", 9, sqlite3_column_int64(stmt, 1));
	xvoTableSetInt(tblRow, "likeCount", 9, sqlite3_column_int(stmt, 2));
	xvoTableSetInt(tblRow, "updateTime", 10, sqlite3_column_int64(stmt, 3));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_RequestLikeListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sContentId[32] = {0};
	char sLimit[16] = {0};
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblRet = NULL;
	int64 iContentId = 0;
	int iLimit = 50;
	(void)objServer; (void)objHost; (void)objSession;
	Managed_ReadTextQuery(objReq, "contentId", sContentId, sizeof(sContentId));
	Managed_ReadTextQuery(objReq, "limit", sLimit, sizeof(sLimit));
	iContentId = atoll(sContentId);
	if ( atoi(sLimit) > 0 ) {
		iLimit = atoi(sLimit);
		if ( iLimit > 200 ) iLimit = 200;
	}
	if ( !Managed_AbilityPackMounted("content.like") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "like ability pack is not enabled");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, (iContentId > 0) ? "SELECT id,content_id,actor_id,actor_key,ip,status,create_time,update_time FROM like_record WHERE content_id=? ORDER BY update_time DESC,id DESC LIMIT ?" : "SELECT id,content_id,actor_id,actor_key,ip,status,create_time,update_time FROM like_record ORDER BY update_time DESC,id DESC LIMIT ?", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( iContentId > 0 ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
			sqlite3_bind_int(stmt, 2, iLimit);
		} else {
			sqlite3_bind_int(stmt, 1, iLimit);
		}
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendLikeRecordRow(arrList, stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestLikeCounterListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblRet = NULL;
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.like") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "like ability pack is not enabled");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id,content_id,like_count,update_time FROM like_counter ORDER BY like_count DESC, update_time DESC LIMIT 100", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendLikeCounterRow(arrList, stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestLikeStatsAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	xvalue tblData = xvoCreateTable();
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.like") ) {
		xvoUnref(tblData);
		Managed_SendError(objResp, "like ability pack is not enabled");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		xvoUnref(tblData);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT COUNT(*),COALESCE(SUM(like_count),0),COALESCE(MAX(like_count),0),COALESCE(MAX(update_time),0) FROM like_counter", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvoTableSetInt(tblData, "contentCount", 12, sqlite3_column_int64(stmt, 0));
			xvoTableSetInt(tblData, "totalLikes", 10, sqlite3_column_int64(stmt, 1));
			xvoTableSetInt(tblData, "maxLikes", 8, sqlite3_column_int64(stmt, 2));
			xvoTableSetInt(tblData, "lastUpdateTime", 14, sqlite3_column_int64(stmt, 3));
			Managed_SetTimeText(tblData, "lastUpdateTimeText", 18, sqlite3_column_int64(stmt, 3));
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT COUNT(*),COALESCE(SUM(CASE WHEN status=1 THEN 1 ELSE 0 END),0),COALESCE(SUM(CASE WHEN status=0 THEN 1 ELSE 0 END),0) FROM like_record", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvoTableSetInt(tblData, "recordCount", 11, sqlite3_column_int64(stmt, 0));
			xvoTableSetInt(tblData, "activeRecordCount", 17, sqlite3_column_int64(stmt, 1));
			xvoTableSetInt(tblData, "cancelledRecordCount", 20, sqlite3_column_int64(stmt, 2));
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestLikeSetStatusAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	int64 iContentId = 0;
	int iStatus = tblBody ? (int)xvoTableGetInt(tblBody, "status", 6) : 0;
	int64 iNow = xrtNow();
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.like") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "like ability pack is not enabled");
		return;
	}
	if ( iId <= 0 ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "missing id");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT content_id FROM like_record WHERE id=? LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) iContentId = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE like_record SET status=?,update_time=? WHERE id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iStatus ? 1 : 0);
		sqlite3_bind_int64(stmt, 2, iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	Managed_UpdateLikeCounter(pDb, iContentId);
	Managed_CloseDb(pDb);
	if ( tblBody ) xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "ok"));
}

const char* Managed_ViewVisitorKey(xvalue tblBody)
{
	const char* sVisitorKey = tblBody ? xvoTableGetText(tblBody, "visitorKey", 10) : NULL;
	if ( !Managed_IsBlank(sVisitorKey) ) return sVisitorKey;
	return "anonymous";
}

bool Managed_ViewHasVisitor(sqlite3* pDb, int64 iContentId, const char* sVisitorKey)
{
	sqlite3_stmt* stmt = NULL;
	bool bExists = FALSE;
	if ( (pDb == NULL) || (iContentId <= 0) || Managed_IsBlank(sVisitorKey) ) {
		return FALSE;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id FROM view_log WHERE content_id=? AND visitor_key=? LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_text(stmt, 2, sVisitorKey, -1, SQLITE_TRANSIENT);
		bExists = (sqlite3_step(stmt) == SQLITE_ROW);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return bExists;
}

void Managed_ViewUpdateCounter(sqlite3* pDb, int64 iContentId, bool bUnique)
{
	sqlite3_stmt* stmt = NULL;
	int iViewCount = 0;
	int iUniqueCount = 0;
	int64 iNow = xrtNow();
	if ( (pDb == NULL) || (iContentId <= 0) ) {
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT view_count,unique_view_count FROM view_counter WHERE content_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iViewCount = sqlite3_column_int(stmt, 0);
			iUniqueCount = sqlite3_column_int(stmt, 1);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	iViewCount++;
	if ( bUnique ) iUniqueCount++;
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM view_counter WHERE content_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO view_counter(content_id,view_count,unique_view_count,last_view_time) VALUES(?,?,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_int(stmt, 2, iViewCount);
		sqlite3_bind_int(stmt, 3, iUniqueCount);
		sqlite3_bind_int64(stmt, 4, iNow);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
}

void Managed_ViewUpdateDaily(sqlite3* pDb, int64 iContentId, bool bUnique)
{
	sqlite3_stmt* stmt = NULL;
	str sDate = xrtFormat("%lld", (long long)(xrtNow() / 86400));
	int iViewCount = 0;
	int iUniqueCount = 0;
	if ( (pDb == NULL) || (iContentId <= 0) || (sDate == NULL) ) {
		if ( sDate ) xrtFree(sDate);
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT view_count,unique_view_count FROM view_daily_stat WHERE content_id=? AND stat_date=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_text(stmt, 2, (const char*)sDate, -1, SQLITE_TRANSIENT);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iViewCount = sqlite3_column_int(stmt, 0);
			iUniqueCount = sqlite3_column_int(stmt, 1);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	iViewCount++;
	if ( bUnique ) iUniqueCount++;
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM view_daily_stat WHERE content_id=? AND stat_date=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_text(stmt, 2, (const char*)sDate, -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO view_daily_stat(content_id,stat_date,view_count,unique_view_count) VALUES(?,?,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_text(stmt, 2, (const char*)sDate, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 3, iViewCount);
		sqlite3_bind_int(stmt, 4, iUniqueCount);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	xrtFree(sDate);
}

void Managed_RequestViewRecordPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet;
	int64 iContentId = tblBody ? xvoTableGetInt(tblBody, "contentId", 9) : 0;
	const char* sVisitorKey = Managed_ViewVisitorKey(tblBody);
	const char* sIp = tblBody ? xvoTableGetText(tblBody, "ip", 2) : NULL;
	const char* sReferer = tblBody ? xvoTableGetText(tblBody, "referer", 7) : NULL;
	const char* sUserAgent = tblBody ? xvoTableGetText(tblBody, "userAgent", 9) : NULL;
	int64 iNow = xrtNow();
	bool bUnique;
	bool bEnableViewLog;
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.view-stat") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "view-stat ability pack is not enabled");
		return;
	}
	if ( iContentId <= 0 ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "missing contentId");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	bEnableViewLog = Managed_AbilityPackConfigBool("content.view-stat", "enableViewLog", TRUE);
	bUnique = bEnableViewLog ? !Managed_ViewHasVisitor(pDb, iContentId, sVisitorKey) : FALSE;
	if ( bEnableViewLog ) {
		if ( sqlite3_prepare_v2(pDb, "INSERT INTO view_log(content_id,visitor_key,ip,referer,user_agent,create_time) VALUES(?,?,?,?,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
			sqlite3_bind_text(stmt, 2, sVisitorKey, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sIp ? sIp : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, sReferer ? sReferer : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 5, sUserAgent ? sUserAgent : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 6, iNow);
			sqlite3_step(stmt);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_ViewUpdateCounter(pDb, iContentId, bUnique);
	Managed_ViewUpdateDaily(pDb, iContentId, bUnique);
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "recorded");
	xvoTableSetInt(tblRet, "contentId", 9, iContentId);
	xvoTableSetBool(tblRet, "unique", 6, bUnique ? TRUE : FALSE);
	if ( tblBody ) xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestViewStatusPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sContentId[32] = {0};
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet;
	int64 iContentId;
	int iViewCount = 0;
	int iUniqueCount = 0;
	(void)objServer; (void)objHost; (void)objSession;
	xsReqQueryValue(objReq, "contentId", sContentId, sizeof(sContentId));
	iContentId = atoll(sContentId);
	if ( !Managed_AbilityPackMounted("content.view-stat") ) {
		Managed_SendError(objResp, "view-stat ability pack is not enabled");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT view_count,unique_view_count FROM view_counter WHERE content_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iViewCount = sqlite3_column_int(stmt, 0);
			iUniqueCount = sqlite3_column_int(stmt, 1);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetInt(tblRet, "contentId", 9, iContentId);
	xvoTableSetInt(tblRet, "viewCount", 9, iViewCount);
	xvoTableSetInt(tblRet, "uniqueViewCount", 15, iUniqueCount);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestViewRankPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblRet;
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.view-stat") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "view-stat ability pack is not enabled");
		return;
	}
	if ( !Managed_AbilityPackConfigBool("content.view-stat", "rankEnabled", TRUE) ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "view rank API is disabled");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT content_id,view_count,unique_view_count,last_view_time FROM view_counter ORDER BY view_count DESC, content_id ASC LIMIT 20", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblRow = xvoCreateTable();
			xvoTableSetInt(tblRow, "contentId", 9, sqlite3_column_int64(stmt, 0));
			xvoTableSetInt(tblRow, "viewCount", 9, sqlite3_column_int(stmt, 1));
			xvoTableSetInt(tblRow, "uniqueViewCount", 15, sqlite3_column_int(stmt, 2));
			xvoTableSetInt(tblRow, "lastViewTime", 12, sqlite3_column_int64(stmt, 3));
			xvoArrayAppendValue(arrList, tblRow, TRUE);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_AppendViewCounterRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();
	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblRow, "contentId", 9, sqlite3_column_int64(stmt, 1));
	xvoTableSetInt(tblRow, "viewCount", 9, sqlite3_column_int(stmt, 2));
	xvoTableSetInt(tblRow, "uniqueViewCount", 15, sqlite3_column_int(stmt, 3));
	xvoTableSetInt(tblRow, "lastViewTime", 12, sqlite3_column_int64(stmt, 4));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_AppendViewLogRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();
	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblRow, "contentId", 9, sqlite3_column_int64(stmt, 1));
	xvoTableSetText(tblRow, "visitorKey", 10, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetText(tblRow, "ip", 2, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetText(tblRow, "referer", 7, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetText(tblRow, "userAgent", 9, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
	xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 6));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_AppendViewDailyRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();
	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblRow, "contentId", 9, sqlite3_column_int64(stmt, 1));
	xvoTableSetText(tblRow, "statDate", 8, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetInt(tblRow, "viewCount", 9, sqlite3_column_int(stmt, 3));
	xvoTableSetInt(tblRow, "uniqueViewCount", 15, sqlite3_column_int(stmt, 4));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_RequestViewCounterListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblRet = NULL;
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.view-stat") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "view-stat ability pack is not enabled");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id,content_id,view_count,unique_view_count,last_view_time FROM view_counter ORDER BY view_count DESC,last_view_time DESC LIMIT 100", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendViewCounterRow(arrList, stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestViewLogListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sContentId[32] = {0};
	char sLimit[16] = {0};
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblRet = NULL;
	int64 iContentId = 0;
	int iLimit = 50;
	(void)objServer; (void)objHost; (void)objSession;
	Managed_ReadTextQuery(objReq, "contentId", sContentId, sizeof(sContentId));
	Managed_ReadTextQuery(objReq, "limit", sLimit, sizeof(sLimit));
	iContentId = atoll(sContentId);
	if ( atoi(sLimit) > 0 ) {
		iLimit = atoi(sLimit);
		if ( iLimit > 200 ) iLimit = 200;
	}
	if ( !Managed_AbilityPackMounted("content.view-stat") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "view-stat ability pack is not enabled");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, (iContentId > 0) ? "SELECT id,content_id,visitor_key,ip,referer,user_agent,create_time FROM view_log WHERE content_id=? ORDER BY create_time DESC,id DESC LIMIT ?" : "SELECT id,content_id,visitor_key,ip,referer,user_agent,create_time FROM view_log ORDER BY create_time DESC,id DESC LIMIT ?", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( iContentId > 0 ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
			sqlite3_bind_int(stmt, 2, iLimit);
		} else {
			sqlite3_bind_int(stmt, 1, iLimit);
		}
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendViewLogRow(arrList, stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestViewDailyListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblRet = NULL;
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.view-stat") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "view-stat ability pack is not enabled");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id,content_id,stat_date,view_count,unique_view_count FROM view_daily_stat ORDER BY stat_date DESC,view_count DESC LIMIT 100", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendViewDailyRow(arrList, stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestViewStatsAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	xvalue tblData = xvoCreateTable();
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.view-stat") ) {
		xvoUnref(tblData);
		Managed_SendError(objResp, "view-stat ability pack is not enabled");
		return;
	}
	if ( !Managed_OpenDb(&pDb) ) {
		xvoUnref(tblData);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT COUNT(*),COALESCE(SUM(view_count),0),COALESCE(SUM(unique_view_count),0),COALESCE(MAX(last_view_time),0) FROM view_counter", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvoTableSetInt(tblData, "contentCount", 12, sqlite3_column_int64(stmt, 0));
			xvoTableSetInt(tblData, "totalViews", 10, sqlite3_column_int64(stmt, 1));
			xvoTableSetInt(tblData, "uniqueViews", 11, sqlite3_column_int64(stmt, 2));
			xvoTableSetInt(tblData, "lastViewTime", 12, sqlite3_column_int64(stmt, 3));
			Managed_SetTimeText(tblData, "lastViewTimeText", 16, sqlite3_column_int64(stmt, 3));
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT COUNT(*),COUNT(DISTINCT visitor_key),COALESCE(MAX(create_time),0) FROM view_log", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvoTableSetInt(tblData, "logCount", 8, sqlite3_column_int64(stmt, 0));
			xvoTableSetInt(tblData, "visitorCount", 12, sqlite3_column_int64(stmt, 1));
			xvoTableSetInt(tblData, "lastLogTime", 11, sqlite3_column_int64(stmt, 2));
			Managed_SetTimeText(tblData, "lastLogTimeText", 15, sqlite3_column_int64(stmt, 2));
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

int Managed_SensitiveScanData(sqlite3* pDb, xvalue tblData, const char* sTargetType, int64 iTargetId, xvalue arrHits)
{
	sqlite3_stmt* stmt = NULL;
	xvalue tblSpec = NULL;
	xvalue arrFields = NULL;
	xvalue arrScanFields = NULL;
	const char* arrFallbackFields[] = { "title", "summary", "content", "body", "description", NULL };
	int iHits = 0;
	int64 iNow = xrtNow();

	if ( (pDb == NULL) || (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) ) {
		return 0;
	}
	tblSpec = Managed_LoadSpec();
	arrFields = tblSpec ? Managed_GetFields(tblSpec) : NULL;
	arrScanFields = Managed_AbilityPackConfigArrayDup("content.sensitive", "fields");
	if ( sqlite3_prepare_v2(pDb, "SELECT id,word,level,replacement FROM sensitive_word WHERE status=1 AND delete_time=0 ORDER BY level DESC,id ASC", -1, &stmt, NULL) != SQLITE_OK ) {
		if ( arrScanFields ) xvoUnref(arrScanFields);
		if ( tblSpec ) xvoUnref(tblSpec);
		return 0;
	}
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		int64 iWordId = sqlite3_column_int64(stmt, 0);
		const char* sWord = (const char*)sqlite3_column_text(stmt, 1);
		int iLevel = sqlite3_column_int(stmt, 2);
		const char* sReplacement = (const char*)sqlite3_column_text(stmt, 3);
		int iFallbackIndex = 0;

		for ( uint32 i = 0; (arrFields != NULL) && (xvoType(arrFields) == XVO_DT_ARRAY) && (i < xvoArrayItemCount(arrFields)); i++ ) {
			xvalue tblField = xvoArrayGetValue(arrFields, i);
			const char* sField = ((tblField != NULL) && (xvoType(tblField) == XVO_DT_TABLE)) ? xvoTableGetText(tblField, "name", 4) : NULL;
			xvalue objValue = (!Managed_IsBlank(sField)) ? xvoTableGetValue(tblData, sField, (int)strlen(sField)) : NULL;
			str sText = Managed_ValueToTextDup(objValue);
			if ( arrScanFields && !Managed_ArrayContainsText(arrScanFields, sField) ) {
				if ( sText ) xrtFree(sText);
				continue;
			}
			if ( Managed_TextContainsWord((const char*)sText, sWord) ) {
				xvalue tblHit = xvoCreateTable();
				sqlite3_stmt* logStmt = NULL;
				xvoTableSetInt(tblHit, "wordId", 6, iWordId);
				xvoTableSetText(tblHit, "word", 4, (str)sWord, 0, FALSE);
				xvoTableSetInt(tblHit, "level", 5, iLevel);
				xvoTableSetText(tblHit, "field", 5, (str)(sField ? sField : ""), 0, FALSE);
				xvoTableSetText(tblHit, "replacement", 11, (str)(sReplacement ? sReplacement : ""), 0, FALSE);
				if ( arrHits ) xvoArrayAppendValue(arrHits, tblHit, TRUE);
				else xvoUnref(tblHit);
				if ( sqlite3_prepare_v2(pDb, "INSERT INTO sensitive_hit_log(target_type,target_id,word_id,word,field_name,action,create_time) VALUES(?,?,?,?,?,?,?)", -1, &logStmt, NULL) == SQLITE_OK ) {
					sqlite3_bind_text(logStmt, 1, sTargetType ? sTargetType : "content", -1, SQLITE_TRANSIENT);
					sqlite3_bind_int64(logStmt, 2, (sqlite3_int64)iTargetId);
					sqlite3_bind_int64(logStmt, 3, (sqlite3_int64)iWordId);
					sqlite3_bind_text(logStmt, 4, sWord ? sWord : "", -1, SQLITE_TRANSIENT);
					sqlite3_bind_text(logStmt, 5, sField ? sField : "", -1, SQLITE_TRANSIENT);
					sqlite3_bind_text(logStmt, 6, "hit", -1, SQLITE_TRANSIENT);
					sqlite3_bind_int64(logStmt, 7, iNow);
					sqlite3_step(logStmt);
					sqlite3_finalize(logStmt);
				}
				iHits++;
			}
			if ( sText ) xrtFree(sText);
		}
		while ( arrFallbackFields[iFallbackIndex] != NULL ) {
			const char* sField = arrFallbackFields[iFallbackIndex++];
			if ( Managed_FieldListContainsName(arrFields, sField) ) {
				continue;
			}
			if ( arrScanFields && !Managed_ArrayContainsText(arrScanFields, sField) ) {
				continue;
			}
			xvalue objValue = xvoTableGetValue(tblData, sField, (int)strlen(sField));
			str sText = Managed_ValueToTextDup(objValue);
			if ( Managed_TextContainsWord((const char*)sText, sWord) ) {
				xvalue tblHit = xvoCreateTable();
				sqlite3_stmt* logStmt = NULL;
				xvoTableSetInt(tblHit, "wordId", 6, iWordId);
				xvoTableSetText(tblHit, "word", 4, (str)sWord, 0, FALSE);
				xvoTableSetInt(tblHit, "level", 5, iLevel);
				xvoTableSetText(tblHit, "field", 5, (str)sField, 0, FALSE);
				xvoTableSetText(tblHit, "replacement", 11, (str)(sReplacement ? sReplacement : ""), 0, FALSE);
				if ( arrHits ) xvoArrayAppendValue(arrHits, tblHit, TRUE);
				else xvoUnref(tblHit);
				if ( sqlite3_prepare_v2(pDb, "INSERT INTO sensitive_hit_log(target_type,target_id,word_id,word,field_name,action,create_time) VALUES(?,?,?,?,?,?,?)", -1, &logStmt, NULL) == SQLITE_OK ) {
					sqlite3_bind_text(logStmt, 1, sTargetType ? sTargetType : "content", -1, SQLITE_TRANSIENT);
					sqlite3_bind_int64(logStmt, 2, (sqlite3_int64)iTargetId);
					sqlite3_bind_int64(logStmt, 3, (sqlite3_int64)iWordId);
					sqlite3_bind_text(logStmt, 4, sWord ? sWord : "", -1, SQLITE_TRANSIENT);
					sqlite3_bind_text(logStmt, 5, sField, -1, SQLITE_TRANSIENT);
					sqlite3_bind_text(logStmt, 6, "hit", -1, SQLITE_TRANSIENT);
					sqlite3_bind_int64(logStmt, 7, iNow);
					sqlite3_step(logStmt);
					sqlite3_finalize(logStmt);
				}
				iHits++;
			}
			if ( sText ) xrtFree(sText);
		}
	}
	sqlite3_finalize(stmt);
	if ( arrScanFields ) xvoUnref(arrScanFields);
	if ( tblSpec ) xvoUnref(tblSpec);
	return iHits;
}

void Managed_SensitiveApplyReplacement(xvalue tblData, xvalue arrHits)
{
	uint32 iCount;

	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || (arrHits == NULL) || (xvoType(arrHits) != XVO_DT_ARRAY) ) {
		return;
	}
	iCount = xvoArrayItemCount(arrHits);
	for ( uint32 i = 0; i < iCount; i++ ) {
		xvalue tblHit = xvoArrayGetValue(arrHits, i);
		const char* sField;
		const char* sWord;
		const char* sReplacement;
		str sSafeReplacement;
		xvalue objValue;
		str sText;
		str sNewText;

		if ( (tblHit == NULL) || (xvoType(tblHit) != XVO_DT_TABLE) ) {
			continue;
		}
		sField = xvoTableGetText(tblHit, "field", 5);
		sWord = xvoTableGetText(tblHit, "word", 4);
		sReplacement = xvoTableGetText(tblHit, "replacement", 11);
		if ( Managed_IsBlank(sField) || Managed_IsBlank(sWord) ) {
			continue;
		}
		objValue = xvoTableGetValue(tblData, sField, (int)strlen(sField));
		sText = Managed_ValueToTextDup(objValue);
		if ( Managed_IsBlank((const char*)sText) ) {
			if ( sText ) xrtFree(sText);
			continue;
		}
		sSafeReplacement = (str)(Managed_IsBlank(sReplacement) ? "***" : sReplacement);
		sNewText = xrtReplace(sText, 0, (str)sWord, 0, sSafeReplacement, 0, NULL);
		xrtFree(sText);
		if ( sNewText ) {
			xvoTableSetText(tblData, sField, (int)strlen(sField), sNewText, 0, TRUE);
		}
	}
}

bool Managed_SensitiveBeforeSave(sqlite3* pDb, xvalue tblData, int64 iTargetId, str* psError)
{
	xvalue arrHits = xvoCreateArray();
	str sStrategy = NULL;
	int iHits;
	if ( !Managed_AbilityPackMounted("content.sensitive") ) {
		xvoUnref(arrHits);
		return TRUE;
	}
	sStrategy = Managed_AbilityPackConfigTextDup("content.sensitive", "strategy", "block");
	iHits = Managed_SensitiveScanData(pDb, tblData, "content", iTargetId, arrHits);
	if ( iHits > 0 ) {
		if ( sStrategy && strcmp((const char*)sStrategy, "replace") == 0 ) {
			Managed_SensitiveApplyReplacement(tblData, arrHits);
			xrtFree(sStrategy);
			xvoUnref(arrHits);
			return TRUE;
		}
		if ( sStrategy && strcmp((const char*)sStrategy, "block") != 0 ) {
			xrtFree(sStrategy);
			xvoUnref(arrHits);
			return TRUE;
		}
		if ( psError ) {
			xvalue tblFirst = xvoArrayGetValue(arrHits, 0);
			const char* sWord = tblFirst ? (const char*)xvoTableGetText(tblFirst, "word", 4) : "";
			*psError = xrtFormat("sensitive word hit: %s", sWord ? sWord : "");
		}
		if ( sStrategy ) xrtFree(sStrategy);
		xvoUnref(arrHits);
		return FALSE;
	}
	if ( sStrategy ) xrtFree(sStrategy);
	xvoUnref(arrHits);
	return TRUE;
}

void Managed_SensitivePromotePendingLogs(sqlite3* pDb, int64 iTargetId, int64 iSince)
{
	sqlite3_stmt* stmt = NULL;

	if ( (pDb == NULL) || (iTargetId <= 0) || !Managed_AbilityPackMounted("content.sensitive") ) {
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE sensitive_hit_log SET target_id=? WHERE target_type='content' AND target_id=0 AND create_time>=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTargetId);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iSince);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
}

void Managed_RequestSensitiveCheckPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	xvalue tblData = NULL;
	xvalue arrHits = xvoCreateArray();
	xvalue tblRet = NULL;
	sqlite3* pDb = NULL;
	int iHits = 0;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !Managed_AbilityPackMounted("content.sensitive") ) {
		if ( tblBody ) xvoUnref(tblBody);
		xvoUnref(arrHits);
		Managed_SendError(objResp, "sensitive ability pack is not enabled");
		return;
	}
	if ( tblBody == NULL ) {
		xvoUnref(arrHits);
		Managed_SendError(objResp, "invalid request body");
		return;
	}
	tblData = xvoTableGetValue(tblBody, "data", 4);
	if ( tblData == NULL ) tblData = tblBody;
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblBody);
		xvoUnref(arrHits);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	iHits = Managed_SensitiveScanData(pDb, tblData, "check", 0, arrHits);
	Managed_CloseDb(pDb);
	xvoUnref(tblBody);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetInt(tblRet, "hitCount", 8, iHits);
	xvoTableSetBool(tblRet, "passed", 6, iHits == 0);
	xvoTableSetValue(tblRet, "hits", 4, arrHits, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestSensitiveWordSaveAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	const char* sWord = NULL;
	const char* sScope = NULL;
	const char* sReplacement = NULL;
	int64 iId = 0;
	int iLevel = 1;
	int iStatus = 1;
	int64 iNow = xrtNow();
	bool bInsert = FALSE;

	(void)objServer;
	(void)objHost;

	if ( !Managed_AbilityPackMounted("content.sensitive") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "sensitive ability pack is not enabled");
		return;
	}
	if ( tblBody == NULL ) {
		Managed_SendError(objResp, "invalid request body");
		return;
	}
	iId = xvoTableGetInt(tblBody, "id", 2);
	bInsert = iId <= 0;
	sWord = xvoTableGetText(tblBody, "word", 4);
	sScope = xvoTableGetText(tblBody, "scope", 5);
	sReplacement = xvoTableGetText(tblBody, "replacement", 11);
	iLevel = (int)xvoTableGetInt(tblBody, "level", 5);
	iStatus = (int)xvoTableGetInt(tblBody, "status", 6);
	if ( Managed_IsBlank(sWord) ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "word is required");
		return;
	}
	if ( Managed_IsBlank(sScope) ) sScope = "content";
	if ( iLevel <= 0 ) iLevel = 1;
	if ( iStatus <= 0 ) iStatus = 1;
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( iId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE sensitive_word SET word=?,level=?,scope=?,replacement=?,status=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sWord, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 2, iLevel);
			sqlite3_bind_text(stmt, 3, sScope, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, sReplacement ? sReplacement : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 5, iStatus);
			sqlite3_bind_int64(stmt, 6, iNow);
			sqlite3_bind_int64(stmt, 7, (sqlite3_int64)iId);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	} else {
		if ( sqlite3_prepare_v2(pDb, "INSERT INTO sensitive_word(word,level,scope,replacement,status,create_time,update_time,delete_time) VALUES(?,?,?,?,?,?,?,0)", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sWord, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 2, iLevel);
			sqlite3_bind_text(stmt, 3, sScope, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, sReplacement ? sReplacement : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 5, iStatus);
			sqlite3_bind_int64(stmt, 6, iNow);
			sqlite3_bind_int64(stmt, 7, iNow);
			sqlite3_step(stmt);
			iId = sqlite3_last_insert_rowid(pDb);
			sqlite3_finalize(stmt);
		}
	}
	Managed_AuditLogWithRequest(pDb, "sensitive_word", iId, bInsert ? "sensitive.create" : "sensitive.update", sWord ? sWord : "", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	xvoUnref(tblBody);
	tblRet = Managed_CreateResult(TRUE, "saved");
	xvoTableSetInt(tblRet, "id", 2, iId);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestSensitiveWordDeleteAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = 0;
	int64 iNow = xrtNow();

	(void)objServer;
	(void)objHost;

	if ( !Managed_AbilityPackMounted("content.sensitive") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "sensitive ability pack is not enabled");
		return;
	}
	if ( tblBody == NULL ) {
		Managed_SendError(objResp, "invalid request body");
		return;
	}
	iId = xvoTableGetInt(tblBody, "id", 2);
	if ( iId <= 0 ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "missing id");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE sensitive_word SET delete_time=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, iNow);
		sqlite3_bind_int64(stmt, 2, iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	Managed_AuditLogWithRequest(pDb, "sensitive_word", iId, "sensitive.delete", "delete sensitive word", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "deleted"));
}

void Managed_AppendSensitiveLogRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();
	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetText(tblRow, "targetType", 10, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
	xvoTableSetInt(tblRow, "targetId", 8, sqlite3_column_int64(stmt, 2));
	xvoTableSetInt(tblRow, "wordId", 6, sqlite3_column_int64(stmt, 3));
	xvoTableSetText(tblRow, "word", 4, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetText(tblRow, "fieldName", 9, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
	xvoTableSetText(tblRow, "action", 6, (str)sqlite3_column_text(stmt, 6), 0, FALSE);
	xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 7));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_RequestSensitiveLogListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sTargetId[32] = {0};
	char sLimit[16] = {0};
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblRet = NULL;
	int64 iTargetId = 0;
	int iLimit = 50;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !Managed_AbilityPackMounted("content.sensitive") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "sensitive ability pack is not enabled");
		return;
	}
	Managed_ReadTextQuery(objReq, "targetId", sTargetId, sizeof(sTargetId));
	Managed_ReadTextQuery(objReq, "limit", sLimit, sizeof(sLimit));
	iTargetId = atoll(sTargetId);
	if ( atoi(sLimit) > 0 ) {
		iLimit = atoi(sLimit);
		if ( iLimit > 200 ) iLimit = 200;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		xvoUnref(arrList);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, (iTargetId > 0) ? "SELECT id,target_type,target_id,word_id,word,field_name,action,create_time FROM sensitive_hit_log WHERE target_id=? ORDER BY create_time DESC,id DESC LIMIT ?" : "SELECT id,target_type,target_id,word_id,word,field_name,action,create_time FROM sensitive_hit_log ORDER BY create_time DESC,id DESC LIMIT ?", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( iTargetId > 0 ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTargetId);
			sqlite3_bind_int(stmt, 2, iLimit);
		} else {
			sqlite3_bind_int(stmt, 1, iLimit);
		}
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendSensitiveLogRow(arrList, stmt);
		}
		sqlite3_finalize(stmt);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "ok");
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
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

void Managed_RequestListCommon(XS_ResponseObject objResp, XS_RequestObject objReq, xvalue objSession, bool bAdmin, int iForcedDraftFilter)
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
	bool bAttachTag = FALSE;
	bool bAttachTopic = FALSE;
	bool bAttachComment = FALSE;
	bool bAttachLike = FALSE;
	bool bAttachView = FALSE;
	bool bAttachMedia = FALSE;
	bool bAttachSeo = FALSE;
	bool bAccessPack = FALSE;
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
	bAttachTag = Managed_AbilityPackMounted("content.tag");
	bAttachTopic = Managed_AbilityPackMounted("content.topic");
	bAttachComment = Managed_AbilityPackMounted("content.comment");
	bAttachLike = Managed_AbilityPackMounted("content.like");
	bAttachView = Managed_AbilityPackMounted("content.view-stat");
	bAttachMedia = Managed_AbilityPackMounted("content.media");
	bAttachSeo = Managed_AbilityPackMounted("content.seo");
	bAccessPack = Managed_AbilityPackMounted("content.access");
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
			Managed_AttachAbilityListFields(pDb, tblItem, bAttachTag, bAttachTopic, bAttachComment, bAttachLike, bAttachView, bAttachMedia);
			if ( (!bAccessPack || bAdmin || Managed_AccessCheckRule(pDb, xvoTableGetInt(tblItem, "id", 2), objReq, objSession, bAdmin, NULL))
				&& Managed_RowMatchesFilters(tblItem, tblSpec, objReq, sQuery, bAdmin, iStatusFilter, iDraftFilter) ) {
				xvoArrayAppendValue(arrMatched, xvoCopy(tblItem), TRUE);
			}
			xvoUnref(arrRow);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	if ( sSql ) xrtFree(sSql);
	Managed_SortItems(arrMatched, tblSpec, sRequestedSortField, bSortAsc);
	iCount = xvoArrayItemCount(arrMatched);
	for ( uint32 i = (uint32)iOffset; (i < xvoArrayItemCount(arrMatched)) && (xvoArrayItemCount(arrList) < (uint32)iLimit); i++ ) {
		xvalue tblPageItem = xvoCopy(xvoArrayGetValue(arrMatched, i));
		if ( bAttachSeo ) Managed_AttachSeoMetaField(pDb, tblPageItem);
		xvoArrayAppendValue(arrList, tblPageItem, TRUE);
	}
	Managed_CloseDb(pDb);
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
	Managed_RequestListCommon(objResp, objReq, objSession, FALSE, 0);
}

void Managed_RequestListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;
	Managed_RequestListCommon(objResp, objReq, objSession, TRUE, 0);
}

void Managed_RequestDraftsAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;
	Managed_RequestListCommon(objResp, objReq, objSession, TRUE, 1);
}

void Managed_BuildSearchLikePattern(const char* sQuery, char* sOut, size_t iOutSize)
{
	size_t iPos = 0;
	bool bLastWildcard = FALSE;

	if ( (sOut == NULL) || (iOutSize <= 0) ) return;
	sOut[0] = '\0';
	if ( iOutSize <= 2 ) return;
	sOut[iPos++] = '%';
	bLastWildcard = TRUE;
	if ( sQuery != NULL ) {
		for ( const char* p = sQuery; (*p != '\0') && (iPos + 2 < iOutSize); p++ ) {
			bool bSpace = (*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n');
			if ( bSpace ) {
				if ( !bLastWildcard ) {
					sOut[iPos++] = '%';
					bLastWildcard = TRUE;
				}
				continue;
			}
			sOut[iPos++] = *p;
			bLastWildcard = FALSE;
		}
	}
	if ( !bLastWildcard && (iPos + 1 < iOutSize) ) sOut[iPos++] = '%';
	sOut[iPos] = '\0';
}

int Managed_TextFindIgnoreCase(const char* sText, const char* sNeedle)
{
	size_t iTextLen;
	size_t iNeedleLen;

	if ( Managed_IsBlank(sText) || Managed_IsBlank(sNeedle) ) return -1;
	iTextLen = strlen(sText);
	iNeedleLen = strlen(sNeedle);
	if ( iNeedleLen > iTextLen ) return -1;
	for ( size_t i = 0; i <= iTextLen - iNeedleLen; i++ ) {
		size_t j = 0;
		while ( j < iNeedleLen ) {
			if ( Managed_ToLowerAscii(sText[i + j]) != Managed_ToLowerAscii(sNeedle[j]) ) break;
			j++;
		}
		if ( j == iNeedleLen ) return (int)i;
	}
	return -1;
}

str Managed_SearchBuildSnippet(const char* sText, const char* sQuery)
{
	size_t iLen;
	size_t iStart = 0;
	int iHit;

	if ( Managed_IsBlank(sText) ) return xrtCopyStr("", 0);
	iLen = strlen(sText);
	if ( iLen <= 240 ) return xrtCopyStr((str)sText, 0);
	iHit = Managed_TextFindIgnoreCase(sText, sQuery);
	if ( iHit > 80 ) {
		iStart = (size_t)iHit - 80;
	}
	if ( iStart + 240 > iLen ) {
		iStart = iLen - 240;
	}
	return xrtCopyStr((str)(sText + iStart), 240);
}

void Managed_RequestSearchCommon(XS_ResponseObject objResp, XS_RequestObject objReq, xvalue objSession, bool bAdmin)
{
	char sQuery[160];
	char sLike[384];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblSpec = NULL;
	xvalue tblRet = NULL;
	xvalue arrList = NULL;
	int iPage = Managed_ReadIntQuery(objReq, "page", 1);
	int iLimit = Managed_ReadIntQuery(objReq, "limit", 20);
	int iOffset;
	int iCount = 0;
	int iThreshold = 0;
	const char* sSqlAdmin =
		"SELECT i.id,i.title,i.status,i.payload_json,i.is_draft,i.create_time,i.update_time,i.category_id,"
		"((CASE WHEN idx.title LIKE ? THEN 100 ELSE 0 END)+(CASE WHEN idx.keywords LIKE ? THEN 60 ELSE 0 END)+(CASE WHEN idx.summary LIKE ? THEN 35 ELSE 0 END)+(CASE WHEN idx.body LIKE ? THEN 10 ELSE 0 END)) AS search_score,"
		"idx.summary,idx.body FROM content_search_index idx INNER JOIN content_item i ON i.id=idx.content_id "
		"WHERE i.delete_time=0 AND idx.status>=? AND (idx.title LIKE ? OR idx.keywords LIKE ? OR idx.summary LIKE ? OR idx.body LIKE ?) "
		"ORDER BY search_score DESC,idx.update_time DESC,idx.content_id DESC LIMIT ? OFFSET ?";
	const char* sSqlPublic =
		"SELECT i.id,i.title,i.status,i.payload_json,i.is_draft,i.create_time,i.update_time,i.category_id,"
		"((CASE WHEN idx.title LIKE ? THEN 100 ELSE 0 END)+(CASE WHEN idx.keywords LIKE ? THEN 60 ELSE 0 END)+(CASE WHEN idx.summary LIKE ? THEN 35 ELSE 0 END)+(CASE WHEN idx.body LIKE ? THEN 10 ELSE 0 END)) AS search_score,"
		"idx.summary,idx.body FROM content_search_index idx INNER JOIN content_item i ON i.id=idx.content_id "
		"WHERE i.delete_time=0 AND i.is_draft=0 AND i.status>=? AND idx.status>=? AND (idx.title LIKE ? OR idx.keywords LIKE ? OR idx.summary LIKE ? OR idx.body LIKE ?) "
		"ORDER BY search_score DESC,idx.update_time DESC,idx.content_id DESC LIMIT ? OFFSET ?";
	const char* sCountAdmin =
		"SELECT COUNT(*) FROM content_search_index idx INNER JOIN content_item i ON i.id=idx.content_id "
		"WHERE i.delete_time=0 AND idx.status>=? AND (idx.title LIKE ? OR idx.keywords LIKE ? OR idx.summary LIKE ? OR idx.body LIKE ?)";
	const char* sCountPublic =
		"SELECT COUNT(*) FROM content_search_index idx INNER JOIN content_item i ON i.id=idx.content_id "
		"WHERE i.delete_time=0 AND i.is_draft=0 AND i.status>=? AND idx.status>=? AND (idx.title LIKE ? OR idx.keywords LIKE ? OR idx.summary LIKE ? OR idx.body LIKE ?)";

	if ( !Managed_AbilityPackMounted("content.search") ) {
		Managed_SendError(objResp, "search ability pack is not enabled");
		return;
	}
	Managed_ReadTextQuery(objReq, "q", sQuery, sizeof(sQuery));
	if ( Managed_IsBlank(sQuery) ) {
		Managed_RequestListCommon(objResp, objReq, objSession, bAdmin, 0);
		return;
	}
	if ( iPage < 1 ) iPage = 1;
	if ( iLimit < 1 ) iLimit = 20;
	if ( iLimit > 100 ) iLimit = 100;
	iOffset = (iPage - 1) * iLimit;
	Managed_BuildSearchLikePattern(sQuery, sLike, sizeof(sLike));
	tblSpec = Managed_LoadSpec();
	iThreshold = bAdmin ? 0 : Managed_PublicStatusThreshold(tblSpec);
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	tblRet = Managed_CreateResult(TRUE, NULL);
	arrList = xvoCreateArray();
	if ( sqlite3_prepare_v2(pDb, bAdmin ? sCountAdmin : sCountPublic, -1, &stmt, NULL) == SQLITE_OK ) {
		int iBind = 1;
		if ( bAdmin ) {
			sqlite3_bind_int(stmt, iBind++, iThreshold);
		} else {
			sqlite3_bind_int(stmt, iBind++, iThreshold);
			sqlite3_bind_int(stmt, iBind++, iThreshold);
		}
		for ( int i = 0; i < 4; i++ ) sqlite3_bind_text(stmt, iBind++, sLike, -1, SQLITE_TRANSIENT);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) iCount = sqlite3_column_int(stmt, 0);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( sqlite3_prepare_v2(pDb, bAdmin ? sSqlAdmin : sSqlPublic, -1, &stmt, NULL) == SQLITE_OK ) {
		int iBind = 1;
		for ( int i = 0; i < 4; i++ ) sqlite3_bind_text(stmt, iBind++, sLike, -1, SQLITE_TRANSIENT);
		if ( bAdmin ) {
			sqlite3_bind_int(stmt, iBind++, iThreshold);
		} else {
			sqlite3_bind_int(stmt, iBind++, iThreshold);
			sqlite3_bind_int(stmt, iBind++, iThreshold);
		}
		for ( int i = 0; i < 4; i++ ) sqlite3_bind_text(stmt, iBind++, sLike, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, iBind++, iLimit);
		sqlite3_bind_int(stmt, iBind++, iOffset);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblItem = Managed_CreateItemFromStmt(stmt, tblSpec);
			const char* sIndexSummary = (const char*)sqlite3_column_text(stmt, 9);
			const char* sIndexBody = (const char*)sqlite3_column_text(stmt, 10);
			if ( bAdmin || Managed_AccessCheckRule(pDb, xvoTableGetInt(tblItem, "id", 2), objReq, objSession, FALSE, NULL) ) {
				const char* sSnippet = Managed_TextContainsIgnoreCase(sIndexSummary, sQuery) ? sIndexSummary : sIndexBody;
				str sSnippetText = Managed_SearchBuildSnippet(sSnippet, sQuery);
				xvoTableSetInt(tblItem, "searchScore", 11, sqlite3_column_int(stmt, 8));
				xvoTableSetText(tblItem, "searchTerm", 10, (str)sQuery, 0, FALSE);
				xvoTableSetText(tblItem, "searchSnippet", 13, sSnippetText ? sSnippetText : (str)"", 0, sSnippetText ? TRUE : FALSE);
				xvoArrayAppendValue(arrList, tblItem, TRUE);
			}
			if ( tblItem ) xvoUnref(tblItem);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	if ( tblSpec ) xvoUnref(tblSpec);
	if ( xvoArrayItemCount(arrList) <= 0 ) {
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_RequestListCommon(objResp, objReq, objSession, bAdmin, 0);
		return;
	}
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetInt(tblRet, "page", 4, iPage);
	xvoTableSetInt(tblRet, "pageSize", 8, iLimit);
	xvoTableSetText(tblRet, "source", 6, "content_search_index", 0, FALSE);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestSearchPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;
	Managed_RequestSearchCommon(objResp, objReq, objSession, FALSE);
}

void Managed_RequestSearchAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;
	Managed_RequestSearchCommon(objResp, objReq, objSession, TRUE);
}

str Managed_AppendOwnedText(str sBase, const char* sAdd);

str Managed_SearchBuildBody(xvalue tblItem, xvalue tblSpec)
{
	xvalue tblData = tblItem ? xvoTableGetValue(tblItem, "data", 4) : NULL;
	xvalue arrFields = Managed_GetFields(tblSpec);
	str sBody = xrtCopyStr("", 0);
	const char* sTitle = tblItem ? xvoTableGetText(tblItem, "title", 5) : NULL;
	const char* sSlug = tblItem ? xvoTableGetText(tblItem, "slug", 4) : NULL;
	const char* sSummary = tblItem ? xvoTableGetText(tblItem, "summary", 7) : NULL;

	sBody = Managed_AppendOwnedText(sBody, sTitle);
	sBody = Managed_AppendOwnedText(sBody, " ");
	sBody = Managed_AppendOwnedText(sBody, sSlug);
	sBody = Managed_AppendOwnedText(sBody, " ");
	sBody = Managed_AppendOwnedText(sBody, sSummary);
	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) ) {
		return sBody;
	}
	for ( uint32 i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
		xvalue tblField = xvoArrayGetValue(arrFields, i);
		str sText = NULL;

		if ( !Managed_FieldIsSearchable(tblField) ) continue;
		sText = Managed_BuildListFieldSummary(tblField, tblData);
		if ( !Managed_IsBlank((const char*)sText) ) {
			sBody = Managed_AppendOwnedText(sBody, " ");
			sBody = Managed_AppendOwnedText(sBody, (const char*)sText);
		}
		if ( sText ) xrtFree(sText);
	}
	return sBody;
}

void Managed_SearchIndexDelete(sqlite3* pDb, int64 iContentId)
{
	sqlite3_stmt* stmt = NULL;

	if ( !Managed_AbilityPackMounted("content.search") || (pDb == NULL) || (iContentId <= 0) ) return;
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM content_search_index WHERE content_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
}

bool Managed_SearchIndexUpsert(sqlite3* pDb, xvalue tblItem, xvalue tblSpec, int64 iNow)
{
	sqlite3_stmt* stmt = NULL;
	str sBody = NULL;
	int64 iContentId;
	int iStatus;
	bool bOk = FALSE;

	if ( !Managed_AbilityPackMounted("content.search") || (pDb == NULL) || (tblItem == NULL) || (tblSpec == NULL) ) return FALSE;
	iContentId = xvoTableGetInt(tblItem, "id", 2);
	if ( iContentId <= 0 ) return FALSE;
	iStatus = (int)xvoTableGetInt(tblItem, "status", 6);
	sBody = Managed_SearchBuildBody(tblItem, tblSpec);
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_search_index(content_id,title,summary,body,keywords,status,update_time) VALUES(?,?,?,?,?,?,?) ON CONFLICT(content_id) DO UPDATE SET title=excluded.title,summary=excluded.summary,body=excluded.body,keywords=excluded.keywords,status=excluded.status,update_time=excluded.update_time", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_text(stmt, 2, (const char*)(xvoTableGetText(tblItem, "title", 5) ? xvoTableGetText(tblItem, "title", 5) : (str)""), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, (const char*)(xvoTableGetText(tblItem, "summary", 7) ? xvoTableGetText(tblItem, "summary", 7) : (str)""), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, sBody ? (const char*)sBody : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 5, (const char*)(xvoTableGetText(tblItem, "slug", 4) ? xvoTableGetText(tblItem, "slug", 4) : (str)""), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 6, iStatus);
		sqlite3_bind_int64(stmt, 7, (sqlite3_int64)iNow);
		bOk = (sqlite3_step(stmt) == SQLITE_DONE);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	if ( sBody ) xrtFree(sBody);
	return bOk;
}

void Managed_SearchIndexSyncData(sqlite3* pDb, xvalue tblSpec, int64 iContentId, const char* sTitle, int iStatus, xvalue tblData, int64 iNow)
{
	xvalue tblItem;

	if ( !Managed_AbilityPackMounted("content.search") || (pDb == NULL) || (tblSpec == NULL) || (tblData == NULL) || (iContentId <= 0) ) return;
	tblItem = xvoCreateTable();
	xvoTableSetInt(tblItem, "id", 2, iContentId);
	xvoTableSetText(tblItem, "title", 5, (str)(sTitle ? sTitle : ""), 0, FALSE);
	xvoTableSetInt(tblItem, "status", 6, iStatus);
	xvoTableSetValue(tblItem, "data", 4, xvoCopy(tblData), TRUE);
	Managed_AppendDerivedFields(tblItem, tblSpec);
	Managed_SearchIndexUpsert(pDb, tblItem, tblSpec, iNow);
	xvoUnref(tblItem);
}

void Managed_RequestSearchRebuildAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblSpec = NULL;
	xvalue tblRet = NULL;
	int64 iNow = xrtNow();
	int iTotal = 0;
	int iIndexed = 0;

	(void)objServer; (void)objHost; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.search") ) {
		Managed_SendError(objResp, "search ability pack is not enabled");
		return;
	}
	tblSpec = Managed_LoadSpec();
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) || (tblSpec == NULL) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	Managed_ExecSql(pDb, "BEGIN IMMEDIATE");
	Managed_ExecSql(pDb, "DELETE FROM content_search_index");
	if ( sqlite3_prepare_v2(pDb, "SELECT id,title,status,payload_json,is_draft,create_time,update_time,category_id FROM content_item WHERE delete_time=0 ORDER BY id ASC", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblItem = Managed_CreateItemFromStmt(stmt, tblSpec);
			iTotal++;
			if ( Managed_SearchIndexUpsert(pDb, tblItem, tblSpec, iNow) ) iIndexed++;
			if ( tblItem ) xvoUnref(tblItem);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_ExecSql(pDb, "COMMIT");
	Managed_CloseDb(pDb);
	if ( tblSpec ) xvoUnref(tblSpec);
	tblRet = Managed_CreateResult(TRUE, "search index rebuilt");
	xvoTableSetInt(tblRet, "total", 5, iTotal);
	xvoTableSetInt(tblRet, "indexed", 7, iIndexed);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestSearchStatsAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblData = xvoCreateTable();
	xvalue tblRet = NULL;
	int64 iContentCount = 0;
	int64 iIndexCount = 0;
	int64 iLastUpdate = 0;

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.search") ) {
		xvoUnref(tblData);
		Managed_SendError(objResp, "search ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblData);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT COUNT(*) FROM content_item WHERE delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) iContentCount = sqlite3_column_int64(stmt, 0);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( sqlite3_prepare_v2(pDb, "SELECT COUNT(*),COALESCE(MAX(update_time),0) FROM content_search_index", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iIndexCount = sqlite3_column_int64(stmt, 0);
			iLastUpdate = sqlite3_column_int64(stmt, 1);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoTableSetInt(tblData, "contentCount", 12, iContentCount);
	xvoTableSetInt(tblData, "indexCount", 10, iIndexCount);
	xvoTableSetInt(tblData, "missingCount", 12, iContentCount > iIndexCount ? (iContentCount - iIndexCount) : 0);
	xvoTableSetInt(tblData, "lastUpdate", 10, iLastUpdate);
	Managed_SetTimeText(tblData, "lastUpdateText", 14, iLastUpdate);
	tblRet = Managed_CreateResult(TRUE, NULL);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

str Managed_AppendOwnedText(str sBase, const char* sAdd)
{
	str sNext = xrtFormat("%s%s", sBase ? (const char*)sBase : "", sAdd ? sAdd : "");
	if ( sBase ) xrtFree(sBase);
	return sNext;
}

str Managed_XmlEscape(const char* sText)
{
	str sOut = xrtCopyStr("", 0);
	const char* p;
	char sBuf[2];

	if ( sText == NULL ) return sOut;
	sBuf[1] = '\0';
	for ( p = sText; *p; p++ ) {
		switch ( *p ) {
		case '&': sOut = Managed_AppendOwnedText(sOut, "&amp;"); break;
		case '<': sOut = Managed_AppendOwnedText(sOut, "&lt;"); break;
		case '>': sOut = Managed_AppendOwnedText(sOut, "&gt;"); break;
		case '"': sOut = Managed_AppendOwnedText(sOut, "&quot;"); break;
		case '\'': sOut = Managed_AppendOwnedText(sOut, "&apos;"); break;
		default:
			sBuf[0] = *p;
			sOut = Managed_AppendOwnedText(sOut, sBuf);
			break;
		}
	}
	return sOut;
}

bool Managed_SitemapIsAbsoluteUrl(const char* sUrl)
{
	if ( Managed_IsBlank(sUrl) ) return FALSE;
	return (strncmp(sUrl, "http://", 7) == 0) || (strncmp(sUrl, "https://", 8) == 0);
}

bool Managed_SitemapUrlConfigSafe(const char* sUrl)
{
	const char* p = sUrl;

	if ( Managed_IsBlank(sUrl) ) return TRUE;
	while ( *p ) {
		if ( (*p == '\r') || (*p == '\n') ) return FALSE;
		p++;
	}
	return Managed_SitemapIsAbsoluteUrl(sUrl);
}

str Managed_SitemapSiteUrlDup()
{
	str sBase = Managed_AbilityPackConfigTextDup("content.sitemap", "siteUrl", "");
	int nLen;

	if ( Managed_IsBlank(sBase) || !Managed_SitemapUrlConfigSafe((const char*)sBase) ) {
		if ( sBase ) xrtFree(sBase);
		return NULL;
	}
	nLen = (int)strlen((const char*)sBase);
	while ( nLen > 0 && (((const char*)sBase)[nLen - 1] <= ' ' || ((const char*)sBase)[nLen - 1] == '/') ) {
		((char*)sBase)[nLen - 1] = 0;
		nLen--;
	}
	if ( nLen <= 0 ) {
		if ( sBase ) xrtFree(sBase);
		return NULL;
	}
	return sBase;
}

int Managed_SitemapCacheTtlSeconds()
{
	int iTtl = Managed_AbilityPackConfigInt("content.sitemap", "cacheTtlSeconds", 3600);
	if ( iTtl < 0 ) iTtl = 0;
	return iTtl;
}

str Managed_SitemapAbsoluteUrl(const char* sUrl)
{
	str sBase = NULL;
	str sOut = NULL;

	if ( Managed_IsBlank(sUrl) ) return xrtCopyStr((str)"", 0);
	if ( Managed_SitemapIsAbsoluteUrl(sUrl) ) return xrtCopyStr((str)sUrl, 0);
	sBase = Managed_SitemapSiteUrlDup();
	if ( Managed_IsBlank(sBase) ) return xrtCopyStr((str)sUrl, 0);
	if ( sUrl[0] == '/' ) {
		sOut = xrtFormat("%s%s", (const char*)sBase, sUrl);
	} else {
		sOut = xrtFormat("%s/%s", (const char*)sBase, sUrl);
	}
	if ( sBase ) xrtFree(sBase);
	return sOut;
}

str Managed_SitemapItemUrl(xvalue tblItem)
{
	int64 iId = tblItem ? xvoTableGetInt(tblItem, "id", 2) : 0;
	const char* sSlug = tblItem ? xvoTableGetText(tblItem, "slug", 4) : NULL;
	str sRel = NULL;
	str sOut = NULL;

	if ( !Managed_IsBlank(sSlug) && Managed_AbilityPackMounted("content.slug") ) {
		sRel = xrtFormat("/plugin/{{PLUGIN_XID}}?slug=%s", sSlug);
	} else {
		sRel = xrtFormat("/plugin/{{PLUGIN_XID}}?id=%lld", (long long)iId);
	}
	sOut = Managed_SitemapAbsoluteUrl(sRel ? (const char*)sRel : "");
	if ( sRel ) xrtFree(sRel);
	return sOut;
}

bool Managed_SitemapUpsertEntry(sqlite3* pDb, xvalue tblItem, int64 iNow)
{
	sqlite3_stmt* stmt = NULL;
	str sLoc = NULL;
	int64 iContentId;
	bool bOk = FALSE;

	if ( !Managed_AbilityPackMounted("content.sitemap") || (pDb == NULL) || (tblItem == NULL) ) return FALSE;
	iContentId = xvoTableGetInt(tblItem, "id", 2);
	if ( iContentId <= 0 ) return FALSE;
	sLoc = Managed_SitemapItemUrl(tblItem);
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_sitemap_entry(content_id,loc,title,type,priority,changefreq,status,update_time) VALUES(?,?,?,?,?,?,?,?) ON CONFLICT(content_id,type) DO UPDATE SET loc=excluded.loc,title=excluded.title,priority=excluded.priority,changefreq=excluded.changefreq,status=excluded.status,update_time=excluded.update_time", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_text(stmt, 2, sLoc ? (const char*)sLoc : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, (const char*)(xvoTableGetText(tblItem, "title", 5) ? xvoTableGetText(tblItem, "title", 5) : (str)""), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, "content", -1, SQLITE_TRANSIENT);
		sqlite3_bind_double(stmt, 5, 0.8);
		sqlite3_bind_text(stmt, 6, "weekly", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 7, 1);
		sqlite3_bind_int64(stmt, 8, (sqlite3_int64)iNow);
		bOk = (sqlite3_step(stmt) == SQLITE_DONE);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	if ( sLoc ) xrtFree(sLoc);
	return bOk;
}

void Managed_SitemapRemoveEntry(sqlite3* pDb, int64 iContentId)
{
	sqlite3_stmt* stmt = NULL;

	if ( !Managed_AbilityPackMounted("content.sitemap") || (pDb == NULL) || (iContentId <= 0) ) return;
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_sitemap_entry SET status=0,update_time=? WHERE content_id=? AND type='content'", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)xrtNow());
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iContentId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
}

bool Managed_SitemapWriteCacheFiles(sqlite3* pDb, xvalue tblSpec);

void Managed_SitemapSyncData(sqlite3* pDb, xvalue tblSpec, int64 iContentId, const char* sTitle, int iStatus, bool bDraft, xvalue tblData, int64 iNow)
{
	xvalue tblItem;

	if ( !Managed_AbilityPackMounted("content.sitemap") || (pDb == NULL) || (tblSpec == NULL) || (tblData == NULL) || (iContentId <= 0) ) return;
	if ( bDraft || (iStatus < Managed_PublicStatusThreshold(tblSpec)) ) {
		Managed_SitemapRemoveEntry(pDb, iContentId);
		Managed_SitemapWriteCacheFiles(pDb, tblSpec);
		return;
	}
	tblItem = xvoCreateTable();
	xvoTableSetInt(tblItem, "id", 2, iContentId);
	xvoTableSetText(tblItem, "title", 5, (str)(sTitle ? sTitle : ""), 0, FALSE);
	xvoTableSetInt(tblItem, "status", 6, iStatus);
	xvoTableSetValue(tblItem, "data", 4, xvoCopy(tblData), TRUE);
	Managed_AppendDerivedFields(tblItem, tblSpec);
	Managed_SitemapUpsertEntry(pDb, tblItem, iNow);
	xvoUnref(tblItem);
	Managed_SitemapWriteCacheFiles(pDb, tblSpec);
}

void Managed_RequestSitemapEntryListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblSpec = Managed_LoadSpec();
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();
	str sSql = NULL;

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.sitemap") ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		if ( tblRet ) xvoUnref(tblRet);
		if ( arrList ) xvoUnref(arrList);
		Managed_SendError(objResp, "sitemap ability pack is not enabled");
		return;
	}
	sSql = xrtFormat("SELECT id,title,status,payload_json,is_draft,create_time,update_time,category_id FROM content_item WHERE delete_time=0 AND is_draft=0 AND status >= %d ORDER BY update_time DESC,id DESC LIMIT 500", Managed_PublicStatusThreshold(tblSpec));
	if ( (sSql == NULL) || !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( sSql ) xrtFree(sSql);
		if ( tblSpec ) xvoUnref(tblSpec);
		if ( tblRet ) xvoUnref(tblRet);
		if ( arrList ) xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblItem = Managed_CreateItemFromStmt(stmt, tblSpec);
			str sLoc = Managed_SitemapItemUrl(tblItem);
			if ( tblItem ) {
				xvoTableSetInt(tblItem, "contentId", 9, xvoTableGetInt(tblItem, "id", 2));
				xvoTableSetText(tblItem, "loc", 3, sLoc ? sLoc : (str)"", 0, FALSE);
				xvoTableSetText(tblItem, "changefreq", 10, "weekly", 0, FALSE);
				xvoTableSetFloat(tblItem, "priority", 8, 0.8);
				xvoArrayAppendValue(arrList, tblItem, TRUE);
			}
			if ( sLoc ) xrtFree(sLoc);
			if ( tblItem ) xvoUnref(tblItem);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	if ( sSql ) xrtFree(sSql);
	if ( tblSpec ) xvoUnref(tblSpec);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestSitemapRefreshAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblSpec = Managed_LoadSpec();
	xvalue tblRet = NULL;
	str sSql = NULL;
	int64 iNow = xrtNow();
	int iTotal = 0;
	int iSaved = 0;

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.sitemap") ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "sitemap ability pack is not enabled");
		return;
	}
	sSql = xrtFormat("SELECT id,title,status,payload_json,is_draft,create_time,update_time,category_id FROM content_item WHERE delete_time=0 AND is_draft=0 AND status >= %d ORDER BY update_time DESC,id DESC LIMIT 5000", Managed_PublicStatusThreshold(tblSpec));
	if ( (sSql == NULL) || !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( sSql ) xrtFree(sSql);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	Managed_ExecSql(pDb, "BEGIN IMMEDIATE");
	Managed_ExecSql(pDb, "UPDATE content_sitemap_entry SET status=0 WHERE type='content'");
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblItem = Managed_CreateItemFromStmt(stmt, tblSpec);
			iTotal++;
			if ( Managed_SitemapUpsertEntry(pDb, tblItem, iNow) ) iSaved++;
			if ( tblItem ) xvoUnref(tblItem);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_ExecSql(pDb, "COMMIT");
	Managed_SitemapWriteCacheFiles(pDb, tblSpec);
	Managed_CloseDb(pDb);
	if ( sSql ) xrtFree(sSql);
	if ( tblSpec ) xvoUnref(tblSpec);
	tblRet = Managed_CreateResult(TRUE, "sitemap refreshed");
	xvoTableSetInt(tblRet, "total", 5, iTotal);
	xvoTableSetInt(tblRet, "saved", 5, iSaved);
	Managed_SendJsonValue(objResp, tblRet);
}

int Managed_SitemapEntryCount(sqlite3* pDb);

void Managed_RequestSitemapStatsAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblData = xvoCreateTable();
	xvalue tblRet = NULL;
	int64 iEnabled = 0;
	int64 iDisabled = 0;
	int64 iLastUpdate = 0;
	int iCacheEntryCount = 0;
	int iCacheTtl = Managed_SitemapCacheTtlSeconds();
	int64 iNow = xrtNow();

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.sitemap") ) {
		xvoUnref(tblData);
		Managed_SendError(objResp, "sitemap ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblData);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT SUM(CASE WHEN status=1 THEN 1 ELSE 0 END),SUM(CASE WHEN status<>1 THEN 1 ELSE 0 END),COALESCE(MAX(update_time),0) FROM content_sitemap_entry", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iEnabled = sqlite3_column_int64(stmt, 0);
			iDisabled = sqlite3_column_int64(stmt, 1);
			iLastUpdate = sqlite3_column_int64(stmt, 2);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	iCacheEntryCount = Managed_SitemapEntryCount(pDb);
	Managed_CloseDb(pDb);
	xvoTableSetInt(tblData, "enabledCount", 12, iEnabled);
	xvoTableSetInt(tblData, "disabledCount", 13, iDisabled);
	xvoTableSetInt(tblData, "totalCount", 10, iEnabled + iDisabled);
	xvoTableSetInt(tblData, "lastUpdate", 10, iLastUpdate);
	Managed_SetTimeText(tblData, "lastUpdateText", 14, iLastUpdate);
	xvoTableSetText(tblData, "cachePolicy", 11, "write-through", 0, FALSE);
	xvoTableSetText(tblData, "cacheFile", 9, "sitemap/cache.json", 0, FALSE);
	xvoTableSetInt(tblData, "cacheEntryCount", 15, iCacheEntryCount);
	xvoTableSetInt(tblData, "cacheTtlSeconds", 15, iCacheTtl);
	xvoTableSetInt(tblData, "cacheExpiresAt", 14, (iLastUpdate > 0 && iCacheTtl > 0) ? (iLastUpdate + iCacheTtl) : 0);
	xvoTableSetBool(tblData, "cacheExpired", 12, (iLastUpdate > 0 && iCacheTtl > 0 && iNow > iLastUpdate + iCacheTtl) ? TRUE : FALSE);
	tblRet = Managed_CreateResult(TRUE, NULL);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

int Managed_SitemapEntryCount(sqlite3* pDb)
{
	sqlite3_stmt* stmt = NULL;
	int iCount = 0;

	if ( pDb == NULL ) return 0;
	if ( sqlite3_prepare_v2(pDb, "SELECT COUNT(*) FROM content_sitemap_entry WHERE status=1 AND type='content' AND loc<>''", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) iCount = sqlite3_column_int(stmt, 0);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return iCount;
}

int Managed_AppendCachedSitemapXmlPage(sqlite3* pDb, str* psXml, int iLimit, int iOffset)
{
	sqlite3_stmt* stmt = NULL;
	int iCount = 0;

	if ( (pDb == NULL) || (psXml == NULL) || (iLimit <= 0) ) return 0;
	if ( iOffset < 0 ) iOffset = 0;
	if ( sqlite3_prepare_v2(pDb, "SELECT loc,changefreq,priority FROM content_sitemap_entry WHERE status=1 AND type='content' AND loc<>'' ORDER BY update_time DESC,id DESC LIMIT ? OFFSET ?", -1, &stmt, NULL) != SQLITE_OK ) {
		return 0;
	}
	sqlite3_bind_int(stmt, 1, iLimit);
	sqlite3_bind_int(stmt, 2, iOffset);
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		const char* sLoc = (const char*)sqlite3_column_text(stmt, 0);
		const char* sChangefreq = (const char*)sqlite3_column_text(stmt, 1);
		double dPriority = sqlite3_column_double(stmt, 2);
		str sAbsLoc = Managed_SitemapAbsoluteUrl(sLoc ? sLoc : "");
		str sEscLoc = Managed_XmlEscape(sAbsLoc ? (const char*)sAbsLoc : "");
		str sEscChangefreq = Managed_XmlEscape(Managed_IsBlank(sChangefreq) ? "weekly" : sChangefreq);
		str sNode = xrtFormat("  <url><loc>%s</loc><changefreq>%s</changefreq><priority>%.1f</priority></url>\n", sEscLoc ? (const char*)sEscLoc : "", sEscChangefreq ? (const char*)sEscChangefreq : "weekly", dPriority > 0 ? dPriority : 0.8);
		*psXml = Managed_AppendOwnedText(*psXml, sNode);
		if ( sNode ) xrtFree(sNode);
		if ( sEscChangefreq ) xrtFree(sEscChangefreq);
		if ( sEscLoc ) xrtFree(sEscLoc);
		if ( sAbsLoc ) xrtFree(sAbsLoc);
		iCount++;
	}
	sqlite3_finalize(stmt);
	return iCount;
}

int Managed_AppendCachedSitemapXml(sqlite3* pDb, str* psXml, int iLimit)
{
	return Managed_AppendCachedSitemapXmlPage(pDb, psXml, iLimit, 0);
}

int Managed_AppendCachedRssXml(sqlite3* pDb, str* psXml, int iLimit)
{
	sqlite3_stmt* stmt = NULL;
	int iCount = 0;

	if ( (pDb == NULL) || (psXml == NULL) || (iLimit <= 0) ) return 0;
	if ( sqlite3_prepare_v2(pDb, "SELECT loc,title,update_time FROM content_sitemap_entry WHERE status=1 AND type='content' AND loc<>'' ORDER BY update_time DESC,id DESC LIMIT ?", -1, &stmt, NULL) != SQLITE_OK ) {
		return 0;
	}
	sqlite3_bind_int(stmt, 1, iLimit);
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		const char* sLoc = (const char*)sqlite3_column_text(stmt, 0);
		const char* sTitle = (const char*)sqlite3_column_text(stmt, 1);
		str sAbsLoc = Managed_SitemapAbsoluteUrl(sLoc ? sLoc : "");
		str sEscLoc = Managed_XmlEscape(sAbsLoc ? (const char*)sAbsLoc : "");
		str sEscTitle = Managed_XmlEscape(Managed_IsBlank(sTitle) ? sLoc : sTitle);
		str sNode = xrtFormat("<item><title>%s</title><link>%s</link><description>%s</description></item>\n", sEscTitle ? (const char*)sEscTitle : "", sEscLoc ? (const char*)sEscLoc : "", sEscTitle ? (const char*)sEscTitle : "");
		*psXml = Managed_AppendOwnedText(*psXml, sNode);
		if ( sNode ) xrtFree(sNode);
		if ( sEscTitle ) xrtFree(sEscTitle);
		if ( sEscLoc ) xrtFree(sEscLoc);
		if ( sAbsLoc ) xrtFree(sAbsLoc);
		iCount++;
	}
	sqlite3_finalize(stmt);
	return iCount;
}

str Managed_BuildSitemapXmlFromDb(sqlite3* pDb)
{
	str sXml = xrtCopyStr("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n", 0);

	Managed_AppendCachedSitemapXml(pDb, &sXml, 5000);
	sXml = Managed_AppendOwnedText(sXml, "</urlset>\n");
	return sXml;
}

str Managed_BuildSitemapIndexXmlFromDb(sqlite3* pDb)
{
	int iPageSize = 5000;
	int iTotal = Managed_SitemapEntryCount(pDb);
	int iPages = (iTotal + iPageSize - 1) / iPageSize;
	int i;
	str sXml = xrtCopyStr("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<sitemapindex xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n", 0);

	if ( iPages < 1 ) iPages = 1;
	for ( i = 1; i <= iPages; i++ ) {
		str sRel = xrtFormat("/api/plugin/{{PLUGIN_XID}}/sitemap.xml?page=%d", i);
		str sAbs = Managed_SitemapAbsoluteUrl(sRel ? (const char*)sRel : "");
		str sEsc = Managed_XmlEscape(sAbs ? (const char*)sAbs : "");
		str sNode = xrtFormat("  <sitemap><loc>%s</loc></sitemap>\n", sEsc ? (const char*)sEsc : "");
		sXml = Managed_AppendOwnedText(sXml, sNode);
		if ( sNode ) xrtFree(sNode);
		if ( sEsc ) xrtFree(sEsc);
		if ( sAbs ) xrtFree(sAbs);
		if ( sRel ) xrtFree(sRel);
	}
	sXml = Managed_AppendOwnedText(sXml, "</sitemapindex>\n");
	return sXml;
}

str Managed_BuildRssXmlFromDb(sqlite3* pDb)
{
	str sHomeUrl = Managed_SitemapAbsoluteUrl("/plugin/{{PLUGIN_XID}}");
	str sEscHomeUrl = Managed_XmlEscape(sHomeUrl ? (const char*)sHomeUrl : "/plugin/{{PLUGIN_XID}}");
	str sXml = xrtFormat("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rss version=\"2.0\"><channel><title>{{PLUGIN_XID}}</title><link>%s</link><description>{{PLUGIN_XID}} feed</description>\n", sEscHomeUrl ? (const char*)sEscHomeUrl : "/plugin/{{PLUGIN_XID}}");

	Managed_AppendCachedRssXml(pDb, &sXml, 100);
	sXml = Managed_AppendOwnedText(sXml, "</channel></rss>\n");
	if ( sEscHomeUrl ) xrtFree(sEscHomeUrl);
	if ( sHomeUrl ) xrtFree(sHomeUrl);
	return sXml;
}

str Managed_BuildRobotsTxt()
{
	str sSitemapUrl = Managed_SitemapAbsoluteUrl("/api/plugin/{{PLUGIN_XID}}/sitemap.xml");
	str sText = xrtFormat("User-agent: *\nAllow: /\nSitemap: %s\n", sSitemapUrl ? (const char*)sSitemapUrl : "/api/plugin/{{PLUGIN_XID}}/sitemap.xml");

	if ( sSitemapUrl ) xrtFree(sSitemapUrl);
	return sText;
}

bool Managed_SitemapWriteCacheFile(const char* sRelPath, const char* sContent)
{
	str sFilePath = NULL;
	str sDirPath = NULL;
	bool bOK = FALSE;

	if ( Managed_IsBlank(sRelPath) || (sContent == NULL) || (G_Handle == NULL) ) return FALSE;
	sFilePath = XAdmin_PluginResourcePath(G_Handle, "static", (str)sRelPath);
	if ( sFilePath == NULL ) return FALSE;
	sDirPath = xrtPathGetDir(sFilePath, 0);
	if ( sDirPath ) {
		xrtDirCreateAll(sDirPath);
		xrtFree(sDirPath);
	}
	bOK = xrtFilePutAll(sFilePath, (ptr)sContent, (uint32)strlen(sContent)) >= 0;
	xrtFree(sFilePath);
	return bOK;
}

bool Managed_SitemapWriteCacheFiles(sqlite3* pDb, xvalue tblSpec)
{
	str sXml = NULL;
	str sIndexXml = NULL;
	str sRss = NULL;
	str sRobots = NULL;
	str sCacheMeta = NULL;
	bool bOK = FALSE;
	int iEntryCount = 0;
	int64 iGeneratedAt = xrtNow();
	int iCacheTtl = Managed_SitemapCacheTtlSeconds();

	(void)tblSpec;
	if ( !Managed_AbilityPackMounted("content.sitemap") || (pDb == NULL) ) return FALSE;
	iEntryCount = Managed_SitemapEntryCount(pDb);
	sXml = Managed_BuildSitemapXmlFromDb(pDb);
	sIndexXml = Managed_BuildSitemapIndexXmlFromDb(pDb);
	sRss = Managed_BuildRssXmlFromDb(pDb);
	sRobots = Managed_BuildRobotsTxt();
	sCacheMeta = xrtFormat("{\"generatedAt\":%lld,\"entryCount\":%d,\"policy\":\"write-through\",\"ttlSeconds\":%d,\"expiresAt\":%lld}\n", (long long)iGeneratedAt, iEntryCount, iCacheTtl, (long long)((iCacheTtl > 0) ? (iGeneratedAt + iCacheTtl) : 0));
	bOK = Managed_SitemapWriteCacheFile("sitemap/sitemap.xml", sXml ? (const char*)sXml : "")
		&& Managed_SitemapWriteCacheFile("sitemap/sitemap-index.xml", sIndexXml ? (const char*)sIndexXml : "")
		&& Managed_SitemapWriteCacheFile("sitemap/rss.xml", sRss ? (const char*)sRss : "")
		&& Managed_SitemapWriteCacheFile("sitemap/robots.txt", sRobots ? (const char*)sRobots : "")
		&& Managed_SitemapWriteCacheFile("sitemap/cache.json", sCacheMeta ? (const char*)sCacheMeta : "{}\n");
	if ( sCacheMeta ) xrtFree(sCacheMeta);
	if ( sRobots ) xrtFree(sRobots);
	if ( sRss ) xrtFree(sRss);
	if ( sIndexXml ) xrtFree(sIndexXml);
	if ( sXml ) xrtFree(sXml);
	return bOK;
}

bool Managed_SitemapReplyCache(XS_ResponseObject objResp, const char* sRelPath, const char* sContentType)
{
	str sFilePath = NULL;
	ptr pData = NULL;
	size_t iSize = 0;

	if ( (objResp == NULL) || Managed_IsBlank(sRelPath) || (G_Handle == NULL) ) return FALSE;
	sFilePath = XAdmin_PluginResourcePath(G_Handle, "static", (str)sRelPath);
	if ( (sFilePath == NULL) || !xrtFileExists(sFilePath) ) {
		if ( sFilePath ) xrtFree(sFilePath);
		return FALSE;
	}
	pData = xrtFileGetAll(sFilePath, &iSize);
	xrtFree(sFilePath);
	if ( pData == NULL ) return FALSE;
	xsHttpReplyAuto(objResp, 200, sContentType ? sContentType : "Content-Type: text/plain; charset=utf-8\r\n", pData, iSize);
	xrtFree(pData);
	return TRUE;
}

void Managed_RequestSitemapXmlPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblSpec = Managed_LoadSpec();
	str sSql = NULL;
	str sXml = xrtCopyStr("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n", 0);
	int iCached = 0;
	int iPage = Managed_ReadIntQuery(objReq, "page", 1);
	int iPageSize = 5000;
	int iOffset = 0;

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( iPage < 1 ) iPage = 1;
	iOffset = (iPage - 1) * iPageSize;
	if ( !Managed_AbilityPackMounted("content.sitemap") ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		if ( sXml ) xrtFree(sXml);
		xsHttpReplyAuto(objResp, 404, "Content-Type: text/plain; charset=utf-8\r\n", "sitemap ability pack is not enabled", 0);
		return;
	}
	if ( (iPage == 1) && Managed_SitemapReplyCache(objResp, "sitemap/sitemap.xml", "Content-Type: application/xml; charset=utf-8\r\n") ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		if ( sXml ) xrtFree(sXml);
		return;
	}
	sSql = xrtFormat("SELECT id,title,status,payload_json,is_draft,create_time,update_time,category_id FROM content_item WHERE delete_time=0 AND is_draft=0 AND status >= %d ORDER BY update_time DESC,id DESC LIMIT %d OFFSET %d", Managed_PublicStatusThreshold(tblSpec), iPageSize, iOffset);
	if ( Managed_EnsureSchema() && Managed_OpenDb(&pDb) ) {
		iCached = Managed_AppendCachedSitemapXmlPage(pDb, &sXml, iPageSize, iOffset);
	}
	if ( (iCached <= 0) && (pDb != NULL) && (sSql != NULL) && (sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK) ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblItem = Managed_CreateItemFromStmt(stmt, tblSpec);
			str sLoc = Managed_SitemapItemUrl(tblItem);
			str sEscLoc = Managed_XmlEscape(sLoc ? (const char*)sLoc : "");
			str sNode = xrtFormat("  <url><loc>%s</loc><changefreq>weekly</changefreq><priority>0.8</priority></url>\n", sEscLoc ? (const char*)sEscLoc : "");
			sXml = Managed_AppendOwnedText(sXml, sNode);
			if ( sNode ) xrtFree(sNode);
			if ( sEscLoc ) xrtFree(sEscLoc);
			if ( sLoc ) xrtFree(sLoc);
			if ( tblItem ) xvoUnref(tblItem);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	if ( pDb ) Managed_CloseDb(pDb);
	if ( sSql ) xrtFree(sSql);
	if ( tblSpec ) xvoUnref(tblSpec);
	sXml = Managed_AppendOwnedText(sXml, "</urlset>\n");
	xsHttpReplyAuto(objResp, 200, "Content-Type: application/xml; charset=utf-8\r\n", sXml ? (const char*)sXml : "", sXml ? strlen((const char*)sXml) : 0);
	if ( sXml ) xrtFree(sXml);
}

void Managed_RequestSitemapIndexPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	str sXml = NULL;

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.sitemap") ) {
		xsHttpReplyAuto(objResp, 404, "Content-Type: text/plain; charset=utf-8\r\n", "sitemap ability pack is not enabled", 0);
		return;
	}
	if ( Managed_SitemapReplyCache(objResp, "sitemap/sitemap-index.xml", "Content-Type: application/xml; charset=utf-8\r\n") ) {
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "failed to open plugin database", 0);
		return;
	}
	sXml = Managed_BuildSitemapIndexXmlFromDb(pDb);
	Managed_CloseDb(pDb);
	xsHttpReplyAuto(objResp, 200, "Content-Type: application/xml; charset=utf-8\r\n", sXml ? (const char*)sXml : "", sXml ? strlen((const char*)sXml) : 0);
	if ( sXml ) xrtFree(sXml);
}

void Managed_RequestRssXmlPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblSpec = Managed_LoadSpec();
	str sSql = NULL;
	str sHomeUrl = Managed_SitemapAbsoluteUrl("/plugin/{{PLUGIN_XID}}");
	str sEscHomeUrl = Managed_XmlEscape(sHomeUrl ? (const char*)sHomeUrl : "/plugin/{{PLUGIN_XID}}");
	str sXml = xrtFormat("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<rss version=\"2.0\"><channel><title>{{PLUGIN_XID}}</title><link>%s</link><description>{{PLUGIN_XID}} feed</description>\n", sEscHomeUrl ? (const char*)sEscHomeUrl : "/plugin/{{PLUGIN_XID}}");
	int iCached = 0;

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.sitemap") ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		if ( sEscHomeUrl ) xrtFree(sEscHomeUrl);
		if ( sHomeUrl ) xrtFree(sHomeUrl);
		if ( sXml ) xrtFree(sXml);
		xsHttpReplyAuto(objResp, 404, "Content-Type: text/plain; charset=utf-8\r\n", "sitemap ability pack is not enabled", 0);
		return;
	}
	if ( Managed_SitemapReplyCache(objResp, "sitemap/rss.xml", "Content-Type: application/rss+xml; charset=utf-8\r\n") ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		if ( sEscHomeUrl ) xrtFree(sEscHomeUrl);
		if ( sHomeUrl ) xrtFree(sHomeUrl);
		if ( sXml ) xrtFree(sXml);
		return;
	}
	sSql = xrtFormat("SELECT id,title,status,payload_json,is_draft,create_time,update_time,category_id FROM content_item WHERE delete_time=0 AND is_draft=0 AND status >= %d ORDER BY update_time DESC,id DESC LIMIT 100", Managed_PublicStatusThreshold(tblSpec));
	if ( Managed_EnsureSchema() && Managed_OpenDb(&pDb) ) {
		iCached = Managed_AppendCachedRssXml(pDb, &sXml, 100);
	}
	if ( (iCached <= 0) && (pDb != NULL) && (sSql != NULL) && (sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK) ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblItem = Managed_CreateItemFromStmt(stmt, tblSpec);
			str sLoc = Managed_SitemapItemUrl(tblItem);
			str sEscLoc = Managed_XmlEscape(sLoc ? (const char*)sLoc : "");
			str sEscTitle = Managed_XmlEscape(tblItem ? xvoTableGetText(tblItem, "title", 5) : (str)"");
			str sEscSummary = Managed_XmlEscape(tblItem ? xvoTableGetText(tblItem, "summary", 7) : (str)"");
			str sNode = xrtFormat("<item><title>%s</title><link>%s</link><description>%s</description></item>\n", sEscTitle ? (const char*)sEscTitle : "", sEscLoc ? (const char*)sEscLoc : "", sEscSummary ? (const char*)sEscSummary : "");
			sXml = Managed_AppendOwnedText(sXml, sNode);
			if ( sNode ) xrtFree(sNode);
			if ( sEscSummary ) xrtFree(sEscSummary);
			if ( sEscTitle ) xrtFree(sEscTitle);
			if ( sEscLoc ) xrtFree(sEscLoc);
			if ( sLoc ) xrtFree(sLoc);
			if ( tblItem ) xvoUnref(tblItem);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	if ( pDb ) Managed_CloseDb(pDb);
	if ( sSql ) xrtFree(sSql);
	if ( tblSpec ) xvoUnref(tblSpec);
	if ( sEscHomeUrl ) xrtFree(sEscHomeUrl);
	if ( sHomeUrl ) xrtFree(sHomeUrl);
	sXml = Managed_AppendOwnedText(sXml, "</channel></rss>\n");
	xsHttpReplyAuto(objResp, 200, "Content-Type: application/rss+xml; charset=utf-8\r\n", sXml ? (const char*)sXml : "", sXml ? strlen((const char*)sXml) : 0);
	if ( sXml ) xrtFree(sXml);
}

void Managed_RequestRobotsTxtPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	str sSitemapUrl = Managed_SitemapAbsoluteUrl("/api/plugin/{{PLUGIN_XID}}/sitemap.xml");
	str sText = xrtFormat("User-agent: *\nAllow: /\nSitemap: %s\n", sSitemapUrl ? (const char*)sSitemapUrl : "/api/plugin/{{PLUGIN_XID}}/sitemap.xml");

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.sitemap") ) {
		if ( sText ) xrtFree(sText);
		if ( sSitemapUrl ) xrtFree(sSitemapUrl);
		xsHttpReplyAuto(objResp, 404, "Content-Type: text/plain; charset=utf-8\r\n", "sitemap ability pack is not enabled", 0);
		return;
	}
	if ( Managed_SitemapReplyCache(objResp, "sitemap/robots.txt", "Content-Type: text/plain; charset=utf-8\r\n") ) {
		if ( sText ) xrtFree(sText);
		if ( sSitemapUrl ) xrtFree(sSitemapUrl);
		return;
	}
	xsHttpReplyAuto(objResp, 200, "Content-Type: text/plain; charset=utf-8\r\n", sText ? (const char*)sText : "", sText ? strlen((const char*)sText) : 0);
	if ( sText ) xrtFree(sText);
	if ( sSitemapUrl ) xrtFree(sSitemapUrl);
}

void Managed_AppendRelatedAdminRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();

	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblRow, "sourceContentId", 15, sqlite3_column_int64(stmt, 1));
	xvoTableSetText(tblRow, "sourceTitle", 11, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetInt(tblRow, "relatedContentId", 16, sqlite3_column_int64(stmt, 3));
	xvoTableSetText(tblRow, "relatedTitle", 12, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetText(tblRow, "relationType", 12, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
	xvoTableSetInt(tblRow, "weight", 6, sqlite3_column_int(stmt, 6));
	xvoTableSetInt(tblRow, "status", 6, sqlite3_column_int(stmt, 7));
	Managed_SetTimeText(tblRow, "updateTimeText", 14, sqlite3_column_int64(stmt, 8));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_RequestRelatedListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.related") ) {
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "related ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT r.id,r.source_content_id,src.title,r.related_content_id,dst.title,r.relation_type,r.weight,r.status,r.update_time FROM content_related r LEFT JOIN content_item src ON src.id=r.source_content_id LEFT JOIN content_item dst ON dst.id=r.related_content_id WHERE r.delete_time=0 ORDER BY r.update_time DESC,r.id DESC LIMIT 500", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendRelatedAdminRow(arrList, stmt);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestRelatedSaveAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	int64 iSourceId = tblBody ? xvoTableGetInt(tblBody, "sourceContentId", 15) : 0;
	int64 iRelatedId = tblBody ? xvoTableGetInt(tblBody, "relatedContentId", 16) : 0;
	const char* sType = tblBody ? xvoTableGetText(tblBody, "relationType", 12) : NULL;
	int iWeight = tblBody ? (int)xvoTableGetInt(tblBody, "weight", 6) : 0;
	int iStatus = tblBody ? (int)xvoTableGetInt(tblBody, "status", 6) : 1;
	int64 iNow = xrtNow();

	(void)objServer; (void)objHost; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.related") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "related ability pack is not enabled");
		return;
	}
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) || (tblBody == NULL) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "invalid request body");
		return;
	}
	if ( (iSourceId <= 0) || (iRelatedId <= 0) || (iSourceId == iRelatedId) ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "sourceContentId and relatedContentId are required");
		return;
	}
	if ( Managed_IsBlank(sType) ) sType = "manual";
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( iId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE content_related SET source_content_id=?,related_content_id=?,relation_type=?,weight=?,status=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iSourceId);
			sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iRelatedId);
			sqlite3_bind_text(stmt, 3, sType, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 4, iWeight);
			sqlite3_bind_int(stmt, 5, iStatus);
			sqlite3_bind_int64(stmt, 6, (sqlite3_int64)iNow);
			sqlite3_bind_int64(stmt, 7, (sqlite3_int64)iId);
			sqlite3_step(stmt);
		}
	} else {
		if ( sqlite3_prepare_v2(pDb, "INSERT OR REPLACE INTO content_related(source_content_id,related_content_id,relation_type,weight,status,create_time,update_time,delete_time) VALUES(?,?,?,?,?,?,?,0)", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iSourceId);
			sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iRelatedId);
			sqlite3_bind_text(stmt, 3, sType, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 4, iWeight);
			sqlite3_bind_int(stmt, 5, iStatus);
			sqlite3_bind_int64(stmt, 6, (sqlite3_int64)iNow);
			sqlite3_bind_int64(stmt, 7, (sqlite3_int64)iNow);
			sqlite3_step(stmt);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_AuditLogWithRequest(pDb, "related", iId > 0 ? iId : sqlite3_last_insert_rowid(pDb), iId > 0 ? "related.update" : "related.create", sType ? sType : "manual", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "saved"));
}

void Managed_RequestRelatedDeleteAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	int64 iNow = xrtNow();

	(void)objServer; (void)objHost; (void)objReq;
	if ( !Managed_AbilityPackMounted("content.related") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "related ability pack is not enabled");
		return;
	}
	if ( iId <= 0 ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "id is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_related SET delete_time=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_AuditLogWithRequest(pDb, "related", iId, "related.delete", "delete related content", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	if ( tblBody ) xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "deleted"));
}

void Managed_RelatedRebuildOne(sqlite3* pDb, int64 iSourceId, int iCategoryId, int iLimit, int64 iNow, int* pCreated)
{
	sqlite3_stmt* stmt = NULL;
	sqlite3_stmt* stmtIns = NULL;
	int iRank = 0;

	if ( (pDb == NULL) || (iSourceId <= 0) || (iCategoryId <= 0) || (iLimit <= 0) ) return;
	if ( sqlite3_prepare_v2(pDb, "SELECT id FROM content_item WHERE category_id=? AND id<>? AND delete_time=0 AND is_draft=0 AND status>=1 AND NOT EXISTS (SELECT 1 FROM content_related m WHERE m.source_content_id=? AND m.related_content_id=content_item.id AND m.relation_type='manual' AND m.status=1 AND m.delete_time=0) ORDER BY update_time DESC,id DESC LIMIT ?", -1, &stmt, NULL) != SQLITE_OK ) return;
	sqlite3_bind_int(stmt, 1, iCategoryId);
	sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iSourceId);
	sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iSourceId);
	sqlite3_bind_int(stmt, 4, iLimit);
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		int64 iRelatedId = sqlite3_column_int64(stmt, 0);
		int iWeight = (iLimit - iRank) * 10;
		iRank++;
		if ( sqlite3_prepare_v2(pDb, "INSERT OR REPLACE INTO content_related(source_content_id,related_content_id,relation_type,weight,status,create_time,update_time,delete_time) VALUES(?,?,'rule',?,?,?, ?,0)", -1, &stmtIns, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmtIns, 1, (sqlite3_int64)iSourceId);
			sqlite3_bind_int64(stmtIns, 2, (sqlite3_int64)iRelatedId);
			sqlite3_bind_int(stmtIns, 3, iWeight);
			sqlite3_bind_int(stmtIns, 4, 1);
			sqlite3_bind_int64(stmtIns, 5, (sqlite3_int64)iNow);
			sqlite3_bind_int64(stmtIns, 6, (sqlite3_int64)iNow);
			if ( sqlite3_step(stmtIns) == SQLITE_DONE ) {
				if ( pCreated ) (*pCreated)++;
			}
		}
		if ( stmtIns ) sqlite3_finalize(stmtIns);
		stmtIns = NULL;
	}
	if ( stmt ) sqlite3_finalize(stmt);
}

void Managed_RelatedSyncPeers(sqlite3* pDb, int64 iContentId, int iCategoryId, int iLimit, int64 iNow, int* pCreated)
{
	sqlite3_stmt* stmt = NULL;
	sqlite3_stmt* stmtIns = NULL;
	int iRank = 0;

	if ( (pDb == NULL) || (iContentId <= 0) || (iCategoryId <= 0) || (iLimit <= 0) ) return;
	/* Keep save-time fan-out bounded: only recent same-category peers get a reverse rule link. */
	if ( sqlite3_prepare_v2(pDb, "SELECT id FROM content_item WHERE category_id=? AND id<>? AND delete_time=0 AND is_draft=0 AND status>=1 AND NOT EXISTS (SELECT 1 FROM content_related m WHERE m.source_content_id=content_item.id AND m.related_content_id=? AND m.relation_type='manual' AND m.status=1 AND m.delete_time=0) ORDER BY update_time DESC,id DESC LIMIT ?", -1, &stmt, NULL) != SQLITE_OK ) return;
	sqlite3_bind_int(stmt, 1, iCategoryId);
	sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iContentId);
	sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iContentId);
	sqlite3_bind_int(stmt, 4, iLimit);
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		int64 iPeerId = sqlite3_column_int64(stmt, 0);
		int iWeight = (iLimit - iRank) * 10;
		iRank++;
		if ( sqlite3_prepare_v2(pDb, "INSERT OR REPLACE INTO content_related(source_content_id,related_content_id,relation_type,weight,status,create_time,update_time,delete_time) VALUES(?,?,'rule',?,?,?, ?,0)", -1, &stmtIns, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmtIns, 1, (sqlite3_int64)iPeerId);
			sqlite3_bind_int64(stmtIns, 2, (sqlite3_int64)iContentId);
			sqlite3_bind_int(stmtIns, 3, iWeight);
			sqlite3_bind_int(stmtIns, 4, 1);
			sqlite3_bind_int64(stmtIns, 5, (sqlite3_int64)iNow);
			sqlite3_bind_int64(stmtIns, 6, (sqlite3_int64)iNow);
			if ( sqlite3_step(stmtIns) == SQLITE_DONE ) {
				if ( pCreated ) (*pCreated)++;
			}
		}
		if ( stmtIns ) sqlite3_finalize(stmtIns);
		stmtIns = NULL;
	}
	if ( stmt ) sqlite3_finalize(stmt);
}

void Managed_RelatedSyncData(sqlite3* pDb, xvalue tblSpec, int64 iContentId, int iCategoryId, int iStatus, bool bDraft, int64 iNow)
{
	sqlite3_stmt* stmt = NULL;
	int iCreated = 0;
	int iLimit = Managed_AbilityPackConfigInt("content.related", "ruleLimit", 5);

	if ( !Managed_AbilityPackMounted("content.related") || (pDb == NULL) || (tblSpec == NULL) || (iContentId <= 0) ) return;
	if ( iLimit <= 0 ) iLimit = 5;
	if ( iLimit > 20 ) iLimit = 20;

	if ( sqlite3_prepare_v2(pDb, "UPDATE content_related SET delete_time=?,update_time=? WHERE source_content_id=? AND relation_type='rule' AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iContentId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( bDraft || (iStatus < Managed_PublicStatusThreshold(tblSpec)) || (iCategoryId <= 0) ) return;
	Managed_RelatedRebuildOne(pDb, iContentId, iCategoryId, iLimit, iNow, &iCreated);
	Managed_RelatedSyncPeers(pDb, iContentId, iCategoryId, iLimit, iNow, &iCreated);
}

void Managed_RequestRelatedRebuildAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	int64 iContentId = tblBody ? xvoTableGetInt(tblBody, "contentId", 9) : 0;
	int iLimit = tblBody ? (int)xvoTableGetInt(tblBody, "limit", 5) : 5;
	int64 iNow = xrtNow();
	int iSources = 0;
	int iCreated = 0;

	(void)objServer; (void)objHost;
	if ( !Managed_AbilityPackMounted("content.related") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "related ability pack is not enabled");
		return;
	}
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "method not allowed");
		return;
	}
	if ( iLimit <= 0 ) iLimit = 5;
	if ( iLimit > 20 ) iLimit = 20;
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	Managed_ExecSql(pDb, "BEGIN IMMEDIATE");
	if ( iContentId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE content_related SET delete_time=?,update_time=? WHERE source_content_id=? AND relation_type='rule' AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iNow);
			sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iNow);
			sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iContentId);
			sqlite3_step(stmt);
		}
		if ( stmt ) sqlite3_finalize(stmt);
		stmt = NULL;
		if ( sqlite3_prepare_v2(pDb, "SELECT id,category_id FROM content_item WHERE id=? AND delete_time=0 AND is_draft=0 AND status>=1 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
			if ( sqlite3_step(stmt) == SQLITE_ROW ) {
				iSources++;
				Managed_RelatedRebuildOne(pDb, sqlite3_column_int64(stmt, 0), sqlite3_column_int(stmt, 1), iLimit, iNow, &iCreated);
			}
		}
	} else {
		Managed_ExecSql(pDb, "UPDATE content_related SET delete_time=strftime('%s','now') WHERE relation_type='rule' AND delete_time=0");
		if ( sqlite3_prepare_v2(pDb, "SELECT id,category_id FROM content_item WHERE delete_time=0 AND is_draft=0 AND status>=1 AND category_id>0 ORDER BY update_time DESC,id DESC LIMIT 1000", -1, &stmt, NULL) == SQLITE_OK ) {
			while ( sqlite3_step(stmt) == SQLITE_ROW ) {
				iSources++;
				Managed_RelatedRebuildOne(pDb, sqlite3_column_int64(stmt, 0), sqlite3_column_int(stmt, 1), iLimit, iNow, &iCreated);
			}
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_ExecSql(pDb, "COMMIT");
	Managed_AuditLogWithRequest(pDb, "related", iContentId, "related.rebuild", "rebuild related rules", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	if ( tblBody ) xvoUnref(tblBody);
	tblRet = Managed_CreateResult(TRUE, "related rules rebuilt");
	xvoTableSetInt(tblRet, "sources", 7, iSources);
	xvoTableSetInt(tblRet, "created", 7, iCreated);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestRelatedListPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sContentId[32];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblSpec = Managed_LoadSpec();
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();
	int64 iContentId;

	(void)objServer; (void)objHost; (void)objSession;
	memset(sContentId, 0, sizeof(sContentId));
	xsReqQueryValue(objReq, "contentId", sContentId, sizeof(sContentId));
	if ( sContentId[0] == '\0' ) xsReqQueryValue(objReq, "id", sContentId, sizeof(sContentId));
	iContentId = atoll(sContentId);
	if ( !Managed_AbilityPackMounted("content.related") ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "related ability pack is not enabled");
		return;
	}
	if ( iContentId <= 0 ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "contentId is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT i.id,i.title,i.status,i.payload_json,i.is_draft,i.create_time,i.update_time,i.category_id,r.relation_type,r.weight FROM content_related r INNER JOIN content_item i ON i.id=r.related_content_id WHERE r.source_content_id=? AND r.status=1 AND r.delete_time=0 AND i.delete_time=0 AND i.is_draft=0 AND i.status>=? ORDER BY r.weight DESC,r.update_time DESC,r.id DESC LIMIT 20", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_int(stmt, 2, Managed_PublicStatusThreshold(tblSpec));
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblItem = Managed_CreateItemFromStmt(stmt, tblSpec);
			xvoTableSetText(tblItem, "relationType", 12, (str)sqlite3_column_text(stmt, 8), 0, FALSE);
			xvoTableSetInt(tblItem, "weight", 6, sqlite3_column_int(stmt, 9));
			xvoArrayAppendValue(arrList, tblItem, TRUE);
			xvoUnref(tblItem);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	if ( tblSpec ) xvoUnref(tblSpec);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_AppendFormRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();

	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblRow, "contentId", 9, sqlite3_column_int64(stmt, 1));
	xvoTableSetText(tblRow, "formKey", 7, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetText(tblRow, "title", 5, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetText(tblRow, "schemaJson", 10, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetInt(tblRow, "status", 6, sqlite3_column_int(stmt, 5));
	Managed_SetTimeText(tblRow, "updateTimeText", 14, sqlite3_column_int64(stmt, 6));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_AppendFormSubmissionRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();

	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblRow, "formId", 6, sqlite3_column_int64(stmt, 1));
	xvoTableSetText(tblRow, "formKey", 7, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetText(tblRow, "formTitle", 9, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetInt(tblRow, "contentId", 9, sqlite3_column_int64(stmt, 4));
	xvoTableSetText(tblRow, "dataJson", 8, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
	xvoTableSetText(tblRow, "ip", 2, (str)sqlite3_column_text(stmt, 6), 0, FALSE);
	xvoTableSetInt(tblRow, "status", 6, sqlite3_column_int(stmt, 7));
	Managed_SetTimeText(tblRow, "createTimeText", 14, sqlite3_column_int64(stmt, 8));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_RequestFormListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.form") ) {
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "form ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id,content_id,form_key,title,schema_json,status,update_time FROM content_form WHERE delete_time=0 ORDER BY update_time DESC,id DESC LIMIT 500", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendFormRow(arrList, stmt);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestFormSaveAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	bool bInsert = iId <= 0;
	int64 iContentId = tblBody ? xvoTableGetInt(tblBody, "contentId", 9) : 0;
	const char* sFormKey = tblBody ? xvoTableGetText(tblBody, "formKey", 7) : NULL;
	const char* sTitle = tblBody ? xvoTableGetText(tblBody, "title", 5) : NULL;
	const char* sSchemaJson = tblBody ? xvoTableGetText(tblBody, "schemaJson", 10) : NULL;
	int iStatus = tblBody ? (int)xvoTableGetInt(tblBody, "status", 6) : 1;
	int64 iNow = xrtNow();

	(void)objServer; (void)objHost; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.form") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "form ability pack is not enabled");
		return;
	}
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) || (tblBody == NULL) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "invalid request body");
		return;
	}
	if ( Managed_IsBlank(sFormKey) || Managed_IsBlank(sTitle) ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "formKey and title are required");
		return;
	}
	if ( Managed_IsBlank(sSchemaJson) ) sSchemaJson = "{}";
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( iId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE content_form SET content_id=?,form_key=?,title=?,schema_json=?,status=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
			sqlite3_bind_text(stmt, 2, sFormKey, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sTitle, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, sSchemaJson, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 5, iStatus);
			sqlite3_bind_int64(stmt, 6, (sqlite3_int64)iNow);
			sqlite3_bind_int64(stmt, 7, (sqlite3_int64)iId);
			sqlite3_step(stmt);
		}
	} else {
		if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_form(content_id,form_key,title,schema_json,status,create_time,update_time,delete_time) VALUES(?,?,?,?,?,?,?,0)", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
			sqlite3_bind_text(stmt, 2, sFormKey, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sTitle, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, sSchemaJson, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 5, iStatus);
			sqlite3_bind_int64(stmt, 6, (sqlite3_int64)iNow);
			sqlite3_bind_int64(stmt, 7, (sqlite3_int64)iNow);
			sqlite3_step(stmt);
			iId = sqlite3_last_insert_rowid(pDb);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_AuditLogWithRequest(pDb, "form", iId, bInsert ? "form.create" : "form.update", sTitle ? sTitle : "", sSchemaJson ? sSchemaJson : "{}", objReq, objSession);
	Managed_CloseDb(pDb);
	xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(iId > 0, iId > 0 ? "saved" : "save failed"));
}

void Managed_RequestFormDeleteAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	int64 iNow = xrtNow();

	(void)objServer; (void)objHost; (void)objReq;
	if ( !Managed_AbilityPackMounted("content.form") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "form ability pack is not enabled");
		return;
	}
	if ( iId <= 0 ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "id is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_form SET delete_time=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_AuditLogWithRequest(pDb, "form", iId, "form.delete", "delete form", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	if ( tblBody ) xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "deleted"));
}

void Managed_RequestFormSubmissionListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.form") ) {
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "form ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT s.id,s.form_id,f.form_key,f.title,s.content_id,s.data_json,s.ip,s.status,s.create_time FROM content_form_submission s LEFT JOIN content_form f ON f.id=s.form_id WHERE s.delete_time=0 ORDER BY s.create_time DESC,s.id DESC LIMIT 500", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendFormSubmissionRow(arrList, stmt);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestFormSubmissionExportAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();
	str sJson = NULL;
	size_t iJsonSize = 0;

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.form") ) {
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "form ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT s.id,s.form_id,f.form_key,f.title,s.content_id,s.data_json,s.ip,s.status,s.create_time FROM content_form_submission s LEFT JOIN content_form f ON f.id=s.form_id WHERE s.delete_time=0 ORDER BY s.create_time DESC,s.id DESC LIMIT 5000", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendFormSubmissionRow(arrList, stmt);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	sJson = xrtStringifyJSON(arrList, FALSE, &iJsonSize);
	xvoTableSetInt(tblRet, "count", 5, xvoArrayItemCount(arrList));
	xvoTableSetText(tblRet, "exportType", 10, "json", 0, FALSE);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	xvoTableSetText(tblRet, "json", 4, sJson ? sJson : (str)"[]", 0, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestFormSubmissionStatusAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	int iStatus = tblBody ? (int)xvoTableGetInt(tblBody, "status", 6) : 1;

	(void)objServer; (void)objHost; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.form") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "form ability pack is not enabled");
		return;
	}
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) || (tblBody == NULL) || (iId <= 0) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "id is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_form_submission SET status=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iStatus);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_AuditLogWithRequest(pDb, "form_submission", iId, "form.submission.status", "update form submission status", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	if ( tblBody ) xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "updated"));
}

bool Managed_FormValidateRequired(xvalue tblData, const char* sSchemaJson, str* psMissing)
{
	xvalue tblSchema = NULL;
	xvalue arrRequired = NULL;
	str sMissing = NULL;
	bool bOK = TRUE;

	if ( psMissing ) *psMissing = NULL;
	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || Managed_IsBlank(sSchemaJson) ) return TRUE;
	tblSchema = xrtParseJSON((str)sSchemaJson, (int)strlen(sSchemaJson));
	if ( (tblSchema == NULL) || (xvoType(tblSchema) != XVO_DT_TABLE) ) {
		if ( tblSchema ) xvoUnref(tblSchema);
		return TRUE;
	}
	arrRequired = xvoTableGetValue(tblSchema, "required", 8);
	if ( arrRequired && (xvoType(arrRequired) == XVO_DT_ARRAY) ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrRequired); i++ ) {
			xvalue objName = xvoArrayGetValue(arrRequired, i);
			const char* sName = xvoGetText(objName);
			xvalue objValue = Managed_IsBlank(sName) ? NULL : xvoTableGetValue(tblData, sName, (int)strlen(sName));
			if ( Managed_IsBlank(sName) ) continue;
			if ( (objValue == NULL) || ((xvoType(objValue) == XVO_DT_TEXT) && Managed_IsBlank(xvoGetText(objValue))) ) {
				str sPart = sMissing ? xrtFormat("%s,%s", (const char*)sMissing, sName) : xrtCopyStr((str)sName, 0);
				if ( sMissing ) xrtFree(sMissing);
				sMissing = sPart;
				bOK = FALSE;
			}
		}
	}
	xvoUnref(tblSchema);
	if ( psMissing ) {
		*psMissing = sMissing;
	} else if ( sMissing ) {
		xrtFree(sMissing);
	}
	return bOK;
}

bool Managed_FormValueIsEmpty(xvalue objValue)
{
	if ( objValue == NULL ) return TRUE;
	if ( xvoType(objValue) == XVO_DT_TEXT ) return Managed_IsBlank(xvoGetText(objValue));
	if ( xvoType(objValue) == XVO_DT_ARRAY ) return xvoArrayItemCount(objValue) == 0;
	return FALSE;
}

bool Managed_FormTextIsNumber(const char* sText, bool bIntegerOnly)
{
	const char* p = sText;
	bool bDigit = FALSE;
	bool bDot = FALSE;

	if ( Managed_IsBlank(sText) ) return FALSE;
	if ( (*p == '-') || (*p == '+') ) p++;
	while ( *p ) {
		if ( (*p >= '0') && (*p <= '9') ) {
			bDigit = TRUE;
		} else if ( (*p == '.') && !bIntegerOnly && !bDot ) {
			bDot = TRUE;
		} else {
			return FALSE;
		}
		p++;
	}
	return bDigit;
}

bool Managed_FormTextMatchesFormat(const char* sText, const char* sFormat)
{
	const char* pAt = NULL;
	const char* pDot = NULL;

	if ( Managed_IsBlank(sFormat) || Managed_IsBlank(sText) ) return TRUE;
	if ( strcmp(sFormat, "email") == 0 ) {
		pAt = strchr(sText, '@');
		pDot = pAt ? strchr(pAt + 1, '.') : NULL;
		return (pAt != NULL) && (pDot != NULL) && (pAt > sText) && (*(pDot + 1) != '\0');
	}
	if ( strcmp(sFormat, "url") == 0 ) {
		return (strncmp(sText, "http://", 7) == 0) || (strncmp(sText, "https://", 8) == 0);
	}
	if ( strcmp(sFormat, "date") == 0 ) {
		return strlen(sText) == 10 && sText[4] == '-' && sText[7] == '-'
			&& (sText[0] >= '0' && sText[0] <= '9') && (sText[1] >= '0' && sText[1] <= '9')
			&& (sText[2] >= '0' && sText[2] <= '9') && (sText[3] >= '0' && sText[3] <= '9')
			&& (sText[5] >= '0' && sText[5] <= '9') && (sText[6] >= '0' && sText[6] <= '9')
			&& (sText[8] >= '0' && sText[8] <= '9') && (sText[9] >= '0' && sText[9] <= '9');
	}
	return TRUE;
}

bool Managed_FormValueMatchesType(xvalue objValue, const char* sType)
{
	const char* sText = NULL;

	if ( Managed_IsBlank(sType) || (objValue == NULL) ) return TRUE;
	if ( strcmp(sType, "string") == 0 || strcmp(sType, "text") == 0 || strcmp(sType, "email") == 0 || strcmp(sType, "url") == 0 ) {
		return xvoType(objValue) == XVO_DT_TEXT;
	}
	if ( strcmp(sType, "number") == 0 ) {
		if ( (xvoType(objValue) == XVO_DT_INT) || (xvoType(objValue) == XVO_DT_FLOAT) ) return TRUE;
		sText = (xvoType(objValue) == XVO_DT_TEXT) ? xvoGetText(objValue) : NULL;
		return Managed_FormTextIsNumber(sText, FALSE);
	}
	if ( strcmp(sType, "integer") == 0 || strcmp(sType, "int") == 0 ) {
		if ( xvoType(objValue) == XVO_DT_INT ) return TRUE;
		sText = (xvoType(objValue) == XVO_DT_TEXT) ? xvoGetText(objValue) : NULL;
		return Managed_FormTextIsNumber(sText, TRUE);
	}
	if ( strcmp(sType, "boolean") == 0 || strcmp(sType, "bool") == 0 ) return xvoType(objValue) == XVO_DT_BOOL;
	if ( strcmp(sType, "array") == 0 ) return xvoType(objValue) == XVO_DT_ARRAY;
	if ( strcmp(sType, "object") == 0 || strcmp(sType, "table") == 0 ) return xvoType(objValue) == XVO_DT_TABLE;
	return TRUE;
}

double Managed_FormValueAsNumber(xvalue objValue, bool* pOK)
{
	const char* sText = NULL;

	if ( pOK ) *pOK = FALSE;
	if ( objValue == NULL ) return 0;
	if ( xvoType(objValue) == XVO_DT_INT ) {
		if ( pOK ) *pOK = TRUE;
		return (double)xvoGetInt(objValue);
	}
	if ( xvoType(objValue) == XVO_DT_FLOAT ) {
		if ( pOK ) *pOK = TRUE;
		return xvoGetFloat(objValue);
	}
	if ( xvoType(objValue) == XVO_DT_TEXT ) {
		sText = xvoGetText(objValue);
		if ( Managed_FormTextIsNumber(sText, FALSE) ) {
			if ( pOK ) *pOK = TRUE;
			return atof(sText);
		}
	}
	return 0;
}

bool Managed_FormTextMatchesPattern(const char* sText, const char* sPattern)
{
	xregex* pRegex = NULL;
	xregexspan span[1];
	int iLen;
	bool bMatch = FALSE;

	if ( Managed_IsBlank(sPattern) || (sText == NULL) ) return TRUE;
	iLen = (int)strlen(sText);
	pRegex = xrtRegexCreate(sPattern);
	if ( pRegex == NULL || (xrtRegexGetErrorMsg(pRegex) && xrtRegexGetErrorMsg(pRegex)[0]) ) {
		if ( pRegex ) xrtRegexDestroy(pRegex);
		return FALSE;
	}
	memset(span, 0, sizeof(span));
	bMatch = xrtRegexCaptures(pRegex, sText, iLen, span, 1) == 0;
	xrtRegexDestroy(pRegex);
	return bMatch;
}

bool Managed_FormValueMatchesLimits(xvalue objValue, xvalue tblField, const char* sName, str* psError)
{
	const char* sText = (objValue && (xvoType(objValue) == XVO_DT_TEXT)) ? xvoGetText(objValue) : NULL;
	const char* sPattern = tblField ? xvoTableGetText(tblField, "pattern", 7) : NULL;
	int64 iMinLength = tblField ? xvoTableGetInt(tblField, "minLength", 9) : 0;
	int64 iMaxLength = tblField ? xvoTableGetInt(tblField, "maxLength", 9) : 0;
	xvalue objMin = tblField ? xvoTableGetValue(tblField, "min", 3) : NULL;
	xvalue objMax = tblField ? xvoTableGetValue(tblField, "max", 3) : NULL;
	bool bValueOK = FALSE;
	double dValue = Managed_FormValueAsNumber(objValue, &bValueOK);

	if ( sText ) {
		int iLen = (int)strlen(sText);
		if ( (iMinLength > 0) && (iLen < iMinLength) ) {
			if ( psError ) *psError = xrtFormat("%s minLength is %lld", sName, (long long)iMinLength);
			return FALSE;
		}
		if ( (iMaxLength > 0) && (iLen > iMaxLength) ) {
			if ( psError ) *psError = xrtFormat("%s maxLength is %lld", sName, (long long)iMaxLength);
			return FALSE;
		}
		if ( !Managed_FormTextMatchesPattern(sText, sPattern) ) {
			if ( psError ) *psError = xrtFormat("%s pattern is invalid", sName);
			return FALSE;
		}
	}
	if ( objMin || objMax ) {
		bool bLimitOK = FALSE;
		if ( !bValueOK ) {
			if ( psError ) *psError = xrtFormat("%s must be numeric", sName);
			return FALSE;
		}
		if ( objMin ) {
			double dMin = Managed_FormValueAsNumber(objMin, &bLimitOK);
			if ( bLimitOK && (dValue < dMin) ) {
				if ( psError ) *psError = xrtFormat("%s min is %g", sName, dMin);
				return FALSE;
			}
		}
		if ( objMax ) {
			double dMax = Managed_FormValueAsNumber(objMax, &bLimitOK);
			if ( bLimitOK && (dValue > dMax) ) {
				if ( psError ) *psError = xrtFormat("%s max is %g", sName, dMax);
				return FALSE;
			}
		}
	}
	return TRUE;
}

bool Managed_FormOptionContainsValue(xvalue arrOptions, xvalue objValue)
{
	str sNeedle = NULL;
	bool bFound = FALSE;

	if ( (arrOptions == NULL) || (xvoType(arrOptions) != XVO_DT_ARRAY) ) return TRUE;
	sNeedle = Managed_ValueToTextDup(objValue);
	for ( uint32 i = 0; i < xvoArrayItemCount(arrOptions); i++ ) {
		xvalue tblOption = xvoArrayGetValue(arrOptions, i);
		xvalue objAllowed = (tblOption && (xvoType(tblOption) == XVO_DT_TABLE)) ? xvoTableGetValue(tblOption, "value", 5) : tblOption;
		str sAllowed = Managed_ValueToTextDup(objAllowed);
		if ( strcmp(sNeedle ? (const char*)sNeedle : "", sAllowed ? (const char*)sAllowed : "") == 0 ) {
			bFound = TRUE;
		}
		if ( sAllowed ) xrtFree(sAllowed);
		if ( bFound ) break;
	}
	if ( sNeedle ) xrtFree(sNeedle);
	return bFound;
}

bool Managed_FormValueMatchesOptions(xvalue objValue, xvalue tblField, const char* sName, str* psError)
{
	xvalue arrOptions = Managed_ResolveFieldList(tblField);
	bool bOK = TRUE;

	if ( (arrOptions == NULL) || (xvoType(arrOptions) != XVO_DT_ARRAY) || (xvoArrayItemCount(arrOptions) <= 0) ) {
		if ( arrOptions ) xvoUnref(arrOptions);
		return TRUE;
	}
	if ( objValue && (xvoType(objValue) == XVO_DT_ARRAY) ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(objValue); i++ ) {
			if ( !Managed_FormOptionContainsValue(arrOptions, xvoArrayGetValue(objValue, i)) ) {
				bOK = FALSE;
				break;
			}
		}
	} else {
		bOK = Managed_FormOptionContainsValue(arrOptions, objValue);
	}
	xvoUnref(arrOptions);
	if ( !bOK && psError ) *psError = xrtFormat("%s option is invalid", sName);
	return bOK;
}

bool Managed_FormValidateSchema(xvalue tblData, const char* sSchemaJson, str* psError)
{
	xvalue tblSchema = NULL;
	xvalue arrFields = NULL;
	str sMissing = NULL;

	if ( psError ) *psError = NULL;
	if ( !Managed_FormValidateRequired(tblData, sSchemaJson, &sMissing) ) {
		if ( psError ) *psError = xrtFormat("required fields missing: %s", sMissing ? (const char*)sMissing : "");
		if ( sMissing ) xrtFree(sMissing);
		return FALSE;
	}
	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || Managed_IsBlank(sSchemaJson) ) return TRUE;
	tblSchema = xrtParseJSON((str)sSchemaJson, (int)strlen(sSchemaJson));
	if ( (tblSchema == NULL) || (xvoType(tblSchema) != XVO_DT_TABLE) ) {
		if ( tblSchema ) xvoUnref(tblSchema);
		return TRUE;
	}
	arrFields = xvoTableGetValue(tblSchema, "fields", 6);
	if ( arrFields && (xvoType(arrFields) == XVO_DT_ARRAY) ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
			xvalue tblField = xvoArrayGetValue(arrFields, i);
			const char* sName;
			const char* sType;
			const char* sFormat;
			xvalue objValue;
			if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) continue;
			sName = xvoTableGetText(tblField, "name", 4);
			sType = xvoTableGetText(tblField, "type", 4);
			sFormat = xvoTableGetText(tblField, "format", 6);
			if ( Managed_IsBlank(sFormat) && (strcmp(sType ? sType : "", "email") == 0 || strcmp(sType ? sType : "", "url") == 0) ) sFormat = sType;
			if ( Managed_IsBlank(sName) ) continue;
			objValue = xvoTableGetValue(tblData, sName, (int)strlen(sName));
			if ( Managed_FormValueIsEmpty(objValue) ) {
				if ( xvoTableGetBool(tblField, "required", 8) ) {
					if ( psError ) *psError = xrtFormat("%s is required", sName);
					xvoUnref(tblSchema);
					return FALSE;
				}
				continue;
			}
			if ( !Managed_FormValueMatchesType(objValue, sType) ) {
				if ( psError ) *psError = xrtFormat("%s type is invalid", sName);
				xvoUnref(tblSchema);
				return FALSE;
			}
			if ( !Managed_FormValueMatchesOptions(objValue, tblField, sName, psError) ) {
				xvoUnref(tblSchema);
				return FALSE;
			}
			if ( !Managed_FormTextMatchesFormat((xvoType(objValue) == XVO_DT_TEXT) ? xvoGetText(objValue) : NULL, sFormat) ) {
				if ( psError ) *psError = xrtFormat("%s format is invalid", sName);
				xvoUnref(tblSchema);
				return FALSE;
			}
			if ( !Managed_FormValueMatchesLimits(objValue, tblField, sName, psError) ) {
				xvoUnref(tblSchema);
				return FALSE;
			}
		}
	}
	xvoUnref(tblSchema);
	return TRUE;
}

bool Managed_FormAntiSpamPass(xvalue tblBody, int64 iNow, str* psError)
{
	const char* sHoneypot = NULL;
	int64 iStartedAt = 0;

	if ( psError ) *psError = NULL;
	if ( (tblBody == NULL) || (xvoType(tblBody) != XVO_DT_TABLE) ) return TRUE;
	sHoneypot = xvoTableGetText(tblBody, "honeypot", 8);
	if ( Managed_IsBlank(sHoneypot) ) sHoneypot = xvoTableGetText(tblBody, "_hp", 3);
	if ( Managed_IsBlank(sHoneypot) ) sHoneypot = xvoTableGetText(tblBody, "website", 7);
	if ( !Managed_IsBlank(sHoneypot) ) {
		if ( psError ) *psError = xrtCopyStr("spam rejected", 0);
		return FALSE;
	}
	iStartedAt = xvoTableGetInt(tblBody, "startedAt", 9);
	if ( iStartedAt <= 0 ) iStartedAt = xvoTableGetInt(tblBody, "formStartedAt", 13);
	if ( (iStartedAt > 0) && (iNow > iStartedAt) && ((iNow - iStartedAt) < 2) ) {
		if ( psError ) *psError = xrtCopyStr("form submitted too quickly", 0);
		return FALSE;
	}
	return TRUE;
}

void Managed_RequestFormSubmitPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	xvalue tblData = tblBody ? xvoTableGetValue(tblBody, "data", 4) : NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	str sSchemaJson = NULL;
	str sMissing = NULL;
	const char* sFormKey = tblBody ? xvoTableGetText(tblBody, "formKey", 7) : NULL;
	int64 iFormId = tblBody ? xvoTableGetInt(tblBody, "formId", 6) : 0;
	int64 iContentId = tblBody ? xvoTableGetInt(tblBody, "contentId", 9) : 0;
	int64 iNow = xrtNow();
	size_t iJsonSize = 0;
	str sDataJson = NULL;
	const char* sIp = Managed_AuditSessionIp(objSession);

	(void)objServer; (void)objHost;
	if ( !Managed_AbilityPackMounted("content.form") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "form ability pack is not enabled");
		return;
	}
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) || (tblBody == NULL) || (tblData == NULL) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "invalid request body");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( iFormId <= 0 ) {
		if ( sqlite3_prepare_v2(pDb, "SELECT id,content_id,schema_json FROM content_form WHERE form_key=? AND status=1 AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sFormKey ? sFormKey : "", -1, SQLITE_TRANSIENT);
			if ( sqlite3_step(stmt) == SQLITE_ROW ) {
				iFormId = sqlite3_column_int64(stmt, 0);
				if ( iContentId <= 0 ) iContentId = sqlite3_column_int64(stmt, 1);
				sSchemaJson = xrtCopyStr((str)sqlite3_column_text(stmt, 2), 0);
			}
		}
		if ( stmt ) sqlite3_finalize(stmt);
		stmt = NULL;
	} else {
		if ( sqlite3_prepare_v2(pDb, "SELECT content_id,schema_json FROM content_form WHERE id=? AND status=1 AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iFormId);
			if ( sqlite3_step(stmt) == SQLITE_ROW ) {
				if ( iContentId <= 0 ) iContentId = sqlite3_column_int64(stmt, 0);
				sSchemaJson = xrtCopyStr((str)sqlite3_column_text(stmt, 1), 0);
			}
		}
		if ( stmt ) sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if ( iFormId <= 0 ) {
		Managed_CloseDb(pDb);
		xvoUnref(tblBody);
		Managed_SendError(objResp, "form not found");
		return;
	}
	if ( !Managed_FormAntiSpamPass(tblBody, iNow, &sMissing) ) {
		Managed_CloseDb(pDb);
		if ( sSchemaJson ) xrtFree(sSchemaJson);
		xvoUnref(tblBody);
		Managed_SendError(objResp, sMissing ? (const char*)sMissing : "spam rejected");
		if ( sMissing ) xrtFree(sMissing);
		return;
	}
	if ( !Managed_FormValidateSchema(tblData, sSchemaJson ? (const char*)sSchemaJson : NULL, &sMissing) ) {
		Managed_CloseDb(pDb);
		if ( sSchemaJson ) xrtFree(sSchemaJson);
		xvoUnref(tblBody);
		Managed_SendError(objResp, sMissing ? (const char*)sMissing : "form data is invalid");
		if ( sMissing ) xrtFree(sMissing);
		return;
	}
	sDataJson = xrtStringifyJSON(tblData, FALSE, &iJsonSize);
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_form_submission(form_id,content_id,data_json,ip,status,create_time,delete_time) VALUES(?,?,?,?,1,?,0)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iFormId);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iContentId);
		sqlite3_bind_text(stmt, 3, sDataJson ? (const char*)sDataJson : "{}", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, sIp, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 5, (sqlite3_int64)iNow);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	if ( sDataJson ) xrtFree(sDataJson);
	if ( sSchemaJson ) xrtFree(sSchemaJson);
	if ( sMissing ) xrtFree(sMissing);
	xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "submitted"));
}

void Managed_AppendAccessRuleRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();
	int64 iTargetId = sqlite3_column_int64(stmt, 1);

	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetText(tblRow, "targetType", 10, iTargetId < 0 ? "category" : "content", 0, FALSE);
	xvoTableSetInt(tblRow, "targetId", 8, iTargetId < 0 ? -iTargetId : iTargetId);
	xvoTableSetInt(tblRow, "categoryId", 10, iTargetId < 0 ? -iTargetId : 0);
	xvoTableSetInt(tblRow, "contentId", 9, iTargetId > 0 ? iTargetId : 0);
	xvoTableSetInt(tblRow, "ruleTargetId", 12, iTargetId);
	xvoTableSetText(tblRow, "contentTitle", 12, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetText(tblRow, "accessMode", 10, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetInt(tblRow, "requiredReadLevel", 17, sqlite3_column_int64(stmt, 4));
	xvoTableSetText(tblRow, "memberGroupIds", 14, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
	xvoTableSetInt(tblRow, "price", 5, sqlite3_column_int64(stmt, 6));
	xvoTableSetInt(tblRow, "status", 6, sqlite3_column_int(stmt, 7));
	Managed_SetTimeText(tblRow, "updateTimeText", 14, sqlite3_column_int64(stmt, 8));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_RequestAccessRuleListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.access") ) {
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "access ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT r.id,r.content_id,CASE WHEN r.content_id<0 THEN c.title ELSE i.title END AS target_title,r.access_mode,r.required_read_level,r.member_group_ids,r.price,r.status,r.update_time FROM content_access_rule r LEFT JOIN content_item i ON i.id=r.content_id LEFT JOIN content_category c ON c.id=-r.content_id WHERE r.delete_time=0 ORDER BY r.update_time DESC,r.id DESC LIMIT 500", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendAccessRuleRow(arrList, stmt);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestAccessRuleSaveAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	int64 iContentId = tblBody ? xvoTableGetInt(tblBody, "contentId", 9) : 0;
	int64 iCategoryId = tblBody ? xvoTableGetInt(tblBody, "categoryId", 10) : 0;
	int64 iTargetId = tblBody ? xvoTableGetInt(tblBody, "targetId", 8) : 0;
	const char* sTargetType = tblBody ? xvoTableGetText(tblBody, "targetType", 10) : NULL;
	const char* sMode = tblBody ? xvoTableGetText(tblBody, "accessMode", 10) : NULL;
	const char* sGroupIds = tblBody ? xvoTableGetText(tblBody, "memberGroupIds", 14) : NULL;
	const char* sPassword = tblBody ? xvoTableGetText(tblBody, "password", 8) : NULL;
	str sPasswordHash = NULL;
	int64 iRequiredLevel = tblBody ? xvoTableGetInt(tblBody, "requiredReadLevel", 17) : 0;
	int64 iPrice = tblBody ? xvoTableGetInt(tblBody, "price", 5) : 0;
	int iStatus = tblBody ? (int)xvoTableGetInt(tblBody, "status", 6) : 1;
	int64 iNow = xrtNow();

	(void)objServer; (void)objHost; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.access") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "access ability pack is not enabled");
		return;
	}
	if ( !Managed_IsBlank(sTargetType) && (strcmp(sTargetType, "category") == 0) ) {
		if ( iCategoryId <= 0 ) iCategoryId = iTargetId;
		iContentId = iCategoryId > 0 ? -iCategoryId : 0;
	} else if ( iContentId <= 0 && iTargetId > 0 ) {
		iContentId = iTargetId;
	}
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) || (tblBody == NULL) || (iContentId == 0) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "targetId is required");
		return;
	}
	if ( Managed_IsBlank(sMode) ) sMode = "public";
	if ( Managed_IsBlank(sGroupIds) ) sGroupIds = "";
	if ( Managed_IsBlank(sPassword) ) sPassword = "";
	sPasswordHash = (strcmp(sMode, "password") == 0) ? Managed_AccessPasswordEncode(iContentId, sPassword) : xrtCopyStr((str)"", 0);
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( sPasswordHash ) xrtFree(sPasswordHash);
		xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( iId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE content_access_rule SET content_id=?,access_mode=?,required_read_level=?,member_group_ids=?,password_hash=?,price=?,status=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
			sqlite3_bind_text(stmt, 2, sMode, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iRequiredLevel);
			sqlite3_bind_text(stmt, 4, sGroupIds, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 5, sPasswordHash ? (const char*)sPasswordHash : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 6, (sqlite3_int64)iPrice);
			sqlite3_bind_int(stmt, 7, iStatus);
			sqlite3_bind_int64(stmt, 8, (sqlite3_int64)iNow);
			sqlite3_bind_int64(stmt, 9, (sqlite3_int64)iId);
			sqlite3_step(stmt);
		}
	} else {
		if ( sqlite3_prepare_v2(pDb, "INSERT OR REPLACE INTO content_access_rule(content_id,access_mode,required_read_level,member_group_ids,password_hash,price,status,create_time,update_time,delete_time) VALUES(?,?,?,?,?,?,?,?,?,0)", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
			sqlite3_bind_text(stmt, 2, sMode, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iRequiredLevel);
			sqlite3_bind_text(stmt, 4, sGroupIds, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 5, sPasswordHash ? (const char*)sPasswordHash : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 6, (sqlite3_int64)iPrice);
			sqlite3_bind_int(stmt, 7, iStatus);
			sqlite3_bind_int64(stmt, 8, (sqlite3_int64)iNow);
			sqlite3_bind_int64(stmt, 9, (sqlite3_int64)iNow);
			sqlite3_step(stmt);
			iId = sqlite3_last_insert_rowid(pDb);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_AuditLogWithRequest(pDb, "access_rule", iId, "access_rule.save", sMode ? sMode : "", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	if ( sPasswordHash ) xrtFree(sPasswordHash);
	xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(iId > 0, iId > 0 ? "saved" : "save failed"));
}

void Managed_RequestAccessRuleDeleteAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	int64 iNow = xrtNow();

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.access") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "access ability pack is not enabled");
		return;
	}
	if ( iId <= 0 ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "id is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_access_rule SET delete_time=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_AuditLogWithRequest(pDb, "access_rule", iId, "access_rule.delete", "delete access rule", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	if ( tblBody ) xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "deleted"));
}

void Managed_RequestAccessCheckPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sContentId[32];
	sqlite3* pDb = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue tblAccess = xvoCreateTable();
	int64 iContentId;
	bool bAllowed = FALSE;

	(void)objServer; (void)objHost;
	memset(sContentId, 0, sizeof(sContentId));
	xsReqQueryValue(objReq, "contentId", sContentId, sizeof(sContentId));
	if ( sContentId[0] == '\0' ) xsReqQueryValue(objReq, "id", sContentId, sizeof(sContentId));
	iContentId = atoll(sContentId);
	if ( !Managed_AbilityPackMounted("content.access") ) {
		xvoUnref(tblRet);
		xvoUnref(tblAccess);
		Managed_SendError(objResp, "access ability pack is not enabled");
		return;
	}
	if ( (iContentId <= 0) || !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblRet);
		xvoUnref(tblAccess);
		Managed_SendError(objResp, "contentId is required");
		return;
	}
	bAllowed = Managed_AccessCheckRule(pDb, iContentId, objReq, objSession, FALSE, tblAccess);
	Managed_CloseDb(pDb);
	xvoTableSetBool(tblRet, "allowed", 7, bAllowed);
	xvoTableSetValue(tblRet, "data", 4, tblAccess, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

const char* Managed_AuditSessionIp(xvalue objSession)
{
	const char* sIp = NULL;

	if ( objSession && (xvoType(objSession) == XVO_DT_TABLE) ) {
		sIp = xvoTableGetText(objSession, "ip", 2);
		if ( Managed_IsBlank(sIp) ) sIp = xvoTableGetText(objSession, "clientIp", 8);
		if ( Managed_IsBlank(sIp) ) sIp = xvoTableGetText(objSession, "remoteAddr", 10);
	}
	if ( Managed_IsBlank(sIp) || strchr(sIp, '\r') || strchr(sIp, '\n') ) {
		return "";
	}
	return sIp;
}

const char* Managed_AuditRequestIp(XS_RequestObject objReq)
{
	const char* sIp = objReq ? xsReqRemote(objReq) : NULL;
	if ( Managed_IsBlank(sIp) || strchr(sIp, '\r') || strchr(sIp, '\n') ) {
		return "";
	}
	return sIp;
}

void Managed_AuditLogCore(sqlite3* pDb, const char* sTargetType, int64 iTargetId, const char* sAction, const char* sSummary, const char* sDetailJson, const char* sRequestIp, xvalue objSession)
{
	sqlite3_stmt* stmt = NULL;
	const char* sOperatorType = "";
	const char* sIp = Managed_IsBlank(sRequestIp) ? Managed_AuditSessionIp(objSession) : sRequestIp;
	int64 iOperatorId = 0;

	/* Audit writes are optional capability-pack effects. Disabled packs do not touch the audit table. */
	if ( !Managed_AbilityPackMounted("content.audit-log") || (pDb == NULL) || Managed_IsBlank(sAction) ) return;
	if ( objSession && (xvoType(objSession) == XVO_DT_TABLE) ) {
		iOperatorId = xvoTableGetInt(objSession, "id", 2);
		if ( xvoTableGetInt(objSession, "memberId", 8) > 0 ) {
			iOperatorId = xvoTableGetInt(objSession, "memberId", 8);
			sOperatorType = "member";
		} else if ( xvoTableGetInt(objSession, "memberID", 8) > 0 ) {
			iOperatorId = xvoTableGetInt(objSession, "memberID", 8);
			sOperatorType = "member";
		} else if ( iOperatorId > 0 ) {
			sOperatorType = "admin";
		}
	}
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_audit_log(target_type,target_id,action,summary,detail_json,operator_type,operator_id,ip,create_time) VALUES(?,?,?,?,?,?,?,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, Managed_IsBlank(sTargetType) ? "content" : sTargetType, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iTargetId);
		sqlite3_bind_text(stmt, 3, sAction, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, sSummary ? sSummary : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 5, sDetailJson ? sDetailJson : "{}", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 6, sOperatorType, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 7, (sqlite3_int64)iOperatorId);
		sqlite3_bind_text(stmt, 8, sIp, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 9, (sqlite3_int64)xrtNow());
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
}

void Managed_AppendAuditLogRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();

	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetText(tblRow, "targetType", 10, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
	xvoTableSetInt(tblRow, "targetId", 8, sqlite3_column_int64(stmt, 2));
	xvoTableSetText(tblRow, "action", 6, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetText(tblRow, "summary", 7, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetText(tblRow, "detailJson", 10, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
	xvoTableSetText(tblRow, "operatorType", 12, (str)sqlite3_column_text(stmt, 6), 0, FALSE);
	xvoTableSetInt(tblRow, "operatorId", 10, sqlite3_column_int64(stmt, 7));
	xvoTableSetText(tblRow, "ip", 2, (str)sqlite3_column_text(stmt, 8), 0, FALSE);
	xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 9));
	Managed_SetTimeText(tblRow, "createTimeText", 14, sqlite3_column_int64(stmt, 9));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_RequestAuditLogListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.audit-log") ) {
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "audit-log ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id,target_type,target_id,action,summary,detail_json,operator_type,operator_id,ip,create_time FROM content_audit_log ORDER BY create_time DESC,id DESC LIMIT 500", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendAuditLogRow(arrList, stmt);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestAuditLogCleanupAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	xvalue tblRet = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iKeepDays = tblBody ? xvoTableGetInt(tblBody, "keepDays", 8) : 0;
	int64 iBeforeTime = tblBody ? xvoTableGetInt(tblBody, "beforeTime", 10) : 0;
	int64 iNow = xrtNow();
	int iDeleted = 0;

	(void)objServer; (void)objHost; (void)objReq;
	if ( !Managed_AbilityPackMounted("content.audit-log") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "audit-log ability pack is not enabled");
		return;
	}
	if ( (iBeforeTime <= 0) && (iKeepDays > 0) ) {
		iBeforeTime = iNow - (iKeepDays * 86400);
	}
	/* Keep cleanup predictable and prevent accidental full audit removal. */
	if ( (iBeforeTime <= 0) || (iBeforeTime >= iNow) ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "keepDays or beforeTime is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( tblBody ) xvoUnref(tblBody);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM content_audit_log WHERE create_time < ?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iBeforeTime);
		sqlite3_step(stmt);
		iDeleted = sqlite3_changes(pDb);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_AuditLogWithRequest(pDb, "audit_log", 0, "audit.cleanup", "cleanup audit logs", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	if ( tblBody ) xvoUnref(tblBody);
	tblRet = Managed_CreateResult(TRUE, "cleanup finished");
	xvoTableSetInt(tblRet, "deletedCount", 12, iDeleted);
	xvoTableSetInt(tblRet, "beforeTime", 10, iBeforeTime);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_AppendImportJobRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();

	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetText(tblRow, "sourceName", 10, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
	xvoTableSetText(tblRow, "status", 6, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetInt(tblRow, "totalCount", 10, sqlite3_column_int64(stmt, 3));
	xvoTableSetInt(tblRow, "successCount", 12, sqlite3_column_int64(stmt, 4));
	xvoTableSetInt(tblRow, "failCount", 9, sqlite3_column_int64(stmt, 5));
	xvoTableSetText(tblRow, "reportJson", 10, (str)sqlite3_column_text(stmt, 6), 0, FALSE);
	xvoTableSetInt(tblRow, "operatorId", 10, sqlite3_column_int64(stmt, 7));
	Managed_SetTimeText(tblRow, "createTimeText", 14, sqlite3_column_int64(stmt, 8));
	Managed_SetTimeText(tblRow, "finishTimeText", 14, sqlite3_column_int64(stmt, 9));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_AppendExportJobRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();

	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetText(tblRow, "exportType", 10, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
	xvoTableSetText(tblRow, "status", 6, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetInt(tblRow, "totalCount", 10, sqlite3_column_int64(stmt, 3));
	xvoTableSetText(tblRow, "filterJson", 10, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetText(tblRow, "resultJson", 10, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
	xvoTableSetInt(tblRow, "operatorId", 10, sqlite3_column_int64(stmt, 6));
	Managed_SetTimeText(tblRow, "createTimeText", 14, sqlite3_column_int64(stmt, 7));
	Managed_SetTimeText(tblRow, "finishTimeText", 14, sqlite3_column_int64(stmt, 8));
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

bool Managed_ImportFieldAllowed(xvalue arrFields, const char* sName)
{
	int i;
	int iCount;

	if ( Managed_IsBlank(sName) ) return FALSE;
	if ( (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) || (xvoArrayItemCount(arrFields) <= 0) ) return TRUE;
	iCount = (int)xvoArrayItemCount(arrFields);
	for ( i = 0; i < iCount; i++ ) {
		const char* sField = xvoArrayGetText(arrFields, i);
		if ( (sField != NULL) && (strcmp(sField, sName) == 0) ) return TRUE;
	}
	return FALSE;
}

xvalue Managed_ImportBuildData(xvalue tblItem, xvalue tblSpec, xvalue arrFields)
{
	xvalue tblData = xvoCreateTable();
	xvalue arrSpecFields = Managed_GetFields(tblSpec);
	int i;
	int iCount;

	if ( (tblItem == NULL) || (xvoType(tblItem) != XVO_DT_TABLE) ) return tblData;
	if ( (arrSpecFields == NULL) || (xvoType(arrSpecFields) != XVO_DT_ARRAY) ) return xvoCopy(tblItem);
	iCount = (int)xvoArrayItemCount(arrSpecFields);
	for ( i = 0; i < iCount; i++ ) {
		xvalue tblField = xvoArrayGetValue(arrSpecFields, i);
		const char* sName = ((tblField != NULL) && (xvoType(tblField) == XVO_DT_TABLE)) ? xvoTableGetText(tblField, "name", 4) : NULL;
		xvalue objValue;

		if ( !Managed_ImportFieldAllowed(arrFields, sName) ) continue;
		objValue = xvoTableGetValue(tblItem, sName, (int)strlen(sName));
		if ( objValue ) {
			xvoTableSetValue(tblData, sName, (int)strlen(sName), xvoCopy(objValue), TRUE);
		}
	}
	if ( Managed_ImportFieldAllowed(arrFields, "slug") ) {
		xvalue objSlug = xvoTableGetValue(tblItem, "slug", 4);
		if ( objSlug ) xvoTableSetValue(tblData, "slug", 4, xvoCopy(objSlug), TRUE);
	}
	if ( Managed_ImportFieldAllowed(arrFields, "summary") ) {
		xvalue objSummary = xvoTableGetValue(tblItem, "summary", 7);
		if ( objSummary ) xvoTableSetValue(tblData, "summary", 7, xvoCopy(objSummary), TRUE);
	}
	return tblData;
}

void Managed_RequestImportPreviewAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	xvalue arrItems = tblBody ? xvoTableGetValue(tblBody, "items", 5) : NULL;
	xvalue arrFields = tblBody ? xvoTableGetValue(tblBody, "fields", 6) : NULL;
	xvalue arrReport = xvoCreateArray();
	xvalue tblSpec = Managed_LoadSpec();
	xvalue tblRet = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	str sReportJson = NULL;
	const char* sSourceName = tblBody ? xvoTableGetText(tblBody, "sourceName", 10) : NULL;
	int64 iNow = xrtNow();
	int64 iOperatorId = (objSession && (xvoType(objSession) == XVO_DT_TABLE)) ? xvoTableGetInt(objSession, "id", 2) : 0;
	int iTotal = 0;
	int iSuccess = 0;
	int iFail = 0;
	int i;

	(void)objServer; (void)objHost;
	if ( !Managed_AbilityPackMounted("content.import-export") ) {
		if ( tblBody ) xvoUnref(tblBody);
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(arrReport);
		Managed_SendError(objResp, "import-export ability pack is not enabled");
		return;
	}
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) || (tblBody == NULL) || (arrItems == NULL) || (xvoType(arrItems) != XVO_DT_ARRAY) || (tblSpec == NULL) ) {
		if ( tblBody ) xvoUnref(tblBody);
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(arrReport);
		Managed_SendError(objResp, "items array is required");
		return;
	}
	iTotal = (int)xvoArrayItemCount(arrItems);
	for ( i = 0; i < iTotal; i++ ) {
		xvalue tblItem = xvoArrayGetValue(arrItems, i);
		xvalue tblData = NULL;
		xvalue tblRow = xvoCreateTable();
		str sError = NULL;
		bool bOk = FALSE;

		if ( tblItem && (xvoType(tblItem) == XVO_DT_TABLE) ) {
			tblData = Managed_ImportBuildData(tblItem, tblSpec, arrFields);
			Managed_ApplyMissingDefaults(tblData, tblSpec);
			Managed_NormalizeNullableFields(tblData, tblSpec);
			Managed_CoerceFieldValues(tblData, tblSpec);
			bOk = Managed_ValidateData(tblSpec, tblData, FALSE, &sError);
		} else {
			sError = xrtCopyStr("row is not an object", 0);
		}
		if ( bOk ) {
			iSuccess++;
		} else {
			iFail++;
		}
		xvoTableSetInt(tblRow, "rowIndex", 8, i + 1);
		xvoTableSetBool(tblRow, "ok", 2, bOk);
		xvoTableSetText(tblRow, "message", 7, sError ? sError : (str)"ok", 0, FALSE);
		if ( tblData ) xvoTableSetValue(tblRow, "data", 4, xvoCopy(tblData), TRUE);
		xvoArrayAppendValue(arrReport, tblRow, TRUE);
		if ( tblData ) xvoUnref(tblData);
		if ( sError ) xrtFree(sError);
	}
	sReportJson = xrtStringifyJSON(arrReport, FALSE, NULL);
	if ( Managed_EnsureSchema() && Managed_OpenDb(&pDb) ) {
		if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_import_job(source_name,status,total_count,success_count,fail_count,report_json,operator_id,create_time,finish_time) VALUES(?,'preview',?,?,?,?,?,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, Managed_IsBlank(sSourceName) ? "json-preview" : sSourceName, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 2, iTotal);
			sqlite3_bind_int(stmt, 3, iSuccess);
			sqlite3_bind_int(stmt, 4, iFail);
			sqlite3_bind_text(stmt, 5, sReportJson ? (const char*)sReportJson : "[]", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 6, (sqlite3_int64)iOperatorId);
			sqlite3_bind_int64(stmt, 7, (sqlite3_int64)iNow);
			sqlite3_bind_int64(stmt, 8, (sqlite3_int64)iNow);
			sqlite3_step(stmt);
		}
		if ( stmt ) sqlite3_finalize(stmt);
		Managed_AuditLogWithRequest(pDb, "import_job", sqlite3_last_insert_rowid(pDb), "import.preview", Managed_IsBlank(sSourceName) ? "json-preview" : sSourceName, sReportJson ? (const char*)sReportJson : "[]", objReq, objSession);
		Managed_CloseDb(pDb);
	}
	tblRet = Managed_CreateResult(TRUE, "preview finished");
	xvoTableSetInt(tblRet, "totalCount", 10, iTotal);
	xvoTableSetInt(tblRet, "successCount", 12, iSuccess);
	xvoTableSetInt(tblRet, "failCount", 9, iFail);
	xvoTableSetValue(tblRet, "data", 4, arrReport, TRUE);
	if ( tblBody ) xvoUnref(tblBody);
	if ( tblSpec ) xvoUnref(tblSpec);
	if ( sReportJson ) xrtFree(sReportJson);
	Managed_SendJsonValue(objResp, tblRet);
}

bool Managed_ContentExists(sqlite3* pDb, int64 iContentId)
{
	sqlite3_stmt* stmt = NULL;
	bool bExists = FALSE;

	if ( (pDb == NULL) || (iContentId <= 0) ) return FALSE;
	if ( sqlite3_prepare_v2(pDb, "SELECT 1 FROM content_item WHERE id=? AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		bExists = (sqlite3_step(stmt) == SQLITE_ROW);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return bExists;
}

void Managed_RequestImportCommitAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	xvalue arrItems = tblBody ? xvoTableGetValue(tblBody, "items", 5) : NULL;
	xvalue arrFields = tblBody ? xvoTableGetValue(tblBody, "fields", 6) : NULL;
	xvalue arrReport = xvoCreateArray();
	xvalue tblSpec = Managed_LoadSpec();
	xvalue tblRet = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	str sReportJson = NULL;
	const char* sSourceName = tblBody ? xvoTableGetText(tblBody, "sourceName", 10) : NULL;
	const char* sConflictMode = tblBody ? xvoTableGetText(tblBody, "conflictMode", 12) : NULL;
	bool bConfirm = tblBody ? xvoTableGetBool(tblBody, "confirm", 7) : FALSE;
	int64 iNow = xrtNow();
	int64 iOperatorId = (objSession && (xvoType(objSession) == XVO_DT_TABLE)) ? xvoTableGetInt(objSession, "id", 2) : 0;
	int iTotal = 0;
	int iSuccess = 0;
	int iFail = 0;
	int i;
	bool bUpdateExisting = !Managed_IsBlank(sConflictMode) && (strcmp(sConflictMode, "update") == 0);
	bool bSkipExisting = !Managed_IsBlank(sConflictMode) && (strcmp(sConflictMode, "skip") == 0);

	(void)objServer; (void)objHost;
	if ( !Managed_AbilityPackMounted("content.import-export") ) {
		if ( tblBody ) xvoUnref(tblBody);
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(arrReport);
		Managed_SendError(objResp, "import-export ability pack is not enabled");
		return;
	}
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) || !bConfirm || (tblBody == NULL) || (arrItems == NULL) || (xvoType(arrItems) != XVO_DT_ARRAY) || (tblSpec == NULL) ) {
		if ( tblBody ) xvoUnref(tblBody);
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(arrReport);
		Managed_SendError(objResp, "confirm=true and items array are required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(arrReport);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	iTotal = (int)xvoArrayItemCount(arrItems);
	Managed_ExecSql(pDb, "BEGIN IMMEDIATE");
	for ( i = 0; i < iTotal; i++ ) {
		xvalue tblItem = xvoArrayGetValue(arrItems, i);
		xvalue tblData = NULL;
		xvalue tblRow = xvoCreateTable();
		str sError = NULL;
		str sTitle = NULL;
		str sPayloadJson = NULL;
		int iCategoryId = 0;
		int iStatus = 0;
		bool bDraft = FALSE;
		bool bOk = FALSE;
		bool bSkipped = FALSE;
		int64 iContentId = 0;
		int64 iImportContentId = 0;

		if ( tblItem && (xvoType(tblItem) == XVO_DT_TABLE) ) {
			tblData = Managed_ImportBuildData(tblItem, tblSpec, arrFields);
			iImportContentId = xvoTableGetInt(tblItem, "id", 2);
			if ( iImportContentId <= 0 ) iImportContentId = xvoTableGetInt(tblItem, "contentId", 9);
			bDraft = Managed_DraftEnabled(tblSpec) ? xvoTableGetBool(tblItem, "isDraft", 7) : FALSE;
			iCategoryId = (int)xvoTableGetInt(tblItem, "categoryId", 10);
			if ( iCategoryId <= 0 ) iCategoryId = (int)xvoTableGetInt(tblData, "categoryId", 10);
			Managed_ApplyMissingDefaults(tblData, tblSpec);
			Managed_NormalizeNullableFields(tblData, tblSpec);
			Managed_CoerceFieldValues(tblData, tblSpec);
			iStatus = Managed_NormalizeStatusForSave(tblSpec, bDraft, Managed_ExtractStatus(tblData, tblSpec));
			Managed_StoreStatusValue(tblData, tblSpec, iStatus);
			Managed_EnsurePublishedAtValue(tblData, tblSpec, bDraft, iStatus, iNow);
			bOk = Managed_ValidateData(tblSpec, tblData, bDraft, &sError);
			if ( bOk ) {
				sTitle = Managed_ExtractTitle(tblData, tblSpec);
				sPayloadJson = xrtStringifyJSON(tblData, FALSE, NULL);
				if ( bSkipExisting && Managed_ContentExists(pDb, iImportContentId) ) {
					iContentId = iImportContentId;
					iSuccess++;
					bSkipped = TRUE;
				} else if ( bUpdateExisting && Managed_ContentExists(pDb, iImportContentId) ) {
					if ( sqlite3_prepare_v2(pDb, "UPDATE content_item SET title=?,status=?,payload_json=?,category_id=?,is_draft=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
						sqlite3_bind_text(stmt, 1, sTitle ? (const char*)sTitle : "", -1, SQLITE_TRANSIENT);
						sqlite3_bind_int(stmt, 2, iStatus);
						sqlite3_bind_text(stmt, 3, sPayloadJson ? (const char*)sPayloadJson : "{}", -1, SQLITE_TRANSIENT);
						sqlite3_bind_int(stmt, 4, iCategoryId > 0 ? iCategoryId : 0);
						sqlite3_bind_int(stmt, 5, bDraft ? 1 : 0);
						sqlite3_bind_int64(stmt, 6, (sqlite3_int64)iNow);
						sqlite3_bind_int64(stmt, 7, (sqlite3_int64)iImportContentId);
						if ( sqlite3_step(stmt) == SQLITE_DONE ) {
							iContentId = iImportContentId;
							iSuccess++;
						} else {
							bOk = FALSE;
							sError = xrtCopyStr("update failed", 0);
						}
					} else {
						bOk = FALSE;
						sError = xrtCopyStr("prepare update failed", 0);
					}
				} else if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_item(title,status,payload_json,category_id,is_draft,create_time,update_time,delete_time) VALUES(?,?,?,?,?,?,?,0)", -1, &stmt, NULL) == SQLITE_OK ) {
					sqlite3_bind_text(stmt, 1, sTitle ? (const char*)sTitle : "", -1, SQLITE_TRANSIENT);
					sqlite3_bind_int(stmt, 2, iStatus);
					sqlite3_bind_text(stmt, 3, sPayloadJson ? (const char*)sPayloadJson : "{}", -1, SQLITE_TRANSIENT);
					sqlite3_bind_int(stmt, 4, iCategoryId > 0 ? iCategoryId : 0);
					sqlite3_bind_int(stmt, 5, bDraft ? 1 : 0);
					sqlite3_bind_int64(stmt, 6, (sqlite3_int64)iNow);
					sqlite3_bind_int64(stmt, 7, (sqlite3_int64)iNow);
					if ( sqlite3_step(stmt) == SQLITE_DONE ) {
						iContentId = sqlite3_last_insert_rowid(pDb);
						iSuccess++;
					} else {
						bOk = FALSE;
						sError = xrtCopyStr("insert failed", 0);
					}
				} else {
					bOk = FALSE;
					sError = xrtCopyStr("prepare insert failed", 0);
				}
				if ( stmt ) sqlite3_finalize(stmt);
				stmt = NULL;
				if ( (iContentId > 0) && !bSkipped ) {
					Managed_RevisionSnapshot(pDb, iContentId, sTitle, iStatus, iCategoryId > 0 ? iCategoryId : 0, bDraft, sPayloadJson, bUpdateExisting ? "import-update" : "import", iNow);
					Managed_MediaSyncRefs(pDb, iContentId, tblData, iNow);
					Managed_SeoSyncData(pDb, iContentId, sTitle, tblData, iNow);
					Managed_SearchIndexSyncData(pDb, tblSpec, iContentId, sTitle, iStatus, tblData, iNow);
					Managed_SitemapSyncData(pDb, tblSpec, iContentId, sTitle, iStatus, bDraft, tblData, iNow);
					Managed_RelatedSyncData(pDb, tblSpec, iContentId, iCategoryId > 0 ? iCategoryId : 0, iStatus, bDraft, iNow);
					Managed_AuditLogWithRequest(pDb, "content", iContentId, bUpdateExisting ? "content.import.update" : "content.import", sTitle ? (const char*)sTitle : "", sPayloadJson ? (const char*)sPayloadJson : "{}", objReq, objSession);
				}
				if ( sTitle ) xrtFree(sTitle);
				if ( sPayloadJson ) xrtFree(sPayloadJson);
			}
		} else {
			sError = xrtCopyStr("row is not an object", 0);
		}
		if ( !bOk ) iFail++;
		xvoTableSetInt(tblRow, "rowIndex", 8, i + 1);
		xvoTableSetBool(tblRow, "ok", 2, bOk);
		xvoTableSetInt(tblRow, "contentId", 9, iContentId);
		xvoTableSetText(tblRow, "message", 7, sError ? sError : (bSkipped ? (str)"skipped existing" : (str)"imported"), 0, FALSE);
		if ( !bOk && tblItem && (xvoType(tblItem) == XVO_DT_TABLE) ) xvoTableSetValue(tblRow, "data", 4, xvoCopy(tblItem), TRUE);
		xvoArrayAppendValue(arrReport, tblRow, TRUE);
		if ( tblData ) xvoUnref(tblData);
		if ( sError ) xrtFree(sError);
	}
	Managed_ExecSql(pDb, "COMMIT");
	sReportJson = xrtStringifyJSON(arrReport, FALSE, NULL);
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_import_job(source_name,status,total_count,success_count,fail_count,report_json,operator_id,create_time,finish_time) VALUES(?,'imported',?,?,?,?,?,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, Managed_IsBlank(sSourceName) ? "json-import" : sSourceName, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 2, iTotal);
		sqlite3_bind_int(stmt, 3, iSuccess);
		sqlite3_bind_int(stmt, 4, iFail);
		sqlite3_bind_text(stmt, 5, sReportJson ? (const char*)sReportJson : "[]", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 6, (sqlite3_int64)iOperatorId);
		sqlite3_bind_int64(stmt, 7, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 8, (sqlite3_int64)iNow);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_AuditLogWithRequest(pDb, "import_job", sqlite3_last_insert_rowid(pDb), "import.commit", Managed_IsBlank(sSourceName) ? "json-import" : sSourceName, sReportJson ? (const char*)sReportJson : "[]", objReq, objSession);
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "import finished");
	xvoTableSetInt(tblRet, "totalCount", 10, iTotal);
	xvoTableSetInt(tblRet, "successCount", 12, iSuccess);
	xvoTableSetInt(tblRet, "failCount", 9, iFail);
	xvoTableSetValue(tblRet, "data", 4, arrReport, TRUE);
	if ( tblBody ) xvoUnref(tblBody);
	if ( tblSpec ) xvoUnref(tblSpec);
	if ( sReportJson ) xrtFree(sReportJson);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestImportJobListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.import-export") ) {
		xvoUnref(tblRet); xvoUnref(arrList);
		Managed_SendError(objResp, "import-export ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblRet); xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id,source_name,status,total_count,success_count,fail_count,report_json,operator_id,create_time,finish_time FROM content_import_job ORDER BY create_time DESC,id DESC LIMIT 200", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) Managed_AppendImportJobRow(arrList, stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestImportReplayAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	xvalue tblRet = Managed_CreateResult(TRUE, "replay rows loaded");
	xvalue arrItems = xvoCreateArray();
	xvalue arrReport = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	str sReportJson = NULL;
	int64 iJobId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	int i;
	int iCount = 0;

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.import-export") ) {
		if ( tblBody ) xvoUnref(tblBody);
		xvoUnref(tblRet);
		xvoUnref(arrItems);
		Managed_SendError(objResp, "import-export ability pack is not enabled");
		return;
	}
	if ( iJobId <= 0 ) {
		if ( tblBody ) xvoUnref(tblBody);
		xvoUnref(tblRet);
		xvoUnref(arrItems);
		Managed_SendError(objResp, "import job id is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( tblBody ) xvoUnref(tblBody);
		xvoUnref(tblRet);
		xvoUnref(arrItems);
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT report_json FROM content_import_job WHERE id=? LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iJobId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			str sText = (str)sqlite3_column_text(stmt, 0);
			sReportJson = xrtCopyStr(sText, sText ? strlen((const char*)sText) : 0);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	if ( Managed_IsBlank(sReportJson) ) {
		if ( tblBody ) xvoUnref(tblBody);
		xvoUnref(tblRet);
		xvoUnref(arrItems);
		if ( sReportJson ) xrtFree(sReportJson);
		Managed_SendError(objResp, "import job report is empty");
		return;
	}
	arrReport = xrtParseJSON(sReportJson, strlen((const char*)sReportJson));
	if ( arrReport && (xvoType(arrReport) == XVO_DT_ARRAY) ) {
		iCount = (int)xvoArrayItemCount(arrReport);
		for ( i = 0; i < iCount; i++ ) {
			xvalue tblRow = xvoArrayGetValue(arrReport, i);
			xvalue tblData = (tblRow && (xvoType(tblRow) == XVO_DT_TABLE)) ? xvoTableGetValue(tblRow, "data", 4) : NULL;
			if ( tblRow && (xvoType(tblRow) == XVO_DT_TABLE) && !xvoTableGetBool(tblRow, "ok", 2) && tblData && (xvoType(tblData) == XVO_DT_TABLE) ) {
				xvoArrayAppendValue(arrItems, xvoCopy(tblData), TRUE);
			}
		}
	}
	xvoTableSetInt(tblRet, "jobId", 5, iJobId);
	xvoTableSetInt(tblRet, "failedCount", 11, xvoArrayItemCount(arrItems));
	xvoTableSetValue(tblRet, "items", 5, arrItems, TRUE);
	if ( tblBody ) xvoUnref(tblBody);
	if ( arrReport ) xvoUnref(arrReport);
	if ( sReportJson ) xrtFree(sReportJson);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestExportJsonAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	xvalue arrFields = tblBody ? xvoTableGetValue(tblBody, "fields", 6) : NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblSpec = Managed_LoadSpec();
	xvalue arrItems = xvoCreateArray();
	xvalue tblRet = NULL;
	str sResultJson = NULL;
	int64 iNow = xrtNow();
	int64 iOperatorId = (objSession && (xvoType(objSession) == XVO_DT_TABLE)) ? xvoTableGetInt(objSession, "id", 2) : 0;
	int iTotal = 0;
	int iExported = 0;
	int iLimit = tblBody ? (int)xvoTableGetInt(tblBody, "limit", 5) : 0;
	int iOffset = tblBody ? (int)xvoTableGetInt(tblBody, "offset", 6) : 0;
	bool bChunked = iLimit > 0;

	(void)objServer; (void)objHost;
	if ( iLimit < 0 ) iLimit = 0;
	if ( iLimit > 1000 ) iLimit = 1000;
	if ( iOffset < 0 ) iOffset = 0;
	if ( !Managed_AbilityPackMounted("content.import-export") ) {
		if ( tblBody ) xvoUnref(tblBody);
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(arrItems);
		Managed_SendError(objResp, "import-export ability pack is not enabled");
		return;
	}
	if ( (tblSpec == NULL) || !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(arrItems);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT COUNT(*) FROM content_item WHERE delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) iTotal = sqlite3_column_int(stmt, 0);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( sqlite3_prepare_v2(pDb,
		bChunked
			? "SELECT id,title,status,payload_json,is_draft,create_time,update_time,category_id FROM content_item WHERE delete_time=0 ORDER BY id ASC LIMIT ? OFFSET ?"
			: "SELECT id,title,status,payload_json,is_draft,create_time,update_time,category_id FROM content_item WHERE delete_time=0 ORDER BY id ASC",
		-1, &stmt, NULL) == SQLITE_OK ) {
		if ( bChunked ) {
			sqlite3_bind_int(stmt, 1, iLimit);
			sqlite3_bind_int(stmt, 2, iOffset);
		}
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblItem = Managed_CreateItemFromStmt(stmt, tblSpec);
			xvalue tblData = Managed_ImportBuildData(tblItem, tblSpec, arrFields);
			xvoTableSetInt(tblData, "id", 2, xvoTableGetInt(tblItem, "id", 2));
			xvoTableSetInt(tblData, "status", 6, xvoTableGetInt(tblItem, "status", 6));
			xvoTableSetBool(tblData, "isDraft", 7, xvoTableGetBool(tblItem, "isDraft", 7));
			xvoTableSetInt(tblData, "categoryId", 10, xvoTableGetInt(tblItem, "categoryId", 10));
			xvoArrayAppendValue(arrItems, tblData, TRUE);
			if ( tblItem ) xvoUnref(tblItem);
			iExported++;
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	sResultJson = xrtStringifyJSON(arrItems, FALSE, NULL);
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_export_job(export_type,status,total_count,filter_json,result_json,operator_id,create_time,finish_time) VALUES('json','finished',?,'{}',?,?,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, bChunked ? iExported : iTotal);
		sqlite3_bind_text(stmt, 2, sResultJson ? (const char*)sResultJson : "[]", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iOperatorId);
		sqlite3_bind_int64(stmt, 4, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 5, (sqlite3_int64)iNow);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_AuditLogWithRequest(pDb, "export_job", sqlite3_last_insert_rowid(pDb), "export.json", "content json export", sResultJson ? (const char*)sResultJson : "[]", objReq, objSession);
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "export finished");
	xvoTableSetInt(tblRet, "totalCount", 10, iTotal);
	xvoTableSetInt(tblRet, "exportedCount", 13, iExported);
	xvoTableSetInt(tblRet, "offset", 6, iOffset);
	xvoTableSetInt(tblRet, "limit", 5, iLimit);
	xvoTableSetInt(tblRet, "nextOffset", 10, iOffset + iExported);
	xvoTableSetBool(tblRet, "hasMore", 7, (iOffset + iExported) < iTotal ? TRUE : FALSE);
	xvoTableSetValue(tblRet, "data", 4, arrItems, TRUE);
	if ( tblBody ) xvoUnref(tblBody);
	if ( tblSpec ) xvoUnref(tblSpec);
	if ( sResultJson ) xrtFree(sResultJson);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestExportJobListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.import-export") ) {
		xvoUnref(tblRet); xvoUnref(arrList);
		Managed_SendError(objResp, "import-export ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblRet); xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id,export_type,status,total_count,filter_json,result_json,operator_id,create_time,finish_time FROM content_export_job ORDER BY create_time DESC,id DESC LIMIT 200", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) Managed_AppendExportJobRow(arrList, stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestExportDownloadAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sId[32];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	str sHeader = NULL;
	const char* sJson = NULL;
	int64 iId = 0;

	(void)objServer; (void)objHost; (void)objSession;
	memset(sId, 0, sizeof(sId));
	xsReqQueryValue(objReq, "id", sId, sizeof(sId));
	iId = atoll(sId);
	if ( !Managed_AbilityPackMounted("content.import-export") ) {
		Managed_SendError(objResp, "import-export ability pack is not enabled");
		return;
	}
	if ( (iId <= 0) || !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "export job id is required");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT result_json FROM content_export_job WHERE id=? AND status='finished' LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			sJson = (const char*)sqlite3_column_text(stmt, 0);
			sHeader = xrtFormat("Content-Type: application/json; charset=utf-8\r\nContent-Disposition: attachment; filename=\"content-export-%lld.json\"\r\nCache-Control: no-store\r\n", (long long)iId);
			xsHttpReplyAuto(objResp, 200, sHeader ? (const char*)sHeader : "Content-Type: application/json; charset=utf-8\r\n", sJson ? sJson : "[]", sJson ? strlen(sJson) : 2);
		} else {
			Managed_SendError(objResp, "export job not found");
		}
	} else {
		Managed_SendError(objResp, "failed to load export job");
	}
	if ( sHeader ) xrtFree(sHeader);
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
}

void Managed_RequestDetailCommon(XS_ResponseObject objResp, XS_RequestObject objReq, xvalue objSession, bool bAdmin)
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
	bool bAttachTag = FALSE;
	bool bAttachTopic = FALSE;
	bool bAttachComment = FALSE;
	bool bAttachLike = FALSE;
	bool bAttachView = FALSE;
	bool bAttachMedia = FALSE;
	bool bAttachRelated = FALSE;
	bool bAttachAccess = FALSE;

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
	if ( (sSlug[0] != '\0') && !Managed_AbilityPackMounted("content.slug") ) {
		if ( sSqlById ) xrtFree(sSqlById);
		if ( sSqlScan ) xrtFree(sSqlScan);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "slug capability is not enabled");
		return;
	}
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
		if ( sSqlById ) xrtFree(sSqlById);
		if ( sSqlScan ) xrtFree(sSqlScan);
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
	if ( tblData ) {
		bAttachTag = Managed_AbilityPackMounted("content.tag");
		bAttachTopic = Managed_AbilityPackMounted("content.topic");
		bAttachComment = Managed_AbilityPackMounted("content.comment");
		bAttachLike = Managed_AbilityPackMounted("content.like");
		bAttachView = Managed_AbilityPackMounted("content.view-stat");
		bAttachMedia = Managed_AbilityPackMounted("content.media");
		bAttachRelated = Managed_AbilityPackMounted("content.related");
		bAttachAccess = Managed_AbilityPackMounted("content.access");
		Managed_AttachAbilityListFields(pDb, tblData, bAttachTag, bAttachTopic, bAttachComment, bAttachLike, bAttachView, bAttachMedia);
		if ( bAttachAccess ) {
			Managed_AttachAccessInfo(pDb, tblData, objReq, objSession, bAdmin);
			if ( !Managed_AccessCheckRule(pDb, xvoTableGetInt(tblData, "id", 2), objReq, objSession, bAdmin, NULL) ) {
				Managed_CloseDb(pDb);
				if ( sSqlById ) xrtFree(sSqlById);
				if ( sSqlScan ) xrtFree(sSqlScan);
				if ( tblSpec ) xvoUnref(tblSpec);
				if ( tblData ) xvoUnref(tblData);
				Managed_SendError(objResp, "content access denied");
				return;
			}
		}
		Managed_AttachRelatedFields(pDb, tblData, tblSpec, bAttachRelated);
	}
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
	Managed_RequestDetailCommon(objResp, objReq, objSession, FALSE);
}

void Managed_RequestSlugResolvePublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sSlug[160];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblSpec = Managed_LoadSpec();
	xvalue tblItem = NULL;
	xvalue tblRet = NULL;
	xvalue tblData = NULL;
	str sSql = NULL;

	(void)objServer;
	(void)objHost;
	(void)objSession;
	memset(sSlug, 0, sizeof(sSlug));
	xsReqQueryValue(objReq, "slug", sSlug, sizeof(sSlug));
	if ( Managed_IsBlank(sSlug) ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "slug is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	sSql = xrtFormat("SELECT id, title, status, payload_json, is_draft, create_time, update_time, category_id FROM content_item WHERE delete_time = 0 AND is_draft = 0 AND status >= %d ORDER BY update_time DESC, id DESC", Managed_PublicStatusThreshold(tblSpec));
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblCandidate = Managed_CreateItemFromStmt(stmt, tblSpec);
			if ( Managed_ItemMatchesSlug(tblCandidate, sSlug) ) {
				tblItem = tblCandidate;
				break;
			}
			if ( tblCandidate ) xvoUnref(tblCandidate);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	if ( sSql ) xrtFree(sSql);
	if ( tblSpec ) xvoUnref(tblSpec);
	if ( tblItem == NULL ) {
		Managed_SendError(objResp, "slug not found");
		return;
	}
	tblData = xvoCreateTable();
	xvoTableSetInt(tblData, "id", 2, xvoTableGetInt(tblItem, "id", 2));
	xvoTableSetText(tblData, "title", 5, (str)Managed_ItemTextOrEmpty(tblItem, "title"), 0, FALSE);
	xvoTableSetText(tblData, "slug", 4, (str)Managed_ItemTextOrEmpty(tblItem, "slug"), 0, FALSE);
	xvoTableSetText(tblData, "url", 3, xrtFormat("/plugin/{{PLUGIN_XID}}?slug=%s", Managed_ItemTextOrEmpty(tblItem, "slug")), 0, TRUE);
	tblRet = Managed_CreateResult(TRUE, NULL);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	xvoUnref(tblItem);
	Managed_SendJsonValue(objResp, tblRet);
}

str Managed_SlugBuildRiskWarning(const char* sSlug)
{
	if ( Managed_IsBlank(sSlug) ) return NULL;
	if ( strcmp(sSlug, "admin") == 0 || strncmp(sSlug, "admin/", 6) == 0 || strcmp(sSlug, "api") == 0 || strncmp(sSlug, "api/", 4) == 0 ) {
		return xrtCopyStr("slug overlaps admin/API prefix; submit-time warning only, route hot path is unchanged", 0);
	}
	if ( strcmp(sSlug, "css") == 0 || strncmp(sSlug, "css/", 4) == 0 || strcmp(sSlug, "js") == 0 || strncmp(sSlug, "js/", 3) == 0 || strcmp(sSlug, "img") == 0 || strncmp(sSlug, "img/", 4) == 0 || strcmp(sSlug, "res") == 0 || strncmp(sSlug, "res/", 4) == 0 || strcmp(sSlug, "static") == 0 || strncmp(sSlug, "static/", 7) == 0 || strcmp(sSlug, "uploads") == 0 || strncmp(sSlug, "uploads/", 8) == 0 || strcmp(sSlug, "plugin-static") == 0 || strncmp(sSlug, "plugin-static/", 14) == 0 ) {
		return xrtCopyStr("slug looks like a static resource prefix; verify URL rules before enabling dynamic/pretty routing", 0);
	}
	return NULL;
}

void Managed_RequestSlugCheckAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sSlug[160];
	char sId[32];
	sqlite3* pDb = NULL;
	xvalue tblSpec = NULL;
	xvalue tblRet = NULL;
	xvalue tblData = NULL;
	str sRiskWarning = NULL;
	int64 iConflictId = 0;
	int64 iExcludeId = 0;
	bool bExists = FALSE;

	(void)objServer;
	(void)objHost;
	(void)objSession;
	memset(sSlug, 0, sizeof(sSlug));
	memset(sId, 0, sizeof(sId));
	xsReqQueryValue(objReq, "slug", sSlug, sizeof(sSlug));
	xsReqQueryValue(objReq, "id", sId, sizeof(sId));
	iExcludeId = sId[0] ? atoll(sId) : 0;
	if ( !Managed_AbilityPackMounted("content.slug") ) {
		Managed_SendError(objResp, "slug ability pack is not enabled");
		return;
	}
	if ( Managed_IsBlank(sSlug) ) {
		Managed_SendError(objResp, "slug is required");
		return;
	}
	tblSpec = Managed_LoadSpec();
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	bExists = Managed_SlugExists(pDb, tblSpec, sSlug, iExcludeId, &iConflictId);
	Managed_CloseDb(pDb);
	if ( tblSpec ) xvoUnref(tblSpec);

	tblData = xvoCreateTable();
	xvoTableSetText(tblData, "slug", 4, sSlug, 0, FALSE);
	xvoTableSetBool(tblData, "available", 9, bExists ? FALSE : TRUE);
	xvoTableSetInt(tblData, "conflictId", 10, iConflictId);
	sRiskWarning = Managed_SlugBuildRiskWarning(sSlug);
	if ( sRiskWarning ) {
		xvoTableSetText(tblData, "warning", 7, sRiskWarning, 0, FALSE);
		xrtFree(sRiskWarning);
	}
	tblRet = Managed_CreateResult(TRUE, NULL);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestSlugPreviewAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sSlug[160];
	char sId[32];
	sqlite3* pDb = NULL;
	xvalue tblSpec = NULL;
	xvalue tblRet = NULL;
	xvalue tblData = NULL;
	str sRiskWarning = NULL;
	int64 iConflictId = 0;
	int64 iExcludeId = 0;
	bool bExists = FALSE;

	(void)objServer;
	(void)objHost;
	(void)objSession;
	memset(sSlug, 0, sizeof(sSlug));
	memset(sId, 0, sizeof(sId));
	xsReqQueryValue(objReq, "slug", sSlug, sizeof(sSlug));
	xsReqQueryValue(objReq, "id", sId, sizeof(sId));
	iExcludeId = sId[0] ? atoll(sId) : 0;
	if ( !Managed_AbilityPackMounted("content.slug") ) {
		Managed_SendError(objResp, "slug ability pack is not enabled");
		return;
	}
	if ( Managed_IsBlank(sSlug) ) {
		Managed_SendError(objResp, "slug is required");
		return;
	}
	tblSpec = Managed_LoadSpec();
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	bExists = Managed_SlugExists(pDb, tblSpec, sSlug, iExcludeId, &iConflictId);
	Managed_CloseDb(pDb);
	if ( tblSpec ) xvoUnref(tblSpec);

	tblData = xvoCreateTable();
	xvoTableSetText(tblData, "slug", 4, sSlug, 0, FALSE);
	xvoTableSetText(tblData, "canonicalUrl", 12, xrtFormat("/plugin/{{PLUGIN_XID}}?slug=%s", sSlug), 0, TRUE);
	xvoTableSetText(tblData, "resolveApi", 10, xrtFormat("/api/plugin/{{PLUGIN_XID}}/slug/resolve?slug=%s", sSlug), 0, TRUE);
	xvoTableSetBool(tblData, "available", 9, bExists ? FALSE : TRUE);
	xvoTableSetInt(tblData, "conflictId", 10, iConflictId);
	sRiskWarning = Managed_SlugBuildRiskWarning(sSlug);
	if ( sRiskWarning ) {
		xvoTableSetText(tblData, "warning", 7, sRiskWarning, 0, FALSE);
		xrtFree(sRiskWarning);
	}
	tblRet = Managed_CreateResult(TRUE, NULL);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestSlugRepairAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblForm = NULL;
	xvalue tblSpec = NULL;
	xvalue tblRet = NULL;
	xvalue tblData = NULL;
	xvalue arrRows = NULL;
	const char* sSlugField = NULL;
	bool bConfirm = FALSE;
	int iLimit = 500;
	int64 iNow = xrtNow();
	int iTotal = 0;
	int iChanged = 0;
	int iSaved = 0;

	(void)objServer;
	(void)objHost;
	(void)objSession;
	if ( !Managed_AbilityPackMounted("content.slug") ) {
		Managed_SendError(objResp, "slug ability pack is not enabled");
		return;
	}
	tblForm = Managed_ParseJsonBody(objReq);
	if ( tblForm && (xvoType(tblForm) == XVO_DT_TABLE) ) {
		bConfirm = xvoTableGetBool(tblForm, "confirm", 7);
		iLimit = xvoTableGetInt(tblForm, "limit", 5);
		if ( iLimit <= 0 ) iLimit = 500;
		if ( iLimit > 2000 ) iLimit = 2000;
	}
	tblSpec = Managed_LoadSpec();
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblSpec ) xvoUnref(tblSpec);
		if ( tblForm ) xvoUnref(tblForm);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	sSlugField = Managed_GetSlugField(tblSpec);
	arrRows = xvoCreateArray();
	if ( bConfirm ) Managed_ExecSql(pDb, "BEGIN");
	if ( sqlite3_prepare_v2(pDb, "SELECT id,title,status,payload_json,is_draft,create_time,update_time,category_id FROM content_item WHERE delete_time=0 ORDER BY id ASC LIMIT ?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iLimit);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			int64 iContentId = sqlite3_column_int64(stmt, 0);
			const char* sTitle = (const char*)sqlite3_column_text(stmt, 1);
			const char* sPayloadJson = (const char*)sqlite3_column_text(stmt, 3);
			xvalue tblItem = Managed_CreateItemFromStmt(stmt, tblSpec);
			xvalue tblRow = xvoCreateTable();
			str sOldSlug = Managed_ExtractSlugValue(tblItem, tblSpec);
			str sNewSlug = NULL;
			str sRiskWarning = NULL;
			int64 iConflictId = 0;
			bool bMissing = Managed_IsBlank((const char*)sOldSlug);
			bool bConflict = (!bMissing) && Managed_SlugExists(pDb, tblSpec, (const char*)sOldSlug, iContentId, &iConflictId);
			bool bChanged = FALSE;
			bool bSaved = FALSE;

			iTotal++;
			if ( bMissing || bConflict ) {
				sNewSlug = Managed_BuildRepairSlug(pDb, tblSpec, bMissing ? NULL : (const char*)sOldSlug, iContentId, bConflict ? TRUE : FALSE);
				bChanged = !Managed_IsBlank((const char*)sNewSlug) && (bMissing || strcmp((const char*)sOldSlug, (const char*)sNewSlug) != 0);
			}
			if ( bChanged ) {
				iChanged++;
				if ( bConfirm && Managed_UpdateContentSlugPayload(pDb, iContentId, sPayloadJson, sSlugField, (const char*)sNewSlug, iNow) ) {
					bSaved = TRUE;
					iSaved++;
					if ( !bMissing ) {
						Managed_SlugHistoryInsert(pDb, iContentId, (const char*)sOldSlug, (const char*)sNewSlug, iNow);
						Managed_SlugRedirectSync(pDb, iContentId, (const char*)sOldSlug, (const char*)sNewSlug, iNow);
					}
				}
			}
			xvoTableSetInt(tblRow, "contentId", 9, iContentId);
			xvoTableSetText(tblRow, "title", 5, (str)(sTitle ? sTitle : ""), 0, FALSE);
			xvoTableSetText(tblRow, "oldSlug", 7, sOldSlug ? sOldSlug : (str)"", 0, FALSE);
			xvoTableSetText(tblRow, "newSlug", 7, sNewSlug ? sNewSlug : (sOldSlug ? sOldSlug : (str)""), 0, FALSE);
			xvoTableSetText(tblRow, "action", 6, (str)(bMissing ? "fill" : (bConflict ? "dedupe" : "keep")), 0, FALSE);
			xvoTableSetBool(tblRow, "changed", 7, bChanged);
			xvoTableSetBool(tblRow, "saved", 5, bSaved);
			xvoTableSetInt(tblRow, "conflictId", 10, iConflictId);
			sRiskWarning = Managed_SlugBuildRiskWarning(sNewSlug ? (const char*)sNewSlug : (sOldSlug ? (const char*)sOldSlug : ""));
			if ( sRiskWarning ) {
				xvoTableSetText(tblRow, "warning", 7, sRiskWarning, 0, FALSE);
				xrtFree(sRiskWarning);
			}
			xvoArrayAppendValue(arrRows, tblRow, TRUE);
			if ( sOldSlug ) xrtFree(sOldSlug);
			if ( sNewSlug ) xrtFree(sNewSlug);
			if ( tblItem ) xvoUnref(tblItem);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	if ( bConfirm ) Managed_ExecSql(pDb, "COMMIT");
	Managed_CloseDb(pDb);
	if ( tblSpec ) xvoUnref(tblSpec);
	if ( tblForm ) xvoUnref(tblForm);

	tblData = xvoCreateTable();
	xvoTableSetBool(tblData, "confirm", 7, bConfirm);
	xvoTableSetInt(tblData, "total", 5, iTotal);
	xvoTableSetInt(tblData, "changed", 7, iChanged);
	xvoTableSetInt(tblData, "saved", 5, iSaved);
	xvoTableSetInt(tblData, "limit", 5, iLimit);
	xvoTableSetValue(tblData, "rows", 4, arrRows, TRUE);
	tblRet = Managed_CreateResult(TRUE, NULL);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_AppendSlugHistoryRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblItem = xvoCreateTable();

	xvoTableSetInt(tblItem, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblItem, "contentId", 9, sqlite3_column_int64(stmt, 1));
	xvoTableSetText(tblItem, "oldSlug", 7, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetText(tblItem, "newSlug", 7, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetInt(tblItem, "status", 6, sqlite3_column_int(stmt, 4));
	xvoTableSetInt(tblItem, "createTime", 10, sqlite3_column_int64(stmt, 5));
	Managed_SetTimeText(tblItem, "createTimeText", 14, sqlite3_column_int64(stmt, 5));
	xvoArrayAppendValue(arrList, tblItem, TRUE);
}

void Managed_RequestSlugHistoryAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sContentId[32];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();
	int64 iContentId = 0;

	(void)objServer;
	(void)objHost;
	(void)objSession;
	memset(sContentId, 0, sizeof(sContentId));
	xsReqQueryValue(objReq, "contentId", sContentId, sizeof(sContentId));
	iContentId = sContentId[0] ? atoll(sContentId) : 0;
	if ( !Managed_AbilityPackMounted("content.slug") ) {
		xvoUnref(tblRet); xvoUnref(arrList);
		Managed_SendError(objResp, "slug ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblRet); xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, iContentId > 0
		? "SELECT id,content_id,old_slug,new_slug,status,create_time FROM content_slug_history WHERE content_id=? ORDER BY create_time DESC,id DESC LIMIT 200"
		: "SELECT id,content_id,old_slug,new_slug,status,create_time FROM content_slug_history ORDER BY create_time DESC,id DESC LIMIT 200", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( iContentId > 0 ) sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendSlugHistoryRow(arrList, stmt);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestRedirectResolvePublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sPath[512];
	char sUrl[512];
	char sRedirect[16];
	char sSource[768];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblData = NULL;
	xvalue tblRet = NULL;
	int64 iId = 0;
	int64 iNow = xrtNow();
	bool bHttpRedirect = FALSE;

	(void)objServer;
	(void)objHost;
	(void)objSession;
	memset(sPath, 0, sizeof(sPath));
	memset(sUrl, 0, sizeof(sUrl));
	memset(sRedirect, 0, sizeof(sRedirect));
	Managed_ReadTextQuery(objReq, "path", sPath, sizeof(sPath));
	Managed_ReadTextQuery(objReq, "url", sUrl, sizeof(sUrl));
	Managed_ReadTextQuery(objReq, "redirect", sRedirect, sizeof(sRedirect));
	if ( sRedirect[0] == '\0' ) Managed_ReadTextQuery(objReq, "go", sRedirect, sizeof(sRedirect));
	bHttpRedirect = (strcmp(sRedirect, "1") == 0) || (strcmp(sRedirect, "true") == 0);
	if ( !Managed_NormalizeRedirectPath(!Managed_IsBlank(sPath) ? sPath : sUrl, sSource, sizeof(sSource)) ) {
		Managed_SendError(objResp, "path is required");
		return;
	}
	if ( !Managed_AbilityPackMounted("content.redirect") ) {
		Managed_SendError(objResp, "redirect ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id,source_path,target_url,status_code,hit_count,last_hit_time FROM content_redirect WHERE source_path=? AND status=1 AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sSource, -1, SQLITE_TRANSIENT);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iId = sqlite3_column_int64(stmt, 0);
			tblData = xvoCreateTable();
			xvoTableSetInt(tblData, "id", 2, iId);
			xvoTableSetText(tblData, "sourcePath", 10, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
			xvoTableSetText(tblData, "targetUrl", 9, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
			xvoTableSetInt(tblData, "statusCode", 10, sqlite3_column_int(stmt, 3));
			xvoTableSetInt(tblData, "hitCount", 8, sqlite3_column_int64(stmt, 4) + 1);
			xvoTableSetInt(tblData, "lastHitTime", 11, iNow);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( iId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE content_redirect SET hit_count=hit_count+1,last_hit_time=?,update_time=? WHERE id=?", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, iNow);
			sqlite3_bind_int64(stmt, 2, iNow);
			sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
			sqlite3_step(stmt);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	if ( tblData == NULL ) {
		Managed_SendError(objResp, "redirect rule not found");
		return;
	}
	if ( bHttpRedirect ) {
		const char* sTarget = (const char*)xvoTableGetText(tblData, "targetUrl", 9);
		int iStatusCode = xvoTableGetInt(tblData, "statusCode", 10);
		str sHeader = NULL;

		if ( (iStatusCode != 301) && (iStatusCode != 302) ) iStatusCode = 302;
		if ( !Managed_RedirectLocationSafe(sTarget) ) {
			xvoUnref(tblData);
			Managed_SendError(objResp, "invalid redirect target");
			return;
		}
		sHeader = xrtFormat("Location: %s\r\nContent-Type: text/plain; charset=utf-8\r\nCache-Control: no-store\r\n", sTarget);
		xsHttpReplyAuto(objResp, iStatusCode, sHeader ? (const char*)sHeader : "Content-Type: text/plain; charset=utf-8\r\n", "Redirecting", 0);
		if ( sHeader ) xrtFree(sHeader);
		xvoUnref(tblData);
		return;
	}
	tblRet = Managed_CreateResult(TRUE, NULL);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_AppendRedirectRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblItem = xvoCreateTable();

	if ( (arrList == NULL) || (stmt == NULL) || (tblItem == NULL) ) {
		if ( tblItem ) xvoUnref(tblItem);
		return;
	}
	xvoTableSetInt(tblItem, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetText(tblItem, "sourcePath", 10, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
	xvoTableSetText(tblItem, "targetUrl", 9, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetInt(tblItem, "statusCode", 10, sqlite3_column_int(stmt, 3));
	xvoTableSetInt(tblItem, "hitCount", 8, sqlite3_column_int64(stmt, 4));
	xvoTableSetInt(tblItem, "lastHitTime", 11, sqlite3_column_int64(stmt, 5));
	xvoTableSetInt(tblItem, "status", 6, sqlite3_column_int(stmt, 6));
	xvoTableSetInt(tblItem, "createTime", 10, sqlite3_column_int64(stmt, 7));
	xvoTableSetInt(tblItem, "updateTime", 10, sqlite3_column_int64(stmt, 8));
	Managed_SetTimeText(tblItem, "lastHitTimeText", 15, sqlite3_column_int64(stmt, 5));
	Managed_SetTimeText(tblItem, "createTimeText", 14, sqlite3_column_int64(stmt, 7));
	Managed_SetTimeText(tblItem, "updateTimeText", 14, sqlite3_column_int64(stmt, 8));
	xvoArrayAppendValue(arrList, tblItem, TRUE);
}

bool Managed_RedirectSourceExists(sqlite3* pDb, const char* sSource, int64 iExcludeId)
{
	sqlite3_stmt* stmt = NULL;
	bool bExists = FALSE;

	if ( (pDb == NULL) || Managed_IsBlank(sSource) ) {
		return FALSE;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id FROM content_redirect WHERE source_path=? AND delete_time=0 AND id<>? LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sSource, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iExcludeId);
		bExists = (sqlite3_step(stmt) == SQLITE_ROW);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return bExists;
}

bool Managed_RedirectValidateRule(const char* sSourceIn, const char* sTargetUrl, char* sSourceOut, int iSourceOutSize, str* psError)
{
	if ( !Managed_NormalizeRedirectPath(sSourceIn, sSourceOut, iSourceOutSize) ) {
		if ( psError ) *psError = xrtCopyStr("sourcePath is required", 0);
		return FALSE;
	}
	if ( Managed_IsBlank(sTargetUrl) ) {
		if ( psError ) *psError = xrtCopyStr("targetUrl is required", 0);
		return FALSE;
	}
	if ( strcmp(sSourceOut, sTargetUrl) == 0 ) {
		if ( psError ) *psError = xrtCopyStr("redirect loop is not allowed", 0);
		return FALSE;
	}
	return TRUE;
}

str Managed_RedirectBuildRiskWarning(const char* sSource)
{
	if ( Managed_IsBlank(sSource) ) return NULL;
	if ( strcmp(sSource, "/") == 0 ) {
		return xrtCopyStr("redirect sourcePath is site root; verify it will not hide the public home page", 0);
	}
	if ( strncmp(sSource, "/admin", 6) == 0 || strncmp(sSource, "/api", 4) == 0 ) {
		return xrtCopyStr("redirect sourcePath overlaps admin/API prefix; verify permission and route ownership before publishing", 0);
	}
	if ( strncmp(sSource, "/css", 4) == 0 || strncmp(sSource, "/js", 3) == 0 || strncmp(sSource, "/img", 4) == 0 || strncmp(sSource, "/res", 4) == 0 || strncmp(sSource, "/static", 7) == 0 || strncmp(sSource, "/uploads", 8) == 0 || strncmp(sSource, "/plugin-static", 14) == 0 ) {
		return xrtCopyStr("redirect sourcePath looks like a static resource path; submit-time warning only, request hot path is unchanged", 0);
	}
	return NULL;
}

bool Managed_RedirectUpsert(sqlite3* pDb, const char* sSource, const char* sTargetUrl, int iStatusCode, int iStatus, int64 iNow)
{
	sqlite3_stmt* stmt = NULL;
	bool bOk = FALSE;

	if ( (pDb == NULL) || Managed_IsBlank(sSource) || Managed_IsBlank(sTargetUrl) ) {
		return FALSE;
	}
	if ( (iStatusCode != 301) && (iStatusCode != 302) ) iStatusCode = 301;
	iStatus = iStatus ? 1 : 0;
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_redirect SET target_url=?,status_code=?,status=?,update_time=? WHERE source_path=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sTargetUrl, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 2, iStatusCode);
		sqlite3_bind_int(stmt, 3, iStatus);
		sqlite3_bind_int64(stmt, 4, iNow);
		sqlite3_bind_text(stmt, 5, sSource, -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		bOk = sqlite3_changes(pDb) > 0;
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( !bOk && (sqlite3_prepare_v2(pDb, "INSERT INTO content_redirect(source_path,target_url,status_code,status,create_time,update_time,delete_time) VALUES(?,?,?,?,?,?,0)", -1, &stmt, NULL) == SQLITE_OK) ) {
		sqlite3_bind_text(stmt, 1, sSource, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, sTargetUrl, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 3, iStatusCode);
		sqlite3_bind_int(stmt, 4, iStatus);
		sqlite3_bind_int64(stmt, 5, iNow);
		sqlite3_bind_int64(stmt, 6, iNow);
		bOk = (sqlite3_step(stmt) == SQLITE_DONE);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return bOk;
}

void Managed_RequestRedirectListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sPage[32];
	char sLimit[32];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();
	int iPage = 1;
	int iLimit = 20;
	int iOffset = 0;
	int iCount = 0;

	(void)objServer; (void)objHost; (void)objSession;
	Managed_ReadTextQuery(objReq, "page", sPage, sizeof(sPage));
	Managed_ReadTextQuery(objReq, "limit", sLimit, sizeof(sLimit));
	if ( atoi(sPage) > 0 ) iPage = atoi(sPage);
	if ( atoi(sLimit) > 0 ) iLimit = atoi(sLimit);
	if ( iLimit > 200 ) iLimit = 200;
	iOffset = (iPage - 1) * iLimit;
	if ( !Managed_AbilityPackMounted("content.redirect") ) {
		if ( tblRet ) xvoUnref(tblRet);
		if ( arrList ) xvoUnref(arrList);
		Managed_SendError(objResp, "redirect ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblRet ) xvoUnref(tblRet);
		if ( arrList ) xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT COUNT(*) FROM content_redirect WHERE delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) iCount = sqlite3_column_int(stmt, 0);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( sqlite3_prepare_v2(pDb, "SELECT id,source_path,target_url,status_code,hit_count,last_hit_time,status,create_time,update_time FROM content_redirect WHERE delete_time=0 ORDER BY id DESC LIMIT ? OFFSET ?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iLimit);
		sqlite3_bind_int(stmt, 2, iOffset);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendRedirectRow(arrList, stmt);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetInt(tblRet, "page", 4, iPage);
	xvoTableSetInt(tblRet, "pageSize", 8, iLimit);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestRedirectSaveAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	char sSource[768];
	const char* sSourceIn = tblBody ? xvoTableGetText(tblBody, "sourcePath", 10) : NULL;
	const char* sTargetUrl = tblBody ? xvoTableGetText(tblBody, "targetUrl", 9) : NULL;
	str sRuleError = NULL;
	str sRiskWarning = NULL;
	xvalue tblRet = NULL;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	int iStatusCode = tblBody ? xvoTableGetInt(tblBody, "statusCode", 10) : 301;
	int iStatus = tblBody ? xvoTableGetInt(tblBody, "status", 6) : 1;
	int64 iNow = xrtNow();

	(void)objServer; (void)objHost; (void)objSession;
	if ( tblBody == NULL ) {
		Managed_SendError(objResp, "invalid json body");
		return;
	}
	if ( !Managed_AbilityPackMounted("content.redirect") ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "redirect ability pack is not enabled");
		return;
	}
	if ( !Managed_RedirectValidateRule(sSourceIn, sTargetUrl, sSource, sizeof(sSource), &sRuleError) ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, sRuleError ? (const char*)sRuleError : "invalid redirect rule");
		if ( sRuleError ) xrtFree(sRuleError);
		return;
	}
	if ( (iStatusCode != 301) && (iStatusCode != 302) ) iStatusCode = 301;
	iStatus = iStatus ? 1 : 0;
	sRiskWarning = Managed_RedirectBuildRiskWarning(sSource);
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblBody);
		if ( sRiskWarning ) xrtFree(sRiskWarning);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( Managed_RedirectSourceExists(pDb, sSource, iId) ) {
		Managed_CloseDb(pDb);
		xvoUnref(tblBody);
		if ( sRiskWarning ) xrtFree(sRiskWarning);
		Managed_SendError(objResp, "sourcePath already exists");
		return;
	}
	if ( iId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE content_redirect SET source_path=?,target_url=?,status_code=?,status=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sSource, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, sTargetUrl, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 3, iStatusCode);
			sqlite3_bind_int(stmt, 4, iStatus);
			sqlite3_bind_int64(stmt, 5, iNow);
			sqlite3_bind_int64(stmt, 6, (sqlite3_int64)iId);
			sqlite3_step(stmt);
		}
	} else {
		if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_redirect(source_path,target_url,status_code,status,create_time,update_time,delete_time) VALUES(?,?,?,?,?,?,0)", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sSource, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, sTargetUrl, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 3, iStatusCode);
			sqlite3_bind_int(stmt, 4, iStatus);
			sqlite3_bind_int64(stmt, 5, iNow);
			sqlite3_bind_int64(stmt, 6, iNow);
			sqlite3_step(stmt);
			iId = sqlite3_last_insert_rowid(pDb);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoUnref(tblBody);
	tblRet = Managed_CreateResult(iId > 0, iId > 0 ? "ok" : "save failed");
	if ( tblRet && sRiskWarning ) {
		xvoTableSetText(tblRet, "warning", 7, sRiskWarning, 0, FALSE);
	}
	if ( sRiskWarning ) xrtFree(sRiskWarning);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestRedirectImportAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	xvalue arrItems = tblBody ? xvoTableGetValue(tblBody, "items", 5) : NULL;
	xvalue arrReport = xvoCreateArray();
	xvalue tblRet = NULL;
	sqlite3* pDb = NULL;
	bool bConfirm = tblBody ? xvoTableGetBool(tblBody, "confirm", 7) : FALSE;
	int64 iNow = xrtNow();
	int iTotal = 0;
	int iValid = 0;
	int iSaved = 0;
	int iFail = 0;
	int i;

	(void)objServer; (void)objHost; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.redirect") ) {
		if ( tblBody ) xvoUnref(tblBody);
		xvoUnref(arrReport);
		Managed_SendError(objResp, "redirect ability pack is not enabled");
		return;
	}
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) || (tblBody == NULL) || (arrItems == NULL) || (xvoType(arrItems) != XVO_DT_ARRAY) ) {
		if ( tblBody ) xvoUnref(tblBody);
		xvoUnref(arrReport);
		Managed_SendError(objResp, "items array is required");
		return;
	}
	if ( bConfirm && (!Managed_EnsureSchema() || !Managed_OpenDb(&pDb)) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblBody);
		xvoUnref(arrReport);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	iTotal = (int)xvoArrayItemCount(arrItems);
	for ( i = 0; i < iTotal; i++ ) {
		xvalue tblItem = xvoArrayGetValue(arrItems, i);
		xvalue tblRow = xvoCreateTable();
		char sSource[768];
		const char* sSourceIn = ((tblItem != NULL) && (xvoType(tblItem) == XVO_DT_TABLE)) ? xvoTableGetText(tblItem, "sourcePath", 10) : NULL;
		const char* sTargetUrl = ((tblItem != NULL) && (xvoType(tblItem) == XVO_DT_TABLE)) ? xvoTableGetText(tblItem, "targetUrl", 9) : NULL;
		int iStatusCode = ((tblItem != NULL) && (xvoType(tblItem) == XVO_DT_TABLE)) ? xvoTableGetInt(tblItem, "statusCode", 10) : 301;
		int iStatus = ((tblItem != NULL) && (xvoType(tblItem) == XVO_DT_TABLE)) ? xvoTableGetInt(tblItem, "status", 6) : 1;
		str sError = NULL;
		str sWarning = NULL;
		bool bOk = FALSE;
		bool bSaved = FALSE;

		memset(sSource, 0, sizeof(sSource));
		if ( (iStatusCode != 301) && (iStatusCode != 302) ) iStatusCode = 301;
		bOk = ((tblItem != NULL) && (xvoType(tblItem) == XVO_DT_TABLE) && Managed_RedirectValidateRule(sSourceIn, sTargetUrl, sSource, sizeof(sSource), &sError));
		if ( bOk ) {
			sWarning = Managed_RedirectBuildRiskWarning(sSource);
			iValid++;
			if ( bConfirm ) {
				bSaved = Managed_RedirectUpsert(pDb, sSource, sTargetUrl, iStatusCode, iStatus, iNow);
				if ( bSaved ) iSaved++;
				else {
					iFail++;
					sError = xrtCopyStr("save failed", 0);
				}
			}
		} else {
			iFail++;
		}
		xvoTableSetInt(tblRow, "rowIndex", 8, i + 1);
		xvoTableSetText(tblRow, "sourcePath", 10, (str)(sSource[0] ? sSource : (sSourceIn ? sSourceIn : "")), 0, FALSE);
		xvoTableSetText(tblRow, "targetUrl", 9, (str)(sTargetUrl ? sTargetUrl : ""), 0, FALSE);
		xvoTableSetInt(tblRow, "statusCode", 10, iStatusCode);
		xvoTableSetBool(tblRow, "ok", 2, bOk);
		xvoTableSetBool(tblRow, "saved", 5, bSaved);
		xvoTableSetText(tblRow, "message", 7, sError ? sError : (str)(bConfirm ? "saved" : "preview ok"), 0, FALSE);
		if ( sWarning ) {
			xvoTableSetText(tblRow, "warning", 7, sWarning, 0, FALSE);
		}
		xvoArrayAppendValue(arrReport, tblRow, TRUE);
		if ( sError ) xrtFree(sError);
		if ( sWarning ) xrtFree(sWarning);
	}
	if ( pDb ) Managed_CloseDb(pDb);
	if ( tblBody ) xvoUnref(tblBody);
	tblRet = Managed_CreateResult(TRUE, bConfirm ? "import finished" : "preview finished");
	xvoTableSetInt(tblRet, "total", 5, iTotal);
	xvoTableSetInt(tblRet, "valid", 5, iValid);
	xvoTableSetInt(tblRet, "saved", 5, iSaved);
	xvoTableSetInt(tblRet, "failed", 6, iFail);
	xvoTableSetValue(tblRet, "data", 4, arrReport, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestRedirectDeleteAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	int64 iNow = xrtNow();

	(void)objServer; (void)objHost; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.redirect") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "redirect ability pack is not enabled");
		return;
	}
	if ( iId <= 0 ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "invalid redirect id");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_redirect SET delete_time=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, iNow);
		sqlite3_bind_int64(stmt, 2, iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	if ( tblBody ) xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "deleted"));
}

void Managed_AppendMediaRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblItem = xvoCreateTable();

	if ( (arrList == NULL) || (stmt == NULL) || (tblItem == NULL) ) {
		if ( tblItem ) xvoUnref(tblItem);
		return;
	}
	xvoTableSetInt(tblItem, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetText(tblItem, "attachmentXid", 13, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
	xvoTableSetText(tblItem, "title", 5, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetText(tblItem, "url", 3, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetText(tblItem, "mime", 4, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetText(tblItem, "ext", 3, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
	xvoTableSetInt(tblItem, "size", 4, sqlite3_column_int64(stmt, 6));
	xvoTableSetInt(tblItem, "width", 5, sqlite3_column_int(stmt, 7));
	xvoTableSetInt(tblItem, "height", 6, sqlite3_column_int(stmt, 8));
	xvoTableSetInt(tblItem, "status", 6, sqlite3_column_int(stmt, 9));
	xvoTableSetInt(tblItem, "createTime", 10, sqlite3_column_int64(stmt, 10));
	xvoTableSetInt(tblItem, "updateTime", 10, sqlite3_column_int64(stmt, 11));
	Managed_SetTimeText(tblItem, "createTimeText", 14, sqlite3_column_int64(stmt, 10));
	Managed_SetTimeText(tblItem, "updateTimeText", 14, sqlite3_column_int64(stmt, 11));
	if ( sqlite3_column_count(stmt) > 12 ) {
		xvoTableSetInt(tblItem, "refCount", 8, sqlite3_column_int64(stmt, 12));
	}
	xvoArrayAppendValue(arrList, tblItem, TRUE);
}

int64 Managed_MediaRefCount(sqlite3* pDb, int64 iMediaId)
{
	sqlite3_stmt* stmt = NULL;
	int64 iCount = 0;

	if ( (pDb == NULL) || (iMediaId <= 0) ) return 0;
	if ( sqlite3_prepare_v2(pDb, "SELECT COUNT(*) FROM content_media_ref WHERE media_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iMediaId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) iCount = sqlite3_column_int64(stmt, 0);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return iCount;
}

void Managed_MediaSyncRefOne(sqlite3* pDb, int64 iContentId, int64 iMediaId, const char* sRefType, int64 iNow)
{
	sqlite3_stmt* stmt = NULL;

	if ( (pDb == NULL) || (iContentId <= 0) || (iMediaId <= 0) || Managed_IsBlank(sRefType) ) return;
	if ( sqlite3_prepare_v2(pDb, "INSERT OR IGNORE INTO content_media_ref(media_id,content_id,ref_type,create_time) VALUES(?,?,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iMediaId);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iContentId);
		sqlite3_bind_text(stmt, 3, sRefType, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 4, iNow);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
}

void Managed_MediaSyncRefs(sqlite3* pDb, int64 iContentId, xvalue tblData, int64 iNow)
{
	sqlite3_stmt* stmt = NULL;
	int64 iCoverId = 0;
	xvalue objMediaIds = NULL;

	if ( !Managed_AbilityPackMounted("content.media") || (pDb == NULL) || (iContentId <= 0) || (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) ) return;
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM content_media_ref WHERE content_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	iCoverId = xvoTableGetInt(tblData, "cover_media_id", 14);
	if ( iCoverId <= 0 ) iCoverId = xvoTableGetInt(tblData, "coverMediaId", 12);
	if ( iCoverId > 0 ) Managed_MediaSyncRefOne(pDb, iContentId, iCoverId, "cover", iNow);
	objMediaIds = xvoTableGetValue(tblData, "media_ids", 9);
	if ( objMediaIds == NULL ) objMediaIds = xvoTableGetValue(tblData, "mediaIds", 8);
	if ( objMediaIds && (xvoType(objMediaIds) == XVO_DT_ARRAY) ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(objMediaIds); i++ ) {
			Managed_MediaSyncRefOne(pDb, iContentId, Managed_ValueToInt64(xvoArrayGetValue(objMediaIds, i)), "body", iNow);
		}
	} else if ( objMediaIds ) {
		char sBuf[1024];
		char* p;
		const char* sText = xvoTableGetText(tblData, "media_ids", 9);
		if ( sText == NULL ) sText = xvoTableGetText(tblData, "mediaIds", 8);
		snprintf(sBuf, sizeof(sBuf), "%s", sText ? sText : "");
		p = strtok(sBuf, ",");
		while ( p ) {
			Managed_MediaSyncRefOne(pDb, iContentId, atoll(p), "body", iNow);
			p = strtok(NULL, ",");
		}
	}
}

void Managed_RequestMediaListCommon(XS_ResponseObject objResp, bool bAdmin)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();
	const char* sSql = bAdmin
		? "SELECT m.id,m.attachment_xid,m.title,m.url,m.mime,m.ext,m.size,m.width,m.height,m.status,m.create_time,m.update_time,(SELECT COUNT(*) FROM content_media_ref r WHERE r.media_id=m.id) FROM content_media m WHERE m.delete_time=0 ORDER BY m.id DESC"
		: "SELECT m.id,m.attachment_xid,m.title,m.url,m.mime,m.ext,m.size,m.width,m.height,m.status,m.create_time,m.update_time,(SELECT COUNT(*) FROM content_media_ref r WHERE r.media_id=m.id) FROM content_media m WHERE m.status=1 AND m.delete_time=0 ORDER BY m.id DESC";

	if ( !Managed_AbilityPackMounted("content.media") ) {
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "media ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) Managed_AppendMediaRow(arrList, stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestMediaListPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	Managed_RequestMediaListCommon(objResp, FALSE);
}

void Managed_RequestMediaListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	Managed_RequestMediaListCommon(objResp, TRUE);
}

void Managed_AppendMediaRefRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblItem = xvoCreateTable();

	if ( (arrList == NULL) || (stmt == NULL) || (tblItem == NULL) ) {
		if ( tblItem ) xvoUnref(tblItem);
		return;
	}
	xvoTableSetInt(tblItem, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblItem, "mediaId", 7, sqlite3_column_int64(stmt, 1));
	xvoTableSetText(tblItem, "mediaTitle", 10, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetText(tblItem, "mediaUrl", 8, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetInt(tblItem, "contentId", 9, sqlite3_column_int64(stmt, 4));
	xvoTableSetText(tblItem, "contentTitle", 12, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
	xvoTableSetText(tblItem, "refType", 7, (str)sqlite3_column_text(stmt, 6), 0, FALSE);
	xvoTableSetInt(tblItem, "createTime", 10, sqlite3_column_int64(stmt, 7));
	Managed_SetTimeText(tblItem, "createTimeText", 14, sqlite3_column_int64(stmt, 7));
	xvoArrayAppendValue(arrList, tblItem, TRUE);
}

void Managed_RequestMediaRefListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sMediaId[32];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();
	int64 iMediaId = 0;
	const char* sSqlAll = "SELECT r.id,r.media_id,m.title,m.url,r.content_id,i.title,r.ref_type,r.create_time FROM content_media_ref r INNER JOIN content_media m ON m.id=r.media_id LEFT JOIN content_item i ON i.id=r.content_id AND i.delete_time=0 WHERE m.delete_time=0 ORDER BY r.create_time DESC,r.id DESC LIMIT 300";
	const char* sSqlOne = "SELECT r.id,r.media_id,m.title,m.url,r.content_id,i.title,r.ref_type,r.create_time FROM content_media_ref r INNER JOIN content_media m ON m.id=r.media_id LEFT JOIN content_item i ON i.id=r.content_id AND i.delete_time=0 WHERE m.delete_time=0 AND r.media_id=? ORDER BY r.create_time DESC,r.id DESC LIMIT 300";

	(void)objServer; (void)objHost; (void)objSession;
	memset(sMediaId, 0, sizeof(sMediaId));
	Managed_ReadTextQuery(objReq, "mediaId", sMediaId, sizeof(sMediaId));
	iMediaId = sMediaId[0] ? atoll(sMediaId) : 0;
	if ( !Managed_AbilityPackMounted("content.media") ) {
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "media ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblRet);
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, iMediaId > 0 ? sSqlOne : sSqlAll, -1, &stmt, NULL) == SQLITE_OK ) {
		if ( iMediaId > 0 ) sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iMediaId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) Managed_AppendMediaRefRow(arrList, stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestMediaDetailPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sId[32];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblRet = NULL;
	int64 iId = 0;

	(void)objServer; (void)objHost; (void)objSession;
	Managed_ReadTextQuery(objReq, "id", sId, sizeof(sId));
	iId = atoll(sId);
	if ( iId <= 0 ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "missing media id");
		return;
	}
	if ( !Managed_AbilityPackMounted("content.media") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "media ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id,attachment_xid,title,url,mime,ext,size,width,height,status,create_time,update_time FROM content_media WHERE id=? AND status=1 AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) Managed_AppendMediaRow(arrList, stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	if ( xvoArrayItemCount(arrList) <= 0 ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "media not found");
		return;
	}
	tblRet = Managed_CreateResult(TRUE, NULL);
	xvoTableSetValue(tblRet, "data", 4, xvoArrayGetValue(arrList, 0), FALSE);
	xvoUnref(arrList);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_MediaInferExtFromUrl(const char* sUrl, char* sExt, size_t iExtSize)
{
	const char* pDot = NULL;
	const char* p = NULL;
	size_t i = 0;

	if ( (sExt == NULL) || (iExtSize <= 0) ) return;
	sExt[0] = '\0';
	if ( Managed_IsBlank(sUrl) ) return;
	pDot = strrchr(sUrl, '.');
	if ( pDot == NULL ) return;
	p = pDot + 1;
	while ( *p && (*p != '?') && (*p != '#') && (*p != '/') && (i + 1 < iExtSize) ) {
		char c = *p;
		if ( (c >= 'A') && (c <= 'Z') ) c = (char)(c - 'A' + 'a');
		if ( !((c >= 'a') && (c <= 'z')) && !((c >= '0') && (c <= '9')) ) break;
		sExt[i++] = c;
		p++;
	}
	sExt[i] = '\0';
}

const char* Managed_MediaMimeFromExt(const char* sExt)
{
	if ( Managed_IsBlank(sExt) ) return "";
	if ( (strcmp(sExt, "jpg") == 0) || (strcmp(sExt, "jpeg") == 0) ) return "image/jpeg";
	if ( strcmp(sExt, "png") == 0 ) return "image/png";
	if ( strcmp(sExt, "gif") == 0 ) return "image/gif";
	if ( strcmp(sExt, "webp") == 0 ) return "image/webp";
	if ( strcmp(sExt, "svg") == 0 ) return "image/svg+xml";
	if ( strcmp(sExt, "avif") == 0 ) return "image/avif";
	if ( strcmp(sExt, "pdf") == 0 ) return "application/pdf";
	if ( strcmp(sExt, "mp4") == 0 ) return "video/mp4";
	if ( strcmp(sExt, "webm") == 0 ) return "video/webm";
	if ( strcmp(sExt, "mp3") == 0 ) return "audio/mpeg";
	if ( strcmp(sExt, "wav") == 0 ) return "audio/wav";
	if ( strcmp(sExt, "txt") == 0 ) return "text/plain";
	return "";
}

void Managed_RequestMediaSaveAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	const char* sTitle = tblBody ? xvoTableGetText(tblBody, "title", 5) : NULL;
	const char* sUrl = tblBody ? xvoTableGetText(tblBody, "url", 3) : NULL;
	const char* sMime = tblBody ? xvoTableGetText(tblBody, "mime", 4) : NULL;
	const char* sExt = tblBody ? xvoTableGetText(tblBody, "ext", 3) : NULL;
	const char* sAttachmentXid = tblBody ? xvoTableGetText(tblBody, "attachmentXid", 13) : NULL;
	char sInferredExt[32];
	const char* sSaveExt = sExt;
	const char* sSaveMime = sMime;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	bool bInsert = iId <= 0;
	int64 iNow = xrtNow();

	(void)objServer; (void)objHost;
	if ( tblBody == NULL ) {
		Managed_SendError(objResp, "invalid json body");
		return;
	}
	if ( !Managed_AbilityPackMounted("content.media") ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "media ability pack is not enabled");
		return;
	}
	if ( Managed_IsBlank(sUrl) ) {
		xvoUnref(tblBody);
		Managed_SendError(objResp, "url is required");
		return;
	}
	memset(sInferredExt, 0, sizeof(sInferredExt));
	if ( Managed_IsBlank(sSaveExt) ) {
		Managed_MediaInferExtFromUrl(sUrl, sInferredExt, sizeof(sInferredExt));
		sSaveExt = sInferredExt;
	}
	if ( Managed_IsBlank(sSaveMime) ) {
		sSaveMime = Managed_MediaMimeFromExt(sSaveExt);
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( iId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE content_media SET attachment_xid=?,title=?,url=?,mime=?,ext=?,size=?,width=?,height=?,status=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sAttachmentXid ? sAttachmentXid : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, sTitle ? sTitle : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sUrl ? sUrl : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, sSaveMime ? sSaveMime : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 5, sSaveExt ? sSaveExt : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 6, xvoTableGetInt(tblBody, "size", 4));
			sqlite3_bind_int(stmt, 7, (int)xvoTableGetInt(tblBody, "width", 5));
			sqlite3_bind_int(stmt, 8, (int)xvoTableGetInt(tblBody, "height", 6));
			sqlite3_bind_int(stmt, 9, xvoTableGetInt(tblBody, "status", 6) ? 1 : 0);
			sqlite3_bind_int64(stmt, 10, iNow);
			sqlite3_bind_int64(stmt, 11, (sqlite3_int64)iId);
			sqlite3_step(stmt);
		}
	} else {
		if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_media(attachment_xid,title,url,mime,ext,size,width,height,status,create_time,update_time,delete_time) VALUES(?,?,?,?,?,?,?,?,?,?,?,0)", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sAttachmentXid ? sAttachmentXid : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, sTitle ? sTitle : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sUrl ? sUrl : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, sSaveMime ? sSaveMime : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 5, sSaveExt ? sSaveExt : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 6, xvoTableGetInt(tblBody, "size", 4));
			sqlite3_bind_int(stmt, 7, (int)xvoTableGetInt(tblBody, "width", 5));
			sqlite3_bind_int(stmt, 8, (int)xvoTableGetInt(tblBody, "height", 6));
			sqlite3_bind_int(stmt, 9, xvoTableGetInt(tblBody, "status", 6) ? 1 : 0);
			sqlite3_bind_int64(stmt, 10, iNow);
			sqlite3_bind_int64(stmt, 11, iNow);
			sqlite3_step(stmt);
			iId = sqlite3_last_insert_rowid(pDb);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_AuditLogWithRequest(pDb, "media", iId, bInsert ? "media.create" : "media.update", sTitle ? sTitle : "", sUrl ? sUrl : "", objReq, objSession);
	Managed_CloseDb(pDb);
	xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(iId > 0, iId > 0 ? "ok" : "save failed"));
}

void Managed_RequestMediaDeleteAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	int64 iNow = xrtNow();

	(void)objServer; (void)objHost;
	if ( !Managed_AbilityPackMounted("content.media") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "media ability pack is not enabled");
		return;
	}
	if ( iId <= 0 ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "invalid media id");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( Managed_MediaRefCount(pDb, iId) > 0 ) {
		Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "media is still referenced");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_media SET delete_time=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, iNow);
		sqlite3_bind_int64(stmt, 2, iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_AuditLogWithRequest(pDb, "media", iId, "media.delete", "delete media", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	if ( tblBody ) xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "deleted"));
}

void Managed_RequestMediaBatchAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	xvalue arrIds = tblBody ? xvoTableGetValue(tblBody, "ids", 3) : NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	xvalue arrReport = xvoCreateArray();
	const char* sAction = tblBody ? xvoTableGetText(tblBody, "action", 6) : NULL;
	int64 iNow = xrtNow();
	int iTotal = 0;
	int iSuccess = 0;
	int iFail = 0;
	uint32 i;

	(void)objServer; (void)objHost;
	if ( !Managed_AbilityPackMounted("content.media") ) {
		if ( tblBody ) xvoUnref(tblBody);
		xvoUnref(arrReport);
		Managed_SendError(objResp, "media ability pack is not enabled");
		return;
	}
	if ( (tblBody == NULL) || (arrIds == NULL) || (xvoType(arrIds) != XVO_DT_ARRAY) || Managed_IsBlank(sAction) ) {
		if ( tblBody ) xvoUnref(tblBody);
		xvoUnref(arrReport);
		Managed_SendError(objResp, "ids array and action are required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		xvoUnref(arrReport);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	iTotal = (int)xvoArrayItemCount(arrIds);
	for ( i = 0; i < xvoArrayItemCount(arrIds); i++ ) {
		int64 iId = xvoGetInt(xvoArrayGetValue(arrIds, i));
		xvalue tblRow = xvoCreateTable();
		bool bOk = FALSE;
		const char* sMessage = "invalid media id";

		if ( iId > 0 ) {
			if ( strcmp(sAction, "delete") == 0 ) {
				if ( Managed_MediaRefCount(pDb, iId) > 0 ) {
					sMessage = "media is still referenced";
				} else if ( sqlite3_prepare_v2(pDb, "UPDATE content_media SET delete_time=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
					sqlite3_bind_int64(stmt, 1, iNow);
					sqlite3_bind_int64(stmt, 2, iNow);
					sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
					bOk = (sqlite3_step(stmt) == SQLITE_DONE) && (sqlite3_changes(pDb) > 0);
					sMessage = bOk ? "deleted" : "not found";
				}
			} else if ( (strcmp(sAction, "enable") == 0) || (strcmp(sAction, "disable") == 0) ) {
				if ( sqlite3_prepare_v2(pDb, "UPDATE content_media SET status=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
					sqlite3_bind_int(stmt, 1, strcmp(sAction, "enable") == 0 ? 1 : 0);
					sqlite3_bind_int64(stmt, 2, iNow);
					sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
					bOk = (sqlite3_step(stmt) == SQLITE_DONE) && (sqlite3_changes(pDb) > 0);
					sMessage = bOk ? "updated" : "not found";
				}
			} else {
				sMessage = "unsupported action";
			}
			if ( stmt ) sqlite3_finalize(stmt);
			stmt = NULL;
		}
		if ( bOk ) iSuccess++; else iFail++;
		xvoTableSetInt(tblRow, "id", 2, iId);
		xvoTableSetBool(tblRow, "ok", 2, bOk);
		xvoTableSetText(tblRow, "message", 7, (str)sMessage, 0, FALSE);
		xvoArrayAppendValue(arrReport, tblRow, TRUE);
	}
	Managed_AuditLogWithRequest(pDb, "media", 0, "media.batch", sAction ? sAction : "", NULL, objReq, objSession);
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "batch finished");
	xvoTableSetInt(tblRet, "totalCount", 10, iTotal);
	xvoTableSetInt(tblRet, "successCount", 12, iSuccess);
	xvoTableSetInt(tblRet, "failCount", 9, iFail);
	xvoTableSetValue(tblRet, "data", 4, arrReport, TRUE);
	if ( tblBody ) xvoUnref(tblBody);
	Managed_SendJsonValue(objResp, tblRet);
}

int64 Managed_RevisionNextNo(sqlite3* pDb, int64 iContentId)
{
	sqlite3_stmt* stmt = NULL;
	int64 iNext = 1;

	if ( (pDb == NULL) || (iContentId <= 0) ) return 1;
	if ( sqlite3_prepare_v2(pDb, "SELECT COALESCE(MAX(revision_no),0)+1 FROM content_revision WHERE content_id=?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) iNext = sqlite3_column_int64(stmt, 0);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return iNext > 0 ? iNext : 1;
}

void Managed_RevisionSnapshot(sqlite3* pDb, int64 iContentId, const char* sTitle, int iStatus, int iCategoryId, bool bDraft, const char* sPayloadJson, const char* sAction, int64 iNow)
{
	sqlite3_stmt* stmt = NULL;
	int64 iRevisionNo;

	if ( !Managed_AbilityPackMounted("content.revision") || (pDb == NULL) || (iContentId <= 0) ) return;
	iRevisionNo = Managed_RevisionNextNo(pDb, iContentId);
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_revision(content_id,revision_no,title,status,category_id,is_draft,payload_json,action,operator_id,create_time) VALUES(?,?,?,?,?,?,?,?,0,?)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iRevisionNo);
		sqlite3_bind_text(stmt, 3, sTitle ? sTitle : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 4, iStatus);
		sqlite3_bind_int(stmt, 5, iCategoryId);
		sqlite3_bind_int(stmt, 6, bDraft ? 1 : 0);
		sqlite3_bind_text(stmt, 7, sPayloadJson ? sPayloadJson : "{}", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 8, sAction ? sAction : "save", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 9, (sqlite3_int64)iNow);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
}

void Managed_AppendRevisionRow(xvalue arrList, sqlite3_stmt* stmt, bool bWithPayload)
{
	xvalue tblItem = xvoCreateTable();

	xvoTableSetInt(tblItem, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblItem, "contentId", 9, sqlite3_column_int64(stmt, 1));
	xvoTableSetInt(tblItem, "revisionNo", 10, sqlite3_column_int64(stmt, 2));
	xvoTableSetText(tblItem, "title", 5, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetInt(tblItem, "status", 6, sqlite3_column_int(stmt, 4));
	xvoTableSetInt(tblItem, "categoryId", 10, sqlite3_column_int(stmt, 5));
	xvoTableSetBool(tblItem, "isDraft", 7, sqlite3_column_int(stmt, 6) ? TRUE : FALSE);
	if ( bWithPayload ) {
		const char* sPayload = (const char*)sqlite3_column_text(stmt, 7);
		xvalue tblData = sPayload ? xrtParseJSON((str)sPayload, strlen(sPayload)) : NULL;
		xvoTableSetText(tblItem, "payloadJson", 11, (str)(sPayload ? sPayload : "{}"), 0, FALSE);
		if ( tblData ) {
			xvoTableSetValue(tblItem, "data", 4, tblData, TRUE);
		}
	}
	xvoTableSetText(tblItem, "action", 6, (str)sqlite3_column_text(stmt, 8), 0, FALSE);
	xvoTableSetInt(tblItem, "operatorId", 10, sqlite3_column_int64(stmt, 9));
	xvoTableSetInt(tblItem, "createTime", 10, sqlite3_column_int64(stmt, 10));
	Managed_SetTimeText(tblItem, "createTimeText", 14, sqlite3_column_int64(stmt, 10));
	xvoArrayAppendValue(arrList, tblItem, TRUE);
}

int64 Managed_RevisionPrevId(sqlite3* pDb, int64 iContentId, int64 iRevisionNo)
{
	sqlite3_stmt* stmt = NULL;
	int64 iPrevId = 0;

	if ( (pDb == NULL) || (iContentId <= 0) || (iRevisionNo <= 0) ) return 0;
	if ( sqlite3_prepare_v2(pDb, "SELECT id FROM content_revision WHERE content_id=? AND revision_no<? ORDER BY revision_no DESC,id DESC LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iRevisionNo);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) iPrevId = sqlite3_column_int64(stmt, 0);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return iPrevId;
}

xvalue Managed_LoadRevisionRecord(sqlite3* pDb, int64 iId)
{
	sqlite3_stmt* stmt = NULL;
	xvalue tblItem = NULL;

	if ( (pDb == NULL) || (iId <= 0) ) return NULL;
	if ( sqlite3_prepare_v2(pDb, "SELECT id,content_id,revision_no,title,status,category_id,is_draft,payload_json,action,operator_id,create_time FROM content_revision WHERE id=? LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue arrTemp = xvoCreateArray();
			Managed_AppendRevisionRow(arrTemp, stmt, TRUE);
			tblItem = (xvoArrayItemCount(arrTemp) > 0) ? xvoCopy(xvoArrayGetValue(arrTemp, 0)) : NULL;
			xvoUnref(arrTemp);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return tblItem;
}

void Managed_AppendRevisionDiffRow(xvalue arrList, const char* sField, const char* sTitle, xvalue objBefore, xvalue objAfter)
{
	str sBefore = Managed_ValueToTextDup(objBefore);
	str sAfter = Managed_ValueToTextDup(objAfter);
	bool bChanged = strcmp(sBefore ? (const char*)sBefore : "", sAfter ? (const char*)sAfter : "") != 0;
	xvalue tblRow;

	if ( !bChanged ) {
		if ( sBefore ) xrtFree(sBefore);
		if ( sAfter ) xrtFree(sAfter);
		return;
	}
	tblRow = xvoCreateTable();
	xvoTableSetText(tblRow, "field", 5, (str)(sField ? sField : ""), 0, FALSE);
	xvoTableSetText(tblRow, "title", 5, (str)(sTitle ? sTitle : (sField ? sField : "")), 0, FALSE);
	xvoTableSetText(tblRow, "before", 6, sBefore ? sBefore : (str)"", 0, TRUE);
	xvoTableSetText(tblRow, "after", 5, sAfter ? sAfter : (str)"", 0, TRUE);
	xvoTableSetBool(tblRow, "changed", 7, TRUE);
	xvoArrayAppendValue(arrList, tblRow, TRUE);
}

void Managed_AppendRevisionDiffFields(xvalue arrList, xvalue tblSpec, xvalue tblBefore, xvalue tblAfter)
{
	xvalue arrFields = Managed_GetFields(tblSpec);
	xvalue tblBeforeData = tblBefore ? xvoTableGetValue(tblBefore, "data", 4) : NULL;
	xvalue tblAfterData = tblAfter ? xvoTableGetValue(tblAfter, "data", 4) : NULL;

	Managed_AppendRevisionDiffRow(arrList, "title", "Title", tblBefore ? xvoTableGetValue(tblBefore, "title", 5) : NULL, tblAfter ? xvoTableGetValue(tblAfter, "title", 5) : NULL);
	Managed_AppendRevisionDiffRow(arrList, "status", "Status", tblBefore ? xvoTableGetValue(tblBefore, "status", 6) : NULL, tblAfter ? xvoTableGetValue(tblAfter, "status", 6) : NULL);
	Managed_AppendRevisionDiffRow(arrList, "categoryId", "Category", tblBefore ? xvoTableGetValue(tblBefore, "categoryId", 10) : NULL, tblAfter ? xvoTableGetValue(tblAfter, "categoryId", 10) : NULL);
	Managed_AppendRevisionDiffRow(arrList, "isDraft", "Draft", tblBefore ? xvoTableGetValue(tblBefore, "isDraft", 7) : NULL, tblAfter ? xvoTableGetValue(tblAfter, "isDraft", 7) : NULL);

	if ( (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) ) return;
	for ( uint32 i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
		xvalue tblField = xvoArrayGetValue(arrFields, i);
		const char* sName = (tblField && (xvoType(tblField) == XVO_DT_TABLE)) ? xvoTableGetText(tblField, "name", 4) : NULL;
		const char* sTitle = (tblField && (xvoType(tblField) == XVO_DT_TABLE)) ? xvoTableGetText(tblField, "title", 5) : NULL;
		if ( Managed_IsBlank(sName) ) continue;
		Managed_AppendRevisionDiffRow(arrList, sName, Managed_IsBlank(sTitle) ? sName : sTitle,
			(tblBeforeData && (xvoType(tblBeforeData) == XVO_DT_TABLE)) ? xvoTableGetValue(tblBeforeData, sName, (uint32)strlen(sName)) : NULL,
			(tblAfterData && (xvoType(tblAfterData) == XVO_DT_TABLE)) ? xvoTableGetValue(tblAfterData, sName, (uint32)strlen(sName)) : NULL);
	}
}

str Managed_BuildContentAuditDiffJson(xvalue tblSpec, xvalue tblBefore, xvalue tblAfter, int* pChangeCount)
{
	xvalue arrChanges = xvoCreateArray();
	str sJson = NULL;

	if ( pChangeCount ) *pChangeCount = 0;
	if ( arrChanges == NULL ) return NULL;
	Managed_AppendRevisionDiffFields(arrChanges, tblSpec, tblBefore, tblAfter);
	if ( pChangeCount ) *pChangeCount = (int)xvoArrayItemCount(arrChanges);
	sJson = xrtStringifyJSON(arrChanges, FALSE, NULL);
	xvoUnref(arrChanges);
	return sJson;
}

void Managed_RequestRevisionListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sContentId[32];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();
	int64 iContentId = 0;

	(void)objServer; (void)objHost; (void)objSession;
	Managed_ReadTextQuery(objReq, "contentId", sContentId, sizeof(sContentId));
	iContentId = atoll(sContentId);
	if ( !Managed_AbilityPackMounted("content.revision") ) {
		xvoUnref(tblRet); xvoUnref(arrList);
		Managed_SendError(objResp, "revision ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblRet); xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, iContentId > 0
		? "SELECT id,content_id,revision_no,title,status,category_id,is_draft,payload_json,action,operator_id,create_time FROM content_revision WHERE content_id=? ORDER BY revision_no DESC,id DESC"
		: "SELECT id,content_id,revision_no,title,status,category_id,is_draft,payload_json,action,operator_id,create_time FROM content_revision ORDER BY id DESC LIMIT 200", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( iContentId > 0 ) sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) Managed_AppendRevisionRow(arrList, stmt, FALSE);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestRevisionDetailAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sId[32];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = xvoCreateArray();
	xvalue tblRet = NULL;
	int64 iId = 0;

	(void)objServer; (void)objHost; (void)objSession;
	Managed_ReadTextQuery(objReq, "id", sId, sizeof(sId));
	iId = atoll(sId);
	if ( !Managed_AbilityPackMounted("content.revision") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "revision ability pack is not enabled");
		return;
	}
	if ( iId <= 0 ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "revision id is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT id,content_id,revision_no,title,status,category_id,is_draft,payload_json,action,operator_id,create_time FROM content_revision WHERE id=? LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) Managed_AppendRevisionRow(arrList, stmt, TRUE);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	if ( xvoArrayItemCount(arrList) <= 0 ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "revision not found");
		return;
	}
	tblRet = Managed_CreateResult(TRUE, NULL);
	xvoTableSetValue(tblRet, "data", 4, xvoArrayGetValue(arrList, 0), FALSE);
	xvoUnref(arrList);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestRevisionDiffAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sId[32];
	char sBaseId[32];
	sqlite3* pDb = NULL;
	xvalue tblSpec = NULL;
	xvalue tblAfter = NULL;
	xvalue tblBefore = NULL;
	xvalue tblData = NULL;
	xvalue arrChanges = xvoCreateArray();
	xvalue tblRet = NULL;
	int64 iId = 0;
	int64 iBaseId = 0;

	(void)objServer; (void)objHost; (void)objSession;
	memset(sId, 0, sizeof(sId));
	memset(sBaseId, 0, sizeof(sBaseId));
	Managed_ReadTextQuery(objReq, "id", sId, sizeof(sId));
	Managed_ReadTextQuery(objReq, "baseId", sBaseId, sizeof(sBaseId));
	iId = atoll(sId);
	iBaseId = atoll(sBaseId);
	if ( !Managed_AbilityPackMounted("content.revision") ) {
		xvoUnref(arrChanges);
		Managed_SendError(objResp, "revision ability pack is not enabled");
		return;
	}
	if ( iId <= 0 ) {
		xvoUnref(arrChanges);
		Managed_SendError(objResp, "revision id is required");
		return;
	}
	tblSpec = Managed_LoadSpec();
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) || (tblSpec == NULL) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(arrChanges);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	tblAfter = Managed_LoadRevisionRecord(pDb, iId);
	if ( tblAfter && (iBaseId <= 0) ) {
		iBaseId = Managed_RevisionPrevId(pDb, xvoTableGetInt(tblAfter, "contentId", 9), xvoTableGetInt(tblAfter, "revisionNo", 10));
	}
	if ( iBaseId > 0 ) {
		tblBefore = Managed_LoadRevisionRecord(pDb, iBaseId);
	}
	Managed_CloseDb(pDb);
	if ( tblAfter == NULL ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		if ( tblBefore ) xvoUnref(tblBefore);
		xvoUnref(arrChanges);
		Managed_SendError(objResp, "revision not found");
		return;
	}
	Managed_AppendRevisionDiffFields(arrChanges, tblSpec, tblBefore, tblAfter);
	tblData = xvoCreateTable();
	xvoTableSetValue(tblData, "base", 4, tblBefore ? xvoCopy(tblBefore) : xvoCreateNull(), TRUE);
	xvoTableSetValue(tblData, "target", 6, xvoCopy(tblAfter), TRUE);
	xvoTableSetValue(tblData, "changes", 7, arrChanges, TRUE);
	xvoTableSetInt(tblData, "changeCount", 11, xvoArrayItemCount(arrChanges));
	tblRet = Managed_CreateResult(TRUE, NULL);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	if ( tblSpec ) xvoUnref(tblSpec);
	if ( tblBefore ) xvoUnref(tblBefore);
	if ( tblAfter ) xvoUnref(tblAfter);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestRevisionRestorePreviewAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sId[32];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblSpec = NULL;
	xvalue tblTarget = NULL;
	xvalue tblCurrent = NULL;
	xvalue tblData = NULL;
	xvalue arrChanges = xvoCreateArray();
	xvalue tblRet = NULL;
	int64 iId = 0;
	int64 iContentId = 0;

	(void)objServer; (void)objHost; (void)objSession;
	memset(sId, 0, sizeof(sId));
	Managed_ReadTextQuery(objReq, "id", sId, sizeof(sId));
	iId = atoll(sId);
	if ( !Managed_AbilityPackMounted("content.revision") ) {
		xvoUnref(arrChanges);
		Managed_SendError(objResp, "revision ability pack is not enabled");
		return;
	}
	if ( iId <= 0 ) {
		xvoUnref(arrChanges);
		Managed_SendError(objResp, "revision id is required");
		return;
	}
	tblSpec = Managed_LoadSpec();
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) || (tblSpec == NULL) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(arrChanges);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	tblTarget = Managed_LoadRevisionRecord(pDb, iId);
	iContentId = tblTarget ? xvoTableGetInt(tblTarget, "contentId", 9) : 0;
	if ( iContentId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "SELECT id,title,status,payload_json,is_draft,create_time,update_time,category_id FROM content_item WHERE id=? AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
			if ( sqlite3_step(stmt) == SQLITE_ROW ) {
				tblCurrent = Managed_CreateItemFromStmt(stmt, tblSpec);
			}
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	if ( (tblTarget == NULL) || (tblCurrent == NULL) ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		if ( tblTarget ) xvoUnref(tblTarget);
		if ( tblCurrent ) xvoUnref(tblCurrent);
		xvoUnref(arrChanges);
		Managed_SendError(objResp, tblTarget ? "current content item not found" : "revision not found");
		return;
	}
	Managed_AppendRevisionDiffFields(arrChanges, tblSpec, tblCurrent, tblTarget);
	tblData = xvoCreateTable();
	xvoTableSetValue(tblData, "current", 7, xvoCopy(tblCurrent), TRUE);
	xvoTableSetValue(tblData, "target", 6, xvoCopy(tblTarget), TRUE);
	xvoTableSetValue(tblData, "changes", 7, arrChanges, TRUE);
	xvoTableSetInt(tblData, "changeCount", 11, xvoArrayItemCount(arrChanges));
	xvoTableSetInt(tblData, "contentId", 9, iContentId);
	tblRet = Managed_CreateResult(TRUE, NULL);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	if ( tblSpec ) xvoUnref(tblSpec);
	if ( tblTarget ) xvoUnref(tblTarget);
	if ( tblCurrent ) xvoUnref(tblCurrent);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestRevisionRestoreAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	int64 iContentId = 0;
	int iStatus = 0;
	int iCategoryId = 0;
	bool bDraft = FALSE;
	str sTitle = NULL;
	str sPayloadJson = NULL;
	int64 iNow = xrtNow();

	(void)objServer; (void)objHost; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.revision") ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "revision ability pack is not enabled");
		return;
	}
	if ( iId <= 0 ) {
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "revision id is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT content_id,title,status,category_id,is_draft,payload_json FROM content_revision WHERE id=? LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iContentId = sqlite3_column_int64(stmt, 0);
			sTitle = xrtCopyStr((str)sqlite3_column_text(stmt, 1), 0);
			iStatus = sqlite3_column_int(stmt, 2);
			iCategoryId = sqlite3_column_int(stmt, 3);
			bDraft = sqlite3_column_int(stmt, 4) ? TRUE : FALSE;
			sPayloadJson = xrtCopyStr((str)sqlite3_column_text(stmt, 5), 0);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( iContentId <= 0 ) {
		Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		if ( sTitle ) xrtFree(sTitle);
		if ( sPayloadJson ) xrtFree(sPayloadJson);
		Managed_SendError(objResp, "revision not found");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_item SET title=?,status=?,payload_json=?,category_id=?,is_draft=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sTitle ? (const char*)sTitle : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 2, iStatus);
		sqlite3_bind_text(stmt, 3, sPayloadJson ? (const char*)sPayloadJson : "{}", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 4, iCategoryId);
		sqlite3_bind_int(stmt, 5, bDraft ? 1 : 0);
		sqlite3_bind_int64(stmt, 6, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 7, (sqlite3_int64)iContentId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_RevisionSnapshot(pDb, iContentId, sTitle, iStatus, iCategoryId, bDraft, sPayloadJson, "restore", iNow);
	Managed_AuditLogWithRequest(pDb, "content", iContentId, "content.restore", sTitle ? (const char*)sTitle : "", sPayloadJson ? (const char*)sPayloadJson : "{}", objReq, objSession);
	Managed_CloseDb(pDb);
	if ( tblBody ) xvoUnref(tblBody);
	if ( sTitle ) xrtFree(sTitle);
	if ( sPayloadJson ) xrtFree(sPayloadJson);
	Managed_SendJsonValue(objResp, Managed_CreateResult(TRUE, "restored"));
}

void Managed_WorkflowAppendLog(sqlite3* pDb, int64 iContentId, const char* sAction, int iFromStatus, int iToStatus, bool bFromDraft, bool bToDraft, const char* sReason, int64 iAssigneeId, int64 iNow)
{
	sqlite3_stmt* stmt = NULL;

	if ( !Managed_AbilityPackMounted("content.workflow") || (pDb == NULL) || (iContentId <= 0) || Managed_IsBlank(sAction) ) return;
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_workflow_log(content_id,action,from_status,to_status,from_draft,to_draft,reason,operator_id,assignee_id,create_time) VALUES(?,?,?,?,?,?,?,0,?,?)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		sqlite3_bind_text(stmt, 2, sAction, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 3, iFromStatus);
		sqlite3_bind_int(stmt, 4, iToStatus);
		sqlite3_bind_int(stmt, 5, bFromDraft ? 1 : 0);
		sqlite3_bind_int(stmt, 6, bToDraft ? 1 : 0);
		sqlite3_bind_text(stmt, 7, sReason ? sReason : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 8, (sqlite3_int64)iAssigneeId);
		sqlite3_bind_int64(stmt, 9, (sqlite3_int64)iNow);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
}

bool Managed_WorkflowLoadContent(sqlite3* pDb, int64 iContentId, str* psTitle, str* psPayloadJson, int* pCategoryId, int* pStatus, bool* pDraft)
{
	sqlite3_stmt* stmt = NULL;
	bool bFound = FALSE;

	if ( psTitle ) *psTitle = NULL;
	if ( psPayloadJson ) *psPayloadJson = NULL;
	if ( pCategoryId ) *pCategoryId = 0;
	if ( pStatus ) *pStatus = 0;
	if ( pDraft ) *pDraft = FALSE;
	if ( (pDb == NULL) || (iContentId <= 0) ) return FALSE;
	if ( sqlite3_prepare_v2(pDb, "SELECT title,payload_json,category_id,status,is_draft FROM content_item WHERE id=? AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			if ( psTitle ) *psTitle = xrtCopyStr((str)sqlite3_column_text(stmt, 0), 0);
			if ( psPayloadJson ) *psPayloadJson = xrtCopyStr((str)sqlite3_column_text(stmt, 1), 0);
			if ( pCategoryId ) *pCategoryId = sqlite3_column_int(stmt, 2);
			if ( pStatus ) *pStatus = sqlite3_column_int(stmt, 3);
			if ( pDraft ) *pDraft = sqlite3_column_int(stmt, 4) ? TRUE : FALSE;
			bFound = TRUE;
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return bFound;
}

void Managed_WorkflowSyncContentEffects(sqlite3* pDb, xvalue tblSpec, int64 iContentId, int iStatus, bool bDraft, int64 iNow)
{
	str sTitle = NULL;
	str sPayloadJson = NULL;
	xvalue tblData = NULL;
	int iCategoryId = 0;

	if ( (pDb == NULL) || (tblSpec == NULL) || (iContentId <= 0) ) return;
	if ( !Managed_WorkflowLoadContent(pDb, iContentId, &sTitle, &sPayloadJson, &iCategoryId, NULL, NULL) ) return;
	tblData = Managed_IsBlank(sPayloadJson) ? NULL : xrtParseJSON(sPayloadJson, (int)strlen((const char*)sPayloadJson));
	if ( tblData && (xvoType(tblData) == XVO_DT_TABLE) ) {
		Managed_SeoSyncData(pDb, iContentId, sTitle, tblData, iNow);
		Managed_SearchIndexSyncData(pDb, tblSpec, iContentId, sTitle, iStatus, tblData, iNow);
		Managed_SitemapSyncData(pDb, tblSpec, iContentId, sTitle, iStatus, bDraft, tblData, iNow);
		Managed_RelatedSyncData(pDb, tblSpec, iContentId, iCategoryId > 0 ? iCategoryId : 0, iStatus, bDraft, iNow);
	}
	Managed_StaticMaybeAutoGenerate(pDb, iContentId, bDraft, iStatus);
	if ( tblData ) xvoUnref(tblData);
	if ( sPayloadJson ) xrtFree(sPayloadJson);
	if ( sTitle ) xrtFree(sTitle);
}

bool Managed_WorkflowSetPayloadPublishedAt(sqlite3* pDb, xvalue tblSpec, int64 iContentId, int64 iPublishAt)
{
	str sTitle = NULL;
	str sPayloadJson = NULL;
	xvalue tblData = NULL;
	str sNewJson = NULL;
	sqlite3_stmt* stmt = NULL;
	const char* sPublishedAtField = Managed_GetPublishedAtField(tblSpec);
	bool bOk = FALSE;

	if ( (pDb == NULL) || (tblSpec == NULL) || (iContentId <= 0) || (iPublishAt <= 0) || Managed_IsBlank(sPublishedAtField) ) return FALSE;
	if ( !Managed_WorkflowLoadContent(pDb, iContentId, &sTitle, &sPayloadJson, NULL, NULL, NULL) ) return FALSE;
	tblData = Managed_IsBlank(sPayloadJson) ? xvoCreateTable() : xrtParseJSON(sPayloadJson, (int)strlen((const char*)sPayloadJson));
	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) ) {
		if ( tblData ) xvoUnref(tblData);
		tblData = xvoCreateTable();
	}
	xvoTableSetInt(tblData, sPublishedAtField, (int)strlen(sPublishedAtField), iPublishAt);
	sNewJson = xrtStringifyJSON(tblData, FALSE, NULL);
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_item SET payload_json=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sNewJson ? (const char*)sNewJson : "{}", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)xrtNow());
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iContentId);
		bOk = (sqlite3_step(stmt) == SQLITE_DONE);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	if ( sNewJson ) xrtFree(sNewJson);
	if ( tblData ) xvoUnref(tblData);
	if ( sPayloadJson ) xrtFree(sPayloadJson);
	if ( sTitle ) xrtFree(sTitle);
	return bOk;
}

int Managed_WorkflowPublishDue(sqlite3* pDb, xvalue tblSpec, int iLimit, int64 iNow)
{
	sqlite3_stmt* stmt = NULL;
	sqlite3_stmt* stmtUpdate = NULL;
	int iPublished = 0;
	const char* sPublishedAtField = Managed_GetPublishedAtField(tblSpec);

	if ( !Managed_AbilityPackMounted("content.workflow") || (pDb == NULL) || (tblSpec == NULL) || Managed_IsBlank(sPublishedAtField) ) return 0;
	if ( iLimit <= 0 ) iLimit = 100;
	if ( iLimit > 1000 ) iLimit = 1000;
	/* Timed publish is evaluated in the admin/task path, never in the request hot path. */
	if ( sqlite3_prepare_v2(pDb, "SELECT id,title,payload_json,category_id,status,is_draft FROM content_item WHERE delete_time=0 AND is_draft=1 ORDER BY update_time ASC,id ASC LIMIT ?", -1, &stmt, NULL) != SQLITE_OK ) return 0;
	sqlite3_bind_int(stmt, 1, iLimit);
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		int64 iContentId = sqlite3_column_int64(stmt, 0);
		const char* sTitle = (const char*)sqlite3_column_text(stmt, 1);
		const char* sPayloadJson = (const char*)sqlite3_column_text(stmt, 2);
		int iFromStatus = sqlite3_column_int(stmt, 4);
		bool bFromDraft = sqlite3_column_int(stmt, 5) ? TRUE : FALSE;
		xvalue tblData = Managed_IsBlank(sPayloadJson) ? NULL : xrtParseJSON((str)sPayloadJson, (int)strlen(sPayloadJson));
		xvalue objPublishedAt = (tblData && (xvoType(tblData) == XVO_DT_TABLE)) ? xvoTableGetValue(tblData, sPublishedAtField, (int)strlen(sPublishedAtField)) : NULL;
		int64 iPublishAt = Managed_ValueToInt64(objPublishedAt);
		if ( (iPublishAt > 0) && (iPublishAt <= iNow) ) {
			int iToStatus = Managed_PublicStatusThreshold(tblSpec);
			if ( sqlite3_prepare_v2(pDb, "UPDATE content_item SET status=?,is_draft=0,update_time=? WHERE id=? AND delete_time=0", -1, &stmtUpdate, NULL) == SQLITE_OK ) {
				sqlite3_bind_int(stmtUpdate, 1, iToStatus);
				sqlite3_bind_int64(stmtUpdate, 2, (sqlite3_int64)iNow);
				sqlite3_bind_int64(stmtUpdate, 3, (sqlite3_int64)iContentId);
				if ( sqlite3_step(stmtUpdate) == SQLITE_DONE ) {
					Managed_WorkflowAppendLog(pDb, iContentId, "scheduled-publish", iFromStatus, iToStatus, bFromDraft, FALSE, "scheduled publish due", 0, iNow);
					Managed_AuditLog(pDb, "content", iContentId, "workflow.scheduled-publish", sTitle ? sTitle : "", sPayloadJson ? sPayloadJson : "{}", NULL);
					Managed_WorkflowSyncContentEffects(pDb, tblSpec, iContentId, iToStatus, FALSE, iNow);
					iPublished++;
				}
			}
			if ( stmtUpdate ) sqlite3_finalize(stmtUpdate);
			stmtUpdate = NULL;
		}
		if ( tblData ) xvoUnref(tblData);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	return iPublished;
}

void Managed_RequestWorkflowActionAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	const char* sAction = tblBody ? xvoTableGetText(tblBody, "action", 6) : NULL;
	const char* sReason = tblBody ? xvoTableGetText(tblBody, "reason", 6) : NULL;
	int64 iId = tblBody ? xvoTableGetInt(tblBody, "id", 2) : 0;
	int64 iPublishAt = tblBody ? xvoTableGetInt(tblBody, "publishAt", 9) : 0;
	int64 iAssigneeId = tblBody ? xvoTableGetInt(tblBody, "assigneeId", 10) : 0;
	int iFromStatus = 0;
	int iToStatus = 0;
	bool bFromDraft = FALSE;
	bool bToDraft = FALSE;
	xvalue tblSpec = Managed_LoadSpec();
	xvalue tblRet = NULL;
	int64 iNow = xrtNow();

	(void)objServer; (void)objHost; (void)objSession;
	if ( (iAssigneeId <= 0) && tblBody ) iAssigneeId = xvoTableGetInt(tblBody, "reviewerId", 10);
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		if ( tblBody ) xvoUnref(tblBody);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "method not allowed");
		return;
	}
	if ( !Managed_AbilityPackMounted("content.workflow") ) {
		if ( tblBody ) xvoUnref(tblBody);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "workflow ability pack is not enabled");
		return;
	}
	if ( (iId <= 0) || Managed_IsBlank(sAction) ) {
		if ( tblBody ) xvoUnref(tblBody);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "id and action are required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT status,is_draft FROM content_item WHERE id=? AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iFromStatus = sqlite3_column_int(stmt, 0);
			bFromDraft = sqlite3_column_int(stmt, 1) ? TRUE : FALSE;
		} else {
			sqlite3_finalize(stmt);
			Managed_CloseDb(pDb);
			if ( tblBody ) xvoUnref(tblBody);
			if ( tblSpec ) xvoUnref(tblSpec);
			Managed_SendError(objResp, "content not found");
			return;
		}
		sqlite3_finalize(stmt);
	}
	stmt = NULL;
	iToStatus = iFromStatus;
	bToDraft = bFromDraft;
	if ( strcmp(sAction, "submit") == 0 ) {
		iToStatus = Managed_StatusFlowNeedsReview(tblSpec) ? 1 : Managed_PublicStatusThreshold(tblSpec);
		bToDraft = FALSE;
	} else if ( (strcmp(sAction, "approve") == 0) || (strcmp(sAction, "publish") == 0) ) {
		iToStatus = Managed_PublicStatusThreshold(tblSpec);
		bToDraft = FALSE;
	} else if ( strcmp(sAction, "reject") == 0 ) {
		iToStatus = 0;
		bToDraft = TRUE;
	} else if ( strcmp(sAction, "offline") == 0 ) {
		iToStatus = 0;
		bToDraft = FALSE;
	} else if ( strcmp(sAction, "schedule") == 0 ) {
		if ( iPublishAt <= iNow ) {
			Managed_CloseDb(pDb);
			if ( tblBody ) xvoUnref(tblBody);
			if ( tblSpec ) xvoUnref(tblSpec);
			Managed_SendError(objResp, "publishAt must be a future timestamp");
			return;
		}
		iToStatus = 0;
		bToDraft = TRUE;
	} else {
		Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "unsupported workflow action");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_item SET status=?,is_draft=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iToStatus);
		sqlite3_bind_int(stmt, 2, bToDraft ? 1 : 0);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 4, (sqlite3_int64)iId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	if ( strcmp(sAction, "schedule") == 0 ) {
		Managed_WorkflowSetPayloadPublishedAt(pDb, tblSpec, iId, iPublishAt);
	}
	Managed_WorkflowAppendLog(pDb, iId, sAction, iFromStatus, iToStatus, bFromDraft, bToDraft, sReason, iAssigneeId, iNow);
	Managed_WorkflowSyncContentEffects(pDb, tblSpec, iId, iToStatus, bToDraft, iNow);
	{
		str sAuditAction = xrtFormat("workflow.%s", sAction);
		Managed_AuditLogWithRequest(pDb, "content", iId, sAuditAction ? (const char*)sAuditAction : "workflow.action", sReason ? sReason : "", NULL, objReq, objSession);
		if ( sAuditAction ) xrtFree(sAuditAction);
	}
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "workflow action saved");
	xvoTableSetInt(tblRet, "id", 2, iId);
	xvoTableSetText(tblRet, "action", 6, (str)sAction, 0, FALSE);
	xvoTableSetInt(tblRet, "status", 6, iToStatus);
	xvoTableSetBool(tblRet, "isDraft", 7, bToDraft);
	xvoTableSetInt(tblRet, "assigneeId", 10, iAssigneeId);
	if ( strcmp(sAction, "schedule") == 0 ) {
		xvoTableSetInt(tblRet, "publishAt", 9, iPublishAt);
		Managed_SetTimeText(tblRet, "publishAtText", 13, iPublishAt);
	}
	Managed_SendJsonValue(objResp, tblRet);
	if ( tblBody ) xvoUnref(tblBody);
	if ( tblSpec ) xvoUnref(tblSpec);
}

void Managed_RequestWorkflowLogListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sContentId[32];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();
	int64 iContentId = 0;

	(void)objServer; (void)objHost; (void)objSession;
	Managed_ReadTextQuery(objReq, "contentId", sContentId, sizeof(sContentId));
	iContentId = atoll(sContentId);
	if ( !Managed_AbilityPackMounted("content.workflow") ) {
		xvoUnref(tblRet); xvoUnref(arrList);
		Managed_SendError(objResp, "workflow ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblRet); xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, iContentId > 0
		? "SELECT id,content_id,action,from_status,to_status,from_draft,to_draft,reason,operator_id,assignee_id,create_time FROM content_workflow_log WHERE content_id=? ORDER BY id DESC"
		: "SELECT id,content_id,action,from_status,to_status,from_draft,to_draft,reason,operator_id,assignee_id,create_time FROM content_workflow_log ORDER BY id DESC LIMIT 200", -1, &stmt, NULL) == SQLITE_OK ) {
		if ( iContentId > 0 ) sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblItem = xvoCreateTable();
			xvoTableSetInt(tblItem, "id", 2, sqlite3_column_int64(stmt, 0));
			xvoTableSetInt(tblItem, "contentId", 9, sqlite3_column_int64(stmt, 1));
			xvoTableSetText(tblItem, "action", 6, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
			xvoTableSetInt(tblItem, "fromStatus", 10, sqlite3_column_int(stmt, 3));
			xvoTableSetInt(tblItem, "toStatus", 8, sqlite3_column_int(stmt, 4));
			xvoTableSetBool(tblItem, "fromDraft", 9, sqlite3_column_int(stmt, 5) ? TRUE : FALSE);
			xvoTableSetBool(tblItem, "toDraft", 7, sqlite3_column_int(stmt, 6) ? TRUE : FALSE);
			xvoTableSetText(tblItem, "reason", 6, (str)sqlite3_column_text(stmt, 7), 0, FALSE);
			xvoTableSetInt(tblItem, "operatorId", 10, sqlite3_column_int64(stmt, 8));
			xvoTableSetInt(tblItem, "assigneeId", 10, sqlite3_column_int64(stmt, 9));
			xvoTableSetInt(tblItem, "createTime", 10, sqlite3_column_int64(stmt, 10));
			Managed_SetTimeText(tblItem, "createTimeText", 14, sqlite3_column_int64(stmt, 10));
			xvoArrayAppendValue(arrList, tblItem, TRUE);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestWorkflowTodoListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Managed_AbilityPackMounted("content.workflow") ) {
		xvoUnref(tblRet); xvoUnref(arrList);
		Managed_SendError(objResp, "workflow ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblRet); xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}

	/* 待办只读取草稿或未发布内容，并附带最后一条工作流日志，便于审核入口直接定位处理对象。 */
	if ( sqlite3_prepare_v2(pDb,
		"SELECT c.id,c.title,c.status,c.is_draft,c.update_time,"
		"COALESCE(l.assignee_id,0),COALESCE(l.reason,''),COALESCE(l.action,''),COALESCE(l.create_time,0) "
		"FROM content_item c "
		"LEFT JOIN content_workflow_log l ON l.id=("
			"SELECT l2.id FROM content_workflow_log l2 WHERE l2.content_id=c.id ORDER BY l2.id DESC LIMIT 1"
		") "
		"WHERE c.delete_time=0 AND (c.is_draft=1 OR c.status=0) "
		"ORDER BY COALESCE(l.create_time,c.update_time) DESC,c.id DESC LIMIT 200", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblItem = xvoCreateTable();
			xvoTableSetInt(tblItem, "id", 2, sqlite3_column_int64(stmt, 0));
			xvoTableSetText(tblItem, "title", 5, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
			xvoTableSetInt(tblItem, "status", 6, sqlite3_column_int(stmt, 2));
			xvoTableSetBool(tblItem, "isDraft", 7, sqlite3_column_int(stmt, 3) ? TRUE : FALSE);
			xvoTableSetInt(tblItem, "updateTime", 10, sqlite3_column_int64(stmt, 4));
			Managed_SetTimeText(tblItem, "updateTimeText", 14, sqlite3_column_int64(stmt, 4));
			xvoTableSetInt(tblItem, "assigneeId", 10, sqlite3_column_int64(stmt, 5));
			xvoTableSetText(tblItem, "lastReason", 10, (str)sqlite3_column_text(stmt, 6), 0, FALSE);
			xvoTableSetText(tblItem, "lastAction", 10, (str)sqlite3_column_text(stmt, 7), 0, FALSE);
			xvoTableSetInt(tblItem, "logTime", 7, sqlite3_column_int64(stmt, 8));
			Managed_SetTimeText(tblItem, "logTimeText", 11, sqlite3_column_int64(stmt, 8));
			xvoArrayAppendValue(arrList, tblItem, TRUE);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestWorkflowScheduledPublishAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody = Managed_ParseJsonBody(objReq);
	xvalue tblSpec = Managed_LoadSpec();
	xvalue tblRet = NULL;
	sqlite3* pDb = NULL;
	int iLimit = tblBody ? (int)xvoTableGetInt(tblBody, "limit", 5) : 100;
	int iPublished = 0;
	int64 iNow = xrtNow();

	(void)objServer; (void)objHost; (void)objSession;
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		if ( tblBody ) xvoUnref(tblBody);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "method not allowed");
		return;
	}
	if ( !Managed_AbilityPackMounted("content.workflow") ) {
		if ( tblBody ) xvoUnref(tblBody);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "workflow ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) || (tblSpec == NULL) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblBody ) xvoUnref(tblBody);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	Managed_ExecSql(pDb, "BEGIN IMMEDIATE");
	iPublished = Managed_WorkflowPublishDue(pDb, tblSpec, iLimit, iNow);
	Managed_ExecSql(pDb, "COMMIT");
	Managed_CloseDb(pDb);
	tblRet = Managed_CreateResult(TRUE, "scheduled publish processed");
	xvoTableSetInt(tblRet, "published", 9, iPublished);
	xvoTableSetInt(tblRet, "limit", 5, iLimit);
	Managed_SetTimeText(tblRet, "processTimeText", 15, iNow);
	Managed_SendJsonValue(objResp, tblRet);
	if ( tblBody ) xvoUnref(tblBody);
	if ( tblSpec ) xvoUnref(tblSpec);
}

void Managed_RequestSeoMetaPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sId[32];
	char sSlug[160];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblSpec = Managed_LoadSpec();
	xvalue tblData = NULL;
	xvalue tblRet = NULL;
	str sSqlById = NULL;
	str sSqlScan = NULL;

	(void)objServer;
	(void)objHost;
	(void)objSession;
	memset(sId, 0, sizeof(sId));
	memset(sSlug, 0, sizeof(sSlug));
	xsReqQueryValue(objReq, "id", sId, sizeof(sId));
	xsReqQueryValue(objReq, "slug", sSlug, sizeof(sSlug));
	if ( (sSlug[0] != '\0') && !Managed_AbilityPackMounted("content.slug") ) {
		if ( sSqlById ) xrtFree(sSqlById);
		if ( sSqlScan ) xrtFree(sSqlScan);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "slug capability is not enabled");
		return;
	}
	if ( (sId[0] == '\0') && (sSlug[0] == '\0') ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "id or slug is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblSpec ) xvoUnref(tblSpec);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	sSqlById = xrtFormat("SELECT id, title, status, payload_json, is_draft, create_time, update_time, category_id FROM content_item WHERE id = ? AND delete_time = 0 AND is_draft = 0 AND status >= %d", Managed_PublicStatusThreshold(tblSpec));
	sSqlScan = xrtFormat("SELECT id, title, status, payload_json, is_draft, create_time, update_time, category_id FROM content_item WHERE delete_time = 0 AND is_draft = 0 AND status >= %d ORDER BY update_time DESC, id DESC", Managed_PublicStatusThreshold(tblSpec));
	if ( sqlite3_prepare_v2(pDb, (sId[0] != '\0') ? sSqlById : sSqlScan, -1, &stmt, NULL) == SQLITE_OK ) {
		if ( sId[0] != '\0' ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)atoll(sId));
			if ( sqlite3_step(stmt) == SQLITE_ROW ) {
				tblData = Managed_CreateItemFromStmt(stmt, tblSpec);
			}
		} else {
			while ( sqlite3_step(stmt) == SQLITE_ROW ) {
				xvalue tblCandidate = Managed_CreateItemFromStmt(stmt, tblSpec);
				if ( Managed_ItemMatchesSlug(tblCandidate, sSlug) ) {
					tblData = tblCandidate;
					break;
				}
				if ( tblCandidate ) xvoUnref(tblCandidate);
			}
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	if ( sSqlById ) xrtFree(sSqlById);
	if ( sSqlScan ) xrtFree(sSqlScan);
	if ( tblSpec ) xvoUnref(tblSpec);
	if ( tblData == NULL ) {
		Managed_CloseDb(pDb);
		Managed_SendError(objResp, "content item not found");
		return;
	}
	tblRet = Managed_CreateResult(TRUE, NULL);
	xvoTableSetValue(tblRet, "data", 4, Managed_LoadSeoMeta(pDb, tblData), TRUE);
	xvoUnref(tblData);
	if ( pDb ) Managed_CloseDb(pDb);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_AppendSeoMetaAdminRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblItem = xvoCreateTable();

	xvoTableSetInt(tblItem, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetInt(tblItem, "contentId", 9, sqlite3_column_int64(stmt, 1));
	xvoTableSetText(tblItem, "contentTitle", 12, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetText(tblItem, "seoTitle", 8, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetText(tblItem, "seoKeywords", 11, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetText(tblItem, "seoDescription", 14, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
	xvoTableSetText(tblItem, "canonical", 9, (str)sqlite3_column_text(stmt, 6), 0, FALSE);
	xvoTableSetInt(tblItem, "status", 6, sqlite3_column_int(stmt, 7));
	xvoTableSetInt(tblItem, "updateTime", 10, sqlite3_column_int64(stmt, 8));
	Managed_SetTimeText(tblItem, "updateTimeText", 14, sqlite3_column_int64(stmt, 8));
	xvoArrayAppendValue(arrList, tblItem, TRUE);
}

void Managed_RequestSeoMetaListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	if ( !Managed_AbilityPackMounted("content.seo") ) {
		xvoUnref(tblRet); xvoUnref(arrList);
		Managed_SendError(objResp, "seo ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblRet); xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "SELECT m.id,m.content_id,COALESCE(c.title,''),m.seo_title,m.seo_keywords,m.seo_description,m.canonical,m.status,m.update_time FROM content_seo_meta m LEFT JOIN content_item c ON c.id=m.content_id WHERE m.delete_time=0 ORDER BY m.update_time DESC,m.id DESC LIMIT 500", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Managed_AppendSeoMetaAdminRow(arrList, stmt);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestSeoMetaSaveAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblForm = Managed_ParseJsonBody(objReq);
	xvalue tblRet = NULL;
	int64 iId = tblForm ? xvoTableGetInt(tblForm, "id", 2) : 0;
	int64 iContentId = tblForm ? xvoTableGetInt(tblForm, "contentId", 9) : 0;
	const char* sSeoTitle = tblForm ? (const char*)xvoTableGetText(tblForm, "seoTitle", 8) : "";
	const char* sSeoKeywords = tblForm ? (const char*)xvoTableGetText(tblForm, "seoKeywords", 11) : "";
	const char* sSeoDescription = tblForm ? (const char*)xvoTableGetText(tblForm, "seoDescription", 14) : "";
	const char* sCanonical = tblForm ? (const char*)xvoTableGetText(tblForm, "canonical", 9) : "";
	int iStatus = tblForm ? xvoTableGetInt(tblForm, "status", 6) : 1;
	int64 iNow = xrtNow();

	(void)objServer;
	(void)objHost;
	(void)objSession;
	if ( !Managed_AbilityPackMounted("content.seo") ) {
		if ( tblForm ) xvoUnref(tblForm);
		Managed_SendError(objResp, "seo ability pack is not enabled");
		return;
	}
	if ( (tblForm == NULL) || (iContentId <= 0) ) {
		if ( tblForm ) xvoUnref(tblForm);
		Managed_SendError(objResp, "contentId is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblForm);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( iId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE content_seo_meta SET content_id=?,seo_title=?,seo_keywords=?,seo_description=?,canonical=?,status=?,update_time=?,delete_time=0 WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
			sqlite3_bind_text(stmt, 2, sSeoTitle ? sSeoTitle : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sSeoKeywords ? sSeoKeywords : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, sSeoDescription ? sSeoDescription : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 5, sCanonical ? sCanonical : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 6, iStatus);
			sqlite3_bind_int64(stmt, 7, (sqlite3_int64)iNow);
			sqlite3_bind_int64(stmt, 8, (sqlite3_int64)iId);
			sqlite3_step(stmt);
		}
	} else {
		if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_seo_meta(content_id,seo_title,seo_keywords,seo_description,canonical,status,create_time,update_time,delete_time) VALUES(?,?,?,?,?,?,?,?,0) ON CONFLICT(content_id) DO UPDATE SET seo_title=excluded.seo_title,seo_keywords=excluded.seo_keywords,seo_description=excluded.seo_description,canonical=excluded.canonical,status=excluded.status,update_time=excluded.update_time,delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iContentId);
			sqlite3_bind_text(stmt, 2, sSeoTitle ? sSeoTitle : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sSeoKeywords ? sSeoKeywords : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, sSeoDescription ? sSeoDescription : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 5, sCanonical ? sCanonical : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 6, iStatus);
			sqlite3_bind_int64(stmt, 7, (sqlite3_int64)iNow);
			sqlite3_bind_int64(stmt, 8, (sqlite3_int64)iNow);
			sqlite3_step(stmt);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoUnref(tblForm);
	tblRet = Managed_CreateResult(TRUE, "saved");
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestSeoMetaDeleteAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblForm = Managed_ParseJsonBody(objReq);
	xvalue tblRet = NULL;
	int64 iId = tblForm ? xvoTableGetInt(tblForm, "id", 2) : 0;
	int64 iNow = xrtNow();

	(void)objServer;
	(void)objHost;
	(void)objSession;
	if ( !Managed_AbilityPackMounted("content.seo") ) {
		if ( tblForm ) xvoUnref(tblForm);
		Managed_SendError(objResp, "seo ability pack is not enabled");
		return;
	}
	if ( (tblForm == NULL) || (iId <= 0) ) {
		if ( tblForm ) xvoUnref(tblForm);
		Managed_SendError(objResp, "id is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblForm);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_seo_meta SET status=0,delete_time=?,update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	xvoUnref(tblForm);
	tblRet = Managed_CreateResult(TRUE, "deleted");
	Managed_SendJsonValue(objResp, tblRet);
}

void Managed_RequestGetAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;
	Managed_RequestDetailCommon(objResp, objReq, objSession, TRUE);
}

void Managed_RequestSave(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm = NULL;
	xvalue tblData = NULL;
	xvalue tblSpec = NULL;
	xvalue tblAuditBefore = NULL;
	xvalue tblAuditAfter = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	str sPayloadJson = NULL;
	str sTitle = NULL;
	str sError = NULL;
	str sOldSlug = NULL;
	str sNewSlug = NULL;
	str sAuditDetailJson = NULL;
	str sAuditSummary = NULL;
	int64 iId = 0;
	int iCategoryId = 0;
	int iStatus = 0;
	int iAuditChangeCount = 0;
	bool bDraft = FALSE;
	bool bInsert = FALSE;
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
	bInsert = iId <= 0;
	iCategoryId = (int)xvoTableGetInt(tblForm, "categoryId", 10);
	if ( iCategoryId <= 0 ) {
		iCategoryId = (int)xvoTableGetInt(tblData, "categoryId", 10);
	}
	sTitle = Managed_ExtractTitle(tblData, tblSpec);
	iStatus = Managed_NormalizeStatusForSave(tblSpec, bDraft, Managed_ExtractStatus(tblData, tblSpec));
	Managed_StoreStatusValue(tblData, tblSpec, iStatus);
	Managed_EnsurePublishedAtValue(tblData, tblSpec, bDraft, iStatus, iNow);

	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblSpec);
		xvoUnref(tblForm);
		if ( sTitle ) xrtFree(sTitle);
		if ( sPayloadJson ) xrtFree(sPayloadJson);
		if ( sOldSlug ) xrtFree(sOldSlug);
		if ( sNewSlug ) xrtFree(sNewSlug);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( !Managed_SensitiveBeforeSave(pDb, tblData, iId, &sError) ) {
		Managed_CloseDb(pDb);
		xvoUnref(tblSpec);
		xvoUnref(tblForm);
		if ( sTitle ) xrtFree(sTitle);
		if ( sPayloadJson ) xrtFree(sPayloadJson);
		if ( sOldSlug ) xrtFree(sOldSlug);
		if ( sNewSlug ) xrtFree(sNewSlug);
		Managed_SendError(objResp, sError ? (const char*)sError : "sensitive word hit");
		if ( sError ) xrtFree(sError);
		return;
	}
	sNewSlug = Managed_ExtractSlugValue(tblData, tblSpec);
	if ( !Managed_IsBlank((const char*)sNewSlug) && Managed_SlugExists(pDb, tblSpec, (const char*)sNewSlug, iId, NULL) ) {
		Managed_CloseDb(pDb);
		xvoUnref(tblSpec);
		xvoUnref(tblForm);
		if ( sTitle ) xrtFree(sTitle);
		if ( sPayloadJson ) xrtFree(sPayloadJson);
		if ( sOldSlug ) xrtFree(sOldSlug);
		if ( sNewSlug ) xrtFree(sNewSlug);
		Managed_SendError(objResp, "slug already exists");
		return;
	}
	if ( iId > 0 ) {
		sOldSlug = Managed_LoadContentSlug(pDb, tblSpec, iId);
		tblAuditBefore = Managed_LoadContentItemById(pDb, tblSpec, iId);
	}
	if ( sTitle ) xrtFree(sTitle);
	sTitle = Managed_ExtractTitle(tblData, tblSpec);
	sPayloadJson = xrtStringifyJSON(tblData, FALSE, NULL);

	if ( iId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE content_item SET title = ?, status = ?, payload_json = ?, category_id = ?, is_draft = ?, update_time = ? WHERE id = ? AND delete_time = 0", -1, &stmt, NULL) != SQLITE_OK ) {
			Managed_CloseDb(pDb);
			xvoUnref(tblSpec);
			xvoUnref(tblForm);
			if ( sTitle ) xrtFree(sTitle);
			if ( sPayloadJson ) xrtFree(sPayloadJson);
			if ( sOldSlug ) xrtFree(sOldSlug);
			if ( sNewSlug ) xrtFree(sNewSlug);
			if ( tblAuditBefore ) xvoUnref(tblAuditBefore);
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
			if ( sOldSlug ) xrtFree(sOldSlug);
			if ( sNewSlug ) xrtFree(sNewSlug);
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
	Managed_SlugHistoryInsert(pDb, iId, (const char*)sOldSlug, (const char*)sNewSlug, iNow);
	Managed_SlugRedirectSync(pDb, iId, (const char*)sOldSlug, (const char*)sNewSlug, iNow);
	Managed_RevisionSnapshot(pDb, iId, sTitle, iStatus, iCategoryId > 0 ? iCategoryId : 0, bDraft, sPayloadJson, "save", iNow);
	if ( bInsert ) {
		Managed_AuditLogWithRequest(pDb, "content", iId, "content.create", sTitle ? (const char*)sTitle : "", sPayloadJson ? (const char*)sPayloadJson : "{}", objReq, objSession);
	} else {
		tblAuditAfter = Managed_LoadContentItemById(pDb, tblSpec, iId);
		sAuditDetailJson = Managed_BuildContentAuditDiffJson(tblSpec, tblAuditBefore, tblAuditAfter, &iAuditChangeCount);
		sAuditSummary = xrtFormat("%s; changedFields=%d", sTitle ? (const char*)sTitle : "", iAuditChangeCount);
		Managed_AuditLogWithRequest(pDb, "content", iId, "content.update", sAuditSummary ? (const char*)sAuditSummary : "", sAuditDetailJson ? (const char*)sAuditDetailJson : "[]", objReq, objSession);
	}
	if ( bInsert ) {
		Managed_SensitivePromotePendingLogs(pDb, iId, iNow);
	}
	Managed_MediaSyncRefs(pDb, iId, tblData, iNow);
	Managed_SeoSyncData(pDb, iId, sTitle, tblData, iNow);
	Managed_SearchIndexSyncData(pDb, tblSpec, iId, sTitle, iStatus, tblData, iNow);
	Managed_SitemapSyncData(pDb, tblSpec, iId, sTitle, iStatus, bDraft, tblData, iNow);
	Managed_RelatedSyncData(pDb, tblSpec, iId, iCategoryId > 0 ? iCategoryId : 0, iStatus, bDraft, iNow);
	Managed_StaticMaybeAutoGenerate(pDb, iId, bDraft, iStatus);
	Managed_CloseDb(pDb);
	xvoUnref(tblSpec);
	xvoUnref(tblForm);
	if ( sTitle ) xrtFree(sTitle);
	if ( sPayloadJson ) xrtFree(sPayloadJson);
	if ( sOldSlug ) xrtFree(sOldSlug);
	if ( sNewSlug ) xrtFree(sNewSlug);
	if ( sAuditDetailJson ) xrtFree(sAuditDetailJson);
	if ( sAuditSummary ) xrtFree(sAuditSummary);
	if ( tblAuditBefore ) xvoUnref(tblAuditBefore);
	if ( tblAuditAfter ) xvoUnref(tblAuditAfter);

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
	xvalue tblSpec = NULL;
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
	tblSpec = Managed_LoadSpec();
	if ( sqlite3_prepare_v2(pDb, "UPDATE content_item SET delete_time = ?, update_time = ? WHERE id = ? AND delete_time = 0", -1, &stmt, NULL) == SQLITE_OK ) {
		xtime iNow = xrtNow();
		sqlite3_bind_int64(stmt, 1, iNow);
		sqlite3_bind_int64(stmt, 2, iNow);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_AuditLogWithRequest(pDb, "content", iId, "content.delete", "delete content", NULL, objReq, objSession);
	Managed_SeoDelete(pDb, iId);
	Managed_SearchIndexDelete(pDb, iId);
	Managed_SitemapRemoveEntry(pDb, iId);
	if ( tblSpec ) Managed_SitemapWriteCacheFiles(pDb, tblSpec);
	Managed_StaticMaybeAutoClean(pDb, iId);
	Managed_CloseDb(pDb);
	if ( tblSpec ) xvoUnref(tblSpec);
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
	xvoTableSetText(tblItem, "description", 11, (str)sqlite3_column_text(stmt, 8), 0, FALSE);
	xvoTableSetText(tblItem, "coverUrl", 8, (str)sqlite3_column_text(stmt, 9), 0, FALSE);
	xvoTableSetText(tblItem, "templateKey", 11, (str)sqlite3_column_text(stmt, 10), 0, FALSE);
	xvoTableSetText(tblItem, "seoTitle", 8, (str)sqlite3_column_text(stmt, 11), 0, FALSE);
	xvoTableSetText(tblItem, "seoKeywords", 11, (str)sqlite3_column_text(stmt, 12), 0, FALSE);
	xvoTableSetText(tblItem, "seoDescription", 14, (str)sqlite3_column_text(stmt, 13), 0, FALSE);
	Managed_SetTimeText(tblItem, "createTimeText", 14, sqlite3_column_int64(stmt, 14));
	Managed_SetTimeText(tblItem, "updateTimeText", 14, sqlite3_column_int64(stmt, 15));
	xvoTableSetInt(tblItem, "contentCount", 12, sqlite3_column_int(stmt, 16));
	{
		int iLevel = sqlite3_column_int(stmt, 5);
		str sTreeTitle = xrtFormat("%*s%s", iLevel > 0 ? iLevel * 2 : 0, "", (const char*)sqlite3_column_text(stmt, 2));
		xvoTableSetText(tblItem, "treeTitle", 9, sTreeTitle ? sTreeTitle : (str)sqlite3_column_text(stmt, 2), 0, FALSE);
		if ( sTreeTitle ) xrtFree(sTreeTitle);
	}
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
		"SELECT c.id, c.parent_id, c.title, c.slug, c.path, c.level, c.sort, c.status, c.description, c.cover_url, c.template_key, c.seo_title, c.seo_keywords, c.seo_description, c.create_time, c.update_time, "
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

void Managed_RequestCategoryTreeAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	Managed_RequestCategoryListAdmin(objServer, objHost, objReq, objResp, objSession);
}

void Managed_RequestCategoryListPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();
	int iCount = 0;
	const char* sSql =
		"SELECT c.id, c.parent_id, c.title, c.slug, c.path, c.level, c.sort, c.status, c.description, c.cover_url, c.template_key, c.seo_title, c.seo_keywords, c.seo_description, c.create_time, c.update_time, "
		"(SELECT COUNT(*) FROM content_item i WHERE i.category_id = c.id AND i.delete_time = 0 AND i.is_draft = 0) AS content_count "
		"FROM content_category c WHERE c.delete_time = 0 AND c.status = 1 ORDER BY c.path ASC, c.sort ASC, c.id ASC";

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	if ( !Managed_AbilityPackMounted("content.category") ) {
		xvoUnref(arrList);
		xvoUnref(tblRet);
		Managed_SendError(objResp, "category ability pack is not enabled");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(arrList);
		xvoUnref(tblRet);
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

void Managed_RequestCategoryDetailPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	xvalue arrList = xvoCreateArray();
	char sId[32];
	char sSlug[128];
	int64 iId = 0;

	(void)objServer;
	(void)objHost;
	(void)objSession;
	memset(sId, 0, sizeof(sId));
	memset(sSlug, 0, sizeof(sSlug));
	xsReqQueryValue(objReq, "id", sId, sizeof(sId));
	xsReqQueryValue(objReq, "slug", sSlug, sizeof(sSlug));
	iId = atoll(sId);
	if ( !Managed_AbilityPackMounted("content.category") ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "category ability pack is not enabled");
		return;
	}
	if ( (iId <= 0) && (sSlug[0] == '\0') ) {
		xvoUnref(arrList);
		Managed_SendError(objResp, "id or slug is required");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(arrList);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb,
		"SELECT c.id, c.parent_id, c.title, c.slug, c.path, c.level, c.sort, c.status, c.description, c.cover_url, c.template_key, c.seo_title, c.seo_keywords, c.seo_description, c.create_time, c.update_time, "
		"(SELECT COUNT(*) FROM content_item i WHERE i.category_id = c.id AND i.delete_time = 0 AND i.is_draft = 0) AS content_count "
		"FROM content_category c WHERE c.delete_time = 0 AND c.status = 1 AND ((? > 0 AND c.id = ?) OR (? = 0 AND c.slug = ?)) LIMIT 1",
		-1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iId);
		sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
		sqlite3_bind_text(stmt, 4, sSlug, -1, SQLITE_TRANSIENT);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) Managed_AppendCategoryRow(arrList, stmt);
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

void Managed_RequestCategoryContentsPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sCategoryId[32];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblSpec = Managed_LoadSpec();
	xvalue tblRet = Managed_CreateResult(TRUE, NULL);
	xvalue arrList = xvoCreateArray();
	int64 iCategoryId;
	str sSql = NULL;
	int iCount = 0;

	(void)objServer;
	(void)objHost;
	(void)objSession;
	memset(sCategoryId, 0, sizeof(sCategoryId));
	xsReqQueryValue(objReq, "categoryId", sCategoryId, sizeof(sCategoryId));
	if ( sCategoryId[0] == '\0' ) xsReqQueryValue(objReq, "id", sCategoryId, sizeof(sCategoryId));
	iCategoryId = atoll(sCategoryId);
	if ( !Managed_AbilityPackMounted("content.category") ) {
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(tblRet); xvoUnref(arrList);
		Managed_SendError(objResp, "category ability pack is not enabled");
		return;
	}
	if ( (iCategoryId <= 0) || (tblSpec == NULL) || !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		if ( tblSpec ) xvoUnref(tblSpec);
		xvoUnref(tblRet); xvoUnref(arrList);
		Managed_SendError(objResp, "categoryId is required");
		return;
	}
	sSql = xrtFormat("SELECT id,title,status,payload_json,is_draft,create_time,update_time,category_id FROM content_item WHERE category_id=? AND delete_time=0 AND is_draft=0 AND status >= %d ORDER BY update_time DESC,id DESC LIMIT 200", Managed_PublicStatusThreshold(tblSpec));
	if ( sSql && sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iCategoryId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblItem = Managed_CreateItemFromStmt(stmt, tblSpec);
			xvoArrayAppendValue(arrList, tblItem, TRUE);
			iCount++;
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CloseDb(pDb);
	if ( sSql ) xrtFree(sSql);
	if ( tblSpec ) xvoUnref(tblSpec);
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
		"SELECT c.id, c.parent_id, c.title, c.slug, c.path, c.level, c.sort, c.status, c.description, c.cover_url, c.template_key, c.seo_title, c.seo_keywords, c.seo_description, c.create_time, c.update_time, "
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

void Managed_CategoryRewriteDescendantPaths(sqlite3* pDb, int64 iId, const char* sOldPath, const char* sNewPath, int iOldLevel, int iNewLevel, int64 iNow)
{
	sqlite3_stmt* stmt = NULL;
	str sLikePath = NULL;
	int iSuffixStart = 0;
	int iLevelDelta = 0;

	if ( (pDb == NULL) || (iId <= 0) || Managed_IsBlank(sOldPath) || Managed_IsBlank(sNewPath) ) return;
	if ( strcmp(sOldPath, sNewPath) == 0 && iOldLevel == iNewLevel ) return;

	sLikePath = xrtFormat("%s%%", sOldPath);
	iSuffixStart = (int)strlen(sOldPath) + 1;
	iLevelDelta = iNewLevel - iOldLevel;
	if ( sqlite3_prepare_v2(pDb,
		"UPDATE content_category "
		"SET path = ? || substr(path, ?), level = level + ?, update_time = ? "
		"WHERE id <> ? AND delete_time = 0 AND path LIKE ?",
		-1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sNewPath, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 2, iSuffixStart);
		sqlite3_bind_int(stmt, 3, iLevelDelta);
		sqlite3_bind_int64(stmt, 4, iNow);
		sqlite3_bind_int64(stmt, 5, (sqlite3_int64)iId);
		sqlite3_bind_text(stmt, 6, sLikePath ? (const char*)sLikePath : "", -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	if ( sLikePath ) xrtFree(sLikePath);
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
	int iLevel = 0;
	xtime iNow = xrtNow();
	const char* sTitle;
	const char* sSlug;
	const char* sDescription;
	const char* sCoverUrl;
	const char* sTemplateKey;
	const char* sSeoTitle;
	const char* sSeoKeywords;
	const char* sSeoDescription;
	str sPath = NULL;
	str sParentPath = NULL;
	str sOldPath = NULL;
	int iOldLevel = 0;

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
	sDescription = xvoTableGetText(tblForm, "description", 11);
	sCoverUrl = xvoTableGetText(tblForm, "coverUrl", 8);
	sTemplateKey = xvoTableGetText(tblForm, "templateKey", 11);
	sSeoTitle = xvoTableGetText(tblForm, "seoTitle", 8);
	sSeoKeywords = xvoTableGetText(tblForm, "seoKeywords", 11);
	sSeoDescription = xvoTableGetText(tblForm, "seoDescription", 14);
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
	if ( (iParentId > 0) && (iParentId == iId) ) {
		xvoUnref(tblForm);
		Managed_SendError(objResp, "parent category cannot be itself");
		return;
	}
	if ( !Managed_EnsureSchema() || !Managed_OpenDb(&pDb) ) {
		if ( pDb ) Managed_CloseDb(pDb);
		xvoUnref(tblForm);
		Managed_SendError(objResp, "failed to open plugin database");
		return;
	}
	if ( iId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "SELECT path,level FROM content_category WHERE id=? AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
			if ( sqlite3_step(stmt) == SQLITE_ROW ) {
				sOldPath = xrtCopyStr((str)sqlite3_column_text(stmt, 0), 0);
				iOldLevel = sqlite3_column_int(stmt, 1);
			}
		}
		if ( stmt ) sqlite3_finalize(stmt);
		stmt = NULL;
	}
	if ( iParentId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "SELECT path,level FROM content_category WHERE id=? AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iParentId);
			if ( sqlite3_step(stmt) == SQLITE_ROW ) {
				sParentPath = xrtCopyStr((str)sqlite3_column_text(stmt, 0), 0);
				iLevel = sqlite3_column_int(stmt, 1) + 1;
			}
		}
		if ( stmt ) sqlite3_finalize(stmt);
		stmt = NULL;
		if ( sParentPath == NULL ) {
			Managed_CloseDb(pDb);
			xvoUnref(tblForm);
			if ( sOldPath ) xrtFree(sOldPath);
			Managed_SendError(objResp, "parent category not found");
			return;
		}
		if ( sOldPath && (strncmp((const char*)sParentPath, (const char*)sOldPath, strlen((const char*)sOldPath)) == 0) ) {
			Managed_CloseDb(pDb);
			xvoUnref(tblForm);
			if ( sParentPath ) xrtFree(sParentPath);
			if ( sOldPath ) xrtFree(sOldPath);
			Managed_SendError(objResp, "parent category cannot be descendant");
			return;
		}
	}
	sPath = xrtFormat("%s%s/", sParentPath ? (const char*)sParentPath : "/", sSlug);
	if ( iId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE content_category SET parent_id = ?, title = ?, slug = ?, path = ?, level = ?, sort = ?, status = ?, description = ?, cover_url = ?, template_key = ?, seo_title = ?, seo_keywords = ?, seo_description = ?, update_time = ? WHERE id = ? AND delete_time = 0", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int(stmt, 1, iParentId > 0 ? iParentId : 0);
			sqlite3_bind_text(stmt, 2, sTitle, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sSlug, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, sPath ? (const char*)sPath : "/", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 5, iLevel);
			sqlite3_bind_int(stmt, 6, iSort);
			sqlite3_bind_int(stmt, 7, iStatus);
			sqlite3_bind_text(stmt, 8, sDescription ? sDescription : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 9, sCoverUrl ? sCoverUrl : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 10, sTemplateKey ? sTemplateKey : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 11, sSeoTitle ? sSeoTitle : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 12, sSeoKeywords ? sSeoKeywords : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 13, sSeoDescription ? sSeoDescription : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 14, iNow);
			sqlite3_bind_int64(stmt, 15, (sqlite3_int64)iId);
			sqlite3_step(stmt);
		}
	} else {
		if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_category(parent_id, title, slug, path, level, sort, status, description, cover_url, template_key, seo_title, seo_keywords, seo_description, create_time, update_time, delete_time) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int(stmt, 1, iParentId > 0 ? iParentId : 0);
			sqlite3_bind_text(stmt, 2, sTitle, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sSlug, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, sPath ? (const char*)sPath : "/", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 5, iLevel);
			sqlite3_bind_int(stmt, 6, iSort);
			sqlite3_bind_int(stmt, 7, iStatus);
			sqlite3_bind_text(stmt, 8, sDescription ? sDescription : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 9, sCoverUrl ? sCoverUrl : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 10, sTemplateKey ? sTemplateKey : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 11, sSeoTitle ? sSeoTitle : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 12, sSeoKeywords ? sSeoKeywords : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 13, sSeoDescription ? sSeoDescription : "", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 14, iNow);
			sqlite3_bind_int64(stmt, 15, iNow);
			sqlite3_step(stmt);
			iId = sqlite3_last_insert_rowid(pDb);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Managed_CategoryRewriteDescendantPaths(pDb, iId, (const char*)sOldPath, (const char*)sPath, iOldLevel, iLevel, iNow);
	Managed_CloseDb(pDb);
	xvoUnref(tblForm);
	if ( sPath ) xrtFree(sPath);
	if ( sParentPath ) xrtFree(sParentPath);
	if ( sOldPath ) xrtFree(sOldPath);
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
	int iChildCount = 0;
	int iContentCount = 0;
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
	if ( sqlite3_prepare_v2(pDb, "SELECT (SELECT COUNT(*) FROM content_category WHERE parent_id=? AND delete_time=0),(SELECT COUNT(*) FROM content_item WHERE category_id=? AND delete_time=0)", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
		sqlite3_bind_int64(stmt, 2, (sqlite3_int64)iId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iChildCount = sqlite3_column_int(stmt, 0);
			iContentCount = sqlite3_column_int(stmt, 1);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;
	if ( iChildCount > 0 || iContentCount > 0 ) {
		Managed_CloseDb(pDb);
		xvoUnref(tblForm);
		Managed_SendError(objResp, iChildCount > 0 ? "category has child categories" : "category has contents");
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
	for ( uint32 i = 0; i < xvoArrayItemCount(arrItems); i++ ) {
		xvalue tblItem = xvoArrayGetValue(arrItems, i);
		int64 iId;
		int64 iParentId;
		int iSort;
		bool bMoveParent;
		str sOldPath = NULL;
		str sSlug = NULL;
		str sParentPath = NULL;
		str sNewPath = NULL;
		int iOldLevel = 0;
		int iNewLevel = 0;
		if ( (tblItem == NULL) || (xvoType(tblItem) != XVO_DT_TABLE) ) {
			continue;
		}
		iId = xvoTableGetInt(tblItem, "id", 2);
		iSort = (int)xvoTableGetInt(tblItem, "sort", 4);
		bMoveParent = xvoTableExists(tblItem, "parentId", 8);
		iParentId = bMoveParent ? xvoTableGetInt(tblItem, "parentId", 8) : 0;
		if ( iId <= 0 ) {
			continue;
		}
		if ( !bMoveParent ) {
			if ( sqlite3_prepare_v2(pDb, "UPDATE content_category SET sort = ?, update_time = ? WHERE id = ? AND delete_time = 0", -1, &stmt, NULL) == SQLITE_OK ) {
				sqlite3_bind_int(stmt, 1, iSort);
				sqlite3_bind_int64(stmt, 2, iNow);
				sqlite3_bind_int64(stmt, 3, (sqlite3_int64)iId);
				if ( sqlite3_step(stmt) == SQLITE_DONE ) iUpdated++;
			}
			if ( stmt ) { sqlite3_finalize(stmt); stmt = NULL; }
			continue;
		}
		if ( iParentId == iId ) {
			continue;
		}
		if ( sqlite3_prepare_v2(pDb, "SELECT slug,path,level FROM content_category WHERE id=? AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iId);
			if ( sqlite3_step(stmt) == SQLITE_ROW ) {
				sSlug = xrtCopyStr((str)sqlite3_column_text(stmt, 0), 0);
				sOldPath = xrtCopyStr((str)sqlite3_column_text(stmt, 1), 0);
				iOldLevel = sqlite3_column_int(stmt, 2);
			}
		}
		if ( stmt ) { sqlite3_finalize(stmt); stmt = NULL; }
		if ( iParentId > 0 ) {
			if ( sqlite3_prepare_v2(pDb, "SELECT path,level FROM content_category WHERE id=? AND delete_time=0 LIMIT 1", -1, &stmt, NULL) == SQLITE_OK ) {
				sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iParentId);
				if ( sqlite3_step(stmt) == SQLITE_ROW ) {
					sParentPath = xrtCopyStr((str)sqlite3_column_text(stmt, 0), 0);
					iNewLevel = sqlite3_column_int(stmt, 1) + 1;
				}
			}
			if ( stmt ) { sqlite3_finalize(stmt); stmt = NULL; }
			if ( (sParentPath == NULL) || (sOldPath && strncmp((const char*)sParentPath, (const char*)sOldPath, strlen((const char*)sOldPath)) == 0) ) {
				if ( sOldPath ) xrtFree(sOldPath);
				if ( sSlug ) xrtFree(sSlug);
				if ( sParentPath ) xrtFree(sParentPath);
				continue;
			}
		}
		sNewPath = xrtFormat("%s%s/", sParentPath ? (const char*)sParentPath : "/", sSlug ? (const char*)sSlug : "");
		if ( sqlite3_prepare_v2(pDb, "UPDATE content_category SET parent_id=?, path=?, level=?, sort=?, update_time=? WHERE id=? AND delete_time=0", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)(iParentId > 0 ? iParentId : 0));
			sqlite3_bind_text(stmt, 2, sNewPath ? (const char*)sNewPath : "/", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 3, iNewLevel);
			sqlite3_bind_int(stmt, 4, iSort);
			sqlite3_bind_int64(stmt, 5, iNow);
			sqlite3_bind_int64(stmt, 6, (sqlite3_int64)iId);
			if ( sqlite3_step(stmt) == SQLITE_DONE ) iUpdated++;
		}
		if ( stmt ) { sqlite3_finalize(stmt); stmt = NULL; }
		Managed_CategoryRewriteDescendantPaths(pDb, iId, (const char*)sOldPath, (const char*)sNewPath, iOldLevel, iNewLevel, iNow);
		if ( sOldPath ) xrtFree(sOldPath);
		if ( sSlug ) xrtFree(sSlug);
		if ( sParentPath ) xrtFree(sParentPath);
		if ( sNewPath ) xrtFree(sNewPath);
	}
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

void Managed_RequestDashboardView(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	if ( !Managed_SendAssetHtml(objResp, "generated/dashboard.html") ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "managed dashboard page missing", 0);
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

void Managed_RequestAbilityPackView(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;
	if ( !Managed_SendAbilityAssetHtml(objReq, objResp) ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "managed ability pack page missing", 0);
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
	XAdminAuthGroupDecl authGroup;
	XAdminAuthDecl auth;
	XAdminUriAuthDecl uriAuth;
	int iRootMenuId = 0;
	int iAbilityAuthGroupId = 0;
	int auth_content_comment = 0;
	int auth_content_tag = 0;
	int auth_content_topic = 0;
	int auth_content_sensitive = 0;
	int auth_content_static = 0;
	int auth_content_like = 0;
	int auth_content_view_stat = 0;
	xvalue tblSpec = Managed_LoadSpec();
	bool bAdminCrud = Managed_AdminCrudEnabled(tblSpec);
	bool bPublicApi = Managed_PublicApiEnabled(tblSpec);
	bool bCategoryPack = Managed_AbilityPackMounted("content.category");
	bool bSlugPack = Managed_AbilityPackMounted("content.slug");
	bool bSeoPack = Managed_AbilityPackMounted("content.seo");
	bool bRedirectPack = Managed_AbilityPackMounted("content.redirect");
	bool bMediaPack = Managed_AbilityPackMounted("content.media");
	bool bRevisionPack = Managed_AbilityPackMounted("content.revision");
	bool bWorkflowPack = Managed_AbilityPackMounted("content.workflow");
	bool bSearchPack = Managed_AbilityPackMounted("content.search");
	bool bSitemapPack = Managed_AbilityPackMounted("content.sitemap");
	bool bRelatedPack = Managed_AbilityPackMounted("content.related");
	bool bFormPack = Managed_AbilityPackMounted("content.form");
	bool bAccessPack = Managed_AbilityPackMounted("content.access");
	bool bAuditLogPack = Managed_AbilityPackMounted("content.audit-log");
	bool bImportExportPack = Managed_AbilityPackMounted("content.import-export");
	bool bCommentPack = Managed_AbilityPackMounted("content.comment");
	bool bTagPack = Managed_AbilityPackMounted("content.tag");
	bool bTopicPack = Managed_AbilityPackMounted("content.topic");
	bool bSensitivePack = Managed_AbilityPackMounted("content.sensitive");
	bool bStaticPack = Managed_AbilityPackMounted("content.static");
	bool bLikePack = Managed_AbilityPackMounted("content.like");
	bool bViewPack = Managed_AbilityPackMounted("content.view-stat");

	if ( !Managed_EnsureSchema() ) {
		printf("        [ManagedPlugin] start failed during schema ensure: xid={{PLUGIN_XID}}\n");
		if ( tblSpec ) xvoUnref(tblSpec);
		return -1;
	}

{{ABILITY_PACK_AUTH_REGISTRATIONS}}

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

		if ( bSlugPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/slug/resolve";
			route.proc = Managed_RequestSlugResolvePublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/audit-log/cleanup";
			route.proc = Managed_RequestAuditLogCleanupAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bSeoPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/seo/meta";
			route.proc = Managed_RequestSeoMetaPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bCategoryPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/category/list";
			route.proc = Managed_RequestCategoryListPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/category/detail";
			route.proc = Managed_RequestCategoryDetailPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/category/contents";
			route.proc = Managed_RequestCategoryContentsPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bRedirectPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/redirect/resolve";
			route.proc = Managed_RequestRedirectResolvePublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bMediaPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/media/list";
			route.proc = Managed_RequestMediaListPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/media/detail";
			route.proc = Managed_RequestMediaDetailPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bSearchPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/search";
			route.proc = Managed_RequestSearchPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bSitemapPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/sitemap.xml";
			route.proc = Managed_RequestSitemapXmlPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/sitemap-index.xml";
			route.proc = Managed_RequestSitemapIndexPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/rss.xml";
			route.proc = Managed_RequestRssXmlPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/robots.txt";
			route.proc = Managed_RequestRobotsTxtPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bRelatedPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/related/list";
			route.proc = Managed_RequestRelatedListPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bFormPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/form/submit";
			route.proc = Managed_RequestFormSubmitPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bAccessPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/access/check";
			route.proc = Managed_RequestAccessCheckPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bCommentPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/comment/list";
			route.proc = Managed_RequestCommentListPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bCommentPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/comment/create";
			route.proc = Managed_RequestCommentCreatePublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bCommentPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/comment/count";
			route.proc = Managed_RequestCommentCountPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bCommentPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/comment/hide";
			route.proc = Managed_RequestCommentHidePublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bTagPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/tag/list";
			route.proc = Managed_RequestTagListPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bTagPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/tag/contents";
			route.proc = Managed_RequestTagContentsPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bTagPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/tag/detail";
			route.proc = Managed_RequestTagDetailPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bTopicPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/topic/list";
			route.proc = Managed_RequestTopicListPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bTopicPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/topic/contents";
			route.proc = Managed_RequestTopicContentsPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bTopicPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/topic/detail";
			route.proc = Managed_RequestTopicDetailPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bSensitivePack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/sensitive/check";
			route.proc = Managed_RequestSensitiveCheckPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bStaticPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/static/generate";
			route.proc = Managed_RequestStaticGeneratePublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bStaticPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/static/preview";
			route.proc = Managed_RequestStaticPreviewPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bLikePack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/like/status";
			route.proc = Managed_RequestLikeStatusPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bLikePack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/like/create";
			route.proc = Managed_RequestLikeCreatePublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bLikePack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/like/cancel";
			route.proc = Managed_RequestLikeCancelPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bLikePack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/like/list";
			route.proc = Managed_RequestLikeListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_like;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bLikePack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/like/counter/list";
			route.proc = Managed_RequestLikeCounterListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_like;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bLikePack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/like/stats";
			route.proc = Managed_RequestLikeStatsAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_like;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bLikePack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/like/status";
			route.proc = Managed_RequestLikeSetStatusAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_like;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bViewPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/view/record";
			route.proc = Managed_RequestViewRecordPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bViewPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/view/status";
			route.proc = Managed_RequestViewStatusPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bViewPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/view/count";
			route.proc = Managed_RequestViewStatusPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bViewPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/view/detail";
			route.proc = Managed_RequestViewStatusPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bViewPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/api/plugin/{{PLUGIN_XID}}/view/rank";
			route.proc = Managed_RequestViewRankPublic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bViewPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/view/counter/list";
			route.proc = Managed_RequestViewCounterListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_view_stat;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bViewPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/view/stats";
			route.proc = Managed_RequestViewStatsAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_view_stat;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bViewPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/view/log/list";
			route.proc = Managed_RequestViewLogListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_view_stat;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bViewPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/view/daily/list";
			route.proc = Managed_RequestViewDailyListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_view_stat;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

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

		if ( bCategoryPack ) {
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
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/category/tree";
			route.proc = Managed_RequestCategoryTreeAdmin;
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
		}

		if ( bSeoPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/seo/list";
			route.proc = Managed_RequestSeoMetaListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/seo/save";
			route.proc = Managed_RequestSeoMetaSaveAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/seo/delete";
			route.proc = Managed_RequestSeoMetaDeleteAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bRedirectPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/redirect/list";
			route.proc = Managed_RequestRedirectListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/redirect/save";
			route.proc = Managed_RequestRedirectSaveAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/redirect/import";
			route.proc = Managed_RequestRedirectImportAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/redirect/delete";
			route.proc = Managed_RequestRedirectDeleteAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bMediaPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/media/list";
			route.proc = Managed_RequestMediaListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/media/ref/list";
			route.proc = Managed_RequestMediaRefListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/media/save";
			route.proc = Managed_RequestMediaSaveAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/media/batch";
			route.proc = Managed_RequestMediaBatchAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/media/delete";
			route.proc = Managed_RequestMediaDeleteAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bRevisionPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/revision/list";
			route.proc = Managed_RequestRevisionListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/revision/detail";
			route.proc = Managed_RequestRevisionDetailAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/revision/diff";
			route.proc = Managed_RequestRevisionDiffAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/revision/restore";
			route.proc = Managed_RequestRevisionRestoreAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/revision/restore-preview";
			route.proc = Managed_RequestRevisionRestorePreviewAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bWorkflowPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/workflow/action";
			route.proc = Managed_RequestWorkflowActionAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/workflow/log/list";
			route.proc = Managed_RequestWorkflowLogListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/workflow/todo/list";
			route.proc = Managed_RequestWorkflowTodoListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/workflow/scheduled-publish";
			route.proc = Managed_RequestWorkflowScheduledPublishAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bSearchPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/search";
			route.proc = Managed_RequestSearchAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/search/rebuild";
			route.proc = Managed_RequestSearchRebuildAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/search/stats";
			route.proc = Managed_RequestSearchStatsAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bSitemapPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/sitemap/entry/list";
			route.proc = Managed_RequestSitemapEntryListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/sitemap/refresh";
			route.proc = Managed_RequestSitemapRefreshAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/sitemap/stats";
			route.proc = Managed_RequestSitemapStatsAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bRelatedPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/related/list";
			route.proc = Managed_RequestRelatedListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/related/save";
			route.proc = Managed_RequestRelatedSaveAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/related/rebuild";
			route.proc = Managed_RequestRelatedRebuildAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/related/delete";
			route.proc = Managed_RequestRelatedDeleteAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bFormPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/form/list";
			route.proc = Managed_RequestFormListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/form/save";
			route.proc = Managed_RequestFormSaveAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/form/delete";
			route.proc = Managed_RequestFormDeleteAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/form/submission/list";
			route.proc = Managed_RequestFormSubmissionListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/form/submission/export";
			route.proc = Managed_RequestFormSubmissionExportAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/form/submission/status";
			route.proc = Managed_RequestFormSubmissionStatusAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bAccessPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/access/rule/list";
			route.proc = Managed_RequestAccessRuleListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/access/rule/save";
			route.proc = Managed_RequestAccessRuleSaveAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/access/rule/delete";
			route.proc = Managed_RequestAccessRuleDeleteAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bAuditLogPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/audit-log/list";
			route.proc = Managed_RequestAuditLogListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bImportExportPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/import-export/import/preview";
			route.proc = Managed_RequestImportPreviewAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/import-export/import/commit";
			route.proc = Managed_RequestImportCommitAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/import-export/import/jobs";
			route.proc = Managed_RequestImportJobListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/import-export/import/replay";
			route.proc = Managed_RequestImportReplayAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/import-export/export/json";
			route.proc = Managed_RequestExportJsonAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/import-export/export/jobs";
			route.proc = Managed_RequestExportJobListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/import-export/export/download";
			route.proc = Managed_RequestExportDownloadAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
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

		if ( bCommentPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/comment/status";
			route.proc = Managed_RequestCommentStatusAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_comment;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bCommentPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/comment/delete";
			route.proc = Managed_RequestCommentDeleteAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_comment;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bTagPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/tag/save";
			route.proc = Managed_RequestTagSaveAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_tag;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bTagPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/tag/delete";
			route.proc = Managed_RequestTagDeleteAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_tag;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bTagPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/tag/bind";
			route.proc = Managed_RequestTagBindAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_tag;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bTagPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/tag/content/list";
			route.proc = Managed_RequestTagContentListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_tag;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bTagPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/tag/unbind";
			route.proc = Managed_RequestTagUnbindAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_tag;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bTopicPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/topic/save";
			route.proc = Managed_RequestTopicSaveAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_topic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bTopicPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/topic/delete";
			route.proc = Managed_RequestTopicDeleteAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_topic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bTopicPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/topic/bind";
			route.proc = Managed_RequestTopicBindAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_topic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bTopicPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/topic/bind-content";
			route.proc = Managed_RequestTopicBindContentAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_topic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bTopicPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/topic/content/list";
			route.proc = Managed_RequestTopicContentListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_topic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bTopicPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/topic/unbind";
			route.proc = Managed_RequestTopicUnbindAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_topic;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bSensitivePack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/sensitive/word/save";
			route.proc = Managed_RequestSensitiveWordSaveAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_sensitive;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bSensitivePack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/sensitive/word/delete";
			route.proc = Managed_RequestSensitiveWordDeleteAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_sensitive;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bSensitivePack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/sensitive/log/list";
			route.proc = Managed_RequestSensitiveLogListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_sensitive;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bStaticPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/static/rule/save";
			route.proc = Managed_RequestStaticRuleSaveAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_static;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bStaticPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/static/rule/list";
			route.proc = Managed_RequestStaticRuleListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_static;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bStaticPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/static/rule/delete";
			route.proc = Managed_RequestStaticRuleDeleteAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_static;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bStaticPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/static/generate";
			route.proc = Managed_RequestStaticGeneratePublic;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_static;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bStaticPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/static/task/list";
			route.proc = Managed_RequestStaticTaskListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_static;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bStaticPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/static/task/status";
			route.proc = Managed_RequestStaticTaskStatusAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_static;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bStaticPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/static/artifact/list";
			route.proc = Managed_RequestStaticArtifactListAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_static;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bStaticPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/static/clean";
			route.proc = Managed_RequestStaticCleanAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			route.auth_id = auth_content_static;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/api/plugin/{{PLUGIN_XID}}/pack/meta";
		route.proc = Managed_RequestAbilityPackMetaAdmin;
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
			printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
			goto failed;
		}

		memset(&route, 0, sizeof(route));
		route.path = "/admin/api/plugin/{{PLUGIN_XID}}/pack/list";
		route.proc = Managed_RequestAbilityPackListAdmin;
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

		if ( bSlugPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/slug/check";
			route.proc = Managed_RequestSlugCheckAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/slug/preview";
			route.proc = Managed_RequestSlugPreviewAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/slug/repair";
			route.proc = Managed_RequestSlugRepairAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

			memset(&route, 0, sizeof(route));
			route.path = "/admin/api/plugin/{{PLUGIN_XID}}/slug/history";
			route.proc = Managed_RequestSlugHistoryAdmin;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bViewPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/view/plugin/{{PLUGIN_XID}}";
			route.proc = Managed_RequestAdminView;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bViewPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/view/plugin/{{PLUGIN_XID}}/articles";
			route.proc = Managed_RequestAdminView;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bViewPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/view/plugin/{{PLUGIN_XID}}/drafts";
			route.proc = Managed_RequestDraftsView;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bViewPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/view/plugin/{{PLUGIN_XID}}/editor";
			route.proc = Managed_RequestEditorView;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}

		}

		if ( bLikePack || bViewPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/view/plugin/{{PLUGIN_XID}}/dashboard";
			route.proc = Managed_RequestDashboardView;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

		if ( bCategoryPack ) {
			memset(&route, 0, sizeof(route));
			route.path = "/admin/view/plugin/{{PLUGIN_XID}}/categories";
			route.proc = Managed_RequestCategoriesView;
			route.need_auth = TRUE;
			route.admin_only = TRUE;
			if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) {
				printf("        [ManagedPlugin] route register failed: xid={{PLUGIN_XID}} path=%s\n", route.path);
				goto failed;
			}
		}

{{ABILITY_PACK_ROUTE_REGISTRATIONS}}

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

		if ( bCategoryPack ) {
			memset(&menu, 0, sizeof(menu));
			menu.key = "{{PLUGIN_XID}}.categories";
			menu.parent_id = iRootMenuId;
			menu.title = "\xE6\xA0\x8F\xE7\x9B\xAE\xE7\xAE\xA1\xE7\x90\x86";
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
		}

		memset(&menu, 0, sizeof(menu));
		menu.key = "{{PLUGIN_XID}}.articles";
		menu.parent_id = iRootMenuId;
		menu.title = "\xE6\x96\x87\xE7\xAB\xA0\xE5\x88\x97\xE8\xA1\xA8";
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
		menu.title = "\xE8\x8D\x89\xE7\xA8\xBF\xE7\xAE\xB1";
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

		if ( bLikePack || bViewPack ) {
			memset(&menu, 0, sizeof(menu));
			menu.key = "{{PLUGIN_XID}}.dashboard";
			menu.parent_id = iRootMenuId;
			menu.title = "\xE7\xBB\x9F\xE8\xAE\xA1\xE7\x9C\x8B\xE6\x9D\xBF";
			menu.icon = "layui-icon layui-icon-chart";
			menu.type = 1;
			menu.open_type = "_component";
			menu.href = "/admin/view/plugin/{{PLUGIN_XID}}/dashboard";
			menu.sort = 40;
			menu.visible = TRUE;
			menu.remark = "Managed content metric dashboard";
			if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) {
				printf("        [ManagedPlugin] menu register failed: xid={{PLUGIN_XID}} href=%s\n", menu.href);
				goto failed;
			}
		}

{{ABILITY_PACK_MENU_REGISTRATIONS}}
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
