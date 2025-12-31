


// 编译好的 SQL 语句
sqlite3_stmt* stmt_sers_menu = NULL;



// 服务模块初始化
void Admin_Init()
{
	printf("        Admin_Init \n");
	
	// 编译菜单用的SQL语句
	/*
	int iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT id, name FROM services WHERE isDelete = 0 ORDER BY id ASC;", -1, SQL_PREPARE_DEFAULT, &stmt_sers_menu, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Services_Init [stmt_sers_menu] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	*/
	
}



// 服务模块卸载
void Admin_Unit()
{
	printf("        Admin_Unit \n");
	
	// 释放预编译 SQL 语句
	/*
	sqlite3_finalize(stmt_sers_menu);
	*/
}


