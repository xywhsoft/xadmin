


// Session 过期时间设置（单位：秒）
#define SESSION_EXPIRE_ADMIN    7200    // 后台 Session 过期时间：2小时
#define SESSION_EXPIRE_MEMBER   86400   // 前台 Session 过期时间：24小时



// 初始化 Session 模块
void Session_Init()
{
	printf("        Session_Init \n");
	
	// 初始化后台管理员 Session 表
	G_AdminSession = xvoCreateTable();
	if ( G_AdminSession == NULL ) {
		printf("!!! ERROR !!! Create admin sessions table failed !\n");
		exit(1);
	}
	
	// 初始化前台用户 Session 表
	G_MemberSession = xvoCreateTable();
	if ( G_MemberSession == NULL ) {
		printf("!!! ERROR !!! Create member sessions table failed !\n");
		exit(1);
	}
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
	xvalue session = xvoCreateTable();
	if ( session ) {
		int64 now = xrtNow();
		xvoTableSetInt(session, "_createTime", 11, now);
		xvoTableSetInt(session, "_activeTime", 11, now);
		xvoTableSetInt(session, "_expireTime", 11, now + SESSION_EXPIRE_ADMIN);
		xvoTableSetValue(G_AdminSession, sessionId, 32, session, TRUE);
	}
	return session;
}



// 创建新的前台 Session 并设置过期时间
xvalue Session_CreateMember(str sessionId)
{
	xvalue session = xvoCreateTable();
	if ( session ) {
		int64 now = xrtNow();
		xvoTableSetInt(session, "_createTime", 11, now);
		xvoTableSetInt(session, "_activeTime", 11, now);
		xvoTableSetInt(session, "_expireTime", 11, now + SESSION_EXPIRE_MEMBER);
		xvoTableSetValue(G_MemberSession, sessionId, 32, session, TRUE);
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
	xvoUnref(G_AdminSession);
	xvoUnref(G_MemberSession);
}


