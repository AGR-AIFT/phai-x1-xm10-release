# Ex.37 — FES Hub Module Control (CAN-FD DOP V3 ES-vector 자극 제어)

> 🎯 **학습 목표**:
> - **FES Hub** (Functional Electrical Stimulation 모듈) 를 XM10 에서 CAN-FD DOP V3 로 제어합니다.
> - ES-vector SDO 명령으로 전류 진폭·주파수·듀티·버스트를 원자적으로 전송하는 패턴을 익힙니다.
> - PnP 자동 연결 → NMT OPERATIONAL → TSM 3-상태 (OFF / STANDBY / ACTIVE) 전이 흐름을 이해합니다.
>
> ⏱️ 권장 시간: 45분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.03 Button LED FSM](../03_Button_LED_FSM/) + [Ex.11 Passive Mode](../11_Passive_Mode/) | 📚 관련 docs: [TSM](../../docs/api-reference/01-task-state-machine.md) · [LED/BTN](../../docs/api-reference/03-led-btn-control.md)

> ⚠️ **안전 주의** — FES 자극은 실제 전기 자극입니다. 전극 부착 전 FES Hub 제품 안전 가이드를 반드시 확인하십시오.

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

XM10 이 **FES Hub 형제 모듈**과 CAN-FD 로 통신하여 채널 1 의 전기 자극을 버튼으로 켜고 끕니다.

| 상태 | LED 1 | 동작 |
|------|-------|------|
| OFF | 1초 깜빡임 (Blink) | FES Hub PnP 연결 대기 |
| STANDBY | 상시 점등 (Solid) | FES Hub OPERATIONAL — 자극 준비 완료 |
| ACTIVE | 200ms 빠른 깜빡임 | CH1 자극 중 (PID 가 target 전류 추종) |

**버튼 조작 요약**:

| 버튼 | 상태 | 동작 |
|------|------|------|
| BTN1 클릭 | STANDBY | CH1 자극 시작 → ACTIVE 전환 |
| BTN2 클릭 | ACTIVE | ES-vector mid-update (20 mA → 40 mA) |
| BTN1 클릭 | ACTIVE | CH1 자극 정지 → STANDBY 복귀 |

> 📸 `![FES Hub 연결 흐름](../assets/img/37_fes_hub_ctrl.gif)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **FES Hub** — angel KIT H10 생태계의 형제 모듈. CAN-FD DOP V3 로 XM10(Master) 에 연결됩니다.
  Node ID `0x0C`, TPDO 10ms 주기로 채널 전류·임피던스·FSM 상태를 자동 전송합니다.

- **ES-vector (ES = Electrical Stimulation)** — FES 자극 파라미터 6바이트 구조체.
  SDO 0x6300 으로 단일 CAN-FD 프레임에 원자적으로 전달됩니다.
  ```
  ch_select    — 채널 선택 (1=CH1, 2=CH2, 3=Both)
  amplitude_mA — 전류 진폭 (mA, 0~80)
  duty_x1000   — 듀티비 × 0.001 (예: 20 → 0.020)
  frequency_Hz — 펄스 주파수 (10~100 Hz)
  burst_ms     — 연속 자극 시간 (ms, 65535 = 무한)
  ```

- **Master Command (0x6310)** — 가상 ISI(Intra-Stim Interval) EXT 트리거.
  `FESHUB_MCMD_TOGGLE_CH1` 을 보내면 FES Hub FSM 이 READY ↔ STIMULATING 을 토글합니다.

- **PnP 자동 연결** — XM 전원 ON 시 `FesHub_Drv_Init()` 가 PnP Master 에 Slave 를 등록합니다.
  FES Hub 전원 ON → Boot-up → Pre-Op SM 자동 실행(TPDO Mapping SDO → NMT START) → OPERATIONAL.
  사용자 코드에서는 `FesHub_Drv_IsConnected()` 로 연결 상태만 폴링하면 됩니다.

- **전이 대기 패턴** — Master Command 전송 후 FES Hub FSM 전이(READY → STIMULATING)가 1ms tick 에 완료됩니다.
  본 예제는 10ms(`FES_STIM_TRANSITION_MS`) 경과 후 ES-vector 를 보내 전이 완료를 보장합니다.

- **Master heartbeat timeout** — XM 이 5초간 CAN 프레임을 보내지 않으면 FES Hub 가 자동으로
  `target_amplitude_mA = 0` 으로 설정하여 PID 가 0 mA 로 수렴합니다 (안전 정지).

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
/* ① Setup — TSM 3-상태 등록 */
void Control_Setup(void)
{
    s_user_tsm = XM_TSM_Create(XM_STATE_OFF);          // 초기 상태 = OFF

    XmStateConfig_t off_conf = {
        .id       = XM_STATE_OFF,
        .on_entry = Off_Entry,
        .on_loop  = Off_Loop
    };
    XM_TSM_AddState(s_user_tsm, &off_conf);

    /* STANDBY, ACTIVE 도 동일하게 등록 (on_exit 는 ACTIVE 만 정의) */
    ...
}

void Control_Loop(void)
{
    XM_TSM_Run(s_user_tsm);                            // ② 2ms 마다 현재 상태의 on_loop 호출
}

/* ③ OFF → STANDBY: PnP 연결 감지 */
static void Off_Loop(void)
{
    if (FesHub_Drv_IsConnected()) {
        XM_TSM_TransitionTo(s_user_tsm, XM_STATE_STANDBY);
    }
}

/* ④ STANDBY → ACTIVE: BTN1 클릭 → TOGGLE 전송 → pending 플래그 */
static void Standby_Loop(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        FesHub_Drv_SendMasterCommand(FESHUB_MCMD_TOGGLE_CH1); // CH1 READY→STIM
        s_stim_start_tick  = XM_GetTick();
        s_pending_es_vector = true;
        XM_TSM_TransitionTo(s_user_tsm, XM_STATE_ACTIVE);
    }
}

/* ⑤ ACTIVE: pending 해소 → ES-vector 전송 */
static void Active_Loop(void)
{
    if (s_pending_es_vector &&
        (XM_GetTick() - s_stim_start_tick) >= FES_STIM_TRANSITION_MS) {

        FesHub_ESVector_t esv = {
            .ch_select    = 1,
            .amplitude_mA = FES_DEFAULT_AMPLITUDE_MA,  // 20 mA
            .duty_x1000   = FES_DEFAULT_DUTY_X1000,    // 0.020
            .frequency_Hz = FES_DEFAULT_FREQUENCY_HZ,  // 50 Hz
            .burst_ms     = FES_DEFAULT_BURST_MS,       // 무한
        };
        FesHub_Drv_SendESVector(&esv);                 // SDO 0x6300 전송
        s_pending_es_vector = false;
    }

    /* BTN2 — mid-update: amplitude 20 → 40 mA */
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        FesHub_ESVector_t esv_update = { .ch_select = 1,
                                         .amplitude_mA = FES_UPDATE_AMPLITUDE_MA, ... };
        FesHub_Drv_SendESVector(&esv_update);          // 즉시 PID 추종 변경
    }

    /* BTN1 — 자극 정지 → STANDBY */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        FesHub_Drv_SendMasterCommand(FESHUB_MCMD_TOGGLE_CH1); // CH1 STIM→READY
        XM_TSM_TransitionTo(s_user_tsm, XM_STATE_STANDBY);
    }
}
```

전체 코드: [`ex_fes_control.c`](ex_fes_control.c)

> 🧒 **핵심 포인트**: TOGGLE 명령(FSM 전이)과 ES-vector(자극 파라미터) 는 별도 SDO 입니다.
> TOGGLE 로 채널을 열고, 10ms 후 ES-vector 로 파라미터를 적용하는 순서를 지켜야 합니다.

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **HW 준비**: XM10 ↔ FES Hub CAN-FD 연결 + 양 모듈 전원
2. **빌드 + 플래시** → ✅ `0 errors`
3. **XM10 만 전원 ON** → ✅ LED1 1초 깜빡임 (OFF 상태, FES Hub 대기)
4. **FES Hub 전원 ON** → ✅ PnP 교환 완료 후 LED1 상시 점등 (STANDBY)
5. **BTN1 클릭** → ✅ LED1 빠른 깜빡임 (ACTIVE) — FES Hub CH1 자극 시작
6. **BTN2 클릭** → ✅ 자극 amplitude 20 mA → 40 mA mid-update (자극 유지 중 변경)
7. **BTN1 다시 클릭** → ✅ 자극 정지 + LED1 상시 점등 (STANDBY 복귀)
8. **FES Hub 전원 OFF** → ✅ LED1 1초 깜빡임으로 전환 (OFF 자동 복귀)
9. **변형 1 — 주파수 변경**: `FES_DEFAULT_FREQUENCY_HZ` 를 50 → 30 또는 80 으로 바꿔 자극 느낌 차이를 확인하십시오.
10. **변형 2 — 듀티 변경**: `FES_DEFAULT_DUTY_X1000` 을 20 (0.020) → 50 (0.050) 으로 조정하여 펄스 폭 변화를 관찰하십시오.
11. **변형 3 — 양채널 동시 자극**: `ch_select = 3` (`BOTH`) 으로 변경하여 CH1·CH2 동시 제어를 시험하십시오.
12. **변형 4 — TPDO 모니터링**: `FesHub_Drv_GetRxData()` 로 `ch_current_mA`, `ch_impedance` 를 읽어 USB CDC 로 출력하면 PID 추종 상태를 실시간으로 확인할 수 있습니다.

---

## 5️⃣ 다음 단계

- FSM 기반 모드 전환 심화: [Ex.03 Button LED FSM](../03_Button_LED_FSM/)
- H10 보행 보조와 FES 병행: [Ex.12 Active Assist Mode](../12_Active_Assist_Mode/)
- 복수 모듈 동기 제어 (IMU + FES): [Ex.29 Bilateral Coordination](../29_Bilateral_Coordination/)
- 자극 데이터 실시간 스트리밍: [Ex.09 CDC Stream](../09_CDC_Stream/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| FES Hub 연결 후에도 STANDBY 전환 안 됨 | PnP Pre-Op SM 아직 진행 중 (TPDO Mapping 완료까지 ~200ms) | 전원 ON 후 1초 이상 대기 후 연결 상태 재확인 |
| BTN1 눌렀는데 자극 안 됨 | TOGGLE 후 ES-vector 미전송 (`s_pending_es_vector` 처리 누락) | `Active_Loop` 에서 pending 플래그 해소 로직 확인 |
| 자극이 5초 후 자동 정지됨 | Master heartbeat timeout (FES Hub 안전 정지 동작) | 정상 동작 — XM 이 CAN 프레임을 보내는 한 유지됨 |
| amplitude mid-update 후 적용 안 됨 | ES-vector 를 STIMULATING 상태가 아닌 시점에 전송 | BTN2 는 ACTIVE 상태(자극 중)에서만 동작 확인 |
| FES Hub 연결 끊김 → ACTIVE 폭주 우려 | 연결 해제 시 OFF 자동 복귀 (안전 설계) | `Active_Loop` 첫 줄 `FesHub_Drv_IsConnected()` false 시 OFF 전환 확인 |
| `ch_select = 3` 인데 CH2 자극 안 됨 | FES Hub FW 버전에 따라 BOTH 지원 여부 상이 | FES Hub FW 릴리즈 노트에서 `FESHUB_MCMD_TOGGLE_BOTH` 지원 확인 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
