static const XASmsField XA_SmsYunpianFields[] = {{"api_key", "API Key", true, true, ""}};
static bool XA_SmsYunpianBuild(const xvalue* o, const xvalue* t, const XASmsMessage* m, XASmsHttpRequest* r)
{
    const char* id = XA_Text(t, "id", 128); if (!id || !*id || strspn(id, "0123456789") != strlen(id) ||
        strncmp(m->phone, "+86", 3) || strlen(m->phone) != 14) return false;
    xvalue* vars = ValueObject(); xvalue* data = ValueObject(); size_t i; bool ok = vars && data;
    xvalue* names = ValueGet(t, "parameters");
    for (i = 0; ok && i < ValueCount(names); ++i) {
        const char* name = ValueArrayText(names, i); char* key = xrtFormat("#%s#", name);
        ok = key && ValueSetRef(vars, key, ValueGet(m->parameters, name)); xrtFree(key);
    }
    char* encoded = ok ? XA_SmsForm(vars) : NULL;
    ok = encoded && ValueSetText(data, "apikey", XA_SmsOption(o, "api_key")) && ValueSetText(data, "mobile", m->phone + 3) &&
        ValueSetText(data, "tpl_id", id) && ValueSetText(data, "tpl_value", encoded) && ValueSetText(data, "uid", m->request_id) &&
        XA_SmsBodyValue(r, "https://sms.yunpian.com/v2/sms/tpl_single_send.json", data, true);
    XA_SmsSecretFree(encoded); xrtValueRelease(vars); xrtValueRelease(data); return ok;
}
static void XA_SmsYunpianParse(int status, const xvalue* value, XASmsReceipt* r)
{
    (void)status; int64 code, sid;
    if (!xrtValueGetInt(ValueGet(value, "code"), &code)) return;
    snprintf(r->provider_code, sizeof(r->provider_code), "%lld", (long long)code);
    r->status = code == 0 ? XA_SMS_ACCEPTED : code == -50 ? XA_SMS_UNKNOWN : XA_SMS_FAILED;
    if (xrtValueGetInt(ValueGet(value, "sid"), &sid)) snprintf(r->message_id, sizeof(r->message_id), "%lld", (long long)sid);
}
static const XASmsProvider XA_SmsYunpian = {XA_SMS_ABI, "yunpian", "云片短信",
    XA_SmsYunpianFields, 1, NULL, XA_SmsYunpianBuild, XA_SmsYunpianParse,true};
