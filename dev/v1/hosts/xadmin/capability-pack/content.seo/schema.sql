CREATE TABLE IF NOT EXISTS content_seo_meta (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	content_id INTEGER NOT NULL DEFAULT 0,
	seo_title TEXT NOT NULL DEFAULT '',
	seo_keywords TEXT NOT NULL DEFAULT '',
	seo_description TEXT NOT NULL DEFAULT '',
	canonical TEXT NOT NULL DEFAULT '',
	status INTEGER NOT NULL DEFAULT 1,
	create_time INTEGER NOT NULL DEFAULT 0,
	update_time INTEGER NOT NULL DEFAULT 0,
	delete_time INTEGER NOT NULL DEFAULT 0
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_content_seo_meta_content ON content_seo_meta(content_id);
