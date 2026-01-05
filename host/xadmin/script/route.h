



// 初始化 HTTP 路由表
void RouteHTTP_Init()
{
	printf("        RouteHTTP_Init \n");
	// 创建 HTTP 全局静态路由表
	G_StaticRouteTableHTTP = xrtDictCreate(sizeof(RouteInfo));
	
	
	
	//					路由 URI								执行函数								记录日志		需要鉴权
	
	// 添加 HTTP 静态路由 - Index
	AddStaticRouteHTTP("/",									Request_Index						, FALSE,	TRUE );
	
	// 添加 HTTP 静态路由 - Login
	AddStaticRouteHTTP("/login",							Request_Login						, TRUE ,	FALSE);
	AddStaticRouteHTTP("/logout",							Request_Logout						, FALSE,	TRUE );
	
	// 添加 HTTP 静态路由 - Menu
	AddStaticRouteHTTP("/menu",								Request_Menu						, FALSE,	TRUE );
	
	// 添加 HTTP 静态路由 - Home
	AddStaticRouteHTTP("/view/home",						Request_View_Home					, TRUE ,	TRUE );
	
	// 添加 HTTP 静态路由 - Logs
	AddStaticRouteHTTP("/view/logs",						Request_View_Logs					, TRUE ,	TRUE );
	AddStaticRouteHTTP("/logs",								Request_Logs						, FALSE,	TRUE );
	AddStaticRouteHTTP("/logs/clear",						Request_Logs_Clear					, TRUE ,	TRUE );
	
	// 添加 HTTP 静态路由 - Auth - User
	AddStaticRouteHTTP("/auth/user",						Request_Auth_User					, TRUE ,	TRUE );
	AddStaticRouteHTTP("/auth/user/repwd",					Request_Auth_User_Repwd				, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/user",					Request_View_Auth_User				, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/user/add",				Request_View_Auth_User_Add			, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/user/edit",				Request_View_Auth_User_Edit			, TRUE ,	TRUE );
	
	// 添加 HTTP 静态路由 - Auth - Role
	AddStaticRouteHTTP("/auth/role",						Request_Auth_Role					, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/role",					Request_View_Auth_Role				, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/role/add",				Request_View_Auth_Role_Add			, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/role/edit",				Request_View_Auth_Role_Edit			, TRUE ,	TRUE );
	
	// 添加 HTTP 静态路由 - Auth - Group
	AddStaticRouteHTTP("/auth/group",						Request_Auth_Group					, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/group",					Request_View_Auth_Group				, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/group/add",				Request_View_Auth_Group_Add			, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/group/edit",				Request_View_Auth_Group_Edit		, TRUE ,	TRUE );
	
	// 添加 HTTP 静态路由 - Auth - Auth
	AddStaticRouteHTTP("/auth/auth",						Request_Auth_Auth					, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/auth",					Request_View_Auth_Auth				, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/auth/add",				Request_View_Auth_Auth_Add			, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/auth/edit",				Request_View_Auth_Auth_Edit			, TRUE ,	TRUE );
	
	// 添加 HTTP 静态路由 - Auth - URIs
	AddStaticRouteHTTP("/auth/uris",						Request_Auth_URIs					, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/uris",					Request_View_Auth_URIs				, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/uris/edit",				Request_View_Auth_URIs_Edit			, TRUE ,	TRUE );
	
	// 添加 HTTP 静态路由 - Option
	AddStaticRouteHTTP("/option",							Request_Option						, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/option",						Request_View_Option					, TRUE ,	TRUE );
	
	// 添加 HTTP 静态路由 - Auth - Menu
	AddStaticRouteHTTP("/auth/menu",						Request_Auth_Menu					, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/menu",					Request_View_Auth_Menu				, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/menu/add",				Request_View_Auth_Menu_Add			, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/menu/add/category",		Request_View_Auth_Menu_Add_Category	, TRUE ,	TRUE );
	AddStaticRouteHTTP("/view/auth/menu/edit",				Request_View_Auth_Menu_Edit			, TRUE ,	TRUE );
	
}





// 卸载 HTTP 路由表
void RouteHTTP_Unit()
{
	printf("        RouteHTTP_Unit \n");
	xrtDictDestroy(G_StaticRouteTableHTTP);
}


