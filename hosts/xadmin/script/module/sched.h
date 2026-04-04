
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#define SCHED_YEAR_BASE 1970
#define SCHED_YEAR_SPAN 260

typedef struct SchedTaskSnapshot {
	int64 id;
	str sName;
	int enabled;
	str sScheduleType;
	str sExecType;
	str sShellType;
	str sCodeText;
	str sCustomText;
	str sCronExpr;
	int64 onceAt;
	int64 intervalValue;
	str sIntervalUnit;
	int64 startAt;
	int64 nextRunAt;
	int64 lastRunAt;
	int64 lastFinishAt;
	int timeoutSec;
	str sOverlapPolicy;
	str sMisfirePolicy;
	str sWorkDir;
	int isRunning;
	int runningCount;
	int pendingRun;
	int parallelLimit;
	int retryCount;
	int retryDelaySec;
	int retryState;
} SchedTaskSnapshot;

typedef struct SchedWorkerContext {
	SchedTaskSnapshot Task;
	str sTriggerSource;
} SchedWorkerContext;

typedef struct SchedCronExpr {
	bool second[60];
	bool minute[60];
	bool hour[24];
	bool day[32];
	bool month[13];
	bool weekday[7];
	bool year[SCHED_YEAR_SPAN];
	bool anyDay;
	bool anyWeekday;
	bool anyYear;
} SchedCronExpr;

static xthread G_SchedThread = NULL;
static xmutex G_SchedLock = NULL;
static xcond G_SchedCond = NULL;
static bool G_SchedStop = FALSE;
static int G_SchedWorkerCount = 0;
static str G_SchedPath = NULL;
static str G_SchedCachePath = NULL;
static str G_SchedXSPath = NULL;

typedef struct SchedTccErrorBuffer {
	str sText;
} SchedTccErrorBuffer;

static bool Sched_SaveTaskRequest(xvalue tblBody, bool bUpdate, str* psMessage, int64* pTaskId);
static bool Sched_StartTaskRunInternal(int64 id, const char* sTriggerSource, bool bAllowDisabled, str* psMessage);
bool Sched_RunNow(int64 id, str* psMessage);

static str Sched_CopyText(const char* sText)
{
	return xrtCopyStr(sText ? (str)sText : (str)"", 0);
}

static str Sched_StrOrEmpty(str sText)
{
	return sText ? sText : (str)"";
}

static const char* Sched_CStrOrEmpty(const char* sText)
{
	return sText ? sText : "";
}

static str Sched_XvoTextOrEmpty(xvalue tblValue, const char* sKey, size_t iKeyLen)
{
	return Sched_StrOrEmpty((str)xvoTableGetText(tblValue, sKey, iKeyLen));
}

static str Sched_SQLiteTextOrEmpty(sqlite3_stmt* stmt, int iCol)
{
	const unsigned char* sText = sqlite3_column_text(stmt, iCol);
	return (str)(sText ? sText : (const unsigned char*)"");
}

static str Sched_CopyBufferText(const void* pData, size_t iSize)
{
	str sText;

	if ( pData == NULL || iSize == 0 ) {
		return xrtCopyStr("", 0);
	}

	sText = (str)xrtMalloc(iSize + 1);
	memcpy(sText, pData, iSize);
	sText[iSize] = '\0';
	return sText;
}

static str Sched_CopySlashPath(const char* sPath)
{
	str sCopy;
	size_t i;

	sCopy = xrtCopyStr(sPath ? (str)sPath : (str)"", 0);
	if ( sCopy == NULL ) {
		return NULL;
	}
	for ( i = 0; sCopy[i] != '\0'; i++ ) {
		if ( sCopy[i] == '\\' ) {
			sCopy[i] = '/';
		}
	}
	return sCopy;
}

static str Sched_EscapeCString(const char* sText)
{
	const unsigned char* pSrc = (const unsigned char*)(sText ? sText : "");
	size_t iLen = strlen((const char*)pSrc);
	str sOut = (str)xrtMalloc(iLen * 2 + 1);
	size_t iIn = 0;
	size_t iOut = 0;

	while ( iIn < iLen ) {
		unsigned char ch = pSrc[iIn++];
		if ( ch == '\\' || ch == '"' ) {
			sOut[iOut++] = '\\';
			sOut[iOut++] = (char)ch;
		} else if ( ch == '\r' ) {
			sOut[iOut++] = '\\';
			sOut[iOut++] = 'r';
		} else if ( ch == '\n' ) {
			sOut[iOut++] = '\\';
			sOut[iOut++] = 'n';
		} else if ( ch == '\t' ) {
			sOut[iOut++] = '\\';
			sOut[iOut++] = 't';
		} else {
			sOut[iOut++] = (char)ch;
		}
	}
	sOut[iOut] = '\0';
	return sOut;
}

static const char* Sched_PathFileName(const char* sPath)
{
	const char* sSlash;
	const char* sBackslash;
	const char* sName;

	if ( sPath == NULL || sPath[0] == '\0' ) {
		return "";
	}

	sSlash = strrchr(sPath, '/');
	sBackslash = strrchr(sPath, '\\');
	sName = sSlash;
	if ( sBackslash && (sName == NULL || sBackslash > sName) ) {
		sName = sBackslash;
	}
	return sName ? (sName + 1) : sPath;
}

static str Sched_RenderRunnerTemplate(const char* sTaskSourceCode)
{
	xvalue tblData;
	str sOutput;

	tblData = xvoCreateTable();
	xvoTableSetText(tblData, "taskSourceCode", 14, sTaskSourceCode ? (str)sTaskSourceCode : (str)"", 0, FALSE);
	sOutput = MakeTextWithTemplate("sched/c_task_runner.c", tblData, NULL);
	xvoUnref(tblData);
	return sOutput;
}

static bool Sched_TextEquals(const char* a, const char* b)
{
	if ( a == NULL || b == NULL ) {
		return FALSE;
	}
	return strcmp(a, b) == 0;
}

static const char* Sched_GetWorkDir(const SchedTaskSnapshot* pTask)
{
	if ( pTask && pTask->sWorkDir && pTask->sWorkDir[0] != '\0' ) {
		return pTask->sWorkDir;
	}
	if ( AppPath && AppPath[0] != '\0' ) {
		return AppPath;
	}
	return ".";
}

static uint32 Sched_GetTimeoutMs(const SchedTaskSnapshot* pTask)
{
	int64 iTimeoutSec = 300;

	if ( pTask && pTask->timeoutSec > 0 ) {
		iTimeoutSec = pTask->timeoutSec;
	}
	if ( iTimeoutSec > 86400 ) {
		iTimeoutSec = 86400;
	}
	return (uint32)(iTimeoutSec * 1000);
}

static void Sched_FreeTaskSnapshot(SchedTaskSnapshot* pTask)
{
	if ( pTask == NULL ) {
		return;
	}

	if ( pTask->sName ) xrtFree(pTask->sName);
	if ( pTask->sScheduleType ) xrtFree(pTask->sScheduleType);
	if ( pTask->sExecType ) xrtFree(pTask->sExecType);
	if ( pTask->sShellType ) xrtFree(pTask->sShellType);
	if ( pTask->sCodeText ) xrtFree(pTask->sCodeText);
	if ( pTask->sCustomText ) xrtFree(pTask->sCustomText);
	if ( pTask->sCronExpr ) xrtFree(pTask->sCronExpr);
	if ( pTask->sIntervalUnit ) xrtFree(pTask->sIntervalUnit);
	if ( pTask->sOverlapPolicy ) xrtFree(pTask->sOverlapPolicy);
	if ( pTask->sMisfirePolicy ) xrtFree(pTask->sMisfirePolicy);
	if ( pTask->sWorkDir ) xrtFree(pTask->sWorkDir);
	memset(pTask, 0, sizeof(*pTask));
}

static void Sched_FreeWorkerContext(SchedWorkerContext* pCtx)
{
	if ( pCtx == NULL ) {
		return;
	}

	Sched_FreeTaskSnapshot(&pCtx->Task);
	if ( pCtx->sTriggerSource ) xrtFree(pCtx->sTriggerSource);
	xrtFree(pCtx);
}

static sqlite3* Sched_OpenStandaloneDB(void)
{
	sqlite3* pDB = NULL;
	int iRet;
	str sFile = xrtPathJoin(2, DBPath, "main.db");

	iRet = sqlite3_open(sFile, &pDB);
	xrtFree(sFile);
	if ( iRet != SQLITE_OK ) {
		if ( pDB ) {
			sqlite3_close(pDB);
		}
		return NULL;
	}

	sqlite3_busy_timeout(pDB, 5000);
	sqlite3_exec(pDB, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
	return pDB;
}

static bool Sched_ExecSQL(sqlite3* pDB, const char* sSQL)
{
	char* sErr = NULL;
	int iRet;

	if ( pDB == NULL || sSQL == NULL ) {
		return FALSE;
	}

	iRet = sqlite3_exec(pDB, sSQL, NULL, NULL, &sErr);
	if ( iRet != SQLITE_OK ) {
		if ( sErr ) {
			printf("[sched] sql error: %s\n", sErr);
			sqlite3_free(sErr);
		}
		return FALSE;
	}
	return TRUE;
}

static bool Sched_TableColumnExists(sqlite3* pDB, const char* sTableName, const char* sColumnName)
{
	sqlite3_stmt* stmt = NULL;
	str sSQL = NULL;
	bool bExists = FALSE;

	if ( pDB == NULL || sTableName == NULL || sColumnName == NULL ) {
		return FALSE;
	}

	sSQL = xrtFormat("PRAGMA table_info(%s)", sTableName);
	if ( sSQL == NULL ) {
		return FALSE;
	}
	if ( sqlite3_prepare_v3(pDB, sSQL, -1, 0, &stmt, NULL) != SQLITE_OK ) {
		xrtFree(sSQL);
		return FALSE;
	}
	xrtFree(sSQL);

	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		const char* sName = (const char*)sqlite3_column_text(stmt, 1);
		if ( sName && strcmp(sName, sColumnName) == 0 ) {
			bExists = TRUE;
			break;
		}
	}
	sqlite3_finalize(stmt);
	return bExists;
}

static void Sched_EnsureColumn(sqlite3* pDB, const char* sTableName, const char* sColumnName, const char* sColumnDef)
{
	str sSQL = NULL;

	if ( pDB == NULL || sTableName == NULL || sColumnName == NULL || sColumnDef == NULL ) {
		return;
	}
	if ( Sched_TableColumnExists(pDB, sTableName, sColumnName) ) {
		return;
	}

	sSQL = xrtFormat("ALTER TABLE %s ADD COLUMN %s %s", sTableName, sColumnName, sColumnDef);
	if ( sSQL ) {
		Sched_ExecSQL(pDB, sSQL);
		xrtFree(sSQL);
	}
}

void Sched_NotifyChanged(void)
{
	if ( G_SchedLock == NULL || G_SchedCond == NULL ) {
		return;
	}

	xrtMutexLock(G_SchedLock);
	xrtCondBroadcast(G_SchedCond);
	xrtMutexUnlock(G_SchedLock);
}

static void Sched_SendJson(XS_ResponseObject objResp, xvalue tblRet)
{
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(objResp, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}

static str Sched_TimeText(int64 iTime)
{
	if ( iTime <= 0 ) {
		return xrtCopyStr("", 0);
	}
	return xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME);
}

static void Sched_SetTimeFields(xvalue tblRow, const char* sKeyTime, const char* sKeyText, int64 iValue)
{
	str sText;
	xvoTableSetInt(tblRow, sKeyTime, 0, iValue);
	sText = Sched_TimeText(iValue);
	xvoTableSetText(tblRow, sKeyText, 0, sText, 0, TRUE);
}

static int64 Sched_IntervalSeconds(int64 iValue, const char* sUnit)
{
	if ( iValue <= 0 ) {
		return 0;
	}
	if ( Sched_TextEquals(sUnit, "second") ) return iValue;
	if ( Sched_TextEquals(sUnit, "minute") ) return iValue * 60;
	if ( Sched_TextEquals(sUnit, "hour") ) return iValue * 3600;
	if ( Sched_TextEquals(sUnit, "day") ) return iValue * 86400;
	return 0;
}

static bool Sched_ParseCronNumber(const char* sText, int iMin, int iMax, bool bWeekday, int* pValue)
{
	int iValue;

	if ( sText == NULL || sText[0] == '\0' ) {
		return FALSE;
	}

	iValue = atoi(sText);
	if ( bWeekday && iValue == 7 ) {
		iValue = 0;
	}
	if ( iValue < iMin || iValue > iMax ) {
		return FALSE;
	}
	*pValue = iValue;
	return TRUE;
}

static char* Sched_StrTokNext(char* sText, const char* sDelim, char** psContext)
{
	char* sBegin;
	char* sEnd;

	if ( psContext == NULL || sDelim == NULL ) {
		return NULL;
	}
	if ( sText != NULL ) {
		*psContext = sText;
	}

	sBegin = *psContext;
	if ( sBegin == NULL ) {
		return NULL;
	}

	sBegin += strspn(sBegin, sDelim);
	if ( sBegin[0] == '\0' ) {
		*psContext = NULL;
		return NULL;
	}

	sEnd = sBegin + strcspn(sBegin, sDelim);
	if ( sEnd[0] == '\0' ) {
		*psContext = NULL;
	} else {
		*sEnd = '\0';
		*psContext = sEnd + 1;
	}

	return sBegin;
}

static bool Sched_ParseCronField(const char* sField, bool* arrAllowed, int iMin, int iMax, bool* pIsWildcard, bool bWeekday)
{
	char* sCopy;
	char* sPart;
	char* sContext = NULL;

	if ( sField == NULL || sField[0] == '\0' ) {
		return FALSE;
	}

	memset(arrAllowed, 0, (size_t)(iMax + 1) * sizeof(bool));
	*pIsWildcard = FALSE;

	sCopy = xrtCopyStr((str)sField, 0);
	sPart = Sched_StrTokNext(sCopy, ",", &sContext);
	while ( sPart ) {
		char* sSlash = strchr(sPart, '/');
		char* sDash = strchr(sPart, '-');
		int iStep = 1;
		int iStart = 0;
		int iEnd = 0;

		if ( sSlash ) {
			*sSlash = '\0';
			iStep = atoi(sSlash + 1);
			if ( iStep <= 0 ) {
				xrtFree(sCopy);
				return FALSE;
			}
		}

		if ( strcmp(sPart, "*") == 0 ) {
			iStart = iMin;
			iEnd = iMax;
			*pIsWildcard = TRUE;
		} else if ( sDash ) {
			int iValueStart;
			int iValueEnd;
			*sDash = '\0';
			if ( !Sched_ParseCronNumber(sPart, iMin, iMax, bWeekday, &iValueStart) ) {
				xrtFree(sCopy);
				return FALSE;
			}
			if ( !Sched_ParseCronNumber(sDash + 1, iMin, iMax, bWeekday, &iValueEnd) ) {
				xrtFree(sCopy);
				return FALSE;
			}
			if ( iValueEnd < iValueStart ) {
				xrtFree(sCopy);
				return FALSE;
			}
			iStart = iValueStart;
			iEnd = iValueEnd;
		} else {
			int iValue;
			if ( !Sched_ParseCronNumber(sPart, iMin, iMax, bWeekday, &iValue) ) {
				xrtFree(sCopy);
				return FALSE;
			}
			iStart = iValue;
			iEnd = iValue;
		}

		for ( int i = iStart; i <= iEnd; i += iStep ) {
			arrAllowed[i] = TRUE;
		}

		sPart = Sched_StrTokNext(NULL, ",", &sContext);
	}
	xrtFree(sCopy);
	return TRUE;
}

static bool Sched_ParseCronYearField(const char* sField, SchedCronExpr* pCron)
{
	bool bWildcard = FALSE;

	memset(pCron->year, 0, sizeof(pCron->year));
	if ( sField == NULL || sField[0] == '\0' || strcmp(sField, "*") == 0 ) {
		pCron->anyYear = TRUE;
		for ( int i = 0; i < SCHED_YEAR_SPAN; i++ ) {
			pCron->year[i] = TRUE;
		}
		return TRUE;
	}

	{
		char* sCopy = xrtCopyStr((str)sField, 0);
		char* sPart;
		char* sContext = NULL;

		sPart = Sched_StrTokNext(sCopy, ",", &sContext);
		while ( sPart ) {
			char* sSlash = strchr(sPart, '/');
			char* sDash = strchr(sPart, '-');
			int iStep = 1;
			int iStart;
			int iEnd;

			if ( sSlash ) {
				*sSlash = '\0';
				iStep = atoi(sSlash + 1);
				if ( iStep <= 0 ) {
					xrtFree(sCopy);
					return FALSE;
				}
			}

			if ( strcmp(sPart, "*") == 0 ) {
				iStart = SCHED_YEAR_BASE;
				iEnd = SCHED_YEAR_BASE + SCHED_YEAR_SPAN - 1;
				bWildcard = TRUE;
			} else if ( sDash ) {
				*sDash = '\0';
				iStart = atoi(sPart);
				iEnd = atoi(sDash + 1);
				if ( iEnd < iStart ) {
					xrtFree(sCopy);
					return FALSE;
				}
			} else {
				iStart = atoi(sPart);
				iEnd = iStart;
			}

			if ( iStart < SCHED_YEAR_BASE ) iStart = SCHED_YEAR_BASE;
			if ( iEnd > (SCHED_YEAR_BASE + SCHED_YEAR_SPAN - 1) ) iEnd = SCHED_YEAR_BASE + SCHED_YEAR_SPAN - 1;
			for ( int iYear = iStart; iYear <= iEnd; iYear += iStep ) {
				pCron->year[iYear - SCHED_YEAR_BASE] = TRUE;
			}

			sPart = Sched_StrTokNext(NULL, ",", &sContext);
		}
		xrtFree(sCopy);
	}

	pCron->anyYear = bWildcard;
	return TRUE;
}

static bool Sched_ParseCronExpr(const char* sExpr, SchedCronExpr* pCron)
{
	char* sCopy;
	char* sToken;
	char* sContext = NULL;
	char* arrField[7] = {0};
	int iCount = 0;
	bool bWildcard = FALSE;

	if ( sExpr == NULL || sExpr[0] == '\0' || pCron == NULL ) {
		return FALSE;
	}

	memset(pCron, 0, sizeof(*pCron));
	sCopy = xrtCopyStr((str)sExpr, 0);
	sToken = Sched_StrTokNext(sCopy, " \t\r\n", &sContext);
	while ( sToken && iCount < 7 ) {
		arrField[iCount++] = sToken;
		sToken = Sched_StrTokNext(NULL, " \t\r\n", &sContext);
	}
	if ( sToken != NULL || (iCount != 6 && iCount != 7) ) {
		xrtFree(sCopy);
		return FALSE;
	}

	if ( !Sched_ParseCronField(arrField[0], pCron->second, 0, 59, &bWildcard, FALSE) ) { xrtFree(sCopy); return FALSE; }
	if ( !Sched_ParseCronField(arrField[1], pCron->minute, 0, 59, &bWildcard, FALSE) ) { xrtFree(sCopy); return FALSE; }
	if ( !Sched_ParseCronField(arrField[2], pCron->hour, 0, 23, &bWildcard, FALSE) ) { xrtFree(sCopy); return FALSE; }
	if ( !Sched_ParseCronField(arrField[3], pCron->day, 1, 31, &pCron->anyDay, FALSE) ) { xrtFree(sCopy); return FALSE; }
	if ( !Sched_ParseCronField(arrField[4], pCron->month, 1, 12, &bWildcard, FALSE) ) { xrtFree(sCopy); return FALSE; }
	if ( !Sched_ParseCronField(arrField[5], pCron->weekday, 0, 6, &pCron->anyWeekday, TRUE) ) { xrtFree(sCopy); return FALSE; }
	if ( iCount == 7 ) {
		if ( !Sched_ParseCronYearField(arrField[6], pCron) ) {
			xrtFree(sCopy);
			return FALSE;
		}
	} else {
		pCron->anyYear = TRUE;
		for ( int i = 0; i < SCHED_YEAR_SPAN; i++ ) {
			pCron->year[i] = TRUE;
		}
	}

	xrtFree(sCopy);
	return TRUE;
}

static bool Sched_CronYearAllowed(const SchedCronExpr* pCron, int64 iYear)
{
	if ( pCron->anyYear ) {
		return TRUE;
	}
	if ( iYear < SCHED_YEAR_BASE || iYear >= (SCHED_YEAR_BASE + SCHED_YEAR_SPAN) ) {
		return FALSE;
	}
	return pCron->year[iYear - SCHED_YEAR_BASE];
}

static int Sched_NextAllowed(const bool* arrAllowed, int iMin, int iMax, int iCurrent)
{
	for ( int i = iCurrent; i <= iMax; i++ ) {
		if ( arrAllowed[i] ) {
			return i;
		}
	}
	return -1;
}

static int Sched_NormalizeWeekday(int iWeekday)
{
	if ( iWeekday == 7 ) return 0;
	if ( iWeekday < 0 ) return 0;
	if ( iWeekday > 6 ) return iWeekday % 7;
	return iWeekday;
}

static bool Sched_CronDayMatches(const SchedCronExpr* pCron, int iDay, int iWeekday)
{
	bool bDay = pCron->day[iDay];
	bool bWeekday = pCron->weekday[iWeekday];

	if ( pCron->anyDay && pCron->anyWeekday ) return TRUE;
	if ( pCron->anyDay ) return bWeekday;
	if ( pCron->anyWeekday ) return bDay;
	return bDay || bWeekday;
}

static int64 Sched_CalcCronNextTime(const char* sExpr, int64 iAfter)
{
	SchedCronExpr tCron;
	int64 iYear;
	int iMonth;
	int iDay;
	int iHour;
	int iMinute;
	int iSecond;
	int iWeekday;
	int iDayOfYear;

	if ( !Sched_ParseCronExpr(sExpr, &tCron) ) {
		return 0;
	}

	xrtDecodeSerial(iAfter + 1, &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, &iWeekday, &iDayOfYear);
	iWeekday = Sched_NormalizeWeekday(iWeekday);

	while ( iYear < (SCHED_YEAR_BASE + SCHED_YEAR_SPAN) ) {
		int iDaysInMonth;
		int iNext;

		if ( iMonth > 12 ) {
			iYear++;
			iMonth = 1;
			iDay = 1;
			iHour = 0;
			iMinute = 0;
			iSecond = 0;
			continue;
		}
		if ( !Sched_CronYearAllowed(&tCron, iYear) ) {
			iYear++;
			iMonth = 1;
			iDay = 1;
			iHour = 0;
			iMinute = 0;
			iSecond = 0;
			continue;
		}

		iNext = Sched_NextAllowed(tCron.month, 1, 12, iMonth);
		if ( iNext < 0 ) {
			iYear++;
			iMonth = 1;
			iDay = 1;
			iHour = 0;
			iMinute = 0;
			iSecond = 0;
			continue;
		}
		if ( iNext != iMonth ) {
			iMonth = iNext;
			iDay = 1;
			iHour = 0;
			iMinute = 0;
			iSecond = 0;
		}

		iDaysInMonth = xrtDaysInMonth((int)iYear, iMonth);
		if ( iDay > iDaysInMonth ) {
			iMonth++;
			iDay = 1;
			iHour = 0;
			iMinute = 0;
			iSecond = 0;
			continue;
		}

		{
			bool bMatched = FALSE;
			while ( iDay <= iDaysInMonth ) {
				int iDayWeekday = Sched_NormalizeWeekday(xrtWeekday(xrtDateSerial(iYear, iMonth, iDay)));
				if ( Sched_CronDayMatches(&tCron, iDay, iDayWeekday) ) {
					bMatched = TRUE;
					break;
				}
				iDay++;
				iHour = 0;
				iMinute = 0;
				iSecond = 0;
			}
			if ( !bMatched ) {
				iMonth++;
				iDay = 1;
				iHour = 0;
				iMinute = 0;
				iSecond = 0;
				continue;
			}
		}

		iNext = Sched_NextAllowed(tCron.hour, 0, 23, iHour);
		if ( iNext < 0 ) {
			iDay++;
			iHour = 0;
			iMinute = 0;
			iSecond = 0;
			continue;
		}
		if ( iNext != iHour ) {
			iHour = iNext;
			iMinute = 0;
			iSecond = 0;
		}

		iNext = Sched_NextAllowed(tCron.minute, 0, 59, iMinute);
		if ( iNext < 0 ) {
			iHour++;
			iMinute = 0;
			iSecond = 0;
			continue;
		}
		if ( iNext != iMinute ) {
			iMinute = iNext;
			iSecond = 0;
		}

		iNext = Sched_NextAllowed(tCron.second, 0, 59, iSecond);
		if ( iNext < 0 ) {
			iMinute++;
			iSecond = 0;
			continue;
		}
		iSecond = iNext;

		return xrtDateTimeSerial(iYear, iMonth, iDay, iHour, iMinute, iSecond);
	}

	return 0;
}

static int64 Sched_CalcNextTime(const SchedTaskSnapshot* pTask, int64 iBaseTime)
{
	if ( pTask == NULL || !pTask->enabled ) {
		return 0;
	}
	if ( Sched_TextEquals(pTask->sScheduleType, "once") ) return pTask->onceAt;
	if ( Sched_TextEquals(pTask->sScheduleType, "interval") ) {
		int64 iSeconds = Sched_IntervalSeconds(pTask->intervalValue, pTask->sIntervalUnit);
		int64 iAnchor = pTask->startAt > 0 ? pTask->startAt : iBaseTime;
		if ( iSeconds <= 0 ) return 0;
		return (iAnchor <= iBaseTime) ? (iBaseTime + iSeconds) : iAnchor;
	}
	if ( Sched_TextEquals(pTask->sScheduleType, "cron") ) return Sched_CalcCronNextTime(pTask->sCronExpr, iBaseTime);
	return 0;
}

static int64 Sched_CalcNextAfterRun(const SchedTaskSnapshot* pTask, int64 iFinishTime)
{
	if ( pTask == NULL ) {
		return 0;
	}
	if ( Sched_TextEquals(pTask->sScheduleType, "once") ) return 0;
	if ( Sched_TextEquals(pTask->sScheduleType, "interval") ) {
		int64 iSeconds = Sched_IntervalSeconds(pTask->intervalValue, pTask->sIntervalUnit);
		return (iSeconds > 0) ? (iFinishTime + iSeconds) : 0;
	}
	if ( Sched_TextEquals(pTask->sScheduleType, "cron") ) return Sched_CalcCronNextTime(pTask->sCronExpr, iFinishTime);
	return 0;
}

static int64 Sched_CalcBusyNext(const SchedTaskSnapshot* pTask, int64 iNow)
{
	if ( pTask == NULL ) {
		return 0;
	}
	if ( Sched_TextEquals(pTask->sScheduleType, "once") ) return 0;
	if ( Sched_TextEquals(pTask->sScheduleType, "interval") ) {
		int64 iSeconds = Sched_IntervalSeconds(pTask->intervalValue, pTask->sIntervalUnit);
		int64 iNext = pTask->nextRunAt;
		if ( iSeconds <= 0 ) return 0;
		if ( iNext <= 0 ) iNext = iNow + iSeconds;
		while ( iNext <= iNow ) {
			iNext += iSeconds;
		}
		return iNext;
	}
	if ( Sched_TextEquals(pTask->sScheduleType, "cron") ) return Sched_CalcCronNextTime(pTask->sCronExpr, iNow);
	return 0;
}

static bool Sched_IsOverlapPolicy(const SchedTaskSnapshot* pTask, const char* sPolicy)
{
	return pTask && Sched_TextEquals(pTask->sOverlapPolicy, sPolicy);
}

static bool Sched_IsMisfirePolicy(const SchedTaskSnapshot* pTask, const char* sPolicy)
{
	return pTask && Sched_TextEquals(pTask->sMisfirePolicy, sPolicy);
}

static bool Sched_TaskCanRetry(const SchedTaskSnapshot* pTask, const char* sFinalStatus)
{
	if ( pTask == NULL || sFinalStatus == NULL ) {
		return FALSE;
	}
	if ( pTask->retryCount <= 0 ) {
		return FALSE;
	}
	if ( !pTask->enabled ) {
		return FALSE;
	}
	if ( Sched_IsOverlapPolicy(pTask, "parallel") ) {
		return FALSE;
	}
	if ( !Sched_TextEquals(sFinalStatus, "failed") && !Sched_TextEquals(sFinalStatus, "timeout") ) {
		return FALSE;
	}
	return pTask->retryState < pTask->retryCount;
}

static bool Sched_LoadTaskSnapshotById(sqlite3* pDB, int64 id, SchedTaskSnapshot* pTask)
{
	sqlite3_stmt* stmt = NULL;
	int iRet;

	if ( pDB == NULL || pTask == NULL || id <= 0 ) {
		return FALSE;
	}
	memset(pTask, 0, sizeof(*pTask));

	iRet = sqlite3_prepare_v3(pDB, "SELECT id, name, enabled, scheduleType, execType, shellType, codeText, customText, cronExpr, onceAt, intervalValue, intervalUnit, startAt, nextRunAt, lastRunAt, lastFinishAt, timeoutSec, overlapPolicy, misfirePolicy, workDir, isRunning, runningCount, pendingRun, parallelLimit, retryCount, retryDelaySec, retryState FROM sched_task WHERE id = ? AND isDelete = 0", -1, 0, &stmt, NULL);
	if ( iRet != SQLITE_OK ) {
		return FALSE;
	}

	sqlite3_bind_int64(stmt, 1, id);
	if ( sqlite3_step(stmt) != SQLITE_ROW ) {
		sqlite3_finalize(stmt);
		return FALSE;
	}

	pTask->id = sqlite3_column_int64(stmt, 0);
	pTask->sName = Sched_CopyText((const char*)sqlite3_column_text(stmt, 1));
	pTask->enabled = sqlite3_column_int(stmt, 2);
	pTask->sScheduleType = Sched_CopyText((const char*)sqlite3_column_text(stmt, 3));
	pTask->sExecType = Sched_CopyText((const char*)sqlite3_column_text(stmt, 4));
	pTask->sShellType = Sched_CopyText((const char*)sqlite3_column_text(stmt, 5));
	pTask->sCodeText = Sched_CopyText((const char*)sqlite3_column_text(stmt, 6));
	pTask->sCustomText = Sched_CopyText((const char*)sqlite3_column_text(stmt, 7));
	pTask->sCronExpr = Sched_CopyText((const char*)sqlite3_column_text(stmt, 8));
	pTask->onceAt = sqlite3_column_int64(stmt, 9);
	pTask->intervalValue = sqlite3_column_int64(stmt, 10);
	pTask->sIntervalUnit = Sched_CopyText((const char*)sqlite3_column_text(stmt, 11));
	pTask->startAt = sqlite3_column_int64(stmt, 12);
	pTask->nextRunAt = sqlite3_column_int64(stmt, 13);
	pTask->lastRunAt = sqlite3_column_int64(stmt, 14);
	pTask->lastFinishAt = sqlite3_column_int64(stmt, 15);
	pTask->timeoutSec = sqlite3_column_int(stmt, 16);
	pTask->sOverlapPolicy = Sched_CopyText((const char*)sqlite3_column_text(stmt, 17));
	pTask->sMisfirePolicy = Sched_CopyText((const char*)sqlite3_column_text(stmt, 18));
	pTask->sWorkDir = Sched_CopyText((const char*)sqlite3_column_text(stmt, 19));
	pTask->isRunning = sqlite3_column_int(stmt, 20);
	pTask->runningCount = sqlite3_column_int(stmt, 21);
	pTask->pendingRun = sqlite3_column_int(stmt, 22);
	pTask->parallelLimit = sqlite3_column_int(stmt, 23);
	pTask->retryCount = sqlite3_column_int(stmt, 24);
	pTask->retryDelaySec = sqlite3_column_int(stmt, 25);
	pTask->retryState = sqlite3_column_int(stmt, 26);
	sqlite3_finalize(stmt);
	return TRUE;
}

static xvalue Sched_TaskSnapshotToValue(const SchedTaskSnapshot* pTask)
{
	xvalue tblRow = xvoCreateTable();
	str sOnceText = Sched_TimeText(pTask->onceAt);
	str sStartText = Sched_TimeText(pTask->startAt);

	xvoTableSetInt(tblRow, "id", 2, pTask->id);
	xvoTableSetText(tblRow, "name", 4, Sched_StrOrEmpty(pTask->sName), 0, FALSE);
	xvoTableSetInt(tblRow, "enabled", 7, pTask->enabled);
	xvoTableSetText(tblRow, "scheduleType", 12, Sched_StrOrEmpty(pTask->sScheduleType), 0, FALSE);
	xvoTableSetText(tblRow, "execType", 8, Sched_StrOrEmpty(pTask->sExecType), 0, FALSE);
	xvoTableSetText(tblRow, "shellType", 9, Sched_StrOrEmpty(pTask->sShellType), 0, FALSE);
	xvoTableSetText(tblRow, "codeText", 8, Sched_StrOrEmpty(pTask->sCodeText), 0, FALSE);
	xvoTableSetText(tblRow, "customText", 10, Sched_StrOrEmpty(pTask->sCustomText), 0, FALSE);
	xvoTableSetText(tblRow, "cronExpr", 8, Sched_StrOrEmpty(pTask->sCronExpr), 0, FALSE);
	xvoTableSetInt(tblRow, "onceAt", 6, pTask->onceAt);
	xvoTableSetText(tblRow, "onceAtText", 10, sOnceText, 0, TRUE);
	xvoTableSetInt(tblRow, "intervalValue", 13, pTask->intervalValue);
	xvoTableSetText(tblRow, "intervalUnit", 12, Sched_StrOrEmpty(pTask->sIntervalUnit), 0, FALSE);
	xvoTableSetInt(tblRow, "startAt", 7, pTask->startAt);
	xvoTableSetText(tblRow, "startAtText", 11, sStartText, 0, TRUE);
	xvoTableSetInt(tblRow, "nextRunAt", 9, pTask->nextRunAt);
	xvoTableSetInt(tblRow, "lastRunAt", 9, pTask->lastRunAt);
	xvoTableSetInt(tblRow, "lastFinishAt", 12, pTask->lastFinishAt);
	xvoTableSetInt(tblRow, "timeoutSec", 10, pTask->timeoutSec);
	xvoTableSetText(tblRow, "overlapPolicy", 13, Sched_StrOrEmpty(pTask->sOverlapPolicy), 0, FALSE);
	xvoTableSetText(tblRow, "misfirePolicy", 13, Sched_StrOrEmpty(pTask->sMisfirePolicy), 0, FALSE);
	xvoTableSetText(tblRow, "workDir", 7, Sched_StrOrEmpty(pTask->sWorkDir), 0, FALSE);
	xvoTableSetInt(tblRow, "isRunning", 9, pTask->isRunning);
	xvoTableSetInt(tblRow, "runningCount", 12, pTask->runningCount);
	xvoTableSetInt(tblRow, "pendingRun", 10, pTask->pendingRun);
	xvoTableSetInt(tblRow, "parallelLimit", 13, pTask->parallelLimit);
	xvoTableSetInt(tblRow, "retryCount", 10, pTask->retryCount);
	xvoTableSetInt(tblRow, "retryDelaySec", 13, pTask->retryDelaySec);
	xvoTableSetInt(tblRow, "retryState", 10, pTask->retryState);
	Sched_SetTimeFields(tblRow, "nextRunAt", "nextRunText", pTask->nextRunAt);
	Sched_SetTimeFields(tblRow, "lastRunAt", "lastRunText", pTask->lastRunAt);
	Sched_SetTimeFields(tblRow, "lastFinishAt", "lastFinishText", pTask->lastFinishAt);
	return tblRow;
}

static bool Sched_TaskIsValidForSave(SchedTaskSnapshot* pTask, str* psMessage)
{
	if ( pTask->sName == NULL || pTask->sName[0] == '\0' ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Task name is required", 0);
		return FALSE;
	}
	if ( !Sched_TextEquals(pTask->sScheduleType, "once") && !Sched_TextEquals(pTask->sScheduleType, "interval") && !Sched_TextEquals(pTask->sScheduleType, "cron") ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Unsupported schedule type", 0);
		return FALSE;
	}
	if ( !Sched_TextEquals(pTask->sExecType, "shell") && !Sched_TextEquals(pTask->sExecType, "c") ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Unsupported exec type", 0);
		return FALSE;
	}
	if ( pTask->sCodeText == NULL || pTask->sCodeText[0] == '\0' ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Task code is required", 0);
		return FALSE;
	}
	if ( pTask->sCustomText && pTask->sCustomText[0] != '\0' ) {
		xvalue objCustom = xrtParseJSON(pTask->sCustomText, strlen(pTask->sCustomText));
		if ( objCustom == NULL ) {
			if ( psMessage ) *psMessage = xrtCopyStr("Custom params must be valid JSON", 0);
			return FALSE;
		}
		xvoUnref(objCustom);
	}
	if ( Sched_TextEquals(pTask->sScheduleType, "once") && pTask->onceAt <= 0 ) {
		if ( psMessage ) *psMessage = xrtCopyStr("onceAt is required", 0);
		return FALSE;
	}
	if ( Sched_TextEquals(pTask->sScheduleType, "interval") && Sched_IntervalSeconds(pTask->intervalValue, pTask->sIntervalUnit) <= 0 ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Interval config is invalid", 0);
		return FALSE;
	}
	if ( Sched_TextEquals(pTask->sScheduleType, "cron") ) {
		SchedCronExpr tCron;
		if ( !Sched_ParseCronExpr(pTask->sCronExpr, &tCron) ) {
			if ( psMessage ) *psMessage = xrtCopyStr("Cron expression is invalid", 0);
			return FALSE;
		}
	}

	if ( pTask->timeoutSec <= 0 ) pTask->timeoutSec = 300;
	if ( pTask->sOverlapPolicy == NULL || pTask->sOverlapPolicy[0] == '\0' ) {
		if ( pTask->sOverlapPolicy ) xrtFree(pTask->sOverlapPolicy);
		pTask->sOverlapPolicy = xrtCopyStr("skip", 0);
	}
	if ( !Sched_TextEquals(pTask->sOverlapPolicy, "skip") && !Sched_TextEquals(pTask->sOverlapPolicy, "queue_one") && !Sched_TextEquals(pTask->sOverlapPolicy, "parallel") ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Unsupported overlap policy", 0);
		return FALSE;
	}
	if ( pTask->sMisfirePolicy == NULL || pTask->sMisfirePolicy[0] == '\0' ) {
		if ( pTask->sMisfirePolicy ) xrtFree(pTask->sMisfirePolicy);
		pTask->sMisfirePolicy = xrtCopyStr("skip", 0);
	}
	if ( !Sched_TextEquals(pTask->sMisfirePolicy, "skip") && !Sched_TextEquals(pTask->sMisfirePolicy, "fire_once_now") ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Unsupported misfire policy", 0);
		return FALSE;
	}
	if ( pTask->parallelLimit < 0 ) pTask->parallelLimit = 0;
	if ( pTask->retryCount < 0 ) pTask->retryCount = 0;
	if ( pTask->retryCount > 20 ) pTask->retryCount = 20;
	if ( pTask->retryDelaySec < 0 ) pTask->retryDelaySec = 0;
	if ( pTask->retryDelaySec > 86400 ) pTask->retryDelaySec = 86400;
	if ( pTask->sShellType == NULL || pTask->sShellType[0] == '\0' ) {
		if ( pTask->sShellType ) xrtFree(pTask->sShellType);
		#if defined(_WIN32) || defined(_WIN64)
			pTask->sShellType = xrtCopyStr("powershell", 0);
		#else
			pTask->sShellType = xrtCopyStr("sh", 0);
		#endif
	}
	if ( pTask->sWorkDir == NULL || pTask->sWorkDir[0] == '\0' ) {
		if ( pTask->sWorkDir ) xrtFree(pTask->sWorkDir);
		pTask->sWorkDir = xrtCopyStr(AppPath, 0);
	}
	return TRUE;
}

static void Sched_ReadTaskFieldFromBody(xvalue tblBody, SchedTaskSnapshot* pTask)
{
	str sOnceText;
	str sStartText;
	pTask->id = xvoTableGetInt(tblBody, "id", 2);
	pTask->enabled = xvoTableGetInt(tblBody, "enabled", 7);
	pTask->onceAt = xvoTableGetInt(tblBody, "onceAt", 6);
	pTask->intervalValue = xvoTableGetInt(tblBody, "intervalValue", 13);
	pTask->startAt = xvoTableGetInt(tblBody, "startAt", 7);
	pTask->timeoutSec = xvoTableGetInt(tblBody, "timeoutSec", 10);
	pTask->parallelLimit = xvoTableGetInt(tblBody, "parallelLimit", 13);
	pTask->retryCount = xvoTableGetInt(tblBody, "retryCount", 10);
	pTask->retryDelaySec = xvoTableGetInt(tblBody, "retryDelaySec", 13);
	pTask->sName = Sched_CopyText(xvoTableGetText(tblBody, "name", 4));
	pTask->sScheduleType = Sched_CopyText(xvoTableGetText(tblBody, "scheduleType", 12));
	pTask->sExecType = Sched_CopyText(xvoTableGetText(tblBody, "execType", 8));
	pTask->sShellType = Sched_CopyText(xvoTableGetText(tblBody, "shellType", 9));
	pTask->sCodeText = Sched_CopyText(xvoTableGetText(tblBody, "codeText", 8));
	pTask->sCustomText = Sched_CopyText(xvoTableGetText(tblBody, "customText", 10));
	pTask->sCronExpr = Sched_CopyText(xvoTableGetText(tblBody, "cronExpr", 8));
	pTask->sIntervalUnit = Sched_CopyText(xvoTableGetText(tblBody, "intervalUnit", 12));
	pTask->sOverlapPolicy = Sched_CopyText(xvoTableGetText(tblBody, "overlapPolicy", 13));
	pTask->sMisfirePolicy = Sched_CopyText(xvoTableGetText(tblBody, "misfirePolicy", 13));
	pTask->sWorkDir = Sched_CopyText(xvoTableGetText(tblBody, "workDir", 7));
	sOnceText = xvoTableGetText(tblBody, "onceAtText", 10);
	sStartText = xvoTableGetText(tblBody, "startAtText", 11);
	if ( pTask->onceAt <= 0 && sOnceText && sOnceText[0] ) {
		pTask->onceAt = xrtStrToTime(sOnceText, strlen(sOnceText));
	}
	if ( pTask->startAt <= 0 && sStartText && sStartText[0] ) {
		pTask->startAt = xrtStrToTime(sStartText, strlen(sStartText));
	}
}

static void Sched_NormalizePreviewTask(SchedTaskSnapshot* pTask)
{
	if ( pTask == NULL ) {
		return;
	}

	pTask->enabled = 1;
	if ( pTask->sScheduleType == NULL || pTask->sScheduleType[0] == '\0' ) {
		if ( pTask->sScheduleType ) xrtFree(pTask->sScheduleType);
		pTask->sScheduleType = xrtCopyStr("once", 0);
	}
	if ( pTask->sIntervalUnit == NULL || pTask->sIntervalUnit[0] == '\0' ) {
		if ( pTask->sIntervalUnit ) xrtFree(pTask->sIntervalUnit);
		pTask->sIntervalUnit = xrtCopyStr("minute", 0);
	}
}

static void Sched_AddPreviewItem(xvalue arrList, int64 iTime)
{
	xvalue tblItem;
	str sText;

	if ( arrList == NULL || iTime <= 0 ) {
		return;
	}

	tblItem = xvoCreateTable();
	xvoTableSetInt(tblItem, "time", 4, iTime);
	sText = Sched_TimeText(iTime);
	xvoTableSetText(tblItem, "text", 4, sText, 0, TRUE);
	xvoArrayAppendValue(arrList, tblItem, TRUE);
}

static xvalue Sched_PreviewTask(xvalue tblBody, int iLimit, str* psMessage)
{
	SchedTaskSnapshot tTask;
	SchedCronExpr tCron;
	xvalue tblRet = xvoCreateTable();
	xvalue arrData = xvoCreateArray();
	int64 iNow = xrtNow();
	int64 iNext = 0;
	int64 iStep;
	str sMessage = NULL;

	memset(&tTask, 0, sizeof(tTask));
	memset(&tCron, 0, sizeof(tCron));
	if ( iLimit <= 0 ) iLimit = 5;
	if ( iLimit > 10 ) iLimit = 10;

	if ( tblBody == NULL || xvoType(tblBody) != XVO_DT_TABLE ) {
		xvoTableSetBool(tblRet, "result", 6, FALSE);
		xvoTableSetText(tblRet, "message", 7, "Invalid preview request", 0, FALSE);
		xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
		return tblRet;
	}

	Sched_ReadTaskFieldFromBody(tblBody, &tTask);
	Sched_NormalizePreviewTask(&tTask);

	if ( Sched_TextEquals(tTask.sScheduleType, "once") ) {
		if ( tTask.onceAt <= 0 ) {
			sMessage = xrtCopyStr("请先设置单次执行时间", 0);
		} else if ( tTask.onceAt <= iNow ) {
			sMessage = xrtCopyStr("该时间已早于当前时间，保存后不会再自动执行", 0);
			Sched_AddPreviewItem(arrData, tTask.onceAt);
		} else {
			Sched_AddPreviewItem(arrData, tTask.onceAt);
		}
	} else if ( Sched_TextEquals(tTask.sScheduleType, "interval") ) {
		iStep = Sched_IntervalSeconds(tTask.intervalValue, tTask.sIntervalUnit);
		if ( iStep <= 0 ) {
			sMessage = xrtCopyStr("请先设置有效的间隔值", 0);
		} else {
			iNext = Sched_CalcNextTime(&tTask, iNow);
			for ( int i = 0; i < iLimit && iNext > 0; i++ ) {
				Sched_AddPreviewItem(arrData, iNext);
				iNext += iStep;
			}
		}
	} else if ( Sched_TextEquals(tTask.sScheduleType, "cron") ) {
		if ( tTask.sCronExpr == NULL || tTask.sCronExpr[0] == '\0' ) {
			sMessage = xrtCopyStr("请先输入 Cron 表达式", 0);
		} else if ( !Sched_ParseCronExpr(tTask.sCronExpr, &tCron) ) {
			sMessage = xrtCopyStr("Cron 表达式格式无效", 0);
		} else {
			int64 iCursor = iNow;
			for ( int i = 0; i < iLimit; i++ ) {
				iNext = Sched_CalcCronNextTime(tTask.sCronExpr, iCursor);
				if ( iNext <= 0 ) {
					break;
				}
				Sched_AddPreviewItem(arrData, iNext);
				iCursor = iNext;
			}
		}
	} else {
		sMessage = xrtCopyStr("不支持的调度类型", 0);
	}

	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetText(tblRet, "message", 7, Sched_StrOrEmpty(sMessage), 0, FALSE);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	if ( psMessage ) {
		*psMessage = sMessage ? xrtCopyStr(sMessage, 0) : NULL;
	}
	if ( sMessage ) {
		xrtFree(sMessage);
	}
	Sched_FreeTaskSnapshot(&tTask);
	return tblRet;
}

static bool Sched_CreateExampleTask(const char* sKind, str* psMessage, int64* pTaskId)
{
	xvalue tblBody;
	str sCode = NULL;
	str sName = NULL;
	bool bRet;

	if ( sKind == NULL || sKind[0] == '\0' ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Example kind is required", 0);
		return FALSE;
	}

	tblBody = xvoCreateTable();
	xvoTableSetInt(tblBody, "enabled", 7, 0);
	xvoTableSetText(tblBody, "scheduleType", 12, "interval", 0, FALSE);
	xvoTableSetInt(tblBody, "intervalValue", 13, 5);
	xvoTableSetText(tblBody, "intervalUnit", 12, "minute", 0, FALSE);
	xvoTableSetInt(tblBody, "timeoutSec", 10, 300);
	xvoTableSetText(tblBody, "overlapPolicy", 13, "skip", 0, FALSE);
	xvoTableSetText(tblBody, "misfirePolicy", 13, "skip", 0, FALSE);
	xvoTableSetText(tblBody, "workDir", 7, Sched_StrOrEmpty(AppPath), 0, FALSE);
	xvoTableSetText(tblBody, "customText", 10, "{\"message\":\"hello from custom params\",\"count\":1}", 0, FALSE);
	xvoTableSetInt(tblBody, "parallelLimit", 13, 0);
	xvoTableSetInt(tblBody, "retryCount", 10, 0);
	xvoTableSetInt(tblBody, "retryDelaySec", 13, 60);

	if ( Sched_TextEquals(sKind, "shell") ) {
		sName = xrtFormat("示例 Shell 任务 %lld", xrtNow());
		sCode = xrtCopyStr(
			"$now = Get-Date -Format \"yyyy-MM-dd HH:mm:ss\"\n"
			"Write-Output (\"[sched] shell example start: \" + $now)\n"
			"Write-Output \"hello from xadmin scheduler\"\n"
			"Write-Output (\"workdir=\" + (Get-Location).Path)\n",
			0
		);
		xvoTableSetText(tblBody, "execType", 8, "shell", 0, FALSE);
		xvoTableSetText(tblBody, "shellType", 9, "powershell", 0, FALSE);
	} else if ( Sched_TextEquals(sKind, "c") ) {
		sName = xrtFormat("示例 C 任务 %lld", xrtNow());
		sCode = xrtCopyStr(
			"int TaskProc(TaskInfo* pTaskInfo)\n"
			"{\n"
			"\tif ( pTaskInfo == NULL ) {\n"
			"\t\treturn 1;\n"
			"\t}\n"
			"\tsnprintf(pTaskInfo->output, sizeof(pTaskInfo->output), \"task_id=%lld\\ntrigger=%s\\ncustom=%s\",\n"
			"\t\t(long long)pTaskInfo->task_id,\n"
			"\t\tpTaskInfo->trigger_source ? pTaskInfo->trigger_source : \"unknown\",\n"
			"\t\tpTaskInfo->custom_json ? pTaskInfo->custom_json : \"{}\");\n"
			"\treturn 0;\n"
			"}\n",
			0
		);
		xvoTableSetText(tblBody, "execType", 8, "c", 0, FALSE);
		xvoTableSetText(tblBody, "shellType", 9, "", 0, FALSE);
	} else {
		xvoUnref(tblBody);
		if ( psMessage ) *psMessage = xrtCopyStr("Unsupported example kind", 0);
		return FALSE;
	}

	xvoTableSetText(tblBody, "name", 4, Sched_StrOrEmpty(sName), 0, FALSE);
	xvoTableSetText(tblBody, "codeText", 8, Sched_StrOrEmpty(sCode), 0, FALSE);
	bRet = Sched_SaveTaskRequest(tblBody, FALSE, psMessage, pTaskId);

	if ( sName ) xrtFree(sName);
	if ( sCode ) xrtFree(sCode);
	xvoUnref(tblBody);
	return bRet;
}

static bool Sched_CopyTask(int64 id, str* psMessage, int64* pTaskId)
{
	SchedTaskSnapshot tTask;
	xvalue tblBody;
	str sName = NULL;
	bool bRet;

	memset(&tTask, 0, sizeof(tTask));
	if ( id <= 0 ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Task id is required", 0);
		return FALSE;
	}
	if ( !Sched_LoadTaskSnapshotById(G_DB, id, &tTask) ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Task not found", 0);
		return FALSE;
	}

	tblBody = xvoCreateTable();
	xvoTableSetInt(tblBody, "enabled", 7, 0);
	xvoTableSetInt(tblBody, "onceAt", 6, tTask.onceAt);
	xvoTableSetInt(tblBody, "intervalValue", 13, tTask.intervalValue);
	xvoTableSetInt(tblBody, "startAt", 7, tTask.startAt);
	xvoTableSetInt(tblBody, "timeoutSec", 10, tTask.timeoutSec);
	xvoTableSetText(tblBody, "scheduleType", 12, Sched_StrOrEmpty(tTask.sScheduleType), 0, FALSE);
	xvoTableSetText(tblBody, "execType", 8, Sched_StrOrEmpty(tTask.sExecType), 0, FALSE);
	xvoTableSetText(tblBody, "shellType", 9, Sched_StrOrEmpty(tTask.sShellType), 0, FALSE);
	xvoTableSetText(tblBody, "codeText", 8, Sched_StrOrEmpty(tTask.sCodeText), 0, FALSE);
	xvoTableSetText(tblBody, "customText", 10, Sched_StrOrEmpty(tTask.sCustomText), 0, FALSE);
	xvoTableSetText(tblBody, "cronExpr", 8, Sched_StrOrEmpty(tTask.sCronExpr), 0, FALSE);
	xvoTableSetText(tblBody, "intervalUnit", 12, Sched_StrOrEmpty(tTask.sIntervalUnit), 0, FALSE);
	xvoTableSetText(tblBody, "overlapPolicy", 13, Sched_StrOrEmpty(tTask.sOverlapPolicy), 0, FALSE);
	xvoTableSetText(tblBody, "misfirePolicy", 13, Sched_StrOrEmpty(tTask.sMisfirePolicy), 0, FALSE);
	xvoTableSetText(tblBody, "workDir", 7, Sched_StrOrEmpty(tTask.sWorkDir), 0, FALSE);
	xvoTableSetInt(tblBody, "parallelLimit", 13, tTask.parallelLimit);
	xvoTableSetInt(tblBody, "retryCount", 10, tTask.retryCount);
	xvoTableSetInt(tblBody, "retryDelaySec", 13, tTask.retryDelaySec);

	sName = xrtFormat("%s - 副本", Sched_CStrOrEmpty((const char*)tTask.sName));
	xvoTableSetText(tblBody, "name", 4, Sched_StrOrEmpty(sName), 0, FALSE);

	bRet = Sched_SaveTaskRequest(tblBody, FALSE, psMessage, pTaskId);
	if ( sName ) xrtFree(sName);
	xvoUnref(tblBody);
	Sched_FreeTaskSnapshot(&tTask);
	return bRet;
}

static bool Sched_TaskRunning(sqlite3* pDB, int64 id)
{
	sqlite3_stmt* stmt = NULL;
	int bRunning = 0;

	if ( sqlite3_prepare_v3(pDB, "SELECT isRunning FROM sched_task WHERE id = ? AND isDelete = 0", -1, 0, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}
	sqlite3_bind_int64(stmt, 1, id);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		bRunning = sqlite3_column_int(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return bRunning ? TRUE : FALSE;
}

static bool Sched_SaveTaskRequest(xvalue tblBody, bool bUpdate, str* psMessage, int64* pTaskId)
{
	SchedTaskSnapshot tTask;
	sqlite3_stmt* stmt = NULL;
	int iRet;
	int64 iNow = xrtNow();

	if ( tblBody == NULL || xvoType(tblBody) != XVO_DT_TABLE ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Invalid request body", 0);
		return FALSE;
	}

	memset(&tTask, 0, sizeof(tTask));
	Sched_ReadTaskFieldFromBody(tblBody, &tTask);
	if ( bUpdate && tTask.id <= 0 ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Task id is required", 0);
		Sched_FreeTaskSnapshot(&tTask);
		return FALSE;
	}
	if ( bUpdate && Sched_TaskRunning(G_DB, tTask.id) ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Task is running", 0);
		Sched_FreeTaskSnapshot(&tTask);
		return FALSE;
	}
	if ( !Sched_TaskIsValidForSave(&tTask, psMessage) ) {
		Sched_FreeTaskSnapshot(&tTask);
		return FALSE;
	}

	tTask.nextRunAt = Sched_CalcNextTime(&tTask, iNow);
	if ( !tTask.enabled ) tTask.nextRunAt = 0;

	if ( !bUpdate ) {
		iRet = sqlite3_prepare_v3(G_DB, "INSERT INTO sched_task (name, enabled, scheduleType, execType, shellType, codeText, customText, cronExpr, onceAt, intervalValue, intervalUnit, startAt, nextRunAt, timeoutSec, overlapPolicy, misfirePolicy, workDir, parallelLimit, retryCount, retryDelaySec, retryState, createTime, updateTime, isDelete, isRunning, runningCount, pendingRun, lastStatus, lastMessage, lastExitCode, lastRunAt, lastFinishAt) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?, ?, 0, 0, 0, 0, '', '', 0, 0, 0)", -1, 0, &stmt, NULL);
		if ( iRet == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, tTask.sName, -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 2, tTask.enabled);
			sqlite3_bind_text(stmt, 3, tTask.sScheduleType, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 4, tTask.sExecType, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 5, tTask.sShellType, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 6, tTask.sCodeText, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 7, tTask.sCustomText, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 8, tTask.sCronExpr, -1, SQLITE_STATIC);
			sqlite3_bind_int64(stmt, 9, tTask.onceAt);
			sqlite3_bind_int64(stmt, 10, tTask.intervalValue);
			sqlite3_bind_text(stmt, 11, tTask.sIntervalUnit, -1, SQLITE_STATIC);
			sqlite3_bind_int64(stmt, 12, tTask.startAt);
			sqlite3_bind_int64(stmt, 13, tTask.nextRunAt);
			sqlite3_bind_int(stmt, 14, tTask.timeoutSec);
			sqlite3_bind_text(stmt, 15, tTask.sOverlapPolicy, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 16, tTask.sMisfirePolicy, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 17, tTask.sWorkDir, -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 18, tTask.parallelLimit);
			sqlite3_bind_int(stmt, 19, tTask.retryCount);
			sqlite3_bind_int(stmt, 20, tTask.retryDelaySec);
			sqlite3_bind_int64(stmt, 21, iNow);
			sqlite3_bind_int64(stmt, 22, iNow);
		}
	} else {
		iRet = sqlite3_prepare_v3(G_DB, "UPDATE sched_task SET name = ?, enabled = ?, scheduleType = ?, execType = ?, shellType = ?, codeText = ?, customText = ?, cronExpr = ?, onceAt = ?, intervalValue = ?, intervalUnit = ?, startAt = ?, nextRunAt = ?, timeoutSec = ?, overlapPolicy = ?, misfirePolicy = ?, workDir = ?, parallelLimit = ?, retryCount = ?, retryDelaySec = ?, retryState = 0, updateTime = ?, isRunning = 0, runningCount = 0, pendingRun = 0 WHERE id = ? AND isDelete = 0", -1, 0, &stmt, NULL);
		if ( iRet == SQLITE_OK ) {
			sqlite3_bind_text(stmt, 1, tTask.sName, -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 2, tTask.enabled);
			sqlite3_bind_text(stmt, 3, tTask.sScheduleType, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 4, tTask.sExecType, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 5, tTask.sShellType, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 6, tTask.sCodeText, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 7, tTask.sCustomText, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 8, tTask.sCronExpr, -1, SQLITE_STATIC);
			sqlite3_bind_int64(stmt, 9, tTask.onceAt);
			sqlite3_bind_int64(stmt, 10, tTask.intervalValue);
			sqlite3_bind_text(stmt, 11, tTask.sIntervalUnit, -1, SQLITE_STATIC);
			sqlite3_bind_int64(stmt, 12, tTask.startAt);
			sqlite3_bind_int64(stmt, 13, tTask.nextRunAt);
			sqlite3_bind_int(stmt, 14, tTask.timeoutSec);
			sqlite3_bind_text(stmt, 15, tTask.sOverlapPolicy, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 16, tTask.sMisfirePolicy, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 17, tTask.sWorkDir, -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 18, tTask.parallelLimit);
			sqlite3_bind_int(stmt, 19, tTask.retryCount);
			sqlite3_bind_int(stmt, 20, tTask.retryDelaySec);
			sqlite3_bind_int64(stmt, 21, iNow);
			sqlite3_bind_int64(stmt, 22, tTask.id);
		}
	}

	if ( iRet != SQLITE_OK || sqlite3_step(stmt) != SQLITE_DONE ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Failed to save task", 0);
		if ( stmt ) sqlite3_finalize(stmt);
		Sched_FreeTaskSnapshot(&tTask);
		return FALSE;
	}

	if ( !bUpdate ) {
		tTask.id = sqlite3_last_insert_rowid(G_DB);
	}
	if ( pTaskId ) *pTaskId = tTask.id;
	if ( psMessage ) *psMessage = xrtCopyStr(bUpdate ? "Task updated" : "Task created", 0);
	sqlite3_finalize(stmt);
	Sched_FreeTaskSnapshot(&tTask);
	Sched_NotifyChanged();
	return TRUE;
}

xvalue Sched_ListTasks(int64 iPage, int64 iLimit, const char* sSearch)
{
	sqlite3_stmt* stmtCount = NULL;
	sqlite3_stmt* stmtList = NULL;
	xvalue tblRet = xvoCreateTable();
	xvalue arrData = xvoCreateArray();
	int64 iCount = 0;
	int64 iOffset;
	bool bSearch = (sSearch && sSearch[0] != '\0');

	if ( iPage <= 0 ) iPage = 1;
	if ( iLimit <= 0 ) iLimit = 20;
	iOffset = (iPage - 1) * iLimit;

	if ( bSearch ) {
		sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM sched_task WHERE isDelete = 0 AND name LIKE ?", -1, 0, &stmtCount, NULL);
		sqlite3_bind_text(stmtCount, 1, sSearch, -1, SQLITE_STATIC);
	} else {
		sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM sched_task WHERE isDelete = 0", -1, 0, &stmtCount, NULL);
	}
	if ( stmtCount && sqlite3_step(stmtCount) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmtCount, 0);
	}
	if ( stmtCount ) sqlite3_finalize(stmtCount);

	if ( bSearch ) {
		sqlite3_prepare_v3(G_DB, "SELECT id FROM sched_task WHERE isDelete = 0 AND name LIKE ? ORDER BY id DESC LIMIT ? OFFSET ?", -1, 0, &stmtList, NULL);
		sqlite3_bind_text(stmtList, 1, sSearch, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmtList, 2, iLimit);
		sqlite3_bind_int64(stmtList, 3, iOffset);
	} else {
		sqlite3_prepare_v3(G_DB, "SELECT id FROM sched_task WHERE isDelete = 0 ORDER BY id DESC LIMIT ? OFFSET ?", -1, 0, &stmtList, NULL);
		sqlite3_bind_int64(stmtList, 1, iLimit);
		sqlite3_bind_int64(stmtList, 2, iOffset);
	}

	while ( stmtList && sqlite3_step(stmtList) == SQLITE_ROW ) {
		SchedTaskSnapshot tTask;
		memset(&tTask, 0, sizeof(tTask));
		if ( Sched_LoadTaskSnapshotById(G_DB, sqlite3_column_int64(stmtList, 0), &tTask) ) {
			xvalue tblRow = Sched_TaskSnapshotToValue(&tTask);
			xvoArrayAppendValue(arrData, tblRow, TRUE);
			Sched_FreeTaskSnapshot(&tTask);
		}
	}
	if ( stmtList ) sqlite3_finalize(stmtList);

	xvoTableSetInt(tblRet, "code", 4, 0);
	xvoTableSetText(tblRet, "msg", 3, "", 0, FALSE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	return tblRet;
}

xvalue Sched_GetTask(int64 id)
{
	SchedTaskSnapshot tTask;
	memset(&tTask, 0, sizeof(tTask));
	if ( !Sched_LoadTaskSnapshotById(G_DB, id, &tTask) ) {
		return NULL;
	}
	{
		xvalue tblRet = Sched_TaskSnapshotToValue(&tTask);
		Sched_FreeTaskSnapshot(&tTask);
		return tblRet;
	}
}

static xvalue Sched_BuildImportTaskBody(xvalue tblSource)
{
	xvalue tblTask = xvoCreateTable();

	if ( tblSource == NULL || xvoType(tblSource) != XVO_DT_TABLE ) {
		return tblTask;
	}

	xvoTableSetInt(tblTask, "enabled", 7, xvoTableGetInt(tblSource, "enabled", 7));
	xvoTableSetInt(tblTask, "onceAt", 6, xvoTableGetInt(tblSource, "onceAt", 6));
	xvoTableSetInt(tblTask, "intervalValue", 13, xvoTableGetInt(tblSource, "intervalValue", 13));
	xvoTableSetInt(tblTask, "startAt", 7, xvoTableGetInt(tblSource, "startAt", 7));
	xvoTableSetInt(tblTask, "timeoutSec", 10, xvoTableGetInt(tblSource, "timeoutSec", 10));
	xvoTableSetInt(tblTask, "parallelLimit", 13, xvoTableGetInt(tblSource, "parallelLimit", 13));
	xvoTableSetInt(tblTask, "retryCount", 10, xvoTableGetInt(tblSource, "retryCount", 10));
	xvoTableSetInt(tblTask, "retryDelaySec", 13, xvoTableGetInt(tblSource, "retryDelaySec", 13));
	xvoTableSetText(tblTask, "name", 4, Sched_XvoTextOrEmpty(tblSource, "name", 4), 0, FALSE);
	xvoTableSetText(tblTask, "scheduleType", 12, Sched_XvoTextOrEmpty(tblSource, "scheduleType", 12), 0, FALSE);
	xvoTableSetText(tblTask, "execType", 8, Sched_XvoTextOrEmpty(tblSource, "execType", 8), 0, FALSE);
	xvoTableSetText(tblTask, "shellType", 9, Sched_XvoTextOrEmpty(tblSource, "shellType", 9), 0, FALSE);
	xvoTableSetText(tblTask, "codeText", 8, Sched_XvoTextOrEmpty(tblSource, "codeText", 8), 0, FALSE);
	xvoTableSetText(tblTask, "customText", 10, Sched_XvoTextOrEmpty(tblSource, "customText", 10), 0, FALSE);
	xvoTableSetText(tblTask, "cronExpr", 8, Sched_XvoTextOrEmpty(tblSource, "cronExpr", 8), 0, FALSE);
	xvoTableSetText(tblTask, "intervalUnit", 12, Sched_XvoTextOrEmpty(tblSource, "intervalUnit", 12), 0, FALSE);
	xvoTableSetText(tblTask, "overlapPolicy", 13, Sched_XvoTextOrEmpty(tblSource, "overlapPolicy", 13), 0, FALSE);
	xvoTableSetText(tblTask, "misfirePolicy", 13, Sched_XvoTextOrEmpty(tblSource, "misfirePolicy", 13), 0, FALSE);
	xvoTableSetText(tblTask, "workDir", 7, Sched_XvoTextOrEmpty(tblSource, "workDir", 7), 0, FALSE);
	xvoTableSetText(tblTask, "onceAtText", 10, Sched_XvoTextOrEmpty(tblSource, "onceAtText", 10), 0, FALSE);
	xvoTableSetText(tblTask, "startAtText", 11, Sched_XvoTextOrEmpty(tblSource, "startAtText", 11), 0, FALSE);
	return tblTask;
}

static int64 Sched_FindTaskIdByName(const char* sName)
{
	sqlite3_stmt* stmt = NULL;
	int64 iTaskId = 0;

	if ( G_DB == NULL || sName == NULL || sName[0] == '\0' ) {
		return 0;
	}
	if ( sqlite3_prepare_v3(G_DB, "SELECT id FROM sched_task WHERE isDelete = 0 AND name = ? ORDER BY id DESC LIMIT 1", -1, 0, &stmt, NULL) != SQLITE_OK ) {
		return 0;
	}
	sqlite3_bind_text(stmt, 1, sName, -1, SQLITE_TRANSIENT);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iTaskId = sqlite3_column_int64(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return iTaskId;
}

static str Sched_BuildImportedCopyName(const char* sName)
{
	str sCandidate;
	int iIndex = 2;

	if ( sName == NULL || sName[0] == '\0' ) {
		sName = "Imported Task";
	}

	sCandidate = xrtFormat("%s - 导入副本", sName);
	while ( Sched_FindTaskIdByName(sCandidate) > 0 ) {
		str sNext = xrtFormat("%s - 导入副本 %d", sName, iIndex++);
		xrtFree(sCandidate);
		sCandidate = sNext;
	}
	return sCandidate;
}

xvalue Sched_ExportTasks(int64 iTaskId, const char* sSearch)
{
	sqlite3_stmt* stmtList = NULL;
	xvalue tblRet = xvoCreateTable();
	xvalue arrData = xvoCreateArray();
	bool bSearch = (sSearch && sSearch[0] != '\0');

	if ( iTaskId > 0 ) {
		SchedTaskSnapshot tTask;
		memset(&tTask, 0, sizeof(tTask));
		if ( Sched_LoadTaskSnapshotById(G_DB, iTaskId, &tTask) ) {
			xvalue tblRow = Sched_TaskSnapshotToValue(&tTask);
			xvoArrayAppendValue(arrData, tblRow, TRUE);
			Sched_FreeTaskSnapshot(&tTask);
		}
	} else {
		if ( bSearch ) {
			sqlite3_prepare_v3(G_DB, "SELECT id FROM sched_task WHERE isDelete = 0 AND name LIKE ? ORDER BY id DESC", -1, 0, &stmtList, NULL);
			if ( stmtList ) sqlite3_bind_text(stmtList, 1, sSearch, -1, SQLITE_TRANSIENT);
		} else {
			sqlite3_prepare_v3(G_DB, "SELECT id FROM sched_task WHERE isDelete = 0 ORDER BY id DESC", -1, 0, &stmtList, NULL);
		}

		while ( stmtList && sqlite3_step(stmtList) == SQLITE_ROW ) {
			SchedTaskSnapshot tTask;
			memset(&tTask, 0, sizeof(tTask));
			if ( Sched_LoadTaskSnapshotById(G_DB, sqlite3_column_int64(stmtList, 0), &tTask) ) {
				xvalue tblRow = Sched_TaskSnapshotToValue(&tTask);
				xvoArrayAppendValue(arrData, tblRow, TRUE);
				Sched_FreeTaskSnapshot(&tTask);
			}
		}
		if ( stmtList ) sqlite3_finalize(stmtList);
	}

	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetText(tblRet, "message", 7, "Task export ready", 0, FALSE);
	xvoTableSetInt(tblRet, "count", 5, xvoArrayItemCount(arrData));
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	return tblRet;
}

xvalue Sched_ExportTasksSelected(xvalue arrIDs)
{
	xvalue tblRet = xvoCreateTable();
	xvalue arrData = xvoCreateArray();
	uint32 i;

	if ( arrIDs == NULL || xvoType(arrIDs) != XVO_DT_ARRAY || xvoArrayItemCount(arrIDs) <= 0 ) {
		xvoTableSetBool(tblRet, "result", 6, FALSE);
		xvoTableSetText(tblRet, "message", 7, "No task ids provided", 0, FALSE);
		xvoTableSetInt(tblRet, "count", 5, 0);
		xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
		return tblRet;
	}

	for ( i = 0; i < xvoArrayItemCount(arrIDs); i++ ) {
		int64 iTaskId = xvoArrayGetInt(arrIDs, i);
		SchedTaskSnapshot tTask;
		memset(&tTask, 0, sizeof(tTask));
		if ( iTaskId <= 0 ) {
			continue;
		}
		if ( Sched_LoadTaskSnapshotById(G_DB, iTaskId, &tTask) ) {
			xvalue tblRow = Sched_TaskSnapshotToValue(&tTask);
			xvoArrayAppendValue(arrData, tblRow, TRUE);
			Sched_FreeTaskSnapshot(&tTask);
		}
	}

	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetText(tblRet, "message", 7, "Selected task export ready", 0, FALSE);
	xvoTableSetInt(tblRet, "count", 5, xvoArrayItemCount(arrData));
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	return tblRet;
}

bool Sched_RunNowBatch(xvalue arrIDs, int64* pQueued, int64* pSkipped, int64* pFailed, str* psMessage)
{
	uint32 i;
	int64 iQueued = 0;
	int64 iSkipped = 0;
	int64 iFailed = 0;

	if ( pQueued ) *pQueued = 0;
	if ( pSkipped ) *pSkipped = 0;
	if ( pFailed ) *pFailed = 0;
	if ( arrIDs == NULL || xvoType(arrIDs) != XVO_DT_ARRAY || xvoArrayItemCount(arrIDs) <= 0 ) {
		if ( psMessage ) *psMessage = xrtCopyStr("No task ids provided", 0);
		return FALSE;
	}

	for ( i = 0; i < xvoArrayItemCount(arrIDs); i++ ) {
		int64 id = xvoArrayGetInt(arrIDs, i);
		str sItemMessage = NULL;
		bool bRet;

		if ( id <= 0 ) {
			iSkipped++;
			continue;
		}

		bRet = Sched_RunNow(id, &sItemMessage);
		if ( bRet ) {
			iQueued++;
		} else if (
			Sched_TextEquals(sItemMessage, "Task not found") ||
			Sched_TextEquals(sItemMessage, "Task is already running") ||
			Sched_TextEquals(sItemMessage, "Parallel limit reached")
		) {
			iSkipped++;
		} else {
			iFailed++;
		}
		if ( sItemMessage ) xrtFree(sItemMessage);
	}

	if ( pQueued ) *pQueued = iQueued;
	if ( pSkipped ) *pSkipped = iSkipped;
	if ( pFailed ) *pFailed = iFailed;
	if ( psMessage ) {
		*psMessage = xrtFormat("Queued %lld task(s), skipped %lld, failed %lld", iQueued, iSkipped, iFailed);
	}
	return (iQueued > 0 || iSkipped > 0) && iFailed == 0;
}

static int64 Sched_StartOfToday(void)
{
	int64 iYear;
	int iMonth;
	int iDay;
	int iHour;
	int iMinute;
	int iSecond;
	int iWeekday;
	int iDayOfYear;

	xrtDecodeSerial(xrtNow(), &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, &iWeekday, &iDayOfYear);
	return xrtDateTimeSerial(iYear, iMonth, iDay, 0, 0, 0);
}

xvalue Sched_DashboardData(void)
{
	xvalue tblRet = xvoCreateTable();
	xvalue tblData = xvoCreateTable();
	xvalue tblStats = xvoCreateTable();
	xvalue tblHealth = xvoCreateTable();
	xvalue arrUpcoming = xvoCreateArray();
	xvalue arrRecent = xvoCreateArray();
	sqlite3_stmt* stmt = NULL;
	int64 iTodayStart = Sched_StartOfToday();
	int64 iLast24h = xrtNow() - 86400;

	if ( sqlite3_prepare_v3(G_DB, "SELECT COUNT(*), SUM(CASE WHEN enabled = 1 THEN 1 ELSE 0 END), SUM(CASE WHEN enabled = 0 THEN 1 ELSE 0 END), SUM(runningCount), SUM(pendingRun), SUM(CASE WHEN execType = 'shell' THEN 1 ELSE 0 END), SUM(CASE WHEN execType = 'c' THEN 1 ELSE 0 END), SUM(CASE WHEN overlapPolicy = 'parallel' THEN 1 ELSE 0 END), SUM(CASE WHEN overlapPolicy = 'queue_one' THEN 1 ELSE 0 END), SUM(CASE WHEN retryCount > 0 THEN 1 ELSE 0 END) FROM sched_task WHERE isDelete = 0", -1, 0, &stmt, NULL) == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvoTableSetInt(tblStats, "totalTasks", 10, sqlite3_column_int64(stmt, 0));
			xvoTableSetInt(tblStats, "enabledTasks", 12, sqlite3_column_int64(stmt, 1));
			xvoTableSetInt(tblStats, "disabledTasks", 13, sqlite3_column_int64(stmt, 2));
			xvoTableSetInt(tblStats, "runningInstances", 16, sqlite3_column_int64(stmt, 3));
			xvoTableSetInt(tblStats, "pendingRuns", 11, sqlite3_column_int64(stmt, 4));
			xvoTableSetInt(tblStats, "shellTasks", 10, sqlite3_column_int64(stmt, 5));
			xvoTableSetInt(tblStats, "cTasks", 6, sqlite3_column_int64(stmt, 6));
			xvoTableSetInt(tblStats, "parallelTasks", 13, sqlite3_column_int64(stmt, 7));
			xvoTableSetInt(tblStats, "queueTasks", 10, sqlite3_column_int64(stmt, 8));
			xvoTableSetInt(tblStats, "retryTasks", 10, sqlite3_column_int64(stmt, 9));
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
	}

	if ( sqlite3_prepare_v3(G_DB, "SELECT COUNT(*), SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END), SUM(CASE WHEN status = 'failed' THEN 1 ELSE 0 END), SUM(CASE WHEN status = 'timeout' THEN 1 ELSE 0 END) FROM sched_run_log WHERE startTime >= ?", -1, 0, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, iTodayStart);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvoTableSetInt(tblStats, "todayRuns", 9, sqlite3_column_int64(stmt, 0));
			xvoTableSetInt(tblStats, "todaySuccess", 12, sqlite3_column_int64(stmt, 1));
			xvoTableSetInt(tblStats, "todayFailed", 11, sqlite3_column_int64(stmt, 2));
			xvoTableSetInt(tblStats, "todayTimeout", 12, sqlite3_column_int64(stmt, 3));
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
	}

	if ( sqlite3_prepare_v3(G_DB, "SELECT COUNT(*), SUM(CASE WHEN status = 'success' THEN 1 ELSE 0 END), SUM(CASE WHEN status = 'failed' THEN 1 ELSE 0 END), SUM(CASE WHEN status = 'timeout' THEN 1 ELSE 0 END) FROM sched_run_log WHERE startTime >= ?", -1, 0, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, iLast24h);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvoTableSetInt(tblHealth, "runCount24h", 11, sqlite3_column_int64(stmt, 0));
			xvoTableSetInt(tblHealth, "success24h", 10, sqlite3_column_int64(stmt, 1));
			xvoTableSetInt(tblHealth, "failed24h", 9, sqlite3_column_int64(stmt, 2));
			xvoTableSetInt(tblHealth, "timeout24h", 10, sqlite3_column_int64(stmt, 3));
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
	}

	if ( sqlite3_prepare_v3(G_DB, "SELECT id FROM sched_task WHERE isDelete = 0 AND enabled = 1 AND nextRunAt > 0 ORDER BY nextRunAt ASC, id ASC LIMIT 6", -1, 0, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			SchedTaskSnapshot tTask;
			memset(&tTask, 0, sizeof(tTask));
			if ( Sched_LoadTaskSnapshotById(G_DB, sqlite3_column_int64(stmt, 0), &tTask) ) {
				xvalue tblRow = Sched_TaskSnapshotToValue(&tTask);
				xvoArrayAppendValue(arrUpcoming, tblRow, TRUE);
				Sched_FreeTaskSnapshot(&tTask);
			}
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
	}

	if ( sqlite3_prepare_v3(G_DB, "SELECT id, taskId, taskName, triggerSource, startTime, finishTime, status, exitCode, message FROM sched_run_log ORDER BY id DESC LIMIT 8", -1, 0, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblRow = xvoCreateTable();
			str sStartText;
			str sFinishText;
			xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
			xvoTableSetInt(tblRow, "taskId", 6, sqlite3_column_int64(stmt, 1));
			xvoTableSetText(tblRow, "taskName", 8, Sched_SQLiteTextOrEmpty(stmt, 2), 0, FALSE);
			xvoTableSetText(tblRow, "triggerSource", 13, Sched_SQLiteTextOrEmpty(stmt, 3), 0, FALSE);
			xvoTableSetInt(tblRow, "startTime", 9, sqlite3_column_int64(stmt, 4));
			xvoTableSetInt(tblRow, "finishTime", 10, sqlite3_column_int64(stmt, 5));
			xvoTableSetText(tblRow, "status", 6, Sched_SQLiteTextOrEmpty(stmt, 6), 0, FALSE);
			xvoTableSetInt(tblRow, "exitCode", 8, sqlite3_column_int(stmt, 7));
			xvoTableSetText(tblRow, "message", 7, Sched_SQLiteTextOrEmpty(stmt, 8), 0, FALSE);
			sStartText = Sched_TimeText(sqlite3_column_int64(stmt, 4));
			sFinishText = Sched_TimeText(sqlite3_column_int64(stmt, 5));
			xvoTableSetText(tblRow, "startText", 9, Sched_StrOrEmpty(sStartText), 0, TRUE);
			xvoTableSetText(tblRow, "finishText", 10, Sched_StrOrEmpty(sFinishText), 0, TRUE);
			xvoArrayAppendValue(arrRecent, tblRow, TRUE);
		}
		sqlite3_finalize(stmt);
		stmt = NULL;
	}

	xvoTableSetValue(tblData, "stats", 5, tblStats, TRUE);
	xvoTableSetValue(tblData, "health", 6, tblHealth, TRUE);
	xvoTableSetValue(tblData, "upcoming", 8, arrUpcoming, TRUE);
	xvoTableSetValue(tblData, "recent", 6, arrRecent, TRUE);

	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetText(tblRet, "message", 7, "Dashboard ready", 0, FALSE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	return tblRet;
}

bool Sched_ImportTasks(xvalue tblBody, int64* pImported, int64* pSkipped, int64* pFailed, str* psMessage)
{
	xvalue arrTasks = NULL;
	const char* sConflictPolicy = "skip";
	uint32 i;
	int64 iImported = 0;
	int64 iSkipped = 0;
	int64 iFailed = 0;

	if ( pImported ) *pImported = 0;
	if ( pSkipped ) *pSkipped = 0;
	if ( pFailed ) *pFailed = 0;
	if ( tblBody == NULL ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Invalid request body", 0);
		return FALSE;
	}

	if ( xvoType(tblBody) == XVO_DT_ARRAY ) {
		arrTasks = tblBody;
	} else if ( xvoType(tblBody) == XVO_DT_TABLE ) {
		arrTasks = xvoTableGetValue(tblBody, "tasks", 5);
		{
			const char* sPolicy = (const char*)xvoTableGetText(tblBody, "conflictPolicy", 14);
			if ( sPolicy && sPolicy[0] != '\0' ) {
				sConflictPolicy = sPolicy;
			}
		}
		if ( arrTasks == NULL || xvoType(arrTasks) != XVO_DT_ARRAY ) {
			arrTasks = tblBody;
		}
	} else {
		if ( psMessage ) *psMessage = xrtCopyStr("Unsupported import data", 0);
		return FALSE;
	}

	if ( xvoType(arrTasks) == XVO_DT_TABLE ) {
		xvalue tblTask = Sched_BuildImportTaskBody(arrTasks);
		int64 iTaskId = 0;
		bool bUpdate = FALSE;
		str sTaskName = Sched_CopyText(xvoTableGetText(tblTask, "name", 4));
		int64 iExists = Sched_FindTaskIdByName(sTaskName);
		if ( iExists > 0 ) {
			if ( Sched_TextEquals(sConflictPolicy, "skip") ) {
				xvoUnref(tblTask);
				if ( sTaskName ) xrtFree(sTaskName);
				if ( pSkipped ) *pSkipped = 1;
				if ( psMessage ) {
					if ( *psMessage ) xrtFree(*psMessage);
					*psMessage = xrtCopyStr("Imported 0 task(s), skipped 1 task(s), failed 0 task(s)", 0);
				}
				return TRUE;
			}
			if ( Sched_TextEquals(sConflictPolicy, "rename") ) {
				str sNewName = Sched_BuildImportedCopyName(sTaskName);
				xvoTableSetText(tblTask, "name", 4, sNewName, 0, TRUE);
			} else if ( Sched_TextEquals(sConflictPolicy, "overwrite") ) {
				xvoTableSetInt(tblTask, "id", 2, iExists);
				bUpdate = TRUE;
			}
		}
		bool bRet = Sched_SaveTaskRequest(tblTask, bUpdate, psMessage, &iTaskId);
		xvoUnref(tblTask);
		if ( sTaskName ) xrtFree(sTaskName);
		if ( !bRet ) {
			if ( pFailed ) *pFailed = 1;
			return FALSE;
		}
		if ( pImported ) *pImported = 1;
		if ( psMessage ) {
			if ( *psMessage ) xrtFree(*psMessage);
			*psMessage = xrtCopyStr("Imported 1 task", 0);
		}
		return TRUE;
	}

	if ( arrTasks == NULL || xvoType(arrTasks) != XVO_DT_ARRAY || xvoArrayItemCount(arrTasks) <= 0 ) {
		if ( psMessage ) *psMessage = xrtCopyStr("No task data to import", 0);
		return FALSE;
	}

	for ( i = 0; i < xvoArrayItemCount(arrTasks); i++ ) {
		xvalue tblItem = xvoArrayGetValue(arrTasks, i);
		xvalue tblTask;
		int64 iTaskId = 0;
		int64 iExists = 0;
		bool bUpdate = FALSE;
		bool bRet;
		str sTaskName = NULL;

		if ( tblItem == NULL || xvoType(tblItem) != XVO_DT_TABLE ) {
			iFailed++;
			continue;
		}

		tblTask = Sched_BuildImportTaskBody(tblItem);
		sTaskName = Sched_CopyText(xvoTableGetText(tblTask, "name", 4));
		iExists = Sched_FindTaskIdByName(sTaskName);
		if ( iExists > 0 ) {
			if ( Sched_TextEquals(sConflictPolicy, "skip") ) {
				iSkipped++;
				xvoUnref(tblTask);
				if ( sTaskName ) xrtFree(sTaskName);
				continue;
			}
			if ( Sched_TextEquals(sConflictPolicy, "rename") ) {
				str sNewName = Sched_BuildImportedCopyName(sTaskName);
				xvoTableSetText(tblTask, "name", 4, sNewName, 0, TRUE);
			} else if ( Sched_TextEquals(sConflictPolicy, "overwrite") ) {
				xvoTableSetInt(tblTask, "id", 2, iExists);
				bUpdate = TRUE;
			}
		}
		bRet = Sched_SaveTaskRequest(tblTask, bUpdate, psMessage, &iTaskId);
		xvoUnref(tblTask);
		if ( sTaskName ) xrtFree(sTaskName);
		if ( !bRet ) {
			iFailed++;
			if ( psMessage && *psMessage ) {
				xrtFree(*psMessage);
				*psMessage = NULL;
			}
			continue;
		}
		iImported++;
	}

	if ( pImported ) *pImported = iImported;
	if ( pSkipped ) *pSkipped = iSkipped;
	if ( pFailed ) *pFailed = iFailed;
	if ( psMessage ) *psMessage = xrtFormat("Imported %lld task(s), skipped %lld task(s), failed %lld task(s)", iImported, iSkipped, iFailed);
	return (iImported > 0 || iSkipped > 0) && iFailed == 0;
}

xvalue Sched_ListLogs(int64 iTaskId, int64 iPage, int64 iLimit, const char* sStatus, const char* sKeyword, int64 iStartFrom, int64 iStartTo)
{
	sqlite3_stmt* stmtCount = NULL;
	sqlite3_stmt* stmtList = NULL;
	xvalue tblRet = xvoCreateTable();
	xvalue arrData = xvoCreateArray();
	int64 iCount = 0;
	int64 iOffset;
	char sWhere[512] = {0};
	char sCountSQL[768] = {0};
	char sListSQL[1024] = {0};
	char sKeywordLike[320] = {0};
	int iBindIndex;
	bool bHasWhere = FALSE;

	if ( iPage <= 0 ) iPage = 1;
	if ( iLimit <= 0 ) iLimit = 20;
	iOffset = (iPage - 1) * iLimit;

	if ( iTaskId > 0 ) {
		strcat(sWhere, bHasWhere ? " AND" : " WHERE");
		strcat(sWhere, " taskId = ?");
		bHasWhere = TRUE;
	}
	if ( sStatus && sStatus[0] != '\0' ) {
		strcat(sWhere, bHasWhere ? " AND" : " WHERE");
		strcat(sWhere, " status = ?");
		bHasWhere = TRUE;
	}
	if ( sKeyword && sKeyword[0] != '\0' ) {
		strcat(sWhere, bHasWhere ? " AND" : " WHERE");
		strcat(sWhere, " (taskName LIKE ? OR message LIKE ? OR stdoutText LIKE ? OR stderrText LIKE ?)");
		bHasWhere = TRUE;
		snprintf(sKeywordLike, sizeof(sKeywordLike), "%%%s%%", sKeyword);
	}
	if ( iStartFrom > 0 ) {
		strcat(sWhere, bHasWhere ? " AND" : " WHERE");
		strcat(sWhere, " startTime >= ?");
		bHasWhere = TRUE;
	}
	if ( iStartTo > 0 ) {
		strcat(sWhere, bHasWhere ? " AND" : " WHERE");
		strcat(sWhere, " startTime <= ?");
		bHasWhere = TRUE;
	}

	snprintf(sCountSQL, sizeof(sCountSQL), "SELECT COUNT(*) FROM sched_run_log%s", sWhere);
	snprintf(sListSQL, sizeof(sListSQL), "SELECT id, taskId, taskName, triggerSource, startTime, finishTime, durationMs, status, exitCode, stdoutText, stderrText, message FROM sched_run_log%s ORDER BY id DESC LIMIT ? OFFSET ?", sWhere);

	sqlite3_prepare_v3(G_DB, sCountSQL, -1, 0, &stmtCount, NULL);
	iBindIndex = 1;
	if ( stmtCount ) {
		if ( iTaskId > 0 ) sqlite3_bind_int64(stmtCount, iBindIndex++, iTaskId);
		if ( sStatus && sStatus[0] != '\0' ) sqlite3_bind_text(stmtCount, iBindIndex++, sStatus, -1, SQLITE_TRANSIENT);
		if ( sKeywordLike[0] != '\0' ) {
			sqlite3_bind_text(stmtCount, iBindIndex++, sKeywordLike, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmtCount, iBindIndex++, sKeywordLike, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmtCount, iBindIndex++, sKeywordLike, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmtCount, iBindIndex++, sKeywordLike, -1, SQLITE_TRANSIENT);
		}
		if ( iStartFrom > 0 ) sqlite3_bind_int64(stmtCount, iBindIndex++, iStartFrom);
		if ( iStartTo > 0 ) sqlite3_bind_int64(stmtCount, iBindIndex++, iStartTo);
	}
	if ( stmtCount && sqlite3_step(stmtCount) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmtCount, 0);
	}
	if ( stmtCount ) sqlite3_finalize(stmtCount);

	sqlite3_prepare_v3(G_DB, sListSQL, -1, 0, &stmtList, NULL);
	iBindIndex = 1;
	if ( stmtList ) {
		if ( iTaskId > 0 ) sqlite3_bind_int64(stmtList, iBindIndex++, iTaskId);
		if ( sStatus && sStatus[0] != '\0' ) sqlite3_bind_text(stmtList, iBindIndex++, sStatus, -1, SQLITE_TRANSIENT);
		if ( sKeywordLike[0] != '\0' ) {
			sqlite3_bind_text(stmtList, iBindIndex++, sKeywordLike, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmtList, iBindIndex++, sKeywordLike, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmtList, iBindIndex++, sKeywordLike, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmtList, iBindIndex++, sKeywordLike, -1, SQLITE_TRANSIENT);
		}
		if ( iStartFrom > 0 ) sqlite3_bind_int64(stmtList, iBindIndex++, iStartFrom);
		if ( iStartTo > 0 ) sqlite3_bind_int64(stmtList, iBindIndex++, iStartTo);
		sqlite3_bind_int64(stmtList, iBindIndex++, iLimit);
		sqlite3_bind_int64(stmtList, iBindIndex++, iOffset);
	}

	while ( stmtList && sqlite3_step(stmtList) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		str sStart = Sched_TimeText(sqlite3_column_int64(stmtList, 4));
		str sFinish = Sched_TimeText(sqlite3_column_int64(stmtList, 5));
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmtList, 0));
		xvoTableSetInt(tblRow, "taskId", 6, sqlite3_column_int64(stmtList, 1));
		xvoTableSetText(tblRow, "taskName", 8, Sched_SQLiteTextOrEmpty(stmtList, 2), 0, FALSE);
		xvoTableSetText(tblRow, "triggerSource", 13, Sched_SQLiteTextOrEmpty(stmtList, 3), 0, FALSE);
		xvoTableSetInt(tblRow, "startTime", 9, sqlite3_column_int64(stmtList, 4));
		xvoTableSetText(tblRow, "startText", 9, sStart, 0, TRUE);
		xvoTableSetInt(tblRow, "finishTime", 10, sqlite3_column_int64(stmtList, 5));
		xvoTableSetText(tblRow, "finishText", 10, sFinish, 0, TRUE);
		xvoTableSetInt(tblRow, "durationMs", 10, sqlite3_column_int64(stmtList, 6));
		xvoTableSetText(tblRow, "status", 6, Sched_SQLiteTextOrEmpty(stmtList, 7), 0, FALSE);
		xvoTableSetInt(tblRow, "exitCode", 8, sqlite3_column_int(stmtList, 8));
		xvoTableSetText(tblRow, "stdoutText", 10, Sched_SQLiteTextOrEmpty(stmtList, 9), 0, FALSE);
		xvoTableSetText(tblRow, "stderrText", 10, Sched_SQLiteTextOrEmpty(stmtList, 10), 0, FALSE);
		xvoTableSetText(tblRow, "message", 7, Sched_SQLiteTextOrEmpty(stmtList, 11), 0, FALSE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	if ( stmtList ) sqlite3_finalize(stmtList);

	xvoTableSetInt(tblRet, "code", 4, 0);
	xvoTableSetText(tblRet, "msg", 3, "", 0, FALSE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	return tblRet;
}

xvalue Sched_ExportLogs(int64 iTaskId, const char* sStatus, const char* sKeyword, int64 iStartFrom, int64 iStartTo)
{
	sqlite3_stmt* stmtList = NULL;
	xvalue tblRet = xvoCreateTable();
	xvalue arrData = xvoCreateArray();
	char sWhere[512] = {0};
	char sListSQL[1024] = {0};
	char sKeywordLike[320] = {0};
	int iBindIndex = 1;
	bool bHasWhere = FALSE;

	if ( iTaskId > 0 ) {
		strcat(sWhere, bHasWhere ? " AND" : " WHERE");
		strcat(sWhere, " taskId = ?");
		bHasWhere = TRUE;
	}
	if ( sStatus && sStatus[0] != '\0' ) {
		strcat(sWhere, bHasWhere ? " AND" : " WHERE");
		strcat(sWhere, " status = ?");
		bHasWhere = TRUE;
	}
	if ( sKeyword && sKeyword[0] != '\0' ) {
		strcat(sWhere, bHasWhere ? " AND" : " WHERE");
		strcat(sWhere, " (taskName LIKE ? OR message LIKE ? OR stdoutText LIKE ? OR stderrText LIKE ?)");
		bHasWhere = TRUE;
		snprintf(sKeywordLike, sizeof(sKeywordLike), "%%%s%%", sKeyword);
	}
	if ( iStartFrom > 0 ) {
		strcat(sWhere, bHasWhere ? " AND" : " WHERE");
		strcat(sWhere, " startTime >= ?");
		bHasWhere = TRUE;
	}
	if ( iStartTo > 0 ) {
		strcat(sWhere, bHasWhere ? " AND" : " WHERE");
		strcat(sWhere, " startTime <= ?");
		bHasWhere = TRUE;
	}

	snprintf(sListSQL, sizeof(sListSQL), "SELECT id, taskId, taskName, triggerSource, startTime, finishTime, durationMs, status, exitCode, stdoutText, stderrText, message FROM sched_run_log%s ORDER BY id DESC", sWhere);
	sqlite3_prepare_v3(G_DB, sListSQL, -1, 0, &stmtList, NULL);
	if ( stmtList ) {
		if ( iTaskId > 0 ) sqlite3_bind_int64(stmtList, iBindIndex++, iTaskId);
		if ( sStatus && sStatus[0] != '\0' ) sqlite3_bind_text(stmtList, iBindIndex++, sStatus, -1, SQLITE_TRANSIENT);
		if ( sKeywordLike[0] != '\0' ) {
			sqlite3_bind_text(stmtList, iBindIndex++, sKeywordLike, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmtList, iBindIndex++, sKeywordLike, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmtList, iBindIndex++, sKeywordLike, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmtList, iBindIndex++, sKeywordLike, -1, SQLITE_TRANSIENT);
		}
		if ( iStartFrom > 0 ) sqlite3_bind_int64(stmtList, iBindIndex++, iStartFrom);
		if ( iStartTo > 0 ) sqlite3_bind_int64(stmtList, iBindIndex++, iStartTo);
	}

	while ( stmtList && sqlite3_step(stmtList) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		str sStart = Sched_TimeText(sqlite3_column_int64(stmtList, 4));
		str sFinish = Sched_TimeText(sqlite3_column_int64(stmtList, 5));
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmtList, 0));
		xvoTableSetInt(tblRow, "taskId", 6, sqlite3_column_int64(stmtList, 1));
		xvoTableSetText(tblRow, "taskName", 8, Sched_SQLiteTextOrEmpty(stmtList, 2), 0, FALSE);
		xvoTableSetText(tblRow, "triggerSource", 13, Sched_SQLiteTextOrEmpty(stmtList, 3), 0, FALSE);
		xvoTableSetInt(tblRow, "startTime", 9, sqlite3_column_int64(stmtList, 4));
		xvoTableSetText(tblRow, "startText", 9, sStart, 0, TRUE);
		xvoTableSetInt(tblRow, "finishTime", 10, sqlite3_column_int64(stmtList, 5));
		xvoTableSetText(tblRow, "finishText", 10, sFinish, 0, TRUE);
		xvoTableSetInt(tblRow, "durationMs", 10, sqlite3_column_int64(stmtList, 6));
		xvoTableSetText(tblRow, "status", 6, Sched_SQLiteTextOrEmpty(stmtList, 7), 0, FALSE);
		xvoTableSetInt(tblRow, "exitCode", 8, sqlite3_column_int(stmtList, 8));
		xvoTableSetText(tblRow, "stdoutText", 10, Sched_SQLiteTextOrEmpty(stmtList, 9), 0, FALSE);
		xvoTableSetText(tblRow, "stderrText", 10, Sched_SQLiteTextOrEmpty(stmtList, 10), 0, FALSE);
		xvoTableSetText(tblRow, "message", 7, Sched_SQLiteTextOrEmpty(stmtList, 11), 0, FALSE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	if ( stmtList ) sqlite3_finalize(stmtList);

	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetText(tblRet, "message", 7, "Log export ready", 0, FALSE);
	xvoTableSetInt(tblRet, "count", 5, xvoArrayItemCount(arrData));
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	return tblRet;
}

bool Sched_ClearLogs(int64 iTaskId, const char* sStatus, const char* sKeyword, int64 iStartFrom, int64 iStartTo, int64* pAffected, str* psMessage)
{
	sqlite3_stmt* stmt = NULL;
	char sWhere[512] = {0};
	char sDeleteSQL[768] = {0};
	char sKeywordLike[320] = {0};
	int iBindIndex = 1;
	bool bHasWhere = FALSE;
	int iRet;

	if ( pAffected ) {
		*pAffected = 0;
	}

	if ( iTaskId > 0 ) {
		strcat(sWhere, bHasWhere ? " AND" : " WHERE");
		strcat(sWhere, " taskId = ?");
		bHasWhere = TRUE;
	}
	if ( sStatus && sStatus[0] != '\0' ) {
		strcat(sWhere, bHasWhere ? " AND" : " WHERE");
		strcat(sWhere, " status = ?");
		bHasWhere = TRUE;
	}
	if ( sKeyword && sKeyword[0] != '\0' ) {
		strcat(sWhere, bHasWhere ? " AND" : " WHERE");
		strcat(sWhere, " (taskName LIKE ? OR message LIKE ? OR stdoutText LIKE ? OR stderrText LIKE ?)");
		bHasWhere = TRUE;
		snprintf(sKeywordLike, sizeof(sKeywordLike), "%%%s%%", sKeyword);
	}
	if ( iStartFrom > 0 ) {
		strcat(sWhere, bHasWhere ? " AND" : " WHERE");
		strcat(sWhere, " startTime >= ?");
		bHasWhere = TRUE;
	}
	if ( iStartTo > 0 ) {
		strcat(sWhere, bHasWhere ? " AND" : " WHERE");
		strcat(sWhere, " startTime <= ?");
		bHasWhere = TRUE;
	}

	snprintf(sDeleteSQL, sizeof(sDeleteSQL), "DELETE FROM sched_run_log%s", sWhere);
	iRet = sqlite3_prepare_v3(G_DB, sDeleteSQL, -1, 0, &stmt, NULL);
	if ( iRet != SQLITE_OK ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Failed to clear logs", 0);
		return FALSE;
	}

	if ( iTaskId > 0 ) sqlite3_bind_int64(stmt, iBindIndex++, iTaskId);
	if ( sStatus && sStatus[0] != '\0' ) sqlite3_bind_text(stmt, iBindIndex++, sStatus, -1, SQLITE_TRANSIENT);
	if ( sKeywordLike[0] != '\0' ) {
		sqlite3_bind_text(stmt, iBindIndex++, sKeywordLike, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, iBindIndex++, sKeywordLike, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, iBindIndex++, sKeywordLike, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, iBindIndex++, sKeywordLike, -1, SQLITE_TRANSIENT);
	}
	if ( iStartFrom > 0 ) sqlite3_bind_int64(stmt, iBindIndex++, iStartFrom);
	if ( iStartTo > 0 ) sqlite3_bind_int64(stmt, iBindIndex++, iStartTo);

	iRet = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if ( iRet != SQLITE_DONE ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Failed to clear logs", 0);
		return FALSE;
	}

	if ( pAffected ) {
		*pAffected = sqlite3_changes(G_DB);
	}
	if ( psMessage ) {
		*psMessage = xrtFormat("Cleared %lld log record(s)", pAffected ? *pAffected : (int64)sqlite3_changes(G_DB));
	}
	return TRUE;
}

bool Sched_DeleteTask(int64 id, str* psMessage)
{
	sqlite3_stmt* stmt = NULL;
	int iRet;

	if ( id <= 0 ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Task id is required", 0);
		return FALSE;
	}
	if ( Sched_TaskRunning(G_DB, id) ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Task is running", 0);
		return FALSE;
	}

	iRet = sqlite3_prepare_v3(G_DB, "UPDATE sched_task SET isDelete = 1, enabled = 0, nextRunAt = 0, pendingRun = 0, runningCount = 0, isRunning = 0, updateTime = ? WHERE id = ? AND isDelete = 0", -1, 0, &stmt, NULL);
	if ( iRet != SQLITE_OK ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Failed to delete task", 0);
		return FALSE;
	}
	sqlite3_bind_int64(stmt, 1, xrtNow());
	sqlite3_bind_int64(stmt, 2, id);
	iRet = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	if ( iRet != SQLITE_DONE ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Failed to delete task", 0);
		return FALSE;
	}

	if ( psMessage ) *psMessage = xrtCopyStr("Task deleted", 0);
	Sched_NotifyChanged();
	return TRUE;
}

bool Sched_SetEnabled(int64 id, bool bEnabled, str* psMessage)
{
	SchedTaskSnapshot tTask;
	sqlite3_stmt* stmt = NULL;
	int64 iNext;
	int iRet;

	memset(&tTask, 0, sizeof(tTask));
	if ( !Sched_LoadTaskSnapshotById(G_DB, id, &tTask) ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Task not found", 0);
		return FALSE;
	}

	tTask.enabled = bEnabled ? 1 : 0;
	iNext = bEnabled ? Sched_CalcNextTime(&tTask, xrtNow()) : 0;
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE sched_task SET enabled = ?, nextRunAt = ?, pendingRun = CASE WHEN ? = 0 THEN 0 ELSE pendingRun END, updateTime = ? WHERE id = ? AND isDelete = 0", -1, 0, &stmt, NULL);
	if ( iRet != SQLITE_OK ) {
		Sched_FreeTaskSnapshot(&tTask);
		if ( psMessage ) *psMessage = xrtCopyStr("Failed to update task status", 0);
		return FALSE;
	}
	sqlite3_bind_int(stmt, 1, bEnabled ? 1 : 0);
	sqlite3_bind_int64(stmt, 2, iNext);
	sqlite3_bind_int(stmt, 3, bEnabled ? 1 : 0);
	sqlite3_bind_int64(stmt, 4, xrtNow());
	sqlite3_bind_int64(stmt, 5, id);
	iRet = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	Sched_FreeTaskSnapshot(&tTask);
	if ( iRet != SQLITE_DONE ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Failed to update task status", 0);
		return FALSE;
	}

	if ( psMessage ) *psMessage = xrtCopyStr(bEnabled ? "Task enabled" : "Task disabled", 0);
	Sched_NotifyChanged();
	return TRUE;
}

bool Sched_SetEnabledBatch(xvalue arrIDs, bool bEnabled, int64* pAffected, str* psMessage)
{
	uint32 i;
	int64 iAffected = 0;
	int64 iSkipped = 0;
	int64 iFailed = 0;
	bool bAnyChange = FALSE;

	if ( pAffected ) *pAffected = 0;
	if ( arrIDs == NULL || xvoType(arrIDs) != XVO_DT_ARRAY || xvoArrayItemCount(arrIDs) <= 0 ) {
		if ( psMessage ) *psMessage = xrtCopyStr("No task ids provided", 0);
		return FALSE;
	}

	for ( i = 0; i < xvoArrayItemCount(arrIDs); i++ ) {
		int64 id = xvoArrayGetInt(arrIDs, i);
		SchedTaskSnapshot tTask;
		sqlite3_stmt* stmt = NULL;
		int64 iNext = 0;
		int iRet;

		if ( id <= 0 ) {
			iSkipped++;
			continue;
		}

		memset(&tTask, 0, sizeof(tTask));
		if ( !Sched_LoadTaskSnapshotById(G_DB, id, &tTask) ) {
			iSkipped++;
			continue;
		}
		if ( Sched_TaskRunning(G_DB, id) ) {
			Sched_FreeTaskSnapshot(&tTask);
			iSkipped++;
			continue;
		}

		tTask.enabled = bEnabled ? 1 : 0;
		iNext = bEnabled ? Sched_CalcNextTime(&tTask, xrtNow()) : 0;
		iRet = sqlite3_prepare_v3(G_DB, "UPDATE sched_task SET enabled = ?, nextRunAt = ?, pendingRun = CASE WHEN ? = 0 THEN 0 ELSE pendingRun END, updateTime = ? WHERE id = ? AND isDelete = 0", -1, 0, &stmt, NULL);
		if ( iRet != SQLITE_OK ) {
			Sched_FreeTaskSnapshot(&tTask);
			iFailed++;
			continue;
		}

		sqlite3_bind_int(stmt, 1, bEnabled ? 1 : 0);
		sqlite3_bind_int64(stmt, 2, iNext);
		sqlite3_bind_int(stmt, 3, bEnabled ? 1 : 0);
		sqlite3_bind_int64(stmt, 4, xrtNow());
		sqlite3_bind_int64(stmt, 5, id);
		iRet = sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		Sched_FreeTaskSnapshot(&tTask);

		if ( iRet == SQLITE_DONE ) {
			iAffected++;
			bAnyChange = TRUE;
		} else {
			iFailed++;
		}
	}

	if ( pAffected ) *pAffected = iAffected;
	if ( bAnyChange ) {
		Sched_NotifyChanged();
	}

	if ( psMessage ) {
		*psMessage = xrtFormat(
			"%s %lld task(s), skipped %lld, failed %lld",
			bEnabled ? "Enabled" : "Disabled",
			iAffected,
			iSkipped,
			iFailed
		);
	}
	return (iFailed == 0 && iAffected > 0);
}

bool Sched_DeleteTaskBatch(xvalue arrIDs, int64* pAffected, str* psMessage)
{
	uint32 i;
	int64 iAffected = 0;
	int64 iSkipped = 0;
	int64 iFailed = 0;
	bool bAnyChange = FALSE;

	if ( pAffected ) *pAffected = 0;
	if ( arrIDs == NULL || xvoType(arrIDs) != XVO_DT_ARRAY || xvoArrayItemCount(arrIDs) <= 0 ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Task ids are required", 0);
		return FALSE;
	}

	for ( i = 0; i < xvoArrayItemCount(arrIDs); i++ ) {
		int64 id = xvoArrayGetInt(arrIDs, i);
		sqlite3_stmt* stmt = NULL;
		int iRet;

		if ( id <= 0 ) {
			iSkipped++;
			continue;
		}
		if ( Sched_TaskRunning(G_DB, id) ) {
			iSkipped++;
			continue;
		}

		iRet = sqlite3_prepare_v3(G_DB, "UPDATE sched_task SET isDelete = 1, enabled = 0, nextRunAt = 0, pendingRun = 0, updateTime = ? WHERE id = ? AND isDelete = 0", -1, 0, &stmt, NULL);
		if ( iRet != SQLITE_OK ) {
			iFailed++;
			continue;
		}

		sqlite3_bind_int64(stmt, 1, xrtNow());
		sqlite3_bind_int64(stmt, 2, id);
		iRet = sqlite3_step(stmt);
		sqlite3_finalize(stmt);

		if ( iRet == SQLITE_DONE && sqlite3_changes(G_DB) > 0 ) {
			iAffected++;
			bAnyChange = TRUE;
		} else if ( iRet == SQLITE_DONE ) {
			iSkipped++;
		} else {
			iFailed++;
		}
	}

	if ( pAffected ) *pAffected = iAffected;
	if ( bAnyChange ) {
		Sched_NotifyChanged();
	}

	if ( psMessage ) {
		*psMessage = xrtFormat("Deleted %lld task(s), skipped %lld, failed %lld", iAffected, iSkipped, iFailed);
	}
	return (iFailed == 0 && iAffected > 0);
}

static bool Sched_UpdateTaskRunningFlag(sqlite3* pDB, int64 id, int bRunning, const char* sStatus, const char* sMessage)
{
	sqlite3_stmt* stmt = NULL;
	int iRet = sqlite3_prepare_v3(pDB, "UPDATE sched_task SET isRunning = ?, lastStatus = ?, lastMessage = ?, updateTime = ? WHERE id = ? AND isDelete = 0", -1, 0, &stmt, NULL);
	if ( iRet != SQLITE_OK ) {
		return FALSE;
	}
	sqlite3_bind_int(stmt, 1, bRunning);
	sqlite3_bind_text(stmt, 2, sStatus ? sStatus : "", -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 3, sMessage ? sMessage : "", -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt, 4, xrtNow());
	sqlite3_bind_int64(stmt, 5, id);
	iRet = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return (iRet == SQLITE_DONE);
}

static void Sched_TccErrorCallback(void* pOpaque, const char* sMessage)
{
	SchedTccErrorBuffer* pBuffer = (SchedTccErrorBuffer*)pOpaque;
	str sNext;

	if ( pBuffer == NULL || sMessage == NULL || sMessage[0] == '\0' ) {
		return;
	}

	sNext = xrtFormat("%s%s%s", Sched_StrOrEmpty(pBuffer->sText), (pBuffer->sText && pBuffer->sText[0] != '\0') ? "\n" : "", Sched_CStrOrEmpty(sMessage));
	if ( pBuffer->sText ) {
		xrtFree(pBuffer->sText);
	}
	pBuffer->sText = sNext;
}

typedef struct SchedTextBuffer {
	char* sText;
	size_t iSize;
	size_t iCap;
	size_t iLimit;
	bool bTruncated;
} SchedTextBuffer;

static void Sched_TextBufferInit(SchedTextBuffer* pBuffer, size_t iLimit)
{
	memset(pBuffer, 0, sizeof(*pBuffer));
	pBuffer->iLimit = iLimit;
}

static void Sched_TextBufferAppend(SchedTextBuffer* pBuffer, const char* pData, size_t iSize)
{
	size_t iNeed;
	size_t iWrite = iSize;

	if ( pBuffer == NULL || pData == NULL || iSize == 0 ) {
		return;
	}
	if ( pBuffer->iLimit > 0 && pBuffer->iSize >= pBuffer->iLimit ) {
		pBuffer->bTruncated = TRUE;
		return;
	}
	if ( pBuffer->iLimit > 0 && (pBuffer->iSize + iWrite) > pBuffer->iLimit ) {
		iWrite = pBuffer->iLimit - pBuffer->iSize;
		pBuffer->bTruncated = TRUE;
	}
	if ( iWrite == 0 ) {
		return;
	}

	iNeed = pBuffer->iSize + iWrite + 1;
	if ( iNeed > pBuffer->iCap ) {
		size_t iNewCap = pBuffer->iCap ? pBuffer->iCap : 4096;
		while ( iNewCap < iNeed ) {
			iNewCap *= 2;
		}
		pBuffer->sText = (char*)xrtRealloc(pBuffer->sText, iNewCap);
		pBuffer->iCap = iNewCap;
	}

	memcpy(pBuffer->sText + pBuffer->iSize, pData, iWrite);
	pBuffer->iSize += iWrite;
	pBuffer->sText[pBuffer->iSize] = '\0';
}

static str Sched_TextBufferDetach(SchedTextBuffer* pBuffer)
{
	str sText;

	if ( pBuffer == NULL ) {
		return xrtCopyStr("", 0);
	}
	if ( pBuffer->sText == NULL ) {
		return xrtCopyStr("", 0);
	}
	sText = pBuffer->sText;
	pBuffer->sText = NULL;
	pBuffer->iSize = 0;
	pBuffer->iCap = 0;
	return sText;
}

static str Sched_QuoteCommandArg(const char* sArg)
{
	str sOut;
	size_t iLen;
	size_t iPos = 0;

	if ( sArg == NULL ) {
		return xrtCopyStr("\"\"", 0);
	}

	iLen = strlen(sArg);
	sOut = (str)xrtMalloc(iLen * 2 + 3);
	sOut[iPos++] = '"';
	for ( size_t i = 0; i < iLen; i++ ) {
		if ( sArg[i] == '"' ) {
			sOut[iPos++] = '\\';
		}
		sOut[iPos++] = sArg[i];
	}
	sOut[iPos++] = '"';
	sOut[iPos] = '\0';
	return sOut;
}

static str Sched_BuildCommandLine(const char** arrArgs, uint32 iArgCount)
{
	str sCommand = xrtCopyStr("", 0);

	for ( uint32 i = 0; i < iArgCount; i++ ) {
		str sQuoted = Sched_QuoteCommandArg(arrArgs[i] ? arrArgs[i] : "");
		str sNext = xrtFormat("%s%s%s", Sched_StrOrEmpty(sCommand), i == 0 ? "" : " ", Sched_StrOrEmpty(sQuoted));
		xrtFree(sCommand);
		if ( sQuoted ) xrtFree(sQuoted);
		sCommand = sNext;
	}

	return sCommand;
}

#if defined(_WIN32) || defined(_WIN64)
static void Sched_ReadPipeOutput(HANDLE hPipe, SchedTextBuffer* pBuffer, bool bDrainAll)
{
	char sChunk[4096];

	if ( hPipe == NULL || hPipe == INVALID_HANDLE_VALUE || pBuffer == NULL ) {
		return;
	}

	while ( TRUE ) {
		DWORD iAvail = 0;
		DWORD iRead = 0;
		BOOL bPeek = PeekNamedPipe(hPipe, NULL, 0, NULL, &iAvail, NULL);
		DWORD iNeedRead = iAvail;

		if ( !bPeek ) {
			if ( GetLastError() == ERROR_BROKEN_PIPE ) {
				break;
			}
			if ( !bDrainAll ) {
				break;
			}
			iNeedRead = sizeof(sChunk);
		}
		if ( iNeedRead == 0 ) {
			if ( !bDrainAll ) {
				break;
			}
			iNeedRead = sizeof(sChunk);
		}
		if ( iNeedRead > sizeof(sChunk) ) {
			iNeedRead = sizeof(sChunk);
		}
		if ( !ReadFile(hPipe, sChunk, iNeedRead, &iRead, NULL) || iRead == 0 ) {
			break;
		}
		Sched_TextBufferAppend(pBuffer, sChunk, iRead);
		if ( !bDrainAll && iRead < iNeedRead ) {
			break;
		}
	}
}

static bool Sched_RunProcessCommand(const char* sCommandLine, const char* sWorkDir, uint32 iTimeoutMs, int* pExitCode, int* pTimedOut, str* psStdout, str* psStderr, str* psMessage)
{
	SECURITY_ATTRIBUTES tSA;
	STARTUPINFOA tSI;
	PROCESS_INFORMATION tPI;
	HANDLE hStdoutRead = NULL;
	HANDLE hStdoutWrite = NULL;
	HANDLE hStderrRead = NULL;
	HANDLE hStderrWrite = NULL;
	HANDLE hJob = NULL;
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION tJobLimit;
	SchedTextBuffer tStdout;
	SchedTextBuffer tStderr;
	DWORD iWaitRet;
	DWORD iExit = 0;
	DWORD iOsError = 0;
	ULONGLONG iStartTick;
	BOOL bCreated = FALSE;
	BOOL bTimedOut = FALSE;
	str sCmdMutable = NULL;

	memset(&tSA, 0, sizeof(tSA));
	memset(&tSI, 0, sizeof(tSI));
	memset(&tPI, 0, sizeof(tPI));
	memset(&tJobLimit, 0, sizeof(tJobLimit));
	Sched_TextBufferInit(&tStdout, 262144);
	Sched_TextBufferInit(&tStderr, 262144);

	tSA.nLength = sizeof(tSA);
	tSA.bInheritHandle = TRUE;
	if ( !CreatePipe(&hStdoutRead, &hStdoutWrite, &tSA, 0) ) goto cleanup;
	if ( !SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0) ) goto cleanup;
	if ( !CreatePipe(&hStderrRead, &hStderrWrite, &tSA, 0) ) goto cleanup;
	if ( !SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0) ) goto cleanup;

	tSI.cb = sizeof(tSI);
	tSI.dwFlags = STARTF_USESTDHANDLES;
	tSI.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	tSI.hStdOutput = hStdoutWrite;
	tSI.hStdError = hStderrWrite;

	hJob = CreateJobObjectA(NULL, NULL);
	if ( hJob ) {
		tJobLimit.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
		SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &tJobLimit, sizeof(tJobLimit));
	}

	sCmdMutable = xrtCopyStr(sCommandLine ? (str)sCommandLine : (str)"", 0);
	bCreated = CreateProcessA(NULL, sCmdMutable, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, sWorkDir, &tSI, &tPI);
	if ( !bCreated ) {
		iOsError = GetLastError();
		goto cleanup;
	}
	if ( hJob ) {
		AssignProcessToJobObject(hJob, tPI.hProcess);
	}

	CloseHandle(hStdoutWrite);
	hStdoutWrite = NULL;
	CloseHandle(hStderrWrite);
	hStderrWrite = NULL;

	iStartTick = GetTickCount64();
	while ( TRUE ) {
		Sched_ReadPipeOutput(hStdoutRead, &tStdout, FALSE);
		Sched_ReadPipeOutput(hStderrRead, &tStderr, FALSE);
		iWaitRet = WaitForSingleObject(tPI.hProcess, 50);
		if ( iWaitRet == WAIT_OBJECT_0 ) {
			break;
		}
		if ( iWaitRet == WAIT_FAILED ) {
			iOsError = GetLastError();
			break;
		}
		if ( iTimeoutMs > 0 && (GetTickCount64() - iStartTick) >= iTimeoutMs ) {
			bTimedOut = TRUE;
			if ( hJob ) TerminateJobObject(hJob, 124);
			else TerminateProcess(tPI.hProcess, 124);
			WaitForSingleObject(tPI.hProcess, 5000);
			break;
		}
	}

	Sched_ReadPipeOutput(hStdoutRead, &tStdout, TRUE);
	Sched_ReadPipeOutput(hStderrRead, &tStderr, TRUE);
	if ( bCreated ) {
		GetExitCodeProcess(tPI.hProcess, &iExit);
	}

cleanup:
	if ( hStdoutWrite ) CloseHandle(hStdoutWrite);
	if ( hStderrWrite ) CloseHandle(hStderrWrite);
	if ( hStdoutRead ) CloseHandle(hStdoutRead);
	if ( hStderrRead ) CloseHandle(hStderrRead);
	if ( tPI.hThread ) CloseHandle(tPI.hThread);
	if ( tPI.hProcess ) CloseHandle(tPI.hProcess);
	if ( hJob ) CloseHandle(hJob);
	if ( sCmdMutable ) xrtFree(sCmdMutable);

	if ( pExitCode ) {
		*pExitCode = (int)iExit;
	}
	if ( pTimedOut ) {
		*pTimedOut = bTimedOut ? 1 : 0;
	}
	if ( psStdout ) *psStdout = Sched_TextBufferDetach(&tStdout); else if ( tStdout.sText ) xrtFree(tStdout.sText);
	if ( psStderr ) *psStderr = Sched_TextBufferDetach(&tStderr); else if ( tStderr.sText ) xrtFree(tStderr.sText);

	if ( psMessage ) {
		if ( !bCreated ) *psMessage = xrtFormat("Process start failed (os=%lu)", (unsigned long)iOsError);
		else if ( bTimedOut ) *psMessage = xrtCopyStr("Task timed out and was terminated", 0);
		else if ( iOsError != 0 ) *psMessage = xrtFormat("Process wait failed (os=%lu)", (unsigned long)iOsError);
		else if ((int)iExit == 0) *psMessage = xrtCopyStr("Task finished", 0);
		else *psMessage = xrtFormat("Task exited with code %d", (int)iExit);
	}

	return bCreated && !bTimedOut && iOsError == 0;
}
#endif

static bool Sched_RunShellTask(const SchedTaskSnapshot* pTask, const char* sTriggerSource, int* pExitCode, int* pTimedOut, str* psStdout, str* psStderr, str* psMessage)
{
	str sExt;
	str sFileName;
	str sScriptPath;
	const char* sCodeText;
	uint32 iCodeTextLen;
	bool bRet = FALSE;

	if ( G_SchedCachePath == NULL ) {
		if ( pTimedOut ) *pTimedOut = 0;
		if ( psMessage ) *psMessage = xrtCopyStr("Scheduler cache path is missing", 0);
		return FALSE;
	}

	if ( Sched_TextEquals(pTask->sShellType, "cmd") ) sExt = xrtCopyStr("bat", 0);
	else if ( Sched_TextEquals(pTask->sShellType, "powershell") ) sExt = xrtCopyStr("ps1", 0);
	else sExt = xrtCopyStr("sh", 0);

	sFileName = xrtFormat("task_%lld_%lld.%s", pTask->id, xrtNow(), sExt);
	sScriptPath = xrtPathJoin(2, G_SchedCachePath, sFileName);
	xrtFree(sFileName);
	xrtFree(sExt);

	sCodeText = Sched_CStrOrEmpty((const char*)pTask->sCodeText);
	iCodeTextLen = (uint32)strlen(sCodeText);

	if ( !xrtFilePutAll(sScriptPath, (str)sCodeText, iCodeTextLen) ) {
		xrtFree(sScriptPath);
		if ( psMessage ) *psMessage = xrtCopyStr("Failed to write script file", 0);
		return FALSE;
	}

	#if defined(_WIN32) || defined(_WIN64)
		{
			const char* arrArgs[7];
			uint32 iArgCount = 0;
			str sCommandLine;

			if ( Sched_TextEquals(pTask->sShellType, "cmd") ) {
				arrArgs[iArgCount++] = "cmd.exe";
				arrArgs[iArgCount++] = "/D";
				arrArgs[iArgCount++] = "/S";
				arrArgs[iArgCount++] = "/C";
				arrArgs[iArgCount++] = sScriptPath;
			} else {
				arrArgs[iArgCount++] = "powershell.exe";
				arrArgs[iArgCount++] = "-NoLogo";
				arrArgs[iArgCount++] = "-NoProfile";
				arrArgs[iArgCount++] = "-ExecutionPolicy";
				arrArgs[iArgCount++] = "Bypass";
				arrArgs[iArgCount++] = "-File";
				arrArgs[iArgCount++] = sScriptPath;
			}

			sCommandLine = Sched_BuildCommandLine(arrArgs, iArgCount);
			bRet = Sched_RunProcessCommand(sCommandLine, Sched_GetWorkDir(pTask), Sched_GetTimeoutMs(pTask), pExitCode, pTimedOut, psStdout, psStderr, psMessage);
			xrtFree(sCommandLine);
		}
	#else
		{
			str sCommand = NULL;
			FILE* fp = NULL;
			char sBuffer[1024];
			str sOutput = xrtCopyStr("", 0);

			if ( Sched_TextEquals(pTask->sShellType, "bash") ) sCommand = xrtFormat("cd \"%s\" && bash \"%s\" 2>&1", Sched_GetWorkDir(pTask), sScriptPath);
			else sCommand = xrtFormat("cd \"%s\" && sh \"%s\" 2>&1", Sched_GetWorkDir(pTask), sScriptPath);

			fp = popen(sCommand, "r");
			if ( fp == NULL ) {
				if ( pTimedOut ) *pTimedOut = 0;
				xrtFileDelete(sScriptPath);
				xrtFree(sScriptPath);
				xrtFree(sCommand);
				xrtFree(sOutput);
				if ( psMessage ) *psMessage = xrtCopyStr("Shell process start failed", 0);
				return FALSE;
			}
			while ( fgets(sBuffer, sizeof(sBuffer), fp) != NULL ) {
				str sNext = xrtFormat("%s%s", sOutput ? sOutput : "", sBuffer);
				xrtFree(sOutput);
				sOutput = sNext;
			}

			if ( pExitCode ) *pExitCode = pclose(fp);
			if ( pTimedOut ) *pTimedOut = 0;
			if ( psStdout ) *psStdout = sOutput; else xrtFree(sOutput);
			if ( psStderr ) *psStderr = xrtCopyStr("", 0);
			if ( psMessage ) *psMessage = xrtCopyStr((pExitCode && *pExitCode == 0) ? "Task finished" : "Task exited with code", 0);
			xrtFree(sCommand);
			bRet = TRUE;
		}
	#endif

	xrtFileDelete(sScriptPath);
	xrtFree(sScriptPath);
	(void)sTriggerSource;
	return bRet;
}

static bool Sched_RunCTask(const SchedTaskSnapshot* pTask, const char* sTriggerSource, int* pExitCode, int* pTimedOut, str* psStdout, str* psStderr, str* psMessage)
{
	str sRunnerName = NULL;
	str sConfigName = NULL;
	str sParamName = NULL;
	str sRunnerFile = NULL;
	str sConfigFile = NULL;
	str sParamFile = NULL;
	str sBaseName = NULL;
	const char* sCodeText = NULL;
	str sTaskNameEscaped = NULL;
	str sTriggerEscaped = NULL;
	str sScheduleTypeEscaped = NULL;
	str sExecTypeEscaped = NULL;
	str sShellTypeEscaped = NULL;
	str sOverlapPolicyEscaped = NULL;
	str sCustomTextEscaped = NULL;
	str sWorkDirEscaped = NULL;
	str sAppPathEscaped = NULL;
	str sSchedPathEscaped = NULL;
	str sCachePathEscaped = NULL;
	str sRunnerFileEscaped = NULL;
	str sConfigFileEscaped = NULL;
	str sParamFileEscaped = NULL;
	str sRunnerCode = NULL;
	str sConfigJson = NULL;
	str sCachePathSlash = NULL;
	str sParamPathSlash = NULL;
	str sParamJson = NULL;
	str sCommandLine = NULL;
	bool bRet = FALSE;

	if ( G_SchedCachePath == NULL || G_SchedXSPath == NULL || !xrtFileExists(G_SchedXSPath) ) {
		if ( pTimedOut ) *pTimedOut = 0;
		if ( psMessage ) *psMessage = xrtCopyStr("Scheduler xs runtime is missing", 0);
		return FALSE;
	}

	sBaseName = xrtFormat("task_%lld_%lld", pTask->id, xrtNow());
	sRunnerName = xrtFormat("%s_runner.c", sBaseName);
	sConfigName = xrtFormat("%s_runner.json", sBaseName);
	sParamName = xrtFormat("%s_param.json", sBaseName);
	sRunnerFile = xrtPathJoin(2, G_SchedCachePath, sRunnerName);
	sConfigFile = xrtPathJoin(2, G_SchedCachePath, sConfigName);
	sParamFile = xrtPathJoin(2, G_SchedCachePath, sParamName);
	if ( sBaseName ) xrtFree(sBaseName);
	if ( sRunnerName ) xrtFree(sRunnerName);
	if ( sConfigName ) xrtFree(sConfigName);
	if ( sParamName ) xrtFree(sParamName);
	sCodeText = Sched_CStrOrEmpty((const char*)pTask->sCodeText);

	sTaskNameEscaped = Sched_EscapeCString(Sched_CStrOrEmpty((const char*)pTask->sName));
	sTriggerEscaped = Sched_EscapeCString(Sched_CStrOrEmpty(sTriggerSource ? sTriggerSource : "scheduler"));
	sScheduleTypeEscaped = Sched_EscapeCString(Sched_CStrOrEmpty((const char*)pTask->sScheduleType));
	sExecTypeEscaped = Sched_EscapeCString(Sched_CStrOrEmpty((const char*)pTask->sExecType));
	sShellTypeEscaped = Sched_EscapeCString(Sched_CStrOrEmpty((const char*)pTask->sShellType));
	sOverlapPolicyEscaped = Sched_EscapeCString(Sched_CStrOrEmpty((const char*)pTask->sOverlapPolicy));
	sCustomTextEscaped = Sched_EscapeCString(Sched_CStrOrEmpty((const char*)pTask->sCustomText));
	sWorkDirEscaped = Sched_EscapeCString(Sched_GetWorkDir(pTask));
	sAppPathEscaped = Sched_EscapeCString(Sched_CStrOrEmpty((const char*)AppPath));
	sSchedPathEscaped = Sched_EscapeCString(Sched_CStrOrEmpty((const char*)G_SchedPath));
	sCachePathEscaped = Sched_EscapeCString(Sched_CStrOrEmpty((const char*)G_SchedCachePath));
	sRunnerFileEscaped = Sched_EscapeCString(Sched_CStrOrEmpty((const char*)sRunnerFile));
	sConfigFileEscaped = Sched_EscapeCString(Sched_CStrOrEmpty((const char*)sConfigFile));
	sParamFileEscaped = Sched_EscapeCString(Sched_CStrOrEmpty((const char*)sParamFile));
	sParamJson = xrtFormat(
		"{\n"
		"\t\"taskId\": %lld,\n"
		"\t\"taskName\": \"%s\",\n"
		"\t\"triggerSource\": \"%s\",\n"
		"\t\"scheduleType\": \"%s\",\n"
		"\t\"execType\": \"%s\",\n"
		"\t\"shellType\": \"%s\",\n"
		"\t\"overlapPolicy\": \"%s\",\n"
		"\t\"customJson\": \"%s\",\n"
		"\t\"workDir\": \"%s\",\n"
		"\t\"appPath\": \"%s\",\n"
		"\t\"schedPath\": \"%s\",\n"
		"\t\"cachePath\": \"%s\",\n"
		"\t\"runnerFile\": \"%s\",\n"
		"\t\"runnerConfigFile\": \"%s\",\n"
		"\t\"runnerParamFile\": \"%s\",\n"
		"\t\"startTime\": %lld,\n"
		"\t\"timeoutSec\": %d\n"
		"}\n",
		pTask->id,
		Sched_StrOrEmpty(sTaskNameEscaped),
		Sched_StrOrEmpty(sTriggerEscaped),
		Sched_StrOrEmpty(sScheduleTypeEscaped),
		Sched_StrOrEmpty(sExecTypeEscaped),
		Sched_StrOrEmpty(sShellTypeEscaped),
		Sched_StrOrEmpty(sOverlapPolicyEscaped),
		Sched_StrOrEmpty(sCustomTextEscaped),
		Sched_StrOrEmpty(sWorkDirEscaped),
		Sched_StrOrEmpty(sAppPathEscaped),
		Sched_StrOrEmpty(sSchedPathEscaped),
		Sched_StrOrEmpty(sCachePathEscaped),
		Sched_StrOrEmpty(sRunnerFileEscaped),
		Sched_StrOrEmpty(sConfigFileEscaped),
		Sched_StrOrEmpty(sParamFileEscaped),
		xrtNow(),
		pTask->timeoutSec
	);
	if ( sParamJson == NULL || !xrtFilePutAll(sParamFile, sParamJson, (uint32)strlen(sParamJson)) ) {
		if ( pTimedOut ) *pTimedOut = 0;
		if ( psMessage ) *psMessage = xrtCopyStr("Failed to write C task param file", 0);
		if ( sParamJson ) xrtFree(sParamJson);
		if ( sTaskNameEscaped ) xrtFree(sTaskNameEscaped);
		if ( sTriggerEscaped ) xrtFree(sTriggerEscaped);
		if ( sScheduleTypeEscaped ) xrtFree(sScheduleTypeEscaped);
		if ( sExecTypeEscaped ) xrtFree(sExecTypeEscaped);
		if ( sShellTypeEscaped ) xrtFree(sShellTypeEscaped);
		if ( sOverlapPolicyEscaped ) xrtFree(sOverlapPolicyEscaped);
		if ( sCustomTextEscaped ) xrtFree(sCustomTextEscaped);
		if ( sWorkDirEscaped ) xrtFree(sWorkDirEscaped);
		if ( sAppPathEscaped ) xrtFree(sAppPathEscaped);
		if ( sSchedPathEscaped ) xrtFree(sSchedPathEscaped);
		if ( sCachePathEscaped ) xrtFree(sCachePathEscaped);
		if ( sRunnerFileEscaped ) xrtFree(sRunnerFileEscaped);
		if ( sConfigFileEscaped ) xrtFree(sConfigFileEscaped);
		if ( sParamFileEscaped ) xrtFree(sParamFileEscaped);
		if ( sRunnerFile ) xrtFree(sRunnerFile);
		if ( sConfigFile ) xrtFree(sConfigFile);
		if ( sParamFile ) {
			xrtFileDelete(sParamFile);
			xrtFree(sParamFile);
		}
		return FALSE;
	}
	xrtFree(sParamJson);
	sParamJson = NULL;

	sRunnerCode = Sched_RenderRunnerTemplate(sCodeText);
	if ( sTaskNameEscaped ) xrtFree(sTaskNameEscaped);
	if ( sTriggerEscaped ) xrtFree(sTriggerEscaped);
	if ( sScheduleTypeEscaped ) xrtFree(sScheduleTypeEscaped);
	if ( sExecTypeEscaped ) xrtFree(sExecTypeEscaped);
	if ( sShellTypeEscaped ) xrtFree(sShellTypeEscaped);
	if ( sOverlapPolicyEscaped ) xrtFree(sOverlapPolicyEscaped);
	if ( sCustomTextEscaped ) xrtFree(sCustomTextEscaped);
	if ( sWorkDirEscaped ) xrtFree(sWorkDirEscaped);
	if ( sAppPathEscaped ) xrtFree(sAppPathEscaped);
	if ( sSchedPathEscaped ) xrtFree(sSchedPathEscaped);
	if ( sCachePathEscaped ) xrtFree(sCachePathEscaped);
	if ( sRunnerFileEscaped ) xrtFree(sRunnerFileEscaped);
	if ( sConfigFileEscaped ) xrtFree(sConfigFileEscaped);
	if ( sParamFileEscaped ) xrtFree(sParamFileEscaped);
	if ( sRunnerCode == NULL || !xrtFilePutAll(sRunnerFile, sRunnerCode, (uint32)strlen(sRunnerCode)) ) {
		if ( pTimedOut ) *pTimedOut = 0;
		if ( psMessage ) *psMessage = xrtCopyStr("Failed to write C task runner file", 0);
		if ( sRunnerCode ) xrtFree(sRunnerCode);
		if ( sRunnerFile ) {
			xrtFileDelete(sRunnerFile);
			xrtFree(sRunnerFile);
		}
		if ( sConfigFile ) xrtFree(sConfigFile);
		if ( sParamFile ) {
			xrtFileDelete(sParamFile);
			xrtFree(sParamFile);
		}
		return FALSE;
	}
	xrtFree(sRunnerCode);
	sRunnerCode = NULL;

	sCachePathSlash = Sched_CopySlashPath(G_SchedCachePath);
	sParamPathSlash = Sched_CopySlashPath(sParamFile);
	sConfigJson = xrtFormat(
		"{\n"
		"\t\"services\": [\n"
		"\t\t{\n"
		"\t\t\t\"enabled\": true,\n"
		"\t\t\t\"class\": \"custom\",\n"
		"\t\t\t\"name\": \"Sched Task Runner\",\n"
		"\t\t\t\"desc\": \"Sched Task Runner\",\n"
		"\t\t\t\"param\": \"%s\",\n"
		"\t\t\t\"ip\": \"127.0.0.1\",\n"
		"\t\t\t\"port\": 16511,\n"
		"\t\t\t\"debug\": false,\n"
		"\t\t\t\"path\": \"%s\",\n"
		"\t\t\t\"devlang\": \"c\",\n"
		"\t\t\t\"devfile\": \"%s\"\n"
		"\t\t}\n"
		"\t]\n"
		"}\n",
		Sched_StrOrEmpty(sParamPathSlash),
		Sched_StrOrEmpty(sCachePathSlash),
		Sched_PathFileName(sRunnerFile)
	);
	if ( sCachePathSlash ) xrtFree(sCachePathSlash);
	if ( sParamPathSlash ) xrtFree(sParamPathSlash);
	if ( sConfigJson == NULL || !xrtFilePutAll(sConfigFile, sConfigJson, (uint32)strlen(sConfigJson)) ) {
		if ( pTimedOut ) *pTimedOut = 0;
		if ( psMessage ) *psMessage = xrtCopyStr("Failed to write C task runner config", 0);
		if ( sConfigJson ) xrtFree(sConfigJson);
		xrtFileDelete(sRunnerFile);
		xrtFileDelete(sParamFile);
		xrtFree(sRunnerFile);
		xrtFree(sConfigFile);
		xrtFree(sParamFile);
		return FALSE;
	}
	xrtFree(sConfigJson);
	sConfigJson = NULL;

	{
		const char* arrArgs[2];
		arrArgs[0] = G_SchedXSPath;
		arrArgs[1] = sConfigFile;
		sCommandLine = Sched_BuildCommandLine(arrArgs, 2);
		bRet = Sched_RunProcessCommand(sCommandLine, AppPath ? (const char*)AppPath : Sched_GetWorkDir(pTask), Sched_GetTimeoutMs(pTask), pExitCode, pTimedOut, psStdout, psStderr, psMessage);
		xrtFree(sCommandLine);
	}

	xrtFileDelete(sRunnerFile);
	xrtFileDelete(sConfigFile);
	xrtFileDelete(sParamFile);
	xrtFree(sRunnerFile);
	xrtFree(sConfigFile);
	xrtFree(sParamFile);
	return bRet;
}

static uint32 Sched_WorkerProc(ptr pParam)
{
	SchedWorkerContext* pCtx = (SchedWorkerContext*)pParam;
	sqlite3* pDB = Sched_OpenStandaloneDB();
	sqlite3_stmt* stmt = NULL;
	SchedTaskSnapshot tState;
	int64 iRunLogId = 0;
	int64 iStart = xrtNow();
	int64 iFinish;
	int64 iDuration;
	int64 iNextRun;
	int64 iRetryNextRun = 0;
	int iNextRunningCount = 0;
	int iNextRetryState = 0;
	int iExitCode = -1;
	int bTimedOut = 0;
	int bStartPending = 0;
	int bScheduleRetry = 0;
	const char* sFinalStatus;
	const char* sTaskStatus;
	str sStdout = NULL;
	str sStderr = NULL;
	str sMessage = NULL;
	bool bResult = FALSE;

	if ( pCtx == NULL || pDB == NULL ) {
		if ( pDB ) sqlite3_close(pDB);
		Sched_FreeWorkerContext(pCtx);
		return 1;
	}

	sqlite3_prepare_v3(pDB, "INSERT INTO sched_run_log (taskId, taskName, triggerSource, startTime, finishTime, durationMs, status, exitCode, stdoutText, stderrText, message) VALUES (?, ?, ?, ?, 0, 0, 'running', 0, '', '', '')", -1, 0, &stmt, NULL);
	if ( stmt ) {
		sqlite3_bind_int64(stmt, 1, pCtx->Task.id);
		sqlite3_bind_text(stmt, 2, pCtx->Task.sName, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 3, Sched_CStrOrEmpty((const char*)pCtx->sTriggerSource), -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt, 4, iStart);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		iRunLogId = sqlite3_last_insert_rowid(pDB);
	}

	Sched_UpdateTaskRunningFlag(pDB, pCtx->Task.id, 1, "running", "Task started");

	if ( Sched_TextEquals(pCtx->Task.sExecType, "shell") ) bResult = Sched_RunShellTask(&pCtx->Task, pCtx->sTriggerSource, &iExitCode, &bTimedOut, &sStdout, &sStderr, &sMessage);
	else bResult = Sched_RunCTask(&pCtx->Task, pCtx->sTriggerSource, &iExitCode, &bTimedOut, &sStdout, &sStderr, &sMessage);

	iFinish = xrtNow();
	iDuration = (iFinish - iStart) * 1000;
	iNextRun = Sched_CalcNextAfterRun(&pCtx->Task, iFinish);
	sFinalStatus = bTimedOut ? "timeout" : (bResult && iExitCode == 0 ? "success" : "failed");
	memset(&tState, 0, sizeof(tState));
	if ( Sched_LoadTaskSnapshotById(pDB, pCtx->Task.id, &tState) ) {
		iNextRunningCount = tState.runningCount > 0 ? (tState.runningCount - 1) : 0;
		if (
			!Sched_TextEquals(pCtx->Task.sScheduleType, "once") &&
			Sched_IsOverlapPolicy(&tState, "queue_one") &&
			tState.pendingRun &&
			iNextRunningCount == 0 &&
			tState.enabled
		) {
			bStartPending = 1;
		}
		if ( !bStartPending && iNextRunningCount == 0 && Sched_TaskCanRetry(&tState, sFinalStatus) ) {
			bScheduleRetry = 1;
			iNextRetryState = tState.retryState + 1;
			iRetryNextRun = iFinish + (tState.retryDelaySec > 0 ? tState.retryDelaySec : 1);
		}
	}
	sTaskStatus = bScheduleRetry ? "retrying" : sFinalStatus;

	if ( iRunLogId > 0 ) {
		sqlite3_prepare_v3(pDB, "UPDATE sched_run_log SET finishTime = ?, durationMs = ?, status = ?, exitCode = ?, stdoutText = ?, stderrText = ?, message = ? WHERE id = ?", -1, 0, &stmt, NULL);
		if ( stmt ) {
			sqlite3_bind_int64(stmt, 1, iFinish);
			sqlite3_bind_int64(stmt, 2, iDuration);
			sqlite3_bind_text(stmt, 3, sFinalStatus, -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 4, iExitCode);
			sqlite3_bind_text(stmt, 5, Sched_CStrOrEmpty((const char*)sStdout), -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 6, Sched_CStrOrEmpty((const char*)sStderr), -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 7, Sched_CStrOrEmpty((const char*)sMessage), -1, SQLITE_STATIC);
			sqlite3_bind_int64(stmt, 8, iRunLogId);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}

	if ( bScheduleRetry ) {
		str sRetryMessage = xrtFormat("Retry %d/%d scheduled after %d second(s)", iNextRetryState, tState.retryCount, tState.retryDelaySec > 0 ? tState.retryDelaySec : 1);
		sqlite3_prepare_v3(pDB, "UPDATE sched_task SET isRunning = 0, runningCount = 0, pendingRun = 0, nextRunAt = ?, retryState = ?, lastRunAt = ?, lastFinishAt = ?, lastStatus = 'retrying', lastMessage = ?, lastExitCode = ?, updateTime = ? WHERE id = ?", -1, 0, &stmt, NULL);
		if ( stmt ) {
			sqlite3_bind_int64(stmt, 1, iRetryNextRun);
			sqlite3_bind_int(stmt, 2, iNextRetryState);
			sqlite3_bind_int64(stmt, 3, iStart);
			sqlite3_bind_int64(stmt, 4, iFinish);
			sqlite3_bind_text(stmt, 5, Sched_CStrOrEmpty((const char*)sRetryMessage), -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 6, iExitCode);
			sqlite3_bind_int64(stmt, 7, iFinish);
			sqlite3_bind_int64(stmt, 8, pCtx->Task.id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		if ( sRetryMessage ) xrtFree(sRetryMessage);
	} else if ( Sched_TextEquals(pCtx->Task.sScheduleType, "once") ) {
		sqlite3_prepare_v3(pDB, "UPDATE sched_task SET enabled = 0, isRunning = 0, runningCount = 0, pendingRun = 0, nextRunAt = 0, retryState = 0, lastRunAt = ?, lastFinishAt = ?, lastStatus = ?, lastMessage = ?, lastExitCode = ?, updateTime = ? WHERE id = ?", -1, 0, &stmt, NULL);
		if ( stmt ) {
			sqlite3_bind_int64(stmt, 1, iStart);
			sqlite3_bind_int64(stmt, 2, iFinish);
			sqlite3_bind_text(stmt, 3, sTaskStatus, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 4, Sched_CStrOrEmpty((const char*)sMessage), -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 5, iExitCode);
			sqlite3_bind_int64(stmt, 6, iFinish);
			sqlite3_bind_int64(stmt, 7, pCtx->Task.id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	} else if ( Sched_IsOverlapPolicy(&tState, "parallel") ) {
		sqlite3_prepare_v3(pDB, "UPDATE sched_task SET isRunning = CASE WHEN ? > 0 THEN 1 ELSE 0 END, runningCount = ?, retryState = 0, lastRunAt = ?, lastFinishAt = ?, lastStatus = ?, lastMessage = ?, lastExitCode = ?, updateTime = ? WHERE id = ?", -1, 0, &stmt, NULL);
		if ( stmt ) {
			sqlite3_bind_int(stmt, 1, iNextRunningCount);
			sqlite3_bind_int(stmt, 2, iNextRunningCount);
			sqlite3_bind_int64(stmt, 3, iStart);
			sqlite3_bind_int64(stmt, 4, iFinish);
			sqlite3_bind_text(stmt, 5, sTaskStatus, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 6, Sched_CStrOrEmpty((const char*)sMessage), -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 7, iExitCode);
			sqlite3_bind_int64(stmt, 8, iFinish);
			sqlite3_bind_int64(stmt, 9, pCtx->Task.id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	} else if ( bStartPending ) {
		sqlite3_prepare_v3(pDB, "UPDATE sched_task SET isRunning = 0, runningCount = 0, pendingRun = 0, retryState = 0, lastRunAt = ?, lastFinishAt = ?, lastStatus = ?, lastMessage = ?, lastExitCode = ?, updateTime = ? WHERE id = ?", -1, 0, &stmt, NULL);
		if ( stmt ) {
			sqlite3_bind_int64(stmt, 1, iStart);
			sqlite3_bind_int64(stmt, 2, iFinish);
			sqlite3_bind_text(stmt, 3, sTaskStatus, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 4, Sched_CStrOrEmpty((const char*)sMessage), -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 5, iExitCode);
			sqlite3_bind_int64(stmt, 6, iFinish);
			sqlite3_bind_int64(stmt, 7, pCtx->Task.id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	} else {
		sqlite3_prepare_v3(pDB, "UPDATE sched_task SET isRunning = CASE WHEN ? > 0 THEN 1 ELSE 0 END, runningCount = ?, nextRunAt = CASE WHEN enabled = 0 THEN 0 ELSE ? END, retryState = 0, lastRunAt = ?, lastFinishAt = ?, lastStatus = ?, lastMessage = ?, lastExitCode = ?, updateTime = ? WHERE id = ?", -1, 0, &stmt, NULL);
		if ( stmt ) {
			sqlite3_bind_int(stmt, 1, iNextRunningCount);
			sqlite3_bind_int(stmt, 2, iNextRunningCount);
			sqlite3_bind_int64(stmt, 3, iNextRun);
			sqlite3_bind_int64(stmt, 4, iStart);
			sqlite3_bind_int64(stmt, 5, iFinish);
			sqlite3_bind_text(stmt, 6, sTaskStatus, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 7, Sched_CStrOrEmpty((const char*)sMessage), -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 8, iExitCode);
			sqlite3_bind_int64(stmt, 9, iFinish);
			sqlite3_bind_int64(stmt, 10, pCtx->Task.id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}

	if ( bStartPending ) {
		Sched_StartTaskRunInternal(pCtx->Task.id, "queued", FALSE, NULL);
	}

	if ( sStdout ) xrtFree(sStdout);
	if ( sStderr ) xrtFree(sStderr);
	if ( sMessage ) xrtFree(sMessage);
	Sched_FreeTaskSnapshot(&tState);
	sqlite3_close(pDB);
	Sched_FreeWorkerContext(pCtx);
	if ( G_SchedLock && G_SchedCond ) {
		xrtMutexLock(G_SchedLock);
		if ( G_SchedWorkerCount > 0 ) {
			G_SchedWorkerCount--;
		}
		xrtCondBroadcast(G_SchedCond);
		xrtMutexUnlock(G_SchedLock);
	}
	Sched_NotifyChanged();
	return 0;
}

static bool Sched_StartTaskRunInternal(int64 id, const char* sTriggerSource, bool bAllowDisabled, str* psMessage)
{
	sqlite3* pDB = Sched_OpenStandaloneDB();
	SchedTaskSnapshot tTask;
	SchedWorkerContext* pCtx = NULL;
	xthread hThread = NULL;
	sqlite3_stmt* stmt = NULL;

	memset(&tTask, 0, sizeof(tTask));
	if ( pDB == NULL ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Failed to open scheduler db", 0);
		return FALSE;
	}
	if ( !Sched_LoadTaskSnapshotById(pDB, id, &tTask) ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Task not found", 0);
		sqlite3_close(pDB);
		return FALSE;
	}
	if ( !bAllowDisabled && !tTask.enabled ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Task is disabled", 0);
		Sched_FreeTaskSnapshot(&tTask);
		sqlite3_close(pDB);
		return FALSE;
	}
	if ( tTask.runningCount > 0 && Sched_IsOverlapPolicy(&tTask, "skip") ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Task is already running", 0);
		Sched_FreeTaskSnapshot(&tTask);
		sqlite3_close(pDB);
		return FALSE;
	}
	if ( Sched_IsOverlapPolicy(&tTask, "parallel") && tTask.parallelLimit > 0 && tTask.runningCount >= tTask.parallelLimit ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Parallel limit reached", 0);
		Sched_FreeTaskSnapshot(&tTask);
		sqlite3_close(pDB);
		return FALSE;
	}
	if ( tTask.runningCount > 0 && Sched_IsOverlapPolicy(&tTask, "queue_one") ) {
		sqlite3_prepare_v3(pDB, "UPDATE sched_task SET pendingRun = 1, lastStatus = 'queued', lastMessage = ?, updateTime = ? WHERE id = ? AND isDelete = 0", -1, 0, &stmt, NULL);
		if ( stmt ) {
			sqlite3_bind_text(stmt, 1, tTask.pendingRun ? "Pending run already queued" : "Queued to run once after current execution", -1, SQLITE_STATIC);
			sqlite3_bind_int64(stmt, 2, xrtNow());
			sqlite3_bind_int64(stmt, 3, id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		Sched_FreeTaskSnapshot(&tTask);
		sqlite3_close(pDB);
		if ( psMessage ) *psMessage = xrtCopyStr("Task queued to run after current execution", 0);
		return TRUE;
	}

	sqlite3_prepare_v3(pDB, "UPDATE sched_task SET runningCount = CASE WHEN runningCount < 0 THEN 1 ELSE runningCount + 1 END, isRunning = 1, lastStatus = 'queued', lastMessage = ?, updateTime = ? WHERE id = ? AND isDelete = 0", -1, 0, &stmt, NULL);
	if ( stmt ) {
		sqlite3_bind_text(stmt, 1, sTriggerSource ? sTriggerSource : "scheduler", -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt, 2, xrtNow());
		sqlite3_bind_int64(stmt, 3, id);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	pCtx = (SchedWorkerContext*)xrtMalloc(sizeof(SchedWorkerContext));
	memset(pCtx, 0, sizeof(*pCtx));
	pCtx->Task = tTask;
	pCtx->sTriggerSource = xrtCopyStr(sTriggerSource ? (str)sTriggerSource : (str)"scheduler", 0);

	if ( G_SchedLock ) {
		xrtMutexLock(G_SchedLock);
		G_SchedWorkerCount++;
		xrtMutexUnlock(G_SchedLock);
	}

	hThread = xrtThreadCreate((ptr)Sched_WorkerProc, pCtx, 0);
	if ( hThread == NULL ) {
		sqlite3_prepare_v3(pDB, "UPDATE sched_task SET runningCount = CASE WHEN runningCount > 0 THEN runningCount - 1 ELSE 0 END, isRunning = CASE WHEN runningCount > 1 THEN 1 ELSE 0 END, lastStatus = 'failed', lastMessage = 'Failed to start task thread', updateTime = ? WHERE id = ? AND isDelete = 0", -1, 0, &stmt, NULL);
		if ( stmt ) {
			sqlite3_bind_int64(stmt, 1, xrtNow());
			sqlite3_bind_int64(stmt, 2, id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
			stmt = NULL;
		}
		Sched_FreeWorkerContext(pCtx);
		if ( G_SchedLock ) {
			xrtMutexLock(G_SchedLock);
			if ( G_SchedWorkerCount > 0 ) {
				G_SchedWorkerCount--;
			}
			xrtCondBroadcast(G_SchedCond);
			xrtMutexUnlock(G_SchedLock);
		}
		sqlite3_close(pDB);
		if ( psMessage ) *psMessage = xrtCopyStr("Failed to start task thread", 0);
		return FALSE;
	}

	hThread->bAutoDestroy = 1;
	sqlite3_close(pDB);
	if ( psMessage ) *psMessage = xrtCopyStr("Task queued", 0);
	return TRUE;
}

bool Sched_RunNow(int64 id, str* psMessage)
{
	return Sched_StartTaskRunInternal(id, "manual", TRUE, psMessage);
}

static bool Sched_ProcessDueTask(sqlite3* pDB, const SchedTaskSnapshot* pTask, int64 iNow, str* psMessage)
{
	sqlite3_stmt* stmt = NULL;
	int64 iNextRunAt;
	const char* sMessage = NULL;
	bool bShouldStart = FALSE;

	if ( pDB == NULL || pTask == NULL || pTask->id <= 0 ) {
		return FALSE;
	}

	iNextRunAt = Sched_CalcBusyNext(pTask, iNow);
	if ( pTask->runningCount > 0 && Sched_IsOverlapPolicy(pTask, "skip") ) {
		sMessage = "Skipped due run while task was already running";
	} else if ( pTask->runningCount > 0 && Sched_IsOverlapPolicy(pTask, "queue_one") ) {
		sMessage = pTask->pendingRun ? "Pending run already queued" : "Queued one pending run";
	} else if ( Sched_IsOverlapPolicy(pTask, "parallel") && pTask->parallelLimit > 0 && pTask->runningCount >= pTask->parallelLimit ) {
		sMessage = "Skipped due run because parallel limit was reached";
	} else {
		sMessage = "Scheduler reserved next run";
		bShouldStart = TRUE;
	}

	if ( pTask->runningCount > 0 && Sched_IsOverlapPolicy(pTask, "queue_one") ) {
		if ( sqlite3_prepare_v3(pDB, "UPDATE sched_task SET nextRunAt = ?, pendingRun = 1, lastStatus = 'queued', lastMessage = ?, updateTime = ? WHERE id = ? AND isDelete = 0", -1, 0, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmt, 1, iNextRunAt);
			sqlite3_bind_text(stmt, 2, sMessage, -1, SQLITE_STATIC);
			sqlite3_bind_int64(stmt, 3, iNow);
			sqlite3_bind_int64(stmt, 4, pTask->id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		if ( psMessage ) *psMessage = xrtCopyStr((str)sMessage, 0);
		return TRUE;
	}

	if ( sqlite3_prepare_v3(pDB, "UPDATE sched_task SET nextRunAt = ?, lastStatus = ?, lastMessage = ?, updateTime = ? WHERE id = ? AND isDelete = 0", -1, 0, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}
	sqlite3_bind_int64(stmt, 1, iNextRunAt);
	sqlite3_bind_text(stmt, 2, bShouldStart ? "queued" : "skipped", -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 3, sMessage, -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt, 4, iNow);
	sqlite3_bind_int64(stmt, 5, pTask->id);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if ( psMessage ) *psMessage = xrtCopyStr((str)sMessage, 0);
	return bShouldStart;
}

static bool Sched_FetchDueTask(sqlite3* pDB, int64 iNow, SchedTaskSnapshot* pTask)
{
	sqlite3_stmt* stmt = NULL;
	int64 id = 0;

	if ( sqlite3_prepare_v3(pDB, "SELECT id FROM sched_task WHERE isDelete = 0 AND enabled = 1 AND nextRunAt > 0 AND nextRunAt <= ? ORDER BY nextRunAt ASC, id ASC LIMIT 1", -1, 0, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}
	sqlite3_bind_int64(stmt, 1, iNow);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		id = sqlite3_column_int64(stmt, 0);
	}
	sqlite3_finalize(stmt);
	if ( id <= 0 ) {
		return FALSE;
	}
	return Sched_LoadTaskSnapshotById(pDB, id, pTask);
}

static int64 Sched_QueryNextWakeTime(sqlite3* pDB)
{
	sqlite3_stmt* stmt = NULL;
	int64 iNext = 0;

	if ( sqlite3_prepare_v3(pDB, "SELECT MIN(nextRunAt) FROM sched_task WHERE isDelete = 0 AND enabled = 1 AND nextRunAt > 0", -1, 0, &stmt, NULL) != SQLITE_OK ) {
		return 0;
	}
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iNext = sqlite3_column_int64(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return iNext;
}

static uint32 Sched_ThreadProc(ptr pParam)
{
	sqlite3* pDB = Sched_OpenStandaloneDB();
	(void)pParam;

	if ( pDB == NULL ) {
		return 1;
	}

	while ( TRUE ) {
		SchedTaskSnapshot tTask;
		int64 iNow;
		int64 iNextWake;
		bool bShouldStart = FALSE;

		xrtMutexLock(G_SchedLock);
		if ( G_SchedStop ) {
			xrtMutexUnlock(G_SchedLock);
			break;
		}
		xrtMutexUnlock(G_SchedLock);

		iNow = xrtNow();
		memset(&tTask, 0, sizeof(tTask));
		if ( Sched_FetchDueTask(pDB, iNow, &tTask) ) {
			bShouldStart = Sched_ProcessDueTask(pDB, &tTask, iNow, NULL);
			if ( bShouldStart ) {
				Sched_StartTaskRunInternal(tTask.id, "scheduler", FALSE, NULL);
			}
			Sched_FreeTaskSnapshot(&tTask);
			continue;
		}

		iNextWake = Sched_QueryNextWakeTime(pDB);
		xrtMutexLock(G_SchedLock);
		if ( G_SchedStop ) {
			xrtMutexUnlock(G_SchedLock);
			break;
		}
		if ( iNextWake <= 0 ) {
			xrtCondWait(G_SchedCond, G_SchedLock);
		} else {
			int64 iWaitSeconds = iNextWake - xrtNow();
			uint32 iWaitMs;
			if ( iWaitSeconds < 0 ) iWaitSeconds = 0;
			iWaitMs = (uint32)(iWaitSeconds > 86400 ? 86400000 : iWaitSeconds * 1000);
			if ( iWaitMs < 50 ) iWaitMs = 50;
			xrtCondWaitTimeout(G_SchedCond, G_SchedLock, iWaitMs);
		}
		xrtMutexUnlock(G_SchedLock);
	}

	sqlite3_close(pDB);
	return 0;
}

static void Sched_ApplyMisfirePolicies(sqlite3* pDB)
{
	sqlite3_stmt* stmt = NULL;
	int64 iNow = xrtNow();

	if ( pDB == NULL ) {
		return;
	}
	if ( sqlite3_prepare_v3(pDB, "SELECT id FROM sched_task WHERE isDelete = 0 AND enabled = 1 AND nextRunAt > 0 AND nextRunAt < ?", -1, 0, &stmt, NULL) != SQLITE_OK ) {
		return;
	}
	sqlite3_bind_int64(stmt, 1, iNow);
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		SchedTaskSnapshot tTask;
		sqlite3_stmt* stmtUpdate = NULL;
		int64 id = sqlite3_column_int64(stmt, 0);
		int64 iNextRunAt = 0;

		memset(&tTask, 0, sizeof(tTask));
		if ( !Sched_LoadTaskSnapshotById(pDB, id, &tTask) ) {
			continue;
		}
		if ( Sched_IsMisfirePolicy(&tTask, "fire_once_now") ) {
			Sched_FreeTaskSnapshot(&tTask);
			continue;
		}

		if ( Sched_TextEquals(tTask.sScheduleType, "once") ) {
			if ( sqlite3_prepare_v3(pDB, "UPDATE sched_task SET enabled = 0, nextRunAt = 0, pendingRun = 0, lastStatus = 'missed', lastMessage = 'Missed once task skipped while scheduler was offline', updateTime = ? WHERE id = ?", -1, 0, &stmtUpdate, NULL) == SQLITE_OK ) {
				sqlite3_bind_int64(stmtUpdate, 1, iNow);
				sqlite3_bind_int64(stmtUpdate, 2, tTask.id);
				sqlite3_step(stmtUpdate);
				sqlite3_finalize(stmtUpdate);
			}
			Sched_FreeTaskSnapshot(&tTask);
			continue;
		}

		iNextRunAt = Sched_CalcBusyNext(&tTask, iNow);
		if ( sqlite3_prepare_v3(pDB, "UPDATE sched_task SET nextRunAt = ?, pendingRun = 0, lastStatus = 'missed', lastMessage = 'Missed schedule skipped while scheduler was offline', updateTime = ? WHERE id = ?", -1, 0, &stmtUpdate, NULL) == SQLITE_OK ) {
			sqlite3_bind_int64(stmtUpdate, 1, iNextRunAt);
			sqlite3_bind_int64(stmtUpdate, 2, iNow);
			sqlite3_bind_int64(stmtUpdate, 3, tTask.id);
			sqlite3_step(stmtUpdate);
			sqlite3_finalize(stmtUpdate);
		}
		Sched_FreeTaskSnapshot(&tTask);
	}
	sqlite3_finalize(stmt);
}

static void Sched_EnsureSchema(void)
{
	Sched_ExecSQL(G_DB, "PRAGMA journal_mode=WAL;");
	Sched_ExecSQL(G_DB, "CREATE TABLE IF NOT EXISTS sched_task (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, enabled INTEGER DEFAULT 1, scheduleType TEXT NOT NULL, execType TEXT NOT NULL, shellType TEXT DEFAULT '', codeText TEXT DEFAULT '', customText TEXT DEFAULT '', cronExpr TEXT DEFAULT '', onceAt INTEGER DEFAULT 0, intervalValue INTEGER DEFAULT 0, intervalUnit TEXT DEFAULT '', startAt INTEGER DEFAULT 0, nextRunAt INTEGER DEFAULT 0, lastRunAt INTEGER DEFAULT 0, lastFinishAt INTEGER DEFAULT 0, timeoutSec INTEGER DEFAULT 300, overlapPolicy TEXT DEFAULT 'skip', misfirePolicy TEXT DEFAULT 'skip', workDir TEXT DEFAULT '', isRunning INTEGER DEFAULT 0, runningCount INTEGER DEFAULT 0, pendingRun INTEGER DEFAULT 0, parallelLimit INTEGER DEFAULT 0, retryCount INTEGER DEFAULT 0, retryDelaySec INTEGER DEFAULT 60, retryState INTEGER DEFAULT 0, lastStatus TEXT DEFAULT '', lastMessage TEXT DEFAULT '', lastExitCode INTEGER DEFAULT 0, createTime INTEGER DEFAULT 0, updateTime INTEGER DEFAULT 0, isDelete INTEGER DEFAULT 0)");
	Sched_ExecSQL(G_DB, "CREATE TABLE IF NOT EXISTS sched_run_log (id INTEGER PRIMARY KEY AUTOINCREMENT, taskId INTEGER DEFAULT 0, taskName TEXT DEFAULT '', triggerSource TEXT DEFAULT '', startTime INTEGER DEFAULT 0, finishTime INTEGER DEFAULT 0, durationMs INTEGER DEFAULT 0, status TEXT DEFAULT '', exitCode INTEGER DEFAULT 0, stdoutText TEXT DEFAULT '', stderrText TEXT DEFAULT '', message TEXT DEFAULT '')");
	Sched_ExecSQL(G_DB, "CREATE INDEX IF NOT EXISTS idx_sched_task_next ON sched_task (isDelete, enabled, nextRunAt)");
	Sched_ExecSQL(G_DB, "CREATE INDEX IF NOT EXISTS idx_sched_log_task ON sched_run_log (taskId, id)");
	Sched_EnsureColumn(G_DB, "sched_task", "customText", "TEXT DEFAULT ''");
	Sched_EnsureColumn(G_DB, "sched_task", "misfirePolicy", "TEXT DEFAULT 'skip'");
	Sched_EnsureColumn(G_DB, "sched_task", "runningCount", "INTEGER DEFAULT 0");
	Sched_EnsureColumn(G_DB, "sched_task", "pendingRun", "INTEGER DEFAULT 0");
	Sched_EnsureColumn(G_DB, "sched_task", "parallelLimit", "INTEGER DEFAULT 0");
	Sched_EnsureColumn(G_DB, "sched_task", "retryCount", "INTEGER DEFAULT 0");
	Sched_EnsureColumn(G_DB, "sched_task", "retryDelaySec", "INTEGER DEFAULT 60");
	Sched_EnsureColumn(G_DB, "sched_task", "retryState", "INTEGER DEFAULT 0");
	Sched_ExecSQL(G_DB, "UPDATE sched_task SET overlapPolicy = 'skip' WHERE overlapPolicy IS NULL OR overlapPolicy = ''");
	Sched_ExecSQL(G_DB, "UPDATE sched_task SET misfirePolicy = 'skip' WHERE misfirePolicy IS NULL OR misfirePolicy = ''");
	Sched_ExecSQL(G_DB, "UPDATE sched_task SET parallelLimit = 0 WHERE parallelLimit IS NULL OR parallelLimit < 0");
	Sched_ExecSQL(G_DB, "UPDATE sched_task SET retryCount = 0 WHERE retryCount IS NULL OR retryCount < 0");
	Sched_ExecSQL(G_DB, "UPDATE sched_task SET retryDelaySec = 60 WHERE retryDelaySec IS NULL OR retryDelaySec < 0");
	Sched_ExecSQL(G_DB, "UPDATE sched_task SET retryState = 0 WHERE retryState IS NULL OR retryState < 0");
	Sched_ExecSQL(G_DB, "UPDATE sched_task SET runningCount = 0, pendingRun = 0, isRunning = 0, retryState = 0 WHERE runningCount <> 0 OR pendingRun <> 0 OR isRunning <> 0 OR retryState <> 0");
}

void Sched_Init()
{
	printf("        Sched_Init \n");
	Sched_EnsureSchema();

	G_SchedPath = xrtPathJoin(3, AppPath, "data", "temp");
	G_SchedCachePath = xrtPathJoin(4, AppPath, "data", "temp", "sched");
	G_SchedXSPath = xrtPathJoin(2, ExePath, "xs.exe");
	xrtDirCreateAll(G_SchedPath);
	xrtDirCreateAll(G_SchedCachePath);
	Sched_ApplyMisfirePolicies(G_DB);

	G_SchedLock = xrtMutexCreate();
	G_SchedCond = xrtCondCreate();
	G_SchedStop = FALSE;
	G_SchedThread = xrtThreadCreate((ptr)Sched_ThreadProc, NULL, 0);
}

void Sched_Unit()
{
	printf("        Sched_Unit \n");

	if ( G_SchedLock && G_SchedCond ) {
		xrtMutexLock(G_SchedLock);
		G_SchedStop = TRUE;
		xrtCondBroadcast(G_SchedCond);
		xrtMutexUnlock(G_SchedLock);
	}

	if ( G_SchedThread ) {
		xrtThreadWait(G_SchedThread);
		xrtThreadDestroy(G_SchedThread);
		G_SchedThread = NULL;
	}
	if ( G_SchedLock && G_SchedCond ) {
		xrtMutexLock(G_SchedLock);
		while ( G_SchedWorkerCount > 0 ) {
			xrtCondWaitTimeout(G_SchedCond, G_SchedLock, 100);
		}
		xrtMutexUnlock(G_SchedLock);
	}
	if ( G_SchedCond ) {
		xrtCondDestroy(G_SchedCond);
		G_SchedCond = NULL;
	}
	if ( G_SchedLock ) {
		xrtMutexDestroy(G_SchedLock);
		G_SchedLock = NULL;
	}
	if ( G_SchedPath ) {
		xrtFree(G_SchedPath);
		G_SchedPath = NULL;
	}
	if ( G_SchedCachePath ) {
		xrtFree(G_SchedCachePath);
		G_SchedCachePath = NULL;
	}
	if ( G_SchedXSPath ) {
		xrtFree(G_SchedXSPath);
		G_SchedXSPath = NULL;
	}
	G_SchedWorkerCount = 0;
}
