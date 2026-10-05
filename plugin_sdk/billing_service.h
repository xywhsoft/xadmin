#ifndef XADMIN_BILLING_SERVICE_H
#define XADMIN_BILLING_SERVICE_H
#include <stdint.h>
#include <stddef.h>
#include <xsbase.h>
/* All methods run under the host application lock. No service vtable or lease
 * may be retained across unlocked network I/O. Persist the reservation ID and
 * acquire a fresh service lease to finalize after a reload. Amounts are CNY
 * micro-units; zero is valid, unknown usage is a separate pending state. */
#define XADMIN_BILLING_SERVICE "xadmin.billing"
#define XBILL_OK 0
#define XBILL_UNAVAILABLE 503
#define XBILL_INSUFFICIENT 402
#define XBILL_CONFLICT 409
#define XBILL_INVALID 400
#define XBILL_MAX_AMOUNT INT64_C(1000000000000000)
typedef struct XBillingAccount {
    uint32_t size;
    int64_t member_id, cash, cash_reserved, credit, credit_reserved;
} XBillingAccount;
typedef struct XBillingEntitlement {
    uint32_t size;
    int discount_bps, concurrency_limit;
    int64_t expires_at;
    char plan_id[65], model_ids[1025]; /* comma-separated exact model IDs; * */
} XBillingEntitlement;
typedef struct XBillingReservation {
    uint32_t size;
    int64_t member_id, amount, expires_at;
    const char* request_id;
    const char* service;
    const char* model;
} XBillingReservation;
typedef struct XBillingSettlement {
    uint32_t size;
    const char* request_id;
    int64_t amount;
    const char* usage_json; /* <=16 KiB metadata; no prompts or credentials */
    const char* outcome; /* settled, released, pending */
} XBillingSettlement;
typedef struct XBillingResult {
    uint32_t size;
    int64_t member_id, reserved, charged, expires_at;
    char state[17];
} XBillingResult;
typedef struct XBillingService {
    uint32_t size, version;
    int (*account)(int64_t member_id,XBillingAccount* out);
    int (*entitlement)(int64_t member_id,XBillingEntitlement* out);
    int (*reserve)(const XBillingReservation* request);
    int (*finalize)(const XBillingSettlement* request);
    int (*lookup)(const char* request_id,XBillingResult* out);
    int (*adjust_cash)(int64_t member_id,int64_t amount,const char* operation_id,
        const char* reason,const char* actor);
    int (*refund)(const char* request_id,const char* operation_id,const char* reason,const char* actor);
    xvalue* (*transactions)(int64_t member_id,int64_t offset,int limit); /* owned */
} XBillingService;
#endif
