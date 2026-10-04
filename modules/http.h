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
} XAdminRequest;
typedef XAdminRequest* XS_RequestObject;
typedef XAdminRequest* XS_ResponseObject;

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
	xhttpfield fields[10]; size_t count = 0;
	char* copy = xrtStrDup(headers ? headers : "");
	char* line = copy; const char* content_type = NULL;
	bool ok;
	if (req->replied || !copy) { xrtFree(copy); return -1; }
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
			else if (!xrtStrCaseEqual(xrtStrView(line), XRT_STR_LITERAL("Content-Length"))) {
				if (count == 10) { xrtFree(copy); return -1; }
				fields[count++] = (xhttpfield){xrtStrView(line), xrtStrView(value)};
			}
		}
		line = end ? end + 2 : NULL;
	}
	ok = ReplyRawHeaders(req->raw, (uint16)code, content_type, body, size, fields, count);
	req->replied = true;
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
	/* 从 raw HTTP 头中按名取值（借用视图，仅当前请求内有效）。 */
	size_t i;
	XAdminRequest* req = (XAdminRequest*)objReq;
	if (!req || !req->raw || !req->raw->head || !sName) return NULL;
	for (i = 0; i < req->raw->head->FieldCount; i++) {
		const xhttpfield* f = &req->raw->head->Fields[i];
		if (f->Name.Size == strlen(sName) && !memcmp(f->Name.Data, sName, f->Name.Size))
			return (const char*)f->Value.Data;
	}
	return NULL;
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
