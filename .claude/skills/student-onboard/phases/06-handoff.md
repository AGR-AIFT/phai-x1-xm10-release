# Phase 6 — 핸드오프: 학생 자율 학습 시작

> 📌 **이 Phase 가 끝나면**: 학생이 첫 코드 변경 (Ex.00 Quick Start) 을 보드에 반영합니다.
> ⏱️ 예상 시간: 15~30 분 (실험 포함)
> 🧰 사전 조건: Phase 5 통과 (LED 점등 확인)

## 💡 WHY — 왜 Ex.00 부터인가

환경 구축은 끝났습니다. 이제 학생이 직접 코드를 바꿔보고, **빌드 → 플래시 → 결과 확인** 의 개발 사이클을 직접 돌려봐야 진짜 시작입니다.
Ex.00 (Quick Start) 은 그 사이클을 가장 짧게 도는 예제입니다. 한 줄만 바꿔도 보드가 다르게 동작합니다.

> 🧒 비유: 운전 면허 학원 첫 시간에 "시동 걸기 → 1단 출발 → 정지" 만 반복하는 것과 같음. 이 사이클 하나가 익숙해지면 다른 모든 것이 응용.

## 📖 WHAT — 사용자 코드 영역 위치

| 위치 | 역할 | 학생 수정 |
|------|------|----------|
| `XM_Apps/User_Algorithm/user_app.c` | 메인 사용자 코드 (`User_Setup()` + `User_Loop()`) | ✅ 자유롭게 수정 |
| `examples/<번호>_<이름>/*.c` | 학습용 예제 (40개+) | ✅ 복사해서 user_app.c 자리에 붙여넣기 |
| `XM_FW/XM_API/` | 공개 API 헤더 (`xm_api.h` umbrella) | ❌ 봉인 (호출만) |
| `XM_Lib/`, `Drivers/`, `Middlewares/` | 라이브러리 | ❌ 봉인 |

## 🔧 HOW — 첫 실험

### 1. Ex.00 Quick Start 코드 열기

```
C:\dev\Extension_Module\examples\00_Quick_Start\quick_start.c
```

또는 CubeIDE Project Explorer 에서 동일 경로 탐색.

### 2. user_app.c 에 적용

Ex.00 의 `User_Setup()` + `User_Loop()` 본문을 복사해서 `XM_Apps/User_Algorithm/user_app.c` 에 붙여넣기.
(또는 `examples/00_Quick_Start/` 폴더 자체를 CubeIDE 빌드 대상으로 추가 — 자세한 방법은 예제 README 참조)

### 3. 빌드 + 플래시 사이클 (Phase 3 + 4 반복)

- `Ctrl + B` 빌드
- ✅ "0 errors" 확인
- `Run` → `Debug As` → `STM32 C/C++ Application` 으로 플래시
- 보드에서 결과 관찰

### 4. 변형 실험

Ex.00 README 의 "4️⃣ 실험 — 직접 해보기" 변형 1, 2 를 따라 해보세요. 한 줄씩 바꾸면서 보드 반응이 어떻게 달라지는지 체감하는 것이 핵심.

## 🗺️ 학습 로드맵 — 어디로 갈까

학생 목표에 따라 추천 경로:

### 🛤️ 트랙 A — "수업 프로젝트 (한 학기)"
1. Ex.00 → Ex.01 → Ex.02 → Ex.03 (Button/LED 4종)
2. Ex.04 → Ex.05* → Ex.06 (외부 IO)
3. Ex.07 → Ex.09 (USB 통신)
4. Ex.10 ~ Ex.10c (USB MSC 로깅)
5. 학기 후반: Ex.11~13 (Exo 모드) 또는 Ex.14~17 (제어 알고리즘)

### 🛤️ 트랙 B — "센서 추가해서 보행 분석"
1. Ex.00 → Ex.04 → Ex.05b (FSR 8ch)
2. Ex.07 → Ex.08 (CDC 텍스트 디버깅)
3. Ex.09 (CDC PhAI Studio 스트림)
4. Ex.17 (FSM Gait Intent) → Ex.32 (GRF)

### 🛤️ 트랙 C — "H10 로봇 제어 알고리즘"
1. Ex.00 → Ex.11~13 (Passive/Active/Resistive)
2. Ex.14 (PD Control) → Ex.20 (Impedance)
3. Ex.21~25 (CPG, Gait Adaptive, ILC, MRAC, Admittance)

전체 로드맵 + prerequisite graph: [docs/tutorials/README.md](../../../docs/tutorials/README.md)

## ⚠️ 막혔을 때 — 다음 도움말

- **개별 예제 트러블슈팅**: AI 에게 `"Ex.07 에서 PhAI Studio 가 안 잡혀"` 처럼 질문 → `example-helper` skill 자동 호출
- **빌드/플래시 일반 에러**: [docs/troubleshooting.md](../../../docs/troubleshooting.md)
- **API 사용법**: [docs/api-reference/](../../../docs/api-reference/)
- **외골격/제어 이론**: [docs/architecture/](../../../docs/architecture/)
- **GitHub Issue**: 위 어디에도 답이 없으면 `.github/ISSUE_TEMPLATE/` 4종 중 골라 제보

## 🎓 메모

- 학생이 student-onboard 를 다시 돌리고 싶다면:
  ```powershell
  Remove-Item $HOME\.xm10-onboard-done
  ```
- 환경 구축은 한 번만. 이후는 코드 작업 + 빌드/플래시 사이클 반복.

**환영합니다 — 이제부터 진짜 시작입니다. 🚀**
