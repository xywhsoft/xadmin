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
	return !session || xrtValueType(session) != XVALUE_OBJECT || xrtNow() > ValueInt(session, "_expireTime");
}
/* R4 上限执行与 R3 保留式撤销在 Store 之后定义，此处前置声明。 */
static void Session_EnforceAccountCap(xvalue* sessions, xvalue* session);
static void Session_RevokeAccountExcept(bool admin, int64 account_id, xvalue* keep);

static xvalue* Session_Create(int seconds)
{
	xvalue* value = xrtValueObject(); int64 now = xrtNow();
	ValueSetInt(value, "_createTime", now);
	ValueSetInt(value, "_activeTime", now);
	ValueSetInt(value, "_expireTime", now + (int64)seconds * 1000000);
	return value;
}
static xvalue* Session_CreateAdmin(const char* id) { (void)id; return Session_Create(SESSION_EXPIRE_ADMIN); }
static xvalue* Session_CreateMember(const char* id) { (void)id; return Session_Create(SESSION_EXPIRE_MEMBER); }
static bool Session_StoreAdmin(const char* id, xvalue* session) { bool ok = id && *id && session && xrtValueObjectSet(G_AdminSessions, xrtStrView(id), session); if (ok) Session_EnforceAccountCap(G_AdminSessions, session); return ok; }
static bool Session_StoreMember(const char* id, xvalue* session) { bool ok = id && *id && session && xrtValueObjectSet(G_MemberSessions, xrtStrView(id), session); if (ok) Session_EnforceAccountCap(G_MemberSessions, session); return ok; }
static void Session_RemoveAdminByID(const char* id) { if (id) xrtValueObjectRemove(G_AdminSessions, xrtStrView(id)); }
static void Session_RemoveMemberByID(const char* id) { if (id) xrtValueObjectRemove(G_MemberSessions, xrtStrView(id)); }
/* R4：同账号会话数上限；超出按 _activeTime 踢最旧（登录即续期，
 * 最旧即最久未活跃）。固定上限 5，兼容多端登录与既有测试的双会话。 */
#define SESSION_PER_ACCOUNT 5
static size_t Session_CountAccount(xvalue* sessions, int64 account)
{
	xvalueiter it = {0}; xvaluekey key; xvalue* session; size_t count = 0;
	if (xrtValueIterBegin(sessions, &it)) {
		while ((session = xrtValueIterNext(&it, &key)))
			if (ValueInt(session, "id") == account) count++;
		xrtValueIterEnd(&it);
	}
	return count;
}
static void Session_DropOldestAccount(xvalue* sessions, int64 account)
{
	xvalueiter it = {0}; xvaluekey key; xvalue* session;
	str sOldest = NULL; int64 iOldestActive = 0;
	if (xrtValueIterBegin(sessions, &it)) {
		while ((session = xrtValueIterNext(&it, &key))) {
			if (ValueInt(session, "id") != account) continue;
			{
				int64 iActive = ValueInt(session, "_activeTime");
				if ((sOldest == NULL) || (iActive < iOldestActive)) {
					if (sOldest) xrtFree(sOldest);
					sOldest = xrtStrDupN(key.String.Data, key.String.Size);
					iOldestActive = iActive;
				}
			}
		}
		xrtValueIterEnd(&it);
	}
	if (sOldest) {
		xrtValueObjectRemove(sessions, xrtStrView(sOldest));
		xrtFree(sOldest);
	}
}
static void Session_EnforceAccountCap(xvalue* sessions, xvalue* session)
{
	int64 account = ValueInt(session, "id");
	if (account <= 0) return;
	while (Session_CountAccount(sessions, account) > SESSION_PER_ACCOUNT)
		Session_DropOldestAccount(sessions, account);
}

static void Session_Extend(xvalue* session, int seconds)
{
	int64 now = xrtNow();
	ValueSetInt(session, "_activeTime", now);
	ValueSetInt(session, "_expireTime", now + (int64)seconds * 1000000);
}
static void Session_ExtendAdmin(xvalue* value) { Session_Extend(value, SESSION_EXPIRE_ADMIN); }
static void Session_ExtendMember(xvalue* value) { Session_Extend(value, SESSION_EXPIRE_MEMBER); }
static xvalue* Session_Acquire(bool admin, const char* id)
{
	xvalue* map = admin ? G_AdminSessions : G_MemberSessions;
	xvalue* value = xrtValueObjectGet(map, xrtStrView(id));
	if (value && (Session_IsExpired(value) || (admin && !XA_MFAValidSession(true,ValueInt(value,"id"),ValueInt(value,"mfa_version"),ValueInt(value,"mfa_verified_at"))))) { xrtValueObjectRemove(map, xrtStrView(id)); value = NULL; }
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
/* R3：改密/重置成功后撤销该账号的其他会话，保留当前请求持有的会话。 */
static void Session_RevokeAccountExcept(bool admin, int64 account_id, xvalue* keep)
{
	if (!admin) XA_SessionRevokeAccount(account_id,keep?ValueText(keep,"sid"):NULL);
	xvalue* sessions = admin ? G_AdminSessions : G_MemberSessions;
	xvalueiter it = {0}; xvaluekey key; xvalue* session;
	if (account_id <= 0) return;
	if (xrtValueIterBegin(sessions, &it)) {
		while ((session = xrtValueIterNext(&it, &key))) {
			if (session != keep && ValueInt(session, "id") == account_id)
				ValueSetInt(session, "_expireTime", -1);
		}
		xrtValueIterEnd(&it);
	}
	Session_Prune(sessions);
}

/* 删除账号成功后撤销该账号的所有登录。先标记再清理，避免遍历时删除键；
 * 即使清理临时分配失败，Acquire 也会拒绝已标记会话。当前请求仍持有引用。 */
static void Session_RevokeAccount(bool admin, int64 account_id)
{
	if (!admin) XA_SessionRevokeAccount(account_id,NULL);
	xvalue* sessions = admin ? G_AdminSessions : G_MemberSessions;
	xvalueiter it = {0}; xvaluekey key; xvalue* session;
	if (account_id <= 0) return;
	if (xrtValueIterBegin(sessions, &it)) {
		while ((session = xrtValueIterNext(&it, &key))) {
			if (ValueInt(session, "id") == account_id)
				ValueSetInt(session, "_expireTime", -1);
		}
		xrtValueIterEnd(&it);
	}
	Session_Prune(sessions);
}
/* Role levels are copied into login sessions; revoke those snapshots after a
 * successful level change, including the current request's retained session. */
static void Session_RevokeRole(int64 role_id)
{
	xvalueiter it = {0}; xvaluekey key; xvalue* session;
	if (role_id <= 0) return;
	if (xrtValueIterBegin(G_AdminSessions, &it)) {
		while ((session = xrtValueIterNext(&it, &key))) {
			if (ValueInt(session, "roleID") == role_id)
				ValueSetInt(session, "_expireTime", -1);
		}
		xrtValueIterEnd(&it);
	}
	Session_Prune(G_AdminSessions);
}
static void Session_Tick(void* unused)
{
	(void)unused;
	xrtMutexLock(G_RequestLock);
	Session_Prune(G_AdminSessions); Session_Prune(G_MemberSessions);
	XA_SessionMaintenance();
	CacheRetireSweep();
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
/* F3：会员 MSID 与后台 XSID 同一强化等级；此前手拼头缺 SameSite。 */
static char* Session_MemberHeaders(XAdminRequest* req, const char* id, int max_age, const char* location)
{
	char age[48] = {0};
	if (max_age >= 0) snprintf(age, sizeof(age), "; Max-Age=%d", max_age);
	return xrtFormat("%sSet-Cookie: MSID=%s; Path=/; HttpOnly; SameSite=Lax%s%s\r\n%s%s%s",
		HTTP_CT_JSON, id ? id : "", req->raw->tls ? "; Secure" : "", age,
		location ? "Location: " : "", location ? location : "", location ? "\r\n" : "");
}
