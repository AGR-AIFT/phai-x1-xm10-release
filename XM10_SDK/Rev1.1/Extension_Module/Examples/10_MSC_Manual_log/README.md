# Ex.10 — MSC Manual Log (레거시 — 신규 학습은 10a/10b/10c)

> 🎯 **학습 목표**:
> - TSM (STANDBY/ACTIVE) 와 USB MSC 로깅을 결합한 **레거시 패턴** 을 이해합니다.
> - 본 예제는 호환을 위해 유지됩니다. **신규 학습은 [10a](../10a_MSC_Basic_Log/) → [10b](../10b_MSC_Custom_Struct/) → [10c](../10c_MSC_Advanced_Log/)** 순서를 권장합니다.
>
> ⏱️ 권장 시간: 25분 | 🔧 난이도: ⭐⭐ (레거시)
> 🧰 사전 예제: [Ex.03 FSM](../03_Button_LED_FSM/) | 📚 관련 docs: [USB Connectivity](../../docs/api-reference/05-usb-connectivity.md)

---

## ⚠️ 본 예제는 레거시입니다

신규 학생은 다음 순서 권장:

| 예제 | 학습 핵심 |
|------|----------|
| **10a Basic Log** | 최소 구조체 + 버튼 Start/Stop (가장 단순) |
| **10b Custom Struct** | 다양한 타입 혼합 + 수동 타임스탬프 |
| **10c Advanced Log** | TSM + 에러 모니터링 + 파일 롤링 |

본 예제 (10) 는 **TSM + 자동 타임스탬프** 의 옛 패턴을 보존한 것입니다.

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

| BTN | 동작 |
|:---:|------|
| BTN 1 | 녹화 시작 → `/LOGS/TestRun_001/` 폴더 생성 + ACTIVE 진입, LED 1 깜빡 |
| BTN 2 (ACTIVE 중) | 녹화 종료 → STANDBY 복귀, 파일 닫기 |

저장 내용: `cmd_torque` (float) + `res_angle` (float) per record. tick_ms 는 System 이 자동 삽입.

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **USB MSC (Mass Storage Class)** — USB 메모리 (Flash Drive) 를 보드가 호스트로 마운트해 파일 저장.
- **자동 타임스탬프** — `XM_SetUsbLogAutoTimestamp(true)` (기본). 매 레코드 앞에 4 바이트 `tick_ms` 자동 삽입. → User payload 는 본인 데이터만 정의.
- **`XM_SetUsbLogSource(ptr, size)`** — 매 1 ms 마다 System 이 ptr 에서 size 바이트를 읽어 자동 저장. Setup 1회.
- **`XM_StartUsbDataLog(sessionName, metadata)`** — 폴더 생성 + metadata.txt 생성 + 로깅 시작.
- **`XM_IsUsbLogReady()`** — USB 메모리가 마운트되어 쓰기 가능한지 확인.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
typedef struct {
    float cmd_torque;
    float res_angle;
} MiniLog_t;                                                          // ① User payload

static MiniLog_t myLog;

void User_Setup(void)
{
    /* TSM (STANDBY/ACTIVE) 등록 생략 */
    XM_SetUsbLogSource(&myLog, sizeof(MiniLog_t));                      // ② 데이터 소스 등록
    // XM_SetUsbLogAutoTimestamp(false);  // (옵션) 직접 관리할 때만
    // XM_SetUsbLogRollingSize(20);        // (옵션) 기본 10MB → 20MB
}

static void Standby_loop(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        if (XM_IsUsbLogReady()) {                                       // ③ 마운트 확인
            bool ok = XM_StartUsbDataLog(                                // ④ 폴더 + metadata
                "TestRun_001",
                "command_torque(float), result_angle(float)");
            if (ok) XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
            else    XM_SetLedEffect(XM_LED_2, XM_LED_HEARTBEAT, 200);    // 실패 표시
        } else {
            XM_SetLedEffect(XM_LED_3, XM_LED_HEARTBEAT, 200);            // USB 없음
        }
    }
}

static void Active_Loop(void)
{
    myLog.cmd_torque = XM.command.assist_torque_rh;                     // ⑤ User payload 갱신
    myLog.res_angle  = XM.status.h10.rightHipAngle;
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

static void Active_Exit(void)
{
    if (XM_GetUsbLogStatus() == XM_LOG_STATUS_LOGGING) {                // ⑥ 안전 종료
        XM_StopUsbDataLog();
    }
}
```

전체 코드: [`msc_manual_log.c`](msc_manual_log.c)

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **HW**: 32 GB FAT32 포맷 USB 메모리 (Sandisk Ultra Dual Drive 권장) 를 XM10 USB-C 에 삽입.
2. **빌드 + 플래시** → ✅ `0 errors`
3. **BTN 1 누름** → ✅ LED 1 깜빡 (녹화 중), USB 메모리에 `/LOGS/TestRun_001/` 생성
4. **잠시 대기** (5초 정도) → 데이터 누적
5. **BTN 2 누름** → ✅ LED 1 OFF, 파일 닫힘
6. **USB 분리 + PC 삽입** → ✅ `metadata.txt` + `data_000_part_000.bin` 확인
7. **Python 디코더로 CSV 변환**: `python PythonDecoder/MSC/data_decoder_xm10.py /path/to/USB/LOGS/TestRun_001/`
8. **변형 1**: rolling size 변경 (`XM_SetUsbLogRollingSize(5)`) → 5MB 마다 파일 분할.

---

## 5️⃣ 다음 단계 (강력 추천)

레거시 대신 **신규 시리즈로 이동**:

- [Ex.10a MSC Basic Log](../10a_MSC_Basic_Log/) — 최소 구조체, 가장 단순
- [Ex.10b MSC Custom Struct](../10b_MSC_Custom_Struct/) — 다양한 타입 + 수동 타임스탬프
- [Ex.10c MSC Advanced Log](../10c_MSC_Advanced_Log/) — TSM + 에러 모니터링

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| BTN 1 눌렀는데 LED 3 깜빡 (USB 없음) | USB 미삽입 또는 FAT32 아님 | FAT32 포맷 (cluster 32 KB 권장) USB 사용 |
| 폴더 생성됐는데 데이터 0 byte | `XM_SetUsbLogSource` 누락 | Setup 에서 호출 확인 |
| 파일 이름이 영문이 아님 | sessionName 한글 사용 | 영문/숫자/언더스코어만 |
| 너무 큰 파일 1개로 누적 | 기본 rolling 10 MB 의도된 동작 | `XM_SetUsbLogRollingSize(5)` 로 5 MB 마다 분할 |
| 종료 안 했는데 USB 빼서 파일 손상 | `Active_Exit` 호출 안 됨 (강제 종료) | 반드시 BTN 2 로 정상 종료 후 USB 분리 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
