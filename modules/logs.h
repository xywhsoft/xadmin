

// prepared sql
sqlite3_stmt* stmt_logs_all = NULL;
sqlite3_stmt* stmt_logs_sel = NULL;
sqlite3_stmt* stmt_logs_add = NULL;
sqlite3_stmt* stmt_logs_clear = NULL;



// init logs module
void Logs_Init()
{
	printf("        Logs_Init \n");

	int iRet = sqlite3_prepare_v3(G_DB, "SELECT *, COUNT(*) OVER() AS total_count FROM logs ORDER BY id DESC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_logs_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Logs_Init [stmt_logs_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT *, COUNT(*) OVER() AS total_count FROM logs WHERE uri LIKE ? ORDER BY id DESC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_logs_sel, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Logs_Init [stmt_logs_sel] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "INSERT INTO logs (user, ip, uri, method, param, body, createTime) VALUES (?, ?, ?, ?, ?, ?, ?);", -1, SQL_PREPARE_DEFAULT, &stmt_logs_add, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Logs_Init [stmt_logs_add] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "DELETE FROM logs WHERE createTime < ?;", -1, SQL_PREPARE_DEFAULT, &stmt_logs_clear, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Logs_Init [stmt_logs_clear] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
}



// add access log
void Logs_Add(XS_RequestObject objReq, xvalue* objSession)
{
	const char* sUser = "(guest)";
	const char* sIP = xsReqRemote(objReq);
	const char* sURI = xsReqPath(objReq);
	const char* sQuery = xsReqQuery(objReq);
	const char* sMethod = xsReqMethod(objReq);
	const char* pBody = NULL;
	size_t iBodyLen = 0;
	xtime now = XA_Now();

	if ( objSession && (xrtValueType(objSession) == XVO_DT_TABLE) ) {
		sUser = xvoTableGetText(objSession, "user", 4);
		if ( !sUser ) {
			sUser = "(unknown)";
		}
	}
	if ( sIP == NULL || sIP[0] == '\0' ) {
		sIP = "(unknown)";
	}
	if ( sURI == NULL ) {
		sURI = "";
	}
	if ( sQuery == NULL ) {
		sQuery = "";
	}
	if ( sMethod == NULL ) {
		sMethod = "";
	}
	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_POST) || (xsReqMethodID(objReq) == XHTTPD_METHOD_PUT) ) {
		pBody = (str)xsReqBody(objReq);
		iBodyLen = xsReqBodyLen(objReq);
	}
	if ( pBody == NULL ) {
		pBody = "";
		iBodyLen = 0;
	}

	sqlite3_bind_text(stmt_logs_add, 1, sUser, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt_logs_add, 2, sIP, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt_logs_add, 3, sURI, (int)strlen(sURI), SQLITE_STATIC);
	sqlite3_bind_text(stmt_logs_add, 4, sMethod, (int)strlen(sMethod), SQLITE_STATIC);
	sqlite3_bind_text(stmt_logs_add, 5, sQuery, (int)strlen(sQuery), SQLITE_STATIC);
	sqlite3_bind_text(stmt_logs_add, 6, pBody, (int)iBodyLen, SQLITE_STATIC);
	sqlite3_bind_int64(stmt_logs_add, 7, now);
	sqlite3_step(stmt_logs_add);
	sqlite3_reset(stmt_logs_add);
}



// free logs module
void Logs_Unit()
{
	printf("        Logs_Unit \n");
	sqlite3_finalize(stmt_logs_all);
	sqlite3_finalize(stmt_logs_sel);
	sqlite3_finalize(stmt_logs_add);
	sqlite3_finalize(stmt_logs_clear);
}
