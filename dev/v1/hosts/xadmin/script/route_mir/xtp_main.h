

#include <xsbase.h>
#include <string.h>
#include <stdint.h>



void ServiceInit(XS_ServerObject objServer, XS_HostObject objHost)
{
	(void)objServer;
	(void)objHost;
}



void ServiceUnit(XS_ServerObject objServer, XS_HostObject objHost)
{
	(void)objServer;
	(void)objHost;
}



void EventOpenProc(XS_ServerObject objServer, void* pStream)
{
	(void)objServer;

	xsDataRemoveByQuery("mir", "m2stream", 1);
	xsDataRegisterEx(xvoCreateInt((int64_t)pStream), "mir", "m2stream", 0);
}



void EventCloseProc(XS_ServerObject objServer, void* pStream, int iReason)
{
	(void)objServer;
	(void)pStream;
	(void)iReason;

	xsDataRemoveByQuery("mir", "m2stream", 1);
}



bool EventXtpProc(XS_ServerObject objServer, void* pStream, void* pMsg)
{
	(void)objServer;

	if ( pMsg == NULL ) {
		return FALSE;
	}

	if ( xsXtpCmdIs(pMsg, "ping") ) {
		xsXtpReplyOKText(pStream, pMsg, "pong", "pong");
		return TRUE;
	}

	if ( xsXtpNeedReply(pMsg) ) {
		xsXtpReplyUnsupportedCmd(pStream, pMsg, xsXtpCmd(pMsg));
	}

	return TRUE;
}
