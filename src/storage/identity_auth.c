static const char XA_AuthSchema1[] =
    "CREATE TABLE identity_key(id TEXT PRIMARY KEY,secret TEXT NOT NULL,active INTEGER NOT NULL CHECK(active IN (0,1)),created_at INTEGER NOT NULL);"
    "CREATE UNIQUE INDEX identity_key_active ON identity_key(active) WHERE active=1;"
    "CREATE TABLE member_session(sid TEXT PRIMARY KEY,member_id INTEGER NOT NULL REFERENCES member(id),cookie_hash TEXT NOT NULL UNIQUE,csrf_hash TEXT NOT NULL,created_at INTEGER NOT NULL,last_used INTEGER NOT NULL,expires_at INTEGER NOT NULL,revoked_at INTEGER NOT NULL DEFAULT 0,reauth_until INTEGER NOT NULL DEFAULT 0,ip TEXT NOT NULL,user_agent TEXT NOT NULL);"
    "CREATE INDEX member_session_owner ON member_session(member_id,revoked_at,expires_at);"
    "CREATE TABLE member_refresh(hash TEXT PRIMARY KEY,sid TEXT NOT NULL REFERENCES member_session(sid),used_at INTEGER NOT NULL DEFAULT 0);"
    "CREATE INDEX member_refresh_session ON member_refresh(sid);";

/* Checksummed one-version components. Later schema changes append a new
 * component/version; changing installed SQL must never be silently accepted. */
static bool XA_MigrateComponent(const char* component,const char* sql)
{
    char hash[65],backup_name[100];sqlite3_stmt* s;int rc;
    if(!XA_Hash(sql,hash))return false;
    s=XA_SQL("SELECT version,checksum FROM xadmin_migration WHERE component=? ORDER BY version DESC LIMIT 1");
    if(!s)return false;XA_BindText(s,1,component);rc=sqlite3_step(s);
    if(rc==SQLITE_ROW){char stored[65];bool ok=sqlite3_column_int(s,0)==1&&XA_CopyColumn(s,1,stored,sizeof(stored))&&!strcmp(stored,hash);sqlite3_finalize(s);return ok;}
    sqlite3_finalize(s);if(rc!=SQLITE_DONE)return false;
    snprintf(backup_name,sizeof(backup_name),"identity-before-%s-v1.db",component);
    char* path=xrtPathJoin(DBPath,backup_name);bool ok=path&&XA_IdentityBackup(G_DB,path);xrtFree(path);
    if(!ok||!XA_Begin())return false;
    ok=sqlite3_exec(G_DB,sql,NULL,NULL,NULL)==SQLITE_OK;
    if(ok){s=XA_SQL("INSERT INTO xadmin_migration(component,version,checksum,applied_at)VALUES(?,1,?,?)");
        XA_BindText(s,1,component);XA_BindText(s,2,hash);if(s)sqlite3_bind_int64(s,3,xrtNow());ok=XA_Done(s,true);}
    return XA_End(ok);
}
static bool XA_KeyInit(void)
{
    sqlite3_stmt* s=XA_SQL("SELECT count(*) FROM identity_key WHERE active=1");int rc;int count=0;
    if(!s)return false;rc=sqlite3_step(s);if(rc==SQLITE_ROW)count=sqlite3_column_int(s,0);sqlite3_finalize(s);
    if(rc!=SQLITE_ROW)return false;if(count==1)return true;
    char id[65],secret[65];if(!XA_Random(id)||!XA_Random(secret))return false;
    s=XA_SQL("INSERT INTO identity_key(id,secret,active,created_at)VALUES(?,?,1,?)");
    XA_BindText(s,1,id);XA_BindText(s,2,secret);if(s)sqlite3_bind_int64(s,3,XA_Now());
    bool ok=XA_Done(s,true);xrtSecureZero(secret,sizeof(secret));return ok;
}
typedef struct XAAccount {
    int64 id,group,level,balance;int status;
    char username[65],nickname[257],salt[257],password[257];
} XAAccount;
static bool XA_AccountRead(sqlite3_stmt* s,XAAccount* a)
{
    memset(a,0,sizeof(*a));if(!s||sqlite3_step(s)!=SQLITE_ROW)return false;
    a->id=sqlite3_column_int64(s,0);a->group=sqlite3_column_int64(s,4);
    a->level=sqlite3_column_int64(s,5);a->balance=sqlite3_column_int64(s,6);a->status=sqlite3_column_int(s,8);
    return XA_CopyColumn(s,1,a->username,sizeof(a->username))&&XA_CopyColumn(s,2,a->salt,sizeof(a->salt))&&
        XA_CopyColumn(s,3,a->password,sizeof(a->password))&&XA_CopyColumn(s,7,a->nickname,sizeof(a->nickname));
}
static bool XA_AccountByID(int64 id,XAAccount* a)
{
    sqlite3_stmt* s=XA_SQL("SELECT id,username,salt,pwd,groupId,authLevel,balance,nickname,status FROM member WHERE id=? AND isDelete=0");
    if(s)sqlite3_bind_int64(s,1,id);bool ok=XA_AccountRead(s,a);sqlite3_finalize(s);return ok;
}
static bool XA_AccountByIdentifier(const XAIdentifier* identifier,XAAccount* a)
{
    const char* sql=identifier->kind==XA_IDENTIFIER_ACCOUNT?
        "SELECT id,username,salt,pwd,groupId,authLevel,balance,nickname,status FROM member WHERE lower(username)=? AND isDelete=0":
        identifier->kind==XA_IDENTIFIER_PHONE?
        "SELECT id,username,salt,pwd,groupId,authLevel,balance,nickname,status FROM member WHERE phone_key=? AND phone_verified_at>0 AND isDelete=0":
        "SELECT id,username,salt,pwd,groupId,authLevel,balance,nickname,status FROM member WHERE email_key=? AND email_verified_at>0 AND isDelete=0";
    sqlite3_stmt* s=XA_SQL(sql);XA_BindText(s,1,identifier->key);
    bool ok=XA_AccountRead(s,a);sqlite3_finalize(s);return ok;
}
