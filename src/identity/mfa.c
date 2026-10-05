/* Shared TOTP implementation. All calls run under G_RequestLock.
 * The separate 0600 key file must accompany encrypted database disaster recovery.
 * HMAC-SHA1 below composes the runtime SHA1 primitive for RFC 4226/6238. */
#define XA_MFA_SECRET_SIZE 20
#define XA_MFA_BLOB_SIZE 48
static xaesgcm G_MFACipher;
static bool G_MFACipherReady;
static const char XA_MFASchema1[] =
    "CREATE TABLE mfa_key(id INTEGER PRIMARY KEY CHECK(id=1),fingerprint TEXT NOT NULL);"
    "CREATE TABLE mfa_factor(realm TEXT NOT NULL CHECK(realm IN('admin','member')),owner INTEGER NOT NULL,enabled INTEGER NOT NULL DEFAULT 0 CHECK(enabled IN(0,1)),version INTEGER NOT NULL DEFAULT 0,secret BLOB,last_step INTEGER NOT NULL DEFAULT -1,failures INTEGER NOT NULL DEFAULT 0,locked_until INTEGER NOT NULL DEFAULT 0,updated_at INTEGER NOT NULL,PRIMARY KEY(realm,owner));"
    "CREATE TABLE mfa_setup(hash TEXT PRIMARY KEY,realm TEXT NOT NULL,owner INTEGER NOT NULL,session_id TEXT NOT NULL,version INTEGER NOT NULL,secret BLOB NOT NULL,expires_at INTEGER NOT NULL,attempts INTEGER NOT NULL DEFAULT 0);"
    "CREATE UNIQUE INDEX mfa_setup_owner ON mfa_setup(realm,owner);"
    "CREATE TABLE mfa_challenge(hash TEXT PRIMARY KEY,realm TEXT NOT NULL,owner INTEGER NOT NULL,version INTEGER NOT NULL,credential TEXT NOT NULL,remember INTEGER NOT NULL,created_at INTEGER NOT NULL,expires_at INTEGER NOT NULL,attempts INTEGER NOT NULL DEFAULT 0,consumed_at INTEGER NOT NULL DEFAULT 0);"
    "CREATE INDEX mfa_challenge_owner ON mfa_challenge(realm,owner,expires_at);"
    "CREATE TABLE mfa_recovery(realm TEXT NOT NULL,owner INTEGER NOT NULL,version INTEGER NOT NULL,hash TEXT NOT NULL,used_at INTEGER NOT NULL DEFAULT 0,PRIMARY KEY(realm,owner,hash));"
    "CREATE TABLE mfa_audit(id INTEGER PRIMARY KEY,realm TEXT NOT NULL,owner INTEGER NOT NULL,event TEXT NOT NULL,ip TEXT NOT NULL,created_at INTEGER NOT NULL);"
    "ALTER TABLE member_session ADD COLUMN mfa_version INTEGER NOT NULL DEFAULT 0;"
    "ALTER TABLE member_session ADD COLUMN mfa_verified_at INTEGER NOT NULL DEFAULT 0;";

typedef struct XAMFAFactor {
    int enabled; int64 version,last_step,locked_until;
    unsigned char secret[XA_MFA_BLOB_SIZE];
} XAMFAFactor;
static bool XA_MFAFactorRead(const char* realm,int64 owner,XAMFAFactor* f)
{
    memset(f,0,sizeof(*f));f->last_step=-1;
    sqlite3_stmt* s=XA_SQL("SELECT enabled,version,secret,last_step,locked_until FROM mfa_factor WHERE realm=? AND owner=?");
    XA_BindText(s,1,realm);if(s)sqlite3_bind_int64(s,2,owner);
    int rc=s?sqlite3_step(s):SQLITE_ERROR;bool ok=rc==SQLITE_DONE;
    if(rc==SQLITE_ROW){
        f->enabled=sqlite3_column_int(s,0);f->version=sqlite3_column_int64(s,1);
        f->last_step=sqlite3_column_int64(s,3);f->locked_until=sqlite3_column_int64(s,4);
        ok=!f->enabled||sqlite3_column_bytes(s,2)==XA_MFA_BLOB_SIZE;
        if(ok&&f->enabled)memcpy(f->secret,sqlite3_column_blob(s,2),XA_MFA_BLOB_SIZE);
    }
    sqlite3_finalize(s);return ok;
}
static bool XA_MFAValidSession(bool admin,int64 owner,int64 version,int64 verified)
{
    XAMFAFactor f;return XA_MFAFactorRead(admin?"admin":"member",owner,&f)&&f.version==version&&(!f.enabled||verified>0);
}
static bool XA_MFAKeyInit(void)
{
    unsigned char key[32]={0};char fingerprint[65];size_t n=0;
    const char* override=getenv("XADMIN_MFA_KEY_FILE");
    char* path=override&&*override?xrtStrDup(override):xrtPathJoin(DBPath,"mfa.key");
    if(!path)return false;
    xfileoptions o;xrtFileOptionsInit(&o);o.Flags=XFILE_READ|XFILE_NOFOLLOW;
    xfile file=xrtFileOpen(path,&o);bool ok=false;
    if(file){
        unsigned char extra;
        ok=xrtReadFull(file,key,sizeof(key),&n)&&xrtRead(file,&extra,1,&n)&&n==0;
        if(!xrtClose(file))ok=false;
    }else{
        sqlite3_stmt* s=XA_SQL("SELECT (SELECT count(*) FROM mfa_key)+(SELECT count(*) FROM mfa_factor WHERE enabled=1)+(SELECT count(*) FROM mfa_setup)");
        bool fresh=s&&sqlite3_step(s)==SQLITE_ROW&&sqlite3_column_int64(s,0)==0;sqlite3_finalize(s);
        if(fresh&&!xrtFileExists(path)&&xrtSecureRandom(key,sizeof(key))){
            o.Flags=XFILE_WRITE|XFILE_CREATE|XFILE_EXCLUSIVE|XFILE_NOFOLLOW|XFILE_SYNC;o.Mode=0600;o.Share=XFILE_SHARE_READ;
            file=xrtFileOpen(path,&o);
            if(file){ok=xrtWriteFull(file,key,sizeof(key),&n);if(!xrtClose(file))ok=false;}
        }
    }
    if(ok){unsigned char digest[32];ok=xrtSha256(key,sizeof(key),digest)&&XA_Hex(digest,32,fingerprint);xrtSecureZero(digest,sizeof(digest));}
    if(ok){
        sqlite3_stmt* s=XA_SQL("SELECT fingerprint FROM mfa_key WHERE id=1");
        int rc=s?sqlite3_step(s):SQLITE_ERROR;char expected[65];
        ok=rc==SQLITE_DONE||(rc==SQLITE_ROW&&XA_CopyColumn(s,0,expected,sizeof(expected))&&XA_IsHex(expected,64)&&xrtConstTimeEqual(expected,fingerprint,64));sqlite3_finalize(s);
        if(ok&&rc==SQLITE_DONE){s=XA_SQL("INSERT INTO mfa_key(id,fingerprint)VALUES(1,?)");XA_BindText(s,1,fingerprint);ok=XA_Done(s,true);}
    }
    if(ok)ok=xrtAesGcmInit(&G_MFACipher,key,sizeof(key),16);
    G_MFACipherReady=ok;xrtSecureZero(key,sizeof(key));xrtFree(path);
    if(!ok)printf("[xadmin][mfa] private key missing, invalid or unavailable; refusing startup\n");
    return ok;
}
static void XA_MFAKeyUnit(void) { if(G_MFACipherReady)xrtAesGcmClear(&G_MFACipher);G_MFACipherReady=false; }
static bool XA_MFACrypt(bool seal,const char* realm,int64 owner,int64 version,unsigned char* plain,unsigned char* blob)
{
    char aad[96];snprintf(aad,sizeof(aad),"xadmin.mfa.v1:%s:%lld:%lld",realm,(long long)owner,(long long)version);
    if(!G_MFACipherReady)return false;
    if(seal)return xrtSecureRandom(blob,12)&&xrtAesGcmSeal(&G_MFACipher,blob,12,aad,strlen(aad),plain,XA_MFA_SECRET_SIZE,blob+12,36);
    return xrtAesGcmOpen(&G_MFACipher,blob,12,aad,strlen(aad),blob+12,36,plain,XA_MFA_SECRET_SIZE);
}
static bool XA_MFATOTP(const unsigned char key[20],int64 step,char out[7])
{
    unsigned char pad[64],counter[8],inner[20],mac[20];xsha1 sha;size_t i;bool ok;
    for(i=0;i<64;i++)pad[i]=(i<20?key[i]:0)^0x36;
    for(i=0;i<8;i++)counter[7-i]=(unsigned char)((uint64)step>>(i*8));
    xrtSha1Init(&sha);ok=xrtSha1Update(&sha,pad,64)&&xrtSha1Update(&sha,counter,8)&&xrtSha1Final(&sha,inner);
    for(i=0;i<64;i++)pad[i]=(i<20?key[i]:0)^0x5c;
    xrtSha1Init(&sha);ok=ok&&xrtSha1Update(&sha,pad,64)&&xrtSha1Update(&sha,inner,20)&&xrtSha1Final(&sha,mac);
    if(ok){unsigned int offset=mac[19]&15;uint32 value=((uint32)(mac[offset]&127)<<24)|((uint32)mac[offset+1]<<16)|((uint32)mac[offset+2]<<8)|mac[offset+3];snprintf(out,7,"%06u",value%1000000);}
    xrtSecureZero(pad,sizeof(pad));xrtSecureZero(inner,sizeof(inner));xrtSecureZero(mac,sizeof(mac));xrtSecureZero(&sha,sizeof(sha));return ok;
}
static int64 XA_MFAMatch(const unsigned char* secret,const char* code,int64 last)
{
    if(!code||strlen(code)!=6)return -1;size_t i;for(i=0;i<6;i++)if(code[i]<'0'||code[i]>'9')return -1;
    int64 step=XA_Now()/30,found=-1;int delta;char expected[7];
    for(delta=-1;delta<=1;delta++)if(step+delta>last&&XA_MFATOTP(secret,step+delta,expected)&&xrtConstTimeEqual(expected,code,6))found=step+delta;
    xrtSecureZero(expected,sizeof(expected));return found;
}
static void XA_MFABase32(const unsigned char* secret,char out[33])
{
    const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";uint32 bits=0;int count=0;size_t i,n=0;
    for(i=0;i<20;i++){bits=(bits<<8)|secret[i];count+=8;while(count>=5){count-=5;out[n++]=alphabet[(bits>>count)&31];}}out[n]=0;
}
static bool XA_MFAAudit(const char* realm,int64 owner,const char* event,const char* ip)
{
    sqlite3_stmt* s=XA_SQL("INSERT INTO mfa_audit(realm,owner,event,ip,created_at)VALUES(?,?,?,?,?)");
    XA_BindText(s,1,realm);if(s){sqlite3_bind_int64(s,2,owner);sqlite3_bind_int64(s,5,XA_Now());}
    XA_BindText(s,3,event);XA_BindText(s,4,ip?ip:"");return XA_Done(s,true);
}
static bool XA_MFACredential(const char* realm,int64 owner,char hash[65])
{
    sqlite3_stmt* s=XA_SQL(!strcmp(realm,"admin")?
        "SELECT user,salt,pwd,role,authLevel FROM user WHERE id=? AND isDelete=0":
        "SELECT username,salt,pwd,groupId,authLevel FROM member WHERE id=? AND isDelete=0 AND status=1");
    if(s)sqlite3_bind_int64(s,1,owner);bool ok=false;
    if(s&&sqlite3_step(s)==SQLITE_ROW){
        xvalue* v=ValueArray();int i;bool built=v!=NULL;
        for(i=0;i<5&&built;i++)built=ValueArrayOwn(v,xrtValueString(xrtStrView((const char*)sqlite3_column_text(s,i))));
        size_t n=0;char* text=built?xrtJsonStringify(v,false,&n):NULL;ok=text&&XA_Hash(text,hash);
        if(text)xrtSecureZero(text,n);xrtFree(text);xrtValueRelease(v);
    }sqlite3_finalize(s);return ok;
}
static bool XA_MFACancel(const char* realm,int64 owner)
{
    sqlite3_stmt* s=XA_SQL("DELETE FROM mfa_challenge WHERE realm=? AND owner=?");
    XA_BindText(s,1,realm);if(s)sqlite3_bind_int64(s,2,owner);return XA_Done(s,false);
}
static bool XA_MFAChallengeCreate(XAdminRequest* req,const char* realm,int64 owner,bool remember,char token[65])
{
    XAMFAFactor f;char hash[65],credential[65];
    if(!XA_MFAFactorRead(realm,owner,&f)||!f.enabled||f.locked_until>XA_Now()||!XA_Random(token)||!XA_Hash(token,hash)||!XA_MFACredential(realm,owner,credential))return false;
    sqlite3_stmt* s=XA_SQL("DELETE FROM mfa_challenge WHERE expires_at<?");if(s)sqlite3_bind_int64(s,1,XA_Now());if(!XA_Done(s,false))return false;
    s=XA_SQL("SELECT count(*) FROM mfa_challenge WHERE realm=? AND owner=? AND consumed_at=0");XA_BindText(s,1,realm);if(s)sqlite3_bind_int64(s,2,owner);
    bool bounded=s&&sqlite3_step(s)==SQLITE_ROW&&sqlite3_column_int(s,0)<5;sqlite3_finalize(s);if(!bounded)return false;
    s=XA_SQL("INSERT INTO mfa_challenge(hash,realm,owner,version,credential,remember,created_at,expires_at)VALUES(?,?,?,?,?,?,?,?)");
    XA_BindText(s,1,hash);XA_BindText(s,2,realm);XA_BindText(s,5,credential);
    if(s){sqlite3_bind_int64(s,3,owner);sqlite3_bind_int64(s,4,f.version);sqlite3_bind_int(s,6,remember);sqlite3_bind_int64(s,7,XA_Now());sqlite3_bind_int64(s,8,XA_Now()+300);}
    return XA_Done(s,true);
}
static char* XA_MFAPendingHeaders(XAdminRequest* req,const char* challenge)
{
    const char* secure=(G_Identity.secure_cookie||req->raw->tls)?"; Secure":"";
    return xrtFormat("Set-Cookie: MSID=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0%s\r\nSet-Cookie: MCSRF=; Path=/; SameSite=Lax; Max-Age=0%s\r\nSet-Cookie: MMFA=%s; Path=/api/v1/auth/mfa/; HttpOnly; SameSite=Lax; Max-Age=300%s\r\n",secure,secure,challenge,secure);
}
static bool XA_MFARecoveryHash(const char* realm,int64 owner,int64 version,const char* code,char out[65])
{
    char normalized[33];size_t i,n=0;if(!code||strlen(code)>40)return false;
    for(i=0;code[i];i++){char c=code[i];if(c=='-'||c==' ')continue;if(c>='A'&&c<='F')c+=32;if(n>=32||!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;normalized[n++]=c;}
    normalized[n]=0;if(n!=32)return false;
    char* text=xrtFormat("xadmin.mfa.recovery:%s:%lld:%lld:%s",realm,(long long)owner,(long long)version,normalized);
    bool ok=text&&XA_Hash(text,out);if(text)xrtSecureZero(text,strlen(text));xrtFree(text);xrtSecureZero(normalized,sizeof(normalized));return ok;
}
/* Returns 200/401/429/500. Successful verification consumes the factor atomically.
 * A later failed operation may require the next TOTP, never reuse a recovery code. */
static int XA_MFAVerify(const char* realm,int64 owner,const char* code,const char* ip)
{
    XAMFAFactor f;unsigned char secret[20]={0};char recovery[65]={0};int64 step=-1;
    if(!XA_MFAFactorRead(realm,owner,&f)||!f.enabled)return 500;
    if(f.locked_until>XA_Now())return 429;
    if(!XA_MFACrypt(false,realm,owner,f.version,secret,f.secret))return 500;
    step=XA_MFAMatch(secret,code,f.last_step);xrtSecureZero(secret,sizeof(secret));
    bool is_recovery=step<0&&XA_MFARecoveryHash(realm,owner,f.version,code,recovery),accepted=step>=0;
    sqlite3_stmt* s=NULL;
    if(is_recovery){s=XA_SQL("SELECT 1 FROM mfa_recovery WHERE realm=? AND owner=? AND version=? AND hash=? AND used_at=0");
        XA_BindText(s,1,realm);XA_BindText(s,4,recovery);if(s){sqlite3_bind_int64(s,2,owner);sqlite3_bind_int64(s,3,f.version);}accepted=s&&sqlite3_step(s)==SQLITE_ROW;sqlite3_finalize(s);}
    if(!accepted){
        s=XA_SQL("UPDATE mfa_factor SET failures=CASE WHEN locked_until>0 AND locked_until<=? THEN 1 ELSE failures+1 END,locked_until=CASE WHEN (CASE WHEN locked_until>0 AND locked_until<=? THEN 0 ELSE failures END)>=4 THEN ? ELSE 0 END WHERE realm=? AND owner=? AND version=?");
        if(s){sqlite3_bind_int64(s,1,XA_Now());sqlite3_bind_int64(s,2,XA_Now());sqlite3_bind_int64(s,3,XA_Now()+300);sqlite3_bind_int64(s,5,owner);sqlite3_bind_int64(s,6,f.version);}XA_BindText(s,4,realm);
        if(!XA_Done(s,true)||!XA_MFAAudit(realm,owner,"verification_failed",ip))return 500;return 401;
    }
    if(!XA_Begin())return 500;bool ok=true;
    if(is_recovery){s=XA_SQL("UPDATE mfa_recovery SET used_at=? WHERE realm=? AND owner=? AND version=? AND hash=? AND used_at=0");
        if(s){sqlite3_bind_int64(s,1,XA_Now());sqlite3_bind_int64(s,3,owner);sqlite3_bind_int64(s,4,f.version);}XA_BindText(s,2,realm);XA_BindText(s,5,recovery);ok=XA_Done(s,true);}
    if(ok){s=XA_SQL("UPDATE mfa_factor SET last_step=CASE WHEN ?>=0 THEN ? ELSE last_step END,failures=0,locked_until=0 WHERE realm=? AND owner=? AND version=? AND enabled=1");
        if(s){sqlite3_bind_int64(s,1,step);sqlite3_bind_int64(s,2,step);sqlite3_bind_int64(s,4,owner);sqlite3_bind_int64(s,5,f.version);}XA_BindText(s,3,realm);ok=XA_Done(s,true);}
    ok=ok&&XA_MFAAudit(realm,owner,is_recovery?"recovery_used":"verified",ip);return XA_End(ok)?200:500;
}
static xvalue* XA_MFARecoveryGenerate(const char* realm,int64 owner,int64 version)
{
    xvalue* codes=ValueArray();sqlite3_stmt* s=XA_SQL("DELETE FROM mfa_recovery WHERE realm=? AND owner=?");
    XA_BindText(s,1,realm);if(s)sqlite3_bind_int64(s,2,owner);bool ok=codes&&XA_Done(s,false);int i;
    for(i=0;i<10&&ok;i++){
        unsigned char bytes[16];char raw[33],display[36],hash[65];ok=xrtSecureRandom(bytes,sizeof(bytes))&&XA_Hex(bytes,16,raw);
        if(ok){snprintf(display,sizeof(display),"%.8s-%.8s-%.8s-%.8s",raw,raw+8,raw+16,raw+24);ok=XA_MFARecoveryHash(realm,owner,version,display,hash);}
        if(ok){s=XA_SQL("INSERT INTO mfa_recovery(realm,owner,version,hash)VALUES(?,?,?,?)");XA_BindText(s,1,realm);XA_BindText(s,4,hash);if(s){sqlite3_bind_int64(s,2,owner);sqlite3_bind_int64(s,3,version);}ok=XA_Done(s,true)&&ValueArrayOwn(codes,xrtValueString(xrtStrView(display)));}
        xrtSecureZero(bytes,sizeof(bytes));xrtSecureZero(raw,sizeof(raw));xrtSecureZero(display,sizeof(display));
    }
    if(!ok){xrtValueRelease(codes);return NULL;}return codes;
}
