/* 附件后台管理路由：上传/列表/详情/保存/删除/统计 + 访问入口 + 视图页。 */

/* 上传公共流：multipart 解析 → 约束检查 → 落盘 → 建档。
 * member 侧多收 allowHotlink/accessType/accessLevel/price/priceType 表单字段。 */
static void Attachment_HandleUpload(XS_RequestObject objReq, XS_ResponseObject objResp,
	xvalue* objSession, int uploaderType)
{
	const char* contentType = XAdmin_PluginReqHeader(objReq, "Content-Type");
	char boundary[74];
	MultipartPart part;
	size_t offset = 0;
	char* filename = NULL;
	char* ext = NULL;
	char* modelName = NULL;
	int64 recordId = 0;
	const char* fileData = NULL;
	size_t fileSize = 0;
	int allowHotlink = 1, accessType = 0, accessLevel = 0;
	int64 price = 0;
	int priceType = 0;
	int64 uploaderId = ValueInt(objSession, "id");
	char* xid;
	char* path;
	char* fullPath;
	xvalue* cfg;
	const AttachmentMime* mime;

	if (!MultipartBoundary(contentType, boundary, sizeof(boundary))) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid content type\"}", 0);
		return;
	}
	while (MultipartNext(XAdmin_ReqBody(objReq), XAdmin_ReqBodyLen(objReq),

		boundary, strlen(boundary), &offset, &part)) {
		if (MultipartNameIs(&part, "file")) {
			fileData = part.data;
			fileSize = part.size;
			if (part.filename && part.filenameLen > 0) {
				filename = xrtStrDupN(part.filename, part.filenameLen);
				ext = Util_ExtNoDot(filename);
				if (ext) {
					char* p;
					for (p = ext; *p; p++)
						if (*p >= 'A' && *p <= 'Z') *p += 32;
				}
			}
		} else if (MultipartNameIs(&part, "modelName")) {
			modelName = xrtStrDupN(part.data, part.size);
		} else if (MultipartNameIs(&part, "recordId")) {
			char tmp[24] = {0};
			size_t n = part.size < 23 ? part.size : 23;
			memcpy(tmp, part.data, n);
			recordId = Util_ParseI64(tmp);
		} else if (uploaderType == 2 && MultipartNameIs(&part, "allowHotlink")) {
			allowHotlink = (part.size > 0 && part.data[0] == '1') ? 1 : 0;
		} else if (uploaderType == 2 && MultipartNameIs(&part, "accessType")) {
			char tmp[8] = {0};
			size_t n = part.size < 7 ? part.size : 7;
			memcpy(tmp, part.data, n);
			accessType = atoi(tmp);
		} else if (uploaderType == 2 && MultipartNameIs(&part, "accessLevel")) {
			char tmp[8] = {0};
			size_t n = part.size < 7 ? part.size : 7;
			memcpy(tmp, part.data, n);
			accessLevel = atoi(tmp);
		} else if (uploaderType == 2 && MultipartNameIs(&part, "price")) {
			char tmp[24] = {0};
			size_t n = part.size < 23 ? part.size : 23;
			memcpy(tmp, part.data, n);
			price = Util_ParseI64(tmp);
		} else if (uploaderType == 2 && MultipartNameIs(&part, "priceType")) {
			char tmp[8] = {0};
			size_t n = part.size < 7 ? part.size : 7;
			memcpy(tmp, part.data, n);
			priceType = atoi(tmp);
		}
	}

	if (!fileData || fileSize == 0 || !filename || !ext) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"No file uploaded\"}", 0);
		goto done;
	}
	if (!Attachment_IsExtAllowed(ext)) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"File type not allowed\"}", 0);
		goto done;
	}
	{
		int64 maxSize = Attachment_GetMaxSize();
		if (maxSize > 0 && (int64)fileSize > maxSize) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"File too large\"}", 0);
			goto done;
		}
	}
	{
		int64 quota = Attachment_GetUserQuota();
		if (quota > 0 && Attachment_GetUserUsage(uploaderId, uploaderType) + (int64)fileSize > quota) {
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Storage quota exceeded\"}", 0);
			goto done;
		}
	}

	xid = Util_Token();
	path = Attachment_GeneratePath(modelName, xid, ext);
	Attachment_EnsureDir(path);
	fullPath = xrtPathJoin(AttachmentPath, path);
	if (!fullPath || !xrtFileWriteAtomic(fullPath, (xbytesview){(cbytes)fileData, fileSize})) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Failed to save file\"}", 0);
		xrtFree(fullPath); xrtFree(path); xrtFree(xid);
		goto done;
	}
	xrtFree(fullPath);

	mime = Attachment_GetMime(ext);
	if (uploaderType == 1) {
		cfg = ValueGet(G_Option, "attachment");
		allowHotlink = cfg ? (ValueBool(cfg, "defaultHotlink") ? 1 : 0) : 1;
		accessType = cfg ? (int)ValueInt(cfg, "defaultAccessType") : 0;
	}
	if (Attachment_Add(xid, filename, ext, mime ? mime->mime : "application/octet-stream",
		fileSize, path, modelName, recordId, uploaderId, uploaderType,
		allowHotlink, accessType, accessLevel, price, priceType)) {
		str json = xrtFormat("{\"result\":true,\"data\":{\"xid\":\"%s\",\"filename\":\"%s\",\"ext\":\"%s\",\"size\":%lld,\"url\":\"/attachment?xid=%s\"}}",
			xid, filename, ext, (long long)fileSize, xid);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, json, 0);
		xrtFree(json);
	} else {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Failed to save record\"}", 0);
	}
	xrtFree(path);
	xrtFree(xid);

done:
	xrtFree(filename);
	xrtFree(ext);
	if (modelName) xrtFree(modelName);
}

// 后台附件上传
void Request_Attachment_Upload(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method Not Allowed\"}", 0);
		return;
	}
	Attachment_HandleUpload(objReq, objResp, objSession, 1);
}

// 后台附件列表
void Request_Attachment_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char page[12] = {0}, limit[12] = {0};
	char modelName[64] = {0}, ext[16] = {0}, accessType[8] = {0};
	int iPage, iLimit, iOffset;
	xbuffer* sql;
	sqlite3_stmt* stmt = NULL;
	xvalue* data;
	xvalue* tbl;

	(void)objServer; (void)objHost; (void)objSession;
	xsReqQueryValue(objReq, "page", page, sizeof(page));
	xsReqQueryValue(objReq, "limit", limit, sizeof(limit));
	iPage = atoi(page); iLimit = atoi(limit);
	if (iPage < 1) iPage = 1;
	if (iLimit < 1) iLimit = 20;
	if (iLimit > 100) iLimit = 100;
	iOffset = (iPage - 1) * iLimit;
	xsReqQueryValue(objReq, "modelName", modelName, sizeof(modelName));
	xsReqQueryValue(objReq, "ext", ext, sizeof(ext));
	xsReqQueryValue(objReq, "accessType", accessType, sizeof(accessType));

	sql = xrtBufferCreate();
	xrtBufferAppend(sql, (xbytesview){(cbytes)"SELECT xid, filename, ext, mime, size, modelName, recordId, uploaderId, uploaderType, allowHotlink, accessType, accessLevel, price, priceType, salesCount, downloadCount, createTime FROM attachment WHERE isDelete = 0", strlen("SELECT xid, filename, ext, mime, size, modelName, recordId, uploaderId, uploaderType, allowHotlink, accessType, accessLevel, price, priceType, salesCount, downloadCount, createTime FROM attachment WHERE isDelete = 0")});
	if (modelName[0]) {
		xrtBufferAppend(sql, (xbytesview){(cbytes)" AND modelName = '", strlen(" AND modelName = '")});
		xrtBufferAppend(sql, (xbytesview){(cbytes)modelName, strlen(modelName)});
		xrtBufferAppend(sql, (xbytesview){(cbytes)"'", 1});
	}
	if (ext[0]) {
		xrtBufferAppend(sql, (xbytesview){(cbytes)" AND ext = '", strlen(" AND ext = '")});
		xrtBufferAppend(sql, (xbytesview){(cbytes)ext, strlen(ext)});
		xrtBufferAppend(sql, (xbytesview){(cbytes)"'", 1});
	}
	if (accessType[0]) {
		xrtBufferAppend(sql, (xbytesview){(cbytes)" AND accessType = ", strlen(" AND accessType = ")});
		xrtBufferAppend(sql, (xbytesview){(cbytes)accessType, strlen(accessType)});
	}
	xrtBufferAppend(sql, (xbytesview){(cbytes)" ORDER BY createTime DESC LIMIT ? OFFSET ?", strlen(" ORDER BY createTime DESC LIMIT ? OFFSET ?")});

	data = xrtValueArray();
	if (sqlite3_prepare_v3(G_DB, (const char*)xrtBufferView(sql).Data, xrtBufferView(sql).Size,
		0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int(stmt, 1, iLimit);
		sqlite3_bind_int(stmt, 2, iOffset);
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			xvalue* row = xrtValueObject();
			ValueSetText(row, "xid", (const char*)sqlite3_column_text(stmt, 0));
			ValueSetText(row, "filename", (const char*)sqlite3_column_text(stmt, 1));
			ValueSetText(row, "ext", (const char*)sqlite3_column_text(stmt, 2));
			ValueSetText(row, "mime", (const char*)sqlite3_column_text(stmt, 3));
			ValueSetInt(row, "size", sqlite3_column_int64(stmt, 4));
			ValueSetText(row, "modelName", (const char*)sqlite3_column_text(stmt, 5));
			ValueSetInt(row, "recordId", sqlite3_column_int64(stmt, 6));
			ValueSetInt(row, "uploaderId", sqlite3_column_int64(stmt, 7));
			ValueSetInt(row, "uploaderType", sqlite3_column_int(stmt, 8));
			ValueSetBool(row, "allowHotlink", sqlite3_column_int(stmt, 9));
			ValueSetInt(row, "accessType", sqlite3_column_int(stmt, 10));
			ValueSetInt(row, "accessLevel", sqlite3_column_int(stmt, 11));
			ValueSetInt(row, "price", sqlite3_column_int64(stmt, 12));
			ValueSetInt(row, "priceType", sqlite3_column_int(stmt, 13));
			ValueSetInt(row, "salesCount", sqlite3_column_int64(stmt, 14));
			ValueSetInt(row, "downloadCount", sqlite3_column_int64(stmt, 15));
			ValueSetOwnedText(row, "createTime", TimeText(sqlite3_column_int64(stmt, 16), TIME_TEXT_DATETIME));
			xrtValueArrayAppendNew(data, row);
		}
		sqlite3_finalize(stmt);
	}
	xrtBufferDestroy(sql);

	/* 总数 */
	{
		sqlite3_stmt* stmtCount = NULL;
		int64 count = 0;
		xbuffer* sql2 = xrtBufferCreate();
		xrtBufferAppend(sql2, (xbytesview){(cbytes)"SELECT COUNT(*) FROM attachment WHERE isDelete = 0", strlen("SELECT COUNT(*) FROM attachment WHERE isDelete = 0")});
		if (modelName[0]) {
			xrtBufferAppend(sql2, (xbytesview){(cbytes)" AND modelName = '", strlen(" AND modelName = '")});
			xrtBufferAppend(sql2, (xbytesview){(cbytes)modelName, strlen(modelName)});
			xrtBufferAppend(sql2, (xbytesview){(cbytes)"'", 1});
		}
		if (ext[0]) {
			xrtBufferAppend(sql2, (xbytesview){(cbytes)" AND ext = '", strlen(" AND ext = '")});
			xrtBufferAppend(sql2, (xbytesview){(cbytes)ext, strlen(ext)});
			xrtBufferAppend(sql2, (xbytesview){(cbytes)"'", 1});
		}
		if (accessType[0]) {
			xrtBufferAppend(sql2, (xbytesview){(cbytes)" AND accessType = ", strlen(" AND accessType = ")});
			xrtBufferAppend(sql2, (xbytesview){(cbytes)accessType, strlen(accessType)});
		}
		if (sqlite3_prepare_v3(G_DB, (const char*)xrtBufferView(sql2).Data, xrtBufferView(sql2).Size,
			0, &stmtCount, NULL) == SQLITE_OK) {
			if (sqlite3_step(stmtCount) == SQLITE_ROW) count = sqlite3_column_int64(stmtCount, 0);
			sqlite3_finalize(stmtCount);
		}
		xrtBufferDestroy(sql2);

		NotifyReplyAdminTable(objResp, data, count);
	}
}

// 后台附件详情
void Request_Attachment_Get(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char xid[48] = {0};
	xvalue* tblData;

	(void)objServer; (void)objHost; (void)objSession;
	xsReqQueryValue(objReq, "xid", xid, sizeof(xid));
	if (!xid[0]) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing xid\"}", 0);
		return;
	}
	sqlite3_bind_text(stmt_attachment_get, 1, xid, -1, NULL);
	if (sqlite3_step(stmt_attachment_get) != SQLITE_ROW) {
		sqlite3_reset(stmt_attachment_get);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Not found\"}", 0);
		return;
	}
	tblData = xrtValueObject();
	ValueSetText(tblData, "xid", (const char*)sqlite3_column_text(stmt_attachment_get, 0));
	ValueSetText(tblData, "filename", (const char*)sqlite3_column_text(stmt_attachment_get, 1));
	ValueSetText(tblData, "ext", (const char*)sqlite3_column_text(stmt_attachment_get, 2));
	ValueSetText(tblData, "mime", (const char*)sqlite3_column_text(stmt_attachment_get, 3));
	ValueSetInt(tblData, "size", sqlite3_column_int64(stmt_attachment_get, 4));
	ValueSetText(tblData, "path", (const char*)sqlite3_column_text(stmt_attachment_get, 5));
	ValueSetText(tblData, "modelName", (const char*)sqlite3_column_text(stmt_attachment_get, 6));
	ValueSetInt(tblData, "recordId", sqlite3_column_int64(stmt_attachment_get, 7));
	ValueSetInt(tblData, "uploaderId", sqlite3_column_int64(stmt_attachment_get, 8));
	ValueSetInt(tblData, "uploaderType", sqlite3_column_int(stmt_attachment_get, 9));
	ValueSetBool(tblData, "allowHotlink", sqlite3_column_int(stmt_attachment_get, 10));
	ValueSetInt(tblData, "accessType", sqlite3_column_int(stmt_attachment_get, 11));
	ValueSetInt(tblData, "accessLevel", sqlite3_column_int(stmt_attachment_get, 12));
	ValueSetInt(tblData, "price", sqlite3_column_int64(stmt_attachment_get, 13));
	ValueSetInt(tblData, "priceType", sqlite3_column_int(stmt_attachment_get, 14));
	ValueSetInt(tblData, "salesCount", sqlite3_column_int64(stmt_attachment_get, 15));
	ValueSetInt(tblData, "downloadCount", sqlite3_column_int64(stmt_attachment_get, 16));
	ValueSetText(tblData, "remark", (const char*)sqlite3_column_text(stmt_attachment_get, 17));
	ValueSetOwnedText(tblData, "createTime", TimeText(sqlite3_column_int64(stmt_attachment_get, 18), TIME_TEXT_DATETIME));
	sqlite3_reset(stmt_attachment_get);

	NotifyReplyAdminTable(objResp, tblData, 0);
}

// 后台附件保存
void Request_Attachment_Save(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* form;
	str xid;
	sqlite3_stmt* stmt = NULL;
	int rc;

	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method Not Allowed\"}", 0);
		return;
	}
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
	sqlite3_prepare_v3(G_DB,
		"UPDATE attachment SET allowHotlink=?, accessType=?, accessLevel=?, price=?, priceType=?, remark=? WHERE xid=?",
		-1, 0, &stmt, NULL);
	sqlite3_bind_int(stmt, 1, ValueBool(form, "allowHotlink") ? 1 : 0);
	sqlite3_bind_int(stmt, 2, (int)ValueInt(form, "accessType"));
	sqlite3_bind_int(stmt, 3, (int)ValueInt(form, "accessLevel"));
	sqlite3_bind_int64(stmt, 4, ValueInt(form, "price"));
	sqlite3_bind_int(stmt, 5, (int)ValueInt(form, "priceType"));
	{
		str remark = ValueText(form, "remark");
		sqlite3_bind_text(stmt, 6, remark ? remark : "", -1, NULL);
	}
	sqlite3_bind_text(stmt, 7, xid, -1, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	xrtValueRelease(form);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON,
		rc == SQLITE_DONE ? "{\"result\":true,\"message\":\"保存成功\"}" : "{\"result\":false,\"message\":\"保存失败\"}", 0);
}

// 后台附件删除
void Request_Attachment_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char xid[48] = {0};
	char* path = NULL;
	sqlite3_stmt* stmt = NULL;
	int rc;

	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_DELETE && xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method Not Allowed\"}", 0);
		return;
	}
	xsReqQueryValue(objReq, "xid", xid, sizeof(xid));
	if (!xid[0]) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing xid\"}", 0);
		return;
	}
	sqlite3_bind_text(stmt_attachment_get, 1, xid, -1, NULL);
	if (sqlite3_step(stmt_attachment_get) == SQLITE_ROW) {
		const char* tmp = (const char*)sqlite3_column_text(stmt_attachment_get, 5);
		if (tmp) path = xrtStrDup(tmp);
	}
	sqlite3_reset(stmt_attachment_get);
	if (path) {
		char* fullPath = xrtPathJoin(AttachmentPath, path);
		xrtFileDelete(fullPath);
		xrtFree(fullPath);
		xrtFree(path);
	}
	sqlite3_prepare_v3(G_DB, "UPDATE attachment SET isDelete = 1 WHERE xid = ?", -1, 0, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, xid, -1, NULL);
	rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON,
		rc == SQLITE_DONE ? "{\"result\":true,\"message\":\"删除成功\"}" : "{\"result\":false,\"message\":\"删除失败\"}", 0);
}

// 存储统计
void Request_Attachment_Stats(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* tbl = xrtValueObject();
	xvalue* tblData = xrtValueObject();
	xvalue* tblTotal = xrtValueObject();
	xvalue* arrByModel = xrtValueArray();
	xvalue* arrByExt = xrtValueArray();
	size_t size = 0;
	char* json;

	(void)objServer; (void)objHost; (void)objSession; (void)objReq;
	ValueSetBool(tbl, "result", true);
	if (sqlite3_step(stmt_attachment_stats_total) == SQLITE_ROW) {
		int64 total = sqlite3_column_int64(stmt_attachment_stats_total, 0);
		int64 bytes = sqlite3_column_int64(stmt_attachment_stats_total, 1);
		char sizeText[32];
		ValueSetInt(tblTotal, "count", total);
		ValueSetInt(tblTotal, "size", bytes);
		if (bytes >= 1073741824) sprintf(sizeText, "%.2f GB", (double)bytes / 1073741824);
		else if (bytes >= 1048576) sprintf(sizeText, "%.2f MB", (double)bytes / 1048576);
		else if (bytes >= 1024) sprintf(sizeText, "%.2f KB", (double)bytes / 1024);
		else sprintf(sizeText, "%lld B", (long long)bytes);
		ValueSetText(tblTotal, "sizeText", sizeText);
	}
	sqlite3_reset(stmt_attachment_stats_total);
	ValueSetOwn(tblData, "total", tblTotal);

	while (sqlite3_step(stmt_attachment_stats_by_model) == SQLITE_ROW) {
		xvalue* item = xrtValueObject();
		const char* model = (const char*)sqlite3_column_text(stmt_attachment_stats_by_model, 0);
		ValueSetText(item, "modelName", model ? model : "");
		ValueSetText(item, "title", (model && model[0]) ? model : "全局附件");
		ValueSetInt(item, "count", sqlite3_column_int64(stmt_attachment_stats_by_model, 1));
		ValueSetInt(item, "size", sqlite3_column_int64(stmt_attachment_stats_by_model, 2));
		xrtValueArrayAppendNew(arrByModel, item);
	}
	sqlite3_reset(stmt_attachment_stats_by_model);
	ValueSetOwn(tblData, "byModel", arrByModel);

	while (sqlite3_step(stmt_attachment_stats_by_ext) == SQLITE_ROW) {
		xvalue* item = xrtValueObject();
		ValueSetText(item, "ext", (const char*)sqlite3_column_text(stmt_attachment_stats_by_ext, 0));
		ValueSetInt(item, "count", sqlite3_column_int64(stmt_attachment_stats_by_ext, 1));
		ValueSetInt(item, "size", sqlite3_column_int64(stmt_attachment_stats_by_ext, 2));
		xrtValueArrayAppendNew(arrByExt, item);
	}
	sqlite3_reset(stmt_attachment_stats_by_ext);
	ValueSetOwn(tblData, "byExt", arrByExt);

	ValueSetOwn(tbl, "data", tblData);
	json = xrtJsonStringify(tbl, false, &size);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, json, 0);
	xrtFree(json);
	xrtValueRelease(tbl);
}

// 附件访问入口 /attachment?xid=...
void Request_Attachment_Access(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char xid[64] = {0};
	char* filename = NULL;
	char* ext = NULL;
	char* mime = NULL;
	char* path = NULL;
	int64 uploaderId;
	int uploaderType, allowHotlink, accessType, accessLevel, priceType;
	int64 price;
	char* fullPath;
	bytes fileData;
	size_t fileSize = 0;
	const AttachmentMime* mimeInfo;
	char header[512];

	(void)objServer; (void)objHost;
	if (xsReqQueryValue(objReq, "xid", xid, sizeof(xid)) <= 0) {
		xsHttpReplyAuto(objResp, 400, HTTP_CT_JSON, "{\"error\":\"Missing xid parameter\"}", 0);
		return;
	}
	sqlite3_bind_text(stmt_attachment_get, 1, xid, -1, NULL);
	if (sqlite3_step(stmt_attachment_get) != SQLITE_ROW) {
		sqlite3_reset(stmt_attachment_get);
		xsHttpReplyAuto(objResp, 404, HTTP_CT_JSON, "{\"error\":\"Not found\"}", 0);
		return;
	}
	filename = xrtStrDup((const char*)sqlite3_column_text(stmt_attachment_get, 1));
	ext = xrtStrDup((const char*)sqlite3_column_text(stmt_attachment_get, 2));
	mime = xrtStrDup((const char*)sqlite3_column_text(stmt_attachment_get, 3));
	path = xrtStrDup((const char*)sqlite3_column_text(stmt_attachment_get, 5));
	uploaderId = sqlite3_column_int64(stmt_attachment_get, 8);
	uploaderType = sqlite3_column_int(stmt_attachment_get, 9);
	allowHotlink = sqlite3_column_int(stmt_attachment_get, 10);
	accessType = sqlite3_column_int(stmt_attachment_get, 11);
	accessLevel = sqlite3_column_int(stmt_attachment_get, 12);
	price = sqlite3_column_int64(stmt_attachment_get, 13);
	priceType = sqlite3_column_int(stmt_attachment_get, 14);
	sqlite3_reset(stmt_attachment_get);

	if (!Attachment_CheckHotlink(objReq, allowHotlink != 0)) {
		xsHttpReplyAuto(objResp, 403, HTTP_CT_JSON, "{\"error\":\"Hotlink not allowed\"}", 0);
		goto done;
	}
	if ((accessType == 1 || accessType == 2 || accessType == 3)
		&& (!objSession || xrtValueType(objSession) != XVALUE_OBJECT)) {
		xsHttpReplyAuto(objResp, 401, HTTP_CT_JSON, "{\"error\":\"Login required\"}", 0);
		goto done;
	}
	if (accessType == 2) {
		int64 memberId = ValueInt(objSession, "id");
		if (!(uploaderType == 2 && uploaderId == memberId)
			&& !Attachment_CheckPurchased(xid, memberId)) {
			str json = xrtFormat("{\"error\":\"Payment required\",\"price\":%lld,\"priceType\":%d}",
				(long long)price, priceType);
			xsHttpReplyAuto(objResp, 402, HTTP_CT_JSON, json, 0);
			xrtFree(json);
			goto done;
		}
	} else if (accessType == 3) {
		int userLevel = (int)ValueInt(objSession, "authLevel");
		if (userLevel < accessLevel) {
			str json = xrtFormat("{\"error\":\"Insufficient permission level\",\"required\":%d,\"current\":%d}",
				accessLevel, userLevel);
			xsHttpReplyAuto(objResp, 403, HTTP_CT_JSON, json, 0);
			xrtFree(json);
			goto done;
		}
	}

	fullPath = xrtPathJoin(AttachmentPath, path);
	if (!fullPath || !xrtFileExists(fullPath)) {
		xrtFree(fullPath);
		xsHttpReplyAuto(objResp, 404, HTTP_CT_JSON, "{\"error\":\"File not found\"}", 0);
		goto done;
	}
	fileData = xrtFileReadAll(fullPath, &fileSize);
	xrtFree(fullPath);
	if (!fileData) {
		xsHttpReplyAuto(objResp, 500, HTTP_CT_JSON, "{\"error\":\"Failed to read file\"}", 0);
		goto done;
	}
	mimeInfo = Attachment_GetMime(ext);
	if (!mimeInfo || !mimeInfo->isInline) {
		Attachment_UpdateDownloadCount(xid);
		snprintf(header, sizeof(header), "Content-Type: %s\r\nContent-Disposition: attachment; filename=\"%s\"\r\n",
			mime ? mime : "application/octet-stream", filename ? filename : "file");
	} else {
		snprintf(header, sizeof(header), "Content-Type: %s\r\n", mime ? mime : "application/octet-stream");
	}
	xsHttpReplyAuto(objResp, 200, header, fileData, fileSize);
	xrtFree(fileData);

done:
	xrtFree(filename);
	xrtFree(ext);
	xrtFree(mime);
	xrtFree(path);
}

// ---- 视图页 ----

void Request_View_Attachment_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "attachment/list.html");
}

void Request_View_Attachment_Stats(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "attachment/stats.html");
}

void Request_View_Attachment_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "attachment/edit.html");
}

void Request_View_Attachment_Upload(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "attachment/upload.html");
}
