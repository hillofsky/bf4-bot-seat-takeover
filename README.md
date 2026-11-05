# BF4 Bot Seat Takeover 0.10

BF4 전용 서버의 아군 봇에게서 차량 좌석을 인계받는 비공식 추가 모듈입니다. 서버 DLL과 게임 클라이언트 DLL을 함께 사용합니다. 기존 봇 모드가 설치된, 아래 실행 파일 지문과 일치하는 환경을 대상으로 합니다.

## 기능

- 차량 밖에서 **E**: 아군 봇 운전석 인계.
- 탑승 중 **F1·F2·이후 좌석 키**: 해당 좌석의 아군 봇에게서 인계.
- 스폰 화면에서 **차량 아이콘 → 배치**: 봇이 운전 중인 장비의 운전석에 스폰.
- 1인승 및 만석 차량도 교체 인계. 유한 정원을 넘게 되면 봇이 완전히 하차합니다.
- J-20·Q-5·전차·장갑차·대공차·보트·헬기 등 차종 이름에 따른 제한이 없는 공통 경로.

사람과 적군의 좌석에는 추가 권한을 부여하지 않습니다. 원래 팀·파괴·스폰 대기·금지 좌석 등 배치 검사를 유지합니다. 선택한 병과의 병사 생성도 원래 엔진 경로를 사용합니다. 차량 아이콘이 없는 장비를 새로 만드는 기능은 없습니다. 모든 차종을 개별 검증한 것은 아닙니다.

## 설치와 실행

1. 서버를 종료하고 기존 추가 모듈을 백업합니다.
2. `release/` 안의 `CodexSeatTakeover` 폴더와 CMD 두 개를 게임의 **Server 폴더**에 복사합니다. 게임 원본 EXE 및 DINPUT8.dll은 포함하거나 덮어쓰지 않습니다.
3. 아래 두 INI의 `HumanName=YOUR_PLAYER_NAME`을 자신의 게임 닉네임으로 함께 변경합니다.
   - `Server/CodexSeatTakeover/BF4SeatTakeover.ini`
   - `Server/CodexSeatTakeover/Client/BF4DeployAll.ini`
4. **StartServer_SeatTakeover.cmd**를 실행합니다. Windows 관리자 권한 창이 뜨면 승인합니다.
5. 게임은 기존 실행기로 실행하고 서버에 접속합니다.

CMD는 파일을 덮어쓰는 설치기가 아닙니다. 서버가 꺼져 있으면 서버를 시작하고 모듈을 적용하며, 이미 켜져 있으면 재시작 없이 모듈을 적용합니다. 설치 파일을 교체할 때는 서버와 게임을 먼저 종료하세요. 서버가 실행 중인 동안 숨김 로더가 게임 재실행을 감시합니다. 기존 서버 BAT만 실행하면 이 추가 모듈은 자동 적용되지 않습니다.

**StopSeatTakeover.cmd**는 감시 로더와 추가 기능을 끕니다. 안전하게 DLL을 해제할 수 없으면 기능을 비활성화하고 프로세스 종료까지 유지합니다. 서버/게임 프로세스를 강제로 종료하지 않습니다.

## 지원 환경

Windows x64, 아래 SHA-256과 일치하는 서버와 게임을 대상으로 합니다. 파일명만 같은 다른 빌드에는 적용되지 않습니다.

| 대상 | SHA-256 |
|---|---|
| bf4_Server_Final.exe | `160ec988aad0632946936a21a9aed1c73f5b906161f2d96b2a085ee65ce183ba` |
| bf4_no_drm.exe | `0f3b4ca5e4b9642413b8df5d869d94337a8bb690238ae2b2e5a780a68942aeb4` |

기존 봇 모드 및 게임 파일은 별도로 준비해야 합니다. 이 저장소는 봇 AI, 게임 데이터 또는 게임 실행 파일을 배포하지 않습니다.

## 빌드와 검증

Visual Studio C++ x64 도구, Windows SDK, MASM 및 C++17이 필요합니다. `build.cmd`는 설치된 Visual Studio를 vswhere로 찾거나 현재 x64 개발자 명령 프롬프트를 사용합니다.

```bat
build.cmd
test.cmd
```

서버 결과물: `native-adapter/build-addon/BF4SeatTakeover.dll`.
클라이언트 결과물: `native-client/build/BF4DeployClient.dll`; 설치할 때 **BF4DeployAll.dll**로 이름을 바꿉니다. 빌드 결과를 적용할 때 배포 manifest의 DLL 해시도 갱신하세요.

실제 서버 이미지의 양성 지문 검사까지 실행하려면 `BF4_SERVER_EXE` 환경 변수에 실행 파일 경로를 설정합니다. 없으면 그 검사만 건너뛰고 나머지 회귀 검사는 실행됩니다.

## 구성

| 경로 | 내용 |
|---|---|
| native-adapter | 실제 서버 연결부, 봇 식별, 좌석 교체, 배치 요청 처리 |
| native-client | 배치 화면 연결부, 만석 예외, 다중 작업 스레드 상태 관리 |
| core | 엔진과 독립적인 초기 모델과 회귀 검사 |
| release | 0.10 DLL, 설정 템플릿, 실행 및 중지 스크립트 |
| docs | 검증 결과와 설계 기록 |
| vendor/minhook | MinHook 1.3.4 소스와 라이선스 |

[검증 범위](docs/VALIDATION.md) · [변경 기록](CHANGELOG.md) · [구조](docs/ARCHITECTURE.md) · [외부 코드](THIRD-PARTY.md)

이 프로젝트 자체에는 별도 오픈소스 라이선스를 지정하지 않았습니다. MinHook에는 동봉된 라이선스가 적용됩니다.
