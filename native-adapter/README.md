# 서버 연결부

실제 적용 DLL의 진입점은 `seat_addon.cpp`입니다. `build_addon.cmd`로 빌드하며 `build-addon/BF4SeatTakeover.dll`을 만듭니다. 설치 방법은 저장소 루트 README를 참고하세요.

- `bot_identity.hpp`: 관찰된 봇과 현재 소유 관계 및 명단 재검증.
- `driver_policy.*`: 플레이어 요청과 아군 봇 좌석 인계 정책.
- `capacity_policy.hpp`: 유한 정원 교체와 하차 정책.
- `deployment_proof.*`: 클라이언트가 읽는 짧은 수명의 배치 허용 증명.
- `r63_seat_reader.*`, `server_image_guard.*`: 읽기 경계, 실행 파일 지문 및 버전 확인.
- `abi/`: 원래 봇 제어 호출의 하드웨어 상태 보존 및 독립 회귀 검사.
- `native_adapter.*`, `r63_bindings.*`, `r63_vehicle_actions.*`: 초기 추상 경계와 별도 회귀 검사. 실제 런타임 설치 경로와 구분합니다.

초기 설계 기록은 [BOUNDARY-HISTORY.md](../docs/BOUNDARY-HISTORY.md), 현재 검증 범위는 [VALIDATION.md](../docs/VALIDATION.md)를 참고하세요.
