# 예제 10c: 상태 머신(TSM) + 에러 모니터링 + 파일 롤링 (MSC Advanced Log)

본 예제는 실제 제품 수준의 데이터 로깅 시스템을 구현하는 **고급 패턴**을 다룹니다. 상태 머신(TSM)으로 로깅 흐름을 제어하고, 실시간 에러 모니터링과 자동 복구를 포함합니다.

## 🎯 학습 목표 (Objective)

* **상태 머신(TSM)**과 데이터 로깅을 연동하는 패턴을 학습합니다.
* `XM_GetUsbLogStatus()`로 로깅 상태(WARNING, ERROR)를 실시간 모니터링합니다.
* **LED 피드백**으로 사용자에게 시스템 상태를 시각적으로 전달합니다.
* `XM_SetUsbLogRollingSize()`로 **파일 롤링 크기**를 런타임에 설정합니다.
* 에러 발생 시 상태 머신을 통한 **자동 복귀** 전략을 이해합니다.

## ⚙️ 동작 원리 (How it Works)

* **상태 전이:**
  ```
  STANDBY ──(BTN1)──▶ ACTIVE ──(BTN2)──▶ STANDBY
                        │                    ▲
                        └──(ERROR 감지)──────┘
  ```
* **Entry/Exit 패턴:** `Active_Entry()`에서 세션을 시작하고, `Active_Exit()`에서 정리합니다. 에러로 인한 자동 전이 시에도 `on_exit`이 호출되어 안전하게 종료됩니다.
* **파일 롤링:** `XM_SetUsbLogRollingSize(5)`로 5MB마다 새 파일로 분할합니다. 장시간 로깅 시 하나의 파일이 지나치게 커지는 것을 방지합니다.
* **세션 카운터:** `Gait_000`, `Gait_001`, ... 형식으로 매 세션마다 고유한 폴더명을 자동 생성합니다.

## 📊 LED 피드백 표

| LED   | 상태     | 의미 |
|-------|----------|------|
| LED1 OFF    | STANDBY  | 대기 중 |
| LED1 BLINK  | ACTIVE   | 로깅 진행 중 |
| LED2 BLINK  | WARNING  | 버퍼 90%+ 또는 쓰기 지연 발생 |
| LED3 SOLID  | ERROR    | 로깅 중단, STANDBY로 자동 복귀 |
| LED3 HEARTBEAT | -    | USB 메모리 미준비 |

## 🚀 실행 방법 (How to Use)

1.  USB 메모리를 연결하고 코드를 업로드합니다.
2.  **BTN1 클릭:** 로깅 시작 → LED1 깜빡임
3.  로깅 중 LED2가 깜빡이면 쓰기 속도가 느려지고 있다는 경고입니다.
4.  **BTN2 클릭:** 로깅 정지 → 모든 LED 꺼짐
5.  에러 발생 시 LED3이 켜지고 자동으로 STANDBY 상태로 복귀합니다.
6.  USB 메모리에서 `/LOGS/Gait_000/` 폴더를 확인합니다.
7.  **Python 디코더로 CSV 변환:**
    ```bash
    python data_decoder_xm10.py /LOGS/Gait_000
    ```

## 💡 직접 해보기 (Things to Try)

* `XM_SetUsbLogRollingSize(1)`로 설정하여 1MB마다 파일이 분할되는 것을 확인해보세요.
* 로깅 중 USB 메모리를 의도적으로 뽑아 에러 복구 동작을 테스트해보세요.
* `DataLogger_GetStats()`를 통해 `dropped_packets`와 `write_errors` 값을 모니터링하는 코드를 추가해보세요.
* 실제 보행 테스트에서 10분 이상 로깅 후 `summary.txt`의 통계를 분석해보세요.
