



// 菜单管理预编译 SQL 语句句柄
sqlite3_stmt* stmt_menu_all = NULL;
sqlite3_stmt* stmt_menu_get = NULL;
sqlite3_stmt* stmt_menu_add = NULL;
sqlite3_stmt* stmt_menu_put = NULL;
sqlite3_stmt* stmt_menu_del = NULL;
sqlite3_stmt* stmt_menu_chk = NULL;
sqlite3_stmt* stmt_menu_tree = NULL;



// 预编译菜单管理 SQL 语句
void Menu_CompileSQL()
{
	int iRet = sqlite3_prepare_v3(G_DB, "SELECT id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime FROM menu WHERE isDelete = 0 ORDER BY sort ASC, id ASC", -1, SQL_PREPARE_DEFAULT, &stmt_menu_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Menu_CompileSQL [stmt_menu_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}

	iRet = sqlite3_prepare_v3(G_DB, "SELECT id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime FROM menu WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt_menu_get, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Menu_CompileSQL [stmt_menu_get] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}

	iRet = sqlite3_prepare_v3(G_DB, "INSERT INTO menu (parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)", -1, SQL_PREPARE_DEFAULT, &stmt_menu_add, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Menu_CompileSQL [stmt_menu_add] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}

	iRet = sqlite3_prepare_v3(G_DB, "UPDATE menu SET parent = ?, title = ?, icon = ?, type = ?, openType = ?, href = ?, sort = ?, visible = ?, remark = ?, updateTime = ? WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt_menu_put, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Menu_CompileSQL [stmt_menu_put] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}

	iRet = sqlite3_prepare_v3(G_DB, "UPDATE menu SET isDelete = 1, updateTime = ? WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt_menu_del, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Menu_CompileSQL [stmt_menu_del] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}

	iRet = sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) as cnt FROM menu WHERE parent = ? AND isDelete = 0", -1, SQL_PREPARE_DEFAULT, &stmt_menu_chk, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Menu_CompileSQL [stmt_menu_chk] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}

	iRet = sqlite3_prepare_v3(G_DB, "SELECT id, parent, title, icon, type, openType, href, sort FROM menu WHERE isDelete = 0 AND visible = 1 ORDER BY sort ASC, id ASC", -1, SQL_PREPARE_DEFAULT, &stmt_menu_tree, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Menu_CompileSQL [stmt_menu_tree] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
}



// 构建菜单树（递归）
void Menu_BuildTree_Recursive(xvalue* arrResult, xvalue* arrAll, int iParentID)
{
	uint32 iCount = ValueCount(arrAll);

	for ( uint32 i = 0; i < iCount; i++ ) {
		xvalue* tblItem = xrtValueArrayGet(arrAll, i);
		int iItemParent = ValueInt(tblItem, "parent");

		if ( iItemParent == iParentID ) {
			xvalue* tblMenu = ValueObject();

			int iID = ValueInt(tblItem, "id");
			ValueSetInt(tblMenu, "id", iID);
			ValueSetText(tblMenu, "title", ValueText(tblItem, "title"));

			// 确保 icon 字段始终输出，即使为空
			str sIcon = ValueText(tblItem, "icon");
			ValueSetText(tblMenu, "icon", sIcon ? (ptr)sIcon : (str)"");

			ValueSetInt(tblMenu, "type", ValueInt(tblItem, "type"));

			str sOpenType = ValueText(tblItem, "openType");
			if ( sOpenType && (strlen(sOpenType) > 0) ) {
				ValueSetText(tblMenu, "openType", sOpenType);
			}

			str sHref = ValueText(tblItem, "href");
			if ( sHref && (strlen(sHref) > 0) ) {
				ValueSetText(tblMenu, "href", sHref);
			}

			xvalue* arrChildren = ValueArray();
			Menu_BuildTree_Recursive(arrChildren, arrAll, iID);

			if ( ValueCount(arrChildren) > 0 ) {
				ValueSetOwn(tblMenu, "children", arrChildren);
			} else {
				xrtValueRelease(arrChildren);
			}

			ValueArrayOwn(arrResult, tblMenu);
		}
	}
}



// 构建菜单树（入口函数，只返回可见且启用的菜单）
xvalue* Menu_BuildTree()
{
	xvalue* arrAll = ValueArray();

	while ( sqlite3_step(stmt_menu_tree) == SQLITE_ROW ) {
		xvalue* tblRow = ValueObject();
		ValueSetInt(tblRow, "id", sqlite3_column_int(stmt_menu_tree, 0));
		ValueSetInt(tblRow, "parent", sqlite3_column_int(stmt_menu_tree, 1));
		ValueSetText(tblRow, "title", (char*)sqlite3_column_text(stmt_menu_tree, 2));

		str sIcon = (str)sqlite3_column_text(stmt_menu_tree, 3);
		ValueSetText(tblRow, "icon", sIcon ? sIcon : (str)"");

		ValueSetInt(tblRow, "type", sqlite3_column_int(stmt_menu_tree, 4));
		ValueSetText(tblRow, "openType", (char*)sqlite3_column_text(stmt_menu_tree, 5));
		ValueSetText(tblRow, "href", (char*)sqlite3_column_text(stmt_menu_tree, 6));
		ValueSetInt(tblRow, "sort", sqlite3_column_int(stmt_menu_tree, 7));
		ValueArrayOwn(arrAll, tblRow);
	}
	sqlite3_reset(stmt_menu_tree);

	xvalue* arrResult = ValueArray();

	xvalue* tblHome = ValueObject();
	ValueSetText(tblHome, "id", "home");
	ValueSetText(tblHome, "title", "主页");
	ValueSetText(tblHome, "icon", "layui-icon layui-icon-home");
	ValueSetInt(tblHome, "type", 1);
	ValueSetText(tblHome, "openType", "_iframe");
	ValueSetText(tblHome, "href", "/admin/view/home");
	ValueArrayOwn(arrResult, tblHome);

	Menu_BuildTree_Recursive(arrResult, arrAll, 0);

	xrtValueRelease(arrAll);
	return arrResult;
}



// 检查菜单是否有子菜单
int Menu_HasChildren(int iID)
{
	sqlite3_bind_int(stmt_menu_chk, 1, iID);
	int iCount = 0;
	if ( sqlite3_step(stmt_menu_chk) == SQLITE_ROW ) {
		iCount = sqlite3_column_int(stmt_menu_chk, 0);
	}
	sqlite3_reset(stmt_menu_chk);
	return iCount;
}



static void Menu_EnsureOptionManagerMenu()
{
	sqlite3_stmt* stmt = NULL;
	int iRet;
	int iMenuID = 0;
	int iParentID = 0;
	xtime now = XAdmin_UnixNowUs();

	iRet = sqlite3_prepare_v3(G_DB, "SELECT id, parent FROM menu WHERE isDelete = 0 AND title = '设置管理' ORDER BY id ASC LIMIT 1;", -1, SQL_PREPARE_DEFAULT, &stmt, NULL);
	if ( iRet == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iMenuID = sqlite3_column_int(stmt, 0);
			iParentID = sqlite3_column_int(stmt, 1);
		}
		sqlite3_finalize(stmt);
	}

	if ( iParentID <= 0 ) {
		iRet = sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE isDelete = 0 AND type = 0 AND title = '设置' ORDER BY id ASC LIMIT 1;", -1, SQL_PREPARE_DEFAULT, &stmt, NULL);
		if ( iRet == SQLITE_OK ) {
			if ( sqlite3_step(stmt) == SQLITE_ROW ) {
				iParentID = sqlite3_column_int(stmt, 0);
			}
			sqlite3_finalize(stmt);
		}
	}

	if ( iMenuID > 0 ) {
		iRet = sqlite3_prepare_v3(G_DB, "UPDATE menu SET parent = ?, icon = 'layui-icon layui-icon-set-fill', openType = '_component', href = '/admin/view/option/files', visible = 1, remark = '管理设置文件和配置结构', updateTime = ? WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt, NULL);
		if ( iRet == SQLITE_OK ) {
			sqlite3_bind_int(stmt, 1, iParentID);
			sqlite3_bind_int64(stmt, 2, now);
			sqlite3_bind_int(stmt, 3, iMenuID);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		return;
	}

	iRet = sqlite3_prepare_v3(G_DB, "INSERT INTO menu (parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete) VALUES (?, '设置管理', 'layui-icon layui-icon-set-fill', 1, '_component', '/admin/view/option/files', 600900, 1, '管理设置文件和配置结构', ?, ?, 0);", -1, SQL_PREPARE_DEFAULT, &stmt, NULL);
	if ( iRet == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iParentID);
		sqlite3_bind_int64(stmt, 2, now);
		sqlite3_bind_int64(stmt, 3, now);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
}



// 初始化菜单模块
void Menu_Init()
{
	printf("        Menu_Init \n");
	Menu_CompileSQL();
}



// 卸载菜单模块
void Menu_Unit()
{
	printf("        Menu_Unit \n");
	sqlite3_finalize(stmt_menu_all);
	sqlite3_finalize(stmt_menu_get);
	sqlite3_finalize(stmt_menu_add);
	sqlite3_finalize(stmt_menu_put);
	sqlite3_finalize(stmt_menu_del);
	sqlite3_finalize(stmt_menu_chk);
	sqlite3_finalize(stmt_menu_tree);
}
