static const XASmsField XA_SmsBaiduFields[] = {
    {"access_key_id", "AccessKey ID", true, true, ""}, {"access_key_secret", "Secret Access Key", true, true, ""},
    {"signature_id", "短信签名 ID", false, true, ""}, {"region", "地域", false, true, "bj"}
};
static bool XA_SmsBaiduValid(const xvalue* o) { return XA_SmsName(XA_SmsOption(o, "region")); }
static bool XA_SmsBaiduBuild(const xvalue* o, const xvalue* t, const XASmsMessage* m, XASmsHttpRequest* r)
{
    const char* id = XA_Text(t, "id", 128); if (!id || !*id) return false;
    xvalue* data = ValueObject(); const char* phone = !strncmp(m->phone, "+86", 3) && strlen(m->phone) == 14 ? m->phone + 3 : m->phone;
    char* host = xrtFormat("smsv3.%s.baidubce.com", XA_SmsOption(o, "region"));
    char* url = host ? xrtFormat("https://%s/api/v3/sendSms", host) : NULL;
    bool ok = data && url && ValueSetText(data, "mobile", phone) && ValueSetText(data, "template", id) &&
        ValueSetText(data, "signatureId", XA_SmsOption(o, "signature_id")) && ValueSetRef(data, "contentVar", m->parameters) &&
        XA_SmsBodyValue(r, url, data, false);
    char* date = xrtTimeFormat(xrtNow(), 0, XRT_STR_LITERAL("%Y-%m-%dT%H:%M:%SZ"));
    char* encoded = date ? xoauth2UrlEncode(date) : NULL;
    char* prefix = date ? xrtFormat("bce-auth-v1/%s/%s/1800", XA_SmsOption(o, "access_key_id"), date) : NULL;
    char* canonical = host && encoded ? xrtFormat("POST\n/api/v3/sendSms\n\nhost:%s\nx-bce-date:%s", host, encoded) : NULL;
    char signing_key[65], signature[65];
    ok = ok && prefix && canonical && XA_SmsHmac(XA_SmsOption(o, "access_key_secret"), prefix, signing_key) &&
        XA_SmsHmac(signing_key, canonical, signature);
    char* auth = ok ? xrtFormat("%s/host;x-bce-date/%s", prefix, signature) : NULL;
    ok = auth && XA_SmsHttpHeader(r, "Authorization", auth) && XA_SmsHttpHeader(r, "x-bce-date", date);
    xrtSecureZero(signing_key, sizeof(signing_key)); XA_SmsSecretFree(auth); XA_SmsSecretFree(canonical);
    xrtFree(date); xrtFree(encoded); xrtFree(prefix); xrtFree(host); xrtFree(url); xrtValueRelease(data); return ok;
}
static void XA_SmsBaiduParse(int status, const xvalue* value, XASmsReceipt* r)
{
    (void)status; const char* code = XA_Text(value, "code", 128);
    if (!code) return;
    if (strcmp(code, "1000")) { XA_SmsParseCode(value, "code", "1000", "requestId", r); return; }
    xvalue* data = ValueGet(value, "data");
    if (xrtValueType(data) == XVALUE_ARRAY && ValueCount(data) == 1)
        XA_SmsParseCode(xrtValueArrayGet(data, 0), "code", "1000", "messageId", r);
}
static const XASmsProvider XA_SmsBaidu = {XA_SMS_ABI, "baidu", "百度智能云短信",
    XA_SmsBaiduFields, 4, XA_SmsBaiduValid, XA_SmsBaiduBuild, XA_SmsBaiduParse,true};
