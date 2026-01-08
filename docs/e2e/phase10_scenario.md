# Phase 10 E2E Demo Scenario

This scenario documents the sequential request/response order used by the minimal demo client.
It mirrors the draft protocol in `docs/protocol.md` with lightweight sample payloads.

## Request/Response Order

1. **Login**
   - Request: `LoginReq`
   - Response: `LoginRes`

2. **PartyCreate**
   - Request: `PartyCreateReq`
   - Response: `PartyCreateRes`

3. **MatchReq**
   - Request: `MatchReq`
   - Response: `MatchFoundNotify`

4. **DungeonEnter**
   - Request: `DungeonEnterReq`
   - Response: `DungeonEnterRes`

5. **DungeonResult**
   - Request: `DungeonResultNotify`
   - Response: ACK summary (demo only)

6. **Reward grant**
   - Request: `RewardGrantNotify` (demo-only placeholder)
   - Response: `RewardGrantAck` (demo-only placeholder)

7. **Inventory update**
   - Request: `InventoryUpdateNotify`
   - Response: `InventoryUpdateAck` (demo-only placeholder)

## Example Payloads

### Login
```json
{
  "version": "1.0",
  "trace_id": "11111111-1111-1111-1111-111111111111",
  "type": "LoginReq",
  "timestamp": "2024-02-01T12:00:00Z",
  "payload": {
    "account": "demo@dungeonhub",
    "password": "***",
    "device_id": "device-123"
  }
}
```
```json
{
  "result": { "ok": true, "code": "OK", "message": "" },
  "data": {
    "token": "token-abc",
    "session_id": "session-001",
    "expires_in_sec": 3600
  }
}
```

### PartyCreate
```json
{
  "version": "1.0",
  "trace_id": "22222222-2222-2222-2222-222222222222",
  "type": "PartyCreateReq",
  "timestamp": "2024-02-01T12:00:01Z",
  "payload": { "leader_char_id": 1001 }
}
```
```json
{
  "result": { "ok": true, "code": "OK", "message": "" },
  "data": { "party_id": "party-123" }
}
```

### MatchReq → MatchFoundNotify
```json
{
  "version": "1.0",
  "trace_id": "33333333-3333-3333-3333-333333333333",
  "type": "MatchReq",
  "timestamp": "2024-02-01T12:00:02Z",
  "payload": {
    "party_id": "party-123",
    "dungeon_id": 2001,
    "difficulty": "normal"
  }
}
```
```json
{
  "party_id": "party-123",
  "instance_id": "instance-777",
  "endpoint": "dungeon.local:7777",
  "ticket": "enter-ticket"
}
```

### DungeonEnter
```json
{
  "version": "1.0",
  "trace_id": "44444444-4444-4444-4444-444444444444",
  "type": "DungeonEnterReq",
  "timestamp": "2024-02-01T12:00:03Z",
  "payload": {
    "instance_id": "instance-777",
    "ticket": "enter-ticket",
    "char_id": 1001
  }
}
```
```json
{
  "result": { "ok": true, "code": "OK", "message": "" },
  "data": { "state": "READY", "seed": 123456 }
}
```

### DungeonResult
```json
{
  "version": "1.0",
  "trace_id": "55555555-5555-5555-5555-555555555555",
  "type": "DungeonResultNotify",
  "timestamp": "2024-02-01T12:00:04Z",
  "payload": {
    "result": "CLEAR",
    "stats": { "time_sec": 320, "deaths": 1 },
    "rewards": [ { "item_id": 3001, "count": 2 } ]
  }
}
```
```json
{
  "result": { "ok": true, "code": "OK", "message": "" },
  "data": { "summary": "result recorded" }
}
```

### Reward grant (demo-only placeholder)
```json
{
  "version": "1.0",
  "trace_id": "66666666-6666-6666-6666-666666666666",
  "type": "RewardGrantNotify",
  "timestamp": "2024-02-01T12:00:05Z",
  "payload": {
    "char_id": 1001,
    "items": [ { "item_id": 3001, "count": 2 } ]
  }
}
```
```json
{
  "result": { "ok": true, "code": "OK", "message": "" },
  "data": { "grant_id": "grant-555" }
}
```

### Inventory update (demo-only placeholder)
```json
{
  "version": "1.0",
  "trace_id": "77777777-7777-7777-7777-777777777777",
  "type": "InventoryUpdateNotify",
  "timestamp": "2024-02-01T12:00:06Z",
  "payload": {
    "char_id": 1001,
    "items": [ { "item_id": 3001, "count": 2 } ]
  }
}
```
```json
{
  "result": { "ok": true, "code": "OK", "message": "" },
  "data": { "inventory_version": 42 }
}
```
