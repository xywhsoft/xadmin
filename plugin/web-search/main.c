/* Web search proxy. Plugin state is touched under the host request lock.
 * Outbound calls use owned snapshots; no statement/transaction crosses I/O. */
#include <xs_plugin.h>
#include <value_util.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static XAdminPluginHandle G_Handle;
static XAdminHostContext* G_Host;
static sqlite3* G_DB;
static char* G_CredentialPath;
typedef struct SearchConfig {
    char provider[16], verification[16];
    int minute_limit, daily_limit, global_daily_limit, max_results, timeout_ms, max_concurrent;
    bool bocha_enabled, zai_enabled;
} SearchConfig;
static SearchConfig G_Config;
static int64 G_Active[16];

XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int index, void* value)
{ if (index == XADMIN_GLOBAL_HOST_CONTEXT) G_Host = value; }

/* Embedded NUL strings must never be accepted via a C-string truncation. */
static const char* Search_Text(const xvalue* object, const char* key, size_t max)
{
    xstrview view = {0};
    if (!xrtValueGetString(ValueGet(object, key), &view) || view.Size > max ||
        memchr(view.Data, 0, view.Size) || !xrtUtf8Valid(view,NULL)) return NULL;
    return view.Data;
}
static bool Search_Fields(const xvalue* object, const char* const* allowed, size_t count)
{
    if (!object || xrtValueType(object) != XVALUE_OBJECT) return false;
    xvalueiter it = {0}; xvaluekey key; xvalue* value; bool ok = true;
    if (xrtValueIterBegin(object, &it)) {
        while ((value = xrtValueIterNext(&it, &key))) {
            size_t i; for (i = 0; i < count; i++)
                if (key.String.Size == strlen(allowed[i]) && !memcmp(key.String.Data, allowed[i], key.String.Size)) break;
            if (i == count) { ok = false; break; }
        }
        xrtValueIterEnd(&it);
    }
    return ok;
}
static xvalue* Search_Body(XS_RequestObject req, size_t limit)
{
    size_t n = XAdmin_ReqBodyLen(req); const char* body = XAdmin_ReqBody(req);
    if (!body || !n || n > limit || memchr(body, 0, n)) return NULL;
    return xrtJsonParse(xrtStrViewN(body, n));
}
static void Search_Reply(XS_ResponseObject resp, int status, const char* message, xvalue* data)
{
    xvalue* result = ValueObject(); size_t n = 0;
    ValueSetInt(result, "code", status < 400 ? 0 : status);
    ValueSetText(result, "message", message ? message : "");
    if (data) ValueSetOwn(result, "data", data);
    char* json = xrtJsonStringify(result, false, &n);
    xsHttpReplyAuto(resp, status, "Content-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\n", json ? json : "{}", json ? n : 2);
    xrtFree(json); xrtValueRelease(result);
}

#include "src/config.c"
#include "src/providers.c"
#include "src/quota.c"
#include "src/service.c"
#include "src/admin.c"

static int Search_Load(XAdminPluginHandle* handle)
{ G_Handle = handle ? *handle : NULL; return G_Handle ? 0 : -1; }
static void Search_Stop(XAdminPluginHandle handle)
{
    (void)handle; sqlite3_close(G_DB); G_DB = NULL;
    xrtFree(G_CredentialPath); G_CredentialPath = NULL;
    memset(G_Active, 0, sizeof(G_Active));
}
static int Search_Start(XAdminPluginHandle handle)
{
    G_Handle = handle;
    if (!G_Host || !G_Host->plugin_private_db_path || !G_Host->plugin_data_path) return -1;
#if !defined(_WIN32) && !defined(_WIN64)
    /* Keys and temporary atomic-write files are readable only by this service. */
    if (!xrtPathSetMode(G_Host->plugin_data_path,false,0700)) return -1;
#endif
    G_CredentialPath = xrtPathJoin(G_Host->plugin_data_path, "credentials.json");
    if (!G_CredentialPath || sqlite3_open(G_Host->plugin_private_db_path, &G_DB) != SQLITE_OK) return -1;
    sqlite3_busy_timeout(G_DB, 1000);
    if (sqlite3_exec(G_DB, "CREATE TABLE IF NOT EXISTS search_budget(owner INTEGER NOT NULL,kind INTEGER NOT NULL,window INTEGER NOT NULL,used INTEGER NOT NULL CHECK(used>=0),PRIMARY KEY(owner,kind,window));PRAGMA user_version=1;", NULL, NULL, NULL) != SQLITE_OK) return -1;
    int group_id = 0, auth_id = 0;
    XAdminAuthGroupDecl group = {XADMIN_AUTH_SCOPE_ADMIN,"web-search","联网搜索","联网搜索服务管理",990010};
    if (XAdmin_RegisterAuthGroup(handle,&group,&group_id,NULL)) return -1;
    XAdminAuthDecl auth = {XADMIN_AUTH_SCOPE_ADMIN,"web-search.manage",group_id,"管理联网搜索","查看和维护搜索服务密钥",990010};
    if (XAdmin_RegisterAuth(handle,&auth,&auth_id,NULL)) return -1;
    XAdminRouteDecl routes[] = {
        {"/api/v1/search", Search_Request, false, false, 0, 0},
        {"/api/v1/search/providers", Search_Providers, false, false, 0, 0},
        {"/api/v1/search/usage", Search_Usage, false, false, 0, 0},
        {"/admin/web-search", Search_Admin, true, true, auth_id, 0},
        {"/admin/web-search/credentials", Search_Credentials, true, true, auth_id, 0}
    };
    size_t i; for (i = 0; i < sizeof(routes)/sizeof(routes[0]); i++)
        if (XAdmin_RegisterRoute(handle, &routes[i], NULL)) return -1;
    XAdminMenuDecl menu = {0};
    menu.key = "web-search.settings"; menu.title = "联网搜索";
    menu.icon = "layui-icon layui-icon-search"; menu.type = 1;
    menu.open_type = "_iframe"; menu.href = "/admin/web-search";
    menu.sort = 990010; menu.visible = true; menu.remark = "博查 / z.ai 搜索代理";
    return XAdmin_RegisterMenu(handle, &menu, NULL, NULL);
}
static int Search_Health(XAdminPluginHandle handle, XAdminHealthReport* report)
{
    (void)handle; char key[1001]; bool ready = false;
    if (G_CredentialPath) {
        ready = (G_Config.bocha_enabled && Search_Key("bocha", key)) ||
            (G_Config.zai_enabled && Search_Key("zai", key));
        xrtSecureZero(key, sizeof(key));
    }
    if (report) { report->status_code = ready ? 0 : 1; report->message = ready ? "ready" : "provider credentials required"; }
    return 0;
}
static XAdminPluginDescriptor G_Descriptor = {
    XADMIN_ABI_VERSION, sizeof(XAdminPluginDescriptor), "web-search", "1.0.0", "联网搜索代理",
    Search_Load, NULL, Search_Start, Search_ConfigChanged, Search_Health, Search_Stop, NULL
};
XADMIN_DECLARE_PLUGIN(G_Descriptor)
