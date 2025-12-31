


// 编译好的 SQL 语句
sqlite3_stmt* stmt_logs_all = NULL;
sqlite3_stmt* stmt_logs_sel = NULL;
sqlite3_stmt* stmt_logs_add = NULL;
sqlite3_stmt* stmt_logs_clear = NULL;



// 日志模块初始化
void Logs_Init()
{
	printf("        Logs_Init \n");
	
	// 编译 logs 表的 SQL 语句
	int iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT *, COUNT(*) OVER() AS total_count FROM logs ORDER BY id DESC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_logs_all, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Logs_Init [stmt_logs_all] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "SELECT *, COUNT(*) OVER() AS total_count FROM logs WHERE uri LIKE ? ORDER BY id DESC LIMIT ?  OFFSET ?;", -1, SQL_PREPARE_DEFAULT, &stmt_logs_sel, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Logs_Init [stmt_logs_sel] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	iRet = sqlite3_prepare_v3(G_DB->objDB, "INSERT INTO logs (user, ip, uri, method, param, body, createTime) VALUES (?, ?, ?, ?, ?, ?, ?);", -1, SQL_PREPARE_DEFAULT, &stmt_logs_add, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Logs_Init [stmt_logs_add] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
	
	// 编译清理日志的 SQL 语句
	iRet = sqlite3_prepare_v3(G_DB->objDB, "DELETE FROM logs WHERE createTime < ?;", -1, SQL_PREPARE_DEFAULT, &stmt_logs_clear, NULL);
	if ( iRet != SQLITE_OK ) {
		printf("!!! ERROR !!! Logs_Init [stmt_logs_clear] - sqlite3_prepare_v3 error code : %d\n%s\n", iRet, sqlite3_errmsg(G_DB->objDB));
		exit(0);
	}
}



// 记录访问日志
void Logs_Add(struct mg_connection* c, struct mg_http_message* hm)
{
	// 获取用户名
	str user = "(guest)";
	if ( hm->session && (hm->session->Type == XVO_DT_TABLE) ) {
		user = xvoTableGetText(hm->session, "user", 4);
		if ( !user ) user = "(unknown)";
	}
	
	// 获取 IP 地址（存储为字符串）
	char ip_str[64];
    mg_snprintf(ip_str, sizeof(ip_str), "%M", mg_print_ip, &c->rem);
	
	// 获取 URI
	str uri = "";
	if ( hm->uri.len > 0 ) {
		uri = hm->uri.buf;
	}
	
	// 获取 URL 参数
	str param = "";
	if ( hm->query.len > 0 ) {
		param = hm->query.buf;
	}
	
	// 获取请求体
	str body = "";
	if ( ((hm->methodCode == HTTP_POST) || (hm->methodCode == HTTP_PUT)) && hm->body.len > 0 ) {
		body = hm->body.buf;
	}
	
	// 获取当前时间
	xtime now = xrtNow();
	
	// 绑定参数并执行
	sqlite3_bind_text(stmt_logs_add, 1, user, strlen(user), SQLITE_STATIC);
	sqlite3_bind_text(stmt_logs_add, 2, ip_str, strlen(ip_str), SQLITE_STATIC);
	sqlite3_bind_text(stmt_logs_add, 3, uri, hm->uri.len, SQLITE_STATIC);
	sqlite3_bind_text(stmt_logs_add, 4, hm->method.buf, hm->method.len, SQLITE_STATIC);
	sqlite3_bind_text(stmt_logs_add, 5, param, hm->query.len, SQLITE_STATIC);
	sqlite3_bind_text(stmt_logs_add, 6, body, hm->body.len, SQLITE_STATIC);
	sqlite3_bind_int64(stmt_logs_add, 7, now);
	
	// 执行并重置
	sqlite3_step(stmt_logs_add);
	sqlite3_reset(stmt_logs_add);
}



// 日志模块卸载
void Logs_Unit()
{
	printf("        Logs_Unit \n");
	sqlite3_finalize(stmt_logs_all);
	sqlite3_finalize(stmt_logs_sel);
	sqlite3_finalize(stmt_logs_add);
	sqlite3_finalize(stmt_logs_clear);
}


