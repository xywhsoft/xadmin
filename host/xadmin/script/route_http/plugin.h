


// ============================================
// 插件管理后台路由
// ============================================



// 获取插件列表 API
void Request_Plugin_List(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	xvalue arrList = PluginMgr_GetList();
	
	xvalue tblRet = xvoCreateTable();
	xvoTableSetInt(tblRet, "code", 4, 0);
	xvoTableSetText(tblRet, "msg", 3, "", 0, FALSE);
	xvoTableSetInt(tblRet, "count", 5, xvoArrayItemCount(arrList));
	xvoTableSetValue(tblRet, "data", 4, arrList, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}



// 获取插件详情 API
void Request_Plugin_Get(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sName[64];
	mg_http_get_var(&hm->query, "name", sName, sizeof(sName));
	
	if ( strlen(sName) == 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing plugin name\"}", 0);
		return;
	}
	
	PluginInstance* pPlugin = PluginMgr_GetPlugin(sName);
	if ( !pPlugin ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Plugin not found\"}", 0);
		return;
	}
	
	xvalue tblData = xvoCreateTable();
	xvoTableSetText(tblData, "name", 4, pPlugin->sName, 0, FALSE);
	xvoTableSetText(tblData, "title", 5, pPlugin->sTitle, 0, FALSE);
	xvoTableSetText(tblData, "desc", 4, pPlugin->sDesc, 0, FALSE);
	xvoTableSetText(tblData, "version", 7, pPlugin->sVersion, 0, FALSE);
	xvoTableSetText(tblData, "author", 6, pPlugin->sAuthor, 0, FALSE);
	xvoTableSetInt(tblData, "sort", 4, pPlugin->iSort);
	xvoTableSetBool(tblData, "enabled", 7, pPlugin->bEnabled);
	xvoTableSetBool(tblData, "loaded", 6, pPlugin->bLoaded);
	xvoTableSetText(tblData, "path", 4, pPlugin->sPath, 0, FALSE);
	
	if ( pPlugin->tblSettings ) {
		xvoAddRef(pPlugin->tblSettings);
		xvoTableSetValue(tblData, "settings", 8, pPlugin->tblSettings, TRUE);
	}
	
	xvalue tblRet = xvoCreateTable();
	xvoTableSetBool(tblRet, "result", 6, TRUE);
	xvoTableSetValue(tblRet, "data", 4, tblData, TRUE);
	
	size_t iSize = 0;
	str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iSize);
	xrtFree(sJson);
	xvoUnref(tblRet);
}



// 启用插件 API
void Request_Plugin_Enable(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( !tblForm ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		return;
	}
	
	str sName = xvoTableGetText(tblForm, "name", 4);
	if ( !sName || strlen(sName) == 0 ) {
		xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing plugin name\"}", 0);
		return;
	}
	
	bool bResult = PluginMgr_EnablePlugin(sName);
	xvoUnref(tblForm);
	
	if ( bResult ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"Plugin enabled\"}", 0);
	} else {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Failed to enable plugin\"}", 0);
	}
}



// 禁用插件 API
void Request_Plugin_Disable(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( !tblForm ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		return;
	}
	
	str sName = xvoTableGetText(tblForm, "name", 4);
	if ( !sName || strlen(sName) == 0 ) {
		xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing plugin name\"}", 0);
		return;
	}
	
	bool bResult = PluginMgr_DisablePlugin(sName);
	xvoUnref(tblForm);
	
	if ( bResult ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"Plugin disabled\"}", 0);
	} else {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Failed to disable plugin\"}", 0);
	}
}



// 重载插件 API
void Request_Plugin_Reload(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
		return;
	}
	
	xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( !tblForm ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
		return;
	}
	
	str sName = xvoTableGetText(tblForm, "name", 4);
	if ( !sName || strlen(sName) == 0 ) {
		xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing plugin name\"}", 0);
		return;
	}
	
	bool bResult = PluginMgr_ReloadPlugin(sName);
	xvoUnref(tblForm);
	
	if ( bResult ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"Plugin reloaded\"}", 0);
	} else {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Failed to reload plugin\"}", 0);
	}
}



// 插件设置 API
void Request_Plugin_Settings(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode == HTTP_GET ) {
		// 获取设置
		char sName[64];
		mg_http_get_var(&hm->query, "name", sName, sizeof(sName));
		
		if ( strlen(sName) == 0 ) {
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing plugin name\"}", 0);
			return;
		}
		
		PluginInstance* pPlugin = PluginMgr_GetPlugin(sName);
		if ( !pPlugin ) {
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Plugin not found\"}", 0);
			return;
		}
		
		xvalue tblRet = xvoCreateTable();
		xvoTableSetBool(tblRet, "result", 6, TRUE);
		if ( pPlugin->tblSettings ) {
			xvoAddRef(pPlugin->tblSettings);
			xvoTableSetValue(tblRet, "data", 4, pPlugin->tblSettings, TRUE);
		} else {
			xvalue tblEmpty = xvoCreateTable();
			xvoTableSetValue(tblRet, "data", 4, tblEmpty, TRUE);
		}
		
		size_t iSize = 0;
		str sJson = xrtStringifyJSON(tblRet, FALSE, &iSize);
		http_reply(c, 200, HTTP_CT_JSON, sJson, iSize);
		xrtFree(sJson);
		xvoUnref(tblRet);
		
	} else if ( hm->methodCode == HTTP_POST ) {
		// 保存设置
		xvalue tblForm = xrtParseJSON(hm->body.buf, hm->body.len);
		if ( !tblForm ) {
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Invalid data\"}", 0);
			return;
		}
		
		str sName = xvoTableGetText(tblForm, "name", 4);
		if ( !sName || strlen(sName) == 0 ) {
			xvoUnref(tblForm);
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Missing plugin name\"}", 0);
			return;
		}
		
		PluginInstance* pPlugin = PluginMgr_GetPlugin(sName);
		if ( !pPlugin ) {
			xvoUnref(tblForm);
			http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Plugin not found\"}", 0);
			return;
		}
		
		xvalue tblSettings = xvoTableGetValue(tblForm, "settings", 8);
		if ( tblSettings ) {
			if ( pPlugin->tblSettings ) {
				xvoUnref(pPlugin->tblSettings);
			}
			xvoAddRef(tblSettings);
			pPlugin->tblSettings = tblSettings;
			Plugin_SaveConfig(pPlugin);
		}
		
		xvoUnref(tblForm);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"Settings saved\"}", 0);
		
	} else {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"Method not allowed\"}", 0);
	}
}



// 插件列表页面
void Request_View_Plugin_List(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	
	LoadPage(c, 200, HTTP_CT_HTML, "plugin/list.html");
	
}


