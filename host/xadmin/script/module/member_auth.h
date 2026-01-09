



// 前台用户权限缓存模块 - 与后台 auth.h 对应，完全独立的权限体系



// ==================== 前台权限分组缓存 ====================

// 缓存前台权限分组列表 - ComboBox 使用
void ReloadCache_MemberAuth()
{
	xvalue arrRet = xvoCreateArray();
	
	// 使用预编译语句查询数据
	while ( sqlite3_step(stmt_cache_mauth) == SQLITE_ROW ) {
		xvalue tblRow = xvoCreateTable();
		int64 id = sqlite3_column_int64(stmt_cache_mauth, 0);
		int64 groupId = sqlite3_column_int64(stmt_cache_mauth, 1);
		xvoTableSetInt(tblRow, "id", 2, id);
		xvoTableSetInt(tblRow, "groupId", 7, groupId);
		xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt_cache_mauth, 2), 0, FALSE);
		xvoArrayAppendValue(arrRet, tblRow, TRUE);
	}
	sqlite3_reset(stmt_cache_mauth);
	
	// 替换全局缓存（线程安全写法）
	if ( G_CACHE_MemberAuth ) {
		xvalue oldCache = G_CACHE_MemberAuth;
		G_CACHE_MemberAuth = arrRet;
		xvoUnref(oldCache);
	} else {
		G_CACHE_MemberAuth = arrRet;
	}
}



// ==================== 前台权限分类缓存 ====================

// 缓存前台权限分类列表（依赖 G_CACHE_MemberAuth 数据）
void ReloadCache_MemberAuthGroup()
{
	// 查询所有权限分类（按排序值）
	xvalue arrRet = xvoCreateArray();
	
	// 创建 groupId -> 数组下标 的映射表
	xvalue listIndex = xvoCreateList();
	int iIndex = 0;
	
	// 使用预编译语句查询数据
	while ( sqlite3_step(stmt_cache_magroup) == SQLITE_ROW ) {
		int64 groupId = sqlite3_column_int64(stmt_cache_magroup, 0);
		str groupName = (str)sqlite3_column_text(stmt_cache_magroup, 1);
		
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
	sqlite3_reset(stmt_cache_magroup);
	
	// 遍历 G_CACHE_MemberAuth，将权限分组添加到对应分类的 auths 中
	for ( int i = 1; i <= G_CACHE_MemberAuth->vArray->Count; i++ ) {
		xvalue pAuth = xrtPtrArrayGet_Inline(G_CACHE_MemberAuth->vArray, i);
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
	
	// 替换全局缓存（线程安全写法）
	if ( G_CACHE_MemberAuthGroup ) {
		xvalue oldCache = G_CACHE_MemberAuthGroup;
		G_CACHE_MemberAuthGroup = arrRet;
		xvoUnref(oldCache);
	} else {
		G_CACHE_MemberAuthGroup = arrRet;
	}
}



// ==================== 前台用户组权限缓存 ====================

// 前台路由权限分类回调 - 用于构建用户组的 URI 权限字典
bool MemberAuthRouteCategorize(Dict_Key* pKey, RouteInfo* pInfo, ptr param)
{
	struct {
		xvalue listAuth;
		xvalue tblURI;
	} *pAuthInfo = param;
	
	// 仅处理前台 URI（bAdmin = FALSE）且需要鉴权的路由
	if ( !pInfo->bAdmin && pInfo->bAuth && (pInfo->AuthID > 0) ) {
		bool bPass = xvoListGetBool(pAuthInfo->listAuth, pInfo->AuthID);
		if ( bPass ) {
			xvoTableSetBool(pAuthInfo->tblURI, pKey->Key, pKey->KeyLen, TRUE);
		}
	}
	return FALSE;
}

// 重新加载前台用户组权限缓存
// G_CACHE_MemberGroup: 用户组列表缓存 - 前端 ComboBox 用
// G_CACHE_MemberGroupAuth: 用户组权限缓存 - 后端鉴权查表用
void MemberAuth_ReloadCache()
{
	// 使用预编译语句查询数据
	xvalue arrRet = xvoCreateArray();	// 用户组列表
	xvalue lstRet = xvoCreateList();	// 用户组权限映射表
	
	while ( sqlite3_step(stmt_cache_mgroup) == SQLITE_ROW ) {
		// 添加到列表缓存
		xvalue tblRow = xvoCreateTable();
		int64 id = sqlite3_column_int64(stmt_cache_mgroup, 0);
		str name = (str)sqlite3_column_text(stmt_cache_mgroup, 1);
		int64 authLevel = sqlite3_column_int64(stmt_cache_mgroup, 4);
		xvoTableSetInt(tblRow, "id", 2, id);
		xvoTableSetText(tblRow, "name", 4, name, 0, FALSE);
		xvoTableSetInt(tblRow, "authLevel", 9, authLevel);
		xvoArrayAppendValue(arrRet, tblRow, TRUE);
		
		// 解析权限分组列表 - 转换为 list 方便按ID索引
		xvalue listAuth = xvoCreateList();
		str sAuthList = (str)sqlite3_column_text(stmt_cache_mgroup, 3);
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
		
		// 构建对应用户组的 URI 权限字典
		xvalue tblURI = xvoCreateTable();
		struct {
			xvalue listAuth;
			xvalue tblURI;
		} dictWalkInfo = { listAuth, tblURI };
		xrtDictWalk(G_StaticRouteTableHTTP, (ptr)MemberAuthRouteCategorize, &dictWalkInfo);
		xvoUnref(listAuth);
		
		// 权限字典添加元数据
		xvoTableSetInt(tblURI, "__id__", 6, id);
		xvoTableSetText(tblURI, "__name__", 8, name, 0, FALSE);
		xvoTableSetInt(tblURI, "__authLevel__", 13, authLevel);
		
		// 将整理好的权限字典添加到缓存表
		xvoListSetValue(lstRet, id, tblURI, TRUE);
	}
	sqlite3_reset(stmt_cache_mgroup);
	
	// 替换全局缓存 - 用户组列表（线程安全写法）
	if ( G_CACHE_MemberGroup ) {
		xvalue oldCache = G_CACHE_MemberGroup;
		G_CACHE_MemberGroup = arrRet;
		xvoUnref(oldCache);
	} else {
		G_CACHE_MemberGroup = arrRet;
	}
	
	// 替换全局缓存 - 用户组权限表（线程安全写法）
	if ( G_CACHE_MemberGroupAuth ) {
		xvalue oldCache = G_CACHE_MemberGroupAuth;
		G_CACHE_MemberGroupAuth = lstRet;
		xvoUnref(oldCache);
	} else {
		G_CACHE_MemberGroupAuth = lstRet;
	}
}



// ==================== 前台 URI 缓存加载 ====================

// 加载前台 URI 配置到路由表（从 uris 表筛选 isBackend=0 的记录）
void MemberAuth_LoadURIS()
{
	// 遍历数据库中的前台 URI 记录，更新路由表中的 AuthID
	while ( sqlite3_step(stmt_cache_muris) == SQLITE_ROW ) {
		int64 authID = sqlite3_column_int64(stmt_cache_muris, 1);
		str uri = (str)sqlite3_column_text(stmt_cache_muris, 2);
		size_t iSize = strlen(uri);
		RouteInfo* pInfo = xrtDictGet(G_StaticRouteTableHTTP, uri, iSize);
		if ( pInfo && !pInfo->bAdmin ) {
			// 前台路由存在，更新 AuthID
			pInfo->AuthID = authID;
		}
	}
	sqlite3_reset(stmt_cache_muris);
}



// ==================== 模块初始化与卸载 ====================

// 前台权限缓存模块初始化
void MemberAuth_Init()
{
	printf("        MemberAuth_Init \n");
	
	// 从 uris 表加载前台 URI 配置到路由表
	MemberAuth_LoadURIS();
	
	// 重新加载全局缓存
	ReloadCache_MemberAuth();
	ReloadCache_MemberAuthGroup();
	MemberAuth_ReloadCache();
}



// 前台权限缓存模块卸载
void MemberAuth_Unit()
{
	printf("        MemberAuth_Unit \n");
	
	// 释放全局缓存表
	if ( G_CACHE_MemberGroupAuth ) {
		xvoUnref(G_CACHE_MemberGroupAuth);
		G_CACHE_MemberGroupAuth = NULL;
	}
	if ( G_CACHE_MemberAuth ) {
		xvoUnref(G_CACHE_MemberAuth);
		G_CACHE_MemberAuth = NULL;
	}
	if ( G_CACHE_MemberAuthGroup ) {
		xvoUnref(G_CACHE_MemberAuthGroup);
		G_CACHE_MemberAuthGroup = NULL;
	}
	if ( G_CACHE_MemberGroup ) {
		xvoUnref(G_CACHE_MemberGroup);
		G_CACHE_MemberGroup = NULL;
	}
}


