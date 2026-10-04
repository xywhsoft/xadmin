/* Shared verified HTTPS transport for identity delivery and plugins. A total
 * deadline covers DNS, handshake, writes and reads; wire/body limits are separate.
 * Calls must run outside G_RequestLock with request-owned snapshots. */
#include "../../include/xadmin/http_client.h"
static bool XA_HttpHeaderName(const char* name)
{
    if (!name || !*name) return false;
    for (; *name; name++) {
        unsigned char c = *name;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-')) return false;
    }
    return true;
}
typedef struct XAHttpUrl { char host[256], authority[280], path[2049]; unsigned port; } XAHttpUrl;
static bool XA_HttpsUrlParse(const char* url, XAHttpUrl* out)
{
    memset(out, 0, sizeof(*out)); out->port = 443;
    if (!url || strncmp(url, "https://", 8) || strlen(url) > 2048 || strpbrk(url, "\r\n\t #\\")) return false;
    const unsigned char* cursor=(const unsigned char*)url;
    for(;*cursor;cursor++)if(*cursor<33 || *cursor==127)return false;
    const char* start = url + 8; size_t n = strcspn(start, "/?");
    if (!n || n >= sizeof(out->authority) || memchr(start, '@', n)) return false;
    memcpy(out->authority, start, n); out->authority[n] = 0;
    char* colon = strchr(out->authority, ':');size_t hostlen;
    bool ipv6=start[0]=='[';
    if(ipv6){
        char* close=strchr(out->authority,']');if(!close)return false;
        hostlen=(size_t)(close-out->authority)+1;colon=close[1]==':'?close+1:NULL;
        if(close[1] && !colon)return false;
    }else hostlen = colon ? (size_t)(colon - out->authority) : n;
    if (!hostlen || hostlen >= sizeof(out->host)) return false;
    size_t i;
    for (i = 0; !ipv6 && i < hostlen; ++i) {
        unsigned char c = start[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
    }
    if(ipv6){if(hostlen<3)return false;memcpy(out->host,start+1,hostlen-2);out->host[hostlen-2]=0;}
    else{memcpy(out->host, start, hostlen); out->host[hostlen] = 0;}
    if (colon) {
        unsigned port = 0; const char* s = colon + 1;
        if (!*s) return false;
        for (; *s; ++s) { if (*s < '0' || *s > '9' || port > 6553) return false; port = port * 10 + *s - '0'; }
        if (!port || port > 65535) return false; out->port = port;
    }
    if(ipv6){xnetaddr address;if(!xrtNetAddrParse(&address,out->host,(uint16)out->port)||address.Family!=XNET_FAMILY_IPV6)return false;}
    snprintf(out->path, sizeof(out->path), "%s%s", start[n] == '/' ? "" : "/", start + n);
    return true;
}
static bool XA_HttpsHttpHeader(XAHttpRequest* req, const char* name, const char* value)
{
    size_t i;
    if (!req || !XA_HttpHeaderName(name) || strlen(name) > 64 || !value || strlen(value) > 1024 ||
        strpbrk(value, "\r\n") || req->header_count >= XA_HTTP_MAX_HEADERS) return false;
    const char* reserved[] = {"Host", "Content-Length", "Content-Type", "Connection", "Transfer-Encoding", "Accept"};
    for (i = 0; i < 6; ++i) if (xrtStrCaseEqual(xrtStrView(name), xrtStrView(reserved[i]))) return false;
    for (i = 0; i < req->header_count; ++i)
        if (xrtStrCaseEqual(xrtStrView(name), xrtStrView(req->names[i]))) return false;
    i = req->header_count++; strcpy(req->names[i], name); strcpy(req->values[i], value); return true;
}
static bool XA_HttpsHttpBody(XAHttpRequest* req, const char* url, const char* type, const char* body)
{
    XAHttpUrl parsed;
    if (!req || !XA_HttpsUrlParse(url, &parsed) || !type || strlen(type) > 80 ||
        strpbrk(type, "\r\n") || !body || strlen(body) > 16384 || req->body) return false;
    req->body = xrtFormat("%s", body);
    if (!req->body) return false;
    strcpy(req->url, url); strcpy(req->content_type, type); return true;
}
static void XA_HttpsHttpUnit(XAHttpRequest* req)
{
    if (req->body) xrtSecureZero(req->body, strlen(req->body));
    xrtFree(req->body); xrtSecureZero(req, sizeof(*req));
}
static bool XA_HttpsWait(xfuture* future, xdeadline deadline)
{
    return future && xrtDeadlineRemaining(deadline) &&
        xrtFutureWaitFor(future, xrtDeadlineRemaining(deadline)) == XWAIT_OK &&
        xrtFutureState(future) == XFUTURE_RESOLVED;
}
static void XA_HttpsFutureDone(xfuture* future)
{
    if (!future) return;
    if (xrtFutureState(future) == XFUTURE_PENDING) xrtFutureCancel(future);
    xrtFutureDestroy(future);
}
static bool XA_HttpsHttp(void* engine, const char* ca_pem, const XAHttpRequest* req, unsigned timeout_ms, size_t max_body, int* status, char** response)
{
    XAHttpUrl url; xbuffer wire, incoming;
    xnetresolver* resolver = NULL; xtlsverifier* verifier = NULL; xtlsstream* stream = NULL;
    xfuture* future = NULL; bool ok = false, end = false; size_t i, headsize = 0;
    xdeadline deadline = xrtDeadlineAfter((uint64)timeout_ms * 1000);
    size_t max_wire = max_body + 32768;
    *status = 0; *response = NULL;
    if (!engine || !req || timeout_ms < 100 || timeout_ms > 60000 || !max_body || max_body > 1048576 || !req->body || !XA_HttpsUrlParse(req->url, &url)) return false;
    xrtBufferInit(&wire); xrtBufferInit(&incoming);
    xhttpfield fields[XA_HTTP_MAX_HEADERS + 5]; char length[24];
    snprintf(length, sizeof(length), "%u", (unsigned)strlen(req->body));
    fields[0] = (xhttpfield){XRT_STR_LITERAL("Host"), xrtStrView(url.authority)};
    fields[1] = (xhttpfield){XRT_STR_LITERAL("Content-Length"), xrtStrView(length)};
    fields[2] = (xhttpfield){XRT_STR_LITERAL("Content-Type"), xrtStrView(req->content_type)};
    fields[3] = (xhttpfield){XRT_STR_LITERAL("Connection"), XRT_STR_LITERAL("close")};
    fields[4] = (xhttpfield){XRT_STR_LITERAL("Accept"), XRT_STR_LITERAL("application/json")};
    for (i = 0; i < req->header_count; ++i)
        fields[5 + i] = (xhttpfield){xrtStrView(req->names[i]), xrtStrView(req->values[i])};
    if (!xrtHttp1RequestWrite(XRT_STR_LITERAL("POST"), xrtStrView(url.path), XHTTP_VERSION_1_1,
        fields, req->header_count + 5, NULL, 0, &headsize) || !xrtBufferResize(&wire, headsize) ||
        !xrtHttp1RequestWrite(XRT_STR_LITERAL("POST"), xrtStrView(url.path), XHTTP_VERSION_1_1,
        fields, req->header_count + 5, wire.Data, wire.Size, &headsize) ||
        !xrtBufferAppend(&wire, (xbytesview){(cbytes)req->body, strlen(req->body)})) goto done;
    xnetresolverconfig rc; xrtNetResolverConfigInit(&rc); resolver = xrtNetResolverCreate(&rc);
    xtlsverifierconfig vc; xrtTlsVerifierConfigInit(&vc); xx509store* store = ca_pem?xrtX509StoreCreate():xrtX509StoreSystem();
    if(store && ca_pem && !xrtX509StoreAddPem(store,ca_pem,strlen(ca_pem),NULL)){xrtX509StoreFree(store);store=NULL;}
    if (store) { vc.Store = store; verifier = xrtTlsVerifierCreate(&vc); xrtX509StoreFree(store); }
    if (!resolver || !verifier) goto done;
    xtlsclientconfig tls; xtlsdialconfig dial;
    xrtTlsClientConfigInit(&tls); xrtTlsDialConfigInit(&dial);
    tls.Verifier = verifier; tls.VerifyName = xrtStrView(url.host);
    if(url.host[0]=='[')tls.VerifyName=(xstrview){url.host+1,strlen(url.host)-2};
    xnetaddr address;
    if (!xrtNetAddrParse(&address, url.host, (uint16)url.port)) tls.ServerName = tls.VerifyName;
    dial.ServerNameFromHost = false; dial.Timeout = xrtDeadlineRemaining(deadline);
    if (!dial.Timeout) goto done;
    future = xrtTlsDialAsync(engine, resolver, url.host, (uint16)url.port, &tls, &dial, NULL, NULL);
    if (!XA_HttpsWait(future, deadline)) goto done;
    stream = xrtTlsStreamRef(xrtFutureValue(future)); XA_HttpsFutureDone(future); future = NULL;
    if (!stream) goto done;
    for (i = 0; i < wire.Size;) {
        size_t n = wire.Size - i; if (n > 16384) n = 16384;
        future = xrtTlsStreamSendAsync(stream, (cbytes)wire.Data + i, n);
        if (!XA_HttpsWait(future, deadline)) goto done;
        XA_HttpsFutureDone(future); future = NULL; i += n;
    }
    for (;;) {
        xhttpfield headers[64], trailers[16]; xhttp1message message;
        xhttp1limits hl; xhttp1bodylimits bl; xhttp1errorinfo error;
        xrtHttp1LimitsInit(&hl); hl.MaxFields = 64;
        xrtHttp1BodyLimitsInit(&bl); bl.MaxBody = max_body; bl.MaxTrailers = 16;
        xrtHttp1MessageInit(&message, headers, 64, trailers, 16);
        xhttp1status parsed = xrtHttp1ResponseMessageParse(xrtBufferView(&incoming), end,
            XRT_STR_LITERAL("POST"), &message, &hl, &bl, &error);
        if (parsed == XHTTP1_READY) {
            size_t n = 0;
            if (message.Head.Status < 200) goto done; /* no informational response is treated as acceptance */
            if (!xrtHttp1MessageBodyCopy(&message, NULL, 0, &n) || n > max_body) goto done;
            char* bytes = xrtMalloc(n + 1);
            if (!bytes) goto done;
            if (!xrtHttp1MessageBodyCopy(&message, bytes, n, &n) || memchr(bytes, 0, n)) { xrtFree(bytes); goto done; }
            bytes[n] = 0; *response = bytes; *status = message.Head.Status; ok = true; break;
        }
        if (parsed != XHTTP1_MORE || end || incoming.Size >= max_wire) goto done;
        future = xrtTlsStreamRecvAsync(stream, 8192);
        if (!future || !xrtDeadlineRemaining(deadline) ||
            xrtFutureWaitFor(future, xrtDeadlineRemaining(deadline)) != XWAIT_OK) goto done;
        if (xrtFutureState(future) == XFUTURE_CLOSED) {
            XA_HttpsFutureDone(future); future = xrtTlsStreamWaitAsync(stream, XTLS_STREAM_WAIT_END);
            if (!XA_HttpsWait(future, deadline)) goto done;
            end = true;
        } else if (xrtFutureState(future) == XFUTURE_RESOLVED) {
            xnetbytes* bytes = xrtFutureValue(future);
            xbytesview view = bytes ? xrtNetBytesView(bytes) : (xbytesview){0};
            if (!view.Size || view.Size > 8192 || incoming.Size + view.Size > max_wire || !xrtBufferAppend(&incoming, view)) goto done;
        } else goto done;
        XA_HttpsFutureDone(future); future = NULL;
    }
done:
    XA_HttpsFutureDone(future);
    if (stream) { xrtTlsStreamAbort(stream); xrtTlsStreamDestroy(stream); }
    if (resolver) xrtNetResolverDestroy(resolver);
    if (verifier) xrtTlsVerifierRelease(verifier);
    if (wire.Data) xrtSecureZero(wire.Data, wire.Size);
    if (incoming.Data) xrtSecureZero(incoming.Data, incoming.Size);
    xrtBufferUnit(&wire); xrtBufferUnit(&incoming); return ok;
}
