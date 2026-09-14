CREATE TABLE IF NOT EXISTS content_category (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	parent_id INTEGER NOT NULL DEFAULT 0,
	title TEXT NOT NULL DEFAULT '',
	slug TEXT NOT NULL DEFAULT '',
	description TEXT NOT NULL DEFAULT '',
	cover_url TEXT NOT NULL DEFAULT '',
	template_key TEXT NOT NULL DEFAULT '',
	path TEXT NOT NULL DEFAULT '',
	level INTEGER NOT NULL DEFAULT 0,
	sort INTEGER NOT NULL DEFAULT 0,
	status INTEGER NOT NULL DEFAULT 1,
	seo_title TEXT NOT NULL DEFAULT '',
	seo_keywords TEXT NOT NULL DEFAULT '',
	seo_description TEXT NOT NULL DEFAULT '',
	create_time INTEGER NOT NULL DEFAULT 0,
	update_time INTEGER NOT NULL DEFAULT 0,
	delete_time INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_content_category_parent_sort ON content_category(parent_id, sort, id);
CREATE UNIQUE INDEX IF NOT EXISTS idx_content_category_parent_slug ON content_category(parent_id, slug) WHERE delete_time = 0;

CREATE TABLE IF NOT EXISTS content_category_bind (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	content_id INTEGER NOT NULL DEFAULT 0,
	category_id INTEGER NOT NULL DEFAULT 0,
	status INTEGER NOT NULL DEFAULT 1,
	create_time INTEGER NOT NULL DEFAULT 0,
	update_time INTEGER NOT NULL DEFAULT 0,
	delete_time INTEGER NOT NULL DEFAULT 0
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_content_category_bind_content ON content_category_bind(content_id) WHERE delete_time = 0;
CREATE INDEX IF NOT EXISTS idx_content_category_bind_category ON content_category_bind(category_id, status, delete_time);
