static void Relay_List(XS_ServerObject server,XS_HostObject host,XS_RequestObject req,XS_ResponseObject response,xvalue* session)
{
    (void)server;(void)host;int64 owner=Relay_Member(response,session);
    if(!owner||!Relay_Method(req,response,XHTTP_METHOD_GET))return;
    sqlite3_stmt* statement=Relay_SQL("SELECT id,name,platform,app_version,enabled,revoked,created_at,updated_at,last_seen FROM device WHERE member_id=? ORDER BY created_at,id LIMIT 33");
    if(statement)sqlite3_bind_int64(statement,1,owner);
    xvalue* items=ValueArray();bool ok=statement&&items;int step=SQLITE_ERROR;
    while(ok&&(step=sqlite3_step(statement))==SQLITE_ROW){
        char id[33],name[97],platform[25],version[49];
        ok=Relay_Column(statement,0,id,sizeof(id))&&Relay_Column(statement,1,name,sizeof(name))&&
            Relay_Column(statement,2,platform,sizeof(platform))&&Relay_Column(statement,3,version,sizeof(version));
        RelayConnection* native=ok?Relay_Native(owner,id):NULL;xvalue* item=ok?ValueObject():NULL;
        ok=ok&&item&&ValueSetText(item,"id",id)&&ValueSetText(item,"name",name)&&ValueSetText(item,"platform",platform)&&
            ValueSetText(item,"app_version",version)&&ValueSetBool(item,"allow_remote",sqlite3_column_int(statement,4)!=0)&&
            ValueSetBool(item,"revoked",sqlite3_column_int(statement,5)!=0)&&ValueSetBool(item,"online",native&&native->open)&&
            ValueSetBool(item,"controlled",ok&&Relay_Peers(owner,id,false)>0)&&ValueSetInt(item,"viewers",ok?Relay_Peers(owner,id,true):0)&&
            ValueSetInt(item,"created_at",sqlite3_column_int64(statement,6))&&ValueSetInt(item,"updated_at",sqlite3_column_int64(statement,7))&&
            ValueSetInt(item,"last_seen",sqlite3_column_int64(statement,8));
        if(ok)ok=ValueArrayOwn(items,item);else xrtValueRelease(item);
    }
    sqlite3_finalize(statement);ok=ok&&step==SQLITE_DONE;
    xvalue* data=ok?ValueObject():NULL;
    if(ok)ok=data&&ValueSetBool(data,"enabled",Config.enabled)&&ValueSetInt(data,"protocol_version",1);
    if(ok){ok=ValueSetOwn(data,"devices",items);items=NULL;}
    xrtValueRelease(items);
    if(!ok){xrtValueRelease(data);Relay_Reply(response,500,"device storage unavailable",NULL);return;}
    Relay_Reply(response,200,"",data);
}
static void Relay_Register(XS_ServerObject server,XS_HostObject host,XS_RequestObject req,XS_ResponseObject response,xvalue* session)
{
    (void)server;(void)host;int64 owner=Relay_Member(response,session);
    if(!owner||!Relay_Method(req,response,XHTTP_METHOD_POST))return;
    if(!Config.enabled){Relay_Reply(response,503,"device relay disabled",NULL);return;}
    xvalue* body=Relay_Body(req);const char* fields[]={"device_id","device_secret","name","platform","app_version","allow_remote","reactivate"};
    const char* id=Relay_Text(body,"device_id",32),*secret=Relay_Text(body,"device_secret",64);
    const char* name=Relay_Text(body,"name",96),*platform=Relay_Text(body,"platform",24),*version=Relay_Text(body,"app_version",48);
    bool enabled=false,reactivate=false;char hash[65]={0};RelayDevice previous={0};
    int status=400;const char* error="invalid device registration";xvalue* data=NULL;sqlite3_stmt* statement=NULL;
    if(!Relay_Fields(body,fields,7)||!Relay_Hex(id,32)||!Relay_Hex(secret,64)||!name||!name[0]||!platform||!platform[0]||!version||
       !xrtValueGetBool(ValueGet(body,"allow_remote"),&enabled)||
       (ValueHas(body,"reactivate")&&!xrtValueGetBool(ValueGet(body,"reactivate"),&reactivate))||!Relay_Hash(secret,hash))goto done;
    int found=Relay_DeviceRead(owner,id,&previous);
    if(found<0){status=500;error="device storage unavailable";goto done;}
    if(found&&(!Relay_HashEqual(hash,previous.secret_hash)||(previous.revoked&&enabled&&!reactivate))){status=403;error="device proof invalid or explicit reactivation required";goto done;}
    if(!found){
        statement=Relay_SQL("SELECT COUNT(*) FROM device WHERE member_id=?");
        if(statement)sqlite3_bind_int64(statement,1,owner);
        bool counted=statement&&sqlite3_step(statement)==SQLITE_ROW;int count=counted?sqlite3_column_int(statement,0):0;
        sqlite3_finalize(statement);statement=NULL;
        if(!counted){status=500;error="device storage unavailable";goto done;}
        if(count>=Config.devices){status=409;error="device registration limit reached";goto done;}
        statement=Relay_SQL("INSERT INTO device(id,member_id,secret_hash,name,platform,app_version,enabled,revoked,created_at,updated_at) VALUES(?,?,?,?,?,?,?,0,?,?)");
        if(statement){sqlite3_bind_int64(statement,2,owner);sqlite3_bind_int(statement,7,enabled);sqlite3_bind_int64(statement,8,Relay_Now());sqlite3_bind_int64(statement,9,Relay_Now());}
        Relay_Bind(statement,1,id);Relay_Bind(statement,3,hash);Relay_Bind(statement,4,name);Relay_Bind(statement,5,platform);Relay_Bind(statement,6,version);
    }else{
        statement=Relay_SQL("UPDATE device SET name=?,platform=?,app_version=?,enabled=?,revoked=CASE WHEN ? THEN 0 ELSE revoked END,updated_at=? WHERE id=? AND member_id=?");
        Relay_Bind(statement,1,name);Relay_Bind(statement,2,platform);Relay_Bind(statement,3,version);Relay_Bind(statement,7,id);
        if(statement){sqlite3_bind_int(statement,4,enabled);sqlite3_bind_int(statement,5,reactivate&&enabled);sqlite3_bind_int64(statement,6,Relay_Now());sqlite3_bind_int64(statement,8,owner);}
    }
    if(!Relay_Done(statement)){statement=NULL;status=sqlite3_errcode(Database)==SQLITE_CONSTRAINT?409:500;error="device registration failed";goto done;}statement=NULL;
    if(!enabled)Relay_Revoke(owner,id);
    data=ValueObject();if(!data||!ValueSetText(data,"device_id",id)||!ValueSetBool(data,"allow_remote",enabled)||
        !ValueSetBool(data,"revoked",found&&previous.revoked&&!(reactivate&&enabled))){xrtValueRelease(data);data=NULL;status=500;error="allocation failed";goto done;}
    status=found?200:201;error="";
done:
    sqlite3_finalize(statement);xrtSecureZero(hash,sizeof(hash));xrtSecureZero(&previous,sizeof(previous));
    if(secret)xrtSecureZero((void*)secret,strlen(secret));xrtValueRelease(body);
    Relay_Reply(response,status,error,data);
}
static void Relay_RemoveOrRevoke(XS_ServerObject server,XS_HostObject host,XS_RequestObject req,XS_ResponseObject response,xvalue* session)
{
    (void)server;(void)host;int64 owner=Relay_Member(response,session);
    if(!owner||!Relay_Method(req,response,XHTTP_METHOD_POST))return;
    xvalue* body=Relay_Body(req);const char* fields[]={"device_id"};const char* id=Relay_Text(body,"device_id",32);RelayDevice device={0};
    int status=400;const char* error="invalid device id";
    if(Relay_Fields(body,fields,1)&&Relay_Hex(id,32)){
        int found=Relay_DeviceRead(owner,id,&device);status=found<0?500:404;error=found<0?"device storage unavailable":"device not found";
        if(found==1){
            bool remove=!strcmp(XAdmin_ReqPath(req),"/api/v1/devices/remove");
            sqlite3_stmt* statement=Relay_SQL(remove?"DELETE FROM device WHERE id=? AND member_id=?":
                "UPDATE device SET enabled=0,revoked=1,updated_at=?3 WHERE id=?1 AND member_id=?2");
            Relay_Bind(statement,1,id);if(statement){sqlite3_bind_int64(statement,2,owner);if(!remove)sqlite3_bind_int64(statement,3,Relay_Now());}
            if(Relay_Done(statement)){Relay_Revoke(owner,id);status=200;error="";}else{status=500;error="device storage unavailable";}
        }
    }
    xrtSecureZero(&device,sizeof(device));xrtValueRelease(body);Relay_Reply(response,status,error,NULL);
}
