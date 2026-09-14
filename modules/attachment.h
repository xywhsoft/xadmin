/* 附件存储核心：路径布局 data/uploads/<model>/<YYYY>/<MM>/<xid>.<ext>、
 * MIME 映射、防盗链白名单、扩展名/配额检查与附件/订单表操作。
 * 时间列已由 DB_MigrateTimeUnits 换算微秒；新写入一律 xrtNow()。 */

static char* AttachmentPath;

/* ---- MIME 类型映射 ---- */

typedef struct {
	const char* ext;
	const char* mime;
	int isInline;  /* 1=inline 显示, 0=attachment 下载 */
} AttachmentMime;

static const AttachmentMime G_AttachmentMimeTable[] = {
	{"jpg",  "image/jpeg", 1},
	{"jpeg", "image/jpeg", 1},
	{"png",  "image/png", 1},
	{"gif",  "image/gif", 1},
	{"webp", "image/webp", 1},
	{"bmp",  "image/bmp", 1},
	{"svg",  "image/svg+xml", 1},
	{"ico",  "image/x-icon", 1},
	{"txt",  "text/plain; charset=utf-8", 1},
	{"html", "text/html; charset=utf-8", 1},
	{"htm",  "text/html; charset=utf-8", 1},
	{"css",  "text/css; charset=utf-8", 1},
	{"js",   "application/javascript; charset=utf-8", 1},
	{"json", "application/json; charset=utf-8", 1},
	{"xml",  "application/xml; charset=utf-8", 1},
	{"md",   "text/markdown; charset=utf-8", 1},
	{"pdf",  "application/pdf", 1},
	{"mp4",  "video/mp4", 1},
	{"webm", "video/webm", 1},
	{"ogg",  "video/ogg", 1},
	{"mp3",  "audio/mpeg", 1},
	{"wav",  "audio/wav", 1},
	{"flac", "audio/flac", 1},
	{"zip",  "application/zip", 0},
	{"rar",  "application/x-rar-compressed", 0},
	{"7z",   "application/x-7z-compressed", 0},
	{"tar",  "application/x-tar", 0},
	{"gz",   "application/gzip", 0},
	{"doc",  "application/msword", 0},
	{"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document", 0},
	{"xls",  "application/vnd.ms-excel", 0},
	{"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", 0},
	{"ppt",  "application/vnd.ms-powerpoint", 0},
	{"pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation", 0},
	{NULL, NULL, 0}
};

static const AttachmentMime* Attachment_GetMime(const char* ext)
{
	int i;
	if (!ext) return NULL;
	for (i = 0; G_AttachmentMimeTable[i].ext; i++)
		if (xrtStrCaseCompare(xrtStrView(ext), xrtStrView(G_AttachmentMimeTable[i].ext)) == 0)
			return &G_AttachmentMimeTable[i];
	return NULL;
}

/* ---- 防盗链白名单 ---- */

static xmap* G_HotlinkExact;
static xmap* G_HotlinkSuffix;

static void Attachment_LoadHotlinkWhitelist(void)
{
	xvalue* cfg;
	str whitelist;

	if (G_HotlinkExact) xrtMapDestroy(G_HotlinkExact);
	if (G_HotlinkSuffix) xrtMapDestroy(G_HotlinkSuffix);
	G_HotlinkExact = xrtMapCreate(0);
	G_HotlinkSuffix = xrtMapCreate(0);

	cfg = ValueGet(G_Option, "attachment");
	if (!cfg) return;
	whitelist = ValueText(cfg, "hotlinkWhitelist");
	if (!whitelist || !whitelist[0]) return;
	{
		char* copy = xrtStrDup(whitelist);
		char* line = copy;
		while (line) {
			char* next = strpbrk(line, "\r\n");
			char* p;
			size_t len;
			if (next) *next++ = '\0';
			p = line;
			while (*p == ' ' || *p == '\t') p++;
			len = strlen(p);
			while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t')) p[--len] = '\0';
			if (len > 0) {
				if (p[0] == '*' && p[1] == '.')
					xrtMapGetOrAdd(G_HotlinkSuffix, KeyViewN(p + 1, len - 1), NULL);
				else
					xrtMapGetOrAdd(G_HotlinkExact, KeyViewN(p, len), NULL);
			}
			line = next;
		}
		xrtFree(copy);
	}
}

static bool Attachment_CheckHotlinkWhitelist(const char* host, size_t len)
{
	const char* p;

	if (!host || len == 0) return false;
	if (xrtMapGet(G_HotlinkExact, KeyViewN(host, len))) return true;
	for (p = host; (p = strchr(p, '.')) != NULL; p++) {
		if (xrtMapGet(G_HotlinkSuffix, KeyViewN(p, len - (size_t)(p - host)))) return true;
	}
	return false;
}

static bool Attachment_CheckHotlink(XS_RequestObject objReq, bool allowHotlink)
{
	const char* referer;
	const char* host;
	const char* p;
	const char* pEnd;
	size_t hostLen;

	if (allowHotlink) return true;
	referer = XAdmin_PluginReqHeader(objReq, "Referer");
	if (!referer || !referer[0]) return true;   /* 直达访问放行 */
	p = strstr(referer, "://");
	p = p ? p + 3 : referer;
	pEnd = p;
	while (*pEnd && *pEnd != '/' && *pEnd != ':' && *pEnd != '?') pEnd++;
	hostLen = (size_t)(pEnd - p);
	host = p;
	if (!hostLen) return false;
	if ((hostLen == 9 && strncmp(host, "localhost", 9) == 0) ||
	    (hostLen == 9 && strncmp(host, "127.0.0.1", 9) == 0))
		return true;
	return Attachment_CheckHotlinkWhitelist(host, hostLen);
}

/* ---- 上传约束 ---- */

static bool Attachment_IsExtAllowed(const char* ext)
{
	xvalue* cfg = ValueGet(G_Option, "attachment");
	str allowed;
	char* copy;
	char* token;
	bool found = false;

	if (!ext) return false;
	if (!cfg) return true;
	allowed = ValueText(cfg, "allowedExts");
	if (!allowed || !allowed[0]) return true;
	copy = xrtStrDup(allowed);
	token = copy;
	while (token) {
		char* next = strchr(token, ',');
		if (next) *next++ = '\0';
		while (*token == ' ') token++;
		if (xrtStrCaseCompare(xrtStrView(token), xrtStrView(ext)) == 0) {
			found = true;
			break;
		}
		token = next;
	}
	xrtFree(copy);
	return found;
}

static int64 Attachment_GetMaxSize(void)
{
	xvalue* cfg = ValueGet(G_Option, "attachment");
	int64 maxMB = cfg ? ValueInt(cfg, "maxSize") : 0;
	return maxMB > 0 ? maxMB * 1024 * 1024 : 0;
}

static int64 Attachment_GetUserQuota(void)
{
	xvalue* cfg = ValueGet(G_Option, "attachment");
	int64 quotaMB = cfg ? ValueInt(cfg, "userQuota") : 0;
	return quotaMB > 0 ? quotaMB * 1024 * 1024 : 0;
}

/* ---- 存储路径 ---- */

static char* Attachment_GeneratePath(const char* modelName, const char* xid, const char* ext)
{
	xdatetime date;
	const char* subDir = (modelName && modelName[0]) ? modelName : "global";

	if (!xrtTimeLocal(xrtNow(), &date)) return NULL;
	return xrtFormat("%s/%d/%d/%s.%s", subDir, (int)date.Year, (int)date.Month, xid, ext);
}

static bool Attachment_EnsureDir(const char* path)
{
	char* full = xrtPathJoin(AttachmentPath, path);
	char* dir = xrtPathParent(full);
	bool ok = xrtDirCreateAll(dir);
	xrtFree(dir);
	xrtFree(full);
	return ok;
}

/* ---- 数据库操作 ---- */

static sqlite3_stmt* stmt_attachment_add;
static sqlite3_stmt* stmt_attachment_get;
static sqlite3_stmt* stmt_attachment_check_order;
static sqlite3_stmt* stmt_attachment_add_order;
static sqlite3_stmt* stmt_attachment_update_sales;
static sqlite3_stmt* stmt_attachment_user_usage;
static sqlite3_stmt* stmt_attachment_stats_total;
static sqlite3_stmt* stmt_attachment_stats_by_model;
static sqlite3_stmt* stmt_attachment_stats_by_ext;

static bool Attachment_Add(const char* xid, const char* filename, const char* ext, const char* mime,
	int64 size, const char* path, const char* modelName, int64 recordId,
	int64 uploaderId, int uploaderType, int allowHotlink, int accessType,
	int accessLevel, int64 price, int priceType)
{
	sqlite3_bind_text(stmt_attachment_add, 1, xid, -1, NULL);
	sqlite3_bind_text(stmt_attachment_add, 2, filename, -1, NULL);
	sqlite3_bind_text(stmt_attachment_add, 3, ext, -1, NULL);
	sqlite3_bind_text(stmt_attachment_add, 4, mime, -1, NULL);
	sqlite3_bind_int64(stmt_attachment_add, 5, size);
	sqlite3_bind_text(stmt_attachment_add, 6, path, -1, NULL);
	sqlite3_bind_text(stmt_attachment_add, 7, modelName ? modelName : "", -1, NULL);
	sqlite3_bind_int64(stmt_attachment_add, 8, recordId);
	sqlite3_bind_int64(stmt_attachment_add, 9, uploaderId);
	sqlite3_bind_int(stmt_attachment_add, 10, uploaderType);
	sqlite3_bind_int(stmt_attachment_add, 11, allowHotlink);
	sqlite3_bind_int(stmt_attachment_add, 12, accessType);
	sqlite3_bind_int(stmt_attachment_add, 13, accessLevel);
	sqlite3_bind_int64(stmt_attachment_add, 14, price);
	sqlite3_bind_int(stmt_attachment_add, 15, priceType);
	sqlite3_bind_int64(stmt_attachment_add, 16, xrtNow());
	{
		int rc = sqlite3_step(stmt_attachment_add);
		sqlite3_reset(stmt_attachment_add);
		return rc == SQLITE_DONE;
	}
}

static bool Attachment_CheckPurchased(const char* xid, int64 memberId)
{
	sqlite3_bind_text(stmt_attachment_check_order, 1, xid, -1, NULL);
	sqlite3_bind_int64(stmt_attachment_check_order, 2, memberId);
	{
		bool purchased = sqlite3_step(stmt_attachment_check_order) == SQLITE_ROW;
		sqlite3_reset(stmt_attachment_check_order);
		return purchased;
	}
}

static bool Attachment_AddOrder(const char* xid, int64 memberId, int64 price, int priceType,
	int64 sellerId, int64 sellerIncome)
{
	sqlite3_bind_text(stmt_attachment_add_order, 1, xid, -1, NULL);
	sqlite3_bind_int64(stmt_attachment_add_order, 2, memberId);
	sqlite3_bind_int64(stmt_attachment_add_order, 3, price);
	sqlite3_bind_int(stmt_attachment_add_order, 4, priceType);
	sqlite3_bind_int64(stmt_attachment_add_order, 5, sellerId);
	sqlite3_bind_int64(stmt_attachment_add_order, 6, sellerIncome);
	sqlite3_bind_int64(stmt_attachment_add_order, 7, xrtNow());
	{
		int rc = sqlite3_step(stmt_attachment_add_order);
		sqlite3_reset(stmt_attachment_add_order);
		return rc == SQLITE_DONE;
	}
}

static bool Attachment_UpdateSales(const char* xid)
{
	sqlite3_bind_text(stmt_attachment_update_sales, 1, xid, -1, NULL);
	{
		int rc = sqlite3_step(stmt_attachment_update_sales);
		sqlite3_reset(stmt_attachment_update_sales);
		return rc == SQLITE_DONE;
	}
}

static bool Attachment_UpdateDownloadCount(const char* xid)
{
	sqlite3_stmt* stmt = NULL;
	bool ok = false;

	if (sqlite3_prepare_v3(G_DB, "UPDATE attachment SET downloadCount = downloadCount + 1 WHERE xid = ?",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_bind_text(stmt, 1, xid, -1, NULL);
		ok = sqlite3_step(stmt) == SQLITE_DONE;
		sqlite3_finalize(stmt);
	}
	return ok;
}

static int64 Attachment_GetUserUsage(int64 uploaderId, int uploaderType)
{
	int64 usage = 0;

	sqlite3_bind_int64(stmt_attachment_user_usage, 1, uploaderId);
	sqlite3_bind_int(stmt_attachment_user_usage, 2, uploaderType);
	if (sqlite3_step(stmt_attachment_user_usage) == SQLITE_ROW)
		usage = sqlite3_column_int64(stmt_attachment_user_usage, 0);
	sqlite3_reset(stmt_attachment_user_usage);
	return usage;
}

/* ---- 自装：URI 面（会员 API 与静态入口的确定性 isBackend=0） ---- */

static void Attachment_EnsureURI(const char* uri, bool needAuth)
{
	Notify_EnsureURIItem(uri, needAuth);
}

/* 静态入口仅修正侧别，不覆盖运营已配置的 authID/日志开关。 */
static void Attachment_EnsureAccessURI(void)
{
	sqlite3_stmt* stmt = NULL;

	if (sqlite3_prepare_v3(G_DB, "UPDATE uris SET isBackend = 0 WHERE uri = '/attachment'",
		-1, 0, &stmt, NULL) == SQLITE_OK) {
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}
}

void Attachment_Init(void)
{
	printf("        Attachment_Init \n");
	AttachmentPath = xrtPathJoin(xrtPathJoin(AppPath, "data"), "uploads");
	xrtDirCreateAll(AttachmentPath);
	Attachment_LoadHotlinkWhitelist();

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
	sqlite3_prepare_v3(G_DB,
		"SELECT COUNT(*), COALESCE(SUM(size), 0) FROM attachment WHERE isDelete = 0",
		-1, 0, &stmt_attachment_stats_total, NULL);
	sqlite3_prepare_v3(G_DB,
		"SELECT modelName, COUNT(*), COALESCE(SUM(size), 0) FROM attachment WHERE isDelete = 0 GROUP BY modelName",
		-1, 0, &stmt_attachment_stats_by_model, NULL);
	sqlite3_prepare_v3(G_DB,
		"SELECT ext, COUNT(*), COALESCE(SUM(size), 0) FROM attachment WHERE isDelete = 0 GROUP BY ext ORDER BY SUM(size) DESC",
		-1, 0, &stmt_attachment_stats_by_ext, NULL);

	Attachment_EnsureURI("/api/v1/attachment/upload", true);
	Attachment_EnsureURI("/api/v1/attachment/purchase", true);
	Attachment_EnsureURI("/api/v1/attachment/my", true);
	Attachment_EnsureURI("/api/v1/attachment/purchased", true);
	/* 静态入口须读 MSID（会员付费下载上下文），v1 种子里的 isBackend=1 修正为 0。 */
	Attachment_EnsureAccessURI();
}

void Attachment_Unit(void)
{
	printf("        Attachment_Unit \n");
	if (AttachmentPath) { xrtFree(AttachmentPath); AttachmentPath = NULL; }
	if (G_HotlinkExact) { xrtMapDestroy(G_HotlinkExact); G_HotlinkExact = NULL; }
	if (G_HotlinkSuffix) { xrtMapDestroy(G_HotlinkSuffix); G_HotlinkSuffix = NULL; }
	if (stmt_attachment_add) sqlite3_finalize(stmt_attachment_add);
	if (stmt_attachment_get) sqlite3_finalize(stmt_attachment_get);
	if (stmt_attachment_check_order) sqlite3_finalize(stmt_attachment_check_order);
	if (stmt_attachment_add_order) sqlite3_finalize(stmt_attachment_add_order);
	if (stmt_attachment_update_sales) sqlite3_finalize(stmt_attachment_update_sales);
	if (stmt_attachment_user_usage) sqlite3_finalize(stmt_attachment_user_usage);
	if (stmt_attachment_stats_total) sqlite3_finalize(stmt_attachment_stats_total);
	if (stmt_attachment_stats_by_model) sqlite3_finalize(stmt_attachment_stats_by_model);
	if (stmt_attachment_stats_by_ext) sqlite3_finalize(stmt_attachment_stats_by_ext);
}
