


// 暴力破解防护表
typedef struct {
	xtime CoolDown;		// 冷静截止时间
	int FailCount;		// 失败尝试次数
	int TimeRate;		// 时间倍率 ( 每次触发冷静期，叠加时间倍数 )
} GuardInfo;
xlist G_BruteGuardIPV4 = NULL;
xdict G_BruteGuardIPV6 = NULL;

// 连续失败多少次之后，进入冷静期
#define BRUTE_COUNT 5

// 冷静期初始时间是多少秒
#define BRUTE_TIMES 300



// 验证目标地址是否在冷静期
xtime Guard_Check(struct mg_addr* ip)
{
	GuardInfo* pInfo;
	bool bNew = FALSE;
	if ( ip->is_ip6 ) {
		// IPV6
		pInfo = xrtDictSet(G_BruteGuardIPV6, ip->ip, 16, &bNew);
	} else {
		// IPV4
		pInfo = xrtListSet(G_BruteGuardIPV4, ((uint32*)ip)[0], &bNew);
	}
	if ( pInfo == NULL ) {
		return 0;
	}
	// 首次尝试
	if ( bNew ) {
		pInfo->FailCount = 0;
		pInfo->CoolDown = 0;
		pInfo->TimeRate = 0;
		return 0;
	}
	if ( pInfo->CoolDown > xrtNow() ) {
		return pInfo->CoolDown;
	} else {
		return 0;
	}
}



// 累加目标地址失败数据
void Guard_Failed(struct mg_addr* ip)
{
	GuardInfo* pInfo;
	if ( ip->is_ip6 ) {
		// IPV6
		pInfo = xrtDictSet(G_BruteGuardIPV6, ip->ip, 16, NULL);
	} else {
		// IPV4
		pInfo = xrtListSet(G_BruteGuardIPV4, ((uint32*)ip)[0], NULL);
	}
	pInfo->FailCount++;
	if ( pInfo->FailCount >= BRUTE_COUNT ) {
		pInfo->FailCount = 0;
		pInfo->TimeRate++;
		pInfo->CoolDown = xrtNow() + (BRUTE_TIMES * pInfo->TimeRate);
	}
}



// 重置目标地址信息
void Guard_Reset(struct mg_addr* ip)
{
	GuardInfo* pInfo;
	if ( ip->is_ip6 ) {
		// IPV6
		pInfo = xrtDictSet(G_BruteGuardIPV6, ip->ip, 16, NULL);
	} else {
		// IPV4
		pInfo = xrtListSet(G_BruteGuardIPV4, ((uint32*)ip)[0], NULL);
	}
	pInfo->FailCount = 0;
	pInfo->CoolDown = 0;
	pInfo->TimeRate = 0;
}





// 初始化防护模块
void Guard_Init()
{
	printf("        Guard_Init \n");
	G_BruteGuardIPV4 = xrtListCreate(sizeof(GuardInfo));
	G_BruteGuardIPV6 = xrtDictCreate(sizeof(GuardInfo));
}



// 卸载防护模块
void Guard_Unit()
{
	printf("        Guard_Unit \n");
	xrtListDestroy(G_BruteGuardIPV4);
	xrtDictDestroy(G_BruteGuardIPV6);
}


