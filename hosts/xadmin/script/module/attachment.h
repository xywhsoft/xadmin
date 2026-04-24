


// 附件存储路径
str AttachmentPath = NULL;

// 防盗链白名单字典
xdict G_HotlinkExact = NULL;   // 精确域名字典
xdict G_HotlinkSuffix = NULL;  // 后缀域名字典

// 预编译的 SQL 语句
sqlite3_stmt* stmt_attachment_add = NULL;
sqlite3_stmt* stmt_attachment_get = NULL;
sqlite3_stmt* stmt_attachment_list = NULL;
sqlite3_stmt* stmt_attachment_update = NULL;
sqlite3_stmt* stmt_attachment_delete = NULL;
sqlite3_stmt* stmt_attachment_count = NULL;
sqlite3_stmt* stmt_attachment_check_order = NULL;
sqlite3_stmt* stmt_attachment_add_order = NULL;
sqlite3_stmt* stmt_attachment_update_sales = NULL;
sqlite3_stmt* stmt_attachment_stats_total = NULL;
sqlite3_stmt* stmt_attachment_stats_by_model = NULL;
sqlite3_stmt* stmt_attachment_stats_by_ext = NULL;
sqlite3_stmt* stmt_attachment_user_usage = NULL;



// ==================== MIME 类型映射 ====================

typedef struct {
	str ext;
	str mime;
	int isInline;  // 1=inline显示, 0=attachment下载
} MimeMapping;

MimeMapping G_MimeTable[] = {
	// 图片
	{"jpg",  "image/jpeg", 1},
	{"jpeg", "image/jpeg", 1},
	{"png",  "image/png", 1},
	{"gif",  "image/gif", 1},
	{"webp", "image/webp", 1},
	{"bmp",  "image/bmp", 1},
	{"svg",  "image/svg+xml", 1},
	{"ico",  "image/x-icon", 1},
	
	// 文本
	{"txt",  "text/plain; charset=utf-8", 1},
	{"html", "text/html; charset=utf-8", 1},
	{"htm",  "text/html; charset=utf-8", 1},
	{"css",  "text/css; charset=utf-8", 1},
	{"js",   "application/javascript; charset=utf-8", 1},
	{"json", "application/json; charset=utf-8", 1},
	{"xml",  "application/xml; charset=utf-8", 1},
	{"md",   "text/markdown; charset=utf-8", 1},
	
	// 文档
	{"pdf",  "application/pdf", 1},
	
	// 音视�?
	{"mp4",  "video/mp4", 1},
	{"webm", "video/webm", 1},
	{"ogg",  "video/ogg", 1},
	{"mp3",  "audio/mpeg", 1},
	{"wav",  "audio/wav", 1},
	{"flac", "audio/flac", 1},
	
	// 压缩包（下载�?
	{"zip",  "application/zip", 0},
	{"rar",  "application/x-rar-compressed", 0},
	{"7z",   "application/x-7z-compressed", 0},
	{"tar",  "application/x-tar", 0},
	{"gz",   "application/gzip", 0},
	
	// Office（下载）
	{"doc",  "application/msword", 0},
	{"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document", 0},
	{"xls",  "application/vnd.ms-excel", 0},
	{"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", 0},
	{"ppt",  "application/vnd.ms-powerpoint", 0},
	{"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation", 0},
	
	// 结束标记
	{NULL, NULL, 0}
};

// 根据扩展名获取MIME信息
MimeMapping* Attachment_GetMime(str sExt)
{
	if ( !sExt ) return NULL;
	for ( int i = 0; G_MimeTable[i].ext != NULL; i++ ) {
		if ( xrtStrComp(sExt, G_MimeTable[i].ext, 0, FALSE) == 0 ) {
			return &G_MimeTable[i];
		}
	}
	return NULL;
}



// ==================== 防盗链白名单 ====================

// 加载防盗链白名单配置
void Attachment_LoadHotlinkWhitelist()
{
	// 清空并重建字�?
	if ( G_HotlinkExact ) {
		xrtDictDestroy(G_HotlinkExact);
	}
	if ( G_HotlinkSuffix ) {
		xrtDictDestroy(G_HotlinkSuffix);
	}
	G_HotlinkExact = xrtDictCreate(0, XRT_OBJMODE_SHARED);
	G_HotlinkSuffix = xrtDictCreate(0, XRT_OBJMODE_SHARED);
	xrtOwnerActivateShared(&G_HotlinkExact->Owner);
	xrtOwnerActivateShared(&G_HotlinkExact->AVLT.Owner);
	xrtOwnerActivateShared(&G_HotlinkSuffix->Owner);
	xrtOwnerActivateShared(&G_HotlinkSuffix->AVLT.Owner);
	
	// 读取配置
	xvalue tblAttachment = xvoTableGetValue(G_Option, "attachment", 10);
	if ( !tblAttachment || tblAttachment->Type != XVO_DT_TABLE ) return;
	
	str sWhitelist = xvoTableGetText(tblAttachment, "hotlinkWhitelist", 16);
	if ( !sWhitelist || !*sWhitelist ) return;
	
	// 按行解析
	str sCopy = xrtCopyStr(sWhitelist, 0);
	str sLine = strtok(sCopy, "\r\n");
	while ( sLine ) {
		// 跳过空行
		while ( *sLine == ' ' || *sLine == '\t' ) sLine++;
		if ( *sLine ) {
			size_t iLen = strlen(sLine);
			// 去除尾部空格
			while ( iLen > 0 && (sLine[iLen-1] == ' ' || sLine[iLen-1] == '\t') ) {
				iLen--;
			}
			
			if ( iLen > 0 ) {
				if ( sLine[0] == '*' && sLine[1] == '.' ) {
					// 通配符模�? *.example.com -> 存储 .example.com
					xrtDictSet(G_HotlinkSuffix, sLine + 1, iLen - 1, (ptr)1);
				} else {
					// 精确匹配
					xrtDictSet(G_HotlinkExact, sLine, iLen, (ptr)1);
				}
			}
		}
		sLine = strtok(NULL, "\r\n");
	}
	xrtFree(sCopy);
}

// 检查域名是否在白名单中
bool Attachment_CheckHotlinkWhitelist(str sHost, size_t iLen)
{
	if ( !sHost || iLen == 0 ) return FALSE;
	
	// 1. 精确匹配 O(1)
	if ( xrtDictGet(G_HotlinkExact, sHost, iLen) ) {
		return TRUE;
	}
	
	// 2. 后缀匹配 O(k)，k=域名层级�?
	str p = sHost;
	while ( (p = strchr(p, '.')) != NULL ) {
		size_t iSuffixLen = iLen - (p - sHost);
		if ( xrtDictGet(G_HotlinkSuffix, p, iSuffixLen) ) {
			return TRUE;
		}
		p++;
	}
	
	return FALSE;
}

// �?Referer 提取 host
str Attachment_ExtractHost(str sReferer, size_t* pLen)
{
	if ( !sReferer ) return NULL;
	
	// 跳过 scheme
	str p = strstr(sReferer, "://");
	if ( p ) {
		p += 3;
	} else {
		p = sReferer;
	}
	
	// 找到 host 结束位置
	str pEnd = p;
	while ( *pEnd && *pEnd != '/' && *pEnd != ':' && *pEnd != '?' ) {
		pEnd++;
	}
	
	*pLen = pEnd - p;
	return p;
}

// 检查防盗链
bool Attachment_CheckHotlink(XS_RequestObject objReq, bool bAllowHotlink)
{
	if ( bAllowHotlink ) return TRUE;
	
	const char* sReferer = xsReqHeader(objReq, "Referer");
	if ( sReferer == NULL || sReferer[0] == '\0' ) {
		// �?Referer，允许（直接访问�?
		return TRUE;
	}
	
	// 提取 host
	size_t iHostLen = 0;
	str sHost = Attachment_ExtractHost((str)sReferer, &iHostLen);
	if ( !sHost || iHostLen == 0 ) return FALSE;
	
	// 检查是否为本站
	// TODO: 从配置获取本站域名列�?
	if ( ((iHostLen == 9) && (strncmp(sHost, "localhost", 9) == 0)) ||
		 ((iHostLen == 9) && (strncmp(sHost, "127.0.0.1", 9) == 0)) ) {
		return TRUE;
	}
	
	// 检查白名单
	return Attachment_CheckHotlinkWhitelist(sHost, iHostLen);
}



// ==================== 辅助函数 ====================

// 检查扩展名是否允许上传
bool Attachment_IsExtAllowed(str sExt)
{
	if ( !sExt ) return FALSE;
	
	xvalue tblAttachment = xvoTableGetValue(G_Option, "attachment", 10);
	if ( !tblAttachment ) return TRUE;  // 无配置则允许
	
	str sAllowed = xvoTableGetText(tblAttachment, "allowedExts", 11);
	if ( !sAllowed || !*sAllowed ) return TRUE;  // 无配置则允许
	
	// 搜索扩展�?
	str sCopy = xrtCopyStr(sAllowed, 0);
	str sToken = strtok(sCopy, ",");
	bool bFound = FALSE;
	while ( sToken ) {
		while ( *sToken == ' ' ) sToken++;
		if ( xrtStrComp(sToken, sExt, 0, FALSE) == 0 ) {
			bFound = TRUE;
			break;
		}
		sToken = strtok(NULL, ",");
	}
	xrtFree(sCopy);
	return bFound;
}

// 获取最大上传大小（字节�?
int64 Attachment_GetMaxSize()
{
	xvalue tblAttachment = xvoTableGetValue(G_Option, "attachment", 10);
	if ( !tblAttachment ) return 0;
	
	int64 iMaxMB = xvoTableGetInt(tblAttachment, "maxSize", 7);
	if ( iMaxMB <= 0 ) return 0;  // 0表示不限�?
	
	return iMaxMB * 1024 * 1024;
}

// 获取用户配额（字节）
int64 Attachment_GetUserQuota()
{
	xvalue tblAttachment = xvoTableGetValue(G_Option, "attachment", 10);
	if ( !tblAttachment ) return 0;
	
	int64 iQuotaMB = xvoTableGetInt(tblAttachment, "userQuota", 9);
	if ( iQuotaMB <= 0 ) return 0;  // 0表示不限�?
	
	return iQuotaMB * 1024 * 1024;
}

// 获取用户已使用空�?
int64 Attachment_GetUserUsage(int64 iUploaderId, int iUploaderType)
{
	sqlite3_bind_int64(stmt_attachment_user_usage, 1, iUploaderId);
	sqlite3_bind_int(stmt_attachment_user_usage, 2, iUploaderType);
	
	int64 iUsage = 0;
	if ( sqlite3_step(stmt_attachment_user_usage) == SQLITE_ROW ) {
		iUsage = sqlite3_column_int64(stmt_attachment_user_usage, 0);
	}
	sqlite3_reset(stmt_attachment_user_usage);
	return iUsage;
}

// 生成存储路径
str Attachment_GeneratePath(str sModelName, str sXID, str sExt)
{
	// 获取当前年月
	xtime now = xrtNow();
	str sDate = xrtTimeToStr(now, XRT_TIME_FORMAT_DATE);
	
	// 提取年月 (YYYY-MM-DD -> YYYY/MM)
	char sYearMonth[8];
	memcpy(sYearMonth, sDate, 4);
	sYearMonth[4] = '/';
	memcpy(sYearMonth + 5, sDate + 5, 2);
	sYearMonth[7] = '\0';
	xrtFree(sDate);
	
	// 构建路径
	str sSubDir = (sModelName && *sModelName) ? sModelName : (str)"global";
	str sPath = xrtFormat("%s/%s/%s.%s", sSubDir, sYearMonth, sXID, sExt);
	
	return sPath;
}

// 确保目录存在
bool Attachment_EnsureDir(str sPath)
{
	str sFullPath = xrtPathJoin(2, AttachmentPath, sPath);
	str sDir = xrtPathGetDir(sFullPath, 0);
	bool bRet = xrtDirCreateAll(sDir);
	xrtFree(sDir);
	xrtFree(sFullPath);
	return bRet;
}



// ==================== 数据库操�?====================

// 添加附件记录
bool Attachment_Add(str sXID, str sFilename, str sExt, str sMime, int64 iSize, str sPath,
					str sModelName, int64 iRecordId, int64 iUploaderId, int iUploaderType,
					int iAllowHotlink, int iAccessType, int iAccessLevel, int64 iPrice, int iPriceType)
{
	sqlite3_bind_text(stmt_attachment_add, 1, sXID, -1, NULL);
	sqlite3_bind_text(stmt_attachment_add, 2, sFilename, -1, NULL);
	sqlite3_bind_text(stmt_attachment_add, 3, sExt, -1, NULL);
	sqlite3_bind_text(stmt_attachment_add, 4, sMime, -1, NULL);
	sqlite3_bind_int64(stmt_attachment_add, 5, iSize);
	sqlite3_bind_text(stmt_attachment_add, 6, sPath, -1, NULL);
	sqlite3_bind_text(stmt_attachment_add, 7, sModelName ? sModelName : (str)"", -1, NULL);
	sqlite3_bind_int64(stmt_attachment_add, 8, iRecordId);
	sqlite3_bind_int64(stmt_attachment_add, 9, iUploaderId);
	sqlite3_bind_int(stmt_attachment_add, 10, iUploaderType);
	sqlite3_bind_int(stmt_attachment_add, 11, iAllowHotlink);
	sqlite3_bind_int(stmt_attachment_add, 12, iAccessType);
	sqlite3_bind_int(stmt_attachment_add, 13, iAccessLevel);
	sqlite3_bind_int64(stmt_attachment_add, 14, iPrice);
	sqlite3_bind_int(stmt_attachment_add, 15, iPriceType);
	sqlite3_bind_int64(stmt_attachment_add, 16, xrtNow());
	
	int rc = sqlite3_step(stmt_attachment_add);
	sqlite3_reset(stmt_attachment_add);
	return rc == SQLITE_DONE;
}

// 检查是否已购买
bool Attachment_CheckPurchased(str sXID, int64 iMemberId)
{
	sqlite3_bind_text(stmt_attachment_check_order, 1, sXID, -1, NULL);
	sqlite3_bind_int64(stmt_attachment_check_order, 2, iMemberId);
	
	bool bPurchased = sqlite3_step(stmt_attachment_check_order) == SQLITE_ROW;
	sqlite3_reset(stmt_attachment_check_order);
	return bPurchased;
}

// 添加购买记录
bool Attachment_AddOrder(str sXID, int64 iMemberId, int64 iPrice, int iPriceType, 
						 int64 iSellerId, int64 iSellerIncome)
{
	sqlite3_bind_text(stmt_attachment_add_order, 1, sXID, -1, NULL);
	sqlite3_bind_int64(stmt_attachment_add_order, 2, iMemberId);
	sqlite3_bind_int64(stmt_attachment_add_order, 3, iPrice);
	sqlite3_bind_int(stmt_attachment_add_order, 4, iPriceType);
	sqlite3_bind_int64(stmt_attachment_add_order, 5, iSellerId);
	sqlite3_bind_int64(stmt_attachment_add_order, 6, iSellerIncome);
	sqlite3_bind_int64(stmt_attachment_add_order, 7, xrtNow());
	
	int rc = sqlite3_step(stmt_attachment_add_order);
	sqlite3_reset(stmt_attachment_add_order);
	return rc == SQLITE_DONE;
}

// 更新销�?
bool Attachment_UpdateSales(str sXID)
{
	sqlite3_bind_text(stmt_attachment_update_sales, 1, sXID, -1, NULL);
	int rc = sqlite3_step(stmt_attachment_update_sales);
	sqlite3_reset(stmt_attachment_update_sales);
	return rc == SQLITE_DONE;
}

// 更新下载次数
bool Attachment_UpdateDownloadCount(str sXID)
{
	sqlite3_stmt* stmt;
	sqlite3_prepare_v3(G_DB, 
		"UPDATE attachment SET downloadCount = downloadCount + 1 WHERE xid = ?",
		-1, 0, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, sXID, -1, NULL);
	int rc = sqlite3_step(stmt);
	sqlite3_finalize(stmt);
	return rc == SQLITE_DONE;
}

// 删除记录关联的附�?
void Attachment_DeleteByRecord(str sModelName, int64 iRecordId)
{
	sqlite3_stmt* stmt;
	sqlite3_prepare_v3(G_DB,
		"SELECT xid, path FROM attachment WHERE modelName = ? AND recordId = ? AND isDelete = 0",
		-1, 0, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, sModelName, -1, NULL);
	sqlite3_bind_int64(stmt, 2, iRecordId);
	
	while ( sqlite3_step(stmt) == SQLITE_ROW ) {
		str sPath = (str)sqlite3_column_text(stmt, 1);
		if ( sPath ) {
			str sFullPath = xrtPathJoin(2, AttachmentPath, sPath);
			xrtFileDelete(sFullPath);
			xrtFree(sFullPath);
		}
	}
	sqlite3_finalize(stmt);
	
	// 标记删除
	sqlite3_prepare_v3(G_DB,
		"UPDATE attachment SET isDelete = 1 WHERE modelName = ? AND recordId = ?",
		-1, 0, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, sModelName, -1, NULL);
	sqlite3_bind_int64(stmt, 2, iRecordId);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}



// ==================== 初始化和卸载 ====================

void Attachment_Init()
{
	printf("        Attachment_Init \n");
	
	// 设置存储路径
	AttachmentPath = xrtPathJoin(3, AppPath, "data", "uploads");
	
	// 确保目录存在
	xrtDirCreateAll(AttachmentPath);
	
	// 加载防盗链白名单
	Attachment_LoadHotlinkWhitelist();
	
	// 预编�?SQL 语句
	sqlite3_prepare_v3(G_DB,
		"INSERT INTO attachment (xid, filename, ext, mime, size, path, modelName, recordId, "
		"uploaderId, uploaderType, allowHotlink, accessType, accessLevel, price, priceType, createTime) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
		-1, 0, &stmt_attachment_add, NULL);
	
	sqlite3_prepare_v3(G_DB,
		"SELECT xid, filename, ext, mime, size, path, modelName, recordId, uploaderId, uploaderType, "
		"allowHotlink, accessType, accessLevel, price, priceType, salesCount, downloadCount, remark, createTime "
		"FROM attachment WHERE xid = ? AND isDelete = 0",
		-1, 0, &stmt_attachment_get, NULL);
	
	sqlite3_prepare_v3(G_DB,
		"SELECT 1 FROM attachmentOrder WHERE attachmentXid = ? AND memberId = ?",
		-1, 0, &stmt_attachment_check_order, NULL);
	
	sqlite3_prepare_v3(G_DB,
		"INSERT INTO attachmentOrder (attachmentXid, memberId, price, priceType, sellerId, sellerIncome, createTime) "
		"VALUES (?, ?, ?, ?, ?, ?, ?)",
		-1, 0, &stmt_attachment_add_order, NULL);
	
	sqlite3_prepare_v3(G_DB,
		"UPDATE attachment SET salesCount = salesCount + 1 WHERE xid = ?",
		-1, 0, &stmt_attachment_update_sales, NULL);
	
	sqlite3_prepare_v3(G_DB,
		"SELECT COALESCE(SUM(size), 0) FROM attachment WHERE uploaderId = ? AND uploaderType = ? AND isDelete = 0",
		-1, 0, &stmt_attachment_user_usage, NULL);
	
	// 统计相关
	sqlite3_prepare_v3(G_DB,
		"SELECT COUNT(*), COALESCE(SUM(size), 0) FROM attachment WHERE isDelete = 0",
		-1, 0, &stmt_attachment_stats_total, NULL);
	
	sqlite3_prepare_v3(G_DB,
		"SELECT modelName, COUNT(*), COALESCE(SUM(size), 0) FROM attachment WHERE isDelete = 0 GROUP BY modelName",
		-1, 0, &stmt_attachment_stats_by_model, NULL);
	
	sqlite3_prepare_v3(G_DB,
		"SELECT ext, COUNT(*), COALESCE(SUM(size), 0) FROM attachment WHERE isDelete = 0 GROUP BY ext ORDER BY SUM(size) DESC",
		-1, 0, &stmt_attachment_stats_by_ext, NULL);
}

void Attachment_Unit()
{
	printf("        Attachment_Unit \n");
	
	// 释放路径
	if ( AttachmentPath ) {
		xrtFree(AttachmentPath);
		AttachmentPath = NULL;
	}
	
	// 释放字典
	if ( G_HotlinkExact ) {
		xrtDictDestroy(G_HotlinkExact);
		G_HotlinkExact = NULL;
	}
	if ( G_HotlinkSuffix ) {
		xrtDictDestroy(G_HotlinkSuffix);
		G_HotlinkSuffix = NULL;
	}
	
	// 释放 SQL 语句
	if ( stmt_attachment_add ) sqlite3_finalize(stmt_attachment_add);
	if ( stmt_attachment_get ) sqlite3_finalize(stmt_attachment_get);
	if ( stmt_attachment_check_order ) sqlite3_finalize(stmt_attachment_check_order);
	if ( stmt_attachment_add_order ) sqlite3_finalize(stmt_attachment_add_order);
	if ( stmt_attachment_update_sales ) sqlite3_finalize(stmt_attachment_update_sales);
	if ( stmt_attachment_user_usage ) sqlite3_finalize(stmt_attachment_user_usage);
	if ( stmt_attachment_stats_total ) sqlite3_finalize(stmt_attachment_stats_total);
	if ( stmt_attachment_stats_by_model ) sqlite3_finalize(stmt_attachment_stats_by_model);
	if ( stmt_attachment_stats_by_ext ) sqlite3_finalize(stmt_attachment_stats_by_ext);
}


