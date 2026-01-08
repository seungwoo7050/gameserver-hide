# DungeonHub

DungeonHub is a C++ server project blueprint aimed at Neople-style action RPG backend roles.
It focuses on session management, instance lifecycle, and data integrity for dungeon-based gameplay.

## Build
```bash
cmake -S . -B build
cmake --build build
```

## Run
```bash
./build/dungeonhub
```

## E2E Demo
Run the minimal scripted demo client and review the generated logs/docs.
```bash
./scripts/e2e_demo.sh
```
- Demo script: `scripts/e2e_demo.sh`
- Sample log: `docs/e2e/phase10_log.txt`
- Scenario doc: `docs/e2e/phase10_scenario.md`

## Development Flow
1. 설계 문서 확인: `docs/architecture.md`, `docs/protocol.md`
2. 모듈 경계 확인: `src/net`, `src/match`, `src/dungeon`, `src/party` 등
3. 새로운 기능 추가 시 프로토콜/시퀀스 다이어그램 갱신
4. 테스트 추가/검증: `tests/`에 케이스 확장

## Roadmap (Draft)
- 게이트웨이 세션 재접속/재전송 정책 강화
- 매칭/파티 상태 동기화 및 재시도 로직 확장
- 던전 이벤트 리플레이 및 전투 로그 저장
- 운영/GM 툴 API에 모니터링 지표 추가

## Docs
- [Architecture draft](docs/architecture.md)
- [Protocol spec](docs/protocol.md)
- [Contributing guide](CONTRIBUTING.md)
