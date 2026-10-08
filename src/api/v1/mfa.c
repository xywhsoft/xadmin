#include "../../identity/qr/qrcodegen.c"

static bool XA_MFAPrimaryRecent(const char* realm,xvalue* session)
{
    int64 until=!strcmp(realm,"admin")?ValueInt(session,"primary_verified_at")+300:ValueInt(session,"reauth_until");
    return until>XA_Now()&&until<=XA_Now()+300;
}
static bool XA_MFARecent(const char* realm,xvalue* session)
{
    XAMFAFactor f;if(!XA_MFAFactorRead(realm,ValueInt(session,"id"),&f))return false;
    int64 at=ValueInt(session,"mfa_verified_at");
    return !f.enabled||(at>0&&at<=XA_Now()&&at+300>XA_Now());
}
static bool XA_MFAAdminSession(XAdminRequest* req,int64 owner,int64 version,int64 verified,bool remember,xvalue** result,char** headers)
{
    *result=NULL;*headers=NULL;if(!XA_MFAValidSession(true,owner,version,verified))return false;
    sqlite3_stmt* s=XA_SQL("SELECT user,role,authLevel FROM user WHERE id=? AND isDelete=0");if(s)sqlite3_bind_int64(s,1,owner);
    char username[257];int64 role=0,level=0,role_level=-1;
    bool ok=s&&sqlite3_step(s)==SQLITE_ROW&&XA_CopyColumn(s,0,username,sizeof(username));
    if(ok){role=sqlite3_column_int64(s,1);level=sqlite3_column_int64(s,2);}sqlite3_finalize(s);
    if(!ok||!Auth_DBRoleGetAccess(role,0,&role_level))return false;
    char* id=Util_Token();xvalue* session=id?Session_CreateAdmin(id,remember):NULL;
    char csrf[65];
    ok=session&&XA_Random(csrf)&&ValueSetText(session,"xid",id)&&ValueSetText(session,"user",username)&&
        ValueSetInt(session,"id",owner)&&ValueSetInt(session,"roleID",role)&&ValueSetInt(session,"authLevel",level>role_level?level:role_level)&&
        ValueSetInt(session,"mfa_version",version)&&ValueSetInt(session,"mfa_verified_at",verified)&&
        ValueSetInt(session,"primary_verified_at",XA_Now())&&ValueSetText(session,"_mfaCSRF",csrf);
    if(ok){char* cookies=Session_AdminHeaders(req,id,remember?Session_AdminRememberSeconds():-1,NULL);
        *headers=cookies?xrtFormat("%sCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n",cookies):NULL;xrtFree(cookies);}
    ok=ok&&*headers&&Session_StoreAdmin(id,session);xrtFree(id);
    if(!ok){xrtFree(*headers);*headers=NULL;xrtValueRelease(session);return false;}
    *result=session;return true;
}
static bool XA_MFAPrimaryVerify(XAdminRequest* req,const char* realm,xvalue* session,xvalue* body)
{
    const char* password=XA_Text(body,"password",128);int64 owner=ValueInt(session,"id");
    if(!password||!password[0])return XA_MFAPrimaryRecent(realm,session);
    if(!strcmp(realm,"admin")){
        if(!XA_IsHex(password,64))return false;
        sqlite3_stmt* s=XA_SQL("SELECT user,salt,pwd FROM user WHERE id=? AND isDelete=0");if(s)sqlite3_bind_int64(s,1,owner);
        char user[257],salt[257],stored[257];bool ok=s&&sqlite3_step(s)==SQLITE_ROW&&XA_CopyColumn(s,0,user,sizeof(user))&&XA_CopyColumn(s,1,salt,sizeof(salt))&&XA_CopyColumn(s,2,stored,sizeof(stored));sqlite3_finalize(s);
        char* computed=ok?ServerHashPassword(user,salt,password):NULL;
        ok=computed&&strlen(computed)==strlen(stored)&&xrtConstTimeEqual(computed,stored,strlen(stored));
        if(computed)xrtSecureZero(computed,strlen(computed));xrtFree(computed);xrtSecureZero(stored,sizeof(stored));
        if(ok)ValueSetInt(session,"primary_verified_at",XA_Now());return ok;
    }
    XAAccount a;bool ok=XA_AccountByID(owner,&a)&&XA_VerifyPassword(&a,password);xrtSecureZero(&a,sizeof(a));
    if(ok){xvalue* current=XA_SessionRead(ValueText(session,"sid"),NULL);ok=current&&ValueInt(current,"id")==owner;xrtValueRelease(current);}
    if(ok){
        sqlite3_stmt* s=XA_SQL("UPDATE member_session SET reauth_until=? WHERE sid=? AND revoked_at=0");
        if(s)sqlite3_bind_int64(s,1,XA_Now()+300);XA_BindText(s,2,ValueText(session,"sid"));ok=XA_Done(s,true);
        if(ok)ValueSetInt(session,"reauth_until",XA_Now()+300);
    }return ok;
}
static int XA_MFARequireProof(XAdminRequest* req,const char* realm,xvalue* session,xvalue* body)
{
    xmap* guard=!strcmp(realm,"admin")?G_GuardAdmin:G_GuardMember;
    if(Guard_Check(guard,req->remote))return 429;
    if(!XA_MFAPrimaryVerify(req,realm,session,body)){Guard_Failed(guard,req->remote);return 403;}
    XAMFAFactor f;if(!XA_MFAFactorRead(realm,ValueInt(session,"id"),&f))return 500;
    const char* code=XA_Text(body,"code",40);
    if(f.enabled&&code&&*code){
        int status=XA_MFAVerify(realm,ValueInt(session,"id"),code,req->remote);if(status!=200)return status;
        if(!strcmp(realm,"admin"))ValueSetInt(session,"mfa_verified_at",XA_Now());
        else{
            sqlite3_stmt* s=XA_SQL("UPDATE member_session SET mfa_verified_at=? WHERE sid=? AND revoked_at=0");
            if(s)sqlite3_bind_int64(s,1,XA_Now());XA_BindText(s,2,ValueText(session,"sid"));if(!XA_Done(s,true))return 500;
            ValueSetInt(session,"mfa_verified_at",XA_Now());
        }
    }
    if(f.enabled&&!XA_MFARecent(realm,session))return 403;
    return 200;
}
static char* XA_MFAEscape(const char* text)
{
    size_t n=strlen(text),i,p=0;char* out=xrtMalloc(n*3+1);const char* hex="0123456789ABCDEF";
    if(!out)return NULL;for(i=0;i<n;i++){unsigned char c=(unsigned char)text[i];
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.')out[p++]=c;
        else {out[p++]='%';out[p++]=hex[c>>4];out[p++]=hex[c&15];}}
    out[p]=0;return out;
}
static char* XA_MFAQR(const char* uri)
{
    uint8_t tmp[qrcodegen_BUFFER_LEN_MAX],qr[qrcodegen_BUFFER_LEN_MAX];
    if(!qrcodegen_encodeText(uri,tmp,qr,qrcodegen_Ecc_MEDIUM,1,qrcodegen_VERSION_MAX,qrcodegen_Mask_AUTO,true))return NULL;
    int size=qrcodegen_getSize(qr),x,y;size_t cap=(size_t)size*size*45+256,used;
    char* svg=xrtMalloc(cap);if(!svg)return NULL;
    used=(size_t)snprintf(svg,cap,"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 %d %d\" shape-rendering=\"crispEdges\"><rect width=\"100%%\" height=\"100%%\" fill=\"white\"/><path fill=\"black\" d=\"",size+8,size+8);
    for(y=0;y<size;y++)for(x=0;x<size;x++)if(qrcodegen_getModule(qr,x,y))used+=(size_t)snprintf(svg+used,cap-used,"M%d %dh1v1h-1z",x+4,y+4);
    snprintf(svg+used,cap-used,"\"/></svg>");xrtSecureZero(tmp,sizeof(tmp));xrtSecureZero(qr,sizeof(qr));return svg;
}
static void XA_MFAStatus(XAdminRequest* req,const char* realm,xvalue* session)
{
    int64 owner=ValueInt(session,"id");XAMFAFactor f;
    if(!XA_MFAFactorRead(realm,owner,&f)){XA_Reply(req,500,"MFA unavailable",NULL,NULL);return;}
    sqlite3_stmt* s=XA_SQL("SELECT count(*) FROM mfa_recovery WHERE realm=? AND owner=? AND version=? AND used_at=0");
    XA_BindText(s,1,realm);if(s){sqlite3_bind_int64(s,2,owner);sqlite3_bind_int64(s,3,f.version);}
    int remaining=s&&sqlite3_step(s)==SQLITE_ROW?sqlite3_column_int(s,0):-1;sqlite3_finalize(s);
    xvalue* data=ValueObject();bool ok=remaining>=0&&data&&ValueSetBool(data,"enabled",f.enabled)&&ValueSetText(data,"realm",realm)&&
        ValueSetText(data,"username",ValueText(session,!strcmp(realm,"admin")?"user":"username"))&&ValueSetInt(data,"recovery_codes_remaining",remaining)&&
        ValueSetBool(data,"primary_recent",XA_MFAPrimaryRecent(realm,session))&&ValueSetBool(data,"mfa_recent",XA_MFARecent(realm,session));
    if(!strcmp(realm,"admin")){
        char csrf[65];if(!XA_IsHex(ValueText(session,"_mfaCSRF"),64))ok=ok&&XA_Random(csrf)&&ValueSetText(session,"_mfaCSRF",csrf);
        ok=ok&&ValueSetText(data,"csrf_token",ValueText(session,"_mfaCSRF"));
    }
    XA_Reply(req,ok?200:500,ok?"success":"MFA unavailable",ok?data:NULL,NULL);xrtValueRelease(data);
}
static void XA_MFASetup(XAdminRequest* req,const char* realm,xvalue* session)
{
    int64 owner=ValueInt(session,"id");XAMFAFactor f;unsigned char secret[20]={0},blob[48];char id[65],hash[65],base32[33];
    if(!XA_MFAFactorRead(realm,owner,&f)||!xrtSecureRandom(secret,20)||!XA_Random(id)||!XA_Hash(id,hash)||
       !XA_MFACrypt(true,realm,owner,f.version+1,secret,blob)){xrtSecureZero(secret,sizeof(secret));XA_Reply(req,500,"MFA unavailable",NULL,NULL);return;}
    XA_MFABase32(secret,base32);xrtSecureZero(secret,sizeof(secret));
    char issuer[180];bool bad=false;const xhttpfield* host=XA_Header(req,"Host",&bad);
    snprintf(issuer,sizeof(issuer),"xAdmin %s %.*s",realm,!bad&&host?(int)(host->Value.Size<100?host->Value.Size:100):0,!bad&&host?host->Value.Data:"");
    char* label=xrtFormat("%s:%s",issuer,ValueText(session,!strcmp(realm,"admin")?"user":"username"));
    char* escaped_issuer=XA_MFAEscape(issuer);char* escaped_label=label?XA_MFAEscape(label):NULL;xrtFree(label);
    char* uri=escaped_issuer&&escaped_label?xrtFormat("otpauth://totp/%s?secret=%s&issuer=%s&algorithm=SHA1&digits=6&period=30",escaped_label,base32,escaped_issuer):NULL;
    char* svg=uri?XA_MFAQR(uri):NULL;xvalue* data=ValueObject();
    bool ok=data&&svg&&ValueSetText(data,"setup_id",id)&&ValueSetText(data,"secret",base32)&&ValueSetText(data,"otpauth_uri",uri)&&ValueSetText(data,"qr_svg",svg)&&ValueSetInt(data,"expires_in",600);
    if(ok&&XA_Begin()){
        sqlite3_stmt* s=XA_SQL("DELETE FROM mfa_setup WHERE realm=? AND owner=?");XA_BindText(s,1,realm);if(s)sqlite3_bind_int64(s,2,owner);ok=XA_Done(s,false);
        if(ok){s=XA_SQL("INSERT INTO mfa_setup(hash,realm,owner,session_id,version,secret,expires_at)VALUES(?,?,?,?,?,?,?)");XA_BindText(s,1,hash);XA_BindText(s,2,realm);XA_BindText(s,4,ValueText(session,!strcmp(realm,"admin")?"xid":"sid"));
            if(s){sqlite3_bind_int64(s,3,owner);sqlite3_bind_int64(s,5,f.version+1);sqlite3_bind_blob(s,6,blob,48,SQLITE_TRANSIENT);sqlite3_bind_int64(s,7,XA_Now()+600);}ok=XA_Done(s,true)&&XA_MFAAudit(realm,owner,"setup_started",req->remote);}
        ok=XA_End(ok);
    }else ok=false;
    XA_Reply(req,ok?200:500,ok?"scan and confirm":"MFA unavailable",ok?data:NULL,NULL);
    if(uri)xrtSecureZero(uri,strlen(uri));if(svg)xrtSecureZero(svg,strlen(svg));xrtSecureZero(base32,sizeof(base32));xrtFree(svg);xrtFree(uri);xrtFree(escaped_issuer);xrtFree(escaped_label);xrtValueRelease(data);
}
/* Rotate the current credential as well: previously copied Bearer/Cookie
 * credentials must not become MFA-authenticated just because enrollment did. */
static bool XA_MFARotateMember(XAdminRequest* req,int64 owner,int64 version,int64 verified,XATokenSet* tokens)
{
    return XA_SessionIssueVerified(req,owner,tokens,version,verified)&&XA_SessionRevokeAccount(owner,tokens->sid);
}
static void XA_MFAChange(XAdminRequest* req,const char* realm,xvalue* session,xvalue* body,const char* action)
{
    bool admin=!strcmp(realm,"admin"),confirm=!strcmp(action,"confirm"),disable=!strcmp(action,"disable");
    int64 owner=ValueInt(session,"id");XAMFAFactor f;char setup_hash[65]={0};unsigned char secret[20]={0},blob[48];int64 step=-1;
    if(!XA_MFAFactorRead(realm,owner,&f)||(!confirm&&!f.enabled)){XA_Reply(req,409,"MFA is not enabled",NULL,NULL);return;}
    if(confirm){
        const char* id=XA_Text(body,"setup_id",64);const char* code=XA_Text(body,"code",6);
        if(!XA_IsHex(id,64)||!XA_Hash(id,setup_hash)){XA_Reply(req,400,"invalid setup",NULL,NULL);return;}
        sqlite3_stmt* s=XA_SQL("SELECT secret FROM mfa_setup WHERE hash=? AND realm=? AND owner=? AND session_id=? AND version=? AND expires_at>? AND attempts<5");
        XA_BindText(s,1,setup_hash);XA_BindText(s,2,realm);XA_BindText(s,4,ValueText(session,admin?"xid":"sid"));
        if(s){sqlite3_bind_int64(s,3,owner);sqlite3_bind_int64(s,5,f.version+1);sqlite3_bind_int64(s,6,XA_Now());}
        bool ok=s&&sqlite3_step(s)==SQLITE_ROW&&sqlite3_column_bytes(s,0)==48;
        if(ok)memcpy(blob,sqlite3_column_blob(s,0),48);sqlite3_finalize(s);
        if(!ok){XA_Reply(req,401,"setup invalid or expired",NULL,NULL);return;}
        s=XA_SQL("UPDATE mfa_setup SET attempts=attempts+1 WHERE hash=? AND attempts<5");XA_BindText(s,1,setup_hash);ok=XA_Done(s,true)&&XA_MFACrypt(false,realm,owner,f.version+1,secret,blob);
        if(ok)step=XA_MFAMatch(secret,code,-1);xrtSecureZero(secret,sizeof(secret));
        if(step<0){XA_Reply(req,401,"verification invalid or expired",NULL,NULL);return;}
    }
    if(!XA_Begin()){XA_Reply(req,500,"MFA unavailable",NULL,NULL);return;}
    bool ok=true;int64 version=(confirm||disable)?f.version+1:f.version;sqlite3_stmt* s;
    if(confirm){
        s=XA_SQL("INSERT INTO mfa_factor(realm,owner,enabled,version,secret,last_step,updated_at)VALUES(?,?,1,?,?,?,?) ON CONFLICT(realm,owner)DO UPDATE SET enabled=1,version=excluded.version,secret=excluded.secret,last_step=excluded.last_step,failures=0,locked_until=0,updated_at=excluded.updated_at");
        XA_BindText(s,1,realm);if(s){sqlite3_bind_int64(s,2,owner);sqlite3_bind_int64(s,3,version);sqlite3_bind_blob(s,4,blob,48,SQLITE_TRANSIENT);sqlite3_bind_int64(s,5,step);sqlite3_bind_int64(s,6,XA_Now());}ok=XA_Done(s,true);
    }else if(disable){
        s=XA_SQL("UPDATE mfa_factor SET enabled=0,version=version+1,secret=NULL,last_step=-1,failures=0,locked_until=0,updated_at=? WHERE realm=? AND owner=? AND version=? AND enabled=1");
        XA_BindText(s,2,realm);if(s){sqlite3_bind_int64(s,1,XA_Now());sqlite3_bind_int64(s,3,owner);sqlite3_bind_int64(s,4,f.version);}ok=XA_Done(s,true);
        if(ok){s=XA_SQL("DELETE FROM mfa_recovery WHERE realm=? AND owner=?");XA_BindText(s,1,realm);if(s)sqlite3_bind_int64(s,2,owner);ok=XA_Done(s,false);}
    }
    xvalue* codes=ok&&!disable?XA_MFARecoveryGenerate(realm,owner,version):NULL;
    if(!disable&&!codes)ok=false;
    if(ok&&(confirm||disable)){s=XA_SQL("DELETE FROM mfa_setup WHERE realm=? AND owner=?");XA_BindText(s,1,realm);if(s)sqlite3_bind_int64(s,2,owner);ok=XA_Done(s,false);}
    if(ok)ok=XA_MFACancel(realm,owner)&&XA_MFAAudit(realm,owner,confirm?"enabled":disable?"disabled":"recovery_regenerated",req->remote);
    XATokenSet tokens={0};char* headers=NULL;xvalue* data=NULL; xvalue* renewed=NULL;
    if(ok&&!admin)ok=XA_MFARotateMember(req,owner,version,disable?0:XA_Now(),&tokens);
    if(ok){data=admin?ValueObject():XA_TokenData(&tokens);ok=data&&ValueSetBool(data,"enabled",!disable)&&(!codes||ValueSetRef(data,"recovery_codes",codes));}
    if(ok&&!admin){headers=XA_TokenHeaders(req,&tokens);ok=headers!=NULL;}
    if(ok&&admin)ok=XA_MFAAdminSession(req,owner,version,disable?0:XA_Now(),ValueBool(session,"_remember"),&renewed,&headers);
    ok=XA_End(ok);
    if(admin&&renewed){
        if(ok)Session_RevokeAccountExcept(true,owner,renewed);
        else Session_RemoveAdminByID(ValueText(renewed,"xid"));
    }
    xrtValueRelease(renewed);
    XA_Reply(req,ok?200:500,ok?"MFA updated":"MFA unavailable",ok?data:NULL,ok?headers:NULL);
    xrtFree(headers);xrtValueRelease(data);xrtValueRelease(codes);XA_TokensUnit(&tokens);
}
static void XA_MFALoginComplete(XAdminRequest* req,const char* realm,xvalue* body)
{
    const char* id=XA_Text(body,"challenge_id",64);const char* code=XA_Text(body,"code",40);
    if(!code)code=XA_Text(body,"recovery_code",40);
    char hash[65],credential[65],expected[65];int64 owner=0,version=0;bool remember=false;
    if(!XA_IsHex(id,64)||!XA_Hash(id,hash)||!code){XA_Reply(req,400,"invalid MFA challenge",NULL,NULL);return;}
    sqlite3_stmt* s=XA_SQL("SELECT owner,version,credential,remember FROM mfa_challenge WHERE hash=? AND realm=? AND consumed_at=0 AND expires_at>? AND attempts<5");
    XA_BindText(s,1,hash);XA_BindText(s,2,realm);if(s)sqlite3_bind_int64(s,3,XA_Now());
    bool ok=s&&sqlite3_step(s)==SQLITE_ROW&&XA_CopyColumn(s,2,expected,sizeof(expected))&&XA_IsHex(expected,64);
    if(ok){owner=sqlite3_column_int64(s,0);version=sqlite3_column_int64(s,1);remember=sqlite3_column_int(s,3);}sqlite3_finalize(s);
    XAMFAFactor f;
    ok=ok&&XA_MFAFactorRead(realm,owner,&f)&&f.enabled&&f.version==version&&XA_MFACredential(realm,owner,credential)&&xrtConstTimeEqual(credential,expected,64);
    if(!ok){XA_Reply(req,401,"MFA challenge invalid or expired",NULL,NULL);return;}
    s=XA_SQL("UPDATE mfa_challenge SET attempts=attempts+1 WHERE hash=? AND consumed_at=0 AND attempts<5");XA_BindText(s,1,hash);
    if(!XA_Done(s,true)){XA_Reply(req,500,"MFA unavailable",NULL,NULL);return;}
    int status=XA_MFAVerify(realm,owner,code,req->remote);
    if(status!=200){XA_Reply(req,status,status==429?"too many MFA attempts":"verification invalid, reused or expired",NULL,NULL);return;}
    XATokenSet tokens={0};char* headers=NULL;xvalue* data=NULL;bool admin=!strcmp(realm,"admin");
    if(!XA_Begin()){XA_Reply(req,500,"MFA unavailable",NULL,NULL);return;}
    s=XA_SQL("UPDATE mfa_challenge SET consumed_at=? WHERE hash=? AND consumed_at=0 AND expires_at>?");
    if(s){sqlite3_bind_int64(s,1,XA_Now());sqlite3_bind_int64(s,3,XA_Now());}XA_BindText(s,2,hash);ok=XA_Done(s,true);
    if(ok&&!admin)ok=XA_SessionIssueVerified(req,owner,&tokens,version,XA_Now());
    if(ok&&!admin){data=XA_TokenData(&tokens);headers=XA_TokenHeaders(req,&tokens);ok=data&&headers;}
    xvalue* new_session=NULL;
    if(ok&&admin)ok=XA_MFAAdminSession(req,owner,version,XA_Now(),remember,&new_session,&headers);
    ok=XA_End(ok);
    if(!ok&&new_session)Session_RemoveAdminByID(ValueText(new_session,"xid"));
    xrtValueRelease(new_session);
    if(ok&&admin){Guard_Reset(G_GuardAdmin,req->remote);xsHttpReplyAuto(req,200,headers,"{\"result\":true,\"message\":\"登录成功\"}",0);}
    else XA_Reply(req,ok?200:500,ok?"signed in":"MFA unavailable",ok?data:NULL,ok?headers:NULL);
    xrtFree(headers);xrtValueRelease(data);XA_TokensUnit(&tokens);
}
static void XA_MFACancelLogin(XAdminRequest* req,const char* realm,xvalue* body)
{
    const char* id=XA_Text(body,"challenge_id",64);char hash[65];
    if(!XA_IsHex(id,64)||!XA_Hash(id,hash)){XA_Reply(req,400,"invalid MFA challenge",NULL,NULL);return;}
    sqlite3_stmt* s=XA_SQL("DELETE FROM mfa_challenge WHERE hash=? AND realm=?");XA_BindText(s,1,hash);XA_BindText(s,2,realm);
    bool ok=XA_Done(s,false);XA_Reply(req,ok?200:500,ok?"cancelled":"MFA unavailable",NULL,
        !strcmp(realm,"member")?"Set-Cookie: MMFA=; Path=/api/v1/auth/mfa/; HttpOnly; SameSite=Lax; Max-Age=0\r\n":NULL);
}
static void XA_MFAHandler(XS_ServerObject server,XS_HostObject host,XAdminRequest* req,XAdminRequest* resp,xvalue* session)
{
    (void)server;(void)host;(void)resp;int method=xsReqMethodID(req);
    bool admin=!strncmp(req->path,"/admin/",7),cancel=!strcmp(req->path,"/api/v1/auth/mfa/cancel"),verify=!strcmp(req->path,"/api/v1/auth/mfa/verify"),pending=!strcmp(req->path,"/api/v1/auth/mfa/pending");
    const char* realm=admin?"admin":"member";const char* action=strrchr(req->path,'/')+1;
    if(!strcmp(req->path,"/admin/view/auth/mfa")){
        if(xrtValueType(session)!=XVALUE_OBJECT)XA_Reply(req,401,"unauthorized",NULL,NULL);
        else if(method!=XHTTP_METHOD_GET)XA_Reply(req,405,"method not allowed",NULL,NULL);
        else LoadPage(req,200,HTTP_CT_HTML,"admin/mfa.html");return;
    }
    bool read=pending||!strcmp(action,"mfa");
    if(method!=(read?XHTTP_METHOD_GET:XHTTP_METHOD_POST)){XA_Reply(req,405,"method not allowed",NULL,NULL);return;}
    if(pending){
        char token[65],hash[65];bool ok=XA_SameOrigin(req)&&XA_Cookie(req,"MMFA",token,sizeof(token))==64&&XA_IsHex(token,64)&&XA_Hash(token,hash);
        sqlite3_stmt* s=ok?XA_SQL("SELECT 1 FROM mfa_challenge WHERE hash=? AND realm='member' AND consumed_at=0 AND expires_at>? AND attempts<5"):NULL;
        XA_BindText(s,1,hash);if(s)sqlite3_bind_int64(s,2,XA_Now());ok=s&&sqlite3_step(s)==SQLITE_ROW;sqlite3_finalize(s);
        xvalue* data=ok?ValueObject():NULL;if(data){ValueSetBool(data,"mfa_required",true);ValueSetText(data,"challenge_id",token);}
        XA_Reply(req,ok?200:404,ok?"MFA required":"no pending MFA",data,NULL);xrtValueRelease(data);return;
    }
    if(!verify&&!cancel&&xrtValueType(session)!=XVALUE_OBJECT){XA_Reply(req,401,"unauthorized",NULL,NULL);return;}
    if(read){XA_MFAStatus(req,realm,session);return;}
    bool csrf=verify||cancel;
    if(admin){bool bad=false;const xhttpfield* h=XA_Header(req,"X-CSRF-Token",&bad);const char* expected=ValueText(session,"_mfaCSRF");csrf=!bad&&h&&h->Value.Size==64&&XA_IsHex(expected,64)&&xrtConstTimeEqual(h->Value.Data,expected,64);}
    else if(!verify&&!cancel)csrf=XA_RequestCSRF(req,session);
    if(!csrf||!(admin?XA_SameOrigin(req):XA_OriginAllowed(req))){XA_Reply(req,403,"CSRF verification failed",NULL,NULL);return;}
    xvalue* body=XA_Body(req);if(!body){XA_Reply(req,400,"a bounded JSON object is required",NULL,NULL);return;}
    if(cancel)XA_MFACancelLogin(req,"member",body);
    else if(verify)XA_MFALoginComplete(req,"member",body);
    else if(!strcmp(action,"confirm")){
        /* New-factor confirmation must not consume the old factor's code. */
        if(!XA_MFAPrimaryRecent(realm,session)||!XA_MFARecent(realm,session))XA_Reply(req,403,"confirm your identity again",NULL,NULL);
        else XA_MFAChange(req,realm,session,body,action);
    }else{
        int status=XA_MFARequireProof(req,realm,session,body);
        if(status!=200)XA_Reply(req,status,status==403?"confirm your identity and MFA again":status==429?"too many verification attempts":"verification invalid or expired",NULL,NULL);
        else if(!strcmp(action,"setup"))XA_MFASetup(req,realm,session);
        else if(!strcmp(action,"reauth"))XA_Reply(req,200,"identity confirmed",NULL,NULL);
        else if(!strcmp(action,"disable")||!strcmp(action,"recovery-codes"))XA_MFAChange(req,realm,session,body,action);
        else XA_Reply(req,404,"not found",NULL,NULL);
    }
    xrtValueRelease(body);
}
static void XA_MFARegisterRoutes(void)
{
    const char* paths[]={"/api/v1/auth/mfa/verify","/api/v1/auth/mfa/pending","/api/v1/auth/mfa/cancel","/api/v1/profile/mfa","/api/v1/profile/mfa/setup","/api/v1/profile/mfa/confirm","/api/v1/profile/mfa/disable","/api/v1/profile/mfa/recovery-codes","/api/v1/profile/mfa/reauth",
        "/admin/view/auth/mfa","/admin/auth/mfa","/admin/auth/mfa/setup","/admin/auth/mfa/confirm","/admin/auth/mfa/disable","/admin/auth/mfa/recovery-codes","/admin/auth/mfa/reauth"};
    size_t i;for(i=0;i<sizeof(paths)/sizeof(paths[0]);i++){
        RouteInfo* r=AddStaticRouteHTTP(paths[i],XHTTP_METHOD_ANY,XA_MFAHandler,true);
        if(r){r->bAdmin=!strncmp(paths[i],"/admin/",7);r->bAuth=false;}
    }
}
