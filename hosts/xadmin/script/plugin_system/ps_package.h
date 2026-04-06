#ifndef XADMIN_PLUGIN_SYSTEM_PACKAGE_H
#define XADMIN_PLUGIN_SYSTEM_PACKAGE_H

typedef struct {
	const char* sRootPath;
	size_t iRootLen;
	xpkObject objPack;
	xpkWriteOptions tWriteOptions;
	int iFileCount;
	str sError;
} PS_PackageExportContext;

typedef struct {
	bool bHasPluginJson;
	int iFileCount;
	str sError;
} PS_PackageInspectContext;

typedef struct {
	xpkObject objPack;
	const char* sExtractRoot;
	int iFileCount;
	str sError;
} PS_PackageExtractContext;

static void PS_PackageSetError(str* psError, const char* sMessage)
{
	if ( psError == NULL ) {
		return;
	}
	if ( *psError ) {
		xrtFree(*psError);
		*psError = NULL;
	}
	if ( sMessage && sMessage[0] ) {
		*psError = xrtCopyStr((str)sMessage, 0);
	}
}

static void PS_PackageSetErrorTake(str* psError, str sMessage)
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
	*psError = sMessage;
}

static str PS_PackageBuildXpkError(xpkObject objPack, const char* sAction, const char* sPath)
{
	const char* sLastError = xpkLastErrorMessage(objPack);

	if ( sLastError == NULL || sLastError[0] == '\0' ) {
		sLastError = "xpack error";
	}
	if ( sPath && sPath[0] ) {
		return xrtFormat("%s: %s (%s)", sAction ? sAction : "xpack", sLastError, sPath);
	}
	return xrtFormat("%s: %s", sAction ? sAction : "xpack", sLastError);
}

static str PS_PackageMakeTempPath(const char* sPrefix, const char* sXid, const char* sExt)
{
	str sSeed;
	str sFileName;
	str sPath;
	const char* sPrefixText;
	const char* sXidText;
	const char* sSeedText;
	const char* sExtText;

	if ( TempPath == NULL ) {
		return NULL;
	}

	sSeed = xrtMakeXIDS();
	sPrefixText = (sPrefix && sPrefix[0]) ? sPrefix : "plugin_pkg";
	sXidText = (sXid && sXid[0]) ? sXid : "pkg";
	sSeedText = (sSeed && sSeed[0]) ? (const char*)sSeed : "tmp";
	sExtText = (sExt && sExt[0]) ? sExt : "";
	sFileName = xrtFormat("%s_%s_%s%s",
		sPrefixText,
		sXidText,
		sSeedText,
		sExtText);
	sPath = sFileName ? xrtPathJoin(2, TempPath, sFileName) : NULL;
	if ( sSeed ) {
		xrtFree(sSeed);
	}
	if ( sFileName ) {
		xrtFree(sFileName);
	}
	return sPath;
}

static str PS_PackageInfoPathDup(const xpkFileInfoPath* pInfo)
{
	size_t iLen = 0;

	if ( pInfo == NULL ) {
		return NULL;
	}

	while ( iLen < XPK_PATH_BYTES && pInfo->pathBytes[iLen] != '\0' ) {
		iLen++;
	}
	return xrtCopyStr((str)pInfo->pathBytes, iLen);
}

static str PS_PackageNormalizeRelPath(const char* sInput)
{
	size_t iLen;
	size_t iOut = 0;
	size_t iSegmentStart = 0;
	size_t iSegmentLen = 0;
	bool bLastSlash = FALSE;
	str sOut;

	if ( sInput == NULL || sInput[0] == '\0' ) {
		return NULL;
	}

	if ( sInput[0] == '/' || sInput[0] == '\\' ) {
		return NULL;
	}

	iLen = strlen(sInput);
	sOut = xrtMalloc(iLen + 1);
	if ( sOut == NULL ) {
		return NULL;
	}

	for ( size_t i = 0; i < iLen; i++ ) {
		unsigned char ch = (unsigned char)sInput[i];
		if ( ch < 32u ) {
			xrtFree(sOut);
			return NULL;
		}
		if ( ch == ':' ) {
			xrtFree(sOut);
			return NULL;
		}
		if ( ch == '/' || ch == '\\' ) {
			if ( iOut == 0 || bLastSlash ) {
				xrtFree(sOut);
				return NULL;
			}
			if ( (iSegmentLen == 1 && sOut[iSegmentStart] == '.')
				|| (iSegmentLen == 2 && sOut[iSegmentStart] == '.' && sOut[iSegmentStart + 1] == '.') ) {
				xrtFree(sOut);
				return NULL;
			}
			sOut[iOut++] = '/';
			bLastSlash = TRUE;
			iSegmentStart = iOut;
			iSegmentLen = 0;
			continue;
		}

		sOut[iOut++] = (char)ch;
		bLastSlash = FALSE;
		iSegmentLen++;
	}

	if ( iOut == 0 || bLastSlash ) {
		xrtFree(sOut);
		return NULL;
	}
	if ( (iSegmentLen == 1 && sOut[iSegmentStart] == '.')
		|| (iSegmentLen == 2 && sOut[iSegmentStart] == '.' && sOut[iSegmentStart + 1] == '.') ) {
		xrtFree(sOut);
		return NULL;
	}

	sOut[iOut] = '\0';
	return sOut;
}

static str PS_PackageToNativeRelPath(const char* sRelPath)
{
	str sPath;

	if ( sRelPath == NULL || sRelPath[0] == '\0' ) {
		return NULL;
	}

	sPath = xrtCopyStr((str)sRelPath, 0);
	if ( sPath == NULL ) {
		return NULL;
	}

	for ( str p = sPath; *p; p++ ) {
		if ( *p == '/' || *p == '\\' ) {
#if defined(_WIN32) || defined(_WIN64)
			*p = '\\';
#else
			*p = '/';
#endif
		}
	}
	return sPath;
}

static int PS_PackageExportScanProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
{
	PS_PackageExportContext* pCtx = (PS_PackageExportContext*)Param;
	const char* sRelPath;
	str sPackagePath = NULL;

	(void)pData;
	if ( pCtx == NULL || sPath == NULL ) {
		return TRUE;
	}
	if ( bDir ) {
		return FALSE;
	}

	if ( iSize == 0 ) {
		iSize = strlen(sPath);
	}
	if ( iSize < pCtx->iRootLen || strncmp(sPath, pCtx->sRootPath, pCtx->iRootLen) != 0 ) {
		PS_PackageSetErrorTake(&pCtx->sError, xrtFormat("\xE5\xAF\xBC\xE5\x87\xBA\xE8\xB7\xAF\xE5\xBE\x84\xE8\xB6\x8A\xE8\xBF\x87\xE4\xBA\x86\xE6\x8F\x92\xE4\xBB\xB6\xE6\xA0\xB9\xE7\x9B\xAE\xE5\xBD\x95: %s", sPath));
		return TRUE;
	}

	sRelPath = sPath + pCtx->iRootLen;
	if ( *sRelPath == '/' || *sRelPath == '\\' ) {
		sRelPath++;
	} else if ( *sRelPath != '\0' ) {
		PS_PackageSetErrorTake(&pCtx->sError, xrtFormat("\xE6\x97\xA0\xE6\xB3\x95\xE6\x9E\x84\xE5\xBB\xBA\xE6\x89\x93\xE5\x8C\x85\xE8\xB7\xAF\xE5\xBE\x84: %s", sPath));
		return TRUE;
	}
	if ( sRelPath[0] == '\0' ) {
		return FALSE;
	}

	sPackagePath = PS_PackageNormalizeRelPath(sRelPath);
	if ( sPackagePath == NULL ) {
		PS_PackageSetErrorTake(&pCtx->sError, xrtFormat("\xE6\x8F\x92\xE4\xBB\xB6\xE6\x96\x87\xE4\xBB\xB6\xE8\xB7\xAF\xE5\xBE\x84\xE4\xB8\x8D\xE5\xAE\x89\xE5\x85\xA8: %s", sRelPath));
		return TRUE;
	}
	if ( xpkPathAddFile(pCtx->objPack, sPackagePath, sPath, &pCtx->tWriteOptions) != XPK_OK ) {
		PS_PackageSetErrorTake(&pCtx->sError, PS_PackageBuildXpkError(pCtx->objPack, "\xE6\xB7\xBB\xE5\x8A\xA0\xE6\x89\x93\xE5\x8C\x85\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB1\xE8\xB4\xA5", sPackagePath));
		xrtFree(sPackagePath);
		return TRUE;
	}

	pCtx->iFileCount++;
	xrtFree(sPackagePath);
	return FALSE;
}

static int PS_PackageInspectProc(xpkObject objPack, uint32_t iPos, const void* pInfo, void* pUserData)
{
	const xpkFileInfoPath* pPathInfo = (const xpkFileInfoPath*)pInfo;
	PS_PackageInspectContext* pCtx = (PS_PackageInspectContext*)pUserData;
	str sRawPath = NULL;
	str sRelPath = NULL;

	(void)objPack;
	(void)iPos;
	if ( pCtx == NULL || pPathInfo == NULL ) {
		return XPK_ERR_PARAM;
	}

	sRawPath = PS_PackageInfoPathDup(pPathInfo);
	sRelPath = PS_PackageNormalizeRelPath(sRawPath);
	if ( sRelPath == NULL ) {
		PS_PackageSetErrorTake(&pCtx->sError, xrtFormat("\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE8\xB7\xAF\xE5\xBE\x84\xE4\xB8\x8D\xE5\xAE\x89\xE5\x85\xA8: %s", sRawPath ? (const char*)sRawPath : "(null)"));
		if ( sRawPath ) {
			xrtFree(sRawPath);
		}
		return XPK_ERR_PARAM;
	}

	if ( strcmp(sRelPath, "plugin.json") == 0 ) {
		pCtx->bHasPluginJson = TRUE;
	}
	pCtx->iFileCount++;

	xrtFree(sRawPath);
	xrtFree(sRelPath);
	return XPK_OK;
}

static int PS_PackageExtractProc(xpkObject objPack, uint32_t iPos, const void* pInfo, void* pUserData)
{
	const xpkFileInfoPath* pPathInfo = (const xpkFileInfoPath*)pInfo;
	PS_PackageExtractContext* pCtx = (PS_PackageExtractContext*)pUserData;
	str sRawPath = NULL;
	str sRelPath = NULL;
	str sNativeRelPath = NULL;
	str sDestPath = NULL;
	str sDestDir = NULL;

	(void)iPos;
	if ( pCtx == NULL || pPathInfo == NULL ) {
		return XPK_ERR_PARAM;
	}

	sRawPath = PS_PackageInfoPathDup(pPathInfo);
	sRelPath = PS_PackageNormalizeRelPath(sRawPath);
	if ( sRelPath == NULL ) {
		PS_PackageSetErrorTake(&pCtx->sError, xrtFormat("\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE8\xB7\xAF\xE5\xBE\x84\xE4\xB8\x8D\xE5\xAE\x89\xE5\x85\xA8: %s", sRawPath ? (const char*)sRawPath : "(null)"));
		goto Fail;
	}

	sNativeRelPath = PS_PackageToNativeRelPath(sRelPath);
	if ( sNativeRelPath == NULL ) {
		PS_PackageSetError(&pCtx->sError, "\xE6\x97\xA0\xE6\xB3\x95\xE6\x9E\x84\xE5\xBB\xBA\xE7\x9B\xAE\xE6\xA0\x87\xE8\xB7\xAF\xE5\xBE\x84");
		goto Fail;
	}
	sDestPath = xrtPathJoin(2, (str)pCtx->sExtractRoot, sNativeRelPath);
	if ( sDestPath == NULL ) {
		PS_PackageSetError(&pCtx->sError, "\xE6\x97\xA0\xE6\xB3\x95\xE6\x9E\x84\xE5\xBB\xBA\xE7\x9B\xAE\xE6\xA0\x87\xE8\xB7\xAF\xE5\xBE\x84");
		goto Fail;
	}
	sDestDir = xrtPathGetDir(sDestPath, 0);
	if ( sDestDir && !xrtDirCreateAll(sDestDir) ) {
		PS_PackageSetErrorTake(&pCtx->sError, xrtFormat("\xE5\x88\x9B\xE5\xBB\xBA\xE6\x8F\x92\xE4\xBB\xB6\xE7\x9B\xAE\xE5\xBD\x95\xE5\xA4\xB1\xE8\xB4\xA5: %s", sDestDir));
		goto Fail;
	}
	if ( xpkPathReadToFile(objPack, sRawPath, sDestPath) != XPK_OK ) {
		PS_PackageSetErrorTake(&pCtx->sError, PS_PackageBuildXpkError(objPack, "\xE8\xA7\xA3\xE5\x8C\x85\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB1\xE8\xB4\xA5", sRawPath));
		goto Fail;
	}

	pCtx->iFileCount++;
	if ( sDestDir ) {
		xrtFree(sDestDir);
	}
	if ( sDestPath ) {
		xrtFree(sDestPath);
	}
	if ( sNativeRelPath ) {
		xrtFree(sNativeRelPath);
	}
	if ( sRelPath ) {
		xrtFree(sRelPath);
	}
	if ( sRawPath ) {
		xrtFree(sRawPath);
	}
	return XPK_OK;

Fail:
	if ( sDestDir ) {
		xrtFree(sDestDir);
	}
	if ( sDestPath ) {
		xrtFree(sDestPath);
	}
	if ( sNativeRelPath ) {
		xrtFree(sNativeRelPath);
	}
	if ( sRelPath ) {
		xrtFree(sRelPath);
	}
	if ( sRawPath ) {
		xrtFree(sRawPath);
	}
	return XPK_ERR_PARAM;
}

static bool PluginSystem_ExportPackageArchive(const char* sName, ptr* ppData, size_t* pOutSize, str* psFileName, str* psError)
{
	PluginSystemPackage* pPackage;
	xpkOpenOptions tOpenOptions;
	PS_PackageExportContext tCtx;
	str sArchivePath = NULL;
	ptr pArchiveData = NULL;
	size_t iArchiveSize = 0;
	xpkObject objPack = NULL;
	bool bOK = FALSE;

	if ( ppData ) {
		*ppData = NULL;
	}
	if ( pOutSize ) {
		*pOutSize = 0;
	}
	if ( psFileName ) {
		*psFileName = NULL;
	}
	PS_PackageSetError(psError, NULL);

	pPackage = PluginSystem_FindPackage((str)sName);
	if ( pPackage == NULL || pPackage->sRootPath == NULL || !xrtDirExists(pPackage->sRootPath) ) {
		PS_PackageSetError(psError, "\xE6\x9C\xAA\xE6\x89\xBE\xE5\x88\xB0\xE6\x8F\x92\xE4\xBB\xB6");
		return FALSE;
	}

	sArchivePath = PS_PackageMakeTempPath("plugin_export", PS_PackageKey(pPackage), ".xpk");
	if ( sArchivePath == NULL ) {
		PS_PackageSetError(psError, "\xE5\x88\x9B\xE5\xBB\xBA\xE4\xB8\xB4\xE6\x97\xB6\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE8\xB7\xAF\xE5\xBE\x84\xE5\xA4\xB1\xE8\xB4\xA5");
		return FALSE;
	}

	memset(&tOpenOptions, 0, sizeof(tOpenOptions));
	tOpenOptions.createIfMissing = 1;
	objPack = xpkOpen(sArchivePath, &tOpenOptions);
	if ( objPack == NULL ) {
		PS_PackageSetError(psError, "\xE5\x88\x9B\xE5\xBB\xBA\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB1\xE8\xB4\xA5");
		goto Exit;
	}
	if ( xpkSetPackType(objPack, XPK_PACK_LINUX) != XPK_OK ) {
		PS_PackageSetErrorTake(psError, PS_PackageBuildXpkError(objPack, "\xE8\xAE\xBE\xE7\xBD\xAE\xE6\x89\x93\xE5\x8C\x85\xE7\xB1\xBB\xE5\x9E\x8B\xE5\xA4\xB1\xE8\xB4\xA5", NULL));
		goto Exit;
	}
	if ( xpkSetDefaultComp(objPack, 15) != XPK_OK ) {
		PS_PackageSetErrorTake(psError, PS_PackageBuildXpkError(objPack, "\xE8\xAE\xBE\xE7\xBD\xAE\xE9\xBB\x98\xE8\xAE\xA4\xE5\x8E\x8B\xE7\xBC\xA9\xE7\xBA\xA7\xE5\x88\xAB\xE5\xA4\xB1\xE8\xB4\xA5", NULL));
		goto Exit;
	}
	if ( xpkSetMetaComp(objPack, 15) != XPK_OK ) {
		PS_PackageSetErrorTake(psError, PS_PackageBuildXpkError(objPack, "\xE8\xAE\xBE\xE7\xBD\xAE\xE5\x85\x83\xE6\x95\xB0\xE6\x8D\xAE\xE5\x8E\x8B\xE7\xBC\xA9\xE7\xBA\xA7\xE5\x88\xAB\xE5\xA4\xB1\xE8\xB4\xA5", NULL));
		goto Exit;
	}
	if ( xpkSetInfoComp(objPack, 15) != XPK_OK ) {
		PS_PackageSetErrorTake(psError, PS_PackageBuildXpkError(objPack, "\xE8\xAE\xBE\xE7\xBD\xAE\xE7\xB4\xA2\xE5\xBC\x95\xE5\x8E\x8B\xE7\xBC\xA9\xE7\xBA\xA7\xE5\x88\xAB\xE5\xA4\xB1\xE8\xB4\xA5", NULL));
		goto Exit;
	}

	memset(&tCtx, 0, sizeof(tCtx));
	tCtx.sRootPath = pPackage->sRootPath;
	tCtx.iRootLen = strlen(pPackage->sRootPath);
	tCtx.objPack = objPack;
	memset(&tCtx.tWriteOptions, 0, sizeof(tCtx.tWriteOptions));
	tCtx.tWriteOptions.compLevel = 15;

	xrtDirScan(pPackage->sRootPath, TRUE, PS_PackageExportScanProc, &tCtx);
	if ( tCtx.sError ) {
		PS_PackageSetErrorTake(psError, tCtx.sError);
		tCtx.sError = NULL;
		goto Exit;
	}
	if ( tCtx.iFileCount <= 0 ) {
		PS_PackageSetError(psError, "\xE6\x8F\x92\xE4\xBB\xB6\xE7\x9B\xAE\xE5\xBD\x95\xE4\xB8\xBA\xE7\xA9\xBA");
		goto Exit;
	}
	if ( xpkSave(objPack) != XPK_OK ) {
		PS_PackageSetErrorTake(psError, PS_PackageBuildXpkError(objPack, "\xE4\xBF\x9D\xE5\xAD\x98\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE5\xA4\xB1\xE8\xB4\xA5", NULL));
		goto Exit;
	}

	pArchiveData = xrtFileGetAll(sArchivePath, &iArchiveSize);
	if ( pArchiveData == NULL || iArchiveSize == 0 ) {
		PS_PackageSetError(psError, "\xE8\xAF\xBB\xE5\x8F\x96\xE5\xAF\xBC\xE5\x87\xBA\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE5\xA4\xB1\xE8\xB4\xA5");
		goto Exit;
	}

	if ( ppData ) {
		*ppData = pArchiveData;
		pArchiveData = NULL;
	}
	if ( pOutSize ) {
		*pOutSize = iArchiveSize;
	}
	if ( psFileName ) {
		if ( pPackage->sVersion && pPackage->sVersion[0] ) {
			*psFileName = xrtFormat("%s-%s.xpk", PS_PackageKey(pPackage), pPackage->sVersion);
		} else {
			*psFileName = xrtFormat("%s.xpk", PS_PackageKey(pPackage));
		}
	}

	bOK = TRUE;

Exit:
	if ( objPack ) {
		xpkClose(objPack);
	}
	if ( pArchiveData ) {
		xrtFree(pArchiveData);
	}
	if ( sArchivePath ) {
		xrtFileDelete(sArchivePath);
		xrtFree(sArchivePath);
	}
	return bOK;
}

static bool PluginSystem_ImportPackageBuffer(const void* pData, size_t iSize, bool bAutoEnable, str* psImportedXid, str* psError)
{
	str sArchivePath = NULL;
	str sExtractBasePath = NULL;
	str sExtractRootPath = NULL;
	str sTargetPath = NULL;
	str sBackupPath = NULL;
	str sImportedXid = NULL;
	PluginSystemPackage* pExisting = NULL;
	PluginSystemPackage* pPackage = NULL;
	xpkObject objPack = NULL;
	xpkOpenOptions tOpenOptions;
	xpkPackType iPackType = XPK_PACK_CORE;
	PS_PackageInspectContext tInspectCtx;
	PS_PackageExtractContext tExtractCtx;
	xvalue tblManifest = NULL;
	ptr pManifestData = NULL;
	uint64_t iManifestSize = 0;
	bool bHadTargetDir = FALSE;
	bool bWasEnabled = FALSE;
	bool bMovedTargetToBackup = FALSE;
	bool bMovedExtractToTarget = FALSE;
	bool bDiscoveredTarget = FALSE;
	bool bOK = FALSE;

	if ( psImportedXid ) {
		*psImportedXid = NULL;
	}
	PS_PackageSetError(psError, NULL);

	if ( pData == NULL || iSize == 0 || G_PluginSystem == NULL || G_PluginSystem->sPluginRootPath == NULL ) {
		PS_PackageSetError(psError, "\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE6\x95\xB0\xE6\x8D\xAE\xE6\x97\xA0\xE6\x95\x88");
		return FALSE;
	}

	sArchivePath = PS_PackageMakeTempPath("plugin_import", NULL, ".xpk");
	if ( sArchivePath == NULL ) {
		PS_PackageSetError(psError, "\xE5\x88\x9B\xE5\xBB\xBA\xE4\xB8\xB4\xE6\x97\xB6\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE8\xB7\xAF\xE5\xBE\x84\xE5\xA4\xB1\xE8\xB4\xA5");
		goto Exit;
	}
	if ( xrtFilePutAll(sArchivePath, (ptr)pData, iSize) == 0 && iSize > 0 ) {
		PS_PackageSetError(psError, "\xE4\xBF\x9D\xE5\xAD\x98\xE4\xB8\x8A\xE4\xBC\xA0\xE7\x9A\x84\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE5\xA4\xB1\xE8\xB4\xA5");
		goto Exit;
	}

	memset(&tOpenOptions, 0, sizeof(tOpenOptions));
	tOpenOptions.readonly = 1;
	objPack = xpkOpen(sArchivePath, &tOpenOptions);
	if ( objPack == NULL ) {
		PS_PackageSetError(psError, "\xE6\x89\x93\xE5\xBC\x80\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE5\xA4\xB1\xE8\xB4\xA5");
		goto Exit;
	}
	if ( xpkVerifyAll(objPack) != XPK_OK ) {
		PS_PackageSetErrorTake(psError, PS_PackageBuildXpkError(objPack, "\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE6\xA0\xA1\xE9\xAA\x8C\xE5\xA4\xB1\xE8\xB4\xA5", NULL));
		goto Exit;
	}
	if ( xpkGetPackType(objPack, &iPackType) != XPK_OK ) {
		PS_PackageSetErrorTake(psError, PS_PackageBuildXpkError(objPack, "\xE8\xAF\x86\xE5\x88\xAB\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE7\xB1\xBB\xE5\x9E\x8B\xE5\xA4\xB1\xE8\xB4\xA5", NULL));
		goto Exit;
	}
	if ( iPackType != XPK_PACK_LINUX && iPackType != XPK_PACK_WIN32 ) {
		PS_PackageSetError(psError, "\xE4\xB8\x8D\xE6\x94\xAF\xE6\x8C\x81\xE7\x9A\x84\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE7\xB1\xBB\xE5\x9E\x8B");
		goto Exit;
	}

	pManifestData = xpkPathReadToMemory(objPack, "plugin.json", &iManifestSize);
	if ( pManifestData == NULL || iManifestSize == 0 ) {
		PS_PackageSetError(psError, "\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE6\xA0\xB9\xE7\x9B\xAE\xE5\xBD\x95\xE7\xBC\xBA\xE5\xB0\x91 plugin.json");
		goto Exit;
	}
	tblManifest = xrtParseJSON((str)pManifestData, (size_t)iManifestSize);
	if ( tblManifest == NULL || xvoType(tblManifest) != XVO_DT_TABLE ) {
		PS_PackageSetError(psError, "plugin.json \xE6\x97\xA0\xE6\x95\x88");
		goto Exit;
	}

	sImportedXid = PS_ManifestTextDup(tblManifest, "xid", 3, NULL);
	if ( sImportedXid == NULL ) {
		sImportedXid = PS_ManifestTextDup(tblManifest, "id", 2, NULL);
	}
	if ( !PS_ManagerIsValidXid(sImportedXid) ) {
		PS_PackageSetError(psError, "\xE6\x8F\x92\xE4\xBB\xB6 xid \xE6\x97\xA0\xE6\x95\x88");
		goto Exit;
	}

	memset(&tInspectCtx, 0, sizeof(tInspectCtx));
	if ( xpkEach(objPack, PS_PackageInspectProc, &tInspectCtx) != XPK_OK ) {
		if ( tInspectCtx.sError ) {
			PS_PackageSetErrorTake(psError, tInspectCtx.sError);
			tInspectCtx.sError = NULL;
		} else {
			PS_PackageSetErrorTake(psError, PS_PackageBuildXpkError(objPack, "\xE6\xA3\x80\xE6\x9F\xA5\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE5\xA4\xB1\xE8\xB4\xA5", NULL));
		}
		goto Exit;
	}
	if ( !tInspectCtx.bHasPluginJson ) {
		PS_PackageSetError(psError, "\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE6\xA0\xB9\xE7\x9B\xAE\xE5\xBD\x95\xE7\xBC\xBA\xE5\xB0\x91 plugin.json");
		goto Exit;
	}
	if ( tInspectCtx.iFileCount <= 0 ) {
		PS_PackageSetError(psError, "\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE4\xB8\xBA\xE7\xA9\xBA");
		goto Exit;
	}

	sExtractBasePath = PS_PackageMakeTempPath("plugin_unpack", sImportedXid, "");
	if ( sExtractBasePath == NULL ) {
		PS_PackageSetError(psError, "\xE5\x88\x9B\xE5\xBB\xBA\xE4\xB8\xB4\xE6\x97\xB6\xE8\xA7\xA3\xE5\x8C\x85\xE7\x9B\xAE\xE5\xBD\x95\xE5\xA4\xB1\xE8\xB4\xA5");
		goto Exit;
	}
	sExtractRootPath = xrtPathJoin(2, sExtractBasePath, sImportedXid);
	if ( sExtractRootPath == NULL || !xrtDirCreateAll(sExtractRootPath) ) {
		PS_PackageSetError(psError, "\xE5\x88\x9B\xE5\xBB\xBA\xE8\xA7\xA3\xE5\x8C\x85\xE7\x9B\xAE\xE5\xBD\x95\xE5\xA4\xB1\xE8\xB4\xA5");
		goto Exit;
	}

	memset(&tExtractCtx, 0, sizeof(tExtractCtx));
	tExtractCtx.objPack = objPack;
	tExtractCtx.sExtractRoot = sExtractRootPath;
	if ( xpkEach(objPack, PS_PackageExtractProc, &tExtractCtx) != XPK_OK ) {
		if ( tExtractCtx.sError ) {
			PS_PackageSetErrorTake(psError, tExtractCtx.sError);
			tExtractCtx.sError = NULL;
		} else {
			PS_PackageSetErrorTake(psError, PS_PackageBuildXpkError(objPack, "\xE8\xA7\xA3\xE5\x8E\x8B\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE5\xA4\xB1\xE8\xB4\xA5", NULL));
		}
		goto Exit;
	}
	if ( tExtractCtx.iFileCount <= 0 ) {
		PS_PackageSetError(psError, "\xE6\x8F\x92\xE4\xBB\xB6\xE5\x8C\x85\xE4\xB8\xBA\xE7\xA9\xBA");
		goto Exit;
	}

	sTargetPath = xrtPathJoin(2, G_PluginSystem->sPluginRootPath, sImportedXid);
	if ( sTargetPath == NULL ) {
		PS_PackageSetError(psError, "\xE6\x97\xA0\xE6\xB3\x95\xE6\x9E\x84\xE5\xBB\xBA\xE6\x8F\x92\xE4\xBB\xB6\xE7\x9B\xAE\xE5\xBD\x95\xE8\xB7\xAF\xE5\xBE\x84");
		goto Exit;
	}
	pExisting = PluginSystem_FindPackage(sImportedXid);
	bWasEnabled = (pExisting != NULL && pExisting->bEnabled);
	bHadTargetDir = xrtDirExists(sTargetPath);
	if ( bHadTargetDir ) {
		sBackupPath = PS_PackageMakeTempPath("plugin_backup", sImportedXid, "");
		if ( sBackupPath == NULL ) {
			PS_PackageSetError(psError, "\xE5\x88\x9B\xE5\xBB\xBA\xE6\x8F\x92\xE4\xBB\xB6\xE5\xA4\x87\xE4\xBB\xBD\xE7\x9B\xAE\xE5\xBD\x95\xE5\xA4\xB1\xE8\xB4\xA5");
			goto Exit;
		}
		if ( xrtDirMove(sTargetPath, sBackupPath, FALSE) < 0 || !xrtDirExists(sBackupPath) ) {
			PS_PackageSetError(psError, "\xE5\xA4\x87\xE4\xBB\xBD\xE7\x8E\xB0\xE6\x9C\x89\xE6\x8F\x92\xE4\xBB\xB6\xE5\xA4\xB1\xE8\xB4\xA5");
			goto Exit;
		}
		bMovedTargetToBackup = TRUE;
	}

	if ( xrtDirMove(sExtractRootPath, sTargetPath, FALSE) < 0 || !xrtDirExists(sTargetPath) ) {
		PS_PackageSetError(psError, "\xE5\xAE\x89\xE8\xA3\x85\xE6\x8F\x92\xE4\xBB\xB6\xE6\x96\x87\xE4\xBB\xB6\xE5\xA4\xB1\xE8\xB4\xA5");
		goto Exit;
	}
	bMovedExtractToTarget = TRUE;

	pPackage = PS_ManagerDiscoverPackagePath(sTargetPath, TRUE);
	if ( pPackage == NULL ) {
		PS_PackageSetError(psError, "\xE5\xAF\xBC\xE5\x85\xA5\xE7\x9A\x84\xE6\x8F\x92\xE4\xBB\xB6\xE6\x97\xA0\xE6\x95\x88");
		goto Exit;
	}
	bDiscoveredTarget = TRUE;

	if ( bWasEnabled || bAutoEnable ) {
		bool bActivated;
		if ( bWasEnabled && pExisting != NULL ) {
			bActivated = PluginSystem_Reload(sImportedXid);
		} else {
			bActivated = PluginSystem_Enable(sImportedXid);
		}
		if ( !bActivated ) {
			PS_PackageSetErrorTake(psError, xrtFormat("\xE6\x8F\x92\xE4\xBB\xB6\xE6\x96\x87\xE4\xBB\xB6\xE5\xB7\xB2\xE5\xAF\xBC\xE5\x85\xA5\xEF\xBC\x8C\xE4\xBD\x86\xE5\x90\xAF\xE7\x94\xA8\xE5\xA4\xB1\xE8\xB4\xA5: %s", sImportedXid));
			goto Exit;
		}
	}

	if ( psImportedXid ) {
		*psImportedXid = xrtCopyStr(sImportedXid, 0);
	}
	bOK = TRUE;

Exit:
	if ( !bOK && bMovedExtractToTarget && !bDiscoveredTarget ) {
		xrtDirDelete(sTargetPath);
		if ( bMovedTargetToBackup && sBackupPath && xrtDirExists(sBackupPath) ) {
			xrtDirMove(sBackupPath, sTargetPath, FALSE);
			bMovedTargetToBackup = FALSE;
		}
	} else if ( bMovedTargetToBackup && sBackupPath && xrtDirExists(sBackupPath) ) {
		xrtDirDelete(sBackupPath);
		bMovedTargetToBackup = FALSE;
	}

	if ( pManifestData ) {
		xpkFree(pManifestData);
	}
	if ( tblManifest ) {
		xvoUnref(tblManifest);
	}
	if ( objPack ) {
		xpkClose(objPack);
	}
	if ( sArchivePath ) {
		xrtFileDelete(sArchivePath);
		xrtFree(sArchivePath);
	}
	if ( sExtractBasePath ) {
		if ( xrtDirExists(sExtractBasePath) ) {
			xrtDirDelete(sExtractBasePath);
		}
		xrtFree(sExtractBasePath);
	}
	if ( sExtractRootPath ) {
		xrtFree(sExtractRootPath);
	}
	if ( sTargetPath ) {
		xrtFree(sTargetPath);
	}
	if ( sBackupPath ) {
		if ( bMovedTargetToBackup && xrtDirExists(sBackupPath) ) {
			xrtDirDelete(sBackupPath);
		}
		xrtFree(sBackupPath);
	}
	if ( sImportedXid ) {
		xrtFree(sImportedXid);
	}
	return bOK;
}

#endif
