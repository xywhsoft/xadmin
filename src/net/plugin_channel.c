/* Host-owned channels preserve xs' stream callback table (and script lease).
 * All metadata/queues/callbacks are under G_RequestLock. Only the owning thread
 * touches frame parsing and socket I/O, always outside that lock. A slot stays
 * allocated until its thread is joined: neither plugin code nor a stream can
 * outlive its owner. Payloads are transient and deliberately never logged. */
#define PLUGIN_CHANNEL_MAX 32
#define PLUGIN_CHANNEL_MESSAGES 64
#define PLUGIN_CHANNEL_SEND_US 2000000u
#define PLUGIN_CHANNEL_IDLE_US 60000000u
#define PLUGIN_CHANNEL_PING_US 20000000u
#define PLUGIN_CHANNEL_AUTH_US 5000000u

typedef struct PluginChannelPacket {
    struct PluginChannelPacket* next;
    size_t size;
    bool binary;
    char bytes[];
} PluginChannelPacket;
typedef struct PluginChannelState {
    PluginInstance* plugin;
    XAdminChannel id;
    XAdminChannelConfig config;
    XS_HttpReq raw;
    xthread* thread;
    char sid[65];
    int64 owner;
    char head[512]; size_t head_size;
    char* input; size_t input_size;
    char* message; size_t message_size; bool binary;
    xwsmessagestate parser;
    PluginChannelPacket *first, *last;
    size_t queued_bytes, queued_count;
    uint16 close_code;
    bool closing, closed, joining;
} PluginChannelState;
static PluginChannelState* G_PluginChannels[PLUGIN_CHANNEL_MAX];
static XAdminChannel G_PluginChannelSequence;
static bool G_PluginChannelsStopping;
static unsigned G_PluginChannelCallbacks;

static PluginInstance* PluginChannel_Owner(XAdminPluginHandle handle)
{
    size_t i; for(i=0;i<G_PluginCount;i++)if(handle==&G_Plugins[i])return &G_Plugins[i];
    return NULL;
}
static PluginChannelState* PluginChannel_Find(XAdminPluginHandle handle,XAdminChannel id)
{
    size_t i; if(!id)return NULL;
    for(i=0;i<PLUGIN_CHANNEL_MAX;i++)if(G_PluginChannels[i]&&
        G_PluginChannels[i]->id==id&&G_PluginChannels[i]->plugin==handle)return G_PluginChannels[i];
    return NULL;
}
static void PluginChannel_PacketFree(PluginChannelPacket* packet)
{
    if(!packet)return; xrtSecureZero(packet->bytes,packet->size);xrtFree(packet);
}
static void PluginChannel_Free(PluginChannelState* channel)
{
    PluginChannelPacket* packet=channel->first;
    while(packet){PluginChannelPacket* next=packet->next;PluginChannel_PacketFree(packet);packet=next;}
    if(channel->input)xrtSecureZero(channel->input,channel->config.message_limit+XWS_FRAME_HEAD_MAX);
    if(channel->message)xrtSecureZero(channel->message,channel->config.message_limit);
    xrtFree(channel->input);xrtFree(channel->message);
    xrtNetStreamDestroy(channel->raw.tcp);xrtTlsStreamDestroy(channel->raw.tls);
    if(channel->raw.server)xsServerRelease(channel->raw.server);
    xrtSecureZero(channel,sizeof(*channel));xrtFree(channel);
}
static bool PluginChannel_Live(PluginChannelState* channel)
{
    return channel->raw.tls?xrtTlsStreamState(channel->raw.tls)==XTLS_STREAM_OPEN:
        xrtNetStreamState(channel->raw.tcp)==XNET_STREAM_OPEN;
}
static bool PluginChannel_SendBytes(PluginChannelState* channel,const void* bytes,size_t size,XAdminDeadline deadline)
{
    const char* data=bytes;size_t offset=0;
    while(offset<size){
        size_t count=size-offset;if(count>16384)count=16384;
        if(XAdmin_DeadlineExpired(deadline)||!PluginChannel_Live(channel))return false;
        if(channel->raw.tls){
            if(!PluginAsync_Future(xrtTlsStreamSendAsync(channel->raw.tls,data+offset,count),deadline)||
               !PluginAsync_Future(xrtTlsStreamWaitAsync(channel->raw.tls,XTLS_STREAM_WAIT_DRAIN),deadline))return false;
        }else{
            size_t limit=xrtNetStreamWriteLimit(channel->raw.tcp);if(!limit)return false;
            if(count>limit)count=limit;
            xnetresult sent=xrtNetStreamSend(channel->raw.tcp,data+offset,count);
            if((sent!=XNET_RESULT_OK&&sent!=XNET_RESULT_AGAIN)||
               !xrtNetStreamWait(channel->raw.tcp,XNET_STREAM_WAIT_DRAIN,XAdmin_DeadlineRemainingMs(deadline),NULL))return false;
            if(sent==XNET_RESULT_AGAIN)continue;
        }
        offset+=count;
    }
    return true;
}
static bool PluginChannel_Frame(PluginChannelState* channel,uint8 opcode,const void* data,size_t size)
{
    xwsframe frame;char head[XWS_FRAME_HEAD_MAX];size_t count=0;
    xrtWsFrameInit(&frame);frame.Opcode=opcode;frame.Flags=XWS_FRAME_FIN;frame.PayloadSize=size;
    XAdminDeadline deadline=XAdmin_DeadlineAfterMs(PLUGIN_CHANNEL_SEND_US / 1000);
    return xrtWsFrameWrite(&frame,NULL,head,sizeof(head),&count)&&
        PluginChannel_SendBytes(channel,head,count,deadline)&&PluginChannel_SendBytes(channel,data,size,deadline);
}
static bool PluginChannel_Validate(PluginChannelState* channel)
{
    xvalue* session=XA_SessionRead(channel->sid,NULL);
    bool valid=session&&ValueInt(session,"id")==channel->owner;
    xrtValueRelease(session);return valid;
}
static bool PluginChannel_Fail(PluginChannelState* channel,uint16 code)
{
    xrtMutexLock(G_RequestLock);
    if(!channel->closing){channel->closing=true;channel->close_code=code;}
    xrtMutexUnlock(G_RequestLock);return false;
}
static bool PluginChannel_Receive(PluginChannelState* channel,uint64* last_read)
{
    size_t available=channel->raw.tls?xrtTlsStreamAvailable(channel->raw.tls):xrtNetStreamAvailable(channel->raw.tcp);
    if(available){
        size_t capacity=channel->config.message_limit+XWS_FRAME_HEAD_MAX-channel->input_size;
        if(!capacity)return PluginChannel_Fail(channel,1009);
        xfuture* future=channel->raw.tls?xrtTlsStreamRecvAsync(channel->raw.tls,capacity):xrtNetStreamRecvAsync(channel->raw.tcp,capacity);
        bool ok=future&&xrtFutureWaitFor(future,XAdmin_DeadlineRemainingMs(XAdmin_DeadlineAfterMs(PLUGIN_CHANNEL_SEND_US / 1000)))==XWAIT_OK&&xrtFutureState(future)==XFUTURE_RESOLVED;
        xnetbytes* bytes=ok?xrtFutureValue(future):NULL;
        if(!bytes)ok=false;
        if(ok){xbytesview view=xrtNetBytesView(bytes);ok=view.Size<=capacity;
            if(ok){memcpy(channel->input+channel->input_size,view.Data,view.Size);channel->input_size+=view.Size;}}
        if(!ok&&future)xrtFutureCancel(future);xrtFutureDestroy(future);
        if(!ok)return false;
    }
    while(channel->input_size){
        xwsframe frame;xwsframeconfig config;xwsmessageinfo info;xwsmessageerrorinfo error={0};
        xrtWsFrameConfigInit(&config);config.Mask=XWS_MASK_REQUIRED;config.MaxPayload=channel->config.message_limit;
        xwsframestatus status=xrtWsFrameParse((xbytesview){(uint8*)channel->input,channel->input_size},&frame,&config,NULL);
        if(status==XWS_FRAME_MORE)break;
        if(status!=XWS_FRAME_READY)return PluginChannel_Fail(channel,1002);
        size_t size=(size_t)frame.PayloadSize,total=frame.HeadSize+size;
        if(total>channel->input_size)break;
        char* payload=channel->input+frame.HeadSize;
        if(!xrtWsMask(payload,size,frame.Mask,0)||
           !xrtWsMessageFrameBegin(&channel->parser,&frame,&info,&error)||
           !xrtWsMessagePayload(&channel->parser,(xbytesview){(uint8*)payload,size},&error)||
           !xrtWsMessageFrameEnd(&channel->parser,&error))return PluginChannel_Fail(channel,error.CloseCode?error.CloseCode:1002);
        *last_read=XAdmin_MonotonicUs();
        if(frame.Opcode==XWS_OPCODE_CLOSE){
            uint16 code=size>=2?((uint16)(uint8)payload[0]<<8)|(uint8)payload[1]:1000;
            return PluginChannel_Fail(channel,code);
        }
        if(frame.Opcode==XWS_OPCODE_PING){if(!PluginChannel_Frame(channel,XWS_OPCODE_PONG,payload,size))return false;}
        else if(frame.Opcode==XWS_OPCODE_TEXT||frame.Opcode==XWS_OPCODE_BINARY||frame.Opcode==XWS_OPCODE_CONTINUATION){
            if(frame.Opcode!=XWS_OPCODE_CONTINUATION){channel->message_size=0;channel->binary=frame.Opcode==XWS_OPCODE_BINARY;}
            if(size>channel->config.message_limit-channel->message_size)return PluginChannel_Fail(channel,1009);
            memcpy(channel->message+channel->message_size,payload,size);channel->message_size+=size;
            if(frame.Flags&XWS_FRAME_FIN){
                xrtMutexLock(G_RequestLock);
                if(!channel->closing&&(!channel->plugin->started||!PluginChannel_Validate(channel))){channel->closing=true;channel->close_code=1008;}
                if(!channel->closing){G_PluginChannelCallbacks++;
                    channel->config.on_message(channel->id,channel->binary,channel->message,channel->message_size,channel->config.data);
                    G_PluginChannelCallbacks--;}
                bool closing=channel->closing;xrtMutexUnlock(G_RequestLock);
                xrtSecureZero(channel->message,channel->message_size);channel->message_size=0;
                if(closing)return false;
            }
        }
        channel->input_size-=total;memmove(channel->input,channel->input+total,channel->input_size);
    }
    return true;
}
static int32 PluginChannel_Run(void* data)
{
    PluginChannelState* channel=data;uint64 last_read=XAdmin_MonotonicUs(),last_ping=last_read,last_auth=last_read,last_owner=last_read;
    xwsmessageconfig config;xrtWsMessageConfigInitSafe(&config);config.MaxSize=channel->config.message_limit;
    bool ok=xrtWsMessageInit(&channel->parser,&config)&&
        PluginChannel_SendBytes(channel,channel->head,channel->head_size,XAdmin_DeadlineAfterMs(PLUGIN_CHANNEL_SEND_US / 1000));
    xrtMutexLock(G_RequestLock);
    if(ok&&!channel->closing&&channel->plugin->started&&PluginChannel_Validate(channel)){
        if(channel->config.on_open){G_PluginChannelCallbacks++;
            channel->config.on_open(channel->id,channel->config.data);G_PluginChannelCallbacks--;}
    }else{channel->closing=true;channel->close_code=ok?1008:1006;}
    xrtMutexUnlock(G_RequestLock);
    while(ok){
        uint64 now=XAdmin_MonotonicUs();PluginChannelPacket* packet=NULL;
        /* TAKEOVER connections can otherwise keep a retired generation alive
         * forever. Public retained-server lookup detects publication without
         * peeking at xs' private generation or replacing its callbacks. */
        if(now-last_owner>=250000u){
            XS_ServerInfo* current=xsServerFind(channel->raw.server->Name);
            bool retired=current!=channel->raw.server;
            xsServerRelease(current);last_owner=now;
            if(retired){PluginChannel_Fail(channel,1001);break;}
        }
        xrtMutexLock(G_RequestLock);
        if(!channel->closing&&now-last_auth>=PLUGIN_CHANNEL_AUTH_US){
            last_auth=now;if(!PluginChannel_Validate(channel)){channel->closing=true;channel->close_code=1008;}}
        bool closing=channel->closing;
        if(!closing&&channel->first){packet=channel->first;channel->first=packet->next;
            if(!channel->first)channel->last=NULL;channel->queued_bytes-=packet->size;channel->queued_count--;}
        xrtMutexUnlock(G_RequestLock);
        if(closing)break;
        if(packet){ok=PluginChannel_Frame(channel,packet->binary?XWS_OPCODE_BINARY:XWS_OPCODE_TEXT,packet->bytes,packet->size);PluginChannel_PacketFree(packet);}
        if(ok)ok=PluginChannel_Live(channel)&&PluginChannel_Receive(channel,&last_read);
        now=XAdmin_MonotonicUs(); /* Receive can advance last_read; compare the newer clock. */
        if(ok&&now-last_read>=PLUGIN_CHANNEL_IDLE_US){PluginChannel_Fail(channel,1001);break;}
        if(ok&&now-last_ping>=PLUGIN_CHANNEL_PING_US){ok=PluginChannel_Frame(channel,XWS_OPCODE_PING,NULL,0);last_ping=now;}
        if(ok&&!packet)xrtSleep(10);
    }
    xrtMutexLock(G_RequestLock);
    if(!channel->closing){channel->closing=true;channel->close_code=1006;}
    uint16 code=channel->close_code;
    xrtMutexUnlock(G_RequestLock);
    bool graceful=false;
    if(PluginChannel_Live(channel)&&xrtWsCloseCodeValid(code)){
        char payload[2];size_t size=0;
        graceful=xrtWsCloseWrite(code,xrtStrView(""),payload,sizeof(payload),&size)&&PluginChannel_Frame(channel,XWS_OPCODE_CLOSE,payload,size);
    }
    if(channel->raw.tls){if(graceful)xrtTlsStreamClose(channel->raw.tls);else xrtTlsStreamAbort(channel->raw.tls);}
    else{if(graceful)xrtNetStreamClose(channel->raw.tcp);else xrtNetStreamAbort(channel->raw.tcp);}
    xrtMutexLock(G_RequestLock);channel->closed=true;
    if(channel->config.on_close){G_PluginChannelCallbacks++;
        channel->config.on_close(channel->id,code,channel->config.data);G_PluginChannelCallbacks--;}
    xrtMutexUnlock(G_RequestLock);return 0;
}
static int XAdmin_ChannelSend(XAdminPluginHandle plugin,XAdminChannel id,bool binary,const void* bytes,size_t size)
{
    PluginChannelState* channel=PluginChannel_Find(plugin,id);
    if(!channel||channel->closing||!channel->plugin->started||(size&&!bytes)||size>channel->config.message_limit)return -1;
    if(channel->queued_count>=PLUGIN_CHANNEL_MESSAGES||size>channel->config.queue_limit-channel->queued_bytes)return -2;
    if(!binary&&!xrtUtf8Valid(xrtStrViewN(bytes,size),NULL))return -1;
    PluginChannelPacket* packet=xrtMalloc(sizeof(*packet)+size);if(!packet)return -2;
    packet->next=NULL;packet->size=size;packet->binary=binary;if(size)memcpy(packet->bytes,bytes,size);
    if(channel->last)channel->last->next=packet;else channel->first=packet;
    channel->last=packet;channel->queued_bytes+=size;channel->queued_count++;return 0;
}
static int XAdmin_ChannelClose(XAdminPluginHandle plugin,XAdminChannel id,uint16 code)
{
    PluginChannelState* channel=PluginChannel_Find(plugin,id);
    if(!channel||!xrtWsCloseCodeValid(code))return -1;
    if(!channel->closing){channel->closing=true;channel->close_code=code;}return 0;
}
static void PluginChannel_Revoke(const char* sid,int64 owner,const char* keep_sid)
{
    size_t i;for(i=0;i<PLUGIN_CHANNEL_MAX;i++){
        PluginChannelState* channel=G_PluginChannels[i];if(!channel||channel->closing)continue;
        if((sid&&!strcmp(channel->sid,sid))||(owner>0&&channel->owner==owner&&(!keep_sid||strcmp(channel->sid,keep_sid)))){
            channel->closing=true;channel->close_code=1008;}}
}
static int XAdmin_ChannelAccept(XAdminPluginHandle handle,XS_RequestObject req,xvalue* session,
    const XAdminChannelConfig* config,XAdminChannel* out)
{
    PluginInstance* plugin=PluginChannel_Owner(handle);size_t i,slot=PLUGIN_CHANNEL_MAX;
    if(out)*out=0;
    if(!out||!plugin||!plugin->started||plugin->stopping||G_PluginChannelsStopping||G_PluginRegIdx>=0||
       !req||!req->raw||!req->raw->head||req->deferred||req->replied||req->body_size||
       memchr(req->raw->head->Target.Data,'?',req->raw->head->Target.Size)||!config||config->size!=sizeof(*config)||!config->on_message||
       !config->protocol||!config->protocol[0]||strlen(config->protocol)>128||!config->message_limit||
       config->message_limit>256u*1024u||config->queue_limit<config->message_limit||config->queue_limit>4u*1024u*1024u||
       (!config->allow_cross_origin&&!XA_OriginAllowed(req))||G_PluginChannelSequence==UINTPTR_MAX)return -1;
    const char* sid=XA_Text(session,"sid",64);int64 owner=ValueInt(session,"id");
    xvalue* verified=XA_IsHex(sid,64)?XA_SessionRead(sid,NULL):NULL;
    bool authorized=verified&&owner>0&&ValueInt(verified,"id")==owner;xrtValueRelease(verified);
    if(!authorized)return -1;
    xwsupgradeserverconfig upgrade_config;xwsupgrade upgrade;
    xrtWsUpgradeServerConfigInit(&upgrade_config);upgrade_config.Protocols=xrtStrView(config->protocol);
    if(!xrtWsUpgradeRequestCheck(req->raw->head,&upgrade_config,&upgrade)||!upgrade.Protocol.Size)return -1;
    for(i=0;i<PLUGIN_CHANNEL_MAX;i++){
        PluginChannelState* old=G_PluginChannels[i];
        if(old&&!old->joining&&xrtThreadState(old->thread)==XTHREAD_FINISHED){
            xrtThreadWait(old->thread);xrtThreadDestroy(old->thread);PluginChannel_Free(old);G_PluginChannels[i]=NULL;}
        if(!G_PluginChannels[i]&&slot==PLUGIN_CHANNEL_MAX)slot=i;
    }
    if(slot==PLUGIN_CHANNEL_MAX)return -1;
    PluginChannelState* channel=xrtCalloc(1,sizeof(*channel));if(!channel)return -1;
    channel->plugin=plugin;channel->config=*config;channel->config.protocol=NULL;
    channel->id=++G_PluginChannelSequence;channel->owner=owner;memcpy(channel->sid,sid,65);
    channel->input=xrtMalloc(config->message_limit+XWS_FRAME_HEAD_MAX);channel->message=xrtMalloc(config->message_limit);
    channel->raw.server=xsServerRetain(req->raw->server);channel->raw.host=req->raw->host;
    channel->raw.tcp=req->raw->tcp?xrtNetStreamRef(req->raw->tcp):NULL;
    channel->raw.tls=req->raw->tls?xrtTlsStreamRef(req->raw->tls):NULL;
    xhttpfield fields[XWS_UPGRADE_RESPONSE_FIELDS_MAX];size_t count=0;
    if(!channel->input||!channel->message||!channel->raw.server||(!channel->raw.tcp&&!channel->raw.tls)||
       !xrtWsUpgradeResponseFields(xrtStrView(upgrade.Accept),upgrade.Protocol,xrtStrView(""),fields,XWS_UPGRADE_RESPONSE_FIELDS_MAX,&count)||
       !xrtHttp1ResponseWrite(XHTTP_VERSION_1_1,101,XRT_STR_LITERAL("Switching Protocols"),fields,count,
            channel->head,sizeof(channel->head),&channel->head_size)){PluginChannel_Free(channel);return -1;}
    G_PluginChannels[slot]=channel;channel->thread=xrtThreadCreate(PluginChannel_Run,channel,0);
    if(!channel->thread){G_PluginChannels[slot]=NULL;PluginChannel_Free(channel);return -1;}
    req->deferred=true;req->replied=true;*out=channel->id;return 0;
}
/* Called with G_RequestLock. Joining releases it so close callbacks can finish.
 * Joining marks exclude slots from opportunistic GC in other plugins. */
static void PluginChannel_Stop(PluginInstance* plugin)
{
    size_t i;bool found=false;PluginChannelState* channels[PLUGIN_CHANNEL_MAX]={0};
    for(i=0;i<PLUGIN_CHANNEL_MAX;i++)if(G_PluginChannels[i]&&G_PluginChannels[i]->plugin==plugin){
        channels[i]=G_PluginChannels[i];channels[i]->joining=true;channels[i]->closing=true;channels[i]->close_code=1001;found=true;}
    if(!found)return;
    xrtMutexUnlock(G_RequestLock);
    for(i=0;i<PLUGIN_CHANNEL_MAX;i++)if(channels[i])xrtThreadWait(channels[i]->thread);
    xrtMutexLock(G_RequestLock);
    for(i=0;i<PLUGIN_CHANNEL_MAX;i++)if(channels[i]){
        xrtThreadDestroy(channels[i]->thread);PluginChannel_Free(channels[i]);G_PluginChannels[i]=NULL;}
}
static void PluginChannel_Unit(void)
{
    size_t i;xrtMutexLock(G_RequestLock);G_PluginChannelsStopping=true;
    for(i=0;i<G_PluginCount;i++){G_Plugins[i].stopping=true;PluginChannel_Stop(&G_Plugins[i]);}
    xrtMutexUnlock(G_RequestLock);
}
