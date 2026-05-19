# eXtension Module: XM10

<p align="center">
  <img width="332" height="231" alt="XM10 Board" src="https://github.com/user-attachments/assets/871dc578-57ab-41ed-8d39-76a43e65f24d" />
</p>
<p align="center">
  <a href="https://github.com/AGR-EXO/Extension_Module/releases/tag/v2.2.1"><img src="https://img.shields.io/badge/Release-v2.2.1-brightgreen.svg" alt="Release"></a>
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

> **🧭 길 찾기** — 뭘 찾는지는 알겠는데 어디 있는지 모르겠다면 **[docs/find-it.md](docs/find-it.md)** 한 페이지에 다 있어요. "버튼 제어 API 어디?", "Ex.07 안 돼", "보드 핀맵" 같은 자주 묻는 키워드 → 정답 페이지 직행.

---

## 시작하기

본 SDK 는 **GitHub Releases 에서 ZIP 으로 배포**됩니다. git clone 이 아닌 **본인 보드 리비전 ZIP 만 다운로드**하면 됩니다.

📦 **다운로드**: [Releases v2.2.1](https://github.com/AGR-EXO/Extension_Module/releases/tag/v2.2.1) → Assets 섹션
- **Rev2.0 보드** → `Rev2.0.zip` (대부분 여기)
- **Rev1.1 보드** → `Rev1.1.zip`
- 보드 라벨로 본인 리비전 확인 후 선택. 모호하면 → [docs/hardware/README.md - 보드 리비전 비교](docs/hardware/README.md#보드-리비전-비교)

### AI 와 함께 (가장 빠릅니다)

STM32CubeIDE 도, 임베디드도 처음이어도 괜찮습니다. **Claude Code** 가 설치부터 보드 LED 점등까지 대화로 안내해줍니다.

```
1. SDK ZIP 다운로드     →   Releases 에서 본인 Rev 의 ZIP
2. 압축 해제            →   C:\dev\Extension_Module  (한글·공백 경로 금지)
3. Claude Code 설치     →   https://claude.com/claude-code
4. 압축 푼 폴더에서 실행 →   claude
5. AI 에게 한 줄         →   "처음 시작할게"   또는   /student-onboard
```

자세한 흐름: [docs/getting-started/00-claude-code-quickstart.md](docs/getting-started/00-claude-code-quickstart.md)

### 직접 진행하고 싶다면

```bash
# 1. SDK ZIP 다운로드
#    https://github.com/AGR-EXO/Extension_Module/releases/latest
#    → Rev2.0.zip (또는 Rev1.1.zip) 선택

# 2. 압축 해제 — 한글·공백 없는 짧은 경로에
#    예: C:\dev\Extension_Module\
#    압축 풀면 ZIP root 에 .project, CLAUDE.md, docs/, examples/, .claude/ 가 함께 있음

# 3. STM32CubeIDE 에서 Import
#    File → Import → Existing Projects into Workspace
#    Root: 압축 해제 폴더 자체 (C:\dev\Extension_Module\ — .project 가 여기에 있음)

# 4. Build (Ctrl+B) → Debug (F11)
```

> 💡 본 SDK 는 **Rev 별로 독립 ZIP 배포** 입니다. Rev2.0.zip 을 풀면 Rev 2.0 SDK 한 벌이 ZIP root 에 평탄화되어 들어있고, Rev1.1.zip 도 마찬가지로 Rev 1.1 SDK 만 들어있습니다. 보드 라벨로 본인 Rev 확인 후 정확한 ZIP 선택하세요.

단계별 상세: [docs/getting-started/](docs/getting-started/) (하드웨어 → 환경 구축 → 첫 빌드)

> **H10 펌웨어 버전 확인**: XM v2.2.1 은 **KIT H10 v2.3.0** 과 짝입니다. 구버전이면 먼저 → [KIT H10 Firmware 가이드](docs/kit-h10-firmware/)

---

## 무엇을 만들 수 있나

- **외골격 제어 알고리즘** — C 코드로 직접 작성. 사전 정의 움직임 명령부터 직접 토크 계산까지 자유.
- **센서 통합** — EMG, 발 접지 (GRF), FSR 같은 센서 허브를 CAN-FD 로 바로 붙여서 사용자 의도에 실시간 반응.
- **데이터 수집** — 1 ms 주기 USB 메모리 로깅 또는 PC 실시간 스트리밍. MATLAB / Python 으로 바로 분석.
- **AI 상위 제어** — Jetson 시리즈 연동 강화학습부터 MCU 안에서 직접 돌리는 Tiny NN 까지.

---

## 시스템 큰 그림

<p align="center">
  <img width="1984" height="900" alt="xm10-system-architecture" src="https://github.com/user-attachments/assets/9fd4fc8b-6e4f-412e-aa83-3dff5f408362" />
</p>

내가 손대는 곳은 한 곳뿐입니다 — `XM_Apps/ 영역`. 그 외 CAN 통신, USB 송수신, 외골격 데이터 파싱 같은 일은 모두 XM 라이브러리가 자동으로 처리합니다. 매 1 ms 마다 `Control_Loop()` 가 호출되고, 그 안에서 `XM.status.h10.*` 로 센서를 읽고 `XM_SetAssistTorque*()` 같은 함수로 명령을 보내는 게 전부예요.

> 자세한 흐름 (1 ms 제어 루프, 시작 시퀀스, 내 코드 위치): **[docs/architecture/](docs/architecture/)**

---

## 학습 경로

41 개의 예제 모두 같은 형식으로 정돈되어 있어요 — 목표, 사전 지식, 핵심 코드, 실험, 흔한 실수. 한 예제는 30 분 안에 끝낼 수 있도록 설계했습니다.

| 수준 | 추천 순서 | 시간 |
| :--- | :--- | :--- |
| 입문 | Ex.00 → 01 → 04 → 07 → 10a → 11 | 3 시간 정도 |
| 중급 | Ex.02 → 05b → 08 → 10b → 12 → 14 → 18 | 1 주 정도 (하루 1~2 시간) |
| 고급 | Ex.03 → 09 → 10c → 15 → 16 → 17 → 19 → 20+ | 한 학기 (예제당 1~2 주씩 깊이) |
| Physical AI 응용 | Ex.21 → 31 → 32 → 33 → 36 | 자기주도 |

전체 인덱스 + 난이도별 정리: **[docs/tutorials/](docs/tutorials/)** · 한 학기 수업 진도표 예시도 같은 페이지 · 예제 카탈로그: **[examples/](examples/)**

---

## 폴더 구조 한눈에

> 본 트리는 **GitHub 레포 전체 구조** (사내 개발 view). 학생이 받는 **ZIP 다운로더 (Rev1.1.zip / Rev2.0.zip)** 는 본인 Rev SDK 한 벌이 ZIP root 에 평탄화되어 있고, `docs/`, `examples/`, `.claude/`, `CLAUDE.md` (Rev 특화) 가 함께 들어있습니다. 학생은 압축 푼 폴더 안에서만 작업합니다.

```
Extension_Module/
├── README.md              ← 지금 보는 페이지
├── docs/                  ← 학생용 문서 (9 카테고리)
│   ├── find-it.md         ← 🧭 빠른 찾기 인덱스 (먼저 가보세요)
│   ├── getting-started/   ← 환경 구축 + 첫 빌드
│   ├── hardware/          ← 보드 외부 인터페이스 + GPIO 핀맵
│   ├── tutorials/         ← 41 개 예제 학습 흐름 + 16 주 진도표
│   ├── api-reference/     ← XM 함수 전체 명세
│   ├── architecture/      ← 내 코드가 언제·어디서 동작하는지
│   ├── advanced/          ← AI 데이터 파이프라인 + 심화 트랙
│   ├── bootloader/        ← 펌웨어 업로드 방법
│   ├── kit-h10-firmware/  ← H10 외골격 펌웨어 호환성
│   ├── release-notes/     ← 버전별 첨부 파일 + 호환성 매트릭스
│   └── troubleshooting.md ← 자주 마주치는 문제 모음
├── examples/              ← 41 개 실습 예제 (각 폴더에 README)
├── XM10_SDK/              ← STM32CubeIDE 프로젝트
│   ├── Rev1.1/Extension_Module/  ← PCB Rev 1.1 용
│   └── Rev2.0/Extension_Module/  ← PCB Rev 2.0 용 (대부분 여기)
├── PythonDecoder/         ← USB 메모리 / 시리얼 디코더
├── assets/img/            ← 보드 사진 + 다이어그램 (사용자가 추가)
├── .claude/skills/        ← Claude Code 학생 온보딩 / 예제 트러블슈팅 스킬
├── CLAUDE.md / AGENTS.md  ← AI 코딩 도구 진입점 (Claude Code 등)
├── CHANGELOG.md           ← 버전별 변경 이력
└── LICENSE
```

내가 손대는 코드 위치: `XM_Apps/User_Algorithm/user_app.c` (ZIP 압축 푼 폴더 기준). 그 외는 라이브러리가 알아서.

## 전체 문서

| 문서 | 설명 |
| :--- | :--- |
| **[🧭 빠른 찾기](docs/find-it.md)** | **"어디 가야 하지" 막힐 때 첫 번째 출구** |
| [Getting Started](docs/getting-started/) | 하드웨어 연결, 환경 구축, 첫 빌드 — 3 단계 |
| [Hardware](docs/hardware/) | 보드 외부 인터페이스 + 외부 GPIO 핀맵 (Rev 별) |
| [Tutorials](docs/tutorials/) | 41 개 예제 학습 로드맵 + 한 학기 진도표 |
| [API Reference](docs/api-reference/) | XM 함수 전체 명세 + 흔한 실수 |
| [Architecture](docs/architecture/) | 내 코드가 어디서 어떻게 동작하는지 |
| [KIT H10 Firmware](docs/kit-h10-firmware/) | H10 펌웨어/컨텐츠 호환성 + 업데이트 |
| [Bootloader](docs/bootloader/) | 펌웨어 업로드 방법 (SWD 직접 / USB) |
| [Advanced Topics](docs/advanced/) | AI 데이터 파이프라인 + 관심 분야별 자기주도 학습 |
| [Troubleshooting](docs/troubleshooting.md) | 자주 마주치는 문제 정리 |
| [Examples](examples/) | 41 개 예제 (각 폴더에 5 단계 README) |
| [Python Tools](PythonDecoder/) | USB 시리얼/메모리 디코더, MATLAB 변환 |
| [Release Notes](docs/release-notes/) | 버전별 첨부 파일 + 호환성 매트릭스 |
| [Changelog](CHANGELOG.md) | 버전별 변경 이력 요약 |

---

## 참여 + 지원

활발히 연구 개발 중인 프로젝트입니다. 기능 개선, 버그 수정, 문서 보강이 수시로 일어나니 가끔 업데이트 받아주세요.

- 버그 리포트 · 기능 제안 → [GitHub Issues](https://github.com/AGR-EXO/Extension_Module/issues)
- Q&A · 사용 사례 공유 → [GitHub Discussions](https://github.com/AGR-EXO/Extension_Module/discussions)
- "Ex.XX 가 안 돼" 트러블슈팅 → Claude Code 에서 `example-helper` 호출

---

## 라이선스

[MIT License](/LICENSE)
