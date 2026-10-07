/* Native streaming transport. A deferred job pins request, socket and plugin.
 * HTTP framing is decoded incrementally by xrt; no whole-response buffer.
 * Every callback enters the application lock, so plugin DB/config state has
 * the same serialization contract as ordinary routes. */
static bool PluginStream_PeerOpen(XS_RequestObject req)
{
    return req && req->raw && (req->raw->tls ?
        xrtTlsStreamState(req->raw->tls) < XTLS_STREAM_CLOSING :
        req->raw->tcp && xrtNetStreamState(req->raw->tcp) < XNET_STREAM_CLOSING);
}
static void PluginStream_Abort(XS_RequestObject req)
{
    if (req->raw->tls) xrtTlsStreamAbort(req->raw->tls);
    else xrtNetStreamAbort(req->raw->tcp);
}
/* Called without the application lock. Send copies the bytes; drain provides
 * bounded backpressure. Keep this buffer alive through the completed future. */
static bool PluginStream_Send(XS_RequestObject req, const void* bytes, size_t size)
{
    size_t offset = 0;
    while (offset < size) {
        size_t n = size-offset; if (n > 16384) n = 16384;
        if (!PluginStream_PeerOpen(req) || XAdmin_DeadlineExpired(req->stream_deadline)) return false;
        if (req->raw->tls) {
            if (!PluginAsync_Future(xrtTlsStreamSendAsync(req->raw->tls,(cbytes)bytes+offset,n),req->stream_deadline) ||
                !PluginAsync_Future(xrtTlsStreamWaitAsync(req->raw->tls,XTLS_STREAM_WAIT_DRAIN),req->stream_deadline)) return false;
        } else {
            size_t limit = xrtNetStreamWriteLimit(req->raw->tcp);
            if (!limit) return false; if (n > limit) n = limit;
            xnetresult sent = xrtNetStreamSend(req->raw->tcp,(cbytes)bytes+offset,n);
            if ((sent != XNET_RESULT_OK && sent != XNET_RESULT_AGAIN) ||
                !xrtNetStreamWait(req->raw->tcp,XNET_STREAM_WAIT_DRAIN,XAdmin_DeadlineRemainingMs(req->stream_deadline),NULL)) return false;
            if (sent == XNET_RESULT_AGAIN) continue;
        }
        offset += n;
    }
    return true;
}
static int XAdmin_StreamBegin(XS_RequestObject req, uint16 status,
    const xhttpfield* fields, size_t count, unsigned timeout_ms)
{
    char head[4096]; xhttpfield all[17]; size_t n, i;
    if (!req || !req->deferred || req->replied || !req->raw || status < 200 || status > 599 ||
        count > 13 || (count && !fields) || timeout_ms < 100 || timeout_ms > 600000) return -1;
    for (i=0;i<count;i++) {
        if (xrtStrCaseEqual(fields[i].Name,XRT_STR_LITERAL("Content-Length")) ||
            xrtStrCaseEqual(fields[i].Name,XRT_STR_LITERAL("Transfer-Encoding")) ||
            xrtStrCaseEqual(fields[i].Name,XRT_STR_LITERAL("Connection"))) return -1;
        all[i]=fields[i];
    }
    all[count++]=(xhttpfield){XRT_STR_LITERAL("Transfer-Encoding"),XRT_STR_LITERAL("chunked")};
    all[count++]=(xhttpfield){XRT_STR_LITERAL("Connection"),XRT_STR_LITERAL("close")};
    xstrview origin=XA_CORSOrigin(req->raw);
    if(origin.Size){all[count++]=(xhttpfield){XRT_STR_LITERAL("Access-Control-Allow-Origin"),origin};
        all[count++]=(xhttpfield){XRT_STR_LITERAL("Vary"),XRT_STR_LITERAL("Origin")};}
    if (!xrtHttp1ResponseWrite(XHTTP_VERSION_1_1,status,xrtHttpStatusText(status),all,count,head,sizeof(head),&n)) return -1;
    req->replied=true; req->streaming=true; req->stream_deadline=XAdmin_DeadlineAfterMs(timeout_ms);
    xrtMutexUnlock(G_RequestLock); bool ok=PluginStream_Send(req,head,n);
    if(!ok)PluginStream_Abort(req); xrtMutexLock(G_RequestLock);
    req->stream_failed=!ok; return ok?0:-1;
}
static int XAdmin_StreamWrite(XS_RequestObject req,const void* bytes,size_t size)
{
    if(!req || !req->streaming || req->stream_failed || (size&&!bytes) || size>1048576) return -1;
    size_t offset=0; bool ok=true; xrtMutexUnlock(G_RequestLock);
    while(ok && offset<size){
        char line[32]; size_t n=size-offset; if(n>16384)n=16384;
        int length=snprintf(line,sizeof(line),"%zx\r\n",n);
        ok=length>0 && PluginStream_Send(req,line,(size_t)length) &&
            PluginStream_Send(req,(cbytes)bytes+offset,n) && PluginStream_Send(req,"\r\n",2);
        offset+=n;
    }
    if(!ok)PluginStream_Abort(req); xrtMutexLock(G_RequestLock);
    req->stream_failed=!ok; return ok?0:-1;
}
static int XAdmin_StreamFinish(XS_RequestObject req,bool success)
{
    if(!req || !req->streaming)return -1;
    bool ok=success&&!req->stream_failed; xrtMutexUnlock(G_RequestLock);
    if(ok)ok=PluginStream_Send(req,"0\r\n\r\n",5);
    if(!ok)PluginStream_Abort(req); xrtMutexLock(G_RequestLock);
    req->streaming=false; req->stream_failed=!ok; return ok?0:-1;
}
static bool PluginStream_Wait(XS_RequestObject req,xfuture* future,XAdminDeadline total,unsigned idle_ms)
{
    if(!future)return false;
    XAdminDeadline idle=XAdmin_DeadlineAfterMs(idle_ms);
    while(xrtFutureState(future)==XFUTURE_PENDING){
        uint64 remaining=XAdmin_DeadlineRemainingMs(total), r=XAdmin_DeadlineRemainingMs(idle);
        if(!remaining || !r || !PluginStream_PeerOpen(req))break;
        if(r<remaining)remaining=r; if(remaining>100)remaining=100;
        xrtFutureWaitFor(future,remaining);
    }
    return xrtFutureState(future)!=XFUTURE_PENDING && PluginStream_PeerOpen(req);
}
static int PluginStream_Headers(const XAdminHttpStreamConfig* config,const xhttp1head* head)
{
    int result; xrtMutexLock(G_RequestLock);
    result=config->on_headers(config->data,head->Status,head->Fields,head->FieldCount);
    xrtMutexUnlock(G_RequestLock); return result;
}
static int PluginStream_Data(const XAdminHttpStreamConfig* config,xbytesview bytes)
{
    int result; xrtMutexLock(G_RequestLock);
    result=config->on_data(config->data,bytes.Data,bytes.Size);
    xrtMutexUnlock(G_RequestLock); return result;
}
static int XAdmin_HttpStream(XAdminPluginHandle handle,XS_RequestObject req,
    const XAdminHttpStreamConfig* input,int* upstream_status)
{
    PluginInstance* inst=NULL; size_t i, head_size; XAHttpUrl url; bool secure;
    if(upstream_status)*upstream_status=0;
    for(i=0;i<G_PluginCount;i++)if(handle==&G_Plugins[i])inst=&G_Plugins[i];
    if(!inst || !inst->started || !req || !req->deferred || !input || input->size!=sizeof(*input) ||
        !upstream_status || !input->on_headers || !input->on_data || !input->url ||
        input->header_count>16 || (input->header_count&&!input->headers) ||
        !input->body || !input->body_size || input->body_size>1048576 ||
        input->timeout_ms<100 || input->timeout_ms>600000 ||
        !input->first_byte_timeout_ms || input->first_byte_timeout_ms>input->timeout_ms ||
        !input->idle_timeout_ms || input->idle_timeout_ms>input->timeout_ms ||
        !input->max_response || input->max_response>32u*1024u*1024u)return -1;
    XAdminHttpStreamConfig config=*input; secure=!strncmp(input->url,"https://",8);
    char* converted=NULL;
    if(!secure){if(!config.allow_http || strncmp(input->url,"http://",7))return -1;
        converted=xrtFormat("https://%s",input->url+7);}
    bool valid=XA_HttpsUrlParse(secure?input->url:converted,&url); xrtFree(converted);
    if(!valid)return -1;
    if(!secure && !strchr(url.authority,':'))url.port=80;
    xbuffer wire, incoming; xrtBufferInit(&wire); xrtBufferInit(&incoming);
    char* ca_copy=NULL;
    if(config.ca_pem){if(strlen(config.ca_pem)>131072)goto invalid;ca_copy=xrtStrDup(config.ca_pem);if(!ca_copy)goto invalid;config.ca_pem=ca_copy;}
    xhttpfield fields[22]; char length[32];
    snprintf(length,sizeof(length),"%llu",(unsigned long long)config.body_size);
    fields[0]=(xhttpfield){XRT_STR_LITERAL("Host"),xrtStrView(url.authority)};
    fields[1]=(xhttpfield){XRT_STR_LITERAL("Content-Length"),xrtStrView(length)};
    fields[2]=(xhttpfield){XRT_STR_LITERAL("Content-Type"),XRT_STR_LITERAL("application/json")};
    fields[3]=(xhttpfield){XRT_STR_LITERAL("Connection"),XRT_STR_LITERAL("close")};
    fields[4]=(xhttpfield){XRT_STR_LITERAL("Accept"),XRT_STR_LITERAL("application/json, text/event-stream")};
    fields[5]=(xhttpfield){XRT_STR_LITERAL("Accept-Encoding"),XRT_STR_LITERAL("identity")};
    for(i=0;i<config.header_count;i++){
        xhttpfield field=config.headers[i];
        const char* reserved[]={"Host","Content-Length","Content-Type","Connection","Transfer-Encoding","Accept","Accept-Encoding"};
        size_t j; for(j=0;j<7;j++)if(xrtStrCaseEqual(field.Name,xrtStrView(reserved[j])))break;
        if(j!=7 || field.Name.Size>64 || field.Value.Size>2048)goto invalid;
        for(j=0;j<i;j++)if(xrtStrCaseEqual(field.Name,config.headers[j].Name))goto invalid;
        fields[i+6]=field;
    }
    if(!xrtHttp1RequestWrite(XRT_STR_LITERAL("POST"),xrtStrView(url.path),XHTTP_VERSION_1_1,fields,config.header_count+6,NULL,0,&head_size) ||
        !xrtBufferResize(&wire,head_size) ||
        !xrtHttp1RequestWrite(XRT_STR_LITERAL("POST"),xrtStrView(url.path),XHTTP_VERSION_1_1,fields,config.header_count+6,wire.Data,wire.Size,&head_size) ||
        !xrtBufferAppend(&wire,(xbytesview){config.body,config.body_size}))goto invalid;
    /* Request wire now owns all secret headers and body. No caller view is
     * read after unlocking, except the documented pinned callback context. */
    xnetresolver* resolver=NULL; xtlsverifier* verifier=NULL; xtlsstream* tls=NULL; xnetstream* tcp=NULL;
    xfuture* future=NULL; int result=-1; bool end=false, headed=false; size_t offset=0;
    xhttp1head head; xhttpfield response_fields[64], trailers[16];
    xhttp1body body; xhttp1bodyplan plan; xhttp1limits limits; xhttp1bodylimits body_limits; xhttp1errorinfo error;
    xrtHttp1HeadInit(&head,response_fields,64); xrtHttp1LimitsInit(&limits); limits.MaxFields=64;
    xrtHttp1BodyLimitsInit(&body_limits); body_limits.MaxBody=config.max_response; body_limits.MaxTrailers=16;
    XAdminDeadline deadline=XAdmin_DeadlineAfterMs(config.timeout_ms);
    XAdminDeadline first=XAdmin_DeadlineAfterMs(config.first_byte_timeout_ms);
    inst->activeIo++; xrtMutexUnlock(G_RequestLock);
    xnetresolverconfig rc; xrtNetResolverConfigInit(&rc); resolver=xrtNetResolverCreate(&rc);
    if(!resolver)goto done;
    if(secure){
        xtlsverifierconfig vc; xrtTlsVerifierConfigInit(&vc);
        xx509store* store=config.ca_pem?xrtX509StoreCreate():xrtX509StoreSystem();
        if(store && config.ca_pem && !xrtX509StoreAddPem(store,config.ca_pem,strlen(config.ca_pem),NULL)){xrtX509StoreFree(store);store=NULL;}
        if(store){vc.Store=store;verifier=xrtTlsVerifierCreate(&vc);xrtX509StoreFree(store);} if(!verifier)goto done;
        xtlsclientconfig tc; xtlsdialconfig dc; xrtTlsClientConfigInit(&tc);xrtTlsDialConfigInit(&dc);
        tc.Verifier=verifier;tc.VerifyName=xrtStrView(url.host); xnetaddr address;
        if(!xrtNetAddrParse(&address,url.host,(uint16)url.port))tc.ServerName=tc.VerifyName;
        dc.ServerNameFromHost=false;dc.Timeout=XAdmin_DeadlineRemainingMs(first);
        future=xrtTlsDialAsync(req->raw->server->Engine,resolver,url.host,(uint16)url.port,&tc,&dc,NULL,NULL);
    }else{
        xnetdialconfig dc;xrtNetDialConfigInit(&dc);dc.Timeout=XAdmin_DeadlineRemainingMs(first);
        future=xrtNetDialAsync(req->raw->server->Engine,resolver,url.host,(uint16)url.port,&dc,NULL,NULL);
    }
    if(!PluginStream_Wait(req,future,first,config.first_byte_timeout_ms) || xrtFutureState(future)!=XFUTURE_RESOLVED)goto done;
    if(secure)tls=xrtTlsStreamRef(xrtFutureValue(future));else tcp=xrtNetStreamRef(xrtFutureValue(future));
    XA_HttpsFutureDone(future);future=NULL; if(!tls&&!tcp)goto done;
    if(config.on_send){xrtMutexLock(G_RequestLock);config.on_send(config.data);xrtMutexUnlock(G_RequestLock);}
    for(i=0;i<wire.Size;){
        size_t n=wire.Size-i;if(n>16384)n=16384;
        if(tls){future=xrtTlsStreamSendAsync(tls,(cbytes)wire.Data+i,n);
            if(!PluginStream_Wait(req,future,first,config.first_byte_timeout_ms)||xrtFutureState(future)!=XFUTURE_RESOLVED)goto done;
            XA_HttpsFutureDone(future);future=NULL;
        }else{
            size_t limit=xrtNetStreamWriteLimit(tcp);if(!limit)goto done;if(n>limit)n=limit;
            xnetresult sent=xrtNetStreamSend(tcp,(cbytes)wire.Data+i,n);
            if((sent!=XNET_RESULT_OK&&sent!=XNET_RESULT_AGAIN)||!xrtNetStreamWait(tcp,XNET_STREAM_WAIT_DRAIN,XAdmin_DeadlineRemainingMs(first),NULL))goto done;
            if(sent==XNET_RESULT_AGAIN)continue;
        }i+=n;
    }
    for(;;){
        if(!headed){
            xhttp1status parsed=xrtHttp1ResponseParse(xrtBufferView(&incoming),&head,&limits,&error);
            if(parsed==XHTTP1_READY){
                if(head.Status<200 || !xrtHttp1ResponseBodyPlan(&head,XRT_STR_LITERAL("POST"),&plan) ||
                    !xrtHttp1BodyInit(&body,&plan,trailers,16,&body_limits))goto done;
                const xhttpfield* encoding=xrtHttp1Field(&head,XRT_STR_LITERAL("Content-Encoding"));
                if(encoding && !xrtStrCaseEqual(encoding->Value,XRT_STR_LITERAL("identity")))goto done;
                *upstream_status=head.Status;
                if(PluginStream_Headers(&config,&head)){result=-3;goto done;}
                offset=head.Bytes; headed=true;
            }else if(parsed!=XHTTP1_MORE || end || incoming.Size>=32768)goto done;
        }
        if(headed){
            for(;;){size_t used=0; xbytesview bytes={0};
                xhttp1bodystatus parsed=xrtHttp1BodyRead(&body,(xbytesview){(cbytes)incoming.Data+offset,incoming.Size-offset},end,&used,&bytes,&error);
                offset+=used;
                if(parsed==XHTTP1_BODY_DATA){if(!used || !bytes.Size)goto done;
                    if(PluginStream_Data(&config,bytes)){result=-3;goto done;}continue;}
                if(parsed==XHTTP1_BODY_DONE){result=0;goto done;}
                if(parsed!=XHTTP1_BODY_MORE || end)goto done;break;
            }
            if(offset){memmove(incoming.Data,(cbytes)incoming.Data+offset,incoming.Size-offset);incoming.Size-=offset;offset=0;}
        }
        if(end || incoming.Size>=65536)goto done;
        future=tls?xrtTlsStreamRecvAsync(tls,8192):xrtNetStreamRecvAsync(tcp,8192);
        if(!PluginStream_Wait(req,future,headed?deadline:first,headed?config.idle_timeout_ms:config.first_byte_timeout_ms))goto done;
        if(xrtFutureState(future)==XFUTURE_CLOSED){
            XA_HttpsFutureDone(future);future=NULL;
            if(tls){future=xrtTlsStreamWaitAsync(tls,XTLS_STREAM_WAIT_END);
                if(!PluginStream_Wait(req,future,deadline,config.idle_timeout_ms)||xrtFutureState(future)!=XFUTURE_RESOLVED)goto done;
            }else if(xrtNetStreamError(tcp))goto done;
            end=true;
        }else if(xrtFutureState(future)==XFUTURE_RESOLVED){
            xnetbytes* bytes=xrtFutureValue(future);xbytesview view=bytes?xrtNetBytesView(bytes):(xbytesview){0};
            if(!view.Size || incoming.Size+view.Size>65536 || !xrtBufferAppend(&incoming,view))goto done;
        }else goto done;
        XA_HttpsFutureDone(future);future=NULL;
    }
done:
    if(result && !PluginStream_PeerOpen(req))result=-3;
    else if(result && ((future && xrtFutureState(future)==XFUTURE_PENDING) ||
        XAdmin_DeadlineExpired(deadline)||(!headed&&XAdmin_DeadlineExpired(first))))result=-2;
    XA_HttpsFutureDone(future);
    if(tls){xrtTlsStreamAbort(tls);xrtTlsStreamDestroy(tls);}if(tcp){xrtNetStreamAbort(tcp);xrtNetStreamDestroy(tcp);}
    if(resolver)xrtNetResolverDestroy(resolver);if(verifier)xrtTlsVerifierRelease(verifier);
    xrtMutexLock(G_RequestLock);inst->activeIo--;
    if(wire.Data)xrtSecureZero(wire.Data,wire.Size);if(incoming.Data)xrtSecureZero(incoming.Data,incoming.Size);
    xrtBufferUnit(&wire);xrtBufferUnit(&incoming);xrtFree(ca_copy);return result;
invalid:
    if(wire.Data)xrtSecureZero(wire.Data,wire.Size);xrtBufferUnit(&wire);xrtBufferUnit(&incoming);xrtFree(ca_copy);return -1;
}
