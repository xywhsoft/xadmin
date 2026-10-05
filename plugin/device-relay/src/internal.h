/* All plugin state is serialized by xadmin. No I/O wait or payload logging. */
#define RELAY_CONNECTIONS 32
#define RELAY_TICKETS 128
#define RELAY_HEADER 21u
#define RELAY_MESSAGE (256u*1024u)
#define RELAY_PAYLOAD (RELAY_MESSAGE-RELAY_HEADER)
#define RELAY_PROTOCOL "xadmin.device-relay.v1"
typedef struct RelayConfig {bool enabled;int devices,viewers,ttl;} RelayConfig;
typedef struct RelayDevice {
    char id[33],secret_hash[65],name[97],platform[25],version[49];
    int64 owner,created,updated,last_seen;
    bool enabled,revoked;
} RelayDevice;
typedef struct RelayTicket {
    char hash[65],device[33];
    xvalue* session;
    int64 owner,expires;
    bool device_role,view;
} RelayTicket;
typedef struct RelayConnection {
    XAdminChannel channel,target;
    int64 owner;
    char device[33],peer[33];uint8 peer_bytes[16];
    bool device_role,view,open;
} RelayConnection;
static XAdminPluginHandle Handle;
static XAdminHostContext* Host;
static sqlite3* Database;
static RelayConfig Config;
static RelayTicket Tickets[RELAY_TICKETS];
static RelayConnection Connections[RELAY_CONNECTIONS];

static int64 Relay_Now(void){return xrtNow()/1000000;}
static bool Relay_Hex(const char* text,size_t length)
{
    size_t i;if(!text||strlen(text)!=length)return false;
    for(i=0;i<length;i++)if(!((text[i]>='0'&&text[i]<='9')||(text[i]>='a'&&text[i]<='f')))return false;
    return true;
}
static void Relay_HexWrite(const uint8* bytes,size_t size,char* out)
{
    static const char digits[]="0123456789abcdef";size_t i;
    for(i=0;i<size;i++){out[i*2]=digits[bytes[i]>>4];out[i*2+1]=digits[bytes[i]&15];}out[size*2]=0;
}
static bool Relay_Random(char out[65])
{
    uint8 bytes[32];bool ok=xrtSecureRandom(bytes,sizeof(bytes));
    if(ok)Relay_HexWrite(bytes,sizeof(bytes),out);else out[0]=0;
    xrtSecureZero(bytes,sizeof(bytes));return ok;
}
static bool Relay_Hash(const char* text,char out[65])
{
    uint8 bytes[32];if(!text||!xrtSha256(text,strlen(text),bytes))return false;
    Relay_HexWrite(bytes,sizeof(bytes),out);xrtSecureZero(bytes,sizeof(bytes));return true;
}
static bool Relay_HashEqual(const char* left,const char* right)
{
    size_t i;volatile uint8 difference=0;
    for(i=0;i<64;i++)difference|=(uint8)left[i]^(uint8)right[i];return difference==0;
}
static const char* Relay_Text(const xvalue* object,const char* key,size_t max)
{
    xstrview text={0};if(!xrtValueGetString(ValueGet(object,key),&text)||text.Size>max||
        memchr(text.Data,0,text.Size)||!xrtUtf8Valid(text,NULL))return NULL;return text.Data;
}
static bool Relay_Fields(const xvalue* object,const char* const* allowed,size_t count)
{
    if(!object||xrtValueType(object)!=XVALUE_OBJECT)return false;
    xvalueiter iter={0};xvaluekey key;xvalue* item;bool ok=true;
    if(xrtValueIterBegin(object,&iter)){
        while((item=xrtValueIterNext(&iter,&key))){size_t i;
            for(i=0;i<count;i++)if(xrtStrEqual(key.String,xrtStrView(allowed[i])))break;
            if(i==count){ok=false;break;}}
        xrtValueIterEnd(&iter);
    }return ok;
}
static xvalue* Relay_Body(XS_RequestObject req)
{
    size_t size=XAdmin_ReqBodyLen(req);const char* bytes=XAdmin_ReqBody(req);
    if(!bytes||!size||size>4096||memchr(bytes,0,size))return NULL;
    xjsonreadconfig config;xrtJsonReadConfigInit(&config);config.MaxInputBytes=4096;
    config.MaxDepth=3;config.MaxValues=32;config.MaxContainerItems=16;
    return xrtJsonRead(xrtStrViewN(bytes,size),&config);
}
static void Relay_Reply(XS_ResponseObject response,int status,const char* message,xvalue* data)
{
    xvalue* result=ValueObject();size_t size=0;
    bool ok=result&&ValueSetInt(result,"code",status<400?0:status)&&ValueSetText(result,"message",message);
    if(data){if(ok)ok=ValueSetOwn(result,"data",data);else xrtValueRelease(data);}
    char* json=ok?xrtJsonStringify(result,false,&size):NULL;
    xsHttpReplyAuto(response,json?status:500,"Content-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\n",
        json?json:"{\"code\":500,\"message\":\"allocation failed\"}",json?size:0);
    if(json)xrtSecureZero(json,size);xrtFree(json);xrtValueRelease(result);
}
static int64 Relay_Member(XS_ResponseObject response,xvalue* session)
{
    int64 owner=ValueInt(session,"id");
    if(owner<=0||XAdmin_MemberContactStatus(session)<0){Relay_Reply(response,401,"member login required",NULL);return 0;}
    return owner;
}
static bool Relay_Method(XS_RequestObject req,XS_ResponseObject response,xhttpmethod method)
{
    if(xsReqMethodID(req)==method)return true;
    Relay_Reply(response,405,"method not allowed",NULL);return false;
}
static void Relay_TicketClear(RelayTicket* ticket)
{xrtValueRelease(ticket->session);xrtSecureZero(ticket,sizeof(*ticket));}
static void Relay_TicketsPrune(void)
{size_t i;int64 now=Relay_Now();for(i=0;i<RELAY_TICKETS;i++)if(Tickets[i].session&&Tickets[i].expires<=now)Relay_TicketClear(&Tickets[i]);}
