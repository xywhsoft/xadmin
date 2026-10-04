/* PBKDF2-HMAC-SHA256 uses xrt's implementation. The stored record is self
 * describing, bounded before computation, and independent of login identifier.
 * Raw passwords travel only over site HTTPS (loopback dev is supported). */
#define XA_PASSWORD_ITERATIONS 600000
static bool XA_PasswordPolicy(const char* password) { return password&&strlen(password)>=8&&strlen(password)<=128; }
static bool XA_Unhex(const char* text,unsigned char* output,size_t bytes)
{
    size_t i;for(i=0;i<bytes;i++){
        char a=text[i*2],b=text[i*2+1];int x,y;
        x=a>='0'&&a<='9'?a-'0':a>='a'&&a<='f'?a-'a'+10:-1;
        y=b>='0'&&b<='9'?b-'0':b>='a'&&b<='f'?b-'a'+10:-1;
        if(x<0||y<0)return false;output[i]=(unsigned char)(x*16+y);
    }return true;
}
static bool XA_PasswordHash(const char* password,char out[257])
{
    unsigned char salt[16],key[32];char saltHex[33],keyHex[65];bool ok;
    if(!password||!strlen(password)||strlen(password)>128||!xrtSecureRandom(salt,16))return false;
    ok=xrtPbkdf2Sha256(password,strlen(password),salt,16,XA_PASSWORD_ITERATIONS,key,32);
    if(ok){XA_Hex(salt,16,saltHex);XA_Hex(key,32,keyHex);snprintf(out,257,"pbkdf2-sha256$600000$%s$%s",saltHex,keyHex);}
    xrtSecureZero(salt,sizeof(salt));xrtSecureZero(key,sizeof(key));xrtSecureZero(keyHex,sizeof(keyHex));return ok;
}
static bool XA_PasswordCheck(const XAAccount* account,const char* password)
{
    if(!password||!strlen(password)||strlen(password)>128)return false;
    if(!strncmp(account->password,"pbkdf2-sha256$",14)){
        const char* salt=account->password+21;unsigned char saltBytes[16],expected[32],actual[32];bool ok;
        if(strlen(account->password)!=118||strncmp(account->password,"pbkdf2-sha256$600000$",21)||salt[32]!='$')return false;
        if(!XA_Unhex(salt,saltBytes,16)||!XA_Unhex(salt+33,expected,32))return false;
        ok=xrtPbkdf2Sha256(password,strlen(password),saltBytes,16,XA_PASSWORD_ITERATIONS,actual,32)&&xrtConstTimeEqual(actual,expected,32);
        xrtSecureZero(actual,32);xrtSecureZero(expected,32);return ok;
    }
    /* Only stored legacy credentials are accepted here; there is no legacy
     * HTTP login protocol. Recompute its old client proof from raw password. */
    if(!account->username[0]||strlen(account->password)!=64||!account->salt[0])return false;
    char* input=xrtFormat("%s_xywhsoft_%s",account->username,password);char proof[65];char* hash=NULL;
    if(input&&XA_Hash(input,proof))hash=ServerHashPassword(account->username,account->salt,proof);
    bool ok=hash&&strlen(hash)==64&&xrtConstTimeEqual(hash,account->password,64);
    if(input)xrtSecureZero(input,strlen(input));xrtFree(input);
    if(hash)xrtSecureZero(hash,strlen(hash));xrtFree(hash);xrtSecureZero(proof,sizeof(proof));return ok;
}
static bool XA_PasswordCurrent(const XAAccount* snapshot)
{
    XAAccount current;return XA_AccountByID(snapshot->id,&current)&&current.status==1&&
        !strcmp(snapshot->username,current.username)&&!strcmp(snapshot->password,current.password)&&!strcmp(snapshot->salt,current.salt);
}
static bool XA_VerifyPassword(const XAAccount* account,const char* password)
{
    /* Request execution itself holds the xs script generation lease. No
     * SQLite statement or mutable cache is borrowed across this unlocked work. */
    xrtMutexUnlock(G_RequestLock);bool ok=XA_PasswordCheck(account,password);xrtMutexLock(G_RequestLock);
    return ok&&XA_PasswordCurrent(account);
}
static bool XA_SetPassword(int64 id,const char* record)
{
    if(!XA_Begin())return false;
    sqlite3_stmt* s=XA_SQL("UPDATE member SET salt='',pwd=?,updateTime=? WHERE id=? AND isDelete=0");
    XA_BindText(s,1,record);if(s){sqlite3_bind_int64(s,2,xrtNow());sqlite3_bind_int64(s,3,id);}
    return XA_End(XA_Done(s,true)&&XA_SecurityClose(id,"credentials_changed",NULL,NULL));
}
