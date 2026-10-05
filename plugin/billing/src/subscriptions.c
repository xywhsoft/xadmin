static bool Billing_ModelList(const char* text)
{
    if(!text || !*text || strlen(text)>1024)return false;if(!strcmp(text,"*"))return true;
    const char* p=text;while(*p){const char* end=strchr(p,',');if(!end)end=p+strlen(p);
        char id[129];size_t n=(size_t)(end-p);if(!n || n>=sizeof(id))return false;
        memcpy(id,p,n);id[n]=0;if(!XP_Id(id,128))return false;if(!*end)return true;p=end+1;if(!*p)return false;}
    return false;
}
static int Billing_Entitlement(int64_t owner,XBillingEntitlement* out)
{
    if(!out || out->size!=sizeof(*out))return XBILL_INVALID;
    memset((char*)out+sizeof(out->size),0,sizeof(*out)-sizeof(out->size));out->discount_bps=10000;out->concurrency_limit=1;
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT plan_id,expires_at,discount_bps,concurrency_limit,model_ids FROM subscription WHERE member_id=? AND starts_at<=? AND expires_at>? AND cancelled_at IS NULL ORDER BY starts_at DESC,id DESC LIMIT 1");
    int64_t now=(int64_t)time(NULL);if(s){sqlite3_bind_int64(s,1,owner);sqlite3_bind_int64(s,2,now);sqlite3_bind_int64(s,3,now);}
    int rc=s?sqlite3_step(s):SQLITE_ERROR;
    if(rc==SQLITE_ROW){snprintf(out->plan_id,sizeof(out->plan_id),"%s",sqlite3_column_text(s,0));out->expires_at=sqlite3_column_int64(s,1);
        out->discount_bps=sqlite3_column_int(s,2);out->concurrency_limit=sqlite3_column_int(s,3);snprintf(out->model_ids,sizeof(out->model_ids),"%s",sqlite3_column_text(s,4));}
    sqlite3_finalize(s);return rc==SQLITE_ROW || rc==SQLITE_DONE?0:XBILL_UNAVAILABLE;
}
static int Billing_SavePlan(const xvalue* body)
{
    const char* names[]={"id","title","duration_days","period_days","credit_micros","discount_bps","concurrency_limit","model_ids","enabled"};
    const char* id=XP_Text(body,"id",64),*title=XP_Text(body,"title",128),*models=XP_Text(body,"model_ids",1024);
    int64_t days,period,credit,discount,concurrency;bool enabled;
    if(!XP_Fields(body,names,9) || !XP_Id(id,64) || !title || !*title || !Billing_ModelList(models) ||
        !XP_Int(body,"duration_days",1,366,&days) || !XP_Int(body,"period_days",1,366,&period) || period>days ||
        !XP_Int(body,"credit_micros",0,XBILL_MAX_AMOUNT,&credit) || !XP_Int(body,"discount_bps",1,10000,&discount) ||
        !XP_Int(body,"concurrency_limit",1,16,&concurrency) || !xrtValueGetBool(ValueGet(body,"enabled"),&enabled))return XBILL_INVALID;
    sqlite3_stmt* s=XP_SQL(G_DB,"INSERT INTO plan(id,title,duration_seconds,period_seconds,credit_amount,discount_bps,concurrency_limit,model_ids,enabled)VALUES(?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET title=excluded.title,duration_seconds=excluded.duration_seconds,period_seconds=excluded.period_seconds,credit_amount=excluded.credit_amount,discount_bps=excluded.discount_bps,concurrency_limit=excluded.concurrency_limit,model_ids=excluded.model_ids,enabled=excluded.enabled");
    XP_Bind(s,1,id);XP_Bind(s,2,title);XP_Bind(s,8,models);
    if(s){sqlite3_bind_int64(s,3,days*86400);sqlite3_bind_int64(s,4,period*86400);sqlite3_bind_int64(s,5,credit);
        sqlite3_bind_int64(s,6,discount);sqlite3_bind_int64(s,7,concurrency);sqlite3_bind_int(s,9,enabled?1:0);}return XP_Done(s)?0:XBILL_UNAVAILABLE;
}
static int Billing_Subscribe(int64_t owner,const char* plan,const char* key,const char* actor)
{
    if(!XP_Id(plan,64) || !XP_Id(key,96) || !Billing_Ensure(owner))return XBILL_INVALID;
    if(!Billing_Begin())return XBILL_UNAVAILABLE;
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT member_id,plan_id FROM subscription WHERE operation_id=?");XP_Bind(s,1,key);
    int rc=s?sqlite3_step(s):SQLITE_ERROR;
    if(rc==SQLITE_ROW){bool same=sqlite3_column_int64(s,0)==owner && !strcmp((const char*)sqlite3_column_text(s,1),plan);
        sqlite3_finalize(s);return Billing_End(true)?(same?0:XBILL_CONFLICT):XBILL_UNAVAILABLE;}
    sqlite3_finalize(s);if(rc!=SQLITE_DONE){Billing_End(false);return XBILL_UNAVAILABLE;}
    int64_t now=(int64_t)time(NULL),starts=now;
    s=XP_SQL(G_DB,"SELECT COALESCE(MAX(expires_at),0) FROM subscription WHERE member_id=? AND plan_id=? AND expires_at>? AND cancelled_at IS NULL");
    if(s){sqlite3_bind_int64(s,1,owner);XP_Bind(s,2,plan);sqlite3_bind_int64(s,3,now);}
    bool ok=s && sqlite3_step(s)==SQLITE_ROW;if(ok && sqlite3_column_int64(s,0)>starts)starts=sqlite3_column_int64(s,0);sqlite3_finalize(s);
    /* Same-plan renewal starts after its predecessor and does not grant the
     * current period twice. A replacement cancels future grants, not credit
     * already earned under the previous membership. */
    s=ok?XP_SQL(G_DB,"UPDATE subscription SET cancelled_at=? WHERE member_id=? AND plan_id<>? AND cancelled_at IS NULL"):NULL;
    if(s){sqlite3_bind_int64(s,1,now);sqlite3_bind_int64(s,2,owner);}XP_Bind(s,3,plan);ok=ok && XP_Done(s);
    s=ok?XP_SQL(G_DB,"INSERT INTO subscription(member_id,plan_id,title,starts_at,expires_at,discount_bps,concurrency_limit,model_ids,operation_id,period_seconds,credit_amount,next_grant_at,actor) SELECT ?,id,title,?,?+duration_seconds,discount_bps,concurrency_limit,model_ids,?,period_seconds,credit_amount,?,? FROM plan WHERE id=? AND enabled=1"):NULL;
    if(s){sqlite3_bind_int64(s,1,owner);sqlite3_bind_int64(s,2,starts);sqlite3_bind_int64(s,3,starts);sqlite3_bind_int64(s,5,starts);}
    XP_Bind(s,4,key);XP_Bind(s,6,actor);XP_Bind(s,7,plan);ok=ok && XP_Done(s) && sqlite3_changes(G_DB)==1;
    return Billing_End(ok)?0:XBILL_CONFLICT;
}
static bool Billing_Periods(void)
{
    int64_t now=(int64_t)time(NULL);
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT id,member_id,plan_id,starts_at,expires_at,period_seconds,credit_amount FROM subscription WHERE next_grant_at<=? AND expires_at>? AND cancelled_at IS NULL LIMIT 100");
    if(s){sqlite3_bind_int64(s,1,now);sqlite3_bind_int64(s,2,now);}xvalue* rows=XP_Rows(s);if(!rows)return false;bool ok=true;uint32 i;
    for(i=0;ok && i<ValueCount(rows);i++){
        xvalue* row=xrtValueArrayGet(rows,i);int64_t id=ValueInt(row,"id"),owner=ValueInt(row,"member_id"),starts=ValueInt(row,"starts_at"),period=ValueInt(row,"period_seconds"),amount=ValueInt(row,"credit_amount");
        int64_t number=(now-starts)/period,end=starts+(number+1)*period,expires=ValueInt(row,"expires_at");if(end>expires)end=expires;
        char key[96];snprintf(key,sizeof(key),"subscription.%lld.period.%lld",(long long)id,(long long)number);
        if(!Billing_Begin()){ok=false;break;}
        bool applied=!amount || Billing_AddCredit(owner,amount,end,ValueText(row,"plan_id"),key,"subscription");
        s=applied?XP_SQL(G_DB,"UPDATE subscription SET next_grant_at=? WHERE id=?"):NULL;
        if(s){sqlite3_bind_int64(s,1,end);sqlite3_bind_int64(s,2,id);}ok=Billing_End(applied && XP_Done(s));
    }xrtValueRelease(rows);return ok;
}
static int Billing_Cancel(int64_t owner,int64_t id)
{
    sqlite3_stmt* s=XP_SQL(G_DB,"UPDATE subscription SET cancelled_at=COALESCE(cancelled_at,?) WHERE id=? AND member_id=?");
    if(s){sqlite3_bind_int64(s,1,(int64_t)time(NULL));sqlite3_bind_int64(s,2,id);sqlite3_bind_int64(s,3,owner);}
    if(!XP_Done(s))return XBILL_UNAVAILABLE;return sqlite3_changes(G_DB)==1?0:404;
}
