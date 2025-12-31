


// 初始化数据库模块
void DB_Init()
{
	printf("        DB_Init \n");
	str sFile = xrtPathJoin(2, DBPath, "main.db");
	G_DB = xdoConnectSQLite(sFile);
	xrtFree(sFile);
	if ( G_DB == NULL ) {
		printf("!!! ERROR !!! ServiceInit - xdoConnectSQLite error.\n");
		exit(1);
	}
}



// 卸载数据库模块
void DB_Unit()
{
	printf("        DB_Unit \n");
	if ( G_DB ) {
		xdoDisconnect(G_DB);
	}
}


