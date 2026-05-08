CREATE TABLE IF NOT EXISTS content_redirect (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	source_path TEXT NOT NULL DEFAULT '',
	target_url TEXT NOT NULL DEFAULT '',
	status_code INTEGER NOT NULL DEFAULT 301,
	hit_count INTEGER NOT NULL DEFAULT 0,
	last_hit_time INTEGER NOT NULL DEFAULT 0,
	status INTEGER NOT NULL DEFAULT 1,
	create_time INTEGER NOT NULL DEFAULT 0,
	update_time INTEGER NOT NULL DEFAULT 0,
	delete_time INTEGER NOT NULL DEFAULT 0
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_content_redirect_source ON content_redirect(source_path) WHERE delete_time = 0;
CREATE INDEX IF NOT EXISTS idx_content_redirect_status_source ON content_redirect(status, source_path, delete_time);
