# eXtension Module: XM10 부분 개방형 R&D 플랫폼

<p align="center">
  <img width="348" height="271" alt="image" src="https://github.com/user-attachments/assets/797cb252-48a7-4d6c-aa9d-3d7ffda565de" />
</p>
<p align="center">
  <a href="/LICENSE"><img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License: MIT"></a>
  <a href="#"><img src="https://img.shields.io/badge/Platform-STM32H7-blue.svg" alt="Platform"></a>
  <a href="#"><img src="https://img.shields.io/badge/OS-FreeRTOS-orange.svg" alt="OS"></a>
</p>

**`XM10`은 `angel Robotics`의 고관절 보조 로봇 `KIT H10`의 두뇌를 확장하는 강력한 알고리즘 개발 플랫폼입니다. 연구실에 갇혀 있던 훌륭한 아이디어들을 현실로 꺼내보세요.**

로봇 연구의 가장 큰 장벽은 복잡하고 긴 하드웨어 개발 과정입니다. `XM10`은 이미 검증된 `KIT H10`의 정밀한 하드웨어는 그대로 사용하면서, 제어 알고리즘을 사용자가 원하는 대로 자유롭게 설계하고 테스트할 수 있는 환경을 제공하며,
추가 생체형 신호 센서를 사용할 수 있는 확장성과 AI 외부 모듈과의 연계를 통해 이 장벽을 허뭅니다.

---

## 🎯 누구를 위한 플랫폼인가? (Target Audience)

`XM10`은 웨어러블 로봇 분야의 혁신을 꿈꾸는 모든 창작자를 위한 부분 개방형 R&D 플랫폼입니다.

* **대학 및 기업 연구원:** 차세대 재활 기술을 연구하고 논문으로 증명하고자 하는 분들.
* **의료기기 엔지니어:** 새로운 의료기기 아이디어를 신속하게 프로토타이핑하고 검증하려는 분들.
* **로봇 공학 학생:** 이론으로만 배우던 제어 알고리즘을 실제 로봇에서 구현하며 심도 있게 학습하려는 분들.

---

## ✨ `XM10`의 핵심 가치 (Core Features)

| 가치 | 설명 |
| :--- | :--- |
| **🧠 독자적 알고리즘 개발** | C 코드로 독자적인 제어 알고리즘을 제약 없이 이식하고 테스트할 수 있습니다. `PIF-Vectors`와 `Auxiliary Inputs`를 직접 인가하여 `KIT H10`의 모든 움직임을 원하는 대로 설계하세요. |
| **🧬 지능형 생체 신호 연동** | `EMG`, `GRF`, `FSR` 등 다양한 생체 신호 센서 허브를 `CAN-FD`로 손쉽게 연동하여, 사용자의 의도에 실시간으로 반응하는 지능형 시스템을 구축할 수 있습니다. |
| **📊 고해상도 데이터 분석** | 로봇과 센서의 모든 데이터를 **2ms 주기**로 USB 메모리에 직접 저장(`USB-MSC`)하거나, PC로 실시간 스트리밍(`USB-CDC`)하여 `MATLAB`, `Python` 등으로 정밀하게 분석할 수 있습니다. |
| **🤖 AI 기반 상위 제어 확장** | `Jetson Orin NX`와 같은 고성능 `AM(Application Module)`과 연동하여 , 강화학습, 머신러닝 기반의 차세대 상위 제어 알고리즘을 설계하고 통합할 수 있습니다. |

---

## 🏛️ 시스템 아키텍처

`XM10`은 인간의 동작 제어 시스템을 모방한 **계층적 분산 제어 아키텍처**를 기반으로 합니다. 이를 통해 복잡한 로봇 제어를 명확한 역할 분담으로 나누어 안정성과 확장성을 확보했습니다.
인간의 동작 제어 시스템을 모방한 `angel Robotics`에서 개발한 Module들의 시스템 아키텍처입니다.

<p align="center">
  <img width="712" height="351" alt="image" src="https://github.com/user-attachments/assets/911991bc-8c85-4590-8177-47c9af677cbf" />
</p>

그 중에서 `KIT H10`의 포함된 모터드라이버는 분산 제어에 특화된 형태의 시스템으로서 아래와 같은 구조로 설계되었습니다.
<p align="center">
  <img width="753" height="383" alt="image" src="https://github.com/user-attachments/assets/656dbcc5-6e87-495b-8d1c-9741060ee3d6" />
</p>

이러한 분산 제어 구조를 기반으로 `KIT H10`과 `XM10`의 전체 시스템 아키텍처가 다음과 같이 구성됩니다.

<p align="center">
  <img width="1934" height="987" alt="image" src="https://github.com/user-attachments/assets/29428128-6802-4bbf-ad9f-51a806e92d2b" />
</p>

결과적으로 실제 개발자가 마주하게 될 `XM10`의 내부 FW 시스템 아키텍처 구조는 아래와 같습니다.
<p align="center">
  <img width="1895" height="930" alt="image" src="https://github.com/user-attachments/assets/6f3e0f15-6865-459b-9fe2-6bf2cff27103" />
</p>

* **💻 Application Layer (사용자 영역):** `XM_Apps/User_Algorithm` 폴더에서 바로 여러분의 알고리즘을 설계하고 구현할 수 있습니다. 사용자는 오직 `XM API`만을 `#include`하여 로봇의 모든 기능을 제어합니다.
* **🧩 Façade Layer (XM API):** `파사드 패턴(Façade Pattern)`을 적용하여 복잡한 내부 시스템을 숨기고, 사용자가 단순하고 통일된 창구(API)를 통해 XM10의 모든 기능에 접근할 수 있도록 합니다.
* **🏗️ angel Robotics Library (제공 영역):** `System (PnP, Rx/Tx Task)`, `Middlewares (RTOS, USB)`, `Devices (센서 드라이버)`, `IOIF (HAL 래퍼)` 등 로봇 구동의 핵심 요소들은 안정화된 라이브러리 형태로 제공되거나 `Background Task` 형태로 동작됩니다.

---

## 🚀 Getting Started (시작하기)

간단하게 첫 번째 코드를 `KIT H10`에서 실행해볼 수 있습니다.

### **1. 개발 환경 구축**

* **STM32CubeIDE 설치:** ([링크](https://www.st.com/en/development-tools/stm32cubeide.html))에 **`[1.14.1]`** 버전을 설치해주세요. (경로에 한글이 없도록 주의)
* **Git 설치 및 레포지토리 Clone:**
    ```bash
    git clone [https://github.com/YourUsername/Extension_Module.git](https://github.com/YourUsername/Extension_Module.git)
    cd Extension_Module
    ```

### **2. 프로젝트 불러오기 및 빌드**

1.  `STM32CubeIDE`에서 `File > Import... > General > Existing Projects into Workspace`를 선택합니다.
2.  Clone 받은 `Extension_Module` 폴더를 `Root directory`로 지정하고 프로젝트를 불러옵니다.
3.  툴바의 <img width="21" height="23" alt="image" src="https://github.com/user-attachments/assets/06d3cdfb-4974-4e9e-8119-5e8a92e5b081" />(Build)을 클릭하여 에러 없이 빌드가 완료되는지 확인합니다.

### **3. 하드웨어 연결 및 펌웨어 업로드**

1.  `KIT H10`과 `XM10`을 전용 케이블로 연결합니다.
2.  PC와 ST-Link 디버거, 그리고 `XM10`의 `SWD` 포트를 연결합니다.
3.  `STM32CubeIDE`의 <img width="23" height="21" alt="image" src="https://github.com/user-attachments/assets/da49493b-a58f-4b43-9dc3-83ba26bdc7de" />(Debug)을 클릭하여 펌웨어를 업로드하고 디버깅 세션을 시작합니다.

> 📚 더 자세한 내용은 **[Quick Start Guide](/quick-start-guide)** 를 참고하세요.

---

## 📖 Tutorials (튜토리얼)

XM10의 강력한 기능들을 단계별로 마스터할 수 있도록 다양한 튜토리얼을 제공합니다.

### **Part 1: XM10 기본 기능 마스터하기**

가장 기초적인 입출력 제어부터 상태 기반 프로그래밍(FSM)까지, 임베디드 제어의 핵심을 익힙니다.

  * **[Example 01: 버튼과 LED 기초 (Basic)](/examples/01_button_led_basic)** \* 단순한 폴링(Polling) 방식으로 버튼을 눌러 LED를 켜고 끄는 법을 배웁니다.
  * **[Example 02: 이벤트와 특수 효과 (Event & Effect)](/examples/02_button_led_event)** \* 클릭(Click) 이벤트를 감지하고, 원샷(One-shot) 등의 LED 특수 효과를 다룹니다.
  * **[Example 03: 상태 머신 제어 (FSM)](/examples/03_button_led_fsm)** \* 롱 프레스(Long Press)로 모드를 전환하며, 체계적인 상태 머신(TSM)을 구현합니다.
  * **[Example 04: 외부 디지털 제어 (External GPIO)](/examples/04_ext_io_basic)** \* 확장 포트를 통해 외부 스위치와 LED 회로를 구성하고 제어합니다.
  * **[Example 05: 아날로그 센서 모니터링 (ADC)](/examples/05_ext_io_analog)** \* 가변저항이나 조도 센서의 전압 값을 읽고 임계치를 판단합니다.
  * **[Example 06: 외부 안전 제어 (Safety Logic)](/examples/06_ext_io_safety_control)** \* 리미트 스위치 등 외부 신호를 이용해 로봇을 비상 정지시키는 안전 로직을 구현합니다.

### **Part 2: XM10 USB 기능 마스터하기**

PC와의 실시간 통신 및 데이터 로깅 기능을 활용하여 개발 효율을 극대화합니다.

  * **[Example 07: USB 시리얼 통신 기초 (CDC Basic)](/examples/07_cdc_basic_print)** \* PC 터미널로 텍스트 메시지를 보내고 통신 연결을 확인합니다.
  * **[Example 08: USB 시리얼 통신 센서 데이터 모니터링 (CDC Sensor)](/examples/08_cdc_sensor_print)** \* 로봇의 센서 값을 문자열로 변환하여 실시간으로 확인합니다.
  * **[Example 09: USB 시리얼 통신 고속 데이터 스트리밍 (Binary Stream)](/examples/09_cdc_high_speed_stream)** \* Serial Plotter 등을 위해 500Hz 고속 데이터를 바이너리로 전송합니다.
  * **[Example 10: USB 메모리 사용자 정의 데이터 로깅 (MSC Logging)](/examples/10_msc_manual_log)** \* USB 메모리에 Binary 파일을 생성하고 데이터를 저장하는 방법을 익힙니다.

### **Part 3: KIT H10 제어 알고리즘 구현**

실제 웨어러블 로봇(SUIT H10)을 제어하는 고급 알고리즘을 구현합니다.

  * **[Example 11: 패시브 모드 (Passive Mode)](/examples/15_passive_mode)**
      * P-Vector를 사용하여 설정된 범위를 왕복하는 자동 운동 모드를 구현합니다.
  * **[Example 12: 액티브 어시스트 (Active Assist)](/examples/16_active_assist)**
      * 사용자의 의도(센서 데이터)를 파악하여 보행을 보조하는 토크 제어 알고리즘을 구현합니다.
  * **[Example 13: 저항 모드 (Resistive Mode)](/examples/13_resistive_mode)** \* H10 슈트의 내장 기능을 활용하여, 물속을 걷는 듯한 저항 운동 모드를 구현합니다.
  * **[Example 14: 중력 보상 (Gravity Compensation)](/examples/14_gravity_compensation)** *(Planned)*
      * 로봇의 무게를 상쇄하여 착용자가 무게감을 느끼지 않게 하는 투명 모드를 구현합니다.
  * **[Example 15: 통합 모드 (Total Application)](/examples/17_total_mode)** *(Planned)*
      * 위의 모든 모드를 버튼 하나로 전환하며 사용하는 완성된 애플리케이션을 만듭니다.

> 🎓 모든 튜토리얼과 예제 코드는 **[Examples](/examples)** 폴더에서 확인하실 수 있습니다.

---

## 📜 API Reference (API 레퍼런스)

사용자 알고리즘 개발에 필요한 모든 API 함수에 대한 상세 설명은 공식 API 문서에서 확인하실 수 있습니다.

> 👉 **[API Reference](/api-reference)**

---

## 🤝 기여하기 (Contributing)

본 프로젝트는 더 나은 웨어러블 로봇 기술 연구 생태계를 만들기 위한 오픈 플랫폼입니다. 버그 리포트, 기능 제안, 문서 개선 등 어떤 형태의 기여도 환영합니다. 자세한 내용은 `CONTRIBUTING.md` 파일을 참고해주세요.

---

## ⚠️ 개발 진행 상황 (Development Status)

본 프로젝트는 아직 상용화된 플랫폼이 아닌, 활발하게 연구 개발이 진행 중인 프로젝트입니다.

기능 개선, 버그 수정, 문서 업데이트 등이 수시로 이루어질 예정이므로, 최신 변경 사항을 확인하기 위해 주기적으로 프로젝트를 업데이트해주시기 바랍니다. 또한, 예고 없이 API나 프로젝트 구조가 변경될 수 있는 점 참고 부탁드립니다.

---

## 📄 라이선스 (License)

본 프로젝트는 [MIT License](/LICENSE)를 따릅니다.
