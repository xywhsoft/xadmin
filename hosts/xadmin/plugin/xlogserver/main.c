#include "xs_plugin.h"

typedef struct {
	int iMaxLogLines;
	int iDefaultRefreshInterval;
} XLogConfigState;

static XAdminPluginHandle G_XLogHandle = NULL;
static sqlite3* G_XLogMainDb = NULL;
static const char* G_XLogXid = NULL;
static const char* G_XLogRootPath = NULL;
static const char* G_XLogDataPath = NULL;
static const char* G_XLogPrivateDbPath = NULL;
static XLogConfigState G_XLogConfig = { 10000, 3000 };

#define XLOG_SERVICES_DB_DIR "logs"

static const char* G_XLogMainSchemaSql =
	"CREATE TABLE IF NOT EXISTS services ("
	"id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"name TEXT NOT NULL,"
	"desc TEXT,"
	"db TEXT NOT NULL,"
	"createTime INTEGER,"
	"updateTime INTEGER,"
	"isDelete INTEGER DEFAULT 0"
	");";

XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int idx, void* ptr)
{
	if ( idx == XADMIN_GLOBAL_MAIN_DB ) {
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

void XLog_SendJson(XS_ResponseObject objResp, xvalue tblData)
{
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblData, FALSE, &iSize);
	if ( sJson ) {
		http_reply(objResp, 200, "Content-Type: application/json\r\n", sJson, iSize);
		xrtFree(sJson);
	}
	xvoUnref(tblData);
}

xvalue XLog_CreateResult(bool bResult, const char* sMessage)
{
	xvalue tblRet = xvoCreateTable();
	if ( tblRet == NULL ) return NULL;
	xvoTableSetBool(tblRet, "result", 6, bResult);
	if ( sMessage ) {
		xvoTableSetText(tblRet, "message", 7, (str)sMessage, 0, FALSE);
	}
	return tblRet;
}

void XLog_SendError(XS_ResponseObject objResp, const char* sMessage)
{
	XLog_SendJson(objResp, XLog_CreateResult(FALSE, sMessage));
}

void XLog_SendOk(XS_ResponseObject objResp, const char* sMessage)
{
	XLog_SendJson(objResp, XLog_CreateResult(TRUE, sMessage));
}

int XLog_ReadIntQuery(XS_RequestObject objReq, const char* sName, int iDefault)
{
	char sBuf[32];
	memset(sBuf, 0, sizeof(sBuf));
	HttpGetQueryVar(objReq, sName, sBuf, sizeof(sBuf));
	if ( sBuf[0] == '\0' ) return iDefault;
	return atoi(sBuf);
}

int64 XLog_ReadInt64Query(XS_RequestObject objReq, const char* sName, int64 iDefault)
{
	char sBuf[32];
	memset(sBuf, 0, sizeof(sBuf));
	HttpGetQueryVar(objReq, sName, sBuf, sizeof(sBuf));
	if ( sBuf[0] == '\0' ) return iDefault;
	return xrtStrToI64(sBuf);
}

str XLog_ReadQuery(XS_RequestObject objReq, const char* sName)
{
	char sBuf[256];
	memset(sBuf, 0, sizeof(sBuf));
	HttpGetQueryVar(objReq, sName, sBuf, sizeof(sBuf));
	if ( sBuf[0] == '\0' ) return NULL;
	return xrtCopyStr(sBuf, 0);
}

xvalue XLog_ParseJsonBody(XS_RequestObject objReq)
{
	xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( (tblForm == NULL) || (xvoType(tblForm) != XVO_DT_TABLE) ) {
		if ( tblForm ) xvoUnref(tblForm);
		return NULL;
	}
	return tblForm;
}

static xdict G_XLogServiceDbCache = NULL;
static xmutex G_XLogServiceDbLock = NULL;

str XLog_GetLogsDir()
{
	if ( G_XLogDataPath == NULL ) return NULL;
	str sDir = xrtPathJoin(2, G_XLogDataPath, XLOG_SERVICES_DB_DIR);
	if ( sDir && !xrtDirExists(sDir) ) {
		xrtDirCreate(sDir);
	}
	return sDir;
}

bool XLog_OpenMainDb(sqlite3** ppDb)
{
	sqlite3* pDb = NULL;
	int iRet;
	if ( ppDb ) *ppDb = NULL;
	if ( (ppDb == NULL) || (G_XLogPrivateDbPath == NULL) || (G_XLogPrivateDbPath[0] == '\0') ) return FALSE;
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

bool XLog_EnsureMainSchema()
{
	sqlite3* pDb = NULL;
	char* sError = NULL;
	if ( !XLog_OpenMainDb(&pDb) ) return FALSE;
	if ( sqlite3_exec(pDb, G_XLogMainSchemaSql, NULL, NULL, &sError) != SQLITE_OK ) {
		if ( sError ) sqlite3_free(sError);
		XLog_CloseDb(pDb);
		return FALSE;
	}
	if ( sError ) sqlite3_free(sError);
	XLog_CloseDb(pDb);
	return TRUE;
}

sqlite3* XLog_ServiceConnectDB(int64 serviceId, const char* sDbName)
{
	str sDir = XLog_GetLogsDir();
	sqlite3* pDb = NULL;
	int iRet;
	str sDbPath;

	if ( sDir == NULL ) return NULL;
	sDbPath = xrtPathJoin(2, sDir, (str)sDbName);
	xrtFree(sDir);
	if ( sDbPath == NULL ) return NULL;

	iRet = sqlite3_open_v2(sDbPath, &pDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
	xrtFree(sDbPath);
	if ( iRet != SQLITE_OK ) {
		if ( pDb ) sqlite3_close(pDb);
		return NULL;
	}
	sqlite3_busy_timeout(pDb, 3000);

	if ( G_XLogServiceDbLock ) xrtMutexLock(G_XLogServiceDbLock);
	if ( G_XLogServiceDbCache ) {
		sqlite3** ppExisting = (sqlite3**)xrtDictGetPtr(G_XLogServiceDbCache, (str)&serviceId, sizeof(int64));
		if ( ppExisting && *ppExisting ) {
			sqlite3_close(*ppExisting);
		}
		sqlite3* pCopy = pDb;
		xrtDictSetPtr(G_XLogServiceDbCache, (str)&serviceId, sizeof(int64), pCopy, NULL);
	}
	if ( G_XLogServiceDbLock ) xrtMutexUnlock(G_XLogServiceDbLock);

	return pDb;
}

sqlite3* XLog_ServiceGetDB(int64 serviceId)
{
	sqlite3* pDb = NULL;
	if ( G_XLogServiceDbLock ) xrtMutexLock(G_XLogServiceDbLock);
	if ( G_XLogServiceDbCache ) {
		sqlite3** ppDb = (sqlite3**)xrtDictGetPtr(G_XLogServiceDbCache, (str)&serviceId, sizeof(int64));
		if ( ppDb ) pDb = *ppDb;
	}
	if ( G_XLogServiceDbLock ) xrtMutexUnlock(G_XLogServiceDbLock);
	return pDb;
}

void XLog_ServiceDisconnectDB(int64 serviceId)
{
	if ( G_XLogServiceDbLock ) xrtMutexLock(G_XLogServiceDbLock);
	if ( G_XLogServiceDbCache ) {
		sqlite3** ppDb = (sqlite3**)xrtDictGetPtr(G_XLogServiceDbCache, (str)&serviceId, sizeof(int64));
		if ( ppDb && *ppDb ) {
			sqlite3_close(*ppDb);
			*ppDb = NULL;
		}
	}
	if ( G_XLogServiceDbLock ) xrtMutexUnlock(G_XLogServiceDbLock);
}

static bool XLog_DisconnectAllProc(Dict_Key* pKey, sqlite3** ppDb, void* pArg)
{
	(void)pKey; (void)pArg;
	if ( ppDb && *ppDb ) {
		sqlite3_close(*ppDb);
		*ppDb = NULL;
	}
	return FALSE;
}

void XLog_ServiceDisconnectAll()
{
	if ( G_XLogServiceDbCache == NULL ) return;
	xrtDictWalk(G_XLogServiceDbCache, (Dict_EachProc)XLog_DisconnectAllProc, NULL);
}

typedef bool (*XLog_DictWalkProc)(int64* pKey, sqlite3** ppDb, void* pParam);

void XLog_ServiceLoadAllDB()
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;

	if ( !XLog_OpenMainDb(&pDb) ) return;
	if ( sqlite3_prepare_v2(pDb, "SELECT id, db FROM services WHERE isDelete = 0;", -1, &stmt, NULL) != SQLITE_OK ) {
		XLog_CloseDb(pDb);
		return;
	}
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		int64 id = sqlite3_column_int64(stmt, 0);
		const char* sDbName = (const char*)sqlite3_column_text(stmt, 1);
		if ( sDbName ) {
			XLog_ServiceConnectDB(id, sDbName);
		}
	}
	sqlite3_finalize(stmt);
	XLog_CloseDb(pDb);
}

bool XLog_SendAssetHtml(XS_ResponseObject objResp, const char* sFileName)
{
	str sPath;
	ptr pData;
	size_t iSize = 0;

	if ( (G_XLogRootPath == NULL) || (sFileName == NULL) ) return FALSE;
	sPath = xrtPathJoin(2, G_XLogRootPath, (str)sFileName);
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

str XLog_RenderTemplate(const char* sFileName, xvalue tblData)
{
	str sPath;
	str sText;
	size_t iSize;
	xbuffer_struct tBuf = {0};
	size_t iPos;
	size_t iTextLen;
	size_t iKeyLen;
	str sKey;
	str sVal;

	if ( (G_XLogRootPath == NULL) || (sFileName == NULL) ) return NULL;
	sPath = xrtPathJoin(3, G_XLogRootPath, (str)"template", (str)sFileName);
	if ( (sPath == NULL) || !xrtFileExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		return NULL;
	}
	sText = xrtFileReadAll(sPath, XRT_CP_UTF8, NULL);
	xrtFree(sPath);
	if ( sText == NULL ) return NULL;

	if ( tblData == NULL ) return sText;

	xrtBufferInit(&tBuf, 0);
	iTextLen = strlen(sText);
	iPos = 0;

	while ( iPos < iTextLen ) {
		if ( (sText[iPos] == '{') && (iPos + 1 < iTextLen) && (sText[iPos + 1] == '$') ) {
			size_t iStart = iPos + 2;
			size_t iEnd = iStart;
			while ( (iEnd < iTextLen) && (sText[iEnd] != '}') ) iEnd++;
			if ( iEnd < iTextLen ) {
				iKeyLen = iEnd - iStart;
				sKey = xrtCopyStr(sText + iStart, iKeyLen);
				if ( sKey ) {
					sVal = xvoTableGetText(tblData, sKey, (int)strlen(sKey));
					if ( sVal && sVal[0] ) {
						xrtBufferAppend(&tBuf, sVal, (uint32)strlen(sVal), XBUF_BINARY);
					}
					xrtFree(sKey);
				}
				iPos = iEnd + 1;
				continue;
			}
		}
		{
			char ch = sText[iPos];
			xrtBufferAppend(&tBuf, &ch, 1, XBUF_BINARY);
		}
		iPos++;
	}

	xrtFree(sText);
	{
		char chZero = 0;
		xrtBufferAppend(&tBuf, &chZero, 1, XBUF_BINARY);
	}
	return (str)tBuf.Buffer;
}

void XLog_SendTemplatePage(XS_ResponseObject objResp, const char* sFileName, xvalue tblData)
{
	str sPage = XLog_RenderTemplate(sFileName, tblData);
	if ( sPage ) {
		size_t iLen = strlen(sPage);
		http_reply(objResp, 200, "Content-Type: text/html; charset=utf-8\r\n", sPage, iLen);
		xrtFree(sPage);
	} else {
		http_reply(objResp, 500, "Content-Type: text/plain\r\n", "template not found", 0);
	}
}

xvalue XLog_ServiceGetOne(sqlite3* pDb, int64 id)
{
	sqlite3_stmt* stmt = NULL;
	xvalue tblInfo = xvoCreateTable();
	if ( sqlite3_prepare_v2(pDb, "SELECT * FROM services WHERE id = ?;", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, id);
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

xvalue XLog_TaskGetOne(sqlite3* pDb, int64 id)
{
	sqlite3_stmt* stmt = NULL;
	xvalue tblInfo = xvoCreateTable();
	if ( sqlite3_prepare_v2(pDb, "SELECT * FROM tasks WHERE id = ?;", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, id);
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
	xtime now = xrtNow();
	int64 newId = 0;
	sqlite3_stmt* stmt = NULL;
	str sql;
	str tableName;

	sql = "INSERT INTO tasks (name, desc, tableName, createTime, updateTime, isDelete) VALUES (?, ?, '', ?, ?, 0);";
	if ( sqlite3_prepare_v2(pDb, sql, -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sName ? sName : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, sDesc ? sDesc : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 3, now);
		sqlite3_bind_int64(stmt, 4, now);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		newId = sqlite3_last_insert_rowid(pDb);
	}

	if ( newId > 0 ) {
		tableName = xrtFormat("log_%lld", newId);
		sql = "UPDATE tasks SET tableName = ? WHERE id = ?;";
		if ( sqlite3_prepare_v2(pDb, sql, -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, tableName, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 2, newId);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		{
			str createSql = xrtFormat("CREATE TABLE IF NOT EXISTS %s (id INTEGER PRIMARY KEY AUTOINCREMENT, time INTEGER, class TEXT, text TEXT);", tableName);
			if ( sqlite3_prepare_v2(pDb, createSql, -1, &stmt, NULL) == SQLITE_OK ) {
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);
			}
			xrtFree(createSql);
		}
		xrtFree(tableName);
	}
	return newId;
}

int64 XLog_LogAdd(sqlite3* pDb, const char* sTableName, const char* sClass, const char* sText)
{
	xtime now = xrtNow();
	str sql = xrtFormat("INSERT INTO %s (time, class, text) VALUES (?, ?, ?);", sTableName);
	sqlite3_stmt* stmt = NULL;
	int64 logId = 0;

	if ( sqlite3_prepare_v2(pDb, sql, -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, now);
		sqlite3_bind_text(stmt, 2, sClass ? sClass : "info", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, sText ? sText : "", -1, SQLITE_TRANSIENT);
		if ( sqlite3_step(stmt) == SQLITE_DONE ) {
			logId = sqlite3_last_insert_rowid(pDb);
		}
		sqlite3_finalize(stmt);
	}
	xrtFree(sql);
	return logId;
}

void XLog_ReplyApiCode(XS_ResponseObject objResp, int iCode, const char* sMessage)
{
	xvalue tblRet = xvoCreateTable();
	xvoTableSetInt(tblRet, "code", 4, iCode);
	xvoTableSetText(tblRet, "message", 7, (str)sMessage, 0, FALSE);
	if ( iCode == 0 ) {
		xvoTableSetBool(tblRet, "result", 6, TRUE);
	} else {
		xvoTableSetBool(tblRet, "result", 6, FALSE);
	}
	XLog_SendJson(objResp, tblRet);
}

void XLog_ReplyApiData(XS_ResponseObject objResp, xvalue tblExtra)
{
	xvalue tblRet = xvoCreateTable();
	xvoTableSetInt(tblRet, "code", 4, 0);
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetText(tblRet, "message", 7, (str)"success", 0, FALSE);
	if ( tblExtra ) {
		xvoTableSetValue(tblRet, "data", 4, tblExtra, TRUE);
	}
	XLog_SendJson(objResp, tblRet);
}

void XLog_Req_ViewServices(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !XLog_SendAssetHtml(objResp, "page/services.html") ) {
		http_reply(objResp, 500, "Content-Type: text/plain\r\n", "page not found", 0);
	}
}

void XLog_Req_ViewServicesAdd(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !XLog_SendAssetHtml(objResp, "page/services_add.html") ) {
		http_reply(objResp, 500, "Content-Type: text/plain\r\n", "page not found", 0);
	}
}

void XLog_Req_ViewServicesEdit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	int64 id;
	xvalue tblInfo;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !HttpMethodIs(objReq, "GET") ) {
		http_reply(objResp, 405, "Content-Type: text/plain\r\n", "method not allowed", 0);
		return;
	}

	id = XLog_ReadInt64Query(objReq, "id", 0);
	if ( id <= 0 ) {
		http_reply(objResp, 400, "Content-Type: text/plain\r\n", "invalid id", 0);
		return;
	}

	if ( !XLog_OpenMainDb(&pDb) ) {
		http_reply(objResp, 500, "Content-Type: text/plain\r\n", "db error", 0);
		return;
	}
	tblInfo = XLog_ServiceGetOne(pDb, id);
	XLog_CloseDb(pDb);

	XLog_SendTemplatePage(objResp, "services_edit.html", tblInfo);
	xvoUnref(tblInfo);
}

void XLog_Req_ViewTasks(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int64 serviceId;
	sqlite3* pSvcDb;
	sqlite3* pMainDb = NULL;
	xvalue tblData;
	const char* sServiceName = "Unknown";

	(void)objServer; (void)objHost; (void)objSession;

	if ( !HttpMethodIs(objReq, "GET") ) {
		http_reply(objResp, 405, "Content-Type: text/plain\r\n", "method not allowed", 0);
		return;
	}

	serviceId = XLog_ReadInt64Query(objReq, "serviceId", 0);
	if ( serviceId <= 0 ) {
		http_reply(objResp, 400, "Content-Type: text/plain\r\n", "invalid serviceId", 0);
		return;
	}

	pSvcDb = XLog_ServiceGetDB(serviceId);
	if ( pSvcDb == NULL ) {
		http_reply(objResp, 404, "Content-Type: text/plain\r\n", "service not found", 0);
		return;
	}

	if ( XLog_OpenMainDb(&pMainDb) ) {
		xvalue tblSvc = XLog_ServiceGetOne(pMainDb, serviceId);
		str sName = xvoTableGetText(tblSvc, "name", 4);
		if ( sName && sName[0] ) sServiceName = sName;
		XLog_CloseDb(pMainDb);
		xvoUnref(tblSvc);
	}

	tblData = xvoCreateTable();
	xvoTableSetInt(tblData, "serviceId", 9, serviceId);
	xvoTableSetText(tblData, "serviceName", 11, (str)sServiceName, 0, FALSE);
	XLog_SendTemplatePage(objResp, "tasks_index.html", tblData);
	xvoUnref(tblData);
}

void XLog_Req_ViewTasksAdd(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int64 serviceId;
	xvalue tblData;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !HttpMethodIs(objReq, "GET") ) {
		http_reply(objResp, 405, "Content-Type: text/plain\r\n", "method not allowed", 0);
		return;
	}

	serviceId = XLog_ReadInt64Query(objReq, "serviceId", 0);
	tblData = xvoCreateTable();
	xvoTableSetInt(tblData, "serviceId", 9, serviceId);
	XLog_SendTemplatePage(objResp, "tasks_add.html", tblData);
	xvoUnref(tblData);
}

void XLog_Req_ViewTasksEdit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int64 serviceId, taskId;
	sqlite3* pSvcDb;
	xvalue tblInfo;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !HttpMethodIs(objReq, "GET") ) {
		http_reply(objResp, 405, "Content-Type: text/plain\r\n", "method not allowed", 0);
		return;
	}

	serviceId = XLog_ReadInt64Query(objReq, "serviceId", 0);
	taskId = XLog_ReadInt64Query(objReq, "id", 0);
	if ( serviceId <= 0 || taskId <= 0 ) {
		http_reply(objResp, 400, "Content-Type: text/plain\r\n", "invalid params", 0);
		return;
	}

	pSvcDb = XLog_ServiceGetDB(serviceId);
	if ( pSvcDb == NULL ) {
		http_reply(objResp, 404, "Content-Type: text/plain\r\n", "service not found", 0);
		return;
	}

	tblInfo = XLog_TaskGetOne(pSvcDb, taskId);
	xvoTableSetInt(tblInfo, "serviceId", 9, serviceId);
	XLog_SendTemplatePage(objResp, "tasks_edit.html", tblInfo);
	xvoUnref(tblInfo);
}

void XLog_Req_ViewTasksLogs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int64 serviceId, taskId;
	sqlite3* pSvcDb;
	xvalue tblInfo;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !HttpMethodIs(objReq, "GET") ) {
		http_reply(objResp, 405, "Content-Type: text/plain\r\n", "method not allowed", 0);
		return;
	}

	serviceId = XLog_ReadInt64Query(objReq, "serviceId", 0);
	taskId = XLog_ReadInt64Query(objReq, "taskId", 0);
	if ( serviceId <= 0 || taskId <= 0 ) {
		http_reply(objResp, 400, "Content-Type: text/plain\r\n", "invalid params", 0);
		return;
	}

	pSvcDb = XLog_ServiceGetDB(serviceId);
	if ( pSvcDb == NULL ) {
		http_reply(objResp, 404, "Content-Type: text/plain\r\n", "service not found", 0);
		return;
	}

	tblInfo = XLog_TaskGetOne(pSvcDb, taskId);
	xvoTableSetInt(tblInfo, "serviceId", 9, serviceId);
	xvoTableSetInt(tblInfo, "taskId", 6, taskId);
	XLog_SendTemplatePage(objResp, "tasks_logs.html", tblInfo);
	xvoUnref(tblInfo);
}

void XLog_Req_ApiServices(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !XLog_OpenMainDb(&pDb) ) {
		XLog_SendError(objResp, "database error");
		return;
	}

	if ( HttpMethodIs(objReq, "GET") ) {
		int iPage = XLog_ReadIntQuery(objReq, "page", 1);
		int iLimit = XLog_ReadIntQuery(objReq, "limit", 10);
		int iOffset = (iPage - 1) * iLimit;
		str sSearch = XLog_ReadQuery(objReq, "search");
		sqlite3_stmt* stmt = NULL;
		xvalue arrData = xvoCreateArray();
		int64 iCount = 0;
		str sql;
		int rc;

		if ( sSearch && sSearch[0] ) {
			sql = "SELECT *, COUNT(*) OVER() AS total_count FROM services WHERE (isDelete = 0) AND ((name LIKE ?) OR (desc LIKE ?)) ORDER BY id DESC LIMIT ? OFFSET ?;";
			rc = sqlite3_prepare_v2(pDb, sql, -1, &stmt, NULL);
			if ( rc == SQLITE_OK ) {
				str sPattern = xrtFormat("%%%s%%", sSearch);
				sqlite3_bind_text(stmt, 1, sPattern, -1, SQLITE_TRANSIENT);
				sqlite3_bind_text(stmt, 2, sPattern, -1, SQLITE_TRANSIENT);
				sqlite3_bind_int(stmt, 3, iLimit);
				sqlite3_bind_int(stmt, 4, iOffset);
				xrtFree(sPattern);
			}
		} else {
			sql = "SELECT *, COUNT(*) OVER() AS total_count FROM services WHERE isDelete = 0 ORDER BY id DESC LIMIT ? OFFSET ?;";
			rc = sqlite3_prepare_v2(pDb, sql, -1, &stmt, NULL);
			if ( rc == SQLITE_OK ) {
				sqlite3_bind_int(stmt, 1, iLimit);
				sqlite3_bind_int(stmt, 2, iOffset);
			}
		}
		if ( sSearch ) xrtFree(sSearch);

		if ( (rc == SQLITE_OK) && stmt ) {
			while ( sqlite3_step(stmt) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
				xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
				xvoTableSetText(tblRow, "db", 2, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
				{
					xtime iTime = sqlite3_column_int64(stmt, 4);
					xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
					iTime = sqlite3_column_int64(stmt, 5);
					xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				}
				if ( iCount <= 0 ) iCount = sqlite3_column_int64(stmt, 7);
				xvoArrayAppendValue(arrData, tblRow, TRUE);
			}
			sqlite3_finalize(stmt);
		}

		{
			xvalue tblRet = XLog_CreateResult(TRUE, NULL);
			xvoTableSetInt(tblRet, "code", 4, 0);
			xvoTableSetInt(tblRet, "count", 5, iCount);
			xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
			XLog_SendJson(objResp, tblRet);
		}

	} else if ( HttpMethodIs(objReq, "POST") ) {
		xvalue tblForm = XLog_ParseJsonBody(objReq);
		str sName, sDesc, sDbXID, sDbName;
		xtime now;
		sqlite3_stmt* stmt = NULL;
		int64 newId;
		sqlite3* pSvcDb;

		if ( tblForm == NULL ) { XLog_SendError(objResp, "invalid json"); XLog_CloseDb(pDb); return; }
		sName = xvoTableGetText(tblForm, "name", 4);
		if ( !sName || !sName[0] ) sName = "Unnamed Service";
		sDesc = xvoTableGetText(tblForm, "desc", 4);
		if ( !sDesc ) sDesc = "";

		sDbXID = xrtMakeXIDS();
		sDbName = xrtFormat("%s.db", sDbXID);
		xrtFree(sDbXID);

		now = xrtNow();
		if ( sqlite3_prepare_v2(pDb, "INSERT INTO services (name, desc, db, createTime, updateTime, isDelete) VALUES (?, ?, ?, ?, ?, 0);", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sName, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, sDesc, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sDbName, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 4, now);
			sqlite3_bind_int64(stmt, 5, now);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		newId = sqlite3_last_insert_rowid(pDb);

		pSvcDb = XLog_ServiceConnectDB(newId, sDbName);
		if ( pSvcDb ) {
			if ( sqlite3_prepare_v2(pSvcDb, "CREATE TABLE IF NOT EXISTS tasks (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, desc TEXT, tableName TEXT NOT NULL, createTime INTEGER, updateTime INTEGER, isDelete INTEGER DEFAULT 0);", -1, &stmt, NULL) == SQLITE_OK ) {
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);
			}
		}
		xrtFree(sDbName);
		xvoUnref(tblForm);

		{
			xvalue tblRet = XLog_CreateResult(TRUE, NULL);
			xvalue tblData = xvoCreateTable();
			xvoTableSetInt(tblData, "id", 2, newId);
			xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
			XLog_SendJson(objResp, tblRet);
		}

	} else if ( HttpMethodIs(objReq, "PUT") ) {
		xvalue tblForm = XLog_ParseJsonBody(objReq);
		int64 id;
		str sName, sDesc;
		xtime now;
		sqlite3_stmt* stmt = NULL;

		if ( tblForm == NULL ) { XLog_SendError(objResp, "invalid json"); XLog_CloseDb(pDb); return; }
		id = xvoTableGetInt(tblForm, "id", 2);
		sName = xvoTableGetText(tblForm, "name", 4);
		if ( !sName || !sName[0] ) sName = "Unnamed Service";
		sDesc = xvoTableGetText(tblForm, "desc", 4);
		if ( !sDesc ) sDesc = "";

		now = xrtNow();
		if ( sqlite3_prepare_v2(pDb, "UPDATE services SET name = ?, desc = ?, updateTime = ? WHERE id = ?;", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, sName, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, sDesc, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 3, now);
			sqlite3_bind_int64(stmt, 4, id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		xvoUnref(tblForm);
		XLog_SendOk(objResp, "updated");

	} else if ( HttpMethodIs(objReq, "DELETE") ) {
		xvalue arrID = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( (arrID == NULL) || (xvoType(arrID) != XVO_DT_ARRAY) ) {
			if ( arrID ) xvoUnref(arrID);
			XLog_SendError(objResp, "invalid request data");
			XLog_CloseDb(pDb);
			return;
		}
		{
			int iCount = xvoArrayItemCount(arrID);
			int i;
			sqlite3_stmt* stmt = NULL;
			for ( i = 0; i < iCount; i++ ) {
				int64 id = xvoArrayGetInt(arrID, i);
				if ( id > 0 ) {
					XLog_ServiceDisconnectDB(id);
					if ( sqlite3_prepare_v2(pDb, "UPDATE services SET isDelete = 1 WHERE id = ?;", -1, &stmt, NULL) == SQLITE_OK ) {
						sqlite3_bind_int64(stmt, 1, id);
						sqlite3_step(stmt);
						sqlite3_finalize(stmt);
					}
				}
			}
		}
		xvoUnref(arrID);
		XLog_SendOk(objResp, "deleted");

	} else {
		http_reply(objResp, 405, "Content-Type: application/json\r\n", "{\"result\":false,\"message\":\"method not allowed\"}", 0);
	}

	XLog_CloseDb(pDb);
}

void XLog_Req_ApiTasks(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int64 serviceId;
	sqlite3* pSvcDb;

	(void)objServer; (void)objHost; (void)objSession;

	serviceId = XLog_ReadInt64Query(objReq, "serviceId", 0);
	if ( serviceId <= 0 ) {
		XLog_SendError(objResp, "invalid serviceId");
		return;
	}

	pSvcDb = XLog_ServiceGetDB(serviceId);
	if ( pSvcDb == NULL ) {
		XLog_SendError(objResp, "service not found");
		return;
	}

	if ( HttpMethodIs(objReq, "GET") ) {
		int iPage = XLog_ReadIntQuery(objReq, "page", 1);
		int iLimit = XLog_ReadIntQuery(objReq, "limit", 10);
		int iOffset = (iPage - 1) * iLimit;
		str sSearch = XLog_ReadQuery(objReq, "search");
		sqlite3_stmt* stmt = NULL;
		xvalue arrData = xvoCreateArray();
		int64 iCount = 0;
		str sql;
		int rc;

		if ( sSearch && sSearch[0] ) {
			sql = xrtFormat("SELECT *, COUNT(*) OVER() AS total_count FROM tasks WHERE (isDelete = 0) AND ((name LIKE '%%%s%%') OR (desc LIKE '%%%s%%')) ORDER BY id DESC LIMIT ? OFFSET ?;", sSearch, sSearch);
			rc = sqlite3_prepare_v2(pSvcDb, sql, -1, &stmt, NULL);
			xrtFree(sql);
			if ( rc == SQLITE_OK ) {
				sqlite3_bind_int(stmt, 1, iLimit);
				sqlite3_bind_int(stmt, 2, iOffset);
			}
		} else {
			sql = "SELECT *, COUNT(*) OVER() AS total_count FROM tasks WHERE isDelete = 0 ORDER BY id DESC LIMIT ? OFFSET ?;";
			rc = sqlite3_prepare_v2(pSvcDb, sql, -1, &stmt, NULL);
			if ( rc == SQLITE_OK ) {
				sqlite3_bind_int(stmt, 1, iLimit);
				sqlite3_bind_int(stmt, 2, iOffset);
			}
		}
		if ( sSearch ) xrtFree(sSearch);

		if ( (rc == SQLITE_OK) && stmt ) {
			while ( sqlite3_step(stmt) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
				xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
				xvoTableSetText(tblRow, "tableName", 9, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
				{
					xtime iTime = sqlite3_column_int64(stmt, 4);
					xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
					iTime = sqlite3_column_int64(stmt, 5);
					xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				}
				if ( iCount <= 0 ) iCount = sqlite3_column_int64(stmt, 7);
				xvoArrayAppendValue(arrData, tblRow, TRUE);
			}
			sqlite3_finalize(stmt);
		}

		{
			xvalue tblRet = XLog_CreateResult(TRUE, NULL);
			xvoTableSetInt(tblRet, "code", 4, 0);
			xvoTableSetInt(tblRet, "count", 5, iCount);
			xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
			XLog_SendJson(objResp, tblRet);
		}

	} else if ( HttpMethodIs(objReq, "POST") ) {
		xvalue tblForm = XLog_ParseJsonBody(objReq);
		str sName, sDesc;
		int64 newId;
		if ( tblForm == NULL ) { XLog_SendError(objResp, "invalid json"); return; }
		sName = xvoTableGetText(tblForm, "name", 4);
		if ( !sName || !sName[0] ) sName = "Unnamed Task";
		sDesc = xvoTableGetText(tblForm, "desc", 4);
		if ( !sDesc ) sDesc = "";
		newId = XLog_TaskCreate(pSvcDb, sName, sDesc);
		xvoUnref(tblForm);
		{
			xvalue tblRet = XLog_CreateResult(TRUE, NULL);
			xvalue tblData = xvoCreateTable();
			xvoTableSetInt(tblData, "id", 2, newId);
			xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
			XLog_SendJson(objResp, tblRet);
		}

	} else if ( HttpMethodIs(objReq, "PUT") ) {
		xvalue tblForm = XLog_ParseJsonBody(objReq);
		int64 id;
		str sName, sDesc;
		sqlite3_stmt* stmt = NULL;
		if ( tblForm == NULL ) { XLog_SendError(objResp, "invalid json"); return; }
		id = xvoTableGetInt(tblForm, "id", 2);
		sName = xvoTableGetText(tblForm, "name", 4);
		if ( !sName || !sName[0] ) sName = "Unnamed Task";
		sDesc = xvoTableGetText(tblForm, "desc", 4);
		if ( !sDesc ) sDesc = "";
		{
			xtime now = xrtNow();
			if ( sqlite3_prepare_v2(pSvcDb, "UPDATE tasks SET name = ?, desc = ?, updateTime = ? WHERE id = ?;", -1, &stmt, NULL) == SQLITE_OK ) {
				sqlite3_bind_text(stmt, 1, sName, -1, SQLITE_TRANSIENT);
				sqlite3_bind_text(stmt, 2, sDesc, -1, SQLITE_TRANSIENT);
				sqlite3_bind_int64(stmt, 3, now);
				sqlite3_bind_int64(stmt, 4, id);
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);
			}
		}
		xvoUnref(tblForm);
		XLog_SendOk(objResp, "updated");

	} else if ( HttpMethodIs(objReq, "DELETE") ) {
		xvalue arrID = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		if ( (arrID == NULL) || (xvoType(arrID) != XVO_DT_ARRAY) ) {
			if ( arrID ) xvoUnref(arrID);
			XLog_SendError(objResp, "invalid request data");
			return;
		}
		{
			int iCount = xvoArrayItemCount(arrID);
			int i;
			for ( i = 0; i < iCount; i++ ) {
				int64 id = xvoArrayGetInt(arrID, i);
				if ( id > 0 ) {
					sqlite3_stmt* stmt = NULL;
					if ( sqlite3_prepare_v2(pSvcDb, "UPDATE tasks SET isDelete = 1 WHERE id = ?;", -1, &stmt, NULL) == SQLITE_OK ) {
						sqlite3_bind_int64(stmt, 1, id);
						sqlite3_step(stmt);
						sqlite3_finalize(stmt);
					}
				}
			}
		}
		xvoUnref(arrID);
		XLog_SendOk(objResp, "deleted");

	} else {
		http_reply(objResp, 405, "Content-Type: application/json\r\n", "{\"result\":false,\"message\":\"method not allowed\"}", 0);
	}
}

void XLog_Req_ApiTaskLogs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int64 serviceId, taskId;
	sqlite3* pSvcDb;
	xvalue tblTask;
	str sTableName;

	(void)objServer; (void)objHost; (void)objSession;

	serviceId = XLog_ReadInt64Query(objReq, "serviceId", 0);
	taskId = XLog_ReadInt64Query(objReq, "taskId", 0);
	if ( serviceId <= 0 || taskId <= 0 ) {
		XLog_SendError(objResp, "invalid params");
		return;
	}

	pSvcDb = XLog_ServiceGetDB(serviceId);
	if ( pSvcDb == NULL ) {
		XLog_SendError(objResp, "service not found");
		return;
	}

	tblTask = XLog_TaskGetOne(pSvcDb, taskId);
	sTableName = xvoTableGetText(tblTask, "tableName", 9);
	if ( !sTableName || !sTableName[0] ) {
		xvoUnref(tblTask);
		XLog_SendError(objResp, "task not found");
		return;
	}

	if ( HttpMethodIs(objReq, "GET") ) {
		int64 lastId = XLog_ReadInt64Query(objReq, "lastId", 0);
		int iLimit = XLog_ReadIntQuery(objReq, "limit", 100);
		sqlite3_stmt* stmt = NULL;
		xvalue arrData = xvoCreateArray();
		str sql;
		int rc;

		if ( lastId > 0 ) {
			sql = xrtFormat("SELECT * FROM %s WHERE id > ? ORDER BY id ASC LIMIT ?;", sTableName);
			rc = sqlite3_prepare_v2(pSvcDb, sql, -1, &stmt, NULL);
			xrtFree(sql);
			if ( rc == SQLITE_OK ) {
				sqlite3_bind_int64(stmt, 1, lastId);
				sqlite3_bind_int(stmt, 2, iLimit);
			}
		} else {
			sql = xrtFormat("SELECT * FROM %s ORDER BY id ASC LIMIT ? OFFSET 0;", sTableName);
			rc = sqlite3_prepare_v2(pSvcDb, sql, -1, &stmt, NULL);
			xrtFree(sql);
			if ( rc == SQLITE_OK ) {
				sqlite3_bind_int(stmt, 1, iLimit);
			}
		}

		if ( (rc == SQLITE_OK) && stmt ) {
			while ( sqlite3_step(stmt) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
				{
					xtime iTime = sqlite3_column_int64(stmt, 1);
					xvoTableSetText(tblRow, "time", 4, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				}
				xvoTableSetText(tblRow, "class", 5, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
				xvoTableSetText(tblRow, "text", 4, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
				xvoArrayAppendValue(arrData, tblRow, TRUE);
			}
			sqlite3_finalize(stmt);
		}

		{
			xvalue tblRet = xvoCreateTable();
			xvoTableSetInt(tblRet, "code", 4, 0);
			xvoTableSetText(tblRet, "message", 7, (str)"success", 0, FALSE);
			xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
			XLog_SendJson(objResp, tblRet);
		}

	} else if ( HttpMethodIs(objReq, "DELETE") ) {
		sqlite3_stmt* stmt = NULL;
		str sql = xrtFormat("DELETE FROM %s;", sTableName);
		if ( sqlite3_prepare_v2(pSvcDb, sql, -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		xrtFree(sql);
		sql = xrtFormat("DELETE FROM sqlite_sequence WHERE name='%s';", sTableName);
		if ( sqlite3_prepare_v2(pSvcDb, sql, -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		xrtFree(sql);
		XLog_ReplyApiCode(objResp, 0, "success");

	} else {
		XLog_ReplyApiCode(objResp, 1, "method not allowed");
	}

	xvoUnref(tblTask);
}

void XLog_Req_ApiLogPush(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm;
	int64 serviceId, taskId, logId;
	str sClass, sText;
	sqlite3* pSvcDb;
	xvalue tblTask;
	str sTableName;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !HttpMethodIs(objReq, "POST") ) {
		XLog_ReplyApiCode(objResp, 1, "POST only");
		return;
	}

	tblForm = XLog_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		XLog_ReplyApiCode(objResp, 1, "invalid json");
		return;
	}

	serviceId = xvoTableGetInt(tblForm, "service", 7);
	taskId = xvoTableGetInt(tblForm, "task", 4);
	sClass = xvoTableGetText(tblForm, "class", 5);
	sText = xvoTableGetText(tblForm, "text", 4);

	if ( serviceId <= 0 || taskId <= 0 ) {
		xvoUnref(tblForm);
		XLog_ReplyApiCode(objResp, 1, "invalid service or task id");
		return;
	}

	pSvcDb = XLog_ServiceGetDB(serviceId);
	if ( pSvcDb == NULL ) {
		xvoUnref(tblForm);
		XLog_ReplyApiCode(objResp, 1, "service not found");
		return;
	}

	tblTask = XLog_TaskGetOne(pSvcDb, taskId);
	sTableName = xvoTableGetText(tblTask, "tableName", 9);
	if ( !sTableName || !sTableName[0] ) {
		xvoUnref(tblTask);
		xvoUnref(tblForm);
		XLog_ReplyApiCode(objResp, 1, "task not found");
		return;
	}

	logId = XLog_LogAdd(pSvcDb, sTableName, sClass, sText);
	xvoUnref(tblTask);
	xvoUnref(tblForm);

	{
		xvalue tblRet = xvoCreateTable();
		xvoTableSetInt(tblRet, "code", 4, 0);
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetText(tblRet, "message", 7, (str)"success", 0, FALSE);
		{
			xvalue tblData = xvoCreateTable();
			xvoTableSetInt(tblData, "id", 2, logId);
			xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
		}
		XLog_SendJson(objResp, tblRet);
	}
}

void XLog_Req_ApiTaskCreate(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm;
	int64 serviceId, taskId;
	str sName, sDesc;
	sqlite3* pSvcDb;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !HttpMethodIs(objReq, "POST") ) {
		XLog_ReplyApiCode(objResp, 1, "POST only");
		return;
	}

	tblForm = XLog_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		XLog_ReplyApiCode(objResp, 1, "invalid json");
		return;
	}

	serviceId = xvoTableGetInt(tblForm, "service", 7);
	sName = xvoTableGetText(tblForm, "name", 4);
	sDesc = xvoTableGetText(tblForm, "desc", 4);

	if ( serviceId <= 0 ) {
		xvoUnref(tblForm);
		XLog_ReplyApiCode(objResp, 1, "invalid service id");
		return;
	}
	if ( !sName || !sName[0] ) {
		xvoUnref(tblForm);
		XLog_ReplyApiCode(objResp, 1, "task name required");
		return;
	}

	pSvcDb = XLog_ServiceGetDB(serviceId);
	if ( pSvcDb == NULL ) {
		xvoUnref(tblForm);
		XLog_ReplyApiCode(objResp, 1, "service not found");
		return;
	}

	taskId = XLog_TaskCreate(pSvcDb, sName, sDesc ? sDesc : "");
	xvoUnref(tblForm);

	{
		xvalue tblRet = xvoCreateTable();
		xvoTableSetInt(tblRet, "code", 4, 0);
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetText(tblRet, "message", 7, (str)"success", 0, FALSE);
		{
			xvalue tblData = xvoCreateTable();
			xvoTableSetInt(tblData, "id", 2, taskId);
			xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
		}
		XLog_SendJson(objResp, tblRet);
	}
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
	XLog_GetLogsDir();
	return 0;
}

int XLog_OnStart(XAdminPluginHandle handle)
{
	XAdminMenuDecl menu;
	XAdminRouteDecl route;
	int iAuthGroupId = 0;
	int iAuthId = 0;
	XAdminAuthGroupDecl authGroup;
	XAdminAuthDecl auth;

	G_XLogHandle = handle;

	if ( !XLog_EnsureMainSchema() ) return -1;

	G_XLogServiceDbLock = xrtMutexCreate();
	G_XLogServiceDbCache = xrtDictCreate(sizeof(sqlite3*), XRT_OBJMODE_SHARED);
	XLog_ServiceLoadAllDB();

	memset(&authGroup, 0, sizeof(authGroup));
	authGroup.scope = XADMIN_AUTH_SCOPE_ADMIN;
	authGroup.name = "Log Service";
	authGroup.description = "Log service management permissions";
	authGroup.sort = 100000;
	if ( XAdmin_RegisterAuthGroup(handle, &authGroup, &iAuthGroupId, NULL) != 0 ) return -1;

	memset(&auth, 0, sizeof(auth));
	auth.scope = XADMIN_AUTH_SCOPE_ADMIN;
	auth.group_id = iAuthGroupId;
	auth.name = "xlogserver.manage";
	auth.description = "Manage log services and tasks";
	auth.sort = 100001;
	if ( XAdmin_RegisterAuth(handle, &auth, &iAuthId, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/xlogserver";
	route.proc = XLog_Req_ViewServices;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/xlogserver/services/add";
	route.proc = XLog_Req_ViewServicesAdd;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/xlogserver/services/edit";
	route.proc = XLog_Req_ViewServicesEdit;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/xlogserver/tasks";
	route.proc = XLog_Req_ViewTasks;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/xlogserver/tasks/add";
	route.proc = XLog_Req_ViewTasksAdd;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/xlogserver/tasks/edit";
	route.proc = XLog_Req_ViewTasksEdit;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/xlogserver/tasks/logs";
	route.proc = XLog_Req_ViewTasksLogs;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/xlogserver/services";
	route.proc = XLog_Req_ApiServices;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/xlogserver/tasks";
	route.proc = XLog_Req_ApiTasks;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/xlogserver/task/logs";
	route.proc = XLog_Req_ApiTaskLogs;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/api/v1/log/push";
	route.proc = XLog_Req_ApiLogPush;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/api/v1/task/create";
	route.proc = XLog_Req_ApiTaskCreate;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&menu, 0, sizeof(menu));
	menu.title = "Log Service";
	menu.icon = "layui-icon layui-icon-log";
	menu.type = 1;
	menu.open_type = "_iframe";
	menu.href = "/admin/view/plugin/xlogserver";
	menu.sort = 100;
	menu.visible = TRUE;
	menu.remark = "Multi-service log management";
	if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) return -1;

	printf("[xlogserver] started\n");
	return 0;
}

int XLog_OnConfigChanged(XAdminPluginHandle handle, xvalue new_cfg)
{
	(void)handle;
	memset(&G_XLogConfig, 0, sizeof(G_XLogConfig));
	G_XLogConfig.iMaxLogLines = 10000;
	G_XLogConfig.iDefaultRefreshInterval = 3000;
	if ( new_cfg && (xvoType(new_cfg) == XVO_DT_TABLE) ) {
		int64 iVal = xvoTableGetInt(new_cfg, "maxLogLines", 12);
		if ( iVal >= 100 ) G_XLogConfig.iMaxLogLines = (int)iVal;
		iVal = xvoTableGetInt(new_cfg, "defaultRefreshInterval", 22);
		if ( iVal >= 1000 ) G_XLogConfig.iDefaultRefreshInterval = (int)iVal;
	}
	return 0;
}

int XLog_OnHealthCheck(XAdminPluginHandle handle, XAdminHealthReport* out_report)
{
	(void)handle;
	if ( out_report ) {
		out_report->status_code = 0;
		out_report->message = "ok";
	}
	return 0;
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
	printf("[xlogserver] stopped\n");
}

void XLog_OnUnload(XAdminPluginHandle handle)
{
	(void)handle;
	G_XLogHandle = NULL;
	G_XLogMainDb = NULL;
	G_XLogXid = NULL;
	G_XLogRootPath = NULL;
	G_XLogDataPath = NULL;
	G_XLogPrivateDbPath = NULL;
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
