
// ============================================
// 模型: 新闻 (new)
// 自动生成代码 - 请勿手动修改
// 生成时间: 2026-01-11 22:53:54
// TCC独立状态机编译
// ============================================

// 引入 xserver 基础库
#include <xsbase.h>

// 全局变量（通过 Model_SetGlobalData 传入）
XDO_Connect G_DB;

// 模型管理器特有函数声明
// 注意：Model_AddRoute 实际返回 RouteInfo*，但模型代码不需要使用返回值
extern void* Model_AddRoute(str, void*, bool, bool, int, int);
extern void Model_RemoveRoute(str);

// HTTP 响应常量
#define HTTP_CT_JSON "Content-Type: application/json\r\n"

// 接收主系统传递的全局数据
void Model_SetGlobalData(int idx, void* ptr)
{
	if ( idx == 1 ) {
		G_DB = (XDO_Connect)ptr;
	}
}

// ==================== 模型代码 ====================

// 预编译语句
sqlite3_stmt* stmt_new_all;
sqlite3_stmt* stmt_new_get;
sqlite3_stmt* stmt_new_add;
sqlite3_stmt* stmt_new_put;
sqlite3_stmt* stmt_new_del;
sqlite3_stmt* stmt_new_count;



/* REPLY DISABLED

// 评论预编译语句
sqlite3_stmt* stmt_new_reply_list;
sqlite3_stmt* stmt_new_reply_add;
sqlite3_stmt* stmt_new_reply_del;
sqlite3_stmt* stmt_new_reply_count;
sqlite3_stmt* stmt_new_reply_get;
*/


// 草稿预编译语句
sqlite3_stmt* stmt_new_draft_all;
sqlite3_stmt* stmt_new_draft_get;
sqlite3_stmt* stmt_new_draft_add;
sqlite3_stmt* stmt_new_draft_put;
sqlite3_stmt* stmt_new_draft_del;
sqlite3_stmt* stmt_new_draft_count;
sqlite3_stmt* stmt_new_draft_publish;
sqlite3_stmt* stmt_new_draft_clear;


// 初始化预编译语句
void Model_new_InitStmt()
{
	sqlite3* db = G_DB->objDB;
	
	// 分页获取列表
	sqlite3_prepare_v3(db,

		"SELECT id, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, authorType, authorId, authorName, createTime, updateTime FROM model_new_new "

		"WHERE isDelete = 0 ORDER BY id DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt_new_all, NULL);
	
	// 根据ID获取单条记录
	sqlite3_prepare_v3(db,

		"SELECT id, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, authorType, authorId, authorName, createTime, updateTime FROM model_new_new WHERE id = ? AND isDelete = 0",

		-1, 0, &stmt_new_get, NULL);
	
	// 添加记录
	sqlite3_prepare_v3(db,

		"INSERT INTO model_new_new (a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, authorType, authorId, authorName, createTime, updateTime, isDelete) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)",

		-1, 0, &stmt_new_add, NULL);
	
	// 更新记录（不更新作者字段，保留原作者信息）
	sqlite3_prepare_v3(db,

		"UPDATE model_new_new SET a1 = ?, a2 = ?, a3 = ?, a4 = ?, a5 = ?, a6 = ?, a7 = ?, a8 = ?, a9 = ?, a10 = ?, a11 = ?, a12 = ?, a13 = ?, a14 = ?, a15 = ?, a16 = ?, a17 = ?, a18 = ?, a19 = ?, updateTime = ? WHERE id = ?",

		-1, 0, &stmt_new_put, NULL);
	
	// 删除记录（软删除）
	sqlite3_prepare_v3(db,
		"UPDATE model_new_new SET isDelete = 1, updateTime = ? WHERE id = ?",
		-1, 0, &stmt_new_del, NULL);
	
	// 统计总数
	sqlite3_prepare_v3(db,
		"SELECT COUNT(*) FROM model_new_new WHERE isDelete = 0",
		-1, 0, &stmt_new_count, NULL);
	

	
/* REPLY DISABLED

	// 评论列表（根据内容ID）
	sqlite3_prepare_v3(db,
		"SELECT id, userId, userType, content, quoteId, quoteText, status, createTime "
		"FROM reply WHERE modelName = 'new' AND contentId = ? AND isDelete = 0 "
		"ORDER BY id DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt_new_reply_list, NULL);
	
	// 获取单条评论
	sqlite3_prepare_v3(db,
		"SELECT id, userId, userType, content, quoteId, quoteText, status, createTime "
		"FROM reply WHERE id = ? AND isDelete = 0",
		-1, 0, &stmt_new_reply_get, NULL);
	
	// 添加评论
	sqlite3_prepare_v3(db,
		"INSERT INTO reply (modelName, contentId, userId, userType, content, quoteId, quoteText, status, createTime, updateTime, isDelete) "
		"VALUES ('new', ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)",
		-1, 0, &stmt_new_reply_add, NULL);
	
	// 删除评论（软删除）
	sqlite3_prepare_v3(db,
		"UPDATE reply SET isDelete = 1, updateTime = ? WHERE id = ?",
		-1, 0, &stmt_new_reply_del, NULL);
	
	// 评论总数
	sqlite3_prepare_v3(db,
		"SELECT COUNT(*) FROM reply WHERE modelName = 'new' AND contentId = ? AND isDelete = 0",
		-1, 0, &stmt_new_reply_count, NULL);
*/


	// 草稿列表
	sqlite3_prepare_v3(db,
		"SELECT id, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, authorType, authorId, authorName, createTime, updateTime FROM model_new_new_draft "
		"ORDER BY id DESC LIMIT ? OFFSET ?",
		-1, 0, &stmt_new_draft_all, NULL);
	
	// 获取单条草稿
	sqlite3_prepare_v3(db,
		"SELECT id, a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, authorType, authorId, authorName, createTime, updateTime FROM model_new_new_draft WHERE id = ?",
		-1, 0, &stmt_new_draft_get, NULL);
	
	// 添加草稿
	sqlite3_prepare_v3(db,
		"INSERT INTO model_new_new_draft (a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12, a13, a14, a15, a16, a17, a18, a19, authorType, authorId, authorName, createTime, updateTime) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
		-1, 0, &stmt_new_draft_add, NULL);
	
	// 更新草稿
	sqlite3_prepare_v3(db,
		"UPDATE model_new_new_draft SET a1 = ?, a2 = ?, a3 = ?, a4 = ?, a5 = ?, a6 = ?, a7 = ?, a8 = ?, a9 = ?, a10 = ?, a11 = ?, a12 = ?, a13 = ?, a14 = ?, a15 = ?, a16 = ?, a17 = ?, a18 = ?, a19 = ?, authorType = ?, authorId = ?, authorName = ?, updateTime = ? WHERE id = ?",
		-1, 0, &stmt_new_draft_put, NULL);
	
	// 删除草稿（硬删除）
	sqlite3_prepare_v3(db,
		"DELETE FROM model_new_new_draft WHERE id = ?",
		-1, 0, &stmt_new_draft_del, NULL);
	
	// 草稿总数
	sqlite3_prepare_v3(db,
		"SELECT COUNT(*) FROM model_new_new_draft",
		-1, 0, &stmt_new_draft_count, NULL);
	
	// 发布草稿（插入主表后删除草稿）- 只删除草稿记录
	sqlite3_prepare_v3(db,
		"DELETE FROM model_new_new_draft WHERE id = ?",
		-1, 0, &stmt_new_draft_publish, NULL);
	
	// 清空草稿箱
	sqlite3_prepare_v3(db,
		"DELETE FROM model_new_new_draft",
		-1, 0, &stmt_new_draft_clear, NULL);

}

// 销毁预编译语句
void Model_new_FreeStmt()
{
	if ( stmt_new_all ) sqlite3_finalize(stmt_new_all);
	if ( stmt_new_get ) sqlite3_finalize(stmt_new_get);
	if ( stmt_new_add ) sqlite3_finalize(stmt_new_add);
	if ( stmt_new_put ) sqlite3_finalize(stmt_new_put);
	if ( stmt_new_del ) sqlite3_finalize(stmt_new_del);
	if ( stmt_new_count ) sqlite3_finalize(stmt_new_count);

/* REPLY DISABLED

	if ( stmt_new_reply_list ) sqlite3_finalize(stmt_new_reply_list);
	if ( stmt_new_reply_get ) sqlite3_finalize(stmt_new_reply_get);
	if ( stmt_new_reply_add ) sqlite3_finalize(stmt_new_reply_add);
	if ( stmt_new_reply_del ) sqlite3_finalize(stmt_new_reply_del);
	if ( stmt_new_reply_count ) sqlite3_finalize(stmt_new_reply_count);
*/

	if ( stmt_new_draft_all ) sqlite3_finalize(stmt_new_draft_all);
	if ( stmt_new_draft_get ) sqlite3_finalize(stmt_new_draft_get);
	if ( stmt_new_draft_add ) sqlite3_finalize(stmt_new_draft_add);
	if ( stmt_new_draft_put ) sqlite3_finalize(stmt_new_draft_put);
	if ( stmt_new_draft_del ) sqlite3_finalize(stmt_new_draft_del);
	if ( stmt_new_draft_count ) sqlite3_finalize(stmt_new_draft_count);
	if ( stmt_new_draft_publish ) sqlite3_finalize(stmt_new_draft_publish);
	if ( stmt_new_draft_clear ) sqlite3_finalize(stmt_new_draft_clear);

}




// ==================== 前台 API ====================

// 获取列表
void Api_new_List(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_GET ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	char sParam[64];
	mg_http_get_var(&hm->query, "page", sParam, sizeof(sParam));
	int64 iPage = xrtStrToI64(sParam);
	if ( iPage <= 0 ) iPage = 1;
	mg_http_get_var(&hm->query, "limit", sParam, sizeof(sParam));
	int64 iLimit = xrtStrToI64(sParam);
	if ( iLimit <= 0 ) iLimit = 20;
	if ( iLimit > 100 ) iLimit = 100;
	int64 iOffset = (iPage - 1) * iLimit;
	
	// 获取总数
	int64 iCount = 0;
	if ( sqlite3_step(stmt_new_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_new_count, 0);
	}
	sqlite3_reset(stmt_new_count);
	
	// 获取数据
	xvalue arrData = xvoCreateArray();
	sqlite3_bind_int64(stmt_new_all, 1, iLimit);
	sqlite3_bind_int64(stmt_new_all, 2, iOffset);
	while ( sqlite3_step(stmt_new_all) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_new_all, iCol++));
		xvoTableSetText(tblRow, "a1", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a2", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a3", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a4", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a5", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a6", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a7", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a8", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a9", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a10", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a11", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a12", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a13", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a14", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a15", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a16", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a17", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a18", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a19", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);

		xtime iTime = sqlite3_column_int64(stmt_new_all, iCol++);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_new_all, iCol++);
		xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_reset(stmt_new_all);
	
	// 构建响应
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}

// 获取详情
void Api_new_Get(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sID[24];
	mg_http_get_var(&hm->query, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	

	
	xvalue tblData = NULL;
	sqlite3_bind_int64(stmt_new_get, 1, iID);
	if ( sqlite3_step(stmt_new_get) == SQLITE_ROW ) {
		tblData = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblData, "id", 2, sqlite3_column_int64(stmt_new_get, iCol++));
		xvoTableSetText(tblData, "a1", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a2", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a3", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a4", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a5", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a6", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a7", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a8", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a9", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a10", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a11", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a12", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a13", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a14", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a15", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a16", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a17", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a18", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a19", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);

		xtime iTime = sqlite3_column_int64(stmt_new_get, iCol++);
		xvoTableSetText(tblData, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_new_get, iCol++);
		xvoTableSetText(tblData, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
	}
	sqlite3_reset(stmt_new_get);
	
	if ( !tblData ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Not found\"}", 0);
		return;
	}
	
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}




/* SUBMIT DISABLED

// ==================== 前台投稿 ====================

// 投稿内容
void Api_new_Submit(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		if ( tblForm ) xvoUnref(tblForm);
		return;
	}
	
	// 获取当前用户信息
	int64 iMemberId = 0;
	str sMemberNickname = NULL;
	if ( hm->session ) {
		iMemberId = xvoTableGetInt(hm->session, "id", 2);
		sMemberNickname = xvoTableGetText(hm->session, "nickname", 8);
	}
	
	// 检查是否允许游客投稿
	int iAllowGuest = 0;
	if ( (iMemberId <= 0) && !iAllowGuest ) {
		xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Please login first\"}", 0);
		return;
	}
	
	// 决定数据去向：游客始终进草稿，会员根据配置决定
	int iNeedReview = 0;
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
	{ str sVal = xvoTableGetText(tblForm, "a1", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a2", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a3", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a4", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a5", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a6", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a7", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a8", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a9", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a10", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a11", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a12", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a13", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a14", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a15", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a16", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a17", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a18", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a19", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }

		sqlite3_bind_int64(stmt_new_draft_add, iIdx++, iAuthorType);
		sqlite3_bind_int64(stmt_new_draft_add, iIdx++, iAuthorId);
		sqlite3_bind_text(stmt_new_draft_add, iIdx++, sAuthorName, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_new_draft_add, iIdx++, now);
		sqlite3_bind_int64(stmt_new_draft_add, iIdx++, now);
		sqlite3_step(stmt_new_draft_add);
		newId = sqlite3_last_insert_rowid(G_DB->objDB);
		sqlite3_reset(stmt_new_draft_add);
	} else {
		// 直接发布
	{ str sVal = xvoTableGetText(tblForm, "a1", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a2", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a3", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a4", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a5", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a6", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a7", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a8", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a9", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a10", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a11", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a12", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a13", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a14", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a15", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a16", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a17", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a18", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a19", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }


		// 作者字段
		sqlite3_bind_int64(stmt_new_add, iIdx++, iAuthorType);
		sqlite3_bind_int64(stmt_new_add, iIdx++, iAuthorId);
		sqlite3_bind_text(stmt_new_add, iIdx++, sAuthorName, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_new_add, iIdx++, now);
		sqlite3_bind_int64(stmt_new_add, iIdx++, now);
		sqlite3_step(stmt_new_add);
		newId = sqlite3_last_insert_rowid(G_DB->objDB);
		sqlite3_reset(stmt_new_add);
	}
	
	xvoUnref(tblForm);
	
	if ( bToDraft ) {
		mg_http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Submitted to review\", \"data\": {\"id\": %lld, \"isDraft\": true}}", newId);
	} else {
		mg_http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Published\", \"data\": {\"id\": %lld, \"isDraft\": false}}", newId);
	}
}
*/



/* REPLY DISABLED

// ==================== 评论 API ====================

// 获取评论列表
void Api_new_Reply_List(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sParam[64];
	mg_http_get_var(&hm->query, "contentId", sParam, sizeof(sParam));
	int64 iContentId = xrtStrToI64(sParam);
	if ( iContentId <= 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing contentId\"}", 0);
		return;
	}
	
	mg_http_get_var(&hm->query, "page", sParam, sizeof(sParam));
	int64 iPage = xrtStrToI64(sParam);
	if ( iPage <= 0 ) iPage = 1;
	mg_http_get_var(&hm->query, "limit", sParam, sizeof(sParam));
	int64 iLimit = xrtStrToI64(sParam);
	if ( iLimit <= 0 ) iLimit = 20;
	if ( iLimit > 100 ) iLimit = 100;
	int64 iOffset = (iPage - 1) * iLimit;
	
	// 获取总数
	int64 iCount = 0;
	sqlite3_bind_int64(stmt_new_reply_count, 1, iContentId);
	if ( sqlite3_step(stmt_new_reply_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_new_reply_count, 0);
	}
	sqlite3_reset(stmt_new_reply_count);
	
	// 获取评论列表
	xvalue arrData = xvoCreateArray();
	sqlite3_bind_int64(stmt_new_reply_list, 1, iContentId);
	sqlite3_bind_int64(stmt_new_reply_list, 2, iLimit);
	sqlite3_bind_int64(stmt_new_reply_list, 3, iOffset);
	while ( sqlite3_step(stmt_new_reply_list) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_new_reply_list, iCol++));
		xvoTableSetInt(tblRow, "userId", 6, sqlite3_column_int64(stmt_new_reply_list, iCol++));
		xvoTableSetInt(tblRow, "userType", 8, sqlite3_column_int64(stmt_new_reply_list, iCol++));
		xvoTableSetText(tblRow, "content", 7, (str)sqlite3_column_text(stmt_new_reply_list, iCol++), 0, FALSE);
		xvoTableSetInt(tblRow, "quoteId", 7, sqlite3_column_int64(stmt_new_reply_list, iCol++));
		xvoTableSetText(tblRow, "quoteText", 9, (str)sqlite3_column_text(stmt_new_reply_list, iCol++), 0, FALSE);
		xvoTableSetInt(tblRow, "status", 6, sqlite3_column_int64(stmt_new_reply_list, iCol++));
		xtime iTime = sqlite3_column_int64(stmt_new_reply_list, iCol++);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_reset(stmt_new_reply_list);
	
	// 构建响应
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}

// 添加评论
void Api_new_Reply_Add(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		if ( tblForm ) xvoUnref(tblForm);
		return;
	}
	
	int64 iContentId = xvoTableGetInt(tblForm, "contentId", 9);
	if ( iContentId <= 0 ) {
		xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing contentId\"}", 0);
		return;
	}
	
	str sContent = xvoTableGetText(tblForm, "content", 7);
	if ( !sContent || strlen(sContent) == 0 ) {
		xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Content is required\"}", 0);
		return;
	}
	
	// 获取引用信息
	int64 iQuoteId = xvoTableGetInt(tblForm, "quoteId", 7);
	str sQuoteText = "";
	bool bFreeQuote = FALSE;
	if ( iQuoteId > 0 ) {
		// 获取被引用评论的内容摘要
		sqlite3_bind_int64(stmt_new_reply_get, 1, iQuoteId);
		if ( sqlite3_step(stmt_new_reply_get) == SQLITE_ROW ) {
			str sOrigContent = (str)sqlite3_column_text(stmt_new_reply_get, 3);  // content 在第4列
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
		sqlite3_reset(stmt_new_reply_get);
	}
	
	// 获取用户信息（从 session 中获取，这里简化处理）
	int64 iUserId = xvoTableGetInt(tblForm, "userId", 6);
	int64 iUserType = xvoTableGetInt(tblForm, "userType", 8);  // 0=会员, 1=后台用户
	
	// 状态：是否需要审核
	int64 iStatus = {{REPLY_NEED_APPROVE}} ? 0 : 1;  // 0=待审核, 1=已发布
	
	xtime now = xrtNow();
	int iIdx = 1;
	sqlite3_bind_int64(stmt_new_reply_add, iIdx++, iContentId);
	sqlite3_bind_int64(stmt_new_reply_add, iIdx++, iUserId);
	sqlite3_bind_int64(stmt_new_reply_add, iIdx++, iUserType);
	sqlite3_bind_text(stmt_new_reply_add, iIdx++, sContent, -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt_new_reply_add, iIdx++, iQuoteId);
	sqlite3_bind_text(stmt_new_reply_add, iIdx++, sQuoteText, -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt_new_reply_add, iIdx++, iStatus);
	sqlite3_bind_int64(stmt_new_reply_add, iIdx++, now);
	sqlite3_bind_int64(stmt_new_reply_add, iIdx++, now);
	sqlite3_step(stmt_new_reply_add);
	int64 newId = sqlite3_last_insert_rowid(G_DB->objDB);
	sqlite3_reset(stmt_new_reply_add);
	
	// 清理临时内存
	if ( bFreeQuote ) {
		xrtFree(sQuoteText);
	}
	xvoUnref(tblForm);
	
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"id\": %lld}}", newId);
}

// 删除评论（需要校验权限）
void Api_new_Reply_Delete(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sParam[24];
	mg_http_get_var(&hm->query, "id", sParam, sizeof(sParam));
	int64 iID = xrtStrToI64(sParam);
	if ( iID <= 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	// TODO: 应该校验当前用户是否有权删除该评论
	
	xtime now = xrtNow();
	sqlite3_bind_int64(stmt_new_reply_del, 1, now);
	sqlite3_bind_int64(stmt_new_reply_del, 2, iID);
	sqlite3_step(stmt_new_reply_del);
	sqlite3_reset(stmt_new_reply_del);
	
	http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}
*/




// ==================== 后台管理 API ====================

// 后台列表
void Admin_new_List(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_GET ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	char sParam[64];
	mg_http_get_var(&hm->query, "page", sParam, sizeof(sParam));
	int64 iPage = xrtStrToI64(sParam);
	if ( iPage <= 0 ) iPage = 1;
	mg_http_get_var(&hm->query, "limit", sParam, sizeof(sParam));
	int64 iLimit = xrtStrToI64(sParam);
	if ( iLimit <= 0 ) iLimit = 20;
	int64 iOffset = (iPage - 1) * iLimit;
	
	// 获取总数
	int64 iCount = 0;
	if ( sqlite3_step(stmt_new_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_new_count, 0);
	}
	sqlite3_reset(stmt_new_count);
	
	// 获取数据
	xvalue arrData = xvoCreateArray();
	sqlite3_bind_int64(stmt_new_all, 1, iLimit);
	sqlite3_bind_int64(stmt_new_all, 2, iOffset);
	while ( sqlite3_step(stmt_new_all) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_new_all, iCol++));
		xvoTableSetText(tblRow, "a1", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a2", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a3", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a4", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a5", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a6", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a7", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a8", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a9", 2, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a10", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a11", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a12", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a13", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a14", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a15", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a16", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a17", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a18", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a19", 3, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);


		// 作者字段
		xvoTableSetInt(tblRow, "authorType", 10, sqlite3_column_int64(stmt_new_all, iCol++));
		xvoTableSetInt(tblRow, "authorId", 8, sqlite3_column_int64(stmt_new_all, iCol++));
		xvoTableSetText(tblRow, "authorName", 10, (str)sqlite3_column_text(stmt_new_all, iCol++), 0, FALSE);
		xtime iTime = sqlite3_column_int64(stmt_new_all, iCol++);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_new_all, iCol++);
		xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_reset(stmt_new_all);
	
	// layui table 格式响应
	xvalue tblRet = xvoCreateTable();
	xvoTableSetInt(tblRet, "code", 4, 0);
	xvoTableSetText(tblRet, "msg", 3, "", 0, FALSE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}

// 后台获取单条
void Admin_new_Get(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sID[24];
	mg_http_get_var(&hm->query, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xvalue tblData = NULL;
	sqlite3_bind_int64(stmt_new_get, 1, iID);
	if ( sqlite3_step(stmt_new_get) == SQLITE_ROW ) {
		tblData = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblData, "id", 2, sqlite3_column_int64(stmt_new_get, iCol++));
		xvoTableSetText(tblData, "a1", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a2", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a3", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a4", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a5", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a6", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a7", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a8", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a9", 2, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a10", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a11", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a12", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a13", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a14", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a15", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a16", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a17", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a18", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a19", 3, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);


		// 作者字段
		xvoTableSetInt(tblData, "authorType", 10, sqlite3_column_int64(stmt_new_get, iCol++));
		xvoTableSetInt(tblData, "authorId", 8, sqlite3_column_int64(stmt_new_get, iCol++));
		xvoTableSetText(tblData, "authorName", 10, (str)sqlite3_column_text(stmt_new_get, iCol++), 0, FALSE);
		xtime iTime = sqlite3_column_int64(stmt_new_get, iCol++);
		xvoTableSetText(tblData, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_new_get, iCol++);
		xvoTableSetText(tblData, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
	}
	sqlite3_reset(stmt_new_get);
	
	if ( !tblData ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Not found\"}", 0);
		return;
	}
	
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}

// 后台添加
void Admin_new_Add(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		if ( tblForm ) xvoUnref(tblForm);
		return;
	}
	
	// 检查是否保存为草稿
	int bIsDraft = xvoTableGetInt(tblForm, "isDraft", 7);
	

	if ( bIsDraft ) {
		// 保存到草稿箱
		xtime now = xrtNow();
		int iIdx = 1;
		{ str sVal = xvoTableGetText(tblForm, "a1", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a2", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a3", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a4", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a5", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a6", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a7", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a8", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a9", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a10", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a11", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a12", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a13", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a14", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a15", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a16", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a17", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a18", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a19", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }

		// 作者字段（优先使用表单中的authorName，否则使用当前管理员）
		int64 iAdminId = xvoTableGetInt(hm->session, "id", 2);
		str sAdminUser = xvoTableGetText(hm->session, "user", 4);
		str sFormAuthor = xvoTableGetText(tblForm, "authorName", 10);
		str sAuthorName = (sFormAuthor && strlen(sFormAuthor) > 0) ? sFormAuthor : (sAdminUser ? sAdminUser : (str)"");
		sqlite3_bind_int64(stmt_new_draft_add, iIdx++, 0);  // authorType = 0 (后台管理员)
		sqlite3_bind_int64(stmt_new_draft_add, iIdx++, iAdminId);
		sqlite3_bind_text(stmt_new_draft_add, iIdx++, sAuthorName, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_new_draft_add, iIdx++, now);
		sqlite3_bind_int64(stmt_new_draft_add, iIdx++, now);
		sqlite3_step(stmt_new_draft_add);
		int64 newId = sqlite3_last_insert_rowid(G_DB->objDB);
		sqlite3_reset(stmt_new_draft_add);
		xvoUnref(tblForm);
		
		mg_http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"已保存到草稿箱\", \"data\": {\"id\": %lld}}", newId);
		return;
	}

	
	// 正常发布
	xtime now = xrtNow();
	int iIdx = 1;
	{ str sVal = xvoTableGetText(tblForm, "a1", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a2", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a3", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a4", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a5", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a6", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a7", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a8", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a9", 2); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a10", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a11", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a12", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a13", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a14", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a15", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a16", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a17", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a18", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a19", 3); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }


	// 作者字段（优先使用表单中的authorName，否则使用当前管理员）
	int64 iAdminId = xvoTableGetInt(hm->session, "id", 2);
	str sAdminUser = xvoTableGetText(hm->session, "user", 4);
	str sFormAuthor = xvoTableGetText(tblForm, "authorName", 10);
	str sAuthorName = (sFormAuthor && strlen(sFormAuthor) > 0) ? sFormAuthor : (sAdminUser ? sAdminUser : (str)"");
	sqlite3_bind_int64(stmt_new_add, iIdx++, 0);  // authorType = 0 (后台管理员)
	sqlite3_bind_int64(stmt_new_add, iIdx++, iAdminId);
	sqlite3_bind_text(stmt_new_add, iIdx++, sAuthorName, -1, SQLITE_STATIC);
	sqlite3_bind_int64(stmt_new_add, iIdx++, now);
	sqlite3_bind_int64(stmt_new_add, iIdx++, now);
	sqlite3_step(stmt_new_add);
	int64 newId = sqlite3_last_insert_rowid(G_DB->objDB);
	sqlite3_reset(stmt_new_add);
	xvoUnref(tblForm);
	
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"id\": %lld}}", newId);
}

// 后台更新
void Admin_new_Save(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		if ( tblForm ) xvoUnref(tblForm);
		return;
	}
	
	int64 iID = xvoTableGetInt(tblForm, "id", 2);
	if ( iID <= 0 ) {
		xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xtime now = xrtNow();
	int iIdx = 1;
	{ str sVal = xvoTableGetText(tblForm, "a1", 2); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a2", 2); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a3", 2); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a4", 2); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a5", 2); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a6", 2); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a7", 2); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a8", 2); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a9", 2); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a10", 3); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a11", 3); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a12", 3); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a13", 3); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a14", 3); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a15", 3); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a16", 3); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a17", 3); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a18", 3); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a19", 3); sqlite3_bind_text(stmt_new_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }


	// 不更新作者字段，保留原作者信息
	sqlite3_bind_int64(stmt_new_put, iIdx++, now);
	sqlite3_bind_int64(stmt_new_put, iIdx++, iID);
	sqlite3_step(stmt_new_put);
	sqlite3_reset(stmt_new_put);
	xvoUnref(tblForm);
	
	http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}

// 后台删除
void Admin_new_Delete(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sID[24];
	mg_http_get_var(&hm->query, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xtime now = xrtNow();
	sqlite3_bind_int64(stmt_new_del, 1, now);
	sqlite3_bind_int64(stmt_new_del, 2, iID);
	sqlite3_step(stmt_new_del);
	sqlite3_reset(stmt_new_del);
	
	http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}





// ==================== 草稿管理 API ====================

// 草稿列表
void Admin_new_Draft_List(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_GET ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	char sParam[64];
	mg_http_get_var(&hm->query, "page", sParam, sizeof(sParam));
	int64 iPage = xrtStrToI64(sParam);
	if ( iPage <= 0 ) iPage = 1;
	mg_http_get_var(&hm->query, "limit", sParam, sizeof(sParam));
	int64 iLimit = xrtStrToI64(sParam);
	if ( iLimit <= 0 ) iLimit = 20;
	int64 iOffset = (iPage - 1) * iLimit;
	
	// 获取总数
	int64 iCount = 0;
	if ( sqlite3_step(stmt_new_draft_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_new_draft_count, 0);
	}
	sqlite3_reset(stmt_new_draft_count);
	
	// 获取数据
	xvalue arrData = xvoCreateArray();
	sqlite3_bind_int64(stmt_new_draft_all, 1, iLimit);
	sqlite3_bind_int64(stmt_new_draft_all, 2, iOffset);
	while ( sqlite3_step(stmt_new_draft_all) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblRow, "id", 2, sqlite3_column_int64(stmt_new_draft_all, iCol++));
		xvoTableSetText(tblRow, "a1", 2, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a2", 2, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a3", 2, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a4", 2, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a5", 2, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a6", 2, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a7", 2, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a8", 2, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a9", 2, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a10", 3, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a11", 3, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a12", 3, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a13", 3, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a14", 3, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a15", 3, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a16", 3, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a17", 3, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a18", 3, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xvoTableSetText(tblRow, "a19", 3, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);

		xvoTableSetInt(tblRow, "authorType", 10, sqlite3_column_int64(stmt_new_draft_all, iCol++));
		xvoTableSetInt(tblRow, "authorId", 8, sqlite3_column_int64(stmt_new_draft_all, iCol++));
		xvoTableSetText(tblRow, "authorName", 10, (str)sqlite3_column_text(stmt_new_draft_all, iCol++), 0, FALSE);
		xtime iTime = sqlite3_column_int64(stmt_new_draft_all, iCol++);
		xvoTableSetText(tblRow, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_new_draft_all, iCol++);
		xvoTableSetText(tblRow, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		xvoArrayAppendValue(arrData, tblRow, TRUE);
	}
	sqlite3_reset(stmt_new_draft_all);
	
	// layui table 格式响应
	xvalue tblRet = xvoCreateTable();
	xvoTableSetInt(tblRet, "code", 4, 0);
	xvoTableSetText(tblRet, "msg", 3, "", 0, FALSE);
	xvoTableSetInt(tblRet, "count", 5, iCount);
	xvoTableSetValue(tblRet, "data", 4, arrData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}

// 获取单条草稿
void Admin_new_Draft_Get(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sID[24];
	mg_http_get_var(&hm->query, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	xvalue tblData = NULL;
	sqlite3_bind_int64(stmt_new_draft_get, 1, iID);
	if ( sqlite3_step(stmt_new_draft_get) == SQLITE_ROW ) {
		tblData = xvoCreateTable();
		int iCol = 0;
		xvoTableSetInt(tblData, "id", 2, sqlite3_column_int64(stmt_new_draft_get, iCol++));
		xvoTableSetText(tblData, "a1", 2, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a2", 2, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a3", 2, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a4", 2, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a5", 2, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a6", 2, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a7", 2, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a8", 2, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a9", 2, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a10", 3, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a11", 3, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a12", 3, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a13", 3, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a14", 3, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a15", 3, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a16", 3, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a17", 3, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a18", 3, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xvoTableSetText(tblData, "a19", 3, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);

		xvoTableSetInt(tblData, "authorType", 10, sqlite3_column_int64(stmt_new_draft_get, iCol++));
		xvoTableSetInt(tblData, "authorId", 8, sqlite3_column_int64(stmt_new_draft_get, iCol++));
		xvoTableSetText(tblData, "authorName", 10, (str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0, FALSE);
		xtime iTime = sqlite3_column_int64(stmt_new_draft_get, iCol++);
		xvoTableSetText(tblData, "createTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
		iTime = sqlite3_column_int64(stmt_new_draft_get, iCol++);
		xvoTableSetText(tblData, "updateTime", 10, xrtTimeToStr(iTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
	}
	sqlite3_reset(stmt_new_draft_get);
	
	if ( !tblData ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Not found\"}", 0);
		return;
	}
	
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}

// 保存草稿
void Admin_new_Draft_Save(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		if ( tblForm ) xvoUnref(tblForm);
		return;
	}
	
	int64 iID = xvoTableGetInt(tblForm, "id", 2);
	xtime now = xrtNow();
	
	if ( iID > 0 ) {
		// 更新草稿
		int iIdx = 1;
	{ str sVal = xvoTableGetText(tblForm, "a1", 2); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a2", 2); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a3", 2); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a4", 2); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a5", 2); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a6", 2); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a7", 2); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a8", 2); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a9", 2); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a10", 3); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a11", 3); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a12", 3); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a13", 3); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a14", 3); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a15", 3); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a16", 3); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a17", 3); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a18", 3); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = xvoTableGetText(tblForm, "a19", 3); sqlite3_bind_text(stmt_new_draft_put, iIdx++, sVal ? sVal : (str)"", -1, NULL); }

		// 作者字段（优先使用表单中的authorName，否则使用当前管理员）
		int64 iEditorId = xvoTableGetInt(hm->session, "id", 2);
		str sEditorUser = xvoTableGetText(hm->session, "user", 4);
		str sFormAuthor = xvoTableGetText(tblForm, "authorName", 10);
		str sAuthorName = (sFormAuthor && strlen(sFormAuthor) > 0) ? sFormAuthor : (sEditorUser ? sEditorUser : (str)"");
		sqlite3_bind_int64(stmt_new_draft_put, iIdx++, 0);  // authorType = 0 (后台管理员)
		sqlite3_bind_int64(stmt_new_draft_put, iIdx++, iEditorId);
		sqlite3_bind_text(stmt_new_draft_put, iIdx++, sAuthorName, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_new_draft_put, iIdx++, now);
		sqlite3_bind_int64(stmt_new_draft_put, iIdx++, iID);
		sqlite3_step(stmt_new_draft_put);
		sqlite3_reset(stmt_new_draft_put);
	} else {
		// 添加草稿
		int iIdx = 1;
		{ str sVal = xvoTableGetText(tblForm, "a1", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a2", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a3", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a4", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a5", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a6", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a7", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a8", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a9", 2); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a10", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a11", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a12", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a13", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a14", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a15", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a16", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a17", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a18", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
		{ str sVal = xvoTableGetText(tblForm, "a19", 3); sqlite3_bind_text(stmt_new_draft_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }

		// 作者字段（优先使用表单中的authorName，否则使用当前管理员）
		int64 iAdminId = xvoTableGetInt(hm->session, "id", 2);
		str sAdminUser = xvoTableGetText(hm->session, "user", 4);
		str sFormAuthor = xvoTableGetText(tblForm, "authorName", 10);
		str sAuthorName = (sFormAuthor && strlen(sFormAuthor) > 0) ? sFormAuthor : (sAdminUser ? sAdminUser : (str)"");
		sqlite3_bind_int64(stmt_new_draft_add, iIdx++, 0);  // authorType = 0 (后台管理员)
		sqlite3_bind_int64(stmt_new_draft_add, iIdx++, iAdminId);
		sqlite3_bind_text(stmt_new_draft_add, iIdx++, sAuthorName, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_new_draft_add, iIdx++, now);
		sqlite3_bind_int64(stmt_new_draft_add, iIdx++, now);
		sqlite3_step(stmt_new_draft_add);
		iID = sqlite3_last_insert_rowid(G_DB->objDB);
		sqlite3_reset(stmt_new_draft_add);
	}
	
	xvoUnref(tblForm);
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"id\": %lld}}", iID);
}

// 删除草稿
void Admin_new_Draft_Delete(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sID[24];
	mg_http_get_var(&hm->query, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	sqlite3_bind_int64(stmt_new_draft_del, 1, iID);
	sqlite3_step(stmt_new_draft_del);
	sqlite3_reset(stmt_new_draft_del);
	
	http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}

// 发布草稿
void Admin_new_Draft_Publish(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sID[24];
	mg_http_get_var(&hm->query, "id", sID, sizeof(sID));
	int64 iID = xrtStrToI64(sID);
	if ( iID <= 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ID\"}", 0);
		return;
	}
	
	// 获取草稿数据
	sqlite3_bind_int64(stmt_new_draft_get, 1, iID);
	if ( sqlite3_step(stmt_new_draft_get) != SQLITE_ROW ) {
		sqlite3_reset(stmt_new_draft_get);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Draft not found\"}", 0);
		return;
	}
	
	// 提取草稿数据
	xtime now = xrtNow();
	int iIdx = 1;
	int iCol = 1;  // 跳过 id
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }

	// 作者信息
	int64 iAuthorType = sqlite3_column_int64(stmt_new_draft_get, iCol++);
	int64 iAuthorId = sqlite3_column_int64(stmt_new_draft_get, iCol++);
	str sAuthorName = xrtCopyStr((str)sqlite3_column_text(stmt_new_draft_get, iCol++), 0);
	sqlite3_reset(stmt_new_draft_get);
	

	// 绑定作者信息到主表插入语句
	sqlite3_bind_int64(stmt_new_add, iIdx++, iAuthorType);
	sqlite3_bind_int64(stmt_new_add, iIdx++, iAuthorId);
	sqlite3_bind_text(stmt_new_add, iIdx++, sAuthorName ? sAuthorName : (str)"", -1, SQLITE_STATIC);
	
	// 插入主表
	sqlite3_bind_int64(stmt_new_add, iIdx++, now);
	sqlite3_bind_int64(stmt_new_add, iIdx++, now);
	sqlite3_step(stmt_new_add);
	int64 newId = sqlite3_last_insert_rowid(G_DB->objDB);
	sqlite3_reset(stmt_new_add);
	
	// 删除草稿
	sqlite3_bind_int64(stmt_new_draft_publish, 1, iID);
	sqlite3_step(stmt_new_draft_publish);
	sqlite3_reset(stmt_new_draft_publish);
	
	if ( sAuthorName ) xrtFree(sAuthorName);
	
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"id\": %lld}}", newId);
}

// 批量发布草稿
void Admin_new_Draft_BatchPublish(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		if ( tblForm ) xvoUnref(tblForm);
		return;
	}
	
	xvalue arrIds = xvoTableGetValue(tblForm, "ids", 3);
	if ( !arrIds || arrIds->Type != XVO_DT_ARRAY ) {
		xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ids\"}", 0);
		return;
	}
	
	int iCount = xvoArrayItemCount(arrIds);
	int iSuccess = 0;
	xtime now = xrtNow();
	
	for ( int i = 0; i < iCount; i++ ) {
		int64 iID = xvoArrayGetInt(arrIds, i);
		if ( iID <= 0 ) continue;
		
		// 获取草稿数据
		sqlite3_bind_int64(stmt_new_draft_get, 1, iID);
		if ( sqlite3_step(stmt_new_draft_get) == SQLITE_ROW ) {
			int iIdx = 1;
			int iCol = 1;  // 跳过 id
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }
	{ str sVal = (str)sqlite3_column_text(stmt_new_draft_get, iCol++); sqlite3_bind_text(stmt_new_add, iIdx++, sVal ? sVal : (str)"", -1, NULL); }

			// 读取作者信息
			int64 iAuthorType = sqlite3_column_int64(stmt_new_draft_get, iCol++);
			int64 iAuthorId = sqlite3_column_int64(stmt_new_draft_get, iCol++);
			str sTmpAuthorName = (str)sqlite3_column_text(stmt_new_draft_get, iCol++);
			sqlite3_reset(stmt_new_draft_get);
			

			// 绑定作者信息到主表插入语句
			sqlite3_bind_int64(stmt_new_add, iIdx++, iAuthorType);
			sqlite3_bind_int64(stmt_new_add, iIdx++, iAuthorId);
			sqlite3_bind_text(stmt_new_add, iIdx++, sTmpAuthorName ? sTmpAuthorName : (str)"", -1, SQLITE_TRANSIENT);
			
			// 插入主表
			sqlite3_bind_int64(stmt_new_add, iIdx++, now);
			sqlite3_bind_int64(stmt_new_add, iIdx++, now);
			sqlite3_step(stmt_new_add);
			sqlite3_reset(stmt_new_add);
			
			// 删除草稿
			sqlite3_bind_int64(stmt_new_draft_publish, 1, iID);
			sqlite3_step(stmt_new_draft_publish);
			sqlite3_reset(stmt_new_draft_publish);
			
			iSuccess++;
		} else {
			sqlite3_reset(stmt_new_draft_get);
		}
	}
	
	xvoUnref(tblForm);
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"count\": %d}}", iSuccess);
}

// 批量删除草稿
void Admin_new_Draft_BatchDelete(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_DELETE ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( !tblForm || tblForm->Type != XVO_DT_TABLE ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		if ( tblForm ) xvoUnref(tblForm);
		return;
	}
	
	xvalue arrIds = xvoTableGetValue(tblForm, "ids", 3);
	if ( !arrIds || arrIds->Type != XVO_DT_ARRAY ) {
		xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing ids\"}", 0);
		return;
	}
	
	int iCount = xvoArrayItemCount(arrIds);
	int iSuccess = 0;
	
	for ( int i = 0; i < iCount; i++ ) {
		int64 iID = xvoArrayGetInt(arrIds, i);
		if ( iID <= 0 ) continue;
		
		// 删除草稿
		sqlite3_bind_int64(stmt_new_draft_del, 1, iID);
		if ( sqlite3_step(stmt_new_draft_del) == SQLITE_DONE ) {
			iSuccess++;
		}
		sqlite3_reset(stmt_new_draft_del);
	}
	
	xvoUnref(tblForm);
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\", \"data\": {\"count\": %d}}", iSuccess);
}

// 清空草稿箱
void Admin_new_Draft_Clear(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_DELETE ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	// 清空草稿箱
	sqlite3_step(stmt_new_draft_clear);
	sqlite3_reset(stmt_new_draft_clear);
	
	// 重置ID自增序列
	char* sErr = NULL;
	sqlite3_exec(G_DB->objDB, "DELETE FROM sqlite_sequence WHERE name='model_new_new_draft'", NULL, NULL, &sErr);
	if ( sErr ) sqlite3_free(sErr);
	
	http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"Success\"}", 0);
}

// 获取草稿数量
void Admin_new_Draft_Count(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	int64 iCount = 0;
	if ( sqlite3_step(stmt_new_draft_count) == SQLITE_ROW ) {
		iCount = sqlite3_column_int64(stmt_new_draft_count, 0);
	}
	sqlite3_reset(stmt_new_draft_count);
	
	mg_http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"count\": %lld}", iCount);
}




// ==================== 路由注册 ====================

void Model_new_RegisterRoutes()
{
	printf("        [Model] Registering routes for new...\n");
	

	// 前台 API 路由
	Model_AddRoute("/api/v1/new/new/list", Api_new_List, FALSE, FALSE, 0, 0);
	Model_AddRoute("/api/v1/new/new/get", Api_new_Get, FALSE, FALSE, 0, 0);


/* SUBMIT DISABLED

	// 前台投稿路由
	Model_AddRoute("/api/v1/new/new/submit", Api_new_Submit, TRUE, FALSE, 0, 0);
*/

/* REPLY DISABLED

	// 评论路由
	Model_AddRoute("/api/v1/new/new/reply/list", Api_new_Reply_List, FALSE, FALSE, 0, 0);
	Model_AddRoute("/api/v1/new/new/reply/add", Api_new_Reply_Add, TRUE, FALSE, 0, {{REPLY_AUTH_LEVEL}});
	Model_AddRoute("/api/v1/new/new/reply/delete", Api_new_Reply_Delete, TRUE, FALSE, 0, {{REPLY_AUTH_LEVEL}});
*/


	// 后台管理路由
	Model_AddRoute("/admin/new/new/list", Admin_new_List, TRUE, TRUE, 23, 0);
	Model_AddRoute("/admin/new/new/get", Admin_new_Get, TRUE, TRUE, 23, 0);
	Model_AddRoute("/admin/new/new/add", Admin_new_Add, TRUE, TRUE, 23, 0);
	Model_AddRoute("/admin/new/new/save", Admin_new_Save, TRUE, TRUE, 23, 0);
	Model_AddRoute("/admin/new/new/delete", Admin_new_Delete, TRUE, TRUE, 23, 0);



	// 草稿管理路由
	Model_AddRoute("/admin/new/new/draft/list", Admin_new_Draft_List, TRUE, TRUE, 23, 0);
	Model_AddRoute("/admin/new/new/draft/get", Admin_new_Draft_Get, TRUE, TRUE, 23, 0);
	Model_AddRoute("/admin/new/new/draft/save", Admin_new_Draft_Save, TRUE, TRUE, 23, 0);
	Model_AddRoute("/admin/new/new/draft/delete", Admin_new_Draft_Delete, TRUE, TRUE, 23, 0);
	Model_AddRoute("/admin/new/new/draft/publish", Admin_new_Draft_Publish, TRUE, TRUE, 23, 0);
	Model_AddRoute("/admin/new/new/draft/batch_publish", Admin_new_Draft_BatchPublish, TRUE, TRUE, 23, 0);
	Model_AddRoute("/admin/new/new/draft/delete_batch", Admin_new_Draft_BatchDelete, TRUE, TRUE, 23, 0);
	Model_AddRoute("/admin/new/new/draft/clear", Admin_new_Draft_Clear, TRUE, TRUE, 23, 0);
	Model_AddRoute("/admin/new/new/draft/count", Admin_new_Draft_Count, TRUE, TRUE, 23, 0);

}

void Model_new_UnregisterRoutes()
{
	printf("        [Model] Unregistering routes for new...\n");
	

	Model_RemoveRoute("/api/v1/new/new/list");
	Model_RemoveRoute("/api/v1/new/new/get");


/* SUBMIT DISABLED

	Model_RemoveRoute("/api/v1/new/new/submit");
*/

/* REPLY DISABLED

	Model_RemoveRoute("/api/v1/new/new/reply/list");
	Model_RemoveRoute("/api/v1/new/new/reply/add");
	Model_RemoveRoute("/api/v1/new/new/reply/delete");
*/


	Model_RemoveRoute("/admin/new/new/list");
	Model_RemoveRoute("/admin/new/new/get");
	Model_RemoveRoute("/admin/new/new/add");
	Model_RemoveRoute("/admin/new/new/save");
	Model_RemoveRoute("/admin/new/new/delete");



	Model_RemoveRoute("/admin/new/new/draft/list");
	Model_RemoveRoute("/admin/new/new/draft/get");
	Model_RemoveRoute("/admin/new/new/draft/save");
	Model_RemoveRoute("/admin/new/new/draft/delete");
	Model_RemoveRoute("/admin/new/new/draft/publish");
	Model_RemoveRoute("/admin/new/new/draft/batch_publish");
	Model_RemoveRoute("/admin/new/new/draft/delete_batch");
	Model_RemoveRoute("/admin/new/new/draft/clear");
	Model_RemoveRoute("/admin/new/new/draft/count");

}



// ==================== 模型入口 ====================

void Model_new_Init()
{
	printf("    [Model] Init new...\n");
	Model_new_InitStmt();
	Model_new_RegisterRoutes();
}

void Model_new_Unit()
{
	printf("    [Model] Unit new...\n");
	Model_new_UnregisterRoutes();
	Model_new_FreeStmt();
}
