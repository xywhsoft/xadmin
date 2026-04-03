


// 鑾峰彇 logs 绠＄悊椤甸潰瑙嗗浘
void Request_View_Logs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 鏃ュ織椤甸潰
		LoadPage(objResp, 200, HTTP_CT_HTML, "logs.html");
		
	} else {
		
		// 鍏朵粬璇锋眰鏂规硶杩斿洖 404 椤甸潰
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// logs 涓绘帴鍙?
void Request_Logs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "GET") ) {
		
		// 浠?URL 鏌ヨ瀛楃涓蹭腑鎻愬彇鍙傛暟
		char sParam[64];
		HttpGetQueryVar(objReq, "page", sParam, sizeof(sParam));
		int64 iPage = xrtStrToI64(sParam);
		if ( iPage <= 0 ) { iPage = 1; }
		HttpGetQueryVar(objReq, "limit", sParam, sizeof(sParam));
		int64 iLimit = xrtStrToI64(sParam);
		if ( iLimit <= 0 ) { iLimit = 10; }
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = HttpGetQueryVar(objReq, "search", sParam, sizeof(sParam));
		
		// 浠庢暟鎹簱涓煡璇㈡暟鎹?
		xvalue data = xvoCreateArray();
		int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 鏌ヨ鍏ㄩ儴
			sqlite3_bind_int64(stmt_logs_all, 1, iLimit);
			sqlite3_bind_int64(stmt_logs_all, 2, iOffset);
			while ( sqlite3_step(stmt_logs_all) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_logs_all, 0));
				xvoTableSetText(tblRow, "user", 4, (str)sqlite3_column_text(stmt_logs_all, 1), 0, FALSE);
				xvoTableSetText(tblRow, "ip", 2, (str)sqlite3_column_text(stmt_logs_all, 2), 0, FALSE);
				xvoTableSetText(tblRow, "uri", 3, (str)sqlite3_column_text(stmt_logs_all, 3), 0, FALSE);
				xvoTableSetText(tblRow, "method", 6, (str)sqlite3_column_text(stmt_logs_all, 4), 0, FALSE);
				xvoTableSetText(tblRow, "param", 5, (str)sqlite3_column_text(stmt_logs_all, 5), 0, FALSE);
				xvoTableSetText(tblRow, "body", 4, (str)sqlite3_column_text(stmt_logs_all, 6), 0, FALSE);
				xtime iTime = sqlite3_column_int64(stmt_logs_all, 7);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				if ( iCount <= 0 ) {
					iCount = sqlite3_column_int64(stmt_logs_all, 8);
				}
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_logs_all);
		} else {
			// 绛涢�?
			sqlite3_bind_text(stmt_logs_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_logs_sel, 2, iLimit);
			sqlite3_bind_int64(stmt_logs_sel, 3, iOffset);
			while ( sqlite3_step(stmt_logs_sel) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_logs_sel, 0));
				xvoTableSetText(tblRow, "user", 4, (str)sqlite3_column_text(stmt_logs_sel, 1), 0, FALSE);
				xvoTableSetText(tblRow, "ip", 2, (str)sqlite3_column_text(stmt_logs_sel, 2), 0, FALSE);
				xvoTableSetText(tblRow, "uri", 3, (str)sqlite3_column_text(stmt_logs_sel, 3), 0, FALSE);
				xvoTableSetText(tblRow, "method", 6, (str)sqlite3_column_text(stmt_logs_sel, 4), 0, FALSE);
				xvoTableSetText(tblRow, "param", 5, (str)sqlite3_column_text(stmt_logs_sel, 5), 0, FALSE);
				xvoTableSetText(tblRow, "body", 4, (str)sqlite3_column_text(stmt_logs_sel, 6), 0, FALSE);
				xtime iTime = sqlite3_column_int64(stmt_logs_sel, 7);
				xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				if ( iCount <= 0 ) {
					iCount = sqlite3_column_int64(stmt_logs_sel, 8);
				}
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_logs_sel);
		}
		
		// 鏋勫缓杩斿洖鍊?
		xvalue tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetInt(tblRet, "code", 4, 0);
		xvoTableSetInt(tblRet, "count", 5, iCount);
		xvoTableSetText(tblRet, "message", 7, "日志数据获取成功！", 0, FALSE);
		xvoTableSetValue(tblRet, "data", 4, data, TRUE);
		
		// 鐢熸垚 JSON
		size_t iRetSize = 0;
		char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
		http_reply(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xvoUnref(tblRet);
		
	} else {
		
		// 鍏朵粬璇锋眰鏂规硶杩斿洖 404 椤甸潰
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 娓呯悊 7 澶╁墠鐨勬棩蹇?
void Request_Logs_Clear(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( HttpMethodIs(objReq, "POST") ) {
		
		// 娓呯悊鏁版嵁搴?
		xtime tDay7 = xrtNow() - (7 * 24 * 60 * 60);
		sqlite3_bind_int64(stmt_logs_clear, 1, tDay7);
		sqlite3_step(stmt_logs_clear);
		sqlite3_reset(stmt_logs_clear);
		
		// 杩斿洖缁撴灉
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"7天前的日志已清空！\"}", 0);
		
	} else {
		
		// 鍏朵粬璇锋眰鏂规硶杩斿洖 404 椤甸潰
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}


