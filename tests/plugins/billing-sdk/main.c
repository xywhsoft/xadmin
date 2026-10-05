#include <plugin_support.h>
#include <billing_service.h>
static XAdminPluginHandle Handle;
XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int index,void* value){(void)index;(void)value;}
static void Route(XS_ServerObject s,XS_HostObject h,XS_RequestObject req,XS_ResponseObject resp,xvalue* session)
{
    (void)s;(void)h;if(!XP_Member(req,resp,session,XHTTP_METHOD_POST))return;
    XAdminServiceLease lease=NULL;const void* table=NULL;
    if(XAdmin_AcquireService(Handle,XADMIN_BILLING_SERVICE,1,&lease,&table)){XP_Reply(resp,503,"No billing",NULL);return;}
    const XBillingService* b=table;xvalue* body=XP_Body(req,8192);const char* action=ValueText(body,"action"),*id=ValueText(body,"request_id");int status=400;
    if(action && !strcmp(action,"reserve")){
        XBillingReservation r={sizeof(r),ValueInt(session,"id"),ValueInt(body,"amount"),(int64_t)time(NULL)+3600,id,"fixture","test"};status=b->reserve(&r);
    }else if(action && !strcmp(action,"finalize")){
        XBillingSettlement r={sizeof(r),id,ValueInt(body,"amount"),"{\"input_tokens\":2}",ValueText(body,"outcome")};status=b->finalize(&r);
    }
    XAdmin_ReleaseService(lease);xrtValueRelease(body);XP_Reply(resp,status?status:200,status?"Rejected":"",NULL);
}
static int Start(XAdminPluginHandle h){Handle=h;XAdminRouteDecl r={"/__test/billing",Route,false,false,0,0};return XAdmin_RegisterRoute(h,&r,NULL);}
static XAdminPluginDescriptor Descriptor={XADMIN_ABI_VERSION,sizeof(XAdminPluginDescriptor),"billing-sdk","1.0.0","Billing fixture",NULL,NULL,Start,NULL,NULL,NULL,NULL};
XADMIN_DECLARE_PLUGIN(Descriptor)
