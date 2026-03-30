#ifndef XS_VNEXT_FULL_H
#define XS_VNEXT_FULL_H

#include "xrt.h"
#include "inline_libtcc.h"
#include "inline_sqlite3.h"
#include "xs_vnext.h"

xrtGlobalData* g_pXCore = NULL;
void* g_pXsLoop = NULL;
xarray g_arrXsServerList = NULL;

static inline void XS_SetGlobalDate(int idx, void* ptr)
{
	if ( idx == 1 ) {
		g_pXsLoop = ptr;
	} else if ( idx == 2 ) {
		g_arrXsServerList = (xarray)ptr;
	} else if ( idx == 3 ) {
		g_pXCore = (xrtGlobalData*)ptr;
	}
}

typedef struct {
	uint16 KeySize;
	uint16 ValSize;
} XTP_ParamInfo;

typedef struct XTP_Message {
	uint16 Flags;
	uint16 MsgType;
	uint64 MsgID;
	int32 Status;
	uint32 PackSize;
	char* pPackBuf;
	const char* pCmd;
	uint16 CmdSize;
	const XTP_ParamInfo* pParamInfo;
	const char* pParamData;
	uint16 ParamCount;
	const char* pBody;
	uint32 BodySize;
} XTP_Message, *XTP_MessageObject;

int64_t xsDataRegister(xvalue objValue);
int64_t xsDataRegisterEx(xvalue objValue, const char* sNamespace, const char* sTag, int64_t iTTL);
xvalue xsDataGet(int64_t iID);
int xsDataRetain(int64_t iID);
int xsDataRelease(int64_t iID);
int xsDataRemove(int64_t iID);
int64_t xsDataRemoveByQuery(const char* sNamespace, const char* sTag, int iLimit);
int64_t xsDataFindFirst(const char* sNamespace, const char* sTag);
char* xsBusStatusJson(void);
char* xsBusStatusJsonEx(const char* sNamespace, const char* sTag);
char* xsBusNamespaceJson(void);
int xsBusLastErrorCode(void);
const char* xsBusLastError(void);
int xsMsgSendToHost(const char* sServer, const char* sHost, const char* sTopic, int64_t iDataID, xvalue objArgs);
int xsMsgSendToServer(const char* sServer, const char* sTopic, int64_t iDataID, xvalue objArgs);
int xsMsgBroadcast(const char* sTopic, int64_t iDataID, xvalue objArgs);

#endif
