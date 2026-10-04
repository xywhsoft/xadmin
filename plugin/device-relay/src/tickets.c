static bool Relay_Admission(const RelayDevice* device,bool device_role,bool view,int* status,const char** error)
{
    if(!device->enabled||device->revoked){*status=403;*error="device does not allow remote control";return false;}
    RelayConnection* native=Relay_Native(device->owner,device->id);
    if(device_role){if(native){*status=409;*error="device is already connected";return false;}}
    else{
        if(!native||!native->open){*status=409;*error="device is offline";return false;}
        if(Relay_Peers(device->owner,device->id,view)>=(size_t)(view?Config.viewers:1)){
            *status=409;*error=view?"device viewer limit reached":"device already has a controller";return false;}
    }return true;
}
static void Relay_Ticket(XS_ServerObject server,XS_HostObject host,XS_RequestObject req,XS_ResponseObject response,xvalue* session)
{
    (void)server;(void)host;int64 owner=Relay_Member(response,session);
    if(!owner||!Relay_Method(req,response,XHTTP_METHOD_POST))return;
    if(!Config.enabled){Relay_Reply(response,503,"device relay disabled",NULL);return;}
    Relay_TicketsPrune();xvalue* body=Relay_Body(req);const char* fields[]={"device_id","device_secret","role","mode"};
    const char *id=Relay_Text(body,"device_id",32),*role=Relay_Text(body,"role",16),*secret=Relay_Text(body,"device_secret",64),*mode=Relay_Text(body,"mode",16);
    char token[65]={0},hash[65]={0};RelayDevice device={0};size_t i,slot=RELAY_TICKETS,owned=0;
    bool device_role=false,view=false;int status=400;const char* error="invalid ticket request";xvalue* data=NULL;
    if(!Relay_Fields(body,fields,4)||!Relay_Hex(id,32)||!role||(strcmp(role,"device")&&strcmp(role,"controller")))goto done;
    device_role=!strcmp(role,"device");
    if(device_role){if(!Relay_Hex(secret,64)||ValueHas(body,"mode"))goto done;}
    else{if(ValueHas(body,"device_secret")||!mode||(strcmp(mode,"control")&&strcmp(mode,"view")))goto done;view=!strcmp(mode,"view");}
    int found=Relay_DeviceRead(owner,id,&device);if(found!=1){status=found<0?500:404;error=found<0?"device storage unavailable":"device not found";goto done;}
    if(device_role&&(!Relay_Hash(secret,hash)||!Relay_HashEqual(hash,device.secret_hash))){status=403;error="invalid device proof";goto done;}
    if(!Relay_Admission(&device,device_role,view,&status,&error))goto done;
    for(i=0;i<RELAY_TICKETS;i++){if(!Tickets[i].session&&slot==RELAY_TICKETS)slot=i;if(Tickets[i].session&&Tickets[i].owner==owner)owned++;}
    if(slot==RELAY_TICKETS||owned>=8){status=429;error="pending connection ticket limit reached";goto done;}
    const char* sid=Relay_Text(session,"sid",64);
    if(!Relay_Hex(sid,64)||!Relay_Random(token)||!Relay_Hash(token,hash)){status=500;error="ticket creation failed";goto done;}
    RelayTicket* ticket=&Tickets[slot];ticket->session=ValueObject();
    if(!ticket->session||!ValueSetText(ticket->session,"sid",sid)||!ValueSetInt(ticket->session,"id",owner)){Relay_TicketClear(ticket);status=500;error="allocation failed";goto done;}
    ticket->owner=owner;ticket->device_role=device_role;ticket->view=view;ticket->expires=Relay_Now()+Config.ttl;
    memcpy(ticket->device,id,33);memcpy(ticket->hash,hash,65);
    data=ValueObject();
    if(!data||!ValueSetText(data,"ticket",token)||!ValueSetText(data,"protocol",RELAY_PROTOCOL)||
        !ValueSetText(data,"path","/api/v1/devices/connect")||!ValueSetInt(data,"expires_at",ticket->expires)||
        !ValueSetInt(data,"payload_limit",RELAY_PAYLOAD)){
        xrtValueRelease(data);data=NULL;Relay_TicketClear(ticket);status=500;error="allocation failed";goto done;}
    status=200;error="";
done:
    if(secret)xrtSecureZero((void*)secret,strlen(secret));xrtValueRelease(body);
    xrtSecureZero(token,sizeof(token));xrtSecureZero(hash,sizeof(hash));xrtSecureZero(&device,sizeof(device));Relay_Reply(response,status,error,data);
}
/* Proof is a protocol token, never a URL or cookie. Consumption precedes
 * upgrade validation, so even a failed handshake cannot reuse the proof. */
static void Relay_Connect(XS_ServerObject server,XS_HostObject host,XS_RequestObject req,XS_ResponseObject response,xvalue* ignored)
{
    (void)server;(void)host;(void)ignored;
    if(!Relay_Method(req,response,XHTTP_METHOD_GET))return;
    if(!Config.enabled){Relay_Reply(response,503,"device relay disabled",NULL);return;}
    Relay_TicketsPrune();char token[65]={0},hash[65]={0},protocols[513]={0};
    int protocol_size=XAdmin_ReqHeaderCopy(req,"Sec-WebSocket-Protocol",protocols,sizeof(protocols));
    size_t i;RelayTicket consumed={0};RelayDevice device={0};RelayConnection* connection=NULL;
    int status=401;const char* error="invalid connection ticket";
    if(protocol_size<=0||!xrtWsProtocolsValid(xrtStrViewN(protocols,(size_t)protocol_size)))goto done;
    const char* cursor=protocols;bool duplicate=false;
    while(*cursor){
        while(*cursor==' '||*cursor=='\t')cursor++;const char* end=strchr(cursor,',');if(!end)end=cursor+strlen(cursor);
        const char* tail=end;while(tail>cursor&&(tail[-1]==' '||tail[-1]=='\t'))tail--;
        if((size_t)(tail-cursor)==78&&!memcmp(cursor,"xadmin.ticket.",14)){
            if(token[0])duplicate=true;memcpy(token,cursor+14,64);token[64]=0;}
        cursor=*end?end+1:end;
    }
    if(duplicate||!Relay_Hex(token,64)||!Relay_Hash(token,hash))goto done;
    for(i=0;i<RELAY_TICKETS;i++)if(Tickets[i].session&&Relay_HashEqual(hash,Tickets[i].hash)){
        consumed=Tickets[i];memset(&Tickets[i],0,sizeof(Tickets[i]));break;}
    if(!consumed.session)goto done;
    int found=Relay_DeviceRead(consumed.owner,consumed.device,&device);
    if(found!=1){status=found<0?500:404;error=found<0?"device storage unavailable":"device not found";goto done;}
    if(!Relay_Admission(&device,consumed.device_role,consumed.view,&status,&error))goto done;
    for(i=0;i<RELAY_CONNECTIONS;i++)if(!Connections[i].channel){connection=&Connections[i];break;}
    if(!connection){status=503;error="relay connection limit reached";goto done;}
    connection->owner=consumed.owner;connection->device_role=consumed.device_role;connection->view=consumed.view;
    memcpy(connection->device,consumed.device,33);
    if(!consumed.device_role){
        RelayConnection* native=Relay_Native(consumed.owner,consumed.device);connection->target=native->channel;
        if(!xrtSecureRandom(connection->peer_bytes,16)){status=500;error="peer creation failed";goto done;}
        Relay_HexWrite(connection->peer_bytes,16,connection->peer);
    }
    XAdminChannelConfig config={0};config.size=sizeof(config);config.protocol=RELAY_PROTOCOL;
    config.message_limit=RELAY_MESSAGE;config.queue_limit=1024u*1024u;config.allow_cross_origin=true;
    config.data=connection;config.on_open=Relay_Open;config.on_message=Relay_Message;config.on_close=Relay_Close;
    if(XAdmin_ChannelAccept(Handle,req,consumed.session,&config,&connection->channel)==0){
        Relay_TicketClear(&consumed);xrtSecureZero(token,sizeof(token));xrtSecureZero(hash,sizeof(hash));xrtSecureZero(&device,sizeof(device));return;
    }
    status=403;error="connection rejected";
done:
    if(connection)xrtSecureZero(connection,sizeof(*connection));Relay_TicketClear(&consumed);
    xrtSecureZero(token,sizeof(token));xrtSecureZero(hash,sizeof(hash));xrtSecureZero(&device,sizeof(device));Relay_Reply(response,status,error,NULL);
}
