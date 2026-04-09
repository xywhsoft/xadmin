#include <xs_plugin.h>

typedef struct {
	char sPublicTitle[128];
	char sDefaultAuthor[128];
	bool bModerationEnabled;
} CommentConfigState;

typedef struct {
	char sContentPlugin[128];
	int64 iContentId;
} CommentRequestContext;

typedef struct {
	char sThreadTitle[128];
	bool bShowAdminLink;
	bool bAllowPublicPost;
	bool bNewestFirst;
	int iPageSize;
	int iModerationMode;
} CommentMountOptions;

static XAdminPluginHandle G_CommentHandle = NULL;
static const char* G_CommentRootPath = NULL;
static const char* G_CommentPrivateDbPath = NULL;
static CommentConfigState G_CommentConfig = {
	"Comments",
	"Guest",
	TRUE
};

static const char* G_CommentSchemaSql =
	"CREATE TABLE IF NOT EXISTS comment_item ("
	"id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"author TEXT NOT NULL DEFAULT '',"
	"body TEXT NOT NULL DEFAULT '',"
	"status INTEGER NOT NULL DEFAULT 0,"
	"create_time INTEGER NOT NULL,"
	"update_time INTEGER NOT NULL DEFAULT 0,"
	"delete_time INTEGER NOT NULL DEFAULT 0"
	");";

static const char* G_CommentIndexSql =
	"CREATE INDEX IF NOT EXISTS idx_comment_public ON comment_item(delete_time, status, create_time DESC);"
	"CREATE INDEX IF NOT EXISTS idx_comment_admin ON comment_item(delete_time, update_time DESC);"
	"CREATE INDEX IF NOT EXISTS idx_comment_context_public ON comment_item(content_plugin, content_item_id, delete_time, status, create_time DESC);"
	"CREATE INDEX IF NOT EXISTS idx_comment_context_admin ON comment_item(content_plugin, content_item_id, delete_time, update_time DESC);";

XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int idx, void* ptr)
{
	if ( idx == XADMIN_GLOBAL_PLUGIN_ROOT_PATH ) {
		G_CommentRootPath = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_PRIVATE_DB_PATH ) {
		G_CommentPrivateDbPath = (const char*)ptr;
	}
}

bool Comment_IsBlank(const char* sText)
{
	const unsigned char* p = (const unsigned char*)sText;

	if ( p == NULL ) return TRUE;
	while ( *p ) {
		if ( (*p != ' ') && (*p != '\t') && (*p != '\r') && (*p != '\n') ) return FALSE;
		p++;
	}
	return TRUE;
}

void Comment_CopyText(char* sTarget, size_t iTargetSize, const char* sSource)
{
	if ( (sTarget == NULL) || (iTargetSize == 0) ) return;
	snprintf(sTarget, iTargetSize, "%s", sSource ? sSource : "");
}

void Comment_ContextReset(CommentRequestContext* pContext)
{
	if ( pContext == NULL ) return;
	memset(pContext, 0, sizeof(*pContext));
}

int Comment_ContextState(const CommentRequestContext* pContext)
{
	bool bHasPlugin;
	bool bHasContentId;

	if ( pContext == NULL ) return 0;
	bHasPlugin = !Comment_IsBlank(pContext->sContentPlugin);
	bHasContentId = pContext->iContentId > 0;
	if ( bHasPlugin && bHasContentId ) return 2;
	if ( bHasPlugin || bHasContentId ) return 1;
	return 0;
}

void Comment_ReadTextQuery(XS_RequestObject objReq, const char* sName, char* sValue, size_t iValueSize)
{
	if ( (sValue == NULL) || (iValueSize == 0) ) return;
	memset(sValue, 0, iValueSize);
	if ( (objReq == NULL) || (sName == NULL) ) return;
	HttpGetQueryVar(objReq, sName, sValue, iValueSize);
}

void Comment_ContextFromQuery(XS_RequestObject objReq, CommentRequestContext* pContext)
{
	char sContentPlugin[128];
	char sContentId[32];

	if ( pContext == NULL ) return;
	Comment_ReadTextQuery(objReq, "contentPlugin", sContentPlugin, sizeof(sContentPlugin));
	Comment_ReadTextQuery(objReq, "contentId", sContentId, sizeof(sContentId));
	if ( !Comment_IsBlank(sContentPlugin) ) Comment_CopyText(pContext->sContentPlugin, sizeof(pContext->sContentPlugin), sContentPlugin);
	if ( sContentId[0] ) pContext->iContentId = atoll(sContentId);
}

void Comment_ContextFromBody(xvalue tblForm, CommentRequestContext* pContext)
{
	const char* sContentPlugin;

	if ( (tblForm == NULL) || (pContext == NULL) ) return;
	sContentPlugin = xvoTableGetText(tblForm, "contentPlugin", 13);
	if ( !Comment_IsBlank(sContentPlugin) ) Comment_CopyText(pContext->sContentPlugin, sizeof(pContext->sContentPlugin), sContentPlugin);
	pContext->iContentId = xvoTableGetInt(tblForm, "contentId", 9);
}

xvalue Comment_ParseMountOptionsQuery(XS_RequestObject objReq)
{
	char sJson[4096];
	xvalue tblOptions;

	Comment_ReadTextQuery(objReq, "mountOptions", sJson, sizeof(sJson));
	if ( Comment_IsBlank(sJson) ) return xvoCreateTable();
	tblOptions = xrtParseJSON((str)sJson, strlen(sJson));
	if ( (tblOptions == NULL) || (xvoType(tblOptions) != XVO_DT_TABLE) ) {
		if ( tblOptions ) xvoUnref(tblOptions);
		return xvoCreateTable();
	}
	return tblOptions;
}

void Comment_MountOptionsReset(CommentMountOptions* pOptions)
{
	if ( pOptions == NULL ) return;
	memset(pOptions, 0, sizeof(*pOptions));
	pOptions->bShowAdminLink = TRUE;
	pOptions->bAllowPublicPost = TRUE;
	pOptions->bNewestFirst = TRUE;
	pOptions->iPageSize = 0;
	pOptions->iModerationMode = 0;
}

int Comment_IntValue(xvalue objValue, int iDefault)
{
	if ( objValue == NULL ) return iDefault;
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
			return iDefault;
	}
}

bool Comment_BoolValue(xvalue objValue, bool bDefault)
{
	if ( objValue == NULL ) return bDefault;
	switch ( xvoType(objValue) ) {
		case XVO_DT_BOOL:
			return xvoGetBool(objValue) ? TRUE : FALSE;
		case XVO_DT_INT:
			return xvoGetInt(objValue) != 0;
		case XVO_DT_FLOAT:
			return xvoGetFloat(objValue) != 0.0;
		case XVO_DT_TEXT: {
			const char* sText = xvoGetText(objValue);
			if ( sText == NULL ) return bDefault;
			if ( strcmp(sText, "0") == 0 || strcmp(sText, "false") == 0 || strcmp(sText, "FALSE") == 0 ) return FALSE;
			if ( strcmp(sText, "1") == 0 || strcmp(sText, "true") == 0 || strcmp(sText, "TRUE") == 0 ) return TRUE;
			return bDefault;
		}
		default:
			return bDefault;
	}
}

void Comment_MountOptionsFromTable(xvalue tblOptions, CommentMountOptions* pOptions)
{
	const char* sThreadTitle;
	const char* sSortOrder;
	const char* sModerationMode;
	int iPageSize;

	if ( pOptions == NULL ) return;
	Comment_MountOptionsReset(pOptions);
	if ( (tblOptions == NULL) || (xvoType(tblOptions) != XVO_DT_TABLE) ) return;
	sThreadTitle = xvoTableGetText(tblOptions, "threadTitle", 11);
	if ( !Comment_IsBlank(sThreadTitle) ) Comment_CopyText(pOptions->sThreadTitle, sizeof(pOptions->sThreadTitle), sThreadTitle);
	pOptions->bShowAdminLink = Comment_BoolValue(xvoTableGetValue(tblOptions, "showAdminLink", 13), TRUE);
	pOptions->bAllowPublicPost = Comment_BoolValue(xvoTableGetValue(tblOptions, "allowPublicPost", 15), TRUE);
	iPageSize = Comment_IntValue(xvoTableGetValue(tblOptions, "pageSize", 8), 0);
	if ( iPageSize < 0 ) iPageSize = 0;
	if ( iPageSize > 200 ) iPageSize = 200;
	pOptions->iPageSize = iPageSize;
	sSortOrder = xvoTableGetText(tblOptions, "sortOrder", 9);
	if ( !Comment_IsBlank(sSortOrder) && (strcmp(sSortOrder, "oldest") == 0) ) {
		pOptions->bNewestFirst = FALSE;
	}
	sModerationMode = xvoTableGetText(tblOptions, "moderationMode", 14);
	if ( !Comment_IsBlank(sModerationMode) ) {
		if ( strcmp(sModerationMode, "require-review") == 0 ) pOptions->iModerationMode = 1;
		else if ( strcmp(sModerationMode, "auto-publish") == 0 ) pOptions->iModerationMode = 2;
	}
}

xvalue Comment_BuildMountOptionsValue(const CommentMountOptions* pOptions)
{
	xvalue tblRet = xvoCreateTable();

	if ( pOptions == NULL ) return tblRet;
	if ( !Comment_IsBlank(pOptions->sThreadTitle) ) xvoTableSetText(tblRet, "threadTitle", 11, (str)pOptions->sThreadTitle, 0, FALSE);
	xvoTableSetBool(tblRet, "showAdminLink", 13, pOptions->bShowAdminLink);
	xvoTableSetBool(tblRet, "allowPublicPost", 15, pOptions->bAllowPublicPost);
	xvoTableSetText(tblRet, "sortOrder", 9, (str)(pOptions->bNewestFirst ? "newest" : "oldest"), 0, FALSE);
	xvoTableSetText(tblRet, "moderationMode", 14, (str)(pOptions->iModerationMode == 1 ? "require-review" : (pOptions->iModerationMode == 2 ? "auto-publish" : "inherit")), 0, FALSE);
	if ( pOptions->iPageSize > 0 ) xvoTableSetInt(tblRet, "pageSize", 8, pOptions->iPageSize);
	return tblRet;
}

xvalue Comment_ParseMountOptionsBody(xvalue tblForm)
{
	xvalue tblOptions = xvoTableGetValue(tblForm, "mountOptions", 12);
	if ( (tblOptions == NULL) || (xvoType(tblOptions) != XVO_DT_TABLE) ) return xvoCreateTable();
	return xvoCopy(tblOptions);
}

void Comment_ConfigReset(void)
{
	snprintf(G_CommentConfig.sPublicTitle, sizeof(G_CommentConfig.sPublicTitle), "%s", "Comments");
	snprintf(G_CommentConfig.sDefaultAuthor, sizeof(G_CommentConfig.sDefaultAuthor), "%s", "Guest");
	G_CommentConfig.bModerationEnabled = TRUE;
}

bool Comment_OpenDb(sqlite3** ppDb)
{
	sqlite3* pDb = NULL;

	if ( ppDb ) *ppDb = NULL;
	if ( (ppDb == NULL) || (G_CommentPrivateDbPath == NULL) || (G_CommentPrivateDbPath[0] == '\0') ) return FALSE;
	if ( sqlite3_open_v2(G_CommentPrivateDbPath, &pDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL) != SQLITE_OK ) {
		if ( pDb ) sqlite3_close(pDb);
		return FALSE;
	}
	sqlite3_busy_timeout(pDb, 3000);
	*ppDb = pDb;
	return TRUE;
}

void Comment_CloseDb(sqlite3* pDb)
{
	if ( pDb ) sqlite3_close(pDb);
}

bool Comment_ExecSql(sqlite3* pDb, const char* sSql)
{
	char* sError = NULL;
	int iRet;

	if ( (pDb == NULL) || (sSql == NULL) ) return FALSE;
	iRet = sqlite3_exec(pDb, sSql, NULL, NULL, &sError);
	if ( sError ) sqlite3_free(sError);
	return iRet == SQLITE_OK;
}

bool Comment_TableHasColumn(sqlite3* pDb, const char* sTable, const char* sColumn)
{
	char sSql[128];
	sqlite3_stmt* stmt = NULL;
	bool bFound = FALSE;

	if ( (pDb == NULL) || (sTable == NULL) || (sColumn == NULL) ) return FALSE;
	snprintf(sSql, sizeof(sSql), "PRAGMA table_info(%s)", sTable);
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) != SQLITE_OK ) return FALSE;
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		const char* sName = (const char*)sqlite3_column_text(stmt, 1);
		if ( (sName != NULL) && (strcmp(sName, sColumn) == 0) ) {
			bFound = TRUE;
			break;
		}
	}
	sqlite3_finalize(stmt);
	return bFound;
}

bool Comment_EnsureColumn(sqlite3* pDb, const char* sTable, const char* sColumn, const char* sAlterSql)
{
	if ( (pDb == NULL) || (sTable == NULL) || (sColumn == NULL) || (sAlterSql == NULL) ) return FALSE;
	if ( Comment_TableHasColumn(pDb, sTable, sColumn) ) return TRUE;
	return Comment_ExecSql(pDb, sAlterSql);
}

bool Comment_EnsureSchema(void)
{
	sqlite3* pDb = NULL;
	bool bOK = FALSE;

	if ( !Comment_OpenDb(&pDb) ) return FALSE;
	bOK = Comment_ExecSql(pDb, G_CommentSchemaSql);
	if ( bOK ) bOK = Comment_EnsureColumn(pDb, "comment_item", "content_plugin", "ALTER TABLE comment_item ADD COLUMN content_plugin TEXT NOT NULL DEFAULT '';");
	if ( bOK ) bOK = Comment_EnsureColumn(pDb, "comment_item", "content_item_id", "ALTER TABLE comment_item ADD COLUMN content_item_id INTEGER NOT NULL DEFAULT 0;");
	if ( bOK ) bOK = Comment_ExecSql(pDb, G_CommentIndexSql);
	Comment_CloseDb(pDb);
	return bOK;
}

xvalue Comment_CreateResult(bool bResult, const char* sMessage)
{
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, bResult);
	if ( sMessage ) xvoTableSetText(tblRet, "message", 7, (str)sMessage, 0, FALSE);
	return tblRet;
}

void Comment_SendJsonValue(XS_ResponseObject objResp, xvalue objValue)
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

void Comment_SendError(XS_ResponseObject objResp, const char* sMessage)
{
	Comment_SendJsonValue(objResp, Comment_CreateResult(FALSE, sMessage ? sMessage : "request failed"));
}

bool Comment_ValidateContextOrReply(XS_ResponseObject objResp, const CommentRequestContext* pContext)
{
	if ( Comment_ContextState(pContext) == 1 ) {
		Comment_SendError(objResp, "content context requires both contentPlugin and contentId");
		return FALSE;
	}
	return TRUE;
}

xvalue Comment_ParseJsonBody(XS_RequestObject objReq)
{
	xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( (tblForm == NULL) || (xvoType(tblForm) != XVO_DT_TABLE) ) {
		if ( tblForm ) xvoUnref(tblForm);
		return NULL;
	}
	return tblForm;
}

bool Comment_SendAssetHtml(XS_ResponseObject objResp, const char* sFileName)
{
	str sPath;
	ptr pData;
	size_t iSize = 0;

	if ( (G_CommentRootPath == NULL) || (sFileName == NULL) ) return FALSE;
	sPath = xrtPathJoin(2, G_CommentRootPath, (str)sFileName);
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

int Comment_ReadIntQuery(XS_RequestObject objReq, const char* sName, int iDefault)
{
	char sValue[32];

	memset(sValue, 0, sizeof(sValue));
	HttpGetQueryVar(objReq, sName, sValue, sizeof(sValue));
	return sValue[0] ? atoi(sValue) : iDefault;
}

void Comment_SetTimeText(xvalue tblItem, const char* sKey, int iKeyLen, xtime iTime)
{
	str sText = (iTime > 0) ? xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME) : xrtCopyStr("", 0);
	xvoTableSetText(tblItem, sKey, iKeyLen, sText ? sText : (str)"", 0, TRUE);
}

void Comment_AppendRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblItem = xvoCreateTable();

	xvoTableSetInt(tblItem, "id", 2, sqlite3_column_int64(stmt, 0));
	xvoTableSetText(tblItem, "author", 6, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
	xvoTableSetText(tblItem, "body", 4, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetInt(tblItem, "status", 6, sqlite3_column_int(stmt, 3));
	Comment_SetTimeText(tblItem, "createTimeText", 14, sqlite3_column_int64(stmt, 4));
	Comment_SetTimeText(tblItem, "updateTimeText", 14, sqlite3_column_int64(stmt, 5));
	xvoTableSetText(tblItem, "contentPlugin", 13, (str)sqlite3_column_text(stmt, 6), 0, FALSE);
	xvoTableSetInt(tblItem, "contentItemId", 13, sqlite3_column_int64(stmt, 7));
	xvoArrayAppendValue(arrList, tblItem, TRUE);
}

void Comment_RequestMetaCommon(XS_ResponseObject objResp, XS_RequestObject objReq, bool bAdmin)
{
	xvalue tblRet = Comment_CreateResult(TRUE, NULL);
	xvalue tblData = xvoCreateTable();
	xvalue tblMountOptions = NULL;
	xvalue tblResolvedMountOptions = NULL;
	CommentRequestContext context;
	CommentMountOptions mountOptions;
	char sContextLabel[196];
	char sMountSlotKey[128];
	int iContextState;

	Comment_ContextReset(&context);
	Comment_ContextFromQuery(objReq, &context);
	iContextState = Comment_ContextState(&context);
	memset(sMountSlotKey, 0, sizeof(sMountSlotKey));
	Comment_ReadTextQuery(objReq, "mountSlotKey", sMountSlotKey, sizeof(sMountSlotKey));
	tblMountOptions = Comment_ParseMountOptionsQuery(objReq);
	Comment_MountOptionsFromTable(tblMountOptions, &mountOptions);
	tblResolvedMountOptions = Comment_BuildMountOptionsValue(&mountOptions);
	memset(sContextLabel, 0, sizeof(sContextLabel));
	if ( iContextState == 2 ) snprintf(sContextLabel, sizeof(sContextLabel), "%s #%lld", context.sContentPlugin, (long long)context.iContentId);
	else if ( iContextState == 1 ) snprintf(sContextLabel, sizeof(sContextLabel), "%s", "Partial content context");
	else snprintf(sContextLabel, sizeof(sContextLabel), "%s", "Global comments");

	xvoTableSetText(tblData, "pluginXid", 9, "comment-system", 0, FALSE);
	xvoTableSetText(tblData, "title", 5, (str)(Comment_IsBlank(mountOptions.sThreadTitle) ? "Comment System" : mountOptions.sThreadTitle), 0, FALSE);
	xvoTableSetText(tblData, "publicTitle", 11, (str)(Comment_IsBlank(mountOptions.sThreadTitle) ? G_CommentConfig.sPublicTitle : mountOptions.sThreadTitle), 0, FALSE);
	xvoTableSetText(tblData, "defaultAuthor", 13, (str)G_CommentConfig.sDefaultAuthor, 0, FALSE);
	xvoTableSetBool(tblData, "moderationEnabled", 17, G_CommentConfig.bModerationEnabled);
	xvoTableSetBool(tblData, "showAdminLink", 13, mountOptions.bShowAdminLink);
	xvoTableSetBool(tblData, "allowPublicPost", 15, mountOptions.bAllowPublicPost);
	xvoTableSetInt(tblData, "pageSize", 8, mountOptions.iPageSize);
	xvoTableSetText(tblData, "sortOrder", 9, (str)(mountOptions.bNewestFirst ? "newest" : "oldest"), 0, FALSE);
	xvoTableSetText(tblData, "moderationMode", 14, (str)(mountOptions.iModerationMode == 1 ? "require-review" : (mountOptions.iModerationMode == 2 ? "auto-publish" : "inherit")), 0, FALSE);
	xvoTableSetBool(tblData, "admin", 5, bAdmin);
	xvoTableSetText(tblData, "contentPlugin", 13, (str)context.sContentPlugin, 0, FALSE);
	xvoTableSetInt(tblData, "contentItemId", 13, context.iContentId);
	xvoTableSetBool(tblData, "contextBound", 12, iContextState == 2);
	xvoTableSetBool(tblData, "contextPartial", 14, iContextState == 1);
	xvoTableSetText(tblData, "contextLabel", 12, (str)sContextLabel, 0, FALSE);
	xvoTableSetText(tblData, "mountSlotKey", 12, (str)sMountSlotKey, 0, FALSE);
	xvoTableSetValue(tblData, "mountOptions", 12, tblResolvedMountOptions ? tblResolvedMountOptions : xvoCreateTable(), TRUE);
	if ( tblMountOptions ) xvoUnref(tblMountOptions);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	Comment_SendJsonValue(objResp, tblRet);
}

void Comment_RequestMetaPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	Comment_RequestMetaCommon(objResp, objReq, FALSE);
}

void Comment_RequestMetaAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	Comment_RequestMetaCommon(objResp, objReq, TRUE);
}

void Comment_RequestListCommon(XS_ResponseObject objResp, XS_RequestObject objReq, bool bAdmin)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblRet = NULL;
	xvalue arrList = NULL;
	xvalue tblMountOptions = NULL;
	CommentRequestContext context;
	CommentMountOptions mountOptions;
	char sMountSlotKey[128];
	int iContextState;
	int iPage = Comment_ReadIntQuery(objReq, "page", 1);
	int iLimit = Comment_ReadIntQuery(objReq, "limit", 100);
	int iOffset;
	const char* sSqlGlobalAdminDesc = "SELECT id, author, body, status, create_time, update_time, content_plugin, content_item_id FROM comment_item WHERE delete_time = 0 ORDER BY create_time DESC, id DESC LIMIT ? OFFSET ?";
	const char* sSqlGlobalPublicDesc = "SELECT id, author, body, status, create_time, update_time, content_plugin, content_item_id FROM comment_item WHERE delete_time = 0 AND status > 0 ORDER BY create_time DESC, id DESC LIMIT ? OFFSET ?";
	const char* sSqlContextAdminDesc = "SELECT id, author, body, status, create_time, update_time, content_plugin, content_item_id FROM comment_item WHERE delete_time = 0 AND content_plugin = ? AND content_item_id = ? ORDER BY create_time DESC, id DESC LIMIT ? OFFSET ?";
	const char* sSqlContextPublicDesc = "SELECT id, author, body, status, create_time, update_time, content_plugin, content_item_id FROM comment_item WHERE delete_time = 0 AND status > 0 AND content_plugin = ? AND content_item_id = ? ORDER BY create_time DESC, id DESC LIMIT ? OFFSET ?";
	const char* sSqlGlobalAdminAsc = "SELECT id, author, body, status, create_time, update_time, content_plugin, content_item_id FROM comment_item WHERE delete_time = 0 ORDER BY create_time ASC, id ASC LIMIT ? OFFSET ?";
	const char* sSqlGlobalPublicAsc = "SELECT id, author, body, status, create_time, update_time, content_plugin, content_item_id FROM comment_item WHERE delete_time = 0 AND status > 0 ORDER BY create_time ASC, id ASC LIMIT ? OFFSET ?";
	const char* sSqlContextAdminAsc = "SELECT id, author, body, status, create_time, update_time, content_plugin, content_item_id FROM comment_item WHERE delete_time = 0 AND content_plugin = ? AND content_item_id = ? ORDER BY create_time ASC, id ASC LIMIT ? OFFSET ?";
	const char* sSqlContextPublicAsc = "SELECT id, author, body, status, create_time, update_time, content_plugin, content_item_id FROM comment_item WHERE delete_time = 0 AND status > 0 AND content_plugin = ? AND content_item_id = ? ORDER BY create_time ASC, id ASC LIMIT ? OFFSET ?";
	const char* sSql;

	if ( iPage < 1 ) iPage = 1;
	if ( iLimit < 1 ) iLimit = 20;
	if ( iLimit > 200 ) iLimit = 200;
	Comment_ContextReset(&context);
	Comment_ContextFromQuery(objReq, &context);
	memset(sMountSlotKey, 0, sizeof(sMountSlotKey));
	Comment_ReadTextQuery(objReq, "mountSlotKey", sMountSlotKey, sizeof(sMountSlotKey));
	tblMountOptions = Comment_ParseMountOptionsQuery(objReq);
	Comment_MountOptionsFromTable(tblMountOptions, &mountOptions);
	if ( (mountOptions.iPageSize > 0) && (iLimit > mountOptions.iPageSize) ) {
		iLimit = mountOptions.iPageSize;
	}
	iOffset = (iPage - 1) * iLimit;
	iContextState = Comment_ContextState(&context);
	if ( !Comment_ValidateContextOrReply(objResp, &context) ) {
		if ( tblMountOptions ) xvoUnref(tblMountOptions);
		return;
	}
	sSql = bAdmin
		? (iContextState == 2
			? (mountOptions.bNewestFirst ? sSqlContextAdminDesc : sSqlContextAdminAsc)
			: (mountOptions.bNewestFirst ? sSqlGlobalAdminDesc : sSqlGlobalAdminAsc))
		: (iContextState == 2
			? (mountOptions.bNewestFirst ? sSqlContextPublicDesc : sSqlContextPublicAsc)
			: (mountOptions.bNewestFirst ? sSqlGlobalPublicDesc : sSqlGlobalPublicAsc));

	if ( !Comment_EnsureSchema() || !Comment_OpenDb(&pDb) ) {
		if ( pDb ) Comment_CloseDb(pDb);
		Comment_SendError(objResp, "failed to open comment database");
		return;
	}

	tblRet = Comment_CreateResult(TRUE, NULL);
	arrList = xvoCreateArray();
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
		if ( iContextState == 2 ) {
			sqlite3_bind_text(stmt, 1, context.sContentPlugin, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 2, context.iContentId);
			sqlite3_bind_int(stmt, 3, iLimit);
			sqlite3_bind_int(stmt, 4, iOffset);
		} else {
			sqlite3_bind_int(stmt, 1, iLimit);
			sqlite3_bind_int(stmt, 2, iOffset);
		}
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			Comment_AppendRow(arrList, stmt);
		}
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Comment_CloseDb(pDb);

	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	xvoTableSetInt(tblRet, "page", 4, iPage);
	xvoTableSetInt(tblRet, "pageSize", 8, iLimit);
	xvoTableSetText(tblRet, "mountSlotKey", 12, (str)sMountSlotKey, 0, FALSE);
	xvoTableSetValue(tblRet, "mountOptions", 12, Comment_BuildMountOptionsValue(&mountOptions), TRUE);
	xvoTableSetText(tblRet, "sortOrder", 9, (str)(mountOptions.bNewestFirst ? "newest" : "oldest"), 0, FALSE);
	xvoTableSetText(tblRet, "contentPlugin", 13, (str)context.sContentPlugin, 0, FALSE);
	xvoTableSetInt(tblRet, "contentId", 9, context.iContentId);
	xvoTableSetBool(tblRet, "contextBound", 12, iContextState == 2);
	if ( tblMountOptions ) xvoUnref(tblMountOptions);
	Comment_SendJsonValue(objResp, tblRet);
}

void Comment_RequestListPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	Comment_RequestListCommon(objResp, objReq, FALSE);
}

void Comment_RequestListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	Comment_RequestListCommon(objResp, objReq, TRUE);
}

void Comment_RequestPost(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblMountOptions = NULL;
	const char* sAuthor;
	const char* sBody;
	CommentRequestContext context;
	CommentMountOptions mountOptions;
	xtime iNow = xrtNow();
	int iStatus;
	int64 iId = 0;
	xvalue tblRet;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !HttpMethodIs(objReq, "POST") ) {
		Comment_SendError(objResp, "method not allowed");
		return;
	}
	tblForm = Comment_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		Comment_SendError(objResp, "invalid json body");
		return;
	}
	sAuthor = xvoTableGetText(tblForm, "author", 6);
	sBody = xvoTableGetText(tblForm, "body", 4);
	Comment_ContextReset(&context);
	Comment_ContextFromBody(tblForm, &context);
	tblMountOptions = Comment_ParseMountOptionsBody(tblForm);
	Comment_MountOptionsFromTable(tblMountOptions, &mountOptions);
	if ( !Comment_ValidateContextOrReply(objResp, &context) ) {
		if ( tblMountOptions ) xvoUnref(tblMountOptions);
		xvoUnref(tblForm);
		return;
	}
	if ( Comment_IsBlank(sBody) ) {
		if ( tblMountOptions ) xvoUnref(tblMountOptions);
		xvoUnref(tblForm);
		Comment_SendError(objResp, "body is required");
		return;
	}
	if ( !mountOptions.bAllowPublicPost ) {
		if ( tblMountOptions ) xvoUnref(tblMountOptions);
		xvoUnref(tblForm);
		Comment_SendError(objResp, "public posting is disabled for this mounted thread");
		return;
	}
	if ( Comment_IsBlank(sAuthor) ) {
		sAuthor = G_CommentConfig.sDefaultAuthor;
	}
	iStatus = G_CommentConfig.bModerationEnabled ? 0 : 1;
	if ( mountOptions.iModerationMode == 1 ) iStatus = 0;
	else if ( mountOptions.iModerationMode == 2 ) iStatus = 1;
	if ( !Comment_EnsureSchema() || !Comment_OpenDb(&pDb) ) {
		if ( pDb ) Comment_CloseDb(pDb);
		if ( tblMountOptions ) xvoUnref(tblMountOptions);
		xvoUnref(tblForm);
		Comment_SendError(objResp, "failed to open comment database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb, "INSERT INTO comment_item(author, body, status, create_time, update_time, delete_time, content_plugin, content_item_id) VALUES(?, ?, ?, ?, ?, 0, ?, ?)", -1, &stmt, NULL) != SQLITE_OK ) {
		Comment_CloseDb(pDb);
		if ( tblMountOptions ) xvoUnref(tblMountOptions);
		xvoUnref(tblForm);
		Comment_SendError(objResp, "failed to prepare insert");
		return;
	}
	sqlite3_bind_text(stmt, 1, sAuthor ? sAuthor : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, sBody ? sBody : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, iStatus);
	sqlite3_bind_int64(stmt, 4, iNow);
	sqlite3_bind_int64(stmt, 5, iNow);
	sqlite3_bind_text(stmt, 6, context.sContentPlugin, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 7, context.iContentId);
	sqlite3_step(stmt);
	iId = sqlite3_last_insert_rowid(pDb);
	sqlite3_finalize(stmt);
	Comment_CloseDb(pDb);
	if ( tblMountOptions ) xvoUnref(tblMountOptions);
	xvoUnref(tblForm);

	tblRet = Comment_CreateResult(TRUE, iStatus > 0 ? "comment published" : "comment submitted for moderation");
	xvoTableSetInt(tblRet, "id", 2, iId);
	xvoTableSetInt(tblRet, "status", 6, iStatus);
	xvoTableSetText(tblRet, "contentPlugin", 13, (str)context.sContentPlugin, 0, FALSE);
	xvoTableSetInt(tblRet, "contentId", 9, context.iContentId);
	Comment_SendJsonValue(objResp, tblRet);
}

void Comment_RequestApprove(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	CommentRequestContext context;
	int64 iId = 0;
	xtime iNow = xrtNow();

	(void)objServer; (void)objHost; (void)objSession;

	if ( !HttpMethodIs(objReq, "POST") ) {
		Comment_SendError(objResp, "method not allowed");
		return;
	}
	tblForm = Comment_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		Comment_SendError(objResp, "invalid json body");
		return;
	}
	iId = xvoTableGetInt(tblForm, "id", 2);
	Comment_ContextReset(&context);
	Comment_ContextFromBody(tblForm, &context);
	xvoUnref(tblForm);
	if ( !Comment_ValidateContextOrReply(objResp, &context) ) return;
	if ( iId <= 0 ) {
		Comment_SendError(objResp, "id is required");
		return;
	}
	if ( !Comment_EnsureSchema() || !Comment_OpenDb(&pDb) ) {
		if ( pDb ) Comment_CloseDb(pDb);
		Comment_SendError(objResp, "failed to open comment database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb,
		Comment_ContextState(&context) == 2
			? "UPDATE comment_item SET status = 1, update_time = ? WHERE id = ? AND delete_time = 0 AND content_plugin = ? AND content_item_id = ?"
			: "UPDATE comment_item SET status = 1, update_time = ? WHERE id = ? AND delete_time = 0",
		-1,
		&stmt,
		NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, iNow);
		sqlite3_bind_int64(stmt, 2, iId);
		if ( Comment_ContextState(&context) == 2 ) {
			sqlite3_bind_text(stmt, 3, context.sContentPlugin, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 4, context.iContentId);
		}
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Comment_CloseDb(pDb);
	Comment_SendJsonValue(objResp, Comment_CreateResult(TRUE, "comment approved"));
}

void Comment_RequestDelete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm = NULL;
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	CommentRequestContext context;
	int64 iId = 0;
	xtime iNow = xrtNow();

	(void)objServer; (void)objHost; (void)objSession;

	if ( !HttpMethodIs(objReq, "POST") ) {
		Comment_SendError(objResp, "method not allowed");
		return;
	}
	tblForm = Comment_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		Comment_SendError(objResp, "invalid json body");
		return;
	}
	iId = xvoTableGetInt(tblForm, "id", 2);
	Comment_ContextReset(&context);
	Comment_ContextFromBody(tblForm, &context);
	xvoUnref(tblForm);
	if ( !Comment_ValidateContextOrReply(objResp, &context) ) return;
	if ( iId <= 0 ) {
		Comment_SendError(objResp, "id is required");
		return;
	}
	if ( !Comment_EnsureSchema() || !Comment_OpenDb(&pDb) ) {
		if ( pDb ) Comment_CloseDb(pDb);
		Comment_SendError(objResp, "failed to open comment database");
		return;
	}
	if ( sqlite3_prepare_v2(pDb,
		Comment_ContextState(&context) == 2
			? "UPDATE comment_item SET delete_time = ?, update_time = ? WHERE id = ? AND delete_time = 0 AND content_plugin = ? AND content_item_id = ?"
			: "UPDATE comment_item SET delete_time = ?, update_time = ? WHERE id = ? AND delete_time = 0",
		-1,
		&stmt,
		NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, iNow);
		sqlite3_bind_int64(stmt, 2, iNow);
		sqlite3_bind_int64(stmt, 3, iId);
		if ( Comment_ContextState(&context) == 2 ) {
			sqlite3_bind_text(stmt, 4, context.sContentPlugin, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 5, context.iContentId);
		}
		sqlite3_step(stmt);
	}
	if ( stmt ) sqlite3_finalize(stmt);
	Comment_CloseDb(pDb);
	Comment_SendJsonValue(objResp, Comment_CreateResult(TRUE, "comment deleted"));
}

void Comment_RequestPublicView(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Comment_SendAssetHtml(objResp, "public.html") ) {
		http_reply(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "comment public page missing", 0);
	}
}

void Comment_RequestAdminView(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Comment_SendAssetHtml(objResp, "admin.html") ) {
		http_reply(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "comment admin page missing", 0);
	}
}

int Comment_OnLoad(XAdminPluginHandle* out_handle)
{
	if ( out_handle ) G_CommentHandle = *out_handle;
	return 0;
}

int Comment_OnStart(XAdminPluginHandle handle)
{
	XAdminRouteDecl route;
	XAdminMenuDecl menu;

	G_CommentHandle = handle;
	if ( !Comment_EnsureSchema() ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/api/plugin/comment-system/meta";
	route.proc = Comment_RequestMetaPublic;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/api/plugin/comment-system/list";
	route.proc = Comment_RequestListPublic;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/api/plugin/comment-system/post";
	route.proc = Comment_RequestPost;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/plugin/comment-system";
	route.proc = Comment_RequestPublicView;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/comment-system/meta";
	route.proc = Comment_RequestMetaAdmin;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/comment-system/list";
	route.proc = Comment_RequestListAdmin;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/comment-system/approve";
	route.proc = Comment_RequestApprove;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/comment-system/delete";
	route.proc = Comment_RequestDelete;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/comment-system";
	route.proc = Comment_RequestAdminView;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&menu, 0, sizeof(menu));
	menu.title = "Comments";
	menu.icon = "layui-icon layui-icon-dialogue";
	menu.type = 1;
	menu.open_type = "_iframe";
	menu.href = "/admin/view/plugin/comment-system";
	menu.sort = 990080;
	menu.visible = TRUE;
	menu.remark = "Comment capability provider";
	if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) return -1;

	return 0;
}

int Comment_OnConfigChanged(XAdminPluginHandle handle, xvalue new_cfg)
{
	const char* sPublicTitle;
	const char* sDefaultAuthor;

	(void)handle;
	Comment_ConfigReset();
	if ( (new_cfg == NULL) || (xvoType(new_cfg) != XVO_DT_TABLE) ) return 0;

	sPublicTitle = xvoTableGetText(new_cfg, "publicTitle", 11);
	sDefaultAuthor = xvoTableGetText(new_cfg, "defaultAuthor", 13);
	if ( !Comment_IsBlank(sPublicTitle) ) snprintf(G_CommentConfig.sPublicTitle, sizeof(G_CommentConfig.sPublicTitle), "%s", sPublicTitle);
	if ( !Comment_IsBlank(sDefaultAuthor) ) snprintf(G_CommentConfig.sDefaultAuthor, sizeof(G_CommentConfig.sDefaultAuthor), "%s", sDefaultAuthor);
	G_CommentConfig.bModerationEnabled = xvoTableGetBool(new_cfg, "moderationEnabled", 17);
	return 0;
}

void Comment_OnStop(XAdminPluginHandle handle)
{
	(void)handle;
}

void Comment_OnUnload(XAdminPluginHandle handle)
{
	(void)handle;
}

static XAdminPluginDescriptor G_CommentPlugin = {
	XADMIN_ABI_VERSION,
	sizeof(XAdminPluginDescriptor),
	"comment-system",
	"0.1.0",
	"Comment System",
	Comment_OnLoad,
	NULL,
	Comment_OnStart,
	Comment_OnConfigChanged,
	NULL,
	Comment_OnStop,
	Comment_OnUnload
};

XADMIN_DECLARE_PLUGIN(G_CommentPlugin)
