CREATE TABLE IF NOT EXISTS content_access_rule (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	content_id INTEGER NOT NULL DEFAULT 0,
	access_mode TEXT NOT NULL DEFAULT 'public',
	required_read_level INTEGER NOT NULL DEFAULT 0,
	member_group_ids TEXT NOT NULL DEFAULT '',
	password_hash TEXT NOT NULL DEFAULT '',
	price INTEGER NOT NULL DEFAULT 0,
	status INTEGER NOT NULL DEFAULT 1,
	create_time INTEGER NOT NULL DEFAULT 0,
	update_time INTEGER NOT NULL DEFAULT 0,
	delete_time INTEGER NOT NULL DEFAULT 0
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_content_access_rule_content ON content_access_rule(content_id) WHERE delete_time=0;
CREATE INDEX IF NOT EXISTS idx_content_access_rule_status ON content_access_rule(status,delete_time);
