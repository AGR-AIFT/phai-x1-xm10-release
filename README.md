# eXtension Module: XM10

<p align="center">
  <img width="348" height="271" alt="XM10 Board" src="https://github.com/user-attachments/assets/797cb252-48a7-4d6c-aa9d-3d7ffda565de" />
</p>
<p align="center">
  <a href="https://github.com/angel-robotics/Extension_Module/releases/tag/v2.0.0"><img src="https://img.shields.io/badge/Release-v2.0.0-brightgreen.svg" alt="Release"></a>
  <a href="#"><img src="https://img.shields.io/badge/Platform-STM32H7-blue.svg" alt="Platform"></a>
  <a href="#"><img src="https://img.shields.io/badge/OS-FreeRTOS-orange.svg" alt="OS"></a>
  <a href="#"><img src="https://img.shields.io/badge/Comm-CAN--FD-red.svg" alt="CAN-FD"></a>
  <a href="/LICENSE"><img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License: MIT"></a>
</p>

**`XM10`은 angel Robotics의 고관절 보조 로봇 `KIT H10`의 두뇌를 확장하는 알고리즘 개발 플랫폼입니다.**

연구실에 갇혀 있던 아이디어를 실제 로봇에서 실현하세요. 검증된 하드웨어 위에 사용자 알고리즘을 자유롭게 설계하고, 생체 신호 센서를 연동하며, AI 모듈과의 연계까지 확장할 수 있습니다.

> **대학 및 기업 연구원** · **의료기기 엔지니어** · **로봇 공학 학생** 을 위한 부분 개방형 R&D 플랫폼

---

## 핵심 기능

| 기능 | 설명 |
| :--- | :--- |
| **독자적 알고리즘 개발** | C 코드로 제어 알고리즘을 제약 없이 이식. PIF-Vectors와 Auxiliary Inputs로 KIT H10의 움직임을 설계 |
| **생체 신호 연동** | EMG, GRF, FSR 등 센서 허브를 CAN-FD로 손쉽게 연동. 사용자 의도에 실시간 반응하는 시스템 구축 |
| **고해상도 데이터 분석** | 1ms 주기로 USB 메모리 저장(MSC) 또는 PC 실시간 스트리밍(CDC). MATLAB, Python으로 정밀 분석 |
| **AI 기반 상위 제어** | Jetson Orin NX, AGX 등과 연동하여 강화학습, 머신러닝 기반 상위 제어 알고리즘 통합 |

---

## 시스템 아키텍처

<p align="center">
  <img width="1895" height="930" alt="XM10 FW Architecture" src="https://github.com/user-attachments/assets/6f3e0f15-6865-459b-9fe2-6bf2cff27103" />
</p>

* **Application Layer** — `XM_Apps/User_Algorithm`에서 `XM API`만으로 로봇의 모든 기능을 제어
* **Facade Layer (XM API)** — 복잡한 내부를 숨기고 단순한 창구(API)를 제공하는 파사드 패턴
* **angel Robotics Library** — System, Services, Middlewares, Devices, IOIF가 라이브러리로 제공

> 전체 아키텍처 상세: **[Architecture](docs/architecture/)**

---

## 빠른 시작 (Quick Start)

### 1. 개발 환경 구축

* [STM32CubeIDE **v2.0.0** 이상](https://www.st.com/en/development-tools/stm32cubeide.html) 설치
* 레포지토리 Clone:

```bash
git clone https://github.com/AGR-EXO/Extension_Module.git C:\XM_SDK
```

> **경로 주의사항**
> * 프로젝트를 **짧은 경로**에 Clone하세요 (예: `C:\XM_SDK\`, `D:\Projects\XM\`)
> * `C:\Users\...\OneDrive\...\` 같은 깊은 경로는 Windows MAX_PATH 제한으로 빌드 오류 발생 가능
> * 경로에 **한글, 공백, 특수문자**가 포함되지 않도록 주의
>
> 상세 내용: [Troubleshooting - 경로 문제](docs/troubleshooting.md#경로-길이-문제-windows-max_path)

### 2. 프로젝트 빌드

1. STM32CubeIDE → `File > Import... > Existing Projects into Workspace`
2. Clone 받은 `XM10_SDK/Extension_Module` 폴더를 Root directory로 지정
3. <img width="21" height="23" alt="Build" src="https://github.com/user-attachments/assets/06d3cdfb-4974-4e9e-8119-5e8a92e5b081" /> Build 클릭 → 에러 없이 완료 확인

### 3. 펌웨어 업로드

1. KIT H10 ↔ XM10 연결, ST-Link 디버거 ↔ XM10 SWD 포트 연결
2. <img width="23" height="21" alt="Debug" src="https://github.com/user-attachments/assets/da49493b-a58f-4b43-9dc3-83ba26bdc7de" /> Debug 클릭 → 펌웨어 업로드 및 디버깅 시작

> 단계별 상세 가이드: **[Getting Started](docs/getting-started/)**

> **KIT H10 FW 호환성:** XM FW v2.0.0은 **KIT H10 FW v2.3.0** 이상이 필요합니다. H10 FW가 이전 버전이라면 먼저 업데이트하세요. → **[KIT H10 Firmware 가이드](docs/kit-h10-firmware/)**

---

## 문서

| 문서 | 설명 |
| :--- | :--- |
| **[Getting Started](docs/getting-started/)** | 하드웨어 연결부터 첫 빌드까지 3단계 가이드 |
| **[Tutorials](docs/tutorials/)** | 20개 예제로 배우는 단계별 학습 (기초 → USB → 로봇 제어) |
| **[API Reference](docs/api-reference/)** | XM API 전체 함수 명세 |
| **[KIT H10 Firmware](docs/kit-h10-firmware/)** | H10 FW/ContentsFiles 버전 호환성 및 업데이트 가이드 |
| **[Examples](examples/)** | 예제 소스 코드 (각 폴더에 README 포함) |
| **[Python Tools](PythonDecoder/)** | CDC/MSC 데이터 수신, 분석, 디코딩 도구 |
| **[Architecture](docs/architecture/)** | 시스템 아키텍처, 프로토콜, FW 레이어 구조 |
| **[Troubleshooting](docs/troubleshooting.md)** | FAQ 및 빌드 오류 해결 |
| **[Changelog](CHANGELOG.md)** | 버전별 변경 이력 |

---

## 개발 진행 상황

본 프로젝트는 활발하게 연구 개발이 진행 중입니다. 기능 개선, 버그 수정, 문서 업데이트가 수시로 이루어질 예정이므로 주기적으로 업데이트해 주시기 바랍니다.

버그 리포트, 기능 제안, 문서 개선 등 어떤 형태의 기여도 환영합니다.

---

## 라이선스

본 프로젝트는 [MIT License](/LICENSE)를 따릅니다.
