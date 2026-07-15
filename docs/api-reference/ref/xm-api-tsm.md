# `xm_api_tsm.h` — 상태 기반 태스크 제어 (Task State Machine)

> **대상 헤더**: `XM_FW/XM_API/xm_api_tsm.h` (Rev1.1 / Rev2.0 완전 동일 — Rev 전용 항목 없음)
> **관련 개념 문서**: [01. 상태 머신 (TSM)](../01-task-state-machine.md)
> **관련 예제**: [00_Quick_Start](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/00_Quick_Start/) · [01_Button_LED_Basic](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/01_Button_LED_Basic/) · [03_Button_LED_FSM](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/03_Button_LED_FSM/) · [06_Ext_IO_Safety_Switch](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/06_Ext_IO_Safety_Switch/) · [10c_MSC_Advanced_Log](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/10c_MSC_Advanced_Log/) · [11_Passive_Mode](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/11_Passive_Mode/) · [12_Active_Assist_Mode](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/12_Active_Assist_Mode/) · [17_FSM_Gait_Intent](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/17_FSM_Gait_Intent/)

복잡한 제어 로직을 진입(Entry) → 반복(Loop) → 종료(Exit) 3단계 생명주기를 가진 **상태(State)** 단위로 나누어 구현할 수 있게 해주는 프레임워크입니다. 사용자는 각 상태에서 실행할 함수만 등록하면 되고, 생명주기 전환은 TSM이 대신 관리합니다.

> 사용자 코드에서는 이 헤더를 직접 include 하지 않고, 상위 파사드 헤더 `xm_api.h` 하나만 include 하면 됩니다 (`xm_api.h` 가 내부적으로 `xm_api_tsm.h` 를 포함합니다).

---

## 언제 사용하나

`Control_Setup()` / `Control_Loop()` 안에서 여러 동작 모드(예: STANDBY ↔ ACTIVE, 또는 보행 단계별 상태)를 다루고, 모드 전환 시 "진입할 때 한 번만", "동작 중 매 주기", "빠져나갈 때 한 번만" 실행할 코드를 깔끔히 분리하고 싶을 때 사용합니다. `if`/`switch` 로 상태를 직접 관리하는 대신 상태별 `on_entry`/`on_loop`/`on_exit` 함수만 등록하면 됩니다. 동작 원리, 설계 배경, 흔한 실수는 [01. 상태 머신 (TSM) 개념 문서](../01-task-state-machine.md) 를 먼저 읽어보세요 — 이 페이지는 각 함수의 시그니처/파라미터 상세만 다룹니다.

---

## 함수 목록

| 함수 | 한 줄 설명 |
|------|-----------|
| [`XM_TSM_Create`](#xm_tsm_create) | 새 TSM 인스턴스 생성 (초기 상태 지정) |
| [`XM_TSM_AddState`](#xm_tsm_addstate) | TSM에 상태 하나(entry/loop/exit) 등록 |
| [`XM_TSM_Run`](#xm_tsm_run) | TSM 1회 실행(dispatch) — 매 주기 호출 |
| [`XM_TSM_TransitionTo`](#xm_tsm_transitionto) | 다른 상태로 전환 요청 |

---

## 함수 상세

### `XM_TSM_Create`

```c
XmTsmHandle_t XM_TSM_Create(uint8_t initial_state_id);
```

새로운 Task State Machine 인스턴스를 생성합니다. 반환된 핸들로 이후 `XM_TSM_AddState` / `XM_TSM_Run` / `XM_TSM_TransitionTo` 를 호출합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `initial_state_id` | `uint8_t` | 시작 상태의 ID. 표준 상태(`XmStateId_e`) 값을 그대로 넘기거나, `XM_STATE_USER_START`(10) 이상의 사용자 정의 값을 넘깁니다 |

**반환값**: `XmTsmHandle_t` — 생성된 TSM 핸들 (실패 시 `NULL`). 이후 모든 TSM 함수 호출에 이 핸들을 넘깁니다.

**⚠️ 호출 컨텍스트**: `Control_Setup()` 에서 상태 머신당 1회만 호출합니다 (초기화 시점). `Control_Loop()` 이나 반복 호출 컨텍스트에서 부르면 상태 머신이 매 주기 재생성되어 의도한 동작이 깨집니다. 헤더에 스레드/ISR 안전성이 별도로 명시되어 있지 않으므로, ISR에서는 호출하지 않는 것을 기본 원칙으로 합니다.

**예제**

```c
static XmTsmHandle_t s_tsm;

void Control_Setup(void)
{
    s_tsm = XM_TSM_Create(XM_STATE_STANDBY);
    // ... 아래 XM_TSM_AddState 호출로 이어짐
}
```

**참고**: [`XM_TSM_AddState`](#xm_tsm_addstate), [`XmStateId_e`](#xmstateid_e)

---

### `XM_TSM_AddState`

```c
void XM_TSM_AddState(XmTsmHandle_t handle, const XmStateConfig_t* config);
```

TSM에 상태 하나의 동작(진입/반복/종료 함수)을 등록합니다. `Control_Loop()` 에 진입하기 전에 사용할 상태를 모두 등록해 두어야 합니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `handle` | `XmTsmHandle_t` | `XM_TSM_Create` 로 얻은 TSM 핸들 |
| `config` | `const XmStateConfig_t*` | 상태 ID + `on_entry`/`on_loop`/`on_exit` 함수 포인터를 담은 설정 구조체 포인터. `on_entry`/`on_exit` 는 생략(자동 `NULL`) 가능 |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Setup()` 에서 `XM_TSM_Create` 직후, 등록할 상태 개수만큼 반복 호출합니다. `Control_Loop()` 안에서 매 주기 호출할 필요는 없습니다 (등록은 1회로 충분).

**예제**

```c
XmStateConfig_t sb_conf = {
    .id       = XM_STATE_STANDBY,
    .on_entry = Standby_Entry,
    .on_loop  = Standby_Loop,
    // .on_exit 는 필요 없으면 생략 (자동 NULL)
};
XM_TSM_AddState(s_tsm, &sb_conf);
```

**참고**: [`XmStateConfig_t`](#xmstateconfig_t), [`XM_TSM_Create`](#xm_tsm_create)

---

### `XM_TSM_Run`

```c
void XM_TSM_Run(XmTsmHandle_t handle);
```

현재 상태에 맞는 생명주기 함수(진입 시 1회 → 반복 실행 → 전환 요청 시 종료 1회)를 실제로 실행(dispatch)합니다. 상태를 등록만 해두고 이 함수를 호출하지 않으면 어떤 상태 함수도 실행되지 않습니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `handle` | `XmTsmHandle_t` | 실행할 TSM 핸들 |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: `Control_Loop()` 안에서 **매 주기 반드시 호출**해야 합니다. 호출 주기가 곧 상태의 `on_loop` 실행 주기가 됩니다 (일반적으로 1 kHz `Control_Loop`).

**예제**

```c
void Control_Loop(void)
{
    XM_TSM_Run(s_tsm);
}
```

**참고**: [`XM_TSM_TransitionTo`](#xm_tsm_transitionto)

---

### `XM_TSM_TransitionTo`

```c
void XM_TSM_TransitionTo(XmTsmHandle_t handle, uint8_t next_state_id);
```

다른 상태로 전환을 요청합니다. 호출 즉시 전환되는 것이 아니라, 현재 상태의 `on_exit` 이 실행된 뒤 **다음 `XM_TSM_Run` 주기**에 새 상태의 `on_entry` 가 실행됩니다.

**파라미터**

| 이름 | 타입 | 설명 |
|------|------|------|
| `handle` | `XmTsmHandle_t` | TSM 핸들 |
| `next_state_id` | `uint8_t` | 전환할 목표 상태 ID (`XM_TSM_AddState` 로 미리 등록되어 있어야 함) |

**반환값**: 없음 (`void`)

**⚠️ 호출 컨텍스트**: 일반적으로 상태의 `on_loop`(또는 `on_entry`) 콜백 안, 즉 `XM_TSM_Run` 이 호출되는 `Control_Loop()` 컨텍스트 안에서 호출합니다. "즉시 전환이 아니라 지연 전환"이라는 점은 헤더가 명시적으로 보장하는 동작입니다 — 당장 반영이 필요하면 별도 플래그로 처리해야 합니다.

**예제**

```c
static void Standby_Loop(void)
{
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_LONG_PRESS) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}
```

**참고**: [`XM_TSM_Run`](#xm_tsm_run)

---

## 타입/매크로

### `XmStateId_e`

XM10 이 권장하는 표준 상태 ID입니다.

```c
typedef enum {
    XM_STATE_OFF        = 0,
    XM_STATE_STANDBY    = 1,
    XM_STATE_ACTIVE     = 2,
    XM_STATE_ERROR      = 3,
    XM_STATE_USER_START = 10
} XmStateId_e;
```

| 값 | 설명 |
|----|------|
| `XM_STATE_OFF` = 0 | 초기 상태, 동작 정지 |
| `XM_STATE_STANDBY` = 1 | 대기 상태 (센서 On, 출력 Off) |
| `XM_STATE_ACTIVE` = 2 | 동작 중 (제어 알고리즘 수행) |
| `XM_STATE_ERROR` = 3 | 에러 발생 (안전 모드) |
| `XM_STATE_USER_START` = 10 | 사용자 정의 상태 시작 번호 |

이 값들은 강제가 아니라 **권장 규약**입니다 — 그대로 써도 되고, 10번(`XM_STATE_USER_START`)부터 완전히 다른 이름의 상태를 자유롭게 정의해도 됩니다. 다만 표준 값(0~3)과 사용자 정의 값이 겹치지 않도록 주의해야 합니다.

### `XmLifecycle_t`

TSM 생명주기 단계를 나타냅니다.

```c
typedef enum {
    XM_LIFECYCLE_ENTRY = 0,
    XM_LIFECYCLE_LOOP  = 1,
    XM_LIFECYCLE_EXIT  = 2
} XmLifecycle_t;
```

| 값 | 설명 |
|----|------|
| `XM_LIFECYCLE_ENTRY` = 0 | 진입(Initialization) — 상태 진입 시 1회 실행 |
| `XM_LIFECYCLE_LOOP` = 1 | 반복(Execution) — 상태 유지 중 매 주기 실행 |
| `XM_LIFECYCLE_EXIT` = 2 | 종료(Clean-up) — 상태 탈출 시 1회 실행 |

`XmTask_t.currentStep` / `prevStep` 필드에 담기는 값이며, 사용자가 직접 대입할 일은 거의 없고 주로 모니터링(디버깅) 용도로 읽습니다.

### `XmTask_t`

TSM 태스크 객체입니다. `XmTsmHandle_t` 가 실제로 가리키는 구조체입니다.

```c
typedef struct {
    XmStateId_e   currentStateId;
    XmLifecycle_t currentStep;
    XmStateId_e   prevStateId;
    XmLifecycle_t prevStep;
} XmTask_t;
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `currentStateId` | `XmStateId_e` | 현재 상태 ID |
| `currentStep` | `XmLifecycle_t` | 현재 상태에서의 실행 단계 (Entry/Loop/Exit) |
| `prevStateId` | `XmStateId_e` | 이전 상태 ID |
| `prevStep` | `XmLifecycle_t` | 이전 상태에서의 실행 단계 |

모든 필드는 **읽기 전용**으로 취급합니다 — 직접 대입해서 상태를 바꾸지 말고 반드시 [`XM_TSM_TransitionTo`](#xm_tsm_transitionto) 를 통해 전환하세요. STM32CubeIDE 의 Live Expression 창에 `*yourTask` 형태로 추가하면 실시간 모니터링에 유용합니다.

### `XmTsmHandle_t`

TSM 인스턴스를 가리키는 핸들 타입입니다.

```c
typedef XmTask_t* XmTsmHandle_t;
```

`XM_TSM_Create` 의 반환값이며, 이후 모든 TSM 함수의 첫 번째 인자로 사용합니다. 사용자는 이 타입의 변수(보통 `static`)를 하나 선언해 TSM 하나를 관리합니다.

### `XmStateConfig_t`

상태 하나를 정의하는 설정 구조체입니다.

```c
typedef struct {
    XmStateId_e   id;
    XmStateFunc_t on_entry;
    XmStateFunc_t on_loop;
    XmStateFunc_t on_exit;
} XmStateConfig_t;
```

| 필드 | 타입 | 설명 |
|------|------|------|
| `id` | `XmStateId_e` | 이 설정이 적용될 상태 ID |
| `on_entry` | `XmStateFunc_t` | 진입 시 1회 실행할 함수. 생략 시 `NULL` (아무 동작 없음) |
| `on_loop` | `XmStateFunc_t` | 상태 유지 중 매 주기 실행할 함수. 생략 시 `NULL` |
| `on_exit` | `XmStateFunc_t` | 탈출 시 1회 실행할 함수. 생략 시 `NULL` |

구조체 지정 초기화(`.field = value`)로 필요한 필드만 채운 뒤 포인터로 `XM_TSM_AddState` 에 넘깁니다. 세 콜백 모두 필수는 아니며, C 구조체 지정 초기화 특성상 채우지 않은 필드는 자동으로 `NULL` 이 됩니다.

### `XmStateFunc_t`

상태 콜백 함수 포인터 타입입니다.

```c
typedef void (*XmStateFunc_t)(void);
```

`on_entry`/`on_loop`/`on_exit` 에 등록하는 함수의 시그니처입니다. 매개변수와 반환값이 없는 `void func(void)` 형태의 함수만 등록할 수 있습니다 — 상태 간 데이터 전달이 필요하면 `static` 전역/파일 스코프 변수를 사용하세요.

---

## 내부 전용 (호출 금지)

이 헤더에 정의된 함수/타입 중 `[Internal]` 또는 System-only 로 표시된 항목은 없습니다 — 위에서 다룬 4개 함수와 6개 타입(구조체 2 + 열거형 2 + 함수포인터/핸들 typedef 2) 모두 공개(Public) API 입니다.

---

## Rev1.1 / Rev2.0 차이

`xm_api_tsm.h` 는 Rev1.1과 Rev2.0에서 바이트 단위로 동일합니다. 이 헤더가 다루는 TSM은 순수 소프트웨어 프레임워크로, 특정 HW 페리페럴에 의존하지 않기 때문입니다. Rev 전용 표기가 필요한 항목이 없습니다.

---

## 관련 문서

- [01. 상태 머신 (TSM) — 개념 문서](../01-task-state-machine.md) — 생명주기 설계 배경, 흔한 실수, 예제별 활용 표
- [09. 보조 task + 데이터 공유](../09-task-creation.md) — TSM 과 별도로 백그라운드 작업이 필요할 때
- [API 참고서 개요](../README.md)
