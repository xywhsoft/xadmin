xvalue* G_CACHE_RoleAuth = NULL;					// 角色权限缓存 - 后端鉴权查表用
xvalue* G_CACHE_Auth = NULL;						// 权限分组缓存 - 前端列表渲染用
xvalue* G_CACHE_Group = NULL;					// 权限分类缓存 - 前端列表渲染用
xvalue* G_CACHE_Role = NULL;						// 角色列表缓存 - 前端列表渲染用
xvalue* G_CACHE_RoleAuthIndex = NULL;			// role id -> permission array index
xvalue* G_CACHE_RoleAuthLevel = NULL;			// role id -> authLevel

// 前台全局权限缓存
xvalue* G_CACHE_MemberGroupAuth = NULL;			// 前台用户组权限缓存 - 鉴权查表用
xvalue* G_CACHE_MemberAuth = NULL;				// 前台权限分组缓存
xvalue* G_CACHE_MemberAuthGroup = NULL;			// 前台权限分类缓存
xvalue* G_CACHE_MemberGroup = NULL;				// 前台用户组列表缓存
xvalue* G_CACHE_MemberGroupAuthIndex = NULL;	// group id -> permission array index
xvalue* G_CACHE_MemberGroupAuthLevel = NULL;	// group id -> authLevel



// 预编译的 SQL 语句 - uris
sqlite3_stmt* stmt_uris_all = NULL;				// 分页获取所有 URI 数据
sqlite3_stmt* stmt_uris_sel = NULL;				// 分页条件查询 URI 数据
sqlite3_stmt* stmt_uris_add = NULL;				// 添加 URI 记录
sqlite3_stmt* stmt_uris_del = NULL;				// 删除 URI 记录
sqlite3_stmt* stmt_uris_put = NULL;				// 修改 URI 记录
sqlite3_stmt* stmt_uris_get = NULL;				// 根据 ID 获取 URI 记录

// 预编译的 SQL 语句 - auth
sqlite3_stmt* stmt_auth_all = NULL;				// 分页获取所有权限组数据
sqlite3_stmt* stmt_auth_sel = NULL;				// 分页条件查询权限组数据
sqlite3_stmt* stmt_auth_add = NULL;				// 添加权限组记录
sqlite3_stmt* stmt_auth_del = NULL;				// 删除权限组记录（软删除）
sqlite3_stmt* stmt_auth_put = NULL;				// 修改权限组记录
sqlite3_stmt* stmt_auth_get = NULL;				// 根据 ID 获取权限组记录
sqlite3_stmt* stmt_auth_sum = NULL;				// 统计关联的 URI 权限数量
sqlite3_stmt* stmt_auth_mov = NULL;				// 移动权限组下的 URI 权限到默认分组

// 预编译的 SQL 语句 - authGroup
sqlite3_stmt* stmt_group_all = NULL;			// 分页获取所有权限分类数据
sqlite3_stmt* stmt_group_sel = NULL;			// 分页条件查询权限分类数据
sqlite3_stmt* stmt_group_add = NULL;			// 添加权限分类记录
sqlite3_stmt* stmt_group_del = NULL;			// 删除权限分类记录（软删除）
sqlite3_stmt* stmt_group_put = NULL;			// 修改权限分类记录
sqlite3_stmt* stmt_group_get = NULL;			// 根据 ID 获取权限分类记录
sqlite3_stmt* stmt_group_sum = NULL;			// 统计关联的权限组数量
sqlite3_stmt* stmt_group_mov = NULL;			// 移动权限分类下的权限组到默认分类

// 预编译的 SQL 语句 - role
sqlite3_stmt* stmt_role_all = NULL;				// 分页获取所有角色数据
sqlite3_stmt* stmt_role_sel = NULL;				// 分页条件查询角色数据
sqlite3_stmt* stmt_role_get = NULL;				// 根据 ID 获取角色记录
sqlite3_stmt* stmt_role_add = NULL;				// 添加角色记录
sqlite3_stmt* stmt_role_put = NULL;				// 修改角色记录
sqlite3_stmt* stmt_role_del = NULL;				// 删除角色记录（软删除）
sqlite3_stmt* stmt_role_sum = NULL;				// 统计关联的用户数量

// 预编译的 SQL 语句 - user
sqlite3_stmt* stmt_user_all = NULL;				// 分页获取所有用户数据
sqlite3_stmt* stmt_user_sel = NULL;
// L1：auth 系列表计数独立语句
sqlite3_stmt* stmt_uris_count_all = NULL;
sqlite3_stmt* stmt_uris_count_sel = NULL;
sqlite3_stmt* stmt_auth_count_all = NULL;
sqlite3_stmt* stmt_auth_count_sel = NULL;
sqlite3_stmt* stmt_group_count_all = NULL;
sqlite3_stmt* stmt_group_count_sel = NULL;
sqlite3_stmt* stmt_role_count_all = NULL;
sqlite3_stmt* stmt_role_count_sel = NULL;
sqlite3_stmt* stmt_user_count_all = NULL;
sqlite3_stmt* stmt_user_count_sel = NULL;
				// 分页条件查询用户数据
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

// 预编译的 SQL 语句 - member
sqlite3_stmt* stmt_member_all = NULL;			// 分页获取所有前台用户数据
sqlite3_stmt* stmt_member_sel = NULL;			// 分页条件查询前台用户数据
sqlite3_stmt* stmt_member_count_all = NULL;	// L3：member 全量计数（预编译，原为每请求 prepare）
sqlite3_stmt* stmt_member_count_sel = NULL;	// L2/L9：member 按搜索条件计数
sqlite3_stmt* stmt_member_get = NULL;			// 根据 ID 获取前台用户记录
sqlite3_stmt* stmt_member_add = NULL;			// 添加前台用户记录
sqlite3_stmt* stmt_member_put = NULL;			// 修改前台用户记录
sqlite3_stmt* stmt_member_del = NULL;			// 删除前台用户记录（软删除）
sqlite3_stmt* stmt_member_chk = NULL;			// 检查用户名是否已存在
sqlite3_stmt* stmt_member_pwd = NULL;			// 修改用户密码
sqlite3_stmt* stmt_member_balance = NULL;		// 修改用户余额
sqlite3_stmt* stmt_member_profile = NULL;			// 会员自助资料更新（F1：不触碰状态与权限列）

// 预编译的 SQL 语句 - memberGroup
sqlite3_stmt* stmt_mgroup_all = NULL;			// 分页获取所有前台用户组数据
sqlite3_stmt* stmt_mgroup_sel = NULL;			// 分页条件查询前台用户组数据
sqlite3_stmt* stmt_mgroup_get = NULL;			// 根据 ID 获取前台用户组记录
sqlite3_stmt* stmt_mgroup_add = NULL;			// 添加前台用户组记录
sqlite3_stmt* stmt_mgroup_put = NULL;			// 修改前台用户组记录
sqlite3_stmt* stmt_mgroup_del = NULL;			// 删除前台用户组记录（软删除）
sqlite3_stmt* stmt_mgroup_sum = NULL;			// 统计关联的用户数量

// 预编译的 SQL 语句 - memberAuthGroup
sqlite3_stmt* stmt_magroup_all = NULL;			// 分页获取所有前台权限分类数据
sqlite3_stmt* stmt_magroup_sel = NULL;			// 分页条件查询前台权限分类数据
sqlite3_stmt* stmt_magroup_get = NULL;			// 根据 ID 获取前台权限分类记录
sqlite3_stmt* stmt_magroup_add = NULL;			// 添加前台权限分类记录
sqlite3_stmt* stmt_magroup_put = NULL;			// 修改前台权限分类记录
sqlite3_stmt* stmt_magroup_del = NULL;			// 删除前台权限分类记录（软删除）
sqlite3_stmt* stmt_magroup_sum = NULL;			// 统计关联的权限分组数量
sqlite3_stmt* stmt_magroup_mov = NULL;			// 移动权限分类下的权限分组到默认分类

// 预编译的 SQL 语句 - memberAuth
sqlite3_stmt* stmt_mauth_all = NULL;			// 分页获取所有前台权限分组数据
sqlite3_stmt* stmt_mauth_sel = NULL;			// 分页条件查询前台权限分组数据
sqlite3_stmt* stmt_mauth_get = NULL;			// 根据 ID 获取前台权限分组记录
sqlite3_stmt* stmt_mauth_add = NULL;			// 添加前台权限分组记录
sqlite3_stmt* stmt_mauth_put = NULL;			// 修改前台权限分组记录
sqlite3_stmt* stmt_mauth_del = NULL;			// 删除前台权限分组记录（软删除）
sqlite3_stmt* stmt_mauth_sum = NULL;			// 统计关联的 URI 权限数量
sqlite3_stmt* stmt_mauth_mov = NULL;			// 移动权限分组下的 URI 权限到默认分类


// 预编译的 SQL 语句 - memberBalanceLog
sqlite3_stmt* stmt_mbalance_all = NULL;		// 分页获取余额变动日志
sqlite3_stmt* stmt_mbalance_add = NULL;		// 添加余额变动日志

// 预编译的 SQL 语句 - 前台登录相关
sqlite3_stmt* stmt_member_login = NULL;		// 根据用户名获取前台用户信息

// 预编译的 SQL 语句 - 前台缓存相关
sqlite3_stmt* stmt_cache_mauth = NULL;			// 获取所有前台权限分组数据（缓存用）
sqlite3_stmt* stmt_cache_magroup = NULL;		// 获取所有前台权限分类数据（缓存用）
sqlite3_stmt* stmt_cache_mgroup = NULL;		// 获取所有前台用户组数据（缓存用）
sqlite3_stmt* stmt_cache_muris = NULL;			// 获取所有前台URI记录（从uris表筛选isBackend=0）



// 初始化全局定义

/* F6：缓存换代的延迟退役槽。写路径只换指针，旧缓存压入此处，
 * 由 Session_Tick（5 分钟周期，请求锁内）统一销毁——20k 级角色时
 * 旧缓存销毁耗时可达秒级（内嵌 xrt 分配器路径），不能阻塞写请求。
 * 槽满时退化为立即销毁（生产规模几十个角色，销毁为微秒级）。 */
#define CACHE_RETIRE_SLOTS 256
static xvalue* G_RetiredValues[CACHE_RETIRE_SLOTS];
static size_t G_RetiredCount;
static void CacheRetireSweep(void);
static void CacheRetire(xvalue* value)
{
	if (value == NULL) return;
	if (G_RetiredCount == CACHE_RETIRE_SLOTS) {
		/* 满槽时清扫最旧一半而非当值销毁：写延迟增量有界，内存同样有界。 */
		size_t i, half = CACHE_RETIRE_SLOTS / 2;
		for (i = 0; i < half; i++) xrtValueRelease(G_RetiredValues[i]);
		memmove(G_RetiredValues, G_RetiredValues + half,
		        (CACHE_RETIRE_SLOTS - half) * sizeof(xvalue*));
		G_RetiredCount -= half;
	}
	G_RetiredValues[G_RetiredCount++] = value;
}
static void CacheRetireSweep(void)
{
	size_t i;
	for (i = 0; i < G_RetiredCount; i++) xrtValueRelease(G_RetiredValues[i]);
	G_RetiredCount = 0;
}


