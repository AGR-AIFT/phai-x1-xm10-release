# XM10 Documentation

XM10 플랫폼의 전체 문서 인덱스입니다. 아래 학습 경로를 따라 기초부터 심화까지 단계적으로 학습할 수 있습니다.

---

## 학습 로드맵

```
┌─────────────────────────────────────────────────────────────────────┐
│                                                                     │
│   Getting Started ──► Tutorials Part 1 ──► Tutorials Part 2         │
│   (환경 구축)          (기본 I/O)            (USB 통신/로깅)         │
│                                                    │                │
│                                                    ▼                │
│   API Reference ◄── Tutorials Part 3 ◄────────────┘                │
│   (함수 명세 참조)    (로봇 제어)                                    │
│                            │                                        │
│                            ▼                                        │
│                      Tutorials Part 4 ──► Advanced Topics           │
│                      (심화 프로젝트)       (로드맵/연구)              │
│                            │                                        │
│                            ▼                                        │
│                      Architecture                                   │
│                      (시스템 이해)                                    │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 문서 구조

### 1. [Getting Started](getting-started/) — 시작하기

XM10 개발을 위한 환경 구축 가이드입니다. 처음 사용하시는 분은 여기부터 시작하세요.

| 순서 | 문서 | 내용 |
| :---: | :--- | :--- |
| 1 | [하드웨어 연결](getting-started/01-hardware-setup.md) | KIT H10, ST-Link, 센서 허브 연결 |
| 2 | [개발 환경 구축](getting-started/02-software-setup.md) | STM32CubeIDE 설치, GitHub Clone |
| 3 | [첫 빌드 & 실행](getting-started/03-first-build.md) | 프로젝트 Import, 빌드, 펌웨어 업로드 |

---

### 2. [Tutorials](tutorials/) — 단계별 학습

27개의 예제를 통해 XM10의 기능을 단계적으로 마스터합니다.

| Part | 주제 | 예제 | 난이도 |
| :---: | :--- | :--- | :---: |
| **Part 0** | Quick Start | Ex.00 (1개) | 입문 |
| **Part 1** | 기본 I/O 제어 | Ex.01 ~ Ex.06 (10개) | 초급 ~ 고급 |
| **Part 2** | USB 통신 & 데이터 로깅 | Ex.07 ~ Ex.10c (7개) | 초급 ~ 고급 |
| **Part 3** | KIT H10 로봇 제어 | Ex.11 ~ Ex.13 (3개) | 응용 |
| **Part 4** | 심화 프로젝트 | Ex.14 ~ Ex.19 (6개) | 중급 ~ 고급 |

> 난이도별 학습 경로와 전체 예제 목록은 **[Tutorials README](tutorials/)** 또는 **[Examples README](../examples/README.md)** 에서 확인하세요.

---

### 3. [API Reference](api-reference/) — 함수 명세

사용자 알고리즘 개발에 필요한 `XM API` 전체 함수의 상세 설명입니다.

| 문서 | 범위 |
| :--- | :--- |
| [Task State Machine](api-reference/01-task-state-machine.md) | TSM 생성, 상태 등록, 전이 제어 |
| [H10 Control & Data](api-reference/02-h10-control-n-data.md) | 로봇 센서 읽기, 토크/위치 명령, 모드 전환 |
| [LED & Button](api-reference/03-led-btn-control.md) | 보드 LED 제어, 버튼 상태/이벤트 감지 |
| [External I/O](api-reference/04-external-io.md) | GPIO/ADC 제어, DIO↔ADC 전환 |
| [USB Connectivity](api-reference/05-usb-connectivity.md) | CDC 시리얼 통신, MSC 데이터 로깅 |

---

### 4. [KIT H10 Firmware](kit-h10-firmware/) — 펌웨어 & 컨텐츠 업데이트

XM10과 연동되는 KIT H10의 펌웨어 및 SD카드 컨텐츠 파일 업데이트 가이드입니다.

| 내용 | 설명 |
| :--- | :--- |
| [버전 호환성 매트릭스](kit-h10-firmware/#버전-호환성-매트릭스) | XM FW ↔ H10 FW 대응 버전 확인 |
| [펌웨어 업데이트](kit-h10-firmware/#펌웨어-업데이트-usb-stick-방식) | USB Stick을 이용한 CM/SAM10 FW 업데이트 |
| [컨텐츠 파일 업데이트](kit-h10-firmware/#컨텐츠-파일-업데이트-sd카드-방식) | SD카드 교체를 통한 컨텐츠 파일 업데이트 |

> **XM FW 버전을 변경할 때는 반드시 대응하는 H10 FW/ContentsFiles를 함께 업데이트하세요.**

---

### 5. [Architecture](architecture/) — 시스템 아키텍처

XM10의 하드웨어 및 펌웨어 레이어 구조, 통신 프로토콜, 제어 루프를 설명합니다.

---

### 6. [Advanced Topics](advanced/) — 심화 주제

센서 허브 연동, PhAI 프로토콜, TinyML 등 고급 활용법을 다룹니다.

---

### 7. [Troubleshooting](troubleshooting.md) — FAQ & 문제 해결

빌드 오류, USB 연결, CAN-FD 통신 등 자주 발생하는 문제의 해결 방법입니다.

---

## 추가 리소스

| 리소스 | 설명 |
| :--- | :--- |
| [Examples](../examples/) | 27개 예제 소스 코드 (각 폴더에 README 포함) |
| [Python Tools](../PythonDecoder/) | CDC 수신기, CSV 분석기, MSC 바이너리 디코더 |
| [Changelog](../CHANGELOG.md) | 버전별 변경 이력 |
| [XM10 SDK](../XM10_SDK/) | SDK 프로젝트 (libXM_Lib.a + 헤더 + user_app.c) |
