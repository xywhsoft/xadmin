#ifndef XADMIN_IDENTITY_DELIVERY_H
#define XADMIN_IDENTITY_DELIVERY_H
#include <stdbool.h>
/* A synchronous, bounded adapter. Borrowed strings/engine are valid only for
 * this invocation. Do not retain pointers or launch detached work. Configure
 * before serving requests; callbacks belong to the application's xs generation.
 * Codes must never enter production logs, persistent queues or HTTP responses. */
typedef enum XAIdentityDeliveryResult {
    XA_DELIVERY_UNKNOWN=-1, XA_DELIVERY_FAILED=0, XA_DELIVERY_SENT=1
} XAIdentityDeliveryResult;
typedef struct XAIdentityMessage {
    const char* challenge_id;
    const char* channel;
    const char* target;
    const char* purpose;
    const char* code;
    unsigned expires_in;
    void* borrowed_engine;
    bool security_notice; /* code is empty; notify a previously verified target */
} XAIdentityMessage;
typedef XAIdentityDeliveryResult (*XAIdentityDeliveryProc)(const XAIdentityMessage*,void* context);
static bool XA_SetIdentityDelivery(XAIdentityDeliveryProc proc,void* context);
#endif
