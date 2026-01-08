# Contributing Guide

DungeonHub는 던전/세션 기반 서버 구조를 학습하기 위한 샘플 프로젝트입니다.
변경 사항은 문서/코드가 함께 업데이트되는 것을 원칙으로 합니다.

## Build & Run
```bash
cmake -S . -B build
cmake --build build
./build/dungeonhub
```

## Development Flow
1. **문서 확인**
   - 아키텍처와 프로토콜 문서를 먼저 읽고 변경 범위를 정의합니다.
   - 관련 문서: `docs/architecture.md`, `docs/protocol.md`
2. **작업 범위 선정**
   - 세션/네트워크: `src/net`
   - 매칭/파티: `src/match`, `src/party`
   - 던전 로직: `src/dungeon`, `src/combat`
3. **변경 반영**
   - 메시지 포맷 변경 시 `docs/protocol.md` 갱신
   - 상태/플로우 변경 시 `docs/architecture.md` 다이어그램 갱신
4. **테스트 추가**
   - 신규 기능은 `tests/`에 최소 1개 이상의 시나리오 추가를 권장합니다.

## Roadmap (Draft)
- 세션 재접속 및 패킷 재전송 정책 세분화
- 매칭 실패/취소 케이스에 대한 서버 로직 강화
- 던전 이벤트 리플레이 및 전투 로그 저장
- 운영/GM API: 모니터링 지표/대시보드 연동

## Pull Request Checklist
- [ ] 관련 문서(architecture/protocol) 업데이트 여부 확인
- [ ] 새 메시지 타입 정의 시 버전/호환성 검토
- [ ] 테스트 또는 수동 검증 결과 기록
