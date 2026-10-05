/* Member authentication uses the common identity service. Business APIs below
 * retain their routing/permission contracts. No v2/legacy authentication fork. */
#include "../src/api/v1/identity.c"
#define XA_IDENTITY_ROUTE(name) void name(XS_ServerObject s,XS_HostObject h,XS_RequestObject req,XS_ResponseObject resp,xvalue* session) { XA_IdentityHandler(s,h,req,resp,session); }
XA_IDENTITY_ROUTE(API_Login)
XA_IDENTITY_ROUTE(API_Register)
XA_IDENTITY_ROUTE(API_Logout)
XA_IDENTITY_ROUTE(API_Profile)
XA_IDENTITY_ROUTE(API_Password)
#undef XA_IDENTITY_ROUTE

// ==================== 余额查询接口 ====================

// GET /api/v1/balance - 获取当前用户余额
void API_Balance(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( !(xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	
	int64 iMemberId = ValueInt(objSession, "id");
	
	// 从数据库获取最新余额
	sqlite3_bind_int64(stmt_member_get, 1, iMemberId);
	if ( sqlite3_step(stmt_member_get) == SQLITE_ROW ) {
		int64 iBalance = sqlite3_column_int64(stmt_member_get, 4);
		xvalue* data=ValueObject();bool ok=Member_SetBalance(data,iMemberId,iBalance);
		XA_Reply(objResp,ok?200:503,ok?"success":"Balance provider unavailable",data,NULL);xrtValueRelease(data);
	} else {
		xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, "{\"code\":404,\"msg\":\"用户不存在\"}", 0);
	}
	sqlite3_reset(stmt_member_get);
}



// GET /api/v1/balance/log - 获取余额变动日志
void API_BalanceLog(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	if ( !(xsReqMethodID(objReq) == XHTTP_METHOD_GET) ) {
		xsHttpReplyAuto(objResp, 405, HTTP_CT_JSON, "{\"code\":405,\"msg\":\"Method Not Allowed\"}", 0);
		return;
	}
	
	int64 iMemberId = ValueInt(objSession, "id");
	
	// 解析分页参数
	int64 iPage = 1;
	int64 iLimit = 20;
	char sParam[64];
	if ( xsReqQueryValue(objReq, "page", sParam, sizeof(sParam)) > 0 ) {
		iPage = atoll(sParam);
	}
	if ( xsReqQueryValue(objReq, "limit", sParam, sizeof(sParam)) > 0 ) {
		iLimit = atoll(sParam);
	}
	if ( iPage < 1 ) iPage = 1;
	if ( iLimit < 1 ) iLimit = 20;
	if ( iLimit > 100 ) iLimit = 100;
	
	int64 iOffset = (iPage - 1) * iLimit;
	char provider[65];int managed=Member_BalanceAuthority(provider);
	if(managed){
		xvalue* rows=managed>0?Member_ServiceTransactions(iMemberId,iOffset,(int)iLimit):NULL;
		XA_Reply(objResp,rows?200:503,rows?"success":"Balance provider unavailable",rows,NULL);xrtValueRelease(rows);return;
	}
	
	// 构建返回数据
	xvalue* arrRet = ValueArray();
	
	sqlite3_bind_int64(stmt_mbalance_all, 1, iMemberId);
	sqlite3_bind_int64(stmt_mbalance_all, 2, iLimit);
	sqlite3_bind_int64(stmt_mbalance_all, 3, iOffset);
	
	while ( sqlite3_step(stmt_mbalance_all) == SQLITE_ROW ) {
		xvalue* tblRow = ValueObject();
		ValueSetInt(tblRow, "id", sqlite3_column_int64(stmt_mbalance_all, 0));
		ValueSetInt(tblRow, "type", sqlite3_column_int(stmt_mbalance_all, 2));
		ValueSetInt(tblRow, "amount", sqlite3_column_int64(stmt_mbalance_all, 3));
		ValueSetInt(tblRow, "balance", sqlite3_column_int64(stmt_mbalance_all, 4));
		ValueSetText(tblRow, "remark", (str)sqlite3_column_text(stmt_mbalance_all, 5));
		ValueSetText(tblRow, "operator", (str)sqlite3_column_text(stmt_mbalance_all, 6));
		ValueSetInt(tblRow, "createTime", sqlite3_column_int64(stmt_mbalance_all, 7));
		ValueArrayOwn(arrRet, tblRow);
	}
	sqlite3_reset(stmt_mbalance_all);
	
	// 返回 JSON 响应
	size_t iJSONSize = 0;
	str sJSON = xrtJsonStringify(arrRet, false, &iJSONSize);
	xsHttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"code\":0,\"msg\":\"success\",\"data\":%s}", sJSON);
	xrtFree(sJSON);
	xrtValueRelease(arrRet);
}

