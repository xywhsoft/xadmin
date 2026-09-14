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
#include "modules/multipart.h"
#include "modules/attachment.h"
#include "modules/sched.h"
#if XADMIN_WITH_SMTP
#include "modules/mail.h"
#endif
#include "modules/menu.h"
#include "modules/logs.h"
#include "modules/option.h"
#include "modules/form.h"
#include "modules/plugin_host.h"
#include "route_http/index.h"
#include "route_http/login.h"
#include "route_http/auth.h"
#include "route_http/member.h"
#include "route_http/api.h"
#include "route_http/notify.h"
#include "route_http/attachment.h"
#include "route_http/attachment_api.h"
#include "route_http/sched.h"
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
#include "route_http/brand.h"
#include "route.h"
#include "modules/protocol.h"

static bool G_SessionStarted, G_BusinessStarted;

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
	if (!G_RequestLock || !G_PageRoot || !G_TemplateRoot || !G_Templates || !DB_Init() || !DB_MigrateTimeUnits() || !DB_EnsureUrisMaskColumn() || !DB_EnsurePluginResourceIndex()) {
		printf("[xadmin][error] initialization failed; check migrated resources and db/main.db\n");
		return;
	}
	G_SessionStarted = true;
	if (!Session_Init()) return;
	Guard_Init(); RouteHTTP_Init();
	if (!RouteHTTP_Compile()) return;
	/* SQL 与业务缓存复用 v1。这里不执行历史数据修复/重新安装。 */
	Auth_CompileSQL(); ReloadCache_Auth_Auth(); ReloadCache_Auth_Group();
	Member_Init(); MemberAuth_Init(); Notify_Init(); Attachment_Init(); Sched_Init();
#if XADMIN_WITH_SMTP
	Mail_Init();
#endif
	Menu_Init(); Logs_Init(); Option_Init(); Form_Init();
	PluginHost_Init();
	Auth_SyncURIS(); MemberAuth_ReloadCache();
	G_BusinessStarted = true;
	G_Ready = true;
	Session_StartTimer(host);
	printf("[xadmin] ready; database=%s/main.db\n", DBPath);
}

void ServiceUnit(XS_HostInfo* host)
{
	(void)host;
	if (G_BusinessStarted) {
		PluginHost_Unit();
		Attachment_Unit();
		Sched_Unit();
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
	if (G_PageRoot) xrtRootClose(G_PageRoot);
	G_PageRoot = NULL;
	RouteHTTP_Unit();
	DB_Unit();
	xrtFree(AppPath); xrtFree(DBPath); xrtFree(OptionPath);
	if (G_RequestLock) xrtMutexDestroy(G_RequestLock);
}
