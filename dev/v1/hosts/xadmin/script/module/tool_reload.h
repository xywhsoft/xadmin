static int ToolReload_FindToolsMenuParentID()
{
	sqlite3_stmt* stmt = NULL;
	int iParentID = 0;
	xtime now;

	if ( G_DB == NULL ) {
		return 0;
	}

	if ( sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE isDelete = 0 AND type = 0 AND title = '工具' ORDER BY id ASC LIMIT 1;", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iParentID = sqlite3_column_int(stmt, 0);
		}
	}
	if ( stmt ) {
		sqlite3_finalize(stmt);
	}

	if ( iParentID > 0 ) {
		return iParentID;
	}

	now = xrtNow();
	if ( sqlite3_prepare_v3(G_DB,
		"INSERT INTO menu (parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete) "
		"VALUES (0, '工具', 'layui-icon layui-icon-util', 0, '', '', 700000, 1, '系统工具入口', ?, ?, 0);",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, now);
		sqlite3_bind_int64(stmt, 2, now);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		return (int)sqlite3_last_insert_rowid(G_DB);
	}

	return iParentID;
}

static void ToolReload_EnsureMenuItem(const char* sTitle, const char* sIcon, const char* sHref, int iParentID, int iSort, const char* sRemark)
{
	sqlite3_stmt* stmt = NULL;
	int iMenuID = 0;
	xtime now = xrtNow();

	if ( (G_DB == NULL) || (sTitle == NULL) || (sHref == NULL) ) {
		return;
	}

	if ( sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE isDelete = 0 AND href = ? ORDER BY id ASC LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sHref, -1, SQLITE_STATIC);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iMenuID = sqlite3_column_int(stmt, 0);
		}
	}
	if ( stmt ) {
		sqlite3_finalize(stmt);
		stmt = NULL;
	}

	if ( iMenuID > 0 ) {
		if ( sqlite3_prepare_v3(G_DB,
			"UPDATE menu SET parent = ?, title = ?, icon = ?, type = 1, openType = '_component', href = ?, sort = ?, visible = 1, remark = ?, updateTime = ? WHERE id = ?",
			-1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int(stmt, 1, iParentID);
			sqlite3_bind_text(stmt, 2, sTitle, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 3, sIcon ? sIcon : "", -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 4, sHref, -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 5, iSort);
			sqlite3_bind_text(stmt, 6, sRemark ? sRemark : "", -1, SQLITE_STATIC);
			sqlite3_bind_int64(stmt, 7, now);
			sqlite3_bind_int(stmt, 8, iMenuID);
			sqlite3_step(stmt);
		}
		if ( stmt ) {
			sqlite3_finalize(stmt);
		}
		return;
	}

	if ( sqlite3_prepare_v3(G_DB,
		"INSERT INTO menu (parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete) "
		"VALUES (?, ?, ?, 1, '_component', ?, ?, 1, ?, ?, ?, 0)",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iParentID);
		sqlite3_bind_text(stmt, 2, sTitle, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 3, sIcon ? sIcon : "", -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 4, sHref, -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt, 5, iSort);
		sqlite3_bind_text(stmt, 6, sRemark ? sRemark : "", -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt, 7, now);
		sqlite3_bind_int64(stmt, 8, now);
		sqlite3_step(stmt);
	}
	if ( stmt ) {
		sqlite3_finalize(stmt);
	}
}

static void ToolReload_EnsureMenus()
{
	int iParentID = ToolReload_FindToolsMenuParentID();
	if ( iParentID <= 0 ) {
		return;
	}

	ToolReload_EnsureMenuItem("重新加载", "layui-icon layui-icon-refresh-3", "/admin/view/tool/reload", iParentID, 701100, "模板缓存与脚本宿主重载工具");
}

void ToolReload_Init()
{
	ToolReload_EnsureMenus();
}

void ToolReload_Unit()
{
}
