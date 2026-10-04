/* Common request/response utilities are shared by small provider adapters. */
static void XA_SmsSecretFree(char* text)
{
    if (text) xrtSecureZero(text, strlen(text)); xrtFree(text);
}
static const char* XA_SmsOption(const xvalue* options, const char* name)
{
    const char* value = ValueText(options, name); return value ? value : "";
}
static char* XA_SmsJson(const xvalue* value) { return xrtJsonStringify(value, false, NULL); }
static char* XA_SmsForm(const xvalue* value)
{
    xbuffer output; xrtBufferInit(&output);
    xvalueiter it = {0}; xvaluekey key; xvalue* field; bool ok = true;
    if (xrtValueIterBegin(value, &it)) {
        while ((field = xrtValueIterNext(&it, &key))) {
            xstrview text;
            if (key.Type != XVALUE_KEY_STRING || !xrtValueGetString(field, &text)) { ok = false; break; }
            char* name = xoauth2UrlEncode(key.String.Data); char* encoded = xoauth2UrlEncode(text.Data);
            char* pair = name && encoded ? xrtFormat("%s%s=%s", output.Size ? "&" : "", name, encoded) : NULL;
            ok = pair && xrtBufferAppend(&output, (xbytesview){(cbytes)pair, strlen(pair)});
            XA_SmsSecretFree(name); XA_SmsSecretFree(encoded); XA_SmsSecretFree(pair);
            if (!ok) break;
        }
        xrtValueIterEnd(&it);
    }
    char* result = ok ? xrtFormat("%.*s", (int)output.Size, output.Size ? (char*)output.Data : "") : NULL;
    if (output.Data) xrtSecureZero(output.Data, output.Size); xrtBufferUnit(&output); return result;
}
static bool XA_SmsBodyValue(XASmsHttpRequest* request, const char* url, const xvalue* value, bool form)
{
    char* body = form ? XA_SmsForm(value) : XA_SmsJson(value);
    bool ok = body && XA_SmsHttpBody(request, url,
        form ? "application/x-www-form-urlencoded" : "application/json", body);
    XA_SmsSecretFree(body); return ok;
}
static xvalue* XA_SmsOrdered(const xvalue* t, const XASmsMessage* message)
{
    xvalue* array = ValueArray(); xvalue* names = ValueGet(t, "parameters"); size_t i;
    if (!array) return NULL;
    for (i = 0; i < ValueCount(names); ++i)
        if (!ValueArrayRef(array, ValueGet(message->parameters, ValueArrayText(names, i)))) { xrtValueRelease(array); return NULL; }
    return array;
}
static bool XA_SmsDigest(const char* text, char out[65])
{
    unsigned char bytes[32]; bool ok = text && xrtSha256(text, strlen(text), bytes);
    if (ok) XA_Hex(bytes, 32, out); xrtSecureZero(bytes, sizeof(bytes)); return ok;
}
static bool XA_SmsHmac(const char* key, const char* text, char out[65])
{
    unsigned char bytes[32]; bool ok = key && text && xrtHmacSha256(key, strlen(key), text, strlen(text), bytes);
    if (ok) XA_Hex(bytes, 32, out); xrtSecureZero(bytes, sizeof(bytes)); return ok;
}
static bool XA_SmsCopyCode(char* output, size_t cap, const char* text)
{
    size_t i, n = text ? strlen(text) : 0;
    if (!n || n >= cap) return false;
    for (i = 0; i < n; ++i) if ((unsigned char)text[i] < 33 || (unsigned char)text[i] > 126) return false;
    memcpy(output, text, n + 1); return true;
}
static void XA_SmsParseCode(const xvalue* value, const char* key, const char* success,
                            const char* id_key, XASmsReceipt* receipt)
{
    const char* code = XA_Text(value, key, 128);
    if (!code || !XA_SmsCopyCode(receipt->provider_code, sizeof(receipt->provider_code), code)) return;
    receipt->status = strcmp(code, success) ? XA_SMS_FAILED : XA_SMS_ACCEPTED;
    XA_SmsCopyCode(receipt->message_id, sizeof(receipt->message_id), XA_Text(value, id_key, 256));
}
static XASmsStatus XA_SmsSend(const XASmsConfig* config, const XASmsContext* context,
                             const XASmsMessage* input, XASmsReceipt* receipt)
{
    XASmsReceipt local = {0}; if (!receipt) receipt = &local;
    memset(receipt, 0, sizeof(*receipt)); receipt->status = XA_SMS_FAILED;
    if (!config || !context || !input || !input->phone || !XA_SmsName(input->template_name) ||
        input->kind < XA_SMS_VERIFICATION || input->kind > XA_SMS_NOTIFICATION ||
        xrtValueType(input->parameters) != XVALUE_OBJECT || ValueCount(input->parameters) > 16 ||
        (input->request_id && !XA_SmsName(input->request_id))) return receipt->status;
    XAIdentifier phone;
    if (!XA_IdentifierParse(input->phone, strlen(input->phone), NULL, &phone) ||
        phone.kind != XA_IDENTIFIER_PHONE || strcmp(phone.key, input->phone)) return receipt->status;
    xvalue* value = XA_SmsConfigValue(config, false); xvalue* parameters = ValueObject();
    const XASmsProvider* p = value ? XA_SmsFind(ValueText(value, "provider")) : NULL;
    xvalue* t = value ? ValueGet(ValueGet(value, "templates"), input->template_name) : NULL;
    const char* type = t ? ValueText(t, "type") : NULL;
    XASmsHttpRequest request = {0}; char* response = NULL;
    if (!p || !parameters || !ValueBool(value, "enabled") || !type ||
        strcmp(type, input->kind == XA_SMS_VERIFICATION ? "verification" : "notification")) goto done;
    xvalue* names = ValueGet(t, "parameters"); size_t i;
    for (i = 0; i < ValueCount(names); ++i) {
        const char* name = ValueArrayText(names, i); const char* text = XA_Text(input->parameters, name, 1024);
        if (!text || !*text || strpbrk(text, "\r\n") || !ValueSetText(parameters, name, text)) goto done;
    }
    if (input->kind == XA_SMS_NOTIFICATION && ValueCount(parameters) != ValueCount(input->parameters)) goto done;
    if (input->kind == XA_SMS_VERIFICATION) {
        const char* code = ValueText(parameters, "code");
        if (!code || strlen(code) != 6 || strspn(code, "0123456789") != 6) goto done;
    }
    XASmsMessage message = *input; message.parameters = parameters;
    if (!p->build(ValueGet(value, "options"), t, &message, &request)) goto done;
    receipt->status = XA_SMS_UNKNOWN;
    bool transported = context->transport ? context->transport(&request, &receipt->http_status, &response, context->transport_context) :
        XA_SmsHttp(context->borrowed_engine, context->ca_pem, &request, &receipt->http_status, &response);
    if (transported) {
        xjsonreadconfig limits; xrtJsonReadConfigInit(&limits);
        limits.MaxInputBytes = 65536; limits.MaxDepth = 8; limits.MaxValues = 256; limits.MaxStringBytes = 8192;
        xvalue* body = response && strlen(response) <= 65536 ? xrtJsonRead(xrtStrView(response), &limits) : NULL;
        int status = receipt->http_status;
        if (status >= 200 && status < 300 && xrtValueType(body) == XVALUE_OBJECT) p->parse(status, body, receipt);
        else if (status >= 400 && status < 500 && status != 408) receipt->status = XA_SMS_FAILED;
        xrtValueRelease(body);
    }
done:
    XA_SmsSecretFree(response); XA_SmsHttpUnit(&request);
    xrtValueRelease(parameters); xrtValueRelease(value); return receipt->status;
}
