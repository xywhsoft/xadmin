CREATE TABLE IF NOT EXISTS content_media (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	attachment_xid TEXT NOT NULL DEFAULT '',
	title TEXT NOT NULL DEFAULT '',
	url TEXT NOT NULL DEFAULT '',
	mime TEXT NOT NULL DEFAULT '',
	ext TEXT NOT NULL DEFAULT '',
	size INTEGER NOT NULL DEFAULT 0,
	width INTEGER NOT NULL DEFAULT 0,
	height INTEGER NOT NULL DEFAULT 0,
	status INTEGER NOT NULL DEFAULT 1,
	create_time INTEGER NOT NULL DEFAULT 0,
	update_time INTEGER NOT NULL DEFAULT 0,
	delete_time INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_content_media_status ON content_media(status, delete_time, id);

CREATE TABLE IF NOT EXISTS content_media_ref (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	media_id INTEGER NOT NULL DEFAULT 0,
	content_id INTEGER NOT NULL DEFAULT 0,
	ref_type TEXT NOT NULL DEFAULT '',
	create_time INTEGER NOT NULL DEFAULT 0
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_content_media_ref_unique ON content_media_ref(media_id, content_id, ref_type);
CREATE INDEX IF NOT EXISTS idx_content_media_ref_media ON content_media_ref(media_id);
CREATE INDEX IF NOT EXISTS idx_content_media_ref_content ON content_media_ref(content_id);
