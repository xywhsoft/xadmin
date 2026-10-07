/* Thin HTTP boundary: services own credentials, persistence and authentication.
 * Response shape is code/msg/data; errors use their real HTTP status. */
static bool XA_SessionStillValid(xvalue* session,bool recent);
static void XA_LoginAPI(XAdminRequest* req,xvalue* body)
{
    const char* identifier=XA_Text(body,"identifier",254);const char* password=XA_Text(body,"password",128);
    XAIdentifier parsed;XAAccount account;XATokenSet tokens={0};char upgrade[257]={0};
    if(!identifier||!password||!XA_IdentifierParse(identifier,strlen(identifier),G_Identity.country,&parsed)){
        XA_Reply(req,400,"identifier or password is invalid",NULL,NULL);return;}
    if(Guard_Check(G_GuardMember,req->remote)){XA_Reply(req,429,"too many login attempts",NULL,NULL);return;}
    bool found=XA_AccountByIdentifier(&parsed,&account);
    bool verified=found&&account.status==1&&XA_VerifyPassword(&account,password);
    /* Apply equivalent KDF work to missing/disabled identifiers to avoid a
     * fast account-existence oracle. Input parsing and rate limits precede it. */
    if(!found||account.status!=1){xrtMutexUnlock(G_RequestLock);XA_PasswordHash(password,upgrade);xrtMutexLock(G_RequestLock);xrtSecureZero(upgrade,sizeof(upgrade));}
    if(!verified){Guard_Failed(G_GuardMember,req->remote);XA_Reply(req,401,"identifier or password is incorrect",NULL,NULL);return;}
    if(strncmp(account.password,"pbkdf2-sha256$",14)){
        xrtMutexUnlock(G_RequestLock);bool ok=XA_PasswordHash(password,upgrade);xrtMutexLock(G_RequestLock);
        if(!ok||!XA_PasswordCurrent(&account)||!XA_SetPassword(account.id,upgrade)){xrtSecureZero(upgrade,sizeof(upgrade));XA_Reply(req,500,"password upgrade unavailable",NULL,NULL);return;}
        xrtSecureZero(upgrade,sizeof(upgrade));
    }
    if(!XA_SessionIssue(req,account.id,&tokens)){int status=tokens.error_status==429?429:500;XA_TokensUnit(&tokens);XA_Reply(req,status,status==429?"too many MFA attempts":"session unavailable",NULL,NULL);return;}
    xvalue* data=XA_TokenData(&tokens);char* headers=XA_TokenHeaders(req,&tokens);
    if(data&&headers){if(!tokens.mfa_challenge[0]){ValueSetText(data,"username",account.username);ValueSetText(data,"nickname",account.nickname);Member_SetBalance(data,account.id,account.balance);}
        Guard_Reset(G_GuardMember,req->remote);XA_Reply(req,200,"signed in",data,headers);}
    else {XA_SessionRevoke(tokens.sid);XA_Reply(req,500,"session unavailable",NULL,NULL);}
    xrtFree(headers);xrtValueRelease(data);XA_TokensUnit(&tokens);xrtSecureZero(&account,sizeof(account));
}
static void XA_RegisterAPI(XAdminRequest* req,xvalue* body)
{
    const char* username=XA_Text(body,"username",64);const char* password=XA_Text(body,"password",128);
    const char* nickname=XA_Text(body,"nickname",64);char key[65],record[257]={0};sqlite3_stmt* s;
    if(!G_Identity.registration){XA_Reply(req,403,"registration is disabled",NULL,NULL);return;}
    if(!username||!XA_AccountKey(username,strlen(username),key)||!XA_PasswordPolicy(password)||
       (ValueHas(body,"nickname")&&!nickname)||ValueHas(body,"phone")||ValueHas(body,"email")){
        XA_Reply(req,400,"invalid registration fields; contacts require verification",NULL,NULL);return;}
    int interval=atoi(Option_GetGlobalText("registerIntervalSecond","60"));
    if(interval<0)interval=60;
    if(interval>0&&Register_WaitSeconds(req->remote,interval)>0){XA_Reply(req,429,"registration is too frequent",NULL,NULL);return;}
    s=XA_SQL("SELECT id FROM member WHERE lower(username)=?");XA_BindText(s,1,key);
    int rc=s?sqlite3_step(s):SQLITE_ERROR;sqlite3_finalize(s);
    if(rc==SQLITE_ROW){XA_Reply(req,409,"account name is reserved",NULL,NULL);return;}
    if(rc!=SQLITE_DONE){XA_Reply(req,500,"registration unavailable",NULL,NULL);return;}
    xrtMutexUnlock(G_RequestLock);bool hashed=XA_PasswordHash(password,record);xrtMutexLock(G_RequestLock);
    if(!hashed){XA_Reply(req,500,"password service unavailable",NULL,NULL);return;}
    s=XA_SQL("INSERT INTO member(username,salt,pwd,groupId,authLevel,balance,nickname,email,phone,avatar,status,createTime,updateTime,isDelete)VALUES(?,'',?,1,0,0,?,'','','',1,?,?,0)");
    XA_BindText(s,1,username);XA_BindText(s,2,record);XA_BindText(s,3,nickname?nickname:username);
    if(s){sqlite3_bind_int64(s,4,XAdmin_UnixNowUs());sqlite3_bind_int64(s,5,XAdmin_UnixNowUs());}
    bool ok=XA_Done(s,true);xrtSecureZero(record,sizeof(record));
    if(!ok){int code=sqlite3_extended_errcode(G_DB);int status=(code==SQLITE_CONSTRAINT_UNIQUE||code==SQLITE_CONSTRAINT_PRIMARYKEY)?409:500;XA_Reply(req,status,status==409?"account name is reserved":"registration unavailable",NULL,NULL);return;}
    int64 id=sqlite3_last_insert_rowid(G_DB);Register_Note(req->remote);
    xvalue* data=ValueObject();ValueSetInt(data,"id",id);XA_Reply(req,201,"registered",data,NULL);xrtValueRelease(data);
}
static void XA_ProfileAPI(XAdminRequest* req,xvalue* session,xvalue* body)
{
    int64 owner=ValueInt(session,"id");sqlite3_stmt* s;
    if(xsReqMethodID(req)==XHTTP_METHOD_GET){
        s=XA_SQL("SELECT id,username,nickname,email,phone,avatar,phone_verified_at,email_verified_at,groupId,authLevel,balance,createTime,EXISTS(SELECT 1 FROM member_security_question WHERE member_id=member.id) FROM member WHERE id=? AND isDelete=0 AND status=1");
        if(s)sqlite3_bind_int64(s,1,owner);
        xvalue* data=NULL;
        if(s&&sqlite3_step(s)==SQLITE_ROW){data=ValueObject();ValueSetInt(data,"id",sqlite3_column_int64(s,0));
            const char* fields[]={"username","nickname","email","phone","avatar"};int i;
            for(i=0;i<5;i++)ValueSetText(data,fields[i],(const char*)sqlite3_column_text(s,i+1));
            ValueSetBool(data,"phone_verified",sqlite3_column_int64(s,6)>0);ValueSetBool(data,"email_verified",sqlite3_column_int64(s,7)>0);
            ValueSetInt(data,"phone_verified_at",sqlite3_column_int64(s,6));ValueSetInt(data,"email_verified_at",sqlite3_column_int64(s,7));
            ValueSetBool(data,"security_questions_configured",sqlite3_column_int(s,12)>0);
            ValueSetInt(data,"groupId",sqlite3_column_int64(s,8));ValueSetInt(data,"authLevel",sqlite3_column_int64(s,9));
            Member_SetBalance(data,owner,sqlite3_column_int64(s,10));ValueSetInt(data,"createTime",sqlite3_column_int64(s,11));}
        sqlite3_finalize(s);XA_Reply(req,data?200:500,data?"success":"profile unavailable",data,NULL);xrtValueRelease(data);return;
    }
    /* A strict whitelist also rejects aliases for verification/privilege fields. */
    xvalueiter it={0};xvaluekey key;xvalue* value;bool valid=true;
    if(xrtValueIterBegin(body,&it)){
        while((value=xrtValueIterNext(&it,&key)))if(key.Type!=XVALUE_KEY_STRING||
            (!xrtStrEqual(key.String,XRT_STR_LITERAL("nickname"))&&!xrtStrEqual(key.String,XRT_STR_LITERAL("avatar"))))valid=false;
        xrtValueIterEnd(&it);}
    const char* nickname=XA_Text(body,"nickname",64);const char* avatar=XA_Text(body,"avatar",512);
    if(!valid||(ValueHas(body,"nickname")&&!nickname)||(ValueHas(body,"avatar")&&!avatar)){
        XA_Reply(req,400,"only nickname and avatar can be edited; use contact verification",NULL,NULL);return;}
    s=XA_SQL("UPDATE member SET nickname=COALESCE(?,nickname),avatar=COALESCE(?,avatar),updateTime=? WHERE id=? AND status=1 AND isDelete=0");
    XA_BindText(s,1,nickname);XA_BindText(s,2,avatar);if(s){sqlite3_bind_int64(s,3,XAdmin_UnixNowUs());sqlite3_bind_int64(s,4,owner);}
    bool ok=XA_Done(s,true);XA_Reply(req,ok?200:500,ok?"updated":"profile unavailable",NULL,NULL);
}
static void XA_PasswordAPI(XAdminRequest* req,xvalue* session,xvalue* body,bool reauth)
{
    XAAccount account;char record[257]={0};const char* old=XA_Text(body,reauth?"password":"oldPassword",128);
    const char* password=reauth?NULL:XA_Text(body,"newPassword",128);
    if(!old||(!reauth&&!XA_PasswordPolicy(password))){XA_Reply(req,400,"invalid password fields",NULL,NULL);return;}
    if(Guard_Check(G_GuardMember,req->remote)){XA_Reply(req,429,"too many verification attempts",NULL,NULL);return;}
    if(!XA_AccountByID(ValueInt(session,"id"),&account)||!XA_VerifyPassword(&account,old)){
        Guard_Failed(G_GuardMember,req->remote);XA_Reply(req,401,"password is incorrect",NULL,NULL);return;}
    if(!XA_MFARecent("member",session)){
        const char* code=XA_Text(body,"code",40);int status=code?XA_MFAVerify("member",account.id,code,req->remote):403;
        if(status!=200){XA_Reply(req,status,"confirm your MFA again",NULL,NULL);return;}
        sqlite3_stmt* proof=XA_SQL("UPDATE member_session SET mfa_verified_at=? WHERE sid=? AND revoked_at=0");
        if(proof)sqlite3_bind_int64(proof,1,XA_Now());XA_BindText(proof,2,ValueText(session,"sid"));
        if(!XA_Done(proof,true)){XA_Reply(req,500,"identity service unavailable",NULL,NULL);return;}
        ValueSetInt(session,"mfa_verified_at",XA_Now());
    }
    if(!reauth){xrtMutexUnlock(G_RequestLock);bool ok=XA_PasswordHash(password,record);xrtMutexLock(G_RequestLock);
        if(!ok||!XA_PasswordCurrent(&account)){xrtSecureZero(record,sizeof(record));XA_Reply(req,409,"account changed; retry",NULL,NULL);return;}}
    if(!XA_SessionStillValid(session,false)){xrtSecureZero(record,sizeof(record));XA_Reply(req,401,"session revoked",NULL,NULL);return;}
    if(!XA_Begin()){xrtSecureZero(record,sizeof(record));XA_Reply(req,500,"identity service unavailable",NULL,NULL);return;}
    bool ok=true;const char* sid=ValueText(session,"sid");sqlite3_stmt* s;
    if(!reauth)ok=XA_SetPassword(account.id,record)&&XA_SessionRevokeAccount(account.id,sid);
    if(ok){s=XA_SQL("UPDATE member_session SET reauth_until=? WHERE sid=? AND revoked_at=0 AND expires_at>?");
        if(s){sqlite3_bind_int64(s,1,XA_Now()+300);sqlite3_bind_int64(s,3,XA_Now());}XA_BindText(s,2,sid);ok=XA_Done(s,true);}
    ok=XA_End(ok);xrtSecureZero(record,sizeof(record));xrtSecureZero(&account,sizeof(account));
    XA_Reply(req,ok?200:500,ok?(reauth?"identity confirmed":"password updated"):"identity service unavailable",NULL,NULL);
}
static void XA_SessionsAPI(XAdminRequest* req,xvalue* session,xvalue* body)
{
    const char* current=ValueText(session,"sid");int64 owner=ValueInt(session,"id");
    if(xsReqMethodID(req)==XHTTP_METHOD_GET){
        sqlite3_stmt* s=XA_SQL("SELECT sid,created_at,last_used,expires_at,ip,user_agent FROM member_session WHERE member_id=? AND revoked_at=0 AND expires_at>? ORDER BY created_at DESC,rowid DESC LIMIT 5");
        if(s){sqlite3_bind_int64(s,1,owner);sqlite3_bind_int64(s,2,XA_Now());}xvalue* data=ValueArray();int rc=SQLITE_ERROR;
        if(s)while((rc=sqlite3_step(s))==SQLITE_ROW){xvalue* row=ValueObject();const char* sid=(const char*)sqlite3_column_text(s,0);
            ValueSetText(row,"id",sid);ValueSetBool(row,"current",sid&&!strcmp(sid,current));ValueSetInt(row,"created_at",sqlite3_column_int64(s,1));
            ValueSetInt(row,"last_used",sqlite3_column_int64(s,2));ValueSetInt(row,"expires_at",sqlite3_column_int64(s,3));
            ValueSetText(row,"ip",(const char*)sqlite3_column_text(s,4));ValueSetText(row,"user_agent",(const char*)sqlite3_column_text(s,5));ValueArrayOwn(data,row);}
        sqlite3_finalize(s);XA_Reply(req,rc==SQLITE_DONE?200:500,rc==SQLITE_DONE?"success":"sessions unavailable",rc==SQLITE_DONE?data:NULL,NULL);xrtValueRelease(data);return;
    }
    const char* sid=XA_Text(body,"session_id",64);bool others=ValueBool(body,"others");
    if(!others&&!XA_IsHex(sid,64)){XA_Reply(req,400,"session_id or others is required",NULL,NULL);return;}
    bool ok;
    if(others)ok=XA_SessionRevokeAccount(owner,current);
    else {sqlite3_stmt* s=XA_SQL("UPDATE member_session SET revoked_at=? WHERE sid=? AND member_id=? AND revoked_at=0");
        if(s){sqlite3_bind_int64(s,1,XA_Now());sqlite3_bind_int64(s,3,owner);}XA_BindText(s,2,sid);ok=XA_Done(s,true);}
    XA_Reply(req,ok?200:404,ok?"sessions revoked":"session not found",NULL,NULL);
}
/* Recheck after unlocked KDF work: another device may have revoked the session. */
static bool XA_SessionStillValid(xvalue* session,bool recent)
{
    xvalue* current=XA_SessionRead(ValueText(session,"sid"),NULL);
    bool ok=current&&ValueInt(current,"id")==ValueInt(session,"id")&&(!recent||XA_Recent(current));
    xrtValueRelease(current);return ok;
}
static void XA_CredentialsAPI(XAdminRequest* req,xvalue* session,xvalue* body)
{
    const char* username=XA_Text(body,"username",64);const char* password=XA_Text(body,"password",128);
    char key[65],record[257]={0};XAAccount account;
    if(!username||!XA_AccountKey(username,strlen(username),key)||!XA_PasswordPolicy(password)){
        XA_Reply(req,400,"a valid account name and 8-128 byte password are required",NULL,NULL);return;}
    if(!XA_Recent(session)){XA_Reply(req,403,"confirm your identity again",NULL,NULL);return;}
    if(!XA_AccountByID(ValueInt(session,"id"),&account)){XA_Reply(req,401,"unauthorized",NULL,NULL);return;}
    xrtMutexUnlock(G_RequestLock);bool ok=XA_PasswordHash(password,record);xrtMutexLock(G_RequestLock);
    if(!ok||!XA_PasswordCurrent(&account)||!XA_SessionStillValid(session,true)){
        xrtSecureZero(record,sizeof(record));XA_Reply(req,409,"account or session changed; retry",NULL,NULL);return;}
    if(!XA_Begin()){xrtSecureZero(record,sizeof(record));XA_Reply(req,500,"identity service unavailable",NULL,NULL);return;}
    sqlite3_stmt* s=XA_SQL("UPDATE member SET username=?,salt='',pwd=?,updateTime=? WHERE id=? AND status=1 AND isDelete=0");
    XA_BindText(s,1,username);XA_BindText(s,2,record);
    if(s){sqlite3_bind_int64(s,3,XAdmin_UnixNowUs());sqlite3_bind_int64(s,4,account.id);}
    ok=XA_Done(s,true);int error=sqlite3_extended_errcode(G_DB);
    if(ok)ok=XA_SecurityClose(account.id,"credentials_changed",NULL,NULL)&&XA_SessionRevokeAccount(account.id,ValueText(session,"sid"));
    ok=XA_End(ok);xrtSecureZero(record,sizeof(record));xrtSecureZero(&account,sizeof(account));
    int status=ok?200:error==SQLITE_CONSTRAINT_UNIQUE?409:500;
    XA_Reply(req,status,ok?"account credentials updated":status==409?"account name is reserved":"identity service unavailable",NULL,NULL);
}
static void XA_CurrentSessionAPI(XAdminRequest* req,xvalue* session)
{
    if(!XA_OriginAllowed(req)){XA_Reply(req,403,"origin is not allowed",NULL,NULL);return;}
    char csrf[65]={0},hash[65];char* headers=NULL;
    bool cookie=!strcmp(ValueText(session,"source"),"cookie");
    if(cookie){
        if(!XA_SameOrigin(req)){XA_Reply(req,403,"cookie sessions require the site origin",NULL,NULL);return;}
        int n=XA_Cookie(req,"MCSRF",csrf,sizeof(csrf));
        bool valid=n==64&&XA_IsHex(csrf,64)&&XA_Hash(csrf,hash)&&xrtConstTimeEqual(hash,ValueText(session,"csrf_hash"),64);
        if(!valid){
            sqlite3_stmt* s=NULL;bool ok=XA_Random(csrf)&&XA_Hash(csrf,hash);
            if(ok){s=XA_SQL("UPDATE member_session SET csrf_hash=? WHERE sid=? AND revoked_at=0 AND expires_at>?");
                XA_BindText(s,1,hash);XA_BindText(s,2,ValueText(session,"sid"));if(s)sqlite3_bind_int64(s,3,XA_Now());ok=XA_Done(s,true);}
            if(!ok){XA_Reply(req,500,"session unavailable",NULL,NULL);return;}
        }
        /* Keep the browser cookie aligned with its sliding server session,
         * without rotating a valid CSRF token shared by other tabs. */
        headers=XA_CookieHeader(req,ValueText(session,"msid"),false,csrf);
        if(!headers){XA_Reply(req,500,"session unavailable",NULL,NULL);return;}
    }
    char* access=XA_AccessToken(ValueText(session,"sid"),ValueInt(session,"id"));
    xvalue* data=ValueObject();
    ValueSetInt(data,"id",ValueInt(session,"id"));ValueSetText(data,"session_id",ValueText(session,"sid"));
    ValueSetInt(data,"reauth_until",ValueInt(session,"reauth_until"));
    ValueSetInt(data,"mfa_verified_at",ValueInt(session,"mfa_verified_at"));
    ValueSetText(data,"access_token",access);ValueSetText(data,"token_type","Bearer");ValueSetInt(data,"expires_in",900);
    if(cookie)ValueSetText(data,"csrf_token",csrf);
    XA_Reply(req,access?200:500,access?"success":"session unavailable",access?data:NULL,headers);
    if(access)xrtSecureZero(access,strlen(access));xrtFree(access);xrtFree(headers);xrtValueRelease(data);
}
static void XA_IdentityHandler(XS_ServerObject server,XS_HostObject host,XAdminRequest* req,XAdminRequest* resp,xvalue* session)
{
    (void)server;(void)host;(void)resp;
    const char* path=req->path;int method=xsReqMethodID(req);bool public_route=!strcmp(path,"/api/v1/login")||!strcmp(path,"/api/v1/register")||!strcmp(path,"/api/v1/token/refresh");
    bool get_allowed=!strcmp(path,"/api/v1/profile")||!strcmp(path,"/api/v1/sessions")||!strcmp(path,"/api/v1/session");
    bool method_ok=method==XHTTP_METHOD_POST||((method==XHTTP_METHOD_GET)&&get_allowed)||
        (method==XHTTP_METHOD_PUT&&!strcmp(path,"/api/v1/profile"))||
        (method==XHTTP_METHOD_DELETE&&!strcmp(path,"/api/v1/sessions"));
    if(!method_ok||(method==XHTTP_METHOD_POST&&get_allowed)) {XA_Reply(req,405,"method not allowed",NULL,NULL);return;}
    if(!public_route&&xrtValueType(session)!=XVALUE_OBJECT){XA_Reply(req,401,"unauthorized",NULL,NULL);return;}
    if(method!=XHTTP_METHOD_GET&&(!XA_OriginAllowed(req)||(!public_route&&!XA_RequestCSRF(req,session)))){
        XA_Reply(req,403,"CSRF verification failed",NULL,NULL);return;}
    xvalue* body=method==XHTTP_METHOD_GET?NULL:XA_Body(req);
    /* Logout needs no request body, but still requires X-CSRF-Token for cookies. */
    if(method!=XHTTP_METHOD_GET&&!body&&strcmp(path,"/api/v1/logout")) {XA_Reply(req,400,"a bounded JSON object is required",NULL,NULL);return;}
    if(!strcmp(path,"/api/v1/login"))XA_LoginAPI(req,body);
    else if(!strcmp(path,"/api/v1/register"))XA_RegisterAPI(req,body);
    else if(!strcmp(path,"/api/v1/profile"))XA_ProfileAPI(req,session,body);
    else if(!strcmp(path,"/api/v1/profile/password"))XA_PasswordAPI(req,session,body,false);
    else if(!strcmp(path,"/api/v1/profile/reauth"))XA_PasswordAPI(req,session,body,true);
    else if(!strcmp(path,"/api/v1/profile/credentials"))XA_CredentialsAPI(req,session,body);
    else if(!strcmp(path,"/api/v1/session"))XA_CurrentSessionAPI(req,session);
    else if(!strcmp(path,"/api/v1/sessions"))XA_SessionsAPI(req,session,body);
    else if(!strcmp(path,"/api/v1/logout")){
        bool ok=XA_SessionRevoke(ValueText(session,"sid"));char* header=XA_CookieHeader(req,"",true,"");
        XA_Reply(req,ok?200:500,ok?"signed out":"logout unavailable",NULL,ok?header:NULL);xrtFree(header);
    }else if(!strcmp(path,"/api/v1/token/refresh")){
        const char* refresh=XA_Text(body,"refresh_token",64);XATokenSet tokens={0};int status=401;
        bool ok=XA_SessionRefresh(refresh,&tokens,&status);xvalue* data=ok?XA_TokenData(&tokens):NULL;
        XA_Reply(req,ok&&data?200:(ok?500:status),ok?"refreshed":"refresh token invalid",data,NULL);xrtValueRelease(data);XA_TokensUnit(&tokens);
    }else XA_Reply(req,404,"not found",NULL,NULL);
    xrtValueRelease(body);
}
static void XA_IdentityRegisterRoutes(void)
{
    const char* paths[]={"/api/v1/token/refresh","/api/v1/sessions","/api/v1/profile/reauth","/api/v1/session","/api/v1/profile/credentials"};size_t i;
    for(i=0;i<sizeof(paths)/sizeof(paths[0]);i++){
        RouteInfo* route=AddStaticRouteHTTP(paths[i],XHTTP_METHOD_ANY,XA_IdentityHandler,true);
        if(route){route->bAdmin=false;route->bAuth=false;}
    }
}
