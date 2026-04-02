#ifndef XADMIN_PLUGIN_SYSTEM_TYPES_H
#define XADMIN_PLUGIN_SYSTEM_TYPES_H

#include "ps_plugin_api.h"

typedef enum {
	PS_INSTANCE_STATUS_DISCOVERED = 0,
	PS_INSTANCE_STATUS_DISABLED = 1,
	PS_INSTANCE_STATUS_RESOLVED = 2,
	PS_INSTANCE_STATUS_ACTIVE = 3,
	PS_INSTANCE_STATUS_FAILED = 4
} PluginSystemInstanceStatus;

typedef enum {
	PS_GENERATION_STATE_DISCOVERED = 0,
	PS_GENERATION_STATE_COMPILED = 1,
	PS_GENERATION_STATE_LOADED = 2,
	PS_GENERATION_STATE_STARTED = 3,
	PS_GENERATION_STATE_ACTIVE = 4,
	PS_GENERATION_STATE_STOPPED = 5,
	PS_GENERATION_STATE_FAILED = 6
} PluginSystemGenerationState;

typedef struct PluginSystemGeneration {
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
	xlist lstRouteTokens;
} PluginSystemGeneration;

typedef struct PluginSystemInstance {
	str sInstanceId;
	str sPackageId;
	str sInstanceName;
	str sMountPath;
	bool bEnabled;
	bool bInstalled;
	int iStatus;
	uint32_t iNextGeneration;
	uint32_t iActiveGeneration;
	xvalue tblConfig;
	PluginSystemGeneration* pActiveGeneration;
	int64 iCreateTime;
	int64 iUpdateTime;
} PluginSystemInstance;

typedef struct PluginSystemPackage {
	int iFormatVersion;
	int iSort;
	str sPluginId;
	str sName;
	str sTitle;
	str sDescription;
	str sVersion;
	str sAuthor;
	str sKind;
	bool bMultiInstance;
	str sRootPath;
	str sManifestPath;
	str sEntry;
	xvalue tblManifest;
	xvalue tblDefaultConfig;
	xvalue tblConfigSchema;
	xlist lstInstances;
} PluginSystemPackage;

typedef struct PluginSystemManager {
	xlist lstPackages;
	str sPluginRootPath;
	str sDataPath;
} PluginSystemManager;

PluginSystemManager* G_PluginSystem = NULL;

str PS_PackageKey(PluginSystemPackage* pPackage)
{
	if ( pPackage == NULL ) {
		return NULL;
	}
	return pPackage->sPluginId ? pPackage->sPluginId : pPackage->sName;
}

const char* PS_InstanceStatusText(int iStatus)
{
	switch ( iStatus ) {
		case PS_INSTANCE_STATUS_DISCOVERED: return "discovered";
		case PS_INSTANCE_STATUS_DISABLED: return "disabled";
		case PS_INSTANCE_STATUS_RESOLVED: return "resolved";
		case PS_INSTANCE_STATUS_ACTIVE: return "active";
		case PS_INSTANCE_STATUS_FAILED: return "failed";
		default: return "unknown";
	}
}

int PS_InstanceStatusFromText(str sStatus)
{
	if ( !sStatus ) return PS_INSTANCE_STATUS_DISCOVERED;
	if ( strcmp(sStatus, "disabled") == 0 ) return PS_INSTANCE_STATUS_DISABLED;
	if ( strcmp(sStatus, "resolved") == 0 ) return PS_INSTANCE_STATUS_RESOLVED;
	if ( strcmp(sStatus, "active") == 0 ) return PS_INSTANCE_STATUS_ACTIVE;
	if ( strcmp(sStatus, "failed") == 0 ) return PS_INSTANCE_STATUS_FAILED;
	return PS_INSTANCE_STATUS_DISCOVERED;
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
	pGeneration->lstRouteTokens = xrtListCreate(sizeof(ptr), 0);
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

	if ( pGeneration->pTccState ) {
		xsDestroyTCC(pGeneration->pTccState);
		pGeneration->pTccState = NULL;
	}

	PS_FreeString(&pGeneration->sWorkDir);
	PS_FreeString(&pGeneration->sCompileHash);
	PS_FreeString(&pGeneration->sErrorMessage);
	xrtFree(pGeneration);
}

PluginSystemInstance* PS_CreateInstance(str sInstanceId, str sPackageId)
{
	PluginSystemInstance* pInstance = xrtMalloc(sizeof(PluginSystemInstance));
	if ( !pInstance ) {
		return NULL;
	}

	memset(pInstance, 0, sizeof(PluginSystemInstance));
	pInstance->sInstanceId = xrtCopyStr(sInstanceId, 0);
	pInstance->sPackageId = xrtCopyStr(sPackageId, 0);
	pInstance->sInstanceName = xrtCopyStr(sInstanceId, 0);
	pInstance->iStatus = PS_INSTANCE_STATUS_DISCOVERED;
	pInstance->iNextGeneration = 1;
	pInstance->iCreateTime = xrtNow();
	pInstance->iUpdateTime = pInstance->iCreateTime;
	return pInstance;
}

void PS_DestroyInstance(PluginSystemInstance* pInstance)
{
	if ( pInstance == NULL ) {
		return;
	}

	if ( pInstance->pActiveGeneration ) {
		PS_DestroyGeneration(pInstance->pActiveGeneration);
		pInstance->pActiveGeneration = NULL;
	}

	if ( pInstance->tblConfig ) {
		xvoUnref(pInstance->tblConfig);
		pInstance->tblConfig = NULL;
	}

	PS_FreeString(&pInstance->sInstanceId);
	PS_FreeString(&pInstance->sPackageId);
	PS_FreeString(&pInstance->sInstanceName);
	PS_FreeString(&pInstance->sMountPath);
	xrtFree(pInstance);
}

PluginSystemPackage* PS_CreatePackage()
{
	PluginSystemPackage* pPackage = xrtMalloc(sizeof(PluginSystemPackage));
	if ( !pPackage ) {
		return NULL;
	}

	memset(pPackage, 0, sizeof(PluginSystemPackage));
	pPackage->lstInstances = xrtListCreate(sizeof(ptr), 0);
	return pPackage;
}

void PS_DestroyPackage(PluginSystemPackage* pPackage)
{
	if ( pPackage == NULL ) {
		return;
	}

	if ( pPackage->lstInstances ) {
		int iCount = xrtListCount(pPackage->lstInstances);
		for ( int i = 0; i < iCount; i++ ) {
			PluginSystemInstance* pInstance = xrtListGetPtr(pPackage->lstInstances, i);
			PS_DestroyInstance(pInstance);
			xrtListSetPtr(pPackage->lstInstances, i, NULL, NULL);
		}
		xrtListDestroy(pPackage->lstInstances);
		pPackage->lstInstances = NULL;
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

	PS_FreeString(&pPackage->sPluginId);
	PS_FreeString(&pPackage->sName);
	PS_FreeString(&pPackage->sTitle);
	PS_FreeString(&pPackage->sDescription);
	PS_FreeString(&pPackage->sVersion);
	PS_FreeString(&pPackage->sAuthor);
	PS_FreeString(&pPackage->sKind);
	PS_FreeString(&pPackage->sRootPath);
	PS_FreeString(&pPackage->sManifestPath);
	PS_FreeString(&pPackage->sEntry);
	xrtFree(pPackage);
}

PluginSystemInstance* PS_GetDefaultInstance(PluginSystemPackage* pPackage)
{
	if ( (pPackage == NULL) || (pPackage->lstInstances == NULL) || (xrtListCount(pPackage->lstInstances) <= 0) ) {
		return NULL;
	}
	return xrtListGetPtr(pPackage->lstInstances, 0);
}

#endif
