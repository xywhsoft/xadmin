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
	return xrtPathJoin(3, AppPath, "data", "forms");
}

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
	str sBasePath;
	str sFilePath;

	if ( !Form_IsValidFileName(sFileName) ) {
		return NULL;
	}

	sBasePath = Form_GetBasePath();
	if ( sBasePath == NULL ) {
		return NULL;
	}

	sFilePath = xrtPathJoin(2, sBasePath, (str)sFileName);
	if ( sBasePath != G_FormPath ) {
		xrtFree(sBasePath);
	}
	return sFilePath;
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

typedef struct FormTemplateHtmlRenderResult
{
	xvalue tblSchema;
	xvalue tblValues;
} FormTemplateHtmlRenderResult;

static void Form_BufferAppendText(xbuffer pBuf, const char* sText)
{
	if ( (pBuf == NULL) || (sText == NULL) ) {
		return;
	}
	xrtBufferAppend(pBuf, (ptr)sText, (uint32)strlen(sText), XBUF_ANSI);
}

static void Form_BufferAppendChar(xbuffer pBuf, char cChar)
{
	if ( pBuf == NULL ) {
		return;
	}
	xrtBufferAppend(pBuf, &cChar, 1, XBUF_BINARY);
}

static void Form_BufferAppendEscaped(xbuffer pBuf, const char* sText)
{
	const char* sRead;

	if ( (pBuf == NULL) || (sText == NULL) ) {
		return;
	}

	for ( sRead = sText; *sRead != '\0'; sRead++ ) {
		switch ( *sRead ) {
			case '&': Form_BufferAppendText(pBuf, "&amp;"); break;
			case '<': Form_BufferAppendText(pBuf, "&lt;"); break;
			case '>': Form_BufferAppendText(pBuf, "&gt;"); break;
			case '"': Form_BufferAppendText(pBuf, "&quot;"); break;
			case '\'': Form_BufferAppendText(pBuf, "&#39;"); break;
			default: Form_BufferAppendChar(pBuf, *sRead); break;
		}
	}
}

static char* Form_ValueToTextDup(xvalue objValue)
{
	int iType;

	if ( objValue == NULL ) {
		return xrtCopyStr("", 0);
	}

	iType = xvoType(objValue);
	switch ( iType ) {
		case XVO_DT_TEXT:
			return xrtCopyStr(xvoGetText(objValue), 0);
		case XVO_DT_BOOL:
			return xrtCopyStr(xvoGetBool(objValue) ? "true" : "false", 0);
		case XVO_DT_INT:
			return xrtFormat("%lld", (long long)xvoGetInt(objValue));
		case XVO_DT_FLOAT:
			return xrtFormat("%g", xvoGetFloat(objValue));
		case XVO_DT_NULL:
			return xrtCopyStr("", 0);
		default:
			return xrtStringifyJSON(objValue, FALSE, NULL);
	}
}

static xvalue Form_GetTemplateFieldValue(xvalue tblValues, xvalue tblField)
{
	str sName;
	xvalue objValue;

	if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
		return NULL;
	}

	sName = xvoTableGetText(tblField, "name", 4);
	if ( Form_HasNonSpaceText(sName) && (tblValues != NULL) && (xvoType(tblValues) == XVO_DT_TABLE) ) {
		objValue = xvoTableGetValue(tblValues, sName, 0);
		if ( objValue != NULL ) {
			return objValue;
		}
	}

	objValue = xvoTableGetValue(tblField, "value", 5);
	if ( objValue != NULL ) {
		return objValue;
	}

	return xvoTableGetValue(tblField, "default", 7);
}

static void Form_RenderTemplateFieldDesc(xbuffer pBuf, xvalue tblField)
{
	str sDesc = xvoTableGetText(tblField, "desc", 4);
	if ( Form_HasNonSpaceText(sDesc) ) {
		Form_BufferAppendText(pBuf, "<div class=\"xform-tpl-field-desc\">");
		Form_BufferAppendEscaped(pBuf, sDesc);
		Form_BufferAppendText(pBuf, "</div>");
	}
}

static void Form_RenderTemplateOptionList(xbuffer pBuf, xvalue arrList, const char* sCurrent)
{
	uint32 iCount;

	if ( (arrList == NULL) || (xvoType(arrList) != XVO_DT_ARRAY) ) {
		return;
	}

	iCount = xvoArrayItemCount(arrList);
	for ( uint32 i = 0; i < iCount; i++ ) {
		xvalue tblItem = xvoArrayGetValue(arrList, i);
		str sValue;
		str sLabel;
		bool bSelected;

		if ( (tblItem == NULL) || (xvoType(tblItem) != XVO_DT_TABLE) ) {
			continue;
		}

		sValue = xvoTableGetText(tblItem, "value", 5);
		sLabel = xvoTableGetText(tblItem, "label", 5);
		if ( !Form_HasNonSpaceText(sLabel) ) {
			sLabel = sValue;
		}
		bSelected = (sCurrent != NULL) && (sValue != NULL) && (strcmp(sCurrent, sValue) == 0);

		Form_BufferAppendText(pBuf, "<option value=\"");
		Form_BufferAppendEscaped(pBuf, Form_CStrOr(sValue, ""));
		Form_BufferAppendText(pBuf, "\"");
		if ( bSelected ) {
			Form_BufferAppendText(pBuf, " selected");
		}
		Form_BufferAppendText(pBuf, ">");
		Form_BufferAppendEscaped(pBuf, Form_CStrOr(sLabel, ""));
		Form_BufferAppendText(pBuf, "</option>");
	}
}

static void Form_RenderTemplateChoices(xbuffer pBuf, xvalue tblField, xvalue arrList, xvalue objValue, const char* sInputType)
{
	uint32 iCount;
	char* sCurrentText;

	if ( (arrList == NULL) || (xvoType(arrList) != XVO_DT_ARRAY) ) {
		return;
	}

	sCurrentText = Form_ValueToTextDup(objValue);
	iCount = xvoArrayItemCount(arrList);
	for ( uint32 i = 0; i < iCount; i++ ) {
		xvalue tblItem = xvoArrayGetValue(arrList, i);
		str sValue;
		str sLabel;
		bool bChecked = FALSE;

		if ( (tblItem == NULL) || (xvoType(tblItem) != XVO_DT_TABLE) ) {
			continue;
		}

		sValue = xvoTableGetText(tblItem, "value", 5);
		sLabel = xvoTableGetText(tblItem, "label", 5);
		if ( !Form_HasNonSpaceText(sLabel) ) {
			sLabel = sValue;
		}

		if ( strcmp(sInputType, "radio") == 0 ) {
			bChecked = (sCurrentText != NULL) && (sValue != NULL) && (strcmp(sCurrentText, sValue) == 0);
		} else if ( objValue && (xvoType(objValue) == XVO_DT_ARRAY) ) {
			for ( uint32 j = 0; j < xvoArrayItemCount(objValue); j++ ) {
				xvalue objItem = xvoArrayGetValue(objValue, j);
				char* sItemValue = Form_ValueToTextDup(objItem);
				if ( (sItemValue != NULL) && (sValue != NULL) && (strcmp(sItemValue, sValue) == 0) ) {
					bChecked = TRUE;
				}
				xrtFree(sItemValue);
				if ( bChecked ) {
					break;
				}
			}
		}

		Form_BufferAppendText(pBuf, "<label class=\"xform-tpl-choice\"><input type=\"");
		Form_BufferAppendText(pBuf, sInputType);
		Form_BufferAppendText(pBuf, "\" name=\"");
		Form_BufferAppendEscaped(pBuf, xvoTableGetText(tblField, "name", 4));
		Form_BufferAppendText(pBuf, "\" value=\"");
		Form_BufferAppendEscaped(pBuf, Form_CStrOr(sValue, ""));
		Form_BufferAppendText(pBuf, "\"");
		if ( bChecked ) {
			Form_BufferAppendText(pBuf, " checked");
		}
		Form_BufferAppendText(pBuf, "><span>");
		Form_BufferAppendEscaped(pBuf, Form_CStrOr(sLabel, ""));
		Form_BufferAppendText(pBuf, "</span></label>");
	}

	if ( sCurrentText ) {
		xrtFree(sCurrentText);
	}
}

static char* Form_GetRangeItemText(xvalue objValue, uint32 iIndex)
{
	xvalue objItem;

	if ( objValue == NULL ) {
		return xrtCopyStr("", 0);
	}

	if ( xvoType(objValue) == XVO_DT_ARRAY ) {
		objItem = xvoArrayGetValue(objValue, iIndex);
		return Form_ValueToTextDup(objItem);
	}

	return xrtCopyStr("", 0);
}

static void Form_RenderTemplateRangeInput(xbuffer pBuf, xvalue tblField, xvalue objValue, const char* sInputType, const char* sStep)
{
	str sName = xvoTableGetText(tblField, "name", 4);
	xvalue tblProps = xvoTableGetValue(tblField, "props", 5);
	str sMin = (tblProps && xvoType(tblProps) == XVO_DT_TABLE) ? xvoTableGetText(tblProps, "min", 3) : NULL;
	str sMax = (tblProps && xvoType(tblProps) == XVO_DT_TABLE) ? xvoTableGetText(tblProps, "max", 3) : NULL;
	char* sStart = Form_GetRangeItemText(objValue, 0);
	char* sEnd = Form_GetRangeItemText(objValue, 1);

	Form_BufferAppendText(pBuf, "<div class=\"xform-tpl-range\">");
	Form_BufferAppendText(pBuf, "<input class=\"xform-tpl-input\" type=\"");
	Form_BufferAppendText(pBuf, sInputType);
	Form_BufferAppendText(pBuf, "\" name=\"");
	Form_BufferAppendEscaped(pBuf, Form_CStrOr(sName, ""));
	Form_BufferAppendText(pBuf, "_start\" value=\"");
	Form_BufferAppendEscaped(pBuf, Form_CStrOr(sStart, ""));
	Form_BufferAppendText(pBuf, "\"");
	if ( Form_HasNonSpaceText(sMin) ) {
		Form_BufferAppendText(pBuf, " min=\"");
		Form_BufferAppendEscaped(pBuf, sMin);
		Form_BufferAppendText(pBuf, "\"");
	}
	if ( Form_HasNonSpaceText(sMax) ) {
		Form_BufferAppendText(pBuf, " max=\"");
		Form_BufferAppendEscaped(pBuf, sMax);
		Form_BufferAppendText(pBuf, "\"");
	}
	if ( Form_HasNonSpaceText(sStep) ) {
		Form_BufferAppendText(pBuf, " step=\"");
		Form_BufferAppendEscaped(pBuf, sStep);
		Form_BufferAppendText(pBuf, "\"");
	}
	Form_BufferAppendText(pBuf, ">");
	Form_BufferAppendText(pBuf, "<span class=\"xform-tpl-range-sep\">to</span>");
	Form_BufferAppendText(pBuf, "<input class=\"xform-tpl-input\" type=\"");
	Form_BufferAppendText(pBuf, sInputType);
	Form_BufferAppendText(pBuf, "\" name=\"");
	Form_BufferAppendEscaped(pBuf, Form_CStrOr(sName, ""));
	Form_BufferAppendText(pBuf, "_end\" value=\"");
	Form_BufferAppendEscaped(pBuf, Form_CStrOr(sEnd, ""));
	Form_BufferAppendText(pBuf, "\"");
	if ( Form_HasNonSpaceText(sMin) ) {
		Form_BufferAppendText(pBuf, " min=\"");
		Form_BufferAppendEscaped(pBuf, sMin);
		Form_BufferAppendText(pBuf, "\"");
	}
	if ( Form_HasNonSpaceText(sMax) ) {
		Form_BufferAppendText(pBuf, " max=\"");
		Form_BufferAppendEscaped(pBuf, sMax);
		Form_BufferAppendText(pBuf, "\"");
	}
	if ( Form_HasNonSpaceText(sStep) ) {
		Form_BufferAppendText(pBuf, " step=\"");
		Form_BufferAppendEscaped(pBuf, sStep);
		Form_BufferAppendText(pBuf, "\"");
	}
	Form_BufferAppendText(pBuf, ">");
	Form_BufferAppendText(pBuf, "</div>");

	if ( sStart ) {
		xrtFree(sStart);
	}
	if ( sEnd ) {
		xrtFree(sEnd);
	}
}

static void Form_RenderTemplateFieldInput(xbuffer pBuf, xvalue tblField, xvalue objValue)
{
	str sType = xvoTableGetText(tblField, "type", 4);
	str sName = xvoTableGetText(tblField, "name", 4);
	xvalue arrList = xvoTableGetValue(tblField, "list", 4);
	xvalue tblProps = xvoTableGetValue(tblField, "props", 5);
	char* sValueText = Form_ValueToTextDup(objValue);
	str sHeight = (tblProps && xvoType(tblProps) == XVO_DT_TABLE) ? xvoTableGetText(tblProps, "height", 6) : NULL;

	if ( !Form_HasNonSpaceText(sType) ) {
		sType = "text";
	}

	if ( (strcmp(sType, "textarea") == 0) || (strcmp(sType, "editor_html") == 0) || (strcmp(sType, "editor_md") == 0) || (strcmp(sType, "editor_code") == 0) ) {
		Form_BufferAppendText(pBuf, "<textarea class=\"xform-tpl-textarea\" name=\"");
		Form_BufferAppendEscaped(pBuf, Form_CStrOr(sName, ""));
		Form_BufferAppendText(pBuf, "\"");
		if ( Form_HasNonSpaceText(sHeight) ) {
			Form_BufferAppendText(pBuf, " style=\"height:");
			Form_BufferAppendEscaped(pBuf, sHeight);
			Form_BufferAppendText(pBuf, ";\"");
		}
		Form_BufferAppendText(pBuf, ">");
		Form_BufferAppendEscaped(pBuf, sValueText ? sValueText : "");
		Form_BufferAppendText(pBuf, "</textarea>");
	} else if ( strcmp(sType, "select") == 0 ) {
		Form_BufferAppendText(pBuf, "<select class=\"xform-tpl-select\" name=\"");
		Form_BufferAppendEscaped(pBuf, Form_CStrOr(sName, ""));
		Form_BufferAppendText(pBuf, "\"><option value=\"\">Please select</option>");
		Form_RenderTemplateOptionList(pBuf, arrList, sValueText);
		Form_BufferAppendText(pBuf, "</select>");
	} else if ( strcmp(sType, "combobox") == 0 ) {
		Form_BufferAppendText(pBuf, "<input class=\"xform-tpl-input\" type=\"text\" list=\"list_");
		Form_BufferAppendEscaped(pBuf, Form_CStrOr(sName, ""));
		Form_BufferAppendText(pBuf, "\" name=\"");
		Form_BufferAppendEscaped(pBuf, Form_CStrOr(sName, ""));
		Form_BufferAppendText(pBuf, "\" value=\"");
		Form_BufferAppendEscaped(pBuf, sValueText ? sValueText : "");
		Form_BufferAppendText(pBuf, "\"><datalist id=\"list_");
		Form_BufferAppendEscaped(pBuf, Form_CStrOr(sName, ""));
		Form_BufferAppendText(pBuf, "\">");
		if ( (arrList != NULL) && (xvoType(arrList) == XVO_DT_ARRAY) ) {
			for ( uint32 i = 0; i < xvoArrayItemCount(arrList); i++ ) {
				xvalue tblItem = xvoArrayGetValue(arrList, i);
				str sValue;
				if ( (tblItem == NULL) || (xvoType(tblItem) != XVO_DT_TABLE) ) {
					continue;
				}
				sValue = xvoTableGetText(tblItem, "value", 5);
				Form_BufferAppendText(pBuf, "<option value=\"");
				Form_BufferAppendEscaped(pBuf, Form_CStrOr(sValue, ""));
				Form_BufferAppendText(pBuf, "\"></option>");
			}
		}
		Form_BufferAppendText(pBuf, "</datalist>");
	} else if ( strcmp(sType, "radio") == 0 ) {
		Form_BufferAppendText(pBuf, "<div class=\"xform-tpl-choice-list\">");
		Form_RenderTemplateChoices(pBuf, tblField, arrList, objValue, "radio");
		Form_BufferAppendText(pBuf, "</div>");
	} else if ( (strcmp(sType, "checkbox") == 0) || (strcmp(sType, "checklist") == 0) ) {
		Form_BufferAppendText(pBuf, "<div class=\"xform-tpl-choice-list\">");
		Form_RenderTemplateChoices(pBuf, tblField, arrList, objValue, "checkbox");
		Form_BufferAppendText(pBuf, "</div>");
	} else if ( strcmp(sType, "switch") == 0 ) {
		Form_BufferAppendText(pBuf, "<label class=\"xform-tpl-switch\"><input type=\"checkbox\" name=\"");
		Form_BufferAppendEscaped(pBuf, Form_CStrOr(sName, ""));
		Form_BufferAppendText(pBuf, "\"");
		if ( xvoGetBool(objValue) ) {
			Form_BufferAppendText(pBuf, " checked");
		}
		Form_BufferAppendText(pBuf, "><span>");
		Form_BufferAppendText(pBuf, xvoGetBool(objValue) ? "ON" : "OFF");
		Form_BufferAppendText(pBuf, "</span></label>");
	} else if ( strcmp(sType, "icon_picker") == 0 ) {
		Form_BufferAppendText(pBuf, "<div class=\"xform-tpl-icon\">");
		Form_BufferAppendEscaped(pBuf, sValueText ? sValueText : "");
		Form_BufferAppendText(pBuf, "</div>");
	} else if ( strcmp(sType, "intrange") == 0 ) {
		Form_RenderTemplateRangeInput(pBuf, tblField, objValue, "number", "1");
	} else if ( strcmp(sType, "numrange") == 0 ) {
		Form_RenderTemplateRangeInput(pBuf, tblField, objValue, "number", "0.01");
	} else if ( strcmp(sType, "daterange") == 0 ) {
		Form_RenderTemplateRangeInput(pBuf, tblField, objValue, "date", NULL);
	} else if ( strcmp(sType, "timerange") == 0 ) {
		Form_RenderTemplateRangeInput(pBuf, tblField, objValue, "time", NULL);
	} else if ( strcmp(sType, "datetimerange") == 0 ) {
		Form_RenderTemplateRangeInput(pBuf, tblField, objValue, "datetime-local", NULL);
	} else {
		const char* sInputType = "text";
		if ( (strcmp(sType, "number") == 0) || (strcmp(sType, "int") == 0) || (strcmp(sType, "num") == 0) ) sInputType = "number";
		else if ( strcmp(sType, "password") == 0 ) sInputType = "password";
		else if ( strcmp(sType, "date") == 0 ) sInputType = "date";
		else if ( strcmp(sType, "datetime") == 0 ) sInputType = "datetime-local";
		else if ( strcmp(sType, "time") == 0 ) sInputType = "time";

		Form_BufferAppendText(pBuf, "<input class=\"xform-tpl-input\" type=\"");
		Form_BufferAppendText(pBuf, sInputType);
		Form_BufferAppendText(pBuf, "\" name=\"");
		Form_BufferAppendEscaped(pBuf, Form_CStrOr(sName, ""));
		Form_BufferAppendText(pBuf, "\" value=\"");
		Form_BufferAppendEscaped(pBuf, sValueText ? sValueText : "");
		Form_BufferAppendText(pBuf, "\">");
	}

	if ( sValueText ) {
		xrtFree(sValueText);
	}
}

static void Form_RenderTemplateField(xbuffer pBuf, xvalue tblField, xvalue tblValues)
{
	str sLabel = xvoTableGetText(tblField, "label", 5);
	xvalue objValue = Form_GetTemplateFieldValue(tblValues, tblField);

	if ( !Form_HasNonSpaceText(sLabel) ) {
		sLabel = xvoTableGetText(tblField, "name", 4);
	}

	Form_BufferAppendText(pBuf, "<div class=\"xform-tpl-field\"><label class=\"xform-tpl-label\">");
		Form_BufferAppendEscaped(pBuf, Form_CStrOr(sLabel, ""));
	if ( xvoTableGetBool(tblField, "required", 8) ) {
		Form_BufferAppendText(pBuf, "<span class=\"xform-tpl-required\">*</span>");
	}
	Form_BufferAppendText(pBuf, "</label><div class=\"xform-tpl-control\">");
	Form_RenderTemplateFieldInput(pBuf, tblField, objValue);
	Form_RenderTemplateFieldDesc(pBuf, tblField);
	Form_BufferAppendText(pBuf, "</div></div>");
}

static void Form_RenderTemplateGroups(xbuffer pBuf, xvalue tblSchema, xvalue tblValues)
{
	xvalue arrGroups = xvoTableGetValue(tblSchema, "groups", 6);

	if ( (arrGroups == NULL) || (xvoType(arrGroups) != XVO_DT_ARRAY) ) {
		return;
	}

	for ( uint32 i = 0; i < xvoArrayItemCount(arrGroups); i++ ) {
		xvalue tblGroup = xvoArrayGetValue(arrGroups, i);
		xvalue arrFields;
		str sTitle;
		str sDesc;

		if ( (tblGroup == NULL) || (xvoType(tblGroup) != XVO_DT_TABLE) ) {
			continue;
		}

		arrFields = xvoTableGetValue(tblGroup, "fields", 6);
		if ( (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) ) {
			continue;
		}

		sTitle = xvoTableGetText(tblGroup, "title", 5);
		sDesc = xvoTableGetText(tblGroup, "desc", 4);
		Form_BufferAppendText(pBuf, "<section class=\"xform-tpl-group\">");
		if ( Form_HasNonSpaceText(sTitle) ) {
			Form_BufferAppendText(pBuf, "<div class=\"xform-tpl-group-title\">");
			Form_BufferAppendEscaped(pBuf, sTitle);
			Form_BufferAppendText(pBuf, "</div>");
		}
		if ( Form_HasNonSpaceText(sDesc) ) {
			Form_BufferAppendText(pBuf, "<div class=\"xform-tpl-group-desc\">");
			Form_BufferAppendEscaped(pBuf, sDesc);
			Form_BufferAppendText(pBuf, "</div>");
		}
		Form_BufferAppendText(pBuf, "<div class=\"xform-tpl-group-fields\">");
		for ( uint32 j = 0; j < xvoArrayItemCount(arrFields); j++ ) {
			xvalue tblField = xvoArrayGetValue(arrFields, j);
			if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
				continue;
			}
			Form_RenderTemplateField(pBuf, tblField, tblValues);
		}
		Form_BufferAppendText(pBuf, "</div></section>");
	}
}

static bool Form_ResolveTemplateRenderSpec(xvalue tblSpec, FormTemplateHtmlRenderResult* pOut, str* psError)
{
	xvalue tblLoaded = NULL;
	xvalue tblValues = NULL;
	xvalue tblSchema = NULL;
	str sFile;
	str sSource;
	xvalue objOverrideValues;

	if ( pOut == NULL ) {
		return FALSE;
	}

	memset(pOut, 0, sizeof(*pOut));
	if ( (tblSpec == NULL) || (xvoType(tblSpec) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("form block json must be an object", 0);
		return FALSE;
	}

	sFile = xvoTableGetText(tblSpec, "file", 4);
	sSource = xvoTableGetText(tblSpec, "source", 6);

	if ( Form_HasNonSpaceText(sFile) ) {
		if ( strcmp(Form_CStrOr(sSource, "form"), "option") == 0 ) {
			tblLoaded = Option_LoadFile(sFile);
			if ( tblLoaded == NULL ) {
				if ( psError ) *psError = xrtFormat("option file not found: %s", sFile);
				return FALSE;
			}
			tblSchema = Form_CreateSchemaFromOptionConfig(tblLoaded, &tblValues);
			xvoUnref(tblLoaded);
			if ( tblSchema == NULL ) {
				if ( psError ) *psError = xrtFormat("option schema build failed: %s", sFile);
				return FALSE;
			}
		} else {
			tblLoaded = Form_LoadFile(sFile);
			if ( tblLoaded == NULL ) {
				if ( psError ) *psError = xrtFormat("form file not found: %s", sFile);
				return FALSE;
			}
			tblSchema = xvoCopy(tblLoaded);
			objOverrideValues = xvoTableGetValue(tblLoaded, "demoData", 8);
			tblValues = objOverrideValues ? xvoCopy(objOverrideValues) : xvoCreateTable();
			xvoUnref(tblLoaded);
		}
	} else {
		tblSchema = xvoCopy(tblSpec);
		tblValues = xvoCreateTable();
	}

	objOverrideValues = xvoTableGetValue(tblSpec, "values", 6);
	if ( objOverrideValues == NULL ) {
		objOverrideValues = xvoTableGetValue(tblSpec, "data", 4);
	}
	if ( objOverrideValues && (xvoType(objOverrideValues) == XVO_DT_TABLE) ) {
		if ( tblValues ) {
			xvoUnref(tblValues);
		}
		tblValues = xvoCopy(objOverrideValues);
	}

	pOut->tblSchema = tblSchema;
	pOut->tblValues = tblValues;
	return TRUE;
}

static char* Form_RenderTemplateBlockHTML(xvalue tblSpec, str* psError)
{
	FormTemplateHtmlRenderResult tResolved;
	xbuffer pBuf;
	str sTitle;
	str sDesc;
	char* sResult = NULL;

	if ( !Form_ResolveTemplateRenderSpec(tblSpec, &tResolved, psError) ) {
		return NULL;
	}

	pBuf = xrtBufferCreate(4096);
	if ( pBuf == NULL ) {
		if ( psError ) *psError = xrtCopyStr("buffer alloc failed", 0);
		xvoUnref(tResolved.tblSchema);
		xvoUnref(tResolved.tblValues);
		return NULL;
	}

	Form_BufferAppendText(pBuf, "<div class=\"xform-tpl-block\"><style>.xform-tpl-block{background:#fff;border:1px solid #e5e7eb;border-radius:10px;padding:20px;box-shadow:0 2px 12px rgba(15,23,42,.04)}.xform-tpl-title{font-size:22px;font-weight:700;color:#172554;margin-bottom:8px}.xform-tpl-desc{color:#667085;line-height:1.7;margin-bottom:18px}.xform-tpl-group{border:1px solid #eef2f7;border-radius:8px;margin-bottom:16px;overflow:hidden}.xform-tpl-group-title{padding:12px 16px;font-size:16px;font-weight:700;background:#f8fbff;border-bottom:1px solid #eef2f7}.xform-tpl-group-desc{padding:0 16px 12px;color:#667085;font-size:13px}.xform-tpl-group-fields{padding:16px}.xform-tpl-field{display:flex;gap:18px;margin-bottom:16px;align-items:flex-start}.xform-tpl-field:last-child{margin-bottom:0}.xform-tpl-label{width:180px;max-width:180px;font-weight:600;color:#344054;line-height:40px}.xform-tpl-control{flex:1}.xform-tpl-required{color:#ef4444;margin-left:4px}.xform-tpl-field-desc,.xform-tpl-group-desc{color:#98a2b3}.xform-tpl-input,.xform-tpl-select,.xform-tpl-textarea{width:100%;border:1px solid #d0d5dd;border-radius:8px;padding:10px 12px;font-size:14px;box-sizing:border-box;background:#fff}.xform-tpl-textarea{min-height:120px;line-height:1.7;resize:vertical}.xform-tpl-choice-list{display:flex;flex-wrap:wrap;gap:14px}.xform-tpl-choice{display:flex;align-items:center;gap:6px;color:#344054}.xform-tpl-switch{display:inline-flex;align-items:center;gap:8px;color:#344054}.xform-tpl-icon{display:inline-flex;align-items:center;min-height:40px;padding:0 12px;border:1px solid #d0d5dd;border-radius:8px;color:#344054;background:#fff;font-family:monospace}</style>");

	sTitle = xvoTableGetText(tResolved.tblSchema, "title", 5);
	sDesc = xvoTableGetText(tResolved.tblSchema, "desc", 4);
	if ( Form_HasNonSpaceText(sTitle) ) {
		Form_BufferAppendText(pBuf, "<div class=\"xform-tpl-title\">");
		Form_BufferAppendEscaped(pBuf, sTitle);
		Form_BufferAppendText(pBuf, "</div>");
	}
	if ( Form_HasNonSpaceText(sDesc) ) {
		Form_BufferAppendText(pBuf, "<div class=\"xform-tpl-desc\">");
		Form_BufferAppendEscaped(pBuf, sDesc);
		Form_BufferAppendText(pBuf, "</div>");
	}

	Form_RenderTemplateGroups(pBuf, tResolved.tblSchema, tResolved.tblValues);
	Form_BufferAppendText(pBuf, "</div>");
	Form_BufferAppendChar(pBuf, '\0');

	sResult = xrtCopyStr(pBuf->Buffer ? pBuf->Buffer : "", 0);
	xrtBufferDestroy(pBuf);
	xvoUnref(tResolved.tblSchema);
	xvoUnref(tResolved.tblValues);
	return sResult;
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
