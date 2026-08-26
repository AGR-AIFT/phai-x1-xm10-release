# `xm_api.h` — XM10 SDK 진입점 (전체 API 통합 헤더)

> **대상 헤더**: [`XM_FW/XM_API/xm_api.h`](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/XM10_SDK/Rev2.0/Extension_Module/XM_FW/XM_API/xm_api.h) (Rev2.0 기준 — Rev1.1 에도 동일 파일이 있으며 차이는 [§2](#2-이-헤더가-통합하는-api)에서 다룹니다)
> **관련 개념 문서**: [API 참고서 전체 인덱스](../README.md) — 01~09 전 항목이 이 헤더가 통합하는 하위 API 입니다
> **관련 예제**: 사실상 모든 예제 — 저장소 예제 중 `xm_api.h` 를 include 하지 않는 것이 거의 없습니다. `XM_GetTick()` 을 직접 쓰는 대표 예제는 [00_Quick_Start](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/00_Quick_Start/)

---

## 언제 사용하나

XM10 예제(`.c`) 파일 맨 위에 아래 한 줄만 추가하면 로봇 제어, 센서 읽기, LED/버튼, 확장 IO, USB 통신·로깅, 메모리, RTC, 보조 task 까지 SDK 의 모든 사용자 API 를 한 번에 쓸 수 있습니다.

```c
#include "xm_api.h"
```

`xm_api.h` 자신은 실제 기능을 구현하지 않는 **파사드(facade) 헤더**입니다. 하위 API 헤더들을 한 번에 포함시켜 주는 역할과, 시스템 전역에서 자주 쓰는 타이머 유틸리티 함수 `XM_GetTick()` 하나만 직접 선언합니다. 각 도메인의 실제 사용법(함수 목록, 예제, 주의사항)은 아래 표에서 연결된 개념 문서를 참고하세요 — 이 문서는 각 도메인을 다시 설명하지 않습니다.

## 이 헤더가 통합하는 API

`xm_api.h` 는 다음 하위 API 헤더들을 `#include` 합니다. Rev2.0 기준 9개, Rev1.1 기준 8개입니다(차이는 아래 표의 비고 참고).

| # | 포함 헤더 | 한 줄 설명 | 개념 문서 |
|---|-----------|-----------|-----------|
| 1 | `xm_api_data.h` | 로봇(KIT H10) 센서 데이터 읽기 + 토크/위치 제어 명령 | [02 KIT H10 제어 + 데이터](../02-h10-control-n-data.md) |
| 2 | `xm_api_tsm.h` | 태스크 상태 머신 (FSM) | [01 상태 머신 (TSM)](../01-task-state-machine.md) |
| 3 | `xm_api_led_btn.h` | 내장 LED 및 버튼 제어 | [03 LED + 버튼](../03-led-btn-control.md) |
| 4 | `xm_api_external_io.h` | 확장 포트 GPIO/ADC | [04 외부 IO](../04-external-io.md) |
| 5 | `xm_api_usb.h` | USB 시리얼 통신 (CDC) | [05 USB 시리얼 통신](../05-usb-connectivity.md) |
| 6 | `xm_api_user_custom.h` | Total Data(0x20) 패킷 내 사용자 커스텀 슬롯(28B) — PhAI Studio 스트리밍/녹화에 자동 포함 | 전용 개념 문서 없음 — 헤더 안 Doxygen 예제(`@code` 블록) 참고 |
| 7 | `xm_api_memory.h` | 메모리 영역 접근 (Workspace/PSRAM/DTCM/비휘발성 저장소) | [07 메모리 영역](../07-memory-management.md) — 🟢 **Rev 2.0 전용** |
| 8 | `xm_api_rtc.h` | RTC 날짜/시간 관리 | [08 실시간 시계](../08-rtc-clock.md) — ⚠️ Rev1.1 에서는 stub |
| 9 | `xm_api_freertos.h` | 보조 task 생성 + Mutex (FreeRTOS 학생 친화 wrapper) | [09 보조 task + 데이터 공유](../09-task-creation.md) |
| 10 | `xm_api_safety.h` | 안전 토크 헬퍼 (v2.6.0 신규) — 이상값 차단 + 한계 클램프 + 진입 소프트스타트 + 기울기 제한을 한 번에 처리하는 `XM_SafeTorque_*`, 보조 레벨 클램프 `XM_SafeAssistLevel()`, 센서 신선도 확인 `XM_SafeIsFresh()`. 헤더 안에 구현이 들어 있어 별도 `.c` 가 없습니다 | [02 KIT H10 제어 + 데이터](../02-h10-control-n-data.md) |

**Rev 차이 요약**:

- 🟢 **Rev 2.0 전용**: `xm_api_memory.h` — Rev1.1 SDK 에는 이 헤더 파일 자체가 없습니다(즉 `xm_api.h` 가 이 헤더를 include 하지 않습니다). Workspace/PSRAM 8MB 등 Rev2.0 전용 하드웨어를 반영한 API 입니다.
- ⚠️ **Rev1.1 제한**: `xm_api_rtc.h` 는 두 리비전 모두에 파일이 존재하지만, Rev1.1 보드에는 RTC 칩(MCP79510)이 실장되어 있지 않아 모든 함수가 stub(항상 `false` 반환/무동작)으로 컴파일됩니다. Rev1.1 에서 시간 기록이 필요하면 `XM_GetTick()` 기반 경과시간으로 대체하세요.
- 그 외 7개 헤더(`xm_api_data/tsm/led_btn/external_io/usb/user_custom/freertos`)는 두 리비전에서 내용이 동일합니다.

## 함수 목록

`xm_api.h` 자신이 직접 선언하는 공개 함수는 다음 1개뿐입니다(나머지는 위 §2 헤더들이 선언).

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_GetTick`](#xm_gettick) | 시스템 부팅 후 경과된 시간(ms)을 가져옵니다 |

## 함수 상세

### `XM_GetTick`

```c
uint32_t XM_GetTick(void);
```

시스템이 부팅한 후 경과된 시간을 밀리초(ms) 단위로 반환합니다. 32비트 카운터이므로 약 49.7일(2³² ms)마다 0으로 돌아갑니다(rollover).

**파라미터**

없음.

**반환값**

| 타입 | 설명 |
|------|------|
| `uint32_t` | 부팅 후 경과 시간 (ms) |

⚠️ **호출 컨텍스트**: 헤더 주석에는 별도의 태스크/ISR 제약이 명시되어 있지 않습니다. `Control_Setup()` / `Control_Loop()` 컨텍스트 기준으로 사용하는 것을 권장합니다 — SDK 예제는 모두 이 두 함수 안에서만 호출합니다.

**예제** — rollover에 안전한 경과시간 계산 ([Ex.00 Quick Start](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/00_Quick_Start/) 패턴):

```c
static uint32_t s_boot_timer;

void Control_Setup(void)
{
    s_boot_timer = XM_GetTick();
}

void Control_Loop(void)
{
    uint32_t now = XM_GetTick();
    if (now - s_boot_timer >= 500) {   /* 부호 없는 뺄셈 → rollover 안전 */
        s_boot_timer = now;
        /* ... 500ms 마다 실행할 로직 ... */
    }
}
```

⚠️ 시간 비교는 반드시 `(now - past) >= interval` 형태로 작성하세요. `now >= past + interval` 형태는 `past + interval` 자체가 오버플로우될 수 있어 rollover 시점 부근에서 오작동합니다.

**참고**:

- [09 보조 task + 데이터 공유](../09-task-creation.md) — 주기적 task 에서 tick 활용
- [00_Quick_Start 예제](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/00_Quick_Start/)

## 내부 전용 (호출 금지)

`xm_api.h` 자체에는 `[Internal]` 로 표시되었거나 System 전용인 심볼이 없습니다. 하위 헤더 각각의 내부 전용 함수는 해당 개념 문서(§2 표) 쪽에서 다룹니다.

## 참고

- [API 참고서 인덱스](../README.md)
- 위 §2 표에 연결된 01~09 개념 문서
