# 예제 17: FSM 기반 보행 의도 인식 (FSM Gait Intent)

본 예제는 보행 주기(Gait Cycle)를 **7단계 유한 상태 머신(FSM)** 으로 분해하고, 센서 데이터 기반으로 각 단계 전환을 실시간 감지하여 **단계별 최적의 보조 토크를 차등 적용**하는 고급 보행 보조 알고리즘을 구현합니다.

> 📖 API 레퍼런스: [H10 Control & Data](../../docs/api-reference/02-h10-control-n-data.md) · [Task State Machine](../../docs/api-reference/01-task-state-machine.md)

## 🎯 학습 목표 (Objective)

* **보행 주기(Gait Cycle)** 의 7단계 분해(Loading → MidStance → Terminal → PreSwing → InitSwing → MidSwing → TermSwing)를 이해합니다.
* 허벅지 각도, 무릎 각도, 발 접지 신호를 조합한 **센서 기반 단계 전환 조건**을 학습합니다.
* 각 단계별 보조 전략(Phase-Dependent Assist)을 설계하고, **LPF 스무딩**으로 부드러운 토크 전환을 구현합니다.
* 양쪽 다리를 **독립적인 FSM**으로 관리하여 비대칭 보행에도 대응하는 구조를 이해합니다.

---

## ⚙️ 동작 원리 (How it Works)

이 예제는 OFF → STANDBY → **ACTIVE**(보행 의도 인식 + 토크 보조)의 3단계 상태 머신으로 동작합니다.

### 1. 보행 7단계 FSM

정상 보행 1주기는 Stance(지지기, ~60%)와 Swing(유각기, ~40%)으로 구성됩니다.

| 단계 | 구간 | 생체역학적 의미 | 전환 조건 |
|------|------|----------------|----------|
| **Loading Response** | Stance | 체중 수용, 충격 흡수 | `footContact` 감지 |
| **Mid-Stance** | Stance | 단하지 지지, 에너지 최소 | 허벅지 신전 > 5도 |
| **Terminal Stance** | Stance | 추진 준비, 발뒤꿈치 들림 | 허벅지 < -3도 |
| **Pre-Swing** | Stance→Swing | 발끝 떨어짐 직전 | 발 접지 해제 |
| **Initial Swing** | Swing | 다리 전진 시작 | 무릎 굴곡 > 15도 |
| **Mid-Swing** | Swing | 최대 무릎 굴곡 | 무릎 굴곡 > 40도 |
| **Terminal Swing** | Swing | 착지 준비, 감속 | 무릎 < 20도 → 착지 |

### 2. Phase-Dependent 보조 전략

| 단계 | 보조 토크 | 목적 |
|------|----------|------|
| Loading Response | +0.5 Nm (신전) | 무릎 안정화 |
| Terminal Stance | +1.5 Nm (신전) | 추진력 보강 |
| Pre-Swing | -1.0 Nm (굴곡) | 다리 들어올리기 |
| 나머지 단계 | 0 Nm | 자연스러운 움직임 보존 |

모든 토크는 **LPF(Low-Pass Filter)** 를 거쳐 부드럽게 전환됩니다.

### 3. 버튼 및 LED

*  **BTN 1 클릭:** USB MSC 데이터 로깅 시작/정지 토글. LED 3 Blink로 로깅 상태 표시.
*  **LED 1:** Stance(지지기) — ON
*  **LED 2:** Swing(유각기) — ON (오른쪽 다리 기준)

---

## 🚀 실행 방법 (How to Use)

1.  `STM32CubeIDE`에서 본 예제 소스파일을 `user_app.c`으로 옮겨와서 빌드하고 펌웨어를 `XM10`에 업로드합니다.
2.  `KIT H10`의 전원을 켜고 `XM10`과 연결합니다.
3.  `KIT H10`의 모드를 **`ASSIST_MODE`로 변경**합니다.
4.  슈트를 입고 걸어보세요. LED 1(Stance)과 LED 2(Swing)가 보행에 맞춰 교대로 켜지는지 확인합니다.
5.  슈트 본체의 **보조 레벨 버튼**으로 보조 강도를 조절하며 보조 토크의 차이를 체감합니다.
6.  USB CDC 터미널에서 200ms마다 `Gait | RH:... LH:... Tau_R:... Tau_L:...` 디버그 메시지를 확인합니다.
7.  **BTN 1**을 눌러 로깅을 시작하면 USB MSC에 보행 데이터가 기록됩니다. 나중에 PC에서 분석할 수 있습니다.

---

## 💡 직접 해보기 (Things to Try)

* **전환 임계치 변경:** `MIDSTANCE_ANGLE_THRESHOLD`, `SWING_KNEE_THRESHOLD` 등의 임계값을 변경하여 단계 전환의 민감도를 조절해보세요. 임계값이 너무 작으면 오탐이, 너무 크면 지연이 발생합니다.
* **보조 토크 크기 조절:** `LOADING_ASSIST_NM`, `TERMINAL_STANCE_ASSIST_NM`, `PRE_SWING_ASSIST_NM` 값을 변경하여 각 단계별 보조 강도를 튜닝해보세요.
* **새 보조 전략 추가:** 현재 0 Nm인 `GAIT_MID_STANCE` 단계에 약한 보조 토크를 추가하여 단하지 지지 구간에서의 안정성 향상을 시도해보세요.
