/* v1 安装向导契约（module/install.h + http.h 拦截）：
 * install.lock 缺失且 db/main.db 缺失 → 向导接管请求（放行必需静态资源）；GET 出 install.html
 * （客户端 SHA-256(用户名+"_xywhsoft_"+密码) 后 POST 到 /），POST 四步：
 * 建库（本代改为执行 install/init.sql 全量脚本，v1 为复制预置库文件）→
 * 业务段启动 → 创建超管（随机 salt + 服务端二次哈希）→ 写 install.lock。
 * 加固偏差：db/main.db 已存在而 lock 缺失视为存量部署，不进向导（v1 会复制
 * 预置库覆盖既有数据，本代拒绝覆盖）；密码必须为 64 位小写十六进制客户端哈希。 */
static bool G_Install;     /* install.lock 存在 */
static bool G_InstallMode; /* 向导接管中（未安装） */
static bool G_InstallBusy; /* 安装事务执行中（防并发重复提交） */

static bool G_BusinessStarted; /* main.c 暂定定义合并：向导据此判断业务段是否已拉起 */
static bool XAdmin_BusinessStart(XS_HostInfo* host); /* main.c 后段定义 */

static void Install_Init(void)
{
	char* path = xrtPathJoin(AppPath, "install.lock");
	G_Install = xrtPathExists(path);
	xrtFree(path);
	G_InstallMode = !G_Install;
	if (G_InstallMode) {
		char* db = xrtPathJoin(DBPath, "main.db");
		if (xrtPathExists(db)) {
			G_InstallMode = false;
			printf("[xadmin][warn] install.lock missing but db/main.db present; treat as migrated deployment\n");
		}
		xrtFree(db);
	}
}

static bool Install_IsHex64(const char* text)
{
	return IsClientPasswordHash(text);
}

/* Only the wizard's public resources bypass installation interception. Exact
 * paths keep database/source files and encoded traversal in the wizard. */
static bool Install_IsPublicAsset(const char* path)
{
	static const char* assets[] = {
		"/layui/layui.js", "/layui/css/layui.css",
		"/layui/font/iconfont.eot", "/layui/font/iconfont.svg",
		"/layui/font/iconfont.ttf", "/layui/font/iconfont.woff", "/layui/font/iconfont.woff2"
	};
	size_t i;
	for (i = 0; i < sizeof(assets)/sizeof(assets[0]); i++)
		if (!strcmp(path, assets[i])) return true;
	return false;
}

static bool Install_ValidUsername(const char* user)
{
	size_t i, len;
	if (!user || (len = strlen(user)) < 3 || len > 32) return false;
	for (i = 0; i < len; i++) {
		char c = user[i];
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
		      c == '-' || c == '_' || c == '.')) return false;
	}
	return true;
}

/* 建库：显式安装动作（全应用唯一允许创建 main.db 的路径），执行 install/init.sql。 */
static bool Install_CreateDatabase(char* err, size_t errSize)
{
	char* script; char* dbPath; sqlite3* db = NULL; char* serr = NULL; bool ok = false;
	size_t size = 0;

	dbPath = xrtPathJoin(DBPath, "main.db");
	if (!dbPath) { snprintf(err, errSize, "out of memory"); return false; }
	xrtDirCreateAll(DBPath);
	if (sqlite3_open(dbPath, &db) != SQLITE_OK) {
		snprintf(err, errSize, "sqlite open failed: %s", db ? sqlite3_errmsg(db) : "?");
		goto done;
	}
	script = (char*)xrtFileReadAll(xrtPathJoin(AppPath, "install/init.sql"), &size);
	/* 兼容 BOM */
	if (script && size >= 3 && (unsigned char)script[0] == 0xEF && (unsigned char)script[1] == 0xBB && (unsigned char)script[2] == 0xBF) {
		script += 3; size -= 3;
	}
	if (!script || !size) {
		snprintf(err, errSize, "install/init.sql missing or empty");
		goto done;
	}
	if (sqlite3_exec(db, script, NULL, NULL, &serr) != SQLITE_OK) {
		snprintf(err, errSize, "init.sql exec failed: %s", serr ? serr : "?");
		goto done;
	}
	ok = true;
done:
	if (serr) sqlite3_free(serr);
	if (db) sqlite3_close(db);
	xrtFree(dbPath);
	return ok;
}

/* 创建超管：role=1 / authLevel=999（v1 同语义），随机 salt + 服务端二次哈希。 */
static bool Install_CreateAdmin(const char* user, const char* clientHash, char* err, size_t errSize)
{
	str salt = Util_Token();
	str pwd = salt ? ServerHashPassword(user, salt, clientHash) : NULL;
	xtime now = xrtNow();
	sqlite3_stmt* stmt = NULL;
	bool ok = false;

	if (!salt || !pwd) { snprintf(err, errSize, "hash material failed"); goto done; }
	if (sqlite3_prepare_v2(G_DB,
		"INSERT INTO user (user, salt, pwd, role, authLevel, createTime, updateTime, isDelete) "
		"VALUES (?, ?, ?, 1, 999, ?, ?, 0);", -1, &stmt, NULL) != SQLITE_OK) {
		snprintf(err, errSize, "prepare failed: %s", sqlite3_errmsg(G_DB));
		goto done;
	}
	sqlite3_bind_text(stmt, 1, user, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 2, salt, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, pwd, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int64(stmt, 4, now);
	sqlite3_bind_int64(stmt, 5, now);
	if (sqlite3_step(stmt) != SQLITE_DONE) {
		snprintf(err, errSize, "admin insert failed: %s", sqlite3_errmsg(G_DB));
		sqlite3_finalize(stmt);
		goto done;
	}
	sqlite3_finalize(stmt);
	ok = true;
done:
	xrtFree(salt);
	xrtFree(pwd);
	return ok;
}

static void Install_ReplyJSON(XAdminRequest* req, int code, bool result, const char* message)
{
	xvalue* reply = ValueObject(); size_t size = 0;
	ValueSetBool(reply, "result", result);
	ValueSetText(reply, "message", message);
	if (result) ValueSetText(reply, "loginPath", Option_GetAdminLoginPath());
	char* body = xrtJsonStringify(reply, false, &size);
	xsHttpReplyAuto(req, code, HTTP_CT_JSON, body, size);
	xrtFree(body);
	xrtValueRelease(reply);
}

/* 由 Protocol 在向导模式调用：接管一切请求。host 与 ServiceInit 收到的是同一
 * 宿主对象（请求视图侧为 const），安装链内沿用原指针。 */
static void Install_RequestWizard(const XS_HostInfo* host, XAdminRequest* req)
{
	char err[256] = {0};
	xvalue* form = NULL;
	str user = NULL; str clientHash = NULL;
	char* lockPath;

	if (xsReqMethodID(req) == XHTTP_METHOD_GET || xsReqMethodID(req) == XHTTP_METHOD_HEAD) {
		LoadPage(req, 200, HTTP_CT_HTML, "install.html");
		return;
	}
	if (xsReqMethodID(req) != XHTTP_METHOD_POST) {
		LoadPage(req, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}
	if (G_InstallBusy) {
		Install_ReplyJSON(req, 409, false, "installation already in progress");
		return;
	}
	G_InstallBusy = true;

	form = JsonParseN(req->body, req->body_size);
	user = ValueText(form, "username");
	clientHash = ValueText(form, "password");
	if (!Install_ValidUsername(user) || !Install_IsHex64(clientHash)) {
		G_InstallBusy = false;
		xrtValueRelease(form);
		Install_ReplyJSON(req, 400, false, "invalid username or password hash");
		return;
	}
	/* step 1：建库（幂等——已存在则跳过，重试路径） */
	{
		char* dbFile = xrtPathJoin(DBPath, "main.db");
		bool exists = xrtPathExists(dbFile);
		xrtFree(dbFile);
		if (!exists && !Install_CreateDatabase(err, sizeof(err))) {
			G_InstallBusy = false;
			xrtValueRelease(form);
			Install_ReplyJSON(req, 500, false, err);
			return;
		}
	}
	/* step 2：业务段启动（幂等——成功过则跳过） */
	if (!G_BusinessStarted && !XAdmin_BusinessStart((XS_HostInfo*)host)) {
		G_InstallBusy = false;
		xrtValueRelease(form);
		Install_ReplyJSON(req, 500, false, "business start failed after install");
		return;
	}
	/* step 3：创建超管 */
	if (!Install_CreateAdmin(user, clientHash, err, sizeof(err))) {
		G_InstallBusy = false;
		xrtValueRelease(form);
		Install_ReplyJSON(req, 500, false, err);
		return;
	}
	/* step 4：写安装锁并退出向导模式 */
	lockPath = xrtPathJoin(AppPath, "install.lock");
	if (!xrtFileWriteAtomic(lockPath, (xbytesview){(cbytes)"xadmin installed", 17})) {
		xrtFree(lockPath);
		G_InstallBusy = false;
		xrtValueRelease(form);
		Install_ReplyJSON(req, 500, false, "write install.lock failed");
		return;
	}
	xrtFree(lockPath);
	G_Install = true;
	G_InstallMode = false;
	G_InstallBusy = false;
	xrtValueRelease(form);
	printf("[xadmin] installed; administrator created\n");
	Install_ReplyJSON(req, 200, true, "xadmin 安装成功，即将进入登录页");
}
