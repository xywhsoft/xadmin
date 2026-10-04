/* Only this test composition installs an in-memory transport and diagnostic
 * routes. Production main.c never exposes codes, raw responses or send APIs. */
#define ServiceInit XAdmin_ServiceInit
#define ServiceUnit XAdmin_ServiceUnit
#include "../main.c"
#undef ServiceInit
#undef ServiceUnit
static xvalue* TestSmsDeliveries;
static char TestSmsResult[16]="accepted";
static bool SmsIdentityTransport(const XASmsHttpRequest* request,int* status,char** response,void* context)
{
    (void)context;const char* start=strstr(request->body,"code=");char code[7]={0};
    if(start && strlen(start+5)>=6)memcpy(code,start+5,6);
    xrtMutexLock(G_RequestLock);
    xvalue* item=ValueObject();ValueSetText(item,"code",code);ValueSetText(item,"result",TestSmsResult);
    ValueArrayOwn(TestSmsDeliveries,item);xrtMutexUnlock(G_RequestLock);
    *status=200;*response=xrtFormat("%s",!strcmp(TestSmsResult,"accepted")?"{\"sent\":true}":
        !strcmp(TestSmsResult,"failed")?"{\"sent\":false}":"{}");return true;
}
static void TestSmsRead(XS_ServerObject s,XS_HostObject h,XAdminRequest* req,XAdminRequest* resp,xvalue* session)
{
    (void)s;(void)h;(void)resp;(void)session;
    if(xsReqMethodID(req)==XHTTP_METHOD_POST){
        xvalue* body=XA_Body(req);const char* result=XA_Text(body,"result",15);
        if(result)strcpy(TestSmsResult,result);xrtValueRelease(body);
    }
    XA_Reply(req,200,"success",TestSmsDeliveries,NULL);
}
static void TestSmsNotify(XS_ServerObject s,XS_HostObject h,XAdminRequest* req,XAdminRequest* resp,xvalue* session)
{
    (void)s;(void)h;(void)resp;(void)session;
    xvalue* params=xrtJsonParse(XRT_STR_LITERAL("{\"order\":\"123\"}"));XASmsConfig config=G_Identity.sms;
    XASmsMessage message={XA_SMS_NOTIFICATION,"+8613800138000","order_update",params,"notice-id"};
    XASmsContext context={req->raw->server->Engine,G_SmsTransport,G_SmsTransportContext,NULL};XASmsReceipt receipt;
    xrtMutexUnlock(G_RequestLock);XASmsStatus status=XA_SmsSend(&config,&context,&message,&receipt);xrtMutexLock(G_RequestLock);
    xrtSecureZero(&config,sizeof(config));xrtValueRelease(params);xvalue* data=ValueObject();ValueSetInt(data,"status",status);
    XA_Reply(req,200,"success",data,NULL);xrtValueRelease(data);
}
void ServiceInit(XS_HostInfo* host)
{
    TestSmsDeliveries=ValueArray();XA_SmsSetTransport(SmsIdentityTransport,NULL);XAdmin_ServiceInit(host);
    if(!G_Ready)return;G_Ready=false;
    RouteInfo* r=AddStaticRouteHTTP("/__test/sms",XHTTP_METHOD_ANY,TestSmsRead,true);if(r){r->bAdmin=false;r->bAuth=false;}
    r=AddStaticRouteHTTP("/__test/sms/notify",XHTTP_METHOD_POST,TestSmsNotify,true);if(r){r->bAdmin=false;r->bAuth=false;}
    G_Ready=RouteHTTP_RecompileDynamic();
}
void ServiceUnit(XS_HostInfo* host)
{
    xrtValueRelease(TestSmsDeliveries);TestSmsDeliveries=NULL;XAdmin_ServiceUnit(host);
}
