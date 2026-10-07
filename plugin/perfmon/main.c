#include "xs_plugin.h"
#include <sqlite3.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "value_util.h"
#include "util.h"
#define xrtFileGetAll(p, n) ((char*)xrtFileReadAll(p, n))

static str Managed_CopyStrN(const char* s, size_t n)
{
	str out;
	if (!s || !n) return xrtStrDup("");
	out = (str)xrtMalloc(n + 1);
	if (!out) return xrtStrDup("");
	memcpy(out, s, n);
	out[n] = '\0';
	return out;
}


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

void PM_SendJson(XS_ResponseObject objResp, xvalue* tblData)
{
	size_t iSize = 0;
	str sJson = xrtJsonStringify(tblData, false, &iSize);
	if ( sJson ) {
		xsHttpReplyAuto(objResp, 200, "Content-Type: application/json\r\n", sJson, iSize);
		xrtFree(sJson);
	}
	xrtValueRelease(tblData);
}

xvalue* PM_CreateResult(bool bResult, const char* sMessage)
{
	xvalue* tblRet = ValueObject();
	if ( tblRet == NULL ) return NULL;
	ValueSetBool(tblRet, "result", bResult);
	if ( sMessage ) {
		ValueSetText(tblRet, "message", (str)sMessage);
	}
	return tblRet;
}

void PM_SendError(XS_ResponseObject objResp, const char* sMessage)
{
	PM_SendJson(objResp, PM_CreateResult(false, sMessage));
}

bool PM_SendAssetHtml(XS_ResponseObject objResp, const char* sFileName)
{
	str sPath;
	ptr pData;
	size_t iSize = 0;

	if ( (G_PMRootPath == NULL) || (sFileName == NULL) ) return false;
	sPath = xrtPathJoin(G_PMRootPath, (str)sFileName);
	if ( (sPath == NULL) || !xrtFileExists(sPath) ) {
		if ( sPath ) xrtFree(sPath);
		return false;
	}
	pData = xrtFileGetAll(sPath, &iSize);
	xrtFree(sPath);
	if ( pData == NULL ) return false;
	xsHttpReplyAuto(objResp, 200, "Content-Type: text/html; charset=utf-8\r\n", pData, iSize);
	xrtFree(pData);
	return true;
}

bool PM_OpenDb(sqlite3** ppDb)
{
	sqlite3* pDb = NULL;
	int iRet;
	if ( ppDb ) *ppDb = NULL;
	if ( (ppDb == NULL) || (G_PMPrivateDbPath == NULL) || (G_PMPrivateDbPath[0] == '\0') ) return false;
	iRet = sqlite3_open_v2(G_PMPrivateDbPath, &pDb, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
	if ( iRet != SQLITE_OK ) {
		if ( pDb ) sqlite3_close(pDb);
		return false;
	}
	sqlite3_busy_timeout(pDb, 3000);
	*ppDb = pDb;
	return true;
}

void PM_CloseDb(sqlite3* pDb)
{
	if ( pDb ) sqlite3_close(pDb);
}

bool PM_EnsureSchema()
{
	sqlite3* pDb = NULL;
	char* sError = NULL;
	if ( !PM_OpenDb(&pDb) ) return false;
	if ( sqlite3_exec(pDb, G_PMSchemaSql, NULL, NULL, &sError) != SQLITE_OK ) {
		if ( sError ) sqlite3_free(sError);
		PM_CloseDb(pDb);
		return false;
	}
	if ( sError ) sqlite3_free(sError);
	PM_CloseDb(pDb);
	return true;
}

str PM_ReadQuery(XS_RequestObject objReq, const char* sName)
{
	char sBuf[256];
	memset(sBuf, 0, sizeof(sBuf));
	xsReqQueryValue(objReq, sName, sBuf, sizeof(sBuf));
	if ( sBuf[0] == '\0' ) return NULL;
	return xrtStrDup(sBuf);
}

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
/* iphlpapi 精确布局声明（TCC winapi VFS 无该头；布局与 Windows SDK 一致，
 * 字段用到 dwOutOctets/dwDescrLen 为止；链接经 build.libraries iphlpapi） */
typedef unsigned long DWORD_; typedef unsigned char BYTE_; typedef unsigned short WCHAR_;
typedef struct { WCHAR_ wszName[256]; DWORD_ dwIndex, dwType, dwMtu, dwSpeed, dwPhysAddrLen;
	BYTE_ bPhysAddr[8]; DWORD_ dwAdminStatus, dwOperStatus, dwLastChange, dwInOctets,
	dwInUcastPkts, dwInNUcastPkts, dwInDiscards, dwInErrors, dwInUnknownProtos,
	dwOutOctets, dwOutUcastPkts, dwOutNUcastPkts, dwOutDiscards, dwOutErrors,
	dwOutQLen, dwDescrLen; BYTE_ bDescr[256]; } MIB_IFROW;
typedef struct { DWORD_ dwNumEntries; MIB_IFROW table[1]; } MIB_IFTABLE_, *PMIB_IFTABLE_;
typedef struct { char String[16]; } PM_IPADDRSTRING_;
typedef struct { struct { char String[16]; } IpAddress; struct { char String[16]; } IpMask; DWORD_ Context; struct _PMLS_* Next; } PM_IPADDR_;
typedef struct _PMLS_ { struct _PMLS_* Next; void* ComboIndex; char AdapterName[260]; char Description[132]; UINT AddressLength; BYTE_ Address[8]; DWORD_ Index; UINT Type; UINT DhcpEnabled; void* CurrentIpAddress; PM_IPADDR_ IpAddressList; DWORD_ HaveWins; PM_IPADDR_ PrimaryWinsServer; PM_IPADDR_ SecondaryWinsServer; char LeaseObtained[8]; char LeaseExpires[8]; } IP_ADAPTER_INFO_;
typedef IP_ADAPTER_INFO_* PIP_ADAPTER_INFO_;
/* iphlpapi/ifmib 接口类型常量（官方取值） */
#define IF_TYPE_OTHER 1
#define IF_TYPE_REGULAR_1822 2
#define IF_TYPE_HDH_1822 3
#define IF_TYPE_DDN_X25 4
#define IF_TYPE_RFC877_X25 5
#define IF_TYPE_ETHERNET_CSMACD 6
#define IF_TYPE_ISO88023_CSMACD 7
#define IF_TYPE_ISO88024_TOKENBUS 8
#define IF_TYPE_ISO88025_TOKENRING 9
#define IF_TYPE_ISO88026_MAN 10
#define IF_TYPE_STARLAN 11
#define IF_TYPE_PROTEON_10MBIT 12
#define IF_TYPE_PROTEON_80MBIT 13
#define IF_TYPE_HYPERCHANNEL 14
#define IF_TYPE_FDDI 15
#define IF_TYPE_LAPB 16
#define IF_TYPE_SDLC 17
#define IF_TYPE_DS1 18
#define IF_TYPE_E1 19
#define IF_TYPE_BASIC_ISDN 20
#define IF_TYPE_PRIMARY_ISDN 21
#define IF_TYPE_PROP_POINT2POINT_SERIAL 22
#define IF_TYPE_PPP 23
#define IF_TYPE_SOFTWARE_LOOPBACK 24
#define IF_TYPE_EON 25
#define IF_TYPE_ETHERNET_3MBIT 26
#define IF_TYPE_NSIP 27
#define IF_TYPE_SLIP 28
#define IF_TYPE_ULTRA 29
#define IF_TYPE_DS3 30
#define IF_TYPE_SIP 31
#define IF_TYPE_FRAMERELAY 32
#define IF_TYPE_RS232 33
#define IF_TYPE_PARA 34
#define IF_TYPE_ARCNET 35
#define IF_TYPE_ATM 37
#define IF_TYPE_MIO_X25 38
#define IF_TYPE_SONET 39
#define IF_TYPE_X25_PLE 40
#define IF_TYPE_ISDN 41
#define IF_TYPE_V35 42
#define IF_TYPE_HSSI 43
#define IF_TYPE_HIPPI 44
#define IF_TYPE_MODEM 45
#define IF_TYPE_ADSL 46
#define IF_TYPE_SDSL 47
#define IF_TYPE_VDSL 48
#define IF_TYPE_IPIP 49
#define IF_TYPE_ISO88025_CRFPRINT 50
#define IF_TYPE_MYRINET 51
#define IF_TYPE_VOICE_EM 52
#define IF_TYPE_VOICE_FXO 53
#define IF_TYPE_VOICE_FXS 54
#define IF_TYPE_VOICE_ENCAP 55
#define IF_TYPE_VOICE_OVERIP 56
#define IF_TYPE_ATM_DXI 57
#define IF_TYPE_ATM_FUNI 58
#define IF_TYPE_ATM_IMA 59
#define IF_TYPE_PPPMULTILINK 60
#define IF_TYPE_IPOVER_CDLC 61
#define IF_TYPE_IPOVER_CLAW 62
#define IF_TYPE_STACKTOSTACK 63
#define IF_TYPE_VIRTUALIP 64
#define IF_TYPE_MPC 65
#define IF_TYPE_IPOVER_ATM 66
#define IF_TYPE_ISO88025_FIBER 67
#define IF_TYPE_TDLC 68
#define IF_TYPE_GIGABIT_ETHERNET 69
#define IF_TYPE_HDLC 70
#define IF_TYPE_LAP_F 71
#define IF_TYPE_V37 72
#define IF_TYPE_X25_MLP 73
#define IF_TYPE_X25_HUNTGROUP 74
#define IF_TYPE_TRANSPHDLC 75
#define IF_TYPE_INTERLEAVE 76
#define IF_TYPE_FAST 77
#define IF_TYPE_IP 78
#define IF_TYPE_DOCSCABLE_MACLAYER 79
#define IF_TYPE_DOCSCABLE_DOWNSTREAM 80
#define IF_TYPE_DOCSCABLE_UPSTREAM 81
#define IF_TYPE_A12MPPSWITCH 82
#define IF_TYPE_TUNNEL 83
#define IF_TYPE_COFFEE 84
#define IF_TYPE_CES 85
#define IF_TYPE_ATM_SUBINTERFACE 86
#define IF_TYPE_L2_VLAN 87
#define IF_TYPE_L3_IPVLAN 88
#define IF_TYPE_L3_IPFORWARD 89
#define IF_TYPE_L3_X25 90
#define IF_TYPE_L3_IPX 91
#define IF_TYPE_L3_CLNP 92
#define IF_TYPE_L3_APPLETALK 93
#define IF_TYPE_L3_DECNET 94
#define IF_TYPE_DNAL 95
#define IF_TYPE_BRIDGE 96
#define IF_TYPE_STATION 97
/* MIB 操作/管理状态常量（官方取值） */
__declspec(dllimport) unsigned long long __stdcall GetTickCount64(void);
#define MIB_IF_ADMIN_STATUS_UP 1
#define MIB_IF_ADMIN_STATUS_DOWN 2
#define MIB_IF_ADMIN_STATUS_TESTING 3
#define MIB_IF_OPER_STATUS_NON_OPERATIONAL 0
#define MIB_IF_OPER_STATUS_UNREACHABLE 1
#define MIB_IF_OPER_STATUS_DISCONNECTED 2
#define MIB_IF_OPER_STATUS_CONNECTING 3
#define MIB_IF_OPER_STATUS_CONNECTED 4
#define MIB_IF_OPER_STATUS_OPERATIONAL 5
#define IF_TYPE_IEEE 142
#define MIB_IF_TYPE_LOOPBACK 24
#define MIB_IF_TYPE_ETHERNET 6

#define MIB_IF_TYPE_ETHERNET 6
typedef IP_ADAPTER_INFO_ IP_ADAPTER_INFO;
typedef IP_ADAPTER_INFO_* PIP_ADAPTER_INFO;
typedef MIB_IFTABLE_* PMIB_IFTABLE;
__declspec(dllimport) DWORD_ __stdcall GetIfTable(MIB_IFTABLE_* pIfTable, DWORD_* pdwSize, int bOrder);
__declspec(dllimport) DWORD_ __stdcall GetAdaptersInfo(IP_ADAPTER_INFO_* pAdapterInfo, DWORD_* pOutBufLen);
#pragma comment(lib, "iphlpapi")

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
	static BOOL s_bFirst = true;

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
		s_bFirst = false;
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
	MIB_IFTABLE_* pIfTable = NULL;
	DWORD dwSize = 0;
	int64 totalTx = 0, totalRx = 0;

	if ( GetIfTable(NULL, &dwSize, false) == ERROR_INSUFFICIENT_BUFFER ) {
		pIfTable = (MIB_IFTABLE_*)xrtMalloc(dwSize);
		if ( pIfTable ) {
			if ( GetIfTable(pIfTable, &dwSize, false) == NO_ERROR ) {
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
					pMounts[iCount] = xrtStrDup(mount);
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
	xtime now = XAdmin_UnixNowUs();
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
			xtime iCutoff = XAdmin_UnixNowUs() - ((int64)G_PMConfig.iHistoryRetentionDays * 24 * 60 * 60 * 1000);
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

static void PM_GetServerInfo(xvalue* tblData)
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

	ValueSetText(tblData, "os_type", (str)"Windows");

	size = sizeof(computerName);
	if ( GetComputerNameA(computerName, &size) ) {
		ValueSetOwnedText(tblData, "hostname", Managed_CopyStrN(computerName, strlen((char*)computerName)));
	} else {
		ValueSetText(tblData, "hostname", (str)"Unknown");
	}

	GetSystemInfo(&sysInfo);
	ValueSetInt(tblData, "cpu_cores", sysInfo.dwNumberOfProcessors);

	switch ( sysInfo.wProcessorArchitecture ) {
		case PROCESSOR_ARCHITECTURE_AMD64:
			ValueSetText(tblData, "architecture", (str)"x64"); break;
		case PROCESSOR_ARCHITECTURE_INTEL:
			ValueSetText(tblData, "architecture", (str)"x86"); break;
		case PROCESSOR_ARCHITECTURE_ARM:
			ValueSetText(tblData, "architecture", (str)"ARM"); break;
		case PROCESSOR_ARCHITECTURE_ARM64:
			ValueSetText(tblData, "architecture", (str)"ARM64"); break;
		default:
			ValueSetText(tblData, "architecture", (str)"Unknown"); break;
	}

	hNtdll = GetModuleHandleA("ntdll.dll");
	if ( hNtdll ) {
		RtlGetVersionFunc pRtlGetVersion = (RtlGetVersionFunc)GetProcAddress(hNtdll, "RtlGetVersion");
		if ( pRtlGetVersion ) {
			ovi.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
			pRtlGetVersion(&ovi);
			if ( ovi.dwMajorVersion == 10 && ovi.dwBuildNumber >= 22000 ) {
				ValueSetOwnedText(tblData, "os_version", xrtFormat("Windows 11 (Build %d)", ovi.dwBuildNumber));
			} else if ( ovi.dwMajorVersion == 10 ) {
				ValueSetOwnedText(tblData, "os_version", xrtFormat("Windows 10 (Build %d)", ovi.dwBuildNumber));
			} else if ( ovi.dwMajorVersion == 6 && ovi.dwMinorVersion == 3 ) {
				ValueSetText(tblData, "os_version", (str)"Windows 8.1");
			} else if ( ovi.dwMajorVersion == 6 && ovi.dwMinorVersion == 2 ) {
				ValueSetText(tblData, "os_version", (str)"Windows 8");
			} else if ( ovi.dwMajorVersion == 6 && ovi.dwMinorVersion == 1 ) {
				ValueSetText(tblData, "os_version", (str)"Windows 7");
			} else {
				ValueSetOwnedText(tblData, "os_version", xrtFormat("Windows %d.%d (Build %d)", ovi.dwMajorVersion, ovi.dwMinorVersion, ovi.dwBuildNumber));
			}
			ValueSetOwnedText(tblData, "kernel_version", xrtFormat("%d.%d.%d", ovi.dwMajorVersion, ovi.dwMinorVersion, ovi.dwBuildNumber));
		}
	}

	if ( RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS ) {
		bufSize = sizeof(cpuName);
		if ( RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, (LPBYTE)cpuName, &bufSize) == ERROR_SUCCESS ) {
			char* start = cpuName;
			while ( *start == ' ' ) start++;
			ValueSetOwnedText(tblData, "cpu_model", Managed_CopyStrN(start, strlen(start)));
		} else {
			ValueSetText(tblData, "cpu_model", (str)"Unknown");
		}
		RegCloseKey(hKey);
	} else {
		ValueSetText(tblData, "cpu_model", (str)"Unknown");
	}

	ValueSetInt(tblData, "uptime", (int64)(GetTickCount64() / 1000));

	{
		MIB_IFTABLE_* pIfTable = NULL;
		DWORD dwSize = 0;

		if ( GetIfTable(NULL, &dwSize, false) == ERROR_INSUFFICIENT_BUFFER ) {
			pIfTable = (MIB_IFTABLE_*)xrtMalloc(dwSize);
			if ( pIfTable && GetIfTable(pIfTable, &dwSize, false) == NO_ERROR ) {
				DWORD i;
				for ( i = 0; i < pIfTable->dwNumEntries; i++ ) {
					MIB_IFROW* pRow = &pIfTable->table[i];
					if ( pRow->dwType == MIB_IF_TYPE_LOOPBACK ) continue;
					if ( pRow->dwType != IF_TYPE_ETHERNET_CSMACD && pRow->dwType != IF_TYPE_IEEE80211 ) continue;
					if ( pRow->dwOperStatus == MIB_IF_OPER_STATUS_OPERATIONAL || pRow->dwOperStatus == MIB_IF_OPER_STATUS_CONNECTED ) {
						if ( pRow->dwPhysAddrLen == 6 ) {
							ValueSetText(tblData, "mac_address", xrtFormat("%02X:%02X:%02X:%02X:%02X:%02X",
									pRow->bPhysAddr[0], pRow->bPhysAddr[1],
									pRow->bPhysAddr[2], pRow->bPhysAddr[3],
									pRow->bPhysAddr[4], pRow->bPhysAddr[5]));
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
										ValueSetText(tblData, "ip_address", Managed_CopyStrN(pAdapter->IpAddressList.IpAddress.String, strlen(pAdapter->IpAddressList.IpAddress.String)));
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

static void PM_GetServerInfo(xvalue* tblData)
{
	char hostname[256];
	int cpuCores;
	struct utsname uts;
	FILE* file;
	char buffer[256];

	ValueSetText(tblData, "os_type", (str)"Linux");

	if ( gethostname(hostname, sizeof(hostname)) == 0 ) {
		ValueSetOwnedText(tblData, "hostname", Managed_CopyStrN(hostname, strlen(hostname)));
	} else {
		ValueSetText(tblData, "hostname", (str)"Unknown");
	}

	cpuCores = (int)sysconf(_SC_NPROCESSORS_ONLN);
	ValueSetInt(tblData, "cpu_cores", cpuCores);

	if ( uname(&uts) == 0 ) {
		ValueSetOwnedText(tblData, "architecture", Managed_CopyStrN(uts.machine, strlen(uts.machine)));
		ValueSetOwnedText(tblData, "kernel_version", Managed_CopyStrN(uts.release, strlen(uts.release)));
	} else {
		ValueSetText(tblData, "architecture", (str)"Unknown");
		ValueSetText(tblData, "kernel_version", (str)"Unknown");
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
				ValueSetOwnedText(tblData, "os_version", Managed_CopyStrN(start, strlen(start)));
				break;
			}
		}
		fclose(file);
	}
	if ( ValueText(tblData, "os_version") == NULL ) {
		ValueSetText(tblData, "os_version", (str)"Linux");
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
					ValueSetOwnedText(tblData, "cpu_model", Managed_CopyStrN(start, strlen(start)));
					break;
				}
			}
		}
		fclose(file);
	}
	if ( ValueText(tblData, "cpu_model") == NULL ) {
		ValueSetText(tblData, "cpu_model", (str)"Unknown");
	}

	{
		struct sysinfo si;
		if ( sysinfo(&si) == 0 ) {
			ValueSetInt(tblData, "uptime", (int64)si.uptime);
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
					ValueSetOwnedText(tblData, "mac_address", Managed_CopyStrN(macAddr, strlen(macAddr)));
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
							ValueSetOwnedText(tblData, "ip_address", Managed_CopyStrN(ipStr, strlen(ipStr)));
						}
					}
					close(sock);
				}
			}
		}
	}

	if ( ValueText(tblData, "ip_address") == NULL ) {
		ValueSetText(tblData, "ip_address", (str)"Unknown");
	}
	if ( ValueText(tblData, "mac_address") == NULL ) {
		ValueSetText(tblData, "mac_address", (str)"Unknown");
	}
}

#endif

void PM_Req_ViewDashboard(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !PM_SendAssetHtml(objResp, "page/dashboard.html") ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain\r\n", "page not found", 0);
	}
}

void PM_Req_ViewDetail(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer; (void)objHost; (void)objReq; (void)objSession;
	if ( !PM_SendAssetHtml(objResp, "page/performance.html") ) {
		xsHttpReplyAuto(objResp, 500, "Content-Type: text/plain\r\n", "page not found", 0);
	}
}

void PM_Req_ApiCurrent(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	PMData data;
	xvalue* tblRet; xvalue* tblData;

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;

	data = G_PMLastData;

	tblRet = PM_CreateResult(true, NULL);
	tblData = ValueObject();
	ValueSetFloat(tblData, "cpu", data.cpu);
	ValueSetFloat(tblData, "memory", data.memory);
	ValueSetInt(tblData, "memory_total", data.memory_total);
	ValueSetInt(tblData, "memory_used", data.memory_used);
	ValueSetFloat(tblData, "network_tx", data.network_tx);
	ValueSetFloat(tblData, "network_rx", data.network_rx);
	ValueSetFloat(tblData, "disk_read", data.disk_read);
	ValueSetFloat(tblData, "disk_write", data.disk_write);
	ValueSetInt(tblData, "timestamp", data.timestamp);
	ValueSetOwn(tblRet, "data", tblData);
	PM_SendJson(objResp, tblRet);
}

void PM_Req_ApiDisk(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	str* pMounts = NULL;
	int64* pTotals = NULL;
	int64* pUseds = NULL;
	int64* pAvailables = NULL;
	int iCount, i;
	xvalue* tblRet; xvalue* arrData;

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;

	iCount = PM_GetDiskList(&pMounts, &pTotals, &pUseds, &pAvailables);

	tblRet = PM_CreateResult(true, NULL);
	arrData = ValueArray();
	for ( i = 0; i < iCount; i++ ) {
		xvalue* tblDisk = ValueObject();
		ValueSetText(tblDisk, "mount", pMounts[i]);
		ValueSetInt(tblDisk, "total", pTotals[i]);
		ValueSetInt(tblDisk, "used", pUseds[i]);
		ValueSetInt(tblDisk, "available", pAvailables[i]);
		ValueArrayOwn(arrData, tblDisk);
	}
	ValueSetOwn(tblRet, "data", arrData);
	PM_SendJson(objResp, tblRet);

	for ( i = 0; i < iCount; i++ ) xrtFree(pMounts[i]);
	xrtFree(pMounts);
	xrtFree(pTotals);
	xrtFree(pUseds);
	xrtFree(pAvailables);
}

void PM_Req_ApiHistory(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	str sPeriod;
	xtime now = XAdmin_UnixNowUs();
	xtime startTime = now;
	int iLimit = 60;
	sqlite3* pDb;
	sqlite3_stmt* stmt;
	str sSQL;
	xvalue* tblRet; xvalue* arrData;
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

	tblRet = PM_CreateResult(true, NULL);
	arrData = ValueArray();

	if ( PM_OpenDb(&pDb) ) {
		rc = sqlite3_prepare_v2(pDb, sSQL, -1, &stmt, NULL);
		if ( rc == SQLITE_OK ) {
			while ( sqlite3_step(stmt) == SQLITE_ROW ) {
				xvalue* tblRow = ValueObject();
				ValueSetFloat(tblRow, "cpu", (float)sqlite3_column_double(stmt, 0));
				ValueSetFloat(tblRow, "memory", (float)sqlite3_column_double(stmt, 1));
				ValueSetFloat(tblRow, "network_tx", (float)sqlite3_column_double(stmt, 2));
				ValueSetFloat(tblRow, "network_rx", (float)sqlite3_column_double(stmt, 3));
				ValueSetFloat(tblRow, "disk_read", (float)sqlite3_column_double(stmt, 4));
				ValueSetFloat(tblRow, "disk_write", (float)sqlite3_column_double(stmt, 5));
				ValueSetInt(tblRow, "timestamp", sqlite3_column_int64(stmt, 6));
				ValueArrayOwn(arrData, tblRow);
			}
			sqlite3_finalize(stmt);
		}
		PM_CloseDb(pDb);
	}

	xrtFree(sSQL);
	ValueSetOwn(tblRet, "data", arrData);
	PM_SendJson(objResp, tblRet);
}

void PM_Req_ApiServerInfo(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* tblRet; xvalue* tblData;

	(void)objServer; (void)objHost; (void)objReq; (void)objSession;

	tblRet = PM_CreateResult(true, NULL);
	tblData = ValueObject();
	PM_GetServerInfo(tblData);
	ValueSetOwn(tblRet, "data", tblData);
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
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/view/plugin/perfmon/detail";
	route.proc = PM_Req_ViewDetail;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/perfmon/current";
	route.proc = PM_Req_ApiCurrent;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/perfmon/disk";
	route.proc = PM_Req_ApiDisk;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/perfmon/history";
	route.proc = PM_Req_ApiHistory;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&route, 0, sizeof(route));
	route.path = "/admin/api/plugin/perfmon/serverinfo";
	route.proc = PM_Req_ApiServerInfo;
	route.need_auth = true;
	route.admin_only = true;
	route.auth_id = iAuthId;
	if ( XAdmin_RegisterRoute(handle, &route, NULL) != 0 ) return -1;

	memset(&menu, 0, sizeof(menu));
	menu.title = "Perf Monitor";
	menu.icon = "layui-icon layui-icon-chart";
	menu.type = 1;
	menu.open_type = "_iframe";
	menu.href = "/admin/view/plugin/perfmon";
	menu.sort = 300;
	menu.visible = true;
	menu.remark = "System performance monitoring";
	if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) return -1;

	memset(&menu, 0, sizeof(menu));
	menu.title = "Perf Detail";
	menu.icon = "layui-icon layui-icon-chart-screen";
	menu.type = 1;
	menu.open_type = "_iframe";
	menu.href = "/admin/view/plugin/perfmon/detail";
	menu.sort = 301;
	menu.visible = true;
	menu.remark = "Detailed performance charts";
	if ( XAdmin_RegisterMenu(handle, &menu, NULL, NULL) != 0 ) return -1;

	printf("[perfmon] started\n");
	return 0;
}

int PM_OnConfigChanged(XAdminPluginHandle handle, xvalue* new_cfg)
{
	(void)handle;
	memset(&G_PMConfig, 0, sizeof(G_PMConfig));
	G_PMConfig.iCollectInterval = 3;
	G_PMConfig.iHistoryRetentionDays = 30;
	if ( new_cfg && (xrtValueType(new_cfg) == XVALUE_OBJECT) ) {
		int64 iVal = ValueInt(new_cfg, "collectInterval");
		if ( iVal >= 1 && iVal <= 60 ) G_PMConfig.iCollectInterval = (int)iVal;
		iVal = ValueInt(new_cfg, "historyRetentionDays");
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