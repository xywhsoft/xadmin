CREATE TABLE IF NOT EXISTS content_revision (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  content_id INTEGER NOT NULL DEFAULT 0,
  revision_no INTEGER NOT NULL DEFAULT 0,
  title TEXT NOT NULL DEFAULT '',
  status INTEGER NOT NULL DEFAULT 0,
  category_id INTEGER NOT NULL DEFAULT 0,
  is_draft INTEGER NOT NULL DEFAULT 0,
  payload_json TEXT NOT NULL DEFAULT '{}',
  action TEXT NOT NULL DEFAULT '',
  operator_id INTEGER NOT NULL DEFAULT 0,
  create_time INTEGER NOT NULL DEFAULT 0
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_content_revision_unique ON content_revision(content_id, revision_no);
CREATE INDEX IF NOT EXISTS idx_content_revision_content ON content_revision(content_id, id DESC);
