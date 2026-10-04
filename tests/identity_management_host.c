/* Functional maintenance hook exists only in this isolated test composition. */
#define ServiceInit XAdmin_ServiceInit
#include "../main.c"
#undef ServiceInit
static void TestMaintain(XS_ServerObject s,XS_HostObject h,XAdminRequest* req,XAdminRequest* resp,xvalue* session)
{
    (void)s;(void)h;(void)resp;(void)session;XA_SessionMaintenance();XA_Reply(req,200,"maintained",NULL,NULL);
}
void ServiceInit(XS_HostInfo* host)
{
    XAdmin_ServiceInit(host);if(!G_Ready)return;G_Ready=false;
    RouteInfo* r=AddStaticRouteHTTP("/__test/identity/maintenance",XHTTP_METHOD_POST,TestMaintain,true);
    if(r){r->bAdmin=false;r->bAuth=false;}G_Ready=RouteHTTP_RecompileDynamic();
}
