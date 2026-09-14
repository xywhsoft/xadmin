/* 邮件家族路由（XADMIN_WITH_SMTP 门控）：任务列表/创建/状态/触发执行/
 * 重试/删除 + 视图页 + SMTP 连通测试。 */

#if XADMIN_WITH_SMTP

// 邮件任务管理接口（GET 列表 / POST 创建发送任务）
void Request_Member_Mail(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost;

	if (xsReqMethodID(objReq) == XHTTP_METHOD_GET) {
		char param[128];
		int64 page = 1, limit = 20, count = 0, taskId = 0;
		const char* search = NULL;
		xvalue* data;

		if (xsReqQueryValue(objReq, "page", param, sizeof(param)) > 0)
			page = Util_ParseI64(param);
		if (xsReqQueryValue(objReq, "limit", param, sizeof(param)) > 0)
			limit = Util_ParseI64(param);
		if (xsReqQueryValue(objReq, "search", param, sizeof(param)) > 0)
			search = param;
		if (xsReqQueryValue(objReq, "taskId", param, sizeof(param)) > 0)
			taskId = Util_ParseI64(param);

		data = Mail_AdminList(page, limit, search, taskId, &count);
		NotifyReplyAdminTable(objResp, data, count);
		return;
	}

	if (xsReqMethodID(objReq) == XHTTP_METHOD_POST) {
		xvalue* form = JsonParseN(XAdmin_ReqBody(objReq), XAdmin_ReqBodyLen(objReq));
		str subject, content, sendType, memberIds, groupIds, message = NULL;
		int64 created = 0, skipped = 0;
		bool ok;

		if (!form || xrtValueType(form) != XVALUE_OBJECT) {
			if (form) xrtValueRelease(form);
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据\"}", 0);
			return;
		}
		subject = ValueText(form, "subject");
		content = ValueText(form, "content");
		sendType = ValueText(form, "sendType");
		memberIds = ValueText(form, "memberIds");
		groupIds = ValueText(form, "groupIds");

		ok = Mail_CreateTasks(sendType ? sendType : "users", memberIds, groupIds,
			subject, content, &message, &created, &skipped);
		xrtValueRelease(form);

		if (ok)
			xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON,
				"{\"result\": true, \"message\": \"%s\", \"data\": {\"created\": %lld, \"skipped\": %lld}}",
				message ? message : "", (long long)created, (long long)skipped);
		else
			xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"%s\"}",
				message ? message : "");
		xrtFree(message);
		return;
	}

	LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

// 邮件队列状态
void Request_Member_Mail_Status(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_GET) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	{
		xvalue* ret = ValueObject();
		size_t size = 0;
		char* json;
		ValueSetBool(ret, "result", true);
		ValueSetOwn(ret, "data", Mail_QueueStatus());
		json = xrtJsonStringify(ret, false, &size);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, json, 0);
		xrtFree(json);
		xrtValueRelease(ret);
	}
}

// 触发队列立即执行
void Request_Member_Mail_RunPending(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	if (!Mail_Enabled()) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"邮件功能未启用\"}", 0);
		return;
	}
	if (!Mail_GetBool("mail_queue_enabled", true)) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"邮件队列未启用\"}", 0);
		return;
	}
	Mail_RequestRun();
	xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"%s\"}",
		Mail_IsRunning() ? "邮件队列正在后台执行，请稍后刷新列表" : "已触发邮件队列执行，请稍后刷新列表");
}

// 手动重试
void Request_Member_Mail_Retry(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char param[64] = {0};
	int64 taskId = 0;
	str message = NULL;
	bool ok;

	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	if (xsReqQueryValue(objReq, "id", param, sizeof(param)) > 0)
		taskId = Util_ParseI64(param);
	if (taskId <= 0) {
		xvalue* form = JsonParseN(XAdmin_ReqBody(objReq), XAdmin_ReqBodyLen(objReq));
		if (form && xrtValueType(form) == XVALUE_OBJECT)
			taskId = ValueInt(form, "id");
		if (form) xrtValueRelease(form);
	}
	ok = Mail_RetryTask(taskId, &message);
	xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": %s, \"message\": \"%s\"}",
		ok ? "true" : "false", message ? message : "");
	xrtFree(message);
}

// 删除任务
void Request_Member_Mail_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char param[64] = {0};
	int64 taskId = 0;
	str message = NULL;
	bool ok;

	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	if (xsReqQueryValue(objReq, "id", param, sizeof(param)) > 0)
		taskId = Util_ParseI64(param);
	if (taskId <= 0) {
		xvalue* form = JsonParseN(XAdmin_ReqBody(objReq), XAdmin_ReqBodyLen(objReq));
		if (form && xrtValueType(form) == XVALUE_OBJECT)
			taskId = ValueInt(form, "id");
		if (form) xrtValueRelease(form);
	}
	ok = Mail_DeleteTask(taskId, &message);
	xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": %s, \"message\": \"%s\"}",
		ok ? "true" : "false", message ? message : "");
	xrtFree(message);
}

// SMTP 连通测试
void Request_Option_Tool_TestSmtp(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char param[128] = {0};
	const char* toEmail = NULL;
	str message = NULL;
	bool ok;

	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	if (xsReqQueryValue(objReq, "to", param, sizeof(param)) > 0)
		toEmail = param;
	if (!toEmail || !toEmail[0]) {
		xvalue* form = JsonParseN(XAdmin_ReqBody(objReq), XAdmin_ReqBodyLen(objReq));
		if (form && xrtValueType(form) == XVALUE_OBJECT) {
			str email = ValueText(form, "to");
			if (email && email[0]) {
				snprintf(param, sizeof(param), "%s", email);
				toEmail = param;
			}
		}
		if (form) xrtValueRelease(form);
	}
	ok = Mail_TestSmtp(toEmail, &message);
	xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": %s, \"message\": \"%s\"}",
		ok ? "true" : "false", message ? message : "");
	xrtFree(message);
}

// ---- 视图页 ----

void Request_View_Member_Mail(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "member/mail_task.html");
}

void Request_View_Member_Mail_Send(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "member/mail_send.html");
}

#endif /* XADMIN_WITH_SMTP */
