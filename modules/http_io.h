/*
 * HTTP 请求与响应辅助函数
 *
 * xServer 把解析后的 xrt HTTP 对象直接交给应用。下面这些小函数只负责
 * 普通网站最常见的重复工作：读取完整 body、生成响应头和返回常用内容。
 * 它们不创建新的 Request/Response 对象，也不保存任何请求级全局状态。
 */



/* 根据请求使用的传输层发送数据。tcp 与 tls 在 XS_HttpReq 中二选一。 */
static bool ConnSend(XS_HttpReq* pReq, const void* pData, size_t iSize)
{
	size_t iWritten = 0;

	if ( pReq == NULL || pData == NULL ) {
		return false;
	}
	if ( pReq->tls != NULL ) {
		/* TLS 允许成功短写；只有整段被受理才算本次发送完成。 */
		return xrtTlsStreamSend(pReq->tls, pData, iSize, &iWritten) == XTLS_OK &&
		       iWritten == iSize;
	}
	return pReq->tcp != NULL &&
		xrtNetStreamSend(pReq->tcp, pData, iSize) == XNET_RESULT_OK;
}



/* 返回一段完整 HTTP 内容，并允许调用方附加少量响应头。
 *
 * Content-Length 与 Content-Type 在这里统一生成。HEAD 响应保留 GET 应有的
 * Content-Length，但不发送正文，路由处理函数无需再写一份 HEAD 版本。 */
static bool ReplyRawHeaders(
	XS_HttpReq* pReq,
	uint16 iStatus,
	const char* sContentType,
	const void* pBody,
	size_t iBodySize,
	const xhttpfield* arrExtra,
	size_t iExtraCount
)
{
	char arrHead[1024];
	char arrLength[32];
	xhttpfield arrField[12];
	size_t iFieldCount = 0;
	size_t iHeadSize = 0;
	size_t i;
	xstrview tReason;

	if ( pReq == NULL || pReq->head == NULL ||
	     (pBody == NULL && iBodySize != 0) || iExtraCount > 10 ) {
		return false;
	}

	snprintf(arrLength, sizeof(arrLength), "%llu",
		(unsigned long long)iBodySize);
	arrField[iFieldCount].Name = XRT_STR_LITERAL("Content-Length");
	arrField[iFieldCount].Value = xrtStrView(arrLength);
	iFieldCount++;
	if ( sContentType != NULL ) {
		arrField[iFieldCount].Name = XRT_STR_LITERAL("Content-Type");
		arrField[iFieldCount].Value = xrtStrView(sContentType);
		iFieldCount++;
	}
	for ( i = 0; i < iExtraCount; i++ ) {
		arrField[iFieldCount++] = arrExtra[i];
	}

	tReason = xrtHttpStatusText(iStatus);
	if ( !xrtHttp1ResponseWrite(XHTTP_VERSION_1_1, iStatus, tReason,
		arrField, iFieldCount, arrHead, sizeof(arrHead), &iHeadSize) ) {
		return false;
	}
	if ( !ConnSend(pReq, arrHead, iHeadSize) ) {
		return false;
	}
	if ( iBodySize != 0 && pReq->head->MethodCode != XHTTP_METHOD_HEAD ) {
		return ConnSend(pReq, pBody, iBodySize);
	}
	return true;
}



static bool ReplyRaw(
	XS_HttpReq* pReq,
	uint16 iStatus,
	const char* sContentType,
	const void* pBody,
	size_t iBodySize
)
{
	return ReplyRawHeaders(pReq, iStatus, sContentType,
		pBody, iBodySize, NULL, 0);
}



static bool ReplyText(XS_HttpReq* pReq, uint16 iStatus, const char* sText)
{
	if ( sText == NULL ) sText = "";
	return ReplyRaw(pReq, iStatus, "text/plain; charset=utf-8",
		sText, strlen(sText));
}



static bool ReplyHTML(XS_HttpReq* pReq, uint16 iStatus, const char* sHTML)
{
	if ( sHTML == NULL ) sHTML = "";
	return ReplyRaw(pReq, iStatus, "text/html; charset=utf-8",
		sHTML, strlen(sHTML));
}



/* ReplyJSON 只借用 pObject，调用方仍负责 xrtValueRelease。 */
static bool ReplyJSON(XS_HttpReq* pReq, uint16 iStatus, const xvalue* pObject)
{
	str sJSON;
	size_t iJSONSize = 0;
	bool bResult;

	if ( pObject == NULL ) {
		return false;
	}
	sJSON = xrtJsonStringify(pObject, false, &iJSONSize);
	if ( sJSON == NULL ) {
		return false;
	}
	bResult = ReplyRaw(pReq, iStatus, "application/json; charset=utf-8",
		sJSON, iJSONSize);
	xrtFree(sJSON);
	return bResult;
}



/* 常见的成功/失败结果。函数内部创建并释放 JSON 对象。 */
static bool ReplyResult(XS_HttpReq* pReq, bool bOK, const char* sMessage)
{
	xvalue* pObject = xrtValueObject();
	bool bResult;

	if ( pObject == NULL ) {
		return false;
	}
	xrtValueObjectSetNew(pObject, XRT_STR_LITERAL("result"), xrtValueBool(bOK));
	xrtValueObjectSetNew(pObject, XRT_STR_LITERAL("msg"),
		xrtValueString(xrtStrView((sMessage ? sMessage : ""))));
	bResult = ReplyJSON(pReq, 200, pObject);
	xrtValueRelease(pObject);
	return bResult;
}



/* 把 body 解码器本次产出的数据追加到结果缓冲。
 * xrtHttp1BodyRead 同时支持 Content-Length 和 chunked framing。 */
static bool ReqBodyFeed(
	XS_HttpReq* pReq,
	const unsigned char* pData,
	size_t iSize,
	char** psOutput,
	size_t* pOutputSize,
	size_t* pConsumed
)
{
	xbytesview tInput;
	xbytesview tData;
	xhttp1errorinfo tError;
	xhttp1bodystatus eBody;

	tInput.Data = (unsigned char*)pData;
	tInput.Size = iSize;
	eBody = xrtHttp1BodyRead(pReq->body, tInput, false,
		pConsumed, &tData, &tError);
	if ( tData.Size != 0 ) {
		char* sNew = (char*)xrtRealloc(*psOutput,
			*pOutputSize + tData.Size + 1);

		if ( sNew == NULL ) {
			return false;
		}
		*psOutput = sNew;
		memcpy(*psOutput + *pOutputSize, tData.Data, tData.Size);
		*pOutputSize += tData.Size;
	}
	return eBody != XHTTP1_BODY_ERROR && eBody != XHTTP1_BODY_FIELDS;
}



/* 读取完整请求正文。
 *
 * 返回值是 NUL 结尾的独立字符串，调用方使用 xrtFree 释放；正文的真实
 * 字节数通过 pSize 返回，结尾 NUL 不计入长度。无正文或解码失败返回 NULL。 */
static char* ReqBodyText(XS_HttpReq* pReq, size_t* pSize)
{
	const xnetbuf* pBuffer;
	size_t iAvailable;
	unsigned char arrChunk[4096];
	char* sOutput = NULL;
	size_t iUsed = 0;
	size_t iOutputSize = 0;

	if ( pSize != NULL ) *pSize = 0;
	if ( pReq == NULL || pReq->body == NULL ) {
		return NULL;
	}
	pBuffer = pReq->tls != NULL ? xrtTlsStreamBuffer(pReq->tls)
	                                : xrtNetStreamBuffer(pReq->tcp);
	iAvailable = pBuffer != NULL ? xrtNetBufSize(pBuffer) : 0;
	if ( iAvailable == 0 ) {
		return NULL;
	}

	while ( iUsed < iAvailable ) {
		size_t iRead = iAvailable - iUsed > sizeof(arrChunk)
			? sizeof(arrChunk) : iAvailable - iUsed;
		size_t iConsumed = 0;

		if ( xrtNetBufPeek(pBuffer, iUsed, arrChunk, iRead) != iRead ) {
			xrtFree(sOutput);
			return NULL;
		}
		if ( !ReqBodyFeed(pReq, arrChunk, iRead, &sOutput,
			&iOutputSize, &iConsumed) ) {
			xrtFree(sOutput);
			return NULL;
		}
		iUsed += iConsumed;
		if ( xrtHttp1BodyDone(pReq->body) ) break;
		if ( iConsumed == 0 ) {
			xrtFree(sOutput);
			return NULL;
		}
	}

	/* 定长正文取满时，解码器可能先发布 DATA，再在空输入上发布 DONE。 */
	{
		size_t iConsumed = 0;

		if ( !ReqBodyFeed(pReq, NULL, 0, &sOutput,
			&iOutputSize, &iConsumed) ) {
			xrtFree(sOutput);
			return NULL;
		}
	}
	if ( !xrtHttp1BodyDone(pReq->body) || iOutputSize == 0 ) {
		xrtFree(sOutput);
		return NULL;
	}
	sOutput[iOutputSize] = '\0';
	if ( pSize != NULL ) *pSize = iOutputSize;
	return sOutput;
}



/* 读取并解析 JSON 正文。返回的 xvalue 由调用方释放。
 * pMissing 用于区分“没有正文”和“正文不是合法 JSON”。 */
static xvalue* ReqBodyJSON(XS_HttpReq* pReq, bool* pMissing)
{
	size_t iSize = 0;
	char* sBody = ReqBodyText(pReq, &iSize);
	xvalue* pBody;

	if ( sBody == NULL ) {
		if ( pMissing != NULL ) *pMissing = true;
		return NULL;
	}
	if ( pMissing != NULL ) *pMissing = false;
	pBody = xrtJsonParse(xrtStrViewN(sBody, iSize));
	xrtFree(sBody);
	return pBody;
}



/* 取得 JSON 对象中的字符串字段，并复制成 NUL 结尾字符串。
 * 空字段和非字符串字段返回 NULL；返回值由调用方使用 xrtFree 释放。 */
static char* ObjTextDup(xvalue* pObject, const char* sKey)
{
	xstrview tValue = {0};

	if ( pObject == NULL || sKey == NULL ||
	     !xrtValueGetString(xrtValueObjectGet(pObject, xrtStrView(sKey)), &tValue) ||
	     tValue.Size == 0 ) {
		return NULL;
	}
	return xrtStrDupN(tValue.Data, tValue.Size);
}
