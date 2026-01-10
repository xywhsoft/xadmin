


// ==================== 前台附件上传 ====================

void Request_Api_Attachment_Upload(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 405, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method Not Allowed\"}", 0);
		return;
	}
	
	// 检查登录状态
	if ( !hm->session || hm->session->Type != XVO_DT_TABLE ) {
		http_reply(c, 401, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Please login first\"}", 0);
		return;
	}
	
	int64 iUploaderId = xvoTableGetInt(hm->session, "id", 2);
	int iUploaderType = 2;  // 前台会员
	
	// 解析 multipart
	struct mg_http_part part;
	size_t ofs = 0;
	str sFilename = NULL;
	str sExt = NULL;
	str sModelName = NULL;
	int64 iRecordId = 0;
	ptr pFileData = NULL;
	size_t iFileSize = 0;
	int iAllowHotlink = 1;
	int iAccessType = 0;
	int64 iPrice = 0;
	int iPriceType = 0;
	
	while ( (ofs = mg_http_next_multipart(hm->body, ofs, &part)) > 0 ) {
		if ( mg_match(part.name, mg_str("file"), NULL) ) {
			pFileData = (ptr)part.body.buf;
			iFileSize = part.body.len;
			if ( part.filename.len > 0 ) {
				sFilename = xrtCopyStr(part.filename.buf, part.filename.len);
				sExt = xrtPathGetExt(sFilename, 0);
				if ( sExt ) {
					for ( str p = sExt; *p; p++ ) {
						if ( *p >= 'A' && *p <= 'Z' ) *p += 32;
					}
				}
			}
		} else if ( mg_match(part.name, mg_str("modelName"), NULL) ) {
			sModelName = xrtCopyStr(part.body.buf, part.body.len);
		} else if ( mg_match(part.name, mg_str("recordId"), NULL) ) {
			char sTmp[24] = {0};
			size_t iLen = part.body.len < 23 ? part.body.len : 23;
			memcpy(sTmp, part.body.buf, iLen);
			iRecordId = xrtStrToI64(sTmp);
		} else if ( mg_match(part.name, mg_str("allowHotlink"), NULL) ) {
			iAllowHotlink = (part.body.len > 0 && part.body.buf[0] == '1') ? 1 : 0;
		} else if ( mg_match(part.name, mg_str("accessType"), NULL) ) {
			char sTmp[8] = {0};
			size_t iLen = part.body.len < 7 ? part.body.len : 7;
			memcpy(sTmp, part.body.buf, iLen);
			iAccessType = atoi(sTmp);
		} else if ( mg_match(part.name, mg_str("price"), NULL) ) {
			char sTmp[24] = {0};
			size_t iLen = part.body.len < 23 ? part.body.len : 23;
			memcpy(sTmp, part.body.buf, iLen);
			iPrice = xrtStrToI64(sTmp);
		} else if ( mg_match(part.name, mg_str("priceType"), NULL) ) {
			char sTmp[8] = {0};
			size_t iLen = part.body.len < 7 ? part.body.len : 7;
			memcpy(sTmp, part.body.buf, iLen);
			iPriceType = atoi(sTmp);
		}
	}
	
	// 验证文件
	if ( !pFileData || iFileSize == 0 || !sFilename || !sExt ) {
		if ( sFilename ) xrtFree(sFilename);
		if ( sExt ) xrtFree(sExt);
		if ( sModelName ) xrtFree(sModelName);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"No file uploaded\"}", 0);
		return;
	}
	
	// 检查扩展名
	if ( !Attachment_IsExtAllowed(sExt) ) {
		xrtFree(sFilename);
		xrtFree(sExt);
		if ( sModelName ) xrtFree(sModelName);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"File type not allowed\"}", 0);
		return;
	}
	
	// 检查文件大小
	int64 iMaxSize = Attachment_GetMaxSize();
	if ( iMaxSize > 0 && (int64)iFileSize > iMaxSize ) {
		xrtFree(sFilename);
		xrtFree(sExt);
		if ( sModelName ) xrtFree(sModelName);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"File too large\"}", 0);
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
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Storage quota exceeded\"}", 0);
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
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Failed to save file\"}", 0);
		return;
	}
	fwrite(pFileData, 1, iFileSize, fp);
	fclose(fp);
	xrtFree(sFullPath);
	
	// 获取 MIME 类型
	MimeMapping* pMime = Attachment_GetMime(sExt);
	str sMime = pMime ? pMime->mime : (str)"application/octet-stream";
	
	// 添加数据库记录
	bool bOK = Attachment_Add(sXID, sFilename, sExt, sMime, iFileSize, sPath,
							  sModelName, iRecordId, iUploaderId, iUploaderType,
							  iAllowHotlink, iAccessType, iPrice, iPriceType);
	
	if ( bOK ) {
		str sJson = xrtFormat("{\"result\":true,\"data\":{\"xid\":\"%s\",\"filename\":\"%s\",\"ext\":\"%s\",\"size\":%lld,\"url\":\"/attachment?xid=%s\"}}",
							  sXID, sFilename, sExt, (int64)iFileSize, sXID);
		http_reply(c, 200, HTTP_CT_JSON, sJson, 0);
		xrtFree(sJson);
	} else {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Failed to save record\"}", 0);
	}
	
	xrtFree(sPath);
	xrtFree(sXID);
	xrtFree(sFilename);
	xrtFree(sExt);
	if ( sModelName ) xrtFree(sModelName);
}



// ==================== 附件购买 ====================

void Request_Api_Attachment_Purchase(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 405, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method Not Allowed\"}", 0);
		return;
	}
	
	// 检查登录状态
	if ( !hm->session || hm->session->Type != XVO_DT_TABLE ) {
		http_reply(c, 401, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Please login first\"}", 0);
		return;
	}
	
	int64 iMemberId = xvoTableGetInt(hm->session, "id", 2);
	
	// 解析请求
	xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		if ( tblForm ) xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		return;
	}
	
	str sXID = xvoTableGetText(tblForm, "xid", 3);
	if ( !sXID || !*sXID ) {
		xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing xid\"}", 0);
		return;
	}
	
	// 查询附件信息
	sqlite3_bind_text(stmt_attachment_get, 1, sXID, -1, NULL);
	if ( sqlite3_step(stmt_attachment_get) != SQLITE_ROW ) {
		sqlite3_reset(stmt_attachment_get);
		xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Attachment not found\"}", 0);
		return;
	}
	
	int64 iUploaderId = sqlite3_column_int64(stmt_attachment_get, 8);
	int iUploaderType = sqlite3_column_int(stmt_attachment_get, 9);
	int iAccessType = sqlite3_column_int(stmt_attachment_get, 11);
	int64 iPrice = sqlite3_column_int64(stmt_attachment_get, 12);
	int iPriceType = sqlite3_column_int(stmt_attachment_get, 13);
	
	sqlite3_reset(stmt_attachment_get);
	
	// 检查是否为付费附件
	if ( iAccessType != 2 ) {
		xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Attachment is free\"}", 0);
		return;
	}
	
	// 检查是否为上传者
	if ( iUploaderType == 2 && iUploaderId == iMemberId ) {
		xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Cannot purchase own attachment\"}", 0);
		return;
	}
	
	// 检查是否已购买
	if ( Attachment_CheckPurchased(sXID, iMemberId) ) {
		xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"Already purchased\"}", 0);
		return;
	}
	
	// 检查余额（priceType=0 使用余额）
	if ( iPriceType == 0 ) {
		sqlite3_stmt* stmt;
		sqlite3_prepare_v3(G_DB->objDB, "SELECT balance FROM member WHERE id = ?", -1, 0, &stmt, NULL);
		sqlite3_bind_int64(stmt, 1, iMemberId);
		
		int64 iBalance = 0;
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iBalance = sqlite3_column_int64(stmt, 0);
		}
		sqlite3_finalize(stmt);
		
		if ( iBalance < iPrice ) {
			xvoUnref(tblForm);
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Insufficient balance\"}", 0);
			return;
		}
		
		// 计算卖家收益（平台抽成）
		xvalue tblAttachment = xvoTableGetValue(G_Option, "attachment", 10);
		int iPlatformFeeRate = tblAttachment ? (int)xvoTableGetInt(tblAttachment, "platformFeeRate", 15) : 10;
		int64 iSellerIncome = iPrice * (100 - iPlatformFeeRate) / 100;
		
		// 开始事务
		sqlite3_exec(G_DB->objDB, "BEGIN TRANSACTION", NULL, NULL, NULL);
		
		// 扣除买家余额
		sqlite3_prepare_v3(G_DB->objDB, "UPDATE member SET balance = balance - ? WHERE id = ?", -1, 0, &stmt, NULL);
		sqlite3_bind_int64(stmt, 1, iPrice);
		sqlite3_bind_int64(stmt, 2, iMemberId);
		int rc1 = sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		
		// 增加卖家余额（如果是会员上传的）
		int rc2 = SQLITE_DONE;
		if ( iUploaderType == 2 && iSellerIncome > 0 ) {
			sqlite3_prepare_v3(G_DB->objDB, "UPDATE member SET balance = balance + ? WHERE id = ?", -1, 0, &stmt, NULL);
			sqlite3_bind_int64(stmt, 1, iSellerIncome);
			sqlite3_bind_int64(stmt, 2, iUploaderId);
			rc2 = sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		
		// 添加购买记录
		bool bOrderOK = Attachment_AddOrder(sXID, iMemberId, iPrice, iPriceType, 
										   (iUploaderType == 2) ? iUploaderId : 0, iSellerIncome);
		
		// 更新销量
		Attachment_UpdateSales(sXID);
		
		if ( rc1 == SQLITE_DONE && rc2 == SQLITE_DONE && bOrderOK ) {
			sqlite3_exec(G_DB->objDB, "COMMIT", NULL, NULL, NULL);
			xvoUnref(tblForm);
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"Purchase successful\"}", 0);
		} else {
			sqlite3_exec(G_DB->objDB, "ROLLBACK", NULL, NULL, NULL);
			xvoUnref(tblForm);
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Transaction failed\"}", 0);
		}
	} else {
		// 其他货币类型暂不支持
		xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Currency type not supported\"}", 0);
	}
}



// ==================== 我的附件列表 ====================

void Request_Api_Attachment_MyList(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	// 检查登录状态
	if ( !hm->session || hm->session->Type != XVO_DT_TABLE ) {
		http_reply(c, 401, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Please login first\"}", 0);
		return;
	}
	
	int64 iMemberId = xvoTableGetInt(hm->session, "id", 2);
	
	// 获取分页参数
	char sPage[12], sLimit[12];
	mg_http_get_var(&hm->query, "page", sPage, sizeof(sPage));
	mg_http_get_var(&hm->query, "limit", sLimit, sizeof(sLimit));
	int iPage = atoi(sPage);
	int iLimit = atoi(sLimit);
	if ( iPage < 1 ) iPage = 1;
	if ( iLimit < 1 ) iLimit = 20;
	if ( iLimit > 100 ) iLimit = 100;
	int iOffset = (iPage - 1) * iLimit;
	
	// 查询
	sqlite3_stmt* stmt;
	sqlite3_prepare_v3(G_DB->objDB,
		"SELECT xid, filename, ext, size, accessType, price, priceType, salesCount, downloadCount, createTime "
		"FROM attachment WHERE uploaderId = ? AND uploaderType = 2 AND isDelete = 0 "
		"ORDER BY createTime DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt, NULL);
	sqlite3_bind_int64(stmt, 1, iMemberId);
	sqlite3_bind_int(stmt, 2, iLimit);
	sqlite3_bind_int(stmt, 3, iOffset);
	
	xvalue arrData = xvoCreateArray();
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		xvoTableSetText(tblRow, "xid", 3, (str)sqlite3_column_text(stmt, 0), 0, FALSE);
		xvoTableSetText(tblRow, "filename", 8, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
		xvoTableSetText(tblRow, "ext", 3, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
		xvoTableSetInt(tblRow, "size", 4, sqlite3_column_int64(stmt, 3));
		xvoTableSetInt(tblRow, "accessType", 10, sqlite3_column_int(stmt, 4));
		xvoTableSetInt(tblRow, "price", 5, sqlite3_column_int64(stmt, 5));
		xvoTableSetInt(tblRow, "priceType", 9, sqlite3_column_int(stmt, 6));
		xvoTableSetInt(tblRow, "salesCount", 10, sqlite3_column_int(stmt, 7));
		xvoTableSetInt(tblRow, "downloadCount", 13, sqlite3_column_int(stmt, 8));
		xtime iTime = sqlite3_column_int64(stmt, 9);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		
		str sXid = (str)sqlite3_column_text(stmt, 0);
		str sUrl = xrtFormat("/attachment?xid=%s", sXid);
		xvoTableSetText(tblRow, "url", 3, sUrl, 0, TRUE);
		
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_finalize(stmt);
	
	// 统计总数
	sqlite3_prepare_v3(G_DB->objDB,
		"SELECT COUNT(*) FROM attachment WHERE uploaderId = ? AND uploaderType = 2 AND isDelete = 0",
		-1, 0, &stmt, NULL);
	sqlite3_bind_int64(stmt, 1, iMemberId);
	int64 iCount = 0;
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt, 0);
	}
	sqlite3_finalize(stmt);
	
	// 构建响应
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}



// ==================== 已购买附件列表 ====================

void Request_Api_Attachment_Purchased(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	// 检查登录状态
	if ( !hm->session || hm->session->Type != XVO_DT_TABLE ) {
		http_reply(c, 401, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Please login first\"}", 0);
		return;
	}
	
	int64 iMemberId = xvoTableGetInt(hm->session, "id", 2);
	
	// 获取分页参数
	char sPage[12], sLimit[12];
	mg_http_get_var(&hm->query, "page", sPage, sizeof(sPage));
	mg_http_get_var(&hm->query, "limit", sLimit, sizeof(sLimit));
	int iPage = atoi(sPage);
	int iLimit = atoi(sLimit);
	if ( iPage < 1 ) iPage = 1;
	if ( iLimit < 1 ) iLimit = 20;
	if ( iLimit > 100 ) iLimit = 100;
	int iOffset = (iPage - 1) * iLimit;
	
	// 查询
	sqlite3_stmt* stmt;
	sqlite3_prepare_v3(G_DB->objDB,
		"SELECT o.attachmentXid, a.filename, a.ext, a.size, o.price, o.priceType, o.createTime "
		"FROM attachmentOrder o "
		"LEFT JOIN attachment a ON o.attachmentXid = a.xid "
		"WHERE o.memberId = ? ORDER BY o.createTime DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt, NULL);
	sqlite3_bind_int64(stmt, 1, iMemberId);
	sqlite3_bind_int(stmt, 2, iLimit);
	sqlite3_bind_int(stmt, 3, iOffset);
	
	xvalue arrData = xvoCreateArray();
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		str sXid = (str)sqlite3_column_text(stmt, 0);
		xvoTableSetText(tblRow, "xid", 3, sXid, 0, FALSE);
		xvoTableSetText(tblRow, "filename", 8, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
		xvoTableSetText(tblRow, "ext", 3, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
		xvoTableSetInt(tblRow, "size", 4, sqlite3_column_int64(stmt, 3));
		xvoTableSetInt(tblRow, "price", 5, sqlite3_column_int64(stmt, 4));
		xvoTableSetInt(tblRow, "priceType", 9, sqlite3_column_int(stmt, 5));
		xtime iTime = sqlite3_column_int64(stmt, 6);
		xvoTableSetText(tblRow, "purchaseTime", 12, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		
		str sUrl = xrtFormat("/attachment?xid=%s", sXid);
		xvoTableSetText(tblRow, "url", 3, sUrl, 0, TRUE);
		
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_finalize(stmt);
	
	// 统计总数
	sqlite3_prepare_v3(G_DB->objDB, "SELECT COUNT(*) FROM attachmentOrder WHERE memberId = ?", -1, 0, &stmt, NULL);
	sqlite3_bind_int64(stmt, 1, iMemberId);
	int64 iCount = 0;
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt, 0);
	}
	sqlite3_finalize(stmt);
	
	// 构建响应
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}


