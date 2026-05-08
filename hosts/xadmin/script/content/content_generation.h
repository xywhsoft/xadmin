bool Content_LoadModelForGeneration(const char* sXid, int* piModelId, int* piRevision, str* psGeneratedPluginXid, str* psTitle, str* psSpecJson, char** psError)
{
	sqlite3_stmt* stmt = NULL;
	int iModelId = 0;

	if ( psError ) *psError = NULL;
	if ( !Content_IsValidXid(sXid) ) {
		if ( psError ) *psError = xrtCopyStr("invalid xid", 0);
		return FALSE;
	}

	if ( sqlite3_prepare_v3(G_DB, "SELECT id,current_revision,generated_plugin_xid,title,spec_json FROM content_model WHERE xid=? AND status <> 'deleted' LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		if ( psError ) *psError = xrtCopyStr("query model failed", 0);
		return FALSE;
	}
	Content_BindText(stmt, 1, sXid);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iModelId = sqlite3_column_int(stmt, 0);
		if ( piModelId ) *piModelId = iModelId;
		if ( piRevision ) *piRevision = sqlite3_column_int(stmt, 1);
		if ( psGeneratedPluginXid ) *psGeneratedPluginXid = xrtCopyStr((str)sqlite3_column_text(stmt, 2), 0);
		if ( psTitle ) *psTitle = xrtCopyStr((str)sqlite3_column_text(stmt, 3), 0);
		if ( psSpecJson ) *psSpecJson = xrtCopyStr((str)sqlite3_column_text(stmt, 4), 0);
	}
	sqlite3_finalize(stmt);

	if ( iModelId <= 0 ) {
		if ( psError ) *psError = xrtCopyStr("model not found", 0);
		return FALSE;
	}

	if ( psGeneratedPluginXid && ((*psGeneratedPluginXid == NULL) || ((*psGeneratedPluginXid)[0] == '\0')) ) {
		if ( *psGeneratedPluginXid ) xrtFree(*psGeneratedPluginXid);
		*psGeneratedPluginXid = xrtCopyStr((str)sXid, 0);
	}
	return TRUE;
}

void Content_RecordGenerationSuccess(int iModelId, int iRevision, const char* sGeneratedPluginXid, const char* sOutputJson, const char* sAdvisorJson, int64 iCreateTime)
{
	sqlite3_stmt* stmt = NULL;

	if ( sqlite3_prepare_v3(G_DB, "INSERT INTO content_generation (model_id,target_revision,plugin_xid,status,output_json,advisor_json,error_message,create_time,finish_time) VALUES (?,?,?,'success',?,?,'',?,?)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iModelId);
		sqlite3_bind_int(stmt, 2, iRevision);
		Content_BindText(stmt, 3, sGeneratedPluginXid);
		Content_BindText(stmt, 4, sOutputJson);
		Content_BindText(stmt, 5, sAdvisorJson ? sAdvisorJson : "{}");
		sqlite3_bind_int64(stmt, 6, iCreateTime);
		sqlite3_bind_int64(stmt, 7, xrtNow());
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
}

void Content_RecordGenerationFailure(int iModelId, int iRevision, const char* sGeneratedPluginXid, const char* sAdvisorJson, const char* sErrorMessage, int64 iCreateTime)
{
	sqlite3_stmt* stmt = NULL;

	if ( (G_DB == NULL) || (iModelId <= 0) ) {
		return;
	}

	if ( sqlite3_prepare_v3(G_DB, "INSERT INTO content_generation (model_id,target_revision,plugin_xid,status,output_json,advisor_json,error_message,create_time,finish_time) VALUES (?,?,?,'failed','{}',?,?,?)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iModelId);
		sqlite3_bind_int(stmt, 2, iRevision);
		Content_BindText(stmt, 3, sGeneratedPluginXid);
		Content_BindText(stmt, 4, sAdvisorJson ? sAdvisorJson : "{}");
		Content_BindText(stmt, 5, sErrorMessage ? sErrorMessage : "generate plugin failed");
		sqlite3_bind_int64(stmt, 6, iCreateTime);
		sqlite3_bind_int64(stmt, 7, xrtNow());
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
}

void Content_UpdateAppliedRevision(int iModelId, int iRevision, const char* sGeneratedPluginXid)
{
	sqlite3_stmt* stmt = NULL;

	if ( sqlite3_prepare_v3(G_DB, "UPDATE content_model SET generated_plugin_xid=?, applied_revision=?, update_time=? WHERE id=?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		Content_BindText(stmt, 1, sGeneratedPluginXid);
		sqlite3_bind_int(stmt, 2, iRevision);
		sqlite3_bind_int64(stmt, 3, xrtNow());
		sqlite3_bind_int(stmt, 4, iModelId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
}

void Content_SetGeneratedFile(XAdminGeneratedFile* pFile, const char* sRelativePath, const char* sData)
{
	const char* sSafeData = sData ? sData : "";

	if ( pFile == NULL ) {
		return;
	}
	pFile->relative_path = sRelativePath;
	pFile->data = sSafeData;
	pFile->size = strlen(sSafeData);
}

bool Content_SpecHasCapability(xvalue tblSpec, const char* sKey)
{
	xvalue arrCapabilities = tblSpec ? xvoTableGetValue(tblSpec, "capabilities", 12) : NULL;

	if ( (sKey == NULL) || (arrCapabilities == NULL) || (xvoType(arrCapabilities) != XVO_DT_ARRAY) ) {
		return FALSE;
	}
	for ( uint32 i = 0; i < xvoArrayItemCount(arrCapabilities); i++ ) {
		xvalue tblCap = xvoArrayGetValue(arrCapabilities, i);
		const char* sCapKey;
		xvalue objEnabled;
		bool bEnabled = TRUE;

		if ( (tblCap == NULL) || (xvoType(tblCap) != XVO_DT_TABLE) ) {
			continue;
		}
		sCapKey = xvoTableGetText(tblCap, "key", 3);
		objEnabled = xvoTableGetValue(tblCap, "enabled", 7);
		if ( objEnabled && (xvoType(objEnabled) == XVO_DT_BOOL) ) {
			bEnabled = xvoGetBool(objEnabled) ? TRUE : FALSE;
		}
		if ( bEnabled && sCapKey && (strcmp(sCapKey, sKey) == 0) ) {
			return TRUE;
		}
	}
	return FALSE;
}

bool Content_TableBoolDefault(xvalue tbl, const char* sKey, int iKeyLen, bool bDefault)
{
	xvalue objValue = tbl ? xvoTableGetValue(tbl, sKey, iKeyLen) : NULL;
	if ( objValue == NULL ) {
		return bDefault;
	}
	return xvoGetBool(objValue);
}

const char* Content_FieldStorageType(const char* sType)
{
	if ( sType == NULL ) return "text";
	if ( strcmp(sType, "integer") == 0 || strcmp(sType, "int") == 0 ) return "integer";
	if ( strcmp(sType, "number") == 0 || strcmp(sType, "float") == 0 || strcmp(sType, "double") == 0 ) return "real";
	if ( strcmp(sType, "bool") == 0 || strcmp(sType, "boolean") == 0 || strcmp(sType, "switch") == 0 ) return "integer";
	return "text";
}

const char* Content_FieldComponentType(const char* sType, const char* sOptions)
{
	if ( sType && strcmp(sType, "textarea") == 0 ) return "textarea";
	if ( sType && strcmp(sType, "text") == 0 ) return "text";
	if ( sType && strcmp(sType, "number") == 0 ) return "number";
	if ( sType && strcmp(sType, "int") == 0 ) return "int";
	if ( sType && strcmp(sType, "integer") == 0 ) return "int";
	if ( sType && strcmp(sType, "decimal") == 0 ) return "number";
	if ( sType && strcmp(sType, "password") == 0 ) return "password";
	if ( sType && strcmp(sType, "select") == 0 ) return "select";
	if ( sType && strcmp(sType, "combobox") == 0 ) return "combobox";
	if ( sType && strcmp(sType, "radio") == 0 ) return "radio";
	if ( sType && strcmp(sType, "checkbox") == 0 ) return "checkbox";
	if ( sType && strcmp(sType, "checklist") == 0 ) return "checklist";
	if ( sType && strcmp(sType, "date") == 0 ) return "date";
	if ( sType && strcmp(sType, "datetime") == 0 ) return "datetime";
	if ( sType && strcmp(sType, "time") == 0 ) return "time";
	if ( sType && strcmp(sType, "intrange") == 0 ) return "intrange";
	if ( sType && strcmp(sType, "numrange") == 0 ) return "numrange";
	if ( sType && strcmp(sType, "daterange") == 0 ) return "daterange";
	if ( sType && strcmp(sType, "timerange") == 0 ) return "timerange";
	if ( sType && strcmp(sType, "datetimerange") == 0 ) return "datetimerange";
	if ( sType && strcmp(sType, "editor_md") == 0 ) return "editor_md";
	if ( sType && strcmp(sType, "editor_html") == 0 ) return "editor_html";
	if ( sType && strcmp(sType, "editor_code") == 0 ) return "editor_code";
	if ( sType && strcmp(sType, "icon_picker") == 0 ) return "icon_picker";
	if ( sType && strcmp(sType, "image") == 0 ) return "image";
	if ( sType && strcmp(sType, "images") == 0 ) return "images";
	if ( sType && strcmp(sType, "file") == 0 ) return "file";
	if ( sType && strcmp(sType, "files") == 0 ) return "files";
	if ( sType && strcmp(sType, "badge_picker") == 0 ) return "badge_picker";
	if ( sOptions && sOptions[0] ) return "select";
	if ( sType && (strcmp(sType, "bool") == 0 || strcmp(sType, "boolean") == 0 || strcmp(sType, "switch") == 0) ) return "switch";
	return "input";
}

const char* Content_FieldSemanticRole(const char* sName)
{
	if ( sName == NULL ) return "";
	if ( strcmp(sName, "title") == 0 ) return "title";
	if ( strcmp(sName, "status") == 0 ) return "status";
	if ( strcmp(sName, "slug") == 0 ) return "slug";
	if ( strcmp(sName, "summary") == 0 ) return "summary";
	if ( strcmp(sName, "cover") == 0 ) return "cover";
	if ( strcmp(sName, "publishedAt") == 0 || strcmp(sName, "published_at") == 0 ) return "publishedAt";
	return "";
}

xvalue Content_ParseOptionList(const char* sOptions)
{
	xvalue arrList = xvoCreateArray();
	str sCopy;
	char* p;
	char* sPart;

	if ( arrList == NULL ) {
		return NULL;
	}
	if ( (sOptions == NULL) || (sOptions[0] == '\0') ) {
		return arrList;
	}
	sCopy = xrtCopyStr((str)sOptions, 0);
	if ( sCopy == NULL ) {
		return arrList;
	}
	p = (char*)sCopy;
	while ( p && *p ) {
		char* sNext = strchr(p, ',');
		char* sSep;
		char* sValue;
		char* sLabel;
		xvalue tblItem;
		if ( sNext ) {
			*sNext = '\0';
			sNext++;
		}
		while ( *p == ' ' || *p == '\t' ) p++;
		sPart = p;
		sSep = strchr(sPart, ':');
		if ( sSep ) {
			*sSep = '\0';
			sLabel = sSep + 1;
		} else {
			sLabel = sPart;
		}
		sValue = sPart;
		while ( sValue[0] == ' ' || sValue[0] == '\t' ) sValue++;
		while ( sLabel[0] == ' ' || sLabel[0] == '\t' ) sLabel++;
		if ( sValue[0] ) {
			tblItem = xvoCreateTable();
			xvoTableSetText(tblItem, "value", 5, (str)sValue, 0, FALSE);
			xvoTableSetText(tblItem, "label", 5, (str)(sLabel[0] ? sLabel : sValue), 0, FALSE);
			xvoArrayAppendValue(arrList, tblItem, TRUE);
		}
		p = sNext;
	}
	xrtFree(sCopy);
	return arrList;
}

xvalue Content_BuildManagedSpecField(xvalue tblField)
{
	const char* sName = xvoTableGetText(tblField, "name", 4);
	const char* sType = xvoTableGetText(tblField, "type", 4);
	const char* sOptions = xvoTableGetText(tblField, "options", 7);
	const char* sRole = Content_FieldSemanticRole(sName);
	xvalue tblOut = xvoCopy(tblField);
	xvalue tblStorage = xvoCreateTable();
	xvalue tblComponent = xvoCreateTable();
	xvalue tblSemantic = xvoCreateTable();
	xvalue arrList = Content_ParseOptionList(sOptions);
	xvalue tblExistingStorage = xvoTableGetValue(tblField, "storage", 7);
	xvalue tblExistingComponent = xvoTableGetValue(tblField, "component", 9);
	xvalue tblExistingSemantic = xvoTableGetValue(tblField, "semantic", 8);

	if ( tblOut == NULL ) {
		tblOut = xvoCreateTable();
	}
	if ( tblExistingStorage && xvoType(tblExistingStorage) == XVO_DT_TABLE ) {
		xvoUnref(tblStorage);
		xvoTableSetValue(tblOut, "storage", 7, xvoCopy(tblExistingStorage), TRUE);
	} else {
		xvoTableSetText(tblStorage, "type", 4, (str)Content_FieldStorageType(sType), 0, FALSE);
		xvoTableSetValue(tblOut, "storage", 7, tblStorage, TRUE);
	}
	if ( tblExistingComponent && xvoType(tblExistingComponent) == XVO_DT_TABLE ) {
		xvoUnref(tblComponent);
		xvoTableSetValue(tblOut, "component", 9, xvoCopy(tblExistingComponent), TRUE);
	} else {
		xvoTableSetText(tblComponent, "type", 4, (str)Content_FieldComponentType(sType, sOptions), 0, FALSE);
		if ( arrList && xvoArrayItemCount(arrList) > 0 ) {
			xvoTableSetValue(tblComponent, "list", 4, xvoCopy(arrList), TRUE);
		}
		xvoTableSetValue(tblOut, "component", 9, tblComponent, TRUE);
	}
	if ( tblExistingSemantic && xvoType(tblExistingSemantic) == XVO_DT_TABLE ) {
		xvoUnref(tblSemantic);
		xvoTableSetValue(tblOut, "semantic", 8, xvoCopy(tblExistingSemantic), TRUE);
	} else if ( sRole[0] ) {
		xvoTableSetText(tblSemantic, "role", 4, (str)sRole, 0, FALSE);
		xvoTableSetValue(tblOut, "semantic", 8, tblSemantic, TRUE);
	} else {
		xvoUnref(tblSemantic);
	}
	if ( arrList && xvoArrayItemCount(arrList) > 0 && !xvoTableGetValue(tblOut, "options", 7) ) {
		xvoTableSetValue(tblOut, "list", 4, arrList, TRUE);
	} else if ( arrList ) {
		xvoUnref(arrList);
	}
	if ( xvoTableGetValue(tblOut, "showInForm", 10) == NULL ) {
		xvoTableSetBool(tblOut, "showInForm", 10, TRUE);
	}
	if ( xvoTableGetValue(tblOut, "showInList", 10) == NULL ) {
		xvoTableSetBool(tblOut, "showInList", 10, Content_TableBoolDefault(tblField, "list", 4, TRUE));
	}
	if ( xvoTableGetValue(tblOut, "showInDetail", 12) == NULL ) {
		xvoTableSetBool(tblOut, "showInDetail", 12, Content_TableBoolDefault(tblField, "detail", 6, TRUE));
	}
	return tblOut;
}

str Content_BuildManagedSpecJson(xvalue tblSpec, const char* sModelXid, const char* sTitle, const char* sDescription)
{
	xvalue tblRoot = xvoCreateTable();
	xvalue tblIdentity = xvoCreateTable();
	xvalue tblEntity = xvoCreateTable();
	xvalue arrFields = tblSpec ? xvoTableGetValue(tblSpec, "fields", 6) : NULL;
	xvalue arrManagedFields = xvoCreateArray();
	xvalue tblCore = xvoCreateTable();
	xvalue tblDraft = xvoCreateTable();
	xvalue tblUi = xvoCreateTable();
	xvalue tblUiList = xvoCreateTable();
	xvalue tblUiForm = xvoCreateTable();
	xvalue tblPolicies = tblSpec ? xvoTableGetValue(tblSpec, "policies", 8) : NULL;
	xvalue tblPages = tblSpec ? xvoTableGetValue(tblSpec, "pages", 5) : NULL;
	xvalue arrCapabilities = tblSpec ? xvoTableGetValue(tblSpec, "capabilities", 12) : NULL;
	xvalue arrGroups = NULL;
	const char* sName = tblSpec ? xvoTableGetText(tblSpec, "name", 4) : NULL;
	const char* sNamespace = tblSpec ? xvoTableGetText(tblSpec, "namespace", 9) : NULL;
	const char* sTableName = tblSpec ? xvoTableGetText(tblSpec, "tableName", 9) : NULL;
	str sJson;

	xvoTableSetText(tblIdentity, "xid", 3, (str)Content_TextOr(sModelXid, ""), 0, FALSE);
	xvoTableSetText(tblIdentity, "name", 4, (str)Content_TextOr(sName, sModelXid), 0, FALSE);
	xvoTableSetText(tblIdentity, "namespace", 9, (str)Content_TextOr(sNamespace, ""), 0, FALSE);
	xvoTableSetText(tblIdentity, "title", 5, (str)Content_TextOr(sTitle, sModelXid), 0, FALSE);
	xvoTableSetText(tblIdentity, "description", 11, (str)Content_TextOr(sDescription, ""), 0, FALSE);
	xvoTableSetValue(tblRoot, "identity", 8, tblIdentity, TRUE);

	if ( arrFields && xvoType(arrFields) == XVO_DT_ARRAY ) {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
			xvalue tblField = xvoArrayGetValue(arrFields, i);
			if ( tblField && xvoType(tblField) == XVO_DT_TABLE ) {
				xvoArrayAppendValue(arrManagedFields, Content_BuildManagedSpecField(tblField), TRUE);
			}
		}
	}
	xvoTableSetText(tblEntity, "table", 5, (str)Content_TextOr(sTableName, "content_item"), 0, FALSE);
	xvoTableSetText(tblEntity, "entityName", 10, (str)Content_TextOr(sName, "content"), 0, FALSE);
	xvoTableSetText(tblEntity, "titleField", 10, "title", 0, FALSE);
	xvoTableSetText(tblEntity, "statusField", 11, "status", 0, FALSE);
	xvoTableSetValue(tblEntity, "fields", 6, arrManagedFields, TRUE);
	xvoTableSetValue(tblRoot, "entity", 6, tblEntity, TRUE);

	xvoTableSetBool(tblDraft, "enabled", 7, TRUE);
	xvoTableSetText(tblDraft, "mode", 4, "same-table", 0, FALSE);
	xvoTableSetValue(tblCore, "draft", 5, tblDraft, TRUE);
	xvoTableSetBool(tblCore, "adminCrud", 9, TRUE);
	xvoTableSetBool(tblCore, "publicApi", 9, TRUE);
	xvoTableSetValue(tblRoot, "coreFeatures", 12, tblCore, TRUE);

	xvoTableSetInt(tblUiList, "pageSize", 8, tblPages ? xvoTableGetInt(tblPages, "pageSize", 8) : 20);
	xvoTableSetValue(tblUi, "list", 4, tblUiList, TRUE);
	xvoTableSetText(tblUiForm, "layout", 6, "single-column", 0, FALSE);
	xvoTableSetValue(tblUi, "form", 4, tblUiForm, TRUE);
	xvoTableSetValue(tblRoot, "ui", 2, tblUi, TRUE);

	if ( tblPolicies && xvoType(tblPolicies) == XVO_DT_TABLE ) {
		xvoTableSetValue(tblRoot, "policies", 8, xvoCopy(tblPolicies), TRUE);
	}
	if ( arrCapabilities && xvoType(arrCapabilities) == XVO_DT_ARRAY ) {
		xvoTableSetValue(tblRoot, "capabilitySlots", 15, xvoCopy(arrCapabilities), TRUE);
	}
	if ( tblPages && xvoType(tblPages) == XVO_DT_TABLE ) {
		arrGroups = xvoTableGetValue(tblPages, "fieldGroups", 11);
		if ( arrGroups && xvoType(arrGroups) == XVO_DT_ARRAY ) {
			xvalue tblPresentation = xvoCreateTable();
			xvoTableSetValue(tblPresentation, "groups", 6, xvoCopy(arrGroups), TRUE);
			xvoTableSetValue(tblRoot, "presentation", 12, tblPresentation, TRUE);
		}
	}

	sJson = xrtStringifyJSON(tblRoot, FALSE, NULL);
	xvoUnref(tblRoot);
	return sJson;
}

xvalue Content_GeneratePluginForModel(const char* sXid, char** psError)
{
	int iModelId = 0;
	int iRevision = 0;
	str sGeneratedPluginXid = NULL;
	str sTitle = NULL;
	str sPluginTitle = NULL;
	str sMenuTitle = NULL;
	str sPluginDescription = NULL;
	str sSpecJson = NULL;
	str sManagedSpecJson = NULL;
	xvalue tblSpecJson = NULL;
	xvalue tblAdvisor = NULL;
	XAdminGeneratedFile files[19];
	XAdminGeneratedPluginSpec spec;
	str sPluginJson = NULL;
	str sMainC = NULL;
	str sAdminHtml = NULL;
	str sDraftHtml = NULL;
	str sEditorHtml = NULL;
	str sCategoryHtml = NULL;
	str sDefaults = NULL;
	str sSchema = NULL;
	str sManaged = NULL;
	str sContracts = NULL;
	str sPublicHtml = NULL;
	str sAbilityHtml = NULL;
	str sStaticDetailHtml = NULL;
	str sMountExample = NULL;
	str sMountSchema = NULL;
	str sMigrationPlan = NULL;
	str sMigrationSql = NULL;
	str sCustomReadme = NULL;
	str sOutputJson = NULL;
	str sAdvisorJson = NULL;
	char* sValidateError = NULL;
	xvalue tblRet = NULL;
	int64 iNow = xrtNow();
	bool bCategoryPack = FALSE;
	bool bOK;
	int iFileCount = 0;

	if ( psError ) *psError = NULL;

	if ( !Content_LoadModelForGeneration(sXid, &iModelId, &iRevision, &sGeneratedPluginXid, &sTitle, &sSpecJson, psError) ) {
		return NULL;
	}

	if ( sSpecJson ) {
		tblSpecJson = xrtParseJSON(sSpecJson, strlen(sSpecJson));
	}
	if ( (tblSpecJson == NULL) || (xvoType(tblSpecJson) != XVO_DT_TABLE) || !Content_SpecValidate(tblSpecJson, &sValidateError) ) {
		str sMessage = xrtFormat("stored spec invalid: %s", sValidateError ? sValidateError : "invalid json");
		if ( psError ) *psError = xrtCopyStr(sMessage ? sMessage : "stored spec invalid", 0);
		Content_RecordGenerationFailure(iModelId, iRevision, sGeneratedPluginXid, "{}", sMessage ? sMessage : "stored spec invalid", iNow);
		if ( sMessage ) xrtFree(sMessage);
		goto cleanup;
	}
	tblAdvisor = Content_BuildAdvisor(tblSpecJson);
	if ( tblAdvisor ) {
		str sAdvisorStatus = xvoTableGetText(tblAdvisor, "status", 6);
		sAdvisorJson = xrtStringifyJSON(tblAdvisor, FALSE, NULL);
		if ( (sAdvisorStatus == NULL) || (strcmp((const char*)sAdvisorStatus, "ok") != 0) ) {
			if ( psError ) *psError = xrtCopyStr("advisor check failed", 0);
			Content_RecordGenerationFailure(iModelId, iRevision, sGeneratedPluginXid, sAdvisorJson, "advisor check failed", iNow);
			goto cleanup;
		}
	} else {
		sAdvisorJson = xrtCopyStr("{}", 0);
	}
	sPluginTitle = xvoTableGetText(tblSpecJson, "pluginTitle", 11);
	sMenuTitle = xvoTableGetText(tblSpecJson, "menuTitle", 9);
	sPluginDescription = xvoTableGetText(tblSpecJson, "description", 11);
	bCategoryPack = Content_SpecHasCapability(tblSpecJson, "content.category");
	sManagedSpecJson = Content_BuildManagedSpecJson(tblSpecJson, sXid, sTitle, sPluginDescription);

	sPluginJson = Content_BuildGeneratedPluginJson(sGeneratedPluginXid, Content_TextOr((const char*)sPluginTitle, (const char*)sTitle), sPluginDescription);
	sMainC = Content_BuildManagedMainC(sGeneratedPluginXid, Content_TextOr((const char*)sPluginTitle, (const char*)sTitle), Content_TextOr((const char*)sMenuTitle, Content_TextOr((const char*)sPluginTitle, (const char*)sTitle)), tblSpecJson);
	sAdminHtml = Content_BuildManagedAdminPageHtml(sGeneratedPluginXid, "articles");
	sDraftHtml = Content_BuildManagedAdminPageHtml(sGeneratedPluginXid, "drafts");
	sEditorHtml = Content_BuildManagedEditorHtml(sGeneratedPluginXid);
	if ( bCategoryPack ) {
		sCategoryHtml = Content_BuildManagedCategoryHtml(sGeneratedPluginXid);
	}
	sPublicHtml = Content_BuildManagedPublicHtml(sGeneratedPluginXid);
	sAbilityHtml = Content_BuildManagedAbilityHtml(sGeneratedPluginXid);
	sStaticDetailHtml = Content_BuildManagedStaticDetailHtml();
	sDefaults = xrtCopyStr("{\"pageSize\":20}\n", 0);
	sSchema = xrtCopyStr("{\"type\":\"object\",\"properties\":{\"pageSize\":{\"type\":\"integer\",\"title\":\"Page Size\"}},\"additionalProperties\":false}\n", 0);
	sManaged = xrtFormat("{\"managed\":true,\"managedBy\":\"content\",\"managedType\":\"generated-plugin\",\"pluginXid\":\"%s\",\"contentTypeRevision\":%d,\"generatedRoot\":\"generated\",\"runtimeRoot\":\"runtime\",\"customRoot\":\"custom\",\"generatedAt\":%lld}\n", sGeneratedPluginXid ? (const char*)sGeneratedPluginXid : "", iRevision, (long long)iNow);
	sContracts = Content_BuildGeneratedContracts(sXid, iRevision, tblSpecJson);
	sMountExample = xrtCopyStr("{\"mounts\":[]}\n", 0);
	sMountSchema = xrtCopyStr("{\"type\":\"object\",\"properties\":{\"mounts\":{\"type\":\"array\"}},\"required\":[\"mounts\"]}\n", 0);
	sMigrationPlan = xrtFormat("{\"pluginXid\":\"%s\",\"revision\":%d,\"items\":[],\"sql\":[]}\n", sGeneratedPluginXid ? (const char*)sGeneratedPluginXid : "", iRevision);
	sMigrationSql = xrtCopyStr("-- Managed content migration is handled by generated plugin startup.\n", 0);
	sCustomReadme = xrtCopyStr("This directory is reserved for user-owned extensions.\n", 0);
	sOutputJson = xrtFormat(
		bCategoryPack
			? "{\"pluginXid\":\"%s\",\"revision\":%d,\"files\":[\"plugin.json\",\"generated/main.c\",\"generated/admin.html\",\"generated/drafts.html\",\"generated/editor.html\",\"generated/categories.html\",\"generated/public.html\",\"generated/ability.html\",\"generated/spec.json\",\"template/static/detail.html\",\"config.defaults.json\",\"config.schema.json\",\"runtime/managed.json\",\"runtime/contracts.json\",\"runtime/capability.mounts.example.json\",\"runtime/capability.mounts.schema.json\",\"runtime/migration.plan.json\",\"generated/migration.sql\",\"custom/README.txt\"]}"
			: "{\"pluginXid\":\"%s\",\"revision\":%d,\"files\":[\"plugin.json\",\"generated/main.c\",\"generated/admin.html\",\"generated/drafts.html\",\"generated/editor.html\",\"generated/public.html\",\"generated/ability.html\",\"generated/spec.json\",\"template/static/detail.html\",\"config.defaults.json\",\"config.schema.json\",\"runtime/managed.json\",\"runtime/contracts.json\",\"runtime/capability.mounts.example.json\",\"runtime/capability.mounts.schema.json\",\"runtime/migration.plan.json\",\"generated/migration.sql\",\"custom/README.txt\"]}",
		sGeneratedPluginXid ? (const char*)sGeneratedPluginXid : "",
		iRevision
	);

	memset(files, 0, sizeof(files));
	Content_SetGeneratedFile(&files[iFileCount++], "plugin.json", sPluginJson);
	Content_SetGeneratedFile(&files[iFileCount++], "generated/main.c", sMainC);
	Content_SetGeneratedFile(&files[iFileCount++], "generated/admin.html", sAdminHtml);
	Content_SetGeneratedFile(&files[iFileCount++], "generated/drafts.html", sDraftHtml);
	Content_SetGeneratedFile(&files[iFileCount++], "generated/editor.html", sEditorHtml);
	if ( bCategoryPack ) {
		Content_SetGeneratedFile(&files[iFileCount++], "generated/categories.html", sCategoryHtml);
	}
	Content_SetGeneratedFile(&files[iFileCount++], "generated/public.html", sPublicHtml);
	Content_SetGeneratedFile(&files[iFileCount++], "generated/ability.html", sAbilityHtml);
	Content_SetGeneratedFile(&files[iFileCount++], "generated/spec.json", sManagedSpecJson);
	Content_SetGeneratedFile(&files[iFileCount++], "template/static/detail.html", sStaticDetailHtml);
	Content_SetGeneratedFile(&files[iFileCount++], "config.defaults.json", sDefaults);
	Content_SetGeneratedFile(&files[iFileCount++], "config.schema.json", sSchema);
	Content_SetGeneratedFile(&files[iFileCount++], "runtime/managed.json", sManaged);
	Content_SetGeneratedFile(&files[iFileCount++], "runtime/contracts.json", sContracts);
	Content_SetGeneratedFile(&files[iFileCount++], "runtime/capability.mounts.example.json", sMountExample);
	Content_SetGeneratedFile(&files[iFileCount++], "runtime/capability.mounts.schema.json", sMountSchema);
	Content_SetGeneratedFile(&files[iFileCount++], "runtime/migration.plan.json", sMigrationPlan);
	Content_SetGeneratedFile(&files[iFileCount++], "generated/migration.sql", sMigrationSql);
	Content_SetGeneratedFile(&files[iFileCount++], "custom/README.txt", sCustomReadme);

	memset(&spec, 0, sizeof(spec));
	spec.xid = sGeneratedPluginXid;
	spec.title = sTitle ? sTitle : "Generated Content Plugin";
	spec.version = "1.0.0";
	spec.entry = "generated/main.c";
	spec.auto_enable = 0;
	spec.file_count = iFileCount;
	spec.files = files;
	bOK = PluginSystem_Generate(&spec);

	if ( bOK ) {
		Content_RecordGenerationSuccess(iModelId, iRevision, sGeneratedPluginXid, sOutputJson, sAdvisorJson, iNow);
		Content_UpdateAppliedRevision(iModelId, iRevision, sGeneratedPluginXid);

		tblRet = xvoCreateTable();
		xvoTableSetText(tblRet, "pluginXid", 9, sGeneratedPluginXid, 0, FALSE);
		xvoTableSetInt(tblRet, "revision", 8, iRevision);
		xvoTableSetBool(tblRet, "generated", 9, TRUE);
	} else {
		if ( psError ) {
			*psError = xrtCopyStr("generate plugin failed", 0);
		}
		Content_RecordGenerationFailure(iModelId, iRevision, sGeneratedPluginXid, sAdvisorJson, "generate plugin failed", iNow);
	}

cleanup:
	if ( tblAdvisor ) xvoUnref(tblAdvisor);
	if ( tblSpecJson ) xvoUnref(tblSpecJson);
	if ( sGeneratedPluginXid ) xrtFree(sGeneratedPluginXid);
	if ( sTitle ) xrtFree(sTitle);
	if ( sSpecJson ) xrtFree(sSpecJson);
	if ( sManagedSpecJson ) xrtFree(sManagedSpecJson);
	if ( sPluginJson ) xrtFree(sPluginJson);
	if ( sMainC ) xrtFree(sMainC);
	if ( sAdminHtml ) xrtFree(sAdminHtml);
	if ( sDraftHtml ) xrtFree(sDraftHtml);
	if ( sEditorHtml ) xrtFree(sEditorHtml);
	if ( sCategoryHtml ) xrtFree(sCategoryHtml);
	if ( sDefaults ) xrtFree(sDefaults);
	if ( sSchema ) xrtFree(sSchema);
	if ( sManaged ) xrtFree(sManaged);
	if ( sContracts ) xrtFree(sContracts);
	if ( sPublicHtml ) xrtFree(sPublicHtml);
	if ( sAbilityHtml ) xrtFree(sAbilityHtml);
	if ( sStaticDetailHtml ) xrtFree(sStaticDetailHtml);
	if ( sMountExample ) xrtFree(sMountExample);
	if ( sMountSchema ) xrtFree(sMountSchema);
	if ( sMigrationPlan ) xrtFree(sMigrationPlan);
	if ( sMigrationSql ) xrtFree(sMigrationSql);
	if ( sCustomReadme ) xrtFree(sCustomReadme);
	if ( sOutputJson ) xrtFree(sOutputJson);
	if ( sAdvisorJson ) xrtFree(sAdvisorJson);
	if ( sValidateError ) xrtFree(sValidateError);
	return tblRet;
}
