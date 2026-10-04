static sqlite3_stmt* Relay_SQL(const char* sql)
{sqlite3_stmt* statement=NULL;if(sqlite3_prepare_v2(Database,sql,-1,&statement,NULL)!=SQLITE_OK)return NULL;return statement;}
static void Relay_Bind(sqlite3_stmt* statement,int index,const char* text)
{if(statement)sqlite3_bind_text(statement,index,text,-1,SQLITE_TRANSIENT);}
static bool Relay_Done(sqlite3_stmt* statement)
{bool ok=statement&&sqlite3_step(statement)==SQLITE_DONE;sqlite3_finalize(statement);return ok;}
static bool Relay_Column(sqlite3_stmt* statement,int index,char* out,size_t capacity)
{
    const char* text=(const char*)sqlite3_column_text(statement,index);int size=sqlite3_column_bytes(statement,index);
    if(!text||size<0||(size_t)size>=capacity||memchr(text,0,(size_t)size))return false;
    memcpy(out,text,(size_t)size);out[size]=0;return true;
}
static bool Relay_StorageInit(void)
{
    sqlite3_stmt* statement=Relay_SQL("PRAGMA user_version");
    int version=statement&&sqlite3_step(statement)==SQLITE_ROW?sqlite3_column_int(statement,0):-1;
    sqlite3_finalize(statement);if(version<0||version>1)return false;
    if(version==0&&sqlite3_exec(Database,
        "BEGIN IMMEDIATE;CREATE TABLE IF NOT EXISTS device("
        "id TEXT PRIMARY KEY,member_id INTEGER NOT NULL,secret_hash TEXT NOT NULL,"
        "name TEXT NOT NULL,platform TEXT NOT NULL,app_version TEXT NOT NULL,"
        "enabled INTEGER NOT NULL CHECK(enabled IN(0,1)),revoked INTEGER NOT NULL CHECK(revoked IN(0,1)),"
        "created_at INTEGER NOT NULL,updated_at INTEGER NOT NULL,last_seen INTEGER NOT NULL DEFAULT 0);"
        "CREATE INDEX IF NOT EXISTS device_owner ON device(member_id);PRAGMA user_version=1;COMMIT;",
        NULL,NULL,NULL)!=SQLITE_OK){sqlite3_exec(Database,"ROLLBACK",NULL,NULL,NULL);return false;}
    statement=Relay_SQL("SELECT id,member_id,secret_hash,name,platform,app_version,enabled,revoked,created_at,updated_at,last_seen FROM device LIMIT 0");
    bool ok=statement!=NULL;sqlite3_finalize(statement);return ok;
}
/* -1 storage failure, 0 absent (including another member), 1 owned record. */
static int Relay_DeviceRead(int64 owner,const char* id,RelayDevice* device)
{
    sqlite3_stmt* statement=Relay_SQL("SELECT id,secret_hash,name,platform,app_version,enabled,revoked,created_at,updated_at,last_seen FROM device WHERE id=? AND member_id=?");
    if(!statement)return -1;Relay_Bind(statement,1,id);sqlite3_bind_int64(statement,2,owner);
    int step=sqlite3_step(statement),result=-1;memset(device,0,sizeof(*device));
    if(step==SQLITE_DONE)result=0;
    else if(step==SQLITE_ROW&&Relay_Column(statement,0,device->id,sizeof(device->id))&&
        Relay_Column(statement,1,device->secret_hash,sizeof(device->secret_hash))&&
        Relay_Column(statement,2,device->name,sizeof(device->name))&&
        Relay_Column(statement,3,device->platform,sizeof(device->platform))&&
        Relay_Column(statement,4,device->version,sizeof(device->version))&&Relay_Hex(device->id,32)&&Relay_Hex(device->secret_hash,64)){
        device->owner=owner;device->enabled=sqlite3_column_int(statement,5)!=0;device->revoked=sqlite3_column_int(statement,6)!=0;
        device->created=sqlite3_column_int64(statement,7);device->updated=sqlite3_column_int64(statement,8);device->last_seen=sqlite3_column_int64(statement,9);result=1;
    }sqlite3_finalize(statement);return result;
}
static void Relay_Seen(const RelayConnection* connection)
{
    sqlite3_stmt* statement=Relay_SQL("UPDATE device SET last_seen=? WHERE id=? AND member_id=?");
    if(statement){sqlite3_bind_int64(statement,1,Relay_Now());sqlite3_bind_int64(statement,3,connection->owner);}
    Relay_Bind(statement,2,connection->device);Relay_Done(statement);
}
static bool Relay_DeviceAllowed(RelayConnection* connection)
{
    RelayDevice device;bool ok=Relay_DeviceRead(connection->owner,connection->device,&device)==1&&device.enabled&&!device.revoked;
    xrtSecureZero(&device,sizeof(device));return ok;
}
