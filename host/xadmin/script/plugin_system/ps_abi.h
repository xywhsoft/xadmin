#ifndef XADMIN_PLUGIN_SYSTEM_ABI_H
#define XADMIN_PLUGIN_SYSTEM_ABI_H

#include <stdarg.h>
#include "ps_types.h"

XAdminHostAPI G_PluginSystemHostAPI;

typedef struct {
	str sPath;
	void* pProc;
} PluginSystemRouteToken;

const char* PS_HostLogLevelName(int iLevel)
{
	switch ( iLevel ) {
		case LOG_DEBUG: return "debug";
		case LOG_INFO: return "info";
		case LOG_WARN: return "warn";
		case LOG_ERROR: return "error";
		default: return "log";
	}
}

void PS_HostLog(int level, const char* fmt, ...)
{
	va_list args;

	printf("[plugin_system:%s] ", PS_HostLogLevelName(level));
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	printf("\n");
}

int64_t PS_HostTimeNow()
{
	return xrtNow();
}

void* PS_HostAlloc(size_t size)
{
	return xrtMalloc(size);
}

void PS_HostFree(void* ptr)
{
	if ( ptr ) {
		xrtFree(ptr);
	}
}

int PS_HostRegisterRoute(void* plugin_handle, const XAdminRouteDecl* decl, XAdminRouteToken* token)
{
	PluginSystemGeneration* pGeneration = (PluginSystemGeneration*)plugin_handle;
	PluginSystemRouteToken* pToken;
	RouteInfo* pInfo;
	int iIndex;

	if ( (decl == NULL) || (decl->path == NULL) || (decl->proc == NULL) ) {
		return -1;
	}

	pToken = xrtMalloc(sizeof(PluginSystemRouteToken));
	if ( pToken == NULL ) {
		return -1;
	}
	memset(pToken, 0, sizeof(PluginSystemRouteToken));

	pToken->sPath = xrtCopyStr((str)decl->path, 0);
	pToken->pProc = decl->proc;
	if ( pToken->sPath == NULL ) {
		xrtFree(pToken);
		return -1;
	}

	AddStaticRouteHTTP((str)decl->path, decl->proc);
	pInfo = (RouteInfo*)xrtDictGet(G_StaticRouteTableHTTP, (str)decl->path, strlen(decl->path));
	if ( pInfo ) {
		pInfo->bAuth = decl->need_auth;
		pInfo->bAdmin = decl->admin_only;
		pInfo->AuthID = decl->auth_id;
		pInfo->AuthLevel = decl->auth_level;
	}

	if ( token ) {
		*token = (XAdminRouteToken)(uintptr_t)pToken;
	}

	if ( pGeneration && pGeneration->lstRouteTokens ) {
		iIndex = xrtListCount(pGeneration->lstRouteTokens);
		xrtListSetPtr(pGeneration->lstRouteTokens, iIndex, pToken, NULL);
	}

	return 0;
}

int PS_HostUnregisterRoute(XAdminRouteToken token)
{
	PluginSystemRouteToken* pToken = (PluginSystemRouteToken*)(uintptr_t)token;
	RouteInfo* pCurrent;

	if ( pToken == NULL ) {
		return -1;
	}

	if ( pToken->sPath ) {
		pCurrent = (RouteInfo*)xrtDictGet(G_StaticRouteTableHTTP, pToken->sPath, strlen(pToken->sPath));
		if ( pCurrent && (pCurrent->Proc == pToken->pProc) ) {
			xrtDictRemove(G_StaticRouteTableHTTP, pToken->sPath, strlen(pToken->sPath));
		}
		xrtFree(pToken->sPath);
	}

	xrtFree(pToken);
	return 0;
}

int PS_HostReplyJson(XS_ResponseObject resp, int code, const char* json, size_t len)
{
	return http_reply(resp, code, HTTP_CT_JSON, json, len);
}

int PS_HostReplyHtml(XS_ResponseObject resp, int code, const char* html)
{
	return http_reply(resp, code, HTTP_CT_HTML, html, 0);
}

int PS_HostRegisterService(void* plugin_handle, const XAdminServiceDecl* decl, const void* vtable)
{
	(void)plugin_handle;
	(void)decl;
	(void)vtable;
	return -1;
}

int PS_HostAcquireService(void* plugin_handle, const char* name, int major, XAdminServiceLease* out_lease, const void** out_vtable)
{
	(void)plugin_handle;
	(void)name;
	(void)major;
	(void)out_lease;
	(void)out_vtable;
	return -1;
}

int PS_HostReleaseService(XAdminServiceLease lease)
{
	(void)lease;
	return -1;
}

void PS_HostAPI_Init()
{
	memset(&G_PluginSystemHostAPI, 0, sizeof(G_PluginSystemHostAPI));

	G_PluginSystemHostAPI.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.size = sizeof(G_PluginSystemHostAPI);

	G_PluginSystemHostAPI.core.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.core.hdr.size = sizeof(XAdminCoreAPI);
	G_PluginSystemHostAPI.core.host_version = "3.0.0";
	G_PluginSystemHostAPI.core.app_path = AppPath;
	G_PluginSystemHostAPI.core.data_path = G_PluginSystem ? G_PluginSystem->sDataPath : NULL;
	G_PluginSystemHostAPI.core.log = PS_HostLog;
	G_PluginSystemHostAPI.core.time_now = PS_HostTimeNow;
	G_PluginSystemHostAPI.core.alloc = PS_HostAlloc;
	G_PluginSystemHostAPI.core.free = PS_HostFree;

	G_PluginSystemHostAPI.http.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.http.hdr.size = sizeof(XAdminHttpAPI);
	G_PluginSystemHostAPI.http.register_route = PS_HostRegisterRoute;
	G_PluginSystemHostAPI.http.unregister_route = PS_HostUnregisterRoute;
	G_PluginSystemHostAPI.http.reply_json = PS_HostReplyJson;
	G_PluginSystemHostAPI.http.reply_html = PS_HostReplyHtml;

	G_PluginSystemHostAPI.db.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.db.hdr.size = sizeof(XAdminDbAPI);
	G_PluginSystemHostAPI.ui.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.ui.hdr.size = sizeof(XAdminUiAPI);
	G_PluginSystemHostAPI.auth.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.auth.hdr.size = sizeof(XAdminAuthAPI);
	G_PluginSystemHostAPI.event.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.event.hdr.size = sizeof(XAdminEventAPI);
	G_PluginSystemHostAPI.hook.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.hook.hdr.size = sizeof(XAdminHookAPI);
	G_PluginSystemHostAPI.job.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.job.hdr.size = sizeof(XAdminJobAPI);
	G_PluginSystemHostAPI.fs.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.fs.hdr.size = sizeof(XAdminFsAPI);
	G_PluginSystemHostAPI.tpl.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.tpl.hdr.size = sizeof(XAdminTemplateAPI);
	G_PluginSystemHostAPI.model.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.model.hdr.size = sizeof(XAdminModelAPI);

	G_PluginSystemHostAPI.service.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.service.hdr.size = sizeof(XAdminServiceAPI);
	G_PluginSystemHostAPI.service.register_service = PS_HostRegisterService;
	G_PluginSystemHostAPI.service.acquire_service = PS_HostAcquireService;
	G_PluginSystemHostAPI.service.release_service = PS_HostReleaseService;
}

#endif
