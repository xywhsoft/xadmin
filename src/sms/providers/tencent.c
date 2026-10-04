/* Sms 2021-01-11, TC3-HMAC-SHA256; ordered parameters follow template config. */
static const XASmsField XA_SmsTencentFields[] = {
    {"secret_id", "SecretId", true, true, ""}, {"secret_key", "SecretKey", true, true, ""},
    {"sdk_app_id", "SmsSdkAppId", false, true, ""}, {"sign_name", "短信签名", false, true, ""},
    {"region", "地域", false, true, "ap-guangzhou"}
};
static bool XA_SmsTencentBuild(const xvalue* o, const xvalue* t, const XASmsMessage* m, XASmsHttpRequest* r)
{
    const char* id = XA_Text(t, "id", 128); if (!id || !*id) return false;
    xvalue* body = ValueObject(); xvalue* phones = ValueArray();
    bool ok = body && phones && ValueArrayOwn(phones, xrtValueString(xrtStrView(m->phone)));
    if (ok) { ok = ValueSetOwn(body, "PhoneNumberSet", phones); phones = NULL; } xrtValueRelease(phones);
    ok = ok && ValueSetText(body, "SmsSdkAppId", XA_SmsOption(o, "sdk_app_id")) &&
        ValueSetText(body, "SignName", XA_SmsOption(o, "sign_name")) && ValueSetText(body, "TemplateId", id) &&
        ValueSetOwn(body, "TemplateParamSet", XA_SmsOrdered(t, m)) && ValueSetText(body, "SessionContext", m->request_id) &&
        XA_SmsBodyValue(r, "https://sms.tencentcloudapi.com/", body, false);
    char payload_hash[65], canonical_hash[65], signature[65], timestamp[32]; unsigned char kdate[32], kservice[32], ksign[32];
    xtime now=xrtNow(); char* date = xrtTimeFormat(now, 0, XRT_STR_LITERAL("%Y-%m-%d"));
    snprintf(timestamp, sizeof(timestamp), "%lld", (long long)(now/1000000));
    ok = ok && date && XA_SmsDigest(r->body, payload_hash);
    char* canonical = ok ? xrtFormat("POST\n/\n\ncontent-type:application/json\nhost:sms.tencentcloudapi.com\n\ncontent-type;host\n%s", payload_hash) : NULL;
    ok = canonical && XA_SmsDigest(canonical, canonical_hash);
    char* signtext = ok ? xrtFormat("TC3-HMAC-SHA256\n%s\n%s/sms/tc3_request\n%s", timestamp, date, canonical_hash) : NULL;
    char* secret = xrtFormat("TC3%s", XA_SmsOption(o, "secret_key")); unsigned char digest[32];
    ok = signtext && secret && xrtHmacSha256(secret, strlen(secret), date, strlen(date), kdate) &&
        xrtHmacSha256(kdate, 32, "sms", 3, kservice) && xrtHmacSha256(kservice, 32, "tc3_request", 11, ksign) &&
        xrtHmacSha256(ksign, 32, signtext, strlen(signtext), digest) && XA_Hex(digest, 32, signature);
    char* auth = ok ? xrtFormat("TC3-HMAC-SHA256 Credential=%s/%s/sms/tc3_request, SignedHeaders=content-type;host, Signature=%s",
        XA_SmsOption(o, "secret_id"), date, signature) : NULL;
    ok = auth && XA_SmsHttpHeader(r, "Authorization", auth) && XA_SmsHttpHeader(r, "X-TC-Action", "SendSms") &&
        XA_SmsHttpHeader(r, "X-TC-Version", "2021-01-11") && XA_SmsHttpHeader(r, "X-TC-Region", XA_SmsOption(o, "region")) &&
        XA_SmsHttpHeader(r, "X-TC-Timestamp", timestamp);
    xrtSecureZero(kdate,32); xrtSecureZero(kservice,32); xrtSecureZero(ksign,32); xrtSecureZero(digest,32);
    XA_SmsSecretFree(secret); XA_SmsSecretFree(canonical); XA_SmsSecretFree(signtext); XA_SmsSecretFree(auth);
    xrtFree(date); xrtValueRelease(body); return ok;
}
static void XA_SmsTencentParse(int status, const xvalue* value, XASmsReceipt* r)
{
    (void)status; xvalue* response = ValueGet(value, "Response"); xvalue* error = ValueGet(response, "Error");
    if (error) {
        XA_SmsParseCode(error, "Code", "Ok", "RequestId", r);
        if (r->status == XA_SMS_ACCEPTED) r->status = XA_SMS_UNKNOWN;
    }
    xvalue* set = ValueGet(response, "SendStatusSet");
    if (!error && xrtValueType(set) == XVALUE_ARRAY && ValueCount(set) == 1)
        XA_SmsParseCode(xrtValueArrayGet(set, 0), "Code", "Ok", "SerialNo", r);
    /* Provider-side timeouts have the same uncertainty as transport timeouts.
     * Preserve the code for diagnosis, but never trigger an automatic resend. */
    const char* code = r->provider_code;
    if (!strcmp(code, "InternalError.SendAndRecvFail") || !strcmp(code, "InternalError.Timeout") ||
        !strcmp(code, "InternalError.UnknownError") || !strcmp(code, "InternalError.OtherError"))
        r->status = XA_SMS_UNKNOWN;
}
static const XASmsProvider XA_SmsTencent = {XA_SMS_ABI, "tencent", "腾讯云短信",
    XA_SmsTencentFields, 5, NULL, XA_SmsTencentBuild, XA_SmsTencentParse,true};
