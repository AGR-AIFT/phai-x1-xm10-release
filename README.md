# Extension_Module: XM10 부분 개방형 R&D 플랫폼

<p align="center">
  <img width="348" height="271" alt="image" src="https://github.com/user-attachments/assets/797cb252-48a7-4d6c-aa9d-3d7ffda565de" />
</p>
<p align="center">
  <a href="/LICENSE"><img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License: MIT"></a>
  <a href="#"><img src="https://img.shields.io/badge/Platform-STM32H7-blue.svg" alt="Platform"></a>
  <a href="#"><img src="https://img.shields.io/badge/OS-FreeRTOS-orange.svg" alt="OS"></a>
</p>

**`XM10`은 `angel Robotics`의 고관절 보조 로봇 `SUIT H10`의 두뇌를 확장하는 강력한 알고리즘 개발 플랫폼입니다. 연구실에 갇혀 있던 훌륭한 아이디어들을 현실로 꺼내보세요.**

로봇 연구의 가장 큰 장벽은 복잡하고 긴 하드웨어 개발 과정입니다. `XM10`은 이미 검증된 `SUIT H10`의 정밀한 하드웨어는 그대로 사용하면서, 제어 알고리즘을 사용자가 원하는 대로 자유롭게 설계하고 테스트할 수 있는 환경을 제공하며,
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
| **🧠 독자적 알고리즘 개발** | C 코드로 독자적인 제어 알고리즘을 제약 없이 이식하고 테스트할 수 있습니다. `PIF-Vectors`와 `Auxiliary Inputs`를 직접 인가하여 `SUIT H10`의 모든 움직임을 원하는 대로 설계하세요. |
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

그 중에서 `SUIT H10`의 포함된 모터드라이버는 분산 제어에 특화된 형태의 시스템으로서 아래와 같은 구조로 설계되었습니다.
<p align="center">
  <img width="753" height="383" alt="image" src="https://github.com/user-attachments/assets/656dbcc5-6e87-495b-8d1c-9741060ee3d6" />
</p>

이러한 분산 제어 구조를 기반으로 `SUIT H10`과 `XM10`의 전체 시스템 아키텍처가 다음과 같이 구성됩니다.

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

간단하게 첫 번째 코드를 `SUIT H10`에서 실행해볼 수 있습니다.

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

1.  `SUIT H10`과 `XM10`을 전용 케이블로 연결합니다.
2.  PC와 ST-Link 디버거, 그리고 `XM10`의 `SWD` 포트를 연결합니다.
3.  `STM32CubeIDE`의 <img width="23" height="21" alt="image" src="https://github.com/user-attachments/assets/da49493b-a58f-4b43-9dc3-83ba26bdc7de" />(Debug)을 클릭하여 펌웨어를 업로드하고 디버깅 세션을 시작합니다.

> 📚 더 자세한 내용은 **[Quick Start Guide (문서 링크)]** 를 참고하세요.

---

## 📖 Tutorials (튜토리얼)

XM10의 강력한 기능들을 단계별로 마스터할 수 있도록 다양한 튜토리얼을 제공합니다.

* **Part 1: XM10 기본 기능 마스터하기**
    * [Tutorial 1: Hello, XM10! (LED & 버튼 제어)](/examples/01_Button_LED)
    * [Tutorial 2: 외부 디지털/아날로그 연결 (GPIO & ADC)](/examples/02_GPIO_ADC)
 
* **Part 2: XM10 USB 기능 마스터하기**
    * [Tutorial 3: PC와 실시간 데이터 모니터링 (USB-CDC 통신)](/examples/03_USB_CDC_Monitoring)
    * [Tutorial 4: USB 메모리에 데이터 파일로 기록 (USB-MSC 저장)](/examples/04_USB_MSC_Save)

* **Part 3: SUIT H10 제어 알고리즘 구현**
    * [Tutorial 5: SUIT-oriented 기능을 이용한 Resistive Mode 구현](/examples/05_Resisitve_Mode)
    * [Tutorial 6: SUIT-oriented 기능을 이용한 Gravitiy Compensaion 구현](/examples/06_Gravity_Compensation)
    * [Tutorial 7: PI Vector를 이용한 Passive Mode 구현](/examples/07_1_Passive_Mode)
    * [Tutorial 8: PI Vector & Aux Input을 이용한 Active-Assist Mode 구현](/examples/08_1_Active_Assist_Mode_Step)
    * [Tutorial 9: 3가지 Mode(Passive, Active-Assist, Resistive)를 이용한 Total Mode Application 구현](/examples/09_Total_Mode_Application)

> 🎓 모든 튜토리얼과 예제 코드는 **[/examples](/examples) 폴더**에서 확인하실 수 있습니다.

---

## 📜 API Reference (API 레퍼런스)

사용자 알고리즘 개발에 필요한 모든 API 함수에 대한 상세 설명은 공식 API 문서 사이트에서 확인하실 수 있습니다.

> 👉 **[XM10 공식 API 문서 사이트로 이동 (링크)]**

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
