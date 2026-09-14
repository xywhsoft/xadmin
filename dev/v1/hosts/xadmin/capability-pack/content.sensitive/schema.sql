CREATE TABLE IF NOT EXISTS sensitive_word (
id INTEGER PRIMARY KEY AUTOINCREMENT,
word TEXT NOT NULL DEFAULT '',
level INTEGER NOT NULL DEFAULT 1,
scope TEXT NOT NULL DEFAULT 'content',
group_key TEXT NOT NULL DEFAULT 'default',
replacement TEXT NOT NULL DEFAULT '',
status INTEGER NOT NULL DEFAULT 1,
create_time INTEGER NOT NULL DEFAULT 0,
update_time INTEGER NOT NULL DEFAULT 0,
delete_time INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS sensitive_hit_log (
id INTEGER PRIMARY KEY AUTOINCREMENT,
target_type TEXT NOT NULL DEFAULT '',
target_id INTEGER NOT NULL DEFAULT 0,
word_id INTEGER NOT NULL DEFAULT 0,
word TEXT NOT NULL DEFAULT '',
field_name TEXT NOT NULL DEFAULT '',
action TEXT NOT NULL DEFAULT '',
create_time INTEGER NOT NULL DEFAULT 0
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_sensitive_word_word_scope ON sensitive_word(word, scope) WHERE delete_time = 0;
CREATE INDEX IF NOT EXISTS idx_sensitive_hit_target ON sensitive_hit_log(target_type, target_id, create_time DESC);
