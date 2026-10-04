static const XASmsField XA_SmsSubmailFields[] = {
    {"app_id", "App ID", false, true, ""}, {"app_key", "App Key", true, true, ""}
};
static bool XA_SmsSubmailBuild(const xvalue* o, const xvalue* t, const XASmsMessage* m, XASmsHttpRequest* r)
{
    const char* id = XA_Text(t, "id", 128); if (!id || !*id || strncmp(m->phone, "+86", 3) || strlen(m->phone) != 14) return false;
    xvalue* body = ValueObject(); char* params = XA_SmsJson(m->parameters);
    bool ok = body && params && ValueSetText(body, "appid", XA_SmsOption(o,"app_id")) &&
        ValueSetText(body, "signature", XA_SmsOption(o,"app_key")) && ValueSetText(body, "sign_type", "normal") &&
        ValueSetText(body, "to", m->phone + 3) && ValueSetText(body, "project", id) && ValueSetText(body, "vars", params) &&
        XA_SmsBodyValue(r, "https://api-v4.mysubmail.com/sms/xsend.json", body, true);
    XA_SmsSecretFree(params); xrtValueRelease(body); return ok;
}
static void XA_SmsSubmailParse(int status, const xvalue* value, XASmsReceipt* r)
{
    (void)status; XA_SmsParseCode(value, "status", "success", "send_id", r);
}
static const XASmsProvider XA_SmsSubmail = {XA_SMS_ABI, "submail", "SUBMAIL 赛邮",
    XA_SmsSubmailFields, 2, NULL, XA_SmsSubmailBuild, XA_SmsSubmailParse,true};
