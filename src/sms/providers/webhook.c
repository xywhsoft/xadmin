static const XASmsField XA_SmsWebhookFields[] = {
    {"url", "HTTPS Webhook 地址", false, true, ""},
    {"token", "Bearer token", true, true, ""}
};
static bool XA_SmsWebhookValid(const xvalue* o)
{
    XASmsUrl url; return XA_SmsUrlParse(XA_SmsOption(o, "url"), &url);
}
static bool XA_SmsWebhookBuild(const xvalue* o, const xvalue* t, const XASmsMessage* m, XASmsHttpRequest* r)
{
    (void)t; xvalue* data = ValueObject(); char* auth = xrtFormat("Bearer %s", XA_SmsOption(o, "token"));
    char* params = XA_SmsJson(m->parameters);
    bool ok = data && auth && params && ValueSetText(data, "phone", m->phone) &&
        ValueSetText(data, "type", m->kind == XA_SMS_VERIFICATION ? "verification" : "notification") &&
        ValueSetText(data, "template", m->template_name) && ValueSetText(data, "parameters", params) &&
        ValueSetText(data, "challenge_id", m->request_id) &&
        ValueSetText(data, "code", XA_SmsOption(m->parameters, "code")) &&
        ValueSetText(data, "purpose", XA_SmsOption(m->parameters, "purpose")) &&
        ValueSetText(data, "expires_in", XA_SmsOption(m->parameters, "expires_in")) &&
        XA_SmsHttpHeader(r, "Authorization", auth) && XA_SmsBodyValue(r, XA_SmsOption(o, "url"), data, true);
    XA_SmsSecretFree(auth); XA_SmsSecretFree(params); xrtValueRelease(data); return ok;
}
static void XA_SmsWebhookParse(int status, const xvalue* value, XASmsReceipt* r)
{
    (void)status; bool sent;
    if (xrtValueGetBool(ValueGet(value, "sent"), &sent)) r->status = sent ? XA_SMS_ACCEPTED : XA_SMS_FAILED;
    XA_SmsCopyCode(r->message_id, sizeof(r->message_id), XA_Text(value, "message_id", 256));
}
static const XASmsProvider XA_SmsWebhook = {XA_SMS_ABI, "webhook", "通用 HTTPS Webhook",
    XA_SmsWebhookFields, 2, XA_SmsWebhookValid, XA_SmsWebhookBuild, XA_SmsWebhookParse};
