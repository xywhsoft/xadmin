CREATE TABLE IF NOT EXISTS content_search_index (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  content_id INTEGER NOT NULL DEFAULT 0,
  title TEXT NOT NULL DEFAULT '',
  summary TEXT NOT NULL DEFAULT '',
  body TEXT NOT NULL DEFAULT '',
  keywords TEXT NOT NULL DEFAULT '',
  status INTEGER NOT NULL DEFAULT 1,
  update_time INTEGER NOT NULL DEFAULT 0
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_content_search_index_content ON content_search_index(content_id);
CREATE INDEX IF NOT EXISTS idx_content_search_index_status ON content_search_index(status, update_time DESC);
