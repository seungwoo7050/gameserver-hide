# DungeonHub 프로토콜 스펙 (Draft)

본 문서는 클라이언트와 서버(게이트웨이/매칭/던전 등) 간 통신 프로토콜의 공통 규약을 정리합니다.
실제 구현에서는 메시지 ID와 필드가 확장될 수 있으며, 버전 관리(Version/Feature Flag)를 통해 호환성을 유지합니다.

## 1. 전송 계층 및 메시지 프레이밍
- **전송**: TCP 기반, keep-alive/heartbeat 지원
- **인코딩**: JSON(가독성 우선), 향후 바이너리 포맷 전환 가능
- **프레임 구조**
  - `length`(uint32) + `payload`(UTF-8 JSON)
  - `payload`는 아래 공통 Envelope 형식을 따른다.

## 2. 공통 Envelope
```json
{
  "version": "1.0",
  "trace_id": "uuid",
  "type": "LoginReq",
  "timestamp": "2024-01-01T00:00:00Z",
  "payload": { }
}
```
- `version`: 프로토콜 버전
- `trace_id`: 분산 추적용 식별자
- `type`: 메시지 타입(요청/응답/알림)
- `timestamp`: 클라이언트/서버가 기록하는 표준 시간
- `payload`: 메시지별 본문

## 3. 공통 응답 규칙
- 모든 응답에는 `result` 필드를 포함한다.
```json
{
  "result": {
    "ok": true,
    "code": "OK",
    "message": ""
  },
  "data": { }
}
```
- 오류 코드는 서비스 공통 규약을 유지한다.
  - `OK`
  - `AUTH_INVALID`
  - `SESSION_EXPIRED`
  - `MATCH_TIMEOUT`
  - `DUNGEON_NOT_FOUND`
  - `INTERNAL_ERROR`

## 4. 인증/세션 프로토콜
### 4.1 LoginReq/LoginRes
```json
// LoginReq
{
  "account": "user@example.com",
  "password": "***",
  "device_id": "device-uuid"
}
```
```json
// LoginRes
{
  "result": { "ok": true, "code": "OK", "message": "" },
  "data": {
    "token": "jwt-or-opaque-token",
    "session_id": "session-uuid",
    "expires_in_sec": 3600
  }
}
```

### 4.2 HeartbeatReq/HeartbeatRes
- 세션 유효성 유지 및 RTT 측정
```json
// HeartbeatReq
{
  "seq": 42
}
```
```json
// HeartbeatRes
{
  "result": { "ok": true, "code": "OK", "message": "" },
  "data": { "seq": 42, "server_time": "2024-01-01T00:00:00Z" }
}
```

### 4.3 SessionReconnectReq/SessionReconnectRes
```json
// SessionReconnectReq
{
  "token": "jwt-or-opaque-token",
  "last_seq": 128
}
```
```json
// SessionReconnectRes
{
  "result": { "ok": true, "code": "OK", "message": "" },
  "data": { "session_id": "session-uuid", "resume_from_seq": 129 }
}
```

## 5. 파티/매칭 프로토콜
### 5.1 PartyCreateReq/PartyCreateRes
```json
// PartyCreateReq
{ "leader_char_id": 1001 }
```
```json
// PartyCreateRes
{
  "result": { "ok": true, "code": "OK", "message": "" },
  "data": { "party_id": "party-uuid" }
}
```

### 5.2 MatchReq/MatchFoundNotify
```json
// MatchReq
{
  "party_id": "party-uuid",
  "dungeon_id": 2001,
  "difficulty": "normal"
}
```
```json
// MatchFoundNotify
{
  "party_id": "party-uuid",
  "instance_id": "instance-uuid",
  "endpoint": "dungeon.example.com:7777",
  "ticket": "enter-ticket"
}
```

### 5.3 MatchCancelReq/MatchCancelRes
```json
// MatchCancelReq
{ "party_id": "party-uuid" }
```

## 6. 던전 인스턴스 프로토콜
### 6.1 DungeonEnterReq/DungeonEnterRes
```json
// DungeonEnterReq
{
  "instance_id": "instance-uuid",
  "ticket": "enter-ticket",
  "char_id": 1001
}
```
```json
// DungeonEnterRes
{
  "result": { "ok": true, "code": "OK", "message": "" },
  "data": { "state": "READY", "seed": 123456 }
}
```

### 6.2 DungeonEventNotify
- 전투/스킬/데미지 등 실시간 이벤트
```json
{
  "event_type": "SkillCast",
  "seq": 88,
  "payload": {
    "caster_id": 1001,
    "skill_id": 501,
    "target_ids": [2001]
  }
}
```

### 6.3 DungeonResultNotify
```json
{
  "result": "CLEAR",
  "stats": { "time_sec": 320, "deaths": 1 },
  "rewards": [ { "item_id": 3001, "count": 2 } ]
}
```

## 7. 보상/인벤토리 프로토콜
### 7.1 InventoryUpdateNotify
```json
{
  "char_id": 1001,
  "items": [ { "item_id": 3001, "count": 2 } ]
}
```

## 8. 운영/관리 프로토콜
### 8.1 AdminKickReq/AdminKickRes
```json
// AdminKickReq
{ "session_id": "session-uuid", "reason": "policy" }
```

## 9. 버전 관리 및 확장 규칙
- `version` 필드 증가 시 하위 호환 유지
- 신규 메시지는 `type` 충돌 방지 규칙을 따른다.
- 필드 추가는 optional로 간주하며, 기본값을 적용한다.
