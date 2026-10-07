/* 站内通知：notify_message / notify_recipient 的核心操作与自装（菜单/URI）。
 * 秒单位 v1 库列已由 DB_MigrateTimeUnits 换算为微秒；新写入一律 XAdmin_UnixNowUs()。 */

static const char* Notify_StrOrEmpty(const char* text)
{
	return text ? text : "";
}

/* 站内信为纯文本业务：输出边界统一做 HTML 实体转义，任何客户端
 * （layui/外部 App）直接渲染都不执行脚本（攻击评审 NOT-8 裁定）。 */
static str Notify_EscapeHtml(const char* text)
{
	size_t cap = 1, i;
	char* out;
	size_t n = 0;

	if (!text) return NULL;
	for (i = 0; text[i]; i++) {
		switch (text[i]) {
		case '&': cap += 5; break;
		case '<': case '>': cap += 4; break;
		case '"': case '\'': cap += 6; break;
		default: cap++;
		}
	}
	out = xrtMalloc(cap);
	for (i = 0; text[i]; i++) {
		switch (text[i]) {
		case '&': memcpy(out + n, "&amp;", 5); n += 5; break;
		case '<': memcpy(out + n, "&lt;", 4); n += 4; break;
		case '>': memcpy(out + n, "&gt;", 4); n += 4; break;
		case '"': memcpy(out + n, "&quot;", 6); n += 6; break;
		case '\'': memcpy(out + n, "&#39;", 5); n += 5; break;
		default: out[n++] = text[i];
		}
	}
	out[n] = 0;
	return out;
}

/* actionUrl 只放行站内相对路径与 http(s) 外链，阻断 javascript:/data: 等。 */
static const char* Notify_SafeActionUrl(const char* url)
{
	if (!url || !url[0]) return "";
	if (url[0] == '/' && url[1] != '/' && url[1] != '\\') return url;   /* 排除 // 与 /\ */
	if (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0) return url;
	return "";
}

/* ID 文本解析：抽取所有正整数（逗号/空白/任意分隔），去重入数组。 */
static void Notify_ParseIDText(const char* text, xvalue* ids)
{
	const char* p;
	int64 value = 0;
	bool inNumber = false;

	if (!text || !ids || xrtValueType(ids) != XVALUE_ARRAY) return;
	for (p = text; ; p++) {
		char c = *p;
		if (c >= '0' && c <= '9') {
			value = value * 10 + (int64)(c - '0');
			inNumber = true;
			continue;
		}
		if (inNumber) {
			if (value > 0 && !XAdminIDArrayContainsInt(ids, value))
				xrtValueArrayAppendNew(ids, xrtValueInt(value));
			value = 0;
			inNumber = false;
		}
		if (c == '\0') break;
	}
}

static void Notify_AppendRecipient(xvalue* recipients, int64 member_id, const char* username, const char* nickname, const char* email, int64 group_id)
{
	size_t i, count;

	if (!recipients || xrtValueType(recipients) != XVALUE_ARRAY || member_id <= 0) return;
	count = xrtValueCount(recipients);
	for (i = 0; i < count; i++) {
		xvalue* item = xrtValueArrayGet(recipients, i);
		if (item && ValueInt(item, "memberId") == member_id) return;
	}
	{
		xvalue* row = xrtValueObject();
		ValueSetInt(row, "memberId", member_id);
		ValueSetInt(row, "groupId", group_id);
		ValueSetText(row, "username", Notify_StrOrEmpty(username));
		ValueSetText(row, "nickname", Notify_StrOrEmpty(nickname));
		ValueSetText(row, "email", Notify_StrOrEmpty(email));
		xrtValueArrayAppendNew(recipients, row);
	}
}

static void Notify_CollectMembersByIDList(xvalue* recipients, xvalue* ids)
{
	sqlite3_stmt* stmt = NULL;
	size_t i, count;

	if (!recipients || !ids || xrtValueType(ids) != XVALUE_ARRAY) return;
	if (sqlite3_prepare_v3(G_DB,
		"SELECT id, username, nickname, email, groupId FROM member WHERE id = ? AND isDelete = 0 AND status = 1",
		-1, 0, &stmt, NULL) != SQLITE_OK) return;
	count = xrtValueCount(ids);
	for (i = 0; i < count; i++) {
		sqlite3_bind_int64(stmt, 1, ValueArrayInt(ids, i));
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			Notify_AppendRecipient(recipients,
				sqlite3_column_int64(stmt, 0),
				(const char*)sqlite3_column_text(stmt, 1),
				(const char*)sqlite3_column_text(stmt, 2),
				(const char*)sqlite3_column_text(stmt, 3),
				sqlite3_column_int64(stmt, 4));
		}
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
	}
	sqlite3_finalize(stmt);
}

static void Notify_CollectMembersByGroupList(xvalue* recipients, xvalue* ids)
{
	sqlite3_stmt* stmt = NULL;
	size_t i, count;

	if (!recipients || !ids || xrtValueType(ids) != XVALUE_ARRAY) return;
	if (sqlite3_prepare_v3(G_DB,
		"SELECT id, username, nickname, email, groupId FROM member WHERE groupId = ? AND isDelete = 0 AND status = 1 ORDER BY id ASC",
		-1, 0, &stmt, NULL) != SQLITE_OK) return;
	count = xrtValueCount(ids);
	for (i = 0; i < count; i++) {
		sqlite3_bind_int64(stmt, 1, ValueArrayInt(ids, i));
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			Notify_AppendRecipient(recipients,
				sqlite3_column_int64(stmt, 0),
				(const char*)sqlite3_column_text(stmt, 1),
				(const char*)sqlite3_column_text(stmt, 2),
				(const char*)sqlite3_column_text(stmt, 3),
				sqlite3_column_int64(stmt, 4));
		}
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
	}
	sqlite3_finalize(stmt);
}

static void Notify_CollectAllMembers(xvalue* recipients)
{
	sqlite3_stmt* stmt = NULL;

	if (!recipients || xrtValueType(recipients) != XVALUE_ARRAY) return;
	if (sqlite3_prepare_v3(G_DB,
		"SELECT id, username, nickname, email, groupId FROM member WHERE isDelete = 0 AND status = 1 ORDER BY id ASC",
		-1, 0, &stmt, NULL) != SQLITE_OK) return;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		Notify_AppendRecipient(recipients,
			sqlite3_column_int64(stmt, 0),
			(const char*)sqlite3_column_text(stmt, 1),
			(const char*)sqlite3_column_text(stmt, 2),
			(const char*)sqlite3_column_text(stmt, 3),
			sqlite3_column_int64(stmt, 4));
	}
	sqlite3_finalize(stmt);
}

static xvalue* Notify_BuildRecipients(const char* sendType, const char* memberIds, const char* groupIds)
{
	xvalue* recipients = xrtValueArray();

	if (!recipients) return NULL;
	if (sendType && (strcmp(sendType, "single") == 0 || strcmp(sendType, "users") == 0)) {
		xvalue* ids = xrtValueArray();
		Notify_ParseIDText(memberIds, ids);
		Notify_CollectMembersByIDList(recipients, ids);
		xrtValueRelease(ids);
		return recipients;
	}
	if (sendType && strcmp(sendType, "groups") == 0) {
		xvalue* ids = xrtValueArray();
		Notify_ParseIDText(groupIds, ids);
		Notify_CollectMembersByGroupList(recipients, ids);
		xrtValueRelease(ids);
		return recipients;
	}
	if (sendType && strcmp(sendType, "all") == 0)
		Notify_CollectAllMembers(recipients);
	return recipients;
}

bool Notify_Send(const char* sendType, const char* memberIds, const char* groupIds,
	const char* title, const char* content, const char* actionUrl,
	int64 senderAdminId, str* outMessage, int64* outMessageId, int64* outRecipientCount)
{
	xvalue* recipients = NULL;
	sqlite3_stmt* stmtMessage = NULL;
	sqlite3_stmt* stmtRecipient = NULL;
	xtime now = XAdmin_UnixNowUs();
	int64 count = 0, messageId = 0;
	bool ok = false;

	if (outMessage) *outMessage = NULL;
	if (outMessageId) *outMessageId = 0;
	if (outRecipientCount) *outRecipientCount = 0;

	if (!title || !title[0] || !content || !content[0]) {
		if (outMessage) *outMessage = xrtStrDup("标题和内容不能为空");
		return false;
	}
	recipients = Notify_BuildRecipients(sendType, memberIds, groupIds);
	if (!recipients) {
		if (outMessage) *outMessage = xrtStrDup("收件人构建失败");
		return false;
	}
	count = (int64)xrtValueCount(recipients);
	if (count <= 0) {
		if (outMessage) *outMessage = xrtStrDup("没有匹配到可用的前台用户");
		xrtValueRelease(recipients);
		return false;
	}

	sqlite3_exec(G_DB, "BEGIN TRANSACTION", NULL, NULL, NULL);
	if (sqlite3_prepare_v3(G_DB,
		"INSERT INTO notify_message (title, content, type, sourcePlugin, bizType, bizId, actionUrl, payloadJson, senderAdminId, sendType, createTime) "
		"VALUES (?, ?, 'manual', '', '', 0, ?, '', ?, ?, ?)",
		-1, 0, &stmtMessage, NULL) != SQLITE_OK) goto cleanup;
	sqlite3_bind_text(stmtMessage, 1, title, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmtMessage, 2, content, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmtMessage, 3, Notify_SafeActionUrl(actionUrl), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmtMessage, 4, senderAdminId);
	sqlite3_bind_text(stmtMessage, 5, Notify_StrOrEmpty(sendType), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmtMessage, 6, now);
	if (sqlite3_step(stmtMessage) != SQLITE_DONE) goto cleanup;
	messageId = sqlite3_last_insert_rowid(G_DB);
	sqlite3_finalize(stmtMessage);
	stmtMessage = NULL;

	if (sqlite3_prepare_v3(G_DB,
		"INSERT INTO notify_recipient (messageId, memberId, isRead, readTime, isDelete, deleteTime, createTime) VALUES (?, ?, 0, 0, 0, 0, ?)",
		-1, 0, &stmtRecipient, NULL) != SQLITE_OK) goto cleanup;
	{
		size_t i, total = xrtValueCount(recipients);
		for (i = 0; i < total; i++) {
			xvalue* row = xrtValueArrayGet(recipients, i);
			sqlite3_bind_int64(stmtRecipient, 1, messageId);
			sqlite3_bind_int64(stmtRecipient, 2, ValueInt(row, "memberId"));
			sqlite3_bind_int64(stmtRecipient, 3, now);
			if (sqlite3_step(stmtRecipient) != SQLITE_DONE) goto cleanup;
			sqlite3_reset(stmtRecipient);
			sqlite3_clear_bindings(stmtRecipient);
		}
	}
	ok = true;

cleanup:
	if (stmtMessage) { sqlite3_finalize(stmtMessage); stmtMessage = NULL; }
	if (stmtRecipient) { sqlite3_finalize(stmtRecipient); stmtRecipient = NULL; }
	xrtValueRelease(recipients);
	if (ok) {
		sqlite3_exec(G_DB, "COMMIT", NULL, NULL, NULL);
		if (outMessage) *outMessage = xrtFormat("已发送给 %lld 位用户", (long long)count);
		if (outMessageId) *outMessageId = messageId;
		if (outRecipientCount) *outRecipientCount = count;
	} else {
		sqlite3_exec(G_DB, "ROLLBACK", NULL, NULL, NULL);
		if (outMessage && !*outMessage) *outMessage = xrtStrDup("发送失败，请稍后重试");
	}
	return ok;
}

static xvalue* Notify_AdminList(int64 page, int64 limit, const char* search, int64* outCount)
{
	sqlite3_stmt* stmt = NULL, * stmtCount = NULL;
	int64 offset;
	xvalue* data = xrtValueArray();
	bool hasSearch = search && search[0];

	if (outCount) *outCount = 0;
	if (page <= 0) page = 1;
	if (limit <= 0) limit = 20;
	offset = (page - 1) * limit;

	if (hasSearch) {
		sqlite3_prepare_v3(G_DB,
			"SELECT m.id, m.title, m.type, m.sendType, m.createTime, "
			"(SELECT COUNT(*) FROM notify_recipient r WHERE r.messageId = m.id) "
			"FROM notify_message m WHERE m.title LIKE ? OR m.content LIKE ? ORDER BY m.id DESC LIMIT ? OFFSET ?",
			-1, 0, &stmt, NULL);
		sqlite3_bind_text(stmt, 1, search, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, search, -1, SQLITE_TRANSIENT);
		sqlite3_prepare_v3(G_DB,
			"SELECT COUNT(*) FROM notify_message WHERE title LIKE ? OR content LIKE ?",
			-1, 0, &stmtCount, NULL);
		sqlite3_bind_text(stmtCount, 1, search, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmtCount, 2, search, -1, SQLITE_TRANSIENT);
	} else {
		sqlite3_prepare_v3(G_DB,
			"SELECT m.id, m.title, m.type, m.sendType, m.createTime, "
			"(SELECT COUNT(*) FROM notify_recipient r WHERE r.messageId = m.id) "
			"FROM notify_message m ORDER BY m.id DESC LIMIT ? OFFSET ?",
			-1, 0, &stmt, NULL);
		sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM notify_message", -1, 0, &stmtCount, NULL);
	}
	if (stmt) {
		int bindBase = hasSearch ? 3 : 1;
		sqlite3_bind_int64(stmt, bindBase, limit);
		sqlite3_bind_int64(stmt, bindBase + 1, offset);
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			xvalue* row = xrtValueObject();
			ValueSetInt(row, "id", sqlite3_column_int64(stmt, 0));
			ValueSetOwnedText(row, "title", Notify_EscapeHtml(Notify_StrOrEmpty((const char*)sqlite3_column_text(stmt, 1))));
			ValueSetText(row, "type", Notify_StrOrEmpty((const char*)sqlite3_column_text(stmt, 2)));
			ValueSetText(row, "sendType", Notify_StrOrEmpty((const char*)sqlite3_column_text(stmt, 3)));
			ValueSetInt(row, "recipientCount", sqlite3_column_int64(stmt, 5));
			ValueSetOwnedText(row, "createTime", TimeText(sqlite3_column_int64(stmt, 4), TIME_TEXT_DATETIME));
			xrtValueArrayAppendNew(data, row);
		}
		sqlite3_finalize(stmt);
	}
	if (stmtCount) {
		if (sqlite3_step(stmtCount) == SQLITE_ROW && outCount)
			*outCount = sqlite3_column_int64(stmtCount, 0);
		sqlite3_finalize(stmtCount);
	}
	return data;
}

static xvalue* Notify_FillRow(xvalue* row, sqlite3_stmt* stmt)
{
	ValueSetInt(row, "id", sqlite3_column_int64(stmt, 0));
	ValueSetInt(row, "messageId", sqlite3_column_int64(stmt, 1));
	ValueSetBool(row, "isRead", sqlite3_column_int(stmt, 2) == 1);
	ValueSetInt(row, "readTime", sqlite3_column_int64(stmt, 3));
	ValueSetOwnedText(row, "title", Notify_EscapeHtml(Notify_StrOrEmpty((const char*)sqlite3_column_text(stmt, 4))));
	ValueSetOwnedText(row, "content", Notify_EscapeHtml(Notify_StrOrEmpty((const char*)sqlite3_column_text(stmt, 5))));
	ValueSetText(row, "type", Notify_StrOrEmpty((const char*)sqlite3_column_text(stmt, 6)));
	ValueSetText(row, "actionUrl", Notify_SafeActionUrl(Notify_StrOrEmpty((const char*)sqlite3_column_text(stmt, 7))));
	ValueSetInt(row, "createTime", sqlite3_column_int64(stmt, 8));
	return row;
}

static xvalue* Notify_MemberList(int64 memberId, int64 page, int64 limit, int64* outCount)
{
	sqlite3_stmt* stmt = NULL, * stmtCount = NULL;
	xvalue* data = xrtValueArray();
	int64 offset;

	if (outCount) *outCount = 0;
	if (page <= 0) page = 1;
	if (limit <= 0) limit = 20;
	offset = (page - 1) * limit;

	if (sqlite3_prepare_v3(G_DB,
		"SELECT r.id, r.messageId, r.isRead, r.readTime, m.title, m.content, m.type, m.actionUrl, m.createTime "
		"FROM notify_recipient r INNER JOIN notify_message m ON r.messageId = m.id "
		"WHERE r.memberId = ? AND r.isDelete = 0 ORDER BY r.id DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, memberId);
		sqlite3_bind_int64(stmt, 2, limit);
		sqlite3_bind_int64(stmt, 3, offset);
		while (sqlite3_step(stmt) == SQLITE_ROW)
			xrtValueArrayAppendNew(data, Notify_FillRow(xrtValueObject(), stmt));
		sqlite3_finalize(stmt);
	}
	if (sqlite3_prepare_v3(G_DB,
		"SELECT COUNT(*) FROM notify_recipient WHERE memberId = ? AND isDelete = 0",
		-1, 0, &stmtCount, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmtCount, 1, memberId);
		if (sqlite3_step(stmtCount) == SQLITE_ROW && outCount)
			*outCount = sqlite3_column_int64(stmtCount, 0);
		sqlite3_finalize(stmtCount);
	}
	return data;
}

static int64 Notify_MemberUnreadCount(int64 memberId)
{
	sqlite3_stmt* stmt = NULL;
	int64 count = 0;

	if (sqlite3_prepare_v3(G_DB,
		"SELECT COUNT(*) FROM notify_recipient WHERE memberId = ? AND isDelete = 0 AND isRead = 0",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, memberId);
		if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);
	}
	return count;
}

static xvalue* Notify_MemberDetail(int64 memberId, int64 recipientId)
{
	sqlite3_stmt* stmt = NULL;
	xvalue* row = NULL;

	if (sqlite3_prepare_v3(G_DB,
		"SELECT r.id, r.messageId, r.isRead, r.readTime, m.title, m.content, m.type, m.actionUrl, m.createTime "
		"FROM notify_recipient r INNER JOIN notify_message m ON r.messageId = m.id "
		"WHERE r.id = ? AND r.memberId = ? AND r.isDelete = 0 LIMIT 1",
		-1, 0, &stmt, NULL) != SQLITE_OK) return NULL;
	sqlite3_bind_int64(stmt, 1, recipientId);
	sqlite3_bind_int64(stmt, 2, memberId);
	if (sqlite3_step(stmt) == SQLITE_ROW)
		row = Notify_FillRow(xrtValueObject(), stmt);
	sqlite3_finalize(stmt);
	return row;
}

static bool Notify_MarkRead(int64 memberId, xvalue* ids, bool readAll)
{
	sqlite3_stmt* stmt = NULL;
	xtime now = XAdmin_UnixNowUs();
	bool ok = false;

	if (readAll) {
		if (sqlite3_prepare_v3(G_DB,
			"UPDATE notify_recipient SET isRead = 1, readTime = ? WHERE memberId = ? AND isDelete = 0 AND isRead = 0",
			-1, 0, &stmt, NULL) != SQLITE_OK) return false;
		sqlite3_bind_int64(stmt, 1, now);
		sqlite3_bind_int64(stmt, 2, memberId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		return true;
	}
	if (!ids || xrtValueType(ids) != XVALUE_ARRAY || xrtValueCount(ids) == 0) return false;
	if (sqlite3_prepare_v3(G_DB,
		"UPDATE notify_recipient SET isRead = 1, readTime = ? WHERE id = ? AND memberId = ? AND isDelete = 0",
		-1, 0, &stmt, NULL) != SQLITE_OK) return false;
	{
		size_t i, count = xrtValueCount(ids);
		for (i = 0; i < count; i++) {
			sqlite3_bind_int64(stmt, 1, now);
			sqlite3_bind_int64(stmt, 2, ValueArrayInt(ids, i));
			sqlite3_bind_int64(stmt, 3, memberId);
			sqlite3_step(stmt);
			sqlite3_reset(stmt);
			sqlite3_clear_bindings(stmt);
			ok = true;
		}
	}
	sqlite3_finalize(stmt);
	return ok;
}

static bool Notify_MemberDelete(int64 memberId, xvalue* ids)
{
	sqlite3_stmt* stmt = NULL;
	xtime now = XAdmin_UnixNowUs();
	bool ok = false;

	if (!ids || xrtValueType(ids) != XVALUE_ARRAY || xrtValueCount(ids) == 0) return false;
	if (sqlite3_prepare_v3(G_DB,
		"UPDATE notify_recipient SET isDelete = 1, deleteTime = ? WHERE id = ? AND memberId = ? AND isDelete = 0",
		-1, 0, &stmt, NULL) != SQLITE_OK) return false;
	{
		size_t i, count = xrtValueCount(ids);
		for (i = 0; i < count; i++) {
			sqlite3_bind_int64(stmt, 1, now);
			sqlite3_bind_int64(stmt, 2, ValueArrayInt(ids, i));
			sqlite3_bind_int64(stmt, 3, memberId);
			sqlite3_step(stmt);
			sqlite3_reset(stmt);
			sqlite3_clear_bindings(stmt);
			ok = true;
		}
	}
	sqlite3_finalize(stmt);
	return ok;
}

/* ---- 自装：表 / URI / 菜单 ---- */

static void Notify_ExecSQLIgnore(const char* sql)
{
	sqlite3_exec(G_DB, sql, NULL, NULL, NULL);
}

static bool Notify_EnsureURIItem(const char* uri, bool needAuth)
{
	sqlite3_stmt* stmt = NULL;
	xtime now = XAdmin_UnixNowUs();
	int64 id = 0;

	if (!uri || !uri[0]) return false;
	if (sqlite3_prepare_v3(G_DB, "SELECT id FROM uris WHERE uri = ?", -1, 0, &stmt, NULL) != SQLITE_OK)
		return false;
	sqlite3_bind_text(stmt, 1, uri, -1, SQLITE_TRANSIENT);
	if (sqlite3_step(stmt) == SQLITE_ROW) id = sqlite3_column_int64(stmt, 0);
	sqlite3_finalize(stmt);

	if (id > 0) {
		if (sqlite3_prepare_v3(G_DB,
			"UPDATE uris SET authID = 1, isBackend = 0, needAuth = ?, needLog = 0, keepActive = 0, updateTime = ? WHERE id = ?",
			-1, 0, &stmt, NULL) != SQLITE_OK) return false;
		sqlite3_bind_int(stmt, 1, needAuth ? 1 : 0);
		sqlite3_bind_int64(stmt, 2, now);
		sqlite3_bind_int64(stmt, 3, id);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		return true;
	}
	if (sqlite3_prepare_v3(G_DB,
		"INSERT INTO uris (authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime) "
		"VALUES (1, ?, '', 0, ?, 0, 0, 0, ?, ?)",
		-1, 0, &stmt, NULL) != SQLITE_OK) return false;
	sqlite3_bind_text(stmt, 1, uri, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, needAuth ? 1 : 0);
	sqlite3_bind_int64(stmt, 3, now);
	sqlite3_bind_int64(stmt, 4, now);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return true;
}

static int Notify_FindMenuParentID(void)
{
	sqlite3_stmt* stmt = NULL;
	int parentId = 0;

	if (sqlite3_prepare_v3(G_DB,
		"SELECT parent FROM menu WHERE isDelete = 0 AND href = '/admin/view/member/user' ORDER BY id ASC LIMIT 1",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW) parentId = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}
	return parentId;
}

static void Notify_EnsureMenuItem(const char* title, const char* icon, const char* href, int parentId, int sort, const char* remark)
{
	sqlite3_stmt* stmt = NULL;
	xtime now = XAdmin_UnixNowUs();
	int64 id = 0;

	if (!href || !href[0] || parentId <= 0) return;
	if (sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE isDelete = 0 AND href = ? ORDER BY id ASC LIMIT 1",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_text(stmt, 1, href, -1, SQLITE_TRANSIENT);
		if (sqlite3_step(stmt) == SQLITE_ROW) id = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);
	}
	if (id > 0) {
		if (sqlite3_prepare_v3(G_DB,
			"UPDATE menu SET parent = ?, title = ?, icon = ?, type = 1, openType = '_component', href = ?, sort = ?, visible = 1, remark = ?, updateTime = ? WHERE id = ?",
			-1, 0, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_int(stmt, 1, parentId);
			sqlite3_bind_text(stmt, 2, title, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, icon, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, href, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 5, sort);
			sqlite3_bind_text(stmt, 6, remark, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 7, now);
			sqlite3_bind_int64(stmt, 8, id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		return;
	}
	if (sqlite3_prepare_v3(G_DB,
		"INSERT INTO menu (parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete) "
		"VALUES (?, ?, ?, 1, '_component', ?, ?, 1, ?, ?, ?, 0)",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int(stmt, 1, parentId);
		sqlite3_bind_text(stmt, 2, title, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, icon, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, href, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 5, sort);
		sqlite3_bind_text(stmt, 6, remark, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 7, now);
		sqlite3_bind_int64(stmt, 8, now);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
}

void Notify_Init(void)
{
	printf("        Notify_Init \n");
	Notify_ExecSQLIgnore("CREATE TABLE IF NOT EXISTS notify_message (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT DEFAULT '', content TEXT DEFAULT '', type TEXT DEFAULT 'manual', sourcePlugin TEXT DEFAULT '', bizType TEXT DEFAULT '', bizId INTEGER DEFAULT 0, actionUrl TEXT DEFAULT '', payloadJson TEXT DEFAULT '', senderAdminId INTEGER DEFAULT 0, sendType TEXT DEFAULT 'users', createTime INTEGER DEFAULT 0)");
	Notify_ExecSQLIgnore("CREATE TABLE IF NOT EXISTS notify_recipient (id INTEGER PRIMARY KEY AUTOINCREMENT, messageId INTEGER DEFAULT 0, memberId INTEGER DEFAULT 0, isRead INTEGER DEFAULT 0, readTime INTEGER DEFAULT 0, isDelete INTEGER DEFAULT 0, deleteTime INTEGER DEFAULT 0, createTime INTEGER DEFAULT 0)");
	Notify_ExecSQLIgnore("CREATE TABLE IF NOT EXISTS member_notify_setting (memberId INTEGER PRIMARY KEY, siteInboxEnabled INTEGER DEFAULT 1, emailEnabled INTEGER DEFAULT 1, updateTime INTEGER DEFAULT 0)");
	Notify_ExecSQLIgnore("CREATE INDEX IF NOT EXISTS idx_notify_recipient_member ON notify_recipient(memberId, isDelete, isRead)");
	Notify_EnsureURIItem("/api/v1/notify/list", true);
	Notify_EnsureURIItem("/api/v1/notify/unread_count", true);
	Notify_EnsureURIItem("/api/v1/notify/detail", true);
	Notify_EnsureURIItem("/api/v1/notify/read", true);
	Notify_EnsureURIItem("/api/v1/notify/read_all", true);
	Notify_EnsureURIItem("/api/v1/notify/delete", true);
	{
		int parent = Notify_FindMenuParentID();
		Notify_EnsureMenuItem("站内信管理", "layui-icon layui-icon-notice", "/admin/view/member/notify", parent, 401600, "前台用户站内信发送与查看");
	}
}
