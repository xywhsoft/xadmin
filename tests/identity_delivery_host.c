/* An isolated test composition: production main.c has no mock switch or code
 * echo. This callback only exists in the test host used by the Python runner. */
#define ServiceInit XAdmin_ServiceInit
#define ServiceUnit XAdmin_ServiceUnit
#include "../main.c"
#undef ServiceInit
#undef ServiceUnit
static xvalue* TestDeliveries;
static XAIdentityDeliveryResult TestDelivery(const XAIdentityMessage* message,void* context)
{
    (void)context;
    xrtMutexLock(G_RequestLock);
    xvalue* v=ValueObject();ValueSetText(v,"code",message->code);ValueSetText(v,"target",message->target);ValueSetText(v,"purpose",message->purpose);
    ValueSetOwn(TestDeliveries,message->challenge_id,v);xrtMutexUnlock(G_RequestLock);return XA_DELIVERY_SENT;
}
static void DeliveryRead(XS_ServerObject server,XS_HostObject host,XAdminRequest* req,XAdminRequest* resp,xvalue* session)
{
    (void)server;(void)host;(void)resp;(void)session;xstrview id={0};xsReqRouteValue(req,"id",&id);
    xvalue* v=xrtValueObjectGet(TestDeliveries,id);XA_Reply(req,v?200:404,v?"success":"not delivered",v,NULL);
}
void ServiceInit(XS_HostInfo* host)
{
    XAdmin_ServiceInit(host);if(!G_Ready)return;G_Ready=false;TestDeliveries=ValueObject();
    XA_SetIdentityDelivery(TestDelivery,NULL);
    RouteInfo* route=AddDynamicRouteHTTP("/__test/identity/delivery/{id}",XHTTP_METHOD_GET,DeliveryRead,true);
    if(route){route->bAdmin=false;route->bAuth=false;}
    G_Ready=RouteHTTP_Compile();
}
void ServiceUnit(XS_HostInfo* host)
{
    xrtValueRelease(TestDeliveries);TestDeliveries=NULL;XAdmin_ServiceUnit(host);
}
