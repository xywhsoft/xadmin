
// 引入 xs 开发框架
#include <xsbase.h>

#include <stdarg.h>
#include <string.h>



// 固定相对路径定义
#define INIT_HOST_REL			"hosts/xadmin"
#define INIT_DATA_REL			"hosts/xadmin/data"
#define INIT_INSTALL_REL		"hosts/xadmin/data/install"
#define INIT_BACKUP_REL			"tools/init/backup"



// 初始化工具上下文
typedef struct {
	str RootPath;
	str HostPath;
	str DataPath;
	str InstallPath;
	str BackupRootPath;
	str BackupSessionPath;
} InitContext;



// 需要清理的运行态目录
typedef struct {
	const char* sDesc;
	const char* sRelPath;
} InitMoveTask;



// 全局上下文
static InitContext G_InitCtx = { 0 };



// 统一输出日志
static void InitLog(const char* sFormat, ...)
{
	va_list args;
	
	printf("[xadmin:init] ");
	va_start(args, sFormat);
	vprintf(sFormat, args);
	va_end(args);
	printf("\n");
	fflush(stdout);
}



// 输出错误日志
static void InitError(const char* sFormat, ...)
{
	va_list args;
	
	printf("[xadmin:init][ERROR] ");
	va_start(args, sFormat);
	vprintf(sFormat, args);
	va_end(args);
	printf("\n");
	fflush(stdout);
}



// 释放初始化工具上下文
static void InitContext_Unit(void)
{
	if ( G_InitCtx.BackupSessionPath ) {
		xrtFree(G_InitCtx.BackupSessionPath);
		G_InitCtx.BackupSessionPath = NULL;
	}
	
	if ( G_InitCtx.BackupRootPath ) {
		xrtFree(G_InitCtx.BackupRootPath);
		G_InitCtx.BackupRootPath = NULL;
	}
	
	if ( G_InitCtx.InstallPath ) {
		xrtFree(G_InitCtx.InstallPath);
		G_InitCtx.InstallPath = NULL;
	}
	
	if ( G_InitCtx.DataPath ) {
		xrtFree(G_InitCtx.DataPath);
		G_InitCtx.DataPath = NULL;
	}
	
	if ( G_InitCtx.HostPath ) {
		xrtFree(G_InitCtx.HostPath);
		G_InitCtx.HostPath = NULL;
	}
	
	if ( G_InitCtx.RootPath ) {
		xrtFree(G_InitCtx.RootPath);
		G_InitCtx.RootPath = NULL;
	}
}



// 按仓库根目录拼接绝对路径
static str InitBuildAbsPath(const char* sRelPath)
{
	if ( (G_InitCtx.RootPath == NULL) || (sRelPath == NULL) ) {
		return NULL;
	}
	
	return xrtPathJoin(2, G_InitCtx.RootPath, (str)sRelPath);
}



// 生成本次备份目录名称
static str InitBuildBackupSessionName(void)
{
	xtime now;
	int64 iYear;
	int iMonth;
	int iDay;
	int iHour;
	int iMinute;
	int iSecond;
	int iWeekday;
	int iDayOfYear;
	
	now = xrtNow();
	xrtDecodeSerial(now, &iYear, &iMonth, &iDay, &iHour, &iMinute, &iSecond, &iWeekday, &iDayOfYear);
	return xrtFormat("%04lld%02d%02d_%02d%02d%02d", iYear, iMonth, iDay, iHour, iMinute, iSecond);
}



// 初始化上下文路径，并创建本次备份目录
static bool InitContext_Init(void)
{
	str sSessionName = NULL;
	str sCandidate = NULL;
	int iSuffix = 0;
	
	G_InitCtx.RootPath = xrtCopyStr((str)xsAppPath(), 0);
	if ( G_InitCtx.RootPath == NULL ) {
		InitError("无法获取 xs 应用根目录。");
		return FALSE;
	}
	
	G_InitCtx.HostPath = InitBuildAbsPath(INIT_HOST_REL);
	G_InitCtx.DataPath = InitBuildAbsPath(INIT_DATA_REL);
	G_InitCtx.InstallPath = InitBuildAbsPath(INIT_INSTALL_REL);
	G_InitCtx.BackupRootPath = InitBuildAbsPath(INIT_BACKUP_REL);
	
	if ( (G_InitCtx.HostPath == NULL) || (G_InitCtx.DataPath == NULL) || (G_InitCtx.InstallPath == NULL) || (G_InitCtx.BackupRootPath == NULL) ) {
		InitError("初始化关键路径失败。");
		return FALSE;
	}
	
	if ( !xrtDirCreateAll(G_InitCtx.BackupRootPath) ) {
		InitError("无法创建备份根目录：%s", G_InitCtx.BackupRootPath);
		return FALSE;
	}
	
	sSessionName = InitBuildBackupSessionName();
	if ( sSessionName == NULL ) {
		InitError("生成备份目录名称失败。");
		return FALSE;
	}
	
	while ( TRUE ) {
		if ( sCandidate ) {
			xrtFree(sCandidate);
			sCandidate = NULL;
		}
		
		if ( iSuffix <= 0 ) {
			sCandidate = xrtPathJoin(2, G_InitCtx.BackupRootPath, sSessionName);
		} else {
			str sNameWithSuffix = xrtFormat("%s_%02d", sSessionName, iSuffix);
			sCandidate = xrtPathJoin(2, G_InitCtx.BackupRootPath, sNameWithSuffix);
			xrtFree(sNameWithSuffix);
		}
		
		if ( sCandidate == NULL ) {
			xrtFree(sSessionName);
			InitError("拼接备份目录路径失败。");
			return FALSE;
		}
		
		if ( !xrtPathExists(sCandidate) ) {
			break;
		}
		
		iSuffix++;
	}
	
	xrtFree(sSessionName);
	G_InitCtx.BackupSessionPath = sCandidate;
	
	if ( !xrtDirCreateAll(G_InitCtx.BackupSessionPath) ) {
		InitError("无法创建本次备份目录：%s", G_InitCtx.BackupSessionPath);
		return FALSE;
	}
	
	return TRUE;
}



// 将绝对路径转换为相对仓库根目录的路径
static str InitMakeRelativeToRoot(str sFullPath)
{
	size_t iRootLen;
	const char* sRelative;
	
	if ( (G_InitCtx.RootPath == NULL) || (sFullPath == NULL) ) {
		return NULL;
	}
	
	iRootLen = strlen(G_InitCtx.RootPath);
	if ( strncmp(sFullPath, G_InitCtx.RootPath, iRootLen) == 0 ) {
		sRelative = sFullPath + iRootLen;
		while ( (*sRelative == '\\') || (*sRelative == '/') ) {
			sRelative++;
		}
		return xrtCopyStr((str)sRelative, 0);
	}
	
	return xrtPathGetNameExt(sFullPath, 0);
}



// 根据源路径生成备份目标路径
static str InitBuildBackupTarget(str sSourcePath)
{
	str sRelative;
	str sTargetPath;
	
	if ( (G_InitCtx.BackupSessionPath == NULL) || (sSourcePath == NULL) ) {
		return NULL;
	}
	
	sRelative = InitMakeRelativeToRoot(sSourcePath);
	if ( sRelative == NULL ) {
		return NULL;
	}
	
	sTargetPath = xrtPathJoin(2, G_InitCtx.BackupSessionPath, sRelative);
	xrtFree(sRelative);
	return sTargetPath;
}



// 确保目标路径的父目录存在
static bool InitEnsureParentDir(str sTargetPath)
{
	str sParentDir;
	bool bOK;
	
	if ( sTargetPath == NULL ) {
		return FALSE;
	}
	
	sParentDir = xrtPathGetDir(sTargetPath, 0);
	if ( sParentDir == NULL ) {
		return FALSE;
	}
	
	bOK = xrtDirCreateAll(sParentDir);
	xrtFree(sParentDir);
	return bOK;
}



// 确保目录存在
static bool InitEnsureDir(str sDirPath, const char* sDesc)
{
	if ( sDirPath == NULL ) {
		InitError("%s 的目录路径为空。", sDesc ? sDesc : "目标");
		return FALSE;
	}
	
	if ( xrtDirExists(sDirPath) ) {
		return TRUE;
	}
	
	if ( !xrtDirCreateAll(sDirPath) ) {
		InitError("无法创建%s：%s", sDesc ? sDesc : "目录", sDirPath);
		return FALSE;
	}
	
	return TRUE;
}



// 将文件或目录移动到备份目录，并保留原目录结构
static bool InitMovePathToBackup(str sSourcePath, bool bIsDir, const char* sDesc)
{
	str sTargetPath = NULL;
	bool bExists;
	bool bOK = FALSE;
	int iMoveCount;
	
	if ( sSourcePath == NULL ) {
		InitError("%s 的源路径为空。", sDesc ? sDesc : "待备份对象");
		return FALSE;
	}
	
	bExists = bIsDir ? xrtDirExists(sSourcePath) : xrtFileExists(sSourcePath);
	if ( !bExists ) {
		InitLog("%s 不存在，跳过：%s", sDesc ? sDesc : "目标", sSourcePath);
		return TRUE;
	}
	
	sTargetPath = InitBuildBackupTarget(sSourcePath);
	if ( sTargetPath == NULL ) {
		InitError("生成备份目标路径失败：%s", sSourcePath);
		return FALSE;
	}
	
	if ( !InitEnsureParentDir(sTargetPath) ) {
		InitError("创建备份父目录失败：%s", sTargetPath);
		xrtFree(sTargetPath);
		return FALSE;
	}
	
	if ( bIsDir ) {
		iMoveCount = xrtDirMove(sSourcePath, sTargetPath, TRUE);
		bOK = iMoveCount >= 0;
	} else {
		bOK = xrtFileMove(sSourcePath, sTargetPath, TRUE);
	}
	
	if ( !bOK ) {
		InitError("%s 备份失败：%s -> %s", sDesc ? sDesc : "目标", sSourcePath, sTargetPath);
		xrtFree(sTargetPath);
		return FALSE;
	}
	
	InitLog("%s 已移动到备份目录：%s", sDesc ? sDesc : "目标", sTargetPath);
	xrtFree(sTargetPath);
	return TRUE;
}



// 按相对路径移动文件或目录到备份目录
static bool InitMoveRelativeToBackup(const char* sRelPath, bool bIsDir, const char* sDesc)
{
	str sSourcePath;
	bool bOK;
	
	sSourcePath = InitBuildAbsPath(sRelPath);
	if ( sSourcePath == NULL ) {
		InitError("拼接 %s 的绝对路径失败。", sDesc ? sDesc : "目标");
		return FALSE;
	}
	
	bOK = InitMovePathToBackup(sSourcePath, bIsDir, sDesc);
	xrtFree(sSourcePath);
	return bOK;
}



// 从安装模板复制单个文件到目标位置
static bool InitCopyInstallFile(const char* sInstallRelPath, const char* sTargetRelPath, const char* sDesc)
{
	str sSourcePath;
	str sTargetPath;
	bool bOK = FALSE;
	
	sSourcePath = InitBuildAbsPath(sInstallRelPath);
	sTargetPath = InitBuildAbsPath(sTargetRelPath);
	
	if ( (sSourcePath == NULL) || (sTargetPath == NULL) ) {
		InitError("拼接 %s 的复制路径失败。", sDesc ? sDesc : "文件");
		goto finish;
	}
	
	if ( !xrtFileExists(sSourcePath) ) {
		InitError("%s 模板文件不存在：%s", sDesc ? sDesc : "模板文件", sSourcePath);
		goto finish;
	}
	
	if ( !InitEnsureParentDir(sTargetPath) ) {
		InitError("创建 %s 的目标目录失败：%s", sDesc ? sDesc : "文件", sTargetPath);
		goto finish;
	}
	
	if ( !xrtFileCopy(sSourcePath, sTargetPath, TRUE) ) {
		InitError("%s 复制失败：%s -> %s", sDesc ? sDesc : "文件", sSourcePath, sTargetPath);
		goto finish;
	}
	
	InitLog("%s 已恢复：%s", sDesc ? sDesc : "文件", sTargetPath);
	bOK = TRUE;
	
finish:
	if ( sTargetPath ) {
		xrtFree(sTargetPath);
	}
	if ( sSourcePath ) {
		xrtFree(sSourcePath);
	}
	return bOK;
}



// 从安装模板复制整个目录到目标位置
static bool InitCopyInstallDir(const char* sInstallRelPath, const char* sTargetRelPath, const char* sDesc)
{
	str sSourcePath;
	str sTargetPath;
	int iCopyCount;
	bool bOK = FALSE;
	
	sSourcePath = InitBuildAbsPath(sInstallRelPath);
	sTargetPath = InitBuildAbsPath(sTargetRelPath);
	
	if ( (sSourcePath == NULL) || (sTargetPath == NULL) ) {
		InitError("拼接 %s 的目录路径失败。", sDesc ? sDesc : "目录");
		goto finish;
	}
	
	if ( !xrtDirExists(sSourcePath) ) {
		InitError("%s 模板目录不存在：%s", sDesc ? sDesc : "模板目录", sSourcePath);
		goto finish;
	}
	
	if ( !InitEnsureDir(sTargetPath, sDesc) ) {
		goto finish;
	}
	
	iCopyCount = xrtDirCopy(sSourcePath, sTargetPath, TRUE);
	if ( iCopyCount < 0 ) {
		InitError("%s 复制失败：%s -> %s", sDesc ? sDesc : "目录", sSourcePath, sTargetPath);
		goto finish;
	}
	
	InitLog("%s 已恢复：%s", sDesc ? sDesc : "目录", sTargetPath);
	bOK = TRUE;
	
finish:
	if ( sTargetPath ) {
		xrtFree(sTargetPath);
	}
	if ( sSourcePath ) {
		xrtFree(sSourcePath);
	}
	return bOK;
}



// 移除安装锁文件
static bool InitResetInstallLock(void)
{
	return InitMoveRelativeToBackup("hosts/xadmin/install.lock", FALSE, "安装锁文件");
}



// 重建主数据库目录，并恢复干净数据库
static bool InitResetDatabase(void)
{
	str sDBDirPath = NULL;
	bool bOK = FALSE;
	
	InitLog("开始恢复主数据库（同时清理 SQLite sidecar / journal 文件）...");
	
	if ( !InitMoveRelativeToBackup("hosts/xadmin/data/db", TRUE, "主数据库目录") ) {
		return FALSE;
	}
	
	sDBDirPath = InitBuildAbsPath("hosts/xadmin/data/db");
	if ( sDBDirPath == NULL ) {
		InitError("拼接主数据库目录路径失败。");
		goto finish;
	}
	
	if ( !InitEnsureDir(sDBDirPath, "主数据库目录") ) {
		goto finish;
	}
	
	if ( !InitCopyInstallFile("hosts/xadmin/data/install/main.db", "hosts/xadmin/data/db/main.db", "主数据库文件") ) {
		goto finish;
	}
	
	bOK = TRUE;
	
finish:
	if ( sDBDirPath ) {
		xrtFree(sDBDirPath);
	}
	return bOK;
}



// 重建 options 目录，并恢复安装模板配置
static bool InitResetOptions(void)
{
	str sOptionDirPath = NULL;
	bool bOK = FALSE;
	
	InitLog("开始恢复系统配置目录...");
	
	if ( !InitMoveRelativeToBackup("hosts/xadmin/data/options", TRUE, "系统配置目录") ) {
		return FALSE;
	}
	
	sOptionDirPath = InitBuildAbsPath("hosts/xadmin/data/options");
	if ( sOptionDirPath == NULL ) {
		InitError("拼接系统配置目录路径失败。");
		goto finish;
	}
	
	if ( !InitEnsureDir(sOptionDirPath, "系统配置目录") ) {
		goto finish;
	}
	
	if ( !InitCopyInstallDir("hosts/xadmin/data/install/options", "hosts/xadmin/data/options", "系统配置目录") ) {
		goto finish;
	}
	
	bOK = TRUE;
	
finish:
	if ( sOptionDirPath ) {
		xrtFree(sOptionDirPath);
	}
	return bOK;
}



// 清理运行态目录，并重新创建空目录
static bool InitCleanupRuntimeDirs(void)
{
	static const InitMoveTask arrTasks[] = {
		{ "日志目录", "hosts/xadmin/data/logs" },
		{ "临时目录", "hosts/xadmin/data/temp" },
		{ "上传目录", "hosts/xadmin/data/uploads" },
		{ "插件上传目录", "hosts/xadmin/data/uploads_plugin" },
		{ "插件私有数据目录", "hosts/xadmin/data/plugin" },
		{ "插件代际缓存目录", "hosts/xadmin/data/plugin_system" }
	};
	
	int i;
	
	InitLog("开始清理运行态目录...");
	InitLog("额外纳入初始化范围：data/plugin（插件配置/私有数据库）与 data/plugin_system（代际运行缓存）。");
	
	for ( i = 0; i < (int)(sizeof(arrTasks) / sizeof(arrTasks[0])); i++ ) {
		str sTargetDir = InitBuildAbsPath(arrTasks[i].sRelPath);
		bool bMoveOK;
		bool bEnsureOK;
		
		if ( sTargetDir == NULL ) {
			InitError("拼接 %s 路径失败。", arrTasks[i].sDesc);
			return FALSE;
		}
		
		bMoveOK = InitMovePathToBackup(sTargetDir, TRUE, arrTasks[i].sDesc);
		bEnsureOK = FALSE;
		if ( bMoveOK ) {
			bEnsureOK = InitEnsureDir(sTargetDir, arrTasks[i].sDesc);
		}
		
		xrtFree(sTargetDir);
		
		if ( !bMoveOK || !bEnsureOK ) {
			return FALSE;
		}
	}
	
	return TRUE;
}



// 执行初始化工具主流程
static bool InitRun(void)
{
	InitLog("开始执行 xAdmin 环境初始化工具...");
	
	if ( !InitContext_Init() ) {
		return FALSE;
	}
	
	InitLog("本次备份目录：%s", G_InitCtx.BackupSessionPath);
	
	if ( !InitResetInstallLock() ) {
		return FALSE;
	}
	
	if ( !InitResetDatabase() ) {
		return FALSE;
	}
	
	if ( !InitCleanupRuntimeDirs() ) {
		return FALSE;
	}
	
	if ( !InitResetOptions() ) {
		return FALSE;
	}
	
	InitLog("xAdmin 环境初始化完成。");
	InitLog("如需恢复误移动的数据，请检查备份目录：%s", G_InitCtx.BackupSessionPath);
	return TRUE;
}



// 服务初始化
void ServiceInit(XS_ServerObject objServer, XS_HostObject objHost)
{
	int iExitCode;
	
	(void)objServer;
	(void)objHost;
	
	iExitCode = InitRun() ? 0 : 1;
	
	InitContext_Unit();
	
	if ( iExitCode == 0 ) {
		InitLog("初始化工具执行完毕，准备退出 xs。");
	} else {
		InitError("初始化工具执行失败，准备退出 xs。");
	}
	
	exit(iExitCode);
}



// 服务卸载
void ServiceUnit(XS_ServerObject objServer, XS_HostObject objHost)
{
	(void)objServer;
	(void)objHost;
	
	// 当前工具在 ServiceInit 中直接退出进程，理论上不会走到这里。
	printf("[xadmin:init] ServiceUnit called.\n");
	fflush(stdout);
}


