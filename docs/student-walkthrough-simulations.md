# 학생 5 명 가상 시뮬레이션 — UX 검증 워크스루

41 개 예제 + docs 전면 개편 (2026-05) 결과를 검증하기 위한 가상 페르소나 시나리오예요. 각 학생이 처음 GitHub URL 을 받은 시점부터 따라가며, "어디서 막힐 가능성이 있는가" 를 시뮬레이션합니다.

> 이 문서는 **사용자 (또는 멘토 강사) 가 직접 따라 읽으며** "내가 이 학생이라면 막힐까?" 를 1 인칭으로 검증하는 용도예요. 발견되는 막힘은 해당 페이지 보강의 입력으로 들어갑니다.

---

## 가상 학생 5 명

| 코드명 | 배경 | 가진 환경 | 목표 |
|--------|------|----------|------|
| **A. 민재** | 대학 2 학년, 기계공학과. C 언어 수업만 들음. MCU 처음 | 노트북 (한글 사용자 폴더), 보드 미수령 | URL 만 받음. "처음 시작" |
| **B. 지원** | 대학 3 학년, 전자공학과. Arduino 동아리 1 년. CubeIDE 처음 | 노트북, ST-Link, 보드 Rev 2.0 수령 | 빌드 + LED 점등까지 |
| **C. 서준** | 대학원 1 년차, 로봇공학. MATLAB 기반 시뮬레이션 경험. C 가물가물 | 데스크톱, 모든 장비 보유 | Ex.14 PD 제어를 자기 알고리즘으로 변형 |
| **D. 하은** | 학부 4 학년, 컴퓨터공학. 임베디드 처음. AI 관심 많음 | 노트북, Cursor 사용 중, Claude Code 모름 | 외골격 데이터로 AI 모델 학습하고 싶음 |
| **E. 강사 김** | 대학 강사. XM10 으로 한 학기 수업 설계 중 | 모든 환경 보유, 학생 30 명 가르칠 예정 | 수업 진도표 + 학생 트러블슈팅 흐름 |

각 시나리오는 4 단계로 평가합니다.

```
① 진입 (URL/레포 인지)
   ↓
② 환경 구축 (CubeIDE 설치 + clone + import)
   ↓
③ 첫 빌드/플래시 (LED 점등)
   ↓
④ 목표 달성 (각 학생별 골)
```

---

## A. 민재 — MCU 0 경험, URL 만 받음

### ① 진입 — URL 만 보고 시작

받은 메시지: `https://github.com/AGR-EXO/Extension_Module 이 보드 받았어, 다음 주에 수업이야`

| 단계 | 보는 페이지 | 예상 막힘 | 현재 대응 |
|------|------------|----------|----------|
| URL 클릭 → 첫 화면 | 루트 `README.md` | "STM32CubeIDE 가 뭔지 모름" | "AI 와 함께 (가장 빠릅니다)" 박스가 최상단. Claude Code 설치 가이드 링크. ✅ |
| Claude Code 설치 결정 | claude.com/claude-code | "설치 후 뭐 해야 하지" | README 의 3 줄 명령 + `/student-onboard` 트리거 명시. ✅ |
| Claude Code 실행 | 자기 PC | "어디서 실행?" | README 에 "레포 폴더에서 실행 → `claude`". 그런데 **민재는 아직 clone 을 안 했음** | ⚠️ |

**🚨 발견된 막힘**: 보드만 받은 상태에서 Claude Code 부터 시작하려는 학생은 "clone 도 안 했는데 어디서 실행하라는 거지" 에서 막힐 수 있어요.

**보강 제안**:
- `docs/getting-started/00-claude-code-quickstart.md` 첫 문단에 "**아직 clone 안 했어도 괜찮아요** — 임의의 빈 폴더에서 `claude` 실행 후 URL 만 알려주면, Claude Code 가 clone 부터 안내합니다" 명시
- 또는 루트 README "AI 와 함께" 박스에 한 줄 추가: "*clone 전이어도 OK — Claude Code 가 안내해줍니다*"

### ② 환경 구축

| 단계 | 보는 페이지 | 예상 막힘 | 현재 대응 |
|------|------------|----------|----------|
| CubeIDE 설치 | `docs/getting-started/02-software-setup.md` | ST 회원가입 필요 | 페이지 내 안내 있음 ✅ |
| clone 위치 결정 | 루트 README + 02 페이지 | "C:\dev 가 뭐예요" | "한글 없는 짧은 경로" 안내 + 경로 예시 ✅ |
| **한글 사용자 폴더 자동 검출** | — | 민재 PC = `C:\Users\민재` | Claude Code `student-onboard` skill 의 Phase 0 가 자동 차단 (memory 에 기록된 동작) ✅ |
| import workspace | `docs/getting-started/03-first-build.md` | "Existing Projects into Workspace 가 어디" | 스크린샷 placeholder 만 있고 실물 없음 | ⚠️ |

**🚨 발견된 막힘**: CubeIDE Import 메뉴 위치를 처음 보는 학생은 헷갈려요. 03-first-build 의 스크린샷 placeholder 가 실제 이미지로 채워지기 전까지는 외부 검색에 의존.

**보강 제안**: `assets/img/README.md` 의 **Tier 1** 에 `cubeide-import-workspace.png` 추가. 우선순위 끌어올림.

### ③ 첫 빌드/플래시

| 단계 | 보는 페이지 | 예상 막힘 | 현재 대응 |
|------|------------|----------|----------|
| Build (Ctrl+B) | 03 페이지 | "에러 0 이 정상인가" | "✅ Build Finished. 0 errors" 명시 ✅ |
| ST-Link 연결 | `01-hardware-setup.md` | 케이블 핀 헷갈림 | 핀맵 placeholder, 실물 없음 | ⚠️ Tier 1 |
| Flash (F11) | 03 페이지 | "Permissions 권한 팝업" | 안내 없음 | ⚠️ |
| **LED 점등 확인** | `Ex.00 Quick Start` | "어떤 LED 가 켜져야?" | 3 LED 시퀀스 영상 placeholder, 실물 없음 | ⚠️ Tier 1 |

**🚨 발견된 막힘**: 펌웨어 업로드는 됐는데 LED 시퀀스가 어떻게 보여야 정상인지 학생이 자기 보드를 비교할 기준이 없어요.

**보강 제안**: `00_boot_sequence.gif` (Tier 1) 가 가장 우선순위. 없으면 텍스트로 "3 개 LED 가 순차적으로 점멸 후 LED1 만 천천히 호흡하면 정상" 같은 명확한 기술이 필요.

### ④ 목표 달성

민재의 목표 = "수업 첫 날 준비". `Ex.00` 까지 도달했으면 ✅. 다음 수업에서 Ex.01 부터 시작.

---

## B. 지원 — Arduino 1 년, 보드 Rev 2.0 수령

### ① 진입

지원은 Arduino 의 `digitalWrite(13, HIGH)` 에 익숙해요. "STM32 도 비슷할 거야" 라고 생각하면서 진입.

- 루트 README 의 "직접 진행하고 싶다면" 으로 바로 점프 (AI 안 씀)
- 3 줄 명령으로 clone 시도 → 한글 경로 없는 `C:\dev` 에 성공

### ② 환경 구축

| 단계 | 예상 막힘 | 대응 |
|------|----------|------|
| CubeIDE 설치 | "Eclipse 기반이라 익숙하지 않음" | 02 페이지 ✅ |
| **Rev 2.0 / Rev 1.1 어느 프로젝트?** | 루트 README 명령에 `Rev2.0` 명시 ✅ |
| Build | 첫 시도에서 "Headers Discovered" 단계가 느림 | 안내 있음 (03 페이지) ✅ |

### ③ 첫 빌드/플래시

지원은 ST-Link 연결을 빠르게 마치고 LED 점등 성공 → Ex.01 로 바로 진행.

**Ex.01 Button & LED Basic** 학습 시:

```c
if (XM_GetButtonState(BUTTON_USER1) == BUTTON_PRESSED) {
    XM_SetLedState(LED_USER1, LED_ON);
}
```

- "이게 Arduino 의 `digitalRead/Write` 랑 똑같네" → 빠르게 이해 ✅
- Ex.01 README 의 "변형" 시도 → ⭐ 난이도라 어렵지 않음 ✅

### ④ 목표 달성

목표 = "내 보드가 작동한다 + 기본 IO 이해" → Ex.01 ~ Ex.03 까지 30 분 만에 도달 ✅

**🟢 지원 같은 학생에게는 현재 UX 가 잘 동작**. Arduino 경험이 디딤돌이 됨.

---

## C. 서준 — MATLAB 시뮬레이션 경험, C 가물가물

### ① 진입 + 환경 구축

서준은 시뮬레이션 출신이라 "실제 하드웨어가 무서움". 천천히 진행.

- README 의 학습 경로 표에서 **"고급" 트랙** 을 봄 → 03 → 09 → 10c → 15 → 16 → 17
- 그런데 Ex.15 가 목표라 빨리 가고 싶어서 Ex.00 → 직접 Ex.14 PD 로 점프 시도

### ② 첫 시도 — Ex.14 PD Realtime Control

| 단계 | 예상 막힘 | 대응 |
|------|----------|------|
| Ex.14 README 읽기 | 5 단계 lab manual 포맷 ✅ | |
| "사전 지식" 섹션 | "PD 제어식은 알지만 `XM_SetUserTorque*` API 가 처음" | API 시그니처 명시 ✅ |
| 핵심 코드 복사 | `user_app.c` 에 그대로 붙임 | 잘됨 ✅ |
| **빌드 후 H10 연결** | "H10 펌웨어 v2.3.0 인지 어떻게 확인?" | 루트 README 에 "XM v2.0.0 이상 ↔ H10 v2.3.0 짝" 명시 + kit-h10-firmware 링크 ✅ |
| **실험 4 단계 변형** | "Kp 값을 어떻게 튜닝해야 하지" | Ex.14 README 의 변형 섹션이 "Kp 를 2 배로 늘려보세요" 같은 구체 안내 ✅ |

### ③ 깊이 있는 변형

서준의 진짜 목표 = "MATLAB 에서 짠 자기 알고리즘을 보드에 올리기"

- `XM_SendUserBodyData()` 가 필요한지 헷갈림 → API Reference Body Data 전제조건 박스가 명확 ✅
- 자기 토크 계산 결과를 PhAI Studio 로 보내고 싶음 → Ex.09 의 `XM_SetUsbCustomMeta` 패턴 참조 ✅

**🟢 서준 같은 학생에게는 5 단계 lab manual + API Reference 가 잘 작동**. 단, **Ex.15 까지 가는 학습 시간은 1 주 이상** 으로 readme 표에서 명시한 시간보다 길어질 가능성.

**보강 제안**: 학습 경로 표의 "고급: 한 학기" 가 정확. 1 주로 오해하지 않도록 추가 강조 검토.

---

## D. 하은 — AI 관심, Cursor 사용 중, Claude Code 모름

### ① 진입 — Cursor 와 혼동

하은은 Cursor 에 익숙해서 "Claude Code 가 뭐지" 에서 멈춤.

| 단계 | 예상 막힘 | 대응 |
|------|----------|------|
| 루트 README 보기 | "Claude Code = Cursor 같은 거?" | 안내 부족 | ⚠️ |
| `claude.com/claude-code` 클릭 | 공식 사이트로 이동 | 외부 자료라 충분 ✅ |

**🚨 발견된 막힘**: AI 코딩 도구를 처음 접하는 학생은 Cursor 와 Claude Code 의 차이를 모를 수 있어요. 단, 이건 사용자의 메모리 `feedback_claude_code_only.md` 에 따라 "Cursor 동기화 불필요" 가 정책이라 안내를 일부러 안 한 것일 수도.

**보강 제안 (선택)**: 루트 README 의 "AI 와 함께" 박스에 "*Cursor 등 다른 AI 도구도 이 레포를 읽을 수 있지만, `student-onboard` 같은 자동 phased 안내는 Claude Code 전용입니다*" 한 줄. 사용자 정책에 따라 결정.

### ② 환경 구축 + ③ 빌드

하은은 결국 Claude Code 를 설치하고 `/student-onboard` 트리거. 빠르게 LED 점등 도달.

### ④ AI 학습 목표

목표 = "외골격 데이터로 AI 학습"

- 루트 README 의 "무엇을 만들 수 있나" → AI 상위 제어 (Jetson + Tiny NN) 언급 ✅
- 학습 경로 "Physical AI 응용" 트랙: Ex.21 → 31 → 32 → 33 → 36
- Ex.33 Kinesthetic Teaching README → "암묵지를 데이터로" 설명 ✅

**🚨 발견된 막힘**: 하은이 진짜 알고 싶은 건 "내 보드 데이터를 PyTorch 로 어떻게 가져오지" 인데, 그 파이프라인이 단편적으로 흩어져 있어요. PythonDecoder, PhAI Studio, USB MSC 로깅 → matplotlib 변환 같은 흐름이 한 페이지에 없음.

**보강 제안**: `docs/advanced/` 에 "AI 학습 데이터 파이프라인" 페이지 신설 — Ex.10b 로 SD 카드 로깅 → PythonDecoder 로 변환 → PyTorch DataLoader 예시. 현재 advanced 의 stub 상태에서 우선순위 항목.

---

## E. 강사 김 — 한 학기 수업 설계

### ① 강사 시점 진입

강사 김은 학생 30 명을 가르쳐야 해서 "전체 진도를 짤 수 있는가" 부터 본다.

| 단계 | 보는 페이지 | 평가 |
|------|------------|------|
| 루트 README | "한 학기 분량인가" | 학습 경로 표 (입문 3 시간 / 중급 1 주 / 고급 한 학기) ✅ |
| `docs/tutorials/README.md` | 41 개 예제 인덱스 | Part 0~7 분할 + Body Data 전제 ✅ |
| `examples/README.md` | Stage 1~5 큰 그림 | 학기 후반부 Physical AI 흐름 ✅ |
| **추천 학습 경로** | 5 가지 트랙 | "수업용 / 자기주도 / 응용 프로젝트" 명확 ✅ |

### ② 학생 트러블슈팅 시뮬레이션

강사 김이 학생 질문을 받았을 때 의지할 자료:

| 학생 질문 | 강사가 보낼 링크 | 평가 |
|-----------|------------------|------|
| "빌드가 안 돼요" | `docs/troubleshooting.md` | ✅ |
| "Ex.07 에서 PhAI Studio 가 충돌해요" | Ex.07 README 의 ⚠️ 흔한 실수 + `.c` 파일 헤더 경고 | ✅ |
| "Rev 1.1 인데 Ex.04 핀이 안 맞아요" | `docs/hardware/external-gpio-rev1.1.md` | ✅ (사용자가 핀맵 자료 채워야 완성) |
| "Body Data 가 뭐예요" | `docs/api-reference/README.md#-body-data-전제조건` | ✅ |
| "Ex.27 MRAC 어려워요" | 학습 경로 "한 학기" 트랙으로 안내 + 사전 예제 (Ex.14, 26) | ✅ |

**🚨 발견된 막힘**: 강사 김이 30 명 학생을 동시에 가르치려면 "이번 주 진도 = Ex.04~06" 같은 명시적 **수업 단위 진도표** 가 있으면 좋아요. 현재는 학생 자기주도 트랙만 있음.

**보강 제안**: `docs/tutorials/README.md` 에 "한 학기 수업 (16 주) 진도표" 예시 추가 검토. 강사 입장에서 바로 채택 가능한 자료.

### ③ 강사 → AI 도구 활용

- `.claude/skills/example-helper/` 스킬 → 강사가 학생 질문을 받았을 때 "이거 Claude Code 에 물어봐" 라고 위임 가능 ✅
- 강사 자신도 `student-onboard` 으로 새 학기 시범 운영 가능 ✅

---

## 종합 — 막힘 우선순위

5 명 시뮬레이션에서 발견된 막힘을 우선순위별로 정리.

### 🔴 High — 학생 첫 30 분에 영향

1. **이미지 자료 부재 (Tier 1)** — `00_boot_sequence.gif`, `01_btn_led_basic.gif`, `cubeide-import-workspace.png`, `boards/board-photo.png`. 텍스트만으로 "내 보드가 정상인가" 판단 어려움.
2. **clone 전 학생 안내** — `docs/getting-started/00-claude-code-quickstart.md` 에 "clone 전이어도 OK" 한 줄 추가 검토.
3. **Cursor 사용자 진입** — 정책에 따라 안내 추가 검토 (현재 의도된 미언급일 수도).

### 🟡 Medium — 학생 첫 1 주에 영향

4. **AI 학습 데이터 파이프라인 통합 페이지** — 하은 같은 AI 관심 학생을 위해 `docs/advanced/` 에 "Ex.10b → PythonDecoder → PyTorch" 흐름 한 페이지로.
5. **한 학기 수업 진도표** — 강사 김 같은 사용자를 위해 `docs/tutorials/README.md` 에 16 주 예시.

### 🟢 Low — 자기주도 학습 단계

6. **학습 시간 명시 강화** — 학습 경로 표의 "한 학기" 가 1 주로 오해되지 않도록 검토.

---

## 다음 액션

- High 항목은 **사용자가 자료 (이미지/짧은 텍스트) 를 채워야 완성** 되므로 본 plan 범위 밖.
- Medium 항목 (#4, #5) 은 별도 후속 plan 검토 — 본 plan 의 PR-8 (Verification) 결과로 도출된 신규 항목.
- Low 항목 (#6) 은 다음 docs refine 사이클에서 함께.

본 시뮬레이션 문서 자체는 강사/멘토가 새 학생 받았을 때 "어떤 막힘이 예상되는지" 미리 보는 체크리스트로 재사용 가능합니다.
