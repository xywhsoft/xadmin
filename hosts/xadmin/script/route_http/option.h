// 获取配置页面视图
void Request_View_Option(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( HttpMethodIs(objReq, "GET") ) {
		char sFileName[128];
		str sHeader;

		if ( HttpGetQueryVar(objReq, "file", sFileName, sizeof(sFileName)) <= 0 ) {
			memcpy(sFileName, "global.json", sizeof("global.json"));
		}
		if ( !Form_IsValidFileName(sFileName) ) {
			LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
			return;
		}

		sHeader = xrtFormat("Content-Type: text/plain\r\nLocation: /admin/view/form?source=option&file=%s\r\n", sFileName);
		http_reply(objResp, 302, sHeader, "", 0);
		xrtFree(sHeader);
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

// 配置数据接口（正式切换为动态表单版本）
void Request_Option(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;

	if ( HttpMethodIs(objReq, "GET") ) {
		char sFileName[128];
		xvalue tblConfig;
		xvalue tblSchema;
		xvalue tblValues = NULL;
		xvalue tblTypes;
		xvalue tblRetData;
		int64 iAuthLevelRequired;
		int64 iAuthLevelUser = 0;

		if ( HttpGetQueryVar(objReq, "file", sFileName, sizeof(sFileName)) <= 0 ) {
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

		iAuthLevelRequired = xvoTableGetInt(tblConfig, "authLevel", 9);
		if ( objSession && (objSession->Type == XVO_DT_TABLE) ) {
			iAuthLevelUser = xvoTableGetInt(objSession, "authLevel", 9);
		}
		if ( (iAuthLevelRequired > 0) && (iAuthLevelUser < iAuthLevelRequired) ) {
			xvoUnref(tblConfig);
			Form_ReplyError(objResp, "权限不足");
			return;
		}

		tblSchema = Form_CreateSchemaFromOptionConfig(tblConfig, &tblValues);
		xvoUnref(tblConfig);
		if ( tblSchema == NULL ) {
			Form_ReplyError(objResp, "配置文件转表单失败");
			return;
		}

		tblTypes = Form_LoadFieldTypes();
		tblRetData = xvoCreateTable();
		xvoTableSetText(tblRetData, "file", 4, sFileName, 0, FALSE);
		xvoTableSetText(tblRetData, "source", 6, "option", 0, FALSE);
		xvoTableSetValue(tblRetData, "schema", 6, tblSchema, TRUE);
		xvoTableSetValue(tblRetData, "fieldTypes", 10, tblTypes, TRUE);
		xvoTableSetValue(tblRetData, "values", 6, tblValues, TRUE);

		Form_ReplySuccess(objResp, "表单数据获取成功", tblRetData);
		return;
	}

	if ( HttpMethodIs(objReq, "POST") ) {
		xvalue tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		str sFileName;
		xvalue tblData;
		xvalue tblConfig;
		int64 iAuthLevelRequired;
		int64 iAuthLevelUser = 0;
		bool bRet;

		if ( tblBody == NULL ) {
			Form_ReplyError(objResp, "请求数据格式错误");
			return;
		}

		sFileName = xvoTableGetText(tblBody, "file", 4);
		tblData = xvoTableGetValue(tblBody, "data", 4);
		if ( !Form_IsValidFileName(sFileName) ) {
			xvoUnref(tblBody);
			Form_ReplyError(objResp, "非法的表单文件名");
			return;
		}
		if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) ) {
			xvoUnref(tblBody);
			Form_ReplyError(objResp, "缺少 data 参数");
			return;
		}

		tblConfig = Option_LoadFile(sFileName);
		if ( tblConfig == NULL ) {
			xvoUnref(tblBody);
			Form_ReplyError(objResp, "配置文件不存在或解析失败");
			return;
		}
		iAuthLevelRequired = xvoTableGetInt(tblConfig, "authLevel", 9);
		xvoUnref(tblConfig);
		if ( objSession && (objSession->Type == XVO_DT_TABLE) ) {
			iAuthLevelUser = xvoTableGetInt(objSession, "authLevel", 9);
		}
		if ( (iAuthLevelRequired > 0) && (iAuthLevelUser < iAuthLevelRequired) ) {
			xvoUnref(tblBody);
			Form_ReplyError(objResp, "权限不足");
			return;
		}

		bRet = Option_SaveFile(sFileName, tblData);
		xvoUnref(tblBody);
		if ( !bRet ) {
			Form_ReplyError(objResp, "配置保存失败");
			return;
		}

		Form_ReplySuccess(objResp, "表单保存成功", NULL);
		return;
	}

	LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}
