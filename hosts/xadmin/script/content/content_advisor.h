xvalue Content_CreateAdvisorItem(const char* sLevel, const char* sMessage, const char* sField)
{
	xvalue tblItem = xvoCreateTable();
	xvoTableSetText(tblItem, "level", 5, (str)((sLevel && sLevel[0]) ? sLevel : "info"), 0, FALSE);
	xvoTableSetText(tblItem, "message", 7, (str)(sMessage ? sMessage : ""), 0, FALSE);
	if ( (sField != NULL) && (sField[0] != '\0') ) {
		xvoTableSetText(tblItem, "field", 5, (str)sField, 0, FALSE);
	}
	return tblItem;
}

xvalue Content_CreateCapabilityAcceptanceItem(const char* sPackId, const char* sApiPath, const char* sViewPath)
{
	xvalue tblItem = Content_CreateAdvisorItem("info", "Capability acceptance path registered", sPackId);
	xvoTableSetText(tblItem, "kind", 4, "acceptance", 0, FALSE);
	xvoTableSetText(tblItem, "packId", 6, (str)(sPackId ? sPackId : ""), 0, FALSE);
	xvoTableSetText(tblItem, "apiPath", 7, (str)(sApiPath ? sApiPath : ""), 0, FALSE);
	xvoTableSetText(tblItem, "viewPath", 8, (str)(sViewPath ? sViewPath : ""), 0, FALSE);
	return tblItem;
}

const char* Content_AbilityAdvisorMessage(const char* sPackId)
{
	(void)sPackId;
	return "Capability pack enabled: verify generated routes, schema, permissions and runtime cost before production launch.";
}

const char* Content_AbilityAcceptanceApiPath(const char* sPackId)
{
	(void)sPackId;
	return "/admin/api/plugin/{pluginXid}/contracts";
}

const char* Content_AbilityAcceptanceViewPath(const char* sPackId)
{
	(void)sPackId;
	return "/admin/view/plugin/{pluginXid}/ability";
}

str Content_CopyAbilityManifestText(const char* sPackId, const char* sKey)
{
	xvalue tblPack = NULL;
	const char* sManifestJson = NULL;
	xvalue tblManifest = NULL;
	const char* sValue = NULL;
	str sRet = NULL;

	if ( (sPackId == NULL) || (sKey == NULL) ) return NULL;
	tblPack = ContentPack_GetDetail(sPackId);
	if ( tblPack == NULL ) return NULL;
	sManifestJson = xvoTableGetText(tblPack, "manifestJson", 12);
	if ( (sManifestJson != NULL) && (sManifestJson[0] != '\0') ) {
		tblManifest = xrtParseJSON((str)sManifestJson, strlen(sManifestJson));
		if ( tblManifest && (xvoType(tblManifest) == XVO_DT_TABLE) ) {
			sValue = xvoTableGetText(tblManifest, sKey, (uint32)strlen(sKey));
			if ( (sValue != NULL) && (sValue[0] != '\0') ) {
				sRet = xrtCopyStr((str)sValue, 0);
			}
		}
	}
	if ( tblManifest ) xvoUnref(tblManifest);
	xvoUnref(tblPack);
	return sRet;
}

void Content_AppendCapabilityAdvisorItems(xvalue arrItems, xvalue arrCapabilities)
{
	if ( (arrItems == NULL) || (arrCapabilities == NULL) || (xvoType(arrCapabilities) != XVO_DT_ARRAY) ) return;
	for ( uint32 i = 0; i < xvoArrayItemCount(arrCapabilities); i++ ) {
		xvalue tblCap = xvoArrayGetValue(arrCapabilities, i);
		const char* sPackId = (tblCap && (xvoType(tblCap) == XVO_DT_TABLE)) ? xvoTableGetText(tblCap, "key", 3) : NULL;
		const char* sNormalizedPackId = ContentPack_NormalizeId(sPackId);
		str sManifestMessage = Content_CopyAbilityManifestText(sNormalizedPackId, "advisorMessage");
		str sManifestApiPath = Content_CopyAbilityManifestText(sNormalizedPackId, "acceptanceApiPath");
		str sManifestViewPath = Content_CopyAbilityManifestText(sNormalizedPackId, "acceptanceViewPath");
		const char* sMessage = sManifestMessage ? (const char*)sManifestMessage : Content_AbilityAdvisorMessage(sNormalizedPackId);
		const char* sApiPath = sManifestApiPath ? (const char*)sManifestApiPath : Content_AbilityAcceptanceApiPath(sNormalizedPackId);
		const char* sViewPath = sManifestViewPath ? (const char*)sManifestViewPath : Content_AbilityAcceptanceViewPath(sNormalizedPackId);

		if ( sMessage ) {
			xvoArrayAppendValue(arrItems, Content_CreateAdvisorItem("warning", sMessage, sNormalizedPackId), TRUE);
		}
		xvoArrayAppendValue(arrItems,
			Content_CreateCapabilityAcceptanceItem(
				sNormalizedPackId,
				sApiPath,
				sViewPath),
			TRUE);
		if ( sManifestMessage ) xrtFree(sManifestMessage);
		if ( sManifestApiPath ) xrtFree(sManifestApiPath);
		if ( sManifestViewPath ) xrtFree(sManifestViewPath);
	}
}

xvalue Content_BuildAdvisor(xvalue tblSpec)
{
	xvalue tblRet = xvoCreateTable();
	xvalue arrItems = xvoCreateArray();
	xvalue arrFields = NULL;
	xvalue tblPages = NULL;
	xvalue arrCapabilities = NULL;
	str sXid = NULL;
	str sTitle = NULL;
	int iErrorCount = 0;

	if ( tblSpec && (xvoType(tblSpec) == XVO_DT_TABLE) ) {
		sXid = xvoTableGetText(tblSpec, "xid", 3);
		sTitle = xvoTableGetText(tblSpec, "title", 5);
		arrFields = xvoTableGetValue(tblSpec, "fields", 6);
		tblPages = xvoTableGetValue(tblSpec, "pages", 5);
		arrCapabilities = xvoTableGetValue(tblSpec, "capabilities", 12);
	}

	if ( !Content_IsValidXid(sXid) ) {
		xvoArrayAppendValue(arrItems, Content_CreateAdvisorItem("error", "Invalid or missing model xid", NULL), TRUE);
		iErrorCount++;
	}
	if ( (sTitle == NULL) || (sTitle[0] == '\0') ) {
		xvoArrayAppendValue(arrItems, Content_CreateAdvisorItem("error", "Model title is required", NULL), TRUE);
		iErrorCount++;
	}
	if ( (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) || (xvoArrayItemCount(arrFields) <= 0) ) {
		xvoArrayAppendValue(arrItems, Content_CreateAdvisorItem("error", "At least one field is required", NULL), TRUE);
		iErrorCount++;
	} else {
		for ( uint32 i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
			xvalue tblField = xvoArrayGetValue(arrFields, i);
			str sName = (tblField && (xvoType(tblField) == XVO_DT_TABLE)) ? xvoTableGetText(tblField, "name", 4) : NULL;
			if ( (sName == NULL) || (sName[0] == '\0') ) {
				xvoArrayAppendValue(arrItems, Content_CreateAdvisorItem("error", "Field name is required", NULL), TRUE);
				iErrorCount++;
				continue;
			}
			for ( uint32 j = i + 1; j < xvoArrayItemCount(arrFields); j++ ) {
				xvalue tblOther = xvoArrayGetValue(arrFields, j);
				str sOther = (tblOther && (xvoType(tblOther) == XVO_DT_TABLE)) ? xvoTableGetText(tblOther, "name", 4) : NULL;
				if ( sOther && (strcmp(sName, sOther) == 0) ) {
					xvoArrayAppendValue(arrItems, Content_CreateAdvisorItem("error", "Duplicate field name", sName), TRUE);
					iErrorCount++;
				}
			}
		}
	}

	if ( arrFields && (xvoType(arrFields) == XVO_DT_ARRAY) ) {
		str sMessage = xrtFormat("Fields: %u field(s) will be generated into payload and admin views", xvoArrayItemCount(arrFields));
		xvoArrayAppendValue(arrItems, Content_CreateAdvisorItem("info", sMessage, "fields"), TRUE);
		if ( sMessage ) xrtFree(sMessage);
	}
	if ( tblPages && (xvoType(tblPages) == XVO_DT_TABLE) ) {
		xvoArrayAppendValue(arrItems, Content_CreateAdvisorItem("info", "Pages: admin/public/detail page switches will affect generated routes and page files", "pages"), TRUE);
	}
	if ( arrCapabilities && (xvoType(arrCapabilities) == XVO_DT_ARRAY) && (xvoArrayItemCount(arrCapabilities) > 0) ) {
		str sMessage = xrtFormat("Capabilities: %u capability item(s) will be written into contracts", xvoArrayItemCount(arrCapabilities));
		xvoArrayAppendValue(arrItems, Content_CreateAdvisorItem("info", sMessage, "capabilities"), TRUE);
		if ( sMessage ) xrtFree(sMessage);
		Content_AppendCapabilityAdvisorItems(arrItems, arrCapabilities);
	}
	xvoArrayAppendValue(arrItems, Content_CreateAdvisorItem("info", "Database: business records stay in generated plugin private database", "database"), TRUE);

	xvoTableSetText(tblRet, "status", 6, iErrorCount > 0 ? "error" : "ok", 0, FALSE);
	xvoTableSetInt(tblRet, "errorCount", 10, iErrorCount);
	xvoTableSetValue(tblRet, "items", 5, arrItems, TRUE);
	return tblRet;
}
