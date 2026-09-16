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

/* ==================== {{#form}} 模板块渲染器（v1 form.h 对齐） ====================
 * 语法：{{#form}} JSON 规格 {{#end}}。块内 JSON 三种来源：
 *   {"source":"form","file":"x.json"}   —— forms/ 目录表单定义（demoData 为示例值）
 *   {"source":"option","file":"x.json"} —— options/ 配置文件转表单展示
 *   内联 schema（groups/fields 结构） + 可选 values/data 覆盖值
 * 渲染为带内嵌样式的静态表单 HTML；经模板扩展注册表（RAW_BLOCK）接入原生引擎。
 * v1 在编译期预渲染缓存产物，v3 扩展回调在渲染期执行——纯内存拼接，代价相当。 */

typedef struct FormTemplateHtmlRenderResult {
	xvalue* schema;
	xvalue* values;
} FormTemplateHtmlRenderResult;

static void Form_BufferAppendText(xbuffer* buf, const char* text)
{
	if (!buf || !text) return;
	xrtBufferAppend(buf, (xbytesview){(cbytes)text, strlen(text)});
}

static void Form_BufferAppendChar(xbuffer* buf, char ch)
{
	if (!buf) return;
	xrtBufferAppendByte(buf, (unsigned char)ch);
}

static void Form_BufferAppendEscaped(xbuffer* buf, const char* text)
{
	const char* p;
	if (!buf || !text) return;
	for (p = text; *p; p++) {
		switch (*p) {
			case '&': Form_BufferAppendText(buf, "&amp;"); break;
			case '<': Form_BufferAppendText(buf, "&lt;"); break;
			case '>': Form_BufferAppendText(buf, "&gt;"); break;
			case '"': Form_BufferAppendText(buf, "&quot;"); break;
			case '\'': Form_BufferAppendText(buf, "&#39;"); break;
			default: Form_BufferAppendChar(buf, *p); break;
		}
	}
}

static char* Form_ValueToTextDup(const xvalue* value)
{
	size_t size = 0;
	if (!value) return xrtStrDup("");
	switch (xrtValueType(value)) {
		case XVALUE_STRING: {
			xstrview text = {0};
			char* out;
			if (!xrtValueGetString(value, &text) || !text.Size) return xrtStrDup("");
			out = (char*)xrtMalloc(text.Size + 1);
			if (!out) return xrtStrDup("");
			memcpy(out, text.Data, text.Size);
			out[text.Size] = '\0';
			return out;
		}
		case XVALUE_BOOL:
			return xrtStrDup(ValueBoolOf(value) ? "true" : "false");
		case XVALUE_INT:
			return xrtFormat("%lld", (long long)ValueIntOf(value));
		case XVALUE_FLOAT:
			return xrtFormat("%g", ValueFloatOf(value));
		case XVALUE_NULL:
			return xrtStrDup("");
		default: {
			char* json = xrtJsonStringify((xvalue*)value, false, &size);
			return json ? json : xrtStrDup("");
		}
	}
}

static const xvalue* Form_GetTemplateFieldValue(const xvalue* values, const xvalue* field)
{
	str name;
	const xvalue* value;
	if (!field || xrtValueType(field) != XVALUE_OBJECT) return NULL;
	name = ValueText(field, "name");
	if (Form_HasNonSpaceText(name) && values && xrtValueType(values) == XVALUE_OBJECT) {
		value = ValueGet(values, name);
		if (value) return value;
	}
	if ((value = ValueGet(field, "value")) != NULL) return value;
	return ValueGet(field, "default");
}

static void Form_RenderTemplateFieldDesc(xbuffer* buf, const xvalue* field)
{
	str desc = ValueText(field, "desc");
	if (Form_HasNonSpaceText(desc)) {
		Form_BufferAppendText(buf, "<div class=\"xform-tpl-field-desc\">");
		Form_BufferAppendEscaped(buf, desc);
		Form_BufferAppendText(buf, "</div>");
	}
}

static void Form_RenderTemplateOptionList(xbuffer* buf, const xvalue* list, const char* current)
{
	uint32 i;
	if (!list || xrtValueType(list) != XVALUE_ARRAY) return;
	for (i = 0; i < ValueCount(list); i++) {
		const xvalue* item = xrtValueArrayGet(list, i);
		str value; str label; bool selected;
		if (!item || xrtValueType(item) != XVALUE_OBJECT) continue;
		value = ValueText(item, "value");
		label = ValueText(item, "label");
		if (!Form_HasNonSpaceText(label)) label = value;
		selected = current && value && !strcmp(current, value);
		Form_BufferAppendText(buf, "<option value=\"");
		Form_BufferAppendEscaped(buf, value ? value : "");
		Form_BufferAppendText(buf, "\"");
		if (selected) Form_BufferAppendText(buf, " selected");
		Form_BufferAppendText(buf, ">");
		Form_BufferAppendEscaped(buf, label ? label : "");
		Form_BufferAppendText(buf, "</option>");
	}
}

static void Form_RenderTemplateChoices(xbuffer* buf, const xvalue* field, const xvalue* list, const xvalue* value, const char* inputType)
{
	uint32 i, j;
	char* currentText;
	if (!list || xrtValueType(list) != XVALUE_ARRAY) return;
	currentText = Form_ValueToTextDup(value);
	for (i = 0; i < ValueCount(list); i++) {
		const xvalue* item = xrtValueArrayGet(list, i);
		str itemValue; str itemLabel; bool checked = false;
		if (!item || xrtValueType(item) != XVALUE_OBJECT) continue;
		itemValue = ValueText(item, "value");
		itemLabel = ValueText(item, "label");
		if (!Form_HasNonSpaceText(itemLabel)) itemLabel = itemValue;
		if (!strcmp(inputType, "radio")) {
			checked = currentText && itemValue && !strcmp(currentText, itemValue);
		} else if (value && xrtValueType(value) == XVALUE_ARRAY) {
			for (j = 0; j < ValueCount(value); j++) {
				char* text = Form_ValueToTextDup(xrtValueArrayGet(value, j));
				if (text && itemValue && !strcmp(text, itemValue)) checked = true;
				xrtFree(text);
				if (checked) break;
			}
		}
		Form_BufferAppendText(buf, "<label class=\"xform-tpl-choice\"><input type=\"");
		Form_BufferAppendText(buf, inputType);
		Form_BufferAppendText(buf, "\" name=\"");
		Form_BufferAppendEscaped(buf, ValueText(field, "name") ? ValueText(field, "name") : "");
		Form_BufferAppendText(buf, "\" value=\"");
		Form_BufferAppendEscaped(buf, itemValue ? itemValue : "");
		Form_BufferAppendText(buf, "\"");
		if (checked) Form_BufferAppendText(buf, " checked");
		Form_BufferAppendText(buf, "><span>");
		Form_BufferAppendEscaped(buf, itemLabel ? itemLabel : "");
		Form_BufferAppendText(buf, "</span></label>");
	}
	xrtFree(currentText);
}

static char* Form_GetRangeItemText(const xvalue* value, uint32 index)
{
	if (!value) return xrtStrDup("");
	if (xrtValueType(value) == XVALUE_ARRAY) {
		if (index < ValueCount(value)) return Form_ValueToTextDup(xrtValueArrayGet(value, index));
		return xrtStrDup("");
	}
	return xrtStrDup("");
}

static void Form_RenderTemplateRangeInput(xbuffer* buf, const xvalue* field, const xvalue* value, const char* inputType, const char* step)
{
	str name = ValueText(field, "name");
	const xvalue* props = ValueGet(field, "props");
	str minV = (props && xrtValueType(props) == XVALUE_OBJECT) ? ValueText(props, "min") : NULL;
	str maxV = (props && xrtValueType(props) == XVALUE_OBJECT) ? ValueText(props, "max") : NULL;
	char* start = Form_GetRangeItemText(value, 0);
	char* end = Form_GetRangeItemText(value, 1);
	int side;
	for (side = 0; side < 2; side++) {
		Form_BufferAppendText(buf, "<input class=\"xform-tpl-input\" type=\"");
		Form_BufferAppendText(buf, inputType);
		Form_BufferAppendText(buf, "\" name=\"");
		Form_BufferAppendEscaped(buf, name ? name : "");
		Form_BufferAppendText(buf, side == 0 ? "_start\" value=\"" : "_end\" value=\"");
		Form_BufferAppendEscaped(buf, side == 0 ? (start ? start : "") : (end ? end : ""));
		Form_BufferAppendText(buf, "\"");
		if (Form_HasNonSpaceText(minV)) {
			Form_BufferAppendText(buf, " min=\"");
			Form_BufferAppendEscaped(buf, minV);
			Form_BufferAppendText(buf, "\"");
		}
		if (Form_HasNonSpaceText(maxV)) {
			Form_BufferAppendText(buf, " max=\"");
			Form_BufferAppendEscaped(buf, maxV);
			Form_BufferAppendText(buf, "\"");
		}
		if (side == 0 && Form_HasNonSpaceText((char*)step)) {
			Form_BufferAppendText(buf, " step=\"");
			Form_BufferAppendEscaped(buf, step);
			Form_BufferAppendText(buf, "\"");
		}
		Form_BufferAppendText(buf, ">");
		if (side == 0) Form_BufferAppendText(buf, "<span class=\"xform-tpl-range-sep\">to</span>");
	}
	xrtFree(start);
	xrtFree(end);
}

static void Form_RenderTemplateFieldInput(xbuffer* buf, const xvalue* field, const xvalue* value)
{
	str type = ValueText(field, "type");
	str name = ValueText(field, "name");
	const xvalue* list = ValueGet(field, "list");
	const xvalue* props = ValueGet(field, "props");
	char* valueText = Form_ValueToTextDup(value);
	str height = (props && xrtValueType(props) == XVALUE_OBJECT) ? ValueText(props, "height") : NULL;

	if (!Form_HasNonSpaceText(type)) type = "text";

	if (!strcmp(type, "textarea") || !strcmp(type, "editor_html") || !strcmp(type, "editor_md") || !strcmp(type, "editor_code")) {
		Form_BufferAppendText(buf, "<textarea class=\"xform-tpl-textarea\" name=\"");
		Form_BufferAppendEscaped(buf, name ? name : "");
		Form_BufferAppendText(buf, "\"");
		if (Form_HasNonSpaceText(height)) {
			Form_BufferAppendText(buf, " style=\"height:");
			Form_BufferAppendEscaped(buf, height);
			Form_BufferAppendText(buf, ";\"");
		}
		Form_BufferAppendText(buf, ">");
		Form_BufferAppendEscaped(buf, valueText ? valueText : "");
		Form_BufferAppendText(buf, "</textarea>");
	} else if (!strcmp(type, "select")) {
		Form_BufferAppendText(buf, "<select class=\"xform-tpl-select\" name=\"");
		Form_BufferAppendEscaped(buf, name ? name : "");
		Form_BufferAppendText(buf, "\"><option value=\"\">Please select</option>");
		Form_RenderTemplateOptionList(buf, list, valueText);
		Form_BufferAppendText(buf, "</select>");
	} else if (!strcmp(type, "combobox")) {
		uint32 i;
		Form_BufferAppendText(buf, "<input class=\"xform-tpl-input\" type=\"text\" list=\"list_");
		Form_BufferAppendEscaped(buf, name ? name : "");
		Form_BufferAppendText(buf, "\" name=\"");
		Form_BufferAppendEscaped(buf, name ? name : "");
		Form_BufferAppendText(buf, "\" value=\"");
		Form_BufferAppendEscaped(buf, valueText ? valueText : "");
		Form_BufferAppendText(buf, "\"><datalist id=\"list_");
		Form_BufferAppendEscaped(buf, name ? name : "");
		Form_BufferAppendText(buf, "\">");
		if (list && xrtValueType(list) == XVALUE_ARRAY) {
			for (i = 0; i < ValueCount(list); i++) {
				const xvalue* item = xrtValueArrayGet(list, i);
				str itemValue;
				if (!item || xrtValueType(item) != XVALUE_OBJECT) continue;
				itemValue = ValueText(item, "value");
				Form_BufferAppendText(buf, "<option value=\"");
				Form_BufferAppendEscaped(buf, itemValue ? itemValue : "");
				Form_BufferAppendText(buf, "\"></option>");
			}
		}
		Form_BufferAppendText(buf, "</datalist>");
	} else if (!strcmp(type, "radio")) {
		Form_BufferAppendText(buf, "<div class=\"xform-tpl-choice-list\">");
		Form_RenderTemplateChoices(buf, field, list, value, "radio");
		Form_BufferAppendText(buf, "</div>");
	} else if (!strcmp(type, "checkbox") || !strcmp(type, "checklist")) {
		Form_BufferAppendText(buf, "<div class=\"xform-tpl-choice-list\">");
		Form_RenderTemplateChoices(buf, field, list, value, "checkbox");
		Form_BufferAppendText(buf, "</div>");
	} else if (!strcmp(type, "switch")) {
		Form_BufferAppendText(buf, "<label class=\"xform-tpl-switch\"><input type=\"checkbox\" name=\"");
		Form_BufferAppendEscaped(buf, name ? name : "");
		Form_BufferAppendText(buf, "\"");
		if (ValueBoolOf(value)) Form_BufferAppendText(buf, " checked");
		Form_BufferAppendText(buf, "><span>");
		Form_BufferAppendText(buf, ValueBoolOf(value) ? "ON" : "OFF");
		Form_BufferAppendText(buf, "</span></label>");
	} else if (!strcmp(type, "icon_picker")) {
		Form_BufferAppendText(buf, "<div class=\"xform-tpl-icon\">");
		Form_BufferAppendEscaped(buf, valueText ? valueText : "");
		Form_BufferAppendText(buf, "</div>");
	} else if (!strcmp(type, "intrange")) {
		Form_RenderTemplateRangeInput(buf, field, value, "number", "1");
	} else if (!strcmp(type, "numrange")) {
		Form_RenderTemplateRangeInput(buf, field, value, "number", "0.01");
	} else if (!strcmp(type, "daterange")) {
		Form_RenderTemplateRangeInput(buf, field, value, "date", NULL);
	} else if (!strcmp(type, "timerange")) {
		Form_RenderTemplateRangeInput(buf, field, value, "time", NULL);
	} else if (!strcmp(type, "datetimerange")) {
		Form_RenderTemplateRangeInput(buf, field, value, "datetime-local", NULL);
	} else {
		const char* inputType = "text";
		if (!strcmp(type, "number") || !strcmp(type, "int") || !strcmp(type, "num")) inputType = "number";
		else if (!strcmp(type, "password")) inputType = "password";
		else if (!strcmp(type, "date")) inputType = "date";
		else if (!strcmp(type, "datetime")) inputType = "datetime-local";
		else if (!strcmp(type, "time")) inputType = "time";
		Form_BufferAppendText(buf, "<input class=\"xform-tpl-input\" type=\"");
		Form_BufferAppendText(buf, inputType);
		Form_BufferAppendText(buf, "\" name=\"");
		Form_BufferAppendEscaped(buf, name ? name : "");
		Form_BufferAppendText(buf, "\" value=\"");
		Form_BufferAppendEscaped(buf, valueText ? valueText : "");
		Form_BufferAppendText(buf, "\">");
	}
	xrtFree(valueText);
}

static void Form_RenderTemplateField(xbuffer* buf, const xvalue* field, const xvalue* values)
{
	str label = ValueText(field, "label");
	const xvalue* value = Form_GetTemplateFieldValue(values, field);
	if (!Form_HasNonSpaceText(label)) label = ValueText(field, "name");
	Form_BufferAppendText(buf, "<div class=\"xform-tpl-field\"><label class=\"xform-tpl-label\">");
	Form_BufferAppendEscaped(buf, label ? label : "");
	if (ValueBool(field, "required")) Form_BufferAppendText(buf, "<span class=\"xform-tpl-required\">*</span>");
	Form_BufferAppendText(buf, "</label><div class=\"xform-tpl-control\">");
	Form_RenderTemplateFieldInput(buf, field, value);
	Form_RenderTemplateFieldDesc(buf, field);
	Form_BufferAppendText(buf, "</div></div>");
}

static void Form_RenderTemplateGroups(xbuffer* buf, const xvalue* schema, const xvalue* values)
{
	const xvalue* groups = ValueGet(schema, "groups");
	uint32 i, j;
	if (!groups || xrtValueType(groups) != XVALUE_ARRAY) return;
	for (i = 0; i < ValueCount(groups); i++) {
		const xvalue* group = xrtValueArrayGet(groups, i);
		const xvalue* fields;
		str title; str desc;
		if (!group || xrtValueType(group) != XVALUE_OBJECT) continue;
		fields = ValueGet(group, "fields");
		if (!fields || xrtValueType(fields) != XVALUE_ARRAY) continue;
		title = ValueText(group, "title");
		desc = ValueText(group, "desc");
		Form_BufferAppendText(buf, "<section class=\"xform-tpl-group\">");
		if (Form_HasNonSpaceText(title)) {
			Form_BufferAppendText(buf, "<div class=\"xform-tpl-group-title\">");
			Form_BufferAppendEscaped(buf, title);
			Form_BufferAppendText(buf, "</div>");
		}
		if (Form_HasNonSpaceText(desc)) {
			Form_BufferAppendText(buf, "<div class=\"xform-tpl-group-desc\">");
			Form_BufferAppendEscaped(buf, desc);
			Form_BufferAppendText(buf, "</div>");
		}
		Form_BufferAppendText(buf, "<div class=\"xform-tpl-group-fields\">");
		for (j = 0; j < ValueCount(fields); j++) {
			const xvalue* field = xrtValueArrayGet(fields, j);
			if (!field || xrtValueType(field) != XVALUE_OBJECT) continue;
			Form_RenderTemplateField(buf, field, values);
		}
		Form_BufferAppendText(buf, "</div></section>");
	}
}

static bool Form_ResolveTemplateRenderSpec(const xvalue* spec, FormTemplateHtmlRenderResult* out, str* error)
{
	const xvalue* loaded = NULL;
	xvalue* values = NULL;
	xvalue* schema = NULL;
	const xvalue* override;
	str file; str source;

	if (!out) return false;
	memset(out, 0, sizeof(*out));
	if (!spec || xrtValueType(spec) != XVALUE_OBJECT) {
		if (error) *error = xrtStrDup("form block json must be an object");
		return false;
	}
	file = ValueText(spec, "file");
	source = ValueText(spec, "source");
	if (Form_HasNonSpaceText(file)) {
		if (source && !strcmp(source, "option")) {
			loaded = Option_LoadFile((str)file);
			if (!loaded) {
				if (error) *error = xrtFormat("option file not found: %s", file);
				return false;
			}
			schema = Form_CreateSchemaFromOptionConfig((xvalue*)loaded, &values);
			xrtValueRelease((xvalue*)loaded);
			if (!schema) {
				if (error) *error = xrtFormat("option schema build failed: %s", file);
				return false;
			}
		} else {
			loaded = Form_LoadFile(file);
			if (!loaded) {
				if (error) *error = xrtFormat("form file not found: %s", file);
				return false;
			}
			schema = xrtValueClone(loaded);
			override = ValueGet(loaded, "demoData");
			values = override ? xrtValueClone(override) : ValueObject();
			xrtValueRelease((xvalue*)loaded);
		}
	} else {
		schema = xrtValueClone(spec);
		values = ValueObject();
	}
	/* 块内 values/data 覆盖示例值 */
	override = ValueGet(spec, "values");
	if (!override) override = ValueGet(spec, "data");
	if (override && xrtValueType(override) == XVALUE_OBJECT) {
		xrtValueRelease(values);
		values = xrtValueClone(override);
	}
	out->schema = schema;
	out->values = values;
	return true;
}

static char* Form_RenderTemplateBlockHTML(const xvalue* spec, str* error)
{
	FormTemplateHtmlRenderResult resolved;
	xbuffer* buf;
	str title; str desc;
	char* result;

	if (!Form_ResolveTemplateRenderSpec(spec, &resolved, error)) return NULL;
	buf = xrtBufferCreate();
	if (!buf) {
		if (error) *error = xrtStrDup("buffer alloc failed");
		xrtValueRelease(resolved.schema);
		xrtValueRelease(resolved.values);
		return NULL;
	}
	Form_BufferAppendText(buf, "<div class=\"xform-tpl-block\"><style>.xform-tpl-block{background:#fff;border:1px solid #e5e7eb;border-radius:10px;padding:20px;box-shadow:0 2px 12px rgba(15,23,42,.04)}.xform-tpl-title{font-size:22px;font-weight:700;color:#172554;margin-bottom:8px}.xform-tpl-desc{color:#667085;line-height:1.7;margin-bottom:18px}.xform-tpl-group{border:1px solid #eef2f7;border-radius:8px;margin-bottom:16px;overflow:hidden}.xform-tpl-group-title{padding:12px 16px;font-size:16px;font-weight:700;background:#f8fbff;border-bottom:1px solid #eef2f7}.xform-tpl-group-desc{padding:0 16px 12px;color:#667085;font-size:13px}.xform-tpl-group-fields{padding:16px}.xform-tpl-field{display:flex;gap:18px;margin-bottom:16px;align-items:flex-start}.xform-tpl-field:last-child{margin-bottom:0}.xform-tpl-label{width:180px;max-width:180px;font-weight:600;color:#344054;line-height:40px}.xform-tpl-control{flex:1}.xform-tpl-required{color:#ef4444;margin-left:4px}.xform-tpl-field-desc,.xform-tpl-group-desc{color:#98a2b3}.xform-tpl-input,.xform-tpl-select,.xform-tpl-textarea{width:100%;border:1px solid #d0d5dd;border-radius:8px;padding:10px 12px;font-size:14px;box-sizing:border-box;background:#fff}.xform-tpl-textarea{min-height:120px;line-height:1.7;resize:vertical}.xform-tpl-choice-list{display:flex;flex-wrap:wrap;gap:14px}.xform-tpl-choice{display:flex;align-items:center;gap:6px;color:#344054}.xform-tpl-switch{display:inline-flex;align-items:center;gap:8px;color:#344054}.xform-tpl-icon{display:inline-flex;align-items:center;min-height:40px;padding:0 12px;border:1px solid #d0d5dd;border-radius:8px;color:#344054;background:#fff;font-family:monospace}</style>");
	title = ValueText(resolved.schema, "title");
	desc = ValueText(resolved.schema, "desc");
	if (Form_HasNonSpaceText(title)) {
		Form_BufferAppendText(buf, "<div class=\"xform-tpl-title\">");
		Form_BufferAppendEscaped(buf, title);
		Form_BufferAppendText(buf, "</div>");
	}
	if (Form_HasNonSpaceText(desc)) {
		Form_BufferAppendText(buf, "<div class=\"xform-tpl-desc\">");
		Form_BufferAppendEscaped(buf, desc);
		Form_BufferAppendText(buf, "</div>");
	}
	Form_RenderTemplateGroups(buf, resolved.schema, resolved.values);
	Form_BufferAppendText(buf, "</div>");
	/* xbuffer 不保证 NUL 终止：补零后再转 C 串，防止 strlen 越界读堆
	 * （越界垃圾进 JSON 会使 xrtJsonStringify 静默失败返回 NULL） */
	xrtBufferAppendByte(buf, 0);
	result = buf->Data ? xrtStrDup((const char*)buf->Data) : xrtStrDup("");
	xrtBufferDestroy(buf);
	xrtValueRelease(resolved.schema);
	xrtValueRelease(resolved.values);
	return result;
}

/* ---- 模板扩展注册（{{#form}} 原始块） ---- */

static bool Form_TemplateBlockCall(xtemplatecall* call)
{
	xstrview raw = xrtTemplateCallRaw(call);
	xvalue* spec = raw.Size ? xrtJsonParse(xrtStrViewN(raw.Data, raw.Size)) : NULL;
	char* html; str error = NULL; bool ok;
	if (!spec) return false;
	html = Form_RenderTemplateBlockHTML(spec, &error);
	xrtValueRelease(spec);
	if (!html) {
		printf("[template][form] block render failed: %s\n", error ? error : "unknown");
		xrtFree(error);
		return false;
	}
	ok = xrtTemplateCallWrite(call, xrtStrView(html));
	xrtFree(html);
	return ok;
}

static xtemplateextension G_FormTemplateExtension = {
	XRT_STR_LITERAL("form"),
	XTEMPLATE_EXTENSION_RAW_BLOCK,
	0,
	0,
	Form_TemplateBlockCall,
	NULL,
	NULL
};

static void Form_TemplateRegistryInit(void)
{
	G_TemplateRegistry = xrtTemplateRegistryCreate(&G_FormTemplateExtension, 1);
	if (!G_TemplateRegistry) printf("[template][form] extension registry create failed\n");
}

static void Form_TemplateRegistryUnit(void)
{
	if (G_TemplateRegistry) {
		xrtTemplateRegistryRelease(G_TemplateRegistry);
		G_TemplateRegistry = NULL;
	}
}
