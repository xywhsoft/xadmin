#ifndef XADMIN_CONTENT_CAPABILITY_H
#define XADMIN_CONTENT_CAPABILITY_H

bool ContentCapability_UpsertDefinition(xvalue tblCap);
int ContentCapability_LoadFileProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param);

bool ContentCapability_UpsertBuiltin(const char* sKey, const char* sTitle, const char* sSurfacesJson, const char* sDefaultsJson, const char* sSchemaJson, int iSort)
{
	sqlite3_stmt* stmt = NULL;
	int64 iNow = xrtNow();
	bool bOK;

	if ( G_DB == NULL ) {
		return FALSE;
	}

	if ( sqlite3_prepare_v3(G_DB,
		"INSERT INTO content_capability (capability_key,title,provider_kind,provider_xid,surfaces_json,defaults_json,schema_json,status,sort,create_time,update_time) "
		"VALUES (?,?,'builtin','',?,?,?,'active',?,?,?) "
		"ON CONFLICT(capability_key) DO UPDATE SET title=excluded.title,surfaces_json=excluded.surfaces_json,defaults_json=excluded.defaults_json,schema_json=excluded.schema_json,status='active',sort=excluded.sort,update_time=excluded.update_time",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}

	sqlite3_bind_text(stmt, 1, sKey, -1, NULL);
	sqlite3_bind_text(stmt, 2, sTitle, -1, NULL);
	sqlite3_bind_text(stmt, 3, sSurfacesJson, -1, NULL);
	sqlite3_bind_text(stmt, 4, sDefaultsJson, -1, NULL);
	sqlite3_bind_text(stmt, 5, sSchemaJson, -1, NULL);
	sqlite3_bind_int(stmt, 6, iSort);
	sqlite3_bind_int64(stmt, 7, iNow);
	sqlite3_bind_int64(stmt, 8, iNow);
	bOK = (sqlite3_step(stmt) == SQLITE_DONE);
	sqlite3_finalize(stmt);
	return bOK;
}

bool ContentCapability_InitBuiltins()
{
	bool bOK = TRUE;
	str sCapabilityDir = xrtPathJoin(4, AppPath, "data", "content", "capabilities");

	if ( sCapabilityDir ) {
		xrtDirCreateAll(sCapabilityDir);
		if ( xrtDirExists(sCapabilityDir) ) {
			xrtDirScan(sCapabilityDir, FALSE, ContentCapability_LoadFileProc, &bOK);
		}
		xrtFree(sCapabilityDir);
	}

	bOK = ContentCapability_UpsertBuiltin(
		"comment",
		"Comment",
		"[\"admin_list\",\"admin_detail\",\"public_detail\"]",
		"{\"enabled\":true,\"moderation\":\"manual\"}",
		"{\"type\":\"object\",\"properties\":{\"enabled\":{\"type\":\"boolean\"},\"moderation\":{\"type\":\"string\"}}}",
		100
	) && bOK;

	return bOK;
}

bool ContentCapability_UpsertDefinition(xvalue tblCap)
{
	sqlite3_stmt* stmt = NULL;
	str sKey;
	str sTitle;
	str sProviderKind;
	str sProviderXid;
	str sStatus;
	str sSurfacesJson;
	str sDefaultsJson;
	str sSchemaJson;
	xvalue arrSurfaces;
	xvalue tblDefaults;
	xvalue tblSchema;
	int iSort;
	int64 iNow = xrtNow();
	bool bOK;

	if ( (tblCap == NULL) || (xvoType(tblCap) != XVO_DT_TABLE) || (G_DB == NULL) ) {
		return FALSE;
	}

	sKey = xvoTableGetText(tblCap, "key", 3);
	sTitle = xvoTableGetText(tblCap, "title", 5);
	sProviderKind = xvoTableGetText(tblCap, "providerKind", 12);
	sProviderXid = xvoTableGetText(tblCap, "providerXid", 11);
	sStatus = xvoTableGetText(tblCap, "status", 6);
	arrSurfaces = xvoTableGetValue(tblCap, "surfaces", 8);
	tblDefaults = xvoTableGetValue(tblCap, "defaults", 8);
	tblSchema = xvoTableGetValue(tblCap, "schema", 6);
	iSort = xvoTableGetInt(tblCap, "sort", 4);

	if ( (sKey == NULL) || (sKey[0] == '\0') || (sTitle == NULL) || (sTitle[0] == '\0') ) {
		return FALSE;
	}

	sSurfacesJson = arrSurfaces ? xrtStringifyJSON(arrSurfaces, FALSE, NULL) : xrtCopyStr("[]", 0);
	sDefaultsJson = tblDefaults ? xrtStringifyJSON(tblDefaults, FALSE, NULL) : xrtCopyStr("{}", 0);
	sSchemaJson = tblSchema ? xrtStringifyJSON(tblSchema, FALSE, NULL) : xrtCopyStr("{}", 0);

	bOK = FALSE;
	if ( sqlite3_prepare_v3(G_DB,
		"INSERT INTO content_capability (capability_key,title,provider_kind,provider_xid,surfaces_json,defaults_json,schema_json,status,sort,create_time,update_time) "
		"VALUES (?,?,?,?,?,?,?,?,?,?,?) "
		"ON CONFLICT(capability_key) DO UPDATE SET title=excluded.title,provider_kind=excluded.provider_kind,provider_xid=excluded.provider_xid,surfaces_json=excluded.surfaces_json,defaults_json=excluded.defaults_json,schema_json=excluded.schema_json,status=excluded.status,sort=excluded.sort,update_time=excluded.update_time",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sKey ? (const char*)sKey : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, sTitle ? (const char*)sTitle : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, sProviderKind ? (const char*)sProviderKind : "builtin", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, sProviderXid ? (const char*)sProviderXid : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 5, sSurfacesJson ? (const char*)sSurfacesJson : "[]", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 6, sDefaultsJson ? (const char*)sDefaultsJson : "{}", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 7, sSchemaJson ? (const char*)sSchemaJson : "{}", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 8, sStatus ? (const char*)sStatus : "active", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 9, iSort);
		sqlite3_bind_int64(stmt, 10, iNow);
		sqlite3_bind_int64(stmt, 11, iNow);
		bOK = (sqlite3_step(stmt) == SQLITE_DONE);
		sqlite3_finalize(stmt);
	}

	if ( sSurfacesJson ) xrtFree(sSurfacesJson);
	if ( sDefaultsJson ) xrtFree(sDefaultsJson);
	if ( sSchemaJson ) xrtFree(sSchemaJson);
	return bOK;
}

int ContentCapability_LoadFileProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
{
	size_t iLen;
	size_t iJsonSize = 0;
	str sJson;
	xvalue tblCap;
	bool* pbOK = (bool*)Param;

	(void)iSize;
	(void)pData;

	if ( (sPath == NULL) || (bDir != 0) ) {
		return TRUE;
	}
	iLen = strlen(sPath);
	if ( (iLen < 6) || (sqlite3_stricmp(sPath + iLen - 5, ".json") != 0) ) {
		return TRUE;
	}

	sJson = xrtFileGetAll(sPath, &iJsonSize);
	if ( sJson == NULL ) {
		if ( pbOK ) *pbOK = FALSE;
		return FALSE;
	}
	tblCap = xrtParseJSON(sJson, iJsonSize);
	xrtFree(sJson);
	if ( (tblCap == NULL) || (xvoType(tblCap) != XVO_DT_TABLE) ) {
		if ( tblCap ) xvoUnref(tblCap);
		if ( pbOK ) *pbOK = FALSE;
		return FALSE;
	}
	if ( !ContentCapability_UpsertDefinition(tblCap) && pbOK ) {
		*pbOK = FALSE;
	}
	xvoUnref(tblCap);
	return TRUE;
}

xvalue ContentCapability_ListActive()
{
	sqlite3_stmt* stmt = NULL;
	xvalue arrRows = xvoCreateArray();

	if ( sqlite3_prepare_v3(G_DB, "SELECT capability_key,title,provider_kind,provider_xid,surfaces_json,defaults_json,schema_json,status,sort FROM content_capability WHERE status='active' ORDER BY sort ASC,id ASC", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblRow = xvoCreateTable();
			xvoTableSetText(tblRow, "key", 3, (str)sqlite3_column_text(stmt, 0), 0, FALSE);
			xvoTableSetText(tblRow, "title", 5, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
			xvoTableSetText(tblRow, "providerKind", 12, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
			xvoTableSetText(tblRow, "providerXid", 11, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
			xvoTableSetText(tblRow, "surfacesJson", 12, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
			xvoTableSetText(tblRow, "defaultsJson", 12, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
			xvoTableSetText(tblRow, "schemaJson", 10, (str)sqlite3_column_text(stmt, 6), 0, FALSE);
			xvoTableSetText(tblRow, "status", 6, (str)sqlite3_column_text(stmt, 7), 0, FALSE);
			xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int(stmt, 8));
			xvoArrayAppendValue(arrRows, tblRow, TRUE);
		}
		sqlite3_finalize(stmt);
	}
	return arrRows;
}

bool ContentCapability_Exists(const char* sKey)
{
	sqlite3_stmt* stmt = NULL;
	bool bExists = FALSE;

	if ( (G_DB == NULL) || (sKey == NULL) || (sKey[0] == '\0') ) {
		return FALSE;
	}
	if ( sqlite3_prepare_v3(G_DB, "SELECT 1 FROM content_capability WHERE capability_key=? AND status='active' LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}
	sqlite3_bind_text(stmt, 1, sKey, -1, SQLITE_TRANSIENT);
	bExists = (sqlite3_step(stmt) == SQLITE_ROW);
	sqlite3_finalize(stmt);
	return bExists;
}

#endif
