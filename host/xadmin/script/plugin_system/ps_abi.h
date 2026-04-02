#ifndef XADMIN_PLUGIN_SYSTEM_ABI_H
#define XADMIN_PLUGIN_SYSTEM_ABI_H

#include <stdarg.h>
#include "ps_service.h"
#include "ps_signal.h"

XAdminHostAPI G_PluginSystemHostAPI;

bool PluginSystem_Enable(str sName);
bool PluginSystem_Disable(str sName);
bool PluginSystem_Reload(str sName);
bool PluginSystem_Generate(const XAdminGeneratedPluginSpec* spec);

typedef struct {
	PluginSystemGeneration* pGeneration;
	bool bReleased;
	int iResourceId;
} PluginSystemTokenBase;

typedef struct {
	PluginSystemTokenBase base;
	str sPath;
	void* pProc;
} PluginSystemRouteToken;

typedef struct {
	PluginSystemTokenBase base;
	int iMenuId;
} PluginSystemMenuToken;

typedef struct {
	PluginSystemTokenBase base;
	int iScope;
	int iGroupId;
} PluginSystemAuthGroupToken;

typedef struct {
	PluginSystemTokenBase base;
	int iScope;
	int iAuthId;
} PluginSystemAuthToken;

typedef struct {
	PluginSystemTokenBase base;
	int iScope;
	int iUriId;
	int iAuthId;
	str sUri;
} PluginSystemUriAuthToken;

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

bool PS_HostTextEquals(const char* sLeft, const char* sRight)
{
	if ( sLeft == sRight ) {
		return TRUE;
	}
	if ( (sLeft == NULL) || (sRight == NULL) ) {
		return FALSE;
	}
	return strcmp(sLeft, sRight) == 0;
}

PluginSystemGeneration* PS_HostGetGeneration(void* plugin_handle)
{
	return (PluginSystemGeneration*)plugin_handle;
}

const char* PS_HostGetActorXid(void* plugin_handle)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);
	if ( pGeneration && pGeneration->pPackage && PS_PackageKey(pGeneration->pPackage) ) {
		return (const char*)PS_PackageKey(pGeneration->pPackage);
	}
	return "(system)";
}

void PS_HostAppendToken(xlist lstTokens, ptr pToken)
{
	int iIndex;

	if ( (lstTokens == NULL) || (pToken == NULL) ) {
		return;
	}

	iIndex = xrtListCount(lstTokens);
	xrtListSetPtr(lstTokens, iIndex, pToken, NULL);
}

void PS_HostDetachToken(xlist lstTokens, ptr pToken)
{
	int iCount;

	if ( (lstTokens == NULL) || (pToken == NULL) ) {
		return;
	}

	iCount = xrtListCount(lstTokens);
	for ( int i = 0; i < iCount; i++ ) {
		if ( xrtListGetPtr(lstTokens, i) == pToken ) {
			xrtListSetPtr(lstTokens, i, NULL, NULL);
			return;
		}
	}
}

void PS_HostFormatRefInt(int iValue, char sBuf[32])
{
	snprintf(sBuf, 32, "%d", iValue);
}

bool PS_HostMatchOwnedRow(const char* sSQL, int iRowId, PluginSystemGeneration* pGeneration)
{
	sqlite3_stmt* stmt = NULL;
	bool bMatch = FALSE;
	const unsigned char* sInstanceId = NULL;
	int iGeneration = 0;

	if ( (G_DB == NULL) || (sSQL == NULL) || (iRowId <= 0) || (pGeneration == NULL) || (pGeneration->pInstance == NULL) ) {
		return FALSE;
	}

	if ( sqlite3_prepare_v3(G_DB, sSQL, -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}

	sqlite3_bind_int(stmt, 1, iRowId);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		sInstanceId = sqlite3_column_text(stmt, 0);
		iGeneration = sqlite3_column_int(stmt, 1);
		bMatch = PS_HostTextEquals((const char*)sInstanceId, pGeneration->pInstance->sInstanceId) && (iGeneration == (int)pGeneration->iGeneration);
	}

	sqlite3_finalize(stmt);
	return bMatch;
}

void PS_HostRefreshAdminCaches()
{
	ReloadCache_Auth_Auth();
	ReloadCache_Auth_Group();
	Auth_ReloadCache();
}

void PS_HostRefreshMemberCaches()
{
	ReloadCache_MemberAuth();
	ReloadCache_MemberAuthGroup();
	MemberAuth_ReloadCache();
}

void PS_HostRefreshRouteCaches()
{
	Auth_ReloadCache();
	MemberAuth_ReloadCache();
}

void PS_HostRefreshCachesByScope(int iScope)
{
	if ( iScope == XADMIN_AUTH_SCOPE_MEMBER ) {
		PS_HostRefreshMemberCaches();
	} else {
		PS_HostRefreshAdminCaches();
	}
}

const char* PS_HostAuthGroupTable(int iScope)
{
	return (iScope == XADMIN_AUTH_SCOPE_MEMBER) ? "memberAuthGroup" : "authGroup";
}

const char* PS_HostAuthTable(int iScope)
{
	return (iScope == XADMIN_AUTH_SCOPE_MEMBER) ? "memberAuth" : "auth";
}

const char* PS_HostAuthGroupResourceType(int iScope)
{
	return (iScope == XADMIN_AUTH_SCOPE_MEMBER) ? "member_auth_group" : "admin_auth_group";
}

const char* PS_HostAuthResourceType(int iScope)
{
	return (iScope == XADMIN_AUTH_SCOPE_MEMBER) ? "member_auth" : "admin_auth";
}

const char* PS_HostUriAuthResourceType(int iScope)
{
	return (iScope == XADMIN_AUTH_SCOPE_MEMBER) ? "member_uri_auth" : "admin_uri_auth";
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
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);
	PluginSystemRouteToken* pToken;
	RouteInfo* pInfo;

	if ( (decl == NULL) || (decl->path == NULL) || (decl->proc == NULL) || (pGeneration == NULL) ) {
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

	pToken = xrtMalloc(sizeof(PluginSystemRouteToken));
	if ( pToken == NULL ) {
		return -1;
	}
	memset(pToken, 0, sizeof(PluginSystemRouteToken));
	pToken->base.pGeneration = pGeneration;
	pToken->sPath = xrtCopyStr((str)decl->path, 0);
	pToken->pProc = decl->proc;
	pToken->base.iResourceId = PS_StorageTrackResource(pGeneration, "generation", "route", decl->path, decl->path, "auto_unload");
	if ( pToken->sPath == NULL ) {
		xrtFree(pToken);
		return -1;
	}

	if ( token ) {
		*token = (XAdminRouteToken)(uintptr_t)pToken;
	}
	PS_HostAppendToken(pGeneration->lstRouteTokens, pToken);
	PS_HostRefreshRouteCaches();
	return 0;
}

int PS_HostUnregisterRoute(XAdminRouteToken token)
{
	PluginSystemRouteToken* pToken = (PluginSystemRouteToken*)(uintptr_t)token;
	RouteInfo* pCurrent;

	if ( pToken == NULL ) {
		return -1;
	}
	if ( pToken->base.bReleased ) {
		return 0;
	}

	pToken->base.bReleased = TRUE;
	if ( pToken->base.pGeneration ) {
		PS_HostDetachToken(pToken->base.pGeneration->lstRouteTokens, pToken);
	}

	if ( pToken->sPath ) {
		pCurrent = (RouteInfo*)xrtDictGet(G_StaticRouteTableHTTP, pToken->sPath, strlen(pToken->sPath));
		if ( pCurrent && (pCurrent->Proc == pToken->pProc) ) {
			xrtDictRemove(G_StaticRouteTableHTTP, pToken->sPath, strlen(pToken->sPath));
		}
		xrtFree(pToken->sPath);
		pToken->sPath = NULL;
	}

	PS_StorageUpdateResourceStatus(pToken->base.iResourceId, "removed");
	xrtFree(pToken);
	PS_HostRefreshRouteCaches();
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

int PS_HostGeneratePlugin(void* plugin_handle, const XAdminGeneratedPluginSpec* spec)
{
	if ( (spec == NULL) || (spec->xid == NULL) || (spec->xid[0] == '\0') ) {
		return -1;
	}

	PS_HostLog(LOG_INFO, "plugin generate requested: actor=%s xid=%s", PS_HostGetActorXid(plugin_handle), spec->xid);
	return PluginSystem_Generate(spec) ? 0 : -1;
}

int PS_HostReloadPlugin(void* plugin_handle, const char* xid)
{
	if ( (xid == NULL) || (xid[0] == '\0') ) {
		return -1;
	}

	PS_HostLog(LOG_INFO, "plugin reload requested: actor=%s xid=%s", PS_HostGetActorXid(plugin_handle), xid);
	return PluginSystem_Reload((str)xid) ? 0 : -1;
}

int PS_HostSetPluginEnabled(void* plugin_handle, const char* xid, int enabled)
{
	if ( (xid == NULL) || (xid[0] == '\0') ) {
		return -1;
	}

	PS_HostLog(LOG_INFO, "plugin enable state requested: actor=%s xid=%s enabled=%d", PS_HostGetActorXid(plugin_handle), xid, enabled ? 1 : 0);
	return (enabled ? PluginSystem_Enable((str)xid) : PluginSystem_Disable((str)xid)) ? 0 : -1;
}

int PS_HostFindMenuId(PluginSystemGeneration* pGeneration, const XAdminMenuDecl* decl)
{
	sqlite3_stmt* stmt = NULL;
	int iMenuId = 0;

	if ( (G_DB == NULL) || (pGeneration == NULL) || (pGeneration->pInstance == NULL) || (decl == NULL) ) {
		return 0;
	}

	if ( decl->href && decl->href[0] ) {
		if ( sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE plugin_instance_id = ? AND href = ? ORDER BY id DESC LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return 0;
		}
		PS_StorageBindText(stmt, 1, pGeneration->pInstance->sInstanceId);
		PS_StorageBindText(stmt, 2, decl->href);
	} else {
		if ( sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE plugin_instance_id = ? AND parent = ? AND title = ? AND type = ? ORDER BY id DESC LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return 0;
		}
		PS_StorageBindText(stmt, 1, pGeneration->pInstance->sInstanceId);
		sqlite3_bind_int(stmt, 2, decl->parent_id);
		PS_StorageBindText(stmt, 3, decl->title);
		sqlite3_bind_int(stmt, 4, decl->type);
	}

	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iMenuId = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return iMenuId;
}

int PS_HostRegisterMenu(void* plugin_handle, const XAdminMenuDecl* decl, int* out_menu_id, XAdminMenuToken* token)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);
	PluginSystemMenuToken* pToken = NULL;
	sqlite3_stmt* stmt = NULL;
	char sRef[32];
	int iMenuId = 0;
	int64 iNow;

	if ( (decl == NULL) || (decl->title == NULL) || (decl->title[0] == '\0') || (pGeneration == NULL) || (pGeneration->pInstance == NULL) ) {
		return -1;
	}

	iMenuId = PS_HostFindMenuId(pGeneration, decl);
	iNow = xrtNow();
	if ( iMenuId > 0 ) {
		if ( sqlite3_prepare_v3(G_DB, "UPDATE menu SET parent = ?, title = ?, icon = ?, type = ?, openType = ?, href = ?, sort = ?, visible = ?, remark = ?, updateTime = ?, plugin_instance_id = ?, plugin_generation = ?, isDelete = 0 WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return -1;
		}
		sqlite3_bind_int(stmt, 1, decl->parent_id);
		PS_StorageBindText(stmt, 2, decl->title);
		PS_StorageBindText(stmt, 3, decl->icon);
		sqlite3_bind_int(stmt, 4, decl->type);
		PS_StorageBindText(stmt, 5, decl->open_type ? decl->open_type : "_component");
		PS_StorageBindText(stmt, 6, decl->href);
		sqlite3_bind_int(stmt, 7, decl->sort);
		sqlite3_bind_int(stmt, 8, decl->visible ? 1 : 0);
		PS_StorageBindText(stmt, 9, decl->remark);
		sqlite3_bind_int64(stmt, 10, iNow);
		PS_StorageBindText(stmt, 11, pGeneration->pInstance->sInstanceId);
		sqlite3_bind_int(stmt, 12, (int)pGeneration->iGeneration);
		sqlite3_bind_int(stmt, 13, iMenuId);
	} else {
		if ( sqlite3_prepare_v3(G_DB, "INSERT INTO menu (parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, plugin_instance_id, plugin_generation, isDelete) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return -1;
		}
		sqlite3_bind_int(stmt, 1, decl->parent_id);
		PS_StorageBindText(stmt, 2, decl->title);
		PS_StorageBindText(stmt, 3, decl->icon);
		sqlite3_bind_int(stmt, 4, decl->type);
		PS_StorageBindText(stmt, 5, decl->open_type ? decl->open_type : "_component");
		PS_StorageBindText(stmt, 6, decl->href);
		sqlite3_bind_int(stmt, 7, decl->sort);
		sqlite3_bind_int(stmt, 8, decl->visible ? 1 : 0);
		PS_StorageBindText(stmt, 9, decl->remark);
		sqlite3_bind_int64(stmt, 10, iNow);
		sqlite3_bind_int64(stmt, 11, iNow);
		PS_StorageBindText(stmt, 12, pGeneration->pInstance->sInstanceId);
		sqlite3_bind_int(stmt, 13, (int)pGeneration->iGeneration);
	}

	if ( sqlite3_step(stmt) != SQLITE_DONE ) {
		sqlite3_finalize(stmt);
		return -1;
	}
	if ( iMenuId <= 0 ) {
		iMenuId = (int)sqlite3_last_insert_rowid(G_DB);
	}
	sqlite3_finalize(stmt);

	pToken = xrtMalloc(sizeof(PluginSystemMenuToken));
	if ( pToken == NULL ) {
		return -1;
	}
	memset(pToken, 0, sizeof(PluginSystemMenuToken));
	pToken->base.pGeneration = pGeneration;
	pToken->iMenuId = iMenuId;
	PS_HostFormatRefInt(iMenuId, sRef);
	pToken->base.iResourceId = PS_StorageTrackResource(pGeneration, "generation", "menu", decl->key ? decl->key : ((decl->href && decl->href[0]) ? decl->href : decl->title), sRef, "soft_delete");
	PS_HostAppendToken(pGeneration->lstMenuTokens, pToken);

	if ( out_menu_id ) {
		*out_menu_id = iMenuId;
	}
	if ( token ) {
		*token = (XAdminMenuToken)(uintptr_t)pToken;
	}
	return 0;
}

int PS_HostUnregisterMenu(XAdminMenuToken token)
{
	PluginSystemMenuToken* pToken = (PluginSystemMenuToken*)(uintptr_t)token;
	sqlite3_stmt* stmt = NULL;

	if ( pToken == NULL ) {
		return -1;
	}
	if ( pToken->base.bReleased ) {
		return 0;
	}

	pToken->base.bReleased = TRUE;
	if ( pToken->base.pGeneration ) {
		PS_HostDetachToken(pToken->base.pGeneration->lstMenuTokens, pToken);
	}

	if ( PS_HostMatchOwnedRow("SELECT plugin_instance_id, plugin_generation FROM menu WHERE id = ?", pToken->iMenuId, pToken->base.pGeneration) ) {
		if ( sqlite3_prepare_v3(G_DB, "UPDATE menu SET isDelete = 1, updateTime = ? WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, xrtNow());
			sqlite3_bind_int(stmt, 2, pToken->iMenuId);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}

	PS_StorageUpdateResourceStatus(pToken->base.iResourceId, "removed");
	xrtFree(pToken);
	return 0;
}

int PS_HostFindAuthGroupId(PluginSystemGeneration* pGeneration, int iScope, const char* sName)
{
	sqlite3_stmt* stmt = NULL;
	str sSQL = NULL;
	int iGroupId = 0;

	if ( (pGeneration == NULL) || (pGeneration->pInstance == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return 0;
	}

	sSQL = xrtFormat("SELECT id FROM %s WHERE plugin_instance_id = ? AND name = ? ORDER BY id DESC LIMIT 1", PS_HostAuthGroupTable(iScope));
	if ( (sSQL == NULL) || (sqlite3_prepare_v3(G_DB, sSQL, -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK) ) {
		if ( sSQL ) {
			xrtFree(sSQL);
		}
		return 0;
	}

	PS_StorageBindText(stmt, 1, pGeneration->pInstance->sInstanceId);
	PS_StorageBindText(stmt, 2, sName);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iGroupId = sqlite3_column_int(stmt, 0);
	}

	sqlite3_finalize(stmt);
	xrtFree(sSQL);
	return iGroupId;
}

int PS_HostRegisterAuthGroup(void* plugin_handle, const XAdminAuthGroupDecl* decl, int* out_group_id, XAdminAuthGroupToken* token)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);
	PluginSystemAuthGroupToken* pToken = NULL;
	sqlite3_stmt* stmt = NULL;
	str sSQL = NULL;
	char sRef[32];
	int iGroupId = 0;
	int iScope;
	int64 iNow;

	if ( (decl == NULL) || (decl->name == NULL) || (decl->name[0] == '\0') || (pGeneration == NULL) || (pGeneration->pInstance == NULL) ) {
		return -1;
	}

	iScope = (decl->scope == XADMIN_AUTH_SCOPE_MEMBER) ? XADMIN_AUTH_SCOPE_MEMBER : XADMIN_AUTH_SCOPE_ADMIN;
	iGroupId = PS_HostFindAuthGroupId(pGeneration, iScope, decl->name);
	iNow = xrtNow();
	if ( iGroupId > 0 ) {
		sSQL = xrtFormat("UPDATE %s SET name = ?, desc = ?, sort = ?, updateTime = ?, plugin_instance_id = ?, plugin_generation = ?, isDelete = 0 WHERE id = ?", PS_HostAuthGroupTable(iScope));
	} else {
		sSQL = xrtFormat("INSERT INTO %s (name, desc, sort, createTime, updateTime, plugin_instance_id, plugin_generation, isDelete) VALUES (?, ?, ?, ?, ?, ?, ?, 0)", PS_HostAuthGroupTable(iScope));
	}
	if ( (sSQL == NULL) || (sqlite3_prepare_v3(G_DB, sSQL, -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK) ) {
		if ( sSQL ) {
			xrtFree(sSQL);
		}
		return -1;
	}

	PS_StorageBindText(stmt, 1, decl->name);
	PS_StorageBindText(stmt, 2, decl->description);
	sqlite3_bind_int(stmt, 3, decl->sort);
	if ( iGroupId > 0 ) {
		sqlite3_bind_int64(stmt, 4, iNow);
		PS_StorageBindText(stmt, 5, pGeneration->pInstance->sInstanceId);
		sqlite3_bind_int(stmt, 6, (int)pGeneration->iGeneration);
		sqlite3_bind_int(stmt, 7, iGroupId);
	} else {
		sqlite3_bind_int64(stmt, 4, iNow);
		sqlite3_bind_int64(stmt, 5, iNow);
		PS_StorageBindText(stmt, 6, pGeneration->pInstance->sInstanceId);
		sqlite3_bind_int(stmt, 7, (int)pGeneration->iGeneration);
	}

	if ( sqlite3_step(stmt) != SQLITE_DONE ) {
		sqlite3_finalize(stmt);
		xrtFree(sSQL);
		return -1;
	}
	if ( iGroupId <= 0 ) {
		iGroupId = (int)sqlite3_last_insert_rowid(G_DB);
	}

	sqlite3_finalize(stmt);
	xrtFree(sSQL);

	pToken = xrtMalloc(sizeof(PluginSystemAuthGroupToken));
	if ( pToken == NULL ) {
		return -1;
	}
	memset(pToken, 0, sizeof(PluginSystemAuthGroupToken));
	pToken->base.pGeneration = pGeneration;
	pToken->iScope = iScope;
	pToken->iGroupId = iGroupId;
	PS_HostFormatRefInt(iGroupId, sRef);
	pToken->base.iResourceId = PS_StorageTrackResource(pGeneration, "generation", PS_HostAuthGroupResourceType(iScope), decl->key ? decl->key : decl->name, sRef, "soft_delete");
	PS_HostAppendToken(pGeneration->lstAuthGroupTokens, pToken);

	if ( out_group_id ) {
		*out_group_id = iGroupId;
	}
	if ( token ) {
		*token = (XAdminAuthGroupToken)(uintptr_t)pToken;
	}

	PS_HostRefreshCachesByScope(iScope);
	return 0;
}

int PS_HostUnregisterAuthGroup(XAdminAuthGroupToken token)
{
	PluginSystemAuthGroupToken* pToken = (PluginSystemAuthGroupToken*)(uintptr_t)token;
	sqlite3_stmt* stmt = NULL;
	str sSQL = NULL;
	int iScope;

	if ( pToken == NULL ) {
		return -1;
	}
	if ( pToken->base.bReleased ) {
		return 0;
	}

	iScope = pToken->iScope;
	pToken->base.bReleased = TRUE;
	if ( pToken->base.pGeneration ) {
		PS_HostDetachToken(pToken->base.pGeneration->lstAuthGroupTokens, pToken);
	}

	sSQL = xrtFormat("SELECT plugin_instance_id, plugin_generation FROM %s WHERE id = ?", PS_HostAuthGroupTable(pToken->iScope));
	if ( (sSQL != NULL) && PS_HostMatchOwnedRow(sSQL, pToken->iGroupId, pToken->base.pGeneration) ) {
		str sMoveSQL = xrtFormat("UPDATE %s SET groupID = 1, updateTime = ? WHERE groupID = ? AND isDelete = 0", PS_HostAuthTable(pToken->iScope));
		if ( (sMoveSQL != NULL) && (sqlite3_prepare_v3(G_DB, sMoveSQL, -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK) ) {
			sqlite3_bind_int64(stmt, 1, xrtNow());
			sqlite3_bind_int(stmt, 2, pToken->iGroupId);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		if ( sMoveSQL ) {
			xrtFree(sMoveSQL);
		}

		stmt = NULL;
		xrtFree(sSQL);
		sSQL = xrtFormat("UPDATE %s SET isDelete = 1, updateTime = ? WHERE id = ?", PS_HostAuthGroupTable(pToken->iScope));
		if ( (sSQL != NULL) && (sqlite3_prepare_v3(G_DB, sSQL, -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK) ) {
			sqlite3_bind_int64(stmt, 1, xrtNow());
			sqlite3_bind_int(stmt, 2, pToken->iGroupId);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	} else if ( sSQL != NULL ) {
		xrtFree(sSQL);
		sSQL = NULL;
	}
	if ( sSQL ) {
		xrtFree(sSQL);
	}

	PS_StorageUpdateResourceStatus(pToken->base.iResourceId, "removed");
	xrtFree(pToken);
	PS_HostRefreshCachesByScope(iScope);
	return 0;
}

int PS_HostFindAuthId(PluginSystemGeneration* pGeneration, int iScope, const char* sName)
{
	sqlite3_stmt* stmt = NULL;
	str sSQL = NULL;
	int iAuthId = 0;

	if ( (pGeneration == NULL) || (pGeneration->pInstance == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return 0;
	}

	sSQL = xrtFormat("SELECT id FROM %s WHERE plugin_instance_id = ? AND name = ? ORDER BY id DESC LIMIT 1", PS_HostAuthTable(iScope));
	if ( (sSQL == NULL) || (sqlite3_prepare_v3(G_DB, sSQL, -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK) ) {
		if ( sSQL ) {
			xrtFree(sSQL);
		}
		return 0;
	}

	PS_StorageBindText(stmt, 1, pGeneration->pInstance->sInstanceId);
	PS_StorageBindText(stmt, 2, sName);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iAuthId = sqlite3_column_int(stmt, 0);
	}

	sqlite3_finalize(stmt);
	xrtFree(sSQL);
	return iAuthId;
}

int PS_HostRegisterAuth(void* plugin_handle, const XAdminAuthDecl* decl, int* out_auth_id, XAdminAuthToken* token)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);
	PluginSystemAuthToken* pToken = NULL;
	sqlite3_stmt* stmt = NULL;
	str sSQL = NULL;
	char sRef[32];
	int iAuthId = 0;
	int iScope;
	int64 iNow;

	if ( (decl == NULL) || (decl->name == NULL) || (decl->name[0] == '\0') || (pGeneration == NULL) || (pGeneration->pInstance == NULL) || (decl->group_id <= 0) ) {
		return -1;
	}

	iScope = (decl->scope == XADMIN_AUTH_SCOPE_MEMBER) ? XADMIN_AUTH_SCOPE_MEMBER : XADMIN_AUTH_SCOPE_ADMIN;
	iAuthId = PS_HostFindAuthId(pGeneration, iScope, decl->name);
	iNow = xrtNow();
	if ( iAuthId > 0 ) {
		sSQL = xrtFormat("UPDATE %s SET groupID = ?, name = ?, desc = ?, sort = ?, updateTime = ?, plugin_instance_id = ?, plugin_generation = ?, isDelete = 0 WHERE id = ?", PS_HostAuthTable(iScope));
	} else {
		sSQL = xrtFormat("INSERT INTO %s (groupID, name, desc, sort, createTime, updateTime, plugin_instance_id, plugin_generation, isDelete) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0)", PS_HostAuthTable(iScope));
	}
	if ( (sSQL == NULL) || (sqlite3_prepare_v3(G_DB, sSQL, -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK) ) {
		if ( sSQL ) {
			xrtFree(sSQL);
		}
		return -1;
	}

	sqlite3_bind_int(stmt, 1, decl->group_id);
	PS_StorageBindText(stmt, 2, decl->name);
	PS_StorageBindText(stmt, 3, decl->description);
	sqlite3_bind_int(stmt, 4, decl->sort);
	if ( iAuthId > 0 ) {
		sqlite3_bind_int64(stmt, 5, iNow);
		PS_StorageBindText(stmt, 6, pGeneration->pInstance->sInstanceId);
		sqlite3_bind_int(stmt, 7, (int)pGeneration->iGeneration);
		sqlite3_bind_int(stmt, 8, iAuthId);
	} else {
		sqlite3_bind_int64(stmt, 5, iNow);
		sqlite3_bind_int64(stmt, 6, iNow);
		PS_StorageBindText(stmt, 7, pGeneration->pInstance->sInstanceId);
		sqlite3_bind_int(stmt, 8, (int)pGeneration->iGeneration);
	}

	if ( sqlite3_step(stmt) != SQLITE_DONE ) {
		sqlite3_finalize(stmt);
		xrtFree(sSQL);
		return -1;
	}
	if ( iAuthId <= 0 ) {
		iAuthId = (int)sqlite3_last_insert_rowid(G_DB);
	}

	sqlite3_finalize(stmt);
	xrtFree(sSQL);

	pToken = xrtMalloc(sizeof(PluginSystemAuthToken));
	if ( pToken == NULL ) {
		return -1;
	}
	memset(pToken, 0, sizeof(PluginSystemAuthToken));
	pToken->base.pGeneration = pGeneration;
	pToken->iScope = iScope;
	pToken->iAuthId = iAuthId;
	PS_HostFormatRefInt(iAuthId, sRef);
	pToken->base.iResourceId = PS_StorageTrackResource(pGeneration, "generation", PS_HostAuthResourceType(iScope), decl->key ? decl->key : decl->name, sRef, "soft_delete");
	PS_HostAppendToken(pGeneration->lstAuthTokens, pToken);

	if ( out_auth_id ) {
		*out_auth_id = iAuthId;
	}
	if ( token ) {
		*token = (XAdminAuthToken)(uintptr_t)pToken;
	}

	PS_HostRefreshCachesByScope(iScope);
	return 0;
}

int PS_HostUnregisterAuth(XAdminAuthToken token)
{
	PluginSystemAuthToken* pToken = (PluginSystemAuthToken*)(uintptr_t)token;
	sqlite3_stmt* stmt = NULL;
	str sSQL = NULL;
	int iScope;

	if ( pToken == NULL ) {
		return -1;
	}
	if ( pToken->base.bReleased ) {
		return 0;
	}

	iScope = pToken->iScope;
	pToken->base.bReleased = TRUE;
	if ( pToken->base.pGeneration ) {
		PS_HostDetachToken(pToken->base.pGeneration->lstAuthTokens, pToken);
	}

	sSQL = xrtFormat("SELECT plugin_instance_id, plugin_generation FROM %s WHERE id = ?", PS_HostAuthTable(pToken->iScope));
	if ( (sSQL != NULL) && PS_HostMatchOwnedRow(sSQL, pToken->iAuthId, pToken->base.pGeneration) ) {
		str sMoveSQL = xrtCopyStr((pToken->iScope == XADMIN_AUTH_SCOPE_MEMBER) ? "UPDATE uris SET authID = 1, updateTime = ? WHERE authID = ? AND isBackend = 0" : "UPDATE uris SET authID = 1, updateTime = ? WHERE authID = ? AND isBackend = 1", 0);
		if ( (sMoveSQL != NULL) && (sqlite3_prepare_v3(G_DB, sMoveSQL, -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK) ) {
			sqlite3_bind_int64(stmt, 1, xrtNow());
			sqlite3_bind_int(stmt, 2, pToken->iAuthId);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		if ( sMoveSQL ) {
			xrtFree(sMoveSQL);
		}

		stmt = NULL;
		xrtFree(sSQL);
		sSQL = xrtFormat("UPDATE %s SET isDelete = 1, updateTime = ? WHERE id = ?", PS_HostAuthTable(pToken->iScope));
		if ( (sSQL != NULL) && (sqlite3_prepare_v3(G_DB, sSQL, -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK) ) {
			sqlite3_bind_int64(stmt, 1, xrtNow());
			sqlite3_bind_int(stmt, 2, pToken->iAuthId);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	} else if ( sSQL != NULL ) {
		xrtFree(sSQL);
		sSQL = NULL;
	}
	if ( sSQL ) {
		xrtFree(sSQL);
	}

	PS_StorageUpdateResourceStatus(pToken->base.iResourceId, "removed");
	xrtFree(pToken);
	PS_HostRefreshCachesByScope(iScope);
	return 0;
}

int PS_HostFindUriId(const char* sUri)
{
	sqlite3_stmt* stmt = NULL;
	int iUriId = 0;

	if ( (G_DB == NULL) || (sUri == NULL) || (sUri[0] == '\0') ) {
		return 0;
	}

	if ( sqlite3_prepare_v3(G_DB, "SELECT id FROM uris WHERE uri = ? LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return 0;
	}

	PS_StorageBindText(stmt, 1, sUri);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iUriId = sqlite3_column_int(stmt, 0);
	}

	sqlite3_finalize(stmt);
	return iUriId;
}

int PS_HostRegisterUriAuth(void* plugin_handle, const XAdminUriAuthDecl* decl, int* out_uri_id, XAdminUriAuthToken* token)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);
	PluginSystemUriAuthToken* pToken = NULL;
	sqlite3_stmt* stmt = NULL;
	RouteInfo* pInfo;
	char sRef[32];
	int iUriId = 0;
	int iScope;
	int64 iNow;

	if ( (decl == NULL) || (decl->uri == NULL) || (decl->uri[0] == '\0') || (decl->auth_id <= 0) || (pGeneration == NULL) || (pGeneration->pInstance == NULL) ) {
		return -1;
	}

	pInfo = (RouteInfo*)xrtDictGet(G_StaticRouteTableHTTP, (str)decl->uri, strlen(decl->uri));
	if ( pInfo == NULL ) {
		return -1;
	}

	iScope = (decl->scope == XADMIN_AUTH_SCOPE_MEMBER) ? XADMIN_AUTH_SCOPE_MEMBER : XADMIN_AUTH_SCOPE_ADMIN;
	iUriId = PS_HostFindUriId(decl->uri);
	iNow = xrtNow();
	if ( iUriId > 0 ) {
		if ( sqlite3_prepare_v3(G_DB, "UPDATE uris SET authID = ?, uri = ?, desc = ?, isBackend = ?, needAuth = ?, needLog = ?, keepActive = ?, sort = ?, updateTime = ?, plugin_instance_id = ?, plugin_generation = ? WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return -1;
		}
		sqlite3_bind_int(stmt, 1, decl->auth_id);
		PS_StorageBindText(stmt, 2, decl->uri);
		PS_StorageBindText(stmt, 3, decl->description);
		sqlite3_bind_int(stmt, 4, (iScope == XADMIN_AUTH_SCOPE_MEMBER) ? 0 : 1);
		sqlite3_bind_int(stmt, 5, decl->need_auth ? 1 : 0);
		sqlite3_bind_int(stmt, 6, decl->need_log ? 1 : 0);
		sqlite3_bind_int(stmt, 7, decl->keep_active ? 1 : 0);
		sqlite3_bind_int(stmt, 8, decl->sort);
		sqlite3_bind_int64(stmt, 9, iNow);
		PS_StorageBindText(stmt, 10, pGeneration->pInstance->sInstanceId);
		sqlite3_bind_int(stmt, 11, (int)pGeneration->iGeneration);
		sqlite3_bind_int(stmt, 12, iUriId);
	} else {
		if ( sqlite3_prepare_v3(G_DB, "INSERT INTO uris (authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime, plugin_instance_id, plugin_generation) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return -1;
		}
		sqlite3_bind_int(stmt, 1, decl->auth_id);
		PS_StorageBindText(stmt, 2, decl->uri);
		PS_StorageBindText(stmt, 3, decl->description);
		sqlite3_bind_int(stmt, 4, (iScope == XADMIN_AUTH_SCOPE_MEMBER) ? 0 : 1);
		sqlite3_bind_int(stmt, 5, decl->need_auth ? 1 : 0);
		sqlite3_bind_int(stmt, 6, decl->need_log ? 1 : 0);
		sqlite3_bind_int(stmt, 7, decl->keep_active ? 1 : 0);
		sqlite3_bind_int(stmt, 8, decl->sort);
		sqlite3_bind_int64(stmt, 9, iNow);
		sqlite3_bind_int64(stmt, 10, iNow);
		PS_StorageBindText(stmt, 11, pGeneration->pInstance->sInstanceId);
		sqlite3_bind_int(stmt, 12, (int)pGeneration->iGeneration);
	}

	if ( sqlite3_step(stmt) != SQLITE_DONE ) {
		sqlite3_finalize(stmt);
		return -1;
	}
	if ( iUriId <= 0 ) {
		iUriId = (int)sqlite3_last_insert_rowid(G_DB);
	}
	sqlite3_finalize(stmt);

	pInfo->AuthID = decl->auth_id;
	pInfo->bAdmin = (iScope == XADMIN_AUTH_SCOPE_MEMBER) ? FALSE : TRUE;
	pInfo->bAuth = decl->need_auth;
	pInfo->bPutLog = decl->need_log;
	pInfo->bActive = decl->keep_active;

	pToken = xrtMalloc(sizeof(PluginSystemUriAuthToken));
	if ( pToken == NULL ) {
		return -1;
	}
	memset(pToken, 0, sizeof(PluginSystemUriAuthToken));
	pToken->base.pGeneration = pGeneration;
	pToken->iScope = iScope;
	pToken->iUriId = iUriId;
	pToken->iAuthId = decl->auth_id;
	pToken->sUri = xrtCopyStr((str)decl->uri, 0);
	PS_HostFormatRefInt(iUriId, sRef);
	pToken->base.iResourceId = PS_StorageTrackResource(pGeneration, "generation", PS_HostUriAuthResourceType(iScope), decl->key ? decl->key : decl->uri, sRef, "delete");
	PS_HostAppendToken(pGeneration->lstUriAuthTokens, pToken);

	if ( out_uri_id ) {
		*out_uri_id = iUriId;
	}
	if ( token ) {
		*token = (XAdminUriAuthToken)(uintptr_t)pToken;
	}

	PS_HostRefreshCachesByScope(iScope);
	return 0;
}

int PS_HostUnregisterUriAuth(XAdminUriAuthToken token)
{
	PluginSystemUriAuthToken* pToken = (PluginSystemUriAuthToken*)(uintptr_t)token;
	sqlite3_stmt* stmt = NULL;
	RouteInfo* pInfo;
	int iScope;

	if ( pToken == NULL ) {
		return -1;
	}
	if ( pToken->base.bReleased ) {
		return 0;
	}

	iScope = pToken->iScope;
	pToken->base.bReleased = TRUE;
	if ( pToken->base.pGeneration ) {
		PS_HostDetachToken(pToken->base.pGeneration->lstUriAuthTokens, pToken);
	}

	if ( PS_HostMatchOwnedRow("SELECT plugin_instance_id, plugin_generation FROM uris WHERE id = ?", pToken->iUriId, pToken->base.pGeneration) ) {
		if ( sqlite3_prepare_v3(G_DB, "DELETE FROM uris WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int(stmt, 1, pToken->iUriId);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}

		if ( pToken->sUri ) {
			pInfo = (RouteInfo*)xrtDictGet(G_StaticRouteTableHTTP, pToken->sUri, strlen(pToken->sUri));
			if ( pInfo ) {
				pInfo->AuthID = 0;
				pInfo->bAuth = FALSE;
				pInfo->bPutLog = FALSE;
				pInfo->bActive = FALSE;
			}
		}
	}

	PS_StorageUpdateResourceStatus(pToken->base.iResourceId, "removed");
	if ( pToken->sUri ) {
		xrtFree(pToken->sUri);
	}
	xrtFree(pToken);
	PS_HostRefreshCachesByScope(iScope);
	return 0;
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

	G_PluginSystemHostAPI.ui.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.ui.hdr.size = sizeof(XAdminUiAPI);
	G_PluginSystemHostAPI.ui.register_menu = PS_HostRegisterMenu;
	G_PluginSystemHostAPI.ui.unregister_menu = PS_HostUnregisterMenu;

	G_PluginSystemHostAPI.auth.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.auth.hdr.size = sizeof(XAdminAuthAPI);
	G_PluginSystemHostAPI.auth.register_auth_group = PS_HostRegisterAuthGroup;
	G_PluginSystemHostAPI.auth.unregister_auth_group = PS_HostUnregisterAuthGroup;
	G_PluginSystemHostAPI.auth.register_auth = PS_HostRegisterAuth;
	G_PluginSystemHostAPI.auth.unregister_auth = PS_HostUnregisterAuth;
	G_PluginSystemHostAPI.auth.register_uri_auth = PS_HostRegisterUriAuth;
	G_PluginSystemHostAPI.auth.unregister_uri_auth = PS_HostUnregisterUriAuth;

	G_PluginSystemHostAPI.event.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.event.hdr.size = sizeof(XAdminEventAPI);
	G_PluginSystemHostAPI.event.listen = PS_HostListenEvent;
	G_PluginSystemHostAPI.event.unlisten = PS_HostUnlistenEvent;
	G_PluginSystemHostAPI.event.emit = PS_HostEmitEvent;
	G_PluginSystemHostAPI.hook.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.hook.hdr.size = sizeof(XAdminHookAPI);
	G_PluginSystemHostAPI.hook.register_hook = PS_HostRegisterHook;
	G_PluginSystemHostAPI.hook.unregister_hook = PS_HostUnregisterHook;
	G_PluginSystemHostAPI.hook.invoke = PS_HostInvokeHook;

	G_PluginSystemHostAPI.service.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.service.hdr.size = sizeof(XAdminServiceAPI);
	G_PluginSystemHostAPI.service.register_service = PS_HostRegisterService;
	G_PluginSystemHostAPI.service.acquire_service = PS_HostAcquireService;
	G_PluginSystemHostAPI.service.release_service = PS_HostReleaseService;

	G_PluginSystemHostAPI.pluginctl.hdr.abi_version = XADMIN_ABI_VERSION;
	G_PluginSystemHostAPI.pluginctl.hdr.size = sizeof(XAdminPluginControlAPI);
	G_PluginSystemHostAPI.pluginctl.generate_plugin = PS_HostGeneratePlugin;
	G_PluginSystemHostAPI.pluginctl.reload_plugin = PS_HostReloadPlugin;
	G_PluginSystemHostAPI.pluginctl.set_plugin_enabled = PS_HostSetPluginEnabled;
}

#endif
