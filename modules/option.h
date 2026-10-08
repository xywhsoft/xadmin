
typedef struct OptionNamespaceCheckContext {
	str sNamespace;
	str sExcludeFileName;
	bool bFound;
} OptionNamespaceCheckContext;

typedef struct OptionListContext {
	xvalue* arrFiles;
} OptionListContext;

static str Option_StrOrEmpty(str sText)
{
	return sText ? sText : (str)"";
}

/* 内建字段在读取时补齐，升级旧配置无需替换用户的 global.json。
 * 只有显式保存时才将补齐后的定义写回文件。 */
static void Option_EnsureGlobalSettings(xvalue* config)
{
	const char* ns = ValueText(config, "namespace");
	xvalue* classes = ValueGet(config, "classList");
	if (!ns || strcmp(ns, "global") || xrtValueType(classes) != XVALUE_ARRAY) return;
	for (size_t n = 0; n < GLOBAL_SETTING_COUNT; n++) {
		const GlobalSettingSpec* spec = &G_GlobalSettings[n];
		xvalue* field = NULL; xvalue* group = NULL;
		for (size_t i = 0; i < ValueCount(classes); i++) {
			xvalue* candidateGroup = xrtValueArrayGet(classes, i);
			const char* title = ValueText(candidateGroup, "title");
			if (title && !strcmp(title, spec->group)) group = candidateGroup;
			xvalue* options = ValueGet(candidateGroup, "options");
			for (size_t j = 0; j < ValueCount(options); j++) {
				xvalue* candidate = xrtValueArrayGet(options, j);
				const char* name = ValueText(candidate, "name");
				if (name && !strcmp(name, spec->name)) field = candidate;
			}
		}
		if (!field) {
			if (!group) {
				group = ValueObject();
				ValueSetText(group, "title", spec->group);
				ValueSetOwn(group, "options", ValueArray());
				ValueArrayOwn(classes, group);
			}
			field = ValueObject();
			ValueSetText(field, "name", spec->name);
			ValueArrayOwn(ValueGet(group, "options"), field);
		}
		ValueSetText(field, "title", spec->title);
		ValueSetText(field, "desc", spec->desc);
		ValueSetText(field, "type", spec->boolean ? "switch" : "int");
		ValueSetBool(field, "required", !spec->boolean);
		if (!spec->boolean) {
			ValueSetInt(field, "min", spec->minimum);
			ValueSetInt(field, "max", spec->maximum);
			ValueSetInt(field, "step", 1);
		}
		if (!Global_ValidSetting(spec, ValueGet(field, "value"))) {
			if (spec->boolean) ValueSetBool(field, "value", spec->fallback != 0);
			else ValueSetInt(field, "value", spec->fallback);
		}
	}
}

static bool Option_IsSpaceChar(char c)
{
	return (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n');
}

static xvalue* Option_CreateSharedTableValue()
{
	xvalue* pVal = ValueObject();
	if ( pVal != NULL ) {
		XAdminValuePublishShared(pVal);
	}
	return pVal;
}

static xvalue* Option_CreateSharedArrayValue()
{
	xvalue* pVal = ValueArray();
	if ( pVal != NULL ) {
		XAdminValuePublishShared(pVal);
	}
	return pVal;
}

typedef struct OptionSharedCloneTableContext {
	xvalue* tblSrc;
	xvalue* tblDst;
} OptionSharedCloneTableContext;

static xvalue* Option_CloneSharedValue(xvalue* objSrc);

static bool Option_CloneSharedTableItemProc(xstrview key, xvalue* value, void* context)
{
	OptionSharedCloneTableContext* ctx = (OptionSharedCloneTableContext*)context;
	xvalue* src;
	xvalue* dst;

	(void)value;
	if (!ctx || !ctx->tblSrc || !ctx->tblDst) return false;

	src = xrtValueObjectGet(ctx->tblSrc, key);
	dst = Option_CloneSharedValue(src);
	if (dst != NULL) xrtValueObjectSetNew(ctx->tblDst, key, dst);
	return false;
}

static xvalue* Option_CloneSharedValue(xvalue* objSrc)
{
	xvalue* objDst = NULL;
	int iType;

	if ( objSrc == NULL ) {
		return NULL;
	}

	iType = xrtValueType(objSrc);
	switch ( iType ) {
		case XVALUE_NULL:
			return xrtValueNull();
		case XVALUE_BOOL:
			return xrtValueBool(ValueBoolOf(objSrc));
		case XVALUE_INT:
			return xrtValueInt(ValueIntOf(objSrc));
		case XVALUE_FLOAT:
			return xrtValueFloat(ValueFloatOf(objSrc));
		case XVALUE_STRING:
			{
			xstrview text = {0};
			(void)xrtValueGetString(objSrc, &text);
			return xrtValueString(text);
		}
		case XVALUE_TIME:
			return xrtValueTime(ValueTimeOf(objSrc));
		case XVALUE_ARRAY:
			objDst = Option_CreateSharedArrayValue();
			if ( objDst != NULL ) {
				uint32 iCount = ValueCount(objSrc);
				for ( uint32 i = 0; i < iCount; i++ ) {
					xvalue* objItem = Option_CloneSharedValue(xrtValueArrayGet(objSrc, i));
					if ( objItem != NULL ) {
						ValueArrayOwn(objDst, objItem);
					}
				}
			}
			return objDst;
		case XVALUE_OBJECT:
			objDst = Option_CreateSharedTableValue();
			if ( objDst != NULL ) {
				OptionSharedCloneTableContext tCtx;
				tCtx.tblSrc = objSrc;
				tCtx.tblDst = objDst;
				ValueWalk(objSrc, Option_CloneSharedTableItemProc, &tCtx);
			}
			return objDst;
		default:
			return xrtValueDeepClone(objSrc);
	}
}

static bool Option_HasNonSpaceText(const char* sText)
{
	if ( sText == NULL ) {
		return false;
	}

	while ( *sText ) {
		if ( !Option_IsSpaceChar(*sText) ) {
			return true;
		}
		sText++;
	}

	return false;
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
		return false;
	}

	return xrtMapGet(G_StaticRouteTableHTTP, KeyView(sPath)) != NULL;
}

static bool Option_IsAdminEntryPathValid(const char* sPath)
{
	str sNormalized;
	bool bValid;

	if ( !Option_HasNonSpaceText(sPath) ) {
		return true;
	}

	sNormalized = Option_NormalizeAdminEntryPath((str)sPath);
	if ( sNormalized == NULL ) {
		return false;
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
			str sSeed = Util_Token();
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
			uint64 iFill = (uint64)XAdmin_UnixNowUs() + (uint64)(iTry + 1) * 1315423911ull + (uint64)(iWrite * 2654435761ull);
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
		str sPath = xrtStrDup("/0123456789abcdef0123456789abcdef");
		if ( sPath && Option_IsAdminEntryPathValid(sPath) ) {
			return sPath;
		}
		if ( sPath ) {
			xrtFree(sPath);
		}
	}

	return xrtStrDup("/fedcba9876543210fedcba9876543210");
}

str Option_GetGlobalText(const char* sName, const char* sDefaultValue)
{
	xvalue* tblGlobal;
	str sValue;

	if ( (G_Option == NULL) || (xrtValueType(G_Option) != XVALUE_OBJECT) ) {
		return (str)(sDefaultValue ? sDefaultValue : "");
	}

	tblGlobal = ValueGet(G_Option, "global");
	if ( (tblGlobal == NULL) || (xrtValueType(tblGlobal) != XVALUE_OBJECT) ) {
		return (str)(sDefaultValue ? sDefaultValue : "");
	}

	sValue = ValueText(tblGlobal, sName);
	if ( Option_HasNonSpaceText(sValue) ) {
		return sValue;
	}

	return (str)(sDefaultValue ? sDefaultValue : "");
}

void Option_RefreshAdminEntryConfig()
{
	xvalue* tblGlobal;
	str sAdminPath = NULL;

	G_AdminEntryEnabled = false;
	if ( G_AdminEntryPath != NULL ) {
		xrtFree(G_AdminEntryPath);
		G_AdminEntryPath = NULL;
	}

	if ( (G_Option == NULL) || (xrtValueType(G_Option) != XVALUE_OBJECT) ) {
		return;
	}

	tblGlobal = ValueGet(G_Option, "global");
	if ( (tblGlobal == NULL) || (xrtValueType(tblGlobal) != XVALUE_OBJECT) ) {
		return;
	}

	sAdminPath = Option_NormalizeAdminEntryPath(ValueText(tblGlobal, "cp_url"));
	if ( Option_AdminEntryConflictsRoute(sAdminPath) ) {
		xrtFree(sAdminPath);
		sAdminPath = NULL;
	}
	if ( sAdminPath != NULL ) {
		G_AdminEntryPath = sAdminPath;
		G_AdminEntryEnabled = true;
		printf("[option] protected admin entry enabled\n");
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
		return false;
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
		return false;
	}

	iLen = strlen(sFileName);
	if ( (iLen <= 5) || (strcmp(sFileName + iLen - 5, ".json") != 0) ) {
		return false;
	}

	iBaseLen = iLen - 5;
	for ( i = 0; i < iBaseLen; i++ ) {
		char c = sFileName[i];
		bool bOK = ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) || ((c >= '0') && (c <= '9')) || (c == '_') || (c == '-');
		if ( !bOK ) {
			return false;
		}
	}

	return true;
}



str Option_BuildFilePath(str sFileName)
{
	return xrtPathJoin(OptionPath, sFileName);
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

	sBaseName = xrtPathStem(sPath);
	sExt = Util_ExtNoDot(sPath);
	if ( sBaseName != NULL ) {
		if ( (sExt != NULL) && (sExt[0] != '\0') ) {
			sFileName = xrtFormat("%s.%s", sBaseName, sExt);
		} else {
			sFileName = xrtStrDup(sBaseName);
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
	bool bExists = false;

	if ( !Option_IsValidFileName(sFileName) ) {
		return false;
	}

	sFilePath = Option_BuildFilePath(sFileName);
	sText = xrtFileReadAll(sFilePath, NULL);
	if ( sText != NULL ) {
		bExists = true;
		xrtFree(sText);
	}
	xrtFree(sFilePath);

	return bExists;
}



int Option_CountFields(xvalue* tblConfig)
{
	int iCount = 0;
	xvalue* arrClassList = ValueGet(tblConfig, "classList");

	if ( (arrClassList == NULL) || (xrtValueType(arrClassList) != XVALUE_ARRAY) ) {
		return 0;
	}

	for ( uint32 i = 0; i < ValueCount(arrClassList); i++ ) {
		xvalue* tblClass = xrtValueArrayGet(arrClassList, i);
		xvalue* arrOptions;

		if ( (tblClass == NULL) || (xrtValueType(tblClass) != XVALUE_OBJECT) ) {
			continue;
		}

		arrOptions = ValueGet(tblClass, "options");
		if ( (arrOptions == NULL) || (xrtValueType(arrOptions) != XVALUE_ARRAY) ) {
			continue;
		}

		iCount += (int)ValueCount(arrOptions);
	}

	return iCount;
}



int Option_CountGroups(xvalue* tblConfig)
{
	xvalue* arrClassList = ValueGet(tblConfig, "classList");

	if ( (arrClassList == NULL) || (xrtValueType(arrClassList) != XVALUE_ARRAY) ) {
		return 0;
	}

	return (int)ValueCount(arrClassList);
}



bool Option_IsLockedConfig(xvalue* tblConfig)
{
	if ( (tblConfig == NULL) || (xrtValueType(tblConfig) != XVALUE_OBJECT) ) {
		return false;
	}

	return ValueBool(tblConfig, "locked");
}



// 扫描配置文件的回调函数
int ScanOptionFileProc(const char* sPath, size_t iSize, bool bDir, void* pParam)
{
	(void)iSize;
	(void)pParam;

	if ( bDir == 0 ) {
		str sExt = Util_ExtNoDot(sPath);
		if ( (sExt != NULL) && (xrtStrCaseCompare(xrtStrViewN(sExt, 4), xrtStrViewN("json", 4)) == 0) ) {
			xvalue* tblConfig = JsonParseFile(sPath);
			if ( tblConfig != NULL ) {
				Option_EnsureGlobalSettings(tblConfig);
				str sNamespace = ValueText(tblConfig, "namespace");
				if ( sNamespace != NULL ) {
					xvalue* tblNamespace = ValueGet(G_Option, sNamespace);
					if ( (tblNamespace == NULL) || (xrtValueType(tblNamespace) != XVALUE_OBJECT) ) {
						tblNamespace = Option_CreateSharedTableValue();
						ValueSetOwn(G_Option, sNamespace, tblNamespace);
					}

					xvalue* arrClassList = ValueGet(tblConfig, "classList");
					if ( (arrClassList != NULL) && (xrtValueType(arrClassList) == XVALUE_ARRAY) ) {
						uint32 iClassCount = ValueCount(arrClassList);
						for ( uint32 i = 0; i < iClassCount; i++ ) {
							xvalue* tblClass = xrtValueArrayGet(arrClassList, i);
							if ( (tblClass != NULL) && (xrtValueType(tblClass) == XVALUE_OBJECT) ) {
								xvalue* arrOptions = ValueGet(tblClass, "options");
								if ( (arrOptions != NULL) && (xrtValueType(arrOptions) == XVALUE_ARRAY) ) {
									uint32 iOptCount = ValueCount(arrOptions);
									for ( uint32 j = 0; j < iOptCount; j++ ) {
										xvalue* tblOpt = xrtValueArrayGet(arrOptions, j);
										if ( (tblOpt != NULL) && (xrtValueType(tblOpt) == XVALUE_OBJECT) ) {
											str sName = ValueText(tblOpt, "name");
											xvalue* varValue = ValueGet(tblOpt, "value");
											if ( sName != NULL ) {
												if ( varValue != NULL ) {
													xvalue* varSharedValue = Option_CloneSharedValue(varValue);
													if ( varSharedValue != NULL ) {
														ValueSetOwn(tblNamespace, sName, varSharedValue);
													} else {
														ValueSetText(tblNamespace, sName, "");
													}
												} else {
													ValueSetText(tblNamespace, sName, "");
												}
											}
										}
									}
								}
							}
						}
					}
				}
				xrtValueRelease(tblConfig);
			} else {
				printf("!!! ERROR !!! Option_Init - Failed to parse config file: %s\n", sPath);
			}
		}
		xrtFree(sExt);
	}
	return false;
}



static int Option_CheckNamespaceProc(const char* sPath, size_t iSize, bool bDir, void* pParam)
{
	OptionNamespaceCheckContext* pCtx = (OptionNamespaceCheckContext*)pParam;

	(void)iSize;

	if ( (pCtx == NULL) || bDir != 0 || pCtx->bFound ) {
		return false;
	}

	if ( sPath != NULL ) {
		str sFileName = Option_PathToFileName((str)sPath);
		str sExt = Util_ExtNoDot(sPath);
		if ( (sExt != NULL) && (xrtStrCaseCompare(xrtStrViewN(sExt, 4), xrtStrViewN("json", 4)) == 0) ) {
			bool bSkip = false;
			if ( (pCtx->sExcludeFileName != NULL) && (sFileName != NULL) && (strcmp(sFileName, pCtx->sExcludeFileName) == 0) ) {
				bSkip = true;
			}
			if ( !bSkip ) {
				xvalue* tblConfig = JsonParseFile(sPath);
				if ( tblConfig != NULL ) {
					str sNamespace = ValueText(tblConfig, "namespace");
					if ( (sNamespace != NULL) && (strcmp(sNamespace, pCtx->sNamespace) == 0) ) {
						pCtx->bFound = true;
					}
					xrtValueRelease(tblConfig);
				}
			}
		}
		xrtFree(sExt);
		xrtFree(sFileName);
	}

	return false;
}



bool Option_HasDuplicateNamespace(str sNamespace, str sExcludeFileName)
{
	OptionNamespaceCheckContext tCtx;

	if ( (sNamespace == NULL) || (sNamespace[0] == '\0') ) {
		return false;
	}

	tCtx.sNamespace = sNamespace;
	tCtx.sExcludeFileName = sExcludeFileName;
	tCtx.bFound = false;
	DirScan(OptionPath, false, Option_CheckNamespaceProc, &tCtx);

	return tCtx.bFound;
}



static int Option_ListFilesProc(const char* sPath, size_t iSize, bool bDir, void* pParam)
{
	OptionListContext* pCtx = (OptionListContext*)pParam;

	(void)iSize;

	if ( (pCtx == NULL) || bDir != 0 ) {
		return false;
	}

	if ( sPath != NULL ) {
		str sExt = Util_ExtNoDot(sPath);
		if ( (sExt != NULL) && (xrtStrCaseCompare(xrtStrViewN(sExt, 4), xrtStrViewN("json", 4)) == 0) ) {
			str sFileName = Option_PathToFileName((str)sPath);
			xvalue* tblConfig = JsonParseFile(sPath);
			xvalue* tblRow = ValueObject();
			if ( sFileName != NULL ) {
				ValueSetText(tblRow, "file", sFileName);
			}
			if ( tblConfig != NULL ) {
				str sTitle = ValueText(tblConfig, "title");
				str sNamespace = ValueText(tblConfig, "namespace");
				str sDesc = ValueText(tblConfig, "desc");
				bool bLocked = Option_IsLockedConfig(tblConfig);
				ValueSetBool(tblRow, "locked", bLocked);
				ValueSetBool(tblRow, "canDelete", !bLocked);
				ValueSetBool(tblRow, "canEditDefinition", !bLocked);
				ValueSetText(tblRow, "title", sTitle ? sTitle : Option_StrOrEmpty(sFileName));
				ValueSetText(tblRow, "namespace", Option_StrOrEmpty(sNamespace));
				ValueSetText(tblRow, "desc", Option_StrOrEmpty(sDesc));
				ValueSetInt(tblRow, "authLevel", ValueInt(tblConfig, "authLevel"));
				ValueSetInt(tblRow, "groupCount", Option_CountGroups(tblConfig));
				ValueSetInt(tblRow, "fieldCount", Option_CountFields(tblConfig));
			} else {
				ValueSetBool(tblRow, "locked", false);
				ValueSetBool(tblRow, "canDelete", true);
				ValueSetBool(tblRow, "canEditDefinition", true);
				ValueSetText(tblRow, "title", Option_StrOrEmpty(sFileName));
				ValueSetText(tblRow, "namespace", "");
				ValueSetOwnedText(tblRow, "desc", xrtStrDup("配置文件解析失败"));
				ValueSetInt(tblRow, "authLevel", 0);
				ValueSetInt(tblRow, "groupCount", 0);
				ValueSetInt(tblRow, "fieldCount", 0);
			}
			ValueArrayOwn(pCtx->arrFiles, tblRow);
			if ( tblConfig != NULL ) {
				xrtValueRelease(tblConfig);
			}
			xrtFree(sFileName);
		}
		xrtFree(sExt);
	}

	return false;
}



xvalue* Option_ListFiles()
{
	OptionListContext tCtx;

	tCtx.arrFiles = ValueArray();
	DirScan(OptionPath, false, Option_ListFilesProc, &tCtx);

	return tCtx.arrFiles;
}
#if 0
	for ( int i = 0; i < tCtx.iCount; i++ ) {
		str sPath = tCtx.arrPath[i];
		str sFileName;
		xvalue* tblConfig;
		xvalue* tblRow;

		if ( sPath == NULL ) {
			continue;
		}

		sFileName = Option_PathToFileName(sPath);
		printf("[option] Option_ListFiles build row for %s\n", sFileName ? sFileName : "(null)");
		fflush(stdout);
		tblConfig = JsonParseFile(sPath);
		tblRow = ValueObject();

		if ( sFileName != NULL ) {
			ValueSetText(tblRow, "file", sFileName);
			ValueSetBool(tblRow, "canDelete", strcmp(sFileName, "global.json") != 0);
		}

		if ( tblConfig != NULL ) {
			str sTitle = ValueText(tblConfig, "title");
			str sNamespace = ValueText(tblConfig, "namespace");
			str sDesc = ValueText(tblConfig, "desc");
			ValueSetText(tblRow, "title", sTitle ? sTitle : (sFileName ? sFileName : ""));
			ValueSetText(tblRow, "namespace", sNamespace ? sNamespace : "");
			ValueSetText(tblRow, "desc", sDesc ? sDesc : "");
			ValueSetInt(tblRow, "authLevel", ValueInt(tblConfig, "authLevel"));
			ValueSetInt(tblRow, "groupCount", Option_CountGroups(tblConfig));
			ValueSetInt(tblRow, "fieldCount", Option_CountFields(tblConfig));
		} else {
			ValueSetText(tblRow, "title", sFileName ? sFileName : "");
			ValueSetText(tblRow, "namespace", "");
			ValueSetText(tblRow, "desc", "閰嶇疆鏂囦欢瑙ｆ瀽澶辫触");
			ValueSetInt(tblRow, "authLevel", 0);
			ValueSetInt(tblRow, "groupCount", 0);
			ValueSetInt(tblRow, "fieldCount", 0);
		}

		printf("[option] Option_ListFiles before append for %s\n", sFileName ? sFileName : "(null)");
		fflush(stdout);
		ValueArrayOwn(arrFiles, tblRow);
		printf("[option] Option_ListFiles appended row for %s\n", sFileName ? sFileName : "(null)");
		fflush(stdout);
		if ( tblConfig != NULL ) {
			xrtValueRelease(tblConfig);
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
	printf("[option] Option_ListFiles done, count=%u\n", (unsigned int)ValueCount(arrFiles));
	fflush(stdout);

	return arrFiles;
#endif



bool Option_ValidateConfig(str sFileName, xvalue* tblConfig, str* psError)
{
	xvalue* arrClassList;
	xvalue* tblNameMap;
	str sNamespace;

	if ( (tblConfig == NULL) || (xrtValueType(tblConfig) != XVALUE_OBJECT) ) {
		if ( psError ) *psError = xrtStrDup("配置数据格式错误");
		return false;
	}

	sNamespace = ValueText(tblConfig, "namespace");
	if ( (sNamespace == NULL) || (sNamespace[0] == '\0') ) {
		if ( psError ) *psError = xrtStrDup("namespace 不能为空");
		return false;
	}
	if ( Option_HasDuplicateNamespace(sNamespace, sFileName) ) {
		if ( psError ) *psError = xrtStrDup("namespace 已被其他配置文件占用");
		return false;
	}

	arrClassList = ValueGet(tblConfig, "classList");
	if ( (arrClassList == NULL) || (xrtValueType(arrClassList) != XVALUE_ARRAY) ) {
		if ( psError ) *psError = xrtStrDup("classList 必须是数组");
		return false;
	}

	tblNameMap = ValueObject();
	for ( uint32 i = 0; i < ValueCount(arrClassList); i++ ) {
		xvalue* tblClass = xrtValueArrayGet(arrClassList, i);
		xvalue* arrOptions;

		if ( (tblClass == NULL) || (xrtValueType(tblClass) != XVALUE_OBJECT) ) {
			xrtValueRelease(tblNameMap);
			if ( psError ) *psError = xrtFormat("第 %d 个分组格式错误", (int)i + 1);
			return false;
		}

		arrOptions = ValueGet(tblClass, "options");
		if ( (arrOptions == NULL) || (xrtValueType(arrOptions) != XVALUE_ARRAY) ) {
			xrtValueRelease(tblNameMap);
			if ( psError ) *psError = xrtFormat("第 %d 个分组缺少 options 数组", (int)i + 1);
			return false;
		}

		for ( uint32 j = 0; j < ValueCount(arrOptions); j++ ) {
			xvalue* tblOpt = xrtValueArrayGet(arrOptions, j);
			str sName;

			if ( (tblOpt == NULL) || (xrtValueType(tblOpt) != XVALUE_OBJECT) ) {
				xrtValueRelease(tblNameMap);
				if ( psError ) *psError = xrtFormat("第 %d 个分组的第 %d 个字段格式错误", (int)i + 1, (int)j + 1);
				return false;
			}

			sName = ValueText(tblOpt, "name");
			if ( (sName == NULL) || (sName[0] == '\0') ) {
				xrtValueRelease(tblNameMap);
				if ( psError ) *psError = xrtFormat("第 %d 个分组的第 %d 个字段缺少 name", (int)i + 1, (int)j + 1);
				return false;
			}
			if ( ValueBool(tblNameMap, sName) ) {
				xrtValueRelease(tblNameMap);
				if ( psError ) *psError = xrtFormat("字段名重复：%s", sName);
				return false;
			}
			ValueSetBool(tblNameMap, sName, true);
		}
	}
	xrtValueRelease(tblNameMap);

	return true;
}



// 加载指定配置文件，返回完整的配置结构（用于页面渲染）
xvalue* Option_LoadFile(str sFileName)
{
	str sFilePath;
	xvalue* tblConfig;

	if ( !Option_IsValidFileName(sFileName) ) {
		return NULL;
	}

	sFilePath = Option_BuildFilePath(sFileName);
	tblConfig = JsonParseFile(sFilePath);
	xrtFree(sFilePath);
	Option_EnsureGlobalSettings(tblConfig);

	return tblConfig;
}



void Option_RebuildCache()
{
	int previousTimeout = Session_AdminTimeoutSeconds();
	int previousLimit = Global_Int("admin_session_limit");
	printf("[option] Option_RebuildCache begin\n");
	fflush(stdout);
	if ( G_Option != NULL ) {
		xrtValueRelease(G_Option);
	}
	G_Option = Option_CreateSharedTableValue();
	if ( G_Option == NULL ) {
		return;
	}
	DirScan(OptionPath, false, ScanOptionFileProc, NULL);
	XAdminValuePublishShared(G_Option);
	Option_RefreshAdminEntryConfig();
	if (previousTimeout != Session_AdminTimeoutSeconds()) Session_RefreshAdminTimeout();
	if (previousLimit != Global_Int("admin_session_limit")) Session_RefreshAdminLimit();
	Logs_SyncCleanupConfig();
	printf("[option] Option_RebuildCache done\n");
	fflush(stdout);
}



// 保存配置文件中的 value
bool Option_SaveFile(str sFileName, xvalue* tblFormData, str* psError)
{
	str sFilePath = Option_BuildFilePath(sFileName);
	xvalue* tblConfig = Option_LoadFile(sFileName);
	bool bRet = false;

	if ( tblConfig == NULL ) {
		xrtFree(sFilePath);
		return false;
	}

	str sNamespace = ValueText(tblConfig, "namespace");
	if ( (sNamespace != NULL) && (strcmp(sNamespace, "global") == 0) ) {
		for (size_t i = 0; i < GLOBAL_SETTING_COUNT; i++) {
			const GlobalSettingSpec* spec = &G_GlobalSettings[i];
			xvalue* value = ValueGet(tblFormData, spec->name);
			if (value && !Global_ValidSetting(spec, value)) {
				if (psError) *psError = spec->boolean ? xrtFormat("%s 必须为开关值", spec->title) :
					xrtFormat("%s 必须为 %d–%d 的整数", spec->title, spec->minimum, spec->maximum);
				xrtValueRelease(tblConfig);
				xrtFree(sFilePath);
				return false;
			}
		}
		str sAdminEntry = ValueText(tblFormData, "cp_url");
		if ( !Option_IsAdminEntryPathValid(sAdminEntry) ) {
			xrtValueRelease(tblConfig);
			xrtFree(sFilePath);
			return false;
		}
	}

	xvalue* arrClassList = ValueGet(tblConfig, "classList");
	if ( (arrClassList != NULL) && (xrtValueType(arrClassList) == XVALUE_ARRAY) ) {
		uint32 iClassCount = ValueCount(arrClassList);
		for ( uint32 i = 0; i < iClassCount; i++ ) {
			xvalue* tblClass = xrtValueArrayGet(arrClassList, i);
			if ( (tblClass != NULL) && (xrtValueType(tblClass) == XVALUE_OBJECT) ) {
				xvalue* arrOptions = ValueGet(tblClass, "options");
				if ( (arrOptions != NULL) && (xrtValueType(arrOptions) == XVALUE_ARRAY) ) {
					uint32 iOptCount = ValueCount(arrOptions);
					for ( uint32 j = 0; j < iOptCount; j++ ) {
						xvalue* tblOpt = xrtValueArrayGet(arrOptions, j);
						if ( (tblOpt != NULL) && (xrtValueType(tblOpt) == XVALUE_OBJECT) ) {
							str sName = ValueText(tblOpt, "name");
							if ( sName != NULL ) {
								xvalue* varNewValue = ValueGet(tblFormData, sName);
								if ( varNewValue != NULL ) {
									xrtValueRetain(varNewValue);
									ValueSetOwn(tblOpt, "value", varNewValue);
								}
							}
						}
					}
				}
			}
		}
	}

	bRet = JsonWriteFile(sFilePath, tblConfig, true);
	xrtValueRelease(tblConfig);
	xrtFree(sFilePath);
	if (bRet) Option_RebuildCache();
	if (bRet && !G_LogsCleanupSyncOK) {
		if (psError) *psError = xrtStrDup("配置已保存，但日志清理计划任务同步失败，请检查数据库后重新保存");
		return false;
	}

	return bRet;
}



bool Option_SaveDefinition(str sFileName, xvalue* tblConfig, bool bCreate, str* psError)
{
	str sFilePath;
	int iRet;

	if ( !Option_IsValidFileName(sFileName) ) {
		if ( psError ) *psError = xrtStrDup("文件名只能包含字母、数字、下划线或中划线，并以 .json 结尾");
		return false;
	}

	if ( bCreate ) {
		if ( Option_FileExists(sFileName) ) {
			if ( psError ) *psError = xrtStrDup("配置文件已存在");
			return false;
		}
	} else {
		if ( !Option_FileExists(sFileName) ) {
			if ( psError ) *psError = xrtStrDup("配置文件不存在");
			return false;
		}
	}

	if ( !bCreate ) {
		xvalue* tblOldConfig = Option_LoadFile(sFileName);
		if ( tblOldConfig == NULL ) {
			if ( psError ) *psError = xrtStrDup("配置文件不存在或解析失败");
			return false;
		}
		if ( Option_IsLockedConfig(tblOldConfig) ) {
			xrtValueRelease(tblOldConfig);
			if ( psError ) *psError = xrtStrDup("该配置文件已锁定，不允许修改结构");
			return false;
		}
		xrtValueRelease(tblOldConfig);
	}

	if ( !Option_ValidateConfig(sFileName, tblConfig, psError) ) {
		return false;
	}

	sFilePath = Option_BuildFilePath(sFileName);
	iRet = JsonWriteFile(sFilePath, tblConfig, true);
	xrtFree(sFilePath);
	if ( !iRet ) {
		if ( psError ) *psError = xrtStrDup("写入配置文件失败");
		return false;
	}

	Option_RebuildCache();
	return true;
}



bool Option_DeleteFile(str sFileName, str* psError)
{
	str sFilePath;
	bool bRet;
	xvalue* tblConfig;

	if ( !Option_IsValidFileName(sFileName) ) {
		if ( psError ) *psError = xrtStrDup("非法的文件名");
		return false;
	}
	if ( !Option_FileExists(sFileName) ) {
		if ( psError ) *psError = xrtStrDup("配置文件不存在");
		return false;
	}

	tblConfig = Option_LoadFile(sFileName);
	if ( tblConfig == NULL ) {
		if ( psError ) *psError = xrtStrDup("配置文件不存在或解析失败");
		return false;
	}
	if ( Option_IsLockedConfig(tblConfig) ) {
		xrtValueRelease(tblConfig);
		if ( psError ) *psError = xrtStrDup("该配置文件已锁定，不允许删除");
		return false;
	}
	xrtValueRelease(tblConfig);

	sFilePath = Option_BuildFilePath(sFileName);
	bRet = xrtFileDelete(sFilePath);
	xrtFree(sFilePath);
	if ( !bRet ) {
		if ( psError ) *psError = xrtStrDup("删除配置文件失败");
		return false;
	}

	Option_RebuildCache();
	return true;
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
		xrtValueRelease(G_Option);
		G_Option = NULL;
	}
	if ( G_AdminEntryPath != NULL ) {
		xrtFree(G_AdminEntryPath);
		G_AdminEntryPath = NULL;
	}
	G_AdminEntryEnabled = false;
}
