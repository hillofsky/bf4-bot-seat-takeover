# 검증 결과

0.10, 2026-11-05. 실제 Windows x64 BF4 서버/게임 세션과 독립 회귀 검사에서 확인했습니다.

- 기존 E 인계: AA와 M142.
- 기존 좌석 키 인계: LAV/DV15 F1, F2.
- 0.9: DV15 배치 화면 버튼 깜빡임 해소, 운전석 스폰.
- 0.10 시험본: Q-5 공격기 조종석 스폰을 사용자가 확인. 서버 인계 성공 2회, 클라이언트 만석 예외 158회, 관찰 오류 0회.
- 0.10 상시 DLL: 사용자가 조종석 스폰 및 봇 정상 출격을 확인.
- 모든 차량 이름을 허용하는 공통 경로지만, J-20과 헬기를 포함한 모든 기종의 개별 성공을 주장하지 않습니다.

회귀 검사: 봇 식별의 오래된 시각 거부 회귀 RED→GREEN, 실제 생산 정원 정책과 하차 처리 RED→GREEN, 5개 실제 작업 스레드의 겹친 UI 처리, 상태 세대/재사용/실패 복원, 공유 증명 40000회 동시 발행 및 읽기 전용 뷰. 인라인 FULL 어댑터는 실제 JMP 호출 구조에서 허용/거부 각각 10000회를 검사했습니다. RAX·GP·XMM·플래그·MXCSR·RSP 보존과 원래 6바이트 실행을 확인했습니다.

서버·클라이언트 DLL은 MSVC C++17 /O2 /W4 /WX 빌드를 통과했습니다. 배포 스크립트 구문과 서버 종료 시 로더 종료를 확인했습니다. 게임을 강제 종료하지 않았으므로 완전 종료 후 UAC 자동 시작 경로를 새로 실증한 것은 아닙니다.

DLL 해시는 release/CodexSeatTakeover의 manifest.json 두 개를 확인하세요. 설정은 배포를 위해 닉네임 템플릿으로 바꿨습니다. 현재 배포본은 실제 확인한 0.11 서버 DLL과 기존 0.10 클라이언트 DLL입니다.

GitHub 정리본도 별도 폴더에서 서버·클라이언트 빌드와 test.cmd 전체를 다시 실행해 통과했습니다. 실제 서버 파일을 지정한 양성 지문 검사도 통과했습니다. 개인 닉네임·설치 경로·인증 정보와 게임 원본 실행 파일은 업로드 대상에서 제외했습니다.

## 0.11 — 서버 첫 시작의 비행기 배치 수정

지상/보트 처리만 관찰하면 먼저 처리되는 비행기 제어가 조기 반환해 봇 식별을 건너뛰었습니다. 등록 지점을 첫 공통 처리 함수로 옮겼습니다. 빈 등록 목록에서 외부 식별 정보 주입 없이 봇 32명 모두 자동 등록됐으며, 이전에 누락된 봇 3명의 실제 소유 관계도 검증했습니다. 사용자가 수정본의 비행기 조종석 스폰과 봇 정상 출격을 확인했습니다.

변경된 서버 DLL의 /O2 /W4 /WX 빌드 및 기존 투명 연결부 ABI 검사(Boat/Ground 각각 10000회)를 통과했습니다. 실행 중 등록 목록을 비운 상태에서 검증했으며, 프로세스 전체를 강제로 재시작한 시험은 아닙니다.

### English

Version 0.11 observes bots before aircraft-specific handlers can return early. All 32 bots were automatically registered from an empty registry without importing identities; ownership of the three previously missed bots was verified. The user confirmed aircraft cockpit spawning and normal bot sorties. The changed server DLL passed the optimized warning-clean build and existing observer ABI tests. This validation reset the live registry; it did not forcibly restart the entire server process.
