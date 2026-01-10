


// ============================================
// 模型管理器
// ============================================



// ==================== 数据结构定义 ====================

// 模型实例结构体
typedef struct {
	
	// ===== 基础信息 =====
	str sName;					// 模型标识（英文，目录名）
	str sTitle;					// 模型显示名称
	str sDesc;					// 模型描述
	str sNamespace;				// 命名空间（API路径前缀）
	str sIcon;					// 图标
	str sTableName;				// 数据表名（model_{namespace}_{name}）
	str sConfigPath;			// 配置文件路径
	str sCodePath;				// 生成的代码路径
	
	// ===== 状态 =====
	bool bEnabled;				// 是否启用
	bool bCompiled;				// 是否已编译
	
	// ===== TCC状态机 =====
	TCCState* pTccState;		// TCC编译状态
	
	// ===== 路由列表 =====
	xlist lstRoutes;			// 该模型注册的所有路由URI
	
	// ===== 功能开关 =====
	bool bEnableApi;			// 启用前台API
	bool bEnableAdmin;			// 启用后台管理
	bool bEnableSubmit;			// 启用前台投稿
	bool bEnableReply;			// 启用评论功能
	
	// ===== 权限配置 =====
	int iApiAuthLevel;			// 前台API权限级别
	int iApiAuthId;				// 前台API权限组ID
	int iAdminAuthId;			// 后台管理权限组ID
	
	// ===== 菜单配置 =====
	int iMenuParent;			// 菜单父级ID
	int iMenuSort;				// 菜单排序
	
	// ===== 评论配置 =====
	bool bReplyNeedApprove;		// 评论需审核
	int iReplyAuthLevel;		// 评论所需权限级别
	int iReplyQuoteMaxLen;		// 引用摘要最大长度
	
	// ===== 字段列表 =====
	xvalue arrFields;			// 字段配置数组
	
	// ===== 时间戳 =====
	int64 iCreateTime;
	int64 iUpdateTime;
	
} ModelInstance;



// 模型管理器结构体
typedef struct {
	
	xdict tblModels;			// 模型实例表（key: name）
	xdict tblNamespaces;		// 命名空间占用表（key: namespace/name，用于唯一性检查）
	xlist lstEnabledModels;		// 已启用的模型列表
	
} ModelManager;



// 全局模型管理器
ModelManager* G_ModelMgr = NULL;



// ==================== 模型上下文接口 ====================

// 暴露给TCC状态机的接口结构
typedef struct {
	
	// ===== 数据库 =====
	XDO_Connect pDB;						// 全局数据库对象
	
	// ===== 路由操作 =====
	RouteInfo* (*AddRoute)(str uri, void* proc, bool bAuth, bool bAdmin, int authId, int authLevel);
	void (*RemoveRoute)(str uri);
	
	// ===== HTTP响应 =====
	void (*SendJson)(struct mg_connection* c, int code, str json);
	void (*SendHtml)(struct mg_connection* c, int code, str html);
	void (*SendPage)(struct mg_connection* c, str pagePath, xvalue data);
	
	// ===== JSON操作 =====
	xvalue (*JsonParse)(str json);
	str (*JsonStringify)(xvalue val);
	void (*JsonFree)(xvalue val);
	
	// ===== 会话操作 =====
	xvalue (*GetAdminSession)(str token);
	xvalue (*GetMemberSession)(str token);
	
	// ===== 工具函数 =====
	int64 (*TimeNow)();
	str (*Format)(str fmt, ...);
	void (*Free)(void* ptr);
	
	// ===== 日志 =====
	void (*Log)(str format, ...);
	
} ModelContext;



// 全局模型上下文
ModelContext* G_ModelCtx = NULL;



// ==================== 动态路由管理 ====================

// 动态添加路由（返回 RouteInfo* 以便进一步配置）
RouteInfo* Model_AddRoute(str uri, void* proc, bool bAuth, bool bAdmin, int authId, int authLevel)
{
	RouteInfo* pInfo = xrtDictSet(G_StaticRouteTableHTTP, uri, strlen(uri), NULL);
	if ( pInfo ) {
		pInfo->Proc = proc;
		pInfo->bAuth = bAuth;
		pInfo->bAdmin = bAdmin;
		pInfo->bPutLog = FALSE;
		pInfo->bActive = FALSE;
		pInfo->AuthID = authId;
		pInfo->AuthLevel = authLevel;
		printf("        [Model] Add route: %s\n", uri);
		return pInfo;
	} else {
		printf("        [Model] Add route failed: %s\n", uri);
		return NULL;
	}
}


// 模型数据视图处理函数（通用）
void Request_View_Model_Data(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode == HTTP_GET ) {
		LoadPage(c, 200, HTTP_CT_HTML, "model/data.html");
	} else {
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
	}
}

// 模型数据添加页面
void Request_View_Model_Data_Add(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode == HTTP_GET ) {
		LoadPage(c, 200, HTTP_CT_HTML, "model/data_add.html");
	} else {
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
	}
}

// 模型数据编辑页面
void Request_View_Model_Data_Edit(XS_ServerObject objServer, XS_HostObject objHost, struct mg_connection* c, struct mg_http_message* hm)
{
	if ( hm->methodCode == HTTP_GET ) {
		LoadPage(c, 200, HTTP_CT_HTML, "model/data_edit.html");
	} else {
		LoadPage(c, 404, HTTP_CT_HTML, "status/404.html");
	}
}


// 动态移除路由
void Model_RemoveRoute(str uri)
{
	if ( xrtDictRemove(G_StaticRouteTableHTTP, uri, strlen(uri)) ) {
		printf("        [Model] Remove route: %s\n", uri);
	}
}


// 将 URI 添加到数据库 uris 表
bool Model_AddUriToDb(str uri, int authId, bool isBackend)
{
	printf("        [Model] Adding URI to DB: %s (authId=%d, isBackend=%d)\n", uri, authId, isBackend);
	
	// 检查是否已存在
	sqlite3_stmt* stmt_check;
	sqlite3_prepare_v3(G_DB->objDB,
		"SELECT id FROM uris WHERE uri = ?",
		-1, 0, &stmt_check, NULL);
	sqlite3_bind_text(stmt_check, 1, uri, -1, SQLITE_STATIC);
	
	int64 iExistId = 0;
	if ( sqlite3_step(stmt_check) == SQLITE_ROW ) {
		iExistId = sqlite3_column_int64(stmt_check, 0);
	}
	sqlite3_finalize(stmt_check);
	
	xtime now = xrtNow();
	
	if ( iExistId > 0 ) {
		// 已存在，更新 authID 和 isBackend
		sqlite3_stmt* stmt_update;
		sqlite3_prepare_v3(G_DB->objDB,
			"UPDATE uris SET authID = ?, isBackend = ?, needAuth = 1, updateTime = ? WHERE id = ?",
			-1, 0, &stmt_update, NULL);
		sqlite3_bind_int(stmt_update, 1, authId);
		sqlite3_bind_int(stmt_update, 2, isBackend ? 1 : 0);
		sqlite3_bind_int64(stmt_update, 3, now);
		sqlite3_bind_int64(stmt_update, 4, iExistId);
		sqlite3_step(stmt_update);
		sqlite3_finalize(stmt_update);
		printf("        [Model] URI updated in DB: %s\n", uri);
	} else {
		// 不存在，插入新记录
		sqlite3_stmt* stmt_insert;
		sqlite3_prepare_v3(G_DB->objDB,
			"INSERT INTO uris (authID, uri, desc, sort, isBackend, needAuth, needLog, keepActive, createTime, updateTime) "
			"VALUES (?, ?, '', 0, ?, 1, 0, 0, ?, ?)",
			-1, 0, &stmt_insert, NULL);
		sqlite3_bind_int(stmt_insert, 1, authId);
		sqlite3_bind_text(stmt_insert, 2, uri, -1, SQLITE_STATIC);
		sqlite3_bind_int(stmt_insert, 3, isBackend ? 1 : 0);
		sqlite3_bind_int64(stmt_insert, 4, now);
		sqlite3_bind_int64(stmt_insert, 5, now);
		sqlite3_step(stmt_insert);
		sqlite3_finalize(stmt_insert);
		printf("        [Model] URI inserted to DB: %s\n", uri);
	}
	
	// 同步更新路由表中的 AuthID
	RouteInfo* pInfo = xrtDictGet(G_StaticRouteTableHTTP, uri, strlen(uri));
	if ( pInfo ) {
		pInfo->AuthID = authId;
		pInfo->bAdmin = isBackend;
		pInfo->bAuth = TRUE;
	}
	
	return TRUE;
}


// 从数据库 uris 表删除 URI
bool Model_RemoveUriFromDb(str uri)
{
	printf("        [Model] Removing URI from DB: %s\n", uri);
	
	sqlite3_stmt* stmt_del;
	sqlite3_prepare_v3(G_DB->objDB,
		"DELETE FROM uris WHERE uri = ?",
		-1, 0, &stmt_del, NULL);
	sqlite3_bind_text(stmt_del, 1, uri, -1, SQLITE_STATIC);
	sqlite3_step(stmt_del);
	sqlite3_finalize(stmt_del);
	
	return TRUE;
}


// 刷新权限缓存（声明外部函数）
extern void Auth_ReloadCache();
extern void ReloadCache_Auth_Auth();
extern void ReloadCache_Auth_Group();



// ==================== 命名空间管理 ====================

// 生成命名空间键
str Model_MakeNamespaceKey(str sNamespace, str sName)
{
	if ( sNamespace && strlen(sNamespace) > 0 ) {
		return xrtFormat("%s/%s", sNamespace, sName);
	} else {
		return xrtCopyStr(sName, 0);
	}
}


// 检查命名空间是否可用
bool Model_CheckNamespace(str sNamespace, str sName, str sExcludeName)
{
	str sKey = Model_MakeNamespaceKey(sNamespace, sName);
	
	// 排除当前正在编辑的模型
	if ( sExcludeName ) {
		ModelInstance* pExclude = xrtDictGet(G_ModelMgr->tblModels, sExcludeName, strlen(sExcludeName));
		if ( pExclude ) {
			str sExcludeKey = Model_MakeNamespaceKey(pExclude->sNamespace, pExclude->sName);
			if ( strcmp(sKey, sExcludeKey) == 0 ) {
				xrtFree(sKey);
				xrtFree(sExcludeKey);
				return TRUE;  // 是自己，可用
			}
			xrtFree(sExcludeKey);
		}
	}
	
	bool bExists = (xrtDictGet(G_ModelMgr->tblNamespaces, sKey, strlen(sKey)) != NULL);
	xrtFree(sKey);
	
	return !bExists;  // 不存在则可用
}


// 注册命名空间
void Model_RegisterNamespace(ModelInstance* pModel)
{
	str sKey = Model_MakeNamespaceKey(pModel->sNamespace, pModel->sName);
	xrtDictSet(G_ModelMgr->tblNamespaces, sKey, strlen(sKey), NULL);
	xrtFree(sKey);
}


// 注销命名空间
void Model_UnregisterNamespace(ModelInstance* pModel)
{
	str sKey = Model_MakeNamespaceKey(pModel->sNamespace, pModel->sName);
	xrtDictRemove(G_ModelMgr->tblNamespaces, sKey, strlen(sKey));
	xrtFree(sKey);
}



// ==================== 上下文接口实现 ====================

// JSON解析包装
xvalue ModelCtx_JsonParse(str json)
{
	return xrtParseJSON(json, strlen(json));
}

// JSON序列化包装
str ModelCtx_JsonStringify(xvalue val)
{
	return xrtStringifyJSON(val, FALSE, NULL);
}

// JSON释放包装
void ModelCtx_JsonFree(xvalue val)
{
	xvoUnref(val);
}

// 获取后台Session
xvalue ModelCtx_GetAdminSession(str token)
{
	return xvoTableGetValue(G_AdminSession, token, strlen(token));
}

// 获取前台Session
xvalue ModelCtx_GetMemberSession(str token)
{
	return xvoTableGetValue(G_MemberSession, token, strlen(token));
}

// 获取当前时间戳
int64 ModelCtx_TimeNow()
{
	return xrtNow();
}

// 日志输出
void ModelCtx_Log(str format, ...)
{
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
	printf("\n");
}



// ==================== 模型实例管理 ====================

// 创建模型实例
ModelInstance* Model_Create(str sName)
{
	ModelInstance* pModel = xrtMalloc(sizeof(ModelInstance));
	memset(pModel, 0, sizeof(ModelInstance));
	
	pModel->sName = xrtCopyStr(sName, 0);
	pModel->lstRoutes = xrtListCreate(sizeof(ptr));
	pModel->arrFields = NULL;
	pModel->pTccState = NULL;
	pModel->bEnabled = FALSE;
	pModel->bCompiled = FALSE;
	
	// 设置默认值
	pModel->bEnableApi = TRUE;
	pModel->bEnableAdmin = TRUE;
	pModel->bEnableSubmit = FALSE;
	pModel->bEnableReply = FALSE;
	pModel->iReplyQuoteMaxLen = 50;
	
	return pModel;
}


// 销毁模型实例
void Model_Destroy(ModelInstance* pModel)
{
	if ( !pModel ) return;
	
	// 如果已启用，先禁用
	if ( pModel->bEnabled ) {
		// TODO: Model_Disable(pModel);
	}
	
	// 释放TCC状态机
	if ( pModel->pTccState ) {
		tcc_delete(pModel->pTccState);
		pModel->pTccState = NULL;
	}
	
	// 释放路由列表
	if ( pModel->lstRoutes ) {
		xrtListDestroy(pModel->lstRoutes);
	}
	
	// 释放字段数组
	if ( pModel->arrFields ) {
		xvoUnref(pModel->arrFields);
	}
	
	// 释放字符串
	if ( pModel->sName ) xrtFree(pModel->sName);
	if ( pModel->sTitle ) xrtFree(pModel->sTitle);
	if ( pModel->sDesc ) xrtFree(pModel->sDesc);
	if ( pModel->sNamespace ) xrtFree(pModel->sNamespace);
	if ( pModel->sIcon ) xrtFree(pModel->sIcon);
	if ( pModel->sTableName ) xrtFree(pModel->sTableName);
	if ( pModel->sConfigPath ) xrtFree(pModel->sConfigPath);
	if ( pModel->sCodePath ) xrtFree(pModel->sCodePath);
	
	xrtFree(pModel);
}



// ==================== 代码生成 ====================

// 根据字段类型生成读取代码（列表用，使用 tblRow 和 stmt_*_all）
str Model_GenFieldReadCode(str sFieldName, str sFieldType)
{
	int iNameLen = strlen(sFieldName);
	
	if ( strcmp(sFieldType, "text") == 0 || strcmp(sFieldType, "textarea") == 0 || 
		 strcmp(sFieldType, "richtext") == 0 || strcmp(sFieldType, "select") == 0 ||
		 strcmp(sFieldType, "image") == 0 || strcmp(sFieldType, "file") == 0 ) {
		return xrtFormat("\t\txvoTableSetText(tblRow, \"%s\", %d, (str)sqlite3_column_text(stmt_{{MODEL_NAME}}_all, iCol++), 0, FALSE);\n", 
			sFieldName, iNameLen);
	} else if ( strcmp(sFieldType, "number") == 0 || strcmp(sFieldType, "switch") == 0 ) {
		return xrtFormat("\t\txvoTableSetInt(tblRow, \"%s\", %d, sqlite3_column_int64(stmt_{{MODEL_NAME}}_all, iCol++));\n", 
			sFieldName, iNameLen);
	} else if ( strcmp(sFieldType, "date") == 0 || strcmp(sFieldType, "datetime") == 0 ) {
		return xrtFormat("\t\t{ xtime iFieldTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_all, iCol++); xvoTableSetText(tblRow, \"%s\", %d, xrtTimeToStr(iFieldTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE); }\n",
			sFieldName, iNameLen);
	} else {
		return xrtFormat("\t\txvoTableSetText(tblRow, \"%s\", %d, (str)sqlite3_column_text(stmt_{{MODEL_NAME}}_all, iCol++), 0, FALSE);\n", 
			sFieldName, iNameLen);
	}
}

// 根据字段类型生成读取代码（详情用，使用 tblData 和 stmt_*_get）
str Model_GenFieldReadCodeGet(str sFieldName, str sFieldType)
{
	int iNameLen = strlen(sFieldName);
	
	if ( strcmp(sFieldType, "text") == 0 || strcmp(sFieldType, "textarea") == 0 || 
		 strcmp(sFieldType, "richtext") == 0 || strcmp(sFieldType, "select") == 0 ||
		 strcmp(sFieldType, "image") == 0 || strcmp(sFieldType, "file") == 0 ) {
		return xrtFormat("\t\txvoTableSetText(tblData, \"%s\", %d, (str)sqlite3_column_text(stmt_{{MODEL_NAME}}_get, iCol++), 0, FALSE);\n", 
			sFieldName, iNameLen);
	} else if ( strcmp(sFieldType, "number") == 0 || strcmp(sFieldType, "switch") == 0 ) {
		return xrtFormat("\t\txvoTableSetInt(tblData, \"%s\", %d, sqlite3_column_int64(stmt_{{MODEL_NAME}}_get, iCol++));\n", 
			sFieldName, iNameLen);
	} else if ( strcmp(sFieldType, "date") == 0 || strcmp(sFieldType, "datetime") == 0 ) {
		return xrtFormat("\t\t{ xtime iFieldTime = sqlite3_column_int64(stmt_{{MODEL_NAME}}_get, iCol++); xvoTableSetText(tblData, \"%s\", %d, xrtTimeToStr(iFieldTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE); }\n",
			sFieldName, iNameLen);
	} else {
		return xrtFormat("\t\txvoTableSetText(tblData, \"%s\", %d, (str)sqlite3_column_text(stmt_{{MODEL_NAME}}_get, iCol++), 0, FALSE);\n", 
			sFieldName, iNameLen);
	}
}

// 根据字段类型生成绑定代码（添加）
str Model_GenFieldBindAddCode(str sFieldName, str sFieldType)
{
	int iNameLen = strlen(sFieldName);
	
	if ( strcmp(sFieldType, "text") == 0 || strcmp(sFieldType, "textarea") == 0 || 
		 strcmp(sFieldType, "richtext") == 0 || strcmp(sFieldType, "select") == 0 ||
		 strcmp(sFieldType, "image") == 0 || strcmp(sFieldType, "file") == 0 ) {
		return xrtFormat("\t{ str sVal = xvoTableGetText(tblForm, \"%s\", %d); sqlite3_bind_text(stmt_{{MODEL_NAME}}_add, iIdx++, sVal ? sVal : (str)\"\", -1, NULL); }\n",
			sFieldName, iNameLen);
	} else if ( strcmp(sFieldType, "number") == 0 || strcmp(sFieldType, "switch") == 0 ) {
		return xrtFormat("\tsqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, xvoTableGetInt(tblForm, \"%s\", %d));\n",
			sFieldName, iNameLen);
	} else if ( strcmp(sFieldType, "date") == 0 || strcmp(sFieldType, "datetime") == 0 ) {
		return xrtFormat("\t{ str sVal = xvoTableGetText(tblForm, \"%s\", %d); sqlite3_bind_int64(stmt_{{MODEL_NAME}}_add, iIdx++, sVal ? xrtStrToTime(sVal, 0) : 0); }\n",
			sFieldName, iNameLen);
	} else {
		return xrtFormat("\t{ str sVal = xvoTableGetText(tblForm, \"%s\", %d); sqlite3_bind_text(stmt_{{MODEL_NAME}}_add, iIdx++, sVal ? sVal : (str)\"\", -1, NULL); }\n",
			sFieldName, iNameLen);
	}
}

// 根据字段类型生成绑定代码（更新）
str Model_GenFieldBindUpdateCode(str sFieldName, str sFieldType)
{
	int iNameLen = strlen(sFieldName);
	
	if ( strcmp(sFieldType, "text") == 0 || strcmp(sFieldType, "textarea") == 0 || 
		 strcmp(sFieldType, "richtext") == 0 || strcmp(sFieldType, "select") == 0 ||
		 strcmp(sFieldType, "image") == 0 || strcmp(sFieldType, "file") == 0 ) {
		return xrtFormat("\t{ str sVal = xvoTableGetText(tblForm, \"%s\", %d); sqlite3_bind_text(stmt_{{MODEL_NAME}}_put, iIdx++, sVal ? sVal : (str)\"\", -1, NULL); }\n",
			sFieldName, iNameLen);
	} else if ( strcmp(sFieldType, "number") == 0 || strcmp(sFieldType, "switch") == 0 ) {
		return xrtFormat("\tsqlite3_bind_int64(stmt_{{MODEL_NAME}}_put, iIdx++, xvoTableGetInt(tblForm, \"%s\", %d));\n",
			sFieldName, iNameLen);
	} else if ( strcmp(sFieldType, "date") == 0 || strcmp(sFieldType, "datetime") == 0 ) {
		return xrtFormat("\t{ str sVal = xvoTableGetText(tblForm, \"%s\", %d); sqlite3_bind_int64(stmt_{{MODEL_NAME}}_put, iIdx++, sVal ? xrtStrToTime(sVal, 0) : 0); }\n",
			sFieldName, iNameLen);
	} else {
		return xrtFormat("\t{ str sVal = xvoTableGetText(tblForm, \"%s\", %d); sqlite3_bind_text(stmt_{{MODEL_NAME}}_put, iIdx++, sVal ? sVal : (str)\"\", -1, NULL); }\n",
			sFieldName, iNameLen);
	}
}

// 生成模型代码
bool Model_GenerateCode(ModelInstance* pModel)
{
	printf("        [Model] Generating code for %s...\n", pModel->sName);
	
	// 读取模板文件
	str sTplPath = xrtPathJoin(2, ModelTemplatePath, "model_code.tpl");
	str sTemplate = xrtFileReadAll(sTplPath, XRT_CP_UTF8, NULL);
	xrtFree(sTplPath);
	
	if ( !sTemplate ) {
		printf("        [Model] Failed to read template file\n");
		return FALSE;
	}
	
	// 构建字段名称列表和占位符
	str sFieldNames = xrtCopyStr("", 0);
	str sFieldPlaceholders = xrtCopyStr("", 0);
	str sFieldUpdateSet = xrtCopyStr("", 0);
	str sFieldReadCode = xrtCopyStr("", 0);
	str sFieldReadCodeGet = xrtCopyStr("", 0);
	str sFieldBindAddCode = xrtCopyStr("", 0);
	str sFieldBindUpdateCode = xrtCopyStr("", 0);
	
	xvalue arrFields = pModel->arrFields;
	int iFieldCount = arrFields ? xvoArrayItemCount(arrFields) : 0;
	
	for ( int i = 0; i < iFieldCount; i++ ) {
		xvalue tblField = xvoArrayGetValue(arrFields, i);
		str sFieldName = xvoTableGetText(tblField, "name", 4);
		str sFieldType = xvoTableGetText(tblField, "type", 4);
		if ( !sFieldName || !sFieldType ) continue;
		
		// 字段名称列表
		if ( i > 0 ) {
			str sTemp = xrtFormat("%s, %s", sFieldNames, sFieldName);
			xrtFree(sFieldNames);
			sFieldNames = sTemp;
		} else {
			xrtFree(sFieldNames);
			sFieldNames = xrtCopyStr(sFieldName, 0);
		}
		
		// 占位符
		if ( i > 0 ) {
			str sTemp = xrtFormat("%s, ?", sFieldPlaceholders);
			xrtFree(sFieldPlaceholders);
			sFieldPlaceholders = sTemp;
		} else {
			xrtFree(sFieldPlaceholders);
			sFieldPlaceholders = xrtCopyStr("?", 0);
		}
		
		// UPDATE SET
		if ( i > 0 ) {
			str sTemp = xrtFormat("%s, %s = ?", sFieldUpdateSet, sFieldName);
			xrtFree(sFieldUpdateSet);
			sFieldUpdateSet = sTemp;
		} else {
			str sTemp = xrtFormat("%s = ?", sFieldName);
			xrtFree(sFieldUpdateSet);
			sFieldUpdateSet = sTemp;
		}
		
		// 读取代码（列表用）
		str sReadCode = Model_GenFieldReadCode(sFieldName, sFieldType);
		str sTemp = xrtFormat("%s%s", sFieldReadCode, sReadCode);
		xrtFree(sFieldReadCode);
		sFieldReadCode = sTemp;
		xrtFree(sReadCode);
		
		// 读取代码（详情用）
		str sReadCodeGet = Model_GenFieldReadCodeGet(sFieldName, sFieldType);
		sTemp = xrtFormat("%s%s", sFieldReadCodeGet, sReadCodeGet);
		xrtFree(sFieldReadCodeGet);
		sFieldReadCodeGet = sTemp;
		xrtFree(sReadCodeGet);
		
		// 添加绑定代码
		str sBindAddCode = Model_GenFieldBindAddCode(sFieldName, sFieldType);
		sTemp = xrtFormat("%s%s", sFieldBindAddCode, sBindAddCode);
		xrtFree(sFieldBindAddCode);
		sFieldBindAddCode = sTemp;
		xrtFree(sBindAddCode);
		
		// 更新绑定代码
		str sBindUpdateCode = Model_GenFieldBindUpdateCode(sFieldName, sFieldType);
		sTemp = xrtFormat("%s%s", sFieldBindUpdateCode, sBindUpdateCode);
		xrtFree(sFieldBindUpdateCode);
		sFieldBindUpdateCode = sTemp;
		xrtFree(sBindUpdateCode);
	}
	
	// 如果没有字段，设置默认值
	if ( iFieldCount == 0 ) {
		xrtFree(sFieldNames);
		sFieldNames = xrtCopyStr("id", 0);  // 至少有id字段
		xrtFree(sFieldPlaceholders);
		sFieldPlaceholders = xrtCopyStr("", 0);
		xrtFree(sFieldUpdateSet);
		sFieldUpdateSet = xrtCopyStr("id = id", 0);  // 空操作
	}
	
	// 命名空间路径
	str sNamespacePath = pModel->sNamespace && strlen(pModel->sNamespace) > 0 
		? xrtFormat("%s/", pModel->sNamespace) 
		: xrtCopyStr("", 0);
	
	// 生成时间
	xtime now = xrtNow();
	str sGenTime = xrtTimeToStr(now, XRT_TIME_FORMAT_DATETIME);
	
	// 替换模板变量
	str sCode = sTemplate;
	
	// 基础替换
	str sTemp = xrtReplace(sCode, 0, "{{MODEL_NAME}}", 0, pModel->sName, 0, NULL);
	xrtFree(sCode); sCode = sTemp;
	
	sTemp = xrtReplace(sCode, 0, "{{MODEL_TITLE}}", 0, pModel->sTitle ? pModel->sTitle : pModel->sName, 0, NULL);
	xrtFree(sCode); sCode = sTemp;
	
	sTemp = xrtReplace(sCode, 0, "{{TABLE_NAME}}", 0, pModel->sTableName ? pModel->sTableName : pModel->sName, 0, NULL);
	xrtFree(sCode); sCode = sTemp;
	
	sTemp = xrtReplace(sCode, 0, "{{NAMESPACE_PATH}}", 0, sNamespacePath, 0, NULL);
	xrtFree(sCode); sCode = sTemp;
	
	sTemp = xrtReplace(sCode, 0, "{{GEN_TIME}}", 0, sGenTime, 0, NULL);
	xrtFree(sCode); sCode = sTemp;
	
	// 字段相关替换
	sTemp = xrtReplace(sCode, 0, "{{FIELD_NAMES}}", 0, sFieldNames, 0, NULL);
	xrtFree(sCode); sCode = sTemp;
	
	sTemp = xrtReplace(sCode, 0, "{{FIELD_PLACEHOLDERS}}", 0, sFieldPlaceholders, 0, NULL);
	xrtFree(sCode); sCode = sTemp;
	
	sTemp = xrtReplace(sCode, 0, "{{FIELD_UPDATE_SET}}", 0, sFieldUpdateSet, 0, NULL);
	xrtFree(sCode); sCode = sTemp;
	
	sTemp = xrtReplace(sCode, 0, "{{FIELD_READ_CODE}}", 0, sFieldReadCode, 0, NULL);
	xrtFree(sCode); sCode = sTemp;
	
	sTemp = xrtReplace(sCode, 0, "{{FIELD_READ_CODE_GET}}", 0, sFieldReadCodeGet, 0, NULL);
	xrtFree(sCode); sCode = sTemp;
	
	sTemp = xrtReplace(sCode, 0, "{{FIELD_BIND_ADD_CODE}}", 0, sFieldBindAddCode, 0, NULL);
	xrtFree(sCode); sCode = sTemp;
	
	sTemp = xrtReplace(sCode, 0, "{{FIELD_BIND_UPDATE_CODE}}", 0, sFieldBindUpdateCode, 0, NULL);
	xrtFree(sCode); sCode = sTemp;
	
	// 第二次 MODEL_NAME 替换（处理字段代码中的模板变量）
	sTemp = xrtReplace(sCode, 0, "{{MODEL_NAME}}", 0, pModel->sName, 0, NULL);
	xrtFree(sCode); sCode = sTemp;
	
	// 权限配置
	char sAuthLevel[16];
	sprintf(sAuthLevel, "%d", pModel->iApiAuthLevel);
	sTemp = xrtReplace(sCode, 0, "{{API_AUTH_LEVEL}}", 0, sAuthLevel, 0, NULL);
	xrtFree(sCode); sCode = sTemp;
	
	char sAdminAuthId[16];
	sprintf(sAdminAuthId, "%d", pModel->iAdminAuthId);
	sTemp = xrtReplace(sCode, 0, "{{ADMIN_AUTH_ID}}", 0, sAdminAuthId, 0, NULL);
	xrtFree(sCode); sCode = sTemp;
	
	// 处理条件块
	if ( pModel->bEnableApi ) {
		sTemp = xrtReplace(sCode, 0, "{{#IF_ENABLE_API}}", 0, "", 0, NULL);
		xrtFree(sCode); sCode = sTemp;
		sTemp = xrtReplace(sCode, 0, "{{#ENDIF_ENABLE_API}}", 0, "", 0, NULL);
		xrtFree(sCode); sCode = sTemp;
	} else {
		// 删除整个 API 块（简化处理，只删除标记）
		sTemp = xrtReplace(sCode, 0, "{{#IF_ENABLE_API}}", 0, "/* API DISABLED\n", 0, NULL);
		xrtFree(sCode); sCode = sTemp;
		sTemp = xrtReplace(sCode, 0, "{{#ENDIF_ENABLE_API}}", 0, "*/", 0, NULL);
		xrtFree(sCode); sCode = sTemp;
	}
	
	if ( pModel->bEnableSubmit ) {
		sTemp = xrtReplace(sCode, 0, "{{#IF_ENABLE_SUBMIT}}", 0, "", 0, NULL);
		xrtFree(sCode); sCode = sTemp;
		sTemp = xrtReplace(sCode, 0, "{{#ENDIF_ENABLE_SUBMIT}}", 0, "", 0, NULL);
		xrtFree(sCode); sCode = sTemp;
	} else {
		sTemp = xrtReplace(sCode, 0, "{{#IF_ENABLE_SUBMIT}}", 0, "/* SUBMIT DISABLED\n", 0, NULL);
		xrtFree(sCode); sCode = sTemp;
		sTemp = xrtReplace(sCode, 0, "{{#ENDIF_ENABLE_SUBMIT}}", 0, "*/", 0, NULL);
		xrtFree(sCode); sCode = sTemp;
	}
	
	if ( pModel->bEnableAdmin ) {
		sTemp = xrtReplace(sCode, 0, "{{#IF_ENABLE_ADMIN}}", 0, "", 0, NULL);
		xrtFree(sCode); sCode = sTemp;
		sTemp = xrtReplace(sCode, 0, "{{#ENDIF_ENABLE_ADMIN}}", 0, "", 0, NULL);
		xrtFree(sCode); sCode = sTemp;
	} else {
		sTemp = xrtReplace(sCode, 0, "{{#IF_ENABLE_ADMIN}}", 0, "/* ADMIN DISABLED\n", 0, NULL);
		xrtFree(sCode); sCode = sTemp;
		sTemp = xrtReplace(sCode, 0, "{{#ENDIF_ENABLE_ADMIN}}", 0, "*/", 0, NULL);
		xrtFree(sCode); sCode = sTemp;
	}
	
	// 评论功能
	if ( pModel->bEnableReply ) {
		sTemp = xrtReplace(sCode, 0, "{{#IF_ENABLE_REPLY}}", 0, "", 0, NULL);
		xrtFree(sCode); sCode = sTemp;
		sTemp = xrtReplace(sCode, 0, "{{#ENDIF_ENABLE_REPLY}}", 0, "", 0, NULL);
		xrtFree(sCode); sCode = sTemp;
		
		// 评论配置参数
		char sReplyAuthLevel[16];
		sprintf(sReplyAuthLevel, "%d", pModel->iReplyAuthLevel);
		sTemp = xrtReplace(sCode, 0, "{{REPLY_AUTH_LEVEL}}", 0, sReplyAuthLevel, 0, NULL);
		xrtFree(sCode); sCode = sTemp;
		
		char sReplyQuoteMaxLen[16];
		sprintf(sReplyQuoteMaxLen, "%d", pModel->iReplyQuoteMaxLen > 0 ? pModel->iReplyQuoteMaxLen : 50);
		sTemp = xrtReplace(sCode, 0, "{{REPLY_QUOTE_MAX_LEN}}", 0, sReplyQuoteMaxLen, 0, NULL);
		xrtFree(sCode); sCode = sTemp;
		
		sTemp = xrtReplace(sCode, 0, "{{REPLY_NEED_APPROVE}}", 0, pModel->bReplyNeedApprove ? "1" : "0", 0, NULL);
		xrtFree(sCode); sCode = sTemp;
	} else {
		sTemp = xrtReplace(sCode, 0, "{{#IF_ENABLE_REPLY}}", 0, "/* REPLY DISABLED\n", 0, NULL);
		xrtFree(sCode); sCode = sTemp;
		sTemp = xrtReplace(sCode, 0, "{{#ENDIF_ENABLE_REPLY}}", 0, "*/", 0, NULL);
		xrtFree(sCode); sCode = sTemp;
	}
	
	// 保存生成的代码
	str sCodePath = xrtFormat("%s/%s/code.h", ModelPath, pModel->sName);
	
	// 确保目录存在
	str sModelDir = xrtFormat("%s/%s", ModelPath, pModel->sName);
	xrtDirCreate(sModelDir);
	xrtFree(sModelDir);
	
	bool bResult = xrtFilePutAll(sCodePath, sCode, strlen(sCode));
	
	if ( bResult ) {
		if ( pModel->sCodePath ) xrtFree(pModel->sCodePath);
		pModel->sCodePath = sCodePath;
		printf("        [Model] Code generated: %s\n", sCodePath);
	} else {
		xrtFree(sCodePath);
		printf("        [Model] Failed to save code file\n");
	}
	
	// 清理
	xrtFree(sCode);
	xrtFree(sFieldNames);
	xrtFree(sFieldPlaceholders);
	xrtFree(sFieldUpdateSet);
	xrtFree(sFieldReadCode);
	xrtFree(sFieldReadCodeGet);
	xrtFree(sFieldBindAddCode);
	xrtFree(sFieldBindUpdateCode);
	xrtFree(sNamespacePath);
	xrtFree(sGenTime);
	
	return bResult;
}


// 根据字段类型获取SQLite字段类型
str Model_GetSqliteType(str sFieldType)
{
	if ( strcmp(sFieldType, "number") == 0 || strcmp(sFieldType, "switch") == 0 ||
		 strcmp(sFieldType, "date") == 0 || strcmp(sFieldType, "datetime") == 0 ) {
		return "INTEGER";
	}
	return "TEXT";
}


// 检查表是否存在
bool Model_TableExists(str sTableName)
{
	sqlite3_stmt* stmt;
	sqlite3_prepare_v3(G_DB->objDB,
		"SELECT name FROM sqlite_master WHERE type='table' AND name=?",
		-1, 0, &stmt, NULL);
	sqlite3_bind_text(stmt, 1, sTableName, -1, SQLITE_STATIC);
	bool bExists = (sqlite3_step(stmt) == SQLITE_ROW);
	sqlite3_finalize(stmt);
	return bExists;
}


// 获取表的现有列信息（返回列名数组）
xvalue Model_GetTableColumns(str sTableName)
{
	xvalue arrColumns = xvoCreateArray();
	
	str sSQL = xrtFormat("PRAGMA table_info(%s)", sTableName);
	sqlite3_stmt* stmt;
	if ( sqlite3_prepare_v3(G_DB->objDB, sSQL, -1, 0, &stmt, NULL) == SQLITE_OK ) {
		while ( sqlite3_step(stmt) == SQLITE_ROW ) {
			str sColName = (str)sqlite3_column_text(stmt, 1);  // 第2列是列名
			if ( sColName ) {
				xvalue tblCol = xvoCreateTable();
				xvoTableSetText(tblCol, "name", 4, sColName, 0, FALSE);
				xvoArrayAppendValue(arrColumns, tblCol, TRUE);
			}
		}
		sqlite3_finalize(stmt);
	}
	xrtFree(sSQL);
	
	return arrColumns;
}


// 检查列名是否在数组中存在
bool Model_ColumnInArray(xvalue arrColumns, str sColName)
{
	int iCount = xvoArrayItemCount(arrColumns);
	for ( int i = 0; i < iCount; i++ ) {
		xvalue tblCol = xvoArrayGetValue(arrColumns, i);
		str sName = xvoTableGetText(tblCol, "name", 4);
		if ( sName && strcmp(sName, sColName) == 0 ) {
			return TRUE;
		}
	}
	return FALSE;
}


// 获取模型定义的所有列名（包含系统列）
xvalue Model_GetModelColumns(ModelInstance* pModel)
{
	xvalue arrColumns = xvoCreateArray();
	
	// 添加系统列
	xvalue tblId = xvoCreateTable();
	xvoTableSetText(tblId, "name", 4, "id", 0, FALSE);
	xvoArrayAppendValue(arrColumns, tblId, TRUE);
	
	// 添加模型字段
	xvalue arrFields = pModel->arrFields;
	int iFieldCount = arrFields ? xvoArrayItemCount(arrFields) : 0;
	for ( int i = 0; i < iFieldCount; i++ ) {
		xvalue tblField = xvoArrayGetValue(arrFields, i);
		str sFieldName = xvoTableGetText(tblField, "name", 4);
		if ( sFieldName ) {
			xvalue tblCol = xvoCreateTable();
			xvoTableSetText(tblCol, "name", 4, sFieldName, 0, FALSE);
			xvoArrayAppendValue(arrColumns, tblCol, TRUE);
		}
	}
	
	// 添加系统列
	xvalue tblCreate = xvoCreateTable();
	xvoTableSetText(tblCreate, "name", 4, "createTime", 0, FALSE);
	xvoArrayAppendValue(arrColumns, tblCreate, TRUE);
	
	xvalue tblUpdate = xvoCreateTable();
	xvoTableSetText(tblUpdate, "name", 4, "updateTime", 0, FALSE);
	xvoArrayAppendValue(arrColumns, tblUpdate, TRUE);
	
	xvalue tblDel = xvoCreateTable();
	xvoTableSetText(tblDel, "name", 4, "isDelete", 0, FALSE);
	xvoArrayAppendValue(arrColumns, tblDel, TRUE);
	
	return arrColumns;
}


// 检查表结构是否需要迁移
bool Model_NeedsMigration(xvalue arrTableCols, xvalue arrModelCols)
{
	int iTableCount = xvoArrayItemCount(arrTableCols);
	int iModelCount = xvoArrayItemCount(arrModelCols);
	
	// 列数不同，需要迁移
	if ( iTableCount != iModelCount ) {
		return TRUE;
	}
	
	// 检查表中每一列是否在模型中存在
	for ( int i = 0; i < iTableCount; i++ ) {
		xvalue tblCol = xvoArrayGetValue(arrTableCols, i);
		str sName = xvoTableGetText(tblCol, "name", 4);
		if ( !Model_ColumnInArray(arrModelCols, sName) ) {
			return TRUE;  // 表中有模型没有的列
		}
	}
	
	// 检查模型中每一列是否在表中存在
	for ( int i = 0; i < iModelCount; i++ ) {
		xvalue tblCol = xvoArrayGetValue(arrModelCols, i);
		str sName = xvoTableGetText(tblCol, "name", 4);
		if ( !Model_ColumnInArray(arrTableCols, sName) ) {
			return TRUE;  // 模型中有表没有的列
		}
	}
	
	return FALSE;
}


// 执行表结构迁移
bool Model_MigrateTable(ModelInstance* pModel, xvalue arrTableCols, xvalue arrModelCols)
{
	printf("        [Model] Migrating table structure for %s...\n", pModel->sName);
	
	str sTableName = pModel->sTableName;
	str sTempTable = xrtFormat("%s_migrate_temp", sTableName);
	char* sErr = NULL;
	int iResult;
	
	// 1. 构建新表的列定义
	str sColumns = xrtCopyStr("", 0);
	xvalue arrFields = pModel->arrFields;
	int iFieldCount = arrFields ? xvoArrayItemCount(arrFields) : 0;
	
	for ( int i = 0; i < iFieldCount; i++ ) {
		xvalue tblField = xvoArrayGetValue(arrFields, i);
		str sFieldName = xvoTableGetText(tblField, "name", 4);
		str sFieldType = xvoTableGetText(tblField, "type", 4);
		if ( !sFieldName || !sFieldType ) continue;
		
		str sSqlType = Model_GetSqliteType(sFieldType);
		str sTemp = xrtFormat("%s,\n    %s %s", sColumns, sFieldName, sSqlType);
		xrtFree(sColumns);
		sColumns = sTemp;
	}
	
	// 2. 创建临时表（新结构）
	str sCreateSQL = xrtFormat(
		"CREATE TABLE %s (\n"
		"    id INTEGER PRIMARY KEY AUTOINCREMENT%s,\n"
		"    createTime INTEGER,\n"
		"    updateTime INTEGER,\n"
		"    isDelete INTEGER DEFAULT 0\n"
		")",
		sTempTable, sColumns
	);
	xrtFree(sColumns);
	
	printf("        [Model] Creating temp table: %s\n", sTempTable);
	iResult = sqlite3_exec(G_DB->objDB, sCreateSQL, NULL, NULL, &sErr);
	xrtFree(sCreateSQL);
	
	if ( iResult != SQLITE_OK ) {
		printf("        [Model] Failed to create temp table: %s\n", sErr ? sErr : "unknown");
		if ( sErr ) sqlite3_free(sErr);
		xrtFree(sTempTable);
		return FALSE;
	}
	
	// 3. 构建共同列列表（既在旧表又在新表的列）
	str sCommonCols = xrtCopyStr("", 0);
	int iCommonCount = 0;
	int iModelCount = xvoArrayItemCount(arrModelCols);
	
	for ( int i = 0; i < iModelCount; i++ ) {
		xvalue tblCol = xvoArrayGetValue(arrModelCols, i);
		str sColName = xvoTableGetText(tblCol, "name", 4);
		if ( Model_ColumnInArray(arrTableCols, sColName) ) {
			if ( iCommonCount > 0 ) {
				str sTemp = xrtFormat("%s, %s", sCommonCols, sColName);
				xrtFree(sCommonCols);
				sCommonCols = sTemp;
			} else {
				xrtFree(sCommonCols);
				sCommonCols = xrtCopyStr(sColName, 0);
			}
			iCommonCount++;
		}
	}
	
	// 4. 复制数据到临时表
	if ( iCommonCount > 0 ) {
		str sCopySQL = xrtFormat("INSERT INTO %s (%s) SELECT %s FROM %s",
			sTempTable, sCommonCols, sCommonCols, sTableName);
		printf("        [Model] Copying data: %s\n", sCopySQL);
		iResult = sqlite3_exec(G_DB->objDB, sCopySQL, NULL, NULL, &sErr);
		xrtFree(sCopySQL);
		
		if ( iResult != SQLITE_OK ) {
			printf("        [Model] Failed to copy data: %s\n", sErr ? sErr : "unknown");
			if ( sErr ) sqlite3_free(sErr);
			// 清理临时表
			str sDropTemp = xrtFormat("DROP TABLE %s", sTempTable);
			sqlite3_exec(G_DB->objDB, sDropTemp, NULL, NULL, NULL);
			xrtFree(sDropTemp);
			xrtFree(sCommonCols);
			xrtFree(sTempTable);
			return FALSE;
		}
	}
	xrtFree(sCommonCols);
	
	// 5. 删除旧表
	str sDropSQL = xrtFormat("DROP TABLE %s", sTableName);
	printf("        [Model] Dropping old table: %s\n", sTableName);
	iResult = sqlite3_exec(G_DB->objDB, sDropSQL, NULL, NULL, &sErr);
	xrtFree(sDropSQL);
	
	if ( iResult != SQLITE_OK ) {
		printf("        [Model] Failed to drop old table: %s\n", sErr ? sErr : "unknown");
		if ( sErr ) sqlite3_free(sErr);
		xrtFree(sTempTable);
		return FALSE;
	}
	
	// 6. 重命名临时表为原表名
	str sRenameSQL = xrtFormat("ALTER TABLE %s RENAME TO %s", sTempTable, sTableName);
	printf("        [Model] Renaming temp table to: %s\n", sTableName);
	iResult = sqlite3_exec(G_DB->objDB, sRenameSQL, NULL, NULL, &sErr);
	xrtFree(sRenameSQL);
	xrtFree(sTempTable);
	
	if ( iResult != SQLITE_OK ) {
		printf("        [Model] Failed to rename table: %s\n", sErr ? sErr : "unknown");
		if ( sErr ) sqlite3_free(sErr);
		return FALSE;
	}
	
	printf("        [Model] Table migration completed successfully\n");
	return TRUE;
}


// 创建或同步数据库表
bool Model_CreateTable(ModelInstance* pModel)
{
	printf("        [Model] Syncing table for %s...\n", pModel->sName);
	
	if ( !pModel->sTableName ) {
		printf("        [Model] Table name is not set\n");
		return FALSE;
	}
	
	// 检查表是否存在
	if ( !Model_TableExists(pModel->sTableName) ) {
		// 表不存在，直接创建
		printf("        [Model] Table does not exist, creating...\n");
		
		str sColumns = xrtCopyStr("", 0);
		xvalue arrFields = pModel->arrFields;
		int iFieldCount = arrFields ? xvoArrayItemCount(arrFields) : 0;
		
		for ( int i = 0; i < iFieldCount; i++ ) {
			xvalue tblField = xvoArrayGetValue(arrFields, i);
			str sFieldName = xvoTableGetText(tblField, "name", 4);
			str sFieldType = xvoTableGetText(tblField, "type", 4);
			if ( !sFieldName || !sFieldType ) continue;
			
			str sSqlType = Model_GetSqliteType(sFieldType);
			str sTemp = xrtFormat("%s,\n    %s %s", sColumns, sFieldName, sSqlType);
			xrtFree(sColumns);
			sColumns = sTemp;
		}
		
		str sSQL = xrtFormat(
			"CREATE TABLE %s (\n"
			"    id INTEGER PRIMARY KEY AUTOINCREMENT%s,\n"
			"    createTime INTEGER,\n"
			"    updateTime INTEGER,\n"
			"    isDelete INTEGER DEFAULT 0\n"
			")",
			pModel->sTableName, sColumns
		);
		xrtFree(sColumns);
		
		printf("        [Model] SQL: %s\n", sSQL);
		
		char* sErr = NULL;
		int iResult = sqlite3_exec(G_DB->objDB, sSQL, NULL, NULL, &sErr);
		xrtFree(sSQL);
		
		if ( iResult != SQLITE_OK ) {
			printf("        [Model] Failed to create table: %s\n", sErr ? sErr : "unknown error");
			if ( sErr ) sqlite3_free(sErr);
			return FALSE;
		}
		
		printf("        [Model] Table created: %s\n", pModel->sTableName);
		return TRUE;
	}
	
	// 表已存在，检查是否需要迁移
	printf("        [Model] Table exists, checking structure...\n");
	
	xvalue arrTableCols = Model_GetTableColumns(pModel->sTableName);
	xvalue arrModelCols = Model_GetModelColumns(pModel);
	
	if ( Model_NeedsMigration(arrTableCols, arrModelCols) ) {
		printf("        [Model] Structure mismatch, migration needed\n");
		bool bResult = Model_MigrateTable(pModel, arrTableCols, arrModelCols);
		xvoUnref(arrTableCols);
		xvoUnref(arrModelCols);
		return bResult;
	}
	
	printf("        [Model] Table structure is up to date\n");
	xvoUnref(arrTableCols);
	xvoUnref(arrModelCols);
	return TRUE;
}


// 更新 model.h 主引用文件
void Model_UpdateModelHeader()
{
	printf("        [Model] Updating model.h...\n");
	
	str sHeader = xrtCopyStr(
		"\n// ============================================\n"
		"// 模型代码主引用文件\n"
		"// 由模型管理器自动生成和维护\n"
		"// ============================================\n\n"
		"// 此文件在模型启用/禁用时自动更新\n"
		"// 请勿手动修改\n\n"
		"// --- 已启用的模型列表 ---\n",
		0
	);
	
	// 遍历所有已启用的模型
	int iCount = xrtListCount(G_ModelMgr->lstEnabledModels);
	for ( int i = 0; i < iCount; i++ ) {
		ModelInstance* pModel = xrtListGetPtr(G_ModelMgr->lstEnabledModels, i);
		if ( pModel && pModel->sCodePath ) {
			str sTemp = xrtFormat("%s#include \"%s/%s/code.h\"\n", sHeader, ModelPath, pModel->sName);
			xrtFree(sHeader);
			sHeader = sTemp;
		}
	}
	
	if ( iCount == 0 ) {
		str sTemp = xrtFormat("%s// (当前没有启用的模型)\n", sHeader);
		xrtFree(sHeader);
		sHeader = sTemp;
	}
	
	// 保存文件
	str sPath = xrtFormat("%s/model.h", ModelPath);
	xrtFilePutAll(sPath, sHeader, strlen(sHeader));
	xrtFree(sPath);
	xrtFree(sHeader);
}


// 编译模型
bool Model_Compile(ModelInstance* pModel)
{
	printf("        [Model] Compiling %s...\n", pModel->sName);
	
	// 1. 生成代码
	if ( !Model_GenerateCode(pModel) ) {
		return FALSE;
	}
	
	// 2. 创建数据库表
	if ( !Model_CreateTable(pModel) ) {
		return FALSE;
	}
	
	// 3. 更新编译状态
	pModel->bCompiled = TRUE;
	
	// 4. 更新配置文件
	xvalue tblConfig = xrtParseJSON_File(pModel->sConfigPath);
	if ( tblConfig ) {
		xvalue tblStatus = xvoTableGetValue(tblConfig, "status", 6);
		if ( !tblStatus ) {
			tblStatus = xvoCreateTable();
			xvoTableSetValue(tblConfig, "status", 6, tblStatus, TRUE);
		}
		xvoTableSetBool(tblStatus, "compiled", 8, TRUE);
		xvoTableSetInt(tblConfig, "updateTime", 10, xrtNow());
		xrtStringifyJSON_File(pModel->sConfigPath, tblConfig, TRUE);
		xvoUnref(tblConfig);
	}
	
	printf("        [Model] Compile success: %s\n", pModel->sName);
	return TRUE;
}



// ==================== TCC 独立状态机 ====================

// TCC错误回调
void Model_TccErrorFunc(void* opaque, const char* msg)
{
	printf("        [TCC] %s\n", msg);
}

// 使用TCC加载模型代码
bool Model_TccLoad(ModelInstance* pModel)
{
	printf("        [TCC] Loading model: %s\n", pModel->sName);
	
	if ( !pModel->sCodePath ) {
		printf("        [TCC] Code path not set\n");
		return FALSE;
	}
	
	// 读取代码文件
	str sCode = xrtFileReadAll(pModel->sCodePath, XRT_CP_UTF8, NULL);
	if ( !sCode ) {
		printf("        [TCC] Failed to read code file: %s\n", pModel->sCodePath);
		return FALSE;
	}
	
	// 创建TCC状态机（使用 xsCreateTCC 自动配置路径和导入运行时函数）
	TCCState* pTcc = xsCreateTCC(ModelPath);
	if ( !pTcc ) {
		printf("        [TCC] Failed to create TCC state\n");
		xrtFree(sCode);
		return FALSE;
	}
	
	// 设置错误回调
	tcc_set_error_func(pTcc, stderr, Model_TccErrorFunc);
	
	// 编译代码
	if ( tcc_compile_string(pTcc, sCode) < 0 ) {
		printf("        [TCC] Compile failed\n");
		xsDestroyTCC(pTcc);
		xrtFree(sCode);
		return FALSE;
	}
	xrtFree(sCode);
	
	// 注册模型管理器特有的符号
	tcc_add_symbol(pTcc, "Model_AddRoute", Model_AddRoute);
	tcc_add_symbol(pTcc, "Model_RemoveRoute", Model_RemoveRoute);
	tcc_add_symbol(pTcc, "G_ModelCtx", &G_ModelCtx);
	
	// 地址重定向
	if ( tcc_relocate(pTcc) < 0 ) {
		printf("        [TCC] Relocate failed\n");
		xsDestroyTCC(pTcc);
		return FALSE;
	}
	
	// 保存TCC状态机
	pModel->pTccState = pTcc;
	
	// 获取并调用全局数据传递函数（参考 xserver 的 DynLoad_C_GlobalData）
	void (*procSetGlobalData)(int, void*) = tcc_get_symbol(pTcc, "Model_SetGlobalData");
	if ( procSetGlobalData ) {
		procSetGlobalData(1, G_DB);  // 传递数据库连接
	}
	
	// 获取初始化函数
	str sInitFuncName = xrtFormat("Model_%s_Init", pModel->sName);
	void (*procInit)() = tcc_get_symbol(pTcc, sInitFuncName);
	xrtFree(sInitFuncName);
	
	if ( !procInit ) {
		printf("        [TCC] Init function not found\n");
		xsDestroyTCC(pTcc);
		pModel->pTccState = NULL;
		return FALSE;
	}
	
	// 调用初始化函数
	procInit();
	
	printf("        [TCC] Model loaded: %s\n", pModel->sName);
	return TRUE;
}

// 卸载模型TCC状态机
bool Model_TccUnload(ModelInstance* pModel)
{
	printf("        [TCC] Unloading model: %s\n", pModel->sName);
	
	if ( !pModel->pTccState ) {
		return TRUE;  // 已经卸载或未加载
	}
	
	// 获取卸载函数
	str sUnitFuncName = xrtFormat("Model_%s_Unit", pModel->sName);
	void (*procUnit)() = tcc_get_symbol(pModel->pTccState, sUnitFuncName);
	xrtFree(sUnitFuncName);
	
	// 调用卸载函数
	if ( procUnit ) {
		procUnit();
	}
	
	// 释放TCC状态机
	xsDestroyTCC(pModel->pTccState);
	pModel->pTccState = NULL;
	
	printf("        [TCC] Model unloaded: %s\n", pModel->sName);
	return TRUE;
}



// ==================== 菜单与权限管理 ====================

// 为模型创建权限分组
int Model_CreateAuthGroup(ModelInstance* pModel)
{
	printf("        [Model] Creating auth group for %s...\n", pModel->sName);
	
	// 检查是否已存在
	str sAuthName = xrtFormat("模型:%s", pModel->sTitle ? pModel->sTitle : pModel->sName);
	
	sqlite3_stmt* stmt_check;
	sqlite3_prepare_v3(G_DB->objDB,
		"SELECT id FROM auth WHERE name = ? AND isDelete = 0",
		-1, 0, &stmt_check, NULL);
	sqlite3_bind_text(stmt_check, 1, sAuthName, -1, SQLITE_STATIC);
	
	int iAuthId = 0;
	if ( sqlite3_step(stmt_check) == SQLITE_ROW ) {
		iAuthId = sqlite3_column_int(stmt_check, 0);
		printf("        [Model] Auth group already exists: %d\n", iAuthId);
	}
	sqlite3_finalize(stmt_check);
	
	// 如果不存在则创建
	if ( iAuthId == 0 ) {
		xtime now = xrtNow();
		str sAuthDesc = xrtFormat("[%s] 模型管理权限", pModel->sTitle ? pModel->sTitle : pModel->sName);
		
		sqlite3_stmt* stmt_insert;
		sqlite3_prepare_v3(G_DB->objDB,
			"INSERT INTO auth (groupID, name, desc, sort, createTime, updateTime, isDelete) VALUES (1, ?, ?, 900000, ?, ?, 0)",
			-1, 0, &stmt_insert, NULL);
		sqlite3_bind_text(stmt_insert, 1, sAuthName, -1, SQLITE_STATIC);
		sqlite3_bind_text(stmt_insert, 2, sAuthDesc, -1, SQLITE_STATIC);
		sqlite3_bind_int64(stmt_insert, 3, now);
		sqlite3_bind_int64(stmt_insert, 4, now);
		sqlite3_step(stmt_insert);
		iAuthId = sqlite3_last_insert_rowid(G_DB->objDB);
		sqlite3_finalize(stmt_insert);
		
		xrtFree(sAuthDesc);
		printf("        [Model] Auth group created: %d\n", iAuthId);
	}
	
	xrtFree(sAuthName);
	return iAuthId;
}


// 删除模型的权限分组（不再删除，保留权限组以便复用）
void Model_RemoveAuthGroup(ModelInstance* pModel)
{
	// 不再删除权限组，这样用户分配的权限不会因为模型禁用/启用而失效
	// 权限组会在下次启用时被复用
	printf("        [Model] Keeping auth group for %s (will be reused on next enable)\n", pModel->sName);
}


// 为模型创建后台菜单
int Model_CreateMenu(ModelInstance* pModel)
{
	printf("        [Model] Creating menu for %s...\n", pModel->sName);
	
	// 构建菜单href
	str sHref = NULL;
	if ( pModel->sNamespace && strlen(pModel->sNamespace) > 0 ) {
		sHref = xrtFormat("/admin/view/model/data/%s/%s", pModel->sNamespace, pModel->sName);
	} else {
		sHref = xrtFormat("/admin/view/model/data/%s", pModel->sName);
	}
	
	// 检查菜单是否已存在
	sqlite3_stmt* stmt_check;
	sqlite3_prepare_v3(G_DB->objDB,
		"SELECT id FROM menu WHERE href = ? AND isDelete = 0",
		-1, 0, &stmt_check, NULL);
	sqlite3_bind_text(stmt_check, 1, sHref, -1, SQLITE_STATIC);
	
	int iMenuId = 0;
	if ( sqlite3_step(stmt_check) == SQLITE_ROW ) {
		iMenuId = sqlite3_column_int(stmt_check, 0);
		printf("        [Model] Menu already exists: %d\n", iMenuId);
	}
	sqlite3_finalize(stmt_check);
	
	// 如果不存在则创建
	if ( iMenuId == 0 ) {
		xtime now = xrtNow();
		
		sqlite3_stmt* stmt_insert;
		sqlite3_prepare_v3(G_DB->objDB,
			"INSERT INTO menu (parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete) "
			"VALUES (?, ?, ?, 1, '_iframe', ?, ?, 1, ?, ?, ?, 0)",
			-1, 0, &stmt_insert, NULL);
		
		// 父级菜单，默认为顶级菜单
		int iParent = pModel->iMenuParent > 0 ? pModel->iMenuParent : 0;
		sqlite3_bind_int(stmt_insert, 1, iParent);
		
		str sTitle = pModel->sTitle ? pModel->sTitle : pModel->sName;
		sqlite3_bind_text(stmt_insert, 2, sTitle, -1, SQLITE_STATIC);
		
		str sIcon = pModel->sIcon ? pModel->sIcon : (str)"layui-icon layui-icon-file";
		sqlite3_bind_text(stmt_insert, 3, sIcon, -1, SQLITE_STATIC);
		
		sqlite3_bind_text(stmt_insert, 4, sHref, -1, SQLITE_STATIC);
		
		int iSort = pModel->iMenuSort > 0 ? pModel->iMenuSort : 210000;
		sqlite3_bind_int(stmt_insert, 5, iSort);
		
		str sRemark = xrtFormat("%s 模型数据管理", sTitle);
		sqlite3_bind_text(stmt_insert, 6, sRemark, -1, SQLITE_STATIC);
		
		sqlite3_bind_int64(stmt_insert, 7, now);
		sqlite3_bind_int64(stmt_insert, 8, now);
		
		sqlite3_step(stmt_insert);
		iMenuId = sqlite3_last_insert_rowid(G_DB->objDB);
		sqlite3_finalize(stmt_insert);
		
		xrtFree(sRemark);
		printf("        [Model] Menu created: %d\n", iMenuId);
	}
	
	xrtFree(sHref);
	return iMenuId;
}


// 删除模型的后台菜单
void Model_RemoveMenu(ModelInstance* pModel)
{
	printf("        [Model] Removing menu for %s...\n", pModel->sName);
	
	// 构建菜单href
	str sHref = NULL;
	if ( pModel->sNamespace && strlen(pModel->sNamespace) > 0 ) {
		sHref = xrtFormat("/admin/view/model/data/%s/%s", pModel->sNamespace, pModel->sName);
	} else {
		sHref = xrtFormat("/admin/view/model/data/%s", pModel->sName);
	}
	
	sqlite3_stmt* stmt_del;
	sqlite3_prepare_v3(G_DB->objDB,
		"UPDATE menu SET isDelete = 1, updateTime = ? WHERE href = ?",
		-1, 0, &stmt_del, NULL);
	sqlite3_bind_int64(stmt_del, 1, xrtNow());
	sqlite3_bind_text(stmt_del, 2, sHref, -1, SQLITE_STATIC);
	sqlite3_step(stmt_del);
	sqlite3_finalize(stmt_del);
	
	xrtFree(sHref);
}


// 将模型的所有 URI 同步到数据库
void Model_SyncUrisToDb(ModelInstance* pModel)
{
	printf("        [Model] Syncing URIs to DB for %s...\n", pModel->sName);
	
	str sNs = pModel->sNamespace;
	str sName = pModel->sName;
	int iAuthId = pModel->iAdminAuthId;
	
	// 构建 API 前缀
	str sAdminPrefix = NULL;
	str sApiPrefix = NULL;
	if ( sNs && strlen(sNs) > 0 ) {
		sAdminPrefix = xrtFormat("/admin/%s/%s", sNs, sName);
		sApiPrefix = xrtFormat("/api/v1/%s/%s", sNs, sName);
	} else {
		sAdminPrefix = xrtFormat("/admin/%s", sName);
		sApiPrefix = xrtFormat("/api/v1/%s", sName);
	}
	
	str sUri = NULL;
	
	// 后台视图路由
	if ( pModel->bEnableAdmin ) {
		if ( sNs && strlen(sNs) > 0 ) {
			sUri = xrtFormat("/admin/view/model/data/%s/%s", sNs, sName);
		} else {
			sUri = xrtFormat("/admin/view/model/data/%s", sName);
		}
		Model_AddUriToDb(sUri, iAuthId, TRUE);
		xrtFree(sUri);
		
		// 添加和编辑视图路由
		if ( sNs && strlen(sNs) > 0 ) {
			sUri = xrtFormat("/admin/view/model/data/%s/%s/add", sNs, sName);
		} else {
			sUri = xrtFormat("/admin/view/model/data/%s/add", sName);
		}
		Model_AddUriToDb(sUri, iAuthId, TRUE);
		xrtFree(sUri);
		
		if ( sNs && strlen(sNs) > 0 ) {
			sUri = xrtFormat("/admin/view/model/data/%s/%s/edit", sNs, sName);
		} else {
			sUri = xrtFormat("/admin/view/model/data/%s/edit", sName);
		}
		Model_AddUriToDb(sUri, iAuthId, TRUE);
		xrtFree(sUri);
		
		// 后台 API 路由
		sUri = xrtFormat("%s/list", sAdminPrefix);
		Model_AddUriToDb(sUri, iAuthId, TRUE);
		xrtFree(sUri);
		
		sUri = xrtFormat("%s/get", sAdminPrefix);
		Model_AddUriToDb(sUri, iAuthId, TRUE);
		xrtFree(sUri);
		
		sUri = xrtFormat("%s/add", sAdminPrefix);
		Model_AddUriToDb(sUri, iAuthId, TRUE);
		xrtFree(sUri);
		
		sUri = xrtFormat("%s/save", sAdminPrefix);
		Model_AddUriToDb(sUri, iAuthId, TRUE);
		xrtFree(sUri);
		
		sUri = xrtFormat("%s/delete", sAdminPrefix);
		Model_AddUriToDb(sUri, iAuthId, TRUE);
		xrtFree(sUri);
	}
	
	// 前台 API 路由（不需要后台权限，但可能需要前台权限）
	if ( pModel->bEnableApi ) {
		// 前台 API 不需要后台权限，设置 isBackend=FALSE
		sUri = xrtFormat("%s/all", sApiPrefix);
		Model_AddUriToDb(sUri, 0, FALSE);  // 前台接口，无权限组
		xrtFree(sUri);
		
		sUri = xrtFormat("%s/get", sApiPrefix);
		Model_AddUriToDb(sUri, 0, FALSE);
		xrtFree(sUri);
	}
	
	// 前台投稿路由
	if ( pModel->bEnableSubmit ) {
		sUri = xrtFormat("%s/submit", sApiPrefix);
		Model_AddUriToDb(sUri, 0, FALSE);  // 前台接口
		xrtFree(sUri);
	}
	
	xrtFree(sAdminPrefix);
	xrtFree(sApiPrefix);
	
	printf("        [Model] URIs synced to DB\n");
}


// 从数据库删除模型的所有 URI
void Model_RemoveUrisFromDb(ModelInstance* pModel)
{
	printf("        [Model] Removing URIs from DB for %s...\n", pModel->sName);
	
	str sNs = pModel->sNamespace;
	str sName = pModel->sName;
	
	// 构建 API 前缀
	str sAdminPrefix = NULL;
	str sApiPrefix = NULL;
	if ( sNs && strlen(sNs) > 0 ) {
		sAdminPrefix = xrtFormat("/admin/%s/%s", sNs, sName);
		sApiPrefix = xrtFormat("/api/v1/%s/%s", sNs, sName);
	} else {
		sAdminPrefix = xrtFormat("/admin/%s", sName);
		sApiPrefix = xrtFormat("/api/v1/%s", sName);
	}
	
	str sUri = NULL;
	
	// 后台视图路由
	if ( sNs && strlen(sNs) > 0 ) {
		sUri = xrtFormat("/admin/view/model/data/%s/%s", sNs, sName);
	} else {
		sUri = xrtFormat("/admin/view/model/data/%s", sName);
	}
	Model_RemoveUriFromDb(sUri);
	xrtFree(sUri);
	
	// 添加和编辑视图路由
	if ( sNs && strlen(sNs) > 0 ) {
		sUri = xrtFormat("/admin/view/model/data/%s/%s/add", sNs, sName);
	} else {
		sUri = xrtFormat("/admin/view/model/data/%s/add", sName);
	}
	Model_RemoveUriFromDb(sUri);
	xrtFree(sUri);
	
	if ( sNs && strlen(sNs) > 0 ) {
		sUri = xrtFormat("/admin/view/model/data/%s/%s/edit", sNs, sName);
	} else {
		sUri = xrtFormat("/admin/view/model/data/%s/edit", sName);
	}
	Model_RemoveUriFromDb(sUri);
	xrtFree(sUri);
	
	// 后台 API 路由
	sUri = xrtFormat("%s/list", sAdminPrefix);
	Model_RemoveUriFromDb(sUri);
	xrtFree(sUri);
	
	sUri = xrtFormat("%s/get", sAdminPrefix);
	Model_RemoveUriFromDb(sUri);
	xrtFree(sUri);
	
	sUri = xrtFormat("%s/add", sAdminPrefix);
	Model_RemoveUriFromDb(sUri);
	xrtFree(sUri);
	
	sUri = xrtFormat("%s/save", sAdminPrefix);
	Model_RemoveUriFromDb(sUri);
	xrtFree(sUri);
	
	sUri = xrtFormat("%s/delete", sAdminPrefix);
	Model_RemoveUriFromDb(sUri);
	xrtFree(sUri);
	
	// 前台 API 路由
	sUri = xrtFormat("%s/all", sApiPrefix);
	Model_RemoveUriFromDb(sUri);
	xrtFree(sUri);
	
	sUri = xrtFormat("%s/get", sApiPrefix);
	Model_RemoveUriFromDb(sUri);
	xrtFree(sUri);
	
	// 前台投稿路由
	sUri = xrtFormat("%s/submit", sApiPrefix);
	Model_RemoveUriFromDb(sUri);
	xrtFree(sUri);
	
	xrtFree(sAdminPrefix);
	xrtFree(sApiPrefix);
	
	printf("        [Model] URIs removed from DB\n");
}


// 启用模型
bool Model_Enable(ModelInstance* pModel)
{
	printf("        [Model] Enabling %s...\n", pModel->sName);
	
	if ( !pModel->bCompiled ) {
		printf("        [Model] Model is not compiled\n");
		return FALSE;
	}
	
	if ( pModel->bEnabled ) {
		printf("        [Model] Model is already enabled\n");
		return TRUE;
	}
	
	// 创建权限分组
	int iAuthId = Model_CreateAuthGroup(pModel);
	pModel->iAdminAuthId = iAuthId;
	
	// 创建后台菜单（如果启用了后台管理）
	if ( pModel->bEnableAdmin ) {
		Model_CreateMenu(pModel);
		
		// 注册视图路由
		str sViewUri = NULL;
		str sAddUri = NULL;
		str sEditUri = NULL;
		if ( pModel->sNamespace && strlen(pModel->sNamespace) > 0 ) {
			sViewUri = xrtFormat("/admin/view/model/data/%s/%s", pModel->sNamespace, pModel->sName);
			sAddUri = xrtFormat("/admin/view/model/data/%s/%s/add", pModel->sNamespace, pModel->sName);
			sEditUri = xrtFormat("/admin/view/model/data/%s/%s/edit", pModel->sNamespace, pModel->sName);
		} else {
			sViewUri = xrtFormat("/admin/view/model/data/%s", pModel->sName);
			sAddUri = xrtFormat("/admin/view/model/data/%s/add", pModel->sName);
			sEditUri = xrtFormat("/admin/view/model/data/%s/edit", pModel->sName);
		}
		RouteInfo* pViewRoute = Model_AddRoute(sViewUri, Request_View_Model_Data, TRUE, TRUE, iAuthId, 0);
		if ( pViewRoute ) {
			printf("        [Model] View route registered: %s\n", sViewUri);
		}
		Model_AddRoute(sAddUri, Request_View_Model_Data_Add, TRUE, TRUE, iAuthId, 0);
		printf("        [Model] Add route registered: %s\n", sAddUri);
		Model_AddRoute(sEditUri, Request_View_Model_Data_Edit, TRUE, TRUE, iAuthId, 0);
		printf("        [Model] Edit route registered: %s\n", sEditUri);
		xrtFree(sViewUri);
		xrtFree(sAddUri);
		xrtFree(sEditUri);
	}
	
	// 使用TCC加载模型代码
	if ( !Model_TccLoad(pModel) ) {
		printf("        [Model] Failed to load model with TCC\n");
		// 回滚菜单、权限和视图路由
		if ( pModel->bEnableAdmin ) {
			// 移除视图路由
			str sViewUri = NULL;
			str sAddUri = NULL;
			str sEditUri = NULL;
			if ( pModel->sNamespace && strlen(pModel->sNamespace) > 0 ) {
				sViewUri = xrtFormat("/admin/view/model/data/%s/%s", pModel->sNamespace, pModel->sName);
				sAddUri = xrtFormat("/admin/view/model/data/%s/%s/add", pModel->sNamespace, pModel->sName);
				sEditUri = xrtFormat("/admin/view/model/data/%s/%s/edit", pModel->sNamespace, pModel->sName);
			} else {
				sViewUri = xrtFormat("/admin/view/model/data/%s", pModel->sName);
				sAddUri = xrtFormat("/admin/view/model/data/%s/add", pModel->sName);
				sEditUri = xrtFormat("/admin/view/model/data/%s/edit", pModel->sName);
			}
			Model_RemoveRoute(sViewUri);
			Model_RemoveRoute(sAddUri);
			Model_RemoveRoute(sEditUri);
			xrtFree(sViewUri);
			xrtFree(sAddUri);
			xrtFree(sEditUri);
			
			Model_RemoveMenu(pModel);
		}
		Model_RemoveAuthGroup(pModel);
		return FALSE;
	}
	
	// 添加到已启用列表
	int iIdx = xrtListCount(G_ModelMgr->lstEnabledModels);
	xrtListSetPtr(G_ModelMgr->lstEnabledModels, iIdx, pModel, NULL);
	
	// 更新状态
	pModel->bEnabled = TRUE;
	
	// 更新配置文件
	xvalue tblConfig = xrtParseJSON_File(pModel->sConfigPath);
	if ( tblConfig ) {
		xvalue tblStatus = xvoTableGetValue(tblConfig, "status", 6);
		if ( !tblStatus ) {
			tblStatus = xvoCreateTable();
			xvoTableSetValue(tblConfig, "status", 6, tblStatus, TRUE);
		}
		xvoTableSetBool(tblStatus, "enabled", 7, TRUE);
		xvoTableSetInt(tblConfig, "updateTime", 10, xrtNow());
		
		// 保存权限ID
		xvalue tblAdmin = xvoTableGetValue(tblConfig, "admin", 5);
		if ( !tblAdmin ) {
			tblAdmin = xvoCreateTable();
			xvoTableSetValue(tblConfig, "admin", 5, tblAdmin, TRUE);
		}
		xvoTableSetInt(tblAdmin, "authId", 6, iAuthId);
		
		xrtStringifyJSON_File(pModel->sConfigPath, tblConfig, TRUE);
		xvoUnref(tblConfig);
	}
	
	// 同步 URI 到数据库并刷新权限缓存
	Model_SyncUrisToDb(pModel);
	Auth_ReloadCache();
	
	printf("        [Model] Model enabled: %s\n", pModel->sName);
	return TRUE;
}


// 禁用模型
bool Model_Disable(ModelInstance* pModel)
{
	printf("        [Model] Disabling %s...\n", pModel->sName);
	
	if ( !pModel->bEnabled ) {
		printf("        [Model] Model is already disabled\n");
		return TRUE;
	}
	
	// 使用TCC卸载模型
	Model_TccUnload(pModel);
	
	// 删除后台菜单和视图路由
	if ( pModel->bEnableAdmin ) {
		// 移除视图路由
		str sViewUri = NULL;
		str sAddUri = NULL;
		str sEditUri = NULL;
		if ( pModel->sNamespace && strlen(pModel->sNamespace) > 0 ) {
			sViewUri = xrtFormat("/admin/view/model/data/%s/%s", pModel->sNamespace, pModel->sName);
			sAddUri = xrtFormat("/admin/view/model/data/%s/%s/add", pModel->sNamespace, pModel->sName);
			sEditUri = xrtFormat("/admin/view/model/data/%s/%s/edit", pModel->sNamespace, pModel->sName);
		} else {
			sViewUri = xrtFormat("/admin/view/model/data/%s", pModel->sName);
			sAddUri = xrtFormat("/admin/view/model/data/%s/add", pModel->sName);
			sEditUri = xrtFormat("/admin/view/model/data/%s/edit", pModel->sName);
		}
		Model_RemoveRoute(sViewUri);
		Model_RemoveRoute(sAddUri);
		Model_RemoveRoute(sEditUri);
		xrtFree(sViewUri);
		xrtFree(sAddUri);
		xrtFree(sEditUri);
		
		Model_RemoveMenu(pModel);
	}
	
	// 删除权限分组
	Model_RemoveAuthGroup(pModel);
	
	// 从已启用列表中移除
	int iCount = xrtListCount(G_ModelMgr->lstEnabledModels);
	for ( int i = 0; i < iCount; i++ ) {
		ModelInstance* pItem = xrtListGetPtr(G_ModelMgr->lstEnabledModels, i);
		if ( pItem == pModel ) {
			xrtListRemove(G_ModelMgr->lstEnabledModels, i);
			break;
		}
	}
	
	// 更新状态
	pModel->bEnabled = FALSE;
	
	// 更新配置文件
	xvalue tblConfig = xrtParseJSON_File(pModel->sConfigPath);
	if ( tblConfig ) {
		xvalue tblStatus = xvoTableGetValue(tblConfig, "status", 6);
		if ( !tblStatus ) {
			tblStatus = xvoCreateTable();
			xvoTableSetValue(tblConfig, "status", 6, tblStatus, TRUE);
		}
		xvoTableSetBool(tblStatus, "enabled", 7, FALSE);
		xvoTableSetInt(tblConfig, "updateTime", 10, xrtNow());
		xrtStringifyJSON_File(pModel->sConfigPath, tblConfig, TRUE);
		xvoUnref(tblConfig);
	}
	
	// 从数据库删除 URI 并刷新权限缓存
	Model_RemoveUrisFromDb(pModel);
	Auth_ReloadCache();
	
	printf("        [Model] Model disabled: %s\n", pModel->sName);
	return TRUE;
}



// 从配置文件加载模型实例
ModelInstance* Model_LoadFromConfig(str sName, str sConfigPath)
{
	// 读取配置文件
	str sJson = xrtFileReadAll(sConfigPath, XRT_CP_UTF8, NULL);
	if ( !sJson ) {
		printf("        [Model] Failed to read config: %s\n", sConfigPath);
		return NULL;
	}
	
	// 解析JSON
	xvalue tblConfig = xrtParseJSON(sJson, strlen(sJson));
	xrtFree(sJson);
	
	if ( !tblConfig ) {
		printf("        [Model] Failed to parse config: %s\n", sConfigPath);
		return NULL;
	}
	
	// 创建模型实例
	ModelInstance* pModel = Model_Create(sName);
	pModel->sConfigPath = xrtCopyStr(sConfigPath, 0);
	
	// 基础信息
	str sTitle = xvoTableGetText(tblConfig, "title", 5);
	if ( sTitle ) pModel->sTitle = xrtCopyStr(sTitle, 0);
	
	str sDesc = xvoTableGetText(tblConfig, "desc", 4);
	if ( sDesc ) pModel->sDesc = xrtCopyStr(sDesc, 0);
	
	str sNamespace = xvoTableGetText(tblConfig, "namespace", 9);
	if ( sNamespace ) pModel->sNamespace = xrtCopyStr(sNamespace, 0);
	
	str sIcon = xvoTableGetText(tblConfig, "icon", 4);
	if ( sIcon ) pModel->sIcon = xrtCopyStr(sIcon, 0);
	
	// 表名
	xvalue tblTable = xvoTableGetValue(tblConfig, "table", 5);
	if ( tblTable ) {
		str sTableName = xvoTableGetText(tblTable, "name", 4);
		if ( sTableName ) pModel->sTableName = xrtCopyStr(sTableName, 0);
	}
	
	// 状态
	xvalue tblStatus = xvoTableGetValue(tblConfig, "status", 6);
	if ( tblStatus ) {
		pModel->bEnabled = xvoTableGetBool(tblStatus, "enabled", 7);
		pModel->bCompiled = xvoTableGetBool(tblStatus, "compiled", 8);
	}
	
	// 功能开关
	xvalue tblFeatures = xvoTableGetValue(tblConfig, "features", 8);
	if ( tblFeatures ) {
		pModel->bEnableApi = xvoTableGetBool(tblFeatures, "enableApi", 9);
		pModel->bEnableAdmin = xvoTableGetBool(tblFeatures, "enableAdmin", 11);
		pModel->bEnableSubmit = xvoTableGetBool(tblFeatures, "enableSubmit", 12);
		pModel->bEnableReply = xvoTableGetBool(tblFeatures, "enableReply", 11);
	}
	
	// API权限配置
	xvalue tblApi = xvoTableGetValue(tblConfig, "api", 3);
	if ( tblApi ) {
		pModel->iApiAuthLevel = xvoTableGetInt(tblApi, "authLevel", 9);
		pModel->iApiAuthId = xvoTableGetInt(tblApi, "authId", 6);
	}
	
	// 后台配置
	xvalue tblAdmin = xvoTableGetValue(tblConfig, "admin", 5);
	if ( tblAdmin ) {
		pModel->iMenuParent = xvoTableGetInt(tblAdmin, "menuParent", 10);
		pModel->iMenuSort = xvoTableGetInt(tblAdmin, "menuSort", 8);
		pModel->iAdminAuthId = xvoTableGetInt(tblAdmin, "authId", 6);
	}
	
	// 评论配置
	xvalue tblReply = xvoTableGetValue(tblConfig, "reply", 5);
	if ( tblReply ) {
		pModel->bReplyNeedApprove = xvoTableGetBool(tblReply, "needApprove", 11);
		pModel->iReplyAuthLevel = xvoTableGetInt(tblReply, "authLevel", 9);
		pModel->iReplyQuoteMaxLen = xvoTableGetInt(tblReply, "quoteMaxLen", 11);
		if ( pModel->iReplyQuoteMaxLen <= 0 ) pModel->iReplyQuoteMaxLen = 50;
	}
	
	// 字段列表
	xvalue arrFields = xvoTableGetValue(tblConfig, "fields", 6);
	if ( arrFields ) {
		pModel->arrFields = arrFields;
		xvoAddRef(arrFields);  // 增加引用计数
	}
	
	// 设置代码文件路径（用于TCC加载）
	pModel->sCodePath = xrtFormat("%s/%s/code.h", ModelPath, sName);
	
	// 时间戳
	pModel->iCreateTime = xvoTableGetInt(tblConfig, "createTime", 10);
	pModel->iUpdateTime = xvoTableGetInt(tblConfig, "updateTime", 10);
	
	xvoUnref(tblConfig);
	
	printf("        [Model] Loaded model: %s (%s)\n", pModel->sName, pModel->sTitle ? pModel->sTitle : (str)"");
	return pModel;
}



// ==================== 模型管理器初始化 ====================

// 销毁模型的回调函数
bool ModelMgr_DestroyWalkProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	ModelInstance** ppModel = (ModelInstance**)pVal;
	if ( ppModel && *ppModel ) {
		Model_Destroy(*ppModel);
	}
	return TRUE;  // 继续遍历
}

// 获取模型列表的回调函数
bool ModelMgr_ListWalkProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	xvalue arrList = (xvalue)pArg;
	ModelInstance** ppModel = (ModelInstance**)pVal;
	if ( !ppModel || !(*ppModel) ) return TRUE;
	
	ModelInstance* pModel = *ppModel;
	
	xvalue tblItem = xvoCreateTable();
	xvoTableSetText(tblItem, "name", 4, pModel->sName, 0, FALSE);
	xvoTableSetText(tblItem, "title", 5, pModel->sTitle ? pModel->sTitle : (str)"", 0, FALSE);
	xvoTableSetText(tblItem, "desc", 4, pModel->sDesc ? pModel->sDesc : (str)"", 0, FALSE);
	xvoTableSetText(tblItem, "namespace", 9, pModel->sNamespace ? pModel->sNamespace : (str)"", 0, FALSE);
	xvoTableSetText(tblItem, "icon", 4, pModel->sIcon ? pModel->sIcon : (str)"", 0, FALSE);
	xvoTableSetBool(tblItem, "enabled", 7, pModel->bEnabled);
	xvoTableSetBool(tblItem, "compiled", 8, pModel->bCompiled);
	xvoTableSetText(tblItem, "createTime", 10, xrtTimeToStr(pModel->iCreateTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
	xvoTableSetText(tblItem, "updateTime", 10, xrtTimeToStr(pModel->iUpdateTime, XRT_TIME_FORMAT_DATETIME), 0, TRUE);
	
	xvoArrayAppendValue(arrList, tblItem, TRUE);
	return TRUE;  // 继续遍历
}

// 扫描模型目录的回调函数
int ModelMgr_ScanDirProc(str sPath, size_t iSize, int bDir, ptr pData, size_t iPathSize)
{
	// 只处理目录（进入时，bDir=1），跳过文件(0)和离开目录(2)
	if ( bDir != 1 ) return FALSE;
	
	// 获取目录名
	str sName = xrtPathGetName(sPath, 0);
	if ( !sName ) return FALSE;
	
	// 跳过隐藏目录和特殊目录
	if ( sName[0] == '.' || sName[0] == '_' ) {
		xrtFree(sName);
		return FALSE;
	}
	
	// 检查是否存在 config.json
	str sConfigPath = xrtFormat("%s/config.json", sPath);
	if ( !xrtFileExists(sConfigPath) ) {
		xrtFree(sConfigPath);
		xrtFree(sName);
		return FALSE;
	}
	
	// 加载模型
	ModelInstance* pModel = Model_LoadFromConfig(sName, sConfigPath);
	xrtFree(sConfigPath);
	xrtFree(sName);
	
	if ( pModel ) {
		// 添加到模型表
		ModelInstance** ppSlot = xrtDictSet(G_ModelMgr->tblModels, pModel->sName, strlen(pModel->sName), NULL);
		if ( ppSlot ) {
			*ppSlot = pModel;
		}
		
		// 注册命名空间
		Model_RegisterNamespace(pModel);
		
		// 此时只加载配置，不在扫描时启用
		// 启用操作在 ModelMgr_Init 完成后统一执行
	}
	
	return FALSE;  // FALSE表示不递归进入子目录
}

// 扫描并加载所有模型
void ModelMgr_ScanModels()
{
	printf("        [Model] Scanning models in: %s\n", ModelPath);
	
	// 使用回调方式遍历模型目录
	xrtDirScan(ModelPath, FALSE, ModelMgr_ScanDirProc, NULL);
}


// 自动启用状态为启用的模型
bool ModelMgr_AutoEnableWalkProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	ModelInstance** ppModel = (ModelInstance**)pVal;
	if ( !ppModel || !(*ppModel) ) return TRUE;
	
	ModelInstance* pModel = *ppModel;
	
	// 只启用已编译且状态为启用的模型
	if ( pModel->bCompiled && pModel->bEnabled ) {
		printf("        [Model] Auto-enabling: %s\n", pModel->sName);
		// 先置为未启用，然后调用 Model_Enable 执行完整启用流程
		pModel->bEnabled = FALSE;
		Model_Enable(pModel);
	}
	
	return TRUE;  // 继续遍历
}

void ModelMgr_AutoEnableModels()
{
	printf("        [Model] Auto-enabling models...\n");
	xrtDictWalk(G_ModelMgr->tblModels, ModelMgr_AutoEnableWalkProc, NULL);
	printf("        [Model] Auto-enable complete.\n");
}


// 初始化模型上下文
void ModelMgr_InitContext()
{
	G_ModelCtx = xrtMalloc(sizeof(ModelContext));
	memset(G_ModelCtx, 0, sizeof(ModelContext));
	
	// 数据库
	G_ModelCtx->pDB = G_DB;
	
	// 路由操作
	G_ModelCtx->AddRoute = Model_AddRoute;
	G_ModelCtx->RemoveRoute = Model_RemoveRoute;
	
	// JSON操作
	G_ModelCtx->JsonParse = ModelCtx_JsonParse;
	G_ModelCtx->JsonStringify = ModelCtx_JsonStringify;
	G_ModelCtx->JsonFree = ModelCtx_JsonFree;
	
	// 会话操作
	G_ModelCtx->GetAdminSession = ModelCtx_GetAdminSession;
	G_ModelCtx->GetMemberSession = ModelCtx_GetMemberSession;
	
	// 工具函数
	G_ModelCtx->TimeNow = ModelCtx_TimeNow;
	G_ModelCtx->Format = xrtFormat;
	G_ModelCtx->Free = xrtFree;
	
	// 日志
	G_ModelCtx->Log = ModelCtx_Log;
}


// 检查并创建模型管理菜单
void ModelMgr_EnsureMenu()
{
	// 检查菜单是否已存在
	sqlite3_stmt* stmt_check;
	sqlite3_prepare_v3(G_DB->objDB, 
		"SELECT id FROM menu WHERE href = '/admin/view/model' AND isDelete = 0",
		-1, 0, &stmt_check, NULL);
	
	int bExists = FALSE;
	if ( sqlite3_step(stmt_check) == SQLITE_ROW ) {
		bExists = TRUE;
	}
	sqlite3_finalize(stmt_check);
	
	// 如果菜单不存在，则创建
	if ( !bExists ) {
		printf("        [Model] Creating menu item...\n");
		
		sqlite3_stmt* stmt_insert;
		sqlite3_prepare_v3(G_DB->objDB,
			"INSERT INTO menu (parent, title, icon, type, openType, href, sort, visible, remark, createTime, updateTime, isDelete) "
			"VALUES (0, '内容模型', 'layui-icon layui-icon-component', 1, '_component', '/admin/view/model', 200000, 1, '内容模型管理，可定义自定义内容结构', ?, ?, 0)",
			-1, 0, &stmt_insert, NULL);
		
		xtime now = xrtNow();
		sqlite3_bind_int64(stmt_insert, 1, now);
		sqlite3_bind_int64(stmt_insert, 2, now);
		sqlite3_step(stmt_insert);
		sqlite3_finalize(stmt_insert);
		
		printf("        [Model] Menu item created.\n");
	}
}


// 初始化模型管理器
void ModelMgr_Init()
{
	printf("        ModelMgr_Init \n");
	
	// 创建管理器
	G_ModelMgr = xrtMalloc(sizeof(ModelManager));
	memset(G_ModelMgr, 0, sizeof(ModelManager));
	
	// 初始化数据结构
	G_ModelMgr->tblModels = xrtDictCreate(sizeof(ModelInstance*));
	G_ModelMgr->tblNamespaces = xrtDictCreate(0);
	G_ModelMgr->lstEnabledModels = xrtListCreate(sizeof(ptr));
	
	// 初始化上下文
	ModelMgr_InitContext();
	
	// 确保菜单存在
	ModelMgr_EnsureMenu();
	
	// 扫描并加载模型
	ModelMgr_ScanModels();
	
	// 自动启用状态为启用的模型
	ModelMgr_AutoEnableModels();
}


// 卸载模型管理器
void ModelMgr_Unit()
{
	printf("        ModelMgr_Unit \n");
	
	if ( !G_ModelMgr ) return;
	
	// 遍历并销毁所有模型
	xrtDictWalk(G_ModelMgr->tblModels, ModelMgr_DestroyWalkProc, NULL);
	
	// 销毁数据结构
	xrtDictDestroy(G_ModelMgr->tblModels);
	xrtDictDestroy(G_ModelMgr->tblNamespaces);
	xrtListDestroy(G_ModelMgr->lstEnabledModels);
	
	// 释放上下文
	if ( G_ModelCtx ) {
		xrtFree(G_ModelCtx);
		G_ModelCtx = NULL;
	}
	
	// 释放管理器
	xrtFree(G_ModelMgr);
	G_ModelMgr = NULL;
}



// ==================== 模型列表查询 ====================

// 获取所有模型列表
xvalue ModelMgr_GetModelList()
{
	xvalue arrList = xvoCreateArray();
	xrtDictWalk(G_ModelMgr->tblModels, ModelMgr_ListWalkProc, arrList);
	return arrList;
}


// 根据名称获取模型
ModelInstance* ModelMgr_GetModel(str sName)
{
	if ( !sName ) return NULL;
	
	ModelInstance** ppModel = xrtDictGet(G_ModelMgr->tblModels, sName, strlen(sName));
	if ( ppModel ) {
		return *ppModel;
	}
	return NULL;
}


