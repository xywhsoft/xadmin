CREATE TABLE IF NOT EXISTS content_form (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	content_id INTEGER NOT NULL DEFAULT 0,
	form_key TEXT NOT NULL DEFAULT '',
	title TEXT NOT NULL DEFAULT '',
	schema_json TEXT NOT NULL DEFAULT '{}',
	status INTEGER NOT NULL DEFAULT 1,
	create_time INTEGER NOT NULL DEFAULT 0,
	update_time INTEGER NOT NULL DEFAULT 0,
	delete_time INTEGER NOT NULL DEFAULT 0
);

CREATE UNIQUE INDEX IF NOT EXISTS idx_content_form_key ON content_form(form_key) WHERE delete_time=0;
CREATE INDEX IF NOT EXISTS idx_content_form_content ON content_form(content_id,status,delete_time);

CREATE TABLE IF NOT EXISTS content_form_submission (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	form_id INTEGER NOT NULL DEFAULT 0,
	content_id INTEGER NOT NULL DEFAULT 0,
	data_json TEXT NOT NULL DEFAULT '{}',
	ip TEXT NOT NULL DEFAULT '',
	status INTEGER NOT NULL DEFAULT 1,
	process_note TEXT NOT NULL DEFAULT '',
	processed_by TEXT NOT NULL DEFAULT '',
	process_time INTEGER NOT NULL DEFAULT 0,
	create_time INTEGER NOT NULL DEFAULT 0,
	delete_time INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_content_form_submission_form ON content_form_submission(form_id,create_time DESC);
CREATE INDEX IF NOT EXISTS idx_content_form_submission_content ON content_form_submission(content_id,create_time DESC);

CREATE TABLE IF NOT EXISTS content_form_notification (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	submission_id INTEGER NOT NULL DEFAULT 0,
	form_id INTEGER NOT NULL DEFAULT 0,
	content_id INTEGER NOT NULL DEFAULT 0,
	event TEXT NOT NULL DEFAULT '',
	title TEXT NOT NULL DEFAULT '',
	body TEXT NOT NULL DEFAULT '',
	status INTEGER NOT NULL DEFAULT 0,
	create_time INTEGER NOT NULL DEFAULT 0,
	read_time INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_content_form_notification_submission ON content_form_notification(submission_id, id DESC);
CREATE INDEX IF NOT EXISTS idx_content_form_notification_status ON content_form_notification(status, create_time DESC);
