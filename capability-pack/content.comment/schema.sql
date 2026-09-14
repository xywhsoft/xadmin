CREATE TABLE IF NOT EXISTS comment_thread (
id INTEGER PRIMARY KEY AUTOINCREMENT,
content_id INTEGER NOT NULL DEFAULT 0,
comment_count INTEGER NOT NULL DEFAULT 0,
visible_count INTEGER NOT NULL DEFAULT 0,
status INTEGER NOT NULL DEFAULT 1,
create_time INTEGER NOT NULL DEFAULT 0,
update_time INTEGER NOT NULL DEFAULT 0,
UNIQUE(content_id)
);
CREATE TABLE IF NOT EXISTS comment_item (
id INTEGER PRIMARY KEY AUTOINCREMENT,
content_id INTEGER NOT NULL DEFAULT 0,
thread_id INTEGER NOT NULL DEFAULT 0,
parent_id INTEGER NOT NULL DEFAULT 0,
author_name TEXT NOT NULL DEFAULT '',
author_id TEXT NOT NULL DEFAULT '',
body TEXT NOT NULL DEFAULT '',
ip TEXT NOT NULL DEFAULT '',
user_agent TEXT NOT NULL DEFAULT '',
status INTEGER NOT NULL DEFAULT 0,
create_time INTEGER NOT NULL DEFAULT 0,
update_time INTEGER NOT NULL DEFAULT 0,
delete_time INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS comment_audit_log (
id INTEGER PRIMARY KEY AUTOINCREMENT,
comment_id INTEGER NOT NULL DEFAULT 0,
action TEXT NOT NULL DEFAULT '',
operator_id TEXT NOT NULL DEFAULT '',
note TEXT NOT NULL DEFAULT '',
create_time INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS comment_notification (
id INTEGER PRIMARY KEY AUTOINCREMENT,
comment_id INTEGER NOT NULL DEFAULT 0,
content_id INTEGER NOT NULL DEFAULT 0,
event TEXT NOT NULL DEFAULT '',
target_type TEXT NOT NULL DEFAULT '',
target_id TEXT NOT NULL DEFAULT '',
title TEXT NOT NULL DEFAULT '',
body TEXT NOT NULL DEFAULT '',
status INTEGER NOT NULL DEFAULT 0,
create_time INTEGER NOT NULL DEFAULT 0,
read_time INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_comment_item_content ON comment_item(content_id, status, create_time DESC);
CREATE INDEX IF NOT EXISTS idx_comment_item_audit ON comment_item(status, create_time DESC);
CREATE INDEX IF NOT EXISTS idx_comment_audit_log_comment ON comment_audit_log(comment_id, create_time DESC);
CREATE INDEX IF NOT EXISTS idx_comment_notification_status ON comment_notification(status, create_time DESC);
