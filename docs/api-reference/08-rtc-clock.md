# API 08: RTC (Real-Time Clock)

> 📌 **이 페이지를 읽고 나면**: 외장 RTC (MCP79510) 로 날짜/시간을 읽고 쓸 수 있고, 배터리 방전을 감지할 수 있습니다.
> ⏱️ 예상 학습 시간: 10분
> 🧰 사전 지식: 없음 (독립 API)
> 🎯 핵심 함수: `XM_RTC_SetDateTime` / `XM_RTC_GetDateTime` / `XM_RTC_IsRunning`

> **헤더 파일**: `xm_api_rtc.h`

MCP79510 RTC를 통한 날짜/시간 관리 API입니다. 내부적으로 2-digit year(0~99)를 4-digit year(2000~2099)로 변환합니다.

---

## 타입

### `XmDateTime_t`

```c
typedef struct {
    uint16_t year;      // 연도 (2000~2099)
    uint8_t  month;     // 월 (1~12)
    uint8_t  day;       // 일 (1~31)
    uint8_t  weekday;   // 요일 (1=Mon ~ 7=Sun)
    uint8_t  hour;      // 시 (0~23)
    uint8_t  minute;    // 분 (0~59)
    uint8_t  second;    // 초 (0~59)
} XmDateTime_t;
```

---

## API 함수

### `XM_RTC_SetDateTime()`

```c
bool XM_RTC_SetDateTime(const XmDateTime_t* dt);
```

RTC에 날짜/시간을 설정합니다.

| 파라미터 | 설명 |
|----------|------|
| `dt` | 설정할 날짜/시간 (year: 2000~2099) |

| 반환값 | 의미 |
|--------|------|
| `true` | 성공 |
| `false` | 실패 (범위 초과 또는 SPI 에러) |

### `XM_RTC_GetDateTime()`

```c
bool XM_RTC_GetDateTime(XmDateTime_t* dt);
```

RTC에서 현재 날짜/시간을 읽습니다.

| 파라미터 | 설명 |
|----------|------|
| `dt` | 읽은 날짜/시간을 저장할 구조체 포인터 |

| 반환값 | 의미 |
|--------|------|
| `true` | 성공 |
| `false` | 실패 (SPI 에러) |

### `XM_RTC_IsRunning()`

```c
bool XM_RTC_IsRunning(void);
```

RTC 오실레이터가 동작 중인지 확인합니다.

| 반환값 | 의미 |
|--------|------|
| `true` | 동작 중 |
| `false` | 정지 (배터리 방전 등) |

---

## 사용 예시

```c
void User_Setup(void) {
    // RTC 동작 확인 및 시간 설정
    if (!XM_RTC_IsRunning()) {
        XmDateTime_t dt = {
            .year = 2026, .month = 4, .day = 3,
            .weekday = 5,  // 금요일
            .hour = 14, .minute = 30, .second = 0
        };
        XM_RTC_SetDateTime(&dt);
    }

    // 현재 시간 읽기
    XmDateTime_t now;
    if (XM_RTC_GetDateTime(&now)) {
        // now.year, now.month, now.day, now.hour, now.minute, now.second 사용
    }
}
```

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| `XM_RTC_GetDateTime` 가 항상 false | SPI 통신 실패 (배선 불량 또는 CS 누락) | 하드웨어 점검 + `XM_RTC_IsRunning()` 으로 동작 확인 |
| `XM_RTC_IsRunning` 가 false | RTC 백업 배터리 방전 (코인셀 CR1220) | 배터리 교체 + `XM_RTC_SetDateTime` 으로 시간 재설정 |
| `year = 1970` 또는 2000 | 첫 전원 인가 후 시간 설정 안 함 | `User_Setup` 에서 `IsRunning` 체크 후 `SetDateTime` |
| `year = 2100` 등 범위 초과 | `SetDateTime` 에 잘못된 값 전달 | `year` 는 2000~2099 (MCP79510 2-digit) |
| 매 cycle `GetDateTime` 호출하니 느려짐 | SPI 통신은 ms 단위 비용 | 1초마다 한 번만 호출 (timestamp 캐싱) |
| Weekday 값이 안 맞음 | 1=월요일 ~ 7=일요일 규칙 미숙지 | 표준 ISO 8601 `1=Mon ... 7=Sun` 준수 |
| 시간 설정 후 전원 OFF/ON 시 리셋 | 백업 배터리 미장착 또는 방전 | 배터리 점검 — 약 5년 수명 |
| `weekday` 자동 계산 안 됨 | MCP79510 은 weekday 도 사용자 입력 필수 | Zeller 공식 또는 `<time.h>` 로 사전 계산 |

---

## 관련 예제

| 예제 | 활용 |
|------|------|
| [10_MSC_Manual_log](../../examples/10_MSC_Manual_log/) | 로그 파일명에 timestamp 사용 (`/LOGS/20260512_143000.bin`) |
| [10c_MSC_Advanced_Log](../../examples/10c_MSC_Advanced_Log/) | summary.txt 에 시작/종료 시각 기록 |
