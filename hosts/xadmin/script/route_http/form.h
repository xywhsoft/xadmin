void Request_View_Form(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		LoadPage(objResp, 200, HTTP_CT_HTML, "form.html");
	} else {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
	}
}

void Request_Form(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer;
	(void)objHost;

	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_GET) ) {
		char sFileName[128];
		char sSource[32];
		char sPluginXid[128];
		xvalue tblForm;
		xvalue tblTypes;
		xvalue tblData;
		xvalue tblRetData;
		bool bOptionSource = FALSE;
		bool bPluginOptionSource = FALSE;
		int64 iAuthLevelRequired = 0;
		int64 iAuthLevelUser = 0;

		sPluginXid[0] = '\0';
		if ( xsReqQueryValue(objReq, "source", sSource, sizeof(sSource)) > 0 ) {
			bOptionSource = (strcmp(sSource, "option") == 0);
			bPluginOptionSource = (strcmp(sSource, "plugin-option") == 0);
		}
		xsReqQueryValue(objReq, "plugin", sPluginXid, sizeof(sPluginXid));
		if ( xsReqQueryValue(objReq, "file", sFileName, sizeof(sFileName)) <= 0 ) {
			if ( bOptionSource ) {
				memcpy(sFileName, "global.json", sizeof("global.json"));
			} else {
				memcpy(sFileName, "demo_form.json", sizeof("demo_form.json"));
			}
		}

		if ( !Form_IsValidFileName(sFileName) ) {
			Form_ReplyError(objResp, "�Ƿ��ı���ļ���");
			return;
		}

		if ( bOptionSource ) {
			xvalue tblConfig = Option_LoadFile(sFileName);
			if ( tblConfig == NULL ) {
				Form_ReplyError(objResp, "�����ļ������ڻ����ʧ��");
				return;
			}
			iAuthLevelRequired = xvoTableGetInt(tblConfig, "authLevel", 9);
			if ( objSession && (objSession->Type == XVO_DT_TABLE) ) {
				iAuthLevelUser = xvoTableGetInt(objSession, "authLevel", 9);
			}
			if ( (iAuthLevelRequired > 0) && (iAuthLevelUser < iAuthLevelRequired) ) {
				xvoUnref(tblConfig);
				Form_ReplyError(objResp, "Ȩ�޲���");
				return;
			}
			tblForm = Form_CreateSchemaFromOptionConfig(tblConfig, &tblData);
			xvoUnref(tblConfig);
			if ( tblForm == NULL ) {
				Form_ReplyError(objResp, "�����ļ�ת���ʧ��");
				return;
			}
		} else if ( bPluginOptionSource ) {
			xvalue tblConfig;
			if ( !PS_ResourceIsSafeRelativePath(sPluginXid) ) {
				Form_ReplyError(objResp, "invalid plugin");
				return;
			}
			tblConfig = PS_PluginOptionLoadFile(sPluginXid, sFileName);
			if ( tblConfig == NULL ) {
				Form_ReplyError(objResp, "plugin option file not found");
				return;
			}
			iAuthLevelRequired = xvoTableGetInt(tblConfig, "authLevel", 9);
			if ( objSession && (objSession->Type == XVO_DT_TABLE) ) {
				iAuthLevelUser = xvoTableGetInt(objSession, "authLevel", 9);
			}
			if ( (iAuthLevelRequired > 0) && (iAuthLevelUser < iAuthLevelRequired) ) {
				xvoUnref(tblConfig);
				Form_ReplyError(objResp, "permission denied");
				return;
			}
			tblForm = Form_CreateSchemaFromOptionConfig(tblConfig, &tblData);
			xvoUnref(tblConfig);
			if ( tblForm == NULL ) {
				Form_ReplyError(objResp, "plugin option schema invalid");
				return;
			}
		} else {
			tblForm = Form_LoadFile(sFileName);
			if ( tblForm == NULL ) {
				Form_ReplyError(objResp, "����ļ������ڻ����ʧ��");
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
		xvoTableSetText(tblRetData, "source", 6, bPluginOptionSource ? "plugin-option" : (bOptionSource ? "option" : "form"), 0, FALSE);
		if ( bPluginOptionSource ) {
			xvoTableSetText(tblRetData, "plugin", 6, sPluginXid, 0, FALSE);
		}
		xvoTableSetValue(tblRetData, "schema", 6, tblForm, TRUE);
		xvoTableSetValue(tblRetData, "fieldTypes", 10, tblTypes, TRUE);
		xvoTableSetValue(tblRetData, "values", 6, tblData, TRUE);

		Form_ReplySuccess(objResp, "������ݻ�ȡ�ɹ�", tblRetData);
		return;
	}

	if ( (xsReqMethodID(objReq) == XHTTPD_METHOD_POST) ) {
		xvalue tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		str sFileName;
		str sSource;
		str sPluginXid;
		xvalue tblData;
		xvalue tblConfig = NULL;
		str sError = NULL;
		bool bRet;
		int64 iAuthLevelRequired = 0;
		int64 iAuthLevelUser = 0;

		if ( tblBody == NULL ) {
			Form_ReplyError(objResp, "�������ݸ�ʽ����");
			return;
		}

		sFileName = xvoTableGetText(tblBody, "file", 4);
		sSource = xvoTableGetText(tblBody, "source", 6);
		sPluginXid = xvoTableGetText(tblBody, "plugin", 6);
		tblData = xvoTableGetValue(tblBody, "data", 4);
		if ( !Form_IsValidFileName(sFileName) ) {
			xvoUnref(tblBody);
			Form_ReplyError(objResp, "�Ƿ��ı���ļ���");
			return;
		}
		if ( (tblData == NULL) || (xvoType(tblData) != XVO_DT_TABLE) ) {
			xvoUnref(tblBody);
			Form_ReplyError(objResp, "ȱ�� data ����");
			return;
		}

		if ( (sSource != NULL) && (strcmp(sSource, "option") == 0) ) {
			tblConfig = Option_LoadFile(sFileName);
			if ( tblConfig == NULL ) {
				xvoUnref(tblBody);
				Form_ReplyError(objResp, "�����ļ������ڻ����ʧ��");
				return;
			}
			iAuthLevelRequired = xvoTableGetInt(tblConfig, "authLevel", 9);
			xvoUnref(tblConfig);
			if ( objSession && (objSession->Type == XVO_DT_TABLE) ) {
				iAuthLevelUser = xvoTableGetInt(objSession, "authLevel", 9);
			}
			if ( (iAuthLevelRequired > 0) && (iAuthLevelUser < iAuthLevelRequired) ) {
				xvoUnref(tblBody);
				Form_ReplyError(objResp, "Ȩ�޲���");
				return;
			}
			bRet = Option_SaveFile(sFileName, tblData);
			if ( !bRet ) {
				sError = xrtCopyStr("���ñ���ʧ��", 0);
			}
		} else if ( (sSource != NULL) && (strcmp(sSource, "plugin-option") == 0) ) {
			if ( !PS_ResourceIsSafeRelativePath(sPluginXid) ) {
				xvoUnref(tblBody);
				Form_ReplyError(objResp, "invalid plugin");
				return;
			}
			tblConfig = PS_PluginOptionLoadFile(sPluginXid, sFileName);
			if ( tblConfig == NULL ) {
				xvoUnref(tblBody);
				Form_ReplyError(objResp, "plugin option file not found");
				return;
			}
			iAuthLevelRequired = xvoTableGetInt(tblConfig, "authLevel", 9);
			xvoUnref(tblConfig);
			if ( objSession && (objSession->Type == XVO_DT_TABLE) ) {
				iAuthLevelUser = xvoTableGetInt(objSession, "authLevel", 9);
			}
			if ( (iAuthLevelRequired > 0) && (iAuthLevelUser < iAuthLevelRequired) ) {
				xvoUnref(tblBody);
				Form_ReplyError(objResp, "permission denied");
				return;
			}
			bRet = PS_PluginOptionSaveFile(sPluginXid, sFileName, tblData);
			if ( !bRet ) {
				sError = xrtCopyStr("save plugin option failed", 0);
			}
		} else {
			bRet = Form_SaveDemoData(sFileName, tblData, &sError);
		}
		xvoUnref(tblBody);
		if ( !bRet ) {
			if ( sError != NULL ) {
				Form_ReplyError(objResp, (const char*)sError);
			} else {
				Form_ReplyError(objResp, "�������ʧ��");
			}
			if ( sError ) xrtFree(sError);
			return;
		}
		if ( sError ) xrtFree(sError);

		Form_ReplySuccess(objResp, "�������ɹ�", NULL);
		return;
	}

	LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
}
