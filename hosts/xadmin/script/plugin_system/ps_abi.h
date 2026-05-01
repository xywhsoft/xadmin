#ifndef XADMIN_PLUGIN_SYSTEM_ABI_H
#define XADMIN_PLUGIN_SYSTEM_ABI_H

#include <stdarg.h>
#include "ps_service.h"
#include "ps_signal.h"

bool PluginSystem_Enable(str sName);
bool PluginSystem_Disable(str sName);
bool PluginSystem_DisableWithActor(str sName, PluginSystemGeneration* pActorGeneration);
bool PluginSystem_Reload(str sName);
bool PluginSystem_ReloadWithActor(str sName, PluginSystemGeneration* pActorGeneration);
bool PluginSystem_Generate(const XAdminGeneratedPluginSpec* spec);
void PS_TCCRegisterPluginSdkSymbols(TCCState* pTcc);
int XAdmin_GrantDefaultAdminRoleAuth(int auth_id);

typedef struct {
	PluginSystemGeneration* pGeneration;
	bool bReleased;
	bool bPublished;
	int iResourceId;
} PluginSystemTokenBase;

typedef struct {
	PluginSystemTokenBase base;
	str sPath;
	void* pProc;
	bool bNeedAuth;
	bool bAdminOnly;
	int iAuthId;
	int iAuthLevel;
} PluginSystemRouteToken;

typedef struct {
	PluginSystemTokenBase base;
	int iMenuId;
	int iTempMenuId;
	str sKey;
	int iParentId;
	str sTitle;
	str sIcon;
	int iType;
	str sOpenType;
	str sHref;
	int iSort;
	bool bVisible;
	str sRemark;
} PluginSystemMenuToken;

typedef struct {
	PluginSystemTokenBase base;
	int iScope;
	int iGroupId;
	int iTempGroupId;
	str sKey;
	str sName;
	str sDescription;
	int iSort;
} PluginSystemAuthGroupToken;

typedef struct {
	PluginSystemTokenBase base;
	int iScope;
	int iAuthId;
	int iTempAuthId;
	str sKey;
	int iGroupId;
	str sName;
	str sDescription;
	int iSort;
} PluginSystemAuthToken;

typedef struct {
	PluginSystemTokenBase base;
	int iScope;
	int iUriId;
	int iTempUriId;
	int iAuthId;
	str sKey;
	str sUri;
	str sDescription;
	int iSort;
	bool bNeedAuth;
	bool bNeedLog;
	bool bKeepActive;
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

const char* PS_HostGenerationXid(PluginSystemGeneration* pGeneration)
{
	if ( pGeneration && pGeneration->pPackage && PS_PackageKey(pGeneration->pPackage) ) {
		return (const char*)PS_PackageKey(pGeneration->pPackage);
	}
	return "(unknown)";
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

str PS_HostCopyOptionalText(const char* sValue)
{
	if ( (sValue == NULL) || (sValue[0] == '\0') ) {
		return NULL;
	}
	return xrtCopyStr((str)sValue, 0);
}

int PS_HostAllocStagedId(PluginSystemGeneration* pGeneration)
{
	int iValue;

	if ( pGeneration == NULL ) {
		return 0;
	}
	iValue = pGeneration->iNextStagedId;
	pGeneration->iNextStagedId--;
	return iValue;
}

int PS_HostResolveMenuId(PluginSystemGeneration* pGeneration, int iMenuId)
{
	if ( (pGeneration == NULL) || (iMenuId >= 0) || (pGeneration->lstMenuTokens == NULL) ) {
		return iMenuId;
	}

	for ( int i = 0; i < xrtListCount(pGeneration->lstMenuTokens); i++ ) {
		PluginSystemMenuToken* pToken = xrtListGetPtr(pGeneration->lstMenuTokens, i);
		if ( pToken && (pToken->iTempMenuId == iMenuId) ) {
			return pToken->iMenuId;
		}
	}
	return 0;
}

int PS_HostResolveAuthGroupId(PluginSystemGeneration* pGeneration, int iGroupId)
{
	if ( (pGeneration == NULL) || (iGroupId >= 0) || (pGeneration->lstAuthGroupTokens == NULL) ) {
		return iGroupId;
	}

	for ( int i = 0; i < xrtListCount(pGeneration->lstAuthGroupTokens); i++ ) {
		PluginSystemAuthGroupToken* pToken = xrtListGetPtr(pGeneration->lstAuthGroupTokens, i);
		if ( pToken && (pToken->iTempGroupId == iGroupId) ) {
			return pToken->iGroupId;
		}
	}
	return 0;
}

int PS_HostResolveAuthId(PluginSystemGeneration* pGeneration, int iAuthId)
{
	if ( (pGeneration == NULL) || (iAuthId >= 0) || (pGeneration->lstAuthTokens == NULL) ) {
		return iAuthId;
	}

	for ( int i = 0; i < xrtListCount(pGeneration->lstAuthTokens); i++ ) {
		PluginSystemAuthToken* pToken = xrtListGetPtr(pGeneration->lstAuthTokens, i);
		if ( pToken && (pToken->iTempAuthId == iAuthId) ) {
			return pToken->iAuthId;
		}
	}
	return 0;
}

void PS_HostFreeRouteToken(PluginSystemRouteToken* pToken)
{
	if ( pToken == NULL ) {
		return;
	}
	PS_FreeString(&pToken->sPath);
	xrtFree(pToken);
}

void PS_HostFreeMenuToken(PluginSystemMenuToken* pToken)
{
	if ( pToken == NULL ) {
		return;
	}
	PS_FreeString(&pToken->sKey);
	PS_FreeString(&pToken->sTitle);
	PS_FreeString(&pToken->sIcon);
	PS_FreeString(&pToken->sOpenType);
	PS_FreeString(&pToken->sHref);
	PS_FreeString(&pToken->sRemark);
	xrtFree(pToken);
}

void PS_HostFreeAuthGroupToken(PluginSystemAuthGroupToken* pToken)
{
	if ( pToken == NULL ) {
		return;
	}
	PS_FreeString(&pToken->sKey);
	PS_FreeString(&pToken->sName);
	PS_FreeString(&pToken->sDescription);
	xrtFree(pToken);
}

void PS_HostFreeAuthToken(PluginSystemAuthToken* pToken)
{
	if ( pToken == NULL ) {
		return;
	}
	PS_FreeString(&pToken->sKey);
	PS_FreeString(&pToken->sName);
	PS_FreeString(&pToken->sDescription);
	xrtFree(pToken);
}

void PS_HostFreeUriAuthToken(PluginSystemUriAuthToken* pToken)
{
	if ( pToken == NULL ) {
		return;
	}
	PS_FreeString(&pToken->sKey);
	PS_FreeString(&pToken->sUri);
	PS_FreeString(&pToken->sDescription);
	xrtFree(pToken);
}

bool PS_HostMatchOwnedRow(const char* sSQL, int iRowId, PluginSystemGeneration* pGeneration)
{
	sqlite3_stmt* stmt = NULL;
	bool bMatch = FALSE;
	const unsigned char* sXid = NULL;
	int iGeneration = 0;

	if ( (G_DB == NULL) || (sSQL == NULL) || (iRowId <= 0) || (pGeneration == NULL) || (pGeneration->pPackage == NULL) ) {
		return FALSE;
	}

	if ( sqlite3_prepare_v3(G_DB, sSQL, -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}

	sqlite3_bind_int(stmt, 1, iRowId);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		sXid = sqlite3_column_text(stmt, 0);
		iGeneration = sqlite3_column_int(stmt, 1);
		bMatch = PS_HostTextEquals((const char*)sXid, PS_HostGenerationXid(pGeneration)) && (iGeneration == (int)pGeneration->iGeneration);
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

void PS_HostDestroyGenerationRouteTokens(PluginSystemGeneration* pGeneration)
{
	if ( (pGeneration == NULL) || (pGeneration->lstRouteTokens == NULL) ) {
		return;
	}

	for ( int i = 0; i < xrtListCount(pGeneration->lstRouteTokens); i++ ) {
		PluginSystemRouteToken* pToken = xrtListGetPtr(pGeneration->lstRouteTokens, i);
		if ( pToken ) {
			PS_HostFreeRouteToken(pToken);
			xrtListSetPtr(pGeneration->lstRouteTokens, i, NULL, NULL);
		}
	}
}

void PS_HostInvokeRoute(RouteInfo* pInfo, XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	PluginSystemRouteToken* pToken = pInfo ? (PluginSystemRouteToken*)pInfo->pPluginRouteToken : NULL;
	PluginSystemGeneration* pGeneration = pToken ? pToken->base.pGeneration : NULL;

	if ( pToken && (pToken->base.bReleased || !pToken->base.bPublished || (pGeneration == NULL) || (pGeneration->iState != PS_GENERATION_STATE_ACTIVE)) ) {
		xsHttpReplyAuto(objResp, 404, "Content-Type: text/plain\r\n", "plugin route unavailable", 0);
		return;
	}

	if ( pGeneration ) {
		pGeneration->iRefCount++;
	}
	if ( pInfo && pInfo->Proc ) {
		pInfo->Proc(objServer, objHost, objReq, objResp, objSession);
	}
	if ( pGeneration && (pGeneration->iRefCount > 0) ) {
		pGeneration->iRefCount--;
		if ( (pGeneration->iState == PS_GENERATION_STATE_DRAINING) && (pGeneration->iRefCount <= 0) ) {
			PS_RuntimeOnGenerationRefReleased(pGeneration);
		}
	}
}

int PS_HostApplyRouteToken(PluginSystemRouteToken* pToken, bool bForce)
{
	RouteInfo* pInfo;
	int iAuthId;

	if ( pToken == NULL ) {
		return -1;
	}
	if ( pToken->base.bPublished && !bForce ) {
		return 0;
	}

	iAuthId = 0;
	if ( pToken->iAuthId != 0 ) {
		iAuthId = PS_HostResolveAuthId(pToken->base.pGeneration, pToken->iAuthId);
		if ( iAuthId <= 0 ) {
			return -1;
		}
	}

	AddStaticRouteHTTP(pToken->sPath, pToken->pProc);
	pInfo = (RouteInfo*)xrtDictGet(G_StaticRouteTableHTTP, pToken->sPath, strlen(pToken->sPath));
	if ( pInfo == NULL ) {
		return -1;
	}

	pInfo->bAuth = pToken->bNeedAuth;
	pInfo->bAdmin = pToken->bAdminOnly;
	pInfo->AuthID = iAuthId;
	pInfo->AuthLevel = pToken->iAuthLevel;
	pInfo->pPluginRouteToken = pToken;

	if ( pToken->base.iResourceId <= 0 ) {
		pToken->base.iResourceId = PS_StorageTrackResource(pToken->base.pGeneration, "generation", "route", pToken->sPath, pToken->sPath, "auto_unload");
	}
	pToken->base.bPublished = TRUE;
	PS_HostRefreshRouteCaches();
	return 0;
}

int PS_HostPublishRouteToken(PluginSystemRouteToken* pToken)
{
	return PS_HostApplyRouteToken(pToken, FALSE);
}

int PS_HostRegisterRoute(void* plugin_handle, const XAdminRouteDecl* decl, XAdminRouteToken* token)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);
	PluginSystemRouteToken* pToken;

	if ( (decl == NULL) || (decl->path == NULL) || (decl->proc == NULL) || (pGeneration == NULL) ) {
		return -1;
	}

	pToken = xrtMalloc(sizeof(PluginSystemRouteToken));
	if ( pToken == NULL ) {
		return -1;
	}
	memset(pToken, 0, sizeof(PluginSystemRouteToken));
	pToken->base.pGeneration = pGeneration;
	pToken->sPath = xrtCopyStr((str)decl->path, 0);
	pToken->pProc = decl->proc;
	pToken->bNeedAuth = decl->need_auth;
	pToken->bAdminOnly = decl->admin_only;
	pToken->iAuthId = decl->auth_id;
	pToken->iAuthLevel = decl->auth_level;
	if ( pToken->sPath == NULL ) {
		xrtFree(pToken);
		return -1;
	}

	if ( token ) {
		*token = (XAdminRouteToken)(uintptr_t)pToken;
	}
	PS_HostAppendToken(pGeneration->lstRouteTokens, pToken);
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

	if ( pToken->base.bPublished && pToken->sPath ) {
		pCurrent = (RouteInfo*)xrtDictGet(G_StaticRouteTableHTTP, pToken->sPath, strlen(pToken->sPath));
		if ( pCurrent && (pCurrent->Proc == pToken->pProc) ) {
			pCurrent->pPluginRouteToken = NULL;
			xrtDictRemove(G_StaticRouteTableHTTP, pToken->sPath, strlen(pToken->sPath));
		}
	}

	if ( pToken->base.iResourceId > 0 ) {
		PS_StorageUpdateResourceStatus(pToken->base.iResourceId, "removed");
	}
	if ( pToken->base.bPublished ) {
		PS_HostRefreshRouteCaches();
	}
	pToken->base.bPublished = FALSE;
	return 0;
}

int PS_HostReplyJson(XS_ResponseObject resp, int code, const char* json, size_t len)
{
	return xsHttpReplyAuto(resp, code, HTTP_CT_JSON, json, len);
}

int PS_HostReplyHtml(XS_ResponseObject resp, int code, const char* html)
{
	return xsHttpReplyAuto(resp, code, HTTP_CT_HTML, html, 0);
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
	return PluginSystem_ReloadWithActor((str)xid, PS_HostGetGeneration(plugin_handle)) ? 0 : -1;
}

int PS_HostSetPluginEnabled(void* plugin_handle, const char* xid, int enabled)
{
	if ( (xid == NULL) || (xid[0] == '\0') ) {
		return -1;
	}

	PS_HostLog(LOG_INFO, "plugin enable state requested: actor=%s xid=%s enabled=%d", PS_HostGetActorXid(plugin_handle), xid, enabled ? 1 : 0);
	return (enabled ? PluginSystem_Enable((str)xid) : PluginSystem_DisableWithActor((str)xid, PS_HostGetGeneration(plugin_handle))) ? 0 : -1;
}

int PS_HostLoadPluginPage(void* plugin_handle, XS_ResponseObject resp, int code, const char* header, const char* page)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);

	if ( (pGeneration == NULL) || (pGeneration->pPackage == NULL) || (resp == NULL) || (page == NULL) || (page[0] == '\0') ) {
		return -1;
	}

	return PS_ResourceLoadPluginPage(pGeneration, resp, code, header, page) ? 0 : -1;
}

char* PS_HostRenderPluginTemplate(void* plugin_handle, const char* template_name, xvalue data, size_t* out_size, char** out_error)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);

	if ( out_error ) {
		*out_error = NULL;
	}
	if ( (pGeneration == NULL) || (pGeneration->pPackage == NULL) || (template_name == NULL) || (template_name[0] == '\0') ) {
		if ( out_error ) *out_error = xrtCopyStr("invalid plugin template request", 0);
		return NULL;
	}
	return PS_PluginRenderTemplateFile(pGeneration->pPackage, template_name, data, out_size, out_error);
}

xvalue PS_HostPluginOptionLoad(void* plugin_handle, const char* file_name)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);

	if ( (pGeneration == NULL) || (pGeneration->pPackage == NULL) || (file_name == NULL) || (file_name[0] == '\0') ) {
		return NULL;
	}
	return PS_PluginOptionLoadFile((const char*)PS_PackageKey(pGeneration->pPackage), file_name);
}

int PS_HostPluginOptionSave(void* plugin_handle, const char* file_name, xvalue values)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);

	if ( (pGeneration == NULL) || (pGeneration->pPackage == NULL) || (file_name == NULL) || (file_name[0] == '\0') || (values == NULL) ) {
		return -1;
	}
	return PS_PluginOptionSaveFile((const char*)PS_PackageKey(pGeneration->pPackage), file_name, values) ? 0 : -1;
}

const char* PS_HostPluginPrivateDbPath(void* plugin_handle)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);

	if ( (pGeneration == NULL) || (pGeneration->pPackage == NULL) ) {
		return NULL;
	}
	return (const char*)pGeneration->pPackage->sPrivateDbPath;
}

int PS_HostOpenPluginPrivateDb(void* plugin_handle, sqlite3** out_db)
{
	const char* sPath = PS_HostPluginPrivateDbPath(plugin_handle);

	if ( out_db ) {
		*out_db = NULL;
	}
	if ( (sPath == NULL) || (sPath[0] == '\0') || (out_db == NULL) ) {
		return -1;
	}
	return sqlite3_open_v2(sPath, out_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL) == SQLITE_OK ? 0 : -1;
}

int64 PS_HostSessionInt(xvalue session, const char* key, int key_len)
{
	if ( (session == NULL) || (xvoType(session) != XVO_DT_TABLE) || (key == NULL) ) {
		return 0;
	}
	return xvoTableGetInt(session, key, key_len);
}

char* PS_HostPluginResourcePath(void* plugin_handle, const char* resource_dir, const char* rel_path)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);

	if ( (pGeneration == NULL) || (pGeneration->pPackage == NULL) || !PS_ResourceIsSafeRelativePath(resource_dir) || !PS_ResourceIsSafeRelativePath(rel_path) ) {
		return NULL;
	}
	return PS_ResourceBuildPath(pGeneration->pPackage, resource_dir, rel_path);
}

static bool PS_HostAppendQueryText(char* sQuery, size_t iCap, size_t* pOffset, const char* sKey, const char* sValue)
{
	return xrtQueryAppendPair(sQuery, iCap, pOffset, sKey, sValue ? sValue : "");
}

char* PS_HostAttachmentUrl(const char* attachment_xid)
{
	char sQuery[256] = {0};
	size_t iOffset = 0;

	if ( (attachment_xid == NULL) || (attachment_xid[0] == '\0') ) {
		return NULL;
	}
	if ( !PS_HostAppendQueryText(sQuery, sizeof(sQuery), &iOffset, "xid", attachment_xid) ) {
		return NULL;
	}
	return xrtFormat("/attachment?%s", sQuery);
}

char* PS_HostAttachmentUploadUrl(void* plugin_handle, const char* model_name, int64 record_id)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);
	char sQuery[512] = {0};
	char sRecordID[48];
	size_t iOffset = 0;
	const char* sModelName = model_name;

	if ( (pGeneration == NULL) || (pGeneration->pPackage == NULL) ) {
		return NULL;
	}
	if ( (sModelName == NULL) || (sModelName[0] == '\0') ) {
		sModelName = (const char*)PS_PackageKey(pGeneration->pPackage);
	}
	if ( !PS_HostAppendQueryText(sQuery, sizeof(sQuery), &iOffset, "modelName", sModelName ? sModelName : "") ) {
		return NULL;
	}
	if ( record_id > 0 ) {
		snprintf(sRecordID, sizeof(sRecordID), "%lld", (long long)record_id);
		if ( !PS_HostAppendQueryText(sQuery, sizeof(sQuery), &iOffset, "recordId", sRecordID) ) {
			return NULL;
		}
	}
	return xrtFormat("/admin/view/attachment/upload?%s", sQuery);
}

char* PS_HostAttachmentListUrl(void* plugin_handle, const char* model_name)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);
	char sQuery[512] = {0};
	size_t iOffset = 0;
	const char* sModelName = model_name;

	if ( (pGeneration == NULL) || (pGeneration->pPackage == NULL) ) {
		return NULL;
	}
	if ( (sModelName == NULL) || (sModelName[0] == '\0') ) {
		sModelName = (const char*)PS_PackageKey(pGeneration->pPackage);
	}
	if ( !PS_HostAppendQueryText(sQuery, sizeof(sQuery), &iOffset, "modelName", sModelName ? sModelName : "") ) {
		return NULL;
	}
	return xrtFormat("/admin/view/attachment?%s", sQuery);
}

int PS_HostReplyJsonValue(XS_ResponseObject resp, int code, xvalue data)
{
	size_t iSize = 0;
	str sJson;

	if ( (resp == NULL) || (data == NULL) ) {
		return -1;
	}
	sJson = xrtStringifyJSON(data, FALSE, &iSize);
	if ( sJson == NULL ) {
		return -1;
	}
	xsHttpReplyAuto(resp, code, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	return 0;
}

int PS_HostFindMenuId(PluginSystemGeneration* pGeneration, const XAdminMenuDecl* decl)
{
	sqlite3_stmt* stmt = NULL;
	int iMenuId = 0;

	if ( (G_DB == NULL) || (pGeneration == NULL) || (pGeneration->pPackage == NULL) || (decl == NULL) ) {
		return 0;
	}

	if ( decl->href && decl->href[0] ) {
		if ( sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE plugin_xid = ? AND href = ? ORDER BY id DESC LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return 0;
		}
		PS_StorageBindText(stmt, 1, PS_HostGenerationXid(pGeneration));
		PS_StorageBindText(stmt, 2, decl->href);
	} else {
		if ( sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE plugin_xid = ? AND parent = ? AND title = ? AND type = ? ORDER BY id DESC LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return 0;
		}
		PS_StorageBindText(stmt, 1, PS_HostGenerationXid(pGeneration));
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

int PS_HostApplyMenuToken(PluginSystemMenuToken* pToken, bool bForce)
{
	XAdminMenuDecl decl;
	sqlite3_stmt* stmt = NULL;
	char sRef[32];
	int iMenuId = 0;
	int iParentId;
	int64 iNow;

	if ( (pToken == NULL) || (pToken->base.pGeneration == NULL) || (pToken->base.pGeneration->pPackage == NULL) ) {
		return -1;
	}
	if ( pToken->base.bPublished && !bForce ) {
		return 0;
	}

	iParentId = pToken->iParentId;
	if ( iParentId < 0 ) {
		iParentId = PS_HostResolveMenuId(pToken->base.pGeneration, iParentId);
		if ( iParentId <= 0 ) {
			return -1;
		}
	}

	memset(&decl, 0, sizeof(decl));
	decl.parent_id = iParentId;
	decl.title = pToken->sTitle;
	decl.icon = pToken->sIcon;
	decl.type = pToken->iType;
	decl.open_type = pToken->sOpenType;
	decl.href = pToken->sHref;
	decl.sort = pToken->iSort;
	decl.visible = pToken->bVisible;
	decl.remark = pToken->sRemark;

	iMenuId = PS_HostFindMenuId(pToken->base.pGeneration, &decl);
	iNow = xrtNow();
	if ( iMenuId > 0 ) {
		if ( sqlite3_prepare_v3(G_DB, "UPDATE menu SET parent = ?, title = ?, icon = ?, type = ?, openType = ?, href = ?, sort = ?, visible = ?, remark = ?, updateTime = ?, plugin_xid = ?, plugin_generation = ?, isDelete = 0 WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return -1;
		}
		sqlite3_bind_int(stmt, 1, iParentId);
		PS_StorageBindText(stmt, 2, pToken->sTitle);
		PS_StorageBindText(stmt, 3, pToken->sIcon);
		sqlite3_bind_int(stmt, 4, pToken->iType);
		PS_StorageBindText(stmt, 5, pToken->sOpenType ? pToken->sOpenType : (str)"_component");
		PS_StorageBindText(stmt, 6, pToken->sHref);
		sqlite3_bind_int(stmt, 7, pToken->iSort);
		sqlite3_bind_int(stmt, 8, pToken->bVisible ? 1 : 0);
		PS_StorageBindText(stmt, 9, pToken->sRemark);
		sqlite3_bind_int64(stmt, 10, iNow);
		PS_StorageBindText(stmt, 11, PS_HostGenerationXid(pToken->base.pGeneration));
		sqlite3_bind_int(stmt, 12, (int)pToken->base.pGeneration->iGeneration);
		sqlite3_bind_int(stmt, 13, iMenuId);
	} else {
		if ( sqlite3_prepare_v3(G_DB, "INSERT INTO menu (parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, plugin_xid, plugin_generation, isDelete) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return -1;
		}
		sqlite3_bind_int(stmt, 1, iParentId);
		PS_StorageBindText(stmt, 2, pToken->sTitle);
		PS_StorageBindText(stmt, 3, pToken->sIcon);
		sqlite3_bind_int(stmt, 4, pToken->iType);
		PS_StorageBindText(stmt, 5, pToken->sOpenType ? pToken->sOpenType : (str)"_component");
		PS_StorageBindText(stmt, 6, pToken->sHref);
		sqlite3_bind_int(stmt, 7, pToken->iSort);
		sqlite3_bind_int(stmt, 8, pToken->bVisible ? 1 : 0);
		PS_StorageBindText(stmt, 9, pToken->sRemark);
		sqlite3_bind_int64(stmt, 10, iNow);
		sqlite3_bind_int64(stmt, 11, iNow);
		PS_StorageBindText(stmt, 12, PS_HostGenerationXid(pToken->base.pGeneration));
		sqlite3_bind_int(stmt, 13, (int)pToken->base.pGeneration->iGeneration);
	}

	if ( sqlite3_step(stmt) != SQLITE_DONE ) {
		sqlite3_finalize(stmt);
		return -1;
	}
	if ( iMenuId <= 0 ) {
		iMenuId = (int)sqlite3_last_insert_rowid(G_DB);
	}
	sqlite3_finalize(stmt);

	pToken->iMenuId = iMenuId;
	PS_HostFormatRefInt(iMenuId, sRef);
	if ( pToken->base.iResourceId <= 0 ) {
		pToken->base.iResourceId = PS_StorageTrackResource(pToken->base.pGeneration, "generation", "menu", pToken->sKey ? pToken->sKey : ((pToken->sHref && pToken->sHref[0]) ? pToken->sHref : pToken->sTitle), sRef, "soft_delete");
	}
	pToken->base.bPublished = TRUE;
	return 0;
}

int PS_HostPublishMenuToken(PluginSystemMenuToken* pToken)
{
	return PS_HostApplyMenuToken(pToken, FALSE);
}

int PS_HostRegisterMenu(void* plugin_handle, const XAdminMenuDecl* decl, int* out_menu_id, XAdminMenuToken* token)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);
	PluginSystemMenuToken* pToken = NULL;

	if ( (decl == NULL) || (decl->title == NULL) || (decl->title[0] == '\0') || (pGeneration == NULL) || (pGeneration->pPackage == NULL) ) {
		return -1;
	}

	pToken = xrtMalloc(sizeof(PluginSystemMenuToken));
	if ( pToken == NULL ) {
		return -1;
	}
	memset(pToken, 0, sizeof(PluginSystemMenuToken));
	pToken->base.pGeneration = pGeneration;
	pToken->iTempMenuId = PS_HostAllocStagedId(pGeneration);
	pToken->sKey = PS_HostCopyOptionalText(decl->key);
	pToken->iParentId = decl->parent_id;
	pToken->sTitle = PS_HostCopyOptionalText(decl->title);
	pToken->sIcon = PS_HostCopyOptionalText(decl->icon);
	pToken->iType = decl->type;
	pToken->sOpenType = PS_HostCopyOptionalText(decl->open_type ? decl->open_type : "_component");
	pToken->sHref = PS_HostCopyOptionalText(decl->href);
	pToken->iSort = decl->sort;
	pToken->bVisible = decl->visible;
	pToken->sRemark = PS_HostCopyOptionalText(decl->remark);
	PS_HostAppendToken(pGeneration->lstMenuTokens, pToken);

	if ( out_menu_id ) {
		*out_menu_id = pToken->iTempMenuId;
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

	if ( pToken->base.bPublished && PS_HostMatchOwnedRow("SELECT plugin_xid, plugin_generation FROM menu WHERE id = ?", pToken->iMenuId, pToken->base.pGeneration) ) {
		if ( sqlite3_prepare_v3(G_DB, "UPDATE menu SET isDelete = 1, updateTime = ? WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, xrtNow());
			sqlite3_bind_int(stmt, 2, pToken->iMenuId);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}

	if ( pToken->base.iResourceId > 0 ) {
		PS_StorageUpdateResourceStatus(pToken->base.iResourceId, "removed");
	}
	PS_HostFreeMenuToken(pToken);
	return 0;
}

int PS_HostFindAuthGroupId(PluginSystemGeneration* pGeneration, int iScope, const char* sName)
{
	sqlite3_stmt* stmt = NULL;
	str sSQL = NULL;
	int iGroupId = 0;

	if ( (pGeneration == NULL) || (pGeneration->pPackage == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return 0;
	}

	sSQL = xrtFormat("SELECT id FROM %s WHERE plugin_xid = ? AND name = ? ORDER BY id DESC LIMIT 1", PS_HostAuthGroupTable(iScope));
	if ( (sSQL == NULL) || (sqlite3_prepare_v3(G_DB, sSQL, -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK) ) {
		if ( sSQL ) {
			xrtFree(sSQL);
		}
		return 0;
	}

	PS_StorageBindText(stmt, 1, PS_HostGenerationXid(pGeneration));
	PS_StorageBindText(stmt, 2, sName);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iGroupId = sqlite3_column_int(stmt, 0);
	}

	sqlite3_finalize(stmt);
	xrtFree(sSQL);
	return iGroupId;
}

int PS_HostApplyAuthGroupToken(PluginSystemAuthGroupToken* pToken, bool bForce)
{
	sqlite3_stmt* stmt = NULL;
	str sSQL = NULL;
	char sRef[32];
	int iGroupId = 0;
	int64 iNow;

	if ( (pToken == NULL) || (pToken->base.pGeneration == NULL) || (pToken->base.pGeneration->pPackage == NULL) ) {
		return -1;
	}
	if ( pToken->base.bPublished && !bForce ) {
		return 0;
	}

	iGroupId = PS_HostFindAuthGroupId(pToken->base.pGeneration, pToken->iScope, pToken->sName);
	iNow = xrtNow();
	if ( iGroupId > 0 ) {
		sSQL = xrtFormat("UPDATE %s SET name = ?, desc = ?, sort = ?, updateTime = ?, plugin_xid = ?, plugin_generation = ?, isDelete = 0 WHERE id = ?", PS_HostAuthGroupTable(pToken->iScope));
	} else {
		sSQL = xrtFormat("INSERT INTO %s (name, desc, sort, createTime, updateTime, plugin_xid, plugin_generation, isDelete) VALUES (?, ?, ?, ?, ?, ?, ?, 0)", PS_HostAuthGroupTable(pToken->iScope));
	}
	if ( (sSQL == NULL) || (sqlite3_prepare_v3(G_DB, sSQL, -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK) ) {
		if ( sSQL ) {
			xrtFree(sSQL);
		}
		return -1;
	}

	PS_StorageBindText(stmt, 1, pToken->sName);
	PS_StorageBindText(stmt, 2, pToken->sDescription);
	sqlite3_bind_int(stmt, 3, pToken->iSort);
	if ( iGroupId > 0 ) {
		sqlite3_bind_int64(stmt, 4, iNow);
		PS_StorageBindText(stmt, 5, PS_HostGenerationXid(pToken->base.pGeneration));
		sqlite3_bind_int(stmt, 6, (int)pToken->base.pGeneration->iGeneration);
		sqlite3_bind_int(stmt, 7, iGroupId);
	} else {
		sqlite3_bind_int64(stmt, 4, iNow);
		sqlite3_bind_int64(stmt, 5, iNow);
		PS_StorageBindText(stmt, 6, PS_HostGenerationXid(pToken->base.pGeneration));
		sqlite3_bind_int(stmt, 7, (int)pToken->base.pGeneration->iGeneration);
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

	pToken->iGroupId = iGroupId;
	PS_HostFormatRefInt(iGroupId, sRef);
	if ( pToken->base.iResourceId <= 0 ) {
		pToken->base.iResourceId = PS_StorageTrackResource(pToken->base.pGeneration, "generation", PS_HostAuthGroupResourceType(pToken->iScope), pToken->sKey ? pToken->sKey : pToken->sName, sRef, "soft_delete");
	}
	pToken->base.bPublished = TRUE;
	PS_HostRefreshCachesByScope(pToken->iScope);
	return 0;
}

int PS_HostPublishAuthGroupToken(PluginSystemAuthGroupToken* pToken)
{
	return PS_HostApplyAuthGroupToken(pToken, FALSE);
}

int PS_HostRegisterAuthGroup(void* plugin_handle, const XAdminAuthGroupDecl* decl, int* out_group_id, XAdminAuthGroupToken* token)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);
	PluginSystemAuthGroupToken* pToken = NULL;
	int iScope;

	if ( (decl == NULL) || (decl->name == NULL) || (decl->name[0] == '\0') || (pGeneration == NULL) || (pGeneration->pPackage == NULL) ) {
		return -1;
	}

	iScope = (decl->scope == XADMIN_AUTH_SCOPE_MEMBER) ? XADMIN_AUTH_SCOPE_MEMBER : XADMIN_AUTH_SCOPE_ADMIN;

	pToken = xrtMalloc(sizeof(PluginSystemAuthGroupToken));
	if ( pToken == NULL ) {
		return -1;
	}
	memset(pToken, 0, sizeof(PluginSystemAuthGroupToken));
	pToken->base.pGeneration = pGeneration;
	pToken->iScope = iScope;
	pToken->iTempGroupId = PS_HostAllocStagedId(pGeneration);
	pToken->sKey = PS_HostCopyOptionalText(decl->key);
	pToken->sName = PS_HostCopyOptionalText(decl->name);
	pToken->sDescription = PS_HostCopyOptionalText(decl->description);
	pToken->iSort = decl->sort;
	PS_HostAppendToken(pGeneration->lstAuthGroupTokens, pToken);

	if ( out_group_id ) {
		*out_group_id = pToken->iTempGroupId;
	}
	if ( token ) {
		*token = (XAdminAuthGroupToken)(uintptr_t)pToken;
	}

	return 0;
}

int PS_HostUnregisterAuthGroup(XAdminAuthGroupToken token)
{
	PluginSystemAuthGroupToken* pToken = (PluginSystemAuthGroupToken*)(uintptr_t)token;
	sqlite3_stmt* stmt = NULL;
	str sSQL = NULL;
	bool bWasPublished;
	int iScope;

	if ( pToken == NULL ) {
		return -1;
	}
	if ( pToken->base.bReleased ) {
		return 0;
	}

	iScope = pToken->iScope;
	bWasPublished = pToken->base.bPublished;
	pToken->base.bReleased = TRUE;
	if ( pToken->base.pGeneration ) {
		PS_HostDetachToken(pToken->base.pGeneration->lstAuthGroupTokens, pToken);
	}

	if ( bWasPublished ) {
		sSQL = xrtFormat("SELECT plugin_xid, plugin_generation FROM %s WHERE id = ?", PS_HostAuthGroupTable(pToken->iScope));
	}
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

	if ( pToken->base.iResourceId > 0 ) {
		PS_StorageUpdateResourceStatus(pToken->base.iResourceId, "removed");
	}
	PS_HostFreeAuthGroupToken(pToken);
	if ( bWasPublished ) {
		PS_HostRefreshCachesByScope(iScope);
	}
	return 0;
}

int PS_HostFindAuthId(PluginSystemGeneration* pGeneration, int iScope, const char* sName)
{
	sqlite3_stmt* stmt = NULL;
	str sSQL = NULL;
	int iAuthId = 0;

	if ( (pGeneration == NULL) || (pGeneration->pPackage == NULL) || (sName == NULL) || (sName[0] == '\0') ) {
		return 0;
	}

	sSQL = xrtFormat("SELECT id FROM %s WHERE plugin_xid = ? AND name = ? ORDER BY id DESC LIMIT 1", PS_HostAuthTable(iScope));
	if ( (sSQL == NULL) || (sqlite3_prepare_v3(G_DB, sSQL, -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK) ) {
		if ( sSQL ) {
			xrtFree(sSQL);
		}
		return 0;
	}

	PS_StorageBindText(stmt, 1, PS_HostGenerationXid(pGeneration));
	PS_StorageBindText(stmt, 2, sName);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iAuthId = sqlite3_column_int(stmt, 0);
	}

	sqlite3_finalize(stmt);
	xrtFree(sSQL);
	return iAuthId;
}

int PS_HostApplyAuthToken(PluginSystemAuthToken* pToken, bool bForce)
{
	sqlite3_stmt* stmt = NULL;
	str sSQL = NULL;
	char sRef[32];
	int iAuthId = 0;
	int iGroupId;
	int64 iNow;

	if ( (pToken == NULL) || (pToken->base.pGeneration == NULL) || (pToken->base.pGeneration->pPackage == NULL) ) {
		return -1;
	}
	if ( pToken->base.bPublished && !bForce ) {
		return 0;
	}

	iGroupId = pToken->iGroupId;
	if ( iGroupId < 0 ) {
		iGroupId = PS_HostResolveAuthGroupId(pToken->base.pGeneration, iGroupId);
		if ( iGroupId <= 0 ) {
			return -1;
		}
	}

	iAuthId = PS_HostFindAuthId(pToken->base.pGeneration, pToken->iScope, pToken->sName);
	iNow = xrtNow();
	if ( iAuthId > 0 ) {
		sSQL = xrtFormat("UPDATE %s SET groupID = ?, name = ?, desc = ?, sort = ?, updateTime = ?, plugin_xid = ?, plugin_generation = ?, isDelete = 0 WHERE id = ?", PS_HostAuthTable(pToken->iScope));
	} else {
		sSQL = xrtFormat("INSERT INTO %s (groupID, name, desc, sort, createTime, updateTime, plugin_xid, plugin_generation, isDelete) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0)", PS_HostAuthTable(pToken->iScope));
	}
	if ( (sSQL == NULL) || (sqlite3_prepare_v3(G_DB, sSQL, -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK) ) {
		if ( sSQL ) {
			xrtFree(sSQL);
		}
		return -1;
	}

	sqlite3_bind_int(stmt, 1, iGroupId);
	PS_StorageBindText(stmt, 2, pToken->sName);
	PS_StorageBindText(stmt, 3, pToken->sDescription);
	sqlite3_bind_int(stmt, 4, pToken->iSort);
	if ( iAuthId > 0 ) {
		sqlite3_bind_int64(stmt, 5, iNow);
		PS_StorageBindText(stmt, 6, PS_HostGenerationXid(pToken->base.pGeneration));
		sqlite3_bind_int(stmt, 7, (int)pToken->base.pGeneration->iGeneration);
		sqlite3_bind_int(stmt, 8, iAuthId);
	} else {
		sqlite3_bind_int64(stmt, 5, iNow);
		sqlite3_bind_int64(stmt, 6, iNow);
		PS_StorageBindText(stmt, 7, PS_HostGenerationXid(pToken->base.pGeneration));
		sqlite3_bind_int(stmt, 8, (int)pToken->base.pGeneration->iGeneration);
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

	pToken->iAuthId = iAuthId;
	if ( pToken->iScope == XADMIN_AUTH_SCOPE_ADMIN ) {
		XAdmin_GrantDefaultAdminRoleAuth(iAuthId);
	}
	PS_HostFormatRefInt(iAuthId, sRef);
	if ( pToken->base.iResourceId <= 0 ) {
		pToken->base.iResourceId = PS_StorageTrackResource(pToken->base.pGeneration, "generation", PS_HostAuthResourceType(pToken->iScope), pToken->sKey ? pToken->sKey : pToken->sName, sRef, "soft_delete");
	}
	pToken->base.bPublished = TRUE;
	PS_HostRefreshCachesByScope(pToken->iScope);
	return 0;
}

int PS_HostPublishAuthToken(PluginSystemAuthToken* pToken)
{
	return PS_HostApplyAuthToken(pToken, FALSE);
}

int PS_HostRegisterAuth(void* plugin_handle, const XAdminAuthDecl* decl, int* out_auth_id, XAdminAuthToken* token)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);
	PluginSystemAuthToken* pToken = NULL;
	int iScope;

	if ( (decl == NULL) || (decl->name == NULL) || (decl->name[0] == '\0') || (pGeneration == NULL) || (pGeneration->pPackage == NULL) || (decl->group_id == 0) ) {
		return -1;
	}

	iScope = (decl->scope == XADMIN_AUTH_SCOPE_MEMBER) ? XADMIN_AUTH_SCOPE_MEMBER : XADMIN_AUTH_SCOPE_ADMIN;

	pToken = xrtMalloc(sizeof(PluginSystemAuthToken));
	if ( pToken == NULL ) {
		return -1;
	}
	memset(pToken, 0, sizeof(PluginSystemAuthToken));
	pToken->base.pGeneration = pGeneration;
	pToken->iScope = iScope;
	pToken->iTempAuthId = PS_HostAllocStagedId(pGeneration);
	pToken->sKey = PS_HostCopyOptionalText(decl->key);
	pToken->iGroupId = decl->group_id;
	pToken->sName = PS_HostCopyOptionalText(decl->name);
	pToken->sDescription = PS_HostCopyOptionalText(decl->description);
	pToken->iSort = decl->sort;
	PS_HostAppendToken(pGeneration->lstAuthTokens, pToken);

	if ( out_auth_id ) {
		*out_auth_id = pToken->iTempAuthId;
	}
	if ( token ) {
		*token = (XAdminAuthToken)(uintptr_t)pToken;
	}

	return 0;
}

int PS_HostUnregisterAuth(XAdminAuthToken token)
{
	PluginSystemAuthToken* pToken = (PluginSystemAuthToken*)(uintptr_t)token;
	sqlite3_stmt* stmt = NULL;
	str sSQL = NULL;
	bool bWasPublished;
	int iScope;

	if ( pToken == NULL ) {
		return -1;
	}
	if ( pToken->base.bReleased ) {
		return 0;
	}

	iScope = pToken->iScope;
	bWasPublished = pToken->base.bPublished;
	pToken->base.bReleased = TRUE;
	if ( pToken->base.pGeneration ) {
		PS_HostDetachToken(pToken->base.pGeneration->lstAuthTokens, pToken);
	}

	if ( bWasPublished ) {
		sSQL = xrtFormat("SELECT plugin_xid, plugin_generation FROM %s WHERE id = ?", PS_HostAuthTable(pToken->iScope));
	}
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

	if ( pToken->base.iResourceId > 0 ) {
		PS_StorageUpdateResourceStatus(pToken->base.iResourceId, "removed");
	}
	PS_HostFreeAuthToken(pToken);
	if ( bWasPublished ) {
		PS_HostRefreshCachesByScope(iScope);
	}
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

int PS_HostApplyUriAuthToken(PluginSystemUriAuthToken* pToken, bool bForce)
{
	sqlite3_stmt* stmt = NULL;
	RouteInfo* pInfo;
	char sRef[32];
	int iUriId;
	int iAuthId;
	int64 iNow;

	if ( (pToken == NULL) || (pToken->base.pGeneration == NULL) || (pToken->base.pGeneration->pPackage == NULL) || (pToken->sUri == NULL) || (pToken->sUri[0] == '\0') ) {
		return -1;
	}
	if ( pToken->base.bPublished && !bForce ) {
		return 0;
	}

	pInfo = (RouteInfo*)xrtDictGet(G_StaticRouteTableHTTP, pToken->sUri, strlen(pToken->sUri));
	if ( pInfo == NULL ) {
		return -1;
	}

	iAuthId = pToken->iAuthId;
	if ( iAuthId < 0 ) {
		iAuthId = PS_HostResolveAuthId(pToken->base.pGeneration, iAuthId);
		if ( iAuthId <= 0 ) {
			return -1;
		}
	}

	iUriId = pToken->iUriId;
	if ( (iUriId <= 0) || !PS_HostMatchOwnedRow("SELECT plugin_xid, plugin_generation FROM uris WHERE id = ?", iUriId, pToken->base.pGeneration) ) {
		iUriId = PS_HostFindUriId(pToken->sUri);
	}

	iNow = xrtNow();
	if ( iUriId > 0 ) {
		if ( sqlite3_prepare_v3(G_DB, "UPDATE uris SET authID = ?, uri = ?, desc = ?, isBackend = ?, needAuth = ?, needLog = ?, keepActive = ?, sort = ?, updateTime = ?, plugin_xid = ?, plugin_generation = ? WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return -1;
		}
		sqlite3_bind_int(stmt, 1, iAuthId);
		PS_StorageBindText(stmt, 2, pToken->sUri);
		PS_StorageBindText(stmt, 3, pToken->sDescription);
		sqlite3_bind_int(stmt, 4, (pToken->iScope == XADMIN_AUTH_SCOPE_MEMBER) ? 0 : 1);
		sqlite3_bind_int(stmt, 5, pToken->bNeedAuth ? 1 : 0);
		sqlite3_bind_int(stmt, 6, pToken->bNeedLog ? 1 : 0);
		sqlite3_bind_int(stmt, 7, pToken->bKeepActive ? 1 : 0);
		sqlite3_bind_int(stmt, 8, pToken->iSort);
		sqlite3_bind_int64(stmt, 9, iNow);
		PS_StorageBindText(stmt, 10, PS_HostGenerationXid(pToken->base.pGeneration));
		sqlite3_bind_int(stmt, 11, (int)pToken->base.pGeneration->iGeneration);
		sqlite3_bind_int(stmt, 12, iUriId);
	} else {
		if ( sqlite3_prepare_v3(G_DB, "INSERT INTO uris (authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime, plugin_xid, plugin_generation) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return -1;
		}
		sqlite3_bind_int(stmt, 1, iAuthId);
		PS_StorageBindText(stmt, 2, pToken->sUri);
		PS_StorageBindText(stmt, 3, pToken->sDescription);
		sqlite3_bind_int(stmt, 4, (pToken->iScope == XADMIN_AUTH_SCOPE_MEMBER) ? 0 : 1);
		sqlite3_bind_int(stmt, 5, pToken->bNeedAuth ? 1 : 0);
		sqlite3_bind_int(stmt, 6, pToken->bNeedLog ? 1 : 0);
		sqlite3_bind_int(stmt, 7, pToken->bKeepActive ? 1 : 0);
		sqlite3_bind_int(stmt, 8, pToken->iSort);
		sqlite3_bind_int64(stmt, 9, iNow);
		sqlite3_bind_int64(stmt, 10, iNow);
		PS_StorageBindText(stmt, 11, PS_HostGenerationXid(pToken->base.pGeneration));
		sqlite3_bind_int(stmt, 12, (int)pToken->base.pGeneration->iGeneration);
	}

	if ( sqlite3_step(stmt) != SQLITE_DONE ) {
		sqlite3_finalize(stmt);
		return -1;
	}
	if ( iUriId <= 0 ) {
		iUriId = (int)sqlite3_last_insert_rowid(G_DB);
	}
	sqlite3_finalize(stmt);

	pInfo->AuthID = iAuthId;
	pInfo->bAdmin = (pToken->iScope == XADMIN_AUTH_SCOPE_MEMBER) ? FALSE : TRUE;
	pInfo->bAuth = pToken->bNeedAuth;
	pInfo->bPutLog = pToken->bNeedLog;
	pInfo->bActive = pToken->bKeepActive;

	pToken->iUriId = iUriId;
	pToken->iAuthId = iAuthId;
	PS_HostFormatRefInt(iUriId, sRef);
	if ( pToken->base.iResourceId <= 0 ) {
		pToken->base.iResourceId = PS_StorageTrackResource(pToken->base.pGeneration, "generation", PS_HostUriAuthResourceType(pToken->iScope), pToken->sKey ? pToken->sKey : pToken->sUri, sRef, "delete");
	}
	pToken->base.bPublished = TRUE;
	PS_HostRefreshCachesByScope(pToken->iScope);
	return 0;
}

int PS_HostPublishUriAuthToken(PluginSystemUriAuthToken* pToken)
{
	return PS_HostApplyUriAuthToken(pToken, FALSE);
}

int PS_HostRegisterUriAuth(void* plugin_handle, const XAdminUriAuthDecl* decl, int* out_uri_id, XAdminUriAuthToken* token)
{
	PluginSystemGeneration* pGeneration = PS_HostGetGeneration(plugin_handle);
	PluginSystemUriAuthToken* pToken = NULL;
	int iScope;

	if ( (decl == NULL) || (decl->uri == NULL) || (decl->uri[0] == '\0') || (decl->auth_id == 0) || (pGeneration == NULL) || (pGeneration->pPackage == NULL) ) {
		return -1;
	}

	iScope = (decl->scope == XADMIN_AUTH_SCOPE_MEMBER) ? XADMIN_AUTH_SCOPE_MEMBER : XADMIN_AUTH_SCOPE_ADMIN;
	pToken = xrtMalloc(sizeof(PluginSystemUriAuthToken));
	if ( pToken == NULL ) {
		return -1;
	}

	memset(pToken, 0, sizeof(PluginSystemUriAuthToken));
	pToken->base.pGeneration = pGeneration;
	pToken->iScope = iScope;
	pToken->iTempUriId = PS_HostAllocStagedId(pGeneration);
	pToken->iAuthId = decl->auth_id;
	pToken->sKey = PS_HostCopyOptionalText(decl->key);
	pToken->sUri = PS_HostCopyOptionalText(decl->uri);
	pToken->sDescription = PS_HostCopyOptionalText(decl->description);
	pToken->iSort = decl->sort;
	pToken->bNeedAuth = decl->need_auth;
	pToken->bNeedLog = decl->need_log;
	pToken->bKeepActive = decl->keep_active;
	if ( pToken->sUri == NULL ) {
		PS_HostFreeUriAuthToken(pToken);
		return -1;
	}

	PS_HostAppendToken(pGeneration->lstUriAuthTokens, pToken);
	if ( out_uri_id ) {
		*out_uri_id = pToken->iTempUriId;
	}
	if ( token ) {
		*token = (XAdminUriAuthToken)(uintptr_t)pToken;
	}
	return 0;
}

int PS_HostUnregisterUriAuth(XAdminUriAuthToken token)
{
	PluginSystemUriAuthToken* pToken = (PluginSystemUriAuthToken*)(uintptr_t)token;
	sqlite3_stmt* stmt = NULL;
	PluginSystemRouteToken* pRouteToken = NULL;
	RouteInfo* pInfo;
	bool bWasPublished;
	int iScope;

	if ( pToken == NULL ) {
		return -1;
	}
	if ( pToken->base.bReleased ) {
		return 0;
	}

	iScope = pToken->iScope;
	bWasPublished = pToken->base.bPublished;
	pToken->base.bReleased = TRUE;
	if ( pToken->base.pGeneration ) {
		PS_HostDetachToken(pToken->base.pGeneration->lstUriAuthTokens, pToken);
	}

	if ( bWasPublished && PS_HostMatchOwnedRow("SELECT plugin_xid, plugin_generation FROM uris WHERE id = ?", pToken->iUriId, pToken->base.pGeneration) ) {
		if ( sqlite3_prepare_v3(G_DB, "DELETE FROM uris WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int(stmt, 1, pToken->iUriId);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}

		if ( pToken->sUri ) {
			pInfo = (RouteInfo*)xrtDictGet(G_StaticRouteTableHTTP, pToken->sUri, strlen(pToken->sUri));
			if ( pInfo && pInfo->pPluginRouteToken ) {
				pRouteToken = (PluginSystemRouteToken*)pInfo->pPluginRouteToken;
			}
			if ( pInfo && ((pRouteToken == NULL) || (pRouteToken->base.pGeneration == pToken->base.pGeneration)) ) {
				pInfo->AuthID = 0;
				pInfo->bAuth = FALSE;
				pInfo->bPutLog = FALSE;
				pInfo->bActive = FALSE;
			}
		}
	}

	if ( pToken->base.iResourceId > 0 ) {
		PS_StorageUpdateResourceStatus(pToken->base.iResourceId, "removed");
	}
	PS_HostFreeUriAuthToken(pToken);
	if ( bWasPublished ) {
		PS_HostRefreshCachesByScope(iScope);
	}
	return 0;
}

bool PS_HostApplyMenuGenerationTokens(PluginSystemGeneration* pGeneration, bool bForce)
{
	bool bProgress;

	if ( (pGeneration == NULL) || (pGeneration->lstMenuTokens == NULL) ) {
		return TRUE;
	}

	if ( bForce ) {
		for ( int i = 0; i < xrtListCount(pGeneration->lstMenuTokens); i++ ) {
			PluginSystemMenuToken* pToken = xrtListGetPtr(pGeneration->lstMenuTokens, i);
			if ( pToken && !pToken->base.bReleased && PS_HostApplyMenuToken(pToken, TRUE) != 0 ) {
				return FALSE;
			}
		}
		return TRUE;
	}

	do {
		bProgress = FALSE;
		for ( int i = 0; i < xrtListCount(pGeneration->lstMenuTokens); i++ ) {
			PluginSystemMenuToken* pToken = xrtListGetPtr(pGeneration->lstMenuTokens, i);
			if ( (pToken == NULL) || pToken->base.bReleased ) {
				continue;
			}
			if ( pToken->base.bPublished ) {
				continue;
			}
			if ( PS_HostApplyMenuToken(pToken, FALSE) == 0 ) {
				bProgress = TRUE;
			}
		}
	} while ( bProgress );

	for ( int i = 0; i < xrtListCount(pGeneration->lstMenuTokens); i++ ) {
		PluginSystemMenuToken* pToken = xrtListGetPtr(pGeneration->lstMenuTokens, i);
		if ( (pToken != NULL) && !pToken->base.bReleased && !pToken->base.bPublished ) {
			return FALSE;
		}
	}
	return TRUE;
}

void PS_HostCleanupObsoleteMenuResources(PluginSystemGeneration* pGeneration)
{
	sqlite3_stmt* stmt = NULL;
	sqlite3_stmt* stmtMenu = NULL;
	sqlite3_stmt* stmtResource = NULL;
	const char* sXid;

	if ( (G_DB == NULL) || (pGeneration == NULL) || (pGeneration->pPackage == NULL) ) {
		return;
	}
	sXid = PS_HostGenerationXid(pGeneration);
	if ( (sXid == NULL) || (sXid[0] == '\0') ) {
		return;
	}
	if ( sqlite3_prepare_v3(
		G_DB,
		"SELECT id, resource_ref FROM plugin_resource WHERE xid = ? AND resource_type = 'menu' AND status = 'active' AND generation <> ?",
		-1,
		SQL_PREPARE_DEFAULT,
		&stmt,
		NULL) != SQLITE_OK ) {
		return;
	}
	PS_StorageBindText(stmt, 1, (str)sXid);
	sqlite3_bind_int(stmt, 2, (int)pGeneration->iGeneration);
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		int iResourceId = sqlite3_column_int(stmt, 0);
		const char* sRef = (const char*)sqlite3_column_text(stmt, 1);
		int iMenuId = sRef ? atoi(sRef) : 0;
		if ( iMenuId > 0 ) {
			if ( sqlite3_prepare_v3(G_DB, "UPDATE menu SET isDelete = 1, updateTime = ? WHERE id = ? AND plugin_xid = ?", -1, SQL_PREPARE_DEFAULT, &stmtMenu, NULL) == SQLITE_OK ) {
				sqlite3_bind_int64(stmtMenu, 1, xrtNow());
				sqlite3_bind_int(stmtMenu, 2, iMenuId);
				PS_StorageBindText(stmtMenu, 3, (str)sXid);
				sqlite3_step(stmtMenu);
				sqlite3_finalize(stmtMenu);
				stmtMenu = NULL;
			}
		}
		if ( sqlite3_prepare_v3(G_DB, "UPDATE plugin_resource SET status = 'removed' WHERE id = ?", -1, SQL_PREPARE_DEFAULT, &stmtResource, NULL) == SQLITE_OK ) {
			sqlite3_bind_int(stmtResource, 1, iResourceId);
			sqlite3_step(stmtResource);
			sqlite3_finalize(stmtResource);
			stmtResource = NULL;
		}
	}
	sqlite3_finalize(stmt);
}

bool PS_HostApplyGenerationEntryPoints(PluginSystemGeneration* pGeneration, bool bForce)
{
	bool bMenuOK;
	if ( pGeneration == NULL ) {
		return FALSE;
	}

	if ( pGeneration->lstAuthGroupTokens ) {
		for ( int i = 0; i < xrtListCount(pGeneration->lstAuthGroupTokens); i++ ) {
			PluginSystemAuthGroupToken* pToken = xrtListGetPtr(pGeneration->lstAuthGroupTokens, i);
			if ( pToken && !pToken->base.bReleased && PS_HostApplyAuthGroupToken(pToken, bForce) != 0 ) {
				return FALSE;
			}
		}
	}

	if ( pGeneration->lstAuthTokens ) {
		for ( int i = 0; i < xrtListCount(pGeneration->lstAuthTokens); i++ ) {
			PluginSystemAuthToken* pToken = xrtListGetPtr(pGeneration->lstAuthTokens, i);
			if ( pToken && !pToken->base.bReleased && PS_HostApplyAuthToken(pToken, bForce) != 0 ) {
				return FALSE;
			}
		}
	}

	if ( pGeneration->lstRouteTokens ) {
		for ( int i = 0; i < xrtListCount(pGeneration->lstRouteTokens); i++ ) {
			PluginSystemRouteToken* pToken = xrtListGetPtr(pGeneration->lstRouteTokens, i);
			if ( pToken && !pToken->base.bReleased && PS_HostApplyRouteToken(pToken, bForce) != 0 ) {
				return FALSE;
			}
		}
	}

	if ( pGeneration->lstUriAuthTokens ) {
		for ( int i = 0; i < xrtListCount(pGeneration->lstUriAuthTokens); i++ ) {
			PluginSystemUriAuthToken* pToken = xrtListGetPtr(pGeneration->lstUriAuthTokens, i);
			if ( pToken && !pToken->base.bReleased && PS_HostApplyUriAuthToken(pToken, bForce) != 0 ) {
				return FALSE;
			}
		}
	}

	bMenuOK = PS_HostApplyMenuGenerationTokens(pGeneration, bForce);
	if ( bMenuOK && !bForce ) {
		PS_HostCleanupObsoleteMenuResources(pGeneration);
	}
	return bMenuOK;
}

bool PS_HostPublishGenerationEntryPoints(PluginSystemGeneration* pGeneration)
{
	return PS_HostApplyGenerationEntryPoints(pGeneration, FALSE);
}

bool PS_HostRestoreGenerationEntryPoints(PluginSystemGeneration* pGeneration)
{
	return PS_HostApplyGenerationEntryPoints(pGeneration, TRUE);
}

static bool PS_HostRoleAuthListContains(xvalue arrAuth, int64 iAuthId)
{
	if ( (arrAuth == NULL) || (xvoType(arrAuth) != XVO_DT_ARRAY) || (iAuthId <= 0) ) {
		return FALSE;
	}
	for ( uint32 i = 0; i < xvoArrayItemCount(arrAuth); i++ ) {
		if ( xvoArrayGetInt(arrAuth, i) == iAuthId ) {
			return TRUE;
		}
	}
	return FALSE;
}

int XAdmin_GrantDefaultAdminRoleAuth(int auth_id)
{
	sqlite3_stmt* stmt = NULL;
	xvalue arrAuth = NULL;
	str sAuthList = NULL;
	str sNextAuthList = NULL;
	int iRet = -1;

	if ( (G_DB == NULL) || (auth_id <= 0) ) {
		return -1;
	}

	if ( sqlite3_prepare_v3(G_DB, "SELECT authList FROM role WHERE id = 1 AND isDelete = 0 LIMIT 1;", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return -1;
	}
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		const unsigned char* sText = sqlite3_column_text(stmt, 0);
		if ( sText && sText[0] ) {
			sAuthList = xrtCopyStr((str)sText, 0);
		}
	}
	sqlite3_finalize(stmt);
	stmt = NULL;

	if ( sAuthList && (strlen(sAuthList) > 0) ) {
		arrAuth = xrtParseJSON(sAuthList, 0);
	}
	if ( (arrAuth == NULL) || (xvoType(arrAuth) != XVO_DT_ARRAY) ) {
		if ( arrAuth ) {
			xvoUnref(arrAuth);
		}
		arrAuth = xvoCreateArray();
	}
	if ( arrAuth == NULL ) {
		if ( sAuthList ) {
			xrtFree(sAuthList);
		}
		return -1;
	}
	if ( PS_HostRoleAuthListContains(arrAuth, auth_id) ) {
		iRet = 0;
		goto cleanup;
	}

	xvoArrayAppendInt(arrAuth, auth_id);
	sNextAuthList = xrtStringifyJSON(arrAuth, FALSE, NULL);
	if ( sNextAuthList == NULL ) {
		goto cleanup;
	}
	if ( sqlite3_prepare_v3(G_DB, "UPDATE role SET authList = ?, updateTime = ? WHERE id = 1 AND isDelete = 0;", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		goto cleanup;
	}
	PS_StorageBindText(stmt, 1, sNextAuthList);
	sqlite3_bind_int64(stmt, 2, xrtNow());
	if ( sqlite3_step(stmt) == SQLITE_DONE ) {
		iRet = 0;
		Auth_ReloadCache();
	}

cleanup:
	if ( stmt ) {
		sqlite3_finalize(stmt);
	}
	if ( sNextAuthList ) {
		xrtFree(sNextAuthList);
	}
	if ( sAuthList ) {
		xrtFree(sAuthList);
	}
	if ( arrAuth ) {
		xvoUnref(arrAuth);
	}
	return iRet;
}

int XAdmin_RegisterRoute(XAdminPluginHandle plugin_handle, const XAdminRouteDecl* decl, XAdminRouteToken* token)
{
	return PS_HostRegisterRoute(plugin_handle, decl, token);
}

int XAdmin_UnregisterRoute(XAdminRouteToken token)
{
	return PS_HostUnregisterRoute(token);
}

int XAdmin_RegisterMenu(XAdminPluginHandle plugin_handle, const XAdminMenuDecl* decl, int* out_menu_id, XAdminMenuToken* token)
{
	return PS_HostRegisterMenu(plugin_handle, decl, out_menu_id, token);
}

int XAdmin_UnregisterMenu(XAdminMenuToken token)
{
	return PS_HostUnregisterMenu(token);
}

int XAdmin_RegisterAuthGroup(XAdminPluginHandle plugin_handle, const XAdminAuthGroupDecl* decl, int* out_group_id, XAdminAuthGroupToken* token)
{
	return PS_HostRegisterAuthGroup(plugin_handle, decl, out_group_id, token);
}

int XAdmin_UnregisterAuthGroup(XAdminAuthGroupToken token)
{
	return PS_HostUnregisterAuthGroup(token);
}

int XAdmin_RegisterAuth(XAdminPluginHandle plugin_handle, const XAdminAuthDecl* decl, int* out_auth_id, XAdminAuthToken* token)
{
	return PS_HostRegisterAuth(plugin_handle, decl, out_auth_id, token);
}

int XAdmin_UnregisterAuth(XAdminAuthToken token)
{
	return PS_HostUnregisterAuth(token);
}

int XAdmin_RegisterUriAuth(XAdminPluginHandle plugin_handle, const XAdminUriAuthDecl* decl, int* out_uri_id, XAdminUriAuthToken* token)
{
	return PS_HostRegisterUriAuth(plugin_handle, decl, out_uri_id, token);
}

int XAdmin_UnregisterUriAuth(XAdminUriAuthToken token)
{
	return PS_HostUnregisterUriAuth(token);
}

int XAdmin_ListenEvent(XAdminPluginHandle plugin_handle, const XAdminEventDecl* decl, XAdminEventToken* token)
{
	return PS_HostListenEvent(plugin_handle, decl, token);
}

int XAdmin_UnlistenEvent(XAdminEventToken token)
{
	return PS_HostUnlistenEvent(token);
}

int XAdmin_EmitEvent(XAdminPluginHandle plugin_handle, const char* event_name, void* payload, size_t payload_size)
{
	return PS_HostEmitEvent(plugin_handle, event_name, payload, payload_size);
}

int XAdmin_RegisterHook(XAdminPluginHandle plugin_handle, const XAdminHookDecl* decl, XAdminHookToken* token)
{
	return PS_HostRegisterHook(plugin_handle, decl, token);
}

int XAdmin_UnregisterHook(XAdminHookToken token)
{
	return PS_HostUnregisterHook(token);
}

int XAdmin_InvokeHook(XAdminPluginHandle plugin_handle, const char* hook_name, void* payload, size_t payload_size)
{
	return PS_HostInvokeHook(plugin_handle, hook_name, payload, payload_size);
}

int XAdmin_RegisterService(XAdminPluginHandle plugin_handle, const XAdminServiceDecl* decl, const void* vtable)
{
	return PS_HostRegisterService(plugin_handle, decl, vtable);
}

int XAdmin_AcquireService(XAdminPluginHandle plugin_handle, const char* name, int major, XAdminServiceLease* out_lease, const void** out_vtable)
{
	return PS_HostAcquireService(plugin_handle, name, major, out_lease, out_vtable);
}

int XAdmin_ReleaseService(XAdminServiceLease lease)
{
	return PS_HostReleaseService(lease);
}

int XAdmin_GeneratePlugin(XAdminPluginHandle plugin_handle, const XAdminGeneratedPluginSpec* spec)
{
	return PS_HostGeneratePlugin(plugin_handle, spec);
}

int XAdmin_ReloadPlugin(XAdminPluginHandle plugin_handle, const char* xid)
{
	return PS_HostReloadPlugin(plugin_handle, xid);
}

int XAdmin_SetPluginEnabled(XAdminPluginHandle plugin_handle, const char* xid, int enabled)
{
	return PS_HostSetPluginEnabled(plugin_handle, xid, enabled);
}

int XAdmin_LoadPluginPage(XAdminPluginHandle plugin_handle, XS_ResponseObject resp, int code, const char* header, const char* page)
{
	return PS_HostLoadPluginPage(plugin_handle, resp, code, header, page);
}

char* XAdmin_RenderPluginTemplate(XAdminPluginHandle plugin_handle, const char* template_name, xvalue data, size_t* out_size, char** out_error)
{
	return PS_HostRenderPluginTemplate(plugin_handle, template_name, data, out_size, out_error);
}

xvalue XAdmin_PluginOptionLoad(XAdminPluginHandle plugin_handle, const char* file_name)
{
	return PS_HostPluginOptionLoad(plugin_handle, file_name);
}

int XAdmin_PluginOptionSave(XAdminPluginHandle plugin_handle, const char* file_name, xvalue values)
{
	return PS_HostPluginOptionSave(plugin_handle, file_name, values);
}

void XAdmin_Log(XAdminPluginHandle plugin_handle, int level, const char* message)
{
	PS_HostLog(level, "plugin=%s %s", PS_HostGetActorXid(plugin_handle), message ? message : "");
}

int XAdmin_ReplyJson(XS_ResponseObject resp, int code, xvalue data)
{
	return PS_HostReplyJsonValue(resp, code, data);
}

const char* XAdmin_PluginPrivateDbPath(XAdminPluginHandle plugin_handle)
{
	return PS_HostPluginPrivateDbPath(plugin_handle);
}

int XAdmin_OpenPluginPrivateDb(XAdminPluginHandle plugin_handle, sqlite3** out_db)
{
	return PS_HostOpenPluginPrivateDb(plugin_handle, out_db);
}

int64 XAdmin_SessionAdminId(xvalue session)
{
	return PS_HostSessionInt(session, "id", 2);
}

int64 XAdmin_SessionAdminRoleId(xvalue session)
{
	return PS_HostSessionInt(session, "roleID", 6);
}

char* XAdmin_PluginResourcePath(XAdminPluginHandle plugin_handle, const char* resource_dir, const char* rel_path)
{
	return PS_HostPluginResourcePath(plugin_handle, resource_dir, rel_path);
}

char* XAdmin_AttachmentUrl(const char* attachment_xid)
{
	return PS_HostAttachmentUrl(attachment_xid);
}

char* XAdmin_AttachmentUploadUrl(XAdminPluginHandle plugin_handle, const char* model_name, int64 record_id)
{
	return PS_HostAttachmentUploadUrl(plugin_handle, model_name, record_id);
}

char* XAdmin_AttachmentListUrl(XAdminPluginHandle plugin_handle, const char* model_name)
{
	return PS_HostAttachmentListUrl(plugin_handle, model_name);
}

void XAdmin_Free(void* ptr)
{
	PS_HostFree(ptr);
}

void PS_TCCRegisterPluginSdkSymbols(TCCState* pTcc)
{
	if ( pTcc == NULL ) {
		return;
	}

	tcc_add_symbol(pTcc, "XAdmin_RegisterRoute", XAdmin_RegisterRoute);
	tcc_add_symbol(pTcc, "XAdmin_UnregisterRoute", XAdmin_UnregisterRoute);
	tcc_add_symbol(pTcc, "xsHttpReplyFormat", xsHttpReplyFormat);
	tcc_add_symbol(pTcc, "LoadPage", LoadPage);
	tcc_add_symbol(pTcc, "XAdmin_RegisterMenu", XAdmin_RegisterMenu);
	tcc_add_symbol(pTcc, "XAdmin_UnregisterMenu", XAdmin_UnregisterMenu);
	tcc_add_symbol(pTcc, "XAdmin_RegisterAuthGroup", XAdmin_RegisterAuthGroup);
	tcc_add_symbol(pTcc, "XAdmin_UnregisterAuthGroup", XAdmin_UnregisterAuthGroup);
	tcc_add_symbol(pTcc, "XAdmin_RegisterAuth", XAdmin_RegisterAuth);
	tcc_add_symbol(pTcc, "XAdmin_UnregisterAuth", XAdmin_UnregisterAuth);
	tcc_add_symbol(pTcc, "XAdmin_GrantDefaultAdminRoleAuth", XAdmin_GrantDefaultAdminRoleAuth);
	tcc_add_symbol(pTcc, "XAdmin_RegisterUriAuth", XAdmin_RegisterUriAuth);
	tcc_add_symbol(pTcc, "XAdmin_UnregisterUriAuth", XAdmin_UnregisterUriAuth);
	tcc_add_symbol(pTcc, "XAdmin_ListenEvent", XAdmin_ListenEvent);
	tcc_add_symbol(pTcc, "XAdmin_UnlistenEvent", XAdmin_UnlistenEvent);
	tcc_add_symbol(pTcc, "XAdmin_EmitEvent", XAdmin_EmitEvent);
	tcc_add_symbol(pTcc, "XAdmin_RegisterHook", XAdmin_RegisterHook);
	tcc_add_symbol(pTcc, "XAdmin_UnregisterHook", XAdmin_UnregisterHook);
	tcc_add_symbol(pTcc, "XAdmin_InvokeHook", XAdmin_InvokeHook);
	tcc_add_symbol(pTcc, "XAdmin_RegisterService", XAdmin_RegisterService);
	tcc_add_symbol(pTcc, "XAdmin_AcquireService", XAdmin_AcquireService);
	tcc_add_symbol(pTcc, "XAdmin_ReleaseService", XAdmin_ReleaseService);
	tcc_add_symbol(pTcc, "XAdmin_GeneratePlugin", XAdmin_GeneratePlugin);
	tcc_add_symbol(pTcc, "XAdmin_ReloadPlugin", XAdmin_ReloadPlugin);
	tcc_add_symbol(pTcc, "XAdmin_SetPluginEnabled", XAdmin_SetPluginEnabled);
	tcc_add_symbol(pTcc, "XAdmin_LoadPluginPage", XAdmin_LoadPluginPage);
	tcc_add_symbol(pTcc, "XAdmin_RenderPluginTemplate", XAdmin_RenderPluginTemplate);
	tcc_add_symbol(pTcc, "XAdmin_PluginOptionLoad", XAdmin_PluginOptionLoad);
	tcc_add_symbol(pTcc, "XAdmin_PluginOptionSave", XAdmin_PluginOptionSave);
	tcc_add_symbol(pTcc, "XAdmin_Log", XAdmin_Log);
	tcc_add_symbol(pTcc, "XAdmin_ReplyJson", XAdmin_ReplyJson);
	tcc_add_symbol(pTcc, "XAdmin_PluginPrivateDbPath", XAdmin_PluginPrivateDbPath);
	tcc_add_symbol(pTcc, "XAdmin_OpenPluginPrivateDb", XAdmin_OpenPluginPrivateDb);
	tcc_add_symbol(pTcc, "XAdmin_SessionAdminId", XAdmin_SessionAdminId);
	tcc_add_symbol(pTcc, "XAdmin_SessionAdminRoleId", XAdmin_SessionAdminRoleId);
	tcc_add_symbol(pTcc, "XAdmin_PluginResourcePath", XAdmin_PluginResourcePath);
	tcc_add_symbol(pTcc, "XAdmin_AttachmentUrl", XAdmin_AttachmentUrl);
	tcc_add_symbol(pTcc, "XAdmin_AttachmentUploadUrl", XAdmin_AttachmentUploadUrl);
	tcc_add_symbol(pTcc, "XAdmin_AttachmentListUrl", XAdmin_AttachmentListUrl);
	tcc_add_symbol(pTcc, "XAdmin_Free", XAdmin_Free);
}

#endif
