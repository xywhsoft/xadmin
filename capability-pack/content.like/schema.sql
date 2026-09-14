CREATE TABLE IF NOT EXISTS like_record (
id INTEGER PRIMARY KEY AUTOINCREMENT,
content_id INTEGER NOT NULL DEFAULT 0,
actor_id TEXT NOT NULL DEFAULT '',
actor_key TEXT NOT NULL DEFAULT '',
ip TEXT NOT NULL DEFAULT '',
status INTEGER NOT NULL DEFAULT 1,
create_time INTEGER NOT NULL DEFAULT 0,
update_time INTEGER NOT NULL DEFAULT 0,
UNIQUE(content_id, actor_key)
);
CREATE TABLE IF NOT EXISTS like_counter (
id INTEGER PRIMARY KEY AUTOINCREMENT,
content_id INTEGER NOT NULL DEFAULT 0,
like_count INTEGER NOT NULL DEFAULT 0,
update_time INTEGER NOT NULL DEFAULT 0,
UNIQUE(content_id)
);
CREATE INDEX IF NOT EXISTS idx_like_record_content ON like_record(content_id, status, create_time DESC);
CREATE INDEX IF NOT EXISTS idx_like_record_actor ON like_record(actor_key, content_id);
