# Ex.19 — Memory Aware Design (malloc 없는 정적 설계 + Ring Buffer + Pool)

> 🎯 **학습 목표**:
> - 임베디드 양산 코드에서 **`malloc` 금지** 이유 + 정적 메모리 패턴 2종 직접 구현.
> - **Ring Buffer** (이동 평균 필터) + **Pool Allocator** (이벤트 로깅) — STL/heap 없이.
> - `sizeof()` 실시간 메모리 리포트 + 컴파일러 패딩 체험.
>
> ⏱️ 권장 시간: 40분 | 🔧 난이도: ⭐⭐⭐
> 🧰 사전 예제: [Ex.10b MSC Custom Struct](../10b_MSC_Custom_Struct/) + [Ex.18 Debug Monitor](../18_Debug_Monitor/) | 📚 관련 docs: [Memory Management](../../docs/api-reference/07-memory-management.md)

---

> ⚠️ **USB-CDC 단일 점유** — 본 예제 실행 중 PhAI Studio 를 동시에 열어두지 마세요 (같은 COM 포트 충돌). 실시간 그래프 필요 시 PhAI Studio 만 단독 실행.

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

H10 우측 고관절 각도를 **2개의 정적 자료구조** 로 처리:

| 자료구조 | 크기 | 용도 |
|---------|------|------|
| **Ring Buffer** (50 floats + meta) | ~210 B | 50-샘플 이동 평균 필터 (50 ms 윈도우 @ 1 kHz) |
| **Pool Allocator** (10 × DataSlot_t + flags) | ~210 B | 급격한 각도 변화 (|delta|>5°) 이벤트 로깅 |

USB 스트리밍 4채널 (0xF0): `Raw Angle / Filtered / Pool Used / Ring Count`
USB CDC 이벤트: `[EVENT] Angle jump: 12.3 -> 18.7 (delta=6.40) at 5230ms [3/10 slots]`

버튼: **BTN 1** Memory Report / **BTN 2** Event Dump + Pool 해제 / **BTN 3** Ring Buffer 초기화.

> 📸 `![PhAI 4채널 그래프 — Raw vs Filtered](../assets/img/19_filter_compare.png)` placeholder

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

### 왜 임베디드에서 `malloc` 금지인가

| 동적 (`malloc/free`) | 정적 (배열 + flag) |
|---------------------|-------------------|
| 메모리 파편화 → 시간 지나면 할당 실패 | 파편화 0 (고정 슬롯) |
| 할당 시간 비결정론적 (O(?)) | O(1) ~ O(N), N 작으면 무시 |
| Heap overflow 시 silent fail | Compile-time 크기 확정 |
| RTOS 사용 시 thread-safe heap 필요 | 정적 변수는 .bss 라 안전 |

**규칙**: Angel Robotics 양산 코드는 `malloc/free` 호출 금지.

### Ring Buffer 동작

```
[ slot 0 ][ slot 1 ][ slot 2 ] ... [ slot 49 ]
  ↑tail                              ↑head
Push: data[head++] = value     (head % SIZE)
Pop:  return data[tail++]
Full 시 Push → tail++ (가장 오래된 데이터 폐기)
```

### Pool Allocator 동작

```
pool[0]  pool[1]  pool[2] ... pool[9]
is_used[0]=0  [1]=1  [2]=0 ... [9]=0
Alloc: 선형 탐색하여 is_used[i]=0 인 슬롯 발견 → 1 로 마크
Free:  포인터 → 인덱스 변환 → is_used[i]=0
```

### 이동 평균 필터 효과

- 50 ms 윈도우 = 0~10 Hz 통과 (저주파)
- 보행 (1~2 Hz) 보존, 노이즈 (50 Hz 이상) 제거
- 트레이드오프: 윈도우 클수록 부드럽지만 **지연** ↑

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define RING_BUF_SIZE        50U                                    // 50 ms 윈도우
#define POOL_SIZE            10U
#define EVENT_THRESHOLD_DEG  5.0f

typedef struct {                                                    // ① Ring Buffer 자료구조
    float    data[RING_BUF_SIZE];
    uint16_t head, tail, count;
} RingBuffer_t;

typedef struct { uint32_t timestamp; float values[4]; } DataSlot_t;

typedef struct {                                                    // ② Pool Allocator 자료구조
    DataSlot_t pool[POOL_SIZE];
    bool       is_used[POOL_SIZE];
} PoolAlloc_t;

static RingBuffer_t s_ring_buf;     /* .bss — 0 으로 초기화 */
static PoolAlloc_t  s_pool;

static bool _RingBuf_Push(RingBuffer_t *p, float v)                 // ③ Push (Full 시 overwrite)
{
    bool overwrite = false;
    if (p->count >= RING_BUF_SIZE) {
        p->tail = (p->tail + 1) % RING_BUF_SIZE;   /* 가장 오래된 데이터 폐기 */
        p->count--;
        overwrite = true;
    }
    p->data[p->head] = v;
    p->head = (p->head + 1) % RING_BUF_SIZE;
    p->count++;
    return !overwrite;
}

static float _CalcMovingAverage(const RingBuffer_t *p)              // ④ 이동 평균 O(N)
{
    if (p->count == 0) return 0.0f;
    float sum = 0.0f;
    uint16_t idx = p->tail;
    for (uint16_t i = 0; i < p->count; i++) {
        sum += p->data[idx];
        idx = (idx + 1) % RING_BUF_SIZE;
    }
    return sum / (float)p->count;
}

static DataSlot_t *_Pool_Alloc(PoolAlloc_t *p)                      // ⑤ malloc 대체
{
    for (uint16_t i = 0; i < POOL_SIZE; i++) {
        if (!p->is_used[i]) {
            p->is_used[i] = true;
            memset(&p->pool[i], 0, sizeof(DataSlot_t));
            return &p->pool[i];
        }
    }
    return NULL;                                                     /* 풀 가득 참 */
}

static void Run_Loop(void)                                          // ⑥ 매 1 ms
{
    float raw = XM.status.h10.rightHipAngle;
    _RingBuf_Push(&s_ring_buf, raw);
    float filtered = _CalcMovingAverage(&s_ring_buf);

    /* 5° 이상 점프 → 이벤트 기록 */
    float delta = raw - s_prev_angle;
    if (delta < 0) delta = -delta;
    if (delta > EVENT_THRESHOLD_DEG) {
        DataSlot_t *slot = _Pool_Alloc(&s_pool);
        if (slot) { slot->timestamp = XM_GetTick(); slot->values[0]=...; }
        else       { XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, 200); }  /* 풀 포화 경고 */
    }
    s_prev_angle = raw;

    /* USB 스트리밍 4 ch */
    s_stream_data.raw_angle = raw;
    s_stream_data.filtered_angle = filtered;
    s_stream_data.pool_used_count = (float)_Pool_GetUsedCount(&s_pool);
    s_stream_data.ring_count = (float)_RingBuf_GetCount(&s_ring_buf);
    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF0);
}
```

전체 코드: [`memory_aware_design.c`](memory_aware_design.c)

> 🧒 ③ `(head + 1) % SIZE` 가 핵심 — 모듈로 연산으로 인덱스 wrap-around. SIZE 가 2 의 거듭제곱이면 `& (SIZE-1)` 로 최적화 가능 (50 은 아님).

---

## 4️⃣ 실험 — 직접 해보기 (체크포인트)

1. **빌드 + 플래시** → ✅ `[MEM] Memory-Aware Design example started.` USB 메시지
2. **PhAI Studio 4채널 그래프** (0xF0): Raw / Filtered / Pool / Ring → ✅ 모든 채널 실시간 갱신
3. **다리 천천히 움직임** → ✅ Filtered 가 Raw 를 부드럽게 따라옴 (지연 ~25 ms)
4. **다리 빠르게 흔들기 (>5°/ms)** → ✅ `[EVENT] Angle jump: ...` USB 메시지 + Pool used 증가
5. **이벤트 10개 누적 후 추가 시도** → ✅ LED 3 빠른 Blink (풀 가득 참)
6. **BTN 1 클릭** → ✅ `========== MEMORY REPORT ==========` 출력 (Ring 210 B / Pool ~210 B / 합계 / 1 MB 대비 비율)
7. **BTN 2 클릭** → ✅ 모든 이벤트 timestamp + delta + 토크 덤프 후 풀 해제, LED 3 OFF
8. **BTN 3 클릭** → ✅ Ring Buffer 비움 + `[RING] Buffer cleared: N samples popped.`
9. **변형 1 — 윈도우 크기**: `RING_BUF_SIZE` 50 → 10 (20 ms, 빠른 반응) vs 200 (200 ms, 매우 부드러움)
10. **변형 2 — 풀 크기 축소**: `POOL_SIZE` 10 → 3. 빠른 움직임으로 포화 trigger
11. **변형 3 — sizeof 패딩 관찰**: `DataSlot_t` 에 `uint8_t flag;` 한 줄 추가 → MemoryReport 의 sizeof 가 1 이 아닌 4 또는 8 증가 (alignment)
12. **변형 4 — 증분 합산 O(1)**: `_CalcMovingAverage` 를 `sum += new_val; sum -= old_val;` 로 변경 → 오차 누적 관찰
13. **변형 5 — DataSlot_t 필드 추가**: `values[4]` → `values[8]` 로 늘리고 Pool 메모리 변화 측정

---

## 5️⃣ 다음 단계

- 메모리 영역 (DTCM/PSRAM/Flash NV): [docs API07](../../docs/api-reference/07-memory-management.md)
- 빠른 데이터 처리 (USB MSC 로깅): [Ex.10c Advanced Log](../10c_MSC_Advanced_Log/)
- Tiny ML 가중치 (PSRAM 사용): [Ex.16 TinyAI](../16_TinyAI_Sensor_Fusion/)

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| `_Pool_Alloc` 가 항상 NULL 반환 | `_Pool_Init` 호출 누락 → `is_used[]` 가 garbage | `User_Setup` 에서 `_Pool_Init(&s_pool)` 호출 확인 |
| Filtered Angle 이 Raw 와 동일 | 첫 호출에서 count=1 → 평균 = raw | 정상. 50 cycle 지나면 안정 |
| 갑자기 Filtered 가 점프 | Ring Buffer Push 시 `count` 갱신 누락 | Push 후 `p->count++` 필수 |
| Pool 가득 찼는데 LED 3 안 켜짐 | `_Pool_Alloc` 반환값 NULL 체크 누락 | `if (!slot) XM_SetLedEffect(LED_3, BLINK, 200)` |
| Free 후에도 슬롯 사용 중으로 표시 | `_Pool_Free` 호출 시 잘못된 포인터 | 풀 범위 검사 (`>= pool[0] && <= pool[POOL_SIZE-1]`) 확인 |
| MemoryReport sizeof 가 예상보다 큼 | 컴파일러 alignment 패딩 (`uint8_t` + `float` → 3 B 패딩) | 정상. 큰 필드를 먼저 선언하면 줄어듦 |
| 이동 평균이 너무 느림 (지연 큼) | 윈도우 50 = 50 ms 지연 | `RING_BUF_SIZE` 10 으로 줄임 (트레이드오프) |
| `XM_SendUsbDataWithId` 가 데이터 안 보냄 | `XM_SetUsbCustomMeta(0xF0, ...)` 누락 | `User_Setup` 에서 메타 등록 필수 |
| `malloc` 쓰면 안 되나? | XM 양산 규칙 — heap fragmentation 위험 | 항상 정적 배열 + flag 패턴 사용 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
