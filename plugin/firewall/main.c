#include "xs_plugin.h"

typedef struct {
	int bAutoApplyOnStart;
} FWConfigState;

static XAdminPluginHandle G_FWHandle = NULL;
static sqlite3* G_FWMainDb = NULL;
static const char* G_FWXid = NULL;
static const char* G_FWRootPath = NULL;
static const char* G_FWDataPath = NULL;
static const char* G_FWPrivateDbPath = NULL;
static FWConfigState G_FWConfig = { 1 };

static const char* G_FWSchemaSql =
	"CREATE TABLE IF NOT EXISTS firewall_rules ("
	"id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"protocol TEXT NOT NULL,"
	"port TEXT DEFAULT '',"
	"source_ip TEXT DEFAULT '',"
	"action TEXT NOT NULL,"
	"comment TEXT DEFAULT '',"
	"enabled INTEGER DEFAULT 1"
	");";

XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int idx, void* ptr)
{
	if ( idx == XADMIN_GLOBAL_MAIN_DB ) {
		G_FWMainDb = (sqlite3*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_XID ) {
		G_FWXid = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_ROOT_PATH ) {
		G_FWRootPath = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_DATA_PATH ) {
		G_FWDataPath = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_PRIVATE_DB_PATH ) {
		G_FWPrivateDbPath = (const char*)ptr;
	}
}

void FW_SendJson(XS_ResponseObject objResp, xvalue tblData)
{
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblData, FALSE, &iSize);
	if ( sJson ) {
		xsHttpReplyAuto(objResp, 200, "Content-Type: application/json\r\n", sJson, iSize);
		xrtFree(sJson);
	}
	xvoUnref(tblData);
}

xvalue FW_CreateResult(bool bResult, const char* sMessage)
{
	xvalue tblRet = xvoCreateTable();
	if ( tblRet == NULL ) return NULL;
	xvoTableSetBool(tblRet, "result", 6, bResult);
	if ( sMessage ) {
		xvoTableSetText(tblRet, "message", 7, (str)sMessage, 0, FALSE);
	}
	return tblRet;
}

void FW_SendError(XS_ResponseObject objResp, const char* sMessage)
{
	FW_SendJson(objResp, FW_CreateResult(FALSE, sMessage));
}

void FW_SendOk(XS_ResponseObject objResp, const char* sMessage)
{
	FW_SendJson(objResp, FW_CreateResult(TRUE, sMessage));
}

bool FW_SendAssetHtml(XS_ResponseObject objResp, const char* sFileName)
{
	str sPath;
	ptr pData;
	size_t iSize = 0;

	if ( (G_FWRootPath == NULL) || (sFileName == NULL) ) return FALSE;
	sPath = xrtPathJoin(2, G_FWRootPath, (str)sFileName);
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

xvalue FW_ParseJsonBody(XS_RequestObject objReq)
{
	xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( (tblForm == NULL) || (xvoType(tblForm) != XVO_DT_TABLE) ) {
		if ( tblForm ) xvoUnref(tblForm);
		return NULL;
	}
	return tblForm;
}

int FW_ReadIntQuery(XS_RequestObject objReq, const char* sName, int iDefault)
{
	char sBuf[32];
	memset(sBuf, 0, sizeof(sBuf));
	xsReqQueryValue(objReq, sName, sBuf, sizeof(sBuf));
	if ( sBuf[0] == '\0' ) return iDefault;
	return atoi(sBuf);
}

str FW_ReadQuery(XS_RequestObject objReq, const char* sName)
{
	char sBuf[256];
	memset(sBuf, 0, sizeof(sBuf));
	xsReqQueryValue(objReq, sName, sBuf, sizeof(sBuf));
	if ( sBuf[0] == '\0' ) return NULL;
	return xrtCopyStr(sBuf, 0);
}

bool FW_OpenDb(sqlite3** ppDb)
{
	sqlite3* pDb = NULL;
	int iRet;
	if ( ppDb ) *ppDb = NULL;
	if ( (ppDb == NULL) || (G_FWPrivateDbPath == NULL) || (G_FWPrivateDbPath[0] == '\0') ) return FALSE;
	iRet = sqlite3_open_v2(G_FWPrivateDbPath, &pDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
	if ( iRet != SQLITE_OK ) {
		if ( pDb ) sqlite3_close(pDb);
		return FALSE;
	}
	sqlite3_busy_timeout(pDb, 3000);
	*ppDb = pDb;
	return TRUE;
}

void FW_CloseDb(sqlite3* pDb)
{
	if ( pDb ) sqlite3_close(pDb);
}

bool FW_EnsureSchema()
{
	sqlite3* pDb = NULL;
	char* sError = NULL;
	if ( !FW_OpenDb(&pDb) ) return FALSE;
	if ( sqlite3_exec(pDb, G_FWSchemaSql, NULL, NULL, &sError) != SQLITE_OK ) {
		if ( sError ) sqlite3_free(sError);
		FW_CloseDb(pDb);
		return FALSE;
	}
	if ( sError ) sqlite3_free(sError);
	FW_CloseDb(pDb);
	return TRUE;
}

#ifdef _WIN32

void FW_Req_ViewPage(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	xsHttpReplyAuto(objResp, 200, "Content-Type: text/html; charset=utf-8\r\n",
		"<div style=\"padding:40px;text-align:center;\">"
		"<h3>Firewall management is only available on Linux</h3>"
		"<p>This plugin requires iptables which is not available on Windows.</p>"
		"</div>", 0);
}

void FW_Req_ApiList(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	FW_SendError(objResp, "Firewall management is only available on Linux");
}

void FW_Req_ApiAdd(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	FW_SendError(objResp, "Firewall management is only available on Linux");
}

void FW_Req_ApiDelete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	FW_SendError(objResp, "Firewall management is only available on Linux");
}

void FW_Req_ApiToggle(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	FW_SendError(objResp, "Firewall management is only available on Linux");
}

void FW_Req_ApiSave(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	FW_SendError(objResp, "Firewall management is only available on Linux");
}

#else

static str FW_ExecCmd(const char* cmd)
{
	FILE* fp = popen(cmd, "r");
	str sResult;
	size_t iTotal = 0;
	char sBuf[1024];

	if ( !fp ) return NULL;

	sResult = (str)xrtMalloc(65536);
	if ( !sResult ) {
		pclose(fp);
		return NULL;
	}

	while ( fgets(sBuf, sizeof(sBuf), fp) ) {
		size_t iLen = strlen(sBuf);
		if ( iTotal + iLen < 65535 ) {
			memcpy(sResult + iTotal, sBuf, iLen);
			iTotal += iLen;
		}
	}
	sResult[iTotal] = '\0';
	pclose(fp);
	return sResult;
}

static int FW_ApplyRule(int iId, const char* sProtocol, const char* sPort, const char* sSourceIp, const char* sAction)
{
	char sCmd[2048];
	char sPortSpec[128] = "";
	char sSourceSpec[256] = "";
	char sProtoSpec[32] = "";

	if ( strcmp(sProtocol, "all") != 0 ) {
		snprintf(sProtoSpec, sizeof(sProtoSpec), "-p %s", sProtocol);
	}
	if ( sPort[0] && strcmp(sProtocol, "icmp") != 0 ) {
		snprintf(sPortSpec, sizeof(sPortSpec), "--dport %s", sPort);
	}
	if ( sSourceIp[0] ) {
		snprintf(sSourceSpec, sizeof(sSourceSpec), "-s %s", sSourceIp);
	}

	snprintf(sCmd, sizeof(sCmd),
		"iptables -A INPUT %s %s %s -j %s -m comment --comment \"xadmin_fw_%d\"",
		sProtoSpec, sSourceSpec, sPortSpec, sAction, iId);

	return system(sCmd) == 0 ? 0 : -1;
}

static int FW_ClearRules()
{
	str sOutput = FW_ExecCmd("iptables -L INPUT --line-numbers -n | grep 'xadmin_fw_' | awk '{print $1}' | tac");
	if ( sOutput ) {
		char* pLine = strtok((char*)sOutput, "\n");
		while ( pLine ) {
			int iLineNum = atoi(pLine);
			if ( iLineNum > 0 ) {
				char sCmd[256];
				snprintf(sCmd, sizeof(sCmd), "iptables -D INPUT %d", iLineNum);
				system(sCmd);
			}
			pLine = strtok(NULL, "\n");
		}
		xrtFree(sOutput);
	}
	return 0;
}

static int FW_ApplyAllRules()
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;

	FW_ClearRules();

	if ( !FW_OpenDb(&pDb) ) return -1;

	if ( sqlite3_prepare_v2(pDb, "SELECT id, protocol, port, source_ip, action FROM firewall_rules WHERE enabled = 1;", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			int iId = sqlite3_column_int(stmt, 0);
			const char* sProtocol = (const char*)sqlite3_column_text(stmt, 1);
			const char* sPort = (const char*)sqlite3_column_text(stmt, 2);
			const char* sSourceIp = (const char*)sqlite3_column_text(stmt, 3);
			const char* sAction = (const char*)sqlite3_column_text(stmt, 4);

			if ( sProtocol && sAction ) {
				FW_ApplyRule(iId, sProtocol, sPort ? sPort : "", sSourceIp ? sSourceIp : "", sAction);
			}
		}
		sqlite3_finalize(stmt);
	}

	FW_CloseDb(pDb);

	system("iptables-save > /etc/iptables/rules.v4 2>/dev/null || true");
	return 0;
}

static int FW_DeleteIptablesRule(int iRuleId)
{
	char sSearch[64];
	char sCmd[256];
	str sOutput;

	snprintf(sSearch, sizeof(sSearch), "xadmin_fw_%d", iRuleId);
	sOutput = FW_ExecCmd("iptables -L INPUT --line-numbers -n | grep 'xadmin_fw_' | awk '{print $1, $0}'");

	if ( sOutput ) {
		char* pLine = strtok((char*)sOutput, "\n");
		while ( pLine ) {
			if ( strstr(pLine, sSearch) ) {
				int iLineNum;
				sscanf(pLine, "%d", &iLineNum);
				snprintf(sCmd, sizeof(sCmd), "iptables -D INPUT %d", iLineNum);
				system(sCmd);
				break;
			}
			pLine = strtok(NULL, "\n");
		}
		xrtFree(sOutput);
	}

	system("iptables-save > /etc/iptables/rules.v4 2>/dev/null || true");
	return 0;
}

static xvalue FW_GetIptablesStats()
{
	xvalue arrRules = xvoCreateArray();
	str sOutput = FW_ExecCmd("iptables -L INPUT -n -v --line-numbers");

	if ( sOutput ) {
		char* pLine = strtok((char*)sOutput, "\n");
		int iLineCount = 0;
		while ( pLine && iLineCount < 2 ) {
			pLine = strtok(NULL, "\n");
			iLineCount++;
		}

		while ( pLine ) {
			if ( strstr(pLine, "xadmin_fw_") ) {
				char sNum[16], sPkts[32], sBytes[32], sTarget[32], sProt[16], sOpt[16];
				char sIn[16], sOut[16], sSource[64], sDest[64];
				char sExtra[512] = "";

				int n = sscanf(pLine, "%s %s %s %s %s %s %s %s %s %s %[^\n]",
					sNum, sPkts, sBytes, sTarget, sProt, sOpt, sIn, sOut, sSource, sDest, sExtra);

				if ( n >= 10 ) {
					char* pComment = strstr(pLine, "xadmin_fw_");
					int iRuleId = 0;
					char sPort[64] = "";

					if ( pComment ) {
						iRuleId = atoi(pComment + 10);
					}

					{
						char* pDpt = strstr(sExtra, "dpt:");
						if ( pDpt ) {
							sscanf(pDpt + 4, "%s", sPort);
						}
					}

					{
						xvalue tblRow = xvoCreateTable();
						xvoTableSetInt(tblRow, "id", 2, iRuleId);
						xvoTableSetText(tblRow, "protocol", 8, (str)sProt, 0, FALSE);
						xvoTableSetText(tblRow, "port", 4, (str)sPort, 0, FALSE);
						xvoTableSetText(tblRow, "source_ip", 9, (str)(strcmp(sSource, "0.0.0.0/0") == 0 ? "" : sSource), 0, FALSE);
						xvoTableSetText(tblRow, "action", 6, (str)sTarget, 0, FALSE);
						xvoTableSetText(tblRow, "packets", 7, (str)sPkts, 0, FALSE);
						xvoTableSetText(tblRow, "bytes", 5, (str)sBytes, 0, FALSE);
						xvoArrayAppendValue(arrRules, tblRow, TRUE);
					}
				}
			}
			pLine = strtok(NULL, "\n");
		}
		xrtFree(sOutput);
	}

	return arrRules;
}

void FW_Req_ViewPage(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !FW_SendAssetHtml(objResp, "page/firewall.html") ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain\r\n", "page not found", 0);
	}
}

void FW_Req_ApiList(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue arrDbRules = xvoCreateArray();
	xvalue arrLiveStats = NULL;
	xvalue tblRet;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		xsHttpReplyAuto(objResp, 405, "Content-Type: text/plain\r\n", "method not allowed", 0);
		return;
	}

	if ( !FW_OpenDb(&pDb) ) {
		FW_SendError(objResp, "database error");
		return;
	}

	if ( sqlite3_prepare_v2(pDb, "SELECT id, protocol, port, source_ip, action, comment, enabled FROM firewall_rules ORDER BY id;", -1, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblRow = xvoCreateTable();
			xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
			xvoTableSetText(tblRow, "protocol", 8, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
			xvoTableSetText(tblRow, "port", 4, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
			xvoTableSetText(tblRow, "source_ip", 9, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
			xvoTableSetText(tblRow, "action", 6, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
			xvoTableSetText(tblRow, "comment", 7, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
			xvoTableSetInt(tblRow, "enabled", 7, sqlite3_column_int(stmt, 6));
			xvoArrayAppendValue(arrDbRules, tblRow, TRUE);
		}
		sqlite3_finalize(stmt);
	}

	FW_CloseDb(pDb);

	arrLiveStats = FW_GetIptablesStats();

	tblRet = FW_CreateResult(TRUE, "success");
	xvoTableSetInt(tblRet, "code", 4, 0);
	xvoTableSetValue(tblRet, "rules", 5, arrDbRules, TRUE);
	xvoTableSetValue(tblRet, "liveStats", 9, arrLiveStats, TRUE);
	FW_SendJson(objResp, tblRet);
}

void FW_Req_ApiAdd(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblForm;
	str sProtocol, sPort, sSourceIp, sAction, sComment;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xsHttpReplyAuto(objResp, 405, "Content-Type: text/plain\r\n", "method not allowed", 0);
		return;
	}

	tblForm = FW_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		FW_SendError(objResp, "invalid request body");
		return;
	}

	sProtocol = xvoTableGetText(tblForm, "protocol", 8);
	sPort = xvoTableGetText(tblForm, "port", 4);
	sSourceIp = xvoTableGetText(tblForm, "source_ip", 9);
	sAction = xvoTableGetText(tblForm, "action", 6);
	sComment = xvoTableGetText(tblForm, "comment", 7);

	if ( !sProtocol || !sProtocol[0] || !sAction || !sAction[0] ) {
		xvoUnref(tblForm);
		FW_SendError(objResp, "protocol and action are required");
		return;
	}

	if ( !FW_OpenDb(&pDb) ) {
		xvoUnref(tblForm);
		FW_SendError(objResp, "database error");
		return;
	}

	if ( sqlite3_prepare_v2(pDb, "INSERT INTO firewall_rules (protocol, port, source_ip, action, comment, enabled) VALUES (?, ?, ?, ?, ?, 1);", -1, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, (char*)sProtocol, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, sPort ? (char*)sPort : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, sSourceIp ? (char*)sSourceIp : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, (char*)sAction, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 5, sComment ? (char*)sComment : "", -1, SQLITE_TRANSIENT);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	{
		int64 iNewId = sqlite3_last_insert_rowid(pDb);
		FW_CloseDb(pDb);

		FW_ApplyRule((int)iNewId, (char*)sProtocol, sPort ? (char*)sPort : "", sSourceIp ? (char*)sSourceIp : "", (char*)sAction);
		system("iptables-save > /etc/iptables/rules.v4 2>/dev/null || true");
	}

	xvoUnref(tblForm);
	FW_SendOk(objResp, "rule added");
}

void FW_Req_ApiDelete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblForm;
	str sIdStr;
	int64 iId;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xsHttpReplyAuto(objResp, 405, "Content-Type: text/plain\r\n", "method not allowed", 0);
		return;
	}

	tblForm = FW_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		sIdStr = FW_ReadQuery(objReq, "id");
	} else {
		sIdStr = xvoTableGetText(tblForm, "id", 2);
	}

	if ( !sIdStr || !sIdStr[0] ) {
		if ( tblForm ) xvoUnref(tblForm);
		FW_SendError(objResp, "id is required");
		return;
	}

	iId = xrtStrToI64((char*)sIdStr);
	if ( iId <= 0 ) {
		if ( tblForm ) xvoUnref(tblForm);
		FW_SendError(objResp, "invalid id");
		return;
	}

	FW_DeleteIptablesRule((int)iId);

	if ( FW_OpenDb(&pDb) ) {
		if ( sqlite3_prepare_v2(pDb, "DELETE FROM firewall_rules WHERE id = ?;", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, iId);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		FW_CloseDb(pDb);
	}

	if ( tblForm ) xvoUnref(tblForm);
	FW_SendOk(objResp, "rule deleted");
}

void FW_Req_ApiToggle(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	sqlite3_stmt* stmt = NULL;
	xvalue tblForm;
	str sIdStr, sEnabledStr;
	int64 iId;
	int iEnabled;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xsHttpReplyAuto(objResp, 405, "Content-Type: text/plain\r\n", "method not allowed", 0);
		return;
	}

	tblForm = FW_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		FW_SendError(objResp, "invalid request body");
		return;
	}

	sIdStr = xvoTableGetText(tblForm, "id", 2);
	sEnabledStr = xvoTableGetText(tblForm, "enabled", 7);

	if ( !sIdStr || !sIdStr[0] ) {
		xvoUnref(tblForm);
		FW_SendError(objResp, "id is required");
		return;
	}

	iId = xrtStrToI64((char*)sIdStr);
	iEnabled = sEnabledStr ? atoi((char*)sEnabledStr) : 0;

	if ( FW_OpenDb(&pDb) ) {
		if ( sqlite3_prepare_v2(pDb, "UPDATE firewall_rules SET enabled = ? WHERE id = ?;", -1, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int(stmt, 1, iEnabled);
			sqlite3_bind_int64(stmt, 2, iId);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		FW_CloseDb(pDb);
	}

	xvoUnref(tblForm);

	FW_ApplyAllRules();

	FW_SendOk(objResp, iEnabled ? "rule enabled" : "rule disabled");
}

void FW_Req_ApiSave(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	sqlite3* pDb = NULL;
	xvalue tblForm;
	xvalue arrRules;
	int iRuleCount;
	int i;

	(void)objServer; (void)objHost; (void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xsHttpReplyAuto(objResp, 405, "Content-Type: text/plain\r\n", "method not allowed", 0);
		return;
	}

	tblForm = FW_ParseJsonBody(objReq);
	if ( tblForm == NULL ) {
		FW_SendError(objResp, "invalid request body");
		return;
	}

	arrRules = xvoTableGetValue(tblForm, "rules", 5);
	if ( arrRules == NULL || xvoType(arrRules) != XVO_DT_ARRAY ) {
		xvoUnref(tblForm);
		FW_SendError(objResp, "rules array is required");
		return;
	}

	iRuleCount = (int)xvoArrayLength(arrRules);

	if ( FW_OpenDb(&pDb) ) {
		sqlite3_exec(pDb, "DELETE FROM firewall_rules;", NULL, NULL, NULL);

		for ( i = 0; i < iRuleCount; i++ ) {
			xvalue tblRule = xvoArrayGetValue(arrRules, i);
			if ( tblRule && xvoType(tblRule) == XVO_DT_TABLE ) {
				str sProtocol = xvoTableGetText(tblRule, "protocol", 8);
				str sPort = xvoTableGetText(tblRule, "port", 4);
				str sSourceIp = xvoTableGetText(tblRule, "source_ip", 9);
				str sAction = xvoTableGetText(tblRule, "action", 6);
				str sComment = xvoTableGetText(tblRule, "comment", 7);
				int64 iEnabled = xvoTableGetInt(tblRule, "enabled", 7);
				sqlite3_stmt* stmt = NULL;

				if ( sqlite3_prepare_v2(pDb, "INSERT INTO firewall_rules (protocol, port, source_ip, action, comment, enabled) VALUES (?, ?, ?, ?, ?, ?);", -1, &stmt, NULL) == SQLITE_OK ) {
					sqlite3_bind_text(stmt, 1, sProtocol ? (char*)sProtocol : "tcp", -1, SQLITE_TRANSIENT);
					sqlite3_bind_text(stmt, 2, sPort ? (char*)sPort : "", -1, SQLITE_TRANSIENT);
					sqlite3_bind_text(stmt, 3, sSourceIp ? (char*)sSourceIp : "", -1, SQLITE_TRANSIENT);
					sqlite3_bind_text(stmt, 4, sAction ? (char*)sAction : "ACCEPT", -1, SQLITE_TRANSIENT);
					sqlite3_bind_text(stmt, 5, sComment ? (char*)sComment : "", -1, SQLITE_TRANSIENT);
					sqlite3_bind_int(stmt, 6, iEnabled ? 1 : 0);
					sqlite3_step(stmt);
					sqlite3_finalize(stmt);
				}
			}
		}

		FW_CloseDb(pDb);
	}

	xvoUnref(tblForm);

	FW_ApplyAllRules();

	FW_SendOk(objResp, "rules saved and applied");
}

#endif

int FW_OnLoad(XAdminPluginHandle* out_handle)
{
	if ( out_handle ) G_FWHandle = *out_handle;
	return 0;
}

int FW_OnInstall(XAdminPluginHandle handle)
{
	(void)handle;
	FW_EnsureSchema();
	return 0;
}

int FW_OnStart(XAdminPluginHandle handle)
{
	XAdminMenuDecl menu;
	XAdminRouteDecl route;
	int iAuthGroupId = 0;
	int iAuthId = 0;
	XAdminAuthGroupDecl authGroup;
	XAdminAuthDecl auth;

	G_FWHandle = handle;

	if ( !FW_EnsureSchema() ) return -1;

#ifndef _WIN32
	if ( G_FWConfig.bAutoApplyOnStart ) {
		FW_ApplyAllRules();
	}
#endif

	memset(&authGroup, 0, sizeof(authGroup));
	authGroup.scope = XADMIN_AUTH_SCOPE_ADMIN;
	authGroup.name = "Firewall";
	authGroup.description = "Firewall management permissions";
	authGroup.sort = 200000;
	if ( XAdmin_RegisterAuthGroup(handle, &authGroup, &iAuthGroupId, NULL) != 0 ) return -1;

	memset(&auth, 0, sizeof(auth));
	auth.scope = XADMIN_AUTH_SCOPE_ADMIN;
	auth.group_id = iAuthGroupId;
	auth.name = "firewall.manage";
	auth.description = "Manage firewall rules";
	auth.sort = 200001;
	if ( XAdmin_RegisterAuth(handle, &auth, &iAuthId, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/firewall";
	route.proc = FW_Req_ViewPage;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/firewall/list";
	route.proc = FW_Req_ApiList;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/firewall/add";
	route.proc = FW_Req_ApiAdd;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/firewall/delete";
	route.proc = FW_Req_ApiDelete;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/firewall/toggle";
	route.proc = FW_Req_ApiToggle;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/firewall/save";
	route.proc = FW_Req_ApiSave;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&menu, 0, sizeof(menu));
	menu.title = "Firewall";
	menu.icon = "layui-icon layui-icon-auz";
	menu.type = 1;
	menu.open_type = "_iframe";
	menu.href = "/admin/view/plugin/firewall";
	menu.sort = 200;
	menu.visible = TRUE;
	menu.remark = "Firewall rule management";
	if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) return -1;

	printf("[firewall] started\n");
	return 0;
}

int FW_OnConfigChanged(XAdminPluginHandle handle, xvalue new_cfg)
{
	(void)handle;
	memset(&G_FWConfig, 0, sizeof(G_FWConfig));
	G_FWConfig.bAutoApplyOnStart = 1;
	if ( new_cfg && (xvoType(new_cfg) == XVO_DT_TABLE) ) {
		int64 iVal = xvoTableGetInt(new_cfg, "autoApplyOnStart", 16);
		G_FWConfig.bAutoApplyOnStart = (iVal != 0) ? 1 : 0;
	}
	return 0;
}

int FW_OnHealthCheck(XAdminPluginHandle handle, XAdminHealthReport* out_report)
{
	(void)handle;
	if ( out_report ) {
		out_report->status_code = 0;
		out_report->message = "ok";
	}
	return 0;
}

void FW_OnStop(XAdminPluginHandle handle)
{
	(void)handle;
	G_FWHandle = NULL;
	printf("[firewall] stopped\n");
}

void FW_OnUnload(XAdminPluginHandle handle)
{
	(void)handle;
	G_FWHandle = NULL;
	G_FWMainDb = NULL;
	G_FWXid = NULL;
	G_FWRootPath = NULL;
	G_FWDataPath = NULL;
	G_FWPrivateDbPath = NULL;
}

static XAdminPluginDescriptor G_FWPlugin = {
	XADMIN_ABI_VERSION,
	sizeof(XAdminPluginDescriptor),
	"firewall",
	"1.0.0",
	"Firewall Manager",
	FW_OnLoad,
	FW_OnInstall,
	FW_OnStart,
	FW_OnConfigChanged,
	FW_OnHealthCheck,
	FW_OnStop,
	FW_OnUnload
};

XADMIN_DECLARE_PLUGIN(G_FWPlugin)