#ifndef XADMIN_PLUGIN_SYSTEM_MANIFEST_H
#define XADMIN_PLUGIN_SYSTEM_MANIFEST_H

#include "ps_types.h"

#define PS_MANIFEST_HOST_VERSION "4.0.0"

int PS_ManifestCompareVersion(const char* sLeft, const char* sRight)
{
	int iMajorLeft = 0, iMinorLeft = 0, iPatchLeft = 0;
	int iMajorRight = 0, iMinorRight = 0, iPatchRight = 0;

	if ( (sLeft == NULL) || (sRight == NULL) ) {
		return 0;
	}

	sscanf(sLeft, "%d.%d.%d", &iMajorLeft, &iMinorLeft, &iPatchLeft);
	sscanf(sRight, "%d.%d.%d", &iMajorRight, &iMinorRight, &iPatchRight);
	if ( iMajorLeft != iMajorRight ) return (iMajorLeft < iMajorRight) ? -1 : 1;
	if ( iMinorLeft != iMinorRight ) return (iMinorLeft < iMinorRight) ? -1 : 1;
	if ( iPatchLeft != iPatchRight ) return (iPatchLeft < iPatchRight) ? -1 : 1;
	return 0;
}

bool PS_ManifestCheckVersionRequirement(const char* sActualVersion, const char* sMinVersion, const char* sMaxVersion)
{
	if ( (sActualVersion == NULL) || (sActualVersion[0] == '\0') ) {
		return FALSE;
	}
	if ( sMinVersion && sMinVersion[0] && (PS_ManifestCompareVersion(sActualVersion, sMinVersion) < 0) ) {
		return FALSE;
	}
	if ( sMaxVersion && sMaxVersion[0] && (PS_ManifestCompareVersion(sActualVersion, sMaxVersion) > 0) ) {
		return FALSE;
	}
	return TRUE;
}

bool PS_ManifestValidateIdentity(PluginSystemPackage* pPackage, str sRootPath)
{
	str sDirName;
	bool bOK;

	if ( (pPackage == NULL) || (sRootPath == NULL) || (pPackage->sXid == NULL) || (pPackage->sXid[0] == '\0') ) {
		return FALSE;
	}

	sDirName = xrtPathGetName(sRootPath, 0);
	bOK = (sDirName != NULL) && (strcmp(sDirName, pPackage->sXid) == 0);
	if ( sDirName ) {
		xrtFree(sDirName);
	}
	return bOK;
}

bool PS_ManifestValidateCompat(PluginSystemPackage* pPackage)
{
	xvalue tblCompat;
	str sMinHostVersion;
	str sMaxHostVersion;
	int iAbiVersion;

	if ( (pPackage == NULL) || (pPackage->tblManifest == NULL) ) {
		return FALSE;
	}

	tblCompat = xvoTableGetValue(pPackage->tblManifest, "compat", 6);
	if ( (tblCompat == NULL) || (xvoType(tblCompat) != XVO_DT_TABLE) ) {
		return FALSE;
	}

	iAbiVersion = xvoTableGetInt(tblCompat, "abiVersion", 10);
	if ( iAbiVersion != XADMIN_ABI_VERSION ) {
		return FALSE;
	}

	sMinHostVersion = xvoTableGetText(tblCompat, "minHostVersion", 14);
	sMaxHostVersion = xvoTableGetText(tblCompat, "maxHostVersion", 14);
	return PS_ManifestCheckVersionRequirement(PS_MANIFEST_HOST_VERSION, sMinHostVersion, sMaxHostVersion);
}

bool PS_ManifestValidateRequiredFields(PluginSystemPackage* pPackage, xvalue tblBuild)
{
	if ( (pPackage == NULL) || (pPackage->tblManifest == NULL) || (tblBuild == NULL) || (xvoType(tblBuild) != XVO_DT_TABLE) ) {
		return FALSE;
	}
	if ( pPackage->iFormatVersion < 4 ) {
		return FALSE;
	}
	if ( (pPackage->sXid == NULL) || (pPackage->sXid[0] == '\0') ) {
		return FALSE;
	}
	if ( (pPackage->sName == NULL) || (pPackage->sName[0] == '\0') ) {
		return FALSE;
	}
	if ( (pPackage->sTitle == NULL) || (pPackage->sTitle[0] == '\0') ) {
		return FALSE;
	}
	if ( (pPackage->sVersion == NULL) || (pPackage->sVersion[0] == '\0') ) {
		return FALSE;
	}
	if ( (pPackage->sKind == NULL) || (pPackage->sKind[0] == '\0') ) {
		return FALSE;
	}
	if ( (pPackage->sEntry == NULL) || (pPackage->sEntry[0] == '\0') ) {
		return FALSE;
	}
	if ( xvoTableGetText(tblBuild, "entry", 5) == NULL ) {
		return FALSE;
	}
	return TRUE;
}

str PS_ManifestTextDup(xvalue tblData, const char* sKey, int iKeyLen, const char* sDefault)
{
	str sValue = tblData ? xvoTableGetText(tblData, sKey, iKeyLen) : NULL;
	if ( sValue && sValue[0] ) {
		return xrtCopyStr(sValue, 0);
	}
	return sDefault ? xrtCopyStr((str)sDefault, 0) : NULL;
}

xvalue PS_ManifestLoadJsonIfExists(str sRootPath, str sRelPath)
{
	str sPath;
	xvalue tblData = NULL;

	if ( (sRootPath == NULL) || (sRelPath == NULL) || (sRelPath[0] == '\0') ) {
		return NULL;
	}

	sPath = xrtPathJoin(2, sRootPath, sRelPath);
	if ( sPath && xrtFileExists(sPath) ) {
		tblData = PS_ValueParseJsonFileShared(sPath);
	}
	if ( sPath ) {
		xrtFree(sPath);
	}
	return tblData;
}

bool PS_LoadManifest(PluginSystemPackage* pPackage, str sRootPath)
{
	xvalue tblBuild;
	str sDefaultConfig;
	str sConfigSchema;

	if ( (pPackage == NULL) || (sRootPath == NULL) ) {
		return FALSE;
	}

	pPackage->sRootPath = xrtCopyStr(sRootPath, 0);
	pPackage->sManifestPath = xrtPathJoin(2, sRootPath, "plugin.json");
	if ( (pPackage->sManifestPath == NULL) || !xrtFileExists(pPackage->sManifestPath) ) {
		return FALSE;
	}

	pPackage->tblManifest = PS_ValueParseJsonFileShared(pPackage->sManifestPath);
	if ( pPackage->tblManifest == NULL ) {
		return FALSE;
	}

	pPackage->iFormatVersion = xvoTableGetInt(pPackage->tblManifest, "formatVersion", 13);
	pPackage->iSort = xvoTableGetInt(pPackage->tblManifest, "sort", 4);
	pPackage->sXid = PS_ManifestTextDup(pPackage->tblManifest, "xid", 3, NULL);
	if ( pPackage->sXid == NULL ) {
		pPackage->sXid = PS_ManifestTextDup(pPackage->tblManifest, "id", 2, NULL);
	}
	pPackage->sName = PS_ManifestTextDup(pPackage->tblManifest, "name", 4, pPackage->sXid);
	pPackage->sTitle = PS_ManifestTextDup(pPackage->tblManifest, "title", 5, pPackage->sName);
	pPackage->sDescription = PS_ManifestTextDup(pPackage->tblManifest, "description", 11, "");
	pPackage->sVersion = PS_ManifestTextDup(pPackage->tblManifest, "version", 7, "0.0.0");
	pPackage->sAuthor = PS_ManifestTextDup(pPackage->tblManifest, "author", 6, "");
	pPackage->sKind = PS_ManifestTextDup(pPackage->tblManifest, "kind", 4, "singleton");

	tblBuild = xvoTableGetValue(pPackage->tblManifest, "build", 5);
	if ( tblBuild ) {
		pPackage->sEntry = PS_ManifestTextDup(tblBuild, "entry", 5, NULL);
	}
	if ( pPackage->sEntry == NULL ) {
		pPackage->sEntry = PS_ManifestTextDup(pPackage->tblManifest, "entry", 5, "main.c");
	}
	if ( !PS_ManifestValidateRequiredFields(pPackage, tblBuild) ) {
		return FALSE;
	}
	if ( !PS_ManifestValidateIdentity(pPackage, sRootPath) ) {
		return FALSE;
	}
	if ( !PS_ManifestValidateCompat(pPackage) ) {
		return FALSE;
	}

	sDefaultConfig = xvoTableGetText(pPackage->tblManifest, "defaultConfig", 13);
	if ( sDefaultConfig == NULL ) {
		sDefaultConfig = "config.defaults.json";
	}
	pPackage->tblDefaultConfig = PS_ManifestLoadJsonIfExists(sRootPath, sDefaultConfig);

	sConfigSchema = xvoTableGetText(pPackage->tblManifest, "configSchema", 12);
	if ( sConfigSchema == NULL ) {
		sConfigSchema = "config.schema.json";
	}
	pPackage->tblConfigSchema = PS_ManifestLoadJsonIfExists(sRootPath, sConfigSchema);

	return TRUE;
}

#endif
