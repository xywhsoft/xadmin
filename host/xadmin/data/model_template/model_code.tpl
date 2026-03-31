
// ============================================
// 模型: {{MODEL_TITLE}} ({{MODEL_NAME}})
// 自动生成代码 - 请勿手动修改
// 生成时间: {{GEN_TIME}}
// TCC独立状态机编译
// ============================================

// 引入 xserver 基础库
#include <xs_vnext_full.h>

// 全局变量（通过 Model_SetGlobalData 传入）
sqlite3* G_DB;

// 模型管理器特有函数声明
// 注意：Model_AddRoute 实际返回 RouteInfo*，但模型代码不需要使用返回值
extern void* Model_AddRoute(str, void*, bool, bool, int, int);
extern void Model_RemoveRoute(str);

// HTTP helper declarations
bool HttpMethodIs(XS_RequestObject objReq, const char* sMethod);
int HttpGetQueryVar(XS_RequestObject objReq, const char* sName, char* sOut, size_t iOutCap);
int http_reply(XS_ResponseObject objResp, int iCode, str sHead, const void* pBody, size_t iLen);
int HttpReplyFormat(XS_ResponseObject objResp, int iCode, str sHead, str sFormat, ...);

#define HTTP_CT_JSON "Content-Type: application/json\r\n"

// 接收主系统传递的全局数据
void Model_SetGlobalData(int idx, void* ptr)
{
	if ( idx == 1 ) {
		G_DB = (sqlite3*)ptr;
	}
}

// ==================== 模型代码 ====================

// 预编译语句
sqlite3_stmt* stmt_{{MODEL_NAME}}_all;
sqlite3_stmt* stmt_{{MODEL_NAME}}_get;
sqlite3_stmt* stmt_{{MODEL_NAME}}_add;
sqlite3_stmt* stmt_{{MODEL_NAME}}_put;
sqlite3_stmt* stmt_{{MODEL_NAME}}_del;
sqlite3_stmt* stmt_{{MODEL_NAME}}_count;

{{#IF_ENABLE_ACCESS_CONTROL}}
// 访问控制预编译语句
sqlite3_stmt* stmt_{{MODEL_NAME}}_access;
sqlite3_stmt* stmt_{{MODEL_NAME}}_member_level;
{{#ENDIF_ENABLE_ACCESS_CONTROL}}

{{#IF_ENABLE_REPLY}}
// 评论预编译语句
sqlite3_stmt* stmt_{{MODEL_NAME}}_reply_list;
sqlite3_stmt* stmt_{{MODEL_NAME}}_reply_add;
sqlite3_stmt* stmt_{{MODEL_NAME}}_reply_del;
sqlite3_stmt* stmt_{{MODEL_NAME}}_reply_count;
sqlite3_stmt* stmt_{{MODEL_NAME}}_reply_get;
{{#ENDIF_ENABLE_REPLY}}

{{#IF_ENABLE_DRAFT}}
// 草稿预编译语句
sqlite3_stmt* stmt_{{MODEL_NAME}}_draft_all;
sqlite3_stmt* stmt_{{MODEL_NAME}}_draft_get;
sqlite3_stmt* stmt_{{MODEL_NAME}}_draft_add;
sqlite3_stmt* stmt_{{MODEL_NAME}}_draft_put;
sqlite3_stmt* stmt_{{MODEL_NAME}}_draft_del;
sqlite3_stmt* stmt_{{MODEL_NAME}}_draft_count;
sqlite3_stmt* stmt_{{MODEL_NAME}}_draft_publish;
sqlite3_stmt* stmt_{{MODEL_NAME}}_draft_clear;
{{#ENDIF_ENABLE_DRAFT}}

// 初始化预编译语句
void Model_{{MODEL_NAME}}_InitStmt()
{
	sqlite3* db = G_DB;
	
	// 分页获取列表
	sqlite3_prepare_v3(db,
{{#IF_ENABLE_ACCESS_CONTROL}}
		"SELECT id, {{FIELD_NAMES}}, accessLevel, accessPrice, accessPreview, authorType, authorId, authorName, createTime, updateTime FROM {{TABLE_NAME}} "
{{#ELSE}}
		"SELECT id, {{FIELD_NAMES}}, authorType, authorId, authorName, createTime, updateTime FROM {{TABLE_NAME}} "
{{#ENDIF_ENABLE_ACCESS_CONTROL}}
		"WHERE isDelete = 0 ORDER BY id DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt_{{MODEL_NAME}}_all, NULL);
	
	// 根据ID获取单条记录
	sqlite3_prepare_v3(db,
{{#IF_ENABLE_ACCESS_CONTROL}}
		"SELECT id, {{FIELD_NAMES}}, accessLevel, accessPrice, accessPreview, authorType, authorId, authorName, createTime, updateTime FROM {{TABLE_NAME}} WHERE id = ? AND isDelete = 0",
{{#ELSE}}
		"SELECT id, {{FIELD_NAMES}}, authorType, authorId, authorName, createTime, updateTime FROM {{TABLE_NAME}} WHERE id = ? AND isDelete = 0",
{{#ENDIF_ENABLE_ACCESS_CONTROL}}
		-1, 0, &stmt_{{MODEL_NAME}}_get, NULL);
	
	// 添加记录
	sqlite3_prepare_v3(db,
{{#IF_ENABLE_ACCESS_CONTROL}}
		"INSERT INTO {{TABLE_NAME}} ({{FIELD_NAMES}}, accessLevel, accessPrice, accessPreview, authorType, authorId, authorName, createTime, updateTime, isDelete) VALUES ({{FIELD_PLACEHOLDERS}}, ?, ?, ?, ?, ?, ?, ?, ?, 0)",
{{#ELSE}}
		"INSERT INTO {{TABLE_NAME}} ({{FIELD_NAMES}}, authorType, authorId, authorName, createTime, updateTime, isDelete) VALUES ({{FIELD_PLACEHOLDERS}}, ?, ?, ?, ?, ?, 0)",
{{#ENDIF_ENABLE_ACCESS_CONTROL}}
		-1, 0, &stmt_{{MODEL_NAME}}_add, NULL);
	
	// 更新记录（不更新作者字段，保留原作者信息）
	sqlite3_prepare_v3(db,
{{#IF_ENABLE_ACCESS_CONTROL}}
		"UPDATE {{TABLE_NAME}} SET {{FIELD_UPDATE_SET}}, accessLevel = ?, accessPrice = ?, accessPreview = ?, updateTime = ? WHERE id = ?",
{{#ELSE}}
		"UPDATE {{TABLE_NAME}} SET {{FIELD_UPDATE_SET}}, updateTime = ? WHERE id = ?",
{{#ENDIF_ENABLE_ACCESS_CONTROL}}
		-1, 0, &stmt_{{MODEL_NAME}}_put, NULL);
	
	// 删除记录（软删除）
	sqlite3_prepare_v3(db,
		"UPDATE {{TABLE_NAME}} SET isDelete = 1, updateTime = ? WHERE id = ?",
		-1, 0, &stmt_{{MODEL_NAME}}_del, NULL);
	
	// 统计总数
	sqlite3_prepare_v3(db,
		"SELECT COUNT(*) FROM {{TABLE_NAME}} WHERE isDelete = 0",
		-1, 0, &stmt_{{MODEL_NAME}}_count, NULL);
	
{{#IF_ENABLE_ACCESS_CONTROL}}
	// 获取内容的访问控制信息
	sqlite3_prepare_v3(db,
		"SELECT accessLevel, accessPrice, accessPreview FROM {{TABLE_NAME}} WHERE id = ? AND isDelete = 0",
		-1, 0, &stmt_{{MODEL_NAME}}_access, NULL);
	
	// 获取会员的权限级别
	sqlite3_prepare_v3(db,
		"SELECT g.authLevel FROM member m JOIN memberGroup g ON m.groupId = g.id WHERE m.id = ? AND m.isDelete = 0 AND m.status = 1",
		-1, 0, &stmt_{{MODEL_NAME}}_member_level, NULL);
{{#ENDIF_ENABLE_ACCESS_CONTROL}}
	
{{#IF_ENABLE_REPLY}}
	// 评论列表（根据内容ID）
	sqlite3_prepare_v3(db,
		"SELECT id, userId, userType, content, quoteId, quoteText, status, createTime "
		"FROM reply WHERE modelName = '{{MODEL_NAME}}' AND contentId = ? AND isDelete = 0 "
		"ORDER BY id DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt_{{MODEL_NAME}}_reply_list, NULL);
	
	// 获取单条评论
	sqlite3_prepare_v3(db,
		"SELECT id, userId, userType, content, quoteId, quoteText, status, createTime "
		"FROM reply WHERE id = ? AND isDelete = 0",
		-1, 0, &stmt_{{MODEL_NAME}}_reply_get, NULL);
	
	// 添加评论
	sqlite3_prepare_v3(db,
		"INSERT INTO reply (modelName, contentId, userId, userType, content, quoteId, quoteText, status, createTime, updateTime, isDelete) "
		"VALUES ('{{MODEL_NAME}}', ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)",
		-1, 0, &stmt_{{MODEL_NAME}}_reply_add, NULL);
	
	// 删除评论（软删除）
	sqlite3_prepare_v3(db,
		"UPDATE reply SET isDelete = 1, updateTime = ? WHERE id = ?",
		-1, 0, &stmt_{{MODEL_NAME}}_reply_del, NULL);
	
	// 评论总数
	sqlite3_prepare_v3(db,
		"SELECT COUNT(*) FROM reply WHERE modelName = '{{MODEL_NAME}}' AND contentId = ? AND isDelete = 0",
		-1, 0, &stmt_{{MODEL_NAME}}_reply_count, NULL);
{{#ENDIF_ENABLE_REPLY}}

{{#IF_ENABLE_DRAFT}}
	// 草稿列表
	sqlite3_prepare_v3(db,
		"SELECT id, {{FIELD_NAMES}}, authorType, authorId, authorName, createTime, updateTime FROM {{DRAFT_TABLE_NAME}} "
		"ORDER BY id DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt_{{MODEL_NAME}}_draft_all, NULL);
	
	// 获取单条草稿
	sqlite3_prepare_v3(db,
		"SELECT id, {{FIELD_NAMES}}, authorType, authorId, authorName, createTime, updateTime FROM {{DRAFT_TABLE_NAME}} WHERE id = ?",
		-1, 0, &stmt_{{MODEL_NAME}}_draft_get, NULL);
	
	// 添加草稿
	sqlite3_prepare_v3(db,
		"INSERT INTO {{DRAFT_TABLE_NAME}} ({{FIELD_NAMES}}, authorType, authorId, authorName, createTime, updateTime) VALUES ({{FIELD_PLACEHOLDERS}}, ?, ?, ?, ?, ?)",
		-1, 0, &stmt_{{MODEL_NAME}}_draft_add, NULL);
	
	// 更新草稿
	sqlite3_prepare_v3(db,
		"UPDATE {{DRAFT_TABLE_NAME}} SET {{FIELD_UPDATE_SET}}, authorType = ?, authorId = ?, authorName = ?, updateTime = ? WHERE id = ?",
		-1, 0, &stmt_{{MODEL_NAME}}_draft_put, NULL);
	
	// 删除草稿（硬删除）
	sqlite3_prepare_v3(db,
		"DELETE FROM {{DRAFT_TABLE_NAME}} WHERE id = ?",
		-1, 0, &stmt_{{MODEL_NAME}}_draft_del, NULL);
	
	// 草稿总数
	sqlite3_prepare_v3(db,
		"SELECT COUNT(*) FROM {{DRAFT_TABLE_NAME}}",
		-1, 0, &stmt_{{MODEL_NAME}}_draft_count, NULL);
	
	// 发布草稿（插入主表后删除草稿）- 只删除草稿记录
	sqlite3_prepare_v3(db,
		"DELETE FROM {{DRAFT_TABLE_NAME}} WHERE id = ?",
		-1, 0, &stmt_{{MODEL_NAME}}_draft_publish, NULL);
	
	// 清空草稿箱
	sqlite3_prepare_v3(db,
		"DELETE FROM {{DRAFT_TABLE_NAME}}",
		-1, 0, &stmt_{{MODEL_NAME}}_draft_clear, NULL);
{{#ENDIF_ENABLE_DRAFT}}
}

// 销毁预编译语句
void Model_{{MODEL_NAME}}_FreeStmt()
{
	if ( stmt_{{MODEL_NAME}}_all ) sqlite3_finalize(stmt_{{MODEL_NAME}}_all);
	if ( stmt_{{MODEL_NAME}}_get ) sqlite3_finalize(stmt_{{MODEL_NAME}}_get);
	if ( stmt_{{MODEL_NAME}}_add ) sqlite3_finalize(stmt_{{MODEL_NAME}}_add);
	if ( stmt_{{MODEL_NAME}}_put ) sqlite3_finalize(stmt_{{MODEL_NAME}}_put);
	if ( stmt_{{MODEL_NAME}}_del ) sqlite3_finalize(stmt_{{MODEL_NAME}}_del);
	if ( stmt_{{MODEL_NAME}}_count ) sqlite3_finalize(stmt_{{MODEL_NAME}}_count);
{{#IF_ENABLE_ACCESS_CONTROL}}
	if ( stmt_{{MODEL_NAME}}_access ) sqlite3_finalize(stmt_{{MODEL_NAME}}_access);
	if ( stmt_{{MODEL_NAME}}_member_level ) sqlite3_finalize(stmt_{{MODEL_NAME}}_member_level);
{{#ENDIF_ENABLE_ACCESS_CONTROL}}
{{#IF_ENABLE_REPLY}}
	if ( stmt_{{MODEL_NAME}}_reply_list ) sqlite3_finalize(stmt_{{MODEL_NAME}}_reply_list);
	if ( stmt_{{MODEL_NAME}}_reply_get ) sqlite3_finalize(stmt_{{MODEL_NAME}}_reply_get);
	if ( stmt_{{MODEL_NAME}}_reply_add ) sqlite3_finalize(stmt_{{MODEL_NAME}}_reply_add);
	if ( stmt_{{MODEL_NAME}}_reply_del ) sqlite3_finalize(stmt_{{MODEL_NAME}}_reply_del);
	if ( stmt_{{MODEL_NAME}}_reply_count ) sqlite3_finalize(stmt_{{MODEL_NAME}}_reply_count);
{{#ENDIF_ENABLE_REPLY}}
{{#IF_ENABLE_DRAFT}}
	if ( stmt_{{MODEL_NAME}}_draft_all ) sqlite3_finalize(stmt_{{MODEL_NAME}}_draft_all);
	if ( stmt_{{MODEL_NAME}}_draft_get ) sqlite3_finalize(stmt_{{MODEL_NAME}}_draft_get);
	if ( stmt_{{MODEL_NAME}}_draft_add ) sqlite3_finalize(stmt_{{MODEL_NAME}}_draft_add);
	if ( stmt_{{MODEL_NAME}}_draft_put ) sqlite3_finalize(stmt_{{MODEL_NAME}}_draft_put);
	if ( stmt_{{MODEL_NAME}}_draft_del ) sqlite3_finalize(stmt_{{MODEL_NAME}}_draft_del);
	if ( stmt_{{MODEL_NAME}}_draft_count ) sqlite3_finalize(stmt_{{MODEL_NAME}}_draft_count);
	if ( stmt_{{MODEL_NAME}}_draft_publish ) sqlite3_finalize(stmt_{{MODEL_NAME}}_draft_publish);
	if ( stmt_{{MODEL_NAME}}_draft_clear ) sqlite3_finalize(stmt_{{MODEL_NAME}}_draft_clear);
{{#ENDIF_ENABLE_DRAFT}}
}



{{#IF_ENABLE_API}}
// ==================== 前台 API ====================

// 获取列表
void Api_{{MODEL_NAME}}_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
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
	if ( sqlite3_step(stmt_{{MODEL_NAME}}_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_{{MODEL_NAME}}_count, 0);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_count);
	
	// 获取数据
	xvalue arrData = xvoCreateArray();
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_all, 1, iLimit);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_all, 2, iOffset);
	while ( sqlite3_step(stmt_{{MODEL_NAME}}_all) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_{{MODEL_NAME}}_all, iCol++));
{{FIELD_READ_CODE}}
		xtime iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_all, iCol++);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_all, iCol++);
		xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_all);
	
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
void Api_{{MODEL_NAME}}_Get(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sID[24];
	HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
{{#IF_ENABLE_ACCESS_CONTROL}}
	// ========== 访问控制检查 ==========
	int iAccessLevel = 0;
	double fAccessPrice = 0;
	str sAccessPreview = NULL;
	
	// 获取内容的访问控制信息
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_access, 1, iID);
	if ( sqlite3_step(stmt_{{MODEL_NAME}}_access) == SQLITE_ROW ) {
		iAccessLevel = sqlite3_column_int(stmt_{{MODEL_NAME}}_access, 0);
		fAccessPrice = sqlite3_column_double(stmt_{{MODEL_NAME}}_access, 1);
		str sTmp = (str)sqlite3_column_text(stmt_{{MODEL_NAME}}_access, 2);
		if ( sTmp && strlen(sTmp) > 0 ) {
			sAccessPreview = xrtCopyStr(sTmp, 0);
		}
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_access);
	
	// 检查访问权限
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
				sqlite3_bind_int64(stmt_{{MODEL_NAME}}_member_level, 1, iMemberId);
				if ( sqlite3_step(stmt_{{MODEL_NAME}}_member_level) == SQLITE_ROW ) {
					iMemberAuthLevel = sqlite3_column_int(stmt_{{MODEL_NAME}}_member_level, 0);
				}
				sqlite3_reset(stmt_{{MODEL_NAME}}_member_level);
			}
		}
		
		if ( iAccessLevel > 0 ) {
			// 需要指定权限级别
			if ( iMemberId <= 0 ) {
				bAccessDenied = TRUE;
				sAccessDeniedMsg = "请先登录";
				iRequiredLevel = iAccessLevel;
			} else if ( iMemberAuthLevel < iAccessLevel ) {
				bAccessDenied = TRUE;
				sAccessDeniedMsg = "权限不足，需要更高级别会员";
				iRequiredLevel = iAccessLevel;
			}
		} else if ( iAccessLevel == -1 ) {
			// 付费内容 - 待实现购买记录检查
			// TODO: 检查用户是否已购买
			bAccessDenied = TRUE;
			sAccessDeniedMsg = "付费内容，请购买后查看";
		} else if ( iAccessLevel == -2 ) {
			// 仅作者可见 - 待实现作者字段检查
			// TODO: 检查是否为内容作者
			bAccessDenied = TRUE;
			sAccessDeniedMsg = "仅作者可见";
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
			xvoTableSetFloat(tblRet, "accessPrice", 11, fAccessPrice);
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
	// ========== 访问控制检查结束 ==========
{{#ENDIF_ENABLE_ACCESS_CONTROL}}
	
	xvalue tblData = NULL;
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_get, 1, iID);
	if ( sqlite3_step(stmt_{{MODEL_NAME}}_get) == SQLITE_ROW ) {
		tblData = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblData, "id", 2, sqlite3_column_int64(stmt_{{MODEL_NAME}}_get, iCol++));
{{FIELD_READ_CODE_GET}}
		xtime iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_get, iCol++);
		xvoTableSetText(tblData, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_get, iCol++);
		xvoTableSetText(tblData, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_get);
	
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
{{#ENDIF_ENABLE_API}}



{{#IF_ENABLE_SUBMIT}}
// ==================== 前台投稿 ====================

// 投稿内容
void Api_{{MODEL_NAME}}_Submit(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
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
	
	// 检查是否允许游客投稿
	int iAllowGuest = {{ALLOW_GUEST_SUBMIT}};
	if ( (iMemberId <= 0) && !iAllowGuest ) {
		xvoUnref(tblForm);
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Please login first\"}", 0);
		return;
	}
	
	// 决定数据去向：游客始终进草稿，会员根据配置决定
	int iNeedReview = {{SUBMIT_NEED_REVIEW}};
	bool bToDraft = (iMemberId <= 0) || iNeedReview;
	
	// 作者信息
	int iAuthorType = (iMemberId <= 0) ? 2 : 1;  // 2=游客, 1=前台会员
	int64 iAuthorId = iMemberId;
	str sAuthorName = xvoTableGetText(tblForm, "authorName", 10);
	if ( !sAuthorName || strlen(sAuthorName) == 0 ) {
		sAuthorName = sMemberNickname ? sMemberNickname : (str)"匿名用户";
	}
	
	xtime now = xrtNow();
	int iIdx = 1;
	int64 newId = 0;
	
	if ( bToDraft ) {
		// 进入草稿箱
{{FIELD_BIND_ADD_CODE}}
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_add, iIdx++, iAuthorType);
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_add, iIdx++, iAuthorId);
		sqlite3_bind_text(stmt_{{MODEL_NAME}}_draft_add, iIdx++, sAuthorName, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_add, iIdx++, now);
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_add, iIdx++, now);
		sqlite3_step(stmt_{{MODEL_NAME}}_draft_add);
		newId = sqlite3_last_insert_rowid(G_DB);
		sqlite3_reset(stmt_{{MODEL_NAME}}_draft_add);
	} else {
		// 直接发布
{{FIELD_BIND_ADD_CODE}}
{{#IF_ENABLE_ACCESS_CONTROL}}
		// 访问控制字段（前台投稿默认公开访问）
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, 0);  // accessLevel = 0 (公开)
		sqlite3_bind_double(stmt_{{MODEL_NAME}}_add, iIdx++, 0);  // accessPrice = 0
		sqlite3_bind_text(stmt_{{MODEL_NAME}}_add, iIdx++, (str)"", -1, SQLITE_STATIC);  // accessPreview = ""
{{#ENDIF_ENABLE_ACCESS_CONTROL}}
		// 作者字段
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, iAuthorType);
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, iAuthorId);
		sqlite3_bind_text(stmt_{{MODEL_NAME}}_add, iIdx++, sAuthorName, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, now);
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, now);
		sqlite3_step(stmt_{{MODEL_NAME}}_add);
		newId = sqlite3_last_insert_rowid(G_DB);
		sqlite3_reset(stmt_{{MODEL_NAME}}_add);
	}
	
	xvoUnref(tblForm);
	
	if ( bToDraft ) {
		HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Submitted to review\", \"data\": {\"id\": %lld, \"isDraft\": true}}", newId);
	} else {
		HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Published\", \"data\": {\"id\": %lld, \"isDraft\": false}}", newId);
	}
}
{{#ENDIF_ENABLE_SUBMIT}}



{{#IF_ENABLE_REPLY}}
// ==================== 评论 API ====================

// 获取评论列表
void Api_{{MODEL_NAME}}_Reply_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
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
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_reply_count, 1, iContentId);
	if ( sqlite3_step(stmt_{{MODEL_NAME}}_reply_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_{{MODEL_NAME}}_reply_count, 0);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_reply_count);
	
	// 获取评论列表
	xvalue arrData = xvoCreateArray();
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_reply_list, 1, iContentId);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_reply_list, 2, iLimit);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_reply_list, 3, iOffset);
	while ( sqlite3_step(stmt_{{MODEL_NAME}}_reply_list) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_{{MODEL_NAME}}_reply_list, iCol++));
		xvoTableSetInt(tblRow, "userId", 6, sqlite3_column_int64(stmt_{{MODEL_NAME}}_reply_list, iCol++));
		xvoTableSetInt(tblRow, "userType", 8, sqlite3_column_int64(stmt_{{MODEL_NAME}}_reply_list, iCol++));
		xvoTableSetText(tblRow, "content", 7, (str)sqlite3_column_text(stmt_{{MODEL_NAME}}_reply_list, iCol++), 0, FALSE);
		xvoTableSetInt(tblRow, "quoteId", 7, sqlite3_column_int64(stmt_{{MODEL_NAME}}_reply_list, iCol++));
		xvoTableSetText(tblRow, "quoteText", 9, (str)sqlite3_column_text(stmt_{{MODEL_NAME}}_reply_list, iCol++), 0, FALSE);
		xvoTableSetInt(tblRow, "status", 6, sqlite3_column_int64(stmt_{{MODEL_NAME}}_reply_list, iCol++));
		xtime iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_reply_list, iCol++);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_reply_list);
	
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
void Api_{{MODEL_NAME}}_Reply_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
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
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_reply_get, 1, iQuoteId);
		if ( sqlite3_step(stmt_{{MODEL_NAME}}_reply_get) == SQLITE_ROW ) {
			str sOrigContent = (str)sqlite3_column_text(stmt_{{MODEL_NAME}}_reply_get, 3);  // content 在第4列
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
		sqlite3_reset(stmt_{{MODEL_NAME}}_reply_get);
	}
	
	// 获取用户信息（从 session 中获取，这里简化处理）
	int64 iUserId = xvoTableGetInt(tblForm, "userId", 6);
	int64 iUserType = xvoTableGetInt(tblForm, "userType", 8);  // 0=会员, 1=后台用户
	
	// 状态：是否需要审核
	int64 iStatus = {{REPLY_NEED_APPROVE}} ? 0 : 1;  // 0=待审核, 1=已发布
	
	xtime now = xrtNow();
	int iIdx = 1;
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_reply_add, iIdx++, iContentId);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_reply_add, iIdx++, iUserId);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_reply_add, iIdx++, iUserType);
	sqlite3_bind_text(stmt_{{MODEL_NAME}}_reply_add, iIdx++, sContent, -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_reply_add, iIdx++, iQuoteId);
	sqlite3_bind_text(stmt_{{MODEL_NAME}}_reply_add, iIdx++, sQuoteText, -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_reply_add, iIdx++, iStatus);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_reply_add, iIdx++, now);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_reply_add, iIdx++, now);
	sqlite3_step(stmt_{{MODEL_NAME}}_reply_add);
	int64 newId = sqlite3_last_insert_rowid(G_DB);
	sqlite3_reset(stmt_{{MODEL_NAME}}_reply_add);
	
	// 清理临时内存
	if ( bFreeQuote ) {
		xrtFree(sQuoteText);
	}
	xvoUnref(tblForm);
	
	HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"id\": %lld}}", newId);
}

// 删除评论（需要校验权限）
void Api_{{MODEL_NAME}}_Reply_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sParam[24];
	HttpGetQueryVar(objReq, "id", sParam, sizeof(sParam));
	int64 iID = xrtStrToI64(sParam);
	if ( iID <= 0 ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	// TODO: 应该校验当前用户是否有权删除该评论
	
	xtime now = xrtNow();
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_reply_del, 1, now);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_reply_del, 2, iID);
	sqlite3_step(stmt_{{MODEL_NAME}}_reply_del);
	sqlite3_reset(stmt_{{MODEL_NAME}}_reply_del);
	
	http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}
{{#ENDIF_ENABLE_REPLY}}



{{#IF_ENABLE_ADMIN}}
// ==================== 后台管理 API ====================

// 后台列表
void Admin_{{MODEL_NAME}}_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
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
	if ( sqlite3_step(stmt_{{MODEL_NAME}}_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_{{MODEL_NAME}}_count, 0);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_count);
	
	// 获取数据
	xvalue arrData = xvoCreateArray();
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_all, 1, iLimit);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_all, 2, iOffset);
	while ( sqlite3_step(stmt_{{MODEL_NAME}}_all) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_{{MODEL_NAME}}_all, iCol++));
{{FIELD_READ_CODE}}
{{#IF_ENABLE_ACCESS_CONTROL}}
		// 访问控制字段
		xvoTableSetInt(tblRow, "accessLevel", 11, sqlite3_column_int64(stmt_{{MODEL_NAME}}_all, iCol++));
		xvoTableSetFloat(tblRow, "accessPrice", 11, sqlite3_column_double(stmt_{{MODEL_NAME}}_all, iCol++));
		xvoTableSetText(tblRow, "accessPreview", 13, (str)sqlite3_column_text(stmt_{{MODEL_NAME}}_all, iCol++), 0, FALSE);
{{#ENDIF_ENABLE_ACCESS_CONTROL}}
		// 作者字段
		xvoTableSetInt(tblRow, "authorType", 10, sqlite3_column_int64(stmt_{{MODEL_NAME}}_all, iCol++));
		xvoTableSetInt(tblRow, "authorId", 8, sqlite3_column_int64(stmt_{{MODEL_NAME}}_all, iCol++));
		xvoTableSetText(tblRow, "authorName", 10, (str)sqlite3_column_text(stmt_{{MODEL_NAME}}_all, iCol++), 0, FALSE);
		xtime iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_all, iCol++);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_all, iCol++);
		xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_all);
	
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
void Admin_{{MODEL_NAME}}_Get(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sID[24];
	HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xvalue tblData = NULL;
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_get, 1, iID);
	if ( sqlite3_step(stmt_{{MODEL_NAME}}_get) == SQLITE_ROW ) {
		tblData = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblData, "id", 2, sqlite3_column_int64(stmt_{{MODEL_NAME}}_get, iCol++));
{{FIELD_READ_CODE_GET}}
{{#IF_ENABLE_ACCESS_CONTROL}}
		// 访问控制字段
		xvoTableSetInt(tblData, "accessLevel", 11, sqlite3_column_int64(stmt_{{MODEL_NAME}}_get, iCol++));
		xvoTableSetFloat(tblData, "accessPrice", 11, sqlite3_column_double(stmt_{{MODEL_NAME}}_get, iCol++));
		xvoTableSetText(tblData, "accessPreview", 13, (str)sqlite3_column_text(stmt_{{MODEL_NAME}}_get, iCol++), 0, FALSE);
{{#ENDIF_ENABLE_ACCESS_CONTROL}}
		// 作者字段
		xvoTableSetInt(tblData, "authorType", 10, sqlite3_column_int64(stmt_{{MODEL_NAME}}_get, iCol++));
		xvoTableSetInt(tblData, "authorId", 8, sqlite3_column_int64(stmt_{{MODEL_NAME}}_get, iCol++));
		xvoTableSetText(tblData, "authorName", 10, (str)sqlite3_column_text(stmt_{{MODEL_NAME}}_get, iCol++), 0, FALSE);
		xtime iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_get, iCol++);
		xvoTableSetText(tblData, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_get, iCol++);
		xvoTableSetText(tblData, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_get);
	
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
void Admin_{{MODEL_NAME}}_Add(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
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
	
	// 检查是否保存为草稿
	int bIsDraft = xvoTableGetInt(tblForm, "isDraft", 7);
	
{{#IF_ENABLE_DRAFT}}
	if ( bIsDraft ) {
		// 保存到草稿箱
		xtime now = xrtNow();
		int iIdx = 1;
{{FIELD_BIND_DRAFT_ADD_CODE}}
		// 作者字段（优先使用表单中的authorName，否则使用当前管理员）
		int64 iAdminId = xvoTableGetInt(objSession, "id", 2);
		str sAdminUser = xvoTableGetText(objSession, "user", 4);
		str sFormAuthor = xvoTableGetText(tblForm, "authorName", 10);
		str sAuthorName = (sFormAuthor && strlen(sFormAuthor) > 0) ? sFormAuthor : (sAdminUser ? sAdminUser : (str)"");
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_add, iIdx++, 0);  // authorType = 0 (后台管理员)
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_add, iIdx++, iAdminId);
		sqlite3_bind_text(stmt_{{MODEL_NAME}}_draft_add, iIdx++, sAuthorName, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_add, iIdx++, now);
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_add, iIdx++, now);
		sqlite3_step(stmt_{{MODEL_NAME}}_draft_add);
		int64 newId = sqlite3_last_insert_rowid(G_DB);
		sqlite3_reset(stmt_{{MODEL_NAME}}_draft_add);
		xvoUnref(tblForm);
		
		HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"已保存到草稿箱\", \"data\": {\"id\": %lld}}", newId);
		return;
	}
{{#ENDIF_ENABLE_DRAFT}}
	
	// 正常发布
	xtime now = xrtNow();
	int iIdx = 1;
{{FIELD_BIND_ADD_CODE}}
{{#IF_ENABLE_ACCESS_CONTROL}}
	// 访问控制字段（从tblForm获取，默认公开）
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, xvoTableGetInt(tblForm, "accessLevel", 11));
	sqlite3_bind_double(stmt_{{MODEL_NAME}}_add, iIdx++, xvoTableGetFloat(tblForm, "accessPrice", 11));
	{ str sPreview = xvoTableGetText(tblForm, "accessPreview", 13); sqlite3_bind_text(stmt_{{MODEL_NAME}}_add, iIdx++, sPreview ? sPreview : (str)"", -1, SQLITE_STATIC); }
{{#ENDIF_ENABLE_ACCESS_CONTROL}}
	// 作者字段（优先使用表单中的authorName，否则使用当前管理员）
	int64 iAdminId = xvoTableGetInt(objSession, "id", 2);
	str sAdminUser = xvoTableGetText(objSession, "user", 4);
	str sFormAuthor = xvoTableGetText(tblForm, "authorName", 10);
	str sAuthorName = (sFormAuthor && strlen(sFormAuthor) > 0) ? sFormAuthor : (sAdminUser ? sAdminUser : (str)"");
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, 0);  // authorType = 0 (后台管理员)
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, iAdminId);
	sqlite3_bind_text(stmt_{{MODEL_NAME}}_add, iIdx++, sAuthorName, -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, now);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, now);
	sqlite3_step(stmt_{{MODEL_NAME}}_add);
	int64 newId = sqlite3_last_insert_rowid(G_DB);
	sqlite3_reset(stmt_{{MODEL_NAME}}_add);
	xvoUnref(tblForm);
	
	HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"id\": %lld}}", newId);
}

// 后台更新
void Admin_{{MODEL_NAME}}_Save(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
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
{{FIELD_BIND_UPDATE_CODE}}
{{#IF_ENABLE_ACCESS_CONTROL}}
	// 访问控制字段（从tblForm获取）
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_put, iIdx++, xvoTableGetInt(tblForm, "accessLevel", 11));
	sqlite3_bind_double(stmt_{{MODEL_NAME}}_put, iIdx++, xvoTableGetFloat(tblForm, "accessPrice", 11));
	{ str sPreview = xvoTableGetText(tblForm, "accessPreview", 13); sqlite3_bind_text(stmt_{{MODEL_NAME}}_put, iIdx++, sPreview ? sPreview : (str)"", -1, SQLITE_STATIC); }
{{#ENDIF_ENABLE_ACCESS_CONTROL}}
	// 不更新作者字段，保留原作者信息
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_put, iIdx++, now);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_put, iIdx++, iID);
	sqlite3_step(stmt_{{MODEL_NAME}}_put);
	sqlite3_reset(stmt_{{MODEL_NAME}}_put);
	xvoUnref(tblForm);
	
	http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}

// 后台删除
void Admin_{{MODEL_NAME}}_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sID[24];
	HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xtime now = xrtNow();
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_del, 1, now);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_del, 2, iID);
	sqlite3_step(stmt_{{MODEL_NAME}}_del);
	sqlite3_reset(stmt_{{MODEL_NAME}}_del);
	
	http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}
{{#ENDIF_ENABLE_ADMIN}}



{{#IF_ENABLE_DRAFT}}
// ==================== 草稿管理 API ====================

// 草稿列表
void Admin_{{MODEL_NAME}}_Draft_List(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
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
	if ( sqlite3_step(stmt_{{MODEL_NAME}}_draft_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_{{MODEL_NAME}}_draft_count, 0);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_draft_count);
	
	// 获取数据
	xvalue arrData = xvoCreateArray();
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_all, 1, iLimit);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_all, 2, iOffset);
	while ( sqlite3_step(stmt_{{MODEL_NAME}}_draft_all) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_{{MODEL_NAME}}_draft_all, iCol++));
{{FIELD_READ_CODE_DRAFT_LIST}}
		xvoTableSetInt(tblRow, "authorType", 10, sqlite3_column_int64(stmt_{{MODEL_NAME}}_draft_all, iCol++));
		xvoTableSetInt(tblRow, "authorId", 8, sqlite3_column_int64(stmt_{{MODEL_NAME}}_draft_all, iCol++));
		xvoTableSetText(tblRow, "authorName", 10, (str)sqlite3_column_text(stmt_{{MODEL_NAME}}_draft_all, iCol++), 0, FALSE);
		xtime iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_draft_all, iCol++);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_draft_all, iCol++);
		xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_draft_all);
	
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
void Admin_{{MODEL_NAME}}_Draft_Get(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sID[24];
	HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xvalue tblData = NULL;
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_get, 1, iID);
	if ( sqlite3_step(stmt_{{MODEL_NAME}}_draft_get) == SQLITE_ROW ) {
		tblData = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblData, "id", 2, sqlite3_column_int64(stmt_{{MODEL_NAME}}_draft_get, iCol++));
{{FIELD_READ_CODE_DRAFT_GET}}
		xvoTableSetInt(tblData, "authorType", 10, sqlite3_column_int64(stmt_{{MODEL_NAME}}_draft_get, iCol++));
		xvoTableSetInt(tblData, "authorId", 8, sqlite3_column_int64(stmt_{{MODEL_NAME}}_draft_get, iCol++));
		xvoTableSetText(tblData, "authorName", 10, (str)sqlite3_column_text(stmt_{{MODEL_NAME}}_draft_get, iCol++), 0, FALSE);
		xtime iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_draft_get, iCol++);
		xvoTableSetText(tblData, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_draft_get, iCol++);
		xvoTableSetText(tblData, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_draft_get);
	
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
void Admin_{{MODEL_NAME}}_Draft_Save(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
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
{{FIELD_BIND_DRAFT_UPDATE_CODE}}
		// 作者字段（优先使用表单中的authorName，否则使用当前管理员）
		int64 iEditorId = xvoTableGetInt(objSession, "id", 2);
		str sEditorUser = xvoTableGetText(objSession, "user", 4);
		str sFormAuthor = xvoTableGetText(tblForm, "authorName", 10);
		str sAuthorName = (sFormAuthor && strlen(sFormAuthor) > 0) ? sFormAuthor : (sEditorUser ? sEditorUser : (str)"");
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_put, iIdx++, 0);  // authorType = 0 (后台管理员)
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_put, iIdx++, iEditorId);
		sqlite3_bind_text(stmt_{{MODEL_NAME}}_draft_put, iIdx++, sAuthorName, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_put, iIdx++, now);
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_put, iIdx++, iID);
		sqlite3_step(stmt_{{MODEL_NAME}}_draft_put);
		sqlite3_reset(stmt_{{MODEL_NAME}}_draft_put);
	} else {
		// 添加草稿
		int iIdx = 1;
{{FIELD_BIND_DRAFT_ADD_CODE}}
		// 作者字段（优先使用表单中的authorName，否则使用当前管理员）
		int64 iAdminId = xvoTableGetInt(objSession, "id", 2);
		str sAdminUser = xvoTableGetText(objSession, "user", 4);
		str sFormAuthor = xvoTableGetText(tblForm, "authorName", 10);
		str sAuthorName = (sFormAuthor && strlen(sFormAuthor) > 0) ? sFormAuthor : (sAdminUser ? sAdminUser : (str)"");
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_add, iIdx++, 0);  // authorType = 0 (后台管理员)
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_add, iIdx++, iAdminId);
		sqlite3_bind_text(stmt_{{MODEL_NAME}}_draft_add, iIdx++, sAuthorName, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_add, iIdx++, now);
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_add, iIdx++, now);
		sqlite3_step(stmt_{{MODEL_NAME}}_draft_add);
		iID = sqlite3_last_insert_rowid(G_DB);
		sqlite3_reset(stmt_{{MODEL_NAME}}_draft_add);
	}
	
	xvoUnref(tblForm);
	HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"id\": %lld}}", iID);
}

// 删除草稿
void Admin_{{MODEL_NAME}}_Draft_Delete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sID[24];
	HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_del, 1, iID);
	sqlite3_step(stmt_{{MODEL_NAME}}_draft_del);
	sqlite3_reset(stmt_{{MODEL_NAME}}_draft_del);
	
	http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}

// 发布草稿
void Admin_{{MODEL_NAME}}_Draft_Publish(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	char sID[24];
	HttpGetQueryVar(objReq, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	// 获取草稿数据
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_get, 1, iID);
	if ( sqlite3_step(stmt_{{MODEL_NAME}}_draft_get) != SQLITE_ROW ) {
		sqlite3_reset(stmt_{{MODEL_NAME}}_draft_get);
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Draft not found\"}", 0);
		return;
	}
	
	// 提取草稿数据
	xtime now = xrtNow();
	int iIdx = 1;
	int iCol = 1;  // 跳过 id
{{FIELD_BIND_DRAFT_PUBLISH_CODE}}
	// 作者信息
	int64 iAuthorType = sqlite3_column_int64(stmt_{{MODEL_NAME}}_draft_get, iCol++);
	int64 iAuthorId = sqlite3_column_int64(stmt_{{MODEL_NAME}}_draft_get, iCol++);
	str sAuthorName = xrtCopyStr((str)sqlite3_column_text(stmt_{{MODEL_NAME}}_draft_get, iCol++), 0);
	sqlite3_reset(stmt_{{MODEL_NAME}}_draft_get);
	
{{#IF_ENABLE_ACCESS_CONTROL}}
	// 绑定访问控制字段（草稿发布默认公开访问）
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, 0);  // accessLevel = 0 (公开)
	sqlite3_bind_double(stmt_{{MODEL_NAME}}_add, iIdx++, 0);  // accessPrice = 0
	sqlite3_bind_text(stmt_{{MODEL_NAME}}_add, iIdx++, (str)"", -1, SQLITE_STATIC);  // accessPreview = ""
{{#ENDIF_ENABLE_ACCESS_CONTROL}}
	// 绑定作者信息到主表插入语句
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, iAuthorType);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, iAuthorId);
	sqlite3_bind_text(stmt_{{MODEL_NAME}}_add, iIdx++, sAuthorName ? sAuthorName : (str)"", -1, SQLITE_STATIC);
	
	// 插入主表
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, now);
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, now);
	sqlite3_step(stmt_{{MODEL_NAME}}_add);
	int64 newId = sqlite3_last_insert_rowid(G_DB);
	sqlite3_reset(stmt_{{MODEL_NAME}}_add);
	
	// 删除草稿
	sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_publish, 1, iID);
	sqlite3_step(stmt_{{MODEL_NAME}}_draft_publish);
	sqlite3_reset(stmt_{{MODEL_NAME}}_draft_publish);
	
	if ( sAuthorName ) xrtFree(sAuthorName);
	
	HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"id\": %lld}}", newId);
}

// 批量发布草稿
void Admin_{{MODEL_NAME}}_Draft_BatchPublish(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
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
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_get, 1, iID);
		if ( sqlite3_step(stmt_{{MODEL_NAME}}_draft_get) == SQLITE_ROW ) {
			int iIdx = 1;
			int iCol = 1;  // 跳过 id
{{FIELD_BIND_DRAFT_PUBLISH_CODE}}
			// 读取作者信息
			int64 iAuthorType = sqlite3_column_int64(stmt_{{MODEL_NAME}}_draft_get, iCol++);
			int64 iAuthorId = sqlite3_column_int64(stmt_{{MODEL_NAME}}_draft_get, iCol++);
			str sTmpAuthorName = (str)sqlite3_column_text(stmt_{{MODEL_NAME}}_draft_get, iCol++);
			sqlite3_reset(stmt_{{MODEL_NAME}}_draft_get);
			
{{#IF_ENABLE_ACCESS_CONTROL}}
			// 绑定访问控制字段（草稿发布默认公开访问）
			sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, 0);  // accessLevel = 0 (公开)
			sqlite3_bind_double(stmt_{{MODEL_NAME}}_add, iIdx++, 0);  // accessPrice = 0
			sqlite3_bind_text(stmt_{{MODEL_NAME}}_add, iIdx++, (str)"", -1, SQLITE_STATIC);  // accessPreview = ""
{{#ENDIF_ENABLE_ACCESS_CONTROL}}
			// 绑定作者信息到主表插入语句
			sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, iAuthorType);
			sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, iAuthorId);
			sqlite3_bind_text(stmt_{{MODEL_NAME}}_add, iIdx++, sTmpAuthorName ? sTmpAuthorName : (str)"", -1, SQLITE_TRANSIENT);
			
			// 插入主表
			sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, now);
			sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, now);
			sqlite3_step(stmt_{{MODEL_NAME}}_add);
			sqlite3_reset(stmt_{{MODEL_NAME}}_add);
			
			// 删除草稿
			sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_publish, 1, iID);
			sqlite3_step(stmt_{{MODEL_NAME}}_draft_publish);
			sqlite3_reset(stmt_{{MODEL_NAME}}_draft_publish);
			
			iSuccess++;
		} else {
			sqlite3_reset(stmt_{{MODEL_NAME}}_draft_get);
		}
	}
	
	xvoUnref(tblForm);
	HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"count\": %d}}", iSuccess);
}

// 批量删除草稿
void Admin_{{MODEL_NAME}}_Draft_BatchDelete(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
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
		sqlite3_bind_int64(stmt_{{MODEL_NAME}}_draft_del, 1, iID);
		if ( sqlite3_step(stmt_{{MODEL_NAME}}_draft_del) == SQLITE_DONE ) {
			iSuccess++;
		}
		sqlite3_reset(stmt_{{MODEL_NAME}}_draft_del);
	}
	
	xvoUnref(tblForm);
	HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"count\": %d}}", iSuccess);
}

// 清空草稿箱
void Admin_{{MODEL_NAME}}_Draft_Clear(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	if ( !HttpMethodIs(objReq, "DELETE") ) {
		http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	// 清空草稿箱
	sqlite3_step(stmt_{{MODEL_NAME}}_draft_clear);
	sqlite3_reset(stmt_{{MODEL_NAME}}_draft_clear);
	
	// 重置ID自增序列
	char* sErr = NULL;
	sqlite3_exec(G_DB, "DELETE FROM sqlite_sequence WHERE name='{{DRAFT_TABLE_NAME}}'", NULL, NULL, &sErr);
	if ( sErr ) sqlite3_free(sErr);
	
	http_reply(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}

// 获取草稿数量
void Admin_{{MODEL_NAME}}_Draft_Count(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession)
{
	int64 iCount = 0;
	if ( sqlite3_step(stmt_{{MODEL_NAME}}_draft_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_{{MODEL_NAME}}_draft_count, 0);
	}
	sqlite3_reset(stmt_{{MODEL_NAME}}_draft_count);
	
	HttpReplyFormat(objResp, 200, HTTP_CT_JSON, "{\"result\": true, \"count\": %lld}", iCount);
}
{{#ENDIF_ENABLE_DRAFT}}



// ==================== 路由注册 ====================

void Model_{{MODEL_NAME}}_RegisterRoutes()
{
	printf("        [Model] Registering routes for {{MODEL_NAME}}...\n");
	
{{#IF_ENABLE_API}}
	// 前台 API 路由
	Model_AddRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/all", Api_{{MODEL_NAME}}_List, FALSE, FALSE, 0, {{API_AUTH_LEVEL}});
	Model_AddRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/get", Api_{{MODEL_NAME}}_Get, FALSE, FALSE, 0, {{API_AUTH_LEVEL}});
{{#ENDIF_ENABLE_API}}

{{#IF_ENABLE_SUBMIT}}
	// 前台投稿路由
	Model_AddRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/submit", Api_{{MODEL_NAME}}_Submit, TRUE, FALSE, 0, {{API_AUTH_LEVEL}});
{{#ENDIF_ENABLE_SUBMIT}}

{{#IF_ENABLE_REPLY}}
	// 评论路由
	Model_AddRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/reply/all", Api_{{MODEL_NAME}}_Reply_List, FALSE, FALSE, 0, 0);
	Model_AddRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/reply/add", Api_{{MODEL_NAME}}_Reply_Add, TRUE, FALSE, 0, {{REPLY_AUTH_LEVEL}});
	Model_AddRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/reply/delete", Api_{{MODEL_NAME}}_Reply_Delete, TRUE, FALSE, 0, {{REPLY_AUTH_LEVEL}});
{{#ENDIF_ENABLE_REPLY}}

{{#IF_ENABLE_ADMIN}}
	// 后台管理路由
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/list", Admin_{{MODEL_NAME}}_List, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/get", Admin_{{MODEL_NAME}}_Get, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/add", Admin_{{MODEL_NAME}}_Add, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/save", Admin_{{MODEL_NAME}}_Save, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/delete", Admin_{{MODEL_NAME}}_Delete, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
{{#ENDIF_ENABLE_ADMIN}}

{{#IF_ENABLE_DRAFT}}
	// 草稿管理路由
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/list", Admin_{{MODEL_NAME}}_Draft_List, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/get", Admin_{{MODEL_NAME}}_Draft_Get, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/save", Admin_{{MODEL_NAME}}_Draft_Save, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/delete", Admin_{{MODEL_NAME}}_Draft_Delete, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/publish", Admin_{{MODEL_NAME}}_Draft_Publish, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/batch_publish", Admin_{{MODEL_NAME}}_Draft_BatchPublish, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/delete_batch", Admin_{{MODEL_NAME}}_Draft_BatchDelete, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/clear", Admin_{{MODEL_NAME}}_Draft_Clear, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
	Model_AddRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/count", Admin_{{MODEL_NAME}}_Draft_Count, TRUE, TRUE, {{ADMIN_AUTH_ID}}, 0);
{{#ENDIF_ENABLE_DRAFT}}
}

void Model_{{MODEL_NAME}}_UnregisterRoutes()
{
	printf("        [Model] Unregistering routes for {{MODEL_NAME}}...\n");
	
{{#IF_ENABLE_API}}
	Model_RemoveRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/all");
	Model_RemoveRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/get");
{{#ENDIF_ENABLE_API}}

{{#IF_ENABLE_SUBMIT}}
	Model_RemoveRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/submit");
{{#ENDIF_ENABLE_SUBMIT}}

{{#IF_ENABLE_REPLY}}
	Model_RemoveRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/reply/all");
	Model_RemoveRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/reply/add");
	Model_RemoveRoute("/api/v1/{{NAMESPACE_PATH}}{{MODEL_NAME}}/reply/delete");
{{#ENDIF_ENABLE_REPLY}}

{{#IF_ENABLE_ADMIN}}
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/all");
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/get");
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/add");
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/save");
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/delete");
{{#ENDIF_ENABLE_ADMIN}}

{{#IF_ENABLE_DRAFT}}
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/all");
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/get");
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/save");
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/delete");
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/publish");
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/batch_publish");
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/delete_batch");
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/clear");
	Model_RemoveRoute("/admin/{{NAMESPACE_PATH}}{{MODEL_NAME}}/draft/count");
{{#ENDIF_ENABLE_DRAFT}}
}



// ==================== 模型入口 ====================

void Model_{{MODEL_NAME}}_Init()
{
	printf("    [Model] Init {{MODEL_NAME}}...\n");
	Model_{{MODEL_NAME}}_InitStmt();
	Model_{{MODEL_NAME}}_RegisterRoutes();
}

void Model_{{MODEL_NAME}}_Unit()
{
	printf("    [Model] Unit {{MODEL_NAME}}...\n");
	Model_{{MODEL_NAME}}_UnregisterRoutes();
	Model_{{MODEL_NAME}}_FreeStmt();
}
