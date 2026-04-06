// member message module

static const char* MemberMessage_StrOrEmpty(const char* sText)
{
	return sText ? sText : "";
}

static bool MemberMessage_TextEquals(const char* sLeft, const char* sRight)
{
	if ( sLeft == NULL || sRight == NULL ) {
		return FALSE;
	}
	return strcmp(sLeft, sRight) == 0;
}

static bool MemberMessage_ExecSQLIgnore(const char* sSQL)
{
	if ( sSQL == NULL || sSQL[0] == '\0' ) {
		return FALSE;
	}
	sqlite3_exec(G_DB, sSQL, NULL, NULL, NULL);
	return TRUE;
}

static bool MemberMessage_GetGlobalBool(const char* sName, bool bDefaultValue)
{
	xvalue tblGlobal;

	if ( (G_Option == NULL) || (xvoType(G_Option) != XVO_DT_TABLE) ) {
		return bDefaultValue;
	}

	tblGlobal = xvoTableGetValue(G_Option, "global", 6);
	if ( (tblGlobal == NULL) || (xvoType(tblGlobal) != XVO_DT_TABLE) ) {
		return bDefaultValue;
	}

	if ( xvoTableItemType(tblGlobal, sName, 0) == XVO_DT_NULL ) {
		return bDefaultValue;
	}

	return xvoTableGetBool(tblGlobal, sName, 0);
}

static int64 MemberMessage_GetGlobalInt(const char* sName, int64 iDefaultValue)
{
	xvalue tblGlobal;

	if ( (G_Option == NULL) || (xvoType(G_Option) != XVO_DT_TABLE) ) {
		return iDefaultValue;
	}

	tblGlobal = xvoTableGetValue(G_Option, "global", 6);
	if ( (tblGlobal == NULL) || (xvoType(tblGlobal) != XVO_DT_TABLE) ) {
		return iDefaultValue;
	}

	if ( xvoTableItemType(tblGlobal, sName, 0) == XVO_DT_NULL ) {
		return iDefaultValue;
	}

	return xvoTableGetInt(tblGlobal, sName, 0);
}

static bool MemberMessage_ConfigHasValue(xvalue tblData, const char* sName)
{
	if ( tblData == NULL || xvoType(tblData) != XVO_DT_TABLE || sName == NULL || sName[0] == '\0' ) {
		return FALSE;
	}
	return xvoTableItemType(tblData, sName, 0) != XVO_DT_NULL;
}

static bool MemberMessage_GetConfigBool(xvalue tblData, const char* sName, bool bDefaultValue)
{
	if ( MemberMessage_ConfigHasValue(tblData, sName) ) {
		return xvoTableGetBool(tblData, sName, 0);
	}
	return MemberMessage_GetGlobalBool(sName, bDefaultValue);
}

static int64 MemberMessage_GetConfigInt(xvalue tblData, const char* sName, int64 iDefaultValue)
{
	if ( MemberMessage_ConfigHasValue(tblData, sName) ) {
		return xvoTableGetInt(tblData, sName, 0);
	}
	return MemberMessage_GetGlobalInt(sName, iDefaultValue);
}

static str MemberMessage_GetConfigText(xvalue tblData, const char* sName, const char* sDefaultValue)
{
	if ( MemberMessage_ConfigHasValue(tblData, sName) ) {
		return xvoTableGetText(tblData, sName, 0);
	}
	return Option_GetGlobalText(sName, sDefaultValue);
}

static int64 G_MemberMessage_MailQueueLastRun = 0;
static xmutex G_MemberMessage_MailQueueLock = NULL;
static bool G_MemberMessage_MailQueueRunning = FALSE;
static bool G_MemberMessage_MailQueueForceRun = FALSE;
static int64 G_MemberMessage_MailQueueLastLimit = 0;
static int64 G_MemberMessage_MailQueueLastConcurrency = 0;
static int64 G_MemberMessage_MailQueueLastSuccess = 0;
static int64 G_MemberMessage_MailQueueLastFail = 0;
static int64 G_MemberMessage_MailQueueLastRunTime = 0;

typedef struct {
	int64 iTaskID;
	int64 iMemberID;
	int64 iRetryCount;
	int64 iMaxRetryCount;
	bool bClaimed;
	str sToEmail;
	str sSubject;
	str sHtmlBody;
	str sTextBody;
	str sFinalSubject;
	xsmtpaddr tTo;
	xsmtpmessage tMsg;
	xfuture* pFuture;
} member_message_mail_job;

static str MemberMessage_BuildMailSubject(const char* sSubject);
static bool MemberMessage_UpdateMailTaskResult(sqlite3* pDB, int64 iTaskID, const char* sStatus, int64 iRetryCount, int64 iNextRetryAt, int64 iSendTime, const char* sErrorMessage);
static int64 MemberMessage_RecoverStaleSendingTasks(sqlite3* pDB, int64 iNow);
static int64 MemberMessage_GetMailQueueScanInterval(void);
static int64 MemberMessage_GetMailQueueBatchLimit(int64 iIntervalSec);
static bool MemberMessage_RetryMailTask(int64 iTaskID, str* psMessage);
static bool MemberMessage_DeleteMailTask(int64 iTaskID, str* psMessage);

static int MemberMessage_ParseSmtpSecureMode(const char* sMode)
{
	if ( sMode == NULL || sMode[0] == '\0' ) {
		return XSMTP_SECURE_AUTO;
	}
	if ( strcmp(sMode, "none") == 0 ) {
		return XSMTP_SECURE_NONE;
	}
	if ( strcmp(sMode, "ssl") == 0 || strcmp(sMode, "tls") == 0 ) {
		return XSMTP_SECURE_SSL;
	}
	if ( strcmp(sMode, "starttls") == 0 ) {
		return XSMTP_SECURE_STARTTLS;
	}
	return XSMTP_SECURE_AUTO;
}

static bool MemberMessage_EmailLooksValid(const char* sEmail)
{
	const char* sAt;
	const char* sDot;

	if ( sEmail == NULL || sEmail[0] == '\0' ) {
		return FALSE;
	}

	sAt = strchr(sEmail, '@');
	if ( sAt == NULL || sAt == sEmail || sAt[1] == '\0' ) {
		return FALSE;
	}

	sDot = strrchr(sAt + 1, '.');
	if ( sDot == NULL || sDot == sAt + 1 || sDot[1] == '\0' ) {
		return FALSE;
	}

	return TRUE;
}

static bool MemberMessage_ShouldUseHtmlBody(const char* sHtmlBody, const char* sTextBody)
{
	const char* sHtml = MemberMessage_StrOrEmpty(sHtmlBody);
	const char* sText = MemberMessage_StrOrEmpty(sTextBody);

	if ( sHtml[0] == '\0' ) {
		return FALSE;
	}
	if ( sText[0] != '\0' && strcmp(sHtml, sText) == 0 ) {
		return FALSE;
	}
	if ( strchr(sHtml, '<') == NULL && strchr(sHtml, '&') == NULL ) {
		return FALSE;
	}
	return TRUE;
}

static bool MemberMessage_IsMailQueueRunning(void)
{
	bool bRunning = FALSE;

	if ( G_MemberMessage_MailQueueLock == NULL ) {
		return FALSE;
	}
	xrtMutexLock(G_MemberMessage_MailQueueLock);
	bRunning = G_MemberMessage_MailQueueRunning;
	xrtMutexUnlock(G_MemberMessage_MailQueueLock);
	return bRunning;
}

static void MemberMessage_RequestMailQueueRun(void)
{
	if ( G_MemberMessage_MailQueueLock == NULL ) {
		return;
	}
	xrtMutexLock(G_MemberMessage_MailQueueLock);
	G_MemberMessage_MailQueueForceRun = TRUE;
	xrtMutexUnlock(G_MemberMessage_MailQueueLock);
}

static bool MemberMessage_PeekMailQueueForceRun(void)
{
	bool bForce = FALSE;

	if ( G_MemberMessage_MailQueueLock == NULL ) {
		return FALSE;
	}
	xrtMutexLock(G_MemberMessage_MailQueueLock);
	bForce = G_MemberMessage_MailQueueForceRun;
	xrtMutexUnlock(G_MemberMessage_MailQueueLock);
	return bForce;
}

static bool MemberMessage_BeginMailQueueRun(bool* pbForceRun)
{
	bool bOK = FALSE;
	bool bForce = FALSE;

	if ( pbForceRun ) {
		*pbForceRun = FALSE;
	}
	if ( G_MemberMessage_MailQueueLock == NULL ) {
		return FALSE;
	}

	xrtMutexLock(G_MemberMessage_MailQueueLock);
	if ( !G_MemberMessage_MailQueueRunning ) {
		G_MemberMessage_MailQueueRunning = TRUE;
		bForce = G_MemberMessage_MailQueueForceRun;
		G_MemberMessage_MailQueueForceRun = FALSE;
		bOK = TRUE;
	}
	xrtMutexUnlock(G_MemberMessage_MailQueueLock);

	if ( pbForceRun ) {
		*pbForceRun = bForce;
	}
	return bOK;
}

static void MemberMessage_EndMailQueueRun(void)
{
	if ( G_MemberMessage_MailQueueLock == NULL ) {
		return;
	}
	xrtMutexLock(G_MemberMessage_MailQueueLock);
	G_MemberMessage_MailQueueRunning = FALSE;
	xrtMutexUnlock(G_MemberMessage_MailQueueLock);
}

static void MemberMessage_UpdateMailQueueStats(int64 iLimit, int64 iConcurrency, int64 iSuccess, int64 iFail, int64 iRunTime)
{
	if ( G_MemberMessage_MailQueueLock == NULL ) {
		return;
	}
	xrtMutexLock(G_MemberMessage_MailQueueLock);
	G_MemberMessage_MailQueueLastLimit = iLimit;
	G_MemberMessage_MailQueueLastConcurrency = iConcurrency;
	G_MemberMessage_MailQueueLastSuccess = iSuccess;
	G_MemberMessage_MailQueueLastFail = iFail;
	G_MemberMessage_MailQueueLastRunTime = iRunTime;
	xrtMutexUnlock(G_MemberMessage_MailQueueLock);
}

static xvalue MemberMessage_GetMailQueueStatus(void)
{
	xvalue tblStatus = xvoCreateTable();
	sqlite3_stmt* stmt = NULL;
	int64 iPendingCount = 0;
	int64 iIntervalSec = MemberMessage_GetMailQueueScanInterval();
	int64 iBatchLimit = MemberMessage_GetMailQueueBatchLimit(iIntervalSec);
	int64 iConcurrency = MemberMessage_GetGlobalInt("mail_queue_concurrency", 5);

	if ( iConcurrency < 1 ) iConcurrency = 1;
	if ( iConcurrency > iBatchLimit ) iConcurrency = iBatchLimit;
	if ( iConcurrency > 32 ) iConcurrency = 32;

	if ( sqlite3_prepare_v3(G_DB,
		"SELECT COUNT(*) FROM mail_task WHERE status = 'pending' AND nextRetryAt <= ?",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int64(stmt, 1, xrtNow());
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iPendingCount = sqlite3_column_int64(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}

	xvoTableSetBool(tblStatus, "enabled", 7, MemberMessage_GetGlobalBool("mail_enabled", FALSE));
	xvoTableSetBool(tblStatus, "queueEnabled", 12, MemberMessage_GetGlobalBool("mail_queue_enabled", TRUE));
	xvoTableSetInt(tblStatus, "scanIntervalSec", 15, iIntervalSec);
	xvoTableSetInt(tblStatus, "ratePerMinute", 13, MemberMessage_GetGlobalInt("mail_rate_limit_per_minute", 60));
	xvoTableSetInt(tblStatus, "batchLimit", 10, iBatchLimit);
	xvoTableSetInt(tblStatus, "concurrency", 11, iConcurrency);
	xvoTableSetInt(tblStatus, "pendingCount", 12, iPendingCount);

	if ( G_MemberMessage_MailQueueLock ) {
		xrtMutexLock(G_MemberMessage_MailQueueLock);
		xvoTableSetBool(tblStatus, "running", 7, G_MemberMessage_MailQueueRunning);
		xvoTableSetInt(tblStatus, "lastLimit", 9, G_MemberMessage_MailQueueLastLimit);
		xvoTableSetInt(tblStatus, "lastConcurrency", 15, G_MemberMessage_MailQueueLastConcurrency);
		xvoTableSetInt(tblStatus, "lastSuccess", 11, G_MemberMessage_MailQueueLastSuccess);
		xvoTableSetInt(tblStatus, "lastFail", 8, G_MemberMessage_MailQueueLastFail);
		xvoTableSetInt(tblStatus, "lastRunTime", 11, G_MemberMessage_MailQueueLastRunTime);
		xrtMutexUnlock(G_MemberMessage_MailQueueLock);
	} else {
		xvoTableSetBool(tblStatus, "running", 7, FALSE);
	}

	return tblStatus;
}

static void MemberMessage_MailJobInit(member_message_mail_job* pJob)
{
	if ( pJob == NULL ) {
		return;
	}
	memset(pJob, 0, sizeof(*pJob));
	xrtSmtpMessageInit(&pJob->tMsg);
}

static void MemberMessage_MailJobUnit(member_message_mail_job* pJob)
{
	if ( pJob == NULL ) {
		return;
	}
	if ( pJob->pFuture ) {
		xFutureRelease(pJob->pFuture);
		pJob->pFuture = NULL;
	}
	if ( pJob->sToEmail ) {
		xrtFree(pJob->sToEmail);
	}
	if ( pJob->sSubject ) {
		xrtFree(pJob->sSubject);
	}
	if ( pJob->sHtmlBody ) {
		xrtFree(pJob->sHtmlBody);
	}
	if ( pJob->sTextBody ) {
		xrtFree(pJob->sTextBody);
	}
	if ( pJob->sFinalSubject ) {
		xrtFree(pJob->sFinalSubject);
	}
	memset(pJob, 0, sizeof(*pJob));
}

static bool MemberMessage_ClaimMailTask(sqlite3* pDB, int64 iTaskID)
{
	sqlite3_stmt* stmt = NULL;
	bool bOK = FALSE;

	if ( pDB == NULL || iTaskID <= 0 ) {
		return FALSE;
	}
	if ( sqlite3_prepare_v3(pDB,
		"UPDATE mail_task SET status = 'sending', updateTime = ?, errorMessage = '' "
		"WHERE id = ? AND status = 'pending'",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}

	sqlite3_bind_int64(stmt, 1, xrtNow());
	sqlite3_bind_int64(stmt, 2, iTaskID);
	if ( sqlite3_step(stmt) == SQLITE_DONE ) {
		bOK = (sqlite3_changes(pDB) > 0);
	}
	sqlite3_finalize(stmt);
	return bOK;
}

static bool MemberMessage_PrepareMailJob(member_message_mail_job* pJob, xvalue tblTask)
{
	const char* sToEmail;
	const char* sSubject;
	const char* sHtmlBody;
	const char* sTextBody;

	if ( pJob == NULL || tblTask == NULL || xvoType(tblTask) != XVO_DT_TABLE ) {
		return FALSE;
	}

	MemberMessage_MailJobInit(pJob);
	pJob->iTaskID = xvoTableGetInt(tblTask, "id", 2);
	pJob->iMemberID = xvoTableGetInt(tblTask, "memberId", 8);
	pJob->iRetryCount = xvoTableGetInt(tblTask, "retryCount", 10);
	pJob->iMaxRetryCount = xvoTableGetInt(tblTask, "maxRetryCount", 13);

	sToEmail = xvoTableGetText(tblTask, "toEmail", 7);
	sSubject = xvoTableGetText(tblTask, "subject", 7);
	sHtmlBody = xvoTableGetText(tblTask, "htmlBody", 8);
	sTextBody = xvoTableGetText(tblTask, "textBody", 8);

	pJob->sToEmail = xrtCopyStr((str)MemberMessage_StrOrEmpty(sToEmail), 0);
	pJob->sSubject = xrtCopyStr((str)MemberMessage_StrOrEmpty(sSubject), 0);
	pJob->sHtmlBody = xrtCopyStr((str)MemberMessage_StrOrEmpty(sHtmlBody), 0);
	pJob->sTextBody = xrtCopyStr((str)MemberMessage_StrOrEmpty(sTextBody), 0);
	pJob->sFinalSubject = MemberMessage_BuildMailSubject(sSubject);
	if ( pJob->sToEmail == NULL || pJob->sSubject == NULL || pJob->sHtmlBody == NULL
		|| pJob->sTextBody == NULL || pJob->sFinalSubject == NULL ) {
		return FALSE;
	}

	pJob->tTo.sEmail = pJob->sToEmail;
	pJob->tMsg.arrTo = &pJob->tTo;
	pJob->tMsg.iToCount = 1;
	pJob->tMsg.sSubject = pJob->sFinalSubject;
	pJob->tMsg.sTextBody = pJob->sTextBody;
	pJob->tMsg.sHtmlBody = MemberMessage_ShouldUseHtmlBody(pJob->sHtmlBody, pJob->sTextBody) ? pJob->sHtmlBody : NULL;
	return TRUE;
}

static void MemberMessage_RecordMailJobResult(sqlite3* pDB, member_message_mail_job* pJob, const xsmtpresult* pRet,
	int64 iRetryInterval, int64* pSuccessCount, int64* pFailCount, str* psFirstError)
{
	xsmtpresult tFallbackRet;
	const xsmtpresult* pUseRet = pRet;
	bool bSent;

	if ( pJob == NULL || !pJob->bClaimed ) {
		return;
	}
	if ( pUseRet == NULL ) {
		xrtSmtpResultInit(&tFallbackRet);
		snprintf(tFallbackRet.sError, sizeof(tFallbackRet.sError), "%s", "SMTP async result is null");
		pUseRet = &tFallbackRet;
	}

	bSent = pUseRet->bSuccess;
	if ( bSent ) {
		MemberMessage_UpdateMailTaskResult(pDB, pJob->iTaskID, "success", pJob->iRetryCount, 0, xrtNow(), "");
		if ( pSuccessCount ) {
			(*pSuccessCount)++;
		}
	} else {
		bool bWillRetry = ((pJob->iRetryCount + 1) < pJob->iMaxRetryCount);
		int64 iNextRetryAt = bWillRetry ? (xrtNow() + iRetryInterval) : 0;
		if ( psFirstError && *psFirstError == NULL && pUseRet->sError[0] != '\0' ) {
			*psFirstError = xrtCopyStr((str)pUseRet->sError, 0);
		}
		MemberMessage_UpdateMailTaskResult(pDB, pJob->iTaskID, bWillRetry ? "pending" : "fail",
			pJob->iRetryCount + 1, iNextRetryAt, 0, pUseRet->sError);
		if ( pFailCount ) {
			(*pFailCount)++;
		}
	}
}

static void MemberMessage_ParseIDText(const char* sText, xvalue arrIDs)
{
	const char* p;
	int64 iValue = 0;
	bool bInNumber = FALSE;

	if ( sText == NULL || arrIDs == NULL || xvoType(arrIDs) != XVO_DT_ARRAY ) {
		return;
	}

	for ( p = sText; ; p++ ) {
		char c = *p;

		if ( c >= '0' && c <= '9' ) {
			iValue = iValue * 10 + (int64)(c - '0');
			bInNumber = TRUE;
			continue;
		}

		if ( bInNumber ) {
			if ( iValue > 0 && !XAdminIDArrayContainsInt(arrIDs, iValue) ) {
				xvoArrayAppendInt(arrIDs, iValue);
			}
			iValue = 0;
			bInNumber = FALSE;
		}

		if ( c == '\0' ) {
			break;
		}
	}
}

static void MemberMessage_AppendRecipient(xvalue arrRecipients, int64 iMemberID, const char* sUsername, const char* sNickname, const char* sEmail, int64 iGroupID)
{
	xvalue tblRow;
	uint32 i;
	uint32 iCount;

	if ( arrRecipients == NULL || xvoType(arrRecipients) != XVO_DT_ARRAY || iMemberID <= 0 ) {
		return;
	}

	iCount = xvoArrayItemCount(arrRecipients);
	for ( i = 0; i < iCount; i++ ) {
		xvalue tblItem = xvoArrayGetValue(arrRecipients, i);
		if ( tblItem && xvoTableGetInt(tblItem, "memberId", 8) == iMemberID ) {
			return;
		}
	}

	tblRow = xvoCreateTable();
	xvoTableSetInt(tblRow, "memberId", 8, iMemberID);
	xvoTableSetInt(tblRow, "groupId", 7, iGroupID);
	xvoTableSetText(tblRow, "username", 8, (str)MemberMessage_StrOrEmpty(sUsername), 0, FALSE);
	xvoTableSetText(tblRow, "nickname", 8, (str)MemberMessage_StrOrEmpty(sNickname), 0, FALSE);
	xvoTableSetText(tblRow, "email", 5, (str)MemberMessage_StrOrEmpty(sEmail), 0, FALSE);
	xvoArrayAppendValue(arrRecipients, tblRow, TRUE);
}

static void MemberMessage_CollectMembersByIDList(xvalue arrRecipients, xvalue arrIDs)
{
	sqlite3_stmt* stmt = NULL;
	uint32 i;
	uint32 iCount;

	if ( arrRecipients == NULL || arrIDs == NULL || xvoType(arrIDs) != XVO_DT_ARRAY ) {
		return;
	}

	if ( sqlite3_prepare_v3(G_DB,
		"SELECT id, username, nickname, email, groupId "
		"FROM member WHERE id = ? AND isDelete = 0 AND status = 1",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return;
	}

	iCount = xvoArrayItemCount(arrIDs);
	for ( i = 0; i < iCount; i++ ) {
		int64 iMemberID = xvoArrayGetInt(arrIDs, i);
		sqlite3_bind_int64(stmt, 1, iMemberID);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			MemberMessage_AppendRecipient(
				arrRecipients,
				sqlite3_column_int64(stmt, 0),
				(const char*)sqlite3_column_text(stmt, 1),
				(const char*)sqlite3_column_text(stmt, 2),
				(const char*)sqlite3_column_text(stmt, 3),
				sqlite3_column_int64(stmt, 4)
			);
		}
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
	}

	sqlite3_finalize(stmt);
}

static void MemberMessage_CollectMembersByGroupList(xvalue arrRecipients, xvalue arrIDs)
{
	sqlite3_stmt* stmt = NULL;
	uint32 i;
	uint32 iCount;

	if ( arrRecipients == NULL || arrIDs == NULL || xvoType(arrIDs) != XVO_DT_ARRAY ) {
		return;
	}

	if ( sqlite3_prepare_v3(G_DB,
		"SELECT id, username, nickname, email, groupId "
		"FROM member WHERE groupId = ? AND isDelete = 0 AND status = 1 ORDER BY id ASC",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return;
	}

	iCount = xvoArrayItemCount(arrIDs);
	for ( i = 0; i < iCount; i++ ) {
		int64 iGroupID = xvoArrayGetInt(arrIDs, i);
		sqlite3_bind_int64(stmt, 1, iGroupID);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			MemberMessage_AppendRecipient(
				arrRecipients,
				sqlite3_column_int64(stmt, 0),
				(const char*)sqlite3_column_text(stmt, 1),
				(const char*)sqlite3_column_text(stmt, 2),
				(const char*)sqlite3_column_text(stmt, 3),
				sqlite3_column_int64(stmt, 4)
			);
		}
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
	}

	sqlite3_finalize(stmt);
}

static void MemberMessage_CollectAllMembers(xvalue arrRecipients)
{
	sqlite3_stmt* stmt = NULL;

	if ( arrRecipients == NULL || xvoType(arrRecipients) != XVO_DT_ARRAY ) {
		return;
	}

	if ( sqlite3_prepare_v3(G_DB,
		"SELECT id, username, nickname, email, groupId "
		"FROM member WHERE isDelete = 0 AND status = 1 ORDER BY id ASC",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return;
	}

	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		MemberMessage_AppendRecipient(
			arrRecipients,
			sqlite3_column_int64(stmt, 0),
			(const char*)sqlite3_column_text(stmt, 1),
			(const char*)sqlite3_column_text(stmt, 2),
			(const char*)sqlite3_column_text(stmt, 3),
			sqlite3_column_int64(stmt, 4)
		);
	}

	sqlite3_finalize(stmt);
}

static xvalue MemberMessage_BuildRecipients(const char* sSendType, const char* sMemberIDs, const char* sGroupIDs)
{
	xvalue arrRecipients = xvoCreateArray();

	if ( arrRecipients == NULL ) {
		return NULL;
	}

	if ( MemberMessage_TextEquals(sSendType, "single") || MemberMessage_TextEquals(sSendType, "users") ) {
		xvalue arrIDs = xvoCreateArray();
		MemberMessage_ParseIDText(sMemberIDs, arrIDs);
		MemberMessage_CollectMembersByIDList(arrRecipients, arrIDs);
		xvoUnref(arrIDs);
		return arrRecipients;
	}

	if ( MemberMessage_TextEquals(sSendType, "groups") ) {
		xvalue arrIDs = xvoCreateArray();
		MemberMessage_ParseIDText(sGroupIDs, arrIDs);
		MemberMessage_CollectMembersByGroupList(arrRecipients, arrIDs);
		xvoUnref(arrIDs);
		return arrRecipients;
	}

	if ( MemberMessage_TextEquals(sSendType, "all") ) {
		MemberMessage_CollectAllMembers(arrRecipients);
		return arrRecipients;
	}

	return arrRecipients;
}

static bool MemberMessage_EnsureURIItem(const char* sURI, bool bNeedAuth)
{
	sqlite3_stmt* stmt = NULL;
	xtime iNow = xrtNow();
	int iRet;
	int64 iID = 0;

	if ( sURI == NULL || sURI[0] == '\0' ) {
		return FALSE;
	}

	iRet = sqlite3_prepare_v3(G_DB, "SELECT id FROM uris WHERE uri = ?", -1, SQL_PREPARE_DEFAULT, &stmt, NULL);
	if ( iRet != SQLITE_OK ) {
		return FALSE;
	}
	sqlite3_bind_text(stmt, 1, sURI, -1, SQLITE_TRANSIENT);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iID = sqlite3_column_int64(stmt, 0);
	}
	sqlite3_finalize(stmt);

	if ( iID > 0 ) {
		iRet = sqlite3_prepare_v3(G_DB,
			"UPDATE uris SET authID = 1, isBackend = 0, needAuth = ?, needLog = 0, keepActive = 0, updateTime = ? WHERE id = ?",
			-1, SQL_PREPARE_DEFAULT, &stmt, NULL);
		if ( iRet != SQLITE_OK ) {
			return FALSE;
		}
		sqlite3_bind_int(stmt, 1, bNeedAuth ? 1 : 0);
		sqlite3_bind_int64(stmt, 2, iNow);
		sqlite3_bind_int64(stmt, 3, iID);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		return TRUE;
	}

	iRet = sqlite3_prepare_v3(G_DB,
		"INSERT INTO uris (authID, uri, desc, isBackend, needAuth, needLog, keepActive, sort, createTime, updateTime) "
		"VALUES (1, ?, '', 0, ?, 0, 0, 0, ?, ?)",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL);
	if ( iRet != SQLITE_OK ) {
		return FALSE;
	}
	sqlite3_bind_text(stmt, 1, sURI, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, bNeedAuth ? 1 : 0);
	sqlite3_bind_int64(stmt, 3, iNow);
	sqlite3_bind_int64(stmt, 4, iNow);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return TRUE;
}

static void MemberMessage_EnsureMemberAPIRoutes()
{
	MemberMessage_EnsureURIItem("/api/v1/notify/list", TRUE);
	MemberMessage_EnsureURIItem("/api/v1/notify/unread_count", TRUE);
	MemberMessage_EnsureURIItem("/api/v1/notify/detail", TRUE);
	MemberMessage_EnsureURIItem("/api/v1/notify/read", TRUE);
	MemberMessage_EnsureURIItem("/api/v1/notify/read_all", TRUE);
	MemberMessage_EnsureURIItem("/api/v1/notify/delete", TRUE);
}

static int MemberMessage_FindMenuParentID()
{
	sqlite3_stmt* stmt = NULL;
	int iParentID = 0;

	if ( sqlite3_prepare_v3(G_DB,
		"SELECT parent FROM menu WHERE isDelete = 0 AND href = '/admin/view/member/user' ORDER BY id ASC LIMIT 1",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iParentID = sqlite3_column_int(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}

	return iParentID;
}

static void MemberMessage_EnsureMenuItem(const char* sTitle, const char* sIcon, const char* sHref, int iParentID, int iSort, const char* sRemark)
{
	sqlite3_stmt* stmt = NULL;
	xtime iNow = xrtNow();
	int64 iID = 0;

	if ( sHref == NULL || sHref[0] == '\0' || iParentID <= 0 ) {
		return;
	}

	if ( sqlite3_prepare_v3(G_DB, "SELECT id FROM menu WHERE isDelete = 0 AND href = ? ORDER BY id ASC LIMIT 1", -1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_text(stmt, 1, sHref, -1, SQLITE_TRANSIENT);
		if ( sqlite3_step(stmt) == SQLITE_ROW ) {
			iID = sqlite3_column_int64(stmt, 0);
		}
		sqlite3_finalize(stmt);
	}

	if ( iID > 0 ) {
		if ( sqlite3_prepare_v3(G_DB,
			"UPDATE menu SET parent = ?, title = ?, icon = ?, type = 1, openType = '_component', href = ?, sort = ?, visible = 1, remark = ?, updateTime = ? WHERE id = ?",
			-1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
			sqlite3_bind_int(stmt, 1, iParentID);
			sqlite3_bind_text(stmt, 2, sTitle, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, sIcon, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, sHref, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 5, iSort);
			sqlite3_bind_text(stmt, 6, sRemark, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 7, iNow);
			sqlite3_bind_int64(stmt, 8, iID);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		return;
	}

	if ( sqlite3_prepare_v3(G_DB,
		"INSERT INTO menu (parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete) "
		"VALUES (?, ?, ?, 1, '_component', ?, ?, 1, ?, ?, ?, 0)",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) == SQLITE_OK ) {
		sqlite3_bind_int(stmt, 1, iParentID);
		sqlite3_bind_text(stmt, 2, sTitle, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, sIcon, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, sHref, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmt, 5, iSort);
		sqlite3_bind_text(stmt, 6, sRemark, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 7, iNow);
		sqlite3_bind_int64(stmt, 8, iNow);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
}

static void MemberMessage_EnsureMenus()
{
	int iParentID = MemberMessage_FindMenuParentID();

	if ( iParentID <= 0 ) {
		return;
	}

	MemberMessage_EnsureMenuItem("站内信管理", "layui-icon layui-icon-notice", "/admin/view/member/notify", iParentID, 401600, "前台用户站内信发送与查看");
	MemberMessage_EnsureMenuItem("邮件任务", "layui-icon layui-icon-email", "/admin/view/member/mail", iParentID, 401700, "前台用户邮件任务与发送记录");
}

static void MemberMessage_EnsureSchema()
{
	MemberMessage_ExecSQLIgnore("CREATE TABLE IF NOT EXISTS notify_message (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT DEFAULT '', content TEXT DEFAULT '', type TEXT DEFAULT 'manual', sourcePlugin TEXT DEFAULT '', bizType TEXT DEFAULT '', bizId INTEGER DEFAULT 0, actionUrl TEXT DEFAULT '', payloadJson TEXT DEFAULT '', senderAdminId INTEGER DEFAULT 0, sendType TEXT DEFAULT 'users', createTime INTEGER DEFAULT 0)");
	MemberMessage_ExecSQLIgnore("CREATE TABLE IF NOT EXISTS notify_recipient (id INTEGER PRIMARY KEY AUTOINCREMENT, messageId INTEGER DEFAULT 0, memberId INTEGER DEFAULT 0, isRead INTEGER DEFAULT 0, readTime INTEGER DEFAULT 0, isDelete INTEGER DEFAULT 0, deleteTime INTEGER DEFAULT 0, createTime INTEGER DEFAULT 0)");
	MemberMessage_ExecSQLIgnore("CREATE INDEX IF NOT EXISTS idx_notify_recipient_member ON notify_recipient(memberId, isDelete, isRead)");
	MemberMessage_ExecSQLIgnore("CREATE INDEX IF NOT EXISTS idx_notify_recipient_message ON notify_recipient(messageId)");
	MemberMessage_ExecSQLIgnore("CREATE TABLE IF NOT EXISTS member_notify_setting (memberId INTEGER PRIMARY KEY, siteInboxEnabled INTEGER DEFAULT 1, emailEnabled INTEGER DEFAULT 1, updateTime INTEGER DEFAULT 0)");
	MemberMessage_ExecSQLIgnore("CREATE TABLE IF NOT EXISTS mail_template (id INTEGER PRIMARY KEY AUTOINCREMENT, code TEXT UNIQUE, name TEXT DEFAULT '', sourcePlugin TEXT DEFAULT '', subjectTpl TEXT DEFAULT '', htmlTpl TEXT DEFAULT '', textTpl TEXT DEFAULT '', enabled INTEGER DEFAULT 1, remark TEXT DEFAULT '', createTime INTEGER DEFAULT 0, updateTime INTEGER DEFAULT 0)");
	MemberMessage_ExecSQLIgnore("CREATE TABLE IF NOT EXISTS mail_task (id INTEGER PRIMARY KEY AUTOINCREMENT, memberId INTEGER DEFAULT 0, toEmail TEXT DEFAULT '', templateCode TEXT DEFAULT '', subject TEXT DEFAULT '', htmlBody TEXT DEFAULT '', textBody TEXT DEFAULT '', payloadJson TEXT DEFAULT '', sourcePlugin TEXT DEFAULT '', bizType TEXT DEFAULT '', bizId INTEGER DEFAULT 0, status TEXT DEFAULT 'pending', retryCount INTEGER DEFAULT 0, maxRetryCount INTEGER DEFAULT 0, nextRetryAt INTEGER DEFAULT 0, errorMessage TEXT DEFAULT '', createTime INTEGER DEFAULT 0, updateTime INTEGER DEFAULT 0, sendTime INTEGER DEFAULT 0)");
	MemberMessage_ExecSQLIgnore("CREATE INDEX IF NOT EXISTS idx_mail_task_status ON mail_task(status, nextRetryAt, createTime)");
	MemberMessage_ExecSQLIgnore("CREATE INDEX IF NOT EXISTS idx_mail_task_member ON mail_task(memberId, createTime)");
}

bool MemberMessage_SendNotify(const char* sSendType, const char* sMemberIDs, const char* sGroupIDs, const char* sTitle, const char* sContent, const char* sActionURL, int64 iSenderAdminID, str* psMessage, int64* pMessageID, int64* pRecipientCount)
{
	xvalue arrRecipients = NULL;
	sqlite3_stmt* stmtMessage = NULL;
	sqlite3_stmt* stmtRecipient = NULL;
	xtime iNow = xrtNow();
	int iRet;
	int64 iCount = 0;
	int64 iMessageID = 0;
	bool bOK = FALSE;

	if ( psMessage ) {
		*psMessage = NULL;
	}
	if ( pMessageID ) {
		*pMessageID = 0;
	}
	if ( pRecipientCount ) {
		*pRecipientCount = 0;
	}

	if ( sTitle == NULL || sTitle[0] == '\0' || sContent == NULL || sContent[0] == '\0' ) {
		if ( psMessage ) *psMessage = xrtCopyStr("标题和内容不能为空", 0);
		return FALSE;
	}

	arrRecipients = MemberMessage_BuildRecipients(sSendType, sMemberIDs, sGroupIDs);
	if ( arrRecipients == NULL ) {
		if ( psMessage ) *psMessage = xrtCopyStr("收件人构建失败", 0);
		return FALSE;
	}

	iCount = (int64)xvoArrayItemCount(arrRecipients);
	if ( iCount <= 0 ) {
		if ( psMessage ) *psMessage = xrtCopyStr("没有匹配到可用的前台用户", 0);
		xvoUnref(arrRecipients);
		return FALSE;
	}

	sqlite3_exec(G_DB, "BEGIN TRANSACTION", NULL, NULL, NULL);

	iRet = sqlite3_prepare_v3(G_DB,
		"INSERT INTO notify_message (title, content, type, sourcePlugin, bizType, bizId, actionUrl, payloadJson, senderAdminId, sendType, createTime) "
		"VALUES (?, ?, 'manual', '', '', 0, ?, '', ?, ?, ?)",
		-1, SQL_PREPARE_DEFAULT, &stmtMessage, NULL);
	if ( iRet != SQLITE_OK ) {
		goto cleanup;
	}

	sqlite3_bind_text(stmtMessage, 1, sTitle, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmtMessage, 2, sContent, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmtMessage, 3, MemberMessage_StrOrEmpty(sActionURL), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmtMessage, 4, iSenderAdminID);
	sqlite3_bind_text(stmtMessage, 5, MemberMessage_StrOrEmpty(sSendType), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmtMessage, 6, iNow);
	if ( sqlite3_step(stmtMessage) != SQLITE_DONE ) {
		goto cleanup;
	}
	iMessageID = sqlite3_last_insert_rowid(G_DB);
	sqlite3_finalize(stmtMessage);
	stmtMessage = NULL;

	iRet = sqlite3_prepare_v3(G_DB,
		"INSERT INTO notify_recipient (messageId, memberId, isRead, readTime, isDelete, deleteTime, createTime) VALUES (?, ?, 0, 0, 0, 0, ?)",
		-1, SQL_PREPARE_DEFAULT, &stmtRecipient, NULL);
	if ( iRet != SQLITE_OK ) {
		goto cleanup;
	}

	for ( uint32 i = 0; i < xvoArrayItemCount(arrRecipients); i++ ) {
		xvalue tblRecipient = xvoArrayGetValue(arrRecipients, i);
		sqlite3_bind_int64(stmtRecipient, 1, iMessageID);
		sqlite3_bind_int64(stmtRecipient, 2, xvoTableGetInt(tblRecipient, "memberId", 8));
		sqlite3_bind_int64(stmtRecipient, 3, iNow);
		if ( sqlite3_step(stmtRecipient) != SQLITE_DONE ) {
			goto cleanup;
		}
		sqlite3_reset(stmtRecipient);
		sqlite3_clear_bindings(stmtRecipient);
	}

	sqlite3_exec(G_DB, "COMMIT", NULL, NULL, NULL);
	bOK = TRUE;
	if ( pMessageID ) {
		*pMessageID = iMessageID;
	}
	if ( pRecipientCount ) {
		*pRecipientCount = iCount;
	}
	if ( psMessage ) {
		*psMessage = xrtFormat("已发送 %lld 条站内信", iCount);
	}

cleanup:
	if ( !bOK ) {
		sqlite3_exec(G_DB, "ROLLBACK", NULL, NULL, NULL);
		if ( psMessage && *psMessage == NULL ) {
			*psMessage = xrtCopyStr((str)sqlite3_errmsg(G_DB), 0);
		}
	}
	if ( stmtRecipient ) {
		sqlite3_finalize(stmtRecipient);
	}
	if ( stmtMessage ) {
		sqlite3_finalize(stmtMessage);
	}
	if ( arrRecipients ) {
		xvoUnref(arrRecipients);
	}
	return bOK;
}

bool MemberMessage_CreateMailTasks(const char* sSendType, const char* sMemberIDs, const char* sGroupIDs, const char* sSubject, const char* sContent, str* psMessage, int64* pCreateCount, int64* pSkipCount)
{
	xvalue arrRecipients = NULL;
	sqlite3_stmt* stmtTask = NULL;
	xtime iNow = xrtNow();
	int64 iCreated = 0;
	int64 iSkipped = 0;
	bool bOK = FALSE;

	if ( psMessage ) {
		*psMessage = NULL;
	}
	if ( pCreateCount ) {
		*pCreateCount = 0;
	}
	if ( pSkipCount ) {
		*pSkipCount = 0;
	}

	if ( !MemberMessage_GetGlobalBool("mail_enabled", FALSE) ) {
		if ( psMessage ) *psMessage = xrtCopyStr("邮件功能未启用，请先在全局配置中开启", 0);
		return FALSE;
	}

	if ( sSubject == NULL || sSubject[0] == '\0' || sContent == NULL || sContent[0] == '\0' ) {
		if ( psMessage ) *psMessage = xrtCopyStr("邮件主题和内容不能为空", 0);
		return FALSE;
	}

	arrRecipients = MemberMessage_BuildRecipients(sSendType, sMemberIDs, sGroupIDs);
	if ( arrRecipients == NULL ) {
		if ( psMessage ) *psMessage = xrtCopyStr("收件人构建失败", 0);
		return FALSE;
	}

	if ( sqlite3_prepare_v3(G_DB,
		"INSERT INTO mail_task (memberId, toEmail, templateCode, subject, htmlBody, textBody, payloadJson, sourcePlugin, bizType, bizId, status, retryCount, maxRetryCount, nextRetryAt, errorMessage, createTime, updateTime, sendTime) "
		"VALUES (?, ?, '', ?, ?, ?, '', '', '', 0, 'pending', 0, ?, 0, '', ?, ?, 0)",
		-1, SQL_PREPARE_DEFAULT, &stmtTask, NULL) != SQLITE_OK ) {
		if ( psMessage ) *psMessage = xrtCopyStr((str)sqlite3_errmsg(G_DB), 0);
		xvoUnref(arrRecipients);
		return FALSE;
	}

	sqlite3_exec(G_DB, "BEGIN TRANSACTION", NULL, NULL, NULL);

	for ( uint32 i = 0; i < xvoArrayItemCount(arrRecipients); i++ ) {
		xvalue tblRecipient = xvoArrayGetValue(arrRecipients, i);
		const char* sEmail = xvoTableGetText(tblRecipient, "email", 5);

		if ( !MemberMessage_EmailLooksValid(sEmail) ) {
			iSkipped++;
			continue;
		}

		sqlite3_bind_int64(stmtTask, 1, xvoTableGetInt(tblRecipient, "memberId", 8));
		sqlite3_bind_text(stmtTask, 2, sEmail, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmtTask, 3, sSubject, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmtTask, 4, "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmtTask, 5, sContent, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmtTask, 6, 3);
		sqlite3_bind_int64(stmtTask, 7, iNow);
		sqlite3_bind_int64(stmtTask, 8, iNow);

		if ( sqlite3_step(stmtTask) != SQLITE_DONE ) {
			sqlite3_exec(G_DB, "ROLLBACK", NULL, NULL, NULL);
			if ( psMessage ) *psMessage = xrtCopyStr((str)sqlite3_errmsg(G_DB), 0);
			goto cleanup;
		}

		iCreated++;
		sqlite3_reset(stmtTask);
		sqlite3_clear_bindings(stmtTask);
	}

	if ( iCreated <= 0 ) {
		sqlite3_exec(G_DB, "ROLLBACK", NULL, NULL, NULL);
		if ( psMessage ) {
			*psMessage = xrtCopyStr("没有匹配到有效邮箱，未创建任何邮件任务", 0);
		}
		goto cleanup;
	}

	sqlite3_exec(G_DB, "COMMIT", NULL, NULL, NULL);
	bOK = TRUE;
	if ( pCreateCount ) {
		*pCreateCount = iCreated;
	}
	if ( pSkipCount ) {
		*pSkipCount = iSkipped;
	}
	if ( psMessage ) {
		*psMessage = xrtFormat("已创建 %lld 条邮件任务，跳过 %lld 个无有效邮箱的用户", iCreated, iSkipped);
	}

cleanup:
	if ( stmtTask ) {
		sqlite3_finalize(stmtTask);
	}
	if ( arrRecipients ) {
		xvoUnref(arrRecipients);
	}
	return bOK;
}

static str MemberMessage_BuildMailSubject(const char* sSubject)
{
	const char* sPrefix = Option_GetGlobalText("mail_subject_prefix", "");
	if ( sPrefix && sPrefix[0] ) {
		if ( sSubject && strncmp(sSubject, sPrefix, strlen(sPrefix)) == 0 ) {
			return xrtCopyStr((str)sSubject, 0);
		}
		return xrtFormat("%s %s", sPrefix, MemberMessage_StrOrEmpty(sSubject));
	}
	return xrtCopyStr((str)MemberMessage_StrOrEmpty(sSubject), 0);
}

static bool MemberMessage_UpdateMailTaskResult(sqlite3* pDB, int64 iTaskID, const char* sStatus, int64 iRetryCount, int64 iNextRetryAt, int64 iSendTime, const char* sErrorMessage)
{
	sqlite3_stmt* stmt = NULL;
	bool bOK = FALSE;

	if ( pDB == NULL ) {
		return FALSE;
	}

	if ( sqlite3_prepare_v3(pDB,
		"UPDATE mail_task SET status = ?, retryCount = ?, nextRetryAt = ?, sendTime = ?, updateTime = ?, errorMessage = ? WHERE id = ?",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}

	sqlite3_bind_text(stmt, 1, MemberMessage_StrOrEmpty(sStatus), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, iRetryCount);
	sqlite3_bind_int64(stmt, 3, iNextRetryAt);
	sqlite3_bind_int64(stmt, 4, iSendTime);
	sqlite3_bind_int64(stmt, 5, xrtNow());
	sqlite3_bind_text(stmt, 6, MemberMessage_StrOrEmpty(sErrorMessage), -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 7, iTaskID);

	bOK = (sqlite3_step(stmt) == SQLITE_DONE);
	sqlite3_finalize(stmt);
	return bOK;
}

static int64 MemberMessage_RecoverStaleSendingTasks(sqlite3* pDB, int64 iNow)
{
	sqlite3_stmt* stmt = NULL;
	int64 iRecovered = 0;
	int64 iThreshold = iNow - 300;

	if ( pDB == NULL ) {
		return 0;
	}
	if ( sqlite3_prepare_v3(pDB,
		"UPDATE mail_task "
		"SET status = 'pending', nextRetryAt = 0, updateTime = ?, errorMessage = CASE "
			"WHEN errorMessage IS NULL OR errorMessage = '' THEN 'Recovered from stale sending state' "
			"ELSE errorMessage END "
		"WHERE status = 'sending' AND updateTime > 0 AND updateTime <= ?",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return 0;
	}
	sqlite3_bind_int64(stmt, 1, iNow);
	sqlite3_bind_int64(stmt, 2, iThreshold);
	if ( sqlite3_step(stmt) == SQLITE_DONE ) {
		iRecovered = sqlite3_changes(pDB);
	}
	sqlite3_finalize(stmt);
	return iRecovered;
}

static bool MemberMessage_RetryMailTask(int64 iTaskID, str* psMessage)
{
	sqlite3_stmt* stmt = NULL;
	const char* sStatus = NULL;
	str sStatusCopy = NULL;
	bool bOK = FALSE;

	if ( psMessage ) {
		*psMessage = NULL;
	}
	if ( iTaskID <= 0 ) {
		if ( psMessage ) *psMessage = xrtCopyStr("邮件任务 ID 无效", 0);
		return FALSE;
	}

	if ( sqlite3_prepare_v3(G_DB,
		"SELECT status FROM mail_task WHERE id = ? LIMIT 1",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		if ( psMessage ) *psMessage = xrtCopyStr((str)sqlite3_errmsg(G_DB), 0);
		return FALSE;
	}
	sqlite3_bind_int64(stmt, 1, iTaskID);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		sStatus = (const char*)sqlite3_column_text(stmt, 0);
		if ( sStatus && sStatus[0] ) {
			sStatusCopy = xrtCopyStr((str)sStatus, 0);
		}
	}
	sqlite3_finalize(stmt);
	stmt = NULL;
	sStatus = sStatusCopy;

	if ( sStatus == NULL || sStatus[0] == '\0' ) {
		if ( psMessage ) *psMessage = xrtCopyStr("邮件任务不存在", 0);
		if ( sStatusCopy ) xrtFree(sStatusCopy);
		return FALSE;
	}
	if ( strcmp(sStatus, "sending") == 0 ) {
		if ( psMessage ) *psMessage = xrtCopyStr("邮件任务正在发送中，暂时不能重试", 0);
		if ( sStatusCopy ) xrtFree(sStatusCopy);
		return FALSE;
	}
	if ( 0 && strcmp(sStatus, "pending") == 0 ) {
		if ( psMessage ) *psMessage = xrtCopyStr("邮件任务已在待发送队列中", 0);
		if ( sStatusCopy ) xrtFree(sStatusCopy);
		return TRUE;
	}
	if ( strcmp(sStatus, "fail") != 0 && strcmp(sStatus, "pending") != 0 ) {
		if ( psMessage ) *psMessage = xrtCopyStr("仅失败任务支持手动重试", 0);
		if ( sStatusCopy ) xrtFree(sStatusCopy);
		return FALSE;
	}

	if ( sqlite3_prepare_v3(G_DB,
		"UPDATE mail_task SET status = 'pending', nextRetryAt = 0, updateTime = ?, errorMessage = '' WHERE id = ?",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		if ( psMessage ) *psMessage = xrtCopyStr((str)sqlite3_errmsg(G_DB), 0);
		return FALSE;
	}
	sqlite3_bind_int64(stmt, 1, xrtNow());
	sqlite3_bind_int64(stmt, 2, iTaskID);
	bOK = (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(G_DB) > 0);
	sqlite3_finalize(stmt);

	if ( bOK ) {
		if ( psMessage ) *psMessage = xrtCopyStr("邮件任务已重新加入待发送队列", 0);
		if ( sStatusCopy ) xrtFree(sStatusCopy);
		MemberMessage_RequestMailQueueRun();
		return TRUE;
	}

	if ( psMessage ) *psMessage = xrtCopyStr("邮件任务重试失败", 0);
	if ( sStatusCopy ) xrtFree(sStatusCopy);
	return FALSE;
}

static bool MemberMessage_DeleteMailTask(int64 iTaskID, str* psMessage)
{
	sqlite3_stmt* stmt = NULL;
	const char* sStatus = NULL;
	str sStatusCopy = NULL;
	bool bOK = FALSE;

	if ( psMessage ) {
		*psMessage = NULL;
	}
	if ( iTaskID <= 0 ) {
		if ( psMessage ) *psMessage = xrtCopyStr("邮件任务 ID 无效", 0);
		return FALSE;
	}

	if ( sqlite3_prepare_v3(G_DB,
		"SELECT status FROM mail_task WHERE id = ? LIMIT 1",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		if ( psMessage ) *psMessage = xrtCopyStr((str)sqlite3_errmsg(G_DB), 0);
		return FALSE;
	}
	sqlite3_bind_int64(stmt, 1, iTaskID);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		sStatus = (const char*)sqlite3_column_text(stmt, 0);
		if ( sStatus && sStatus[0] ) {
			sStatusCopy = xrtCopyStr((str)sStatus, 0);
		}
	}
	sqlite3_finalize(stmt);
	stmt = NULL;

	if ( sStatusCopy == NULL || sStatusCopy[0] == '\0' ) {
		if ( psMessage ) *psMessage = xrtCopyStr("邮件任务不存在", 0);
		if ( sStatusCopy ) xrtFree(sStatusCopy);
		return FALSE;
	}
	if ( strcmp(sStatusCopy, "sending") == 0 ) {
		if ( psMessage ) *psMessage = xrtCopyStr("发送中的邮件任务暂不允许删除", 0);
		xrtFree(sStatusCopy);
		return FALSE;
	}

	if ( sqlite3_prepare_v3(G_DB,
		"DELETE FROM mail_task WHERE id = ?",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		if ( psMessage ) *psMessage = xrtCopyStr((str)sqlite3_errmsg(G_DB), 0);
		xrtFree(sStatusCopy);
		return FALSE;
	}
	sqlite3_bind_int64(stmt, 1, iTaskID);
	bOK = (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(G_DB) > 0);
	sqlite3_finalize(stmt);
	xrtFree(sStatusCopy);

	if ( bOK ) {
		if ( psMessage ) *psMessage = xrtCopyStr("邮件任务已删除", 0);
		return TRUE;
	}

	if ( psMessage ) *psMessage = xrtCopyStr("邮件任务删除失败", 0);
	return FALSE;
}

static bool MemberMessage_LoadSMTPConfigFromData(xvalue tblData, xsmtpconfig* pCfg, str* psError)
{
	if ( psError ) *psError = NULL;
	if ( pCfg == NULL ) {
		if ( psError ) *psError = xrtCopyStr("SMTP config object is invalid", 0);
		return FALSE;
	}

	xrtSmtpConfigInit(pCfg);
	pCfg->sHost = MemberMessage_GetConfigText(tblData, "smtp_host", "");
	pCfg->iPort = (uint16)MemberMessage_GetConfigInt(tblData, "smtp_port", 465);
	pCfg->iTimeoutMs = (uint32)(MemberMessage_GetConfigInt(tblData, "smtp_timeout_sec", 15) * 1000);
	pCfg->iSecureMode = MemberMessage_ParseSmtpSecureMode(MemberMessage_GetConfigText(tblData, "smtp_secure", "ssl"));
	pCfg->bAuth = MemberMessage_GetConfigBool(tblData, "smtp_auth", TRUE);
	pCfg->sUser = MemberMessage_GetConfigText(tblData, "smtp_username", "");
	pCfg->sPass = MemberMessage_GetConfigText(tblData, "smtp_password", "");
	pCfg->tFrom.sName = MemberMessage_GetConfigText(tblData, "mail_from_name", "xadmin");
	pCfg->tFrom.sEmail = MemberMessage_GetConfigText(tblData, "mail_from_email", "");
	pCfg->tReplyTo.sEmail = MemberMessage_GetConfigText(tblData, "mail_reply_to", "");
	pCfg->tReplyTo.sName = pCfg->tFrom.sName;
	pCfg->bVerifyPeer = MemberMessage_GetConfigBool(tblData, "smtp_verify_peer", FALSE);

	if ( pCfg->sHost == NULL || pCfg->sHost[0] == '\0' ) {
		if ( psError ) *psError = xrtCopyStr("SMTP host is not configured", 0);
		return FALSE;
	}
	if ( pCfg->tFrom.sEmail == NULL || pCfg->tFrom.sEmail[0] == '\0' ) {
		if ( psError ) *psError = xrtCopyStr("Sender email is not configured", 0);
		return FALSE;
	}
	if ( pCfg->bAuth && (pCfg->sUser == NULL || pCfg->sUser[0] == '\0') ) {
		if ( psError ) *psError = xrtCopyStr("SMTP username is not configured", 0);
		return FALSE;
	}
	if ( pCfg->bAuth && (pCfg->sPass == NULL || pCfg->sPass[0] == '\0') ) {
		if ( psError ) *psError = xrtCopyStr("SMTP password is not configured", 0);
		return FALSE;
	}
	return TRUE;
}

static bool MemberMessage_LoadSMTPConfig(xsmtpconfig* pCfg, str* psError)
{
	if ( psError ) *psError = NULL;
	if ( pCfg == NULL ) {
		if ( psError ) *psError = xrtCopyStr("SMTP 配置对象无效", 0);
		return FALSE;
	}

	xrtSmtpConfigInit(pCfg);
	pCfg->sHost = Option_GetGlobalText("smtp_host", "");
	pCfg->iPort = (uint16)MemberMessage_GetGlobalInt("smtp_port", 465);
	pCfg->iTimeoutMs = (uint32)(MemberMessage_GetGlobalInt("smtp_timeout_sec", 15) * 1000);
	pCfg->iSecureMode = MemberMessage_ParseSmtpSecureMode(Option_GetGlobalText("smtp_secure", "ssl"));
	pCfg->bAuth = MemberMessage_GetGlobalBool("smtp_auth", TRUE);
	pCfg->sUser = Option_GetGlobalText("smtp_username", "");
	pCfg->sPass = Option_GetGlobalText("smtp_password", "");
	pCfg->tFrom.sName = Option_GetGlobalText("mail_from_name", "xadmin");
	pCfg->tFrom.sEmail = Option_GetGlobalText("mail_from_email", "");
	pCfg->tReplyTo.sEmail = Option_GetGlobalText("mail_reply_to", "");
	pCfg->tReplyTo.sName = pCfg->tFrom.sName;
	pCfg->bVerifyPeer = MemberMessage_GetGlobalBool("smtp_verify_peer", FALSE);

	if ( pCfg->sHost == NULL || pCfg->sHost[0] == '\0' ) {
		if ( psError ) *psError = xrtCopyStr("SMTP 主机未配置", 0);
		return FALSE;
	}
	if ( pCfg->tFrom.sEmail == NULL || pCfg->tFrom.sEmail[0] == '\0' ) {
		if ( psError ) *psError = xrtCopyStr("发件邮箱未配置", 0);
		return FALSE;
	}
	if ( pCfg->bAuth && (pCfg->sUser == NULL || pCfg->sUser[0] == '\0') ) {
		if ( psError ) *psError = xrtCopyStr("SMTP 用户名未配置", 0);
		return FALSE;
	}
	if ( pCfg->bAuth && (pCfg->sPass == NULL || pCfg->sPass[0] == '\0') ) {
		if ( psError ) *psError = xrtCopyStr("SMTP 密码未配置", 0);
		return FALSE;
	}
	return TRUE;
}

static bool MemberMessage_RunPendingMailTasksOnDB(sqlite3* pDB, int64 iLimit, bool bIgnoreRetryAt, str* psMessage, int64* pSuccessCount, int64* pFailCount)
{
	sqlite3_stmt* stmt = NULL;
	xvalue arrTasks = NULL;
	xsmtpconfig tCfg;
	xsmtpasyncopts tAsyncOpts;
	xnetengineconfig tEngineCfg;
	xnetengine* pEngine = NULL;
	member_message_mail_job* arrJobs = NULL;
	xtime iNow = xrtNow();
	int64 iSuccess = 0;
	int64 iFail = 0;
	int64 iRetryInterval = MemberMessage_GetGlobalInt("mail_retry_interval_sec", 300);
	int64 iConcurrency = MemberMessage_GetGlobalInt("mail_queue_concurrency", 5);
	bool bAny = FALSE;
	bool bHasPending = FALSE;
	str sFirstError = NULL;
	uint32 iTaskCount;
	uint32 iTaskIndex = 0;

	if ( psMessage ) *psMessage = NULL;
	if ( pSuccessCount ) *pSuccessCount = 0;
	if ( pFailCount ) *pFailCount = 0;

	if ( pDB == NULL ) {
		if ( psMessage ) *psMessage = xrtCopyStr("邮件数据库连接无效", 0);
		return FALSE;
	}
	if ( iLimit <= 0 ) {
		iLimit = 20;
	}
	arrTasks = xvoCreateArray();
	if ( arrTasks == NULL ) {
		if ( psMessage ) *psMessage = xrtCopyStr("邮件任务缓冲创建失败", 0);
		return FALSE;
	}
	if ( !MemberMessage_GetGlobalBool("mail_enabled", FALSE) ) {
		if ( psMessage ) *psMessage = xrtCopyStr("邮件功能未启用", 0);
		xvoUnref(arrTasks);
		return FALSE;
	}
	if ( !MemberMessage_GetGlobalBool("mail_queue_enabled", TRUE) ) {
		if ( psMessage ) *psMessage = xrtCopyStr("邮件队列未启用", 0);
		xvoUnref(arrTasks);
		return FALSE;
	}
	(void)MemberMessage_RecoverStaleSendingTasks(pDB, iNow);
	if ( sqlite3_prepare_v3(pDB,
		bIgnoreRetryAt
			? "SELECT 1 FROM mail_task WHERE status = 'pending' LIMIT 1"
			: "SELECT 1 FROM mail_task WHERE status = 'pending' AND nextRetryAt <= ? LIMIT 1",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		if ( psMessage ) *psMessage = xrtCopyStr((str)sqlite3_errmsg(pDB), 0);
		xvoUnref(arrTasks);
		return FALSE;
	}

	if ( !bIgnoreRetryAt ) {
		sqlite3_bind_int64(stmt, 1, iNow);
	}
	bHasPending = (sqlite3_step(stmt) == SQLITE_ROW);
	sqlite3_finalize(stmt);
	stmt = NULL;

	if ( !bHasPending ) {
		if ( psMessage ) *psMessage = xrtCopyStr("没有等待中的邮件任务", 0);
		xvoUnref(arrTasks);
		return TRUE;
	}
	if ( sqlite3_prepare_v3(pDB,
		bIgnoreRetryAt
			? "SELECT id, memberId, toEmail, subject, htmlBody, textBody, retryCount, maxRetryCount "
			  "FROM mail_task WHERE status = 'pending' ORDER BY id ASC LIMIT ?"
			: "SELECT id, memberId, toEmail, subject, htmlBody, textBody, retryCount, maxRetryCount "
			  "FROM mail_task WHERE status = 'pending' AND nextRetryAt <= ? ORDER BY id ASC LIMIT ?",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		if ( psMessage ) *psMessage = xrtCopyStr((str)sqlite3_errmsg(pDB), 0);
		xvoUnref(arrTasks);
		return FALSE;
	}

	if ( bIgnoreRetryAt ) {
		sqlite3_bind_int64(stmt, 1, iLimit);
	} else {
		sqlite3_bind_int64(stmt, 1, iNow);
		sqlite3_bind_int64(stmt, 2, iLimit);
	}

	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		xvalue tblTask = xvoCreateTable();
		xvoTableSetInt(tblTask, "id", 2, sqlite3_column_int64(stmt, 0));
		xvoTableSetInt(tblTask, "memberId", 8, sqlite3_column_int64(stmt, 1));
		xvoTableSetText(tblTask, "toEmail", 7, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 2)), 0, FALSE);
		xvoTableSetText(tblTask, "subject", 7, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 3)), 0, FALSE);
		xvoTableSetText(tblTask, "htmlBody", 8, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 4)), 0, FALSE);
		xvoTableSetText(tblTask, "textBody", 8, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 5)), 0, FALSE);
		xvoTableSetInt(tblTask, "retryCount", 10, sqlite3_column_int64(stmt, 6));
		xvoTableSetInt(tblTask, "maxRetryCount", 13, sqlite3_column_int64(stmt, 7));
		xvoArrayAppendValue(arrTasks, tblTask, TRUE);
	}

	sqlite3_finalize(stmt);
	stmt = NULL;

	iTaskCount = xvoArrayItemCount(arrTasks);
	if ( iTaskCount == 0 ) {
		if ( psMessage ) *psMessage = xrtCopyStr("没有待发送的邮件任务", 0);
		xvoUnref(arrTasks);
		return TRUE;
	}

	if ( !MemberMessage_LoadSMTPConfig(&tCfg, psMessage) ) {
		xvoUnref(arrTasks);
		return FALSE;
	}
	xrtNetEngineConfigInit(&tEngineCfg);
	pEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( pEngine == NULL ) {
		if ( psMessage ) *psMessage = xrtCopyStr("SMTP network engine create failed", 0);
		xvoUnref(arrTasks);
		return FALSE;
	}
	if ( xrtNetEngineStart(pEngine) != XRT_NET_OK ) {
		if ( psMessage ) *psMessage = xrtCopyStr("SMTP network engine start failed", 0);
		xrtNetEngineDestroy(pEngine);
		xvoUnref(arrTasks);
		return FALSE;
	}
	xrtSmtpAsyncOptsInit(&tAsyncOpts);
	tAsyncOpts.iTimeoutMs = tCfg.iTimeoutMs;
	tAsyncOpts.pEngine = pEngine;
	tAsyncOpts.sDebugName = "mail_queue";

	if ( iConcurrency < 1 ) iConcurrency = 1;
	if ( iConcurrency > iLimit ) iConcurrency = iLimit;
	if ( iConcurrency > 32 ) iConcurrency = 32;

	arrJobs = (member_message_mail_job*)xrtMalloc(sizeof(member_message_mail_job) * (size_t)iConcurrency);
	if ( arrJobs == NULL ) {
		if ( psMessage ) *psMessage = xrtCopyStr("邮件并发任务缓冲创建失败", 0);
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
		xvoUnref(arrTasks);
		return FALSE;
	}
	memset(arrJobs, 0, sizeof(member_message_mail_job) * (size_t)iConcurrency);

	while ( iTaskIndex < iTaskCount ) {
		uint32 iBatchCount = 0;
		uint32 iWaitIndex;

		for ( iWaitIndex = 0; iWaitIndex < (uint32)iConcurrency; iWaitIndex++ ) {
			MemberMessage_MailJobInit(&arrJobs[iWaitIndex]);
		}

		while ( iBatchCount < (uint32)iConcurrency && iTaskIndex < iTaskCount ) {
			xvalue tblTask = xvoArrayGetValue(arrTasks, iTaskIndex++);
			member_message_mail_job* pJob = &arrJobs[iBatchCount];
			xsmtpresult tImmediateRet;

			if ( tblTask == NULL || xvoType(tblTask) != XVO_DT_TABLE ) {
				continue;
			}
			if ( !MemberMessage_PrepareMailJob(pJob, tblTask) ) {
				MemberMessage_MailJobUnit(pJob);
				continue;
			}
			if ( !MemberMessage_ClaimMailTask(pDB, pJob->iTaskID) ) {
				MemberMessage_MailJobUnit(pJob);
				continue;
			}

			pJob->bClaimed = TRUE;
			bAny = TRUE;
			pJob->pFuture = xrtSmtpSendMailFuture(&tCfg, &pJob->tMsg, &tAsyncOpts);
			if ( pJob->pFuture == NULL ) {
				xrtSmtpResultInit(&tImmediateRet);
				snprintf(tImmediateRet.sError, sizeof(tImmediateRet.sError), "%s", "SMTP async future create failed");
				MemberMessage_RecordMailJobResult(pDB, pJob, &tImmediateRet, iRetryInterval, &iSuccess, &iFail, &sFirstError);
				MemberMessage_MailJobUnit(pJob);
				continue;
			}
			iBatchCount++;
		}

		for ( iWaitIndex = 0; iWaitIndex < iBatchCount; iWaitIndex++ ) {
			member_message_mail_job* pJob = &arrJobs[iWaitIndex];
			xsmtpresult* pAsyncRet = NULL;

			if ( pJob->pFuture ) {
				pAsyncRet = (xsmtpresult*)xFutureWaitValue(pJob->pFuture);
				xFutureRelease(pJob->pFuture);
				pJob->pFuture = NULL;
			}

			MemberMessage_RecordMailJobResult(pDB, pJob, pAsyncRet, iRetryInterval, &iSuccess, &iFail, &sFirstError);
			if ( pAsyncRet ) {
				xrtSmtpResultFree(pAsyncRet);
			}
			MemberMessage_MailJobUnit(pJob);
		}
	}

	if ( pSuccessCount ) *pSuccessCount = iSuccess;
	if ( pFailCount ) *pFailCount = iFail;
	MemberMessage_UpdateMailQueueStats(iLimit, iConcurrency, iSuccess, iFail, xrtNow());

	if ( !bAny ) {
		if ( psMessage ) *psMessage = xrtCopyStr("没有待发送的邮件任务", 0);
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
		xrtFree(arrJobs);
		xvoUnref(arrTasks);
		return TRUE;
	}

	if ( psMessage ) {
		*psMessage = xrtFormat("邮件任务执行完成，成功 %lld 条，失败 %lld 条", iSuccess, iFail);
	}
	if ( psMessage && sFirstError != NULL ) {
		if ( *psMessage ) {
			xrtFree(*psMessage);
		}
		*psMessage = xrtFormat("mail queue done: success=%lld fail=%lld first_error=%s", iSuccess, iFail, sFirstError);
	}
	if ( sFirstError ) {
		xrtFree(sFirstError);
	}
	xrtNetEngineStop(pEngine);
	xrtNetEngineDestroy(pEngine);
	xrtFree(arrJobs);
	xvoUnref(arrTasks);
	return TRUE;
}

bool MemberMessage_TestSMTPWithData(xvalue tblData, const char* sToEmail, str* psMessage)
{
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpresult tRet;
	xsmtpaddr tTo;
	str sSubject = NULL;
	str sTextBody = NULL;
	str sError = NULL;
	bool bOK = FALSE;

	if ( psMessage ) *psMessage = NULL;
	if ( sToEmail == NULL || sToEmail[0] == '\0' ) {
		sToEmail = MemberMessage_GetConfigText(tblData, "mail_from_email", "");
	}
	if ( !MemberMessage_EmailLooksValid(sToEmail) ) {
		if ( psMessage ) *psMessage = xrtCopyStr("Test recipient email is invalid", 0);
		return FALSE;
	}
	if ( !MemberMessage_LoadSMTPConfigFromData(tblData, &tCfg, &sError) ) {
		if ( psMessage ) *psMessage = sError;
		return FALSE;
	}

	memset(&tTo, 0, sizeof(tTo));
	xrtSmtpMessageInit(&tMsg);
	xrtSmtpResultInit(&tRet);

	sSubject = xrtFormat("[xadmin] SMTP Test %s", xrtTimeToStr(xrtNow(), XRT_TIME_FORMAT_DATETIME));
	sTextBody = xrtFormat("This is a test email from xadmin SMTP.\r\nTime: %s\r\nHost: %s:%d",
		xrtTimeToStr(xrtNow(), XRT_TIME_FORMAT_DATETIME),
		MemberMessage_StrOrEmpty(tCfg.sHost),
		(int)tCfg.iPort);
	tTo.sEmail = sToEmail;
	tMsg.arrTo = &tTo;
	tMsg.iToCount = 1;
	tMsg.sSubject = sSubject;
	tMsg.sTextBody = sTextBody;

	bOK = xrtSmtpSendMail(&tCfg, &tMsg, &tRet);
	if ( bOK ) {
		if ( psMessage ) *psMessage = xrtFormat("Test email sent to %s", sToEmail);
	} else {
		if ( psMessage ) *psMessage = xrtFormat("SMTP test failed: %s", MemberMessage_StrOrEmpty(tRet.sError));
	}

	if ( sSubject ) xrtFree(sSubject);
	if ( sTextBody ) xrtFree(sTextBody);
	return bOK;
}

bool MemberMessage_TestSMTP(const char* sToEmail, str* psMessage)
{
	xsmtpconfig tCfg;
	xsmtpmessage tMsg;
	xsmtpresult tRet;
	xsmtpaddr tTo;
	str sSubject = NULL;
	str sTextBody = NULL;
	str sError = NULL;
	bool bOK = FALSE;

	if ( psMessage ) *psMessage = NULL;
	if ( sToEmail == NULL || sToEmail[0] == '\0' ) {
		sToEmail = Option_GetGlobalText("mail_from_email", "");
	}
	if ( !MemberMessage_EmailLooksValid(sToEmail) ) {
		if ( psMessage ) *psMessage = xrtCopyStr("测试收件邮箱格式无效", 0);
		return FALSE;
	}
	if ( !MemberMessage_LoadSMTPConfig(&tCfg, &sError) ) {
		if ( psMessage ) *psMessage = sError;
		return FALSE;
	}

	memset(&tTo, 0, sizeof(tTo));
	xrtSmtpMessageInit(&tMsg);
	xrtSmtpResultInit(&tRet);

	sSubject = xrtFormat("[xadmin] SMTP 测试 %s", xrtTimeToStr(xrtNow(), XRT_TIME_FORMAT_DATETIME));
	sTextBody = xrtFormat("这是一封来自 xadmin 的 SMTP 测试邮件。\r\n时间：%s\r\n主机：%s:%d",
		xrtTimeToStr(xrtNow(), XRT_TIME_FORMAT_DATETIME),
		MemberMessage_StrOrEmpty(tCfg.sHost),
		(int)tCfg.iPort);
	tTo.sEmail = sToEmail;
	tMsg.arrTo = &tTo;
	tMsg.iToCount = 1;
	tMsg.sSubject = sSubject;
	tMsg.sTextBody = sTextBody;

	bOK = xrtSmtpSendMail(&tCfg, &tMsg, &tRet);
	if ( bOK ) {
		if ( psMessage ) *psMessage = xrtFormat("测试邮件已发送到 %s", sToEmail);
	} else {
		if ( psMessage ) *psMessage = xrtFormat("测试发信失败：%s", MemberMessage_StrOrEmpty(tRet.sError));
	}

	if ( sSubject ) xrtFree(sSubject);
	if ( sTextBody ) xrtFree(sTextBody);
	return bOK;
}

bool MemberMessage_RunPendingMailTasks(int64 iLimit, str* psMessage, int64* pSuccessCount, int64* pFailCount)
{
#if 0
	sqlite3_stmt* stmt = NULL;
	xvalue arrTasks = NULL;
	xsmtpconfig tCfg;
	xtime iNow = xrtNow();
	int64 iSuccess = 0;
	int64 iFail = 0;
	int64 iRetryInterval = MemberMessage_GetGlobalInt("mail_retry_interval_sec", 300);
	bool bAny = FALSE;

	if ( psMessage ) *psMessage = NULL;
	if ( pSuccessCount ) *pSuccessCount = 0;
	if ( pFailCount ) *pFailCount = 0;

	if ( iLimit <= 0 ) {
		iLimit = 20;
	}
	arrTasks = xvoCreateArray();
	if ( arrTasks == NULL ) {
		if ( psMessage ) *psMessage = xrtCopyStr("邮件任务缓冲创建失败", 0);
		return FALSE;
	}
	if ( !MemberMessage_GetGlobalBool("mail_enabled", FALSE) ) {
		if ( psMessage ) *psMessage = xrtCopyStr("邮件功能未启用", 0);
		xvoUnref(arrTasks);
		return FALSE;
	}

	xrtSmtpConfigInit(&tCfg);
	tCfg.sHost = Option_GetGlobalText("smtp_host", "");
	tCfg.iPort = (uint16)MemberMessage_GetGlobalInt("smtp_port", 465);
	tCfg.iTimeoutMs = (uint32)(MemberMessage_GetGlobalInt("smtp_timeout_sec", 15) * 1000);
	tCfg.iSecureMode = MemberMessage_ParseSmtpSecureMode(Option_GetGlobalText("smtp_secure", "ssl"));
	tCfg.bAuth = MemberMessage_GetGlobalBool("smtp_auth", TRUE);
	tCfg.sUser = Option_GetGlobalText("smtp_username", "");
	tCfg.sPass = Option_GetGlobalText("smtp_password", "");
	tCfg.tFrom.sName = Option_GetGlobalText("mail_from_name", "xadmin");
	tCfg.tFrom.sEmail = Option_GetGlobalText("mail_from_email", "");
	tCfg.tReplyTo.sEmail = Option_GetGlobalText("mail_reply_to", "");
	tCfg.tReplyTo.sName = tCfg.tFrom.sName;
	tCfg.bVerifyPeer = TRUE;

	if ( tCfg.sHost == NULL || tCfg.sHost[0] == '\0' || tCfg.tFrom.sEmail == NULL || tCfg.tFrom.sEmail[0] == '\0' ) {
		if ( psMessage ) *psMessage = xrtCopyStr("SMTP 配置不完整，请先填写主机和发件邮箱", 0);
		xvoUnref(arrTasks);
		return FALSE;
	}

	if ( sqlite3_prepare_v3(G_DB,
		"SELECT id, memberId, toEmail, subject, htmlBody, textBody, retryCount, maxRetryCount "
		"FROM mail_task WHERE status = 'pending' AND nextRetryAt <= ? ORDER BY id ASC LIMIT ?",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		if ( psMessage ) *psMessage = xrtCopyStr((str)sqlite3_errmsg(G_DB), 0);
		xvoUnref(arrTasks);
		return FALSE;
	}

	sqlite3_bind_int64(stmt, 1, iNow);
	sqlite3_bind_int64(stmt, 2, iLimit);

	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		xvalue tblTask = xvoCreateTable();
		xvoTableSetInt(tblTask, "id", 2, sqlite3_column_int64(stmt, 0));
		xvoTableSetInt(tblTask, "memberId", 8, sqlite3_column_int64(stmt, 1));
		xvoTableSetText(tblTask, "toEmail", 7, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 2)), 0, FALSE);
		xvoTableSetText(tblTask, "subject", 7, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 3)), 0, FALSE);
		xvoTableSetText(tblTask, "htmlBody", 8, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 4)), 0, FALSE);
		xvoTableSetText(tblTask, "textBody", 8, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 5)), 0, FALSE);
		xvoTableSetInt(tblTask, "retryCount", 10, sqlite3_column_int64(stmt, 6));
		xvoTableSetInt(tblTask, "maxRetryCount", 13, sqlite3_column_int64(stmt, 7));
		xvoArrayAppendValue(arrTasks, tblTask, TRUE);
	}

	sqlite3_finalize(stmt);
	stmt = NULL;

	for ( uint32 i = 0; i < xvoArrayItemCount(arrTasks); i++ ) {
		xvalue tblTask = xvoArrayGetValue(arrTasks, i);
		int64 iTaskID = xvoTableGetInt(tblTask, "id", 2);
		int64 iMemberID = xvoTableGetInt(tblTask, "memberId", 8);
		const char* sToEmail = xvoTableGetText(tblTask, "toEmail", 7);
		const char* sSubject = xvoTableGetText(tblTask, "subject", 7);
		const char* sHtmlBody = xvoTableGetText(tblTask, "htmlBody", 8);
		const char* sTextBody = xvoTableGetText(tblTask, "textBody", 8);
		int64 iRetryCount = xvoTableGetInt(tblTask, "retryCount", 10);
		int64 iMaxRetryCount = xvoTableGetInt(tblTask, "maxRetryCount", 13);
		xsmtpaddr tTo;
		xsmtpmessage tMsg;
		xsmtpresult tRet;
		str sFinalSubject = NULL;
		int64 iNewRetryCount = iRetryCount + 1;
		bool bSent = FALSE;

		bAny = TRUE;
		memset(&tTo, 0, sizeof(tTo));
		xrtSmtpMessageInit(&tMsg);
		xrtSmtpResultInit(&tRet);

		tTo.sEmail = sToEmail;
		tMsg.arrTo = &tTo;
		tMsg.iToCount = 1;
		tMsg.sTextBody = MemberMessage_StrOrEmpty(sTextBody);
		tMsg.sHtmlBody = MemberMessage_StrOrEmpty(sHtmlBody);
		sFinalSubject = MemberMessage_BuildMailSubject(sSubject);
		tMsg.sSubject = sFinalSubject;

		bSent = xrtSmtpSendMail(&tCfg, &tMsg, &tRet);
		if ( bSent ) {
			MemberMessage_UpdateMailTaskResult(iTaskID, "success", iRetryCount, 0, xrtNow(), "");
			iSuccess++;
		} else {
			bool bWillRetry = (iNewRetryCount < iMaxRetryCount);
			int64 iNextRetryAt = bWillRetry ? (xrtNow() + iRetryInterval) : 0;
			MemberMessage_UpdateMailTaskResult(iTaskID, bWillRetry ? "pending" : "fail", iNewRetryCount, iNextRetryAt, 0, tRet.sError);
			iFail++;
		}

		if ( sFinalSubject ) {
			xrtFree(sFinalSubject);
		}
	}

	if ( pSuccessCount ) *pSuccessCount = iSuccess;
	if ( pFailCount ) *pFailCount = iFail;

	if ( !bAny ) {
		if ( psMessage ) *psMessage = xrtCopyStr("没有待发送的邮件任务", 0);
		xvoUnref(arrTasks);
		return TRUE;
	}

	if ( psMessage ) {
		*psMessage = xrtFormat("邮件任务执行完成，成功 %lld 条，失败 %lld 条", iSuccess, iFail);
	}
	xvoUnref(arrTasks);
	return TRUE;
#endif
	return MemberMessage_RunPendingMailTasksOnDB(G_DB, iLimit, FALSE, psMessage, pSuccessCount, pFailCount);
}

static int64 MemberMessage_GetMailQueueScanInterval(void)
{
	int64 iInterval = MemberMessage_GetGlobalInt("mail_queue_scan_interval_sec", 60);

	if ( iInterval < 5 ) iInterval = 5;
	if ( iInterval > 86400 ) iInterval = 86400;
	return iInterval;
}

static int64 MemberMessage_GetMailQueueBatchLimit(int64 iIntervalSec)
{
	int64 iRatePerMinute = MemberMessage_GetGlobalInt("mail_rate_limit_per_minute", 60);
	int64 iLimit;

	if ( iRatePerMinute <= 0 ) {
		iRatePerMinute = 20;
	}
	if ( iIntervalSec <= 0 ) {
		iIntervalSec = 60;
	}

	iLimit = (iRatePerMinute * iIntervalSec + 59) / 60;
	if ( iLimit < 1 ) iLimit = 1;
	if ( iLimit > 1000 ) iLimit = 1000;
	return iLimit;
}

static void MemberMessage_SchedTick(sqlite3* pDB, int64 iNow, ptr pUserData)
{
	int64 iIntervalSec;
	int64 iLimit;
	int64 iConcurrency;
	int64 iSuccess = 0;
	int64 iFail = 0;
	str sMessage = NULL;
	bool bForceRun = FALSE;

	(void)pUserData;

	if ( !MemberMessage_GetGlobalBool("mail_enabled", FALSE) ) {
		return;
	}
	if ( !MemberMessage_GetGlobalBool("mail_queue_enabled", TRUE) ) {
		return;
	}

	iIntervalSec = MemberMessage_GetMailQueueScanInterval();
	bForceRun = MemberMessage_PeekMailQueueForceRun();
	if ( !bForceRun && G_MemberMessage_MailQueueLastRun > 0 && iNow < (G_MemberMessage_MailQueueLastRun + iIntervalSec) ) {
		return;
	}
	if ( !MemberMessage_BeginMailQueueRun(&bForceRun) ) {
		return;
	}

	G_MemberMessage_MailQueueLastRun = iNow;
	iLimit = MemberMessage_GetMailQueueBatchLimit(iIntervalSec);
	iConcurrency = MemberMessage_GetGlobalInt("mail_queue_concurrency", 5);
	if ( iConcurrency < 1 ) iConcurrency = 1;
	if ( iConcurrency > iLimit ) iConcurrency = iLimit;
	if ( iConcurrency > 32 ) iConcurrency = 32;
	if ( !MemberMessage_RunPendingMailTasksOnDB(pDB, iLimit, bForceRun, &sMessage, &iSuccess, &iFail) ) {
		if ( sMessage && sMessage[0] != '\0' ) {
			printf("[mail_queue] run failed: limit=%lld concurrency=%lld message=%s\n", iLimit, iConcurrency, sMessage);
		}
	} else if ( iSuccess > 0 || iFail > 0 ) {
		printf("[mail_queue] run done: limit=%lld concurrency=%lld success=%lld fail=%lld\n", iLimit, iConcurrency, iSuccess, iFail);
	}

	if ( sMessage ) {
		xrtFree(sMessage);
	}
	MemberMessage_EndMailQueueRun();
}

xvalue MemberMessage_AdminListNotify(int64 iPage, int64 iLimit, const char* sSearch, int64* pCount)
{
	sqlite3_stmt* stmt = NULL;
	sqlite3_stmt* stmtCount = NULL;
	int64 iOffset;
	xvalue arrData = xvoCreateArray();

	if ( pCount ) {
		*pCount = 0;
	}
	if ( iPage <= 0 ) iPage = 1;
	if ( iLimit <= 0 ) iLimit = 20;
	iOffset = (iPage - 1) * iLimit;

	if ( sSearch && sSearch[0] != '\0' ) {
		sqlite3_prepare_v3(G_DB,
			"SELECT m.id, m.title, m.type, m.sendType, m.createTime, "
			"(SELECT COUNT(*) FROM notify_recipient r WHERE r.messageId = m.id) "
			"FROM notify_message m WHERE m.title LIKE ? OR m.content LIKE ? ORDER BY m.id DESC LIMIT ? OFFSET ?",
			-1, SQL_PREPARE_DEFAULT, &stmt, NULL);
		sqlite3_bind_text(stmt, 1, sSearch, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, sSearch, -1, SQLITE_TRANSIENT);

		sqlite3_prepare_v3(G_DB,
			"SELECT COUNT(*) FROM notify_message WHERE title LIKE ? OR content LIKE ?",
			-1, SQL_PREPARE_DEFAULT, &stmtCount, NULL);
		sqlite3_bind_text(stmtCount, 1, sSearch, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmtCount, 2, sSearch, -1, SQLITE_TRANSIENT);
	} else {
		sqlite3_prepare_v3(G_DB,
			"SELECT m.id, m.title, m.type, m.sendType, m.createTime, "
			"(SELECT COUNT(*) FROM notify_recipient r WHERE r.messageId = m.id) "
			"FROM notify_message m ORDER BY m.id DESC LIMIT ? OFFSET ?",
			-1, SQL_PREPARE_DEFAULT, &stmt, NULL);

		sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM notify_message", -1, SQL_PREPARE_DEFAULT, &stmtCount, NULL);
	}

	if ( stmt ) {
		int iBindBase = (sSearch && sSearch[0] != '\0') ? 3 : 1;
		sqlite3_bind_int64(stmt, iBindBase, iLimit);
		sqlite3_bind_int64(stmt, iBindBase + 1, iOffset);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblRow = xvoCreateTable();
			xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
			xvoTableSetText(tblRow, "title", 5, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 1)), 0, FALSE);
			xvoTableSetText(tblRow, "type", 4, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 2)), 0, FALSE);
			xvoTableSetText(tblRow, "sendType", 8, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 3)), 0, FALSE);
			xvoTableSetInt(tblRow, "recipientCount", 14, sqlite3_column_int64(stmt, 5));
			xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(sqlite3_column_int64(stmt, 4), XRT_TIME_FORMAT_DATETIME), 0, TRUE);
			xvoArrayAppendValue(arrData, tblRow, TRUE);
		}
		sqlite3_finalize(stmt);
	}

	if ( stmtCount ) {
		if ( sqlite3_step(stmtCount) == SQLITE_ROW && pCount ) {
			*pCount = sqlite3_column_int64(stmtCount, 0);
		}
		sqlite3_finalize(stmtCount);
	}

	return arrData;
}

xvalue MemberMessage_AdminListMailTasks(int64 iPage, int64 iLimit, const char* sSearch, int64 iTaskID, int64* pCount)
{
	sqlite3_stmt* stmt = NULL;
	sqlite3_stmt* stmtCount = NULL;
	int64 iOffset;
	xvalue arrData = xvoCreateArray();

	if ( pCount ) {
		*pCount = 0;
	}
	if ( iPage <= 0 ) iPage = 1;
	if ( iLimit <= 0 ) iLimit = 20;
	iOffset = (iPage - 1) * iLimit;

	if ( iTaskID > 0 && sSearch && sSearch[0] != '\0' ) {
		sqlite3_prepare_v3(G_DB,
			"SELECT id, memberId, toEmail, subject, status, retryCount, maxRetryCount, createTime, sendTime, errorMessage "
			"FROM mail_task WHERE id = ? AND (toEmail LIKE ? OR subject LIKE ? OR errorMessage LIKE ?) ORDER BY id DESC LIMIT ? OFFSET ?",
			-1, SQL_PREPARE_DEFAULT, &stmt, NULL);
		sqlite3_bind_int64(stmt, 1, iTaskID);
		sqlite3_bind_text(stmt, 2, sSearch, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, sSearch, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 4, sSearch, -1, SQLITE_TRANSIENT);

		sqlite3_prepare_v3(G_DB,
			"SELECT COUNT(*) FROM mail_task WHERE id = ? AND (toEmail LIKE ? OR subject LIKE ? OR errorMessage LIKE ?)",
			-1, SQL_PREPARE_DEFAULT, &stmtCount, NULL);
		sqlite3_bind_int64(stmtCount, 1, iTaskID);
		sqlite3_bind_text(stmtCount, 2, sSearch, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmtCount, 3, sSearch, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmtCount, 4, sSearch, -1, SQLITE_TRANSIENT);
	} else if ( iTaskID > 0 ) {
		sqlite3_prepare_v3(G_DB,
			"SELECT id, memberId, toEmail, subject, status, retryCount, maxRetryCount, createTime, sendTime, errorMessage "
			"FROM mail_task WHERE id = ? ORDER BY id DESC LIMIT ? OFFSET ?",
			-1, SQL_PREPARE_DEFAULT, &stmt, NULL);
		sqlite3_bind_int64(stmt, 1, iTaskID);

		sqlite3_prepare_v3(G_DB,
			"SELECT COUNT(*) FROM mail_task WHERE id = ?",
			-1, SQL_PREPARE_DEFAULT, &stmtCount, NULL);
		sqlite3_bind_int64(stmtCount, 1, iTaskID);
	} else if ( sSearch && sSearch[0] != '\0' ) {
		sqlite3_prepare_v3(G_DB,
			"SELECT id, memberId, toEmail, subject, status, retryCount, maxRetryCount, createTime, sendTime, errorMessage "
			"FROM mail_task WHERE toEmail LIKE ? OR subject LIKE ? OR errorMessage LIKE ? ORDER BY id DESC LIMIT ? OFFSET ?",
			-1, SQL_PREPARE_DEFAULT, &stmt, NULL);
		sqlite3_bind_text(stmt, 1, sSearch, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, sSearch, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 3, sSearch, -1, SQLITE_TRANSIENT);

		sqlite3_prepare_v3(G_DB,
			"SELECT COUNT(*) FROM mail_task WHERE toEmail LIKE ? OR subject LIKE ? OR errorMessage LIKE ?",
			-1, SQL_PREPARE_DEFAULT, &stmtCount, NULL);
		sqlite3_bind_text(stmtCount, 1, sSearch, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmtCount, 2, sSearch, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmtCount, 3, sSearch, -1, SQLITE_TRANSIENT);
	} else {
		sqlite3_prepare_v3(G_DB,
			"SELECT id, memberId, toEmail, subject, status, retryCount, maxRetryCount, createTime, sendTime, errorMessage "
			"FROM mail_task ORDER BY id DESC LIMIT ? OFFSET ?",
			-1, SQL_PREPARE_DEFAULT, &stmt, NULL);

		sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM mail_task", -1, SQL_PREPARE_DEFAULT, &stmtCount, NULL);
	}

	if ( stmt ) {
		int iBindBase = 1;
		if ( iTaskID > 0 && sSearch && sSearch[0] != '\0' ) {
			iBindBase = 5;
		} else if ( iTaskID > 0 ) {
			iBindBase = 2;
		} else if ( sSearch && sSearch[0] != '\0' ) {
			iBindBase = 4;
		}
		sqlite3_bind_int64(stmt, iBindBase, iLimit);
		sqlite3_bind_int64(stmt, iBindBase + 1, iOffset);
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			xvalue tblRow = xvoCreateTable();
			xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
			xvoTableSetInt(tblRow, "memberId", 8, sqlite3_column_int64(stmt, 1));
			xvoTableSetText(tblRow, "toEmail", 7, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 2)), 0, FALSE);
			xvoTableSetText(tblRow, "subject", 7, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 3)), 0, FALSE);
			xvoTableSetText(tblRow, "status", 6, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 4)), 0, FALSE);
			xvoTableSetInt(tblRow, "retryCount", 10, sqlite3_column_int64(stmt, 5));
			xvoTableSetInt(tblRow, "maxRetryCount", 13, sqlite3_column_int64(stmt, 6));
			xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(sqlite3_column_int64(stmt, 7), XRT_TIME_FORMAT_DATETIME), 0, TRUE);
			if ( sqlite3_column_int64(stmt, 8) > 0 ) {
				xvoTableSetText(tblRow, "sendTime", 8, xrtTimeToStr(sqlite3_column_int64(stmt, 8), XRT_TIME_FORMAT_DATETIME), 0, TRUE);
			} else {
				xvoTableSetText(tblRow, "sendTime", 8, "-", 0, FALSE);
			}
			xvoTableSetText(tblRow, "errorMessage", 12, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 9)), 0, FALSE);
			xvoArrayAppendValue(arrData, tblRow, TRUE);
		}
		sqlite3_finalize(stmt);
	}

	if ( stmtCount ) {
		if ( sqlite3_step(stmtCount) == SQLITE_ROW && pCount ) {
			*pCount = sqlite3_column_int64(stmtCount, 0);
		}
		sqlite3_finalize(stmtCount);
	}

	return arrData;
}

xvalue MemberMessage_MemberListNotify(int64 iMemberID, int64 iPage, int64 iLimit, int64* pCount)
{
	sqlite3_stmt* stmt = NULL;
	sqlite3_stmt* stmtCount = NULL;
	xvalue arrData = xvoCreateArray();
	int64 iOffset;

	if ( pCount ) {
		*pCount = 0;
	}
	if ( iPage <= 0 ) iPage = 1;
	if ( iLimit <= 0 ) iLimit = 20;
	iOffset = (iPage - 1) * iLimit;

	sqlite3_prepare_v3(G_DB,
		"SELECT r.id, r.messageId, r.isRead, r.readTime, m.title, m.content, m.type, m.actionUrl, m.createTime "
		"FROM notify_recipient r INNER JOIN notify_message m ON r.messageId = m.id "
		"WHERE r.memberId = ? AND r.isDelete = 0 ORDER BY r.id DESC LIMIT ? OFFSET ?",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL);
	sqlite3_bind_int64(stmt, 1, iMemberID);
	sqlite3_bind_int64(stmt, 2, iLimit);
	sqlite3_bind_int64(stmt, 3, iOffset);

	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
		xvoTableSetInt(tblRow, "messageId", 9, sqlite3_column_int64(stmt, 1));
		xvoTableSetBool(tblRow, "isRead", 6, sqlite3_column_int(stmt, 2) == 1);
		xvoTableSetInt(tblRow, "readTime", 8, sqlite3_column_int64(stmt, 3));
		xvoTableSetText(tblRow, "title", 5, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 4)), 0, FALSE);
		xvoTableSetText(tblRow, "content", 7, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 5)), 0, FALSE);
		xvoTableSetText(tblRow, "type", 4, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 6)), 0, FALSE);
		xvoTableSetText(tblRow, "actionUrl", 9, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 7)), 0, FALSE);
		xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 8));
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_finalize(stmt);

	sqlite3_prepare_v3(G_DB,
		"SELECT COUNT(*) FROM notify_recipient WHERE memberId = ? AND isDelete = 0",
		-1, SQL_PREPARE_DEFAULT, &stmtCount, NULL);
	sqlite3_bind_int64(stmtCount, 1, iMemberID);
	if ( sqlite3_step(stmtCount) == SQLITE_ROW && pCount ) {
		*pCount = sqlite3_column_int64(stmtCount, 0);
	}
	sqlite3_finalize(stmtCount);

	return arrData;
}

int64 MemberMessage_MemberUnreadCount(int64 iMemberID)
{
	sqlite3_stmt* stmt = NULL;
	int64 iCount = 0;

	sqlite3_prepare_v3(G_DB,
		"SELECT COUNT(*) FROM notify_recipient WHERE memberId = ? AND isDelete = 0 AND isRead = 0",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL);
	sqlite3_bind_int64(stmt, 1, iMemberID);
	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt, 0);
	}
	sqlite3_finalize(stmt);
	return iCount;
}

xvalue MemberMessage_MemberNotifyDetail(int64 iMemberID, int64 iRecipientID)
{
	sqlite3_stmt* stmt = NULL;
	xvalue tblRow = NULL;

	sqlite3_prepare_v3(G_DB,
		"SELECT r.id, r.messageId, r.isRead, r.readTime, m.title, m.content, m.type, m.actionUrl, m.createTime "
		"FROM notify_recipient r INNER JOIN notify_message m ON r.messageId = m.id "
		"WHERE r.id = ? AND r.memberId = ? AND r.isDelete = 0 LIMIT 1",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL);
	sqlite3_bind_int64(stmt, 1, iRecipientID);
	sqlite3_bind_int64(stmt, 2, iMemberID);

	if ( sqlite3_step(stmt) == SQLITE_ROW ) {
		tblRow = xvoCreateTable();
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt, 0));
		xvoTableSetInt(tblRow, "messageId", 9, sqlite3_column_int64(stmt, 1));
		xvoTableSetBool(tblRow, "isRead", 6, sqlite3_column_int(stmt, 2) == 1);
		xvoTableSetInt(tblRow, "readTime", 8, sqlite3_column_int64(stmt, 3));
		xvoTableSetText(tblRow, "title", 5, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 4)), 0, FALSE);
		xvoTableSetText(tblRow, "content", 7, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 5)), 0, FALSE);
		xvoTableSetText(tblRow, "type", 4, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 6)), 0, FALSE);
		xvoTableSetText(tblRow, "actionUrl", 9, (str)MemberMessage_StrOrEmpty((const char*)sqlite3_column_text(stmt, 7)), 0, FALSE);
		xvoTableSetInt(tblRow, "createTime", 10, sqlite3_column_int64(stmt, 8));
	}

	sqlite3_finalize(stmt);
	return tblRow;
}

bool MemberMessage_MarkNotifyRead(int64 iMemberID, xvalue arrIDs, bool bReadAll)
{
	sqlite3_stmt* stmt = NULL;
	xtime iNow = xrtNow();
	bool bOK = FALSE;

	if ( bReadAll ) {
		if ( sqlite3_prepare_v3(G_DB,
			"UPDATE notify_recipient SET isRead = 1, readTime = ? WHERE memberId = ? AND isDelete = 0 AND isRead = 0",
			-1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
			return FALSE;
		}
		sqlite3_bind_int64(stmt, 1, iNow);
		sqlite3_bind_int64(stmt, 2, iMemberID);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		return TRUE;
	}

	if ( arrIDs == NULL || xvoType(arrIDs) != XVO_DT_ARRAY || xvoArrayItemCount(arrIDs) == 0 ) {
		return FALSE;
	}

	if ( sqlite3_prepare_v3(G_DB,
		"UPDATE notify_recipient SET isRead = 1, readTime = ? WHERE id = ? AND memberId = ? AND isDelete = 0",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}

	for ( uint32 i = 0; i < xvoArrayItemCount(arrIDs); i++ ) {
		sqlite3_bind_int64(stmt, 1, iNow);
		sqlite3_bind_int64(stmt, 2, xvoArrayGetInt(arrIDs, i));
		sqlite3_bind_int64(stmt, 3, iMemberID);
		sqlite3_step(stmt);
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
		bOK = TRUE;
	}

	sqlite3_finalize(stmt);
	return bOK;
}

bool MemberMessage_DeleteNotify(int64 iMemberID, xvalue arrIDs)
{
	sqlite3_stmt* stmt = NULL;
	xtime iNow = xrtNow();
	bool bOK = FALSE;

	if ( arrIDs == NULL || xvoType(arrIDs) != XVO_DT_ARRAY || xvoArrayItemCount(arrIDs) == 0 ) {
		return FALSE;
	}

	if ( sqlite3_prepare_v3(G_DB,
		"UPDATE notify_recipient SET isDelete = 1, deleteTime = ? WHERE id = ? AND memberId = ? AND isDelete = 0",
		-1, SQL_PREPARE_DEFAULT, &stmt, NULL) != SQLITE_OK ) {
		return FALSE;
	}

	for ( uint32 i = 0; i < xvoArrayItemCount(arrIDs); i++ ) {
		sqlite3_bind_int64(stmt, 1, iNow);
		sqlite3_bind_int64(stmt, 2, xvoArrayGetInt(arrIDs, i));
		sqlite3_bind_int64(stmt, 3, iMemberID);
		sqlite3_step(stmt);
		sqlite3_reset(stmt);
		sqlite3_clear_bindings(stmt);
		bOK = TRUE;
	}

	sqlite3_finalize(stmt);
	return bOK;
}

void MemberMessage_Init()
{
	printf("        MemberMessage_Init \n");
	if ( G_MemberMessage_MailQueueLock == NULL ) {
		G_MemberMessage_MailQueueLock = xrtMutexCreate();
	}
	MemberMessage_EnsureSchema();
	MemberMessage_EnsureMemberAPIRoutes();
	if ( !Sched_RegisterSystemTickHook("member_mail_queue", 5, MemberMessage_SchedTick, NULL) ) {
		printf("[mail_queue] failed to register scheduler hook\n");
	}
}

void MemberMessage_Unit()
{
	printf("        MemberMessage_Unit \n");
	if ( G_MemberMessage_MailQueueLock ) {
		xrtMutexDestroy(G_MemberMessage_MailQueueLock);
		G_MemberMessage_MailQueueLock = NULL;
	}
}
