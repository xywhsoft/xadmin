/* Native model proxy. Model execution remains upstream; agent/tools stay in
 * the client. Billing is a separate generic service, not a second cash store. */
#include <plugin_support.h>
#include <billing_service.h>
#include <stdlib.h>
#include "include/gateway_types.h"
static XAdminPluginHandle G_Handle;
static XAdminHostContext* G_Host;
static sqlite3* G_DB;
static char* G_CredentialPath;
static GatewayConfig G_Config;
static GatewayActive G_Active[16];
XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int index,void* value)
{if(index==XADMIN_GLOBAL_HOST_CONTEXT)G_Host=value;}
static bool Gateway_Recover(void);
#include "src/storage.c"
#include "src/credentials.c"
#include "src/catalog.c"
#include "src/metering.c"
#include "src/policy.c"
#include "src/requests.c"
#include "src/proxy.c"
#include "src/routes.c"
#include "src/admin.c"
static int Gateway_Config(XAdminPluginHandle handle,xvalue* value)
{
    (void)handle;const char* names[]={"max_concurrent","minute_limit","daily_limit","global_daily_limit","daily_budget_micros"};
    int64_t a,b,c,d,e;
    if(!XP_Fields(value,names,5) || !XP_Int(value,names[0],1,16,&a) || !XP_Int(value,names[1],1,1000,&b) ||
        !XP_Int(value,names[2],1,100000,&c) || !XP_Int(value,names[3],1,1000000,&d) || !XP_Int(value,names[4],1,1000000000000LL,&e))return -1;
    G_Config=(GatewayConfig){(int)a,(int)b,(int)c,(int)d,e};return 0;
}
static int Gateway_Start(XAdminPluginHandle handle)
{
    G_Handle=handle;if(!G_Host || !G_Host->plugin_private_db_path || !G_Host->plugin_data_path)return -1;
    if(sqlite3_open(G_Host->plugin_private_db_path,&G_DB)!=SQLITE_OK)return -1;sqlite3_busy_timeout(G_DB,1000);
    if(!Gateway_Schema())return -1;
#if !defined(_WIN32) && !defined(_WIN64)
    if(!xrtPathSetMode(G_Host->plugin_data_path,false,0700))return -1;
#endif
    G_CredentialPath=xrtPathJoin(G_Host->plugin_data_path,"credentials.json");if(!G_CredentialPath || !Gateway_Recover())return -1;
    XAdminEventDecl event={"billing.finalized",Gateway_BillingEvent};if(XAdmin_ListenEvent(handle,&event,NULL))return -1;
    int group_id=0,auth_id=0;XAdminAuthGroupDecl group={XADMIN_AUTH_SCOPE_ADMIN,"model-gateway","模型网关","模型服务管理",990030};
    XAdminAuthDecl auth={XADMIN_AUTH_SCOPE_ADMIN,"model-gateway.manage",0,"管理模型网关","管理模型、渠道、价格与用量",990030};
    if(XAdmin_RegisterAuthGroup(handle,&group,&group_id,NULL))return -1;auth.group_id=group_id;
    if(XAdmin_RegisterAuth(handle,&auth,&auth_id,NULL))return -1;
    XAdminRouteDecl routes[]={
        {"/api/v1/ai/chat/completions",Gateway_Request,false,false,0,0},
        {"/api/v1/ai/responses",Gateway_Request,false,false,0,0},
        {"/api/v1/ai/messages",Gateway_Request,false,false,0,0},
        {"/api/v1/ai/models",Gateway_Models,false,false,0,0},
        {"/api/v1/ai/catalog",Gateway_Models,false,false,0,0},
        {"/api/v1/ai/usage",Gateway_Usage,false,false,0,0},
        {"/account/models",Gateway_Page,false,false,0,0},
        {"/admin/model-gateway",Gateway_Page,true,true,auth_id,0},
        {"/admin/model-gateway/state",Gateway_Admin,true,true,auth_id,0},
        {"/admin/model-gateway/channel",Gateway_Admin,true,true,auth_id,0},
        {"/admin/model-gateway/model",Gateway_Admin,true,true,auth_id,0},
        {"/admin/model-gateway/credentials",Gateway_Admin,true,true,auth_id,0}
    };
    size_t i;for(i=0;i<sizeof(routes)/sizeof(routes[0]);i++)if(XAdmin_RegisterRoute(handle,&routes[i],NULL))return -1;
    XAdminDynamicRouteDecl detail={0};detail.path="model-gateway.request";detail.pattern="/api/v1/ai/requests/{id}";
    detail.proc=Gateway_RequestDetail;detail.method=XHTTP_METHOD_GET;if(XAdmin_RegisterDynamicRoute(handle,&detail,NULL))return -1;
    XAdminMenuDecl menu={0};menu.key="model-gateway.models";menu.title="模型网关";menu.icon="layui-icon layui-icon-dialogue";
    menu.type=1;menu.open_type="_iframe";menu.href="/admin/model-gateway";menu.sort=990030;menu.visible=true;
    return XAdmin_RegisterMenu(handle,&menu,NULL,NULL);
}
static void Gateway_Stop(XAdminPluginHandle handle)
{(void)handle;if(G_DB)sqlite3_close(G_DB);G_DB=NULL;xrtFree(G_CredentialPath);G_CredentialPath=NULL;memset(G_Active,0,sizeof(G_Active));}
static int Gateway_Health(XAdminPluginHandle handle,XAdminHealthReport* report)
{
    (void)handle;XAdminServiceLease lease;const XBillingService* billing=Gateway_Billing(&lease);
    bool ready=G_DB && billing;if(lease)XAdmin_ReleaseService(lease);
    if(report){report->status_code=ready?0:1;report->message=ready?"ready":"billing unavailable";}return 0;
}
static XAdminPluginDescriptor G_Descriptor={XADMIN_ABI_VERSION,sizeof(XAdminPluginDescriptor),"model-gateway","1.0.0","模型网关",NULL,NULL,Gateway_Start,Gateway_Config,Gateway_Health,Gateway_Stop,NULL};
XADMIN_DECLARE_PLUGIN(G_Descriptor)
