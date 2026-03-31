


void Auth_CompileSQL()
{
	// 预编�?SQL 语句 - uris �?
	int iRet = sqlite3_prepare_v3(G_DB, "SELECT uris.*, auth.name AS authName, memberAuth.name AS memberAuthName, COUNT(*) OVER() AS total_count FROM uris LEFT JOIN auth ON uris.authID = auth.id LEFT JOIN memberAuth ON uris.authID = memberAuth.id ORDER BY sort ASC, id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_uris_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_uris_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT uris.*, auth.name AS authName, memberAuth.name AS memberAuthName, COUNT(*) OVER() AS total_count FROM uris LEFT JOIN auth ON uris.authID = auth.id LEFT JOIN memberAuth ON uris.authID = memberAuth.id WHERE (uris.uri LIKE ?) OR (uris.desc LIKE ?) ORDER BY sort ASC, id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_uris_sel, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_uris_sel] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "INSERT INTO uris (authID, uri, desc, sort, createTime, updateTime) VALUES (1, ?, \"\", 0, ?, ?);", -1, SQL_PREPARE_DEFAULT, &stmt_uris_add, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_uris_add] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE uris SET authID = ?, desc = ?, sort = ?, isBackend = ?, needAuth = ?, needLog = ?, keepActive = ?, updateTime = ? WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_uris_put, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_uris_put] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "DELETE FROM uris WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_uris_del, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_uris_del] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT * FROM uris WHERE id=?;", -1, SQL_PREPARE_DEFAULT, &stmt_uris_get, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_uris_get] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	
	// 预编�?SQL 语句 - auth �?
	iRet = sqlite3_prepare_v3(G_DB, "SELECT auth.*, authGroup.name, COUNT(*) OVER() AS total_count FROM auth JOIN authGroup ON auth.groupID = authGroup.id WHERE auth.isDelete = 0 ORDER BY sort ASC, id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_auth_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_auth_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT auth.*, authGroup.name, COUNT(*) OVER() AS total_count FROM auth JOIN authGroup ON auth.groupID = authGroup.id WHERE (auth.isDelete = 0) AND ((auth.name LIKE ?) OR (auth.desc LIKE ?)) ORDER BY sort ASC, id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_auth_sel, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_auth_sel] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "INSERT INTO auth (groupID, name, desc, sort, createTime, updateTime) VALUES (?, ?, ?, ?, ?, ?);", -1, SQL_PREPARE_DEFAULT, &stmt_auth_add, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_auth_add] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE auth SET groupID = ?, name = ?, desc = ?, sort = ?, updateTime = ? WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_auth_put, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_auth_put] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE auth SET isDelete = 1 WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_auth_del, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_auth_del] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT * FROM auth WHERE id=?;", -1, SQL_PREPARE_DEFAULT, &stmt_auth_get, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_auth_get] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM uris WHERE authID = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_auth_sum, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_auth_sum] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE uris SET authID = 1 WHERE authID = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_auth_mov, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_auth_mov] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	
	// 预编�?SQL 语句 - authGroup �?
	iRet = sqlite3_prepare_v3(G_DB, "SELECT authGroup.*, COUNT(auth.id) AS authCount, COUNT(*) OVER() AS total_count FROM authGroup LEFT JOIN auth ON auth.groupID = authGroup.id AND auth.isDelete = 0 WHERE authGroup.isDelete = 0 GROUP BY authGroup.id ORDER BY authGroup.sort ASC, authGroup.id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_group_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_group_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT authGroup.*, COUNT(auth.id) AS authCount, COUNT(*) OVER() AS total_count FROM authGroup LEFT JOIN auth ON auth.groupID = authGroup.id AND auth.isDelete = 0 WHERE (authGroup.isDelete = 0) AND ((authGroup.name LIKE ?) OR (authGroup.desc LIKE ?)) GROUP BY authGroup.id ORDER BY authGroup.sort ASC, authGroup.id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_group_sel, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_group_sel] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "INSERT INTO authGroup (name, desc, sort, createTime, updateTime) VALUES (?, ?, ?, ?, ?);", -1, SQL_PREPARE_DEFAULT, &stmt_group_add, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_group_add] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE authGroup SET name = ?, desc = ?, sort = ?, updateTime = ? WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_group_put, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_group_put] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE authGroup SET isDelete = 1 WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_group_del, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_group_del] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT * FROM authGroup WHERE id=?;", -1, SQL_PREPARE_DEFAULT, &stmt_group_get, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_group_get] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM auth WHERE groupID = ? AND isDelete = 0;", -1, SQL_PREPARE_DEFAULT, &stmt_group_sum, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_group_sum] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE auth SET groupID = 1 WHERE (groupID = ?) AND (isDelete = 0);", -1, SQL_PREPARE_DEFAULT, &stmt_group_mov, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_group_mov] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	
	// 预编�?SQL 语句 - role �?
	iRet = sqlite3_prepare_v3(G_DB, "SELECT r.*, json_array_length(r.authList) AS authCount, (SELECT COUNT(*) FROM user WHERE role = r.id AND isDelete = 0) AS userCount, COUNT(*) OVER() AS total_count FROM role r WHERE r.isDelete = 0 ORDER BY r.id ASC LIMIT ? OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_role_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_role_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT r.*, json_array_length(r.authList) AS authCount, (SELECT COUNT(*) FROM user WHERE role = r.id AND isDelete = 0) AS userCount, COUNT(*) OVER() AS total_count FROM role r WHERE (r.isDelete = 0) AND ((r.name LIKE ?) OR (r.desc LIKE ?)) ORDER BY r.id ASC LIMIT ? OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_role_sel, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_role_sel] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT * FROM role WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_role_get, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_role_get] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "INSERT INTO role (name, desc, authList, authLevel, createTime, updateTime, isDelete) VALUES (?, ?, ?, 0, ?, ?, 0);", -1, SQL_PREPARE_DEFAULT, &stmt_role_add, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_role_add] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE role SET name = ?, desc = ?, authList = ?, authLevel = ?, updateTime = ? WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_role_put, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_role_put] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE role SET isDelete = 1 WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_role_del, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_role_del] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM user WHERE role = ? AND isDelete = 0;", -1, SQL_PREPARE_DEFAULT, &stmt_role_sum, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_role_sum] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	
	// 预编�?SQL 语句 - user �?
	iRet = sqlite3_prepare_v3(G_DB, "SELECT u.*, role.name AS roleName, COUNT(*) OVER() AS total_count FROM user u LEFT JOIN role ON u.role = role.id WHERE u.isDelete = 0 ORDER BY u.id ASC LIMIT ? OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_user_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_user_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT u.*, role.name AS roleName, COUNT(*) OVER() AS total_count FROM user u LEFT JOIN role ON u.role = role.id WHERE (u.isDelete = 0) AND (u.user LIKE ?) ORDER BY u.id ASC LIMIT ? OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_user_sel, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_user_sel] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT u.*, role.name AS roleName FROM user u LEFT JOIN role ON u.role = role.id WHERE u.id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_user_get, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_user_get] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "INSERT INTO user (user, salt, pwd, role, authLevel, createTime, updateTime, isDelete) VALUES (?, ?, ?, ?, ?, ?, ?, 0);", -1, SQL_PREPARE_DEFAULT, &stmt_user_add, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_user_add] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE user SET role = ?, authLevel = ?, updateTime = ? WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_user_put, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_user_put] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE user SET isDelete = 1 WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_user_del, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_user_del] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT id FROM user WHERE (user = ?) AND (id != ?) AND (isDelete = 0);", -1, SQL_PREPARE_DEFAULT, &stmt_user_chk, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_user_chk] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE user SET salt = ?, pwd = ?, updateTime = ? WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_user_pwd, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_user_pwd] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	
	// 预编�?SQL 语句 - 登录相关（根据用户名查询用户信息�?salt�?
	iRet = sqlite3_prepare_v3(G_DB, "SELECT * FROM user WHERE (user = ?) AND (isDelete = 0);", -1, SQL_PREPARE_DEFAULT, &stmt_login_get, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_login_get] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	
	// 预编�?SQL 语句 - 缓存相关
	iRet = sqlite3_prepare_v3(G_DB, "SELECT id, groupID, name FROM auth WHERE isDelete = 0 ORDER BY sort ASC, id ASC;", -1, SQL_PREPARE_DEFAULT, &stmt_cache_auth, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_cache_auth] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT id, name FROM authGroup WHERE isDelete = 0 ORDER BY sort ASC, id ASC;", -1, SQL_PREPARE_DEFAULT, &stmt_cache_group, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_cache_group] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT id, name, authList, authLevel FROM role WHERE isDelete = 0;", -1, SQL_PREPARE_DEFAULT, &stmt_cache_role, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_cache_role] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT id, authID, uri, isBackend, needAuth, needLog, keepActive FROM uris;", -1, SQL_PREPARE_DEFAULT, &stmt_cache_uris, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_cache_uris] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	
}



// 缓存权限分组列表 - ComboBox 使用



static bool XAdminValuePublishShared(xvalue pVal);

static bool XAdminPublishListValueProc(int64 iKey, ptr pVal, ptr pArg)
{
	xvalue objList = (xvalue)pArg;
	xvalue objVal = xvoListGetValue(objList, iKey);
	XAdminValuePublishShared(objVal);
	return FALSE;
}

static bool XAdminPublishTableValueProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	xvalue objTable = (xvalue)pArg;
	xvalue objVal = xvoTableGetValue(objTable, pKey->Key, pKey->KeyLen);
	XAdminValuePublishShared(objVal);
	return FALSE;
}

static bool XAdminValuePublishShared(xvalue pVal)
{
	if ( pVal == NULL || pVal->IsStatic ) {
		return TRUE;
	}

	switch ( pVal->Type ) {
		case XVO_DT_ARRAY:
			for ( int i = 0; i < pVal->vArray->Count; i++ ) {
				XAdminValuePublishShared(xvoArrayGetValue(pVal, i));
			}
			xrtOwnerActivateShared(&pVal->vArray->Owner);
			break;
		case XVO_DT_LIST:
			xrtListWalk(pVal->vList, (ptr)XAdminPublishListValueProc, pVal);
			xrtOwnerActivateShared(&pVal->vList->AVLT.Owner);
			xrtOwnerActivateShared(&pVal->vList->Owner);
			break;
		case XVO_DT_TABLE:
			xrtDictWalk(pVal->vTable, (ptr)XAdminPublishTableValueProc, pVal);
			xrtOwnerActivateShared(&pVal->vTable->AVLT.Owner);
			xrtOwnerActivateShared(&pVal->vTable->Owner);
			break;
		case XVO_DT_COLL:
			xrtOwnerActivateShared(&pVal->vColl->Owner);
			break;
		default:
			break;
	}

	xvoSetShared_Inline(pVal);
	return TRUE;
}

void ReloadCache_Auth_Auth()
{
	xvalue arrRet = xvoCreateArray();
	
	// 使用预编译语句查询数�?
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
	XAdminValuePublishShared(arrRet);
	
	// 替换全局缓存（这样写是为了多线程同步无冲突）
	if ( G_CACHE_Auth ) {
		xvalue oldCache = G_CACHE_Auth;
		G_CACHE_Auth = arrRet;
		xvoUnref(oldCache);
	} else {
		G_CACHE_Auth = arrRet;
	}
}



// 缓存权限分类列表 - ComboBox �?角色权限分配使用（依�?G_CACHE_Auth 的数据，必须�?ReloadCache_Auth_Auth 之后调用�?
void ReloadCache_Auth_Group()
{
	// 查询所有权限分类（按排序值）
	xvalue arrRet = xvoCreateArray();
	
	// 创建 groupId -> 数组下标 的映射表
	xvalue listIndex = xvoCreateList();
	int iIndex = 0;
	
	// 使用预编译语句查询数�?
	while ( sqlite3_step(stmt_cache_group) == SQLITE_ROW ) {
		int64 groupId = sqlite3_column_int64(stmt_cache_group, 0);
		str groupName = (str)sqlite3_column_text(stmt_cache_group, 1);
		
		// 创建分类�?
		xvalue tblGroup = xvoCreateTable();
		xvoTableSetInt(tblGroup, "id", 2, groupId);
		xvoTableSetText(tblGroup, "name", 4, groupName, 0, FALSE);
		
		// 创建空的 auths 数组
		xvalue arrAuths = xvoCreateArray();
		xvoTableSetValue(tblGroup, "auths", 5, arrAuths, TRUE);
		
		// 添加到缓存数�?
		xvoArrayAppendValue(arrRet, tblGroup, TRUE);
		
		// 记录 groupId -> 数组下标 的映�?
		iIndex++;
		xvoListSetInt(listIndex, groupId, iIndex);
	}
	sqlite3_reset(stmt_cache_group);
	
	// 遍历 G_CACHE_Auth，将权限分组添加到对应分类的 auths �?
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
			
			// 创建权限项并添加�?auths 数组
			xvalue tblAuth = xvoCreateTable();
			xvoTableSetInt(tblAuth, "id", 2, authId);
			xvoTableSetText(tblAuth, "name", 4, authName, 0, FALSE);
			xvoArrayAppendValue(arrAuths, tblAuth, TRUE);
		}
	}
	XAdminValuePublishShared(arrRet);
	
	// 释放映射�?
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



// 重新加载全局缓存 - 两份缓存：G_CACHE_Role �?ComboBox 列表缓存、G_CACHE_RoleAuth 为权限映射表缓存
static int XAdminIDCacheFormatKey(int64 id, char sKey[32])
{
	int iLen;

	if ( sKey == NULL || id <= 0 ) {
		return 0;
	}

	sKey[0] = 'i';
	sKey[1] = 'd';
	sKey[2] = ':';
	iLen = xrtI64ToStr(id, sKey + 3);
	if ( iLen <= 0 || iLen >= 29 ) {
		return 0;
	}
	sKey[iLen + 3] = '\0';

	return iLen + 3;
}



static xvalue XAdminIndexedCacheGetValue(xvalue objIndexCache, xvalue objValueCache, int64 id)
{
	int64 iIndex;

	if ( objIndexCache == NULL || objValueCache == NULL || id <= 0 ) {
		return NULL;
	}

	if ( objIndexCache->Type != XVO_DT_LIST || objValueCache->Type != XVO_DT_ARRAY ) {
		return NULL;
	}

	iIndex = xvoListGetInt(objIndexCache, id);
	if ( iIndex <= 0 || iIndex > objValueCache->vArray->Count ) {
		return NULL;
	}

	return xvoArrayGetValue(objValueCache, (uint32)(iIndex - 1));
}



static xvalue XAdminIDCacheGetValue(xvalue objCache, int64 id)
{
	char sKey[32];
	int iKeyLen;

	if ( objCache == NULL || id <= 0 ) {
		return NULL;
	}

	if ( objCache->Type == XVO_DT_ARRAY ) {
		for ( int i = 0; i < objCache->vArray->Count; i++ ) {
			xvalue tblItem = xvoArrayGetValue(objCache, i);
			if ( tblItem && (tblItem->Type == XVO_DT_TABLE) ) {
				int64 iItemID = xvoTableGetInt(tblItem, "id", 2);
				if ( iItemID <= 0 ) {
					iItemID = xvoTableGetInt(tblItem, "__id__", 6);
				}
				if ( iItemID == id ) {
					return tblItem;
				}
			}
		}
		if ( objCache == G_CACHE_RoleAuth ) {
			return XAdminIndexedCacheGetValue(G_CACHE_RoleAuthIndex, G_CACHE_RoleAuth, id);
		}
		if ( objCache == G_CACHE_MemberGroupAuth ) {
			return XAdminIndexedCacheGetValue(G_CACHE_MemberGroupAuthIndex, G_CACHE_MemberGroupAuth, id);
		}
		return NULL;
	}

	if ( objCache->Type == XVO_DT_TABLE ) {
		iKeyLen = XAdminIDCacheFormatKey(id, sKey);
		if ( iKeyLen <= 0 ) {
			return NULL;
		}
		return xvoTableGetValue(objCache, sKey, iKeyLen);
	}

	if ( objCache->Type == XVO_DT_LIST ) {
		return xvoListGetValue(objCache, id);
	}

	return NULL;
}



static bool XAdminIDCacheGetBool(xvalue objCache, int64 id)
{
	return xvoGetBool(XAdminIDCacheGetValue(objCache, id));
}



static int64 XAdminIDCacheGetInt(xvalue objCache, int64 id)
{
	return xvoGetInt(XAdminIDCacheGetValue(objCache, id));
}



static bool XAdminIDArrayContainsInt(xvalue objIDs, int64 id)
{
	if ( objIDs == NULL || id <= 0 ) {
		return FALSE;
	}

	if ( objIDs->Type == XVO_DT_ARRAY ) {
		for ( int i = 0; i < objIDs->vArray->Count; i++ ) {
			if ( xvoArrayGetInt(objIDs, i) == id ) {
				return TRUE;
			}
		}
		return FALSE;
	}

	return XAdminIDCacheGetBool(objIDs, id);
}



static bool Auth_DBRoleGetAccess(int64 iRoleID, int64 iAuthID, int64* pAuthLevel)
{
	bool bAllowed = FALSE;
	str sAuthList = NULL;
	xvalue arrAuth = NULL;

	if ( pAuthLevel ) {
		*pAuthLevel = -1;
	}
	if ( iRoleID <= 0 ) {
		return FALSE;
	}

	sqlite3_bind_int64(stmt_role_get, 1, iRoleID);
	if ( sqlite3_step(stmt_role_get) == SQLITE_ROW ) {
		if ( sqlite3_column_int64(stmt_role_get, 7) == 0 ) {
			if ( pAuthLevel ) {
				*pAuthLevel = sqlite3_column_int64(stmt_role_get, 4);
			}
			if ( iAuthID <= 0 ) {
				bAllowed = TRUE;
			} else {
				sAuthList = (str)sqlite3_column_text(stmt_role_get, 3);
				if ( sAuthList && strlen(sAuthList) > 2 ) {
					arrAuth = xrtParseJSON(sAuthList, 0);
					if ( arrAuth && (arrAuth->Type == XVO_DT_ARRAY) ) {
						bAllowed = XAdminIDArrayContainsInt(arrAuth, iAuthID);
					}
					if ( arrAuth ) {
						xvoUnref(arrAuth);
					}
				}
			}
		}
	}
	sqlite3_reset(stmt_role_get);
	return bAllowed;
}



static bool XAdminIDCacheSetValue(xvalue objCache, int64 id, xvalue pVal, bool bColloc)
{
	char sKey[32];
	int iKeyLen;

	if ( objCache == NULL || id <= 0 ) {
		return FALSE;
	}

	if ( objCache->Type == XVO_DT_TABLE ) {
		iKeyLen = XAdminIDCacheFormatKey(id, sKey);
		if ( iKeyLen <= 0 ) {
			return FALSE;
		}
		return xvoTableSetValue(objCache, sKey, iKeyLen, pVal, bColloc);
	}

	if ( objCache->Type == XVO_DT_LIST ) {
		return xvoListSetValue(objCache, id, pVal, bColloc);
	}

	return FALSE;
}



static bool XAdminIDCacheSetBool(xvalue objCache, int64 id, bool bVal)
{
	return XAdminIDCacheSetValue(objCache, id, xvoCreateBool(bVal), TRUE);
}



static bool XAdminIDCacheSetInt(xvalue objCache, int64 id, int64 iVal)
{
	return XAdminIDCacheSetValue(objCache, id, xvoCreateInt(iVal), TRUE);
}



bool AuthRouteCategorize(Dict_Key* pKey, RouteInfo* pInfo, ptr param)
{
	struct {
		xvalue listAuth;
		xvalue tblURI;
	} *pAuthInfo = param;
	if ( pInfo->bAuth && (pInfo->AuthID > 0) ) {
		bool bPass = XAdminIDArrayContainsInt(pAuthInfo->listAuth, pInfo->AuthID);
		if ( bPass ) {
			xvoTableSetBool(pAuthInfo->tblURI, pKey->Key, pKey->KeyLen, TRUE);
		}
	}
	return FALSE;
}
void Auth_ReloadCache()
{
	// 使用预编译语句查询数�?
	xvalue arrRet = xvoCreateArray();
	xvalue lstRet = xvoCreateArray();
	xvalue idxRet = xvoCreateList();
	xvalue lvlRet = xvoCreateList();
	
	while ( sqlite3_step(stmt_cache_role) == SQLITE_ROW ) {
		// 添加到列表缓�?
		xvalue tblRow = xvoCreateTable();
		int64 id = sqlite3_column_int64(stmt_cache_role, 0);
		str name = (str)sqlite3_column_text(stmt_cache_role, 1);
		int64 authLevel = sqlite3_column_int64(stmt_cache_role, 3);
		xvoTableSetInt(tblRow, "id", 2, id);
		xvoTableSetText(tblRow, "name", 4, name, 0, FALSE);
		xvoTableSetInt(tblRow, "authLevel", 9, authLevel);
		xvoArrayAppendValue(arrRet, tblRow, TRUE);
		// 解析权限分组列表 - 转换�?list 方便按ID索引
		xvalue listAuth = NULL;
		str sAuthList = (str)sqlite3_column_text(stmt_cache_role, 2);
		if ( sAuthList && (strlen(sAuthList) > 2) ) {
			xvalue arrAuth = xrtParseJSON(sAuthList, 0);
			if ( arrAuth && (arrAuth->Type == XVO_DT_ARRAY) ) {
				listAuth = arrAuth;
			} else if ( arrAuth ) {
				xvoUnref(arrAuth);
			}
		}
		// 构建对应角色�?URI 权限字典
		xvalue tblURI = xvoCreateTable();
		struct {
			xvalue listAuth;
			xvalue tblURI;
		} dictWalkInfo = { listAuth, tblURI };
		xrtDictWalk(G_StaticRouteTableHTTP, (ptr)AuthRouteCategorize, &dictWalkInfo);
		if ( listAuth ) {
			xvoUnref(listAuth);
		}
		// 权限字典添加元数�?
		xvoTableSetInt(tblURI, "id", 2, id);
		xvoTableSetText(tblURI, "name", 4, name, 0, FALSE);
		xvoTableSetInt(tblURI, "authLevel", 9, authLevel);
		xvoTableSetInt(tblURI, "__id__", 6, id);
		xvoTableSetText(tblURI, "__name__", 8, name, 0, FALSE);
		xvoTableSetInt(tblURI, "__authLevel__", 13, authLevel);
		// 将整理好的权限字典添加到缓存�?
		xvoArrayAppendValue(lstRet, tblURI, TRUE);
		xvoListSetInt(idxRet, id, lstRet->vArray->Count);
		xvoListSetInt(lvlRet, id, authLevel);
	}
	sqlite3_reset(stmt_cache_role);
	XAdminValuePublishShared(arrRet);
	XAdminValuePublishShared(lstRet);
	XAdminValuePublishShared(idxRet);
	XAdminValuePublishShared(lvlRet);
	
	// 替换全局缓存 - 角色列表（这样写是为了多线程同步无冲突）
	if ( G_CACHE_Role ) {
		xvalue oldCache = G_CACHE_Role;
		G_CACHE_Role = arrRet;
		xvoUnref(oldCache);
	} else {
		G_CACHE_Role = arrRet;
	}
	
	// 替换全局缓存 - 角色权限表（这样写是为了多线程同步无冲突�?
	if ( G_CACHE_RoleAuth ) {
		xvalue oldCache = G_CACHE_RoleAuth;
		G_CACHE_RoleAuth = lstRet;
		xvoUnref(oldCache);
	} else {
		G_CACHE_RoleAuth = lstRet;
	}
	if ( G_CACHE_RoleAuthIndex ) {
		xvalue oldCache = G_CACHE_RoleAuthIndex;
		G_CACHE_RoleAuthIndex = idxRet;
		xvoUnref(oldCache);
	} else {
		G_CACHE_RoleAuthIndex = idxRet;
	}
	if ( G_CACHE_RoleAuthLevel ) {
		xvalue oldCache = G_CACHE_RoleAuthLevel;
		G_CACHE_RoleAuthLevel = lvlRet;
		xvoUnref(oldCache);
	} else {
		G_CACHE_RoleAuthLevel = lvlRet;
	}
	//xvoPrintValue(lstRet, 0, 0, 0, NULL);
}



// 更新 uris �?( 添加未收录的 URI，删除已失效�?URI，并从数据库加载配置到路由表 )
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
	// 从数据库加载 URI 配置到路由表，同时删除已失效的记�?
	while ( sqlite3_step(stmt_cache_uris) == SQLITE_ROW ) {
		int64 id = sqlite3_column_int64(stmt_cache_uris, 0);
		int64 authID = sqlite3_column_int64(stmt_cache_uris, 1);
		str uri = (str)sqlite3_column_text(stmt_cache_uris, 2);
		int isBackend = sqlite3_column_int(stmt_cache_uris, 3);
		int needAuth = sqlite3_column_int(stmt_cache_uris, 4);
		int needLog = sqlite3_column_int(stmt_cache_uris, 5);
		int keepActive = sqlite3_column_int(stmt_cache_uris, 6);
		size_t iSize = strlen(uri);
		RouteInfo* pInfo = xrtDictGet(G_StaticRouteTableHTTP, uri, iSize);
		if ( pInfo ) {
			// 从数据库加载配置到路由表
			pInfo->AuthID = authID;
			pInfo->bAdmin = isBackend ? TRUE : FALSE;
			pInfo->bAuth = needAuth ? TRUE : FALSE;
			pInfo->bPutLog = needLog ? TRUE : FALSE;
			pInfo->bActive = keepActive ? TRUE : FALSE;
		} else {
			printf("            remove uris table item : %.*s (%d)\n", iSize, uri, id);
			sqlite3_bind_int64(stmt_uris_del, 1, id);
			sqlite3_step(stmt_uris_del);
			sqlite3_reset(stmt_uris_del);
		}
	}
	sqlite3_reset(stmt_cache_uris);
	// 遍历路由表，将数据库中不存在的记录添加进�?
	xrtDictWalk(G_StaticRouteTableHTTP, (ptr)AuthRouteCheckProc, NULL);
}





// 权限模块初始�?
void Auth_Init()
{
	printf("        Auth_Init \n");
	
	// 预编�?SQL 语句
	Auth_CompileSQL();
	
	// 重新加载全局缓存
	ReloadCache_Auth_Auth();
	ReloadCache_Auth_Group();
	Auth_ReloadCache();
}

// 同步 URI 表（应在所有路由注册完成后调用�?
void Auth_SyncURIS()
{
	// 更新 uris �?( 添加未收录的 URI，删除已失效�?URI )
	Auth_UpdateURIS();
	
	// 重新加载缓存
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
	
	// 释放全局缓存�?
	xvoUnref(G_CACHE_RoleAuth);
	xvoUnref(G_CACHE_RoleAuthIndex);
	xvoUnref(G_CACHE_RoleAuthLevel);
	xvoUnref(G_CACHE_Auth);
	xvoUnref(G_CACHE_Group);
	xvoUnref(G_CACHE_Role);
}


