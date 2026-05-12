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
  <b>angel Robotics 의 고관절 보조 로봇 <code>KIT H10</code> 두뇌를 확장하는 알고리즘 개발 플랫폼</b>
</p>

<p align="center">
  검증된 외골격 하드웨어 위에 사용자 알고리즘을 자유롭게 설계.<br>
  생체 신호 센서 연동, AI 모듈 확장, 실시간 데이터 분석까지 — <b>한 보드에서 끝</b>.
</p>

<p align="center">
  <i>대학 · 기업 연구원 · 의료기기 엔지니어 · 로봇 공학 학생 을 위한 부분 개방형 R&D 플랫폼</i>
</p>

---

## 🚀 시작하기

### 🤖 AI 와 함께 (권장 — 처음이라면)

`STM32CubeIDE` / 임베디드 경험이 전혀 없어도 OK. **Claude Code** 가 환경 구축부터 LED 점등까지 6 단계를 대화로 안내합니다.

```
1. Claude Code 설치    →   https://claude.com/claude-code
2. 본 레포에서 실행    →   claude
3. AI 에게 한 줄       →   "처음 시작할게"   또는   /student-onboard
```

> 📖 상세 가이드: **[docs/getting-started/00-claude-code-quickstart.md](docs/getting-started/00-claude-code-quickstart.md)**

### 🛠️ 수동 진행 (AI 미사용)

```bash
# 1. Clone — 짧고 한글 없는 경로 권장
git clone https://github.com/AGR-EXO/Extension_Module.git C:\dev\Extension_Module

# 2. STM32CubeIDE 에서 Import
#    File → Import → Existing Projects into Workspace
#    Root: C:\dev\Extension_Module\XM10_SDK\Rev2.0\Extension_Module\

# 3. Build (Ctrl+B) → Debug (F11)
```

> 📖 단계별 상세: **[docs/getting-started/](docs/getting-started/)** (Hardware → Software → First Build)
>
> ⚠️ **H10 펌웨어 호환성**: XM v2.0.0 ↔ **KIT H10 v2.3.0** 필수. 구버전이면 → [KIT H10 Firmware 가이드](docs/kit-h10-firmware/)

---

## 🛠️ 무엇을 만들 수 있나

| 영역 | 내용 |
| :--- | :--- |
| 🦾 **외골격 제어 알고리즘** | C 코드로 직접 설계. PI-Vector + Auxiliary Inputs 로 KIT H10 움직임 완전 제어 |
| 🧠 **생체 신호 통합** | EMG / GRF / FSR 센서 허브 CAN-FD 직결, 사용자 의도에 실시간 반응 |
| 📈 **고해상도 데이터** | 1 ms 주기 USB 메모리 로깅 (MSC) + PC 실시간 스트리밍 (CDC), MATLAB / Python 자동 분석 |
| 🤖 **AI 상위 제어** | Jetson Orin NX / AGX 연동 강화학습 + 머신러닝 + MCU 내장 Tiny NN |

---

## 🧱 시스템 아키텍처

<p align="center">
  <img width="1895" height="930" alt="XM10 FW Architecture" src="https://github.com/user-attachments/assets/6f3e0f15-6865-459b-9fe2-6bf2cff27103" />
</p>

| 레이어 | 역할 | 학생 작업? |
| :--- | :--- | :-: |
| **Application** | `User_Algorithm/user_app.c` — XM API 만으로 로봇 전체 제어 | ✅ |
| **Facade (XM API)** | 복잡한 내부를 숨기는 단일 API 창구 (파사드 패턴) | 호출만 |
| **angel Robotics Library** | System / Services / Devices / IOIF — `.a` 라이브러리 제공 | 사용만 |
| **Middlewares + HAL** | FreeRTOS, USB CDC/MSC, FatFs, STM32H7 HAL | — |

> 📖 IPO 제어 루프 + RTOS Task 우선순위 + 호출 체인 상세: **[docs/architecture/](docs/architecture/)**

---

## 📚 학습 경로

41 개의 예제가 **5-step Lab Manual** 형식 (목표 → 사전 지식 → 핵심 코드 → 실험 → 다음 단계 + ⚠️) 으로 통일되어 있습니다.

| 단계 | 추천 경로 | 소요 시간 |
| :--- | :--- | :--- |
| **🚀 입문** | Ex.00 → 01 → 04 → 07 → 10a → 11 | ~3 시간 |
| **🛠️ 중급** | Ex.02 → 05b → 08 → 10b → 12 → 14 → 18 | ~1 주 |
| **🧠 고급** | Ex.03 → 09 → 10c → 15 → 16 → 17 → 19 → 20+ | 수업 학기 |
| **🤖 응용 (Physical AI)** | Ex.21 → 31 → 32 → 33 → 36 | 자기주도 |

> 📖 7 개 Part 전체 인덱스 + 난이도 별: **[docs/tutorials/](docs/tutorials/)**
> 📖 예제 41 개 카탈로그: **[examples/](examples/)**

---

## 📖 전체 문서

| 문서 | 설명 |
| :--- | :--- |
| **[Getting Started](docs/getting-started/)** | 하드웨어 연결 → 환경 구축 → 첫 빌드 (3 단계) |
| **[Tutorials](docs/tutorials/)** | 41 개 예제 학습 로드맵 (난이도/시간/추천 경로) |
| **[API Reference](docs/api-reference/)** | XM API 8 그룹 전체 함수 명세 + ⚠️ 흔한 실수 |
| **[Architecture](docs/architecture/)** | FW 레이어 + IPO 제어 루프 + RTOS Task |
| **[KIT H10 Firmware](docs/kit-h10-firmware/)** | H10 FW + ContentsFiles 호환성 + 업데이트 |
| **[Bootloader](docs/bootloader/)** | AGR_BOOT V2 — SWD + PhAI Studio FTP |
| **[Advanced Topics](docs/advanced/)** | 자기주도 학습 권장 경로 6 분야 |
| **[Troubleshooting](docs/troubleshooting.md)** | 빌드/하드웨어/통신 오류 학생 친화 카탈로그 |
| **[Examples](examples/)** | 예제 41 개 (각 폴더에 5-step README) |
| **[Python Tools](PythonDecoder/)** | CDC/MSC 디코더, MATLAB 변환 |
| **[Changelog](CHANGELOG.md)** | 버전별 변경 이력 |

---

## 🤝 참여 + 지원

본 프로젝트는 활발히 연구 개발 중. 기능 개선 · 버그 수정 · 문서 보강이 수시로 진행됩니다.

| 채널 | 용도 |
| :--- | :--- |
| [GitHub Issues](https://github.com/AGR-EXO/Extension_Module/issues) | 버그 리포트 · 기능 제안 |
| [GitHub Discussions](https://github.com/AGR-EXO/Extension_Module/discussions) | Q&A · 사용 사례 공유 |
| Claude Code 안의 `example-helper` 스킬 | "Ex.XX 가 안 돼" 즉시 트러블슈팅 |

---

## 라이선스

[MIT License](/LICENSE)
