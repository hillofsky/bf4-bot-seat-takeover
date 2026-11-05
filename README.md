# BF4 Bot Seat Takeover 0.10

[한국어](#한국어) · [English](#english)

## 한국어

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

### 기존 실행 순서에서 바뀌는 부분

`StartServer_SeatTakeover.cmd`는 **2번의 기존 서버 BAT 실행을 대체**합니다. 적용 후에는 다음 순서로 실행하세요.

1. `BF4BlazeServer.exe` 실행.
2. **`StartServer_SeatTakeover.cmd` 실행** — 기존 서버 BAT를 내부에서 실행하고 좌석 인계 모듈과 클라이언트 감시 로더를 적용합니다.
3. `BF4Launcher.exe`로 BF4 실행 후 서버 접속.
4. 필요하면 `PRoCon.exe`를 실행해 서버 제어.

서버 BAT를 따로 한 번 더 실행할 필요는 없습니다. 이 CMD는 Blaze 서버, 게임 런처 또는 PRoCon을 대신 실행하지 않습니다.

현재 스크립트는 `Server` 폴더의 **`!StartServer.bat`**를 찾습니다. 실제 파일명이 `StartServer.bat`라면 `CodexSeatTakeover/Control-SeatTakeover.ps1`의 `$serverLauncher=Join-Path $serverRoot '!StartServer.bat'`에서 파일명을 실제 이름으로 바꾸세요. 기존 BAT의 내용은 그대로 유지합니다. 이미 서버가 실행 중이면 BAT를 다시 실행하지 않고 추가 모듈만 적용합니다.

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

## English

An unofficial add-on for taking over vehicle seats occupied by allied bots on a BF4 dedicated server. It uses both a server DLL and a game client DLL. An existing bot mod and the exact supported executable builds are required.

### Features

- **E outside a vehicle:** take over the driver's seat from an allied bot.
- **F1, F2, and subsequent seat keys while inside:** take over the corresponding seat from an allied bot.
- **Vehicle icon → Deploy on the deployment screen:** spawn into the driver's seat of a vehicle driven by an allied bot.
- Supports replacing bots in single-seat and fully occupied vehicles. When necessary to stay within a finite player capacity, the displaced bot exits the vehicle completely.
- Uses a common path without vehicle-name restrictions, including J-20, Q-5, tanks, armored vehicles, mobile AA, boats, and helicopters. Not every vehicle type has been tested individually.

The add-on grants no extra takeover permission for seats occupied by human players or enemies. Native team, destruction, spawn cooldown, and prohibited-seat checks remain in place. Soldiers are created through the original engine path using the selected class and loadout. The add-on does not create vehicles that have no deployment icon.

### Installation and use

1. Close the server and game, and back up any existing add-on files.
2. Copy the `CodexSeatTakeover` folder and both CMD files from `release/` into the game's **Server folder**. Original game executables and `DINPUT8.dll` are neither included nor replaced.
3. Replace `HumanName=YOUR_PLAYER_NAME` with your in-game nickname in **both** configuration files:
   - `Server/CodexSeatTakeover/BF4SeatTakeover.ini`
   - `Server/CodexSeatTakeover/Client/BF4DeployAll.ini`
4. Run **StartServer_SeatTakeover.cmd** and approve the Windows administrator prompt if shown.
5. Start the game with your usual launcher and connect to the server.

### How this changes your existing startup sequence

`StartServer_SeatTakeover.cmd` **replaces step 2: running the original server BAT**. Use this sequence:

1. Start `BF4BlazeServer.exe`.
2. Run **`StartServer_SeatTakeover.cmd`**. It invokes the original server BAT internally, applies the seat takeover module, and starts the client watcher.
3. Launch BF4 through `BF4Launcher.exe` and connect to the server.
4. Optionally start `PRoCon.exe` to manage the server.

Do not run the server BAT a second time. This CMD does not start or replace the Blaze server, game launcher, or PRoCon.

The current script expects **`!StartServer.bat`** in the `Server` folder. If your file is named `StartServer.bat`, edit the filename in `$serverLauncher=Join-Path $serverRoot '!StartServer.bat'` inside `CodexSeatTakeover/Control-SeatTakeover.ps1` to match it. Keep the original BAT contents unchanged. If the server is already running, the CMD applies the add-on without running the BAT again.

The CMD launcher starts the server if it is stopped, or applies the add-on without restarting an already running server. Close both the server and game before replacing installed files. While the server runs, a hidden loader watches for game restarts and applies the client companion. Running only the original server BAT does not automatically apply this add-on.

Run **StopSeatTakeover.cmd** to stop the watcher and disable the add-on. If a DLL cannot be safely unloaded immediately, it stays loaded but disabled until the process exits. The stop script does not forcibly terminate the server or game.

### Supported builds

Windows x64. Both executable hashes must match exactly; matching filenames alone are insufficient.

| Executable | SHA-256 |
|---|---|
| bf4_Server_Final.exe | `160ec988aad0632946936a21a9aed1c73f5b906161f2d96b2a085ee65ce183ba` |
| bf4_no_drm.exe | `0f3b4ca5e4b9642413b8df5d869d94337a8bb690238ae2b2e5a780a68942aeb4` |

Supply the existing bot mod and game files separately. This repository does not distribute bot AI, game data, or game executables.

### Building and testing

Requires Visual Studio C++ x64 tools, the Windows SDK, MASM, and C++17 support. The build scripts locate Visual Studio through `vswhere`, or use the current x64 developer command prompt.

```bat
build.cmd
test.cmd
```

- Server output: `native-adapter/build-addon/BF4SeatTakeover.dll`.
- Client output: `native-client/build/BF4DeployClient.dll`. Rename it to **BF4DeployAll.dll** when installing.
- Update the DLL hashes in the deployment manifests when installing newly built binaries.
- Set `BF4_SERVER_EXE` to the actual server executable path to run the positive executable-fingerprint test. Without it, only that test is skipped; the other regression tests still run.

Live validation includes AA/M142 outside entry, LAV/DV15 F1, F2 seat takeover, DV15 deployment without button flicker, and Q-5 cockpit spawning in the prototype. The permanent 0.10 build was also confirmed to support cockpit spawning with normal bot departures from base. This does not establish individual compatibility for every vehicle. See the [validation report](docs/VALIDATION.md) for the scope and limitations.

### Repository layout

| Directory | Contents |
|---|---|
| native-adapter | Server integration, bot identification, seat replacement, and deployment requests |
| native-client | Deployment UI integration, full-vehicle exception, and concurrent UI state management |
| core | Initial engine-independent model and regression tests |
| release | 0.10 DLLs, configuration templates, and start/stop scripts |
| docs | Validation results and design records |
| vendor/minhook | MinHook 1.3.4 source and license |

[Validation](docs/VALIDATION.md) · [Changelog](CHANGELOG.md) · [Architecture](docs/ARCHITECTURE.md) · [Third-party code](THIRD-PARTY.md)

The linked project documents are currently in Korean, except for the third-party notice. No separate open-source license has been assigned to this project's own code. MinHook is covered by its included license.
