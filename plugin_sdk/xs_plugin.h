#ifndef XS_PLUGIN_H
#define XS_PLUGIN_H

/* 插件 ABI v4。宿主经 plugin_sdk/ 包含目录提供给插件编译单元；
 * 值/时间/文件等运行时 API 直接使用 xsbase.h 的原生声明与
 * xsCreateTCC 预置符号，SDK 只承载插件契约层。 */

#include "xsbase.h"
#include <stdint.h>

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

#define XADMIN_AUTH_SCOPE_ADMIN 1
#define XADMIN_AUTH_SCOPE_MEMBER 2

#define XADMIN_HOOK_CONTINUE 0
#define XADMIN_HOOK_STOP 1
#define XADMIN_HOOK_ERROR -1

typedef void* XAdminPluginHandle;
typedef void* XAdminServiceLease;
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

typedef struct {
	const char* path;
	void* proc;
	bool need_auth;
	bool admin_only;
	int auth_id;
	int auth_level;
} XAdminRouteDecl;

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
const char* XAdmin_ReqBody(XS_RequestObject objReq);
size_t XAdmin_ReqBodyLen(XS_RequestObject objReq);
#endif

int XAdmin_RegisterRoute(XAdminPluginHandle plugin_handle, const XAdminRouteDecl* decl, XAdminRouteToken* token);
int XAdmin_UnregisterRoute(XAdminRouteToken token);

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

#define XADMIN_DECLARE_PLUGIN(descriptor) \
	XADMIN_EXPORT const XAdminPluginDescriptor* XAdmin_GetPluginDescriptor(void) \
	{ \
		return &(descriptor); \
	}

#endif
