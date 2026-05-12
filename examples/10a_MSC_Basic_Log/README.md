# Ex.10a — MSC Basic Log (USB 메모리 자동 로깅 — 최소 구조)

> 🎯 **학습 목표**:
> - 가장 단순한 USB MSC 로깅: 단일 float 변수 + 버튼 Start/Stop.
> - 자동 세션 이름 (sessionName=`NULL` → `B%03lu_%03lu` 자동 생성) 활용.
>
> ⏱️ 권장 시간: 25분 | 🔧 난이도: ⭐⭐
> 🧰 사전 예제: [Ex.02 Button Event](../02_Button_LED_Event/) | 📚 관련 docs: [USB Connectivity](../../docs/api-reference/05-usb-connectivity.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

USB 메모리에 **좌측 고관절 각도 (float)** 를 1 ms 주기로 자동 저장:

| BTN | 동작 |
|:---:|------|
| BTN 1 | 로깅 시작 → 자동 세션 `B%03lu_%03lu` (boot count + session number) 생성 |
| BTN 2 | 로깅 정지 → 파일 닫고 summary.txt 자동 생성 |

저장 경로 예: `/LOGS/B001_000/`
- `metadata.txt` — 데이터 설명
- `summary.txt` — 통계 (Stop 시)
- `data_000_part_000.bin` — 12 B/record × N records

> 📸 `![USB 메모리 자동 마운트](../assets/img/10a_usb_log.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **자동 세션 이름** — `XM_StartUsbDataLog(NULL, "...")` 호출 시 System 이 `B<부팅카운트>_<세션번호>` 자동 생성. 학생이 이름 안 정해도 OK.
- **자동 타임스탬프** — 기본 ON. 매 레코드 앞에 `[Header:4][tick_ms:4]` 8 B 헤더 + User payload 가 붙음.
- **레코드 크기 계산** — 본 예제: `[Header:4][tick:4][leftHipAngle:4]` = 12 B. 1 kHz → 12 KB/s → 1 분 ≈ 720 KB.
- **`XM_IsUsbLogReady()`** — USB 가 마운트되어 쓰기 가능한지. 사용자 코드의 첫 가드.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
typedef struct {
    float leftHipAngle;
} BasicLog_t;                                                         // ① 단일 필드

static BasicLog_t s_log;
static bool s_is_logging = false;                                     // ② 상태 플래그

void User_Setup(void)
{
    XM_SetUsbLogSource(&s_log, sizeof(BasicLog_t));                    // ③ 소스 등록 (1회)
}

void User_Loop(void)
{
    /* BTN 1: 로깅 시작 */
    if (!s_is_logging && XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        if (XM_IsUsbLogReady()) {                                      // ④ USB 준비 확인
            s_is_logging = XM_StartUsbDataLog(                          // ⑤ 시작 (자동 이름)
                NULL,
                "leftHipAngle(float)");
            if (s_is_logging) XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 500);
        }
    }

    /* BTN 2: 정지 */
    if (s_is_logging && XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        XM_StopUsbDataLog();                                            // ⑥ 정지 + 파일 닫기
        s_is_logging = false;
        XM_SetLedEffect(XM_LED_1, XM_LED_SOLID, 0);
    }

    /* 데이터 갱신 (System 이 1 ms 마다 자동 저장) */
    if (s_is_logging) {
        s_log.leftHipAngle = XM.status.h10.leftHipAngle;                // ⑦ 갱신만
    }
}
```

전체 코드: [`msc_basic_log.c`](msc_basic_log.c)

> 🧒 학생이 `fwrite()` 같은 파일 입출력 직접 안 짜도 됨 — System 이 ③ 등록만 받으면 자동 저장.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **FAT32 32GB USB** 를 XM10 에 삽입.
2. **빌드 + 플래시** → ✅ `0 errors`
3. **BTN 1** → ✅ LED 1 깜빡 (로깅 중)
4. **5초 대기** → 데이터 누적
5. **BTN 2** → ✅ LED 1 SOLID (정지)
6. **USB 빼서 PC** → ✅ `/LOGS/B001_000/` 에 metadata.txt + summary.txt + data*.bin
7. **Python 디코더로 CSV 변환** + Matplotlib 그래프
8. **변형 1 — 다중 필드**: 구조체에 `rightHipAngle` 추가. metadata 도 갱신. → [Ex.10b](../10b_MSC_Custom_Struct/) 패턴.
9. **변형 2 — 이름 직접 지정**: `XM_StartUsbDataLog("MyGait_v1", "...")` → 폴더 이름 커스터마이즈.
10. **변형 3 — rolling**: `XM_SetUsbLogRollingSize(5)` Setup 에 추가 → 5 MB 파일 분할.

---

## 5️⃣ 다음 단계

- 다양한 타입 + 수동 타임스탬프: [Ex.10b MSC Custom Struct](../10b_MSC_Custom_Struct/)
- TSM + 에러 모니터링: [Ex.10c MSC Advanced Log](../10c_MSC_Advanced_Log/)
- 실시간 PC 스트리밍과 결합: [Ex.09 CDC Stream](../09_CDC_Stream/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| `XM_IsUsbLogReady()` 항상 false | USB 미삽입 또는 exFAT (지원 X) | **FAT32** 포맷, cluster 32 KB 권장 |
| 폴더는 생기는데 .bin 파일 없음 | `XM_SetUsbLogSource` 누락 | Setup 에서 호출 확인 |
| .bin 파일이 0 byte | Source 등록 후 로깅 시작 전 종료 | 시작 + 데이터 갱신 + 정지 순서 |
| Stop 안 하고 USB 분리 → 파일 손상 | 마지막 buffer flush 안 됨 | 반드시 BTN 2 (또는 `XM_StopUsbDataLog`) 호출 후 분리 |
| 짧은 세션인데 큰 USB 잔여 변화 | metadata + alignment 오버헤드 | 정상 (수십 KB 소비) |
| 디코더가 파일 못 읽음 | metadata.txt 의 포맷 문자열과 .bin 의 실제 구조 불일치 | `XM_StartUsbDataLog` 의 metadata string 확인 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
