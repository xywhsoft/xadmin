#ifndef XADMIN_PLUGIN_SYSTEM_MANAGER_H
#define XADMIN_PLUGIN_SYSTEM_MANAGER_H

#include "ps_runtime.h"
#include "ps_manifest.h"

PluginSystemPackage* PluginSystem_FindPackage(str sName)
{
	if ( (G_PluginSystem == NULL) || (G_PluginSystem->lstPackages == NULL) || (sName == NULL) ) {
		return NULL;
	}

	for ( int i = 0; i < xrtListCount(G_PluginSystem->lstPackages); i++ ) {
		PluginSystemPackage* pPackage = xrtListGetPtr(G_PluginSystem->lstPackages, i);
		if ( pPackage == NULL ) {
			continue;
		}
		if ( (pPackage->sName && strcmp(pPackage->sName, sName) == 0) || (pPackage->sPluginId && strcmp(pPackage->sPluginId, sName) == 0) ) {
			return pPackage;
		}
	}
	return NULL;
}

PluginSystemInstance* PluginSystem_FindInstance(str sName)
{
	PluginSystemPackage* pPackage = PluginSystem_FindPackage(sName);
	return pPackage ? PS_GetDefaultInstance(pPackage) : NULL;
}

xvalue PS_ManagerBuildPackageData(PluginSystemPackage* pPackage)
{
	PluginSystemInstance* pInstance;
	xvalue tblData;

	if ( pPackage == NULL ) {
		return NULL;
	}

	pInstance = PS_GetDefaultInstance(pPackage);
	tblData = xvoCreateTable();
	xvoTableSetInt(tblData, "formatVersion", 13, pPackage->iFormatVersion);
	xvoTableSetText(tblData, "id", 2, pPackage->sPluginId ? pPackage->sPluginId : (str)"", 0, FALSE);
	xvoTableSetText(tblData, "name", 4, pPackage->sName ? pPackage->sName : (str)"", 0, FALSE);
	xvoTableSetText(tblData, "title", 5, pPackage->sTitle ? pPackage->sTitle : (str)"", 0, FALSE);
	xvoTableSetText(tblData, "desc", 4, pPackage->sDescription ? pPackage->sDescription : (str)"", 0, FALSE);
	xvoTableSetText(tblData, "version", 7, pPackage->sVersion ? pPackage->sVersion : (str)"", 0, FALSE);
	xvoTableSetText(tblData, "author", 6, pPackage->sAuthor ? pPackage->sAuthor : (str)"", 0, FALSE);
	xvoTableSetText(tblData, "kind", 4, pPackage->sKind ? pPackage->sKind : (str)"singleton", 0, FALSE);
	xvoTableSetText(tblData, "entry", 5, pPackage->sEntry ? pPackage->sEntry : (str)"main.c", 0, FALSE);
	xvoTableSetText(tblData, "path", 4, pPackage->sRootPath ? pPackage->sRootPath : (str)"", 0, FALSE);
	xvoTableSetBool(tblData, "multiInstance", 13, pPackage->bMultiInstance);
	xvoTableSetInt(tblData, "sort", 4, pPackage->iSort);

	if ( pInstance ) {
		xvoTableSetBool(tblData, "enabled", 7, pInstance->bEnabled);
		xvoTableSetBool(tblData, "loaded", 6, pInstance->pActiveGeneration != NULL);
		xvoTableSetText(tblData, "status", 6, (str)PS_InstanceStatusText(pInstance->iStatus), 0, FALSE);
		xvoTableSetInt(tblData, "generation", 10, (int)pInstance->iActiveGeneration);
		if ( pInstance->tblConfig ) {
			xvalue tblConfig = PS_ValueDup(pInstance->tblConfig);
			if ( tblConfig ) {
				xvoTableSetValue(tblData, "settings", 8, tblConfig, TRUE);
			}
		}
	} else {
		xvoTableSetBool(tblData, "enabled", 7, FALSE);
		xvoTableSetBool(tblData, "loaded", 6, FALSE);
		xvoTableSetText(tblData, "status", 6, (str)PS_InstanceStatusText(PS_INSTANCE_STATUS_DISCOVERED), 0, FALSE);
		xvoTableSetInt(tblData, "generation", 10, 0);
	}

	return tblData;
}

bool PS_ManagerEnsureDefaultInstance(PluginSystemPackage* pPackage)
{
	PluginSystemInstance* pInstance;
	int iIndex;

	if ( pPackage == NULL ) {
		return FALSE;
	}

	if ( PS_GetDefaultInstance(pPackage) ) {
		return TRUE;
	}

	pInstance = PS_CreateInstance(PS_PackageKey(pPackage), PS_PackageKey(pPackage));
	if ( pInstance == NULL ) {
		return FALSE;
	}

	pInstance->tblConfig = pPackage->tblDefaultConfig ? PS_ValueDup(pPackage->tblDefaultConfig) : xvoCreateTable();
	if ( !PS_StorageLoadInstanceState(pInstance) ) {
		PS_StorageSaveInstance(pInstance);
	}

	iIndex = xrtListCount(pPackage->lstInstances);
	xrtListSetPtr(pPackage->lstInstances, iIndex, pInstance, NULL);
	return TRUE;
}

int PS_ManagerScanPluginProc(str sPath, size_t iSize, int bDir, ptr pData, size_t iPathSize)
{
	PluginSystemPackage* pPackage;
	str sName;
	int iIndex;

	(void)iSize;
	(void)pData;
	(void)iPathSize;

	if ( bDir != 1 ) {
		return FALSE;
	}

	sName = xrtPathGetName(sPath, 0);
	if ( (sName == NULL) || (sName[0] == '.') || (sName[0] == '_') ) {
		if ( sName ) {
			xrtFree(sName);
		}
		return FALSE;
	}

	pPackage = PS_CreatePackage();
	if ( (pPackage == NULL) || !PS_LoadManifest(pPackage, sPath) ) {
		if ( pPackage ) {
			PS_DestroyPackage(pPackage);
		}
		xrtFree(sName);
		return FALSE;
	}

	if ( pPackage->sName == NULL ) {
		pPackage->sName = xrtCopyStr(sName, 0);
	}
	if ( pPackage->sTitle == NULL ) {
		pPackage->sTitle = xrtCopyStr(pPackage->sName, 0);
	}

	PS_StorageSavePackage(pPackage);
	PS_ManagerEnsureDefaultInstance(pPackage);
	printf("        [PluginSystem] Discovered package: %s\n", PS_PackageKey(pPackage) ? (const char*)PS_PackageKey(pPackage) : "(unknown)");

	iIndex = xrtListCount(G_PluginSystem->lstPackages);
	xrtListSetPtr(G_PluginSystem->lstPackages, iIndex, pPackage, NULL);
	xrtFree(sName);
	return FALSE;
}

void PS_ManagerScanPackages()
{
	if ( (G_PluginSystem == NULL) || (G_PluginSystem->sPluginRootPath == NULL) ) {
		return;
	}

	xrtDirScan(G_PluginSystem->sPluginRootPath, FALSE, PS_ManagerScanPluginProc, NULL);
}

void PS_ManagerAutoStartEnabled()
{
	if ( (G_PluginSystem == NULL) || (G_PluginSystem->lstPackages == NULL) ) {
		return;
	}

	for ( int i = 0; i < xrtListCount(G_PluginSystem->lstPackages); i++ ) {
		PluginSystemPackage* pPackage = xrtListGetPtr(G_PluginSystem->lstPackages, i);
		PluginSystemInstance* pInstance = PS_GetDefaultInstance(pPackage);
		printf("        [PluginSystem] AutoStart check: package=%s enabled=%d status=%d active=%u\n",
			PS_PackageKey(pPackage) ? (const char*)PS_PackageKey(pPackage) : "(unknown)",
			pInstance ? pInstance->bEnabled : 0,
			pInstance ? pInstance->iStatus : -1,
			pInstance ? pInstance->iActiveGeneration : 0);
		if ( pInstance && pInstance->bEnabled ) {
			if ( !PS_RuntimeStartInstance(pPackage, pInstance) ) {
				printf("        [PluginSystem] AutoStart failed: package=%s instance=%s\n",
					PS_PackageKey(pPackage) ? (const char*)PS_PackageKey(pPackage) : "(unknown)",
					pInstance->sInstanceId ? (const char*)pInstance->sInstanceId : "(unknown)");
			}
		}
	}
}

xvalue PluginSystem_GetList()
{
	xvalue arrList = xvoCreateArray();

	if ( (G_PluginSystem == NULL) || (G_PluginSystem->lstPackages == NULL) ) {
		return arrList;
	}

	for ( int i = 0; i < xrtListCount(G_PluginSystem->lstPackages); i++ ) {
		PluginSystemPackage* pPackage = xrtListGetPtr(G_PluginSystem->lstPackages, i);
		xvalue tblItem = PS_ManagerBuildPackageData(pPackage);
		if ( tblItem ) {
			xvoArrayAppendValue(arrList, tblItem, TRUE);
		}
	}
	return arrList;
}

xvalue PluginSystem_GetPackageData(str sName)
{
	PluginSystemPackage* pPackage = PluginSystem_FindPackage(sName);
	return pPackage ? PS_ManagerBuildPackageData(pPackage) : NULL;
}

xvalue PluginSystem_GetSettings(str sName)
{
	PluginSystemInstance* pInstance = PluginSystem_FindInstance(sName);
	if ( (pInstance == NULL) || (pInstance->tblConfig == NULL) ) {
		return xvoCreateTable();
	}
	return PS_ValueDup(pInstance->tblConfig);
}

bool PluginSystem_SaveSettings(str sName, xvalue tblSettings)
{
	PluginSystemPackage* pPackage = PluginSystem_FindPackage(sName);
	PluginSystemInstance* pInstance = pPackage ? PS_GetDefaultInstance(pPackage) : NULL;
	xvalue tblNewConfig;
	xvalue tblOldConfig;

	if ( (pPackage == NULL) || (pInstance == NULL) ) {
		return FALSE;
	}

	tblNewConfig = tblSettings ? PS_ValueDup(tblSettings) : xvoCreateTable();
	if ( tblNewConfig == NULL ) {
		tblNewConfig = xvoCreateTable();
	}

	tblOldConfig = pInstance->tblConfig ? PS_ValueDup(pInstance->tblConfig) : NULL;
	if ( pInstance->tblConfig ) {
		xvoUnref(pInstance->tblConfig);
	}
	pInstance->tblConfig = tblNewConfig;

	if ( pInstance->pActiveGeneration && pInstance->pActiveGeneration->pDescriptor && pInstance->pActiveGeneration->pDescriptor->OnConfigChanged ) {
		if ( pInstance->pActiveGeneration->pDescriptor->OnConfigChanged((XAdminPluginHandle)pInstance->pActiveGeneration, pInstance->tblConfig) != 0 ) {
			if ( pInstance->tblConfig ) {
				xvoUnref(pInstance->tblConfig);
			}
			pInstance->tblConfig = tblOldConfig ? tblOldConfig : xvoCreateTable();
			return FALSE;
		}
	}

	if ( tblOldConfig ) {
		xvoUnref(tblOldConfig);
	}

	pInstance->iUpdateTime = xrtNow();
	return PS_StorageSaveInstance(pInstance);
}

bool PluginSystem_Enable(str sName)
{
	PluginSystemPackage* pPackage = PluginSystem_FindPackage(sName);
	PluginSystemInstance* pInstance = pPackage ? PS_GetDefaultInstance(pPackage) : NULL;
	if ( (pPackage == NULL) || (pInstance == NULL) ) {
		return FALSE;
	}
	if ( pInstance->pActiveGeneration ) {
		return TRUE;
	}
	return PS_RuntimeStartInstance(pPackage, pInstance);
}

bool PluginSystem_Disable(str sName)
{
	PluginSystemInstance* pInstance = PluginSystem_FindInstance(sName);
	if ( pInstance == NULL ) {
		return FALSE;
	}
	return PS_RuntimeStopInstance(pInstance);
}

bool PluginSystem_Reload(str sName)
{
	PluginSystemPackage* pPackage = PluginSystem_FindPackage(sName);
	PluginSystemInstance* pInstance = pPackage ? PS_GetDefaultInstance(pPackage) : NULL;
	if ( (pPackage == NULL) || (pInstance == NULL) ) {
		return FALSE;
	}
	return PS_RuntimeReloadInstance(pPackage, pInstance);
}

void PluginSystem_Init()
{
	printf("        PluginSystem_Init \n");

	if ( G_PluginSystem ) {
		return;
	}

	G_PluginSystem = xrtMalloc(sizeof(PluginSystemManager));
	memset(G_PluginSystem, 0, sizeof(PluginSystemManager));

	G_PluginSystem->lstPackages = xrtListCreate(sizeof(ptr), 0);
	G_PluginSystem->sPluginRootPath = xrtPathJoin(2, AppPath, "script/plugin");
	G_PluginSystem->sDataPath = xrtPathJoin(3, AppPath, "data", "plugin_system");

	if ( G_PluginSystem->sPluginRootPath ) {
		xrtDirCreateAll(G_PluginSystem->sPluginRootPath);
	}
	if ( G_PluginSystem->sDataPath ) {
		str sGenerationPath = xrtPathJoin(2, G_PluginSystem->sDataPath, "generation");
		xrtDirCreateAll(G_PluginSystem->sDataPath);
		if ( sGenerationPath ) {
			xrtDirCreateAll(sGenerationPath);
			xrtFree(sGenerationPath);
		}
	}

	PS_StorageInit();
	PS_HostAPI_Init();
	PS_ManagerScanPackages();
	PS_ManagerAutoStartEnabled();
}

void PluginSystem_Unit()
{
	if ( G_PluginSystem == NULL ) {
		return;
	}

	printf("        PluginSystem_Unit \n");

	if ( G_PluginSystem->lstPackages ) {
		for ( int i = 0; i < xrtListCount(G_PluginSystem->lstPackages); i++ ) {
			PluginSystemPackage* pPackage = xrtListGetPtr(G_PluginSystem->lstPackages, i);
			PluginSystemInstance* pInstance = PS_GetDefaultInstance(pPackage);
			printf("        [PluginSystem] Destroy package: %s\n", PS_PackageKey(pPackage) ? (const char*)PS_PackageKey(pPackage) : "(unknown)");
			if ( pInstance && pInstance->pActiveGeneration ) {
				PS_RuntimeStopInstance(pInstance);
			}
			PS_DestroyPackage(pPackage);
			xrtListSetPtr(G_PluginSystem->lstPackages, i, NULL, NULL);
		}
		xrtListDestroy(G_PluginSystem->lstPackages);
	}

	PS_StorageUnit();
	PS_FreeString(&G_PluginSystem->sPluginRootPath);
	PS_FreeString(&G_PluginSystem->sDataPath);
	xrtFree(G_PluginSystem);
	G_PluginSystem = NULL;
}

#endif
