



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
	int iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime FROM menu WHERE isDelete = 0 ORDER BY sort ASC", -1, SQL_PREPARE_DEFAULT, &stmt_menu_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Menu_CompileSQL [stmt_menu_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT id, parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime FROM menu WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt_menu_get, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Menu_CompileSQL [stmt_menu_get] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	
	iRet = sqlite3_prepare_v3(G_DB->objDB, "INSERT INTO menu (parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, plugin_id, isDelete) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)", -1, SQL_PREPARE_DEFAULT, &stmt_menu_add, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Menu_CompileSQL [stmt_menu_add] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	
	iRet = sqlite3_prepare_v3(G_DB->objDB, "UPDATE menu SET parent = ?, title = ?, icon = ?, type = ?, openType = ?, href = ?, sort = ?, visible = ?, remark = ?, updateTime = ?, plugin_id = ? WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt_menu_put, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Menu_CompileSQL [stmt_menu_put] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	
	iRet = sqlite3_prepare_v3(G_DB->objDB, "UPDATE menu SET isDelete = 1, updateTime = ? WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt_menu_del, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Menu_CompileSQL [stmt_menu_del] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT COUNT(*) as cnt FROM menu WHERE parent = ? AND isDelete = 0", -1, SQL_PREPARE_DEFAULT, &stmt_menu_chk, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Menu_CompileSQL [stmt_menu_chk] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT id, parent, title, icon, type, openType, href, sort FROM menu WHERE isDelete = 0 AND visible = 1 ORDER BY sort ASC", -1, SQL_PREPARE_DEFAULT, &stmt_menu_tree, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Menu_CompileSQL [stmt_menu_tree] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
}



// 构建菜单树（递归）
void Menu_BuildTree_Recursive(xvalue arrResult, xvalue arrAll, int iParentID)
{
	uint32 iCount = xvoArrayItemCount(arrAll);
	
	for ( uint32 i = 0; i < iCount; i++ ) {
		xvalue tblItem = xvoArrayGetValue(arrAll, i);
		int iItemParent = xvoTableGetInt(tblItem, "parent", 6);
		
		if ( iItemParent == iParentID ) {
			xvalue tblMenu = xvoCreateTable();
			
			int iID = xvoTableGetInt(tblItem, "id", 2);
			xvoTableSetInt(tblMenu, "id", 2, iID);
			xvoTableSetText(tblMenu, "title", 5, xvoTableGetText(tblItem, "title", 5), 0, FALSE);
			
			// 确保 icon 字段始终输出（即使为空）
			str sIcon = xvoTableGetText(tblItem, "icon", 4);
			xvoTableSetText(tblMenu, "icon", 4, sIcon ? (ptr)sIcon : (str)"", 0, FALSE);
			
			xvoTableSetInt(tblMenu, "type", 4, xvoTableGetInt(tblItem, "type", 4));
			
			str sOpenType = xvoTableGetText(tblItem, "openType", 8);
			if ( sOpenType && (strlen(sOpenType) > 0) ) {
				xvoTableSetText(tblMenu, "openType", 8, sOpenType, 0, FALSE);
			}
			
			str sHref = xvoTableGetText(tblItem, "href", 4);
			if ( sHref && (strlen(sHref) > 0) ) {
				xvoTableSetText(tblMenu, "href", 4, sHref, 0, FALSE);
			}
			
			// 递归获取子菜单
			xvalue arrChildren = xvoCreateArray();
			Menu_BuildTree_Recursive(arrChildren, arrAll, iID);
			
			if ( xvoArrayItemCount(arrChildren) > 0 ) {
				xvoTableSetValue(tblMenu, "children", 8, arrChildren, TRUE);
			} else {
				xvoUnref(arrChildren);
			}
			
			xvoArrayAppendValue(arrResult, tblMenu, TRUE);
		}
	}
}



// 构建菜单树（入口函数，只返回可见且启用的菜单）
xvalue Menu_BuildTree()
{
	xvalue arrAll = xvoCreateArray();
	
	// 查询所有可见且启用的菜单
	while ( sqlite3_step(stmt_menu_tree) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int(stmt_menu_tree, 0));
		xvoTableSetInt(tblRow, "parent", 6, sqlite3_column_int(stmt_menu_tree, 1));
		xvoTableSetText(tblRow, "title", 5, (char*)sqlite3_column_text(stmt_menu_tree, 2), 0, FALSE);
		
		// 确保 icon 字段始终有值
		str sIcon = (str)sqlite3_column_text(stmt_menu_tree, 3);
		xvoTableSetText(tblRow, "icon", 4, sIcon ? sIcon : (str)"", 0, FALSE);
		
		xvoTableSetInt(tblRow, "type", 4, sqlite3_column_int(stmt_menu_tree, 4));
		xvoTableSetText(tblRow, "openType", 8, (char*)sqlite3_column_text(stmt_menu_tree, 5), 0, FALSE);
		xvoTableSetText(tblRow, "href", 4, (char*)sqlite3_column_text(stmt_menu_tree, 6), 0, FALSE);
		xvoTableSetInt(tblRow, "sort", 4, sqlite3_column_int(stmt_menu_tree, 7));
		xvoArrayAppendValue(arrAll, tblRow, TRUE);
	}
	sqlite3_reset(stmt_menu_tree);
	
	// 构建树形结构
	xvalue arrResult = xvoCreateArray();
	
	// 添加主页菜单（与 pear.config.json 中的默认页面关联，避免重复标签页）
	xvalue tblHome = xvoCreateTable();
	xvoTableSetText(tblHome, "id", 2, "home", 0, FALSE);
	xvoTableSetText(tblHome, "title", 5, "主页", 0, FALSE);
	xvoTableSetText(tblHome, "icon", 4, "layui-icon layui-icon-home", 0, FALSE);
	xvoTableSetInt(tblHome, "type", 4, 1);
	xvoTableSetText(tblHome, "openType", 8, "_iframe", 0, FALSE);
	xvoTableSetText(tblHome, "href", 4, "/admin/view/home", 0, FALSE);
	xvoArrayAppendValue(arrResult, tblHome, TRUE);
	
	Menu_BuildTree_Recursive(arrResult, arrAll, 0);
	
	xvoUnref(arrAll);
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


