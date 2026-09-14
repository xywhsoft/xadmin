/* v1 动态表单引擎：JSON 表单定义 → schema/values 数据接口 + 演示数据保存。
 * 路径适配：v1 的 data/forms、data/install/forms 按本代目录规划平铺为
 * AppPath/forms 与 AppPath/install/forms。
 * 未迁移：v1 模板 {{#form}} 块的静态 HTML 渲染器（约 600 行，仅服务
 * form/block_demo.html 演示模板与 /admin/view/template/form_demo 演示页）；
 * 新模板引擎经 xtemplateregistry 扩展面接入时一并恢复。 */
static str G_FormPath = NULL;
static str G_FormInstallPath = NULL;

static str Form_GetBasePath()
{
	if ( G_FormPath != NULL ) {
		return G_FormPath;
	}
	if ( AppPath == NULL ) {
		return NULL;
	}
	return xrtPathJoin(AppPath, "forms");
}

static bool Form_IsSpaceChar(char c)
{
	return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

static bool Form_HasNonSpaceText(const char* sText)
{
	if ( sText == NULL ) {
		return false;
	}

	while ( *sText ) {
		if ( !Form_IsSpaceChar(*sText) ) {
			return true;
		}
		sText++;
	}

	return false;
}

static const char* Form_CStrOr(str sText, const char* sDefault)
{
	return sText ? (const char*)sText : sDefault;
}

static bool Form_IsValidFileName(const char* sFileName)
{
	size_t iLen;
	size_t iBaseLen;
	size_t i;

	if ( (sFileName == NULL) || (sFileName[0] == '\0') ) {
		return false;
	}

	iLen = strlen(sFileName);
	if ( (iLen <= 5) || (strcmp(sFileName + iLen - 5, ".json") != 0) ) {
		return false;
	}

	iBaseLen = iLen - 5;
	for ( i = 0; i < iBaseLen; i++ ) {
		char c = sFileName[i];
		bool bOK = ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '_') || (c == '-');
		if ( !bOK ) {
			return false;
		}
	}

	return true;
}

static str Form_BuildFilePath(const char* sFileName)
{
	str sBasePath;
	str sFilePath;

	if ( !Form_IsValidFileName(sFileName) ) {
		return NULL;
	}

	sBasePath = Form_GetBasePath();
	if ( sBasePath == NULL ) {
		return NULL;
	}

	sFilePath = xrtPathJoin(sBasePath, sFileName);
	if ( sBasePath != G_FormPath ) {
		xrtFree(sBasePath);
	}
	return sFilePath;
}

static xvalue* Form_LoadFile(const char* sFileName)
{
	str sFilePath;
	xvalue* tblForm;

	if ( !Form_IsValidFileName(sFileName) ) {
		return NULL;
	}

	sFilePath = Form_BuildFilePath(sFileName);
	if ( sFilePath == NULL ) {
		return NULL;
	}

	tblForm = JsonParseFile(sFilePath);
	xrtFree(sFilePath);
	return tblForm;
}

static xvalue* Form_LoadFieldTypes()
{
	xvalue* tblTypes = Form_LoadFile("field_types.json");

	if ( tblTypes != NULL ) {
		return tblTypes;
	}

	tblTypes = ValueObject();
	ValueSetText(tblTypes, "title", "Field Types");
	ValueSetOwn(tblTypes, "types", ValueArray());
	return tblTypes;
}

static xvalue* Form_CreatePropsFromOption(xvalue* tblOption)
{
	xvalue* tblProps = ValueObject();
	xvalue* objVal;

	if ( (tblOption == NULL) || (xrtValueType(tblOption) != XVALUE_OBJECT) ) {
		return tblProps;
	}

	objVal = ValueGet(tblOption, "height");
	if ( objVal ) ValueSetOwn(tblProps, "height", xrtValueDeepClone(objVal));
	objVal = ValueGet(tblOption, "placeholder");
	if ( objVal ) ValueSetOwn(tblProps, "placeholder", xrtValueDeepClone(objVal));
	objVal = ValueGet(tblOption, "min");
	if ( objVal ) ValueSetOwn(tblProps, "min", xrtValueDeepClone(objVal));
	objVal = ValueGet(tblOption, "max");
	if ( objVal ) ValueSetOwn(tblProps, "max", xrtValueDeepClone(objVal));
	objVal = ValueGet(tblOption, "step");
	if ( objVal ) ValueSetOwn(tblProps, "step", xrtValueDeepClone(objVal));
	objVal = ValueGet(tblOption, "mode");
	if ( objVal ) ValueSetOwn(tblProps, "mode", xrtValueDeepClone(objVal));
	objVal = ValueGet(tblOption, "theme");
	if ( objVal ) ValueSetOwn(tblProps, "theme", xrtValueDeepClone(objVal));
	objVal = ValueGet(tblOption, "readonly");
	if ( objVal ) ValueSetOwn(tblProps, "readonly", xrtValueDeepClone(objVal));
	objVal = ValueGet(tblOption, "disabled");
	if ( objVal ) ValueSetOwn(tblProps, "disabled", xrtValueDeepClone(objVal));

	return tblProps;
}

static xvalue* Form_CreateSchemaFromOptionConfig(xvalue* tblConfig, xvalue** ppValues)
{
	xvalue* tblSchema;
	xvalue* arrGroups;
	xvalue* tblValues;
	xvalue* arrClassList;

	if ( (tblConfig == NULL) || (xrtValueType(tblConfig) != XVALUE_OBJECT) ) {
		return NULL;
	}

	tblSchema = ValueObject();
	arrGroups = ValueArray();
	tblValues = ValueObject();
	ValueSetText(tblSchema, "title", ValueText(tblConfig, "title"));
	ValueSetText(tblSchema, "desc", ValueText(tblConfig, "desc"));
	ValueSetOwn(tblSchema, "groups", arrGroups);

	arrClassList = ValueGet(tblConfig, "classList");
	if ( (arrClassList != NULL) && (xrtValueType(arrClassList) == XVALUE_ARRAY) ) {
		for ( uint32 i = 0; i < ValueCount(arrClassList); i++ ) {
			xvalue* tblClass = xrtValueArrayGet(arrClassList, i);
			xvalue* tblGroup;
			xvalue* arrFields;
			xvalue* arrOptions;

			if ( (tblClass == NULL) || (xrtValueType(tblClass) != XVALUE_OBJECT) ) {
				continue;
			}

			tblGroup = ValueObject();
			arrFields = ValueArray();
			ValueSetOwnedText(tblGroup, "key", xrtFormat("group_%u", (unsigned int)i));
			ValueSetText(tblGroup, "title", ValueText(tblClass, "title"));
			ValueSetText(tblGroup, "desc", ValueText(tblClass, "desc"));
			ValueSetOwn(tblGroup, "fields", arrFields);

			arrOptions = ValueGet(tblClass, "options");
			if ( (arrOptions != NULL) && (xrtValueType(arrOptions) == XVALUE_ARRAY) ) {
				for ( uint32 j = 0; j < ValueCount(arrOptions); j++ ) {
					xvalue* tblOption = xrtValueArrayGet(arrOptions, j);
					xvalue* tblField;
					xvalue* objValue;
					xvalue* objList;
					str sName;

					if ( (tblOption == NULL) || (xrtValueType(tblOption) != XVALUE_OBJECT) ) {
						continue;
					}

					sName = ValueText(tblOption, "name");
					if ( !Form_HasNonSpaceText(sName) ) {
						continue;
					}

					tblField = ValueObject();
					ValueSetText(tblField, "name", sName);
					ValueSetText(tblField, "type", ValueText(tblOption, "type"));
					ValueSetText(tblField, "label", ValueText(tblOption, "title"));
					ValueSetText(tblField, "desc", ValueText(tblOption, "desc"));
					ValueSetBool(tblField, "required", ValueBool(tblOption, "required"));
					ValueSetBool(tblField, "readonly", ValueBool(tblOption, "readonly"));
					ValueSetBool(tblField, "disabled", ValueBool(tblOption, "disabled"));
					ValueSetOwn(tblField, "props", Form_CreatePropsFromOption(tblOption));

					objList = ValueGet(tblOption, "list");
					if ( objList != NULL ) {
						ValueSetOwn(tblField, "list", xrtValueDeepClone(objList));
					}
					objList = ValueGet(tblOption, "actions");
					if ( objList != NULL ) {
						ValueSetOwn(tblField, "actions", xrtValueDeepClone(objList));
					}

					objValue = ValueGet(tblOption, "value");
					if ( objValue != NULL ) {
						ValueSetOwn(tblValues, sName, xrtValueDeepClone(objValue));
					}

					ValueArrayOwn(arrFields, tblField);
				}
			}

			ValueArrayOwn(arrGroups, tblGroup);
		}
	}

	if ( ppValues != NULL ) {
		*ppValues = tblValues;
	} else {
		xrtValueRelease(tblValues);
	}

	return tblSchema;
}

static bool Form_ValueIsEmpty(xvalue* objValue)
{
	int iType;

	if ( objValue == NULL ) {
		return true;
	}

	iType = xrtValueType(objValue);
	switch ( iType ) {
		case XVALUE_NULL:
			return true;
		case XVALUE_STRING:
			return !Form_HasNonSpaceText(ValueTextOf(objValue));
		case XVALUE_ARRAY:
			return ValueCount(objValue) == 0;
		default:
			return false;
	}
}

static bool Form_ValidateFieldsArray(xvalue* arrFields, xvalue* tblData, str* psError)
{
	uint32 iCount;

	if ( (arrFields == NULL) || (xrtValueType(arrFields) != XVALUE_ARRAY) ) {
		return true;
	}

	iCount = ValueCount(arrFields);
	for ( uint32 i = 0; i < iCount; i++ ) {
		xvalue* tblField = xrtValueArrayGet(arrFields, i);
		str sName;
		str sLabel;
		bool bRequired;
		xvalue* objValue;

		if ( (tblField == NULL) || (xrtValueType(tblField) != XVALUE_OBJECT) ) {
			continue;
		}

		sName = ValueText(tblField, "name");
		if ( !Form_HasNonSpaceText(sName) ) {
			continue;
		}

		bRequired = ValueBool(tblField, "required");
		if ( !bRequired ) {
			continue;
		}

		objValue = ValueGet(tblData, sName);
		if ( !Form_ValueIsEmpty(objValue) ) {
			continue;
		}

		sLabel = ValueText(tblField, "label");
		if ( !Form_HasNonSpaceText(sLabel) ) {
			sLabel = ValueText(tblField, "title");
		}
		if ( !Form_HasNonSpaceText(sLabel) ) {
			sLabel = sName;
		}

		if ( psError != NULL ) {
			*psError = xrtFormat("%s 为必填项", sLabel);
		}
		return false;
	}

	return true;
}

static bool Form_ValidateSubmitData(xvalue* tblForm, xvalue* tblData, str* psError)
{
	xvalue* arrGroups;
	xvalue* arrClassList;

	if ( (tblForm == NULL) || (tblData == NULL) ) {
		if ( psError != NULL ) {
			*psError = xrtStrDup("表单数据无效");
		}
		return false;
	}

	arrGroups = ValueGet(tblForm, "groups");
	if ( (arrGroups != NULL) && (xrtValueType(arrGroups) == XVALUE_ARRAY) ) {
		for ( uint32 i = 0; i < ValueCount(arrGroups); i++ ) {
			xvalue* tblGroup = xrtValueArrayGet(arrGroups, i);
			xvalue* arrFields;

			if ( (tblGroup == NULL) || (xrtValueType(tblGroup) != XVALUE_OBJECT) ) {
				continue;
			}

			arrFields = ValueGet(tblGroup, "fields");
			if ( !Form_ValidateFieldsArray(arrFields, tblData, psError) ) {
				return false;
			}
		}
	}

	arrClassList = ValueGet(tblForm, "classList");
	if ( (arrClassList != NULL) && (xrtValueType(arrClassList) == XVALUE_ARRAY) ) {
		for ( uint32 i = 0; i < ValueCount(arrClassList); i++ ) {
			xvalue* tblGroup = xrtValueArrayGet(arrClassList, i);
			xvalue* arrOptions;

			if ( (tblGroup == NULL) || (xrtValueType(tblGroup) != XVALUE_OBJECT) ) {
				continue;
			}

			arrOptions = ValueGet(tblGroup, "options");
			if ( !Form_ValidateFieldsArray(arrOptions, tblData, psError) ) {
				return false;
			}
		}
	}

	return true;
}

static bool Form_SaveDemoData(const char* sFileName, xvalue* tblData, str* psError)
{
	xvalue* tblForm;
	str sFilePath;
	bool bPersist;
	bool bRet = false;

	if ( !Form_IsValidFileName(sFileName) ) {
		if ( psError != NULL ) {
			*psError = xrtStrDup("非法的表单文件名");
		}
		return false;
	}

	tblForm = Form_LoadFile(sFileName);
	if ( tblForm == NULL ) {
		if ( psError != NULL ) {
			*psError = xrtStrDup("表单文件不存在或解析失败");
		}
		return false;
	}

	if ( !Form_ValidateSubmitData(tblForm, tblData, psError) ) {
		xrtValueRelease(tblForm);
		return false;
	}

	bPersist = ValueBool(tblForm, "persistDemoData");
	if ( !bPersist ) {
		xrtValueRelease(tblForm);
		return true;
	}

	ValueSetOwn(tblForm, "demoData", xrtValueDeepClone(tblData));
	sFilePath = Form_BuildFilePath(sFileName);
	if ( sFilePath != NULL ) {
		bRet = JsonWriteFile(sFilePath, tblForm, true);
		xrtFree(sFilePath);
	}

	xrtValueRelease(tblForm);

	if ( !bRet && psError != NULL ) {
		*psError = xrtStrDup("表单演示数据保存失败");
	}

	return bRet;
}

/* v1 的 xsHttpJsonValueTake 语义：接管并释放传入值。 */
static void Form_ReplyJSONValue(XS_ResponseObject objResp, xvalue* tblRet)
{
	size_t iRetSize = 0;
	char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
	xrtValueRelease(tblRet);
}

static void Form_ReplyError(XS_ResponseObject objResp, const char* sMessage)
{
	xvalue* tblRet = ValueObject();
	ValueSetBool(tblRet, "result", false);
	ValueSetText(tblRet, "message", (str)(sMessage ? sMessage : "请求失败"));
	Form_ReplyJSONValue(objResp, tblRet);
}

static void Form_ReplySuccess(XS_ResponseObject objResp, const char* sMessage, xvalue* tblData)
{
	xvalue* tblRet = ValueObject();
	ValueSetBool(tblRet, "result", true);
	ValueSetText(tblRet, "message", (str)(sMessage ? sMessage : "操作成功"));
	if ( tblData != NULL ) {
		ValueSetOwn(tblRet, "data", tblData);
	}
	Form_ReplyJSONValue(objResp, tblRet);
}

void Form_Init()
{
	printf("        Form_Init \n");
	G_FormPath = xrtPathJoin(AppPath, "forms");
	G_FormInstallPath = xrtPathJoin(xrtPathJoin(AppPath, "install"), "forms");
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
