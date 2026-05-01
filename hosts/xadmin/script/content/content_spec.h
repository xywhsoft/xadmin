bool Content_SpecIsValidIdent(const char* sText, bool bAllowDot)
{
	size_t iLen;

	if ( (sText == NULL) || (sText[0] == '\0') ) {
		return FALSE;
	}
	iLen = strlen(sText);
	if ( iLen > 96 ) {
		return FALSE;
	}
	for ( size_t i = 0; i < iLen; i++ ) {
		char ch = sText[i];
		if ( ((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) || ((ch >= '0') && (ch <= '9')) || (ch == '_') || (ch == '-') || (bAllowDot && (ch == '.')) ) {
			continue;
		}
		return FALSE;
	}
	return TRUE;
}

bool Content_SpecValidate(xvalue tblSpec, char** psError)
{
	xvalue arrFields;
	xvalue arrCapabilities;
	xvalue tblPages;
	xvalue tblPolicies;
	str sXid;
	str sGeneratedPluginXid;

	if ( psError ) {
		*psError = NULL;
	}
	if ( (tblSpec == NULL) || (xvoType(tblSpec) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("spec: invalid object", 0);
		return FALSE;
	}

	sXid = xvoTableGetText(tblSpec, "xid", 3);
	if ( !Content_SpecIsValidIdent(sXid, TRUE) ) {
		if ( psError ) *psError = xrtCopyStr("xid: invalid content model xid", 0);
		return FALSE;
	}
	sGeneratedPluginXid = xvoTableGetText(tblSpec, "generatedPluginXid", 18);
	if ( (sGeneratedPluginXid != NULL) && (sGeneratedPluginXid[0] != '\0') && !Content_SpecIsValidIdent(sGeneratedPluginXid, TRUE) ) {
		if ( psError ) *psError = xrtCopyStr("generatedPluginXid: invalid plugin xid", 0);
		return FALSE;
	}

	arrFields = xvoTableGetValue(tblSpec, "fields", 6);
	if ( (arrFields == NULL) || (xvoType(arrFields) != XVO_DT_ARRAY) || (xvoArrayItemCount(arrFields) <= 0) ) {
		if ( psError ) *psError = xrtCopyStr("fields: at least one field is required", 0);
		return FALSE;
	}

	for ( uint32 i = 0; i < xvoArrayItemCount(arrFields); i++ ) {
		xvalue tblField = xvoArrayGetValue(arrFields, i);
		str sName = NULL;
		if ( (tblField == NULL) || (xvoType(tblField) != XVO_DT_TABLE) ) {
			if ( psError ) *psError = xrtFormat("fields[%u]: invalid field object", i);
			return FALSE;
		}
		sName = xvoTableGetText(tblField, "name", 4);
		if ( !Content_SpecIsValidIdent(sName, FALSE) ) {
			if ( psError ) *psError = xrtFormat("fields[%u].name: invalid field name", i);
			return FALSE;
		}
		for ( uint32 j = i + 1; j < xvoArrayItemCount(arrFields); j++ ) {
			xvalue tblOther = xvoArrayGetValue(arrFields, j);
			str sOther = (tblOther && (xvoType(tblOther) == XVO_DT_TABLE)) ? xvoTableGetText(tblOther, "name", 4) : NULL;
			if ( sOther && (strcmp(sName, sOther) == 0) ) {
				if ( psError ) *psError = xrtFormat("fields[%u].name: duplicate field name '%s'", j, sName);
				return FALSE;
			}
		}
	}

	arrCapabilities = xvoTableGetValue(tblSpec, "capabilities", 12);
	if ( arrCapabilities != NULL ) {
		if ( xvoType(arrCapabilities) != XVO_DT_ARRAY ) {
			if ( psError ) *psError = xrtCopyStr("capabilities: invalid capability list", 0);
			return FALSE;
		}
		for ( uint32 i = 0; i < xvoArrayItemCount(arrCapabilities); i++ ) {
			xvalue tblCap = xvoArrayGetValue(arrCapabilities, i);
			xvalue tblConfig;
			str sKey;
			if ( (tblCap == NULL) || (xvoType(tblCap) != XVO_DT_TABLE) ) {
				if ( psError ) *psError = xrtFormat("capabilities[%u]: invalid capability object", i);
				return FALSE;
			}
			sKey = xvoTableGetText(tblCap, "key", 3);
			if ( !Content_SpecIsValidIdent(sKey, TRUE) ) {
				if ( psError ) *psError = xrtFormat("capabilities[%u].key: invalid capability key", i);
				return FALSE;
			}
			if ( !ContentCapability_Exists(sKey) ) {
				if ( psError ) *psError = xrtFormat("capabilities[%u].key: unknown capability '%s'", i, sKey);
				return FALSE;
			}
			for ( uint32 j = i + 1; j < xvoArrayItemCount(arrCapabilities); j++ ) {
				xvalue tblOther = xvoArrayGetValue(arrCapabilities, j);
				str sOther = (tblOther && (xvoType(tblOther) == XVO_DT_TABLE)) ? xvoTableGetText(tblOther, "key", 3) : NULL;
				if ( sOther && (strcmp(sKey, sOther) == 0) ) {
					if ( psError ) *psError = xrtFormat("capabilities[%u].key: duplicate capability '%s'", j, sKey);
					return FALSE;
				}
			}
			tblConfig = xvoTableGetValue(tblCap, "config", 6);
			if ( (tblConfig != NULL) && (xvoType(tblConfig) != XVO_DT_TABLE) ) {
				if ( psError ) *psError = xrtFormat("capabilities[%u].config: invalid config object", i);
				return FALSE;
			}
		}
	}

	tblPages = xvoTableGetValue(tblSpec, "pages", 5);
	if ( (tblPages != NULL) && (xvoType(tblPages) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("pages: invalid pages object", 0);
		return FALSE;
	}
	if ( tblPages != NULL ) {
		xvalue arrDisplayGroups = xvoTableGetValue(tblPages, "displayGroups", 13);
		xvalue arrFieldGroups = xvoTableGetValue(tblPages, "fieldGroups", 11);
		if ( (arrDisplayGroups != NULL) && (xvoType(arrDisplayGroups) != XVO_DT_ARRAY) ) {
			if ( psError ) *psError = xrtCopyStr("pages.displayGroups: invalid group list", 0);
			return FALSE;
		}
		if ( (arrFieldGroups != NULL) && (xvoType(arrFieldGroups) != XVO_DT_ARRAY) ) {
			if ( psError ) *psError = xrtCopyStr("pages.fieldGroups: invalid group list", 0);
			return FALSE;
		}
	}

	tblPolicies = xvoTableGetValue(tblSpec, "policies", 8);
	if ( (tblPolicies != NULL) && (xvoType(tblPolicies) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("policies: invalid policies object", 0);
		return FALSE;
	}
	return TRUE;
}

uint64 Content_SpecHash64(const char* sText)
{
	uint64 h = 1469598103934665603ULL;
	const unsigned char* p = (const unsigned char*)(sText ? sText : "");

	while ( *p ) {
		h ^= (uint64)(*p++);
		h *= 1099511628211ULL;
	}
	return h;
}

str Content_SpecHashText(const char* sSpecJson)
{
	uint64 h = Content_SpecHash64(sSpecJson);
	return xrtFormat("%08x%08x", (uint32)(h >> 32), (uint32)(h & 0xffffffffu));
}
