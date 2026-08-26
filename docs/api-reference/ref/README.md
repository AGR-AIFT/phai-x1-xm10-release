# API 함수 레퍼런스

> 상위: [API 레퍼런스 개요](../README.md) · [문서 인덱스](../../README.md)

`XM_FW/XM_API/` 의 각 헤더가 선언하는 **정확한 함수 시그니처 · 파라미터 · 반환값 · 타입**을 헤더 단위로 정리한 페이지 모음입니다. "왜 이 API 를 쓰는지 / 어떻게 알고리즘에 녹이는지"는 상위 [개념 가이드(01~09)](../README.md#함수-그룹)에서 먼저 확인하세요. 이 페이지들은 코드를 작성하며 **정확한 호출 규약**을 빠르게 찾아보는 용도입니다.

---

## 전체 통합 (진입점)

| 헤더 | 한 줄 설명 | 레퍼런스 | 관련 개념 문서 | 대표 예제 |
|------|-----------|----------|---------------|-----------|
| `xm_api.h` | SDK 의 모든 API 헤더를 한 번에 include 하는 파사드 + `XM_GetTick()` | [xm-api.md](xm-api.md) | [API 레퍼런스 개요](../README.md) | [00_Quick_Start](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/00_Quick_Start/) — 사실상 전 예제가 이 헤더를 include |

## 제어 · 데이터

| 헤더 | 한 줄 설명 | 레퍼런스 | 관련 개념 문서 | 대표 예제 |
|------|-----------|----------|---------------|-----------|
| `xm_api_data.h` | KIT H10 센서 데이터 읽기(`XM.status`) + 토크/위치 제어 명령(P/I/F-Vector) — 23함수·3매크로·13타입 | [xm-api-data.md](xm-api-data.md) | [02. KIT H10 제어 + 데이터](../02-h10-control-n-data.md) | [12_Active_Assist_Mode](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/12_Active_Assist_Mode/) |

## 상태 머신

| 헤더 | 한 줄 설명 | 레퍼런스 | 관련 개념 문서 | 대표 예제 |
|------|-----------|----------|---------------|-----------|
| `xm_api_tsm.h` | 상태(State) 단위로 제어 로직을 분리하는 Task State Machine — 4함수·6타입 | [xm-api-tsm.md](xm-api-tsm.md) | [01. 상태 머신 (TSM)](../01-task-state-machine.md) | [03_Button_LED_FSM](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/03_Button_LED_FSM/) |

## IO

| 헤더 | 한 줄 설명 | 레퍼런스 | 관련 개념 문서 | 대표 예제 |
|------|-----------|----------|---------------|-----------|
| `xm_api_led_btn.h` | 내장 LED 효과/상태 제어 + 버튼 이벤트 읽기 — 6함수·7타입 | [xm-api-led-btn.md](xm-api-led-btn.md) | [03. LED + 버튼](../03-led-btn-control.md) | [01_Button_LED_Basic](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/01_Button_LED_Basic/) |
| `xm_api_external_io.h` | 확장 포트 디지털 GPIO 입출력 + ADC 아날로그 읽기 — 21심볼(14함수 등) | [xm-api-external-io.md](xm-api-external-io.md) | [04. 외부 IO](../04-external-io.md) | [04_Ext_IO_Basic](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/04_Ext_IO_Basic/) |

## USB · 로깅

| 헤더 | 한 줄 설명 | 레퍼런스 | 관련 개념 문서 | 대표 예제 |
|------|-----------|----------|---------------|-----------|
| `xm_api_usb.h` | USB 시리얼(CDC) 실시간 스트리밍 | [xm-api-usb.md](xm-api-usb.md) | [05. USB 시리얼](../05-usb-connectivity.md) | [09_CDC_Stream](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) |

## 메모리 · RTC

| 헤더 | 한 줄 설명 | 레퍼런스 | 관련 개념 문서 | 대표 예제 |
|------|-----------|----------|---------------|-----------|
| `xm_api_memory.h` | Workspace/PSRAM/DTCM 빠른 메모리 + 비휘발성(Flash) 저장소 접근 — 🟢 **Rev 2.0 전용**, 11함수 | [xm-api-memory.md](xm-api-memory.md) | [07. 메모리 영역](../07-memory-management.md) | [36_OnDevice_Kinesthetic_Learning](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/36_OnDevice_Kinesthetic_Learning/) |
| `xm_api_rtc.h` | RTC 날짜/시간 설정·조회 — ⚠️ Rev1.1 은 하드웨어 부재로 stub, 3함수 | [xm-api-rtc.md](xm-api-rtc.md) | [08. 실시간 시계](../08-rtc-clock.md) | — |

## RTOS

| 헤더 | 한 줄 설명 | 레퍼런스 | 관련 개념 문서 | 대표 예제 |
|------|-----------|----------|---------------|-----------|
| `xm_api_freertos.h` | 보조 task 생성(1회성/주기) + Mutex — FreeRTOS 학생 친화 wrapper, 17함수(15 활성+2 deprecated) | [xm-api-freertos.md](xm-api-freertos.md) | [09. 보조 task + 데이터 공유](../09-task-creation.md) | [38_Periodic_Background_Task](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/38_Periodic_Background_Task/) |

## 사용자 데이터

| 헤더 | 한 줄 설명 | 레퍼런스 | 관련 개념 문서 | 대표 예제 |
|------|-----------|----------|---------------|-----------|
| `xm_api_user_custom.h` | Total Data Packet(0x20) 안 28바이트 사용자 커스텀 슬롯 — 7함수·4매크로·1타입 | [xm-api-user-custom.md](xm-api-user-custom.md) | [05. USB 시리얼 통신](../05-usb-connectivity.md) | 직접 사용 예제 없음(v2.3.0 신규 API) — 같은 패킷을 다루는 [09_CDC_Stream](https://github.com/AGR-AIFT/phai-x1-xm10-release/tree/Develop/examples/09_CDC_Stream/) 참고 |

---

## 참고

- [API 레퍼런스 개요](../README.md) — 01~09 개념 가이드 인덱스
- [문서 인덱스](../../README.md)
