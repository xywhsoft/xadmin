/* Transport injection only in this test composition. Production has fixed
 * provider endpoints and no mock/endpoint-override configuration switch. */
#define ServiceInit XAdmin_ServiceInit
#include "../main.c"
#undef ServiceInit
static bool MockOAuth(const char* method,const char* url,const char* body,const char* auth,char** response,int* status,void* context)
{
    (void)context;*response=NULL;*status=200;
    if(strstr(url,"github.com/login/oauth/access_token")){
        if(strcmp(method,"POST")||!body||!strstr(body,"code_verifier=")||!strstr(body,"client_id=github-test")){*status=400;return true;}
        if(strstr(body,"code=denied")){*status=403;return true;}
        const char* token=strstr(body,"code=gh_two")?"github-202":strstr(body,"code=gh_three")?"github-303":strstr(body,"code=renamed")?"github-renamed":"github-101";
        if(strstr(body,"code=delayed"))xrtSleep(300);
        *response=xrtFormat("{\"access_token\":\"%s\",\"token_type\":\"bearer\"}",token);return true;
    }
    if(strstr(url,"api.github.com/user")){
        if(strcmp(method,"GET")||!auth||strncmp(auth,"Bearer github-",14)){*status=401;return true;}
        int id=strstr(auth,"202")?202:strstr(auth,"303")?303:101;
        *response=xrtFormat("{\"id\":%d,\"login\":\"%s\",\"name\":\"Mock GitHub member\",\"email\":\"same@example.com\"}",id,strstr(auth,"renamed")?"new-name":"old-name");return true;
    }
    if(strstr(url,"api.weixin.qq.com/sns/oauth2/access_token")){
        if(strcmp(method,"GET")||!strstr(url,"appid=wechat-test")||!strstr(url,"secret=wechat-secret")||!strstr(url,"grant_type=authorization_code")){*status=400;return true;}
        const char* token=strstr(url,"code=bad_openid")?"mismatch":"wechat-token";
        *response=xrtFormat("{\"access_token\":\"%s\",\"openid\":\"wx_openid_1\",\"expires_in\":7200}",token);return true;
    }
    if(strstr(url,"api.weixin.qq.com/sns/userinfo")){
        if(strcmp(method,"GET")||auth||!strstr(url,"openid=wx_openid_1")||!strstr(url,"access_token=")){*status=400;return true;}
        *response=xrtFormat("{\"openid\":\"%s\",\"unionid\":\"wx_union_1\",\"nickname\":\"Mock WeChat member\"}",strstr(url,"access_token=mismatch")?"different_openid":"wx_openid_1");return true;
    }
    *status=404;return true;
}
void ServiceInit(XS_HostInfo* host)
{
    XAdmin_ServiceInit(host);if(!G_Ready)return;G_Ready=false;
    G_IdentityOAuthTransport=MockOAuth;G_IdentityOAuthContext=NULL;
    G_Ready=true;
}
