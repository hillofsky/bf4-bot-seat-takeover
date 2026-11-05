# BF4 Bot Seat Takeover 0.11 — 설치 안내

아군 봇이 탑승한 차량의 좌석을 인계받는 비공식 추가 모듈입니다. 서버와 게임 클라이언트에 각각 DLL을 적용합니다. 게임 원본 파일과 기존 봇 모드는 포함하지 않습니다.

1. 서버와 게임을 종료하고 기존 추가 모듈을 백업합니다.
2. ZIP의 `CodexSeatTakeover` 폴더와 CMD 두 개를 게임의 **Server 폴더**에 복사합니다.
3. `CodexSeatTakeover/BF4SeatTakeover.ini`와 `CodexSeatTakeover/Client/BF4DeployAll.ini`의 `HumanName=YOUR_PLAYER_NAME`을 자신의 게임 닉네임으로 함께 변경합니다.
4. `StartServer_SeatTakeover.cmd`를 실행하고 Windows 관리자 권한 창을 승인합니다.
5. 게임은 기존 실행기로 실행해 서버에 접속합니다.

차량 밖에서는 E, 탑승 중에는 F1/F2 이후 좌석 키로 아군 봇에게서 인계받습니다. 스폰 화면에서는 봇이 탄 장비 아이콘을 선택한 뒤 배치합니다. 1인승·만석 장비도 공통 경로를 사용하지만 모든 기종을 개별 시험한 것은 아닙니다.

CMD는 파일을 덮어쓰는 설치기가 아닙니다. 이미 서버가 실행 중이면 재시작 없이 모듈만 적용합니다. 파일을 새 버전으로 교체할 때는 서버와 게임을 먼저 종료하세요. 서버가 실행 중인 동안 로더가 게임 재실행을 감시합니다. 추가 기능을 끄려면 `StopSeatTakeover.cmd`를 실행합니다.

정확히 지원되는 실행 파일의 SHA-256과 빌드 방법은 [저장소 사용법](https://github.com/hillofsky/bf4-bot-seat-takeover#readme), 실제 시험 범위는 [검증 결과](https://github.com/hillofsky/bf4-bot-seat-takeover/blob/main/docs/VALIDATION.md)를 참고하세요. 다른 빌드에는 적용되지 않습니다. DLL 해시는 동봉된 manifest.json 두 개에 있습니다.

MinHook 1.3.4의 라이선스는 MinHook-LICENSE.txt에 있습니다. 프로젝트 자체에는 별도 오픈소스 라이선스를 지정하지 않았습니다.
