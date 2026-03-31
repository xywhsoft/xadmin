


// 全局定义
#define SQL_PREPARE_DEFAULT		SQLITE_PREPARE_PERSISTENT | SQLITE_PREPARE_DONT_LOG



// 补充 API 定义



// 全局路径
str ExePath;
str AppPath;
str WebPath;
str DBPath;
str LogPath;
str TempPath;
str PagePath;
str ToolPath;
str OptionPath;
str InstallPath;
str TemplatePath;
str ModelPath;				// 模型实例目录 (script/model)
str ModelTemplatePath;		// 模型代码模板目录 (data/model_template)



// HTML 载荷类型
str HTTP_CT_HTML = "Content-Type: text/html\r\n";
str HTTP_CT_TEXT = "Content-Type: text/plain\r\n";
str HTTP_CT_JSON = "Content-Type: application/json\r\n";



// 服务端二�?SHA-256 哈希函数
// 计算：SHA256(用户�?+ salt + 客户端哈希�?
// salt 为每个用户独立的随机盐（存储在数据库 user.salt 字段�?
// 返回动态分配的哈希字符串，调用方需�?xrtFree 释放
str ServerHashPassword(str user, str salt, str clientHash)
{
	str sCombined = xrtFormat("%s%s%s", user, salt, clientHash);
	uint8 arrHash[32];
	str sPwdHash;

	xrtSHA256((const ptr)sCombined, strlen(sCombined), arrHash);
	sPwdHash = xrtHexEncode(arrHash, sizeof(arrHash));
	xrtFree(sCombined);
	return sPwdHash;
}



// 是否已安�?
bool G_Install = FALSE;



// 全局 Session �?- 后台管理�?
xvalue G_AdminSession = NULL;
xdict G_AdminSessionMap = NULL;

// 全局 Session �?- 前台用户
xvalue G_MemberSession = NULL;
xdict G_MemberSessionMap = NULL;



// 全局配置�?
xvalue G_Option = NULL;



// 全局数据库对�?
sqlite3* G_DB = NULL;



// 全局服务缓存�?
xlist G_Services = NULL;



// 全局静态路由表 - HTTP
typedef struct {
	
	// 对应 URI 的处理函�?
	void (*Proc)(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue objSession);
	
	// 是否必须鉴权才能访问
	bool bAuth;
	
	// 是否是后�?URI（TRUE为后台、FALSE为前台）
	bool bAdmin;
	
	// 是否记录访问日志（后台选项�?
	bool bPutLog;
	
	// 是否保持活跃（访问了保持活跃的链接，会自动延�?session 寿命�?
	bool bActive;
	
	// 所属权限组ID
	uint32 AuthID;
	
	// 权限级别�?为不限制，否则必须用户组具备大于等于这个数字的权限级别才能访问）
	uint32 AuthLevel;
	
} RouteInfo;
xdict G_StaticRouteTableHTTP;

// 添加全局静态路由表�?- HTTP
void AddStaticRouteHTTP(str uri, void* proc)
{
	RouteInfo* pInfo = xrtDictSet(G_StaticRouteTableHTTP, uri, strlen(uri), NULL);
	if ( pInfo ) {
		pInfo->Proc = proc;
		pInfo->bAuth = TRUE;		// 默认需要鉴权（安全优先�?
		pInfo->bAdmin = TRUE;		// 默认后台接口
		pInfo->bPutLog = FALSE;		// 默认不记录日�?
		pInfo->bActive = FALSE;		// 默认不保持活�?
		pInfo->AuthID = 0;
		pInfo->AuthLevel = 0;
	} else {
		printf("add static http route failed : %s.\n", uri);
	}
}



// 后台全局权限�?

// xadmin http helper api
typedef struct HttpMultipartPart {
	const char* sName;
	size_t iNameLen;
	const char* sFileName;
	size_t iFileNameLen;
	const char* pBody;
	size_t iBodyLen;
} HttpMultipartPart;
bool HttpMultipartNameIs(const HttpMultipartPart* pPart, const char* sName);
bool HttpMultipartNext(XS_RequestObject objReq, size_t* pOffset, HttpMultipartPart* pPart);
bool HttpMethodIs(XS_RequestObject objReq, const char* sMethod);
size_t HttpMethodLen(XS_RequestObject objReq);
size_t HttpPathLen(XS_RequestObject objReq);
size_t HttpQueryLen(XS_RequestObject objReq);
int HttpGetQueryVar(XS_RequestObject objReq, const char* sName, char* sOut, size_t iOutCap);
int http_reply(XS_ResponseObject objResp, int iCode, str sHead, const void* pBody, size_t iLen);
int HttpReplyFormat(XS_ResponseObject objResp, int iCode, str sHead, str sFormat, ...);
void LoadPage(XS_ResponseObject objResp, int iCode, str sHead, str sPage);
void XS_ImportScriptAPI(TCCState* s);

static void XAdminTCCErrorHandler(void* pOpaque, const char* sMsg)
{
	(void)pOpaque;
	fprintf(stderr, "[TCC] %s\n", sMsg);
}

static void XAdminTCCAddIncludePathEx(TCCState* s, const char* sBasePath, const char* sRelPath)
{
	str sPath;
	
	if ( (s == NULL) || (sRelPath == NULL) || (sRelPath[0] == '\0') ) {
		return;
	}
	
	if ( (sBasePath != NULL) && (sBasePath[0] != '\0') ) {
		sPath = xrtPathJoin(2, (str)sBasePath, (str)sRelPath);
		if ( sPath ) {
			if ( xrtDirExists(sPath) ) {
				tcc_add_include_path(s, sPath);
			}
			xrtFree(sPath);
		}
	}
	
	tcc_add_include_path(s, sRelPath);
}

static void XAdminTCCAddLibraryPathEx(TCCState* s, const char* sBasePath, const char* sRelPath)
{
	str sPath;
	
	if ( (s == NULL) || (sRelPath == NULL) || (sRelPath[0] == '\0') ) {
		return;
	}
	
	if ( (sBasePath != NULL) && (sBasePath[0] != '\0') ) {
		sPath = xrtPathJoin(2, (str)sBasePath, (str)sRelPath);
		if ( sPath ) {
			if ( xrtDirExists(sPath) ) {
				tcc_add_library_path(s, sPath);
			}
			xrtFree(sPath);
		}
	}
	
	tcc_add_library_path(s, sRelPath);
}

TCCState* xsCreateTCC(const char* sWorkPath)
{
	TCCState* s = tcc_new();
	const char* sAppPath = xsAppPath();
	
	if ( s == NULL ) {
		return NULL;
	}
	
	tcc_set_error_func(s, stderr, XAdminTCCErrorHandler);
	
	#if defined(_WIN32) || defined(_WIN64)
		XAdminTCCAddIncludePathEx(s, sAppPath, "tcc/include_win/winapi");
		XAdminTCCAddIncludePathEx(s, sAppPath, "tcc/include_win");
	#else
		XAdminTCCAddIncludePathEx(s, sAppPath, "tcc/include_linux");
		tcc_add_include_path(s, "/usr/include");
		tcc_add_include_path(s, "/usr/include/i386-linux-gnu");
		tcc_add_include_path(s, "/usr/include/i386-linux-gnu/sys");
		tcc_add_include_path(s, "/usr/include/x86_64-linux-gnu");
		tcc_add_include_path(s, "/usr/include/x86_64-linux-gnu/sys");
		tcc_add_library_path(s, "/usr/lib");
		tcc_add_library_path(s, "/usr/lib/i386-linux-gnu");
		tcc_add_library_path(s, "/usr/lib/x86_64-linux-gnu");
	#endif
	
	XAdminTCCAddIncludePathEx(s, sAppPath, "tcc/inc_xs");
	XAdminTCCAddIncludePathEx(s, sAppPath, "tcc/include");
	XAdminTCCAddLibraryPathEx(s, sAppPath, "tcc/lib");
	
	if ( (sWorkPath != NULL) && (sWorkPath[0] != '\0') ) {
		tcc_add_include_path(s, sWorkPath);
		tcc_add_library_path(s, sWorkPath);
	}
	
	tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
	XS_ImportScriptAPI(s);
	return s;
}

void xsDestroyTCC(TCCState* s)
{
	if ( s ) {
		tcc_delete(s);
	}
}

xvalue G_CACHE_RoleAuth = NULL;					// 角色权限缓存 - 后端鉴权查表�?
xvalue G_CACHE_Auth = NULL;						// 权限分组缓存 - 前端列表渲染�?
xvalue G_CACHE_Group = NULL;					// 权限分类缓存 - 前端列表渲染�?
xvalue G_CACHE_Role = NULL;						// 角色列表缓存 - 前端列表渲染�?
xvalue G_CACHE_RoleAuthIndex = NULL;			// role id -> permission array index
xvalue G_CACHE_RoleAuthLevel = NULL;			// role id -> authLevel

// 前台全局权限�?
xvalue G_CACHE_MemberGroupAuth = NULL;			// 前台用户组权限缓�?- 鉴权查表�?
xvalue G_CACHE_MemberAuth = NULL;				// 前台权限分组缓存
xvalue G_CACHE_MemberAuthGroup = NULL;			// 前台权限分类缓存
xvalue G_CACHE_MemberGroup = NULL;				// 前台用户组列表缓�?
xvalue G_CACHE_MemberGroupAuthIndex = NULL;	// group id -> permission array index
xvalue G_CACHE_MemberGroupAuthLevel = NULL;	// group id -> authLevel



// 预编译的 SQL 语句 - uris �?
sqlite3_stmt* stmt_uris_all = NULL;				// 分页获取所�?URI 数据
sqlite3_stmt* stmt_uris_sel = NULL;				// 分页条件查询 URI 数据
sqlite3_stmt* stmt_uris_add = NULL;				// 添加 URI 记录
sqlite3_stmt* stmt_uris_del = NULL;				// 删除 URI 记录
sqlite3_stmt* stmt_uris_put = NULL;				// 修改 URI 记录
sqlite3_stmt* stmt_uris_get = NULL;				// 根据 ID 获取 URI 记录

// 预编译的 SQL 语句 - auth �?
sqlite3_stmt* stmt_auth_all = NULL;				// 分页获取所有权限组数据
sqlite3_stmt* stmt_auth_sel = NULL;				// 分页条件查询权限组数�?
sqlite3_stmt* stmt_auth_add = NULL;				// 添加权限组记�?
sqlite3_stmt* stmt_auth_del = NULL;				// 删除权限组记录（软删除）
sqlite3_stmt* stmt_auth_put = NULL;				// 修改权限组记�?
sqlite3_stmt* stmt_auth_get = NULL;				// 根据 ID 获取权限组记�?
sqlite3_stmt* stmt_auth_sum = NULL;				// 统计关联�?URI 权限数量
sqlite3_stmt* stmt_auth_mov = NULL;				// 移动权限组下�?URI 权限到默认分�?

// 预编译的 SQL 语句 - authGroup �?
sqlite3_stmt* stmt_group_all = NULL;			// 分页获取所有权限分类数�?
sqlite3_stmt* stmt_group_sel = NULL;			// 分页条件查询权限分类数据
sqlite3_stmt* stmt_group_add = NULL;			// 添加权限分类记录
sqlite3_stmt* stmt_group_del = NULL;			// 删除权限分类记录（软删除�?
sqlite3_stmt* stmt_group_put = NULL;			// 修改权限分类记录
sqlite3_stmt* stmt_group_get = NULL;			// 根据 ID 获取权限分类记录
sqlite3_stmt* stmt_group_sum = NULL;			// 统计关联的权限组数量
sqlite3_stmt* stmt_group_mov = NULL;			// 移动权限分类下的权限组到默认分类

// 预编译的 SQL 语句 - role �?
sqlite3_stmt* stmt_role_all = NULL;				// 分页获取所有角色数�?
sqlite3_stmt* stmt_role_sel = NULL;				// 分页条件查询角色数据
sqlite3_stmt* stmt_role_get = NULL;				// 根据 ID 获取角色记录
sqlite3_stmt* stmt_role_add = NULL;				// 添加角色记录
sqlite3_stmt* stmt_role_put = NULL;				// 修改角色记录
sqlite3_stmt* stmt_role_del = NULL;				// 删除角色记录（软删除�?
sqlite3_stmt* stmt_role_sum = NULL;				// 统计关联的用户数�?

// 预编译的 SQL 语句 - user �?
sqlite3_stmt* stmt_user_all = NULL;				// 分页获取所有用户数�?
sqlite3_stmt* stmt_user_sel = NULL;				// 分页条件查询用户数据
sqlite3_stmt* stmt_user_get = NULL;				// 根据 ID 获取用户记录
sqlite3_stmt* stmt_user_add = NULL;				// 添加用户记录
sqlite3_stmt* stmt_user_put = NULL;				// 修改用户记录
sqlite3_stmt* stmt_user_del = NULL;				// 删除用户记录（软删除�?
sqlite3_stmt* stmt_user_chk = NULL;				// 检查用户名是否已存�?
sqlite3_stmt* stmt_user_pwd = NULL;				// 修改用户密码

// 预编译的 SQL 语句 - 登录相关
sqlite3_stmt* stmt_login_get = NULL;			// 根据用户名和密码获取用户信息

// 预编译的 SQL 语句 - 缓存相关
sqlite3_stmt* stmt_cache_auth = NULL;			// 获取所有权限组数据（缓存用�?
sqlite3_stmt* stmt_cache_group = NULL;			// 获取所有权限分类数据（缓存用）
sqlite3_stmt* stmt_cache_role = NULL;			// 获取所有角色数据（缓存用）
sqlite3_stmt* stmt_cache_uris = NULL;			// 获取所有URI记录（用于更新URI表）

// ==================== 前台用户系统预编译SQL ====================

// 预编译的 SQL 语句 - member �?
sqlite3_stmt* stmt_member_all = NULL;			// 分页获取所有前台用户数�?
sqlite3_stmt* stmt_member_sel = NULL;			// 分页条件查询前台用户数据
sqlite3_stmt* stmt_member_get = NULL;			// 根据 ID 获取前台用户记录
sqlite3_stmt* stmt_member_add = NULL;			// 添加前台用户记录
sqlite3_stmt* stmt_member_put = NULL;			// 修改前台用户记录
sqlite3_stmt* stmt_member_del = NULL;			// 删除前台用户记录（软删除�?
sqlite3_stmt* stmt_member_chk = NULL;			// 检查用户名是否已存�?
sqlite3_stmt* stmt_member_pwd = NULL;			// 修改用户密码
sqlite3_stmt* stmt_member_balance = NULL;		// 修改用户余额

// 预编译的 SQL 语句 - memberGroup �?
sqlite3_stmt* stmt_mgroup_all = NULL;			// 分页获取所有前台用户组数据
sqlite3_stmt* stmt_mgroup_sel = NULL;			// 分页条件查询前台用户组数�?
sqlite3_stmt* stmt_mgroup_get = NULL;			// 根据 ID 获取前台用户组记�?
sqlite3_stmt* stmt_mgroup_add = NULL;			// 添加前台用户组记�?
sqlite3_stmt* stmt_mgroup_put = NULL;			// 修改前台用户组记�?
sqlite3_stmt* stmt_mgroup_del = NULL;			// 删除前台用户组记录（软删除）
sqlite3_stmt* stmt_mgroup_sum = NULL;			// 统计关联的用户数�?

// 预编译的 SQL 语句 - memberAuthGroup �?
sqlite3_stmt* stmt_magroup_all = NULL;			// 分页获取所有前台权限分类数�?
sqlite3_stmt* stmt_magroup_sel = NULL;			// 分页条件查询前台权限分类数据
sqlite3_stmt* stmt_magroup_get = NULL;			// 根据 ID 获取前台权限分类记录
sqlite3_stmt* stmt_magroup_add = NULL;			// 添加前台权限分类记录
sqlite3_stmt* stmt_magroup_put = NULL;			// 修改前台权限分类记录
sqlite3_stmt* stmt_magroup_del = NULL;			// 删除前台权限分类记录（软删除�?
sqlite3_stmt* stmt_magroup_sum = NULL;			// 统计关联的权限分组数�?
sqlite3_stmt* stmt_magroup_mov = NULL;			// 移动权限分类下的权限分组到默认分�?

// 预编译的 SQL 语句 - memberAuth �?
sqlite3_stmt* stmt_mauth_all = NULL;			// 分页获取所有前台权限分组数�?
sqlite3_stmt* stmt_mauth_sel = NULL;			// 分页条件查询前台权限分组数据
sqlite3_stmt* stmt_mauth_get = NULL;			// 根据 ID 获取前台权限分组记录
sqlite3_stmt* stmt_mauth_add = NULL;			// 添加前台权限分组记录
sqlite3_stmt* stmt_mauth_put = NULL;			// 修改前台权限分组记录
sqlite3_stmt* stmt_mauth_del = NULL;			// 删除前台权限分组记录（软删除�?
sqlite3_stmt* stmt_mauth_sum = NULL;			// 统计关联�?URI 权限数量
sqlite3_stmt* stmt_mauth_mov = NULL;			// 移动权限分组下的 URI 权限到默认分�?


// 预编译的 SQL 语句 - memberBalanceLog �?
sqlite3_stmt* stmt_mbalance_all = NULL;		// 分页获取余额变动日志
sqlite3_stmt* stmt_mbalance_add = NULL;		// 添加余额变动日志

// 预编译的 SQL 语句 - 前台登录相关
sqlite3_stmt* stmt_member_login = NULL;		// 根据用户名获取前台用户信�?

// 预编译的 SQL 语句 - 前台缓存相关
sqlite3_stmt* stmt_cache_mauth = NULL;			// 获取所有前台权限分组数据（缓存用）
sqlite3_stmt* stmt_cache_magroup = NULL;		// 获取所有前台权限分类数据（缓存用）
sqlite3_stmt* stmt_cache_mgroup = NULL;		// 获取所有前台用户组数据（缓存用�?
sqlite3_stmt* stmt_cache_muris = NULL;			// 获取所有前台URI记录（从uris表筛选isBackend=0�?



// 初始化全局定义
void Define_Init(XS_ServerObject objServer, XS_HostObject objHost)
{
	printf("        Define_Init \n");
	ExePath = (str)xsAppPath();
	WebPath = (str)xsHostPath(objHost);
	AppPath = xrtPathGetDir(WebPath, 0);
	DBPath = xrtPathJoin(3, AppPath, "data", "db");
	LogPath = xrtPathJoin(3, AppPath, "data", "logs");
	TempPath = xrtPathJoin(3, AppPath, "data", "temp");
	PagePath = xrtPathJoin(3, AppPath, "data", "page");
	ToolPath = xrtPathJoin(2, ExePath, "tools");
	OptionPath = xrtPathJoin(3, AppPath, "data", "options");
	InstallPath = xrtPathJoin(3, AppPath, "data", "install");
	TemplatePath = xrtPathJoin(3, AppPath, "data", "template");
	ModelPath = xrtPathJoin(2, AppPath, "script/model");
	ModelTemplatePath = xrtPathJoin(3, AppPath, "data", "model_template");
	
	// 自动创建目录
	xrtDirCreate(LogPath);
	xrtDirCreate(TempPath);
	xrtDirCreate(ModelPath);
	xrtDirCreate(ModelTemplatePath);
}



// 卸载全局数据
void Define_Unit()
{
	printf("        Define_Unit \n");
	xrtFree(AppPath);
	xrtFree(DBPath);
	xrtFree(LogPath);
	xrtFree(TempPath);
	xrtFree(PagePath);
	xrtFree(ToolPath);
	xrtFree(OptionPath);
	xrtFree(InstallPath);
	xrtFree(TemplatePath);
	xrtFree(ModelPath);
	xrtFree(ModelTemplatePath);
}


