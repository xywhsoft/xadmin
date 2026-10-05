/* Database is authoritative: revocation and account status are checked on every
 * request, including JWTs. Cookie identifiers and refresh tokens are never
 * persisted in plaintext. Times in these tables are Unix seconds. */
static bool XA_SessionRevoke(const char* sid)
{
    sqlite3_stmt* s=XA_SQL("UPDATE member_session SET revoked_at=? WHERE sid=? AND revoked_at=0");
    if(s)sqlite3_bind_int64(s,1,XA_Now());XA_BindText(s,2,sid);
    bool ok=XA_Done(s,false);if(ok)PluginChannel_Revoke(sid,0,NULL);return ok;
}
static bool XA_SessionRevokeAccount(int64 account,const char* keep_sid)
{
    if(!XA_MFACancel("member",account))return false;
    sqlite3_stmt* s=XA_SQL("UPDATE member_session SET revoked_at=? WHERE member_id=? AND revoked_at=0 AND sid<>?");
    if(s){sqlite3_bind_int64(s,1,XA_Now());sqlite3_bind_int64(s,2,account);}XA_BindText(s,3,keep_sid?keep_sid:"");
    bool ok=XA_Done(s,false);if(ok)PluginChannel_Revoke(NULL,account,keep_sid);return ok;
}
/* Renew only an authenticated, still-live session. An expired or revoked
 * session is never resurrected; the optional absolute cap always wins. */
static int64 XA_SessionDeadline(int64 created, int64 now)
{
    int64 deadline = now + G_Identity.session_idle_days * 86400;
    int64 maximum = created + G_Identity.session_max_days * 86400;
    return G_Identity.session_max_days && maximum < deadline ? maximum : deadline;
}
static xvalue* XA_SessionRead(const char* sid,const char* cookie)
{
    sqlite3_stmt* s;char hash[65];xvalue* session=NULL;XAAccount a;char session_id[65],csrf[65];int64 expiry,reauth,created,mfa_version,mfa_verified;
    if(cookie){if(!XA_IsHex(cookie,64)||!XA_Hash(cookie,hash))return NULL;
        s=XA_SQL("SELECT sid,member_id,csrf_hash,expires_at,reauth_until,created_at,mfa_version,mfa_verified_at FROM member_session WHERE cookie_hash=? AND revoked_at=0 AND expires_at>?");XA_BindText(s,1,hash);}
    else {if(!XA_IsHex(sid,64))return NULL;
        s=XA_SQL("SELECT sid,member_id,csrf_hash,expires_at,reauth_until,created_at,mfa_version,mfa_verified_at FROM member_session WHERE sid=? AND revoked_at=0 AND expires_at>?");XA_BindText(s,1,sid);}
    if(s)sqlite3_bind_int64(s,2,XA_Now());
    if(s&&sqlite3_step(s)==SQLITE_ROW&&XA_CopyColumn(s,0,session_id,sizeof(session_id))&&XA_CopyColumn(s,2,csrf,sizeof(csrf))){
        int64 owner=sqlite3_column_int64(s,1);expiry=sqlite3_column_int64(s,3);reauth=sqlite3_column_int64(s,4);created=sqlite3_column_int64(s,5);mfa_version=sqlite3_column_int64(s,6);mfa_verified=sqlite3_column_int64(s,7);
        sqlite3_finalize(s);s=NULL;
        int64 group_level=-1;
        if(XA_MFAValidSession(false,owner,mfa_version,mfa_verified)&&XA_SessionDeadline(created,XA_Now())>XA_Now()&&XA_AccountByID(owner,&a)&&a.status==1&&MemberAuth_DBGroupGetAccess(a.group,0,&group_level)){
            session=ValueObject();bool ok=session&&ValueSetText(session,"sid",session_id)&&ValueSetInt(session,"id",a.id)&&
                ValueSetInt(session,"groupId",a.group)&&ValueSetInt(session,"authLevel",a.level>group_level?a.level:group_level)&&
                ValueSetInt(session,"balance",a.balance)&&ValueSetText(session,"username",a.username)&&ValueSetText(session,"nickname",a.nickname)&&
                ValueSetText(session,"csrf_hash",csrf)&&ValueSetText(session,"msid",cookie?cookie:"")&&
                ValueSetInt(session,"_expireTime",expiry*1000000)&&ValueSetInt(session,"_createTime",created*1000000)&&
                ValueSetInt(session,"mfa_version",mfa_version)&&ValueSetInt(session,"mfa_verified_at",mfa_verified)&&ValueSetInt(session,"reauth_until",reauth)&&ValueSetText(session,"source",cookie?"cookie":"bearer");
            if(!ok){xrtValueRelease(session);session=NULL;}
        }
    }
    sqlite3_finalize(s);
    if(session){
        int64 now=XA_Now(), deadline=XA_SessionDeadline(created,now);
        s=XA_SQL("UPDATE member_session SET last_used=?,expires_at=? WHERE sid=? AND revoked_at=0 AND expires_at>? AND (last_used<? OR expires_at<? OR expires_at>?)");
        if(s){sqlite3_bind_int64(s,1,now);sqlite3_bind_int64(s,2,deadline);sqlite3_bind_int64(s,4,now);
            sqlite3_bind_int64(s,5,now-60);sqlite3_bind_int64(s,6,deadline-60);sqlite3_bind_int64(s,7,deadline);}
        XA_BindText(s,3,session_id);
        if(!XA_Done(s,false)){xrtValueRelease(session);session=NULL;}
        else if(sqlite3_changes(G_DB)>0)ValueSetInt(session,"_expireTime",deadline*1000000);
    }
    return session;
}
static char* XA_AccessToken(const char* sid,int64 owner)
{
    char kid[65],secret[65],sub[32],jti[65];sqlite3_stmt* s=XA_SQL("SELECT id,secret FROM identity_key WHERE active=1");
    bool ok=s&&sqlite3_step(s)==SQLITE_ROW&&XA_CopyColumn(s,0,kid,sizeof(kid))&&XA_CopyColumn(s,1,secret,sizeof(secret));
    sqlite3_finalize(s);if(!ok||!XA_Random(jti))return NULL;
    snprintf(sub,sizeof(sub),"%lld",(long long)owner);
    xjwtconfig config;xjwtConfigInit(&config);config.Alg=XJWT_ALG_HS256;config.KeyPem=secret;config.KeyId=kid;
    config.Issuer=G_Identity.issuer;config.Audience=G_Identity.audience;config.Subject=sub;config.Jti=jti;config.ExpireSeconds=900;
    xvalue* claims=ValueObject();char* token=NULL;
    if(claims&&ValueSetText(claims,"realm","member")&&ValueSetText(claims,"sid",sid))token=xjwtSign(&config,claims);
    xrtValueRelease(claims);xrtSecureZero(secret,sizeof(secret));return token;
}
static xvalue* XA_AccessVerify(const char* token)
{
    int alg=-1;const char* kid=NULL;char secret[65];xvalue* claims=NULL;sqlite3_stmt* s=NULL;
    if(!token||strlen(token)>4096)return NULL;
    xvalue* header=xjwtDecodeHeader(token,&alg,&kid);
    bool ok=header&&alg==XJWT_ALG_HS256&&XA_IsHex(kid,64)&&!ValueHas(header,"jku")&&!ValueHas(header,"jwk")&&!ValueHas(header,"x5u")&&!ValueHas(header,"crit");
    xrtValueRelease(header);
    if(ok){s=XA_SQL("SELECT secret FROM identity_key WHERE id=?");XA_BindText(s,1,kid);
        ok=s&&sqlite3_step(s)==SQLITE_ROW&&XA_CopyColumn(s,0,secret,sizeof(secret))&&XA_IsHex(secret,64);}
    sqlite3_finalize(s);xrtFree((void*)kid);if(!ok)return NULL;
    xjwtcheck check;xjwtCheckInit(&check);check.Issuer=G_Identity.issuer;check.Audience=G_Identity.audience;
    claims=xjwtVerify(token,secret,&check);xrtSecureZero(secret,sizeof(secret));if(!claims)return NULL;
    const char* sid=XA_Text(claims,"sid",64);const char* sub=XA_Text(claims,"sub",20);const char* realm=XA_Text(claims,"realm",16);const char* jti=XA_Text(claims,"jti",64);
    int64 expiry=0,issued=0;int64 now=XA_Now();int64 owner=sub?Util_ParseI64(sub):0;char canonical[32];
    snprintf(canonical,sizeof(canonical),"%lld",(long long)owner);
    ok=owner>0&&sub&&!strcmp(sub,canonical)&&realm&&!strcmp(realm,"member")&&XA_IsHex(sid,64)&&XA_IsHex(jti,64)&&
       xrtValueGetInt(ValueGet(claims,"exp"),&expiry)&&xrtValueGetInt(ValueGet(claims,"iat"),&issued)&&
       issued<=now&&issued>=0&&expiry>now&&expiry>=issued&&expiry-issued<=900;
    xvalue* session=ok?XA_SessionRead(sid,NULL):NULL;
    if(session&&ValueInt(session,"id")!=owner){xrtValueRelease(session);session=NULL;}
    xrtValueRelease(claims);return session;
}
static xvalue* XA_RequestSession(XAdminRequest* req,bool* invalid)
{
    const xhttpfield* auth=XA_Header(req,"Authorization",invalid);
    if(*invalid)return NULL;
    if(auth){
        if(auth->Value.Size<8||auth->Value.Size>4160||
           !xrtStrCaseEqual(xrtStrViewN(auth->Value.Data,6),XRT_STR_LITERAL("Bearer"))||
           auth->Value.Data[6]!=' '||memchr(auth->Value.Data,0,auth->Value.Size)){*invalid=true;return NULL;}
        size_t prefix=7;while(prefix<auth->Value.Size&&auth->Value.Data[prefix]==' ')prefix++;
        if(prefix==auth->Value.Size||auth->Value.Size-prefix>4096){*invalid=true;return NULL;}
        char* token=xrtStrDupN(auth->Value.Data+prefix,auth->Value.Size-prefix);
        xvalue* s=token?XA_AccessVerify(token):NULL;xrtFree(token);if(!s)*invalid=true;return s;
    }
    char cookie[66]={0};int n=XA_Cookie(req,"MSID",cookie,sizeof(cookie));
    if(n==-1)return NULL;if(n!=64){*invalid=true;return NULL;}
    xvalue* s=XA_SessionRead(NULL,cookie);if(!s)*invalid=true;return s;
}
static bool XA_RequestCSRF(XAdminRequest* req,xvalue* session)
{
    bool bad=false;const char* source=ValueText(session,"source");
    if(source&&!strcmp(source,"bearer"))return true;
    const xhttpfield* f=XA_Header(req,"X-CSRF-Token",&bad);char value[65],hash[65];
    if(bad||!f||f->Value.Size!=64)return false;memcpy(value,f->Value.Data,64);value[64]=0;
    const char* expected=ValueText(session,"csrf_hash");
    return XA_SameOrigin(req)&&XA_IsHex(value,64)&&XA_Hash(value,hash)&&expected&&strlen(expected)==64&&xrtConstTimeEqual(hash,expected,64);
}
typedef struct XATokenSet {
    char sid[65],cookie[65],refresh[65],csrf[65],mfa_challenge[65];char* access;int64 owner;int error_status;
} XATokenSet;
static void XA_TokensUnit(XATokenSet* t) { if(t->access){xrtSecureZero(t->access,strlen(t->access));xrtFree(t->access);}xrtSecureZero(t,sizeof(*t)); }
static bool XA_SessionIssueVerified(XAdminRequest* req,int64 owner,XATokenSet* t,int64 version,int64 verified)
{
    XAAccount a;int64 group_level;char cookie_hash[65],csrf_hash[65],refresh_hash[65];sqlite3_stmt* s;
    memset(t,0,sizeof(*t));t->owner=owner;
    if(!XA_MFAValidSession(false,owner,version,verified))return false;
    if(!XA_AccountByID(owner,&a)||a.status!=1||!MemberAuth_DBGroupGetAccess(a.group,0,&group_level)||
       !XA_Random(t->sid)||!XA_Random(t->cookie)||!XA_Random(t->refresh)||!XA_Random(t->csrf)||
       !XA_Hash(t->cookie,cookie_hash)||!XA_Hash(t->csrf,csrf_hash)||!XA_Hash(t->refresh,refresh_hash))return false;
    t->access=XA_AccessToken(t->sid,owner);if(!t->access||!XA_Begin())return false;
    bool bad=false;const xhttpfield* ua=XA_Header(req,"User-Agent",&bad);char agent[257]={0};
    if(ua&&!bad){size_t n=ua->Value.Size<256?ua->Value.Size:256;memcpy(agent,ua->Value.Data,n);}
    int64 now=XA_Now();
    s=XA_SQL("INSERT INTO member_session(sid,member_id,cookie_hash,csrf_hash,created_at,last_used,expires_at,reauth_until,ip,user_agent,mfa_version,mfa_verified_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?)");
    XA_BindText(s,1,t->sid);if(s)sqlite3_bind_int64(s,2,owner);XA_BindText(s,3,cookie_hash);XA_BindText(s,4,csrf_hash);
    if(s){sqlite3_bind_int64(s,5,now);sqlite3_bind_int64(s,6,now);sqlite3_bind_int64(s,7,XA_SessionDeadline(now,now));sqlite3_bind_int64(s,8,now+300);}
    XA_BindText(s,9,req->remote);XA_BindText(s,10,agent);if(s){sqlite3_bind_int64(s,11,version);sqlite3_bind_int64(s,12,verified);}bool ok=XA_Done(s,true);
    if(ok){s=XA_SQL("INSERT INTO member_refresh(hash,sid)VALUES(?,?)");XA_BindText(s,1,refresh_hash);XA_BindText(s,2,t->sid);ok=XA_Done(s,true);}
    if(ok){ /* Bounded live sessions; expired/revoked rows remain manageable. */
        s=XA_SQL("UPDATE member_session SET revoked_at=? WHERE member_id=? AND revoked_at=0 AND expires_at>? AND sid NOT IN(SELECT sid FROM member_session WHERE member_id=? AND revoked_at=0 AND expires_at>? ORDER BY created_at DESC,rowid DESC LIMIT 5)");
        if(s){sqlite3_bind_int64(s,1,now);sqlite3_bind_int64(s,2,owner);sqlite3_bind_int64(s,3,now);sqlite3_bind_int64(s,4,owner);sqlite3_bind_int64(s,5,now);}ok=XA_Done(s,false);
    }
    return XA_End(ok);
}
static bool XA_SessionIssue(XAdminRequest* req,int64 owner,XATokenSet* t)
{
    memset(t,0,sizeof(*t));t->owner=owner;t->error_status=500;
    XAMFAFactor f;if(!XA_MFAFactorRead("member",owner,&f))return false;
    if(f.enabled&&f.locked_until>XA_Now()){t->error_status=429;return false;}
    if(!f.enabled)return XA_SessionIssueVerified(req,owner,t,f.version,0);
    return XA_MFAChallengeCreate(req,"member",owner,false,t->mfa_challenge);
}
static xvalue* XA_TokenData(const XATokenSet* t)
{
    if(t->mfa_challenge[0]){xvalue* pending=ValueObject();bool ok=pending&&ValueSetBool(pending,"mfa_required",true)&&ValueSetText(pending,"challenge_id",t->mfa_challenge)&&ValueSetInt(pending,"expires_in",300);if(!ok){xrtValueRelease(pending);return NULL;}return pending;}
    xvalue* v=ValueObject();bool ok=v&&ValueSetText(v,"access_token",t->access)&&ValueSetText(v,"refresh_token",t->refresh)&&
        ValueSetText(v,"token_type","Bearer")&&ValueSetInt(v,"expires_in",900)&&ValueSetText(v,"csrf_token",t->csrf)&&ValueSetInt(v,"id",t->owner);
    if(!ok){xrtValueRelease(v);return NULL;}
    if(!t->csrf[0])xrtValueObjectRemove(v,XRT_STR_LITERAL("csrf_token"));
    return v;
}
static char* XA_CookieHeader(XAdminRequest* req,const char* cookie,bool clear,const char* csrf)
{
    const char* secure=(G_Identity.secure_cookie||req->raw->tls)?"; Secure":"";
    return xrtFormat("Set-Cookie: MSID=%s; Path=/; HttpOnly; SameSite=Lax; Max-Age=%d%s\r\nSet-Cookie: MCSRF=%s; Path=/; SameSite=Lax; Max-Age=%d%s\r\n",
        cookie?cookie:"",clear?0:(int)(G_Identity.session_idle_days*86400),secure,
        csrf?csrf:"",clear?0:(int)(G_Identity.session_idle_days*86400),secure);
}
static char* XA_TokenHeaders(XAdminRequest* req,const XATokenSet* t)
{
    if(t->mfa_challenge[0])return XA_MFAPendingHeaders(req,t->mfa_challenge);
    char* cookies=XA_CookieHeader(req,t->cookie,false,t->csrf);
    char* headers=cookies?xrtFormat("%sSet-Cookie: MMFA=; Path=/api/v1/auth/mfa/; HttpOnly; SameSite=Lax; Max-Age=0%s\r\n",cookies,(req->raw->tls||G_Identity.secure_cookie)?"; Secure":""):NULL;
    xrtFree(cookies);return headers;
}
static bool XA_SessionRefresh(const char* refresh,XATokenSet* t,int* status)
{
    char hash[65],sid[65],newhash[65];sqlite3_stmt* s;bool ok=false;int64 used=0;
    memset(t,0,sizeof(*t));*status=401;
    if(!XA_IsHex(refresh,64)||!XA_Hash(refresh,hash))return false;
    s=XA_SQL("SELECT sid,used_at FROM member_refresh WHERE hash=?");XA_BindText(s,1,hash);
    if(s&&sqlite3_step(s)==SQLITE_ROW&&XA_CopyColumn(s,0,sid,sizeof(sid))){used=sqlite3_column_int64(s,1);ok=true;}sqlite3_finalize(s);
    if(!ok)return false;
    if(used){if(!XA_SessionRevoke(sid))*status=500;return false;}
    xvalue* session=XA_SessionRead(sid,NULL);if(!session)return false;
    t->owner=ValueInt(session,"id");strcpy(t->sid,sid);xrtValueRelease(session);
    if(!XA_Random(t->refresh)||!XA_Hash(t->refresh,newhash)||!(t->access=XA_AccessToken(sid,t->owner))||!XA_Begin()){*status=500;return false;}
    s=XA_SQL("UPDATE member_refresh SET used_at=? WHERE hash=? AND used_at=0");
    if(s)sqlite3_bind_int64(s,1,XA_Now());XA_BindText(s,2,hash);ok=XA_Done(s,true);
    if(ok){s=XA_SQL("INSERT INTO member_refresh(hash,sid)VALUES(?,?)");XA_BindText(s,1,newhash);XA_BindText(s,2,sid);ok=XA_Done(s,true);}
    if(!XA_End(ok)){*status=500;return false;}return true;
}

/* Refresh replay evidence is kept for the entire live session, then seven
 * further days. Never prune used refresh tokens from an active family. */
static void XA_SessionMaintenance(void)
{
    if(!G_DB||!XA_Begin())return;
    int64 cutoff=XA_Now()-604800;
    sqlite3_stmt* s=XA_SQL("DELETE FROM member_refresh WHERE sid IN(SELECT sid FROM member_session WHERE expires_at<? OR(revoked_at>0 AND revoked_at<?))");
    if(s){sqlite3_bind_int64(s,1,cutoff);sqlite3_bind_int64(s,2,cutoff);}bool ok=XA_Done(s,false);
    if(ok){s=XA_SQL("DELETE FROM member_session WHERE expires_at<? OR(revoked_at>0 AND revoked_at<?)");
        if(s){sqlite3_bind_int64(s,1,cutoff);sqlite3_bind_int64(s,2,cutoff);}ok=XA_Done(s,false);}
    if(!XA_End(ok))printf("[xadmin][identity] session cleanup deferred\n");
}
