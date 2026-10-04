/* Test composition only: never linked into production; no outbound SMS. */
#define ServiceInit XAdmin_ServiceInit
#define ServiceUnit XAdmin_ServiceUnit
#define RequestProc XAdmin_RequestProc
#include "../main.c"
#undef ServiceInit
#undef ServiceUnit
#undef RequestProc
static int SmsAssertions, SmsFailures, SmsCalls;
static xvalue* SmsCaptured;
static const char* SmsReply;
static int SmsHTTP = 200;
static bool SmsTransported = true;
static char* SmsCA;
static char* SmsNativeURL;
#define SMS_CHECK(e) do { ++SmsAssertions; if (!(e)) { ++SmsFailures; printf("FAIL SMS:%d: %s\n",__LINE__,#e); } } while(0)
static bool TestSmsTransport(const XASmsHttpRequest* r,int* status,char** response,void* context)
{
    (void)context; ++SmsCalls; xvalue* capture=ValueObject();xvalue* headers=ValueObject();size_t i;
    for(i=0;i<r->header_count;i++)ValueSetText(headers,r->names[i],r->values[i]);
    ValueSetText(capture,"url",r->url);ValueSetText(capture,"body",r->body);ValueSetText(capture,"content_type",r->content_type);
    ValueSetOwn(capture,"headers",headers);ValueArrayOwn(SmsCaptured,capture);
    *status=SmsHTTP;*response=xrtFormat("%s",SmsReply);return SmsTransported;
}
static xvalue* SmsFixture(const XASmsProvider* p)
{
    xvalue* v=ValueObject();xvalue* options=ValueObject();xvalue* templates=ValueObject();size_t i;
    ValueSetBool(v,"enabled",true);ValueSetText(v,"provider",p->id);
    for(i=0;i<p->field_count;i++){
        const XASmsField* f=&p->fields[i];
        const char* s=f->default_value&&*f->default_value?f->default_value:f->secret?"test-secret":"sample";
        if(!strcmp(f->name,"endpoint"))s="https://smsapi.cn-north-4.myhuaweicloud.com:443";
        if(!strcmp(f->name,"url"))s="https://sms.example.test/send";
        if(!strcmp(f->name,"sdk_app_id"))s="1400000000";
        ValueSetText(options,f->name,s);
    }
    xvalue* verification=xrtJsonParse(XRT_STR_LITERAL("{\"type\":\"verification\",\"id\":\"123\",\"parameters\":[\"code\",\"minutes\"]}"));
    xvalue* notice=xrtJsonParse(XRT_STR_LITERAL("{\"type\":\"notification\",\"id\":\"456\",\"parameters\":[\"order\"]}"));
    ValueSetOwn(templates,"verification",verification);ValueSetOwn(templates,"order_update",notice);
    ValueSetOwn(v,"options",options);ValueSetOwn(v,"templates",templates);return v;
}
void ServiceInit(XS_HostInfo* host)
{
    char* root=xrtPathParent(host->Path);char* path=xrtPathJoin(root,"ca.pem");SmsCA=xrtFileReadAll(path,NULL);xrtFree(path);
    path=xrtPathJoin(root,"native-url.txt");SmsNativeURL=xrtFileReadAll(path,NULL);xrtFree(path);xrtFree(root);
    SmsCaptured=ValueArray();SMS_CHECK(XA_SmsRegisterBuiltins());
    SMS_CHECK(!XA_SmsRegister(&XA_SmsWebhook));
    const char* good[]={"{\"sent\":true}","{\"Code\":\"OK\",\"BizId\":\"ali-id\"}",
        "{\"Response\":{\"SendStatusSet\":[{\"Code\":\"Ok\",\"SerialNo\":\"tc-id\"}]}}",
        "{\"code\":\"000000\",\"result\":[{\"status\":\"000000\",\"smsMsgId\":\"hw-id\"}]}",
        "{\"code\":\"1000\",\"data\":[{\"code\":\"1000\",\"messageId\":\"bd-id\"}]}",
        "{\"code\":0,\"sid\":3310228978}","{\"code\":\"000000\",\"failNum\":\"0\",\"successNum\":\"1\",\"msgId\":\"cl-id\"}",
        "{\"statusCode\":\"000000\",\"templateSMS\":{\"smsMessageSid\":\"rl-id\"}}","{\"status\":\"success\",\"send_id\":\"sm-id\"}"};
    const char* bad[]={"{\"sent\":false}","{\"Code\":\"isv.TEMPLATE_ILLEGAL\"}",
        "{\"Response\":{\"SendStatusSet\":[{\"Code\":\"FailedOperation.SignatureIncorrectOrUnapproved\"}]}}",
        "{\"code\":\"000000\",\"result\":[{\"status\":\"E0001\"}]}",
        "{\"code\":\"1000\",\"data\":[{\"code\":\"1001\"}]}","{\"code\":-3}",
        "{\"code\":\"000000\",\"failNum\":\"1\",\"successNum\":\"0\"}","{\"statusCode\":\"160040\"}","{\"status\":\"error\"}"};
    XASmsContext ctx={NULL,TestSmsTransport,NULL};size_t i;
    for(i=0;i<G_SmsProviderCount;i++){
        const XASmsProvider* p=G_SmsProviders[i];xvalue* v=SmsFixture(p);XASmsConfig config;
        SMS_CHECK(XA_SmsConfigDecode(v,&config));SMS_CHECK(XA_SmsAvailable(&config,"verification"));
        xvalue* redacted=XA_SmsConfigValue(&config,true);char* serialized=XA_SmsJson(redacted);
        SMS_CHECK(serialized&&!strstr(serialized,"test-secret"));xrtFree(serialized);xrtValueRelease(redacted);
        xvalue* params=xrtJsonParse(XRT_STR_LITERAL("{\"code\":\"123456\",\"minutes\":\"5\",\"purpose\":\"login\"}"));
        XASmsMessage m={XA_SMS_VERIFICATION,"+8613800138000","verification",params,"test-request"};XASmsReceipt receipt;
        SmsReply=good[i];SmsHTTP=200;SmsTransported=true;
        SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_ACCEPTED);SMS_CHECK(receipt.http_status==200);
        SmsReply=bad[i];SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_FAILED);
        SmsReply="{}";SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_UNKNOWN);
        if(!strcmp(p->id,"tencent")) {
            SmsReply="{\"Response\":{\"Error\":{\"Code\":\"InternalError.SendAndRecvFail\"}}}";
            SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_UNKNOWN);
            SmsReply="{\"Response\":{\"SendStatusSet\":[{\"Code\":\"InternalError.Timeout\"}]}}";
            SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_UNKNOWN);
            SmsReply="{\"Response\":{\"Error\":{\"Code\":\"AuthFailure.SignatureFailure\"}}}";
            SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_FAILED);
            SmsReply="{\"Response\":{\"Error\":{\"Code\":\"Ok\"}}}";
            SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_UNKNOWN);
        }
        if(!strcmp(p->id,"chuanglan")) {
            SmsReply="{\"code\":\"000000\",\"failNum\":\"0\"}";
            SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_UNKNOWN);
            SmsReply="{\"code\":\"000000\",\"failNum\":\"0\",\"successNum\":\"0\"}";
            SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_UNKNOWN);
            SmsReply="{\"code\":\"000000\",\"failNum\":-1,\"successNum\":1}";
            SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_UNKNOWN);
            SmsReply="{\"code\":\"000000\",\"failNum\":0,\"successNum\":1}";
            SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_ACCEPTED);
        }
        SmsReply=good[i];SmsTransported=false;SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_UNKNOWN);SmsTransported=true;
        SmsHTTP=503;SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_UNKNOWN);
        SmsHTTP=408;SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_UNKNOWN);
        SmsHTTP=401;SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_FAILED);SmsHTTP=200;
        SmsReply="{\"Code\":\"OK\"}"; /* response keys from another provider cannot grant acceptance */
        if(strcmp(p->id,"aliyun"))SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_UNKNOWN);
        int before=SmsCalls;m.kind=XA_SMS_NOTIFICATION;SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_FAILED);SMS_CHECK(before==SmsCalls);
        m.template_name="order_update";xrtValueRelease(params);params=xrtJsonParse(XRT_STR_LITERAL("{\"order\":\"订单 A&B+测试\"}"));m.parameters=params;
        SmsReply=good[i];SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_ACCEPTED);
        ValueSetText(params,"extra","rejected");before=SmsCalls;SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_FAILED);SMS_CHECK(before==SmsCalls);
        xrtValueRelease(params);ValueSetText(ValueGet(v,"options"),"unknown_field","x");SMS_CHECK(!XA_SmsConfigDecode(v,&config));xrtValueRelease(v);
    }
    xvalue* v=SmsFixture(&XA_SmsTencent);XASmsConfig config;
    ValueSetText(ValueGet(v,"options"),"secret_key","bad\r\nHeader: injected");SMS_CHECK(!XA_SmsConfigDecode(v,&config));xrtValueRelease(v);
    v=SmsFixture(&XA_SmsWebhook);
    ValueSetInt(ValueGet(ValueGet(v,"templates"),"verification"),"id",123);
    SMS_CHECK(!XA_SmsConfigDecode(v,&config));xrtValueRelease(v);
    v=SmsFixture(&XA_SmsTencent);xvalue* patch=xrtJsonParse(XRT_STR_LITERAL("{\"options\":{\"sign_name\":\"new\"}}"));
    SMS_CHECK(XA_SmsConfigMerge(v,patch));SMS_CHECK(!strcmp(ValueText(ValueGet(v,"options"),"secret_key"),"test-secret"));xrtValueRelease(patch);
    patch=xrtJsonParse(XRT_STR_LITERAL("{\"provider\":\"yunpian\",\"options\":{\"api_key\":\"new-key\"}}"));
    SMS_CHECK(XA_SmsConfigMerge(v,patch));SMS_CHECK(!ValueHas(ValueGet(v,"options"),"secret_key"));SMS_CHECK(XA_SmsConfigDecode(v,&config));
    xrtValueRelease(patch);xrtValueRelease(v);
    XASmsHttpRequest req={0};SMS_CHECK(!XA_SmsHttpHeader(&req,"Host","override"));SMS_CHECK(!XA_SmsHttpHeader(&req,"Authorization","a\r\nb"));
    SMS_CHECK(!XA_SmsHttpBody(&req,"http://example.test/","application/json","{}"));
    XASmsUrl url;SMS_CHECK(!XA_SmsUrlParse("https://user@evil.test/",&url));SMS_CHECK(!XA_SmsUrlParse("https://evil.test:999999/",&url));
    SMS_CHECK(XA_SmsUrlParse("https://[::1]:8443/send",&url));SMS_CHECK(!strcmp(url.host,"::1"));SMS_CHECK(url.port==8443);
    SMS_CHECK(!XA_SmsUrlParse("https://example.test/\001bad",&url));
    static const XASmsProvider custom={XA_SMS_ABI,"example_sms","Custom registered SMS",XA_SmsWebhookFields,2,
        XA_SmsWebhookValid,XA_SmsWebhookBuild,XA_SmsWebhookParse,false};
    SMS_CHECK(XA_SmsRegister(&custom));SMS_CHECK(XA_SmsFind("example_sms")==&custom);
    v=SmsFixture(&custom);SMS_CHECK(XA_SmsConfigDecode(v,&config));xrtValueRelease(v);
    xvalue* params=xrtJsonParse(XRT_STR_LITERAL("{\"code\":\"123456\",\"minutes\":\"5\"}"));
    XASmsMessage m={XA_SMS_VERIFICATION,"+8613800138000","verification",params,"ext-id"};XASmsReceipt receipt;
    SmsReply="{\"sent\":true}";SMS_CHECK(XA_SmsSend(&config,&ctx,&m,&receipt)==XA_SMS_ACCEPTED);xrtValueRelease(params);
    G_SmsFrozen=true;SMS_CHECK(!XA_SmsRegister(&XA_SmsWebhook));
    printf("SMS UNIT: %d assertions, %d failures\n",SmsAssertions,SmsFailures);
}
void ServiceUnit(XS_HostInfo* host) { (void)host;xrtValueRelease(SmsCaptured);xrtFree(SmsCA);xrtFree(SmsNativeURL); }
XS_RequestResult RequestProc(XS_HttpReq* req)
{
    if(SmsNativeURL && req->head->Target.Size>=8 && !memcmp(req->head->Target.Data,"/native/",8)){
        char* url=xrtFormat("%s/%.*s",SmsNativeURL,(int)(req->head->Target.Size-8),req->head->Target.Data+8);
        XASmsHttpRequest wire={0};int status=0;char* response=NULL;
        bool built=XA_SmsHttpBody(&wire,url,"application/json","{\"test\":true}");
        bool untrusted=xrtStrEqual(req->head->Target,XRT_STR_LITERAL("/native/untrusted"));
        bool ok=built && XA_SmsHttp(req->server->Engine,untrusted?NULL:SmsCA,&wire,&status,&response);
        xvalue* value=ValueObject();ValueSetBool(value,"ok",ok);ValueSetInt(value,"status",status);ValueSetText(value,"response",response);
        char* body=XA_SmsJson(value);char head[256];int n=snprintf(head,sizeof(head),"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %u\r\nConnection: close\r\n\r\n",(unsigned)strlen(body));
        xrtNetStreamSend(req->tcp,head,n);xrtNetStreamSend(req->tcp,body,strlen(body));
        xrtFree(url);XA_SmsSecretFree(response);XA_SmsSecretFree(body);XA_SmsHttpUnit(&wire);xrtValueRelease(value);return XS_OK;
    }
    xvalue* response=ValueObject();ValueSetInt(response,"assertions",SmsAssertions);ValueSetInt(response,"failures",SmsFailures);
    ValueSetRef(response,"captured",SmsCaptured);ValueSetOwn(response,"providers",XA_SmsCatalog());
    char* body=XA_SmsJson(response);char head[256];
    int n=snprintf(head,sizeof(head),"HTTP/1.1 %d OK\r\nContent-Type: application/json\r\nContent-Length: %u\r\nConnection: close\r\n\r\n",SmsFailures?500:200,(unsigned)strlen(body));
    xrtNetStreamSend(req->tcp,head,n);xrtNetStreamSend(req->tcp,body,strlen(body));
    XA_SmsSecretFree(body);xrtValueRelease(response);return XS_OK;
}
