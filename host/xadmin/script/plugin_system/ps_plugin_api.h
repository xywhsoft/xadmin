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
	const char* service_name;
	int major_version;
	int minor_version;
	const char* provider_instance_id;
	int lifecycle_scope;
	size_t vtable_size;
	const char* capabilities_required;
} XAdminServiceDecl;

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
} XAdminUiAPI;

typedef struct {
	XAdminAbiHeader hdr;
} XAdminAuthAPI;

typedef struct {
	XAdminAbiHeader hdr;
} XAdminEventAPI;

typedef struct {
	XAdminAbiHeader hdr;
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
