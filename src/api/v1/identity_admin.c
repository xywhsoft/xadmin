/* Website settings use the existing administrator realm and route permissions.
 * Secrets are write-only to the browser. Saved config becomes live on reload. */
static bool XA_AdminIdentityCSRF(XAdminRequest* req,xvalue* session)
{
    bool bad=false;const xhttpfield* header=XA_Header(req,"X-CSRF-Token",&bad);
    const char* expected=ValueText(session,"_identityCSRF");
    return XA_SameOrigin(req)&&!bad&&header&&header->Value.Size==64&&expected&&strlen(expected)==64&&
        xrtConstTimeEqual(header->Value.Data,expected,64);
}
static bool XA_ConfigRevision(const XAIdentityConfig* config,char revision[65])
{
    xvalue* value=XA_ConfigValue(config,false);size_t n=0;
    char* bytes=value?xrtJsonStringify(value,false,&n):NULL;
    bool ok=bytes&&XA_Hash(bytes,revision);
    if(bytes)xrtSecureZero(bytes,n);xrtFree(bytes);xrtValueRelease(value);return ok;
}
/* A partial update preserves omitted secrets. An explicit empty secret clears
 * it; enabled providers then fail validation rather than silently disabling. */
static bool XA_ConfigMerge(xvalue* current,xvalue* patch)
{
    if(xrtValueType(patch)!=XVALUE_OBJECT)return false;
    xvalueiter it={0};xvaluekey key;xvalue* value;bool ok=true;
    if(xrtValueIterBegin(patch,&it)){
        while((value=xrtValueIterNext(&it,&key))){
            if(key.Type!=XVALUE_KEY_STRING){ok=false;break;}
            bool provider=xrtStrEqual(key.String,XRT_STR_LITERAL("github"))||xrtStrEqual(key.String,XRT_STR_LITERAL("wechat"));
            if(provider){
                xvalue* target=xrtValueObjectGet(current,key.String);xvalueiter nested={0};xvaluekey name;xvalue* field;
                if(xrtValueType(value)!=XVALUE_OBJECT||!target){ok=false;break;}
                if(xrtValueIterBegin(value,&nested)){
                    while((field=xrtValueIterNext(&nested,&name)))
                        if(name.Type!=XVALUE_KEY_STRING||!xrtValueObjectSet(target,name.String,field))ok=false;
                    xrtValueIterEnd(&nested);
                }
            }else if(!xrtValueObjectSet(current,key.String,value))ok=false;
        }
        xrtValueIterEnd(&it);
    }
    return ok;
}
static void XA_AdminIdentity(XS_ServerObject server,XS_HostObject host,XAdminRequest* req,XAdminRequest* resp,xvalue* session)
{
    (void)server;(void)host;(void)resp;
    if(xrtValueType(session)!=XVALUE_OBJECT){XA_Reply(req,401,"administrator login required",NULL,NULL);return;}
    if(!strcmp(req->path,"/admin/view/member/identity")){
        if(xsReqMethodID(req)==XHTTP_METHOD_GET)LoadPage(req,200,HTTP_CT_HTML,"member/identity.html");
        else XA_Reply(req,405,"method not allowed",NULL,NULL);return;
    }
    bool rotation=!strcmp(req->path,"/admin/member/identity/keys/rotate");
    int method=xsReqMethodID(req);
    if(!rotation&&method==XHTTP_METHOD_GET){
        XAIdentityConfig staged;char revision[65],csrf[65];
        if(!XA_ConfigLoad(&staged)||!XA_ConfigRevision(&staged,revision)){
            xrtSecureZero(&staged,sizeof(staged));XA_Reply(req,500,"private configuration is invalid",NULL,NULL);return;}
        if(!XA_IsHex(ValueText(session,"_identityCSRF"),64)){
            if(!XA_Random(csrf)||!ValueSetText(session,"_identityCSRF",csrf)){
                xrtSecureZero(&staged,sizeof(staged));XA_Reply(req,500,"configuration unavailable",NULL,NULL);return;}
        }
        xvalue* data=XA_ConfigValue(&staged,true);char live[65];
        bool ok=data&&ValueSetText(data,"revision",revision)&&ValueSetText(data,"csrf_token",ValueText(session,"_identityCSRF"))&&
            ValueSetBool(data,"reload_required",!XA_ConfigRevision(&G_Identity,live)||strcmp(live,revision)!=0);
        XA_Reply(req,ok?200:500,ok?"success":"configuration unavailable",ok?data:NULL,NULL);
        xrtValueRelease(data);xrtSecureZero(&staged,sizeof(staged));return;
    }
    if(method!=(rotation?XHTTP_METHOD_POST:XHTTP_METHOD_PUT)){XA_Reply(req,405,"method not allowed",NULL,NULL);return;}
    if(!XA_AdminIdentityCSRF(req,session)){XA_Reply(req,403,"CSRF verification failed",NULL,NULL);return;}
    xvalue* body=XA_Body(req);
    if(!body){XA_Reply(req,400,"a bounded JSON object is required",NULL,NULL);return;}
    if(rotation){
        bool invalidate=false;const char* const fields[]={"invalidate_existing"};
        if(!XA_ConfigFields(body,fields,1)||!XA_ConfigBool(body,"invalidate_existing",&invalidate)){
            XA_Reply(req,400,"invalid rotation fields",NULL,NULL);
        }else if(!XA_Begin())XA_Reply(req,500,"key rotation unavailable",NULL,NULL);
        else {
            bool ok=sqlite3_exec(G_DB,"UPDATE identity_key SET active=0",NULL,NULL,NULL)==SQLITE_OK&&XA_KeyInit();
            if(ok&&invalidate){
                sqlite3_stmt* s=XA_SQL("UPDATE member_session SET revoked_at=? WHERE revoked_at=0");
                if(s)sqlite3_bind_int64(s,1,XA_Now());ok=XA_Done(s,false)&&
                    sqlite3_exec(G_DB,"DELETE FROM identity_key WHERE active=0",NULL,NULL,NULL)==SQLITE_OK;
            }
            ok=XA_End(ok);XA_Reply(req,ok?200:500,ok?"key rotated":"key rotation unavailable",NULL,NULL);
        }
    }else {
        XAIdentityConfig staged,next;char revision[65];xvalue* full=NULL;bool loaded=XA_ConfigLoad(&staged);
        const char* expected=XA_Text(body,"revision",64);const char* const fields[]={"revision","config"};
        int status=500;const char* message="configuration unavailable";
        if(!XA_ConfigFields(body,fields,2)||!expected||!XA_IsHex(expected,64)) {status=400;message="revision and config are required";}
        else if(loaded&&XA_ConfigRevision(&staged,revision)){
            if(strcmp(expected,revision)){status=409;message="configuration changed; reload the form";}
            else {
                full=XA_ConfigValue(&staged,false);
                if(!XA_ConfigMerge(full,ValueGet(body,"config"))||!XA_ConfigDecode(full,&next)){
                    status=400;message="invalid identity configuration";
                }else {
                    char* path=xrtPathJoin(DBPath,"identity.json");size_t n=0;char* bytes=xrtJsonStringify(full,true,&n);
                    bool ok=path&&bytes&&n<=16384&&xrtFileWriteAtomic(path,(xbytesview){(cbytes)bytes,n});
                    if(bytes)xrtSecureZero(bytes,n);xrtFree(bytes);xrtFree(path);
                    status=ok?200:500;message=ok?"saved; reload the application to apply":"configuration write failed";
                }
            }
        }
        xvalue* data=status==200?ValueObject():NULL;if(data)ValueSetBool(data,"reload_required",true);
        XA_Reply(req,status,message,data,NULL);xrtValueRelease(data);xrtValueRelease(full);
        xrtSecureZero(&staged,sizeof(staged));xrtSecureZero(&next,sizeof(next));
    }
    xrtValueRelease(body);
}
static void XA_AdminIdentityRegisterRoutes(void)
{
    const char* paths[]={"/admin/view/member/identity","/admin/member/identity/config","/admin/member/identity/keys/rotate"};size_t i;
    for(i=0;i<3;i++){
        RouteInfo* route=AddStaticRouteHTTP(paths[i],XHTTP_METHOD_ANY,XA_AdminIdentity,true);
        if(route)route->bPutLog=true;
    }
}
