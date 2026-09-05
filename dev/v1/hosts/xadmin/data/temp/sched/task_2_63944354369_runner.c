#include <xsbase.h>
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
	xvalue custom_data;
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

int TaskProc(TaskInfo* pTaskInfo)
{
	if ( pTaskInfo == NULL ) {
		return 1;
	}
	snprintf(pTaskInfo->output, sizeof(pTaskInfo->output),
		"task_id=%lld\nschedule=%s\ntrigger=%s\nwork_dir=%s\ncustom=%s",
		(long long)pTaskInfo->task_id,
		pTaskInfo->schedule_type ? pTaskInfo->schedule_type : "unknown",
		pTaskInfo->trigger_source ? pTaskInfo->trigger_source : "unknown",
		pTaskInfo->work_dir ? pTaskInfo->work_dir : "",
		pTaskInfo->custom_json ? pTaskInfo->custom_json : "{}");
	return 0;
}

void ServiceInit(XS_ServerObject objServer, XS_HostObject objHost)
{
	const char* sParamPath = xsServerParam(objServer);
	xvalue tblParam = NULL;
	TaskInfo tTaskInfo;
	int iRet;

	(void)objHost;
	if ( sParamPath == NULL || sParamPath[0] == '\0' ) {
		fputs("runner param is missing\n", stderr);
		fflush(stderr);
		exit(1);
	}

	tblParam = xrtParseJSON_File((str)sParamPath);
	if ( tblParam == NULL ) {
		fprintf(stderr, "failed to read runner param: %s\n", sParamPath);
		fflush(stderr);
		exit(1);
	}

	memset(&tTaskInfo, 0, sizeof(tTaskInfo));
	tTaskInfo.task_id = xvoTableGetInt(tblParam, "taskId", 6);
	tTaskInfo.task_name = xvoTableGetText(tblParam, "taskName", 8);
	tTaskInfo.trigger_source = xvoTableGetText(tblParam, "triggerSource", 13);
	tTaskInfo.schedule_type = xvoTableGetText(tblParam, "scheduleType", 12);
	tTaskInfo.exec_type = xvoTableGetText(tblParam, "execType", 8);
	tTaskInfo.shell_type = xvoTableGetText(tblParam, "shellType", 9);
	tTaskInfo.overlap_policy = xvoTableGetText(tblParam, "overlapPolicy", 13);
	tTaskInfo.custom_json = xvoTableGetText(tblParam, "customJson", 10);
	tTaskInfo.work_dir = xvoTableGetText(tblParam, "workDir", 7);
	tTaskInfo.app_path = xvoTableGetText(tblParam, "appPath", 7);
	tTaskInfo.sched_path = xvoTableGetText(tblParam, "schedPath", 9);
	tTaskInfo.cache_path = xvoTableGetText(tblParam, "cachePath", 9);
	tTaskInfo.runner_file = xvoTableGetText(tblParam, "runnerFile", 10);
	tTaskInfo.runner_config_file = xvoTableGetText(tblParam, "runnerConfigFile", 16);
	tTaskInfo.runner_param_file = xvoTableGetText(tblParam, "runnerParamFile", 15);
	tTaskInfo.start_time = xvoTableGetInt(tblParam, "startTime", 9);
	tTaskInfo.timeout_sec = (int)xvoTableGetInt(tblParam, "timeoutSec", 10);
	if ( tTaskInfo.custom_json && tTaskInfo.custom_json[0] != '\0' ) {
		tTaskInfo.custom_data = xrtParseJSON((str)tTaskInfo.custom_json, strlen(tTaskInfo.custom_json));
		if ( tTaskInfo.custom_data == NULL ) {
			fprintf(stderr, "invalid customJson: %s\n", tTaskInfo.custom_json);
			xvoUnref(tblParam);
			fflush(stderr);
			exit(2);
		}
	}

	iRet = TaskProc(&tTaskInfo);
	if ( tTaskInfo.output[0] != '\0' ) {
		fputs(tTaskInfo.output, stdout);
		fflush(stdout);
	}

	if ( tTaskInfo.custom_data ) {
		xvoUnref(tTaskInfo.custom_data);
	}
	xvoUnref(tblParam);
	fflush(stderr);
	exit(iRet);
}

void ServiceUnit(XS_ServerObject objServer, XS_HostObject objHost)
{
	(void)objServer;
	(void)objHost;
}
