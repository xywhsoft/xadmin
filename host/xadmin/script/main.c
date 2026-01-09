


// XS 基础服务库
#include <xsbase.h>
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



// 全局静态路由表
#include "route.h"

// HTTP 协议处理
#include "module/http.h"





// 服务初始化
void ServiceInit(XS_ServerObject objServer, XS_HostObject objHost)
{
	
	// 初始化全局定义
	Define_Init(objServer, objHost);
	
	// 初始化安全防护模块
	Guard_Init();
	
	// 初始化模板渲染功能
	Template_Init();
	
	// 安装相关功能初始化
	Install_Init();
	
	// 创建全局 Session 表
	Session_Init();
	
	// 连接到主数据库
	DB_Init();
	
	// 初始化 HTTP 路由表
	RouteHTTP_Init();
	
	// 初始化后台功能模块
	Admin_Init();
	
	// 初始化权限管理模块
	Auth_Init();
	
	// 初始化日志记录模块
	Logs_Init();
	
	// 初始化配置管理模块
	Option_Init();
	
	// 初始化菜单管理模块
	Menu_Init();
	
	// 初始化前台用户模块（SQL预编译）
	Member_Init();
	
	// 初始化前台权限缓存模块
	MemberAuth_Init();
	
	// 初始化模型管理器
	ModelMgr_Init();
	
	// 初始化附件管理模块
	Attachment_Init();
	
	// 同步 URI 表（在所有路由注册完成后）
	Auth_SyncURIS();
	
}



// 服务卸载
void ServiceUnit(XS_ServerObject objServer, XS_HostObject objHost)
{
	
	// 卸载附件管理模块
	Attachment_Unit();
	
	// 卸载模型管理器
	ModelMgr_Unit();
	
	// 卸载前台权限缓存模块
	MemberAuth_Unit();
	
	// 卸载前台用户模块
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