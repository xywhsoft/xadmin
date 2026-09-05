/* v1 前后台会话语义不变：独立 Cookie、空闲续期、2h/24h 过期。
 * Map 拥有一份引用，请求再持有一份，注销后当前请求仍能安全收尾。
 * 本模块只在 G_RequestLock 内使用；本阶段会话随脚本代销毁，重载后重新登录。
 * 不导出可能落后于注销操作的会话快照，也不跨代保留 TCC 回调地址。
 */
#define SESSION_EXPIRE_ADMIN 7200
#define SESSION_EXPIRE_MEMBER 86400
static xvalue* G_AdminSessions;
static xvalue* G_MemberSessions;
static XS_HostInfo* G_SessionOwner;
static uint64 G_SessionTimer;
static bool Session_IsExpired(xvalue* session)
{
	return !session || xrtValueType(session) != XVALUE_OBJECT || XA_Now() > xvoTableGetInt(session, "_expireTime", 11);
}
static xvalue* Session_Create(int seconds)
{
	xvalue* value = xrtValueObject(); int64 now = XA_Now();
	xvoTableSetInt(value, "_createTime", 11, now);
	xvoTableSetInt(value, "_activeTime", 11, now);
	xvoTableSetInt(value, "_expireTime", 11, now + seconds);
	return value;
}
static xvalue* Session_CreateAdmin(const char* id) { (void)id; return Session_Create(SESSION_EXPIRE_ADMIN); }
static xvalue* Session_CreateMember(const char* id) { (void)id; return Session_Create(SESSION_EXPIRE_MEMBER); }
static bool Session_StoreAdmin(const char* id, xvalue* session) { return id && *id && session && xrtValueObjectSet(G_AdminSessions, xrtStrView(id), session); }
static bool Session_StoreMember(const char* id, xvalue* session) { return id && *id && session && xrtValueObjectSet(G_MemberSessions, xrtStrView(id), session); }
static void Session_RemoveAdminByID(const char* id) { if (id) xrtValueObjectRemove(G_AdminSessions, xrtStrView(id)); }
static void Session_RemoveMemberByID(const char* id) { if (id) xrtValueObjectRemove(G_MemberSessions, xrtStrView(id)); }
static void Session_Extend(xvalue* session, int seconds)
{
	int64 now = XA_Now();
	xvoTableSetInt(session, "_activeTime", 11, now);
	xvoTableSetInt(session, "_expireTime", 11, now + seconds);
}
static void Session_ExtendAdmin(xvalue* value) { Session_Extend(value, SESSION_EXPIRE_ADMIN); }
static void Session_ExtendMember(xvalue* value) { Session_Extend(value, SESSION_EXPIRE_MEMBER); }
static xvalue* Session_Acquire(bool admin, const char* id)
{
	xvalue* map = admin ? G_AdminSessions : G_MemberSessions;
	xvalue* value = xrtValueObjectGet(map, xrtStrView(id));
	if (value && Session_IsExpired(value)) { xrtValueObjectRemove(map, xrtStrView(id)); value = NULL; }
	return value ? xrtValueRetain(value) : xrtValueNull();
}
/* 先收集键，再删除；遍历期间不修改被遍历的容器。每五分钟清理无人再访问的过期项。 */
static void Session_Prune(xvalue* sessions)
{
	xvalueiter it = {0}; xvaluekey key; xvalue* value; xvalue* expired = xrtValueArray(); size_t i;
	if (!expired) return;
	if (xrtValueIterBegin(sessions, &it)) {
		while ((value = xrtValueIterNext(&it, &key))) if (Session_IsExpired(value))
			xrtValueArrayAppendNew(expired, xrtValueString(key.String));
		xrtValueIterEnd(&it);
	}
	for (i = 0; i < xrtValueCount(expired); i++) {
		xstrview name;
		if (xrtValueGetString(xrtValueArrayGet(expired, i), &name)) xrtValueObjectRemove(sessions, name);
	}
	xrtValueRelease(expired);
}
static void Session_Tick(void* unused)
{
	(void)unused;
	xrtMutexLock(G_RequestLock);
	Session_Prune(G_AdminSessions); Session_Prune(G_MemberSessions);
	G_SessionTimer = xsTimerAfter(G_SessionOwner, 300000, Session_Tick, NULL);
	xrtMutexUnlock(G_RequestLock);
}
static void Session_StartTimer(XS_HostInfo* host)
{
	G_SessionOwner = host;
	G_SessionTimer = xsTimerAfter(host, 300000, Session_Tick, NULL);
}
static bool Session_Init(void)
{
	G_AdminSessions = xrtValueObject(); G_MemberSessions = xrtValueObject();
	return G_AdminSessions && G_MemberSessions;
}
static void Session_Unit(void)
{
	if (G_SessionTimer) xsTimerCancel(G_SessionTimer);
	G_SessionTimer = 0; G_SessionOwner = NULL;
	xrtValueRelease(G_AdminSessions); xrtValueRelease(G_MemberSessions);
	G_AdminSessions = G_MemberSessions = NULL;
}
/* 后台同源页面使用 Lax；HTTPS 才设置 Secure。保留 v1 的记住登录期限。
 * 会员 API 的跨站 Cookie 策略本阶段不改；完整 CSRF 防护另行单独回归。 */
static char* Session_AdminHeaders(XAdminRequest* req, const char* id, int max_age, const char* location)
{
	char age[48] = {0};
	if (max_age >= 0) snprintf(age, sizeof(age), "; Max-Age=%d", max_age);
	return xrtFormat("%sSet-Cookie: XSID=%s; Path=/; HttpOnly; SameSite=Lax%s%s\r\n%s%s%s",
		HTTP_CT_JSON, id ? id : "", req->raw->tls ? "; Secure" : "", age,
		location ? "Location: " : "", location ? location : "", location ? "\r\n" : "");
}
