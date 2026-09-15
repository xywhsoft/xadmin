


// 获取 logs 管理页面视图
void Request_View_Logs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 日志页面
		LoadPage(objResp, 200, HTTP_CT_HTML, "logs.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// logs 主接口
void Request_Logs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		
		// 从 URL 查询字符串中提取参数
		char sParam[64];
		xsReqQueryValue(objReq, "page", sParam, sizeof(sParam));
		int64 iPage = Util_ParseI64(sParam);
		if ( iPage <= 0 ) { iPage = 1; }
		xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam));
		int64 iLimit = Util_ParseI64(sParam);
		if ( iLimit <= 0 ) { iLimit = 10; }
		if ( iLimit > 100 ) { iLimit = 100; }	/* F9 */
		int64 iOffset = (iPage - 1) * iLimit;
		int iSize = xsReqQueryValue(objReq, "search", sParam, sizeof(sParam));
		
		// 从数据库中查询数据
		xvalue* data = ValueArray();
		int64 iCount = 0;
		if ( iSize <= 0 ) {
			// 查询全部
			sqlite3_bind_int64(stmt_logs_all, 1, iLimit);
			sqlite3_bind_int64(stmt_logs_all, 2, iOffset);
			while ( sqlite3_step(stmt_logs_all) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject();
				ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_logs_all, 0));
				ValueSetText(tblRow, "user", (str)sqlite3_column_text(stmt_logs_all, 1));
				ValueSetText(tblRow, "ip", (str)sqlite3_column_text(stmt_logs_all, 2));
				ValueSetText(tblRow, "uri", (str)sqlite3_column_text(stmt_logs_all, 3));
				ValueSetText(tblRow, "method", (str)sqlite3_column_text(stmt_logs_all, 4));
				ValueSetText(tblRow, "param", (str)sqlite3_column_text(stmt_logs_all, 5));
				ValueSetText(tblRow, "body", (str)sqlite3_column_text(stmt_logs_all, 6));
				xtime iTime = sqlite3_column_int64(stmt_logs_all, 7);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_logs_all);
		} else {
			// 筛选
			sqlite3_bind_text(stmt_logs_sel, 1, sParam, iSize, NULL);
			sqlite3_bind_int64(stmt_logs_sel, 2, iLimit);
			sqlite3_bind_int64(stmt_logs_sel, 3, iOffset);
			while ( sqlite3_step(stmt_logs_sel) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject();
				ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_logs_sel, 0));
				ValueSetText(tblRow, "user", (str)sqlite3_column_text(stmt_logs_sel, 1));
				ValueSetText(tblRow, "ip", (str)sqlite3_column_text(stmt_logs_sel, 2));
				ValueSetText(tblRow, "uri", (str)sqlite3_column_text(stmt_logs_sel, 3));
				ValueSetText(tblRow, "method", (str)sqlite3_column_text(stmt_logs_sel, 4));
				ValueSetText(tblRow, "param", (str)sqlite3_column_text(stmt_logs_sel, 5));
				ValueSetText(tblRow, "body", (str)sqlite3_column_text(stmt_logs_sel, 6));
				xtime iTime = sqlite3_column_int64(stmt_logs_sel, 7);
				ValueSetOwnedText(tblRow, "createTime", TimeText(iTime, TIME_TEXT_DATETIME));
				ValueArrayOwn(data, tblRow);
			}
			sqlite3_reset(stmt_logs_sel);
		}
		
		// 构建返回值
		xvalue* tblRet = ValueObject();
		ValueSetBool(tblRet, "result", true);
		ValueSetInt(tblRet, "code", 0);
		// Independent counting (narrow scan, no materialization of wide rows)
		if ( iSize <= 0 ) {
			if ( sqlite3_step(stmt_logs_count) == SQLITE_ROW ) {
				iCount = sqlite3_column_int64(stmt_logs_count, 0);
			}
			sqlite3_reset(stmt_logs_count);
		} else {
			sqlite3_bind_text(stmt_logs_count_sel, 1, sParam, iSize, NULL);
			if ( sqlite3_step(stmt_logs_count_sel) == SQLITE_ROW ) {
				iCount = sqlite3_column_int64(stmt_logs_count_sel, 0);
			}
			sqlite3_reset(stmt_logs_count_sel);
		}
		
		ValueSetInt(tblRet, "count", iCount);
		ValueSetText(tblRet, "message", "日志数据获取成功！");
		ValueSetOwn(tblRet, "data", data);
		
		// 生成 JSON
		size_t iRetSize = 0;
		char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xrtValueRelease(tblRet);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 清理 7 天前的日志
void Request_Logs_Clear(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		
		// 清理数据库
		xtime tDay7 = xrtNow() - (7LL * 24 * 60 * 60 * 1000000);
		sqlite3_bind_int64(stmt_logs_clear, 1, tDay7);
		if (ReplyIfWriteFailed(objResp, DB_Write(stmt_logs_clear, false))) return;
		
		// 返回结果
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"7天前的日志已清理！\"}", 0);
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}


