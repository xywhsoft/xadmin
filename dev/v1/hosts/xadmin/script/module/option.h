
typedef struct OptionNamespaceCheckContext {
	str sNamespace;
	str sExcludeFileName;
	bool bFound;
} OptionNamespaceCheckContext;

typedef struct OptionListContext {
	xvalue arrFiles;
} OptionListContext;

static str Option_StrOrEmpty(str sText)
{
	return sText ? sText : (str)"";
}

static bool Option_IsSpaceChar(char c)
{
	return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

static xvalue Option_CreateSharedTableValue()
{
	xvalue pVal = xvoCreateTableEx(XRT_OBJMODE_SHARED);
	if ( pVal != NULL ) {
		XAdminValuePublishShared(pVal);
	}
	return pVal;
}

static xvalue Option_CreateSharedArrayValue()
{
	xvalue pVal = xvoCreateArrayEx(XRT_OBJMODE_SHARED);
	if ( pVal != NULL ) {
		XAdminValuePublishShared(pVal);
	}
	return pVal;
}

typedef struct OptionSharedCloneTableContext {
	xvalue tblSrc;
	xvalue tblDst;
} OptionSharedCloneTableContext;

static xvalue Option_CloneSharedValue(xvalue objSrc);

static bool Option_CloneSharedTableItemProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	OptionSharedCloneTableContext* pCtx = (OptionSharedCloneTableContext*)pArg;
	xvalue objSrcVal;
	xvalue objDstVal;

	(void)pVal;

	if ( (pCtx == NULL) || (pCtx->tblSrc == NULL) || (pCtx->tblDst == NULL) || (pKey == NULL) ) {
		return FALSE;
	}

	objSrcVal = xvoTableGetValue(pCtx->tblSrc, pKey->Key, pKey->KeyLen);
	objDstVal = Option_CloneSharedValue(objSrcVal);
	if ( objDstVal != NULL ) {
		xvoTableSetValue(pCtx->tblDst, pKey->Key, pKey->KeyLen, objDstVal, TRUE);
	}

	return FALSE;
}

static xvalue Option_CloneSharedValue(xvalue objSrc)
{
	xvalue objDst = NULL;
	int iType;

	if ( objSrc == NULL ) {
		return NULL;
	}

	iType = xvoType(objSrc);
	switch ( iType ) {
		case XVO_DT_NULL:
			return xvoCreateNull();
		case XVO_DT_BOOL:
			return xvoCreateBool(xvoGetBool(objSrc));
		case XVO_DT_INT:
			return xvoCreateInt(xvoGetInt(objSrc));
		case XVO_DT_FLOAT:
			return xvoCreateFloat(xvoGetFloat(objSrc));
		case XVO_DT_TEXT:
			return xvoCreateText(xvoGetText(objSrc), 0, FALSE);
		case XVO_DT_TIME:
			return xvoCreateTime(xvoGetTime(objSrc));
		case XVO_DT_ARRAY:
			objDst = Option_CreateSharedArrayValue();
			if ( objDst != NULL ) {
				uint32 iCount = xvoArrayItemCount(objSrc);
				for ( uint32 i = 0; i < iCount; i++ ) {
					xvalue objItem = Option_CloneSharedValue(xvoArrayGetValue(objSrc, i));
					if ( objItem != NULL ) {
						xvoArrayAppendValue(objDst, objItem, TRUE);
					}
				}
			}
			return objDst;
		case XVO_DT_TABLE:
			objDst = Option_CreateSharedTableValue();
			if ( objDst != NULL ) {
				OptionSharedCloneTableContext tCtx;
				tCtx.tblSrc = objSrc;
				tCtx.tblDst = objDst;
				xrtDictWalk(objSrc->vTable, (ptr)Option_CloneSharedTableItemProc, &tCtx);
			}
			return objDst;
		default:
			return xvoCopy(objSrc);
	}
}

static bool Option_HasNonSpaceText(const char* sText)
{
	if ( sText == NULL ) {
		return FALSE;
	}

	while ( *sText ) {
		if ( !Option_IsSpaceChar(*sText) ) {
			return TRUE;
		}
		sText++;
	}

	return FALSE;
}

static str Option_NormalizeAdminEntryPath(str sPath)
{
	const char* sStart;
	const char* sEnd;
	size_t iLen;
	size_t iWrite = 0;
	str sOut;
	size_t i;

	if ( sPath == NULL ) {
		return NULL;
	}

	sStart = sPath;
	while ( *sStart && Option_IsSpaceChar(*sStart) ) {
		sStart++;
	}
	sEnd = sStart + strlen(sStart);
	while ( (sEnd > sStart) && Option_IsSpaceChar(*(sEnd - 1)) ) {
		sEnd--;
	}
	if ( sEnd <= sStart ) {
		return NULL;
	}

	iLen = (size_t)(sEnd - sStart);
	sOut = xrtMalloc(iLen + 2);
	if ( sOut == NULL ) {
		return NULL;
	}

	if ( sStart[0] != '/' ) {
		sOut[iWrite++] = '/';
	}

	for ( i = 0; i < iLen; i++ ) {
		char c = sStart[i];
		if ( (c == '?') || (c == '#') ) {
			break;
		}
		sOut[iWrite++] = c;
	}
	sOut[iWrite] = '\0';

	while ( (iWrite > 1) && (sOut[iWrite - 1] == '/') ) {
		sOut[--iWrite] = '\0';
	}

	if ( iWrite == 0 ) {
		xrtFree(sOut);
		return NULL;
	}

	if ( strcmp(sOut, "/") == 0 ) {
		xrtFree(sOut);
		return NULL;
	}

	// 保留 /admin 作为默认后台入口，自定义安全入口不得继续占用 admin 前缀。
	if ( (strcmp(sOut, "/admin") == 0) || (strncmp(sOut, "/admin/", 7) == 0) ) {
		xrtFree(sOut);
		return NULL;
	}

	return sOut;
}

static bool Option_AdminEntryConflictsRoute(const char* sPath)
{
	if ( (sPath == NULL) || (sPath[0] == '\0') || (G_StaticRouteTableHTTP == NULL) ) {
		return FALSE;
	}

	if ( xrtDictGet(G_StaticRouteTableHTTP, (str)sPath, strlen(sPath)) != NULL ) {
		return TRUE;
	}
	return FindDynamicRouteHTTP((str)sPath) != NULL;
}

static bool Option_IsAdminEntryPathValid(const char* sPath)
{
	str sNormalized;
	bool bValid;

	if ( !Option_HasNonSpaceText(sPath) ) {
		return TRUE;
	}

	sNormalized = Option_NormalizeAdminEntryPath((str)sPath);
	if ( sNormalized == NULL ) {
		return FALSE;
	}

	bValid = !Option_AdminEntryConflictsRoute(sNormalized);
	xrtFree(sNormalized);
	return bValid;
}

static bool Option_IsAdminEntryTokenChar(char c)
{
	return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9'));
}

str Option_GenerateAdminEntryPath()
{
	int iTry;

	for ( iTry = 0; iTry < 16; iTry++ ) {
		char sToken[33];
		int iWrite = 0;
		int iSeedTry;

		for ( iSeedTry = 0; iSeedTry < 8 && iWrite < 32; iSeedTry++ ) {
			str sSeed = xrtMakeXIDS();
			int iRead;

			if ( sSeed == NULL ) {
				break;
			}

			for ( iRead = 0; sSeed[iRead] != '\0' && iWrite < 32; iRead++ ) {
				char c = sSeed[iRead];

				if ( !Option_IsAdminEntryTokenChar(c) ) {
					continue;
				}
				if ( (c >= 'A') && (c <= 'Z') ) {
					c = (char)(c - 'A' + 'a');
				}
				sToken[iWrite++] = c;
			}
			xrtFree(sSeed);
		}

		while ( iWrite < 32 ) {
			const char sHex[] = "0123456789abcdef";
			uint64 iFill = (uint64)xrtNow() + (uint64)(iTry + 1) * 1315423911ull + (uint64)(iWrite * 2654435761ull);
			sToken[iWrite++] = sHex[iFill & 0x0f];
		}
		sToken[32] = '\0';

		{
			str sPath = xrtMalloc(34);
			if ( sPath == NULL ) {
				return NULL;
			}
			sPath[0] = '/';
			memcpy(sPath + 1, sToken, 33);
			if ( Option_IsAdminEntryPathValid(sPath) ) {
				return sPath;
			}
			xrtFree(sPath);
		}
	}

	{
		str sPath = xrtCopyStr("/0123456789abcdef0123456789abcdef", 0);
		if ( sPath && Option_IsAdminEntryPathValid(sPath) ) {
			return sPath;
		}
		if ( sPath ) {
			xrtFree(sPath);
		}
	}

	return xrtCopyStr("/fedcba9876543210fedcba9876543210", 0);
}

str Option_GetGlobalText(const char* sName, const char* sDefaultValue)
{
	xvalue tblGlobal;
	str sValue;

	if ( (G_Option == NULL) || (xvoType(G_Option) != XVO_DT_TABLE) ) {
		return (str)(sDefaultValue ? sDefaultValue : "");
	}

	tblGlobal = xvoTableGetValue(G_Option, "global", 6);
	if ( (tblGlobal == NULL) || (xvoType(tblGlobal) != XVO_DT_TABLE) ) {
		return (str)(sDefaultValue ? sDefaultValue : "");
	}

	sValue = xvoTableGetText(tblGlobal, sName, 0);
	if ( Option_HasNonSpaceText(sValue) ) {
		return sValue;
	}

	return (str)(sDefaultValue ? sDefaultValue : "");
}

void Option_RefreshAdminEntryConfig()
{
	xvalue tblGlobal;
	str sAdminPath = NULL;

	G_AdminEntryEnabled = FALSE;
	if ( G_AdminEntryPath != NULL ) {
		xrtFree(G_AdminEntryPath);
		G_AdminEntryPath = NULL;
	}

	if ( (G_Option == NULL) || (xvoType(G_Option) != XVO_DT_TABLE) ) {
		return;
	}

	tblGlobal = xvoTableGetValue(G_Option, "global", 6);
	if ( (tblGlobal == NULL) || (xvoType(tblGlobal) != XVO_DT_TABLE) ) {
		return;
	}

	sAdminPath = Option_NormalizeAdminEntryPath(xvoTableGetText(tblGlobal, "cp_url", 6));
	if ( Option_AdminEntryConflictsRoute(sAdminPath) ) {
		xrtFree(sAdminPath);
		sAdminPath = NULL;
	}
	if ( sAdminPath != NULL ) {
		G_AdminEntryPath = sAdminPath;
		G_AdminEntryEnabled = TRUE;
		printf("[option] admin entry protected: %s\n", G_AdminEntryPath);
		fflush(stdout);
	}
}

bool Option_AdminEntryEnabled()
{
	return G_AdminEntryEnabled && (G_AdminEntryPath != NULL) && (G_AdminEntryPath[0] != '\0');
}

bool Option_AdminEntryIsMatch(const char* sPath)
{
	if ( !Option_AdminEntryEnabled() || (sPath == NULL) ) {
		return FALSE;
	}
	return strcmp(sPath, G_AdminEntryPath) == 0;
}

const char* Option_GetAdminLoginPath()
{
	if ( Option_AdminEntryEnabled() ) {
		return G_AdminEntryPath;
	}
	return "/admin/login";
}



bool Option_IsValidFileName(str sFileName)
{
	size_t iLen;
	size_t iBaseLen;
	size_t i;

	if ( (sFileName == NULL) || (sFileName[0] == '\0') ) {
		return FALSE;
	}

	iLen = strlen(sFileName);
	if ( (iLen <= 5) || (strcmp(sFileName + iLen - 5, ".json") != 0) ) {
		return FALSE;
	}

	iBaseLen = iLen - 5;
	for ( i = 0; i < iBaseLen; i++ ) {
		char c = sFileName[i];
		bool bOK = ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '_') || (c == '-');
		if ( !bOK ) {
			return FALSE;
		}
	}

	return TRUE;
}



str Option_BuildFilePath(str sFileName)
{
	return xrtPathJoin(2, OptionPath, sFileName);
}



str Option_BuildViewHref(str sFileName)
{
	return xrtFormat("/admin/view/option?file=%s", Option_StrOrEmpty(sFileName));
}



str Option_PathToFileName(str sPath)
{
	str sBaseName;
	str sExt;
	str sFileName = NULL;

	if ( sPath == NULL ) {
		return NULL;
	}

	sBaseName = xrtPathGetName(sPath, 0);
	sExt = xrtPathGetExt(sPath, 0);
	if ( sBaseName != NULL ) {
		if ( (sExt != NULL) && (sExt[0] != '\0') ) {
			sFileName = xrtFormat("%s.%s", sBaseName, sExt);
		} else {
			sFileName = xrtCopyStr(sBaseName, 0);
		}
	}
	if ( sBaseName ) xrtFree(sBaseName);
	if ( sExt ) xrtFree(sExt);

	return sFileName;
}



bool Option_FileExists(str sFileName)
{
	str sFilePath;
	str sText;
	bool bExists = FALSE;

	if ( !Option_IsValidFileName(sFileName) ) {
		return FALSE;
	}

	sFilePath = Option_BuildFilePath(sFileName);
	sText = xrtFileReadAll(sFilePath, XRT_CP_BINARY, NULL);
	if ( sText != NULL ) {
		bExists = TRUE;
		xrtFree(sText);
	}
	xrtFree(sFilePath);

	return bExists;
}



int Option_CountFields(xvalue tblConfig)
{
	int iCount = 0;
	xvalue arrClassList = xvoTableGetValue(tblConfig, "classList", 9);

	if ( (arrClassList == NULL) || (xvoType(arrClassList) != XVO_DT_ARRAY) ) {
		return 0;
	}

	for ( uint32 i = 0; i < xvoArrayItemCount(arrClassList); i++ ) {
		xvalue tblClass = xvoArrayGetValue(arrClassList, i);
		xvalue arrOptions;

		if ( (tblClass == NULL) || (xvoType(tblClass) != XVO_DT_TABLE) ) {
			continue;
		}

		arrOptions = xvoTableGetValue(tblClass, "options", 7);
		if ( (arrOptions == NULL) || (xvoType(arrOptions) != XVO_DT_ARRAY) ) {
			continue;
		}

		iCount += (int)xvoArrayItemCount(arrOptions);
	}

	return iCount;
}



int Option_CountGroups(xvalue tblConfig)
{
	xvalue arrClassList = xvoTableGetValue(tblConfig, "classList", 9);

	if ( (arrClassList == NULL) || (xvoType(arrClassList) != XVO_DT_ARRAY) ) {
		return 0;
	}

	return (int)xvoArrayItemCount(arrClassList);
}



bool Option_IsLockedConfig(xvalue tblConfig)
{
	if ( (tblConfig == NULL) || (xvoType(tblConfig) != XVO_DT_TABLE) ) {
		return FALSE;
	}

	return xvoTableGetBool(tblConfig, "locked", 6);
}



// 扫描配置文件的回调函数
int ScanOptionFileProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
{
	(void)iSize;
	(void)pData;
	(void)Param;

	if ( bDir == 0 ) {
		str sExt = xrtPathGetExt(sPath, 0);
		if ( (sExt != NULL) && (xrtStrComp(sExt, "json", 4, FALSE) == 0) ) {
			xvalue tblConfig = xrtParseJSON_File(sPath);
			if ( tblConfig != NULL ) {
				str sNamespace = xvoTableGetText(tblConfig, "namespace", 9);
				if ( sNamespace != NULL ) {
					xvalue tblNamespace = xvoTableGetValue(G_Option, sNamespace, 0);
					if ( (tblNamespace == NULL) || (tblNamespace->Type != XVO_DT_TABLE) ) {
						tblNamespace = Option_CreateSharedTableValue();
						xvoTableSetValue(G_Option, sNamespace, 0, tblNamespace, TRUE);
					}

					xvalue arrClassList = xvoTableGetValue(tblConfig, "classList", 9);
					if ( (arrClassList != NULL) && (xvoType(arrClassList) == XVO_DT_ARRAY) ) {
						uint32 iClassCount = xvoArrayItemCount(arrClassList);
						for ( uint32 i = 0; i < iClassCount; i++ ) {
							xvalue tblClass = xvoArrayGetValue(arrClassList, i);
							if ( (tblClass != NULL) && (xvoType(tblClass) == XVO_DT_TABLE) ) {
								xvalue arrOptions = xvoTableGetValue(tblClass, "options", 7);
								if ( (arrOptions != NULL) && (xvoType(arrOptions) == XVO_DT_ARRAY) ) {
									uint32 iOptCount = xvoArrayItemCount(arrOptions);
									for ( uint32 j = 0; j < iOptCount; j++ ) {
										xvalue tblOpt = xvoArrayGetValue(arrOptions, j);
										if ( (tblOpt != NULL) && (xvoType(tblOpt) == XVO_DT_TABLE) ) {
											str sName = xvoTableGetText(tblOpt, "name", 4);
											xvalue varValue = xvoTableGetValue(tblOpt, "value", 5);
											if ( sName != NULL ) {
												if ( varValue != NULL ) {
													xvalue varSharedValue = Option_CloneSharedValue(varValue);
													if ( varSharedValue != NULL ) {
														xvoTableSetValue(tblNamespace, sName, 0, varSharedValue, TRUE);
													} else {
														xvoTableSetText(tblNamespace, sName, 0, "", 0, FALSE);
													}
												} else {
													xvoTableSetText(tblNamespace, sName, 0, "", 0, FALSE);
												}
											}
										}
									}
								}
							}
						}
					}
				}
				xvoUnref(tblConfig);
			} else {
				printf("!!! ERROR !!! Option_Init - Failed to parse config file: %s\n", sPath);
			}
		}
		xrtFree(sExt);
	}
	return FALSE;
}



static int Option_CheckNamespaceProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
{
	OptionNamespaceCheckContext* pCtx = (OptionNamespaceCheckContext*)Param;

	(void)iSize;
	(void)pData;

	if ( (pCtx == NULL) || bDir != 0 || pCtx->bFound ) {
		return FALSE;
	}

	if ( sPath != NULL ) {
		str sFileName = Option_PathToFileName(sPath);
		str sExt = xrtPathGetExt(sPath, 0);
		if ( (sExt != NULL) && (xrtStrComp(sExt, "json", 4, FALSE) == 0) ) {
			bool bSkip = FALSE;
			if ( (pCtx->sExcludeFileName != NULL) && (sFileName != NULL) && (strcmp(sFileName, pCtx->sExcludeFileName) == 0) ) {
				bSkip = TRUE;
			}
			if ( !bSkip ) {
				xvalue tblConfig = xrtParseJSON_File(sPath);
				if ( tblConfig != NULL ) {
					str sNamespace = xvoTableGetText(tblConfig, "namespace", 9);
					if ( (sNamespace != NULL) && (strcmp(sNamespace, pCtx->sNamespace) == 0) ) {
						pCtx->bFound = TRUE;
					}
					xvoUnref(tblConfig);
				}
			}
		}
		xrtFree(sExt);
		xrtFree(sFileName);
	}

	return FALSE;
}



bool Option_HasDuplicateNamespace(str sNamespace, str sExcludeFileName)
{
	OptionNamespaceCheckContext tCtx;

	if ( (sNamespace == NULL) || (sNamespace[0] == '\0') ) {
		return FALSE;
	}

	tCtx.sNamespace = sNamespace;
	tCtx.sExcludeFileName = sExcludeFileName;
	tCtx.bFound = FALSE;
	xrtDirScan(OptionPath, FALSE, Option_CheckNamespaceProc, &tCtx);

	return tCtx.bFound;
}



static int Option_ListFilesProc(str sPath, size_t iSize, int bDir, ptr pData, ptr Param)
{
	OptionListContext* pCtx = (OptionListContext*)Param;

	(void)iSize;
	(void)pData;

	if ( (pCtx == NULL) || bDir != 0 ) {
		return FALSE;
	}

	if ( sPath != NULL ) {
		str sExt = xrtPathGetExt(sPath, 0);
		if ( (sExt != NULL) && (xrtStrComp(sExt, "json", 4, FALSE) == 0) ) {
			str sFileName = Option_PathToFileName(sPath);
			xvalue tblConfig = xrtParseJSON_File(sPath);
			xvalue tblRow = xvoCreateTable();
			if ( sFileName != NULL ) {
				xvoTableSetText(tblRow, "file", 4, sFileName, 0, FALSE);
			}
			if ( tblConfig != NULL ) {
				str sTitle = xvoTableGetText(tblConfig, "title", 5);
				str sNamespace = xvoTableGetText(tblConfig, "namespace", 9);
				str sDesc = xvoTableGetText(tblConfig, "desc", 4);
				bool bLocked = Option_IsLockedConfig(tblConfig);
				xvoTableSetBool(tblRow, "locked", 6, bLocked);
				xvoTableSetBool(tblRow, "canDelete", 9, !bLocked);
				xvoTableSetBool(tblRow, "canEditDefinition", 17, !bLocked);
				xvoTableSetText(tblRow, "title", 5, sTitle ? sTitle : Option_StrOrEmpty(sFileName), 0, FALSE);
				xvoTableSetText(tblRow, "namespace", 9, Option_StrOrEmpty(sNamespace), 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, Option_StrOrEmpty(sDesc), 0, FALSE);
				xvoTableSetInt(tblRow, "authLevel", 9, xvoTableGetInt(tblConfig, "authLevel", 9));
				xvoTableSetInt(tblRow, "groupCount", 10, Option_CountGroups(tblConfig));
				xvoTableSetInt(tblRow, "fieldCount", 10, Option_CountFields(tblConfig));
			} else {
				xvoTableSetBool(tblRow, "locked", 6, FALSE);
				xvoTableSetBool(tblRow, "canDelete", 9, TRUE);
				xvoTableSetBool(tblRow, "canEditDefinition", 17, TRUE);
				xvoTableSetText(tblRow, "title", 5, Option_StrOrEmpty(sFileName), 0, FALSE);
				xvoTableSetText(tblRow, "namespace", 9, "", 0, FALSE);
				xvoTableSetText(tblRow, "desc", 4, xrtCopyStr("配置文件解析失败", 0), 0, TRUE);
				xvoTableSetInt(tblRow, "authLevel", 9, 0);
				xvoTableSetInt(tblRow, "groupCount", 10, 0);
				xvoTableSetInt(tblRow, "fieldCount", 10, 0);
			}
			xvoArrayAppendValue(pCtx->arrFiles, tblRow, TRUE);
			if ( tblConfig != NULL ) {
				xvoUnref(tblConfig);
			}
			xrtFree(sFileName);
		}
		xrtFree(sExt);
	}

	return FALSE;
}



xvalue Option_ListFiles()
{
	OptionListContext tCtx;

	tCtx.arrFiles = xvoCreateArray();
	xrtDirScan(OptionPath, FALSE, Option_ListFilesProc, &tCtx);

	return tCtx.arrFiles;
}
#if 0
	for ( int i = 0; i < tCtx.iCount; i++ ) {
		str sPath = tCtx.arrPath[i];
		str sFileName;
		xvalue tblConfig;
		xvalue tblRow;

		if ( sPath == NULL ) {
			continue;
		}

		sFileName = Option_PathToFileName(sPath);
		printf("[option] Option_ListFiles build row for %s\n", sFileName ? sFileName : "(null)");
		fflush(stdout);
		tblConfig = xrtParseJSON_File(sPath);
		tblRow = xvoCreateTable();

		if ( sFileName != NULL ) {
			xvoTableSetText(tblRow, "file", 4, sFileName, 0, FALSE);
			xvoTableSetBool(tblRow, "canDelete", 9, strcmp(sFileName, "global.json") != 0);
		}

		if ( tblConfig != NULL ) {
			str sTitle = xvoTableGetText(tblConfig, "title", 5);
			str sNamespace = xvoTableGetText(tblConfig, "namespace", 9);
			str sDesc = xvoTableGetText(tblConfig, "desc", 4);
			xvoTableSetText(tblRow, "title", 5, sTitle ? sTitle : (sFileName ? sFileName : ""), 0, FALSE);
			xvoTableSetText(tblRow, "namespace", 9, sNamespace ? sNamespace : "", 0, FALSE);
			xvoTableSetText(tblRow, "desc", 4, sDesc ? sDesc : "", 0, FALSE);
			xvoTableSetInt(tblRow, "authLevel", 9, xvoTableGetInt(tblConfig, "authLevel", 9));
			xvoTableSetInt(tblRow, "groupCount", 10, Option_CountGroups(tblConfig));
			xvoTableSetInt(tblRow, "fieldCount", 10, Option_CountFields(tblConfig));
		} else {
			xvoTableSetText(tblRow, "title", 5, sFileName ? sFileName : "", 0, FALSE);
			xvoTableSetText(tblRow, "namespace", 9, "", 0, FALSE);
			xvoTableSetText(tblRow, "desc", 4, "閰嶇疆鏂囦欢瑙ｆ瀽澶辫触", 0, FALSE);
			xvoTableSetInt(tblRow, "authLevel", 9, 0);
			xvoTableSetInt(tblRow, "groupCount", 10, 0);
			xvoTableSetInt(tblRow, "fieldCount", 10, 0);
		}

		printf("[option] Option_ListFiles before append for %s\n", sFileName ? sFileName : "(null)");
		fflush(stdout);
		xvoArrayAppendValue(arrFiles, tblRow, TRUE);
		printf("[option] Option_ListFiles appended row for %s\n", sFileName ? sFileName : "(null)");
		fflush(stdout);
		if ( tblConfig != NULL ) {
			xvoUnref(tblConfig);
		}
		if ( sFileName != NULL ) {
			xrtFree(sFileName);
		}
		xrtFree(sPath);
		tCtx.arrPath[i] = NULL;
	}
	if ( tCtx.lstPaths != NULL ) {
		xrtListDestroy(tCtx.lstPaths);
	}
	printf("[option] Option_ListFiles done, count=%u\n", (unsigned int)xvoArrayItemCount(arrFiles));
	fflush(stdout);

	return arrFiles;
#endif



bool Option_ValidateConfig(str sFileName, xvalue tblConfig, str* psError)
{
	xvalue arrClassList;
	xvalue tblNameMap;
	str sNamespace;

	if ( (tblConfig == NULL) || (xvoType(tblConfig) != XVO_DT_TABLE) ) {
		if ( psError ) *psError = xrtCopyStr("配置数据格式错误", 0);
		return FALSE;
	}

	sNamespace = xvoTableGetText(tblConfig, "namespace", 9);
	if ( (sNamespace == NULL) || (sNamespace[0] == '\0') ) {
		if ( psError ) *psError = xrtCopyStr("namespace 不能为空", 0);
		return FALSE;
	}
	if ( Option_HasDuplicateNamespace(sNamespace, sFileName) ) {
		if ( psError ) *psError = xrtCopyStr("namespace 已被其他配置文件占用", 0);
		return FALSE;
	}

	arrClassList = xvoTableGetValue(tblConfig, "classList", 9);
	if ( (arrClassList == NULL) || (xvoType(arrClassList) != XVO_DT_ARRAY) ) {
		if ( psError ) *psError = xrtCopyStr("classList 必须是数组", 0);
		return FALSE;
	}

	tblNameMap = xvoCreateTable();
	for ( uint32 i = 0; i < xvoArrayItemCount(arrClassList); i++ ) {
		xvalue tblClass = xvoArrayGetValue(arrClassList, i);
		xvalue arrOptions;

		if ( (tblClass == NULL) || (xvoType(tblClass) != XVO_DT_TABLE) ) {
			xvoUnref(tblNameMap);
			if ( psError ) *psError = xrtFormat("第 %d 个分组格式错误", (int)i + 1);
			return FALSE;
		}

		arrOptions = xvoTableGetValue(tblClass, "options", 7);
		if ( (arrOptions == NULL) || (xvoType(arrOptions) != XVO_DT_ARRAY) ) {
			xvoUnref(tblNameMap);
			if ( psError ) *psError = xrtFormat("第 %d 个分组缺少 options 数组", (int)i + 1);
			return FALSE;
		}

		for ( uint32 j = 0; j < xvoArrayItemCount(arrOptions); j++ ) {
			xvalue tblOpt = xvoArrayGetValue(arrOptions, j);
			str sName;

			if ( (tblOpt == NULL) || (xvoType(tblOpt) != XVO_DT_TABLE) ) {
				xvoUnref(tblNameMap);
				if ( psError ) *psError = xrtFormat("第 %d 个分组的第 %d 个字段格式错误", (int)i + 1, (int)j + 1);
				return FALSE;
			}

			sName = xvoTableGetText(tblOpt, "name", 4);
			if ( (sName == NULL) || (sName[0] == '\0') ) {
				xvoUnref(tblNameMap);
				if ( psError ) *psError = xrtFormat("第 %d 个分组的第 %d 个字段缺少 name", (int)i + 1, (int)j + 1);
				return FALSE;
			}
			if ( xvoTableGetBool(tblNameMap, sName, 0) ) {
				xvoUnref(tblNameMap);
				if ( psError ) *psError = xrtFormat("字段名重复：%s", sName);
				return FALSE;
			}
			xvoTableSetBool(tblNameMap, sName, 0, TRUE);
		}
	}
	xvoUnref(tblNameMap);

	return TRUE;
}



// 加载指定配置文件，返回完整的配置结构（用于页面渲染）
xvalue Option_LoadFile(str sFileName)
{
	str sFilePath;
	xvalue tblConfig;

	if ( !Option_IsValidFileName(sFileName) ) {
		return NULL;
	}

	sFilePath = Option_BuildFilePath(sFileName);
	tblConfig = xrtParseJSON_File(sFilePath);
	xrtFree(sFilePath);

	return tblConfig;
}



void Option_RebuildCache()
{
	printf("[option] Option_RebuildCache begin\n");
	fflush(stdout);
	if ( G_Option != NULL ) {
		xvoUnref(G_Option);
	}
	G_Option = Option_CreateSharedTableValue();
	if ( G_Option == NULL ) {
		return;
	}
	xrtDirScan(OptionPath, FALSE, ScanOptionFileProc, NULL);
	XAdminValuePublishShared(G_Option);
	Option_RefreshAdminEntryConfig();
	printf("[option] Option_RebuildCache done\n");
	fflush(stdout);
}



// 保存配置文件中的 value
bool Option_SaveFile(str sFileName, xvalue tblFormData)
{
	str sFilePath = Option_BuildFilePath(sFileName);
	xvalue tblConfig = xrtParseJSON_File(sFilePath);
	bool bRet = FALSE;

	if ( tblConfig == NULL ) {
		xrtFree(sFilePath);
		return FALSE;
	}

	str sNamespace = xvoTableGetText(tblConfig, "namespace", 9);
	if ( (sNamespace != NULL) && (strcmp(sNamespace, "global") == 0) ) {
		str sAdminEntry = xvoTableGetText(tblFormData, "cp_url", 6);
		if ( !Option_IsAdminEntryPathValid(sAdminEntry) ) {
			xvoUnref(tblConfig);
			xrtFree(sFilePath);
			return FALSE;
		}
	}

	xvalue arrClassList = xvoTableGetValue(tblConfig, "classList", 9);
	if ( (arrClassList != NULL) && (xvoType(arrClassList) == XVO_DT_ARRAY) ) {
		uint32 iClassCount = xvoArrayItemCount(arrClassList);
		for ( uint32 i = 0; i < iClassCount; i++ ) {
			xvalue tblClass = xvoArrayGetValue(arrClassList, i);
			if ( (tblClass != NULL) && (xvoType(tblClass) == XVO_DT_TABLE) ) {
				xvalue arrOptions = xvoTableGetValue(tblClass, "options", 7);
				if ( (arrOptions != NULL) && (xvoType(arrOptions) == XVO_DT_ARRAY) ) {
					uint32 iOptCount = xvoArrayItemCount(arrOptions);
					for ( uint32 j = 0; j < iOptCount; j++ ) {
						xvalue tblOpt = xvoArrayGetValue(arrOptions, j);
						if ( (tblOpt != NULL) && (xvoType(tblOpt) == XVO_DT_TABLE) ) {
							str sName = xvoTableGetText(tblOpt, "name", 4);
							if ( sName != NULL ) {
								xvalue varNewValue = xvoTableGetValue(tblFormData, sName, 0);
								if ( varNewValue != NULL ) {
									xvoAddRef(varNewValue);
									xvoTableSetValue(tblOpt, "value", 5, varNewValue, TRUE);
								}
							}
						}
					}
				}
			}
		}
	}

	bRet = xrtStringifyJSON_File(sFilePath, tblConfig, TRUE);
	xvoUnref(tblConfig);
	xrtFree(sFilePath);
	Option_RebuildCache();

	return bRet;
}



bool Option_SaveDefinition(str sFileName, xvalue tblConfig, bool bCreate, str* psError)
{
	str sFilePath;
	int iRet;

	if ( !Option_IsValidFileName(sFileName) ) {
		if ( psError ) *psError = xrtCopyStr("文件名只能包含字母、数字、下划线或中划线，并以 .json 结尾", 0);
		return FALSE;
	}

	if ( bCreate ) {
		if ( Option_FileExists(sFileName) ) {
			if ( psError ) *psError = xrtCopyStr("配置文件已存在", 0);
			return FALSE;
		}
	} else {
		if ( !Option_FileExists(sFileName) ) {
			if ( psError ) *psError = xrtCopyStr("配置文件不存在", 0);
			return FALSE;
		}
	}

	if ( !bCreate ) {
		xvalue tblOldConfig = Option_LoadFile(sFileName);
		if ( tblOldConfig == NULL ) {
			if ( psError ) *psError = xrtCopyStr("配置文件不存在或解析失败", 0);
			return FALSE;
		}
		if ( Option_IsLockedConfig(tblOldConfig) ) {
			xvoUnref(tblOldConfig);
			if ( psError ) *psError = xrtCopyStr("该配置文件已锁定，不允许修改结构", 0);
			return FALSE;
		}
		xvoUnref(tblOldConfig);
	}

	if ( !Option_ValidateConfig(sFileName, tblConfig, psError) ) {
		return FALSE;
	}

	sFilePath = Option_BuildFilePath(sFileName);
	iRet = xrtStringifyJSON_File(sFilePath, tblConfig, TRUE);
	xrtFree(sFilePath);
	if ( !iRet ) {
		if ( psError ) *psError = xrtCopyStr("写入配置文件失败", 0);
		return FALSE;
	}

	Option_RebuildCache();
	return TRUE;
}



bool Option_DeleteFile(str sFileName, str* psError)
{
	str sFilePath;
	bool bRet;
	xvalue tblConfig;

	if ( !Option_IsValidFileName(sFileName) ) {
		if ( psError ) *psError = xrtCopyStr("非法的文件名", 0);
		return FALSE;
	}
	if ( !Option_FileExists(sFileName) ) {
		if ( psError ) *psError = xrtCopyStr("配置文件不存在", 0);
		return FALSE;
	}

	tblConfig = Option_LoadFile(sFileName);
	if ( tblConfig == NULL ) {
		if ( psError ) *psError = xrtCopyStr("配置文件不存在或解析失败", 0);
		return FALSE;
	}
	if ( Option_IsLockedConfig(tblConfig) ) {
		xvoUnref(tblConfig);
		if ( psError ) *psError = xrtCopyStr("该配置文件已锁定，不允许删除", 0);
		return FALSE;
	}
	xvoUnref(tblConfig);

	sFilePath = Option_BuildFilePath(sFileName);
	bRet = xrtFileDelete(sFilePath);
	xrtFree(sFilePath);
	if ( !bRet ) {
		if ( psError ) *psError = xrtCopyStr("删除配置文件失败", 0);
		return FALSE;
	}

	Option_RebuildCache();
	return TRUE;
}



// 初始化配置模块
void Option_Init()
{
	printf("        Option_Init \n");
	Option_RebuildCache();
}



// 卸载配置模块
void Option_Unit()
{
	printf("        Option_Unit \n");

	if ( G_Option != NULL ) {
		xvoUnref(G_Option);
		G_Option = NULL;
	}
	if ( G_AdminEntryPath != NULL ) {
		xrtFree(G_AdminEntryPath);
		G_AdminEntryPath = NULL;
	}
	G_AdminEntryEnabled = FALSE;
}


