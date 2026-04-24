


// ==================== 附件上传 ====================

void Request_Attachment_Upload(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method Not Allowed\"}", 0);
		return;
	}
	
	// 获取上传者信息
	int64 iUploaderId = xvoTableGetInt(objSession, "id", 2);
	int iUploaderType = 1;  // 后台管理员
	
	// 解析 multipart
	xrtmultipartpartview part;
	size_t ofs = 0;
	str sFilename = NULL;
	str sExt = NULL;
	str sModelName = NULL;
	int64 iRecordId = 0;
	ptr pFileData = NULL;
	size_t iFileSize = 0;
	
	while ( xsReqMultipartNext(objReq, &ofs, &part) ) {
		if ( xsMultipartNameIs(&part, "file") ) {
			// 文件数据
			pFileData = (ptr)part.tBody.sPtr;
			iFileSize = part.tBody.iLen;
			
			// 提取文件名
			if ( part.tFileName.iLen > 0 ) {
				sFilename = xrtCopyStr((str)part.tFileName.sPtr, part.tFileName.iLen);
				sExt = xrtPathGetExt(sFilename, 0);
				if ( sExt ) {
					// 转小写
					for ( str p = sExt; *p; p++ ) {
						if ( *p >= 'A' && *p <= 'Z' ) *p += 32;
					}
				}
			}
		} else if ( xsMultipartNameIs(&part, "modelName") ) {
			sModelName = xrtCopyStr((str)part.tBody.sPtr, part.tBody.iLen);
		} else if ( xsMultipartNameIs(&part, "recordId") ) {
			char sTmp[24] = {0};
			size_t iLen = part.tBody.iLen < 23 ? part.tBody.iLen : 23;
			memcpy(sTmp, part.tBody.sPtr, iLen);
			iRecordId = xrtStrToI64(sTmp);
		}
	}
	
	// 验证文件
	if ( !pFileData || iFileSize == 0 || !sFilename || !sExt ) {
		if ( sFilename ) xrtFree(sFilename);
		if ( sExt ) xrtFree(sExt);
		if ( sModelName ) xrtFree(sModelName);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"No file uploaded\"}", 0);
		return;
	}
	
	// 检查扩展名
	if ( !Attachment_IsExtAllowed(sExt) ) {
		xrtFree(sFilename);
		xrtFree(sExt);
		if ( sModelName ) xrtFree(sModelName);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"File type not allowed\"}", 0);
		return;
	}
	
	// 检查文件大小
	int64 iMaxSize = Attachment_GetMaxSize();
	if ( iMaxSize > 0 && (int64)iFileSize > iMaxSize ) {
		xrtFree(sFilename);
		xrtFree(sExt);
		if ( sModelName ) xrtFree(sModelName);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"File too large\"}", 0);
		return;
	}
	
	// 检查用户配额
	int64 iQuota = Attachment_GetUserQuota();
	if ( iQuota > 0 ) {
		int64 iUsage = Attachment_GetUserUsage(iUploaderId, iUploaderType);
		if ( iUsage + (int64)iFileSize > iQuota ) {
			xrtFree(sFilename);
			xrtFree(sExt);
			if ( sModelName ) xrtFree(sModelName);
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Storage quota exceeded\"}", 0);
			return;
		}
	}
	
	// 生成 XID
	str sXID = xrtMakeXIDS();
	
	// 生成存储路径
	str sPath = Attachment_GeneratePath(sModelName, sXID, sExt);
	
	// 确保目录存在
	Attachment_EnsureDir(sPath);
	
	// 保存文件
	str sFullPath = xrtPathJoin(2, AttachmentPath, sPath);
	FILE* fp = fopen(sFullPath, "wb");
	if ( !fp ) {
		xrtFree(sFullPath);
		xrtFree(sPath);
		xrtFree(sXID);
		xrtFree(sFilename);
		xrtFree(sExt);
		if ( sModelName ) xrtFree(sModelName);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Failed to save file\"}", 0);
		return;
	}
	fwrite(pFileData, 1, iFileSize, fp);
	fclose(fp);
	xrtFree(sFullPath);
	
	// 获取 MIME 类型
	MimeMapping* pMime = Attachment_GetMime(sExt);
	str sMime = pMime ? pMime->mime : (str)"application/octet-stream";
	
	// 获取默认配置
	xvalue tblAttachment = xvoTableGetValue(G_Option, "attachment", 10);
	int iAllowHotlink = tblAttachment ? (xvoTableGetBool(tblAttachment, "defaultHotlink", 14) ? 1 : 0) : 1;
	int iAccessType = tblAttachment ? (int)xvoTableGetInt(tblAttachment, "defaultAccessType", 17) : 0;
	
	// 添加数据库记录
	bool bOK = Attachment_Add(sXID, sFilename, sExt, sMime, iFileSize, sPath,
							  sModelName, iRecordId, iUploaderId, iUploaderType,
							  iAllowHotlink, iAccessType, 0, 0, 0);
	
	if ( bOK ) {
		str sJson = xrtFormat("{\"result\":true,\"data\":{\"xid\":\"%s\",\"filename\":\"%s\",\"ext\":\"%s\",\"size\":%lld,\"url\":\"/attachment?xid=%s\"}}",
							  sXID, sFilename, sExt, (int64)iFileSize, sXID);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sJson, 0);
		xrtFree(sJson);
	} else {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Failed to save record\"}", 0);
	}
	
	xrtFree(sPath);
	xrtFree(sXID);
	xrtFree(sFilename);
	xrtFree(sExt);
	if ( sModelName ) xrtFree(sModelName);
}



// ==================== 附件列表 ====================

void Request_Attachment_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	// 获取分页参数
	char sPage[12], sLimit[12];
	xsReqQueryValue(objReq, "page", sPage, sizeof(sPage));
	xsReqQueryValue(objReq, "limit", sLimit, sizeof(sLimit));
	int iPage = atoi(sPage);
	int iLimit = atoi(sLimit);
	if ( iPage < 1 ) iPage = 1;
	if ( iLimit < 1 ) iLimit = 20;
	if ( iLimit > 100 ) iLimit = 100;
	int iOffset = (iPage - 1) * iLimit;
	
	// 获取筛选参数
	char sModelName[64] = {0};
	char sExt[16] = {0};
	char sAccessType[8] = {0};
	xsReqQueryValue(objReq, "modelName", sModelName, sizeof(sModelName));
	xsReqQueryValue(objReq, "ext", sExt, sizeof(sExt));
	xsReqQueryValue(objReq, "accessType", sAccessType, sizeof(sAccessType));
	
	// 构建查询
	xbuffer_struct bufSQL;
	xrtBufferInit(&bufSQL, 256);
	xrtBufferAppend(&bufSQL, "SELECT xid, filename, ext, mime, size, modelName, recordId, "
		"uploaderId, uploaderType, allowHotlink, accessType, accessLevel, price, priceType, salesCount, downloadCount, createTime "
		"FROM attachment WHERE isDelete = 0", 0, XBUF_ANSI);
	
	if ( sModelName[0] ) {
		xrtBufferAppend(&bufSQL, " AND modelName = '", 0, XBUF_ANSI);
		xrtBufferAppend(&bufSQL, sModelName, 0, XBUF_ANSI);
		xrtBufferAppend(&bufSQL, "'", 0, XBUF_ANSI);
	}
	if ( sExt[0] ) {
		xrtBufferAppend(&bufSQL, " AND ext = '", 0, XBUF_ANSI);
		xrtBufferAppend(&bufSQL, sExt, 0, XBUF_ANSI);
		xrtBufferAppend(&bufSQL, "'", 0, XBUF_ANSI);
	}
	if ( sAccessType[0] ) {
		xrtBufferAppend(&bufSQL, " AND accessType = ", 0, XBUF_ANSI);
		xrtBufferAppend(&bufSQL, sAccessType, 0, XBUF_ANSI);
	}
	
	xrtBufferAppend(&bufSQL, " ORDER BY createTime DESC LIMIT ? OFFSET ?", 0, XBUF_ANSI);
	
	sqlite3_stmt* stmt;
	sqlite3_prepare_v3(G_DB, bufSQL.Buffer, -1, 0, &stmt, NULL);
	sqlite3_bind_int(stmt, 1, iLimit);
	sqlite3_bind_int(stmt, 2, iOffset);
	
	xvalue arrData = xvoCreateArray();
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		xvoTableSetText(tblRow, "xid", 3, (str)sqlite3_column_text(stmt, 0), 0, FALSE);
		xvoTableSetText(tblRow, "filename", 8, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
		xvoTableSetText(tblRow, "ext", 3, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
		xvoTableSetText(tblRow, "mime", 4, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
		xvoTableSetInt(tblRow, "size", 4, sqlite3_column_int64(stmt, 4));
		xvoTableSetText(tblRow, "modelName", 9, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
		xvoTableSetInt(tblRow, "recordId", 8, sqlite3_column_int64(stmt, 6));
		xvoTableSetInt(tblRow, "uploaderId", 10, sqlite3_column_int64(stmt, 7));
		xvoTableSetInt(tblRow, "uploaderType", 12, sqlite3_column_int(stmt, 8));
		xvoTableSetBool(tblRow, "allowHotlink", 12, sqlite3_column_int(stmt, 9));
		xvoTableSetInt(tblRow, "accessType", 10, sqlite3_column_int(stmt, 10));
		xvoTableSetInt(tblRow, "accessLevel", 11, sqlite3_column_int(stmt, 11));
		xvoTableSetInt(tblRow, "price", 5, sqlite3_column_int64(stmt, 12));
		xvoTableSetInt(tblRow, "priceType", 9, sqlite3_column_int(stmt, 13));
		xvoTableSetInt(tblRow, "salesCount", 10, sqlite3_column_int(stmt, 14));
		xvoTableSetInt(tblRow, "downloadCount", 13, sqlite3_column_int(stmt, 15));
		xtime iTime = sqlite3_column_int64(stmt, 16);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_finalize(stmt);
	
	// 统计总数
	xrtBufferUnit(&bufSQL);
	xrtBufferInit(&bufSQL, 256);
	xrtBufferAppend(&bufSQL, "SELECT COUNT(*) FROM attachment WHERE isDelete = 0", 0, XBUF_ANSI);
	if ( sModelName[0] ) {
		xrtBufferAppend(&bufSQL, " AND modelName = '", 0, XBUF_ANSI);
		xrtBufferAppend(&bufSQL, sModelName, 0, XBUF_ANSI);
		xrtBufferAppend(&bufSQL, "'", 0, XBUF_ANSI);
	}
	if ( sExt[0] ) {
		xrtBufferAppend(&bufSQL, " AND ext = '", 0, XBUF_ANSI);
		xrtBufferAppend(&bufSQL, sExt, 0, XBUF_ANSI);
		xrtBufferAppend(&bufSQL, "'", 0, XBUF_ANSI);
	}
	if ( sAccessType[0] ) {
		xrtBufferAppend(&bufSQL, " AND accessType = ", 0, XBUF_ANSI);
		xrtBufferAppend(&bufSQL, sAccessType, 0, XBUF_ANSI);
	}
	
	sqlite3_prepare_v3(G_DB, bufSQL.Buffer, -1, 0, &stmt, NULL);
	int64 iCount = 0;
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt, 0);
	}
	sqlite3_finalize(stmt);
	xrtBufferUnit(&bufSQL);
	
	// 构建响应
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}



// ==================== 附件详情 ====================

void Request_Attachment_Get(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sXID[48];
	xsReqQueryValue(objReq, "xid", sXID, sizeof(sXID));
	if ( !sXID[0] ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing xid\"}", 0);
		return;
	}
	
	sqlite3_bind_text(stmt_attachment_get, 1, sXID, -1, NULL);
	if ( sqlite3_step(stmt_attachment_get) != SQLITE_ROW ) {
		sqlite3_reset(stmt_attachment_get);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Not found\"}", 0);
		return;
	}
	
	xvalue tblData = xvoCreateTable();
	xvoTableSetText(tblData, "xid", 3, (str)sqlite3_column_text(stmt_attachment_get, 0), 0, FALSE);
	xvoTableSetText(tblData, "filename", 8, (str)sqlite3_column_text(stmt_attachment_get, 1), 0, FALSE);
	xvoTableSetText(tblData, "ext", 3, (str)sqlite3_column_text(stmt_attachment_get, 2), 0, FALSE);
	xvoTableSetText(tblData, "mime", 4, (str)sqlite3_column_text(stmt_attachment_get, 3), 0, FALSE);
	xvoTableSetInt(tblData, "size", 4, sqlite3_column_int64(stmt_attachment_get, 4));
	xvoTableSetText(tblData, "path", 4, (str)sqlite3_column_text(stmt_attachment_get, 5), 0, FALSE);
	xvoTableSetText(tblData, "modelName", 9, (str)sqlite3_column_text(stmt_attachment_get, 6), 0, FALSE);
	xvoTableSetInt(tblData, "recordId", 8, sqlite3_column_int64(stmt_attachment_get, 7));
	xvoTableSetInt(tblData, "uploaderId", 10, sqlite3_column_int64(stmt_attachment_get, 8));
	xvoTableSetInt(tblData, "uploaderType", 12, sqlite3_column_int(stmt_attachment_get, 9));
	xvoTableSetBool(tblData, "allowHotlink", 12, sqlite3_column_int(stmt_attachment_get, 10));
	xvoTableSetInt(tblData, "accessType", 10, sqlite3_column_int(stmt_attachment_get, 11));
	xvoTableSetInt(tblData, "accessLevel", 11, sqlite3_column_int(stmt_attachment_get, 12));
	xvoTableSetInt(tblData, "price", 5, sqlite3_column_int64(stmt_attachment_get, 13));
	xvoTableSetInt(tblData, "priceType", 9, sqlite3_column_int(stmt_attachment_get, 14));
	xvoTableSetInt(tblData, "salesCount", 10, sqlite3_column_int(stmt_attachment_get, 15));
	xvoTableSetInt(tblData, "downloadCount", 13, sqlite3_column_int(stmt_attachment_get, 16));
	xvoTableSetText(tblData, "remark", 6, (str)sqlite3_column_text(stmt_attachment_get, 17), 0, FALSE);
	xtime iTime = sqlite3_column_int64(stmt_attachment_get, 18);
	xvoTableSetText(tblData, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
	
	sqlite3_reset(stmt_attachment_get);
	
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}



// ==================== 附件保存 ====================

void Request_Attachment_Save(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !(xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method Not Allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		if ( tblForm ) xvoUnref(tblForm);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		return;
	}
	
	str sXID = xvoTableGetText(tblForm, "xid", 3);
	if ( !sXID || !*sXID ) {
		xvoUnref(tblForm);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing xid\"}", 0);
		return;
	}
	
	// 更新记录
	sqlite3_stmt* stmt;
	sqlite3_prepare_v3(G_DB,
		"UPDATE attachment SET allowHotlink=?, accessType=?, accessLevel=?, price=?, priceType=?, remark=? WHERE xid=?",
		-1, 0, &stmt, NULL);
	
	sqlite3_bind_int(stmt, 1, xvoTableGetBool(tblForm, "allowHotlink", 12) ? 1 : 0);
	sqlite3_bind_int(stmt, 2, (int)xvoTableGetInt(tblForm, "accessType", 10));
	sqlite3_bind_int(stmt, 3, (int)xvoTableGetInt(tblForm, "accessLevel", 11));
	sqlite3_bind_int64(stmt, 4, xvoTableGetInt(tblForm, "price", 5));
	sqlite3_bind_int(stmt, 5, (int)xvoTableGetInt(tblForm, "priceType", 9));
	str sRemark = xvoTableGetText(tblForm, "remark", 6);
	sqlite3_bind_text(stmt, 6, sRemark ? sRemark : (str)"", -1, NULL);
	sqlite3_bind_text(stmt, 7, sXID, -1, NULL);
	
	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	xvoUnref(tblForm);
	
	if ( rc == SQLITE_DONE ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"保存成功\"}", 0);
	} else {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"保存失败\"}", 0);
	}
}



// ==================== 附件删除 ====================

void Request_Attachment_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( (!(xsReqMethodID(objReq) == XHTTPD_METHOD_DELETE)) && (!(xsReqMethodID(objReq) == XHTTPD_METHOD_POST)) ) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method Not Allowed\"}", 0);
		return;
	}
	
	char sXID[48];
	xsReqQueryValue(objReq, "xid", sXID, sizeof(sXID));
	if ( !sXID[0] ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing xid\"}", 0);
		return;
	}
	
	// 获取文件路径
	sqlite3_bind_text(stmt_attachment_get, 1, sXID, -1, NULL);
	str sPath = NULL;
	if ( sqlite3_step(stmt_attachment_get) == SQLITE_ROW ) {
		str sTmp = (str)sqlite3_column_text(stmt_attachment_get, 5);  // path
		if ( sTmp ) sPath = xrtCopyStr(sTmp, 0);
	}
	sqlite3_reset(stmt_attachment_get);
	
	// 删除文件
	if ( sPath ) {
		str sFullPath = xrtPathJoin(2, AttachmentPath, sPath);
		xrtFileDelete(sFullPath);
		xrtFree(sFullPath);
		xrtFree(sPath);
	}
	
	// 标记删除
	sqlite3_stmt* stmt;
	sqlite3_prepare_v3(G_DB, "UPDATE attachment SET isDelete = 1 WHERE xid = ?", -1, 0, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, sXID, -1, NULL);
	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	
	if ( rc == SQLITE_DONE ) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"删除成功\"}", 0);
	} else {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"删除失败\"}", 0);
	}
}



// ==================== 存储统计 ====================

void Request_Attachment_Stats(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	
	xvalue tblData = xvoCreateTable();
	
	// 总计
	xvalue tblTotal = xvoCreateTable();
	if ( sqlite3_step(stmt_attachment_stats_total) == SQLITE_ROW ) {
		int64 iCount = sqlite3_column_int64(stmt_attachment_stats_total, 0);
		int64 iSize = sqlite3_column_int64(stmt_attachment_stats_total, 1);
		xvoTableSetInt(tblTotal, "count", 5, iCount);
		xvoTableSetInt(tblTotal, "size", 4, iSize);
		
		// 格式化大小
		char sSizeText[32];
		if ( iSize >= 1073741824 ) {
			sprintf(sSizeText, "%.2f GB", (double)iSize / 1073741824);
		} else if ( iSize >= 1048576 ) {
			sprintf(sSizeText, "%.2f MB", (double)iSize / 1048576);
		} else if ( iSize >= 1024 ) {
			sprintf(sSizeText, "%.2f KB", (double)iSize / 1024);
		} else {
			sprintf(sSizeText, "%lld B", iSize);
		}
		xvoTableSetText(tblTotal, "sizeText", 8, sSizeText, 0, FALSE);
	}
	sqlite3_reset(stmt_attachment_stats_total);
	xvoTableSetValue(tblData, "total", 5, tblTotal, TRUE);
	
	// 按模型统计
	xvalue arrByModel = xvoCreateArray();
	while ( sqlite3_step(stmt_attachment_stats_by_model) == SQLITE_ROW ) {
		xvalue tblItem = xvoCreateTable();
		str sModel = (str)sqlite3_column_text(stmt_attachment_stats_by_model, 0);
		xvoTableSetText(tblItem, "modelName", 9, sModel ? sModel : (str)"", 0, FALSE);
		xvoTableSetText(tblItem, "title", 5, (sModel && *sModel) ? sModel : (str)"全局附件", 0, FALSE);
		xvoTableSetInt(tblItem, "count", 5, sqlite3_column_int64(stmt_attachment_stats_by_model, 1));
		xvoTableSetInt(tblItem, "size", 4, sqlite3_column_int64(stmt_attachment_stats_by_model, 2));
		xvoArrayAppendValue(arrByModel, tblItem, TRUE);
	}
	sqlite3_reset(stmt_attachment_stats_by_model);
	xvoTableSetValue(tblData, "byModel", 7, arrByModel, TRUE);
	
	// 按扩展名统计
	xvalue arrByExt = xvoCreateArray();
	while ( sqlite3_step(stmt_attachment_stats_by_ext) == SQLITE_ROW ) {
		xvalue tblItem = xvoCreateTable();
		xvoTableSetText(tblItem, "ext", 3, (str)sqlite3_column_text(stmt_attachment_stats_by_ext, 0), 0, FALSE);
		xvoTableSetInt(tblItem, "count", 5, sqlite3_column_int64(stmt_attachment_stats_by_ext, 1));
		xvoTableSetInt(tblItem, "size", 4, sqlite3_column_int64(stmt_attachment_stats_by_ext, 2));
		xvoArrayAppendValue(arrByExt, tblItem, TRUE);
	}
	sqlite3_reset(stmt_attachment_stats_by_ext);
	xvoTableSetValue(tblData, "byExt", 5, arrByExt, TRUE);
	
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}



// ==================== 访问附件 ====================

void Request_Attachment_Access(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	// 从查询参数获取 XID: /attachment?xid=xxxxx
	char sXID[64];
	int iLen = xsReqQueryValue(objReq, "xid", sXID, sizeof(sXID));
	if ( iLen <= 0 ) {
		xsHttpReplyAuto(objResp, 400, HTTP_CT_JSON, "{\"error\":\"Missing xid parameter\"}", 0);
		return;
	}
	
	// 查询附件
	sqlite3_bind_text(stmt_attachment_get, 1, sXID, -1, NULL);
	if ( sqlite3_step(stmt_attachment_get) != SQLITE_ROW ) {
		sqlite3_reset(stmt_attachment_get);
		xsHttpReplyAuto(objResp, 404, HTTP_CT_JSON, "{\"error\":\"Not found\"}", 0);
		return;
	}
	
	// 读取数据
	str sFilename = xrtCopyStr((str)sqlite3_column_text(stmt_attachment_get, 1), 0);
	str sExt = xrtCopyStr((str)sqlite3_column_text(stmt_attachment_get, 2), 0);
	str sMime = xrtCopyStr((str)sqlite3_column_text(stmt_attachment_get, 3), 0);
	str sPath = xrtCopyStr((str)sqlite3_column_text(stmt_attachment_get, 5), 0);
	int64 iUploaderId = sqlite3_column_int64(stmt_attachment_get, 8);
	int iUploaderType = sqlite3_column_int(stmt_attachment_get, 9);
	int iAllowHotlink = sqlite3_column_int(stmt_attachment_get, 10);
	int iAccessType = sqlite3_column_int(stmt_attachment_get, 11);
	int iAccessLevel = sqlite3_column_int(stmt_attachment_get, 12);
	int64 iPrice = sqlite3_column_int64(stmt_attachment_get, 13);
	int iPriceType = sqlite3_column_int(stmt_attachment_get, 14);
	
	sqlite3_reset(stmt_attachment_get);
	
	// 检查防盗链
	if ( !Attachment_CheckHotlink(objReq, iAllowHotlink) ) {
		xrtFree(sFilename);
		xrtFree(sExt);
		xrtFree(sMime);
		xrtFree(sPath);
		xsHttpReplyAuto(objResp, 403, HTTP_CT_JSON, "{\"error\":\"Hotlink not allowed\"}", 0);
		return;
	}
	
	// 检查访问权限
	if ( iAccessType == 1 ) {
		// 登录可见
		if ( !objSession || objSession->Type != XVO_DT_TABLE ) {
			xrtFree(sFilename);
			xrtFree(sExt);
			xrtFree(sMime);
			xrtFree(sPath);
			xsHttpReplyAuto(objResp, 401, HTTP_CT_JSON, "{\"error\":\"Login required\"}", 0);
			return;
		}
	} else if ( iAccessType == 2 ) {
		// 付费下载
		if ( !objSession || objSession->Type != XVO_DT_TABLE ) {
			xrtFree(sFilename);
			xrtFree(sExt);
			xrtFree(sMime);
			xrtFree(sPath);
			xsHttpReplyAuto(objResp, 401, HTTP_CT_JSON, "{\"error\":\"Login required\"}", 0);
			return;
		}
		
		int64 iMemberId = xvoTableGetInt(objSession, "id", 2);
		
		// 检查是否为上传者
		bool bIsOwner = (iUploaderType == 2 && iUploaderId == iMemberId);
		
		if ( !bIsOwner ) {
			// 检查是否已购买
			if ( !Attachment_CheckPurchased(sXID, iMemberId) ) {
				xrtFree(sFilename);
				xrtFree(sExt);
				xrtFree(sMime);
				xrtFree(sPath);
				
				str sJson = xrtFormat("{\"error\":\"Payment required\",\"price\":%lld,\"priceType\":%d}", iPrice, iPriceType);
				xsHttpReplyAuto(objResp, 402, HTTP_CT_JSON, sJson, 0);
				xrtFree(sJson);
				return;
			}
		}
	} else if ( iAccessType == 3 ) {
		// 权限级别限制
		if ( !objSession || objSession->Type != XVO_DT_TABLE ) {
			xrtFree(sFilename);
			xrtFree(sExt);
			xrtFree(sMime);
			xrtFree(sPath);
			xsHttpReplyAuto(objResp, 401, HTTP_CT_JSON, "{\"error\":\"Login required\"}", 0);
			return;
		}
		
		// 获取用户权限级别
		int iUserLevel = xvoTableGetInt(objSession, "authLevel", 9);
		
		// 检查权限级别是否足够
		if ( iUserLevel < iAccessLevel ) {
			xrtFree(sFilename);
			xrtFree(sExt);
			xrtFree(sMime);
			xrtFree(sPath);
			
			str sJson = xrtFormat("{\"error\":\"Insufficient permission level\",\"required\":%d,\"current\":%d}", iAccessLevel, iUserLevel);
			xsHttpReplyAuto(objResp, 403, HTTP_CT_JSON, sJson, 0);
			xrtFree(sJson);
			return;
		}
	}
	
	// 返回文件
	str sFullPath = xrtPathJoin(2, AttachmentPath, sPath);
	
	// 检查文件是否存在
	if ( !xrtFileExists(sFullPath) ) {
		xrtFree(sFullPath);
		xrtFree(sFilename);
		xrtFree(sExt);
		xrtFree(sMime);
		xrtFree(sPath);
		xsHttpReplyAuto(objResp, 404, HTTP_CT_JSON, "{\"error\":\"File not found\"}", 0);
		return;
	}
	
	// 确定 Content-Disposition
	MimeMapping* pMime = Attachment_GetMime(sExt);
	int bIsInline = pMime ? pMime->isInline : 0;
	
	// 如果是下载类型，增加下载计数
	if ( !bIsInline ) {
		Attachment_UpdateDownloadCount(sXID);
	}
	
	// 构建响应头
	char sHeader[512];
	if ( bIsInline ) {
		sprintf(sHeader, "Content-Type: %s\r\n", sMime);
	} else {
		sprintf(sHeader, "Content-Type: %s\r\nContent-Disposition: attachment; filename=\"%s\"\r\n", sMime, sFilename);
	}
	
	// 使用 mongoose 的文件服务
	size_t iDownloadSize = 0;
	str sFileData = xrtFileGetAll(sFullPath, &iDownloadSize);
	if ( sFileData == NULL ) {
		xrtFree(sFullPath);
		xrtFree(sFilename);
		xrtFree(sExt);
		xrtFree(sMime);
		xrtFree(sPath);
		xsHttpReplyAuto(objResp, 500, HTTP_CT_JSON, "{\"error\":\"Failed to read file\"}", 0);
		return;
	}
	
	xsHttpReplyAuto(objResp, 200, sHeader, sFileData, iDownloadSize);
	xrtFree(sFileData);
	
	xrtFree(sFullPath);
	xrtFree(sFilename);
	xrtFree(sExt);
	xrtFree(sMime);
	xrtFree(sPath);
}



// ==================== 页面路由 ====================

void Request_View_Attachment_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	LoadPage(objResp, 200, HTTP_CT_HTML, "attachment/list.html");
}

void Request_View_Attachment_Stats(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	LoadPage(objResp, 200, HTTP_CT_HTML, "attachment/stats.html");
}

void Request_View_Attachment_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	LoadPage(objResp, 200, HTTP_CT_HTML, "attachment/edit.html");
}

void Request_View_Attachment_Upload(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	LoadPage(objResp, 200, HTTP_CT_HTML, "attachment/upload.html");
}



