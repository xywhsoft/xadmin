/* 计划任务管理路由：任务 CRUD/批量/启停/立即运行/复制/示例/预览、
 * 仪表盘、运行日志（查询/清空/导出）、任务导入导出 + 视图页。 */

/* 导出公共：selected 时按 ids 过滤 */
static void Sched_ExportCommon(XS_RequestObject objReq, XS_ResponseObject objResp, bool selected)
{
	sqlite3_stmt* stmt = NULL;
	xvalue* data = xrtValueArray();
	size_t size = 0;
	char* json;
	str header;

	if (sqlite3_prepare_v3(G_DB,
		"SELECT name, scheduleType, execType, shellType, codeText, customText, cronExpr, onceAt, "
		"intervalValue, intervalUnit, startAt, timeoutSec, overlapPolicy, misfirePolicy, workDir, "
		"parallelLimit, retryCount, retryDelaySec FROM sched_task WHERE isDelete = 0 AND systemKey IS NULL ORDER BY id ASC",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		xvalue* keep = NULL;
		xvalue* form = NULL;
		if (selected) {
			form = JsonParseN(XAdmin_ReqBody(objReq), XAdmin_ReqBodyLen(objReq));
			/* keep 为 ValueGet 借用引用（不可直接 release），存活期由 form 持有 */
			keep = form && xrtValueType(form) == XVALUE_OBJECT ? ValueGet(form, "ids") : NULL;
		}
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			xvalue* row = xrtValueObject();
			ValueSetText(row, "name", Sched_SQLiteTextOrEmpty(stmt, 0));
			ValueSetText(row, "scheduleType", Sched_SQLiteTextOrEmpty(stmt, 1));
			ValueSetText(row, "execType", Sched_SQLiteTextOrEmpty(stmt, 2));
			ValueSetText(row, "shellType", Sched_SQLiteTextOrEmpty(stmt, 3));
			ValueSetText(row, "codeText", Sched_SQLiteTextOrEmpty(stmt, 4));
			ValueSetText(row, "customText", Sched_SQLiteTextOrEmpty(stmt, 5));
			ValueSetText(row, "cronExpr", Sched_SQLiteTextOrEmpty(stmt, 6));
			ValueSetInt(row, "onceAt", sqlite3_column_int64(stmt, 7));
			ValueSetInt(row, "intervalValue", sqlite3_column_int64(stmt, 8));
			ValueSetText(row, "intervalUnit", Sched_SQLiteTextOrEmpty(stmt, 9));
			ValueSetInt(row, "startAt", sqlite3_column_int64(stmt, 10));
			ValueSetInt(row, "timeoutSec", sqlite3_column_int(stmt, 11));
			ValueSetText(row, "overlapPolicy", Sched_SQLiteTextOrEmpty(stmt, 12));
			ValueSetText(row, "misfirePolicy", Sched_SQLiteTextOrEmpty(stmt, 13));
			ValueSetText(row, "workDir", Sched_SQLiteTextOrEmpty(stmt, 14));
			ValueSetInt(row, "parallelLimit", sqlite3_column_int(stmt, 15));
			ValueSetInt(row, "retryCount", sqlite3_column_int(stmt, 16));
			ValueSetInt(row, "retryDelaySec", sqlite3_column_int(stmt, 17));
			xrtValueArrayAppendNew(data, row);
		}
		sqlite3_finalize(stmt);
		xrtValueRelease(form); /* 借用的 keep 随容器一并释放（M7：原实现泄漏 form 且过度释放借用） */
	}
	json = xrtJsonStringify(data, false, &size);
	xrtValueRelease(data);
	header = xrtFormat("Content-Type: application/json; charset=utf-8\r\nContent-Disposition: attachment; filename=\"sched_tasks.json\"\r\n");
	xsHttpReplyAuto(objResp, 200, header, json, 0);
	xrtFree(header);
	xrtFree(json);
}

static void Sched_Reply(XS_ResponseObject objResp, xvalue* ret)
{
	size_t size = 0;
	char* json = xrtJsonStringify(ret, false, &size);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, json, 0);
	xrtFree(json);
	xrtValueRelease(ret);
}

static void Sched_ReplyMessage(XS_ResponseObject objResp, bool ok, str message)
{
	xvalue* ret = xrtValueObject();
	ValueSetBool(ret, "result", ok);
	ValueSetText(ret, "message", message ? message : "");
	Sched_Reply(objResp, ret);
	xrtFree(message);
}

static void Sched_FillTaskRow(xvalue* row, sqlite3_stmt* stmt)
{
	ValueSetInt(row, "id", sqlite3_column_int64(stmt, 0));
	ValueSetText(row, "name", Sched_SQLiteTextOrEmpty(stmt, 1));
	ValueSetBool(row, "enabled", sqlite3_column_int(stmt, 2) != 0);
	ValueSetText(row, "scheduleType", Sched_SQLiteTextOrEmpty(stmt, 3));
	ValueSetText(row, "execType", Sched_SQLiteTextOrEmpty(stmt, 4));
	ValueSetBool(row, "managed", Sched_IsManagedTask(sqlite3_column_int64(stmt, 0)));
	ValueSetText(row, "shellType", Sched_SQLiteTextOrEmpty(stmt, 5));
	ValueSetText(row, "overlapPolicy", Sched_SQLiteTextOrEmpty(stmt, 6));
	ValueSetText(row, "misfirePolicy", Sched_SQLiteTextOrEmpty(stmt, 7));
	ValueSetInt(row, "timeoutSec", sqlite3_column_int(stmt, 8));
	ValueSetInt(row, "nextRunAt", sqlite3_column_int64(stmt, 9));
	ValueSetInt(row, "lastRunAt", sqlite3_column_int64(stmt, 10));
	ValueSetInt(row, "lastFinishAt", sqlite3_column_int64(stmt, 11));
	ValueSetInt(row, "runningCount", sqlite3_column_int(stmt, 12));
	ValueSetBool(row, "pendingRun", sqlite3_column_int(stmt, 13) != 0);
	ValueSetInt(row, "retryCount", sqlite3_column_int(stmt, 14));
	ValueSetInt(row, "retryState", sqlite3_column_int(stmt, 15));
	ValueSetText(row, "lastStatus", Sched_SQLiteTextOrEmpty(stmt, 16));
	ValueSetText(row, "lastMessage", Sched_SQLiteTextOrEmpty(stmt, 17));
	ValueSetInt(row, "lastExitCode", sqlite3_column_int(stmt, 18));
	ValueSetOwnedText(row, "nextRunText", TimeText(sqlite3_column_int64(stmt, 9), TIME_TEXT_DATETIME));
	ValueSetOwnedText(row, "lastRunText", TimeText(sqlite3_column_int64(stmt, 10), TIME_TEXT_DATETIME));
}

// 任务列表
void Request_Sched_Tasks(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char page[12] = {0}, limit[12] = {0}, search[64] = {0};
	int iPage, iLimit, iOffset;
	sqlite3_stmt* stmt = NULL, * stmtCount = NULL;
	xvalue* data = xrtValueArray();
	int64 count = 0;
	bool hasSearch;

	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_GET) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	xsReqQueryValue(objReq, "page", page, sizeof(page));
	xsReqQueryValue(objReq, "limit", limit, sizeof(limit));
	xsReqQueryValue(objReq, "search", search, sizeof(search));
	iPage = atoi(page); iLimit = atoi(limit);
	if (iPage < 1) iPage = 1;
	if (iLimit < 1) iLimit = 20;
	if (iLimit > 100) iLimit = 100;
	iOffset = (iPage - 1) * iLimit;
	hasSearch = search[0] != '\0';

	if (sqlite3_prepare_v3(G_DB,
		hasSearch
		? "SELECT id, name, enabled, scheduleType, execType, shellType, overlapPolicy, misfirePolicy, timeoutSec, nextRunAt, lastRunAt, lastFinishAt, runningCount, pendingRun, retryCount, retryState, lastStatus, lastMessage, lastExitCode FROM sched_task WHERE isDelete = 0 AND name LIKE ? ORDER BY id DESC LIMIT ? OFFSET ?"
		: "SELECT id, name, enabled, scheduleType, execType, shellType, overlapPolicy, misfirePolicy, timeoutSec, nextRunAt, lastRunAt, lastFinishAt, runningCount, pendingRun, retryCount, retryState, lastStatus, lastMessage, lastExitCode FROM sched_task WHERE isDelete = 0 ORDER BY id DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		int bind = 1;
		if (hasSearch) {
			sqlite3_bind_text(stmt, bind++, search, -1, SQLITE_TRANSIENT);
		}
		sqlite3_bind_int(stmt, bind++, iLimit);
		sqlite3_bind_int(stmt, bind++, iOffset);
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			xvalue* row = xrtValueObject();
			Sched_FillTaskRow(row, stmt);
			xrtValueArrayAppendNew(data, row);
		}
		sqlite3_finalize(stmt);
	}
	if (sqlite3_prepare_v3(G_DB,
		hasSearch ? "SELECT COUNT(*) FROM sched_task WHERE isDelete = 0 AND name LIKE ?"
			: "SELECT COUNT(*) FROM sched_task WHERE isDelete = 0",
		-1, 0, &stmtCount, NULL) == SQLITE_OK) {
		if (hasSearch) sqlite3_bind_text(stmtCount, 1, search, -1, SQLITE_TRANSIENT);
		if (sqlite3_step(stmtCount) == SQLITE_ROW) count = sqlite3_column_int64(stmtCount, 0);
		sqlite3_finalize(stmtCount);
	}
	NotifyReplyAdminTable(objResp, data, count);
}

// 任务详情
void Request_Sched_Task(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char idText[24] = {0};
	int64 id;
	SchedTaskSnapshot task;
	sqlite3_stmt* stmt = NULL;
	xvalue* row;

	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_GET) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	xsReqQueryValue(objReq, "id", idText, sizeof(idText));
	id = Util_ParseI64(idText);
	memset(&task, 0, sizeof(task));
	if (id <= 0 || !Sched_LoadTaskSnapshotById(G_DB, id, &task)) {
		Sched_ReplyMessage(objResp, false, xrtStrDup("任务不存在"));
		Sched_FreeTaskSnapshot(&task);
		return;
	}
	row = xrtValueObject();
	ValueSetInt(row, "id", task.id);
	ValueSetText(row, "name", task.sName);
	ValueSetBool(row, "enabled", task.enabled);
	ValueSetText(row, "scheduleType", task.sScheduleType);
	ValueSetText(row, "execType", task.sExecType);
	ValueSetText(row, "shellType", task.sShellType);
	ValueSetText(row, "codeText", task.sCodeText);
	ValueSetText(row, "customText", task.sCustomText);
	ValueSetText(row, "cronExpr", task.sCronExpr);
	ValueSetInt(row, "onceAt", task.onceAt);
	ValueSetInt(row, "intervalValue", task.intervalValue);
	ValueSetText(row, "intervalUnit", task.sIntervalUnit);
	ValueSetInt(row, "startAt", task.startAt);
	ValueSetInt(row, "timeoutSec", task.timeoutSec);
	ValueSetText(row, "overlapPolicy", task.sOverlapPolicy);
	ValueSetText(row, "misfirePolicy", task.sMisfirePolicy);
	ValueSetText(row, "workDir", task.sWorkDir);
	ValueSetInt(row, "parallelLimit", task.parallelLimit);
	ValueSetInt(row, "retryCount", task.retryCount);
	ValueSetInt(row, "retryDelaySec", task.retryDelaySec);
	{
		xvalue* ret = xrtValueObject();
		ValueSetBool(ret, "result", true);
		ValueSetOwn(ret, "data", row);
		Sched_Reply(objResp, ret);
	}
	Sched_FreeTaskSnapshot(&task);
	(void)stmt;
}

// 任务保存（POST 新建 / PUT 更新）
void Request_Sched_Task_Save(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* form;
	bool update;
	str message = NULL;
	int64 taskId = 0;
	bool ok;

	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST && xsReqMethodID(objReq) != XHTTP_METHOD_PUT) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	update = xsReqMethodID(objReq) == XHTTP_METHOD_PUT;
	form = JsonParseN(XAdmin_ReqBody(objReq), XAdmin_ReqBodyLen(objReq));
	ok = Sched_SaveTaskRequest(form, update, &message, &taskId);
	if (form) xrtValueRelease(form);
	if (ok) {
		xvalue* ret = xrtValueObject();
		ValueSetBool(ret, "result", true);
		ValueSetText(ret, "message", message ? message : "");
		ValueSetInt(ret, "id", taskId);
		Sched_Reply(objResp, ret);
		xrtFree(message);
		return;
	}
	Sched_ReplyMessage(objResp, false, message);
}

// 任务删除
void Request_Sched_Task_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char idText[24] = {0};
	str message = NULL;

	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_DELETE && xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	xsReqQueryValue(objReq, "id", idText, sizeof(idText));
	if (Sched_DeleteTask(Util_ParseI64(idText), &message))
		Sched_ReplyMessage(objResp, true, message);
	else
		Sched_ReplyMessage(objResp, false, message);
}

// 任务启用/停用
void Request_Sched_Task_Enable(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char idText[24] = {0}, on[8] = {0};
	str message = NULL;

	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	xsReqQueryValue(objReq, "id", idText, sizeof(idText));
	xsReqQueryValue(objReq, "enabled", on, sizeof(on));
	if (Sched_SetEnabled(Util_ParseI64(idText), on[0] == '1', &message))
		Sched_ReplyMessage(objResp, true, message);
	else
		Sched_ReplyMessage(objResp, false, message);
}

// 立即运行
void Request_Sched_Task_Run(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char idText[24] = {0};
	str message = NULL;

	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	xsReqQueryValue(objReq, "id", idText, sizeof(idText));
	if (Sched_RunNow(Util_ParseI64(idText), &message))
		Sched_ReplyMessage(objResp, true, message);
	else
		Sched_ReplyMessage(objResp, false, message);
}

// 复制任务
void Request_Sched_Task_Copy(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char idText[24] = {0};
	str message = NULL;
	int64 taskId = 0;

	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	xsReqQueryValue(objReq, "id", idText, sizeof(idText));
	if (Sched_CopyTask(Util_ParseI64(idText), &message, &taskId)) {
		xvalue* ret = xrtValueObject();
		ValueSetBool(ret, "result", true);
		ValueSetText(ret, "message", message ? message : "");
		ValueSetInt(ret, "id", taskId);
		Sched_Reply(objResp, ret);
		xrtFree(message);
		return;
	}
	Sched_ReplyMessage(objResp, false, message);
}

// 示例任务
void Request_Sched_Task_Example(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char kind[8] = {0};
	str message = NULL;
	int64 taskId = 0;

	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	xsReqQueryValue(objReq, "kind", kind, sizeof(kind));
	if (Sched_CreateExampleTask(kind[0] == 'c' ? "c" : "shell", &message, &taskId)) {
		xvalue* ret = xrtValueObject();
		ValueSetBool(ret, "result", true);
		ValueSetText(ret, "message", message ? message : "");
		ValueSetInt(ret, "id", taskId);
		Sched_Reply(objResp, ret);
		xrtFree(message);
		return;
	}
	Sched_ReplyMessage(objResp, false, message);
}

// 批量：启用/停用/删除/运行
void Request_Sched_Task_Batch(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* form;
	str op, message = NULL;
	bool ok = false;

	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	form = JsonParseN(XAdmin_ReqBody(objReq), XAdmin_ReqBodyLen(objReq));
	if (!form || xrtValueType(form) != XVALUE_OBJECT) {
		if (form) xrtValueRelease(form);
		Sched_ReplyMessage(objResp, false, xrtStrDup("无效的请求数据"));
		return;
	}
	op = ValueText(form, "op");
	{
		xvalue* ids = ValueGet(form, "ids");
		int64 affected = 0, queued = 0, skipped = 0, failed = 0;
		if (Sched_TextEquals(op, "enable")) ok = Sched_SetEnabledBatch(ids, true, &affected, &message);
		else if (Sched_TextEquals(op, "disable")) ok = Sched_SetEnabledBatch(ids, false, &affected, &message);
		else if (Sched_TextEquals(op, "delete")) ok = Sched_DeleteTaskBatch(ids, &affected, &message);
		else if (Sched_TextEquals(op, "run")) ok = Sched_RunNowBatch(ids, &queued, &skipped, &failed, &message);
		else message = xrtStrDup("未知批量操作");
	}
	xrtValueRelease(form);
	Sched_ReplyMessage(objResp, ok, message);
}

// 触发预览（未来 N 次）
void Request_Sched_Preview(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* form;
	SchedTaskSnapshot task;
	xvalue* list = xrtValueArray();
	int64 cursor;
	int i;

	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	form = JsonParseN(XAdmin_ReqBody(objReq), XAdmin_ReqBodyLen(objReq));
	memset(&task, 0, sizeof(task));
	if (form && xrtValueType(form) == XVALUE_OBJECT)
		Sched_ReadTaskFieldFromBody(form, &task);
	if (form) xrtValueRelease(form);
	if (!Sched_TaskIsValidForSave(&task, NULL)) {
		Sched_FreeTaskSnapshot(&task);
		Sched_ReplyMessage(objResp, false, xrtStrDup("任务参数无效，无法预览"));
		return;
	}
	cursor = XAdmin_UnixNowUs();
	for (i = 0; i < 5; i++) {
		int64 next = Sched_CalcNextTime(&task, cursor);
		if (next <= 0) break;
		{
			xvalue* item = xrtValueObject();
			ValueSetInt(item, "at", next);
			ValueSetOwnedText(item, "text", TimeText(next, TIME_TEXT_DATETIME));
			xrtValueArrayAppendNew(list, item);
		}
		if (Sched_TextEquals(task.sScheduleType, "once")) break;
		cursor = next + 1;   /* 开区间：下一次计算严格晚于本次触发点 */
	}
	Sched_FreeTaskSnapshot(&task);
	{
		xvalue* ret = xrtValueObject();
		ValueSetBool(ret, "result", true);
		ValueSetOwn(ret, "data", list);
		Sched_Reply(objResp, ret);
	}
}

// 仪表盘
void Request_Sched_Dashboard(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	sqlite3_stmt* stmt = NULL;
	xvalue* ret = xrtValueObject();
	xvalue* data = xrtValueObject();

	(void)objServer; (void)objHost; (void)objSession;
	if (sqlite3_prepare_v3(G_DB,
		"SELECT COUNT(*), SUM(CASE WHEN enabled = 1 THEN 1 ELSE 0 END), SUM(CASE WHEN enabled = 0 THEN 1 ELSE 0 END), "
		"SUM(runningCount), SUM(pendingRun) FROM sched_task WHERE isDelete = 0",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			ValueSetInt(data, "total", sqlite3_column_int64(stmt, 0));
			ValueSetInt(data, "enabled", sqlite3_column_int64(stmt, 1));
			ValueSetInt(data, "disabled", sqlite3_column_int64(stmt, 2));
			ValueSetInt(data, "running", sqlite3_column_int64(stmt, 3));
			ValueSetInt(data, "pending", sqlite3_column_int64(stmt, 4));
		}
		sqlite3_finalize(stmt);
	}
	if (sqlite3_prepare_v3(G_DB,
		"SELECT status, COUNT(*) FROM sched_run_log WHERE startTime > ? GROUP BY status",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, XAdmin_UnixNowUs() - 86400LL * 1000000);
		{
			xvalue* byStatus = xrtValueObject();
			while (sqlite3_step(stmt) == SQLITE_ROW)
				ValueSetInt(byStatus, Sched_SQLiteTextOrEmpty(stmt, 0), sqlite3_column_int64(stmt, 1));
			sqlite3_finalize(stmt);
			ValueSetOwn(data, "lastDayStatus", byStatus);
		}
	}
	ValueSetBool(ret, "result", true);
	ValueSetOwn(ret, "data", data);
	Sched_Reply(objResp, ret);
}

// 运行日志
void Request_Sched_Logs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char page[12] = {0}, limit[12] = {0}, task[24] = {0}, status[16] = {0};
	int iPage, iLimit, iOffset;
	sqlite3_stmt* stmt = NULL, * stmtCount = NULL;
	xvalue* data = xrtValueArray();
	int64 count = 0;
	int64 taskId;

	(void)objServer; (void)objHost; (void)objSession;
	xsReqQueryValue(objReq, "page", page, sizeof(page));
	xsReqQueryValue(objReq, "limit", limit, sizeof(limit));
	xsReqQueryValue(objReq, "taskId", task, sizeof(task));
	xsReqQueryValue(objReq, "status", status, sizeof(status));
	iPage = atoi(page); iLimit = atoi(limit);
	if (iPage < 1) iPage = 1;
	if (iLimit < 1) iLimit = 20;
	if (iLimit > 100) iLimit = 100;
	iOffset = (iPage - 1) * iLimit;
	taskId = Util_ParseI64(task);

	if (sqlite3_prepare_v3(G_DB,
		"SELECT id, taskId, taskName, triggerSource, startTime, finishTime, durationMs, status, exitCode, message "
		"FROM sched_run_log WHERE (? <= 0 OR taskId = ?) AND (? = '' OR status = ?) ORDER BY id DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, taskId);
		sqlite3_bind_int64(stmt, 2, taskId);
		sqlite3_bind_text(stmt, 3, status, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, status, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 5, iLimit);
		sqlite3_bind_int(stmt, 6, iOffset);
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			xvalue* row = xrtValueObject();
			ValueSetInt(row, "id", sqlite3_column_int64(stmt, 0));
			ValueSetInt(row, "taskId", sqlite3_column_int64(stmt, 1));
			ValueSetText(row, "taskName", Sched_SQLiteTextOrEmpty(stmt, 2));
			ValueSetText(row, "triggerSource", Sched_SQLiteTextOrEmpty(stmt, 3));
			ValueSetOwnedText(row, "startTimeText", TimeText(sqlite3_column_int64(stmt, 4), TIME_TEXT_DATETIME));
			ValueSetOwnedText(row, "finishTimeText", TimeText(sqlite3_column_int64(stmt, 5), TIME_TEXT_DATETIME));
			ValueSetInt(row, "durationMs", sqlite3_column_int64(stmt, 6));
			ValueSetText(row, "status", Sched_SQLiteTextOrEmpty(stmt, 7));
			ValueSetInt(row, "exitCode", sqlite3_column_int(stmt, 8));
			ValueSetText(row, "message", Sched_SQLiteTextOrEmpty(stmt, 9));
			xrtValueArrayAppendNew(data, row);
		}
		sqlite3_finalize(stmt);
	}
	if (sqlite3_prepare_v3(G_DB,
		"SELECT COUNT(*) FROM sched_run_log WHERE (? <= 0 OR taskId = ?) AND (? = '' OR status = ?)",
		-1, 0, &stmtCount, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmtCount, 1, taskId);
		sqlite3_bind_int64(stmtCount, 2, taskId);
		sqlite3_bind_text(stmtCount, 3, status, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmtCount, 4, status, -1, SQLITE_TRANSIENT);
		if (sqlite3_step(stmtCount) == SQLITE_ROW) count = sqlite3_column_int64(stmtCount, 0);
		sqlite3_finalize(stmtCount);
	}
	NotifyReplyAdminTable(objResp, data, count);
}

// 日志清空
void Request_Sched_Logs_Clear(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char task[24] = {0};
	str message = NULL;

	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	xsReqQueryValue(objReq, "taskId", task, sizeof(task));
	Sched_ClearLogs(Util_ParseI64(task), &message);
	Sched_ReplyMessage(objResp, true, message);
}

// 日志导出（JSON 附件）
void Request_Sched_Logs_Export(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	sqlite3_stmt* stmt = NULL;
	xvalue* data = xrtValueArray();
	size_t size = 0;
	char* json;
	str header;

	(void)objServer; (void)objHost; (void)objSession;
	if (sqlite3_prepare_v3(G_DB,
		"SELECT id, taskId, taskName, triggerSource, startTime, finishTime, durationMs, status, exitCode, stdoutText, stderrText, message "
		"FROM sched_run_log ORDER BY id DESC LIMIT 5000",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			xvalue* row = xrtValueObject();
			ValueSetInt(row, "id", sqlite3_column_int64(stmt, 0));
			ValueSetInt(row, "taskId", sqlite3_column_int64(stmt, 1));
			ValueSetText(row, "taskName", Sched_SQLiteTextOrEmpty(stmt, 2));
			ValueSetText(row, "triggerSource", Sched_SQLiteTextOrEmpty(stmt, 3));
			ValueSetInt(row, "startTime", sqlite3_column_int64(stmt, 4));
			ValueSetInt(row, "finishTime", sqlite3_column_int64(stmt, 5));
			ValueSetInt(row, "durationMs", sqlite3_column_int64(stmt, 6));
			ValueSetText(row, "status", Sched_SQLiteTextOrEmpty(stmt, 7));
			ValueSetInt(row, "exitCode", sqlite3_column_int(stmt, 8));
			ValueSetText(row, "stdout", Sched_SQLiteTextOrEmpty(stmt, 9));
			ValueSetText(row, "stderr", Sched_SQLiteTextOrEmpty(stmt, 10));
			ValueSetText(row, "message", Sched_SQLiteTextOrEmpty(stmt, 11));
			xrtValueArrayAppendNew(data, row);
		}
		sqlite3_finalize(stmt);
	}
	json = xrtJsonStringify(data, false, &size);
	xrtValueRelease(data);
	header = xrtFormat("Content-Type: application/json; charset=utf-8\r\nContent-Disposition: attachment; filename=\"sched_logs.json\"\r\n");
	xsHttpReplyAuto(objResp, 200, header, json, 0);
	xrtFree(header);
	xrtFree(json);
}

// 任务导出（全量 JSON 附件）
void Request_Sched_Export(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	Sched_ExportCommon(objReq, objResp, false);
}

void Request_Sched_Export_Selected(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	Sched_ExportCommon(objReq, objResp, true);
}

// 任务导入
void Request_Sched_Import(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* form;
	str message = NULL;
	int64 imported = 0, skipped = 0, failed = 0;
	size_t i, count;

	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	form = JsonParseN(XAdmin_ReqBody(objReq), XAdmin_ReqBodyLen(objReq));
	if (!form || xrtValueType(form) != XVALUE_ARRAY) {
		if (form) xrtValueRelease(form);
		Sched_ReplyMessage(objResp, false, xrtStrDup("无效的导入数据（需要任务数组）"));
		return;
	}
	count = xrtValueCount(form);
	for (i = 0; i < count; i++) {
		xvalue* item = xrtValueArrayGet(form, i);
		str message2 = NULL;
		int64 tid = 0;
		if (!item || xrtValueType(item) != XVALUE_OBJECT) { failed++; continue; }
		{
			str name = ValueText(item, "name");
			if (!name || !name[0] || Sched_FindTaskIdByName(name) > 0) { skipped++; continue; }
		}
		if (Sched_SaveTaskRequest(item, false, &message2, &tid)) imported++;
		else failed++;
		xrtFree(message2);
	}
	xrtValueRelease(form);
	message = xrtFormat("导入 %lld，跳过 %lld，失败 %lld",
		(long long)imported, (long long)skipped, (long long)failed);
	Sched_ReplyMessage(objResp, true, message);
}

// ---- 视图页 ----

void Request_View_Sched_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "sched/list.html");
}

void Request_View_Sched_Edit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "sched/edit.html");
}

void Request_View_Sched_Dashboard(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "sched/dashboard.html");
}

void Request_View_Sched_Log(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "sched/log.html");
}
