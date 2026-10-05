static void Gateway_WipeKeys(xvalue* keys)
{
    xvalueiter it={0};xvaluekey key;xvalue* value;
    if(xrtValueIterBegin(keys,&it)){while((value=xrtValueIterNext(&it,&key))){xstrview text={0};
        if(xrtValueGetString(value,&text))xrtSecureZero((void*)text.Data,text.Size);}xrtValueIterEnd(&it);}
    xrtValueRelease(keys);
}
static bool Gateway_ValidKey(const char* text)
{
    if(!text || strlen(text)>1000)return false;const unsigned char* p=(const unsigned char*)text;
    for(;*p;p++)if(*p<33 || *p>126)return false;return true;
}
static xvalue* Gateway_ReadKeys(bool* invalid)
{
    *invalid=false;if(!G_CredentialPath)return NULL;
    if(!xrtFileExists(G_CredentialPath))return ValueObject();
    size_t size=0;char* bytes=xrtFileReadAllLimit(G_CredentialPath,131072,&size);
    xvalue* keys=bytes && !memchr(bytes,0,size)?xrtJsonParse(xrtStrViewN(bytes,size)):NULL;
    if(bytes)xrtSecureZero(bytes,size);xrtFree(bytes);
    bool valid=keys && xrtValueType(keys)==XVALUE_OBJECT && ValueCount(keys)<=128;
    xvalueiter it={0};xvaluekey key;xvalue* value;
    if(valid && xrtValueIterBegin(keys,&it)){while((value=xrtValueIterNext(&it,&key))){
        char id[65];xstrview text={0};
        if(key.String.Size>=sizeof(id) || memchr(key.String.Data,0,key.String.Size)){valid=false;break;}
        memcpy(id,key.String.Data,key.String.Size);id[key.String.Size]=0;
        if(!XP_Id(id,64) || !xrtValueGetString(value,&text) || text.Size>1000 || memchr(text.Data,0,text.Size) || !Gateway_ValidKey(text.Data)){valid=false;break;}
    }xrtValueIterEnd(&it);}
    if(!valid){Gateway_WipeKeys(keys);*invalid=true;return NULL;}return keys;
}
static void Gateway_KeyEnv(const char* id,char env[100])
{
    snprintf(env,100,"MODEL_GATEWAY_%s_API_KEY",id);char* p;
    for(p=env;*p;p++){if(*p>='a'&&*p<='z')*p-='a'-'A';if(*p=='-'||*p=='.')*p='_';}
}
static bool Gateway_Key(const GatewayChannel* channel,char out[1001])
{
    memset(out,0,1001);if(!strcmp(channel->auth,"none"))return true;
    char env[100];Gateway_KeyEnv(channel->id,env);const char* key=getenv(env);
    if(key && *key){if(!Gateway_ValidKey(key))return false;strcpy(out,key);return true;}
    bool invalid=false;xvalue* keys=Gateway_ReadKeys(&invalid);key=XP_Text(keys,channel->id,1000);
    bool valid=!invalid && key && *key;if(valid)strcpy(out,key);Gateway_WipeKeys(keys);return valid;
}
static int Gateway_SaveKey(const xvalue* body)
{
    const char* names[]={"channel_id","api_key","replace_invalid"};
    const char* id=XP_Text(body,"channel_id",64),*key=XP_Text(body,"api_key",1000);bool replace=false;
    if(!XP_Fields(body,names,3) || !XP_Id(id,64) || !Gateway_ValidKey(key) ||
        (ValueHas(body,"replace_invalid") && !xrtValueGetBool(ValueGet(body,"replace_invalid"),&replace)))return 400;
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT 1 FROM channel WHERE id=?");XP_Bind(s,1,id);bool exists=s && sqlite3_step(s)==SQLITE_ROW;sqlite3_finalize(s);if(!exists)return 404;
    bool invalid=false;xvalue* keys=Gateway_ReadKeys(&invalid);
    if(invalid && !replace)return 409;if(!keys)keys=ValueObject();
    const char* old=XP_Text(keys,id,1000);if(old)xrtSecureZero((void*)old,strlen(old));
    bool ok=ValueSetText(keys,id,key);size_t size=0;char* bytes=ok?xrtJsonStringify(keys,false,&size):NULL;
    ok=bytes && size<=131072 && xrtFileWriteAtomic(G_CredentialPath,(xbytesview){(cbytes)bytes,size});
#if !defined(_WIN32) && !defined(_WIN64)
    if(ok)ok=xrtPathSetMode(G_CredentialPath,false,0600);
#endif
    if(bytes)xrtSecureZero(bytes,size);xrtFree(bytes);Gateway_WipeKeys(keys);return ok?0:503;
}
