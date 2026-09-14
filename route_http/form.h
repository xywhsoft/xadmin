/* v1 动态表单页与数据接口。GET 返回 schema/fieldTypes/values；POST 保存：
 * option 源经配置写入（含 authLevel 检查），其余走演示数据校验落盘。
 * 契约与错误文案保持 v1。 */
void Request_View_Form(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objReq;
	(void)objSession;

	LoadPage(objResp, 200, HTTP_CT_HTML, "form.html");
}

void Request_Form(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer;
	(void)objHost;

	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		char sFileName[128];
		char sSource[32];
		xvalue* tblForm;
		xvalue* tblTypes;
		xvalue* tblData;
		xvalue* tblRetData;
		bool bOptionSource = false;
		int64 iAuthLevelRequired = 0;
		int64 iAuthLevelUser = 0;

		if ( xsReqQueryValue(objReq, "source", sSource, sizeof(sSource)) > 0 ) {
			bOptionSource = (strcmp(sSource, "option") == 0);
		}
		if ( xsReqQueryValue(objReq, "file", sFileName, sizeof(sFileName)) <= 0 ) {
			if ( bOptionSource ) {
				memcpy(sFileName, "global.json", sizeof("global.json"));
			} else {
				memcpy(sFileName, "demo_form.json", sizeof("demo_form.json"));
			}
		}

		if ( !Form_IsValidFileName(sFileName) ) {
			Form_ReplyError(objResp, "非法的表单文件名");
			return;
		}

		if ( bOptionSource ) {
			xvalue* tblConfig = Option_LoadFile(sFileName);
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
			tblForm = Form_CreateSchemaFromOptionConfig(tblConfig, &tblData);
			xrtValueRelease(tblConfig);
			if ( tblForm == NULL ) {
				Form_ReplyError(objResp, "配置文件转表单失败");
				return;
			}
		} else {
			tblForm = Form_LoadFile(sFileName);
			if ( tblForm == NULL ) {
				Form_ReplyError(objResp, "表单文件不存在或解析失败");
				return;
			}

			tblData = ValueGet(tblForm, "demoData");
			if ( tblData == NULL ) {
				tblData = ValueObject();
			} else {
				tblData = xrtValueDeepClone(tblData);
			}
		}

		tblTypes = Form_LoadFieldTypes();

		tblRetData = ValueObject();
		ValueSetText(tblRetData, "file", sFileName);
		ValueSetText(tblRetData, "source", bOptionSource ? "option" : "form");
		ValueSetOwn(tblRetData, "schema", tblForm);
		ValueSetOwn(tblRetData, "fieldTypes", tblTypes);
		ValueSetOwn(tblRetData, "values", tblData);

		Form_ReplySuccess(objResp, "表单数据获取成功", tblRetData);
		return;
	}

	if ( (xsReqMethodID(objReq) == XHTTP_METHOD_POST) ) {
		xvalue* tblBody = JsonParseN((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		str sFileName;
		str sSource;
		xvalue* tblData;
		xvalue* tblConfig = NULL;
		str sError = NULL;
		bool bRet;
		int64 iAuthLevelRequired = 0;
		int64 iAuthLevelUser = 0;

		if ( tblBody == NULL ) {
			Form_ReplyError(objResp, "请求数据格式错误");
			return;
		}

		sFileName = ValueText(tblBody, "file");
		sSource = ValueText(tblBody, "source");
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

		if ( (sSource != NULL) && (strcmp(sSource, "option") == 0) ) {
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
			if ( !bRet ) {
				sError = xrtStrDup("配置保存失败");
			}
		} else {
			bRet = Form_SaveDemoData(sFileName, tblData, &sError);
		}
		xrtValueRelease(tblBody);
		if ( !bRet ) {
			if ( sError != NULL ) {
				Form_ReplyError(objResp, (const char*)sError);
			} else {
				Form_ReplyError(objResp, "表单数据保存失败");
			}
			if ( sError ) xrtFree(sError);
			return;
		}
		if ( sError ) xrtFree(sError);

		Form_ReplySuccess(objResp, "表单保存成功", NULL);
		return;
	}

	LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}
