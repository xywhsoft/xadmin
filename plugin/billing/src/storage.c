/* Each monetary transition is one short transaction in this database. Gateway
 * records live elsewhere and converge by idempotent finalize/lookup calls. */
static bool Billing_Begin(void){return sqlite3_exec(G_DB,"BEGIN IMMEDIATE",NULL,NULL,NULL)==SQLITE_OK;}
static bool Billing_End(bool ok)
{
    if(ok && sqlite3_exec(G_DB,"COMMIT",NULL,NULL,NULL)==SQLITE_OK)return true;
    sqlite3_exec(G_DB,"ROLLBACK",NULL,NULL,NULL);return false;
}
static bool Billing_Schema(void)
{
    if(!XP_SchemaSupported(G_DB,3))return false;
    return sqlite3_exec(G_DB,
        "PRAGMA foreign_keys=ON;PRAGMA journal_mode=WAL;PRAGMA synchronous=FULL;"
        "CREATE TABLE IF NOT EXISTS account(member_id INTEGER PRIMARY KEY,cash INTEGER NOT NULL DEFAULT 0 CHECK(cash BETWEEN 0 AND 1000000000000000),reserved INTEGER NOT NULL DEFAULT 0 CHECK(reserved BETWEEN 0 AND cash));"
        "CREATE TABLE IF NOT EXISTS credit(id INTEGER PRIMARY KEY,member_id INTEGER NOT NULL REFERENCES account(member_id),balance INTEGER NOT NULL CHECK(balance BETWEEN 0 AND 1000000000000000),reserved INTEGER NOT NULL DEFAULT 0 CHECK(reserved BETWEEN 0 AND balance),expires_at INTEGER NOT NULL,source TEXT NOT NULL,operation_id TEXT NOT NULL UNIQUE);"
        "CREATE INDEX IF NOT EXISTS credit_member ON credit(member_id,expires_at);"
        "CREATE TABLE IF NOT EXISTS reservation(request_id TEXT PRIMARY KEY,member_id INTEGER NOT NULL REFERENCES account(member_id),amount INTEGER NOT NULL CHECK(amount>=0),charged INTEGER NOT NULL DEFAULT 0 CHECK(charged>=0),service TEXT NOT NULL,model TEXT NOT NULL,state TEXT NOT NULL CHECK(state IN('reserved','pending','settled','released','refunded')),expires_at INTEGER NOT NULL,created_at INTEGER NOT NULL,finished_at INTEGER,usage_json TEXT);"
        "CREATE INDEX IF NOT EXISTS reservation_member ON reservation(member_id,created_at);"
        "CREATE TABLE IF NOT EXISTS allocation(request_id TEXT NOT NULL REFERENCES reservation(request_id),credit_id INTEGER NOT NULL,amount INTEGER NOT NULL CHECK(amount>=0),charged INTEGER NOT NULL DEFAULT 0 CHECK(charged BETWEEN 0 AND amount),PRIMARY KEY(request_id,credit_id));"
        "CREATE TABLE IF NOT EXISTS ledger(id INTEGER PRIMARY KEY,entry_key TEXT NOT NULL UNIQUE,member_id INTEGER NOT NULL REFERENCES account(member_id),kind TEXT NOT NULL,amount INTEGER NOT NULL,source TEXT NOT NULL,request_id TEXT NOT NULL,reason TEXT NOT NULL,actor TEXT NOT NULL,created_at INTEGER NOT NULL);"
        "CREATE INDEX IF NOT EXISTS ledger_member ON ledger(member_id,created_at);"
        "CREATE TRIGGER IF NOT EXISTS ledger_no_update BEFORE UPDATE ON ledger BEGIN SELECT RAISE(ABORT,'immutable ledger'); END;"
        "CREATE TRIGGER IF NOT EXISTS ledger_no_delete BEFORE DELETE ON ledger BEGIN SELECT RAISE(ABORT,'immutable ledger'); END;"
        "CREATE TABLE IF NOT EXISTS plan(id TEXT PRIMARY KEY,title TEXT NOT NULL,duration_seconds INTEGER NOT NULL,period_seconds INTEGER NOT NULL,credit_amount INTEGER NOT NULL,discount_bps INTEGER NOT NULL CHECK(discount_bps BETWEEN 1 AND 10000),concurrency_limit INTEGER NOT NULL CHECK(concurrency_limit BETWEEN 1 AND 16),model_ids TEXT NOT NULL,enabled INTEGER NOT NULL CHECK(enabled IN(0,1)));"
        "CREATE TABLE IF NOT EXISTS subscription(id INTEGER PRIMARY KEY,member_id INTEGER NOT NULL REFERENCES account(member_id),plan_id TEXT NOT NULL,title TEXT NOT NULL,starts_at INTEGER NOT NULL,expires_at INTEGER NOT NULL,discount_bps INTEGER NOT NULL,concurrency_limit INTEGER NOT NULL,model_ids TEXT NOT NULL,operation_id TEXT NOT NULL UNIQUE,period_seconds INTEGER NOT NULL,credit_amount INTEGER NOT NULL,next_grant_at INTEGER NOT NULL,cancelled_at INTEGER,actor TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS receipt_change(sequence INTEGER PRIMARY KEY,service TEXT NOT NULL,request_id TEXT NOT NULL,member_id INTEGER NOT NULL,charged INTEGER NOT NULL,state TEXT NOT NULL);"
        "CREATE INDEX IF NOT EXISTS receipt_change_service ON receipt_change(service,sequence);"
        "CREATE INDEX IF NOT EXISTS receipt_change_request ON receipt_change(request_id);"
        "CREATE TRIGGER IF NOT EXISTS receipt_change_no_update BEFORE UPDATE ON receipt_change BEGIN SELECT RAISE(ABORT,'immutable receipt change'); END;"
        "CREATE TRIGGER IF NOT EXISTS receipt_change_no_delete BEFORE DELETE ON receipt_change BEGIN SELECT RAISE(ABORT,'immutable receipt change'); END;"
        "INSERT INTO receipt_change(service,request_id,member_id,charged,state) SELECT service,request_id,member_id,charged,state FROM reservation r WHERE state IN('settled','released','refunded') AND NOT EXISTS(SELECT 1 FROM receipt_change c WHERE c.request_id=r.request_id);"
        "CREATE TABLE IF NOT EXISTS cash_payment(operation_id TEXT PRIMARY KEY,buyer_id INTEGER NOT NULL REFERENCES account(member_id),seller_id INTEGER NOT NULL,amount INTEGER NOT NULL CHECK(amount BETWEEN 0 AND 1000000000000000),seller_income INTEGER NOT NULL CHECK(seller_income BETWEEN 0 AND amount),created_at INTEGER NOT NULL);"
        "CREATE TRIGGER IF NOT EXISTS cash_payment_no_update BEFORE UPDATE ON cash_payment BEGIN SELECT RAISE(ABORT,'immutable cash payment'); END;"
        "CREATE TRIGGER IF NOT EXISTS cash_payment_no_delete BEFORE DELETE ON cash_payment BEGIN SELECT RAISE(ABORT,'immutable cash payment'); END;"
        "PRAGMA user_version=3;",NULL,NULL,NULL)==SQLITE_OK;
}
/* Same transaction as the monetary transition. An in-memory event alone
 * cannot repair consumers that were stopped during a refund or settlement. */
static bool Billing_Changed(const char* id)
{
    sqlite3_stmt* s=XP_SQL(G_DB,"INSERT INTO receipt_change(service,request_id,member_id,charged,state) SELECT service,request_id,member_id,charged,state FROM reservation WHERE request_id=?");XP_Bind(s,1,id);return XP_Done(s) && sqlite3_changes(G_DB)==1;
}
static bool Billing_Ensure(int64_t owner)
{
    if(owner<=0)return false;
    sqlite3_stmt* s=XP_SQL(G_Host->main_db,"SELECT 1 FROM member WHERE id=? AND isDelete=0");
    if(s)sqlite3_bind_int64(s,1,owner);bool found=s && sqlite3_step(s)==SQLITE_ROW;sqlite3_finalize(s);
    if(!found)return false;
    s=XP_SQL(G_DB,"INSERT OR IGNORE INTO account(member_id) VALUES(?)");if(s)sqlite3_bind_int64(s,1,owner);return XP_Done(s);
}
static bool Billing_Ledger(int64_t owner,const char* key,const char* kind,int64_t amount,
    const char* source,const char* request_id,const char* reason,const char* actor)
{
    sqlite3_stmt* s=XP_SQL(G_DB,"INSERT INTO ledger(entry_key,member_id,kind,amount,source,request_id,reason,actor,created_at)VALUES(?,?,?,?,?,?,?,?,?)");
    XP_Bind(s,1,key);if(s){sqlite3_bind_int64(s,2,owner);sqlite3_bind_int64(s,4,amount);sqlite3_bind_int64(s,9,(int64_t)time(NULL));}
    XP_Bind(s,3,kind);XP_Bind(s,5,source);XP_Bind(s,6,request_id);XP_Bind(s,7,reason);XP_Bind(s,8,actor);
    return XP_Done(s) && sqlite3_changes(G_DB)==1;
}
/* Expire only unreserved credit. A running request retains its allocations;
 * later release/refund expires them again without converting to cash. */
static bool Billing_Expire(int64_t owner)
{
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT id,balance-reserved FROM credit WHERE member_id=? AND expires_at>0 AND expires_at<=? AND balance>reserved");
    if(s){sqlite3_bind_int64(s,1,owner);sqlite3_bind_int64(s,2,(int64_t)time(NULL));}
    xvalue* rows=XP_Rows(s);if(!rows)return false;bool ok=true;uint32 i;
    for(i=0;ok && i<ValueCount(rows);i++){
        xvalue* row=xrtValueArrayGet(rows,i);int64_t id=ValueInt(row,"id"),amount=ValueInt(row,"balance-reserved");
        char key[100];
        /* A random key permits a second expiry after a refund. */
        char random[33];if(!XP_Random(random)){ok=false;break;}snprintf(key,sizeof(key),"expire.%lld.%s",(long long)id,random);
        s=XP_SQL(G_DB,"UPDATE credit SET balance=reserved WHERE id=?");if(s)sqlite3_bind_int64(s,1,id);
        ok=XP_Done(s) && Billing_Ledger(owner,key,"expire",-amount,"credit","","Credit expired","system");
    }xrtValueRelease(rows);return ok;
}
static int Billing_ReadAccount(int64_t owner,XBillingAccount* out)
{
    if(!out || out->size!=sizeof(*out) || !Billing_Ensure(owner))return XBILL_UNAVAILABLE;
    if(!Billing_Begin())return XBILL_UNAVAILABLE;
    bool ok=Billing_Expire(owner);sqlite3_stmt* s=ok?XP_SQL(G_DB,
        "SELECT cash,reserved,(SELECT COALESCE(SUM(balance),0) FROM credit WHERE member_id=account.member_id),(SELECT COALESCE(SUM(reserved),0) FROM credit WHERE member_id=account.member_id) FROM account WHERE member_id=?"):NULL;
    if(s)sqlite3_bind_int64(s,1,owner);ok=s && sqlite3_step(s)==SQLITE_ROW;
    if(ok){out->member_id=owner;out->cash=sqlite3_column_int64(s,0);out->cash_reserved=sqlite3_column_int64(s,1);
        out->credit=sqlite3_column_int64(s,2);out->credit_reserved=sqlite3_column_int64(s,3);}
    sqlite3_finalize(s);return Billing_End(ok)?0:XBILL_UNAVAILABLE;
}
static bool Billing_Migrate(void)
{
    /* Mark the cutover BEFORE importing. If initialization fails, legacy writes
     * fail closed instead of writing an obsolete second balance. Import keys
     * make retry safe, including a crash between the two databases' commits. */
    if(sqlite3_exec(G_Host->main_db,
        "CREATE TABLE IF NOT EXISTS xadmin_balance_provider(id INTEGER PRIMARY KEY CHECK(id=1),service_name TEXT NOT NULL);"
        "INSERT OR IGNORE INTO xadmin_balance_provider VALUES(1,'xadmin.billing');",NULL,NULL,NULL)!=SQLITE_OK)return false;
    sqlite3_stmt* authority=XP_SQL(G_Host->main_db,"SELECT service_name FROM xadmin_balance_provider WHERE id=1");
    bool owns=authority && sqlite3_step(authority)==SQLITE_ROW && !strcmp((const char*)sqlite3_column_text(authority,0),XADMIN_BILLING_SERVICE);
    sqlite3_finalize(authority);if(!owns)return false;
    if(!Billing_Begin())return false;
    sqlite3_stmt* source=XP_SQL(G_Host->main_db,"SELECT id,balance FROM member WHERE isDelete=0");bool ok=source!=NULL;int rc=SQLITE_DONE;
    while(ok && (rc=sqlite3_step(source))==SQLITE_ROW){
        int64_t owner=sqlite3_column_int64(source,0),cents=sqlite3_column_int64(source,1);
        if(cents<0 || cents>XBILL_MAX_AMOUNT/10000){ok=false;break;}
        char key[80];snprintf(key,sizeof(key),"import.member.%lld",(long long)owner);
        sqlite3_stmt* check=XP_SQL(G_DB,"SELECT 1 FROM ledger WHERE entry_key=?");XP_Bind(check,1,key);
        int found=check?sqlite3_step(check):SQLITE_ERROR;sqlite3_finalize(check);
        if(found==SQLITE_ROW)continue;if(found!=SQLITE_DONE){ok=false;break;}
        sqlite3_stmt* s=XP_SQL(G_DB,"INSERT OR IGNORE INTO account(member_id,cash)VALUES(?,?)");
        if(s){sqlite3_bind_int64(s,1,owner);sqlite3_bind_int64(s,2,cents*10000);}ok=XP_Done(s) &&
            Billing_Ledger(owner,key,"migration",cents*10000,"cash","","Imported legacy cent balance","system");
    }
    if(rc!=SQLITE_DONE)ok=false;sqlite3_finalize(source);return Billing_End(ok);
}
