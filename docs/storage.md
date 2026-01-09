# Storage & Inventory Persistence

## Overview
DungeonHub의 인벤토리 저장소는 영구 저장소(DB)와 캐시 계층을 분리한 구조를 따른다.
현재 구현은 MySQL 기반 영구 저장소(`MySqlInventoryStorage`) + 인메모리 캐시
(`CachedInventoryStorage` + `InMemoryInventoryStorage`) 조합이다.

## CRUD Flow (User → Character → Inventory)
1. **Create**
   - 유저 생성 → 캐릭터 생성 후 `inventory_id`(= `char_id`)로 초기 인벤토리 레코드 생성.
   - `saveInventory()`로 초기 상태를 영구 저장소에 저장하고 캐시에 반영한다.
2. **Read**
   - `loadInventory()`는 캐시를 우선 조회한다.
   - 캐시 miss 시 영구 저장소에서 로드 후 캐시에 채운다.
3. **Update**
   - `addItem/removeItem/setItem()`는 영구 저장소에 write-through 적용.
   - 영구 저장소 커밋 후 캐시에 동일 변경을 반영한다.
4. **Delete**
   - 수량이 0이 되는 아이템은 인벤토리 엔트리에서 제거한다.
   - 캐시에서 제거되거나 덮어쓰면서 stale 데이터를 제거한다.

## Persistence & Cache Strategy
- **Write-through**: 영구 저장소에 먼저 쓰고, 성공 시 캐시를 갱신한다.
- **Read-through**: 캐시 miss 시 영구 저장소에서 조회 후 캐시에 적재한다.
- **Stale 제거**: 캐시 적용 실패 시 영구 저장소 재조회로 캐시를 갱신한다.
- **Change log**: 변경 이력은 영구 저장소 기준으로 관리한다.

## Transaction Consistency
- `beginTransaction()`은 영구 저장소/캐시 양쪽에서 트랜잭션을 시작한다.
- `commitTransaction()`은 영구 저장소 → 캐시 순서로 커밋한다.
- `rollbackTransaction()`은 영구 저장소/캐시 모두 롤백하여 일관성을 유지한다.

## Operational Notes
- 캐시는 인벤토리 조회 지연을 줄이기 위한 계층이며,
  최종 정합성은 영구 저장소 상태를 기준으로 복구 가능하다.
- 필요 시 캐시 무효화 정책(예: TTL, LRU)을 추가해 확장할 수 있다.
