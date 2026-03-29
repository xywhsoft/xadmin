#include "plugin.h"

PluginContext* ctx;
xvalue g_tblSettings = NULL;
int g_iMenuId = 0;

void Plugin_SetGlobalData(int idx, void* ptr)
{
	if ( idx == 1 ) {
		ctx = (PluginContext*)ptr;
	} else if ( idx == 2 ) {
		g_tblSettings = (xvalue)ptr;
	}
}

void API_Hello_Greeting(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	str sMessage = "Hello World!";
	bool bShowTime = FALSE;

	if ( g_tblSettings ) {
		str sCustomMsg = xvoTableGetText(g_tblSettings, "welcomeMessage", 14);
		if ( sCustomMsg && strlen(sCustomMsg) > 0 ) {
			sMessage = sCustomMsg;
		}
		bShowTime = xvoTableGetBool(g_tblSettings, "showTime", 8);
	}

	xvalue objRet = xvoCreateTable();
	xvoTableSetBool(objRet, "result", 6, TRUE);
	xvoTableSetText(objRet, "message", 7, sMessage, 0, FALSE);
	if ( bShowTime ) {
		xvoTableSetInt(objRet, "time", 4, ctx->TimeNow());
	}

	size_t iSize = 0;
	str sJson = ctx->JsonStringify(objRet, &iSize);
	ctx->SendJson(objResp, 200, sJson, iSize);
	ctx->Free(sJson);
	xvoUnref(objRet);
}

void API_Hello_Info(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	ctx->SendJson(objResp, 200, "{\"result\":true,\"name\":\"hello\",\"title\":\"Hello World Demo Plugin\",\"version\":\"1.0.0\"}", 0);
}

void API_Debug_ExecuteSQL(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue objReqTbl = ctx->JsonParse((const char*)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !objReqTbl ) {
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"invalid request\"}", 0);
		return;
	}

	str sSQL = xvoTableGetText(objReqTbl, "sql", 3);
	if ( !sSQL || strlen(sSQL) == 0 ) {
		xvoUnref(objReqTbl);
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"missing sql\"}", 0);
		return;
	}

	char* sErr = NULL;
	int64 iStart = ctx->TimeNow();
	int iRet = sqlite3_exec(ctx->pDB, sSQL, NULL, NULL, &sErr);
	int64 iEnd = ctx->TimeNow();

	xvalue objRet = xvoCreateTable();
	xvoTableSetBool(objRet, "result", 6, iRet == SQLITE_OK);
	xvoTableSetInt(objRet, "affectedRows", 11, sqlite3_changes(ctx->pDB));
	xvoTableSetInt(objRet, "time", 4, iEnd - iStart);
	xvoTableSetText(objRet, "sql", 3, sSQL, 0, FALSE);

	if ( sErr ) {
		xvoTableSetText(objRet, "error", 5, sErr, 0, FALSE);
		sqlite3_free(sErr);
	} else {
		xvoTableSetText(objRet, "message", 7, iRet == SQLITE_OK ? "ok" : "failed", 0, FALSE);
	}

	size_t iSize = 0;
	str sJson = ctx->JsonStringify(objRet, &iSize);
	ctx->SendJson(objResp, 200, sJson, iSize);
	ctx->Free(sJson);
	xvoUnref(objRet);
	xvoUnref(objReqTbl);
}

void API_Debug_QueryTable(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue objReqTbl = ctx->JsonParse((const char*)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !objReqTbl ) {
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"invalid request\"}", 0);
		return;
	}

	str sTableName = xvoTableGetText(objReqTbl, "table", 5);
	if ( !sTableName || strlen(sTableName) == 0 ) {
		xvoUnref(objReqTbl);
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"missing table\"}", 0);
		return;
	}

	int iLimit = xvoTableGetInt(objReqTbl, "limit", 5);
	if ( (iLimit <= 0) || (iLimit > 1000) ) {
		iLimit = 100;
	}

	str sSQL = xrtFormat("SELECT * FROM %s LIMIT %d", sTableName, iLimit);
	sqlite3_stmt* stmt = NULL;
	int iRet = sqlite3_prepare_v3(ctx->pDB, sSQL, -1, 0, &stmt, NULL);
	xrtFree(sSQL);

	if ( iRet != SQLITE_OK ) {
		xvoUnref(objReqTbl);
		ctx->SendJson(objResp, 200, "{\"result\":false,\"message\":\"prepare failed\"}", 0);
		return;
	}

	xvalue objArrColumns = xvoCreateArray();
	xvalue objArrRows = xvoCreateArray();
	int iColCount = sqlite3_column_count(stmt);

	for ( int i = 0; i < iColCount; i++ ) {
		const char* sColName = sqlite3_column_name(stmt, i);
		xvoArrayAppendText(objArrColumns, sColName, 0, TRUE);
	}

	int iRowCount = 0;
	while ( (sqlite3_step(stmt) == SQLITE_ROW) && (iRowCount < iLimit) ) {
		xvalue objTblRow = xvoCreateTable();
		for ( int i = 0; i < iColCount; i++ ) {
			const char* sColName = sqlite3_column_name(stmt, i);
			int iColType = sqlite3_column_type(stmt, i);

			if ( iColType == SQLITE_INTEGER ) {
				xvoTableSetInt(objTblRow, sColName, 3, sqlite3_column_int64(stmt, i));
			} else if ( iColType == SQLITE_FLOAT ) {
				xvoTableSetFloat(objTblRow, sColName, 3, sqlite3_column_double(stmt, i));
			} else if ( iColType == SQLITE_TEXT ) {
				xvoTableSetText(objTblRow, sColName, 3, (str)sqlite3_column_text(stmt, i), 0, FALSE);
			} else if ( iColType == SQLITE_BLOB ) {
				xvoTableSetText(objTblRow, sColName, 3, "<BLOB>", 0, FALSE);
			} else {
				xvoTableSetNull(objTblRow, sColName, 3);
			}
		}
		xvoArrayAppendValue(objArrRows, objTblRow, TRUE);
		iRowCount++;
	}

	sqlite3_finalize(stmt);

	xvalue objRet = xvoCreateTable();
	xvoTableSetBool(objRet, "result", 6, TRUE);
	xvoTableSetText(objRet, "table", 5, sTableName, 0, FALSE);
	xvoTableSetInt(objRet, "rowCount", 8, iRowCount);
	xvoTableSetValue(objRet, "columns", 7, objArrColumns, FALSE);
	xvoTableSetValue(objRet, "data", 4, objArrRows, FALSE);

	size_t iSize = 0;
	str sJson = ctx->JsonStringify(objRet, &iSize);
	ctx->SendJson(objResp, 200, sJson, iSize);
	ctx->Free(sJson);
	xvoUnref(objRet);
	xvoUnref(objReqTbl);
}

void View_Hello_Page(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	str sHtml = "<div style='padding:30px;text-align:center;'>"
		"<h1 style='color:#1e9fff;'>Hello World Plugin</h1>"
		"<p>This is a demo plugin for xAdmin.</p>"
		"<p>Version: <code>1.0.0</code></p>"
		"<button class='layui-btn' onclick='testApi()'>Test API</button>"
		"<pre id='result' style='margin-top:20px;text-align:left;background:#f8f8f8;padding:15px;border-radius:4px;'></pre>"
		"</div>"
		"<script>"
		"function testApi() {"
		"fetch('/api/plugin/hello/greeting')"
		".then(function(r){return r.json();})"
		".then(function(data){document.getElementById('result').innerText = JSON.stringify(data, null, 2);});"
		"}"
		"</script>";
	ctx->SendHtml(objResp, 200, sHtml);
}

void Hello_RegisterRoutes()
{
	ctx->AddRoute("/api/plugin/hello/greeting", API_Hello_Greeting, FALSE, FALSE, 0, 0);
	ctx->AddRoute("/api/plugin/hello/info", API_Hello_Info, FALSE, FALSE, 0, 0);
	ctx->AddRoute("/api/debug/execute_sql", API_Debug_ExecuteSQL, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/api/debug/query_table", API_Debug_QueryTable, TRUE, TRUE, 0, 0);
	ctx->AddRoute("/admin/view/plugin/hello", View_Hello_Page, TRUE, TRUE, 0, 0);
}

void Hello_UnregisterRoutes()
{
	ctx->RemoveRoute("/api/plugin/hello/greeting");
	ctx->RemoveRoute("/api/plugin/hello/info");
	ctx->RemoveRoute("/api/debug/execute_sql");
	ctx->RemoveRoute("/api/debug/query_table");
	ctx->RemoveRoute("/admin/view/plugin/hello");
}

void Hello_RegisterMenus()
{
}

void Hello_UnregisterMenus()
{
	if ( g_iMenuId > 0 ) {
		ctx->RemoveMenu(g_iMenuId);
		g_iMenuId = 0;
	}
}

void Hello_OnSystemReady(xvalue eventData)
{
	ctx->Log(LOG_INFO, "[Hello Plugin] System ready event received!");
}

void Plugin_hello_Init()
{
	ctx->Log(LOG_INFO, "[Hello Plugin] Initializing...");

	Hello_RegisterRoutes();
	Hello_RegisterMenus();
	ctx->OnEvent(EVENT_SYSTEM_READY, Hello_OnSystemReady);

	ctx->Log(LOG_INFO, "[Hello Plugin] Initialized successfully!");
}

void Plugin_hello_Unit()
{
	ctx->Log(LOG_INFO, "[Hello Plugin] Unloading...");

	ctx->OffEvent(EVENT_SYSTEM_READY, Hello_OnSystemReady);
	Hello_UnregisterRoutes();
	Hello_UnregisterMenus();

	ctx->Log(LOG_INFO, "[Hello Plugin] Unloaded successfully!");
}
