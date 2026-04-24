#include <xs_plugin.h>

#define GB_STATUS_PENDING  0
#define GB_STATUS_APPROVED 1
#define GB_STATUS_REJECTED 2

typedef struct {
	char sBoardTitle[128];
	char sBoardSubtitle[256];
	char sCategories[512];
	int iPageSize;
	int iMaxContentLength;
	int iMaxNicknameLength;
	bool bAllowReactions;
	bool bRequireApproval;
} GbConfig;

typedef struct {
	int iTotalMessages;
	int iApprovedMessages;
	int iPendingMessages;
	int iRejectedMessages;
	int iTotalReactions;
} GbStats;

typedef struct {
	const char* sNickname;
	const char* sContent;
	const char* sCategory;
	int iParentId;
} GbNewMessageEvent;

static XAdminPluginHandle G_Handle = NULL;
static const char* G_Xid = NULL;
static const char* G_RootPath = NULL;
static const char* G_PrivateDbPath = NULL;
static GbConfig G_Config = {
	"Message Board",
	"Share your thoughts with us",
	"General,Feedback,Question,Bug Report,Feature Request",
	15, 500, 32, TRUE, FALSE
};
static int G_EventCount = 0;

static const char* G_SchemaSql =
	"CREATE TABLE IF NOT EXISTS gbdemo_message ("
	"id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"nickname TEXT NOT NULL,"
	"content TEXT NOT NULL,"
	"category TEXT NOT NULL DEFAULT 'General',"
	"parent_id INTEGER NOT NULL DEFAULT 0,"
	"status INTEGER NOT NULL DEFAULT 1,"
	"likes INTEGER NOT NULL DEFAULT 0,"
	"reply TEXT DEFAULT '',"
	"reply_time INTEGER NOT NULL DEFAULT 0,"
	"create_time INTEGER NOT NULL,"
	"update_time INTEGER NOT NULL DEFAULT 0,"
	"ip_addr TEXT DEFAULT ''"
	");";

static const char* G_IndexSql =
	"CREATE INDEX IF NOT EXISTS idx_gbdemo_msg_status_create "
	"ON gbdemo_message(status, create_time DESC);"
	"CREATE INDEX IF NOT EXISTS idx_gbdemo_msg_parent "
	"ON gbdemo_message(parent_id);";

XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int idx, void* ptr)
{
	if ( idx == XADMIN_GLOBAL_PLUGIN_XID ) {
		G_Xid = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_ROOT_PATH ) {
		G_RootPath = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_PRIVATE_DB_PATH ) {
		G_PrivateDbPath = (const char*)ptr;
	}
}

void Gb_CopyText(char* sDest, size_t iCap, const char* sValue, const char* sFallback)
{
	if ( (sDest == NULL) || (iCap == 0) ) return;
	snprintf(sDest, iCap, "%s", (sValue && sValue[0]) ? sValue : (sFallback ? sFallback : ""));
}

int Gb_ClampInt(int iValue, int iMin, int iMax, int iDefault)
{
	if ( iValue < iMin ) return iDefault;
	if ( iValue > iMax ) return iMax;
	return iValue;
}

bool Gb_IsBlank(const char* sText)
{
	const unsigned char* p = (const unsigned char*)sText;
	if ( p == NULL ) return TRUE;
	while ( *p ) {
		if ( (*p != ' ') && (*p != '\t') && (*p != '\r') && (*p != '\n') ) return FALSE;
		p++;
	}
	return TRUE;
}

const char* Gb_StatusText(int iStatus)
{
	if ( iStatus == GB_STATUS_PENDING ) return "pending";
	if ( iStatus == GB_STATUS_APPROVED ) return "approved";
	if ( iStatus == GB_STATUS_REJECTED ) return "rejected";
	return "unknown";
}

bool Gb_OpenDb(sqlite3** ppDb)
{
	sqlite3* pDb = NULL;
	int iRet;
	if ( ppDb ) *ppDb = NULL;
	if ( (ppDb == NULL) || (G_PrivateDbPath == NULL) || (G_PrivateDbPath[0] == '\0') ) return FALSE;
	iRet = sqlite3_open_v2(G_PrivateDbPath, &pDb,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
	if ( iRet != SQLITE_OK ) {
		if ( pDb ) sqlite3_close(pDb);
		return FALSE;
	}
	sqlite3_busy_timeout(pDb, 3000);
	*ppDb = pDb;
	return TRUE;
}

void Gb_CloseDb(sqlite3* pDb)
{
	if ( pDb ) sqlite3_close(pDb);
}

bool Gb_TableHasColumn(sqlite3* pDb, const char* sCol)
{
	sqlite3_stmt* stmt = NULL;
	bool bFound = FALSE;
	if ( (pDb == NULL) || (sCol == NULL) ) return FALSE;
	if ( sqlite3_prepare_v2(pDb, "PRAGMA table_info(gbdemo_message)", -1, &stmt, NULL) != SQLITE_OK ) return FALSE;
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		const unsigned char* sName = sqlite3_column_text(stmt, 1);
		if ( sName && (strcmp((const char*)sName, sCol) == 0) ) { bFound = TRUE; break; }
	}
	sqlite3_finalize(stmt);
	return bFound;
}

bool Gb_EnsureColumn(sqlite3* pDb, const char* sCol, const char* sAlterSql)
{
	char* sErr = NULL;
	bool bOK = FALSE;
	if ( (pDb == NULL) || (sCol == NULL) || (sAlterSql == NULL) ) return FALSE;
	if ( Gb_TableHasColumn(pDb, sCol) ) return TRUE;
	if ( sqlite3_exec(pDb, sAlterSql, NULL, NULL, &sErr) == SQLITE_OK ) bOK = TRUE;
	if ( sErr ) sqlite3_free(sErr);
	return bOK;
}

bool Gb_EnsureSchema(void)
{
	sqlite3* pDb = NULL;
	char* sErr = NULL;
	if ( !Gb_OpenDb(&pDb) ) return FALSE;
	if ( sqlite3_exec(pDb, G_SchemaSql, NULL, NULL, &sErr) != SQLITE_OK ) {
		if ( sErr ) sqlite3_free(sErr);
		Gb_CloseDb(pDb);
		return FALSE;
	}
	if ( sErr ) { sqlite3_free(sErr); sErr = NULL; }

	Gb_EnsureColumn(pDb, "category", "ALTER TABLE gbdemo_message ADD COLUMN category TEXT NOT NULL DEFAULT 'General'");
	Gb_EnsureColumn(pDb, "parent_id", "ALTER TABLE gbdemo_message ADD COLUMN parent_id INTEGER NOT NULL DEFAULT 0");
	Gb_EnsureColumn(pDb, "likes", "ALTER TABLE gbdemo_message ADD COLUMN likes INTEGER NOT NULL DEFAULT 0");
	Gb_EnsureColumn(pDb, "ip_addr", "ALTER TABLE gbdemo_message ADD COLUMN ip_addr TEXT DEFAULT ''");
	Gb_EnsureColumn(pDb, "reply", "ALTER TABLE gbdemo_message ADD COLUMN reply TEXT DEFAULT ''");
	Gb_EnsureColumn(pDb, "reply_time", "ALTER TABLE gbdemo_message ADD COLUMN reply_time INTEGER NOT NULL DEFAULT 0");
	Gb_EnsureColumn(pDb, "update_time", "ALTER TABLE gbdemo_message ADD COLUMN update_time INTEGER NOT NULL DEFAULT 0");

	sqlite3_exec(pDb, G_IndexSql, NULL, NULL, &sErr);
	if ( sErr ) sqlite3_free(sErr);
	Gb_CloseDb(pDb);
	return TRUE;
}

xvalue Gb_CreateResult(bool bResult, const char* sMessage)
{
	xvalue tblRet = xvoCreateTable();
	if ( tblRet == NULL ) return NULL;
	xvoTableSetBool(tblRet, "result", 6, bResult);
	if ( sMessage ) xvoTableSetText(tblRet, "message", 7, (str)sMessage, 0, FALSE);
	return tblRet;
}

void Gb_SendJson(XS_ResponseObject objResp, xvalue objValue)
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

void Gb_SendError(XS_ResponseObject objResp, const char* sMessage)
{
	xvalue tblRet = Gb_CreateResult(FALSE, sMessage ? sMessage : "request failed");
	if ( tblRet == NULL ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: application/json\r\n", "{\"result\":false}", 0);
		return;
	}
	Gb_SendJson(objResp, tblRet);
}

void Gb_SendOk(XS_ResponseObject objResp, const char* sMessage)
{
	xvalue tblRet = Gb_CreateResult(TRUE, sMessage ? sMessage : "ok");
	if ( tblRet == NULL ) {
		xsHttpReplyAuto(objResp, 200, "Content-Type: application/json\r\n", "{\"result\":true}", 0);
		return;
	}
	Gb_SendJson(objResp, tblRet);
}

xvalue Gb_ParseBody(XS_RequestObject objReq)
{
	xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( (tblForm == NULL) || (xvoType(tblForm) != XVO_DT_TABLE) ) {
		if ( tblForm ) xvoUnref(tblForm);
		return NULL;
	}
	return tblForm;
}

int Gb_ReadIntQuery(XS_RequestObject objReq, const char* sName, int iDefault)
{
	char sValue[32];
	memset(sValue, 0, sizeof(sValue));
	xsReqQueryValue(objReq, sName, sValue, sizeof(sValue));
	if ( sValue[0] == '\0' ) return iDefault;
	return atoi(sValue);
}

void Gb_SetTimeText(xvalue tblItem, const char* sKey, int iKeyLen, xtime iTime)
{
	str sValue = (iTime > 0) ? xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME) : xrtCopyStr("", 0);
	xvoTableSetText(tblItem, sKey, iKeyLen, sValue ? sValue : (str)"", 0, TRUE);
}

void Gb_AppendRow(xvalue arrList, sqlite3_stmt* stmt)
{
	xvalue tblItem = xvoCreateTable();
	if ( (arrList == NULL) || (stmt == NULL) || (tblItem == NULL) ) {
		if ( tblItem ) xvoUnref(tblItem);
		return;
	}

	int64 iId = sqlite3_column_int64(stmt, 0);
	const unsigned char* sNick = sqlite3_column_text(stmt, 1);
	const unsigned char* sContent = sqlite3_column_text(stmt, 2);
	const unsigned char* sCategory = sqlite3_column_text(stmt, 3);
	int64 iParentId = sqlite3_column_int64(stmt, 4);
	int iStatus = sqlite3_column_int(stmt, 5);
	int iLikes = sqlite3_column_int(stmt, 6);
	const unsigned char* sReply = sqlite3_column_text(stmt, 7);
	xtime iReplyTime = sqlite3_column_int64(stmt, 8);
	xtime iCreateTime = sqlite3_column_int64(stmt, 9);
	xtime iUpdateTime = sqlite3_column_int64(stmt, 10);
	const unsigned char* sIpAddr = sqlite3_column_text(stmt, 11);

	xvoTableSetInt(tblItem, "id", 2, iId);
	xvoTableSetText(tblItem, "nickname", 8, (str)(sNick ? sNick : (const unsigned char*)""), 0, FALSE);
	xvoTableSetText(tblItem, "content", 7, (str)(sContent ? sContent : (const unsigned char*)""), 0, FALSE);
	xvoTableSetText(tblItem, "category", 8, (str)(sCategory ? sCategory : (const unsigned char*)"General"), 0, FALSE);
	xvoTableSetInt(tblItem, "parentId", 8, iParentId);
	xvoTableSetInt(tblItem, "status", 6, iStatus);
	xvoTableSetText(tblItem, "statusText", 10, (str)Gb_StatusText(iStatus), 0, FALSE);
	xvoTableSetInt(tblItem, "likes", 5, iLikes);
	xvoTableSetText(tblItem, "reply", 5, (str)(sReply ? sReply : (const unsigned char*)""), 0, FALSE);
	xvoTableSetInt(tblItem, "replyTime", 9, iReplyTime);
	xvoTableSetInt(tblItem, "createTime", 10, iCreateTime);
	xvoTableSetInt(tblItem, "updateTime", 10, iUpdateTime > 0 ? iUpdateTime : iCreateTime);
	Gb_SetTimeText(tblItem, "createTimeText", 14, iCreateTime);
	Gb_SetTimeText(tblItem, "updateTimeText", 14, iUpdateTime > 0 ? iUpdateTime : iCreateTime);
	Gb_SetTimeText(tblItem, "replyTimeText", 13, iReplyTime);
	xvoTableSetText(tblItem, "ipAddr", 6, (str)(sIpAddr ? sIpAddr : (const unsigned char*)""), 0, FALSE);
	xvoArrayAppendValue(arrList, tblItem, TRUE);
}

xvalue Gb_QueryMessages(bool bAdmin, int iStatusFilter, const char* sCategory, int iParentId, int iPage, int iLimit, int* piTotal)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrList = NULL;
	str sCountSql = NULL;
	str sQuerySql = NULL;
	str sWhere = xrtCopyStr("", 0);
	bool bHasWhere = FALSE;

	if ( piTotal ) *piTotal = 0;
	if ( !Gb_OpenDb(&pDb) ) {
		if ( sWhere ) xrtFree(sWhere);
		return NULL;
	}

	if ( !bAdmin ) {
		sWhere = xrtFormat("%sWHERE status = %d", sWhere, GB_STATUS_APPROVED);
		bHasWhere = TRUE;
	} else if ( iStatusFilter >= 0 && iStatusFilter <= 2 ) {
		sWhere = xrtFormat("%sWHERE status = %d", sWhere, iStatusFilter);
		bHasWhere = TRUE;
	}

	if ( sCategory && sCategory[0] ) {
		str sOld = sWhere;
		if ( bHasWhere ) {
			sWhere = xrtFormat("%s AND category = ?", sWhere);
		} else {
			sWhere = xrtFormat("%sWHERE category = ?", sWhere);
			bHasWhere = TRUE;
		}
		xrtFree(sOld);
	}

	if ( iParentId >= 0 ) {
		str sOld = sWhere;
		if ( bHasWhere ) {
			sWhere = xrtFormat("%s AND parent_id = %d", sWhere, iParentId);
		} else {
			sWhere = xrtFormat("%sWHERE parent_id = %d", sWhere, iParentId);
			bHasWhere = TRUE;
		}
		xrtFree(sOld);
	}

	sCountSql = xrtFormat("SELECT COUNT(1) FROM gbdemo_message %s", sWhere);
	if ( (sCountSql == NULL) || (sqlite3_prepare_v2(pDb, sCountSql, -1, &stmt, NULL) != SQLITE_OK) ) {
		if ( sCountSql ) xrtFree(sCountSql);
		xrtFree(sWhere);
		Gb_CloseDb(pDb);
		return NULL;
	}
	if ( sCategory && sCategory[0] ) {
		sqlite3_bind_text(stmt, 1, sCategory, -1, SQLITE_TRANSIENT);
	}
	if ( sqlite3_step(stmt) == SQLITE_ROW && piTotal ) {
		*piTotal = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	stmt = NULL;
	xrtFree(sCountSql);

	arrList = xvoCreateArray();
	if ( arrList == NULL ) {
		xrtFree(sWhere);
		Gb_CloseDb(pDb);
		return NULL;
	}

	sQuerySql = xrtFormat(
		"SELECT id, nickname, content, category, parent_id, status, likes, reply, reply_time, create_time, update_time, ip_addr "
		"FROM gbdemo_message %s ORDER BY id DESC LIMIT ? OFFSET ?", sWhere);
	if ( (sQuerySql == NULL) || (sqlite3_prepare_v2(pDb, sQuerySql, -1, &stmt, NULL) != SQLITE_OK) ) {
		if ( sQuerySql ) xrtFree(sQuerySql);
		xvoUnref(arrList);
		xrtFree(sWhere);
		Gb_CloseDb(pDb);
		return NULL;
	}

	int iParamIdx = 1;
	if ( sCategory && sCategory[0] ) {
		sqlite3_bind_text(stmt, iParamIdx++, sCategory, -1, SQLITE_TRANSIENT);
	}
	sqlite3_bind_int(stmt, iParamIdx++, iLimit);
	sqlite3_bind_int(stmt, iParamIdx, (iPage - 1) * iLimit);

	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		Gb_AppendRow(arrList, stmt);
	}

	sqlite3_finalize(stmt);
	xrtFree(sQuerySql);
	xrtFree(sWhere);
	Gb_CloseDb(pDb);
	return arrList;
}

xvalue Gb_BuildListResponse(bool bAdmin, int iStatusFilter, const char* sCategory, int iParentId, int iPage, int iLimit)
{
	int iTotal = 0;
	xvalue arrList = Gb_QueryMessages(bAdmin, iStatusFilter, sCategory, iParentId, iPage, iLimit, &iTotal);
	if ( arrList == NULL ) return NULL;

	xvalue tblRet = Gb_CreateResult(TRUE, NULL);
	if ( tblRet == NULL ) { xvoUnref(arrList); return NULL; }

	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	xvoTableSetInt(tblRet, "page", 4, iPage);
	xvoTableSetInt(tblRet, "pageSize", 8, iLimit);
	xvoTableSetInt(tblRet, "total", 5, iTotal);
	return tblRet;
}

bool Gb_InsertMessage(const char* sNickname, const char* sContent, const char* sCategory, int iParentId, int iStatus, const char* sIp)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xtime iNow;
	bool bOK = FALSE;

	if ( !Gb_OpenDb(&pDb) ) return FALSE;
	if ( sqlite3_prepare_v2(pDb,
		"INSERT INTO gbdemo_message (nickname, content, category, parent_id, status, likes, reply, reply_time, create_time, update_time, ip_addr) "
		"VALUES (?, ?, ?, ?, ?, 0, '', 0, ?, ?, ?)",
		-1, &stmt, NULL) != SQLITE_OK ) {
		Gb_CloseDb(pDb);
		return FALSE;
	}

	iNow = xrtNow();
	sqlite3_bind_text(stmt, 1, sNickname ? sNickname : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, sContent ? sContent : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, sCategory ? sCategory : "General", -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 4, iParentId);
	sqlite3_bind_int(stmt, 5, iStatus);
	sqlite3_bind_int64(stmt, 6, iNow);
	sqlite3_bind_int64(stmt, 7, iNow);
	sqlite3_bind_text(stmt, 8, sIp ? sIp : "", -1, SQLITE_TRANSIENT);
	bOK = (sqlite3_step(stmt) == SQLITE_DONE);

	sqlite3_finalize(stmt);
	Gb_CloseDb(pDb);
	return bOK;
}

bool Gb_UpdateMessage(int64 iId, bool bHasStatus, int iStatus, bool bHasReply, const char* sReply)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	const char* sSql = NULL;
	xtime iNow;
	xtime iReplyTime;
	bool bOK = FALSE;

	if ( (iId <= 0) || (!bHasStatus && !bHasReply) ) return FALSE;
	if ( !Gb_OpenDb(&pDb) ) return FALSE;

	if ( bHasStatus && bHasReply ) {
		sSql = "UPDATE gbdemo_message SET status = ?, reply = ?, reply_time = ?, update_time = ? WHERE id = ?";
	} else if ( bHasStatus ) {
		sSql = "UPDATE gbdemo_message SET status = ?, update_time = ? WHERE id = ?";
	} else {
		sSql = "UPDATE gbdemo_message SET reply = ?, reply_time = ?, update_time = ? WHERE id = ?";
	}
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) != SQLITE_OK ) {
		Gb_CloseDb(pDb);
		return FALSE;
	}

	iNow = xrtNow();
	iReplyTime = (bHasReply && sReply && sReply[0]) ? iNow : 0;
	if ( bHasStatus && bHasReply ) {
		sqlite3_bind_int(stmt, 1, iStatus);
		sqlite3_bind_text(stmt, 2, sReply ? sReply : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 3, iReplyTime);
		sqlite3_bind_int64(stmt, 4, iNow);
		sqlite3_bind_int64(stmt, 5, iId);
	} else if ( bHasStatus ) {
		sqlite3_bind_int(stmt, 1, iStatus);
		sqlite3_bind_int64(stmt, 2, iNow);
		sqlite3_bind_int64(stmt, 3, iId);
	} else {
		sqlite3_bind_text(stmt, 1, sReply ? sReply : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, iReplyTime);
		sqlite3_bind_int64(stmt, 3, iNow);
		sqlite3_bind_int64(stmt, 4, iId);
	}

	if ( sqlite3_step(stmt) == SQLITE_DONE ) bOK = sqlite3_changes(pDb) > 0;
	sqlite3_finalize(stmt);
	Gb_CloseDb(pDb);
	return bOK;
}

bool Gb_DeleteMessage(int64 iId)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	bool bOK = FALSE;

	if ( (iId <= 0) || !Gb_OpenDb(&pDb) ) return FALSE;

	{
		sqlite3_stmt* stmtDel = NULL;
		if ( sqlite3_prepare_v2(pDb, "DELETE FROM gbdemo_message WHERE parent_id = ?", -1, &stmtDel, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmtDel, 1, iId);
			sqlite3_step(stmtDel);
			sqlite3_finalize(stmtDel);
		}
	}

	if ( sqlite3_prepare_v2(pDb, "DELETE FROM gbdemo_message WHERE id = ?", -1, &stmt, NULL) != SQLITE_OK ) {
		Gb_CloseDb(pDb);
		return FALSE;
	}
	sqlite3_bind_int64(stmt, 1, iId);
	if ( sqlite3_step(stmt) == SQLITE_DONE ) bOK = sqlite3_changes(pDb) > 0;
	sqlite3_finalize(stmt);
	Gb_CloseDb(pDb);
	return bOK;
}

bool Gb_IncrementLikes(int64 iId)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	bool bOK = FALSE;

	if ( (iId <= 0) || !Gb_OpenDb(&pDb) ) return FALSE;
	if ( sqlite3_prepare_v2(pDb,
		"UPDATE gbdemo_message SET likes = likes + 1, update_time = ? WHERE id = ? AND status = 1",
		-1, &stmt, NULL) != SQLITE_OK ) {
		Gb_CloseDb(pDb);
		return FALSE;
	}
	sqlite3_bind_int64(stmt, 1, xrtNow());
	sqlite3_bind_int64(stmt, 2, iId);
	if ( sqlite3_step(stmt) == SQLITE_DONE ) bOK = sqlite3_changes(pDb) > 0;
	sqlite3_finalize(stmt);
	Gb_CloseDb(pDb);
	return bOK;
}

bool Gb_GetStats(GbStats* pStats)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;

	if ( (pStats == NULL) || !Gb_OpenDb(&pDb) ) return FALSE;
	memset(pStats, 0, sizeof(GbStats));

	if ( sqlite3_prepare_v2(pDb,
		"SELECT "
		"COUNT(1), "
		"SUM(CASE WHEN status=1 THEN 1 ELSE 0 END), "
		"SUM(CASE WHEN status=0 THEN 1 ELSE 0 END), "
		"SUM(CASE WHEN status=2 THEN 1 ELSE 0 END), "
		"SUM(likes) "
		"FROM gbdemo_message",
		-1, &stmt, NULL) != SQLITE_OK ) {
		Gb_CloseDb(pDb);
		return FALSE;
	}
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		pStats->iTotalMessages = sqlite3_column_int(stmt, 0);
		pStats->iApprovedMessages = sqlite3_column_int(stmt, 1);
		pStats->iPendingMessages = sqlite3_column_int(stmt, 2);
		pStats->iRejectedMessages = sqlite3_column_int(stmt, 3);
		pStats->iTotalReactions = sqlite3_column_int(stmt, 4);
	}
	sqlite3_finalize(stmt);
	Gb_CloseDb(pDb);
	return TRUE;
}

bool Gb_SendAssetHtml(XS_ResponseObject objResp, const char* sFileName)
{
	str sPath;
	ptr pData;
	size_t iSize = 0;
	if ( (G_RootPath == NULL) || (sFileName == NULL) ) return FALSE;
	sPath = xrtPathJoin(2, G_RootPath, (str)sFileName);
	if ( (sPath == NULL) || !xrtFileExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		return FALSE;
	}
	pData = xrtFileGetAll(sPath, &iSize);
	xrtFree(sPath);
	if ( pData == NULL ) return FALSE;
	xsHttpReplyAuto(objResp, 200, "Content-Type: text/html; charset=utf-8\r\n", pData, iSize);
	xrtFree(pData);
	return TRUE;
}

void Gb_RequestMeta(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblRet;
	xvalue tblData;

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;

	tblRet = Gb_CreateResult(TRUE, NULL);
	tblData = xvoCreateTable();
	if ( (tblRet == NULL) || (tblData == NULL) ) {
		if ( tblRet ) xvoUnref(tblRet);
		if ( tblData ) xvoUnref(tblData);
		Gb_SendError(objResp, "failed to build response");
		return;
	}

	xvoTableSetText(tblData, "title", 5, G_Config.sBoardTitle, 0, FALSE);
	xvoTableSetText(tblData, "subtitle", 8, G_Config.sBoardSubtitle, 0, FALSE);
	xvoTableSetText(tblData, "categories", 10, G_Config.sCategories, 0, FALSE);
	xvoTableSetInt(tblData, "pageSize", 8, G_Config.iPageSize);
	xvoTableSetInt(tblData, "maxContentLength", 16, G_Config.iMaxContentLength);
	xvoTableSetInt(tblData, "maxNicknameLength", 17, G_Config.iMaxNicknameLength);
	xvoTableSetBool(tblData, "allowReactions", 14, G_Config.bAllowReactions);
	xvoTableSetBool(tblData, "requireApproval", 15, G_Config.bRequireApproval);
	xvoTableSetText(tblData, "xid", 3, (str)(G_Xid ? G_Xid : "gbdemo"), 0, FALSE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	Gb_SendJson(objResp, tblRet);
}

void Gb_RequestStats(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	GbStats stats;
	xvalue tblRet;
	xvalue tblData;

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;

	tblRet = Gb_CreateResult(TRUE, NULL);
	if ( tblRet == NULL ) { Gb_SendError(objResp, "failed"); return; }

	if ( Gb_GetStats(&stats) ) {
		tblData = xvoCreateTable();
		if ( tblData ) {
			xvoTableSetInt(tblData, "total", 5, stats.iTotalMessages);
			xvoTableSetInt(tblData, "approved", 8, stats.iApprovedMessages);
			xvoTableSetInt(tblData, "pending", 7, stats.iPendingMessages);
			xvoTableSetInt(tblData, "rejected", 8, stats.iRejectedMessages);
			xvoTableSetInt(tblData, "reactions", 9, stats.iTotalReactions);
			xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
		}
	}
	Gb_SendJson(objResp, tblRet);
}

void Gb_RequestListPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int iPage, iLimit;
	char sCategory[64];
	xvalue tblRet;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) { Gb_SendError(objResp, "method not allowed"); return; }

	iPage = Gb_ClampInt(Gb_ReadIntQuery(objReq, "page", 1), 1, 1000000, 1);
	iLimit = Gb_ClampInt(Gb_ReadIntQuery(objReq, "limit", G_Config.iPageSize), 1, 100, G_Config.iPageSize);
	memset(sCategory, 0, sizeof(sCategory));
	xsReqQueryValue(objReq, "category", sCategory, sizeof(sCategory));

	tblRet = Gb_BuildListResponse(FALSE, -1, sCategory[0] ? sCategory : NULL, -1, iPage, iLimit);
	if ( tblRet == NULL ) { Gb_SendError(objResp, "failed to load messages"); return; }
	Gb_SendJson(objResp, tblRet);
}

void Gb_RequestListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int iPage, iLimit, iStatusFilter;
	char sCategory[64];
	xvalue tblRet;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) { Gb_SendError(objResp, "method not allowed"); return; }

	iPage = Gb_ClampInt(Gb_ReadIntQuery(objReq, "page", 1), 1, 1000000, 1);
	iLimit = Gb_ClampInt(Gb_ReadIntQuery(objReq, "limit", G_Config.iPageSize), 1, 100, G_Config.iPageSize);
	iStatusFilter = Gb_ReadIntQuery(objReq, "status", -1);
	memset(sCategory, 0, sizeof(sCategory));
	xsReqQueryValue(objReq, "category", sCategory, sizeof(sCategory));

	if ( (iStatusFilter < -1) || (iStatusFilter > 2) ) iStatusFilter = -1;

	tblRet = Gb_BuildListResponse(TRUE, iStatusFilter, sCategory[0] ? sCategory : NULL, -1, iPage, iLimit);
	if ( tblRet == NULL ) { Gb_SendError(objResp, "failed to load messages"); return; }
	Gb_SendJson(objResp, tblRet);
}

void Gb_RequestAdd(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm;
	str sNickname, sContent, sCategory;
	int iParentId;
	int iStatus;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) { Gb_SendError(objResp, "method not allowed"); return; }

	tblForm = Gb_ParseBody(objReq);
	if ( tblForm == NULL ) { Gb_SendError(objResp, "invalid json body"); return; }

	sNickname = xvoTableGetText(tblForm, "nickname", 8);
	sContent = xvoTableGetText(tblForm, "content", 7);
	sCategory = xvoTableGetText(tblForm, "category", 8);
	iParentId = (int)xvoTableGetInt(tblForm, "parentId", 8);

	if ( Gb_IsBlank(sNickname) ) { xvoUnref(tblForm); Gb_SendError(objResp, "nickname is required"); return; }
	if ( Gb_IsBlank(sContent) ) { xvoUnref(tblForm); Gb_SendError(objResp, "content is required"); return; }
	if ( (int)strlen(sNickname) > G_Config.iMaxNicknameLength ) { xvoUnref(tblForm); Gb_SendError(objResp, "nickname too long"); return; }
	if ( (int)strlen(sContent) > G_Config.iMaxContentLength ) { xvoUnref(tblForm); Gb_SendError(objResp, "content too long"); return; }

	iStatus = G_Config.bRequireApproval ? GB_STATUS_PENDING : GB_STATUS_APPROVED;
	if ( !Gb_InsertMessage(sNickname, sContent, sCategory, iParentId, iStatus, xsReqRemote(objReq)) ) {
		xvoUnref(tblForm);
		Gb_SendError(objResp, "failed to save message");
		return;
	}

	if ( G_Handle ) {
		GbNewMessageEvent evt;
		evt.sNickname = sNickname;
		evt.sContent = sContent;
		evt.sCategory = sCategory ? sCategory : "General";
		evt.iParentId = iParentId;
		XAdmin_EmitEvent(G_Handle, "gbdemo.message.posted", &evt, sizeof(evt));
	}

	xvoUnref(tblForm);
	Gb_SendOk(objResp, G_Config.bRequireApproval ? "message submitted for review" : "message posted");
}

void Gb_RequestUpdate(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm;
	int64 iId;
	int iStatus = GB_STATUS_APPROVED;
	bool bHasStatus, bHasReply;
	str sReply = NULL;
	str sReplyCopy = NULL;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) { Gb_SendError(objResp, "method not allowed"); return; }

	tblForm = Gb_ParseBody(objReq);
	if ( tblForm == NULL ) { Gb_SendError(objResp, "invalid json body"); return; }

	iId = xvoTableGetInt(tblForm, "id", 2);
	bHasStatus = xvoTableExists(tblForm, "status", 6);
	bHasReply = xvoTableExists(tblForm, "reply", 5);
	if ( bHasStatus ) {
		iStatus = (int)xvoTableGetInt(tblForm, "status", 6);
		if ( (iStatus < 0) || (iStatus > 2) ) { xvoUnref(tblForm); Gb_SendError(objResp, "invalid status"); return; }
	}
	if ( bHasReply ) {
		sReply = xvoTableGetText(tblForm, "reply", 5);
		sReplyCopy = xrtCopyStr(sReply ? sReply : (str)"", 0);
	}
	xvoUnref(tblForm);

	if ( iId <= 0 ) { if ( sReplyCopy ) xrtFree(sReplyCopy); Gb_SendError(objResp, "invalid id"); return; }
	if ( !Gb_UpdateMessage(iId, bHasStatus, iStatus, bHasReply, sReplyCopy ? sReplyCopy : (str)"") ) {
		if ( sReplyCopy ) xrtFree(sReplyCopy);
		Gb_SendError(objResp, "failed to update message");
		return;
	}
	if ( sReplyCopy ) xrtFree(sReplyCopy);
	Gb_SendOk(objResp, "message updated");
}

void Gb_RequestDelete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm;
	int64 iId;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) { Gb_SendError(objResp, "method not allowed"); return; }

	tblForm = Gb_ParseBody(objReq);
	if ( tblForm == NULL ) { Gb_SendError(objResp, "invalid json body"); return; }

	iId = xvoTableGetInt(tblForm, "id", 2);
	xvoUnref(tblForm);

	if ( iId <= 0 ) { Gb_SendError(objResp, "invalid id"); return; }
	if ( !Gb_DeleteMessage(iId) ) { Gb_SendError(objResp, "failed to delete message"); return; }
	Gb_SendOk(objResp, "message deleted");
}

void Gb_RequestLike(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblForm;
	int64 iId;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !G_Config.bAllowReactions ) { Gb_SendError(objResp, "reactions disabled"); return; }
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) { Gb_SendError(objResp, "method not allowed"); return; }

	tblForm = Gb_ParseBody(objReq);
	if ( tblForm == NULL ) { Gb_SendError(objResp, "invalid json body"); return; }

	iId = xvoTableGetInt(tblForm, "id", 2);
	xvoUnref(tblForm);

	if ( iId <= 0 ) { Gb_SendError(objResp, "invalid id"); return; }
	if ( !Gb_IncrementLikes(iId) ) { Gb_SendError(objResp, "failed to like message"); return; }
	Gb_SendOk(objResp, "liked");
}

void Gb_RequestPublicView(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Gb_SendAssetHtml(objResp, "public.html") ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "gbdemo public page missing", 0);
	}
}

void Gb_RequestAdminView(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !Gb_SendAssetHtml(objResp, "admin.html") ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "gbdemo admin page missing", 0);
	}
}

void Gb_OnMessageEvent(const char* event_name, void* payload, size_t payload_size)
{
	(void)payload; (void)payload_size;
	G_EventCount++;
	printf("[gbdemo] event observed: %s (total events: %d)\n", event_name ? event_name : "(null)", G_EventCount);
}

int Gb_OnContentFilterHook(const char* hook_name, void* payload, size_t payload_size)
{
	(void)hook_name; (void)payload; (void)payload_size;
	return XADMIN_HOOK_CONTINUE;
}

void Gb_ConfigReset(void)
{
	memset(&G_Config, 0, sizeof(G_Config));
	snprintf(G_Config.sBoardTitle, sizeof(G_Config.sBoardTitle), "%s", "Message Board");
	snprintf(G_Config.sBoardSubtitle, sizeof(G_Config.sBoardSubtitle), "%s", "Share your thoughts with us");
	snprintf(G_Config.sCategories, sizeof(G_Config.sCategories), "%s", "General,Feedback,Question,Bug Report,Feature Request");
	G_Config.iPageSize = 15;
	G_Config.iMaxContentLength = 500;
	G_Config.iMaxNicknameLength = 32;
	G_Config.bAllowReactions = TRUE;
	G_Config.bRequireApproval = FALSE;
}

int Gb_OnLoad(XAdminPluginHandle* out_handle)
{
	if ( out_handle ) G_Handle = *out_handle;
	G_EventCount = 0;
	printf("[gbdemo] loaded\n");
	return 0;
}

int Gb_OnStart(XAdminPluginHandle handle)
{
	XAdminMenuDecl menu;
	XAdminRouteDecl route;
	XAdminAuthGroupDecl authGroup;
	XAdminAuthDecl auth;
	XAdminUriAuthDecl uriAuth;
	XAdminEventDecl eventDecl;
	XAdminHookDecl hookDecl;
	int iAuthGroupId = 0;
	int iAuthId = 0;

	G_Handle = handle;
	if ( !Gb_EnsureSchema() ) {
		printf("[gbdemo] schema init failed\n");
		return -1;
	}

	memset(&authGroup, 0, sizeof(authGroup));
	authGroup.scope = XADMIN_AUTH_SCOPE_ADMIN;
	authGroup.name = "Guestboard Demo";
	authGroup.description = "Permissions for gbdemo plugin";
	authGroup.sort = 990020;
	if ( XAdmin_RegisterAuthGroup(handle, &authGroup, &iAuthGroupId, NULL) != 0 ) return -1;

	memset(&auth, 0, sizeof(auth));
	auth.scope = XADMIN_AUTH_SCOPE_ADMIN;
	auth.group_id = iAuthGroupId;
	auth.name = "gbdemo.plugin.manage";
	auth.description = "Manage gbdemo plugin";
	auth.sort = 990021;
	if ( XAdmin_RegisterAuth(handle, &auth, &iAuthId, NULL) != 0 ) return -1;

	memset(&eventDecl, 0, sizeof(eventDecl));
	eventDecl.event_name = "gbdemo.message.posted";
	eventDecl.proc = Gb_OnMessageEvent;
	if ( XAdmin_ListenEvent(handle, &eventDecl, NULL) != 0 ) return -1;

	memset(&hookDecl, 0, sizeof(hookDecl));
	hookDecl.hook_name = "gbdemo.content.filter";
	hookDecl.sort = 100;
	hookDecl.proc = Gb_OnContentFilterHook;
	if ( XAdmin_RegisterHook(handle, &hookDecl, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/api/plugin/gbdemo/meta";
	route.proc = Gb_RequestMeta;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/api/plugin/gbdemo/stats";
	route.proc = Gb_RequestStats;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/api/plugin/gbdemo/list";
	route.proc = Gb_RequestListPublic;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/api/plugin/gbdemo/add";
	route.proc = Gb_RequestAdd;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/api/plugin/gbdemo/like";
	route.proc = Gb_RequestLike;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/plugin/gbdemo";
	route.proc = Gb_RequestPublicView;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/gbdemo/list";
	route.proc = Gb_RequestListAdmin;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/gbdemo/update";
	route.proc = Gb_RequestUpdate;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/gbdemo/delete";
	route.proc = Gb_RequestDelete;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/gbdemo/stats";
	route.proc = Gb_RequestStats;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/gbdemo";
	route.proc = Gb_RequestAdminView;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&uriAuth, 0, sizeof(uriAuth));
	uriAuth.scope = XADMIN_AUTH_SCOPE_ADMIN;
	uriAuth.auth_id = iAuthId;
	uriAuth.uri = "/admin/view/plugin/gbdemo";
	uriAuth.description = "gbdemo admin page";
	uriAuth.sort = 990021;
	uriAuth.need_auth = TRUE;
	if ( XAdmin_RegisterUriAuth(handle, &uriAuth, NULL, NULL) != 0 ) return -1;

	memset(&menu, 0, sizeof(menu));
	menu.title = "Message Board";
	menu.icon = "layui-icon layui-icon-dialogue";
	menu.type = 1;
	menu.open_type = "_iframe";
	menu.href = "/admin/view/plugin/gbdemo";
	menu.sort = 990020;
	menu.visible = TRUE;
	menu.remark = "Threaded guestbook demo";
	if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) return -1;

	printf("[gbdemo] started\n");
	return 0;
}

int Gb_OnConfigChanged(XAdminPluginHandle handle, xvalue new_cfg)
{
	(void)handle;
	Gb_ConfigReset();
	if ( new_cfg && (xvoType(new_cfg) == XVO_DT_TABLE) ) {
		Gb_CopyText(G_Config.sBoardTitle, sizeof(G_Config.sBoardTitle), xvoTableGetText(new_cfg, "boardTitle", 9), "Message Board");
		Gb_CopyText(G_Config.sBoardSubtitle, sizeof(G_Config.sBoardSubtitle), xvoTableGetText(new_cfg, "boardSubtitle", 12), "Share your thoughts with us");
		Gb_CopyText(G_Config.sCategories, sizeof(G_Config.sCategories), xvoTableGetText(new_cfg, "categories", 10), "General,Feedback,Question,Bug Report,Feature Request");
		G_Config.iPageSize = Gb_ClampInt((int)xvoTableGetInt(new_cfg, "pageSize", 8), 1, 100, 15);
		G_Config.iMaxContentLength = Gb_ClampInt((int)xvoTableGetInt(new_cfg, "maxContentLength", 16), 20, 8192, 500);
		G_Config.iMaxNicknameLength = Gb_ClampInt((int)xvoTableGetInt(new_cfg, "maxNicknameLength", 17), 2, 128, 32);
		G_Config.bAllowReactions = xvoTableGetBool(new_cfg, "allowReactions", 14);
		G_Config.bRequireApproval = xvoTableGetBool(new_cfg, "requireApproval", 15);
	}
	return 0;
}

int Gb_OnHealthCheck(XAdminPluginHandle handle, XAdminHealthReport* out_report)
{
	bool bHealthy;
	(void)handle;
	bHealthy = Gb_EnsureSchema();
	if ( out_report ) {
		out_report->status_code = bHealthy ? 0 : -1;
		out_report->message = bHealthy ? "ok" : "db unavailable";
	}
	return bHealthy ? 0 : -1;
}

void Gb_OnStop(XAdminPluginHandle handle)
{
	(void)handle;
	G_Handle = NULL;
	printf("[gbdemo] stopping\n");
}

void Gb_OnUnload(XAdminPluginHandle handle)
{
	(void)handle;
	G_Handle = NULL;
	G_Xid = NULL;
	G_RootPath = NULL;
	G_PrivateDbPath = NULL;
	G_EventCount = 0;
	Gb_ConfigReset();
	printf("[gbdemo] unloaded\n");
}

static XAdminPluginDescriptor G_Plugin = {
	XADMIN_ABI_VERSION,
	sizeof(XAdminPluginDescriptor),
	"gbdemo",
	"1.0.0",
	"Guestboard Demo",
	Gb_OnLoad,
	NULL,
	Gb_OnStart,
	Gb_OnConfigChanged,
	Gb_OnHealthCheck,
	Gb_OnStop,
	Gb_OnUnload
};

XADMIN_DECLARE_PLUGIN(G_Plugin)
