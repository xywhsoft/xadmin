static const XASmsField XA_SmsHuaweiFields[] = {
    {"app_key", "APP Key", true, true, ""}, {"app_secret", "APP Secret", true, true, ""},
    {"endpoint", "APP 接入地址（HTTPS）", false, true, ""}, {"sender", "签名通道号", false, true, ""},
    {"signature", "签名名称（通用模板填写）", false, false, ""},
    {"wsse_digest", "WSSE 摘要格式：raw / hex", false, false, "raw"}
};
static bool XA_SmsHuaweiValid(const xvalue* o)
{
    XASmsUrl url; const char* endpoint = XA_SmsOption(o, "endpoint"); const char* mode = XA_SmsOption(o, "wsse_digest");
    if (!XA_SmsUrlParse(endpoint, &url) || strcmp(url.path, "/") || strpbrk(XA_SmsOption(o,"app_key"),"\"\\") ||
        (*mode && strcmp(mode,"raw") && strcmp(mode,"hex"))) return false;
    const char* suffix = ".myhuaweicloud.com"; size_t n = strlen(url.host), s = strlen(suffix);
    return n > s && !strcmp(url.host + n - s, suffix);
}
static bool XA_SmsHuaweiBuild(const xvalue* o, const xvalue* t, const XASmsMessage* m, XASmsHttpRequest* r)
{
    const char* id = XA_Text(t,"id",128); if(!id||!*id)return false;
    const char* endpoint = XA_SmsOption(o,"endpoint"); size_t n = strlen(endpoint);
    char* url = xrtFormat("%.*s/sms/batchSendSms/v1",(int)(n && endpoint[n-1]=='/'?n-1:n),endpoint);
    char nonce[65], hex[65]; unsigned char digest[32]; char* date = xrtTimeFormat(xrtNow(),0,XRT_STR_LITERAL("%Y-%m-%dT%H:%M:%SZ"));
    bool ok = date && XA_Random(nonce);
    char* signtext = ok ? xrtFormat("%s%s%s",nonce,date,XA_SmsOption(o,"app_secret")) : NULL;
    ok = signtext && xrtSha256(signtext,strlen(signtext),digest);
    char* encoded = NULL;
    if(ok){
        if(!strcmp(XA_SmsOption(o,"wsse_digest"),"hex")){XA_Hex(digest,32,hex);encoded=xrtBase64EncodeNew(hex,64,NULL);}
        else encoded=xrtBase64EncodeNew(digest,32,NULL);
    }
    char* wsse = encoded ? xrtFormat("UsernameToken Username=\"%s\",PasswordDigest=\"%s\",Nonce=\"%s\",Created=\"%s\"",
        XA_SmsOption(o,"app_key"),encoded,nonce,date) : NULL;
    xvalue* body = ValueObject(); xvalue* ordered = XA_SmsOrdered(t,m); char* params = ordered ? XA_SmsJson(ordered) : NULL;
    ok = wsse && url && body && params && ValueSetText(body,"from",XA_SmsOption(o,"sender")) && ValueSetText(body,"to",m->phone) &&
        ValueSetText(body,"templateId",id) && ValueSetText(body,"templateParas",params);
    if(ok && *XA_SmsOption(o,"signature"))ok=ValueSetText(body,"signature",XA_SmsOption(o,"signature"));
    ok = ok && XA_SmsHttpHeader(r,"Authorization","WSSE realm=\"SDP\",profile=\"UsernameToken\",type=\"Appkey\"") &&
        XA_SmsHttpHeader(r,"X-WSSE",wsse) && XA_SmsBodyValue(r,url,body,true);
    xrtSecureZero(digest,32); xrtSecureZero(hex,65); XA_SmsSecretFree(signtext); XA_SmsSecretFree(encoded);
    XA_SmsSecretFree(wsse); XA_SmsSecretFree(params); xrtFree(date); xrtFree(url); xrtValueRelease(body); xrtValueRelease(ordered); return ok;
}
static void XA_SmsHuaweiParse(int status, const xvalue* value, XASmsReceipt* r)
{
    (void)status; const char* code=XA_Text(value,"code",128);
    if(!code)return;
    if(strcmp(code,"000000")){XA_SmsParseCode(value,"code","000000","requestId",r);return;}
    xvalue* result = ValueGet(value,"result");
    if(xrtValueType(result)==XVALUE_ARRAY && ValueCount(result)==1)
        XA_SmsParseCode(xrtValueArrayGet(result,0),"status","000000","smsMsgId",r);
}
static const XASmsProvider XA_SmsHuawei = {XA_SMS_ABI,"huawei","华为云消息&短信",
    XA_SmsHuaweiFields,6,XA_SmsHuaweiValid,XA_SmsHuaweiBuild,XA_SmsHuaweiParse,true};
