



// 获取配置页面视图
void Request_View_Option(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode == HTTP_GET ) {
		
		// 配置页面
		LoadPage(c, 200, HTTP_CT_HTML, "option.html");
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}



// 配置数据接口
void Request_Option(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode == HTTP_GET ) {
		
		// 获取文件名参数
		char sFileName[128];
		int iSize = mg_http_get_var(&hm->query, "file", sFileName, sizeof(sFileName));
		
		if ( iSize <= 0 ) {
			// 缺少文件名参数
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"缺少 file 参数\"}", 0);
			return;
		}
		
		// 安全检查：防止路径遍历攻击
		if ( (strstr(sFileName, "..") != NULL) || (strstr(sFileName, "/") != NULL) || (strstr(sFileName, "\\") != NULL) ) {
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"非法的文件名\"}", 0);
			return;
		}
		
		// 加载配置文件
		xvalue tblConfig = Option_LoadFile(sFileName);
		if ( tblConfig == NULL ) {
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"配置文件不存在或解析失败\"}", 0);
			return;
		}
		
		// 构建返回值
		xvalue tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		xvoTableSetText(tblRet, "message", 7, "配置数据获取成功", 0, FALSE);
		xvoTableSetValue(tblRet, "data", 4, tblConfig, TRUE);
		
		// 生成 JSON
		size_t iRetSize = 0;
		char* sRet = xrtStringifyJSON(tblRet, FALSE, &iRetSize);
		http_reply(c, 200, HTTP_CT_JSON, sRet, iRetSize);
		xrtFree(sRet);
		xvoUnref(tblRet);
		
	} else if ( hm->methodCode == HTTP_POST ) {
		
		// 解析请求体
		xvalue tblBody = xrtParseJSON(hm->body.buf, hm->body.len);
		if ( tblBody == NULL ) {
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"请求数据格式错误\"}", 0);
			return;
		}
		
		// 获取文件名
		str sFileName = xvoTableGetText(tblBody, "file", 4);
		if ( (sFileName == NULL) || (strlen(sFileName) == 0) ) {
			xvoUnref(tblBody);
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"缺少 file 参数\"}", 0);
			return;
		}
		
		// 安全检查：防止路径遍历攻击
		if ( (strstr(sFileName, "..") != NULL) || (strstr(sFileName, "/") != NULL) || (strstr(sFileName, "\\") != NULL) ) {
			xvoUnref(tblBody);
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"非法的文件名\"}", 0);
			return;
		}
		
		// 获取表单数据
		xvalue tblFormData = xvoTableGetValue(tblBody, "data", 4);
		if ( tblFormData == NULL ) {
			xvoUnref(tblBody);
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"缺少 data 参数\"}", 0);
			return;
		}
		
		// 保存配置
		bool bRet = Option_SaveFile(sFileName, tblFormData);
		xvoUnref(tblBody);
		
		if ( bRet ) {
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\": true, \"message\": \"配置保存成功\"}", 0);
		} else {
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\": false, \"message\": \"配置保存失败\"}", 0);
		}
		
	} else {
		
		// 其他请求方法返回 404 页面
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
		
	}
}


