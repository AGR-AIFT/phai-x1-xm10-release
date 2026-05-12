# Ex.12 — Active Assist Mode (의도 감지 + 양측 독립 토크 보조)

> 🎯 **학습 목표**:
> - **사용자 의도 추적** (관절 각도 변화 감지) + 의도 방향으로 부드러운 토크 보조.
> - 좌/우 다리 **독립 sub-FSM** (한쪽이 의도 감지 → 보조 중, 다른 쪽은 대기 가능).
> - 토크 smoothing (가파른 변화 방지) + 보조 토크 자동 ramp.
>
> ⏱️ 권장 시간: 50분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.11 Passive](../11_Passive_Mode/) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md) · [TSM](../../docs/api-reference/01-task-state-machine.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

H10 슈트가 ASSIST 모드일 때 사용자가 다리를 움직이려는 의도를 감지하면, 해당 다리만 의도 방향으로 부드러운 보조 토크를 가합니다. 양 다리 독립 처리.

| 상태 (계층) | 동작 |
|-------------|------|
| OFF / STANDBY / ACTIVE | Ex.11 과 동일 (CM 연결 + 슈트 ASSIST 대기) |
| ACTIVE → AA_STATE_HOMING | 양 다리 0도 정렬 |
| ACTIVE → AA_STATE_ASSISTING | 각 다리 sub-FSM 독립 동작 (의도 추적 → 보조 → 복귀) |

핵심 알고리즘: 2초간 의도 각도 추적 → 임계치 (5도) 초과 감지 → 그 방향으로 3 Nm 보조 토크 ramp.

> 📸 `![Active Assist 의도 감지](../assets/img/12_active_assist.gif)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **Active Assist vs Passive** — Passive 는 정해진 궤적 반복, Active Assist 는 사용자 의도에 반응. Passive = "끌어가기", Active = "도와주기".
- **의도 감지 (Intent Detection)** — 짧은 시간 (2초) 동안의 각도 변화 적분이 임계치 (5°) 초과 시 그 방향이 사용자 의도.
- **`XM_SetAssistTorque(L_Nm, R_Nm)`** — 좌·우 다리에 실시간 보조 토크 명령 (Nm). 양수 = +방향, 음수 = -방향. ([api-ref](../../docs/api-reference/02-h10-control-n-data.md))
- **`XM_SetControlMode(XM_CTRL_TORQUE)`** — 토크 명령 모드 진입. 미설정 시 명령 무시.
- **Sub-FSM 독립** — 좌·우가 비동기로 다른 단계에 있을 수 있음. 한쪽 추적 중, 다른 쪽 보조 중도 OK.
- **Smoothing factor** `0.005` — 매 cycle 토크 5/1000 비율로 목표값 추종 → 1초에 약 80% 도달.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define ASSIST_TORQUE_NM             3.0f   // 보조 토크 (Nm)
#define INTENT_TRACKING_DELAY_MS     2000   // 의도 추적 시간
#define INTENT_ANGLE_THRESHOLD_DEG10 50     // 5.0도 (=50/10)
#define TORQUE_SMOOTHING_FACTOR      0.005f // 부드러움

/* ① Sub-FSM 구조 (한쪽 다리) */
typedef enum {
    AA_LEG_IDLE,                 // 의도 대기
    AA_LEG_TRACKING_INTENT,      // 의도 추적 중 (2초 측정)
    AA_LEG_ASSISTING,            // 보조 중 (토크 ON)
    AA_LEG_RETURNING,            // 복귀 (토크 OFF)
} ActiveAssistLegState_t;

/* ② 양 다리 독립 처리 */
static void Active_Loop(void)
{
    XM_SetControlMode(XM_CTRL_TORQUE);                              // 토크 모드

    UpdateAssistFor(SYS_NODE_ID_RH, &s_rh_state, &s_rh_torque);     // ③ 독립 sub-FSM
    UpdateAssistFor(SYS_NODE_ID_LH, &s_lh_state, &s_lh_torque);

    XM_SetAssistTorque(s_lh_torque_smooth, s_rh_torque_smooth);    // ④ 매 cycle 송신
}

/* ⑤ Sub-FSM 동작 (의도 추적 → 보조 → 복귀) */
static void UpdateAssistFor(node_id, state, torque)
{
    int16_t cur = (int16_t)round(XM.status.h10.<side>HipMotorAngle * 10.0f);

    switch (*state) {
        case AA_LEG_IDLE:
            if (abs(cur - s_anchor_angle) > MOVEMENT_START_THRESHOLD_DEG10) {
                /* 0.5° 이상 움직임 → 의도 추적 모드로 */
                s_track_start_time = XM_GetTick();
                *state = AA_LEG_TRACKING_INTENT;
            }
            break;
        case AA_LEG_TRACKING_INTENT:
            if (XM_GetTick() - s_track_start_time >= INTENT_TRACKING_DELAY_MS) {
                int16_t delta = cur - s_anchor_angle;
                if (abs(delta) > INTENT_ANGLE_THRESHOLD_DEG10) {     // 5° 임계
                    *torque = (delta > 0) ? +ASSIST_TORQUE_NM : -ASSIST_TORQUE_NM;
                    *state = AA_LEG_ASSISTING;                       // ⑥ 보조 시작
                } else {
                    *state = AA_LEG_IDLE;                            // 의도 부족 → 복귀
                }
            }
            break;
        case AA_LEG_ASSISTING:
            /* 토크 유지. 사용자가 반대 방향 의도 → RETURNING */
            break;
        case AA_LEG_RETURNING:
            *torque = 0;
            *state = AA_LEG_IDLE;
            break;
    }
}

/* ⑦ 토크 smoothing (가파른 변화 방지) */
s_lh_torque_smooth += (s_lh_torque_target - s_lh_torque_smooth) * TORQUE_SMOOTHING_FACTOR;
```

전체 코드: [`active_asssit_mode.c`](active_asssit_mode.c) (긴 파일, 의도/토크/Mode Transition 결합)

> 🧒 핵심: ③⑤ 의 **per-leg sub-FSM** + ⑦ smoothing — 한쪽이 강하게 잡고 다른 쪽이 천천히 따라갈 수 있음.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **HW**: KIT H10 ↔ XM10 + 본체 전원 + USB MSC (선택)
2. **빌드 + 플래시 + CM 연결 + 슈트 ASSIST 모드** → Homing 완료 후 의도 대기
3. **한쪽 다리만 천천히 움직임** → ✅ 2초 후 그 방향으로 부드러운 보조 토크 시작
4. **반대 방향으로 다리 의도** → ✅ 보조 토크가 부드럽게 0 으로 복귀 후 새 방향
5. **양 다리 동시 다른 방향 의도** → ✅ 좌/우가 독립 sub-FSM 진행
6. **변형 1 — 의도 추적 시간**: `INTENT_TRACKING_DELAY_MS` 를 `500` 또는 `5000` 으로 → 빠르게/느리게 반응.
7. **변형 2 — 보조 토크 크기**: `ASSIST_TORQUE_NM` 1.0 ~ 5.0 으로 → 가벼움/강함.
8. **변형 3 — 임계치 변경**: `INTENT_ANGLE_THRESHOLD_DEG10` 50 → 100 (10° 필요) 또는 30 (3°) 로 → 의도 감도 변경.
9. **변형 4 — Smoothing 가파르게**: `TORQUE_SMOOTHING_FACTOR` 0.005 → 0.05 (10배) → 토크 변화가 거의 즉시 (덜 부드러움).

---

## 5️⃣ 다음 단계

- 저항 운동 (H10 내장 기능): [Ex.13 Resistive](../13_Resistive_Mode/)
- 토크 직접 제어 (학생 PD 구현): [Ex.14 PD Realtime](../14_PD_Realtime_Control/)
- AI 의도 추정: [Ex.16 TinyAI](../16_TinyAI_Sensor_Fusion/)
- 7-phase 보행 FSM: [Ex.17 FSM Gait Intent](../17_FSM_Gait_Intent/)
- 보행 위상 적응 토크: [Ex.23 Gait Phase Adaptive](../23_Gait_Phase_Adaptive_Torque/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| 사용자 의도 인식 안 됨 | `MOVEMENT_START_THRESHOLD_DEG10` 너무 큼 (5 = 0.5°) | 사용자 다리 RoM 측정 후 조정 |
| 보조 토크가 항상 0 | `XM_SetControlMode(XM_CTRL_TORQUE)` 누락 | Loop 첫 줄에서 매번 호출 |
| 토크가 갑자기 강하게 들어옴 | Smoothing factor 너무 큼 (즉시 도달) | `0.005` 정도 권장 |
| 양 다리 sub-FSM 가 동일 동작만 | Per-leg state 가 static 이지만 같은 변수 공유 | `s_rh_state`, `s_lh_state` 분리 확인 |
| 의도 추적 중에 보조 시작 | 임계치 너무 작거나 추적 시간 너무 짧음 | 추적 시간 2초 + 임계치 5° 조합으로 천천히 안정 |
| H10 모터 진동 | 토크 변화 너무 빠름 + 임피던스 미설정 | Active Assist 는 토크만 명령, 위치 제어 X. 정상 동작 |
| 추적 시작 anchor 미갱신 | 매 IDLE 진입 시 `s_anchor_angle = cur` 누락 | sub-FSM IDLE 진입 시 anchor 재설정 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
