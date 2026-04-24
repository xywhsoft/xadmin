#ifndef XS_BASE_H
#define XS_BASE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>




#include "inline_xrt.h"
#include "inline_xsmtp.h"
#include "inline_lz4.h"
#include "inline_zstd.h"
#include "inline_lzma.h"
#include "inline_xpack.h"
#include "inline_libtcc.h"
#include "inline_sqlite3.h"
#include "inline_md4c.h"



// 语言类型定义
#define SLT_STATIC		0			// 静态页面
#define SLT_C			1			// C 语言
#define SLT_PROTOCOL	2			// 自定义接口





typedef void* XS_ServerObject;
typedef void* XS_HostObject;
typedef const void* XS_RequestObject;
typedef void* XS_ResponseObject;

void xsLog(const char* sText);
const char* xsServerName(XS_ServerObject objServer);
int xsServerClass(XS_ServerObject objServer);
int xsServerDebug(XS_ServerObject objServer);
const char* xsServerAddr(XS_ServerObject objServer);
const char* xsServerParam(XS_ServerObject objServer);
const char* xsAppPath(void);
const char* xsHostName(XS_HostObject objHost);
const char* xsHostParam(XS_HostObject objHost);
const char* xsHostPath(XS_HostObject objHost);
const char* xsHostDevFile(XS_HostObject objHost);
int xsHostDebug(XS_HostObject objHost);
int xsHostDevMode(XS_HostObject objHost);
const char* xsReqMethod(XS_RequestObject objReq);
unsigned xsReqMethodID(XS_RequestObject objReq);
const char* xsReqTarget(XS_RequestObject objReq);
const char* xsReqPath(XS_RequestObject objReq);
const char* xsReqQuery(XS_RequestObject objReq);
int xsReqQueryValue(XS_RequestObject objReq, const char* sName, char* sOut, size_t iOutCap);
int xsReqCookieValue(XS_RequestObject objReq, const char* sName, char* sOut, size_t iOutCap);
const void* xsReqBody(XS_RequestObject objReq);
size_t xsReqBodyLen(XS_RequestObject objReq);
const char* xsReqRemote(XS_RequestObject objReq);
const char* xsReqHeader(XS_RequestObject objReq, const char* sName);
int xsReqMultipartNext(XS_RequestObject objReq, size_t* pOffset, xrtmultipartpartview* pPart);
int xsMultipartNameIs(const xrtmultipartpartview* pPart, const char* sName);
int xsHttpStatus(XS_ResponseObject objResp, unsigned iStatus, const char* sReason);
int xsHttpHeader(XS_ResponseObject objResp, const char* sName, const char* sValue);
int xsHttpReply(XS_ResponseObject objResp, unsigned iStatus, const char* sReason, const char* sHeaders, const void* pBody, size_t iBodyLen);
int xsHttpReplyAuto(XS_ResponseObject objResp, unsigned iStatus, const char* sHeaders, const void* pBody, size_t iBodyLen);
int xsHttpReplyFormat(XS_ResponseObject objResp, unsigned iStatus, const char* sHeaders, const char* sFormat, ...);
int xsHttpStart(XS_ResponseObject objResp, unsigned iStatus, const char* sReason, const char* sHeaders);
int xsHttpSend(XS_ResponseObject objResp, const void* pData, size_t iLen);
int xsHttpEnd(XS_ResponseObject objResp);
int xsHttpText(XS_ResponseObject objResp, unsigned iStatus, const char* sReason, const char* sText);
int xsHttpBody(XS_ResponseObject objResp, const void* pData, size_t iLen, const char* sContentType);
int xsHttpJson(XS_ResponseObject objResp, unsigned iStatus, const char* sReason, const char* sJson);
int xsHttpJsonValueTake(XS_ResponseObject objResp, unsigned iStatus, xvalue objValue);
int xsHttpMarkdown(XS_ResponseObject objResp, unsigned iStatus, const char* sReason, const char* sMarkdown);
char* xsMarkdownToHtml(const char* sMarkdown);
char* xsMarkdownToHtmlEx(const char* sMarkdown, unsigned iParserFlags, unsigned iRendererFlags);
int xsWsIsOpen(void* pConn);
int xsWsSendText(void* pConn, const char* sText, size_t iLen);
int xsWsSendBinary(void* pConn, const void* pData, size_t iLen);
const char* xsWsProtocol(void* pConn);
int xsWsPing(void* pConn, const void* pData, size_t iLen);
int xsWsClose(void* pConn, unsigned short iCode, const char* sReason);
int xsStreamSend(void* pStream, const void* pData, size_t iLen);
int xsStreamClose(void* pStream, unsigned iFlags);
int xsXtpSend(void* pStream, const char* sCmd, size_t iCmdSize, unsigned iParamCount, const char** arrParam, const char** arrValue, const void* pBody, size_t iBodySize);
int xsXtpSendRequest(void* pStream, uint64_t iMsgID, const char* sCmd, size_t iCmdSize, unsigned iParamCount, const char** arrParam, const char** arrValue, const void* pBody, size_t iBodySize);
int xsXtpSendPush(void* pStream, const char* sCmd, size_t iCmdSize, unsigned iParamCount, const char** arrParam, const char** arrValue, const void* pBody, size_t iBodySize);
int xsXtpSendEvent(void* pStream, const char* sCmd, size_t iCmdSize, unsigned iParamCount, const char** arrParam, const char** arrValue, const void* pBody, size_t iBodySize);
int xsXtpSendEx(void* pStream, unsigned iMsgType, uint64_t iMsgID, unsigned iFlags, int iStatus, const char* sCmd, size_t iCmdSize, unsigned iParamCount, const char** arrParam, const char** arrValue, const void* pBody, size_t iBodySize);
const char* xsXtpGetParam(const void* pMsg, const char* sKey);
int xsXtpReply(void* pStream, const void* pReqMsg, const char* sCmd, size_t iCmdSize, unsigned iParamCount, const char** arrParam, const char** arrValue, const void* pBody, size_t iBodySize);
int xsXtpReplyEx(void* pStream, const void* pReqMsg, int iStatus, const char* sCmd, size_t iCmdSize, unsigned iParamCount, const char** arrParam, const char** arrValue, const void* pBody, size_t iBodySize);
int xsXtpReplySimple(void* pStream, const void* pReqMsg, int iStatus, const char* sCmd, const void* pBody, size_t iBodySize);
void* xsXtpReplyStart(void* pStream, const void* pReqMsg, int iStatus, const char* sCmd);
int xsXtpReplyParam(void* pReply, const char* sKey, const char* sValue);
int xsXtpReplyBody(void* pReply, const void* pBody, size_t iBodySize);
int xsXtpReplyEnd(void* pReply);
void xsXtpReplyAbort(void* pReply);
uint64_t xsXtpMsgId(const void* pMsg);
unsigned xsXtpMsgType(const void* pMsg);
unsigned xsXtpMsgFlags(const void* pMsg);
int xsXtpIsOK(const void* pMsg);
int xsXtpStatus(const void* pMsg);
const char* xsXtpCmd(const void* pMsg);
unsigned xsXtpCmdLen(const void* pMsg);
unsigned xsXtpCmdID(const void* pMsg);
unsigned xsXtpCmdIDFrom(const char* sCmd);
unsigned xsXtpCmdRegister(const char* sCmd, unsigned iCmdID);
const void* xsXtpBody(const void* pMsg);
unsigned xsXtpBodyLen(const void* pMsg);
char* xsXtpCmdDup(const void* pMsg, const char* sDefault);
char* xsXtpMetaText(const void* pMsg);
char* xsXtpMetaJson(const void* pMsg);
char* xsXtpResultJson(const void* pMsg);
char* xsXtpErrorJson(const void* pMsg, const char* sDefault);
char* xsXtpSummaryText(const void* pMsg);
char* xsXtpSummaryJson(const void* pMsg);
unsigned xsXtpParamCount(const void* pMsg);
int xsXtpNeedReply(const void* pMsg);
int xsXtpIsRequest(const void* pMsg);
int xsXtpIsResponse(const void* pMsg);
int xsXtpIsPush(const void* pMsg);
int xsXtpIsEvent(const void* pMsg);
int xsXtpCmdIs(const void* pMsg, const char* sCmd);
int xsXtpHasParam(const void* pMsg, const char* sKey);
const char* xsXtpParamText(const void* pMsg, const char* sKey, const char* sDefault);
const char* xsXtpResultText(const void* pMsg, const char* sDefault);
int xsXtpResultIs(const void* pMsg, const char* sResult);
int xsXtpStatusIs(const void* pMsg, int iStatus);
char* xsXtpErrorText(const void* pMsg, const char* sDefault);
char* xsXtpParamDup(const void* pMsg, const char* sKey, const char* sDefault);
int64_t xsXtpParamInt(const void* pMsg, const char* sKey, int64_t iDefault);
int xsXtpParamBool(const void* pMsg, const char* sKey, int bDefault);
char* xsXtpBodyDup(const void* pMsg, const char* sDefault);
xvalue xsXtpParamsValue(const void* pMsg);
int xsXtpReplyText(void* pStream, const void* pReqMsg, int iStatus, const char* sCmd, unsigned iParamCount, const char** arrParam, const char** arrValue, const char* sText);
int xsXtpReplyJson(void* pStream, const void* pReqMsg, int iStatus, const char* sCmd, unsigned iParamCount, const char** arrParam, const char** arrValue, const char* sJson);
int xsXtpReplyOKText(void* pStream, const void* pReqMsg, const char* sCmd, const char* sText);
int xsXtpReplyErrorText(void* pStream, const void* pReqMsg, int iStatus, const char* sCmd, const char* sText);
int xsXtpReplyOKJson(void* pStream, const void* pReqMsg, const char* sCmd, const char* sJson);
int xsXtpReplyErrorJson(void* pStream, const void* pReqMsg, int iStatus, const char* sCmd, const char* sJson);
int xsXtpReplyMissingParam(void* pStream, const void* pReqMsg, const char* sParam);
int xsXtpReplyUnsupportedCmd(void* pStream, const void* pReqMsg, const char* sCmd);
void* xsXtpClientOpen(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iTimeoutMs);
void xsXtpClientClose(void* pClient);
void* xsXtpClientDo(void* pClient, uint64_t iMsgID, const char* sCmd, unsigned iParamCount, const char** arrParam, const char** arrValue, const void* pBody, size_t iBodySize, unsigned iTimeoutMs);
void* xsXtpClientDoText(void* pClient, uint64_t iMsgID, const char* sCmd, unsigned iParamCount, const char** arrParam, const char** arrValue, const char* sText, unsigned iTimeoutMs);
void* xsXtpClientDoSimple(void* pClient, uint64_t iMsgID, const char* sCmd, unsigned iTimeoutMs);
void* xsXtpClientDoJson(void* pClient, uint64_t iMsgID, const char* sCmd, unsigned iParamCount, const char** arrParam, const char** arrValue, const char* sJson, unsigned iTimeoutMs);
void* xsXtpClientCall(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, const char* sCmd, unsigned iParamCount, const char** arrParam, const char** arrValue, const void* pBody, size_t iBodySize, unsigned iTimeoutMs);
void* xsXtpClientCallText(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, const char* sCmd, unsigned iParamCount, const char** arrParam, const char** arrValue, const char* sText, unsigned iTimeoutMs);
void* xsXtpClientCallSimple(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, const char* sCmd, unsigned iTimeoutMs);
void* xsXtpClientCallJson(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, const char* sCmd, unsigned iParamCount, const char** arrParam, const char** arrValue, const char* sJson, unsigned iTimeoutMs);
char* xsXtpClientCallSimpleBody(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, const char* sCmd, unsigned iTimeoutMs, const char* sDefault);
char* xsXtpClientCallTextBody(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, const char* sCmd, unsigned iParamCount, const char** arrParam, const char** arrValue, const char* sText, unsigned iTimeoutMs, const char* sDefault);
char* xsXtpClientCallJsonBody(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, const char* sCmd, unsigned iParamCount, const char** arrParam, const char** arrValue, const char* sJson, unsigned iTimeoutMs, const char* sDefault);
char* xsXtpClientCallSimpleSummary(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, const char* sCmd, unsigned iTimeoutMs);
char* xsXtpClientCallSimpleSummaryJson(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, const char* sCmd, unsigned iTimeoutMs);
char* xsXtpClientCallSimpleResult(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, const char* sCmd, unsigned iTimeoutMs, const char* sDefault);
char* xsXtpClientCallSimpleError(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, const char* sCmd, unsigned iTimeoutMs, const char* sDefault);
char* xsXtpClientCallSimpleMeta(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, const char* sCmd, unsigned iTimeoutMs);
char* xsXtpClientCallSimpleMetaJson(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, const char* sCmd, unsigned iTimeoutMs);
char* xsXtpClientCallSimpleResultJson(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, const char* sCmd, unsigned iTimeoutMs);
char* xsXtpClientCallSimpleErrorJson(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, const char* sCmd, unsigned iTimeoutMs, const char* sDefault);
int xsXtpClientCallSimpleStatus(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, const char* sCmd, unsigned iTimeoutMs, int iDefault);
char* xsXtpClientCallSimpleCmd(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, const char* sCmd, unsigned iTimeoutMs, const char* sDefault);
void xsXtpMessageFree(void* pMsg);
int xsXtpClientLastErrorCode(void);
const char* xsXtpClientLastError(void);
const char* xsXtpParamKeyAt(const void* pMsg, unsigned iIndex, unsigned* piLen);
const char* xsXtpParamValueAt(const void* pMsg, unsigned iIndex, unsigned* piLen);
int xsXtpFindParamView(const void* pMsg, const char* sKey, const char** ppVal, unsigned* piLen);

xvalue xsXtpValue(const void* pMsg);

xvalue xsXtpClientCallSimpleValue(
	const char* sHost,
	unsigned iPort,
	unsigned iRecvLimit,
	unsigned iConnectTimeoutMs,
	uint64_t iMsgID,
	const char* sCmd,
	unsigned iTimeoutMs
);

xvalue xsXtpClientCallSimpleParamsValue(
	const char* sHost,
	unsigned iPort,
	unsigned iRecvLimit,
	unsigned iConnectTimeoutMs,
	uint64_t iMsgID,
	const char* sCmd,
	unsigned iTimeoutMs
);

void* xsXtpRequestCreate(const char* sCmd);
void xsXtpRequestFree(void* pReq);
int xsXtpRequestSetCmd(void* pReq, const char* sCmd);
int xsXtpRequestSetParamsValue(void* pReq, xvalue objParams);
int xsXtpRequestSetParamText(void* pReq, const char* sKey, const char* sValue);
int xsXtpRequestSetParamInt(void* pReq, const char* sKey, int64_t iValue);
int xsXtpRequestSetParamBool(void* pReq, const char* sKey, int bValue);
int xsXtpRequestSetBodyText(void* pReq, const char* sText);
int xsXtpRequestSetBodyJson(void* pReq, const char* sJson);
int xsXtpRequestSetBodyValue(void* pReq, xvalue objBody);
void* xsXtpClientDoRequest(void* pClient, uint64_t iMsgID, void* pReq, unsigned iTimeoutMs);
void* xsXtpClientCallRequest(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, void* pReq, unsigned iTimeoutMs);
xvalue xsXtpClientCallRequestValue(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, void* pReq, unsigned iTimeoutMs);
xvalue xsXtpClientCallRequestParamsValue(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, void* pReq, unsigned iTimeoutMs);
xvalue xsXtpClientCallRequestBodyValue(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, void* pReq, unsigned iTimeoutMs);
char* xsXtpClientCallRequestBody(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, void* pReq, unsigned iTimeoutMs, const char* sDefault);
char* xsXtpClientCallRequestResult(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, void* pReq, unsigned iTimeoutMs, const char* sDefault);
char* xsXtpClientCallRequestError(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, void* pReq, unsigned iTimeoutMs, const char* sDefault);
char* xsXtpClientCallRequestMeta(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, void* pReq, unsigned iTimeoutMs);
char* xsXtpClientCallRequestMetaJson(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, void* pReq, unsigned iTimeoutMs);
char* xsXtpClientCallRequestResultJson(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, void* pReq, unsigned iTimeoutMs);
char* xsXtpClientCallRequestErrorJson(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, void* pReq, unsigned iTimeoutMs, const char* sDefault);
int xsXtpClientCallRequestOK(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, void* pReq, unsigned iTimeoutMs);
int xsXtpClientCallRequestStatus(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, void* pReq, unsigned iTimeoutMs, int iDefault);
char* xsXtpClientCallRequestCmd(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, void* pReq, unsigned iTimeoutMs, const char* sDefault);
char* xsXtpClientCallRequestSummary(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, void* pReq, unsigned iTimeoutMs);
char* xsXtpClientCallRequestSummaryJson(const char* sHost, unsigned iPort, unsigned iRecvLimit, unsigned iConnectTimeoutMs, uint64_t iMsgID, void* pReq, unsigned iTimeoutMs);

int xsXtpBuildParamArrays(xvalue objParams, const char*** ppParam, const char*** ppValue, unsigned* piCount);
void xsXtpFreeParamArrays(char** arrParam, char** arrValue, unsigned iCount);

void* xsXtpClientCallTableText(
	const char* sHost,
	unsigned iPort,
	unsigned iRecvLimit,
	unsigned iConnectTimeoutMs,
	uint64_t iMsgID,
	const char* sCmd,
	xvalue objParams,
	const char* sText,
	unsigned iTimeoutMs
);

void* xsXtpClientCallTableJson(
	const char* sHost,
	unsigned iPort,
	unsigned iRecvLimit,
	unsigned iConnectTimeoutMs,
	uint64_t iMsgID,
	const char* sCmd,
	xvalue objParams,
	const char* sJson,
	unsigned iTimeoutMs
);

void* xsXtpClientCallTableValue(
	const char* sHost,
	unsigned iPort,
	unsigned iRecvLimit,
	unsigned iConnectTimeoutMs,
	uint64_t iMsgID,
	const char* sCmd,
	xvalue objParams,
	xvalue objBody,
	unsigned iTimeoutMs
);

xvalue xsXtpBodyValue(const void* pMsg);

xvalue xsXtpErrorValue(const void* pMsg);

xvalue xsXtpClientCallSimpleBodyValue(
	const char* sHost,
	unsigned iPort,
	unsigned iRecvLimit,
	unsigned iConnectTimeoutMs,
	uint64_t iMsgID,
	const char* sCmd,
	unsigned iTimeoutMs
);

int xsDgramSendTo(void* pSock, const char* sIP, unsigned short iPort, const void* pData, size_t iLen);
int xsDgramReply(void* pSock, const void* pFromAddr, const void* pData, size_t iLen);
const char* xsAddrText(const void* pAddr);
int xsReloadCurrentHost(XS_ServerObject objServer, XS_HostObject objHost, int bForce);
int xsReloadHostByName(XS_ServerObject objServer, const char* sHostName, int bForce);



// ==================== TCC 状态机管理 ====================

// 动态创建 TCC 状态机（与 xs 主程序一样的配置）
// sWorkPath: 工作目录（会添加到 include 和 library 路径），可为 NULL
// 返回创建好的 TCCState，失败返回 NULL
TCCState* xsCreateTCC(const char* sWorkPath);

// 销毁 TCC 状态机
void xsDestroyTCC(TCCState* s);

// 将函数映射到 TCC 执行环境
void ImportAll(TCCState* s);



// ==================== 热加载函数 ====================

// 热加载指定 Host 的脚本
// 参数:
//   objServer - 服务器对象
//   objHost   - Host 对象
// 返回: 0=成功, 负数=失败
//   -1=非 C 语言, -2=文件不存在, -3=TCC 创建失败, -4=编译失败, -5=重定位失败
//   -100=参数无效
int xsReloadHost(XS_ServerObject objServer, XS_HostObject objHost);

// 通过域名热加载 Host 的脚本
// 参数:
//   objServer - 服务器对象
//   sDomain   - 域名（Host 配置中的 host 字段）
// 返回: 0=成功, 负数=失败
//   -101=域名未找到
int xsReloadHostByDomain(XS_ServerObject objServer, const char* sDomain);

// 热加载 DefaultHost 的脚本
// 参数:
//   objServer - 服务器对象
// 返回: 0=成功, 负数=失败
//   -102=DefaultHost 未启用
int xsReloadDefaultHost(XS_ServerObject objServer);

// 热加载整个 Server 的所有 Host
// 参数:
//   objServer - 服务器对象
// 返回: 成功加载的 Host 数量，负数=失败
int xsReloadServer(XS_ServerObject objServer);
int xsReloadAllServer(int bForce);



// 全局事件循环
void* g_pXsLoop = NULL;

// 全局数据 - 服务器列表
xarray g_arrXsServerList = NULL;

// 使用 extern 可能导致数据指针出现变化，原因不明，先用这样的方式传递全局数据
static inline void XS_SetGlobalDate(int idx, void* ptr)
{
	if ( idx == 1 ) {
		g_pXsLoop = ptr;
	} else if ( idx == 2 ) {
		g_arrXsServerList = (xarray)ptr;
	} else if ( idx == 3 ) {
		xCore = (xrtGlobalData*)ptr;
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
