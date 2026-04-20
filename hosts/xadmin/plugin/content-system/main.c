#include "xs_plugin.h"

#define CS_PLUGIN_XID "content-system"
#define CS_GENERATOR_VERSION "0.1.0"

typedef struct {
	char sMenuTitle[96];
	char sMenuIcon[96];
	int iMenuSort;
} CSConfigState;

typedef struct {
	int64 iTypeId;
	int iCurrentRevision;
	int iAppliedRevision;
	char sPluginXid[128];
	str sSpecJson;
	str sTypeTitle;
} CSTypeSnapshot;

static XAdminPluginHandle G_CSHandle = NULL;
static const char* G_CSRootPath = NULL;
static const char* G_CSPrivateDbPath = NULL;
static CSConfigState G_CSConfig = {
	"Content System",
	"layui-icon layui-icon-template-1",
	990100
};

static const char* G_CSSchemaSql =
	"CREATE TABLE IF NOT EXISTS content_type ("
	"id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"type_key TEXT NOT NULL UNIQUE,"
	"xid TEXT NOT NULL UNIQUE,"
	"name TEXT NOT NULL,"
	"namespace TEXT NOT NULL DEFAULT '',"
	"title TEXT NOT NULL,"
	"description TEXT NOT NULL DEFAULT '',"
	"icon TEXT NOT NULL DEFAULT '',"
	"table_name TEXT NOT NULL DEFAULT '',"
	"field_count INTEGER NOT NULL DEFAULT 0,"
	"spec_json TEXT NOT NULL,"
	"spec_hash TEXT NOT NULL DEFAULT '',"
	"current_revision INTEGER NOT NULL DEFAULT 0,"
	"generated_plugin_xid TEXT NOT NULL DEFAULT '',"
	"create_time INTEGER NOT NULL,"
	"update_time INTEGER NOT NULL"
	");"
	"CREATE TABLE IF NOT EXISTS content_type_revision ("
	"id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"type_id INTEGER NOT NULL,"
	"revision INTEGER NOT NULL,"
	"spec_json TEXT NOT NULL,"
	"spec_hash TEXT NOT NULL DEFAULT '',"
	"note TEXT NOT NULL DEFAULT '',"
	"generator_version TEXT NOT NULL DEFAULT '0.1.0',"
	"create_time INTEGER NOT NULL,"
	"UNIQUE(type_id, revision)"
	");"
	"CREATE TABLE IF NOT EXISTS content_generated_plugin ("
	"id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"type_id INTEGER NOT NULL UNIQUE,"
	"plugin_xid TEXT NOT NULL UNIQUE,"
	"applied_revision INTEGER NOT NULL DEFAULT 0,"
	"status TEXT NOT NULL DEFAULT 'generated',"
	"managed_json TEXT NOT NULL DEFAULT '{}',"
	"last_generate_time INTEGER NOT NULL DEFAULT 0,"
	"last_upgrade_time INTEGER NOT NULL DEFAULT 0,"
	"create_time INTEGER NOT NULL,"
	"update_time INTEGER NOT NULL"
	");"
	"CREATE TABLE IF NOT EXISTS content_upgrade_record ("
	"id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"type_id INTEGER NOT NULL,"
	"from_revision INTEGER NOT NULL DEFAULT 0,"
	"to_revision INTEGER NOT NULL DEFAULT 0,"
	"plan_json TEXT NOT NULL DEFAULT '{}',"
	"result TEXT NOT NULL DEFAULT 'pending',"
	"create_time INTEGER NOT NULL"
	");"
	"CREATE INDEX IF NOT EXISTS idx_content_type_update_time ON content_type(update_time DESC);"
	"CREATE INDEX IF NOT EXISTS idx_content_type_revision_type_rev ON content_type_revision(type_id, revision DESC);"
	"CREATE INDEX IF NOT EXISTS idx_content_upgrade_record_type ON content_upgrade_record(type_id, create_time DESC);";

XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int idx, void* ptr)
{
	if ( idx == XADMIN_GLOBAL_PLUGIN_ROOT_PATH ) {
		G_CSRootPath = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_PRIVATE_DB_PATH ) {
		G_CSPrivateDbPath = (const char*)ptr;
	}
}

void CS_ConfigReset(void)
{
	memset(&G_CSConfig, 0, sizeof(G_CSConfig));
	snprintf(G_CSConfig.sMenuTitle, sizeof(G_CSConfig.sMenuTitle), "%s", "Content System");
	snprintf(G_CSConfig.sMenuIcon, sizeof(G_CSConfig.sMenuIcon), "%s", "layui-icon layui-icon-template-1");
	G_CSConfig.iMenuSort = 990100;
}

void CS_CopyText(char* sDest, size_t iCap, const char* sValue, const char* sFallback)
{
	if ( (sDest == NULL) || (iCap == 0) ) {
		return;
	}
	snprintf(sDest, iCap, "%s", (sValue && sValue[0]) ? sValue : (sFallback ? sFallback : ""));
}

bool CS_IsBlank(const char* sText)
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

bool CS_IsValidXid(const char* sXid)
{
	size_t iLen;

	if ( (sXid == NULL) || (sXid[0] == '\0') ) {
		return FALSE;
	}
	iLen = strlen(sXid);
	if ( (iLen <= 0) || (iLen > 96) ) {
		return FALSE;
	}
	for ( size_t i = 0; i < iLen; i++ ) {
		char ch = sXid[i];
		if ( ((ch >= 'a') && (ch <= 'z'))
			|| ((ch >= 'A') && (ch <= 'Z'))
			|| ((ch >= '0') && (ch <= '9'))
			|| (ch == '.')
			|| (ch == '_')
			|| (ch == '-') ) {
			continue;
		}
		return FALSE;
	}
	return TRUE;
}

bool CS_IsValidName(const char* sName)
{
	size_t iLen;

	if ( (sName == NULL) || (sName[0] == '\0') ) {
		return FALSE;
	}
	iLen = strlen(sName);
	if ( (iLen <= 0) || (iLen > 64) ) {
		return FALSE;
	}
	for ( size_t i = 0; i < iLen; i++ ) {
		char ch = sName[i];
		if ( ((ch >= 'a') && (ch <= 'z'))
			|| ((ch >= 'A') && (ch <= 'Z'))
			|| ((ch >= '0') && (ch <= '9'))
			|| (ch == '_')
			|| (ch == '-') ) {
			continue;
		}
		return FALSE;
	}
	return TRUE;
}

bool CS_OpenDb(sqlite3** ppDb)
{
	sqlite3* pDb = NULL;
	int iRet;

	if ( ppDb ) {
		*ppDb = NULL;
	}
	if ( (ppDb == NULL) || (G_CSPrivateDbPath == NULL) || (G_CSPrivateDbPath[0] == '\0') ) {
		return FALSE;
	}
	iRet = sqlite3_open_v2(
		G_CSPrivateDbPath,
		&pDb,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
		NULL);
	if ( iRet != SQLITE_OK ) {
		if ( pDb ) {
			sqlite3_close(pDb);
		}
		return FALSE;
	}
	sqlite3_busy_timeout(pDb, 3000);
	*ppDb = pDb;
	return TRUE;
}

void CS_CloseDb(sqlite3* pDb)
{
	if ( pDb ) {
		sqlite3_close(pDb);
	}
}

bool CS_ExecSql(sqlite3* pDb, const char* sSql)
{
	char* sError = NULL;
	int iRet;

	if ( (pDb == NULL) || (sSql == NULL) ) {
		return FALSE;
	}
	iRet = sqlite3_exec(pDb, sSql, NULL, NULL, &sError);
	if ( sError ) {
		sqlite3_free(sError);
	}
	return iRet == SQLITE_OK;
}

bool CS_EnsureSchema(void)
{
	sqlite3* pDb = NULL;
	bool bOK = FALSE;

	if ( !CS_OpenDb(&pDb) ) {
		return FALSE;
	}
	bOK = CS_ExecSql(pDb, G_CSSchemaSql);
	CS_CloseDb(pDb);
	return bOK;
}

xvalue CS_CreateResult(bool bResult, const char* sMessage)
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

void CS_SendJsonValue(XS_ResponseObject objResp, xvalue objValue)
{
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(objValue, FALSE, &iSize);

	if ( sJson ) {
		http_reply(objResp, 200, "Content-Type: application/json\r\n", sJson, iSize);
		xrtFree(sJson);
	} else {
		http_reply(objResp, 500, "Content-Type: application/json\r\n", "{\"result\":false,\"message\":\"json encode failed\"}", 0);
	}
	xvoUnref(objValue);
}

void CS_SendError(XS_ResponseObject objResp, const char* sMessage)
{
	xvalue tblRet = CS_CreateResult(FALSE, sMessage ? sMessage : "request failed");

	if ( tblRet == NULL ) {
		http_reply(objResp, 500, "Content-Type: application/json\r\n", "{\"result\":false,\"message\":\"request failed\"}", 0);
		return;
	}
	CS_SendJsonValue(objResp, tblRet);
}

xvalue CS_ParseJsonBody(XS_RequestObject objReq)
{
	xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));

	if ( (tblForm == NULL) || (xvoType(tblForm) != XVO_DT_TABLE) ) {
		if ( tblForm ) {
			xvoUnref(tblForm);
		}
		return NULL;
	}
	return tblForm;
}

void CS_SetTimeText(xvalue tblItem, const char* sKey, int iKeyLen, xtime iTime)
{
	str sValue = (iTime > 0) ? xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME) : xrtCopyStr("", 0);
	xvoTableSetText(tblItem, sKey, iKeyLen, sValue ? sValue : (str)"", 0, TRUE);
}

str CS_StringifyJson(xvalue objValue)
{
	size_t iSize = 0;
	return xrtStringifyJSON(objValue, FALSE, &iSize);
}

unsigned long long CS_HashBuffer(const char* sText)
{
	unsigned long long iHash = 1469598103934665603ULL;
	const unsigned char* p = (const unsigned char*)sText;

	if ( p == NULL ) {
		return iHash;
	}
	while ( *p ) {
		iHash ^= (unsigned long long)(*p++);
		iHash *= 1099511628211ULL;
	}
	return iHash;
}

str CS_BuildHashText(const char* sText)
{
	return xrtFormat("%016llx", CS_HashBuffer(sText));
}

str CS_JsonEscape(const char* sText)
{
	size_t iCap;
	size_t iLen = 0;
	char* sOut;

	if ( sText == NULL ) {
		return xrtCopyStr("", 0);
	}
	iCap = (strlen(sText) * 2) + 32;
	sOut = (char*)xrtMalloc(iCap);
	if ( sOut == NULL ) {
		return NULL;
	}
	for ( const unsigned char* p = (const unsigned char*)sText; *p; p++ ) {
		if ( (*p == '\\') || (*p == '"') ) {
			sOut[iLen++] = '\\';
			sOut[iLen++] = (char)(*p);
		} else if ( *p == '\r' ) {
			sOut[iLen++] = '\\';
			sOut[iLen++] = 'r';
		} else if ( *p == '\n' ) {
			sOut[iLen++] = '\\';
			sOut[iLen++] = 'n';
		} else if ( *p == '\t' ) {
			sOut[iLen++] = '\\';
			sOut[iLen++] = 't';
		} else {
			sOut[iLen++] = (char)(*p);
		}
	}
	sOut[iLen] = '\0';
	return sOut;
}

static xvalue CS_GetTableValue(xvalue tblData, const char* sKey);
xvalue CS_FindFieldByName(xvalue arrFields, const char* sName);

xvalue CS_FindIndexByName(xvalue arrIndexes, const char* sName)
{
	if ( (arrIndexes == NULL) || (xvoType(arrIndexes) != XVO_DT_ARRAY) || CS_IsBlank(sName) ) {
		return NULL;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrIndexes); i++ ) {
		xvalue tblIndex = xvoArrayGetValue(arrIndexes, i);
		const char* sIndexName = xvoTableGetText(tblIndex, "name", 4);
		if ( !CS_IsBlank(sIndexName) && (strcmp(sIndexName, sName) == 0) ) {
			return tblIndex;
		}
	}
	return NULL;
}

const char* CS_GetFieldSemanticRole(xvalue tblField)
{
	xvalue tblSemantic = CS_GetTableValue(tblField, "semantic");
	return ((tblSemantic != NULL) && (xvoType(tblSemantic) == XVO_DT_TABLE))
		? xvoTableGetText(tblSemantic, "role", 4)
		: NULL;
}

bool CS_IsKnownSemanticRole(const char* sRole)
{
	if ( CS_IsBlank(sRole) ) {
		return FALSE;
	}
	return (strcmp(sRole, "title") == 0)
		|| (strcmp(sRole, "slug") == 0)
		|| (strcmp(sRole, "summary") == 0)
		|| (strcmp(sRole, "content") == 0)
		|| (strcmp(sRole, "cover") == 0)
		|| (strcmp(sRole, "status") == 0)
		|| (strcmp(sRole, "author") == 0)
		|| (strcmp(sRole, "publishedAt") == 0)
		|| (strcmp(sRole, "sort") == 0)
		|| (strcmp(sRole, "category") == 0)
		|| (strcmp(sRole, "tag") == 0);
}

bool CS_IsKnownStorageType(const char* sType)
{
	if ( CS_IsBlank(sType) ) {
		return FALSE;
	}
	return (strcmp(sType, "text") == 0)
		|| (strcmp(sType, "int") == 0)
		|| (strcmp(sType, "integer") == 0)
		|| (strcmp(sType, "real") == 0)
		|| (strcmp(sType, "float") == 0)
		|| (strcmp(sType, "number") == 0)
		|| (strcmp(sType, "bool") == 0)
		|| (strcmp(sType, "boolean") == 0)
		|| (strcmp(sType, "date") == 0)
		|| (strcmp(sType, "datetime") == 0)
		|| (strcmp(sType, "time") == 0)
		|| (strcmp(sType, "json") == 0);
}

bool CS_IsKnownComponentType(const char* sType)
{
	if ( CS_IsBlank(sType) ) {
		return FALSE;
	}
	return (strcmp(sType, "input") == 0)
		|| (strcmp(sType, "textarea") == 0)
		|| (strcmp(sType, "number") == 0)
		|| (strcmp(sType, "int") == 0)
		|| (strcmp(sType, "image") == 0)
		|| (strcmp(sType, "file") == 0)
		|| (strcmp(sType, "select") == 0)
		|| (strcmp(sType, "combobox") == 0)
		|| (strcmp(sType, "radio") == 0)
		|| (strcmp(sType, "checkbox") == 0)
		|| (strcmp(sType, "checklist") == 0)
		|| (strcmp(sType, "switch") == 0)
		|| (strcmp(sType, "date") == 0)
		|| (strcmp(sType, "datetime") == 0)
		|| (strcmp(sType, "time") == 0)
		|| (strcmp(sType, "markdown") == 0)
		|| (strcmp(sType, "richtext") == 0)
		|| (strcmp(sType, "code") == 0)
		|| (strcmp(sType, "icon") == 0)
		|| (strcmp(sType, "images") == 0)
		|| (strcmp(sType, "files") == 0);
}

bool CS_IsSingletonSemanticRole(const char* sRole)
{
	if ( CS_IsBlank(sRole) ) {
		return FALSE;
	}
	return (strcmp(sRole, "title") == 0)
		|| (strcmp(sRole, "slug") == 0)
		|| (strcmp(sRole, "summary") == 0)
		|| (strcmp(sRole, "cover") == 0)
		|| (strcmp(sRole, "status") == 0)
		|| (strcmp(sRole, "author") == 0)
		|| (strcmp(sRole, "publishedAt") == 0);
}

bool CS_ArrayHasDuplicateSemanticRole(xvalue arrFields, const char* sRole, int iSkipIndex)
{
	if ( (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) || CS_IsBlank(sRole) ) {
		return FALSE;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
		xvalue tblField;
		const char* sFieldRole;

		if ( i == iSkipIndex ) {
			continue;
		}
		tblField = xvoArrayGetValue(arrFields, i);
		if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
			continue;
		}
		sFieldRole = CS_GetFieldSemanticRole(tblField);
		if ( !CS_IsBlank(sFieldRole) && (strcmp(sFieldRole, sRole) == 0) ) {
			return TRUE;
		}
	}
	return FALSE;
}

bool CS_IsKnownStatusFlow(const char* sValue)
{
	if ( CS_IsBlank(sValue) ) {
		return FALSE;
	}
	return (strcmp(sValue, "draft-published") == 0)
		|| (strcmp(sValue, "draft-review-published") == 0);
}

bool CS_IsKnownAuthorMode(const char* sValue)
{
	if ( CS_IsBlank(sValue) ) {
		return FALSE;
	}
	return (strcmp(sValue, "admin-only") == 0)
		|| (strcmp(sValue, "member-only") == 0)
		|| (strcmp(sValue, "admin-or-member") == 0);
}

bool CS_ValidateEntityFieldSemanticRole(xvalue arrFields, const char* sFieldName, const char* sExpectedRole, const char* sLabel, str* psError)
{
	xvalue tblField;
	const char* sRole;

	if ( psError ) *psError = NULL;
	if ( CS_IsBlank(sFieldName) ) {
		return TRUE;
	}
	tblField = CS_FindFieldByName(arrFields, sFieldName);
	if ( tblField == NULL ) {
		if ( psError ) *psError = xrtFormat("%s references an unknown field", sLabel ? sLabel : "entity field");
		return FALSE;
	}
	if ( CS_IsBlank(sExpectedRole) ) {
		return TRUE;
	}
	sRole = CS_GetFieldSemanticRole(tblField);
	if ( !CS_IsBlank(sRole) && (strcmp(sRole, sExpectedRole) != 0) ) {
		if ( psError ) *psError = xrtFormat("%s should reference a field with semantic.role = %s", sLabel ? sLabel : "entity field", sExpectedRole);
		return FALSE;
	}
	return TRUE;
}

str CS_StringifyValueOrEmptyArray(xvalue objValue)
{
	xvalue arrTemp = NULL;
	str sJson = NULL;

	if ( (objValue != NULL) && (xvoType(objValue) == XVO_DT_ARRAY) ) {
		return xrtStringifyJSON(objValue, FALSE, NULL);
	}
	arrTemp = xvoCreateArray();
	sJson = xrtStringifyJSON(arrTemp, FALSE, NULL);
	xvoUnref(arrTemp);
	return sJson;
}

xvalue CS_GetSpecPresentationGroups(xvalue tblSpec)
{
	xvalue tblPresentation = CS_GetTableValue(tblSpec, "presentation");
	return tblPresentation ? xvoTableGetValue(tblPresentation, "groups", 6) : NULL;
}

xvalue CS_GetSpecCapabilitySlots(xvalue tblSpec)
{
	return CS_GetTableValue(tblSpec, "capabilitySlots");
}

static xvalue CS_GetTableValue(xvalue tblData, const char* sKey)
{
	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || (sKey == NULL) ) {
		return NULL;
	}
	return xvoTableGetValue(tblData, sKey, (int)strlen(sKey));
}

const char* CS_GetNestedText(xvalue tblData, const char* sKey1, const char* sKey2)
{
	xvalue tblInner = CS_GetTableValue(tblData, sKey1);

	if ( (tblInner == NULL) || (xvoType(tblInner) != XVO_DT_TABLE) ) {
		return NULL;
	}
	return xvoTableGetText(tblInner, sKey2, (int)strlen(sKey2));
}

bool CS_ArrayHasDuplicateText(xvalue arrData, const char* sKey, const char* sValue, int iSkipIndex)
{
	if ( (arrData == NULL) || (xvoType(arrData) != XVO_DT_ARRAY) || CS_IsBlank(sKey) || CS_IsBlank(sValue) ) {
		return FALSE;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrData); i++ ) {
		xvalue tblItem;
		const char* sText;

		if ( i == iSkipIndex ) {
			continue;
		}
		tblItem = xvoArrayGetValue(arrData, i);
		if ( (tblItem == NULL) || (xvoType(tblItem) != XVO_DT_TABLE) ) {
			continue;
		}
		sText = xvoTableGetText(tblItem, sKey, (int)strlen(sKey));
		if ( sText && (strcmp(sText, sValue) == 0) ) {
			return TRUE;
		}
	}
	return FALSE;
}

xvalue CS_FindGroupByKey(xvalue arrGroups, const char* sKey)
{
	if ( (arrGroups == NULL) || (xvoType(arrGroups) != XVO_DT_ARRAY) || CS_IsBlank(sKey) ) {
		return NULL;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrGroups); i++ ) {
		xvalue tblGroup = xvoArrayGetValue(arrGroups, i);
		const char* sGroupKey = xvoTableGetText(tblGroup, "key", 3);
		if ( sGroupKey && (strcmp(sGroupKey, sKey) == 0) ) {
			return tblGroup;
		}
	}
	return NULL;
}

bool CS_ValidatePresentationGroups(xvalue arrGroups, str* psError)
{
	if ( psError ) *psError = NULL;
	if ( arrGroups == NULL ) {
		return TRUE;
	}
	if ( xvoType(arrGroups) != XVO_DT_ARRAY ) {
		if ( psError ) *psError = xrtCopyStr("presentation.groups must be an array", 0);
		return FALSE;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrGroups); i++ ) {
		xvalue tblGroup = xvoArrayGetValue(arrGroups, i);
		const char* sKey;
		const char* sTitle;

		if ( (tblGroup == NULL) || (xvoType(tblGroup) != XVO_DT_TABLE) ) {
			if ( psError ) *psError = xrtFormat("presentation.groups[%d] must be an object", i);
			return FALSE;
		}
		sKey = xvoTableGetText(tblGroup, "key", 3);
		sTitle = xvoTableGetText(tblGroup, "title", 5);
		if ( !CS_IsValidName(sKey) ) {
			if ( psError ) *psError = xrtFormat("presentation.groups[%d].key is invalid", i);
			return FALSE;
		}
		if ( CS_IsBlank(sTitle) ) {
			if ( psError ) *psError = xrtFormat("presentation.groups[%d].title is required", i);
			return FALSE;
		}
		if ( CS_ArrayHasDuplicateText(arrGroups, "key", sKey, i) ) {
			if ( psError ) *psError = xrtFormat("presentation.groups[%d].key is duplicated", i);
			return FALSE;
		}
	}
	return TRUE;
}

bool CS_ValidateCapabilitySlots(xvalue arrSlots, str* psError)
{
	if ( psError ) *psError = NULL;
	if ( arrSlots == NULL ) {
		return TRUE;
	}
	if ( xvoType(arrSlots) != XVO_DT_ARRAY ) {
		if ( psError ) *psError = xrtCopyStr("capabilitySlots must be an array", 0);
		return FALSE;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrSlots); i++ ) {
		xvalue tblSlot = xvoArrayGetValue(arrSlots, i);
		const char* sKey;
		const char* sServiceXid;
		const char* sRouteBase;
		const char* sSurface;
		const char* sMode;
		xvalue tblMount;
		xvalue tblConfig;

		if ( (tblSlot == NULL) || (xvoType(tblSlot) != XVO_DT_TABLE) ) {
			if ( psError ) *psError = xrtFormat("capabilitySlots[%d] must be an object", i);
			return FALSE;
		}
		sKey = xvoTableGetText(tblSlot, "key", 3);
		if ( !CS_IsValidName(sKey) ) {
			if ( psError ) *psError = xrtFormat("capabilitySlots[%d].key is invalid", i);
			return FALSE;
		}
		if ( CS_ArrayHasDuplicateText(arrSlots, "key", sKey, i) ) {
			if ( psError ) *psError = xrtFormat("capabilitySlots[%d].key is duplicated", i);
			return FALSE;
		}
		sServiceXid = xvoTableGetText(tblSlot, "serviceXid", 10);
		if ( (!CS_IsBlank(sServiceXid)) && !CS_IsValidXid(sServiceXid) ) {
			if ( psError ) *psError = xrtFormat("capabilitySlots[%d].serviceXid is invalid", i);
			return FALSE;
		}
		sRouteBase = xvoTableGetText(tblSlot, "routeBase", 9);
		if ( !CS_IsBlank(sRouteBase) ) {
			size_t iLen = strlen(sRouteBase);
			if ( (sRouteBase[0] != '/') || (iLen > 96) ) {
				if ( psError ) *psError = xrtFormat("capabilitySlots[%d].routeBase is invalid", i);
				return FALSE;
			}
			for ( size_t j = 1; j < iLen; j++ ) {
				char c = sRouteBase[j];
				bool bOK = ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '/') || (c == '-') || (c == '_');
				if ( !bOK ) {
					if ( psError ) *psError = xrtFormat("capabilitySlots[%d].routeBase is invalid", i);
					return FALSE;
				}
			}
		}
		tblMount = CS_GetTableValue(tblSlot, "mount");
		if ( (tblMount != NULL) && (xvoType(tblMount) != XVO_DT_TABLE) ) {
			if ( psError ) *psError = xrtFormat("capabilitySlots[%d].mount must be an object", i);
			return FALSE;
		}
		sSurface = tblMount ? xvoTableGetText(tblMount, "surface", 7) : NULL;
		if ( !CS_IsBlank(sSurface)
			&& strcmp(sSurface, "admin.record-list") != 0
			&& strcmp(sSurface, "admin.record-detail") != 0
			&& strcmp(sSurface, "admin.record-form") != 0
			&& strcmp(sSurface, "public.record-list") != 0
			&& strcmp(sSurface, "public.record-detail") != 0 ) {
			if ( psError ) *psError = xrtFormat("capabilitySlots[%d].mount.surface is invalid", i);
			return FALSE;
		}
		sMode = tblMount ? xvoTableGetText(tblMount, "mode", 4) : NULL;
		if ( !CS_IsBlank(sMode) && strcmp(sMode, "optional") != 0 && strcmp(sMode, "required") != 0 ) {
			if ( psError ) *psError = xrtFormat("capabilitySlots[%d].mount.mode is invalid", i);
			return FALSE;
		}
		tblConfig = CS_GetTableValue(tblSlot, "configSchema");
		if ( (tblConfig != NULL) && (xvoType(tblConfig) != XVO_DT_TABLE) ) {
			if ( psError ) *psError = xrtFormat("capabilitySlots[%d].configSchema must be an object", i);
			return FALSE;
		}
	}
	return TRUE;
}

bool CS_ValidateFieldOptionalBool(xvalue tblField, const char* sKey, int iIndex, str* psError);
bool CS_ValidateFieldOptionalLayoutSpan(xvalue tblField, int iIndex, str* psError);
bool CS_ValidateFieldDefaultValue(xvalue tblField, int iIndex, str* psError);
bool CS_ValidateFieldComponentCompatibility(xvalue tblField, int iIndex, str* psError);
bool CS_ValidateIndexes(xvalue arrIndexes, xvalue arrFields, str* psError);
bool CS_TextIsInteger(const char* sText);

bool CS_ValueIsMissing(xvalue objValue)
{
	return (objValue == NULL) || (xvoType(objValue) == XVO_DT_NULL);
}

bool CS_TableHasKey(xvalue tblData, const char* sKey)
{
	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || CS_IsBlank(sKey) ) {
		return FALSE;
	}
	return xvoTableExists(tblData, (str)sKey, (uint32)strlen(sKey));
}

char CS_LowerAsciiInline(char c)
{
	if ( (c >= 'A') && (c <= 'Z') ) {
		return (char)(c - 'A' + 'a');
	}
	return c;
}

bool CS_TryReadBoolLike(xvalue objValue, bool* pbValue)
{
	char sBuf[16];
	const char* sText;
	size_t iLen;
	size_t iStart = 0;
	size_t iEnd;

	if ( pbValue ) *pbValue = FALSE;
	if ( CS_ValueIsMissing(objValue) ) {
		if ( pbValue ) *pbValue = FALSE;
		return TRUE;
	}
	if ( xvoType(objValue) == XVO_DT_BOOL ) {
		return TRUE;
	}
	if ( xvoType(objValue) == XVO_DT_INT ) {
		int64 iValue = xvoGetInt(objValue);
		if ( (iValue == 0) || (iValue == 1) ) {
			if ( pbValue ) *pbValue = (iValue != 0);
			return TRUE;
		}
		return FALSE;
	}
	if ( xvoType(objValue) != XVO_DT_TEXT ) {
		return FALSE;
	}
	sText = xvoGetText(objValue);
	if ( sText == NULL ) {
		if ( pbValue ) *pbValue = FALSE;
		return TRUE;
	}
	iLen = strlen(sText);
	iEnd = iLen;
	while ( (iStart < iLen) && ((sText[iStart] == ' ') || (sText[iStart] == '\t') || (sText[iStart] == '\r') || (sText[iStart] == '\n')) ) iStart++;
	while ( (iEnd > iStart) && ((sText[iEnd - 1] == ' ') || (sText[iEnd - 1] == '\t') || (sText[iEnd - 1] == '\r') || (sText[iEnd - 1] == '\n')) ) iEnd--;
	if ( (iEnd - iStart) >= sizeof(sBuf) ) {
		return FALSE;
	}
	for ( size_t i = iStart; i < iEnd; i++ ) {
		sBuf[i - iStart] = CS_LowerAsciiInline(sText[i]);
	}
	sBuf[iEnd - iStart] = '\0';
	if ( (strcmp(sBuf, "true") == 0) || (strcmp(sBuf, "1") == 0) || (strcmp(sBuf, "yes") == 0) || (strcmp(sBuf, "on") == 0) ) {
		if ( pbValue ) *pbValue = TRUE;
		return TRUE;
	}
	if ( (strcmp(sBuf, "false") == 0) || (strcmp(sBuf, "0") == 0) || (strcmp(sBuf, "no") == 0) || (strcmp(sBuf, "off") == 0) || (strcmp(sBuf, "") == 0) ) {
		if ( pbValue ) *pbValue = FALSE;
		return TRUE;
	}
	return FALSE;
}

bool CS_NormalizeTableBool(xvalue tblData, const char* sKey)
{
	xvalue objValue;
	bool bValue = FALSE;

	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || CS_IsBlank(sKey) ) {
		return TRUE;
	}
	if ( !CS_TableHasKey(tblData, sKey) ) {
		return TRUE;
	}
	objValue = xvoTableGetValue(tblData, sKey, (int)strlen(sKey));
	if ( CS_ValueIsMissing(objValue) ) {
		return TRUE;
	}
	if ( xvoType(objValue) == XVO_DT_BOOL ) {
		return TRUE;
	}
	if ( !CS_TryReadBoolLike(objValue, &bValue) ) {
		return FALSE;
	}
	xvoTableSetBool(tblData, sKey, (int)strlen(sKey), bValue);
	return TRUE;
}

bool CS_NormalizeTableInteger(xvalue tblData, const char* sKey, int iMin, int iMax)
{
	xvalue objValue;
	int64 iValue;
	const char* sText;

	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || CS_IsBlank(sKey) ) {
		return TRUE;
	}
	if ( !CS_TableHasKey(tblData, sKey) ) {
		return TRUE;
	}
	objValue = xvoTableGetValue(tblData, sKey, (int)strlen(sKey));
	if ( CS_ValueIsMissing(objValue) ) {
		return TRUE;
	}
	if ( xvoType(objValue) == XVO_DT_INT ) {
		iValue = xvoGetInt(objValue);
	} else if ( xvoType(objValue) == XVO_DT_TEXT ) {
		sText = xvoGetText(objValue);
		if ( CS_IsBlank(sText) ) {
			return TRUE;
		}
		if ( !CS_TextIsInteger(sText) ) {
			return FALSE;
		}
		iValue = atoll(sText);
	} else {
		return FALSE;
	}
	if ( (iValue < iMin) || (iValue > iMax) ) {
		return FALSE;
	}
	xvoTableSetInt(tblData, sKey, (int)strlen(sKey), iValue);
	return TRUE;
}

bool CS_ValidateField(xvalue tblField, int iIndex, str* psError)
{
	const char* sName;
	const char* sTitle;
	const char* sStorageType;
	const char* sComponentType;
	const char* sGroup;
	xvalue tblSemantic;
	xvalue objRole;
	const char* sRole;

	if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtFormat("field[%d] must be an object", iIndex);
		return FALSE;
	}
	sName = xvoTableGetText(tblField, "name", 4);
	sTitle = xvoTableGetText(tblField, "title", 5);
	sStorageType = CS_GetNestedText(tblField, "storage", "type");
	sComponentType = CS_GetNestedText(tblField, "component", "type");
	sGroup = xvoTableGetText(tblField, "group", 5);
	tblSemantic = CS_GetTableValue(tblField, "semantic");
	objRole = ((tblSemantic != NULL) && (xvoType(tblSemantic) == XVO_DT_TABLE))
		? xvoTableGetValue(tblSemantic, "role", 4)
		: NULL;
	sRole = (objRole && (xvoType(objRole) == XVO_DT_TEXT)) ? xvoGetText(objRole) : NULL;
	if ( !CS_IsValidName(sName) ) {
		if ( psError ) *psError = xrtFormat("field[%d].name is invalid", iIndex);
		return FALSE;
	}
	if ( CS_IsBlank(sTitle) ) {
		if ( psError ) *psError = xrtFormat("field[%d].title is required", iIndex);
		return FALSE;
	}
	if ( CS_IsBlank(sStorageType) ) {
		if ( psError ) *psError = xrtFormat("field[%d].storage.type is required", iIndex);
		return FALSE;
	}
	if ( !CS_IsKnownStorageType(sStorageType) ) {
		if ( psError ) *psError = xrtFormat("field[%d].storage.type is invalid", iIndex);
		return FALSE;
	}
	if ( CS_IsBlank(sComponentType) ) {
		if ( psError ) *psError = xrtFormat("field[%d].component.type is required", iIndex);
		return FALSE;
	}
	if ( !CS_IsKnownComponentType(sComponentType) ) {
		if ( psError ) *psError = xrtFormat("field[%d].component.type is invalid", iIndex);
		return FALSE;
	}
	if ( (!CS_IsBlank(sGroup)) && !CS_IsValidName(sGroup) ) {
		if ( psError ) *psError = xrtFormat("field[%d].group is invalid", iIndex);
		return FALSE;
	}
	if ( (tblSemantic != NULL) && (xvoType(tblSemantic) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtFormat("field[%d].semantic must be an object", iIndex);
		return FALSE;
	}
	if ( (objRole != NULL) && (xvoType(objRole) != XVO_DT_TEXT) ) {
		if ( psError ) *psError = xrtFormat("field[%d].semantic.role must be text", iIndex);
		return FALSE;
	}
	if ( !CS_IsBlank(sRole) && !CS_IsKnownSemanticRole(sRole) ) {
		if ( psError ) *psError = xrtFormat("field[%d].semantic.role is invalid", iIndex);
		return FALSE;
	}
	if ( !CS_ValidateFieldOptionalBool(tblField, "required", iIndex, psError)
		|| !CS_ValidateFieldOptionalBool(tblField, "nullable", iIndex, psError)
		|| !CS_ValidateFieldOptionalBool(tblField, "readonly", iIndex, psError)
		|| !CS_ValidateFieldOptionalBool(tblField, "disabled", iIndex, psError)
		|| !CS_ValidateFieldOptionalBool(tblField, "showInForm", iIndex, psError)
		|| !CS_ValidateFieldOptionalBool(tblField, "showInList", iIndex, psError)
		|| !CS_ValidateFieldOptionalBool(tblField, "showInDetail", iIndex, psError)
		|| !CS_ValidateFieldOptionalBool(tblField, "sortable", iIndex, psError)
		|| !CS_ValidateFieldOptionalBool(tblField, "filterable", iIndex, psError)
		|| !CS_ValidateFieldOptionalBool(tblField, "searchable", iIndex, psError)
		|| !CS_ValidateFieldOptionalLayoutSpan(tblField, iIndex, psError)
		|| !CS_ValidateFieldDefaultValue(tblField, iIndex, psError)
		|| !CS_ValidateFieldComponentCompatibility(tblField, iIndex, psError) ) {
		return FALSE;
	}
	return TRUE;
}

bool CS_ValidateFieldOptionalBool(xvalue tblField, const char* sKey, int iIndex, str* psError)
{
	xvalue objValue;

	if ( psError ) *psError = NULL;
	if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) || CS_IsBlank(sKey) ) {
		return TRUE;
	}
	objValue = xvoTableGetValue(tblField, sKey, (int)strlen(sKey));
	if ( !CS_ValueIsMissing(objValue) && !CS_NormalizeTableBool(tblField, sKey) ) {
		if ( psError ) *psError = xrtFormat("field[%d].%s must be a boolean", iIndex, sKey);
		return FALSE;
	}
	return TRUE;
}

bool CS_ValidateFieldOptionalLayoutSpan(xvalue tblField, int iIndex, str* psError)
{
	xvalue objValue;
	int iSpan;

	if ( psError ) *psError = NULL;
	if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
		return TRUE;
	}
	objValue = xvoTableGetValue(tblField, "layoutSpan", 10);
	if ( CS_ValueIsMissing(objValue) ) {
		return TRUE;
	}
	if ( !CS_NormalizeTableInteger(tblField, "layoutSpan", 1, 2) ) {
		if ( psError ) *psError = xrtFormat("field[%d].layoutSpan must be an integer", iIndex);
		return FALSE;
	}
	objValue = xvoTableGetValue(tblField, "layoutSpan", 10);
	if ( xvoType(objValue) == XVO_DT_INT ) {
		iSpan = (int)xvoGetInt(objValue);
	} else if ( xvoType(objValue) == XVO_DT_FLOAT ) {
		double fSpan = xvoGetFloat(objValue);
		iSpan = (int)fSpan;
		if ( ((double)iSpan) != fSpan ) {
			if ( psError ) *psError = xrtFormat("field[%d].layoutSpan must be an integer", iIndex);
			return FALSE;
		}
	} else {
		if ( psError ) *psError = xrtFormat("field[%d].layoutSpan must be an integer", iIndex);
		return FALSE;
	}
	if ( (iSpan < 1) || (iSpan > 2) ) {
		if ( psError ) *psError = xrtFormat("field[%d].layoutSpan must be 1 or 2", iIndex);
		return FALSE;
	}
	return TRUE;
}

bool CS_IsAsciiSpace(char c)
{
	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

char CS_ToLowerAscii(char c)
{
	if ( (c >= 'A') && (c <= 'Z') ) {
		return (char)(c - 'A' + 'a');
	}
	return c;
}

bool CS_TextEqualsIgnoreCase(const char* sLeft, const char* sRight)
{
	size_t i = 0;

	if ( (sLeft == NULL) || (sRight == NULL) ) {
		return FALSE;
	}
	while ( sLeft[i] && sRight[i] ) {
		if ( CS_ToLowerAscii(sLeft[i]) != CS_ToLowerAscii(sRight[i]) ) {
			return FALSE;
		}
		i++;
	}
	return sLeft[i] == '\0' && sRight[i] == '\0';
}

bool CS_TextIsInteger(const char* sText)
{
	const char* sPtr = sText;

	if ( CS_IsBlank(sText) ) {
		return FALSE;
	}
	while ( CS_IsAsciiSpace(*sPtr) ) sPtr++;
	if ( (*sPtr == '+') || (*sPtr == '-') ) sPtr++;
	if ( (*sPtr < '0') || (*sPtr > '9') ) {
		return FALSE;
	}
	while ( (*sPtr >= '0') && (*sPtr <= '9') ) sPtr++;
	while ( CS_IsAsciiSpace(*sPtr) ) sPtr++;
	return *sPtr == '\0';
}

bool CS_TextIsNumber(const char* sText)
{
	char* sEnd = NULL;

	if ( CS_IsBlank(sText) ) {
		return FALSE;
	}
	strtod(sText, &sEnd);
	if ( sEnd == sText ) {
		return FALSE;
	}
	while ( sEnd && CS_IsAsciiSpace(*sEnd) ) sEnd++;
	return (sEnd != NULL) && (*sEnd == '\0');
}

bool CS_FloatIsInteger(double fValue)
{
	int64 iValue = (int64)fValue;
	return ((double)iValue) == fValue;
}

const char* CS_NormalizeStorageTypeName(const char* sStorageType)
{
	if ( CS_IsBlank(sStorageType) ) return "text";
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

bool CS_ValueMatchesStorageType(xvalue objValue, const char* sStorageType)
{
	const char* sType = CS_NormalizeStorageTypeName(sStorageType);

	if ( (objValue == NULL) || (xvoType(objValue) == XVO_DT_NULL) ) {
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
		if ( xvoType(objValue) == XVO_DT_FLOAT ) return CS_FloatIsInteger(xvoGetFloat(objValue));
		if ( xvoType(objValue) == XVO_DT_TEXT ) return CS_TextIsInteger(xvoGetText(objValue));
		return FALSE;
	}
	if ( strcmp(sType, "float") == 0 ) {
		if ( xvoType(objValue) == XVO_DT_INT ) return TRUE;
		if ( xvoType(objValue) == XVO_DT_FLOAT ) return TRUE;
		if ( xvoType(objValue) == XVO_DT_TEXT ) return CS_TextIsNumber(xvoGetText(objValue));
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
			return CS_TextEqualsIgnoreCase(sText, "true")
				|| CS_TextEqualsIgnoreCase(sText, "false")
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

bool CS_ValidateFieldDefaultValue(xvalue tblField, int iIndex, str* psError)
{
	xvalue objDefault;
	const char* sStorageType;

	if ( psError ) *psError = NULL;
	if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
		return TRUE;
	}
	if ( !CS_TableHasKey(tblField, "defaultValue") ) {
		return TRUE;
	}
	objDefault = xvoTableGetValue(tblField, "defaultValue", 12);
	if ( objDefault == NULL ) {
		return TRUE;
	}
	if ( (xvoType(objDefault) == XVO_DT_NULL) && !xvoTableGetBool(tblField, "nullable", 8) ) {
		if ( psError ) *psError = xrtFormat("field[%d].defaultValue cannot be null unless nullable is true", iIndex);
		return FALSE;
	}
	sStorageType = CS_GetNestedText(tblField, "storage", "type");
	if ( !CS_ValueMatchesStorageType(objDefault, sStorageType) ) {
		if ( psError ) *psError = xrtFormat("field[%d].defaultValue must match storage.type = %s", iIndex, CS_NormalizeStorageTypeName(sStorageType));
		return FALSE;
	}
	return TRUE;
}

bool CS_ValidateFieldComponentCompatibility(xvalue tblField, int iIndex, str* psError)
{
	const char* sStorageType;
	const char* sComponentType;
	const char* sType;

	if ( psError ) *psError = NULL;
	if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
		return TRUE;
	}

	sStorageType = CS_GetNestedText(tblField, "storage", "type");
	sComponentType = CS_GetNestedText(tblField, "component", "type");
	sType = CS_NormalizeStorageTypeName(sStorageType);

	if ( CS_IsBlank(sComponentType) ) {
		return TRUE;
	}
	if ( strcmp(sComponentType, "images") == 0 || strcmp(sComponentType, "files") == 0 ) {
		if ( strcmp(sType, "json") != 0 ) {
			if ( psError ) *psError = xrtFormat("field[%d].component.type %s is incompatible with storage.type %s; use storage.type json for multi-asset fields", iIndex, sComponentType, sType);
			return FALSE;
		}
		return TRUE;
	}
	if ( strcmp(sComponentType, "image") == 0 || strcmp(sComponentType, "file") == 0 ) {
		if ( strcmp(sType, "text") != 0 && strcmp(sType, "json") != 0 ) {
			if ( psError ) *psError = xrtFormat("field[%d].component.type %s is incompatible with storage.type %s; use storage.type text for single URLs or json if you need structured payloads", iIndex, sComponentType, sType);
			return FALSE;
		}
	}
	return TRUE;
}

bool CS_IsSingleAssetComponentType(const char* sComponentType)
{
	if ( CS_IsBlank(sComponentType) ) {
		return FALSE;
	}
	return (strcmp(sComponentType, "image") == 0)
		|| (strcmp(sComponentType, "file") == 0);
}

bool CS_IsMultiAssetComponentType(const char* sComponentType)
{
	if ( CS_IsBlank(sComponentType) ) {
		return FALSE;
	}
	return (strcmp(sComponentType, "images") == 0)
		|| (strcmp(sComponentType, "files") == 0);
}

bool CS_IsAssetComponentType(const char* sComponentType)
{
	return CS_IsSingleAssetComponentType(sComponentType) || CS_IsMultiAssetComponentType(sComponentType);
}

bool CS_ComponentStorageCompatible(const char* sComponentType, const char* sStorageType)
{
	const char* sType = CS_NormalizeStorageTypeName(sStorageType);

	if ( CS_IsMultiAssetComponentType(sComponentType) ) {
		return strcmp(sType, "json") == 0;
	}
	if ( CS_IsSingleAssetComponentType(sComponentType) ) {
		return (strcmp(sType, "text") == 0) || (strcmp(sType, "json") == 0);
	}
	return TRUE;
}

const char* CS_GetFieldComponentPropText(xvalue tblField, const char* sKey)
{
	xvalue tblComponent;
	xvalue tblProps;

	if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) || CS_IsBlank(sKey) ) {
		return NULL;
	}
	tblComponent = CS_GetTableValue(tblField, "component");
	tblProps = CS_GetTableValue(tblComponent, "props");
	return tblProps ? xvoTableGetText(tblProps, sKey, (int)strlen(sKey)) : NULL;
}

bool CS_ValidateIndexes(xvalue arrIndexes, xvalue arrFields, str* psError)
{
	if ( psError ) *psError = NULL;
	if ( arrIndexes == NULL ) {
		return TRUE;
	}
	if ( xvoType(arrIndexes) != XVO_DT_ARRAY ) {
		if ( psError ) *psError = xrtCopyStr("entity.indexes must be an array", 0);
		return FALSE;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrIndexes); i++ ) {
		xvalue tblIndex = xvoArrayGetValue(arrIndexes, i);
		xvalue arrIndexFields;
		xvalue objUnique;
		const char* sIndexName;

		if ( (tblIndex == NULL) || (xvoType(tblIndex) != XVO_DT_TABLE) ) {
			if ( psError ) *psError = xrtFormat("entity.indexes[%d] must be an object", i);
			return FALSE;
		}
		sIndexName = xvoTableGetText(tblIndex, "name", 4);
		arrIndexFields = xvoTableGetValue(tblIndex, "fields", 6);
		objUnique = xvoTableGetValue(tblIndex, "unique", 6);
		if ( !CS_IsValidName(sIndexName) ) {
			if ( psError ) *psError = xrtFormat("entity.indexes[%d].name is invalid", i);
			return FALSE;
		}
		if ( CS_ArrayHasDuplicateText(arrIndexes, "name", sIndexName, i) ) {
			if ( psError ) *psError = xrtFormat("entity.indexes[%d].name is duplicated", i);
			return FALSE;
		}
		if ( (arrIndexFields == NULL) || (xvoType(arrIndexFields) != XVO_DT_ARRAY) || (xvoArrayItemCount(arrIndexFields) <= 0) ) {
			if ( psError ) *psError = xrtFormat("entity.indexes[%d].fields must contain at least one field", i);
			return FALSE;
		}
		if ( !CS_ValueIsMissing(objUnique) && !CS_NormalizeTableBool(tblIndex, "unique") ) {
			if ( psError ) *psError = xrtFormat("entity.indexes[%d].unique must be a boolean", i);
			return FALSE;
		}
		for ( int j = 0; j < xvoArrayItemCount(arrIndexFields); j++ ) {
			xvalue objFieldName = xvoArrayGetValue(arrIndexFields, j);
			const char* sFieldName = NULL;

			if ( (objFieldName == NULL) || (xvoType(objFieldName) != XVO_DT_TEXT) ) {
				if ( psError ) *psError = xrtFormat("entity.indexes[%d].fields[%d] must be a field name", i, j);
				return FALSE;
			}
			sFieldName = xvoGetText(objFieldName);
			if ( CS_IsBlank(sFieldName) ) {
				if ( psError ) *psError = xrtFormat("entity.indexes[%d].fields[%d] must not be blank", i, j);
				return FALSE;
			}
			if ( CS_FindFieldByName(arrFields, sFieldName) == NULL ) {
				if ( psError ) *psError = xrtFormat("entity.indexes[%d].fields[%d] references an unknown field", i, j);
				return FALSE;
			}
			for ( int k = 0; k < j; k++ ) {
				xvalue objPrev = xvoArrayGetValue(arrIndexFields, k);
				if ( (objPrev != NULL) && (xvoType(objPrev) == XVO_DT_TEXT) && (strcmp(xvoGetText(objPrev), sFieldName) == 0) ) {
					if ( psError ) *psError = xrtFormat("entity.indexes[%d].fields[%d] is duplicated", i, j);
					return FALSE;
				}
			}
		}
	}
	return TRUE;
}

bool CS_ValidateOptionalBoolValue(xvalue tblData, const char* sKey, const char* sLabel, str* psError)
{
	xvalue objValue;

	if ( psError ) *psError = NULL;
	if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) || CS_IsBlank(sKey) || CS_IsBlank(sLabel) ) {
		return TRUE;
	}
	objValue = xvoTableGetValue(tblData, sKey, (int)strlen(sKey));
	if ( !CS_ValueIsMissing(objValue) && !CS_NormalizeTableBool(tblData, sKey) ) {
		if ( psError ) *psError = xrtFormat("%s must be a boolean", sLabel);
		return FALSE;
	}
	return TRUE;
}

bool CS_ValidateUiListColumns(xvalue arrColumns, xvalue arrFields, str* psError)
{
	if ( psError ) *psError = NULL;
	if ( arrColumns == NULL ) {
		return TRUE;
	}
	if ( xvoType(arrColumns) != XVO_DT_ARRAY ) {
		if ( psError ) *psError = xrtCopyStr("ui.list.columns must be an array", 0);
		return FALSE;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrColumns); i++ ) {
		xvalue objName = xvoArrayGetValue(arrColumns, i);
		const char* sFieldName = NULL;
		if ( (objName == NULL) || (xvoType(objName) != XVO_DT_TEXT) ) {
			if ( psError ) *psError = xrtFormat("ui.list.columns[%d] must be a field name", i);
			return FALSE;
		}
		sFieldName = xvoGetText(objName);
		if ( CS_IsBlank(sFieldName) ) {
			if ( psError ) *psError = xrtFormat("ui.list.columns[%d] must not be blank", i);
			return FALSE;
		}
		if ( CS_FindFieldByName(arrFields, sFieldName) == NULL ) {
			if ( psError ) *psError = xrtFormat("ui.list.columns[%d] references an unknown field", i);
			return FALSE;
		}
		for ( int j = 0; j < i; j++ ) {
			xvalue objPrev = xvoArrayGetValue(arrColumns, j);
			if ( (objPrev != NULL) && (xvoType(objPrev) == XVO_DT_TEXT) && (strcmp(xvoGetText(objPrev), sFieldName) == 0) ) {
				if ( psError ) *psError = xrtFormat("ui.list.columns[%d] is duplicated", i);
				return FALSE;
			}
		}
	}
	return TRUE;
}

bool CS_IsKnownUiListSortField(const char* sField)
{
	if ( CS_IsBlank(sField) ) {
		return FALSE;
	}
	return (strcmp(sField, "id") == 0)
		|| (strcmp(sField, "title") == 0)
		|| (strcmp(sField, "status") == 0)
		|| (strcmp(sField, "createTime") == 0)
		|| (strcmp(sField, "create_time") == 0)
		|| (strcmp(sField, "updateTime") == 0)
		|| (strcmp(sField, "update_time") == 0)
		|| (strcmp(sField, "slug") == 0)
		|| (strcmp(sField, "summary") == 0)
		|| (strcmp(sField, "publishedAt") == 0);
}

bool CS_ValidateUiListDefaultSort(xvalue objDefaultSort, xvalue arrFields, str* psError)
{
	const char* sField;
	const char* sDir;
	xvalue tblField = NULL;

	if ( psError ) *psError = NULL;
	if ( CS_ValueIsMissing(objDefaultSort) ) {
		return TRUE;
	}
	if ( xvoType(objDefaultSort) != XVO_DT_ARRAY ) {
		if ( psError ) *psError = xrtCopyStr("ui.list.defaultSort must be an array", 0);
		return FALSE;
	}
	if ( xvoArrayItemCount(objDefaultSort) != 2 ) {
		if ( psError ) *psError = xrtCopyStr("ui.list.defaultSort must contain [field, direction]", 0);
		return FALSE;
	}
	if ( (xvoType(xvoArrayGetValue(objDefaultSort, 0)) != XVO_DT_TEXT) || (xvoType(xvoArrayGetValue(objDefaultSort, 1)) != XVO_DT_TEXT) ) {
		if ( psError ) *psError = xrtCopyStr("ui.list.defaultSort items must be text", 0);
		return FALSE;
	}
	sField = xvoGetText(xvoArrayGetValue(objDefaultSort, 0));
	sDir = xvoGetText(xvoArrayGetValue(objDefaultSort, 1));
	if ( CS_IsBlank(sField) ) {
		if ( psError ) *psError = xrtCopyStr("ui.list.defaultSort field must not be blank", 0);
		return FALSE;
	}
	tblField = CS_FindFieldByName(arrFields, sField);
	if ( !CS_IsKnownUiListSortField(sField) && (tblField == NULL) ) {
		if ( psError ) *psError = xrtCopyStr("ui.list.defaultSort field references an unknown field", 0);
		return FALSE;
	}
	if ( (tblField != NULL) && !xvoTableGetBool(tblField, "sortable", 8) ) {
		if ( psError ) *psError = xrtCopyStr("ui.list.defaultSort field must be marked sortable", 0);
		return FALSE;
	}
	if ( CS_IsBlank(sDir) || ((strcmp(sDir, "asc") != 0) && (strcmp(sDir, "desc") != 0)) ) {
		if ( psError ) *psError = xrtCopyStr("ui.list.defaultSort direction must be asc or desc", 0);
		return FALSE;
	}
	return TRUE;
}

bool CS_ValidateUiListPageSize(xvalue objPageSize, str* psError)
{
	int64 iPageSize;

	if ( psError ) *psError = NULL;
	if ( CS_ValueIsMissing(objPageSize) ) {
		return TRUE;
	}
	if ( xvoType(objPageSize) == XVO_DT_INT ) {
		iPageSize = xvoGetInt(objPageSize);
	} else if ( xvoType(objPageSize) == XVO_DT_TEXT ) {
		const char* sPageSize = xvoGetText(objPageSize);
		if ( !CS_TextIsInteger(sPageSize) ) {
			if ( psError ) *psError = xrtCopyStr("ui.list.pageSize must be an integer", 0);
			return FALSE;
		}
		iPageSize = atoll(sPageSize);
	} else {
		if ( psError ) *psError = xrtCopyStr("ui.list.pageSize must be an integer", 0);
		return FALSE;
	}
	if ( (iPageSize < 1) || (iPageSize > 200) ) {
		if ( psError ) *psError = xrtCopyStr("ui.list.pageSize must be between 1 and 200", 0);
		return FALSE;
	}
	return TRUE;
}

bool CS_ValidateTextArray(xvalue arrValue, const char* sLabel, str* psError)
{
	if ( psError ) *psError = NULL;
	if ( arrValue == NULL ) {
		return TRUE;
	}
	if ( xvoType(arrValue) != XVO_DT_ARRAY ) {
		if ( psError ) *psError = xrtFormat("%s must be an array", sLabel);
		return FALSE;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrValue); i++ ) {
		xvalue objItem = xvoArrayGetValue(arrValue, i);
		const char* sText = NULL;
		if ( (objItem == NULL) || (xvoType(objItem) != XVO_DT_TEXT) ) {
			if ( psError ) *psError = xrtFormat("%s[%d] must be text", sLabel, i);
			return FALSE;
		}
		sText = xvoGetText(objItem);
		if ( CS_IsBlank(sText) ) {
			if ( psError ) *psError = xrtFormat("%s[%d] must not be blank", sLabel, i);
			return FALSE;
		}
		for ( int j = 0; j < i; j++ ) {
			xvalue objPrev = xvoArrayGetValue(arrValue, j);
			if ( (objPrev != NULL) && (xvoType(objPrev) == XVO_DT_TEXT) && (strcmp(xvoGetText(objPrev), sText) == 0) ) {
				if ( psError ) *psError = xrtFormat("%s[%d] is duplicated", sLabel, i);
				return FALSE;
			}
		}
	}
	return TRUE;
}

bool CS_NormalizeSpec(
	xvalue tblSpec,
	str* psSpecJson,
	str* psSpecHash,
	str* psTypeKey,
	str* psXid,
	str* psName,
	str* psTitle,
	str* psNamespace,
	str* psDescription,
	str* psIcon,
	str* psTable,
	int* piFieldCount,
	str* psError)
{
	xvalue tblIdentity;
	xvalue tblEntity;
	xvalue arrFields;
	xvalue tblPresentation;
	xvalue arrGroups;
	xvalue arrCapabilitySlots;
	xvalue tblCoreFeatures;
	xvalue tblDraft;
	xvalue tblPolicies;
	xvalue tblUi;
	xvalue tblUiList;
	xvalue tblUiDetail;
	xvalue tblUiForm;
	xvalue tblMetadata;
	xvalue arrIndexes;
	xvalue arrMetadataLabels;
	xvalue arrMetadataTags;
	xvalue objMetadataNotes;
	xvalue arrUiListColumns;
	xvalue objUiListDefaultSort;
	xvalue objUiListPageSize;
	xvalue objUiDetailShowAuthor;
	xvalue objUiDetailShowPublishedAt;
	xvalue objUiFormLayout;
	const char* sXid;
	const char* sName;
	const char* sTitle;
	const char* sNamespace;
	const char* sDescription;
	const char* sIcon;
	const char* sTable;
	const char* sSlugField;
	const char* sSummaryField;
	const char* sCoverField;
	const char* sPublishedAtField;
	const char* sTitleField;
	const char* sStatusField;
	const char* sDraftMode;
	const char* sStatusFlow;
	const char* sAuthorMode;
	const char* sDeleteMode;
	const char* sPublishMode;
	str sSpecJson = NULL;
	str sHash = NULL;
	str sTypeKey = NULL;

	if ( psError ) *psError = NULL;
	if ( psSpecJson ) *psSpecJson = NULL;
	if ( psSpecHash ) *psSpecHash = NULL;
	if ( psTypeKey ) *psTypeKey = NULL;
	if ( psXid ) *psXid = NULL;
	if ( psName ) *psName = NULL;
	if ( psTitle ) *psTitle = NULL;
	if ( psNamespace ) *psNamespace = NULL;
	if ( psDescription ) *psDescription = NULL;
	if ( psIcon ) *psIcon = NULL;
	if ( psTable ) *psTable = NULL;
	if ( piFieldCount ) *piFieldCount = 0;

	if ( (tblSpec == NULL) || (xvoType(tblSpec) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("spec must be an object", 0);
		return FALSE;
	}
	tblIdentity = CS_GetTableValue(tblSpec, "identity");
	tblEntity = CS_GetTableValue(tblSpec, "entity");
	tblPresentation = CS_GetTableValue(tblSpec, "presentation");
	arrFields = tblEntity ? xvoTableGetValue(tblEntity, "fields", 6) : NULL;
	arrIndexes = tblEntity ? xvoTableGetValue(tblEntity, "indexes", 7) : NULL;
	arrGroups = tblPresentation ? xvoTableGetValue(tblPresentation, "groups", 6) : NULL;
	arrCapabilitySlots = xvoTableGetValue(tblSpec, "capabilitySlots", 15);
	tblCoreFeatures = CS_GetTableValue(tblSpec, "coreFeatures");
	tblDraft = CS_GetTableValue(tblCoreFeatures, "draft");
	tblPolicies = CS_GetTableValue(tblSpec, "policies");
	tblUi = CS_GetTableValue(tblSpec, "ui");
	tblUiList = CS_GetTableValue(tblUi, "list");
	tblUiDetail = CS_GetTableValue(tblUi, "detail");
	tblUiForm = CS_GetTableValue(tblUi, "form");
	tblMetadata = CS_GetTableValue(tblSpec, "metadata");
	arrMetadataLabels = tblMetadata ? xvoTableGetValue(tblMetadata, "labels", 6) : NULL;
	arrMetadataTags = tblMetadata ? xvoTableGetValue(tblMetadata, "tags", 4) : NULL;
	objMetadataNotes = tblMetadata ? xvoTableGetValue(tblMetadata, "notes", 5) : NULL;
	arrUiListColumns = tblUiList ? xvoTableGetValue(tblUiList, "columns", 7) : NULL;
	if ( tblUiList && (xvoType(tblUiList) == XVO_DT_TABLE) ) {
		xvalue objSortCandidate = xvoTableGetValue(tblUiList, "defaultSort", 11);
		if ( objSortCandidate && (xvoType(objSortCandidate) == XVO_DT_TABLE) ) {
			const char* sSortField = xvoTableGetText(objSortCandidate, "field", 5);
			const char* sSortDir = xvoTableGetText(objSortCandidate, "dir", 3);
			if ( !CS_IsBlank(sSortField) ) {
				xvalue arrSort = xvoCreateArray();
				xvoArrayAppendText(arrSort, (str)sSortField, 0, FALSE);
				xvoArrayAppendText(arrSort, (str)(CS_IsBlank(sSortDir) ? "desc" : sSortDir), 0, FALSE);
				xvoTableSetValue(tblUiList, "defaultSort", 11, arrSort, TRUE);
			}
		}
	}
	objUiListDefaultSort = tblUiList ? xvoTableGetValue(tblUiList, "defaultSort", 11) : NULL;
	objUiListPageSize = tblUiList ? xvoTableGetValue(tblUiList, "pageSize", 8) : NULL;
	objUiDetailShowAuthor = tblUiDetail ? xvoTableGetValue(tblUiDetail, "showAuthor", 10) : NULL;
	objUiDetailShowPublishedAt = tblUiDetail ? xvoTableGetValue(tblUiDetail, "showPublishedAt", 15) : NULL;
	objUiFormLayout = tblUiForm ? xvoTableGetValue(tblUiForm, "layout", 6) : NULL;
	if ( (tblIdentity == NULL) || (xvoType(tblIdentity) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("identity block is required", 0);
		return FALSE;
	}
	if ( (tblEntity == NULL) || (xvoType(tblEntity) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("entity block is required", 0);
		return FALSE;
	}
	if ( (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) || (xvoArrayItemCount(arrFields) <= 0) ) {
		if ( psError ) *psError = xrtCopyStr("entity.fields must contain at least one field", 0);
		return FALSE;
	}

	sXid = xvoTableGetText(tblIdentity, "xid", 3);
	sName = xvoTableGetText(tblIdentity, "name", 4);
	sTitle = xvoTableGetText(tblIdentity, "title", 5);
	sNamespace = xvoTableGetText(tblIdentity, "namespace", 9);
	sDescription = xvoTableGetText(tblIdentity, "description", 11);
	sIcon = xvoTableGetText(tblIdentity, "icon", 4);
	sTable = xvoTableGetText(tblEntity, "table", 5);
	sSlugField = xvoTableGetText(tblEntity, "slugField", 9);
	sSummaryField = xvoTableGetText(tblEntity, "summaryField", 12);
	sCoverField = xvoTableGetText(tblEntity, "coverField", 10);
	sPublishedAtField = xvoTableGetText(tblEntity, "publishedAtField", 16);
	sTitleField = xvoTableGetText(tblEntity, "titleField", 10);
	sStatusField = xvoTableGetText(tblEntity, "statusField", 11);
	sDraftMode = tblDraft ? xvoTableGetText(tblDraft, "mode", 4) : NULL;
	sStatusFlow = tblPolicies ? xvoTableGetText(tblPolicies, "statusFlow", 10) : NULL;
	sAuthorMode = tblPolicies ? xvoTableGetText(tblPolicies, "authorMode", 10) : NULL;
	sDeleteMode = tblPolicies ? xvoTableGetText(tblPolicies, "deleteMode", 10) : NULL;
	sPublishMode = tblPolicies ? xvoTableGetText(tblPolicies, "publishMode", 11) : NULL;
	if ( !CS_IsValidXid(sXid) ) {
		if ( psError ) *psError = xrtCopyStr("identity.xid is invalid", 0);
		return FALSE;
	}
	if ( !CS_IsValidName(sName) ) {
		if ( psError ) *psError = xrtCopyStr("identity.name is invalid", 0);
		return FALSE;
	}
	if ( CS_IsBlank(sTitle) ) {
		if ( psError ) *psError = xrtCopyStr("identity.title is required", 0);
		return FALSE;
	}
	if ( CS_IsBlank(sTable) || !CS_IsValidName(sTable) ) {
		if ( psError ) *psError = xrtCopyStr("entity.table is invalid", 0);
		return FALSE;
	}
	if ( !CS_ValidatePresentationGroups(arrGroups, psError) ) {
		return FALSE;
	}
	if ( !CS_ValidateCapabilitySlots(arrCapabilitySlots, psError) ) {
		return FALSE;
	}
	if ( (tblCoreFeatures != NULL) && (xvoType(tblCoreFeatures) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("coreFeatures must be an object", 0);
		return FALSE;
	}
	if ( (tblDraft != NULL) && (xvoType(tblDraft) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("coreFeatures.draft must be an object", 0);
		return FALSE;
	}
	if ( !CS_ValidateOptionalBoolValue(tblCoreFeatures, "adminCrud", "coreFeatures.adminCrud", psError) ) {
		return FALSE;
	}
	if ( !CS_ValidateOptionalBoolValue(tblCoreFeatures, "publicApi", "coreFeatures.publicApi", psError) ) {
		return FALSE;
	}
	if ( !CS_ValidateOptionalBoolValue(tblDraft, "enabled", "coreFeatures.draft.enabled", psError) ) {
		return FALSE;
	}
	if ( (!CS_IsBlank(sDraftMode)) && (strcmp(sDraftMode, "same-table") != 0) ) {
		if ( psError ) *psError = xrtCopyStr("coreFeatures.draft.mode must be same-table", 0);
		return FALSE;
	}
	if ( (tblPolicies != NULL) && (xvoType(tblPolicies) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("policies must be an object", 0);
		return FALSE;
	}
	if ( !CS_IsBlank(sStatusFlow) && !CS_IsKnownStatusFlow(sStatusFlow) ) {
		if ( psError ) *psError = xrtCopyStr("policies.statusFlow is invalid", 0);
		return FALSE;
	}
	if ( !CS_IsBlank(sAuthorMode) && !CS_IsKnownAuthorMode(sAuthorMode) ) {
		if ( psError ) *psError = xrtCopyStr("policies.authorMode is invalid", 0);
		return FALSE;
	}
	if ( !CS_IsBlank(sDeleteMode) && strcmp(sDeleteMode, "soft-delete") != 0 ) {
		if ( psError ) *psError = xrtCopyStr("policies.deleteMode must be soft-delete", 0);
		return FALSE;
	}
	if ( !CS_IsBlank(sPublishMode) && strcmp(sPublishMode, "manual") != 0 ) {
		if ( psError ) *psError = xrtCopyStr("policies.publishMode must be manual", 0);
		return FALSE;
	}
	if ( (tblUi != NULL) && (xvoType(tblUi) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("ui must be an object", 0);
		return FALSE;
	}
	if ( (tblUiList != NULL) && (xvoType(tblUiList) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("ui.list must be an object", 0);
		return FALSE;
	}
	if ( (tblUiDetail != NULL) && (xvoType(tblUiDetail) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("ui.detail must be an object", 0);
		return FALSE;
	}
	if ( (tblUiForm != NULL) && (xvoType(tblUiForm) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("ui.form must be an object", 0);
		return FALSE;
	}
	if ( !CS_ValueIsMissing(objUiDetailShowAuthor) && !CS_NormalizeTableBool(tblUiDetail, "showAuthor") ) {
		if ( psError ) *psError = xrtCopyStr("ui.detail.showAuthor must be a boolean", 0);
		return FALSE;
	}
	if ( !CS_ValueIsMissing(objUiDetailShowPublishedAt) && !CS_NormalizeTableBool(tblUiDetail, "showPublishedAt") ) {
		if ( psError ) *psError = xrtCopyStr("ui.detail.showPublishedAt must be a boolean", 0);
		return FALSE;
	}
	if ( (objUiFormLayout != NULL) && (xvoType(objUiFormLayout) != XVO_DT_TEXT) ) {
		if ( psError ) *psError = xrtCopyStr("ui.form.layout must be text", 0);
		return FALSE;
	}
	if ( (objUiFormLayout != NULL) && xvoGetText(objUiFormLayout)
		&& (strcmp(xvoGetText(objUiFormLayout), "single-column") != 0)
		&& (strcmp(xvoGetText(objUiFormLayout), "two-column") != 0) ) {
		if ( psError ) *psError = xrtCopyStr("ui.form.layout must be single-column or two-column", 0);
		return FALSE;
	}
	if ( (tblMetadata != NULL) && (xvoType(tblMetadata) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("metadata must be an object", 0);
		return FALSE;
	}
	if ( !CS_ValidateTextArray(arrMetadataLabels, "metadata.labels", psError) ) {
		return FALSE;
	}
	if ( !CS_ValidateTextArray(arrMetadataTags, "metadata.tags", psError) ) {
		return FALSE;
	}
	if ( !CS_ValueIsMissing(objMetadataNotes) && (xvoType(objMetadataNotes) != XVO_DT_TEXT) ) {
		if ( psError ) *psError = xrtCopyStr("metadata.notes must be text", 0);
		return FALSE;
	}
	if ( !CS_ValidateUiListDefaultSort(objUiListDefaultSort, arrFields, psError) ) {
		return FALSE;
	}
	if ( !CS_ValidateUiListPageSize(objUiListPageSize, psError) ) {
		return FALSE;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
		xvalue tblField = xvoArrayGetValue(arrFields, i);
		const char* sFieldName;
		const char* sFieldGroup;
		const char* sFieldRole;
		str sFieldError = NULL;
		if ( !CS_ValidateField(tblField, i, &sFieldError) ) {
			if ( psError ) {
				*psError = sFieldError ? sFieldError : xrtCopyStr("字段校验失败", 0);
			} else if ( sFieldError ) {
				xrtFree(sFieldError);
			}
			return FALSE;
		}
		sFieldName = xvoTableGetText(tblField, "name", 4);
		sFieldGroup = xvoTableGetText(tblField, "group", 5);
		sFieldRole = CS_GetFieldSemanticRole(tblField);
		if ( CS_ArrayHasDuplicateText(arrFields, "name", sFieldName, i) ) {
			if ( psError ) *psError = xrtFormat("entity.fields[%d].name is duplicated", i);
			return FALSE;
		}
		if ( CS_IsSingletonSemanticRole(sFieldRole) && CS_ArrayHasDuplicateSemanticRole(arrFields, sFieldRole, i) ) {
			if ( psError ) *psError = xrtFormat("entity.fields[%d].semantic.role is duplicated", i);
			return FALSE;
		}
		if ( (!CS_IsBlank(sFieldGroup)) && (arrGroups != NULL) && (CS_FindGroupByKey(arrGroups, sFieldGroup) == NULL) ) {
			if ( psError ) *psError = xrtFormat("entity.fields[%d].group references an unknown presentation group", i);
			return FALSE;
		}
	}
	if ( !CS_ValidateEntityFieldSemanticRole(arrFields, sTitleField, "title", "entity.titleField", psError)
		|| !CS_ValidateEntityFieldSemanticRole(arrFields, sStatusField, "status", "entity.statusField", psError)
		|| !CS_ValidateEntityFieldSemanticRole(arrFields, sSlugField, "slug", "entity.slugField", psError)
		|| !CS_ValidateEntityFieldSemanticRole(arrFields, sSummaryField, "summary", "entity.summaryField", psError)
		|| !CS_ValidateEntityFieldSemanticRole(arrFields, sCoverField, "cover", "entity.coverField", psError)
		|| !CS_ValidateEntityFieldSemanticRole(arrFields, sPublishedAtField, "publishedAt", "entity.publishedAtField", psError) ) {
		return FALSE;
	}
	if ( !CS_ValidateUiListColumns(arrUiListColumns, arrFields, psError) ) {
		return FALSE;
	}
	if ( !CS_ValidateIndexes(arrIndexes, arrFields, psError) ) {
		return FALSE;
	}

	sSpecJson = CS_StringifyJson(tblSpec);
	if ( sSpecJson == NULL ) {
		if ( psError ) *psError = xrtCopyStr("规格数据序列化失败", 0);
		return FALSE;
	}
	sHash = CS_BuildHashText(sSpecJson);
	sTypeKey = (sNamespace && sNamespace[0]) ? xrtFormat("%s.%s", sNamespace, sName) : xrtCopyStr((str)sName, 0);
	if ( (sHash == NULL) || (sTypeKey == NULL) ) {
		if ( sSpecJson ) xrtFree(sSpecJson);
		if ( sHash ) xrtFree(sHash);
		if ( sTypeKey ) xrtFree(sTypeKey);
		if ( psError ) *psError = xrtCopyStr("规格元数据初始化失败", 0);
		return FALSE;
	}

	if ( psSpecJson ) *psSpecJson = sSpecJson; else xrtFree(sSpecJson);
	if ( psSpecHash ) *psSpecHash = sHash; else xrtFree(sHash);
	if ( psTypeKey ) *psTypeKey = sTypeKey; else xrtFree(sTypeKey);
	if ( psXid ) *psXid = xrtCopyStr((str)sXid, 0);
	if ( psName ) *psName = xrtCopyStr((str)sName, 0);
	if ( psTitle ) *psTitle = xrtCopyStr((str)sTitle, 0);
	if ( psNamespace ) *psNamespace = xrtCopyStr((str)(sNamespace ? sNamespace : ""), 0);
	if ( psDescription ) *psDescription = xrtCopyStr((str)(sDescription ? sDescription : ""), 0);
	if ( psIcon ) *psIcon = xrtCopyStr((str)(sIcon ? sIcon : ""), 0);
	if ( psTable ) *psTable = xrtCopyStr((str)sTable, 0);
	if ( piFieldCount ) *piFieldCount = xvoArrayItemCount(arrFields);
	return TRUE;
}

void CS_FreeSnapshot(CSTypeSnapshot* pSnapshot)
{
	if ( pSnapshot == NULL ) {
		return;
	}
	if ( pSnapshot->sSpecJson ) {
		xrtFree(pSnapshot->sSpecJson);
		pSnapshot->sSpecJson = NULL;
	}
	if ( pSnapshot->sTypeTitle ) {
		xrtFree(pSnapshot->sTypeTitle);
		pSnapshot->sTypeTitle = NULL;
	}
	memset(pSnapshot->sPluginXid, 0, sizeof(pSnapshot->sPluginXid));
	pSnapshot->iTypeId = 0;
	pSnapshot->iCurrentRevision = 0;
	pSnapshot->iAppliedRevision = 0;
}

bool CS_LoadTypeSnapshot(sqlite3* pDb, int64 iTypeId, CSTypeSnapshot* pSnapshot)
{
	sqlite3_stmt* stmt = NULL;
	const char* sSql =
		"SELECT t.id, t.current_revision, t.generated_plugin_xid, t.spec_json, t.title, "
		"COALESCE(g.applied_revision, 0) "
		"FROM content_type t "
		"LEFT JOIN content_generated_plugin g ON g.type_id = t.id "
		"WHERE t.id = ?";

	if ( (pDb == NULL) || (pSnapshot == NULL) ) {
		return FALSE;
	}
	memset(pSnapshot, 0, sizeof(CSTypeSnapshot));
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}
	sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTypeId);
	if ( sqlite3_step(stmt) != SQLITE_ROW ) {
		sqlite3_finalize(stmt);
		return FALSE;
	}
	pSnapshot->iTypeId = sqlite3_column_int64(stmt, 0);
	pSnapshot->iCurrentRevision = sqlite3_column_int(stmt, 1);
	CS_CopyText(pSnapshot->sPluginXid, sizeof(pSnapshot->sPluginXid), (const char*)sqlite3_column_text(stmt, 2), "");
	pSnapshot->sSpecJson = xrtCopyStr((str)sqlite3_column_text(stmt, 3), 0);
	pSnapshot->sTypeTitle = xrtCopyStr((str)sqlite3_column_text(stmt, 4), 0);
	pSnapshot->iAppliedRevision = sqlite3_column_int(stmt, 5);
	sqlite3_finalize(stmt);
	return TRUE;
}

xvalue CS_ParseJsonText(const char* sJson)
{
	if ( sJson == NULL ) {
		return NULL;
	}
	return xrtParseJSON((str)sJson, strlen(sJson));
}

xvalue CS_FindFieldByName(xvalue arrFields, const char* sName)
{
	if ( (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) || (sName == NULL) ) {
		return NULL;
	}
	for ( int i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
		xvalue tblField = xvoArrayGetValue(arrFields, i);
		const char* sFieldName = xvoTableGetText(tblField, "name", 4);
		if ( sFieldName && (strcmp(sFieldName, sName) == 0) ) {
			return tblField;
		}
	}
	return NULL;
}

void CS_AdvisorAddScopedItem(xvalue arrItems, const char* sSeverity, const char* sKind, const char* sTitle, const char* sMessage, const char* sScope, const char* sFieldName)
{
	xvalue tblItem = xvoCreateTable();

	xvoTableSetText(tblItem, "severity", 8, (str)(sSeverity ? sSeverity : "info"), 0, FALSE);
	xvoTableSetText(tblItem, "kind", 4, (str)(sKind ? sKind : "note"), 0, FALSE);
	xvoTableSetText(tblItem, "title", 5, (str)(sTitle ? sTitle : "Advisor item"), 0, FALSE);
	xvoTableSetText(tblItem, "message", 7, (str)(sMessage ? sMessage : ""), 0, FALSE);
	xvoTableSetText(tblItem, "scope", 5, (str)(sScope ? sScope : "global"), 0, FALSE);
	if ( !CS_IsBlank(sFieldName) ) {
		xvoTableSetText(tblItem, "fieldName", 9, (str)sFieldName, 0, FALSE);
	}
	xvoArrayAppendValue(arrItems, tblItem, TRUE);
}

void CS_AdvisorAddItem(xvalue arrItems, const char* sSeverity, const char* sKind, const char* sTitle, const char* sMessage)
{
	CS_AdvisorAddScopedItem(arrItems, sSeverity, sKind, sTitle, sMessage, "global", NULL);
}

xvalue CS_BuildAdvisorData(xvalue tblFromSpec, int iFromRevision, xvalue tblToSpec, int iToRevision)
{
	xvalue tblRet = xvoCreateTable();
	xvalue arrItems = xvoCreateArray();
	xvalue tblFromEntity = CS_GetTableValue(tblFromSpec, "entity");
	xvalue tblToEntity = CS_GetTableValue(tblToSpec, "entity");
	xvalue arrFromFields = tblFromEntity ? xvoTableGetValue(tblFromEntity, "fields", 6) : NULL;
	xvalue arrToFields = tblToEntity ? xvoTableGetValue(tblToEntity, "fields", 6) : NULL;
	xvalue arrFromIndexes = tblFromEntity ? xvoTableGetValue(tblFromEntity, "indexes", 7) : NULL;
	xvalue arrToIndexes = tblToEntity ? xvoTableGetValue(tblToEntity, "indexes", 7) : NULL;
	int iInfo = 0;
	int iWarn = 0;
	int iError = 0;

	if ( (tblFromSpec == NULL) || (iFromRevision <= 0) ) {
		CS_AdvisorAddItem(arrItems, "info", "initial-generation", "Initial generation", "No applied managed revision exists yet. Generation will create the first managed plugin skeleton.");
		iInfo++;
	} else {
		const char* sFromTitle = xvoTableGetText(CS_GetTableValue(tblFromSpec, "identity"), "title", 5);
		const char* sToTitle = xvoTableGetText(CS_GetTableValue(tblToSpec, "identity"), "title", 5);
		xvalue tblFromPolicies = CS_GetTableValue(tblFromSpec, "policies");
		xvalue tblToPolicies = CS_GetTableValue(tblToSpec, "policies");
		const char* sFromStatusFlow = tblFromPolicies ? xvoTableGetText(tblFromPolicies, "statusFlow", 10) : NULL;
		const char* sToStatusFlow = tblToPolicies ? xvoTableGetText(tblToPolicies, "statusFlow", 10) : NULL;
		const char* sFromAuthorMode = tblFromPolicies ? xvoTableGetText(tblFromPolicies, "authorMode", 10) : NULL;
		const char* sToAuthorMode = tblToPolicies ? xvoTableGetText(tblToPolicies, "authorMode", 10) : NULL;
		const char* sFromPublishMode = tblFromPolicies ? xvoTableGetText(tblFromPolicies, "publishMode", 11) : NULL;
		const char* sToPublishMode = tblToPolicies ? xvoTableGetText(tblToPolicies, "publishMode", 11) : NULL;
		xvalue arrFromGroups = CS_GetTableValue(CS_GetTableValue(tblFromSpec, "presentation"), "groups");
		xvalue arrToGroups = CS_GetTableValue(CS_GetTableValue(tblToSpec, "presentation"), "groups");
		xvalue arrFromSlots = CS_GetTableValue(tblFromSpec, "capabilitySlots");
		xvalue arrToSlots = CS_GetTableValue(tblToSpec, "capabilitySlots");
		str sFromGroupsJson = NULL;
		str sToGroupsJson = NULL;
		str sFromSlotsJson = NULL;
		str sToSlotsJson = NULL;
		if ( sFromTitle && sToTitle && (strcmp(sFromTitle, sToTitle) != 0) ) {
			CS_AdvisorAddItem(arrItems, "info", "meta-change", "Title changed", "Plugin metadata will be regenerated. This is usually safe.");
			iInfo++;
		}
		if ( strcmp(sFromStatusFlow ? sFromStatusFlow : "", sToStatusFlow ? sToStatusFlow : "") != 0 ) {
			CS_AdvisorAddItem(arrItems, "warn", "status-flow-change", "Status flow changed", "Publishing workflow semantics changed. Review admin operations and public visibility rules before regeneration.");
			iWarn++;
		}
		if ( strcmp(sFromAuthorMode ? sFromAuthorMode : "", sToAuthorMode ? sToAuthorMode : "") != 0 ) {
			CS_AdvisorAddItem(arrItems, "warn", "author-mode-change", "Author mode changed", "Author ownership policy changed. Regeneration is safe, but surrounding submission or audit flows should be reviewed.");
			iWarn++;
		}
		if ( strcmp(sFromPublishMode ? sFromPublishMode : "", sToPublishMode ? sToPublishMode : "") != 0 ) {
			CS_AdvisorAddItem(arrItems, "warn", "publish-mode-change", "Publish mode changed", "Publish policy changed. Review editorial workflow because managed runtime behavior may need matching process changes.");
			iWarn++;
		}
		sFromGroupsJson = CS_StringifyValueOrEmptyArray(arrFromGroups);
		sToGroupsJson = CS_StringifyValueOrEmptyArray(arrToGroups);
		if ( ((sFromGroupsJson && sToGroupsJson) && (strcmp((const char*)sFromGroupsJson, (const char*)sToGroupsJson) != 0))
			|| ((sFromGroupsJson == NULL) != (sToGroupsJson == NULL)) ) {
			CS_AdvisorAddItem(arrItems, "info", "presentation-group-change", "Presentation groups changed", "Field grouping changed. Regeneration is safe, but managed form layout should be reviewed.");
			iInfo++;
		}
		if ( sFromGroupsJson ) xrtFree(sFromGroupsJson);
		if ( sToGroupsJson ) xrtFree(sToGroupsJson);
		sFromSlotsJson = CS_StringifyValueOrEmptyArray(arrFromSlots);
		sToSlotsJson = CS_StringifyValueOrEmptyArray(arrToSlots);
		if ( ((sFromSlotsJson && sToSlotsJson) && (strcmp((const char*)sFromSlotsJson, (const char*)sToSlotsJson) != 0))
			|| ((sFromSlotsJson == NULL) != (sToSlotsJson == NULL)) ) {
			CS_AdvisorAddItem(arrItems, "info", "capability-slot-change", "Capability slots changed", "Capability slot metadata changed. Runtime integration is still phase-gated, but regeneration will carry the new contract.");
			iInfo++;
		}
		if ( sFromSlotsJson ) xrtFree(sFromSlotsJson);
		if ( sToSlotsJson ) xrtFree(sToSlotsJson);
	}

	if ( (arrFromFields != NULL) && (xvoType(arrFromFields) == XVO_DT_ARRAY) && (arrToFields != NULL) && (xvoType(arrToFields) == XVO_DT_ARRAY) ) {
		for ( int i = 0; i < xvoArrayItemCount(arrToFields); i++ ) {
			xvalue tblToField = xvoArrayGetValue(arrToFields, i);
			const char* sFieldName = xvoTableGetText(tblToField, "name", 4);
			xvalue tblFromField = CS_FindFieldByName(arrFromFields, sFieldName);
			if ( tblFromField == NULL ) {
				if ( xvoTableGetBool(tblToField, "required", 8) ) {
					CS_AdvisorAddScopedItem(arrItems, "warn", "field-add", "New required field", "A new required field was added. Database and validation migration should provide a default strategy.", "field", sFieldName);
					iWarn++;
				} else {
					CS_AdvisorAddScopedItem(arrItems, "info", "field-add", "New optional field", "A new optional field was added. This is usually a non-destructive schema extension.", "field", sFieldName);
					iInfo++;
				}
				continue;
			}
			{
				const char* sFromStorage = CS_GetNestedText(tblFromField, "storage", "type");
				const char* sToStorage = CS_GetNestedText(tblToField, "storage", "type");
				const char* sFromComponent = CS_GetNestedText(tblFromField, "component", "type");
				const char* sToComponent = CS_GetNestedText(tblToField, "component", "type");
				const char* sFromUploadUrl = CS_GetFieldComponentPropText(tblFromField, "uploadUrl");
				const char* sToUploadUrl = CS_GetFieldComponentPropText(tblToField, "uploadUrl");
				const char* sFromAccept = CS_GetFieldComponentPropText(tblFromField, "accept");
				const char* sToAccept = CS_GetFieldComponentPropText(tblToField, "accept");
				const char* sFromAcceptMime = CS_GetFieldComponentPropText(tblFromField, "acceptMime");
				const char* sToAcceptMime = CS_GetFieldComponentPropText(tblToField, "acceptMime");
				const char* sFromButtonText = CS_GetFieldComponentPropText(tblFromField, "buttonText");
				const char* sToButtonText = CS_GetFieldComponentPropText(tblToField, "buttonText");
				bool bStorageChanged = strcmp(sFromStorage ? sFromStorage : "", sToStorage ? sToStorage : "") != 0;
				bool bComponentChanged = strcmp(sFromComponent ? sFromComponent : "", sToComponent ? sToComponent : "") != 0;
				bool bAssetConfigChanged = strcmp(sFromUploadUrl ? sFromUploadUrl : "", sToUploadUrl ? sToUploadUrl : "") != 0
					|| strcmp(sFromAccept ? sFromAccept : "", sToAccept ? sToAccept : "") != 0
					|| strcmp(sFromAcceptMime ? sFromAcceptMime : "", sToAcceptMime ? sToAcceptMime : "") != 0
					|| strcmp(sFromButtonText ? sFromButtonText : "", sToButtonText ? sToButtonText : "") != 0;

				if ( bStorageChanged ) {
					str sMessage = xrtFormat("Field %s changed storage.type from %s to %s. This is a breaking change and should not be auto-migrated blindly.",
						sFieldName ? sFieldName : "(unnamed)",
						sFromStorage ? sFromStorage : "(empty)",
						sToStorage ? sToStorage : "(empty)");
					CS_AdvisorAddScopedItem(arrItems, "error", "field-storage-change", "Field storage changed", (const char*)sMessage, "field", sFieldName);
					if ( sMessage ) xrtFree(sMessage);
					iError++;
				}
				if ( bComponentChanged ) {
					str sMessage = xrtFormat("Field %s changed component.type from %s to %s. Regeneration is usually safe, but admin/public UI should be reviewed.",
						sFieldName ? sFieldName : "(unnamed)",
						sFromComponent ? sFromComponent : "(empty)",
						sToComponent ? sToComponent : "(empty)");
					CS_AdvisorAddScopedItem(arrItems, "info", "field-ui-change", "Field component changed", (const char*)sMessage, "field", sFieldName);
					if ( sMessage ) xrtFree(sMessage);
					iInfo++;
				}
				if ( CS_IsAssetComponentType(sFromComponent) || CS_IsAssetComponentType(sToComponent) ) {
					if ( bComponentChanged ) {
						if ( !CS_IsAssetComponentType(sFromComponent) && CS_IsAssetComponentType(sToComponent) ) {
							str sMessage = xrtFormat("Field %s moved from %s to asset component %s. Review payload migration, upload defaults, and existing public/admin rendering.",
								sFieldName ? sFieldName : "(unnamed)",
								sFromComponent ? sFromComponent : "(empty)",
								sToComponent ? sToComponent : "(empty)");
							CS_AdvisorAddScopedItem(arrItems, "warn", "asset-field-upgrade", "Field moved to asset component", (const char*)sMessage, "field", sFieldName);
							if ( sMessage ) xrtFree(sMessage);
							iWarn++;
						} else if ( CS_IsAssetComponentType(sFromComponent) && !CS_IsAssetComponentType(sToComponent) ) {
							str sMessage = xrtFormat("Field %s moved away from asset component %s to %s. Existing attachment URLs or JSON payloads may need normalization before regeneration.",
								sFieldName ? sFieldName : "(unnamed)",
								sFromComponent ? sFromComponent : "(empty)",
								sToComponent ? sToComponent : "(empty)");
							CS_AdvisorAddScopedItem(arrItems, "warn", "asset-field-downgrade", "Field left asset component", (const char*)sMessage, "field", sFieldName);
							if ( sMessage ) xrtFree(sMessage);
							iWarn++;
						} else if ( CS_IsMultiAssetComponentType(sFromComponent) != CS_IsMultiAssetComponentType(sToComponent) ) {
							str sMessage = xrtFormat("Field %s changed between single and multi-asset mode (%s -> %s). Review whether stored payloads must move between plain URLs and JSON arrays.",
								sFieldName ? sFieldName : "(unnamed)",
								sFromComponent ? sFromComponent : "(empty)",
								sToComponent ? sToComponent : "(empty)");
							CS_AdvisorAddScopedItem(arrItems, "warn", "asset-cardinality-change", "Asset field cardinality changed", (const char*)sMessage, "field", sFieldName);
							if ( sMessage ) xrtFree(sMessage);
							iWarn++;
						}
					}
					if ( !CS_ComponentStorageCompatible(sToComponent, sToStorage) ) {
						str sMessage = xrtFormat("Field %s uses asset component %s with incompatible storage.type %s. Use text for single URLs and json for multi-asset fields before regeneration.",
							sFieldName ? sFieldName : "(unnamed)",
							sToComponent ? sToComponent : "(empty)",
							sToStorage ? sToStorage : "(empty)");
						CS_AdvisorAddScopedItem(arrItems, "error", "asset-storage-mismatch", "Asset field storage is incompatible", (const char*)sMessage, "field", sFieldName);
						if ( sMessage ) xrtFree(sMessage);
						iError++;
					} else if ( bAssetConfigChanged ) {
						str sMessage = xrtFormat("Field %s changed asset upload props such as uploadUrl, accept, acceptMime, or buttonText. Review uploader behavior and operator guidance after regeneration.",
							sFieldName ? sFieldName : "(unnamed)");
						CS_AdvisorAddScopedItem(arrItems, "info", "asset-config-change", "Asset upload behavior changed", (const char*)sMessage, "field", sFieldName);
						if ( sMessage ) xrtFree(sMessage);
						iInfo++;
					}
				}
			}
			if ( (!xvoTableGetBool(tblFromField, "required", 8)) && xvoTableGetBool(tblToField, "required", 8) ) {
				CS_AdvisorAddScopedItem(arrItems, "warn", "field-required-upgrade", "Field became required", "The field changed from optional to required. Existing records may need backfill before validation is enforced.", "field", sFieldName);
				iWarn++;
			}
		}
		for ( int i = 0; i < xvoArrayItemCount(arrFromFields); i++ ) {
			xvalue tblFromField = xvoArrayGetValue(arrFromFields, i);
			const char* sFieldName = xvoTableGetText(tblFromField, "name", 4);
			if ( CS_FindFieldByName(arrToFields, sFieldName) == NULL ) {
				CS_AdvisorAddScopedItem(arrItems, "error", "field-remove", "Field removed", "A field was removed from the spec. This is destructive and needs an explicit migration plan.", "field", sFieldName);
				iError++;
			}
		}
	}

	if ( (arrFromIndexes != NULL) && (xvoType(arrFromIndexes) == XVO_DT_ARRAY) && (arrToIndexes != NULL) && (xvoType(arrToIndexes) == XVO_DT_ARRAY) ) {
		for ( int i = 0; i < xvoArrayItemCount(arrToIndexes); i++ ) {
			xvalue tblToIndex = xvoArrayGetValue(arrToIndexes, i);
			const char* sIndexName = xvoTableGetText(tblToIndex, "name", 4);
			xvalue tblFromIndex = CS_FindIndexByName(arrFromIndexes, sIndexName);

			if ( tblFromIndex == NULL ) {
				CS_AdvisorAddItem(arrItems, "info", "index-add", "Index added", "A new index definition was added. The current managed runtime records this metadata, but physical database indexes still need an explicit migration plan.");
				iInfo++;
				continue;
			}
			{
				xvalue arrFromIndexFields = xvoTableGetValue(tblFromIndex, "fields", 6);
				xvalue arrToIndexFields = xvoTableGetValue(tblToIndex, "fields", 6);
				str sFromFieldsJson = CS_StringifyValueOrEmptyArray(arrFromIndexFields);
				str sToFieldsJson = CS_StringifyValueOrEmptyArray(arrToIndexFields);
				bool bUniqueFrom = xvoTableGetBool(tblFromIndex, "unique", 6);
				bool bUniqueTo = xvoTableGetBool(tblToIndex, "unique", 6);

				if ( ((sFromFieldsJson && sToFieldsJson) && (strcmp((const char*)sFromFieldsJson, (const char*)sToFieldsJson) != 0))
					|| ((sFromFieldsJson == NULL) != (sToFieldsJson == NULL)) ) {
					CS_AdvisorAddItem(arrItems, "warn", "index-fields-change", "Index fields changed", "An index definition changed its field order or coverage. Review migration SQL because managed generation does not auto-apply physical index rebuilds.");
					iWarn++;
				}
				if ( bUniqueFrom != bUniqueTo ) {
					CS_AdvisorAddItem(arrItems, "warn", "index-unique-change", "Index uniqueness changed", "An index changed between unique and non-unique. This can affect existing data validity and should be migrated manually.");
					iWarn++;
				}
				if ( sFromFieldsJson ) xrtFree(sFromFieldsJson);
				if ( sToFieldsJson ) xrtFree(sToFieldsJson);
			}
		}
		for ( int i = 0; i < xvoArrayItemCount(arrFromIndexes); i++ ) {
			xvalue tblFromIndex = xvoArrayGetValue(arrFromIndexes, i);
			const char* sIndexName = xvoTableGetText(tblFromIndex, "name", 4);
			if ( CS_FindIndexByName(arrToIndexes, sIndexName) == NULL ) {
				CS_AdvisorAddItem(arrItems, "info", "index-remove", "Index removed", "An index definition was removed. Review the database migration plan if a physical index should also be dropped.");
				iInfo++;
			}
		}
	}

	if ( xvoArrayItemCount(arrItems) == 0 ) {
		CS_AdvisorAddItem(arrItems, "info", "no-diff", "No structural diff", "No structural change was detected between the compared revisions.");
		iInfo++;
	}

	xvoTableSetInt(tblRet, "fromRevision", 12, iFromRevision);
	xvoTableSetInt(tblRet, "toRevision", 10, iToRevision);
	xvoTableSetInt(tblRet, "infoCount", 9, iInfo);
	xvoTableSetInt(tblRet, "warnCount", 9, iWarn);
	xvoTableSetInt(tblRet, "errorCount", 10, iError);
	xvoTableSetValue(tblRet, "items", 5, arrItems, TRUE);
	return tblRet;
}

xvalue CS_BuildAdvisorForType(sqlite3* pDb, int64 iTypeId, int iTargetRevision)
{
	CSTypeSnapshot snapshot;
	xvalue tblFromSpec = NULL;
	xvalue tblToSpec = NULL;
	xvalue tblAdvisor = NULL;
	sqlite3_stmt* stmt = NULL;
	const char* sSql = "SELECT spec_json FROM content_type_revision WHERE type_id = ? AND revision = ?";
	int iFromRevision = 0;

	if ( !CS_LoadTypeSnapshot(pDb, iTypeId, &snapshot) ) {
		return NULL;
	}
	if ( iTargetRevision <= 0 ) {
		iTargetRevision = snapshot.iCurrentRevision;
	}
	tblToSpec = CS_ParseJsonText(snapshot.sSpecJson);
	if ( iTargetRevision != snapshot.iCurrentRevision ) {
		if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTypeId);
			sqlite3_bind_int(stmt, 2, iTargetRevision);
			if ( sqlite3_step(stmt) == SQLITE_ROW ) {
				if ( tblToSpec ) xvoUnref(tblToSpec);
				tblToSpec = CS_ParseJsonText((const char*)sqlite3_column_text(stmt, 0));
			}
		}
		if ( stmt ) sqlite3_finalize(stmt);
		stmt = NULL;
	}

	iFromRevision = snapshot.iAppliedRevision;
	if ( (iFromRevision <= 0) && (snapshot.iCurrentRevision > 1) ) {
		iFromRevision = snapshot.iCurrentRevision - 1;
	}
	if ( iFromRevision > 0 ) {
		if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTypeId);
			sqlite3_bind_int(stmt, 2, iFromRevision);
			if ( sqlite3_step(stmt) == SQLITE_ROW ) {
				tblFromSpec = CS_ParseJsonText((const char*)sqlite3_column_text(stmt, 0));
			}
		}
		if ( stmt ) sqlite3_finalize(stmt);
	}

	tblAdvisor = CS_BuildAdvisorData(tblFromSpec, iFromRevision, tblToSpec, iTargetRevision);
	if ( tblFromSpec ) xvoUnref(tblFromSpec);
	if ( tblToSpec ) xvoUnref(tblToSpec);
	CS_FreeSnapshot(&snapshot);
	return tblAdvisor;
}

bool CS_SendAssetHtml(XS_ResponseObject objResp, const char* sFileName)
{
	str sPath;
	ptr pData;
	size_t iSize = 0;

	if ( (G_CSRootPath == NULL) || (sFileName == NULL) ) {
		return FALSE;
	}
	sPath = xrtPathJoin(2, G_CSRootPath, (str)sFileName);
	if ( (sPath == NULL) || !xrtFileExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		return FALSE;
	}
	pData = xrtFileGetAll(sPath, &iSize);
	xrtFree(sPath);
	if ( pData == NULL ) {
		return FALSE;
	}
	http_reply(objResp, 200, "Content-Type: text/html; charset=utf-8\r\nCache-Control: no-store, no-cache, must-revalidate\r\nPragma: no-cache\r\nExpires: 0\r\n", pData, iSize);
	xrtFree(pData);
	return TRUE;
}

str CS_LoadAssetText(const char* sFileName)
{
	str sPath;
	ptr pData;
	size_t iSize = 0;

	if ( (G_CSRootPath == NULL) || (sFileName == NULL) ) {
		return NULL;
	}
	sPath = xrtPathJoin(2, G_CSRootPath, (str)sFileName);
	if ( (sPath == NULL) || !xrtFileExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		return NULL;
	}
	pData = xrtFileGetAll(sPath, &iSize);
	xrtFree(sPath);
	if ( pData == NULL ) {
		return NULL;
	}
	return (str)pData;
}

str CS_ReplaceTemplateToken(str sText, const char* sToken, const char* sReplacement)
{
	str sNext;

	if ( (sText == NULL) || (sToken == NULL) ) {
		return sText;
	}
	sNext = xrtReplace(sText, 0, (str)sToken, 0, (str)(sReplacement ? sReplacement : ""), 0, NULL);
	xrtFree(sText);
	return sNext;
}

void CS_AppendTypeRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblItem = xvoCreateTable();

	xvoTableSetInt(tblItem, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetText(tblItem, "typeKey", 7, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
	xvoTableSetText(tblItem, "xid", 3, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetText(tblItem, "name", 4, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetText(tblItem, "namespace", 9, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetText(tblItem, "title", 5, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
	xvoTableSetText(tblItem, "description", 11, (str)sqlite3_column_text(stmt, 6), 0, FALSE);
	xvoTableSetInt(tblItem, "currentRevision", 15, sqlite3_column_int(stmt, 7));
	xvoTableSetText(tblItem, "generatedPluginXid", 18, (str)sqlite3_column_text(stmt, 8), 0, FALSE);
	xvoTableSetInt(tblItem, "appliedRevision", 15, sqlite3_column_int(stmt, 9));
	CS_SetTimeText(tblItem, "updateTimeText", 14, sqlite3_column_int64(stmt, 10));
	xvoArrayAppendValue(arrList, tblItem, TRUE);
}

void CS_RequestTypeList(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet;
	xvalue arrList;
	const char* sSql =
		"SELECT t.id, t.type_key, t.xid, t.name, t.namespace, t.title, t.description, "
		"t.current_revision, t.generated_plugin_xid, COALESCE(g.applied_revision, 0), t.update_time "
		"FROM content_type t "
		"LEFT JOIN content_generated_plugin g ON g.type_id = t.id "
		"ORDER BY t.update_time DESC, t.id DESC";

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	if ( !CS_EnsureSchema() || !CS_OpenDb(&pDb) ) {
		if ( pDb ) CS_CloseDb(pDb);
		CS_SendError(objResp, "无法打开内容系统数据库");
		return;
	}
	tblRet = CS_CreateResult(TRUE, NULL);
	arrList = xvoCreateArray();
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			CS_AppendTypeRow(arrList, stmt);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	CS_CloseDb(pDb);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	CS_SendJsonValue(objResp, tblRet);
}

void CS_RequestTypeDetail(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sId[32];
	sqlite3* pDb = NULL;
	CSTypeSnapshot snapshot;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	xvalue tblData = NULL;
	xvalue arrRevisions = NULL;
	xvalue tblSpec = NULL;
	xvalue tblAdvisor = NULL;
	const char* sSql = "SELECT id, revision, spec_hash, note, generator_version, create_time FROM content_type_revision WHERE type_id = ? ORDER BY revision DESC";

	(void)objServer;
	(void)objHost;
	(void)objSession;

	memset(sId, 0, sizeof(sId));
	HttpGetQueryVar(objReq, "id", sId, sizeof(sId));
	if ( sId[0] == '\0' ) {
		CS_SendError(objResp, "id is required");
		return;
	}
	if ( !CS_EnsureSchema() || !CS_OpenDb(&pDb) ) {
		if ( pDb ) CS_CloseDb(pDb);
		CS_SendError(objResp, "无法打开内容系统数据库");
		return;
	}
	if ( !CS_LoadTypeSnapshot(pDb, atoll(sId), &snapshot) ) {
		CS_CloseDb(pDb);
		CS_SendError(objResp, "content type not found");
		return;
	}

	tblSpec = CS_ParseJsonText(snapshot.sSpecJson);
	tblAdvisor = CS_BuildAdvisorForType(pDb, snapshot.iTypeId, snapshot.iCurrentRevision);
	tblRet = CS_CreateResult(TRUE, NULL);
	tblData = xvoCreateTable();
	arrRevisions = xvoCreateArray();

	xvoTableSetInt(tblData, "id", 2, snapshot.iTypeId);
	xvoTableSetText(tblData, "title", 5, snapshot.sTypeTitle ? snapshot.sTypeTitle : (str)"", 0, FALSE);
	xvoTableSetInt(tblData, "currentRevision", 15, snapshot.iCurrentRevision);
	xvoTableSetInt(tblData, "appliedRevision", 15, snapshot.iAppliedRevision);
	xvoTableSetText(tblData, "generatedPluginXid", 18, snapshot.sPluginXid, 0, FALSE);
	if ( tblSpec ) {
		xvoTableSetValue(tblData, "spec", 4, tblSpec, TRUE);
		tblSpec = NULL;
	}

	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)snapshot.iTypeId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblItem = xvoCreateTable();
			xvoTableSetInt(tblItem, "id", 2, sqlite3_column_int64(stmt, 0));
			xvoTableSetInt(tblItem, "revision", 8, sqlite3_column_int(stmt, 1));
			xvoTableSetText(tblItem, "specHash", 8, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
			xvoTableSetText(tblItem, "note", 4, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
			xvoTableSetText(tblItem, "generatorVersion", 16, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
			CS_SetTimeText(tblItem, "createTimeText", 14, sqlite3_column_int64(stmt, 5));
			xvoArrayAppendValue(arrRevisions, tblItem, TRUE);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	CS_CloseDb(pDb);

	xvoTableSetValue(tblData, "revisions", 9, arrRevisions, TRUE);
	if ( tblAdvisor ) {
		xvoTableSetValue(tblData, "advisor", 7, tblAdvisor, TRUE);
	}
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	CS_SendJsonValue(objResp, tblRet);
	CS_FreeSnapshot(&snapshot);
}

void CS_RequestAdvisor(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sTypeId[32];
	char sRevision[32];
	sqlite3* pDb = NULL;
	xvalue tblRet = NULL;
	xvalue tblAdvisor = NULL;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	memset(sTypeId, 0, sizeof(sTypeId));
	memset(sRevision, 0, sizeof(sRevision));
	HttpGetQueryVar(objReq, "typeId", sTypeId, sizeof(sTypeId));
	HttpGetQueryVar(objReq, "revision", sRevision, sizeof(sRevision));
	if ( sTypeId[0] == '\0' ) {
		CS_SendError(objResp, "typeId is required");
		return;
	}
	if ( !CS_EnsureSchema() || !CS_OpenDb(&pDb) ) {
		if ( pDb ) CS_CloseDb(pDb);
		CS_SendError(objResp, "无法打开内容系统数据库");
		return;
	}
	tblAdvisor = CS_BuildAdvisorForType(pDb, atoll(sTypeId), sRevision[0] ? atoi(sRevision) : 0);
	CS_CloseDb(pDb);
	if ( tblAdvisor == NULL ) {
		CS_SendError(objResp, "无法生成升级顾问结果");
		return;
	}
	tblRet = CS_CreateResult(TRUE, NULL);
	xvoTableSetValue(tblRet, "data", 4, tblAdvisor, TRUE);
	CS_SendJsonValue(objResp, tblRet);
}

void CS_RequestAdminView(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	if ( !CS_SendAssetHtml(objResp, "admin.html") ) {
		http_reply(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "content-system admin page missing", 0);
	}
}

void CS_RequestRevisions(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sTypeId[32];
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet;
	xvalue arrList;
	const char* sSql = "SELECT id, revision, spec_hash, note, generator_version, create_time FROM content_type_revision WHERE type_id = ? ORDER BY revision DESC";

	(void)objServer;
	(void)objHost;
	(void)objSession;

	memset(sTypeId, 0, sizeof(sTypeId));
	HttpGetQueryVar(objReq, "typeId", sTypeId, sizeof(sTypeId));
	if ( sTypeId[0] == '\0' ) {
		CS_SendError(objResp, "typeId is required");
		return;
	}
	if ( !CS_EnsureSchema() || !CS_OpenDb(&pDb) ) {
		if ( pDb ) CS_CloseDb(pDb);
		CS_SendError(objResp, "无法打开内容系统数据库");
		return;
	}

	tblRet = CS_CreateResult(TRUE, NULL);
	arrList = xvoCreateArray();
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)atoll(sTypeId));
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblItem = xvoCreateTable();
			xvoTableSetInt(tblItem, "id", 2, sqlite3_column_int64(stmt, 0));
			xvoTableSetInt(tblItem, "revision", 8, sqlite3_column_int(stmt, 1));
			xvoTableSetText(tblItem, "specHash", 8, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
			xvoTableSetText(tblItem, "note", 4, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
			xvoTableSetText(tblItem, "generatorVersion", 16, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
			CS_SetTimeText(tblItem, "createTimeText", 14, sqlite3_column_int64(stmt, 5));
			xvoArrayAppendValue(arrList, tblItem, TRUE);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	CS_CloseDb(pDb);
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	CS_SendJsonValue(objResp, tblRet);
}

bool CS_SaveTypeInternal(sqlite3* pDb, int64 iTypeId, const char* sNote, xvalue tblSpec, int64* piSavedTypeId, int* piSavedRevision, bool* pbRevisionCreated, str* psMessage)
{
	str sSpecJson = NULL;
	str sSpecHash = NULL;
	str sTypeKey = NULL;
	str sXid = NULL;
	str sName = NULL;
	str sTitle = NULL;
	str sNamespace = NULL;
	str sDescription = NULL;
	str sIcon = NULL;
	str sTable = NULL;
	str sError = NULL;
	str sCurrentHash = NULL;
	sqlite3_stmt* stmt = NULL;
	bool bOK = FALSE;
	bool bExists = FALSE;
	int iCurrentRevision = 0;
	int iFieldCount = 0;
	xtime iNow = xrtNow();

	if ( piSavedTypeId ) *piSavedTypeId = 0;
	if ( piSavedRevision ) *piSavedRevision = 0;
	if ( pbRevisionCreated ) *pbRevisionCreated = FALSE;
	if ( psMessage ) *psMessage = NULL;

	if ( !CS_NormalizeSpec(tblSpec, &sSpecJson, &sSpecHash, &sTypeKey, &sXid, &sName, &sTitle, &sNamespace, &sDescription, &sIcon, &sTable, &iFieldCount, &sError) ) {
		if ( psMessage ) {
			*psMessage = sError ? sError : xrtCopyStr("内容模型规格校验失败", 0);
		} else if ( sError ) {
			xrtFree(sError);
		}
		goto cleanup;
	}
	if ( !CS_ExecSql(pDb, "BEGIN IMMEDIATE") ) {
		if ( psMessage ) *psMessage = xrtCopyStr("无法开始保存事务", 0);
		goto cleanup;
	}

	if ( iTypeId > 0 ) {
		if ( sqlite3_prepare_v2(pDb, "SELECT id, current_revision, spec_hash FROM content_type WHERE id = ?", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTypeId);
			if ( sqlite3_step(stmt) == SQLITE_ROW ) {
				bExists = TRUE;
				iTypeId = sqlite3_column_int64(stmt, 0);
				iCurrentRevision = sqlite3_column_int(stmt, 1);
				sCurrentHash = xrtCopyStr((str)sqlite3_column_text(stmt, 2), 0);
			}
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;

	if ( !bExists ) {
		if ( sqlite3_prepare_v2(pDb, "SELECT id, current_revision, spec_hash FROM content_type WHERE type_key = ?", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sTypeKey, -1, SQLITE_TRANSIENT);
			if ( sqlite3_step(stmt) == SQLITE_ROW ) {
				bExists = TRUE;
				iTypeId = sqlite3_column_int64(stmt, 0);
				iCurrentRevision = sqlite3_column_int(stmt, 1);
				sCurrentHash = xrtCopyStr((str)sqlite3_column_text(stmt, 2), 0);
			}
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;

	if ( !bExists ) {
		if ( sqlite3_prepare_v2(
			pDb,
			"INSERT INTO content_type(type_key, xid, name, namespace, title, description, icon, table_name, field_count, spec_json, spec_hash, current_revision, generated_plugin_xid, create_time, update_time) "
			"VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, '', ?, ?)",
			-1,
			&stmt,
			NULL) != SQLITE_OK ) {
			if ( psMessage ) *psMessage = xrtCopyStr("创建内容模型失败", 0);
			goto cleanup;
		}
		sqlite3_bind_text(stmt, 1, sTypeKey, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, sXid, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, sName, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, sNamespace, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 5, sTitle, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 6, sDescription, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 7, sIcon, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 8, sTable, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 9, iFieldCount);
		sqlite3_bind_text(stmt, 10, sSpecJson, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 11, sSpecHash, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 12, iNow);
		sqlite3_bind_int64(stmt, 13, iNow);
		if ( sqlite3_step(stmt) != SQLITE_DONE ) {
			if ( psMessage ) *psMessage = xrtCopyStr("保存内容模型失败", 0);
			goto cleanup;
		}
		iTypeId = sqlite3_last_insert_rowid(pDb);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;

	if ( (!bExists) || (sCurrentHash == NULL) || (strcmp(sCurrentHash, sSpecHash) != 0) ) {
		int iNewRevision = iCurrentRevision + 1;
		if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_type_revision(type_id, revision, spec_json, spec_hash, note, generator_version, create_time) VALUES(?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL) != SQLITE_OK ) {
			if ( psMessage ) *psMessage = xrtCopyStr("创建修订记录失败", 0);
			goto cleanup;
		}
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTypeId);
		sqlite3_bind_int(stmt, 2, iNewRevision);
		sqlite3_bind_text(stmt, 3, sSpecJson, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, sSpecHash, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 5, sNote ? sNote : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 6, CS_GENERATOR_VERSION, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 7, iNow);
		if ( sqlite3_step(stmt) != SQLITE_DONE ) {
			if ( psMessage ) *psMessage = xrtCopyStr("保存修订记录失败", 0);
			goto cleanup;
		}
		if ( stmt ) sqlite3_finalize(stmt);
		stmt = NULL;

		if ( sqlite3_prepare_v2(pDb, "UPDATE content_type SET xid = ?, name = ?, namespace = ?, title = ?, description = ?, icon = ?, table_name = ?, field_count = ?, spec_json = ?, spec_hash = ?, current_revision = ?, update_time = ? WHERE id = ?", -1, &stmt, NULL) != SQLITE_OK ) {
			if ( psMessage ) *psMessage = xrtCopyStr("更新内容模型失败", 0);
			goto cleanup;
		}
		sqlite3_bind_text(stmt, 1, sXid, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, sName, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, sNamespace, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, sTitle, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 5, sDescription, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 6, sIcon, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 7, sTable, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 8, iFieldCount);
		sqlite3_bind_text(stmt, 9, sSpecJson, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 10, sSpecHash, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 11, iNewRevision);
		sqlite3_bind_int64(stmt, 12, iNow);
		sqlite3_bind_int64(stmt, 13, (sqlite3_int64)iTypeId);
		if ( sqlite3_step(stmt) != SQLITE_DONE ) {
			if ( psMessage ) *psMessage = xrtCopyStr("提交内容模型失败", 0);
			goto cleanup;
		}
		if ( piSavedRevision ) *piSavedRevision = iNewRevision;
		if ( pbRevisionCreated ) *pbRevisionCreated = TRUE;
		if ( psMessage ) *psMessage = xrtCopyStr("修订已保存", 0);
	} else {
		if ( sqlite3_prepare_v2(pDb, "UPDATE content_type SET xid = ?, name = ?, namespace = ?, title = ?, description = ?, icon = ?, table_name = ?, field_count = ?, spec_json = ?, spec_hash = ?, update_time = ? WHERE id = ?", -1, &stmt, NULL) != SQLITE_OK ) {
			if ( psMessage ) *psMessage = xrtCopyStr("更新内容模型失败", 0);
			goto cleanup;
		}
		sqlite3_bind_text(stmt, 1, sXid, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, sName, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, sNamespace, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, sTitle, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 5, sDescription, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 6, sIcon, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 7, sTable, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 8, iFieldCount);
		sqlite3_bind_text(stmt, 9, sSpecJson, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 10, sSpecHash, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 11, iNow);
		sqlite3_bind_int64(stmt, 12, (sqlite3_int64)iTypeId);
		if ( sqlite3_step(stmt) != SQLITE_DONE ) {
			if ( psMessage ) *psMessage = xrtCopyStr("更新内容模型失败", 0);
			goto cleanup;
		}
		if ( piSavedRevision ) *piSavedRevision = iCurrentRevision;
		if ( psMessage ) *psMessage = xrtCopyStr("规格未变化，已刷新元数据", 0);
	}

	if ( !CS_ExecSql(pDb, "COMMIT") ) {
		if ( psMessage ) *psMessage = xrtCopyStr("提交内容模型事务失败", 0);
		goto cleanup;
	}
	if ( piSavedTypeId ) *piSavedTypeId = iTypeId;
	bOK = TRUE;

cleanup:
	if ( stmt ) sqlite3_finalize(stmt);
	if ( !bOK ) {
		CS_ExecSql(pDb, "ROLLBACK");
	}
	if ( sSpecJson ) xrtFree(sSpecJson);
	if ( sSpecHash ) xrtFree(sSpecHash);
	if ( sTypeKey ) xrtFree(sTypeKey);
	if ( sXid ) xrtFree(sXid);
	if ( sName ) xrtFree(sName);
	if ( sTitle ) xrtFree(sTitle);
	if ( sNamespace ) xrtFree(sNamespace);
	if ( sDescription ) xrtFree(sDescription);
	if ( sIcon ) xrtFree(sIcon);
	if ( sTable ) xrtFree(sTable);
	if ( sCurrentHash ) xrtFree(sCurrentHash);
	return bOK;
}

void CS_RequestSaveType(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm = NULL;
	xvalue tblSpec = NULL;
	sqlite3* pDb = NULL;
	xvalue tblRet = NULL;
	int64 iTypeId = 0;
	int iRevision = 0;
	bool bRevisionCreated = FALSE;
	str sMessage = NULL;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !HttpMethodIs(objReq, "POST") ) {
		CS_SendError(objResp, "请求方法不被允许");
		return;
	}
	tblForm = CS_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		CS_SendError(objResp, "请求体 JSON 不合法");
		return;
	}
	tblSpec = xvoTableGetValue(tblForm, "spec", 4);
	if ( (tblSpec == NULL) || (xvoType(tblSpec) != XVO_DT_TABLE) ) {
		xvoUnref(tblForm);
		CS_SendError(objResp, "spec is required");
		return;
	}
	if ( !CS_EnsureSchema() || !CS_OpenDb(&pDb) ) {
		if ( pDb ) CS_CloseDb(pDb);
		xvoUnref(tblForm);
		CS_SendError(objResp, "无法打开内容系统数据库");
		return;
	}
	if ( !CS_SaveTypeInternal(pDb, xvoTableGetInt(tblForm, "id", 2), xvoTableGetText(tblForm, "note", 4), tblSpec, &iTypeId, &iRevision, &bRevisionCreated, &sMessage) ) {
		CS_CloseDb(pDb);
		xvoUnref(tblForm);
		CS_SendError(objResp, sMessage ? (const char*)sMessage : "保存失败");
		if ( sMessage ) xrtFree(sMessage);
		return;
	}
	CS_CloseDb(pDb);
	xvoUnref(tblForm);

	tblRet = CS_CreateResult(TRUE, sMessage ? (const char*)sMessage : "已保存");
	xvoTableSetInt(tblRet, "typeId", 6, iTypeId);
	xvoTableSetInt(tblRet, "revision", 8, iRevision);
	xvoTableSetBool(tblRet, "revisionCreated", 15, bRevisionCreated);
	CS_SendJsonValue(objResp, tblRet);
	if ( sMessage ) xrtFree(sMessage);
}

void CS_RequestDeleteType(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm = NULL;
	xvalue tblRet = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 iTypeId = 0;
	const char* sPluginXid = NULL;
	bool bExists = FALSE;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !HttpMethodIs(objReq, "POST") ) {
		CS_SendError(objResp, "请求方法不被允许");
		return;
	}
	tblForm = CS_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		CS_SendError(objResp, "请求体 JSON 不合法");
		return;
	}
	iTypeId = xvoTableGetInt(tblForm, "id", 2);
	if ( iTypeId <= 0 ) {
		xvoUnref(tblForm);
		CS_SendError(objResp, "缺少模型 ID");
		return;
	}
	if ( !CS_EnsureSchema() || !CS_OpenDb(&pDb) ) {
		if ( pDb ) CS_CloseDb(pDb);
		xvoUnref(tblForm);
		CS_SendError(objResp, "无法打开内容系统数据库");
		return;
	}

	if ( sqlite3_prepare_v2(pDb, "SELECT id, generated_plugin_xid FROM content_type WHERE id = ?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTypeId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			bExists = TRUE;
			sPluginXid = (const char*)sqlite3_column_text(stmt, 1);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;

	if ( !bExists ) {
		CS_CloseDb(pDb);
		xvoUnref(tblForm);
		CS_SendError(objResp, "内容模型不存在");
		return;
	}
	if ( sPluginXid && sPluginXid[0] ) {
		CS_CloseDb(pDb);
		xvoUnref(tblForm);
		CS_SendError(objResp, "该模型已生成受管插件，请先禁用并删除对应插件");
		return;
	}
	if ( !CS_ExecSql(pDb, "BEGIN IMMEDIATE") ) {
		CS_CloseDb(pDb);
		xvoUnref(tblForm);
		CS_SendError(objResp, "无法开始删除事务");
		return;
	}

	if ( sqlite3_prepare_v2(pDb, "DELETE FROM content_upgrade_record WHERE type_id = ?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTypeId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;

	if ( sqlite3_prepare_v2(pDb, "DELETE FROM content_type_revision WHERE type_id = ?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTypeId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;

	if ( sqlite3_prepare_v2(pDb, "DELETE FROM content_generated_plugin WHERE type_id = ?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTypeId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;

	if ( sqlite3_prepare_v2(pDb, "DELETE FROM content_type WHERE id = ?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTypeId);
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;

	if ( !CS_ExecSql(pDb, "COMMIT") ) {
		CS_ExecSql(pDb, "ROLLBACK");
		CS_CloseDb(pDb);
		xvoUnref(tblForm);
		CS_SendError(objResp, "删除内容模型失败");
		return;
	}

	CS_CloseDb(pDb);
	xvoUnref(tblForm);
	tblRet = CS_CreateResult(TRUE, "内容模型已删除");
	xvoTableSetInt(tblRet, "typeId", 6, iTypeId);
	CS_SendJsonValue(objResp, tblRet);
}

void CS_RecordUpgrade(sqlite3* pDb, int64 iTypeId, int iFromRevision, int iToRevision, const char* sPlanJson, const char* sResult)
{
	sqlite3_stmt* stmt = NULL;

	if ( pDb == NULL ) {
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_upgrade_record(type_id, from_revision, to_revision, plan_json, result, create_time) VALUES(?, ?, ?, ?, ?, ?)", -1, &stmt, NULL) != SQLITE_OK ) {
		return;
	}
	sqlite3_bind_int64(stmt, 1, (sqlite3_int64)iTypeId);
	sqlite3_bind_int(stmt, 2, iFromRevision);
	sqlite3_bind_int(stmt, 3, iToRevision);
	sqlite3_bind_text(stmt, 4, sPlanJson ? sPlanJson : "{}", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 5, sResult ? sResult : "pending", -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 6, xrtNow());
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

str CS_BuildManagedPluginManifest(const char* sPluginXid, const char* sTitle, const char* sDescription, xvalue tblSpec)
{
	str sEscTitle = CS_JsonEscape(sTitle ? sTitle : sPluginXid);
	str sEscDesc = CS_JsonEscape(sDescription ? sDescription : "Managed content plugin generated by content-system");
	str sSlotsJson = CS_StringifyValueOrEmptyArray(CS_GetSpecCapabilitySlots(tblSpec));
	str sGroupsJson = CS_StringifyValueOrEmptyArray(CS_GetSpecPresentationGroups(tblSpec));
	str sJson = xrtFormat(
		"{\n"
		"  \"formatVersion\": 4,\n"
		"  \"xid\": \"%s\",\n"
		"  \"name\": \"%s\",\n"
		"  \"title\": \"%s\",\n"
		"  \"description\": \"%s\",\n"
		"  \"version\": \"%s\",\n"
		"  \"author\": \"xAdmin\",\n"
		"  \"kind\": \"singleton\",\n"
		"  \"sort\": 990200,\n"
		"  \"runtime\": { \"compiler\": \"tcc\", \"language\": \"c\" },\n"
		"  \"build\": { \"entry\": \"generated/main.c\", \"sources\": [\"generated/main.c\"], \"includeDirs\": [], \"libraryDirs\": [], \"libraries\": [], \"defines\": [\"XADMIN_PLUGIN=1\"] },\n"
		"  \"compat\": { \"minHostVersion\": \"4.0.0\", \"maxHostVersion\": \"5.0.0\", \"abiVersion\": 4 },\n"
		"  \"capabilities\": [\"route.public\", \"route.admin\"],\n"
		"  \"dependencies\": { \"plugins\": [], \"services\": [], \"features\": [] },\n"
		"  \"contributes\": { \"menus\": [], \"routes\": [], \"hooks\": [], \"events\": [] },\n"
		"  \"defaultConfig\": \"config.defaults.json\",\n"
		"  \"configSchema\": \"config.schema.json\",\n"
		"  \"xadminManaged\": {\n"
		"    \"managed\": true,\n"
		"    \"generator\": \"%s\",\n"
		"    \"managedBy\": \"%s\",\n"
		"    \"managedType\": \"generated-plugin\",\n"
		"    \"marketPublishAllowed\": false,\n"
		"    \"marketUpdatable\": false,\n"
		"    \"contracts\": {\n"
		"      \"capabilitySlots\": %s,\n"
		"      \"presentationGroups\": %s\n"
		"    },\n"
		"    \"generatedRoot\": \"generated\",\n"
		"    \"customRoot\": \"custom\",\n"
		"    \"runtimeRoot\": \"runtime\",\n"
		"    \"mountRegistryFile\": \"custom/capability.mounts.json\",\n"
		"    \"mountSampleFile\": \"runtime/capability.mounts.example.json\",\n"
		"    \"mountRegistrySchemaFile\": \"runtime/capability.mounts.schema.json\",\n"
		"    \"migrationPlanFile\": \"runtime/migration.plan.json\",\n"
		"    \"migrationSqlFile\": \"generated/migration.sql\"\n"
		"  }\n"
		"}\n",
		sPluginXid,
		sPluginXid,
		sEscTitle ? (const char*)sEscTitle : sPluginXid,
		sEscDesc ? (const char*)sEscDesc : "",
		CS_GENERATOR_VERSION,
		CS_PLUGIN_XID,
		CS_PLUGIN_XID,
		sSlotsJson ? (const char*)sSlotsJson : "[]",
		sGroupsJson ? (const char*)sGroupsJson : "[]");
	if ( sEscTitle ) xrtFree(sEscTitle);
	if ( sEscDesc ) xrtFree(sEscDesc);
	if ( sSlotsJson ) xrtFree(sSlotsJson);
	if ( sGroupsJson ) xrtFree(sGroupsJson);
	return sJson;
}

str CS_BuildManagedMainSource(const char* sPluginXid, const char* sTitle)
{
	str sTemplate = CS_LoadAssetText("managed_main.template.c");
	str sEscTitle = CS_JsonEscape(sTitle ? sTitle : sPluginXid);

	if ( sTemplate == NULL ) {
		if ( sEscTitle ) xrtFree(sEscTitle);
		return NULL;
	}
	sTemplate = CS_ReplaceTemplateToken(sTemplate, "{{PLUGIN_XID}}", sPluginXid);
	sTemplate = CS_ReplaceTemplateToken(sTemplate, "{{PLUGIN_TITLE_C}}", sEscTitle ? (const char*)sEscTitle : sPluginXid);
	sTemplate = CS_ReplaceTemplateToken(sTemplate, "{{PLUGIN_VERSION}}", CS_GENERATOR_VERSION);
	if ( sEscTitle ) xrtFree(sEscTitle);
	return sTemplate;
}

str CS_BuildManagedHtml(const char* sPluginXid, const char* sPageTitle, bool bAdmin)
{
	str sTemplate = CS_LoadAssetText(bAdmin ? "managed_admin.template.html" : "managed_public.template.html");
	(void)sPageTitle;
	if ( sTemplate == NULL ) {
		return NULL;
	}
	sTemplate = CS_ReplaceTemplateToken(sTemplate, "{{PLUGIN_XID}}", sPluginXid);
	return sTemplate;
}

str CS_BuildManagedMountSampleJson(xvalue tblSpec)
{
	xvalue tblRoot = xvoCreateTable();
	xvalue arrMounts = xvoCreateArray();
	xvalue arrSlots = CS_GetSpecCapabilitySlots(tblSpec);
	str sJson;

	xvoTableSetValue(tblRoot, "mounts", 6, arrMounts, TRUE);
	if ( (arrSlots != NULL) && (xvoType(arrSlots) == XVO_DT_ARRAY) ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrSlots); i++ ) {
			xvalue tblSlot = xvoArrayGetValue(arrSlots, i);
			xvalue tblMount;
			xvalue tblAdminEntry;
			xvalue tblPublicEntry;
			const char* sKey;
			const char* sTitle;
			const char* sServiceXid;
			const char* sRouteBase;

			if ( (tblSlot == NULL) || (xvoType(tblSlot) != XVO_DT_TABLE) ) {
				continue;
			}
			sKey = xvoTableGetText(tblSlot, "key", 3);
			if ( CS_IsBlank(sKey) ) {
				continue;
			}
			sTitle = xvoTableGetText(tblSlot, "title", 5);
			sServiceXid = xvoTableGetText(tblSlot, "serviceXid", 10);
			sRouteBase = xvoTableGetText(tblSlot, "routeBase", 9);
			tblMount = xvoCreateTable();
			tblAdminEntry = xvoCreateTable();
			tblPublicEntry = xvoCreateTable();

			xvoTableSetText(tblMount, "slotKey", 7, (str)sKey, 0, FALSE);
			xvoTableSetText(tblMount, "providerPlugin", 14, (str)(CS_IsBlank(sServiceXid) ? "" : sServiceXid), 0, FALSE);
			xvoTableSetText(tblMount, "status", 6, "mounted", 0, FALSE);
			xvoTableSetText(tblMount, "note", 4, "Example mount entry. Copy this file into custom/capability.mounts.json and adapt it for the provider plugin.", 0, FALSE);

			xvoTableSetText(tblAdminEntry, "title", 5, (str)(CS_IsBlank(sTitle) ? sKey : sTitle), 0, FALSE);
			xvoTableSetText(tblAdminEntry, "href", 4, (str)(CS_IsBlank(sRouteBase) ? "" : sRouteBase), 0, FALSE);
			xvoTableSetText(tblPublicEntry, "title", 5, (str)(CS_IsBlank(sTitle) ? sKey : sTitle), 0, FALSE);
			xvoTableSetText(tblPublicEntry, "href", 4, (str)(CS_IsBlank(sRouteBase) ? "" : sRouteBase), 0, FALSE);

			xvoTableSetValue(tblMount, "adminEntry", 10, tblAdminEntry, TRUE);
			xvoTableSetValue(tblMount, "publicEntry", 11, tblPublicEntry, TRUE);
			xvoArrayAppendValue(arrMounts, tblMount, TRUE);
		}
	}

	sJson = CS_StringifyJson(tblRoot);
	xvoUnref(tblRoot);
	return sJson;
}

str CS_BuildManagedMountSchemaJson(void)
{
	return xrtCopyStr(
		"{\n"
		"  \"$schema\": \"https://json-schema.org/draft/2020-12/schema\",\n"
		"  \"title\": \"Managed Capability Mount Registry\",\n"
		"  \"type\": \"object\",\n"
		"  \"properties\": {\n"
		"    \"mounts\": {\n"
		"      \"type\": \"array\",\n"
		"      \"items\": {\n"
		"        \"type\": \"object\",\n"
		"        \"properties\": {\n"
		"          \"slotKey\": { \"type\": \"string\" },\n"
		"          \"providerPlugin\": { \"type\": \"string\" },\n"
		"          \"serviceXid\": { \"type\": \"string\" },\n"
		"          \"status\": { \"type\": \"string\" },\n"
		"          \"note\": { \"type\": \"string\" },\n"
		"          \"mount\": {\n"
		"            \"type\": \"object\",\n"
		"            \"properties\": {\n"
		"              \"surface\": { \"type\": \"string\" },\n"
		"              \"mode\": { \"type\": \"string\" }\n"
		"            },\n"
		"            \"additionalProperties\": true\n"
		"          },\n"
		"          \"adminEntry\": {\n"
		"            \"type\": \"object\",\n"
		"            \"properties\": {\n"
		"              \"title\": { \"type\": \"string\" },\n"
		"              \"href\": { \"type\": \"string\" }\n"
		"            },\n"
		"            \"additionalProperties\": true\n"
		"          },\n"
		"          \"publicEntry\": {\n"
		"            \"type\": \"object\",\n"
		"            \"properties\": {\n"
		"              \"title\": { \"type\": \"string\" },\n"
		"              \"href\": { \"type\": \"string\" }\n"
		"            },\n"
		"            \"additionalProperties\": true\n"
		"          }\n"
		"        },\n"
		"        \"required\": [\"slotKey\"],\n"
		"        \"additionalProperties\": true\n"
		"      }\n"
		"    }\n"
		"  },\n"
		"  \"required\": [\"mounts\"],\n"
		"  \"additionalProperties\": false\n"
		"}\n",
		0);
}

str CS_BuildManagedContractsJson(const char* sPluginXid, int64 iTypeId, int iRevision, const char* sSpecHash, xvalue tblSpec)
{
	str sSlotsJson = CS_StringifyValueOrEmptyArray(CS_GetSpecCapabilitySlots(tblSpec));
	str sGroupsJson = CS_StringifyValueOrEmptyArray(CS_GetSpecPresentationGroups(tblSpec));
	str sJson = xrtFormat(
		"{\n"
		"  \"pluginXid\": \"%s\",\n"
		"  \"typeId\": %lld,\n"
		"  \"appliedRevision\": %d,\n"
		"  \"specHash\": \"%s\",\n"
		"  \"capabilitySlots\": %s,\n"
		"  \"presentationGroups\": %s\n"
		"}\n",
		sPluginXid,
		iTypeId,
		iRevision,
		sSpecHash ? sSpecHash : "",
		sSlotsJson ? (const char*)sSlotsJson : "[]",
		sGroupsJson ? (const char*)sGroupsJson : "[]");
	if ( sSlotsJson ) xrtFree(sSlotsJson);
	if ( sGroupsJson ) xrtFree(sGroupsJson);
	return sJson;
}

void CS_AppendOwnedText(str* psDest, const char* sSuffix)
{
	str sNext;

	if ( (psDest == NULL) || (sSuffix == NULL) || (sSuffix[0] == '\0') ) {
		return;
	}
	if ( *psDest == NULL ) {
		*psDest = xrtCopyStr((str)sSuffix, 0);
		return;
	}
	sNext = xrtFormat("%s%s", *psDest, sSuffix);
	xrtFree(*psDest);
	*psDest = sNext;
}

const char* CS_GetMigrationActionForAdvisorItem(xvalue tblItem)
{
	const char* sKind = xvoTableGetText(tblItem, "kind", 4);
	const char* sSeverity = xvoTableGetText(tblItem, "severity", 8);

	if ( strcmp(sSeverity ? sSeverity : "", "error") == 0 ) {
		return "blocked";
	}
	if ( strcmp(sKind ? sKind : "", "initial-generation") == 0
		|| strcmp(sKind ? sKind : "", "meta-change") == 0
		|| strcmp(sKind ? sKind : "", "presentation-group-change") == 0
		|| strcmp(sKind ? sKind : "", "capability-slot-change") == 0
		|| strcmp(sKind ? sKind : "", "field-ui-change") == 0
		|| strcmp(sKind ? sKind : "", "no-diff") == 0 ) {
		return "auto-safe";
	}
	if ( strcmp(sKind ? sKind : "", "field-add") == 0 ) {
		return strcmp(sSeverity ? sSeverity : "", "warn") == 0 ? "review-required" : "auto-safe";
	}
	return strcmp(sSeverity ? sSeverity : "", "warn") == 0 ? "review-required" : "auto-safe";
}

const char* CS_GetMigrationHintForAdvisorItem(xvalue tblItem)
{
	const char* sKind = xvoTableGetText(tblItem, "kind", 4);

	if ( strcmp(sKind ? sKind : "", "field-add") == 0 ) {
		return "Logical field data is stored in payload_json, so no ALTER TABLE is required for this field addition.";
	}
	if ( strcmp(sKind ? sKind : "", "index-add") == 0 ) {
		return "The current generator records index metadata, but physical index SQL still needs manual review because custom fields are not stored as standalone columns.";
	}
	if ( strcmp(sKind ? sKind : "", "no-diff") == 0 ) {
		return "No structural SQL change is required for this revision pair.";
	}
	if ( strcmp(sKind ? sKind : "", "initial-generation") == 0 ) {
		return "Base managed storage will be bootstrapped through the generated schema SQL below.";
	}
	return "";
}

str CS_BuildMigrationPlanJson(const char* sPluginXid, int64 iTypeId, int iFromRevision, int iToRevision, const char* sSpecHash, xvalue tblAdvisor)
{
	xvalue tblRoot = xvoCreateTable();
	xvalue arrItems = xvoCreateArray();
	xvalue arrSql = xvoCreateArray();
	xvalue arrNotes = xvoCreateArray();
	xvalue arrAdvisorItems = tblAdvisor ? xvoTableGetValue(tblAdvisor, "items", 5) : NULL;
	int iAutoSafe = 0;
	int iReview = 0;
	int iBlocked = 0;
	str sJson = NULL;

	xvoTableSetText(tblRoot, "pluginXid", 9, (str)(sPluginXid ? sPluginXid : ""), 0, FALSE);
	xvoTableSetInt(tblRoot, "typeId", 6, iTypeId);
	xvoTableSetInt(tblRoot, "fromRevision", 12, iFromRevision);
	xvoTableSetInt(tblRoot, "toRevision", 10, iToRevision);
	xvoTableSetText(tblRoot, "specHash", 8, (str)(sSpecHash ? sSpecHash : ""), 0, FALSE);
	xvoTableSetText(tblRoot, "storageModel", 12, "content_item.payload_json", 0, FALSE);
	xvoTableSetText(tblRoot, "planMode", 8, (iFromRevision <= 0) ? "initial-generation" : "regeneration", 0, FALSE);
	xvoTableSetInt(tblRoot, "generatedAt", 11, (int)xrtNow());
	xvoArrayAppendText(arrSql, "BEGIN IMMEDIATE;", 0, FALSE);
	xvoArrayAppendText(arrSql, "CREATE TABLE IF NOT EXISTS content_item (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT NOT NULL DEFAULT '', status INTEGER NOT NULL DEFAULT 0, payload_json TEXT NOT NULL DEFAULT '{}', is_draft INTEGER NOT NULL DEFAULT 0, create_time INTEGER NOT NULL, update_time INTEGER NOT NULL DEFAULT 0, delete_time INTEGER NOT NULL DEFAULT 0);", 0, FALSE);
	xvoArrayAppendText(arrSql, "CREATE INDEX IF NOT EXISTS idx_content_item_public ON content_item(delete_time, is_draft, status, update_time DESC);", 0, FALSE);
	xvoArrayAppendText(arrSql, "CREATE INDEX IF NOT EXISTS idx_content_item_admin ON content_item(delete_time, update_time DESC);", 0, FALSE);
	xvoArrayAppendText(arrSql, "COMMIT;", 0, FALSE);
	xvoArrayAppendText(arrNotes, "Managed content items currently persist custom field payloads inside payload_json.", 0, FALSE);
	xvoArrayAppendText(arrNotes, "Field additions update generated form/API behavior immediately, but they do not emit ALTER TABLE ADD COLUMN because field data is not stored as separate SQL columns in phase 1.", 0, FALSE);
	xvoArrayAppendText(arrNotes, "Index definitions are preserved in DSL metadata and advisor output. Physical index rollout beyond the base content_item indexes still requires explicit migration review.", 0, FALSE);

	if ( (arrAdvisorItems != NULL) && (xvoType(arrAdvisorItems) == XVO_DT_ARRAY) ) {
		for ( int i = 0; i < xvoArrayItemCount(arrAdvisorItems); i++ ) {
			xvalue tblItem = xvoArrayGetValue(arrAdvisorItems, i);
			xvalue tblPlanItem = xvoCopy(tblItem);
			const char* sAction = CS_GetMigrationActionForAdvisorItem(tblItem);
			const char* sHint = CS_GetMigrationHintForAdvisorItem(tblItem);
			if ( tblPlanItem == NULL ) {
				continue;
			}
			xvoTableSetText(tblPlanItem, "action", 6, (str)sAction, 0, FALSE);
			if ( !CS_IsBlank(sHint) ) {
				xvoTableSetText(tblPlanItem, "sqlHint", 7, (str)sHint, 0, FALSE);
			}
			xvoArrayAppendValue(arrItems, tblPlanItem, TRUE);
			if ( strcmp(sAction, "auto-safe") == 0 ) {
				iAutoSafe++;
			} else if ( strcmp(sAction, "review-required") == 0 ) {
				iReview++;
			} else {
				iBlocked++;
			}
		}
	}

	xvoTableSetInt(tblRoot, "autoSafeCount", 13, iAutoSafe);
	xvoTableSetInt(tblRoot, "reviewCount", 11, iReview);
	xvoTableSetInt(tblRoot, "blockedCount", 12, iBlocked);
	xvoTableSetValue(tblRoot, "items", 5, arrItems, TRUE);
	xvoTableSetValue(tblRoot, "sql", 3, arrSql, TRUE);
	xvoTableSetValue(tblRoot, "notes", 5, arrNotes, TRUE);
	sJson = CS_StringifyJson(tblRoot);
	xvoUnref(tblRoot);
	return sJson;
}

str CS_BuildMigrationSql(const char* sPluginXid, int64 iTypeId, int iFromRevision, int iToRevision, xvalue tblAdvisor)
{
	xvalue arrAdvisorItems = tblAdvisor ? xvoTableGetValue(tblAdvisor, "items", 5) : NULL;
	str sSql = NULL;
	str sHeader = xrtFormat("-- Plugin: %s | TypeId: %lld | Revisions: %d -> %d\n\n", sPluginXid ? sPluginXid : "", iTypeId, iFromRevision, iToRevision);

	CS_AppendOwnedText(&sSql, "-- Managed content migration plan\n");
	CS_AppendOwnedText(&sSql, "-- Generated by content-system. This file is idempotent for base storage bootstrap and keeps review notes for non-automatic changes.\n");
	CS_AppendOwnedText(&sSql, sHeader ? (const char*)sHeader : "");
	CS_AppendOwnedText(&sSql, "BEGIN IMMEDIATE;\n");
	CS_AppendOwnedText(&sSql, "CREATE TABLE IF NOT EXISTS content_item (\n");
	CS_AppendOwnedText(&sSql, "  id INTEGER PRIMARY KEY AUTOINCREMENT,\n");
	CS_AppendOwnedText(&sSql, "  title TEXT NOT NULL DEFAULT '',\n");
	CS_AppendOwnedText(&sSql, "  status INTEGER NOT NULL DEFAULT 0,\n");
	CS_AppendOwnedText(&sSql, "  payload_json TEXT NOT NULL DEFAULT '{}',\n");
	CS_AppendOwnedText(&sSql, "  is_draft INTEGER NOT NULL DEFAULT 0,\n");
	CS_AppendOwnedText(&sSql, "  create_time INTEGER NOT NULL,\n");
	CS_AppendOwnedText(&sSql, "  update_time INTEGER NOT NULL DEFAULT 0,\n");
	CS_AppendOwnedText(&sSql, "  delete_time INTEGER NOT NULL DEFAULT 0\n");
	CS_AppendOwnedText(&sSql, ");\n");
	CS_AppendOwnedText(&sSql, "CREATE INDEX IF NOT EXISTS idx_content_item_public ON content_item(delete_time, is_draft, status, update_time DESC);\n");
	CS_AppendOwnedText(&sSql, "CREATE INDEX IF NOT EXISTS idx_content_item_admin ON content_item(delete_time, update_time DESC);\n");
	CS_AppendOwnedText(&sSql, "COMMIT;\n\n");
	CS_AppendOwnedText(&sSql, "-- Notes:\n");
	CS_AppendOwnedText(&sSql, "-- 1. Custom field payloads live in payload_json, so field additions do not emit ALTER TABLE ADD COLUMN in phase 1.\n");
	CS_AppendOwnedText(&sSql, "-- 2. Non-base index definitions remain advisory until a physical index strategy is introduced.\n");
	if ( (arrAdvisorItems != NULL) && (xvoType(arrAdvisorItems) == XVO_DT_ARRAY) ) {
		CS_AppendOwnedText(&sSql, "\n-- Advisor actions:\n");
		for ( int i = 0; i < xvoArrayItemCount(arrAdvisorItems); i++ ) {
			xvalue tblItem = xvoArrayGetValue(arrAdvisorItems, i);
			const char* sAction = CS_GetMigrationActionForAdvisorItem(tblItem);
			const char* sSeverity = xvoTableGetText(tblItem, "severity", 8);
			const char* sTitle = xvoTableGetText(tblItem, "title", 5);
			const char* sMessage = xvoTableGetText(tblItem, "message", 7);
			const char* sFieldName = xvoTableGetText(tblItem, "fieldName", 9);
			str sLine = xrtFormat("-- [%s] %s%s%s: %s\n",
				sAction,
				CS_IsBlank(sFieldName) ? "" : sFieldName,
				CS_IsBlank(sFieldName) ? "" : " | ",
				sTitle ? sTitle : (sSeverity ? sSeverity : "advisor"),
				sMessage ? sMessage : "");
			CS_AppendOwnedText(&sSql, sLine ? (const char*)sLine : "");
			if ( sLine ) xrtFree(sLine);
		}
	}
	if ( sHeader ) xrtFree(sHeader);
	return sSql;
}

str CS_BuildManagedJson(const char* sPluginXid, int64 iTypeId, int iRevision, const char* sSpecHash, xvalue tblSpec, xtime iGeneratedAt)
{
	str sSlotsJson = CS_StringifyValueOrEmptyArray(CS_GetSpecCapabilitySlots(tblSpec));
	str sGroupsJson = CS_StringifyValueOrEmptyArray(CS_GetSpecPresentationGroups(tblSpec));
	str sJson = xrtFormat(
		"{\n"
		"  \"managed\": true,\n"
		"  \"managedBy\": \"%s\",\n"
		"  \"managedType\": \"generated-plugin\",\n"
		"  \"generatorId\": \"%s\",\n"
		"  \"generator\": \"%s\",\n"
		"  \"generatorVersion\": \"%s\",\n"
		"  \"pluginXid\": \"%s\",\n"
		"  \"typeId\": %lld,\n"
		"  \"contentTypeId\": %lld,\n"
		"  \"contentTypeRevision\": %d,\n"
		"  \"appliedRevision\": %d,\n"
		"  \"specHash\": \"%s\",\n"
		"  \"marketPublishAllowed\": false,\n"
		"  \"marketUpdatable\": false,\n"
		"  \"generatedRoot\": \"generated\",\n"
		"  \"customRoot\": \"custom\",\n"
		"  \"runtimeRoot\": \"runtime\",\n"
		"  \"contracts\": {\n"
		"    \"capabilitySlots\": %s,\n"
		"    \"presentationGroups\": %s\n"
		"  },\n"
		"  \"mountRegistryFile\": \"custom/capability.mounts.json\",\n"
		"  \"mountSampleFile\": \"runtime/capability.mounts.example.json\",\n"
		"  \"mountRegistrySchemaFile\": \"runtime/capability.mounts.schema.json\",\n"
		"  \"migrationPlanFile\": \"runtime/migration.plan.json\",\n"
		"  \"migrationSqlFile\": \"generated/migration.sql\",\n"
		"  \"migrationState\": {\n"
		"    \"lastAppliedRevision\": %d,\n"
		"    \"lastAppliedAt\": %lld\n"
		"  },\n"
		"  \"generatedAt\": %lld\n"
		"}\n",
		CS_PLUGIN_XID,
		CS_PLUGIN_XID,
		CS_PLUGIN_XID,
		CS_GENERATOR_VERSION,
		sPluginXid,
		iTypeId,
		iTypeId,
		iRevision,
		iRevision,
		sSpecHash ? sSpecHash : "",
		sSlotsJson ? (const char*)sSlotsJson : "[]",
		sGroupsJson ? (const char*)sGroupsJson : "[]",
		iRevision,
		iGeneratedAt,
		iGeneratedAt);
	if ( sSlotsJson ) xrtFree(sSlotsJson);
	if ( sGroupsJson ) xrtFree(sGroupsJson);
	return sJson;
}

void CS_RequestGenerate(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm = NULL;
	sqlite3* pDb = NULL;
	CSTypeSnapshot snapshot;
	xvalue tblSpec = NULL;
	xvalue tblAdvisor = NULL;
	str sSpecHash = NULL;
	str sXid = NULL;
	str sName = NULL;
	str sTitle = NULL;
	str sNamespace = NULL;
	str sDescription = NULL;
	str sIcon = NULL;
	str sTable = NULL;
	str sTypeKey = NULL;
	str sSpecJsonNorm = NULL;
	str sError = NULL;
	str sManagedJson = NULL;
	str sContractsJson = NULL;
	str sMountSampleJson = NULL;
	str sMountSchemaJson = NULL;
	str sPluginJson = NULL;
	str sMainSource = NULL;
	str sAdminHtml = NULL;
	str sPublicHtml = NULL;
	str sConfigDefaults = NULL;
	str sConfigSchema = NULL;
	str sCustomReadme = NULL;
	str sPlanJson = NULL;
	str sMigrationPlanJson = NULL;
	str sMigrationSql = NULL;
	XAdminGeneratedFile files[14];
	XAdminGeneratedPluginSpec spec;
	xvalue tblRet = NULL;
	sqlite3_stmt* stmt = NULL;
	bool bHasGenerated = FALSE;
	int iFieldCount = 0;
	int iGenerateRet = -1;
	xtime iNow = xrtNow();

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !HttpMethodIs(objReq, "POST") ) {
		CS_SendError(objResp, "请求方法不被允许");
		return;
	}
	tblForm = CS_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		CS_SendError(objResp, "请求体 JSON 不合法");
		return;
	}
	if ( !CS_EnsureSchema() || !CS_OpenDb(&pDb) ) {
		if ( pDb ) CS_CloseDb(pDb);
		xvoUnref(tblForm);
		CS_SendError(objResp, "无法打开内容系统数据库");
		return;
	}
	if ( !CS_LoadTypeSnapshot(pDb, xvoTableGetInt(tblForm, "typeId", 6), &snapshot) ) {
		CS_CloseDb(pDb);
		xvoUnref(tblForm);
		CS_SendError(objResp, "content type not found");
		return;
	}

	tblSpec = CS_ParseJsonText(snapshot.sSpecJson);
	if ( !CS_NormalizeSpec(tblSpec, &sSpecJsonNorm, &sSpecHash, &sTypeKey, &sXid, &sName, &sTitle, &sNamespace, &sDescription, &sIcon, &sTable, &iFieldCount, &sError) ) {
		CS_FreeSnapshot(&snapshot);
		CS_CloseDb(pDb);
		xvoUnref(tblForm);
		if ( tblSpec ) xvoUnref(tblSpec);
		CS_SendError(objResp, sError ? (const char*)sError : "内容模型规格校验失败");
		if ( sError ) xrtFree(sError);
		return;
	}

	tblAdvisor = CS_BuildAdvisorForType(pDb, snapshot.iTypeId, snapshot.iCurrentRevision);
	if ( tblAdvisor ) {
		sPlanJson = CS_StringifyJson(tblAdvisor);
	}
	sMigrationPlanJson = CS_BuildMigrationPlanJson(sXid, snapshot.iTypeId, snapshot.iAppliedRevision, snapshot.iCurrentRevision, sSpecHash, tblAdvisor);
	sMigrationSql = CS_BuildMigrationSql(sXid, snapshot.iTypeId, snapshot.iAppliedRevision, snapshot.iCurrentRevision, tblAdvisor);
	sManagedJson = CS_BuildManagedJson(sXid, snapshot.iTypeId, snapshot.iCurrentRevision, sSpecHash, tblSpec, iNow);
	sContractsJson = CS_BuildManagedContractsJson(sXid, snapshot.iTypeId, snapshot.iCurrentRevision, sSpecHash, tblSpec);
	sMountSampleJson = CS_BuildManagedMountSampleJson(tblSpec);
	sMountSchemaJson = CS_BuildManagedMountSchemaJson();
	sPluginJson = CS_BuildManagedPluginManifest(sXid, sTitle ? sTitle : sName, sDescription, tblSpec);
	sMainSource = CS_BuildManagedMainSource(sXid, sTitle ? (const char*)sTitle : (const char*)sName);
	sAdminHtml = CS_BuildManagedHtml(sXid, sTitle ? (const char*)sTitle : "Managed Content Plugin", TRUE);
	sPublicHtml = CS_BuildManagedHtml(sXid, sTitle ? (const char*)sTitle : "Managed Content Plugin", FALSE);
	sConfigDefaults = xrtCopyStr("{\n  \"pageSize\": 20\n}\n", 0);
	sConfigSchema = xrtCopyStr("{\n  \"type\": \"object\",\n  \"properties\": {\n    \"pageSize\": {\n      \"type\": \"integer\",\n      \"title\": \"Page Size\"\n    }\n  },\n  \"additionalProperties\": false\n}\n", 0);
	sCustomReadme = xrtCopyStr("This directory is reserved for user-owned extensions.\nPhase 1 generation does not overwrite files placed here.\n", 0);

	memset(files, 0, sizeof(files));
	files[0].relative_path = "plugin.json";
	files[0].data = sPluginJson;
	files[0].size = sPluginJson ? strlen(sPluginJson) : 0;
	files[1].relative_path = "generated/main.c";
	files[1].data = sMainSource;
	files[1].size = sMainSource ? strlen(sMainSource) : 0;
	files[2].relative_path = "generated/admin.html";
	files[2].data = sAdminHtml;
	files[2].size = sAdminHtml ? strlen(sAdminHtml) : 0;
	files[3].relative_path = "generated/public.html";
	files[3].data = sPublicHtml;
	files[3].size = sPublicHtml ? strlen(sPublicHtml) : 0;
	files[4].relative_path = "config.defaults.json";
	files[4].data = sConfigDefaults;
	files[4].size = sConfigDefaults ? strlen(sConfigDefaults) : 0;
	files[5].relative_path = "config.schema.json";
	files[5].data = sConfigSchema;
	files[5].size = sConfigSchema ? strlen(sConfigSchema) : 0;
	files[6].relative_path = "generated/spec.json";
	files[6].data = sSpecJsonNorm;
	files[6].size = sSpecJsonNorm ? strlen(sSpecJsonNorm) : 0;
	files[7].relative_path = "runtime/managed.json";
	files[7].data = sManagedJson;
	files[7].size = sManagedJson ? strlen(sManagedJson) : 0;
	files[8].relative_path = "custom/README.txt";
	files[8].data = sCustomReadme;
	files[8].size = sCustomReadme ? strlen(sCustomReadme) : 0;
	files[9].relative_path = "runtime/contracts.json";
	files[9].data = sContractsJson;
	files[9].size = sContractsJson ? strlen(sContractsJson) : 0;
	files[10].relative_path = "runtime/capability.mounts.example.json";
	files[10].data = sMountSampleJson;
	files[10].size = sMountSampleJson ? strlen(sMountSampleJson) : 0;
	files[11].relative_path = "runtime/capability.mounts.schema.json";
	files[11].data = sMountSchemaJson;
	files[11].size = sMountSchemaJson ? strlen(sMountSchemaJson) : 0;
	files[12].relative_path = "runtime/migration.plan.json";
	files[12].data = sMigrationPlanJson;
	files[12].size = sMigrationPlanJson ? strlen(sMigrationPlanJson) : 0;
	files[13].relative_path = "generated/migration.sql";
	files[13].data = sMigrationSql;
	files[13].size = sMigrationSql ? strlen(sMigrationSql) : 0;

	memset(&spec, 0, sizeof(spec));
	spec.xid = sXid;
	spec.title = sTitle ? sTitle : sName;
	spec.version = CS_GENERATOR_VERSION;
	spec.entry = "generated/main.c";
	spec.auto_enable = 1;
	spec.file_count = 14;
	spec.files = files;

	iGenerateRet = XAdmin_GeneratePlugin(G_CSHandle, &spec);
	if ( iGenerateRet != 0 ) {
		CS_RecordUpgrade(pDb, snapshot.iTypeId, snapshot.iAppliedRevision, snapshot.iCurrentRevision, sPlanJson, "failed");
		CS_FreeSnapshot(&snapshot);
		CS_CloseDb(pDb);
		xvoUnref(tblForm);
		if ( tblSpec ) xvoUnref(tblSpec);
		if ( tblAdvisor ) xvoUnref(tblAdvisor);
		CS_SendError(objResp, "受管插件生成失败");
		goto cleanup;
	}

	if ( sqlite3_prepare_v2(pDb, "SELECT id FROM content_generated_plugin WHERE type_id = ?", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, (sqlite3_int64)snapshot.iTypeId);
		bHasGenerated = (sqlite3_step(stmt) == SQLITE_ROW);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	stmt = NULL;

	if ( CS_ExecSql(pDb, "BEGIN IMMEDIATE") ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE content_type SET generated_plugin_xid = ?, update_time = ? WHERE id = ?", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sXid, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 2, iNow);
			sqlite3_bind_int64(stmt, 3, (sqlite3_int64)snapshot.iTypeId);
			sqlite3_step(stmt);
		}
		if ( stmt ) sqlite3_finalize(stmt);
		stmt = NULL;

		if ( bHasGenerated ) {
			if ( sqlite3_prepare_v2(pDb, "UPDATE content_generated_plugin SET plugin_xid = ?, applied_revision = ?, status = ?, managed_json = ?, last_generate_time = ?, last_upgrade_time = ?, update_time = ? WHERE type_id = ?", -1, &stmt, NULL) == SQLITE_OK ) {
				sqlite3_bind_text(stmt, 1, sXid, -1, SQLITE_TRANSIENT);
				sqlite3_bind_int(stmt, 2, snapshot.iCurrentRevision);
				sqlite3_bind_text(stmt, 3, "generated", -1, SQLITE_TRANSIENT);
				sqlite3_bind_text(stmt, 4, sManagedJson ? (const char*)sManagedJson : "{}", -1, SQLITE_TRANSIENT);
				sqlite3_bind_int64(stmt, 5, iNow);
				sqlite3_bind_int64(stmt, 6, iNow);
				sqlite3_bind_int64(stmt, 7, iNow);
				sqlite3_bind_int64(stmt, 8, (sqlite3_int64)snapshot.iTypeId);
				sqlite3_step(stmt);
			}
		} else {
			if ( sqlite3_prepare_v2(pDb, "INSERT INTO content_generated_plugin(type_id, plugin_xid, applied_revision, status, managed_json, last_generate_time, last_upgrade_time, create_time, update_time) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL) == SQLITE_OK ) {
				sqlite3_bind_int64(stmt, 1, (sqlite3_int64)snapshot.iTypeId);
				sqlite3_bind_text(stmt, 2, sXid, -1, SQLITE_TRANSIENT);
				sqlite3_bind_int(stmt, 3, snapshot.iCurrentRevision);
				sqlite3_bind_text(stmt, 4, "generated", -1, SQLITE_TRANSIENT);
				sqlite3_bind_text(stmt, 5, sManagedJson ? (const char*)sManagedJson : "{}", -1, SQLITE_TRANSIENT);
				sqlite3_bind_int64(stmt, 6, iNow);
				sqlite3_bind_int64(stmt, 7, iNow);
				sqlite3_bind_int64(stmt, 8, iNow);
				sqlite3_bind_int64(stmt, 9, iNow);
				sqlite3_step(stmt);
			}
		}
		if ( stmt ) sqlite3_finalize(stmt);
		stmt = NULL;
		CS_RecordUpgrade(pDb, snapshot.iTypeId, snapshot.iAppliedRevision, snapshot.iCurrentRevision, sPlanJson, "success");
		CS_ExecSql(pDb, "COMMIT");
	} else {
		CS_RecordUpgrade(pDb, snapshot.iTypeId, snapshot.iAppliedRevision, snapshot.iCurrentRevision, sPlanJson, "warning");
	}

	tblRet = CS_CreateResult(TRUE, "受管插件已生成");
	xvoTableSetText(tblRet, "pluginXid", 9, sXid, 0, FALSE);
	xvoTableSetInt(tblRet, "revision", 8, snapshot.iCurrentRevision);
	CS_SendJsonValue(objResp, tblRet);

	CS_FreeSnapshot(&snapshot);
	CS_CloseDb(pDb);
	xvoUnref(tblForm);
	if ( tblSpec ) xvoUnref(tblSpec);
	if ( tblAdvisor ) xvoUnref(tblAdvisor);

cleanup:
	if ( stmt ) sqlite3_finalize(stmt);
	if ( sSpecHash ) xrtFree(sSpecHash);
	if ( sXid ) xrtFree(sXid);
	if ( sName ) xrtFree(sName);
	if ( sTitle ) xrtFree(sTitle);
	if ( sNamespace ) xrtFree(sNamespace);
	if ( sDescription ) xrtFree(sDescription);
	if ( sIcon ) xrtFree(sIcon);
	if ( sTable ) xrtFree(sTable);
	if ( sTypeKey ) xrtFree(sTypeKey);
	if ( sSpecJsonNorm ) xrtFree(sSpecJsonNorm);
	if ( sManagedJson ) xrtFree(sManagedJson);
	if ( sContractsJson ) xrtFree(sContractsJson);
	if ( sMountSampleJson ) xrtFree(sMountSampleJson);
	if ( sMountSchemaJson ) xrtFree(sMountSchemaJson);
	if ( sPluginJson ) xrtFree(sPluginJson);
	if ( sMainSource ) xrtFree(sMainSource);
	if ( sAdminHtml ) xrtFree(sAdminHtml);
	if ( sPublicHtml ) xrtFree(sPublicHtml);
	if ( sConfigDefaults ) xrtFree(sConfigDefaults);
	if ( sConfigSchema ) xrtFree(sConfigSchema);
	if ( sCustomReadme ) xrtFree(sCustomReadme);
	if ( sMigrationPlanJson ) xrtFree(sMigrationPlanJson);
	if ( sMigrationSql ) xrtFree(sMigrationSql);
	if ( sPlanJson ) xrtFree(sPlanJson);
}

int CS_OnLoad(XAdminPluginHandle* out_handle)
{
	if ( out_handle ) {
		G_CSHandle = *out_handle;
	}
	return 0;
}

int CS_OnStart(XAdminPluginHandle handle)
{
	XAdminRouteDecl route;
	XAdminMenuDecl menu;

	if ( !CS_EnsureSchema() ) {
		return -1;
	}
	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/content-system/types";
	route.proc = CS_RequestTypeList;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/content-system/type";
	route.proc = CS_RequestTypeDetail;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/content-system/type/save";
	route.proc = CS_RequestSaveType;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/content-system/type/delete";
	route.proc = CS_RequestDeleteType;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/content-system/revisions";
	route.proc = CS_RequestRevisions;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/content-system/advisor";
	route.proc = CS_RequestAdvisor;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/content-system/generate";
	route.proc = CS_RequestGenerate;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/content-system";
	route.proc = CS_RequestAdminView;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&menu, 0, sizeof(menu));
	menu.title = G_CSConfig.sMenuTitle;
	menu.icon = G_CSConfig.sMenuIcon;
	menu.type = 1;
	menu.open_type = "_iframe";
	menu.href = "/admin/view/plugin/content-system";
	menu.sort = G_CSConfig.iMenuSort;
	menu.visible = TRUE;
	menu.remark = "Managed content plugin generator";
	if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) return -1;
	return 0;
}

int CS_OnConfigChanged(XAdminPluginHandle handle, xvalue new_cfg)
{
	(void)handle;
	CS_ConfigReset();
	if ( new_cfg ) {
		CS_CopyText(G_CSConfig.sMenuTitle, sizeof(G_CSConfig.sMenuTitle), xvoTableGetText(new_cfg, "menuTitle", 9), "Content System");
		CS_CopyText(G_CSConfig.sMenuIcon, sizeof(G_CSConfig.sMenuIcon), xvoTableGetText(new_cfg, "menuIcon", 8), "layui-icon layui-icon-template-1");
		if ( xvoTableGetInt(new_cfg, "menuSort", 8) > 0 ) {
			G_CSConfig.iMenuSort = (int)xvoTableGetInt(new_cfg, "menuSort", 8);
		}
	}
	return 0;
}

void CS_OnStop(XAdminPluginHandle handle)
{
	(void)handle;
}

void CS_OnUnload(XAdminPluginHandle handle)
{
	(void)handle;
}

static XAdminPluginDescriptor G_ContentSystemPlugin = {
	XADMIN_ABI_VERSION,
	sizeof(XAdminPluginDescriptor),
	CS_PLUGIN_XID,
	CS_GENERATOR_VERSION,
	"Content System",
	CS_OnLoad,
	NULL,
	CS_OnStart,
	CS_OnConfigChanged,
	NULL,
	CS_OnStop,
	CS_OnUnload
};

XADMIN_DECLARE_PLUGIN(G_ContentSystemPlugin)
