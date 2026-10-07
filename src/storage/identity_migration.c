/* Single-TU application composition. SQL is compiled into the host: packaged
 * VFS and source deployments use identical bytes/checksums. Version registration
 * is separate from the historical PRAGMA user_version (time-unit migration).
 * All member identifiers, including soft-deleted ones, remain reserved. */
static const char XA_IdentitySchema1[] =
    "ALTER TABLE member ADD COLUMN phone_key TEXT;"
    "ALTER TABLE member ADD COLUMN email_key TEXT;"
    "ALTER TABLE member ADD COLUMN phone_verified_at INTEGER NOT NULL DEFAULT 0 CHECK(phone_verified_at>=0 AND ((phone_verified_at=0 AND phone_key IS NULL) OR (phone_verified_at>0 AND phone_key IS NOT NULL AND phone=phone_key)));"
    "ALTER TABLE member ADD COLUMN email_verified_at INTEGER NOT NULL DEFAULT 0 CHECK(email_verified_at>=0 AND ((email_verified_at=0 AND email_key IS NULL) OR (email_verified_at>0 AND email_key IS NOT NULL AND email=email_key)));"
    "CREATE UNIQUE INDEX member_account_key ON member(lower(username)) WHERE username IS NOT NULL;"
    "CREATE UNIQUE INDEX member_phone_key ON member(phone_key) WHERE phone_key IS NOT NULL;"
    "CREATE UNIQUE INDEX member_email_key ON member(email_key) WHERE email_key IS NOT NULL;"
    "CREATE TRIGGER member_account_insert BEFORE INSERT ON member WHEN NEW.username IS NOT NULL AND (length(CAST(NEW.username AS BLOB))<>length(NEW.username) OR length(NEW.username)<3 OR length(NEW.username)>64 OR NEW.username GLOB '*[^a-zA-Z0-9_.-]*' OR NEW.username NOT GLOB '*[a-zA-Z]*') BEGIN SELECT RAISE(ABORT,'invalid account name'); END;"
    "CREATE TRIGGER member_account_update BEFORE UPDATE OF username ON member WHEN NEW.username IS NOT NULL AND (length(CAST(NEW.username AS BLOB))<>length(NEW.username) OR length(NEW.username)<3 OR length(NEW.username)>64 OR NEW.username GLOB '*[^a-zA-Z0-9_.-]*' OR NEW.username NOT GLOB '*[a-zA-Z]*') BEGIN SELECT RAISE(ABORT,'invalid account name'); END;"
    "CREATE TABLE member_external_identity(id INTEGER PRIMARY KEY AUTOINCREMENT,member_id INTEGER NOT NULL REFERENCES member(id),provider TEXT NOT NULL CHECK(provider IN ('github','wechat')),app_namespace TEXT NOT NULL,subject TEXT NOT NULL,union_scope TEXT,union_id TEXT,created_at INTEGER NOT NULL,UNIQUE(provider,app_namespace,subject));"
    "CREATE UNIQUE INDEX member_external_union ON member_external_identity(provider,union_scope,union_id) WHERE union_scope IS NOT NULL AND union_id IS NOT NULL;";

static bool XA_IdentityPreflight(sqlite3* db)
{
    sqlite3_stmt* stmt = NULL; int rc; bool ok = true;
    if (sqlite3_prepare_v2(db, "SELECT id,username FROM member WHERE username IS NOT NULL", -1, &stmt, NULL) != SQLITE_OK) return false;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        char key[65]; const char* name = (const char*)sqlite3_column_text(stmt, 1);
        if (!XA_AccountKey(name, (size_t)sqlite3_column_bytes(stmt, 1), key)) {
            /* Only IDs enter diagnostics: identifiers can contain private data. */
            printf("[xadmin][identity] invalid historical account: member id=%lld; explicit repair required\n", (long long)sqlite3_column_int64(stmt, 0));
            ok = false;
        }
    }
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE || !ok) return false;
    if (sqlite3_prepare_v2(db, "SELECT min(id),max(id) FROM member WHERE username IS NOT NULL GROUP BY lower(username) HAVING count(*)>1", -1, &stmt, NULL) != SQLITE_OK) return false;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        printf("[xadmin][identity] conflicting account IDs=%lld,%lld; explicit repair required\n", (long long)sqlite3_column_int64(stmt, 0), (long long)sqlite3_column_int64(stmt, 1));
        ok = false;
    }
    sqlite3_finalize(stmt); return ok && rc == SQLITE_DONE;
}
static bool XA_IdentityBackup(sqlite3* db, const char* path)
{
    sqlite3* copy = NULL; sqlite3_backup* backup = NULL; int rc;
    /* Never overwrite an earlier snapshot; an interrupted/failed backup must
     * be inspected explicitly. SQLite backup includes committed WAL contents. */
    if (xrtFileExists(path)) { printf("[xadmin][identity] backup already exists; inspect/remove it before retrying migration\n"); return false; }
    rc = sqlite3_open_v2(path, &copy, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
    if (rc == SQLITE_OK) backup = sqlite3_backup_init(copy, "main", db, "main");
    if (backup) { rc = sqlite3_backup_step(backup, -1); if (sqlite3_backup_finish(backup) != SQLITE_OK) rc = SQLITE_ERROR; }
    else rc = SQLITE_ERROR;
    if (copy && sqlite3_close(copy) != SQLITE_OK) rc = SQLITE_ERROR;
    return rc == SQLITE_DONE;
}
static bool XA_IdentityMigrate(sqlite3* db, const char* backup_path)
{
    sqlite3_stmt* stmt = NULL; unsigned char digest[32]; char checksum[65];
    int i, rc, version = 0; bool registered = false;
    if (!xrtSha256(XA_IdentitySchema1, strlen(XA_IdentitySchema1), digest)) return false;
    for (i = 0; i < 32; i++) snprintf(checksum + i * 2, 3, "%02x", digest[i]);
    rc = sqlite3_prepare_v2(db, "SELECT version,checksum FROM xadmin_migration WHERE component='identity' ORDER BY version DESC LIMIT 1", -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            const char* stored = (const char*)sqlite3_column_text(stmt, 1);
            version = sqlite3_column_int(stmt, 0);
            registered = version == 1 && stored && !strcmp(stored, checksum);
        }
        sqlite3_finalize(stmt);
        if (rc != SQLITE_ROW && rc != SQLITE_DONE) return false;
        if (version) {
            if (!registered) printf("[xadmin][identity] unknown migration version or checksum mismatch; refusing schema downgrade\n");
            return registered;
        }
    } else {
        /* A missing registry is valid; other SQL failures are not. */
        if (sqlite3_prepare_v2(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name='xadmin_migration'", -1, &stmt, NULL) != SQLITE_OK) return false;
        rc = sqlite3_step(stmt); sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) return false;
    }
    if (!XA_IdentityPreflight(db) || (backup_path && !XA_IdentityBackup(db, backup_path))) return false;
    if (sqlite3_exec(db, "BEGIN IMMEDIATE", NULL, NULL, NULL) != SQLITE_OK) return false;
    /* Recheck under the database write transaction; another connection may
     * have committed between preflight/backup and acquisition of the write lock. */
    if (!XA_IdentityPreflight(db)) { sqlite3_exec(db,"ROLLBACK",NULL,NULL,NULL); return false; }
    rc = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS xadmin_migration(component TEXT NOT NULL,version INTEGER NOT NULL,checksum TEXT NOT NULL,applied_at INTEGER NOT NULL,PRIMARY KEY(component,version));", NULL, NULL, NULL);
    if (rc == SQLITE_OK) rc = sqlite3_exec(db, XA_IdentitySchema1, NULL, NULL, NULL);
    if (rc == SQLITE_OK) rc = sqlite3_prepare_v2(db, "INSERT INTO xadmin_migration VALUES('identity',1,?,?)", -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, checksum, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, XAdmin_UnixNowUs());
        rc = sqlite3_step(stmt); sqlite3_finalize(stmt);
        if (rc == SQLITE_DONE) rc = sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    }
    if (rc != SQLITE_OK) {
        printf("[xadmin][identity] schema migration failed (%d); rolling back\n", rc);
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL); return false;
    }
    return true;
}
