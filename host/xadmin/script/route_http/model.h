


// ============================================
// 模型管理路由处理
// ============================================



// ==================== 页面视图路由 ====================

// 模型列表页面
void Request_View_Model_List(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode == HTTP_GET ) {
		LoadPage(c, 200, HTTP_CT_HTML, "model/list.html");
	} else {
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
	}
}

// 模型添加页面
void Request_View_Model_Add(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode == HTTP_GET ) {
		LoadPage(c, 200, HTTP_CT_HTML, "model/add.html");
	} else {
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
	}
}

// 模型编辑页面
void Request_View_Model_Edit(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode == HTTP_GET ) {
		LoadPage(c, 200, HTTP_CT_HTML, "model/edit.html");
	} else {
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
	}
}

// 字段管理页面
void Request_View_Model_Fields(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode == HTTP_GET ) {
		LoadPage(c, 200, HTTP_CT_HTML, "model/fields.html");
	} else {
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
	}
}



// ==================== 数据API路由 ====================

// 获取模型列表
void Request_Model_List(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_GET ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"不支持的请求方法\"}", 0);
		return;
	}
	
	// 获取分页参数
	char sParam[256];
	mg_http_get_var(&hm->query, "page", sParam, sizeof(sParam));
	int64 iPage = xrtStrToI64(sParam);
	if ( iPage <= 0 ) { iPage = 1; }
	mg_http_get_var(&hm->query, "limit", sParam, sizeof(sParam));
	int64 iLimit = xrtStrToI64(sParam);
	if ( iLimit <= 0 ) { iLimit = 20; }
	int iSearchSize = mg_http_get_var(&hm->query, "search", sParam, sizeof(sParam));
	
	// 获取所有模型
	xvalue arrList = ModelMgr_GetModelList();
	int iTotal = xvoArrayItemCount(arrList);
	
	// 搜索过滤
	xvalue arrFiltered = xvoCreateArray();
	for ( int i = 0; i < iTotal; i++ ) {
		xvalue tblItem = xvoArrayGetValue(arrList, i);
		if ( iSearchSize > 2 ) {
			str sName = xvoTableGetText(tblItem, "name", 4);
			str sTitle = xvoTableGetText(tblItem, "title", 5);
			// 简单匹配
			if ( (sName && strstr(sName, sParam + 1)) || (sTitle && strstr(sTitle, sParam + 1)) ) {
				xvoAddRef(tblItem);
				xvoArrayAppendValue(arrFiltered, tblItem, TRUE);
			}
		} else {
			xvoAddRef(tblItem);
			xvoArrayAppendValue(arrFiltered, tblItem, TRUE);
		}
	}
	
	int iFilteredTotal = xvoArrayItemCount(arrFiltered);
	
	// 分页
	int iStart = (iPage - 1) * iLimit;
	int iEnd = iStart + iLimit;
	if ( iEnd > iFilteredTotal ) iEnd = iFilteredTotal;
	
	xvalue arrPage = xvoCreateArray();
	for ( int i = iStart; i < iEnd; i++ ) {
		xvalue tblItem = xvoArrayGetValue(arrFiltered, i);
		xvoAddRef(tblItem);
		xvoArrayAppendValue(arrPage, tblItem, TRUE);
	}
	
	// 构建响应
	xvalue tblResponse = xvoCreateTable();
	xvoTableSetInt(tblResponse, "code", 4, 0);
	xvoTableSetText(tblResponse, "msg", 3, "", 0, FALSE);
	xvoTableSetInt(tblResponse, "count", 5, iFilteredTotal);
	xvoTableSetValue(tblResponse, "data", 4, arrPage, TRUE);
	
	// 发送响应
	size_t iRetSize = 0;
	str sJson = xrtStringifyJSON(tblResponse, FALSE, &iRetSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iRetSize);
	xrtFree(sJson);
	
	xvoUnref(tblResponse);
	xvoUnref(arrFiltered);
	xvoUnref(arrList);
}


// 获取单个模型
void Request_Model_Get(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sName[64] = {0};
	mg_http_get_var(&hm->query, "name", sName, sizeof(sName));
	if ( strlen(sName) == 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"缺少模型名称\"}", 0);
		return;
	}
	
	ModelInstance* pModel = ModelMgr_GetModel(sName);
	
	if ( !pModel ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"模型不存在\"}", 0);
		return;
	}
	
	// 构建响应数据
	xvalue tblData = xvoCreateTable();
	xvoTableSetText(tblData, "name", 4, pModel->sName, 0, FALSE);
	xvoTableSetText(tblData, "title", 5, pModel->sTitle ? pModel->sTitle : (str)"", 0, FALSE);
	xvoTableSetText(tblData, "namespace", 9, pModel->sNamespace ? pModel->sNamespace : (str)"", 0, FALSE);
	xvoTableSetText(tblData, "desc", 4, pModel->sDesc ? pModel->sDesc : (str)"", 0, FALSE);
	xvoTableSetText(tblData, "icon", 4, pModel->sIcon ? pModel->sIcon : (str)"", 0, FALSE);
	xvoTableSetBool(tblData, "enabled", 7, pModel->bEnabled);
	xvoTableSetBool(tblData, "compiled", 8, pModel->bCompiled);
	xvoTableSetBool(tblData, "enableApi", 9, pModel->bEnableApi);
	xvoTableSetBool(tblData, "enableAdmin", 11, pModel->bEnableAdmin);
	xvoTableSetBool(tblData, "enableSubmit", 12, pModel->bEnableSubmit);
	xvoTableSetBool(tblData, "enableReply", 11, pModel->bEnableReply);
	xvoTableSetInt(tblData, "apiAuthLevel", 12, pModel->iApiAuthLevel);
	xvoTableSetBool(tblData, "replyNeedApprove", 16, pModel->bReplyNeedApprove);
	xvoTableSetInt(tblData, "replyAuthLevel", 14, pModel->iReplyAuthLevel);
	xvoTableSetInt(tblData, "replyQuoteMaxLen", 16, pModel->iReplyQuoteMaxLen);
	
	xvalue tblResponse = xvoCreateTable();
	xvoTableSetBool(tblResponse, "result", 6, TRUE);
	xvoTableSetValue(tblResponse, "data", 4, tblData, TRUE);
	
	size_t iJsonSize = 0;
	str sJson = xrtStringifyJSON(tblResponse, FALSE, &iJsonSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iJsonSize);
	xrtFree(sJson);
	
	xvoUnref(tblResponse);
}


// 创建模型
void Request_Model_Add(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"不支持的请求方法\"}", 0);
		return;
	}
	
	// 解析JSON
	xvalue tblData = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( !tblData ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"无效的请求数据\"}", 0);
		return;
	}
	
	str sName = xvoTableGetText(tblData, "name", 4);
	str sTitle = xvoTableGetText(tblData, "title", 5);
	str sNamespace = xvoTableGetText(tblData, "namespace", 9);
	str sDesc = xvoTableGetText(tblData, "desc", 4);
	str sIcon = xvoTableGetText(tblData, "icon", 4);
	
	// 验证必填字段
	if ( !sName || strlen(sName) == 0 ) {
		xvoUnref(tblData);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"模型标识不能为空\"}", 0);
		return;
	}
	
	// 检查模型是否已存在
	if ( ModelMgr_GetModel(sName) ) {
		xvoUnref(tblData);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"模型标识已存在\"}", 0);
		return;
	}
	
	// 检查命名空间唯一性
	if ( !Model_CheckNamespace(sNamespace, sName, NULL) ) {
		xvoUnref(tblData);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"命名空间已被占用\"}", 0);
		return;
	}
	
	// 创建模型目录
	str sModelDir = xrtFormat("%s/%s", ModelPath, sName);
	if ( !xrtDirCreate(sModelDir) ) {
		xrtFree(sModelDir);
		xvoUnref(tblData);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"创建模型目录失败\"}", 0);
		return;
	}
	
	// 创建配置文件
	int64 iNow = xrtNow();
	xvalue tblConfig = xvoCreateTable();
	xvoTableSetText(tblConfig, "title", 5, sTitle ? sTitle : (str)"", 0, FALSE);
	xvoTableSetText(tblConfig, "namespace", 9, sNamespace ? sNamespace : (str)"", 0, FALSE);
	xvoTableSetText(tblConfig, "desc", 4, sDesc ? sDesc : (str)"", 0, FALSE);
	xvoTableSetText(tblConfig, "icon", 4, sIcon ? sIcon : (str)"layui-icon-file", 0, FALSE);
	
	// 表配置
	xvalue tblTable = xvoCreateTable();
	str sTableName = xrtFormat("model_%s_%s", (sNamespace && strlen(sNamespace) > 0) ? sNamespace : (str)"default", sName);
	xvoTableSetText(tblTable, "name", 4, sTableName, 0, FALSE);
	xrtFree(sTableName);
	xvoTableSetValue(tblConfig, "table", 5, tblTable, TRUE);
	
	// 状态
	xvalue tblStatus = xvoCreateTable();
	xvoTableSetBool(tblStatus, "enabled", 7, FALSE);
	xvoTableSetBool(tblStatus, "compiled", 8, FALSE);
	xvoTableSetValue(tblConfig, "status", 6, tblStatus, TRUE);
	
	// 功能开关
	xvalue tblFeatures = xvoCreateTable();
	xvoTableSetBool(tblFeatures, "enableApi", 9, TRUE);
	xvoTableSetBool(tblFeatures, "enableAdmin", 11, TRUE);
	xvoTableSetBool(tblFeatures, "enableSubmit", 12, FALSE);
	xvoTableSetBool(tblFeatures, "enableReply", 11, FALSE);
	xvoTableSetValue(tblConfig, "features", 8, tblFeatures, TRUE);
	
	// 空字段数组
	xvalue arrFields = xvoCreateArray();
	xvoTableSetValue(tblConfig, "fields", 6, arrFields, TRUE);
	
	// 时间戳
	xvoTableSetInt(tblConfig, "createTime", 10, iNow);
	xvoTableSetInt(tblConfig, "updateTime", 10, iNow);
	
	// 保存配置文件
	str sConfigPath = xrtFormat("%s/config.json", sModelDir);
	int iResult = xrtStringifyJSON_File(sConfigPath, tblConfig, TRUE);
	
	xvoUnref(tblConfig);
	xrtFree(sConfigPath);
	xrtFree(sModelDir);
	xvoUnref(tblData);
	
	if ( iResult ) {
		// 重新扫描模型
		// 简单方案：直接加载新创建的模型
		str sNewConfigPath = xrtFormat("%s/%s/config.json", ModelPath, sName);
		ModelInstance* pNewModel = Model_LoadFromConfig(sName, sNewConfigPath);
		xrtFree(sNewConfigPath);
		
		if ( pNewModel ) {
			ModelInstance** ppSlot = xrtDictSet(G_ModelMgr->tblModels, pNewModel->sName, strlen(pNewModel->sName), NULL);
			if ( ppSlot ) {
				*ppSlot = pNewModel;
			}
			Model_RegisterNamespace(pNewModel);
		}
		
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"创建成功\"}", 0);
	} else {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"保存配置文件失败\"}", 0);
	}
}


// 保存模型
void Request_Model_Save(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"不支持的请求方法\"}", 0);
		return;
	}
	
	// 解析JSON
	xvalue tblData = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( !tblData ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"无效的请求数据\"}", 0);
		return;
	}
	
	str sName = xvoTableGetText(tblData, "name", 4);
	if ( !sName ) {
		xvoUnref(tblData);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"缺少模型名称\"}", 0);
		return;
	}
	
	ModelInstance* pModel = ModelMgr_GetModel(sName);
	if ( !pModel ) {
		xvoUnref(tblData);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"模型不存在\"}", 0);
		return;
	}
	
	// 读取原配置文件
	xvalue tblConfig = xrtParseJSON_File(pModel->sConfigPath);
	if ( !tblConfig ) {
		xvoUnref(tblData);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"读取配置文件失败\"}", 0);
		return;
	}
	
	// 更新基础信息
	str sTitle = xvoTableGetText(tblData, "title", 5);
	if ( sTitle ) xvoTableSetText(tblConfig, "title", 5, sTitle, 0, FALSE);
	
	str sNamespace = xvoTableGetText(tblData, "namespace", 9);
	xvoTableSetText(tblConfig, "namespace", 9, sNamespace ? sNamespace : (str)"", 0, FALSE);
	
	str sDesc = xvoTableGetText(tblData, "desc", 4);
	xvoTableSetText(tblConfig, "desc", 4, sDesc ? sDesc : (str)"", 0, FALSE);
	
	str sIcon = xvoTableGetText(tblData, "icon", 4);
	if ( sIcon ) xvoTableSetText(tblConfig, "icon", 4, sIcon, 0, FALSE);
	
	// 更新功能开关
	xvalue tblFeatures = xvoTableGetValue(tblConfig, "features", 8);
	if ( !tblFeatures || tblFeatures->Type != XVO_DT_TABLE ) {
		tblFeatures = xvoCreateTable();
		xvoTableSetValue(tblConfig, "features", 8, tblFeatures, TRUE);
	}
	xvoTableSetBool(tblFeatures, "enableApi", 9, xvoTableGetBool(tblData, "enableApi", 9));
	xvoTableSetBool(tblFeatures, "enableAdmin", 11, xvoTableGetBool(tblData, "enableAdmin", 11));
	xvoTableSetBool(tblFeatures, "enableSubmit", 12, xvoTableGetBool(tblData, "enableSubmit", 12));
	xvoTableSetBool(tblFeatures, "enableReply", 11, xvoTableGetBool(tblData, "enableReply", 11));
	
	// 更新API权限配置
	xvalue tblApi = xvoTableGetValue(tblConfig, "api", 3);
	if ( !tblApi || tblApi->Type != XVO_DT_TABLE ) {
		tblApi = xvoCreateTable();
		xvoTableSetValue(tblConfig, "api", 3, tblApi, TRUE);
	}
	xvoTableSetInt(tblApi, "authLevel", 9, xvoTableGetInt(tblData, "apiAuthLevel", 12));
	
	// 更新评论配置
	xvalue tblReply = xvoTableGetValue(tblConfig, "reply", 5);
	if ( !tblReply || tblReply->Type != XVO_DT_TABLE ) {
		tblReply = xvoCreateTable();
		xvoTableSetValue(tblConfig, "reply", 5, tblReply, TRUE);
	}
	xvoTableSetBool(tblReply, "needApprove", 11, xvoTableGetBool(tblData, "replyNeedApprove", 16));
	xvoTableSetInt(tblReply, "authLevel", 9, xvoTableGetInt(tblData, "replyAuthLevel", 14));
	xvoTableSetInt(tblReply, "quoteMaxLen", 11, xvoTableGetInt(tblData, "replyQuoteMaxLen", 16));
	
	// 更新时间戳
	xvoTableSetInt(tblConfig, "updateTime", 10, xrtNow());
	
	// 标记需要重新编译
	xvalue tblStatus = xvoTableGetValue(tblConfig, "status", 6);
	if ( tblStatus ) {
		xvoTableSetBool(tblStatus, "compiled", 8, FALSE);
	}
	
	// 保存配置
	int iResult = xrtStringifyJSON_File(pModel->sConfigPath, tblConfig, TRUE);
	xvoUnref(tblConfig);
	xvoUnref(tblData);
	
	if ( iResult ) {
		// 更新内存中的模型实例
		if ( sTitle && pModel->sTitle ) { xrtFree(pModel->sTitle); pModel->sTitle = xrtCopyStr(sTitle, 0); }
		if ( pModel->sNamespace ) xrtFree(pModel->sNamespace);
		pModel->sNamespace = sNamespace ? xrtCopyStr(sNamespace, 0) : NULL;
		if ( pModel->sDesc ) xrtFree(pModel->sDesc);
		pModel->sDesc = sDesc ? xrtCopyStr(sDesc, 0) : NULL;
		if ( sIcon && pModel->sIcon ) { xrtFree(pModel->sIcon); pModel->sIcon = xrtCopyStr(sIcon, 0); }
		pModel->bCompiled = FALSE;
		pModel->iUpdateTime = xrtNow();
		
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"保存成功\"}", 0);
	} else {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"保存配置文件失败\"}", 0);
	}
}


// 删除模型
void Request_Model_Delete(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sName[64];
	mg_http_get_var(&hm->query, "name", sName, sizeof(sName));
	if ( strlen(sName) == 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"缺少模型名称\"}", 0);
		return;
	}
	
	ModelInstance* pModel = ModelMgr_GetModel(sName);
	if ( !pModel ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"模型不存在\"}", 0);
		return;
	}
	
	// 如果模型已启用，先禁用（移除路由、菜单、权限组）
	if ( pModel->bEnabled ) {
		Model_Disable(pModel);
	}
	
	// 注销命名空间
	Model_UnregisterNamespace(pModel);
	
	// 从模型表中移除
	xrtDictRemove(G_ModelMgr->tblModels, sName, strlen(sName));
	
	// 删除模型目录
	str sModelDir = xrtFormat("%s/%s", ModelPath, sName);
	xrtDirDelete(sModelDir);
	xrtFree(sModelDir);
	
	// 销毁模型实例
	Model_Destroy(pModel);
	
	http_reply(c, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"删除成功\"}", 0);
}


// 获取模型字段
void Request_Model_Fields(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sName[64];
	mg_http_get_var(&hm->query, "name", sName, sizeof(sName));
	if ( strlen(sName) == 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"缺少模型名称\"}", 0);
		return;
	}
	
	ModelInstance* pModel = ModelMgr_GetModel(sName);
	
	if ( !pModel ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"模型不存在\"}", 0);
		return;
	}
	
	xvalue arrFields = pModel->arrFields;
	
	xvalue tblResponse = xvoCreateTable();
	xvoTableSetBool(tblResponse, "result", 6, TRUE);
	if ( arrFields ) {
		xvoAddRef(arrFields);
		xvoTableSetValue(tblResponse, "data", 4, arrFields, TRUE);
	} else {
		xvalue arrEmpty = xvoCreateArray();
		xvoTableSetValue(tblResponse, "data", 4, arrEmpty, TRUE);
	}
	
	size_t iJsonSize = 0;
	str sJson = xrtStringifyJSON(tblResponse, FALSE, &iJsonSize);
	http_reply(c, 200, HTTP_CT_JSON, sJson, iJsonSize);
	xrtFree(sJson);
	
	xvoUnref(tblResponse);
}


// 保存模型字段
void Request_Model_Fields_Save(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode != HTTP_POST ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"不支持的请求方法\"}", 0);
		return;
	}
	
	// 解析JSON
	xvalue tblData = xrtParseJSON(hm->body.buf, hm->body.len);
	if ( !tblData ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"无效的请求数据\"}", 0);
		return;
	}
	
	str sName = xvoTableGetText(tblData, "name", 4);
	if ( !sName ) {
		xvoUnref(tblData);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"缺少模型名称\"}", 0);
		return;
	}
	
	ModelInstance* pModel = ModelMgr_GetModel(sName);
	if ( !pModel ) {
		xvoUnref(tblData);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"模型不存在\"}", 0);
		return;
	}
	
	xvalue arrFields = xvoTableGetValue(tblData, "fields", 6);
	
	// 读取原配置文件
	xvalue tblConfig = xrtParseJSON_File(pModel->sConfigPath);
	if ( !tblConfig ) {
		xvoUnref(tblData);
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"读取配置文件失败\"}", 0);
		return;
	}
	
	// 更新字段列表
	if ( arrFields ) {
		xvoAddRef(arrFields);
		xvoTableSetValue(tblConfig, "fields", 6, arrFields, TRUE);
	} else {
		xvalue arrEmpty = xvoCreateArray();
		xvoTableSetValue(tblConfig, "fields", 6, arrEmpty, TRUE);
	}
	
	// 更新时间戳
	xvoTableSetInt(tblConfig, "updateTime", 10, xrtNow());
	
	// 标记需要重新编译
	xvalue tblStatus = xvoTableGetValue(tblConfig, "status", 6);
	if ( tblStatus ) {
		xvoTableSetBool(tblStatus, "compiled", 8, FALSE);
	}
	
	// 保存配置
	int iResult = xrtStringifyJSON_File(pModel->sConfigPath, tblConfig, TRUE);
	xvoUnref(tblConfig);
	
	if ( iResult ) {
		// 更新内存中的字段列表
		if ( pModel->arrFields ) {
			xvoUnref(pModel->arrFields);
		}
		if ( arrFields ) {
			xvoAddRef(arrFields);
			pModel->arrFields = arrFields;
		} else {
			pModel->arrFields = NULL;
		}
		pModel->bCompiled = FALSE;
		pModel->iUpdateTime = xrtNow();
	}
	
	xvoUnref(tblData);
	
	if ( iResult ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"保存成功\"}", 0);
	} else {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"保存配置文件失败\"}", 0);
	}
}


// 编译模型
void Request_Model_Compile(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sName[64];
	mg_http_get_var(&hm->query, "name", sName, sizeof(sName));
	if ( strlen(sName) == 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"缺少模型名称\"}", 0);
		return;
	}
	
	ModelInstance* pModel = ModelMgr_GetModel(sName);
	
	if ( !pModel ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"模型不存在\"}", 0);
		return;
	}
	
	// 调用编译函数
	if ( Model_Compile(pModel) ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"编译成功\"}", 0);
	} else {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"编译失败\"}", 0);
	}
}


// 启用模型
void Request_Model_Enable(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sName[64];
	mg_http_get_var(&hm->query, "name", sName, sizeof(sName));
	if ( strlen(sName) == 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"缺少模型名称\"}", 0);
		return;
	}
	
	ModelInstance* pModel = ModelMgr_GetModel(sName);
	
	if ( !pModel ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"模型不存在\"}", 0);
		return;
	}
	
	if ( !pModel->bCompiled ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"模型尚未编译\"}", 0);
		return;
	}
	
	// 调用启用函数
	if ( Model_Enable(pModel) ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"已启用\"}", 0);
	} else {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"启用失败\"}", 0);
	}
}


// 禁用模型
void Request_Model_Disable(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	char sName[64];
	mg_http_get_var(&hm->query, "name", sName, sizeof(sName));
	if ( strlen(sName) == 0 ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"缺少模型名称\"}", 0);
		return;
	}
	
	ModelInstance* pModel = ModelMgr_GetModel(sName);
	
	if ( !pModel ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"模型不存在\"}", 0);
		return;
	}
	
	// 调用禁用函数
	if ( Model_Disable(pModel) ) {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":true,\"message\":\"已禁用\"}", 0);
	} else {
		http_reply(c, 200, HTTP_CT_JSON, "{\"result\":false,\"message\":\"禁用失败\"}", 0);
	}
}

