/* Domestic v2 template API, per chuanglanyunzhi/chuanglan-sms-cn official demo. */
static const XASmsField XA_SmsChuanglanFields[] = {
    {"account", "API 账号", true, true, ""}, {"password", "API 密码", true, true, ""},
    {"signature", "短信签名（含括号）", false, false, ""},
    {"notification_account", "通知短信 API 账号（独立账号时填写）", true, false, ""},
    {"notification_password", "通知短信 API 密码", true, false, ""}
};
static bool XA_SmsChuanglanValid(const xvalue* o)
{
    return (*XA_SmsOption(o,"notification_account")!=0)==(*XA_SmsOption(o,"notification_password")!=0);
}
static bool XA_SmsChuanglanBuild(const xvalue* o, const xvalue* t, const XASmsMessage* m, XASmsHttpRequest* r)
{
    const char* id = XA_Text(t, "id", 128); if (!id || !*id || strncmp(m->phone, "+86", 3) || strlen(m->phone) != 14) return false;
    char nonce[65], timestamp[32], md5[33], signature[65]; unsigned char digest[16];
    snprintf(timestamp,sizeof(timestamp),"%lld",(long long)XA_Now());
    bool notification=m->kind==XA_SMS_NOTIFICATION && *XA_SmsOption(o,"notification_account");
    const char* account=XA_SmsOption(o,notification?"notification_account":"account");
    const char* password = XA_SmsOption(o,notification?"notification_password":"password");
    bool ok = XA_Random(nonce) && xrtMd5(password,strlen(password),digest) && XA_Hex(digest,16,md5);
    nonce[32]=0; /* domestic v2 specifies exactly 32 characters */
    const char* sorted[3] = {md5,timestamp,nonce}; size_t i,j;
    for(i=0;ok && i<3;i++)for(j=i+1;j<3;j++)if(strcmp(sorted[i],sorted[j])>0){const char* s=sorted[i];sorted[i]=sorted[j];sorted[j]=s;}
    char* signtext = ok ? xrtFormat("%s%s%s",sorted[0],sorted[1],sorted[2]) : NULL;
    ok = signtext && XA_SmsHmac(md5,signtext,signature);
    xvalue* params = ValueArray(); xvalue* body = ValueObject();xvalue* positional=ValueObject();xvalue* names=ValueGet(t,"parameters");
    ok = ok && params && body && positional;
    for(i=0;ok && i<ValueCount(names);i++){
        char key[16];snprintf(key,sizeof(key),"param%u",(unsigned)(i+1));
        ok=ValueSetRef(positional,key,ValueGet(m->parameters,ValueArrayText(names,i)));
    }
    ok=ok && ValueArrayRef(params,positional);
    char* param_json = ok ? XA_SmsJson(params) : NULL;
    ok = param_json && ValueSetText(body,"account",account) && ValueSetText(body,"timestamp",timestamp) &&
        ValueSetText(body,"nonce",nonce) && ValueSetText(body,"phoneNumbers",m->phone+3) && ValueSetText(body,"templateId",id) &&
        ValueSetText(body,"templateParamJson",param_json) && ValueSetText(body,"signature",XA_SmsOption(o,"signature")) &&
        ValueSetText(body,"uid",m->request_id) && XA_SmsHttpHeader(r,"X-QA-Hmac-Signature",signature) &&
        XA_SmsBodyValue(r,"https://smssh.253.com/msg/sms/v2/tpl/send",body,false);
    xrtSecureZero(digest,16); xrtSecureZero(md5,sizeof(md5)); XA_SmsSecretFree(signtext); XA_SmsSecretFree(param_json);
    xrtValueRelease(positional);xrtValueRelease(params); xrtValueRelease(body); return ok;
}
static bool XA_SmsChuanglanCount(const xvalue* value, const char* key, int64* count)
{
    if (xrtValueGetInt(ValueGet(value, key), count)) return *count >= 0 && *count <= 1;
    const char* text = XA_Text(value, key, 1);
    if (!text || (strcmp(text, "0") && strcmp(text, "1"))) return false;
    *count = text[0] - '0'; return true;
}
static void XA_SmsChuanglanParse(int status, const xvalue* value, XASmsReceipt* r)
{
    (void)status; XA_SmsParseCode(value,"code","000000","msgId",r);
    if (r->status != XA_SMS_ACCEPTED) return;
    int64 failed, accepted;
    /* This interface submits exactly one recipient. A successful envelope
     * alone does not prove that that recipient was accepted. */
    if (!XA_SmsChuanglanCount(value, "failNum", &failed) ||
        !XA_SmsChuanglanCount(value, "successNum", &accepted) || failed + accepted != 1)
        r->status = XA_SMS_UNKNOWN;
    else if (failed) r->status = XA_SMS_FAILED;
}
static const XASmsProvider XA_SmsChuanglan = {XA_SMS_ABI,"chuanglan","创蓝云智 / 253",
    XA_SmsChuanglanFields,5,XA_SmsChuanglanValid,XA_SmsChuanglanBuild,XA_SmsChuanglanParse,true};
