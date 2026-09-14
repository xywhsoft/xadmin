xvalue Content_ListRevisions(int iModelId)
{
	sqlite3_stmt* stmt = NULL;
	xvalue arrRows = xvoCreateArray();

	if ( iModelId <= 0 ) {
		return arrRows;
	}

	if ( sqlite3_prepare_v3(G_DB, "SELECT revision,note,generator_version,create_time FROM content_model_revision WHERE model_id=? ORDER BY revision DESC", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iModelId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblRow = xvoCreateTable();
			xvoTableSetInt(tblRow, "revision", 8, sqlite3_column_int(stmt, 0));
			xvoTableSetText(tblRow, "note", 4, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
			xvoTableSetText(tblRow, "generatorVersion", 16, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
			xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 3));
			xvoArrayAppendValue(arrRows, tblRow, TRUE);
		}
		sqlite3_finalize(stmt);
	}

	return arrRows;
}

xvalue Content_ListGenerations(int iModelId)
{
	sqlite3_stmt* stmt = NULL;
	xvalue arrRows = xvoCreateArray();

	if ( iModelId <= 0 ) {
		return arrRows;
	}

	if ( sqlite3_prepare_v3(G_DB, "SELECT target_revision,plugin_xid,status,output_json,error_message,create_time,finish_time FROM content_generation WHERE model_id=? ORDER BY create_time DESC,id DESC LIMIT 20", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iModelId);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblRow = xvoCreateTable();
			xvoTableSetInt(tblRow, "targetRevision", 14, sqlite3_column_int(stmt, 0));
			xvoTableSetText(tblRow, "pluginXid", 9, (str)sqlite3_column_text(stmt, 1), 0, FALSE);
			xvoTableSetText(tblRow, "status", 6, (str)sqlite3_column_text(stmt, 2), 0, FALSE);
			xvoTableSetText(tblRow, "outputJson", 10, (str)sqlite3_column_text(stmt, 3), 0, FALSE);
			xvoTableSetText(tblRow, "errorMessage", 12, (str)sqlite3_column_text(stmt, 4), 0, FALSE);
			xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 5));
			xvoTableSetInt(tblRow, "finishTime", 10, sqlite3_column_int64(stmt, 6));
			xvoArrayAppendValue(arrRows, tblRow, TRUE);
		}
		sqlite3_finalize(stmt);
	}

	return arrRows;
}
