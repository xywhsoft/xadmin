/* 附件前台开放 API：上传 / 购买 / 我的 / 已购。 */

// 前台附件上传
void Request_Api_Attachment_Upload(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method Not Allowed\"}", 0);
		return;
	}
	if (!objSession || xrtValueType(objSession) != XVALUE_OBJECT) {
		xsHttpReplyAuto(objResp, 401, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Please login first\"}", 0);
		return;
	}
	Attachment_HandleUpload(objReq, objResp, objSession, 2);
}

// 附件购买（余额支付 + 卖家分成）
void Request_Api_Attachment_Purchase(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* form;
	str xid;
	int64 uploaderId, memberId, price;
	int uploaderType, accessType, priceType;
	sqlite3_stmt* stmt = NULL;
	xvalue* cfg;
	int platformFeeRate;
	int64 sellerIncome, balance = 0;
	int rc1, rc2;
	bool orderOK;

	(void)objServer; (void)objHost;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method Not Allowed\"}", 0);
		return;
	}
	if (!objSession || xrtValueType(objSession) != XVALUE_OBJECT) {
		xsHttpReplyAuto(objResp, 401, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Please login first\"}", 0);
		return;
	}
	memberId = ValueInt(objSession, "id");
	form = JsonParseN(XAdmin_ReqBody(objReq), XAdmin_ReqBodyLen(objReq));
	if (!form || xrtValueType(form) != XVALUE_OBJECT) {
		if (form) xrtValueRelease(form);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		return;
	}
	xid = ValueText(form, "xid");
	if (!xid || !xid[0]) {
		xrtValueRelease(form);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing xid\"}", 0);
		return;
	}
	sqlite3_bind_text(stmt_attachment_get, 1, xid, -1, NULL);
	if (sqlite3_step(stmt_attachment_get) != SQLITE_ROW) {
		sqlite3_reset(stmt_attachment_get);
		xrtValueRelease(form);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Attachment not found\"}", 0);
		return;
	}
	uploaderId = sqlite3_column_int64(stmt_attachment_get, 8);
	uploaderType = sqlite3_column_int(stmt_attachment_get, 9);
	accessType = sqlite3_column_int(stmt_attachment_get, 11);
	price = sqlite3_column_int64(stmt_attachment_get, 13);
	priceType = sqlite3_column_int(stmt_attachment_get, 14);
	sqlite3_reset(stmt_attachment_get);

	if (accessType != 2) {
		xrtValueRelease(form);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Attachment is free\"}", 0);
		return;
	}
	if (uploaderType == 2 && uploaderId == memberId) {
		xrtValueRelease(form);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Cannot purchase own attachment\"}", 0);
		return;
	}
	if (Attachment_CheckPurchased(xid, memberId)) {
		xrtValueRelease(form);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"Already purchased\"}", 0);
		return;
	}
	if (priceType != 0) {
		xrtValueRelease(form);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Currency type not supported\"}", 0);
		return;
	}
	if (price <= 0) {
		xrtValueRelease(form);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"Already purchased\"}", 0);
		return;
	}
	if (sqlite3_prepare_v3(G_DB, "SELECT balance FROM member WHERE id = ?", -1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, memberId);
		if (sqlite3_step(stmt) == SQLITE_ROW) balance = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);
	}
	if (balance < price) {
		xrtValueRelease(form);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Insufficient balance\"}", 0);
		return;
	}
	cfg = ValueGet(G_Option, "attachment");
	platformFeeRate = cfg ? (int)ValueInt(cfg, "platformFeeRate") : 10;
	sellerIncome = price * (100 - platformFeeRate) / 100;

	sqlite3_exec(G_DB, "BEGIN TRANSACTION", NULL, NULL, NULL);
	rc1 = SQLITE_DONE;
	if (sqlite3_prepare_v3(G_DB, "UPDATE member SET balance = balance - ? WHERE id = ?", -1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, price);
		sqlite3_bind_int64(stmt, 2, memberId);
		rc1 = sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	rc2 = SQLITE_DONE;
	if (uploaderType == 2 && sellerIncome > 0
		&& sqlite3_prepare_v3(G_DB, "UPDATE member SET balance = balance + ? WHERE id = ?", -1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, sellerIncome);
		sqlite3_bind_int64(stmt, 2, uploaderId);
		rc2 = sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	orderOK = Attachment_AddOrder(xid, memberId, price, priceType,
		(uploaderType == 2) ? uploaderId : 0, sellerIncome);
	Attachment_UpdateSales(xid);
	xrtValueRelease(form);

	if (rc1 == SQLITE_DONE && rc2 == SQLITE_DONE && orderOK) {
		sqlite3_exec(G_DB, "COMMIT", NULL, NULL, NULL);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"Purchase successful\"}", 0);
	} else {
		sqlite3_exec(G_DB, "ROLLBACK", NULL, NULL, NULL);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Transaction failed\"}", 0);
	}
}

// 我的附件列表
void Request_Api_Attachment_MyList(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char page[12] = {0}, limit[12] = {0};
	int iPage, iLimit, iOffset;
	sqlite3_stmt* stmt = NULL, * stmtCount = NULL;
	xvalue* data;
	int64 count = 0;

	(void)objServer; (void)objHost;
	if (!objSession || xrtValueType(objSession) != XVALUE_OBJECT) {
		xsHttpReplyAuto(objResp, 401, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Please login first\"}", 0);
		return;
	}
	xsReqQueryValue(objReq, "page", page, sizeof(page));
	xsReqQueryValue(objReq, "limit", limit, sizeof(limit));
	iPage = atoi(page); iLimit = atoi(limit);
	if (iPage < 1) iPage = 1;
	if (iLimit < 1) iLimit = 20;
	if (iLimit > 100) iLimit = 100;
	iOffset = (iPage - 1) * iLimit;

	data = xrtValueArray();
	if (sqlite3_prepare_v3(G_DB,
		"SELECT xid, filename, ext, size, modelName, accessType, price, priceType, salesCount, downloadCount, createTime "
		"FROM attachment WHERE uploaderId = ? AND uploaderType = 2 AND isDelete = 0 ORDER BY createTime DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, ValueInt(objSession, "id"));
		sqlite3_bind_int(stmt, 2, iLimit);
		sqlite3_bind_int(stmt, 3, iOffset);
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			xvalue* row = xrtValueObject();
			ValueSetText(row, "xid", (const char*)sqlite3_column_text(stmt, 0));
			ValueSetText(row, "filename", (const char*)sqlite3_column_text(stmt, 1));
			ValueSetText(row, "ext", (const char*)sqlite3_column_text(stmt, 2));
			ValueSetInt(row, "size", sqlite3_column_int64(stmt, 3));
			ValueSetText(row, "modelName", (const char*)sqlite3_column_text(stmt, 4));
			ValueSetInt(row, "accessType", sqlite3_column_int(stmt, 5));
			ValueSetInt(row, "price", sqlite3_column_int64(stmt, 6));
			ValueSetInt(row, "priceType", sqlite3_column_int(stmt, 7));
			ValueSetInt(row, "salesCount", sqlite3_column_int64(stmt, 8));
			ValueSetInt(row, "downloadCount", sqlite3_column_int64(stmt, 9));
			{
				char url[128];
				snprintf(url, sizeof(url), "/attachment?xid=%s", (const char*)sqlite3_column_text(stmt, 0));
				ValueSetText(row, "url", url);
			}
			ValueSetOwnedText(row, "createTime", TimeText(sqlite3_column_int64(stmt, 10), TIME_TEXT_DATETIME));
			xrtValueArrayAppendNew(data, row);
		}
		sqlite3_finalize(stmt);
	}
	if (sqlite3_prepare_v3(G_DB,
		"SELECT COUNT(*) FROM attachment WHERE uploaderId = ? AND uploaderType = 2 AND isDelete = 0",
		-1, 0, &stmtCount, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmtCount, 1, ValueInt(objSession, "id"));
		if (sqlite3_step(stmtCount) == SQLITE_ROW) count = sqlite3_column_int64(stmtCount, 0);
		sqlite3_finalize(stmtCount);
	}
	NotifyReplyAdminTable(objResp, data, count);
}

// 已购附件列表
void Request_Api_Attachment_Purchased(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char page[12] = {0}, limit[12] = {0};
	int iPage, iLimit, iOffset;
	sqlite3_stmt* stmt = NULL, * stmtCount = NULL;
	xvalue* data;
	int64 count = 0;

	(void)objServer; (void)objHost;
	if (!objSession || xrtValueType(objSession) != XVALUE_OBJECT) {
		xsHttpReplyAuto(objResp, 401, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Please login first\"}", 0);
		return;
	}
	xsReqQueryValue(objReq, "page", page, sizeof(page));
	xsReqQueryValue(objReq, "limit", limit, sizeof(limit));
	iPage = atoi(page); iLimit = atoi(limit);
	if (iPage < 1) iPage = 1;
	if (iLimit < 1) iLimit = 20;
	if (iLimit > 100) iLimit = 100;
	iOffset = (iPage - 1) * iLimit;

	data = xrtValueArray();
	if (sqlite3_prepare_v3(G_DB,
		"SELECT a.xid, a.filename, a.ext, a.size, a.modelName, o.price, o.priceType, o.createTime "
		"FROM attachmentOrder o INNER JOIN attachment a ON o.attachmentXid = a.xid "
		"WHERE o.memberId = ? AND a.isDelete = 0 ORDER BY o.id DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, ValueInt(objSession, "id"));
		sqlite3_bind_int(stmt, 2, iLimit);
		sqlite3_bind_int(stmt, 3, iOffset);
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			xvalue* row = xrtValueObject();
			ValueSetText(row, "xid", (const char*)sqlite3_column_text(stmt, 0));
			ValueSetText(row, "filename", (const char*)sqlite3_column_text(stmt, 1));
			ValueSetText(row, "ext", (const char*)sqlite3_column_text(stmt, 2));
			ValueSetInt(row, "size", sqlite3_column_int64(stmt, 3));
			ValueSetText(row, "modelName", (const char*)sqlite3_column_text(stmt, 4));
			ValueSetInt(row, "price", sqlite3_column_int64(stmt, 5));
			ValueSetInt(row, "priceType", sqlite3_column_int(stmt, 6));
			{
				char url[128];
				snprintf(url, sizeof(url), "/attachment?xid=%s", (const char*)sqlite3_column_text(stmt, 0));
				ValueSetText(row, "url", url);
			}
			ValueSetOwnedText(row, "purchaseTime", TimeText(sqlite3_column_int64(stmt, 7), TIME_TEXT_DATETIME));
			xrtValueArrayAppendNew(data, row);
		}
		sqlite3_finalize(stmt);
	}
	if (sqlite3_prepare_v3(G_DB,
		"SELECT COUNT(*) FROM attachmentOrder o INNER JOIN attachment a ON o.attachmentXid = a.xid "
		"WHERE o.memberId = ? AND a.isDelete = 0",
		-1, 0, &stmtCount, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmtCount, 1, ValueInt(objSession, "id"));
		if (sqlite3_step(stmtCount) == SQLITE_ROW) count = sqlite3_column_int64(stmtCount, 0);
		sqlite3_finalize(stmtCount);
	}
	NotifyReplyAdminTable(objResp, data, count);
}
