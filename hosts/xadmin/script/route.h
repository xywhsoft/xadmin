


// 初始�?HTTP 路由�?
void RouteHTTP_Init()
{
	printf("        RouteHTTP_Init \n");
	// 创建 HTTP 全局静态路由表
	G_StaticRouteTableHTTP = xrtDictCreate(sizeof(RouteInfo), XRT_OBJMODE_SHARED);
	
	
	
	// ==================== 后台路由 ====================
	
	// 添加 HTTP 静态路由 - Index
	AddStaticRouteHTTP("/",										Request_Index);
	AddStaticRouteHTTP("/admin",									Request_Index);
	
	// 添加 HTTP 静态路由 - Login
	AddStaticRouteHTTP("/admin/login",								Request_Login);
	AddStaticRouteHTTP("/admin/logout",								Request_Logout);
	AddStaticRouteHTTP("/brand/admin",								Request_Admin_Brand);
	RouteInfo* pBrandRoute = xrtDictGet(G_StaticRouteTableHTTP, "/brand/admin", 12);
	if ( pBrandRoute ) {
		pBrandRoute->bAuth = FALSE;
		pBrandRoute->bAdmin = FALSE;
	}
	
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
	AddStaticRouteHTTP("/admin/option/tool/admin-entry",			Request_Option_AdminEntryGenerate);
	AddStaticRouteHTTP("/admin/view/option",						Request_View_Option);
	AddStaticRouteHTTP("/admin/option/files",						Request_Option_Files);
	AddStaticRouteHTTP("/admin/option/file",						Request_Option_File);
	AddStaticRouteHTTP("/admin/option/file/menu",					Request_Option_File_Menu);
	AddStaticRouteHTTP("/admin/view/option/files",					Request_View_Option_Files);
	AddStaticRouteHTTP("/admin/view/option/file",					Request_View_Option_File);
	
	// 添加 HTTP 静态路由 - Option - Menu
	AddStaticRouteHTTP("/admin/option/menu",						Request_Option_Menu);
	AddStaticRouteHTTP("/admin/view/option/menu",					Request_View_Option_Menu);
	AddStaticRouteHTTP("/admin/view/option/menu/add",				Request_View_Option_Menu_Add);
	AddStaticRouteHTTP("/admin/view/option/menu/add/category",		Request_View_Option_Menu_Add_Category);
	AddStaticRouteHTTP("/admin/view/option/menu/edit",				Request_View_Option_Menu_Edit);

	// 添加 HTTP 静态路由 - Scheduler
	AddStaticRouteHTTP("/admin/view/sched",							Request_View_Sched);
	AddStaticRouteHTTP("/admin/view/sched/edit",					Request_View_Sched_Edit);
	AddStaticRouteHTTP("/admin/view/sched/log",						Request_View_Sched_Log);
	AddStaticRouteHTTP("/admin/view/sched/dashboard",				Request_View_Sched_Dashboard);
	AddStaticRouteHTTP("/admin/sched/tasks",						Request_Sched_Tasks);
	AddStaticRouteHTTP("/admin/sched/task",							Request_Sched_Task);
	AddStaticRouteHTTP("/admin/sched/task/enable",					Request_Sched_Task_Enable);
	AddStaticRouteHTTP("/admin/sched/task/batch_enable",			Request_Sched_Task_Batch_Enable);
	AddStaticRouteHTTP("/admin/sched/task/batch_delete",			Request_Sched_Task_Batch_Delete);
	AddStaticRouteHTTP("/admin/sched/task/batch_run",				Request_Sched_Task_Batch_Run);
	AddStaticRouteHTTP("/admin/sched/task/run",						Request_Sched_Task_Run);
	AddStaticRouteHTTP("/admin/sched/task/copy",					Request_Sched_Task_Copy);
	AddStaticRouteHTTP("/admin/sched/task/example",					Request_Sched_Task_Example);
	AddStaticRouteHTTP("/admin/sched/dashboard",					Request_Sched_Dashboard);
	AddStaticRouteHTTP("/admin/sched/export",						Request_Sched_Export);
	AddStaticRouteHTTP("/admin/sched/export_selected",				Request_Sched_Export_Selected);
	AddStaticRouteHTTP("/admin/sched/import",						Request_Sched_Import);
	AddStaticRouteHTTP("/admin/sched/preview",						Request_Sched_Preview);
	AddStaticRouteHTTP("/admin/sched/logs",							Request_Sched_Logs);
	AddStaticRouteHTTP("/admin/sched/logs/export",					Request_Sched_Logs_Export);
	AddStaticRouteHTTP("/admin/sched/logs/clear",					Request_Sched_Logs_Clear);
	
	// 添加 HTTP 静态路由 - Trace (调试接口)
	AddStaticRouteHTTP("/admin/trace",								Request_Trace_Overview);
	AddStaticRouteHTTP("/admin/trace/session",						Request_Trace_Session);
	AddStaticRouteHTTP("/admin/trace/option",						Request_Trace_Option);
	AddStaticRouteHTTP("/admin/trace/auth",							Request_Trace_Auth);
	AddStaticRouteHTTP("/admin/trace/route",						Request_Trace_Route);
	
	// ==================== 后台管理前台用户路由 ====================
	
	// 添加 HTTP 静态路由 - 后台管理前台用户 (Member)
	AddStaticRouteHTTP("/admin/member/user",						Request_Member_User);
	AddStaticRouteHTTP("/admin/member/user/repwd",					Request_Member_User_Repwd);
	AddStaticRouteHTTP("/admin/member/user/balance",				Request_Member_User_Balance);
	AddStaticRouteHTTP("/admin/view/member/user",					Request_View_Member_User);
	AddStaticRouteHTTP("/admin/view/member/user/add",				Request_View_Member_User_Add);
	AddStaticRouteHTTP("/admin/view/member/user/edit",				Request_View_Member_User_Edit);
	AddStaticRouteHTTP("/admin/view/member/user/balance",			Request_View_Member_User_Balance);
	
	// 添加 HTTP 静态路由 - 后台管理前台用户 (Member Group)
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
	
	// ==================== 附件管理路由 ====================
	
	// 附件访问（通过查询参数 ?xid=xxx 访问）
	AddStaticRouteHTTP("/attachment",								Request_Attachment_Access);
	
	// 后台附件管理 API
	AddStaticRouteHTTP("/admin/attachment/upload",					Request_Attachment_Upload);
	AddStaticRouteHTTP("/admin/attachment/list",					Request_Attachment_List);
	AddStaticRouteHTTP("/admin/attachment/get",						Request_Attachment_Get);
	AddStaticRouteHTTP("/admin/attachment/save",					Request_Attachment_Save);
	AddStaticRouteHTTP("/admin/attachment/delete",					Request_Attachment_Delete);
	AddStaticRouteHTTP("/admin/attachment/stats",					Request_Attachment_Stats);
	
	// 后台附件页面
	AddStaticRouteHTTP("/admin/view/attachment",					Request_View_Attachment_List);
	AddStaticRouteHTTP("/admin/view/attachment/edit",				Request_View_Attachment_Edit);
	AddStaticRouteHTTP("/admin/view/attachment/upload",			Request_View_Attachment_Upload);
	AddStaticRouteHTTP("/admin/view/attachment/stats",				Request_View_Attachment_Stats);
	
	// ==================== 前台API路由 ====================
	
	// 添加 HTTP 静态路由 - 前台 API v1
	AddStaticRouteHTTP("/api/v1/login",								API_Login);
	AddStaticRouteHTTP("/api/v1/register",							API_Register);
	AddStaticRouteHTTP("/api/v1/logout",							API_Logout);
	AddStaticRouteHTTP("/api/v1/profile",							API_Profile);
	AddStaticRouteHTTP("/api/v1/profile/password",					API_Password);
	AddStaticRouteHTTP("/api/v1/balance",							API_Balance);
	AddStaticRouteHTTP("/api/v1/balance/log",						API_BalanceLog);
	
	// 前台附件 API
	AddStaticRouteHTTP("/api/v1/attachment/upload",					Request_Api_Attachment_Upload);
	AddStaticRouteHTTP("/api/v1/attachment/purchase",				Request_Api_Attachment_Purchase);
	AddStaticRouteHTTP("/api/v1/attachment/my",						Request_Api_Attachment_MyList);
	AddStaticRouteHTTP("/api/v1/attachment/purchased",				Request_Api_Attachment_Purchased);
	
	// ==================== 插件管理路由 ====================
	
	// 插件管理后台页面
	AddStaticRouteHTTP("/admin/view/plugin",						Request_View_Plugin_List);
	AddStaticRouteHTTP("/admin/view/plugin/installed",				Request_View_Plugin_List);
	AddStaticRouteHTTP("/admin/view/plugin/store",					Request_View_Plugin_Store);
	
	// 插件管理 API
	AddStaticRouteHTTP("/admin/plugin/list",						Request_Plugin_List);
	AddStaticRouteHTTP("/admin/plugin/get",							Request_Plugin_Get);
	AddStaticRouteHTTP("/admin/plugin/enable",						Request_Plugin_Enable);
	AddStaticRouteHTTP("/admin/plugin/disable",						Request_Plugin_Disable);
	AddStaticRouteHTTP("/admin/plugin/reload",						Request_Plugin_Reload);
	AddStaticRouteHTTP("/admin/plugin/settings",					Request_Plugin_Settings);

}





// 卸载 HTTP 路由
void RouteHTTP_Unit()
{
	printf("        RouteHTTP_Unit \n");
	xrtDictDestroy(G_StaticRouteTableHTTP);
}


