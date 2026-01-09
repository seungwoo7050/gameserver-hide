#!/usr/bin/env bash
set -euo pipefail

output=$(sqlite3 :memory: <<'SQL'
CREATE TABLE inventory (char_id INTEGER, item_id INTEGER, count INTEGER);
CREATE TABLE match_history (match_id INTEGER, instance_id INTEGER, char_id INTEGER, result TEXT, time INTEGER);

CREATE INDEX idx_inventory_char_item ON inventory (char_id, item_id);
CREATE INDEX idx_match_history_char_time ON match_history (char_id, time DESC);

EXPLAIN QUERY PLAN SELECT * FROM inventory WHERE char_id = 1;
EXPLAIN QUERY PLAN
SELECT match_id, instance_id, result, time
  FROM match_history
 WHERE char_id = 1
 ORDER BY time DESC
 LIMIT 20;
SQL
)

echo "$output" | grep -q "idx_inventory_char_item"
echo "$output" | grep -q "idx_match_history_char_time"
echo "Query plan checks passed."
