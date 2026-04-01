


// XS 基础服务库
#include <xs_vnext_full.h>
#if defined(_WIN32) || defined(_WIN64)
	// windows 方案
#else
	// 其他平台方案
	#include <pthread.h>
	#include <sys/utsname.h>
#endif



// 全局定义
#include "module/define.h"

// 安全防护模块
#include "module/guard.h"

// 独立页面 API
#include "module/page.h"

// 模板相关功能
#include "module/template.h"

// 安装相关功能
#include "module/install.h"

// Session 相关功能
#include "module/session.h"

// 数据库相关功能
#include "module/db.h"

// 权限管理模块
#include "module/auth.h"

// 日志记录模块
#include "module/logs.h"

// 后台功能模块
#include "module/admin.h"

// 配置管理模块
#include "module/option.h"

// 菜单管理模块
#include "module/menu.h"

// 前台用户模块
#include "module/member.h"

// 前台权限缓存模块
#include "module/member_auth.h"

// 模型字段类型定义
#include "module/model_field.h"

// 模型管理器
#include "module/model_mgr.h"

// 附件管理模块
#include "module/attachment.h"

// 插件上下文定义
#include "module/plugin_ctx.h"

// 插件页面
#include "module/plugin_page.h"

// 插件管理器
#include "module/plugin_mgr.h"



// 路由调用 - HTTP
#include "route_http/index.h"
#include "route_http/login.h"
#include "route_http/logs.h"
#include "route_http/auth.h"
#include "route_http/option.h"
#include "route_http/menu.h"
#include "route_http/trace.h"
#include "route_http/api.h"
#include "route_http/member.h"
#include "route_http/model.h"
#include "route_http/attachment.h"
#include "route_http/attachment_api.h"
#include "route_http/plugin.h"



// 全局静态路由表
#include "route.h"

// HTTP 协议处理
#include "module/http.h"





// 服务初始化
void ServiceInit(XS_ServerObject objServer, XS_HostObject objHost)
{
	printf("[xadmin:init] Define_Init begin\n");
	fflush(stdout);
	Define_Init(objServer, objHost);
	printf("[xadmin:init] Define_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Guard_Init begin\n");
	fflush(stdout);
	Guard_Init();
	printf("[xadmin:init] Guard_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Template_Init begin\n");
	fflush(stdout);
	Template_Init();
	printf("[xadmin:init] Template_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] PluginTemplate_Init begin\n");
	fflush(stdout);
	PluginTemplate_Init();
	printf("[xadmin:init] PluginTemplate_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Install_Init begin\n");
	fflush(stdout);
	Install_Init();
	printf("[xadmin:init] Install_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Session_Init begin\n");
	fflush(stdout);
	Session_Init();
	printf("[xadmin:init] Session_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] DB_Init begin\n");
	fflush(stdout);
	DB_Init();
	printf("[xadmin:init] DB_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] RouteHTTP_Init begin\n");
	fflush(stdout);
	RouteHTTP_Init();
	printf("[xadmin:init] RouteHTTP_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Admin_Init begin\n");
	fflush(stdout);
	Admin_Init();
	printf("[xadmin:init] Admin_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Auth_Init begin\n");
	fflush(stdout);
	Auth_Init();
	printf("[xadmin:init] Auth_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Logs_Init begin\n");
	fflush(stdout);
	Logs_Init();
	printf("[xadmin:init] Logs_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Option_Init begin\n");
	fflush(stdout);
	Option_Init();
	printf("[xadmin:init] Option_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Menu_Init begin\n");
	fflush(stdout);
	Menu_Init();
	printf("[xadmin:init] Menu_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Member_Init begin\n");
	fflush(stdout);
	Member_Init();
	printf("[xadmin:init] Member_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] MemberAuth_Init begin\n");
	fflush(stdout);
	MemberAuth_Init();
	printf("[xadmin:init] MemberAuth_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] ModelMgr_Init begin\n");
	fflush(stdout);
	ModelMgr_Init();
	printf("[xadmin:init] ModelMgr_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Attachment_Init begin\n");
	fflush(stdout);
	Attachment_Init();
	printf("[xadmin:init] Attachment_Init done\n");
	fflush(stdout);

	printf("[xadmin:init] Auth_SyncURIS begin\n");
	fflush(stdout);
	Auth_SyncURIS();
	printf("[xadmin:init] Auth_SyncURIS done\n");
	fflush(stdout);

	printf("[xadmin:init] PluginMgr_Init begin\n");
	fflush(stdout);
	PluginMgr_Init();
	printf("[xadmin:init] PluginMgr_Init done\n");
	fflush(stdout);
}



// 服务卸载
void ServiceUnit(XS_ServerObject objServer, XS_HostObject objHost)
{

	// 卸载插件模板
	PluginTemplate_Unit();

	// 卸载插件管理器
	PluginMgr_Unit();
	
	// 卸载附件管理模块
	Attachment_Unit();
	
	// 卸载模型管理器
	ModelMgr_Unit();
	
	// 卸载前台权限缓存模块
	MemberAuth_Unit();
	
	// 卸载前台权限缓存模块
	Member_Unit();
	
	// 卸载菜单管理模块
	Menu_Unit();
	
	// 卸载配置管理模块
	Option_Unit();
	
	// 卸载后台功能模块
	Admin_Unit();
	
	// 卸载日志记录模块
	Logs_Unit();
	
	// 卸载权限管理模块
	Auth_Unit();
	
	// 卸载 HTTP 路由表
	RouteHTTP_Unit();
	
	// 释放数据库
	DB_Unit();
	
	// 卸载 Session 模块
	Session_Unit();
	
	// 卸载安装模块
	Install_Unit();
	
	// 卸载模板模块
	Template_Unit();
	
	// 卸载防护模块
	Guard_Unit();
	
	// 卸载全局数据
	Define_Unit();
	
}