static const XASmsField XA_SmsRonglianFields[] = {
    {"account_sid", "Account SID", true, true, ""}, {"auth_token", "Auth Token", true, true, ""},
    {"app_id", "App ID", false, true, ""}
};
static bool XA_SmsRonglianValid(const xvalue* o) { return XA_SmsName(XA_SmsOption(o, "account_sid")); }
static bool XA_SmsRonglianBuild(const xvalue* o, const xvalue* t, const XASmsMessage* m, XASmsHttpRequest* r)
{
    const char* id = XA_Text(t, "id", 128); if (!id || !*id || strncmp(m->phone, "+86", 3) || strlen(m->phone) != 14) return false;
    char* date = xrtTimeFormat(xrtNow(), 8 * 60 * 60, XRT_STR_LITERAL("%Y%m%d%H%M%S"));
    const char* sid = XA_SmsOption(o, "account_sid");
    char* signtext = date ? xrtFormat("%s%s%s", sid, XA_SmsOption(o, "auth_token"), date) : NULL;
    char* authtext = date ? xrtFormat("%s:%s", sid, date) : NULL;
    unsigned char digest[16]; char sig[33]; size_t i;
    bool ok = signtext && authtext && xrtMd5(signtext, strlen(signtext), digest) && XA_Hex(digest,16,sig);
    for (i = 0; ok && i < 32; ++i) if (sig[i] >= 'a' && sig[i] <= 'f') sig[i] -= 32;
    char* auth = ok ? xrtBase64EncodeNew(authtext, strlen(authtext), NULL) : NULL;
    char* url = ok ? xrtFormat("https://app.cloopen.com:8883/2013-12-26/Accounts/%s/SMS/TemplateSMS?sig=%s", sid, sig) : NULL;
    xvalue* body = ValueObject();
    ok = auth && url && body && ValueSetText(body, "to", m->phone + 3) && ValueSetText(body, "appId", XA_SmsOption(o, "app_id")) &&
        ValueSetText(body, "templateId", id) && ValueSetOwn(body, "datas", XA_SmsOrdered(t, m)) &&
        XA_SmsHttpHeader(r, "Authorization", auth) && XA_SmsBodyValue(r, url, body, false);
    xrtSecureZero(digest,16); XA_SmsSecretFree(signtext); XA_SmsSecretFree(authtext); XA_SmsSecretFree(auth);
    xrtFree(url); xrtFree(date); xrtValueRelease(body); return ok;
}
static void XA_SmsRonglianParse(int status, const xvalue* value, XASmsReceipt* r)
{
    (void)status; XA_SmsParseCode(value, "statusCode", "000000", "requestId", r);
    if (r->status == XA_SMS_ACCEPTED) XA_SmsCopyCode(r->message_id, sizeof(r->message_id), XA_Text(ValueGet(value,"templateSMS"),"smsMessageSid",256));
}
static const XASmsProvider XA_SmsRonglian = {XA_SMS_ABI, "ronglian", "容联云通讯",
    XA_SmsRonglianFields, 3, XA_SmsRonglianValid, XA_SmsRonglianBuild, XA_SmsRonglianParse,true};
