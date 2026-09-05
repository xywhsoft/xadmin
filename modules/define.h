/* 本代应用状态。数据库与资源路径只在此处解析，不再依赖 data/script。 */
#define SQL_PREPARE_DEFAULT (SQLITE_PREPARE_PERSISTENT | SQLITE_PREPARE_DONT_LOG)
static char* AppPath;
static char* DBPath;
static char* OptionPath;
static const char* HTTP_CT_HTML = "Content-Type: text/html; charset=utf-8\r\n";
static const char* HTTP_CT_TEXT = "Content-Type: text/plain; charset=utf-8\r\n";
static const char* HTTP_CT_JSON = "Content-Type: application/json; charset=utf-8\r\n";
static sqlite3* G_DB;
static xmutex* G_RequestLock;
static bool G_Ready;
static xvalue* G_Option;
static bool G_AdminEntryEnabled;
static char* G_AdminEntryPath;

typedef void (*XAdminRouteProc)(XS_ServerObject, XS_HostObject, XS_RequestObject, XS_ResponseObject, xvalue*);
typedef struct RouteInfo {
	XAdminRouteProc Proc[10];
	const char* Path; /* 权限配置键：静态 URI 或动态 pattern，不是某次捕获的实际路径。 */
	bool bAuth, bAdmin, bPutLog, bActive;
	uint32 AuthID, AuthLevel;
} RouteInfo;
static xdict G_StaticRouteTableHTTP;
static const xhttpmethod G_Methods[10] = {
	XHTTP_METHOD_GET, XHTTP_METHOD_HEAD, XHTTP_METHOD_POST, XHTTP_METHOD_PUT,
	XHTTP_METHOD_DELETE, XHTTP_METHOD_CONNECT, XHTTP_METHOD_OPTIONS,
	XHTTP_METHOD_TRACE, XHTTP_METHOD_PATCH, XHTTP_METHOD_OTHER
};

/* 密码算法保持 v1 契约，兼容既有 salt/pwd 和旧页面的客户端哈希。 */
static char* ServerHashPassword(const char* user, const char* salt, const char* client_hash)
{
	unsigned char digest[32]; char* combined; char* result;
	if (!user || !salt || !client_hash) return NULL;
	combined = xrtFormat("%s%s%s", user, salt, client_hash);
	if (!combined) return NULL;
	if (!xrtSha256(combined, strlen(combined), digest)) { xrtFree(combined); return NULL; }
	result = XA_HexEncode(digest, sizeof(digest));
	xrtFree(combined);
	return result;
}
