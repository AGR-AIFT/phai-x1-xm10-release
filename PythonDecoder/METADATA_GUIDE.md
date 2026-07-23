# XM10 로그 디코더 — 메타데이터 작성 가이드 (사용자용)

> **대상**: XM10 SDK 로 **자신만의 로깅 구조체**를 정의해 USB 로그를 남기고,
> 그 로그를 디코더(실행 파일)로 CSV 로 푸는 사용자.
> **핵심**: 디코더는 로그 안에 필드 정보를 저장하지 않습니다. 대신 세션 폴더의
> `metadata.txt` 로 "이 로그가 어떤 구조체인지"를 알려줘야 합니다.
> **이 정보가 실제 구조체와 어긋나면 CSV 전체가 깨집니다** — 그래서 디코더는
> 크기가 맞지 않으면 **디코딩을 중단하고 오류를 보여줍니다**(자세히는 §7).

---

## 0. 3줄 요약

1. `metadata.txt` 첫 줄에 구조체 필드를 **선언 순서 그대로** `이름(타입)` 으로 나열합니다.
2. 필드 크기 합계가 `System Info` 의 `user_payload_bytes` 와 **정확히 같아야** 합니다.
3. C 구조체는 **`__attribute__((packed))`** 로 선언하는 것이 가장 안전합니다
   (패딩 계산 실수를 원천 차단).

---

## 1. metadata.txt 구조

XM10 FW 가 각 로그 세션 폴더에 자동으로 남기는 텍스트 파일입니다.

```
count(uint32_t), hip_L(float), hip_R(float), gait_phase(uint8_t), _pad(3bytes), h10_loop_count(uint32_t)

=== System Info ===
file_format_version=2
user_payload_bytes=20
record_header_bytes=4
record_total_bytes=24
auto_timestamp=0
timestamp_bytes=0
rtc_start=2026-07-23 10:30:00
logger_period_ms=1
```

- **첫 줄** = 사용자 payload 필드 목록(CSV). 이 줄이 CSV 헤더와 바이트 해석을 결정합니다.
- **빈 줄** 뒤 `=== System Info ===` 부터는 `key=value` 시스템 정보.

---

## 2. 필드 문법

### 2.1 일반 필드 — `이름(타입)`

- **이름**: C 식별자 규칙(`[A-Za-z_][A-Za-z0-9_]*`). 이 이름이 그대로 CSV 열 제목이 됩니다.
- **타입**: 아래 표의 C 타입 중 하나. 구분자는 쉼표 `,` (공백 무시).

| 타입 | 크기(byte) | 부호 | 비고 |
|---|---|---|---|
| `uint8_t` / `int8_t` | 1 | 무/유 | |
| `uint16_t` / `int16_t` | 2 | 무/유 | |
| `uint32_t` / `int32_t` | 4 | 무/유 | |
| `float` | 4 | — | IEEE 754 |
| `double` | 8 | — | IEEE 754 |
| `bool` | 1 | — | |

> 모든 값은 **little-endian** 으로 해석됩니다(STM32 기본).

### 2.2 패딩 필드 — `_이름(Nbytes)`

이름이 **밑줄 `_`** 로 시작하면 패딩으로 간주되어 **N 바이트를 건너뜁니다**(CSV 열 생성 안 함).

```
hip_L(float), _pad(3bytes), gait_phase(uint8_t)
```

### 2.3 지원되지 않는 타입 (주의)

`char[]`, 문자열, enum, union, 가변길이 필드는 **지원되지 않습니다**.
- 표에 없는 타입은 **조용히 건너뜁니다** → 크기 합계가 어긋나 §7 오류로 걸립니다.
  (예: `uint32`(오타), `float32`, `int` 등은 무시됨 — 반드시 `uint32_t`, `float` 처럼 표기)
- enum/상태값은 `uint8_t` 등으로 저장하고 의미는 별도 문서로 남기세요.

---

## 3. ⭐ 가장 흔한 실수 — 정렬(alignment)과 패딩

디코더는 필드를 **빈틈없이 연속(packed)** 으로 읽습니다. 그런데 C 컴파일러는
기본적으로 멤버 사이에 **자동 패딩**을 넣어 정렬을 맞춥니다. 이 둘이 어긋나면
크기는 우연히 맞아도 **값이 한 칸씩 밀려** 전부 깨질 수 있습니다.

**두 가지 안전한 방법 중 하나를 택하세요:**

### 방법 A (권장) — 구조체를 packed 로 선언

```c
typedef struct __attribute__((packed)) {
    uint32_t count;
    float    hip_L;
    float    hip_R;
    uint8_t  gait_phase;
    uint32_t h10_loop_count;
} UserPayload_t;   // 총 17 bytes, 패딩 없음
```
metadata 필드 목록에 `_pad` 를 넣을 필요가 없습니다.
```
count(uint32_t), hip_L(float), hip_R(float), gait_phase(uint8_t), h10_loop_count(uint32_t)
```
→ `user_payload_bytes = sizeof(UserPayload_t) = 17`

### 방법 B — 자연 정렬 구조체 + 명시적 `_pad`

packed 를 쓰지 않으면 컴파일러가 넣는 패딩을 **똑같이** `_pad(Nbytes)` 로 재현해야 합니다.
```c
typedef struct {          // 자연 정렬 (packed 아님)
    uint32_t count;       // offset 0
    float    hip_L;       // 4
    float    hip_R;       // 8
    uint8_t  gait_phase;  // 12
    /* 컴파일러가 여기에 3바이트 패딩 삽입 (uint32_t 4바이트 정렬) */
    uint32_t h10_loop_count; // 16
} UserPayload_t;          // 총 20 bytes
```
```
count(uint32_t), hip_L(float), hip_R(float), gait_phase(uint8_t), _pad(3bytes), h10_loop_count(uint32_t)
```
→ `user_payload_bytes = 20`

> **팁**: FW 에서 `user_payload_bytes` 는 항상 `sizeof(UserPayload_t)` 로 자동 기록하세요.
> 손으로 숫자를 쓰면 어긋납니다.

---

## 4. System Info 필수 키

| 키 | 값 | 필수 | 의미 |
|---|---|---|---|
| `file_format_version` | `1` 또는 `2` | **필수** | 블록 레이아웃(디코더가 이걸로 v1=4100B / v2=4096B 블록을 구분). 누락 시 v1 폴백 → v2 로그면 깨짐. |
| `user_payload_bytes` | 정수 | **필수** | 필드 합계 바이트. 디코더가 필드 목록과 **대조하는 기준**(§7). |
| `record_header_bytes` | 정수 | **필수** | 레코드 헤더 크기(현재 4). |
| `record_total_bytes` | 정수 | **필수** | `record_header_bytes + timestamp_bytes + user_payload_bytes`. |
| `auto_timestamp` | `0`/`1` | **필수** | 레코드마다 4B 타임스탬프 포함 여부. |
| `timestamp_bytes` | `0` 또는 `4` | **필수** | `auto_timestamp=1` 이면 4, 아니면 0. |
| `rtc_start` | `YYYY-MM-DD HH:MM:SS` | 권장 | 세션 시작 RTC(타임스탬프 복원 기준). **FW 빌드 시각(`__DATE__`) 금지**. |
| `logger_period_ms` | 정수 | 권장 | 기록 주기(리포트 기대 rate 계산). |

> 디코더가 모르는 키는 무시됩니다. `fw_git_sha`, `hw_rev`, `session_label` 같은
> 정보를 자유롭게 추가해도 됩니다(개인정보는 넣지 마세요).

---

## 5. `h10_loop_count` (선택 — 동기화 분석 앵커)

디코더는 필드 이름 중 **`h10_loop_count`** (없으면 `count`) 하나만 특별 취급합니다.
이 값의 증가분으로 누락/중복 샘플을 판정합니다.
- H10(외골격)과 연동하는 세션이면 그대로 두세요.
- 자체 구조체라면 **1kHz 등 단조 증가 카운터**를 `h10_loop_count` 또는 `count` 로
  넣으면 품질 리포트(sync/gap)가 의미를 갖습니다. 없어도 디코딩은 됩니다.

---

## 6. 디코딩 방법

1. XM10 을 USB 대용량저장장치(MSC)로 연결 → 세션 폴더(`metadata.txt` + `*.bin`)를 PC 로 복사.
2. 디코더 실행 파일을 켜고 **세션 폴더를 선택**합니다.
3. 결과물이 같은 폴더에 생성됩니다:
   - `decoded_output.csv` — 디코딩된 전체 데이터(열 제목 = 필드 이름)
   - `events.csv` — 이벤트 마커(있는 경우)
   - `data_quality_report.txt` — 누락/중복/동기화 품질 리포트

---

## 7. 오류가 나면 — "필드 정의가 user_payload_bytes 와 일치하지 않습니다"

디코더는 필드 목록 합계와 `user_payload_bytes` 가 다르면 **바로 중단**합니다
(예전엔 경고만 하고 깨진 CSV 를 만들었지만, 이제는 잘못된 데이터를 신뢰하지
않도록 막습니다). 메시지 예:

```
metadata.txt 필드 정의가 user_payload_bytes 와 일치하지 않습니다.
    - 필드 리스트 합계 : 17 bytes (5 fields, struct='<IffBI')
    - user_payload_bytes: 20 bytes  (차이 -3)
```

**차이(±N)를 보고 원인을 좁히세요:**

| 증상 | 원인 | 해결 |
|---|---|---|
| 합계가 **작다**(−N) | 필드 누락, 또는 **타입 오타로 필드가 무시됨** | 타입 철자 확인(`uint32_t` 등), 빠진 필드 추가 |
| 합계가 **작다**(정확히 정렬 패딩 크기만큼) | packed 아닌 구조체인데 `_pad` 누락 | `_pad(Nbytes)` 추가 또는 구조체를 `packed` 로 |
| 합계가 **크다**(+N) | 필드를 더 넣었거나 `_pad` 과다 | 실제 구조체와 목록 대조 |

> **주의**: 크기 합계가 맞아도 **패딩 위치가 틀리면** 값이 밀립니다.
> 필드는 항상 **C 구조체 선언 순서 그대로** 나열하고, packed 사용을 권장합니다(§3).

---

## 8. 체크리스트 (로그 남기기 전)

- [ ] 필드 목록이 C 구조체 **멤버 선언 순서와 동일**
- [ ] 모든 타입이 §2.1 표의 9종 중 하나 (오타 없음)
- [ ] 구조체가 `packed` 이거나, 아니면 정렬 패딩을 `_pad(Nbytes)` 로 정확히 재현
- [ ] `user_payload_bytes = sizeof(구조체)` 로 **자동** 기록(손으로 쓰지 않기)
- [ ] `file_format_version` 이 실제 FW 로그 포맷과 일치
- [ ] `auto_timestamp` / `timestamp_bytes` 가 서로 일관
- [ ] `rtc_start` 는 세션 시작 시각(빌드 시각 아님)

---

## 부록. 최소 FW 예제 (metadata.txt 자동 생성)

```c
static void write_metadata_txt(FILE* f) {
    /* 1행: 필드 목록 — 구조체 선언 순서와 동일 */
    fprintf(f,
        "count(uint32_t), hip_L(float), hip_R(float), "
        "gait_phase(uint8_t), h10_loop_count(uint32_t)\n");

    fprintf(f, "\n=== System Info ===\n");
    fprintf(f, "file_format_version=%d\n", LOG_FILE_FORMAT_VERSION);
    fprintf(f, "user_payload_bytes=%u\n",  (unsigned)sizeof(UserPayload_t)); /* 자동 */
    fprintf(f, "record_header_bytes=%u\n", (unsigned)sizeof(LogPacketHeader_t));
    fprintf(f, "record_total_bytes=%u\n",  (unsigned)sizeof(LogRecord_t));
    fprintf(f, "auto_timestamp=%d\n", auto_ts ? 1 : 0);
    fprintf(f, "timestamp_bytes=%d\n", auto_ts ? 4 : 0);
    fprintf(f, "logger_period_ms=%u\n", period_ms);
    fprintf(f, "rtc_start=%04d-%02d-%02d %02d:%02d:%02d\n",
            y, mon, d, h, m, s);          /* RTC 스냅샷 (빌드 시각 금지) */
    fprintf(f, "fw_git_sha=%s\n", GIT_SHA_SHORT);   /* 디코더 무시, 추적용 */
}
```

구조체 정의·metadata·CSV 헤더를 **한 곳(X-macro)** 에서 선언해 순서 어긋남을
컴파일 타임에 막는 패턴을 쓰면 위 실수를 원천 차단할 수 있습니다(선택).
