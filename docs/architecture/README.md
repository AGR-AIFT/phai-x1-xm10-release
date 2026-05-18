# 시스템 구조

여기서는 XM10 안에서 **내가 작성한 코드가 어떻게 동작하는지** 큰 그림을 잡습니다. 세부 구현이 아니라, 처음 접하는 사람이 "내 코드는 어디에 들어가고, 그 외 일들은 누가 처리하나" 정도를 이해하는 것이 목표입니다.

---

## 큰 그림

`KIT H10` 외골격 로봇은 사람의 동작 제어 시스템을 본떠 만든 분산 제어 구조입니다. 부위마다 역할이 나뉘어 있어서 한 곳이 느려져도 전체가 멈추지 않습니다.

<p align="center">
  <img width="712" height="351" alt="Angel Robotics Module Architecture" src="https://github.com/user-attachments/assets/911991bc-8c85-4590-8177-47c9af677cbf" />
</p>

KIT H10 자체의 분산 제어 구조입니다. 모터 드라이버가 각자 독립적으로 동작하면서, 중앙 제어 모듈 (CM) 이 전체를 조율합니다.

<p align="center">
  <img width="753" height="383" alt="KIT H10 Distributed Control" src="https://github.com/user-attachments/assets/656dbcc5-6e87-495b-8d1c-9741060ee3d6" />
</p>

여기에 XM10 이 들어오면 통합 시스템이 됩니다. XM10 은 외골격에 직접 명령을 내리고, 센서 데이터를 받아 새로운 알고리즘을 시험할 수 있는 "두뇌 확장" 보드입니다.

<p align="center">
  <img width="1934" height="987" alt="KIT H10 + XM10 System Architecture" src="https://github.com/user-attachments/assets/29428128-6802-4bbf-ad9f-51a806e92d2b" />
</p>

---

## XM10 펌웨어 — 내 코드는 어디에?

XM10 보드 내부는 여러 층으로 구성되어 있습니다. 사용자가 손대는 곳은 **맨 위 한 곳** 뿐이에요.

<p align="center">
  <img width="1984" height="900" alt="xm10-system-architecture" src="https://github.com/user-attachments/assets/16cea885-2f31-4e1e-a59f-c2eb6dc6d985" />
</p>

| 층 | 누가 만드나 | 사용자 작업 |
|----|-----------|---------|
| **사용자 알고리즘** (`User_Algorithm/user_app.c`) | 사용자 | ✅ 여기만 작성 |
| **XM API** — 단순 호출 창구 (`XM_*` 함수들) | XM 라이브러리 | 호출만 |
| **내부 시스템** — 센서 통신, USB, 외골격 프로토콜 등 | XM 라이브러리 | 안 건드림 |
| **ST 표준 코드** — 운영체제, USB 드라이버, 보드 페리페럴 | ST + 오픈소스 | 안 건드림 |

자동차에 비유하면 사용자는 운전자입니다. 엑셀 (`XM_SetAssistTorqueRH`), 브레이크 (`XM_SetControlMode`), 핸들 (`XM_GetButtonEvent`) 만 조작하면 됩니다. 엔진/변속기/ECU 같은 내부는 모두 XM 라이브러리가 알아서 돌아갑니다.

> **왜 이렇게 나누었나?**
> CAN 통신, USB 송신, 모터 명령 파싱 같은 일을 사용자가 매번 직접 짜면 — 버그가 생기기 쉽고, 한 사용자가 만든 코드를 다른 사용자가 못 씁니다. 공통 부분을 라이브러리로 빼고 알고리즘만 직접 작성하도록 분리해 두면, 시스템 안정성을 보장하면서도 사용자는 본질 (제어 알고리즘) 에만 집중할 수 있습니다.

---

## 내 코드는 언제 실행되나 — 1 ms 제어 루프

XM10 의 핵심은 **1 ms (1 kHz) 주기로 반복되는 제어 루프**입니다. 매 1 ms 마다 다음 세 가지가 순서대로 일어납니다.

```
  ┌────────────────────────────────────────────────────────────┐
  │            매 1 ms 마다 반복 (1 kHz 제어 루프)              │
  │                                                            │
  │   [입력]            [내 코드]            [출력]             │
  │     ↓                  ↓                   ↓               │
  │  CAN 으로          User_Loop()         XM 함수가          │
  │  받은 센서 →       에서 내가      →    KIT H10 에게       │
  │  데이터가          작성한 알고리즘     CAN 명령 전송       │
  │  자동 갱신                                                 │
  │                                                            │
  │  XM.status.h10.    XM_TSM_Run         XM_SetAssistTorque  │
  │   leftHipAngle      → on_loop()        XM_SendUsbData     │
  │   rightHipAngle                                            │
  │   isFootContact                                            │
  └────────────────────────────────────────────────────────────┘
                              ↓ 1 ms 후 반복
```

- **입력 (자동)**: CAN-FD 로 받은 KIT H10 의 센서·상태가 `XM.status.h10.*` 전역 변수에 매 cycle 자동으로 채워집니다.
- **처리 (내가 작성)**: `User_Loop()` 안에서 그 데이터를 읽고 알고리즘을 돌립니다.
- **출력 (자동)**: `XM_SetAssistTorqueRH(...)` 같은 함수를 호출하면, 내부에서 CAN 메시지로 변환해 KIT H10 에 자동으로 보냅니다.

USB 통신, 데이터 로깅도 함수 호출 한 번이면 끝입니다. 내가 신경 쓸 건 알고리즘 본질뿐이에요.

---

## 전원 켠 순간부터 내 코드까지

사용자가 `Debug (F11)` 로 펌웨어를 올리고 나면, 다음 흐름으로 보드가 시작됩니다.

```
  전원 ON
    ↓
  부트 코드 + 보드 초기화 (자동)
    ↓
  XM 라이브러리 초기화
    — 페리페럴 설정, USB 준비, CAN 연결, 센서 인식 등
    ↓
  내부 스케줄러 시작
    ↓
  ┌──────────────────────────────────────────────────────────────┐
  │  스케줄러가 여러 작업을 우선순위에 따라 번갈아 실행:           │
  │                                                              │
  │   • 사용자 작업    → User_Setup() 1회 → User_Loop() 매 1 ms  │
  │   • CAN 수신       → 외골격 센서 데이터 받아 XM.status 갱신   │
  │   • CAN 송신       → 모터 명령 전송                          │
  │   • USB 작업       → 시리얼/메모리 통신                       │
  │   • 디바이스 검색  → 센서 허브 자동 연결                      │
  └──────────────────────────────────────────────────────────────┘
```

내 코드의 진입점은 두 함수입니다.

- `User_Setup()` — 부팅 직후 1회만 실행. 초기화 코드 (LED 상태 설정, TSM 등록 등) 를 여기에.
- `User_Loop()` — 매 1 ms 마다 호출. 알고리즘 본체를 여기에.

CAN 수신 같은 다른 작업이 동시에 돌아가지만, 사용자는 신경 쓰지 않아도 됩니다. 시스템이 알아서 처리합니다.

---

## 내 파일은 어디?

```
Extension_Module/
├── XM_FW/
│   ├── XM_Apps/
│   │   └── User_Algorithm/
│   │       └── user_app.c     ← 여기가 사용자 작업 공간
│   └── (그 외 폴더는 모두 XM 라이브러리 — 건드리지 않습니다)
└── examples/                  ← 41 개 예제의 user_app.c 모음
    ├── 00_Quick_Start/quick_start.c
    ├── 14_PD_Realtime_Control/pd_realtime_control.c
    └── ...
```

예제를 시험하려면 해당 폴더의 `.c` 파일 내용을 `XM_Apps/User_Algorithm/user_app.c` 에 통째로 복사해서 빌드하면 됩니다.

---

## 통신 정리

세 종류만 알면 됩니다.

| 채널 | 누가 누구와 | 내가 쓰는 함수 |
|------|----------|--------------|
| **CAN-FD** | XM10 ↔ KIT H10 외골격 | `XM.status.h10.*` 읽기, `XM_SetAssistTorque*()` 쓰기 |
| **USB 시리얼 (CDC)** | XM10 → PC 터미널/PhAI Studio | `XM_SendUsbDebugMessage`, `XM_SendUsbDataWithId` |
| **USB 메모리 (MSC)** | XM10 → USB 메모리 (로깅) | `XM_StartUsbLogging`, `XM_LogBinaryData` |

세부 프로토콜은 라이브러리가 알아서 처리합니다. 사용자는 함수 호출만 하면 됩니다.

---

## 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| `main()` 에 코드를 추가했는데 동작 안 함 | `main()` 은 내부 스케줄러를 시작하고 끝남. 사용자 코드 자리는 `user_app.c` | `XM_Apps/User_Algorithm/user_app.c` 만 수정 |
| `User_Loop` 안에서 `while(1)` 사용 | 한 cycle 안에서 빠져나오지 못해 다른 작업이 멈춤 | `User_Loop` 는 한 번 실행하고 return — 반복은 시스템이 알아서 |
| `User_Loop` 안에서 `HAL_Delay(100)` 사용 | 100 ms 동안 다른 작업 모두 정지 | `XM_GetTick()` 차이로 비차단 타이머 구성 |
| `XM.status.h10.leftHipAngle` 이 항상 0 | KIT H10 미연결 또는 ASSIST 모드 진입 전 | `XM_IsCmConnected()` + `h10Mode == XM_H10_MODE_ASSIST` 확인 |
| `XM_SetAssistTorque*` 호출했는데 토크 안 나옴 | 토크 제어 모드 진입 누락 | Active 진입 시 `XM_SetControlMode(XM_CTRL_TORQUE)` 1회 호출 |
| XM 라이브러리 폴더 (IOIF, Devices 등) 코드를 수정 | 라이브러리는 양산 코드 — 임의 수정 시 시스템 깨짐 | 항상 `User_Algorithm/` 안에서만 작업 |
| User_Loop 가 1 ms 안에 못 끝남 | 무거운 sprintf, 부동소수 누적 연산 등 | [Ex.18 Debug Monitor](../../examples/18_Debug_Monitor/) 로 실행 시간 측정 후 분산 |
| 두 예제를 동시에 빌드 시도 | `User_Algorithm/` 안에 `.c` 파일은 하나만 | 한 번에 한 예제만 복사해서 빌드 |

---

## 다음 단계

- 처음 빌드/플래시: [Getting Started — 03 첫 빌드](../getting-started/03-first-build.md)
- 상태 머신 패턴: [TSM API 레퍼런스](../api-reference/01-task-state-machine.md)
- 예제 학습 경로: [Tutorials](../tutorials/README.md)
- 내 코드가 1 ms 안에 끝나는지 확인: [Ex.18 Debug Monitor](../../examples/18_Debug_Monitor/)
