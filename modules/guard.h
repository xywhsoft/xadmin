

// brute-force guard
typedef struct {
	xtime CoolDown;
	int FailCount;
	int TimeRate;
} GuardInfo;
xdict G_BruteGuard = NULL;

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



xtime Guard_Check(str sRemote)
{
	GuardInfo* pInfo;
	bool bNew = FALSE;
	str sKey = Guard_RemoteKey(sRemote);

	pInfo = XA_DictSet(G_BruteGuard, sKey, strlen(sKey), &bNew);
	if ( pInfo == NULL ) {
		return 0;
	}
	if ( bNew ) {
		pInfo->FailCount = 0;
		pInfo->CoolDown = 0;
		pInfo->TimeRate = 0;
		return 0;
	}
	if ( pInfo->CoolDown > XA_Now() ) {
		return pInfo->CoolDown;
	}

	return 0;
}



void Guard_Failed(str sRemote)
{
	GuardInfo* pInfo;
	str sKey = Guard_RemoteKey(sRemote);

	pInfo = XA_DictSet(G_BruteGuard, sKey, strlen(sKey), NULL);
	if ( pInfo == NULL ) {
		return;
	}

	pInfo->FailCount++;
	if ( pInfo->FailCount >= BRUTE_COUNT ) {
		pInfo->FailCount = 0;
		pInfo->TimeRate++;
		pInfo->CoolDown = XA_Now() + (BRUTE_TIMES * pInfo->TimeRate);
	}
}



void Guard_Reset(str sRemote)
{
	GuardInfo* pInfo;
	str sKey = Guard_RemoteKey(sRemote);

	pInfo = XA_DictSet(G_BruteGuard, sKey, strlen(sKey), NULL);
	if ( pInfo == NULL ) {
		return;
	}

	pInfo->FailCount = 0;
	pInfo->CoolDown = 0;
	pInfo->TimeRate = 0;
}



void Guard_Init()
{
	printf("        Guard_Init \n");
	G_BruteGuard = XA_DictCreate(sizeof(GuardInfo), XRT_OBJMODE_SHARED);
}



void Guard_Unit()
{
	printf("        Guard_Unit \n");
	xrtMapDestroy(G_BruteGuard);
}
