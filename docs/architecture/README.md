# 시스템 아키텍처

`KIT H10`은 인간의 동작 제어 시스템을 모방한 **계층적 분산 제어 아키텍처**를 기반으로 합니다. 이를 통해 복잡한 로봇 제어를 명확한 역할 분담으로 나누어 안정성과 확장성을 확보했습니다.

---

## angel Robotics 모듈 시스템

인간의 동작 제어 시스템을 모방한 angel Robotics의 전체 모듈 아키텍처입니다.

<p align="center">
  <img width="712" height="351" alt="Angel Robotics Module Architecture" src="https://github.com/user-attachments/assets/911991bc-8c85-4590-8177-47c9af677cbf" />
</p>

---

## KIT H10 분산 제어 구조

`KIT H10`에 포함된 모터드라이버는 분산 제어에 특화된 형태의 시스템으로서 아래와 같은 구조로 설계되었습니다.

<p align="center">
  <img width="753" height="383" alt="KIT H10 Distributed Control" src="https://github.com/user-attachments/assets/656dbcc5-6e87-495b-8d1c-9741060ee3d6" />
</p>

---

## KIT H10 + XM10 통합 시스템

분산 제어 구조를 기반으로 한 `KIT H10`과 `XM10`의 전체 시스템 아키텍처입니다.

<p align="center">
  <img width="1934" height="987" alt="KIT H10 + XM10 System Architecture" src="https://github.com/user-attachments/assets/29428128-6802-4bbf-ad9f-51a806e92d2b" />
</p>

---

## XM10 FW 아키텍처

실제 개발자가 마주하게 될 `XM10`의 내부 펌웨어 시스템 아키텍처 구조입니다.

<p align="center">
  <img width="1895" height="930" alt="XM10 FW Architecture" src="https://github.com/user-attachments/assets/6f3e0f15-6865-459b-9fe2-6bf2cff27103" />
</p>

### 레이어 구조

| 레이어 | 역할 | 설명 |
| :--- | :--- | :--- |
| **Application Layer** | 사용자 영역 | `XM_Apps/User_Algorithm`에서 `XM API`만으로 로봇의 모든 기능을 제어 |
| **Facade Layer (XM API)** | 인터페이스 | 파사드 패턴을 적용하여 복잡한 내부를 숨기고 단순한 API 창구 제공 |
| **System** | 태스크 관리 | PnP Task, Rx/Tx Task 등 백그라운드 태스크 운영 |
| **Services** | 프로토콜 | AGR DOP V2 (데이터 객체), AGR PnP V2 (디바이스 검색) |
| **Devices** | 드라이버 | Control Module, IMU Hub, Marveldex, XSENS 등 센서 드라이버 |
| **IOIF V3.0** | HAL 래퍼 | FDCAN, UART, GPIO, ADC, TIM, USB, DWT 등 페리페럴 추상화 |
| **Middlewares** | 미들웨어 | FreeRTOS, FatFs, USB CDC/MSC 스택 |
| **HAL / CMSIS** | 하드웨어 | STM32H7 HAL 드라이버, CMSIS 코어 |

### 통신 프로토콜

* **CAN-FD:** XM10 ↔ KIT H10 (제어 명령, 센서 데이터) 및 센서 허브 모듈 연동
* **USB-CDC:** XM10 → PC 실시간 데이터 스트리밍 (PhAI V2 프로토콜)
* **USB-MSC:** XM10 → USB 메모리 고속 데이터 저장

### 제어 루프

* **주기:** 1ms (1000Hz)
* **패턴:** IPO (Input → Process → Output)
  1. **Input:** CAN-FD 수신 데이터(센서, 로봇 상태) 읽기
  2. **Process:** 사용자 알고리즘(`User_Loop`) 실행
  3. **Output:** CAN-FD 제어 명령 전송
  4. **USB:** USB-CDC Data Streaming or USB-MSC Data Logging

---

> 이 문서는 Phase 2에서 각 레이어의 상세 설명 (IOIF V3.0, AGR DOP V2, AGR PnP V2, ISR-to-Task 패턴 등)으로 확장될 예정입니다.
