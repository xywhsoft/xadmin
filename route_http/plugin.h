/* 插件管理路由（v1 契约照搬；import/export 为 P3 批次占位）。 */
static void PluginRoute_SendJson(XS_ResponseObject objResp, xvalue* tblRet)
{
	size_t iSize = 0;
	char* sJson = xrtJsonStringify(tblRet, false, &iSize);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
}

static void PluginRoute_SendResult(XS_ResponseObject objResp, bool bResult, const char* sMessage)
{
	xvalue* tblRet = ValueObject();
	ValueSetBool(tblRet, "result", bResult);
	ValueSetText(tblRet, "message", (str)(sMessage ? sMessage : ""));
	PluginRoute_SendJson(objResp, tblRet);
	xrtValueRelease(tblRet);
}

void Request_Plugin_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* arrList = ValueArray();
	xvalue* tblRet;
	size_t i;
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;

	for (i = 0; i < G_PluginCount; i++) {
		PluginInstance* inst = &G_Plugins[i];
		xvalue* row = ValueObject();
		sqlite3_stmt* stmt = NULL;
		int bEnabled = 0, bInstalled = 0, iGen = 0;
		char sStatus[32];
		snprintf(sStatus, sizeof(sStatus), "%s", "discovered");
		if (sqlite3_prepare_v2(G_DB, "SELECT enabled, installed, status, active_generation FROM plugin_runtime WHERE xid=?;", -1, &stmt, NULL) == SQLITE_OK) {
			Plugin_BindText(stmt, 1, inst->xid);
			if (sqlite3_step(stmt) == SQLITE_ROW) {
				const char* sDbStatus;
				bEnabled = sqlite3_column_int(stmt, 0);
				bInstalled = sqlite3_column_int(stmt, 1);
				sDbStatus = (const char*)sqlite3_column_text(stmt, 2);
				iGen = sqlite3_column_int(stmt, 3);
				/* finalize 前复制：列文本随语句终结失效 */
				if (sDbStatus) snprintf(sStatus, sizeof(sStatus), "%s", sDbStatus);
			}
			sqlite3_finalize(stmt);
		}
		ValueSetText(row, "name", inst->xid);
		ValueSetText(row, "title", ValueText(inst->manifest, "title"));
		ValueSetText(row, "desc", ValueText(inst->manifest, "description"));
		ValueSetText(row, "author", ValueText(inst->manifest, "author"));
		ValueSetText(row, "version", ValueText(inst->manifest, "version"));
		ValueSetText(row, "kind", ValueText(inst->manifest, "kind"));
		ValueSetText(row, "status", sStatus);
		ValueSetBool(row, "enabled", bEnabled != 0);
		ValueSetBool(row, "installed", bInstalled != 0);
		ValueSetInt(row, "generation", iGen);
		ValueSetText(row, "dataPath", inst->dataPath);
		ValueSetText(row, "path", inst->rootPath);
		ValueArrayOwn(arrList, row);
	}

	tblRet = ValueObject();
	ValueSetInt(tblRet, "code", 0);
	ValueSetText(tblRet, "msg", "");
	ValueSetInt(tblRet, "count", ValueCount(arrList));
	ValueSetOwn(tblRet, "data", arrList);
	PluginRoute_SendJson(objResp, tblRet);
	xrtValueRelease(tblRet);
}

void Request_Plugin_Get(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char sName[PLUGIN_NAME_MAX] = {0};
	PluginInstance* inst;
	xvalue* tblData;
	xvalue* tblRet;
	(void)objServer; (void)objHost; (void)objSession;

	xsReqQueryValue(objReq, "name", sName, sizeof(sName));
	inst = Plugin_Find(sName);
	if (!inst) {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Plugin not found\"}", 0);
		return;
	}
	tblData = ValueObject();
	ValueSetText(tblData, "name", inst->xid);
	ValueSetText(tblData, "title", ValueText(inst->manifest, "title"));
	ValueSetText(tblData, "desc", ValueText(inst->manifest, "description"));
	ValueSetText(tblData, "author", ValueText(inst->manifest, "author"));
	ValueSetText(tblData, "version", ValueText(inst->manifest, "version"));
	ValueSetText(tblData, "kind", ValueText(inst->manifest, "kind"));
	ValueSetText(tblData, "rootPath", inst->rootPath);
	ValueSetText(tblData, "dataPath", inst->dataPath);
	ValueSetText(tblData, "dbPath", inst->dbPath);
	if (inst->config) {
		xrtValueRetain(inst->config);
		ValueSetOwn(tblData, "config", inst->config);
	}
	tblRet = ValueObject();
	ValueSetBool(tblRet, "result", true);
	ValueSetOwn(tblRet, "data", tblData);
	PluginRoute_SendJson(objResp, tblRet);
	xrtValueRelease(tblRet);
}

void Request_Plugin_Enable(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char sName[PLUGIN_NAME_MAX] = {0};
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		PluginRoute_SendResult(objResp, false, "请求方法不允许");
		return;
	}
	xsReqQueryValue(objReq, "name", sName, sizeof(sName));
	if (sName[0] == '\0') {
		xvalue* tblBody = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		str s = ValueText(tblBody, "name");
		if (s) snprintf(sName, sizeof(sName), "%s", s);
		xrtValueRelease(tblBody);
	}
	if (!Plugin_XidValid(sName)) {
		PluginRoute_SendResult(objResp, false, "缺少插件 xid");
		return;
	}
	PluginRoute_SendResult(objResp, PluginHost_SetEnabled(sName, true), "插件已启用");
}

void Request_Plugin_Disable(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char sName[PLUGIN_NAME_MAX] = {0};
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		PluginRoute_SendResult(objResp, false, "请求方法不允许");
		return;
	}
	xsReqQueryValue(objReq, "name", sName, sizeof(sName));
	if (sName[0] == '\0') {
		xvalue* tblBody = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		str s = ValueText(tblBody, "name");
		if (s) snprintf(sName, sizeof(sName), "%s", s);
		xrtValueRelease(tblBody);
	}
	if (!Plugin_XidValid(sName)) {
		PluginRoute_SendResult(objResp, false, "缺少插件 xid");
		return;
	}
	PluginRoute_SendResult(objResp, PluginHost_SetEnabled(sName, false), "插件已禁用");
}

void Request_Plugin_Reload(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char sName[PLUGIN_NAME_MAX] = {0};
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		PluginRoute_SendResult(objResp, false, "请求方法不允许");
		return;
	}
	xsReqQueryValue(objReq, "name", sName, sizeof(sName));
	if (sName[0] == '\0') {
		xvalue* tblBody = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		str s = ValueText(tblBody, "name");
		if (s) snprintf(sName, sizeof(sName), "%s", s);
		xrtValueRelease(tblBody);
	}
	if (!Plugin_XidValid(sName)) {
		PluginRoute_SendResult(objResp, false, "缺少插件 xid");
		return;
	}
	PluginRoute_SendResult(objResp, PluginHost_Reload(sName), "插件已重载");
}

void Request_Plugin_Settings(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	char sName[PLUGIN_NAME_MAX] = {0};
	xvalue* tblBody;
	str sNameValue;
	PluginInstance* inst;
	char* sPath;
	xvalue* newCfg;
	(void)objServer; (void)objHost; (void)objSession;

	if (xsReqMethodID(objReq) != XHTTP_METHOD_POST) {
		PluginRoute_SendResult(objResp, false, "请求方法不允许");
		return;
	}
	tblBody = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if (xrtValueType(tblBody) != XVALUE_OBJECT) {
		PluginRoute_SendResult(objResp, false, "请求数据格式错误");
		xrtValueRelease(tblBody);
		return;
	}
	sNameValue = ValueText(tblBody, "name");
	inst = Plugin_Find(sNameValue);
	if (!inst || !sNameValue || !Plugin_XidValid(sNameValue)) {
		PluginRoute_SendResult(objResp, false, "插件不存在");
		xrtValueRelease(tblBody);
		return;
	}
	{
		xvalue* tblConfig = ValueGet(tblBody, "config");
		if (!tblConfig || xrtValueType(tblConfig) != XVALUE_OBJECT) {
			PluginRoute_SendResult(objResp, false, "缺少 config 参数");
			xrtValueRelease(tblBody);
			return;
		}
		sPath = xrtPathJoin(OptionPath, xrtFormat("plugin/%s.json", inst->xid));
		if (!sPath || !JsonWriteFile(sPath, tblConfig, true)) {
			PluginRoute_SendResult(objResp, false, "配置写入失败");
			if (sPath) xrtFree(sPath);
			xrtValueRelease(tblBody);
			return;
		}
		xrtFree(sPath);
	}
	xrtValueRelease(tblBody);
	if (inst->started) {
		xvalue* old = inst->config;
		newCfg = Plugin_LoadConfig(inst);
		inst->config = newCfg;
		if (inst->desc && inst->desc->OnConfigChanged)
			inst->desc->OnConfigChanged(inst, newCfg);
		if (old) CacheRetire(old);
	}
	PluginRoute_SendResult(objResp, true, "配置已保存");
}

void Request_Plugin_Export(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	PluginRoute_SendResult(objResp, false, "导入导出于后续批次接入");
}

void Request_Plugin_Import(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	PluginRoute_SendResult(objResp, false, "导入导出于后续批次接入");
}

void Request_View_Plugin_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) == XHTTP_METHOD_GET) LoadPage(objResp, 200, HTTP_CT_HTML, "plugin/list.html");
	else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}

void Request_View_Plugin_Store(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objSession;
	if (xsReqMethodID(objReq) == XHTTP_METHOD_GET) LoadPage(objResp, 200, HTTP_CT_HTML, "plugin/store.html");
	else LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}


// 表单演示页
void Request_View_Template_FormDemo(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	LoadPage(objResp, 200, HTTP_CT_HTML, "template/form_demo.html");
}
