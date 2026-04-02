#ifndef XADMIN_PS_PLUGIN_API_H
#define XADMIN_PS_PLUGIN_API_H

#include <xs_vnext_full.h>

#if defined(_WIN32) || defined(_WIN64)
#define XADMIN_EXPORT __declspec(dllexport)
#else
#define XADMIN_EXPORT __attribute__((visibility("default")))
#endif

#define XADMIN_ABI_VERSION 3
#define LOG_DEBUG 0
#define LOG_INFO 1
#define LOG_WARN 2
#define LOG_ERROR 3

typedef void* XAdminPluginHandle;
typedef void* XAdminServiceLease;
typedef uintptr_t XAdminRouteToken;
typedef uintptr_t XAdminMenuToken;
typedef uintptr_t XAdminAuthGroupToken;
typedef uintptr_t XAdminAuthToken;
typedef uintptr_t XAdminUriAuthToken;
typedef uintptr_t XAdminEventToken;
typedef uintptr_t XAdminHookToken;

#define XADMIN_AUTH_SCOPE_ADMIN 1
#define XADMIN_AUTH_SCOPE_MEMBER 2
#define XADMIN_HOOK_CONTINUE 0
#define XADMIN_HOOK_STOP 1
#define XADMIN_HOOK_ERROR -1

typedef struct {
	uint32_t abi_version;
	uint32_t size;
} XAdminAbiHeader;

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
	const char* provider_instance_id;
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
	XAdminAbiHeader hdr;
	const char* host_version;
	const char* app_path;
	const char* data_path;
	void (*log)(int level, const char* fmt, ...);
	int64_t (*time_now)();
	void* (*alloc)(size_t size);
	void (*free)(void* ptr);
} XAdminCoreAPI;

typedef struct {
	XAdminAbiHeader hdr;
	int (*register_route)(void* plugin_handle, const XAdminRouteDecl* decl, XAdminRouteToken* token);
	int (*unregister_route)(XAdminRouteToken token);
	int (*reply_json)(XS_ResponseObject resp, int code, const char* json, size_t len);
	int (*reply_html)(XS_ResponseObject resp, int code, const char* html);
} XAdminHttpAPI;

typedef struct {
	XAdminAbiHeader hdr;
} XAdminDbAPI;

typedef struct {
	XAdminAbiHeader hdr;
	int (*register_menu)(void* plugin_handle, const XAdminMenuDecl* decl, int* out_menu_id, XAdminMenuToken* token);
	int (*unregister_menu)(XAdminMenuToken token);
} XAdminUiAPI;

typedef struct {
	XAdminAbiHeader hdr;
	int (*register_auth_group)(void* plugin_handle, const XAdminAuthGroupDecl* decl, int* out_group_id, XAdminAuthGroupToken* token);
	int (*unregister_auth_group)(XAdminAuthGroupToken token);
	int (*register_auth)(void* plugin_handle, const XAdminAuthDecl* decl, int* out_auth_id, XAdminAuthToken* token);
	int (*unregister_auth)(XAdminAuthToken token);
	int (*register_uri_auth)(void* plugin_handle, const XAdminUriAuthDecl* decl, int* out_uri_id, XAdminUriAuthToken* token);
	int (*unregister_uri_auth)(XAdminUriAuthToken token);
} XAdminAuthAPI;

typedef struct {
	XAdminAbiHeader hdr;
	int (*listen)(void* plugin_handle, const XAdminEventDecl* decl, XAdminEventToken* token);
	int (*unlisten)(XAdminEventToken token);
	int (*emit)(void* plugin_handle, const char* event_name, void* payload, size_t payload_size);
} XAdminEventAPI;

typedef struct {
	XAdminAbiHeader hdr;
	int (*register_hook)(void* plugin_handle, const XAdminHookDecl* decl, XAdminHookToken* token);
	int (*unregister_hook)(XAdminHookToken token);
	int (*invoke)(void* plugin_handle, const char* hook_name, void* payload, size_t payload_size);
} XAdminHookAPI;

typedef struct {
	XAdminAbiHeader hdr;
} XAdminJobAPI;

typedef struct {
	XAdminAbiHeader hdr;
} XAdminFsAPI;

typedef struct {
	XAdminAbiHeader hdr;
} XAdminTemplateAPI;

typedef struct {
	XAdminAbiHeader hdr;
	int (*register_service)(void* plugin_handle, const XAdminServiceDecl* decl, const void* vtable);
	int (*acquire_service)(void* plugin_handle, const char* name, int major, XAdminServiceLease* out_lease, const void** out_vtable);
	int (*release_service)(XAdminServiceLease lease);
} XAdminServiceAPI;

typedef struct {
	XAdminAbiHeader hdr;
} XAdminModelAPI;

typedef struct {
	uint32_t abi_version;
	size_t size;
	XAdminCoreAPI core;
	XAdminHttpAPI http;
	XAdminDbAPI db;
	XAdminUiAPI ui;
	XAdminAuthAPI auth;
	XAdminEventAPI event;
	XAdminHookAPI hook;
	XAdminJobAPI job;
	XAdminFsAPI fs;
	XAdminTemplateAPI tpl;
	XAdminServiceAPI service;
	XAdminModelAPI model;
} XAdminHostAPI;

typedef struct XAdminPluginDescriptor {
	uint32_t abi_version;
	size_t size;
	const char* plugin_id;
	const char* version;
	const char* title;
	int (*OnLoad)(const XAdminHostAPI* host, XAdminPluginHandle* out_handle);
	int (*OnInstall)(XAdminPluginHandle handle);
	int (*OnStart)(XAdminPluginHandle handle);
	int (*OnConfigChanged)(XAdminPluginHandle handle, xvalue new_cfg);
	int (*OnHealthCheck)(XAdminPluginHandle handle, XAdminHealthReport* out_report);
	void (*OnStop)(XAdminPluginHandle handle);
	void (*OnUnload)(XAdminPluginHandle handle);
} XAdminPluginDescriptor;

#define XADMIN_DECLARE_PLUGIN(descriptor) \
	XADMIN_EXPORT const XAdminPluginDescriptor* XAdmin_GetPluginDescriptor(void) \
	{ \
		return &(descriptor); \
	}

#endif
