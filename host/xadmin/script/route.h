


// 初始化 HTTP 路由表
void RouteHTTP_Init()
{
	printf("        RouteHTTP_Init \n");
	// 创建 HTTP 全局静态路由表
	G_StaticRouteTableHTTP = xrtDictCreate(sizeof(RouteInfo));
	
	
	
	// ==================== 后台路由 ====================
	
	// 添加 HTTP 静态路由 - Index
	AddStaticRouteHTTP("/admin",									Request_Index);
	
	// 添加 HTTP 静态路由 - Login
	AddStaticRouteHTTP("/admin/login",								Request_Login);
	AddStaticRouteHTTP("/admin/logout",								Request_Logout);
	
	// 添加 HTTP 静态路由 - Menu
	AddStaticRouteHTTP("/admin/menu",								Request_Menu);
	
	// 添加 HTTP 静态路由 - Home
	AddStaticRouteHTTP("/admin/view/home",							Request_View_Home);
	
	// 添加 HTTP 静态路由 - Logs
	AddStaticRouteHTTP("/admin/view/logs",							Request_View_Logs);
	AddStaticRouteHTTP("/admin/logs",								Request_Logs);
	AddStaticRouteHTTP("/admin/logs/clear",							Request_Logs_Clear);
	
	// 添加 HTTP 静态路由 - Auth - User
	AddStaticRouteHTTP("/admin/auth/user",							Request_Auth_User);
	AddStaticRouteHTTP("/admin/auth/user/repwd",					Request_Auth_User_Repwd);
	AddStaticRouteHTTP("/admin/view/auth/user",						Request_View_Auth_User);
	AddStaticRouteHTTP("/admin/view/auth/user/add",					Request_View_Auth_User_Add);
	AddStaticRouteHTTP("/admin/view/auth/user/edit",				Request_View_Auth_User_Edit);
	
	// 添加 HTTP 静态路由 - Auth - Role
	AddStaticRouteHTTP("/admin/auth/role",							Request_Auth_Role);
	AddStaticRouteHTTP("/admin/view/auth/role",						Request_View_Auth_Role);
	AddStaticRouteHTTP("/admin/view/auth/role/add",					Request_View_Auth_Role_Add);
	AddStaticRouteHTTP("/admin/view/auth/role/edit",				Request_View_Auth_Role_Edit);
	
	// 添加 HTTP 静态路由 - Auth - Group
	AddStaticRouteHTTP("/admin/auth/group",							Request_Auth_Group);
	AddStaticRouteHTTP("/admin/view/auth/group",					Request_View_Auth_Group);
	AddStaticRouteHTTP("/admin/view/auth/group/add",				Request_View_Auth_Group_Add);
	AddStaticRouteHTTP("/admin/view/auth/group/edit",				Request_View_Auth_Group_Edit);
	
	// 添加 HTTP 静态路由 - Auth - Auth
	AddStaticRouteHTTP("/admin/auth/auth",							Request_Auth_Auth);
	AddStaticRouteHTTP("/admin/view/auth/auth",						Request_View_Auth_Auth);
	AddStaticRouteHTTP("/admin/view/auth/auth/add",					Request_View_Auth_Auth_Add);
	AddStaticRouteHTTP("/admin/view/auth/auth/edit",				Request_View_Auth_Auth_Edit);
	
	// 添加 HTTP 静态路由 - Auth - URIs
	AddStaticRouteHTTP("/admin/auth/uris",							Request_Auth_URIs);
	AddStaticRouteHTTP("/admin/view/auth/uris",						Request_View_Auth_URIs);
	AddStaticRouteHTTP("/admin/view/auth/uris/edit",				Request_View_Auth_URIs_Edit);
	
	// 添加 HTTP 静态路由 - Option
	AddStaticRouteHTTP("/admin/option",								Request_Option);
	AddStaticRouteHTTP("/admin/view/option",						Request_View_Option);
	
	// 添加 HTTP 静态路由 - Option - Menu
	AddStaticRouteHTTP("/admin/option/menu",						Request_Option_Menu);
	AddStaticRouteHTTP("/admin/view/option/menu",					Request_View_Option_Menu);
	AddStaticRouteHTTP("/admin/view/option/menu/add",				Request_View_Option_Menu_Add);
	AddStaticRouteHTTP("/admin/view/option/menu/add/category",		Request_View_Option_Menu_Add_Category);
	AddStaticRouteHTTP("/admin/view/option/menu/edit",				Request_View_Option_Menu_Edit);
	
	// 添加 HTTP 静态路由 - Trace (调试接口)
	AddStaticRouteHTTP("/admin/trace",								Request_Trace_Overview);
	AddStaticRouteHTTP("/admin/trace/session",						Request_Trace_Session);
	AddStaticRouteHTTP("/admin/trace/option",						Request_Trace_Option);
	AddStaticRouteHTTP("/admin/trace/auth",							Request_Trace_Auth);
	AddStaticRouteHTTP("/admin/trace/route",						Request_Trace_Route);
	
	// 添加 HTTP 静态路由 - Model (模型管理)
	AddStaticRouteHTTP("/admin/model/list",							Request_Model_List);
	AddStaticRouteHTTP("/admin/model/get",							Request_Model_Get);
	AddStaticRouteHTTP("/admin/model/add",							Request_Model_Add);
	AddStaticRouteHTTP("/admin/model/save",							Request_Model_Save);
	AddStaticRouteHTTP("/admin/model/delete",						Request_Model_Delete);
	AddStaticRouteHTTP("/admin/model/fields",						Request_Model_Fields);
	AddStaticRouteHTTP("/admin/model/fields/save",					Request_Model_Fields_Save);
	AddStaticRouteHTTP("/admin/model/compile",						Request_Model_Compile);
	AddStaticRouteHTTP("/admin/model/enable",						Request_Model_Enable);
	AddStaticRouteHTTP("/admin/model/disable",						Request_Model_Disable);
	AddStaticRouteHTTP("/admin/view/model",							Request_View_Model_List);
	AddStaticRouteHTTP("/admin/view/model/add",						Request_View_Model_Add);
	AddStaticRouteHTTP("/admin/view/model/edit",					Request_View_Model_Edit);
	AddStaticRouteHTTP("/admin/view/model/fields",					Request_View_Model_Fields);
	
	// ==================== 后台管理前台用户路由 ====================
	
	// 添加 HTTP 静态路由 - 后台管理前台用户 (Member)
	AddStaticRouteHTTP("/admin/member/user",						Request_Member_User);
	AddStaticRouteHTTP("/admin/member/user/repwd",					Request_Member_User_Repwd);
	AddStaticRouteHTTP("/admin/member/user/balance",				Request_Member_User_Balance);
	AddStaticRouteHTTP("/admin/view/member/user",					Request_View_Member_User);
	AddStaticRouteHTTP("/admin/view/member/user/add",				Request_View_Member_User_Add);
	AddStaticRouteHTTP("/admin/view/member/user/edit",				Request_View_Member_User_Edit);
	
	// 添加 HTTP 静态路由 - 后台管理前台用户组 (Member Group)
	AddStaticRouteHTTP("/admin/member/group",						Request_Member_Group);
	AddStaticRouteHTTP("/admin/view/member/group",					Request_View_Member_Group);
	AddStaticRouteHTTP("/admin/view/member/group/add",				Request_View_Member_Group_Add);
	AddStaticRouteHTTP("/admin/view/member/group/edit",				Request_View_Member_Group_Edit);
	
	// 添加 HTTP 静态路由 - 后台管理前台权限分类 (Member AuthGroup)
	AddStaticRouteHTTP("/admin/member/authgroup",					Request_Member_AuthGroup);
	AddStaticRouteHTTP("/admin/view/member/authgroup",				Request_View_Member_AuthGroup);
	AddStaticRouteHTTP("/admin/view/member/authgroup/add",			Request_View_Member_AuthGroup_Add);
	AddStaticRouteHTTP("/admin/view/member/authgroup/edit",			Request_View_Member_AuthGroup_Edit);
	
	// 添加 HTTP 静态路由 - 后台管理前台权限分组 (Member Auth)
	AddStaticRouteHTTP("/admin/member/auth",						Request_Member_Auth);
	AddStaticRouteHTTP("/admin/view/member/auth",					Request_View_Member_Auth);
	AddStaticRouteHTTP("/admin/view/member/auth/add",				Request_View_Member_Auth_Add);
	AddStaticRouteHTTP("/admin/view/member/auth/edit",				Request_View_Member_Auth_Edit);
	
	// ==================== 前台API路由 ====================
	
	// 添加 HTTP 静态路由 - 前台 API v1
	AddStaticRouteHTTP("/api/v1/login",								API_Login);
	AddStaticRouteHTTP("/api/v1/register",							API_Register);
	AddStaticRouteHTTP("/api/v1/logout",							API_Logout);
	AddStaticRouteHTTP("/api/v1/profile",							API_Profile);
	AddStaticRouteHTTP("/api/v1/profile/password",					API_Password);
	AddStaticRouteHTTP("/api/v1/balance",							API_Balance);
	AddStaticRouteHTTP("/api/v1/balance/log",						API_BalanceLog);
	
}





// 卸载 HTTP 路由表
void RouteHTTP_Unit()
{
	printf("        RouteHTTP_Unit \n");
	xrtDictDestroy(G_StaticRouteTableHTTP);
}


