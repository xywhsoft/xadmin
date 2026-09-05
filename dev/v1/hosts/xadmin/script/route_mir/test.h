


// ����
void Request_MIR_Test(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	xsHttpReplyAuto(objResp, 200, HTTP_CT_TEXT, "OK", 2);
}


