

// init db
void DB_Init()
{
	int iRet;
	str sFile;

	printf("        DB_Init \n");
	sFile = xrtPathJoin(2, DBPath, "main.db");
	iRet = sqlite3_open(sFile, &G_DB);
	xrtFree(sFile);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! ServiceInit - sqlite3_open error.\n");
		if ( G_DB ) {
			printf("%s\n", sqlite3_errmsg(G_DB));
			sqlite3_close(G_DB);
			G_DB = NULL;
		}
		exit(1);
	}
}



// free db
void DB_Unit()
{
	printf("        DB_Unit \n");
	if ( G_DB ) {
		sqlite3_close(G_DB);
		G_DB = NULL;
	}
}
