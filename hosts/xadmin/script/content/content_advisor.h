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
	}
	xvoArrayAppendValue(arrItems, Content_CreateAdvisorItem("info", "Database: business records stay in generated plugin private database", "database"), TRUE);

	xvoTableSetText(tblRet, "status", 6, iErrorCount > 0 ? "error" : "ok", 0, FALSE);
	xvoTableSetInt(tblRet, "errorCount", 10, iErrorCount);
	xvoTableSetValue(tblRet, "items", 5, arrItems, TRUE);
	return tblRet;
}
