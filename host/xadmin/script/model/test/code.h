
// ============================================
// 模型: 测试 (test)
// 自动生成代码 - 请勿手动修改
// 生成时间: 2026-01-11 21:54:33
// TCC独立状态机编译
// ============================================

// 引入 xserver 基础�?
#include <xs_vnext_full.h>

// 全局变量（通过 Model_SetGlobalData 传入�?
sqlite3* G_DB;

// 模型管理器特有函数声�?
// 注意：Model_AddRoute 实际返回 RouteInfo*，但模型代码不需要使用返回�?
extern void* Model_AddRoute(str, void*, bool, bool, int, int);
extern void Model_RemoveRoute(str);

// HTTP helper declarations
bool HttpMethodIs(XS_RequestObject objReq, const char* sMethod);
int HttpGetQueryVar(XS_RequestObject objReq, const char* sName, char* sOut, size_t iOutCap);
int http_reply(XS_ResponseObject objResp, int iCode, str sHead, const void* pBody, size_t iLen);
int HttpReplyFormat(XS_ResponseObject objResp, int iCode, str sHead, str sFormat, ...);

// HTTP 响应常量
#define HTTP_CT_JSON "Content-Type: application/json\r\n"

// 接收主系统传递的全局数据
void Model_SetGlobalData(int idx, void* ptr)
{
	if ( idx == 1 ) {
		G_DB = (sqlite3*)ptr;
	}
}

// ==================== 模型代码 ====================

// 预编译语�?
sqlite3_stmt* stmt_test_all;
sqlite3_stmt* stmt_test_get;
sqlite3_stmt* stmt_test_add;
sqlite3_stmt* stmt_test_put;
sqlite3_stmt* stmt_test_del;
sqlite3_stmt* stmt_test_count;

/* ACCESS_CONTROL DISABLED

// 访问控制预编译语�?
sqlite3_stmt* stmt_test_access;
sqlite3_stmt* stmt_test_member_level;
*/

/* REPLY DISABLED

// 评论预编译语�?
sqlite3_stmt* stmt_test_reply_list;
sqlite3_stmt* stmt_test_reply_add;
sqlite3_stmt* stmt_test_reply_del;
sqlite3_stmt* stmt_test_reply_count;
sqlite3_stmt* stmt_test_reply_get;
*/


// 草稿预编译语�?
sqlite3_stmt* stmt_test_draft_all;
sqlite3_stmt* stmt_test_draft_get;
sqlite3_stmt* stmt_test_draft_add;
sqlite3_stmt* stmt_test_draft_put;
sqlite3_stmt* stmt_test_draft_del;
sqlite3_stmt* stmt_test_draft_count;
sqlite3_stmt* stmt_test_draft_publish;
sqlite3_stmt* stmt_test_draft_clear;


// 初始化预编译语句
void Model_test_InitStmt()
{
	sqlite3* db = G_DB;
	
	// 分页获取列表
	sqlite3_prepare_v3(db,
		"SELECT id, id, authorType, authorId, authorName, createTime, updateTime FROM model_test_test "
		"WHERE isDelete = 0 ORDER BY id DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt_test_all, NULL);
	
	// 根据ID获取单条记录
	sqlite3_prepare_v3(db,
		"SELECT id, id, authorType, authorId, authorName, createTime, updateTime FROM model_test_test WHERE id = ? AND isDelete = 0",
		-1, 0, &stmt_test_get, NULL);
	
	// 添加记录
	sqlite3_prepare_v3(db,
		"INSERT INTO model_test_test (id, authorType, authorId, authorName, createTime, updateTime, isDelete) VALUES (, ?, ?, ?, ?, ?, 0)",
		-1, 0, &stmt_test_add, NULL);
	
	// 更新记录
	sqlite3_prepare_v3(db,
		"UPDATE model_test_test SET id = id, authorType = ?, authorId = ?, authorName = ?, updateTime = ? WHERE id = ?",
		-1, 0, &stmt_test_put, NULL);
	
	// 删除记录（软删除�?
	sqlite3_prepare_v3(db,
		"UPDATE model_test_test SET isDelete = 1, updateTime = ? WHERE id = ?",
		-1, 0, &stmt_test_del, NULL);
	
	// 统计总数
	sqlite3_prepare_v3(db,
		"SELECT COUNT(*) FROM model_test_test WHERE isDelete = 0",
		-1, 0, &stmt_test_count, NULL);
	
/* ACCESS_CONTROL DISABLED

	// 获取内容的访问控制信�?
	sqlite3_prepare_v3(db,
		"SELECT accessLevel, accessPrice, accessPreview FROM model_test_test WHERE id = ? AND isDelete = 0",
		-1, 0, &stmt_test_access, NULL);
	
	// 获取会员的权限级�?
	sqlite3_prepare_v3(db,
		"SELECT g.authLevel FROM member m JOIN memberGroup g ON m.groupId = g.id WHERE m.id = ? AND m.isDelete = 0 AND m.status = 1",
		-1, 0, &stmt_test_member_level, NULL);
*/
	
/* REPLY DISABLED

	// 评论列表（根据内容ID�?
	sqlite3_prepare_v3(db,
		"SELECT id, userId, userType, content, quoteId, quoteText, status, createTime "
		"FROM reply WHERE modelName = 'test' AND contentId = ? AND isDelete = 0 "
		"ORDER BY id DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt_test_reply_list, NULL);
	
	// 获取单条评论
	sqlite3_prepare_v3(db,
		"SELECT id, userId, userType, content, quoteId, quoteText, status, createTime "
		"FROM reply WHERE id = ? AND isDelete = 0",
		-1, 0, &stmt_test_reply_get, NULL);
	
	// 添加评论
	sqlite3_prepare_v3(db,
		"INSERT INTO reply (modelName, contentId, userId, userType, content, quoteId, quoteText, status, createTime, updateTime, isDelete) "
		"VALUES ('test', ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)",
		-1, 0, &stmt_test_reply_add, NULL);
	
	// 删除评论（软删除�?
	sqlite3_prepare_v3(db,
		"UPDATE reply SET isDelete = 1, updateTime = ? WHERE id = ?",
		-1, 0, &stmt_test_reply_del, NULL);
	
	// 评论总数
	sqlite3_prepare_v3(db,
		"SELECT COUNT(*) FROM reply WHERE modelName = 'test' AND contentId = ? AND isDelete = 0",
		-1, 0, &stmt_test_reply_count, NULL);
*/


	// 草稿列表
	sqlite3_prepare_v3(db,
		"SELECT id, id, authorType, authorId, authorName, createTime, updateTime FROM model_test_test_draft "
		"ORDER BY id DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt_test_draft_all, NULL);
	
	// 获取单条草稿
	sqlite3_prepare_v3(db,
		"SELECT id, id, authorType, authorId, authorName, createTime, updateTime FROM model_test_test_draft WHERE id = ?",
		-1, 0, &stmt_test_draft_get, NULL);
	
	// 添加草稿
	sqlite3_prepare_v3(db,
		"INSERT INTO model_test_test_draft (id, authorType, authorId, authorName, createTime, updateTime) VALUES (, ?, ?, ?, ?, ?)",
		-1, 0, &stmt_test_draft_add, NULL);
	
	// 更新草稿
	sqlite3_prepare_v3(db,
		"UPDATE model_test_test_draft SET id = id, authorType = ?, authorId = ?, authorName = ?, updateTime = ? WHERE id = ?",
		-1, 0, &stmt_test_draft_put, NULL);
	
	// 删除草稿（硬删除�?
	sqlite3_prepare_v3(db,
		"DELETE FROM model_test_test_draft WHERE id = ?",
		-1, 0, &stmt_test_draft_del, NULL);
	
	// 草稿总数
	sqlite3_prepare_v3(db,
		"SELECT COUNT(*) FROM model_test_test_draft",
		-1, 0, &stmt_test_draft_count, NULL);
	
	// 发布草稿（插入主表后删除草稿�? 只删除草稿记�?
	sqlite3_prepare_v3(db,
		"DELETE FROM model_test_test_draft WHERE id = ?",
		-1, 0, &stmt_test_draft_publish, NULL);
	
	// 清空草稿�?
	sqlite3_prepare_v3(db,
		"DELETE FROM model_test_test_draft",
		-1, 0, &stmt_test_draft_clear, NULL);

}

// 销毁预编译语句
void Model_test_FreeStmt()
{
	if ( stmt_test_all ) sqlite3_finalize(stmt_test_all);
	if ( stmt_test_get ) sqlite3_finalize(stmt_test_get);
	if ( stmt_test_add ) sqlite3_finalize(stmt_test_add);
	if ( stmt_test_put ) sqlite3_finalize(stmt_test_put);
	if ( stmt_test_del ) sqlite3_finalize(stmt_test_del);
	if ( stmt_test_count ) sqlite3_finalize(stmt_test_count);
/* ACCESS_CONTROL DISABLED

	if ( stmt_test_access ) sqlite3_finalize(stmt_test_access);
	if ( stmt_test_member_level ) sqlite3_finalize(stmt_test_member_level);
*/
/* REPLY DISABLED

	if ( stmt_test_reply_list ) sqlite3_finalize(stmt_test_reply_list);
	if ( stmt_test_reply_get ) sqlite3_finalize(stmt_test_reply_get);
	if ( stmt_test_reply_add ) sqlite3_finalize(stmt_test_reply_add);
	if ( stmt_test_reply_del ) sqlite3_finalize(stmt_test_reply_del);
	if ( stmt_test_reply_count ) sqlite3_finalize(stmt_test_reply_count);
*/

	if ( stmt_test_draft_all ) sqlite3_finalize(stmt_test_draft_all);
	if ( stmt_test_draft_get ) sqlite3_finalize(stmt_test_draft_get);
	if ( stmt_test_draft_add ) sqlite3_finalize(stmt_test_draft_add);
	if ( stmt_test_draft_put ) sqlite3_finalize(stmt_test_draft_put);
	if ( stmt_test_draft_del ) sqlite3_finalize(stmt_test_draft_del);
	if ( stmt_test_draft_count ) sqlite3_finalize(stmt_test_draft_count);
	if ( stmt_test_draft_publish ) sqlite3_finalize(stmt_test_draft_publish);
	if ( stmt_test_draft_clear ) sqlite3_finalize(stmt_test_draft_clear);

}




// ==================== 前台 API ====================

// 获取列表
void Api_test_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !HttpMethodIs(objReq, "GET") ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	char sParam[64];
	HttpGetQueryVar(objReq, "page", sParam, sizeof(sParam));
	int64 iPage = xrtStrToI64(sParam);
	if ( iPage <= 0 ) iPage = 1;
	HttpGetQueryVar(objReq, "limit", sParam, sizeof(sParam));
	int64 iLimit = xrtStrToI64(sParam);
	if ( iLimit <= 0 ) iLimit = 20;
	if ( iLimit > 100 ) iLimit = 100;
	int64 iOffset = (iPage - 1) * iLimit;
	
	// 获取总数
	int64 iCount = 0;
	if ( sqlite3_step(stmt_test_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_test_count, 0);
	}
	sqlite3_reset(stmt_test_count);
	
	// 获取数据
	xvalue arrData = xvoCreateArray();
	sqlite3_bind_int64(stmt_test_all, 1, iLimit);
	sqlite3_bind_int64(stmt_test_all, 2, iOffset);
	while ( sqlite3_step(stmt_test_all) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_test_all, iCol++));

		xtime iTime = sqlite3_column_int64(stmt_test_all, iCol++);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_test_all, iCol++);
		xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_reset(stmt_test_all);
	
	// 构建响应
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(objResp, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}

// 获取详情
void Api_test_Get(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sID[24];
	HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
/* ACCESS_CONTROL DISABLED

	// ========== 访问控制检�?==========
	int iAccessLevel = 0;
	double fAccessPrice = 0;
	str sAccessPreview = NULL;
	
	// 获取内容的访问控制信�?
	sqlite3_bind_int64(stmt_test_access, 1, iID);
	if ( sqlite3_step(stmt_test_access) == SQLITE_ROW ) {
		iAccessLevel = sqlite3_column_int(stmt_test_access, 0);
		fAccessPrice = sqlite3_column_double(stmt_test_access, 1);
		str sTmp = (str)sqlite3_column_text(stmt_test_access, 2);
		if ( sTmp && strlen(sTmp) > 0 ) {
			sAccessPreview = xrtCopyStr(sTmp, 0);
		}
	}
	sqlite3_reset(stmt_test_access);
	
	// 检查访问权�?
	bool bAccessDenied = FALSE;
	str sAccessDeniedMsg = NULL;
	int iRequiredLevel = 0;
	
	if ( iAccessLevel != 0 ) {
		// 获取当前用户信息
		int64 iMemberId = 0;
		int iMemberAuthLevel = 0;
		
		if ( objSession ) {
			iMemberId = xvoTableGetInt(objSession, "id", 2);
			if ( iMemberId > 0 ) {
				// 获取用户的会员组权限级别
				sqlite3_bind_int64(stmt_test_member_level, 1, iMemberId);
				if ( sqlite3_step(stmt_test_member_level) == SQLITE_ROW ) {
					iMemberAuthLevel = sqlite3_column_int(stmt_test_member_level, 0);
				}
				sqlite3_reset(stmt_test_member_level);
			}
		}
		
		if ( iAccessLevel > 0 ) {
			// 需要指定权限级�?
			if ( iMemberId <= 0 ) {
				bAccessDenied = TRUE;
				sAccessDeniedMsg = "请先登录";
				iRequiredLevel = iAccessLevel;
			} else if ( iMemberAuthLevel < iAccessLevel ) {
				bAccessDenied = TRUE;
				sAccessDeniedMsg = "权限不足，需要更高级别会�?;
				iRequiredLevel = iAccessLevel;
			}
		} else if ( iAccessLevel == -1 ) {
			// 付费内容 - 待实现购买记录检�?
			// TODO: 检查用户是否已购买
			bAccessDenied = TRUE;
			sAccessDeniedMsg = "付费内容，请购买后查�?;
		} else if ( iAccessLevel == -2 ) {
			// 仅作者可�?- 待实现作者字段检�?
			// TODO: 检查是否为内容作�?
			bAccessDenied = TRUE;
			sAccessDeniedMsg = "仅作者可�?;
		}
	}
	
	// 如果访问被拒绝，返回预览内容
	if ( bAccessDenied ) {
		xvalue tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetBool(tblRet, "accessDenied", 12, TRUE);
		xvoTableSetText(tblRet, "accessMessage", 13, sAccessDeniedMsg, 0, FALSE);
		xvoTableSetInt(tblRet, "requiredLevel", 13, iRequiredLevel);
		if ( fAccessPrice > 0 ) {
			xvoTableSetDouble(tblRet, "accessPrice", 11, fAccessPrice);
		}
		if ( sAccessPreview ) {
			xvoTableSetText(tblRet, "preview", 7, sAccessPreview, 0, FALSE);
			xrtFree(sAccessPreview);
		}
		
		size_t iSize = 0;
		str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
		http_reply(objResp, 200, HTTP_CT_JSON, sJson, iSize);
		xrtFree(sJson);
		xvoUnref(tblRet);
		return;
	}
	
	if ( sAccessPreview ) xrtFree(sAccessPreview);
	// ========== 访问控制检查结�?==========
*/
	
	xvalue tblData = NULL;
	sqlite3_bind_int64(stmt_test_get, 1, iID);
	if ( sqlite3_step(stmt_test_get) == SQLITE_ROW ) {
		tblData = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblData, "id", 2, sqlite3_column_int64(stmt_test_get, iCol++));

		xtime iTime = sqlite3_column_int64(stmt_test_get, iCol++);
		xvoTableSetText(tblData, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_test_get, iCol++);
		xvoTableSetText(tblData, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
	}
	sqlite3_reset(stmt_test_get);
	
	if ( !tblData ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Not found\"}", 0);
		return;
	}
	
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(objResp, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}




/* SUBMIT DISABLED

// ==================== 前台投稿 ====================

// 投稿内容
void Api_test_Submit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !HttpMethodIs(objReq, "POST") ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		if ( tblForm ) xvoUnref(tblForm);
		return;
	}
	
	// 获取当前用户信息
	int64 iMemberId = 0;
	str sMemberNickname = NULL;
	if ( objSession ) {
		iMemberId = xvoTableGetInt(objSession, "id", 2);
		sMemberNickname = xvoTableGetText(objSession, "nickname", 8);
	}
	
	// 检查是否允许游客投�?
	int iAllowGuest = 0;
	if ( (iMemberId <= 0) && !iAllowGuest ) {
		xvoUnref(tblForm);
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Please login first\"}", 0);
		return;
	}
	
	// 决定数据去向：游客始终进草稿，会员根据配置决�?
	int iNeedReview = 0;
	bool bToDraft = (iMemberId <= 0) || iNeedReview;
	
	// 作者信�?
	int iAuthorType = (iMemberId <= 0) ? 2 : 1;  // 2=游客, 1=前台会员
	int64 iAuthorId = iMemberId;
	str sAuthorName = xvoTableGetText(tblForm, "authorName", 10);
	if ( !sAuthorName || strlen(sAuthorName) == 0 ) {
		sAuthorName = sMemberNickname ? sMemberNickname : "匿名用户";
	}
	
	xtime now = xrtNow();
	int iIdx = 1;
	int64 newId = 0;
	
	if ( bToDraft ) {
		// 进入草稿�?

		sqlite3_bind_int64(stmt_test_draft_add, iIdx++, iAuthorType);
		sqlite3_bind_int64(stmt_test_draft_add, iIdx++, iAuthorId);
		sqlite3_bind_text(stmt_test_draft_add, iIdx++, sAuthorName, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_test_draft_add, iIdx++, now);
		sqlite3_bind_int64(stmt_test_draft_add, iIdx++, now);
		sqlite3_step(stmt_test_draft_add);
		newId = sqlite3_last_insert_rowid(G_DB);
		sqlite3_reset(stmt_test_draft_add);
	} else {
		// 直接发布

		sqlite3_bind_int64(stmt_test_add, iIdx++, iAuthorType);
		sqlite3_bind_int64(stmt_test_add, iIdx++, iAuthorId);
		sqlite3_bind_text(stmt_test_add, iIdx++, sAuthorName, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_test_add, iIdx++, now);
		sqlite3_bind_int64(stmt_test_add, iIdx++, now);
		sqlite3_step(stmt_test_add);
		newId = sqlite3_last_insert_rowid(G_DB);
		sqlite3_reset(stmt_test_add);
	}
	
	xvoUnref(tblForm);
	
	if ( bToDraft ) {
		HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Submitted to review\", \"data\": {\"id\": %lld, \"isDraft\": true}}", newId);
	} else {
		HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Published\", \"data\": {\"id\": %lld, \"isDraft\": false}}", newId);
	}
}
*/



/* REPLY DISABLED

// ==================== 评论 API ====================

// 获取评论列表
void Api_test_Reply_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sParam[64];
	HttpGetQueryVar(objReq, "contentId", sParam, sizeof(sParam));
	int64 iContentId = xrtStrToI64(sParam);
	if ( iContentId <= 0 ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing contentId\"}", 0);
		return;
	}
	
	HttpGetQueryVar(objReq, "page", sParam, sizeof(sParam));
	int64 iPage = xrtStrToI64(sParam);
	if ( iPage <= 0 ) iPage = 1;
	HttpGetQueryVar(objReq, "limit", sParam, sizeof(sParam));
	int64 iLimit = xrtStrToI64(sParam);
	if ( iLimit <= 0 ) iLimit = 20;
	if ( iLimit > 100 ) iLimit = 100;
	int64 iOffset = (iPage - 1) * iLimit;
	
	// 获取总数
	int64 iCount = 0;
	sqlite3_bind_int64(stmt_test_reply_count, 1, iContentId);
	if ( sqlite3_step(stmt_test_reply_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_test_reply_count, 0);
	}
	sqlite3_reset(stmt_test_reply_count);
	
	// 获取评论列表
	xvalue arrData = xvoCreateArray();
	sqlite3_bind_int64(stmt_test_reply_list, 1, iContentId);
	sqlite3_bind_int64(stmt_test_reply_list, 2, iLimit);
	sqlite3_bind_int64(stmt_test_reply_list, 3, iOffset);
	while ( sqlite3_step(stmt_test_reply_list) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_test_reply_list, iCol++));
		xvoTableSetInt(tblRow, "userId", 6, sqlite3_column_int64(stmt_test_reply_list, iCol++));
		xvoTableSetInt(tblRow, "userType", 8, sqlite3_column_int64(stmt_test_reply_list, iCol++));
		xvoTableSetText(tblRow, "content", 7, (str)sqlite3_column_text(stmt_test_reply_list, iCol++), 0, FALSE);
		xvoTableSetInt(tblRow, "quoteId", 7, sqlite3_column_int64(stmt_test_reply_list, iCol++));
		xvoTableSetText(tblRow, "quoteText", 9, (str)sqlite3_column_text(stmt_test_reply_list, iCol++), 0, FALSE);
		xvoTableSetInt(tblRow, "status", 6, sqlite3_column_int64(stmt_test_reply_list, iCol++));
		xtime iTime = sqlite3_column_int64(stmt_test_reply_list, iCol++);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_reset(stmt_test_reply_list);
	
	// 构建响应
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(objResp, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}

// 添加评论
void Api_test_Reply_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !HttpMethodIs(objReq, "POST") ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		if ( tblForm ) xvoUnref(tblForm);
		return;
	}
	
	int64 iContentId = xvoTableGetInt(tblForm, "contentId", 9);
	if ( iContentId <= 0 ) {
		xvoUnref(tblForm);
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing contentId\"}", 0);
		return;
	}
	
	str sContent = xvoTableGetText(tblForm, "content", 7);
	if ( !sContent || strlen(sContent) == 0 ) {
		xvoUnref(tblForm);
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Content is required\"}", 0);
		return;
	}
	
	// 获取引用信息
	int64 iQuoteId = xvoTableGetInt(tblForm, "quoteId", 7);
	str sQuoteText = "";
	bool bFreeQuote = FALSE;
	if ( iQuoteId > 0 ) {
		// 获取被引用评论的内容摘要
		sqlite3_bind_int64(stmt_test_reply_get, 1, iQuoteId);
		if ( sqlite3_step(stmt_test_reply_get) == SQLITE_ROW ) {
			str sOrigContent = (str)sqlite3_column_text(stmt_test_reply_get, 3);  // content 在第4�?
			if ( sOrigContent ) {
				int iMaxLen = {{REPLY_QUOTE_MAX_LEN}};
				int iLen = strlen(sOrigContent);
				if ( iLen > iMaxLen ) {
					sQuoteText = xrtCopyStr(sOrigContent, iMaxLen);
				} else {
					sQuoteText = xrtCopyStr(sOrigContent, iLen);
				}
				bFreeQuote = TRUE;
			}
		}
		sqlite3_reset(stmt_test_reply_get);
	}
	
	// 获取用户信息（从 session 中获取，这里简化处理）
	int64 iUserId = xvoTableGetInt(tblForm, "userId", 6);
	int64 iUserType = xvoTableGetInt(tblForm, "userType", 8);  // 0=会员, 1=后台用户
	
	// 状态：是否需要审�?
	int64 iStatus = {{REPLY_NEED_APPROVE}} ? 0 : 1;  // 0=待审�? 1=已发�?
	
	xtime now = xrtNow();
	int iIdx = 1;
	sqlite3_bind_int64(stmt_test_reply_add, iIdx++, iContentId);
	sqlite3_bind_int64(stmt_test_reply_add, iIdx++, iUserId);
	sqlite3_bind_int64(stmt_test_reply_add, iIdx++, iUserType);
	sqlite3_bind_text(stmt_test_reply_add, iIdx++, sContent, -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt_test_reply_add, iIdx++, iQuoteId);
	sqlite3_bind_text(stmt_test_reply_add, iIdx++, sQuoteText, -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt_test_reply_add, iIdx++, iStatus);
	sqlite3_bind_int64(stmt_test_reply_add, iIdx++, now);
	sqlite3_bind_int64(stmt_test_reply_add, iIdx++, now);
	sqlite3_step(stmt_test_reply_add);
	int64 newId = sqlite3_last_insert_rowid(G_DB);
	sqlite3_reset(stmt_test_reply_add);
	
	// 清理临时内存
	if ( bFreeQuote ) {
		xrtFree(sQuoteText);
	}
	xvoUnref(tblForm);
	
	HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"id\": %lld}}", newId);
}

// 删除评论（需要校验权限）
void Api_test_Reply_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sParam[24];
	HttpGetQueryVar(objReq, "id", sParam, sizeof(sParam));
	int64 iID = xrtStrToI64(sParam);
	if ( iID <= 0 ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	// TODO: 应该校验当前用户是否有权删除该评�?
	
	xtime now = xrtNow();
	sqlite3_bind_int64(stmt_test_reply_del, 1, now);
	sqlite3_bind_int64(stmt_test_reply_del, 2, iID);
	sqlite3_step(stmt_test_reply_del);
	sqlite3_reset(stmt_test_reply_del);
	
	http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}
*/




// ==================== 后台管理 API ====================

// 后台列表
void Admin_test_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !HttpMethodIs(objReq, "GET") ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	char sParam[64];
	HttpGetQueryVar(objReq, "page", sParam, sizeof(sParam));
	int64 iPage = xrtStrToI64(sParam);
	if ( iPage <= 0 ) iPage = 1;
	HttpGetQueryVar(objReq, "limit", sParam, sizeof(sParam));
	int64 iLimit = xrtStrToI64(sParam);
	if ( iLimit <= 0 ) iLimit = 20;
	int64 iOffset = (iPage - 1) * iLimit;
	
	// 获取总数
	int64 iCount = 0;
	if ( sqlite3_step(stmt_test_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_test_count, 0);
	}
	sqlite3_reset(stmt_test_count);
	
	// 获取数据
	xvalue arrData = xvoCreateArray();
	sqlite3_bind_int64(stmt_test_all, 1, iLimit);
	sqlite3_bind_int64(stmt_test_all, 2, iOffset);
	while ( sqlite3_step(stmt_test_all) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_test_all, iCol++));

		xvoTableSetInt(tblRow, "authorType", 10, sqlite3_column_int64(stmt_test_all, iCol++));
		xvoTableSetInt(tblRow, "authorId", 8, sqlite3_column_int64(stmt_test_all, iCol++));
		xvoTableSetText(tblRow, "authorName", 10, (str)sqlite3_column_text(stmt_test_all, iCol++), 0, FALSE);
		xtime iTime = sqlite3_column_int64(stmt_test_all, iCol++);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_test_all, iCol++);
		xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_reset(stmt_test_all);
	
	// layui table 格式响应
	xvalue tblRet = xvoCreateTable();
	xvoTableSetInt(tblRet, "code", 4, 0);
	xvoTableSetText(tblRet, "msg", 3, "", 0, FALSE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(objResp, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}

// 后台获取单条
void Admin_test_Get(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sID[24];
	HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xvalue tblData = NULL;
	sqlite3_bind_int64(stmt_test_get, 1, iID);
	if ( sqlite3_step(stmt_test_get) == SQLITE_ROW ) {
		tblData = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblData, "id", 2, sqlite3_column_int64(stmt_test_get, iCol++));

		xvoTableSetInt(tblData, "authorType", 10, sqlite3_column_int64(stmt_test_get, iCol++));
		xvoTableSetInt(tblData, "authorId", 8, sqlite3_column_int64(stmt_test_get, iCol++));
		xvoTableSetText(tblData, "authorName", 10, (str)sqlite3_column_text(stmt_test_get, iCol++), 0, FALSE);
		xtime iTime = sqlite3_column_int64(stmt_test_get, iCol++);
		xvoTableSetText(tblData, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_test_get, iCol++);
		xvoTableSetText(tblData, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
	}
	sqlite3_reset(stmt_test_get);
	
	if ( !tblData ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Not found\"}", 0);
		return;
	}
	
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(objResp, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}

// 后台添加
void Admin_test_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !HttpMethodIs(objReq, "POST") ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		if ( tblForm ) xvoUnref(tblForm);
		return;
	}
	
	xtime now = xrtNow();
	int iIdx = 1;

	// 作者字�?
	sqlite3_bind_int64(stmt_test_add, iIdx++, xvoTableGetInt(tblForm, "authorType", 10));
	sqlite3_bind_int64(stmt_test_add, iIdx++, xvoTableGetInt(tblForm, "authorId", 8));
	str sAuthorName = xvoTableGetText(tblForm, "authorName", 10);
	sqlite3_bind_text(stmt_test_add, iIdx++, sAuthorName ? sAuthorName : "", -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt_test_add, iIdx++, now);
	sqlite3_bind_int64(stmt_test_add, iIdx++, now);
	sqlite3_step(stmt_test_add);
	int64 newId = sqlite3_last_insert_rowid(G_DB);
	sqlite3_reset(stmt_test_add);
	xvoUnref(tblForm);
	
	HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"id\": %lld}}", newId);
}

// 后台更新
void Admin_test_Save(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !HttpMethodIs(objReq, "POST") ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		if ( tblForm ) xvoUnref(tblForm);
		return;
	}
	
	int64 iID = xvoTableGetInt(tblForm, "id", 2);
	if ( iID <= 0 ) {
		xvoUnref(tblForm);
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xtime now = xrtNow();
	int iIdx = 1;

	// 作者字�?
	sqlite3_bind_int64(stmt_test_put, iIdx++, xvoTableGetInt(tblForm, "authorType", 10));
	sqlite3_bind_int64(stmt_test_put, iIdx++, xvoTableGetInt(tblForm, "authorId", 8));
	str sAuthorName = xvoTableGetText(tblForm, "authorName", 10);
	sqlite3_bind_text(stmt_test_put, iIdx++, sAuthorName ? sAuthorName : "", -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt_test_put, iIdx++, now);
	sqlite3_bind_int64(stmt_test_put, iIdx++, iID);
	sqlite3_step(stmt_test_put);
	sqlite3_reset(stmt_test_put);
	xvoUnref(tblForm);
	
	http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}

// 后台删除
void Admin_test_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sID[24];
	HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xtime now = xrtNow();
	sqlite3_bind_int64(stmt_test_del, 1, now);
	sqlite3_bind_int64(stmt_test_del, 2, iID);
	sqlite3_step(stmt_test_del);
	sqlite3_reset(stmt_test_del);
	
	http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}





// ==================== 草稿管理 API ====================

// 草稿列表
void Admin_test_Draft_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !HttpMethodIs(objReq, "GET") ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	char sParam[64];
	HttpGetQueryVar(objReq, "page", sParam, sizeof(sParam));
	int64 iPage = xrtStrToI64(sParam);
	if ( iPage <= 0 ) iPage = 1;
	HttpGetQueryVar(objReq, "limit", sParam, sizeof(sParam));
	int64 iLimit = xrtStrToI64(sParam);
	if ( iLimit <= 0 ) iLimit = 20;
	int64 iOffset = (iPage - 1) * iLimit;
	
	// 获取总数
	int64 iCount = 0;
	if ( sqlite3_step(stmt_test_draft_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_test_draft_count, 0);
	}
	sqlite3_reset(stmt_test_draft_count);
	
	// 获取数据
	xvalue arrData = xvoCreateArray();
	sqlite3_bind_int64(stmt_test_draft_all, 1, iLimit);
	sqlite3_bind_int64(stmt_test_draft_all, 2, iOffset);
	while ( sqlite3_step(stmt_test_draft_all) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_test_draft_all, iCol++));

		xvoTableSetInt(tblRow, "authorType", 10, sqlite3_column_int64(stmt_test_draft_all, iCol++));
		xvoTableSetInt(tblRow, "authorId", 8, sqlite3_column_int64(stmt_test_draft_all, iCol++));
		xvoTableSetText(tblRow, "authorName", 10, (str)sqlite3_column_text(stmt_test_draft_all, iCol++), 0, FALSE);
		xtime iTime = sqlite3_column_int64(stmt_test_draft_all, iCol++);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_test_draft_all, iCol++);
		xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_reset(stmt_test_draft_all);
	
	// layui table 格式响应
	xvalue tblRet = xvoCreateTable();
	xvoTableSetInt(tblRet, "code", 4, 0);
	xvoTableSetText(tblRet, "msg", 3, "", 0, FALSE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(objResp, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}

// 获取单条草稿
void Admin_test_Draft_Get(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sID[24];
	HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xvalue tblData = NULL;
	sqlite3_bind_int64(stmt_test_draft_get, 1, iID);
	if ( sqlite3_step(stmt_test_draft_get) == SQLITE_ROW ) {
		tblData = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblData, "id", 2, sqlite3_column_int64(stmt_test_draft_get, iCol++));

		xvoTableSetInt(tblData, "authorType", 10, sqlite3_column_int64(stmt_test_draft_get, iCol++));
		xvoTableSetInt(tblData, "authorId", 8, sqlite3_column_int64(stmt_test_draft_get, iCol++));
		xvoTableSetText(tblData, "authorName", 10, (str)sqlite3_column_text(stmt_test_draft_get, iCol++), 0, FALSE);
		xtime iTime = sqlite3_column_int64(stmt_test_draft_get, iCol++);
		xvoTableSetText(tblData, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_test_draft_get, iCol++);
		xvoTableSetText(tblData, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
	}
	sqlite3_reset(stmt_test_draft_get);
	
	if ( !tblData ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Not found\"}", 0);
		return;
	}
	
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(objResp, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}

// 保存草稿
void Admin_test_Draft_Save(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !HttpMethodIs(objReq, "POST") ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		if ( tblForm ) xvoUnref(tblForm);
		return;
	}
	
	int64 iID = xvoTableGetInt(tblForm, "id", 2);
	xtime now = xrtNow();
	
	if ( iID > 0 ) {
		// 更新草稿
		int iIdx = 1;

		sqlite3_bind_int64(stmt_test_draft_put, iIdx++, xvoTableGetInt(tblForm, "authorType", 10));
		sqlite3_bind_int64(stmt_test_draft_put, iIdx++, xvoTableGetInt(tblForm, "authorId", 8));
		str sAuthorName = xvoTableGetText(tblForm, "authorName", 10);
		sqlite3_bind_text(stmt_test_draft_put, iIdx++, sAuthorName ? sAuthorName : "", -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_test_draft_put, iIdx++, now);
		sqlite3_bind_int64(stmt_test_draft_put, iIdx++, iID);
		sqlite3_step(stmt_test_draft_put);
		sqlite3_reset(stmt_test_draft_put);
	} else {
		// 添加草稿
		int iIdx = 1;

		sqlite3_bind_int64(stmt_test_draft_add, iIdx++, xvoTableGetInt(tblForm, "authorType", 10));
		sqlite3_bind_int64(stmt_test_draft_add, iIdx++, xvoTableGetInt(tblForm, "authorId", 8));
		str sAuthorName = xvoTableGetText(tblForm, "authorName", 10);
		sqlite3_bind_text(stmt_test_draft_add, iIdx++, sAuthorName ? sAuthorName : "", -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_test_draft_add, iIdx++, now);
		sqlite3_bind_int64(stmt_test_draft_add, iIdx++, now);
		sqlite3_step(stmt_test_draft_add);
		iID = sqlite3_last_insert_rowid(G_DB);
		sqlite3_reset(stmt_test_draft_add);
	}
	
	xvoUnref(tblForm);
	HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"id\": %lld}}", iID);
}

// 删除草稿
void Admin_test_Draft_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sID[24];
	HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	sqlite3_bind_int64(stmt_test_draft_del, 1, iID);
	sqlite3_step(stmt_test_draft_del);
	sqlite3_reset(stmt_test_draft_del);
	
	http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}

// 发布草稿
void Admin_test_Draft_Publish(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sID[24];
	HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	// 获取草稿数据
	sqlite3_bind_int64(stmt_test_draft_get, 1, iID);
	if ( sqlite3_step(stmt_test_draft_get) != SQLITE_ROW ) {
		sqlite3_reset(stmt_test_draft_get);
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Draft not found\"}", 0);
		return;
	}
	
	// 提取草稿数据
	xtime now = xrtNow();
	int iIdx = 1;
	int iCol = 1;  // 跳过 id

	// 作者信�?
	int64 iAuthorType = sqlite3_column_int64(stmt_test_draft_get, iCol++);
	int64 iAuthorId = sqlite3_column_int64(stmt_test_draft_get, iCol++);
	str sAuthorName = xrtCopyStr((str)sqlite3_column_text(stmt_test_draft_get, iCol++), 0);
	sqlite3_reset(stmt_test_draft_get);
	
	// 插入主表
	sqlite3_bind_int64(stmt_test_add, iIdx++, now);
	sqlite3_bind_int64(stmt_test_add, iIdx++, now);
	sqlite3_step(stmt_test_add);
	int64 newId = sqlite3_last_insert_rowid(G_DB);
	sqlite3_reset(stmt_test_add);
	
	// 删除草稿
	sqlite3_bind_int64(stmt_test_draft_publish, 1, iID);
	sqlite3_step(stmt_test_draft_publish);
	sqlite3_reset(stmt_test_draft_publish);
	
	if ( sAuthorName ) xrtFree(sAuthorName);
	
	HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"id\": %lld}}", newId);
}

// 批量发布草稿
void Admin_test_Draft_BatchPublish(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !HttpMethodIs(objReq, "POST") ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		if ( tblForm ) xvoUnref(tblForm);
		return;
	}
	
	xvalue arrIds = xvoTableGetValue(tblForm, "ids", 3);
	if ( !arrIds || arrIds->Type != XVO_DT_ARRAY ) {
		xvoUnref(tblForm);
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ids\"}", 0);
		return;
	}
	
	int iCount = xvoArrayItemCount(arrIds);
	int iSuccess = 0;
	xtime now = xrtNow();
	
	for ( int i = 0; i < iCount; i++ ) {
		int64 iID = xvoArrayGetInt(arrIds, i);
		if ( iID <= 0 ) continue;
		
		// 获取草稿数据
		sqlite3_bind_int64(stmt_test_draft_get, 1, iID);
		if ( sqlite3_step(stmt_test_draft_get) == SQLITE_ROW ) {
			int iIdx = 1;
			int iCol = 1;  // 跳过 id

			sqlite3_reset(stmt_test_draft_get);
			
			// 插入主表
			sqlite3_bind_int64(stmt_test_add, iIdx++, now);
			sqlite3_bind_int64(stmt_test_add, iIdx++, now);
			sqlite3_step(stmt_test_add);
			sqlite3_reset(stmt_test_add);
			
			// 删除草稿
			sqlite3_bind_int64(stmt_test_draft_publish, 1, iID);
			sqlite3_step(stmt_test_draft_publish);
			sqlite3_reset(stmt_test_draft_publish);
			
			iSuccess++;
		} else {
			sqlite3_reset(stmt_test_draft_get);
		}
	}
	
	xvoUnref(tblForm);
	HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"count\": %d}}", iSuccess);
}

// 批量删除草稿
void Admin_test_Draft_BatchDelete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !HttpMethodIs(objReq, "DELETE") ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON((str)xsReqBody(objReq), xsReqBodyLen(objReq));
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		if ( tblForm ) xvoUnref(tblForm);
		return;
	}
	
	xvalue arrIds = xvoTableGetValue(tblForm, "ids", 3);
	if ( !arrIds || arrIds->Type != XVO_DT_ARRAY ) {
		xvoUnref(tblForm);
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ids\"}", 0);
		return;
	}
	
	int iCount = xvoArrayItemCount(arrIds);
	int iSuccess = 0;
	
	for ( int i = 0; i < iCount; i++ ) {
		int64 iID = xvoArrayGetInt(arrIds, i);
		if ( iID <= 0 ) continue;
		
		// 删除草稿
		sqlite3_bind_int64(stmt_test_draft_del, 1, iID);
		if ( sqlite3_step(stmt_test_draft_del) == SQLITE_DONE ) {
			iSuccess++;
		}
		sqlite3_reset(stmt_test_draft_del);
	}
	
	xvoUnref(tblForm);
	HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"count\": %d}}", iSuccess);
}

// 清空草稿�?
void Admin_test_Draft_Clear(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !HttpMethodIs(objReq, "DELETE") ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	// 清空草稿�?
	sqlite3_step(stmt_test_draft_clear);
	sqlite3_reset(stmt_test_draft_clear);
	
	// 重置ID自增序列
	char* sErr = NULL;
	sqlite3_exec(G_DB, "DELETE FROM sqlite_sequence WHERE name='model_test_test_draft'", NULL, NULL, &sErr);
	if ( sErr ) sqlite3_free(sErr);
	
	http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}

// 获取草稿数量
void Admin_test_Draft_Count(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int64 iCount = 0;
	if ( sqlite3_step(stmt_test_draft_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_test_draft_count, 0);
	}
	sqlite3_reset(stmt_test_draft_count);
	
	HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"count\": %lld}", iCount);
}




// ==================== 路由注册 ====================

void Model_test_RegisterRoutes()
{
	printf("        [Model] Registering routes for test...\n");
	

	// 前台 API 路由
	Model_AddRoute("/api/v1/test/test/all", Api_test_List, FALSE, FALSE, 0, 0);
	Model_AddRoute("/api/v1/test/test/get", Api_test_Get, FALSE, FALSE, 0, 0);


/* SUBMIT DISABLED

	// 前台投稿路由
	Model_AddRoute("/api/v1/test/test/submit", Api_test_Submit, TRUE, FALSE, 0, 0);
*/

/* REPLY DISABLED

	// 评论路由
	Model_AddRoute("/api/v1/test/test/reply/all", Api_test_Reply_List, FALSE, FALSE, 0, 0);
	Model_AddRoute("/api/v1/test/test/reply/add", Api_test_Reply_Add, TRUE, FALSE, 0, {{REPLY_AUTH_LEVEL}});
	Model_AddRoute("/api/v1/test/test/reply/delete", Api_test_Reply_Delete, TRUE, FALSE, 0, {{REPLY_AUTH_LEVEL}});
*/


	// 后台管理路由
	Model_AddRoute("/admin/test/test/list", Admin_test_List, TRUE, TRUE, 0, 0);
	Model_AddRoute("/admin/test/test/get", Admin_test_Get, TRUE, TRUE, 0, 0);
	Model_AddRoute("/admin/test/test/add", Admin_test_Add, TRUE, TRUE, 0, 0);
	Model_AddRoute("/admin/test/test/save", Admin_test_Save, TRUE, TRUE, 0, 0);
	Model_AddRoute("/admin/test/test/delete", Admin_test_Delete, TRUE, TRUE, 0, 0);



	// 草稿管理路由
	Model_AddRoute("/admin/test/test/draft/list", Admin_test_Draft_List, TRUE, TRUE, 0, 0);
	Model_AddRoute("/admin/test/test/draft/get", Admin_test_Draft_Get, TRUE, TRUE, 0, 0);
	Model_AddRoute("/admin/test/test/draft/save", Admin_test_Draft_Save, TRUE, TRUE, 0, 0);
	Model_AddRoute("/admin/test/test/draft/delete", Admin_test_Draft_Delete, TRUE, TRUE, 0, 0);
	Model_AddRoute("/admin/test/test/draft/publish", Admin_test_Draft_Publish, TRUE, TRUE, 0, 0);
	Model_AddRoute("/admin/test/test/draft/batch_publish", Admin_test_Draft_BatchPublish, TRUE, TRUE, 0, 0);
	Model_AddRoute("/admin/test/test/draft/delete_batch", Admin_test_Draft_BatchDelete, TRUE, TRUE, 0, 0);
	Model_AddRoute("/admin/test/test/draft/clear", Admin_test_Draft_Clear, TRUE, TRUE, 0, 0);
	Model_AddRoute("/admin/test/test/draft/count", Admin_test_Draft_Count, TRUE, TRUE, 0, 0);

}

void Model_test_UnregisterRoutes()
{
	printf("        [Model] Unregistering routes for test...\n");
	

	Model_RemoveRoute("/api/v1/test/test/all");
	Model_RemoveRoute("/api/v1/test/test/get");


/* SUBMIT DISABLED

	Model_RemoveRoute("/api/v1/test/test/submit");
*/

/* REPLY DISABLED

	Model_RemoveRoute("/api/v1/test/test/reply/all");
	Model_RemoveRoute("/api/v1/test/test/reply/add");
	Model_RemoveRoute("/api/v1/test/test/reply/delete");
*/


	Model_RemoveRoute("/admin/test/test/all");
	Model_RemoveRoute("/admin/test/test/get");
	Model_RemoveRoute("/admin/test/test/add");
	Model_RemoveRoute("/admin/test/test/save");
	Model_RemoveRoute("/admin/test/test/delete");



	Model_RemoveRoute("/admin/test/test/draft/all");
	Model_RemoveRoute("/admin/test/test/draft/get");
	Model_RemoveRoute("/admin/test/test/draft/save");
	Model_RemoveRoute("/admin/test/test/draft/delete");
	Model_RemoveRoute("/admin/test/test/draft/publish");
	Model_RemoveRoute("/admin/test/test/draft/batch_publish");
	Model_RemoveRoute("/admin/test/test/draft/delete_batch");
	Model_RemoveRoute("/admin/test/test/draft/clear");
	Model_RemoveRoute("/admin/test/test/draft/count");

}



// ==================== 模型入口 ====================

void Model_test_Init()
{
	printf("    [Model] Init test...\n");
	Model_test_InitStmt();
	Model_test_RegisterRoutes();
}

void Model_test_Unit()
{
	printf("    [Model] Unit test...\n");
	Model_test_UnregisterRoutes();
	Model_test_FreeStmt();
}
