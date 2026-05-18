# Ex.11 — Passive Mode (P-Vector + I-Vector 자동 왕복 운동)

> 🎯 **학습 목표**:
> - **P-Vector** (궤적 명령) + **I-Vector** (임피던스 설정) 로 H10 의 두 다리를 자동 왕복 운동.
> - 3-계층 FSM (OFF / STANDBY / ACTIVE) + 내부 sub-FSM (Homing → Mode Transition → Passive Cycle).
> - PVector queue 패턴 (현재 → MAX → MIN → MAX → ... 미리 큐잉으로 끊김 없는 운동).
>
> ⏱️ 권장 시간: 45분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.03 FSM](../03_Button_LED_FSM/) + [Ex.10c MSC Advanced](../10c_MSC_Advanced_Log/) | 📚 관련 docs: [H10 Control](../../docs/api-reference/02-h10-control-n-data.md) · [TSM](../../docs/api-reference/01-task-state-machine.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

KIT H10 의 좌·우 고관절을 **±25.0 도 사이로 부드럽게 왕복** 시킵니다 (재활 운동용 Passive 모드).

| 상태 | 동작 |
|------|------|
| OFF | CM 연결 대기 |
| STANDBY | CM 연결 후 H10 슈트가 ASSIST 모드 요청까지 대기 |
| ACTIVE (Homing) | 현재 위치 → 0도 위치로 부드럽게 정렬 (속도 150 deg/s) |
| ACTIVE (Passive Cycle) | 0도 → +25° → −25° → +25° ... 무한 왕복 (속도 250 deg/s) |
| ACTIVE Exit | H10 STANDBY 요청 시 모터 정지 → STANDBY 복귀 |

저장: `/LOGS/Gait_000/` 폴더에 30+ 필드 ([H10 각도/토크/IMU 9-축]) 1 kHz 자동 로깅.

> 📸 `![H10 왕복 운동](../assets/img/11_passive_motion.gif)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **P-Vector** (Position Vector) — H10 모터의 목표 궤적 명령. `{yd, L, s0, sd}` = 목표각, 시간, 초기/말기 가속도. ([api-ref §2-3](../../docs/api-reference/02-h10-control-n-data.md))
- **I-Vector** (Impedance Vector) — 위치 제어용 강성/감쇠. `{epsilon, kp, kd, lambda, duration}`. 본 예제: `kp=80, kd=1`.
- **PVector queue** — H10 가 다음 PVector 를 큐잉 보관. 첫 궤적 진행 중에 다음 궤적 미리 큐잉하면 도착점에서 끊김 없이 자동 시작.
- **`XM_IsCmConnected()`** — CM (Central Module) 연결 상태. False 시 즉시 OFF 강제 전환 (안전).
- **`XM.status.h10.h10Mode`** — H10 슈트가 표시하는 보조 모드 (`XM_H10_MODE_STANDBY` / `XM_H10_MODE_ASSIST`). 사용자가 슈트 버튼으로 변경.
- **Body Data 전제조건**: 본 예제는 토크 직접 명령 X 라 Body Data 호출 불필요.

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
/* ① Homing FSM — 슈트 ASSIST 요청 후 0도 위치 정렬 */
static void InitHoming(void)
{
    switch (s_homingState) {
        case HOMING_ENTRY:
            XM_SendPVectorReset(SYS_NODE_ID_RH);                   // 기존 궤적 reset
            XM_SendPVectorReset(SYS_NODE_ID_LH);
            XM_SendIVectorKpKdMax(SYS_NODE_ID_RH, 6, 6);           // Kp/Kd 최대값 등록
            s_homingState = HOMING_SET_IMPEDANCE;
            break;
        case HOMING_SET_IMPEDANCE: {
            IVector_t iv = { .epsilon = 0, .kp = 80, .kd = 1, .duration = 50 };
            XM_SendIVector(SYS_NODE_ID_RH, &iv);                    // ② Stiff impedance
            XM_SendIVector(SYS_NODE_ID_LH, &iv);
            s_homingState = HOMING_START_MOTION;
            break;
        }
        case HOMING_START_MOTION: {
            /* 현재 위치 → 0도 까지의 PVector 계산 + 송신 */
            PVector_t pv = { .yd = 0, .L = duration, .s0 = 2, .sd = 2 };
            XM_SendPVector(SYS_NODE_ID_RH, &pv);
            XM_SendPVector(SYS_NODE_ID_LH, &pv);
            s_homingState = HOMING_WAIT_FOR_DONE;
            break;
        }
        case HOMING_WAIT_FOR_DONE:
            if (XM.status.h10.isPVectorRHDone && XM.status.h10.isPVectorLHDone) {
                XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);            // ③ done flag clear
                XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);
                s_homingState = HOMING_FINALIZE_DELAY;
            }
            break;
        /* ... DELAY + CLEANUP → ACTIVE 진입 ... */
    }
}

/* ④ Passive Cycle — 무한 왕복 */
static void UpdatePassiveMode(void)
{
    switch (s_passiveState) {
        case PASSIVE_STATE_START_MOTION: {
            /* [1] 현재 → MAX 궤적 송신 */
            XM_SendPVector(SYS_NODE_ID_RH, &toMaxRH);
            XM_SendPVector(SYS_NODE_ID_LH, &toMaxLH);
            /* [2] MAX → MIN 궤적 미리 큐잉 (pre-queue) */
            XM_SendPVector(SYS_NODE_ID_RH, &toMinRH);                // ⑤ 큐 사용
            XM_SendPVector(SYS_NODE_ID_LH, &toMinLH);
            s_passiveState = PASSIVE_STATE_MOVING_TO_MIN;
            break;
        }
        case PASSIVE_STATE_MOVING_TO_MIN:
            /* MAX 도착 done → MIN 도착 중 → 다음 MAX 미리 큐잉 */
            if (XM.status.h10.isPVectorRHDone && XM.status.h10.isPVectorLHDone) {
                XM_ClearPVectorDoneFlag(...);
                XM_SendPVector(..., &toMaxRH);                         // 다음 → MAX
                s_passiveState = PASSIVE_STATE_MOVING_TO_MAX;
            }
            break;
        /* ... MOVING_TO_MAX 대칭 ... */
    }
}
```

전체 코드: [`passive_mode.c`](passive_mode.c) (684 줄, 가장 큰 예제)

> 🧒 ⑤ 의 **pre-queue 패턴** 이 핵심 — 도착점에서 끊김 없이 다음 궤적 시작.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **HW**: KIT H10 ↔ XM10 CAN-FD 연결 + USB MSC 메모리 + 본체 전원
2. **빌드 + 플래시** → ✅ `0 errors`
3. **CM 연결 확인** → ✅ OFF → STANDBY 자동 전환 (LED 변화 없음)
4. **H10 슈트 버튼** 으로 ASSIST 모드 진입 → ✅ Homing 시작 (0도로 정렬)
5. **Homing 완료** → ✅ Passive 왕복 운동 시작 (±25도, 1초당 한쪽 운동)
6. **MSC 로깅 확인** → ✅ USB 메모리에 `Gait_000` 폴더 + 30 필드 .bin
7. **H10 STANDBY 복귀** → ✅ 부드럽게 정지 + STANDBY 상태 복귀
8. **변형 1 — ROM 변경**: `JOINT_ANGLE_MAX/MIN_ANGLE_INT16` 값 (250 = 25.0°) 을 100 (10°) 또는 400 (40°) 로 변경.
9. **변형 2 — 속도 변경**: `PM_SPEED_RH/LH` 250 → 100 (느림) 또는 400 (빠름).
10. **변형 3 — 임피던스 변경**: Stiff `kp=80, kd=1` 을 `kp=40, kd=2` 로 → 부드러운 추종 vs 강한 추종 비교.

---

## 5️⃣ 다음 단계

- 사용자 의도 감지 + 토크 보조: [Ex.12 Active Assist](../12_Active_Assist_Mode/)
- 저항 운동 (H10 내장 기능 활용): [Ex.13 Resistive](../13_Resistive_Mode/)
- 토크 직접 제어 (PD): [Ex.14 PD Realtime](../14_PD_Realtime_Control/)
- 보행 위상 적응 보조: [Ex.17 FSM Gait Intent](../17_FSM_Gait_Intent/) / [Ex.23 Gait Phase Adaptive](../23_Gait_Phase_Adaptive_Torque/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| Homing 무한 대기 | `isPVectorDone` 플래그 clear 안 됨 → 다음 PVector 가 done 못 함 | 매 단계 `XM_ClearPVectorDoneFlag` 호출 확인 |
| 왕복이 끊김 (도착점에서 멈춤) | Pre-queue 패턴 누락 — 다음 PVector 송신 안 됨 | `START_MOTION` 에서 [1]+[2] 둘 다 송신 |
| H10 가 안 움직임 | `XM_SetControlMode(XM_CTRL_MONITOR)` 만 호출됨 | Active 진입 시 별도 모드 설정 X — H10 슈트가 ASSIST 모드 요청해야 함 |
| 모터가 ROM 끝에서 충돌음 | 가속도 `s0/sd` 너무 큼 | 1~2 권장. 큰 값은 도착 시 급정지 |
| MSC 로깅 안 됨 | USB 미삽입 또는 `XM_SetUsbLogSource` 누락 | Setup 에서 호출 확인 + FAT32 |
| CM 연결 끊김 → 폭주 우려 | OFF 강제 전환으로 안전 | `XM_IsCmConnected()` 가 false 일 때 모든 cycle 첫 줄에서 OFF 전환 |
| 슈트 STANDBY 복귀 시 H10 잔진동 | `MODE_TRANSITION_*` FSM 미동작 | STOP_PENDING → STOP_COMPLETED → DELAYING 단계 진행 확인 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
