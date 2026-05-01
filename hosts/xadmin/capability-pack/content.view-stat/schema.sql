CREATE TABLE IF NOT EXISTS view_counter (
id INTEGER PRIMARY KEY AUTOINCREMENT,
content_id INTEGER NOT NULL DEFAULT 0,
view_count INTEGER NOT NULL DEFAULT 0,
unique_view_count INTEGER NOT NULL DEFAULT 0,
last_view_time INTEGER NOT NULL DEFAULT 0,
UNIQUE(content_id)
);
CREATE TABLE IF NOT EXISTS view_log (
id INTEGER PRIMARY KEY AUTOINCREMENT,
content_id INTEGER NOT NULL DEFAULT 0,
visitor_key TEXT NOT NULL DEFAULT '',
ip TEXT NOT NULL DEFAULT '',
referer TEXT NOT NULL DEFAULT '',
user_agent TEXT NOT NULL DEFAULT '',
create_time INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS view_daily_stat (
id INTEGER PRIMARY KEY AUTOINCREMENT,
content_id INTEGER NOT NULL DEFAULT 0,
stat_date TEXT NOT NULL DEFAULT '',
view_count INTEGER NOT NULL DEFAULT 0,
unique_view_count INTEGER NOT NULL DEFAULT 0,
UNIQUE(content_id, stat_date)
);
CREATE INDEX IF NOT EXISTS idx_view_log_content_time ON view_log(content_id, create_time DESC);
CREATE INDEX IF NOT EXISTS idx_view_daily_date ON view_daily_stat(stat_date, view_count DESC);
