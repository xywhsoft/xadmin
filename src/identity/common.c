/* Identity services run under G_RequestLock except explicitly bracketed CPU/I/O
 * work. Every SQL operation owns its statement; no bound request data escapes. */
static int64 XA_Now(void) { return xrtNow() / 1000000; }
static bool XA_Hex(const void* data, size_t size, char* out)
{
    const unsigned char* bytes = data; size_t i;
    for (i=0;i<size;i++) snprintf(out+i*2,3,"%02x",bytes[i]);
    return true;
}
static bool XA_Random(char out[65])
{
    unsigned char bytes[32]; bool ok=xrtSecureRandom(bytes,sizeof(bytes));
    if (ok) XA_Hex(bytes,sizeof(bytes),out); else out[0]=0;
    xrtSecureZero(bytes,sizeof(bytes));return ok;
}
static bool XA_Hash(const char* text, char out[65])
{
    unsigned char digest[32];
    if (!text || !xrtSha256(text,strlen(text),digest)) return false;
    return XA_Hex(digest,32,out);
}
static bool XA_IsHex(const char* text, size_t length)
{
    size_t i;if (!text || strlen(text)!=length)return false;
    for(i=0;i<length;i++)if(!((text[i]>='0'&&text[i]<='9')||(text[i]>='a'&&text[i]<='f')))return false;
    return true;
}
static const char* XA_Text(const xvalue* object, const char* key, size_t max)
{
    xstrview v={0};
    if(!xrtValueGetString(ValueGet(object,key),&v)||v.Size>max||memchr(v.Data,0,v.Size))return NULL;
    return v.Data;
}
static sqlite3_stmt* XA_SQL(const char* sql)
{
    sqlite3_stmt* s=NULL;
    if(sqlite3_prepare_v2(G_DB,sql,-1,&s,NULL)!=SQLITE_OK)return NULL;
    return s;
}
static bool XA_Done(sqlite3_stmt* s, bool require_row)
{
    bool ok=s&&sqlite3_step(s)==SQLITE_DONE&&(!require_row||sqlite3_changes(G_DB)>0);
    sqlite3_finalize(s);return ok;
}
static bool XA_Begin(void) { return sqlite3_exec(G_DB,"SAVEPOINT xa_identity",NULL,NULL,NULL)==SQLITE_OK; }
static bool XA_End(bool ok)
{
    if(ok&&sqlite3_exec(G_DB,"RELEASE xa_identity",NULL,NULL,NULL)==SQLITE_OK)return true;
    sqlite3_exec(G_DB,"ROLLBACK TO xa_identity",NULL,NULL,NULL);
    sqlite3_exec(G_DB,"RELEASE xa_identity",NULL,NULL,NULL);return false;
}
static void XA_BindText(sqlite3_stmt* s,int index,const char* text)
{
    if(s)sqlite3_bind_text(s,index,text,-1,SQLITE_TRANSIENT);
}
static bool XA_CopyColumn(sqlite3_stmt* s,int column,char* out,size_t cap)
{
    const char* p=(const char*)sqlite3_column_text(s,column);int n=sqlite3_column_bytes(s,column);
    if(!p){out[0]=0;return true;}
    if(n<0||(size_t)n>=cap||memchr(p,0,(size_t)n))return false;
    memcpy(out,p,(size_t)n);out[n]=0;return true;
}
static bool XA_Reply(XAdminRequest* req,int status,const char* message,xvalue* data,const char* extra)
{
    xvalue* result=ValueObject();char* body;size_t size=0;bool ok;
    if(!result)return false;
    ok=ValueSetInt(result,"code",status<400?0:status)&&ValueSetText(result,"msg",message?message:"");
    if(data)ok=ok&&ValueSetRef(result,"data",data);
    else ok=ok&&ValueSetOwn(result,"data",xrtValueNull());
    body=ok?xrtJsonStringify(result,false,&size):NULL;
    if(body){char* headers=xrtFormat("%sCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n%s",HTTP_CT_JSON,extra?extra:"");
        ok=headers&&xsHttpReplyAuto(req,status,headers,body,size)>=0;xrtFree(headers);}
    else ok=false;
    xrtFree(body);xrtValueRelease(result);return ok;
}
static const xhttpfield* XA_Header(XAdminRequest* req,const char* name,bool* invalid)
{
    size_t i;const xhttpfield* found=NULL;
    for(i=0;i<req->raw->head->FieldCount;i++){
        const xhttpfield* f=&req->raw->head->Fields[i];
        if(xrtStrCaseEqual(f->Name,xrtStrView(name))){if(found){*invalid=true;return NULL;}found=f;}
    }
    return found;
}
/* Cookie credentials must not be truncated or resolved by first-match wins. */
static int XA_Cookie(XAdminRequest* req,const char* name,char* out,size_t capacity)
{
    size_t i;int result=-1;out[0]=0;
    for(i=0;i<req->raw->head->FieldCount;i++){
        const xhttpfield* field=&req->raw->head->Fields[i];
        if(!xrtStrCaseEqual(field->Name,XRT_STR_LITERAL("Cookie")))continue;
        if(field->Value.Size>8192||memchr(field->Value.Data,0,field->Value.Size))return -2;
        const char* item=field->Value.Data;const char* limit=item+field->Value.Size;
        while(item<limit){
            const char* end=memchr(item,';',(size_t)(limit-item));if(!end)end=limit;
            while(item<end&&(*item==' '||*item=='\t'))item++;
            const char* equal=memchr(item,'=',(size_t)(end-item));
            if(equal&&(size_t)(equal-item)==strlen(name)&&!memcmp(item,name,strlen(name))){
                const char* value=equal+1;const char* tail=end;
                while(tail>value&&(tail[-1]==' '||tail[-1]=='\t'))tail--;
                size_t n=(size_t)(tail-value);
                if(result!=-1||n>=capacity)return -2;
                memcpy(out,value,n);out[n]=0;result=(int)n;
            }
            item=end<limit?end+1:limit;
        }
    }
    return result;
}
static xvalue* XA_Body(XAdminRequest* req)
{
    bool bad=false;const xhttpfield* ct=XA_Header(req,"Content-Type",&bad);xvalue* body;
    xjsonreadconfig config;
    if(bad||!ct||ct->Value.Size<16||memcmp(ct->Value.Data,"application/json",16)||
       (ct->Value.Size>16&&ct->Value.Data[16]!=';')||!req->body||req->body_size>8192)return NULL;
    xrtJsonReadConfigInit(&config);config.MaxInputBytes=8192;config.MaxDepth=4;
    config.MaxValues=64;config.MaxStringBytes=4096;config.MaxContainerItems=32;
    body=xrtJsonRead(xrtStrViewN(req->body,req->body_size),&config);
    if(xrtValueType(body)!=XVALUE_OBJECT){xrtValueRelease(body);return NULL;}return body;
}
