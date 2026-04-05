void Request_View_Form(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( HttpMethodIs(objReq, "GET") ) {
		LoadPage(objResp, 200, HTTP_CT_HTML, "form.html");
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_Form(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( HttpMethodIs(objReq, "GET") ) {
		char sFileName[128];
		char sSource[32];
		xvalue tblForm;
		xvalue tblTypes;
		xvalue tblData;
		xvalue tblRetData;
		bool bOptionSource = FALSE;

		if ( HttpGetQueryVar(objReq, "file", sFileName, sizeof(sFileName)) <= 0 ) {
			memcpy(sFileName, "demo_form.json", sizeof("demo_form.json"));
		}
		if ( HttpGetQueryVar(objReq, "source", sSource, sizeof(sSource)) > 0 ) {
			bOptionSource = (strcmp(sSource, "option") == 0);
		}

		if ( !Form_IsValidFileName(sFileName) ) {
			Form_ReplyError(objResp, "非法的表单文件名");
			return;
		}

		if ( bOptionSource ) {
			xvalue tblConfig = Option_LoadFile(sFileName);
			if ( tblConfig == NULL ) {
				Form_ReplyError(objResp, "配置文件不存在或解析失败");
				return;
			}
			tblForm = Form_CreateSchemaFromOptionConfig(tblConfig, &tblData);
			xvoUnref(tblConfig);
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

			tblData = xvoTableGetValue(tblForm, "demoData", 8);
			if ( tblData == NULL ) {
				tblData = xvoCreateTable();
			} else {
				tblData = xvoCopy(tblData);
			}
		}

		tblTypes = Form_LoadFieldTypes();

		tblRetData = xvoCreateTable();
		xvoTableSetText(tblRetData, "file", 4, sFileName, 0, FALSE);
		xvoTableSetText(tblRetData, "source", 6, bOptionSource ? "option" : "form", 0, FALSE);
		xvoTableSetValue(tblRetData, "schema", 6, tblForm, TRUE);
		xvoTableSetValue(tblRetData, "fieldTypes", 10, tblTypes, TRUE);
		xvoTableSetValue(tblRetData, "values", 6, tblData, TRUE);

		Form_ReplySuccess(objResp, "表单数据获取成功", tblRetData);
		return;
	}

	if ( HttpMethodIs(objReq, "POST") ) {
		xvalue tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		str sFileName;
		str sSource;
		xvalue tblData;
		str sError = NULL;
		bool bRet;

		if ( tblBody == NULL ) {
			Form_ReplyError(objResp, "请求数据格式错误");
			return;
		}

		sFileName = xvoTableGetText(tblBody, "file", 4);
		sSource = xvoTableGetText(tblBody, "source", 6);
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

		if ( (sSource != NULL) && (strcmp(sSource, "option") == 0) ) {
			bRet = Option_SaveFile(sFileName, tblData);
			if ( !bRet ) {
				sError = xrtCopyStr("配置保存失败", 0);
			}
		} else {
			bRet = Form_SaveDemoData(sFileName, tblData, &sError);
		}
		xvoUnref(tblBody);
		if ( !bRet ) {
			if ( sError != NULL ) {
				Form_ReplyError(objResp, (const char*)sError);
			} else {
				Form_ReplyError(objResp, "表单保存失败");
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
