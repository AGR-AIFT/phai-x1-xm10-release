# `xm_api_rtc.h` — 실시간 시계(RTC) API

> **대상 헤더**: `XM_FW/XM_API/xm_api_rtc.h`
> **관련 개념 문서**: [08. 실시간 시계](../08-rtc-clock.md) · [06. USB 메모리 로깅](../06-usb-data-logging.md) (RTC 타임스탬프 연동)
> **관련 예제**: [34_MSC_GaitAnalysis_Log](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/34_MSC_GaitAnalysis_Log/) — RTC 로 세션 폴더명을 만들고, RTC 가 없거나 미설정이면 tick 기반 이름으로 자동 대체하는 실제 코드

> 🟢 **Rev 2.0 전용 (하드웨어 종속)** — RTC 칩(MCP79510)은 XM10 **Rev2.0** 보드에만 실장되어 있습니다. `xm_api_rtc.h` 헤더 자체는 Rev1.1 / Rev2.0 SDK 양쪽에 완전히 동일하게 포함되어 있지만, **Rev1.1 보드에서는 아래 함수 3개가 모두 stub 으로 동작**합니다 — 항상 `false` 를 반환하고 실제로는 아무 것도 하지 않습니다. Rev1.1 에서 경과 시간이 필요하면 `XM_GetTick()` 기반 타이머로 대체하세요.

---

## 언제 사용하나

로그 파일이나 세션 폴더 이름에 실제 날짜/시간을 남기고 싶을 때, 또는 USB 메모리 로깅으로 저장되는 파일의 타임스탬프를 정확히 찍고 싶을 때 사용합니다. RTC 개념 자체와 흔한 실수(배터리 방전, weekday 미계산 등)는 [08. 실시간 시계](../08-rtc-clock.md) 문서에서 먼저 확인하세요. 본 페이지는 `xm_api_rtc.h` 에 선언된 함수와 타입의 세부 명세만 다룹니다.

RTC 를 설정하지 않은 채로 USB 메모리 로깅을 시작하면 파일 타임스탬프가 기본값(2025-01-01)으로 찍히므로, `Control_Setup()` 에서 한 번 `XM_RTC_IsRunning()` 으로 확인하고 필요하면 `XM_RTC_SetDateTime()` 으로 설정하는 것이 표준 패턴입니다 (자세한 내용은 [06. USB 메모리 로깅 — 주의사항](../06-usb-data-logging.md) 참고).

## 함수 목록

| 함수 | 설명 |
|------|------|
| [`XM_RTC_SetDateTime`](#xm_rtc_setdatetime) | RTC 에 날짜/시간을 설정합니다 |
| [`XM_RTC_GetDateTime`](#xm_rtc_getdatetime) | RTC 에서 현재 날짜/시간을 읽습니다 |
| [`XM_RTC_IsRunning`](#xm_rtc_isrunning) | RTC 오실레이터가 동작 중인지 확인합니다 |

## 함수 상세

### `XM_RTC_SetDateTime`

```c
bool XM_RTC_SetDateTime(const XmDateTime_t* dt);
```

RTC 에 날짜/시간을 설정합니다. 내부적으로 4자리 연도(2000~2099)를 MCP79510 이 실제로 저장하는 2자리 연도로 변환해서 씁니다.

| 파라미터 | 타입 | 설명 |
|----------|------|------|
| `dt` | `const XmDateTime_t*` | 설정할 날짜/시간. `year` 는 반드시 2000~2099 범위 |

**반환값**

| 값 | 의미 |
|----|------|
| `true` | 설정 성공 |
| `false` | 실패 — `year` 범위 초과 또는 SPI 통신 에러. **Rev1.1 에서는 항상 `false`** |

> ⚠️ **호출 컨텍스트**: `Control_Setup()`/`Control_Loop()` 컨텍스트 기준입니다. SPI 로 RTC 칩과 통신하므로 ISR 안에서 호출하지 마세요. 보통 부팅 직후 `Control_Setup()` 에서 [`XM_RTC_IsRunning()`](#xm_rtc_isrunning) 으로 배터리 방전 여부를 먼저 확인한 뒤, 필요할 때만 한 번 호출하는 패턴을 씁니다.

**예제**

```c
XmDateTime_t dt = {
    .year = 2026, .month = 3, .day = 2,
    .weekday = 1,  // 월요일 (1=Mon ~ 7=Sun)
    .hour = 14, .minute = 30, .second = 0
};
XM_RTC_SetDateTime(&dt);
```

**참고**: [`XM_RTC_IsRunning`](#xm_rtc_isrunning) · [08. 실시간 시계](../08-rtc-clock.md)

---

### `XM_RTC_GetDateTime`

```c
bool XM_RTC_GetDateTime(XmDateTime_t* dt);
```

RTC 에서 현재 날짜/시간을 읽어 `dt` 에 채웁니다.

| 파라미터 | 타입 | 설명 |
|----------|------|------|
| `dt` | `XmDateTime_t*` | 읽은 날짜/시간을 담을 출력 버퍼 |

**반환값**

| 값 | 의미 |
|----|------|
| `true` | 읽기 성공 |
| `false` | 실패 — SPI 통신 에러. **Rev1.1 에서는 항상 `false`, `dt` 는 갱신되지 않음** |

> ⚠️ **호출 컨텍스트**: `Control_Setup()`/`Control_Loop()` 컨텍스트 기준입니다. SPI 통신에는 ms 단위 비용이 있으므로, 1 kHz `Control_Loop()` 매 사이클마다 부르지 말고 로그 파일명 생성처럼 실제로 필요한 시점(초 단위 이하 빈도)에만 호출하세요.

**예제 — 실패(Rev1.1/미설정)를 항상 대비하는 실전 패턴**

아래는 [`34_MSC_GaitAnalysis_Log`](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/34_MSC_GaitAnalysis_Log/) 예제에서 실제로 쓰는 방식입니다. `GetDateTime` 이 `false` 를 반환하거나 `year` 가 비정상이면(= Rev1.1 stub 또는 RTC 미설정) tick 기반 이름으로 자동 전환합니다.

```c
static void _GenerateSessionName(char* buf, uint32_t buf_size)
{
    XmDateTime_t rtc = {0};
    XM_RTC_GetDateTime(&rtc);
    if (rtc.year >= 2020 && rtc.year <= 2099) {
        snprintf(buf, buf_size, "%02u%02u%02u_%02u%02u%02u",
                 (unsigned)(rtc.year % 100), (unsigned)rtc.month, (unsigned)rtc.day,
                 (unsigned)rtc.hour, (unsigned)rtc.minute, (unsigned)rtc.second);
    } else {
        /* RTC 미설정/Rev1.1 stub — 부팅 tick 기반 fallback */
        snprintf(buf, buf_size, "XM_%08lu", (unsigned long)osKernelGetTickCount());
    }
}
```

**참고**: [`XM_RTC_SetDateTime`](#xm_rtc_setdatetime) · [06. USB 메모리 로깅](../06-usb-data-logging.md)

---

### `XM_RTC_IsRunning`

```c
bool XM_RTC_IsRunning(void);
```

RTC 오실레이터가 동작 중인지 확인합니다. 백업 배터리(코인셀 CR1220)가 방전되면 오실레이터가 멈추고, 이 경우 `false` 를 반환합니다.

이 함수는 파라미터가 없습니다.

**반환값**

| 값 | 의미 |
|----|------|
| `true` | 동작 중 |
| `false` | 정지 — 배터리 방전 등. **Rev1.1 에서는 항상 `false`** |

> ⚠️ **호출 컨텍스트**: `Control_Setup()`/`Control_Loop()` 컨텍스트 기준입니다. 보통 `Control_Setup()` 시작 시 한 번 호출해서, `false` 이면 `XM_RTC_SetDateTime()` 으로 시간을 재설정하는 가드로 씁니다.

**예제**

```c
void Control_Setup(void)
{
    if (!XM_RTC_IsRunning()) {
        XmDateTime_t dt = { .year = 2026, .month = 4, .day = 3, .weekday = 5,
                             .hour = 14, .minute = 30, .second = 0 };
        XM_RTC_SetDateTime(&dt);
    }
}
```

**참고**: [`XM_RTC_SetDateTime`](#xm_rtc_setdatetime)

---

## 타입/매크로

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

| 필드 | 타입 | 범위 | 설명 |
|------|------|------|------|
| `year` | `uint16_t` | 2000~2099 | 연도 (4자리). `XM_RTC_SetDateTime`/`XM_RTC_GetDateTime` 내부에서 MCP79510 의 2자리 연도로 상호 변환됩니다 |
| `month` | `uint8_t` | 1~12 | 월 |
| `day` | `uint8_t` | 1~31 | 일 |
| `weekday` | `uint8_t` | 1~7 | 요일. ISO 8601 방식 (1=월요일 ~ 7=일요일). MCP79510 은 weekday 를 날짜로부터 자동 계산하지 않으므로 항상 직접 채워야 합니다 |
| `hour` | `uint8_t` | 0~23 | 시 (24시간제) |
| `minute` | `uint8_t` | 0~59 | 분 |
| `second` | `uint8_t` | 0~59 | 초 |

> 헤더에는 이 구조체 외 별도의 매크로나 enum 이 없습니다.

## 내부 전용 (호출 금지)

`xm_api_rtc.h` 에는 `[Internal]` / System-only 로 표시된 심볼이 없습니다. 헤더에 선언된 함수 3개(`XM_RTC_SetDateTime`, `XM_RTC_GetDateTime`, `XM_RTC_IsRunning`)와 타입 1개(`XmDateTime_t`) 모두 공개 API 입니다.

---

## Rev1.1 / Rev2.0 요약

| 항목 | Rev1.1 | Rev2.0 |
|------|--------|--------|
| 헤더 파일 (`xm_api_rtc.h`) | 포함 (Rev2.0 과 완전 동일 내용) | 포함 |
| RTC 하드웨어 (MCP79510) | 없음 | 있음 |
| 함수 3개 실제 동작 | stub — 항상 `false`, 무동작 | 정상 동작 |
| 시간이 필요할 때 대안 | `XM_GetTick()` 기반 경과 시간 | RTC 그대로 사용 |

> 참고: [보드 리비전 비교](../../hardware/README.md#보드-리비전-비교)
