# Ex.17 — FSM Gait Intent (7-phase 보행 FSM + Phase-Dependent 토크)

> 🎯 **학습 목표**:
> - 정상 보행 1주기를 **7단계 FSM** 으로 분해 (Stance 4 + Swing 3).
> - 각 단계 전환을 **물리 기반 임계치** (허벅지/무릎 각도 + 발 접지) 로 실시간 감지.
> - **단계별 차등 토크** (Loading 0.5 / Terminal +1.5 / Pre-Swing −1.0 Nm) + LPF 스무딩.
> - **Body Data 전제조건** — `XM_SendUserBodyData` 필수 (보행 분석 추정치 정확도).
>
> ⏱️ 권장 시간: 50분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.14 PD](../14_PD_Realtime_Control/) + [Ex.10c MSC Advanced](../10c_MSC_Advanced_Log/) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md) · [TSM](../../docs/api-reference/01-task-state-machine.md)

---

## ⚠️ Body Data 전제조건 (먼저 읽으세요)

본 예제의 `rightThighAngle / leftThighAngle / rightKneeAngle / leftKneeAngle / isRightFootContact / isLeftFootContact` 는 H10 CM 내부에서 **IMU + 역기구학** 으로 계산됩니다.

**올바른 추정을 위해** `User_Setup()` 에서 반드시 `XM_SendUserBodyData(bodyData)` 호출:

```c
uint32_t bodyData[8] = { 700, 1750, 0, 0, 0, 0, 0, 0 };  // 70.0kg, 175.0cm
XM_SendUserBodyData(bodyData);
```

신체 정보 미설정 시 → 무릎 각도, 발 접지 감지 부정확 → FSM 오작동.

> [examples/README.md — Body Data 안내](../README.md#part-5)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

좌·우 다리 **각각 독립 FSM** 으로 7-phase 보행 주기를 추적하며 단계별 차등 토크 보조:

| 단계 | 의미 | 보조 |
|------|------|------|
| 1 Loading Response | 체중 수용 | **+0.5 Nm** (약한 신전, 무릎 안정) |
| 2 Mid-Stance | 단하지 지지 | 0 Nm (자연 보존) |
| 3 Terminal Stance | 추진 준비 | **+1.5 Nm** (전방 가속) |
| 4 Pre-Swing | 스윙 준비 | **−1.0 Nm** (다리 들어올리기) |
| 5 Initial Swing | 다리 전진 | 0 Nm |
| 6 Mid-Swing | 무릎 최대 굴곡 | 0 Nm |
| 7 Terminal Swing | 착지 준비 | 0 Nm |

모든 토크에 **LPF 스무딩** (시상수 ~100 ms) + AssistLevel × 비례 스케일 + USB MSC 로깅 (BTN 1 토글) + PhAI 0xF0 6축.

> 📸 `![7-phase Gait Cycle](../assets/img/17_gait_phases.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### 정상 보행 주기

- **Stance Phase (지지기, ~60%)**: 발이 지면에 접촉. Loading → Mid-Stance → Terminal Stance → Pre-Swing
- **Swing Phase (유각기, ~40%)**: 발이 공중. Initial Swing → Mid-Swing → Terminal Swing
- 좌·우 다리는 **약 50% 위상차** (한쪽 Stance 일 때 다른 쪽 Swing 중간)

### 전환 임계값 (본 예제)

```
Loading → MidStance      : thigh > +5°  AND footContact
MidStance → Terminal     : thigh < -3°
Terminal → PreSwing      : !footContact  (Toe-Off)
PreSwing → InitialSwing  : knee > 15°    (무릎 굴곡 시작)
InitialSwing → MidSwing  : knee > 40°    (최대 굴곡)
MidSwing → TerminalSwing : knee < 20°    (무릎 펴짐)
TerminalSwing → Loading  : footContact   (Heel Strike, 순환 완료)
```

### LPF 스무딩

```
current = factor · target + (1 − factor) · current
factor = 0.01 → 시상수 ~100 ms (1 ms 루프)
→ 토크 급변 방지, 착용자 부드러운 전환 체감
```

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
typedef enum {
    GAIT_LOADING_RESPONSE,  GAIT_MID_STANCE,   GAIT_TERMINAL_STANCE,
    GAIT_PRE_SWING,         GAIT_INITIAL_SWING, GAIT_MID_SWING,
    GAIT_TERMINAL_SWING,    GAIT_PHASE_COUNT
} GaitPhase_t;

typedef struct {
    GaitPhase_t phase;
    float       target_torque_nm;
    float       current_torque_nm;
} GaitLegFsm_t;

static GaitLegFsm_t s_gait_rh;                                 // ① 좌·우 독립 FSM
static GaitLegFsm_t s_gait_lh;

void User_Setup(void)
{
    /* ② Body Data 필수 — 미설정 시 추정 정확도 ↓ */
    uint32_t bodyData[8] = { 700, 1750, 0, 0, 0, 0, 0, 0 };
    XM_SendUserBodyData(bodyData);
    /* TSM / Stream / Log 등록 생략 */
}

static void _UpdateGaitPhase(GaitLegFsm_t *fsm, float thigh_deg, float knee_deg, bool foot_contact)
{
    switch (fsm->phase) {
        case GAIT_LOADING_RESPONSE:                            // ③ 단방향 순환
            if (thigh_deg > 5.0f && foot_contact)
                fsm->phase = GAIT_MID_STANCE;
            break;
        case GAIT_MID_STANCE:
            if (thigh_deg < -3.0f) fsm->phase = GAIT_TERMINAL_STANCE;
            break;
        case GAIT_TERMINAL_STANCE:
            if (!foot_contact) fsm->phase = GAIT_PRE_SWING;     // Toe-Off
            break;
        case GAIT_PRE_SWING:
            if (knee_deg > 15.0f) fsm->phase = GAIT_INITIAL_SWING;
            break;
        /* ... InitialSwing → MidSwing → TerminalSwing → Loading 순환 ... */
        case GAIT_TERMINAL_SWING:
            if (foot_contact) fsm->phase = GAIT_LOADING_RESPONSE;  // Heel Strike
            break;
    }
}

static float _GetPhaseAssistTorque(GaitPhase_t phase)
{
    switch (phase) {
        case GAIT_LOADING_RESPONSE:   return +0.5f;
        case GAIT_TERMINAL_STANCE:    return +1.5f;
        case GAIT_PRE_SWING:          return -1.0f;
        default:                       return  0.0f;
    }
}

static void _ApplyPhaseAssist(GaitLegFsm_t *fsm)
{
    fsm->target_torque_nm = _GetPhaseAssistTorque(fsm->phase);
    /* ④ LPF 스무딩 */
    fsm->current_torque_nm = 0.01f * fsm->target_torque_nm
                           + 0.99f * fsm->current_torque_nm;
    /* ⑤ 토크 포화 */
    fsm->current_torque_nm = _ClampTorque(fsm->current_torque_nm);
}

static void Active_Loop(void)
{
    /* ⑥ 좌·우 독립 갱신 */
    _UpdateGaitPhase(&s_gait_rh, XM.status.h10.rightThighAngle,
                     XM.status.h10.rightKneeAngle,
                     XM.status.h10.isRightFootContact);
    _UpdateGaitPhase(&s_gait_lh, /* 좌측 동일 */);

    _ApplyPhaseAssist(&s_gait_rh);
    _ApplyPhaseAssist(&s_gait_lh);

    /* ⑦ AssistLevel × 비례 스케일 + 송신 */
    float scale = (float)XM.status.h10.h10AssistLevel / 9.0f;
    XM_SetAssistTorqueRH(s_gait_rh.current_torque_nm * scale);
    XM_SetAssistTorqueLH(s_gait_lh.current_torque_nm * scale);
}
```

전체 코드: [`fsm_gait_intent.c`](fsm_gait_intent.c) (722 줄)

> 🧒 ③ 의 **단방향 순환** = 핵심. 뒤로 돌아가는 전환 없음 (보행은 일방향).

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **HW + 빌드 + 플래시** → CM 연결 + ASSIST 진입 + 슈트 AssistLevel 5 정도
2. **실제 보행** → ✅ LED 1 (Stance) / LED 2 (Swing) 가 우측 다리 보행 주기에 맞춰 토글
3. **USB CDC** → `Gait | RH:TERMINAL LH:INI_SWNG Tau_R:1.42 Tau_L:-0.85` 매 200 ms
4. **PhAI 0xF0** → 좌·우 phase + torque + thigh angle 6축 실시간 그래프
5. **BTN 1 클릭** → ✅ USB MSC 로깅 시작 `/LOGS/GaitIntent/...`, LED 3 깜빡
6. **다시 BTN 1** → ✅ 로깅 정지 + summary.txt 생성
7. **변형 1 — 임계치 조정**: `MIDSTANCE_ANGLE_THRESHOLD` 5° → 3° (조기 전환) or 8° (지연 전환)
8. **변형 2 — 토크 크기**: `TERMINAL_STANCE_ASSIST_NM` 1.5 → 3.0 (강한 추진) 또는 0.5 (약함)
9. **변형 3 — LPF 시상수**: `TORQUE_LPF_FACTOR` 0.01 → 0.1 (10배 빠른 반응, 덜 부드러움)
10. **변형 4 — 좌·우 비대칭**: 한쪽 다리만 보조 (`SetAssistTorqueLH(0)`) → 사용자 적응 관찰
11. **변형 5 — phase 추가**: 8단계 (Mid-Stance 를 둘로 분할) → enum 확장 + 임계치 추가

---

## 5️⃣ 다음 단계

- 보행 위상 적응 토크 (연속 위상): [Ex.23 Gait Phase Adaptive](../23_Gait_Phase_Adaptive_Torque/) (Phase 2D)
- GRF 기반 위상 추정: [Ex.32 GRF Gait Intent](../32_GRF_Gait_Intent/) (Phase 2D)
- CPG 진동자 (리드믹 동기): [Ex.22 CPG](../22_CPG_Oscillator/) (Phase 2D)
- ILC (반복 학습 제어): [Ex.26 ILC](../26_Iterative_Learning_Control/) (Phase 2D)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| `thighAngle`, `kneeAngle` 가 항상 0 | `XM_SendUserBodyData` 미호출 (Body Data 미설정) | Setup 에서 호출 필수 |
| FSM 단계가 변하지 않음 | `isFootContact` 신호 부정확 (Body Data 미설정 또는 IMU 인접 잡음) | Body Data 우선 점검 |
| 양 다리가 동일 단계만 유지 | 좌·우 독립 변수 분리 안 됨 (오타) | `s_gait_rh` 와 `s_gait_lh` 분리 |
| 토크 출력이 0 만 | `XM_SetControlMode(XM_CTRL_TORQUE)` 누락 | Active_Entry 에서 호출 |
| LPF 가 너무 느려 transition 놓침 | factor 0.01 = 100 ms 시상수 | 0.05~0.1 로 ↑ |
| `H10 AssistLevel=0` 이라 모든 토크 0 | 정상 동작 (사용자가 보조 끔) | 슈트 다이얼 1~9 조정 |
| FSM 가 자주 한 단계 건너뜀 | 보행이 매우 빠르거나 임계치 가까이 노이즈 | 임계치 hysteresis 추가 |
| 로깅 시작했는데 .bin 0 byte | `XM_SetUsbLogSource` 누락 | Setup 에서 호출 확인 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
