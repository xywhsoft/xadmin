static bool Gateway_RatesRead(const xvalue* value,GatewayRates* rates)
{
    const char* names[]={"input","cache_read","cache_write_5m","cache_write_1h","output"};
    return XP_Fields(value,names,5) && XP_Int(value,names[0],0,1000000000000LL,&rates->input) &&
        XP_Int(value,names[1],0,1000000000000LL,&rates->cache_read) &&
        XP_Int(value,names[2],0,1000000000000LL,&rates->cache_write_5m) &&
        XP_Int(value,names[3],0,1000000000000LL,&rates->cache_write_1h) && XP_Int(value,names[4],0,1000000000000LL,&rates->output);
}
static xvalue* Gateway_RatesValue(const GatewayRates* rates)
{
    xvalue* value=ValueObject();ValueSetInt(value,"input",rates->input);ValueSetInt(value,"cache_read",rates->cache_read);
    ValueSetInt(value,"cache_write_5m",rates->cache_write_5m);ValueSetInt(value,"cache_write_1h",rates->cache_write_1h);ValueSetInt(value,"output",rates->output);return value;
}
static bool Gateway_Free(const GatewayModel* model)
{
    const GatewayRates* r=&model->price.sale;
    return !r->input && !r->cache_read && !r->cache_write_5m && !r->cache_write_1h && !r->output;
}
static bool Gateway_ChannelRead(const char* id,GatewayChannel* channel)
{
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT id,title,provider,protocol,url,auth,anthropic_version,timeout_ms,first_byte_ms,idle_ms,max_concurrent,enabled,allow_http FROM channel WHERE id=?");XP_Bind(s,1,id);
    bool ok=s && sqlite3_step(s)==SQLITE_ROW;
    if(ok){memset(channel,0,sizeof(*channel));
        snprintf(channel->id,sizeof(channel->id),"%s",sqlite3_column_text(s,0));snprintf(channel->title,sizeof(channel->title),"%s",sqlite3_column_text(s,1));
        snprintf(channel->provider,sizeof(channel->provider),"%s",sqlite3_column_text(s,2));snprintf(channel->protocol,sizeof(channel->protocol),"%s",sqlite3_column_text(s,3));
        snprintf(channel->url,sizeof(channel->url),"%s",sqlite3_column_text(s,4));snprintf(channel->auth,sizeof(channel->auth),"%s",sqlite3_column_text(s,5));
        snprintf(channel->anthropic_version,sizeof(channel->anthropic_version),"%s",sqlite3_column_text(s,6));
        channel->timeout_ms=sqlite3_column_int(s,7);channel->first_byte_ms=sqlite3_column_int(s,8);channel->idle_ms=sqlite3_column_int(s,9);
        channel->max_concurrent=sqlite3_column_int(s,10);channel->enabled=sqlite3_column_int(s,11)!=0;channel->allow_http=sqlite3_column_int(s,12)!=0;}
    sqlite3_finalize(s);return ok;
}
static bool Gateway_PriceRead(const char* id,GatewayPrice* price)
{
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT version,sale_json,cost_json FROM price WHERE model_id=? ORDER BY version DESC LIMIT 1");XP_Bind(s,1,id);
    bool ok=s && sqlite3_step(s)==SQLITE_ROW;memset(price,0,sizeof(*price));
    if(ok){price->version=sqlite3_column_int64(s,0);xvalue* sale=xrtJsonParse(xrtStrView((const char*)sqlite3_column_text(s,1)));
        ok=Gateway_RatesRead(sale,&price->sale);xrtValueRelease(sale);
        if(sqlite3_column_type(s,2)!=SQLITE_NULL){xvalue* cost=xrtJsonParse(xrtStrView((const char*)sqlite3_column_text(s,2)));
            price->cost_known=Gateway_RatesRead(cost,&price->cost);ok=ok && price->cost_known;xrtValueRelease(cost);}}
    sqlite3_finalize(s);return ok;
}
static bool Gateway_ModelRead(const char* id,GatewayModel* model)
{
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT id,title,description,context_window,max_output,enabled,member_only,tool_calling,vision,reasoning,output_field,default_protocol FROM model WHERE id=?");XP_Bind(s,1,id);
    bool ok=s && sqlite3_step(s)==SQLITE_ROW;
    if(ok){memset(model,0,sizeof(*model));snprintf(model->id,sizeof(model->id),"%s",sqlite3_column_text(s,0));snprintf(model->title,sizeof(model->title),"%s",sqlite3_column_text(s,1));
        snprintf(model->description,sizeof(model->description),"%s",sqlite3_column_text(s,2));model->context_window=sqlite3_column_int(s,3);model->max_output=sqlite3_column_int(s,4);
        model->enabled=sqlite3_column_int(s,5)!=0;model->member_only=sqlite3_column_int(s,6)!=0;model->tool_calling=sqlite3_column_int(s,7)!=0;model->vision=sqlite3_column_int(s,8)!=0;
        snprintf(model->reasoning,sizeof(model->reasoning),"%s",sqlite3_column_text(s,9));snprintf(model->output_field,sizeof(model->output_field),"%s",sqlite3_column_text(s,10));snprintf(model->default_protocol,sizeof(model->default_protocol),"%s",sqlite3_column_text(s,11));}
    sqlite3_finalize(s);return ok && Gateway_PriceRead(id,&model->price);
}
static bool Gateway_Url(const char* url,bool allow_http)
{
    const char* host=NULL;if(!url || strlen(url)>2048)return false;
    if(!strncmp(url,"https://",8))host=url+8;else if(allow_http && !strncmp(url,"http://",7))host=url+7;else return false;
    size_t n=strcspn(host,"/?");if(!n || memchr(host,'@',n))return false;
    const unsigned char* p=(const unsigned char*)url;for(;*p;p++)if(*p<33 || *p>126 || *p=='#' || *p=='\\')return false;
    return true;
}
static int Gateway_SaveChannel(const xvalue* body)
{
    const char* fields[]={"id","title","provider","protocol","url","auth","anthropic_version","timeout_ms","first_byte_ms","idle_ms","max_concurrent","enabled","allow_http"};
    const char* id=XP_Text(body,"id",64),*title=XP_Text(body,"title",128),*provider=XP_Text(body,"provider",64),*protocol=XP_Text(body,"protocol",16),*url=XP_Text(body,"url",2048),*auth=XP_Text(body,"auth",16),*version=XP_Text(body,"anthropic_version",32);
    int64_t timeout,first,idle,concurrency;bool enabled,allow_http;
    if(!XP_Fields(body,fields,13) || !XP_Id(id,64) || !title || !*title || !XP_Id(provider,64) || !Gateway_Protocol(protocol) ||
        !auth || (strcmp(auth,"bearer") && strcmp(auth,"x-api-key") && strcmp(auth,"api-key") && strcmp(auth,"none")) ||
        !XP_Id(version,32) || !XP_Int(body,"timeout_ms",1000,600000,&timeout) || !XP_Int(body,"first_byte_ms",1000,600000,&first) ||
        !XP_Int(body,"idle_ms",1000,600000,&idle) || first>timeout || idle>timeout || !XP_Int(body,"max_concurrent",1,16,&concurrency) ||
        !xrtValueGetBool(ValueGet(body,"enabled"),&enabled) || !xrtValueGetBool(ValueGet(body,"allow_http"),&allow_http) || !Gateway_Url(url,allow_http))return 400;
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT 1 FROM route WHERE channel_id=? AND protocol<>? LIMIT 1");XP_Bind(s,1,id);XP_Bind(s,2,protocol);
    int rc=s?sqlite3_step(s):SQLITE_ERROR;sqlite3_finalize(s);if(rc==SQLITE_ROW)return 409;if(rc!=SQLITE_DONE)return 503;
    s=XP_SQL(G_DB,"SELECT COUNT(*) FROM channel WHERE id<>?");XP_Bind(s,1,id);bool capacity=s && sqlite3_step(s)==SQLITE_ROW && sqlite3_column_int(s,0)<128;sqlite3_finalize(s);if(!capacity)return 409;
    s=XP_SQL(G_DB,"INSERT INTO channel VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET title=excluded.title,provider=excluded.provider,protocol=excluded.protocol,url=excluded.url,auth=excluded.auth,anthropic_version=excluded.anthropic_version,timeout_ms=excluded.timeout_ms,first_byte_ms=excluded.first_byte_ms,idle_ms=excluded.idle_ms,max_concurrent=excluded.max_concurrent,enabled=excluded.enabled,allow_http=excluded.allow_http");
    const char* texts[]={id,title,provider,protocol,url,auth,version};int i;for(i=0;i<7;i++)XP_Bind(s,i+1,texts[i]);
    if(s){sqlite3_bind_int64(s,8,timeout);sqlite3_bind_int64(s,9,first);sqlite3_bind_int64(s,10,idle);sqlite3_bind_int64(s,11,concurrency);sqlite3_bind_int(s,12,enabled);sqlite3_bind_int(s,13,allow_http);}return XP_Done(s)?0:503;
}
static bool Gateway_Reasoning(const char* list)
{
    if(!list || strlen(list)>128)return false;if(!*list)return true;
    const char* p=list;for(;;){const char* end=strchr(p,',');if(!end)end=p+strlen(p);size_t n=(size_t)(end-p);char word[17];
        if(!n || n>=sizeof(word))return false;memcpy(word,p,n);word[n]=0;if(!XP_Id(word,16))return false;if(!*end)return true;p=end+1;}
}
static int Gateway_SaveModel(const xvalue* body)
{
    const char* fields[]={"id","title","description","context_window","max_output","enabled","member_only","tool_calling","vision","reasoning_efforts","output_limit_field","default_protocol","sale_rates","cost_rates","routes"};
    const char* id=XP_Text(body,"id",128),*title=XP_Text(body,"title",128),*description=XP_Text(body,"description",512),*reasoning=XP_Text(body,"reasoning_efforts",128),*field=XP_Text(body,"output_limit_field",32),*default_protocol=XP_Text(body,"default_protocol",16);
    int64_t context,output;bool enabled,member_only,tools,vision;GatewayRates sale={0},cost={0};xvalue* routes=ValueGet(body,"routes"),*cost_value=ValueGet(body,"cost_rates");
    bool has_cost=cost_value && xrtValueType(cost_value)!=XVALUE_NULL;
    if(!XP_Fields(body,fields,15) || !XP_Id(id,128) || !title || !*title || !description || !Gateway_Reasoning(reasoning) ||
        !field || (strcmp(field,"max_tokens") && strcmp(field,"max_completion_tokens")) || !Gateway_Protocol(default_protocol) ||
        !XP_Int(body,"context_window",64,2000000,&context) || !XP_Int(body,"max_output",1,context,&output) ||
        !xrtValueGetBool(ValueGet(body,"enabled"),&enabled) || !xrtValueGetBool(ValueGet(body,"member_only"),&member_only) ||
        !xrtValueGetBool(ValueGet(body,"tool_calling"),&tools) || !xrtValueGetBool(ValueGet(body,"vision"),&vision) ||
        !Gateway_RatesRead(ValueGet(body,"sale_rates"),&sale) || (has_cost && !Gateway_RatesRead(cost_value,&cost)) ||
        !routes || xrtValueType(routes)!=XVALUE_ARRAY || !ValueCount(routes) || ValueCount(routes)>16)return 400;
    uint32 i;bool default_present=false;
    const char* route_fields[]={"protocol","channel_id","wire_model","priority","cost_rates"};
    for(i=0;i<ValueCount(routes);i++){
        xvalue* route=xrtValueArrayGet(routes,i);const char* channel_id=XP_Text(route,"channel_id",64),*protocol=XP_Text(route,"protocol",16),*wire=XP_Text(route,"wire_model",128);int64_t priority;GatewayChannel channel;GatewayRates route_cost;
        xvalue* price=ValueGet(route,"cost_rates");
        if(!XP_Fields(route,route_fields,5) || !XP_Id(channel_id,64) || !Gateway_Protocol(protocol) || !wire || !*wire ||
            !XP_Int(route,"priority",0,1000,&priority) || !Gateway_ChannelRead(channel_id,&channel) || strcmp(channel.protocol,protocol) ||
            (price && xrtValueType(price)!=XVALUE_NULL && !Gateway_RatesRead(price,&route_cost)))return 400;
        if(!strcmp(protocol,default_protocol))default_present=true;
    }if(!default_present)return 400;
    sqlite3_stmt* s=XP_SQL(G_DB,"SELECT COUNT(*) FROM model WHERE id<>?");XP_Bind(s,1,id);bool capacity=s && sqlite3_step(s)==SQLITE_ROW && sqlite3_column_int(s,0)<256;sqlite3_finalize(s);if(!capacity)return 409;
    xvalue* sale_json=Gateway_RatesValue(&sale),*cost_json=has_cost?Gateway_RatesValue(&cost):NULL;
    char* sale_text=xrtJsonStringify(sale_json,false,NULL),*cost_text=has_cost?xrtJsonStringify(cost_json,false,NULL):NULL;
    xrtValueRelease(sale_json);xrtValueRelease(cost_json);if(!sale_text || (has_cost&&!cost_text)){xrtFree(sale_text);xrtFree(cost_text);return 503;}
    if(!Gateway_Begin()){xrtFree(sale_text);xrtFree(cost_text);return 503;}
    s=XP_SQL(G_DB,"INSERT INTO model VALUES(?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET title=excluded.title,description=excluded.description,context_window=excluded.context_window,max_output=excluded.max_output,enabled=excluded.enabled,member_only=excluded.member_only,tool_calling=excluded.tool_calling,vision=excluded.vision,reasoning=excluded.reasoning,output_field=excluded.output_field,default_protocol=excluded.default_protocol");
    XP_Bind(s,1,id);XP_Bind(s,2,title);XP_Bind(s,3,description);XP_Bind(s,10,reasoning);XP_Bind(s,11,field);XP_Bind(s,12,default_protocol);
    if(s){sqlite3_bind_int64(s,4,context);sqlite3_bind_int64(s,5,output);sqlite3_bind_int(s,6,enabled);sqlite3_bind_int(s,7,member_only);sqlite3_bind_int(s,8,tools);sqlite3_bind_int(s,9,vision);}bool ok=XP_Done(s);
    bool same=false;s=ok?XP_SQL(G_DB,"SELECT sale_json,cost_json FROM price WHERE model_id=? ORDER BY version DESC LIMIT 1"):NULL;XP_Bind(s,1,id);
    int rc=s?sqlite3_step(s):SQLITE_ERROR;if(rc==SQLITE_ROW){const char* old_cost=(const char*)sqlite3_column_text(s,1);
        same=!strcmp((const char*)sqlite3_column_text(s,0),sale_text) && ((old_cost && cost_text && !strcmp(old_cost,cost_text)) || (!old_cost&&!cost_text));}
    else if(rc!=SQLITE_DONE)ok=false;sqlite3_finalize(s);
    if(ok&&!same){s=XP_SQL(G_DB,"INSERT INTO price(model_id,sale_json,cost_json,created_at)VALUES(?,?,?,?)");XP_Bind(s,1,id);XP_Bind(s,2,sale_text);
        if(s){if(cost_text)XP_Bind(s,3,cost_text);else sqlite3_bind_null(s,3);sqlite3_bind_int64(s,4,(int64_t)time(NULL));}ok=XP_Done(s);}
    xrtFree(sale_text);xrtFree(cost_text);
    s=ok?XP_SQL(G_DB,"DELETE FROM route WHERE model_id=?"):NULL;XP_Bind(s,1,id);ok=ok && XP_Done(s);
    for(i=0;ok && i<ValueCount(routes);i++){
        xvalue* route=xrtValueArrayGet(routes,i),*price=ValueGet(route,"cost_rates");char* json=NULL;
        if(price && xrtValueType(price)!=XVALUE_NULL){GatewayRates rates;Gateway_RatesRead(price,&rates);xvalue* v=Gateway_RatesValue(&rates);json=xrtJsonStringify(v,false,NULL);xrtValueRelease(v);if(!json){ok=false;break;}}
        s=XP_SQL(G_DB,"INSERT INTO route VALUES(?,?,?,?,?,?)");XP_Bind(s,1,id);XP_Bind(s,2,ValueText(route,"protocol"));XP_Bind(s,3,ValueText(route,"channel_id"));XP_Bind(s,4,ValueText(route,"wire_model"));
        if(s){sqlite3_bind_int64(s,5,ValueInt(route,"priority"));if(json)XP_Bind(s,6,json);else sqlite3_bind_null(s,6);}ok=XP_Done(s);xrtFree(json);
    }
    return Gateway_End(ok)?0:409;
}
static bool Gateway_AllowedModel(const GatewayModel* model,const XBillingEntitlement* entitlement)
{
    if(!model->member_only)return true;if(!entitlement->plan_id[0])return false;
    if(!strcmp(entitlement->model_ids,"*"))return true;
    const char* p=entitlement->model_ids;size_t n=strlen(model->id);
    while(*p){const char* end=strchr(p,',');if(!end)end=p+strlen(p);if((size_t)(end-p)==n && !memcmp(p,model->id,n))return true;if(!*end)break;p=end+1;}return false;
}
