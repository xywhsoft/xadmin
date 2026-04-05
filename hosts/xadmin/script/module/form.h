static str G_FormPath = NULL;
static str G_FormInstallPath = NULL;

static bool Form_IsSpaceChar(char c)
{
	return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

static bool Form_HasNonSpaceText(const char* sText)
{
	if ( sText == NULL ) {
		return FALSE;
	}

	while ( *sText ) {
		if ( !Form_IsSpaceChar(*sText) ) {
			return TRUE;
		}
		sText++;
	}

	return FALSE;
}

static bool Form_IsValidFileName(const char* sFileName)
{
	size_t iLen;
	size_t iBaseLen;
	size_t i;

	if ( (sFileName == NULL) || (sFileName[0] == '\0') ) {
		return FALSE;
	}

	iLen = strlen(sFileName);
	if ( (iLen <= 5) || (strcmp(sFileName + iLen - 5, ".json") != 0) ) {
		return FALSE;
	}

	iBaseLen = iLen - 5;
	for ( i = 0; i < iBaseLen; i++ ) {
		char c = sFileName[i];
		bool bOK = ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '_') || (c == '-');
		if ( !bOK ) {
			return FALSE;
		}
	}

	return TRUE;
}

static str Form_BuildFilePath(const char* sFileName)
{
	if ( (G_FormPath == NULL) || !Form_IsValidFileName(sFileName) ) {
		return NULL;
	}
	return xrtPathJoin(2, G_FormPath, (str)sFileName);
}

static xvalue Form_LoadFile(const char* sFileName)
{
	str sFilePath;
	xvalue tblForm;

	if ( !Form_IsValidFileName(sFileName) ) {
		return NULL;
	}

	sFilePath = Form_BuildFilePath(sFileName);
	if ( sFilePath == NULL ) {
		return NULL;
	}

	tblForm = xrtParseJSON_File(sFilePath);
	xrtFree(sFilePath);
	return tblForm;
}

static xvalue Form_LoadFieldTypes()
{
	xvalue tblTypes = Form_LoadFile("field_types.json");

	if ( tblTypes != NULL ) {
		return tblTypes;
	}

	tblTypes = xvoCreateTable();
	xvoTableSetText(tblTypes, "title", 5, "Field Types", 0, FALSE);
	xvoTableSetValue(tblTypes, "types", 5, xvoCreateArray(), TRUE);
	return tblTypes;
}

static xvalue Form_CreatePropsFromOption(xvalue tblOption)
{
	xvalue tblProps = xvoCreateTable();
	xvalue objVal;

	if ( (tblOption == NULL) || (xvoType(tblOption) != XVO_DT_TABLE) ) {
		return tblProps;
	}

	objVal = xvoTableGetValue(tblOption, "height", 6);
	if ( objVal ) xvoTableSetValue(tblProps, "height", 6, xvoCopy(objVal), TRUE);
	objVal = xvoTableGetValue(tblOption, "placeholder", 11);
	if ( objVal ) xvoTableSetValue(tblProps, "placeholder", 11, xvoCopy(objVal), TRUE);
	objVal = xvoTableGetValue(tblOption, "min", 3);
	if ( objVal ) xvoTableSetValue(tblProps, "min", 3, xvoCopy(objVal), TRUE);
	objVal = xvoTableGetValue(tblOption, "max", 3);
	if ( objVal ) xvoTableSetValue(tblProps, "max", 3, xvoCopy(objVal), TRUE);
	objVal = xvoTableGetValue(tblOption, "step", 4);
	if ( objVal ) xvoTableSetValue(tblProps, "step", 4, xvoCopy(objVal), TRUE);
	objVal = xvoTableGetValue(tblOption, "mode", 4);
	if ( objVal ) xvoTableSetValue(tblProps, "mode", 4, xvoCopy(objVal), TRUE);
	objVal = xvoTableGetValue(tblOption, "theme", 5);
	if ( objVal ) xvoTableSetValue(tblProps, "theme", 5, xvoCopy(objVal), TRUE);
	objVal = xvoTableGetValue(tblOption, "readonly", 8);
	if ( objVal ) xvoTableSetValue(tblProps, "readonly", 8, xvoCopy(objVal), TRUE);
	objVal = xvoTableGetValue(tblOption, "disabled", 8);
	if ( objVal ) xvoTableSetValue(tblProps, "disabled", 8, xvoCopy(objVal), TRUE);

	return tblProps;
}

static xvalue Form_CreateSchemaFromOptionConfig(xvalue tblConfig, xvalue* ppValues)
{
	xvalue tblSchema;
	xvalue arrGroups;
	xvalue tblValues;
	xvalue arrClassList;

	if ( (tblConfig == NULL) || (xvoType(tblConfig) != XVO_DT_TABLE) ) {
		return NULL;
	}

	tblSchema = xvoCreateTable();
	arrGroups = xvoCreateArray();
	tblValues = xvoCreateTable();
	xvoTableSetText(tblSchema, "title", 5, xvoTableGetText(tblConfig, "title", 5), 0, FALSE);
	xvoTableSetText(tblSchema, "desc", 4, xvoTableGetText(tblConfig, "desc", 4), 0, FALSE);
	xvoTableSetValue(tblSchema, "groups", 6, arrGroups, TRUE);

	arrClassList = xvoTableGetValue(tblConfig, "classList", 9);
	if ( (arrClassList != NULL) && (xvoType(arrClassList) == XVO_DT_ARRAY) ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrClassList); i++ ) {
			xvalue tblClass = xvoArrayGetValue(arrClassList, i);
			xvalue tblGroup;
			xvalue arrFields;
			xvalue arrOptions;

			if ( (tblClass == NULL) || (xvoType(tblClass) != XVO_DT_TABLE) ) {
				continue;
			}

			tblGroup = xvoCreateTable();
			arrFields = xvoCreateArray();
			xvoTableSetText(tblGroup, "key", 3, xrtFormat("group_%u", (unsigned int)i), 0, TRUE);
			xvoTableSetText(tblGroup, "title", 5, xvoTableGetText(tblClass, "title", 5), 0, FALSE);
			xvoTableSetText(tblGroup, "desc", 4, xvoTableGetText(tblClass, "desc", 4), 0, FALSE);
			xvoTableSetValue(tblGroup, "fields", 6, arrFields, TRUE);

			arrOptions = xvoTableGetValue(tblClass, "options", 7);
			if ( (arrOptions != NULL) && (xvoType(arrOptions) == XVO_DT_ARRAY) ) {
				for ( uint32 j = 0; j < xvoArrayItemCount(arrOptions); j++ ) {
					xvalue tblOption = xvoArrayGetValue(arrOptions, j);
					xvalue tblField;
					xvalue objValue;
					xvalue objList;
					str sName;

					if ( (tblOption == NULL) || (xvoType(tblOption) != XVO_DT_TABLE) ) {
						continue;
					}

					sName = xvoTableGetText(tblOption, "name", 4);
					if ( !Form_HasNonSpaceText(sName) ) {
						continue;
					}

					tblField = xvoCreateTable();
					xvoTableSetText(tblField, "name", 4, sName, 0, FALSE);
					xvoTableSetText(tblField, "type", 4, xvoTableGetText(tblOption, "type", 4), 0, FALSE);
					xvoTableSetText(tblField, "label", 5, xvoTableGetText(tblOption, "title", 5), 0, FALSE);
					xvoTableSetText(tblField, "desc", 4, xvoTableGetText(tblOption, "desc", 4), 0, FALSE);
					xvoTableSetBool(tblField, "required", 8, xvoTableGetBool(tblOption, "required", 8));
					xvoTableSetBool(tblField, "readonly", 8, xvoTableGetBool(tblOption, "readonly", 8));
					xvoTableSetBool(tblField, "disabled", 8, xvoTableGetBool(tblOption, "disabled", 8));
					xvoTableSetValue(tblField, "props", 5, Form_CreatePropsFromOption(tblOption), TRUE);

					objList = xvoTableGetValue(tblOption, "list", 4);
					if ( objList != NULL ) {
						xvoTableSetValue(tblField, "list", 4, xvoCopy(objList), TRUE);
					}
					objList = xvoTableGetValue(tblOption, "actions", 7);
					if ( objList != NULL ) {
						xvoTableSetValue(tblField, "actions", 7, xvoCopy(objList), TRUE);
					}

					objValue = xvoTableGetValue(tblOption, "value", 5);
					if ( objValue != NULL ) {
						xvoTableSetValue(tblValues, sName, 0, xvoCopy(objValue), TRUE);
					}

					xvoArrayAppendValue(arrFields, tblField, TRUE);
				}
			}

			xvoArrayAppendValue(arrGroups, tblGroup, TRUE);
		}
	}

	if ( ppValues != NULL ) {
		*ppValues = tblValues;
	} else {
		xvoUnref(tblValues);
	}

	return tblSchema;
}

static bool Form_ValueIsEmpty(xvalue objValue)
{
	int iType;

	if ( objValue == NULL ) {
		return TRUE;
	}

	iType = xvoType(objValue);
	switch ( iType ) {
		case XVO_DT_NULL:
			return TRUE;
		case XVO_DT_TEXT:
			return !Form_HasNonSpaceText(xvoGetText(objValue));
		case XVO_DT_ARRAY:
			return xvoArrayItemCount(objValue) == 0;
		default:
			return FALSE;
	}
}

static bool Form_ValidateFieldsArray(xvalue arrFields, xvalue tblData, str* psError)
{
	uint32 iCount;

	if ( (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) ) {
		return TRUE;
	}

	iCount = xvoArrayItemCount(arrFields);
	for ( uint32 i = 0; i < iCount; i++ ) {
		xvalue tblField = xvoArrayGetValue(arrFields, i);
		str sName;
		str sLabel;
		bool bRequired;
		xvalue objValue;

		if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
			continue;
		}

		sName = xvoTableGetText(tblField, "name", 4);
		if ( !Form_HasNonSpaceText(sName) ) {
			continue;
		}

		bRequired = xvoTableGetBool(tblField, "required", 8);
		if ( !bRequired ) {
			continue;
		}

		objValue = xvoTableGetValue(tblData, sName, 0);
		if ( !Form_ValueIsEmpty(objValue) ) {
			continue;
		}

		sLabel = xvoTableGetText(tblField, "label", 5);
		if ( !Form_HasNonSpaceText(sLabel) ) {
			sLabel = xvoTableGetText(tblField, "title", 5);
		}
		if ( !Form_HasNonSpaceText(sLabel) ) {
			sLabel = sName;
		}

		if ( psError != NULL ) {
			*psError = xrtFormat("%s 为必填项", sLabel);
		}
		return FALSE;
	}

	return TRUE;
}

static bool Form_ValidateSubmitData(xvalue tblForm, xvalue tblData, str* psError)
{
	xvalue arrGroups;
	xvalue arrClassList;

	if ( (tblForm == NULL) || (tblData == NULL) ) {
		if ( psError != NULL ) {
			*psError = xrtCopyStr("表单数据无效", 0);
		}
		return FALSE;
	}

	arrGroups = xvoTableGetValue(tblForm, "groups", 6);
	if ( (arrGroups != NULL) && (xvoType(arrGroups) == XVO_DT_ARRAY) ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrGroups); i++ ) {
			xvalue tblGroup = xvoArrayGetValue(arrGroups, i);
			xvalue arrFields;

			if ( (tblGroup == NULL) || (xvoType(tblGroup) != XVO_DT_TABLE) ) {
				continue;
			}

			arrFields = xvoTableGetValue(tblGroup, "fields", 6);
			if ( !Form_ValidateFieldsArray(arrFields, tblData, psError) ) {
				return FALSE;
			}
		}
	}

	arrClassList = xvoTableGetValue(tblForm, "classList", 9);
	if ( (arrClassList != NULL) && (xvoType(arrClassList) == XVO_DT_ARRAY) ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrClassList); i++ ) {
			xvalue tblGroup = xvoArrayGetValue(arrClassList, i);
			xvalue arrOptions;

			if ( (tblGroup == NULL) || (xvoType(tblGroup) != XVO_DT_TABLE) ) {
				continue;
			}

			arrOptions = xvoTableGetValue(tblGroup, "options", 7);
			if ( !Form_ValidateFieldsArray(arrOptions, tblData, psError) ) {
				return FALSE;
			}
		}
	}

	return TRUE;
}

static bool Form_SaveDemoData(const char* sFileName, xvalue tblData, str* psError)
{
	xvalue tblForm;
	str sFilePath;
	bool bPersist;
	bool bRet = FALSE;

	if ( !Form_IsValidFileName(sFileName) ) {
		if ( psError != NULL ) {
			*psError = xrtCopyStr("非法的表单文件名", 0);
		}
		return FALSE;
	}

	tblForm = Form_LoadFile(sFileName);
	if ( tblForm == NULL ) {
		if ( psError != NULL ) {
			*psError = xrtCopyStr("表单文件不存在或解析失败", 0);
		}
		return FALSE;
	}

	if ( !Form_ValidateSubmitData(tblForm, tblData, psError) ) {
		xvoUnref(tblForm);
		return FALSE;
	}

	bPersist = xvoTableGetBool(tblForm, "persistDemoData", 15);
	if ( !bPersist ) {
		xvoUnref(tblForm);
		return TRUE;
	}

	xvoTableSetValue(tblForm, "demoData", 8, xvoCopy(tblData), TRUE);
	sFilePath = Form_BuildFilePath(sFileName);
	if ( sFilePath != NULL ) {
		bRet = xrtStringifyJSON_File(sFilePath, tblForm, TRUE) >= 0;
		xrtFree(sFilePath);
	}

	xvoUnref(tblForm);

	if ( !bRet && psError != NULL ) {
		*psError = xrtCopyStr("表单演示数据保存失败", 0);
	}

	return bRet;
}

static void Form_ReplyJSONValue(XS_ResponseObject objResp, xvalue tblRet)
{
	size_t iRetSize = 0;
	str sRet;

	if ( tblRet == NULL ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"response build failed\"}", 0);
		return;
	}

	sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
	if ( sRet == NULL ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"response stringify failed\"}", 0);
		xvoUnref(tblRet);
		return;
	}

	http_reply(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
	xvoUnref(tblRet);
}

static void Form_ReplyError(XS_ResponseObject objResp, const char* sMessage)
{
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, FALSE);
	xvoTableSetText(tblRet, "message", 7, (str)(sMessage ? sMessage : "请求失败"), 0, FALSE);
	Form_ReplyJSONValue(objResp, tblRet);
}

static void Form_ReplySuccess(XS_ResponseObject objResp, const char* sMessage, xvalue tblData)
{
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetText(tblRet, "message", 7, (str)(sMessage ? sMessage : "操作成功"), 0, FALSE);
	if ( tblData != NULL ) {
		xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	}
	Form_ReplyJSONValue(objResp, tblRet);
}

void Form_Init()
{
	printf("        Form_Init \n");
	G_FormPath = xrtPathJoin(3, AppPath, "data", "forms");
	G_FormInstallPath = xrtPathJoin(4, AppPath, "data", "install", "forms");
	if ( G_FormPath ) {
		xrtDirCreateAll(G_FormPath);
	}
	if ( G_FormInstallPath ) {
		xrtDirCreateAll(G_FormInstallPath);
	}
}

void Form_Unit()
{
	printf("        Form_Unit \n");
	if ( G_FormPath ) {
		xrtFree(G_FormPath);
		G_FormPath = NULL;
	}
	if ( G_FormInstallPath ) {
		xrtFree(G_FormInstallPath);
		G_FormInstallPath = NULL;
	}
}
