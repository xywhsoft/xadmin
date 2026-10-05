/* Generic prepaid account service. No model/provider/site special cases. */
#include <plugin_support.h>
#include <billing_service.h>
#include <stdlib.h>
static XAdminPluginHandle G_Handle;
static XAdminHostContext* G_Host;
static sqlite3* G_DB;
static int G_PendingSeconds=3600;
static bool Billing_Maintain(void);
static bool Billing_Periods(void);
XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int index,void* value)
{if(index==XADMIN_GLOBAL_HOST_CONTEXT)G_Host=value;}
#include "src/storage.c"
#include "src/ledger.c"
#include "src/subscriptions.c"
#include "src/routes.c"
static int Billing_ServiceAccount(int64_t owner,XBillingAccount* out)
{return Billing_Maintain() && Billing_Periods()?Billing_ReadAccount(owner,out):XBILL_UNAVAILABLE;}
static xvalue* Billing_Transactions(int64_t owner,int64_t offset,int limit)
{
    if(owner<1 || offset<0 || limit<1 || limit>100)return NULL;
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT id,kind,amount AS amount_micros,amount/10000 AS amount,source,request_id,reason AS remark,actor AS operator,created_at AS createTime FROM ledger WHERE member_id=? ORDER BY id DESC LIMIT ? OFFSET ?");
    if(s){sqlite3_bind_int64(s,1,owner);sqlite3_bind_int(s,2,limit);sqlite3_bind_int64(s,3,offset);}return XP_Rows(s);
}
static int Billing_ServiceMaintain(void){return Billing_Maintain() && Billing_Periods()?0:XBILL_UNAVAILABLE;}
static xvalue* Billing_Changes(const char* service,int64_t after,int limit)
{
    if(!XP_Id(service,64) || after<0 || limit<1 || limit>100)return NULL;
    /* Cursor addresses immutable changes; return the latest financial truth so
     * replaying an older settlement cannot undo a later refund. */
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT c.sequence,c.request_id,r.member_id,r.charged,r.state FROM receipt_change c JOIN reservation r ON r.request_id=c.request_id WHERE c.service=? AND c.sequence>? ORDER BY c.sequence LIMIT ?");
    XP_Bind(s,1,service);if(s){sqlite3_bind_int64(s,2,after);sqlite3_bind_int(s,3,limit);}return XP_Rows(s);
}
static XBillingService G_Service={sizeof(XBillingService),1,Billing_ServiceAccount,Billing_Entitlement,Billing_Reserve,Billing_Finalize,Billing_Lookup,Billing_AdjustCash,Billing_Refund,Billing_Transactions,Billing_ServiceMaintain,Billing_Changes};
static int Billing_Start(XAdminPluginHandle handle)
{
    G_Handle=handle;if(!G_Host || !G_Host->main_db || !G_Host->plugin_private_db_path)return -1;
    if(sqlite3_open(G_Host->plugin_private_db_path,&G_DB)!=SQLITE_OK)return -1;sqlite3_busy_timeout(G_DB,1000);
    if(!Billing_Schema() || !Billing_Migrate() || !Billing_Maintain() || !Billing_Periods())return -1;
    XAdminServiceDecl service={XADMIN_BILLING_SERVICE,1,0,"billing",0,sizeof(G_Service),""};
    if(XAdmin_RegisterService(handle,&service,&G_Service))return -1;
    int group_id=0,auth_id=0;XAdminAuthGroupDecl group={XADMIN_AUTH_SCOPE_ADMIN,"billing","账户计费","账户与会员服务",990020};
    XAdminAuthDecl auth={XADMIN_AUTH_SCOPE_ADMIN,"billing.manage",0,"管理账户计费","管理余额、会员、额度及退款",990020};
    if(XAdmin_RegisterAuthGroup(handle,&group,&group_id,NULL))return -1;auth.group_id=group_id;
    if(XAdmin_RegisterAuth(handle,&auth,&auth_id,NULL))return -1;
    XAdminRouteDecl routes[]={
        {"/api/v1/billing/account",Billing_Public,false,false,0,0},
        {"/api/v1/billing/transactions",Billing_Public,false,false,0,0},
        {"/api/v1/billing/credits",Billing_Public,false,false,0,0},
        {"/api/v1/billing/plans",Billing_Public,false,false,0,0},
        {"/api/v1/billing/subscriptions",Billing_Public,false,false,0,0},
        {"/account/billing",Billing_Page,false,false,0,0},
        {"/admin/billing",Billing_Page,true,true,auth_id,0},
        {"/admin/billing/state",Billing_AdminAPI,true,true,auth_id,0},
        {"/admin/billing/adjust",Billing_AdminAPI,true,true,auth_id,0},
        {"/admin/billing/grant",Billing_AdminAPI,true,true,auth_id,0},
        {"/admin/billing/plan",Billing_AdminAPI,true,true,auth_id,0},
        {"/admin/billing/subscribe",Billing_AdminAPI,true,true,auth_id,0},
        {"/admin/billing/cancel",Billing_AdminAPI,true,true,auth_id,0},
        {"/admin/billing/refund",Billing_AdminAPI,true,true,auth_id,0},
        {"/admin/billing/resolve",Billing_AdminAPI,true,true,auth_id,0}
    };
    size_t i;for(i=0;i<sizeof(routes)/sizeof(routes[0]);i++)if(XAdmin_RegisterRoute(handle,&routes[i],NULL))return -1;
    XAdminMenuDecl menu={0};menu.key="billing.accounts";menu.title="账户与计费";menu.icon="layui-icon layui-icon-rmb";
    menu.type=1;menu.open_type="_iframe";menu.href="/admin/billing";menu.sort=990020;menu.visible=true;
    return XAdmin_RegisterMenu(handle,&menu,NULL,NULL);
}
static void Billing_Stop(XAdminPluginHandle handle)
{(void)handle;if(G_DB)sqlite3_close(G_DB);G_DB=NULL;}
static int Billing_Health(XAdminPluginHandle handle,XAdminHealthReport* out)
{(void)handle;if(out){out->status_code=G_DB?0:1;out->message=G_DB?"ready":"database unavailable";}return 0;}
static int Billing_Config(XAdminPluginHandle handle,xvalue* config)
{
    (void)handle;const char* names[]={"currency","pending_timeout_seconds"};int64_t seconds;
    if(!XP_Fields(config,names,2) || !XP_Text(config,"currency",3) || strcmp(ValueText(config,"currency"),"CNY") ||
        !XP_Int(config,"pending_timeout_seconds",600,86400,&seconds))return -1;
    G_PendingSeconds=(int)seconds;return 0;
}
static XAdminPluginDescriptor G_Descriptor={XADMIN_ABI_VERSION,sizeof(XAdminPluginDescriptor),"billing","1.0.0","账户与计费",NULL,NULL,Billing_Start,Billing_Config,Billing_Health,Billing_Stop,NULL};
XADMIN_DECLARE_PLUGIN(G_Descriptor)
