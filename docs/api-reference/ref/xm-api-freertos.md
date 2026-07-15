# `xm_api_freertos.h` — RTOS 보조 Task API

> 📄 대상 헤더: [`XM10_SDK/Rev2.0/Extension_Module/XM_FW/XM_API/xm_api_freertos.h`](https://github.com/AGR-EXO/Extension_Module/tree/Develop/XM10_SDK/Rev2.0/Extension_Module/XM_FW/XM_API/xm_api_freertos.h) (Rev1.1 동일 경로에 **바이트 단위로 동일한 파일**이 있습니다 — 이 페이지에 Rev 전용 항목은 없습니다)
> 🧭 관련 개념 문서: [09. 보조 task + 데이터 공유](../09-task-creation.md) — task 우선순위 영역, 데이터 흐름, 공유변수 패턴 4가지를 먼저 읽어보세요.
> 🧰 관련 예제: [Ex.38 Periodic_Background_Task](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/38_Periodic_Background_Task/) · [Ex.39 Task_Lifecycle](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/39_Task_Lifecycle/) · [Ex.36 OnDevice_Kinesthetic_Learning](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/36_OnDevice_Kinesthetic_Learning/) (deprecated API → 신규 API 마이그레이션 사례)

`Control_Loop()` (1 kHz 제어 루프)를 방해하지 않고 별도의 보조 task 를 안전하게 만들고, 그 task 와 데이터를 주고받기 위한 API 입니다.

---

## 언제 사용하나

Control_Loop 안에서 FFT, 신경망 학습, 큰 행렬 연산처럼 무거운 계산을 그대로 돌리면 1 ms 주기가 깨져서 제어가 불안정해집니다. 이럴 때 `XM_Task_CreateOneShot()` / `XM_Task_CreatePeriodic()` 로 별도 RTOS task 를 만들어 계산을 분리하고, `XM_Mutex_*` 로 Control_Loop 와 안전하게 데이터를 주고받습니다.

- task 우선순위를 어떻게 고를지, Control_Loop 와 어떤 흐름으로 데이터를 주고받는지는 → [09. 보조 task + 데이터 공유](../09-task-creation.md) 문서를 먼저 확인하세요.
- 이 페이지는 각 함수의 시그니처 / 파라미터 / 반환값 / 호출 컨텍스트만 다룹니다.

---

## 함수 목록

| 함수 | 한 줄 설명 |
|---|---|
| [`XM_Task_CreateOneShot`](#xm_task_createoneshot) | 한 번 실행 후 자동 종료되는 task 생성 (예: NN 학습, FFT) |
| [`XM_Task_IsComplete`](#xm_task_iscomplete) | OneShot task 가 완료(return)됐는지 확인 |
| [`XM_Task_CreatePeriodic`](#xm_task_createperiodic) | 주기적으로 실행되는 task 생성 (예: 100 Hz 로깅) |
| [`XM_Task_Suspend`](#xm_task_suspend) | Task 일시 중단 |
| [`XM_Task_Resume`](#xm_task_resume) | 중단된 task 재개 |
| [`XM_Task_Delete`](#xm_task_delete) | Task 종료 + 메모리(heap) 해제 |
| [`XM_Mutex_Create`](#xm_mutex_create) | Mutex 생성 |
| [`XM_Mutex_Lock`](#xm_mutex_lock) | Mutex 잠금 시도 |
| [`XM_Mutex_Unlock`](#xm_mutex_unlock) | Mutex 잠금 해제 |
| [`XM_Mutex_Delete`](#xm_mutex_delete) | Mutex 삭제 + 메모리 해제 |
| [`XM_Task_GetHeapFreeBytes`](#xm_task_getheapfreebytes) | 현재 시스템 heap 잔량 조회 |
| [`XM_Task_GetHeapMinEverBytes`](#xm_task_getheapmineverbytes) | 역대 최저 heap 잔량 조회 (누적 누수 감지) |
| [`XM_Task_GetInstanceCount`](#xm_task_getinstancecount) | 현재 내가 만든 task 개수 조회 |
| [`XM_Task_GetBudgetRemainingBytes`](#xm_task_getbudgetremainingbytes) | 사용자 task heap budget 잔량 조회 |
| [`XM_RTOS_PrintTaskList`](#xm_rtos_printtasklist) | 현재 동작 중인 task 목록 출력 (진단용) |

> ⚠️ `XM_BgTask_Create` / `XM_BgTask_IsDone` 은 **Deprecated** 입니다 — 표에서 제외했습니다. 아래 [Deprecated API](#deprecated-api-사용-금지-권장) 절을 참고하세요.

---

## 함수 상세

### OneShot Task API

#### `XM_Task_CreateOneShot`

```c
XmTaskHandle_t XM_Task_CreateOneShot(const char*         name,
                                      XmTaskOneShotFunc_t func,
                                      void*               arg,
                                      XmTaskPrio_t        prio_hint);
```

한 번 실행되고 함수가 `return` 하면 자동으로 종료되는 task 를 만듭니다. 신경망 학습, FFT 같이 시간이 걸리는 계산을 Control_Loop 밖에서 돌릴 때 사용합니다.

| 이름 | 타입 | 설명 |
|---|---|---|
| `name` | `const char*` | Task 이름 (10자 이내 권장). `NULL`, 빈 문자열, 이미 사용 중인 이름은 거부됩니다. |
| `func` | `XmTaskOneShotFunc_t` | task 로 실행할 함수. `return` 하면 완료 처리됩니다. |
| `arg` | `void*` | `func` 에 전달할 인자. **static 변수나 heap 포인터만** 넘기세요 — 호출한 함수의 지역(stack) 변수 주소는 task 가 실행되는 시점에 이미 사라졌을 수 있어 금지입니다. |
| `prio_hint` | `XmTaskPrio_t` | 우선순위 힌트. 특별한 이유가 없다면 `XM_PRIO_BACKGROUND` 를 사용하세요. |

**반환값**: 성공 시 유효한 `XmTaskHandle_t` 핸들, 거부되면 `NULL` (+ USB-CDC 로 `[XM-WARN]` 메시지 출력).

> ⚠️ **호출 컨텍스트**: Control_Setup()/Control_Loop() 컨텍스트 기준입니다(헤더에 별도 ISR-safe 명시 없음 — ISR 안에서 호출하지 마세요). 동시에 최대 [`XM_TASK_MAX_INSTANCES`](#매크로) (4개) 까지만 만들 수 있고, 완료된 task 도 [`XM_Task_Delete`](#xm_task_delete) 로 지우기 전까지는 이 개수에 포함됩니다.

```c
static XmTaskHandle_t s_heavy;

if (trigger && s_heavy == NULL) {
    s_heavy = XM_Task_CreateOneShot("HeavyCalc", _HeavyCalc, NULL, XM_PRIO_BACKGROUND);
}
```

**참고**: [`XM_Task_IsComplete`](#xm_task_iscomplete), [`XM_Task_Delete`](#xm_task_delete), Ex.39 `task_lifecycle.c`

---

#### `XM_Task_IsComplete`

```c
bool XM_Task_IsComplete(XmTaskHandle_t handle);
```

OneShot task 의 함수 본체가 이미 `return` 해서 완료됐는지 확인합니다.

| 이름 | 타입 | 설명 |
|---|---|---|
| `handle` | `XmTaskHandle_t` | `XM_Task_CreateOneShot()` 이 반환한 핸들 |

**반환값**: `true` — 함수가 return 하여 완료됨 / `false` — 아직 실행 중이거나 잘못된(또는 이미 삭제된) 핸들.

> ⚠️ **호출 컨텍스트**: Control_Setup()/Control_Loop() 컨텍스트 기준(헤더에 별도 ISR-safe 명시 없음).

```c
if (s_heavy != NULL && XM_Task_IsComplete(s_heavy)) {
    XM_Task_Delete(s_heavy);
    s_heavy = NULL;   /* dangling pointer 방지 — 반드시 NULL 대입 */
}
```

**참고**: [`XM_Task_CreateOneShot`](#xm_task_createoneshot), [`XM_Task_Delete`](#xm_task_delete)

---

### Periodic Task API

#### `XM_Task_CreatePeriodic`

```c
XmTaskHandle_t XM_Task_CreatePeriodic(const char*          name,
                                       XmTaskPeriodicFunc_t func,
                                       uint32_t             period_ms,
                                       XmTaskPrio_t         prio_hint);
```

지정한 주기마다 반복 호출되는 task 를 만듭니다. 100 Hz 로깅, 10 Hz UI 갱신처럼 Control_Loop 와 별도 주기로 계속 돌아야 하는 작업에 사용합니다.

| 이름 | 타입 | 설명 |
|---|---|---|
| `name` | `const char*` | Task 이름 |
| `func` | `XmTaskPeriodicFunc_t` | 매 주기 호출될 함수. **인자가 없습니다** — Control_Loop 와는 공유 변수(volatile 또는 Mutex)로 데이터를 주고받으세요. |
| `period_ms` | `uint32_t` | 호출 주기 (ms). **1 ms 는 Control_Loop 전용**이므로 최소 2 ms 이상을 권장합니다. |
| `prio_hint` | `XmTaskPrio_t` | 우선순위 힌트 |

**반환값**: 성공 시 유효한 핸들, 거부되면 `NULL`.

> ⚠️ **호출 컨텍스트**: Control_Setup()/Control_Loop() 컨텍스트 기준(헤더에 별도 ISR-safe 명시 없음).

```c
static XmMutexHandle_t s_buf_mutex;
static XmTaskHandle_t  s_summary_task;

void Control_Setup(void) {
    s_buf_mutex    = XM_Mutex_Create();
    s_summary_task = XM_Task_CreatePeriodic("AdcSummary", _SummaryTask,
                                             10U /* 10 ms = 100 Hz */,
                                             XM_PRIO_BACKGROUND);
}
```

**참고**: [09. 보조 task + 데이터 공유 §3 공유변수 패턴](../09-task-creation.md), Ex.38 `periodic_bg_task.c`

---

### Task Lifecycle API

#### `XM_Task_Suspend`

```c
void XM_Task_Suspend(XmTaskHandle_t handle);
```

task 를 일시 중단합니다. [`XM_Task_Resume`](#xm_task_resume) 을 호출하기 전까지 다시 실행되지 않습니다.

| 이름 | 타입 | 설명 |
|---|---|---|
| `handle` | `XmTaskHandle_t` | 중단할 task 핸들 |

**반환값**: 없음 (`void`).

> ⚠️ **호출 컨텍스트**: Control_Setup()/Control_Loop() 컨텍스트 기준(헤더에 별도 ISR-safe 명시 없음). 잘못된 핸들을 넘겨도 크래시 없이 무시됩니다(다른 API 와 동일한 방어 패턴).

**참고**: [`XM_Task_Resume`](#xm_task_resume)

---

#### `XM_Task_Resume`

```c
void XM_Task_Resume(XmTaskHandle_t handle);
```

[`XM_Task_Suspend`](#xm_task_suspend) 로 멈춰둔 task 를 다시 실행시킵니다.

| 이름 | 타입 | 설명 |
|---|---|---|
| `handle` | `XmTaskHandle_t` | 재개할 task 핸들 |

**반환값**: 없음 (`void`).

> ⚠️ **호출 컨텍스트**: Control_Setup()/Control_Loop() 컨텍스트 기준(헤더에 별도 ISR-safe 명시 없음).

**참고**: [`XM_Task_Suspend`](#xm_task_suspend)

---

#### `XM_Task_Delete`

```c
void XM_Task_Delete(XmTaskHandle_t handle);
```

task 를 종료하고 관련 메모리(heap)를 회수합니다. task 를 하나 만들었으면 언젠가 반드시 `Delete` 로 짝을 맞춰야 heap 누수가 생기지 않습니다.

| 이름 | 타입 | 설명 |
|---|---|---|
| `handle` | `XmTaskHandle_t` | 삭제할 task 핸들 |

**반환값**: 없음 (`void`).

- **OneShot**: [`XM_Task_IsComplete`](#xm_task_iscomplete) 로 완료를 확인한 뒤 호출하는 것을 권장합니다.
- **Periodic**: 완료라는 개념이 없으므로 언제든 호출해서 강제 종료할 수 있습니다.
- **자기 자신 삭제 금지**: 현재 실행 중인(자기 자신의) task 를 스스로 `Delete` 하려는 시도는 거부됩니다(헤더 명시 — task 함수 안에서 `XM_Task_Delete(자기_핸들)` 을 호출하지 마세요).

> ⚠️ **호출 컨텍스트**: Control_Setup()/Control_Loop() 컨텍스트 기준(헤더에 별도 ISR-safe 명시 없음).

```c
if (s_heavy != NULL && XM_Task_IsComplete(s_heavy)) {
    XM_Task_Delete(s_heavy);
    s_heavy = NULL;   /* 필수: dangling pointer 방지 */
}
```

**참고**: [`XM_Task_CreateOneShot`](#xm_task_createoneshot), [`XM_Task_CreatePeriodic`](#xm_task_createperiodic), Ex.39 `task_lifecycle.c`

---

### Mutex API

#### `XM_Mutex_Create`

```c
XmMutexHandle_t XM_Mutex_Create(void);
```

Mutex 를 하나 생성합니다. 배열/구조체처럼 **여러 워드**로 이루어진 데이터를 Control_Loop 와 보조 task 가 함께 읽고 쓸 때 이 Mutex 로 보호합니다.

**파라미터**: 없음.

**반환값**: 성공 시 유효한 `XmMutexHandle_t`, 실패 시 `NULL`.

> ⚠️ **호출 컨텍스트**: Control_Setup()/Control_Loop() 컨텍스트 기준(헤더에 별도 ISR-safe 명시 없음). 보통 `Control_Setup()` 에서 한 번만 만들어 두고 계속 재사용합니다.

```c
static XmMutexHandle_t s_buf_mutex;

void Control_Setup(void) {
    s_buf_mutex = XM_Mutex_Create();
}
```

**참고**: [`XM_Mutex_Lock`](#xm_mutex_lock), [09. 보조 task + 데이터 공유 §3 공유변수 패턴](../09-task-creation.md)

---

#### `XM_Mutex_Lock`

```c
bool XM_Mutex_Lock(XmMutexHandle_t mutex, uint32_t timeout_ms);
```

Mutex 잠금을 시도합니다.

| 이름 | 타입 | 설명 |
|---|---|---|
| `mutex` | `XmMutexHandle_t` | 잠글 Mutex 핸들 |
| `timeout_ms` | `uint32_t` | 잠금 대기 시간(ms). **Control_Loop 안에서는 항상 0 을 사용하세요** (헤더 명시 — 절대 블로킹 금지, 1 ms 주기가 깨집니다). 보조 task 안에서도 0 을 권장합니다. |

**반환값**: `true` — 잠금 성공 / `false` — timeout 이거나 잘못된 핸들.

> ⚠️ **호출 컨텍스트**: Control_Setup()/Control_Loop() 컨텍스트 기준(헤더에 별도 ISR-safe 명시 없음). `timeout_ms > 0` 을 Control_Loop 안에서 쓰면 최악의 경우 그만큼 1 kHz 주기가 밀립니다 — 반드시 `0` + 실패 시 skip 패턴을 쓰세요.

```c
/* Control_Loop — 1 kHz Writer, 항상 timeout=0 */
if (XM_Mutex_Lock(s_buf_mutex, 0U)) {
    s_adc_buf[s_adc_idx] = XM_AnalogRead(XM_EXT_ADC_1);
    s_adc_idx = (s_adc_idx + 1) % ADC_BUF_SIZE;
    XM_Mutex_Unlock(s_buf_mutex);
}
/* 실패 시 이번 cycle 의 샘플은 그냥 건너뜁니다 (jitter 보존) */
```

**참고**: [`XM_Mutex_Unlock`](#xm_mutex_unlock), Ex.38 `periodic_bg_task.c`

---

#### `XM_Mutex_Unlock`

```c
void XM_Mutex_Unlock(XmMutexHandle_t mutex);
```

Mutex 잠금을 해제합니다. **잠갔던 task 만** 해제할 수 있습니다.

| 이름 | 타입 | 설명 |
|---|---|---|
| `mutex` | `XmMutexHandle_t` | 해제할 Mutex 핸들 |

**반환값**: 없음 (`void`).

> ⚠️ **호출 컨텍스트**: Control_Setup()/Control_Loop() 컨텍스트 기준(헤더에 별도 ISR-safe 명시 없음). `XM_Mutex_Lock()` 성공 후 early return 하기 전에 반드시 `Unlock` 을 호출하세요 — 누락하면 그 Mutex 는 영구히 잠긴 채로 남습니다.

**참고**: [`XM_Mutex_Lock`](#xm_mutex_lock)

---

#### `XM_Mutex_Delete`

```c
void XM_Mutex_Delete(XmMutexHandle_t mutex);
```

Mutex 를 삭제하고 메모리를 회수합니다.

| 이름 | 타입 | 설명 |
|---|---|---|
| `mutex` | `XmMutexHandle_t` | 삭제할 Mutex 핸들 |

**반환값**: 없음 (`void`).

> ⚠️ **호출 컨텍스트**: Control_Setup()/Control_Loop() 컨텍스트 기준(헤더에 별도 ISR-safe 명시 없음). 이 Mutex 를 사용하는 task 가 아직 남아있는 상태에서 삭제하면 안 됩니다 — 관련 task 를 먼저 [`XM_Task_Delete`](#xm_task_delete) 로 정리한 뒤 호출하세요.

```c
XM_Task_Delete(s_summary_task);
XM_Mutex_Delete(s_buf_mutex);
```

**참고**: [`XM_Mutex_Create`](#xm_mutex_create), [`XM_Task_Delete`](#xm_task_delete)

---

### HW 제약 진단 API

이 네 함수는 모두 파라미터가 없고, 현재 상태를 숫자 하나로 즉시 반환하는 단순 조회(getter) 함수입니다. [09. 보조 task + 데이터 공유 §4](../09-task-creation.md) 에서 설명하는 HW 제약(최대 4개 task, 32 KB heap budget)을 코드에서 직접 확인하고 싶을 때 사용합니다.

#### `XM_Task_GetHeapFreeBytes`

```c
uint32_t XM_Task_GetHeapFreeBytes(void);
```

현재 시스템 heap 의 잔여 바이트 수를 반환합니다.

**반환값**: 잔여 heap (bytes).

> ⚠️ **호출 컨텍스트**: Control_Setup()/Control_Loop() 컨텍스트 기준(헤더에 별도 ISR-safe 명시 없음).

---

#### `XM_Task_GetHeapMinEverBytes`

```c
uint32_t XM_Task_GetHeapMinEverBytes(void);
```

시스템 heap 이 지금까지 가장 적게 남았던 시점의 잔량을 반환합니다. task 생성/삭제를 반복해도 이 값이 계속 줄어든다면 heap 누수(예: `XM_Task_Delete` 누락)를 의심할 수 있습니다.

**반환값**: 역대 최저 heap 잔량 (bytes).

> ⚠️ **호출 컨텍스트**: Control_Setup()/Control_Loop() 컨텍스트 기준(헤더에 별도 ISR-safe 명시 없음). Ex.39 는 버튼을 3회 눌러 OneShot task 생성/삭제를 반복한 뒤 이 값이 더 줄지 않는지로 누수 여부를 검증합니다.

**참고**: Ex.39 `README.md` (heap 누수 검증 시나리오)

---

#### `XM_Task_GetInstanceCount`

```c
uint32_t XM_Task_GetInstanceCount(void);
```

지금까지 만들고 아직 `Delete` 하지 않은(완료됐지만 Delete 전인 OneShot 포함) 사용자 task 개수를 반환합니다.

**반환값**: 현재 사용자 task 개수 (0 ~ [`XM_TASK_MAX_INSTANCES`](#매크로)).

> ⚠️ **호출 컨텍스트**: Control_Setup()/Control_Loop() 컨텍스트 기준(헤더에 별도 ISR-safe 명시 없음). 이 값이 4에 도달하면 이후 `XM_Task_CreateOneShot`/`CreatePeriodic` 은 모두 `NULL` 을 반환합니다 — 새 task 를 만들기 전에 확인해보면 원인 파악에 도움이 됩니다.

---

#### `XM_Task_GetBudgetRemainingBytes`

```c
uint32_t XM_Task_GetBudgetRemainingBytes(void);
```

사용자 task 전용 heap budget(총 [`XM_TASK_HEAP_BUDGET_BYTES`](#매크로), 32 KB) 중 남은 양을 반환합니다.

**반환값**: budget 잔량 (bytes).

> ⚠️ **호출 컨텍스트**: Control_Setup()/Control_Loop() 컨텍스트 기준(헤더에 별도 ISR-safe 명시 없음). 이 값이 부족하면 stack 이 큰 task(예: `XM_PRIO_BACKGROUND` 기본 8 KB)의 생성이 거부될 수 있습니다.

---

### 진단 API

#### `XM_RTOS_PrintTaskList`

```c
void XM_RTOS_PrintTaskList(void);
```

현재 동작 중인 task 목록을 출력합니다(on-demand 진단용).

**파라미터**: 없음. **반환값**: 없음 (`void`).

> ⚠️ **현재 상태(Phase 1)**: 이 SDK 버전에서는 진단 출력 채널이 아직 연결되어 있지 않아 **호출해도 아무 동작이 없는 no-op stub** 입니다(헤더 명시). Phase 2 에서 PhAI Studio 진단 채널과 연동될 예정입니다.
>
> ⚠️ **호출 컨텍스트(미리 대비)**: 지금은 stub 이라 어디서 불러도 안전하지만, 이 함수를 감싸는 내부 구현은 "**1 kHz Control_Loop 안에서 호출 금지**(문자열 포맷팅 누적으로 100 µs 이상 걸릴 수 있음)"라는 제약을 이미 명시해두고 있습니다. Phase 2 연동 이후에도 그대로 안전하게 쓰려면 지금부터 `Control_Setup()` 이나 버튼 이벤트 핸들러처럼 **1 kHz 로 돌지 않는 곳**에서만 호출하는 습관을 들이는 것을 권장합니다.

---

## Deprecated API (사용 금지 권장)

v1.0 호환을 위해 남아있는 API 입니다. **다음 major 릴리즈에서 제거될 예정**이므로 새 코드에서는 사용하지 마세요. 아래 표만 남기고 함수 상세는 생략합니다.

| 함수 | 대체 API | 비고 |
|---|---|---|
| `XmBgTaskHandle_t XM_BgTask_Create(const char* name, XmBgTaskFunc_t func, void* arg, uint32_t stack_words)` | [`XM_Task_CreateOneShot`](#xm_task_createoneshot)`(name, func, arg, XM_PRIO_BACKGROUND)` | 컴파일러가 `deprecated` 경고를 냅니다. 완료 후 [`XM_Task_Delete`](#xm_task_delete) 호출이 필수라는 점은 신규 API 와 동일합니다. |
| `bool XM_BgTask_IsDone(XmBgTaskHandle_t handle)` | [`XM_Task_IsComplete`](#xm_task_iscomplete) | 마찬가지로 `deprecated` 경고 대상입니다. |

> Ex.36 (`OnDevice_Kinesthetic_Learning`) 이 실제로 `XM_BgTask_Create` → `XM_Task_CreateOneShot` 마이그레이션을 거친 예시입니다 (소스 주석에 `[2026-05-13] XM_BgTask_Create → XM_Task_CreateOneShot 마이그레이션` 으로 기록되어 있습니다).

---

## 타입 정의

### `XmTaskPrio_t`

Task 우선순위 힌트 enum 입니다. CMSIS-OS2 의 숫자 우선순위 값을 그대로 노출하는 대신, 의미 단위로 감싸서 제공합니다. 시스템 task 가 이미 점유한 숫자 영역(51~55)은 이 enum 에 존재하지 않습니다 — 실수로라도 시스템 task 와 우선순위가 겹칠 수 없습니다.

```c
typedef enum {
    XM_PRIO_IDLE          = 8,   /* osPriorityLow      — DefaultTask 와 동급 */
    XM_PRIO_BACKGROUND    = 24,  /* osPriorityNormal   — 권장 기본값 */
    XM_PRIO_BELOW_CONTROL = 32,  /* osPriorityAboveNormal — Control_Loop 보다 낮음 */
    XM_PRIO_ABOVE_CONTROL = 40,  /* osPriorityHigh     — USBH 와 동급, Control_Loop 보다 낮음 */
    XM_PRIO_NEAR_REALTIME = 48,  /* osPriorityRealtime — 주의: PnP 통신과 경합 가능 */
} XmTaskPrio_t;
```

| 값 | 숫자 | 기본 stack | 설명 |
|---|---|---|---|
| `XM_PRIO_IDLE` | 8 | 1 KB | `DefaultTask` 와 동급인 가장 낮은 우선순위 |
| `XM_PRIO_BACKGROUND` | 24 | 8 KB | **권장 기본값** |
| `XM_PRIO_BELOW_CONTROL` | 32 | 4 KB | Control_Loop 보다는 낮음 |
| `XM_PRIO_ABOVE_CONTROL` | 40 | 2 KB | USBH 와 동급 — Control_Loop 보다는 여전히 낮음 |
| `XM_PRIO_NEAR_REALTIME` | 48 | 2 KB | ⚠️ PnP 통신(`PnP_Task`, 25)과는 경합하지 않지만 그 위 시스템 task 와는 가까우므로 남용하지 마세요. |

우선순위 영역이 시스템 전체에서 어떻게 배치되는지(51~55 가 왜 제외됐는지)는 [09. 보조 task + 데이터 공유 §1~2](../09-task-creation.md) 에서 전체 그림을 확인하세요.

### 핸들 타입

| 타입 | 정의 | 설명 |
|---|---|---|
| `XmTaskHandle_t` | `typedef void*` | Task 핸들. `NULL` 이면 유효하지 않은 핸들입니다. |
| `XmMutexHandle_t` | `typedef void*` | Mutex 핸들. `NULL` 이면 유효하지 않은 핸들입니다. |

### 함수 포인터 타입

| 타입 | 시그니처 | 설명 |
|---|---|---|
| `XmTaskOneShotFunc_t` | `void (*)(void* arg)` | [`XM_Task_CreateOneShot`](#xm_task_createoneshot) 의 `func` 파라미터 타입 |
| `XmTaskPeriodicFunc_t` | `void (*)(void)` | [`XM_Task_CreatePeriodic`](#xm_task_createperiodic) 의 `func` 파라미터 타입 (인자 없음 — 공유 변수로 데이터 전달) |

---

## 매크로

### 시스템 가드 상수

학생이 직접 바꿀 수 없는 값이지만, 코드에서 조건 분기(예: `if (XM_Task_GetInstanceCount() < XM_TASK_MAX_INSTANCES)`) 에 활용할 수 있도록 헤더에 그대로 노출되어 있습니다.

| 매크로 | 값 | 의미 |
|---|---|---|
| `XM_TASK_MAX_INSTANCES` | `4U` | 동시에 만들 수 있는 사용자 task 최대 개수 |
| `XM_TASK_HEAP_BUDGET_BYTES` | `32768U` (32 KB) | 사용자 task 전체가 쓸 수 있는 heap 총량 |
| `XM_TASK_STACK_MAX_WORDS` | `2048U` (8 KB) | 단일 task 최대 stack (word = 4 byte) |
| `XM_TASK_STACK_MIN_WORDS` | `128U` (512 B) | 단일 task 최소 stack |

### `prio_hint` 별 기본 stack

`XM_Task_CreateOneShot`/`CreatePeriodic` 호출 시 stack 크기를 직접 지정하지 않고, `prio_hint` 에 따라 아래 값이 자동으로 적용됩니다.

| 매크로 | 값 (words) | 바이트 환산 | 대응 `prio_hint` |
|---|---|---|---|
| `XM_TASK_STACK_IDLE_WORDS` | `256U` | 1 KB | `XM_PRIO_IDLE` |
| `XM_TASK_STACK_BACKGROUND_WORDS` | `2048U` | 8 KB | `XM_PRIO_BACKGROUND` |
| `XM_TASK_STACK_BELOW_WORDS` | `1024U` | 4 KB | `XM_PRIO_BELOW_CONTROL` |
| `XM_TASK_STACK_ABOVE_WORDS` | `512U` | 2 KB | `XM_PRIO_ABOVE_CONTROL` |
| `XM_TASK_STACK_NRT_WORDS` | `512U` | 2 KB | `XM_PRIO_NEAR_REALTIME` |

> `XM_TASK_STACK_BACKGROUND_WORDS` (8 KB) 는 헤더 주석에 "Ex.36 NN_Train 실측치" 로 기록되어 있습니다 — 일반적인 알고리즘 계산에도 충분한 크기입니다.

---

## 흔한 실수

자세한 pitfalls 표는 [09. 보조 task + 데이터 공유 §6](../09-task-creation.md) 에 정리되어 있습니다. 이 헤더와 직접 관련된 것만 요약하면:

| 실수 | 결과 | 대응 |
|---|---|---|
| Control_Loop 안에서 `XM_Mutex_Lock(m, 0 보다 큰 값)` | 1 ms 주기 깨짐 | 항상 `timeout_ms = 0`, 실패 시 skip |
| `XM_Mutex_Lock` 성공 후 `XM_Mutex_Unlock` 누락 | mutex 영구 점유 | early return 전에 반드시 Unlock |
| `XM_Task_Delete` 후 handle 재사용 | 이후 호출이 모두 `false`/`NULL` 반환 | Delete 직후 `handle = NULL` 대입 |
| `XM_Task_Delete` 자체를 누락 | 4개 한도 도달 → 이후 Create 가 `NULL` | OneShot 은 `IsComplete` → `Delete` 사이클 지키기 |
| task 함수 안에서 자기 자신을 `XM_Task_Delete` | 거부됨(no-op) | 외부(Control_Loop 등)에서 `IsComplete` 확인 후 Delete |

---

## 관련 문서

- [09. 보조 task + 데이터 공유](../09-task-creation.md) — 시스템 task 인벤토리, 우선순위 영역, 데이터 흐름, 공유변수 패턴
- [Ex.38 Periodic_Background_Task](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/38_Periodic_Background_Task/)
- [Ex.39 Task_Lifecycle](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/39_Task_Lifecycle/)
- [Ex.36 OnDevice_Kinesthetic_Learning](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/36_OnDevice_Kinesthetic_Learning/)
