static void XA_ProvidersAPI(XAdminRequest* req)
{
    xvalue* data=ValueObject();xvalue* providers=ValueArray();const char* names[]={"github","wechat"};int i;
    for(i=0;i<2;i++){XAProviderConfig* config=XA_Provider(names[i]);if(config->enabled){xvalue* item=ValueObject();ValueSetText(item,"id",names[i]);ValueSetText(item,"name",i?"微信":"GitHub");ValueArrayOwn(providers,item);}}
    ValueSetOwn(data,"providers",providers);ValueSetBool(data,"registration",G_Identity.registration);
    ValueSetBool(data,"security_questions",G_Identity.security_questions);
    ValueSetBool(data,"phone_verification",XA_DeliveryAvailable("phone"));
    ValueSetBool(data,"email_verification",XA_DeliveryAvailable("email"));
    ValueSetText(data,"default_country_code",G_Identity.country);XA_Reply(req,200,"success",data,NULL);xrtValueRelease(data);
}
static void XA_IdentitiesAPI(XAdminRequest* req,xvalue* session)
{
    sqlite3_stmt* s=XA_SQL("SELECT id,provider,app_namespace,subject,created_at FROM member_external_identity WHERE member_id=? ORDER BY id");
    if(s)sqlite3_bind_int64(s,1,ValueInt(session,"id"));xvalue* data=ValueArray();int rc=SQLITE_ERROR;
    if(s)while((rc=sqlite3_step(s))==SQLITE_ROW){xvalue* row=ValueObject();ValueSetInt(row,"id",sqlite3_column_int64(s,0));
        ValueSetText(row,"provider",(const char*)sqlite3_column_text(s,1));ValueSetText(row,"app_namespace",(const char*)sqlite3_column_text(s,2));
        ValueSetText(row,"subject",(const char*)sqlite3_column_text(s,3));ValueSetInt(row,"created_at",sqlite3_column_int64(s,4));ValueArrayOwn(data,row);}
    sqlite3_finalize(s);XA_Reply(req,rc==SQLITE_DONE?200:500,rc==SQLITE_DONE?"success":"identities unavailable",data,NULL);xrtValueRelease(data);
}
static void XA_IdentityRemove(XAdminRequest* req,xvalue* session,const char* text)
{
    int64 id=Util_ParseI64(text),owner=ValueInt(session,"id");char canonical[32];snprintf(canonical,sizeof(canonical),"%lld",(long long)id);
    if(id<=0||strcmp(canonical,text)){XA_Reply(req,400,"invalid identity id",NULL,NULL);return;}
    if(!XA_Recent(session)){XA_Reply(req,403,"confirm your identity again",NULL,NULL);return;}
    sqlite3_stmt* s=XA_SQL("SELECT id FROM member_external_identity WHERE id=? AND member_id=?");
    if(s){sqlite3_bind_int64(s,1,id);sqlite3_bind_int64(s,2,owner);}bool exists=s&&sqlite3_step(s)==SQLITE_ROW;sqlite3_finalize(s);
    if(!exists){XA_Reply(req,404,"identity not found",NULL,NULL);return;}
    if(!XA_HasOtherLogin(owner,NULL,id)){XA_Reply(req,409,"cannot remove the last login method",NULL,NULL);return;}
    if(!XA_Begin()){XA_Reply(req,500,"identity service unavailable",NULL,NULL);return;}
    s=XA_SQL("DELETE FROM member_external_identity WHERE id=? AND member_id=?");if(s){sqlite3_bind_int64(s,1,id);sqlite3_bind_int64(s,2,owner);}
    bool ok=XA_Done(s,true)&&XA_SessionRevokeAccount(owner,ValueText(session,"sid"));ok=XA_End(ok);XA_Reply(req,ok?200:500,ok?"identity removed":"identity service unavailable",NULL,NULL);
}
static void XA_OAuthHandler(XS_ServerObject server,XS_HostObject host,XAdminRequest* req,XAdminRequest* resp,xvalue* session)
{
    (void)server;(void)host;(void)resp;
    if(!strcmp(req->path,"/api/v1/auth/providers")){if(xsReqMethodID(req)!=XHTTP_METHOD_GET)XA_Reply(req,405,"method not allowed",NULL,NULL);else XA_ProvidersAPI(req);return;}
    if(!strcmp(req->path,"/api/v1/profile/identities")){
        if(xsReqMethodID(req)!=XHTTP_METHOD_GET)XA_Reply(req,405,"method not allowed",NULL,NULL);
        else if(xrtValueType(session)!=XVALUE_OBJECT)XA_Reply(req,401,"unauthorized",NULL,NULL);else XA_IdentitiesAPI(req,session);return;}
    xstrview param={0};char provider[16];
    if(xsReqRouteValue(req,"provider",&param)&&param.Size<sizeof(provider)){memcpy(provider,param.Data,param.Size);provider[param.Size]=0;
        const char* action=strrchr(req->path,'/');action=action?action+1:"";
        if(!strcmp(action,"callback")){if(xsReqMethodID(req)!=XHTTP_METHOD_GET)XA_Reply(req,405,"method not allowed",NULL,NULL);else XA_OAuthCallback(req,provider);return;}
        bool login=!strcmp(action,"start");
        if(xsReqMethodID(req)!=XHTTP_METHOD_POST){XA_Reply(req,405,"method not allowed",NULL,NULL);return;}
        if(!login&&xrtValueType(session)!=XVALUE_OBJECT){XA_Reply(req,401,"unauthorized",NULL,NULL);return;}
        if(!XA_OriginAllowed(req)||(!login&&!XA_RequestCSRF(req,session))){XA_Reply(req,403,"CSRF verification failed",NULL,NULL);return;}
        xvalue* body=XA_Body(req);if(!body){XA_Reply(req,400,"a bounded JSON object is required",NULL,NULL);return;}xrtValueRelease(body);
        XA_OAuthStart(req,provider,login?"login":!strcmp(action,"reauth")?"reauth":"bind",session);return;
    }
    char id[32];if(xsReqRouteValue(req,"identityId",&param)&&param.Size<sizeof(id)){memcpy(id,param.Data,param.Size);id[param.Size]=0;
        if(xsReqMethodID(req)!=XHTTP_METHOD_DELETE)XA_Reply(req,405,"method not allowed",NULL,NULL);
        else if(xrtValueType(session)!=XVALUE_OBJECT)XA_Reply(req,401,"unauthorized",NULL,NULL);
        else if(!XA_RequestCSRF(req,session))XA_Reply(req,403,"CSRF verification failed",NULL,NULL);else XA_IdentityRemove(req,session,id);return;}
    XA_Reply(req,404,"not found",NULL,NULL);
}
static void XA_OAuthRegisterRoutes(void)
{
    const char* statics[]={"/api/v1/auth/providers","/api/v1/profile/identities"};size_t i;
    for(i=0;i<2;i++){RouteInfo* r=AddStaticRouteHTTP(statics[i],XHTTP_METHOD_ANY,XA_OAuthHandler,true);if(r){r->bAdmin=false;r->bAuth=false;}}
    const char* paths[]={"/api/v1/auth/oauth/{provider}/start","/api/v1/auth/oauth/{provider}/callback","/api/v1/profile/identities/{provider}/bind","/api/v1/profile/identities/{provider}/reauth","/api/v1/profile/identities/{identityId}"};
    for(i=0;i<5;i++){RouteInfo* r=AddDynamicRouteHTTP(paths[i],XHTTP_METHOD_ANY,XA_OAuthHandler,true);if(r){r->bAdmin=false;r->bAuth=false;}}
}
