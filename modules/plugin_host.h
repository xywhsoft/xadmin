/* 插件宿主内核（xs3 重写，参考 v1 ps_* 语义；ABI v4 冻结）。
 * 职责：扫描/路径转换/manifest 校验/配置装载、TCC 编译与代际生命周期、
 * 27 个 XAdmin_* 宿主函数（路由/菜单/权限/URI/事件/钩子/服务）、
 * plugin_resource 台账登记与回收。DB 为唯一事实源：TCC 状态随主脚本
 * 代际销毁，启动时按 plugin_runtime.enabled 重建。
 * 全部操作发生在 G_RequestLock 内（含管理路由与插件 OnStart）。 */

#include <ctype.h>
#define XS_PLUGIN_HOST_SIDE
#include "../plugin_sdk/xs_plugin.h"

/* multipart ABI 出口（modules/multipart.h 的插件侧包装）：
 * 视图借用请求正文缓冲，原地解码与宿主附件上传同一实现；
 * 逐字段拷贝避免 SDK 结构与内部结构布局耦合。 */
static bool XAdmin_MultipartBoundary(const char* sContentType, char* sOut, size_t iCap)
{
	return MultipartBoundary(sContentType, sOut, iCap);
}

static bool XAdmin_MultipartNext(const char* sBody, size_t iBodySize, const char* sBoundary,
	size_t iBoundaryLen, size_t* pOffset, XAdminMultipartPart* pOut)
{
	MultipartPart part;
	if (!MultipartNext(sBody, iBodySize, sBoundary, iBoundaryLen, pOffset, &part))
		return false;
	if (pOut) {
		pOut->name = part.name;
		pOut->nameLen = part.nameLen;
		pOut->filename = part.filename;
		pOut->filenameLen = part.filenameLen;
		pOut->data = part.data;
		pOut->size = part.size;
	}
	return true;
}

static bool PluginHost_SetEnabled(const char* sXid, bool bEnable);
static bool PluginHost_Reload(const char* sXid);

#define PLUGIN_MAX        32
#define PLUGIN_NAME_MAX   64
#define PLUGIN_PATH_MAX   420
#define PLUGIN_ROUTES_MAX 256 /* 生成插件（cms.article）注册 220+ 路由 */
#define PLUGIN_DYN_MAX    16
#define PLUGIN_HOST_VERSION "4.3.0" /* additive SDK APIs; ABI layout stays v4 */
#define PLUGIN_EVT_MAX    128
#define PLUGIN_HOOK_MAX   128
#define PLUGIN_SVC_MAX    32
#define PLUGIN_HOOK_CHAIN 16

typedef struct PluginInstance {
	char xid[PLUGIN_NAME_MAX];
	char rootPath[PLUGIN_PATH_MAX];   /* AppPath/plugin/<xid> */
	char dataPath[PLUGIN_PATH_MAX];   /* AppPath/plugin_data/<xid> */
	char dbPath[PLUGIN_PATH_MAX];     /* AppPath/db/plugin/<xid>/plugin.db */
	char pageDir[32];                 /* manifest resources.page，缺省 page */
	char templateDir[32];             /* manifest resources.template，缺省 template */
	char optionDir[32];               /* manifest resources.option，缺省 option */
	char staticDir[32];               /* manifest resources.static，缺省 static */
	bool allowSourceMap;              /* resources.allowSourceMap / staticAllowSourceMap */
	xvalue* manifest;                 /* plugin.json（扫描时装载，实例存续期持有） */
	xvalue* config;                   /* 扁平配置表（GLOBAL_OPTION_TABLE 借出） */
	TCCState* tcc;
	const XAdminPluginDescriptor* desc;
	void (*setGlobal)(int idx, void* ptr);
	sqlite3_int64 genRowId;
	int generation;
	int activeLeases; /* 未归还的服务租借数：>0 时代际停用不得销毁代码镜像 */
	int activeIo; /* unlocked outbound calls; disable/reload must reject while nonzero */
	bool started;
	bool stopping; /* unlocked channel join: reject concurrent restart/unload */
	str routePaths[PLUGIN_ROUTES_MAX]; /* RouteInfo.Path 指针的所有权在实例 */
	size_t routeCount;
	str dynPatterns[PLUGIN_DYN_MAX];   /* 本插件注册的动态路由 pattern（所有权在实例） */
	size_t dynCount;
	XAdminHostContext hostContext;     /* 聚合宿主上下文（注入槽位 7；实例存续期有效） */
} PluginInstance;

static XAdminHostContext G_PluginHostContextTemplate; /* 宿主公共段，业务启动时填充一次 */

static PluginInstance G_Plugins[PLUGIN_MAX];
static size_t G_PluginCount;
static int G_PluginRegIdx = -1; /* 正在执行 OnStart 的实例下标，注册函数归属判定 */

typedef struct {
	char name[PLUGIN_NAME_MAX];
	XAdminEventProc proc;
	int pluginIdx; /* -1 = 空槽 */
	bool used;
} PluginEventSlot;
static PluginEventSlot G_PluginEvents[PLUGIN_EVT_MAX];

typedef struct {
	char name[PLUGIN_NAME_MAX];
	XAdminHookProc proc;
	int sort;
	int pluginIdx;
	bool used;
} PluginHookSlot;
static PluginHookSlot G_PluginHooks[PLUGIN_HOOK_MAX];

typedef struct {
	char name[PLUGIN_NAME_MAX];
	int major, minor;
	int pluginIdx;
	const void* vtable;
	bool used;
} PluginServiceSlot;
static PluginServiceSlot G_PluginServices[PLUGIN_SVC_MAX];

/* GR1：租借改为堆单元——记录获取时的提供方代码镜像。
 * 提供方换代/停用后 ReleaseService 按 tcc 指针路由到退役队列，
 * 最后一个租借归还时才真正销毁镜像（v1 lease/DRAINING 语义的对齐实现）。 */
typedef struct PluginServiceLeaseCell {
	TCCState* tcc;
	struct PluginInstance* provider;
} PluginServiceLeaseCell;

#define PLUGIN_TCC_RETIRE_MAX 16
typedef struct {
	TCCState* tcc;
	int leases;
	bool used;
} PluginTccRetire;
static PluginTccRetire G_PluginTccRetired[PLUGIN_TCC_RETIRE_MAX];

/* ==================== 路径与 DB 基础 ==================== */

static PluginInstance* Plugin_Find(const char* sXid)
{
	size_t i;
	if (!sXid) return NULL;
	for (i = 0; i < G_PluginCount; i++)
		if (!strcmp(G_Plugins[i].xid, sXid)) return &G_Plugins[i];
	return NULL;
}

/* Only application composition (e.g. tests) may inject a transport. No HTTP
 * route can set an upstream URL, private CA or callback. */
typedef bool (*XAPluginHttpTransport)(void* engine, const XAHttpRequest* request,
    unsigned timeout_ms, size_t max_body, int* status, char** response);
static XAPluginHttpTransport G_PluginHttpTransport;
static int XAdmin_HttpPostJson(XAdminPluginHandle handle, XS_RequestObject req,
    const char* url, const char* bearer, const char* json, unsigned timeout_ms,
    size_t max_response, int* status, char** response)
{
    PluginInstance* inst = NULL; size_t i;
    if (!status || !response) return -1;
    *status = 0; *response = NULL;
    for (i = 0; i < G_PluginCount; i++) if (handle == &G_Plugins[i]) inst = &G_Plugins[i];
    if (!inst || !inst->started || G_PluginRegIdx >= 0 || !req || !req->raw ||
        timeout_ms < 100 || timeout_ms > 60000 || !max_response || max_response > 1048576 ||
        !bearer || !*bearer || strlen(bearer) > 1000) return -1;
    XAHttpRequest request = {0}; char authorization[1025];
    snprintf(authorization, sizeof(authorization), "Bearer %s", bearer);
    bool ok = XA_HttpsHttpBody(&request, url, "application/json", json) &&
        XA_HttpsHttpHeader(&request, "Authorization", authorization);
    xrtSecureZero(authorization, sizeof(authorization));
    if (!ok) { XA_HttpsHttpUnit(&request); return -1; }
    void* engine = req->raw->server->Engine;
    XAPluginHttpTransport transport = G_PluginHttpTransport;
    xdeadline deadline = xrtDeadlineAfter((uint64)timeout_ms * 1000);
    inst->activeIo++;
    xrtMutexUnlock(G_RequestLock);
    ok = transport ? transport(engine, &request, timeout_ms, max_response, status, response) :
        XA_HttpsHttp(engine, NULL, &request, timeout_ms, max_response, status, response);
    xrtMutexLock(G_RequestLock);
    inst->activeIo--;
    XA_HttpsHttpUnit(&request);
    if (!ok || !*response || strlen(*response) > max_response) {
        if (*response) xrtSecureZero(*response, strlen(*response));
        xrtFree(*response); *response = NULL; *status = 0;
        return !xrtDeadlineRemaining(deadline) ? -2 : -1;
    }
    return 0;
}
static int XAdmin_MemberContactStatus(xvalue* session)
{
    sqlite3_stmt* stmt = NULL; int result = -1;
    if (!session || ValueInt(session, "id") <= 0) return -1;
    if (sqlite3_prepare_v2(G_DB, "SELECT phone_verified_at,email_verified_at FROM member WHERE id=? AND status=1 AND isDelete=0", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, ValueInt(session, "id"));
        if (sqlite3_step(stmt) == SQLITE_ROW) result =
            (sqlite3_column_int64(stmt, 0) > 0 ? 1 : 0) | (sqlite3_column_int64(stmt, 1) > 0 ? 2 : 0);
    }
    sqlite3_finalize(stmt); return result;
}
static const char* XAdmin_AdminCSRFToken(xvalue* session)
{
    if (!session || ValueInt(session, "id") <= 0) return NULL;
    const char* token = ValueText(session, "_pluginCSRF");
    if (!XA_IsHex(token, 64)) {
        char value[65];
        if (!XA_Random(value) || !ValueSetText(session, "_pluginCSRF", value)) return NULL;
        token = ValueText(session, "_pluginCSRF");
    }
    return token;
}
static bool XAdmin_CheckAdminCSRF(XS_RequestObject req, xvalue* session)
{
    if (!req || !req->raw || !req->raw->head || !session) return false;
    bool bad = false; const xhttpfield* field = XA_Header(req, "X-CSRF-Token", &bad);
    const char* token = ValueText(session, "_pluginCSRF");
    return req && XA_SameOrigin(req) && !bad && field && field->Value.Size == 64 &&
        XA_IsHex(token, 64) && xrtConstTimeEqual(field->Value.Data, token, 64);
}

#include "../src/net/plugin_async.c"
#include "../src/net/plugin_channel.c"

static bool Plugin_XidValid(const char* sXid)
{
	size_t i, len;
	if (!sXid || !(len = strlen(sXid)) || len >= PLUGIN_NAME_MAX) return false;
	if (!(isalnum((unsigned char)sXid[0]))) return false;
	for (i = 0; i < len; i++)
		if (!(isalnum((unsigned char)sXid[i]) || sXid[i] == '-' || sXid[i] == '_' || sXid[i] == '.'))
			return false;
	return true;
}

/* ==================== 插件资源体系（v1 ps_resource 对齐） ==================== */

static PluginInstance* Plugin_Caller(void); /* 定义在本文件后段（注册归属判定） */

#define PLUGIN_STATIC_PREFIX "/plugin-static/"
#define PLUGIN_STATIC_MAX_FILE_SIZE (16u * 1024u * 1024u)

/* 相对资源路径安全检查：拒绝绝对路径、盘符冒号、..、百分号、反斜杠与控制字符。 */
static bool Plugin_ResourceIsSafeRelativePath(const char* sRelPath)
{
	const char* p;
	if (!sRelPath || !sRelPath[0]) return false;
	if (sRelPath[0] == '/' || sRelPath[0] == '\\') return false;
	if (strchr(sRelPath, ':')) return false;
	if (strstr(sRelPath, "..")) return false;
	if (strchr(sRelPath, '%')) return false;
	for (p = sRelPath; *p; p++)
		if (*p == '\\' || ((unsigned char)*p) < 32) return false;
	return true;
}

/* manifest resources.<dir> 装载校验：不合法配置回退约定目录名。 */
static const char* Plugin_ResourceDirName(const char* sConfigured, const char* sFallback)
{
	if (sConfigured && sConfigured[0] && strlen(sConfigured) < 32 && Plugin_ResourceIsSafeRelativePath(sConfigured))
		return sConfigured;
	return sFallback;
}

static int Plugin_AsciiLower(int ch)
{
	if (ch >= 'A' && ch <= 'Z') return ch + ('a' - 'A');
	return ch;
}

static bool Plugin_ExtEquals(const char* sLeft, const char* sRight)
{
	if (!sLeft || !sRight) return false;
	while (*sLeft && *sRight) {
		if (Plugin_AsciiLower((unsigned char)*sLeft) != Plugin_AsciiLower((unsigned char)*sRight)) return false;
		sLeft++; sRight++;
	}
	return (*sLeft == '\0') && (*sRight == '\0');
}

/* /plugin-static/ MIME 白名单（与 v1 一致的 17 类扩展名）。 */
static const char* Plugin_StaticMimeByPath(const char* sPath)
{
	const char* sExt;
	if (!sPath) return NULL;
	sExt = strrchr(sPath, '.');
	if (!sExt) return NULL;
	if (Plugin_ExtEquals(sExt, ".css")) return "text/css; charset=utf-8";
	if (Plugin_ExtEquals(sExt, ".html") || Plugin_ExtEquals(sExt, ".htm")) return "text/html; charset=utf-8";
	if (Plugin_ExtEquals(sExt, ".js") || Plugin_ExtEquals(sExt, ".mjs")) return "application/javascript; charset=utf-8";
	if (Plugin_ExtEquals(sExt, ".svg")) return "image/svg+xml";
	if (Plugin_ExtEquals(sExt, ".png")) return "image/png";
	if (Plugin_ExtEquals(sExt, ".jpg") || Plugin_ExtEquals(sExt, ".jpeg")) return "image/jpeg";
	if (Plugin_ExtEquals(sExt, ".gif")) return "image/gif";
	if (Plugin_ExtEquals(sExt, ".webp")) return "image/webp";
	if (Plugin_ExtEquals(sExt, ".ico")) return "image/x-icon";
	if (Plugin_ExtEquals(sExt, ".woff")) return "font/woff";
	if (Plugin_ExtEquals(sExt, ".woff2")) return "font/woff2";
	if (Plugin_ExtEquals(sExt, ".ttf")) return "font/ttf";
	if (Plugin_ExtEquals(sExt, ".eot")) return "application/vnd.ms-fontobject";
	if (Plugin_ExtEquals(sExt, ".map")) return "application/json; charset=utf-8";
	return NULL;
}

static bool Plugin_IsSourceMapPath(const char* sPath)
{
	const char* sExt = sPath ? strrchr(sPath, '.') : NULL;
	return Plugin_ExtEquals(sExt, ".map");
}

/* 文件名段含 >=8 位连续十六进制（可被 .-_ 分隔）视为内容寻址版本文件，
 * 可用 immutable 长缓存；否则 no-cache。 */
static bool Plugin_StaticLooksVersioned(const char* sRelPath)
{
	const char* sExt; const char* sDot; const char* p; int iHex = 0;
	if (!sRelPath) return false;
	sExt = strrchr(sRelPath, '.');
	if (!sExt) return false;
	sDot = sExt;
	while (sDot > sRelPath && sDot[-1] != '/') sDot--;
	for (p = sDot; p < sExt; p++) {
		if (*p == '.' || *p == '-' || *p == '_') { iHex = 0; continue; }
		if (((*p >= '0') && (*p <= '9')) || ((*p >= 'a') && (*p <= 'f')) || ((*p >= 'A') && (*p <= 'F'))) {
			if (++iHex >= 8) return true;
		} else {
			iHex = 0;
		}
	}
	return false;
}

/* ETag/Last-Modified/Cache-Control 响应头（v1 "p-mtime-size" 语义）。 */
static char* Plugin_StaticBuildHeader(const char* sContentType, const char* sFilePath, const char* sRelPath)
{
	xfileinfo info; char* sModifiedText; const char* sCacheControl; char* sHeader;
	if (!sFilePath || !xrtPathStat(sFilePath, true, &info) || info.Type != XFILE_TYPE_FILE) return NULL;
	sModifiedText = TimeText(info.Changed, TIME_TEXT_DATETIME);
	sCacheControl = Plugin_StaticLooksVersioned(sRelPath)
		? "public, max-age=31536000, immutable"
		: "no-cache";
	sHeader = xrtFormat(
		"Content-Type: %s\r\nCache-Control: %s\r\nETag: \"p-%lld-%llu\"\r\nLast-Modified: %s\r\n",
		sContentType ? sContentType : "application/octet-stream",
		sCacheControl,
		(long long)info.Changed,
		(unsigned long long)info.Size,
		sModifiedText ? sModifiedText : "1970-01-01 00:00:00");
	xrtFree(sModifiedText);
	return sHeader;
}

static void Plugin_ReplyStatic404(XS_ResponseObject objResp)
{
	xsHttpReplyAuto(objResp, 404, HTTP_CT_HTML, "<h1>404</h1>", 0);
}

/* /plugin-static/<xid>/<相对路径> 静态文件服务：仅 GET/HEAD，仅已启动插件，
 * MIME 白名单，sourcemap 门控，16MB 上限；前缀命中后一律自行应答。 */
static bool Plugin_TryServeStatic(const char* sPath, XAdminRequest* req)
{
	const char* sCursor; const char* sSlash; const char* sRelPath; const char* sMime;
	char sXid[PLUGIN_NAME_MAX]; size_t iXidLen; PluginInstance* inst;
	char* sFilePath; char* sHeader; size_t iSize = 0; bytes pData;
	if (!sPath || strncmp(sPath, PLUGIN_STATIC_PREFIX, strlen(PLUGIN_STATIC_PREFIX)) != 0) return false;
	if (xsReqMethodID(req) != XHTTP_METHOD_GET && xsReqMethodID(req) != XHTTP_METHOD_HEAD) {
		Plugin_ReplyStatic404(req);
		return true;
	}
	sCursor = sPath + strlen(PLUGIN_STATIC_PREFIX);
	sSlash = strchr(sCursor, '/');
	if (!sSlash) { Plugin_ReplyStatic404(req); return true; }
	iXidLen = (size_t)(sSlash - sCursor);
	if (iXidLen == 0 || iXidLen >= sizeof(sXid)) { Plugin_ReplyStatic404(req); return true; }
	memcpy(sXid, sCursor, iXidLen);
	sXid[iXidLen] = '\0';
	sRelPath = sSlash + 1;
	if (!Plugin_ResourceIsSafeRelativePath(sXid) || !Plugin_ResourceIsSafeRelativePath(sRelPath)) {
		Plugin_ReplyStatic404(req);
		return true;
	}
	inst = Plugin_Find(sXid);
	if (!inst || !inst->started) { Plugin_ReplyStatic404(req); return true; }
	sFilePath = xrtPathJoin(xrtPathJoin(inst->rootPath, inst->staticDir), sRelPath);
	if (!sFilePath) { Plugin_ReplyStatic404(req); return true; }
	sMime = Plugin_StaticMimeByPath(sFilePath);
	if (!sMime || (Plugin_IsSourceMapPath(sFilePath) && !inst->allowSourceMap)) {
		xrtFree(sFilePath);
		Plugin_ReplyStatic404(req);
		return true;
	}
	sHeader = Plugin_StaticBuildHeader(sMime, sFilePath, sRelPath);
	if (!sHeader) {
		xrtFree(sFilePath);
		Plugin_ReplyStatic404(req);
		return true;
	}
	if (xsReqMethodID(req) == XHTTP_METHOD_HEAD) {
		xsHttpReplyAuto(req, 200, sHeader, "", 0);
		xrtFree(sHeader); xrtFree(sFilePath);
		return true;
	}
	pData = xrtFileReadAll(sFilePath, &iSize);
	if (!pData || iSize > PLUGIN_STATIC_MAX_FILE_SIZE) {
		xrtFree(pData); xrtFree(sHeader); xrtFree(sFilePath);
		Plugin_ReplyStatic404(req);
		return true;
	}
	xsHttpReplyAuto(req, 200, sHeader, pData, iSize);
	xrtFree(pData); xrtFree(sHeader); xrtFree(sFilePath);
	return true;
}

/* ---- 资源 ABI（插件侧 SDK 函数） ---- */

static PluginInstance* Plugin_ResourceResolve(XAdminPluginHandle plugin_handle)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	return (inst && inst->started) ? inst : NULL;
}

int XAdmin_LoadPluginPage(XAdminPluginHandle plugin_handle, XS_ResponseObject objResp, int iCode, const char* sHeader, const char* sPage)
{
	PluginInstance* inst = Plugin_ResourceResolve(plugin_handle);
	char* sFilePath; size_t iSize = 0; bytes pData;
	if (!inst || !objResp || !sPage) return 0;
	if (!Plugin_ResourceIsSafeRelativePath(sPage)) {
		Plugin_ReplyStatic404(objResp);
		return 1;
	}
	sFilePath = xrtPathJoin(xrtPathJoin(inst->rootPath, inst->pageDir), sPage);
	if (!sFilePath || !(pData = xrtFileReadAll(sFilePath, &iSize)) || iSize > PLUGIN_STATIC_MAX_FILE_SIZE) {
		xrtFree(pData); xrtFree(sFilePath);
		Plugin_ReplyStatic404(objResp);
		return 1;
	}
	xsHttpReplyAuto(objResp, iCode, sHeader ? sHeader : HTTP_CT_HTML, pData, iSize);
	xrtFree(pData); xrtFree(sFilePath);
	return 1;
}

char* XAdmin_RenderPluginTemplate(XAdminPluginHandle plugin_handle, const char* sTemplate, xvalue* tblData, size_t* pRetSize, char** psError)
{
	PluginInstance* inst = Plugin_ResourceResolve(plugin_handle);
	char* sDir; char* sFilePath; char* sSource; size_t iLen = 0; char* sOutput;
	xtemplateconfig config; xtemplate* compiled;
	if (psError) *psError = NULL;
	if (pRetSize) *pRetSize = 0;
	if (!inst || !sTemplate || !sTemplate[0] || !Plugin_ResourceIsSafeRelativePath(sTemplate)) {
		if (psError) *psError = xrtStrDup("template name required");
		return NULL;
	}
	sDir = xrtPathJoin(inst->rootPath, inst->templateDir);
	sFilePath = sDir ? xrtPathJoin(sDir, sTemplate) : NULL;
	sSource = sFilePath ? (char*)xrtFileReadAll(sFilePath, &iLen) : NULL;
	if (sSource) {
		xrtTemplateConfigInit(&config);
		config.Open = XRT_STR_LITERAL("{{"); config.Close = XRT_STR_LITERAL("}}");
		config.Registry = G_TemplateRegistry;
		compiled = xrtTemplateCompileConfig((xstrview){sSource, iLen}, &config);
		xrtFree(sSource);
		if (!compiled) {
			if (psError) *psError = xrtFormat("plugin template parse failed: %s", sTemplate);
			xrtFree(sFilePath); xrtFree(sDir);
			return NULL;
		}
		sOutput = xrtTemplateRender(compiled, tblData, pRetSize);
		xrtTemplateRelease(compiled);
		xrtFree(sFilePath); xrtFree(sDir);
		if (!sOutput && psError) *psError = xrtFormat("plugin template render failed: %s", sTemplate);
		return sOutput;
	}
	xrtFree(sFilePath); xrtFree(sDir);
	/* v1 回退：插件目录无此模板时走全局模板引擎 */
	sOutput = MakePageWithTemplate(sTemplate, tblData, pRetSize);
	if (!sOutput && psError) *psError = xrtStrDup("template render failed");
	return sOutput;
}

/* 选项装载：数据目录覆盖（plugin_data/<xid>/option/）优先，其次包内 option 目录。 */
static xvalue* Plugin_OptionLoad(PluginInstance* inst, const char* sFileName)
{
	char* sDir; char* sPath; xvalue* tbl;
	sDir = xrtPathJoin(inst->dataPath, "option");
	sPath = sDir ? xrtPathJoin(sDir, sFileName) : NULL;
	if (sPath && xrtPathExists(sPath)) {
		tbl = JsonParseFile(sPath);
		xrtFree(sPath); xrtFree(sDir);
		return tbl;
	}
	xrtFree(sPath); xrtFree(sDir);
	sDir = xrtPathJoin(inst->rootPath, inst->optionDir);
	sPath = sDir ? xrtPathJoin(sDir, sFileName) : NULL;
	tbl = sPath ? JsonParseFile(sPath) : NULL;
	xrtFree(sPath); xrtFree(sDir);
	return tbl;
}

xvalue* XAdmin_PluginOptionLoad(XAdminPluginHandle plugin_handle, const char* sFileName)
{
	PluginInstance* inst = Plugin_ResourceResolve(plugin_handle);
	if (!inst || !Plugin_ResourceIsSafeRelativePath(sFileName)) return NULL;
	return Plugin_OptionLoad(inst, sFileName);
}

/* 选项保存：读现有结构（含数据目录覆盖），按 name 合并表单值进 classList[].options[].value，
 * 原子写入数据目录覆盖文件。 */
int XAdmin_PluginOptionSave(XAdminPluginHandle plugin_handle, const char* sFileName, xvalue* tblForm)
{
	PluginInstance* inst = Plugin_ResourceResolve(plugin_handle);
	xvalue* tblConfig; xvalue* arrClassList; char* sDir; char* sPath; bool bOk;
	if (!inst || !tblForm || !Plugin_ResourceIsSafeRelativePath(sFileName)) return -1;
	tblConfig = Plugin_OptionLoad(inst, sFileName);
	if (!tblConfig) return -1;
	arrClassList = ValueGet(tblConfig, "classList");
	if (arrClassList && xrtValueType(arrClassList) == XVALUE_ARRAY) {
		uint32 i, j;
		for (i = 0; i < ValueCount(arrClassList); i++) {
			xvalue* tblClass = xrtValueArrayGet(arrClassList, i);
			xvalue* arrOptions;
			if (!tblClass || xrtValueType(tblClass) != XVALUE_OBJECT) continue;
			arrOptions = ValueGet(tblClass, "options");
			if (!arrOptions || xrtValueType(arrOptions) != XVALUE_ARRAY) continue;
			for (j = 0; j < ValueCount(arrOptions); j++) {
				xvalue* tblOpt = xrtValueArrayGet(arrOptions, j);
				str sName; xvalue* varNew;
				if (!tblOpt || xrtValueType(tblOpt) != XVALUE_OBJECT) continue;
				sName = ValueText(tblOpt, "name");
				if (!sName) continue;
				varNew = ValueGet(tblForm, sName);
				if (varNew) ValueSetRef(tblOpt, "value", varNew);
			}
		}
	}
	sDir = xrtPathJoin(inst->dataPath, "option");
	sPath = sDir ? xrtPathJoin(sDir, sFileName) : NULL;
	if (!sDir || !sPath) { xrtFree(sDir); xrtFree(sPath); xrtValueRelease(tblConfig); return -1; }
	xrtDirCreateAll(sDir);
	bOk = JsonWriteFile(sPath, tblConfig, true);
	xrtFree(sPath); xrtFree(sDir);
	xrtValueRelease(tblConfig);
	return bOk ? 0 : -1;
}

char* XAdmin_PluginResourcePath(XAdminPluginHandle plugin_handle, const char* sResourceDir, const char* sRelPath)
{
	PluginInstance* inst = Plugin_ResourceResolve(plugin_handle);
	if (!inst || !Plugin_ResourceIsSafeRelativePath(sResourceDir) || !Plugin_ResourceIsSafeRelativePath(sRelPath))
		return NULL;
	return xrtPathJoin(xrtPathJoin(inst->rootPath, sResourceDir), sRelPath);
}

/* ---- configSchema 校验器（v1 PS_ManagerValidateConfigSchemaValue 子集：
 * type/required/properties/items/additionalProperties 递归） ---- */

static bool Plugin_ValidateConfigSchemaValue(const xvalue* schema, const xvalue* value, const char* sPath, char* err, size_t errSize);

static bool Plugin_CheckSchemaType(const xvalue* value, const char* sType, const char* sPath, char* err, size_t errSize)
{
	if (!strcmp(sType, "object")) {
		if (value && xrtValueType(value) != XVALUE_OBJECT) {
			snprintf(err, errSize, "%s: expected object", sPath);
			return false;
		}
	} else if (!strcmp(sType, "array")) {
		if (value && xrtValueType(value) != XVALUE_ARRAY) {
			snprintf(err, errSize, "%s: expected array", sPath);
			return false;
		}
	} else if (!strcmp(sType, "string")) {
		if (value && xrtValueType(value) != XVALUE_STRING) {
			snprintf(err, errSize, "%s: expected string", sPath);
			return false;
		}
	} else if (!strcmp(sType, "boolean")) {
		if (value && xrtValueType(value) != XVALUE_BOOL) {
			snprintf(err, errSize, "%s: expected boolean", sPath);
			return false;
		}
	} else if (!strcmp(sType, "number")) {
		if (value && xrtValueType(value) != XVALUE_INT && xrtValueType(value) != XVALUE_FLOAT) {
			snprintf(err, errSize, "%s: expected number", sPath);
			return false;
		}
	} else if (!strcmp(sType, "integer")) {
		if (value && xrtValueType(value) != XVALUE_INT) {
			snprintf(err, errSize, "%s: expected integer", sPath);
			return false;
		}
	} else if (!strcmp(sType, "null")) {
		if (value && xrtValueType(value) != XVALUE_NULL) {
			snprintf(err, errSize, "%s: expected null", sPath);
			return false;
		}
	}
	return true;
}

static bool Plugin_ValidateConfigSchemaValue(const xvalue* schema, const xvalue* value, const char* sPath, char* err, size_t errSize)
{
	xvalue* type; xvalue* properties; xvalue* required; xvalue* items; xvalue* additional;
	if (!schema || xrtValueType(schema) != XVALUE_OBJECT) return true;
	type = ValueGet(schema, "type");
	if (type && xrtValueType(type) == XVALUE_STRING) {
		if (!Plugin_CheckSchemaType(value, ValueTextOf(type), sPath, err, errSize)) return false;
	}
	if (value && xrtValueType(value) == XVALUE_OBJECT) {
		properties = ValueGet(schema, "properties");
		required = ValueGet(schema, "required");
		additional = ValueGet(schema, "additionalProperties");
		if (properties && xrtValueType(properties) == XVALUE_OBJECT) {
			xvalueiter it = {0}; xvaluekey key;
			if (required && xrtValueType(required) == XVALUE_ARRAY) {
				uint32 i;
				for (i = 0; i < ValueCount(required); i++) {
					str need = ValueTextOf(xrtValueArrayGet(required, i));
					if (need && !ValueGet(value, need)) {
						snprintf(err, errSize, "%s.%s: required", sPath, need);
						return false;
					}
				}
			}
			if (additional && xrtValueType(additional) == XVALUE_BOOL && !ValueBoolOf(additional)) {
				if (xrtValueIterBegin((xvalue*)value, &it)) {
					while (xrtValueIterNext(&it, &key) != NULL) {
						if (key.Type == XVALUE_KEY_STRING && !ValueGet(properties, key.String.Data)) {
							snprintf(err, errSize, "%s.%s: additional property not allowed", sPath, key.String.Data);
							xrtValueIterEnd(&it);
							return false;
						}
					}
					xrtValueIterEnd(&it);
				}
			}
			if (xrtValueIterBegin((xvalue*)value, &it)) {
				while (xrtValueIterNext(&it, &key) != NULL) {
					if (key.Type == XVALUE_KEY_STRING) {
						xvalue* sub = ValueGet(properties, key.String.Data);
						char subPath[160];
						if (sub) {
							snprintf(subPath, sizeof(subPath), "%s.%s", sPath, key.String.Data);
							if (!Plugin_ValidateConfigSchemaValue(sub, ValueGet(value, key.String.Data), subPath, err, errSize)) {
								xrtValueIterEnd(&it);
								return false;
							}
						}
					}
				}
				xrtValueIterEnd(&it);
			}
		}
	}
	if (value && xrtValueType(value) == XVALUE_ARRAY) {
		items = ValueGet(schema, "items");
		if (items && xrtValueType(items) == XVALUE_OBJECT) {
			uint32 i;
			char itemPath[160];
			for (i = 0; i < ValueCount(value); i++) {
				snprintf(itemPath, sizeof(itemPath), "%s[%u]", sPath, i);
				if (!Plugin_ValidateConfigSchemaValue(items, xrtValueArrayGet(value, i), itemPath, err, errSize))
					return false;
			}
		}
	}
	return true;
}

/* 按实例 manifest 的 configSchema 声明装载校验 schema；无文件返回 NULL。 */
static xvalue* Plugin_LoadConfigSchema(PluginInstance* inst)
{
	str sFile = ValueText(inst->manifest, "configSchema");
	char* path;
	xvalue* schema;
	if (!sFile || !sFile[0]) sFile = "config.schema.json";
	if (!Plugin_ResourceIsSafeRelativePath(sFile)) return NULL;
	path = xrtPathJoin(inst->rootPath, sFile);
	schema = path ? JsonParseFile(path) : NULL;
	xrtFree(path);
	return schema;
}

static bool Plugin_Exec(const char* sSql)
{
	return sqlite3_exec(G_DB, sSql, NULL, NULL, NULL) == SQLITE_OK;
}

static void Plugin_BindText(sqlite3_stmt* stmt, int idx, const char* text)
{
	sqlite3_bind_text(stmt, idx, text ? text : "", -1, SQLITE_STATIC);
}

static sqlite3_int64 Plugin_LastRow(void)
{
	return sqlite3_last_insert_rowid(G_DB);
}

/* v1 库中的旧布局路径（hosts/xadmin[/data]/plugin/...）幂等转换为主线布局。 */
static void Plugin_ConvertLegacyPaths(void)
{
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(G_DB, "SELECT package_id, xid FROM plugin_package;", -1, &stmt, NULL) != SQLITE_OK)
		return;
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char* sPkg = (const char*)sqlite3_column_text(stmt, 0);
		const char* sXid = (const char*)sqlite3_column_text(stmt, 1);
		sqlite3_stmt* up = NULL;
		char* sRoot = xrtPathJoin(AppPath, "plugin");
		char* sInstall = xrtPathJoin(sRoot, sXid);
		char* sDataRoot = xrtPathJoin(AppPath, "plugin_data");
		char* sData = xrtPathJoin(sDataRoot, sXid);
		char* sDbRoot = xrtPathJoin(AppPath, "db");
		char* sDbRoot2 = xrtPathJoin(sDbRoot, "plugin");
		char* sDbDir = xrtPathJoin(sDbRoot2, sXid);
		char* sDb = xrtPathJoin(sDbDir, "plugin.db");
		char* sSql;
		(void)sPkg;
		sSql = xrtFormat(
			"UPDATE plugin_package SET install_path='%s' WHERE xid='%s' AND install_path != '%s';"
			"UPDATE plugin_runtime SET data_path='%s', private_db_path='%s' WHERE xid='%s' AND (data_path != '%s' OR private_db_path != '%s');",
			sInstall, sXid, sInstall, sData, sDb, sXid, sData, sDb);
		if (sSql) Plugin_Exec(sSql);
		xrtFree(sSql); xrtFree(sDb); xrtFree(sDbDir); xrtFree(sDbRoot2); xrtFree(sDbRoot);
		xrtFree(sData); xrtFree(sDataRoot); xrtFree(sInstall); xrtFree(sRoot);
		(void)up;
	}
	sqlite3_finalize(stmt);
}

/* ==================== 台账 ==================== */

static void Plugin_LedgerAdd(PluginInstance* inst, const char* sType, const char* sKey, const char* sRef, const char* sPolicy)
{
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(G_DB,
		"INSERT INTO plugin_resource (instance_id, generation, owner_scope, resource_type, resource_key, resource_ref, destroy_policy, create_time, status, xid) "
		"VALUES (NULL, ?, 'generation', ?, ?, ?, ?, ?, 'active', ?);", -1, &stmt, NULL) != SQLITE_OK)
		return;
	sqlite3_bind_int(stmt, 1, inst->generation);
	Plugin_BindText(stmt, 2, sType);
	Plugin_BindText(stmt, 3, sKey);
	Plugin_BindText(stmt, 4, sRef);
	Plugin_BindText(stmt, 5, sPolicy);
	sqlite3_bind_int64(stmt, 6, xrtNow());
	Plugin_BindText(stmt, 7, inst->xid);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

static void Plugin_LedgerRemove(const char* sXid, int iGeneration, const char* sType, const char* sRef)
{
	sqlite3_stmt* stmt = NULL;
	if (sqlite3_prepare_v2(G_DB,
		"UPDATE plugin_resource SET status='removed' WHERE xid=? AND generation=? AND resource_type=? AND resource_ref=? AND status='active';",
		-1, &stmt, NULL) != SQLITE_OK)
		return;
	Plugin_BindText(stmt, 1, sXid);
	sqlite3_bind_int(stmt, 2, iGeneration);
	Plugin_BindText(stmt, 3, sType);
	Plugin_BindText(stmt, 4, sRef);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);
}

static void Plugin_ReloadPermissionCaches(void)
{
	ReloadCache_Auth_Auth();
	ReloadCache_Auth_Group();
	Auth_ReloadCache();
	ReloadCache_MemberAuth();
	ReloadCache_MemberAuthGroup();
	MemberAuth_ReloadCache();
	Auth_UpdateURIS();
}

/* 停止时按台账回收全部资源（v1 destroy_policy 语义）。 */
static void Plugin_CleanupResources(PluginInstance* inst)
{
	sqlite3_stmt* stmt = NULL;
	bool bReload = false;
	if (sqlite3_prepare_v2(G_DB,
		"SELECT resource_type, resource_ref FROM plugin_resource "
		"WHERE xid=? AND generation=? AND status='active';", -1, &stmt, NULL) != SQLITE_OK)
		return;
	Plugin_BindText(stmt, 1, inst->xid);
	sqlite3_bind_int(stmt, 2, inst->generation);
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		const char* sType = (const char*)sqlite3_column_text(stmt, 0);
		const char* sRef = (const char*)sqlite3_column_text(stmt, 1);
		if (!strcmp(sType, "route")) {
			xrtMapRemove(G_StaticRouteTableHTTP, KeyView(sRef));
		} else if (!strcmp(sType, "menu")) {
			Plugin_Exec(xrtFormat("UPDATE menu SET isDelete=1, updateTime=%lld WHERE id=%s;", xrtNow(), sRef));
		} else if (!strcmp(sType, "admin_auth")) {
			Plugin_Exec(xrtFormat("UPDATE auth SET isDelete=1, updateTime=%lld WHERE id=%s;", xrtNow(), sRef));
		} else if (!strcmp(sType, "admin_auth_group")) {
			Plugin_Exec(xrtFormat("UPDATE authGroup SET isDelete=1, updateTime=%lld WHERE id=%s;", xrtNow(), sRef));
		} else if (!strcmp(sType, "member_auth")) {
			Plugin_Exec(xrtFormat("UPDATE memberAuth SET isDelete=1, updateTime=%lld WHERE id=%s;", xrtNow(), sRef));
		} else if (!strcmp(sType, "member_auth_group")) {
			Plugin_Exec(xrtFormat("UPDATE memberAuthGroup SET isDelete=1, updateTime=%lld WHERE id=%s;", xrtNow(), sRef));
		} else if (!strcmp(sType, "admin_uri_auth") || !strcmp(sType, "member_uri_auth")) {
			Plugin_Exec(xrtFormat("DELETE FROM uris WHERE id=%s;", sRef));
		}
		Plugin_LedgerRemove(inst->xid, inst->generation, sType, sRef);
		bReload = true;
	}
	sqlite3_finalize(stmt);
	/* 事件/钩子/服务：按归属清空内存槽 */
	{
		int i;
		for (i = 0; i < PLUGIN_EVT_MAX; i++)
			if (G_PluginEvents[i].used && !strcmp(G_Plugins[G_PluginEvents[i].pluginIdx].xid, inst->xid))
				G_PluginEvents[i].used = false;
		for (i = 0; i < PLUGIN_HOOK_MAX; i++)
			if (G_PluginHooks[i].used && !strcmp(G_Plugins[G_PluginHooks[i].pluginIdx].xid, inst->xid))
				G_PluginHooks[i].used = false;
		for (i = 0; i < PLUGIN_SVC_MAX; i++)
			if (G_PluginServices[i].used && !strcmp(G_Plugins[G_PluginServices[i].pluginIdx].xid, inst->xid))
				G_PluginServices[i].used = false;
	}
	if (bReload) Plugin_ReloadPermissionCaches();
}

/* ==================== ABI：路由/菜单/权限/URI ==================== */

static PluginInstance* Plugin_Caller(void)
{
	if (G_PluginRegIdx < 0 || (size_t)G_PluginRegIdx >= G_PluginCount) return NULL;
	if (!G_Plugins[G_PluginRegIdx].started) return NULL;
	return &G_Plugins[G_PluginRegIdx];
}

int XAdmin_RegisterRoute(XAdminPluginHandle plugin_handle, const XAdminRouteDecl* decl, XAdminRouteToken* token)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	RouteInfo* route;
	bool added = false;
	if (!inst || !inst->started || !decl || !decl->path || decl->path[0] != '/' || !decl->proc)
		return -1;
	if (inst->routeCount == PLUGIN_ROUTES_MAX)
		return -1;
	route = xrtMapGetOrAdd(G_StaticRouteTableHTTP, KeyView(decl->path), &added);
	if (!route) return -1;
	if (added) {
		memset(route, 0, sizeof(*route));
		route->Path = xrtStrDup(decl->path);
		route->bAdmin = route->bAuth = true;
	} else {
		printf("[plugin][warn] duplicate route %s from %s\n", decl->path, inst->xid);
		return -1; /* 重复注册失败：路径必须唯一 */
	}
	route->bAuth = decl->need_auth ? true : false;
	route->bAdmin = decl->admin_only ? true : false;
	route->AuthID = (uint32)decl->auth_id;
	route->AuthLevel = (uint32)decl->auth_level;
	RouteSetMethods(route, XHTTP_METHOD_ANY, (XAdminRouteProc)decl->proc);
	route->PluginOwner = inst;
	inst->routePaths[inst->routeCount++] = (str)route->Path;
	Plugin_LedgerAdd(inst, "route", decl->path, decl->path, "auto_unload");
	/* 需鉴权但未声明专属权限的路由（生成插件核心 CRUD 等）：与应用侧自动收录
	 * 一致以 authID=1（基础后台权限）入 uris，否则任何角色都过不了权限检查 */
	if (decl->need_auth && decl->auth_id <= 0) {
		sqlite3_stmt* u = NULL;
		if (sqlite3_prepare_v2(G_DB,
			"INSERT OR IGNORE INTO uris (authID, uri, [desc], sort, isBackend, needAuth, needLog, keepActive, createTime, updateTime, isPersistent, namespace, plugin_xid, plugin_generation) "
			"VALUES (1, ?, ?, 0, ?, 1, 0, 0, ?, ?, 0, 'plugin', ?, ?);", -1, &u, NULL) == SQLITE_OK) {
			Plugin_BindText(u, 1, decl->path);
			Plugin_BindText(u, 2, inst->xid);
			sqlite3_bind_int(u, 3, decl->admin_only ? 1 : 0);
			sqlite3_bind_int64(u, 4, xrtNow());
			sqlite3_bind_int64(u, 5, xrtNow());
			Plugin_BindText(u, 6, inst->xid);
			sqlite3_bind_int(u, 7, inst->generation);
			sqlite3_step(u);
			sqlite3_finalize(u);
		}
		Plugin_ReloadPermissionCaches();
	}
	if (token) *token = (XAdminRouteToken)(uintptr_t)route;
	return 0;
}

int XAdmin_UnregisterRoute(XAdminRouteToken token)
{
	RouteInfo* route = (RouteInfo*)(uintptr_t)token;
	size_t i, j;
	if (!route || !route->Path) return -1;
	for (i = 0; i < G_PluginCount; i++) {
		PluginInstance* inst = &G_Plugins[i];
		for (j = 0; j < inst->routeCount; j++) {
			/* DictRemove 会释放映射条目，Path 必须先取到本地再使用。 */
			if (inst->routePaths[j] == route->Path) {
				const char* sPath = inst->routePaths[j];
				xrtMapRemove(G_StaticRouteTableHTTP, KeyView(sPath));
				xrtFree((void*)sPath);
				memmove(&inst->routePaths[j], &inst->routePaths[j + 1], (inst->routeCount - j - 1) * sizeof(str));
				inst->routeCount--;
				Plugin_LedgerRemove(inst->xid, inst->generation, "route", sPath);
				return 0;
			}
		}
	}
	return -1;
}

/* ---- 动态（pattern 参数）路由族 ABI：cms.article slug 伪静态等场景 ---- */

int XAdmin_RouteParamCount(void)
{
	return (int)G_PluginRouteParamCount;
}

int XAdmin_RouteParam(int index, char* out_value, size_t out_cap)
{
	size_t n;
	if (index < 0 || (size_t)index >= G_PluginRouteParamCount || !out_value || !out_cap) return -1;
	n = G_PluginRouteParams[index].Size;
	if (n >= out_cap) n = out_cap - 1;
	if (n) memcpy(out_value, G_PluginRouteParams[index].Data, n);
	out_value[n] = '\0';
	return 0;
}

static bool Plugin_DynamicRouteRemove(const char* pattern)
{
	size_t i;
	for (i = 0; i < G_DynamicCount; i++) {
		if (!strcmp(G_DynamicRoutes[i].Path, pattern)) {
			xrtFree((void*)G_DynamicRoutes[i].Path);
			memmove(&G_DynamicRoutes[i], &G_DynamicRoutes[i + 1], (G_DynamicCount - i - 1) * sizeof(RouteInfo));
			G_DynamicCount--;
			return true;
		}
	}
	return false;
}

int XAdmin_RegisterDynamicRoute(XAdminPluginHandle plugin_handle, const XAdminDynamicRouteDecl* decl, XAdminRouteToken* token)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	RouteInfo* route = NULL;
	bool existed = false;
	size_t i;
	xhttpmethod methods;
	if (!inst || !inst->started || !decl || !decl->pattern || decl->pattern[0] != '/' || !decl->proc)
		return -1;
	if (inst->dynCount >= PLUGIN_DYN_MAX)
		return -1;
	for (i = 0; i < G_DynamicCount; i++)
		if (!strcmp(G_DynamicRoutes[i].Path, decl->pattern)) { route = &G_DynamicRoutes[i]; existed = true; break; }
	if (!route) {
		str copy;
		if (G_DynamicCount == ROUTE_DYNAMIC_MAX || !(copy = xrtStrDup(decl->pattern)))
			return -1;
		route = &G_DynamicRoutes[G_DynamicCount++];
		memset(route, 0, sizeof(*route));
		route->Path = copy;
		route->bAdmin = route->bAuth = true;
	}
	route->bAuth = decl->need_auth ? true : false;
	route->bAdmin = decl->admin_only ? true : false;
	route->AuthID = (uint32)decl->auth_id;
	route->AuthLevel = (uint32)decl->auth_level;
	route->bPutLog = decl->need_log ? true : false;
	route->bActive = decl->keep_active ? true : false;
	methods = decl->method ? (xhttpmethod)decl->method : XHTTP_METHOD_ANY;
	RouteSetMethods(route, methods, (XAdminRouteProc)decl->proc);
	route->PluginOwner = inst;
	/* 运行时注册须立即重编译 pattern 树；失败回滚本次添加 */
	if (!RouteHTTP_RecompileDynamic()) {
		if (!existed) Plugin_DynamicRouteRemove(decl->pattern);
		return -1;
	}
	if (!existed)
		inst->dynPatterns[inst->dynCount++] = xrtStrDup(decl->pattern);
	Plugin_LedgerAdd(inst, "route", decl->path ? decl->path : decl->pattern, decl->pattern, "auto_unload");
	if (token) *token = (XAdminRouteToken)(uintptr_t)route;
	return 0;
}

int XAdmin_RegisterMenu(XAdminPluginHandle plugin_handle, const XAdminMenuDecl* decl, int* out_menu_id, XAdminMenuToken* token)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	sqlite3_stmt* stmt = NULL;
	int id;
	if (!inst || !inst->started || !decl || !decl->title) return -1;
	/* 幂等 upsert（v1 PS_HostFindMenuId 语义）：同插件同 href（无 href 时同
	 * parent+title+type）复用并复活旧行，避免反复 reload 累积重复菜单 */
	id = 0;
	if (decl->href && decl->href[0]) {
		if (sqlite3_prepare_v2(G_DB, "SELECT id FROM menu WHERE plugin_xid=? AND href=? ORDER BY id DESC LIMIT 1;", -1, &stmt, NULL) == SQLITE_OK) {
			Plugin_BindText(stmt, 1, inst->xid);
			Plugin_BindText(stmt, 2, decl->href);
			if (sqlite3_step(stmt) == SQLITE_ROW) id = sqlite3_column_int(stmt, 0);
			sqlite3_finalize(stmt);
		}
	} else {
		if (sqlite3_prepare_v2(G_DB, "SELECT id FROM menu WHERE plugin_xid=? AND parent=? AND title=? AND type=? ORDER BY id DESC LIMIT 1;", -1, &stmt, NULL) == SQLITE_OK) {
			Plugin_BindText(stmt, 1, inst->xid);
			sqlite3_bind_int(stmt, 2, decl->parent_id);
			Plugin_BindText(stmt, 3, decl->title);
			sqlite3_bind_int(stmt, 4, decl->type);
			if (sqlite3_step(stmt) == SQLITE_ROW) id = sqlite3_column_int(stmt, 0);
			sqlite3_finalize(stmt);
		}
	}
	if (id > 0) {
		if (sqlite3_prepare_v2(G_DB,
			"UPDATE menu SET parent=?, title=?, icon=?, type=?, openType=?, href=?, sort=?, visible=?, remark=?, updateTime=?, isDelete=0, plugin_xid=?, plugin_generation=? WHERE id=?;",
			-1, &stmt, NULL) != SQLITE_OK)
			return -1;
		sqlite3_bind_int(stmt, 1, decl->parent_id);
		Plugin_BindText(stmt, 2, decl->title);
		Plugin_BindText(stmt, 3, decl->icon);
		sqlite3_bind_int(stmt, 4, decl->type);
		Plugin_BindText(stmt, 5, decl->open_type);
		Plugin_BindText(stmt, 6, decl->href);
		sqlite3_bind_int(stmt, 7, decl->sort);
		sqlite3_bind_int(stmt, 8, decl->visible ? 1 : 0);
		Plugin_BindText(stmt, 9, decl->remark);
		sqlite3_bind_int64(stmt, 10, xrtNow());
		Plugin_BindText(stmt, 11, inst->xid);
		sqlite3_bind_int(stmt, 12, inst->generation);
		sqlite3_bind_int(stmt, 13, id);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	} else {
		if (sqlite3_prepare_v2(G_DB,
			"INSERT INTO menu (parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete, plugin_xid, plugin_generation) "
			"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?, ?);", -1, &stmt, NULL) != SQLITE_OK)
			return -1;
		sqlite3_bind_int(stmt, 1, decl->parent_id);
		Plugin_BindText(stmt, 2, decl->title);
		Plugin_BindText(stmt, 3, decl->icon);
		sqlite3_bind_int(stmt, 4, decl->type);
		Plugin_BindText(stmt, 5, decl->open_type);
		Plugin_BindText(stmt, 6, decl->href);
		sqlite3_bind_int(stmt, 7, decl->sort);
		sqlite3_bind_int(stmt, 8, decl->visible ? 1 : 0);
		Plugin_BindText(stmt, 9, decl->remark);
		sqlite3_bind_int64(stmt, 10, xrtNow());
		sqlite3_bind_int64(stmt, 11, xrtNow());
		Plugin_BindText(stmt, 12, inst->xid);
		sqlite3_bind_int(stmt, 13, inst->generation);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		id = (int)Plugin_LastRow();
	}
	if (out_menu_id) *out_menu_id = id;
	Plugin_LedgerAdd(inst, "menu", decl->href ? decl->href : "", xrtFormat("%d", id), "soft_delete");
	if (token) *token = ((XAdminMenuToken)2 << 40) | (uint64)id;
	return 0;
}

int XAdmin_UnregisterMenu(XAdminMenuToken token)
{
	int id = (int)(token & 0xFFFFFFFFFF);
	Plugin_Exec(xrtFormat("UPDATE menu SET isDelete=1, updateTime=%lld WHERE id=%d;", xrtNow(), id));
	return 0;
}

int XAdmin_RegisterAuthGroup(XAdminPluginHandle plugin_handle, const XAdminAuthGroupDecl* decl, int* out_group_id, XAdminAuthGroupToken* token)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	sqlite3_stmt* stmt = NULL;
	const char* sTable = (decl && decl->scope == XADMIN_AUTH_SCOPE_MEMBER) ? "memberAuthGroup" : "authGroup";
	int id;
	if (!inst || !inst->started || !decl || !decl->name) return -1;
	/* 幂等 upsert：同插件同 name 复用并复活旧行 */
	id = 0;
	if (sqlite3_prepare_v2(G_DB, xrtFormat(
		"SELECT id FROM %s WHERE plugin_xid=? AND name=? ORDER BY id DESC LIMIT 1;", sTable),
		-1, &stmt, NULL) == SQLITE_OK) {
		Plugin_BindText(stmt, 1, inst->xid);
		Plugin_BindText(stmt, 2, decl->name);
		if (sqlite3_step(stmt) == SQLITE_ROW) id = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}
	if (id > 0) {
		if (sqlite3_prepare_v2(G_DB, xrtFormat(
			"UPDATE %s SET name=?, [desc]=?, sort=?, updateTime=?, isDelete=0 WHERE id=?;", sTable),
			-1, &stmt, NULL) != SQLITE_OK)
			return -1;
		Plugin_BindText(stmt, 1, decl->name);
		Plugin_BindText(stmt, 2, decl->description);
		sqlite3_bind_int(stmt, 3, decl->sort);
		sqlite3_bind_int64(stmt, 4, xrtNow());
		sqlite3_bind_int(stmt, 5, id);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	} else {
		if (sqlite3_prepare_v2(G_DB, xrtFormat(
			"INSERT INTO %s (name, [desc], sort, createTime, updateTime, isDelete, plugin_xid) VALUES (?, ?, ?, ?, ?, 0, ?);", sTable),
			-1, &stmt, NULL) != SQLITE_OK)
			return -1;
		Plugin_BindText(stmt, 1, decl->name);
		Plugin_BindText(stmt, 2, decl->description);
		sqlite3_bind_int(stmt, 3, decl->sort);
		sqlite3_bind_int64(stmt, 4, xrtNow());
		sqlite3_bind_int64(stmt, 5, xrtNow());
		Plugin_BindText(stmt, 6, inst->xid);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		id = (int)Plugin_LastRow();
	}
	if (out_group_id) *out_group_id = id;
	Plugin_LedgerAdd(inst, (decl->scope == XADMIN_AUTH_SCOPE_MEMBER) ? "member_auth_group" : "admin_auth_group",
		decl->key ? decl->key : decl->name, xrtFormat("%d", id), "soft_delete");
	if (token) *token = ((XAdminAuthGroupToken)3 << 40) | (uint64)id;
	Plugin_ReloadPermissionCaches();
	return 0;
}

int XAdmin_UnregisterAuthGroup(XAdminAuthGroupToken token)
{
	int id = (int)(token & 0xFFFFFFFFFF);
	Plugin_Exec(xrtFormat("UPDATE authGroup SET isDelete=1 WHERE id=%d;", id));
	Plugin_Exec(xrtFormat("UPDATE memberAuthGroup SET isDelete=1 WHERE id=%d;", id));
	return 0;
}

int XAdmin_RegisterAuth(XAdminPluginHandle plugin_handle, const XAdminAuthDecl* decl, int* out_auth_id, XAdminAuthToken* token)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	sqlite3_stmt* stmt = NULL;
	const char* sTable = (decl && decl->scope == XADMIN_AUTH_SCOPE_MEMBER) ? "memberAuth" : "auth";
	int id;
	if (!inst || !inst->started || !decl || !decl->name) return -1;
	/* 幂等 upsert：同插件同 name 复用并复活旧行 */
	id = 0;
	if (sqlite3_prepare_v2(G_DB, xrtFormat(
		"SELECT id FROM %s WHERE plugin_xid=? AND name=? ORDER BY id DESC LIMIT 1;", sTable),
		-1, &stmt, NULL) == SQLITE_OK) {
		Plugin_BindText(stmt, 1, inst->xid);
		Plugin_BindText(stmt, 2, decl->name);
		if (sqlite3_step(stmt) == SQLITE_ROW) id = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}
	if (id > 0) {
		if (sqlite3_prepare_v2(G_DB, xrtFormat(
			"UPDATE %s SET groupID=?, name=?, [desc]=?, sort=?, updateTime=?, isDelete=0 WHERE id=?;", sTable),
			-1, &stmt, NULL) != SQLITE_OK)
			return -1;
		sqlite3_bind_int(stmt, 1, decl->group_id);
		Plugin_BindText(stmt, 2, decl->name);
		Plugin_BindText(stmt, 3, decl->description);
		sqlite3_bind_int(stmt, 4, decl->sort);
		sqlite3_bind_int64(stmt, 5, xrtNow());
		sqlite3_bind_int(stmt, 6, id);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	} else {
		if (sqlite3_prepare_v2(G_DB, xrtFormat(
			"INSERT INTO %s (groupID, name, [desc], sort, createTime, updateTime, isDelete, plugin_xid) VALUES (?, ?, ?, ?, ?, ?, 0, ?);", sTable),
			-1, &stmt, NULL) != SQLITE_OK)
			return -1;
		sqlite3_bind_int(stmt, 1, decl->group_id);
		Plugin_BindText(stmt, 2, decl->name);
		Plugin_BindText(stmt, 3, decl->description);
		sqlite3_bind_int(stmt, 4, decl->sort);
		sqlite3_bind_int64(stmt, 5, xrtNow());
		sqlite3_bind_int64(stmt, 6, xrtNow());
		Plugin_BindText(stmt, 7, inst->xid);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		id = (int)Plugin_LastRow();
	}
	if (out_auth_id) *out_auth_id = id;
	Plugin_LedgerAdd(inst, (decl->scope == XADMIN_AUTH_SCOPE_MEMBER) ? "member_auth" : "admin_auth",
		decl->key ? decl->key : decl->name, xrtFormat("%d", id), "soft_delete");
	if (token) *token = ((XAdminAuthToken)4 << 40) | (uint64)id;
	/* v1 GrantDefaultAdminRoleAuth：后台新权限自动授予超管（role 1）。
	 * v3 权限模型无 role 1 旁路，不授予则超管访问新插件管理路由 403。 */
	if (decl->scope == XADMIN_AUTH_SCOPE_ADMIN) {
		str authList = NULL;
		sqlite3_stmt* rt = NULL;
		if (sqlite3_prepare_v2(G_DB, "SELECT authList FROM role WHERE id=1 AND isDelete=0;", -1, &rt, NULL) == SQLITE_OK) {
			if (sqlite3_step(rt) == SQLITE_ROW) authList = (str)sqlite3_column_text(rt, 0);
			/* 借出文本在 UPDATE 前完成解析判断 */
			if (authList && authList[0]) {
				xvalue* arr = JsonParseN(authList, 0);
				bool has = false;
				uint32 i;
				if (arr && xrtValueType(arr) == XVALUE_ARRAY)
					for (i = 0; i < ValueCount(arr); i++)
						if (ValueIntOf(xrtValueArrayGet(arr, i)) == id) { has = true; break; }
				xrtValueRelease(arr);
				if (!has)
					Plugin_Exec(xrtFormat("UPDATE role SET authList = trim(authList, ']') || ', %d]', updateTime=%lld WHERE id=1;", id, xrtNow()));
			}
			sqlite3_finalize(rt);
		}
	}
	Plugin_ReloadPermissionCaches();
	return 0;
}

int XAdmin_UnregisterAuth(XAdminAuthToken token)
{
	int id = (int)(token & 0xFFFFFFFFFF);
	Plugin_Exec(xrtFormat("UPDATE auth SET isDelete=1 WHERE id=%d;", id));
	Plugin_Exec(xrtFormat("UPDATE memberAuth SET isDelete=1 WHERE id=%d;", id));
	return 0;
}

int XAdmin_RegisterUriAuth(XAdminPluginHandle plugin_handle, const XAdminUriAuthDecl* decl, int* out_uri_id, XAdminUriAuthToken* token)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	sqlite3_stmt* stmt = NULL;
	int id;
	bool bAdmin = decl ? (decl->scope == XADMIN_AUTH_SCOPE_ADMIN) : true;
	if (!inst || !inst->started || !decl || !decl->uri || decl->uri[0] != '/') return -1;
	/* 幂等 upsert：uri 全局唯一，已存在则刷新全部标志并归属本插件（此前依赖
	 * UNIQUE 约束吞掉重复 INSERT，行内容不会更新） */
	id = 0;
	if (sqlite3_prepare_v2(G_DB, "SELECT id FROM uris WHERE uri=? ORDER BY id DESC LIMIT 1;", -1, &stmt, NULL) == SQLITE_OK) {
		Plugin_BindText(stmt, 1, decl->uri);
		if (sqlite3_step(stmt) == SQLITE_ROW) id = sqlite3_column_int(stmt, 0);
		sqlite3_finalize(stmt);
	}
	if (id > 0) {
		if (sqlite3_prepare_v2(G_DB,
			"UPDATE uris SET authID=?, [desc]=?, sort=?, isBackend=?, needAuth=?, needLog=?, keepActive=?, updateTime=?, plugin_xid=?, isPersistent=1 WHERE id=?;",
			-1, &stmt, NULL) != SQLITE_OK)
			return -1;
		sqlite3_bind_int(stmt, 1, decl->auth_id);
		Plugin_BindText(stmt, 2, decl->description);
		sqlite3_bind_int(stmt, 3, decl->sort);
		sqlite3_bind_int(stmt, 4, bAdmin ? 1 : 0);
		sqlite3_bind_int(stmt, 5, decl->need_auth ? 1 : 0);
		sqlite3_bind_int(stmt, 6, decl->need_log ? 1 : 0);
		sqlite3_bind_int(stmt, 7, decl->keep_active ? 1 : 0);
		sqlite3_bind_int64(stmt, 8, xrtNow());
		Plugin_BindText(stmt, 9, inst->xid);
		sqlite3_bind_int(stmt, 10, id);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	} else {
		if (sqlite3_prepare_v2(G_DB,
			"INSERT INTO uris (authID, uri, [desc], sort, isBackend, needAuth, needLog, keepActive, createTime, updateTime, plugin_xid, isPersistent) "
			"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);", -1, &stmt, NULL) != SQLITE_OK)
			return -1;
		sqlite3_bind_int(stmt, 1, decl->auth_id);
		Plugin_BindText(stmt, 2, decl->uri);
		Plugin_BindText(stmt, 3, decl->description);
		sqlite3_bind_int(stmt, 4, decl->sort);
		sqlite3_bind_int(stmt, 5, bAdmin ? 1 : 0);
		sqlite3_bind_int(stmt, 6, decl->need_auth ? 1 : 0);
		sqlite3_bind_int(stmt, 7, decl->need_log ? 1 : 0);
		sqlite3_bind_int(stmt, 8, decl->keep_active ? 1 : 0);
		sqlite3_bind_int64(stmt, 9, xrtNow());
		sqlite3_bind_int64(stmt, 10, xrtNow());
		Plugin_BindText(stmt, 11, inst->xid);
		sqlite3_bind_int(stmt, 12, 1);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		id = (int)Plugin_LastRow();
	}
	if (out_uri_id) *out_uri_id = id;
	/* 已注册的同路径路由同步标志，未注册的路由由 Auth_UpdateURIS 收录 */
	{
		RouteInfo* route = xrtMapGet(G_StaticRouteTableHTTP, KeyView(decl->uri));
		if (route) {
			route->AuthID = (uint32)decl->auth_id;
			route->bAuth = decl->need_auth ? true : false;
			route->bAdmin = bAdmin;
			route->bPutLog = decl->need_log ? true : false;
			route->bActive = decl->keep_active ? true : false;
		}
	}
	Plugin_LedgerAdd(inst, bAdmin ? "admin_uri_auth" : "member_uri_auth", decl->uri, xrtFormat("%d", id), "delete");
	if (token) *token = ((XAdminUriAuthToken)5 << 40) | (uint64)id;
	return 0;
}

int XAdmin_UnregisterUriAuth(XAdminUriAuthToken token)
{
	int id = (int)(token & 0xFFFFFFFFFF);
	Plugin_Exec(xrtFormat("DELETE FROM uris WHERE id=%d;", id));
	return 0;
}

/* ==================== ABI：事件 / 钩子 / 服务 ==================== */

int XAdmin_ListenEvent(XAdminPluginHandle plugin_handle, const XAdminEventDecl* decl, XAdminEventToken* token)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	int i;
	if (!inst || !inst->started || !decl || !decl->event_name || !decl->proc) return -1;
	for (i = 0; i < PLUGIN_EVT_MAX; i++) {
		if (!G_PluginEvents[i].used) {
			snprintf(G_PluginEvents[i].name, PLUGIN_NAME_MAX, "%s", decl->event_name);
			G_PluginEvents[i].proc = decl->proc;
			G_PluginEvents[i].pluginIdx = (int)(inst - G_Plugins);
			G_PluginEvents[i].used = true;
			if (token) *token = (XAdminEventToken)(i + 1);
			return 0;
		}
	}
	return -1;
}

int XAdmin_UnlistenEvent(XAdminEventToken token)
{
	if (token == 0 || token > PLUGIN_EVT_MAX) return -1;
	G_PluginEvents[token - 1].used = false;
	return 0;
}

int XAdmin_EmitEvent(XAdminPluginHandle plugin_handle, const char* event_name, void* payload, size_t payload_size)
{
	int i;
	(void)plugin_handle;
	if (!event_name) return -1;
	for (i = 0; i < PLUGIN_EVT_MAX; i++)
		if (G_PluginEvents[i].used && !strcmp(G_PluginEvents[i].name, event_name))
			G_PluginEvents[i].proc(event_name, payload, payload_size);
	return 0;
}

int XAdmin_RegisterHook(XAdminPluginHandle plugin_handle, const XAdminHookDecl* decl, XAdminHookToken* token)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	int i;
	if (!inst || !inst->started || !decl || !decl->hook_name || !decl->proc) return -1;
	for (i = 0; i < PLUGIN_HOOK_MAX; i++) {
		if (!G_PluginHooks[i].used) {
			snprintf(G_PluginHooks[i].name, PLUGIN_NAME_MAX, "%s", decl->hook_name);
			G_PluginHooks[i].proc = decl->proc;
			G_PluginHooks[i].sort = decl->sort;
			G_PluginHooks[i].pluginIdx = (int)(inst - G_Plugins);
			G_PluginHooks[i].used = true;
			if (token) *token = (XAdminHookToken)(i + 1);
			return 0;
		}
	}
	return -1;
}

int XAdmin_UnregisterHook(XAdminHookToken token)
{
	if (token == 0 || token > PLUGIN_HOOK_MAX) return -1;
	G_PluginHooks[token - 1].used = false;
	return 0;
}

int XAdmin_InvokeHook(XAdminPluginHandle plugin_handle, const char* hook_name, void* payload, size_t payload_size)
{
	int order[PLUGIN_HOOK_CHAIN];
	int count = 0, i, j, ret = XADMIN_HOOK_CONTINUE;
	(void)plugin_handle;
	if (!hook_name) return -1;
	for (i = 0; i < PLUGIN_HOOK_MAX && count < PLUGIN_HOOK_CHAIN; i++)
		if (G_PluginHooks[i].used && !strcmp(G_PluginHooks[i].name, hook_name))
			order[count++] = i;
	/* 按 sort 稳定插入排序（小数组） */
	for (i = 1; i < count; i++) {
		int v = order[i];
		for (j = i - 1; j >= 0 && G_PluginHooks[order[j]].sort > G_PluginHooks[v].sort; j--)
			order[j + 1] = order[j];
		order[j + 1] = v;
	}
	for (i = 0; i < count; i++) {
		ret = G_PluginHooks[order[i]].proc(hook_name, payload, payload_size);
		if (ret != XADMIN_HOOK_CONTINUE) break;
	}
	return ret;
}

int XAdmin_RegisterService(XAdminPluginHandle plugin_handle, const XAdminServiceDecl* decl, const void* vtable)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	int i;
	if (!inst || !inst->started || !decl || !decl->service_name || !vtable) return -1;
	for (i = 0; i < PLUGIN_SVC_MAX; i++) {
		if (G_PluginServices[i].used && !strcmp(G_PluginServices[i].name, decl->service_name))
			return -1; /* 重名服务拒绝 */
	}
	for (i = 0; i < PLUGIN_SVC_MAX; i++) {
		if (!G_PluginServices[i].used) {
			snprintf(G_PluginServices[i].name, PLUGIN_NAME_MAX, "%s", decl->service_name);
			G_PluginServices[i].major = decl->major_version;
			G_PluginServices[i].minor = decl->minor_version;
			G_PluginServices[i].pluginIdx = (int)(inst - G_Plugins);
			G_PluginServices[i].vtable = vtable;
			G_PluginServices[i].used = true;
			return 0;
		}
	}
	return -1;
}

int XAdmin_AcquireService(XAdminPluginHandle plugin_handle, const char* name, int major, XAdminServiceLease* out_lease, const void** out_vtable)
{
	int i;
	(void)plugin_handle;
	if (!name || !out_lease || !out_vtable) return -1;
	for (i = 0; i < PLUGIN_SVC_MAX; i++) {
		PluginServiceSlot* svc = &G_PluginServices[i];
		PluginInstance* provider;
		PluginServiceLeaseCell* cell;
		if (!svc->used || strcmp(svc->name, name)) continue;
		if (svc->major != major) continue; /* ABI 仅按主版本协商 */
		provider = &G_Plugins[svc->pluginIdx];
		if (!provider->started || !provider->tcc) return -1;
		cell = xrtMalloc(sizeof(*cell));
		if (!cell) return -1;
		cell->tcc = provider->tcc;
		cell->provider = provider;
		provider->activeLeases++;
		*out_lease = (XAdminServiceLease)cell;
		*out_vtable = svc->vtable;
		return 0;
	}
	return -1;
}

int XAdmin_ReleaseService(XAdminServiceLease lease)
{
	PluginServiceLeaseCell* cell = (PluginServiceLeaseCell*)lease;
	if (!cell) return -1;
	if (cell->provider && cell->provider->tcc == cell->tcc) {
		/* 提供方仍在同一镜像上运行 */
		if (cell->provider->activeLeases > 0) cell->provider->activeLeases--;
	} else {
		/* 提供方已换代/停用：按镜像指针路由到退役队列 */
		int i;
		for (i = 0; i < PLUGIN_TCC_RETIRE_MAX; i++) {
			if (G_PluginTccRetired[i].used && G_PluginTccRetired[i].tcc == cell->tcc) {
				if (--G_PluginTccRetired[i].leases <= 0) {
					xsDestroyTCC(G_PluginTccRetired[i].tcc);
					G_PluginTccRetired[i].used = false;
				}
				break;
			}
		}
	}
	xrtFree(cell);
	return 0;
}

/* ==================== ABI：插件间管理 ==================== */

/* ---- 脚手架生成（v1 PluginSystem_Generate 对齐；内容系统硬依赖） ---- */

static xvalue* Plugin_LoadFlatJson(const char* sPath); /* 定义于编译配置装载段 */

static bool Plugin_GeneratedSpecHasFile(const XAdminGeneratedPluginSpec* spec, const char* rel)
{
	size_t i;
	if (!spec || !rel || !spec->files) return false;
	for (i = 0; i < spec->file_count; i++)
		if (spec->files[i].relative_path && !strcmp(spec->files[i].relative_path, rel)) return true;
	return false;
}

static bool Plugin_WriteGeneratedFile(const char* root, const XAdminGeneratedFile* file)
{
	char* full; char* dir; size_t size; bool ok;
	if (!root || !file || !Plugin_ResourceIsSafeRelativePath(file->relative_path) || !file->data)
		return false;
	full = xrtPathJoin((char*)root, (char*)file->relative_path);
	if (!full) return false;
	dir = xrtPathParent(full);
	if (dir) { xrtDirCreateAll(dir); xrtFree(dir); }
	size = file->size ? file->size : strlen((const char*)file->data);
	ok = xrtFileWriteAtomic(full, (xbytesview){(cbytes)file->data, size});
	xrtFree(full);
	return ok;
}

/* 默认 manifest：未随 spec 提供 plugin.json 时生成（v1 PS_ManagerWriteGeneratedManifest 字段集） */
static bool Plugin_WriteGeneratedManifest(const char* root, const XAdminGeneratedPluginSpec* spec)
{
	xvalue* m = ValueObject();
	xvalue* runtime = ValueObject();
	xvalue* build = ValueObject();
	xvalue* compat = ValueObject();
	xvalue* obj;
	const char* entry = (spec->entry && spec->entry[0]) ? spec->entry : "main.c";
	char* path; bool ok;
	ValueSetInt(m, "formatVersion", 4);
	ValueSetText(m, "xid", spec->xid);
	ValueSetText(m, "name", spec->xid);
	ValueSetText(m, "title", (spec->title && spec->title[0]) ? spec->title : spec->xid);
	ValueSetText(m, "description", "");
	ValueSetText(m, "version", (spec->version && spec->version[0]) ? spec->version : "1.0.0");
	ValueSetText(m, "author", "generated");
	ValueSetText(m, "kind", "singleton");
	ValueSetText(runtime, "compiler", "tcc");
	ValueSetText(runtime, "language", "c");
	ValueSetOwn(m, "runtime", runtime);
	ValueSetText(build, "entry", entry);
	{
		xvalue* sources = ValueArray();
		if (!xrtValueArrayAppendNew(sources, xrtValueString(xrtStrView(entry)))) { xrtValueRelease(sources); sources = NULL; }
		if (sources) ValueSetOwn(build, "sources", sources);
		ValueSetOwn(build, "includeDirs", ValueArray());
		ValueSetOwn(build, "libraryDirs", ValueArray());
		ValueSetOwn(build, "libraries", ValueArray());
		{
			xvalue* defines = ValueArray();
			if (!xrtValueArrayAppendNew(defines, xrtValueString(xrtStrView("XADMIN_PLUGIN=1")))) { xrtValueRelease(defines); defines = NULL; }
			if (defines) ValueSetOwn(build, "defines", defines);
		}
	}
	ValueSetOwn(m, "build", build);
	ValueSetText(compat, "minHostVersion", "4.0.0");
	ValueSetText(compat, "maxHostVersion", "5.0.0");
	ValueSetInt(compat, "abiVersion", XADMIN_ABI_VERSION);
	ValueSetOwn(m, "compat", compat);
	ValueSetOwn(m, "capabilities", ValueArray());
	obj = ValueObject();
	ValueSetOwn(obj, "plugins", ValueArray());
	ValueSetOwn(obj, "services", ValueArray());
	ValueSetOwn(obj, "features", ValueArray());
	ValueSetOwn(m, "dependencies", obj);
	obj = ValueObject();
	ValueSetOwn(obj, "menus", ValueArray());
	ValueSetOwn(obj, "routes", ValueArray());
	ValueSetOwn(obj, "hooks", ValueArray());
	ValueSetOwn(obj, "events", ValueArray());
	ValueSetOwn(m, "contributes", obj);
	ValueSetText(m, "defaultConfig", "config.defaults.json");
	path = xrtPathJoin((char*)root, "plugin.json");
	ok = path ? JsonWriteFile(path, m, true) : false;
	xrtFree(path);
	xrtValueRelease(m);
	return ok;
}

/* 运行时把生成目录登记为可启用实例（仿扫描装配 + 首发现台账） */
static PluginInstance* Plugin_RegisterGenerated(const char* sRootPath, const char* sXid)
{
	PluginInstance* inst;
	char* manifestPath;
	xvalue* manifest;
	if (G_PluginCount >= PLUGIN_MAX) return NULL;
	manifestPath = xrtPathJoin((char*)sRootPath, "plugin.json");
	manifest = manifestPath ? Plugin_LoadFlatJson(manifestPath) : NULL;
	xrtFree(manifestPath);
	if (!manifest) return NULL;
	inst = &G_Plugins[G_PluginCount++];
	memset(inst, 0, sizeof(*inst));
	snprintf(inst->xid, sizeof(inst->xid), "%s", sXid);
	snprintf(inst->rootPath, sizeof(inst->rootPath), "%s", sRootPath);
	snprintf(inst->dataPath, sizeof(inst->dataPath), "%s", xrtPathJoin(xrtPathJoin(AppPath, "plugin_data"), (char*)sXid));
	snprintf(inst->dbPath, sizeof(inst->dbPath), "%s", xrtPathJoin(xrtPathJoin(xrtPathJoin(AppPath, "db"), "plugin"), xrtFormat("%s/plugin.db", sXid)));
	inst->manifest = manifest;
	{
		xvalue* res = ValueGet(manifest, "resources");
		snprintf(inst->pageDir, sizeof(inst->pageDir), "%s", Plugin_ResourceDirName(res ? ValueText(res, "page") : NULL, "page"));
		snprintf(inst->templateDir, sizeof(inst->templateDir), "%s", Plugin_ResourceDirName(res ? ValueText(res, "template") : NULL, "template"));
		snprintf(inst->optionDir, sizeof(inst->optionDir), "%s", Plugin_ResourceDirName(res ? ValueText(res, "option") : NULL, "option"));
		snprintf(inst->staticDir, sizeof(inst->staticDir), "%s", Plugin_ResourceDirName(res ? ValueText(res, "static") : NULL, "static"));
		inst->allowSourceMap = res && (ValueBool(res, "allowSourceMap") || ValueBool(res, "staticAllowSourceMap"));
	}
	Plugin_Exec(xrtFormat(
		"INSERT OR IGNORE INTO plugin_package (package_id, plugin_id, version, source_type, install_path, checksum, signature, trust_level, manifest_json, install_time, xid) "
		"VALUES ('%s', '%s', '1.0.0', 'generated', '%s', '', '', 'system', '', %lld, '%s');",
		sXid, sXid, sRootPath, xrtNow(), sXid));
	Plugin_Exec(xrtFormat(
		"INSERT OR IGNORE INTO plugin_runtime (package_id, xid, mount_path, data_path, private_db_path, enabled, installed, config_json, status, active_generation, create_time, update_time) "
		"VALUES ('%s', '%s', '', '%s', '%s', 0, 0, '', 'discovered', 0, %lld, %lld);",
		sXid, sXid, inst->dataPath, inst->dbPath, xrtNow(), xrtNow()));
	printf("[plugin] generated %s registered\n", sXid);
	return inst;
}

int XAdmin_GeneratePlugin(XAdminPluginHandle plugin_handle, const XAdminGeneratedPluginSpec* spec)
{
	PluginInstance* inst = plugin_handle ? (PluginInstance*)plugin_handle : Plugin_Caller();
	char* root; char* sub;
	static const char* dirs[] = {"page", "template", "option", "static", "inc", "lib", "src", "data"};
	size_t i;
	if (!inst || !spec || !Plugin_XidValid(spec->xid)) return -1;
	if (Plugin_Find(spec->xid)) return -1; /* 目标已存在：内容系统以换代覆盖，不走本入口 */
	root = xrtPathJoin(xrtPathJoin(AppPath, "plugin"), (char*)spec->xid);
	if (!root) return -1;
	xrtDirCreateAll(root);
	for (i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++) {
		sub = xrtPathJoin(root, (char*)dirs[i]);
		if (sub) { xrtDirCreateAll(sub); xrtFree(sub); }
	}
	for (i = 0; i < spec->file_count; i++) {
		if (!Plugin_WriteGeneratedFile(root, &spec->files[i])) {
			printf("[plugin][error] generate %s: write file failed: %s\n", spec->xid, spec->files[i].relative_path);
			xrtFree(root);
			return -1;
		}
	}
	if (!Plugin_GeneratedSpecHasFile(spec, "plugin.json") && !Plugin_WriteGeneratedManifest(root, spec)) {
		printf("[plugin][error] generate %s: write manifest failed\n", spec->xid);
		xrtFree(root);
		return -1;
	}
	if (!Plugin_RegisterGenerated(root, spec->xid)) {
		xrtFree(root);
		return -1;
	}
	xrtFree(root);
	if (spec->auto_enable)
		return PluginHost_SetEnabled(spec->xid, true) ? 0 : -1;
	return 0;
}

/* v1 便捷 ABI：宿主分配堆的释放入口（xrtFree 别名；v1 生成模板使用）。 */
void XAdmin_Free(void* ptr)
{
	xrtFree(ptr);
}

/* v1 SDK 宏 xsReqRemote 的可注入形态：客户端地址。 */
const char* XAdmin_ReqRemote(XS_RequestObject objReq)
{
	XAdminRequest* req = (XAdminRequest*)objReq;
	return req && req->remote[0] ? req->remote : "";
}

/* v1 SDK 宏 xsReqPath 的可注入形态：取请求路径。 */
const char* XAdmin_ReqPath(XS_RequestObject objReq)
{
	XAdminRequest* req = (XAdminRequest*)objReq;
	return req && req->path ? req->path : "";
}

int XAdmin_ReloadPlugin(XAdminPluginHandle plugin_handle, const char* xid)
{
	(void)plugin_handle;
	return PluginHost_Reload(xid) ? 0 : -1;
}

int XAdmin_SetPluginEnabled(XAdminPluginHandle plugin_handle, const char* xid, int enabled)
{
	(void)plugin_handle;
	return PluginHost_SetEnabled(xid, enabled ? true : false) ? 0 : -1;
}

/* ==================== ABI：HTTP 便捷函数 ==================== */

int HttpReplyFormat(XS_ResponseObject objResp, int iCode, const char* sHead, const char* sFormat, ...)
{
	va_list args;
	char* text;
	int result;
	int count;
	va_start(args, sFormat);
	count = vsnprintf(NULL, 0, sFormat, args);
	va_end(args);
	if (count < 0) return -1;
	text = xrtMalloc((size_t)count + 1);
	if (!text) return -1;
	va_start(args, sFormat);
	vsnprintf(text, (size_t)count + 1, sFormat, args);
	va_end(args);
	result = xsHttpReplyAuto(objResp, iCode, sHead, text, 0);
	xrtFree(text);
	return result;
}

static int XAdmin_PluginMethodID(XS_RequestObject objReq)
{
	return ((XAdminRequest*)objReq)->raw->head->MethodCode;
}

/* ==================== 编译与生命周期 ==================== */

static xvalue* Plugin_LoadFlatJson(const char* sPath)
{
	size_t size = 0;
	bytes data;
	xvalue* tbl;
	data = (bytes)xrtFileReadAll(sPath, &size);
	if (!data) return NULL;
	tbl = xrtJsonParse(xrtStrViewN((const char*)data, size));
	xrtFree(data);
	if (tbl && xrtValueType(tbl) != XVALUE_OBJECT) {
		xrtValueRelease(tbl);
		return NULL;
	}
	return tbl;
}

static xvalue* Plugin_LoadConfig(PluginInstance* inst)
{
	xvalue* cfg = NULL;
	char* sDefaults = xrtPathJoin(inst->rootPath, "config.defaults.json");
	char* sOptions = xrtPathJoin(OptionPath, xrtFormat("plugin/%s.json", inst->xid));
	xvalue* over;
	if (sDefaults) {
		cfg = Plugin_LoadFlatJson(sDefaults);
		xrtFree(sDefaults);
	}
	if (!cfg) cfg = ValueObject();
	over = sOptions ? Plugin_LoadFlatJson(sOptions) : NULL;
	if (over) {
		xvalueiter it = {0};
		xvaluekey key;
		xvalue* val;
		if (xrtValueIterBegin(over, &it)) {
			while ((val = xrtValueIterNext(&it, &key))) {
				ValueSetOwn(cfg, key.String.Data, xrtValueDeepClone(val));
			}
			xrtValueIterEnd(&it);
		}
		xrtValueRelease(over);
	}
	if (sOptions) xrtFree(sOptions);
	return cfg;
}

/* TCC 诊断多段聚合（v1 PS_TCCGenerationErrorHandler 语义）：日志 + 错误缓冲。 */
static void Plugin_AppendError(char* sError, size_t iErrorSize, const char* msg)
{
	size_t used = sError ? strlen(sError) : 0;
	if (!sError || !msg) return;
	if (used && used + 1 < iErrorSize) { sError[used++] = '\n'; sError[used] = '\0'; }
	if (used < iErrorSize) snprintf(sError + used, iErrorSize - used, "%s", msg);
}

static void Plugin_TccErrorProc(void* opaque, const char* msg)
{
	char* buf = (char*)opaque;
	size_t used;
	if (!msg || !msg[0]) return;
	printf("        [tcc] %s\n", msg);
	if (!buf) return;
	used = strlen(buf);
	if (used + 2 > 2048) return; /* v1 同口径：2048 截断 */
	if (used) { buf[used++] = '\n'; buf[used] = '\0'; }
	snprintf(buf + used, 2048 - used + 1, "%s", msg);
}

static bool Plugin_Compile(PluginInstance* inst, char* sError, size_t iErrorSize)
{
	TCCState* tcc;
	char* sSdk = xrtPathJoin(AppPath, "plugin_sdk");
	xvalue* arr;
	bool bOk = false;

	tcc = xsCreateTCC();
	if (!tcc) {
		snprintf(sError, iErrorSize, "tcc create failed");
		return false;
	}
	tcc_set_error_func(tcc, sError, Plugin_TccErrorProc);
	tcc_add_include_path(tcc, inst->rootPath);
	if (sSdk) {
		tcc_add_include_path(tcc, sSdk);
		xrtFree(sSdk);
	}
	arr = ValueGet(ValueGet(inst->manifest, "build"), "includeDirs");
	if (arr && xrtValueType(arr) == XVALUE_ARRAY)
		for (uint32 i = 0; i < ValueCount(arr); i++) {
			str sRel = ValueArrayText(arr, i);
			if (sRel && sRel[0]) {
				char* sFull = xrtPathJoin(inst->rootPath, sRel);
				if (xrtDirExists(sFull)) tcc_add_include_path(tcc, sFull);
				xrtFree(sFull);
			}
		}
	/* v1 约定目录：inc/include/src 进头文件搜索，lib 进库搜索（存在才生效） */
	{
		static const struct { const char* rel; bool include; } conv[] = {
			{"inc", true}, {"lib", false}, {"src", true}, {"include", true},
		};
		size_t k;
		for (k = 0; k < sizeof(conv) / sizeof(conv[0]); k++) {
			char* sFull = xrtPathJoin(inst->rootPath, (char*)conv[k].rel);
			if (sFull) {
				if (xrtDirExists(sFull)) {
					if (conv[k].include) tcc_add_include_path(tcc, sFull);
					else tcc_add_library_path(tcc, sFull);
				}
				xrtFree(sFull);
			}
		}
	}
	/* build.libraryDirs：相对包根的库搜索路径 */
	arr = ValueGet(ValueGet(inst->manifest, "build"), "libraryDirs");
	if (arr && xrtValueType(arr) == XVALUE_ARRAY)
		for (uint32 i = 0; i < ValueCount(arr); i++) {
			str sRel = ValueArrayText(arr, i);
			if (sRel && sRel[0] && Plugin_ResourceIsSafeRelativePath(sRel)) {
				char* sFull = xrtPathJoin(inst->rootPath, sRel);
				if (sFull) {
					if (xrtDirExists(sFull)) tcc_add_library_path(tcc, sFull);
					xrtFree(sFull);
				}
			}
		}
	arr = ValueGet(ValueGet(inst->manifest, "build"), "defines");
	if (arr && xrtValueType(arr) == XVALUE_ARRAY)
		for (uint32 i = 0; i < ValueCount(arr); i++) {
			str sDef = ValueArrayText(arr, i);
			if (sDef && sDef[0]) {
				str sDefCopy = xrtStrDup(sDef);
				if (sDefCopy) {
					str pEq = strchr(sDefCopy, '=');
					if (pEq) {
						*pEq = 0;
						tcc_define_symbol(tcc, sDefCopy, pEq + 1);
					} else {
						tcc_define_symbol(tcc, sDefCopy, "1");
					}
					xrtFree(sDefCopy);
				}
			}
		}
	/* build.libraries：按名链接库（在上述库搜索路径中解析） */
	arr = ValueGet(ValueGet(inst->manifest, "build"), "libraries");
	if (arr && xrtValueType(arr) == XVALUE_ARRAY)
		for (uint32 i = 0; i < ValueCount(arr); i++) {
			str sLib = ValueArrayText(arr, i);
			if (sLib && sLib[0]) tcc_add_library(tcc, sLib);
		}
	/* 符号注入：32 个 ABI 函数 + 应用级请求/回复原语。
	 * 值/时间/文件等原生 xrt API 已由 xsCreateTCC 预置（全量符号 + /xs 头），
	 * 不在此重复注册；插件侧便捷层见 plugin_sdk/。 */
	{
		static const struct { const char* name; const void* ptr; } symbols[] = {
			{"HttpReplyFormat", (const void*)HttpReplyFormat},
			{"LoadPage", (const void*)LoadPage},
			{"xsHttpReplyAuto", (const void*)xsHttpReplyAuto},
			{"xsHttpReplyFormat", (const void*)xsHttpReplyFormat},
			{"xsReqMethodID", (const void*)XAdmin_PluginMethodID},
			{"xsReqQueryValue", (const void*)xsReqQueryValue},
			{"XAdmin_PluginReqHeader", (const void*)XAdmin_PluginReqHeader},
			{"XAdmin_ReqBody", (const void*)XAdmin_ReqBody},
			{"XAdmin_ReqBodyLen", (const void*)XAdmin_ReqBodyLen},
			{"XAdmin_HttpPostJson", (const void*)XAdmin_HttpPostJson},
			{"XAdmin_DeferRoute", (const void*)XAdmin_DeferRoute},
			{"XAdmin_ReplyBinary", (const void*)XAdmin_ReplyBinary},
			{"XAdmin_MemberContactStatus", (const void*)XAdmin_MemberContactStatus},
			{"XAdmin_ChannelAccept", (const void*)XAdmin_ChannelAccept},
			{"XAdmin_ChannelSend", (const void*)XAdmin_ChannelSend},
			{"XAdmin_ChannelClose", (const void*)XAdmin_ChannelClose},
			{"XAdmin_AdminCSRFToken", (const void*)XAdmin_AdminCSRFToken},
			{"XAdmin_CheckAdminCSRF", (const void*)XAdmin_CheckAdminCSRF},
			{"XAdmin_MultipartBoundary", (const void*)XAdmin_MultipartBoundary},
			{"XAdmin_MultipartNext", (const void*)XAdmin_MultipartNext},
			{"XAdmin_RegisterRoute", (const void*)XAdmin_RegisterRoute},
			{"XAdmin_UnregisterRoute", (const void*)XAdmin_UnregisterRoute},
			{"XAdmin_RegisterDynamicRoute", (const void*)XAdmin_RegisterDynamicRoute},
			{"XAdmin_Free", (const void*)XAdmin_Free},
			{"XAdmin_ReqPath", (const void*)XAdmin_ReqPath},
			{"XAdmin_ReqRemote", (const void*)XAdmin_ReqRemote},
			{"ServerHashPassword", (const void*)ServerHashPassword},
			{"XAdmin_RouteParam", (const void*)XAdmin_RouteParam},
			{"XAdmin_RouteParamCount", (const void*)XAdmin_RouteParamCount},
			{"XAdmin_RegisterMenu", (const void*)XAdmin_RegisterMenu},
			{"XAdmin_UnregisterMenu", (const void*)XAdmin_UnregisterMenu},
			{"XAdmin_RegisterAuthGroup", (const void*)XAdmin_RegisterAuthGroup},
			{"XAdmin_UnregisterAuthGroup", (const void*)XAdmin_UnregisterAuthGroup},
			{"XAdmin_RegisterAuth", (const void*)XAdmin_RegisterAuth},
			{"XAdmin_UnregisterAuth", (const void*)XAdmin_UnregisterAuth},
			{"XAdmin_RegisterUriAuth", (const void*)XAdmin_RegisterUriAuth},
			{"XAdmin_UnregisterUriAuth", (const void*)XAdmin_UnregisterUriAuth},
			{"XAdmin_ListenEvent", (const void*)XAdmin_ListenEvent},
			{"XAdmin_UnlistenEvent", (const void*)XAdmin_UnlistenEvent},
			{"XAdmin_EmitEvent", (const void*)XAdmin_EmitEvent},
			{"XAdmin_RegisterHook", (const void*)XAdmin_RegisterHook},
			{"XAdmin_UnregisterHook", (const void*)XAdmin_UnregisterHook},
			{"XAdmin_InvokeHook", (const void*)XAdmin_InvokeHook},
			{"XAdmin_RegisterService", (const void*)XAdmin_RegisterService},
			{"XAdmin_AcquireService", (const void*)XAdmin_AcquireService},
			{"XAdmin_ReleaseService", (const void*)XAdmin_ReleaseService},
			{"XAdmin_GeneratePlugin", (const void*)XAdmin_GeneratePlugin},
			{"XAdmin_ReloadPlugin", (const void*)XAdmin_ReloadPlugin},
			{"XAdmin_SetPluginEnabled", (const void*)XAdmin_SetPluginEnabled},
			{"XAdmin_LoadPluginPage", (const void*)XAdmin_LoadPluginPage},
			{"XAdmin_RenderPluginTemplate", (const void*)XAdmin_RenderPluginTemplate},
			{"XAdmin_PluginOptionLoad", (const void*)XAdmin_PluginOptionLoad},
			{"XAdmin_PluginOptionSave", (const void*)XAdmin_PluginOptionSave},
			{"XAdmin_PluginResourcePath", (const void*)XAdmin_PluginResourcePath},
		};
		size_t i;
		for (i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++)
			tcc_add_symbol(tcc, symbols[i].name, symbols[i].ptr);
	}
	/* 源文件：build.sources 优先，否则 entry */
	{
		str sEntry = ValueText(ValueGet(inst->manifest, "build"), "entry");
		bool bCompiled = false;
		arr = ValueGet(ValueGet(inst->manifest, "build"), "sources");
		if (arr && xrtValueType(arr) == XVALUE_ARRAY)
			for (uint32 i = 0; i < ValueCount(arr); i++) {
				str sSrc = ValueArrayText(arr, i);
				if (sSrc && sSrc[0]) {
					char* sFull = xrtPathJoin(inst->rootPath, sSrc);
					if (tcc_add_file(tcc, sFull) < 0) {
						if (!sError[0]) snprintf(sError, iErrorSize, "compile failed: %s", sSrc);
						xrtFree(sFull);
						goto failed;
					}
					bCompiled = true;
					xrtFree(sFull);
				}
			}
		if (!bCompiled && sEntry && sEntry[0]) {
			char* sFull = xrtPathJoin(inst->rootPath, sEntry);
			if (tcc_add_file(tcc, sFull) < 0) {
				if (!sError[0]) snprintf(sError, iErrorSize, "compile failed: %s", sEntry);
				xrtFree(sFull);
				goto failed;
			}
			xrtFree(sFull);
		}
	}
	if (tcc_relocate(tcc) < 0) {
		Plugin_AppendError(sError, iErrorSize, "relocate failed");
		goto failed;
	}
	inst->desc = (const XAdminPluginDescriptor*)tcc_get_symbol(tcc, "XAdmin_GetPluginDescriptor") ? ((const XAdminPluginDescriptor* (*)(void))tcc_get_symbol(tcc, "XAdmin_GetPluginDescriptor"))() : NULL;
	inst->setGlobal = (void (*)(int, void*))tcc_get_symbol(tcc, "XAdmin_PluginSetGlobalData");
	if (!inst->desc || !inst->setGlobal) {
		Plugin_AppendError(sError, iErrorSize, "descriptor symbols missing");
		goto failed;
	}
	if (inst->desc->abi_version != XADMIN_ABI_VERSION) {
		snprintf(sError, iErrorSize, "abi mismatch: %u", inst->desc->abi_version);
		goto failed;
	}
	inst->tcc = tcc;
	bOk = true;
failed:
	if (!bOk) xsDestroyTCC(tcc);
	return bOk;
}

static bool Plugin_Start(PluginInstance* inst, char* sError, size_t iErrorSize)
{
	void* handle = inst;
	sqlite3_stmt* stmt = NULL;
	int gen = 1;

	if (inst->started) return true;
	if (inst->stopping) return false;
	/* 确保数据与私有库目录存在（v1 插件在 OnStart/OnInstall 中直接写文件/开库）。 */
	xrtDirCreateAll(inst->dataPath);
	{
		char* sDbDir = xrtPathJoin(xrtPathJoin(AppPath, "db"), xrtFormat("plugin/%s", inst->xid));
		if (sDbDir) { xrtDirCreateAll(sDbDir); xrtFree(sDbDir); }
	}
	if (!Plugin_Compile(inst, sError, iErrorSize)) {
		/* v1 语义：编译失败也落代际行（state=failed + 聚合错误详情）供后台展示 */
		if (sqlite3_prepare_v2(G_DB,
			"INSERT INTO plugin_generation (instance_id, generation, package_version, state, compile_hash, load_time, start_time, stop_time, health_status, error_message, xid) "
			"VALUES (NULL, ?, ?, 'failed', '', ?, NULL, ?, '', ?, ?);", -1, &stmt, NULL) == SQLITE_OK) {
			char* sVersion = ValueText(inst->manifest, "version");
			sqlite3_bind_int(stmt, 1, gen);
			Plugin_BindText(stmt, 2, sVersion ? sVersion : "");
			sqlite3_bind_int64(stmt, 3, xrtNow());
			sqlite3_bind_int64(stmt, 4, xrtNow());
			Plugin_BindText(stmt, 5, sError);
			Plugin_BindText(stmt, 6, inst->xid);
			sqlite3_step(stmt);
			sqlite3_finalize(stmt);
		}
		return false;
	}
	inst->config = Plugin_LoadConfig(inst);
	if (!inst->config) inst->config = ValueObject();
	inst->activeLeases = 0;

	/* 代际号与台账行 */
	if (sqlite3_prepare_v2(G_DB, "SELECT MAX(generation) FROM plugin_generation WHERE xid=?;", -1, &stmt, NULL) == SQLITE_OK) {
		Plugin_BindText(stmt, 1, inst->xid);
		if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL)
			gen = (int)sqlite3_column_int64(stmt, 0) + 1;
		sqlite3_finalize(stmt);
	}
	inst->generation = gen;
	inst->genRowId = 0;
	if (sqlite3_prepare_v2(G_DB,
		"INSERT INTO plugin_generation (instance_id, generation, package_version, state, compile_hash, load_time, start_time, stop_time, health_status, error_message, xid) "
		"VALUES (NULL, ?, ?, 'active', ?, ?, ?, NULL, '', '', ?);", -1, &stmt, NULL) == SQLITE_OK) {
		char* sVersion = ValueText(inst->manifest, "version");
		sqlite3_bind_int(stmt, 1, gen);
		Plugin_BindText(stmt, 2, sVersion ? sVersion : "");
		Plugin_BindText(stmt, 3, xrtFormat("%s:%d", sVersion ? sVersion : "", gen));
		sqlite3_bind_int64(stmt, 4, xrtNow());
		sqlite3_bind_int64(stmt, 5, xrtNow());
		Plugin_BindText(stmt, 6, inst->xid);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
		inst->genRowId = Plugin_LastRow();
	}

	/* 生命周期：OnLoad → 全局注入 → 首次 OnInstall → OnConfigChanged → OnStart */
	inst->started = true; /* 注册函数在此之前须可用 */
	G_PluginRegIdx = (int)(inst - G_Plugins);
	if (inst->desc->OnLoad && inst->desc->OnLoad(&handle) != 0) {
		snprintf(sError, iErrorSize, "OnLoad failed");
		goto rollback;
	}
	/* 聚合上下文（v1 XAdminHostContext）：公共模板 + 插件身份段 */
	inst->hostContext = G_PluginHostContextTemplate;
	inst->hostContext.plugin_xid = inst->xid;
	inst->hostContext.plugin_root_path = inst->rootPath;
	inst->hostContext.plugin_data_path = inst->dataPath;
	inst->hostContext.plugin_private_db_path = inst->dbPath;
	inst->hostContext.main_db = G_DB;
	inst->hostContext.option_table = inst->config;
	inst->setGlobal(XADMIN_GLOBAL_HOST_CONTEXT, &inst->hostContext);
	inst->setGlobal(XADMIN_GLOBAL_MAIN_DB, G_DB);
	inst->setGlobal(XADMIN_GLOBAL_OPTION_TABLE, inst->config);
	inst->setGlobal(XADMIN_GLOBAL_PLUGIN_XID, inst->xid);
	inst->setGlobal(XADMIN_GLOBAL_PLUGIN_ROOT_PATH, inst->rootPath);
	inst->setGlobal(XADMIN_GLOBAL_PLUGIN_DATA_PATH, inst->dataPath);
	inst->setGlobal(XADMIN_GLOBAL_PLUGIN_PRIVATE_DB_PATH, inst->dbPath);
	{
		sqlite3_stmt* rt = NULL;
		bool bInstalled = false;
		if (sqlite3_prepare_v2(G_DB, "SELECT installed FROM plugin_runtime WHERE xid=?;", -1, &rt, NULL) == SQLITE_OK) {
			Plugin_BindText(rt, 1, inst->xid);
			if (sqlite3_step(rt) == SQLITE_ROW) bInstalled = sqlite3_column_int(rt, 0) != 0;
			sqlite3_finalize(rt);
		}
		if (!bInstalled && inst->desc->OnInstall) {
			if (inst->desc->OnInstall(handle) != 0) {
				snprintf(sError, iErrorSize, "OnInstall failed");
				goto rollback;
			}
			Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET installed=1 WHERE xid='%s';", inst->xid));
		}
	}
	if (inst->desc->OnConfigChanged && inst->desc->OnConfigChanged(handle, inst->config) != 0) {
		snprintf(sError, iErrorSize, "OnConfigChanged rejected initial configuration");
		goto rollback;
	}
	if (inst->desc->OnStart && inst->desc->OnStart(handle) != 0) {
		snprintf(sError, iErrorSize, "OnStart failed");
		goto rollback;
	}
	G_PluginRegIdx = -1;
	Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET status='running', active_generation=%d, update_time=%lld WHERE xid='%s';", gen, xrtNow(), inst->xid));
	printf("[plugin] started %s (generation %d)\n", inst->xid, gen);
	return true;
rollback:
	G_PluginRegIdx = -1;
	/* GR2 同 Plugin_Stop：回调前关门；GR4 路由兜底出表。 */
	inst->started = false;
	if (inst->desc && inst->desc->OnStop) inst->desc->OnStop(handle);
	Plugin_CleanupResources(inst);
	if (inst->desc && inst->desc->OnUnload) inst->desc->OnUnload(handle);
	{
		size_t k;
		for (k = 0; k < inst->routeCount; k++) {
			xrtMapRemove(G_StaticRouteTableHTTP, KeyView(inst->routePaths[k]));
			xrtFree((void*)inst->routePaths[k]);
		}
		inst->routeCount = 0;
	}
	inst->desc = NULL;
	inst->setGlobal = NULL;
	xsDestroyTCC(inst->tcc);
	inst->tcc = NULL;
	CacheRetire(inst->config);
	inst->config = NULL;
	if (inst->genRowId)
		Plugin_Exec(xrtFormat("UPDATE plugin_generation SET state='stopped', stop_time=%lld, error_message='start failed' WHERE id=%lld;", xrtNow(), inst->genRowId));
	Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET status='error' WHERE xid='%s';", inst->xid));
	return false;
}

static void Plugin_Stop(PluginInstance* inst)
{
	const XAdminPluginDescriptor* desc;
	size_t i;
	if (!inst->started) return;
	desc = inst->desc;
	/* GR2：回调期间先关门——Register* 被 started 门控拒绝，Unregister* 不受影响。 */
	inst->started = false;
	inst->stopping = true;
	PluginChannel_Stop(inst); /* joins without G_RequestLock; code/state still live */
	if (desc && desc->OnStop) desc->OnStop(inst);
	Plugin_CleanupResources(inst);
	if (desc && desc->OnUnload) desc->OnUnload(inst);
	/* GR4：路由按内存权威清单兜底出表（台账漏登时不留悬垂 Proc/Path）。 */
	for (i = 0; i < inst->routeCount; i++) {
		xrtMapRemove(G_StaticRouteTableHTTP, KeyView(inst->routePaths[i]));
		xrtFree((void*)inst->routePaths[i]);
	}
	inst->routeCount = 0;
	/* 动态路由同样按实例清单出表并重编译 pattern 树 */
	if (inst->dynCount) {
		for (i = 0; i < inst->dynCount; i++) {
			Plugin_DynamicRouteRemove(inst->dynPatterns[i]);
			xrtFree(inst->dynPatterns[i]);
		}
		inst->dynCount = 0;
		RouteHTTP_RecompileDynamic();
	}
	inst->desc = NULL;
	inst->setGlobal = NULL;
	/* GR1：有未释放服务租借时不销毁代码镜像，压入退役队列等 ReleaseService 回收。 */
	if (inst->tcc) {
		if (inst->activeLeases > 0) {
			int k, slot = -1;
			for (k = 0; k < PLUGIN_TCC_RETIRE_MAX; k++)
				if (!G_PluginTccRetired[k].used) { slot = k; break; }
			if (slot >= 0) {
				G_PluginTccRetired[slot].used = true;
				G_PluginTccRetired[slot].tcc = inst->tcc;
				G_PluginTccRetired[slot].leases = inst->activeLeases;
			} else {
				/* 队列满退化为泄漏代码镜像而非悬垂指针。 */
				printf("[plugin][warn] tcc retire queue full; leak image of %s (leases=%d)\n",
					inst->xid, inst->activeLeases);
			}
			inst->activeLeases = 0;
			inst->tcc = NULL;
		} else {
			xsDestroyTCC(inst->tcc);
			inst->tcc = NULL;
		}
	}
	CacheRetire(inst->config);
	inst->config = NULL;
	if (inst->genRowId)
		Plugin_Exec(xrtFormat("UPDATE plugin_generation SET state='stopped', stop_time=%lld WHERE id=%lld;", xrtNow(), inst->genRowId));
	inst->genRowId = 0;
	Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET status='disabled', active_generation=0, update_time=%lld WHERE xid='%s';", xrtNow(), inst->xid));
	printf("[plugin] stopped %s\n", inst->xid);
	inst->stopping = false;
}

/* ==================== 管理操作（供路由与插件 API） ==================== */

static bool PluginHost_SetEnabled(const char* sXid, bool bEnable)
{
	PluginInstance* inst = Plugin_Find(sXid);
	char sError[2096] = {0};
	if (!inst) return false;
	if (inst->stopping) return false;
	if (G_PluginChannelCallbacks || G_PluginChannelsStopping) return false;
	if (!bEnable && inst->activeIo) return false;
	if (G_PluginRegIdx >= 0) return false; /* GR3：插件启动期间禁止换代操作（防重入） */
	if (bEnable) {
		if (inst->started) return true;
		Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET enabled=1, update_time=%lld WHERE xid='%s';", xrtNow(), sXid));
		if (!Plugin_Start(inst, sError, sizeof(sError))) {
			printf("[plugin] enable failed %s: %s\n", sXid, sError);
			Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET status='error' WHERE xid='%s';", sXid));
			return false;
		}
		return true;
	}
	Plugin_Stop(inst);
	Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET enabled=0, update_time=%lld WHERE xid='%s';", xrtNow(), sXid));
	return true;
}

static bool PluginHost_Reload(const char* sXid)
{
	PluginInstance* inst = Plugin_Find(sXid);
	if (!inst) return false;
	if (inst->stopping) return false;
	if (G_PluginChannelCallbacks || G_PluginChannelsStopping) return false;
	if (inst->activeIo) return false;
	if (G_PluginRegIdx >= 0) return false; /* GR3：同上 */
	if (inst->started) Plugin_Stop(inst);
	Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET enabled=1 WHERE xid='%s';", sXid));
	{
		char sError[2096] = {0};
		if (!Plugin_Start(inst, sError, sizeof(sError))) {
			printf("[plugin] reload failed %s: %s\n", sXid, sError);
			Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET status='error' WHERE xid='%s';", sXid));
			return false;
		}
	}
	return true;
}

/* ==================== HostContext 聚合注入 ==================== */

/* 宿主公共段填充：路径取自 v3 实际布局；xs 无 exe 路径 API，按部署约定
 * AppPath/xs.exe 派生。派生串本代持有不释放（与静态路径同生命周期）。 */
static void PluginHost_ContextInit(XS_HostInfo* host)
{
	memset(&G_PluginHostContextTemplate, 0, sizeof(G_PluginHostContextTemplate));
	G_PluginHostContextTemplate.size = (uint32_t)sizeof(XAdminHostContext);
	G_PluginHostContextTemplate.abi_version = XADMIN_ABI_VERSION;
	G_PluginHostContextTemplate.exe_path = xrtPathJoin(AppPath, "xs.exe");
	G_PluginHostContextTemplate.app_path = AppPath;
	G_PluginHostContextTemplate.web_path = host && host->Path ? host->Path : "";
	G_PluginHostContextTemplate.db_path = DBPath;
	G_PluginHostContextTemplate.log_path = xrtPathJoin(AppPath, "logs");
	G_PluginHostContextTemplate.temp_path = xrtPathJoin(AppPath, "temp");
	G_PluginHostContextTemplate.page_path = xrtPathJoin(AppPath, "page");
	G_PluginHostContextTemplate.site_page_path = G_SitePagePath ? G_SitePagePath : xrtPathJoin(AppPath, "site");
	G_PluginHostContextTemplate.tool_path = xrtPathJoin(AppPath, "tool");
	G_PluginHostContextTemplate.option_path = OptionPath;
	G_PluginHostContextTemplate.install_path = xrtPathJoin(AppPath, "install");
	G_PluginHostContextTemplate.template_path = xrtPathJoin(AppPath, "template");
	G_PluginHostContextTemplate.attachment_path = AttachmentPath ? AttachmentPath : xrtPathJoin(xrtPathJoin(AppPath, "data"), "uploads");
}

/* ==================== 扫描与初始化 ==================== */

/* "a.b.c" 三元组比较（v1 PS_ManifestCompareVersion 轻量版）。 */
static int Plugin_CompareVersion(const char* a, const char* b)
{
	int ai[3] = {0, 0, 0}, bi[3] = {0, 0, 0};
	int i;
	sscanf(a ? a : "", "%d.%d.%d", &ai[0], &ai[1], &ai[2]);
	sscanf(b ? b : "", "%d.%d.%d", &bi[0], &bi[1], &bi[2]);
	for (i = 0; i < 3; i++) {
		if (ai[i] < bi[i]) return -1;
		if (ai[i] > bi[i]) return 1;
	}
	return 0;
}

static int Plugin_ScanProc(const char* sPath, size_t iSize, bool bDir, void* pParam)
{
	(void)iSize;
	if (bDir) {
		/* DirScan 非递归只回调顶层条目；目录名即候选 xid */
		const char* sName = sPath + strlen(sPath);
		while (sName > sPath && sName[-1] != '/' && sName[-1] != '\\') sName--;
		if (Plugin_XidValid(sName) && G_PluginCount < PLUGIN_MAX) {
			char* sManifestPath = xrtPathJoin(sPath, "plugin.json");
			xvalue* manifest = sManifestPath ? Plugin_LoadFlatJson(sManifestPath) : NULL;
			if (manifest) {
				PluginInstance* inst = &G_Plugins[G_PluginCount++];
				xvalue* tblBuild; xvalue* tblResources;
				memset(inst, 0, sizeof(*inst));
				snprintf(inst->xid, sizeof(inst->xid), "%s", sName);
				snprintf(inst->rootPath, sizeof(inst->rootPath), "%s", sPath);
				snprintf(inst->dataPath, sizeof(inst->dataPath), "%s", xrtPathJoin(xrtPathJoin(AppPath, "plugin_data"), sName));
				snprintf(inst->dbPath, sizeof(inst->dbPath), "%s", xrtPathJoin(xrtPathJoin(xrtPathJoin(AppPath, "db"), "plugin"), xrtFormat("%s/plugin.db", sName)));
				inst->manifest = manifest;
				inst->generation = 0;
				/* 资源目录装载：resources.page/template/option/static（缺省同名约定目录） */
				tblResources = ValueGet(manifest, "resources");
				snprintf(inst->pageDir, sizeof(inst->pageDir), "%s",
					Plugin_ResourceDirName(tblResources ? ValueText(tblResources, "page") : NULL, "page"));
				snprintf(inst->templateDir, sizeof(inst->templateDir), "%s",
					Plugin_ResourceDirName(tblResources ? ValueText(tblResources, "template") : NULL, "template"));
				snprintf(inst->optionDir, sizeof(inst->optionDir), "%s",
					Plugin_ResourceDirName(tblResources ? ValueText(tblResources, "option") : NULL, "option"));
				snprintf(inst->staticDir, sizeof(inst->staticDir), "%s",
					Plugin_ResourceDirName(tblResources ? ValueText(tblResources, "static") : NULL, "static"));
				inst->allowSourceMap = tblResources &&
					(ValueBool(tblResources, "allowSourceMap") || ValueBool(tblResources, "staticAllowSourceMap"));
				/* v1 深校验轻量版：formatVersion/ABI/entry + 必填字段 +
				 * xid==目录名 identity + 宿主版本区间（max 越界拒绝，min 越界仅告警） */
				tblBuild = ValueGet(manifest, "build");
				{
					str sManifestXid = ValueText(manifest, "xid");
					str sMaxHost = ValueText(ValueGet(manifest, "compat"), "maxHostVersion");
					str sMinHost = ValueText(ValueGet(manifest, "compat"), "minHostVersion");
					bool bInvalid =
						(ValueInt(manifest, "formatVersion") != 4) ||
						(ValueInt(ValueGet(manifest, "compat"), "abiVersion") > XADMIN_ABI_VERSION) ||
						!tblBuild || !ValueText(tblBuild, "entry") ||
						!ValueText(manifest, "name") || !ValueText(manifest, "title") ||
						!ValueText(manifest, "version") || !ValueText(manifest, "kind") ||
						(sManifestXid && strcmp(sManifestXid, sName) != 0) ||
						(sMaxHost && sMaxHost[0] && Plugin_CompareVersion(sMaxHost, PLUGIN_HOST_VERSION) < 0);
					if (bInvalid) {
						printf("[plugin][warn] manifest invalid, skipped: %s\n", sName);
						xrtValueRelease(manifest);
						memset(inst, 0, sizeof(*inst));
						G_PluginCount--; /* 回退计数即可（末尾元素） */
					} else if (sMinHost && sMinHost[0] && Plugin_CompareVersion(sMinHost, PLUGIN_HOST_VERSION) > 0) {
						printf("[plugin][warn] %s requires host >= %s (running %s)\n", sName, sMinHost, PLUGIN_HOST_VERSION);
					}
				}
			}
			if (sManifestPath) xrtFree(sManifestPath);
		}
	}
	return 0;
}

static void Plugin_SyncDatabase(void)
{
	size_t i;
	sqlite3_stmt* stmt = NULL;
	for (i = 0; i < G_PluginCount; i++) {
		PluginInstance* inst = &G_Plugins[i];
		char* sManifestText = NULL;
		bool bExists = false;
		if (sqlite3_prepare_v2(G_DB, "SELECT 1 FROM plugin_runtime WHERE xid=?;", -1, &stmt, NULL) == SQLITE_OK) {
			Plugin_BindText(stmt, 1, inst->xid);
			if (sqlite3_step(stmt) == SQLITE_ROW) bExists = true;
			sqlite3_finalize(stmt);
		}
		{
			size_t size = 0;
			char* sPath = xrtPathJoin(inst->rootPath, "plugin.json");
			bytes data = (bytes)xrtFileReadAll(sPath, &size);
			if (data) {
				sManifestText = xrtMalloc(size + 1);
				memcpy(sManifestText, data, size);
				sManifestText[size] = '\0';
				xrtFree(data);
			}
			xrtFree(sPath);
		}
		if (!bExists) {
			if (sqlite3_prepare_v2(G_DB,
				"INSERT INTO plugin_package (package_id, plugin_id, version, source_type, install_path, checksum, signature, trust_level, manifest_json, install_time, xid) "
				"VALUES (?, ?, ?, 'local', ?, '', '', 'system', ?, ?, ?);", -1, &stmt, NULL) == SQLITE_OK) {
				str sVersion = ValueText(inst->manifest, "version");
				Plugin_BindText(stmt, 1, inst->xid);
				Plugin_BindText(stmt, 2, inst->xid);
				Plugin_BindText(stmt, 3, sVersion ? sVersion : "0.0.0");
				Plugin_BindText(stmt, 4, inst->rootPath);
				Plugin_BindText(stmt, 5, sManifestText ? sManifestText : "");
				sqlite3_bind_int64(stmt, 6, xrtNow());
				Plugin_BindText(stmt, 7, inst->xid);
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);
			}
			if (sqlite3_prepare_v2(G_DB,
				"INSERT INTO plugin_runtime (package_id, xid, mount_path, data_path, private_db_path, enabled, installed, config_json, status, active_generation, create_time, update_time) "
				"VALUES (?, ?, '', ?, ?, 0, 0, '', 'discovered', 0, ?, ?);", -1, &stmt, NULL) == SQLITE_OK) {
				Plugin_BindText(stmt, 1, inst->xid);
				Plugin_BindText(stmt, 2, inst->xid);
				Plugin_BindText(stmt, 3, inst->dataPath);
				Plugin_BindText(stmt, 4, inst->dbPath);
				sqlite3_bind_int64(stmt, 5, xrtNow());
				sqlite3_bind_int64(stmt, 6, xrtNow());
				sqlite3_step(stmt);
				sqlite3_finalize(stmt);
			}
			printf("[plugin] discovered %s\n", inst->xid);
		} else {
			/* 路径与 manifest 刷新（幂等） */
			if (sManifestText)
				Plugin_Exec(xrtFormat("UPDATE plugin_package SET install_path='%s', manifest_json='%s' WHERE xid='%s';",
					inst->rootPath, sManifestText, inst->xid));
			Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET data_path='%s', private_db_path='%s' WHERE xid='%s';",
				inst->dataPath, inst->dbPath, inst->xid));
		}
		if (sManifestText) xrtFree(sManifestText);
	}
}

static void PluginHost_Init(void)
{
	/* 状态重置（防陈旧穿透）：DB 的 status 是上一进程的持久字符串，本进程
	 * 启动时尚无任何插件在运行——非 discovered 一律归位 disabled，随后自启动
	 * 把成功者翻成 running、失败者翻成 error。否则上会话的 running 会在
	 * 本轮启动失败时继续显示（实测：列表显示启用+running 而路由不存在）。 */
	Plugin_Exec("UPDATE plugin_runtime SET status='disabled' WHERE status NOT IN ('discovered','disabled');");
	char* sPluginRoot;
	printf("        PluginHost_Init \n");
	memset(G_PluginEvents, 0, sizeof(G_PluginEvents));
	memset(G_PluginHooks, 0, sizeof(G_PluginHooks));
	memset(G_PluginServices, 0, sizeof(G_PluginServices));

	/* 两代共同清理：上次运行的 active 代际在本次启动前全部视为 stopped。 */
	Plugin_Exec(xrtFormat("UPDATE plugin_generation SET state='stopped', stop_time=%lld WHERE state='active';", xrtNow()));

	Plugin_ConvertLegacyPaths();
	sPluginRoot = xrtPathJoin(AppPath, "plugin");
	if (sPluginRoot && xrtDirExists(sPluginRoot))
		DirScan(sPluginRoot, false, Plugin_ScanProc, NULL);
	if (sPluginRoot) xrtFree(sPluginRoot);
	Plugin_SyncDatabase();

	/* 启动 enabled=1 的插件（依赖检查：manifest dependencies.plugins 须已启用） */
	{
		sqlite3_stmt* stmt = NULL;
		if (sqlite3_prepare_v2(G_DB, "SELECT xid FROM plugin_runtime WHERE enabled=1;", -1, &stmt, NULL) == SQLITE_OK) {
			while (sqlite3_step(stmt) == SQLITE_ROW) {
				const char* sXid = (const char*)sqlite3_column_text(stmt, 0);
				PluginInstance* inst = Plugin_Find(sXid);
				char sError[256] = {0};
				if (!inst) continue;
				{
					xvalue* arrDeps = ValueGet(
						ValueGet(inst->manifest, "dependencies"), "plugins");
					bool bMissing = false;
					if (arrDeps && xrtValueType(arrDeps) == XVALUE_ARRAY)
						for (uint32 d = 0; d < ValueCount(arrDeps); d++) {
							str sDep = ValueArrayText(arrDeps, d);
							sqlite3_stmt* chk = NULL;
							int bOn = 0;
							if (sqlite3_prepare_v2(G_DB, "SELECT enabled FROM plugin_runtime WHERE xid=?;", -1, &chk, NULL) == SQLITE_OK) {
								Plugin_BindText(chk, 1, sDep);
								if (sqlite3_step(chk) == SQLITE_ROW) bOn = sqlite3_column_int(chk, 0);
								sqlite3_finalize(chk);
							}
							if (!bOn) {
								printf("[plugin] dependency not enabled: %s needs %s\n", sXid, sDep);
								Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET status='error' WHERE xid='%s';", sXid));
								bMissing = true;
								break;
							}
						}
					if (bMissing) continue;
				}
				if (!Plugin_Start(inst, sError, sizeof(sError)))
				{
					printf("[plugin] start failed %s: %s\n", sXid, sError);
					Plugin_Exec(xrtFormat("UPDATE plugin_runtime SET status='error' WHERE xid='%s';", sXid));
				}
			}
			sqlite3_finalize(stmt);
		}
	}
	/* 孤儿菜单清理（v1 语义：菜单只在插件活跃时可见）：enabled 但本轮未启动
	 * （编译失败/依赖缺失）的插件，其历史菜单行软删——否则菜单在、路由无，
	 * 点进去 404 死链。插件再次启动时 upsert 复活（RegisterMenu 幂等）。 */
	{
		char sStarted[PLUGIN_NAME_MAX * PLUGIN_MAX + 8];
		size_t used = 0;
		size_t mi;
		sStarted[0] = '\0';
		for (mi = 0; mi < G_PluginCount; mi++) {
			if (G_Plugins[mi].started) {
				int n = snprintf(used < sizeof(sStarted) ? sStarted + used : NULL,
					used < sizeof(sStarted) ? sizeof(sStarted) - used : 0,
					"'%s',", G_Plugins[mi].xid);
				if (n < 0) break;
				if (used + (size_t)n >= sizeof(sStarted) - 2) break; /* 截断防护：留闭引号空间 */
				used += (size_t)n;
			}
		}
		if (used > 0) {
			sStarted[used - 1] = '\0'; /* 去尾逗号 */
			Plugin_Exec(xrtFormat(
				"UPDATE menu SET isDelete=1, updateTime=%lld WHERE isDelete=0 AND plugin_xid<>'' AND plugin_xid NOT IN (%s);",
				xrtNow(), sStarted));
				} else {
					/* 本轮零插件启动：清全部插件自有菜单 */
					Plugin_Exec(xrtFormat(
						"UPDATE menu SET isDelete=1, updateTime=%lld WHERE isDelete=0 AND plugin_xid<>'';",
						xrtNow()));
				}
	}
}

static void PluginHost_Unit(void)
{
	size_t i;
	printf("        PluginHost_Unit \n");
	PluginAsync_Unit(); /* callbacks finish before plugin state/code and DB teardown */
	PluginChannel_Unit(); /* channels join before plugin state and code are released */
	for (i = 0; i < G_PluginCount; i++)
		Plugin_Stop(&G_Plugins[i]);
	for (i = 0; i < G_PluginCount; i++) {
		xrtValueRelease(G_Plugins[i].manifest);
	}
	G_PluginCount = 0;
}
