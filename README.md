# eXtension Module: XM10

<p align="center">
  <img width="332" height="231" alt="XM10 Board" src="https://github.com/user-attachments/assets/871dc578-57ab-41ed-8d39-76a43e65f24d" />
</p>
<p align="center">
  <a href="https://github.com/angel-robotics/Extension_Module/releases/tag/v2.0.0"><img src="https://img.shields.io/badge/Release-v2.0.0-brightgreen.svg" alt="Release"></a>
  <a href="#"><img src="https://img.shields.io/badge/Platform-STM32H7-blue.svg" alt="Platform"></a>
  <a href="#"><img src="https://img.shields.io/badge/OS-FreeRTOS-orange.svg" alt="OS"></a>
  <a href="#"><img src="https://img.shields.io/badge/Comm-CAN--FD-red.svg" alt="CAN-FD"></a>
  <a href="#"><img src="https://img.shields.io/badge/Examples-41-success.svg" alt="Examples"></a>
  <a href="/LICENSE"><img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License: MIT"></a>
</p>

<p align="center">
  <b>angel Robotics 고관절 보조 로봇 <code>KIT H10</code> 의 두뇌를 확장하는 알고리즘 개발 보드</b>
</p>

<p align="center">
  검증된 외골격 위에 자기 알고리즘을 자유롭게 올려보고, 센서를 붙이고, 데이터를 분석할 수 있습니다.<br>
  대학·기업 연구원, 의료기기 엔지니어, 로봇 공학 전공 학생을 위한 부분 개방형 R&D 플랫폼.
</p>

---

## 시작하기

### AI 와 함께 (가장 빠릅니다)

STM32CubeIDE 도, 임베디드도 처음이어도 괜찮습니다. **Claude Code** 가 설치부터 보드 LED 점등까지 대화로 안내해줍니다.

```
1. Claude Code 설치    →   https://claude.com/claude-code
2. 레포 폴더에서 실행   →   claude
3. AI 에게 한 줄        →   "처음 시작할게"   또는   /student-onboard
```

자세한 흐름: [docs/getting-started/00-claude-code-quickstart.md](docs/getting-started/00-claude-code-quickstart.md)

### 직접 진행하고 싶다면

```bash
# 1. Clone — 한글 없는 짧은 경로에
git clone https://github.com/AGR-EXO/Extension_Module.git C:\dev\Extension_Module

# 2. STM32CubeIDE 에서 Import
#    File → Import → Existing Projects into Workspace
#    Root: C:\dev\Extension_Module\XM10_SDK\Rev2.0\Extension_Module\

# 3. Build (Ctrl+B) → Debug (F11)
```

단계별 상세: [docs/getting-started/](docs/getting-started/) (하드웨어 → 환경 구축 → 첫 빌드)

> **H10 펌웨어 버전 확인**: XM v2.0.0 은 **KIT H10 v2.3.0** 과 짝입니다. 구버전이면 먼저 → [KIT H10 Firmware 가이드](docs/kit-h10-firmware/)

---

## 무엇을 만들 수 있나

- **외골격 제어 알고리즘** — C 코드로 직접 작성. 사전 정의 움직임 명령부터 직접 토크 계산까지 자유.
- **센서 통합** — EMG, 발 접지 (GRF), FSR 같은 센서 허브를 CAN-FD 로 바로 붙여서 사용자 의도에 실시간 반응.
- **데이터 수집** — 1 ms 주기 USB 메모리 로깅 또는 PC 실시간 스트리밍. MATLAB / Python 으로 바로 분석.
- **AI 상위 제어** — Jetson 시리즈 연동 강화학습부터 MCU 안에서 직접 돌리는 Tiny NN 까지.

---

## 시스템 큰 그림

<p align="center">
  <img width="1895" height="930" alt="XM10 FW Architecture" src="https://github.com/user-attachments/assets/6f3e0f15-6865-459b-9fe2-6bf2cff27103" />
</p>

내가 손대는 곳은 한 곳뿐입니다 — `XM_Apps/User_Algorithm/user_app.c`. 그 외 CAN 통신, USB 송수신, 외골격 데이터 파싱 같은 일은 모두 XM 라이브러리가 자동으로 처리합니다. 매 1 ms 마다 내 `User_Loop()` 가 호출되고, 그 안에서 `XM.status.h10.*` 로 센서를 읽고 `XM_SetAssistTorque*()` 같은 함수로 명령을 보내는 게 전부예요.

> 자세한 흐름 (1 ms 제어 루프, 시작 시퀀스, 내 코드 위치): **[docs/architecture/](docs/architecture/)**

---

## 학습 경로

41 개의 예제 모두 같은 형식으로 정돈되어 있어요 — 목표, 사전 지식, 핵심 코드, 실험, 흔한 실수. 한 예제는 30 분 안에 끝낼 수 있도록 설계했습니다.

| 수준 | 추천 순서 | 시간 |
| :--- | :--- | :--- |
| 입문 | Ex.00 → 01 → 04 → 07 → 10a → 11 | 3 시간 정도 |
| 중급 | Ex.02 → 05b → 08 → 10b → 12 → 14 → 18 | 1 주 정도 |
| 고급 | Ex.03 → 09 → 10c → 15 → 16 → 17 → 19 → 20+ | 한 학기 |
| Physical AI 응용 | Ex.21 → 31 → 32 → 33 → 36 | 자기주도 |

전체 인덱스 + 난이도별 정리: **[docs/tutorials/](docs/tutorials/)** · 예제 카탈로그: **[examples/](examples/)**

---

## 전체 문서

| 문서 | 설명 |
| :--- | :--- |
| [Getting Started](docs/getting-started/) | 하드웨어 연결, 환경 구축, 첫 빌드 — 3 단계 |
| [Tutorials](docs/tutorials/) | 41 개 예제 학습 로드맵 |
| [API Reference](docs/api-reference/) | XM 함수 전체 명세 + 흔한 실수 |
| [Architecture](docs/architecture/) | 내 코드가 어디서 어떻게 동작하는지 |
| [KIT H10 Firmware](docs/kit-h10-firmware/) | H10 펌웨어/컨텐츠 호환성 + 업데이트 |
| [Bootloader](docs/bootloader/) | 펌웨어 업로드 방법 (SWD 직접 / USB) |
| [Advanced Topics](docs/advanced/) | 관심 분야별 자기주도 학습 경로 |
| [Troubleshooting](docs/troubleshooting.md) | 자주 마주치는 문제 정리 |
| [Examples](examples/) | 41 개 예제 (각 폴더에 5 단계 README) |
| [Python Tools](PythonDecoder/) | USB 시리얼/메모리 디코더, MATLAB 변환 |
| [Changelog](CHANGELOG.md) | 버전별 변경 이력 |

---

## 참여 + 지원

활발히 연구 개발 중인 프로젝트입니다. 기능 개선, 버그 수정, 문서 보강이 수시로 일어나니 가끔 업데이트 받아주세요.

- 버그 리포트 · 기능 제안 → [GitHub Issues](https://github.com/AGR-EXO/Extension_Module/issues)
- Q&A · 사용 사례 공유 → [GitHub Discussions](https://github.com/AGR-EXO/Extension_Module/discussions)
- "Ex.XX 가 안 돼" 트러블슈팅 → Claude Code 에서 `example-helper` 호출

---

## 라이선스

[MIT License](/LICENSE)
