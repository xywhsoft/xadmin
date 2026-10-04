#include "../../include/xadmin/sms.h"

static const XASmsProvider* G_SmsProviders[16];
static size_t G_SmsProviderCount;
static bool G_SmsFrozen;
static XASmsTransportProc G_SmsTransport;
static void* G_SmsTransportContext;
static bool XA_SmsSetTransport(XASmsTransportProc transport, void* context)
{
    if(G_SmsFrozen)return false;
    G_SmsTransport=transport;G_SmsTransportContext=context;return true;
}
static bool XA_SmsName(const char* name)
{
    size_t i, n = name ? strlen(name) : 0;
    if (!n || n > 64) return false;
    for (i = 0; i < n; ++i)
        if (!((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= 'A' && name[i] <= 'Z') ||
              (name[i] >= '0' && name[i] <= '9') || name[i] == '_' || name[i] == '-')) return false;
    return true;
}
static const XASmsProvider* XA_SmsFind(const char* id)
{
    size_t i;
    if (!id) return NULL;
    for (i = 0; i < G_SmsProviderCount; ++i)
        if (!strcmp(id, G_SmsProviders[i]->id)) return G_SmsProviders[i];
    return NULL;
}
static bool XA_SmsRegister(const XASmsProvider* p)
{
    size_t i, j;
    if (G_SmsFrozen || !p || p->abi != XA_SMS_ABI || !XA_SmsName(p->id) ||
        !p->label || !p->build || !p->parse || p->field_count > 16 ||
        (p->field_count && !p->fields) || XA_SmsFind(p->id) || G_SmsProviderCount == 16) return false;
    for (i = 0; i < p->field_count; ++i) {
        if (!XA_SmsName(p->fields[i].name) || !p->fields[i].label ||
            (p->fields[i].secret && p->fields[i].default_value && *p->fields[i].default_value)) return false;
        for (j = 0; j < i; ++j) if (!strcmp(p->fields[i].name, p->fields[j].name)) return false;
    }
    G_SmsProviders[G_SmsProviderCount++] = p;
    return true;
}
static xvalue* XA_SmsCatalog(void)
{
    xvalue* catalog = ValueArray(); size_t i, j;
    if (!catalog) return NULL;
    for (i = 0; i < G_SmsProviderCount; ++i) {
        const XASmsProvider* p = G_SmsProviders[i];
        xvalue* item = ValueObject(); xvalue* fields = ValueArray();
        bool ok = item && fields && ValueSetText(item, "id", p->id) && ValueSetText(item, "label", p->label) &&
            ValueSetBool(item,"template_id_required",p->template_id_required);
        for (j = 0; ok && j < p->field_count; ++j) {
            const XASmsField* f = &p->fields[j]; xvalue* value = ValueObject();
            ok = value && ValueSetText(value, "name", f->name) && ValueSetText(value, "label", f->label) &&
                ValueSetBool(value, "secret", f->secret) && ValueSetBool(value, "required", f->required) &&
                ValueSetText(value, "default_value", f->default_value);
            if (ok) ok = ValueArrayOwn(fields, value); else xrtValueRelease(value);
        }
        if (ok) { ok = ValueSetOwn(item, "fields", fields); fields = NULL; }
        xrtValueRelease(fields);
        if (ok) ok = ValueArrayOwn(catalog, item); else xrtValueRelease(item);
        if (!ok) { xrtValueRelease(catalog); return NULL; }
    }
    return catalog;
}
