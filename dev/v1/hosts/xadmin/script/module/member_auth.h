



// 前台用户权限缓存模块 - 与后�?auth.h 对应，完全独立的权限体系



// ==================== 前台权限分组缓存 ====================

// 缓存前台权限分组列表 - ComboBox 使用
static bool MemberAuth_DBGroupGetAccess(int64 iGroupID, int64 iAuthID, int64* pAuthLevel)
{
	bool bAllowed = FALSE;
	str sAuthList = NULL;
	xvalue arrAuth = NULL;

	if ( pAuthLevel ) {
		*pAuthLevel = -1;
	}
	if ( iGroupID <= 0 ) {
		return FALSE;
	}

	sqlite3_bind_int64(stmt_mgroup_get, 1, iGroupID);
	if ( sqlite3_step(stmt_mgroup_get) == SQLITE_ROW ) {
		if ( sqlite3_column_int64(stmt_mgroup_get, 7) == 0 ) {
			if ( pAuthLevel ) {
				*pAuthLevel = sqlite3_column_int64(stmt_mgroup_get, 4);
			}
			if ( iAuthID <= 0 ) {
				bAllowed = TRUE;
			} else {
				sAuthList = (str)sqlite3_column_text(stmt_mgroup_get, 3);
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
	sqlite3_reset(stmt_mgroup_get);
	return bAllowed;
}



void ReloadCache_MemberAuth()
{
	xvalue arrRet = XAdminCreateSharedArrayValue();
	
	// 使用预编译语句查询数�?
	while ( sqlite3_step(stmt_cache_mauth) == SQLITE_ROW ) {
		xvalue tblRow = XAdminCreateSharedTableValue();
		int64 id = sqlite3_column_int64(stmt_cache_mauth, 0);
		int64 groupId = sqlite3_column_int64(stmt_cache_mauth, 1);
		xvoTableSetInt(tblRow, "id", 2, id);
		xvoTableSetInt(tblRow, "groupId", 7, groupId);
		xvoTableSetText(tblRow, "name", 4, (str)sqlite3_column_text(stmt_cache_mauth, 2), 0, FALSE);
		xvoArrayAppendValue(arrRet, tblRow, TRUE);
	}
	sqlite3_reset(stmt_cache_mauth);
	XAdminValuePublishShared(arrRet);
	
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

// 缓存前台权限分类列表（依�?G_CACHE_MemberAuth 数据�?
void ReloadCache_MemberAuthGroup()
{
	// 查询所有权限分类（按排序值）
	xvalue arrRet = XAdminCreateSharedArrayValue();
	
	// 使用预编译语句查询数�?
	while ( sqlite3_step(stmt_cache_magroup) == SQLITE_ROW ) {
		int64 groupId = sqlite3_column_int64(stmt_cache_magroup, 0);
		str groupName = (str)sqlite3_column_text(stmt_cache_magroup, 1);
		
		// 创建分类�?
		xvalue tblGroup = XAdminCreateSharedTableValue();
		xvoTableSetInt(tblGroup, "id", 2, groupId);
		xvoTableSetText(tblGroup, "name", 4, groupName, 0, FALSE);
		
		// 创建空的 auths 数组
		xvalue arrAuths = XAdminCreateSharedArrayValue();
		xvoTableSetValue(tblGroup, "auths", 5, arrAuths, TRUE);
		
		// 添加到缓存数�?
		xvoArrayAppendValue(arrRet, tblGroup, TRUE);
	}
	sqlite3_reset(stmt_cache_magroup);
	
	// 遍历 G_CACHE_MemberAuth，将权限分组添加到对应分类的 auths �?
	for ( int i = 0; i < G_CACHE_MemberAuth->vArray->Count; i++ ) {
		xvalue pAuth = xvoArrayGetValue(G_CACHE_MemberAuth, i);
		int64 authId = xvoTableGetInt(pAuth, "id", 2);
		int64 groupId = xvoTableGetInt(pAuth, "groupId", 7);
		str authName = xvoTableGetText(pAuth, "name", 4);
		
		xvalue pGroup = NULL;
		for ( int j = 0; j < arrRet->vArray->Count; j++ ) {
			xvalue pGroupItem = xvoArrayGetValue(arrRet, j);
			if ( pGroupItem && (xvoTableGetInt(pGroupItem, "id", 2) == groupId) ) {
				pGroup = pGroupItem;
				break;
			}
		}
		if ( pGroup ) {
			xvalue arrAuths = xvoTableGetValue(pGroup, "auths", 5);
			
			// 创建权限项并添加�?auths 数组
			xvalue tblAuth = XAdminCreateSharedTableValue();
			xvoTableSetInt(tblAuth, "id", 2, authId);
			xvoTableSetText(tblAuth, "name", 4, authName, 0, FALSE);
			xvoArrayAppendValue(arrAuths, tblAuth, TRUE);
		}
	}
	XAdminValuePublishShared(arrRet);
	
	// 替换全局缓存（线程安全写法）
	if ( G_CACHE_MemberAuthGroup ) {
		xvalue oldCache = G_CACHE_MemberAuthGroup;
		G_CACHE_MemberAuthGroup = arrRet;
		xvoUnref(oldCache);
	} else {
		G_CACHE_MemberAuthGroup = arrRet;
	}
}



// ==================== 前台用户组权限缓�?====================

// 前台路由权限分类回调 - 用于构建用户组的 URI 权限字典
bool MemberAuthRouteCategorize(Dict_Key* pKey, RouteInfo* pInfo, ptr param)
{
	struct {
		xvalue listAuth;
		xvalue tblURI;
	} *pAuthInfo = param;
	
	// 仅处理前�?URI（bAdmin = FALSE）且需要鉴权的路由
	if ( !pInfo->bAdmin && pInfo->bAuth && (pInfo->AuthID > 0) ) {
		bool bPass = XAdminIDArrayContainsInt(pAuthInfo->listAuth, pInfo->AuthID);
		if ( bPass ) {
			xvoTableSetBool(pAuthInfo->tblURI, pKey->Key, pKey->KeyLen, TRUE);
		}
	}
	return FALSE;
}

// 重新加载前台用户组权限缓�?
// G_CACHE_MemberGroup: 用户组列表缓�?- 前端 ComboBox �?
// G_CACHE_MemberGroupAuth: 用户组权限缓�?- 后端鉴权查表�?
void MemberAuth_ReloadCache()
{
	// ʹ��Ԥ��������ѯ����
	xvalue arrRet = XAdminCreateSharedArrayValue();	// �û����б�
	xvalue lstRet = XAdminCreateSharedArrayValue();	// �û���Ȩ��ӳ���
	xvalue idxRet = XAdminCreateSharedListValue();	// group id -> array index
	xvalue lvlRet = XAdminCreateSharedListValue();	// group id -> authLevel
	
while ( sqlite3_step(stmt_cache_mgroup) == SQLITE_ROW ) {
		// 添加到列表缓�?
		xvalue tblRow = XAdminCreateSharedTableValue();
		int64 id = sqlite3_column_int64(stmt_cache_mgroup, 0);
		str name = (str)sqlite3_column_text(stmt_cache_mgroup, 1);
		int64 authLevel = sqlite3_column_int64(stmt_cache_mgroup, 4);
		xvoTableSetInt(tblRow, "id", 2, id);
		xvoTableSetText(tblRow, "name", 4, name, 0, FALSE);
		xvoTableSetInt(tblRow, "authLevel", 9, authLevel);
		xvoArrayAppendValue(arrRet, tblRow, TRUE);
		
		// 解析权限分组列表
		xvalue listAuth = NULL;
		str sAuthList = (str)sqlite3_column_text(stmt_cache_mgroup, 3);
		if ( sAuthList && (strlen(sAuthList) > 2) ) {
			xvalue arrAuth = xrtParseJSON(sAuthList, 0);
			if ( arrAuth && (arrAuth->Type == XVO_DT_ARRAY) ) {
				listAuth = arrAuth;
			} else if ( arrAuth ) {
				xvoUnref(arrAuth);
			}
		}
		
		// 构建对应用户组的 URI 权限字典
		xvalue tblURI = XAdminCreateSharedTableValue();
		struct {
			xvalue listAuth;
			xvalue tblURI;
		} dictWalkInfo = { listAuth, tblURI };
		xrtDictWalk(G_StaticRouteTableHTTP, (ptr)MemberAuthRouteCategorize, &dictWalkInfo);
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
	sqlite3_reset(stmt_cache_mgroup);
	XAdminValuePublishShared(arrRet);
	XAdminValuePublishShared(lstRet);
	XAdminValuePublishShared(idxRet);
	XAdminValuePublishShared(lvlRet);
	
	// 替换全局缓存 - 用户组列表（线程安全写法�?
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
	if ( G_CACHE_MemberGroupAuthIndex ) {
		xvalue oldCache = G_CACHE_MemberGroupAuthIndex;
		G_CACHE_MemberGroupAuthIndex = idxRet;
		xvoUnref(oldCache);
	} else {
		G_CACHE_MemberGroupAuthIndex = idxRet;
	}
	if ( G_CACHE_MemberGroupAuthLevel ) {
		xvalue oldCache = G_CACHE_MemberGroupAuthLevel;
		G_CACHE_MemberGroupAuthLevel = lvlRet;
		xvoUnref(oldCache);
	} else {
		G_CACHE_MemberGroupAuthLevel = lvlRet;
	}
}



// ==================== 前台 URI 缓存加载 ====================

// 加载前台 URI 配置到路由表（从 uris 表筛�?isBackend=0 的记录）
void MemberAuth_LoadURIS()
{
	// 遍历数据库中的前�?URI 记录，更新路由表中的 AuthID
	while ( sqlite3_step(stmt_cache_muris) == SQLITE_ROW ) {
		int64 authID = sqlite3_column_int64(stmt_cache_muris, 1);
		str uri = (str)sqlite3_column_text(stmt_cache_muris, 2);
		size_t iSize = strlen(uri);
		RouteInfo* pInfo = xrtDictGet(G_StaticRouteTableHTTP, uri, iSize);
		if ( pInfo == NULL ) {
			pInfo = FindDynamicRouteHTTP(uri);
		}
		if ( pInfo && !pInfo->bAdmin ) {
			// 前台路由存在，更�?AuthID
			pInfo->AuthID = authID;
		}
	}
	sqlite3_reset(stmt_cache_muris);
}



// ==================== 模块初始化与卸载 ====================

// 前台权限缓存模块初始�?
void MemberAuth_Init()
{
	printf("        MemberAuth_Init \n");
	
	// �?uris 表加载前�?URI 配置到路由表
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
	
	// 释放全局缓存�?
	if ( G_CACHE_MemberGroupAuth ) {
		xvoUnref(G_CACHE_MemberGroupAuth);
		G_CACHE_MemberGroupAuth = NULL;
	}
	if ( G_CACHE_MemberGroupAuthIndex ) {
		xvoUnref(G_CACHE_MemberGroupAuthIndex);
		G_CACHE_MemberGroupAuthIndex = NULL;
	}
	if ( G_CACHE_MemberGroupAuthLevel ) {
		xvoUnref(G_CACHE_MemberGroupAuthLevel);
		G_CACHE_MemberGroupAuthLevel = NULL;
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


