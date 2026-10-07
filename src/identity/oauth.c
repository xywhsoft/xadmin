/* Every authorization owns an xoauth2client. PKCE verifier/state are ephemeral
 * generation-local memory; DB stores only hashed state/browser binding and the
 * single-consumption ledger. Reload cancels pending authorizations safely.
 * No provider token or authorization code is stored, logged or placed in URLs. */
static const char XA_OAuthSchema1[]=
    "CREATE TABLE identity_oauth(state_hash TEXT PRIMARY KEY,browser_hash TEXT NOT NULL,provider TEXT NOT NULL,purpose TEXT NOT NULL,member_id INTEGER NOT NULL,session_id TEXT NOT NULL,generation TEXT NOT NULL,created_at INTEGER NOT NULL,expires_at INTEGER NOT NULL,status INTEGER NOT NULL DEFAULT 0);"
    "CREATE INDEX identity_oauth_expiry ON identity_oauth(expires_at);";
#define XA_OAUTH_PENDING_MAX 128
typedef struct XAOAuthAttempt {
    bool occupied;xoauth2client client;
    char provider[8],purpose[8],browser_hash[65],state_hash[65],sid[65];
    int64 member,expires;
} XAOAuthAttempt;
static XAOAuthAttempt G_OAuthAttempts[XA_OAUTH_PENDING_MAX];
static char G_OAuthGeneration[65];
/* Test composition may replace the transport before G_Ready. No public option,
 * query parameter or production endpoint can select a fake provider. */
static xoauth2httpproc G_IdentityOAuthTransport;
static void* G_IdentityOAuthContext;
static void XA_OAuthUnit(void)
{
    size_t i;for(i=0;i<XA_OAUTH_PENDING_MAX;i++)if(G_OAuthAttempts[i].occupied){
        xoauth2ClientUnit(&G_OAuthAttempts[i].client);xrtSecureZero(&G_OAuthAttempts[i],sizeof(G_OAuthAttempts[i]));}
    xrtSecureZero(G_OAuthGeneration,sizeof(G_OAuthGeneration));
}
static XAProviderConfig* XA_Provider(const char* name)
{
    return !strcmp(name,"github")?&G_Identity.github:!strcmp(name,"wechat")?&G_Identity.wechat:NULL;
}
static int XA_Query(XAdminRequest* req,const char* name,char* output,size_t cap)
{
    const char* item=req->query;int result=-1;output[0]=0;
    while(item&&*item){const char* end=strchr(item,'&');if(!end)end=item+strlen(item);
        const char* equal=memchr(item,'=',(size_t)(end-item));
        if(equal&&(size_t)(equal-item)==strlen(name)&&!memcmp(item,name,strlen(name))){
            size_t n=(size_t)(end-equal-1);char* decoded=xrtStrDupN(equal+1,n);if(!decoded)return -2;
            size_t i;for(i=0;i<n;i++)if(decoded[i]=='+')decoded[i]=' ';
            bool ok=result==-1&&xrtPercentDecode(xrtStrViewN(decoded,n),decoded,n,&n)&&n<cap&&!memchr(decoded,0,n);
            if(ok){memcpy(output,decoded,n);output[n]=0;result=(int)n;}xrtFree(decoded);if(!ok)return -2;
        }item=*end?end+1:end;
    }return result;
}
static void XA_OAuthStart(XAdminRequest* req,const char* provider,const char* purpose,xvalue* session)
{
    XAProviderConfig* config=XA_Provider(provider);
    if(!config||!config->enabled){XA_Reply(req,404,"login provider not enabled",NULL,NULL);return;}
    bool login=!strcmp(purpose,"login");
    if(!login&&xrtValueType(session)!=XVALUE_OBJECT){XA_Reply(req,401,"unauthorized",NULL,NULL);return;}
    if(!login&&!strcmp(purpose,"bind")&&!XA_Recent(session)){XA_Reply(req,403,"confirm your identity again",NULL,NULL);return;}
    int rate=XA_Rate("oauth-ip",req->remote,12,60);if(rate){XA_Reply(req,rate,"authorization unavailable or too frequent",NULL,NULL);return;}
    size_t i;XAOAuthAttempt* attempt=NULL;
    for(i=0;i<XA_OAUTH_PENDING_MAX;i++){
        if(G_OAuthAttempts[i].occupied&&G_OAuthAttempts[i].expires<=XA_Now()){
            xoauth2ClientUnit(&G_OAuthAttempts[i].client);xrtSecureZero(&G_OAuthAttempts[i],sizeof(G_OAuthAttempts[i]));}
        if(!G_OAuthAttempts[i].occupied&&!attempt)attempt=&G_OAuthAttempts[i];
    }
    if(!attempt){XA_Reply(req,429,"authorization capacity reached",NULL,NULL);return;}
    char browser[65];int existing=XA_Cookie(req,"MOB",browser,sizeof(browser));
    if(existing!=64||!XA_IsHex(browser,64)){if(!XA_Random(browser)){XA_Reply(req,500,"authorization unavailable",NULL,NULL);return;}}
    memset(attempt,0,sizeof(*attempt));strcpy(attempt->provider,provider);strcpy(attempt->purpose,purpose);
    if(!login){attempt->member=ValueInt(session,"id");snprintf(attempt->sid,sizeof(attempt->sid),"%s",ValueText(session,"sid"));}
    if(!strcmp(provider,"github")){xoauth2UseGithub(&attempt->client,config->id,config->secret,config->callback);attempt->client.Config.Scope="read:user";}
    else {xoauth2UseWechat(&attempt->client,config->id,config->secret,config->callback);
        if(!strcmp(config->mode,"web")){attempt->client.Config.AuthorizeUrl="https://open.weixin.qq.com/connect/oauth2/authorize";attempt->client.Config.Scope="snsapi_userinfo";}}
    char* url=xoauth2BeginLogin(&attempt->client);
    bool ok=url&&XA_Hash(browser,attempt->browser_hash)&&XA_Hash(attempt->client.sState,attempt->state_hash);
    attempt->expires=XA_Now()+600;
    sqlite3_stmt* s=XA_SQL("DELETE FROM identity_oauth WHERE expires_at<?");if(s)sqlite3_bind_int64(s,1,XA_Now()-86400);ok=XA_Done(s,false)&&ok;
    if(ok){s=XA_SQL("INSERT INTO identity_oauth(state_hash,browser_hash,provider,purpose,member_id,session_id,generation,created_at,expires_at)VALUES(?,?,?,?,?,?,?,?,?)");
        XA_BindText(s,1,attempt->state_hash);XA_BindText(s,2,attempt->browser_hash);XA_BindText(s,3,provider);XA_BindText(s,4,purpose);
        if(s)sqlite3_bind_int64(s,5,attempt->member);XA_BindText(s,6,attempt->sid);XA_BindText(s,7,G_OAuthGeneration);
        if(s){sqlite3_bind_int64(s,8,XA_Now());sqlite3_bind_int64(s,9,attempt->expires);}ok=XA_Done(s,true);}
    if(!ok){xrtFree(url);xoauth2ClientUnit(&attempt->client);xrtSecureZero(attempt,sizeof(*attempt));XA_Reply(req,500,"authorization unavailable",NULL,NULL);return;}
    attempt->occupied=true;
    char* header=xrtFormat("Set-Cookie: MOB=%s; Path=/api/v1/; HttpOnly; SameSite=Lax; Max-Age=600%s\r\n",browser,(G_Identity.secure_cookie||req->raw->tls)?"; Secure":"");
    xvalue* data=ValueObject();ValueSetText(data,"authorization_url",url);ValueSetInt(data,"expires_in",600);
    XA_Reply(req,200,"authorization started",data,header);xrtValueRelease(data);xrtFree(header);xrtFree(url);xrtSecureZero(browser,sizeof(browser));
}
static int64 XA_ExternalOwner(const char* provider,const char* app,const char* subject,int* error)
{
    sqlite3_stmt* s=XA_SQL("SELECT member_id FROM member_external_identity WHERE provider=? AND app_namespace=? AND subject=?");
    XA_BindText(s,1,provider);XA_BindText(s,2,app);XA_BindText(s,3,subject);int rc=s?sqlite3_step(s):SQLITE_ERROR;
    int64 member=rc==SQLITE_ROW?sqlite3_column_int64(s,0):0;sqlite3_finalize(s);*error=(rc==SQLITE_ROW||rc==SQLITE_DONE)?0:500;return member;
}
static bool XA_ExternalInsert(int64 member,const char* provider,const char* app,const char* subject,const char* union_id)
{
    sqlite3_stmt* s=XA_SQL("INSERT INTO member_external_identity(member_id,provider,app_namespace,subject,union_id,created_at)VALUES(?,?,?,?,?,?)");
    if(s)sqlite3_bind_int64(s,1,member);XA_BindText(s,2,provider);XA_BindText(s,3,app);XA_BindText(s,4,subject);XA_BindText(s,5,union_id);
    if(s)sqlite3_bind_int64(s,6,XAdmin_UnixNowUs());return XA_Done(s,true);
}
static int64 XA_ExternalCreate(xvalue* info,const char* provider)
{
    const char* nickname=XA_Text(info,!strcmp(provider,"wechat")?"nickname":"name",64);
    if(!nickname||!nickname[0])nickname=!strcmp(provider,"wechat")?"WeChat member":"GitHub member";
    const char* avatar=XA_Text(info,!strcmp(provider,"wechat")?"headimgurl":"avatar_url",512);
    if(!avatar||strncmp(avatar,"https://",8)||strpbrk(avatar,"\r\n"))avatar="";
    sqlite3_stmt* s=XA_SQL("INSERT INTO member(username,salt,pwd,groupId,authLevel,balance,nickname,email,phone,avatar,status,createTime,updateTime,isDelete)VALUES(NULL,NULL,NULL,1,0,0,?,'','',?,1,?,?,0)");
    XA_BindText(s,1,nickname);XA_BindText(s,2,avatar);if(s){sqlite3_bind_int64(s,3,XAdmin_UnixNowUs());sqlite3_bind_int64(s,4,XAdmin_UnixNowUs());}
    return XA_Done(s,true)?sqlite3_last_insert_rowid(G_DB):0;
}
static bool XA_ProviderSubject(const char* provider,const xoauth2token* token,xvalue* info,char out[129])
{
    if(!strcmp(provider,"github")){int64 id=0;
        if(!xrtValueGetInt(ValueGet(info,"id"),&id)||id<=0)return false;snprintf(out,129,"%lld",(long long)id);return true;}
    const char* openid=XA_Text(info,"openid",128);
    if(!openid||!openid[0]||!token->OpenId||strcmp(openid,token->OpenId)||ValueInt(info,"errcode"))return false;
    size_t i;for(i=0;openid[i];i++)if(!XA_AsciiLetter((unsigned char)openid[i])&&!(openid[i]>='0'&&openid[i]<='9')&&openid[i]!='_'&&openid[i]!='-')return false;
    strcpy(out,openid);return true;
}
static void XA_OAuthCallback(XAdminRequest* req,const char* provider)
{
    char state[129],code[2049],browser[65],state_hash[65],browser_hash[65],cancelled[129];XAOAuthAttempt local={0};size_t i;
    if(XA_Query(req,"state",state,sizeof(state))<=0||XA_Cookie(req,"MOB",browser,sizeof(browser))!=64||
        !XA_Hash(state,state_hash)||!XA_Hash(browser,browser_hash)){XA_Reply(req,401,"authorization state invalid",NULL,"Referrer-Policy: no-referrer\r\n");return;}
    XAOAuthAttempt* pending=NULL;
    for(i=0;i<XA_OAUTH_PENDING_MAX;i++)if(G_OAuthAttempts[i].occupied&&!strcmp(G_OAuthAttempts[i].state_hash,state_hash)){pending=&G_OAuthAttempts[i];break;}
    if(!pending||pending->expires<=XA_Now()||strcmp(pending->provider,provider)||!xrtConstTimeEqual(pending->browser_hash,browser_hash,64)){
        XA_Reply(req,401,"authorization expired or belongs to another browser/provider",NULL,"Referrer-Policy: no-referrer\r\n");return;}
    sqlite3_stmt* s=XA_SQL("UPDATE identity_oauth SET status=1 WHERE state_hash=? AND browser_hash=? AND provider=? AND generation=? AND status=0 AND expires_at>?");
    XA_BindText(s,1,state_hash);XA_BindText(s,2,browser_hash);XA_BindText(s,3,provider);XA_BindText(s,4,G_OAuthGeneration);if(s)sqlite3_bind_int64(s,5,XA_Now());
    if(!XA_Done(s,true)){XA_Reply(req,401,"authorization already consumed",NULL,NULL);return;}
    /* Transfer the complete client to this request; a concurrent start/callback
     * cannot overwrite its state/verifier or free its owned preset URLs. */
    local=*pending;memset(pending,0,sizeof(*pending));
    int code_size=XA_Query(req,"code",code,sizeof(code));int cancelled_size=XA_Query(req,"error",cancelled,sizeof(cancelled));
    xoauth2token* token=NULL;xvalue* info=NULL;xoauth2httpxrt* http=NULL;
    if(code_size>0&&cancelled_size==-1){
        xrtMutexUnlock(G_RequestLock);
        if(G_IdentityOAuthTransport){local.client.Config.Http=G_IdentityOAuthTransport;local.client.Config.HttpContext=G_IdentityOAuthContext;}
        else if(req->raw->server->Engine){http=xoauth2HttpXrtCreate(req->raw->server->Engine,NULL,15000000);local.client.Config.Http=http?xoauth2HttpXrt:NULL;local.client.Config.HttpContext=http;}
        token=xoauth2CompleteLogin(&local.client,code,state);
        if(token)info=!strcmp(provider,"wechat")?xoauth2GetWechatUserInfo(&local.client,token):xoauth2GetUserInfo(&local.client,token->AccessToken);
        bool clean=!http||xoauth2HttpXrtCleanup(http);if(http&&clean)xoauth2HttpXrtDestroy(http);
        if(!clean){xrtValueRelease(info);info=NULL;}
        xrtMutexLock(G_RequestLock);
    }
    char subject[129];int status=502;bool identified=token&&info&&XA_ProviderSubject(provider,token,info,subject);
    if(!identified)status=cancelled_size>=0?401:502;
    XAProviderConfig* config=XA_Provider(provider);XATokenSet tokens={0};bool completed=false;int64 member=0;
    if(identified){int error=0;member=XA_ExternalOwner(provider,config->id,subject,&error);status=error?500:200;
        bool login=!strcmp(local.purpose,"login"),reauth=!strcmp(local.purpose,"reauth");
        if(status==200&&!login){xvalue* session=XA_SessionRead(local.sid,NULL);
            bool valid=session&&ValueInt(session,"id")==local.member&&(reauth||XA_Recent(session));xrtValueRelease(session);
            if(!valid)status=403;else if(member&&member!=local.member)status=409;else if(reauth&&member!=local.member)status=403;
            if(status==200&&!member)member=local.member;
        }
        if(status==200&&login&&member){XAAccount account;if(!XA_AccountByID(member,&account)||account.status!=1)status=403;}
        if(status==200&&login&&!member&&!G_Identity.oauth_create)status=403;
        if(status==200&&XA_Begin()){
            if(login&&!member)member=XA_ExternalCreate(info,provider);
            bool ok=member>0;int lookup_error=0;
            int64 existing=ok?XA_ExternalOwner(provider,config->id,subject,&lookup_error):0;
            if(ok&&!existing)ok=XA_ExternalInsert(member,provider,config->id,subject,!strcmp(provider,"wechat")?XA_Text(info,"unionid",128):NULL);
            if(lookup_error)ok=false;
            if(ok&&login)ok=XA_SessionIssue(req,member,&tokens);
            else if(ok&&reauth){s=XA_SQL("UPDATE member_session SET reauth_until=? WHERE sid=? AND member_id=? AND revoked_at=0 AND expires_at>?");
                if(s){sqlite3_bind_int64(s,1,XA_Now()+300);sqlite3_bind_int64(s,3,member);sqlite3_bind_int64(s,4,XA_Now());}XA_BindText(s,2,local.sid);ok=XA_Done(s,true);}
            else if(ok)ok=XA_SessionRevokeAccount(member,local.sid);
            if(ok){s=XA_SQL("UPDATE identity_oauth SET status=2 WHERE state_hash=? AND status=1");XA_BindText(s,1,state_hash);ok=XA_Done(s,true);}
            int write_error=sqlite3_extended_errcode(G_DB);completed=XA_End(ok);if(!completed)status=tokens.error_status==429?429:write_error==SQLITE_CONSTRAINT_UNIQUE?409:500;
        }else if(status==200)status=500;
    }
    if(!completed){s=XA_SQL("UPDATE identity_oauth SET status=3 WHERE state_hash=? AND status=1");XA_BindText(s,1,state_hash);XA_Done(s,false);}
    if(completed){
        char* cookie=(tokens.cookie[0]||tokens.mfa_challenge[0])?XA_TokenHeaders(req,&tokens):xrtStrDup("");
        char* headers=cookie?xrtFormat("Content-Type: text/plain; charset=utf-8\r\nCache-Control: no-store\r\nReferrer-Policy: no-referrer\r\nLocation: /account/index.html\r\n%s",cookie):NULL;
        if(headers)xsHttpReplyAuto(req,303,headers,"",0);else {if(tokens.sid[0])XA_SessionRevoke(tokens.sid);XA_Reply(req,500,"authorization unavailable",NULL,NULL);}
        xrtFree(headers);xrtFree(cookie);
    }else XA_Reply(req,status,status==409?"identity belongs to another account":status==403?"account unavailable or identity confirmation required":"authorization failed; start again",NULL,"Referrer-Policy: no-referrer\r\n");
    XA_TokensUnit(&tokens);xrtValueRelease(info);xoauth2TokenFree(token);xoauth2ClientUnit(&local.client);xrtSecureZero(&local,sizeof(local));
    xrtSecureZero(code,sizeof(code));xrtSecureZero(browser,sizeof(browser));xrtSecureZero(state,sizeof(state));
}
