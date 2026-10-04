/* Real SDK consumer; this plugin is copied only into disposable test sites. */
#include <xs_plugin.h>
#include <string.h>
static XAdminPluginHandle Handle;
static unsigned Opens,Closes,Messages;
static unsigned Backpressure;
XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int slot,void* value){(void)slot;(void)value;}
static int Load(XAdminPluginHandle* out){Handle=*out;return 0;}
static void Open(XAdminChannel channel,void* data)
{(void)data;Opens++;XAdmin_ChannelSend(Handle,channel,false,"ready",5);}
static void Message(XAdminChannel channel,bool binary,const void* bytes,size_t size,void* data)
{
    (void)data;Messages++;
    if(!binary&&size==11&&!memcmp(bytes,"reload-self",11)){
        int result=XAdmin_ReloadPlugin(Handle,"channel-sdk");
        XAdmin_ChannelSend(Handle,channel,false,result<0?"rejected":"bad",result<0?8:3);
    }else if(!binary&&size==11&&!memcmp(bytes,"queue-bound",11)){
        char packet[4096];memset(packet,'x',sizeof(packet));
        int first=XAdmin_ChannelSend(Handle,channel,true,packet,sizeof(packet));
        int second=XAdmin_ChannelSend(Handle,channel,true,packet,sizeof(packet));
        int third=XAdmin_ChannelSend(Handle,channel,true,packet,sizeof(packet));
        if(first==0&&second==0&&third==-2)Backpressure++;
        XAdmin_ChannelClose(Handle,channel,1013);
    }else if(XAdmin_ChannelSend(Handle,channel,binary,bytes,size)!=0)XAdmin_ChannelClose(Handle,channel,1013);
}
static void Close(XAdminChannel channel,uint16_t code,void* data)
{(void)channel;(void)code;(void)data;Closes++;}
static void Connect(XS_ServerObject server,XS_HostObject host,XS_RequestObject req,XS_ResponseObject resp,xvalue* session)
{
    (void)server;(void)host;XAdminChannel channel=0;
    XAdminChannelConfig config={0};config.size=sizeof(config);config.protocol="xadmin.test.v1";
    config.message_limit=4096;config.queue_limit=8192;
    config.on_open=Open;config.on_message=Message;config.on_close=Close;
    if(XAdmin_ChannelAccept(Handle,req,session,&config,&channel)!=0)
        xsHttpReplyAuto(resp,403,"Content-Type: text/plain\r\n","channel rejected",0);
}
static void Stats(XS_ServerObject server,XS_HostObject host,XS_RequestObject req,XS_ResponseObject resp,xvalue* session)
{
    (void)server;(void)host;(void)req;(void)session;
    xsHttpReplyFormat(resp,200,"Content-Type: application/json\r\n","{\"opens\":%u,\"closes\":%u,\"messages\":%u,\"backpressure\":%u}",Opens,Closes,Messages,Backpressure);
}
static int Start(XAdminPluginHandle handle)
{
    Handle=handle;XAdminRouteToken token;
    XAdminRouteDecl connect={"/api/v1/channel-test",Connect,false,false,0,0};
    XAdminRouteDecl stats={"/api/v1/channel-test/stats",Stats,false,false,0,0};
    return XAdmin_RegisterRoute(handle,&connect,&token)||XAdmin_RegisterRoute(handle,&stats,&token)?-1:0;
}
static const XAdminPluginDescriptor Descriptor={XADMIN_ABI_VERSION,sizeof(XAdminPluginDescriptor),
    "channel-sdk","1.0.0","Managed channel test",Load,NULL,Start,NULL,NULL,NULL,NULL};
XADMIN_DECLARE_PLUGIN(Descriptor)
