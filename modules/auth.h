



void Auth_CompileSQL()
{
	// 棰勭紪璇?SQL 璇彞 - uris 琛?
	int iRet = sqlite3_prepare_v3(G_DB, "SELECT uris.id, uris.authID, uris.uri, uris.desc, uris.isBackend, uris.needAuth, uris.needLog, uris.keepActive, uris.sort, uris.createTime, uris.updateTime, auth.name AS authName, memberAuth.name AS memberAuthName, uris.isPersistent, uris.namespace, uris.plugin_xid, uris.plugin_generation, uris.routeActive FROM uris LEFT JOIN auth ON uris.authID = auth.id LEFT JOIN memberAuth ON uris.authID = memberAuth.id ORDER BY uris.sort ASC, uris.id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_uris_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_uris_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT uris.id, uris.authID, uris.uri, uris.desc, uris.isBackend, uris.needAuth, uris.needLog, uris.keepActive, uris.sort, uris.createTime, uris.updateTime, auth.name AS authName, memberAuth.name AS memberAuthName, uris.isPersistent, uris.namespace, uris.plugin_xid, uris.plugin_generation, uris.routeActive FROM uris LEFT JOIN auth ON uris.authID = auth.id LEFT JOIN memberAuth ON uris.authID = memberAuth.id WHERE (uris.uri LIKE ?) OR (uris.desc LIKE ?) OR (uris.namespace LIKE ?) OR (uris.plugin_xid LIKE ?) ORDER BY uris.sort ASC, uris.id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_uris_sel, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_uris_sel] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "INSERT INTO uris (authID, uri, desc, sort, maskBody, createTime, updateTime, isPersistent, namespace) VALUES (1, ?, \"\", 0, ?, ?, ?, 0, 'auto');", -1, SQL_PREPARE_DEFAULT, &stmt_uris_add, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_uris_add] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE uris SET authID = ?, desc = ?, sort = ?, isBackend = ?, needAuth = ?, needLog = ?, keepActive = ?, isPersistent = ?, namespace = ?, updateTime = ? WHERE id = ?;", -1, SQL_PREPARE_DEFAULT, &stmt_uris_put, NULL);
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
	
	// 棰勭紪璇?SQL 璇彞 - auth 琛?
	iRet = sqlite3_prepare_v3(G_DB, "SELECT auth.id, auth.groupID, auth.name, auth.desc, auth.sort, auth.createTime, auth.updateTime, authGroup.name AS groupName FROM auth JOIN authGroup ON auth.groupID = authGroup.id WHERE auth.isDelete = 0 ORDER BY auth.sort ASC, auth.id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_auth_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_auth_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT auth.id, auth.groupID, auth.name, auth.desc, auth.sort, auth.createTime, auth.updateTime, authGroup.name AS groupName FROM auth JOIN authGroup ON auth.groupID = authGroup.id WHERE (auth.isDelete = 0) AND ((auth.name LIKE ?) OR (auth.desc LIKE ?)) ORDER BY auth.sort ASC, auth.id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_auth_sel, NULL);
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
	
	// 棰勭紪璇?SQL 璇彞 - authGroup 琛?
	iRet = sqlite3_prepare_v3(G_DB, "SELECT authGroup.*, COUNT(auth.id) AS authCount FROM authGroup LEFT JOIN auth ON auth.groupID = authGroup.id AND auth.isDelete = 0 WHERE authGroup.isDelete = 0 GROUP BY authGroup.id ORDER BY authGroup.sort ASC, authGroup.id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_group_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_group_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT authGroup.*, COUNT(auth.id) AS authCount FROM authGroup LEFT JOIN auth ON auth.groupID = authGroup.id AND auth.isDelete = 0 WHERE (authGroup.isDelete = 0) AND ((authGroup.name LIKE ?) OR (authGroup.desc LIKE ?)) GROUP BY authGroup.id ORDER BY authGroup.sort ASC, authGroup.id ASC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_group_sel, NULL);
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
	
	// 棰勭紪璇?SQL 璇彞 - role 琛?
	iRet = sqlite3_prepare_v3(G_DB, "SELECT r.*, json_array_length(r.authList) AS authCount, (SELECT COUNT(*) FROM user WHERE role = r.id AND isDelete = 0) AS userCount FROM role r WHERE r.isDelete = 0 ORDER BY r.id ASC LIMIT ? OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_role_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_role_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT r.*, json_array_length(r.authList) AS authCount, (SELECT COUNT(*) FROM user WHERE role = r.id AND isDelete = 0) AS userCount FROM role r WHERE (r.isDelete = 0) AND ((r.name LIKE ?) OR (r.desc LIKE ?)) ORDER BY r.id ASC LIMIT ? OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_role_sel, NULL);
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
	
	// 棰勭紪璇?SQL 璇彞 - user 琛?
	iRet = sqlite3_prepare_v3(G_DB, "SELECT u.id, u.user, u.role, u.authLevel, u.createTime, u.updateTime, role.name AS roleName FROM user u LEFT JOIN role ON u.role = role.id WHERE u.isDelete = 0 ORDER BY u.id ASC LIMIT ? OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_user_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_user_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB, "SELECT u.id, u.user, u.role, u.authLevel, u.createTime, u.updateTime, role.name AS roleName FROM user u LEFT JOIN role ON u.role = role.id WHERE (u.isDelete = 0) AND (u.user LIKE ?) ORDER BY u.id ASC LIMIT ? OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_user_sel, NULL);
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
	
	// 棰勭紪璇?SQL 璇彞 - 鐧诲綍鐩稿叧锛堟牴鎹敤鎴峰悕鏌ヨ鐢ㄦ埛淇℃伅鍜?salt锛?
	iRet = sqlite3_prepare_v3(G_DB, "SELECT * FROM user WHERE (user = ?) AND (isDelete = 0);", -1, SQL_PREPARE_DEFAULT, &stmt_login_get, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_login_get] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	
	// 棰勭紪璇?SQL 璇彞 - 缂撳瓨鐩稿叧
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
	iRet = sqlite3_prepare_v3(G_DB, "SELECT id, authID, uri, isBackend, needAuth, needLog, keepActive, isPersistent FROM uris;", -1, SQL_PREPARE_DEFAULT, &stmt_cache_uris, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Auth_Init [stmt_cache_uris] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB));
		exit(0);
	}
	
	// L1：列表计数独立语句（拆掉 COUNT(*) OVER() 宽表反模式）
	sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM uris", -1, SQL_PREPARE_DEFAULT, &stmt_uris_count_all, NULL);
	sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM uris WHERE (uri LIKE ? OR [desc] LIKE ? OR namespace LIKE ? OR plugin_xid LIKE ?)", -1, SQL_PREPARE_DEFAULT, &stmt_uris_count_sel, NULL);
	sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM auth WHERE isDelete = 0", -1, SQL_PREPARE_DEFAULT, &stmt_auth_count_all, NULL);
	sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM auth WHERE isDelete = 0 AND (name LIKE ? OR [desc] LIKE ?)", -1, SQL_PREPARE_DEFAULT, &stmt_auth_count_sel, NULL);
	sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM authGroup WHERE isDelete = 0", -1, SQL_PREPARE_DEFAULT, &stmt_group_count_all, NULL);
	sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM authGroup WHERE isDelete = 0 AND (name LIKE ? OR [desc] LIKE ?)", -1, SQL_PREPARE_DEFAULT, &stmt_group_count_sel, NULL);
	sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM role WHERE isDelete = 0", -1, SQL_PREPARE_DEFAULT, &stmt_role_count_all, NULL);
	sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM role WHERE isDelete = 0 AND (name LIKE ? OR [desc] LIKE ?)", -1, SQL_PREPARE_DEFAULT, &stmt_role_count_sel, NULL);
	sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM user WHERE isDelete = 0", -1, SQL_PREPARE_DEFAULT, &stmt_user_count_all, NULL);
	sqlite3_prepare_v3(G_DB, "SELECT COUNT(*) FROM user WHERE isDelete = 0 AND (user LIKE ?)", -1, SQL_PREPARE_DEFAULT, &stmt_user_count_sel, NULL);

}



// 缂撳瓨鏉冮檺鍒嗙粍鍒楄〃 - ComboBox 浣跨敤



static bool XAdminValuePublishShared(xvalue* pVal);

static bool XAdminPublishListValueProc(int64 iKey, ptr pVal, ptr pArg)
{
	xvalue* objList = (xvalue*)pArg;
	xvalue* objVal = ValueMapGet(objList, iKey);
	XAdminValuePublishShared(objVal);
	return false;
}

static bool XAdminPublishTableValueProc(xstrview key, xvalue* value, void* pArg)
{
	xvalue* objTable = (xvalue*)pArg;
	xvalue* objVal = xrtValueObjectGet(objTable, key);
	(void)value;
	XAdminValuePublishShared(objVal);
	return false;
}

static bool XAdminValuePublishShared(xvalue* pVal)
{
	/* 新版值没有旧 Owner 发布协议；缓存访问统一在请求锁中完成。 */
	(void)pVal;
	return true;
}

static xvalue* XAdminCreateSharedArrayValue()
{
	xvalue* pVal = ValueArray();
	if ( pVal ) {
		XAdminValuePublishShared(pVal);
	}
	return pVal;
}

static xvalue* XAdminCreateSharedListValue()
{
	xvalue* pVal = xrtValueIntMap();
	if ( pVal ) {
		XAdminValuePublishShared(pVal);
	}
	return pVal;
}

static xvalue* XAdminCreateSharedTableValue()
{
	xvalue* pVal = ValueObject();
	if ( pVal ) {
		XAdminValuePublishShared(pVal);
	}
	return pVal;
}

void ReloadCache_Auth_Auth()
{
	xvalue* arrRet = XAdminCreateSharedArrayValue();
	
	// 浣跨敤棰勭紪璇戣鍙ユ煡璇㈡暟鎹?
	while ( sqlite3_step(stmt_cache_auth) == SQLITE_ROW ) {
		xvalue* tblRow = XAdminCreateSharedTableValue();
		int64 id = sqlite3_column_int64(stmt_cache_auth, 0);
		int64 groupId = sqlite3_column_int64(stmt_cache_auth, 1);
		ValueSetInt(tblRow, "id", id);
		ValueSetInt(tblRow, "groupId", groupId);
		ValueSetText(tblRow, "name", (str)sqlite3_column_text(stmt_cache_auth, 2));
		ValueArrayOwn(arrRet, tblRow);
	}
	sqlite3_reset(stmt_cache_auth);
	XAdminValuePublishShared(arrRet);
	
	// 鏇挎崲鍏ㄥ眬缂撳瓨锛堣繖鏍峰啓鏄负浜嗗绾跨▼鍚屾鏃犲啿绐侊級
	if ( G_CACHE_Auth ) {
		xvalue* oldCache = G_CACHE_Auth;
		G_CACHE_Auth = arrRet;
		CacheRetire(oldCache);
	} else {
		G_CACHE_Auth = arrRet;
	}
}



// 缂撳瓨鏉冮檺鍒嗙被鍒楄〃 - ComboBox 鍜?瑙掕壊鏉冮檺鍒嗛厤浣跨敤锛堜緷璧?G_CACHE_Auth 鐨勬暟鎹紝蹇呴』鍦?ReloadCache_Auth_Auth 涔嬪悗璋冪敤锛?
void ReloadCache_Auth_Group()
{
	// 鏌ヨ鎵€鏈夋潈闄愬垎绫伙紙鎸夋帓搴忓€硷級
	xvalue* arrRet = XAdminCreateSharedArrayValue();
	
	// 鍒涘缓 groupId -> 鏁扮粍涓嬫爣 鐨勬槧灏勮〃
	xvalue* listIndex = xrtValueIntMap();
	int iIndex = 0;
	
	// 浣跨敤棰勭紪璇戣鍙ユ煡璇㈡暟鎹?
	while ( sqlite3_step(stmt_cache_group) == SQLITE_ROW ) {
		int64 groupId = sqlite3_column_int64(stmt_cache_group, 0);
		str groupName = (str)sqlite3_column_text(stmt_cache_group, 1);
		
		// 鍒涘缓鍒嗙被椤?
		xvalue* tblGroup = XAdminCreateSharedTableValue();
		ValueSetInt(tblGroup, "id", groupId);
		ValueSetText(tblGroup, "name", groupName);
		
		// 鍒涘缓绌虹殑 auths 鏁扮粍
		xvalue* arrAuths = XAdminCreateSharedArrayValue();
		ValueSetOwn(tblGroup, "auths", arrAuths);
		
		// 娣诲姞鍒扮紦瀛樻暟缁?
		ValueArrayOwn(arrRet, tblGroup);
		
		// 璁板綍 groupId -> 鏁扮粍涓嬫爣 鐨勬槧灏?
		iIndex++;
		ValueMapSetInt(listIndex, groupId, iIndex);
	}
	sqlite3_reset(stmt_cache_group);
	
	// 閬嶅巻 G_CACHE_Auth锛屽皢鏉冮檺鍒嗙粍娣诲姞鍒板搴斿垎绫荤殑 auths 涓?
	for ( int i = 1; i <= xrtValueCount(G_CACHE_Auth); i++ ) {
		xvalue* pAuth = xrtValueArrayGet(G_CACHE_Auth, (i) - 1);
		int64 authId = ValueInt(pAuth, "id");
		int64 groupId = ValueInt(pAuth, "groupId");
		str authName = ValueText(pAuth, "name");
		
		// 閫氳繃鏄犲皠琛ㄨ幏鍙栧垎绫诲湪鏁扮粍涓殑涓嬫爣
		int64 idx = ValueMapInt(listIndex, groupId);
		if ( idx > 0 && idx <= xrtValueCount(arrRet) ) {
			xvalue* pGroup = xrtValueArrayGet(arrRet, (idx) - 1);
			xvalue* arrAuths = ValueGet(pGroup, "auths");
			
			// 鍒涘缓鏉冮檺椤瑰苟娣诲姞鍒?auths 鏁扮粍
			xvalue* tblAuth = XAdminCreateSharedTableValue();
			ValueSetInt(tblAuth, "id", authId);
			ValueSetText(tblAuth, "name", authName);
			ValueArrayOwn(arrAuths, tblAuth);
		}
		}
XAdminValuePublishShared(arrRet);
	
	// 閲婃斁鏄犲皠琛?
	xrtValueRelease(listIndex);
	
	// 鏇挎崲鍏ㄥ眬缂撳瓨锛堣繖鏍峰啓鏄负浜嗗绾跨▼鍚屾鏃犲啿绐侊級
	if ( G_CACHE_Group ) {
		xvalue* oldCache = G_CACHE_Group;
		G_CACHE_Group = arrRet;
		CacheRetire(oldCache);
	} else {
		G_CACHE_Group = arrRet;
	}
}



// 閲嶆柊鍔犺浇鍏ㄥ眬缂撳瓨 - 涓や唤缂撳瓨锛欸_CACHE_Role 涓?ComboBox 鍒楄〃缂撳瓨銆丟_CACHE_RoleAuth 涓烘潈闄愭槧灏勮〃缂撳瓨
static int XAdminIDCacheFormatKey(int64 id, char sKey[32])
{
	int iLen;

	if ( sKey == NULL || id <= 0 ) {
		return 0;
	}

	sKey[0] = 'i';
	sKey[1] = 'd';
	sKey[2] = ':';
	iLen = sprintf(sKey + 3, "%lld", (long long)id);
	if ( iLen <= 0 || iLen >= 29 ) {
		return 0;
	}
	sKey[iLen + 3] = '\0';

	return iLen + 3;
}



static xvalue* XAdminIndexedCacheGetValue(xvalue* objIndexCache, xvalue* objValueCache, int64 id)
{
	int64 iIndex;

	if ( objIndexCache == NULL || objValueCache == NULL || id <= 0 ) {
		return NULL;
	}

	if ( xrtValueType(objIndexCache) != XVALUE_INT_MAP || xrtValueType(objValueCache) != XVALUE_ARRAY ) {
		return NULL;
	}

	iIndex = ValueMapInt(objIndexCache, id);
	if ( iIndex <= 0 || iIndex > xrtValueCount(objValueCache) ) {
		return NULL;
	}

	return xrtValueArrayGet(objValueCache, (uint32)(iIndex - 1));
}



static xvalue* XAdminIDCacheGetValue(xvalue* objCache, int64 id)
{
	char sKey[32];
	int iKeyLen;

	if ( objCache == NULL || id <= 0 ) {
		return NULL;
	}

	if ( xrtValueType(objCache) == XVALUE_ARRAY ) {
		for ( int i = 0; i < xrtValueCount(objCache); i++ ) {
			xvalue* tblItem = xrtValueArrayGet(objCache, i);
			if ( tblItem && (xrtValueType(tblItem) == XVALUE_OBJECT) ) {
				int64 iItemID = ValueInt(tblItem, "id");
				if ( iItemID <= 0 ) {
					iItemID = ValueInt(tblItem, "__id__");
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

	if ( xrtValueType(objCache) == XVALUE_OBJECT ) {
		iKeyLen = XAdminIDCacheFormatKey(id, sKey);
		if ( iKeyLen <= 0 ) {
			return NULL;
		}
		return ValueGet(objCache, sKey);
	}

	if ( xrtValueType(objCache) == XVALUE_INT_MAP ) {
		return ValueMapGet(objCache, id);
	}

	return NULL;
}



static bool XAdminIDCacheGetBool(xvalue* objCache, int64 id)
{
	return ValueBoolOf(XAdminIDCacheGetValue(objCache, id));
}



static int64 XAdminIDCacheGetInt(xvalue* objCache, int64 id)
{
	return ValueIntOf(XAdminIDCacheGetValue(objCache, id));
}



static bool XAdminIDArrayContainsInt(xvalue* objIDs, int64 id)
{
	if ( objIDs == NULL || id <= 0 ) {
		return false;
	}

	if ( xrtValueType(objIDs) == XVALUE_ARRAY ) {
		for ( int i = 0; i < xrtValueCount(objIDs); i++ ) {
			if ( ValueArrayInt(objIDs, i) == id ) {
				return true;
			}
		}
		return false;
	}

	return XAdminIDCacheGetBool(objIDs, id);
}



static bool Auth_DBRoleGetAccess(int64 iRoleID, int64 iAuthID, int64* pAuthLevel)
{
	bool bAllowed = false;
	str sAuthList = NULL;
	xvalue* arrAuth = NULL;

	if ( pAuthLevel ) {
		*pAuthLevel = -1;
	}
	if ( iRoleID <= 0 ) {
		return false;
	}

	sqlite3_bind_int64(stmt_role_get, 1, iRoleID);
	if ( sqlite3_step(stmt_role_get) == SQLITE_ROW ) {
		if ( sqlite3_column_int64(stmt_role_get, 7) == 0 ) {
			if ( pAuthLevel ) {
				*pAuthLevel = sqlite3_column_int64(stmt_role_get, 4);
			}
			if ( iAuthID <= 0 ) {
				bAllowed = true;
			} else {
				sAuthList = (str)sqlite3_column_text(stmt_role_get, 3);
				if ( sAuthList && strlen(sAuthList) > 2 ) {
					arrAuth = JsonParseN(sAuthList, 0);
					if ( arrAuth && (xrtValueType(arrAuth) == XVALUE_ARRAY) ) {
						bAllowed = XAdminIDArrayContainsInt(arrAuth, iAuthID);
					}
					if ( arrAuth ) {
						xrtValueRelease(arrAuth);
					}
				}
			}
		}
	}
	sqlite3_reset(stmt_role_get);
	return bAllowed;
}



static bool XAdminIDCacheSetValue(xvalue* objCache, int64 id, xvalue* pVal, bool bColloc)
{
	char sKey[32];
	int iKeyLen;

	if ( objCache == NULL || id <= 0 ) {
		return false;
	}

	if ( xrtValueType(objCache) == XVALUE_OBJECT ) {
		iKeyLen = XAdminIDCacheFormatKey(id, sKey);
		if ( iKeyLen <= 0 ) {
			return false;
		}
		return bColloc ? xrtValueObjectSetNew(objCache, xrtStrViewN(sKey, (size_t)iKeyLen), pVal)
		              : xrtValueObjectSet(objCache, xrtStrViewN(sKey, (size_t)iKeyLen), pVal);
	}

	if ( xrtValueType(objCache) == XVALUE_INT_MAP ) {
		return bColloc ? xrtValueIntMapSetNew(objCache, id, pVal)
		              : xrtValueIntMapSet(objCache, id, pVal);
	}

	return false;
}



static bool XAdminIDCacheSetBool(xvalue* objCache, int64 id, bool bVal)
{
	return XAdminIDCacheSetValue(objCache, id, xrtValueBool(bVal), true);
}



static bool XAdminIDCacheSetInt(xvalue* objCache, int64 id, int64 iVal)
{
	return XAdminIDCacheSetValue(objCache, id, xrtValueInt(iVal), true);
}



/* F6：单次遍历收集 AuthID -> URI 清单，重建时每角色按 authList 点取，
 * 取代"每角色遍历全部路由"的 O(角色×路由) 写路径劣化（实测角色过千后
 * 每次权限写秒级劣化；语义不变，仍同步重建、写后即时生效）。 */
static bool AuthCollectURIProc(xbytesview key, RouteInfo* pInfo, void* pArg)
{
	xvalue* mapAuthURIs = (xvalue*)pArg;
	char sKey[32];
	int iKeyLen;
	xvalue* arrURIs;
	if ( !pInfo->bAuth || (pInfo->AuthID <= 0) ) {
		return false;
	}
	iKeyLen = XAdminIDCacheFormatKey((int64)pInfo->AuthID, sKey);
	if ( iKeyLen <= 0 ) {
		return false;
	}
	arrURIs = ValueGet(mapAuthURIs, sKey);
	if ( (arrURIs == NULL) || (xrtValueType(arrURIs) != XVALUE_ARRAY) ) {
		arrURIs = XAdminCreateSharedArrayValue();
		if ( (arrURIs == NULL) || !ValueSetOwn(mapAuthURIs, sKey, arrURIs) ) {
			if ( arrURIs != NULL ) xrtValueRelease(arrURIs);
			return false;
		}
	}
	ValueArrayOwn(arrURIs, xrtValueString(xrtStrViewN((const char*)key.Data, key.Size)));
	return false;
}
void Auth_ReloadCache()
{
	// 使用预编译语句查询数据
	xvalue* arrRet = XAdminCreateSharedArrayValue();
	xvalue* lstRet = XAdminCreateSharedArrayValue();
	xvalue* idxRet = XAdminCreateSharedListValue();
	xvalue* lvlRet = XAdminCreateSharedListValue();
	xvalue* mapAuthURIs = XAdminCreateSharedTableValue();

	MapWalk(G_StaticRouteTableHTTP, (MapWalkProc)AuthCollectURIProc, mapAuthURIs);

while ( sqlite3_step(stmt_cache_role) == SQLITE_ROW ) {
		// 娣诲姞鍒板垪琛ㄧ紦瀛?
		xvalue* tblRow = XAdminCreateSharedTableValue();
		int64 id = sqlite3_column_int64(stmt_cache_role, 0);
		str name = (str)sqlite3_column_text(stmt_cache_role, 1);
		int64 authLevel = sqlite3_column_int64(stmt_cache_role, 3);
		ValueSetInt(tblRow, "id", id);
		ValueSetText(tblRow, "name", name);
		ValueSetInt(tblRow, "authLevel", authLevel);
		ValueArrayOwn(arrRet, tblRow);
		// 瑙ｆ瀽鏉冮檺鍒嗙粍鍒楄〃 - 杞崲涓?list 鏂逛究鎸塈D绱㈠紩
		xvalue* listAuth = NULL;
		str sAuthList = (str)sqlite3_column_text(stmt_cache_role, 2);
		if ( sAuthList && (strlen(sAuthList) > 2) ) {
			xvalue* arrAuth = JsonParseN(sAuthList, 0);
			if ( arrAuth && (xrtValueType(arrAuth) == XVALUE_ARRAY) ) {
				listAuth = arrAuth;
			} else if ( arrAuth ) {
				xrtValueRelease(arrAuth);
			}
		}
		// 构建对应角色的 URI 权限字典（F6：按 authList 从预收集清单点取）
		xvalue* tblURI = XAdminCreateSharedTableValue();
		if ( listAuth && (xrtValueType(listAuth) == XVALUE_ARRAY) ) {
			for ( int iAuth = 0; iAuth < xrtValueCount(listAuth); iAuth++ ) {
				char sKey[32];
				int iKeyLen = XAdminIDCacheFormatKey(ValueArrayInt(listAuth, iAuth), sKey);
				xvalue* arrURIs = (iKeyLen > 0) ? ValueGet(mapAuthURIs, sKey) : NULL;
				if ( (arrURIs != NULL) && (xrtValueType(arrURIs) == XVALUE_ARRAY) ) {
					for ( int iURI = 0; iURI < xrtValueCount(arrURIs); iURI++ ) {
						str sURI = ValueArrayText(arrURIs, iURI);
						if ( sURI != NULL ) {
							ValueSetBool(tblURI, sURI, true);
						}
					}
				}
			}
		}
		if ( listAuth ) {
			xrtValueRelease(listAuth);
		}
		// 鏉冮檺瀛楀吀娣诲姞鍏冩暟鎹?
		ValueSetInt(tblURI, "id", id);
		ValueSetText(tblURI, "name", name);
		ValueSetInt(tblURI, "authLevel", authLevel);
		ValueSetInt(tblURI, "__id__", id);
		ValueSetText(tblURI, "__name__", name);
		ValueSetInt(tblURI, "__authLevel__", authLevel);
		// 灏嗘暣鐞嗗ソ鐨勬潈闄愬瓧鍏告坊鍔犲埌缂撳瓨琛?
		ValueArrayOwn(lstRet, tblURI);
		ValueMapSetInt(idxRet, id, xrtValueCount(lstRet));
		ValueMapSetInt(lvlRet, id, authLevel);
	}
	sqlite3_reset(stmt_cache_role);
	XAdminValuePublishShared(arrRet);
	XAdminValuePublishShared(lstRet);
	XAdminValuePublishShared(idxRet);
	XAdminValuePublishShared(lvlRet);
	xrtValueRelease(mapAuthURIs);
	
	// 鏇挎崲鍏ㄥ眬缂撳瓨 - 瑙掕壊鍒楄〃锛堣繖鏍峰啓鏄负浜嗗绾跨▼鍚屾鏃犲啿绐侊級
	if ( G_CACHE_Role ) {
		xvalue* oldCache = G_CACHE_Role;
		G_CACHE_Role = arrRet;
		CacheRetire(oldCache);
	} else {
		G_CACHE_Role = arrRet;
	}
	
	// 鏇挎崲鍏ㄥ眬缂撳瓨 - 瑙掕壊鏉冮檺琛紙杩欐牱鍐欐槸涓轰簡澶氱嚎绋嬪悓姝ユ棤鍐茬獊锛?
	if ( G_CACHE_RoleAuth ) {
		xvalue* oldCache = G_CACHE_RoleAuth;
		G_CACHE_RoleAuth = lstRet;
		CacheRetire(oldCache);
	} else {
		G_CACHE_RoleAuth = lstRet;
	}
	if ( G_CACHE_RoleAuthIndex ) {
		xvalue* oldCache = G_CACHE_RoleAuthIndex;
		G_CACHE_RoleAuthIndex = idxRet;
		CacheRetire(oldCache);
	} else {
		G_CACHE_RoleAuthIndex = idxRet;
	}
	if ( G_CACHE_RoleAuthLevel ) {
		xvalue* oldCache = G_CACHE_RoleAuthLevel;
		G_CACHE_RoleAuthLevel = lvlRet;
		CacheRetire(oldCache);
	} else {
		G_CACHE_RoleAuthLevel = lvlRet;
	}
}



// 鏇存柊 uris 琛?( 娣诲姞鏈敹褰曠殑 URI锛屽垹闄ゅ凡澶辨晥鐨?URI锛屽苟浠庢暟鎹簱鍔犺浇閰嶇疆鍒拌矾鐢辫〃 )
bool AuthRouteCheckProc(xbytesview key, RouteInfo* pInfo, void* param)
{
	if ( pInfo->bAuth && (pInfo->AuthID == 0) ) {
		printf("            new uris table item : %.*s\n", (int)key.Size, (const char*)key.Data);
		sqlite3_bind_text(stmt_uris_add, 1, (const char*)key.Data, (int)key.Size, SQLITE_STATIC);
		/* F2：新路由把注册参数的脱敏开关写入列（仅作记录，运行时以参数为权威）。 */
		sqlite3_bind_int(stmt_uris_add, 2, pInfo->bMaskBody ? 1 : 0);
		xtime tNow = xrtNow();
		sqlite3_bind_int64(stmt_uris_add, 3, tNow);
		sqlite3_bind_int64(stmt_uris_add, 4, tNow);
		sqlite3_step(stmt_uris_add);
		sqlite3_reset(stmt_uris_add);
		pInfo->AuthID = 1;
	}
	return false;
}
void Auth_UpdateURIS()
{
	// 浠庢暟鎹簱鍔犺浇 URI 閰嶇疆鍒拌矾鐢辫〃锛屽悓鏃跺垹闄ゅ凡澶辨晥鐨勮褰?
	while ( sqlite3_step(stmt_cache_uris) == SQLITE_ROW ) {
		int64 id = sqlite3_column_int64(stmt_cache_uris, 0);
		int64 authID = sqlite3_column_int64(stmt_cache_uris, 1);
		str uri = (str)sqlite3_column_text(stmt_cache_uris, 2);
		int isBackend = sqlite3_column_int(stmt_cache_uris, 3);
		int needAuth = sqlite3_column_int(stmt_cache_uris, 4);
		int needLog = sqlite3_column_int(stmt_cache_uris, 5);
		int keepActive = sqlite3_column_int(stmt_cache_uris, 6);
		size_t iSize = strlen(uri);
		RouteInfo* pInfo = RouteHTTP_Registered(uri);
		if ( pInfo ) {
			// 浠庢暟鎹簱鍔犺浇閰嶇疆鍒拌矾鐢辫〃
			pInfo->AuthID = authID;
			pInfo->bAdmin = isBackend ? true : false;
			pInfo->bAuth = needAuth ? true : false;
			pInfo->bPutLog = needLog ? true : false;
			pInfo->bActive = keepActive ? true : false;
		} else {
			/* isPersistent=0 的行：路由已卸载，自动清理；持久声明保留。 */
			int isPersistent = sqlite3_column_int(stmt_cache_uris, 7);
			if (!isPersistent && strstr(uri, "/admin/") != uri) {
				sqlite3_bind_int64(stmt_uris_del, 1, id);
				sqlite3_step(stmt_uris_del);
				sqlite3_reset(stmt_uris_del);
			}
		}
	}
	sqlite3_reset(stmt_cache_uris);
	// 閬嶅巻璺敱琛紝灏嗘暟鎹簱涓笉瀛樺湪鐨勮褰曟坊鍔犺繘鍘?
	MapWalk(G_StaticRouteTableHTTP, (MapWalkProc)AuthRouteCheckProc, NULL);
	for (size_t i = 0; i < G_DynamicCount; i++) {
		xbytesview key = {(cbytes)G_DynamicRoutes[i].Path, strlen(G_DynamicRoutes[i].Path)};
		AuthRouteCheckProc(key, &G_DynamicRoutes[i], NULL);
	}
}





// 鏉冮檺妯″潡鍒濆鍖?
static void XAdminRepairDuplicateAuthGroups()
{
	sqlite3_stmt* stmtDupSig = NULL;
	sqlite3_stmt* stmtDupRows = NULL;
	sqlite3_stmt* stmtMoveAuth = NULL;
	sqlite3_stmt* stmtDeleteGroup = NULL;
	int iRet;
	int iFixed = 0;
	xtime iNow = xrtNow();

	iRet = sqlite3_prepare_v3(G_DB, "SELECT name, desc, sort FROM authGroup WHERE isDelete = 0 GROUP BY name, desc, sort HAVING COUNT(*) > 1;", -1, 0, &stmtDupSig, NULL);
	if ( iRet != SQLITE_OK ) { goto cleanup; }
	iRet = sqlite3_prepare_v3(G_DB, "SELECT g.id, COUNT(a.id) AS authCount FROM authGroup g LEFT JOIN auth a ON a.groupID = g.id AND a.isDelete = 0 WHERE g.isDelete = 0 AND g.name = ? AND g.desc = ? AND g.sort = ? GROUP BY g.id ORDER BY authCount DESC, g.id ASC;", -1, 0, &stmtDupRows, NULL);
	if ( iRet != SQLITE_OK ) { goto cleanup; }
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE auth SET groupID = ? WHERE groupID = ? AND isDelete = 0;", -1, 0, &stmtMoveAuth, NULL);
	if ( iRet != SQLITE_OK ) { goto cleanup; }
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE authGroup SET isDelete = 1, updateTime = ? WHERE id = ?;", -1, 0, &stmtDeleteGroup, NULL);
	if ( iRet != SQLITE_OK ) { goto cleanup; }

	while ( sqlite3_step(stmtDupSig) == SQLITE_ROW ) {
		const char* sName = (const char*)sqlite3_column_text(stmtDupSig, 0);
		const char* sDesc = (const char*)sqlite3_column_text(stmtDupSig, 1);
		int iSort = sqlite3_column_int(stmtDupSig, 2);
		int iKeepId = 0;

		sqlite3_bind_text(stmtDupRows, 1, sName ? sName : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmtDupRows, 2, sDesc ? sDesc : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmtDupRows, 3, iSort);
		while ( sqlite3_step(stmtDupRows) == SQLITE_ROW ) {
			int iDupId = sqlite3_column_int(stmtDupRows, 0);
			if ( iKeepId <= 0 ) {
				iKeepId = iDupId;
				continue;
			}

			sqlite3_bind_int(stmtMoveAuth, 1, iKeepId);
			sqlite3_bind_int(stmtMoveAuth, 2, iDupId);
			sqlite3_step(stmtMoveAuth);
			sqlite3_reset(stmtMoveAuth);
			sqlite3_clear_bindings(stmtMoveAuth);

			sqlite3_bind_int64(stmtDeleteGroup, 1, iNow);
			sqlite3_bind_int(stmtDeleteGroup, 2, iDupId);
			sqlite3_step(stmtDeleteGroup);
			sqlite3_reset(stmtDeleteGroup);
			sqlite3_clear_bindings(stmtDeleteGroup);
			iFixed++;
		}
		sqlite3_reset(stmtDupRows);
		sqlite3_clear_bindings(stmtDupRows);
	}
	sqlite3_reset(stmtDupSig);

cleanup:
	if ( stmtDeleteGroup ) { sqlite3_finalize(stmtDeleteGroup); }
	if ( stmtMoveAuth ) { sqlite3_finalize(stmtMoveAuth); }
	if ( stmtDupRows ) { sqlite3_finalize(stmtDupRows); }
	if ( stmtDupSig ) { sqlite3_finalize(stmtDupSig); }
	if ( iFixed > 0 ) {
		printf("        Auth_Init repaired %d duplicate authGroup rows\n", iFixed);
	}
}

static void XAdminRepairDuplicateAuthItems()
{
	sqlite3_stmt* stmtDupSig = NULL;
	sqlite3_stmt* stmtDupRows = NULL;
	sqlite3_stmt* stmtMoveUris = NULL;
	sqlite3_stmt* stmtDeleteAuth = NULL;
	int iRet;
	int iFixed = 0;
	xtime iNow = xrtNow();

	iRet = sqlite3_prepare_v3(G_DB, "SELECT groupID, name, desc, sort FROM auth WHERE isDelete = 0 GROUP BY groupID, name, desc, sort HAVING COUNT(*) > 1;", -1, 0, &stmtDupSig, NULL);
	if ( iRet != SQLITE_OK ) { goto cleanup; }
	iRet = sqlite3_prepare_v3(G_DB, "SELECT id FROM auth WHERE isDelete = 0 AND groupID = ? AND name = ? AND desc = ? AND sort = ? ORDER BY id ASC;", -1, 0, &stmtDupRows, NULL);
	if ( iRet != SQLITE_OK ) { goto cleanup; }
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE uris SET authID = ? WHERE authID = ?;", -1, 0, &stmtMoveUris, NULL);
	if ( iRet != SQLITE_OK ) { goto cleanup; }
	iRet = sqlite3_prepare_v3(G_DB, "UPDATE auth SET isDelete = 1, updateTime = ? WHERE id = ?;", -1, 0, &stmtDeleteAuth, NULL);
	if ( iRet != SQLITE_OK ) { goto cleanup; }

	while ( sqlite3_step(stmtDupSig) == SQLITE_ROW ) {
		int iGroupId = sqlite3_column_int(stmtDupSig, 0);
		const char* sName = (const char*)sqlite3_column_text(stmtDupSig, 1);
		const char* sDesc = (const char*)sqlite3_column_text(stmtDupSig, 2);
		int iSort = sqlite3_column_int(stmtDupSig, 3);
		int iKeepId = 0;

		sqlite3_bind_int(stmtDupRows, 1, iGroupId);
		sqlite3_bind_text(stmtDupRows, 2, sName ? sName : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_text(stmtDupRows, 3, sDesc ? sDesc : "", -1, SQLITE_TRANSIENT);
		sqlite3_bind_int(stmtDupRows, 4, iSort);
		while ( sqlite3_step(stmtDupRows) == SQLITE_ROW ) {
			int iDupId = sqlite3_column_int(stmtDupRows, 0);
			if ( iKeepId <= 0 ) {
				iKeepId = iDupId;
				continue;
			}

			sqlite3_bind_int(stmtMoveUris, 1, iKeepId);
			sqlite3_bind_int(stmtMoveUris, 2, iDupId);
			sqlite3_step(stmtMoveUris);
			sqlite3_reset(stmtMoveUris);
			sqlite3_clear_bindings(stmtMoveUris);

			sqlite3_bind_int64(stmtDeleteAuth, 1, iNow);
			sqlite3_bind_int(stmtDeleteAuth, 2, iDupId);
			sqlite3_step(stmtDeleteAuth);
			sqlite3_reset(stmtDeleteAuth);
			sqlite3_clear_bindings(stmtDeleteAuth);
			iFixed++;
		}
		sqlite3_reset(stmtDupRows);
		sqlite3_clear_bindings(stmtDupRows);
	}
	sqlite3_reset(stmtDupSig);

cleanup:
	if ( stmtDeleteAuth ) { sqlite3_finalize(stmtDeleteAuth); }
	if ( stmtMoveUris ) { sqlite3_finalize(stmtMoveUris); }
	if ( stmtDupRows ) { sqlite3_finalize(stmtDupRows); }
	if ( stmtDupSig ) { sqlite3_finalize(stmtDupSig); }
	if ( iFixed > 0 ) {
		printf("        Auth_Init repaired %d duplicate auth rows\n", iFixed);
	}
}

void Auth_Init()
{
	printf("        Auth_Init \n");
	
	// 棰勭紪璇?SQL 璇彞
	Auth_CompileSQL();
	XAdminRepairDuplicateAuthGroups();
	XAdminRepairDuplicateAuthItems();
	
	// 閲嶆柊鍔犺浇鍏ㄥ眬缂撳瓨
	ReloadCache_Auth_Auth();
	ReloadCache_Auth_Group();
	Auth_ReloadCache();

}

// 鍚屾 URI 琛紙搴斿湪鎵€鏈夎矾鐢辨敞鍐屽畬鎴愬悗璋冪敤锛?
void Auth_SyncURIS()
{
	// 鏇存柊 uris 琛?( 娣诲姞鏈敹褰曠殑 URI锛屽垹闄ゅ凡澶辨晥鐨?URI )
	Auth_UpdateURIS();
	
	// 閲嶆柊鍔犺浇缂撳瓨
	Auth_ReloadCache();
}



// 鍗歌浇鏉冮檺妯″潡
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
	
	// 閲婃斁缂撳瓨鐩稿叧鐨勯缂栬瘧璇彞
	sqlite3_finalize(stmt_cache_auth);
	sqlite3_finalize(stmt_cache_group);
	sqlite3_finalize(stmt_cache_role);
	sqlite3_finalize(stmt_cache_uris);
	
	// 閲婃斁鍏ㄥ眬缂撳瓨琛?
	xrtValueRelease(G_CACHE_RoleAuth);
	xrtValueRelease(G_CACHE_RoleAuthIndex);
	xrtValueRelease(G_CACHE_RoleAuthLevel);
	xrtValueRelease(G_CACHE_Auth);
	xrtValueRelease(G_CACHE_Group);
	xrtValueRelease(G_CACHE_Role);
	/* L1：计数语句 */
	sqlite3_finalize(stmt_uris_count_all);
	sqlite3_finalize(stmt_uris_count_sel);
	sqlite3_finalize(stmt_auth_count_all);
	sqlite3_finalize(stmt_auth_count_sel);
	sqlite3_finalize(stmt_group_count_all);
	sqlite3_finalize(stmt_group_count_sel);
	sqlite3_finalize(stmt_role_count_all);
	sqlite3_finalize(stmt_role_count_sel);
	sqlite3_finalize(stmt_user_count_all);
	sqlite3_finalize(stmt_user_count_sel);

}


