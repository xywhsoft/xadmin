/* 保留 v1 业务处理函数的参数形式，集中接入新版 XS_HttpReq。
 * 一个请求一个栈上视图；不使用线程局部/全局的“当前请求”。
 * path/query/body 等字符串只在处理函数返回之前有效。
 */
typedef XS_ServerInfo* XS_ServerObject;
typedef const XS_HostInfo* XS_HostObject;
typedef struct XAdminRequest {
	XS_HttpReq* raw;
	char* path;
	char* query;
	char* body;
	size_t body_size;
	char remote[128];
	char method[32];
	/* 动态参数是借用视图，生命周期仅限当前处理函数。 */
	xstrview param_name[8], param_value[8];
	size_t param_count;
	bool replied;
	bool deferred; /* owned async route; protocol returns XS_TAKEOVER */
	bool streaming, stream_failed;
	xdeadline stream_deadline;
	char* header_copy[64]; /* legacy C-string SDK views, owned for this request */
} XAdminRequest;
typedef XAdminRequest* XS_RequestObject;
typedef XAdminRequest* XS_ResponseObject;

/* Deferred callbacks own a connection on a separate thread. Wait for accepted
 * writes to drain before their owner closes it; a reactor-only send is unsafe. */
static int XAdmin_ReplyBinary(XS_RequestObject req, uint16 status,
	const xhttpfield* fields, size_t count, const void* body, size_t size,
	unsigned timeout_ms);
static int XAdmin_StreamFinish(XS_RequestObject req, bool success);

#define XHTTP_METHOD_GET XHTTP_METHOD_GET
#define XHTTP_METHOD_POST XHTTP_METHOD_POST
#define XHTTP_METHOD_PUT XHTTP_METHOD_PUT
#define XHTTP_METHOD_PATCH XHTTP_METHOD_PATCH
#define XHTTP_METHOD_DELETE XHTTP_METHOD_DELETE
#define XHTTP_METHOD_HEAD XHTTP_METHOD_HEAD
#define XHTTP_METHOD_OPTIONS XHTTP_METHOD_OPTIONS
#define xsReqPath(req) ((req)->path)
#define xsReqQuery(req) ((req)->query)
#define xsReqBody(req) ((req)->body)
#define xsReqBodyLen(req) ((req)->body_size)
#define xsReqRemote(req) ((req)->remote)
#define xsReqMethod(req) ((req)->method)
#define xsReqMethodID(req) ((req)->raw->head->MethodCode)

static int XA_ReadParam(const char* text, char delimiter, const char* name, char* out, size_t capacity, bool decode)
{
	const char* item = text;
	if (!out || !capacity) return -1;
	out[0] = 0;
	while (item && *item) {
		const char* end = strchr(item, delimiter);
		const char* equal;
		if (!end) end = item + strlen(item);
		while (item < end && (*item == ' ' || *item == '\t')) item++;
		equal = memchr(item, '=', end - item);
		if (equal && (size_t)(equal - item) == strlen(name) && !memcmp(item, name, equal - item)) {
			size_t size = end - equal - 1;
			char* value = xrtStrDupN(equal + 1, size);
			if (!value) return -1;
			if (decode) {
				char* cursor;
				for (cursor = value; *cursor; cursor++) if (*cursor == '+') *cursor = ' ';
				if (!xrtPercentDecode(xrtStrViewN(value, size), value, size, &size) || memchr(value, 0, size)) {
					xrtFree(value); return -1;
				}
			}
			if (size >= capacity) size = capacity - 1;
			memcpy(out, value, size); out[size] = 0;
			xrtFree(value);
			return (int)size;
		}
		item = *end ? end + 1 : end;
	}
	return -1;
}
static int xsReqQueryValue(XS_RequestObject req, const char* name, char* out, size_t capacity)
{
	return XA_ReadParam(req->query, '&', name, out, capacity, true);
}
static bool xsReqRouteValue(XS_RequestObject req, const char* name, xstrview* out)
{
	size_t i;
	if (!out) return false;
	*out = (xstrview){0};
	for (i = 0; i < req->param_count; i++) if (xrtStrEqual(req->param_name[i], xrtStrView(name))) {
		*out = req->param_value[i]; return true;
	}
	return false;
}
static int xsReqCookieValue(XS_RequestObject req, const char* name, char* out, size_t capacity)
{
	size_t i;
	if (capacity) out[0] = 0;
	for (i = 0; i < req->raw->head->FieldCount; i++) {
		const xhttpfield* field = &req->raw->head->Fields[i];
		if (xrtStrCaseEqual(field->Name, XRT_STR_LITERAL("Cookie"))) {
			char* text = xrtStrDupView(field->Value);
			int found = XA_ReadParam(text, ';', name, out, capacity, false);
			xrtFree(text);
			if (found >= 0) return found;
		}
	}
	return -1;
}

/* v1 的原始响应头只在这里拆成新版 xhttpfield，不修改业务 JSON 格式。
 * Content-Length 由新发送层统一生成，避免双重长度与响应拆分。
 */
static int xsHttpReplyAuto(XS_ResponseObject req, int code, const char* headers, const void* body, size_t size)
{
	xhttpfield fields[13]; size_t count = 0;
	char* copy = xrtStrDup(headers ? headers : "");
	char* line = copy; const char* content_type = NULL;
	bool ok;
	if (!req || req->replied || !copy) { xrtFree(copy); return -1; }
	if (!size && body) size = strlen((const char*)body);
	while (line && *line) {
		char* end = strstr(line, "\r\n");
		char* colon;
		if (end) *end = 0;
		colon = strchr(line, ':');
		if (colon) {
			char* value = colon + 1; *colon = 0;
			while (*value == ' ' || *value == '\t') value++;
			if (xrtStrCaseEqual(xrtStrView(line), XRT_STR_LITERAL("Content-Type"))) content_type = value;
			else if (!xrtStrCaseEqual(xrtStrView(line), XRT_STR_LITERAL("Content-Length")) &&
				!(req->deferred && (xrtStrCaseEqual(xrtStrView(line), XRT_STR_LITERAL("Connection")) ||
				 xrtStrCaseEqual(xrtStrView(line), XRT_STR_LITERAL("Transfer-Encoding"))))) {
				if (count == 10) { xrtFree(copy); return -1; }
				fields[count++] = (xhttpfield){xrtStrView(line), xrtStrView(value)};
			}
		}
		line = end ? end + 2 : NULL;
	}
	if (req->deferred) {
		if (content_type) fields[count++] = (xhttpfield){XRT_STR_LITERAL("Content-Type"),xrtStrView(content_type)};
		xstrview origin = XA_CORSOrigin(req->raw);
		if (origin.Size) {
			fields[count++] = (xhttpfield){XRT_STR_LITERAL("Access-Control-Allow-Origin"),origin};
			fields[count++] = (xhttpfield){XRT_STR_LITERAL("Vary"),XRT_STR_LITERAL("Origin")};
		}
		/* The bounded sender releases G_RequestLock while waiting. It owns
		 * framing and marks replied before writing, so a partial failure can
		 * never fall through to a second response on the same connection. */
		ok = XAdmin_ReplyBinary(req,(uint16)code,fields,count,body,size,30000) == 0;
	} else {
		ok = ReplyRawHeaders(req->raw, (uint16)code, content_type, body, size, fields, count);
		req->replied = true;
	}
	xrtFree(copy);
	return ok ? 0 : -1;
}
static int xsHttpReplyFormat(XS_ResponseObject req, int code, const char* headers, const char* format, ...)
{
	va_list args; int count; char* text; int result;
	va_start(args, format); count = vsnprintf(NULL, 0, format, args); va_end(args);
	if (count < 0) return -1;
	text = xrtMalloc((size_t)count + 1);
	if (!text) return -1;
	va_start(args, format); vsnprintf(text, (size_t)count + 1, format, args); va_end(args);
	result = xsHttpReplyAuto(req, code, headers, text, count);
	xrtFree(text);
	return result;
}

/* 管理端沿用 HTTP 200 + result/message 契约；失败时不返回旧记录 ID。 */
static bool ReplyIfWriteFailed(XS_ResponseObject resp, bool written)
{
	if (written) return false;
	xsHttpReplyAuto(resp, 200, "Content-Type: application/json; charset=utf-8\r\n",
		"{\"result\":false,\"message\":\"数据库写入失败或记录不存在！\"}", 0);
	return true;
}

const char* XAdmin_PluginReqHeader(XS_RequestObject objReq, const char* sName)
{
	/* HTTP names are case-insensitive, and xrt fields are length-bearing views,
	 * not C strings. Keep a request-owned, NUL-terminated copy for legacy ABI. */
	size_t i, index = 64; const xhttpfield* found = NULL;
	XAdminRequest* req = (XAdminRequest*)objReq;
	if (!req || !req->raw || !req->raw->head || !sName || !sName[0]) return NULL;
	for (i = 0; i < req->raw->head->FieldCount; i++) {
		const xhttpfield* f = &req->raw->head->Fields[i];
		if (xrtStrCaseEqual(f->Name,xrtStrView(sName))) {
			if (found) return NULL;
			found = f; index = i;
		}
	}
	if (!found || index >= 64 || found->Value.Size > 32768 ||
		memchr(found->Value.Data,0,found->Value.Size)) return NULL;
	if (!req->header_copy[index]) req->header_copy[index] = xrtStrDupN(found->Value.Data,found->Value.Size);
	return req->header_copy[index];
}
/* New bounded helper avoids borrowing a cache pointer. >=0 bytes copied,
 * -1 absent, -2 invalid/duplicate/insufficient capacity; output cleared. */
static int XAdmin_ReqHeaderCopy(XS_RequestObject req,const char* name,char* out,size_t capacity)
{
	const xhttpfield* found = NULL; size_t i;
	if (out && capacity) out[0] = 0;
	if (!req || !req->raw || !req->raw->head || !name || !name[0] || !out || !capacity) return -2;
	for (i=0;i<req->raw->head->FieldCount;i++) if (xrtStrCaseEqual(req->raw->head->Fields[i].Name,xrtStrView(name))) {
		if (found) return -2; found = &req->raw->head->Fields[i];
	}
	if (!found) return -1;
	if (found->Value.Size >= capacity || found->Value.Size > 32768 || memchr(found->Value.Data,0,found->Value.Size)) return -2;
	memcpy(out,found->Value.Data,found->Value.Size);out[found->Value.Size]=0;return (int)found->Value.Size;
}
static void XAdmin_RequestHeadersRelease(XAdminRequest* req)
{
	size_t i;for(i=0;i<64;i++)if(req->header_copy[i]) {
		xrtSecureZero(req->header_copy[i],strlen(req->header_copy[i]));xrtFree(req->header_copy[i]);req->header_copy[i]=NULL;
	}
}

const char* XAdmin_ReqBody(XS_RequestObject objReq)
{
	return objReq ? ((XAdminRequest*)objReq)->body : NULL;
}

size_t XAdmin_ReqBodyLen(XS_RequestObject objReq)
{
	return objReq ? ((XAdminRequest*)objReq)->body_size : 0;
}

/* LoadPage 由应用模块直接提供（单翻译单元），无需包装。 */
