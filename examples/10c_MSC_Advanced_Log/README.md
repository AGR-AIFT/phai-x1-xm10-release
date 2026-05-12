# Ex.10c — MSC Advanced Log (TSM + 에러 모니터링 + 파일 롤링)

> 🎯 **학습 목표**:
> - TSM (STANDBY/ACTIVE) 와 로깅을 결합한 **제품 수준** 패턴.
> - `XM_GetUsbLogStatus()` 로 실시간 에러 모니터링 (WARNING_QUEUE_FULL / WARNING_DISK_LOW / ERROR_STOPPED).
> - 파일 롤링 (`XM_SetUsbLogRollingSize`) + 세션 카운터 자동 증가.
> - 세션 마커 (`XM_InsertUsbLogMarker`) 로 데이터 후처리 시 분할점 표시.
>
> ⏱️ 권장 시간: 40분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.10b Custom Struct](../10b_MSC_Custom_Struct/) + [Ex.03 FSM](../03_Button_LED_FSM/) | 📚 관련 docs: [USB Connectivity](../../docs/api-reference/05-usb-connectivity.md) · [TSM](../../docs/api-reference/01-task-state-machine.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

본격 제품 수준 로깅 시스템:

```
STANDBY ──(BTN1)──▶ ACTIVE ──(BTN2)──▶ STANDBY
                       │                  ▲
                       └─(ERROR 감지)─────┘ (자동 복귀)
```

### LED 상태 표시 (실시간 진단)

| LED | 상태 | 의미 |
|-----|------|------|
| LED1 BLINK | ACTIVE | 로깅 중 |
| LED2 BLINK | WARNING | 버퍼 90%+ (쓰기 지연) |
| LED2 SOLID | WARNING | 디스크 잔여 50 MB 미만 |
| LED3 SOLID | ERROR | 로깅 중단 (자동 복귀) |

저장: `Gait_000`, `Gait_001`, ... (세션 카운터 자동 증가). 6 필드 × float = 24 B + Header = 28 B/record. 파일 롤링 5 MB.

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **`XM_GetUsbLogStatus()`** 반환: `IDLE / LOGGING / WARNING_QUEUE_FULL / WARNING_DISK_LOW / ERROR_STOPPED` → 학생 코드에서 매 cycle 모니터링.
- **`XM_SetUsbLogRollingSize(MB)`** — 한 파일 최대 크기. 본 예제는 5 MB. 큰 데이터 세션을 여러 파일로 분할.
- **`XM_InsertUsbLogMarker(type, value)`** — 데이터 흐름 중간에 마커 삽입. 후처리 시 세션 시작/정지/이벤트 분할 기준.
- **`XM_GetUsbDiskFreeMB()` / `TotalMB()`** — 디스크 잔여/총량 모니터링.
- **자동 복구** — ERROR_STOPPED 발생 시 ACTIVE → STANDBY 로 강제 전환 → 다음 BTN1 으로 재시작.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
typedef struct {
    float hip_L, hip_R, knee_L, knee_R, torque_L, torque_R;
} GaitLog_t;  /* 24 bytes */

static GaitLog_t s_log;
static uint32_t s_session_counter = 0;
static char s_session_name[32];

void User_Setup(void)
{
    /* TSM STANDBY/ACTIVE 등록 생략 */
    XM_SetUsbLogSource(&s_log, sizeof(GaitLog_t));
    XM_SetUsbLogRollingSize(5);                                         // ① 5 MB 분할
}

static void Active_Entry(void)
{
    snprintf(s_session_name, sizeof(s_session_name),
             "Gait_%03lu", (unsigned long)s_session_counter++);          // ② 자동 카운터

    bool ok = XM_StartUsbDataLog(s_session_name,
        "hip_L(float), hip_R(float), knee_L(float), knee_R(float), "
        "torque_L(float), torque_R(float)");

    if (ok) {
        XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 500);
        XM_InsertUsbLogMarker(1, (uint16_t)s_session_counter);           // ③ 시작 마커
    } else {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);                    // 시작 실패
    }
}

static void Active_Loop(void)
{
    /* 데이터 갱신 */
    s_log.hip_L    = XM.status.h10.leftHipAngle;
    s_log.hip_R    = XM.status.h10.rightHipAngle;
    s_log.knee_L   = XM.status.h10.leftKneeAngle;
    s_log.knee_R   = XM.status.h10.rightKneeAngle;
    s_log.torque_L = XM.command.assist_torque_lh;
    s_log.torque_R = XM.command.assist_torque_rh;

    /* ④ 실시간 상태 모니터링 */
    switch (XM_GetUsbLogStatus()) {
    case XM_LOG_STATUS_WARNING_QUEUE_FULL:
        XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, 200);  break;
    case XM_LOG_STATUS_WARNING_DISK_LOW:
        XM_SetLedEffect(XM_LED_2, XM_LED_SOLID, 0);     break;
    case XM_LOG_STATUS_ERROR_STOPPED:
        XM_SetLedEffect(XM_LED_3, XM_LED_SOLID, 0);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);                    // 자동 복귀
        return;
    default:
        XM_SetLedEffect(XM_LED_2, XM_LED_SOLID, 0);     break;
    }

    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        XM_InsertUsbLogMarker(2, 0);                                     // ⑤ 정지 마커
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}
```

전체 코드: [`msc_advanced_log.c`](msc_advanced_log.c)

> 🧒 핵심: ④ 의 `switch` 가 매 cycle 상태 점검. WARNING 은 LED 만, ERROR 는 자동 복귀.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **FAT32 USB + 빌드 + 플래시**
2. **BTN 1** → ✅ `/LOGS/Gait_000/` 생성, LED 1 깜빡
3. **30초 데이터 누적** → 5 MB 마다 새 파일 (`data_000_part_001.bin`, `_part_002.bin` ...)
4. **BTN 2** → ✅ 정지 + 마커 type=2 삽입
5. **다시 BTN 1** → ✅ `/LOGS/Gait_001/` 자동 생성 (카운터 증가)
6. **변형 1 — WARNING 강제 발생**: rolling size 를 `1` MB 로 → 짧은 시간 내 다음 분할 → 디스크 잔여 가까워질 때 LED 2 SOLID 관찰.
7. **변형 2 — Disk Low 시뮬레이션**: USB 를 거의 가득 채운 상태로 시작 → `WARNING_DISK_LOW` 트리거.
8. **변형 3 — 외부 트리거**: Ex.06 의 외부 안전 스위치로 자동 정지 + 마커 삽입.

---

## 5️⃣ 다음 단계

- 외부 IO + 안전 스위치 + 로깅 결합: [Ex.06 Safety](../06_Ext_IO_Safety_Switch/) + 본 예제
- 실시간 PC 스트리밍 + 보드 저장 동시: [Ex.09 CDC Stream](../09_CDC_Stream/) + [본 예제]
- 보행 분석 데이터 파이프라인 응용: [Ex.34 MSC Gait Analysis Log](../34_MSC_GaitAnalysis_Log/) (있는 경우)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| ERROR 발생 후 다시 시작 안 됨 | 자동 복귀 후 ERROR LED 가 그대로 → 학생이 STANDBY 인 줄 모름 | `Active_Exit` 에서 LED 모두 OFF 처리 또는 manual reset |
| WARNING_QUEUE_FULL 자주 발생 | USB 쓰기 속도 < 데이터 생성 속도 | rolling size ↓ 또는 데이터 크기 ↓ |
| 세션 카운터가 부팅마다 0 으로 초기화 | `s_session_counter` 가 RAM 변수 | 의도된 동작. 영구 카운터 필요 시 NV 사용 |
| 마커가 디코더에서 안 보임 | 디코더가 marker 지원 안 함 | PythonDecoder 최신 버전 사용 |
| 5MB 파일이 안 정확히 5MB | flush 시점 + 마지막 record 정렬 | 정상. 약간 ± 가능 |
| Stop 시 summary.txt 가 비어있음 | session 통계 미수집 (짧은 세션) | 충분히 길게 로깅 후 검증 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
