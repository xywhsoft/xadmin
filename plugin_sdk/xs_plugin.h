#ifndef XS_PLUGIN_H
#define XS_PLUGIN_H

/* 插件 ABI v4。宿主经 plugin_sdk/ 包含目录提供给插件编译单元；
 * 值/时间/文件等运行时 API 直接使用 xsbase.h 的原生声明与
 * xsCreateTCC 预置符号，SDK 只承载插件契约层。 */

#include "xsbase.h"
#include <stdint.h>
#include "../include/xadmin/time.h"

/* 应用侧（宿主 TU）已从 modules/http.h 取得 XS_* 对象类型；
 * 插件侧无该头——这里提供不透明指针定义，两者 ABI 兼容（仅指针传递）。 */
#ifndef XS_PLUGIN_HOST_SIDE
typedef void* XS_ServerObject;
typedef void* XS_HostObject;
typedef void* XS_RequestObject;
typedef void* XS_ResponseObject;
#endif

#if defined(_WIN32) || defined(_WIN64)
#define XADMIN_EXPORT __declspec(dllexport)
#else
#define XADMIN_EXPORT __attribute__((visibility("default")))
#endif

#define XADMIN_ABI_VERSION 4

#define XADMIN_GLOBAL_MAIN_DB 1
#define XADMIN_GLOBAL_OPTION_TABLE 2
#define XADMIN_GLOBAL_PLUGIN_XID 3
#define XADMIN_GLOBAL_PLUGIN_ROOT_PATH 4
#define XADMIN_GLOBAL_PLUGIN_DATA_PATH 5
#define XADMIN_GLOBAL_PLUGIN_PRIVATE_DB_PATH 6
#define XADMIN_GLOBAL_HOST_CONTEXT 7 /* XAdminHostContext*：宿主聚合上下文（v1 契约） */

#define XADMIN_AUTH_SCOPE_ADMIN 1
#define XADMIN_AUTH_SCOPE_MEMBER 2

#define XADMIN_HOOK_CONTINUE 0
#define XADMIN_HOOK_STOP 1
#define XADMIN_HOOK_ERROR -1

typedef void* XAdminPluginHandle;
typedef void* XAdminServiceLease;
/* Host-managed WebSocket handle. Zero is invalid; handles are never recycled. */
typedef uintptr_t XAdminChannel;
typedef struct XAdminChannelConfig {
	uint32_t size;
	const char* protocol;
	size_t message_limit; /* 1..256 KiB; complete text/binary message */
	size_t queue_limit; /* 1..4 MiB; queued outbound payload bytes */
	bool allow_cross_origin; /* only with plugin-verified single-use handshake proof */
	void* data;
	void (*on_open)(XAdminChannel channel, void* data);
	void (*on_message)(XAdminChannel channel, bool binary, const void* bytes,
		size_t size, void* data);
	void (*on_close)(XAdminChannel channel, uint16_t code, void* data);
} XAdminChannelConfig;
typedef uintptr_t XAdminRouteToken;
typedef uintptr_t XAdminMenuToken;
typedef uintptr_t XAdminAuthGroupToken;
typedef uintptr_t XAdminAuthToken;
typedef uintptr_t XAdminUriAuthToken;
typedef uintptr_t XAdminEventToken;
typedef uintptr_t XAdminHookToken;

typedef struct {
	int status_code;
	const char* message;
} XAdminHealthReport;

/* 宿主聚合上下文（v1 字段序 ABI）：一个结构体指针提供全部宿主路径与插件
 * 身份信息，取代零散的标量全局注入（旧槽位 1-6 仍并行可用）。
 * size/abi_version 前向兼容：消费方先校验再取尾部字段。 */
struct sqlite3; /* 前置声明：不强依赖 <sqlite3.h>（指针兼容其 typedef） */
typedef struct XAdminHostContext {
	uint32_t size;
	uint32_t abi_version;
	const char* exe_path;
	const char* app_path;
	const char* web_path;
	const char* db_path;
	const char* log_path;
	const char* temp_path;
	const char* page_path;
	const char* site_page_path;
	const char* tool_path;
	const char* option_path;
	const char* install_path;
	const char* template_path;
	const char* attachment_path;
	const char* plugin_xid;
	const char* plugin_root_path;
	const char* plugin_data_path;
	const char* plugin_private_db_path;
	struct sqlite3* main_db;
	xvalue* option_table;
} XAdminHostContext;

typedef struct {
	const char* path;
	void* proc;
	bool need_auth;
	bool admin_only;
	int auth_id;
	int auth_level;
} XAdminRouteDecl;

/* 动态（pattern 参数）路由：pattern 为 v3 pattern 方言（/api/x/{name} 形式，
 * 命名捕获经 XAdmin_RouteParam 读取）。path 仅作登记键。 */
typedef struct {
	const char* path;
	const char* pattern;
	void* proc;
	int priority;
	int method;
	bool need_auth;
	bool admin_only;
	int auth_id;
	int auth_level;
	const char* description;
	int sort;
	bool need_log;
	bool keep_active;
} XAdminDynamicRouteDecl;

typedef struct {
	const char* key;
	int parent_id;
	const char* title;
	const char* icon;
	int type;
	const char* open_type;
	const char* href;
	int sort;
	bool visible;
	const char* remark;
} XAdminMenuDecl;

typedef struct {
	int scope;
	const char* key;
	const char* name;
	const char* description;
	int sort;
} XAdminAuthGroupDecl;

typedef struct {
	int scope;
	const char* key;
	int group_id;
	const char* name;
	const char* description;
	int sort;
} XAdminAuthDecl;

typedef struct {
	int scope;
	const char* key;
	int auth_id;
	const char* uri;
	const char* description;
	int sort;
	bool need_auth;
	bool need_log;
	bool keep_active;
} XAdminUriAuthDecl;

typedef struct {
	const char* service_name;
	int major_version;
	int minor_version;
	const char* provider_xid;
	int lifecycle_scope;
	size_t vtable_size;
	const char* capabilities_required;
} XAdminServiceDecl;

typedef void (*XAdminEventProc)(const char* event_name, void* payload, size_t payload_size);
typedef int (*XAdminHookProc)(const char* hook_name, void* payload, size_t payload_size);

typedef struct {
	const char* event_name;
	XAdminEventProc proc;
} XAdminEventDecl;

typedef struct {
	const char* hook_name;
	int sort;
	XAdminHookProc proc;
} XAdminHookDecl;

typedef struct {
	const char* relative_path;
	const void* data;
	size_t size;
} XAdminGeneratedFile;

typedef struct {
	const char* xid;
	const char* title;
	const char* version;
	const char* entry;
	int auto_enable;
	size_t file_count;
	const XAdminGeneratedFile* files;
} XAdminGeneratedPluginSpec;

typedef struct XAdminPluginDescriptor {
	uint32 abi_version;
	size_t size;
	const char* xid;
	const char* version;
	const char* title;
	int (*OnLoad)(XAdminPluginHandle* out_handle);
	int (*OnInstall)(XAdminPluginHandle handle);
	int (*OnStart)(XAdminPluginHandle handle);
	int (*OnConfigChanged)(XAdminPluginHandle handle, xvalue* new_cfg);
	int (*OnHealthCheck)(XAdminPluginHandle handle, XAdminHealthReport* out_report);
	void (*OnStop)(XAdminPluginHandle handle);
	void (*OnUnload)(XAdminPluginHandle handle);
} XAdminPluginDescriptor;

/* 应用级请求/回复原语（由宿主注入；其余原生 API 见 xsbase.h）。 */
int HttpReplyFormat(XS_ResponseObject objResp, int iCode, const char* sHead, const char* sFormat, ...);
void LoadPage(XS_ResponseObject objResp, int iCode, const char* sHead, const char* sPage);
#ifndef XS_PLUGIN_HOST_SIDE
void xsHttpReplyAuto(XS_ResponseObject objResp, int iCode, const char* sHeaders, const void* pBody, size_t iSize);
int xsHttpReplyFormat(XS_ResponseObject objResp, int iCode, const char* sHeaders, const char* sFormat, ...);
int xsReqMethodID(XS_RequestObject objReq);
int xsReqQueryValue(XS_RequestObject objReq, const char* sName, char* sOut, size_t iCapacity);
const char* XAdmin_PluginReqHeader(XS_RequestObject objReq, const char* sName);
/* >=0 bytes copied, -1 absent, -2 duplicate/invalid/too long. Names ignore case.
 * On failure output is cleared. No raw field view or unterminated string. */
int XAdmin_ReqHeaderCopy(XS_RequestObject req,const char* name,char* out,size_t capacity);
const char* XAdmin_ReqBody(XS_RequestObject objReq);
size_t XAdmin_ReqBodyLen(XS_RequestObject objReq);
#endif

/* multipart/form-data 迭代（宿主 modules/multipart.h 同一实现的 ABI 出口）。
 * 视图全部借用请求正文缓冲（原地解码），Part 存活期 = 正文存活期。
 * 宿主侧在 modules/plugin_host.h 以 static 实现，声明仅插件侧可见。 */
typedef struct XAdminMultipartPart {
	const char* name;         /* 字段名（非 NUL 结尾，配合 nameLen） */
	size_t      nameLen;
	const char* filename;     /* 无文件字段时为 NULL（非 NUL 结尾） */
	size_t      filenameLen;
	const char* data;         /* Part 正文 */
	size_t      size;
} XAdminMultipartPart;
typedef void (*XAdminAsyncRouteProc)(XS_ServerObject, XS_HostObject,
    XS_RequestObject, XS_ResponseObject, xvalue* session);
/* Host 4.4: bounded, native HTTP/1 streaming. Only deferred routes may use
 * these APIs. Callbacks execute under the application lock; network waits do
 * not. Header/data views are borrowed for one callback. Returning nonzero
 * cancels the upstream. No redirect, retry or automatic decompression. */
typedef struct XAdminHttpStreamConfig {
    uint32_t size;
    const char* url;
    const xhttpfield* headers;
    size_t header_count;
    const void* body;
    size_t body_size; /* <=1 MiB */
    unsigned timeout_ms, first_byte_timeout_ms, idle_timeout_ms;
    size_t max_response; /* <=32 MiB; decoded HTTP body, not SSE frames */
    bool allow_http; /* explicit trusted server configuration only */
    const char* ca_pem; /* optional private CA; never disables verification */
    void* data;
    void (*on_send)(void* data); /* before the first upstream write; may be accepted afterwards */
    int (*on_headers)(void* data, uint16_t status, const xhttpfield* fields, size_t count);
    int (*on_data)(void* data, const void* bytes, size_t size);
} XAdminHttpStreamConfig;
#ifndef XS_PLUGIN_HOST_SIDE
bool XAdmin_MultipartBoundary(const char* sContentType, char* sOut, size_t iCap);
bool XAdmin_MultipartNext(const char* sBody, size_t iBodySize, const char* sBoundary,
	size_t iBoundaryLen, size_t* pOffset, XAdminMultipartPart* pOut);

/* Host 4.1 additions; ABI structure layouts remain v4. Route callbacks only.
 * HTTP temporarily releases the application lock. Copy live configuration
 * before calling; do not keep SQLite statements/transactions open. The host
 * rejects unloading this plugin until I/O completes. Response is xrtFree-owned.
 * Verified HTTPS only; one JSON POST, no redirects/retries, <=1 MiB response.
 * Return 0 on HTTP response, -1 on invalid/local/transport failure, -2 timeout. */
int XAdmin_HttpPostJson(XAdminPluginHandle handle, XS_RequestObject req,
    const char* url, const char* bearer, const char* json, unsigned timeout_ms,
    size_t max_response, int* status, char** response);
/* Copy request/session, pin plugin, and run a route callback on a bounded
 * independent thread. Callback starts under the application lock; HTTPS calls
 * release it. One response, then connection closes. Not a background job API.
 * Only SDK request helpers are valid (no borrowed raw HTTP body reader).
 * The callback must have bounded work; host shutdown joins all callbacks.
 * Call once and return from the original route. 0 accepted, -1 unavailable. */
int XAdmin_DeferRoute(XAdminPluginHandle handle, XS_RequestObject req,
    xvalue* session, XAdminAsyncRouteProc proc);
/* Host 4.2: deferred callbacks only. Sends <=32 MiB in bounded chunks while
 * releasing the application lock. Returns 0 when fully drained, -1 on failure.
 * The connection closes after the callback. Borrowed body/fields remain alive
 * until return; framing fields are generated by the host. No raw socket ABI. */
int XAdmin_ReplyBinary(XS_RequestObject req, uint16 status,
    const xhttpfield* fields, size_t count, const void* body, size_t size,
    unsigned timeout_ms);
/* 0 complete; -1 transport/protocol; -2 deadline; -3 peer/callback cancelled.
 * The request body/headers are copied before releasing the application lock. */
int XAdmin_HttpStream(XAdminPluginHandle plugin, XS_RequestObject req,
    const XAdminHttpStreamConfig* config, int* upstream_status);
/* Chunked response; Begin claims the response even if the first write fails.
 * Write drains a bounded chunk before returning. Finish(false) aborts rather
 * than publishing a misleading successful end. All calls are lock-owned. */
int XAdmin_StreamBegin(XS_RequestObject req, uint16 status,
    const xhttpfield* fields, size_t count, unsigned timeout_ms);
int XAdmin_StreamWrite(XS_RequestObject req, const void* bytes, size_t size);
int XAdmin_StreamFinish(XS_RequestObject req, bool success);
/* Bit 1: verified phone; bit 2: verified email; -1: account unavailable. */
int XAdmin_MemberContactStatus(xvalue* session);
/* Host 4.3: route/callback-only, serialized by the host application lock.
 * Accept retains the connection and binds a verified member session; it
 * rejects query strings, request bodies and invalid WebSocket upgrades.
 * Return 0 accepted, -1 rejected/unavailable. Return immediately after accept.
 * Callbacks borrow message data only for the duration of the call. They must
 * not perform unbounded work. Send copies payload to a bounded queue and never
 * waits for network I/O; -2 means backpressure, -1 means closed/invalid.
 * Close is idempotent. Shutdown/reload joins channels before OnStop/OnUnload;
 * on_close runs exactly once, including failure before on_open. No callback
 * can outlive plugin code. Authentication is rechecked before delivery and
 * periodically; explicit session revocation requests closure immediately. */
int XAdmin_ChannelAccept(XAdminPluginHandle plugin, XS_RequestObject req,
	xvalue* session, const XAdminChannelConfig* config, XAdminChannel* channel);
int XAdmin_ChannelSend(XAdminPluginHandle plugin, XAdminChannel channel,
	bool binary, const void* bytes, size_t size);
int XAdmin_ChannelClose(XAdminPluginHandle plugin, XAdminChannel channel,
	uint16_t code);
/* Admin session-scoped token; borrowed until session destruction. Mutations
 * must check both the token and same-origin request. */
const char* XAdmin_AdminCSRFToken(xvalue* session);
bool XAdmin_CheckAdminCSRF(XS_RequestObject req, xvalue* session);
#endif

int XAdmin_RegisterRoute(XAdminPluginHandle plugin_handle, const XAdminRouteDecl* decl, XAdminRouteToken* token);
int XAdmin_UnregisterRoute(XAdminRouteToken token);
int XAdmin_RegisterDynamicRoute(XAdminPluginHandle plugin_handle, const XAdminDynamicRouteDecl* decl, XAdminRouteToken* token);
int XAdmin_RouteParam(int index, char* out_value, size_t out_cap);
int XAdmin_RouteParamCount(void);
void XAdmin_Free(void* ptr);
const char* XAdmin_ReqPath(XS_RequestObject objReq);
const char* XAdmin_ReqRemote(XS_RequestObject objReq);
char* ServerHashPassword(const char* user, const char* salt, const char* client_hash);

int XAdmin_RegisterMenu(XAdminPluginHandle plugin_handle, const XAdminMenuDecl* decl, int* out_menu_id, XAdminMenuToken* token);
int XAdmin_UnregisterMenu(XAdminMenuToken token);

int XAdmin_RegisterAuthGroup(XAdminPluginHandle plugin_handle, const XAdminAuthGroupDecl* decl, int* out_group_id, XAdminAuthGroupToken* token);
int XAdmin_UnregisterAuthGroup(XAdminAuthGroupToken token);
int XAdmin_RegisterAuth(XAdminPluginHandle plugin_handle, const XAdminAuthDecl* decl, int* out_auth_id, XAdminAuthToken* token);
int XAdmin_UnregisterAuth(XAdminAuthToken token);
int XAdmin_RegisterUriAuth(XAdminPluginHandle plugin_handle, const XAdminUriAuthDecl* decl, int* out_uri_id, XAdminUriAuthToken* token);
int XAdmin_UnregisterUriAuth(XAdminUriAuthToken token);

int XAdmin_ListenEvent(XAdminPluginHandle plugin_handle, const XAdminEventDecl* decl, XAdminEventToken* token);
int XAdmin_UnlistenEvent(XAdminEventToken token);
int XAdmin_EmitEvent(XAdminPluginHandle plugin_handle, const char* event_name, void* payload, size_t payload_size);

int XAdmin_RegisterHook(XAdminPluginHandle plugin_handle, const XAdminHookDecl* decl, XAdminHookToken* token);
int XAdmin_UnregisterHook(XAdminHookToken token);
int XAdmin_InvokeHook(XAdminPluginHandle plugin_handle, const char* hook_name, void* payload, size_t payload_size);

int XAdmin_RegisterService(XAdminPluginHandle plugin_handle, const XAdminServiceDecl* decl, const void* vtable);
int XAdmin_AcquireService(XAdminPluginHandle plugin_handle, const char* name, int major, XAdminServiceLease* out_lease, const void** out_vtable);
int XAdmin_ReleaseService(XAdminServiceLease lease);

int XAdmin_GeneratePlugin(XAdminPluginHandle plugin_handle, const XAdminGeneratedPluginSpec* spec);
int XAdmin_ReloadPlugin(XAdminPluginHandle plugin_handle, const char* xid);
int XAdmin_SetPluginEnabled(XAdminPluginHandle plugin_handle, const char* xid, int enabled);

/* 插件资源体系（page/template/option；静态文件经 /plugin-static/<xid>/ 由宿主直接服务）。
 * 返回的堆串与 xvalue* 由调用方以 xrtFree/xrtValueRelease 释放。 */
int XAdmin_LoadPluginPage(XAdminPluginHandle plugin_handle, XS_ResponseObject resp, int code, const char* header, const char* page);
char* XAdmin_RenderPluginTemplate(XAdminPluginHandle plugin_handle, const char* template_name, xvalue* data, size_t* out_size, char** out_error);
xvalue* XAdmin_PluginOptionLoad(XAdminPluginHandle plugin_handle, const char* file_name);
int XAdmin_PluginOptionSave(XAdminPluginHandle plugin_handle, const char* file_name, xvalue* values);
char* XAdmin_PluginResourcePath(XAdminPluginHandle plugin_handle, const char* resource_dir, const char* rel_path);

#define XADMIN_DECLARE_PLUGIN(descriptor) \
	XADMIN_EXPORT const XAdminPluginDescriptor* XAdmin_GetPluginDescriptor(void) \
	{ \
		return &(descriptor); \
	}

#endif
