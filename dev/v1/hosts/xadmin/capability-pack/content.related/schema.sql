CREATE TABLE IF NOT EXISTS content_related (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	source_content_id INTEGER NOT NULL DEFAULT 0,
	related_content_id INTEGER NOT NULL DEFAULT 0,
	relation_type TEXT NOT NULL DEFAULT 'manual',
	weight INTEGER NOT NULL DEFAULT 0,
	status INTEGER NOT NULL DEFAULT 1,
	create_time INTEGER NOT NULL DEFAULT 0,
	update_time INTEGER NOT NULL DEFAULT 0,
	delete_time INTEGER NOT NULL DEFAULT 0
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_content_related_pair ON content_related(source_content_id,related_content_id,relation_type) WHERE delete_time=0;
CREATE INDEX IF NOT EXISTS idx_content_related_source ON content_related(source_content_id,status,weight,update_time);
CREATE INDEX IF NOT EXISTS idx_content_related_target ON content_related(related_content_id,status);
