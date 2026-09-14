CREATE TABLE IF NOT EXISTS content_import_job (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	source_name TEXT NOT NULL DEFAULT '',
	status TEXT NOT NULL DEFAULT 'preview',
	total_count INTEGER NOT NULL DEFAULT 0,
	success_count INTEGER NOT NULL DEFAULT 0,
	fail_count INTEGER NOT NULL DEFAULT 0,
	report_json TEXT NOT NULL DEFAULT '{}',
	operator_id INTEGER NOT NULL DEFAULT 0,
	create_time INTEGER NOT NULL DEFAULT 0,
	finish_time INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS content_export_job (
	id INTEGER PRIMARY KEY AUTOINCREMENT,
	export_type TEXT NOT NULL DEFAULT 'json',
	status TEXT NOT NULL DEFAULT 'finished',
	total_count INTEGER NOT NULL DEFAULT 0,
	filter_json TEXT NOT NULL DEFAULT '{}',
	result_json TEXT NOT NULL DEFAULT '{}',
	operator_id INTEGER NOT NULL DEFAULT 0,
	create_time INTEGER NOT NULL DEFAULT 0,
	finish_time INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_content_import_job_time ON content_import_job(create_time DESC, id DESC);
CREATE INDEX IF NOT EXISTS idx_content_export_job_time ON content_export_job(create_time DESC, id DESC);
