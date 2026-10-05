static xvalue* Gateway_ModelValue(const GatewayModel* model,bool admin)
{
    xvalue* value=ValueObject();ValueSetText(value,"id",model->id);ValueSetText(value,"title",model->title);ValueSetText(value,"description",model->description);
    ValueSetInt(value,"context_window",model->context_window);ValueSetInt(value,"max_output",model->max_output);
    ValueSetBool(value,"enabled",model->enabled);ValueSetBool(value,"member_only",model->member_only);
    ValueSetBool(value,"tool_calling",model->tool_calling);ValueSetBool(value,"vision",model->vision);
    ValueSetText(value,"reasoning_efforts",model->reasoning);ValueSetText(value,"default_protocol",model->default_protocol);
    ValueSetText(value,"output_limit_field",model->output_field);ValueSetBool(value,"free",Gateway_Free(model));
    ValueSetText(value,"currency","CNY");ValueSetInt(value,"amount_scale",1000000);ValueSetInt(value,"price_token_unit",1000000);
    ValueSetInt(value,"price_version",model->price.version);ValueSetOwn(value,"sale_rates",Gateway_RatesValue(&model->price.sale));
    if(admin)ValueSetOwn(value,"cost_rates",model->price.cost_known?Gateway_RatesValue(&model->price.cost):xrtValueNull());
    return value;
}
static void Gateway_Models(XS_ServerObject server,XS_HostObject host,XS_RequestObject req,XS_ResponseObject resp,xvalue* session)
{
    (void)server;(void)host;if(!XP_Member(req,resp,session,XHTTP_METHOD_GET))return;
    XAdminServiceLease lease;const XBillingService* billing=Gateway_Billing(&lease);XBillingEntitlement e={sizeof(e)};
    int status=billing?billing->entitlement(ValueInt(session,"id"),&e):503;if(lease)XAdmin_ReleaseService(lease);
    if(status){XP_Reply(resp,503,"Billing unavailable",NULL);return;}
    /* Read credentials once per catalog, rather than once per model/route. */
    bool invalid=false;xvalue* keys=Gateway_ReadKeys(&invalid);char ready[128][65];size_t ready_count=0;
    xvalue* channels=XP_Rows(XP_SQL(G_DB,"SELECT id FROM channel WHERE enabled=1 ORDER BY id LIMIT 128"));uint32 i;
    for(i=0;i<ValueCount(channels);i++){
        const char* id=ValueText(xrtValueArrayGet(channels,i),"id");GatewayChannel c;if(!Gateway_ChannelRead(id,&c))continue;
        char env[100];Gateway_KeyEnv(id,env);const char* key=getenv(env);
        bool valid=!strcmp(c.auth,"none") || (key && *key?Gateway_ValidKey(key):!invalid && XP_Text(keys,id,1000) && *XP_Text(keys,id,1000));
        if(valid && ready_count<128)snprintf(ready[ready_count++],65,"%s",id);
    }xrtValueRelease(channels);Gateway_WipeKeys(keys);
    bool rich=!strcmp(XAdmin_ReqPath(req),"/api/v1/ai/catalog");
    xvalue* models=XP_Rows(XP_SQL(G_DB,"SELECT id FROM model WHERE enabled=1 ORDER BY id LIMIT 256")),*list=ValueArray();bool ok=models!=NULL;
    for(i=0;ok && i<ValueCount(models);i++){
        GatewayModel model;if(!Gateway_ModelRead(ValueText(xrtValueArrayGet(models,i),"id"),&model)){ok=false;break;}
        sqlite3_stmt* s=XP_SQL(G_DB,"SELECT protocol,channel_id FROM route WHERE model_id=? ORDER BY priority,channel_id");XP_Bind(s,1,model.id);xvalue* rows=XP_Rows(s),*protocols=ValueArray();
        bool online=false;if(!rows){xrtValueRelease(protocols);ok=false;break;}uint32 j;
        for(j=0;j<ValueCount(rows);j++){
            xvalue* row=xrtValueArrayGet(rows,j);const char* channel=ValueText(row,"channel_id"),*protocol=ValueText(row,"protocol");size_t k;
            for(k=0;k<ready_count;k++)if(!strcmp(ready[k],channel))break;
            if(k==ready_count)continue;online=true;
            bool exists=false;uint32 p;for(p=0;p<ValueCount(protocols);p++)if(!strcmp(ValueTextOf(xrtValueArrayGet(protocols,p)),protocol))exists=true;
            if(!exists)ValueArrayOwn(protocols,xrtValueString(xrtStrView(protocol)));
        }xrtValueRelease(rows);
        bool allowed=Gateway_AllowedModel(&model,&e);
        if(rich){xvalue* value=Gateway_ModelValue(&model,false);ValueSetBool(value,"available",online && allowed);
            ValueSetText(value,"unavailable_reason",!allowed?"membership_required":!online?"channel_unavailable":"");
            ValueSetInt(value,"discount_bps",e.discount_bps);ValueSetOwn(value,"protocols",protocols);ValueArrayOwn(list,value);}
        else{if(online && allowed){xvalue* value=ValueObject();ValueSetText(value,"id",model.id);ValueSetText(value,"object","model");ValueSetInt(value,"created",0);ValueSetText(value,"owned_by","gateway");ValueArrayOwn(list,value);}xrtValueRelease(protocols);}
    }xrtValueRelease(models);
    if(!ok){xrtValueRelease(list);XP_Reply(resp,503,"Catalog unavailable",NULL);return;}
    if(rich){xvalue* value=ValueObject();ValueSetOwn(value,"models",list);char version[65];if(Gateway_Fingerprint(value,version))ValueSetText(value,"version",version);XP_Reply(resp,200,"",value);}
    else{xvalue* value=ValueObject();ValueSetText(value,"object","list");ValueSetOwn(value,"data",list);char* text=xrtJsonStringify(value,false,NULL);
        xsHttpReplyAuto(resp,200,"Content-Type: application/json; charset=utf-8\r\nCache-Control: no-store\r\n",text?text:"{}",text?strlen(text):2);xrtFree(text);xrtValueRelease(value);}
}
static void Gateway_RequestDetail(XS_ServerObject server,XS_HostObject host,XS_RequestObject req,XS_ResponseObject resp,xvalue* session)
{
    (void)server;(void)host;if(!XP_Member(req,resp,session,XHTTP_METHOD_GET))return;
    char id[97];if(XAdmin_RouteParam(0,id,sizeof(id))<0 || !XP_Id(id,96)){XP_Reply(resp,404,"Request not found",NULL);return;}
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT 1 FROM request WHERE id=? AND member_id=?");XP_Bind(s,1,id);if(s)sqlite3_bind_int64(s,2,ValueInt(session,"id"));
    bool exists=s && sqlite3_step(s)==SQLITE_ROW;sqlite3_finalize(s);if(!exists){XP_Reply(resp,404,"Request not found",NULL);return;}
    XAdminServiceLease lease;const XBillingService* billing=Gateway_Billing(&lease);XBillingResult result={sizeof(result)};
    if(billing && !billing->lookup(id,&result))Gateway_Receipt(id,&result);if(lease)XAdmin_ReleaseService(lease);
    s=XP_SQL(G_DB,"SELECT id,model_id,protocol,price_version,price_snapshot,discount_bps,reserved AS reserved_micros,charged AS charged_micros,state,usage_json,upstream_status,error,project_id,session_id,task_id,created_at,first_byte_ms,total_ms FROM request WHERE id=? AND member_id=?");
    XP_Bind(s,1,id);if(s)sqlite3_bind_int64(s,2,ValueInt(session,"id"));xvalue* rows=XP_Rows(s);
    xvalue* row=ValueCount(rows)==1?xrtValueDeepClone(xrtValueArrayGet(rows,0)):NULL;xrtValueRelease(rows);
    /* Cost is private supplier accounting, not part of a member's receipt. */
    const char* snapshot=ValueText(row,"price_snapshot");if(snapshot){xvalue* value=xrtJsonParse(xrtStrView(snapshot));if(value)xrtValueObjectRemove(value,XRT_STR_LITERAL("cost_rates"));ValueSetOwn(row,"price_snapshot",value);}
    const char* usage=ValueText(row,"usage_json");if(usage){xvalue* value=xrtJsonParse(xrtStrView(usage));ValueSetOwn(row,"usage",value);xrtValueObjectRemove(row,XRT_STR_LITERAL("usage_json"));}
    if(row){bool refunded=!strcmp(ValueText(row,"state"),"refunded");ValueSetInt(row,"refunded_micros",refunded?ValueInt(row,"charged_micros"):0);
        if(xrtValueType(ValueGet(row,"charged_micros"))==XVALUE_NULL)ValueSetOwn(row,"net_charged_micros",xrtValueNull());else ValueSetInt(row,"net_charged_micros",refunded?0:ValueInt(row,"charged_micros"));}
    XP_Reply(resp,row?200:503,row?"":"Receipt unavailable",row);
}
static void Gateway_Usage(XS_ServerObject server,XS_HostObject host,XS_RequestObject req,XS_ResponseObject resp,xvalue* session)
{
    (void)server;(void)host;if(!XP_Member(req,resp,session,XHTTP_METHOD_GET))return;
    if(!Gateway_Sync(false)){XP_Reply(resp,503,"Billing reconciliation unavailable",NULL);return;}
    char days_text[8];int64_t days=30;if(xsReqQueryValue(req,"days",days_text,sizeof(days_text))>0 && (!XP_Positive(days_text,&days) || days>90)){XP_Reply(resp,400,"days must be 1..90",NULL);return;}
    int64_t since=(int64_t)time(NULL)-days*86400,owner=ValueInt(session,"id");xvalue* data=ValueObject();ValueSetInt(data,"since",since);ValueSetText(data,"currency","CNY");ValueSetInt(data,"amount_scale",1000000);
    XAdminServiceLease lease;const XBillingService* billing=Gateway_Billing(&lease);if(billing)billing->maintain();if(lease)XAdmin_ReleaseService(lease);
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT COUNT(*) AS requests,COALESCE(SUM(CASE WHEN state='refunded' THEN 0 ELSE charged END),0) AS charged_micros,COALESCE(SUM(charged),0) AS gross_charged_micros,COALESCE(SUM(CASE WHEN state='refunded' THEN charged ELSE 0 END),0) AS refunded_micros,SUM(CASE WHEN state IN('pending','settling','running','admitted','reserved') THEN 1 ELSE 0 END) AS unresolved_requests,COALESCE(SUM(input_tokens+cache_read_tokens+cache_write_5m_tokens+cache_write_1h_tokens),0) AS known_input_tokens,COALESCE(SUM(output_tokens),0) AS known_output_tokens,SUM(CASE WHEN input_tokens IS NULL THEN 1 ELSE 0 END) AS unknown_usage_requests FROM request WHERE member_id=? AND created_at>=?");
    if(s){sqlite3_bind_int64(s,1,owner);sqlite3_bind_int64(s,2,since);}xvalue* totals=XP_Rows(s);ValueSetOwn(data,"totals",totals);
    s=XP_SQL(G_DB,"SELECT model_id,COUNT(*) AS requests,COALESCE(SUM(CASE WHEN state='refunded' THEN 0 ELSE charged END),0) AS charged_micros,COALESCE(SUM(input_tokens+cache_read_tokens+cache_write_5m_tokens+cache_write_1h_tokens),0) AS known_input_tokens,COALESCE(SUM(output_tokens),0) AS known_output_tokens,SUM(CASE WHEN input_tokens IS NULL THEN 1 ELSE 0 END) AS unknown_usage_requests FROM request WHERE member_id=? AND created_at>=? GROUP BY model_id ORDER BY requests DESC LIMIT 256");
    if(s){sqlite3_bind_int64(s,1,owner);sqlite3_bind_int64(s,2,since);}xvalue* by_model=XP_Rows(s);ValueSetOwn(data,"by_model",by_model);
    s=XP_SQL(G_DB,"SELECT id,model_id,protocol,state,charged AS charged_micros,error,created_at,first_byte_ms,total_ms FROM request WHERE member_id=? AND created_at>=? ORDER BY created_at DESC,rowid DESC LIMIT 100");
    if(s){sqlite3_bind_int64(s,1,owner);sqlite3_bind_int64(s,2,since);}xvalue* requests=XP_Rows(s);ValueSetOwn(data,"requests",requests);
    bool ok=totals && by_model && requests;if(!ok){xrtValueRelease(data);data=NULL;}XP_Reply(resp,ok?200:503,ok?"":"Usage unavailable",data);
}
