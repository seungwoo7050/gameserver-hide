# Failure-case specifications

This document lists the canonical rejection codes and log messages emitted by the
server for common failure scenarios. The goal is to keep client-visible error
responses and server logs aligned for rapid triage.

## Invalid dungeon instance transitions

**Scope**: `DungeonResultNotify` requests that attempt to transition a dungeon
instance to a state that is not allowed (for example, `Waiting -> Playing`,
`Clear -> Ready`, or a duplicate `Clear` request).

- **Response code**: `INVALID_STATE`
- **Response message**: `Dungeon state transition rejected`
- **Log event**: `dungeon_result_failed`
- **Log message**: `Dungeon state transition rejected`

## Duplicate reward requests

**Scope**: Reward grants that reuse the same grant identifier. The reward layer
rejects the duplicate and the dungeon result handler maps it to a dedicated
response.

- **Response code**: `REWARD_DUPLICATE`
- **Response message**: `Reward grant already processed`
- **Log event**: `dungeon_result_failed`
- **Log message**: `Reward grant already processed`

## Session timeouts

**Scope**: Sessions that exceed the configured inactivity timeout and are
forcefully disconnected.

- **Log event**: `session_disconnected`
- **Log message**: `Session disconnected`
- **Log reason**: `timeout`

## Inventory update failures

**Scope**: Inventory update requests that fail to apply item additions.

- **Response code**: `INVENTORY_FAILED`
- **Response message**: `Failed to update inventory`
- **Log event**: `inventory_update_failed`
- **Log message**: `Failed to update inventory`
