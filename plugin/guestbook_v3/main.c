#include <xs_plugin.h>
#include "value_util.h"
#include "util.h"
#include <sqlite3.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define GUESTBOOK_STATUS_PENDING 0
#define GUESTBOOK_STATUS_APPROVED 1
#define GUESTBOOK_STATUS_REJECTED 2

typedef struct {
	char sTitle[128];
	char sIntro[256];
	int iPageSize;
	int iMaxContentLength;
	bool bRequireApproval;
} GuestbookConfigState;

static XAdminPluginHandle G_GuestbookHandle = NULL;
static const char* G_GuestbookXid = NULL;
static const char* G_GuestbookRootPath = NULL;
static const char* G_GuestbookPrivateDbPath = NULL;
static GuestbookConfigState G_GuestbookConfig = {
	"Guestbook",
	"Leave a message for the team.",
	20,
	280,
	false
};

static const char* G_GuestbookSchemaSql =
	"CREATE TABLE IF NOT EXISTS guestbook_message ("
	"id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"nickname TEXT NOT NULL,"
	"content TEXT NOT NULL,"
	"status INTEGER NOT NULL DEFAULT 1,"
	"reply TEXT DEFAULT '',"
	"reply_time INTEGER NOT NULL DEFAULT 0,"
	"create_time INTEGER NOT NULL,"
	"update_time INTEGER NOT NULL DEFAULT 0"
	");";

static const char* G_GuestbookIndexSql =
	"CREATE INDEX IF NOT EXISTS idx_guestbook_message_status_create_time "
	"ON guestbook_message(status, create_time DESC);";

XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int idx, void* ptr)
{
	if ( idx == XADMIN_GLOBAL_PLUGIN_XID ) {
		G_GuestbookXid = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_ROOT_PATH ) {
		G_GuestbookRootPath = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_PRIVATE_DB_PATH ) {
		G_GuestbookPrivateDbPath = (const char*)ptr;
	}
}

void Guestbook_ConfigReset(void)
{
	memset(&G_GuestbookConfig, 0, sizeof(G_GuestbookConfig));
	snprintf(G_GuestbookConfig.sTitle, sizeof(G_GuestbookConfig.sTitle), "%s", "Guestbook");
	snprintf(G_GuestbookConfig.sIntro, sizeof(G_GuestbookConfig.sIntro), "%s", "Leave a message for the team.");
	G_GuestbookConfig.iPageSize = 20;
	G_GuestbookConfig.iMaxContentLength = 280;
	G_GuestbookConfig.bRequireApproval = false;
}

void Guestbook_CopyText(char* sDest, size_t iCap, const char* sValue, const char* sFallback)
{
	if ( (sDest == NULL) || (iCap == 0) ) {
		return;
	}
	snprintf(sDest, iCap, "%s", (sValue && sValue[0]) ? sValue : (sFallback ? sFallback : ""));
}

int Guestbook_ClampInt(int iValue, int iMin, int iMax, int iDefault)
{
	if ( iValue < iMin ) {
		return iDefault;
	}
	if ( iValue > iMax ) {
		return iMax;
	}
	return iValue;
}

bool Guestbook_IsBlank(const char* sText)
{
	const unsigned char* p = (const unsigned char*)sText;

	if ( p == NULL ) {
		return true;
	}
	while ( *p ) {
		if ( (*p != ' ') && (*p != '\t') && (*p != '\r') && (*p != '\n') ) {
			return false;
		}
		p++;
	}
	return true;
}

const char* Guestbook_StatusText(int iStatus)
{
	if ( iStatus == GUESTBOOK_STATUS_PENDING ) {
		return "pending";
	}
	if ( iStatus == GUESTBOOK_STATUS_APPROVED ) {
		return "approved";
	}
	if ( iStatus == GUESTBOOK_STATUS_REJECTED ) {
		return "rejected";
	}
	return "unknown";
}

bool Guestbook_OpenDb(sqlite3** ppDb)
{
	sqlite3* pDb = NULL;
	int iRet;

	if ( ppDb ) {
		*ppDb = NULL;
	}
	if ( (ppDb == NULL) || (G_GuestbookPrivateDbPath == NULL) || (G_GuestbookPrivateDbPath[0] == '\0') ) {
		return false;
	}

	iRet = sqlite3_open_v2(
		G_GuestbookPrivateDbPath,
		&pDb,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
		NULL);
	if ( iRet != SQLITE_OK ) {
		if ( pDb ) {
			sqlite3_close(pDb);
		}
		return false;
	}

	sqlite3_busy_timeout(pDb, 3000);
	*ppDb = pDb;
	return true;
}

void Guestbook_CloseDb(sqlite3* pDb)
{
	if ( pDb ) {
		sqlite3_close(pDb);
	}
}

bool Guestbook_TableHasColumn(sqlite3* pDb, const char* sColumnName)
{
	sqlite3_stmt* stmt = NULL;
	bool bFound = false;

	if ( (pDb == NULL) || (sColumnName == NULL) ) {
		return false;
	}
	if ( sqlite3_prepare_v2(pDb, "PRAGMA table_info(guestbook_message)", -1, &stmt, NULL) != SQLITE_OK ) {
		return false;
	}

	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		const unsigned char* sName = sqlite3_column_text(stmt, 1);
		if ( sName && (strcmp((const char*)sName, sColumnName) == 0) ) {
			bFound = true;
			break;
		}
	}

	sqlite3_finalize(stmt);
	return bFound;
}

bool Guestbook_EnsureColumn(sqlite3* pDb, const char* sColumnName, const char* sAlterSql)
{
	char* sError = NULL;
	bool bOK = false;

	if ( (pDb == NULL) || (sColumnName == NULL) || (sAlterSql == NULL) ) {
		return false;
	}
	if ( Guestbook_TableHasColumn(pDb, sColumnName) ) {
		return true;
	}
	if ( sqlite3_exec(pDb, sAlterSql, NULL, NULL, &sError) == SQLITE_OK ) {
		bOK = true;
	}
	if ( sError ) {
		sqlite3_free(sError);
	}
	return bOK;
}

bool Guestbook_EnsureSchema(void)
{
	sqlite3* pDb = NULL;
	char* sError = NULL;
	bool bOK = false;

	if ( !Guestbook_OpenDb(&pDb) ) {
		return false;
	}
	if ( sqlite3_exec(pDb, G_GuestbookSchemaSql, NULL, NULL, &sError) != SQLITE_OK ) {
		if ( sError ) {
			sqlite3_free(sError);
		}
		Guestbook_CloseDb(pDb);
		return false;
	}
	if ( sError ) {
		sqlite3_free(sError);
		sError = NULL;
	}

	bOK =
		Guestbook_EnsureColumn(pDb, "status", "ALTER TABLE guestbook_message ADD COLUMN status INTEGER NOT NULL DEFAULT 1")
		&& Guestbook_EnsureColumn(pDb, "reply", "ALTER TABLE guestbook_message ADD COLUMN reply TEXT DEFAULT ''")
		&& Guestbook_EnsureColumn(pDb, "reply_time", "ALTER TABLE guestbook_message ADD COLUMN reply_time INTEGER NOT NULL DEFAULT 0")
		&& Guestbook_EnsureColumn(pDb, "update_time", "ALTER TABLE guestbook_message ADD COLUMN update_time INTEGER NOT NULL DEFAULT 0");
	if ( bOK ) {
		sqlite3_exec(pDb, "UPDATE guestbook_message SET update_time = create_time WHERE update_time = 0", NULL, NULL, NULL);
		sqlite3_exec(pDb, "UPDATE guestbook_message SET status = 1 WHERE status NOT IN (0,1,2)", NULL, NULL, NULL);
		if ( sqlite3_exec(pDb, G_GuestbookIndexSql, NULL, NULL, &sError) != SQLITE_OK ) {
			bOK = false;
		}
		if ( sError ) {
			sqlite3_free(sError);
		}
	}

	Guestbook_CloseDb(pDb);
	return bOK;
}

xvalue* Guestbook_CreateResult(bool bResult, const char* sMessage)
{
	xvalue* tblRet = ValueObject();

	if ( tblRet == NULL ) {
		return NULL;
	}
	ValueSetBool(tblRet, "result", bResult);
	if ( sMessage ) {
		ValueSetText(tblRet, "message", (str)sMessage);
	}
	return tblRet;
}

void Guestbook_SendJsonValue(XS_ResponseObject objResp, xvalue* objValue)
{
	size_t iSize = 0;
	str sJson = xrtJsonStringify(objValue, false, &iSize);

	if ( sJson ) {
		xsHttpReplyAuto(objResp, 200, "Content-Type: application/json\r\n", sJson, iSize);
		xrtFree(sJson);
	} else {
		xsHttpReplyAuto(objResp, 500, "Content-Type: application/json\r\n", "{\"result\":false,\"message\":\"json encode failed\"}", 0);
	}
	xrtValueRelease(objValue);
}

void Guestbook_SendError(XS_ResponseObject objResp, const char* sMessage)
{
	xvalue* tblRet = Guestbook_CreateResult(false, sMessage ? sMessage : "request failed");

	if ( tblRet == NULL ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: application/json\r\n", "{\"result\":false,\"message\":\"request failed\"}", 0);
		return;
	}
	Guestbook_SendJsonValue(objResp, tblRet);
}

void Guestbook_SendOkMessage(XS_ResponseObject objResp, const char* sMessage)
{
	xvalue* tblRet = Guestbook_CreateResult(true, sMessage ? sMessage : "ok");

	if ( tblRet == NULL ) {
		xsHttpReplyAuto(objResp, 200, "Content-Type: application/json\r\n", "{\"result\":true}", 0);
		return;
	}
	Guestbook_SendJsonValue(objResp, tblRet);
}

xvalue* Guestbook_ParseJsonBody(XS_RequestObject objReq)
{
	xvalue* tblForm = JsonParseN((str)XAdmin_ReqBody(objReq), XAdmin_ReqBodyLen(objReq));

	if ( (tblForm == NULL) || (xrtValueType(tblForm) != XVALUE_OBJECT) ) {
		if ( tblForm ) {
			xrtValueRelease(tblForm);
		}
		return NULL;
	}
	return tblForm;
}

int Guestbook_ReadIntQuery(XS_RequestObject objReq, const char* sName, int iDefault)
{
	char sValue[32];

	memset(sValue, 0, sizeof(sValue));
	xsReqQueryValue(objReq, sName, sValue, sizeof(sValue));
	if ( sValue[0] == '\0' ) {
		return iDefault;
	}
	return atoi(sValue);
}

void Guestbook_SetTimeText(xvalue* tblItem, const char* sKey, xtime iTime)
{
	str sValue = (iTime > 0) ? TimeText(iTime, TIME_TEXT_DATETIME) : xrtStrDup("");
	ValueSetOwnedText(tblItem, sKey, sValue ? sValue : (str)"");
}

void Guestbook_AppendMessageRow(xvalue* arrList, sqlite3_stmt* stmt)
{
	xvalue* tblItem = ValueObject();
	xtime iCreateTime;
	xtime iReplyTime;
	xtime iUpdateTime;
	const unsigned char* sNickname;
	const unsigned char* sContent;
	const unsigned char* sReply;
	int iStatus;

	if ( (arrList == NULL) || (stmt == NULL) || (tblItem == NULL) ) {
		if ( tblItem ) {
			xrtValueRelease(tblItem);
		}
		return;
	}

	iCreateTime = sqlite3_column_int64(stmt, 6);
	iReplyTime = sqlite3_column_int64(stmt, 4);
	iUpdateTime = sqlite3_column_int64(stmt, 7);
	sNickname = sqlite3_column_text(stmt, 1);
	sContent = sqlite3_column_text(stmt, 2);
	sReply = sqlite3_column_text(stmt, 3);
	iStatus = sqlite3_column_int(stmt, 5);

	ValueSetInt(tblItem, "id", sqlite3_column_int64(stmt, 0));
	ValueSetText(tblItem, "nickname", (str)(sNickname ? sNickname : (const unsigned char*)""));
	ValueSetText(tblItem, "content", (str)(sContent ? sContent : (const unsigned char*)""));
	ValueSetText(tblItem, "reply", (str)(sReply ? sReply : (const unsigned char*)""));
	ValueSetInt(tblItem, "status", iStatus);
	ValueSetText(tblItem, "statusText", (str)Guestbook_StatusText(iStatus));
	ValueSetInt(tblItem, "replyTime", iReplyTime);
	ValueSetInt(tblItem, "createTime", iCreateTime);
	ValueSetInt(tblItem, "updateTime", iUpdateTime > 0 ? iUpdateTime : iCreateTime);
	Guestbook_SetTimeText(tblItem, "replyTimeText", iReplyTime);
	Guestbook_SetTimeText(tblItem, "createTimeText", iCreateTime);
	Guestbook_SetTimeText(tblItem, "updateTimeText", iUpdateTime > 0 ? iUpdateTime : iCreateTime);
	ValueArrayOwn(arrList, tblItem);
}

xvalue* Guestbook_QueryMessages(bool bAdmin, int iStatusFilter, int iPage, int iLimit, int* piTotal)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue* arrList = NULL;
	str sCountSql = NULL;
	str sQuerySql = NULL;
	const char* sWhere = "";

	if ( piTotal ) {
		*piTotal = 0;
	}
	if ( !Guestbook_OpenDb(&pDb) ) {
		return NULL;
	}

	if ( !bAdmin ) {
		sWhere = "WHERE status = 1";
	} else if ( iStatusFilter == GUESTBOOK_STATUS_PENDING ) {
		sWhere = "WHERE status = 0";
	} else if ( iStatusFilter == GUESTBOOK_STATUS_APPROVED ) {
		sWhere = "WHERE status = 1";
	} else if ( iStatusFilter == GUESTBOOK_STATUS_REJECTED ) {
		sWhere = "WHERE status = 2";
	}

	sCountSql = xrtFormat("SELECT COUNT(1) FROM guestbook_message %s", sWhere);
	if ( (sCountSql == NULL) || (sqlite3_prepare_v2(pDb, sCountSql, -1, &stmt, NULL) != SQLITE_OK) ) {
		if ( sCountSql ) xrtFree(sCountSql);
		Guestbook_CloseDb(pDb);
		return NULL;
	}
	if ( sqlite3_step(stmt) == SQLITE_ROW && piTotal ) {
		*piTotal = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	stmt = NULL;
	xrtFree(sCountSql);

	arrList = ValueArray();
	if ( arrList == NULL ) {
		Guestbook_CloseDb(pDb);
		return NULL;
	}

	sQuerySql = xrtFormat(
		"SELECT id, nickname, content, reply, reply_time, status, create_time, update_time "
		"FROM guestbook_message %s ORDER BY id DESC LIMIT ? OFFSET ?",
		sWhere);
	if ( (sQuerySql == NULL) || (sqlite3_prepare_v2(pDb, sQuerySql, -1, &stmt, NULL) != SQLITE_OK) ) {
		if ( sQuerySql ) xrtFree(sQuerySql);
		xrtValueRelease(arrList);
		Guestbook_CloseDb(pDb);
		return NULL;
	}

	sqlite3_bind_int(stmt, 1, iLimit);
	sqlite3_bind_int(stmt, 2, (iPage - 1) * iLimit);
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		Guestbook_AppendMessageRow(arrList, stmt);
	}

	sqlite3_finalize(stmt);
	xrtFree(sQuerySql);
	Guestbook_CloseDb(pDb);
	return arrList;
}

xvalue* Guestbook_BuildListResponse(bool bAdmin, int iStatusFilter, int iPage, int iLimit)
{
	xvalue* tblRet = NULL;
	xvalue* arrList = NULL;
	int iTotal = 0;

	arrList = Guestbook_QueryMessages(bAdmin, iStatusFilter, iPage, iLimit, &iTotal);
	if ( arrList == NULL ) {
		return NULL;
	}
	tblRet = Guestbook_CreateResult(true, NULL);
	if ( tblRet == NULL ) {
		xrtValueRelease(arrList);
		return NULL;
	}

	ValueSetOwn(tblRet, "data", arrList);
	ValueSetInt(tblRet, "page", iPage);
	ValueSetInt(tblRet, "pageSize", iLimit);
	ValueSetInt(tblRet, "total", iTotal);
	ValueSetInt(tblRet, "statusFilter", bAdmin ? iStatusFilter : GUESTBOOK_STATUS_APPROVED);
	return tblRet;
}

bool Guestbook_InsertMessage(const char* sNickname, const char* sContent, int iStatus)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xtime iNow;
	bool bOK = false;

	if ( !Guestbook_OpenDb(&pDb) ) {
		return false;
	}
	if ( sqlite3_prepare_v2(
		pDb,
		"INSERT INTO guestbook_message (nickname, content, status, reply, reply_time, create_time, update_time) VALUES (?, ?, ?, '', 0, ?, ?)",
		-1,
		&stmt,
		NULL) != SQLITE_OK ) {
		Guestbook_CloseDb(pDb);
		return false;
	}

	iNow = xrtNow();
	sqlite3_bind_text(stmt, 1, sNickname ? sNickname : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, sContent ? sContent : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 3, iStatus);
	sqlite3_bind_int64(stmt, 4, iNow);
	sqlite3_bind_int64(stmt, 5, iNow);
	bOK = (sqlite3_step(stmt) == SQLITE_DONE);

	sqlite3_finalize(stmt);
	Guestbook_CloseDb(pDb);
	return bOK;
}

bool Guestbook_UpdateMessage(int64 iId, bool bHasStatus, int iStatus, bool bHasReply, const char* sReply)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	const char* sSql = NULL;
	xtime iNow;
	xtime iReplyTime;
	bool bOK = false;

	if ( (iId <= 0) || (!bHasStatus && !bHasReply) ) {
		return false;
	}
	if ( !Guestbook_OpenDb(&pDb) ) {
		return false;
	}

	if ( bHasStatus && bHasReply ) {
		sSql = "UPDATE guestbook_message SET status = ?, reply = ?, reply_time = ?, update_time = ? WHERE id = ?";
	} else if ( bHasStatus ) {
		sSql = "UPDATE guestbook_message SET status = ?, update_time = ? WHERE id = ?";
	} else {
		sSql = "UPDATE guestbook_message SET reply = ?, reply_time = ?, update_time = ? WHERE id = ?";
	}
	if ( sqlite3_prepare_v2(pDb, sSql, -1, &stmt, NULL) != SQLITE_OK ) {
		Guestbook_CloseDb(pDb);
		return false;
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

	if ( sqlite3_step(stmt) == SQLITE_DONE ) {
		bOK = sqlite3_changes(pDb) > 0;
	}

	sqlite3_finalize(stmt);
	Guestbook_CloseDb(pDb);
	return bOK;
}

bool Guestbook_DeleteMessage(int64 iId)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	bool bOK = false;

	if ( (iId <= 0) || !Guestbook_OpenDb(&pDb) ) {
		return false;
	}
	if ( sqlite3_prepare_v2(pDb, "DELETE FROM guestbook_message WHERE id = ?", -1, &stmt, NULL) != SQLITE_OK ) {
		Guestbook_CloseDb(pDb);
		return false;
	}

	sqlite3_bind_int64(stmt, 1, iId);
	if ( sqlite3_step(stmt) == SQLITE_DONE ) {
		bOK = sqlite3_changes(pDb) > 0;
	}

	sqlite3_finalize(stmt);
	Guestbook_CloseDb(pDb);
	return bOK;
}

bool Guestbook_SendAssetHtml(XS_ResponseObject objResp, const char* sFileName)
{
	str sPath;
	ptr pData;
	size_t iSize = 0;

	if ( (G_GuestbookRootPath == NULL) || (sFileName == NULL) ) {
		return false;
	}
	sPath = xrtPathJoin(G_GuestbookRootPath, (str)sFileName);
	if ( (sPath == NULL) || !xrtFileExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		return false;
	}

	pData = xrtFileReadAll(sPath, &iSize);
	xrtFree(sPath);
	if ( pData == NULL ) {
		return false;
	}
	xsHttpReplyAuto(objResp, 200, "Content-Type: text/html; charset=utf-8\r\n", pData, iSize);
	xrtFree(pData);
	return true;
}

void Guestbook_RequestMeta(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* tblRet;
	xvalue* tblData;

	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	tblRet = Guestbook_CreateResult(true, NULL);
	tblData = ValueObject();
	if ( (tblRet == NULL) || (tblData == NULL) ) {
		if ( tblRet ) xrtValueRelease(tblRet);
		if ( tblData ) xrtValueRelease(tblData);
		Guestbook_SendError(objResp, "failed to build response");
		return;
	}

	ValueSetText(tblData, "title", G_GuestbookConfig.sTitle);
	ValueSetText(tblData, "intro", G_GuestbookConfig.sIntro);
	ValueSetInt(tblData, "pageSize", G_GuestbookConfig.iPageSize);
	ValueSetInt(tblData, "maxContentLength", G_GuestbookConfig.iMaxContentLength);
	ValueSetBool(tblData, "requireApproval", G_GuestbookConfig.bRequireApproval);
	ValueSetText(tblData, "xid", (str)(G_GuestbookXid ? G_GuestbookXid : "guestbook_v3"));
	ValueSetOwn(tblRet, "data", tblData);
	Guestbook_SendJsonValue(objResp, tblRet);
}

void Guestbook_RequestListPublic(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* tblRet;
	int iPage;
	int iLimit;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		Guestbook_SendError(objResp, "method not allowed");
		return;
	}

	iPage = Guestbook_ClampInt(Guestbook_ReadIntQuery(objReq, "page", 1), 1, 1000000, 1);
	iLimit = Guestbook_ClampInt(Guestbook_ReadIntQuery(objReq, "limit", G_GuestbookConfig.iPageSize), 1, 100, G_GuestbookConfig.iPageSize);
	tblRet = Guestbook_BuildListResponse(false, GUESTBOOK_STATUS_APPROVED, iPage, iLimit);
	if ( tblRet == NULL ) {
		Guestbook_SendError(objResp, "failed to load messages");
		return;
	}
	Guestbook_SendJsonValue(objResp, tblRet);
}

void Guestbook_RequestListAdmin(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* tblRet;
	int iPage;
	int iLimit;
	int iStatusFilter;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		Guestbook_SendError(objResp, "method not allowed");
		return;
	}

	iPage = Guestbook_ClampInt(Guestbook_ReadIntQuery(objReq, "page", 1), 1, 1000000, 1);
	iLimit = Guestbook_ClampInt(Guestbook_ReadIntQuery(objReq, "limit", G_GuestbookConfig.iPageSize), 1, 100, G_GuestbookConfig.iPageSize);
	iStatusFilter = Guestbook_ReadIntQuery(objReq, "status", -1);
	if ( (iStatusFilter != -1) && (iStatusFilter != GUESTBOOK_STATUS_PENDING) && (iStatusFilter != GUESTBOOK_STATUS_APPROVED) && (iStatusFilter != GUESTBOOK_STATUS_REJECTED) ) {
		iStatusFilter = -1;
	}

	tblRet = Guestbook_BuildListResponse(true, iStatusFilter, iPage, iLimit);
	if ( tblRet == NULL ) {
		Guestbook_SendError(objResp, "failed to load messages");
		return;
	}
	Guestbook_SendJsonValue(objResp, tblRet);
}

void Guestbook_RequestAdd(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* tblForm;
	str sNickname;
	str sContent;
	int iStatus;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		Guestbook_SendError(objResp, "method not allowed");
		return;
	}

	tblForm = Guestbook_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		Guestbook_SendError(objResp, "invalid json body");
		return;
	}

	sNickname = ValueText(tblForm, "nickname");
	sContent = ValueText(tblForm, "content");
	if ( Guestbook_IsBlank(sNickname) ) {
		xrtValueRelease(tblForm);
		Guestbook_SendError(objResp, "nickname is required");
		return;
	}
	if ( Guestbook_IsBlank(sContent) ) {
		xrtValueRelease(tblForm);
		Guestbook_SendError(objResp, "content is required");
		return;
	}
	if ( strlen(sNickname) > 64 ) {
		xrtValueRelease(tblForm);
		Guestbook_SendError(objResp, "nickname is too long");
		return;
	}
	if ( (int)strlen(sContent) > G_GuestbookConfig.iMaxContentLength ) {
		xrtValueRelease(tblForm);
		Guestbook_SendError(objResp, "content is too long");
		return;
	}

	iStatus = G_GuestbookConfig.bRequireApproval ? GUESTBOOK_STATUS_PENDING : GUESTBOOK_STATUS_APPROVED;
	if ( !Guestbook_InsertMessage(sNickname, sContent, iStatus) ) {
		xrtValueRelease(tblForm);
		Guestbook_SendError(objResp, "failed to save message");
		return;
	}

	xrtValueRelease(tblForm);
	Guestbook_SendOkMessage(objResp, G_GuestbookConfig.bRequireApproval ? "message submitted and pending review" : "message posted");
}

void Guestbook_RequestUpdate(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* tblForm;
	int64 iId;
	int iStatus = GUESTBOOK_STATUS_APPROVED;
	bool bHasStatus;
	bool bHasReply;
	str sReply = NULL;
	str sReplyCopy = NULL;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		Guestbook_SendError(objResp, "method not allowed");
		return;
	}

	tblForm = Guestbook_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		Guestbook_SendError(objResp, "invalid json body");
		return;
	}

	iId = ValueInt(tblForm, "id");
	bHasStatus = ValueHas(tblForm, "status");
	bHasReply = ValueHas(tblForm, "reply");
	if ( bHasStatus ) {
		iStatus = ValueInt(tblForm, "status");
		if ( (iStatus != GUESTBOOK_STATUS_PENDING) && (iStatus != GUESTBOOK_STATUS_APPROVED) && (iStatus != GUESTBOOK_STATUS_REJECTED) ) {
			xrtValueRelease(tblForm);
			Guestbook_SendError(objResp, "invalid status");
			return;
		}
	}
	if ( bHasReply ) {
		sReply = ValueText(tblForm, "reply");
		if ( sReply && ((int)strlen(sReply) > G_GuestbookConfig.iMaxContentLength * 4) ) {
			xrtValueRelease(tblForm);
			Guestbook_SendError(objResp, "reply is too long");
			return;
		}
		sReplyCopy = xrtStrDup(sReply ? sReply : (str)"");
		if ( sReplyCopy == NULL ) {
			xrtValueRelease(tblForm);
			Guestbook_SendError(objResp, "failed to copy reply");
			return;
		}
	}
	xrtValueRelease(tblForm);

	if ( iId <= 0 ) {
		if ( sReplyCopy ) xrtFree(sReplyCopy);
		Guestbook_SendError(objResp, "invalid id");
		return;
	}
	if ( !Guestbook_UpdateMessage(iId, bHasStatus, iStatus, bHasReply, sReplyCopy ? sReplyCopy : (str)"") ) {
		if ( sReplyCopy ) xrtFree(sReplyCopy);
		Guestbook_SendError(objResp, "failed to update message");
		return;
	}
	if ( sReplyCopy ) xrtFree(sReplyCopy);
	Guestbook_SendOkMessage(objResp, "message updated");
}

void Guestbook_RequestDelete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* tblForm;
	int64 iId;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		Guestbook_SendError(objResp, "method not allowed");
		return;
	}

	tblForm = Guestbook_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		Guestbook_SendError(objResp, "invalid json body");
		return;
	}

	iId = ValueInt(tblForm, "id");
	xrtValueRelease(tblForm);
	if ( iId <= 0 ) {
		Guestbook_SendError(objResp, "invalid id");
		return;
	}
	if ( !Guestbook_DeleteMessage(iId) ) {
		Guestbook_SendError(objResp, "failed to delete message");
		return;
	}
	Guestbook_SendOkMessage(objResp, "message deleted");
}

void Guestbook_RequestPublicView(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	if ( !Guestbook_SendAssetHtml(objResp, "public.html") ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "guestbook public page missing", 0);
	}
}

void Guestbook_RequestAdminView(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;
	if ( !Guestbook_SendAssetHtml(objResp, "admin.html") ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain; charset=utf-8\r\n", "guestbook admin page missing", 0);
	}
}

int Guestbook_OnLoad(XAdminPluginHandle* out_handle)
{
	if ( out_handle ) {
		G_GuestbookHandle = *out_handle;
	}
	return 0;
}

int Guestbook_OnStart(XAdminPluginHandle handle)
{
	XAdminMenuDecl menu;
	XAdminRouteDecl route;

	G_GuestbookHandle = handle;
	if ( !Guestbook_EnsureSchema() ) {
		return -1;
	}

	memset(&route, 0, sizeof(route));
	route.path = "/api/plugin/guestbook_v3/meta";
	route.proc = Guestbook_RequestMeta;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/api/plugin/guestbook_v3/list";
	route.proc = Guestbook_RequestListPublic;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/api/plugin/guestbook_v3/add";
	route.proc = Guestbook_RequestAdd;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/plugin/guestbook_v3";
	route.proc = Guestbook_RequestPublicView;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/guestbook_v3/list";
	route.proc = Guestbook_RequestListAdmin;
	route.need_auth = true;
	route.admin_only = true;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/guestbook_v3/update";
	route.proc = Guestbook_RequestUpdate;
	route.need_auth = true;
	route.admin_only = true;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/guestbook_v3/delete";
	route.proc = Guestbook_RequestDelete;
	route.need_auth = true;
	route.admin_only = true;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/guestbook_v3";
	route.proc = Guestbook_RequestAdminView;
	route.need_auth = true;
	route.admin_only = true;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&menu, 0, sizeof(menu));
	menu.title = "Guestbook";
	menu.icon = "layui-icon layui-icon-dialogue";
	menu.type = 1;
	menu.open_type = "_iframe";
	menu.href = "/admin/view/plugin/guestbook_v3";
	menu.sort = 990010;
	menu.visible = true;
	menu.remark = "Guestbook plugin";
	if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) return -1;

	return 0;
}

int Guestbook_OnConfigChanged(XAdminPluginHandle handle, xvalue* new_cfg)
{
	(void)handle;

	Guestbook_ConfigReset();
	if ( new_cfg && (xrtValueType(new_cfg) == XVALUE_OBJECT) ) {
		Guestbook_CopyText(G_GuestbookConfig.sTitle, sizeof(G_GuestbookConfig.sTitle), ValueText(new_cfg, "title"), "Guestbook");
		Guestbook_CopyText(G_GuestbookConfig.sIntro, sizeof(G_GuestbookConfig.sIntro), ValueText(new_cfg, "intro"), "Leave a message for the team.");
		G_GuestbookConfig.iPageSize = Guestbook_ClampInt(ValueInt(new_cfg, "pageSize"), 1, 100, 20);
		G_GuestbookConfig.iMaxContentLength = Guestbook_ClampInt(ValueInt(new_cfg, "maxContentLength"), 20, 4096, 280);
		G_GuestbookConfig.bRequireApproval = ValueBool(new_cfg, "requireApproval");
	}
	return 0;
}

int Guestbook_OnHealthCheck(XAdminPluginHandle handle, XAdminHealthReport* out_report)
{
	bool bHealthy;

	(void)handle;
	bHealthy = Guestbook_EnsureSchema();
	if ( out_report ) {
		out_report->status_code = bHealthy ? 0 : -1;
		out_report->message = bHealthy ? "ok" : "db unavailable";
	}
	return bHealthy ? 0 : -1;
}

void Guestbook_OnStop(XAdminPluginHandle handle)
{
	(void)handle;
	G_GuestbookHandle = NULL;
}

void Guestbook_OnUnload(XAdminPluginHandle handle)
{
	(void)handle;
	G_GuestbookHandle = NULL;
	G_GuestbookXid = NULL;
	G_GuestbookRootPath = NULL;
	G_GuestbookPrivateDbPath = NULL;
	Guestbook_ConfigReset();
}

static XAdminPluginDescriptor G_GuestbookPlugin = {
	XADMIN_ABI_VERSION,
	sizeof(XAdminPluginDescriptor),
	"guestbook_v3",
	"1.1.0",
	"Guestbook V3",
	Guestbook_OnLoad,
	NULL,
	Guestbook_OnStart,
	Guestbook_OnConfigChanged,
	Guestbook_OnHealthCheck,
	Guestbook_OnStop,
	Guestbook_OnUnload
};

XADMIN_DECLARE_PLUGIN(G_GuestbookPlugin)
