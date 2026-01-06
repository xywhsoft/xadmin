


void Auth_CompileSQL()
{
	// 预编译 SQL 语句 - uris 表
	int iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT uris.*, auth.name, COUNT(*) OVER() AS total_count FROM uris JOIN auth ON uris.authID = auth.id ORDER BY sort ASC, id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_uris_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_uris_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT uris.*, auth.name, COUNT(*) OVER() AS total_count FROM uris JOIN auth ON uris.authID = auth.id WHERE (uris.uri LIKE ?) OR (uris.desc LIKE ?) ORDER BY sort ASC, id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_uris_sel, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_uris_sel] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "INSERT INTO uris (authID, uri, desc, sort, createTime, updateTime) VALUES (1, ?, \"\", 0, ?, ?);", -1, SQL_PREPARE_DEFAULT, &stmt_uris_add, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_uris_add] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "UPDATE uris SET authID = ?, desc = ?, sort = ?, updateTime = ? WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_uris_put, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_uris_put] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "DELETE FROM uris WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_uris_del, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_uris_del] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT * FROM uris WHERE id=?;", -1, SQL_PREPARE_DEFAULT, &stmt_uris_get, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_uris_get] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	
	// 预编译 SQL 语句 - auth 表
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT auth.*, authGroup.name, COUNT(*) OVER() AS total_count FROM auth JOIN authGroup ON auth.groupID = authGroup.id WHERE auth.isDelete = 0 ORDER BY sort ASC, id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_auth_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_auth_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT auth.*, authGroup.name, COUNT(*) OVER() AS total_count FROM auth JOIN authGroup ON auth.groupID = authGroup.id WHERE (auth.isDelete = 0) AND ((auth.name LIKE ?) OR (auth.desc LIKE ?)) ORDER BY sort ASC, id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_auth_sel, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_auth_sel] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "INSERT INTO auth (groupID, name, desc, sort, createTime, updateTime) VALUES (?, ?, ?, ?, ?, ?);", -1, SQL_PREPARE_DEFAULT, &stmt_auth_add, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_auth_add] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "UPDATE auth SET groupID = ?, name = ?, desc = ?, sort = ?, updateTime = ? WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_auth_put, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_auth_put] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "UPDATE auth SET isDelete = 1 WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_auth_del, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_auth_del] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT * FROM auth WHERE id=?;", -1, SQL_PREPARE_DEFAULT, &stmt_auth_get, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_auth_get] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT COUNT(*) FROM uris WHERE authID = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_auth_sum, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_auth_sum] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "UPDATE uris SET authID = 1 WHERE authID = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_auth_mov, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_auth_mov] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	
	// 预编译 SQL 语句 - authGroup 表
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT authGroup.*, COUNT(auth.id) AS authCount, COUNT(*) OVER() AS total_count FROM authGroup LEFT JOIN auth ON auth.groupID = authGroup.id AND auth.isDelete = 0 WHERE authGroup.isDelete = 0 GROUP BY authGroup.id ORDER BY authGroup.sort ASC, authGroup.id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_group_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_group_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT authGroup.*, COUNT(auth.id) AS authCount, COUNT(*) OVER() AS total_count FROM authGroup LEFT JOIN auth ON auth.groupID = authGroup.id AND auth.isDelete = 0 WHERE (authGroup.isDelete = 0) AND ((authGroup.name LIKE ?) OR (authGroup.desc LIKE ?)) GROUP BY authGroup.id ORDER BY authGroup.sort ASC, authGroup.id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_group_sel, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_group_sel] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "INSERT INTO authGroup (name, desc, sort, createTime, updateTime) VALUES (?, ?, ?, ?, ?);", -1, SQL_PREPARE_DEFAULT, &stmt_group_add, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_group_add] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "UPDATE authGroup SET name = ?, desc = ?, sort = ?, updateTime = ? WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_group_put, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_group_put] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "UPDATE authGroup SET isDelete = 1 WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_group_del, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_group_del] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT * FROM authGroup WHERE id=?;", -1, SQL_PREPARE_DEFAULT, &stmt_group_get, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_group_get] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT COUNT(*) FROM auth WHERE groupID = ? AND isDelete = 0;", -1, SQL_PREPARE_DEFAULT, &stmt_group_sum, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_group_sum] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "UPDATE auth SET groupID = 1 WHERE (groupID = ?) AND (isDelete = 0);", -1, SQL_PREPARE_DEFAULT, &stmt_group_mov, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_group_mov] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	
	// 预编译 SQL 语句 - role 表
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT r.*, json_array_length(r.authList) AS authCount, (SELECT COUNT(*) FROM user WHERE role = r.id AND isDelete = 0) AS userCount, COUNT(*) OVER() AS total_count FROM role r WHERE r.isDelete = 0 ORDER BY r.id ASC LIMIT ? OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_role_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_role_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT r.*, json_array_length(r.authList) AS authCount, (SELECT COUNT(*) FROM user WHERE role = r.id AND isDelete = 0) AS userCount, COUNT(*) OVER() AS total_count FROM role r WHERE (r.isDelete = 0) AND ((r.name LIKE ?) OR (r.desc LIKE ?)) ORDER BY r.id ASC LIMIT ? OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_role_sel, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_role_sel] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT * FROM role WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_role_get, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_role_get] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "INSERT INTO role (name, desc, authList, authLevel, createTime, updateTime, isDelete) VALUES (?, ?, ?, 0, ?, ?, 0);", -1, SQL_PREPARE_DEFAULT, &stmt_role_add, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_role_add] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "UPDATE role SET name = ?, desc = ?, authList = ?, authLevel = ?, updateTime = ? WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_role_put, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_role_put] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "UPDATE role SET isDelete = 1 WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_role_del, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_role_del] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT COUNT(*) FROM user WHERE role = ? AND isDelete = 0;", -1, SQL_PREPARE_DEFAULT, &stmt_role_sum, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_role_sum] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	
	// 预编译 SQL 语句 - user 表
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT u.*, role.name AS roleName, COUNT(*) OVER() AS total_count FROM user u LEFT JOIN role ON u.role = role.id WHERE u.isDelete = 0 ORDER BY u.id ASC LIMIT ? OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_user_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_user_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT u.*, role.name AS roleName, COUNT(*) OVER() AS total_count FROM user u LEFT JOIN role ON u.role = role.id WHERE (u.isDelete = 0) AND (u.user LIKE ?) ORDER BY u.id ASC LIMIT ? OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_user_sel, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_user_sel] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT u.*, role.name AS roleName FROM user u LEFT JOIN role ON u.role = role.id WHERE u.id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_user_get, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_user_get] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "INSERT INTO user (user, salt, pwd, role, roleLevel, createTime, updateTime, isDelete) VALUES (?, ?, ?, ?, ?, ?, ?, 0);", -1, SQL_PREPARE_DEFAULT, &stmt_user_add, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_user_add] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "UPDATE user SET role = ?, roleLevel = ?, updateTime = ? WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_user_put, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_user_put] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "UPDATE user SET isDelete = 1 WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_user_del, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_user_del] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT id FROM user WHERE (user = ?) AND (id != ?) AND (isDelete = 0);", -1, SQL_PREPARE_DEFAULT, &stmt_user_chk, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_user_chk] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "UPDATE user SET salt = ?, pwd = ?, updateTime = ? WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_user_pwd, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_user_pwd] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	
	// 预编译 SQL 语句 - 登录相关（根据用户名查询用户信息和 salt）
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT * FROM user WHERE (user = ?) AND (isDelete = 0);", -1, SQL_PREPARE_DEFAULT, &stmt_login_get, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_login_get] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	
	// 预编译 SQL 语句 - 缓存相关
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT id, groupID, name FROM auth WHERE isDelete = 0 ORDER BY sort ASC, id ASC;", -1, SQL_PREPARE_DEFAULT, &stmt_cache_auth, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_cache_auth] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT id, name FROM authGroup WHERE isDelete = 0 ORDER BY sort ASC, id ASC;", -1, SQL_PREPARE_DEFAULT, &stmt_cache_group, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_cache_group] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT id, name, authList, authLevel FROM role WHERE isDelete = 0;", -1, SQL_PREPARE_DEFAULT, &stmt_cache_role, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_cache_role] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT id, authID, uri FROM uris;", -1, SQL_PREPARE_DEFAULT, &stmt_cache_uris, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_cache_uris] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	
}



// 缓存权限分组列表 - ComboBox 使用
void ReloadCache_Auth_Auth()
{
	xvalue arrRet = xvoCreateArray();
	
	// 使用预编译语句查询数据
	while ( sqlite3_step(stmt_cache_auth) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int64 id = sqlite3_column_int64(stmt_cache_auth, 0);
		int64 groupId = sqlite3_column_int64(stmt_cache_auth, 1);
		xvoTableSetInt(tblRow, "id", 2, id);
		xvoTableSetInt(tblRow, "groupId", 7, groupId);
		xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt_cache_auth, 2), 0, FALSE);
		xvoArrayAppendValue(arrRet, tblRow, TRUE);
	}
	sqlite3_reset(stmt_cache_auth);
	
	// 替换全局缓存（这样写是为了多线程同步无冲突）
	if ( G_CACHE_Auth ) {
		xvalue oldCache = G_CACHE_Auth;
		G_CACHE_Auth = arrRet;
		xvoUnref(oldCache);
	} else {
		G_CACHE_Auth = arrRet;
	}
}



// 缓存权限分类列表 - ComboBox 和 角色权限分配使用（依赖 G_CACHE_Auth 的数据，必须在 ReloadCache_Auth_Auth 之后调用）
void ReloadCache_Auth_Group()
{
	// 查询所有权限分类（按排序值）
	xvalue arrRet = xvoCreateArray();
	
	// 创建 groupId -> 数组下标 的映射表
	xvalue listIndex = xvoCreateList();
	int iIndex = 0;
	
	// 使用预编译语句查询数据
	while ( sqlite3_step(stmt_cache_group) == SQLITE_ROW ) {
		int64 groupId = sqlite3_column_int64(stmt_cache_group, 0);
		str groupName = (str)sqlite3_column_text(stmt_cache_group, 1);
		
		// 创建分类项
		xvalue tblGroup = xvoCreateTable();
		xvoTableSetInt(tblGroup, "id", 2, groupId);
		xvoTableSetText(tblGroup, "name", 4, groupName, 0, FALSE);
		
		// 创建空的 auths 数组
		xvalue arrAuths = xvoCreateArray();
		xvoTableSetValue(tblGroup, "auths", 5, arrAuths, TRUE);
		
		// 添加到缓存数组
		xvoArrayAppendValue(arrRet, tblGroup, TRUE);
		
		// 记录 groupId -> 数组下标 的映射
		iIndex++;
		xvoListSetInt(listIndex, groupId, iIndex);
	}
	sqlite3_reset(stmt_cache_group);
	
	// 遍历 G_CACHE_Auth，将权限分组添加到对应分类的 auths 中
	for ( int i = 1; i <= G_CACHE_Auth->vArray->Count; i++ ) {
		xvalue pAuth = xrtPtrArrayGet_Inline(G_CACHE_Auth->vArray, i);
		int64 authId = xvoTableGetInt(pAuth, "id", 2);
		int64 groupId = xvoTableGetInt(pAuth, "groupId", 7);
		str authName = xvoTableGetText(pAuth, "name", 4);
		
		// 通过映射表获取分类在数组中的下标
		int64 idx = xvoListGetInt(listIndex, groupId);
		if ( idx > 0 && idx <= arrRet->vArray->Count ) {
			xvalue pGroup = xrtPtrArrayGet_Inline(arrRet->vArray, idx);
			xvalue arrAuths = xvoTableGetValue(pGroup, "auths", 5);
			
			// 创建权限项并添加到 auths 数组
			xvalue tblAuth = xvoCreateTable();
			xvoTableSetInt(tblAuth, "id", 2, authId);
			xvoTableSetText(tblAuth, "name", 4, authName, 0, FALSE);
			xvoArrayAppendValue(arrAuths, tblAuth, TRUE);
		}
	}
	
	// 释放映射表
	xvoUnref(listIndex);
	
	// 替换全局缓存（这样写是为了多线程同步无冲突）
	if ( G_CACHE_Group ) {
		xvalue oldCache = G_CACHE_Group;
		G_CACHE_Group = arrRet;
		xvoUnref(oldCache);
	} else {
		G_CACHE_Group = arrRet;
	}
}



// 重新加载全局缓存 - 两份缓存：G_CACHE_Role 为 ComboBox 列表缓存、G_CACHE_RoleAuth 为权限映射表缓存
bool AuthRouteCategorize(Dict_Key* pKey, RouteInfo* pInfo, ptr param)
{
	struct {
		xvalue listAuth;
		xvalue tblURI;
	} *pAuthInfo = param;
	if ( pInfo->bAuth && (pInfo->AuthID > 0) ) {
		bool bPass = xvoListGetBool(pAuthInfo->listAuth, pInfo->AuthID);
		if ( bPass ) {
			xvoTableSetBool(pAuthInfo->tblURI, pKey->Key, pKey->KeyLen, TRUE);
		}
	}
	return FALSE;
}
void Auth_ReloadCache()
{
	// 使用预编译语句查询数据
	xvalue arrRet = xvoCreateArray();
	xvalue lstRet = xvoCreateList();
	
	while ( sqlite3_step(stmt_cache_role) == SQLITE_ROW ) {
		// 添加到列表缓存
		xvalue tblRow = xvoCreateTable();
		int64 id = sqlite3_column_int64(stmt_cache_role, 0);
		str name = (str)sqlite3_column_text(stmt_cache_role, 1);
		int64 authLevel = sqlite3_column_int64(stmt_cache_role, 3);
		xvoTableSetInt(tblRow, "id", 2, id);
		xvoTableSetText(tblRow, "name", 4, name, 0, FALSE);
		xvoTableSetInt(tblRow, "authLevel", 9, authLevel);
		xvoArrayAppendValue(arrRet, tblRow, TRUE);
		// 解析权限分组列表 - 转换为 list 方便按ID索引
		xvalue listAuth = xvoCreateList();
		str sAuthList = (str)sqlite3_column_text(stmt_cache_role, 2);
		if ( sAuthList && (strlen(sAuthList) > 2) ) {
			xvalue arrAuth = xrtParseJSON(sAuthList, 0);
			if ( arrAuth && (arrAuth->Type == XVO_DT_ARRAY) && (arrAuth->vArray->Count > 0) ) {
				for ( int i = 0; i < arrAuth->vArray->Count; i++ ) {
					int64 authID = xvoArrayGetInt(arrAuth, i);
					if ( authID > 0 ) {
						xvoListSetBool(listAuth, authID, TRUE);
					}
				}
			}
			xvoUnref(arrAuth);
		}
		// 构建对应角色的 URI 权限字典
		xvalue tblURI = xvoCreateTable();
		struct {
			xvalue listAuth;
			xvalue tblURI;
		} dictWalkInfo = { listAuth, tblURI };
		xrtDictWalk(G_StaticRouteTableHTTP, (ptr)AuthRouteCategorize, &dictWalkInfo);
		xvoUnref(listAuth);
		// 将整理好的权限字典添加到缓存表
		xvoListSetValue(lstRet, id, tblURI, TRUE);
	}
	sqlite3_reset(stmt_cache_role);
	
	// 替换全局缓存 - 角色列表（这样写是为了多线程同步无冲突）
	if ( G_CACHE_Role ) {
		xvalue oldCache = G_CACHE_Role;
		G_CACHE_Role = arrRet;
		xvoUnref(oldCache);
	} else {
		G_CACHE_Role = arrRet;
	}
	
	// 替换全局缓存 - 角色权限表（这样写是为了多线程同步无冲突）
	if ( G_CACHE_RoleAuth ) {
		xvalue oldCache = G_CACHE_RoleAuth;
		G_CACHE_RoleAuth = lstRet;
		xvoUnref(oldCache);
	} else {
		G_CACHE_RoleAuth = lstRet;
	}
	//xvoPrintValue(lstRet, 0, 0, 0, NULL);
}



// 更新 uris 表 ( 添加未收录的 URI，删除已失效的 URI )
bool AuthRouteCheckProc(Dict_Key* pKey, RouteInfo* pInfo, ptr param)
{
	if ( pInfo->bAuth && (pInfo->AuthID == 0) ) {
		printf("            new uris table item : %.*s\n", pKey->KeyLen, pKey->Key);
		sqlite3_bind_text(stmt_uris_add, 1, pKey->Key, pKey->KeyLen, SQLITE_STATIC);
		xtime tNow = xrtNow();
		sqlite3_bind_int64(stmt_uris_add, 2, tNow);
		sqlite3_bind_int64(stmt_uris_add, 3, tNow);
		sqlite3_step(stmt_uris_add);
		sqlite3_reset(stmt_uris_add);
		pInfo->AuthID = 1;
	}
	return FALSE;
}
void Auth_UpdateURIS()
{
	// 删除数据库中存在，但路由表中不存在的记录
	while ( sqlite3_step(stmt_cache_uris) == SQLITE_ROW ) {
		int64 id = sqlite3_column_int64(stmt_cache_uris, 0);
		int64 authID = sqlite3_column_int64(stmt_cache_uris, 1);
		str uri = (str)sqlite3_column_text(stmt_cache_uris, 2);
		size_t iSize = strlen(uri);
		RouteInfo* pInfo = xrtDictGet(G_StaticRouteTableHTTP, uri, iSize);
		if ( pInfo ) {
			pInfo->AuthID = authID;
		} else {
			printf("            remove uris table item : %.*s (%d)\n", iSize, uri, id);
			sqlite3_bind_int64(stmt_uris_del, 1, id);
			sqlite3_step(stmt_uris_del);
			sqlite3_reset(stmt_uris_del);
		}
	}
	sqlite3_reset(stmt_cache_uris);
	// 遍历路由表，将数据库中不存在的记录添加进去
	xrtDictWalk(G_StaticRouteTableHTTP, (ptr)AuthRouteCheckProc, NULL);
}





// 权限模块初始化
void Auth_Init()
{
	printf("        Auth_Init \n");
	
	// 预编译 SQL 语句
	Auth_CompileSQL();
	
	// 更新 uris 表 ( 添加未收录的 URI，删除已失效的 URI )
	Auth_UpdateURIS();
	
	// 重新加载全局缓存
	ReloadCache_Auth_Auth();
	ReloadCache_Auth_Group();
	Auth_ReloadCache();
}



// 卸载权限模块
void Auth_Unit()
{
	printf("        Auth_Unit \n");
	sqlite3_finalize(stmt_uris_all);
	sqlite3_finalize(stmt_uris_sel);
	sqlite3_finalize(stmt_uris_add);
	sqlite3_finalize(stmt_uris_del);
	sqlite3_finalize(stmt_uris_put);
	sqlite3_finalize(stmt_uris_get);
	sqlite3_finalize(stmt_auth_all);
	sqlite3_finalize(stmt_auth_sel);
	sqlite3_finalize(stmt_auth_add);
	sqlite3_finalize(stmt_auth_del);
	sqlite3_finalize(stmt_auth_put);
	sqlite3_finalize(stmt_auth_get);
	sqlite3_finalize(stmt_group_all);
	sqlite3_finalize(stmt_group_sel);
	sqlite3_finalize(stmt_group_add);
	sqlite3_finalize(stmt_group_del);
	sqlite3_finalize(stmt_group_put);
	sqlite3_finalize(stmt_group_get);
	sqlite3_finalize(stmt_group_sum);
	sqlite3_finalize(stmt_group_mov);
	sqlite3_finalize(stmt_role_all);
	sqlite3_finalize(stmt_role_sel);
	sqlite3_finalize(stmt_role_get);
	sqlite3_finalize(stmt_role_add);
	sqlite3_finalize(stmt_role_put);
	sqlite3_finalize(stmt_role_del);
	sqlite3_finalize(stmt_role_sum);
	sqlite3_finalize(stmt_user_all);
	sqlite3_finalize(stmt_user_sel);
	sqlite3_finalize(stmt_user_get);
	sqlite3_finalize(stmt_user_add);
	sqlite3_finalize(stmt_user_put);
	sqlite3_finalize(stmt_user_del);
	sqlite3_finalize(stmt_user_chk);
	sqlite3_finalize(stmt_user_pwd);
	sqlite3_finalize(stmt_login_get);
	
	// 释放缓存相关的预编译语句
	sqlite3_finalize(stmt_cache_auth);
	sqlite3_finalize(stmt_cache_group);
	sqlite3_finalize(stmt_cache_role);
	sqlite3_finalize(stmt_cache_uris);
	
	// 释放全局缓存表
	xvoUnref(G_CACHE_RoleAuth);
	xvoUnref(G_CACHE_Auth);
	xvoUnref(G_CACHE_Group);
	xvoUnref(G_CACHE_Role);
}


