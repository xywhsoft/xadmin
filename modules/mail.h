/* 邮件家族核心：模板/任务/日志三表 + 基于 xs3 新 xsmtp 客户端的发送 +
 * 独立队列线程（领取-发送-记录-重试）。
 *
 * 【编译门控】XADMIN_WITH_SMTP：新 xsmtp/xmail 符号尚未编入 xs.exe
 * （xserver 集成中）。集成落地后置 1 并按实际头文件形态调整下方
 * include（若 xs 将声明并入 xrt_decl.h 则删去四个 xrt/*.h include 即可，
 * xsbase.h 已携带类型）。门控关闭时本模块与路由完全不参与编译。
 *
 * 【与 v1 的有意差异】v1 用 xrtSmtpSendMailFuture 做并发发送；新 API
 * 是同步客户端 + deadline/cancel，队列改为单线程顺序发送（速率与批量
 * 上限仍生效）。事务邮件场景下顺序队列更可控，并发上限配置保留为
 * 批量上限语义。 */

#ifndef XADMIN_WITH_SMTP
#define XADMIN_WITH_SMTP 1
#endif

#if XADMIN_WITH_SMTP

#include <xsmtp.h>   /* 伞形头：features 级联 + xmail 基座 + smtp 全模块 */

/* ---- 配置读取（global.json 的 smtp 前缀与 mail 前缀字段） ---- */

static xvalue* Mail_GlobalOption(void)
{
	return ValueGet(G_Option, "global");
}

static const char* Mail_GetText(const char* name, const char* fallback)
{
	xvalue* cfg = Mail_GlobalOption();
	str text = cfg ? ValueText(cfg, name) : NULL;
	return text && text[0] ? text : fallback;
}

static int64 Mail_GetInt(const char* name, int64 fallback)
{
	xvalue* cfg = Mail_GlobalOption();
	int64 value = cfg ? ValueInt(cfg, name) : 0;
	return value != 0 ? value : fallback;
}

static bool Mail_GetBool(const char* name, bool fallback)
{
	xvalue* cfg = Mail_GlobalOption();
	xvalue* v = cfg ? ValueGet(cfg, name) : NULL;
	if (!v) return fallback;
	if (xrtValueType(v) == XVALUE_BOOL) return ValueBoolOf(v);
	if (xrtValueType(v) == XVALUE_INT) return ValueIntOf(v) != 0;
	return fallback;
}

static bool Mail_Enabled(void)
{
	return Mail_GetBool("mail_enabled", false);
}

static int64 Mail_ScanInterval(void)
{
	int64 sec = Mail_GetInt("mail_queue_scan_interval_sec", 60);
	if (sec < 5) sec = 5;
	if (sec > 86400) sec = 86400;
	return sec;
}

static int64 Mail_BatchLimit(int64 intervalSec)
{
	int64 rate = Mail_GetInt("mail_rate_limit_per_minute", 60);
	int64 limit;
	if (rate <= 0) rate = 20;
	if (intervalSec <= 0) intervalSec = 60;
	limit = (rate * intervalSec + 59) / 60;
	if (limit < 1) limit = 1;
	if (limit > 1000) limit = 1000;
	return limit;
}

/* ---- 小工具 ---- */

static bool Mail_EmailValid(const char* email)
{
	const char* at;
	if (!email || !email[0]) return false;
	at = strchr(email, '@');
	if (!at || at == email || !at[1] || strchr(at + 1, '@')) return false;
	if (!strchr(at + 1, '.') || strchr(at + 1, '.') == at + 1) return false;
	if (email[0] == '.' || email[strlen(email) - 1] == '.') return false;
	return strpbrk(email, " \t\r\n") == NULL;
}

static char* Mail_BuildSubject(const char* subject)
{
	return xrtFormat("%s", subject ? subject : "");
}

/* ---- SMTP 运行时（借用 Engine/Resolver 的客户端装配 + 发送） ---- */

/* smtp_verify_peer=false：接受所有对端证书的验证回调 */
static xtlsverifydecision Mail_AcceptAllVerify(const xtlspeer* peer, ptr context)
{
	(void)peer; (void)context;
	return XTLS_VERIFY_ACCEPT;
}

typedef struct MailRuntime {
	xnetengine* engine;
	xnetresolver* resolver;
	xtlscontext* tls;
	xtlsverifier* verifier;
	xsmtpclientconfig client;
	bool valid;
	char error[192];
} MailRuntime;

static void Mail_RuntimeUnit(MailRuntime* rt)
{
	if (rt->engine) {
		xrtNetEngineStop(rt->engine);
		xrtNetEngineDestroy(rt->engine);
		rt->engine = NULL;
	}
	if (rt->resolver) {
		xrtNetResolverDestroy(rt->resolver);
		rt->resolver = NULL;
	}
	if (rt->tls) {
		xrtTlsContextRelease(rt->tls);
		rt->tls = NULL;
	}
	if (rt->verifier) {
		xrtTlsVerifierRelease(rt->verifier);
		rt->verifier = NULL;
	}
}

static bool Mail_RuntimeInit(MailRuntime* rt)
{
	xnetengineconfig engineCfg;
	const char* host = Mail_GetText("smtp_host", "");
	const char* fromEmail = Mail_GetText("mail_from_email", "");
	const char* secure = Mail_GetText("smtp_secure", "ssl");
	uint16 port = (uint16)Mail_GetInt("smtp_port", 465);
	xmailsecurity security;

	memset(rt, 0, sizeof(*rt));
	if (!host[0]) {
		snprintf(rt->error, sizeof(rt->error), "SMTP 配置不完整，请先填写主机");
		return false;
	}
	if (!Mail_EmailValid(fromEmail)) {
		snprintf(rt->error, sizeof(rt->error), "SMTP 配置不完整，请先填写发件邮箱");
		return false;
	}
	/* v1 的 auto = STARTTLS 优先失败回落明文；新 API 显式选择 */
	if (strcmp(secure, "starttls") == 0 || strcmp(secure, "auto") == 0)
		security = XMAIL_SECURITY_STARTTLS;
	else if (strcmp(secure, "none") == 0)
		security = XMAIL_SECURITY_PLAIN;
	else
		security = XMAIL_SECURITY_TLS;

	{
		xnetresolverconfig resolverCfg;
		xrtNetEngineConfigInit(&engineCfg);
		xrtNetResolverConfigInit(&resolverCfg);
		rt->engine = xrtNetEngineCreate(&engineCfg);
		rt->resolver = xrtNetResolverCreate(&resolverCfg);
		if (!rt->engine || !rt->resolver || !xrtNetEngineStart(rt->engine)) {
			snprintf(rt->error, sizeof(rt->error), "SMTP 网络引擎创建失败");
			Mail_RuntimeUnit(rt);
			return false;
		}
	}
	xrtSmtpClientConfigInit(&rt->client);
	xrtMailNetConfigInit(&rt->client.Net);
	{
		xtlscontextconfig tlsCfg;
		xtlsverifierconfig verifierCfg;
		xrtTlsContextConfigInit(&tlsCfg);
		rt->tls = xrtTlsContextCreate(&tlsCfg);
		xrtTlsVerifierConfigInit(&verifierCfg);
		verifierCfg.Verify = Mail_AcceptAllVerify;
		rt->verifier = xrtTlsVerifierCreate(&verifierCfg);
	}
	rt->client.Net.Engine = rt->engine;
	rt->client.Net.Resolver = rt->resolver;
	rt->client.Net.Tls.Context = rt->tls;
	rt->client.Net.Tls.Verifier = rt->verifier;
	rt->client.Net.Host = host;
	rt->client.Net.Port = port;
	rt->client.Net.Security = security;
	rt->valid = true;
	return true;
}

static bool Mail_SendOne(const xsmtpclientconfig* config, const char* toEmail, const char* subject,
	const char* htmlBody, const char* textBody, char* error, size_t errorCap)
{
	xsmtpclient* client = NULL;
	xsmtpauthconfig auth;
	xmailmessage msg;
	xmailaddress to;
	XAdminDeadline deadline;
	uint64 timeoutUs = (uint64)(Mail_GetInt("smtp_timeout_sec", 15)) * 1000000;
	const char* user = Mail_GetText("smtp_username", "");
	const char* pass = Mail_GetText("smtp_password", "");
	bool ok = false;

	xrtMailMessageInit(&msg);
	to.Address = xrtStrView(toEmail ? toEmail : "");
	to.Name = xrtStrView("");
	msg.From.Address = xrtStrView(Mail_GetText("mail_from_email", ""));
	msg.From.Name = xrtStrView(Mail_GetText("mail_from_name", "xadmin"));
	msg.ReplyTo.Address = xrtStrView(Mail_GetText("mail_reply_to", ""));
	msg.ReplyTo.Name = xrtStrView("");
	msg.To = &to;
	msg.ToCount = 1;
	msg.Subject = xrtStrView(subject ? subject : "");
	msg.Text = xrtStrView(textBody ? textBody : "");
	if (htmlBody && htmlBody[0]) msg.Html = xrtStrView(htmlBody);

	deadline = XAdmin_DeadlineAfterMs(timeoutUs / 1000);
	if (!xrtSmtpClientConfigValid(config)) {
		snprintf(error, errorCap, "SMTP 配置无效（主机/端口/安全模式/TLS 验证器）");
		return false;
	}
	client = xrtSmtpClientOpen(config, XAdmin_DeadlineRemainingMs(deadline), NULL);
	if (!client) {
		/* auto 模式回落：STARTTLS 失败时按明文重试一次 */
		if (config->Net.Security == XMAIL_SECURITY_STARTTLS &&
			strcmp(Mail_GetText("smtp_secure", "ssl"), "auto") == 0) {
			xsmtpclientconfig plain = *config;
			plain.Net.Security = XMAIL_SECURITY_PLAIN;
			deadline = XAdmin_DeadlineAfterMs(timeoutUs / 1000);
			client = xrtSmtpClientOpen(&plain, XAdmin_DeadlineRemainingMs(deadline), NULL);
		}
	}
	if (!client) {
		snprintf(error, errorCap, "SMTP 连接失败（%s:%d）",
			config->Net.Host ? config->Net.Host : "", (int)config->Net.Port);
		return false;
	}
	if (Mail_GetBool("smtp_auth", true) && user[0] && pass[0]) {
		xrtSmtpAuthConfigInit(&auth);
		auth.Method = XSMTP_AUTH_PLAIN;
		auth.Username = xrtStrView(user);
		auth.Secret = xrtStrView(pass);
		if (!xrtSmtpClientAuth(client, &auth, XAdmin_DeadlineRemainingMs(deadline), NULL)) {
			/* PLAIN 被拒时回落 LOGIN */
			auth.Method = XSMTP_AUTH_LOGIN;
			if (!xrtSmtpClientAuth(client, &auth, XAdmin_DeadlineRemainingMs(deadline), NULL)) {
				snprintf(error, errorCap, "SMTP 认证失败");
				xrtSmtpClientQuit(client, XAdmin_DeadlineRemainingMs(deadline), NULL);
				xrtSmtpClientDestroy(client);
				return false;
			}
		}
	}
	{
		char mailBody[4096];
		char* composed;
		size_t composedSize = 0;
		composed = xrtMailCompose(&msg, &composedSize);
		if (!composed) {
			snprintf(error, errorCap, "SMTP 邮件内容构建失败");
			xrtSmtpClientQuit(client, XAdmin_DeadlineRemainingMs(deadline), NULL);
			xrtSmtpClientDestroy(client);
			return false;
		}
		ok = xrtSmtpClientMail(client, xrtStrView(toEmail ? toEmail : ""), xrtStrView(""), XAdmin_DeadlineRemainingMs(deadline), NULL)
			&& xrtSmtpClientRcpt(client, xrtStrView(toEmail ? toEmail : ""), xrtStrView(""), XAdmin_DeadlineRemainingMs(deadline), NULL);
		if (ok) {
			ok = xrtSmtpClientData(client, xrtStrViewN(composed, composedSize), XAdmin_DeadlineRemainingMs(deadline), NULL);
			if (!ok) {
				xsmtpreply reply;
				if (xrtSmtpClientLastReply(client, &reply)) {
					snprintf(error, errorCap, "SMTP DATA 被拒（%d）", reply.Code);
				}
			}
		} else {
			xsmtpreply reply;
			if (xrtSmtpClientLastReply(client, &reply)) {
				snprintf(error, errorCap, "SMTP MAIL/RCPT 被拒（%d）", reply.Code);
			}
		}
		xrtFree(composed);
		(void)mailBody;
	}
	xrtSmtpClientQuit(client, XAdmin_DeadlineRemainingMs(deadline), NULL);
	xrtSmtpClientDestroy(client);
	return ok;
}

/* ---- 队列状态（线程互斥） ---- */

static xmutex* G_MailLock;
static volatile bool G_MailStop;
static volatile bool G_MailRunning;
static volatile bool G_MailForceRun;
static int64 G_MailLastSuccess, G_MailLastFail, G_MailLastRun;
static xthread* G_MailThread;

static void Mail_RequestRun(void)
{
	if (!G_MailLock) return;
	xrtMutexLock(G_MailLock);
	G_MailForceRun = true;
	xrtMutexUnlock(G_MailLock);
}

static bool Mail_PeekForceRun(void)
{
	bool value = false;
	if (!G_MailLock) return false;
	xrtMutexLock(G_MailLock);
	value = G_MailForceRun;
	G_MailForceRun = false;
	xrtMutexUnlock(G_MailLock);
	return value;
}

static void Mail_SetRunning(bool running)
{
	if (!G_MailLock) return;
	xrtMutexLock(G_MailLock);
	G_MailRunning = running;
	xrtMutexUnlock(G_MailLock);
}

static bool Mail_IsRunning(void)
{
	bool value = false;
	if (!G_MailLock) return false;
	xrtMutexLock(G_MailLock);
	value = G_MailRunning;
	xrtMutexUnlock(G_MailLock);
	return value;
}

/* ---- 任务记录 ---- */

static void Mail_UpdateTaskResult(sqlite3* db, int64 taskId, const char* status,
	int64 retryCount, int64 nextRetryAt, int64 sendTime, const char* errorMessage)
{
	sqlite3_stmt* stmt = NULL;

	if (sqlite3_prepare_v3(db,
		"UPDATE mail_task SET status = ?, retryCount = ?, nextRetryAt = ?, sendTime = ?, errorMessage = ?, updateTime = ? WHERE id = ?",
		-1, 0, &stmt, NULL) != SQLITE_OK) return;
	sqlite3_bind_text(stmt, 1, status, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 2, retryCount);
	sqlite3_bind_int64(stmt, 3, nextRetryAt);
	sqlite3_bind_int64(stmt, 4, sendTime);
	sqlite3_bind_text(stmt, 5, errorMessage ? errorMessage : "", -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 6, XAdmin_UnixNowUs());
	sqlite3_bind_int64(stmt, 7, taskId);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	if (sqlite3_prepare_v3(db,
		"INSERT INTO mail_log (taskId, memberId, toEmail, status, responseText, errorMessage, createTime) "
		"SELECT id, memberId, toEmail, ?, '', ?, ? FROM mail_task WHERE id = ?",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_text(stmt, 1, status, -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmt, 2, errorMessage ? errorMessage : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 3, XAdmin_UnixNowUs());
		sqlite3_bind_int64(stmt, 4, taskId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
}

/* 领取：pending→sending（原子防双发） */
static bool Mail_ClaimTask(sqlite3* db, int64 taskId)
{
	sqlite3_stmt* stmt = NULL;
	bool ok = false;

	if (sqlite3_prepare_v3(db,
		"UPDATE mail_task SET status = 'sending', updateTime = ? WHERE id = ? AND status = 'pending'",
		-1, 0, &stmt, NULL) != SQLITE_OK) return false;
	sqlite3_bind_int64(stmt, 1, XAdmin_UnixNowUs());
	sqlite3_bind_int64(stmt, 2, taskId);
	ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0;
	sqlite3_finalize(stmt);
	return ok;
}

/* 卡死恢复：sending 超过超时×2 视为进程中断，复位为 pending */
static void Mail_RecoverStale(sqlite3* db)
{
	sqlite3_stmt* stmt = NULL;
	int64 staleAfter = Mail_GetInt("smtp_timeout_sec", 15) * 2;
	int64 cutoff = XAdmin_UnixNowUs() - staleAfter * 1000000;

	if (sqlite3_prepare_v3(db,
		"UPDATE mail_task SET status = 'pending', updateTime = ? WHERE status = 'sending' AND updateTime < ?",
		-1, 0, &stmt, NULL) != SQLITE_OK) return;
	sqlite3_bind_int64(stmt, 1, XAdmin_UnixNowUs());
	sqlite3_bind_int64(stmt, 2, cutoff);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

/* ---- 队列执行（独立线程内调用，独立 DB 连接） ---- */

static bool Mail_RunBatch(sqlite3* db, int64 limit, bool ignoreRetryAt,
	int64* successCount, int64* failCount, char* message, size_t messageCap)
{
	sqlite3_stmt* stmt = NULL;
	MailRuntime rt;
	int64 retryInterval = Mail_GetInt("mail_retry_interval_sec", 300);
	int64 processed = 0;
	int64 success = 0, fail = 0;
	xtime now = XAdmin_UnixNowUs();
	char firstError[192] = {0};

	if (successCount) *successCount = 0;
	if (failCount) *failCount = 0;
	if (messageCap) message[0] = '\0';
	if (!Mail_Enabled()) {
		snprintf(message, messageCap, "邮件功能未启用");
		return false;
	}
	if (!Mail_GetBool("mail_queue_enabled", true)) {
		snprintf(message, messageCap, "邮件队列未启用");
		return false;
	}
	Mail_RecoverStale(db);

	/* 到期检查 */
	if (sqlite3_prepare_v3(db,
		ignoreRetryAt
		? "SELECT id, toEmail, subject, htmlBody, textBody, retryCount, maxRetryCount FROM mail_task WHERE status = 'pending' ORDER BY id ASC LIMIT ?"
		: "SELECT id, toEmail, subject, htmlBody, textBody, retryCount, maxRetryCount FROM mail_task WHERE status = 'pending' AND nextRetryAt <= ? ORDER BY id ASC LIMIT ?",
		-1, 0, &stmt, NULL) != SQLITE_OK) {
		snprintf(message, messageCap, "%s", sqlite3_errmsg(db));
		return false;
	}
	if (ignoreRetryAt) {
		sqlite3_bind_int64(stmt, 1, limit);
	} else {
		sqlite3_bind_int64(stmt, 1, now);
		sqlite3_bind_int64(stmt, 2, limit);
	}

	if (!Mail_RuntimeInit(&rt)) {
		snprintf(message, messageCap, "%s", rt.error);
		sqlite3_finalize(stmt);
		return false;
	}
	{
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			int64 taskId = sqlite3_column_int64(stmt, 0);
			const char* toEmail = (const char*)sqlite3_column_text(stmt, 1);
			const char* subject = (const char*)sqlite3_column_text(stmt, 2);
			const char* htmlBody = (const char*)sqlite3_column_text(stmt, 3);
			const char* textBody = (const char*)sqlite3_column_text(stmt, 4);
			int64 retryCount = sqlite3_column_int64(stmt, 5);
			int64 maxRetry = sqlite3_column_int64(stmt, 6);
			char sendError[192];
			char* finalSubject;
			bool sent;

			if (!Mail_ClaimTask(db, taskId)) continue;
			processed++;
			finalSubject = Mail_BuildSubject(subject);
			sendError[0] = '\0';
			sent = Mail_SendOne(&rt.client, toEmail ? toEmail : "", finalSubject,
				htmlBody, textBody, sendError, sizeof(sendError));
			xrtFree(finalSubject);
			if (sent) {
				Mail_UpdateTaskResult(db, taskId, "success", retryCount, 0, XAdmin_UnixNowUs(), "");
				success++;
			} else {
				bool willRetry = (retryCount + 1 < (maxRetry > 0 ? maxRetry : 3));
				int64 nextRetryAt = willRetry ? (XAdmin_UnixNowUs() + retryInterval * 1000000) : 0;
				Mail_UpdateTaskResult(db, taskId, willRetry ? "pending" : "fail",
					retryCount + 1, nextRetryAt, 0, sendError);
				if (!firstError[0]) snprintf(firstError, sizeof(firstError), "%s", sendError);
				fail++;
			}
		}
	}
	sqlite3_finalize(stmt);
	Mail_RuntimeUnit(&rt);

	if (successCount) *successCount = success;
	if (failCount) *failCount = fail;
	if (processed == 0) {
		snprintf(message, messageCap, "没有待发送的邮件任务");
	} else if (firstError[0]) {
		snprintf(message, messageCap, "成功 %lld 条，失败 %lld 条，首错：%s",
			(long long)success, (long long)fail, firstError);
	} else {
		snprintf(message, messageCap, "成功 %lld 条，失败 %lld 条",
			(long long)success, (long long)fail);
	}
	return true;
}

/* ---- 队列线程 ---- */

static int32 Mail_ThreadProc(ptr param)
{
	sqlite3* db = Sched_OpenStandaloneDB();
	int64 lastRun = 0;

	(void)param;
	if (!db) return 1;
	while (true) {
		int64 interval;
		bool force;

		xrtMutexLock(G_MailLock);
		if (G_MailStop) { xrtMutexUnlock(G_MailLock); break; }
		xrtMutexUnlock(G_MailLock);

		interval = Mail_ScanInterval();
		force = Mail_PeekForceRun();
		if (force || lastRun == 0 || XAdmin_UnixNowUs() >= lastRun + interval * 1000000) {
			if (Mail_Enabled() && Mail_GetBool("mail_queue_enabled", true)) {
				int64 success = 0, fail = 0;
				char message[256];
				Mail_SetRunning(true);
				Mail_RunBatch(db, Mail_BatchLimit(interval), force, &success, &fail, message, sizeof(message));
				Mail_SetRunning(false);
				lastRun = XAdmin_UnixNowUs();
				if (success > 0 || fail > 0)
					printf("[mail_queue] success=%lld fail=%lld %s\n", (long long)success, (long long)fail, message);
				/* 一次只消化一个 force 请求 */
				continue;
			}
			lastRun = XAdmin_UnixNowUs();
		}
		xrtSleep(1000);
	}
	sqlite3_close(db);
	return 0;
}

/* ---- 路由支撑（CRUD） ---- */

static xvalue* Mail_AdminList(int64 page, int64 limit, const char* search, int64 taskId, int64* outCount)
{
	sqlite3_stmt* stmt = NULL, * stmtCount = NULL;
	int64 offset;
	xvalue* data = ValueArray();
	int64 count = 0;
	int base = 1;
	char tail[256] = {0};

	if (outCount) *outCount = 0;
	if (page <= 0) page = 1;
	if (limit <= 0) limit = 20;
	offset = (page - 1) * limit;

	if (taskId > 0) {
		snprintf(tail, sizeof(tail), " AND id = %lld", (long long)taskId);
	} else if (search && search[0]) {
		snprintf(tail, sizeof(tail), " AND (toEmail LIKE '%%%s%%' OR subject LIKE '%%%s%%' OR errorMessage LIKE '%%%s%%')",
			search, search, search);
	}
	{
		char sql[1024];
		snprintf(sql, sizeof(sql),
			"SELECT id, memberId, toEmail, subject, status, retryCount, maxRetryCount, createTime, sendTime, errorMessage "
			"FROM mail_task WHERE 1=1%s ORDER BY id DESC LIMIT ? OFFSET ?", tail);
		if (sqlite3_prepare_v3(G_DB, sql, -1, 0, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_int(stmt, base++, (int)limit);
			sqlite3_bind_int(stmt, base++, (int)offset);
			while (sqlite3_step(stmt) == SQLITE_ROW) {
				xvalue* row = ValueObject();
				ValueSetInt(row, "id", sqlite3_column_int64(stmt, 0));
				ValueSetInt(row, "memberId", sqlite3_column_int64(stmt, 1));
				ValueSetText(row, "toEmail", (const char*)sqlite3_column_text(stmt, 2));
				ValueSetText(row, "subject", (const char*)sqlite3_column_text(stmt, 3));
				ValueSetText(row, "status", (const char*)sqlite3_column_text(stmt, 4));
				ValueSetInt(row, "retryCount", sqlite3_column_int64(stmt, 5));
				ValueSetInt(row, "maxRetryCount", sqlite3_column_int64(stmt, 6));
				ValueSetOwnedText(row, "createTime", TimeText(sqlite3_column_int64(stmt, 7), TIME_TEXT_DATETIME));
				{
					int64 sendTime = sqlite3_column_int64(stmt, 8);
					if (sendTime > 0)
						ValueSetOwnedText(row, "sendTime", TimeText(sendTime, TIME_TEXT_DATETIME));
					else
						ValueSetText(row, "sendTime", "-");
				}
				ValueSetText(row, "errorMessage", (const char*)sqlite3_column_text(stmt, 9));
				ValueArrayOwn(data, row);
			}
			sqlite3_finalize(stmt);
		}
		snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM mail_task WHERE 1=1%s", tail);
		if (sqlite3_prepare_v3(G_DB, sql, -1, 0, &stmtCount, NULL) == SQLITE_OK) {
			if (sqlite3_step(stmtCount) == SQLITE_ROW) count = sqlite3_column_int64(stmtCount, 0);
			sqlite3_finalize(stmtCount);
		}
	}
	if (outCount) *outCount = count;
	return data;
}

static xvalue* Mail_QueueStatus(void)
{
	xvalue* status = ValueObject();
	sqlite3_stmt* stmt = NULL;
	int64 interval = Mail_ScanInterval();
	int64 pending = 0;
	int64 concurrency = Mail_GetInt("mail_queue_concurrency", 5);

	if (concurrency < 1) concurrency = 1;
	if (concurrency > 32) concurrency = 32;
	if (sqlite3_prepare_v3(G_DB,
		"SELECT COUNT(*) FROM mail_task WHERE status = 'pending' AND nextRetryAt <= ?",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, XAdmin_UnixNowUs());
		if (sqlite3_step(stmt) == SQLITE_ROW) pending = sqlite3_column_int64(stmt, 0);
		sqlite3_finalize(stmt);
	}
	ValueSetBool(status, "enabled", Mail_Enabled());
	ValueSetBool(status, "queueEnabled", Mail_GetBool("mail_queue_enabled", true));
	ValueSetInt(status, "scanIntervalSec", interval);
	ValueSetInt(status, "ratePerMinute", Mail_GetInt("mail_rate_limit_per_minute", 60));
	ValueSetInt(status, "batchLimit", Mail_BatchLimit(interval));
	ValueSetInt(status, "concurrency", concurrency);
	ValueSetInt(status, "pendingCount", pending);
	ValueSetBool(status, "running", Mail_IsRunning());
	if (G_MailLock) {
		xrtMutexLock(G_MailLock);
		ValueSetInt(status, "lastSuccess", G_MailLastSuccess);
		ValueSetInt(status, "lastFail", G_MailLastFail);
		ValueSetInt(status, "lastRunTime", G_MailLastRun);
		xrtMutexUnlock(G_MailLock);
	}
	return status;
}

static bool Mail_CreateTasks(const char* sendType, const char* memberIds, const char* groupIds,
	const char* subject, const char* content, str* outMessage, int64* created, int64* skipped)
{
	xvalue* recipients = NULL;
	sqlite3_stmt* stmt = NULL;
	int64 maxRetry = Mail_GetInt("mail_max_retry", 3);
	int64 made = 0, skip = 0;
	bool ok = false;

	if (outMessage) *outMessage = NULL;
	if (created) *created = 0;
	if (skipped) *skipped = 0;
	if (!Mail_Enabled()) {
		if (outMessage) *outMessage = xrtStrDup("邮件功能未启用，请先在全局配置中开启");
		return false;
	}
	if (!subject || !subject[0] || !content || !content[0]) {
		if (outMessage) *outMessage = xrtStrDup("邮件主题和内容不能为空");
		return false;
	}
	recipients = Notify_BuildRecipients(sendType, memberIds, groupIds);
	if (!recipients) {
		if (outMessage) *outMessage = xrtStrDup("收件人构建失败");
		return false;
	}
	if (sqlite3_prepare_v3(G_DB,
		"INSERT INTO mail_task (memberId, toEmail, templateCode, subject, htmlBody, textBody, payloadJson, sourcePlugin, bizType, bizId, status, retryCount, maxRetryCount, nextRetryAt, errorMessage, createTime, updateTime, sendTime) "
		"VALUES (?, ?, '', ?, '', ?, '', '', '', 0, 'pending', 0, ?, 0, '', ?, ?, 0)",
		-1, 0, &stmt, NULL) != SQLITE_OK) {
		if (outMessage) *outMessage = xrtStrDup(sqlite3_errmsg(G_DB));
		xrtValueRelease(recipients);
		return false;
	}
	sqlite3_exec(G_DB, "BEGIN TRANSACTION", NULL, NULL, NULL);
	{
		size_t i, count = ValueCount(recipients);
		for (i = 0; i < count; i++) {
			xvalue* row = xrtValueArrayGet(recipients, i);
			const char* email = ValueText(row, "email") ? ValueText(row, "email") : "";
			if (!Mail_EmailValid(email)) {
				skip++;
				continue;
			}
			sqlite3_bind_int64(stmt, 1, ValueInt(row, "memberId"));
			sqlite3_bind_text(stmt, 2, email, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, subject, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, content, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 5, maxRetry);
			sqlite3_bind_int64(stmt, 6, XAdmin_UnixNowUs());
			sqlite3_bind_int64(stmt, 7, XAdmin_UnixNowUs());
			if (sqlite3_step(stmt) == SQLITE_DONE) made++;
			sqlite3_reset(stmt);
			sqlite3_clear_bindings(stmt);
		}
	}
	sqlite3_exec(G_DB, "COMMIT", NULL, NULL, NULL);
	sqlite3_finalize(stmt);
	xrtValueRelease(recipients);
	if (created) *created = made;
	if (skipped) *skipped = skip;
	if (made == 0) {
		if (outMessage) *outMessage = xrtStrDup("没有匹配到可用邮箱的收件人");
		return false;
	}
	if (outMessage) *outMessage = xrtFormat("已创建 %lld 个邮件任务，跳过 %lld 个无效邮箱",
		(long long)made, (long long)skip);
	return true;
}

static bool Mail_RetryTask(int64 taskId, str* outMessage)
{
	sqlite3_stmt* stmt = NULL;
	char status[24] = {0};

	if (outMessage) *outMessage = NULL;
	if (taskId <= 0) {
		if (outMessage) *outMessage = xrtStrDup("邮件任务 ID 无效");
		return false;
	}
	if (sqlite3_prepare_v3(G_DB, "SELECT status FROM mail_task WHERE id = ? LIMIT 1",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, taskId);
		if (sqlite3_step(stmt) == SQLITE_ROW) {
			const char* s = (const char*)sqlite3_column_text(stmt, 0);
			if (s) snprintf(status, sizeof(status), "%s", s);
		}
		sqlite3_finalize(stmt);
	}
	if (!status[0]) {
		if (outMessage) *outMessage = xrtStrDup("邮件任务不存在");
		return false;
	}
	if (strcmp(status, "sending") == 0) {
		if (outMessage) *outMessage = xrtStrDup("邮件任务正在发送中，暂时不能重试");
		return false;
	}
	if (strcmp(status, "fail") != 0 && strcmp(status, "pending") != 0) {
		if (outMessage) *outMessage = xrtStrDup("仅失败任务支持手动重试");
		return false;
	}
	if (sqlite3_prepare_v3(G_DB,
		"UPDATE mail_task SET status = 'pending', nextRetryAt = 0, updateTime = ?, errorMessage = '' WHERE id = ?",
		-1, 0, &stmt, NULL) != SQLITE_OK) {
		if (outMessage) *outMessage = xrtStrDup(sqlite3_errmsg(G_DB));
		return false;
	}
	sqlite3_bind_int64(stmt, 1, XAdmin_UnixNowUs());
	sqlite3_bind_int64(stmt, 2, taskId);
	{
		bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(G_DB) > 0;
		sqlite3_finalize(stmt);
		if (ok) {
			if (outMessage) *outMessage = xrtStrDup("邮件任务已重新加入待发送队列");
			Mail_RequestRun();
			return true;
		}
		if (outMessage) *outMessage = xrtStrDup("邮件任务重试失败");
		return false;
	}
}

static bool Mail_DeleteTask(int64 taskId, str* outMessage)
{
	sqlite3_stmt* stmt = NULL;

	if (outMessage) *outMessage = NULL;
	if (taskId <= 0) {
		if (outMessage) *outMessage = xrtStrDup("邮件任务 ID 无效");
		return false;
	}
	if (sqlite3_prepare_v3(G_DB, "DELETE FROM mail_task WHERE id = ? AND status != 'sending'",
		-1, 0, &stmt, NULL) != SQLITE_OK) {
		if (outMessage) *outMessage = xrtStrDup(sqlite3_errmsg(G_DB));
		return false;
	}
	sqlite3_bind_int64(stmt, 1, taskId);
	{
		bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(G_DB) > 0;
		sqlite3_finalize(stmt);
		if (ok) {
			if (sqlite3_prepare_v3(G_DB, "DELETE FROM mail_log WHERE taskId = ?", -1, 0, &stmt, NULL) == SQLITE_OK) {
				sqlite3_bind_int64(stmt, 1, taskId);
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);
			}
			if (outMessage) *outMessage = xrtStrDup("邮件任务已删除");
			return true;
		}
		if (outMessage) *outMessage = xrtStrDup("邮件任务不存在或正在发送中");
		return false;
	}
}

static bool Mail_TestSmtp(const char* toEmail, str* outMessage)
{
	MailRuntime rt;
	char subject[128];
	char body[512];
	bool ok = false;

	if (outMessage) *outMessage = NULL;
	if (!toEmail || !toEmail[0]) toEmail = Mail_GetText("mail_from_email", "");
	if (!Mail_EmailValid(toEmail)) {
		if (outMessage) *outMessage = xrtStrDup("测试收件邮箱格式无效");
		return false;
	}
	if (!Mail_RuntimeInit(&rt)) {
		if (outMessage) *outMessage = xrtStrDup(rt.error);
		return false;
	}
	{
		char* now = TimeText(XAdmin_UnixNowUs(), TIME_TEXT_DATETIME);
		snprintf(subject, sizeof(subject), "[xadmin] SMTP 测试 %s", now ? now : "");
		snprintf(body, sizeof(body), "这是一封来自 xadmin 的 SMTP 测试邮件。\r\n时间：%s\r\n主机：%s:%d",
			now ? now : "", Mail_GetText("smtp_host", ""), (int)rt.client.Net.Port);
		xrtFree(now);
	}
	{
		char error[192];
		error[0] = '\0';
		ok = Mail_SendOne(&rt.client, toEmail, subject, "", body, error, sizeof(error));
		if (ok) {
			if (outMessage) *outMessage = xrtFormat("测试邮件已发送到 %s", toEmail);
		} else {
			if (outMessage) *outMessage = xrtFormat("测试发信失败：%s", error);
		}
	}
	Mail_RuntimeUnit(&rt);
	return ok;
}

/* ---- 初始化 / 卸载 ---- */

void Mail_Init(void)
{
	/* v1 契约：自装"邮件任务"菜单（幂等，href 命中即复用） */
	{
		sqlite3_stmt* stmt = NULL;
		int menuId = 0;
		int64 now = XAdmin_UnixNowUs();
		if (sqlite3_prepare_v2(G_DB, "SELECT id FROM menu WHERE isDelete=0 AND href='/admin/view/member/mail' LIMIT 1", -1, &stmt, NULL) == SQLITE_OK) {
			if (sqlite3_step(stmt) == SQLITE_ROW) menuId = sqlite3_column_int(stmt, 0);
			sqlite3_finalize(stmt);
		}
		if (!menuId) {
			if (sqlite3_prepare_v2(G_DB, "INSERT INTO menu (parent,title,icon,type,openType,href,sort,visible,remark,createTime,updateTime,isDelete) SELECT 0,'邮件任务','layui-icon layui-icon-email',1,'_iframe','/admin/view/member/mail',401700,1,'前台用户邮件任务与发送记录',?,?,0", -1, &stmt, NULL) == SQLITE_OK) {
				sqlite3_bind_int64(stmt, 1, now);
				sqlite3_bind_int64(stmt, 2, now);
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);
			}
		}
	}
	printf("        Mail_Init \n");
	Notify_ExecSQLIgnore("CREATE TABLE IF NOT EXISTS mail_template (id INTEGER PRIMARY KEY AUTOINCREMENT, code TEXT UNIQUE, name TEXT DEFAULT '', sourcePlugin TEXT DEFAULT '', subjectTpl TEXT DEFAULT '', htmlTpl TEXT DEFAULT '', textTpl TEXT DEFAULT '', enabled INTEGER DEFAULT 1, remark TEXT DEFAULT '', createTime INTEGER DEFAULT 0, updateTime INTEGER DEFAULT 0)");
	Notify_ExecSQLIgnore("CREATE TABLE IF NOT EXISTS mail_task (id INTEGER PRIMARY KEY AUTOINCREMENT, memberId INTEGER DEFAULT 0, toEmail TEXT DEFAULT '', templateCode TEXT DEFAULT '', subject TEXT DEFAULT '', htmlBody TEXT DEFAULT '', textBody TEXT DEFAULT '', payloadJson TEXT DEFAULT '', sourcePlugin TEXT DEFAULT '', bizType TEXT DEFAULT '', bizId INTEGER DEFAULT 0, status TEXT DEFAULT 'pending', retryCount INTEGER DEFAULT 0, maxRetryCount INTEGER DEFAULT 0, nextRetryAt INTEGER DEFAULT 0, errorMessage TEXT DEFAULT '', createTime INTEGER DEFAULT 0, updateTime INTEGER DEFAULT 0, sendTime INTEGER DEFAULT 0)");
	Notify_ExecSQLIgnore("CREATE TABLE IF NOT EXISTS mail_log (id INTEGER PRIMARY KEY AUTOINCREMENT, taskId INTEGER DEFAULT 0, memberId INTEGER DEFAULT 0, toEmail TEXT DEFAULT '', status TEXT DEFAULT '', responseText TEXT DEFAULT '', errorMessage TEXT DEFAULT '', createTime INTEGER DEFAULT 0)");
	Notify_ExecSQLIgnore("CREATE INDEX IF NOT EXISTS idx_mail_task_due ON mail_task(status, nextRetryAt)");

	G_MailLock = xrtMutexCreate();
	G_MailStop = false;
	G_MailRunning = false;
	G_MailForceRun = false;
	G_MailThread = xrtThreadCreate(Mail_ThreadProc, NULL, 0);
}

void Mail_Unit(void)
{
	printf("        Mail_Unit \n");
	size_t i;

	if (G_MailLock) {
		xrtMutexLock(G_MailLock);
		G_MailStop = true;
		xrtMutexUnlock(G_MailLock);
	}
	if (G_MailThread) {
		xrtThreadWait(G_MailThread);
		xrtThreadDestroy(G_MailThread);
		G_MailThread = NULL;
	}
	/* 队列线程每秒轮询，归零窗口极短；上限兜底 */
	for (i = 0; i < 30 && Mail_IsRunning(); i++) xrtSleep(1000);
	if (G_MailLock) { xrtMutexDestroy(G_MailLock); G_MailLock = NULL; }
}

#endif /* XADMIN_WITH_SMTP */
