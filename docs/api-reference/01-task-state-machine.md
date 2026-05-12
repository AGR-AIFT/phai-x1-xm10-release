# API 레퍼런스: Task State Machine (TSM)

> 📌 **이 페이지를 읽고 나면**: TSM 로 상태 기반 제어 알고리즘을 작성할 수 있습니다.
> ⏱️ 예상 학습 시간: 15분
> 🧰 사전 지식: [Ex.00 Quick Start](../../examples/00_Quick_Start/) 의 단일 상태 TSM
> 🎯 핵심 함수: `XM_TSM_Create` / `XM_TSM_AddState` / `XM_TSM_Run` / `XM_TSM_TransitionTo`

`xm_api_tsm.h`에 정의된 태스크 상태 머신 API입니다. 복잡한 제어 로직을 **상태(State)** 단위로 나누어 직관적으로 구현할 수 있도록 돕습니다.
본 API를 사용하면, 사용자는 자신의 알고리즘을 체계적인 상태 머신으로 손쉽게 구성할 수 있습니다.

---

## 📌 개요 (Overview)

XM10의 TSM은 진입(Entry) -\> 반복(Loop) -\> 종료(Exit)의 3단계 생명주기(Lifecycle)를 자동으로 관리합니다. 사용자는 각 단계에서 수행할 함수만 등록하면 됩니다.

## 🛠 데이터 구조 (Data Structures)

### `XmStateConfig_t`

상태 하나를 정의하는 설정 구조체입니다.

```c
typedef struct {
    uint32_t id;             // 상태 ID (예: XM_STATE_STANDBY)
    XmStateFunc_t on_entry;  // [진입] 상태 진입 시 1회 실행
    XmStateFunc_t on_loop;   // [반복] 상태 유지 중 매 주기마다 실행
    XmStateFunc_t on_exit;   // [종료] 상태 탈출 시 1회 실행
} XmStateConfig_t;
```

### `XmStateId_e` (Standard States)

권장되는 표준 상태 ID입니다. (사용자가 10번부터 새로 정의하여 사용 가능)

  * `XM_STATE_OFF` (0): 초기 정지 상태
  * `XM_STATE_STANDBY` (1): 대기 상태
  * `XM_STATE_ACTIVE` (2): 동작 중
  * `XM_STATE_ERROR` (3): 에러 발생
  * ...
  * `XM_STATE_START` (10): 사용자가 임의로 정한 STATE

### `XmTsmHandle_t`

생성된 상태 머신 태스크를 가리키는 고유한 핸들입니다. 이 핸들을 통해 특정 상태 머신을 제어하게 됩니다. 사용자는 반드시 `XmTsmHandle_t yourTask`를 정의해주어야 합니다.
현재 상태와 현재 상태에서의 실행 단계, 이전 상태와 이전 상태에서의 실행 단계를 정의해준 'yourTask'를 Live Experssion에서 확인할 수 있습니다.

```c
typedef struct {
    /* --- Monitoring Area (Live Expression에서 보임) --- */
    XmStateId_e currentStateId; /**< 현재 상태 ID */
    XmLifecycle_t currentStep;  /**< 현재 실행 단계 */

    XmStateId_e prevStateId; /**< 이전 상태 ID */
    XmLifecycle_t prevStep;  /**< 이전 실행 단계 */
} XmTsmHandle_t;
```

---

## 📚 함수 (Functions)

### `XM_TSM_Create`

새로운 상태 머신 인스턴스를 생성합니다.

  * **Parameters**
      * `uint32_t initial_state_id`: 초기 시작 상태의 ID
  * **Return**: `XmTsmHandle_t` (생성된 핸들)

### `XM_TSM_AddState`

TSM에 새로운 상태의 동작을 등록합니다.

  * **Parameters**
      * `XmTsmHandle_t handle`: TSM 핸들
      * `const XmStateConfig_t* config`: 상태 설정 구조체 포인터
  * **Example**
    ```c
    XmStateConfig_t conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop
        // .on_exit는 생략 가능 (NULL 자동 처리)
    };
    XM_TSM_AddState(handle, &conf);
    ```

### `XM_TSM_Run`

TSM을 실행합니다. **User Task의 무한 루프 내에서 반드시 호출**되어야 합니다.

  * **Parameters**
      * `XmTsmHandle_t handle`: 실행할 TSM 핸들

### `XM_TSM_TransitionTo`

다른 상태로 전환을 요청합니다. 즉시 전환되지 않고, 현재 상태의 `Exit` 함수가 실행된 후 다음 주기에 전환됩니다.

  * **Parameters**
      * `XmTsmHandle_t handle`: TSM 핸들
      * `uint32_t next_state_id`: 이동할 다음 상태 ID

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| TSM 등록은 했는데 함수가 호출 안 됨 | `User_Loop` 안에서 `XM_TSM_Run(handle)` 누락 | `XM_TSM_Run` 가 매 주기 호출되어야 dispatch |
| `TransitionTo` 호출했는데 즉시 안 바뀜 | 의도된 동작 — 다음 주기에 `Exit → Entry` 순으로 전환 | 정상. 즉시 전환 필요하면 별도 플래그 처리 |
| Entry 가 매 주기 반복 호출됨 | 학생이 직접 `Entry()` 함수 호출 (TSM 가 자동 호출하는데 중복) | Entry 는 `TransitionTo` 시 1회만. 직접 호출 금지 |
| 상태 ID 충돌 (다른 상태가 같은 ID) | `AddState` 두 번 호출하며 동일 `id` 사용 | 표준 (`XM_STATE_*`) + 사용자 정의 (`XM_STATE_START` = 10) 충돌 회피 |
| `XM_STATE_OFF` 에서 시작했는데 아무 동작 X | `XM_STATE_OFF` 의 `on_loop` 미등록 또는 의도된 idle 상태 | 첫 상태에서 `on_loop` 정의 또는 `TransitionTo` 호출 추가 |

---

## 관련 예제

| 예제 | 난이도 | TSM 활용 |
|------|--------|----------|
| [00_Quick_Start](../../examples/00_Quick_Start/) | 입문 | 단일 상태 TSM |
| [01_Button_LED_Basic](../../examples/01_Button_LED_Basic/) | 초급 | 단일 상태 기본 |
| [03_Button_LED_FSM](../../examples/03_Button_LED_FSM/) | 중급 | STANDBY ↔ ACTIVE 전환 |
| [06_Ext_IO_Safety_Switch](../../examples/06_Ext_IO_Safety_Switch/) | 중급 | 3상태 + ERROR 처리 |
| [10c_MSC_Advanced_Log](../../examples/10c_MSC_Advanced_Log/) | 고급 | TSM + 에러 복구 |
| [11_Passive_Mode](../../examples/11_Passive_Mode/) | 고급 | Homing + 모드 전환 |
| [12_Active_Assist_Mode](../../examples/12_Active_Assist_Mode/) | 고급 | 계층적 FSM |
| [17_FSM_Gait_Intent](../../examples/17_FSM_Gait_Intent/) | 고급 | 보행 7단계 FSM |
