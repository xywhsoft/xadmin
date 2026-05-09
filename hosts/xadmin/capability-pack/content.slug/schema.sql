CREATE TABLE IF NOT EXISTS content_slug_history (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	content_id INTEGER NOT NULL DEFAULT 0,
	old_slug TEXT NOT NULL DEFAULT '',
	new_slug TEXT NOT NULL DEFAULT '',
	status INTEGER NOT NULL DEFAULT 1,
	create_time INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_content_slug_history_old ON content_slug_history(old_slug, status, create_time DESC);
CREATE INDEX IF NOT EXISTS idx_content_slug_history_content ON content_slug_history(content_id, create_time DESC);
