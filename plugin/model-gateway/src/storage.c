static bool Gateway_Begin(void){return sqlite3_exec(G_DB,"BEGIN IMMEDIATE",NULL,NULL,NULL)==SQLITE_OK;}
static bool Gateway_End(bool ok)
{
    if(ok && sqlite3_exec(G_DB,"COMMIT",NULL,NULL,NULL)==SQLITE_OK)return true;
    sqlite3_exec(G_DB,"ROLLBACK",NULL,NULL,NULL);return false;
}
static bool Gateway_Schema(void)
{
    if(!XP_SchemaSupported(G_DB,2))return false;
    return sqlite3_exec(G_DB,
        "PRAGMA foreign_keys=ON;PRAGMA journal_mode=WAL;PRAGMA synchronous=FULL;"
        "CREATE TABLE IF NOT EXISTS channel(id TEXT PRIMARY KEY,title TEXT NOT NULL,provider TEXT NOT NULL,protocol TEXT NOT NULL,url TEXT NOT NULL,auth TEXT NOT NULL,anthropic_version TEXT NOT NULL,timeout_ms INTEGER NOT NULL,first_byte_ms INTEGER NOT NULL,idle_ms INTEGER NOT NULL,max_concurrent INTEGER NOT NULL,enabled INTEGER NOT NULL,allow_http INTEGER NOT NULL);"
        "CREATE TABLE IF NOT EXISTS model(id TEXT PRIMARY KEY,title TEXT NOT NULL,description TEXT NOT NULL,context_window INTEGER NOT NULL,max_output INTEGER NOT NULL,enabled INTEGER NOT NULL,member_only INTEGER NOT NULL,tool_calling INTEGER NOT NULL,vision INTEGER NOT NULL,reasoning TEXT NOT NULL,output_field TEXT NOT NULL,default_protocol TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS route(model_id TEXT NOT NULL REFERENCES model(id),protocol TEXT NOT NULL,channel_id TEXT NOT NULL REFERENCES channel(id),wire_model TEXT NOT NULL,priority INTEGER NOT NULL,cost_json TEXT,PRIMARY KEY(model_id,protocol,channel_id));"
        "CREATE TABLE IF NOT EXISTS price(version INTEGER PRIMARY KEY,model_id TEXT NOT NULL REFERENCES model(id),sale_json TEXT NOT NULL,cost_json TEXT,created_at INTEGER NOT NULL);"
        "CREATE TRIGGER IF NOT EXISTS price_no_update BEFORE UPDATE ON price BEGIN SELECT RAISE(ABORT,'immutable price'); END;"
        "CREATE TRIGGER IF NOT EXISTS price_no_delete BEFORE DELETE ON price BEGIN SELECT RAISE(ABORT,'immutable price'); END;"
        "CREATE TABLE IF NOT EXISTS request(id TEXT PRIMARY KEY,member_id INTEGER NOT NULL,client_key TEXT NOT NULL,body_hash TEXT NOT NULL,model_id TEXT NOT NULL,channel_id TEXT NOT NULL,protocol TEXT NOT NULL,wire_model TEXT NOT NULL,price_version INTEGER NOT NULL,price_snapshot TEXT NOT NULL,discount_bps INTEGER NOT NULL,reserved INTEGER NOT NULL,charged INTEGER,cost INTEGER,state TEXT NOT NULL,final_outcome TEXT,final_amount INTEGER,usage_json TEXT,input_tokens INTEGER,cache_read_tokens INTEGER,cache_write_5m_tokens INTEGER,cache_write_1h_tokens INTEGER,output_tokens INTEGER,reasoning_tokens INTEGER,upstream_status INTEGER,provider_request_id TEXT,error TEXT,project_id TEXT NOT NULL,session_id TEXT NOT NULL,task_id TEXT NOT NULL,created_at INTEGER NOT NULL,first_byte_ms INTEGER,total_ms INTEGER,UNIQUE(member_id,client_key));"
        "CREATE INDEX IF NOT EXISTS request_owner ON request(member_id,created_at);"
        "CREATE TABLE IF NOT EXISTS quota(owner INTEGER NOT NULL,kind INTEGER NOT NULL,window INTEGER NOT NULL,used INTEGER NOT NULL CHECK(used>=0),PRIMARY KEY(owner,kind,window));"
        "CREATE TABLE IF NOT EXISTS receipt_cursor(id INTEGER PRIMARY KEY CHECK(id=1),sequence INTEGER NOT NULL);"
        "INSERT OR IGNORE INTO receipt_cursor VALUES(1,0);"
        "PRAGMA user_version=2;",NULL,NULL,NULL)==SQLITE_OK;
}
static bool Gateway_Protocol(const char* protocol)
{return protocol && (!strcmp(protocol,"chat") || !strcmp(protocol,"responses") || !strcmp(protocol,"anthropic"));}
static const XBillingService* Gateway_Billing(XAdminServiceLease* lease)
{
    const void* table=NULL;*lease=NULL;
    if(XAdmin_AcquireService(G_Handle,XADMIN_BILLING_SERVICE,1,lease,&table))return NULL;
    const XBillingService* billing=table;
    if(!billing || billing->size<sizeof(*billing) || billing->version!=1){XAdmin_ReleaseService(*lease);*lease=NULL;return NULL;}
    return billing;
}
static int Gateway_BillingFinish(const XBillingSettlement* settlement)
{
    XAdminServiceLease lease;const XBillingService* billing=Gateway_Billing(&lease);
    int status=billing?billing->finalize(settlement):503;if(lease)XAdmin_ReleaseService(lease);return status;
}
