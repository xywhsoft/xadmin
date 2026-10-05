#include <xs_plugin.h>
#include <stdlib.h>
#include <string.h>
static XAdminPluginHandle Handle;
XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int index,void* value){(void)index;(void)value;}
typedef struct Reply { XS_RequestObject req; bool started; } Reply;
static int Headers(void* data,uint16_t status,const xhttpfield* fields,size_t count)
{
    Reply* r=data;const char* type="application/octet-stream";size_t i;
    xhttpfield out[1]={{XRT_STR_LITERAL("Content-Type"),xrtStrView(type)}};
    for(i=0;i<count;i++)if(xrtStrCaseEqual(fields[i].Name,XRT_STR_LITERAL("Content-Type")))out[0]=fields[i];
    r->started=true;return XAdmin_StreamBegin(r->req,status,out,1,10000);
}
static int Data(void* data,const void* bytes,size_t size)
{return XAdmin_StreamWrite(((Reply*)data)->req,bytes,size);}
static void Work(XS_ServerObject s,XS_HostObject h,XS_RequestObject req,XS_ResponseObject resp,xvalue* session)
{
    (void)s;(void)h;(void)session;Reply reply={req,false};int status=0;
    XAdminHttpStreamConfig c={0};c.size=sizeof(c);c.url=getenv("STREAM_TEST_URL");c.allow_http=true;
    c.body=XAdmin_ReqBody(req);c.body_size=XAdmin_ReqBodyLen(req);c.timeout_ms=10000;c.first_byte_timeout_ms=2000;c.idle_timeout_ms=1500;
    c.max_response=1048576;c.ca_pem=getenv("STREAM_TEST_CA");c.data=&reply;c.on_headers=Headers;c.on_data=Data;
    int result=XAdmin_HttpStream(Handle,req,&c,&status);
    if(reply.started)XAdmin_StreamFinish(req,result==0);
    else xsHttpReplyAuto(resp,result==-2?504:502,"Content-Type: application/json\r\n","{}",2);
}
static void Route(XS_ServerObject s,XS_HostObject h,XS_RequestObject req,XS_ResponseObject resp,xvalue* session)
{
    (void)s;(void)h;
    if(XAdmin_DeferRoute(Handle,req,session,Work))xsHttpReplyAuto(resp,503,"Content-Type: application/json\r\n","{}",2);
}
static int Start(XAdminPluginHandle h){Handle=h;XAdminRouteDecl r={"/__test/stream",Route,false,false,0,0};return XAdmin_RegisterRoute(h,&r,NULL);}
static XAdminPluginDescriptor Descriptor={XADMIN_ABI_VERSION,sizeof(XAdminPluginDescriptor),"stream-sdk","1.0.0","Stream fixture",NULL,NULL,Start,NULL,NULL,NULL,NULL};
XADMIN_DECLARE_PLUGIN(Descriptor)
