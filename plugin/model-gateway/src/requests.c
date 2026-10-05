static xvalue* Gateway_PriceSnapshot(const GatewayCall* call)
{
    xvalue* value=ValueObject();ValueSetInt(value,"version",call->model.price.version);ValueSetText(value,"currency","CNY");
    ValueSetInt(value,"amount_scale",1000000);ValueSetInt(value,"price_token_unit",1000000);
    ValueSetOwn(value,"sale_rates",Gateway_RatesValue(&call->model.price.sale));
    ValueSetOwn(value,"cost_rates",call->model.price.cost_known?Gateway_RatesValue(&call->model.price.cost):xrtValueNull());
    ValueSetInt(value,"discount_bps",call->discount_bps);ValueSetInt(value,"context_window",call->model.context_window);ValueSetInt(value,"output_limit",call->output_limit);return value;
}
static int Gateway_Quota(int64_t owner,int kind,int64_t window,int limit)
{
    sqlite3_stmt* s=XP_SQL(G_DB,"INSERT OR IGNORE INTO quota VALUES(?,?,?,0)");
    if(s){sqlite3_bind_int64(s,1,owner);sqlite3_bind_int(s,2,kind);sqlite3_bind_int64(s,3,window);}if(!XP_Done(s))return 503;
    s=XP_SQL(G_DB,"UPDATE quota SET used=used+1 WHERE owner=? AND kind=? AND window=? AND used<?");
    if(s){sqlite3_bind_int64(s,1,owner);sqlite3_bind_int(s,2,kind);sqlite3_bind_int64(s,3,window);sqlite3_bind_int(s,4,limit);}
    if(!XP_Done(s))return 503;return sqlite3_changes(G_DB)==1?0:429;
}
static int Gateway_Duplicate(int64_t owner,const char* key,const char* hash,const char* protocol,char id[33])
{
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT id,body_hash,protocol FROM request WHERE member_id=? AND client_key=?");
    if(s)sqlite3_bind_int64(s,1,owner);XP_Bind(s,2,key);int rc=s?sqlite3_step(s):SQLITE_ERROR;int result=0;
    if(rc==SQLITE_ROW){snprintf(id,33,"%s",sqlite3_column_text(s,0));
        result=!strcmp((const char*)sqlite3_column_text(s,1),hash) && !strcmp((const char*)sqlite3_column_text(s,2),protocol)?409:422;}
    else if(rc!=SQLITE_DONE)result=503;sqlite3_finalize(s);return result;
}
static int Gateway_Admit(const GatewayCall* call,const char* key,const char* hash,const char* project,const char* session,const char* task)
{
    if(!Gateway_Begin())return 503;
    int64_t now=(int64_t)time(NULL);sqlite3_stmt* s=XP_SQL(G_DB,"SELECT COALESCE(SUM(CASE WHEN state IN('admitted','running','reserved','pending','settling') THEN reserved WHEN state='settled' THEN charged ELSE 0 END),0) FROM request WHERE member_id=? AND created_at>=?");
    if(s){sqlite3_bind_int64(s,1,call->owner);sqlite3_bind_int64(s,2,now/86400*86400);}
    bool ok=s && sqlite3_step(s)==SQLITE_ROW;int64_t used=ok?sqlite3_column_int64(s,0):0;sqlite3_finalize(s);
    if(!ok){Gateway_End(false);return 503;}
    if(call->reserved>G_Config.daily_budget || used>G_Config.daily_budget-call->reserved){Gateway_End(false);return 429;}
    int status=Gateway_Quota(call->owner,0,now/60,G_Config.minute_limit);
    if(!status)status=Gateway_Quota(call->owner,1,now/86400,G_Config.daily_limit);
    if(!status)status=Gateway_Quota(0,2,now/86400,G_Config.global_daily_limit);
    if(status){Gateway_End(false);return status;}
    xvalue* snapshot=Gateway_PriceSnapshot(call);char* json=xrtJsonStringify(snapshot,false,NULL);xrtValueRelease(snapshot);
    s=json?XP_SQL(G_DB,"INSERT INTO request(id,member_id,client_key,body_hash,model_id,channel_id,protocol,wire_model,price_version,price_snapshot,discount_bps,reserved,state,project_id,session_id,task_id,created_at)VALUES(?,?,?,?,?,?,?,?,?,?,?,?,'admitted',?,?,?,?)"):NULL;
    XP_Bind(s,1,call->id);if(s){sqlite3_bind_int64(s,2,call->owner);sqlite3_bind_int64(s,9,call->model.price.version);sqlite3_bind_int(s,11,call->discount_bps);sqlite3_bind_int64(s,12,call->reserved);sqlite3_bind_int64(s,16,now);}
    XP_Bind(s,3,key);XP_Bind(s,4,hash);XP_Bind(s,5,call->model.id);XP_Bind(s,6,call->channel.id);XP_Bind(s,7,call->protocol);XP_Bind(s,8,call->wire_model);XP_Bind(s,10,json);XP_Bind(s,13,project);XP_Bind(s,14,session);XP_Bind(s,15,task);
    ok=XP_Done(s);xrtFree(json);return Gateway_End(ok)?0:503;
}
static bool Gateway_State(const char* id,const char* state,const char* error)
{
    sqlite3_stmt* s=XP_SQL(G_DB,"UPDATE request SET state=?,error=?,charged=CASE WHEN ? IN('released','rejected') THEN 0 ELSE charged END WHERE id=?");XP_Bind(s,1,state);XP_Bind(s,2,error);XP_Bind(s,3,state);XP_Bind(s,4,id);return XP_Done(s) && sqlite3_changes(G_DB)==1;
}
static bool Gateway_Receipt(const char* id,const XBillingResult* result)
{
    /* A financial hold is not an execution status. Reading a running receipt
     * must not erase a running state or a durable, unfinished settlement. */
    sqlite3_stmt* s=XP_SQL(G_DB,"UPDATE request SET state=CASE WHEN ?1='reserved' OR (?1='pending' AND state='settling' AND final_outcome<>'pending') THEN state ELSE ?1 END,charged=?2 WHERE id=?3");XP_Bind(s,1,result->state);XP_Bind(s,3,id);
    if(s){if(!strcmp(result->state,"pending") || !strcmp(result->state,"reserved"))sqlite3_bind_null(s,2);else sqlite3_bind_int64(s,2,result->charged);}
    return XP_Done(s) && sqlite3_changes(G_DB)==1;
}
static bool Gateway_FinalRecord(GatewayCall* call,const char* outcome,int64_t amount,bool verified,const char* error,char** usage_text)
{
    xvalue* usage=Gateway_UsageValue(call,verified);*usage_text=xrtJsonStringify(usage,false,NULL);xrtValueRelease(usage);if(!*usage_text)return false;
    int64_t cost=0;bool cost_known=verified && call->model.price.cost_known && Gateway_Calculate(&call->usage,&call->model.price.cost,10000,&cost);
    sqlite3_stmt* s=XP_SQL(G_DB,"UPDATE request SET state='settling',final_outcome=?,final_amount=?,usage_json=?,input_tokens=?,cache_read_tokens=?,cache_write_5m_tokens=?,cache_write_1h_tokens=?,output_tokens=?,reasoning_tokens=?,cost=?,upstream_status=?,provider_request_id=?,error=?,first_byte_ms=?,total_ms=? WHERE id=?");
    XP_Bind(s,1,outcome);XP_Bind(s,3,*usage_text);XP_Bind(s,12,call->provider_request_id);XP_Bind(s,13,error);XP_Bind(s,16,call->id);
    if(s){sqlite3_bind_int64(s,2,amount);
        if(verified){sqlite3_bind_int64(s,4,call->usage.input);sqlite3_bind_int64(s,5,call->usage.cache_read);sqlite3_bind_int64(s,6,call->usage.cache_write_5m);sqlite3_bind_int64(s,7,call->usage.cache_write_1h);sqlite3_bind_int64(s,8,call->usage.output);sqlite3_bind_int64(s,9,call->usage.reasoning);}
        else{int i;for(i=4;i<=9;i++)sqlite3_bind_null(s,i);}
        if(cost_known)sqlite3_bind_int64(s,10,cost);else sqlite3_bind_null(s,10);
        if(call->upstream_status)sqlite3_bind_int(s,11,call->upstream_status);else sqlite3_bind_null(s,11);
        if(call->first_byte_us)sqlite3_bind_int64(s,14,(call->first_byte_us-call->started_us)/1000);else sqlite3_bind_null(s,14);
        sqlite3_bind_int64(s,15,(xrtNow()-call->started_us)/1000);}
    return XP_Done(s) && sqlite3_changes(G_DB)==1;
}
static bool Gateway_Sync(bool restart)
{
    if(restart && sqlite3_exec(G_DB,"UPDATE request SET state='settling',final_outcome='pending',final_amount=0,error='process_interrupted',usage_json=COALESCE(usage_json,'{\"usage_source\":\"unresolved\"}') WHERE state IN('admitted','running')",NULL,NULL,NULL)!=SQLITE_OK)return false;
    XAdminServiceLease lease;const XBillingService* billing=Gateway_Billing(&lease);if(!billing)return !restart;
    if(billing->maintain()){XAdmin_ReleaseService(lease);return false;}
    /* Durable receipt feed also catches refunds issued while this plugin was
     * disabled. Cursor and copied receipts advance together in one transaction. */
    sqlite3_stmt* cursor=XP_SQL(G_DB,"SELECT sequence FROM receipt_cursor WHERE id=1");
    bool cursor_ok=cursor && sqlite3_step(cursor)==SQLITE_ROW;int64_t sequence=cursor_ok?sqlite3_column_int64(cursor,0):0;sqlite3_finalize(cursor);
    xvalue* changes=cursor_ok?billing->changes("model-gateway",sequence,100):NULL;
    if(!changes || !Gateway_Begin()){xrtValueRelease(changes);XAdmin_ReleaseService(lease);return false;}
    bool synced=true;uint32 change_index;
    for(change_index=0;synced && change_index<ValueCount(changes);change_index++){
        xvalue* change=xrtValueArrayGet(changes,change_index);
        sqlite3_stmt* update=XP_SQL(G_DB,"UPDATE request SET state=?,charged=? WHERE id=? AND member_id=?");
        XP_Bind(update,1,ValueText(change,"state"));XP_Bind(update,3,ValueText(change,"request_id"));if(update){sqlite3_bind_int64(update,2,ValueInt(change,"charged"));sqlite3_bind_int64(update,4,ValueInt(change,"member_id"));}
        synced=XP_Done(update);sequence=ValueInt(change,"sequence");
    }
    cursor=synced?XP_SQL(G_DB,"UPDATE receipt_cursor SET sequence=? WHERE id=1"):NULL;if(cursor)sqlite3_bind_int64(cursor,1,sequence);
    synced=Gateway_End(synced && XP_Done(cursor));xrtValueRelease(changes);if(!synced){XAdmin_ReleaseService(lease);return false;}
    /* No borrowed SQL statement spans another plugin's transaction. */
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT id,state,final_outcome,final_amount,usage_json FROM request WHERE state IN('settling','pending') ORDER BY CASE WHEN state='settling' THEN 0 ELSE 1 END,created_at LIMIT 100");
    xvalue* rows=XP_Rows(s);bool ok=rows!=NULL;uint32 i;
    for(i=0;ok && i<ValueCount(rows);i++){
        xvalue* row=xrtValueArrayGet(rows,i);const char* id=ValueText(row,"id");XBillingResult result={sizeof(result)};int status=billing->lookup(id,&result);
        if(status==404){ok=Gateway_State(id,"released","admission_interrupted");continue;}
        if(status){ok=false;break;}
        const char* outcome=ValueText(row,"final_outcome");
        if(!strcmp(result.state,"reserved") || !strcmp(result.state,"pending")){
            if(outcome && (!strcmp(ValueText(row,"state"),"settling") || !strcmp(result.state,"reserved"))){
                XBillingSettlement settlement={sizeof(settlement),id,ValueInt(row,"final_amount"),ValueText(row,"usage_json"),outcome};
                if(billing->finalize(&settlement) || billing->lookup(id,&result)){ok=false;break;}
            }
        }
        ok=Gateway_Receipt(id,&result);
    }xrtValueRelease(rows);XAdmin_ReleaseService(lease);return ok;
}
static bool Gateway_Recover(void){return Gateway_Sync(true);}
static void Gateway_BillingEvent(const char* name,void* payload,size_t size)
{
    (void)name;if(!G_DB || size!=sizeof(XBillingEvent) || !payload)return;
    const XBillingEvent* event=payload;
    if(event->size!=sizeof(*event) || !memchr(event->request_id,0,sizeof(event->request_id)) ||
        !memchr(event->state,0,sizeof(event->state)) || !XP_Id(event->request_id,96) || event->charged<0 ||
        (strcmp(event->state,"settled") && strcmp(event->state,"released") && strcmp(event->state,"refunded")))return;
    sqlite3_stmt* s=XP_SQL(G_DB,"UPDATE request SET state=?,charged=? WHERE id=? AND member_id=?");
    XP_Bind(s,1,event->state);XP_Bind(s,3,event->request_id);if(s){sqlite3_bind_int64(s,2,event->charged);sqlite3_bind_int64(s,4,event->member_id);}XP_Done(s);
}
