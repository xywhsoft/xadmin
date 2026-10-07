static void XA_ChallengeHandler(XS_ServerObject server,XS_HostObject host,XAdminRequest* req,XAdminRequest* resp,xvalue* session)
{
    (void)server;(void)host;(void)resp;
    bool binding=!strncmp(req->path,"/api/v1/profile/contacts",24);int method=xsReqMethodID(req);
    if(method!=XHTTP_METHOD_POST&&(method!=XHTTP_METHOD_DELETE||strcmp(req->path,"/api/v1/profile/contacts"))){XA_Reply(req,405,"method not allowed",NULL,NULL);return;}
    if(binding&&xrtValueType(session)!=XVALUE_OBJECT){XA_Reply(req,401,"unauthorized",NULL,NULL);return;}
    if(!XA_OriginAllowed(req)||(binding&&!XA_RequestCSRF(req,session))){XA_Reply(req,403,"CSRF verification failed",NULL,NULL);return;}
    xvalue* body=XA_Body(req);if(!body){XA_Reply(req,400,"a bounded JSON object is required",NULL,NULL);return;}
    if(method==XHTTP_METHOD_DELETE){
        const char* channel=XA_Text(body,"channel",8);int64 owner=ValueInt(session,"id");
        if(!channel||(strcmp(channel,"phone")&&strcmp(channel,"email")))XA_Reply(req,400,"channel must be phone or email",NULL,NULL);
        else if(!XA_Recent(session))XA_Reply(req,403,"confirm your identity again",NULL,NULL);
        else if(!XA_HasOtherLogin(owner,channel,0))XA_Reply(req,409,"cannot remove the last login method",NULL,NULL);
        else if(!XA_Begin())XA_Reply(req,500,"contact service unavailable",NULL,NULL);
        else {
            const char* sql=!strcmp(channel,"phone")?"UPDATE member SET phone='',phone_key=NULL,phone_verified_at=0,updateTime=? WHERE id=?":"UPDATE member SET email='',email_key=NULL,email_verified_at=0,updateTime=? WHERE id=?";
            sqlite3_stmt* s=XA_SQL(sql);if(s){sqlite3_bind_int64(s,1,XAdmin_UnixNowUs());sqlite3_bind_int64(s,2,owner);}
            bool ok=XA_Done(s,true)&&XA_SessionRevokeAccount(owner,ValueText(session,"sid"));ok=XA_End(ok);XA_Reply(req,ok?200:500,ok?"contact removed":"contact service unavailable",NULL,NULL);
        }
    } else if(!strcmp(req->path,"/api/v1/auth/challenges")||!strcmp(req->path,"/api/v1/profile/contacts/challenge"))XA_ChallengeStart(req,body,session,binding);
    else XA_ChallengeConfirm(req,body,session,binding);
    xrtValueRelease(body);
}
static void XA_ChallengeRegisterRoutes(void)
{
    const char* paths[]={"/api/v1/auth/challenges","/api/v1/auth/challenges/verify","/api/v1/profile/contacts/challenge","/api/v1/profile/contacts/confirm","/api/v1/profile/contacts"};size_t i;
    for(i=0;i<sizeof(paths)/sizeof(paths[0]);i++){
        RouteInfo* r=AddStaticRouteHTTP(paths[i],XHTTP_METHOD_ANY,XA_ChallengeHandler,true);
        if(r){r->bAdmin=false;r->bAuth=false;}
    }
}
