#ifndef XS_PLUGIN_H
#define XS_PLUGIN_H

#include "xsbase.h"

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

#define LOG_DEBUG 0
#define LOG_INFO 1
#define LOG_WARN 2
#define LOG_ERROR 3

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
	const char* path;
	const char* pattern;
	void* proc;
	int priority;
	int method;
	bool need_auth;
	bool admin_only;
	int auth_id;
	int auth_level;
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
	uint32_t abi_version;
	size_t size;
	const char* xid;
	const char* version;
	const char* title;
	int (*OnLoad)(XAdminPluginHandle* out_handle);
	int (*OnInstall)(XAdminPluginHandle handle);
	int (*OnStart)(XAdminPluginHandle handle);
	int (*OnConfigChanged)(XAdminPluginHandle handle, xvalue new_cfg);
	int (*OnHealthCheck)(XAdminPluginHandle handle, XAdminHealthReport* out_report);
	void (*OnStop)(XAdminPluginHandle handle);
	void (*OnUnload)(XAdminPluginHandle handle);
} XAdminPluginDescriptor;
int HttpReplyFormat(XS_ResponseObject objResp, int iCode, str sHead, str sFormat, ...);
void LoadPage(XS_ResponseObject objResp, int iCode, str sHead, str sPage);

int XAdmin_RegisterRoute(XAdminPluginHandle plugin_handle, const XAdminRouteDecl* decl, XAdminRouteToken* token);
int XAdmin_RegisterDynamicRoute(XAdminPluginHandle plugin_handle, const XAdminDynamicRouteDecl* decl, XAdminRouteToken* token);
int XAdmin_RouteParam(int index, char* out_value, size_t out_cap);
int XAdmin_RouteParamCount(void);
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
int XAdmin_LoadPluginPage(XAdminPluginHandle plugin_handle, XS_ResponseObject resp, int code, const char* header, const char* page);
char* XAdmin_RenderPluginTemplate(XAdminPluginHandle plugin_handle, const char* template_name, xvalue data, size_t* out_size, char** out_error);
xvalue XAdmin_PluginOptionLoad(XAdminPluginHandle plugin_handle, const char* file_name);
int XAdmin_PluginOptionSave(XAdminPluginHandle plugin_handle, const char* file_name, xvalue values);
void XAdmin_Log(XAdminPluginHandle plugin_handle, int level, const char* message);
int XAdmin_ReplyJson(XS_ResponseObject resp, int code, xvalue data);
const char* XAdmin_PluginPrivateDbPath(XAdminPluginHandle plugin_handle);
int XAdmin_OpenPluginPrivateDb(XAdminPluginHandle plugin_handle, sqlite3** out_db);
int64 XAdmin_SessionAdminId(xvalue session);
int64 XAdmin_SessionAdminRoleId(xvalue session);
char* XAdmin_PluginResourcePath(XAdminPluginHandle plugin_handle, const char* resource_dir, const char* rel_path);
char* XAdmin_AttachmentUrl(const char* attachment_xid);
char* XAdmin_AttachmentUploadUrl(XAdminPluginHandle plugin_handle, const char* model_name, int64 record_id);
char* XAdmin_AttachmentListUrl(XAdminPluginHandle plugin_handle, const char* model_name);
void XAdmin_Free(void* ptr);

#define XADMIN_DECLARE_PLUGIN(descriptor) \
	XADMIN_EXPORT const XAdminPluginDescriptor* XAdmin_GetPluginDescriptor(void) \
	{ \
		return &(descriptor); \
	}

#endif
