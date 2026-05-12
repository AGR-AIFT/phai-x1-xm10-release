# XM10 문서

XM10 의 전체 문서 인덱스입니다. 처음 시작하는 분은 위에서 아래로 순서대로 읽으면 됩니다.

> **🧭 어디 있을까?** — 키워드로 빠른 검색이 필요하면 **[find-it.md](find-it.md)** 한 페이지에 자주 묻는 키워드 → 정답 페이지가 다 모여 있어요.

---

## 시작 흐름

```
환경 구축 ──► 첫 빌드 ──► 예제로 학습 ──► API 참고하며 직접 작성
                                    │
                                    └──► 막히면: Troubleshooting / find-it.md
```

---

## 1. [Getting Started](getting-started/) — 환경 구축

처음 사용하시는 분은 여기부터.

| 순서 | 문서 | 내용 |
| :---: | :--- | :--- |
| 00 | [Claude Code 와 함께 시작](getting-started/00-claude-code-quickstart.md) | AI 자동 안내 (가장 빠름) |
| 01 | [하드웨어 연결](getting-started/01-hardware-setup.md) | KIT H10, ST-Link, 센서 허브 연결 |
| 02 | [개발 환경 구축](getting-started/02-software-setup.md) | STM32CubeIDE 설치, 레포 clone |
| 03 | [첫 빌드 & 실행](getting-started/03-first-build.md) | 프로젝트 import, 빌드, 펌웨어 업로드 |

---

## 2. [Tutorials](tutorials/) — 41 개 예제로 단계 학습

각 예제는 `목표 → 사전 지식 → 핵심 코드 → 실험 → 다음 단계` 5 단계로 통일되어 있어요. 한 예제는 30 분 안에 끝나도록 설계.

| Part | 주제 | 예제 |
| :---: | :--- | :--- |
| 0 | 보드 동작 확인 | Ex.00 |
| 1 | 기본 입출력 (LED·버튼·외부 IO) | Ex.01~06 |
| 2 | USB 통신 + 메모리 로깅 | Ex.07~10c |
| 3 | KIT H10 외골격 기본 모드 | Ex.11~13 |
| 4 | 제어 알고리즘 기초 + 디버깅 | Ex.14~19 |
| 5 | 제어 알고리즘 심화 (Hogan, HZD, CPG 등) | Ex.20~25 |
| 6 | 학습 + 적응 제어 (ILC, MRAC 등) | Ex.26~30 |
| 7 | Physical AI 응용 | Ex.31~36 |

전체 학습 경로 + 추천 순서: **[Tutorials README](tutorials/)**

---

## 3. [API Reference](api-reference/) — 함수 참고서

알고리즘 작성하면서 옆에 두고 보는 문서. 처음부터 다 읽을 필요는 없어요.

| 문서 | 내용 |
| :--- | :--- |
| [Task State Machine](api-reference/01-task-state-machine.md) | 상태 머신 — 상태별 동작 분리 |
| [KIT H10 제어 & 데이터](api-reference/02-h10-control-n-data.md) | 외골격 센서 읽기 + 토크/위치 명령 |
| [LED & 버튼](api-reference/03-led-btn-control.md) | 보드 LED 제어, 버튼 입력 |
| [외부 IO](api-reference/04-external-io.md) | GPIO, ADC 핀 제어 |
| [USB 시리얼 통신](api-reference/05-usb-connectivity.md) | PC 로 메시지/데이터 보내기 |
| [USB 메모리 로깅](api-reference/06-usb-data-logging.md) | USB 메모리에 자동 저장 |
| [메모리 영역](api-reference/07-memory-management.md) | 빠른 메모리, 비휘발성 저장소 |
| [실시간 시계](api-reference/08-rtc-clock.md) | 날짜/시간 읽기·쓰기 |

---

## 4. [Hardware](hardware/) — 보드 외부 인터페이스 + 핀맵

보드에 뭐가 몇 개 있는지, 어디에 꽂는지. 외부 GPIO 핀맵은 자주 보게 되는 페이지라 Rev 1.1 / Rev 2.0 별로 분리되어 있어요.

---

## 5. [KIT H10 Firmware](kit-h10-firmware/) — H10 펌웨어 + 컨텐츠

XM 버전과 KIT H10 펌웨어 버전이 맞아야 정상 동작합니다. 업데이트가 필요하면 여기.

---

## 6. [Architecture](architecture/) — 시스템 큰 그림

내 코드가 어디서 어떻게 동작하는지 — 처음 시작할 때 한 번 읽으면 전체가 보입니다.

---

## 7. [Bootloader](bootloader/) — 펌웨어 업로드 방법

ST-Link SWD 직접 업로드 + PhAI Studio USB 업로드 두 가지 방법 + 부트로더 자동 롤백.

---

## 8. [Advanced Topics](advanced/) — 자기주도 학습 경로

관심 분야별 (제어 / AI / 데이터 분석 / 투명 모드 등) 추천 예제 경로.

---

## 9. [Troubleshooting](troubleshooting.md) — 막혔을 때

빌드 오류, USB 연결, 통신 문제 등 자주 마주치는 상황 정리.

---

## 추가 자료

- [🧭 find-it.md](find-it.md) — 키워드 → 페이지 빠른 찾기
- [Examples](../examples/) — 41 개 예제 소스 코드 (각 폴더에 5 단계 README 포함)
- [Python Tools](../PythonDecoder/) — USB 시리얼 수신기, USB 메모리 디코더
- [Release Notes](release-notes/) — 버전별 첨부 파일 + 호환성 매트릭스
- [Changelog](../CHANGELOG.md) — 버전별 변경 이력 요약
- [XM10 SDK](../XM10_SDK/) — Rev1.1 / Rev2.0 SDK 프로젝트
- [학생 5 명 가상 시뮬레이션](student-walkthrough-simulations.md) — 강사·멘토용 UX 검증 워크스루
