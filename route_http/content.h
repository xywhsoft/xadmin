/* 内容模型系统 HTTP 面（v1 route_http/content.h 对齐）。
 * generate 为阶段 2（生成器+模板）占位，其余模型 CRUD/修订/体检/能力包全量。 */

static void Content_SendData(XS_ResponseObject objResp, bool result, const char* message, xvalue* data)
{
	xvalue* ret = ValueObject();
	ValueSetBool(ret, "result", result);
	if (message) ValueSetText(ret, "message", message);
	if (data) ValueSetOwn(ret, "data", data);
	PluginRoute_SendJson(objResp, ret);
}

static xvalue* Content_ReadBodyJson(XS_RequestObject objReq)
{
	return JsonParseN(XAdmin_ReqBody(objReq), XAdmin_ReqBodyLen(objReq));
}

void Request_View_Content_Index(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if (xsReqMethodID(objReq) == XHTTP_METHOD_GET) LoadPage(objResp, 200, HTTP_CT_HTML, "content/index.html");
	else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

void Request_View_Content_Editor(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if (xsReqMethodID(objReq) == XHTTP_METHOD_GET) LoadPage(objResp, 200, HTTP_CT_HTML, "content/editor.html");
	else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

void Request_View_Content_Packs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if (xsReqMethodID(objReq) == XHTTP_METHOD_GET) LoadPage(objResp, 200, HTTP_CT_HTML, "content/packs.html");
	else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

void Request_View_Content_PackStore(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if (xsReqMethodID(objReq) == XHTTP_METHOD_GET) LoadPage(objResp, 200, HTTP_CT_HTML, "content/pack_store.html");
	else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

void Request_Content_Types(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	int page = 1, limit = 20;
	char buf[32];
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_GET) { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); return; }
	if (xsReqQueryValue(objReq, "page", buf, sizeof(buf)) && buf[0]) page = atoi(buf);
	if (xsReqQueryValue(objReq, "limit", buf, sizeof(buf)) && buf[0]) limit = atoi(buf);
	Content_SendData(objResp, true, NULL, Content_ListModels(page, limit));
}

void Request_Content_Type(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char xid[128] = {0};
	xvalue* model;
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_GET) { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); return; }
	xsReqQueryValue(objReq, "xid", xid, sizeof(xid));
	model = xid[0] ? Content_GetModelByXid(xid) : NULL;
	if (!model) { PluginRoute_SendResult(objResp, false, "模型不存在"); return; }
	Content_SendData(objResp, true, NULL, model);
}

void Request_Content_Save(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* spec; xvalue* data; str error = NULL;
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); return; }
	spec = Content_ReadBodyJson(objReq);
	if (!spec || xrtValueType(spec) != XVALUE_OBJECT) {
		xrtValueRelease(spec);
		PluginRoute_SendResult(objResp, false, "请求体不是有效的 JSON 对象");
		return;
	}
	data = Content_SaveModelSpec(spec, &error);
	xrtValueRelease(spec);
	if (!data) { PluginRoute_SendResult(objResp, false, error ? error : "保存失败"); xrtFree(error); return; }
	xrtFree(error);
	Content_SendData(objResp, true, "模型已保存", data);
}

void Request_Content_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* body; str xid; str error = NULL; bool ok;
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); return; }
	body = Content_ReadBodyJson(objReq);
	xid = ValueText(body, "xid");
	ok = Content_DeleteModelByXid(xid, &error);
	xrtValueRelease(body);
	if (!ok) { PluginRoute_SendResult(objResp, false, error ? error : "删除失败"); xrtFree(error); return; }
	xrtFree(error);
	PluginRoute_SendResult(objResp, true, "模型已删除");
}

void Request_Content_Revisions(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char xid[128] = {0};
	int modelId = 0;
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_GET) { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); return; }
	xsReqQueryValue(objReq, "xid", xid, sizeof(xid));
	if (!Content_FindModelIdAndRevision(xid, &modelId, NULL)) {
		PluginRoute_SendResult(objResp, false, "模型不存在");
		return;
	}
	Content_SendData(objResp, true, NULL, Content_ListRevisions(modelId));
}

void Request_Content_Generations(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char xid[128] = {0};
	int modelId = 0;
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_GET) { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); return; }
	xsReqQueryValue(objReq, "xid", xid, sizeof(xid));
	if (!Content_FindModelIdAndRevision(xid, &modelId, NULL)) {
		PluginRoute_SendResult(objResp, false, "模型不存在");
		return;
	}
	Content_SendData(objResp, true, NULL, Content_ListGenerations(modelId));
}

void Request_Content_Advisor(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* spec; xvalue* advisor;
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); return; }
	spec = Content_ReadBodyJson(objReq);
	advisor = Content_BuildAdvisor(spec);
	xrtValueRelease(spec);
	Content_SendData(objResp, true, NULL, advisor);
}

void Request_Content_Generate(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST && xsReqMethodID(objReq) != XHTTP_METHOD_GET) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	{
		char xid[128] = {0};
		xvalue* body = NULL;
		str sXid = NULL;
		xvalue* data; str error = NULL;
		if (xsReqMethodID(objReq) == XHTTP_METHOD_POST) {
			body = Content_ReadBodyJson(objReq);
			sXid = ValueText(body, "xid");
		}
		if (!sXid) xsReqQueryValue(objReq, "xid", xid, sizeof(xid));
		data = Content_GeneratePluginForModel(sXid ? sXid : xid, &error);
		xrtValueRelease(body);
		if (!data) {
			char msg[256];
			snprintf(msg, sizeof(msg), "%s", error ? error : "生成失败");
			xrtFree(error);
			PluginRoute_SendResult(objResp, false, msg);
			return;
		}
		xrtFree(error);
		Content_SendData(objResp, true, "插件已生成，请在插件管理中启用", data);
	}
}

void Request_Content_Packs(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_GET) { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); return; }
	Content_SendData(objResp, true, NULL, ContentPack_ListAll());
}

void Request_Content_Pack(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char packId[128] = {0};
	xvalue* detail;
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_GET) { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); return; }
	xsReqQueryValue(objReq, "packId", packId, sizeof(packId));
	detail = packId[0] ? ContentPack_GetDetail(packId) : NULL;
	if (!detail) { PluginRoute_SendResult(objResp, false, "能力包不存在"); return; }
	Content_SendData(objResp, true, NULL, detail);
}

void Request_Content_Pack_Options(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* body; str packId; xvalue* options; char* optionsJson = NULL; bool ok;
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); return; }
	body = Content_ReadBodyJson(objReq);
	packId = ValueText(body, "packId");
	options = ValueGet(body, "options");
	if (!packId || !packId[0] || !options) {
		xrtValueRelease(body);
		PluginRoute_SendResult(objResp, false, "缺少 packId 或 options");
		return;
	}
	{
		size_t size = 0;
		optionsJson = xrtJsonStringify(options, false, &size);
	}
	/* packId 借用 body 视图，须在释放前使用 */
	ok = optionsJson ? ContentPack_SaveOptions(packId, optionsJson) : false;
	xrtValueRelease(body);
	xrtFree(optionsJson);
	if (!ok) { PluginRoute_SendResult(objResp, false, "保存能力包选项失败"); return; }
	PluginRoute_SendResult(objResp, true, "能力包选项已保存");
}

void Request_Content_Templates(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	static const char* names[] = {"plugin.admin.html.tpl", "plugin.contracts.json.tpl", "plugin.main.c.tpl", "plugin.managed.json.tpl", "plugin.public.html.tpl"};
	xvalue* arr = ValueArray();
	int i;
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_GET) { LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html"); return; }
	/* v1 装饰性接口：返回固定模板名清单，保持编辑器页兼容 */
	for (i = 0; i < 5; i++) {
		xvalue* item = ValueObject();
		ValueSetText(item, "name", names[i]);
		ValueSetBool(item, "builtin", true);
		ValueArrayOwn(arr, item);
	}
	Content_SendData(objResp, true, NULL, arr);
}
