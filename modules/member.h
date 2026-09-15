


// 前台用户模块 - SQL 预编译和基础操作



// ==================== SQL 预编译初始化 ====================

// 初始化前台用户模块
void Member_Init()
{
	printf("        Member_Init \n");
	sqlite3* db = G_DB;
	
	// member 表 - 分页获取所有前台用户数据
	sqlite3_prepare_v3(db,
		"SELECT id, username, groupId, authLevel, balance, nickname, email, phone, avatar, status, createTime, updateTime "
		"FROM member WHERE isDelete = 0 ORDER BY id DESC LIMIT ? OFFSET ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_member_all, NULL);
	
	// member 表 - 分页条件查询前台用户数据
	sqlite3_prepare_v3(db,
		"SELECT id, username, groupId, authLevel, balance, nickname, email, phone, avatar, status, createTime, updateTime "
		"FROM member WHERE isDelete = 0 AND (username LIKE ? OR nickname LIKE ? OR email LIKE ?) ORDER BY id DESC LIMIT ? OFFSET ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_member_sel, NULL);

	// L2/L3/L9：计数语句（预编译）+ LIKE 转义子句
	sqlite3_prepare_v3(db,
		"SELECT COUNT(*) FROM member WHERE isDelete = 0",
		-1, SQL_PREPARE_DEFAULT, &stmt_member_count_all, NULL);
	sqlite3_prepare_v3(db,
		"SELECT COUNT(*) FROM member WHERE isDelete = 0 AND (username LIKE ?1 OR nickname LIKE ?1 OR email LIKE ?1)",
		-1, SQL_PREPARE_DEFAULT, &stmt_member_count_sel, NULL);
	
	// member 表 - 根据 ID 获取前台用户记录
	sqlite3_prepare_v3(db,
		"SELECT id, username, groupId, authLevel, balance, nickname, email, phone, avatar, status, createTime, updateTime "
		"FROM member WHERE id = ? AND isDelete = 0",
		-1, SQL_PREPARE_DEFAULT, &stmt_member_get, NULL);
	
	// member 表 - 添加前台用户记录
	sqlite3_prepare_v3(db,
		"INSERT INTO member (username, salt, pwd, groupId, authLevel, balance, nickname, email, phone, avatar, status, createTime, updateTime, isDelete) "
		"VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)",
		-1, SQL_PREPARE_DEFAULT, &stmt_member_add, NULL);
	
	// member 表 - 修改前台用户记录
	sqlite3_prepare_v3(db,
		"UPDATE member SET groupId = ?, authLevel = ?, nickname = ?, email = ?, phone = ?, avatar = ?, status = ?, updateTime = ? WHERE id = ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_member_put, NULL);
	
	// member 表 - 删除前台用户记录（软删除）
	sqlite3_prepare_v3(db,
		"UPDATE member SET isDelete = 1, updateTime = ? WHERE id = ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_member_del, NULL);
	
	// member 表 - 检查用户名是否已存在
	sqlite3_prepare_v3(db,
		"SELECT COUNT(*) FROM member WHERE username = ? AND isDelete = 0",
		-1, SQL_PREPARE_DEFAULT, &stmt_member_chk, NULL);
	
	// member 表 - 修改用户密码
	sqlite3_prepare_v3(db,
		"UPDATE member SET salt = ?, pwd = ?, updateTime = ? WHERE id = ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_member_pwd, NULL);
	
	// member 表 - 修改用户余额
	sqlite3_prepare_v3(db,
		"UPDATE member SET balance = ?, updateTime = ? WHERE id = ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_member_balance, NULL);

	// member - 会员自助资料更新（F1：仅资料字段，杜绝 profile 接口覆盖 status/权限）
	sqlite3_prepare_v3(db,
		"UPDATE member SET nickname = ?, email = ?, phone = ?, avatar = ?, updateTime = ? WHERE id = ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_member_profile, NULL);
	
	// member 表 - 前台登录查询
	sqlite3_prepare_v3(db,
		"SELECT id, username, salt, pwd, groupId, authLevel, balance, nickname, status "
		"FROM member WHERE username = ? AND isDelete = 0 AND status = 1",
		-1, SQL_PREPARE_DEFAULT, &stmt_member_login, NULL);
	
	// memberGroup 表 - 分页获取所有前台用户组数据 (包含 total 计数)
	sqlite3_prepare_v3(db,
		"SELECT id, name, desc, authList, authLevel, createTime, updateTime, "
		"(SELECT COUNT(*) FROM memberGroup WHERE isDelete = 0) AS total "
		"FROM memberGroup WHERE isDelete = 0 ORDER BY id ASC LIMIT ? OFFSET ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_mgroup_all, NULL);
	
	// memberGroup 表 - 分页条件查询前台用户组数据 (支持搜索名称和描述)
	sqlite3_prepare_v3(db,
		"SELECT id, name, desc, authList, authLevel, createTime, updateTime, "
		"(SELECT COUNT(*) FROM memberGroup WHERE isDelete = 0 AND (name LIKE ?1 OR desc LIKE ?1)) AS total "
		"FROM memberGroup WHERE isDelete = 0 AND (name LIKE ?1 OR desc LIKE ?1) ORDER BY id ASC LIMIT ?2 OFFSET ?3",
		-1, SQL_PREPARE_DEFAULT, &stmt_mgroup_sel, NULL);
	
	// memberGroup 表 - 根据 ID 获取前台用户组记录
	sqlite3_prepare_v3(db,
		"SELECT id, name, desc, authList, authLevel, createTime, updateTime "
		"FROM memberGroup WHERE id = ? AND isDelete = 0",
		-1, SQL_PREPARE_DEFAULT, &stmt_mgroup_get, NULL);
	
	// memberGroup 表 - 添加前台用户组记录
	sqlite3_prepare_v3(db,
		"INSERT INTO memberGroup (name, desc, authList, authLevel, createTime, updateTime, isDelete) VALUES (?, ?, ?, ?, ?, ?, 0)",
		-1, SQL_PREPARE_DEFAULT, &stmt_mgroup_add, NULL);
	
	// memberGroup 表 - 修改前台用户组记录
	sqlite3_prepare_v3(db,
		"UPDATE memberGroup SET name = ?, desc = ?, authList = ?, authLevel = ?, updateTime = ? WHERE id = ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_mgroup_put, NULL);
	
	// memberGroup 表 - 删除前台用户组记录（软删除）
	sqlite3_prepare_v3(db,
		"UPDATE memberGroup SET isDelete = 1, updateTime = ? WHERE id = ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_mgroup_del, NULL);
	
	// memberGroup 表 - 统计关联的用户数量
	sqlite3_prepare_v3(db,
		"SELECT COUNT(*) FROM member WHERE groupId = ? AND isDelete = 0",
		-1, SQL_PREPARE_DEFAULT, &stmt_mgroup_sum, NULL);
	
	// memberAuthGroup 表 - 分页获取所有前台权限分类数据 (包含 total 计数)
	sqlite3_prepare_v3(db,
		"SELECT id, name, desc, sort, createTime, updateTime, "
		"(SELECT COUNT(*) FROM memberAuthGroup WHERE isDelete = 0) AS total "
		"FROM memberAuthGroup WHERE isDelete = 0 ORDER BY sort ASC LIMIT ? OFFSET ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_magroup_all, NULL);
	
	// memberAuthGroup 表 - 分页条件查询前台权限分类数据 (支持搜索名称和描述)
	sqlite3_prepare_v3(db,
		"SELECT id, name, desc, sort, createTime, updateTime, "
		"(SELECT COUNT(*) FROM memberAuthGroup WHERE isDelete = 0 AND (name LIKE ?1 OR desc LIKE ?1)) AS total "
		"FROM memberAuthGroup WHERE isDelete = 0 AND (name LIKE ?1 OR desc LIKE ?1) ORDER BY sort ASC LIMIT ?2 OFFSET ?3",
		-1, SQL_PREPARE_DEFAULT, &stmt_magroup_sel, NULL);
	
	// memberAuthGroup 表 - 根据 ID 获取前台权限分类记录
	sqlite3_prepare_v3(db,
		"SELECT id, name, desc, sort, createTime, updateTime FROM memberAuthGroup WHERE id = ? AND isDelete = 0",
		-1, SQL_PREPARE_DEFAULT, &stmt_magroup_get, NULL);
	
	// memberAuthGroup 表 - 添加前台权限分类记录
	sqlite3_prepare_v3(db,
		"INSERT INTO memberAuthGroup (name, desc, sort, createTime, updateTime, isDelete) VALUES (?, ?, ?, ?, ?, 0)",
		-1, SQL_PREPARE_DEFAULT, &stmt_magroup_add, NULL);
	
	// memberAuthGroup 表 - 修改前台权限分类记录
	sqlite3_prepare_v3(db,
		"UPDATE memberAuthGroup SET name = ?, desc = ?, sort = ?, updateTime = ? WHERE id = ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_magroup_put, NULL);
	
	// memberAuthGroup 表 - 删除前台权限分类记录（软删除）
	sqlite3_prepare_v3(db,
		"UPDATE memberAuthGroup SET isDelete = 1, updateTime = ? WHERE id = ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_magroup_del, NULL);
	
	// memberAuthGroup 表 - 统计关联的权限分组数量
	sqlite3_prepare_v3(db,
		"SELECT COUNT(*) FROM memberAuth WHERE groupID = ? AND isDelete = 0",
		-1, SQL_PREPARE_DEFAULT, &stmt_magroup_sum, NULL);
	
	// memberAuthGroup 表 - 移动权限分类下的权限分组到默认分组
	sqlite3_prepare_v3(db,
		"UPDATE memberAuth SET groupID = 1, updateTime = ? WHERE groupID = ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_magroup_mov, NULL);
	
	// memberAuth 表 - 分页获取所有前台权限分组数据 (JOIN memberAuthGroup 获取分类名称)
	sqlite3_prepare_v3(db,
		"SELECT a.id, a.groupID, a.name, a.desc, a.sort, a.createTime, a.updateTime, g.name AS groupName, "
		"(SELECT COUNT(*) FROM memberAuth WHERE isDelete = 0) AS total "
		"FROM memberAuth a LEFT JOIN memberAuthGroup g ON a.groupID = g.id "
		"WHERE a.isDelete = 0 ORDER BY a.sort ASC LIMIT ? OFFSET ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_mauth_all, NULL);
	
	// memberAuth 表 - 分页条件查询前台权限分组数据 (JOIN memberAuthGroup 获取分类名称)
	sqlite3_prepare_v3(db,
		"SELECT a.id, a.groupID, a.name, a.desc, a.sort, a.createTime, a.updateTime, g.name AS groupName, "
		"(SELECT COUNT(*) FROM memberAuth WHERE isDelete = 0 AND (name LIKE ?1 OR desc LIKE ?1)) AS total "
		"FROM memberAuth a LEFT JOIN memberAuthGroup g ON a.groupID = g.id "
		"WHERE a.isDelete = 0 AND (a.name LIKE ?1 OR a.desc LIKE ?1) ORDER BY a.sort ASC LIMIT ?2 OFFSET ?3",
		-1, SQL_PREPARE_DEFAULT, &stmt_mauth_sel, NULL);
	
	// memberAuth 表 - 根据 ID 获取前台权限分组记录
	sqlite3_prepare_v3(db,
		"SELECT id, groupID, name, desc, sort, createTime, updateTime FROM memberAuth WHERE id = ? AND isDelete = 0",
		-1, SQL_PREPARE_DEFAULT, &stmt_mauth_get, NULL);
	
	// memberAuth 表 - 添加前台权限分组记录
	sqlite3_prepare_v3(db,
		"INSERT INTO memberAuth (groupID, name, desc, sort, createTime, updateTime, isDelete) VALUES (?, ?, ?, ?, ?, ?, 0)",
		-1, SQL_PREPARE_DEFAULT, &stmt_mauth_add, NULL);
	
	// memberAuth 表 - 修改前台权限分组记录
	sqlite3_prepare_v3(db,
		"UPDATE memberAuth SET groupID = ?, name = ?, desc = ?, sort = ?, updateTime = ? WHERE id = ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_mauth_put, NULL);
	
	// memberAuth 表 - 删除前台权限分组记录（软删除）
	sqlite3_prepare_v3(db,
		"UPDATE memberAuth SET isDelete = 1, updateTime = ? WHERE id = ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_mauth_del, NULL);
	
	// memberAuth 表 - 统计关联的 URI 权限数量（从 uris 表筛选前台 URI）
	sqlite3_prepare_v3(db,
		"SELECT COUNT(*) FROM uris WHERE authID = ? AND isBackend = 0",
		-1, SQL_PREPARE_DEFAULT, &stmt_mauth_sum, NULL);
	
	// memberAuth 表 - 移动权限分组下的 URI 权限到默认分组（仅前台 URI）
	sqlite3_prepare_v3(db,
		"UPDATE uris SET authID = 1, updateTime = ? WHERE authID = ? AND isBackend = 0",
		-1, SQL_PREPARE_DEFAULT, &stmt_mauth_mov, NULL);
	
	// memberBalanceLog 表 - 分页获取余额变动日志
	sqlite3_prepare_v3(db,
		"SELECT id, memberId, type, amount, balance, remark, operator, createTime "
		"FROM memberBalanceLog WHERE memberId = ? ORDER BY id DESC LIMIT ? OFFSET ?",
		-1, SQL_PREPARE_DEFAULT, &stmt_mbalance_all, NULL);
	
	// memberBalanceLog 表 - 添加余额变动日志
	sqlite3_prepare_v3(db,
		"INSERT INTO memberBalanceLog (memberId, type, amount, balance, remark, operator, createTime) VALUES (?, ?, ?, ?, ?, ?, ?)",
		-1, SQL_PREPARE_DEFAULT, &stmt_mbalance_add, NULL);
	
	// 缓存用 SQL - 获取所有前台权限分组数据
	sqlite3_prepare_v3(db,
		"SELECT id, groupID, name, desc, sort FROM memberAuth WHERE isDelete = 0 ORDER BY sort ASC",
		-1, SQL_PREPARE_DEFAULT, &stmt_cache_mauth, NULL);
	
	// 缓存用 SQL - 获取所有前台权限分类数据
	sqlite3_prepare_v3(db,
		"SELECT id, name, desc, sort FROM memberAuthGroup WHERE isDelete = 0 ORDER BY sort ASC",
		-1, SQL_PREPARE_DEFAULT, &stmt_cache_magroup, NULL);
	
	// 缓存用 SQL - 获取所有前台用户组数据
	sqlite3_prepare_v3(db,
		"SELECT id, name, desc, authList, authLevel FROM memberGroup WHERE isDelete = 0 ORDER BY id ASC",
		-1, SQL_PREPARE_DEFAULT, &stmt_cache_mgroup, NULL);
	
	// 缓存用 SQL - 获取所有前台 URI 记录（从 uris 表筛选 isBackend=0 的前台接口）
	sqlite3_prepare_v3(db,
		"SELECT id, authID, uri, desc, sort FROM uris WHERE isBackend = 0 ORDER BY sort ASC",
		-1, SQL_PREPARE_DEFAULT, &stmt_cache_muris, NULL);
}



// 卸载前台用户模块
void Member_Unit()
{
	printf("        Member_Unit \n");
	
	// 释放 member 表预编译语句
	if (stmt_member_all) sqlite3_finalize(stmt_member_all);
	if (stmt_member_sel) sqlite3_finalize(stmt_member_sel);
	if (stmt_member_get) sqlite3_finalize(stmt_member_get);
	if (stmt_member_add) sqlite3_finalize(stmt_member_add);
	if (stmt_member_put) sqlite3_finalize(stmt_member_put);
	if (stmt_member_del) sqlite3_finalize(stmt_member_del);
	if (stmt_member_chk) sqlite3_finalize(stmt_member_chk);
	if (stmt_member_pwd) sqlite3_finalize(stmt_member_pwd);
	if (stmt_member_balance) sqlite3_finalize(stmt_member_balance);
	if (stmt_member_profile) sqlite3_finalize(stmt_member_profile);
	if (stmt_member_login) sqlite3_finalize(stmt_member_login);
	
	// 释放 memberGroup 表预编译语句
	if (stmt_mgroup_all) sqlite3_finalize(stmt_mgroup_all);
	if (stmt_mgroup_sel) sqlite3_finalize(stmt_mgroup_sel);
	if (stmt_mgroup_get) sqlite3_finalize(stmt_mgroup_get);
	if (stmt_mgroup_add) sqlite3_finalize(stmt_mgroup_add);
	if (stmt_mgroup_put) sqlite3_finalize(stmt_mgroup_put);
	if (stmt_mgroup_del) sqlite3_finalize(stmt_mgroup_del);
	if (stmt_mgroup_sum) sqlite3_finalize(stmt_mgroup_sum);
	
	// 释放 memberAuthGroup 表预编译语句
	if (stmt_magroup_all) sqlite3_finalize(stmt_magroup_all);
	if (stmt_magroup_sel) sqlite3_finalize(stmt_magroup_sel);
	if (stmt_magroup_get) sqlite3_finalize(stmt_magroup_get);
	if (stmt_magroup_add) sqlite3_finalize(stmt_magroup_add);
	if (stmt_magroup_put) sqlite3_finalize(stmt_magroup_put);
	if (stmt_magroup_del) sqlite3_finalize(stmt_magroup_del);
	if (stmt_magroup_sum) sqlite3_finalize(stmt_magroup_sum);
	if (stmt_magroup_mov) sqlite3_finalize(stmt_magroup_mov);
	
	// 释放 memberAuth 表预编译语句
	if (stmt_mauth_all) sqlite3_finalize(stmt_mauth_all);
	if (stmt_mauth_sel) sqlite3_finalize(stmt_mauth_sel);
	if (stmt_mauth_get) sqlite3_finalize(stmt_mauth_get);
	if (stmt_mauth_add) sqlite3_finalize(stmt_mauth_add);
	if (stmt_mauth_put) sqlite3_finalize(stmt_mauth_put);
	if (stmt_mauth_del) sqlite3_finalize(stmt_mauth_del);
	if (stmt_mauth_sum) sqlite3_finalize(stmt_mauth_sum);
	if (stmt_mauth_mov) sqlite3_finalize(stmt_mauth_mov);
	
	
	// 释放 memberBalanceLog 表预编译语句
	if (stmt_mbalance_all) sqlite3_finalize(stmt_mbalance_all);
	if (stmt_mbalance_add) sqlite3_finalize(stmt_mbalance_add);
	
	// 释放缓存用预编译语句
	if (stmt_cache_mauth) sqlite3_finalize(stmt_cache_mauth);
	if (stmt_cache_magroup) sqlite3_finalize(stmt_cache_magroup);
	if (stmt_cache_mgroup) sqlite3_finalize(stmt_cache_mgroup);
	if (stmt_cache_muris) sqlite3_finalize(stmt_cache_muris);
}



// ==================== 余额操作函数 ====================

// 修改用户余额并记录日志
// type: 0=系统调整, 1=充值, 2=消费, 3=退款, 4=提现
// amount: 变动金额（正数增加，负数减少，单位：分）
// 返回: true=成功, false=失败
bool Member_ChangeBalance(int64 memberId, int type, int64 amount, str remark, str operator)
{
	if (!DB_BeginWrite()) return false;
	// 获取当前余额
	sqlite3_bind_int64(stmt_member_get, 1, memberId);
	if (sqlite3_step(stmt_member_get) != SQLITE_ROW) {
		sqlite3_reset(stmt_member_get);
		return DB_EndWrite(false);
	}
	int64 currentBalance = sqlite3_column_int64(stmt_member_get, 4); // balance 字段
	sqlite3_reset(stmt_member_get);
	
	// 计算新余额
	int64 newBalance = currentBalance + amount;
	if (newBalance < 0) {
		return DB_EndWrite(false); // 余额不足
	}
	
	int64 now = xrtNow();
	
	// 更新余额
	sqlite3_bind_int64(stmt_member_balance, 1, newBalance);
	sqlite3_bind_int64(stmt_member_balance, 2, now);
	sqlite3_bind_int64(stmt_member_balance, 3, memberId);
	if (!DB_Write(stmt_member_balance, true)) return DB_EndWrite(false);
	
	// 添加余额变动日志
	sqlite3_bind_int64(stmt_mbalance_add, 1, memberId);
	sqlite3_bind_int(stmt_mbalance_add, 2, type);
	sqlite3_bind_int64(stmt_mbalance_add, 3, amount);
	sqlite3_bind_int64(stmt_mbalance_add, 4, newBalance);
	sqlite3_bind_text(stmt_mbalance_add, 5, remark ? remark : (str)"", -1, NULL);
	sqlite3_bind_text(stmt_mbalance_add, 6, operator ? operator : (str)"", -1, NULL);
	sqlite3_bind_int64(stmt_mbalance_add, 7, now);
	return DB_EndWrite(DB_Write(stmt_mbalance_add, true));
}


