/* Production source, disposable in-memory databases, no test routes in main.c. */
#include <xsbase.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include "../src/identity/identifier.c"
#include "../src/storage/identity_migration.c"
static int failures, assertions;
#define CHECK(expr) do { assertions++; if (!(expr)) { failures++; printf("FAIL identity:%d: %s\n", __LINE__, #expr); } } while (0)
static sqlite3* Fresh(void)
{
    sqlite3* db = NULL;
    CHECK(sqlite3_open(":memory:", &db) == SQLITE_OK);
    CHECK(sqlite3_exec(db, "CREATE TABLE member(id INTEGER PRIMARY KEY,username TEXT UNIQUE,phone TEXT,email TEXT);PRAGMA user_version=2;", NULL,NULL,NULL) == SQLITE_OK);
    return db;
}
static int Scalar(sqlite3* db, const char* sql)
{
    sqlite3_stmt* s = NULL; int n = -1;
    CHECK(sqlite3_prepare_v2(db,sql,-1,&s,NULL)==SQLITE_OK);
    if (sqlite3_step(s)==SQLITE_ROW) n=sqlite3_column_int(s,0);
    sqlite3_finalize(s); return n;
}
void ServiceInit(XS_HostInfo* host)
{
    XAIdentifier id; char key[65]; sqlite3* db; (void)host;
    CHECK(XA_AccountKey("Alice_1",7,key) && !strcmp(key,"alice_1"));
    CHECK(!XA_AccountKey("13800138000",11,key));
    CHECK(!XA_AccountKey("alice@example.com",17,key));
    CHECK(!XA_AccountKey("ab",2,key));
    CHECK(!XA_AccountKey("a\0bc",4,key));
    CHECK(!XA_AccountKey("a name",6,key));
    CHECK(XA_IdentifierParse("Alice+tag@EXAMPLE.COM",21,NULL,&id) && id.kind==XA_IDENTIFIER_EMAIL && !strcmp(id.key,"Alice+tag@example.com"));
    CHECK(XA_IdentifierParse("alice@example.com",17,NULL,&id) && strcmp(id.key,"Alice@example.com"));
    CHECK(!XA_IdentifierParse("a..b@example.com",16,NULL,&id));
    CHECK(!XA_IdentifierParse("a@-example.com",14,NULL,&id));
    CHECK(!XA_IdentifierParse("a@localhost",11,NULL,&id));
    CHECK(!XA_IdentifierParse("a@x.com\0evil",12,NULL,&id));
    CHECK(XA_IdentifierParse("+86 138-0013-8000",17,NULL,&id) && !strcmp(id.key,"+8613800138000"));
    CHECK(XA_IdentifierParse("008613800138000",15,NULL,&id) && !strcmp(id.key,"+8613800138000"));
    CHECK(XA_IdentifierParse("13800138000",11,"+86",&id) && !strcmp(id.key,"+8613800138000"));
    CHECK(!XA_IdentifierParse("13800138000",11,NULL,&id));
    CHECK(!XA_IdentifierParse("02012345678",11,"+86",&id));
    CHECK(!XA_IdentifierParse("+0123456789",11,NULL,&id));
    CHECK(!XA_IdentifierParse("+1234567890123456",17,NULL,&id));
    db=Fresh();
    CHECK(sqlite3_exec(db,"INSERT INTO member VALUES(1,'Alice','old phone','old@example.com');",NULL,NULL,NULL)==SQLITE_OK);
    CHECK(XA_IdentityMigrate(db,NULL));
    CHECK(XA_IdentityMigrate(db,NULL));
    CHECK(Scalar(db,"PRAGMA user_version")==2);
    CHECK(Scalar(db,"SELECT count(*) FROM member WHERE phone='old phone' AND email='old@example.com' AND phone_verified_at=0 AND email_verified_at=0 AND phone_key IS NULL AND email_key IS NULL")==1);
    CHECK(sqlite3_exec(db,"INSERT INTO member(username) VALUES('ALICE');",NULL,NULL,NULL)!=SQLITE_OK);
    CHECK(sqlite3_exec(db,"INSERT INTO member(username) VALUES('13800138000');",NULL,NULL,NULL)!=SQLITE_OK);
    CHECK(sqlite3_exec(db,"INSERT INTO member(username) VALUES('a@x.com');",NULL,NULL,NULL)!=SQLITE_OK);
    CHECK(sqlite3_exec(db,"INSERT INTO member(username) VALUES('valid'||char(0)||'hidden');",NULL,NULL,NULL)!=SQLITE_OK);
    CHECK(sqlite3_exec(db,"INSERT INTO member(id,username) VALUES(2,NULL),(3,NULL);",NULL,NULL,NULL)==SQLITE_OK);
    CHECK(sqlite3_exec(db,"UPDATE member SET phone_key='+8613800138000' WHERE id=1;",NULL,NULL,NULL)!=SQLITE_OK);
    CHECK(sqlite3_exec(db,"UPDATE member SET phone='+8613800138000',phone_key='+8613800138000',phone_verified_at=1 WHERE id=1;",NULL,NULL,NULL)==SQLITE_OK);
    CHECK(sqlite3_exec(db,"UPDATE member SET phone='+8613800138000',phone_key='+8613800138000',phone_verified_at=1 WHERE id=2;",NULL,NULL,NULL)!=SQLITE_OK);
    CHECK(sqlite3_exec(db,"UPDATE member SET phone='bypass' WHERE id=1;",NULL,NULL,NULL)!=SQLITE_OK);
    CHECK(sqlite3_exec(db,"INSERT INTO member_external_identity(member_id,provider,app_namespace,subject,created_at) VALUES(1,'github','app','123',1);",NULL,NULL,NULL)==SQLITE_OK);
    CHECK(sqlite3_exec(db,"INSERT INTO member_external_identity(member_id,provider,app_namespace,subject,created_at) VALUES(2,'github','app','123',1);",NULL,NULL,NULL)!=SQLITE_OK);
    CHECK(sqlite3_exec(db,"UPDATE xadmin_migration SET checksum='tampered';",NULL,NULL,NULL)==SQLITE_OK);
    CHECK(!XA_IdentityMigrate(db,NULL)); sqlite3_close(db);
    db=Fresh();
    CHECK(sqlite3_exec(db,"INSERT INTO member(username) VALUES('Alice'),('alice');",NULL,NULL,NULL)==SQLITE_OK);
    CHECK(!XA_IdentityMigrate(db,NULL));
    CHECK(Scalar(db,"SELECT count(*) FROM sqlite_master WHERE name='xadmin_migration'")==0);
    CHECK(Scalar(db,"SELECT count(*) FROM member")==2); sqlite3_close(db);
    db=Fresh();
    CHECK(sqlite3_exec(db,"INSERT INTO member(username) VALUES('13800138000');",NULL,NULL,NULL)==SQLITE_OK);
    CHECK(!XA_IdentityMigrate(db,NULL));
    CHECK(Scalar(db,"SELECT count(*) FROM pragma_table_info('member')")==4); sqlite3_close(db);
    db=Fresh();
    CHECK(sqlite3_exec(db,"CREATE TABLE member_external_identity(dummy);",NULL,NULL,NULL)==SQLITE_OK);
    CHECK(!XA_IdentityMigrate(db,NULL));
    CHECK(Scalar(db,"SELECT count(*) FROM pragma_table_info('member')")==4);
    CHECK(Scalar(db,"SELECT count(*) FROM sqlite_master WHERE name='xadmin_migration'")==0); sqlite3_close(db);
    printf("IDENTITY UNIT: %d assertions, %d failures\n",assertions,failures);
}
XS_RequestResult RequestProc(XS_HttpReq* req)
{
    char body[100],head[200];size_t n;
    snprintf(body,sizeof(body),"%s %d assertions",failures?"FAIL":"PASS",assertions);
    n=(size_t)snprintf(head,sizeof(head),"HTTP/1.1 %d OK\r\nContent-Length: %u\r\nConnection: close\r\n\r\n",failures?500:200,(unsigned)strlen(body));
    xrtNetStreamSend(req->tcp,head,n);xrtNetStreamSend(req->tcp,body,strlen(body));return XS_OK;
}
