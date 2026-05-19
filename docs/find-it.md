# 어디 있을까? — 빠른 찾기 인덱스

자주 묻는 질문 + 자주 찾는 위치를 한 페이지에 모았어요. 메뉴를 9 개씩 뒤지지 말고 여기서 Ctrl+F (`찾기`) 로 키워드 검색.

---

## 🚀 시작하기

| 상황 | 어디로 |
|------|--------|
| 처음 시작, 무엇부터 해야 하지 | [docs/getting-started/00-claude-code-quickstart.md](getting-started/00-claude-code-quickstart.md) (AI 안내) 또는 [01 하드웨어 연결](getting-started/01-hardware-setup.md) |
| Claude Code 가 뭐예요 | [00 Claude Code 시작](getting-started/00-claude-code-quickstart.md) |
| URL 만 받았는데 어떻게 진행 | [00 페이지 - clone 전 OK 박스](getting-started/00-claude-code-quickstart.md) |
| 환경 구축 단계가 막혔어요 | [02 환경 구축](getting-started/02-software-setup.md) → [troubleshooting.md](troubleshooting.md) |
| 첫 빌드가 안 돼요 | [03 첫 빌드](getting-started/03-first-build.md) + [troubleshooting.md](troubleshooting.md) |
| LED 가 안 켜져요 | [Ex.00 Quick Start README](../examples/00_Quick_Start/) |

## 🛠️ 코드 작성

| 상황 | 어디로 |
|------|--------|
| 내 코드 어디에 쓰지 | `XM_Apps/User_Algorithm/user_app.c` (ZIP 압축 푼 폴더 기준) |
| 내 코드가 언제 호출되는지 | [docs/architecture/README.md](architecture/README.md) |
| 함수 시그니처 / API 명세 | [docs/api-reference/](api-reference/) |
| 버튼 / LED 제어 | [api-reference/03-led-btn-control.md](api-reference/03-led-btn-control.md) + Ex.01~03 |
| 외부 GPIO / ADC | [api-reference/04-external-io.md](api-reference/04-external-io.md) + Ex.04~06 |
| USB 시리얼로 PC 에 데이터 보내기 | [api-reference/05-usb-connectivity.md](api-reference/05-usb-connectivity.md) + Ex.07~09 |
| USB 메모리에 로깅 | [api-reference/06-usb-data-logging.md](api-reference/06-usb-data-logging.md) + Ex.10~10c |
| 메모리 영역 (PSRAM, Workspace) | [api-reference/07-memory-management.md](api-reference/07-memory-management.md) + Ex.19 |
| 날짜·시간 (RTC) | [api-reference/08-rtc-clock.md](api-reference/08-rtc-clock.md) |
| KIT H10 외골격 제어 | [api-reference/02-h10-control-n-data.md](api-reference/02-h10-control-n-data.md) + Ex.11~13 |
| 상태 머신 (TSM) | [api-reference/01-task-state-machine.md](api-reference/01-task-state-machine.md) + Ex.03, Ex.10c |

## 📚 예제

| 상황 | 어디로 |
|------|--------|
| 41 개 예제 전체 인덱스 | [examples/README.md](../examples/README.md) |
| 난이도별·트랙별 학습 경로 | [docs/tutorials/README.md](tutorials/README.md) |
| 한 학기 수업 진도표 (16 주) | [tutorials/README.md - 한 학기 진도표 섹션](tutorials/README.md#한-학기-16-주-수업-진도표-예시) |
| Ex.XX 예제가 잘 안 돼요 | 해당 `examples/XX_*/README.md` 의 "⚠️ 흔한 실수" 섹션 |
| 입문자가 첫 30 분 동선 | [examples/README.md - 처음 오신 분](../examples/README.md#처음-오신-분--첫-30-분-동선) |
| PD 제어 / 임피던스 / CPG 등 알고리즘 예제 | examples/14, 15, 20~30 (자세한 매핑은 [examples/README.md](../examples/README.md)) |

## 🔌 하드웨어

| 상황 | 어디로 |
|------|--------|
| 보드에 뭐가 몇 개 있나 (LED / 버튼 / CAN / USB / UART) | [docs/hardware/README.md](hardware/README.md) |
| 외부 GPIO 핀맵 (Rev 1.1) | [hardware/external-gpio-rev1.1.md](hardware/external-gpio-rev1.1.md) |
| 외부 GPIO 핀맵 (Rev 2.0) | [hardware/external-gpio-rev2.0.md](hardware/external-gpio-rev2.0.md) |
| 내 보드가 Rev 1.1 인지 Rev 2.0 인지 | [hardware/README.md - 보드 리비전 비교](hardware/README.md#보드-리비전-비교) |
| 41 개 예제는 어느 리비전에서 동작 | [examples/README.md - 보드 리비전 호환성](../examples/README.md) (둘 다 동작, 외부 GPIO 핀맵만 다름) |

## 📥 펌웨어 업로드

| 상황 | 어디로 |
|------|--------|
| ST-Link SWD 로 직접 업로드 | [docs/getting-started/03-first-build.md](getting-started/03-first-build.md) |
| PhAI Studio FTP 업로드 (배포용) | [docs/bootloader/README.md](bootloader/README.md) |
| 부트로더가 뭐고 왜 필요한가 | [bootloader/README.md - WHY 섹션](bootloader/README.md) |
| KIT H10 외골격 펌웨어 업데이트 | [docs/kit-h10-firmware/README.md](kit-h10-firmware/README.md) |
| XM10 ↔ KIT H10 버전 호환성 | [kit-h10-firmware/README.md](kit-h10-firmware/README.md) |

## 🤖 AI / 데이터 / 심화

| 상황 | 어디로 |
|------|--------|
| 외골격 데이터로 AI 모델 학습 | [docs/advanced/ai-data-pipeline.md](advanced/ai-data-pipeline.md) |
| 보드 안에서 Tiny NN 추론 | Ex.16, Ex.36 + [advanced/README.md](advanced/README.md) |
| Python 디코더 사용법 | [PythonDecoder/](../PythonDecoder/) |
| PhAI Studio 사용법 | 각 예제 README 의 4 단계 실험 부분 + [studio.onephai.com](https://studio.onephai.com) |
| 내 알고리즘을 1 ms 안에 끝내고 싶음 | Ex.18 Debug Monitor + Ex.19 Memory Aware Design |
| 자기주도 학습 트랙 (관심 분야별) | [advanced/README.md - 자기주도 학습 경로](advanced/README.md#자기주도-학습-권장-경로) |

## ❓ 문제 / 디버깅

| 상황 | 어디로 |
|------|--------|
| 빌드 / clone / 한글 경로 에러 | [docs/troubleshooting.md](troubleshooting.md) |
| 시리얼 통신 / USB 연결 안 됨 | [troubleshooting.md](troubleshooting.md) + 해당 예제 README |
| PhAI Studio 가 안 잡혀요 (CDC) | Ex.07~09 의 ⚠️ 흔한 실수 + `.c` 파일 헤더의 PhAI 동시점유 경고 |
| 보드가 무한 리부팅 | [troubleshooting.md](troubleshooting.md) |
| Body Data 가 뭐예요 (gaitCycle 등) | [api-reference/README.md - Body Data 전제조건](api-reference/README.md) |
| 강사 / 멘토 입장에서 학생 막힘 예상 시나리오 | [docs/student-walkthrough-simulations.md](student-walkthrough-simulations.md) |
| 버전별 알려진 이슈 | [docs/release-notes/](release-notes/) |

## 📖 메타 정보

| 상황 | 어디로 |
|------|--------|
| 버전 / 변경 이력 | [CHANGELOG.md](../CHANGELOG.md) |
| 릴리즈 첨부 파일 / 호환성 매트릭스 | [docs/release-notes/](release-notes/) |
| 라이선스 | [LICENSE](../LICENSE) |
| 시스템 아키텍처 큰 그림 | [docs/architecture/README.md](architecture/README.md) |
| 이 레포 전체 문서 인덱스 | [docs/README.md](README.md) |

---

## Claude Code 사용자

위 표를 외울 필요 없어요. Claude Code 에서 다음과 같이 자연스럽게 물어보세요.

```
"USB 메모리 로깅 어떻게 해?"
"Ex.14 빌드 에러 났어"
"보드 리비전 확인하는 법"
"AI 학습 데이터 어떻게 모아?"
```

`example-helper` / `student-onboard` 스킬이 자동으로 해당 페이지를 찾아 인용해줍니다.
