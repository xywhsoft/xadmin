

// brute-force guard
typedef struct {
	xtime CoolDown;
	int FailCount;
	int TimeRate;
} GuardInfo;
/* F4：前后台分域计数，爆破会员 API 不再锁定同 IP 的后台登录。 */
static xmap* G_GuardAdmin = NULL;
static xmap* G_GuardMember = NULL;

// after how many failures enter cooldown
#define BRUTE_COUNT 5

// initial cooldown seconds
#define BRUTE_TIMES 300

static str Guard_RemoteKey(str sRemote)
{
	if ( sRemote == NULL || sRemote[0] == '\0' ) {
		return "(unknown)";
	}

	return sRemote;
}

xtime Guard_Check(xmap* dictGuard, str sRemote)
{
	GuardInfo* pInfo;
	bool bNew = false;
	str sKey = Guard_RemoteKey(sRemote);

	pInfo = xrtMapGetOrAdd(dictGuard, KeyView(sKey), &bNew);
	if ( pInfo == NULL ) {
		return 0;
	}
	if ( bNew ) {
		pInfo->FailCount = 0;
		pInfo->CoolDown = 0;
		pInfo->TimeRate = 0;
		return 0;
	}
	if ( pInfo->CoolDown > XAdmin_UnixNowUs() ) {
		return pInfo->CoolDown;
	}

	return 0;
}

void Guard_Failed(xmap* dictGuard, str sRemote)
{
	GuardInfo* pInfo;
	str sKey = Guard_RemoteKey(sRemote);

	pInfo = xrtMapGetOrAdd(dictGuard, KeyView(sKey), NULL);
	if ( pInfo == NULL ) {
		return;
	}

	pInfo->FailCount++;
	int limit = dictGuard == G_GuardAdmin ? Global_Int("admin_login_failure_limit") : BRUTE_COUNT;
	int seconds = dictGuard == G_GuardAdmin ? Global_Int("admin_login_cooldown_seconds") : BRUTE_TIMES;
	if ( pInfo->FailCount >= limit ) {
		pInfo->FailCount = 0;
		pInfo->TimeRate++;
		pInfo->CoolDown = XAdmin_UnixNowUs() + (xtime)seconds * pInfo->TimeRate * 1000000;
	}
}

void Guard_Reset(xmap* dictGuard, str sRemote)
{
	GuardInfo* pInfo;
	str sKey = Guard_RemoteKey(sRemote);

	pInfo = xrtMapGetOrAdd(dictGuard, KeyView(sKey), NULL);
	if ( pInfo == NULL ) {
		return;
	}

	pInfo->FailCount = 0;
	pInfo->CoolDown = 0;
	pInfo->TimeRate = 0;
}

/* R2：注册限速——每 IP 一个间隔窗口，成功注册才计数（Note 由调用方在写库
 * 成功后触发，失败尝试不占用窗口）。间隔由调用方传入（可配置），<=0 禁用。 */
typedef struct {
	xtime LastOK;
} RegisterInfo;
static xmap* G_RegisterGuard = NULL;

static int Register_WaitSeconds(str sRemote, int iInterval)
{
	RegisterInfo* pInfo;
	str sKey = Guard_RemoteKey(sRemote);
	xtime tNow;

	if ( (G_RegisterGuard == NULL) || (iInterval <= 0) ) {
		return 0;
	}
	pInfo = xrtMapGet(G_RegisterGuard, KeyView(sKey));
	if ( (pInfo == NULL) || (pInfo->LastOK == 0) ) {
		return 0;
	}
	tNow = XAdmin_UnixNowUs();
	if ( tNow - pInfo->LastOK < (xtime)iInterval * 1000000 ) {
		return (int)(((xtime)iInterval * 1000000 - (tNow - pInfo->LastOK)) / 1000000) + 1;
	}
	return 0;
}

static void Register_Note(str sRemote)
{
	RegisterInfo* pInfo;
	str sKey = Guard_RemoteKey(sRemote);

	if ( G_RegisterGuard == NULL ) {
		return;
	}
	pInfo = xrtMapGetOrAdd(G_RegisterGuard, KeyView(sKey), NULL);
	if ( pInfo != NULL ) {
		pInfo->LastOK = XAdmin_UnixNowUs();
	}
}

void Guard_Init()
{
	printf("        Guard_Init \n");
	G_GuardAdmin = xrtMapCreate(sizeof(GuardInfo));
	G_GuardMember = xrtMapCreate(sizeof(GuardInfo));
	G_RegisterGuard = xrtMapCreate(sizeof(RegisterInfo));
}

void Guard_Unit()
{
	printf("        Guard_Unit \n");
	xrtMapDestroy(G_GuardAdmin); G_GuardAdmin = NULL;
	xrtMapDestroy(G_GuardMember); G_GuardMember = NULL;
	xrtMapDestroy(G_RegisterGuard); G_RegisterGuard = NULL;
}
