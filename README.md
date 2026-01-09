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

## Demo Procedure
Use the scripted E2E client to walk through the match → dungeon → reward flow and review
the artifacts it produces.
1. Run the demo script (optionally pass a log file path).
   ```bash
   ./scripts/e2e_demo.sh docs/e2e/phase10_log.txt
   ```
2. Inspect the generated log artifacts in `docs/e2e/phase10_log.txt`.
3. Cross-check the request/response ordering in `docs/e2e/phase10_scenario.md`.
4. Compare the flow against the updated protocol and state machine docs:
   - [Protocol spec](docs/protocol.md)
   - [Architecture/state machine](docs/architecture.md)

## Load Simulation
Build the load simulation target and run it to generate match/overflow logs and a summary.
```bash
cmake -S . -B build
cmake --build build --target dungeonhub_load_sim
./build/dungeonhub_load_sim --sessions 24 --requests-per-session 1 --concurrency 6 \\
  --send-queue-limit 2048 --overflow-payload 4096 --overflow-burst 3 \\
  --log-path docs/load_run.log --summary-path docs/load_summary.md
```
- Script: `scripts/load_match_sim.cpp`
- Summary: `docs/load_summary.md`

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
- [Storage & persistence](docs/storage.md)
- [Contributing guide](CONTRIBUTING.md)
