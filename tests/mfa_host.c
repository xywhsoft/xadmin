/* Isolated RFC vectors and AEAD invariants; never included by production. */
#define ServiceInit XAdmin_ServiceInit
#include "../main.c"
#undef ServiceInit
static void TestMFA(XS_ServerObject server,XS_HostObject host,XAdminRequest* req,XAdminRequest* resp,xvalue* session)
{
    (void)server;(void)host;(void)resp;
    if(xrtValueType(session)!=XVALUE_OBJECT){XA_Reply(req,401,"unauthorized",NULL,NULL);return;}
    const unsigned char key[20]="12345678901234567890";
    const int64 times[]={59,1111111109,1111111111,1234567890,2000000000,INT64_C(20000000000)};
    const char* expected[]={"287082","081804","050471","005924","279037","353130"};
    char code[7],base32[33];unsigned char blob[48],plain[20]={0};bool ok=true;size_t i;
    for(i=0;i<6;i++)ok=ok&&XA_MFATOTP(key,times[i]/30,code)&&!strcmp(code,expected[i]);
    XA_MFABase32(key,base32);ok=ok&&!strcmp(base32,"GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ");
    ok=ok&&XA_MFACrypt(true,"member",42,1,(unsigned char*)key,blob)&&XA_MFACrypt(false,"member",42,1,plain,blob)&&!memcmp(key,plain,20);
    memset(plain,0,20);ok=ok&&!XA_MFACrypt(false,"admin",42,1,plain,blob)&&!memcmp(plain,"\0\0\0\0",4);
    ok=ok&&!XA_MFACrypt(false,"member",43,1,plain,blob)&&!XA_MFACrypt(false,"member",42,2,plain,blob);
    blob[20]^=1;ok=ok&&!XA_MFACrypt(false,"member",42,1,plain,blob);
    xrtSecureZero(plain,sizeof(plain));XA_Reply(req,ok?200:500,ok?"RFC vectors and AEAD isolation passed":"MFA invariants failed",NULL,NULL);
}
void ServiceInit(XS_HostInfo* host)
{
    XAdmin_ServiceInit(host);if(!G_Ready)return;G_Ready=false;
    RouteInfo* r=AddStaticRouteHTTP("/__test/mfa/invariants",XHTTP_METHOD_GET,TestMFA,true);if(r){r->bAdmin=true;r->bAuth=false;}
    G_Ready=RouteHTTP_RecompileDynamic();
}
