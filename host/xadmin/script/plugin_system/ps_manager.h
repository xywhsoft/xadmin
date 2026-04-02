#ifndef XADMIN_PLUGIN_SYSTEM_MANAGER_H
#define XADMIN_PLUGIN_SYSTEM_MANAGER_H

#include "ps_runtime.h"
#include "ps_manifest.h"

bool PS_ManagerEnsureDefaultInstance(PluginSystemPackage* pPackage);

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
		if ( (pPackage->sName && strcmp(pPackage->sName, sName) == 0) || (pPackage->sXid && strcmp(pPackage->sXid, sName) == 0) ) {
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
	xvoTableSetText(tblData, "id", 2, pPackage->sXid ? pPackage->sXid : (str)"", 0, FALSE);
	xvoTableSetText(tblData, "xid", 3, pPackage->sXid ? pPackage->sXid : (str)"", 0, FALSE);
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

bool PS_ManagerIsValidXid(const char* sXid)
{
	size_t iLen;

	if ( (sXid == NULL) || (sXid[0] == '\0') ) {
		return FALSE;
	}

	iLen = strlen(sXid);
	if ( (iLen <= 0) || (iLen > 96) ) {
		return FALSE;
	}

	for ( size_t i = 0; i < iLen; i++ ) {
		char ch = sXid[i];
		if ( ((ch >= 'a') && (ch <= 'z'))
			|| ((ch >= 'A') && (ch <= 'Z'))
			|| ((ch >= '0') && (ch <= '9'))
			|| (ch == '.')
			|| (ch == '_')
			|| (ch == '-') ) {
			continue;
		}
		return FALSE;
	}

	return TRUE;
}

bool PS_ManagerIsSafeRelativePath(const char* sRelPath)
{
	if ( (sRelPath == NULL) || (sRelPath[0] == '\0') ) {
		return FALSE;
	}
	if ( (sRelPath[0] == '/') || (sRelPath[0] == '\\') ) {
		return FALSE;
	}
	if ( strchr(sRelPath, ':') != NULL ) {
		return FALSE;
	}
	if ( strstr(sRelPath, "..") != NULL ) {
		return FALSE;
	}
	return TRUE;
}

void PS_ManagerAssignInstancePaths(PluginSystemPackage* pPackage, PluginSystemInstance* pInstance)
{
	str sDataRoot = NULL;
	str sXid;

	if ( (pPackage == NULL) || (pInstance == NULL) || (AppPath == NULL) ) {
		return;
	}

	sXid = PS_PackageKey(pPackage);
	if ( (sXid == NULL) || (sXid[0] == '\0') ) {
		return;
	}

	sDataRoot = xrtPathJoin(5, AppPath, "data", "plugin", sXid, pInstance->sInstanceName ? pInstance->sInstanceName : pInstance->sInstanceId);
	if ( sDataRoot == NULL ) {
		return;
	}

	if ( (pInstance->sDataPath == NULL) || (pInstance->sDataPath[0] == '\0') ) {
		PS_FreeString(&pInstance->sDataPath);
		pInstance->sDataPath = xrtCopyStr(sDataRoot, 0);
	}
	if ( (pInstance->sPrivateDbPath == NULL) || (pInstance->sPrivateDbPath[0] == '\0') ) {
		PS_FreeString(&pInstance->sPrivateDbPath);
		pInstance->sPrivateDbPath = xrtPathJoin(2, sDataRoot, "plugin.db");
	}

	if ( pInstance->sDataPath ) {
		xrtDirCreateAll(pInstance->sDataPath);
	}
	xrtFree(sDataRoot);
}

void PS_ManagerClearPackageMetadata(PluginSystemPackage* pPackage)
{
	if ( pPackage == NULL ) {
		return;
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

	pPackage->iFormatVersion = 0;
	pPackage->iSort = 0;
	pPackage->bMultiInstance = FALSE;
	PS_FreeString(&pPackage->sXid);
	PS_FreeString(&pPackage->sName);
	PS_FreeString(&pPackage->sTitle);
	PS_FreeString(&pPackage->sDescription);
	PS_FreeString(&pPackage->sVersion);
	PS_FreeString(&pPackage->sAuthor);
	PS_FreeString(&pPackage->sKind);
	PS_FreeString(&pPackage->sRootPath);
	PS_FreeString(&pPackage->sManifestPath);
	PS_FreeString(&pPackage->sEntry);
}

void PS_ManagerAdoptPackageMetadata(PluginSystemPackage* pDest, PluginSystemPackage* pSrc)
{
	if ( (pDest == NULL) || (pSrc == NULL) ) {
		return;
	}

	pDest->iFormatVersion = pSrc->iFormatVersion;
	pDest->iSort = pSrc->iSort;
	pDest->bMultiInstance = pSrc->bMultiInstance;
	pDest->sXid = pSrc->sXid;
	pDest->sName = pSrc->sName;
	pDest->sTitle = pSrc->sTitle;
	pDest->sDescription = pSrc->sDescription;
	pDest->sVersion = pSrc->sVersion;
	pDest->sAuthor = pSrc->sAuthor;
	pDest->sKind = pSrc->sKind;
	pDest->sRootPath = pSrc->sRootPath;
	pDest->sManifestPath = pSrc->sManifestPath;
	pDest->sEntry = pSrc->sEntry;
	pDest->tblManifest = pSrc->tblManifest;
	pDest->tblDefaultConfig = pSrc->tblDefaultConfig;
	pDest->tblConfigSchema = pSrc->tblConfigSchema;

	pSrc->sXid = NULL;
	pSrc->sName = NULL;
	pSrc->sTitle = NULL;
	pSrc->sDescription = NULL;
	pSrc->sVersion = NULL;
	pSrc->sAuthor = NULL;
	pSrc->sKind = NULL;
	pSrc->sRootPath = NULL;
	pSrc->sManifestPath = NULL;
	pSrc->sEntry = NULL;
	pSrc->tblManifest = NULL;
	pSrc->tblDefaultConfig = NULL;
	pSrc->tblConfigSchema = NULL;
}

bool PS_ManagerRefreshPackage(PluginSystemPackage* pPackage, str sRootPath)
{
	PluginSystemPackage* pLoaded = NULL;

	if ( (pPackage == NULL) || (sRootPath == NULL) ) {
		return FALSE;
	}

	pLoaded = PS_CreatePackage();
	if ( (pLoaded == NULL) || !PS_LoadManifest(pLoaded, sRootPath) ) {
		if ( pLoaded ) {
			PS_DestroyPackage(pLoaded);
		}
		return FALSE;
	}

	if ( pLoaded->sXid == NULL ) {
		str sDirName = xrtPathGetName(sRootPath, 0);
		pLoaded->sXid = sDirName;
	}
	if ( pLoaded->sName == NULL ) {
		pLoaded->sName = xrtCopyStr(PS_PackageKey(pLoaded), 0);
	}
	if ( pLoaded->sTitle == NULL ) {
		pLoaded->sTitle = xrtCopyStr(pLoaded->sName, 0);
	}

	PS_ManagerClearPackageMetadata(pPackage);
	PS_ManagerAdoptPackageMetadata(pPackage, pLoaded);
	PS_DestroyPackage(pLoaded);
	return TRUE;
}

PluginSystemPackage* PS_ManagerDiscoverPackagePath(str sPath, bool bRefreshExisting)
{
	PluginSystemPackage* pPackage = NULL;
	PluginSystemPackage* pExisting = NULL;
	str sDirName = NULL;
	int iIndex;

	if ( sPath == NULL ) {
		return NULL;
	}

	pPackage = PS_CreatePackage();
	if ( (pPackage == NULL) || !PS_LoadManifest(pPackage, sPath) ) {
		if ( pPackage ) {
			PS_DestroyPackage(pPackage);
		}
		return NULL;
	}

	sDirName = xrtPathGetName(sPath, 0);
	if ( pPackage->sXid == NULL ) {
		pPackage->sXid = sDirName ? sDirName : NULL;
		sDirName = NULL;
	}
	if ( pPackage->sName == NULL ) {
		pPackage->sName = xrtCopyStr(PS_PackageKey(pPackage), 0);
	}
	if ( pPackage->sTitle == NULL ) {
		pPackage->sTitle = xrtCopyStr(pPackage->sName, 0);
	}

	pExisting = PluginSystem_FindPackage(PS_PackageKey(pPackage));
	if ( pExisting ) {
		if ( bRefreshExisting && PS_ManagerRefreshPackage(pExisting, sPath) ) {
			PS_StorageSavePackage(pExisting);
			PS_ManagerEnsureDefaultInstance(pExisting);
		}
		PS_DestroyPackage(pPackage);
		if ( sDirName ) {
			xrtFree(sDirName);
		}
		return pExisting;
	}

	PS_StorageSavePackage(pPackage);
	PS_ManagerEnsureDefaultInstance(pPackage);
	printf("        [PluginSystem] Discovered package: %s\n", PS_PackageKey(pPackage) ? (const char*)PS_PackageKey(pPackage) : "(unknown)");

	iIndex = xrtListCount(G_PluginSystem->lstPackages);
	xrtListSetPtr(G_PluginSystem->lstPackages, iIndex, pPackage, NULL);
	if ( sDirName ) {
		xrtFree(sDirName);
	}
	return pPackage;
}

bool PS_ManagerWriteGeneratedFile(str sRootPath, const XAdminGeneratedFile* pFile)
{
	str sFullPath = NULL;
	str sDirPath = NULL;
	size_t iSize;
	bool bOK = FALSE;

	if ( (sRootPath == NULL) || (pFile == NULL) || !PS_ManagerIsSafeRelativePath(pFile->relative_path) || (pFile->data == NULL) ) {
		return FALSE;
	}

	sFullPath = xrtPathJoin(2, sRootPath, (str)pFile->relative_path);
	if ( sFullPath == NULL ) {
		return FALSE;
	}

	sDirPath = xrtPathGetDir(sFullPath, 0);
	if ( sDirPath ) {
		xrtDirCreateAll(sDirPath);
		xrtFree(sDirPath);
	}

	iSize = (pFile->size > 0) ? pFile->size : strlen((const char*)pFile->data);
	bOK = xrtFilePutAll(sFullPath, (ptr)pFile->data, iSize) >= 0;
	xrtFree(sFullPath);
	return bOK;
}

bool PS_ManagerGeneratedSpecHasFile(const XAdminGeneratedPluginSpec* spec, const char* sRelPath)
{
	if ( (spec == NULL) || (sRelPath == NULL) || (spec->files == NULL) ) {
		return FALSE;
	}

	for ( size_t i = 0; i < spec->file_count; i++ ) {
		const XAdminGeneratedFile* pFile = &spec->files[i];
		if ( pFile->relative_path && (strcmp(pFile->relative_path, sRelPath) == 0) ) {
			return TRUE;
		}
	}
	return FALSE;
}

bool PS_ManagerWriteGeneratedManifest(str sRootPath, const XAdminGeneratedPluginSpec* spec)
{
	xvalue tblManifest = NULL;
	xvalue tblRuntime = NULL;
	xvalue tblBuild = NULL;
	xvalue tblCompat = NULL;
	xvalue arrSources = NULL;
	xvalue arrIncludeDirs = NULL;
	xvalue arrLibraryDirs = NULL;
	xvalue arrLibraries = NULL;
	xvalue arrDefines = NULL;
	xvalue tblDependencies = NULL;
	xvalue tblContributes = NULL;
	bool bOK = FALSE;
	str sManifestPath = NULL;
	const char* sEntry;

	if ( (sRootPath == NULL) || (spec == NULL) || !PS_ManagerIsValidXid(spec->xid) ) {
		return FALSE;
	}

	sEntry = (spec->entry && spec->entry[0]) ? spec->entry : "main.c";
	tblManifest = xvoCreateTable();
	tblRuntime = xvoCreateTable();
	tblBuild = xvoCreateTable();
	tblCompat = xvoCreateTable();
	arrSources = xvoCreateArray();
	arrIncludeDirs = xvoCreateArray();
	arrLibraryDirs = xvoCreateArray();
	arrLibraries = xvoCreateArray();
	arrDefines = xvoCreateArray();
	tblDependencies = xvoCreateTable();
	tblContributes = xvoCreateTable();
	if ( !tblManifest || !tblRuntime || !tblBuild || !tblCompat || !arrSources || !arrIncludeDirs || !arrLibraryDirs || !arrLibraries || !arrDefines || !tblDependencies || !tblContributes ) {
		goto cleanup;
	}

	xvoTableSetInt(tblManifest, "formatVersion", 13, 3);
	xvoTableSetText(tblManifest, "xid", 3, (str)spec->xid, 0, FALSE);
	xvoTableSetText(tblManifest, "name", 4, (str)(spec->xid ? spec->xid : ""), 0, FALSE);
	xvoTableSetText(tblManifest, "title", 5, (str)((spec->title && spec->title[0]) ? spec->title : spec->xid), 0, FALSE);
	xvoTableSetText(tblManifest, "description", 11, "", 0, FALSE);
	xvoTableSetText(tblManifest, "version", 7, (str)((spec->version && spec->version[0]) ? spec->version : "1.0.0"), 0, FALSE);
	xvoTableSetText(tblManifest, "author", 6, "generated", 0, FALSE);
	xvoTableSetText(tblManifest, "kind", 4, "singleton", 0, FALSE);

	xvoTableSetText(tblRuntime, "compiler", 8, "tcc", 0, FALSE);
	xvoTableSetText(tblRuntime, "language", 8, "c", 0, FALSE);
	xvoTableSetValue(tblManifest, "runtime", 7, tblRuntime, TRUE);

	xvoTableSetText(tblBuild, "entry", 5, (str)sEntry, 0, FALSE);
	xvoArrayAppendText(arrSources, (str)sEntry, 0, FALSE);
	xvoTableSetValue(tblBuild, "sources", 7, arrSources, TRUE);
	xvoTableSetValue(tblBuild, "includeDirs", 11, arrIncludeDirs, TRUE);
	xvoTableSetValue(tblBuild, "libraryDirs", 11, arrLibraryDirs, TRUE);
	xvoTableSetValue(tblBuild, "libraries", 9, arrLibraries, TRUE);
	xvoArrayAppendText(arrDefines, "XADMIN_PLUGIN=1", 0, FALSE);
	xvoTableSetValue(tblBuild, "defines", 7, arrDefines, TRUE);
	xvoTableSetValue(tblManifest, "build", 5, tblBuild, TRUE);

	xvoTableSetText(tblCompat, "minHostVersion", 14, "3.0.0", 0, FALSE);
	xvoTableSetText(tblCompat, "maxHostVersion", 14, "4.0.0", 0, FALSE);
	xvoTableSetInt(tblCompat, "abiVersion", 10, XADMIN_ABI_VERSION);
	xvoTableSetValue(tblManifest, "compat", 6, tblCompat, TRUE);

	xvoTableSetValue(tblManifest, "capabilities", 12, xvoCreateArray(), TRUE);
	xvoTableSetValue(tblDependencies, "plugins", 7, xvoCreateArray(), TRUE);
	xvoTableSetValue(tblDependencies, "services", 8, xvoCreateArray(), TRUE);
	xvoTableSetValue(tblDependencies, "features", 8, xvoCreateArray(), TRUE);
	xvoTableSetValue(tblManifest, "dependencies", 12, tblDependencies, TRUE);
	xvoTableSetValue(tblContributes, "menus", 5, xvoCreateArray(), TRUE);
	xvoTableSetValue(tblContributes, "routes", 6, xvoCreateArray(), TRUE);
	xvoTableSetValue(tblContributes, "hooks", 5, xvoCreateArray(), TRUE);
	xvoTableSetValue(tblContributes, "events", 6, xvoCreateArray(), TRUE);
	xvoTableSetValue(tblManifest, "contributes", 11, tblContributes, TRUE);
	xvoTableSetBool(tblManifest, "multiInstance", 13, FALSE);
	xvoTableSetText(tblManifest, "defaultConfig", 13, "config.defaults.json", 0, FALSE);
	xvoTableSetText(tblManifest, "configSchema", 12, "config.schema.json", 0, FALSE);

	sManifestPath = xrtPathJoin(2, sRootPath, "plugin.json");
	if ( sManifestPath ) {
		bOK = xrtStringifyJSON_File(sManifestPath, tblManifest, TRUE) >= 0;
	}

cleanup:
	if ( sManifestPath ) {
		xrtFree(sManifestPath);
	}
	if ( tblManifest ) {
		xvoUnref(tblManifest);
	}
	return bOK;
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
	PS_ManagerAssignInstancePaths(pPackage, pInstance);
	if ( !PS_StorageLoadInstanceState(pInstance) ) {
		PS_StorageSaveInstance(pInstance);
	} else {
		PS_ManagerAssignInstancePaths(pPackage, pInstance);
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

	pPackage = PS_ManagerDiscoverPackagePath(sPath, TRUE);
	xrtFree(sName);
	(void)pPackage;
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

bool PluginSystem_Generate(const XAdminGeneratedPluginSpec* spec)
{
	PluginSystemPackage* pPackage;
	PluginSystemInstance* pInstance;
	str sRootPath = NULL;
	bool bExists;

	if ( (G_PluginSystem == NULL) || (spec == NULL) || !PS_ManagerIsValidXid(spec->xid) ) {
		return FALSE;
	}

	sRootPath = xrtPathJoin(2, G_PluginSystem->sPluginRootPath, (str)spec->xid);
	if ( sRootPath == NULL ) {
		return FALSE;
	}

	xrtDirCreateAll(sRootPath);
	for ( size_t i = 0; i < spec->file_count; i++ ) {
		if ( !PS_ManagerWriteGeneratedFile(sRootPath, &spec->files[i]) ) {
			xrtFree(sRootPath);
			return FALSE;
		}
	}
	if ( !PS_ManagerGeneratedSpecHasFile(spec, "plugin.json") ) {
		if ( !PS_ManagerWriteGeneratedManifest(sRootPath, spec) ) {
			xrtFree(sRootPath);
			return FALSE;
		}
	}

	bExists = (PluginSystem_FindPackage((str)spec->xid) != NULL);
	pPackage = PS_ManagerDiscoverPackagePath(sRootPath, TRUE);
	xrtFree(sRootPath);
	if ( pPackage == NULL ) {
		return FALSE;
	}

	if ( spec->auto_enable ) {
		pInstance = PS_GetDefaultInstance(pPackage);
		if ( pInstance && pInstance->bEnabled && bExists ) {
			return PluginSystem_Reload((str)spec->xid);
		}
		return PluginSystem_Enable((str)spec->xid);
	}

	return TRUE;
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
	G_PluginSystem->lstServices = xrtListCreate(sizeof(ptr), 0);
	G_PluginSystem->lstServiceSnapshots = xrtListCreate(sizeof(ptr), 0);
	G_PluginSystem->lstEvents = xrtListCreate(sizeof(ptr), 0);
	G_PluginSystem->lstEventSnapshots = xrtListCreate(sizeof(ptr), 0);
	G_PluginSystem->lstHooks = xrtListCreate(sizeof(ptr), 0);
	G_PluginSystem->lstHookSnapshots = xrtListCreate(sizeof(ptr), 0);
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
		}
		for ( int i = 0; i < xrtListCount(G_PluginSystem->lstPackages); i++ ) {
			PluginSystemPackage* pPackage = xrtListGetPtr(G_PluginSystem->lstPackages, i);
			PluginSystemInstance* pInstance = PS_GetDefaultInstance(pPackage);
			if ( pInstance ) {
				PS_RuntimeForceDrainInstance(pInstance);
			}
		}
		for ( int i = 0; i < xrtListCount(G_PluginSystem->lstPackages); i++ ) {
			PluginSystemPackage* pPackage = xrtListGetPtr(G_PluginSystem->lstPackages, i);
			PS_DestroyPackage(pPackage);
			xrtListSetPtr(G_PluginSystem->lstPackages, i, NULL, NULL);
		}
		xrtListDestroy(G_PluginSystem->lstPackages);
	}
	if ( G_PluginSystem->lstServices ) {
		for ( int i = 0; i < xrtListCount(G_PluginSystem->lstServices); i++ ) {
			PluginSystemServiceRegistration* pRegistration = xrtListGetPtr(G_PluginSystem->lstServices, i);
			if ( pRegistration ) {
				PS_ServiceFreeRegistration(pRegistration);
				xrtListSetPtr(G_PluginSystem->lstServices, i, NULL, NULL);
			}
		}
		xrtListDestroy(G_PluginSystem->lstServices);
		G_PluginSystem->lstServices = NULL;
	}
	if ( G_PluginSystem->lstServiceSnapshots ) {
		for ( int i = 0; i < xrtListCount(G_PluginSystem->lstServiceSnapshots); i++ ) {
			ptr pSnapshot = xrtListGetPtr(G_PluginSystem->lstServiceSnapshots, i);
			if ( pSnapshot ) {
				xrtFree(pSnapshot);
				xrtListSetPtr(G_PluginSystem->lstServiceSnapshots, i, NULL, NULL);
			}
		}
		xrtListDestroy(G_PluginSystem->lstServiceSnapshots);
		G_PluginSystem->lstServiceSnapshots = NULL;
	}
	G_PluginSystem->ppPublishedServices = NULL;
	if ( G_PluginSystem->lstEvents ) {
		for ( int i = 0; i < xrtListCount(G_PluginSystem->lstEvents); i++ ) {
			PluginSystemEventRegistration* pRegistration = xrtListGetPtr(G_PluginSystem->lstEvents, i);
			if ( pRegistration ) {
				PS_EventFreeRegistration(pRegistration);
				xrtListSetPtr(G_PluginSystem->lstEvents, i, NULL, NULL);
			}
		}
		xrtListDestroy(G_PluginSystem->lstEvents);
		G_PluginSystem->lstEvents = NULL;
	}
	if ( G_PluginSystem->lstEventSnapshots ) {
		for ( int i = 0; i < xrtListCount(G_PluginSystem->lstEventSnapshots); i++ ) {
			ptr pSnapshot = xrtListGetPtr(G_PluginSystem->lstEventSnapshots, i);
			if ( pSnapshot ) {
				xrtFree(pSnapshot);
				xrtListSetPtr(G_PluginSystem->lstEventSnapshots, i, NULL, NULL);
			}
		}
		xrtListDestroy(G_PluginSystem->lstEventSnapshots);
		G_PluginSystem->lstEventSnapshots = NULL;
	}
	G_PluginSystem->ppPublishedEvents = NULL;
	if ( G_PluginSystem->lstHooks ) {
		for ( int i = 0; i < xrtListCount(G_PluginSystem->lstHooks); i++ ) {
			PluginSystemHookRegistration* pRegistration = xrtListGetPtr(G_PluginSystem->lstHooks, i);
			if ( pRegistration ) {
				PS_HookFreeRegistration(pRegistration);
				xrtListSetPtr(G_PluginSystem->lstHooks, i, NULL, NULL);
			}
		}
		xrtListDestroy(G_PluginSystem->lstHooks);
		G_PluginSystem->lstHooks = NULL;
	}
	if ( G_PluginSystem->lstHookSnapshots ) {
		for ( int i = 0; i < xrtListCount(G_PluginSystem->lstHookSnapshots); i++ ) {
			ptr pSnapshot = xrtListGetPtr(G_PluginSystem->lstHookSnapshots, i);
			if ( pSnapshot ) {
				xrtFree(pSnapshot);
				xrtListSetPtr(G_PluginSystem->lstHookSnapshots, i, NULL, NULL);
			}
		}
		xrtListDestroy(G_PluginSystem->lstHookSnapshots);
		G_PluginSystem->lstHookSnapshots = NULL;
	}
	G_PluginSystem->ppPublishedHooks = NULL;

	PS_StorageUnit();
	PS_FreeString(&G_PluginSystem->sPluginRootPath);
	PS_FreeString(&G_PluginSystem->sDataPath);
	xrtFree(G_PluginSystem);
	G_PluginSystem = NULL;
}

#endif
