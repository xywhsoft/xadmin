


// 获取 logs 管理页面视图
void Request_View_Logs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		
		// 日志页面
		LoadPage(objResp, 200, HTTP_CT_HTML, "logs.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// logs 主接�?
void Request_Logs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		
		// �?URL 查询字符串中提取参数
		char sParam[64];
		xsReqQueryValue(objReq, "page", sParam, sizeof(sParam));
		int64 iPage = xrtStrToI64(sParam);
		if ( iPage <= 0 ) { iPage = 1; }
		xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam));
		int64 iLimit = xrtStrToI64(sParam);
		if ( iLimit <= 0 ) { iLimit = 10; }
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = xsReqQueryValue(objReq, "search", sParam, sizeof(sParam));
		
		// 从数据库中查询数�?
		xvalue* data = xvoCreateArray();
		int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_int64(stmt_logs_all, 1, iLimit);
			sqlite3_bind_int64(stmt_logs_all, 2, iOffset);
			while ( sqlite3_step(stmt_logs_all) == SQLITE_ROW ) {
				xvalue* tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_logs_all, 0));
				xvoTableSetText(tblRow, "user", 4, (str)sqlite3_column_text(stmt_logs_all, 1), 0, FALSE);
				xvoTableSetText(tblRow, "ip", 2, (str)sqlite3_column_text(stmt_logs_all, 2), 0, FALSE);
				xvoTableSetText(tblRow, "uri", 3, (str)sqlite3_column_text(stmt_logs_all, 3), 0, FALSE);
				xvoTableSetText(tblRow, "method", 6, (str)sqlite3_column_text(stmt_logs_all, 4), 0, FALSE);
				xvoTableSetText(tblRow, "param", 5, (str)sqlite3_column_text(stmt_logs_all, 5), 0, FALSE);
				xvoTableSetText(tblRow, "body", 4, (str)sqlite3_column_text(stmt_logs_all, 6), 0, FALSE);
				xtime iTime = sqlite3_column_int64(stmt_logs_all, 7);
				xvoTableSetText(tblRow, "createTime", 10, XA_TimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				if ( iCount <= 0 ) {
					iCount = sqlite3_column_int64(stmt_logs_all, 8);
				}
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_logs_all);
		} else {
			// 筛�?
			sqlite3_bind_text(stmt_logs_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_logs_sel, 2, iLimit);
			sqlite3_bind_int64(stmt_logs_sel, 3, iOffset);
			while ( sqlite3_step(stmt_logs_sel) == SQLITE_ROW ) {
				xvalue* tblRow = xvoCreateTable();
				xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_logs_sel, 0));
				xvoTableSetText(tblRow, "user", 4, (str)sqlite3_column_text(stmt_logs_sel, 1), 0, FALSE);
				xvoTableSetText(tblRow, "ip", 2, (str)sqlite3_column_text(stmt_logs_sel, 2), 0, FALSE);
				xvoTableSetText(tblRow, "uri", 3, (str)sqlite3_column_text(stmt_logs_sel, 3), 0, FALSE);
				xvoTableSetText(tblRow, "method", 6, (str)sqlite3_column_text(stmt_logs_sel, 4), 0, FALSE);
				xvoTableSetText(tblRow, "param", 5, (str)sqlite3_column_text(stmt_logs_sel, 5), 0, FALSE);
				xvoTableSetText(tblRow, "body", 4, (str)sqlite3_column_text(stmt_logs_sel, 6), 0, FALSE);
				xtime iTime = sqlite3_column_int64(stmt_logs_sel, 7);
				xvoTableSetText(tblRow, "createTime", 10, XA_TimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
				if ( iCount <= 0 ) {
					iCount = sqlite3_column_int64(stmt_logs_sel, 8);
				}
				xvoArrayAppendValue(data, tblRow, TRUE);
			}
			sqlite3_reset(stmt_logs_sel);
		}
		
		// 构建返回�?
		xvalue* tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetInt(tblRet, "code", 4, 0);
		xvoTableSetInt(tblRet, "count", 5, iCount);
		xvoTableSetText(tblRet, "message", 7, "��־���ݻ�ȡ�ɹ���", 0, FALSE);
		xvoTableSetValue(tblRet, "data", 4, data, TRUE);
		
		// 生成 JSON
		size_t iRetSize = 0;
		char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xvoUnref(tblRet);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 清理 7 天前的日�?
void Request_Logs_Clear(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		
		// 清理数据�?
		xtime tDay7 = XA_Now() - (7 * 24 * 60 * 60);
		sqlite3_bind_int64(stmt_logs_clear, 1, tDay7);
		sqlite3_step(stmt_logs_clear);
		sqlite3_reset(stmt_logs_clear);
		
		// 返回结果
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"7��ǰ����־����գ�\"}", 0);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}


