# API 레퍼런스: Task State Machine

`XM10`의 펌웨어 아키텍처는 **상태 머신(State Machine)** 기반의 설계를 핵심으로 합니다. 이는 복잡한 제어 로직을 `OFF`, `STANDBY`, `ACTIVE`, `ERROR` 등 명확하게 정의된 '상태'와 상태 간의 '전환'으로 단순화하여, 코드의 가독성을 높이고 버그 발생 가능성을 줄이는 강력한 프로그래밍 모델입니다.

본 API를 사용하면, 사용자는 복잡한 내부 구현을 알 필요 없이 자신의 알고리즘을 체계적인 상태 머신으로 손쉽게 구성할 수 있습니다.

---

## 주요 타입 (Types)

### `TaskState_t`

애플리케이션의 주요 상태를 정의하는 열거형입니다.

```c
typedef enum {
    TASK_STATE_OFF       = 0, // 전원 꺼짐 또는 비활성화 상태
    TASK_STATE_STANDBY   = 1, // 대기 상태
    TASK_STATE_ACTIVE    = 2, // 활성 상태 (메인 로직 수행)
    TASK_STATE_ERROR     = 3, // 오류 발생 상태
} TaskState_t;
```

### `TaskHandle`

생성된 상태 머신 태스크를 가리키는 고유한 핸들입니다. 이 핸들을 통해 특정 상태 머신을 제어하게 됩니다. 사용자는 반드시 `TaskHandle yourTask`를 정의해주어야 합니다.

---

## 함수 목록 (Functions)

### `CreateTask()`

새로운 상태 머신 태스크를 생성하고, 이에 대한 핸들을 반환합니다. 모든 상태 머신 로직의 시작점입니다.

**Syntax**
```c
TaskHandle CreateTask(void);
```

**Parameters**
- 없음

**Returns**
- `TaskHandle`: 성공적으로 생성된 태스크의 핸들입니다.
- `NULL`: 최대 생성 가능한 태스크 개수(`8개`)를 초과하여 생성에 실패한 경우 반환됩니다.

**Example**
```c
// 전역 변수로 태스크 핸들을 선언합니다.
static TaskHandle s_mainTaskHandle = NULL;

void InitUserAlgorithm(void) {
    // 애플리케이션 초기화 시 새로운 태스크를 생성합니다.
    s_mainTaskHandle = CreateTask();
    if (s_mainTaskHandle == NULL) {
        // 핸들 생성 실패 처리
    }
}
```

---

### `AddTaskState()`

생성된 태스크에 특정 상태(`TaskState_t`)와 해당 상태의 동작을 정의하는 함수들을 등록합니다.

**Syntax**
```c
void AddTaskState(TaskHandle handle,
                  TaskState_t state,
                  StateFunc_t onEnterFunc,
                  StateFunc_t onRunFunc,
                  StateFunc_t onExitFunc,
                  bool isDefault);
```

**Parameters**
- `handle`: 상태를 추가할 태스크의 `TaskHandle`.
- `state`: 등록할 상태 (`TASK_STATE_STANDBY` 등).
- `onEnterFunc`: 해당 상태에 처음 진입할 때 **단 한 번** 호출될 함수입니다. 초기화 로직에 적합하며, 필요 없으면 `NULL`을 전달합니다.
- `onRunFunc`: 해당 상태에 머무는 동안 제어 주기(2ms)마다 **반복적으로** 호출될 함수입니다. 메인 알고리즘 로직을 여기에 작성하며, 필요 없으면 `NULL`을 전달합니다.
- `onExitFunc`: 해당 상태에서 다른 상태로 전환될 때 **단 한 번** 호출될 함수입니다. 마무리 로직에 적합하며, 필요 없으면 `NULL`을 전달합니다.
- `isDefault`: `true`로 설정하면 이 상태가 태스크의 시작 상태로 지정됩니다.

**Returns**
- 없음

**Example**
```c
// 각 상태별 실행 함수들을 미리 정의합니다.
void UpdateOffState(void);     { /* 비활성화 상태 반복 로직 */ }
void UpdateStandbyState(void); { /* 대기 상태 반복 로직 */ }
void EnterActive(void);        { /* 활성 상태 진입 로직 */ }
void UpdateActiveState(void);  { /* 활성 상태 반복 로직 */ }

void InitUserAlgorithm(void)
{
    // 애플리케이션 초기화 시 새로운 태스크를 생성합니다.
    s_mainTaskHandle = CreateTask();
    if (s_mainTaskHandle == NULL) {
        // 핸들 생성 실패 처리
    }

    // 각 상태와 실행 함수들을 등록합니다.
    // OFF 상태를 기본 시작 상태로 설정합니다.
    AddTaskState(s_mainTaskHandle, TASK_STATE_OFF,     NULL,         UpdateOffState,     NULL,    true);
    AddTaskState(s_mainTaskHandle, TASK_STATE_STANDBY, NULL,         UpdateStandbyState, NULL,    false);
    AddTaskState(s_mainTaskHandle, TASK_STATE_ACTIVE,  EnterActive,  UpdateActiveState,  NULL,    false);
    AddTaskState(s_mainTaskHandle, TASK_STATE_ERROR,   NULL,         NULL,               NULL,    false);
}
```

---

### `RunTask()`

상태 머신을 한 사이클 실행합니다. 이 함수는 메인 제어 루프(2ms 주기)에서 **주기적으로 호출**. 현재 상태에 맞는 `onRunFunc` 함수가 계속 실행됩니다.

**Syntax**
```c
void RunTask(TaskHandle handle);
```

**Parameters**
- `handle`: 실행할 태스크의 `TaskHandle`.

**Returns**
- 없음

**Example**
```c
// 2ms 주기로 실행되는 메인 제어 함수
void RunUserAlgorithm(void) {
    // RunTask를 호출하여 현재 상태의 onRun 함수를 실행합니다.
    RunTask(s_mainTaskHandle);
}
```

---

### `TransitionTaskTo()`

태스크의 현재 상태를 다른 상태로 안전하게 전환합니다. 이 함수가 호출되면, 상태 머신은 현재 상태의 `onExit` 로직을 수행한 뒤, 새로운 상태의 `onEnter` 로직을 실행하는 전환 절차를 밟습니다.

**Syntax**
```c
void TransitionTaskTo(TaskHandle handle, TaskState_t targetState);
```

**Parameters**
- `handle`: 상태를 전환할 태스크의 `TaskHandle`.
- `targetState`: 전환을 원하는 목표 상태 (`TASK_STATE_ACTIVE` 등).

**Returns**
- 없음

**Example**
```c
void UpdateOffState(void) {
    // 대기 상태에서 CM 연결이 확인되면 Standby 상태로 전환합니다.
    if (IsCmConnected()) {
        TransitionTaskTo(s_mainTaskHandle, TASK_STATE_STANDBY);
    }
}
```
