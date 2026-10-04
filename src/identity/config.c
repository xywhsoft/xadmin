#include <xjwt.h>
#include <xoauth2.h>
typedef struct XAProviderConfig {
    bool enabled; char id[257],secret[513],callback[513],mode[32];
} XAProviderConfig;
typedef struct XAIdentityConfig {
    char issuer[257],audience[129],origin[513],country[5];
    bool registration,oauth_create,secure_cookie;
    char sms_url[1025],sms_token[513];
    XAProviderConfig github,wechat;
} XAIdentityConfig;
static XAIdentityConfig G_Identity;
static bool XA_ConfigString(const xvalue* v,const char* name,char* out,size_t size)
{
    const char* text=XA_Text(v,name,size-1);
    if(!ValueHas(v,name))return true;
    if(!text)return false;strcpy(out,text);return true;
}
static bool XA_ConfigProvider(xvalue* object,XAProviderConfig* p)
{
    if(!object)return true;
    if(xrtValueType(object)!=XVALUE_OBJECT)return false;
    p->enabled=ValueBool(object,"enabled");strcpy(p->mode,"website");
    if(!XA_ConfigString(object,"client_id",p->id,sizeof(p->id))||
       !XA_ConfigString(object,"client_secret",p->secret,sizeof(p->secret))||
       !XA_ConfigString(object,"callback",p->callback,sizeof(p->callback))||
       !XA_ConfigString(object,"mode",p->mode,sizeof(p->mode)))return false;
    return !p->enabled||(p->id[0]&&p->secret[0]&&p->callback[0]);
}
static bool XA_ConfigInit(void)
{
    char* path=xrtPathJoin(DBPath,"identity.json");xvalue* config=NULL;
    memset(&G_Identity,0,sizeof(G_Identity));strcpy(G_Identity.issuer,"xadmin:member");
    strcpy(G_Identity.audience,"member");G_Identity.registration=true;G_Identity.oauth_create=true;
    if(!path)return false;
    if(xrtFileExists(path)){
        size_t n=0;char* bytes=xrtFileReadAll(path,&n);
        if(bytes&&n<=16384)config=xrtJsonParse(xrtStrViewN(bytes,n));
        if(bytes)xrtSecureZero(bytes,n);xrtFree(bytes);
        if(xrtValueType(config)!=XVALUE_OBJECT){xrtFree(path);xrtValueRelease(config);return false;}
    }
    xrtFree(path);if(!config)return true;
    bool ok=XA_ConfigString(config,"issuer",G_Identity.issuer,sizeof(G_Identity.issuer))&&
       XA_ConfigString(config,"audience",G_Identity.audience,sizeof(G_Identity.audience))&&
       XA_ConfigString(config,"public_origin",G_Identity.origin,sizeof(G_Identity.origin))&&
       XA_ConfigString(config,"default_country_code",G_Identity.country,sizeof(G_Identity.country))&&
       XA_ConfigString(config,"sms_webhook_url",G_Identity.sms_url,sizeof(G_Identity.sms_url))&&
       XA_ConfigString(config,"sms_webhook_token",G_Identity.sms_token,sizeof(G_Identity.sms_token))&&
       XA_ConfigProvider(ValueGet(config,"github"),&G_Identity.github)&&
       XA_ConfigProvider(ValueGet(config,"wechat"),&G_Identity.wechat);
    if(ValueHas(config,"registration"))G_Identity.registration=ValueBool(config,"registration");
    if(ValueHas(config,"oauth_create_member"))G_Identity.oauth_create=ValueBool(config,"oauth_create_member");
    G_Identity.secure_cookie=!strncmp(G_Identity.origin,"https://",8);
    if(!G_Identity.issuer[0]||!G_Identity.audience[0])ok=false;
    if(G_Identity.country[0]){
        size_t n=strlen(G_Identity.country),i;
        if(n<2||n>4||G_Identity.country[0]!='+'||G_Identity.country[1]<'1'||G_Identity.country[1]>'9')ok=false;
        for(i=1;i<n;i++)if(G_Identity.country[i]<'0'||G_Identity.country[i]>'9')ok=false;
    }
    if(G_Identity.sms_url[0]&&(strncmp(G_Identity.sms_url,"https://",8)||strpbrk(G_Identity.sms_url,"\r\n")))ok=false;
    if(strpbrk(G_Identity.sms_token,"\r\n"))ok=false;
    /* Only a scheme+authority, never a request-controlled forwarded header. */
    if(G_Identity.origin[0]){
        size_t prefix=G_Identity.secure_cookie?8:7;
        const char* authority=strlen(G_Identity.origin)>prefix?G_Identity.origin+prefix:"";
        if(!G_Identity.secure_cookie&&strncmp(G_Identity.origin,"http://127.0.0.1:",17)&&
           strncmp(G_Identity.origin,"http://localhost:",17))ok=false;
        if(!*authority||strpbrk(authority,"/@?#\\ \r\n\t"))ok=false;
    }
    XAProviderConfig* providers[2]={&G_Identity.github,&G_Identity.wechat};int i;
    for(i=0;i<2;i++)if(providers[i]->enabled){
        size_t len=strlen(G_Identity.origin);
        if(!len||strncmp(providers[i]->callback,G_Identity.origin,len)||providers[i]->callback[len]!='/')ok=false;
        if(strpbrk(providers[i]->callback,"?#\r\n"))ok=false;
    }
    if(G_Identity.wechat.enabled&&strcmp(G_Identity.wechat.mode,"website")&&strcmp(G_Identity.wechat.mode,"web"))ok=false;
    xrtValueRelease(config);return ok;
}
static bool XA_OriginAllowed(XAdminRequest* req)
{
    bool bad=false;const xhttpfield* origin=XA_Header(req,"Origin",&bad);
    if(bad)return false;
    if(!origin)return true; /* Native API clients use explicit Bearer tokens. */
    if(G_Identity.origin[0])return xrtStrEqual(origin->Value,xrtStrView(G_Identity.origin));
    const xhttpfield* host=XA_Header(req,"Host",&bad);char expected[600];
    if(bad||!host||host->Value.Size>500)return false;
    snprintf(expected,sizeof(expected),"%s://%.*s",req->raw->tls?"https":"http",(int)host->Value.Size,host->Value.Data);
    return xrtStrEqual(origin->Value,xrtStrView(expected));
}
