static xvalue* Gateway_AdminState(xvalue* session)
{
    const char* csrf=XAdmin_AdminCSRFToken(session);if(!csrf)return NULL;
    xvalue* data=ValueObject();ValueSetText(data,"csrf_token",csrf);
    bool invalid=false;xvalue* keys=Gateway_ReadKeys(&invalid);ValueSetBool(data,"credentials_invalid",invalid);
    xvalue* channels=XP_Rows(XP_SQL(G_DB,"SELECT * FROM channel ORDER BY id"));uint32 i;
    for(i=0;i<ValueCount(channels);i++){
        xvalue* row=xrtValueArrayGet(channels,i);const char* id=ValueText(row,"id");char env[100];Gateway_KeyEnv(id,env);const char* key=getenv(env);
        ValueSetBool(row,"key_from_environment",key && *key);
        ValueSetBool(row,"key_configured",!strcmp(ValueText(row,"auth"),"none") || (key&&*key?Gateway_ValidKey(key):!invalid && XP_Text(keys,id,1000) && *XP_Text(keys,id,1000)));
    }Gateway_WipeKeys(keys);ValueSetOwn(data,"channels",channels);
    xvalue* ids=XP_Rows(XP_SQL(G_DB,"SELECT id FROM model ORDER BY id")),*models=ValueArray();bool ok=channels && ids;
    for(i=0;ok && i<ValueCount(ids);i++){
        GatewayModel model;if(!Gateway_ModelRead(ValueText(xrtValueArrayGet(ids,i),"id"),&model)){ok=false;break;}
        xvalue* value=Gateway_ModelValue(&model,true);sqlite3_stmt* s=XP_SQL(G_DB,"SELECT protocol,channel_id,wire_model,priority,cost_json FROM route WHERE model_id=? ORDER BY priority,channel_id");XP_Bind(s,1,model.id);xvalue* routes=XP_Rows(s);uint32 j;
        if(!routes){xrtValueRelease(value);ok=false;break;}
        for(j=0;j<ValueCount(routes);j++){xvalue* route=xrtValueArrayGet(routes,j);const char* cost=ValueText(route,"cost_json");
            ValueSetOwn(route,"cost_rates",cost?xrtJsonParse(xrtStrView(cost)):xrtValueNull());xrtValueObjectRemove(route,XRT_STR_LITERAL("cost_json"));}
        ValueSetOwn(value,"routes",routes);ValueArrayOwn(models,value);
    }xrtValueRelease(ids);ValueSetOwn(data,"models",models);
    ValueSetOwn(data,"requests",XP_Rows(XP_SQL(G_DB,"SELECT id,member_id,model_id,channel_id,protocol,state,reserved,charged,cost,input_tokens,cache_read_tokens,cache_write_5m_tokens,cache_write_1h_tokens,output_tokens,reasoning_tokens,upstream_status,error,created_at,first_byte_ms,total_ms FROM request ORDER BY created_at DESC,rowid DESC LIMIT 100")));
    xvalue* config=ValueObject();ValueSetInt(config,"max_concurrent",G_Config.max_concurrent);ValueSetInt(config,"minute_limit",G_Config.minute_limit);ValueSetInt(config,"daily_limit",G_Config.daily_limit);ValueSetInt(config,"global_daily_limit",G_Config.global_daily_limit);ValueSetInt(config,"daily_budget_micros",G_Config.daily_budget);ValueSetOwn(data,"limits",config);
    if(!ok || !ValueGet(data,"requests")){xrtValueRelease(data);return NULL;}return data;
}
static void Gateway_Admin(XS_ServerObject server,XS_HostObject host,XS_RequestObject req,XS_ResponseObject resp,xvalue* session)
{
    (void)server;(void)host;const char* path=XAdmin_ReqPath(req);
    if(!strcmp(path,"/admin/model-gateway/state") && xsReqMethodID(req)==XHTTP_METHOD_GET){
        xvalue* data=Gateway_AdminState(session);XP_Reply(resp,data?200:503,data?"":"Gateway unavailable",data);return;}
    if(xsReqMethodID(req)!=XHTTP_METHOD_POST){XP_Reply(resp,405,"POST required",NULL);return;}
    if(!XAdmin_CheckAdminCSRF(req,session)){XP_Reply(resp,403,"CSRF verification failed",NULL);return;}
    xvalue* body=XP_Body(req,32768);int status=400;
    if(!strcmp(path,"/admin/model-gateway/channel"))status=Gateway_SaveChannel(body);
    else if(!strcmp(path,"/admin/model-gateway/model"))status=Gateway_SaveModel(body);
    else if(!strcmp(path,"/admin/model-gateway/credentials")){
        status=Gateway_SaveKey(body);const char* key=XP_Text(body,"api_key",1000);if(key)xrtSecureZero((void*)key,strlen(key));
        const char* raw=XAdmin_ReqBody(req);if(raw)xrtSecureZero((void*)raw,XAdmin_ReqBodyLen(req));
    }
    xrtValueRelease(body);XP_Reply(resp,status?status:200,status?"Configuration rejected":"Saved",NULL);
}
static void Gateway_Page(XS_ServerObject server,XS_HostObject host,XS_RequestObject req,XS_ResponseObject resp,xvalue* session)
{
    (void)server;(void)host;(void)session;
    if(xsReqMethodID(req)!=XHTTP_METHOD_GET){XP_Reply(resp,405,"GET required",NULL);return;}
    XAdmin_LoadPluginPage(G_Handle,resp,200,"Content-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\n",!strcmp(XAdmin_ReqPath(req),"/account/models")?"account.html":"admin.html");
}
