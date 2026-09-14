/* 站内通知路由：前台 /api/v1/notify 六接口 + 后台管理与视图页。 */

static xvalue* NotifyParseIDArray(xvalue* form)
{
	xvalue* ids = xrtValueArray();

	if (!form || xrtValueType(form) != XVALUE_OBJECT) return ids;
	if (xrtValueType(ValueGet(form, "ids")) == XVALUE_ARRAY) {
		xvalue* input = ValueGet(form, "ids");
		size_t i, count = xrtValueCount(input);
		for (i = 0; i < count; i++) {
			int64 id = ValueArrayInt(input, i);
			if (id > 0 && !XAdminIDArrayContainsInt(ids, id))
				xrtValueArrayAppendNew(ids, xrtValueInt(id));
		}
		return ids;
	}
	if (ValueGet(form, "id") != NULL) {
		int64 id = ValueInt(form, "id");
		if (id > 0) xrtValueArrayAppendNew(ids, xrtValueInt(id));
		return ids;
	}
	Notify_ParseIDText(ValueText(form, "ids"), ids);
	return ids;
}

static void NotifyReplyAdminTable(XS_ResponseObject objResp, xvalue* data, int64 count)
{
	xvalue* tbl = xrtValueObject();
	size_t size = 0;
	char* json;

	ValueSetBool(tbl, "result", true);
	ValueSetInt(tbl, "code", 0);
	ValueSetInt(tbl, "count", count);
	ValueSetOwn(tbl, "data", data);
	json = xrtJsonStringify(tbl, false, &size);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, json, 0);
	xrtFree(json);
	xrtValueRelease(tbl);
}

// 站内信管理列表页
void Request_View_Member_Notify(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) == XHTTP_METHOD_GET)
		LoadPage(objResp, 200, HTTP_CT_HTML, "member/notify.html");
	else
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

// 站内信发送页
void Request_View_Member_Notify_Send(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) == XHTTP_METHOD_GET)
		LoadPage(objResp, 200, HTTP_CT_HTML, "member/notify_send.html");
	else
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

// 站内信管理接口（GET 列表 / POST 发送）
void Request_Member_Notify(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost;

	if (xsReqMethodID(objReq) == XHTTP_METHOD_GET) {
		char param[128];
		int64 page = 1, limit = 20, count = 0;
		const char* search = NULL;
		xvalue* data;

		if (xsReqQueryValue(objReq, "page", param, sizeof(param)) > 0)
			page = Util_ParseI64(param);
		if (xsReqQueryValue(objReq, "limit", param, sizeof(param)) > 0)
			limit = Util_ParseI64(param);
		if (xsReqQueryValue(objReq, "search", param, sizeof(param)) > 0)
			search = param;

		data = Notify_AdminList(page, limit, search, &count);
		NotifyReplyAdminTable(objResp, data, count);
		return;
	}

	if (xsReqMethodID(objReq) == XHTTP_METHOD_POST) {
		xvalue* form = JsonParseN(XAdmin_ReqBody(objReq), XAdmin_ReqBodyLen(objReq));
		str title, content, actionUrl, sendType, memberIds, groupIds, message = NULL;
		int64 messageId = 0, recipientCount = 0;
		bool ok;

		if (!form || xrtValueType(form) != XVALUE_OBJECT) {
			if (form) xrtValueRelease(form);
			xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"无效的请求数据\"}", 0);
			return;
		}
		title = ValueText(form, "title");
		content = ValueText(form, "content");
		actionUrl = ValueText(form, "actionUrl");
		sendType = ValueText(form, "sendType");
		memberIds = ValueText(form, "memberIds");
		groupIds = ValueText(form, "groupIds");

		ok = Notify_Send(sendType ? sendType : "users", memberIds, groupIds,
			title, content, actionUrl, ValueInt(objSession, "id"),
			&message, &messageId, &recipientCount);
		xrtValueRelease(form);

		if (ok)
			xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON,
				"{\"result\": true, \"message\": \"%s\", \"data\": {\"id\": %lld, \"recipientCount\": %lld}}",
				Notify_StrOrEmpty(message), (long long)messageId, (long long)recipientCount);
		else
			xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"%s\"}",
				Notify_StrOrEmpty(message));
		if (message) xrtFree(message);
		return;
	}

	LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

// ---- 前台会员接口 ----

void API_Notify_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char param[64];
	int64 page = 1, limit = 20, count = 0;
	xvalue* data;
	size_t size = 0;
	char* json;

	(void)objServer; (void)objHost;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_GET) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	if (xsReqQueryValue(objReq, "page", param, sizeof(param)) > 0)
		page = Util_ParseI64(param);
	if (xsReqQueryValue(objReq, "limit", param, sizeof(param)) > 0)
		limit = Util_ParseI64(param);

	data = Notify_MemberList(ValueInt(objSession, "id"), page, limit, &count);
	json = xrtJsonStringify(data, false, &size);
	xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"success\",\"count\":%lld,\"data\":%s}",
		(long long)count, json ? json : "[]");
	xrtFree(json);
	xrtValueRelease(data);
}

void API_Notify_UnreadCount(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	int64 count;
	(void)objServer; (void)objHost;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_GET) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	count = Notify_MemberUnreadCount(ValueInt(objSession, "id"));
	xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"success\",\"data\":{\"unread\":%lld}}", (long long)count);
}

void API_Notify_Detail(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char param[64];
	int64 id = 0;
	xvalue* row;
	size_t size = 0;
	char* json;

	(void)objServer; (void)objHost;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_GET) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	if (xsReqQueryValue(objReq, "id", param, sizeof(param)) > 0)
		id = Util_ParseI64(param);
	if (id <= 0) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"缺少有效的消息 ID\"}", 0);
		return;
	}
	row = Notify_MemberDetail(ValueInt(objSession, "id"), id);
	if (!row) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":404,\"msg\":\"消息不存在\"}", 0);
		return;
	}
	json = xrtJsonStringify(row, false, &size);
	xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"success\",\"data\":%s}", json ? json : "null");
	xrtFree(json);
	xrtValueRelease(row);
}

void API_Notify_Read(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* form;
	xvalue* ids;
	bool ok;
	(void)objServer; (void)objHost;

	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	form = JsonParseN(XAdmin_ReqBody(objReq), XAdmin_ReqBodyLen(objReq));
	if (!form || xrtValueType(form) != XVALUE_OBJECT) {
		if (form) xrtValueRelease(form);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"无效的请求数据\"}", 0);
		return;
	}
	ids = NotifyParseIDArray(form);
	ok = Notify_MarkRead(ValueInt(objSession, "id"), ids, false);
	xrtValueRelease(ids);
	xrtValueRelease(form);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON,
		ok ? "{\"code\":0,\"msg\":\"success\"}" : "{\"code\":400,\"msg\":\"没有可标记的消息\"}", 0);
}

void API_Notify_ReadAll(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	Notify_MarkRead(ValueInt(objSession, "id"), NULL, true);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"success\"}", 0);
}

void API_Notify_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* form;
	xvalue* ids;
	bool ok;
	(void)objServer; (void)objHost;

	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	form = JsonParseN(XAdmin_ReqBody(objReq), XAdmin_ReqBodyLen(objReq));
	if (!form || xrtValueType(form) != XVALUE_OBJECT) {
		if (form) xrtValueRelease(form);
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":400,\"msg\":\"无效的请求数据\"}", 0);
		return;
	}
	ids = NotifyParseIDArray(form);
	ok = Notify_MemberDelete(ValueInt(objSession, "id"), ids);
	xrtValueRelease(ids);
	xrtValueRelease(form);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON,
		ok ? "{\"code\":0,\"msg\":\"success\"}" : "{\"code\":400,\"msg\":\"没有可删除的消息\"}", 0);
}
