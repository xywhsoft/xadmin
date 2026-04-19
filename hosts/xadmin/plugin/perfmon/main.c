#include "xs_plugin.h"

typedef struct {
	int iCollectInterval;
	int iHistoryRetentionDays;
} PMConfigState;

typedef struct {
	float cpu;
	float memory;
	int64 memory_total;
	int64 memory_used;
	float network_tx;
	float network_rx;
	float disk_read;
	float disk_write;
	xtime timestamp;
} PMData;

static XAdminPluginHandle G_PMHandle = NULL;
static sqlite3* G_PMMainDb = NULL;
static const char* G_PMXid = NULL;
static const char* G_PMRootPath = NULL;
static const char* G_PMDataPath = NULL;
static const char* G_PMPrivateDbPath = NULL;
static PMConfigState G_PMConfig = { 3, 30 };

static PMData G_PMLastData = {0};
static xtime G_PMLastTime = 0;
static int64 G_PMLastNetTx = 0;
static int64 G_PMLastNetRx = 0;
static int64 G_PMLastDiskRead = 0;
static int64 G_PMLastDiskWrite = 0;
static volatile long G_PMRunning = 0;

static const char* G_PMSchemaSql =
	"CREATE TABLE IF NOT EXISTS performance ("
	"id INTEGER PRIMARY KEY AUTOINCREMENT,"
	"cpu REAL NOT NULL,"
	"memory REAL NOT NULL,"
	"memory_total INTEGER NOT NULL,"
	"memory_used INTEGER NOT NULL,"
	"network_tx REAL NOT NULL,"
	"network_rx REAL NOT NULL,"
	"disk_read REAL NOT NULL,"
	"disk_write REAL NOT NULL,"
	"timestamp INTEGER NOT NULL"
	");"
	"CREATE INDEX IF NOT EXISTS idx_perf_timestamp ON performance(timestamp);";

XADMIN_EXPORT void XAdmin_PluginSetGlobalData(int idx, void* ptr)
{
	if ( idx == XADMIN_GLOBAL_MAIN_DB ) {
		G_PMMainDb = (sqlite3*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_XID ) {
		G_PMXid = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_ROOT_PATH ) {
		G_PMRootPath = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_DATA_PATH ) {
		G_PMDataPath = (const char*)ptr;
	} else if ( idx == XADMIN_GLOBAL_PLUGIN_PRIVATE_DB_PATH ) {
		G_PMPrivateDbPath = (const char*)ptr;
	}
}

void PM_SendJson(XS_ResponseObject objResp, xvalue tblData)
{
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblData, FALSE, &iSize);
	if ( sJson ) {
		http_reply(objResp, 200, "Content-Type: application/json\r\n", sJson, iSize);
		xrtFree(sJson);
	}
	xvoUnref(tblData);
}

xvalue PM_CreateResult(bool bResult, const char* sMessage)
{
	xvalue tblRet = xvoCreateTable();
	if ( tblRet == NULL ) return NULL;
	xvoTableSetBool(tblRet, "result", 6, bResult);
	if ( sMessage ) {
		xvoTableSetText(tblRet, "message", 7, (str)sMessage, 0, FALSE);
	}
	return tblRet;
}

void PM_SendError(XS_ResponseObject objResp, const char* sMessage)
{
	PM_SendJson(objResp, PM_CreateResult(FALSE, sMessage));
}

bool PM_SendAssetHtml(XS_ResponseObject objResp, const char* sFileName)
{
	str sPath;
	ptr pData;
	size_t iSize = 0;

	if ( (G_PMRootPath == NULL) || (sFileName == NULL) ) return FALSE;
	sPath = xrtPathJoin(2, G_PMRootPath, (str)sFileName);
	if ( (sPath == NULL) || !xrtFileExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		return FALSE;
	}
	pData = xrtFileGetAll(sPath, &iSize);
	xrtFree(sPath);
	if ( pData == NULL ) return FALSE;
	http_reply(objResp, 200, "Content-Type: text/html; charset=utf-8\r\n", pData, iSize);
	xrtFree(pData);
	return TRUE;
}

bool PM_OpenDb(sqlite3** ppDb)
{
	sqlite3* pDb = NULL;
	int iRet;
	if ( ppDb ) *ppDb = NULL;
	if ( (ppDb == NULL) || (G_PMPrivateDbPath == NULL) || (G_PMPrivateDbPath[0] == '\0') ) return FALSE;
	iRet = sqlite3_open_v2(G_PMPrivateDbPath, &pDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
	if ( iRet != SQLITE_OK ) {
		if ( pDb ) sqlite3_close(pDb);
		return FALSE;
	}
	sqlite3_busy_timeout(pDb, 3000);
	*ppDb = pDb;
	return TRUE;
}

void PM_CloseDb(sqlite3* pDb)
{
	if ( pDb ) sqlite3_close(pDb);
}

bool PM_EnsureSchema()
{
	sqlite3* pDb = NULL;
	char* sError = NULL;
	if ( !PM_OpenDb(&pDb) ) return FALSE;
	if ( sqlite3_exec(pDb, G_PMSchemaSql, NULL, NULL, &sError) != SQLITE_OK ) {
		if ( sError ) sqlite3_free(sError);
		PM_CloseDb(pDb);
		return FALSE;
	}
	if ( sError ) sqlite3_free(sError);
	PM_CloseDb(pDb);
	return TRUE;
}

str PM_ReadQuery(XS_RequestObject objReq, const char* sName)
{
	char sBuf[256];
	memset(sBuf, 0, sizeof(sBuf));
	HttpGetQueryVar(objReq, sName, sBuf, sizeof(sBuf));
	if ( sBuf[0] == '\0' ) return NULL;
	return xrtCopyStr(sBuf, 0);
}

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <iphlpapi.h>

#ifndef IF_TYPE_IEEE80211
#define IF_TYPE_IEEE80211 71
#endif

#ifndef PROCESSOR_ARCHITECTURE_ARM64
#define PROCESSOR_ARCHITECTURE_ARM64 12
#endif

static float PM_GetCPUUsage()
{
	static ULARGE_INTEGER s_lastIdle = {0};
	static ULARGE_INTEGER s_lastKernel = {0};
	static ULARGE_INTEGER s_lastUser = {0};
	static BOOL s_bFirst = TRUE;

	FILETIME idleTime, kernelTime, userTime;
	ULARGE_INTEGER idle, kernel, user;
	ULONGLONG idleDiff, kernelDiff, userDiff, totalDiff;
	float cpuUsage;

	if ( !GetSystemTimes(&idleTime, &kernelTime, &userTime) ) return 0.0f;

	idle.LowPart = idleTime.dwLowDateTime;
	idle.HighPart = idleTime.dwHighDateTime;
	kernel.LowPart = kernelTime.dwLowDateTime;
	kernel.HighPart = kernelTime.dwHighDateTime;
	user.LowPart = userTime.dwLowDateTime;
	user.HighPart = userTime.dwHighDateTime;

	if ( s_bFirst ) {
		s_lastIdle = idle;
		s_lastKernel = kernel;
		s_lastUser = user;
		s_bFirst = FALSE;
		return 0.0f;
	}

	idleDiff = idle.QuadPart - s_lastIdle.QuadPart;
	kernelDiff = kernel.QuadPart - s_lastKernel.QuadPart;
	userDiff = user.QuadPart - s_lastUser.QuadPart;
	totalDiff = kernelDiff + userDiff;

	s_lastIdle = idle;
	s_lastKernel = kernel;
	s_lastUser = user;

	if ( totalDiff == 0 ) return 0.0f;
	cpuUsage = (float)(totalDiff - idleDiff) * 100.0f / (float)totalDiff;
	return cpuUsage;
}

static void PM_GetMemoryInfo(int64* pTotal, int64* pUsed)
{
	MEMORYSTATUSEX memInfo;
	memInfo.dwLength = sizeof(MEMORYSTATUSEX);
	GlobalMemoryStatusEx(&memInfo);
	*pTotal = (int64)memInfo.ullTotalPhys;
	*pUsed = (int64)(memInfo.ullTotalPhys - memInfo.ullAvailPhys);
}

static void PM_GetNetworkStats(int64* pTx, int64* pRx)
{
	PMIB_IFTABLE pIfTable = NULL;
	DWORD dwSize = 0;
	int64 totalTx = 0, totalRx = 0;

	if ( GetIfTable(NULL, &dwSize, FALSE) == ERROR_INSUFFICIENT_BUFFER ) {
		pIfTable = (PMIB_IFTABLE)xrtMalloc(dwSize);
		if ( pIfTable ) {
			if ( GetIfTable(pIfTable, &dwSize, FALSE) == NO_ERROR ) {
				DWORD i;
				for ( i = 0; i < pIfTable->dwNumEntries; i++ ) {
					MIB_IFROW* pRow = &pIfTable->table[i];
					if ( pRow->dwType == MIB_IF_TYPE_LOOPBACK ) continue;
					if ( pRow->dwType != IF_TYPE_ETHERNET_CSMACD && pRow->dwType != IF_TYPE_IEEE80211 ) continue;
					if ( pRow->dwOperStatus == MIB_IF_OPER_STATUS_OPERATIONAL || pRow->dwOperStatus == MIB_IF_OPER_STATUS_CONNECTED ) {
						totalTx += pRow->dwOutOctets;
						totalRx += pRow->dwInOctets;
					}
				}
			}
			xrtFree(pIfTable);
		}
	}
	*pTx = totalTx;
	*pRx = totalRx;
}

static void PM_GetDiskIOStats(int64* pRead, int64* pWrite)
{
	*pRead = 0;
	*pWrite = 0;
}

static int PM_GetDiskList(str** ppMounts, int64** ppTotals, int64** ppUseds, int64** ppAvailables)
{
	int iCount = 0;
	DWORD drives = GetLogicalDrives();
	int i;
	str* pMounts = (str*)xrtMalloc(sizeof(str) * 26);
	int64* pTotals = (int64*)xrtMalloc(sizeof(int64) * 26);
	int64* pUseds = (int64*)xrtMalloc(sizeof(int64) * 26);
	int64* pAvailables = (int64*)xrtMalloc(sizeof(int64) * 26);

	for ( i = 0; i < 26; i++ ) {
		if ( drives & (1 << i) ) {
			char root[4] = {(char)('A' + i), ':', '\\', '\0'};
			ULARGE_INTEGER freeBytes, totalBytes, totalFree;
			if ( GetDiskFreeSpaceExA(root, &freeBytes, &totalBytes, &totalFree) ) {
				pMounts[iCount] = xrtFormat("%c:", 'A' + i);
				pTotals[iCount] = (int64)totalBytes.QuadPart;
				pAvailables[iCount] = (int64)freeBytes.QuadPart;
				pUseds[iCount] = (int64)(totalBytes.QuadPart - totalFree.QuadPart);
				iCount++;
			}
		}
	}

	*ppMounts = pMounts;
	*ppTotals = pTotals;
	*ppUseds = pUseds;
	*ppAvailables = pAvailables;
	return iCount;
}

#else

#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <sys/utsname.h>

static float PM_GetCPUUsage()
{
	static long long s_lastTotal = 0, s_lastIdle = 0;
	FILE* file = fopen("/proc/stat", "r");
	char buffer[256];
	long long user, nice, system, idle, iowait, irq, softirq, steal;
	long long total, totalDiff, idleDiff;
	float percent;

	if ( !file ) return 0.0f;
	fgets(buffer, sizeof(buffer), file);
	fclose(file);

	sscanf(buffer, "cpu %lld %lld %lld %lld %lld %lld %lld %lld",
		&user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);

	total = user + nice + system + idle + iowait + irq + softirq + steal;

	if ( s_lastTotal != 0 ) {
		totalDiff = total - s_lastTotal;
		idleDiff = idle - s_lastIdle;
		percent = (float)(totalDiff - idleDiff) / (float)totalDiff * 100.0f;
		s_lastTotal = total;
		s_lastIdle = idle;
		return percent;
	}

	s_lastTotal = total;
	s_lastIdle = idle;
	return 0.0f;
}

static void PM_GetMemoryInfo(int64* pTotal, int64* pUsed)
{
	struct sysinfo si;
	if ( sysinfo(&si) == 0 ) {
		*pTotal = (int64)si.totalram * si.mem_unit;
		*pUsed = (int64)(si.totalram - si.freeram - si.bufferram) * si.mem_unit;
	}
}

static void PM_GetNetworkStats(int64* pTx, int64* pRx)
{
	FILE* file = fopen("/proc/net/dev", "r");
	char buffer[256];
	int64 totalRx = 0, totalTx = 0;

	if ( !file ) { *pTx = 0; *pRx = 0; return; }

	fgets(buffer, sizeof(buffer), file);
	fgets(buffer, sizeof(buffer), file);

	while ( fgets(buffer, sizeof(buffer), file) ) {
		char iface[32];
		int64 rxBytes, txBytes;
		if ( sscanf(buffer, "%s %lld %*d %*d %*d %*d %*d %*d %*d %lld", iface, &rxBytes, &txBytes) == 3 ) {
			if ( strstr(iface, "lo:") == NULL ) {
				totalRx += rxBytes;
				totalTx += txBytes;
			}
		}
	}
	fclose(file);
	*pRx = totalRx;
	*pTx = totalTx;
}

static void PM_GetDiskIOStats(int64* pRead, int64* pWrite)
{
	FILE* file = fopen("/proc/diskstats", "r");
	char buffer[256];
	int64 totalRead = 0, totalWrite = 0;

	if ( !file ) { *pRead = 0; *pWrite = 0; return; }

	while ( fgets(buffer, sizeof(buffer), file) ) {
		int major, minor;
		char dev[32];
		long long reads, writes;

		if ( sscanf(buffer, "%d %d %s %*d %*d %lld %*d %*d %*d %lld", &major, &minor, dev, &reads, &writes) == 5 ) {
			if ( major >= 8 && strlen(dev) <= 4 ) {
				totalRead += reads * 512;
				totalWrite += writes * 512;
			}
		}
	}
	fclose(file);
	*pRead = totalRead;
	*pWrite = totalWrite;
}

static int PM_GetDiskList(str** ppMounts, int64** ppTotals, int64** ppUseds, int64** ppAvailables)
{
	FILE* file = fopen("/proc/mounts", "r");
	int iCount = 0;
	char buffer[512];
	str* pMounts;
	int64* pTotals;
	int64* pUseds;
	int64* pAvailables;

	if ( !file ) { *ppMounts = NULL; *ppTotals = NULL; *ppUseds = NULL; *ppAvailables = NULL; return 0; }

	pMounts = (str*)xrtMalloc(sizeof(str) * 32);
	pTotals = (int64*)xrtMalloc(sizeof(int64) * 32);
	pUseds = (int64*)xrtMalloc(sizeof(int64) * 32);
	pAvailables = (int64*)xrtMalloc(sizeof(int64) * 32);

	while ( fgets(buffer, sizeof(buffer), file) && iCount < 32 ) {
		char dev[128], mount[256], type[32];
		if ( sscanf(buffer, "%s %s %s", dev, mount, type) == 3 ) {
			if ( strncmp(dev, "/dev/", 5) == 0 &&
				(strcmp(type, "ext4") == 0 || strcmp(type, "ext3") == 0 ||
				strcmp(type, "xfs") == 0 || strcmp(type, "btrfs") == 0) ) {
				struct statvfs vfs;
				if ( statvfs(mount, &vfs) == 0 ) {
					pMounts[iCount] = xrtCopyStr(mount, 0);
					pTotals[iCount] = (int64)(vfs.f_blocks * vfs.f_frsize);
					pAvailables[iCount] = (int64)(vfs.f_bavail * vfs.f_frsize);
					pUseds[iCount] = (int64)((vfs.f_blocks - vfs.f_bfree) * vfs.f_frsize);
					iCount++;
				}
			}
		}
	}
	fclose(file);

	*ppMounts = pMounts;
	*ppTotals = pTotals;
	*ppUseds = pUseds;
	*ppAvailables = pAvailables;
	return iCount;
}

#endif

static void PM_CollectData(PMData* pData)
{
	xtime now = xrtNow();
	float timeDiff = (float)(now - G_PMLastTime) / 1000.0f;
	int64 netTx, netRx, diskRead, diskWrite;

	pData->cpu = PM_GetCPUUsage();

	PM_GetMemoryInfo(&pData->memory_total, &pData->memory_used);
	pData->memory = pData->memory_total > 0 ? (float)pData->memory_used / (float)pData->memory_total * 100.0f : 0.0f;

	PM_GetNetworkStats(&netTx, &netRx);
	if ( G_PMLastTime > 0 && timeDiff > 0 && G_PMLastNetTx > 0 && G_PMLastNetRx > 0 ) {
		int64 txDiff = netTx - G_PMLastNetTx;
		int64 rxDiff = netRx - G_PMLastNetRx;
		if ( txDiff >= 0 && rxDiff >= 0 ) {
			pData->network_tx = (float)txDiff / timeDiff / 1024.0f / 1024.0f;
			pData->network_rx = (float)rxDiff / timeDiff / 1024.0f / 1024.0f;
		} else {
			pData->network_tx = 0.0f;
			pData->network_rx = 0.0f;
		}
	} else {
		pData->network_tx = 0.0f;
		pData->network_rx = 0.0f;
	}
	G_PMLastNetTx = netTx;
	G_PMLastNetRx = netRx;

	PM_GetDiskIOStats(&diskRead, &diskWrite);
	if ( G_PMLastTime > 0 && timeDiff > 0 && G_PMLastDiskRead > 0 && G_PMLastDiskWrite > 0 ) {
		int64 readDiff = diskRead - G_PMLastDiskRead;
		int64 writeDiff = diskWrite - G_PMLastDiskWrite;
		if ( readDiff >= 0 && writeDiff >= 0 ) {
			pData->disk_read = (float)readDiff / timeDiff / 1024.0f / 1024.0f;
			pData->disk_write = (float)writeDiff / timeDiff / 1024.0f / 1024.0f;
		} else {
			pData->disk_read = 0.0f;
			pData->disk_write = 0.0f;
		}
	} else {
		pData->disk_read = 0.0f;
		pData->disk_write = 0.0f;
	}
	G_PMLastDiskRead = diskRead;
	G_PMLastDiskWrite = diskWrite;

	pData->timestamp = now;
	G_PMLastTime = now;
}

#ifdef _WIN32
static DWORD WINAPI PM_CollectThread(LPVOID param)
#else
static void* PM_CollectThread(void* param)
#endif
{
	int iCleanupCounter = 0;
	int iInterval = G_PMConfig.iCollectInterval;
	if ( iInterval < 1 ) iInterval = 3;

	while ( G_PMRunning ) {
		PMData data;
		sqlite3* pDb;
		sqlite3_stmt* stmt;

		memset(&data, 0, sizeof(data));
		PM_CollectData(&data);
		G_PMLastData = data;

		if ( PM_OpenDb(&pDb) ) {
			if ( sqlite3_prepare_v2(pDb,
				"INSERT INTO performance (cpu, memory, memory_total, memory_used, network_tx, network_rx, disk_read, disk_write, timestamp) "
				"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);",
				-1, &stmt, NULL) == SQLITE_OK ) {
				sqlite3_bind_double(stmt, 1, data.cpu);
				sqlite3_bind_double(stmt, 2, data.memory);
				sqlite3_bind_int64(stmt, 3, data.memory_total);
				sqlite3_bind_int64(stmt, 4, data.memory_used);
				sqlite3_bind_double(stmt, 5, data.network_tx);
				sqlite3_bind_double(stmt, 6, data.network_rx);
				sqlite3_bind_double(stmt, 7, data.disk_read);
				sqlite3_bind_double(stmt, 8, data.disk_write);
				sqlite3_bind_int64(stmt, 9, data.timestamp);
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);
			}
			PM_CloseDb(pDb);
		}

		iCleanupCounter++;
		if ( iCleanupCounter >= 600 ) {
			xtime iCutoff = xrtNow() - ((int64)G_PMConfig.iHistoryRetentionDays * 24 * 60 * 60 * 1000);
			str sSQL = xrtFormat("DELETE FROM performance WHERE timestamp < %lld;", iCutoff);
			if ( PM_OpenDb(&pDb) ) {
				sqlite3_exec(pDb, sSQL, NULL, NULL, NULL);
				PM_CloseDb(pDb);
			}
			xrtFree(sSQL);
			iCleanupCounter = 0;
		}

#ifdef _WIN32
		Sleep(iInterval * 1000);
#else
		sleep(iInterval);
#endif
	}

#ifdef _WIN32
	return 0;
#else
	return NULL;
#endif
}

static void PM_StartCollectThread()
{
	G_PMRunning = 1;
#ifdef _WIN32
	CreateThread(NULL, 0, PM_CollectThread, NULL, 0, NULL);
#else
	{
		pthread_t thread;
		pthread_create(&thread, NULL, PM_CollectThread, NULL);
		pthread_detach(thread);
	}
#endif
}

static void PM_StopCollectThread()
{
	G_PMRunning = 0;
}

#ifdef _WIN32

static void PM_GetServerInfo(xvalue tblData)
{
	char computerName[256];
	DWORD size;
	SYSTEM_INFO sysInfo;
	HKEY hKey;
	char cpuName[256];
	DWORD bufSize;
	HMODULE hNtdll;
	typedef LONG (WINAPI *RtlGetVersionFunc)(PRTL_OSVERSIONINFOW);
	RTL_OSVERSIONINFOW ovi = {0};

	xvoTableSetText(tblData, "os_type", 7, (str)"Windows", 0, FALSE);

	size = sizeof(computerName);
	if ( GetComputerNameA(computerName, &size) ) {
		xvoTableSetText(tblData, "hostname", 8, xrtCopyStr(computerName, strlen((char*)computerName)), 0, TRUE);
	} else {
		xvoTableSetText(tblData, "hostname", 8, (str)"Unknown", 0, FALSE);
	}

	GetSystemInfo(&sysInfo);
	xvoTableSetInt(tblData, "cpu_cores", 9, sysInfo.dwNumberOfProcessors);

	switch ( sysInfo.wProcessorArchitecture ) {
		case PROCESSOR_ARCHITECTURE_AMD64:
			xvoTableSetText(tblData, "architecture", 12, (str)"x64", 0, FALSE); break;
		case PROCESSOR_ARCHITECTURE_INTEL:
			xvoTableSetText(tblData, "architecture", 12, (str)"x86", 0, FALSE); break;
		case PROCESSOR_ARCHITECTURE_ARM:
			xvoTableSetText(tblData, "architecture", 12, (str)"ARM", 0, FALSE); break;
		case PROCESSOR_ARCHITECTURE_ARM64:
			xvoTableSetText(tblData, "architecture", 12, (str)"ARM64", 0, FALSE); break;
		default:
			xvoTableSetText(tblData, "architecture", 12, (str)"Unknown", 0, FALSE); break;
	}

	hNtdll = GetModuleHandleA("ntdll.dll");
	if ( hNtdll ) {
		RtlGetVersionFunc pRtlGetVersion = (RtlGetVersionFunc)GetProcAddress(hNtdll, "RtlGetVersion");
		if ( pRtlGetVersion ) {
			ovi.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
			pRtlGetVersion(&ovi);
			if ( ovi.dwMajorVersion == 10 && ovi.dwBuildNumber >= 22000 ) {
				xvoTableSetText(tblData, "os_version", 10, xrtFormat("Windows 11 (Build %d)", ovi.dwBuildNumber), 0, TRUE);
			} else if ( ovi.dwMajorVersion == 10 ) {
				xvoTableSetText(tblData, "os_version", 10, xrtFormat("Windows 10 (Build %d)", ovi.dwBuildNumber), 0, TRUE);
			} else if ( ovi.dwMajorVersion == 6 && ovi.dwMinorVersion == 3 ) {
				xvoTableSetText(tblData, "os_version", 10, (str)"Windows 8.1", 0, FALSE);
			} else if ( ovi.dwMajorVersion == 6 && ovi.dwMinorVersion == 2 ) {
				xvoTableSetText(tblData, "os_version", 10, (str)"Windows 8", 0, FALSE);
			} else if ( ovi.dwMajorVersion == 6 && ovi.dwMinorVersion == 1 ) {
				xvoTableSetText(tblData, "os_version", 10, (str)"Windows 7", 0, FALSE);
			} else {
				xvoTableSetText(tblData, "os_version", 10, xrtFormat("Windows %d.%d (Build %d)", ovi.dwMajorVersion, ovi.dwMinorVersion, ovi.dwBuildNumber), 0, TRUE);
			}
			xvoTableSetText(tblData, "kernel_version", 14, xrtFormat("%d.%d.%d", ovi.dwMajorVersion, ovi.dwMinorVersion, ovi.dwBuildNumber), 0, TRUE);
		}
	}

	if ( RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS ) {
		bufSize = sizeof(cpuName);
		if ( RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)cpuName, &bufSize) == ERROR_SUCCESS ) {
			char* start = cpuName;
			while ( *start == ' ' ) start++;
			xvoTableSetText(tblData, "cpu_model", 9, xrtCopyStr(start, strlen(start)), 0, TRUE);
		} else {
			xvoTableSetText(tblData, "cpu_model", 9, (str)"Unknown", 0, FALSE);
		}
		RegCloseKey(hKey);
	} else {
		xvoTableSetText(tblData, "cpu_model", 9, (str)"Unknown", 0, FALSE);
	}

	xvoTableSetInt(tblData, "uptime", 6, (int64)(GetTickCount64() / 1000));

	{
		PMIB_IFTABLE pIfTable = NULL;
		DWORD dwSize = 0;

		if ( GetIfTable(NULL, &dwSize, FALSE) == ERROR_INSUFFICIENT_BUFFER ) {
			pIfTable = (PMIB_IFTABLE)xrtMalloc(dwSize);
			if ( pIfTable && GetIfTable(pIfTable, &dwSize, FALSE) == NO_ERROR ) {
				DWORD i;
				for ( i = 0; i < pIfTable->dwNumEntries; i++ ) {
					MIB_IFROW* pRow = &pIfTable->table[i];
					if ( pRow->dwType == MIB_IF_TYPE_LOOPBACK ) continue;
					if ( pRow->dwType != IF_TYPE_ETHERNET_CSMACD && pRow->dwType != IF_TYPE_IEEE80211 ) continue;
					if ( pRow->dwOperStatus == MIB_IF_OPER_STATUS_OPERATIONAL || pRow->dwOperStatus == MIB_IF_OPER_STATUS_CONNECTED ) {
						if ( pRow->dwPhysAddrLen == 6 ) {
							xvoTableSetText(tblData, "mac_address", 11,
								xrtFormat("%02X:%02X:%02X:%02X:%02X:%02X",
									pRow->bPhysAddr[0], pRow->bPhysAddr[1],
									pRow->bPhysAddr[2], pRow->bPhysAddr[3],
									pRow->bPhysAddr[4], pRow->bPhysAddr[5]), 0, TRUE);
						}
						{
							PIP_ADAPTER_INFO pAdapterInfo = NULL;
							ULONG ulLen = sizeof(IP_ADAPTER_INFO);
							pAdapterInfo = (PIP_ADAPTER_INFO)xrtMalloc(ulLen);
							if ( GetAdaptersInfo(pAdapterInfo, &ulLen) == ERROR_BUFFER_OVERFLOW ) {
								xrtFree(pAdapterInfo);
								pAdapterInfo = (PIP_ADAPTER_INFO)xrtMalloc(ulLen);
							}
							if ( pAdapterInfo && GetAdaptersInfo(pAdapterInfo, &ulLen) == NO_ERROR ) {
								PIP_ADAPTER_INFO pAdapter = pAdapterInfo;
								while ( pAdapter ) {
									if ( pAdapter->AddressLength == 6 && memcmp(pAdapter->Address, pRow->bPhysAddr, 6) == 0 ) {
										xvoTableSetText(tblData, "ip_address", 10,
											xrtCopyStr(pAdapter->IpAddressList.IpAddress.String,
												strlen(pAdapter->IpAddressList.IpAddress.String)), 0, TRUE);
										break;
									}
									pAdapter = pAdapter->Next;
								}
							}
							if ( pAdapterInfo ) xrtFree(pAdapterInfo);
						}
						break;
					}
				}
			}
			if ( pIfTable ) xrtFree(pIfTable);
		}
	}
}

#else

static void PM_GetServerInfo(xvalue tblData)
{
	char hostname[256];
	int cpuCores;
	struct utsname uts;
	FILE* file;
	char buffer[256];

	xvoTableSetText(tblData, "os_type", 7, (str)"Linux", 0, FALSE);

	if ( gethostname(hostname, sizeof(hostname)) == 0 ) {
		xvoTableSetText(tblData, "hostname", 8, xrtCopyStr(hostname, strlen(hostname)), 0, TRUE);
	} else {
		xvoTableSetText(tblData, "hostname", 8, (str)"Unknown", 0, FALSE);
	}

	cpuCores = (int)sysconf(_SC_NPROCESSORS_ONLN);
	xvoTableSetInt(tblData, "cpu_cores", 9, cpuCores);

	if ( uname(&uts) == 0 ) {
		xvoTableSetText(tblData, "architecture", 12, xrtCopyStr(uts.machine, strlen(uts.machine)), 0, TRUE);
		xvoTableSetText(tblData, "kernel_version", 14, xrtCopyStr(uts.release, strlen(uts.release)), 0, TRUE);
	} else {
		xvoTableSetText(tblData, "architecture", 12, (str)"Unknown", 0, FALSE);
		xvoTableSetText(tblData, "kernel_version", 14, (str)"Unknown", 0, FALSE);
	}

	file = fopen("/etc/os-release", "r");
	if ( file ) {
		while ( fgets(buffer, sizeof(buffer), file) ) {
			if ( strncmp(buffer, "PRETTY_NAME=", 12) == 0 ) {
				char* start = buffer + 12;
				char* end;
				if ( *start == '"' ) start++;
				end = strchr(start, '"');
				if ( end ) *end = '\0';
				end = strchr(start, '\n');
				if ( end ) *end = '\0';
				xvoTableSetText(tblData, "os_version", 10, xrtCopyStr(start, strlen(start)), 0, TRUE);
				break;
			}
		}
		fclose(file);
	}
	if ( xvoTableGetText(tblData, "os_version", 10) == NULL ) {
		xvoTableSetText(tblData, "os_version", 10, (str)"Linux", 0, FALSE);
	}

	file = fopen("/proc/cpuinfo", "r");
	if ( file ) {
		while ( fgets(buffer, sizeof(buffer), file) ) {
			if ( strncmp(buffer, "model name", 10) == 0 ) {
				char* colon = strchr(buffer, ':');
				if ( colon ) {
					char* start = colon + 1;
					char* end;
					while ( *start == ' ' || *start == '\t' ) start++;
					end = strchr(start, '\n');
					if ( end ) *end = '\0';
					xvoTableSetText(tblData, "cpu_model", 9, xrtCopyStr(start, strlen(start)), 0, TRUE);
					break;
				}
			}
		}
		fclose(file);
	}
	if ( xvoTableGetText(tblData, "cpu_model", 9) == NULL ) {
		xvoTableSetText(tblData, "cpu_model", 9, (str)"Unknown", 0, FALSE);
	}

	{
		struct sysinfo si;
		if ( sysinfo(&si) == 0 ) {
			xvoTableSetInt(tblData, "uptime", 6, (int64)si.uptime);
		}
	}

	{
		char defaultIface[32] = {0};
		file = fopen("/proc/net/route", "r");
		if ( file ) {
			fgets(buffer, sizeof(buffer), file);
			while ( fgets(buffer, sizeof(buffer), file) ) {
				char iface[32];
				unsigned long destination;
				if ( sscanf(buffer, "%s %lx", iface, &destination) == 2 ) {
					if ( destination == 0 && strcmp(iface, "lo") != 0 ) {
						strncpy(defaultIface, iface, sizeof(defaultIface) - 1);
						break;
					}
				}
			}
			fclose(file);
		}

		if ( strlen(defaultIface) == 0 ) {
			if ( access("/sys/class/net/eth0", F_OK) == 0 ) strcpy(defaultIface, "eth0");
			else if ( access("/sys/class/net/ens33", F_OK) == 0 ) strcpy(defaultIface, "ens33");
			else if ( access("/sys/class/net/ens32", F_OK) == 0 ) strcpy(defaultIface, "ens32");
		}

		if ( strlen(defaultIface) > 0 ) {
			char macPath[256];
			snprintf(macPath, sizeof(macPath), "/sys/class/net/%s/address", defaultIface);
			file = fopen(macPath, "r");
			if ( file ) {
				char macAddr[32];
				if ( fgets(macAddr, sizeof(macAddr), file) ) {
					char* end = strchr(macAddr, '\n');
					if ( end ) *end = '\0';
					{
						char* p;
						for ( p = macAddr; *p; p++ ) *p = toupper(*p);
					}
					xvoTableSetText(tblData, "mac_address", 11, xrtCopyStr(macAddr, strlen(macAddr)), 0, TRUE);
				}
				fclose(file);
			}

			{
				int sock = socket(AF_INET, SOCK_DGRAM, 0);
				if ( sock >= 0 ) {
					struct ifreq ifr;
					memset(&ifr, 0, sizeof(ifr));
					strncpy(ifr.ifr_name, defaultIface, IFNAMSIZ - 1);
					if ( ioctl(sock, SIOCGIFADDR, &ifr) == 0 ) {
						struct sockaddr_in* addr = (struct sockaddr_in*)&ifr.ifr_addr;
						char* ipStr = inet_ntoa(addr->sin_addr);
						if ( ipStr ) {
							xvoTableSetText(tblData, "ip_address", 10, xrtCopyStr(ipStr, strlen(ipStr)), 0, TRUE);
						}
					}
					close(sock);
				}
			}
		}
	}

	if ( xvoTableGetText(tblData, "ip_address", 10) == NULL ) {
		xvoTableSetText(tblData, "ip_address", 10, (str)"Unknown", 0, FALSE);
	}
	if ( xvoTableGetText(tblData, "mac_address", 11) == NULL ) {
		xvoTableSetText(tblData, "mac_address", 11, (str)"Unknown", 0, FALSE);
	}
}

#endif

void PM_Req_ViewDashboard(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !PM_SendAssetHtml(objResp, "page/dashboard.html") ) {
		http_reply(objResp, 500, "Content-Type: text/plain\r\n", "page not found", 0);
	}
}

void PM_Req_ViewDetail(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !PM_SendAssetHtml(objResp, "page/performance.html") ) {
		http_reply(objResp, 500, "Content-Type: text/plain\r\n", "page not found", 0);
	}
}

void PM_Req_ApiCurrent(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	PMData data;
	xvalue tblRet, tblData;

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;

	data = G_PMLastData;

	tblRet = PM_CreateResult(TRUE, NULL);
	tblData = xvoCreateTable();
	xvoTableSetFloat(tblData, "cpu", 3, data.cpu);
	xvoTableSetFloat(tblData, "memory", 6, data.memory);
	xvoTableSetInt(tblData, "memory_total", 12, data.memory_total);
	xvoTableSetInt(tblData, "memory_used", 11, data.memory_used);
	xvoTableSetFloat(tblData, "network_tx", 10, data.network_tx);
	xvoTableSetFloat(tblData, "network_rx", 10, data.network_rx);
	xvoTableSetFloat(tblData, "disk_read", 9, data.disk_read);
	xvoTableSetFloat(tblData, "disk_write", 10, data.disk_write);
	xvoTableSetInt(tblData, "timestamp", 9, data.timestamp);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	PM_SendJson(objResp, tblRet);
}

void PM_Req_ApiDisk(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	str* pMounts = NULL;
	int64* pTotals = NULL;
	int64* pUseds = NULL;
	int64* pAvailables = NULL;
	int iCount, i;
	xvalue tblRet, arrData;

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;

	iCount = PM_GetDiskList(&pMounts, &pTotals, &pUseds, &pAvailables);

	tblRet = PM_CreateResult(TRUE, NULL);
	arrData = xvoCreateArray();
	for ( i = 0; i < iCount; i++ ) {
		xvalue tblDisk = xvoCreateTable();
		xvoTableSetText(tblDisk, "mount", 5, pMounts[i], 0, FALSE);
		xvoTableSetInt(tblDisk, "total", 5, pTotals[i]);
		xvoTableSetInt(tblDisk, "used", 4, pUseds[i]);
		xvoTableSetInt(tblDisk, "available", 9, pAvailables[i]);
		xvoArrayAppendValue(arrData, tblDisk, TRUE);
	}
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	PM_SendJson(objResp, tblRet);

	for ( i = 0; i < iCount; i++ ) xrtFree(pMounts[i]);
	xrtFree(pMounts);
	xrtFree(pTotals);
	xrtFree(pUseds);
	xrtFree(pAvailables);
}

void PM_Req_ApiHistory(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	str sPeriod;
	xtime now = xrtNow();
	xtime startTime = now;
	int iLimit = 60;
	sqlite3* pDb;
	sqlite3_stmt* stmt;
	str sSQL;
	xvalue tblRet, arrData;
	int rc;

	(void)objServer; (void)objHost; (void)objSession;

	sPeriod = PM_ReadQuery(objReq, "period");
	if ( sPeriod ) {
		if ( strcmp((char*)sPeriod, "1h") == 0 ) { startTime = now - (1LL * 60 * 60 * 1000); iLimit = 60; }
		else if ( strcmp((char*)sPeriod, "6h") == 0 ) { startTime = now - (6LL * 60 * 60 * 1000); iLimit = 72; }
		else if ( strcmp((char*)sPeriod, "24h") == 0 ) { startTime = now - (24LL * 60 * 60 * 1000); iLimit = 96; }
		else if ( strcmp((char*)sPeriod, "7d") == 0 ) { startTime = now - (7LL * 24 * 60 * 60 * 1000); iLimit = 168; }
		else if ( strcmp((char*)sPeriod, "30d") == 0 ) { startTime = now - (30LL * 24 * 60 * 60 * 1000); iLimit = 720; }
		else { startTime = now - (1LL * 60 * 60 * 1000); }
		xrtFree(sPeriod);
	} else {
		startTime = now - (1LL * 60 * 60 * 1000);
	}

	sSQL = xrtFormat(
		"SELECT cpu, memory, network_tx, network_rx, disk_read, disk_write, timestamp "
		"FROM performance WHERE timestamp >= %lld ORDER BY timestamp DESC LIMIT %d;",
		startTime, iLimit);

	tblRet = PM_CreateResult(TRUE, NULL);
	arrData = xvoCreateArray();

	if ( PM_OpenDb(&pDb) ) {
		rc = sqlite3_prepare_v2(pDb, sSQL, -1, &stmt, NULL);
		if ( rc == SQLITE_OK ) {
			while ( sqlite3_step(stmt) == SQLITE_ROW ) {
				xvalue tblRow = xvoCreateTable();
				xvoTableSetFloat(tblRow, "cpu", 3, (float)sqlite3_column_double(stmt, 0));
				xvoTableSetFloat(tblRow, "memory", 6, (float)sqlite3_column_double(stmt, 1));
				xvoTableSetFloat(tblRow, "network_tx", 10, (float)sqlite3_column_double(stmt, 2));
				xvoTableSetFloat(tblRow, "network_rx", 10, (float)sqlite3_column_double(stmt, 3));
				xvoTableSetFloat(tblRow, "disk_read", 9, (float)sqlite3_column_double(stmt, 4));
				xvoTableSetFloat(tblRow, "disk_write", 10, (float)sqlite3_column_double(stmt, 5));
				xvoTableSetInt(tblRow, "timestamp", 9, sqlite3_column_int64(stmt, 6));
				xvoArrayAppendValue(arrData, tblRow, TRUE);
			}
			sqlite3_finalize(stmt);
		}
		PM_CloseDb(pDb);
	}

	xrtFree(sSQL);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	PM_SendJson(objResp, tblRet);
}

void PM_Req_ApiServerInfo(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xvalue tblRet, tblData;

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;

	tblRet = PM_CreateResult(TRUE, NULL);
	tblData = xvoCreateTable();
	PM_GetServerInfo(tblData);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	PM_SendJson(objResp, tblRet);
}

int PM_OnLoad(XAdminPluginHandle* out_handle)
{
	if ( out_handle ) G_PMHandle = *out_handle;
	return 0;
}

int PM_OnInstall(XAdminPluginHandle handle)
{
	(void)handle;
	PM_EnsureSchema();
	return 0;
}

int PM_OnStart(XAdminPluginHandle handle)
{
	XAdminMenuDecl menu;
	XAdminRouteDecl route;
	int iAuthGroupId = 0;
	int iAuthId = 0;
	XAdminAuthGroupDecl authGroup;
	XAdminAuthDecl auth;

	G_PMHandle = handle;

	if ( !PM_EnsureSchema() ) return -1;

	PM_StartCollectThread();

	memset(&authGroup, 0, sizeof(authGroup));
	authGroup.scope = XADMIN_AUTH_SCOPE_ADMIN;
	authGroup.name = "Performance Monitor";
	authGroup.description = "System performance monitoring permissions";
	authGroup.sort = 300000;
	if ( XAdmin_RegisterAuthGroup(handle, &authGroup, &iAuthGroupId, NULL) != 0 ) return -1;

	memset(&auth, 0, sizeof(auth));
	auth.scope = XADMIN_AUTH_SCOPE_ADMIN;
	auth.group_id = iAuthGroupId;
	auth.name = "perfmon.view";
	auth.description = "View performance monitoring data";
	auth.sort = 300001;
	if ( XAdmin_RegisterAuth(handle, &auth, &iAuthId, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/perfmon";
	route.proc = PM_Req_ViewDashboard;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/perfmon/detail";
	route.proc = PM_Req_ViewDetail;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/perfmon/current";
	route.proc = PM_Req_ApiCurrent;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/perfmon/disk";
	route.proc = PM_Req_ApiDisk;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/perfmon/history";
	route.proc = PM_Req_ApiHistory;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/perfmon/serverinfo";
	route.proc = PM_Req_ApiServerInfo;
	route.need_auth = TRUE;
	route.admin_only = TRUE;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&menu, 0, sizeof(menu));
	menu.title = "Perf Monitor";
	menu.icon = "layui-icon layui-icon-chart";
	menu.type = 1;
	menu.open_type = "_iframe";
	menu.href = "/admin/view/plugin/perfmon";
	menu.sort = 300;
	menu.visible = TRUE;
	menu.remark = "System performance monitoring";
	if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) return -1;

	memset(&menu, 0, sizeof(menu));
	menu.title = "Perf Detail";
	menu.icon = "layui-icon layui-icon-chart-screen";
	menu.type = 1;
	menu.open_type = "_iframe";
	menu.href = "/admin/view/plugin/perfmon/detail";
	menu.sort = 301;
	menu.visible = TRUE;
	menu.remark = "Detailed performance charts";
	if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) return -1;

	printf("[perfmon] started\n");
	return 0;
}

int PM_OnConfigChanged(XAdminPluginHandle handle, xvalue new_cfg)
{
	(void)handle;
	memset(&G_PMConfig, 0, sizeof(G_PMConfig));
	G_PMConfig.iCollectInterval = 3;
	G_PMConfig.iHistoryRetentionDays = 30;
	if ( new_cfg && (xvoType(new_cfg) == XVO_DT_TABLE) ) {
		int64 iVal = xvoTableGetInt(new_cfg, "collectInterval", 15);
		if ( iVal >= 1 && iVal <= 60 ) G_PMConfig.iCollectInterval = (int)iVal;
		iVal = xvoTableGetInt(new_cfg, "historyRetentionDays", 20);
		if ( iVal >= 1 && iVal <= 365 ) G_PMConfig.iHistoryRetentionDays = (int)iVal;
	}
	return 0;
}

int PM_OnHealthCheck(XAdminPluginHandle handle, XAdminHealthReport* out_report)
{
	(void)handle;
	if ( out_report ) {
		out_report->status_code = 0;
		out_report->message = "ok";
	}
	return 0;
}

void PM_OnStop(XAdminPluginHandle handle)
{
	(void)handle;
	PM_StopCollectThread();
	G_PMHandle = NULL;
	printf("[perfmon] stopped\n");
}

void PM_OnUnload(XAdminPluginHandle handle)
{
	(void)handle;
	G_PMHandle = NULL;
	G_PMMainDb = NULL;
	G_PMXid = NULL;
	G_PMRootPath = NULL;
	G_PMDataPath = NULL;
	G_PMPrivateDbPath = NULL;
}

static XAdminPluginDescriptor G_PMPlugin = {
	XADMIN_ABI_VERSION,
	sizeof(XAdminPluginDescriptor),
	"perfmon",
	"1.0.0",
	"Performance Monitor",
	PM_OnLoad,
	PM_OnInstall,
	PM_OnStart,
	PM_OnConfigChanged,
	PM_OnHealthCheck,
	PM_OnStop,
	PM_OnUnload
};

XADMIN_DECLARE_PLUGIN(G_PMPlugin)