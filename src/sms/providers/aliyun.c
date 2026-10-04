/* Dysmsapi 2017-05-25; ACS3 signs the sorted RPC query and fixed headers. */
static const XASmsField XA_SmsAliyunFields[] = {
    {"access_key_id", "AccessKey ID", true, true, ""},
    {"access_key_secret", "AccessKey Secret", true, true, ""},
    {"sign_name", "短信签名", false, true, ""}
};
static bool XA_SmsAliyunBuild(const xvalue* o, const xvalue* t, const XASmsMessage* m, XASmsHttpRequest* r)
{
    const char* id = XA_Text(t, "id", 128); if (!id || !*id || strncmp(m->phone, "+86", 3) || strlen(m->phone) != 14) return false;
    xvalue* query = ValueObject(); char* params = XA_SmsJson(m->parameters);
    bool ok = query && params && ValueSetText(query, "OutId", m->request_id) &&
        ValueSetText(query, "PhoneNumbers", m->phone + 3) && ValueSetText(query, "SignName", XA_SmsOption(o, "sign_name")) &&
        ValueSetText(query, "TemplateCode", id) && ValueSetText(query, "TemplateParam", params);
    char* q = ok ? XA_SmsForm(query) : NULL; char* date = xrtTimeFormat(xrtNow(), 0, XRT_STR_LITERAL("%Y-%m-%dT%H:%M:%SZ"));
    char nonce[65], bodyhash[65], canonhash[65], signature[65];
    const char* signed_headers = "host;x-acs-action;x-acs-content-sha256;x-acs-date;x-acs-signature-nonce;x-acs-version";
    ok = q && date && XA_Random(nonce) && XA_SmsDigest("", bodyhash);
    char* canonical = ok ? xrtFormat("POST\n/\n%s\nhost:dysmsapi.aliyuncs.com\nx-acs-action:SendSms\nx-acs-content-sha256:%s\nx-acs-date:%s\nx-acs-signature-nonce:%s\nx-acs-version:2017-05-25\n\n%s\n%s",
        q, bodyhash, date, nonce, signed_headers, bodyhash) : NULL;
    ok = canonical && XA_SmsDigest(canonical, canonhash);
    char* signtext = ok ? xrtFormat("ACS3-HMAC-SHA256\n%s", canonhash) : NULL;
    ok = signtext && XA_SmsHmac(XA_SmsOption(o, "access_key_secret"), signtext, signature);
    char* auth = ok ? xrtFormat("ACS3-HMAC-SHA256 Credential=%s,SignedHeaders=%s,Signature=%s",
        XA_SmsOption(o, "access_key_id"), signed_headers, signature) : NULL;
    char* url = q ? xrtFormat("https://dysmsapi.aliyuncs.com/?%s", q) : NULL;
    ok = auth && url && XA_SmsHttpHeader(r, "Authorization", auth) &&
        XA_SmsHttpHeader(r, "x-acs-action", "SendSms") && XA_SmsHttpHeader(r, "x-acs-version", "2017-05-25") &&
        XA_SmsHttpHeader(r, "x-acs-date", date) && XA_SmsHttpHeader(r, "x-acs-signature-nonce", nonce) &&
        XA_SmsHttpHeader(r, "x-acs-content-sha256", bodyhash) && XA_SmsHttpBody(r, url, "application/x-www-form-urlencoded", "");
    XA_SmsSecretFree(params); XA_SmsSecretFree(q); XA_SmsSecretFree(canonical); XA_SmsSecretFree(signtext);
    XA_SmsSecretFree(auth); XA_SmsSecretFree(url); xrtFree(date); xrtValueRelease(query); return ok;
}
static void XA_SmsAliyunParse(int status, const xvalue* value, XASmsReceipt* r)
{
    (void)status; XA_SmsParseCode(value, "Code", "OK", "BizId", r);
}
static const XASmsProvider XA_SmsAliyun = {XA_SMS_ABI, "aliyun", "阿里云短信",
    XA_SmsAliyunFields, 3, NULL, XA_SmsAliyunBuild, XA_SmsAliyunParse,true};
