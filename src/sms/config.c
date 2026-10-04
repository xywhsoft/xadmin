/* Private config is serialized into a fixed-size snapshot, so request I/O can
 * run without keeping pointers into the live config or holding the request lock. */
static bool XA_SmsTemplatesValid(const xvalue* templates, bool require_id)
{
    xvalueiter it = {0}; xvaluekey key; xvalue* t; bool ok = true;
    const char* const fields[] = {"type", "id", "parameters", "text"};
    if (xrtValueType(templates) != XVALUE_OBJECT || ValueCount(templates) > 16) return false;
    if (xrtValueIterBegin(templates, &it)) {
        while ((t = xrtValueIterNext(&it, &key))) {
            char name[65]; size_t i, j;
            if (key.Type != XVALUE_KEY_STRING || key.String.Size > 64 || memchr(key.String.Data, 0, key.String.Size)) { ok = false; break; }
            memcpy(name, key.String.Data, key.String.Size); name[key.String.Size] = 0;
            const char* type = XA_Text(t, "type", 16);
            const char* id = XA_Text(t, "id", 128); const char* text = XA_Text(t, "text", 2048);
            xvalue* params = ValueGet(t, "parameters"); bool has_code = false;
            if (!XA_SmsName(name) || xrtValueType(t) != XVALUE_OBJECT || !XA_ConfigFields(t, fields, 4) ||
                !type || (strcmp(type, "verification") && strcmp(type, "notification")) ||
                (ValueHas(t, "id") && !id) || (ValueHas(t, "text") && !text) ||
                (require_id && (!id || !*id)) || (id && strpbrk(id, "\r\n")) || (text && strpbrk(text, "\r\n")) ||
                xrtValueType(params) != XVALUE_ARRAY || ValueCount(params) > 16) { ok = false; break; }
            for (i = 0; i < ValueCount(params); ++i) {
                xstrview v;
                if (!xrtValueGetString(xrtValueArrayGet(params, i), &v) || v.Size > 64 || memchr(v.Data, 0, v.Size)) { ok = false; break; }
                char param[65]; memcpy(param, v.Data, v.Size); param[v.Size] = 0;
                if (!XA_SmsName(param)) { ok = false; break; }
                for (j = 0; j < i; ++j) if (!strcmp(ValueArrayText(params, j), param)) ok = false;
                if (!strcmp(param, "code")) has_code = true;
            }
            if (!strcmp(type, "verification") && !has_code) ok = false;
            if ((!strcmp(name,"verification") && strcmp(type,"verification")) ||
                (!strcmp(name,"contact_changed") && strcmp(type,"notification"))) ok=false;
            if (!ok) break;
        }
        xrtValueIterEnd(&it);
    }
    return ok;
}
static bool XA_SmsConfigDecode(const xvalue* value, XASmsConfig* config)
{
    const char* const names[] = {"enabled", "provider", "options", "templates"};
    memset(config, 0, sizeof(*config)); if (!value) return true;
    bool enabled = false;
    const char* id = XA_Text(value, "provider", 64);
    const XASmsProvider* p = XA_SmsFind(id);
    xvalue* options = ValueGet(value, "options");
    if (xrtValueType(value) != XVALUE_OBJECT || !XA_ConfigFields(value, names, 4) ||
        !XA_ConfigBool(value, "enabled", &enabled) || !p || xrtValueType(options) != XVALUE_OBJECT ||
        !XA_SmsTemplatesValid(ValueGet(value, "templates"), enabled && p->template_id_required)) return false;
    xvalueiter it = {0}; xvaluekey key; xvalue* field; bool ok = true; size_t i;
    if (xrtValueIterBegin(options, &it)) {
        while ((field = xrtValueIterNext(&it, &key))) {
            (void)field;
            for (i = 0; i < p->field_count; ++i)
                if (key.Type == XVALUE_KEY_STRING && xrtStrEqual(key.String, xrtStrView(p->fields[i].name))) break;
            if (i == p->field_count) { ok = false; break; }
        }
        xrtValueIterEnd(&it);
    }
    for (i = 0; ok && i < p->field_count; ++i) {
        const XASmsField* f = &p->fields[i]; const char* s = XA_Text(options, f->name, 1024);
        if (ValueHas(options, f->name) && (!s || strpbrk(s, "\r\n\t"))) ok = false;
        if (enabled && f->required && (!s || !*s)) ok = false;
    }
    if (ok && enabled && p->validate) ok = p->validate(options);
    size_t n = 0; char* json = ok ? xrtJsonStringify(value, false, &n) : NULL;
    ok = json && n <= XA_SMS_MAX_OPTIONS;
    if (ok) memcpy(config->json, json, n + 1);
    if (json) xrtSecureZero(json, n); xrtFree(json); return ok;
}
static xvalue* XA_SmsConfigValue(const XASmsConfig* config, bool redacted)
{
    xvalue* value = config->json[0] ? xrtJsonParse(xrtStrView(config->json)) : ValueObject();
    if (!config->json[0]) {
        if (!value || !ValueSetBool(value, "enabled", false) || !ValueSetText(value, "provider", "webhook") ||
            !ValueSetOwn(value, "options", ValueObject()) || !ValueSetOwn(value, "templates", ValueObject())) goto failed;
    }
    if (redacted) {
        const XASmsProvider* p = XA_SmsFind(ValueText(value, "provider")); size_t i;
        xvalue* options = ValueGet(value, "options"); xvalue* secrets = ValueObject();
        if (!p || !secrets) { xrtValueRelease(secrets); goto failed; }
        for (i = 0; i < p->field_count; ++i) if (p->fields[i].secret) {
            const char* text = ValueText(options, p->fields[i].name);
            if (!ValueSetBool(secrets, p->fields[i].name, text && *text)) { xrtValueRelease(secrets); goto failed; }
            xrtValueObjectRemove(options, xrtStrView(p->fields[i].name));
        }
        if (!ValueSetOwn(value, "secrets_configured", secrets)) goto failed;
    }
    return value;
failed:
    xrtValueRelease(value); return NULL;
}
static bool XA_SmsConfigMerge(xvalue* current, const xvalue* patch)
{
    const char* const fields[] = {"enabled", "provider", "options", "templates"};
    if (xrtValueType(patch) != XVALUE_OBJECT || !XA_ConfigFields(patch, fields, 4)) return false;
    const char* id = ValueText(patch, "provider");
    if (id && strcmp(id, ValueText(current, "provider")))
        if (!ValueSetOwn(current, "options", ValueObject())) return false;
    size_t i;
    for (i = 0; i < 4; ++i) if (ValueHas(patch, fields[i])) {
        xvalue* incoming = ValueGet(patch, fields[i]);
        if (i == 2) {
            if (xrtValueType(incoming) != XVALUE_OBJECT) return false;
            xvalueiter it = {0}; xvaluekey key; xvalue* field; bool ok = true;
            if (xrtValueIterBegin(incoming, &it)) {
                while ((field = xrtValueIterNext(&it, &key)))
                    if (!xrtValueObjectSet(ValueGet(current, "options"), key.String, field)) ok = false;
                xrtValueIterEnd(&it);
            }
            if (!ok) return false;
        } else if (!ValueSetRef(current, fields[i], incoming)) return false;
    }
    return true;
}
static bool XA_SmsAvailable(const XASmsConfig* config, const char* name)
{
    xvalue* value = XA_SmsConfigValue(config, false);
    bool ok = value && ValueBool(value, "enabled") && XA_SmsFind(ValueText(value, "provider")) &&
        ValueGet(ValueGet(value, "templates"), name);
    xrtValueRelease(value); return ok;
}
