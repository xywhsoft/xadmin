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

	if ( HttpMethodIs(objReq, "GET") ) {
		char sFileName[128];
		char sSource[32];
		xvalue tblForm;
		xvalue tblTypes;
		xvalue tblData;
		xvalue tblRetData;
		bool bOptionSource = FALSE;
		int64 iAuthLevelRequired = 0;
		int64 iAuthLevelUser = 0;

		if ( HttpGetQueryVar(objReq, "source", sSource, sizeof(sSource)) > 0 ) {
			bOptionSource = (strcmp(sSource, "option") == 0);
		}
		if ( HttpGetQueryVar(objReq, "file", sFileName, sizeof(sFileName)) <= 0 ) {
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
		xvoTableSetText(tblRetData, "source", 6, bOptionSource ? "option" : "form", 0, FALSE);
		xvoTableSetValue(tblRetData, "schema", 6, tblForm, TRUE);
		xvoTableSetValue(tblRetData, "fieldTypes", 10, tblTypes, TRUE);
		xvoTableSetValue(tblRetData, "values", 6, tblData, TRUE);

		Form_ReplySuccess(objResp, "������ݻ�ȡ�ɹ�", tblRetData);
		return;
	}

	if ( HttpMethodIs(objReq, "POST") ) {
		xvalue tblBody = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
		str sFileName;
		str sSource;
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
