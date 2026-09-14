/* 计划任务核心：cron/间隔/单次三种调度、skip/queue_one/parallel 重叠策略、
 * misfire 策略、失败重试、shell 与 C 双执行器（C 任务以独立 xs.exe 子进程
 * 运行生成的 runner）、运行日志与系统 tick 钩子。时间统一微秒。 */

#define SCHED_YEAR_BASE 2000
#define SCHED_YEAR_SPAN 100
#define SCHED_WORKER_MAX 16

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

typedef struct SchedTaskSnapshot {
	int64 id;
	bool enabled;
	str sName;
	str sScheduleType;   /* once / interval / cron */
	str sExecType;       /* shell / c */
	str sShellType;      /* cmd / powershell / sh */
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
	str sOverlapPolicy;  /* skip / queue_one / parallel */
	str sMisfirePolicy;  /* skip / run_once */
	str sWorkDir;
	int isRunning;
	int runningCount;
	int pendingRun;
	int parallelLimit;
	int retryCount;
	int retryDelaySec;
	int retryState;
} SchedTaskSnapshot;

typedef struct SchedWorkerSlot {
	xthread* thread;
	int64 taskId;
	bool busy;
} SchedWorkerSlot;

static xthread* G_SchedThread;
static xmutex* G_SchedLock;
static xcond* G_SchedCond;
static volatile bool G_SchedStop;
static int G_SchedWorkerCount;
static SchedWorkerSlot G_SchedWorkers[SCHED_WORKER_MAX];
static char* G_SchedPath;
static char* G_SchedCachePath;
static char* G_SchedXSPath;

/* ---- 小工具 ---- */

static bool Sched_TextEquals(const char* a, const char* b)
{
	if (a == NULL || b == NULL) return !a && !b;
	return strcmp(a, b) == 0;
}

static const char* Sched_CStrOrEmpty(const char* s)
{
	return s ? s : "";
}

static str Sched_CopyText(const char* s)
{
	return xrtStrDup(s ? s : "");
}

static const char* Sched_SQLiteTextOrEmpty(sqlite3_stmt* stmt, int col)
{
	const unsigned char* text = sqlite3_column_text(stmt, col);
	return text ? (const char*)text : "";
}

static void Sched_FreeTaskSnapshot(SchedTaskSnapshot* task)
{
	if (!task) return;
	xrtFree(task->sName); xrtFree(task->sScheduleType); xrtFree(task->sExecType);
	xrtFree(task->sShellType); xrtFree(task->sCodeText); xrtFree(task->sCustomText);
	xrtFree(task->sCronExpr); xrtFree(task->sIntervalUnit); xrtFree(task->sOverlapPolicy);
	xrtFree(task->sMisfirePolicy); xrtFree(task->sWorkDir);
	memset(task, 0, sizeof(*task));
}

static sqlite3* Sched_OpenStandaloneDB(void)
{
	sqlite3* db = NULL;
	char* file = xrtPathJoin(DBPath, "main.db");
	if (!file) return NULL;
	sqlite3_open_v2(file, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, NULL);
	xrtFree(file);
	return db;
}

static int64 Sched_IntervalSeconds(int64 value, const char* unit)
{
	if (value <= 0) return 0;
	if (Sched_TextEquals(unit, "second")) return value;
	if (Sched_TextEquals(unit, "minute")) return value * 60;
	if (Sched_TextEquals(unit, "hour")) return value * 3600;
	if (Sched_TextEquals(unit, "day")) return value * 86400;
	return 0;
}

static const char* Sched_GetWorkDir(const SchedTaskSnapshot* task)
{
	return task->sWorkDir && task->sWorkDir[0] ? task->sWorkDir : G_SchedPath;
}

static uint32 Sched_GetTimeoutMs(const SchedTaskSnapshot* task)
{
	return task->timeoutSec > 0 ? (uint32)task->timeoutSec * 1000 : 0;
}

/* ---- cron 解析（v1 语义：7 字段，DOM/DOW 通配时互斥退化） ---- */

static char* Sched_StrTokNext(char* text, const char* delim, char** context)
{
	char* begin;
	char* end;

	if (!context || !delim) return NULL;
	if (text) *context = text;
	begin = *context;
	if (!begin) return NULL;
	begin += strspn(begin, delim);
	if (!begin[0]) { *context = NULL; return NULL; }
	end = begin + strcspn(begin, delim);
	if (!end[0]) *context = NULL;
	else { *end = '\0'; *context = end + 1; }
	return begin;
}

static bool Sched_ParseCronNumber(const char* text, int min, int max, bool weekday, int* value)
{
	int v;

	if (!text || !text[0]) return false;
	v = atoi(text);
	if (weekday && v == 7) v = 0;
	if (v < min || v > max) return false;
	*value = v;
	return true;
}

static bool Sched_ParseCronField(const char* field, bool* allowed, int min, int max, bool* wildcard, bool weekday)
{
	char* copy;
	char* part;
	char* context = NULL;

	if (!field || !field[0]) return false;
	memset(allowed, 0, (size_t)(max + 1) * sizeof(bool));
	*wildcard = false;
	copy = xrtStrDup(field);
	part = Sched_StrTokNext(copy, ",", &context);
	while (part) {
		char* slash = strchr(part, '/');
		char* dash = strchr(part, '-');
		int step = 1, start = 0, end = 0;

		if (slash) {
			*slash = '\0';
			step = atoi(slash + 1);
			if (step <= 0) { xrtFree(copy); return false; }
		}
		if (strcmp(part, "*") == 0) {
			start = min; end = max; *wildcard = true;
		} else if (dash) {
			int vs, ve;
			*dash = '\0';
			if (!Sched_ParseCronNumber(part, min, max, weekday, &vs)
				|| !Sched_ParseCronNumber(dash + 1, min, max, weekday, &ve) || ve < vs) {
				xrtFree(copy);
				return false;
			}
			start = vs; end = ve;
		} else {
			int v;
			if (!Sched_ParseCronNumber(part, min, max, weekday, &v)) { xrtFree(copy); return false; }
			start = end = v;
		}
		{
			int i;
			for (i = start; i <= end; i += step) allowed[i] = true;
		}
		part = Sched_StrTokNext(NULL, ",", &context);
	}
	xrtFree(copy);
	return true;
}

static bool Sched_ParseCronYearField(const char* field, SchedCronExpr* cron)
{
	char* copy;
	char* part;
	char* context = NULL;

	memset(cron->year, 0, sizeof(cron->year));
	if (!field || !field[0] || strcmp(field, "*") == 0) {
		cron->anyYear = true;
		memset(cron->year, 1, sizeof(cron->year));
		return true;
	}
	copy = xrtStrDup(field);
	part = Sched_StrTokNext(copy, ",", &context);
	while (part) {
		char* slash = strchr(part, '/');
		char* dash = strchr(part, '-');
		int step = 1, start, end;

		if (slash) {
			*slash = '\0';
			step = atoi(slash + 1);
			if (step <= 0) { xrtFree(copy); return false; }
		}
		if (strcmp(part, "*") == 0) {
			start = SCHED_YEAR_BASE;
			end = SCHED_YEAR_BASE + SCHED_YEAR_SPAN - 1;
		} else if (dash) {
			*dash = '\0';
			start = atoi(part);
			end = atoi(dash + 1);
			if (end < start) { xrtFree(copy); return false; }
		} else {
			start = end = atoi(part);
		}
		if (start < SCHED_YEAR_BASE) start = SCHED_YEAR_BASE;
		if (end > SCHED_YEAR_BASE + SCHED_YEAR_SPAN - 1) end = SCHED_YEAR_BASE + SCHED_YEAR_SPAN - 1;
		{
			int y;
			for (y = start; y <= end; y += step) cron->year[y - SCHED_YEAR_BASE] = true;
		}
		part = Sched_StrTokNext(NULL, ",", &context);
	}
	xrtFree(copy);
	cron->anyYear = false;
	return true;
}

static bool Sched_ParseCronExpr(const char* expr, SchedCronExpr* cron)
{
	char* copy;
	char* token;
	char* context = NULL;
	char* fields[7] = {0};
	int count = 0;
	bool wildcard = false;

	if (!expr || !expr[0] || !cron) return false;
	memset(cron, 0, sizeof(*cron));
	copy = xrtStrDup(expr);
	token = Sched_StrTokNext(copy, " \t\r\n", &context);
	while (token && count < 7) {
		fields[count++] = token;
		token = Sched_StrTokNext(NULL, " \t\r\n", &context);
	}
	if (token || (count != 6 && count != 7)) { xrtFree(copy); return false; }
	if (!Sched_ParseCronField(fields[0], cron->second, 0, 59, &wildcard, false)
		|| !Sched_ParseCronField(fields[1], cron->minute, 0, 59, &wildcard, false)
		|| !Sched_ParseCronField(fields[2], cron->hour, 0, 23, &wildcard, false)
		|| !Sched_ParseCronField(fields[3], cron->day, 1, 31, &cron->anyDay, false)
		|| !Sched_ParseCronField(fields[4], cron->month, 1, 12, &wildcard, false)
		|| !Sched_ParseCronField(fields[5], cron->weekday, 0, 6, &cron->anyWeekday, true)) {
		xrtFree(copy);
		return false;
	}
	if (count == 7) {
		if (!Sched_ParseCronYearField(fields[6], cron)) { xrtFree(copy); return false; }
	} else {
		cron->anyYear = true;
		memset(cron->year, 1, sizeof(cron->year));
	}
	xrtFree(copy);
	return true;
}

static bool Sched_CronYearAllowed(const SchedCronExpr* cron, int64 year)
{
	if (year < SCHED_YEAR_BASE || year >= SCHED_YEAR_BASE + SCHED_YEAR_SPAN) return false;
	if (cron->anyYear) return true;
	return cron->year[year - SCHED_YEAR_BASE];
}

/* v1 语义：纯前向扫描。回绕变体会让"字段耗尽 + 高位回退"形成
 * 0→60→0 死循环（攻击评审 SCH-4 实测），不得扩展。 */
static int Sched_NextAllowed(const bool* allowed, int min, int max, int current)
{
	int i;
	for (i = current > min ? current : min; i <= max; i++)
		if (allowed[i]) return i;
	return -1;
}

static bool Sched_CronDayMatches(const SchedCronExpr* cron, int day, int weekday)
{
	bool dayOk = cron->day[day];
	bool weekdayOk = cron->weekday[weekday];

	if (cron->anyDay && cron->anyWeekday) return true;
	if (cron->anyDay) return weekdayOk;
	if (cron->anyWeekday) return dayOk;
	return dayOk || weekdayOk;
}

/* 返回 expr 在 afterMicro 之后（本地日历）的下一个触发时刻（微秒），0 表示无解。 */
static int64 Sched_CalcCronNextTime(const char* expr, int64 afterMicro)
{
	SchedCronExpr cron;
	xdatetime date;
	int year, month, day, hour, minute, second;

	if (!Sched_ParseCronExpr(expr, &cron)) return 0;
	if (!xrtTimeLocal((afterMicro + 1) / 1000000, &date)) return 0;
	/* 从 after+1 微秒起步：先把秒进位到候选集合 */
	year = (int)date.Year; month = date.Month; day = date.Day;
	hour = date.Hour; minute = date.Minute; second = date.Second;

	{
		int guard = 0;
		while (year < SCHED_YEAR_BASE + SCHED_YEAR_SPAN) {
			int daysInMonth, next;
			if (++guard > 100000) return 0;   /* 畸形表达式的兜底（如全空字段） */

		if (month > 12) { year++; month = 1; day = 1; hour = 0; minute = 0; second = 0; continue; }
		if (!Sched_CronYearAllowed(&cron, year)) { year++; month = 1; day = 1; hour = 0; minute = 0; second = 0; continue; }

		next = Sched_NextAllowed(cron.month, 1, 12, month);
		if (next < 0) { year++; month = 1; day = 1; hour = 0; minute = 0; second = 0; continue; }
		if (next != month) { month = next; day = 1; hour = 0; minute = 0; second = 0; }

		daysInMonth = xrtDaysInMonth(year, month);
		if (day > daysInMonth) { month++; day = 1; hour = 0; minute = 0; second = 0; continue; }

		{
			bool matched = false;
			while (day <= daysInMonth) {
				xdatetime probe = {0};
				probe.Year = year; probe.Month = month; probe.Day = day;
				{
					xtime t;
					if (xrtTimeMake(&probe, &t)) {
						xdatetime local;
						if (xrtTimeLocal(t, &local) && Sched_CronDayMatches(&cron, day, local.Weekday % 7)) {
							matched = true;
							break;
						}
					}
				}
				day++; hour = 0; minute = 0; second = 0;
			}
			if (!matched) { month++; day = 1; hour = 0; minute = 0; second = 0; continue; }
		}

		next = Sched_NextAllowed(cron.hour, 0, 23, hour);
		if (next < 0) { day++; hour = 0; minute = 0; second = 0; continue; }
		if (next != hour) { hour = next; minute = 0; second = 0; }

		next = Sched_NextAllowed(cron.minute, 0, 59, minute);
		if (next < 0) { hour++; minute = 0; second = 0; continue; }
		if (next != minute) { minute = next; second = 0; }

		next = Sched_NextAllowed(cron.second, 0, 59, second);
		if (next < 0) { minute++; second = 0; continue; }
		second = next;

		{
			xdatetime when = {0};
			xtime t;
			when.Year = year; when.Month = month; when.Day = day;
			when.Hour = hour; when.Minute = minute; when.Second = second;
			if (!xrtTimeMake(&when, &t)) return 0;
			return xrtTimeUnix(t) * 1000000;
		}
	}
	}
	return 0;
}

/* ---- 下一跳计算 ---- */

static int64 Sched_CalcNextTime(const SchedTaskSnapshot* task, int64 baseMicro)
{
	if (Sched_TextEquals(task->sScheduleType, "once"))
		return task->onceAt > 0 ? task->onceAt : 0;
	if (Sched_TextEquals(task->sScheduleType, "interval")) {
		int64 seconds = Sched_IntervalSeconds(task->intervalValue, task->sIntervalUnit);
		if (seconds <= 0) return 0;
		{
			int64 anchor = task->startAt > 0 ? task->startAt : baseMicro;
			return anchor + seconds * 1000000;
		}
	}
	if (Sched_TextEquals(task->sScheduleType, "cron"))
		return Sched_CalcCronNextTime(task->sCronExpr, baseMicro);
	return 0;
}

static int64 Sched_CalcNextAfterRun(const SchedTaskSnapshot* task, int64 finishMicro)
{
	if (Sched_TextEquals(task->sScheduleType, "once")) return 0;
	if (Sched_TextEquals(task->sScheduleType, "interval")) {
		int64 seconds = Sched_IntervalSeconds(task->intervalValue, task->sIntervalUnit);
		if (seconds <= 0) return 0;
		return finishMicro + seconds * 1000000;
	}
	return Sched_CalcCronNextTime(task->sCronExpr, finishMicro);
}

static int64 Sched_CalcBusyNext(const SchedTaskSnapshot* task, int64 nowMicro)
{
	if (Sched_TextEquals(task->sScheduleType, "interval")) {
		int64 seconds = Sched_IntervalSeconds(task->intervalValue, task->sIntervalUnit);
		if (seconds <= 0) return 0;
		{
			int64 next = task->nextRunAt;
			while (next > 0 && next <= nowMicro) next += seconds * 1000000;
			return next;
		}
	}
	return Sched_CalcNextAfterRun(task, nowMicro);
}

static bool Sched_IsOverlapPolicy(const SchedTaskSnapshot* task, const char* policy)
{
	return Sched_TextEquals(task->sOverlapPolicy, policy);
}

static bool Sched_IsMisfirePolicy(const SchedTaskSnapshot* task, const char* policy)
{
	return Sched_TextEquals(task->sMisfirePolicy, policy);
}

static bool Sched_TaskCanRetry(const SchedTaskSnapshot* task, const char* finalStatus)
{
	if (task->retryCount <= 0) return false;
	if (Sched_TextEquals(finalStatus, "success")) return false;
	if (task->retryState >= task->retryCount) return false;
	return true;
}

/* ---- 快照装载 ---- */

static bool Sched_LoadTaskSnapshotById(sqlite3* db, int64 id, SchedTaskSnapshot* task)
{
	sqlite3_stmt* stmt = NULL;

	memset(task, 0, sizeof(*task));
	if (sqlite3_prepare_v3(db,
		"SELECT id, name, enabled, scheduleType, execType, shellType, codeText, customText, cronExpr, "
		"onceAt, intervalValue, intervalUnit, startAt, nextRunAt, lastRunAt, lastFinishAt, timeoutSec, "
		"overlapPolicy, misfirePolicy, workDir, isRunning, runningCount, pendingRun, parallelLimit, "
		"retryCount, retryDelaySec, retryState FROM sched_task WHERE id = ? AND isDelete = 0",
		-1, 0, &stmt, NULL) != SQLITE_OK)
		return false;
	sqlite3_bind_int64(stmt, 1, id);
	if (sqlite3_step(stmt) != SQLITE_ROW) {
		sqlite3_finalize(stmt);
		return false;
	}
	task->id = sqlite3_column_int64(stmt, 0);
	task->sName = Sched_CopyText(Sched_SQLiteTextOrEmpty(stmt, 1));
	task->enabled = sqlite3_column_int(stmt, 2) != 0;
	task->sScheduleType = Sched_CopyText(Sched_SQLiteTextOrEmpty(stmt, 3));
	task->sExecType = Sched_CopyText(Sched_SQLiteTextOrEmpty(stmt, 4));
	task->sShellType = Sched_CopyText(Sched_SQLiteTextOrEmpty(stmt, 5));
	task->sCodeText = Sched_CopyText(Sched_SQLiteTextOrEmpty(stmt, 6));
	task->sCustomText = Sched_CopyText(Sched_SQLiteTextOrEmpty(stmt, 7));
	task->sCronExpr = Sched_CopyText(Sched_SQLiteTextOrEmpty(stmt, 8));
	task->onceAt = sqlite3_column_int64(stmt, 9);
	task->intervalValue = sqlite3_column_int64(stmt, 10);
	task->sIntervalUnit = Sched_CopyText(Sched_SQLiteTextOrEmpty(stmt, 11));
	task->startAt = sqlite3_column_int64(stmt, 12);
	task->nextRunAt = sqlite3_column_int64(stmt, 13);
	task->lastRunAt = sqlite3_column_int64(stmt, 14);
	task->lastFinishAt = sqlite3_column_int64(stmt, 15);
	task->timeoutSec = sqlite3_column_int(stmt, 16);
	task->sOverlapPolicy = Sched_CopyText(Sched_SQLiteTextOrEmpty(stmt, 17));
	task->sMisfirePolicy = Sched_CopyText(Sched_SQLiteTextOrEmpty(stmt, 18));
	task->sWorkDir = Sched_CopyText(Sched_SQLiteTextOrEmpty(stmt, 19));
	task->isRunning = sqlite3_column_int(stmt, 20);
	task->runningCount = sqlite3_column_int(stmt, 21);
	task->pendingRun = sqlite3_column_int(stmt, 22);
	task->parallelLimit = sqlite3_column_int(stmt, 23);
	task->retryCount = sqlite3_column_int(stmt, 24);
	task->retryDelaySec = sqlite3_column_int(stmt, 25);
	task->retryState = sqlite3_column_int(stmt, 26);
	sqlite3_finalize(stmt);
	return task->id > 0;
}

static void Sched_UpdateTaskRunningFlag(sqlite3* db, int64 id, int running, const char* status, const char* message)
{
	sqlite3_stmt* stmt = NULL;

	if (sqlite3_prepare_v3(db,
		"UPDATE sched_task SET isRunning = ?, lastStatus = ?, lastMessage = ?, updateTime = ? WHERE id = ?",
		-1, 0, &stmt, NULL) != SQLITE_OK) return;
	sqlite3_bind_int(stmt, 1, running);
	sqlite3_bind_text(stmt, 2, status, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, message, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 4, xrtNow());
	sqlite3_bind_int64(stmt, 5, id);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

/* ---- 进程执行器（原生 xrtProcess：捕获输出 + 超时击杀进程树） ---- */

static bool Sched_RunProcessCommand(const char* commandLine, const char* workDir, uint32 timeoutMs,
	int* exitCode, int* timedOut, str* outStdout, str* outStderr, str* outMessage)
{
	xprocessconfig cfg;
	xprocess* proc = NULL;
	xprocessstatus status;
	xbuffer* out = xrtBufferCreate();
	xbuffer* err = xrtBufferCreate();
	unsigned char buf[8192];
	int64 n;
	bool timedOutFlag = false;
	bool ok = false;

	if (timedOut) *timedOut = 0;
	if (!xrtProcessShellConfigInit(&cfg, commandLine)) {
		if (outMessage) *outMessage = xrtStrDup("Failed to build process config");
		xrtBufferDestroy(out);
		xrtBufferDestroy(err);
		return false;
	}
	cfg.WorkDir = workDir && workDir[0] ? workDir : NULL;
	cfg.HideWindow = true;
	cfg.NewGroup = true;
	cfg.Stdin.Mode = XPROCESS_IO_NULL;
	cfg.Stdout.Mode = XPROCESS_IO_PIPE;
	cfg.Stderr.Mode = XPROCESS_IO_PIPE;
	proc = xrtProcessSpawn(&cfg);
	if (!proc) {
		if (outMessage) *outMessage = xrtStrDup("Failed to spawn process");
		xrtBufferDestroy(out);
		xrtBufferDestroy(err);
		return false;
	}
	if (timeoutMs > 0) {
		if (xrtProcessWaitFor(proc, (uint64)timeoutMs * 1000) != XWAIT_OK) {
			xrtProcessKillTree(proc);
			xrtProcessWait(proc);
			timedOutFlag = true;
		}
	} else {
		xrtProcessWait(proc);
	}
	while ((n = xrtProcessRead(proc, XPROCESS_STDOUT, buf, sizeof(buf))) > 0)
		xrtBufferAppend(out, (xbytesview){buf, (size_t)n});
	while ((n = xrtProcessRead(proc, XPROCESS_STDERR, buf, sizeof(buf))) > 0)
		xrtBufferAppend(err, (xbytesview){buf, (size_t)n});
	if (xrtProcessStatus(proc, &status)) {
		if (exitCode) *exitCode = status.Kind == XPROCESS_EXIT_CODE ? status.Code : -1;
		ok = !timedOutFlag && status.Kind == XPROCESS_EXIT_CODE;
	}
	{
		xbytesview v = xrtBufferView(out);
		char* text = xrtMalloc(v.Size + 1);
		memcpy(text, v.Data ? v.Data : (cbytes)"", v.Size);
		text[v.Size] = '\0';
		if (outStdout) *outStdout = text; else xrtFree(text);
	}
	{
		xbytesview v = xrtBufferView(err);
		char* text = xrtMalloc(v.Size + 1);
		memcpy(text, v.Data ? v.Data : (cbytes)"", v.Size);
		text[v.Size] = '\0';
		if (outStderr) *outStderr = text; else xrtFree(text);
	}
	if (timedOut) *timedOut = timedOutFlag ? 1 : 0;
	if (timedOutFlag) {
		if (outMessage) *outMessage = xrtFormat("Process timed out after %u ms", timeoutMs);
	} else if (!ok) {
		if (outMessage) *outMessage = xrtStrDup("Process did not exit normally");
	}
	xrtProcessDestroy(proc);
	xrtBufferDestroy(out);
	xrtBufferDestroy(err);
	return ok;
}

/* ---- shell 执行器 ---- */

static bool Sched_RunShellTask(const SchedTaskSnapshot* task, const char* triggerSource,
	int* exitCode, int* timedOut, str* outStdout, str* outStderr, str* outMessage)
{
	const char* ext;
	char* fileName;
	char* scriptPath;
	char* commandLine;
	bool ok;

	(void)triggerSource;
	if (!G_SchedCachePath) {
		if (outMessage) *outMessage = xrtStrDup("Scheduler cache path is missing");
		return false;
	}
	if (Sched_TextEquals(task->sShellType, "cmd")) ext = "bat";
	else if (Sched_TextEquals(task->sShellType, "powershell")) ext = "ps1";
	else ext = "sh";

	fileName = xrtFormat("task_%lld_%lld.%s", (long long)task->id, (long long)xrtNow(), ext);
	scriptPath = xrtPathJoin(G_SchedCachePath, fileName);
	xrtFree(fileName);
	if (!scriptPath
		|| !xrtFileWriteAtomic(scriptPath, (xbytesview){(cbytes)Sched_CStrOrEmpty(task->sCodeText), strlen(Sched_CStrOrEmpty(task->sCodeText))})) {
		xrtFree(scriptPath);
		if (outMessage) *outMessage = xrtStrDup("Failed to write script file");
		return false;
	}
#if defined(_WIN32) || defined(_WIN64)
	if (Sched_TextEquals(task->sShellType, "cmd"))
		commandLine = xrtFormat("cmd.exe /D /S /C \"%s\"", scriptPath);
	else if (Sched_TextEquals(task->sShellType, "powershell"))
		commandLine = xrtFormat("powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File \"%s\"", scriptPath);
	else
		commandLine = xrtFormat("sh \"%s\"", scriptPath);
#else
	commandLine = xrtFormat("sh \"%s\"", scriptPath);
#endif
	ok = Sched_RunProcessCommand(commandLine, Sched_GetWorkDir(task), Sched_GetTimeoutMs(task),
		exitCode, timedOut, outStdout, outStderr, outMessage);
	xrtFree(commandLine);
	xrtFileDelete(scriptPath);
	xrtFree(scriptPath);
	return ok;
}

/* ---- C 执行器：生成 runner 交独立 xs.exe 子进程运行 ---- */

/* JSON 内路径统一正斜杠，规避反斜杠转义问题 */
static str Sched_JsonPath(const char* path)
{
	str out = xrtStrDup(path ? path : "");
	char* p;
	for (p = out; *p; p++)
		if (*p == '\\') *p = '/';
	return out;
}

static str Sched_EscapeCString(const char* text)
{
	size_t n = 0, i;
	char* out;
	size_t j = 0;

	if (!text) return xrtStrDup("");
	for (i = 0; text[i]; i++) {
		switch (text[i]) {
		case '\"': case '\\': n += 2; break;
		case '\n': case '\r': case '\t': n += 2; break;
		default:
			if ((unsigned char)text[i] < 0x20) n += 4;
			else n++;
		}
	}
	out = xrtMalloc(n + 1);
	for (i = 0; text[i]; i++) {
		switch (text[i]) {
		case '\"': out[j++] = '\\'; out[j++] = '\"'; break;
		case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
		case '\n': out[j++] = '\\'; out[j++] = 'n'; break;
		case '\r': out[j++] = '\\'; out[j++] = 'r'; break;
		case '\t': out[j++] = '\\'; out[j++] = 't'; break;
		default:
			if ((unsigned char)text[i] < 0x20) j += (size_t)sprintf(out + j, "\\x%02x", (unsigned char)text[i]);
			else out[j++] = text[i];
		}
	}
	out[j] = '\0';
	return out;
}

static bool Sched_RunCTask(const SchedTaskSnapshot* task, const char* triggerSource,
	int* exitCode, int* timedOut, str* outStdout, str* outStderr, str* outMessage)
{
	char* baseName;
	char* runnerFile;
	char* paramFile;
	char* configFile;
	char* runnerTemplate = NULL;
	char* runnerCode = NULL;
	char* paramJson = NULL;
	char* configJson = NULL;
	char* commandLine = NULL;
	size_t runnerSize = 0;
	bool ok = false;

	if (!G_SchedCachePath || !G_SchedXSPath || !xrtFileExists(G_SchedXSPath)) {
		if (outMessage) *outMessage = xrtStrDup("Scheduler xs runtime is missing");
		return false;
	}
	baseName = xrtFormat("task_%lld_%lld", (long long)task->id, (long long)xrtNow());
	runnerFile = xrtPathJoin(G_SchedCachePath, xrtFormat("%s_runner.c", baseName));
	paramFile = xrtPathJoin(G_SchedCachePath, xrtFormat("%s_param.json", baseName));
	configFile = xrtPathJoin(G_SchedCachePath, xrtFormat("%s_runner.json", baseName));
	{
		str name = Sched_EscapeCString(task->sName);
		str trig = Sched_EscapeCString(triggerSource ? triggerSource : "scheduler");
		str custom = Sched_EscapeCString(task->sCustomText);
		paramJson = xrtFormat(
			"{\"taskId\": %lld, \"taskName\": \"%s\", \"triggerSource\": \"%s\", "
			"\"customJson\": \"%s\", \"startTime\": %lld, \"timeoutSec\": %d}\n",
			(long long)task->id, Sched_CStrOrEmpty(name), Sched_CStrOrEmpty(trig),
			Sched_CStrOrEmpty(custom), (long long)xrtNow(), task->timeoutSec);
		xrtFree(name); xrtFree(trig); xrtFree(custom);
	}
	if (!paramJson || !xrtFileWriteAtomic(paramFile, (xbytesview){(cbytes)paramJson, strlen(paramJson)})) {
		if (outMessage) *outMessage = xrtStrDup("Failed to write C task param file");
		goto cleanup;
	}
	{
		char* tplPath = xrtPathJoin(xrtPathJoin(AppPath, "template"), "sched/c_task_runner.c");
		runnerTemplate = (char*)xrtFileReadAll(tplPath, &runnerSize);
		xrtFree(tplPath);
	}
	if (runnerTemplate) {
		const char* mark = strstr(runnerTemplate, "{{$taskSourceCode}}");
		const char* code = Sched_CStrOrEmpty(task->sCodeText);
		if (mark) {
			size_t head = (size_t)(mark - runnerTemplate);
			runnerCode = xrtMalloc(head + strlen(code) + strlen(mark + 19) + 1);
			memcpy(runnerCode, runnerTemplate, head);
			strcpy(runnerCode + head, code);
			strcat(runnerCode, mark + 19);
		}
		xrtFree(runnerTemplate);
	}
	if (!runnerCode || !xrtFileWriteAtomic(runnerFile, (xbytesview){(cbytes)runnerCode, strlen(runnerCode)})) {
		if (outMessage) *outMessage = xrtStrDup("Failed to write C task runner file");
		goto cleanup;
	}
	{
		char* dir = xrtPathParent(runnerFile);
		char* name = xrtPathStem(runnerFile);
		char* file = xrtFormat("%s.c", name);
		/* 同目录部署共享便捷层，runner 与应用同一套 value 语义 */
		{
			char* src = xrtPathJoin(AppPath, "plugin_sdk/value_util.h");
			char* dst = xrtPathJoin(dir, "value_util.h");
			bytes data;
			size_t size = 0;
			if ((data = xrtFileReadAll(src, &size)) != NULL) {
				xrtFileWriteAtomic(dst, (xbytesview){data, size});
				xrtFree(data);
			}
			xrtFree(src); xrtFree(dst);
		}
		configJson = xrtFormat(
			"{\"services\": [{\"enabled\": true, \"class\": \"custom\", \"name\": \"SchedRunner\", \"ip\": \"127.0.0.1\", \"port\": 0, "
			"\"host_default\": {\"enabled\": true, \"name\": \"runner\", \"param\": \"%s\", "
			"\"path\": \"%s\", \"dev_inc\": \"%s\", \"devlang\": \"c\", \"devfile\": \"%s\"}}]}\n",
			Sched_JsonPath(paramFile), Sched_JsonPath(dir), Sched_JsonPath(dir), Sched_JsonPath(file));
		xrtFree(dir); xrtFree(name); xrtFree(file);
	}
	if (!configJson || !xrtFileWriteAtomic(configFile, (xbytesview){(cbytes)configJson, strlen(configJson)})) {
		if (outMessage) *outMessage = xrtStrDup("Failed to write C task runner config");
		goto cleanup;
	}
	commandLine = xrtFormat("\"%s\" \"%s\"", G_SchedXSPath, configFile);
	ok = Sched_RunProcessCommand(commandLine, AppPath, Sched_GetTimeoutMs(task),
		exitCode, timedOut, outStdout, outStderr, outMessage);

cleanup:
	xrtFileDelete(runnerFile);
	xrtFileDelete(paramFile);
	xrtFileDelete(configFile);
	xrtFree(runnerFile);
	xrtFree(paramFile);
	xrtFree(configFile);
	xrtFree(baseName);
	xrtFree(runnerCode);
	xrtFree(paramJson);
	xrtFree(configJson);
	xrtFree(commandLine);
	return ok;
}

/* ---- 工作线程 ---- */

typedef struct SchedWorkerContext {
	SchedTaskSnapshot task;
	str sTriggerSource;
} SchedWorkerContext;

static void Sched_FreeWorkerContext(SchedWorkerContext* ctx)
{
	if (!ctx) return;
	Sched_FreeTaskSnapshot(&ctx->task);
	xrtFree(ctx->sTriggerSource);
	xrtFree(ctx);
}

static bool Sched_StartTaskRunInternal(int64 id, const char* triggerSource, bool allowDisabled, str* outMessage);

static int32 Sched_WorkerProc(ptr param)
{
	SchedWorkerContext* ctx = (SchedWorkerContext*)param;
	sqlite3* db = Sched_OpenStandaloneDB();
	sqlite3_stmt* stmt = NULL;
	SchedTaskSnapshot state;
	int64 runLogId = 0;
	int64 start = xrtNow();
	int64 finish, durationMs, nextRun, retryNextRun = 0;
	int nextRunningCount = 0;
	int nextRetryState = 0;
	int exitCode = -1;
	int timedOutFlag = 0;
	int startPending = 0;
	int scheduleRetry = 0;
	const char* finalStatus;
	const char* taskStatus;
	str stdoutText = NULL;
	str stderrText = NULL;
	str message = NULL;
	bool result = false;

	if (!ctx || !db) {
		if (db) sqlite3_close(db);
		Sched_FreeWorkerContext(ctx);
		return 1;
	}
	if (sqlite3_prepare_v3(db,
		"INSERT INTO sched_run_log (taskId, taskName, triggerSource, startTime, finishTime, durationMs, status, exitCode, stdoutText, stderrText, message) VALUES (?, ?, ?, ?, 0, 0, 'running', 0, '', '', '')",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, ctx->task.id);
		sqlite3_bind_text(stmt, 2, ctx->task.sName, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 3, Sched_CStrOrEmpty(ctx->sTriggerSource), -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt, 4, start);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		runLogId = sqlite3_last_insert_rowid(db);
	}
	Sched_UpdateTaskRunningFlag(db, ctx->task.id, 1, "running", "Task started");

	if (Sched_TextEquals(ctx->task.sExecType, "shell"))
		result = Sched_RunShellTask(&ctx->task, ctx->sTriggerSource, &exitCode, &timedOutFlag, &stdoutText, &stderrText, &message);
	else
		result = Sched_RunCTask(&ctx->task, ctx->sTriggerSource, &exitCode, &timedOutFlag, &stdoutText, &stderrText, &message);

	finish = xrtNow();
	durationMs = (finish - start) / 1000;
	nextRun = Sched_CalcNextAfterRun(&ctx->task, finish);
	finalStatus = timedOutFlag ? "timeout" : (result && exitCode == 0 ? "success" : "failed");
	memset(&state, 0, sizeof(state));
	if (Sched_LoadTaskSnapshotById(db, ctx->task.id, &state)) {
		nextRunningCount = state.runningCount > 0 ? (state.runningCount - 1) : 0;
		if (!Sched_TextEquals(ctx->task.sScheduleType, "once")
			&& Sched_IsOverlapPolicy(&state, "queue_one")
			&& state.pendingRun && nextRunningCount == 0 && state.enabled)
			startPending = 1;
		if (!startPending && nextRunningCount == 0 && Sched_TaskCanRetry(&state, finalStatus)) {
			scheduleRetry = 1;
			nextRetryState = state.retryState + 1;
			retryNextRun = finish + (int64)(state.retryDelaySec > 0 ? state.retryDelaySec : 1) * 1000000;
		}
	}
	taskStatus = scheduleRetry ? "retrying" : finalStatus;

	if (runLogId > 0 && sqlite3_prepare_v3(db,
		"UPDATE sched_run_log SET finishTime = ?, durationMs = ?, status = ?, exitCode = ?, stdoutText = ?, stderrText = ?, message = ? WHERE id = ?",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, finish);
		sqlite3_bind_int64(stmt, 2, durationMs);
		sqlite3_bind_text(stmt, 3, finalStatus, -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt, 4, exitCode);
		sqlite3_bind_text(stmt, 5, Sched_CStrOrEmpty(stdoutText), -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 6, Sched_CStrOrEmpty(stderrText), -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt, 7, Sched_CStrOrEmpty(message), -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt, 8, runLogId);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	if (scheduleRetry) {
		str retryMessage = xrtFormat("Retry %d/%d scheduled after %d second(s)",
			nextRetryState, state.retryCount, state.retryDelaySec > 0 ? state.retryDelaySec : 1);
		if (sqlite3_prepare_v3(db,
			"UPDATE sched_task SET isRunning = 0, runningCount = 0, pendingRun = 0, nextRunAt = ?, retryState = ?, lastRunAt = ?, lastFinishAt = ?, lastStatus = 'retrying', lastMessage = ?, lastExitCode = ?, updateTime = ? WHERE id = ?",
			-1, 0, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_int64(stmt, 1, retryNextRun);
			sqlite3_bind_int(stmt, 2, nextRetryState);
			sqlite3_bind_int64(stmt, 3, start);
			sqlite3_bind_int64(stmt, 4, finish);
			sqlite3_bind_text(stmt, 5, retryMessage, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 6, exitCode);
			sqlite3_bind_int64(stmt, 7, finish);
			sqlite3_bind_int64(stmt, 8, ctx->task.id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		xrtFree(retryMessage);
	} else if (Sched_TextEquals(ctx->task.sScheduleType, "once")) {
		if (sqlite3_prepare_v3(db,
			"UPDATE sched_task SET enabled = 0, isRunning = 0, runningCount = 0, pendingRun = 0, nextRunAt = 0, retryState = 0, lastRunAt = ?, lastFinishAt = ?, lastStatus = ?, lastMessage = ?, lastExitCode = ?, updateTime = ? WHERE id = ?",
			-1, 0, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_int64(stmt, 1, start);
			sqlite3_bind_int64(stmt, 2, finish);
			sqlite3_bind_text(stmt, 3, taskStatus, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 4, Sched_CStrOrEmpty(message), -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 5, exitCode);
			sqlite3_bind_int64(stmt, 6, finish);
			sqlite3_bind_int64(stmt, 7, ctx->task.id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	} else if (Sched_IsOverlapPolicy(&state, "parallel")) {
		if (sqlite3_prepare_v3(db,
			"UPDATE sched_task SET isRunning = CASE WHEN ? > 0 THEN 1 ELSE 0 END, runningCount = ?, retryState = 0, lastRunAt = ?, lastFinishAt = ?, lastStatus = ?, lastMessage = ?, lastExitCode = ?, updateTime = ? WHERE id = ?",
			-1, 0, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_int(stmt, 1, nextRunningCount);
			sqlite3_bind_int(stmt, 2, nextRunningCount);
			sqlite3_bind_int64(stmt, 3, start);
			sqlite3_bind_int64(stmt, 4, finish);
			sqlite3_bind_text(stmt, 5, taskStatus, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 6, Sched_CStrOrEmpty(message), -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 7, exitCode);
			sqlite3_bind_int64(stmt, 8, finish);
			sqlite3_bind_int64(stmt, 9, ctx->task.id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	} else if (startPending) {
		if (sqlite3_prepare_v3(db,
			"UPDATE sched_task SET isRunning = 0, runningCount = 0, pendingRun = 0, retryState = 0, lastRunAt = ?, lastFinishAt = ?, lastStatus = ?, lastMessage = ?, lastExitCode = ?, updateTime = ? WHERE id = ?",
			-1, 0, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_int64(stmt, 1, start);
			sqlite3_bind_int64(stmt, 2, finish);
			sqlite3_bind_text(stmt, 3, taskStatus, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 4, Sched_CStrOrEmpty(message), -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 5, exitCode);
			sqlite3_bind_int64(stmt, 6, finish);
			sqlite3_bind_int64(stmt, 7, ctx->task.id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	} else {
		if (sqlite3_prepare_v3(db,
			"UPDATE sched_task SET isRunning = CASE WHEN ? > 0 THEN 1 ELSE 0 END, runningCount = ?, nextRunAt = CASE WHEN enabled = 0 THEN 0 ELSE ? END, retryState = 0, lastRunAt = ?, lastFinishAt = ?, lastStatus = ?, lastMessage = ?, lastExitCode = ?, updateTime = ? WHERE id = ?",
			-1, 0, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_int(stmt, 1, nextRunningCount);
			sqlite3_bind_int(stmt, 2, nextRunningCount);
			sqlite3_bind_int64(stmt, 3, nextRun);
			sqlite3_bind_int64(stmt, 4, start);
			sqlite3_bind_int64(stmt, 5, finish);
			sqlite3_bind_text(stmt, 6, taskStatus, -1, SQLITE_STATIC);
			sqlite3_bind_text(stmt, 7, Sched_CStrOrEmpty(message), -1, SQLITE_STATIC);
			sqlite3_bind_int(stmt, 8, exitCode);
			sqlite3_bind_int64(stmt, 9, finish);
			sqlite3_bind_int64(stmt, 10, ctx->task.id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
	}

	if (startPending)
		Sched_StartTaskRunInternal(ctx->task.id, "queued", false, NULL);

	xrtFree(stdoutText);
	xrtFree(stderrText);
	xrtFree(message);
	Sched_FreeTaskSnapshot(&state);
	sqlite3_close(db);
	Sched_FreeWorkerContext(ctx);
	if (G_SchedLock && G_SchedCond) {
		xrtMutexLock(G_SchedLock);
		if (G_SchedWorkerCount > 0) G_SchedWorkerCount--;
		xrtCondBroadcast(G_SchedCond);
		xrtMutexUnlock(G_SchedLock);
	}
	return 0;
}

static bool Sched_StartTaskRunInternal(int64 id, const char* triggerSource, bool allowDisabled, str* outMessage)
{
	sqlite3* db = Sched_OpenStandaloneDB();
	SchedTaskSnapshot task;
	SchedWorkerContext* ctx = NULL;
	xthread* thread = NULL;
	sqlite3_stmt* stmt = NULL;
	size_t i;

	memset(&task, 0, sizeof(task));
	if (!db) {
		if (outMessage) *outMessage = xrtStrDup("Failed to open scheduler db");
		return false;
	}
	if (!Sched_LoadTaskSnapshotById(db, id, &task)) {
		if (outMessage) *outMessage = xrtStrDup("Task not found");
		sqlite3_close(db);
		return false;
	}
	if (!allowDisabled && !task.enabled) {
		if (outMessage) *outMessage = xrtStrDup("Task is disabled");
		Sched_FreeTaskSnapshot(&task);
		sqlite3_close(db);
		return false;
	}
	if (task.runningCount > 0 && Sched_IsOverlapPolicy(&task, "skip")) {
		if (outMessage) *outMessage = xrtStrDup("Task is already running");
		Sched_FreeTaskSnapshot(&task);
		sqlite3_close(db);
		return false;
	}
	if (Sched_IsOverlapPolicy(&task, "parallel") && task.parallelLimit > 0 && task.runningCount >= task.parallelLimit) {
		if (outMessage) *outMessage = xrtStrDup("Parallel limit reached");
		Sched_FreeTaskSnapshot(&task);
		sqlite3_close(db);
		return false;
	}
	if (task.runningCount > 0 && Sched_IsOverlapPolicy(&task, "queue_one")) {
		if (sqlite3_prepare_v3(db,
			"UPDATE sched_task SET pendingRun = 1, lastStatus = 'queued', lastMessage = ?, updateTime = ? WHERE id = ? AND isDelete = 0",
			-1, 0, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_text(stmt, 1, task.pendingRun ? "Pending run already queued" : "Queued to run once after current execution", -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 2, xrtNow());
			sqlite3_bind_int64(stmt, 3, id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		Sched_FreeTaskSnapshot(&task);
		sqlite3_close(db);
		if (outMessage) *outMessage = xrtStrDup("Task queued to run after current execution");
		return true;
	}
	if (sqlite3_prepare_v3(db,
		"UPDATE sched_task SET runningCount = CASE WHEN runningCount < 0 THEN 1 ELSE runningCount + 1 END, isRunning = 1, lastStatus = 'queued', lastMessage = ?, updateTime = ? WHERE id = ? AND isDelete = 0",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_text(stmt, 1, triggerSource ? triggerSource : "scheduler", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, xrtNow());
		sqlite3_bind_int64(stmt, 3, id);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	ctx = (SchedWorkerContext*)xrtMalloc(sizeof(SchedWorkerContext));
	memset(ctx, 0, sizeof(*ctx));
	ctx->task = task;
	ctx->sTriggerSource = Sched_CopyText(triggerSource ? triggerSource : "scheduler");

	if (G_SchedLock) {
		xrtMutexLock(G_SchedLock);
		G_SchedWorkerCount++;
		xrtMutexUnlock(G_SchedLock);
	}
	thread = xrtThreadCreate(Sched_WorkerProc, ctx, 0);
	if (!thread) {
		if (sqlite3_prepare_v3(db,
			"UPDATE sched_task SET runningCount = CASE WHEN runningCount > 0 THEN runningCount - 1 ELSE 0 END, isRunning = CASE WHEN runningCount > 1 THEN 1 ELSE 0 END, lastStatus = 'failed', lastMessage = 'Failed to start task thread', updateTime = ? WHERE id = ? AND isDelete = 0",
			-1, 0, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_int64(stmt, 1, xrtNow());
			sqlite3_bind_int64(stmt, 2, id);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		Sched_FreeWorkerContext(ctx);
		if (G_SchedLock) {
			xrtMutexLock(G_SchedLock);
			if (G_SchedWorkerCount > 0) G_SchedWorkerCount--;
			xrtCondBroadcast(G_SchedCond);
			xrtMutexUnlock(G_SchedLock);
		}
		sqlite3_close(db);
		if (outMessage) *outMessage = xrtStrDup("Failed to start task thread");
		return false;
	}
	/* 登记到槽位，Unit 时统一 join */
	for (i = 0; i < SCHED_WORKER_MAX; i++) {
		if (!G_SchedWorkers[i].busy) {
			G_SchedWorkers[i].thread = thread;
			G_SchedWorkers[i].taskId = id;
			G_SchedWorkers[i].busy = true;
			break;
		}
	}
	xrtThreadDestroy(thread);   /* 引用计数模型：登记后由我们持有的引用回收 */
	sqlite3_close(db);
	if (outMessage) *outMessage = xrtStrDup("Task queued");
	return true;
}

bool Sched_RunNow(int64 id, str* outMessage)
{
	return Sched_StartTaskRunInternal(id, "manual", true, outMessage);
}

/* ---- 调度主循环 ---- */

static bool Sched_ProcessDueTask(sqlite3* db, const SchedTaskSnapshot* task, int64 now, str* outMessage)
{
	sqlite3_stmt* stmt = NULL;
	int64 nextRunAt;
	const char* message = NULL;
	bool shouldStart = false;

	if (!db || !task || task->id <= 0) return false;
	nextRunAt = Sched_CalcBusyNext(task, now);
	if (task->runningCount > 0 && Sched_IsOverlapPolicy(task, "skip")) {
		message = "Skipped due run while task was already running";
	} else if (!task->enabled) {
		message = "Skipped due run for disabled task";
	} else {
		shouldStart = true;
		message = task->runningCount > 0 && Sched_IsOverlapPolicy(task, "queue_one")
			? "Queued due run behind current execution" : "Triggered by scheduler";
	}
	if (sqlite3_prepare_v3(db,
		"UPDATE sched_task SET nextRunAt = ?, lastMessage = ?, updateTime = ? WHERE id = ? AND isDelete = 0",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_int64(stmt, 1, nextRunAt);
		sqlite3_bind_text(stmt, 2, message, -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 3, xrtNow());
		sqlite3_bind_int64(stmt, 4, task->id);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
	if (outMessage) *outMessage = message ? xrtStrDup(message) : NULL;
	return shouldStart;
}

static bool Sched_FetchDueTask(sqlite3* db, int64 now, SchedTaskSnapshot* task)
{
	sqlite3_stmt* stmt = NULL;
	int64 id = 0;

	if (sqlite3_prepare_v3(db,
		"SELECT id FROM sched_task WHERE isDelete = 0 AND enabled = 1 AND isRunning = 0 AND nextRunAt > 0 AND nextRunAt <= ? ORDER BY nextRunAt ASC LIMIT 1",
		-1, 0, &stmt, NULL) != SQLITE_OK)
		return false;
	sqlite3_bind_int64(stmt, 1, now);
	if (sqlite3_step(stmt) == SQLITE_ROW) id = sqlite3_column_int64(stmt, 0);
	sqlite3_finalize(stmt);
	if (id <= 0) return false;
	return Sched_LoadTaskSnapshotById(db, id, task);
}

static int64 Sched_QueryNextWakeTime(sqlite3* db)
{
	sqlite3_stmt* stmt = NULL;
	int64 next = 0;

	if (sqlite3_prepare_v3(db,
		"SELECT MIN(nextRunAt) FROM sched_task WHERE isDelete = 0 AND enabled = 1 AND nextRunAt > 0",
		-1, 0, &stmt, NULL) != SQLITE_OK)
		return 0;
	if (sqlite3_step(stmt) == SQLITE_ROW) next = sqlite3_column_int64(stmt, 0);
	sqlite3_finalize(stmt);
	return next;
}

static void Sched_ApplyMisfirePolicies(sqlite3* db)
{
	sqlite3_stmt* stmt = NULL;
	sqlite3_stmt* stmtUpd = NULL;
	int64 now = xrtNow();

	if (sqlite3_prepare_v3(db,
		"SELECT id, misfirePolicy FROM sched_task WHERE isDelete = 0 AND enabled = 1 AND nextRunAt > 0 AND nextRunAt < ?",
		-1, 0, &stmt, NULL) != SQLITE_OK)
		return;
	sqlite3_bind_int64(stmt, 1, now);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		int64 id = sqlite3_column_int64(stmt, 0);
		const char* policy = Sched_SQLiteTextOrEmpty(stmt, 1);
		if (Sched_TextEquals(policy, "run_once")) {
			if (sqlite3_prepare_v3(db, "UPDATE sched_task SET nextRunAt = ? WHERE id = ?", -1, 0, &stmtUpd, NULL) == SQLITE_OK) {
				sqlite3_bind_int64(stmtUpd, 1, now);
				sqlite3_bind_int64(stmtUpd, 2, id);
				sqlite3_step(stmtUpd);
				sqlite3_finalize(stmtUpd);
			}
		} else {
			/* skip：重算下一跳，避免启动即补跑 */
			SchedTaskSnapshot task;
			memset(&task, 0, sizeof(task));
			if (Sched_LoadTaskSnapshotById(db, id, &task)) {
				int64 next = Sched_CalcNextTime(&task, now);
				if (sqlite3_prepare_v3(db, "UPDATE sched_task SET nextRunAt = ? WHERE id = ?", -1, 0, &stmtUpd, NULL) == SQLITE_OK) {
					sqlite3_bind_int64(stmtUpd, 1, next);
					sqlite3_bind_int64(stmtUpd, 2, id);
					sqlite3_step(stmtUpd);
					sqlite3_finalize(stmtUpd);
				}
				Sched_FreeTaskSnapshot(&task);
			}
		}
	}
	sqlite3_finalize(stmt);
}

static int32 Sched_ThreadProc(ptr param)
{
	sqlite3* db = Sched_OpenStandaloneDB();
	(void)param;

	if (!db) return 1;
	Sched_ApplyMisfirePolicies(db);
	while (true) {
		SchedTaskSnapshot task;
		int64 now, nextWake, waitMicro;
		bool shouldStart = false;

		xrtMutexLock(G_SchedLock);
		if (G_SchedStop) { xrtMutexUnlock(G_SchedLock); break; }
		xrtMutexUnlock(G_SchedLock);

		now = xrtNow();
		memset(&task, 0, sizeof(task));
		if (Sched_FetchDueTask(db, now, &task)) {
			shouldStart = Sched_ProcessDueTask(db, &task, now, NULL);
			if (shouldStart)
				Sched_StartTaskRunInternal(task.id, "scheduler", false, NULL);
			Sched_FreeTaskSnapshot(&task);
			continue;
		}
		nextWake = Sched_QueryNextWakeTime(db);
		xrtMutexLock(G_SchedLock);
		if (G_SchedStop) { xrtMutexUnlock(G_SchedLock); break; }
		if (nextWake <= 0) {
			xrtCondWaitFor(G_SchedCond, G_SchedLock, 1000000);
		} else {
			waitMicro = nextWake - xrtNow();
			if (waitMicro < 100000) waitMicro = 100000;
			if (waitMicro > 86400000000ull) waitMicro = 86400000000ull;
			xrtCondWaitFor(G_SchedCond, G_SchedLock, (uint64)waitMicro);
		}
		xrtMutexUnlock(G_SchedLock);
	}
	sqlite3_close(db);
	return 0;
}

void Sched_NotifyChanged(void)
{
	if (!G_SchedLock || !G_SchedCond) return;
	xrtMutexLock(G_SchedLock);
	xrtCondBroadcast(G_SchedCond);
	xrtMutexUnlock(G_SchedLock);
}

/* ---- 任务 CRUD（保存校验/导入/复制/启用/删除） ---- */

static bool Sched_TaskIsValidForSave(SchedTaskSnapshot* task, str* outMessage)
{
	if (!task->sName || !task->sName[0]) {
		if (outMessage) *outMessage = xrtStrDup("任务名称不能为空");
		return false;
	}
	if (!Sched_TextEquals(task->sScheduleType, "once")
		&& !Sched_TextEquals(task->sScheduleType, "interval")
		&& !Sched_TextEquals(task->sScheduleType, "cron")) {
		if (outMessage) *outMessage = xrtStrDup("调度类型必须是 once/interval/cron");
		return false;
	}
	if (!Sched_TextEquals(task->sExecType, "shell") && !Sched_TextEquals(task->sExecType, "c")) {
		if (outMessage) *outMessage = xrtStrDup("执行类型必须是 shell/c");
		return false;
	}
	if (Sched_TextEquals(task->sScheduleType, "once") && task->onceAt <= 0) {
		if (outMessage) *outMessage = xrtStrDup("单次任务必须指定触发时间");
		return false;
	}
	if (Sched_TextEquals(task->sScheduleType, "interval") && Sched_IntervalSeconds(task->intervalValue, task->sIntervalUnit) <= 0) {
		if (outMessage) *outMessage = xrtStrDup("间隔任务的间隔值/单位无效");
		return false;
	}
	if (Sched_TextEquals(task->sScheduleType, "cron")) {
		SchedCronExpr cron;
		if (!task->sCronExpr || !task->sCronExpr[0]) {
			if (outMessage) *outMessage = xrtStrDup("cron 任务缺少表达式");
			return false;
		}
		if (!Sched_ParseCronExpr(task->sCronExpr, &cron)) {
			if (outMessage) *outMessage = xrtStrDup("cron 表达式无效");
			return false;
		}
	}
	if (!task->sCodeText || !task->sCodeText[0]) {
		if (outMessage) *outMessage = xrtStrDup("任务代码不能为空");
		return false;
	}
	/* 攻击评审 SCH-8：数值字段拒绝负值（超时/并行/重试/时刻/间隔） */
	if (task->timeoutSec < 0 || task->parallelLimit < 0 || task->retryCount < 0
		|| task->retryDelaySec < 0 || task->onceAt < 0 || task->intervalValue < 0
		|| task->startAt < 0) {
		if (outMessage) *outMessage = xrtStrDup("数值字段不允许为负");
		return false;
	}
	if (task->timeoutSec == 0) task->timeoutSec = 300;
	return true;
}

static void Sched_ReadTaskFieldFromBody(xvalue* body, SchedTaskSnapshot* task)
{
	str text;
	task->sName = Sched_CopyText(ValueText(body, "name"));
	text = ValueText(body, "scheduleType");
	task->sScheduleType = Sched_CopyText(text ? text : "once");
	text = ValueText(body, "execType");
	task->sExecType = Sched_CopyText(text ? text : "shell");
	text = ValueText(body, "shellType");
	task->sShellType = Sched_CopyText(text ? text : "cmd");
	task->sCodeText = Sched_CopyText(ValueText(body, "codeText"));
	task->sCustomText = Sched_CopyText(ValueText(body, "customText"));
	task->sCronExpr = Sched_CopyText(ValueText(body, "cronExpr"));
	task->onceAt = ValueInt(body, "onceAt");
	task->intervalValue = ValueInt(body, "intervalValue");
	text = ValueText(body, "intervalUnit");
	task->sIntervalUnit = Sched_CopyText(text ? text : "second");
	task->startAt = ValueInt(body, "startAt");
	task->timeoutSec = (int)ValueInt(body, "timeoutSec");
	text = ValueText(body, "overlapPolicy");
	task->sOverlapPolicy = Sched_CopyText(text ? text : "skip");
	text = ValueText(body, "misfirePolicy");
	task->sMisfirePolicy = Sched_CopyText(text ? text : "skip");
	task->sWorkDir = Sched_CopyText(ValueText(body, "workDir"));
	task->parallelLimit = (int)ValueInt(body, "parallelLimit");
	task->retryCount = (int)ValueInt(body, "retryCount");
	task->retryDelaySec = (int)ValueInt(body, "retryDelaySec");
}

static bool Sched_SaveTaskRequest(xvalue* body, bool update, str* outMessage, int64* outTaskId)
{
	SchedTaskSnapshot task;
	sqlite3_stmt* stmt = NULL;
	int64 id = 0;
	int64 now = xrtNow();
	bool ok = false;

	memset(&task, 0, sizeof(task));
	if (outMessage) *outMessage = NULL;
	if (outTaskId) *outTaskId = 0;
	if (!body || xrtValueType(body) != XVALUE_OBJECT) {
		if (outMessage) *outMessage = xrtStrDup("无效的请求数据");
		return false;
	}
	Sched_ReadTaskFieldFromBody(body, &task);
	task.enabled = update ? false : ValueBool(body, "enabled");
	if (update) id = ValueInt(body, "id");
	if (!Sched_TaskIsValidForSave(&task, outMessage)) {
		Sched_FreeTaskSnapshot(&task);
		return false;
	}
	task.nextRunAt = Sched_CalcNextTime(&task, now);

	if (!update) {
		if (sqlite3_prepare_v3(G_DB,
			"INSERT INTO sched_task (name, enabled, scheduleType, execType, shellType, codeText, customText, cronExpr, onceAt, intervalValue, intervalUnit, startAt, nextRunAt, timeoutSec, overlapPolicy, misfirePolicy, workDir, parallelLimit, retryCount, retryDelaySec, retryState, createTime, updateTime, isDelete, isRunning, runningCount, pendingRun, lastStatus, lastMessage, lastExitCode, lastRunAt, lastFinishAt) "
			"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?, ?, 0, 0, 0, 0, '', '', 0, 0, 0)",
			-1, 0, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_text(stmt, 1, task.sName, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 2, task.enabled ? 1 : 0);
			sqlite3_bind_text(stmt, 3, task.sScheduleType, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, task.sExecType, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 5, task.sShellType, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 6, task.sCodeText, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 7, task.sCustomText, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 8, task.sCronExpr, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 9, task.onceAt);
			sqlite3_bind_int64(stmt, 10, task.intervalValue);
			sqlite3_bind_text(stmt, 11, task.sIntervalUnit, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 12, task.startAt);
			sqlite3_bind_int64(stmt, 13, task.nextRunAt);
			sqlite3_bind_int(stmt, 14, task.timeoutSec);
			sqlite3_bind_text(stmt, 15, task.sOverlapPolicy, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 16, task.sMisfirePolicy, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 17, task.sWorkDir, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 18, task.parallelLimit);
			sqlite3_bind_int(stmt, 19, task.retryCount);
			sqlite3_bind_int(stmt, 20, task.retryDelaySec);
			sqlite3_bind_int64(stmt, 21, now);
			sqlite3_bind_int64(stmt, 22, now);
			if (sqlite3_step(stmt) == SQLITE_DONE) {
				id = sqlite3_last_insert_rowid(G_DB);
				ok = true;
			}
			sqlite3_finalize(stmt);
		}
	} else {
		if (sqlite3_prepare_v3(G_DB,
			"UPDATE sched_task SET name = ?, enabled = ?, scheduleType = ?, execType = ?, shellType = ?, codeText = ?, customText = ?, cronExpr = ?, onceAt = ?, intervalValue = ?, intervalUnit = ?, startAt = ?, nextRunAt = ?, timeoutSec = ?, overlapPolicy = ?, misfirePolicy = ?, workDir = ?, parallelLimit = ?, retryCount = ?, retryDelaySec = ?, retryState = 0, updateTime = ?, isRunning = 0, runningCount = 0, pendingRun = 0 WHERE id = ? AND isDelete = 0",
			-1, 0, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_text(stmt, 1, task.sName, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 2, task.enabled ? 1 : 0);
			sqlite3_bind_text(stmt, 3, task.sScheduleType, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, task.sExecType, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 5, task.sShellType, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 6, task.sCodeText, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 7, task.sCustomText, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 8, task.sCronExpr, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 9, task.onceAt);
			sqlite3_bind_int64(stmt, 10, task.intervalValue);
			sqlite3_bind_text(stmt, 11, task.sIntervalUnit, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 12, task.startAt);
			sqlite3_bind_int64(stmt, 13, task.nextRunAt);
			sqlite3_bind_int(stmt, 14, task.timeoutSec);
			sqlite3_bind_text(stmt, 15, task.sOverlapPolicy, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 16, task.sMisfirePolicy, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 17, task.sWorkDir, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 18, task.parallelLimit);
			sqlite3_bind_int(stmt, 19, task.retryCount);
			sqlite3_bind_int(stmt, 20, task.retryDelaySec);
			sqlite3_bind_int64(stmt, 21, now);
			sqlite3_bind_int64(stmt, 22, id);
			if (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(G_DB) > 0) ok = true;
			sqlite3_finalize(stmt);
		}
	}
	if (ok) {
		if (outTaskId) *outTaskId = id;
		if (outMessage) *outMessage = xrtStrDup(update ? "任务已更新" : "任务已创建");
		Sched_NotifyChanged();
	} else if (outMessage && !*outMessage) {
		if (outMessage) *outMessage = xrtStrDup(update ? "任务不存在或保存失败" : "任务创建失败");
	}
	Sched_FreeTaskSnapshot(&task);
	return ok;
}

static int64 Sched_FindTaskIdByName(const char* name)
{
	sqlite3_stmt* stmt = NULL;
	int64 id = 0;

	if (sqlite3_prepare_v3(G_DB, "SELECT id FROM sched_task WHERE name = ? AND isDelete = 0 ORDER BY id ASC LIMIT 1",
		-1, 0, &stmt, NULL) != SQLITE_OK)
		return 0;
	sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
	if (sqlite3_step(stmt) == SQLITE_ROW) id = sqlite3_column_int64(stmt, 0);
	sqlite3_finalize(stmt);
	return id;
}

bool Sched_DeleteTask(int64 id, str* outMessage)
{
	sqlite3_stmt* stmt = NULL;

	if (sqlite3_prepare_v3(G_DB, "UPDATE sched_task SET isDelete = 1, enabled = 0, updateTime = ? WHERE id = ? AND isDelete = 0",
		-1, 0, &stmt, NULL) != SQLITE_OK) {
		if (outMessage) *outMessage = xrtStrDup("删除失败");
		return false;
	}
	sqlite3_bind_int64(stmt, 1, xrtNow());
	sqlite3_bind_int64(stmt, 2, id);
	{
		bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(G_DB) > 0;
		sqlite3_finalize(stmt);
		if (outMessage) *outMessage = xrtStrDup(ok ? "任务已删除" : "任务不存在");
		Sched_NotifyChanged();
		return ok;
	}
}

bool Sched_SetEnabled(int64 id, bool enabled, str* outMessage)
{
	sqlite3_stmt* stmt = NULL;
	SchedTaskSnapshot task;
	int64 now = xrtNow();

	memset(&task, 0, sizeof(task));
	if (!Sched_LoadTaskSnapshotById(G_DB, id, &task)) {
		if (outMessage) *outMessage = xrtStrDup("任务不存在");
		return false;
	}
	if (sqlite3_prepare_v3(G_DB, "UPDATE sched_task SET enabled = ?, nextRunAt = ?, updateTime = ? WHERE id = ? AND isDelete = 0",
		-1, 0, &stmt, NULL) != SQLITE_OK) {
		Sched_FreeTaskSnapshot(&task);
		if (outMessage) *outMessage = xrtStrDup("操作失败");
		return false;
	}
	sqlite3_bind_int(stmt, 1, enabled ? 1 : 0);
	sqlite3_bind_int64(stmt, 2, enabled ? Sched_CalcNextTime(&task, now) : 0);
	sqlite3_bind_int64(stmt, 3, now);
	sqlite3_bind_int64(stmt, 4, id);
	{
		bool ok = sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(G_DB) > 0;
		sqlite3_finalize(stmt);
		Sched_FreeTaskSnapshot(&task);
		if (outMessage) *outMessage = xrtStrDup(ok ? (enabled ? "任务已启用" : "任务已停用") : "任务不存在");
		Sched_NotifyChanged();
		return ok;
	}
}

static bool Sched_CopyTask(int64 id, str* outMessage, int64* outTaskId)
{
	SchedTaskSnapshot task;
	sqlite3_stmt* stmt = NULL;
	int64 now = xrtNow();
	bool ok = false;

	memset(&task, 0, sizeof(task));
	if (!Sched_LoadTaskSnapshotById(G_DB, id, &task)) {
		if (outMessage) *outMessage = xrtStrDup("任务不存在");
		return false;
	}
	{
		str copyName = xrtFormat("%s (copy)", task.sName);
		if (sqlite3_prepare_v3(G_DB,
			"INSERT INTO sched_task (name, enabled, scheduleType, execType, shellType, codeText, customText, cronExpr, onceAt, intervalValue, intervalUnit, startAt, nextRunAt, timeoutSec, overlapPolicy, misfirePolicy, workDir, parallelLimit, retryCount, retryDelaySec, retryState, createTime, updateTime, isDelete) "
			"VALUES (?, 0, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?, ?, ?, ?, ?, ?, ?, 0, ?, ?, 0)",
			-1, 0, &stmt, NULL) == SQLITE_OK) {
			sqlite3_bind_text(stmt, 1, copyName, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 2, task.sScheduleType, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 3, task.sExecType, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 4, task.sShellType, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 5, task.sCodeText, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 6, task.sCustomText, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 7, task.sCronExpr, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 8, task.onceAt);
			sqlite3_bind_int64(stmt, 9, task.intervalValue);
			sqlite3_bind_text(stmt, 10, task.sIntervalUnit, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int64(stmt, 11, task.startAt);
			sqlite3_bind_int(stmt, 12, task.timeoutSec);
			sqlite3_bind_text(stmt, 13, task.sOverlapPolicy, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 14, task.sMisfirePolicy, -1, SQLITE_TRANSIENT);
			sqlite3_bind_text(stmt, 15, task.sWorkDir, -1, SQLITE_TRANSIENT);
			sqlite3_bind_int(stmt, 16, task.parallelLimit);
			sqlite3_bind_int(stmt, 17, task.retryCount);
			sqlite3_bind_int(stmt, 18, task.retryDelaySec);
			sqlite3_bind_int64(stmt, 19, now);
			sqlite3_bind_int64(stmt, 20, now);
			if (sqlite3_step(stmt) == SQLITE_DONE) {
				if (outTaskId) *outTaskId = sqlite3_last_insert_rowid(G_DB);
				ok = true;
			}
			sqlite3_finalize(stmt);
		}
		xrtFree(copyName);
	}
	Sched_FreeTaskSnapshot(&task);
	if (outMessage) *outMessage = xrtStrDup(ok ? "任务已复制（停用状态）" : "任务复制失败");
	return ok;
}

static bool Sched_CreateExampleTask(const char* kind, str* outMessage, int64* outTaskId)
{
	xvalue* body = xrtValueObject();
	bool ok;

	if (Sched_TextEquals(kind, "c")) {
		ValueSetText(body, "name", "示例：C 任务（输出问候）");
		ValueSetText(body, "execType", "c");
		ValueSetText(body, "scheduleType", "once");
		ValueSetInt(body, "onceAt", (xrtNow() + 60 * 1000000) / 1000000 * 1000000);
		ValueSetText(body, "codeText",
			"int TaskProc(TaskInfo* info) {\n"
			"    snprintf(info->output, SCHED_OUTPUT_CAP, \"hello from C task %lld\", (long long)info->task_id);\n"
			"    return 0;\n"
			"}\n");
	} else {
		ValueSetText(body, "name", "示例：Shell 任务（回显）");
		ValueSetText(body, "execType", "shell");
		ValueSetText(body, "shellType", "cmd");
		ValueSetText(body, "scheduleType", "interval");
		ValueSetInt(body, "intervalValue", 60);
		ValueSetText(body, "intervalUnit", "second");
		ValueSetText(body, "codeText", "@echo sched example task ran\n");
	}
	ok = Sched_SaveTaskRequest(body, false, outMessage, outTaskId);
	xrtValueRelease(body);
	return ok;
}

/* ---- 批量操作 / 导入 / 导出 / 日志 ---- */

bool Sched_RunNowBatch(xvalue* ids, int64* queued, int64* skipped, int64* failed, str* outMessage)
{
	size_t i, count;
	str message = NULL;

	if (queued) *queued = 0;
	if (skipped) *skipped = 0;
	if (failed) *failed = 0;
	if (!ids || xrtValueType(ids) != XVALUE_ARRAY) {
		if (outMessage) *outMessage = xrtStrDup("缺少任务 ID 列表");
		return false;
	}
	count = xrtValueCount(ids);
	for (i = 0; i < count; i++) {
		if (Sched_RunNow(ValueArrayInt(ids, i), &message)) {
			if (queued) (*queued)++;
		} else if (message && (strstr(message, "disabled") || strstr(message, "running"))) {
			if (skipped) (*skipped)++;
		} else {
			if (failed) (*failed)++;
		}
		xrtFree(message);
		message = NULL;
	}
	if (outMessage) *outMessage = xrtFormat("已触发 %lld，跳过 %lld，失败 %lld",
		(long long)(queued ? *queued : 0), (long long)(skipped ? *skipped : 0), (long long)(failed ? *failed : 0));
	return true;
}

bool Sched_SetEnabledBatch(xvalue* ids, bool enabled, int64* affected, str* outMessage)
{
	size_t i, count;
	int64 n = 0;
	str message = NULL;

	if (!ids || xrtValueType(ids) != XVALUE_ARRAY) {
		if (outMessage) *outMessage = xrtStrDup("缺少任务 ID 列表");
		return false;
	}
	count = xrtValueCount(ids);
	for (i = 0; i < count; i++) {
		if (Sched_SetEnabled(ValueArrayInt(ids, i), enabled, &message)) n++;
		xrtFree(message);
		message = NULL;
	}
	if (affected) *affected = n;
	if (outMessage) *outMessage = xrtFormat("已更新 %lld 个任务", (long long)n);
	return true;
}

bool Sched_DeleteTaskBatch(xvalue* ids, int64* affected, str* outMessage)
{
	size_t i, count;
	int64 n = 0;
	str message = NULL;

	if (!ids || xrtValueType(ids) != XVALUE_ARRAY) {
		if (outMessage) *outMessage = xrtStrDup("缺少任务 ID 列表");
		return false;
	}
	count = xrtValueCount(ids);
	for (i = 0; i < count; i++) {
		if (Sched_DeleteTask(ValueArrayInt(ids, i), &message)) n++;
		xrtFree(message);
		message = NULL;
	}
	if (affected) *affected = n;
	if (outMessage) *outMessage = xrtFormat("已删除 %lld 个任务", (long long)n);
	return true;
}

bool Sched_ClearLogs(int64 taskId, str* outMessage)
{
	sqlite3_stmt* stmt = NULL;
	int64 affected = 0;

	if (sqlite3_prepare_v3(G_DB,
		taskId > 0 ? "DELETE FROM sched_run_log WHERE taskId = ?" : "DELETE FROM sched_run_log",
		-1, 0, &stmt, NULL) != SQLITE_OK) {
		if (outMessage) *outMessage = xrtStrDup("清空失败");
		return false;
	}
	if (taskId > 0) sqlite3_bind_int64(stmt, 1, taskId);
	sqlite3_step(stmt);
	affected = sqlite3_changes(G_DB);
	sqlite3_finalize(stmt);
	if (outMessage) *outMessage = xrtFormat("已清空 %lld 条运行日志", (long long)affected);
	return true;
}

/* ---- 初始化 / 卸载 ---- */

void Sched_Init(void)
{
	printf("        Sched_Init \n");
	G_SchedPath = xrtPathJoin(AppPath, "data/sched");
	G_SchedCachePath = xrtPathJoin(G_SchedPath, "cache");
	G_SchedXSPath = xrtPathJoin(xsAppPath(), "xs.exe");
	xrtDirCreateAll(G_SchedCachePath);

	sqlite3_exec(G_DB, "CREATE TABLE IF NOT EXISTS sched_task (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, enabled INTEGER DEFAULT 1, scheduleType TEXT NOT NULL, execType TEXT NOT NULL, shellType TEXT DEFAULT '', codeText TEXT DEFAULT '', customText TEXT DEFAULT '', cronExpr TEXT DEFAULT '', onceAt INTEGER DEFAULT 0, intervalValue INTEGER DEFAULT 0, intervalUnit TEXT DEFAULT '', startAt INTEGER DEFAULT 0, nextRunAt INTEGER DEFAULT 0, lastRunAt INTEGER DEFAULT 0, lastFinishAt INTEGER DEFAULT 0, timeoutSec INTEGER DEFAULT 300, overlapPolicy TEXT DEFAULT 'skip', misfirePolicy TEXT DEFAULT 'skip', workDir TEXT DEFAULT '', isRunning INTEGER DEFAULT 0, runningCount INTEGER DEFAULT 0, pendingRun INTEGER DEFAULT 0, parallelLimit INTEGER DEFAULT 0, retryCount INTEGER DEFAULT 0, retryDelaySec INTEGER DEFAULT 60, retryState INTEGER DEFAULT 0, lastStatus TEXT DEFAULT '', lastMessage TEXT DEFAULT '', lastExitCode INTEGER DEFAULT 0, createTime INTEGER DEFAULT 0, updateTime INTEGER DEFAULT 0, isDelete INTEGER DEFAULT 0)", NULL, NULL, NULL);
	sqlite3_exec(G_DB, "CREATE TABLE IF NOT EXISTS sched_run_log (id INTEGER PRIMARY KEY AUTOINCREMENT, taskId INTEGER DEFAULT 0, taskName TEXT DEFAULT '', triggerSource TEXT DEFAULT '', startTime INTEGER DEFAULT 0, finishTime INTEGER DEFAULT 0, durationMs INTEGER DEFAULT 0, status TEXT DEFAULT '', exitCode INTEGER DEFAULT 0, stdoutText TEXT DEFAULT '', stderrText TEXT DEFAULT '', message TEXT DEFAULT '')", NULL, NULL, NULL);
	sqlite3_exec(G_DB, "CREATE INDEX IF NOT EXISTS idx_sched_task_due ON sched_task(enabled, isDelete, nextRunAt)", NULL, NULL, NULL);
	sqlite3_exec(G_DB, "CREATE INDEX IF NOT EXISTS idx_sched_run_log_task ON sched_run_log(taskId, id)", NULL, NULL, NULL);

	G_SchedLock = xrtMutexCreate();
	G_SchedCond = xrtCondCreate();
	G_SchedStop = false;
	G_SchedWorkerCount = 0;
	memset(G_SchedWorkers, 0, sizeof(G_SchedWorkers));
	G_SchedThread = xrtThreadCreate(Sched_ThreadProc, NULL, 0);
}

void Sched_Unit(void)
{
	printf("        Sched_Unit \n");
	size_t i;

	if (G_SchedLock && G_SchedCond) {
		xrtMutexLock(G_SchedLock);
		G_SchedStop = true;
		xrtCondBroadcast(G_SchedCond);
		xrtMutexUnlock(G_SchedLock);
	}
	if (G_SchedThread) {
		xrtThreadWait(G_SchedThread);
		xrtThreadDestroy(G_SchedThread);
		G_SchedThread = NULL;
	}
	/* 等待在途 worker 归零：worker 结束时仍会触碰全局锁/条件变量，
	 * 提前销毁即释放后使用（攻击浸泡 71 分钟崩溃实测）。上限覆盖
	 * 任务 timeoutSec 上限（默认 300s）并留裕量。 */
	for (i = 0; i < 660; i++) {
		int pending;
		xrtMutexLock(G_SchedLock);
		pending = G_SchedWorkerCount;
		xrtMutexUnlock(G_SchedLock);
		if (pending <= 0) break;
		xrtSleepUs(1000000);
	}
	if (G_SchedCond) { xrtCondDestroy(G_SchedCond); G_SchedCond = NULL; }
	if (G_SchedLock) { xrtMutexDestroy(G_SchedLock); G_SchedLock = NULL; }
	xrtFree(G_SchedPath); G_SchedPath = NULL;
	xrtFree(G_SchedCachePath); G_SchedCachePath = NULL;
	xrtFree(G_SchedXSPath); G_SchedXSPath = NULL;
}
