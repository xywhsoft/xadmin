#include "xs_plugin.h"

#define XLOG_DEFAULT_MAX_LINES 10000
#define XLOG_DEFAULT_REFRESH_MS 3000
#define XLOG_LOG_DB_DIR "logs"

typedef struct {
	int iMaxLogLines;
	int iDefaultRefreshInterval;
} XLogConfigState;

static XAdminPluginHandle G_XLogHandle = NULL;
static const XAdminHostContext* G_XLogHost = NULL;
static const char* G_XLogXid = NULL;
static const char* G_XLogRootPath = NULL;
static const char* G_XLogDataPath = NULL;
static const char* G_XLogPrivateDbPath = NULL;
static sqlite3* G_XLogMainDb = NULL;
static xdict G_XLogServiceDbCache = NULL;
static xmutex G_XLogServiceDbLock = NULL;
static XLogConfigState G_XLogConfig = { XLOG_DEFAULT_MAX_LINES, XLOG_DEFAULT_REFRESH_MS };

static const char* G_XLogMainSchemaSql =
	"CREATE TABLE IF NOT EXISTS services ("
	"id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"name TEXT NOT NULL,"
	"desc TEXT DEFAULT '',"
	"db TEXT NOT NULL,"
	"createTime INTEGER NOT NULL DEFAULT 0,"
	"updateTime INTEGER NOT NULL DEFAULT 0,"
	"isDelete INTEGER NOT NULL DEFAULT 0"
	");"
	"CREATE INDEX IF NOT EXISTS idx_xlog_services_delete_id ON services(isDelete, id DESC);";

static const char* G_XLogTaskSchemaSql =
	"CREATE TABLE IF NOT EXISTS tasks ("
	"id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"name TEXT NOT NULL,"
	"desc TEXT DEFAULT '',"
	"tableName TEXT NOT NULL,"
	"createTime INTEGER NOT NULL DEFAULT 0,"
	"updateTime INTEGER NOT NULL DEFAULT 0,"
	"isDelete INTEGER NOT NULL DEFAULT 0"
	");"
	"CREATE INDEX IF NOT EXISTS idx_xlog_tasks_delete_id ON tasks(isDelete, id DESC);";

XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int idx, void* ptr)
{
	if ( idx == XADMIN_GLOBAL_HOST_CONTEXT ) {
		G_XLogHost = (const XAdminHostContext*)ptr;
		if ( G_XLogHost && G_XLogHost->size >= sizeof(XAdminHostContext) ) {
			G_XLogMainDb = G_XLogHost->main_db;
			G_XLogXid = G_XLogHost->plugin_xid;
			G_XLogRootPath = G_XLogHost->plugin_root_path;
			G_XLogDataPath = G_XLogHost->plugin_data_path;
			G_XLogPrivateDbPath = G_XLogHost->plugin_private_db_path;
		}
	} else if ( idx == XADMIN_GLOBAL_MAIN_DB ) {
		G_XLogMainDb = (sqlite3*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_XID ) {
		G_XLogXid = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_ROOT_PATH ) {
		G_XLogRootPath = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_DATA_PATH ) {
		G_XLogDataPath = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_PRIVATE_DB_PATH ) {
		G_XLogPrivateDbPath = (const char*)ptr;
	}
}

void XLog_ConfigReset(void)
{
	G_XLogConfig.iMaxLogLines = XLOG_DEFAULT_MAX_LINES;
	G_XLogConfig.iDefaultRefreshInterval = XLOG_DEFAULT_REFRESH_MS;
}

int XLog_ClampInt(int iValue, int iMin, int iMax, int iDefault)
{
	if ( iValue < iMin ) return iDefault;
	if ( iValue > iMax ) return iMax;
	return iValue;
}

bool XLog_IsBlank(const char* sText)
{
	const unsigned char* p = (const unsigned char*)sText;
	if ( p == NULL ) return TRUE;
	while ( *p ) {
		if ( (*p != ' ') && (*p != '\t') && (*p != '\r') && (*p != '\n') ) return FALSE;
		p++;
	}
	return TRUE;
}

bool XLog_TableHasId(xvalue tblData)
{
	return tblData && (xvoType(tblData) == XVO_DT_TABLE) && (xvoTableGetInt(tblData, "id", 2) > 0);
}

int XLog_ReadIntQuery(XS_RequestObject objReq, const char* sName, int iDefault)
{
	char sBuf[48];
	memset(sBuf, 0, sizeof(sBuf));
	xsReqQueryValue(objReq, sName, sBuf, sizeof(sBuf));
	if ( sBuf[0] == '\0' ) return iDefault;
	return atoi(sBuf);
}

int64 XLog_ReadInt64Query(XS_RequestObject objReq, const char* sName, int64 iDefault)
{
	char sBuf[64];
	memset(sBuf, 0, sizeof(sBuf));
	xsReqQueryValue(objReq, sName, sBuf, sizeof(sBuf));
	if ( sBuf[0] == '\0' ) return iDefault;
	return xrtStrToI64(sBuf);
}

int64 XLog_ReadServiceIdQuery(XS_RequestObject objReq)
{
	int64 serviceId = XLog_ReadInt64Query(objReq, "serviceId", 0);
	if ( serviceId <= 0 ) serviceId = XLog_ReadInt64Query(objReq, "id", 0);
	return serviceId;
}

str XLog_ReadQuery(XS_RequestObject objReq, const char* sName)
{
	char sBuf[512];
	memset(sBuf, 0, sizeof(sBuf));
	xsReqQueryValue(objReq, sName, sBuf, sizeof(sBuf));
	if ( sBuf[0] == '\0' ) return NULL;
	return xrtCopyStr(sBuf, 0);
}

xvalue XLog_ParseJsonBody(XS_RequestObject objReq)
{
	xvalue tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( (tblBody == NULL) || (xvoType(tblBody) != XVO_DT_TABLE) ) {
		if ( tblBody ) xvoUnref(tblBody);
		return NULL;
	}
	return tblBody;
}

void XLog_SendJsonValue(XS_ResponseObject objResp, xvalue tblData)
{
	if ( tblData == NULL ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: application/json\r\n", "{\"result\":false,\"message\":\"json allocation failed\"}", 0);
		return;
	}
	if ( XAdmin_ReplyJson(objResp, 200, tblData) != 0 ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: application/json\r\n", "{\"result\":false,\"message\":\"json encode failed\"}", 0);
	}
	xvoUnref(tblData);
}

xvalue XLog_NewResult(bool bResult, const char* sMessage)
{
	xvalue tblRet = xvoCreateTable();
	if ( tblRet == NULL ) return NULL;
	xvoTableSetBool(tblRet, "result", 6, bResult);
	xvoTableSetInt(tblRet, "code", 4, bResult ? 0 : 1);
	if ( sMessage ) xvoTableSetText(tblRet, "message", 7, (str)sMessage, 0, FALSE);
	return tblRet;
}

void XLog_SendError(XS_ResponseObject objResp, const char* sMessage)
{
	XLog_SendJsonValue(objResp, XLog_NewResult(FALSE, sMessage ? sMessage : "failed"));
}

void XLog_SendOk(XS_ResponseObject objResp, const char* sMessage)
{
	XLog_SendJsonValue(objResp, XLog_NewResult(TRUE, sMessage ? sMessage : "success"));
}

void XLog_ReplyApiCode(XS_ResponseObject objResp, int iCode, const char* sMessage)
{
	xvalue tblRet = xvoCreateTable();
	xvoTableSetInt(tblRet, "code", 4, iCode);
	xvoTableSetBool(tblRet, "result", 6, iCode == 0);
	xvoTableSetText(tblRet, "message", 7, (str)(sMessage ? sMessage : (iCode == 0 ? "success" : "failed")), 0, FALSE);
	XLog_SendJsonValue(objResp, tblRet);
}

void XLog_ReplyApiData(XS_ResponseObject objResp, xvalue tblData)
{
	xvalue tblRet = xvoCreateTable();
	xvoTableSetInt(tblRet, "code", 4, 0);
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetText(tblRet, "message", 7, (str)"success", 0, FALSE);
	if ( tblData ) xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	XLog_SendJsonValue(objResp, tblRet);
}

bool XLog_OpenMainDb(sqlite3** ppDb)
{
	sqlite3* pDb = NULL;
	int iRet;

	if ( ppDb ) *ppDb = NULL;
	if ( ppDb == NULL ) return FALSE;

	if ( G_XLogHandle && XAdmin_OpenPluginPrivateDb(G_XLogHandle, &pDb) == 0 && pDb ) {
		sqlite3_busy_timeout(pDb, 3000);
		*ppDb = pDb;
		return TRUE;
	}
	if ( (G_XLogPrivateDbPath == NULL) || (G_XLogPrivateDbPath[0] == '\0') ) return FALSE;

	iRet = sqlite3_open_v2(G_XLogPrivateDbPath, &pDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
	if ( iRet != SQLITE_OK ) {
		if ( pDb ) sqlite3_close(pDb);
		return FALSE;
	}
	sqlite3_busy_timeout(pDb, 3000);
	*ppDb = pDb;
	return TRUE;
}

void XLog_CloseDb(sqlite3* pDb)
{
	if ( pDb ) sqlite3_close(pDb);
}

bool XLog_Exec(sqlite3* pDb, const char* sSql)
{
	char* sError = NULL;
	bool bOK;
	if ( (pDb == NULL) || (sSql == NULL) ) return FALSE;
	bOK = sqlite3_exec(pDb, sSql, NULL, NULL, &sError) == SQLITE_OK;
	if ( sError ) sqlite3_free(sError);
	return bOK;
}

bool XLog_EnsureMainSchema(void)
{
	sqlite3* pDb = NULL;
	bool bOK;
	if ( !XLog_OpenMainDb(&pDb) ) return FALSE;
	bOK = XLog_Exec(pDb, G_XLogMainSchemaSql);
	XLog_CloseDb(pDb);
	return bOK;
}

str XLog_GetLogsDir(void)
{
	str sDir;
	if ( (G_XLogDataPath == NULL) || (G_XLogDataPath[0] == '\0') ) return NULL;
	sDir = xrtPathJoin(2, (str)G_XLogDataPath, (str)XLOG_LOG_DB_DIR);
	if ( sDir && !xrtDirExists(sDir) ) {
		xrtDirCreate(sDir);
	}
	return sDir;
}

bool XLog_IsSafeDbName(const char* sDbName)
{
	if ( XLog_IsBlank(sDbName) ) return FALSE;
	if ( strstr(sDbName, "..") != NULL ) return FALSE;
	if ( strchr(sDbName, '/') || strchr(sDbName, '\\') || strchr(sDbName, ':') ) return FALSE;
	return TRUE;
}

bool XLog_IsSafeLogTableName(const char* sTableName)
{
	const char* p;
	if ( (sTableName == NULL) || (strncmp(sTableName, "log_", 4) != 0) ) return FALSE;
	p = sTableName + 4;
	if ( *p == '\0' ) return FALSE;
	while ( *p ) {
		if ( (*p < '0') || (*p > '9') ) return FALSE;
		p++;
	}
	return TRUE;
}

sqlite3* XLog_ServiceConnectDB(int64 serviceId, const char* sDbName)
{
	str sDir;
	str sPath;
	sqlite3* pDb = NULL;
	int iRet;

	if ( (serviceId <= 0) || !XLog_IsSafeDbName(sDbName) ) return NULL;
	sDir = XLog_GetLogsDir();
	if ( sDir == NULL ) return NULL;
	sPath = xrtPathJoin(2, sDir, (str)sDbName);
	xrtFree(sDir);
	if ( sPath == NULL ) return NULL;

	iRet = sqlite3_open_v2(sPath, &pDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
	xrtFree(sPath);
	if ( iRet != SQLITE_OK ) {
		if ( pDb ) sqlite3_close(pDb);
		return NULL;
	}
	sqlite3_busy_timeout(pDb, 3000);
	if ( !XLog_Exec(pDb, G_XLogTaskSchemaSql) ) {
		sqlite3_close(pDb);
		return NULL;
	}

	if ( G_XLogServiceDbLock ) xrtMutexLock(G_XLogServiceDbLock);
	if ( G_XLogServiceDbCache ) {
		sqlite3* pOld = (sqlite3*)xrtDictGetPtr(G_XLogServiceDbCache, (str)&serviceId, sizeof(int64));
		if ( pOld && (pOld != pDb) ) sqlite3_close(pOld);
		xrtDictSetPtr(G_XLogServiceDbCache, (str)&serviceId, sizeof(int64), pDb, NULL);
	}
	if ( G_XLogServiceDbLock ) xrtMutexUnlock(G_XLogServiceDbLock);
	return pDb;
}

sqlite3* XLog_ServiceGetDB(int64 serviceId)
{
	sqlite3* pDb = NULL;
	if ( serviceId <= 0 ) return NULL;
	if ( G_XLogServiceDbLock ) xrtMutexLock(G_XLogServiceDbLock);
	if ( G_XLogServiceDbCache ) {
		pDb = (sqlite3*)xrtDictGetPtr(G_XLogServiceDbCache, (str)&serviceId, sizeof(int64));
	}
	if ( G_XLogServiceDbLock ) xrtMutexUnlock(G_XLogServiceDbLock);
	return pDb;
}

void XLog_ServiceDisconnectDB(int64 serviceId)
{
	if ( serviceId <= 0 ) return;
	if ( G_XLogServiceDbLock ) xrtMutexLock(G_XLogServiceDbLock);
	if ( G_XLogServiceDbCache ) {
		sqlite3* pDb = (sqlite3*)xrtDictRemovePtr(G_XLogServiceDbCache, (str)&serviceId, sizeof(int64));
		if ( pDb ) sqlite3_close(pDb);
	}
	if ( G_XLogServiceDbLock ) xrtMutexUnlock(G_XLogServiceDbLock);
}

static bool XLog_DisconnectAllProc(Dict_Key* pKey, sqlite3** ppDb, void* pArg)
{
	(void)pKey;
	(void)pArg;
	if ( ppDb && *ppDb ) {
		sqlite3_close(*ppDb);
		*ppDb = NULL;
	}
	return FALSE;
}

void XLog_ServiceDisconnectAll(void)
{
	if ( G_XLogServiceDbLock ) xrtMutexLock(G_XLogServiceDbLock);
	if ( G_XLogServiceDbCache ) xrtDictWalk(G_XLogServiceDbCache, (Dict_EachProc)XLog_DisconnectAllProc, NULL);
	if ( G_XLogServiceDbLock ) xrtMutexUnlock(G_XLogServiceDbLock);
}

void XLog_ServiceLoadAllDB(void)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	if ( !XLog_OpenMainDb(&pDb) ) return;
	if ( sqlite3_prepare_v2(pDb, "SELECT id, db FROM services WHERE isDelete = 0 ORDER BY id ASC;", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			int64 serviceId = sqlite3_column_int64(stmt, 0);
			const char* sDbName = (const char*)sqlite3_column_text(stmt, 1);
			XLog_ServiceConnectDB(serviceId, sDbName);
		}
		sqlite3_finalize(stmt);
	}
	XLog_CloseDb(pDb);
}

xvalue XLog_ServiceGetOne(sqlite3* pDb, int64 serviceId)
{
	sqlite3_stmt* stmt = NULL;
	xvalue tblInfo = xvoCreateTable();
	if ( (pDb == NULL) || (tblInfo == NULL) ) return tblInfo;
	if ( sqlite3_prepare_v2(pDb, "SELECT id, name, desc, db FROM services WHERE id = ? AND isDelete = 0 LIMIT 1;", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, serviceId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvoTableSetInt(tblInfo, "id", 2, sqlite3_column_int64(stmt, 0));
			xvoTableSetText(tblInfo, "name", 4, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
			xvoTableSetText(tblInfo, "desc", 4, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
			xvoTableSetText(tblInfo, "db", 2, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
		}
		sqlite3_finalize(stmt);
	}
	return tblInfo;
}

xvalue XLog_TaskGetOne(sqlite3* pDb, int64 taskId)
{
	sqlite3_stmt* stmt = NULL;
	xvalue tblInfo = xvoCreateTable();
	if ( (pDb == NULL) || (tblInfo == NULL) ) return tblInfo;
	if ( sqlite3_prepare_v2(pDb, "SELECT id, name, desc, tableName FROM tasks WHERE id = ? AND isDelete = 0 LIMIT 1;", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, taskId);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvoTableSetInt(tblInfo, "id", 2, sqlite3_column_int64(stmt, 0));
			xvoTableSetText(tblInfo, "name", 4, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
			xvoTableSetText(tblInfo, "desc", 4, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
			xvoTableSetText(tblInfo, "tableName", 9, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
		}
		sqlite3_finalize(stmt);
	}
	return tblInfo;
}

int64 XLog_TaskCreate(sqlite3* pDb, const char* sName, const char* sDesc)
{
	sqlite3_stmt* stmt = NULL;
	int64 iNow = xrtNow();
	int64 iTaskId = 0;
	str sTableName = NULL;
	str sSql = NULL;

	if ( pDb == NULL ) return 0;
	if ( XLog_IsBlank(sName) ) sName = "Unnamed Task";
	if ( sDesc == NULL ) sDesc = "";

	if ( sqlite3_prepare_v2(pDb, "INSERT INTO tasks (name, desc, tableName, createTime, updateTime, isDelete) VALUES (?, ?, '', ?, ?, 0);", -1, &stmt, NULL) != SQLITE_OK ) {
		if ( G_XLogHandle ) XAdmin_Log(G_XLogHandle, LOG_ERROR, sqlite3_errmsg(pDb));
		return 0;
	}
	sqlite3_bind_text(stmt, 1, sName, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, sDesc, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 3, iNow);
	sqlite3_bind_int64(stmt, 4, iNow);
	if ( sqlite3_step(stmt) == SQLITE_DONE ) {
		iTaskId = sqlite3_last_insert_rowid(pDb);
	} else if ( G_XLogHandle ) {
		XAdmin_Log(G_XLogHandle, LOG_ERROR, sqlite3_errmsg(pDb));
	}
	sqlite3_finalize(stmt);
	if ( iTaskId <= 0 ) return 0;

	sTableName = xrtFormat("log_%lld", (long long)iTaskId);
	if ( sqlite3_prepare_v2(pDb, "UPDATE tasks SET tableName = ? WHERE id = ?;", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sTableName, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, iTaskId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	sSql = xrtFormat("CREATE TABLE IF NOT EXISTS %s (id INTEGER PRIMARY KEY AUTOINCREMENT, time INTEGER NOT NULL, class TEXT DEFAULT 'info', text TEXT DEFAULT '');CREATE INDEX IF NOT EXISTS idx_%s_id ON %s(id);", sTableName, sTableName, sTableName);
	XLog_Exec(pDb, sSql);
	xrtFree(sSql);
	xrtFree(sTableName);
	return iTaskId;
}

void XLog_LogTrim(sqlite3* pDb, const char* sTableName, int64 iNewestId)
{
	sqlite3_stmt* stmt = NULL;
	str sSql;
	if ( (pDb == NULL) || !XLog_IsSafeLogTableName(sTableName) || (G_XLogConfig.iMaxLogLines <= 0) ) return;
	if ( iNewestId <= 0 || (iNewestId % 100) != 0 ) return;
	sSql = xrtFormat("DELETE FROM %s WHERE id NOT IN (SELECT id FROM %s ORDER BY id DESC LIMIT ?);", sTableName, sTableName);
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, G_XLogConfig.iMaxLogLines);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	xrtFree(sSql);
}

int64 XLog_LogAdd(sqlite3* pDb, const char* sTableName, const char* sClass, const char* sText)
{
	sqlite3_stmt* stmt = NULL;
	str sSql;
	int64 iLogId = 0;
	if ( (pDb == NULL) || !XLog_IsSafeLogTableName(sTableName) ) return 0;
	if ( XLog_IsBlank(sClass) ) sClass = "info";
	if ( sText == NULL ) sText = "";

	sSql = xrtFormat("INSERT INTO %s (time, class, text) VALUES (?, ?, ?);", sTableName);
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, xrtNow());
		sqlite3_bind_text(stmt, 2, sClass, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, sText, -1, SQLITE_TRANSIENT);
		if ( sqlite3_step(stmt) == SQLITE_DONE ) iLogId = sqlite3_last_insert_rowid(pDb);
		sqlite3_finalize(stmt);
	}
	xrtFree(sSql);
	XLog_LogTrim(pDb, sTableName, iLogId);
	return iLogId;
}

void XLog_SendPage(XS_ResponseObject objResp, const char* sPage)
{
	str sPath;
	ptr pData;
	size_t iSize = 0;

	if ( (G_XLogRootPath == NULL) || (sPage == NULL) || (sPage[0] == '\0') ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "page not found", 0);
		return;
	}
	sPath = xrtPathJoin(3, (str)G_XLogRootPath, (str)"page", (str)sPage);
	if ( (sPath == NULL) || !xrtFileExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "page not found", 0);
		return;
	}
	pData = xrtFileGetAll(sPath, &iSize);
	xrtFree(sPath);
	if ( pData == NULL ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "page not found", 0);
		return;
	}
	xsHttpReplyAuto(objResp, 200, "Content-Type: text/html; charset=utf-8\r\n", pData, iSize);
	xrtFree(pData);
}

str XLog_StringReplaceAll(const char* sText, const char* sNeedle, const char* sValue)
{
	xbuffer_struct tBuf;
	const char* p;
	const char* sCursor;
	size_t iNeedleLen;
	char chZero = 0;

	if ( sText == NULL ) return NULL;
	if ( (sNeedle == NULL) || (sNeedle[0] == '\0') ) return xrtCopyStr((str)sText, 0);
	if ( sValue == NULL ) sValue = "";

	iNeedleLen = strlen(sNeedle);
	xrtBufferInit(&tBuf, (uint32)(strlen(sText) + 32));
	sCursor = sText;
	while ( (p = strstr(sCursor, sNeedle)) != NULL ) {
		if ( p > sCursor ) xrtBufferAppend(&tBuf, (ptr)sCursor, (uint32)(p - sCursor), XBUF_BINARY);
		xrtBufferAppend(&tBuf, (ptr)sValue, (uint32)strlen(sValue), XBUF_BINARY);
		sCursor = p + iNeedleLen;
	}
	if ( *sCursor ) xrtBufferAppend(&tBuf, (ptr)sCursor, (uint32)strlen(sCursor), XBUF_BINARY);
	xrtBufferAppend(&tBuf, &chZero, 1, XBUF_BINARY);
	return (str)tBuf.Buffer;
}

str XLog_EscapeHtml(const char* sText)
{
	xbuffer_struct tBuf;
	char chZero = 0;

	if ( sText == NULL ) return xrtCopyStr("", 0);
	xrtBufferInit(&tBuf, (uint32)(strlen(sText) + 16));
	for ( const char* p = sText; *p; p++ ) {
		char c = *p;
		if ( c == '&' ) xrtBufferAppend(&tBuf, "&amp;", 5, XBUF_BINARY);
		else if ( c == '<' ) xrtBufferAppend(&tBuf, "&lt;", 4, XBUF_BINARY);
		else if ( c == '>' ) xrtBufferAppend(&tBuf, "&gt;", 4, XBUF_BINARY);
		else if ( c == '"' ) xrtBufferAppend(&tBuf, "&quot;", 6, XBUF_BINARY);
		else if ( c == '\'' ) xrtBufferAppend(&tBuf, "&#39;", 5, XBUF_BINARY);
		else xrtBufferAppend(&tBuf, &c, 1, XBUF_BINARY);
	}
	xrtBufferAppend(&tBuf, &chZero, 1, XBUF_BINARY);
	return (str)tBuf.Buffer;
}

void XLog_TemplateSetText(str* psHtml, const char* sKey, const char* sValue)
{
	str sNeedle;
	str sSafe;
	str sNext;

	if ( (psHtml == NULL) || (*psHtml == NULL) || (sKey == NULL) ) return;
	sNeedle = xrtFormat("{$%s}", sKey);
	sSafe = XLog_EscapeHtml(sValue ? sValue : "");
	sNext = XLog_StringReplaceAll((const char*)*psHtml, (const char*)sNeedle, (const char*)sSafe);
	if ( sNeedle ) xrtFree(sNeedle);
	if ( sSafe ) xrtFree(sSafe);
	if ( sNext ) {
		xrtFree(*psHtml);
		*psHtml = sNext;
	}
}

void XLog_TemplateSetInt(str* psHtml, const char* sKey, int64 iValue)
{
	char sBuf[64];
	snprintf(sBuf, sizeof(sBuf), "%lld", (long long)iValue);
	XLog_TemplateSetText(psHtml, sKey, sBuf);
}

str XLog_FindTemplatePath(const char* sTemplate)
{
	str sPath;
	str sRelPath;

	if ( (sTemplate == NULL) || (sTemplate[0] == '\0') ) return NULL;
	if ( G_XLogRootPath ) {
		sPath = xrtPathJoin(3, (str)G_XLogRootPath, (str)"template", (str)sTemplate);
		if ( sPath && xrtFileExists(sPath) ) return sPath;
		if ( sPath ) xrtFree(sPath);

		sRelPath = xrtFormat("template/%s", sTemplate);
		if ( sRelPath ) {
			sPath = xrtPathJoin(2, (str)G_XLogRootPath, sRelPath);
			xrtFree(sRelPath);
			if ( sPath && xrtFileExists(sPath) ) return sPath;
			if ( sPath ) xrtFree(sPath);
		}
	}

	if ( G_XLogHost && G_XLogHost->app_path ) {
		sPath = xrtPathJoin(5, (str)G_XLogHost->app_path, (str)"plugin", (str)"xlogserver", (str)"template", (str)sTemplate);
		if ( sPath && xrtFileExists(sPath) ) return sPath;
		if ( sPath ) xrtFree(sPath);
	}
	return NULL;
}

void XLog_SendTemplate(XS_ResponseObject objResp, const char* sTemplate, xvalue tblData)
{
	str sPath;
	str sHtml;
	str sNext;
	str sError;
	size_t iSize = 0;

	if ( (sTemplate == NULL) || (sTemplate[0] == '\0') ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "template name required", 0);
		return;
	}
	sPath = XLog_FindTemplatePath(sTemplate);
	if ( sPath == NULL ) {
		sError = xrtFormat("template file not found: %s root=%s app=%s", sTemplate, G_XLogRootPath ? G_XLogRootPath : "(null)", (G_XLogHost && G_XLogHost->app_path) ? G_XLogHost->app_path : "(null)");
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", sError ? sError : (str)"template file not found", 0);
		if ( sError ) xrtFree(sError);
		return;
	}
	sHtml = xrtFileGetAll(sPath, &iSize);
	xrtFree(sPath);
	if ( sHtml == NULL ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "template file read failed", 0);
		return;
	}

	sNext = XLog_StringReplaceAll((const char*)sHtml, "{{", "{");
	if ( sNext ) {
		xrtFree(sHtml);
		sHtml = sNext;
	}
	sNext = XLog_StringReplaceAll((const char*)sHtml, "}}", "}");
	if ( sNext ) {
		xrtFree(sHtml);
		sHtml = sNext;
	}

	if ( tblData && (xvoType(tblData) == XVO_DT_TABLE) ) {
		XLog_TemplateSetInt(&sHtml, "id", xvoTableGetInt(tblData, "id", 2));
		XLog_TemplateSetInt(&sHtml, "serviceId", xvoTableGetInt(tblData, "serviceId", 9));
		XLog_TemplateSetInt(&sHtml, "taskId", xvoTableGetInt(tblData, "taskId", 6));
		XLog_TemplateSetInt(&sHtml, "refreshInterval", xvoTableGetInt(tblData, "refreshInterval", 15));
		XLog_TemplateSetText(&sHtml, "name", (const char*)xvoTableGetText(tblData, "name", 4));
		XLog_TemplateSetText(&sHtml, "desc", (const char*)xvoTableGetText(tblData, "desc", 4));
		XLog_TemplateSetText(&sHtml, "serviceName", (const char*)xvoTableGetText(tblData, "serviceName", 11));
		XLog_TemplateSetText(&sHtml, "tableName", (const char*)xvoTableGetText(tblData, "tableName", 9));
	}

	xsHttpReplyAuto(objResp, 200, "Content-Type: text/html; charset=utf-8\r\n", sHtml, strlen((const char*)sHtml));
	xrtFree(sHtml);
}

void XLog_PageServices(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	XLog_SendPage(objResp, "services.html");
}

void XLog_PageServiceAdd(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	XLog_SendPage(objResp, "services_add.html");
}

void XLog_PageServiceEdit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	xvalue tblInfo;
	int64 serviceId;
	(void)objServer; (void)objHost; (void)objSession;
	if ( xsReqMethodID(objReq) != XHTTPD_METHOD_GET ) { xsHttpReplyAuto(objResp, 405, "Content-Type: text/plain\r\n", "method not allowed", 0); return; }
	serviceId = XLog_ReadInt64Query(objReq, "id", 0);
	if ( serviceId <= 0 || !XLog_OpenMainDb(&pDb) ) { xsHttpReplyAuto(objResp, 400, "Content-Type: text/plain\r\n", "invalid service", 0); return; }
	tblInfo = XLog_ServiceGetOne(pDb, serviceId);
	XLog_CloseDb(pDb);
	if ( !XLog_TableHasId(tblInfo) ) {
		xvoUnref(tblInfo);
		xsHttpReplyAuto(objResp, 404, "Content-Type: text/plain; charset=utf-8\r\n", "service not found", 0);
		return;
	}
	XLog_SendTemplate(objResp, "services_edit.html", tblInfo);
	xvoUnref(tblInfo);
}

void XLog_PageTasks(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	xvalue tblData;
	xvalue tblSvc;
	char sServiceName[256] = {0};
	str sName;
	int64 serviceId;
	(void)objServer; (void)objHost; (void)objSession;
	if ( xsReqMethodID(objReq) != XHTTPD_METHOD_GET ) { xsHttpReplyAuto(objResp, 405, "Content-Type: text/plain\r\n", "method not allowed", 0); return; }
	serviceId = XLog_ReadServiceIdQuery(objReq);
	if ( serviceId <= 0 || XLog_ServiceGetDB(serviceId) == NULL ) { xsHttpReplyAuto(objResp, 404, "Content-Type: text/plain\r\n", "service not found", 0); return; }
	if ( XLog_OpenMainDb(&pDb) ) {
		tblSvc = XLog_ServiceGetOne(pDb, serviceId);
		if ( !XLog_TableHasId(tblSvc) ) {
			xvoUnref(tblSvc);
			XLog_CloseDb(pDb);
			xsHttpReplyAuto(objResp, 404, "Content-Type: text/plain; charset=utf-8\r\n", "service not found", 0);
			return;
		}
		sName = xvoTableGetText(tblSvc, "name", 4);
		if ( sName && sName[0] ) {
			snprintf(sServiceName, sizeof(sServiceName), "%s", sName);
		} else {
			snprintf(sServiceName, sizeof(sServiceName), "%s", "Unknown");
		}
		xvoUnref(tblSvc);
		XLog_CloseDb(pDb);
	}
	tblData = xvoCreateTable();
	xvoTableSetInt(tblData, "serviceId", 9, serviceId);
	xvoTableSetText(tblData, "serviceName", 11, (str)sServiceName, 0, FALSE);
	XLog_SendTemplate(objResp, "tasks_index.html", tblData);
	xvoUnref(tblData);
}

void XLog_PageTaskAdd(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblData = xvoCreateTable();
	(void)objServer; (void)objHost; (void)objSession;
	xvoTableSetInt(tblData, "serviceId", 9, XLog_ReadServiceIdQuery(objReq));
	XLog_SendTemplate(objResp, "tasks_add.html", tblData);
	xvoUnref(tblData);
}

void XLog_PageTaskEdit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int64 serviceId;
	int64 taskId;
	sqlite3* pSvcDb;
	xvalue tblTask;
	(void)objServer; (void)objHost; (void)objSession;
	serviceId = XLog_ReadServiceIdQuery(objReq);
	taskId = XLog_ReadInt64Query(objReq, "id", 0);
	pSvcDb = XLog_ServiceGetDB(serviceId);
	if ( (serviceId <= 0) || (taskId <= 0) || (pSvcDb == NULL) ) { xsHttpReplyAuto(objResp, 404, "Content-Type: text/plain\r\n", "task not found", 0); return; }
	tblTask = XLog_TaskGetOne(pSvcDb, taskId);
	if ( !XLog_TableHasId(tblTask) ) {
		xvoUnref(tblTask);
		xsHttpReplyAuto(objResp, 404, "Content-Type: text/plain; charset=utf-8\r\n", "task not found", 0);
		return;
	}
	xvoTableSetInt(tblTask, "serviceId", 9, serviceId);
	XLog_SendTemplate(objResp, "tasks_edit.html", tblTask);
	xvoUnref(tblTask);
}

void XLog_PageTaskLogs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int64 serviceId;
	int64 taskId;
	sqlite3* pSvcDb;
	xvalue tblTask;
	(void)objServer; (void)objHost; (void)objSession;
	serviceId = XLog_ReadServiceIdQuery(objReq);
	taskId = XLog_ReadInt64Query(objReq, "taskId", 0);
	pSvcDb = XLog_ServiceGetDB(serviceId);
	if ( (serviceId <= 0) || (taskId <= 0) || (pSvcDb == NULL) ) { xsHttpReplyAuto(objResp, 404, "Content-Type: text/plain\r\n", "task not found", 0); return; }
	tblTask = XLog_TaskGetOne(pSvcDb, taskId);
	if ( !XLog_TableHasId(tblTask) ) {
		xvoUnref(tblTask);
		xsHttpReplyAuto(objResp, 404, "Content-Type: text/plain; charset=utf-8\r\n", "task not found", 0);
		return;
	}
	xvoTableSetInt(tblTask, "serviceId", 9, serviceId);
	xvoTableSetInt(tblTask, "taskId", 6, taskId);
	xvoTableSetInt(tblTask, "refreshInterval", 15, G_XLogConfig.iDefaultRefreshInterval);
	XLog_SendTemplate(objResp, "tasks_logs.html", tblTask);
	xvoUnref(tblTask);
}

void XLog_ApiServices(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	(void)objServer; (void)objHost; (void)objSession;
	if ( !XLog_OpenMainDb(&pDb) ) { XLog_SendError(objResp, "database error"); return; }

	if ( xsReqMethodID(objReq) == XHTTPD_METHOD_GET ) {
		int iPage = XLog_ClampInt(XLog_ReadIntQuery(objReq, "page", 1), 1, 1000000, 1);
		int iLimit = XLog_ClampInt(XLog_ReadIntQuery(objReq, "limit", 20), 1, 500, 20);
		int iOffset = (iPage - 1) * iLimit;
		str sSearch = XLog_ReadQuery(objReq, "search");
		str sPattern = NULL;
		sqlite3_stmt* stmt = NULL;
		xvalue arrData = xvoCreateArray();
		xvalue tblRet;
		int64 iCount = 0;
		const char* sSql;

		if ( sSearch && sSearch[0] ) {
			sSql = "SELECT id, name, desc, db, createTime, updateTime, COUNT(*) OVER() FROM services WHERE isDelete = 0 AND (name LIKE ? OR desc LIKE ?) ORDER BY id DESC LIMIT ? OFFSET ?;";
			if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
				sPattern = xrtFormat("%%%s%%", sSearch);
				sqlite3_bind_text(stmt, 1, sPattern, -1, SQLITE_TRANSIENT);
				sqlite3_bind_text(stmt, 2, sPattern, -1, SQLITE_TRANSIENT);
				sqlite3_bind_int(stmt, 3, iLimit);
				sqlite3_bind_int(stmt, 4, iOffset);
			}
		} else {
			sSql = "SELECT id, name, desc, db, createTime, updateTime, COUNT(*) OVER() FROM services WHERE isDelete = 0 ORDER BY id DESC LIMIT ? OFFSET ?;";
			if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
				sqlite3_bind_int(stmt, 1, iLimit);
				sqlite3_bind_int(stmt, 2, iOffset);
			}
		}
		if ( stmt ) {
			while ( sqlite3_step(stmt) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
				xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
				xvoTableSetText(tblRow, "db", 2, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(sqlite3_column_int64(stmt, 4), XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(sqlite3_column_int64(stmt, 5), XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iCount = sqlite3_column_int64(stmt, 6);
				xvoArrayAppendValue(arrData, tblRow, TRUE);
			}
			sqlite3_finalize(stmt);
		}
		if ( sPattern ) xrtFree(sPattern);
		if ( sSearch ) xrtFree(sSearch);
		tblRet = XLog_NewResult(TRUE, "success");
		xvoTableSetInt(tblRet, "count", 5, iCount);
		xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
		XLog_SendJsonValue(objResp, tblRet);
	} else if ( xsReqMethodID(objReq) == XHTTPD_METHOD_POST ) {
		xvalue tblBody = XLog_ParseJsonBody(objReq);
		str sName;
		str sDesc;
		str sDbXid;
		str sDbName;
		sqlite3_stmt* stmt = NULL;
		int64 iNow = xrtNow();
		int64 iServiceId = 0;
		sqlite3* pSvcDb;
		if ( tblBody == NULL ) { XLog_SendError(objResp, "invalid json"); XLog_CloseDb(pDb); return; }
		sName = xvoTableGetText(tblBody, "name", 4);
		sDesc = xvoTableGetText(tblBody, "desc", 4);
		if ( XLog_IsBlank(sName) ) sName = "Unnamed Service";
		if ( sDesc == NULL ) sDesc = "";
		sDbXid = xrtMakeXIDS();
		sDbName = xrtFormat("%s.db", sDbXid);
		xrtFree(sDbXid);
		if ( sqlite3_prepare_v2(pDb, "INSERT INTO services (name, desc, db, createTime, updateTime, isDelete) VALUES (?, ?, ?, ?, ?, 0);", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sName, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, sDesc, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sDbName, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 4, iNow);
			sqlite3_bind_int64(stmt, 5, iNow);
			if ( sqlite3_step(stmt) == SQLITE_DONE ) iServiceId = sqlite3_last_insert_rowid(pDb);
			sqlite3_finalize(stmt);
		}
		pSvcDb = XLog_ServiceConnectDB(iServiceId, sDbName);
		xrtFree(sDbName);
		if ( (iServiceId <= 0) || (pSvcDb == NULL) ) { XLog_SendError(objResp, "create service failed"); }
		else {
			xvalue tblRet = XLog_NewResult(TRUE, "created");
			xvalue tblData = xvoCreateTable();
			xvoTableSetInt(tblData, "id", 2, iServiceId);
			xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
			XLog_SendJsonValue(objResp, tblRet);
		}
		xvoUnref(tblBody);
	} else if ( xsReqMethodID(objReq) == XHTTPD_METHOD_PUT ) {
		xvalue tblBody = XLog_ParseJsonBody(objReq);
		sqlite3_stmt* stmt = NULL;
		int64 id;
		str sName;
		str sDesc;
		if ( tblBody == NULL ) { XLog_SendError(objResp, "invalid json"); XLog_CloseDb(pDb); return; }
		id = xvoTableGetInt(tblBody, "id", 2);
		sName = xvoTableGetText(tblBody, "name", 4);
		sDesc = xvoTableGetText(tblBody, "desc", 4);
		if ( XLog_IsBlank(sName) ) sName = "Unnamed Service";
		if ( sDesc == NULL ) sDesc = "";
		if ( sqlite3_prepare_v2(pDb, "UPDATE services SET name = ?, desc = ?, updateTime = ? WHERE id = ? AND isDelete = 0;", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sName, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, sDesc, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 3, xrtNow());
			sqlite3_bind_int64(stmt, 4, id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		xvoUnref(tblBody);
		XLog_SendOk(objResp, "updated");
	} else if ( xsReqMethodID(objReq) == XHTTPD_METHOD_DELETE ) {
		xvalue arrIds = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		int i;
		int iCount;
		if ( (arrIds == NULL) || (xvoType(arrIds) != XVO_DT_ARRAY) ) { if ( arrIds ) xvoUnref(arrIds); XLog_SendError(objResp, "invalid request data"); XLog_CloseDb(pDb); return; }
		iCount = xvoArrayItemCount(arrIds);
		for ( i = 0; i < iCount; i++ ) {
			int64 id = xvoArrayGetInt(arrIds, i);
			sqlite3_stmt* stmt = NULL;
			if ( id <= 0 ) continue;
			XLog_ServiceDisconnectDB(id);
			if ( sqlite3_prepare_v2(pDb, "UPDATE services SET isDelete = 1, updateTime = ? WHERE id = ?;", -1, &stmt, NULL) == SQLITE_OK ) {
				sqlite3_bind_int64(stmt, 1, xrtNow());
				sqlite3_bind_int64(stmt, 2, id);
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);
			}
		}
		xvoUnref(arrIds);
		XLog_SendOk(objResp, "deleted");
	} else {
		xsHttpReplyAuto(objResp, 405, "Content-Type: application/json\r\n", "{\"result\":false,\"message\":\"method not allowed\"}", 0);
	}
	XLog_CloseDb(pDb);
}

void XLog_ApiTasks(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int64 serviceId = XLog_ReadServiceIdQuery(objReq);
	sqlite3* pSvcDb = XLog_ServiceGetDB(serviceId);
	(void)objServer; (void)objHost; (void)objSession;
	if ( (serviceId <= 0) || (pSvcDb == NULL) ) { XLog_SendError(objResp, "service not found"); return; }

	if ( xsReqMethodID(objReq) == XHTTPD_METHOD_GET ) {
		int iPage = XLog_ClampInt(XLog_ReadIntQuery(objReq, "page", 1), 1, 1000000, 1);
		int iLimit = XLog_ClampInt(XLog_ReadIntQuery(objReq, "limit", 20), 1, 500, 20);
		int iOffset = (iPage - 1) * iLimit;
		str sSearch = XLog_ReadQuery(objReq, "search");
		str sPattern = NULL;
		sqlite3_stmt* stmt = NULL;
		xvalue arrData = xvoCreateArray();
		xvalue tblRet;
		int64 iCount = 0;
		if ( sSearch && sSearch[0] ) {
			if ( sqlite3_prepare_v2(pSvcDb, "SELECT id, name, desc, tableName, createTime, updateTime, COUNT(*) OVER() FROM tasks WHERE isDelete = 0 AND (name LIKE ? OR desc LIKE ?) ORDER BY id DESC LIMIT ? OFFSET ?;", -1, &stmt, NULL) == SQLITE_OK ) {
				sPattern = xrtFormat("%%%s%%", sSearch);
				sqlite3_bind_text(stmt, 1, sPattern, -1, SQLITE_TRANSIENT);
				sqlite3_bind_text(stmt, 2, sPattern, -1, SQLITE_TRANSIENT);
				sqlite3_bind_int(stmt, 3, iLimit);
				sqlite3_bind_int(stmt, 4, iOffset);
			}
		} else {
			if ( sqlite3_prepare_v2(pSvcDb, "SELECT id, name, desc, tableName, createTime, updateTime, COUNT(*) OVER() FROM tasks WHERE isDelete = 0 ORDER BY id DESC LIMIT ? OFFSET ?;", -1, &stmt, NULL) == SQLITE_OK ) {
				sqlite3_bind_int(stmt, 1, iLimit);
				sqlite3_bind_int(stmt, 2, iOffset);
			}
		}
		if ( stmt ) {
			while ( sqlite3_step(stmt) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
				xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
				xvoTableSetText(tblRow, "tableName", 9, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(sqlite3_column_int64(stmt, 4), XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(sqlite3_column_int64(stmt, 5), XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				iCount = sqlite3_column_int64(stmt, 6);
				xvoArrayAppendValue(arrData, tblRow, TRUE);
			}
			sqlite3_finalize(stmt);
		}
		if ( sPattern ) xrtFree(sPattern);
		if ( sSearch ) xrtFree(sSearch);
		tblRet = XLog_NewResult(TRUE, "success");
		xvoTableSetInt(tblRet, "count", 5, iCount);
		xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
		XLog_SendJsonValue(objResp, tblRet);
	} else if ( xsReqMethodID(objReq) == XHTTPD_METHOD_POST ) {
		xvalue tblBody = XLog_ParseJsonBody(objReq);
		int64 taskId;
		if ( tblBody == NULL ) { XLog_SendError(objResp, "invalid json"); return; }
		taskId = XLog_TaskCreate(pSvcDb, xvoTableGetText(tblBody, "name", 4), xvoTableGetText(tblBody, "desc", 4));
		xvoUnref(tblBody);
		if ( taskId <= 0 ) XLog_SendError(objResp, "create task failed");
		else {
			xvalue tblRet = XLog_NewResult(TRUE, "created");
			xvalue tblData = xvoCreateTable();
			xvoTableSetInt(tblData, "id", 2, taskId);
			xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
			XLog_SendJsonValue(objResp, tblRet);
		}
	} else if ( xsReqMethodID(objReq) == XHTTPD_METHOD_PUT ) {
		xvalue tblBody = XLog_ParseJsonBody(objReq);
		sqlite3_stmt* stmt = NULL;
		int64 id;
		str sName;
		str sDesc;
		if ( tblBody == NULL ) { XLog_SendError(objResp, "invalid json"); return; }
		id = xvoTableGetInt(tblBody, "id", 2);
		sName = xvoTableGetText(tblBody, "name", 4);
		sDesc = xvoTableGetText(tblBody, "desc", 4);
		if ( XLog_IsBlank(sName) ) sName = "Unnamed Task";
		if ( sDesc == NULL ) sDesc = "";
		if ( sqlite3_prepare_v2(pSvcDb, "UPDATE tasks SET name = ?, desc = ?, updateTime = ? WHERE id = ? AND isDelete = 0;", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sName, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, sDesc, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 3, xrtNow());
			sqlite3_bind_int64(stmt, 4, id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		xvoUnref(tblBody);
		XLog_SendOk(objResp, "updated");
	} else if ( xsReqMethodID(objReq) == XHTTPD_METHOD_DELETE ) {
		xvalue arrIds = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		int i;
		int iCount;
		if ( (arrIds == NULL) || (xvoType(arrIds) != XVO_DT_ARRAY) ) { if ( arrIds ) xvoUnref(arrIds); XLog_SendError(objResp, "invalid request data"); return; }
		iCount = xvoArrayItemCount(arrIds);
		for ( i = 0; i < iCount; i++ ) {
			int64 id = xvoArrayGetInt(arrIds, i);
			sqlite3_stmt* stmt = NULL;
			if ( id <= 0 ) continue;
			if ( sqlite3_prepare_v2(pSvcDb, "UPDATE tasks SET isDelete = 1, updateTime = ? WHERE id = ?;", -1, &stmt, NULL) == SQLITE_OK ) {
				sqlite3_bind_int64(stmt, 1, xrtNow());
				sqlite3_bind_int64(stmt, 2, id);
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);
			}
		}
		xvoUnref(arrIds);
		XLog_SendOk(objResp, "deleted");
	} else {
		xsHttpReplyAuto(objResp, 405, "Content-Type: application/json\r\n", "{\"result\":false,\"message\":\"method not allowed\"}", 0);
	}
}

void XLog_ApiTaskLogs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int64 serviceId = XLog_ReadInt64Query(objReq, "serviceId", 0);
	int64 taskId = XLog_ReadInt64Query(objReq, "taskId", 0);
	sqlite3* pSvcDb = XLog_ServiceGetDB(serviceId);
	xvalue tblTask;
	str sTableName;
	(void)objServer; (void)objHost; (void)objSession;
	if ( (serviceId <= 0) || (taskId <= 0) || (pSvcDb == NULL) ) { XLog_ReplyApiCode(objResp, 1, "invalid params"); return; }
	tblTask = XLog_TaskGetOne(pSvcDb, taskId);
	sTableName = xvoTableGetText(tblTask, "tableName", 9);
	if ( !XLog_IsSafeLogTableName(sTableName) ) { xvoUnref(tblTask); XLog_ReplyApiCode(objResp, 1, "task not found"); return; }

	if ( xsReqMethodID(objReq) == XHTTPD_METHOD_GET ) {
		int64 lastId = XLog_ReadInt64Query(objReq, "lastId", 0);
		int iLimit = XLog_ClampInt(XLog_ReadIntQuery(objReq, "limit", 100), 1, 1000, 100);
		sqlite3_stmt* stmt = NULL;
		xvalue arrData = xvoCreateArray();
		xvalue tblRet;
		str sSql;
		if ( lastId > 0 ) {
			sSql = xrtFormat("SELECT id, time, class, text FROM %s WHERE id > ? ORDER BY id ASC LIMIT ?;", sTableName);
			if ( sqlite3_prepare_v2(pSvcDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
				sqlite3_bind_int64(stmt, 1, lastId);
				sqlite3_bind_int(stmt, 2, iLimit);
			}
		} else {
			sSql = xrtFormat("SELECT id, time, class, text FROM %s ORDER BY id ASC LIMIT ?;", sTableName);
			if ( sqlite3_prepare_v2(pSvcDb, sSql, -1, &stmt, NULL) == SQLITE_OK ) {
				sqlite3_bind_int(stmt, 1, iLimit);
			}
		}
		xrtFree(sSql);
		if ( stmt ) {
			while ( sqlite3_step(stmt) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
				xvoTableSetText(tblRow, "time", 4, xrtTimeToStr(sqlite3_column_int64(stmt, 1), XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				xvoTableSetText(tblRow, "class", 5, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
				xvoTableSetText(tblRow, "text", 4, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
				xvoArrayAppendValue(arrData, tblRow, TRUE);
			}
			sqlite3_finalize(stmt);
		}
		tblRet = xvoCreateTable();
		xvoTableSetInt(tblRet, "code", 4, 0);
		xvoTableSetText(tblRet, "message", 7, (str)"success", 0, FALSE);
		xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
		XLog_SendJsonValue(objResp, tblRet);
	} else if ( xsReqMethodID(objReq) == XHTTPD_METHOD_DELETE ) {
		str sSql = xrtFormat("DELETE FROM %s;DELETE FROM sqlite_sequence WHERE name='%s';", sTableName, sTableName);
		XLog_Exec(pSvcDb, sSql);
		xrtFree(sSql);
		XLog_ReplyApiCode(objResp, 0, "success");
	} else {
		XLog_ReplyApiCode(objResp, 1, "method not allowed");
	}
	xvoUnref(tblTask);
}

void XLog_ApiLogPush(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody;
	int64 serviceId;
	int64 taskId;
	int64 logId;
	sqlite3* pSvcDb;
	xvalue tblTask;
	str sTableName;
	(void)objServer; (void)objHost; (void)objSession;
	if ( xsReqMethodID(objReq) != XHTTPD_METHOD_POST ) { XLog_ReplyApiCode(objResp, 1, "POST only"); return; }
	tblBody = XLog_ParseJsonBody(objReq);
	if ( tblBody == NULL ) { XLog_ReplyApiCode(objResp, 1, "invalid json"); return; }
	serviceId = xvoTableGetInt(tblBody, "service", 7);
	taskId = xvoTableGetInt(tblBody, "task", 4);
	if ( (serviceId <= 0) || (taskId <= 0) ) { xvoUnref(tblBody); XLog_ReplyApiCode(objResp, 1, "invalid service or task id"); return; }
	pSvcDb = XLog_ServiceGetDB(serviceId);
	if ( pSvcDb == NULL ) { xvoUnref(tblBody); XLog_ReplyApiCode(objResp, 1, "service not found"); return; }
	tblTask = XLog_TaskGetOne(pSvcDb, taskId);
	sTableName = xvoTableGetText(tblTask, "tableName", 9);
	if ( !XLog_IsSafeLogTableName(sTableName) ) { xvoUnref(tblTask); xvoUnref(tblBody); XLog_ReplyApiCode(objResp, 1, "task not found"); return; }
	logId = XLog_LogAdd(pSvcDb, sTableName, xvoTableGetText(tblBody, "class", 5), xvoTableGetText(tblBody, "text", 4));
	xvoUnref(tblTask);
	xvoUnref(tblBody);
	if ( logId <= 0 ) {
		XLog_ReplyApiCode(objResp, 1, "write log failed");
	} else {
		xvalue tblData = xvoCreateTable();
		xvoTableSetInt(tblData, "id", 2, logId);
		XLog_ReplyApiData(objResp, tblData);
	}
}

void XLog_ApiTaskCreate(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblBody;
	int64 serviceId;
	int64 taskId;
	str sName;
	sqlite3* pSvcDb;
	(void)objServer; (void)objHost; (void)objSession;
	if ( xsReqMethodID(objReq) != XHTTPD_METHOD_POST ) { XLog_ReplyApiCode(objResp, 1, "POST only"); return; }
	tblBody = XLog_ParseJsonBody(objReq);
	if ( tblBody == NULL ) { XLog_ReplyApiCode(objResp, 1, "invalid json"); return; }
	serviceId = xvoTableGetInt(tblBody, "service", 7);
	sName = xvoTableGetText(tblBody, "name", 4);
	if ( serviceId <= 0 ) { xvoUnref(tblBody); XLog_ReplyApiCode(objResp, 1, "invalid service id"); return; }
	if ( XLog_IsBlank(sName) ) { xvoUnref(tblBody); XLog_ReplyApiCode(objResp, 1, "task name required"); return; }
	pSvcDb = XLog_ServiceGetDB(serviceId);
	if ( pSvcDb == NULL ) { xvoUnref(tblBody); XLog_ReplyApiCode(objResp, 1, "service not found"); return; }
	taskId = XLog_TaskCreate(pSvcDb, sName, xvoTableGetText(tblBody, "desc", 4));
	xvoUnref(tblBody);
	if ( taskId <= 0 ) {
		XLog_ReplyApiCode(objResp, 1, "create task failed");
	} else {
		xvalue tblData = xvoCreateTable();
		xvoTableSetInt(tblData, "id", 2, taskId);
		XLog_ReplyApiData(objResp, tblData);
	}
}

int XLog_RegisterAdminRoutes(XAdminPluginHandle handle, int iAuthId)
{
	XAdminRouteDecl route;
	const char* paths[] = {
		"/admin/view/plugin/xlogserver",
		"/admin/view/plugin/xlogserver/services/add",
		"/admin/view/plugin/xlogserver/services/edit",
		"/admin/view/plugin/xlogserver/tasks",
		"/admin/view/plugin/xlogserver/tasks/add",
		"/admin/view/plugin/xlogserver/tasks/edit",
		"/admin/view/plugin/xlogserver/tasks/logs",
		"/admin/api/plugin/xlogserver/services",
		"/admin/api/plugin/xlogserver/tasks",
		"/admin/api/plugin/xlogserver/task/logs",
		"/view/services",
		"/view/services/add",
		"/view/services/edit",
		"/view/services/tasks",
		"/view/tasks/add",
		"/view/tasks/edit",
		"/view/tasks/logs",
		"/services",
		"/tasks",
		"/task/logs"
	};
	void* procs[] = {
		XLog_PageServices,
		XLog_PageServiceAdd,
		XLog_PageServiceEdit,
		XLog_PageTasks,
		XLog_PageTaskAdd,
		XLog_PageTaskEdit,
		XLog_PageTaskLogs,
		XLog_ApiServices,
		XLog_ApiTasks,
		XLog_ApiTaskLogs,
		XLog_PageServices,
		XLog_PageServiceAdd,
		XLog_PageServiceEdit,
		XLog_PageTasks,
		XLog_PageTaskAdd,
		XLog_PageTaskEdit,
		XLog_PageTaskLogs,
		XLog_ApiServices,
		XLog_ApiTasks,
		XLog_ApiTaskLogs
	};
	int i;
	for ( i = 0; i < (int)(sizeof(paths) / sizeof(paths[0])); i++ ) {
		memset(&route, 0, sizeof(route));
		route.path = paths[i];
		route.proc = procs[i];
		route.need_auth = TRUE;
		route.admin_only = TRUE;
		route.auth_id = iAuthId;
		route.description = paths[i];
		route.sort = 100010 + i;
		route.need_log = strstr(paths[i], "/api/") != NULL ? TRUE : FALSE;
		if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;
	}
	return 0;
}

int XLog_RegisterPublicRoutes(XAdminPluginHandle handle)
{
	XAdminRouteDecl route;
	memset(&route, 0, sizeof(route));
	route.path = "/api/v1/log/push";
	route.proc = XLog_ApiLogPush;
	route.description = "Push log entries";
	route.sort = 100001;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/api/v1/task/create";
	route.proc = XLog_ApiTaskCreate;
	route.description = "Create log task";
	route.sort = 100002;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;
	return 0;
}

int XLog_RegisterMenuAndAuth(XAdminPluginHandle handle)
{
	XAdminAuthGroupDecl authGroup;
	XAdminAuthDecl auth;
	XAdminMenuDecl menu;
	int iAuthGroupId = 0;
	int iAuthId = 0;
	int iRootMenuId = 0;

	memset(&authGroup, 0, sizeof(authGroup));
	authGroup.scope = XADMIN_AUTH_SCOPE_ADMIN;
	authGroup.key = "xlogserver";
	authGroup.name = "Log Service";
	authGroup.description = "Log service management";
	authGroup.sort = 100000;
	if ( XAdmin_RegisterAuthGroup(handle, &authGroup, &iAuthGroupId, NULL) != 0 ) return -1;

	memset(&auth, 0, sizeof(auth));
	auth.scope = XADMIN_AUTH_SCOPE_ADMIN;
	auth.key = "xlogserver.manage";
	auth.group_id = iAuthGroupId;
	auth.name = "xlogserver.manage";
	auth.description = "Manage log services and tasks";
	auth.sort = 100001;
	if ( XAdmin_RegisterAuth(handle, &auth, &iAuthId, NULL) != 0 ) return -1;

	if ( XLog_RegisterAdminRoutes(handle, iAuthId) != 0 ) return -1;

	memset(&menu, 0, sizeof(menu));
	menu.key = "xlogserver.root";
	menu.title = "日志服务";
	menu.icon = "layui-icon layui-icon-log";
	menu.type = 0;
	menu.open_type = "";
	menu.href = "";
	menu.sort = 100;
	menu.visible = TRUE;
	menu.remark = "Log service management";
	if ( XAdmin_RegisterMenu(handle, &menu, &iRootMenuId, NULL) != 0 ) return -1;

	memset(&menu, 0, sizeof(menu));
	menu.key = "xlogserver.services";
	menu.parent_id = iRootMenuId;
	menu.title = "服务管理";
	menu.icon = "layui-icon layui-icon-set";
	menu.type = 1;
	menu.open_type = "_iframe";
	menu.href = "/admin/view/plugin/xlogserver";
	menu.sort = 101;
	menu.visible = TRUE;
	menu.remark = "Manage log services";
	if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) return -1;

	return 0;
}

int XLog_OnLoad(XAdminPluginHandle* out_handle)
{
	if ( out_handle ) G_XLogHandle = *out_handle;
	return 0;
}

int XLog_OnInstall(XAdminPluginHandle handle)
{
	(void)handle;
	XLog_EnsureMainSchema();
	{
		str sDir = XLog_GetLogsDir();
		if ( sDir ) xrtFree(sDir);
	}
	return 0;
}

int XLog_OnStart(XAdminPluginHandle handle)
{
	G_XLogHandle = handle;
	if ( !XLog_EnsureMainSchema() ) return -1;
	G_XLogServiceDbLock = xrtMutexCreate();
	G_XLogServiceDbCache = xrtDictCreate(sizeof(sqlite3*), XRT_OBJMODE_SHARED);
	XLog_ServiceLoadAllDB();
	if ( XLog_RegisterMenuAndAuth(handle) != 0 ) return -1;
	if ( XLog_RegisterPublicRoutes(handle) != 0 ) return -1;
	XAdmin_Log(handle, LOG_INFO, "xlogserver started");
	return 0;
}

int XLog_OnConfigChanged(XAdminPluginHandle handle, xvalue new_cfg)
{
	(void)handle;
	XLog_ConfigReset();
	if ( new_cfg && (xvoType(new_cfg) == XVO_DT_TABLE) ) {
		G_XLogConfig.iMaxLogLines = XLog_ClampInt((int)xvoTableGetInt(new_cfg, "maxLogLines", 12), 100, 1000000, XLOG_DEFAULT_MAX_LINES);
		G_XLogConfig.iDefaultRefreshInterval = XLog_ClampInt((int)xvoTableGetInt(new_cfg, "defaultRefreshInterval", 22), 1000, 60000, XLOG_DEFAULT_REFRESH_MS);
	}
	return 0;
}

int XLog_OnHealthCheck(XAdminPluginHandle handle, XAdminHealthReport* out_report)
{
	bool bOK;
	(void)handle;
	bOK = XLog_EnsureMainSchema();
	if ( out_report ) {
		out_report->status_code = bOK ? 0 : -1;
		out_report->message = bOK ? "ok" : "private database unavailable";
	}
	return bOK ? 0 : -1;
}

void XLog_OnStop(XAdminPluginHandle handle)
{
	(void)handle;
	XLog_ServiceDisconnectAll();
	if ( G_XLogServiceDbCache ) {
		xrtDictDestroy(G_XLogServiceDbCache);
		G_XLogServiceDbCache = NULL;
	}
	if ( G_XLogServiceDbLock ) {
		xrtMutexDestroy(G_XLogServiceDbLock);
		G_XLogServiceDbLock = NULL;
	}
	G_XLogHandle = NULL;
}

void XLog_OnUnload(XAdminPluginHandle handle)
{
	(void)handle;
	G_XLogHandle = NULL;
	G_XLogHost = NULL;
	G_XLogXid = NULL;
	G_XLogRootPath = NULL;
	G_XLogDataPath = NULL;
	G_XLogPrivateDbPath = NULL;
	G_XLogMainDb = NULL;
	XLog_ConfigReset();
}

static XAdminPluginDescriptor G_XLogPlugin = {
	XADMIN_ABI_VERSION,
	sizeof(XAdminPluginDescriptor),
	"xlogserver",
	"1.0.0",
	"Log Service",
	XLog_OnLoad,
	XLog_OnInstall,
	XLog_OnStart,
	XLog_OnConfigChanged,
	XLog_OnHealthCheck,
	XLog_OnStop,
	XLog_OnUnload
};

XADMIN_DECLARE_PLUGIN(G_XLogPlugin)
