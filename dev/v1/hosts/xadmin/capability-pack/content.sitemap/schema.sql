CREATE TABLE IF NOT EXISTS content_sitemap_entry (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	content_id INTEGER NOT NULL DEFAULT 0,
	loc TEXT NOT NULL DEFAULT '',
	title TEXT NOT NULL DEFAULT '',
	type TEXT NOT NULL DEFAULT 'content',
	priority REAL NOT NULL DEFAULT 0.8,
	changefreq TEXT NOT NULL DEFAULT 'weekly',
	status INTEGER NOT NULL DEFAULT 1,
	update_time INTEGER NOT NULL DEFAULT 0
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_content_sitemap_entry_content_type ON content_sitemap_entry(content_id,type);
CREATE INDEX IF NOT EXISTS idx_content_sitemap_entry_status_time ON content_sitemap_entry(status,update_time);
