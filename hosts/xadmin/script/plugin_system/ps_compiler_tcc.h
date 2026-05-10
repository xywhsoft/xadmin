#ifndef XADMIN_PLUGIN_SYSTEM_COMPILER_TCC_H
#define XADMIN_PLUGIN_SYSTEM_COMPILER_TCC_H

#include "ps_types.h"

typedef void (*XAdminPluginSetGlobalDataProc)(int idx, void* ptr);

void PS_TCCGenerationErrorHandler(void* pOpaque, const char* sMsg)
{
	PluginSystemGeneration* pGeneration = (PluginSystemGeneration*)pOpaque;
	str sJoined;

	if ( sMsg == NULL || sMsg[0] == '\0' ) {
		return;
	}

	printf("        [PluginSystem][TCC] %s\n", sMsg);
	if ( pGeneration == NULL ) {
		return;
	}

	if ( pGeneration->sErrorMessage == NULL ) {
		pGeneration->sErrorMessage = xrtCopyStr(sMsg, 0);
		return;
	}
	if ( strlen(pGeneration->sErrorMessage) > 2048 ) {
		return;
	}
	sJoined = xrtFormat("%s\n%s", pGeneration->sErrorMessage, sMsg);
	if ( sJoined ) {
		xrtFree(pGeneration->sErrorMessage);
		pGeneration->sErrorMessage = sJoined;
	}
}

void PS_TCCAddPathIfExists(TCCState* pTcc, str sRootPath, str sRelPath, bool bInclude)
{
	str sPath;

	if ( (pTcc == NULL) || (sRootPath == NULL) || (sRelPath == NULL) || (sRelPath[0] == '\0') ) {
		return;
	}

	sPath = xrtPathJoin(2, sRootPath, sRelPath);
	if ( sPath ) {
		if ( xrtDirExists(sPath) ) {
			if ( bInclude ) {
				tcc_add_include_path(pTcc, sPath);
			} else {
				tcc_add_library_path(pTcc, sPath);
			}
		}
		xrtFree(sPath);
	}
}

void PS_TCCAddPublicInclude(TCCState* pTcc)
{
	str sPath;

	if ( (pTcc == NULL) || (AppPath == NULL) ) {
		return;
	}

	sPath = xrtPathJoin(3, AppPath, "script", "plugin_system");
	if ( sPath ) {
		if ( xrtDirExists(sPath) ) {
			tcc_add_include_path(pTcc, sPath);
		}
		xrtFree(sPath);
	}
}

void PS_TCCApplyBuildDefines(TCCState* pTcc, xvalue arrDefines)
{
	int iCount;

	if ( (pTcc == NULL) || (arrDefines == NULL) ) {
		return;
	}

	iCount = xvoArrayItemCount(arrDefines);
	for ( int i = 0; i < iCount; i++ ) {
		xvalue objItem = xvoArrayGetValue(arrDefines, i);
		str sDefine = PS_ValueToStringDup(objItem);
		if ( sDefine && sDefine[0] ) {
			str pEq = strchr(sDefine, '=');
			if ( pEq ) {
				*pEq = '\0';
				tcc_define_symbol(pTcc, sDefine, pEq + 1);
			} else {
				tcc_define_symbol(pTcc, sDefine, "1");
			}
		}
		if ( sDefine ) {
			xrtFree(sDefine);
		}
	}
}

void PS_TCCApplyBuildLibraries(TCCState* pTcc, xvalue arrLibraries)
{
	int iCount;

	if ( (pTcc == NULL) || (arrLibraries == NULL) ) {
		return;
	}

	iCount = xvoArrayItemCount(arrLibraries);
	for ( int i = 0; i < iCount; i++ ) {
		xvalue objItem = xvoArrayGetValue(arrLibraries, i);
		str sLibrary = PS_ValueToStringDup(objItem);
		if ( sLibrary && sLibrary[0] ) {
			tcc_add_library(pTcc, sLibrary);
		}
		if ( sLibrary ) {
			xrtFree(sLibrary);
		}
	}
}

void PS_TCCApplyRelativePathArray(TCCState* pTcc, str sRootPath, xvalue arrPaths, bool bInclude)
{
	int iCount;

	if ( (pTcc == NULL) || (sRootPath == NULL) || (arrPaths == NULL) ) {
		return;
	}

	iCount = xvoArrayItemCount(arrPaths);
	for ( int i = 0; i < iCount; i++ ) {
		xvalue objItem = xvoArrayGetValue(arrPaths, i);
		str sRelPath = PS_ValueToStringDup(objItem);
		if ( sRelPath && sRelPath[0] ) {
			PS_TCCAddPathIfExists(pTcc, sRootPath, sRelPath, bInclude);
		}
		if ( sRelPath ) {
			xrtFree(sRelPath);
		}
	}
}

bool PS_TCCCompileSourceFile(TCCState* pTcc, str sRootPath, str sRelPath)
{
	str sFullPath;
	int iRet;

	if ( (pTcc == NULL) || (sRootPath == NULL) || (sRelPath == NULL) || (sRelPath[0] == '\0') ) {
		return FALSE;
	}

	sFullPath = xrtPathJoin(2, sRootPath, sRelPath);
	if ( (sFullPath == NULL) || !xrtFileExists(sFullPath) ) {
		if ( sFullPath ) {
			xrtFree(sFullPath);
		}
		return FALSE;
	}

	iRet = tcc_add_file(pTcc, sFullPath);
	xrtFree(sFullPath);
	return (iRet >= 0);
}

bool PS_CompileGeneration(PluginSystemPackage* pPackage, PluginSystemGeneration* pGeneration)
{
	TCCState* pTcc;
	xvalue tblManifest = NULL;
	xvalue tblBuild;
	xvalue arrSources;
	str sGenerationName;
	str sErrorMessage;
	bool bCompiled = FALSE;
	const XAdminPluginDescriptor* (*procGetDescriptor)(void);
	XAdminPluginSetGlobalDataProc procSetGlobalData = NULL;

	if ( (pPackage == NULL) || (pGeneration == NULL) || (pPackage->sRootPath == NULL) ) {
		return FALSE;
	}

	sGenerationName = xrtFormat("%s_%u", PS_PackageKey(pPackage) ? PS_PackageKey(pPackage) : pPackage->sName, pGeneration->iGeneration);
	pGeneration->sWorkDir = xrtPathJoin(3, G_PluginSystem->sDataPath, "generation", sGenerationName);
	xrtFree(sGenerationName);
	if ( pGeneration->sWorkDir ) {
		xrtDirCreateAll(pGeneration->sWorkDir);
	}

	pTcc = xsCreateTCC(pPackage->sRootPath);
	if ( pTcc == NULL ) {
		pGeneration->sErrorMessage = xrtCopyStr("failed to create TCC state", 0);
		pGeneration->iState = PS_GENERATION_STATE_FAILED;
		return FALSE;
	}
	tcc_set_error_func(pTcc, pGeneration, PS_TCCGenerationErrorHandler);

	PS_TCCRegisterPluginSdkSymbols(pTcc);
	PS_TCCAddPublicInclude(pTcc);
	PS_TCCAddPathIfExists(pTcc, pPackage->sRootPath, "inc", TRUE);
	PS_TCCAddPathIfExists(pTcc, pPackage->sRootPath, "lib", FALSE);
	PS_TCCAddPathIfExists(pTcc, pPackage->sRootPath, "src", TRUE);
	PS_TCCAddPathIfExists(pTcc, pPackage->sRootPath, "include", TRUE);

	tblManifest = PS_PackageManifestRef(pPackage);
	tblBuild = tblManifest ? xvoTableGetValue(tblManifest, "build", 5) : NULL;
	if ( tblBuild ) {
		PS_TCCApplyRelativePathArray(pTcc, pPackage->sRootPath, xvoTableGetValue(tblBuild, "includeDirs", 11), TRUE);
		PS_TCCApplyRelativePathArray(pTcc, pPackage->sRootPath, xvoTableGetValue(tblBuild, "libraryDirs", 11), FALSE);
		PS_TCCApplyBuildDefines(pTcc, xvoTableGetValue(tblBuild, "defines", 7));
		PS_TCCApplyBuildLibraries(pTcc, xvoTableGetValue(tblBuild, "libraries", 9));
	}

	arrSources = tblBuild ? xvoTableGetValue(tblBuild, "sources", 7) : NULL;
	if ( arrSources && (xvoArrayItemCount(arrSources) > 0) ) {
		int iCount = xvoArrayItemCount(arrSources);
		bCompiled = TRUE;
		for ( int i = 0; i < iCount; i++ ) {
			xvalue objItem = xvoArrayGetValue(arrSources, i);
			str sSource = PS_ValueToStringDup(objItem);
			bool bOK = (sSource != NULL) ? PS_TCCCompileSourceFile(pTcc, pPackage->sRootPath, sSource) : FALSE;
			if ( sSource ) {
				xrtFree(sSource);
			}
			if ( !bOK ) {
				bCompiled = FALSE;
				break;
			}
		}
	} else {
		bCompiled = PS_TCCCompileSourceFile(pTcc, pPackage->sRootPath, pPackage->sEntry ? pPackage->sEntry : (str)"main.c");
	}

	if ( !bCompiled ) {
		if ( pGeneration->sErrorMessage == NULL ) {
			pGeneration->sErrorMessage = xrtCopyStr("failed to compile plugin sources", 0);
		}
		pGeneration->iState = PS_GENERATION_STATE_FAILED;
		if ( tblManifest ) {
			xvoUnref(tblManifest);
		}
		xsDestroyTCC(pTcc);
		return FALSE;
	}

	if ( tcc_relocate(pTcc) < 0 ) {
		if ( pGeneration->sErrorMessage ) {
			str sRelocateError = xrtFormat("%s\nfailed to relocate plugin image", pGeneration->sErrorMessage);
			if ( sRelocateError ) {
				xrtFree(pGeneration->sErrorMessage);
				pGeneration->sErrorMessage = sRelocateError;
			}
		} else {
			pGeneration->sErrorMessage = xrtCopyStr("failed to relocate plugin image", 0);
		}
		pGeneration->iState = PS_GENERATION_STATE_FAILED;
		if ( tblManifest ) {
			xvoUnref(tblManifest);
		}
		xsDestroyTCC(pTcc);
		return FALSE;
	}

	procSetGlobalData = (XAdminPluginSetGlobalDataProc)tcc_get_symbol(pTcc, "XAdmin_PluginSetGlobalData");
	if ( procSetGlobalData ) {
		procSetGlobalData(XADMIN_GLOBAL_MAIN_DB, G_DB);
		procSetGlobalData(XADMIN_GLOBAL_OPTION_TABLE, G_Option);
		procSetGlobalData(XADMIN_GLOBAL_PLUGIN_XID, pPackage->sXid);
		procSetGlobalData(XADMIN_GLOBAL_PLUGIN_ROOT_PATH, pPackage->sRootPath);
		procSetGlobalData(XADMIN_GLOBAL_PLUGIN_DATA_PATH, pPackage->sDataPath);
		procSetGlobalData(XADMIN_GLOBAL_PLUGIN_PRIVATE_DB_PATH, pPackage->sPrivateDbPath);
	}

	procGetDescriptor = (const XAdminPluginDescriptor* (*)(void))tcc_get_symbol(pTcc, "XAdmin_GetPluginDescriptor");
	if ( procGetDescriptor == NULL ) {
		pGeneration->sErrorMessage = xrtCopyStr("XAdmin_GetPluginDescriptor not found", 0);
		pGeneration->iState = PS_GENERATION_STATE_FAILED;
		if ( tblManifest ) {
			xvoUnref(tblManifest);
		}
		xsDestroyTCC(pTcc);
		return FALSE;
	}

	pGeneration->pDescriptor = procGetDescriptor();
	if ( pGeneration->pDescriptor == NULL ) {
		pGeneration->sErrorMessage = xrtCopyStr("plugin descriptor returned NULL", 0);
		pGeneration->iState = PS_GENERATION_STATE_FAILED;
		if ( tblManifest ) {
			xvoUnref(tblManifest);
		}
		xsDestroyTCC(pTcc);
		return FALSE;
	}

	if ( pGeneration->pDescriptor->abi_version != XADMIN_ABI_VERSION ) {
		sErrorMessage = xrtFormat("abi version mismatch: plugin=%u host=%u", pGeneration->pDescriptor->abi_version, XADMIN_ABI_VERSION);
		pGeneration->sErrorMessage = sErrorMessage;
		pGeneration->iState = PS_GENERATION_STATE_FAILED;
		if ( tblManifest ) {
			xvoUnref(tblManifest);
		}
		xsDestroyTCC(pTcc);
		return FALSE;
	}

	if ( tblManifest ) {
		xvoUnref(tblManifest);
	}
	pGeneration->pTccState = pTcc;
	pGeneration->sCompileHash = xrtFormat("%s:%u", pPackage->sVersion ? pPackage->sVersion : (str)"0.0.0", pGeneration->iGeneration);
	pGeneration->iState = PS_GENERATION_STATE_COMPILED;
	return TRUE;
}

#endif
