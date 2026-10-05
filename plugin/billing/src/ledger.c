static int Billing_AdjustCash(int64_t owner,int64_t amount,const char* key,const char* reason,const char* actor)
{
    if(!XP_Id(key,96) || !reason || strlen(reason)>256 || !actor || strlen(actor)>128 ||
        !amount || amount>XBILL_MAX_AMOUNT || amount<-XBILL_MAX_AMOUNT)return XBILL_INVALID;
    if(!Billing_Ensure(owner) || !Billing_Begin())return XBILL_UNAVAILABLE;
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT member_id,amount,kind FROM ledger WHERE entry_key=?");XP_Bind(s,1,key);
    int rc=s?sqlite3_step(s):SQLITE_ERROR;
    if(rc==SQLITE_ROW){bool same=sqlite3_column_int64(s,0)==owner && sqlite3_column_int64(s,1)==amount &&
        !strcmp((const char*)sqlite3_column_text(s,2),"adjust");sqlite3_finalize(s);
        return Billing_End(true)?(same?0:XBILL_CONFLICT):XBILL_UNAVAILABLE;}
    sqlite3_finalize(s);if(rc!=SQLITE_DONE){Billing_End(false);return XBILL_UNAVAILABLE;}
    s=XP_SQL(G_DB,"UPDATE account SET cash=cash+? WHERE member_id=? AND cash+? BETWEEN reserved AND 1000000000000000");
    if(s){sqlite3_bind_int64(s,1,amount);sqlite3_bind_int64(s,2,owner);sqlite3_bind_int64(s,3,amount);}
    bool ok=XP_Done(s);int changed=sqlite3_changes(G_DB);
    if(ok && changed==1)ok=Billing_Ledger(owner,key,"adjust",amount,"cash","",reason,actor);
    if(!changed){Billing_End(false);return XBILL_INSUFFICIENT;}
    return Billing_End(ok)?0:XBILL_UNAVAILABLE;
}
static bool Billing_AddCredit(int64_t owner,int64_t amount,int64_t expires,const char* source,const char* key,const char* actor)
{
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT credit.member_id,ledger.amount,expires_at,credit.source FROM credit JOIN ledger ON ledger.entry_key=credit.operation_id WHERE operation_id=?");XP_Bind(s,1,key);
    int rc=s?sqlite3_step(s):SQLITE_ERROR;
    if(rc==SQLITE_ROW){bool same=sqlite3_column_int64(s,0)==owner && sqlite3_column_int64(s,1)==amount && sqlite3_column_int64(s,2)==expires &&
        !strcmp((const char*)sqlite3_column_text(s,3),source);sqlite3_finalize(s);return same;}
    sqlite3_finalize(s);if(rc!=SQLITE_DONE)return false;
    if(expires<0 || (expires && expires<=(int64_t)time(NULL)))return false;
    s=XP_SQL(G_DB,"SELECT COALESCE(SUM(balance),0) FROM credit WHERE member_id=?");
    if(s)sqlite3_bind_int64(s,1,owner);
    bool capacity=s && sqlite3_step(s)==SQLITE_ROW && sqlite3_column_int64(s,0)<=XBILL_MAX_AMOUNT-amount;
    sqlite3_finalize(s);if(!capacity)return false;
    s=XP_SQL(G_DB,"INSERT INTO credit(member_id,balance,expires_at,source,operation_id) VALUES(?,?,?,?,?)");
    if(s){sqlite3_bind_int64(s,1,owner);sqlite3_bind_int64(s,2,amount);sqlite3_bind_int64(s,3,expires);}
    XP_Bind(s,4,source);XP_Bind(s,5,key);
    return XP_Done(s) && Billing_Ledger(owner,key,"grant",amount,"credit","",source,actor);
}
static int Billing_Grant(int64_t owner,int64_t amount,int64_t expires,const char* source,const char* key,const char* actor)
{
    if(amount<=0 || amount>XBILL_MAX_AMOUNT || !XP_Id(key,96) || !source || strlen(source)>128 || expires<0 || !actor || strlen(actor)>128)return XBILL_INVALID;
    if(!Billing_Ensure(owner) || !Billing_Begin())return XBILL_UNAVAILABLE;
    return Billing_End(Billing_AddCredit(owner,amount,expires,source,key,actor))?0:XBILL_CONFLICT;
}
static int Billing_Lookup(const char* id,XBillingResult* out)
{
    if(!XP_Id(id,96) || !out || out->size!=sizeof(*out))return XBILL_INVALID;
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT member_id,amount,charged,expires_at,state FROM reservation WHERE request_id=?");XP_Bind(s,1,id);
    int rc=s?sqlite3_step(s):SQLITE_ERROR;
    if(rc==SQLITE_ROW){out->member_id=sqlite3_column_int64(s,0);out->reserved=sqlite3_column_int64(s,1);
        out->charged=sqlite3_column_int64(s,2);out->expires_at=sqlite3_column_int64(s,3);
        snprintf(out->state,sizeof(out->state),"%s",sqlite3_column_text(s,4));}
    sqlite3_finalize(s);return rc==SQLITE_ROW?0:rc==SQLITE_DONE?404:XBILL_UNAVAILABLE;
}
static void Billing_Notify(const char* id)
{
    XBillingResult r={sizeof(r)};if(Billing_Lookup(id,&r))return;
    XBillingEvent event={sizeof(event)};event.member_id=r.member_id;event.charged=r.charged;
    snprintf(event.request_id,sizeof(event.request_id),"%s",id);snprintf(event.state,sizeof(event.state),"%s",r.state);
    XAdmin_EmitEvent(G_Handle,"billing.finalized",&event,sizeof(event));
}
static bool Billing_Allocation(const char* request_id,int64_t bucket,int64_t amount)
{
    sqlite3_stmt* s=XP_SQL(G_DB,"INSERT INTO allocation(request_id,credit_id,amount) VALUES(?,?,?)");
    XP_Bind(s,1,request_id);if(s){sqlite3_bind_int64(s,2,bucket);sqlite3_bind_int64(s,3,amount);}return XP_Done(s);
}
static int Billing_Reserve(const XBillingReservation* r)
{
    int64_t now=(int64_t)time(NULL);
    if(!r || r->size!=sizeof(*r) || !XP_Id(r->request_id,96) || !XP_Id(r->service,64) ||
        !r->model || strlen(r->model)>128 || r->amount<0 || r->amount>XBILL_MAX_AMOUNT ||
        r->expires_at<now+600 || r->expires_at>now+86400)return XBILL_INVALID;
    if(!Billing_Maintain() || !Billing_Periods() || !Billing_Ensure(r->member_id) || !Billing_Begin())return XBILL_UNAVAILABLE;
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT member_id,amount,service,model,state FROM reservation WHERE request_id=?");XP_Bind(s,1,r->request_id);
    int rc=s?sqlite3_step(s):SQLITE_ERROR;
    if(rc==SQLITE_ROW){bool same=sqlite3_column_int64(s,0)==r->member_id && sqlite3_column_int64(s,1)==r->amount &&
        !strcmp((const char*)sqlite3_column_text(s,2),r->service) && !strcmp((const char*)sqlite3_column_text(s,3),r->model) &&
        !strcmp((const char*)sqlite3_column_text(s,4),"reserved");sqlite3_finalize(s);
        return Billing_End(true)?(same?0:XBILL_CONFLICT):XBILL_UNAVAILABLE;}
    sqlite3_finalize(s);if(rc!=SQLITE_DONE || !Billing_Expire(r->member_id)){Billing_End(false);return XBILL_UNAVAILABLE;}
    s=XP_SQL(G_DB,"INSERT INTO reservation(request_id,member_id,amount,service,model,state,expires_at,created_at)VALUES(?,?,?,?,?,'reserved',?,?)");
    XP_Bind(s,1,r->request_id);if(s){sqlite3_bind_int64(s,2,r->member_id);sqlite3_bind_int64(s,3,r->amount);
        sqlite3_bind_int64(s,6,r->expires_at);sqlite3_bind_int64(s,7,now);}XP_Bind(s,4,r->service);XP_Bind(s,5,r->model);
    bool ok=XP_Done(s);int64_t remaining=r->amount;
    s=ok?XP_SQL(G_DB,"SELECT id,balance-reserved AS available FROM credit WHERE member_id=? AND balance>reserved AND (expires_at=0 OR expires_at>?) ORDER BY CASE WHEN expires_at=0 THEN 9223372036854775807 ELSE expires_at END,id"):NULL;
    if(s){sqlite3_bind_int64(s,1,r->member_id);sqlite3_bind_int64(s,2,now);}xvalue* buckets=XP_Rows(s);ok=ok && buckets;
    uint32 i;for(i=0;ok && remaining>0 && i<ValueCount(buckets);i++){
        xvalue* bucket=xrtValueArrayGet(buckets,i);int64_t id=ValueInt(bucket,"id"),amount=ValueInt(bucket,"available");if(amount>remaining)amount=remaining;
        s=XP_SQL(G_DB,"UPDATE credit SET reserved=reserved+? WHERE id=? AND balance-reserved>=?");
        if(s){sqlite3_bind_int64(s,1,amount);sqlite3_bind_int64(s,2,id);sqlite3_bind_int64(s,3,amount);}
        ok=XP_Done(s) && sqlite3_changes(G_DB)==1 && Billing_Allocation(r->request_id,id,amount);remaining-=amount;
    }xrtValueRelease(buckets);
    if(ok && remaining){
        s=XP_SQL(G_DB,"UPDATE account SET reserved=reserved+? WHERE member_id=? AND cash-reserved>=?");
        if(s){sqlite3_bind_int64(s,1,remaining);sqlite3_bind_int64(s,2,r->member_id);sqlite3_bind_int64(s,3,remaining);}
        ok=XP_Done(s);if(ok && sqlite3_changes(G_DB)!=1){Billing_End(false);return XBILL_INSUFFICIENT;}
        if(ok)ok=Billing_Allocation(r->request_id,0,remaining);
    }
    return Billing_End(ok)?0:XBILL_UNAVAILABLE;
}
static int Billing_Finalize(const XBillingSettlement* r)
{
    if(!r || r->size!=sizeof(*r) || !XP_Id(r->request_id,96) || !r->outcome ||
        (strcmp(r->outcome,"settled") && strcmp(r->outcome,"released") && strcmp(r->outcome,"pending")) ||
        r->amount<0 || r->amount>XBILL_MAX_AMOUNT ||
        (r->usage_json && strlen(r->usage_json)>16384) || (strcmp(r->outcome,"settled") && r->amount))return XBILL_INVALID;
    XBillingResult hold={sizeof(hold)};int status=Billing_Lookup(r->request_id,&hold);if(status)return status;
    if(!strcmp(hold.state,"settled") || !strcmp(hold.state,"released") || !strcmp(hold.state,"refunded")){
        return hold.charged==r->amount && (!strcmp(hold.state,r->outcome)||!strcmp(hold.state,"refunded"))?0:XBILL_CONFLICT;}
    if(r->amount>hold.reserved)return XBILL_CONFLICT;
    if(!Billing_Begin())return XBILL_UNAVAILABLE;sqlite3_stmt* s;bool ok=true;
    if(!strcmp(r->outcome,"pending")){
        s=XP_SQL(G_DB,"UPDATE reservation SET state='pending',usage_json=?,expires_at=CASE WHEN state='reserved' THEN ? ELSE expires_at END WHERE request_id=? AND state IN('reserved','pending')");
        XP_Bind(s,1,r->usage_json);XP_Bind(s,3,r->request_id);if(s)sqlite3_bind_int64(s,2,(int64_t)time(NULL)+G_PendingSeconds);
        return Billing_End(XP_Done(s))?0:XBILL_UNAVAILABLE;
    }
    s=XP_SQL(G_DB,"SELECT credit_id,amount FROM allocation LEFT JOIN credit ON credit.id=allocation.credit_id WHERE request_id=? ORDER BY CASE WHEN credit_id=0 THEN 1 ELSE 0 END,CASE WHEN credit.expires_at=0 THEN 9223372036854775807 ELSE credit.expires_at END,credit_id");XP_Bind(s,1,r->request_id);
    xvalue* allocations=XP_Rows(s);ok=allocations!=NULL;int64_t remaining=r->amount;uint32 i;
    for(i=0;ok && i<ValueCount(allocations);i++){
        xvalue* allocation=xrtValueArrayGet(allocations,i);int64_t id=ValueInt(allocation,"credit_id"),held=ValueInt(allocation,"amount"),charge=held<remaining?held:remaining;
        if(id){s=XP_SQL(G_DB,"UPDATE credit SET reserved=reserved-?,balance=balance-? WHERE id=?");
            if(s){sqlite3_bind_int64(s,1,held);sqlite3_bind_int64(s,2,charge);sqlite3_bind_int64(s,3,id);}}
        else{s=XP_SQL(G_DB,"UPDATE account SET reserved=reserved-?,cash=cash-? WHERE member_id=?");
            if(s){sqlite3_bind_int64(s,1,held);sqlite3_bind_int64(s,2,charge);sqlite3_bind_int64(s,3,hold.member_id);}}
        ok=XP_Done(s) && sqlite3_changes(G_DB)==1;
        s=ok?XP_SQL(G_DB,"UPDATE allocation SET charged=? WHERE request_id=? AND credit_id=?"):NULL;
        if(s){sqlite3_bind_int64(s,1,charge);sqlite3_bind_int64(s,3,id);}XP_Bind(s,2,r->request_id);ok=ok && XP_Done(s);
        if(ok && charge){char key[140];snprintf(key,sizeof(key),"charge.%s.%lld",r->request_id,(long long)id);
            ok=Billing_Ledger(hold.member_id,key,"consume",-charge,id?"credit":"cash",r->request_id,"Usage settlement","service");}
        remaining-=charge;
    }xrtValueRelease(allocations);ok=ok && remaining==0;
    s=ok?XP_SQL(G_DB,"UPDATE reservation SET state=?,charged=?,usage_json=?,finished_at=? WHERE request_id=? AND state IN('reserved','pending')"):NULL;
    XP_Bind(s,1,r->outcome);XP_Bind(s,3,r->usage_json);XP_Bind(s,5,r->request_id);
    if(s){sqlite3_bind_int64(s,2,r->amount);sqlite3_bind_int64(s,4,(int64_t)time(NULL));}ok=ok && XP_Done(s) && sqlite3_changes(G_DB)==1;
    if(ok)ok=Billing_Expire(hold.member_id) && Billing_Changed(r->request_id);ok=Billing_End(ok);
    if(ok)Billing_Notify(r->request_id);return ok?0:XBILL_UNAVAILABLE;
}
static int Billing_Refund(const char* id,const char* key,const char* reason,const char* actor)
{
    if(!XP_Id(key,96) || !reason || strlen(reason)>256 || !actor || strlen(actor)>128)return XBILL_INVALID;
    XBillingResult hold={sizeof(hold)};int status=Billing_Lookup(id,&hold);if(status)return status;
    if(!Billing_Begin())return XBILL_UNAVAILABLE;
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT request_id,member_id,kind FROM ledger WHERE entry_key=?");XP_Bind(s,1,key);
    int rc=s?sqlite3_step(s):SQLITE_ERROR;
    if(rc==SQLITE_ROW){bool same=sqlite3_column_int64(s,1)==hold.member_id && !strcmp((const char*)sqlite3_column_text(s,0),id) &&
        !strcmp((const char*)sqlite3_column_text(s,2),"refund_operation");
        sqlite3_finalize(s);return Billing_End(true)?(same?0:XBILL_CONFLICT):XBILL_UNAVAILABLE;}
    sqlite3_finalize(s);
    if(rc!=SQLITE_DONE || strcmp(hold.state,"settled")){Billing_End(false);return XBILL_CONFLICT;}
    s=XP_SQL(G_DB,"SELECT credit_id,charged FROM allocation WHERE request_id=? AND charged>0");XP_Bind(s,1,id);
    xvalue* allocations=XP_Rows(s);bool ok=allocations!=NULL;uint32 i;
    for(i=0;ok && i<ValueCount(allocations);i++){
        xvalue* row=xrtValueArrayGet(allocations,i);int64_t bucket=ValueInt(row,"credit_id"),amount=ValueInt(row,"charged");
        s=XP_SQL(G_DB,bucket?"UPDATE credit SET balance=balance+? WHERE id=?":"UPDATE account SET cash=cash+? WHERE member_id=?");
        if(s){sqlite3_bind_int64(s,1,amount);sqlite3_bind_int64(s,2,bucket?bucket:hold.member_id);}ok=XP_Done(s) && sqlite3_changes(G_DB)==1;
        if(ok){char entry[160];snprintf(entry,sizeof(entry),"refund.%s.%lld",id,(long long)bucket);
            ok=Billing_Ledger(hold.member_id,entry,"refund",amount,bucket?"credit":"cash",id,reason,actor);}
    }xrtValueRelease(allocations);
    if(ok)ok=Billing_Ledger(hold.member_id,key,"refund_operation",0,"",id,reason,actor);
    s=ok?XP_SQL(G_DB,"UPDATE reservation SET state='refunded' WHERE request_id=? AND state='settled'"):NULL;XP_Bind(s,1,id);
    ok=ok && XP_Done(s) && sqlite3_changes(G_DB)==1 && Billing_Expire(hold.member_id) && Billing_Changed(id);ok=Billing_End(ok);
    if(ok)Billing_Notify(id);return ok?0:XBILL_UNAVAILABLE;
}
/* Unknown requests are never silently charged. After the bounded recovery
 * window, release them and record the platform's unresolved cost separately. */
static bool Billing_Maintain(void)
{
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT request_id FROM reservation WHERE state IN('reserved','pending') AND expires_at<=? LIMIT 100");
    if(s)sqlite3_bind_int64(s,1,(int64_t)time(NULL));xvalue* rows=XP_Rows(s);if(!rows)return false;bool ok=true;uint32 i;
    for(i=0;ok && i<ValueCount(rows);i++){
        XBillingSettlement settlement={sizeof(settlement),ValueText(xrtValueArrayGet(rows,i),"request_id"),0,"{\"usage_source\":\"unresolved\",\"resolution\":\"platform_absorbed\"}","released"};
        ok=Billing_Finalize(&settlement)==0;
    }xrtValueRelease(rows);return ok;
}
