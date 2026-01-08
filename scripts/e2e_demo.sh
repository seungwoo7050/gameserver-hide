#!/usr/bin/env bash
set -euo pipefail

log_line() {
  local message="$1"
  if [[ -n "${LOG_FILE:-}" ]]; then
    printf '%s\n' "$message" | tee -a "$LOG_FILE"
  else
    printf '%s\n' "$message"
  fi
}

trace_id() {
  if command -v uuidgen >/dev/null 2>&1; then
    uuidgen
  else
    python3 - <<'PY'
import uuid
print(uuid.uuid4())
PY
  fi
}

timestamp() {
  date -u +"%Y-%m-%dT%H:%M:%SZ"
}

run_step() {
  local step="$1"
  local req="$2"
  local res="$3"
  local trace
  trace="$(trace_id)"
  req="${req//<trace>/${trace}}"
  req="${req//<ts>/$(timestamp)}"
  res="${res//<trace>/${trace}}"
  res="${res//<ts>/$(timestamp)}"
  log_line "[$(timestamp)] step=${step} trace_id=${trace} request=${req}"
  log_line "[$(timestamp)] step=${step} trace_id=${trace} response=${res}"
}

LOG_FILE="${1:-}"

run_step "Login" \
  '{"version":"1.0","trace_id":"<trace>","type":"LoginReq","timestamp":"<ts>","payload":{"account":"demo@dungeonhub","password":"***","device_id":"device-123"}}' \
  '{"result":{"ok":true,"code":"OK","message":""},"data":{"token":"token-abc","session_id":"session-001","expires_in_sec":3600}}'

run_step "PartyCreate" \
  '{"version":"1.0","trace_id":"<trace>","type":"PartyCreateReq","timestamp":"<ts>","payload":{"leader_char_id":1001}}' \
  '{"result":{"ok":true,"code":"OK","message":""},"data":{"party_id":"party-123"}}'

run_step "MatchReq" \
  '{"version":"1.0","trace_id":"<trace>","type":"MatchReq","timestamp":"<ts>","payload":{"party_id":"party-123","dungeon_id":2001,"difficulty":"normal"}}' \
  '{"party_id":"party-123","instance_id":"instance-777","endpoint":"dungeon.local:7777","ticket":"enter-ticket"}'

run_step "DungeonEnter" \
  '{"version":"1.0","trace_id":"<trace>","type":"DungeonEnterReq","timestamp":"<ts>","payload":{"instance_id":"instance-777","ticket":"enter-ticket","char_id":1001}}' \
  '{"result":{"ok":true,"code":"OK","message":""},"data":{"state":"READY","seed":123456}}'

run_step "DungeonResult" \
  '{"version":"1.0","trace_id":"<trace>","type":"DungeonResultNotify","timestamp":"<ts>","payload":{"result":"CLEAR","stats":{"time_sec":320,"deaths":1},"rewards":[{"item_id":3001,"count":2}]}}' \
  '{"result":{"ok":true,"code":"OK","message":""},"data":{"summary":"result recorded"}}'

run_step "RewardGrant" \
  '{"version":"1.0","trace_id":"<trace>","type":"RewardGrantNotify","timestamp":"<ts>","payload":{"char_id":1001,"items":[{"item_id":3001,"count":2}]}}' \
  '{"result":{"ok":true,"code":"OK","message":""},"data":{"grant_id":"grant-555"}}'

run_step "InventoryUpdate" \
  '{"version":"1.0","trace_id":"<trace>","type":"InventoryUpdateNotify","timestamp":"<ts>","payload":{"char_id":1001,"items":[{"item_id":3001,"count":2}]}}' \
  '{"result":{"ok":true,"code":"OK","message":""},"data":{"inventory_version":42}}'
