#ifndef XADMIN_PLUGIN_SYSTEM_TYPES_H
#define XADMIN_PLUGIN_SYSTEM_TYPES_H

#include "ps_plugin_api.h"

typedef struct PluginSystemGeneration PluginSystemGeneration;
typedef struct PluginSystemPackage PluginSystemPackage;
typedef struct PluginSystemServiceRegistration PluginSystemServiceRegistration;
typedef struct PluginSystemServiceLeaseData PluginSystemServiceLeaseData;
typedef struct PluginSystemEventRegistration PluginSystemEventRegistration;
typedef struct PluginSystemHookRegistration PluginSystemHookRegistration;

typedef enum {
	PS_PACKAGE_STATUS_DISCOVERED = 0,
	PS_PACKAGE_STATUS_DISABLED = 1,
	PS_PACKAGE_STATUS_RESOLVED = 2,
	PS_PACKAGE_STATUS_ACTIVE = 3,
	PS_PACKAGE_STATUS_FAILED = 4
} PluginSystemPackageStatus;

typedef enum {
	PS_GENERATION_STATE_DISCOVERED = 0,
	PS_GENERATION_STATE_COMPILED = 1,
	PS_GENERATION_STATE_LOADED = 2,
	PS_GENERATION_STATE_STARTED = 3,
	PS_GENERATION_STATE_ACTIVE = 4,
	PS_GENERATION_STATE_DRAINING = 5,
	PS_GENERATION_STATE_STOPPED = 6,
	PS_GENERATION_STATE_FAILED = 7
} PluginSystemGenerationState;

struct PluginSystemServiceRegistration {
	PluginSystemGeneration* pGeneration;
	str sServiceName;
	str sProviderXid;
	str sCapabilitiesRequired;
	int iMajorVersion;
	int iMinorVersion;
	int iLifecycleScope;
	int iServiceRowId;
	int iResourceId;
	size_t iVtableSize;
	const void* pVtable;
	bool bPublished;
	bool bListed;
};

struct PluginSystemServiceLeaseData {
	PluginSystemGeneration* pConsumerGeneration;
	PluginSystemGeneration* pProviderGeneration;
	PluginSystemServiceRegistration* pRegistration;
	bool bReleased;
};

struct PluginSystemEventRegistration {
	PluginSystemGeneration* pGeneration;
	str sEventName;
	XAdminEventProc pProc;
	int iResourceId;
	bool bPublished;
	bool bListed;
};

struct PluginSystemHookRegistration {
	PluginSystemGeneration* pGeneration;
	str sHookName;
	int iSort;
	XAdminHookProc pProc;
	int iResourceId;
	bool bPublished;
	bool bListed;
};

struct PluginSystemGeneration {
	uint32_t iGeneration;
	int iState;
	int iRefCount;
	int64 iLoadTime;
	int64 iStartTime;
	int64 iStopTime;
	str sWorkDir;
	str sCompileHash;
	str sErrorMessage;
	TCCState* pTccState;
	const XAdminPluginDescriptor* pDescriptor;
	XAdminPluginHandle hPlugin;
	PluginSystemPackage* pPackage;
	xlist lstRouteTokens;
	xlist lstMenuTokens;
	xlist lstAuthGroupTokens;
	xlist lstAuthTokens;
	xlist lstUriAuthTokens;
	xlist lstServiceRegistrations;
	xlist lstEventRegistrations;
	xlist lstHookRegistrations;
};

struct PluginSystemPackage {
	int iFormatVersion;
	int iSort;
	str sXid;
	str sName;
	str sTitle;
	str sDescription;
	str sVersion;
	str sAuthor;
	str sKind;
	str sPackageId;
	str sMountPath;
	str sRootPath;
	str sManifestPath;
	str sEntry;
	str sDataPath;
	str sPrivateDbPath;
	bool bEnabled;
	bool bInstalled;
	int iStatus;
	uint32_t iNextGeneration;
	uint32_t iActiveGeneration;
	xvalue tblConfig;
	PluginSystemGeneration* pActiveGeneration;
	xlist lstDrainingGenerations;
	int64 iCreateTime;
	int64 iUpdateTime;
	xvalue tblManifest;
	xvalue tblDefaultConfig;
	xvalue tblConfigSchema;
};

typedef struct PluginSystemManager {
	xlist lstPackages;
	xlist lstServices;
	xlist lstServiceSnapshots;
	PluginSystemServiceRegistration** ppPublishedServices;
	xlist lstEvents;
	xlist lstEventSnapshots;
	PluginSystemEventRegistration** ppPublishedEvents;
	xlist lstHooks;
	xlist lstHookSnapshots;
	PluginSystemHookRegistration** ppPublishedHooks;
	str sPluginRootPath;
	str sDataPath;
} PluginSystemManager;

PluginSystemManager* G_PluginSystem = NULL;

str PS_PackageKey(PluginSystemPackage* pPackage)
{
	if ( pPackage == NULL ) {
		return NULL;
	}
	return pPackage->sXid ? pPackage->sXid : pPackage->sName;
}

const char* PS_PackageStatusText(int iStatus)
{
	switch ( iStatus ) {
		case PS_PACKAGE_STATUS_DISCOVERED: return "discovered";
		case PS_PACKAGE_STATUS_DISABLED: return "disabled";
		case PS_PACKAGE_STATUS_RESOLVED: return "resolved";
		case PS_PACKAGE_STATUS_ACTIVE: return "active";
		case PS_PACKAGE_STATUS_FAILED: return "failed";
		default: return "unknown";
	}
}

const char* PS_GenerationStateText(int iState)
{
	switch ( iState ) {
		case PS_GENERATION_STATE_ACTIVE: return "active";
		case PS_GENERATION_STATE_DRAINING: return "draining";
		case PS_GENERATION_STATE_STOPPED: return "stopped";
		case PS_GENERATION_STATE_FAILED: return "failed";
		case PS_GENERATION_STATE_STARTED: return "started";
		case PS_GENERATION_STATE_LOADED: return "loaded";
		case PS_GENERATION_STATE_COMPILED: return "compiled";
		case PS_GENERATION_STATE_DISCOVERED: return "discovered";
		default: return "unknown";
	}
}

int PS_PackageStatusFromText(str sStatus)
{
	if ( !sStatus ) return PS_PACKAGE_STATUS_DISCOVERED;
	if ( strcmp(sStatus, "disabled") == 0 ) return PS_PACKAGE_STATUS_DISABLED;
	if ( strcmp(sStatus, "resolved") == 0 ) return PS_PACKAGE_STATUS_RESOLVED;
	if ( strcmp(sStatus, "active") == 0 ) return PS_PACKAGE_STATUS_ACTIVE;
	if ( strcmp(sStatus, "failed") == 0 ) return PS_PACKAGE_STATUS_FAILED;
	return PS_PACKAGE_STATUS_DISCOVERED;
}

void PS_FreeString(str* psValue)
{
	if ( psValue && *psValue ) {
		xrtFree(*psValue);
		*psValue = NULL;
	}
}

xvalue PS_ValueDup(xvalue objValue)
{
	str sJson;
	xvalue objCopy;

	if ( objValue == NULL ) {
		return NULL;
	}

	sJson = xrtStringifyJSON(objValue, FALSE, NULL);
	if ( sJson == NULL ) {
		return NULL;
	}

	objCopy = xrtParseJSON(sJson, strlen(sJson));
	xrtFree(sJson);
	return objCopy;
}

str PS_ValueToStringDup(xvalue objValue)
{
	str sJson;
	size_t iLen;

	if ( objValue == NULL ) {
		return NULL;
	}

	sJson = xrtStringifyJSON(objValue, FALSE, NULL);
	if ( sJson == NULL ) {
		return NULL;
	}

	iLen = strlen(sJson);
	if ( (iLen >= 2) && (sJson[0] == '"') && (sJson[iLen - 1] == '"') ) {
		str sValue;
		sJson[iLen - 1] = '\0';
		sValue = xrtCopyStr(sJson + 1, 0);
		xrtFree(sJson);
		return sValue;
	}

	return sJson;
}

PluginSystemGeneration* PS_CreateGeneration(uint32_t iGeneration)
{
	PluginSystemGeneration* pGeneration = xrtMalloc(sizeof(PluginSystemGeneration));
	if ( !pGeneration ) {
		return NULL;
	}

	memset(pGeneration, 0, sizeof(PluginSystemGeneration));
	pGeneration->iGeneration = iGeneration;
	pGeneration->iState = PS_GENERATION_STATE_DISCOVERED;
	pGeneration->iLoadTime = xrtNow();
	pGeneration->lstRouteTokens = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	pGeneration->lstMenuTokens = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	pGeneration->lstAuthGroupTokens = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	pGeneration->lstAuthTokens = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	pGeneration->lstUriAuthTokens = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	pGeneration->lstServiceRegistrations = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	pGeneration->lstEventRegistrations = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	pGeneration->lstHookRegistrations = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	return pGeneration;
}

void PS_DestroyGeneration(PluginSystemGeneration* pGeneration)
{
	if ( pGeneration == NULL ) {
		return;
	}

	if ( pGeneration->lstRouteTokens ) {
		xrtListDestroy(pGeneration->lstRouteTokens);
		pGeneration->lstRouteTokens = NULL;
	}
	if ( pGeneration->lstMenuTokens ) {
		xrtListDestroy(pGeneration->lstMenuTokens);
		pGeneration->lstMenuTokens = NULL;
	}
	if ( pGeneration->lstAuthGroupTokens ) {
		xrtListDestroy(pGeneration->lstAuthGroupTokens);
		pGeneration->lstAuthGroupTokens = NULL;
	}
	if ( pGeneration->lstAuthTokens ) {
		xrtListDestroy(pGeneration->lstAuthTokens);
		pGeneration->lstAuthTokens = NULL;
	}
	if ( pGeneration->lstUriAuthTokens ) {
		xrtListDestroy(pGeneration->lstUriAuthTokens);
		pGeneration->lstUriAuthTokens = NULL;
	}
	if ( pGeneration->lstServiceRegistrations ) {
		xrtListDestroy(pGeneration->lstServiceRegistrations);
		pGeneration->lstServiceRegistrations = NULL;
	}
	if ( pGeneration->lstEventRegistrations ) {
		xrtListDestroy(pGeneration->lstEventRegistrations);
		pGeneration->lstEventRegistrations = NULL;
	}
	if ( pGeneration->lstHookRegistrations ) {
		xrtListDestroy(pGeneration->lstHookRegistrations);
		pGeneration->lstHookRegistrations = NULL;
	}

	if ( pGeneration->pTccState ) {
		xsDestroyTCC(pGeneration->pTccState);
		pGeneration->pTccState = NULL;
	}

	PS_FreeString(&pGeneration->sWorkDir);
	PS_FreeString(&pGeneration->sCompileHash);
	PS_FreeString(&pGeneration->sErrorMessage);
	xrtFree(pGeneration);
}

PluginSystemPackage* PS_CreatePackage()
{
	PluginSystemPackage* pPackage = xrtMalloc(sizeof(PluginSystemPackage));
	if ( !pPackage ) {
		return NULL;
	}

	memset(pPackage, 0, sizeof(PluginSystemPackage));
	pPackage->sPackageId = NULL;
	pPackage->sMountPath = NULL;
	pPackage->iStatus = PS_PACKAGE_STATUS_DISCOVERED;
	pPackage->iNextGeneration = 1;
	pPackage->lstDrainingGenerations = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	pPackage->iCreateTime = xrtNow();
	pPackage->iUpdateTime = pPackage->iCreateTime;
	return pPackage;
}

void PS_DestroyPackage(PluginSystemPackage* pPackage)
{
	if ( pPackage == NULL ) {
		return;
	}

	if ( pPackage->pActiveGeneration ) {
		PS_DestroyGeneration(pPackage->pActiveGeneration);
		pPackage->pActiveGeneration = NULL;
	}
	if ( pPackage->lstDrainingGenerations ) {
		for ( int i = 0; i < xrtListCount(pPackage->lstDrainingGenerations); i++ ) {
			PluginSystemGeneration* pGeneration = xrtListGetPtr(pPackage->lstDrainingGenerations, i);
			if ( pGeneration ) {
				PS_DestroyGeneration(pGeneration);
				xrtListSetPtr(pPackage->lstDrainingGenerations, i, NULL, NULL);
			}
		}
		xrtListDestroy(pPackage->lstDrainingGenerations);
		pPackage->lstDrainingGenerations = NULL;
	}

	if ( pPackage->tblConfig ) {
		xvoUnref(pPackage->tblConfig);
		pPackage->tblConfig = NULL;
	}

	if ( pPackage->tblManifest ) {
		xvoUnref(pPackage->tblManifest);
		pPackage->tblManifest = NULL;
	}

	if ( pPackage->tblDefaultConfig ) {
		xvoUnref(pPackage->tblDefaultConfig);
		pPackage->tblDefaultConfig = NULL;
	}

	if ( pPackage->tblConfigSchema ) {
		xvoUnref(pPackage->tblConfigSchema);
		pPackage->tblConfigSchema = NULL;
	}

	PS_FreeString(&pPackage->sXid);
	PS_FreeString(&pPackage->sName);
	PS_FreeString(&pPackage->sTitle);
	PS_FreeString(&pPackage->sDescription);
	PS_FreeString(&pPackage->sVersion);
	PS_FreeString(&pPackage->sAuthor);
	PS_FreeString(&pPackage->sKind);
	PS_FreeString(&pPackage->sPackageId);
	PS_FreeString(&pPackage->sMountPath);
	PS_FreeString(&pPackage->sRootPath);
	PS_FreeString(&pPackage->sManifestPath);
	PS_FreeString(&pPackage->sEntry);
	PS_FreeString(&pPackage->sDataPath);
	PS_FreeString(&pPackage->sPrivateDbPath);
	xrtFree(pPackage);
}

const char* PS_PackageLogId(PluginSystemPackage* pPackage)
{
	str sXid = PS_PackageKey(pPackage);
	return sXid ? (const char*)sXid : "(unknown)";
}

#endif
