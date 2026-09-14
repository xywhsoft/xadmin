#include "content_spec.h"

bool Content_IsValidXid(const char* sXid)
{
	size_t iLen;

	if ( (sXid == NULL) || (sXid[0] == '\0') ) {
		return FALSE;
	}
	iLen = strlen(sXid);
	if ( (iLen <= 0) || (iLen > 96) ) {
		return FALSE;
	}
	for ( size_t i = 0; i < iLen; i++ ) {
		char ch = sXid[i];
		if ( ((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) || ((ch >= '0') && (ch <= '9')) || (ch == '.') || (ch == '_') || (ch == '-') ) {
			continue;
		}
		return FALSE;
	}
	return TRUE;
}

void Content_BindText(sqlite3_stmt* stmt, int iIndex, const char* sText)
{
	sqlite3_bind_text(stmt, iIndex, sText ? sText : "", -1, SQLITE_TRANSIENT);
}

int Content_CountFields(xvalue tblSpec)
{
	xvalue arrFields;

	if ( (tblSpec == NULL) || (xvoType(tblSpec) != XVO_DT_TABLE) ) {
		return 0;
	}
	arrFields = xvoTableGetValue(tblSpec, "fields", 6);
	if ( (arrFields != NULL) && (xvoType(arrFields) == XVO_DT_ARRAY) ) {
		return (int)xvoArrayItemCount(arrFields);
	}
	return 0;
}

bool Content_SyncModelCapabilities(int iModelId, xvalue tblSpec, int64 iNow)
{
	sqlite3_stmt* stmt = NULL;
	xvalue arrCapabilities;

	if ( (G_DB == NULL) || (iModelId <= 0) ) {
		return FALSE;
	}

	if ( sqlite3_prepare_v3(G_DB, "DELETE FROM content_model_pack WHERE model_id=?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}
	sqlite3_bind_int(stmt, 1, iModelId);
	if ( sqlite3_step(stmt) != SQLITE_DONE ) {
		sqlite3_finalize(stmt);
		return FALSE;
	}
	sqlite3_finalize(stmt);

	if ( (tblSpec == NULL) || (xvoType(tblSpec) != XVO_DT_TABLE) ) {
		return TRUE;
	}

	arrCapabilities = xvoTableGetValue(tblSpec, "capabilities", 12);
	if ( (arrCapabilities == NULL) || (xvoType(arrCapabilities) != XVO_DT_ARRAY) ) {
		return TRUE;
	}

	for ( uint32 i = 0; i < xvoArrayItemCount(arrCapabilities); i++ ) {
		xvalue tblItem = xvoArrayGetValue(arrCapabilities, i);
		str sKey = NULL;
		str sConfigJson = NULL;
		str sMountJson = NULL;
		xvalue tblConfig = NULL;
		xvalue tblMount = NULL;
		bool bEnabled = TRUE;

		if ( (tblItem == NULL) || (xvoType(tblItem) != XVO_DT_TABLE) ) {
			continue;
		}

		sKey = xvoTableGetText(tblItem, "key", 3);
		sKey = (str)ContentPack_NormalizeId(sKey);
		if ( (sKey == NULL) || (sKey[0] == '\0') ) {
			continue;
		}
		if ( xvoTableExists(tblItem, "enabled", 7) ) {
			bEnabled = xvoTableGetBool(tblItem, "enabled", 7);
		}
		if ( !bEnabled ) {
			continue;
		}

		tblConfig = xvoTableGetValue(tblItem, "config", 6);
		tblMount = xvoTableGetValue(tblItem, "mount", 5);
		sConfigJson = tblConfig ? xrtStringifyJSON(tblConfig, FALSE, NULL) : xrtCopyStr("{}", 0);
		sMountJson = tblMount ? xrtStringifyJSON(tblMount, FALSE, NULL) : xrtCopyStr("{}", 0);

		if ( sqlite3_prepare_v3(G_DB, "INSERT INTO content_model_pack (model_id,pack_id,enabled,instance_options_json,mount_json,sort,create_time,update_time) VALUES (?,?,1,?,?,?, ?, ?)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			if ( sConfigJson ) xrtFree(sConfigJson);
			if ( sMountJson ) xrtFree(sMountJson);
			return FALSE;
		}
		sqlite3_bind_int(stmt, 1, iModelId);
		Content_BindText(stmt, 2, sKey);
		Content_BindText(stmt, 3, sConfigJson);
		Content_BindText(stmt, 4, sMountJson);
		sqlite3_bind_int(stmt, 5, (int)i);
		sqlite3_bind_int64(stmt, 6, iNow);
		sqlite3_bind_int64(stmt, 7, iNow);
		if ( sqlite3_step(stmt) != SQLITE_DONE ) {
			sqlite3_finalize(stmt);
			if ( sConfigJson ) xrtFree(sConfigJson);
			if ( sMountJson ) xrtFree(sMountJson);
			return FALSE;
		}
		sqlite3_finalize(stmt);

		if ( sConfigJson ) xrtFree(sConfigJson);
		if ( sMountJson ) xrtFree(sMountJson);
	}

	return TRUE;
}

xvalue Content_RowToModel(sqlite3_stmt* stmt)
{
	xvalue tblRow = xvoCreateTable();
	xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int(stmt, 0));
	xvoTableSetText(tblRow, "xid", 3, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
	xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
	xvoTableSetText(tblRow, "namespace", 9, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
	xvoTableSetText(tblRow, "title", 5, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
	xvoTableSetText(tblRow, "description", 11, (str)sqlite3_column_text(stmt, 5), 0, FALSE);
	xvoTableSetText(tblRow, "icon", 4, (str)sqlite3_column_text(stmt, 6), 0, FALSE);
	xvoTableSetText(tblRow, "tableName", 9, (str)sqlite3_column_text(stmt, 7), 0, FALSE);
	xvoTableSetText(tblRow, "status", 6, (str)sqlite3_column_text(stmt, 8), 0, FALSE);
	xvoTableSetInt(tblRow, "fieldCount", 10, sqlite3_column_int(stmt, 9));
	xvoTableSetInt(tblRow, "currentRevision", 15, sqlite3_column_int(stmt, 10));
	xvoTableSetInt(tblRow, "appliedRevision", 15, sqlite3_column_int(stmt, 11));
	xvoTableSetText(tblRow, "generatedPluginXid", 18, (str)sqlite3_column_text(stmt, 12), 0, FALSE);
	xvoTableSetText(tblRow, "specJson", 8, (str)sqlite3_column_text(stmt, 13), 0, FALSE);
	xvoTableSetText(tblRow, "specHash", 8, (str)sqlite3_column_text(stmt, 14), 0, FALSE);
	xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 15));
	xvoTableSetInt(tblRow, "updateTime", 10, sqlite3_column_int64(stmt, 16));
	return tblRow;
}

bool Content_FindModelIdAndRevision(const char* sXid, int* piModelId, int* piRevision)
{
	sqlite3_stmt* stmt = NULL;
	bool bFound = FALSE;

	if ( (G_DB == NULL) || (sXid == NULL) ) {
		return FALSE;
	}
	if ( sqlite3_prepare_v3(G_DB, "SELECT id,current_revision FROM content_model WHERE xid = ? AND status <> 'deleted' LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}
	Content_BindText(stmt, 1, sXid);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		if ( piModelId ) *piModelId = sqlite3_column_int(stmt, 0);
		if ( piRevision ) *piRevision = sqlite3_column_int(stmt, 1);
		bFound = TRUE;
	}
	sqlite3_finalize(stmt);
	return bFound;
}

xvalue Content_ListModels(int iPage, int iLimit)
{
	sqlite3_stmt* stmt = NULL;
	xvalue tblData = xvoCreateTable();
	xvalue arrRows = xvoCreateArray();
	int iOffset;
	int iTotal = 0;

	if ( iPage <= 0 ) iPage = 1;
	if ( iLimit <= 0 || iLimit > 100 ) iLimit = 20;
	iOffset = (iPage - 1) * iLimit;

	if ( sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM content_model WHERE status <> 'deleted'", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iTotal = sqlite3_column_int(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}

	if ( sqlite3_prepare_v3(G_DB, "SELECT id,xid,name,namespace,title,description,icon,table_name,status,field_count,current_revision,applied_revision,generated_plugin_xid,spec_json,spec_hash,create_time,update_time FROM content_model WHERE status <> 'deleted' ORDER BY update_time DESC,id DESC LIMIT ? OFFSET ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iLimit);
		sqlite3_bind_int(stmt, 2, iOffset);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvoArrayAppendValue(arrRows, Content_RowToModel(stmt), TRUE);
		}
		sqlite3_finalize(stmt);
	}

	xvoTableSetInt(tblData, "count", 5, iTotal);
	xvoTableSetValue(tblData, "items", 5, arrRows, TRUE);
	return tblData;
}

xvalue Content_GetModelByXid(const char* sXid)
{
	sqlite3_stmt* stmt = NULL;
	xvalue tblModel = NULL;

	if ( !Content_IsValidXid(sXid) ) {
		return NULL;
	}

	if ( sqlite3_prepare_v3(G_DB, "SELECT id,xid,name,namespace,title,description,icon,table_name,status,field_count,current_revision,applied_revision,generated_plugin_xid,spec_json,spec_hash,create_time,update_time FROM content_model WHERE xid = ? AND status <> 'deleted' LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return NULL;
	}
	Content_BindText(stmt, 1, sXid);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		tblModel = Content_RowToModel(stmt);
	}
	sqlite3_finalize(stmt);
	return tblModel;
}

bool Content_DeleteModelByXid(const char* sXid, char** psError)
{
	sqlite3_stmt* stmt = NULL;
	int iModelId = 0;
	int64 iNow = xrtNow();
	bool bOK = FALSE;

	if ( psError ) {
		*psError = NULL;
	}
	if ( !Content_IsValidXid(sXid) ) {
		if ( psError ) *psError = xrtCopyStr("invalid xid", 0);
		return FALSE;
	}
	if ( !Content_FindModelIdAndRevision(sXid, &iModelId, NULL) || (iModelId <= 0) ) {
		if ( psError ) *psError = xrtCopyStr("model not found", 0);
		return FALSE;
	}
	if ( !ContentDB_BeginImmediate() ) {
		if ( psError ) *psError = xrtCopyStr("begin transaction failed", 0);
		return FALSE;
	}
	if ( sqlite3_prepare_v3(G_DB, "UPDATE content_model SET status='deleted', update_time=? WHERE id=? AND status <> 'deleted'", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		ContentDB_Rollback();
		if ( psError ) *psError = xrtCopyStr("prepare delete failed", 0);
		return FALSE;
	}
	sqlite3_bind_int64(stmt, 1, iNow);
	sqlite3_bind_int(stmt, 2, iModelId);
	bOK = (sqlite3_step(stmt) == SQLITE_DONE) && (sqlite3_changes(G_DB) > 0);
	sqlite3_finalize(stmt);
	stmt = NULL;
	if ( !bOK ) {
		ContentDB_Rollback();
		if ( psError ) *psError = xrtCopyStr("delete model failed", 0);
		return FALSE;
	}
	if ( sqlite3_prepare_v3(G_DB, "DELETE FROM content_model_pack WHERE model_id=?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iModelId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	ContentDB_Commit();
	return TRUE;
}

xvalue Content_SaveModelSpec(xvalue tblSpec, char** psError)
{
	str sSpecJson;
	str sSpecHash;
	str sOldSpecHash = NULL;
	size_t iSpecSize = 0;
	str sXid;
	str sName;
	str sTitle;
	str sNamespace;
	str sDescription;
	str sIcon;
	str sTableName;
	str sGeneratedPluginXid;
	str sNote;
	int iFieldCount;
	int iModelId = 0;
	int iCurrentRevision = 0;
	int iNewRevision;
	int64 iNow = xrtNow();
	sqlite3_stmt* stmt = NULL;
	xvalue tblData;

	if ( psError ) {
		*psError = NULL;
	}

	if ( (tblSpec == NULL) || (xvoType(tblSpec) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("invalid json body", 0);
		return NULL;
	}

	sXid = xvoTableGetText(tblSpec, "xid", 3);
	sName = xvoTableGetText(tblSpec, "name", 4);
	sTitle = xvoTableGetText(tblSpec, "title", 5);
	sNamespace = xvoTableGetText(tblSpec, "namespace", 9);
	sDescription = xvoTableGetText(tblSpec, "description", 11);
	if ( sDescription == NULL ) sDescription = xvoTableGetText(tblSpec, "desc", 4);
	sIcon = xvoTableGetText(tblSpec, "icon", 4);
	sTableName = xvoTableGetText(tblSpec, "tableName", 9);
	if ( sTableName == NULL ) sTableName = xvoTableGetText(tblSpec, "table_name", 10);
	sGeneratedPluginXid = xvoTableGetText(tblSpec, "generatedPluginXid", 18);
	sNote = xvoTableGetText(tblSpec, "note", 4);

	if ( !Content_IsValidXid(sXid) ) {
		if ( psError ) *psError = xrtCopyStr("invalid xid", 0);
		return NULL;
	}
	if ( (sName == NULL) || (sName[0] == '\0') ) {
		sName = sXid;
	}
	if ( (sTitle == NULL) || (sTitle[0] == '\0') ) {
		if ( psError ) *psError = xrtCopyStr("title is required", 0);
		return NULL;
	}
	if ( !Content_SpecValidate(tblSpec, psError) ) {
		return NULL;
	}

	iFieldCount = Content_CountFields(tblSpec);
	sSpecJson = xrtStringifyJSON(tblSpec, FALSE, &iSpecSize);
	if ( sSpecJson == NULL ) {
		if ( psError ) *psError = xrtCopyStr("stringify spec failed", 0);
		return NULL;
	}
	sSpecHash = Content_SpecHashText(sSpecJson);
	if ( sSpecHash == NULL ) {
		xrtFree(sSpecJson);
		if ( psError ) *psError = xrtCopyStr("hash spec failed", 0);
		return NULL;
	}

	Content_FindModelIdAndRevision(sXid, &iModelId, &iCurrentRevision);
	iNewRevision = iCurrentRevision + 1;

	if ( iModelId > 0 ) {
		if ( sqlite3_prepare_v3(G_DB, "SELECT spec_hash FROM content_model WHERE id=? LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int(stmt, 1, iModelId);
			if ( sqlite3_step(stmt) == SQLITE_ROW ) {
				sOldSpecHash = xrtCopyStr((str)sqlite3_column_text(stmt, 0), 0);
			}
			sqlite3_finalize(stmt);
		}
		if ( sOldSpecHash && (strcmp((const char*)sOldSpecHash, (const char*)sSpecHash) == 0) ) {
			tblData = xvoCreateTable();
			xvoTableSetInt(tblData, "id", 2, iModelId);
			xvoTableSetText(tblData, "xid", 3, sXid, 0, FALSE);
			xvoTableSetInt(tblData, "revision", 8, iCurrentRevision);
			xvoTableSetInt(tblData, "fieldCount", 10, iFieldCount);
			xvoTableSetInt(tblData, "updateTime", 10, iNow);
			xvoTableSetBool(tblData, "unchanged", 9, TRUE);
			xrtFree(sOldSpecHash);
			xrtFree(sSpecHash);
			xrtFree(sSpecJson);
			return tblData;
		}
		if ( sOldSpecHash ) {
			xrtFree(sOldSpecHash);
			sOldSpecHash = NULL;
		}
	}

	if ( !ContentDB_BeginImmediate() ) {
		xrtFree(sSpecHash);
		xrtFree(sSpecJson);
		if ( psError ) *psError = xrtCopyStr("begin transaction failed", 0);
		return NULL;
	}

	if ( iModelId > 0 ) {
		if ( sqlite3_prepare_v3(G_DB, "UPDATE content_model SET name=?,namespace=?,title=?,description=?,icon=?,table_name=?,field_count=?,current_revision=?,generated_plugin_xid=?,spec_json=?,spec_hash=?,update_time=? WHERE id=?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			ContentDB_Rollback();
			xrtFree(sSpecHash);
			xrtFree(sSpecJson);
			if ( psError ) *psError = xrtCopyStr("prepare update failed", 0);
			return NULL;
		}
		Content_BindText(stmt, 1, sName);
		Content_BindText(stmt, 2, sNamespace);
		Content_BindText(stmt, 3, sTitle);
		Content_BindText(stmt, 4, sDescription);
		Content_BindText(stmt, 5, sIcon);
		Content_BindText(stmt, 6, sTableName);
		sqlite3_bind_int(stmt, 7, iFieldCount);
		sqlite3_bind_int(stmt, 8, iNewRevision);
		Content_BindText(stmt, 9, sGeneratedPluginXid);
		Content_BindText(stmt, 10, sSpecJson);
		Content_BindText(stmt, 11, sSpecHash);
		sqlite3_bind_int64(stmt, 12, iNow);
		sqlite3_bind_int(stmt, 13, iModelId);
	} else {
		if ( sqlite3_prepare_v3(G_DB, "INSERT INTO content_model (xid,name,namespace,title,description,icon,table_name,status,field_count,current_revision,applied_revision,generated_plugin_xid,spec_json,spec_hash,create_time,update_time) VALUES (?,?,?,?,?,?,?,'active',?,1,0,?,?,?,?,?)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			ContentDB_Rollback();
			xrtFree(sSpecHash);
			xrtFree(sSpecJson);
			if ( psError ) *psError = xrtCopyStr("prepare insert failed", 0);
			return NULL;
		}
		Content_BindText(stmt, 1, sXid);
		Content_BindText(stmt, 2, sName);
		Content_BindText(stmt, 3, sNamespace);
		Content_BindText(stmt, 4, sTitle);
		Content_BindText(stmt, 5, sDescription);
		Content_BindText(stmt, 6, sIcon);
		Content_BindText(stmt, 7, sTableName);
		sqlite3_bind_int(stmt, 8, iFieldCount);
		Content_BindText(stmt, 9, sGeneratedPluginXid);
		Content_BindText(stmt, 10, sSpecJson);
		Content_BindText(stmt, 11, sSpecHash);
		sqlite3_bind_int64(stmt, 12, iNow);
		sqlite3_bind_int64(stmt, 13, iNow);
	}

	if ( sqlite3_step(stmt) != SQLITE_DONE ) {
		sqlite3_finalize(stmt);
		ContentDB_Rollback();
		xrtFree(sSpecHash);
		xrtFree(sSpecJson);
		if ( psError ) *psError = xrtCopyStr("save model failed", 0);
		return NULL;
	}
	sqlite3_finalize(stmt);
	if ( iModelId <= 0 ) {
		iModelId = (int)sqlite3_last_insert_rowid(G_DB);
	}

	if ( sqlite3_prepare_v3(G_DB, "INSERT INTO content_model_revision (model_id,revision,spec_json,spec_hash,note,generator_version,create_time) VALUES (?,?,?,?,?,'builtin-0.1',?)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		ContentDB_Rollback();
		xrtFree(sSpecHash);
		xrtFree(sSpecJson);
		if ( psError ) *psError = xrtCopyStr("prepare revision failed", 0);
		return NULL;
	}
	sqlite3_bind_int(stmt, 1, iModelId);
	sqlite3_bind_int(stmt, 2, iNewRevision);
	Content_BindText(stmt, 3, sSpecJson);
	Content_BindText(stmt, 4, sSpecHash);
	Content_BindText(stmt, 5, sNote);
	sqlite3_bind_int64(stmt, 6, iNow);
	if ( sqlite3_step(stmt) != SQLITE_DONE ) {
		sqlite3_finalize(stmt);
		ContentDB_Rollback();
		xrtFree(sSpecHash);
		xrtFree(sSpecJson);
		if ( psError ) *psError = xrtCopyStr("save revision failed", 0);
		return NULL;
	}
	sqlite3_finalize(stmt);

	if ( !Content_SyncModelCapabilities(iModelId, tblSpec, iNow) ) {
		ContentDB_Rollback();
		xrtFree(sSpecHash);
		xrtFree(sSpecJson);
		if ( psError ) *psError = xrtCopyStr("sync capabilities failed", 0);
		return NULL;
	}

	ContentDB_Commit();

	tblData = xvoCreateTable();
	xvoTableSetInt(tblData, "id", 2, iModelId);
	xvoTableSetText(tblData, "xid", 3, sXid, 0, FALSE);
	xvoTableSetInt(tblData, "revision", 8, iNewRevision);
	xvoTableSetInt(tblData, "fieldCount", 10, iFieldCount);
	xvoTableSetInt(tblData, "updateTime", 10, iNow);
	xvoTableSetText(tblData, "specHash", 8, sSpecHash, 0, FALSE);
	xrtFree(sSpecHash);
	xrtFree(sSpecJson);
	return tblData;
}
