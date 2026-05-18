# Ex.39 — OneShot Task Lifecycle

> 보조 task 의 생성 → 완료 polling → 해제 한 사이클 학습. Heap 누수 0 보장 패턴.

## 학습 포인트

1. `XM_Task_CreateOneShot()` — 1회 실행 후 자동 종료되는 task
2. `XM_Task_IsComplete()`    — 본체 함수가 return 했는지 외부에서 polling
3. `XM_Task_Delete()`        — 명시 해제 (heap 회수)

## Lifecycle 순환

```
┌── BTN_1 PRESSED ──┐
│                    ▼
│            Create OneShot ─▶ _HeavyCalc 실행 (200ms) ─▶ osThreadExit
│                    │                                          │
│                    ▼                                          ▼
│             (LED 1 ON)                            slot->done = true
│                                                              │
└───────────── IsComplete ◀────────────────────────────────────┘
                  │
                  ▼
        Delete + handle = NULL ─▶ (LED 1 OFF)
```

## 핵심 패턴 — 매 cycle 반복 호출 안전

```c
/* 1. 트리거 — 동일 task 중복 생성 방지 */
if (event_triggered && s_heavy == NULL) {
    s_heavy = XM_Task_CreateOneShot(...);
}

/* 2. 완료 polling → Delete + handle nil */
if (s_heavy != NULL && XM_Task_IsComplete(s_heavy)) {
    XM_Task_Delete(s_heavy);
    s_heavy = NULL;     /* ← 필수! dangling pointer 방지 */
}
```

## 회로 / 보드 셋업

- Button 1 (`XM_BTN_1`) — 보드 좌측 버튼.
- LED 1   (`XM_LED_1`) — heavy task 동작 중 ON.

## 실행 / 검증

1. `task_lifecycle.c` 를 `XM_Apps/Control_Task/control_task.c` 자리에 복붙.
2. 빌드 → 플래시.
3. BTN_1 누름 → LED 1 ON → 200 ms 후 자동 OFF.
4. 버튼을 3회 누른 뒤 `XM_Task_GetHeapMinEverBytes()` 값이 안정 (감소 없음)
   → heap 누수 0 ✓.

## 흔한 실수

> "Delete 후 같은 변수에 다시 Create 했는데 동작 안 해요."
> → Delete 직후 `s_heavy = NULL` 안 했을 가능성. 가드 O 가 차단 (false 반환).

> "버튼 한 번 눌렀는데 task 가 여러 개 생성됐어요."
> → 가드 B 가 4개에서 거부합니다. 본 예제처럼 `s_heavy == NULL` 가드 필수.

## API contract 가드 (자동 적용)

| 가드 | 조건 | 동작 |
|---|---|---|
| **B** | 동시 4개 한도 초과 | Create 반환 NULL |
| **K** | period_ms = 0 (Periodic 전용) | Create 반환 NULL |
| **O** | Delete 후 handle 재사용 | API false 반환 |
| **P** | 이름 NULL / 빈 문자열 | Create 반환 NULL |
| **U** | task 본체에서 본인 handle Delete 시도 | Delete 거부 (no-op) |

## 다음 단계

- 본 예제 + Ex.38 패턴 결합 → Periodic + OneShot 혼합
- 자세한 토폴로지: [`docs/dev/12_Task_Topology.md`](../../docs/dev/12_Task_Topology.md)
