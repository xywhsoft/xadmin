CREATE TABLE IF NOT EXISTS content_audit_log (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	target_type TEXT NOT NULL DEFAULT 'content',
	target_id INTEGER NOT NULL DEFAULT 0,
	action TEXT NOT NULL DEFAULT '',
	summary TEXT NOT NULL DEFAULT '',
	detail_json TEXT NOT NULL DEFAULT '{}',
	operator_type TEXT NOT NULL DEFAULT '',
	operator_id INTEGER NOT NULL DEFAULT 0,
	ip TEXT NOT NULL DEFAULT '',
	create_time INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_content_audit_log_target ON content_audit_log(target_type, target_id, create_time DESC);
CREATE INDEX IF NOT EXISTS idx_content_audit_log_action ON content_audit_log(action, create_time DESC);
