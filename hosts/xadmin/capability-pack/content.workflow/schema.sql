CREATE TABLE IF NOT EXISTS content_workflow_log (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  content_id INTEGER NOT NULL DEFAULT 0,
  action TEXT NOT NULL DEFAULT '',
  from_status INTEGER NOT NULL DEFAULT 0,
  to_status INTEGER NOT NULL DEFAULT 0,
  from_draft INTEGER NOT NULL DEFAULT 0,
  to_draft INTEGER NOT NULL DEFAULT 0,
  reason TEXT NOT NULL DEFAULT '',
  operator_id INTEGER NOT NULL DEFAULT 0,
  create_time INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_content_workflow_log_content ON content_workflow_log(content_id, id DESC);
CREATE INDEX IF NOT EXISTS idx_content_workflow_log_action ON content_workflow_log(action, create_time DESC);
