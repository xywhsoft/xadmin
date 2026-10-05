/* The relay adds trusted peer identity. Controller-supplied routing fields are
 * never interpreted. Native replies select only peers bound to this channel.
 * Wire: MDR1 + 16 random peer bytes + kind (1 text, 2 binary) + opaque payload. */
static RelayConnection* Relay_Native(int64 owner,const char* device)
{
    size_t i;for(i=0;i<RELAY_CONNECTIONS;i++)if(Connections[i].channel&&Connections[i].device_role&&
        Connections[i].owner==owner&&!strcmp(Connections[i].device,device))return &Connections[i];return NULL;
}
static size_t Relay_Peers(int64 owner,const char* device,bool view)
{
    size_t i,count=0;for(i=0;i<RELAY_CONNECTIONS;i++)if(Connections[i].channel&&!Connections[i].device_role&&
        Connections[i].view==view&&Connections[i].owner==owner&&!strcmp(Connections[i].device,device))count++;return count;
}
static bool Relay_SendValue(XAdminChannel channel,xvalue* value)
{
    size_t size=0;char* json=value?xrtJsonStringify(value,false,&size):NULL;
    bool ok=json&&XAdmin_ChannelSend(Handle,channel,false,json,size)==0;
    xrtFree(json);xrtValueRelease(value);return ok;
}
static bool Relay_Notice(XAdminChannel channel,const char* type,const RelayConnection* peer)
{
    xvalue* value=ValueObject();bool ok=value&&ValueSetText(value,"type",type)&&ValueSetInt(value,"version",1)&&
        ValueSetText(value,"peer_id",peer->peer)&&ValueSetText(value,"mode",peer->view?"view":"control")&&
        ValueSetText(value,"device_id",peer->device)&&ValueSetInt(value,"payload_limit",RELAY_PAYLOAD);
    if(!ok){xrtValueRelease(value);return false;}return Relay_SendValue(channel,value);
}
static void Relay_Open(XAdminChannel channel,void* data)
{
    RelayConnection* connection=data;
    if(connection->channel!=channel||!Relay_DeviceAllowed(connection)){XAdmin_ChannelClose(Handle,channel,1008);return;}
    connection->open=true;
    if(connection->device_role){
        Relay_Seen(connection);
        if(!Relay_Notice(channel,"ready",connection))XAdmin_ChannelClose(Handle,channel,1013);
    }else{
        RelayConnection* native=Relay_Native(connection->owner,connection->device);
        if(!native||!native->open||native->channel!=connection->target||
            !Relay_Notice(native->channel,"peer_open",connection)||!Relay_Notice(channel,"ready",connection))
            XAdmin_ChannelClose(Handle,channel,1013);
    }
}
static void Relay_Close(XAdminChannel channel,uint16_t code,void* data)
{
    RelayConnection* connection=data;size_t i;(void)code;
    if(connection->channel!=channel)return;
    if(connection->device_role){
        Relay_Seen(connection);
        for(i=0;i<RELAY_CONNECTIONS;i++)if(Connections[i].channel&&!Connections[i].device_role&&Connections[i].target==channel)
            XAdmin_ChannelClose(Handle,Connections[i].channel,1001);
    }else{
        RelayConnection* native=Relay_Native(connection->owner,connection->device);
        if(connection->open&&native&&native->open&&native->channel==connection->target&&!Relay_Notice(native->channel,"peer_close",connection))
            XAdmin_ChannelClose(Handle,native->channel,1013);
    }
    xrtSecureZero(connection,sizeof(*connection));
}
static void Relay_Message(XAdminChannel channel,bool binary,const void* bytes,size_t size,void* data)
{
    RelayConnection* connection=data;const uint8* payload=bytes;size_t i;
    if(connection->channel!=channel||!connection->open||!Config.enabled||!Relay_DeviceAllowed(connection)){
        XAdmin_ChannelClose(Handle,channel,1008);return;}
    if(connection->device_role){
        if(!binary||size<RELAY_HEADER||memcmp(payload,"MDR1",4)||(payload[20]!=1&&payload[20]!=2)){
            XAdmin_ChannelClose(Handle,channel,1002);return;}
        for(i=0;i<RELAY_CONNECTIONS;i++){
            RelayConnection* peer=&Connections[i];
            if(peer->channel&&peer->open&&!peer->device_role&&peer->target==channel&&!memcmp(payload+4,peer->peer_bytes,16)){
                if(XAdmin_ChannelSend(Handle,peer->channel,payload[20]==2,payload+RELAY_HEADER,size-RELAY_HEADER)!=0)
                    XAdmin_ChannelClose(Handle,peer->channel,1013);
                return;
            }
        }
        /* A disconnected peer may have a response in flight. Drop it rather
         * than routing it to a new controller or tearing down other peers. */
    }else{
        RelayConnection* native=Relay_Native(connection->owner,connection->device);
        if(!native||!native->open||native->channel!=connection->target||size>RELAY_PAYLOAD){
            XAdmin_ChannelClose(Handle,channel,1009);return;}
        uint8* frame=xrtMalloc(RELAY_HEADER+size);if(!frame){XAdmin_ChannelClose(Handle,channel,1013);return;}
        memcpy(frame,"MDR1",4);memcpy(frame+4,connection->peer_bytes,16);frame[20]=binary?2:1;
        if(size)memcpy(frame+RELAY_HEADER,bytes,size);
        int result=XAdmin_ChannelSend(Handle,native->channel,true,frame,RELAY_HEADER+size);
        xrtSecureZero(frame,RELAY_HEADER+size);xrtFree(frame);
        if(result!=0){XAdmin_ChannelClose(Handle,channel,1013);XAdmin_ChannelClose(Handle,native->channel,1013);}
    }
}
static void Relay_Revoke(int64 owner,const char* device)
{
    size_t i;for(i=0;i<RELAY_TICKETS;i++)if(Tickets[i].session&&Tickets[i].owner==owner&&!strcmp(Tickets[i].device,device))Relay_TicketClear(&Tickets[i]);
    for(i=0;i<RELAY_CONNECTIONS;i++)if(Connections[i].channel&&Connections[i].owner==owner&&!strcmp(Connections[i].device,device))
        XAdmin_ChannelClose(Handle,Connections[i].channel,1008);
}
