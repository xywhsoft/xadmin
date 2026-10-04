#ifndef XADMIN_SMS_H
#define XADMIN_SMS_H
#include <xsbase.h>
#include <stdbool.h>
#include <stddef.h>

/* C interface table, owned by the application's xs generation. Register before
 * ServiceInit starts serving. Metadata and callbacks must remain valid until
 * ServiceUnit; duplicate IDs and late registration are rejected. */
#define XA_SMS_ABI 1u
#define XA_SMS_MAX_OPTIONS 8192u
#define XA_SMS_MAX_HEADERS 16u
typedef enum XASmsStatus {
    XA_SMS_UNKNOWN = -1, /* submission may have happened; do not blindly retry */
    XA_SMS_FAILED = 0,   /* local rejection or explicit provider rejection */
    XA_SMS_ACCEPTED = 1  /* provider accepted, not a handset delivery receipt */
} XASmsStatus;
typedef enum XASmsKind { XA_SMS_VERIFICATION, XA_SMS_NOTIFICATION } XASmsKind;
typedef struct XASmsField {
    const char* name;
    const char* label;
    bool secret;
    bool required;
    const char* default_value;
} XASmsField;
typedef struct XASmsMessage {
    XASmsKind kind;
    const char* phone;          /* normalized E.164, exactly one recipient */
    const char* template_name;  /* server-configured logical name */
    const xvalue* parameters;   /* string values; borrowed for this invocation */
    const char* request_id;     /* caller correlation ID; not a retry guarantee */
} XASmsMessage;
typedef struct XASmsReceipt {
    XASmsStatus status;
    int http_status;
    char provider_code[129];
    char message_id[257];
} XASmsReceipt;
/* Build callbacks use XA_SmsHttpHeader/Body; the framework wipes and frees
 * their output. Never put credentials or verification codes into diagnostics.
 * Parse receives only a bounded JSON object from a 2xx response. Invalid
 * responses remain UNKNOWN without invoking the parser. */
typedef struct XASmsHttpRequest {
    char url[2049], content_type[81];
    char names[XA_SMS_MAX_HEADERS][65], values[XA_SMS_MAX_HEADERS][1025];
    size_t header_count;
    char* body;
} XASmsHttpRequest;
typedef struct XASmsProvider {
    unsigned abi;
    const char* id;
    const char* label;
    const XASmsField* fields;
    size_t field_count;
    bool (*validate)(const xvalue* options);
    bool (*build)(const xvalue* options, const xvalue* template_config,
                  const XASmsMessage* message, XASmsHttpRequest* request);
    void (*parse)(int http_status, const xvalue* response, XASmsReceipt* receipt);
    bool template_id_required;
} XASmsProvider;
/* A copyable secret-bearing snapshot. No pointers into live configuration. */
typedef struct XASmsConfig { char json[XA_SMS_MAX_OPTIONS + 1]; } XASmsConfig;
/* Optional application transport injection. Returns an xrtFree-owned bounded
 * response; production defaults to verified native HTTPS. No HTTP test endpoint
 * or credential override is exposed to clients. */
typedef bool (*XASmsTransportProc)(const XASmsHttpRequest*, int* status, char** response, void* context);
typedef struct XASmsContext {
    void* borrowed_engine;
    XASmsTransportProc transport;
    void* transport_context;
    const char* ca_pem; /* optional private CA; NULL uses system roots, verification always on */
} XASmsContext;

static bool XA_SmsRegister(const XASmsProvider* provider);
static bool XA_SmsSetTransport(XASmsTransportProc transport, void* context);
static const XASmsProvider* XA_SmsFind(const char* id);
static xvalue* XA_SmsCatalog(void); /* owned; no secrets */
static bool XA_SmsConfigDecode(const xvalue* value, XASmsConfig* config);
static xvalue* XA_SmsConfigValue(const XASmsConfig* config, bool redacted);
static bool XA_SmsConfigMerge(xvalue* current, const xvalue* patch);
static bool XA_SmsAvailable(const XASmsConfig* config, const char* template_name);
/* Synchronous and bounded; call outside G_RequestLock, with a previously
 * captured config. Notification callers provide their own business permission,
 * recipient authorization and rate budget. There is no public bulk-send API. */
static XASmsStatus XA_SmsSend(const XASmsConfig* config, const XASmsContext* context,
                            const XASmsMessage* message, XASmsReceipt* receipt);
static bool XA_SmsHttpHeader(XASmsHttpRequest*, const char* name, const char* value);
static bool XA_SmsHttpBody(XASmsHttpRequest*, const char* url,
                          const char* content_type, const char* body);
#endif
