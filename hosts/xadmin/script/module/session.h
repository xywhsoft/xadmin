


// Session 过期时间设置（单位：秒）
#define SESSION_EXPIRE_ADMIN    7200    // 后台 Session 过期时间：2小时
#define SESSION_EXPIRE_MEMBER   86400   // 前台 Session 过期时间：24小时



static void Session_PublishDict(xdict dict)
{
	if ( dict ) {
		xrtOwnerActivateShared(&dict->Owner);
		xrtOwnerActivateShared(&dict->AVLT.Owner);
	}
}

static void Session_PublishTable(xvalue session)
{
	if ( session && (session->Type == XVO_DT_TABLE) ) {
		Session_PublishDict(session->vTable);
		xrtOwnerActivateShared(&session->vTable->Owner);
		xvoSetShared_Inline(session);
	}
}

static bool Session_UnrefMapProc(Dict_Key* pKey, ptr pVal, ptr pArg)
{
	xvalue session = pVal ? *((xvalue*)pVal) : NULL;
	(void)pKey;
	(void)pArg;
	if ( session ) {
		xvoUnref(session);
	}
	return FALSE;
}

static xvalue Session_MapGet(xdict mapSession, str sessionId)
{
	if ( mapSession == NULL || sessionId == NULL || sessionId[0] == '\0' ) {
		return NULL;
	}
	return (xvalue)xrtDictGetPtr(mapSession, sessionId, (uint32)strlen(sessionId));
}

static void Session_MapSet(xdict mapSession, str sessionId, xvalue session)
{
	xvalue oldSession = NULL;
	if ( mapSession == NULL || sessionId == NULL || session == NULL ) {
		return;
	}
	xvoAddRef(session);
	if ( !xrtDictSetPtr(mapSession, sessionId, (uint32)strlen(sessionId), session, (ptr*)&oldSession) ) {
		xvoUnref(session);
		return;
	}
	if ( oldSession ) {
		xvoUnref(oldSession);
	}
}

static void Session_MapRemove(xdict mapSession, str sessionId)
{
	xvalue oldSession;
	if ( mapSession == NULL || sessionId == NULL || sessionId[0] == '\0' ) {
		return;
	}
	oldSession = (xvalue)xrtDictRemovePtr(mapSession, sessionId, (uint32)strlen(sessionId));
	if ( oldSession ) {
		xvoUnref(oldSession);
	}
}

static xvalue Session_GetAdminByID(str sessionId)
{
	return Session_MapGet(G_AdminSessionMap, sessionId);
}

static xvalue Session_GetMemberByID(str sessionId)
{
	return Session_MapGet(G_MemberSessionMap, sessionId);
}

static void Session_RemoveAdminByID(str sessionId)
{
	Session_MapRemove(G_AdminSessionMap, sessionId);
}

static void Session_RemoveMemberByID(str sessionId)
{
	Session_MapRemove(G_MemberSessionMap, sessionId);
}

static void Session_StoreAdmin(str sessionId, xvalue session)
{
	if ( session && session->Type == XVO_DT_TABLE ) {
		Session_PublishTable(session);
		Session_MapSet(G_AdminSessionMap, sessionId, session);
	}
}

static void Session_StoreMember(str sessionId, xvalue session)
{
	if ( session && session->Type == XVO_DT_TABLE ) {
		Session_PublishTable(session);
		Session_MapSet(G_MemberSessionMap, sessionId, session);
	}
}



// 初始化 Session 模块
void Session_Init()
{
	printf("        Session_Init \n");
	
	// 初始化后台管理员 Session 表
	G_AdminSession = xvoCreateTableEx(XRT_OBJMODE_SHARED);
	if ( G_AdminSession == NULL ) {
		printf("!!! ERROR !!! Create admin sessions table failed !\n");
		exit(1);
	}
	Session_PublishTable(G_AdminSession);
	G_AdminSessionMap = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	if ( G_AdminSessionMap == NULL ) {
		printf("!!! ERROR !!! Create admin sessions map failed !\n");
		exit(1);
	}
	Session_PublishDict(G_AdminSessionMap);
	
	// 初始化前台用户 Session 表
	G_MemberSession = xvoCreateTableEx(XRT_OBJMODE_SHARED);
	if ( G_MemberSession == NULL ) {
		printf("!!! ERROR !!! Create member sessions table failed !\n");
		exit(1);
	}
	Session_PublishTable(G_MemberSession);
	G_MemberSessionMap = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_SHARED);
	if ( G_MemberSessionMap == NULL ) {
		printf("!!! ERROR !!! Create member sessions map failed !\n");
		exit(1);
	}
	Session_PublishDict(G_MemberSessionMap);
}



// 更新 Session 活跃时间
void Session_UpdateActiveTime(xvalue session)
{
	if ( session && session->Type == XVO_DT_TABLE ) {
		xvoTableSetInt(session, "_activeTime", 11, xrtNow());
	}
}



// 创建新的后台 Session 并设置过期时间
xvalue Session_CreateAdmin(str sessionId)
{
	xvalue session = xvoCreateTableEx(XRT_OBJMODE_SHARED);
	(void)sessionId;
	if ( session ) {
		int64 now = xrtNow();
		xvoTableSetInt(session, "_createTime", 11, now);
		xvoTableSetInt(session, "_activeTime", 11, now);
		xvoTableSetInt(session, "_expireTime", 11, now + SESSION_EXPIRE_ADMIN);
	}
	return session;
}



// 创建新的前台 Session 并设置过期时间
xvalue Session_CreateMember(str sessionId)
{
	xvalue session = xvoCreateTableEx(XRT_OBJMODE_SHARED);
	(void)sessionId;
	if ( session ) {
		int64 now = xrtNow();
		xvoTableSetInt(session, "_createTime", 11, now);
		xvoTableSetInt(session, "_activeTime", 11, now);
		xvoTableSetInt(session, "_expireTime", 11, now + SESSION_EXPIRE_MEMBER);
	}
	return session;
}



// 检查 Session 是否过期
bool Session_IsExpired(xvalue session)
{
	if ( !session || session->Type != XVO_DT_TABLE ) {
		return TRUE;
	}
	int64 expireTime = xvoTableGetInt(session, "_expireTime", 11);
	return xrtNow() > expireTime;
}



// 延长 Session 有效期（用于活跃操作）
void Session_ExtendAdmin(xvalue session)
{
	if ( session && session->Type == XVO_DT_TABLE ) {
		int64 now = xrtNow();
		xvoTableSetInt(session, "_activeTime", 11, now);
		xvoTableSetInt(session, "_expireTime", 11, now + SESSION_EXPIRE_ADMIN);
	}
}

void Session_ExtendMember(xvalue session)
{
	if ( session && session->Type == XVO_DT_TABLE ) {
		int64 now = xrtNow();
		xvoTableSetInt(session, "_activeTime", 11, now);
		xvoTableSetInt(session, "_expireTime", 11, now + SESSION_EXPIRE_MEMBER);
	}
}



// 卸载 Session 模块
void Session_Unit()
{
	printf("        Session_Unit \n");
	if ( G_AdminSessionMap ) {
		xrtDictWalk(G_AdminSessionMap, Session_UnrefMapProc, NULL);
		xrtDictDestroy(G_AdminSessionMap);
		G_AdminSessionMap = NULL;
	}
	if ( G_MemberSessionMap ) {
		xrtDictWalk(G_MemberSessionMap, Session_UnrefMapProc, NULL);
		xrtDictDestroy(G_MemberSessionMap);
		G_MemberSessionMap = NULL;
	}
	xvoUnref(G_AdminSession);
	G_AdminSession = NULL;
	xvoUnref(G_MemberSession);
	G_MemberSession = NULL;
}
