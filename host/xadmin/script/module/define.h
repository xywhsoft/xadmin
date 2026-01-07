


// 全局定义
#define SQL_PREPARE_DEFAULT		SQLITE_PREPARE_PERSISTENT | SQLITE_PREPARE_DONT_LOG



// 补充 API 定义
uint64 GetTickCount64();



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



// HTML 载荷类型
str HTTP_CT_HTML = "Content-Type: text/html\r\n";
str HTTP_CT_TEXT = "Content-Type: text/plain\r\n";
str HTTP_CT_JSON = "Content-Type: application/json\r\n";



// 服务端二次 SHA-256 哈希函数
// 计算：SHA256(用户名 + salt + 客户端哈希值)
// salt 为每个用户独立的随机盐（存储在数据库 user.salt 字段）
// 返回动态分配的哈希字符串，调用方需要 xrtFree 释放
str ServerHashPassword(str user, str salt, str clientHash)
{
	str sCombined = xrtFormat("%s%s%s", user, salt, clientHash);
	uint8 hash[32];
	mg_sha256_ctx ctx;
	mg_sha256_init(&ctx);
	mg_sha256_update(&ctx, (uint8*)sCombined, strlen(sCombined));
	mg_sha256_final(hash, &ctx);
	str sPwdHash = xrtHexEncode(hash, 32);
	xrtFree(sCombined);
	return sPwdHash;
}



// 是否已安装
bool G_Install = FALSE;



// 全局 Session 表 - 后台管理员
xvalue G_AdminSession = NULL;

// 全局 Session 表 - 前台用户
xvalue G_MemberSession = NULL;



// 全局配置表
xvalue G_Option = NULL;



// 全局数据库对象
XDO_Connect G_DB = NULL;



// 全局服务缓存表
xlist G_Services = NULL;



// 全局静态路由表 - HTTP
typedef struct {
	
	// 对应 URI 的处理函数
	void (*Proc)(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm);
	
	// 是否必须鉴权才能访问
	bool bAuth;
	
	// 是否是后台 URI（TRUE为后台、FALSE为前台）
	bool bAdmin;
	
	// 是否记录访问日志（后台选项）
	bool bPutLog;
	
	// 是否保持活跃（访问了保持活跃的链接，会自动延长 session 寿命）
	bool bActive;
	
	// 所属权限组ID
	uint32 AuthID;
	
	// 权限级别（0为不限制，否则必须用户组具备大于等于这个数字的权限级别才能访问）
	uint32 AuthLevel;
	
} RouteInfo;
xdict G_StaticRouteTableHTTP;

// 添加全局静态路由表项 - HTTP
void AddStaticRouteHTTP(str uri, void* proc, bool bAuth, bool bAdmin, bool bPutLog, bool bActive)
{
	RouteInfo* pInfo = xrtDictSet(G_StaticRouteTableHTTP, uri, strlen(uri), NULL);
	if ( pInfo ) {
		pInfo->Proc = proc;
		pInfo->bAuth = bAuth;
		pInfo->bAdmin = bAdmin;
		pInfo->bPutLog = bPutLog;
		pInfo->bActive = bActive;
		pInfo->AuthID = 0;
		pInfo->AuthLevel = 0;
	} else {
		printf("add static http route failed : %s.\n", uri);
	}
}



// 后台全局权限表
xvalue G_CACHE_RoleAuth = NULL;					// 角色权限缓存 - 后端鉴权查表用
xvalue G_CACHE_Auth = NULL;						// 权限分组缓存 - 前端列表渲染用
xvalue G_CACHE_Group = NULL;					// 权限分类缓存 - 前端列表渲染用
xvalue G_CACHE_Role = NULL;						// 角色列表缓存 - 前端列表渲染用

// 前台全局权限表
xvalue G_CACHE_MemberGroupAuth = NULL;			// 前台用户组权限缓存 - 鉴权查表用
xvalue G_CACHE_MemberAuth = NULL;				// 前台权限分组缓存
xvalue G_CACHE_MemberAuthGroup = NULL;			// 前台权限分类缓存
xvalue G_CACHE_MemberGroup = NULL;				// 前台用户组列表缓存



// 预编译的 SQL 语句 - uris 表
sqlite3_stmt* stmt_uris_all = NULL;				// 分页获取所有 URI 数据
sqlite3_stmt* stmt_uris_sel = NULL;				// 分页条件查询 URI 数据
sqlite3_stmt* stmt_uris_add = NULL;				// 添加 URI 记录
sqlite3_stmt* stmt_uris_del = NULL;				// 删除 URI 记录
sqlite3_stmt* stmt_uris_put = NULL;				// 修改 URI 记录
sqlite3_stmt* stmt_uris_get = NULL;				// 根据 ID 获取 URI 记录

// 预编译的 SQL 语句 - auth 表
sqlite3_stmt* stmt_auth_all = NULL;				// 分页获取所有权限组数据
sqlite3_stmt* stmt_auth_sel = NULL;				// 分页条件查询权限组数据
sqlite3_stmt* stmt_auth_add = NULL;				// 添加权限组记录
sqlite3_stmt* stmt_auth_del = NULL;				// 删除权限组记录（软删除）
sqlite3_stmt* stmt_auth_put = NULL;				// 修改权限组记录
sqlite3_stmt* stmt_auth_get = NULL;				// 根据 ID 获取权限组记录
sqlite3_stmt* stmt_auth_sum = NULL;				// 统计关联的 URI 权限数量
sqlite3_stmt* stmt_auth_mov = NULL;				// 移动权限组下的 URI 权限到默认分组

// 预编译的 SQL 语句 - authGroup 表
sqlite3_stmt* stmt_group_all = NULL;			// 分页获取所有权限分类数据
sqlite3_stmt* stmt_group_sel = NULL;			// 分页条件查询权限分类数据
sqlite3_stmt* stmt_group_add = NULL;			// 添加权限分类记录
sqlite3_stmt* stmt_group_del = NULL;			// 删除权限分类记录（软删除）
sqlite3_stmt* stmt_group_put = NULL;			// 修改权限分类记录
sqlite3_stmt* stmt_group_get = NULL;			// 根据 ID 获取权限分类记录
sqlite3_stmt* stmt_group_sum = NULL;			// 统计关联的权限组数量
sqlite3_stmt* stmt_group_mov = NULL;			// 移动权限分类下的权限组到默认分类

// 预编译的 SQL 语句 - role 表
sqlite3_stmt* stmt_role_all = NULL;				// 分页获取所有角色数据
sqlite3_stmt* stmt_role_sel = NULL;				// 分页条件查询角色数据
sqlite3_stmt* stmt_role_get = NULL;				// 根据 ID 获取角色记录
sqlite3_stmt* stmt_role_add = NULL;				// 添加角色记录
sqlite3_stmt* stmt_role_put = NULL;				// 修改角色记录
sqlite3_stmt* stmt_role_del = NULL;				// 删除角色记录（软删除）
sqlite3_stmt* stmt_role_sum = NULL;				// 统计关联的用户数量

// 预编译的 SQL 语句 - user 表
sqlite3_stmt* stmt_user_all = NULL;				// 分页获取所有用户数据
sqlite3_stmt* stmt_user_sel = NULL;				// 分页条件查询用户数据
sqlite3_stmt* stmt_user_get = NULL;				// 根据 ID 获取用户记录
sqlite3_stmt* stmt_user_add = NULL;				// 添加用户记录
sqlite3_stmt* stmt_user_put = NULL;				// 修改用户记录
sqlite3_stmt* stmt_user_del = NULL;				// 删除用户记录（软删除）
sqlite3_stmt* stmt_user_chk = NULL;				// 检查用户名是否已存在
sqlite3_stmt* stmt_user_pwd = NULL;				// 修改用户密码

// 预编译的 SQL 语句 - 登录相关
sqlite3_stmt* stmt_login_get = NULL;			// 根据用户名和密码获取用户信息

// 预编译的 SQL 语句 - 缓存相关
sqlite3_stmt* stmt_cache_auth = NULL;			// 获取所有权限组数据（缓存用）
sqlite3_stmt* stmt_cache_group = NULL;			// 获取所有权限分类数据（缓存用）
sqlite3_stmt* stmt_cache_role = NULL;			// 获取所有角色数据（缓存用）
sqlite3_stmt* stmt_cache_uris = NULL;			// 获取所有URI记录（用于更新URI表）

// ==================== 前台用户系统预编译SQL ====================

// 预编译的 SQL 语句 - member 表
sqlite3_stmt* stmt_member_all = NULL;			// 分页获取所有前台用户数据
sqlite3_stmt* stmt_member_sel = NULL;			// 分页条件查询前台用户数据
sqlite3_stmt* stmt_member_get = NULL;			// 根据 ID 获取前台用户记录
sqlite3_stmt* stmt_member_add = NULL;			// 添加前台用户记录
sqlite3_stmt* stmt_member_put = NULL;			// 修改前台用户记录
sqlite3_stmt* stmt_member_del = NULL;			// 删除前台用户记录（软删除）
sqlite3_stmt* stmt_member_chk = NULL;			// 检查用户名是否已存在
sqlite3_stmt* stmt_member_pwd = NULL;			// 修改用户密码
sqlite3_stmt* stmt_member_balance = NULL;		// 修改用户余额

// 预编译的 SQL 语句 - memberGroup 表
sqlite3_stmt* stmt_mgroup_all = NULL;			// 分页获取所有前台用户组数据
sqlite3_stmt* stmt_mgroup_sel = NULL;			// 分页条件查询前台用户组数据
sqlite3_stmt* stmt_mgroup_get = NULL;			// 根据 ID 获取前台用户组记录
sqlite3_stmt* stmt_mgroup_add = NULL;			// 添加前台用户组记录
sqlite3_stmt* stmt_mgroup_put = NULL;			// 修改前台用户组记录
sqlite3_stmt* stmt_mgroup_del = NULL;			// 删除前台用户组记录（软删除）
sqlite3_stmt* stmt_mgroup_sum = NULL;			// 统计关联的用户数量

// 预编译的 SQL 语句 - memberAuthGroup 表
sqlite3_stmt* stmt_magroup_all = NULL;			// 分页获取所有前台权限分类数据
sqlite3_stmt* stmt_magroup_sel = NULL;			// 分页条件查询前台权限分类数据
sqlite3_stmt* stmt_magroup_get = NULL;			// 根据 ID 获取前台权限分类记录
sqlite3_stmt* stmt_magroup_add = NULL;			// 添加前台权限分类记录
sqlite3_stmt* stmt_magroup_put = NULL;			// 修改前台权限分类记录
sqlite3_stmt* stmt_magroup_del = NULL;			// 删除前台权限分类记录（软删除）
sqlite3_stmt* stmt_magroup_sum = NULL;			// 统计关联的权限分组数量
sqlite3_stmt* stmt_magroup_mov = NULL;			// 移动权限分类下的权限分组到默认分类

// 预编译的 SQL 语句 - memberAuth 表
sqlite3_stmt* stmt_mauth_all = NULL;			// 分页获取所有前台权限分组数据
sqlite3_stmt* stmt_mauth_sel = NULL;			// 分页条件查询前台权限分组数据
sqlite3_stmt* stmt_mauth_get = NULL;			// 根据 ID 获取前台权限分组记录
sqlite3_stmt* stmt_mauth_add = NULL;			// 添加前台权限分组记录
sqlite3_stmt* stmt_mauth_put = NULL;			// 修改前台权限分组记录
sqlite3_stmt* stmt_mauth_del = NULL;			// 删除前台权限分组记录（软删除）
sqlite3_stmt* stmt_mauth_sum = NULL;			// 统计关联的 URI 权限数量
sqlite3_stmt* stmt_mauth_mov = NULL;			// 移动权限分组下的 URI 权限到默认分组

// 预编译的 SQL 语句 - memberUris 表
sqlite3_stmt* stmt_muris_all = NULL;			// 分页获取所有前台 URI 数据
sqlite3_stmt* stmt_muris_sel = NULL;			// 分页条件查询前台 URI 数据
sqlite3_stmt* stmt_muris_get = NULL;			// 根据 ID 获取前台 URI 记录
sqlite3_stmt* stmt_muris_add = NULL;			// 添加前台 URI 记录
sqlite3_stmt* stmt_muris_put = NULL;			// 修改前台 URI 记录
sqlite3_stmt* stmt_muris_del = NULL;			// 删除前台 URI 记录

// 预编译的 SQL 语句 - memberBalanceLog 表
sqlite3_stmt* stmt_mbalance_all = NULL;		// 分页获取余额变动日志
sqlite3_stmt* stmt_mbalance_add = NULL;		// 添加余额变动日志

// 预编译的 SQL 语句 - 前台登录相关
sqlite3_stmt* stmt_member_login = NULL;		// 根据用户名获取前台用户信息

// 预编译的 SQL 语句 - 前台缓存相关
sqlite3_stmt* stmt_cache_mauth = NULL;			// 获取所有前台权限分组数据（缓存用）
sqlite3_stmt* stmt_cache_magroup = NULL;		// 获取所有前台权限分类数据（缓存用）
sqlite3_stmt* stmt_cache_mgroup = NULL;		// 获取所有前台用户组数据（缓存用）
sqlite3_stmt* stmt_cache_muris = NULL;			// 获取所有前台URI记录（用于更新URI表）



// 初始化全局定义
void Define_Init(XS_ServerObject objServer, XS_HostObject objHost)
{
	printf("        Define_Init \n");
	ExePath = xCore->AppPath;
	WebPath = objHost->Path;
	AppPath = xrtPathGetDir(WebPath, 0);
	DBPath = xrtPathJoin(3, AppPath, "data", "db");
	LogPath = xrtPathJoin(3, AppPath, "data", "logs");
	TempPath = xrtPathJoin(3, AppPath, "data", "temp");
	PagePath = xrtPathJoin(3, AppPath, "data", "page");
	ToolPath = xrtPathJoin(2, ExePath, "tools");
	OptionPath = xrtPathJoin(3, AppPath, "data", "options");
	InstallPath = xrtPathJoin(3, AppPath, "data", "install");
	TemplatePath = xrtPathJoin(3, AppPath, "data", "template");
	
	// 自动创建目录
	xrtDirCreate(LogPath);
	xrtDirCreate(TempPath);
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
}


