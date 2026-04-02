#ifndef XADMIN_PLUGIN_SYSTEM_MANAGER_H
#define XADMIN_PLUGIN_SYSTEM_MANAGER_H

#include "ps_runtime.h"
#include "ps_manifest.h"

bool PS_ManagerEnsureDefaultInstance(PluginSystemPackage* pPackage);
int PS_ManagerLoadPackageInstances(PluginSystemPackage* pPackage);
PluginSystemPackage* PS_ManagerFindPackageByInstanceRef(const char* sInstanceRef);
PluginSystemInstance* PS_ManagerFindPackageInstanceByRef(PluginSystemPackage* pPackage, const char* sInstanceRef);
xvalue PS_ManagerBuildInstanceData(PluginSystemPackage* pPackage, PluginSystemInstance* pInstance);
bool PS_ManagerValidateConfigSchema(PluginSystemPackage* pPackage, xvalue tblConfig, str* psError);

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
	PluginSystemPackage* pInstancePackage;

	if ( pPackage ) {
		return PS_GetDefaultInstance(pPackage);
	}
	pInstancePackage = PS_ManagerFindPackageByInstanceRef((const char*)sName);
	return pInstancePackage ? PS_ManagerFindPackageInstanceByRef(pInstancePackage, (const char*)sName) : NULL;
}

PluginSystemPackage* PS_ManagerFindPackageByInstanceRef(const char* sInstanceRef)
{
	if ( (G_PluginSystem == NULL) || (G_PluginSystem->lstPackages == NULL) || (sInstanceRef == NULL) || (sInstanceRef[0] == '\0') ) {
		return NULL;
	}

	for ( int i = 0; i < xrtListCount(G_PluginSystem->lstPackages); i++ ) {
		PluginSystemPackage* pPackage = xrtListGetPtr(G_PluginSystem->lstPackages, i);
		if ( PS_ManagerFindPackageInstanceByRef(pPackage, sInstanceRef) ) {
			return pPackage;
		}
	}
	return NULL;
}

PluginSystemInstance* PS_ManagerFindPackageInstanceByRef(PluginSystemPackage* pPackage, const char* sInstanceRef)
{
	PluginSystemInstance* pInstance;

	if ( (pPackage == NULL) || (sInstanceRef == NULL) || (sInstanceRef[0] == '\0') ) {
		return NULL;
	}

	pInstance = PS_FindInstanceById(pPackage, sInstanceRef);
	if ( pInstance ) {
		return pInstance;
	}
	return PS_FindInstanceByName(pPackage, sInstanceRef);
}

xvalue PS_ManagerBuildInstanceData(PluginSystemPackage* pPackage, PluginSystemInstance* pInstance)
{
	xvalue tblData;

	if ( (pPackage == NULL) || (pInstance == NULL) ) {
		return NULL;
	}

	tblData = xvoCreateTable();
	xvoTableSetText(tblData, "packageId", 9, PS_PackageKey(pPackage) ? PS_PackageKey(pPackage) : (str)"", 0, FALSE);
	xvoTableSetText(tblData, "xid", 3, pPackage->sXid ? pPackage->sXid : (str)"", 0, FALSE);
	xvoTableSetText(tblData, "instanceId", 10, pInstance->sInstanceId ? pInstance->sInstanceId : (str)"", 0, FALSE);
	xvoTableSetText(tblData, "instanceName", 12, pInstance->sInstanceName ? pInstance->sInstanceName : (str)"", 0, FALSE);
	xvoTableSetText(tblData, "mountPath", 9, pInstance->sMountPath ? pInstance->sMountPath : (str)"", 0, FALSE);
	xvoTableSetText(tblData, "dataPath", 8, pInstance->sDataPath ? pInstance->sDataPath : (str)"", 0, FALSE);
	xvoTableSetText(tblData, "privateDbPath", 13, pInstance->sPrivateDbPath ? pInstance->sPrivateDbPath : (str)"", 0, FALSE);
	xvoTableSetBool(tblData, "enabled", 7, pInstance->bEnabled);
	xvoTableSetBool(tblData, "loaded", 6, pInstance->pActiveGeneration != NULL);
	xvoTableSetText(tblData, "status", 6, (str)PS_InstanceStatusText(pInstance->iStatus), 0, FALSE);
	xvoTableSetInt(tblData, "generation", 10, (int)pInstance->iActiveGeneration);
	xvoTableSetInt(tblData, "createTime", 10, pInstance->iCreateTime);
	xvoTableSetInt(tblData, "updateTime", 10, pInstance->iUpdateTime);
	if ( pInstance->tblConfig ) {
		xvalue tblConfig = PS_ValueDup(pInstance->tblConfig);
		if ( tblConfig ) {
			xvoTableSetValue(tblData, "settings", 8, tblConfig, TRUE);
		}
	}
	return tblData;
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
	xvoTableSetInt(tblData, "sort", 4, pPackage->iSort);
	if ( pPackage->tblConfigSchema ) {
		xvalue tblSchema = PS_ValueDup(pPackage->tblConfigSchema);
		if ( tblSchema ) {
			xvoTableSetValue(tblData, "configSchema", 12, tblSchema, TRUE);
		}
	}

	if ( pInstance ) {
		xvoTableSetText(tblData, "dataPath", 8, pInstance->sDataPath ? pInstance->sDataPath : (str)"", 0, FALSE);
		xvoTableSetText(tblData, "privateDbPath", 13, pInstance->sPrivateDbPath ? pInstance->sPrivateDbPath : (str)"", 0, FALSE);
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

bool PS_ManagerIsValidInstanceName(const char* sInstanceName)
{
	size_t iLen;

	if ( (sInstanceName == NULL) || (sInstanceName[0] == '\0') ) {
		return FALSE;
	}

	iLen = strlen(sInstanceName);
	if ( (iLen <= 0) || (iLen > 96) ) {
		return FALSE;
	}

	for ( size_t i = 0; i < iLen; i++ ) {
		char ch = sInstanceName[i];
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

	sDataRoot = xrtPathJoin(4, AppPath, "data", "plugin", sXid);
	if ( sDataRoot == NULL ) {
		return;
	}

	PS_FreeString(&pInstance->sDataPath);
	pInstance->sDataPath = xrtCopyStr(sDataRoot, 0);
	PS_FreeString(&pInstance->sPrivateDbPath);
	pInstance->sPrivateDbPath = xrtPathJoin(2, sDataRoot, "plugin.db");

	if ( pInstance->sDataPath ) {
		xrtDirCreateAll(pInstance->sDataPath);
	}
	xrtFree(sDataRoot);
}

str PS_ManagerBuildConfigPath(PluginSystemInstance* pInstance)
{
	if ( (pInstance == NULL) || (pInstance->sDataPath == NULL) || (pInstance->sDataPath[0] == '\0') ) {
		return NULL;
	}
	return xrtPathJoin(2, pInstance->sDataPath, "config.json");
}

void PS_ManagerLoadConfigFile(PluginSystemPackage* pPackage, PluginSystemInstance* pInstance)
{
	str sConfigPath;
	xvalue tblConfig = NULL;

	(void)pPackage;

	if ( (pPackage == NULL) || (pInstance == NULL) ) {
		return;
	}

	sConfigPath = PS_ManagerBuildConfigPath(pInstance);
	if ( sConfigPath && xrtFileExists(sConfigPath) ) {
		tblConfig = xrtParseJSON_File(sConfigPath);
	}
	if ( tblConfig && (xvoType(tblConfig) == XVO_DT_TABLE) ) {
		if ( pInstance->tblConfig ) {
			xvoUnref(pInstance->tblConfig);
		}
		pInstance->tblConfig = tblConfig;
		tblConfig = NULL;
	} else if ( tblConfig ) {
		xvoUnref(tblConfig);
	}
	if ( sConfigPath ) {
		xrtFree(sConfigPath);
	}
}

bool PS_ManagerSaveConfigFile(PluginSystemInstance* pInstance)
{
	str sConfigPath;
	xvalue tblConfig = NULL;
	int iRet;

	if ( (pInstance == NULL) || (pInstance->sDataPath == NULL) ) {
		return FALSE;
	}

	xrtDirCreateAll(pInstance->sDataPath);
	sConfigPath = PS_ManagerBuildConfigPath(pInstance);
	if ( sConfigPath == NULL ) {
		return FALSE;
	}

	tblConfig = pInstance->tblConfig ? pInstance->tblConfig : xvoCreateTable();
	iRet = xrtStringifyJSON_File(sConfigPath, tblConfig, TRUE);
	if ( tblConfig && (tblConfig != pInstance->tblConfig) ) {
		xvoUnref(tblConfig);
	}
	xrtFree(sConfigPath);
	return iRet >= 0;
}

str PS_ManagerBuildInstanceId(PluginSystemPackage* pPackage, const char* sInstanceName)
{
	if ( (pPackage == NULL) || (sInstanceName == NULL) || (sInstanceName[0] == '\0') || (PS_PackageKey(pPackage) == NULL) ) {
		return NULL;
	}

	if ( strcmp((const char*)PS_PackageKey(pPackage), sInstanceName) == 0 ) {
		return xrtCopyStr(PS_PackageKey(pPackage), 0);
	}

	return xrtFormat("%s.%s", PS_PackageKey(pPackage), sInstanceName);
}

void PS_ManagerSetSchemaError(str* psError, str sMessage)
{
	if ( psError == NULL ) {
		if ( sMessage ) {
			xrtFree(sMessage);
		}
		return;
	}

	if ( *psError ) {
		xrtFree(*psError);
	}
	*psError = sMessage ? sMessage : xrtCopyStr("invalid config schema", 0);
}

str PS_ManagerBuildSchemaPath(const char* sBasePath, const char* sKey, int iIndex)
{
	if ( sKey && sKey[0] ) {
		if ( sBasePath && sBasePath[0] ) {
			return xrtFormat("%s.%s", sBasePath, sKey);
		}
		return xrtCopyStr((str)sKey, 0);
	}

	if ( iIndex >= 0 ) {
		if ( sBasePath && sBasePath[0] ) {
			return xrtFormat("%s[%d]", sBasePath, iIndex);
		}
		return xrtFormat("[%d]", iIndex);
	}

	return sBasePath ? xrtCopyStr((str)sBasePath, 0) : xrtCopyStr("config", 0);
}

bool PS_ManagerSchemaTypeMatches(xvalue objValue, const char* sType)
{
	int iType;

	if ( (sType == NULL) || (sType[0] == '\0') ) {
		return TRUE;
	}
	if ( strcmp(sType, "null") == 0 ) {
		return (objValue == NULL) || (xvoType(objValue) == XVO_DT_NULL);
	}
	if ( objValue == NULL ) {
		return FALSE;
	}

	iType = xvoType(objValue);
	if ( strcmp(sType, "object") == 0 ) {
		return iType == XVO_DT_TABLE;
	}
	if ( strcmp(sType, "array") == 0 ) {
		return iType == XVO_DT_ARRAY;
	}
	if ( strcmp(sType, "string") == 0 ) {
		return iType == XVO_DT_TEXT;
	}
	if ( strcmp(sType, "boolean") == 0 ) {
		return iType == XVO_DT_BOOL;
	}
	if ( strcmp(sType, "integer") == 0 ) {
		return iType == XVO_DT_INT;
	}
	if ( strcmp(sType, "number") == 0 ) {
		return (iType == XVO_DT_INT) || (iType == XVO_DT_FLOAT);
	}

	return TRUE;
}

bool PS_ManagerValidateConfigSchemaValue(xvalue tblSchema, xvalue objValue, const char* sPath, str* psError)
{
	str sType;
	xvalue tblProperties;
	xvalue arrRequired;
	xvalue objAdditional;
	xvalue tblItems;
	bool bNeedObject;
	bool bNeedArray;
	bool bAllowAdditional = TRUE;

	if ( tblSchema == NULL ) {
		return TRUE;
	}

	sType = xvoTableGetText(tblSchema, "type", 4);
	tblProperties = xvoTableGetValue(tblSchema, "properties", 10);
	arrRequired = xvoTableGetValue(tblSchema, "required", 8);
	objAdditional = xvoTableGetValue(tblSchema, "additionalProperties", 20);
	tblItems = xvoTableGetValue(tblSchema, "items", 5);

	if ( sType && !PS_ManagerSchemaTypeMatches(objValue, sType) ) {
		str sLabel = PS_ManagerBuildSchemaPath(sPath, NULL, -1);
		PS_ManagerSetSchemaError(psError, xrtFormat("%s expects type %s", sLabel ? (const char*)sLabel : "config", sType));
		if ( sLabel ) {
			xrtFree(sLabel);
		}
		return FALSE;
	}

	bNeedObject = ((sType && (strcmp(sType, "object") == 0)) || (tblProperties != NULL) || (arrRequired != NULL) || (objAdditional != NULL));
	if ( bNeedObject ) {
		if ( !PS_ManagerSchemaTypeMatches(objValue, "object") ) {
			str sLabel = PS_ManagerBuildSchemaPath(sPath, NULL, -1);
			PS_ManagerSetSchemaError(psError, xrtFormat("%s expects type object", sLabel ? (const char*)sLabel : "config"));
			if ( sLabel ) {
				xrtFree(sLabel);
			}
			return FALSE;
		}

		if ( arrRequired && (xvoType(arrRequired) == XVO_DT_ARRAY) ) {
			for ( uint32 i = 0; i < xvoArrayItemCount(arrRequired); i++ ) {
				str sRequiredKey = xvoArrayGetText(arrRequired, i);
				if ( (sRequiredKey == NULL) || (sRequiredKey[0] == '\0') ) {
					continue;
				}
				if ( xvoTableGetValue(objValue, sRequiredKey, strlen(sRequiredKey)) == NULL ) {
					str sLabel = PS_ManagerBuildSchemaPath(sPath, sRequiredKey, -1);
					PS_ManagerSetSchemaError(psError, xrtFormat("%s is required", sLabel ? sLabel : sRequiredKey));
					if ( sLabel ) {
						xrtFree(sLabel);
					}
					return FALSE;
				}
			}
		}

		if ( objAdditional && (xvoType(objAdditional) == XVO_DT_BOOL) ) {
			bAllowAdditional = xvoGetBool(objAdditional);
		}

		if ( tblProperties && (xvoType(tblProperties) == XVO_DT_TABLE) ) {
			DICT_FOREACH(tblProperties->vTable, pKey, pUnused) {
				xvalue tblPropertySchema = xvoTableGetValue(tblProperties, (str)pKey->Key, pKey->KeyLen);
				xvalue objPropertyValue = xvoTableGetValue(objValue, (str)pKey->Key, pKey->KeyLen);
				str sChildPath = NULL;

				(void)pUnused;
				if ( objPropertyValue == NULL ) {
					continue;
				}

				sChildPath = PS_ManagerBuildSchemaPath(sPath, (const char*)pKey->Key, -1);
				if ( !PS_ManagerValidateConfigSchemaValue(tblPropertySchema, objPropertyValue, sChildPath, psError) ) {
					if ( sChildPath ) {
						xrtFree(sChildPath);
					}
					return FALSE;
				}
				if ( sChildPath ) {
					xrtFree(sChildPath);
				}
			}
		}

		if ( !bAllowAdditional && objValue && (xvoType(objValue) == XVO_DT_TABLE) ) {
			DICT_FOREACH(objValue->vTable, pKey, pUnused) {
				(void)pUnused;
				if ( (tblProperties == NULL) || (xvoTableGetValue(tblProperties, (str)pKey->Key, pKey->KeyLen) == NULL) ) {
					str sLabel = PS_ManagerBuildSchemaPath(sPath, (const char*)pKey->Key, -1);
					PS_ManagerSetSchemaError(psError, xrtFormat("%s is not allowed", sLabel ? (const char*)sLabel : (const char*)pKey->Key));
					if ( sLabel ) {
						xrtFree(sLabel);
					}
					return FALSE;
				}
			}
		}
	}

	bNeedArray = ((sType && (strcmp(sType, "array") == 0)) || (tblItems != NULL));
	if ( bNeedArray ) {
		if ( !PS_ManagerSchemaTypeMatches(objValue, "array") ) {
			str sLabel = PS_ManagerBuildSchemaPath(sPath, NULL, -1);
			PS_ManagerSetSchemaError(psError, xrtFormat("%s expects type array", sLabel ? (const char*)sLabel : "config"));
			if ( sLabel ) {
				xrtFree(sLabel);
			}
			return FALSE;
		}

		if ( tblItems ) {
			for ( uint32 i = 0; i < xvoArrayItemCount(objValue); i++ ) {
				xvalue objItem = xvoArrayGetValue(objValue, i);
				str sItemPath = PS_ManagerBuildSchemaPath(sPath, NULL, (int)i);
				if ( !PS_ManagerValidateConfigSchemaValue(tblItems, objItem, sItemPath, psError) ) {
					if ( sItemPath ) {
						xrtFree(sItemPath);
					}
					return FALSE;
				}
				if ( sItemPath ) {
					xrtFree(sItemPath);
				}
			}
		}
	}

	return TRUE;
}

bool PS_ManagerValidateConfigSchema(PluginSystemPackage* pPackage, xvalue tblConfig, str* psError)
{
	xvalue tblEffectiveConfig = tblConfig;

	if ( (pPackage == NULL) || (pPackage->tblConfigSchema == NULL) ) {
		return TRUE;
	}
	if ( tblEffectiveConfig == NULL ) {
		tblEffectiveConfig = xvoCreateTable();
		if ( tblEffectiveConfig == NULL ) {
			PS_ManagerSetSchemaError(psError, xrtCopyStr("config expects type object", 0));
			return FALSE;
		}
	}
	if ( !PS_ManagerValidateConfigSchemaValue(pPackage->tblConfigSchema, tblEffectiveConfig, "config", psError) ) {
		if ( tblEffectiveConfig != tblConfig ) {
			xvoUnref(tblEffectiveConfig);
		}
		return FALSE;
	}
	if ( tblEffectiveConfig != tblConfig ) {
		xvoUnref(tblEffectiveConfig);
	}
	return TRUE;
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
			if ( PS_GetInstanceCount(pExisting) <= 0 ) {
				PS_ManagerLoadPackageInstances(pExisting);
				PS_ManagerEnsureDefaultInstance(pExisting);
			}
		}
		PS_DestroyPackage(pPackage);
		if ( sDirName ) {
			xrtFree(sDirName);
		}
		return pExisting;
	}

	PS_StorageSavePackage(pPackage);
	PS_ManagerLoadPackageInstances(pPackage);
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

	xvoTableSetInt(tblManifest, "formatVersion", 13, 4);
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

	xvoTableSetText(tblCompat, "minHostVersion", 14, "4.0.0", 0, FALSE);
	xvoTableSetText(tblCompat, "maxHostVersion", 14, "5.0.0", 0, FALSE);
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

	if ( PS_GetInstanceCount(pPackage) > 0 ) {
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
	PS_ManagerLoadConfigFile(pPackage, pInstance);
	if ( pInstance->tblConfig == NULL ) {
		pInstance->tblConfig = pPackage->tblDefaultConfig ? PS_ValueDup(pPackage->tblDefaultConfig) : xvoCreateTable();
	}
	PS_ManagerSaveConfigFile(pInstance);

	iIndex = xrtListCount(pPackage->lstInstances);
	xrtListSetPtr(pPackage->lstInstances, iIndex, pInstance, NULL);
	return TRUE;
}

int PS_ManagerLoadPackageInstances(PluginSystemPackage* pPackage)
{
	(void)pPackage;
	return 0;
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
		for ( int j = 0; j < PS_GetInstanceCount(pPackage); j++ ) {
			PluginSystemInstance* pInstance = xrtListGetPtr(pPackage->lstInstances, j);
			printf("        [PluginSystem] AutoStart check: package=%s instance=%s enabled=%d status=%d active=%u\n",
				PS_PackageKey(pPackage) ? (const char*)PS_PackageKey(pPackage) : "(unknown)",
				(pInstance && pInstance->sInstanceId) ? (const char*)pInstance->sInstanceId : "(unknown)",
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

xvalue PluginSystem_GetInstances(str sName)
{
	PluginSystemPackage* pPackage = PluginSystem_FindPackage(sName);
	xvalue arrInstances = xvoCreateArray();

	if ( pPackage == NULL ) {
		return arrInstances;
	}

	for ( int i = 0; i < PS_GetInstanceCount(pPackage); i++ ) {
		PluginSystemInstance* pInstance = xrtListGetPtr(pPackage->lstInstances, i);
		xvalue tblInstance = PS_ManagerBuildInstanceData(pPackage, pInstance);
		if ( tblInstance ) {
			xvoArrayAppendValue(arrInstances, tblInstance, TRUE);
		}
	}
	return arrInstances;
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
	PluginSystemInstance* pInstance = PluginSystem_FindInstance(sName);
	PluginSystemPackage* pPackage = pInstance ? PluginSystem_FindPackage(pInstance->sPackageId) : PluginSystem_FindPackage(sName);
	xvalue tblNewConfig;
	xvalue tblOldConfig;
	str sSchemaError = NULL;

	if ( (pPackage == NULL) || (pInstance == NULL) ) {
		return FALSE;
	}

	tblNewConfig = tblSettings ? PS_ValueDup(tblSettings) : xvoCreateTable();
	if ( tblNewConfig == NULL ) {
		tblNewConfig = xvoCreateTable();
	}
	if ( !PS_ManagerValidateConfigSchema(pPackage, tblNewConfig, &sSchemaError) ) {
		printf("        [PluginSystem] Config schema validation failed: package=%s instance=%s reason=%s\n",
			PS_PackageKey(pPackage) ? (const char*)PS_PackageKey(pPackage) : "(unknown)",
			pInstance->sInstanceId ? (const char*)pInstance->sInstanceId : "(unknown)",
			sSchemaError ? (const char*)sSchemaError : "(unknown)");
		if ( sSchemaError ) {
			xrtFree(sSchemaError);
		}
		if ( tblNewConfig ) {
			xvoUnref(tblNewConfig);
		}
		return FALSE;
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
	return PS_ManagerSaveConfigFile(pInstance) && PS_StorageSaveInstance(pInstance);
}

bool PluginSystem_Enable(str sName)
{
	PluginSystemInstance* pInstance = PluginSystem_FindInstance(sName);
	PluginSystemPackage* pPackage = pInstance ? PluginSystem_FindPackage(pInstance->sPackageId) : PluginSystem_FindPackage(sName);
	str sSchemaError = NULL;
	if ( (pPackage == NULL) || (pInstance == NULL) ) {
		return FALSE;
	}
	if ( pInstance->pActiveGeneration ) {
		return TRUE;
	}
	if ( !PS_ManagerValidateConfigSchema(pPackage, pInstance->tblConfig, &sSchemaError) ) {
		printf("        [PluginSystem] Start blocked by invalid config: package=%s instance=%s reason=%s\n",
			PS_PackageKey(pPackage) ? (const char*)PS_PackageKey(pPackage) : "(unknown)",
			pInstance->sInstanceId ? (const char*)pInstance->sInstanceId : "(unknown)",
			sSchemaError ? (const char*)sSchemaError : "(unknown)");
		if ( sSchemaError ) {
			xrtFree(sSchemaError);
		}
		return FALSE;
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
	PluginSystemInstance* pInstance = PluginSystem_FindInstance(sName);
	PluginSystemPackage* pPackage = pInstance ? PluginSystem_FindPackage(pInstance->sPackageId) : PluginSystem_FindPackage(sName);
	str sSchemaError = NULL;
	if ( (pPackage == NULL) || (pInstance == NULL) ) {
		return FALSE;
	}
	if ( !PS_ManagerValidateConfigSchema(pPackage, pInstance->tblConfig, &sSchemaError) ) {
		printf("        [PluginSystem] Reload blocked by invalid config: package=%s instance=%s reason=%s\n",
			PS_PackageKey(pPackage) ? (const char*)PS_PackageKey(pPackage) : "(unknown)",
			pInstance->sInstanceId ? (const char*)pInstance->sInstanceId : "(unknown)",
			sSchemaError ? (const char*)sSchemaError : "(unknown)");
		if ( sSchemaError ) {
			xrtFree(sSchemaError);
		}
		return FALSE;
	}
	return PS_RuntimeReloadInstance(pPackage, pInstance);
}

bool PluginSystem_CreateInstance(str sName, str sInstanceName, str sMountPath)
{
	PluginSystemPackage* pPackage = PluginSystem_FindPackage(sName);
	PluginSystemInstance* pInstance;
	str sInstanceId = NULL;

	if ( (pPackage == NULL) || !PS_ManagerIsValidInstanceName((const char*)sInstanceName) ) {
		return FALSE;
	}
	if ( !pPackage->bMultiInstance && (PS_GetInstanceCount(pPackage) > 0) ) {
		return FALSE;
	}
	if ( PS_FindInstanceByName(pPackage, (const char*)sInstanceName) ) {
		return FALSE;
	}

	sInstanceId = PS_ManagerBuildInstanceId(pPackage, (const char*)sInstanceName);
	if ( (sInstanceId == NULL) || PS_FindInstanceById(pPackage, (const char*)sInstanceId) ) {
		if ( sInstanceId ) {
			xrtFree(sInstanceId);
		}
		return FALSE;
	}

	pInstance = PS_CreateInstance(sInstanceId, PS_PackageKey(pPackage));
	xrtFree(sInstanceId);
	if ( pInstance == NULL ) {
		return FALSE;
	}

	PS_FreeString(&pInstance->sInstanceName);
	pInstance->sInstanceName = xrtCopyStr(sInstanceName, 0);
	if ( sMountPath && sMountPath[0] ) {
		PS_FreeString(&pInstance->sMountPath);
		pInstance->sMountPath = xrtCopyStr(sMountPath, 0);
	}
	pInstance->bEnabled = FALSE;
	pInstance->bInstalled = FALSE;
	pInstance->iStatus = PS_INSTANCE_STATUS_DISABLED;
	pInstance->tblConfig = pPackage->tblDefaultConfig ? PS_ValueDup(pPackage->tblDefaultConfig) : xvoCreateTable();
	PS_ManagerAssignInstancePaths(pPackage, pInstance);
	if ( !PS_StorageSaveInstance(pInstance) ) {
		PS_DestroyInstance(pInstance);
		return FALSE;
	}

	xrtListSetPtr(pPackage->lstInstances, xrtListCount(pPackage->lstInstances), pInstance, NULL);
	return TRUE;
}

bool PluginSystem_DeleteInstance(str sInstanceId)
{
	PluginSystemPackage* pPackage = PS_ManagerFindPackageByInstanceRef((const char*)sInstanceId);
	PluginSystemInstance* pInstance;
	int iIndex = -1;

	if ( (pPackage == NULL) || (sInstanceId == NULL) || (sInstanceId[0] == '\0') ) {
		return FALSE;
	}

	pInstance = PS_FindInstanceById(pPackage, (const char*)sInstanceId);
	if ( pInstance == NULL ) {
		return FALSE;
	}
	if ( PS_GetInstanceCount(pPackage) <= 1 ) {
		return FALSE;
	}

	if ( pInstance->pActiveGeneration ) {
		PS_RuntimeStopInstance(pInstance);
	}
	PS_RuntimeForceDrainInstance(pInstance);
	if ( !PS_StorageDeleteInstance((const char*)pInstance->sInstanceId) ) {
		return FALSE;
	}

	for ( int i = 0; i < PS_GetInstanceCount(pPackage); i++ ) {
		if ( xrtListGetPtr(pPackage->lstInstances, i) == pInstance ) {
			iIndex = i;
			break;
		}
	}
	if ( iIndex < 0 ) {
		return FALSE;
	}

	xrtListRemovePtr(pPackage->lstInstances, iIndex);
	PS_DestroyInstance(pInstance);
	return TRUE;
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

	G_PluginSystem->lstPackages = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	G_PluginSystem->lstServices = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	G_PluginSystem->lstServiceSnapshots = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	G_PluginSystem->lstEvents = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	G_PluginSystem->lstEventSnapshots = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	G_PluginSystem->lstHooks = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	G_PluginSystem->lstHookSnapshots = xrtListCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
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
			printf("        [PluginSystem] Destroy package: %s\n", PS_PackageKey(pPackage) ? (const char*)PS_PackageKey(pPackage) : "(unknown)");
			for ( int j = 0; j < PS_GetInstanceCount(pPackage); j++ ) {
				PluginSystemInstance* pInstance = xrtListGetPtr(pPackage->lstInstances, j);
				if ( pInstance && pInstance->pActiveGeneration ) {
					PS_RuntimeStopInstance(pInstance);
				}
			}
		}
		for ( int i = 0; i < xrtListCount(G_PluginSystem->lstPackages); i++ ) {
			PluginSystemPackage* pPackage = xrtListGetPtr(G_PluginSystem->lstPackages, i);
			for ( int j = 0; j < PS_GetInstanceCount(pPackage); j++ ) {
				PluginSystemInstance* pInstance = xrtListGetPtr(pPackage->lstInstances, j);
				if ( pInstance ) {
					PS_RuntimeForceDrainInstance(pInstance);
				}
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
