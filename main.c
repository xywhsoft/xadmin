/* xAdmin / xs3 —— 应用入口。
 * dev/v1 是只读基线；原有业务处理代码放在 route_http，界面原样复用。
 * 目前接入：前后台身份、权限管理、菜单、日志、配置读取/编辑与设置文件管理，表单引擎，重载工具、调试跟踪与入口生成。
 * 完整迁移范围与未接入项见 docs/migration.md。
 */
#define XADMIN_WITH_SMTP 1
#include <xsbase.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "modules/value_util.h"
#include "modules/util.h"
#include "modules/multipart.h" /* http.h 的 multipart ABI 包装依赖，须在前 */
#include "modules/http_io.h"
#include "modules/http.h"
#include "modules/define.h"
#include "modules/router.h"
#include "modules/state.h"
#include "modules/resource.h"
#include "modules/database.h"
#include "modules/page.h"
#include "modules/template.h"
#include "modules/session.h"
#include "modules/guard.h"
#include "modules/auth.h"
#include "modules/member.h"
#include "modules/member_auth.h"
#include "modules/notify.h"
#include "modules/attachment.h"
#include "modules/sched.h"
#include "modules/standalone_page.h"
#if XADMIN_WITH_SMTP
#include "modules/mail.h"
#endif
#include "modules/menu.h"
#include "modules/logs.h"
#include "modules/option.h"
#include "modules/form.h"
#include "modules/plugin_host.h"
#include "modules/content.h"
#include "modules/content_generator.h"
#include "modules/install.h"
static bool G_SessionStarted, G_BusinessStarted; /* install.h 的向导流程与本段启动链共用 */
#include "route_http/index.h"
#include "route_http/login.h"
#include "route_http/auth.h"
#include "route_http/member.h"
#include "route_http/api.h"
#include "route_http/notify.h"
#include "route_http/attachment.h"
#include "route_http/attachment_api.h"
#include "route_http/sched.h"
#include "route_http/standalone_page.h"
#if XADMIN_WITH_SMTP
#include "route_http/mail.h"
#endif
#include "route_http/menu.h"
#include "route_http/logs.h"
#include "route_http/tool_reload.h"
#include "route_http/trace.h"
#include "route_http/option_tool.h"
#include "route_http/form.h"
#include "route_http/option.h"
#include "route_http/option_file.h"
#include "route_http/plugin.h"
#include "route_http/content.h"
#include "route_http/brand.h"
#include "route.h"
#include "modules/protocol.h"


/* 必需扩展库探测（xs 契约 API xsExtensionEnabled，注册名=构建参数名）：
 * 缺失即拒启，报错到日志——避免运行中期才在功能点炸出难定位的符号错误。 */
static bool XAdmin_RequireExtensions(void)
{
	static const char* required[] = {
		"sqlite",                    /* 主库 */
#if XADMIN_WITH_SMTP
		"xsmtp",                     /* 邮件发送/队列 */
#endif
		"md4c",                      /* 内容系统 markdown 渲染 */
	};
	size_t i;
	for (i = 0; i < sizeof(required) / sizeof(required[0]); i++) {
		if (!xsExtensionEnabled(required[i])) {
			printf("[xadmin][error] required xs extension missing: %s\n", required[i]);
			return false;
		}
	}
	return true;
}

/* 业务段启动：DB 打开/迁移 + 会话/路由/模块/插件全链。正常启动与安装向导
 * 完成后共用（向导在请求线程内调用，G_RequestLock 已序列化）。 */
static bool XAdmin_BusinessStart(XS_HostInfo* host)
{
	if (!XAdmin_RequireExtensions()) return false;
	if (!DB_Init()) { printf("[xadmin][error] business start: DB_Init failed\n"); return false; }
	if (!DB_MigrateTimeUnits()) { printf("[xadmin][error] business start: DB_MigrateTimeUnits failed\n"); return false; }
	if (!DB_EnsureUrisMaskColumn()) { printf("[xadmin][error] business start: uris mask column failed\n"); return false; }
	if (!DB_EnsureUrisEnhancedColumns()) { printf("[xadmin][error] business start: uris enhanced columns failed\n"); return false; }
	if (!DB_EnsurePluginResourceIndex()) { printf("[xadmin][error] business start: plugin resource index failed\n"); return false; }
	G_SessionStarted = true;
	if (!Session_Init()) { printf("[xadmin][error] business start: Session_Init failed\n"); return false; }
	Guard_Init(); RouteHTTP_Init();
	if (!RouteHTTP_Compile()) { printf("[xadmin][error] business start: RouteHTTP_Compile failed\n"); return false; }
	/* SQL 与业务缓存复用 v1。这里不执行历史数据修复/重新安装。 */
	Auth_CompileSQL(); ReloadCache_Auth_Auth(); ReloadCache_Auth_Group();
	{ /* M4：trace 调试接口访问必须留痕（历史 uris 种子 needLog=0；Auth_Init 为未接线的历史入口，故置于此） */
		char* sErr = NULL;
		if ( sqlite3_exec(G_DB, "UPDATE uris SET needLog = 1 WHERE uri LIKE '/admin/trace%'", NULL, NULL, &sErr) != SQLITE_OK && sErr ) sqlite3_free(sErr);
	}
	Member_Init(); MemberAuth_Init(); Notify_Init(); Attachment_Init(); Sched_Init(); StandalonePage_Init();
#if XADMIN_WITH_SMTP
	Mail_Init();
#endif
	Menu_Init(); Logs_Init(); Option_Init(); Form_Init();
	Form_TemplateRegistryInit(); /* {{#form}} 扩展注册表，晚于全部模板编译点声明 */
	if (!Content_Init()) return false; /* 内容系统：7 表 + 能力包装载 + 菜单（生成器阶段 2） */
	PluginHost_ContextInit(host);
	PluginHost_Init();
	Auth_SyncURIS(); MemberAuth_ReloadCache();
	G_BusinessStarted = true;
	G_Ready = true;
	Session_StartTimer(host);
	printf("[xadmin] ready; database=%s/main.db\n", DBPath);
	return true;
}

void ServiceInit(XS_HostInfo* host)
{
	char* path;
	G_RequestLock = xrtMutexCreate();
	AppPath = xrtPathParent(host->Path);
	DBPath = xrtPathJoin(AppPath, "db");
	OptionPath = xrtPathJoin(AppPath, "options");
	path = xrtPathJoin(AppPath, "page"); G_PageRoot = xrtRootOpen(path); xrtFree(path);
	path = xrtPathJoin(AppPath, "template"); G_TemplateRoot = xrtRootOpen(path); xrtFree(path);
	G_Templates = xrtMapCreate(sizeof(xtemplate*));
	if (!G_RequestLock || !G_PageRoot || !G_TemplateRoot || !G_Templates) {
		printf("[xadmin][error] initialization failed; page/template resource missing\n");
		return;
	}
	Install_Init();
	if (G_InstallMode) {
		/* 未安装：业务段延后到向导 POST 完成（建库后在本进程内拉起） */
		printf("[xadmin] install mode; wizard serves all requests until installed\n");
		return;
	}
	if (!XAdmin_BusinessStart(host))
		printf("[xadmin][error] initialization failed; check migrated resources and db/main.db\n");
}

void ServiceUnit(XS_HostInfo* host)
{
	(void)host;
	if (G_BusinessStarted) {
		PluginHost_Unit();
		Attachment_Unit();
		Sched_Unit();
		StandalonePage_Unit();
#if XADMIN_WITH_SMTP
		Mail_Unit();
#endif
		Form_Unit(); Option_Unit(); Logs_Unit(); Menu_Unit(); MemberAuth_Unit(); Member_Unit();
		Auth_Unit();
	}
	Guard_Unit();
	if (G_SessionStarted) Session_Unit();
	G_Ready = false;
	Template_Unit();
	Form_TemplateRegistryUnit(); /* 编译产物全部释放后再回收扩展注册表 */
	if (G_PageRoot) xrtRootClose(G_PageRoot);
	G_PageRoot = NULL;
	RouteHTTP_Unit();
	DB_Unit();
	xrtFree(AppPath); xrtFree(DBPath); xrtFree(OptionPath);
	if (G_RequestLock) xrtMutexDestroy(G_RequestLock);
}
