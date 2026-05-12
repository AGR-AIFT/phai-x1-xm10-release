# 시스템 아키텍처

> 📌 **이 페이지를 읽고 나면**: XM10 펌웨어가 **어떤 레이어** 로 구성되어 있고, 내가 작성하는 `User_Setup` / `User_Loop` 코드가 **RTOS Task 안 어디서** 실행되는지 그림으로 설명할 수 있습니다.
> ⏱️ 예상 학습 시간: 25분
> 🧰 사전 지식: [Ex.00 Quick Start](../../examples/00_Quick_Start/) 빌드/플래시 경험 (학생이 사용자 코드 영역을 본 적 있음)
> 🎯 핵심 개념: **계층적 분산 제어** + **IPO 1 kHz 루프** + **FreeRTOS 멀티태스크**

`KIT H10`은 인간의 동작 제어 시스템을 모방한 **계층적 분산 제어 아키텍처**를 기반으로 합니다. 이를 통해 복잡한 로봇 제어를 명확한 역할 분담으로 나누어 안정성과 확장성을 확보했습니다.

---

## 💡 WHY — 왜 이런 구조가 필요한가

학생 입장에서 "그냥 `main()` 안에 모든 코드를 넣으면 안 되나?" 라는 의문이 자연스럽습니다.

| `main()` 단순 구조의 한계 | XM10 의 해결 방법 |
|--------------------------|------------------|
| CAN 수신 / 모터 제어 / USB 송신을 한 루프에 → 하나가 늦으면 전체 늦음 | **RTOS Task 분리** + 우선순위로 제어 보장 |
| 센서 / 통신 / USB 드라이버 매번 새로 작성 → 학생마다 다른 코드 | **IOIF V3.0 (HAL 래퍼)** 한 번 작성, 모두 재사용 |
| KIT H10 명령 프로토콜을 학생이 직접 파싱 → 버그 천국 | **AGR DOP V2** 가 자동 변환 + `XM.status.h10.*` 로 단순 접근 |
| 사용자 알고리즘이 시스템 코드와 섞임 → 실수로 깨짐 | **사용자 영역 (`XM_Apps/User_Algorithm`) 분리** + Facade API 만 노출 |

> 🧒 **비유**: XM10 은 자동차의 자동변속기 같은 것 — 학생(드라이버) 은 "엑셀/브레이크" (= `XM_SetAssistTorque` / `XM_GetButtonEvent`) 만 조작하면, 내부 (= 엔진/변속기/ECU = `Services` / `IOIF` / `HAL`) 가 알아서 돌아간다.

---

## 📖 WHAT — 무엇이 있는가

### 1. angel Robotics 모듈 시스템 (최상위)

인간의 동작 제어 시스템을 모방한 angel Robotics의 전체 모듈 아키텍처입니다.

<p align="center">
  <img width="712" height="351" alt="Angel Robotics Module Architecture" src="https://github.com/user-attachments/assets/911991bc-8c85-4590-8177-47c9af677cbf" />
</p>

### 2. KIT H10 분산 제어 구조

`KIT H10`에 포함된 모터드라이버는 분산 제어에 특화된 형태의 시스템으로서 아래와 같은 구조로 설계되었습니다.

<p align="center">
  <img width="753" height="383" alt="KIT H10 Distributed Control" src="https://github.com/user-attachments/assets/656dbcc5-6e87-495b-8d1c-9741060ee3d6" />
</p>

### 3. KIT H10 + XM10 통합 시스템

분산 제어 구조를 기반으로 한 `KIT H10`과 `XM10`의 전체 시스템 아키텍처입니다.

<p align="center">
  <img width="1934" height="987" alt="KIT H10 + XM10 System Architecture" src="https://github.com/user-attachments/assets/29428128-6802-4bbf-ad9f-51a806e92d2b" />
</p>

### 4. XM10 FW 아키텍처 (학생이 직접 마주칠 영역)

실제 개발자가 마주하게 될 `XM10`의 내부 펌웨어 시스템 아키텍처 구조입니다.

<p align="center">
  <img width="1895" height="930" alt="XM10 FW Architecture" src="https://github.com/user-attachments/assets/6f3e0f15-6865-459b-9fe2-6bf2cff27103" />
</p>

#### 레이어 구조 (위에서 아래로)

| 레이어 | 역할 | 학생 작업 영역? |
|:--|:--|:-:|
| **Application Layer** | 사용자 영역 (`User_Setup` / `User_Loop`) — XM API 만으로 로봇 제어 | ✅ **여기만 수정** |
| **Facade Layer (XM API)** | 파사드 패턴 — 복잡한 내부를 숨기고 단순한 API 창구 제공 | ❌ 호출만 |
| **System** | PnP Task, Rx/Tx Task 등 백그라운드 태스크 운영 | ❌ |
| **Services** | AGR DOP V2 (데이터 객체), AGR PnP V2 (디바이스 검색) | ❌ |
| **Devices** | Control Module, IMU Hub, Marveldex, XSENS 등 센서 드라이버 | ❌ |
| **IOIF V3.0** | FDCAN, UART, GPIO, ADC, TIM, USB, DWT 등 페리페럴 추상화 | ❌ |
| **Middlewares** | FreeRTOS, FatFs, USB CDC/MSC 스택 | ❌ |
| **HAL / CMSIS** | STM32H7 HAL 드라이버, CMSIS 코어 | ❌ |

> 🎯 **핵심**: 학생은 **Application Layer 한 곳** 만 작업합니다. 다른 레이어는 `XM_*` 함수 호출로만 접근.

---

## 🔧 HOW — 내 코드는 어디서 어떻게 실행되나

### 1. IPO 1 kHz 제어 루프 (가장 중요)

XM10 의 모든 사용자 코드는 **1 ms (1 kHz)** 주기의 IPO (Input → Process → Output) 패턴으로 실행됩니다.

```
  ┌─────────────────────────────────────────────────────────────┐
  │                  User Task (1 kHz, prio=Normal)             │
  │                                                              │
  │   ┌──────────┐    ┌──────────┐    ┌──────────┐              │
  │   │  INPUT   │ →  │ PROCESS  │ →  │  OUTPUT  │              │
  │   │ (자동)   │    │ User_Loop│    │ (자동)   │              │
  │   └────┬─────┘    └────┬─────┘    └────┬─────┘              │
  │        │               │               │                     │
  │  RxTask → XM.status   사용자 코드   XM_SetAssistTorque       │
  │  • h10AssistLevel     XM_TSM_Run    XM_SendUsbData...        │
  │  • leftHipAngle       on_loop()     → TxTask 가 CAN 송신     │
  │  • rightFootContact                                          │
  └─────────────────────────────────────────────────────────────┘
                          ↓ 1ms 후 반복
```

- **Input (자동)**: CAN-FD 로부터 받은 KIT H10 센서/상태 데이터가 `XM.status.h10.*` 전역에 자동 갱신됨
- **Process (학생)**: `User_Loop()` → 내부에서 `XM_TSM_Run` → `on_loop` 콜백 호출 → 학생 알고리즘 실행
- **Output (자동)**: `XM_SetAssistTorqueRH(...)` 호출 시 TxTask 가 자동으로 CAN-FD 프레임 송신
- **USB (병행)**: `XM_SendUsbData...` 는 USB CDC 스트리밍 또는 MSC 로깅

### 2. 호출 체인: 전원 ON → 내 코드까지

```
  Power ON
    ↓
  Reset_Handler (CMSIS)
    ↓
  SystemInit() + HAL_Init() + Clock 480 MHz 설정
    ↓
  main()
    ↓
  XM_System_Init()  ← XM_Lib 가 모든 페리페럴 + RTOS 초기화
    ↓
  osKernelStart()    ← FreeRTOS 스케줄러 시작 (main() 은 여기서 영원히 멈춤)
    ↓
  ┌───────────────────────────────────────────────────────────┐
  │ FreeRTOS Scheduler 가 Task 들을 우선순위 + 시간 슬라이스로 │
  │ 번갈아 실행:                                                │
  │                                                              │
  │   [Idle Task]   prio=0      Background, 전력 절감           │
  │   [User Task]   prio=Normal 1 kHz — User_Setup → User_Loop  │
  │   [Rx Task]     prio=High   CAN-FD 수신 인터럽트 후 처리     │
  │   [Tx Task]     prio=High   CAN-FD 송신 큐 처리              │
  │   [PnP Task]    prio=Low    디바이스 검색/연결               │
  │   [USB Task]    prio=Normal CDC/MSC                          │
  └───────────────────────────────────────────────────────────┘
    ↓
  User Task 의 첫 호출:
    User_Setup()  ← 1회만 실행
    ↓
  while(1) {                  ← 1 kHz 무한 루프
      User_Loop()             ← 매 1 ms 호출
      osDelay(1);             ← 다음 1 ms 까지 대기 (다른 Task 에 CPU 양보)
  }
```

> 🧒 **`osDelay(1)` 의 의미**: 1 ms 단위로 Task 가 잠들면서 다른 Task (Rx/Tx 등) 가 실행될 수 있게 양보. CPU 100% 점유 방지 + 멀티태스크 협력.

### 3. RTOS Task 우선순위 (왜 RxTask 가 User Task 보다 높은가)

```
   우선순위 ↑
   ┌──────────────────────────────────┐
   │ RxTask (prio=High)               │ ← CAN-FD 인터럽트 후 즉시 처리
   │ TxTask (prio=High)               │ ← 송신 큐 비우기 (지연 방지)
   ├──────────────────────────────────┤
   │ USB Task (prio=Normal)           │ ← USB 스택
   │ User Task (prio=Normal) ★        │ ← 학생 알고리즘 (1 kHz)
   ├──────────────────────────────────┤
   │ PnP Task (prio=Low)              │ ← 디바이스 검색 (빠를 필요 없음)
   ├──────────────────────────────────┤
   │ Idle Task (prio=0)               │ ← CPU 놀 때 실행 (전력 관리)
   └──────────────────────────────────┘
```

- **RxTask 가 User Task 보다 높음** → 센서 데이터를 즉시 `XM.status.*` 에 반영 → User Task 가 **stale data** 를 안 보게 됨
- **User Task = Normal** → 학생 코드가 시스템 코드보다 우선순위 낮음 (시스템 안정성 우선)

### 4. 사용자 영역 위치 — 파일 시스템 관점

```
ARC_ExtensionBoard/Extension_Module/
├── XM_FW/                     ← XM 펌웨어 (수정 금지)
│   ├── XM_Apps/
│   │   └── User_Algorithm/    ← ★ 학생 작업 영역
│   │       └── user_app.c     ← User_Setup(), User_Loop() 가 여기
│   ├── XM_Lib/                ← XM_API 라이브러리 (.a 로 빌드, 수정 금지)
│   ├── Devices/ Services/ System/ IOIF/  ← 내부 레이어
│   └── Middlewares/ Drivers/  ← FreeRTOS, HAL
└── examples/                  ← 41 개 예제의 user_app.c 들
    ├── 00_Quick_Start/quick_start.c
    ├── 14_PD_Realtime_Control/pd_realtime_control.c
    └── ...
```

학생은 예제의 `.c` 파일을 `XM_Apps/User_Algorithm/user_app.c` 자리에 덮어쓰고 빌드합니다.

### 5. 통신 프로토콜 한눈 정리

| 채널 | 방향 | 프로토콜 | 학생 접근 방법 |
|------|------|---------|--------------|
| **CAN-FD** | XM10 ↔ KIT H10 | AGR DOP V2 | `XM.status.h10.*` 읽기 + `XM_SetAssistTorque*()` 쓰기 |
| **CAN-FD** | XM10 ↔ 센서 허브 | AGR PnP V2 | `XM.status.imu_hub.*` 등 |
| **USB-CDC** | XM10 → PC | PhAI V2 + 사용자 정의 | `XM_SendUsbDebugMessage`, `XM_SendUsbDataWithId(0xF0~)` |
| **USB-MSC** | XM10 → USB 메모리 | FAT32 32KB cluster | `XM_StartUsbLogging`, `XM_LogBinaryData` |

---

## ⚠️ 흔한 실수 / 학생이 자주 빠지는 함정

| 증상 | 원인 | 해결 |
|------|------|------|
| `main()` 에 코드를 추가했는데 동작 안 함 | `main()` 은 `osKernelStart()` 에서 멈춤. 모든 사용자 코드는 `User_Setup`/`User_Loop` 안에 | `XM_Apps/User_Algorithm/user_app.c` 만 수정 |
| `User_Loop` 안에서 `while(1)` 또는 무한 루프 작성 | User Task 가 못 빠져나옴 → 다른 Task 굶주림 → 시스템 멈춤 | `User_Loop` 은 매 1 ms 호출이므로 한 번 실행 후 return |
| `User_Loop` 에서 `HAL_Delay(100)` 사용 | HAL_Delay 는 polling — 100 ms 동안 다른 Task 굶주림 | 비차단 패턴 (`XM_GetTick()` 차이) 사용 |
| `XM.status.h10.leftHipAngle` 이 0 만 나옴 | KIT H10 미연결 또는 ASSIST 모드 진입 안 함 | `XM_IsCmConnected()` + `h10Mode == XM_H10_MODE_ASSIST` 체크 |
| `XM_SetAssistTorque*` 가 안 먹힘 | `XM_SetControlMode(XM_CTRL_TORQUE)` 호출 누락 | Active 진입 시 1회 호출 필수 |
| IOIF / Devices / Services 코드를 수정 | XM_Lib 는 양산 코드 — 수정 금지 | 항상 Application Layer 만 |
| FreeRTOS API (`xTaskCreate` 등) 직접 호출 | RTOS 직접 접근은 시스템 깨질 위험 | `XM_*` API 사용 |
| User_Loop 가 1 ms 이상 걸려 오버런 | 무거운 sprintf / 부동소수 / sin/cos 누적 | Ex.18 Debug Monitor 로 측정 후 분산 |
| 두 예제를 동시에 빌드하려고 함 | `User_Algorithm/` 폴더에는 .c 파일 하나만 | 한 번에 한 예제만 import + 빌드 |

---

## ➡️ 다음 단계

- 처음 빌드/플래시: [docs/getting-started/03-first-build.md](../getting-started/03-first-build.md)
- TSM 으로 상태 분리: [docs/api-reference/01-task-state-machine.md](../api-reference/01-task-state-machine.md)
- 추천 학습 경로: [docs/tutorials/README.md](../tutorials/README.md)
- 실시간성 검증: [Ex.18 Debug Monitor](../../examples/18_Debug_Monitor/) (오버런 측정)

---

> 이 문서는 Phase 2에서 각 레이어의 상세 설명 (IOIF V3.0, AGR DOP V2, AGR PnP V2, ISR-to-Task 패턴 등)으로 확장될 예정입니다.
