/* Async routes leave the network reactor immediately. XS_TAKEOVER keeps its
 * connection/script ownership; server/stream refs and copied metadata protect
 * a job even if the peer disconnects. Unit joins threads before code teardown.
 * No plugin callback runs outside G_RequestLock except its owned HTTP I/O. */
#define PLUGIN_ASYNC_MAX 16
static xthread* G_PluginThreads[PLUGIN_ASYNC_MAX];
typedef struct PluginAsyncJob {
    PluginInstance* plugin;
    XAdminAsyncRouteProc proc;
    XAdminRequest req;
    XS_HttpReq raw;
    xhttp1head head;
    xhttpfield fields[64];
    xvalue* session;
} PluginAsyncJob;

static bool PluginAsync_CopyView(xstrview source, xstrview* out)
{
    *out = (xstrview){0};
    if (!source.Size) return true;
    if (!source.Data || source.Size > 32768) return false;
    char* copy = xrtStrDupN(source.Data,source.Size);
    if (!copy) return false;
    *out = xrtStrViewN(copy,source.Size); return true;
}
static void PluginAsync_Free(PluginAsyncJob* job)
{
    size_t i;
    XAdmin_RequestHeadersRelease(&job->req);
    for (i = 0; i < job->head.FieldCount; i++) {
        if (job->fields[i].Value.Data) xrtSecureZero((void*)job->fields[i].Value.Data,job->fields[i].Value.Size);
        xrtFree((void*)job->fields[i].Name.Data); xrtFree((void*)job->fields[i].Value.Data);
    }
    xrtFree((void*)job->head.Method.Data); xrtFree((void*)job->head.Target.Data);
    xrtFree(job->req.path); xrtFree(job->req.query);
    for (i = 0; i < job->req.param_count; ++i) {
        xrtFree((void*)job->req.param_name[i].Data);
        xrtFree((void*)job->req.param_value[i].Data);
    }
    if (job->req.body) xrtSecureZero(job->req.body,job->req.body_size);
    xrtFree(job->req.body); xrtValueRelease(job->session);
    if (job->raw.tcp) xrtNetStreamDestroy(job->raw.tcp);
    if (job->raw.tls) xrtTlsStreamDestroy(job->raw.tls);
    if (job->raw.server) xsServerRelease(job->raw.server);
    xrtFree(job);
}
static int32 PluginAsync_Run(void* data)
{
    PluginAsyncJob* job = data;
    xrtMutexLock(G_RequestLock);
    job->proc(job->raw.server,job->raw.host,&job->req,&job->req,job->session);
    if (job->req.streaming) XAdmin_StreamFinish(&job->req,false);
    if (!job->req.replied) xsHttpReplyAuto(&job->req,500,HTTP_CT_JSON,"{\"code\":500,\"message\":\"Async route produced no response\"}",0);
    job->plugin->activeIo--;
    xrtMutexUnlock(G_RequestLock);
    /* Close flushes accepted writes; do not abort a successful response. */
    if (job->raw.tls) xrtTlsStreamClose(job->raw.tls);
    else if (job->raw.tcp) xrtNetStreamClose(job->raw.tcp);
    PluginAsync_Free(job); return 0;
}
static int XAdmin_DeferRoute(XAdminPluginHandle handle, XS_RequestObject req,
    xvalue* session, XAdminAsyncRouteProc proc)
{
    PluginInstance* plugin = NULL; size_t i; int slot = -1;
    for (i = 0; i < G_PluginCount; i++) if (handle == &G_Plugins[i]) plugin = &G_Plugins[i];
    if (!plugin || !plugin->started || G_PluginRegIdx >= 0 || !req || !req->raw ||
        !req->raw->head || req->raw->head->FieldCount > 64 || req->body_size > 1048576 ||
        req->param_count > 8 || req->deferred || req->replied || !proc) return -1;
    for (i = 0; i < PLUGIN_ASYNC_MAX; i++) {
        if (G_PluginThreads[i] && xrtThreadState(G_PluginThreads[i]) == XTHREAD_FINISHED) {
            xrtThreadWait(G_PluginThreads[i]); xrtThreadDestroy(G_PluginThreads[i]); G_PluginThreads[i] = NULL;
        }
        if (!G_PluginThreads[i]) slot = (int)i;
    }
    if (slot < 0) return -1;
    PluginAsyncJob* job = xrtCalloc(1,sizeof(*job)); if (!job) return -1;
    job->plugin = plugin; job->proc = proc;
    job->req.deferred = true; /* owned callback, eligible for bounded streaming */
    job->head = *req->raw->head; job->head.Fields = job->fields; job->head.FieldCapacity = 64;
    job->head.Flags |= XHTTP1_CONNECTION_CLOSE;
    job->head.Method = job->head.Target = job->head.Reason = (xstrview){0};
    job->req.body_size = req->body_size;
    job->req.path = xrtStrDup(req->path); job->req.query = xrtStrDup(req->query);
    job->req.body = xrtStrDupN(req->body ? req->body : "",req->body_size);
    memcpy(job->req.remote,req->remote,sizeof(job->req.remote));
    memcpy(job->req.method,req->method,sizeof(job->req.method));
    job->raw.server = xsServerRetain(req->raw->server); job->raw.host = req->raw->host;
    job->raw.tcp = req->raw->tcp ? xrtNetStreamRef(req->raw->tcp) : NULL;
    job->raw.tls = req->raw->tls ? xrtTlsStreamRef(req->raw->tls) : NULL;
    job->raw.head = &job->head; job->req.raw = &job->raw;
    job->session = session ? xrtValueDeepClone(session) : NULL;
    bool ok = job->req.path && job->req.query && job->req.body && job->raw.server &&
        (job->raw.tcp || job->raw.tls) && (!session || job->session) &&
        PluginAsync_CopyView(req->raw->head->Method,&job->head.Method) &&
        PluginAsync_CopyView(req->raw->head->Target,&job->head.Target);
    size_t header_bytes = 0;
    for (i = 0; ok && i < job->head.FieldCount; i++) {
        header_bytes += req->raw->head->Fields[i].Name.Size + req->raw->head->Fields[i].Value.Size;
        if (header_bytes > 65536) { ok = false; break; }
        ok = PluginAsync_CopyView(req->raw->head->Fields[i].Name,&job->fields[i].Name) &&
            PluginAsync_CopyView(req->raw->head->Fields[i].Value,&job->fields[i].Value);
    }
    for (i = 0; ok && i < req->param_count; ++i) {
        job->req.param_count = i+1;
        ok = PluginAsync_CopyView(req->param_name[i],&job->req.param_name[i]) &&
            PluginAsync_CopyView(req->param_value[i],&job->req.param_value[i]);
    }
    if (!ok) { PluginAsync_Free(job); return -1; }
    plugin->activeIo++;
    G_PluginThreads[slot] = xrtThreadCreate(PluginAsync_Run,job,0);
    if (!G_PluginThreads[slot]) { plugin->activeIo--; PluginAsync_Free(job); return -1; }
    req->deferred = true; return 0;
}
static void PluginAsync_Unit(void)
{
    size_t i;
    /* ServiceUnit runs on xs' reaper, without G_RequestLock; joining must leave
     * the lock available to callbacks that are completing their HTTPS work. */
    for (i = 0; i < PLUGIN_ASYNC_MAX; i++) if (G_PluginThreads[i]) {
        xrtThreadWait(G_PluginThreads[i]); xrtThreadDestroy(G_PluginThreads[i]); G_PluginThreads[i] = NULL;
    }
}

static bool PluginAsync_Future(xfuture* future, XAdminDeadline deadline)
{
    bool ok = future && xrtFutureWaitFor(future,XAdmin_DeadlineRemainingMs(deadline)) == XWAIT_OK &&
        xrtFutureState(future) == XFUTURE_RESOLVED;
    if (!ok && future) xrtFutureCancel(future);
    xrtFutureDestroy(future); return ok;
}
/* Only a deferred route owns a connection away from its network worker.
 * The plugin remains pinned by PluginAsync_Run until this call returns.
 * Body and headers are borrowed; the caller keeps them alive during the call. */
static int XAdmin_ReplyBinary(XS_RequestObject req, uint16 status,
    const xhttpfield* fields, size_t count, const void* body, size_t size,
    unsigned timeout_ms)
{
    /* Ten convenience headers plus Content-Type and two CORS headers fit,
     * followed by the two framing headers generated here. */
    char head[4096], length[32]; xhttpfield all[15]; size_t head_size, i;
    if (!req || !req->deferred || req->replied || !req->raw || !req->raw->head ||
        count > 13 || (count && !fields) || (size && !body) || size > 32u*1024u*1024u ||
        timeout_ms < 100 || timeout_ms > 120000) return -1;
    for (i = 0; i < count; ++i) {
        if (xrtStrCaseEqual(fields[i].Name,XRT_STR_LITERAL("Content-Length")) ||
            xrtStrCaseEqual(fields[i].Name,XRT_STR_LITERAL("Transfer-Encoding")) ||
            xrtStrCaseEqual(fields[i].Name,XRT_STR_LITERAL("Connection"))) return -1;
        all[i] = fields[i];
    }
    snprintf(length,sizeof(length),"%llu",(unsigned long long)size);
    all[count++] = (xhttpfield){XRT_STR_LITERAL("Content-Length"),xrtStrView(length)};
    all[count++] = (xhttpfield){XRT_STR_LITERAL("Connection"),XRT_STR_LITERAL("close")};
    if (!xrtHttp1ResponseWrite(XHTTP_VERSION_1_1,status,xrtHttpStatusText(status),
            all,count,head,sizeof(head),&head_size)) return -1;
    XAdminDeadline deadline = XAdmin_DeadlineAfterMs(timeout_ms);
    req->replied = true; bool ok = true; size_t offset = 0;
    const char* bytes = head; size_t total = head_size; int pass;
    xrtMutexUnlock(G_RequestLock);
    for (pass = 0; ok && pass < 2; ++pass) {
        while (ok && offset < total) {
            size_t chunk = total-offset; if (chunk > 16384) chunk = 16384;
            if (XAdmin_DeadlineExpired(deadline)) { ok = false; break; }
            if (req->raw->tls) {
                ok = PluginAsync_Future(xrtTlsStreamSendAsync(req->raw->tls,bytes+offset,chunk),deadline) &&
                    PluginAsync_Future(xrtTlsStreamWaitAsync(req->raw->tls,XTLS_STREAM_WAIT_DRAIN),deadline);
            } else {
                size_t limit = xrtNetStreamWriteLimit(req->raw->tcp);
                if (!limit) { ok = false; break; }
                if (chunk > limit) chunk = limit;
                xnetresult sent = xrtNetStreamSend(req->raw->tcp,bytes+offset,chunk);
                ok = (sent == XNET_RESULT_OK || sent == XNET_RESULT_AGAIN) &&
                    xrtNetStreamWait(req->raw->tcp,XNET_STREAM_WAIT_DRAIN,XAdmin_DeadlineRemainingMs(deadline),NULL);
                if (ok && sent == XNET_RESULT_AGAIN) continue;
            }
            offset += chunk;
        }
        bytes = body; offset = 0;
        total = req->raw->head->MethodCode == XHTTP_METHOD_HEAD ? 0 : size;
    }
    if (!ok) {
        if (req->raw->tls) xrtTlsStreamAbort(req->raw->tls);
        else xrtNetStreamAbort(req->raw->tcp);
    }
    xrtMutexLock(G_RequestLock); return ok ? 0 : -1;
}
