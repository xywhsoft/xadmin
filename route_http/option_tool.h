/* v1 配置工具路由。本批仅接入后台入口候选地址生成（global.json 中 cp_url
 * 的「自动生成」按钮后端）；Option_GenerateAdminEntryPath 已随配置模块迁入。
 * test-smtp 依赖 SMTP 发送能力，与邮件批次一起接入，其 uris 权限行保留。 */
static void OptionToolReplyJSON(XS_ResponseObject objResp, xvalue* tblRet)
{
	size_t iRetSize = 0;
	char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
}

void Request_Option_AdminEntryGenerate(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	xvalue* tblRet;
	xvalue* tblData;
	str sPath;

	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !(xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	/* 只返回候选地址，不落盘；保存仍走配置文件写入。 */
	sPath = Option_GenerateAdminEntryPath();
	tblRet = ValueObject();
	tblData = ValueObject();
	ValueSetBool(tblRet, "result", true);
	ValueSetText(tblRet, "message", "ok");
	ValueSetText(tblData, "path", sPath ? sPath : (str)"");
	ValueSetOwn(tblRet, "data", tblData);
	OptionToolReplyJSON(objResp, tblRet);
	if ( sPath ) {
		xrtFree(sPath);
	}
	xrtValueRelease(tblRet);
}
