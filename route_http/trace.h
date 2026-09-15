/* v1 调试跟踪接口，纯 JSON 输出（v1 无对应页面资产）。
 * 适配点：v1 的 G_AdminSession 表即本代 G_AdminSessions；会话 dump 同时给出
 * 前台 G_MemberSessions（v1 只输出后台）；G_Install 安装向导标志在本代映射为
 * G_Ready（主库必须预先存在，业务层初始化完成后才置位）；路由 dump 追加本代
 * 新增的动态 pattern 条目。字段名与筛选参数保持 v1。 */
static void TraceReplyJSON(XS_ResponseObject objResp, xvalue* tblRet)
{
	size_t iRetSize = 0;
	char* sRet = xrtJsonStringify(tblRet, false, &iRetSize);
	xsHttpReplyAuto(objResp, 200, HTTP_CT_JSON, sRet, iRetSize);
	xrtFree(sRet);
}
static bool TraceMethodGet(XS_RequestObject objReq)
{
	return xsReqMethodID(objReq) == XHTTP_METHOD_GET;
}

/* M4：dump 脱敏。原样倾倒会话表会泄露在线 XID（直接可冒充 Cookie），
 * 配置缓存会泄露 smtp_password/cp_url 等口令。
 * 注意 xrtValueClone 是 COW 共享外壳——克隆后变异会污染原对象（实测会
 * 改写活会话的 xid 导致 logout 失效）。因此这里对源结构**只读遍历**，
 * 逐节点重建全新快照，敏感字符串在重建时打码，绝不触碰原值。 */
static bool Trace_ShouldMask(const char* name, bool sessionMode)
{
	if (sessionMode) return strcmp(name, "xid") == 0;
	{
		char lower[40];
		size_t k;
		for (k = 0; name[k] && k < sizeof(lower) - 1; k++)
			lower[k] = (name[k] >= 'A' && name[k] <= 'Z') ? (char)(name[k] + 32) : name[k];
		lower[k] = '\0';
		return (strstr(lower, "pass") != NULL || strstr(lower, "pwd") != NULL
			|| strstr(lower, "secret") != NULL || strstr(lower, "token") != NULL
			|| strcmp(lower, "cp_url") == 0);
	}
}

static xvalue* Trace_MaskedString(const xvalue* src)
{
	xstrview text;
	char buf[48];
	size_t keep, rest, j;
	if (!xrtValueGetString(src, &text)) return NULL;
	keep = text.Size > 6 ? 6 : 0;
	rest = text.Size - keep;
	if (rest > 24) rest = 24;
	if (keep) memcpy(buf, text.Data, keep);
	for (j = 0; j < rest; j++) buf[keep + j] = '*';
	buf[keep + rest] = '\0';
	return xrtValueString(xrtStrView(buf));
}

static xvalue* Trace_SnapshotMasked(const xvalue* src, bool sessionMode)
{
	xvalueiter it = {0};
	xvaluekey key;
	xvalue* child;
	xvaluetype type;

	if (!src) return NULL;
	type = xrtValueType(src);
	if (type == XVALUE_OBJECT) {
		xvalue* obj = xrtValueObject();
		if (!obj) return NULL;
		if (xrtValueIterBegin(src, &it)) {
			while ((child = xrtValueIterNext(&it, &key)) != NULL) {
				char name[40];
				size_t n = key.String.Size < sizeof(name) - 1 ? key.String.Size : sizeof(name) - 1;
				memcpy(name, key.String.Data, n);
				name[n] = '\0';
				if (Trace_ShouldMask(name, sessionMode) && xrtValueType(child) == XVALUE_STRING) {
					xvalue* masked = Trace_MaskedString(child);
					if (masked) xrtValueObjectSetNew(obj, key.String, masked);
				} else {
					xvalue* rebuilt = Trace_SnapshotMasked(child, sessionMode);
					if (rebuilt) xrtValueObjectSetNew(obj, key.String, rebuilt);
				}
			}
			xrtValueIterEnd(&it);
		}
		return obj;
	}
	if (type == XVALUE_ARRAY) {
		xvalue* arr = xrtValueArray();
		size_t i, count = xrtValueCount(src);
		if (!arr) return NULL;
		for (i = 0; i < count; i++) {
			xvalue* rebuilt = Trace_SnapshotMasked(xrtValueArrayGet(src, i), sessionMode);
			if (rebuilt) xrtValueArrayAppendNew(arr, rebuilt);
		}
		return arr;
	}
	/* 标量：按类型重建（无变异、无共享） */
	{
		xstrview text;
		bool b;
		int64 i64;
		double f;
		switch (type) {
		case XVALUE_STRING:
			if (!xrtValueGetString(src, &text)) return NULL;
			return xrtValueString(text);
		case XVALUE_BOOL:
			if (!xrtValueGetBool(src, &b)) return NULL;
			return xrtValueBool(b);
		case XVALUE_INT:
			if (!xrtValueGetInt(src, &i64)) return NULL;
			return xrtValueInt(i64);
		case XVALUE_FLOAT:
			if (!xrtValueGetFloat(src, &f)) return NULL;
			return xrtValueFloat(f);
		default:
			return xrtValueNull();
		}
	}
}

// 获取全局缓存概览
void Request_Trace_Overview(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !TraceMethodGet(objReq) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	xvalue* tblRet = ValueObject();
	ValueSetBool(tblRet, "result", true);

	xvalue* tblData = ValueObject();

	// Session 缓存信息 - 前后台各一份
	xvalue* tblSession = ValueObject();
	ValueSetBool(tblSession, "exists", G_AdminSessions != NULL);
	if ( G_AdminSessions != NULL ) {
		ValueSetInt(tblSession, "count", ValueCount(G_AdminSessions));
	}
	ValueSetOwn(tblData, "adminSession", tblSession);
	tblSession = ValueObject();
	ValueSetBool(tblSession, "exists", G_MemberSessions != NULL);
	if ( G_MemberSessions != NULL ) {
		ValueSetInt(tblSession, "count", ValueCount(G_MemberSessions));
	}
	ValueSetOwn(tblData, "memberSession", tblSession);

	// Option 缓存信息
	xvalue* tblOption = ValueObject();
	ValueSetBool(tblOption, "exists", G_Option != NULL);
	if ( G_Option != NULL ) {
		ValueSetInt(tblOption, "count", ValueCount(G_Option));
	}
	ValueSetOwn(tblData, "option", tblOption);

	// 权限缓存信息
	xvalue* tblAuth = ValueObject();
	ValueSetBool(tblAuth, "roleAuth_exists", G_CACHE_RoleAuth != NULL);
	ValueSetBool(tblAuth, "auth_exists", G_CACHE_Auth != NULL);
	ValueSetBool(tblAuth, "group_exists", G_CACHE_Group != NULL);
	ValueSetBool(tblAuth, "role_exists", G_CACHE_Role != NULL);
	if ( G_CACHE_RoleAuth != NULL ) {
		if ( xrtValueType(G_CACHE_RoleAuth) == XVALUE_ARRAY ) {
			ValueSetInt(tblAuth, "roleAuth_count", ValueCount(G_CACHE_RoleAuth));
		} else if ( xrtValueType(G_CACHE_RoleAuth) == XVALUE_OBJECT ) {
			ValueSetInt(tblAuth, "roleAuth_count", ValueCount(G_CACHE_RoleAuth));
		} else if ( xrtValueType(G_CACHE_RoleAuth) == XVALUE_INT_MAP ) {
			ValueSetInt(tblAuth, "roleAuth_count", ValueCount(G_CACHE_RoleAuth));
		}
	}
	if ( G_CACHE_Auth != NULL ) {
		ValueSetInt(tblAuth, "auth_count", ValueCount(G_CACHE_Auth));
	}
	if ( G_CACHE_Group != NULL ) {
		ValueSetInt(tblAuth, "group_count", ValueCount(G_CACHE_Group));
	}
	if ( G_CACHE_Role != NULL ) {
		ValueSetInt(tblAuth, "role_count", ValueCount(G_CACHE_Role));
	}
	ValueSetOwn(tblData, "auth", tblAuth);

	// 路由表信息
	xvalue* tblRoute = ValueObject();
	ValueSetBool(tblRoute, "exists", G_StaticRouteTableHTTP != NULL);
	if ( G_StaticRouteTableHTTP != NULL ) {
		ValueSetInt(tblRoute, "count", xrtMapCount(G_StaticRouteTableHTTP));
	}
	ValueSetOwn(tblData, "route", tblRoute);

	// 数据库连接信息
	xvalue* tblDB = ValueObject();
	ValueSetBool(tblDB, "connected", G_DB != NULL);
	ValueSetOwn(tblData, "database", tblDB);

	// 安装状态（本代无向导；G_Ready 在业务层初始化完成后才置位，等价于已安装）
	ValueSetBool(tblData, "installed", G_Ready);

	ValueSetOwn(tblRet, "data", tblData);
	TraceReplyJSON(objResp, tblRet);
	xrtValueRelease(tblRet);
}

// 获取前后台 Session 缓存数据
void Request_Trace_Session(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !TraceMethodGet(objReq) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	xvalue* tblRet = ValueObject();

	if ( (G_AdminSessions == NULL) || (G_MemberSessions == NULL) ) {
		ValueSetBool(tblRet, "result", false);
		ValueSetText(tblRet, "message", "Session 缓存不存在");
	} else {
		ValueSetBool(tblRet, "result", true);
		xvalue* tblData = ValueObject();
		/* M4：XID 打码的只读重建快照（COW 克隆不可变异） */
		{
			xvalue* adminSnap = Trace_SnapshotMasked(G_AdminSessions, true);
			xvalue* memberSnap = Trace_SnapshotMasked(G_MemberSessions, true);
			ValueSetOwn(tblData, "admin", adminSnap ? adminSnap : ValueObject());
			ValueSetOwn(tblData, "member", memberSnap ? memberSnap : ValueObject());
		}
		ValueSetOwn(tblRet, "data", tblData);
	}
	TraceReplyJSON(objResp, tblRet);
	xrtValueRelease(tblRet);
}

// 获取 Option 缓存数据
void Request_Trace_Option(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !TraceMethodGet(objReq) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	xvalue* tblRet = ValueObject();

	if ( G_Option == NULL ) {
		ValueSetBool(tblRet, "result", false);
		ValueSetText(tblRet, "message", "Option 缓存不存在");
	} else {
		ValueSetBool(tblRet, "result", true);
		/* M4：口令类配置值打码的只读重建快照 */
		{
			xvalue* optionSnap = Trace_SnapshotMasked(G_Option, false);
			ValueSetOwn(tblRet, "data", optionSnap ? optionSnap : ValueObject());
		}
	}
	TraceReplyJSON(objResp, tblRet);
	xrtValueRelease(tblRet);
}

// 获取权限相关缓存数据
void Request_Trace_Auth(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !TraceMethodGet(objReq) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	// 获取类型参数
	char sType[32];
	int iSize = xsReqQueryValue(objReq, "type", sType, sizeof(sType));
	if ( iSize <= 0 ) {
		strcpy(sType, "all");
	}

	xvalue* tblRet = ValueObject();
	ValueSetBool(tblRet, "result", true);

	xvalue* tblData = ValueObject();

	if ( (strcmp(sType, "all") == 0) || (strcmp(sType, "roleAuth") == 0) ) {
		if ( G_CACHE_RoleAuth != NULL ) {
			xvalue* tblRoleAuth = ValueObject();
			xvalue* objRole1 = XAdminIDCacheGetValue(G_CACHE_RoleAuth, 1);
			xvalue* objRole2 = XAdminIDCacheGetValue(G_CACHE_RoleAuth, 2);
			xvalue* objItem0 = (xrtValueType(G_CACHE_RoleAuth) == XVALUE_ARRAY && xrtValueCount(G_CACHE_RoleAuth) > 0) ? xrtValueArrayGet(G_CACHE_RoleAuth, 0) : NULL;
			xvalue* objItem1 = (xrtValueType(G_CACHE_RoleAuth) == XVALUE_ARRAY && xrtValueCount(G_CACHE_RoleAuth) > 1) ? xrtValueArrayGet(G_CACHE_RoleAuth, 1) : NULL;
			ValueSetInt(tblRoleAuth, "cacheType", xrtValueType(G_CACHE_RoleAuth));
			if ( xrtValueType(G_CACHE_RoleAuth) == XVALUE_ARRAY ) {
				ValueSetInt(tblRoleAuth, "count", ValueCount(G_CACHE_RoleAuth));
			} else if ( xrtValueType(G_CACHE_RoleAuth) == XVALUE_OBJECT ) {
				ValueSetInt(tblRoleAuth, "count", ValueCount(G_CACHE_RoleAuth));
			} else if ( xrtValueType(G_CACHE_RoleAuth) == XVALUE_INT_MAP ) {
				ValueSetInt(tblRoleAuth, "count", ValueCount(G_CACHE_RoleAuth));
			}
			ValueSetBool(tblRoleAuth, "role1_exists", objRole1 != NULL);
			ValueSetBool(tblRoleAuth, "item0_exists", objItem0 != NULL);
			ValueSetBool(tblRoleAuth, "item1_exists", objItem1 != NULL);
			ValueSetBool(tblRoleAuth, "role2_exists", objRole2 != NULL);
			if ( objRole1 != NULL ) {
				ValueSetInt(tblRoleAuth, "role1_type", xrtValueType(objRole1));
				if ( xrtValueType(objRole1) == XVALUE_OBJECT ) {
					ValueSetInt(tblRoleAuth, "role1_id", ValueInt(objRole1, "id"));
				}
			}
			if ( objRole2 != NULL ) {
				ValueSetInt(tblRoleAuth, "role2_type", xrtValueType(objRole2));
				if ( xrtValueType(objRole2) == XVALUE_OBJECT ) {
					ValueSetInt(tblRoleAuth, "role2_id", ValueInt(objRole2, "id"));
				}
			}
			if ( objItem0 != NULL ) {
				ValueSetInt(tblRoleAuth, "item0_type", xrtValueType(objItem0));
				if ( xrtValueType(objItem0) == XVALUE_OBJECT ) {
					ValueSetInt(tblRoleAuth, "item0_id", ValueInt(objItem0, "id"));
				}
			}
			if ( objItem1 != NULL ) {
				ValueSetInt(tblRoleAuth, "item1_type", xrtValueType(objItem1));
				if ( xrtValueType(objItem1) == XVALUE_OBJECT ) {
					ValueSetInt(tblRoleAuth, "item1_id", ValueInt(objItem1, "id"));
				}
			}
			ValueSetOwn(tblData, "roleAuth", tblRoleAuth);
		}
	}

	if ( (strcmp(sType, "all") == 0) || (strcmp(sType, "auth") == 0) ) {
		if ( G_CACHE_Auth != NULL ) {
			xrtValueRetain(G_CACHE_Auth);
			ValueSetOwn(tblData, "auth", G_CACHE_Auth);
		}
	}

	if ( (strcmp(sType, "all") == 0) || (strcmp(sType, "group") == 0) ) {
		if ( G_CACHE_Group != NULL ) {
			xrtValueRetain(G_CACHE_Group);
			ValueSetOwn(tblData, "group", G_CACHE_Group);
		}
	}

	if ( (strcmp(sType, "all") == 0) || (strcmp(sType, "role") == 0) ) {
		if ( G_CACHE_Role != NULL ) {
			xrtValueRetain(G_CACHE_Role);
			ValueSetOwn(tblData, "role", G_CACHE_Role);
		}
	}

	ValueSetOwn(tblRet, "data", tblData);
	TraceReplyJSON(objResp, tblRet);
	xrtValueRelease(tblRet);
}

// 路由表遍历回调函数
static bool TraceRouteWalkProc(xbytesview key, RouteInfo* pInfo, void* pArg)
{
	xvalue* arrRoutes = (xvalue*)pArg;
	if ( pInfo ) {
		xvalue* tblRoute = ValueObject();
		xrtValueObjectSetNew(tblRoute, xrtStrView("uri"), xrtValueString(xrtStrViewN((const char*)key.Data, key.Size)));
		ValueSetBool(tblRoute, "log", pInfo->bPutLog);
		ValueSetBool(tblRoute, "auth", pInfo->bAuth);
		ValueSetInt(tblRoute, "authId", pInfo->AuthID);
		ValueArrayOwn(arrRoutes, tblRoute);
	}
	return false;
}

// 获取路由表数据（静态表 + 本代动态 pattern）
void Request_Trace_Route(XS_ServerObject objServer, XS_HostObject objHost, XS_RequestObject objReq, XS_ResponseObject objResp, xvalue* objSession)
{
	(void)objServer;
	(void)objHost;
	(void)objSession;

	if ( !TraceMethodGet(objReq) ) {
		LoadPage(objResp, 404, HTTP_CT_HTML, "status/404.html");
		return;
	}

	xvalue* tblRet = ValueObject();

	if ( G_StaticRouteTableHTTP == NULL ) {
		ValueSetBool(tblRet, "result", false);
		ValueSetText(tblRet, "message", "路由表不存在");
	} else {
		ValueSetBool(tblRet, "result", true);

		xvalue* arrRoutes = ValueArray();
		MapWalk(G_StaticRouteTableHTTP, (MapWalkProc)TraceRouteWalkProc, arrRoutes);
		for ( size_t i = 0; i < G_DynamicCount; i++ ) {
			xvalue* tblRoute = ValueObject();
			ValueSetText(tblRoute, "uri", (str)G_DynamicRoutes[i].Path);
			ValueSetBool(tblRoute, "log", G_DynamicRoutes[i].bPutLog);
			ValueSetBool(tblRoute, "auth", G_DynamicRoutes[i].bAuth);
			ValueSetInt(tblRoute, "authId", G_DynamicRoutes[i].AuthID);
			ValueArrayOwn(arrRoutes, tblRoute);
		}
		ValueSetOwn(tblRet, "data", arrRoutes);
	}
	TraceReplyJSON(objResp, tblRet);
	xrtValueRelease(tblRet);
}
