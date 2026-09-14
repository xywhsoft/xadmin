/* v1 配置编辑视图与数据接口（正式切换为动态表单版本）。
 * 视图 302 到表单页；GET 返回配置转成的 schema/values；POST 经
 * Option_SaveFile 落盘并重建缓存（含 cp_url 安全入口校验）。 */
// 获取配置页面视图
void Request_View_Option(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		char sFileName[128];
		str sHeader;

		if ( xsReqQueryValue(objReq, "file", sFileName, sizeof(sFileName)) <= 0 ) {
			memcpy(sFileName, "global.json", sizeof("global.json"));
		}
		if ( !Form_IsValidFileName(sFileName) ) {
			LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
			return;
		}

		sHeader = xrtFormat("Content-Type: text/plain\r\nLocation: /admin/view/form?source=option&file=%s\r\n", sFileName);
		xsHttpReplyAuto(objResp, 302, sHeader, "", 0);
		xrtFree(sHeader);
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

// 配置数据接口（正式切换为动态表单版本）
void Request_Option(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer;
	(void)objHost;

	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		char sFileName[128];
		xvalue* tblConfig;
		xvalue* tblSchema;
		xvalue* tblValues = NULL;
		xvalue* tblTypes;
		xvalue* tblRetData;
		int64 iAuthLevelRequired;
		int64 iAuthLevelUser = 0;

		if ( xsReqQueryValue(objReq, "file", sFileName, sizeof(sFileName)) <= 0 ) {
			memcpy(sFileName, "global.json", sizeof("global.json"));
		}
		if ( !Form_IsValidFileName(sFileName) ) {
			Form_ReplyError(objResp, "非法的表单文件名");
			return;
		}

		tblConfig = Option_LoadFile(sFileName);
		if ( tblConfig == NULL ) {
			Form_ReplyError(objResp, "配置文件不存在或解析失败");
			return;
		}

		iAuthLevelRequired = ValueInt(tblConfig, "authLevel");
		if ( objSession && (xrtValueType(objSession) == XVALUE_OBJECT) ) {
			iAuthLevelUser = ValueInt(objSession, "authLevel");
		}
		if ( (iAuthLevelRequired > 0) && (iAuthLevelUser < iAuthLevelRequired) ) {
			xrtValueRelease(tblConfig);
			Form_ReplyError(objResp, "权限不足");
			return;
		}

		tblSchema = Form_CreateSchemaFromOptionConfig(tblConfig, &tblValues);
		xrtValueRelease(tblConfig);
		if ( tblSchema == NULL ) {
			Form_ReplyError(objResp, "配置文件转表单失败");
			return;
		}

		tblTypes = Form_LoadFieldTypes();
		tblRetData = ValueObject();
		ValueSetText(tblRetData, "file", sFileName);
		ValueSetText(tblRetData, "source", "option");
		ValueSetOwn(tblRetData, "schema", tblSchema);
		ValueSetOwn(tblRetData, "fieldTypes", tblTypes);
		ValueSetOwn(tblRetData, "values", tblValues);

		Form_ReplySuccess(objResp, "表单数据获取成功", tblRetData);
		return;
	}

	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		xvalue* tblBody = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		str sFileName;
		xvalue* tblData;
		xvalue* tblConfig;
		int64 iAuthLevelRequired;
		int64 iAuthLevelUser = 0;
		bool bRet;

		if ( tblBody == NULL ) {
			Form_ReplyError(objResp, "请求数据格式错误");
			return;
		}

		sFileName = ValueText(tblBody, "file");
		tblData = ValueGet(tblBody, "data");
		if ( !Form_IsValidFileName(sFileName) ) {
			xrtValueRelease(tblBody);
			Form_ReplyError(objResp, "非法的表单文件名");
			return;
		}
		if ( (tblData == NULL) || (xrtValueType(tblData) != XVALUE_OBJECT) ) {
			xrtValueRelease(tblBody);
			Form_ReplyError(objResp, "缺少 data 参数");
			return;
		}

		tblConfig = Option_LoadFile(sFileName);
		if ( tblConfig == NULL ) {
			xrtValueRelease(tblBody);
			Form_ReplyError(objResp, "配置文件不存在或解析失败");
			return;
		}
		iAuthLevelRequired = ValueInt(tblConfig, "authLevel");
		xrtValueRelease(tblConfig);
		if ( objSession && (xrtValueType(objSession) == XVALUE_OBJECT) ) {
			iAuthLevelUser = ValueInt(objSession, "authLevel");
		}
		if ( (iAuthLevelRequired > 0) && (iAuthLevelUser < iAuthLevelRequired) ) {
			xrtValueRelease(tblBody);
			Form_ReplyError(objResp, "权限不足");
			return;
		}

		bRet = Option_SaveFile(sFileName, tblData);
		xrtValueRelease(tblBody);
		if ( !bRet ) {
			Form_ReplyError(objResp, "配置保存失败");
			return;
		}

		Form_ReplySuccess(objResp, "表单保存成功", NULL);
		return;
	}

	LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}
