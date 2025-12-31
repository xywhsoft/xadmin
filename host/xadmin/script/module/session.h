


// 初始化 Session 模块
void Session_Init()
{
	printf("        Session_Init \n");
	G_Session = xvoCreateTable();
	if ( G_Session == NULL ) {
		printf("!!! ERROR !!! Create sessions table failed !\n");
		exit(1);
	}
}



// 卸载 Session 模块
void Session_Unit()
{
	printf("        Session_Unit \n");
	xvoUnref(G_Session);
}


