# 변경 기록

## 0.12 — 2026-11-06

- 좌석 인계 서버 DLL에 LAV/ZBD 봇 운전석 사격 수정을 통합. 차량 AI가 사격 판정에 사용하는 탑승 슬롯을 다시 확인해 주무기와 보조무기를 사용할 수 있도록 수정.
- 서버 시작 시 좌석 인계와 함께 적용. 기존 배치용 클라이언트 DLL을 그대로 사용.
- 사용자가 주무기·보조무기 사격과 통합본 적용 후 서버 렉이 사라진 것을 확인.

### English

- Integrate the LAV/ZBD bot driver weapon fix into the seat takeover server DLL. Correct the seat lookup used by vehicle AI so bot drivers can fire both primary and secondary weapons.
- Enable the fix alongside seat takeover at server startup. Reuse the existing deployment client DLL.
- The user confirmed primary and secondary weapon fire and reported no server lag with the combined build.

## 0.11 — 2026-11-05

- 서버를 처음 켰을 때부터 비행기에 탑승한 봇이 식별되지 않아 배치가 막히는 문제 수정.
- 차종별 처리에서 조기 반환하기 전의 공통 제어 진입점에서 봇을 등록하도록 변경. 기존 하드웨어 상태 보존 연결부를 그대로 사용.
- 식별 목록을 빈 상태로 초기화한 뒤 외부 등록 없이 32명 모두 자동 등록되고, 이전에 누락된 봇의 소유 관계가 검증되는 것을 확인.
- 사용자가 비행기 조종석 스폰과 봇 정상 출격을 확인. 클라이언트 DLL은 검증된 0.10 파일을 유지.

### English

- Fix deployment being unavailable for bots that start in aircraft immediately after server startup.
- Observe the first common control handler before vehicle-specific handlers can return early, using the existing transparent state-preserving adapter.
- Verified automatic registration of all 32 bots from an empty registry without externally importing identities, including previously missed bots.
- The user confirmed aircraft cockpit spawning and normal bot sorties. The client DLL remains the verified 0.10 binary.

## 0.10 — 2026-11-05

- 비행 제어 경로가 바뀐 봇도 살아 있는 소유 관계와 명단을 재검증해 식별하도록 수정.
- 1인승·만석 차량의 배치 화면 FULL 조건을 검증된 아군 봇 인계에 한해 통과.
- 유한 정원 2명 이상도 교체 인계하며 초과 시 봇을 완전히 하차.
- 상시 적용본에서 조종석 스폰과 봇 정상 출격 확인.

## 0.9

- 여러 UI 작업 스레드의 겹친 콜백으로 인한 배치 버튼 깜빡임 수정.
- DV15 배치 화면 운전석 스폰 확인.

## 0.5–0.6

- 봇 제어 호출의 레지스터·SIMD·플래그 보존으로 베이스 대기 회귀 수정.
- DV15 F1과 F2 이후 좌석 인계 지원.
