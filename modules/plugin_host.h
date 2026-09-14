/* 插件宿主内核（xs3 重写，参考 v1 ps_* 语义；ABI v4 冻结）。
 * 职责：扫描/路径转换/manifest 校验/配置装载、TCC 编译与代际生命周期、
 * 27 个 XAdmin_* 宿主函数（路由/菜单/权限/URI/事件/钩子/服务）、
 * plugin_resource 台账登记与回收。DB 为唯一事实源：TCC 状态随主脚本
 * 代际销毁，启动时按 plugin_runtime.enabled 重建。
 * 全部操作发生在 G_RequestLock 内（含管理路由与插件 OnStart）。 */

#include <ctype.h>
#define XS_PLUGIN_HOST_SIDE
#include "../plugin_sdk/xs_plugin.h"

static bool PluginHost_SetEnabled(const char* sXid, bool bEnable);
static bool PluginHost_Reload(const char* sXid);

#define PLUGIN_MAX        32
#define PLUGIN_NAME_MAX   64
#define PLUGIN_PATH_MAX   420
#define PLUGIN_ROUTES_MAX 64
#define PLUGIN_EVT_MAX    128
#define PLUGIN_HOOK_MAX   128
#define PLUGIN_SVC_MAX    32
#define PLUGIN_HOOK_CHAIN 16

typedef struct PluginInstance {
	char xid[PLUGIN_NAME_MAX];
	char rootPath[PLUGIN_PATH_MAX];   /* AppPath/plugin/<xid> */
	char dataPath[PLUGIN_PATH_MAX];   /* AppPath/plugin_data/<xid> */
	char dbPath[PLUGIN_PATH_MAX];     /* AppPath/db/plugin/<xid>/plugin.db */
	xvalue* manifest;                 /* plugin.json（扫描时装载，实例存续期持有） */
	xvalue* config;                   /* 扁平配置表（GLOBAL_OPTION_TABLE 借出） */
	TCCState* tcc;
	const XAdminPluginDescriptor* desc;
	void (*setGlobal)(int idx, void* ptr);
	sqlite3_int64 genRowId;
	int generation;
	int activeLeases; /* 未归还的服务租借数：>0 时代际停用不得销毁代码镜像 */
	bool started;
	str routePaths[PLUGIN_ROUTES_MAX]; /* RouteInfo.Path 指针的所有权在实例 */
	size_t routeCount;
} PluginInstance;

static PluginInstance G_Plugins[PLUGIN_MAX];
static size_t G_PluginCount;
static int G_PluginRegIdx = -1; /* 正在执行 OnStart 的实例下标，注册函数归属判定 */

typedef struct {
	char name[PLUGIN_NAME_MAX];
	XAdminEventProc proc;
	int pluginIdx; /* -1 = 空槽 */
	bool used;
} PluginEventSlot;
static PluginEventSlot G_PluginEvents[PLUGIN_EVT_MAX];

typedef struct {
	char name[PLUGIN_NAME_MAX];
	XAdminHookProc proc;
	int sort;
	int pluginIdx;
	bool used;
} PluginHookSlot;
static PluginHookSlot G_PluginHooks[PLUGIN_HOOK_MAX];

typedef struct {
	char name[PLUGIN_NAME_MAX];
	int major, minor;
	int pluginIdx;
	const void* vtable;
	bool used;
} PluginServiceSlot;
static PluginServiceSlot G_PluginServices[PLUGIN_SVC_MAX];

/* GR1：租借改为堆单元——记录获取时的提供方代码镜像。
 * 提供方换代/停用后 ReleaseService 按 tcc 指针路由到退役队列，
 * 最后一个租借归还时才真正销毁镜像（v1 lease/DRAINING 语义的对齐实现）。 */
typedef struct PluginServiceLeaseCell {
	TCCState* tcc;
	struct PluginInstance* provider;
} PluginServiceLeaseCell;

#define PLUGIN_TCC_RETIRE_MAX 16
typedef struct {
	TCCState* tcc;
	int leases;
	bool used;
} PluginTccRetire;
static PluginTccRetire G_PluginTccRetired[PLUGIN_TCC_RETIRE_MAX];

/* ==================== 路径与 DB 基础 ==================== */

static PluginInstance* Plugin_Find(const char* sXid)
{
	size_t i;
	if (!sXid) return NULL;
	for (i = 0; i < G_PluginCount; i++)
		if (!strcmp(G_Plugins[i].xid, sXid)) return &G_Plugins[i];
	return NULL;
}

static bool Plugin_XidValid(const char* sXid)
{
	size_t i, len;
	if (!sXid || !(len = strlen(sXid)) || len >= PLUGIN_NAME_MAX) return false;
	if (!(isalnum((unsigned char)sXid[0]))) return false;
	for (i = 0; i < len; i++)
		if (!(isalnum((unsigned char)sXid[i]) || sXid[i] == '-' || sXid[i] == '_' || sXid[i] == '.'))
			return false;
	return true;
}

static bool Plugin_Exec(const char* sSql)
{
	return sqlite3_exec(G_DB, sSql, NULL, NULL, NULL) == SQLITE_OK;
}

static void Plugin_BindText(sqlite3_stmt* stmt, int idx, const char* text)
{
	sqlite3_bind_text(stmt, idx, text ? text : "", -1, SQLITE_STATIC);
}

static sqlite3_int64 Plugin_LastRow(void)
{
	return sqlite3_last_insert_rowid(G_DB);
}

/* v1 库中的旧布局路径（hosts/xadmin[/data]/plugin/...）幂等转换为主线布局。 */
static void Plugin_ConvertLegacyPaths(void)
{
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(G_DB, "SELECT package_id, xid FROM plugin_package;", -1, &stmt, NULL) != SQLITE_OK)
		return;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char* sPkg = (const char*)sqlite3_column_text(stmt, 0);
		const char* sXid = (const char*)sqlite3_column_text(stmt, 1);
		sqlite3_stmt* up = NULL;
		char* sRoot = xrtPathJoin(AppPath, "plugin");
		char* sInstall = xrtPathJoin(sRoot, sXid);
		char* sDataRoot = xrtPathJoin(AppPath, "plugin_data");
		char* sData = xrtPathJoin(sDataRoot, sXid);
		char* sDbRoot = xrtPathJoin(AppPath, "db");
		char* sDbRoot2 = xrtPathJoin(sDbRoot, "plugin");
		char* sDbDir = xrtPathJoin(sDbRoot2, sXid);
		char* sDb = xrtPathJoin(sDbDir, "plugin.db");
		char* sSql;
		(void)sPkg;
		sSql = xrtFormat(
			"UPDATE plugin_package SET install_path='%s' WHERE xid='%s' AND install_path != '%s';"
			"UPDATE plugin_runtime SET data_path='%s', private_db_path='%s' WHERE xid='%s' AND (data_path != '%s' OR private_db_path != '%s');",
			sInstall, sXid, sInstall, sData, sDb, sXid, sData, sDb);
		if (sSql) Plugin_Exec(sSql);
		xrtFree(sSql); xrtFree(sDb); xrtFree(sDbDir); xrtFree(sDbRoot2); xrtFree(sDbRoot);
		xrtFree(sData); xrtFree(sDataRoot); xrtFree(sInstall); xrtFree(sRoot);
		(void)up;
	}
	sqlite3_finalize(stmt);
}

/* ==================== 台账 ==================== */

static void Plugin_LedgerAdd(PluginInstance* inst, const char* sType, const char* sKey, const char* sRef, const char* sPolicy)
{
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(G_DB,
		"INSERT INTO plugin_resource (instance_id, generation, owner_scope, resource_type, resource_key, resource_ref, destroy_policy, create_time, status, xid) "
		"VALUES (NULL, ?, 'generation', ?, ?, ?, ?, ?, 'active', ?);", -1, &stmt, NULL) != SQLITE_OK)
		return;
	sqlite3_bind_int(stmt, 1, inst->generation);
	Plugin_BindText(stmt, 2, sType);
	Plugin_BindText(stmt, 3, sKey);
	Plugin_BindText(stmt, 4, sRef);
	Plugin_BindText(stmt, 5, sPolicy);
	sqlite3_bind_int64(stmt, 6, xrtNow());
	Plugin_BindText(stmt, 7, inst->xid);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

static void Plugin_LedgerRemove(const char* sXid, int iGeneration, const char* sType, const char* sRef)
{
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(G_DB,
		"UPDATE plugin_resource SET status='removed' WHERE xid=? AND generation=? AND resource_type=? AND resource_ref=? AND status='active';",
		-1, &stmt, NULL) != SQLITE_OK)
		return;
	Plugin_BindText(stmt, 1, sXid);
	sqlite3_bind_int(stmt, 2, iGeneration);
	Plugin_BindText(stmt, 3, sType);
	Plugin_BindText(stmt, 4, sRef);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

static void Plugin_ReloadPermissionCaches(void)
{
	ReloadCache_Auth_Auth();
	ReloadCache_Auth_Group();
	Auth_ReloadCache();
	ReloadCache_MemberAuth();
	ReloadCache_MemberAuthGroup();
	MemberAuth_ReloadCache();
	Auth_UpdateURIS();
}

/* 停止时按台账回收全部资源（v1 destroy_policy 语义）。 */
static void Plugin_CleanupResources(PluginInstance* inst)
{
	sqlite3_stmt* stmt = NULL;
	bool bReload = false;
	if (sqlite3_prepare_v2(G_DB,
		"SELECT resource_type, resource_ref FROM plugin_resource "
		"WHERE xid=? AND generation=? AND status='active';", -1, &stmt, NULL) != SQLITE_OK)
		return;
	Plugin_BindText(stmt, 1, inst->xid);
	sqlite3_bind_int(stmt, 2, inst->generation);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char* sType = (const char*)sqlite3_column_text(stmt, 0);
		const char* sRef = (const char*)sqlite3_column_text(stmt, 1);
		if (!strcmp(sType, "route")) {
			xrtMapRemove(G_StaticRouteTableHTTP, KeyView(sRef));
		} else if (!strcmp(sType, "menu")) {
			Plugin_Exec(xrtFormat("UPDATE menu SET isDelete=1, updateTime=%lld WHERE id=%s;", xrtNow(), sRef));
		} else if (!strcmp(sType, "admin_auth")) {
			Plugin_Exec(xrtFormat("UPDATE auth SET isDelete=1, updateTime=%lld WHERE id=%s;", xrtNow(), sRef));
		} else if (!strcmp(sType, "admin_auth_group")) {
			Plugin_Exec(xrtFormat("UPDATE authGroup SET isDelete=1, updateTime=%lld WHERE id=%s;", xrtNow(), sRef));
		} else if (!strcmp(sType, "member_auth")) {
			Plugin_Exec(xrtFormat("UPDATE memberAuth SET isDelete=1, updateTime=%lld WHERE id=%s;", xrtNow(), sRef));
		} else if (!strcmp(sType, "member_auth_group")) {
			Plugin_Exec(xrtFormat("UPDATE memberAuthGroup SET isDelete=1, updateTime=%lld WHERE id=%s;", xrtNow(), sRef));
		} else if (!strcmp(sType, "admin_uri_auth") || !strcmp(sType, "member_uri_auth")) {
			Plugin_Exec(xrtFormat("DELETE FROM uris WHERE id=%s;", sRef));
		}
		Plugin_LedgerRemove(inst->xid, inst->generation, sType, sRef);
		bReload = true;
	}
	sqlite3_finalize(stmt);
	/* 事件/钩子/服务：按归属清空内存槽 */
	{
		int i;
		for (i = 0; i < PLUGIN_EVT_MAX; i++)
			if (G_PluginEvents[i].used && !strcmp(G_Plugins[G_PluginEvents[i].pluginIdx].xid, inst->xid))
				G_PluginEvents[i].used = false;
		for (i = 0; i < PLUGIN_HOOK_MAX; i++)
			if (G_PluginHooks[i].used && !strcmp(G_Plugins[G_PluginHooks[i].pluginIdx].xid, inst->xid))
				G_PluginHooks[i].used = false;
		for (i = 0; i < PLUGIN_SVC_MAX; i++)
			if (G_PluginServices[i].used && !strcmp(G_Plugins[G_PluginServices[i].pluginIdx].xid, inst->xid))
				G_PluginServices[i].used = false;
	}
	if (bReload) Plugin_ReloadPermissionCaches();
}

/* ==================== ABI：路由/菜单/权限/URI ==================== */

static PluginInstance* Plugin_Caller(void)
{
	if (G_PluginRegIdx < 0 || (size_t)G_PluginRegIdx >= G_PluginCount) return NULL;
	if (!G_Plugins[G_PluginRegIdx].started) return NULL;
	return &G_Plugins[G_PluginRegIdx];
}

int XAdmin_RegisterRoute(XAdminPluginHandle plugin_handle, const XAdminRouteDecl* decl, XAdminRouteToken* token)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	RouteInfo* route;
	bool added = false;
	if (!inst || !inst->started || !decl || !decl->path || decl->path[0] != '/' || !decl->proc)
		return -1;
	if (inst->routeCount == PLUGIN_ROUTES_MAX)
		return -1;
	route = xrtMapGetOrAdd(G_StaticRouteTableHTTP, KeyView(decl->path), &added);
	if (!route) return -1;
	if (added) {
		memset(route, 0, sizeof(*route));
		route->Path = xrtStrDup(decl->path);
		route->bAdmin = route->bAuth = true;
	} else {
		printf("[plugin][warn] duplicate route %s from %s\n", decl->path, inst->xid);
		return -1; /* 重复注册失败：路径必须唯一 */
	}
	route->bAuth = decl->need_auth ? true : false;
	route->bAdmin = decl->admin_only ? true : false;
	route->AuthID = (uint32)decl->auth_id;
	route->AuthLevel = (uint32)decl->auth_level;
	RouteSetMethods(route, XHTTP_METHOD_ANY, (XAdminRouteProc)decl->proc);
	inst->routePaths[inst->routeCount++] = (str)route->Path;
	Plugin_LedgerAdd(inst, "route", decl->path, decl->path, "auto_unload");
	if (token) *token = (XAdminRouteToken)(uintptr_t)route;
	return 0;
}

int XAdmin_UnregisterRoute(XAdminRouteToken token)
{
	RouteInfo* route = (RouteInfo*)(uintptr_t)token;
	size_t i, j;
	if (!route || !route->Path) return -1;
	for (i = 0; i < G_PluginCount; i++) {
		PluginInstance* inst = &G_Plugins[i];
		for (j = 0; j < inst->routeCount; j++) {
			/* DictRemove 会释放映射条目，Path 必须先取到本地再使用。 */
			if (inst->routePaths[j] == route->Path) {
				const char* sPath = inst->routePaths[j];
				xrtMapRemove(G_StaticRouteTableHTTP, KeyView(sPath));
				xrtFree((void*)sPath);
				memmove(&inst->routePaths[j], &inst->routePaths[j + 1], (inst->routeCount - j - 1) * sizeof(str));
				inst->routeCount--;
				Plugin_LedgerRemove(inst->xid, inst->generation, "route", sPath);
				return 0;
			}
		}
	}
	return -1;
}

int XAdmin_RegisterMenu(XAdminPluginHandle plugin_handle, const XAdminMenuDecl* decl, int* out_menu_id, XAdminMenuToken* token)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	sqlite3_stmt* stmt = NULL;
	int id;
	if (!inst || !inst->started || !decl || !decl->title) return -1;
	if (sqlite3_prepare_v2(G_DB,
		"INSERT INTO menu (parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete, plugin_xid, plugin_generation) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?, ?);", -1, &stmt, NULL) != SQLITE_OK)
		return -1;
	sqlite3_bind_int(stmt, 1, decl->parent_id);
	Plugin_BindText(stmt, 2, decl->title);
	Plugin_BindText(stmt, 3, decl->icon);
	sqlite3_bind_int(stmt, 4, decl->type);
	Plugin_BindText(stmt, 5, decl->open_type);
	Plugin_BindText(stmt, 6, decl->href);
	sqlite3_bind_int(stmt, 7, decl->sort);
	sqlite3_bind_int(stmt, 8, decl->visible ? 1 : 0);
	Plugin_BindText(stmt, 9, decl->remark);
	sqlite3_bind_int64(stmt, 10, xrtNow());
	sqlite3_bind_int64(stmt, 11, xrtNow());
	Plugin_BindText(stmt, 12, inst->xid);
	sqlite3_bind_int(stmt, 13, inst->generation);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	id = (int)Plugin_LastRow();
	if (out_menu_id) *out_menu_id = id;
	Plugin_LedgerAdd(inst, "menu", decl->href ? decl->href : "", xrtFormat("%d", id), "soft_delete");
	if (token) *token = ((XAdminMenuToken)2 << 40) | (uint64)id;
	return 0;
}

int XAdmin_UnregisterMenu(XAdminMenuToken token)
{
	int id = (int)(token & 0xFFFFFFFFFF);
	Plugin_Exec(xrtFormat("UPDATE menu SET isDelete=1, updateTime=%lld WHERE id=%d;", xrtNow(), id));
	return 0;
}

int XAdmin_RegisterAuthGroup(XAdminPluginHandle plugin_handle, const XAdminAuthGroupDecl* decl, int* out_group_id, XAdminAuthGroupToken* token)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	sqlite3_stmt* stmt = NULL;
	const char* sTable = (decl && decl->scope == XADMIN_AUTH_SCOPE_MEMBER) ? "memberAuthGroup" : "authGroup";
	int id;
	if (!inst || !inst->started || !decl || !decl->name) return -1;
	if (sqlite3_prepare_v2(G_DB, xrtFormat(
		"INSERT INTO %s (name, [desc], sort, createTime, updateTime, isDelete, plugin_xid) VALUES (?, ?, ?, ?, ?, 0, ?);", sTable),
		-1, &stmt, NULL) != SQLITE_OK)
		return -1;
	Plugin_BindText(stmt, 1, decl->name);
	Plugin_BindText(stmt, 2, decl->description);
	sqlite3_bind_int(stmt, 3, decl->sort);
	sqlite3_bind_int64(stmt, 4, xrtNow());
	sqlite3_bind_int64(stmt, 5, xrtNow());
	Plugin_BindText(stmt, 6, inst->xid);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	id = (int)Plugin_LastRow();
	if (out_group_id) *out_group_id = id;
	Plugin_LedgerAdd(inst, (decl->scope == XADMIN_AUTH_SCOPE_MEMBER) ? "member_auth_group" : "admin_auth_group",
		decl->key ? decl->key : decl->name, xrtFormat("%d", id), "soft_delete");
	if (token) *token = ((XAdminAuthGroupToken)3 << 40) | (uint64)id;
	Plugin_ReloadPermissionCaches();
	return 0;
}

int XAdmin_UnregisterAuthGroup(XAdminAuthGroupToken token)
{
	int id = (int)(token & 0xFFFFFFFFFF);
	Plugin_Exec(xrtFormat("UPDATE authGroup SET isDelete=1 WHERE id=%d;", id));
	Plugin_Exec(xrtFormat("UPDATE memberAuthGroup SET isDelete=1 WHERE id=%d;", id));
	return 0;
}

int XAdmin_RegisterAuth(XAdminPluginHandle plugin_handle, const XAdminAuthDecl* decl, int* out_auth_id, XAdminAuthToken* token)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	sqlite3_stmt* stmt = NULL;
	const char* sTable = (decl && decl->scope == XADMIN_AUTH_SCOPE_MEMBER) ? "memberAuth" : "auth";
	int id;
	if (!inst || !inst->started || !decl || !decl->name) return -1;
	if (sqlite3_prepare_v2(G_DB, xrtFormat(
		"INSERT INTO %s (groupID, name, [desc], sort, createTime, updateTime, isDelete, plugin_xid) VALUES (?, ?, ?, ?, ?, ?, 0, ?);", sTable),
		-1, &stmt, NULL) != SQLITE_OK)
		return -1;
	sqlite3_bind_int(stmt, 1, decl->group_id);
	Plugin_BindText(stmt, 2, decl->name);
	Plugin_BindText(stmt, 3, decl->description);
	sqlite3_bind_int(stmt, 4, decl->sort);
	sqlite3_bind_int64(stmt, 5, xrtNow());
	sqlite3_bind_int64(stmt, 6, xrtNow());
	Plugin_BindText(stmt, 7, inst->xid);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	id = (int)Plugin_LastRow();
	if (out_auth_id) *out_auth_id = id;
	Plugin_LedgerAdd(inst, (decl->scope == XADMIN_AUTH_SCOPE_MEMBER) ? "member_auth" : "admin_auth",
		decl->key ? decl->key : decl->name, xrtFormat("%d", id), "soft_delete");
	if (token) *token = ((XAdminAuthToken)4 << 40) | (uint64)id;
	Plugin_ReloadPermissionCaches();
	return 0;
}

int XAdmin_UnregisterAuth(XAdminAuthToken token)
{
	int id = (int)(token & 0xFFFFFFFFFF);
	Plugin_Exec(xrtFormat("UPDATE auth SET isDelete=1 WHERE id=%d;", id));
	Plugin_Exec(xrtFormat("UPDATE memberAuth SET isDelete=1 WHERE id=%d;", id));
	return 0;
}

int XAdmin_RegisterUriAuth(XAdminPluginHandle plugin_handle, const XAdminUriAuthDecl* decl, int* out_uri_id, XAdminUriAuthToken* token)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	sqlite3_stmt* stmt = NULL;
	int id;
	bool bAdmin = decl ? (decl->scope == XADMIN_AUTH_SCOPE_ADMIN) : true;
	if (!inst || !inst->started || !decl || !decl->uri || decl->uri[0] != '/') return -1;
	if (sqlite3_prepare_v2(G_DB,
		"INSERT INTO uris (authID, uri, [desc], sort, isBackend, needAuth, needLog, keepActive, createTime, updateTime, plugin_xid) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);", -1, &stmt, NULL) != SQLITE_OK)
		return -1;
	sqlite3_bind_int(stmt, 1, decl->auth_id);
	Plugin_BindText(stmt, 2, decl->uri);
	Plugin_BindText(stmt, 3, decl->description);
	sqlite3_bind_int(stmt, 4, decl->sort);
	sqlite3_bind_int(stmt, 5, bAdmin ? 1 : 0);
	sqlite3_bind_int(stmt, 6, decl->need_auth ? 1 : 0);
	sqlite3_bind_int(stmt, 7, decl->need_log ? 1 : 0);
	sqlite3_bind_int(stmt, 8, decl->keep_active ? 1 : 0);
	sqlite3_bind_int64(stmt, 9, xrtNow());
	sqlite3_bind_int64(stmt, 10, xrtNow());
	Plugin_BindText(stmt, 11, inst->xid);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	id = (int)Plugin_LastRow();
	if (out_uri_id) *out_uri_id = id;
	/* 已注册的同路径路由同步标志，未注册的路由由 Auth_UpdateURIS 收录 */
	{
		RouteInfo* route = xrtMapGet(G_StaticRouteTableHTTP, KeyView(decl->uri));
		if (route) {
			route->AuthID = (uint32)decl->auth_id;
			route->bAuth = decl->need_auth ? true : false;
			route->bAdmin = bAdmin;
			route->bPutLog = decl->need_log ? true : false;
			route->bActive = decl->keep_active ? true : false;
		}
	}
	Plugin_LedgerAdd(inst, bAdmin ? "admin_uri_auth" : "member_uri_auth", decl->uri, xrtFormat("%d", id), "delete");
	if (token) *token = ((XAdminUriAuthToken)5 << 40) | (uint64)id;
	return 0;
}

int XAdmin_UnregisterUriAuth(XAdminUriAuthToken token)
{
	int id = (int)(token & 0xFFFFFFFFFF);
	Plugin_Exec(xrtFormat("DELETE FROM uris WHERE id=%d;", id));
	return 0;
}

/* ==================== ABI：事件 / 钩子 / 服务 ==================== */

int XAdmin_ListenEvent(XAdminPluginHandle plugin_handle, const XAdminEventDecl* decl, XAdminEventToken* token)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	int i;
	if (!inst || !inst->started || !decl || !decl->event_name || !decl->proc) return -1;
	for (i = 0; i < PLUGIN_EVT_MAX; i++) {
		if (!G_PluginEvents[i].used) {
			snprintf(G_PluginEvents[i].name, PLUGIN_NAME_MAX, "%s", decl->event_name);
			G_PluginEvents[i].proc = decl->proc;
			G_PluginEvents[i].pluginIdx = (int)(inst - G_Plugins);
			G_PluginEvents[i].used = true;
			if (token) *token = (XAdminEventToken)(i + 1);
			return 0;
		}
	}
	return -1;
}

int XAdmin_UnlistenEvent(XAdminEventToken token)
{
	if (token == 0 || token > PLUGIN_EVT_MAX) return -1;
	G_PluginEvents[token - 1].used = false;
	return 0;
}

int XAdmin_EmitEvent(XAdminPluginHandle plugin_handle, const char* event_name, void* payload, size_t payload_size)
{
	int i;
	(void)plugin_handle;
	if (!event_name) return -1;
	for (i = 0; i < PLUGIN_EVT_MAX; i++)
		if (G_PluginEvents[i].used && !strcmp(G_PluginEvents[i].name, event_name))
			G_PluginEvents[i].proc(event_name, payload, payload_size);
	return 0;
}

int XAdmin_RegisterHook(XAdminPluginHandle plugin_handle, const XAdminHookDecl* decl, XAdminHookToken* token)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	int i;
	if (!inst || !inst->started || !decl || !decl->hook_name || !decl->proc) return -1;
	for (i = 0; i < PLUGIN_HOOK_MAX; i++) {
		if (!G_PluginHooks[i].used) {
			snprintf(G_PluginHooks[i].name, PLUGIN_NAME_MAX, "%s", decl->hook_name);
			G_PluginHooks[i].proc = decl->proc;
			G_PluginHooks[i].sort = decl->sort;
			G_PluginHooks[i].pluginIdx = (int)(inst - G_Plugins);
			G_PluginHooks[i].used = true;
			if (token) *token = (XAdminHookToken)(i + 1);
			return 0;
		}
	}
	return -1;
}

int XAdmin_UnregisterHook(XAdminHookToken token)
{
	if (token == 0 || token > PLUGIN_HOOK_MAX) return -1;
	G_PluginHooks[token - 1].used = false;
	return 0;
}

int XAdmin_InvokeHook(XAdminPluginHandle plugin_handle, const char* hook_name, void* payload, size_t payload_size)
{
	int order[PLUGIN_HOOK_CHAIN];
	int count = 0, i, j, ret = XADMIN_HOOK_CONTINUE;
	(void)plugin_handle;
	if (!hook_name) return -1;
	for (i = 0; i < PLUGIN_HOOK_MAX && count < PLUGIN_HOOK_CHAIN; i++)
		if (G_PluginHooks[i].used && !strcmp(G_PluginHooks[i].name, hook_name))
			order[count++] = i;
	/* 按 sort 稳定插入排序（小数组） */
	for (i = 1; i < count; i++) {
		int v = order[i];
		for (j = i - 1; j >= 0 && G_PluginHooks[order[j]].sort > G_PluginHooks[v].sort; j--)
			order[j + 1] = order[j];
		order[j + 1] = v;
	}
	for (i = 0; i < count; i++) {
		ret = G_PluginHooks[order[i]].proc(hook_name, payload, payload_size);
		if (ret != XADMIN_HOOK_CONTINUE) break;
	}
	return ret;
}

int XAdmin_RegisterService(XAdminPluginHandle plugin_handle, const XAdminServiceDecl* decl, const void* vtable)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	int i;
	if (!inst || !inst->started || !decl || !decl->service_name || !vtable) return -1;
	for (i = 0; i < PLUGIN_SVC_MAX; i++) {
		if (G_PluginServices[i].used && !strcmp(G_PluginServices[i].name, decl->service_name))
			return -1; /* 重名服务拒绝 */
	}
	for (i = 0; i < PLUGIN_SVC_MAX; i++) {
		if (!G_PluginServices[i].used) {
			snprintf(G_PluginServices[i].name, PLUGIN_NAME_MAX, "%s", decl->service_name);
			G_PluginServices[i].major = decl->major_version;
			G_PluginServices[i].minor = decl->minor_version;
			G_PluginServices[i].pluginIdx = (int)(inst - G_Plugins);
			G_PluginServices[i].vtable = vtable;
			G_PluginServices[i].used = true;
			return 0;
		}
	}
	return -1;
}

int XAdmin_AcquireService(XAdminPluginHandle plugin_handle, const char* name, int major, XAdminServiceLease* out_lease, const void** out_vtable)
{
	int i;
	(void)plugin_handle;
	if (!name || !out_lease || !out_vtable) return -1;
	for (i = 0; i < PLUGIN_SVC_MAX; i++) {
		PluginServiceSlot* svc = &G_PluginServices[i];
		PluginInstance* provider;
		PluginServiceLeaseCell* cell;
		if (!svc->used || strcmp(svc->name, name)) continue;
		if (svc->major != major) continue; /* ABI 仅按主版本协商 */
		provider = &G_Plugins[svc->pluginIdx];
		if (!provider->started || !provider->tcc) return -1;
		cell = xrtMalloc(sizeof(*cell));
		if (!cell) return -1;
		cell->tcc = provider->tcc;
		cell->provider = provider;
		provider->activeLeases++;
		*out_lease = (XAdminServiceLease)cell;
		*out_vtable = svc->vtable;
		return 0;
	}
	return -1;
}

int XAdmin_ReleaseService(XAdminServiceLease lease)
{
	PluginServiceLeaseCell* cell = (PluginServiceLeaseCell*)lease;
	if (!cell) return -1;
	if (cell->provider && cell->provider->tcc == cell->tcc) {
		/* 提供方仍在同一镜像上运行 */
		if (cell->provider->activeLeases > 0) cell->provider->activeLeases--;
	} else {
		/* 提供方已换代/停用：按镜像指针路由到退役队列 */
		int i;
		for (i = 0; i < PLUGIN_TCC_RETIRE_MAX; i++) {
			if (G_PluginTccRetired[i].used && G_PluginTccRetired[i].tcc == cell->tcc) {
				if (--G_PluginTccRetired[i].leases <= 0) {
					xsDestroyTCC(G_PluginTccRetired[i].tcc);
					G_PluginTccRetired[i].used = false;
				}
				break;
			}
		}
	}
	xrtFree(cell);
	return 0;
}

/* ==================== ABI：插件间管理 ==================== */

int XAdmin_GeneratePlugin(XAdminPluginHandle plugin_handle, const XAdminGeneratedPluginSpec* spec)
{
	/* P3 批次（包管理与生成器）接入；先按契约返回失败。 */
	(void)plugin_handle; (void)spec;
	return -1;
}

int XAdmin_ReloadPlugin(XAdminPluginHandle plugin_handle, const char* xid)
{
	(void)plugin_handle;
	return PluginHost_Reload(xid) ? 0 : -1;
}

int XAdmin_SetPluginEnabled(XAdminPluginHandle plugin_handle, const char* xid, int enabled)
{
	(void)plugin_handle;
	return PluginHost_SetEnabled(xid, enabled ? true : false) ? 0 : -1;
}

/* ==================== ABI：HTTP 便捷函数 ==================== */

int HttpReplyFormat(XS_ResponseObject objResp, int iCode, const char* sHead, const char* sFormat, ...)
{
	va_list args;
	char* text;
	int result;
	int count;
	va_start(args, sFormat);
	count = vsnprintf(NULL, 0, sFormat, args);
	va_end(args);
	if (count < 0) return -1;
	text = xrtMalloc((size_t)count + 1);
	if (!text) return -1;
	va_start(args, sFormat);
	vsnprintf(text, (size_t)count + 1, sFormat, args);
	va_end(args);
	result = xsHttpReplyAuto(objResp, iCode, sHead, text, 0);
	xrtFree(text);
	return result;
}

static int XAdmin_PluginMethodID(XS_RequestObject objReq)
{
	return ((XAdminRequest*)objReq)->raw->head->MethodCode;
}

/* ==================== 编译与生命周期 ==================== */

static xvalue* Plugin_LoadFlatJson(const char* sPath)
{
	size_t size = 0;
	bytes data;
	xvalue* tbl;
	data = (bytes)xrtFileReadAll(sPath, &size);
	if (!data) return NULL;
	tbl = xrtJsonParse(xrtStrViewN((const char*)data, size));
	xrtFree(data);
	if (tbl && xrtValueType(tbl) != XVALUE_OBJECT) {
		xrtValueRelease(tbl);
		return NULL;
	}
	return tbl;
}

static xvalue* Plugin_LoadConfig(PluginInstance* inst)
{
	xvalue* cfg = NULL;
	char* sDefaults = xrtPathJoin(inst->rootPath, "config.defaults.json");
	char* sOptions = xrtPathJoin(OptionPath, xrtFormat("plugin/%s.json", inst->xid));
	xvalue* over;
	if (sDefaults) {
		cfg = Plugin_LoadFlatJson(sDefaults);
		xrtFree(sDefaults);
	}
	if (!cfg) cfg = ValueObject();
	over = sOptions ? Plugin_LoadFlatJson(sOptions) : NULL;
	if (over) {
		xvalueiter it = {0};
		xvaluekey key;
		xvalue* val;
		if (xrtValueIterBegin(over, &it)) {
			while ((val = xrtValueIterNext(&it, &key))) {
				ValueSetOwn(cfg, key.String.Data, xrtValueDeepClone(val));
			}
			xrtValueIterEnd(&it);
		}
		xrtValueRelease(over);
	}
	if (sOptions) xrtFree(sOptions);
	return cfg;
}

static bool Plugin_Compile(PluginInstance* inst, char* sError, size_t iErrorSize)
{
	TCCState* tcc;
	char* sSdk = xrtPathJoin(AppPath, "plugin_sdk");
	xvalue* arr;
	bool bOk = false;

	tcc = xsCreateTCC();
	if (!tcc) {
		snprintf(sError, iErrorSize, "tcc create failed");
		return false;
	}
	tcc_add_include_path(tcc, inst->rootPath);
	if (sSdk) {
		tcc_add_include_path(tcc, sSdk);
		xrtFree(sSdk);
	}
	arr = ValueGet(ValueGet(inst->manifest, "build"), "includeDirs");
	if (arr && xrtValueType(arr) == XVALUE_ARRAY)
		for (uint32 i = 0; i < ValueCount(arr); i++) {
			str sRel = ValueArrayText(arr, i);
			if (sRel && sRel[0]) {
				char* sFull = xrtPathJoin(inst->rootPath, sRel);
				if (xrtDirExists(sFull)) tcc_add_include_path(tcc, sFull);
				xrtFree(sFull);
			}
		}
	arr = ValueGet(ValueGet(inst->manifest, "build"), "defines");
	if (arr && xrtValueType(arr) == XVALUE_ARRAY)
		for (uint32 i = 0; i < ValueCount(arr); i++) {
			str sDef = ValueArrayText(arr, i);
			if (sDef && sDef[0]) {
				str sDefCopy = xrtStrDup(sDef);
				if (sDefCopy) {
					str pEq = strchr(sDefCopy, '=');
					if (pEq) {
						*pEq = 0;
						tcc_define_symbol(tcc, sDefCopy, pEq + 1);
					} else {
						tcc_define_symbol(tcc, sDefCopy, "1");
					}
					xrtFree(sDefCopy);
				}
			}
		}
	/* 符号注入：27 个 ABI 函数 + 应用级请求/回复原语。
	 * 值/时间/文件等原生 xrt API 已由 xsCreateTCC 预置（全量符号 + /xs 头），
	 * 不在此重复注册；插件侧便捷层见 plugin_sdk/。 */
	{
		static const struct { const char* name; const void* ptr; } symbols[] = {
			{"HttpReplyFormat", (const void*)HttpReplyFormat},
			{"LoadPage", (const void*)LoadPage},
			{"xsHttpReplyAuto", (const void*)xsHttpReplyAuto},
			{"xsHttpReplyFormat", (const void*)xsHttpReplyFormat},
			{"xsReqMethodID", (const void*)XAdmin_PluginMethodID},
			{"xsReqQueryValue", (const void*)xsReqQueryValue},
			{"XAdmin_PluginReqHeader", (const void*)XAdmin_PluginReqHeader},
			{"XAdmin_ReqBody", (const void*)XAdmin_ReqBody},
			{"XAdmin_ReqBodyLen", (const void*)XAdmin_ReqBodyLen},
			{"XAdmin_RegisterRoute", (const void*)XAdmin_RegisterRoute},
			{"XAdmin_UnregisterRoute", (const void*)XAdmin_UnregisterRoute},
			{"XAdmin_RegisterMenu", (const void*)XAdmin_RegisterMenu},
			{"XAdmin_UnregisterMenu", (const void*)XAdmin_UnregisterMenu},
			{"XAdmin_RegisterAuthGroup", (const void*)XAdmin_RegisterAuthGroup},
			{"XAdmin_UnregisterAuthGroup", (const void*)XAdmin_UnregisterAuthGroup},
			{"XAdmin_RegisterAuth", (const void*)XAdmin_RegisterAuth},
			{"XAdmin_UnregisterAuth", (const void*)XAdmin_UnregisterAuth},
			{"XAdmin_RegisterUriAuth", (const void*)XAdmin_RegisterUriAuth},
			{"XAdmin_UnregisterUriAuth", (const void*)XAdmin_UnregisterUriAuth},
			{"XAdmin_ListenEvent", (const void*)XAdmin_ListenEvent},
			{"XAdmin_UnlistenEvent", (const void*)XAdmin_UnlistenEvent},
			{"XAdmin_EmitEvent", (const void*)XAdmin_EmitEvent},
			{"XAdmin_RegisterHook", (const void*)XAdmin_RegisterHook},
			{"XAdmin_UnregisterHook", (const void*)XAdmin_UnregisterHook},
			{"XAdmin_InvokeHook", (const void*)XAdmin_InvokeHook},
			{"XAdmin_RegisterService", (const void*)XAdmin_RegisterService},
			{"XAdmin_AcquireService", (const void*)XAdmin_AcquireService},
			{"XAdmin_ReleaseService", (const void*)XAdmin_ReleaseService},
			{"XAdmin_GeneratePlugin", (const void*)XAdmin_GeneratePlugin},
			{"XAdmin_ReloadPlugin", (const void*)XAdmin_ReloadPlugin},
			{"XAdmin_SetPluginEnabled", (const void*)XAdmin_SetPluginEnabled},
		};
		size_t i;
		for (i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++)
			tcc_add_symbol(tcc, symbols[i].name, symbols[i].ptr);
	}
	/* 源文件：build.sources 优先，否则 entry */
	{
		str sEntry = ValueText(ValueGet(inst->manifest, "build"), "entry");
		bool bCompiled = false;
		arr = ValueGet(ValueGet(inst->manifest, "build"), "sources");
		if (arr && xrtValueType(arr) == XVALUE_ARRAY)
			for (uint32 i = 0; i < ValueCount(arr); i++) {
				str sSrc = ValueArrayText(arr, i);
				if (sSrc && sSrc[0]) {
					char* sFull = xrtPathJoin(inst->rootPath, sSrc);
					if (tcc_add_file(tcc, sFull) < 0) {
						snprintf(sError, iErrorSize, "compile failed: %s", sSrc);
						xrtFree(sFull);
						goto failed;
					}
					bCompiled = true;
					xrtFree(sFull);
				}
			}
		if (!bCompiled && sEntry && sEntry[0]) {
			char* sFull = xrtPathJoin(inst->rootPath, sEntry);
			if (tcc_add_file(tcc, sFull) < 0) {
				snprintf(sError, iErrorSize, "compile failed: %s", sEntry);
				xrtFree(sFull);
				goto failed;
			}
			xrtFree(sFull);
		}
	}
	if (tcc_relocate(tcc) < 0) {
		snprintf(sError, iErrorSize, "relocate failed");
		goto failed;
	}
	inst->desc = (const XAdminPluginDescriptor*)tcc_get_symbol(tcc, "XAdmin_GetPluginDescriptor") ? ((const XAdminPluginDescriptor* (*)(void))tcc_get_symbol(tcc, "XAdmin_GetPluginDescriptor"))() : NULL;
	inst->setGlobal = (void (*)(int, void*))tcc_get_symbol(tcc, "XAdmin_PluginSetGlobalData");
	if (!inst->desc || !inst->setGlobal) {
		snprintf(sError, iErrorSize, "descriptor symbols missing");
		goto failed;
	}
	if (inst->desc->abi_version != XADMIN_ABI_VERSION) {
		snprintf(sError, iErrorSize, "abi mismatch: %u", inst->desc->abi_version);
		goto failed;
	}
	inst->tcc = tcc;
	bOk = true;
failed:
	if (!bOk) xsDestroyTCC(tcc);
	return bOk;
}

static bool Plugin_Start(PluginInstance* inst, char* sError, size_t iErrorSize)
{
	void* handle = inst;
	sqlite3_stmt* stmt = NULL;
	int gen = 1;

	if (inst->started) return true;
	/* 确保数据与私有库目录存在（v1 插件在 OnStart/OnInstall 中直接写文件/开库）。 */
	xrtDirCreateAll(inst->dataPath);
	{
		char* sDbDir = xrtPathJoin(xrtPathJoin(AppPath, "db"), xrtFormat("plugin/%s", inst->xid));
		if (sDbDir) { xrtDirCreateAll(sDbDir); xrtFree(sDbDir); }
	}
	if (!Plugin_Compile(inst, sError, iErrorSize)) return false;
	inst->config = Plugin_LoadConfig(inst);
	if (!inst->config) inst->config = ValueObject();
	inst->activeLeases = 0;

	/* 代际号与台账行 */
	if (sqlite3_prepare_v2(G_DB, "SELECT MAX(generation) FROM plugin_generation WHERE xid=?;", -1, &stmt, NULL) == SQLITE_OK) {
		Plugin_BindText(stmt, 1, inst->xid);
		if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL)
			gen = (int)sqlite3_column_int64(stmt, 0) + 1;
		sqlite3_finalize(stmt);
	}
	inst->generation = gen;
	inst->genRowId = 0;
	if (sqlite3_prepare_v2(G_DB,
		"INSERT INTO plugin_generation (instance_id, generation, package_version, state, compile_hash, load_time, start_time, stop_time, health_status, error_message, xid) "
		"VALUES (NULL, ?, ?, 'active', ?, ?, ?, NULL, '', '', ?);", -1, &stmt, NULL) == SQLITE_OK) {
		char* sVersion = ValueText(inst->manifest, "version");
		sqlite3_bind_int(stmt, 1, gen);
		Plugin_BindText(stmt, 2, sVersion ? sVersion : "");
		Plugin_BindText(stmt, 3, xrtFormat("%s:%d", sVersion ? sVersion : "", gen));
		sqlite3_bind_int64(stmt, 4, xrtNow());
		sqlite3_bind_int64(stmt, 5, xrtNow());
		Plugin_BindText(stmt, 6, inst->xid);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		inst->genRowId = Plugin_LastRow();
	}

	/* 生命周期：OnLoad → 全局注入 → 首次 OnInstall → OnConfigChanged → OnStart */
	inst->started = true; /* 注册函数在此之前须可用 */
	G_PluginRegIdx = (int)(inst - G_Plugins);
	if (inst->desc->OnLoad && inst->desc->OnLoad(&handle) != 0) {
		snprintf(sError, iErrorSize, "OnLoad failed");
		goto rollback;
	}
	inst->setGlobal(XADMIN_GLOBAL_MAIN_DB, G_DB);
	inst->setGlobal(XADMIN_GLOBAL_OPTION_TABLE, inst->config);
	inst->setGlobal(XADMIN_GLOBAL_PLUGIN_XID, inst->xid);
	inst->setGlobal(XADMIN_GLOBAL_PLUGIN_ROOT_PATH, inst->rootPath);
	inst->setGlobal(XADMIN_GLOBAL_PLUGIN_DATA_PATH, inst->dataPath);
	inst->setGlobal(XADMIN_GLOBAL_PLUGIN_PRIVATE_DB_PATH, inst->dbPath);
	{
		sqlite3_stmt* rt = NULL;
		bool bInstalled = false;
		if (sqlite3_prepare_v2(G_DB, "SELECT installed FROM plugin_runtime WHERE xid=?;", -1, &rt, NULL) == SQLITE_OK) {
			Plugin_BindText(rt, 1, inst->xid);
			if (sqlite3_step(rt) == SQLITE_ROW) bInstalled = sqlite3_column_int(rt, 0) != 0;
			sqlite3_finalize(rt);
		}
		if (!bInstalled && inst->desc->OnInstall) {
			if (inst->desc->OnInstall(handle) != 0) {
				snprintf(sError, iErrorSize, "OnInstall failed");
				goto rollback;
			}
			Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET installed=1 WHERE xid='%s';", inst->xid));
		}
	}
	if (inst->desc->OnConfigChanged) inst->desc->OnConfigChanged(handle, inst->config);
	if (inst->desc->OnStart && inst->desc->OnStart(handle) != 0) {
		snprintf(sError, iErrorSize, "OnStart failed");
		goto rollback;
	}
	G_PluginRegIdx = -1;
	Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET status='running', active_generation=%d, update_time=%lld WHERE xid='%s';", gen, xrtNow(), inst->xid));
	printf("[plugin] started %s (generation %d)\n", inst->xid, gen);
	return true;
rollback:
	G_PluginRegIdx = -1;
	/* GR2 同 Plugin_Stop：回调前关门；GR4 路由兜底出表。 */
	inst->started = false;
	if (inst->desc && inst->desc->OnStop) inst->desc->OnStop(handle);
	Plugin_CleanupResources(inst);
	if (inst->desc && inst->desc->OnUnload) inst->desc->OnUnload(handle);
	{
		size_t k;
		for (k = 0; k < inst->routeCount; k++) {
			xrtMapRemove(G_StaticRouteTableHTTP, KeyView(inst->routePaths[k]));
			xrtFree((void*)inst->routePaths[k]);
		}
		inst->routeCount = 0;
	}
	inst->desc = NULL;
	inst->setGlobal = NULL;
	xsDestroyTCC(inst->tcc);
	inst->tcc = NULL;
	CacheRetire(inst->config);
	inst->config = NULL;
	if (inst->genRowId)
		Plugin_Exec(xrtFormat("UPDATE plugin_generation SET state='stopped', stop_time=%lld, error_message='start failed' WHERE id=%lld;", xrtNow(), inst->genRowId));
	Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET status='error' WHERE xid='%s';", inst->xid));
	return false;
}

static void Plugin_Stop(PluginInstance* inst)
{
	const XAdminPluginDescriptor* desc;
	size_t i;
	if (!inst->started) return;
	desc = inst->desc;
	/* GR2：回调期间先关门——Register* 被 started 门控拒绝，Unregister* 不受影响。 */
	inst->started = false;
	if (desc && desc->OnStop) desc->OnStop(inst);
	Plugin_CleanupResources(inst);
	if (desc && desc->OnUnload) desc->OnUnload(inst);
	/* GR4：路由按内存权威清单兜底出表（台账漏登时不留悬垂 Proc/Path）。 */
	for (i = 0; i < inst->routeCount; i++) {
		xrtMapRemove(G_StaticRouteTableHTTP, KeyView(inst->routePaths[i]));
		xrtFree((void*)inst->routePaths[i]);
	}
	inst->routeCount = 0;
	inst->desc = NULL;
	inst->setGlobal = NULL;
	/* GR1：有未释放服务租借时不销毁代码镜像，压入退役队列等 ReleaseService 回收。 */
	if (inst->tcc) {
		if (inst->activeLeases > 0) {
			int k, slot = -1;
			for (k = 0; k < PLUGIN_TCC_RETIRE_MAX; k++)
				if (!G_PluginTccRetired[k].used) { slot = k; break; }
			if (slot >= 0) {
				G_PluginTccRetired[slot].used = true;
				G_PluginTccRetired[slot].tcc = inst->tcc;
				G_PluginTccRetired[slot].leases = inst->activeLeases;
			} else {
				/* 队列满退化为泄漏代码镜像而非悬垂指针。 */
				printf("[plugin][warn] tcc retire queue full; leak image of %s (leases=%d)\n",
					inst->xid, inst->activeLeases);
			}
			inst->activeLeases = 0;
			inst->tcc = NULL;
		} else {
			xsDestroyTCC(inst->tcc);
			inst->tcc = NULL;
		}
	}
	CacheRetire(inst->config);
	inst->config = NULL;
	if (inst->genRowId)
		Plugin_Exec(xrtFormat("UPDATE plugin_generation SET state='stopped', stop_time=%lld WHERE id=%lld;", xrtNow(), inst->genRowId));
	inst->genRowId = 0;
	Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET status='disabled', active_generation=0, update_time=%lld WHERE xid='%s';", xrtNow(), inst->xid));
	printf("[plugin] stopped %s\n", inst->xid);
}

/* ==================== 管理操作（供路由与插件 API） ==================== */

static bool PluginHost_SetEnabled(const char* sXid, bool bEnable)
{
	PluginInstance* inst = Plugin_Find(sXid);
	char sError[256] = {0};
	if (!inst) return false;
	if (G_PluginRegIdx >= 0) return false; /* GR3：插件启动期间禁止换代操作（防重入） */
	if (bEnable) {
		if (inst->started) return true;
		Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET enabled=1, update_time=%lld WHERE xid='%s';", xrtNow(), sXid));
		if (!Plugin_Start(inst, sError, sizeof(sError))) {
			printf("[plugin] enable failed %s: %s\n", sXid, sError);
			Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET status='error' WHERE xid='%s';", sXid));
			return false;
		}
		return true;
	}
	Plugin_Stop(inst);
	Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET enabled=0, update_time=%lld WHERE xid='%s';", xrtNow(), sXid));
	return true;
}

static bool PluginHost_Reload(const char* sXid)
{
	PluginInstance* inst = Plugin_Find(sXid);
	if (!inst) return false;
	if (G_PluginRegIdx >= 0) return false; /* GR3：同上 */
	if (inst->started) Plugin_Stop(inst);
	Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET enabled=1 WHERE xid='%s';", sXid));
	{
		char sError[256] = {0};
		if (!Plugin_Start(inst, sError, sizeof(sError))) {
			printf("[plugin] reload failed %s: %s\n", sXid, sError);
			Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET status='error' WHERE xid='%s';", sXid));
			return false;
		}
	}
	return true;
}

/* ==================== 扫描与初始化 ==================== */

static int Plugin_ScanProc(const char* sPath, size_t iSize, bool bDir, void* pParam)
{
	(void)iSize;
	if (bDir) {
		/* DirScan 非递归只回调顶层条目；目录名即候选 xid */
		const char* sName = sPath + strlen(sPath);
		while (sName > sPath && sName[-1] != '/' && sName[-1] != '\\') sName--;
		if (Plugin_XidValid(sName) && G_PluginCount < PLUGIN_MAX) {
			char* sManifestPath = xrtPathJoin(sPath, "plugin.json");
			xvalue* manifest = sManifestPath ? Plugin_LoadFlatJson(sManifestPath) : NULL;
			if (manifest) {
				PluginInstance* inst = &G_Plugins[G_PluginCount++];
				xvalue* tblBuild;
				memset(inst, 0, sizeof(*inst));
				snprintf(inst->xid, sizeof(inst->xid), "%s", sName);
				snprintf(inst->rootPath, sizeof(inst->rootPath), "%s", sPath);
				snprintf(inst->dataPath, sizeof(inst->dataPath), "%s", xrtPathJoin(xrtPathJoin(AppPath, "plugin_data"), sName));
				snprintf(inst->dbPath, sizeof(inst->dbPath), "%s", xrtPathJoin(xrtPathJoin(xrtPathJoin(AppPath, "db"), "plugin"), xrtFormat("%s/plugin.db", sName)));
				inst->manifest = manifest;
				inst->generation = 0;
				/* 基本校验：formatVersion 4、ABI<=4、entry 存在 */
				tblBuild = ValueGet(manifest, "build");
				if (ValueInt(manifest, "formatVersion") != 4 ||
				    ValueInt(ValueGet(manifest, "compat"), "abiVersion") > XADMIN_ABI_VERSION ||
				    !tblBuild || !ValueText(tblBuild, "entry")) {
					printf("[plugin][warn] manifest invalid, skipped: %s\n", sName);
					G_PluginCount--; /* 保留 slot 0 占用？——回退计数即可（末尾元素） */
				}
			}
			if (sManifestPath) xrtFree(sManifestPath);
		}
	}
	return 0;
}

static void Plugin_SyncDatabase(void)
{
	size_t i;
	sqlite3_stmt* stmt = NULL;
	for (i = 0; i < G_PluginCount; i++) {
		PluginInstance* inst = &G_Plugins[i];
		char* sManifestText = NULL;
		bool bExists = false;
		if (sqlite3_prepare_v2(G_DB, "SELECT 1 FROM plugin_runtime WHERE xid=?;", -1, &stmt, NULL) == SQLITE_OK) {
			Plugin_BindText(stmt, 1, inst->xid);
			if (sqlite3_step(stmt) == SQLITE_ROW) bExists = true;
			sqlite3_finalize(stmt);
		}
		{
			size_t size = 0;
			char* sPath = xrtPathJoin(inst->rootPath, "plugin.json");
			bytes data = (bytes)xrtFileReadAll(sPath, &size);
			if (data) {
				sManifestText = xrtMalloc(size + 1);
				memcpy(sManifestText, data, size);
				sManifestText[size] = '\0';
				xrtFree(data);
			}
			xrtFree(sPath);
		}
		if (!bExists) {
			if (sqlite3_prepare_v2(G_DB,
				"INSERT INTO plugin_package (package_id, plugin_id, version, source_type, install_path, checksum, signature, trust_level, manifest_json, install_time, xid) "
				"VALUES (?, ?, ?, 'local', ?, '', '', 'system', ?, ?, ?);", -1, &stmt, NULL) == SQLITE_OK) {
				str sVersion = ValueText(inst->manifest, "version");
				Plugin_BindText(stmt, 1, inst->xid);
				Plugin_BindText(stmt, 2, inst->xid);
				Plugin_BindText(stmt, 3, sVersion ? sVersion : "0.0.0");
				Plugin_BindText(stmt, 4, inst->rootPath);
				Plugin_BindText(stmt, 5, sManifestText ? sManifestText : "");
				sqlite3_bind_int64(stmt, 6, xrtNow());
				Plugin_BindText(stmt, 7, inst->xid);
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);
			}
			if (sqlite3_prepare_v2(G_DB,
				"INSERT INTO plugin_runtime (package_id, xid, mount_path, data_path, private_db_path, enabled, installed, config_json, status, active_generation, create_time, update_time) "
				"VALUES (?, ?, '', ?, ?, 0, 0, '', 'discovered', 0, ?, ?);", -1, &stmt, NULL) == SQLITE_OK) {
				Plugin_BindText(stmt, 1, inst->xid);
				Plugin_BindText(stmt, 2, inst->xid);
				Plugin_BindText(stmt, 3, inst->dataPath);
				Plugin_BindText(stmt, 4, inst->dbPath);
				sqlite3_bind_int64(stmt, 5, xrtNow());
				sqlite3_bind_int64(stmt, 6, xrtNow());
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);
			}
			printf("[plugin] discovered %s\n", inst->xid);
		} else {
			/* 路径与 manifest 刷新（幂等） */
			if (sManifestText)
				Plugin_Exec(xrtFormat("UPDATE plugin_package SET install_path='%s', manifest_json='%s' WHERE xid='%s';",
					inst->rootPath, sManifestText, inst->xid));
			Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET data_path='%s', private_db_path='%s' WHERE xid='%s';",
				inst->dataPath, inst->dbPath, inst->xid));
		}
		if (sManifestText) xrtFree(sManifestText);
	}
}

static void PluginHost_Init(void)
{
	char* sPluginRoot;
	printf("        PluginHost_Init \n");
	memset(G_PluginEvents, 0, sizeof(G_PluginEvents));
	memset(G_PluginHooks, 0, sizeof(G_PluginHooks));
	memset(G_PluginServices, 0, sizeof(G_PluginServices));

	/* 两代共同清理：上次运行的 active 代际在本次启动前全部视为 stopped。 */
	Plugin_Exec(xrtFormat("UPDATE plugin_generation SET state='stopped', stop_time=%lld WHERE state='active';", xrtNow()));

	Plugin_ConvertLegacyPaths();
	sPluginRoot = xrtPathJoin(AppPath, "plugin");
	if (sPluginRoot && xrtDirExists(sPluginRoot))
		DirScan(sPluginRoot, false, Plugin_ScanProc, NULL);
	if (sPluginRoot) xrtFree(sPluginRoot);
	Plugin_SyncDatabase();

	/* 启动 enabled=1 的插件（依赖检查：manifest dependencies.plugins 须已启用） */
	{
		sqlite3_stmt* stmt = NULL;
		if (sqlite3_prepare_v2(G_DB, "SELECT xid FROM plugin_runtime WHERE enabled=1;", -1, &stmt, NULL) == SQLITE_OK) {
			while (sqlite3_step(stmt) == SQLITE_ROW) {
				const char* sXid = (const char*)sqlite3_column_text(stmt, 0);
				PluginInstance* inst = Plugin_Find(sXid);
				char sError[256] = {0};
				if (!inst) continue;
				{
					xvalue* arrDeps = ValueGet(
						ValueGet(inst->manifest, "dependencies"), "plugins");
					bool bMissing = false;
					if (arrDeps && xrtValueType(arrDeps) == XVALUE_ARRAY)
						for (uint32 d = 0; d < ValueCount(arrDeps); d++) {
							str sDep = ValueArrayText(arrDeps, d);
							sqlite3_stmt* chk = NULL;
							int bOn = 0;
							if (sqlite3_prepare_v2(G_DB, "SELECT enabled FROM plugin_runtime WHERE xid=?;", -1, &chk, NULL) == SQLITE_OK) {
								Plugin_BindText(chk, 1, sDep);
								if (sqlite3_step(chk) == SQLITE_ROW) bOn = sqlite3_column_int(chk, 0);
								sqlite3_finalize(chk);
							}
							if (!bOn) {
								printf("[plugin] dependency not enabled: %s needs %s\n", sXid, sDep);
								bMissing = true;
								break;
							}
						}
					if (bMissing) continue;
				}
				if (!Plugin_Start(inst, sError, sizeof(sError)))
					printf("[plugin] start failed %s: %s\n", sXid, sError);
			}
			sqlite3_finalize(stmt);
		}
	}
}

static void PluginHost_Unit(void)
{
	size_t i;
	printf("        PluginHost_Unit \n");
	for (i = 0; i < G_PluginCount; i++)
		Plugin_Stop(&G_Plugins[i]);
	for (i = 0; i < G_PluginCount; i++) {
		xrtValueRelease(G_Plugins[i].manifest);
	}
	G_PluginCount = 0;
}
