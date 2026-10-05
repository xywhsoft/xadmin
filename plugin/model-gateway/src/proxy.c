/* One native request per operation. Financial finalization is retryable;
 * generation is deliberately not retried after transmission may have begun. */
static void Gateway_Error(XS_ResponseObject resp,int status,const char* code,const char* message,const char* id)
{
    xvalue* root=ValueObject(),*error=ValueObject();
    ValueSetText(error,"type",status==401?"authentication_error":"gateway_error");
    ValueSetText(error,"code",code);ValueSetText(error,"message",message);
    if(id && *id){ValueSetText(error,"request_id",id);char path[160];snprintf(path,sizeof(path),"/api/v1/ai/requests/%s",id);ValueSetText(error,"receipt_url",path);}
    ValueSetOwn(root,"error",error);char* text=xrtJsonStringify(root,false,NULL);
    xsHttpReplyAuto(resp,status,"Content-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\n",text?text:"{}",text?strlen(text):2);
    xrtFree(text);xrtValueRelease(root);
}
static bool Gateway_HeaderName(xstrview value,const char* expected)
{
    size_t n=strlen(expected),i;if(value.Size!=n)return false;
    for(i=0;i<n;i++){unsigned char c=value.Data[i];if(c>='A'&&c<='Z')c+='a'-'A';if(c!=(unsigned char)expected[i])return false;}return true;
}
static bool Gateway_MediaType(xstrview value,const char* expected)
{
    size_t n=strlen(expected);if(value.Size<n)return false;
    return Gateway_HeaderName((xstrview){value.Data,n},expected) && (value.Size==n || value.Data[n]==';' || value.Data[n]==' ');
}
static int Gateway_ActiveCount(int64_t owner,const char* channel)
{
    int i,count=0;for(i=0;i<16;i++)if(G_Active[i].owner && (!owner || G_Active[i].owner==owner) && (!channel || !strcmp(channel,G_Active[i].channel)))count++;return count;
}
static int Gateway_Select(GatewayCall* call,char key[1001])
{
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT channel_id,wire_model,cost_json FROM route WHERE model_id=? AND protocol=? ORDER BY priority,channel_id");
    XP_Bind(s,1,call->model.id);XP_Bind(s,2,call->protocol);xvalue* rows=XP_Rows(s);if(!rows)return 503;
    int status=503;uint32 i;
    for(i=0;i<ValueCount(rows);i++){
        xvalue* row=xrtValueArrayGet(rows,i);GatewayChannel channel;
        if(!Gateway_ChannelRead(ValueText(row,"channel_id"),&channel) || !channel.enabled || !Gateway_Key(&channel,key))continue;
        if(Gateway_ActiveCount(0,channel.id)>=channel.max_concurrent){status=429;xrtSecureZero(key,1001);continue;}
        call->channel=channel;snprintf(call->wire_model,sizeof(call->wire_model),"%s",ValueText(row,"wire_model"));
        const char* cost=ValueText(row,"cost_json");
        if(cost){xvalue* value=xrtJsonParse(xrtStrView(cost));call->model.price.cost_known=Gateway_RatesRead(value,&call->model.price.cost);xrtValueRelease(value);
            if(!call->model.price.cost_known){xrtSecureZero(key,1001);status=503;break;}}
        status=0;break;
    }xrtValueRelease(rows);return status;
}
static void Gateway_Sending(void* data){((GatewayCall*)data)->sent=true;}
static int Gateway_Headers(void* data,uint16_t status,const xhttpfield* fields,size_t count)
{
    GatewayCall* call=data;call->upstream_status=status;
    if(status<200 || status>=300){snprintf(call->error,sizeof(call->error),"upstream_http_error");return -1;}
    bool json=false,sse=false;size_t i;
    for(i=0;i<count;i++){
        if(Gateway_HeaderName(fields[i].Name,"content-type")){
            json=Gateway_MediaType(fields[i].Value,"application/json");sse=Gateway_MediaType(fields[i].Value,"text/event-stream");}
        if(Gateway_HeaderName(fields[i].Name,"x-request-id") || Gateway_HeaderName(fields[i].Name,"request-id")){
            xstrview id=fields[i].Value;size_t j;bool valid=id.Size>0 && id.Size<sizeof(call->provider_request_id);
            for(j=0;valid && j<id.Size;j++)if((unsigned char)id.Data[j]<33 || (unsigned char)id.Data[j]>126)valid=false;
            if(valid){memcpy(call->provider_request_id,id.Data,id.Size);call->provider_request_id[id.Size]=0;}}
    }
    if(call->stream?!sse:!json){snprintf(call->error,sizeof(call->error),"upstream_content_type");return -1;}
    call->sse=sse;call->sse_first_line=true;call->started=true;
    xhttpfield out[]={
        {XRT_STR_LITERAL("Content-Type"),sse?XRT_STR_LITERAL("text/event-stream; charset=utf-8"):XRT_STR_LITERAL("application/json; charset=utf-8")},
        {XRT_STR_LITERAL("Cache-Control"),XRT_STR_LITERAL("no-store")},
        {XRT_STR_LITERAL("X-Accel-Buffering"),XRT_STR_LITERAL("no")},
        {XRT_STR_LITERAL("X-Request-Id"),xrtStrView(call->id)},
        {XRT_STR_LITERAL("Access-Control-Expose-Headers"),XRT_STR_LITERAL("X-Request-Id")}
    };
    return XAdmin_StreamBegin(call->req,status,out,5,(unsigned)call->channel.timeout_ms);
}
static int Gateway_Data(void* data,const void* bytes,size_t size)
{
    GatewayCall* call=data;if(!call->first_byte_us)call->first_byte_us=xrtNow();
    if(!Gateway_MeterFeed(call,bytes,size)){snprintf(call->error,sizeof(call->error),"upstream_invalid_body");return -1;}
    return XAdmin_StreamWrite(call->req,bytes,size);
}
static bool Gateway_Verified(const GatewayCall* call)
{
    const GatewayUsage* u=&call->usage;
    return !u->invalid && u->authoritative && u->input_known && u->output_known &&
        u->input+u->cache_read+u->cache_write_5m+u->cache_write_1h<=call->model.context_window && u->output<=call->output_limit;
}
static void Gateway_Work(XS_ServerObject server,XS_HostObject host,XS_RequestObject req,XS_ResponseObject resp,xvalue* session)
{
    (void)server;(void)host;GatewayCall call={0};call.req=req;call.started_us=xrtNow();call.owner=ValueInt(session,"id");
    char secret[1001]={0},authorization[1010]={0},key[97]={0},hash[65],project[129],session_id[129],task[129];
    xvalue* body=NULL;char* wire=NULL;size_t wire_size=0;int slot=-1,status=400;const char* code="invalid_request",*message="Invalid or unsupported request";
    if(call.owner<=0 || XAdmin_MemberContactStatus(session)<0){status=401;code="login_required";message="Member login required";goto done;}
    if(!Gateway_Sync(false)){status=503;code="reconciliation_unavailable";message="Billing reconciliation unavailable";goto done;}
    const char* path=XAdmin_ReqPath(req);snprintf(call.protocol,sizeof(call.protocol),"%s",!strcmp(path,"/api/v1/ai/responses")?"responses":!strcmp(path,"/api/v1/ai/messages")?"anthropic":"chat");
    body=XP_Body(req,1048576);if(!body || xrtValueType(body)!=XVALUE_OBJECT || !Gateway_Fingerprint(body,hash))goto done;
    const char* model_id=XP_Text(body,"model",128);if(!XP_Id(model_id,128))goto done;
    int copied=XAdmin_ReqHeaderCopy(req,"Idempotency-Key",key,sizeof(key));
    if(copied<-1 || copied==0 || (copied>0 && !XP_Id(key,96)))goto done;
    if(!*key && !XP_Random(key)){status=503;goto done;}
    status=Gateway_Duplicate(call.owner,key,hash,call.protocol,call.id);
    if(status){code=status==409?"request_already_exists":"idempotency_conflict";message="Use the existing request receipt; generation is not replayed";goto done;}
    if(!Gateway_ModelRead(model_id,&call.model) || !call.model.enabled){status=404;code="model_unavailable";message="Model is unavailable";goto done;}
    if(ValueHas(body,"xadmin_expected_price_version")){int64_t version;
        if(!XP_Int(body,"xadmin_expected_price_version",1,INT64_MAX,&version)){status=400;goto done;}
        if(version!=call.model.price.version){status=409;code="price_changed";message="Refresh the model catalog before requesting this model";goto done;}}
    XBillingEntitlement entitlement={sizeof(entitlement)};XBillingAccount account={sizeof(account)};XAdminServiceLease lease;
    const XBillingService* billing=Gateway_Billing(&lease);
    status=billing?billing->account(call.owner,&account):503;if(!status)status=billing->entitlement(call.owner,&entitlement);
    if(lease)XAdmin_ReleaseService(lease);
    if(status){code="billing_unavailable";message="Billing service unavailable";goto done;}
    if(!Gateway_AllowedModel(&call.model,&entitlement)){status=403;code="membership_required";message="Membership does not include this model";goto done;}
    if(Gateway_ActiveCount(0,NULL)>=G_Config.max_concurrent || Gateway_ActiveCount(call.owner,NULL)>=entitlement.concurrency_limit){status=429;code="concurrency_limit";message="Another request is still active";goto done;}
    status=Gateway_Select(&call,secret);if(status){code="channel_unavailable";message="No configured channel is currently available";goto done;}
    call.discount_bps=entitlement.discount_bps;
    if(!Gateway_ValidateBody(&call,body,project,session_id,task)){status=400;goto done;}
    if(!Gateway_ReserveAmount(&call,&call.reserved) || !XP_Random(call.id)){status=503;code="price_overflow";message="Model price exceeds supported billing limits";goto done;}
    wire=xrtJsonStringify(body,false,&wire_size);if(!wire || wire_size>1048576){status=400;goto done;}
    status=Gateway_Admit(&call,key,hash,project,session_id,task);
    if(status){code=status==429?"quota_exceeded":"admission_failed";message="Request quota or daily spending limit reached";goto done;}
    billing=Gateway_Billing(&lease);
    XBillingReservation reservation={sizeof(reservation),call.owner,call.reserved,(int64_t)time(NULL)+call.channel.timeout_ms/1000+3600,call.id,"model-gateway",call.model.id};
    status=billing?billing->reserve(&reservation):503;if(lease)XAdmin_ReleaseService(lease);
    if(status){Gateway_State(call.id,"rejected","reservation_rejected");code=status==402?"insufficient_balance":"billing_unavailable";message=status==402?"Insufficient available balance for this model's maximum request cost":"Billing reservation unavailable";goto done;}
    if(!Gateway_State(call.id,"running","")){
        XBillingSettlement release={sizeof(release),call.id,0,"{}","released"};Gateway_BillingFinish(&release);status=503;code="storage_unavailable";goto done;}
    int i;for(i=0;i<16;i++)if(!G_Active[i].owner){slot=i;break;}
    if(slot<0){XBillingSettlement release={sizeof(release),call.id,0,"{}","released"};Gateway_BillingFinish(&release);Gateway_State(call.id,"released","concurrency_limit");status=429;goto done;}
    G_Active[slot].owner=call.owner;snprintf(G_Active[slot].channel,sizeof(G_Active[slot].channel),"%s",call.channel.id);
    xhttpfield headers[4];size_t count=0;
    /* Framing, Content-Type and Accept are supplied by the host transport. */
    if(strcmp(call.channel.auth,"none")){
        const char* name="Authorization";if(!strcmp(call.channel.auth,"bearer"))snprintf(authorization,sizeof(authorization),"Bearer %s",secret);
        else{snprintf(authorization,sizeof(authorization),"%s",secret);name=!strcmp(call.channel.auth,"x-api-key")?"x-api-key":"api-key";}
        headers[count++]=(xhttpfield){xrtStrView(name),xrtStrView(authorization)};}
    if(!strcmp(call.protocol,"anthropic"))headers[count++]=(xhttpfield){XRT_STR_LITERAL("anthropic-version"),xrtStrView(call.channel.anthropic_version)};
    XAdminHttpStreamConfig config={0};config.size=sizeof(config);config.url=call.channel.url;config.headers=headers;config.header_count=count;
    config.body=wire;config.body_size=wire_size;config.timeout_ms=call.channel.timeout_ms;config.first_byte_timeout_ms=call.channel.first_byte_ms;
    config.idle_timeout_ms=call.channel.idle_ms;config.max_response=16777216;config.allow_http=call.channel.allow_http;
    config.data=&call;config.on_send=Gateway_Sending;config.on_headers=Gateway_Headers;config.on_data=Gateway_Data;
    int transport=XAdmin_HttpStream(G_Handle,req,&config,&call.upstream_status);
    bool complete=transport==0 && Gateway_MeterFinish(&call),verified=Gateway_Verified(&call);int64_t amount=0;
    if(verified && !Gateway_Calculate(&call.usage,&call.model.price.sale,call.discount_bps,&amount))verified=false;
    const char* outcome=verified?"settled":!call.sent || (call.upstream_status && (call.upstream_status<200 || call.upstream_status>=300))?"released":"pending";
    if(!call.error[0] && (!complete || !verified))snprintf(call.error,sizeof(call.error),"%s",transport==-2?"upstream_timeout":!complete?"stream_interrupted":"usage_unresolved");
    char* usage_text=NULL;bool saved=Gateway_FinalRecord(&call,outcome,verified?amount:0,verified,call.error,&usage_text);
    if(saved){XBillingSettlement settlement={sizeof(settlement),call.id,verified?amount:0,usage_text,outcome};
        int finalized=Gateway_BillingFinish(&settlement);if(!finalized){billing=Gateway_Billing(&lease);XBillingResult result={sizeof(result)};
            if(billing && !billing->lookup(call.id,&result))Gateway_Receipt(call.id,&result);if(lease)XAdmin_ReleaseService(lease);}}
    /* If the receipt write failed, leave the durable billing hold for recovery.
     * Never report a successful end when local metering/storage is incomplete. */
    xrtFree(usage_text);
    if(call.started)XAdmin_StreamFinish(req,complete && saved);
    else{status=transport==-2?504:502;code=call.error[0]?call.error:"upstream_unavailable";message="Upstream model request failed";}
done:
    if(!call.started)Gateway_Error(resp,status,code,message,call.id);
    if(slot>=0)memset(&G_Active[slot],0,sizeof(G_Active[slot]));
    if(wire)xrtSecureZero(wire,wire_size);xrtFree(wire);xrtValueRelease(body);xrtValueRelease(call.reported_usage);
    if(call.line.Data)xrtSecureZero(call.line.Data,call.line.Size);if(call.event.Data)xrtSecureZero(call.event.Data,call.event.Size);if(call.json.Data)xrtSecureZero(call.json.Data,call.json.Size);
    xrtBufferUnit(&call.line);xrtBufferUnit(&call.event);xrtBufferUnit(&call.json);xrtSecureZero(secret,sizeof(secret));xrtSecureZero(authorization,sizeof(authorization));
}
static void Gateway_Request(XS_ServerObject server,XS_HostObject host,XS_RequestObject req,XS_ResponseObject resp,xvalue* session)
{
    (void)server;(void)host;
    if(xsReqMethodID(req)!=XHTTP_METHOD_POST){Gateway_Error(resp,405,"method_not_allowed","POST required",NULL);return;}
    if(XAdmin_ReqBodyLen(req)>1048576){Gateway_Error(resp,413,"request_too_large","Request body exceeds 1 MiB",NULL);return;}
    if(!session || ValueInt(session,"id")<=0 || XAdmin_MemberContactStatus(session)<0){Gateway_Error(resp,401,"login_required","Member login required",NULL);return;}
    if(XAdmin_DeferRoute(G_Handle,req,session,Gateway_Work))Gateway_Error(resp,503,"gateway_busy","Gateway is temporarily busy",NULL);
}
