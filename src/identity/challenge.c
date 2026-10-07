static const char XA_ChallengeSchema1[]=
    "CREATE TABLE identity_challenge(id TEXT PRIMARY KEY,purpose TEXT NOT NULL CHECK(purpose IN('bind','login','recover')),channel TEXT NOT NULL CHECK(channel IN('phone','email')),target TEXT NOT NULL,member_id INTEGER NOT NULL,session_id TEXT NOT NULL,key_id TEXT NOT NULL,code_hash TEXT NOT NULL,created_at INTEGER NOT NULL,expires_at INTEGER NOT NULL,attempts INTEGER NOT NULL DEFAULT 0,delivery INTEGER NOT NULL DEFAULT 0,consumed_at INTEGER NOT NULL DEFAULT 0);"
    "CREATE INDEX identity_challenge_owner ON identity_challenge(member_id,session_id,purpose,expires_at);"
    "CREATE TABLE identity_rate(key TEXT PRIMARY KEY,expires_at INTEGER NOT NULL,count INTEGER NOT NULL);";
typedef struct XAChallenge {
    char id[65],purpose[16],channel[8],target[255],sid[65],kid[65],hash[65];
    int64 member,expires;int attempts,delivery;
} XAChallenge;
static int XA_Rate(const char* category,const char* value,int limit,int window)
{
    char key[65];char* input=xrtFormat("%s:%s",category,value);bool hashed=input&&XA_Hash(input,key);xrtFree(input);
    if(!hashed)return 500;sqlite3_stmt* s=XA_SQL("DELETE FROM identity_rate WHERE expires_at<=?");
    if(s)sqlite3_bind_int64(s,1,XA_Now());if(!XA_Done(s,false))return 500;
    s=XA_SQL("SELECT count FROM identity_rate WHERE key=?");XA_BindText(s,1,key);int rc=s?sqlite3_step(s):SQLITE_ERROR;
    int count=rc==SQLITE_ROW?sqlite3_column_int(s,0):0;sqlite3_finalize(s);
    if(rc==SQLITE_ROW){if(count>=limit)return 429;s=XA_SQL("UPDATE identity_rate SET count=count+1 WHERE key=?");XA_BindText(s,1,key);return XA_Done(s,true)?0:500;}
    if(rc!=SQLITE_DONE)return 500;
    s=XA_SQL("SELECT count(*) FROM identity_rate");int rows=s&&sqlite3_step(s)==SQLITE_ROW?sqlite3_column_int(s,0):-1;sqlite3_finalize(s);
    if(rows<0)return 500;if(rows>=10000)return 429;
    s=XA_SQL("INSERT INTO identity_rate VALUES(?,?,1)");XA_BindText(s,1,key);if(s)sqlite3_bind_int64(s,2,XA_Now()+window);return XA_Done(s,true)?0:500;
}
static bool XA_ChallengeMac(const XAChallenge* c,const char* code,char out[65])
{
    char secret[65];unsigned char digest[32];sqlite3_stmt* s=XA_SQL("SELECT secret FROM identity_key WHERE id=?");XA_BindText(s,1,c->kid);
    bool ok=s&&sqlite3_step(s)==SQLITE_ROW&&XA_CopyColumn(s,0,secret,sizeof(secret));sqlite3_finalize(s);
    char* text=ok?xrtFormat("%s\n%s\n%s\n%s\n%lld\n%s\n%s",c->id,c->purpose,c->channel,c->target,(long long)c->member,c->sid,code):NULL;
    ok=text&&xrtHmacSha256(secret,strlen(secret),text,strlen(text),digest);
    if(ok)XA_Hex(digest,32,out);if(text)xrtSecureZero(text,strlen(text));xrtFree(text);xrtSecureZero(secret,sizeof(secret));return ok;
}
static bool XA_ChallengeRead(const char* id,XAChallenge* c)
{
    if(!XA_IsHex(id,64))return false;
    sqlite3_stmt* s=XA_SQL("SELECT id,purpose,channel,target,member_id,session_id,key_id,code_hash,expires_at,attempts,delivery FROM identity_challenge WHERE id=? AND consumed_at=0 AND expires_at>? AND attempts<5 AND delivery IN(1,2)");
    XA_BindText(s,1,id);if(s)sqlite3_bind_int64(s,2,XA_Now());memset(c,0,sizeof(*c));
    bool ok=s&&sqlite3_step(s)==SQLITE_ROW&&XA_CopyColumn(s,0,c->id,sizeof(c->id))&&XA_CopyColumn(s,1,c->purpose,sizeof(c->purpose))&&
        XA_CopyColumn(s,2,c->channel,sizeof(c->channel))&&XA_CopyColumn(s,3,c->target,sizeof(c->target))&&XA_CopyColumn(s,5,c->sid,sizeof(c->sid))&&
        XA_CopyColumn(s,6,c->kid,sizeof(c->kid))&&XA_CopyColumn(s,7,c->hash,sizeof(c->hash));
    if(ok){c->member=sqlite3_column_int64(s,4);c->expires=sqlite3_column_int64(s,8);c->attempts=sqlite3_column_int(s,9);c->delivery=sqlite3_column_int(s,10);}
    sqlite3_finalize(s);return ok;
}
static bool XA_Recent(xvalue* session) { int64 until=ValueInt(session,"reauth_until");return until>XA_Now()&&until<=XA_Now()+300&&XA_MFARecent("member",session); }
static void XA_ChallengeStart(XAdminRequest* req,xvalue* body,xvalue* session,bool binding)
{
    const char* purpose=binding?"bind":XA_Text(body,"purpose",16);const char* channel=XA_Text(body,"channel",8);const char* target=XA_Text(body,"target",254);
    XAIdentifier parsed;XAChallenge c={0};XADeliveryJob job;char code[7],mac[65];sqlite3_stmt* s;
    if(!purpose||(!binding&&strcmp(purpose,"login")&&strcmp(purpose,"recover"))||!channel||!target||
       !XA_IdentifierParse(target,strlen(target),G_Identity.country,&parsed)||
       (strcmp(channel,"phone")&&strcmp(channel,"email"))||
       (parsed.kind!=(strcmp(channel,"phone")?XA_IDENTIFIER_EMAIL:XA_IDENTIFIER_PHONE))){XA_Reply(req,400,"invalid verification purpose/channel/target",NULL,NULL);return;}
    if(binding&&!XA_Recent(session)){XA_Reply(req,403,"confirm your identity again",NULL,NULL);return;}
    int rate=XA_Rate("challenge-ip",req->remote,20,3600);if(!rate)rate=XA_Rate("challenge-target",parsed.key,1,60);
    if(rate){XA_Reply(req,rate,rate==429?"verification is too frequent":"verification unavailable",NULL,NULL);return;}
    strcpy(c.purpose,purpose);strcpy(c.channel,channel);strcpy(c.target,parsed.key);c.member=binding?ValueInt(session,"id"):0;
    if(binding)snprintf(c.sid,sizeof(c.sid),"%s",ValueText(session,"sid"));
    if(!binding){XAAccount account;if(XA_AccountByIdentifier(&parsed,&account)&&account.status==1)c.member=account.id;}
    unsigned int random;int tries=0;
    do {if(!xrtSecureRandom(&random,sizeof(random))||++tries>8){XA_Reply(req,500,"verification unavailable",NULL,NULL);return;}}while(random>=4294000000U);
    snprintf(code,sizeof(code),"%06u",random%1000000U);
    bool ok=XA_Random(c.id);s=XA_SQL("SELECT id FROM identity_key WHERE active=1");
    ok=ok&&s&&sqlite3_step(s)==SQLITE_ROW&&XA_CopyColumn(s,0,c.kid,sizeof(c.kid));sqlite3_finalize(s);
    c.expires=XA_Now()+300;
    XAIdentityMessage message={c.id,channel,c.target,purpose,code,300,req->raw->server->Engine};
    if(!ok||!XA_ChallengeMac(&c,code,mac)){xrtSecureZero(code,sizeof(code));XA_Reply(req,500,"verification unavailable",NULL,NULL);return;}
    if(!XA_DeliveryPrepare(&message,&job)){xrtSecureZero(code,sizeof(code));xrtSecureZero(&job,sizeof(job));XA_Reply(req,503,"verification delivery is not configured",NULL,NULL);return;}
    s=XA_SQL("DELETE FROM identity_challenge WHERE expires_at<?");if(s)sqlite3_bind_int64(s,1,XA_Now()-86400);ok=XA_Done(s,false);
    s=XA_SQL("SELECT count(*) FROM identity_challenge");int count=s&&sqlite3_step(s)==SQLITE_ROW?sqlite3_column_int(s,0):-1;sqlite3_finalize(s);
    if(!ok||count<0||count>=10000){xrtSecureZero(code,sizeof(code));xrtSecureZero(&job,sizeof(job));XA_Reply(req,count>=10000?429:500,"verification capacity unavailable",NULL,NULL);return;}
    s=XA_SQL("INSERT INTO identity_challenge(id,purpose,channel,target,member_id,session_id,key_id,code_hash,created_at,expires_at)VALUES(?,?,?,?,?,?,?,?,?,?)");
    XA_BindText(s,1,c.id);XA_BindText(s,2,purpose);XA_BindText(s,3,channel);XA_BindText(s,4,c.target);
    if(s)sqlite3_bind_int64(s,5,c.member);XA_BindText(s,6,c.sid);XA_BindText(s,7,c.kid);XA_BindText(s,8,mac);
    if(s){sqlite3_bind_int64(s,9,XA_Now());sqlite3_bind_int64(s,10,c.expires);}ok=XA_Done(s,true);
    if(!ok){xrtSecureZero(code,sizeof(code));xrtSecureZero(&job,sizeof(job));XA_Reply(req,500,"verification unavailable",NULL,NULL);return;}
    xrtMutexUnlock(G_RequestLock);XAIdentityDeliveryResult delivered=XA_Deliver(&job);xrtMutexLock(G_RequestLock);
    xrtSecureZero(code,sizeof(code));xrtSecureZero(&job,sizeof(job));
    s=XA_SQL("UPDATE identity_challenge SET delivery=? WHERE id=?");if(s)sqlite3_bind_int(s,1,delivered==XA_DELIVERY_SENT?1:delivered==XA_DELIVERY_UNKNOWN?2:3);XA_BindText(s,2,c.id);ok=XA_Done(s,true);
    xvalue* data=ValueObject();ValueSetText(data,"challenge_id",c.id);ValueSetInt(data,"expires_in",300);
    ValueSetText(data,"delivery",delivered==XA_DELIVERY_SENT?"sent":delivered==XA_DELIVERY_UNKNOWN?"unknown":"failed");
    XA_Reply(req,!ok?500:delivered==XA_DELIVERY_FAILED?502:202,delivered==XA_DELIVERY_UNKNOWN?"delivery uncertain; no automatic resend":"verification requested",data,NULL);xrtValueRelease(data);
}
static bool XA_ChallengeCheck(const XAChallenge* c,const char* code)
{
    size_t i;if(!code||strlen(code)!=6)return false;for(i=0;i<6;i++)if(code[i]<'0'||code[i]>'9')return false;
    sqlite3_stmt* s=XA_SQL("UPDATE identity_challenge SET attempts=attempts+1 WHERE id=? AND consumed_at=0 AND expires_at>? AND attempts<5");
    XA_BindText(s,1,c->id);if(s)sqlite3_bind_int64(s,2,XA_Now());if(!XA_Done(s,true))return false;
    char actual[65];return XA_ChallengeMac(c,code,actual)&&xrtConstTimeEqual(actual,c->hash,64);
}
static bool XA_ChallengeConsume(const XAChallenge* c)
{
    sqlite3_stmt* s=XA_SQL("UPDATE identity_challenge SET consumed_at=? WHERE id=? AND consumed_at=0 AND expires_at>? AND attempts<=5");
    if(s){sqlite3_bind_int64(s,1,XA_Now());sqlite3_bind_int64(s,3,XA_Now());}XA_BindText(s,2,c->id);return XA_Done(s,true);
}
static bool XA_ChallengeTargetCurrent(const XAChallenge* c)
{
    const char* query=!strcmp(c->channel,"phone")?"SELECT id FROM member WHERE id=? AND phone_key=? AND status=1 AND isDelete=0":"SELECT id FROM member WHERE id=? AND email_key=? AND status=1 AND isDelete=0";
    sqlite3_stmt* s=XA_SQL(query);if(s)sqlite3_bind_int64(s,1,c->member);XA_BindText(s,2,c->target);bool ok=s&&sqlite3_step(s)==SQLITE_ROW;sqlite3_finalize(s);return ok;
}
static void XA_ChallengeConfirm(XAdminRequest* req,xvalue* body,xvalue* session,bool binding)
{
    const char* id=XA_Text(body,"challenge_id",64);const char* code=XA_Text(body,"code",6);XAChallenge c;char password[257]={0};
    if(!XA_ChallengeRead(id,&c)||(binding?strcmp(c.purpose,"bind"):!strcmp(c.purpose,"bind"))||
       (binding&&(c.member!=ValueInt(session,"id")||strcmp(c.sid,ValueText(session,"sid"))))||!XA_ChallengeCheck(&c,code)){
        XA_Reply(req,401,"verification invalid or expired",NULL,NULL);return;}
    if(!binding&&!XA_ChallengeTargetCurrent(&c)){XA_Reply(req,401,"verification invalid or expired",NULL,NULL);return;}
    XAAccount account;bool recover=!strcmp(c.purpose,"recover");
    if(recover){const char* raw=XA_Text(body,"newPassword",128);
        if(!XA_PasswordPolicy(raw)||!XA_AccountByID(c.member,&account)){XA_Reply(req,400,"invalid new password",NULL,NULL);return;}
        xrtMutexUnlock(G_RequestLock);bool hashed=XA_PasswordHash(raw,password);xrtMutexLock(G_RequestLock);
        if(!hashed||!XA_PasswordCurrent(&account)||!XA_ChallengeTargetCurrent(&c)){xrtSecureZero(password,sizeof(password));XA_Reply(req,409,"account changed; restart verification",NULL,NULL);return;}}
    char previous[255]={0};
    if(binding){
        sqlite3_stmt* s=XA_SQL(!strcmp(c.channel,"phone")?"SELECT phone_key FROM member WHERE id=?":"SELECT email_key FROM member WHERE id=?");
        if(s)sqlite3_bind_int64(s,1,c.member);
        bool read=s&&sqlite3_step(s)==SQLITE_ROW&&XA_CopyColumn(s,0,previous,sizeof(previous));sqlite3_finalize(s);
        if(!read){XA_Reply(req,500,"verification unavailable",NULL,NULL);return;}
    }
    if(!XA_Begin()){xrtSecureZero(password,sizeof(password));XA_Reply(req,500,"verification unavailable",NULL,NULL);return;}
    bool ok=XA_ChallengeConsume(&c);XATokenSet tokens={0};
    if(ok&&binding){const char* sql=!strcmp(c.channel,"phone")?
        "UPDATE member SET phone=?,phone_key=?,phone_verified_at=?,updateTime=? WHERE id=? AND status=1 AND isDelete=0":
        "UPDATE member SET email=?,email_key=?,email_verified_at=?,updateTime=? WHERE id=? AND status=1 AND isDelete=0";
        sqlite3_stmt* s=XA_SQL(sql);XA_BindText(s,1,c.target);XA_BindText(s,2,c.target);
        if(s){sqlite3_bind_int64(s,3,XAdmin_UnixNowUs());sqlite3_bind_int64(s,4,XAdmin_UnixNowUs());sqlite3_bind_int64(s,5,c.member);}ok=XA_Done(s,true)&&XA_SessionRevokeAccount(c.member,c.sid);
    } else if(ok&&recover)ok=XA_SetPassword(c.member,password)&&XA_SessionRevokeAccount(c.member,NULL);
    else if(ok)ok=XA_SessionIssue(req,c.member,&tokens);
    int error=sqlite3_extended_errcode(G_DB);ok=XA_End(ok);xrtSecureZero(password,sizeof(password));
    xvalue* data=ok&&!binding&&!recover?XA_TokenData(&tokens):NULL;char* headers=data?XA_TokenHeaders(req,&tokens):NULL;
    if(ok&&!binding&&!recover&&(!data||!headers)){XA_SessionRevoke(tokens.sid);ok=false;}
    int status=ok?200:tokens.error_status==429?429:(error==SQLITE_CONSTRAINT_UNIQUE?409:500);
    if(ok&&binding&&previous[0]&&strcmp(previous,c.target)){
        XAIdentityMessage message={c.id,c.channel,previous,"contact_changed","",0,req->raw->server->Engine,true};XADeliveryJob job;
        if(XA_DeliveryPrepare(&message,&job)){
            xrtMutexUnlock(G_RequestLock);XAIdentityDeliveryResult notified=XA_Deliver(&job);xrtMutexLock(G_RequestLock);
            if(notified!=XA_DELIVERY_SENT)printf("[xadmin][identity] previous-contact notification not confirmed (member id=%lld)\n",(long long)c.member);
        }
        xrtSecureZero(&job,sizeof(job));
    }
    XA_Reply(req,status,ok?(binding?"contact verified":recover?"password reset":"signed in"):status==409?"contact already belongs to another account":status==429?"too many MFA attempts":"verification unavailable",data,headers);
    xrtValueRelease(data);xrtFree(headers);XA_TokensUnit(&tokens);
}
static bool XA_HasOtherLogin(int64 member,const char* channel,int64 external_id)
{
    /* Disabled providers and OTP-only contacts without delivery do not count as
     * usable alternatives. Passwords may still authenticate verified contacts. */
    sqlite3_stmt* s=XA_SQL("SELECT (username IS NOT NULL AND username<>'' AND pwd IS NOT NULL AND pwd<>'') OR (phone_verified_at>0 AND ?<>'phone' AND(COALESCE(pwd,'')<>'' OR ?)) OR (email_verified_at>0 AND ?<>'email' AND(COALESCE(pwd,'')<>'' OR ?)) OR EXISTS(SELECT 1 FROM member_external_identity WHERE member_id=member.id AND id<>? AND((provider='github' AND ? AND app_namespace=?) OR(provider='wechat' AND ? AND app_namespace=?))) FROM member WHERE id=? AND isDelete=0 AND status=1");
    XA_BindText(s,1,channel?channel:"");XA_BindText(s,3,channel?channel:"");
    if(s){sqlite3_bind_int(s,2,XA_DeliveryAvailable("phone"));sqlite3_bind_int(s,4,XA_DeliveryAvailable("email"));
        sqlite3_bind_int64(s,5,external_id);sqlite3_bind_int(s,6,G_Identity.github.enabled);
        sqlite3_bind_int(s,8,G_Identity.wechat.enabled);sqlite3_bind_int64(s,10,member);}
    XA_BindText(s,7,G_Identity.github.id);XA_BindText(s,9,G_Identity.wechat.id);
    bool ok=s&&sqlite3_step(s)==SQLITE_ROW&&sqlite3_column_int(s,0)!=0;sqlite3_finalize(s);return ok;
}
