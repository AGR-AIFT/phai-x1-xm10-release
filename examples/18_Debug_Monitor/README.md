# 예제 18: 시스템 디버깅 모니터 (Debug Monitor)

본 예제는 임베디드 시스템의 **실시간 디버깅 기법**을 다룹니다. 루프 실행 시간 측정, 시스템 Health 대시보드, 진단 LED 패턴, 데이터 신선도(Staleness) 감지 등 양산 환경에서 활용 가능한 **비침습적 모니터링 패턴**을 학습합니다.

> 📖 API 레퍼런스: [LED & Button](../../docs/api-reference/03-led-btn-control.md) · [USB Connectivity](../../docs/api-reference/05-usb-connectivity.md)

## 🎯 학습 목표 (Objective)

* **루프 실행 시간 측정:** Min/Max/Avg 통계 수집 및 오버런(2ms 초과) 감지 방법을 학습합니다.
* **시스템 Health 대시보드:** USB CDC를 활용하여 1초 주기로 시스템 상태를 종합 출력하는 패턴을 이해합니다.
* **진단 LED 패턴:** USB 연결 없이도 LED만으로 시스템 상태(정상/오류/연결 상태)를 파악하는 방법을 학습합니다.
* **Stale Data 감지:** Watchdog 패턴으로 CM 데이터가 5초간 변화하지 않으면 경고를 발생시키는 방법을 이해합니다.

---

## ⚙️ 동작 원리 (How it Works)

이 예제는 **단일 상태(1-state)** TSM으로 동작합니다. 디버그 모니터는 항상 실행되어야 하므로 복잡한 상태 전환 없이 모든 모니터링 기능을 순차적으로 실행합니다.

### 1. 루프 실행 시간 측정 (매 2ms)

`User_Loop` 시작과 끝에서 `XM_GetTick()` 차이를 측정합니다. ms 해상도의 한계로 대부분 0ms로 측정되지만, **오버런(>2ms) 감지**는 시스템 이상 징후를 포착하는 데 유용합니다.

### 2. Health 대시보드 (매 1초)

USB CDC로 다음 정보를 출력합니다:
*  **Uptime:** 시스템 가동 시간 (HH:MM:SS)
*  **Loop Count / Overrun:** 전체 루프 실행 횟수 및 오버런 발생 횟수
*  **Loop Timing:** Min / Max / Avg 실행 시간
*  **CM 상태:** 연결 여부 + 데이터 신선도 (Fresh / STALE)
*  **ADC 값:** 외부 ADC 채널 전압

### 3. 진단 LED 패턴

| LED | 조건 | 패턴 |
|-----|------|------|
| LED 1 | 정상 | Heartbeat (1초) |
| LED 1 | 오버런 또는 STALE | 빠른 Blink (200ms) |
| LED 2 | CM 연결됨 | ON |
| LED 2 | CM 미연결 | OFF |
| LED 3 | 아무 버튼 입력 | Oneshot 깜빡 |

### 4. 버튼 진단 기능

*  **BTN 1 클릭:** 통계 리셋 (Min/Max/Avg/Overrun 초기화)
*  **BTN 2 클릭:** 상세 모드 토글 — 추가 센서 데이터(각도, 토크, ADC 전채널)를 1초마다 출력
*  **BTN 3 롱프레스:** 전체 시스템 정보 덤프 (1회성 종합 출력)

---

## 🚀 실행 방법 (How to Use)

1.  `STM32CubeIDE`에서 본 예제 소스파일을 `user_app.c`으로 옮겨와서 빌드하고 펌웨어를 `XM10`에 업로드합니다.
2.  USB를 PC에 연결하고 시리얼 터미널을 엽니다.
3.  1초마다 `[HEALTH]` 접두사로 시작하는 **대시보드 메시지**가 출력되는지 확인합니다.
4.  **LED 1**이 Heartbeat 하고, CM 연결 시 **LED 2**가 켜지는지 확인합니다.
5.  **BTN 2**를 눌러 상세 모드를 켜면 추가 센서 데이터가 출력됩니다.
6.  **BTN 3**을 1초 이상 꾸욱 눌러 시스템 덤프를 확인합니다.
7.  CM을 꺼서 5초 대기하면 `[WATCH] WARNING: STALE DATA` 경고가 출력되고 LED 1이 빠르게 깜빡이는지 확인합니다.

---

## 💡 직접 해보기 (Things to Try)

* **오버런 임계치 변경:** `LOOP_PERIOD_MS`(기본 2)를 1로 줄이면 더 많은 오버런이 감지됩니다. 실제 루프 실행 시간과 주기 설정의 관계를 관찰해보세요.
* **추가 모니터링 항목 추가:** `_PrintHealthDashboard` 함수에 골반 각도, IMU 데이터 등 추가 센서 값을 출력하도록 확장해보세요.
* **경고 조건 커스터마이징:** `STALE_DATA_TIMEOUT_MS`(기본 5000)를 줄이거나, 오버런 연속 발생 시 LED 3도 경고하도록 코드를 수정해보세요.
