#ifndef INLINE_XSMTP_H
#define INLINE_XSMTP_H

#define XSMTP_SECURE_AUTO      0
#define XSMTP_SECURE_NONE      1
#define XSMTP_SECURE_SSL       2
#define XSMTP_SECURE_STARTTLS  3

#define XSMTP_AUTH_AUTO        0
#define XSMTP_AUTH_NONE        1
#define XSMTP_AUTH_PLAIN       2
#define XSMTP_AUTH_LOGIN       3

#define XSMTP_CAP_AUTH_PLAIN   0x0001u
#define XSMTP_CAP_AUTH_LOGIN   0x0002u
#define XSMTP_CAP_STARTTLS     0x0004u

typedef struct {
	const char* sEmail;
	const char* sName;
} xsmtpaddr;

typedef struct {
	const char* sHost;
	uint16 iPort;
	uint32 iTimeoutMs;
	int iSecureMode;
	bool bAuth;
	int iAuthMode;
	bool bVerifyPeer;
	const char* sUser;
	const char* sPass;
	const char* sHeloName;
	xsmtpaddr tFrom;
	xsmtpaddr tReplyTo;
} xsmtpconfig;

typedef struct {
	xsmtpaddr tFrom;
	xsmtpaddr tReplyTo;
	const xsmtpaddr* arrTo;
	size_t iToCount;
	const xsmtpaddr* arrCc;
	size_t iCcCount;
	const xsmtpaddr* arrBcc;
	size_t iBccCount;
	const char* sSubject;
	const char* sTextBody;
	const char* sHtmlBody;
	const char* const* arrHeaderNames;
	const char* const* arrHeaderValues;
	size_t iHeaderCount;
} xsmtpmessage;

typedef struct {
	bool bSuccess;
	bool bUsedTLS;
	bool bUsedStartTLS;
	int iServerCode;
	int iAuthMode;
	uint32 iCapabilities;
	char sError[256];
	char sLastReply[1024];
} xsmtpresult;

typedef struct {
	uint32 iTimeoutMs;
	xnetengine* pEngine;
	const char* sDebugName;
} xsmtpasyncopts;

void xrtSmtpConfigInit(xsmtpconfig* pCfg);
void xrtSmtpMessageInit(xsmtpmessage* pMsg);
void xrtSmtpResultInit(xsmtpresult* pRet);
void xrtSmtpAsyncOptsInit(xsmtpasyncopts* pOpts);
void xrtSmtpResultFree(xsmtpresult* pRet);
bool xrtSmtpSendMail(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, xsmtpresult* pRet);
xfuture* xrtSmtpSendMailFuture(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, const xsmtpasyncopts* pOpts);
bool xrtSmtpSendMailCo(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, xsmtpresult* pOut);
bool xrtSmtpSendMailAsyncWait(const xsmtpconfig* pCfg, const xsmtpmessage* pMsg, const xsmtpasyncopts* pOpts, xsmtpresult* pOut);

#endif
