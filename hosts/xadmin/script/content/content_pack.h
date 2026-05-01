#ifndef XADMIN_CONTENT_PACK_H
#define XADMIN_CONTENT_PACK_H

int ContentPack_LoadDirProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param);

const char* ContentPack_TextOr(const char* sText, const char* sDefault)
{
	return (sText && sText[0]) ? sText : sDefault;
}

str ContentPack_ReadTextOrDefault(const char* sDir, const char* sName, const char* sDefault)
{
	str sPath;
	str sText = NULL;
	size_t iSize = 0;

	sPath = xrtPathJoin(2, sDir, sName);
	if ( sPath ) {
		if ( xrtFileExists(sPath) ) {
			sText = xrtFileGetAll(sPath, &iSize);
		}
		xrtFree(sPath);
	}
	if ( sText ) {
		return sText;
	}
	return xrtCopyStr((str)ContentPack_TextOr(sDefault, "{}"), 0);
}

str ContentPack_ReadManifestFile(xvalue tblPack, const char* sDir, const char* sKey, int iKeyLen, const char* sDefaultName, const char* sFallbackJson)
{
	str sName = xvoTableGetText(tblPack, (str)sKey, iKeyLen);
	return ContentPack_ReadTextOrDefault(sDir, ContentPack_TextOr((const char*)sName, sDefaultName), sFallbackJson);
}

bool ContentPack_UpsertManifest(xvalue tblPack, const char* sDir)
{
	sqlite3_stmt* stmt = NULL;
	str sPackId;
	str sName;
	str sTitle;
	str sDescription;
	str sVersion;
	str sAuthor;
	str sSource;
	str sInstallType;
	str sStatus;
	str sManifestJson = NULL;
	str sGlobalFormJson = NULL;
	str sInstanceFormJson = NULL;
	str sEffectsJson = NULL;
	str sHooksJson = NULL;
	str sSymbolsJson = NULL;
	str sPatchesJson = NULL;
	str sContractsJson = NULL;
	str sUpdateChannel = NULL;
	str sUpdatePackageId = NULL;
	xvalue tblUpdate;
	xvalue tblProtection;
	xvalue tblConfig;
	int iCanUpdate = 0;
	int iCanUninstall = 0;
	int iReadonly = 0;
	int iSystem = 0;
	int iSort;
	int64 iNow = xrtNow();
	bool bOK = FALSE;

	if ( (G_DB == NULL) || (tblPack == NULL) || (xvoType(tblPack) != XVO_DT_TABLE) ) {
		return FALSE;
	}

	sPackId = xvoTableGetText(tblPack, "packId", 6);
	sName = xvoTableGetText(tblPack, "name", 4);
	sTitle = xvoTableGetText(tblPack, "title", 5);
	sDescription = xvoTableGetText(tblPack, "description", 11);
	sVersion = xvoTableGetText(tblPack, "version", 7);
	sAuthor = xvoTableGetText(tblPack, "author", 6);
	sSource = xvoTableGetText(tblPack, "source", 6);
	sInstallType = xvoTableGetText(tblPack, "installType", 11);
	sStatus = xvoTableGetText(tblPack, "status", 6);
	iSort = xvoTableGetInt(tblPack, "sort", 4);
	tblUpdate = xvoTableGetValue(tblPack, "update", 6);
	tblProtection = xvoTableGetValue(tblPack, "protection", 10);
	tblConfig = xvoTableGetValue(tblPack, "config", 6);

	if ( (sPackId == NULL) || (sPackId[0] == '\0') || (sTitle == NULL) || (sTitle[0] == '\0') ) {
		return FALSE;
	}
	if ( iSort <= 0 ) {
		iSort = 1000;
	}

	if ( tblUpdate && (xvoType(tblUpdate) == XVO_DT_TABLE) ) {
		sUpdateChannel = xvoTableGetText(tblUpdate, "channel", 7);
		sUpdatePackageId = xvoTableGetText(tblUpdate, "packageId", 9);
		iCanUpdate = xvoTableGetBool(tblUpdate, "canUpdate", 9) ? 1 : 0;
		iCanUninstall = xvoTableGetBool(tblUpdate, "canUninstall", 12) ? 1 : 0;
	}
	if ( tblProtection && (xvoType(tblProtection) == XVO_DT_TABLE) ) {
		iReadonly = xvoTableGetBool(tblProtection, "readonly", 8) ? 1 : 0;
		iSystem = xvoTableGetBool(tblProtection, "system", 6) ? 1 : 0;
	}

	sManifestJson = xrtStringifyJSON(tblPack, FALSE, NULL);
	if ( tblConfig && (xvoType(tblConfig) == XVO_DT_TABLE) ) {
		str sGlobalForm = xvoTableGetText(tblConfig, "globalForm", 10);
		str sInstanceForm = xvoTableGetText(tblConfig, "instanceForm", 12);
		sGlobalFormJson = ContentPack_ReadTextOrDefault(sDir, ContentPack_TextOr((const char*)sGlobalForm, "global.xform.json"), "{}");
		sInstanceFormJson = ContentPack_ReadTextOrDefault(sDir, ContentPack_TextOr((const char*)sInstanceForm, "instance.xform.json"), "{}");
	} else {
		sGlobalFormJson = ContentPack_ReadTextOrDefault(sDir, "global.xform.json", "{}");
		sInstanceFormJson = ContentPack_ReadTextOrDefault(sDir, "instance.xform.json", "{}");
	}
	sEffectsJson = ContentPack_ReadManifestFile(tblPack, sDir, "effects", 7, "effects.json", "{}");
	sHooksJson = ContentPack_ReadManifestFile(tblPack, sDir, "hooks", 5, "hooks.json", "{}");
	sSymbolsJson = ContentPack_ReadManifestFile(tblPack, sDir, "symbols", 7, "symbols.json", "{}");
	sPatchesJson = ContentPack_ReadManifestFile(tblPack, sDir, "patches", 7, "patches.json", "{}");
	sContractsJson = ContentPack_ReadManifestFile(tblPack, sDir, "contracts", 9, "contracts.json", "{}");

	if ( sqlite3_prepare_v3(G_DB,
		"INSERT INTO content_pack (pack_id,name,title,description,version,author,source,install_type,status,path,manifest_json,global_form_json,instance_form_json,effects_json,hooks_json,symbols_json,patches_json,contracts_json,update_channel,update_package_id,can_update,can_uninstall,readonly,system,sort,create_time,update_time) "
		"VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
		"ON CONFLICT(pack_id) DO UPDATE SET name=excluded.name,title=excluded.title,description=excluded.description,version=excluded.version,author=excluded.author,source=excluded.source,install_type=excluded.install_type,status=excluded.status,path=excluded.path,manifest_json=excluded.manifest_json,global_form_json=excluded.global_form_json,instance_form_json=excluded.instance_form_json,effects_json=excluded.effects_json,hooks_json=excluded.hooks_json,symbols_json=excluded.symbols_json,patches_json=excluded.patches_json,contracts_json=excluded.contracts_json,update_channel=excluded.update_channel,update_package_id=excluded.update_package_id,can_update=excluded.can_update,can_uninstall=excluded.can_uninstall,readonly=excluded.readonly,system=excluded.system,sort=excluded.sort,update_time=excluded.update_time",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sPackId, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, ContentPack_TextOr((const char*)sName, (const char*)sPackId), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, sTitle, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, ContentPack_TextOr((const char*)sDescription, ""), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 5, ContentPack_TextOr((const char*)sVersion, ""), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 6, ContentPack_TextOr((const char*)sAuthor, ""), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 7, ContentPack_TextOr((const char*)sSource, "local"), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 8, ContentPack_TextOr((const char*)sInstallType, "local"), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 9, ContentPack_TextOr((const char*)sStatus, "active"), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 10, ContentPack_TextOr(sDir, ""), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 11, ContentPack_TextOr((const char*)sManifestJson, "{}"), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 12, ContentPack_TextOr((const char*)sGlobalFormJson, "{}"), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 13, ContentPack_TextOr((const char*)sInstanceFormJson, "{}"), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 14, ContentPack_TextOr((const char*)sEffectsJson, "{}"), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 15, ContentPack_TextOr((const char*)sHooksJson, "{}"), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 16, ContentPack_TextOr((const char*)sSymbolsJson, "{}"), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 17, ContentPack_TextOr((const char*)sPatchesJson, "{}"), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 18, ContentPack_TextOr((const char*)sContractsJson, "{}"), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 19, ContentPack_TextOr((const char*)sUpdateChannel, ""), -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 20, ContentPack_TextOr((const char*)sUpdatePackageId, ""), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 21, iCanUpdate);
		sqlite3_bind_int(stmt, 22, iCanUninstall);
		sqlite3_bind_int(stmt, 23, iReadonly);
		sqlite3_bind_int(stmt, 24, iSystem);
		sqlite3_bind_int(stmt, 25, iSort);
		sqlite3_bind_int64(stmt, 26, iNow);
		sqlite3_bind_int64(stmt, 27, iNow);
		bOK = (sqlite3_step(stmt) == SQLITE_DONE);
		sqlite3_finalize(stmt);
	}

	if ( sManifestJson ) xrtFree(sManifestJson);
	if ( sGlobalFormJson ) xrtFree(sGlobalFormJson);
	if ( sInstanceFormJson ) xrtFree(sInstanceFormJson);
	if ( sEffectsJson ) xrtFree(sEffectsJson);
	if ( sHooksJson ) xrtFree(sHooksJson);
	if ( sSymbolsJson ) xrtFree(sSymbolsJson);
	if ( sPatchesJson ) xrtFree(sPatchesJson);
	if ( sContractsJson ) xrtFree(sContractsJson);
	return bOK;
}

bool ContentPack_Init()
{
	bool bOK = TRUE;
	str sPackDir = xrtPathJoin(2, AppPath, "capability-pack");

	if ( sPackDir ) {
		xrtDirCreateAll(sPackDir);
		if ( xrtDirExists(sPackDir) ) {
			xrtDirScan(sPackDir, FALSE, ContentPack_LoadDirProc, &bOK);
		}
		xrtFree(sPackDir);
	}
	return bOK;
}

int ContentPack_LoadDirProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
{
	str sManifestPath;
	str sJson;
	size_t iJsonSize = 0;
	xvalue tblPack;
	bool* pbOK = (bool*)Param;

	(void)iSize;
	(void)pData;

	if ( (sPath == NULL) || (bDir != 1) ) {
		return FALSE;
	}
	sManifestPath = xrtPathJoin(2, sPath, "pack.json");
	if ( (sManifestPath == NULL) || !xrtFileExists(sManifestPath) ) {
		if ( sManifestPath ) xrtFree(sManifestPath);
		return FALSE;
	}
	sJson = xrtFileGetAll(sManifestPath, &iJsonSize);
	xrtFree(sManifestPath);
	if ( sJson == NULL ) {
		if ( pbOK ) *pbOK = FALSE;
		return TRUE;
	}
	tblPack = xrtParseJSON(sJson, iJsonSize);
	xrtFree(sJson);
	if ( (tblPack == NULL) || (xvoType(tblPack) != XVO_DT_TABLE) ) {
		if ( tblPack ) xvoUnref(tblPack);
		if ( pbOK ) *pbOK = FALSE;
		return TRUE;
	}
	if ( !ContentPack_UpsertManifest(tblPack, sPath) && pbOK ) {
		*pbOK = FALSE;
	}
	xvoUnref(tblPack);
	return FALSE;
}

xvalue ContentPack_ListAll()
{
	sqlite3_stmt* stmt = NULL;
	xvalue arrRows = xvoCreateArray();

	if ( sqlite3_prepare_v3(G_DB, "SELECT pack_id,name,title,description,version,author,source,install_type,status,path,update_channel,update_package_id,can_update,can_uninstall,readonly,system,sort,effects_json,instance_form_json FROM content_pack ORDER BY sort ASC,id ASC", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblRow = xvoCreateTable();
			xvoTableSetText(tblRow, "packId", 6, (str)sqlite3_column_text(stmt, 0), 0, FALSE);
			xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
			xvoTableSetText(tblRow, "title", 5, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
			xvoTableSetText(tblRow, "description", 11, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
			xvoTableSetText(tblRow, "version", 7, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
			xvoTableSetText(tblRow, "author", 6, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
			xvoTableSetText(tblRow, "source", 6, (str)sqlite3_column_text(stmt, 6), 0, FALSE);
			xvoTableSetText(tblRow, "installType", 11, (str)sqlite3_column_text(stmt, 7), 0, FALSE);
			xvoTableSetText(tblRow, "status", 6, (str)sqlite3_column_text(stmt, 8), 0, FALSE);
			xvoTableSetText(tblRow, "path", 4, (str)sqlite3_column_text(stmt, 9), 0, FALSE);
			xvoTableSetText(tblRow, "updateChannel", 13, (str)sqlite3_column_text(stmt, 10), 0, FALSE);
			xvoTableSetText(tblRow, "updatePackageId", 15, (str)sqlite3_column_text(stmt, 11), 0, FALSE);
			xvoTableSetBool(tblRow, "canUpdate", 9, sqlite3_column_int(stmt, 12) != 0);
			xvoTableSetBool(tblRow, "canUninstall", 12, sqlite3_column_int(stmt, 13) != 0);
			xvoTableSetBool(tblRow, "readonly", 8, sqlite3_column_int(stmt, 14) != 0);
			xvoTableSetBool(tblRow, "system", 6, sqlite3_column_int(stmt, 15) != 0);
			xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int(stmt, 16));
			xvoTableSetText(tblRow, "effectsJson", 11, (str)sqlite3_column_text(stmt, 17), 0, FALSE);
			xvoTableSetText(tblRow, "instanceFormJson", 16, (str)sqlite3_column_text(stmt, 18), 0, FALSE);
			xvoArrayAppendValue(arrRows, tblRow, TRUE);
		}
		sqlite3_finalize(stmt);
	}
	return arrRows;
}

xvalue ContentPack_GetDetail(const char* sPackId)
{
	sqlite3_stmt* stmt = NULL;
	xvalue tblRow = NULL;

	if ( (sPackId == NULL) || (sPackId[0] == '\0') ) {
		return NULL;
	}
	if ( sqlite3_prepare_v3(G_DB, "SELECT pack_id,name,title,description,version,author,source,install_type,status,path,manifest_json,global_form_json,instance_form_json,effects_json,hooks_json,symbols_json,patches_json,contracts_json,update_channel,update_package_id,can_update,can_uninstall,readonly,system,sort FROM content_pack WHERE pack_id=? LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return NULL;
	}
	sqlite3_bind_text(stmt, 1, sPackId, -1, SQLITE_TRANSIENT);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		tblRow = xvoCreateTable();
		xvoTableSetText(tblRow, "packId", 6, (str)sqlite3_column_text(stmt, 0), 0, FALSE);
		xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
		xvoTableSetText(tblRow, "title", 5, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
		xvoTableSetText(tblRow, "description", 11, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
		xvoTableSetText(tblRow, "version", 7, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
		xvoTableSetText(tblRow, "author", 6, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
		xvoTableSetText(tblRow, "source", 6, (str)sqlite3_column_text(stmt, 6), 0, FALSE);
		xvoTableSetText(tblRow, "installType", 11, (str)sqlite3_column_text(stmt, 7), 0, FALSE);
		xvoTableSetText(tblRow, "status", 6, (str)sqlite3_column_text(stmt, 8), 0, FALSE);
		xvoTableSetText(tblRow, "path", 4, (str)sqlite3_column_text(stmt, 9), 0, FALSE);
		xvoTableSetText(tblRow, "manifestJson", 12, (str)sqlite3_column_text(stmt, 10), 0, FALSE);
		xvoTableSetText(tblRow, "globalFormJson", 14, (str)sqlite3_column_text(stmt, 11), 0, FALSE);
		xvoTableSetText(tblRow, "instanceFormJson", 16, (str)sqlite3_column_text(stmt, 12), 0, FALSE);
		xvoTableSetText(tblRow, "effectsJson", 11, (str)sqlite3_column_text(stmt, 13), 0, FALSE);
		xvoTableSetText(tblRow, "hooksJson", 9, (str)sqlite3_column_text(stmt, 14), 0, FALSE);
		xvoTableSetText(tblRow, "symbolsJson", 11, (str)sqlite3_column_text(stmt, 15), 0, FALSE);
		xvoTableSetText(tblRow, "patchesJson", 11, (str)sqlite3_column_text(stmt, 16), 0, FALSE);
		xvoTableSetText(tblRow, "contractsJson", 13, (str)sqlite3_column_text(stmt, 17), 0, FALSE);
		xvoTableSetText(tblRow, "updateChannel", 13, (str)sqlite3_column_text(stmt, 18), 0, FALSE);
		xvoTableSetText(tblRow, "updatePackageId", 15, (str)sqlite3_column_text(stmt, 19), 0, FALSE);
		xvoTableSetBool(tblRow, "canUpdate", 9, sqlite3_column_int(stmt, 20) != 0);
		xvoTableSetBool(tblRow, "canUninstall", 12, sqlite3_column_int(stmt, 21) != 0);
		xvoTableSetBool(tblRow, "readonly", 8, sqlite3_column_int(stmt, 22) != 0);
		xvoTableSetBool(tblRow, "system", 6, sqlite3_column_int(stmt, 23) != 0);
		xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int(stmt, 24));
	}
	sqlite3_finalize(stmt);
	return tblRow;
}

bool ContentPack_SaveOptions(const char* sPackId, const char* sOptionsJson)
{
	sqlite3_stmt* stmt = NULL;
	int64 iNow = xrtNow();
	bool bOK = FALSE;

	if ( (G_DB == NULL) || (sPackId == NULL) || (sPackId[0] == '\0') ) {
		return FALSE;
	}
	if ( sqlite3_prepare_v3(G_DB, "INSERT INTO content_pack_option (pack_id,options_json,create_time,update_time) VALUES (?,?,?,?) ON CONFLICT(pack_id) DO UPDATE SET options_json=excluded.options_json,update_time=excluded.update_time", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sPackId, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, sOptionsJson ? sOptionsJson : "{}", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 3, iNow);
		sqlite3_bind_int64(stmt, 4, iNow);
		bOK = (sqlite3_step(stmt) == SQLITE_DONE);
		sqlite3_finalize(stmt);
	}
	return bOK;
}

const char* ContentPack_NormalizeId(const char* sPackId)
{
	if ( sPackId == NULL ) {
		return NULL;
	}
	if ( strcmp(sPackId, "comment") == 0 ) return "content.comment";
	if ( strcmp(sPackId, "tag") == 0 ) return "content.tag";
	if ( strcmp(sPackId, "topic") == 0 ) return "content.topic";
	if ( strcmp(sPackId, "sensitive") == 0 ) return "content.sensitive";
	if ( strcmp(sPackId, "static") == 0 ) return "content.static";
	if ( strcmp(sPackId, "like") == 0 ) return "content.like";
	if ( (strcmp(sPackId, "view") == 0) || (strcmp(sPackId, "view-stat") == 0) ) return "content.view-stat";
	return sPackId;
}

bool ContentPack_Exists(const char* sPackId)
{
	sqlite3_stmt* stmt = NULL;
	bool bExists = FALSE;
	const char* sNormalized;

	if ( (G_DB == NULL) || (sPackId == NULL) || (sPackId[0] == '\0') ) {
		return FALSE;
	}
	sNormalized = ContentPack_NormalizeId(sPackId);
	if ( sqlite3_prepare_v3(G_DB, "SELECT 1 FROM content_pack WHERE pack_id=? AND status='active' LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}
	sqlite3_bind_text(stmt, 1, sNormalized, -1, SQLITE_TRANSIENT);
	bExists = (sqlite3_step(stmt) == SQLITE_ROW);
	sqlite3_finalize(stmt);
	return bExists;
}

#endif
