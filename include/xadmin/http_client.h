#ifndef XADMIN_HTTP_CLIENT_H
#define XADMIN_HTTP_CLIENT_H
#include <stddef.h>

/* Bounded, owned POST request. Native transport always verifies HTTPS and
 * never follows redirects or retries a potentially billable operation. */
#define XA_HTTP_MAX_HEADERS 16u
typedef struct XAHttpRequest {
    char url[2049], content_type[81];
    char names[XA_HTTP_MAX_HEADERS][65], values[XA_HTTP_MAX_HEADERS][1025];
    size_t header_count;
    char* body;
} XAHttpRequest;
#endif
