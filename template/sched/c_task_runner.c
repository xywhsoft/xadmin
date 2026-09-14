#include <xsbase.h>
#include "value_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCHED_OUTPUT_CAP 65536

typedef struct {
	int64 task_id;
	const char* task_name;
	const char* trigger_source;
	const char* schedule_type;
	const char* exec_type;
	const char* shell_type;
	const char* overlap_policy;
	const char* custom_json;
	xvalue* custom_data;
	const char* work_dir;
	const char* app_path;
	const char* sched_path;
	const char* cache_path;
	const char* runner_file;
	const char* runner_config_file;
	const char* runner_param_file;
	xtime start_time;
	int timeout_sec;
	char output[SCHED_OUTPUT_CAP];
} TaskInfo;

{{$taskSourceCode}}

static str TaskParamText(xvalue* obj, const char* key)
{
	xstrview text = {0};
	xvalue* v = obj ? xrtValueObjectGet(obj, xrtStrView(key)) : NULL;
	if (!v || !xrtValueGetString(v, &text) || !text.Size) return NULL;
	return xrtStrDupN(text.Data, text.Size);
}

static int64 TaskParamInt(xvalue* obj, const char* key)
{
	int64 value = 0;
	xvalue* v = obj ? xrtValueObjectGet(obj, xrtStrView(key)) : NULL;
	if (v) (void)xrtValueGetInt(v, &value);
	return value;
}

void ServiceInit(XS_HostInfo* host)
{
	str paramPath = NULL;
	xvalue* tblParam = NULL;
	TaskInfo tTaskInfo;
	int ret;

	(void)host;
	/* 配置中的 param 字段落在 host->Custom（非预设字段集合） */
	if (host && host->Custom)
		paramPath = TaskParamText(host->Custom, "param");
	if (!paramPath || !paramPath[0]) {
		fputs("runner param is missing\n", stderr);
		fflush(stderr);
		exit(1);
	}
	tblParam = JsonParseFile(paramPath);
	xrtFree(paramPath);
	if (!tblParam) {
		fputs("failed to read runner param\n", stderr);
		fflush(stderr);
		exit(1);
	}

	memset(&tTaskInfo, 0, sizeof(tTaskInfo));
	tTaskInfo.task_id = TaskParamInt(tblParam, "taskId");
	tTaskInfo.task_name = TaskParamText(tblParam, "taskName");
	tTaskInfo.trigger_source = TaskParamText(tblParam, "triggerSource");
	tTaskInfo.schedule_type = TaskParamText(tblParam, "scheduleType");
	tTaskInfo.exec_type = TaskParamText(tblParam, "execType");
	tTaskInfo.shell_type = TaskParamText(tblParam, "shellType");
	tTaskInfo.overlap_policy = TaskParamText(tblParam, "overlapPolicy");
	tTaskInfo.custom_json = TaskParamText(tblParam, "customJson");
	tTaskInfo.work_dir = TaskParamText(tblParam, "workDir");
	tTaskInfo.app_path = TaskParamText(tblParam, "appPath");
	tTaskInfo.sched_path = TaskParamText(tblParam, "schedPath");
	tTaskInfo.cache_path = TaskParamText(tblParam, "cachePath");
	tTaskInfo.runner_file = TaskParamText(tblParam, "runnerFile");
	tTaskInfo.runner_config_file = TaskParamText(tblParam, "runnerConfigFile");
	tTaskInfo.runner_param_file = TaskParamText(tblParam, "runnerParamFile");
	tTaskInfo.start_time = TaskParamInt(tblParam, "startTime");
	tTaskInfo.timeout_sec = (int)TaskParamInt(tblParam, "timeoutSec");
	if (tTaskInfo.custom_json && tTaskInfo.custom_json[0]) {
		tTaskInfo.custom_data = JsonParseN(tTaskInfo.custom_json, 0);
		if (!tTaskInfo.custom_data) {
			fprintf(stderr, "invalid customJson: %s\n", tTaskInfo.custom_json);
			fflush(stderr);
			exit(2);
		}
	}

	ret = TaskProc(&tTaskInfo);
	if (tTaskInfo.output[0]) {
		fputs(tTaskInfo.output, stdout);
		fflush(stdout);
	}

	xrtValueRelease(tTaskInfo.custom_data);
	xrtValueRelease(tblParam);
	xrtFree((char*)tTaskInfo.task_name);
	xrtFree((char*)tTaskInfo.trigger_source);
	xrtFree((char*)tTaskInfo.schedule_type);
	xrtFree((char*)tTaskInfo.exec_type);
	xrtFree((char*)tTaskInfo.shell_type);
	xrtFree((char*)tTaskInfo.overlap_policy);
	xrtFree((char*)tTaskInfo.custom_json);
	xrtFree((char*)tTaskInfo.work_dir);
	xrtFree((char*)tTaskInfo.app_path);
	xrtFree((char*)tTaskInfo.sched_path);
	xrtFree((char*)tTaskInfo.cache_path);
	xrtFree((char*)tTaskInfo.runner_file);
	xrtFree((char*)tTaskInfo.runner_config_file);
	xrtFree((char*)tTaskInfo.runner_param_file);
	fflush(stderr);
	exit(ret);
}

void ServiceUnit(XS_HostInfo* host)
{
	(void)host;
}
