



// 前台用户权限缓存模块 - 与后�?auth.h 对应，完全独立的权限体系



// ==================== 前台权限分组缓存 ====================

// 缓存前台权限分组列表 - ComboBox 使用
static bool MemberAuth_DBGroupGetAccess(int64 iGroupID, int64 iAuthID, int64* pAuthLevel)
{
	bool bAllowed = false;
	str sAuthList = NULL;
	xvalue* arrAuth = NULL;

	if ( pAuthLevel ) {
		*pAuthLevel = -1;
	}
	if ( iGroupID <= 0 ) {
		return false;
	}

	sqlite3_bind_int64(stmt_mgroup_get, 1, iGroupID);
	if ( sqlite3_step(stmt_mgroup_get) == SQLITE_ROW ) {
		if ( sqlite3_column_int64(stmt_mgroup_get, 7) == 0 ) {
			if ( pAuthLevel ) {
				*pAuthLevel = sqlite3_column_int64(stmt_mgroup_get, 4);
			}
			if ( iAuthID <= 0 ) {
				bAllowed = true;
			} else {
				sAuthList = (str)sqlite3_column_text(stmt_mgroup_get, 3);
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
	sqlite3_reset(stmt_mgroup_get);
	return bAllowed;
}



void ReloadCache_MemberAuth()
{
	xvalue* arrRet = XAdminCreateSharedArrayValue();
	
	// 使用预编译语句查询数�?
	while ( sqlite3_step(stmt_cache_mauth) == SQLITE_ROW ) {
		xvalue* tblRow = XAdminCreateSharedTableValue();
		int64 id = sqlite3_column_int64(stmt_cache_mauth, 0);
		int64 groupId = sqlite3_column_int64(stmt_cache_mauth, 1);
		ValueSetInt(tblRow, "id", id);
		ValueSetInt(tblRow, "groupId", groupId);
		ValueSetText(tblRow, "name", (str)sqlite3_column_text(stmt_cache_mauth, 2));
		ValueArrayOwn(arrRet, tblRow);
	}
	sqlite3_reset(stmt_cache_mauth);
	XAdminValuePublishShared(arrRet);
	
	// 替换全局缓存（线程安全写法）
	if ( G_CACHE_MemberAuth ) {
		xvalue* oldCache = G_CACHE_MemberAuth;
		G_CACHE_MemberAuth = arrRet;
		CacheRetire(oldCache);
	} else {
		G_CACHE_MemberAuth = arrRet;
	}
}



// ==================== 前台权限分类缓存 ====================

// 缓存前台权限分类列表（依�?G_CACHE_MemberAuth 数据�?
void ReloadCache_MemberAuthGroup()
{
	// 查询所有权限分类（按排序值）
	xvalue* arrRet = XAdminCreateSharedArrayValue();
	
	// 使用预编译语句查询数�?
	while ( sqlite3_step(stmt_cache_magroup) == SQLITE_ROW ) {
		int64 groupId = sqlite3_column_int64(stmt_cache_magroup, 0);
		str groupName = (str)sqlite3_column_text(stmt_cache_magroup, 1);
		
		// 创建分类�?
		xvalue* tblGroup = XAdminCreateSharedTableValue();
		ValueSetInt(tblGroup, "id", groupId);
		ValueSetText(tblGroup, "name", groupName);
		
		// 创建空的 auths 数组
		xvalue* arrAuths = XAdminCreateSharedArrayValue();
		ValueSetOwn(tblGroup, "auths", arrAuths);
		
		// 添加到缓存数�?
		ValueArrayOwn(arrRet, tblGroup);
	}
	sqlite3_reset(stmt_cache_magroup);
	
	// 遍历 G_CACHE_MemberAuth，将权限分组添加到对应分类的 auths �?
	for ( int i = 0; i < xrtValueCount(G_CACHE_MemberAuth); i++ ) {
		xvalue* pAuth = xrtValueArrayGet(G_CACHE_MemberAuth, i);
		int64 authId = ValueInt(pAuth, "id");
		int64 groupId = ValueInt(pAuth, "groupId");
		str authName = ValueText(pAuth, "name");
		
		xvalue* pGroup = NULL;
		for ( int j = 0; j < xrtValueCount(arrRet); j++ ) {
			xvalue* pGroupItem = xrtValueArrayGet(arrRet, j);
			if ( pGroupItem && (ValueInt(pGroupItem, "id") == groupId) ) {
				pGroup = pGroupItem;
				break;
			}
		}
		if ( pGroup ) {
			xvalue* arrAuths = ValueGet(pGroup, "auths");
			
			// 创建权限项并添加�?auths 数组
			xvalue* tblAuth = XAdminCreateSharedTableValue();
			ValueSetInt(tblAuth, "id", authId);
			ValueSetText(tblAuth, "name", authName);
			ValueArrayOwn(arrAuths, tblAuth);
		}
	}
	XAdminValuePublishShared(arrRet);
	
	// 替换全局缓存（线程安全写法）
	if ( G_CACHE_MemberAuthGroup ) {
		xvalue* oldCache = G_CACHE_MemberAuthGroup;
		G_CACHE_MemberAuthGroup = arrRet;
		CacheRetire(oldCache);
	} else {
		G_CACHE_MemberAuthGroup = arrRet;
	}
}



// ==================== 前台用户组权限缓�?====================

// 前台路由权限分类回调 - 用于构建用户组的 URI 权限字典
/* F6：单次遍历收集前台 AuthID -> URI 清单（与后台 Auth_ReloadCache 同构）。 */
static bool MemberAuthCollectURIProc(xbytesview key, RouteInfo* pInfo, void* pArg)
{
	xvalue* mapAuthURIs = (xvalue*)pArg;
	char sKey[32];
	int iKeyLen;
	xvalue* arrURIs;
	if ( pInfo->bAdmin || !pInfo->bAuth || (pInfo->AuthID <= 0) ) {
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

// 重新加载前台用户组权限缓�?
// G_CACHE_MemberGroup: 用户组列表缓�?- 前端 ComboBox �?
// G_CACHE_MemberGroupAuth: 用户组权限缓�?- 后端鉴权查表�?
void MemberAuth_ReloadCache()
{
	xvalue* mapAuthURIs = XAdminCreateSharedTableValue();
	MapWalk(G_StaticRouteTableHTTP, (MapWalkProc)MemberAuthCollectURIProc, mapAuthURIs);
	// ʹ��Ԥ��������ѯ����
	xvalue* arrRet = XAdminCreateSharedArrayValue();	// �û����б�
	xvalue* lstRet = XAdminCreateSharedArrayValue();	// �û���Ȩ��ӳ���
	xvalue* idxRet = XAdminCreateSharedListValue();	// group id -> array index
	xvalue* lvlRet = XAdminCreateSharedListValue();	// group id -> authLevel
	
while ( sqlite3_step(stmt_cache_mgroup) == SQLITE_ROW ) {
		// 添加到列表缓�?
		xvalue* tblRow = XAdminCreateSharedTableValue();
		int64 id = sqlite3_column_int64(stmt_cache_mgroup, 0);
		str name = (str)sqlite3_column_text(stmt_cache_mgroup, 1);
		int64 authLevel = sqlite3_column_int64(stmt_cache_mgroup, 4);
		ValueSetInt(tblRow, "id", id);
		ValueSetText(tblRow, "name", name);
		ValueSetInt(tblRow, "authLevel", authLevel);
		ValueArrayOwn(arrRet, tblRow);
		
		// 解析权限分组列表
		xvalue* listAuth = NULL;
		str sAuthList = (str)sqlite3_column_text(stmt_cache_mgroup, 3);
		if ( sAuthList && (strlen(sAuthList) > 2) ) {
			xvalue* arrAuth = JsonParseN(sAuthList, 0);
			if ( arrAuth && (xrtValueType(arrAuth) == XVALUE_ARRAY) ) {
				listAuth = arrAuth;
			} else if ( arrAuth ) {
				xrtValueRelease(arrAuth);
			}
		}
		
		// 构建对应用户组的 URI 权限字典（F6：按 authList 点取）
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
		
		// 权限字典添加元数�?
		ValueSetInt(tblURI, "id", id);
		ValueSetText(tblURI, "name", name);
		ValueSetInt(tblURI, "authLevel", authLevel);
		ValueSetInt(tblURI, "__id__", id);
		ValueSetText(tblURI, "__name__", name);
		ValueSetInt(tblURI, "__authLevel__", authLevel);
		
		// 将整理好的权限字典添加到缓存�?
		ValueArrayOwn(lstRet, tblURI);
		ValueMapSetInt(idxRet, id, xrtValueCount(lstRet));
		ValueMapSetInt(lvlRet, id, authLevel);
	}
	sqlite3_reset(stmt_cache_mgroup);
	XAdminValuePublishShared(arrRet);
	XAdminValuePublishShared(lstRet);
	XAdminValuePublishShared(idxRet);
	XAdminValuePublishShared(lvlRet);
	xrtValueRelease(mapAuthURIs);
	
	// 替换全局缓存 - 用户组列表（线程安全写法�?
	if ( G_CACHE_MemberGroup ) {
		xvalue* oldCache = G_CACHE_MemberGroup;
		G_CACHE_MemberGroup = arrRet;
		CacheRetire(oldCache);
	} else {
		G_CACHE_MemberGroup = arrRet;
	}
	
	// 替换全局缓存 - 用户组权限表（线程安全写法）
	if ( G_CACHE_MemberGroupAuth ) {
		xvalue* oldCache = G_CACHE_MemberGroupAuth;
		G_CACHE_MemberGroupAuth = lstRet;
		CacheRetire(oldCache);
	} else {
		G_CACHE_MemberGroupAuth = lstRet;
	}
	if ( G_CACHE_MemberGroupAuthIndex ) {
		xvalue* oldCache = G_CACHE_MemberGroupAuthIndex;
		G_CACHE_MemberGroupAuthIndex = idxRet;
		CacheRetire(oldCache);
	} else {
		G_CACHE_MemberGroupAuthIndex = idxRet;
	}
	if ( G_CACHE_MemberGroupAuthLevel ) {
		xvalue* oldCache = G_CACHE_MemberGroupAuthLevel;
		G_CACHE_MemberGroupAuthLevel = lvlRet;
		CacheRetire(oldCache);
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
		RouteInfo* pInfo = xrtMapGet(G_StaticRouteTableHTTP, KeyViewN(uri, iSize));
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
		xrtValueRelease(G_CACHE_MemberGroupAuth);
		G_CACHE_MemberGroupAuth = NULL;
	}
	if ( G_CACHE_MemberGroupAuthIndex ) {
		xrtValueRelease(G_CACHE_MemberGroupAuthIndex);
		G_CACHE_MemberGroupAuthIndex = NULL;
	}
	if ( G_CACHE_MemberGroupAuthLevel ) {
		xrtValueRelease(G_CACHE_MemberGroupAuthLevel);
		G_CACHE_MemberGroupAuthLevel = NULL;
	}
	if ( G_CACHE_MemberAuth ) {
		xrtValueRelease(G_CACHE_MemberAuth);
		G_CACHE_MemberAuth = NULL;
	}
	if ( G_CACHE_MemberAuthGroup ) {
		xrtValueRelease(G_CACHE_MemberAuthGroup);
		G_CACHE_MemberAuthGroup = NULL;
	}
	if ( G_CACHE_MemberGroup ) {
		xrtValueRelease(G_CACHE_MemberGroup);
		G_CACHE_MemberGroup = NULL;
	}
}


