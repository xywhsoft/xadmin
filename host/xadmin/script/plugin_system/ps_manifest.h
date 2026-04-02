#ifndef XADMIN_PLUGIN_SYSTEM_MANIFEST_H
#define XADMIN_PLUGIN_SYSTEM_MANIFEST_H

#include "ps_types.h"

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
		tblData = xrtParseJSON_File(sPath);
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

	pPackage->tblManifest = xrtParseJSON_File(pPackage->sManifestPath);
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
	pPackage->bMultiInstance = xvoTableGetBool(pPackage->tblManifest, "multiInstance", 13);

	tblBuild = xvoTableGetValue(pPackage->tblManifest, "build", 5);
	if ( tblBuild ) {
		pPackage->sEntry = PS_ManifestTextDup(tblBuild, "entry", 5, NULL);
	}
	if ( pPackage->sEntry == NULL ) {
		pPackage->sEntry = PS_ManifestTextDup(pPackage->tblManifest, "entry", 5, "main.c");
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
