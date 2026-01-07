


// 初始化 HTTP 路由表
void RouteHTTP_Init()
{
	printf("        RouteHTTP_Init \n");
	// 创建 HTTP 全局静态路由表
	G_StaticRouteTableHTTP = xrtDictCreate(sizeof(RouteInfo));
	
	
	
	//					路由 URI									执行函数									需要鉴权		后台接口		记录日志		保持活跃
	
	// ==================== 后台路由 (bAdmin = TRUE) ====================
	
	// 添加 HTTP 静态路由 - Index
	AddStaticRouteHTTP("/admin",								Request_Index							, TRUE ,	TRUE ,		FALSE,		TRUE);
	
	// 添加 HTTP 静态路由 - Login
	AddStaticRouteHTTP("/admin/login",							Request_Login							, FALSE,	TRUE ,		TRUE ,		FALSE);
	AddStaticRouteHTTP("/admin/logout",							Request_Logout							, TRUE ,	TRUE ,		FALSE,		FALSE);
	
	// 添加 HTTP 静态路由 - Menu
	AddStaticRouteHTTP("/admin/menu",							Request_Menu							, TRUE ,	TRUE ,		FALSE,		FALSE);
	
	// 添加 HTTP 静态路由 - Home
	AddStaticRouteHTTP("/admin/view/home",						Request_View_Home						, TRUE ,	TRUE ,		TRUE ,		TRUE);
	
	// 添加 HTTP 静态路由 - Logs
	AddStaticRouteHTTP("/admin/view/logs",						Request_View_Logs						, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/logs",							Request_Logs							, TRUE ,	TRUE ,		FALSE,		TRUE);
	AddStaticRouteHTTP("/admin/logs/clear",						Request_Logs_Clear						, TRUE ,	TRUE ,		TRUE ,		TRUE);
	
	// 添加 HTTP 静态路由 - Auth - User
	AddStaticRouteHTTP("/admin/auth/user",						Request_Auth_User						, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/auth/user/repwd",				Request_Auth_User_Repwd					, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/auth/user",					Request_View_Auth_User					, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/auth/user/add",				Request_View_Auth_User_Add				, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/auth/user/edit",			Request_View_Auth_User_Edit				, TRUE ,	TRUE ,		TRUE ,		TRUE);
	
	// 添加 HTTP 静态路由 - Auth - Role
	AddStaticRouteHTTP("/admin/auth/role",						Request_Auth_Role						, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/auth/role",					Request_View_Auth_Role					, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/auth/role/add",				Request_View_Auth_Role_Add				, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/auth/role/edit",			Request_View_Auth_Role_Edit				, TRUE ,	TRUE ,		TRUE ,		TRUE);
	
	// 添加 HTTP 静态路由 - Auth - Group
	AddStaticRouteHTTP("/admin/auth/group",						Request_Auth_Group						, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/auth/group",				Request_View_Auth_Group					, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/auth/group/add",			Request_View_Auth_Group_Add				, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/auth/group/edit",			Request_View_Auth_Group_Edit			, TRUE ,	TRUE ,		TRUE ,		TRUE);
	
	// 添加 HTTP 静态路由 - Auth - Auth
	AddStaticRouteHTTP("/admin/auth/auth",						Request_Auth_Auth						, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/auth/auth",					Request_View_Auth_Auth					, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/auth/auth/add",				Request_View_Auth_Auth_Add				, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/auth/auth/edit",			Request_View_Auth_Auth_Edit				, TRUE ,	TRUE ,		TRUE ,		TRUE);
	
	// 添加 HTTP 静态路由 - Auth - URIs
	AddStaticRouteHTTP("/admin/auth/uris",						Request_Auth_URIs						, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/auth/uris",					Request_View_Auth_URIs					, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/auth/uris/edit",			Request_View_Auth_URIs_Edit				, TRUE ,	TRUE ,		TRUE ,		TRUE);
	
	// 添加 HTTP 静态路由 - Option
	AddStaticRouteHTTP("/admin/option",							Request_Option							, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/option",					Request_View_Option						, TRUE ,	TRUE ,		TRUE ,		TRUE);
	
	// 添加 HTTP 静态路由 - Option - Menu
	AddStaticRouteHTTP("/admin/option/menu",					Request_Option_Menu						, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/option/menu",				Request_View_Option_Menu				, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/option/menu/add",			Request_View_Option_Menu_Add			, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/option/menu/add/category",	Request_View_Option_Menu_Add_Category	, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/option/menu/edit",			Request_View_Option_Menu_Edit			, TRUE ,	TRUE ,		TRUE ,		TRUE);
	
	// 添加 HTTP 静态路由 - Trace (调试接口)
	AddStaticRouteHTTP("/admin/trace",							Request_Trace_Overview					, TRUE ,	TRUE ,		FALSE,		TRUE);
	AddStaticRouteHTTP("/admin/trace/session",					Request_Trace_Session					, TRUE ,	TRUE ,		FALSE,		TRUE);
	AddStaticRouteHTTP("/admin/trace/option",					Request_Trace_Option					, TRUE ,	TRUE ,		FALSE,		TRUE);
	AddStaticRouteHTTP("/admin/trace/auth",						Request_Trace_Auth						, TRUE ,	TRUE ,		FALSE,		TRUE);
	AddStaticRouteHTTP("/admin/trace/route",					Request_Trace_Route						, TRUE ,	TRUE ,		FALSE,		TRUE);
	
	// ==================== 前台路由 (bAdmin = FALSE) ====================
	
	// 添加 HTTP 静态路由 - 后台管理前台用户 (Member)
	AddStaticRouteHTTP("/admin/member/user",					Request_Member_User						, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/member/user/repwd",				Request_Member_User_Repwd				, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/member/user/balance",			Request_Member_User_Balance				, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/member/user",				Request_View_Member_User				, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/member/user/add",			Request_View_Member_User_Add			, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/member/user/edit",			Request_View_Member_User_Edit			, TRUE ,	TRUE ,		TRUE ,		TRUE);
	
	// 添加 HTTP 静态路由 - 后台管理前台用户组 (Member Group)
	AddStaticRouteHTTP("/admin/member/group",					Request_Member_Group					, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/member/group",				Request_View_Member_Group				, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/member/group/add",			Request_View_Member_Group_Add			, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/member/group/edit",			Request_View_Member_Group_Edit			, TRUE ,	TRUE ,		TRUE ,		TRUE);
	
	// 添加 HTTP 静态路由 - 后台管理前台权限分类 (Member AuthGroup)
	AddStaticRouteHTTP("/admin/member/authgroup",				Request_Member_AuthGroup				, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/member/authgroup",			Request_View_Member_AuthGroup			, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/member/authgroup/add",		Request_View_Member_AuthGroup_Add		, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/member/authgroup/edit",		Request_View_Member_AuthGroup_Edit		, TRUE ,	TRUE ,		TRUE ,		TRUE);
	
	// 添加 HTTP 静态路由 - 后台管理前台权限分组 (Member Auth)
	AddStaticRouteHTTP("/admin/member/auth",					Request_Member_Auth						, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/member/auth",				Request_View_Member_Auth				, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/member/auth/add",			Request_View_Member_Auth_Add			, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/member/auth/edit",			Request_View_Member_Auth_Edit			, TRUE ,	TRUE ,		TRUE ,		TRUE);
	
	// 添加 HTTP 静态路由 - 后台管理前台URI权限 (Member Uris)
	AddStaticRouteHTTP("/admin/member/uris",					Request_Member_Uris						, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/member/uris",				Request_View_Member_Uris				, TRUE ,	TRUE ,		TRUE ,		TRUE);
	AddStaticRouteHTTP("/admin/view/member/uris/edit",			Request_View_Member_Uris_Edit			, TRUE ,	TRUE ,		TRUE ,		TRUE);
	
	// ==================== 前台路由 (bAdmin = FALSE) ====================
	
	// 添加 HTTP 静态路由 - 前台 API v1
	AddStaticRouteHTTP("/api/v1/login",							API_Login								, FALSE,	FALSE,		FALSE,		FALSE);
	AddStaticRouteHTTP("/api/v1/register",						API_Register							, FALSE,	FALSE,		FALSE,		FALSE);
	AddStaticRouteHTTP("/api/v1/logout",						API_Logout								, TRUE ,	FALSE,		FALSE,		FALSE);
	AddStaticRouteHTTP("/api/v1/profile",						API_Profile								, TRUE ,	FALSE,		FALSE,		TRUE);
	AddStaticRouteHTTP("/api/v1/profile/password",				API_Password							, TRUE ,	FALSE,		FALSE,		FALSE);
	AddStaticRouteHTTP("/api/v1/balance",						API_Balance								, TRUE ,	FALSE,		FALSE,		TRUE);
	AddStaticRouteHTTP("/api/v1/balance/log",					API_BalanceLog							, TRUE ,	FALSE,		FALSE,		TRUE);
	
}





// 卸载 HTTP 路由表
void RouteHTTP_Unit()
{
	printf("        RouteHTTP_Unit \n");
	xrtDictDestroy(G_StaticRouteTableHTTP);
}


