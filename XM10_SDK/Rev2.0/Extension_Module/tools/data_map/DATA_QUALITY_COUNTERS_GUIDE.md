# Data Quality Counters — phai-studio Developer Guide
# 데이터 품질 카운터 — phai-studio 개발자 가이드

> **Data Map Version**: v2.5 (2026-04-01)
> **Breaking Change**: `timestamp_ms` (uint32, offset 0) → `xm_loop_count` (uint32, offset 0)

---

## 1. Three Independent Counters / 3개의 독립 카운터

The system has three counters running on **different clocks**, each revealing a different layer of the data pipeline.

시스템에는 **서로 다른 시계**에서 동작하는 3개의 카운터가 있으며, 각각 데이터 파이프라인의 다른 계층을 보여줍니다.

```
[CM MCU]                    [XM MCU]                     [PC]
h10AssistModeLoopCnt  →  xm_loop_count  →  SEQ_ID  →  PC receives
  (CM's clock)         (XM's clock)     (XM's clock)   (USB CDC)
       ①                    ②               ③
```

### ① `h10AssistModeLoopCnt` (offset 69, uint32)

| | EN | KR |
|---|---|---|
| **Clock** | CM (Control Module) MCU | CM (제어 모듈) MCU |
| **Increments** | Every 1ms **only in Assist mode** | Assist 모드에서만 1ms마다 증가 |
| **Standby** | Reset to 0 on mode transition | 모드 전환 시 0으로 초기화 |
| **Source** | CM firmware, sent via CAN-FD PDO | CM 펌웨어, CAN-FD PDO로 전송 |
| **Purpose** | Detect if CM data is fresh or stale | CM 데이터가 최신인지 stale인지 감지 |

### ② `xm_loop_count` (offset 0, uint32) — NEW in v2.5

| | EN | KR |
|---|---|---|
| **Clock** | XM (Extension Module) MCU | XM (확장 모듈) MCU |
| **Increments** | Every UserTask execution (~1ms), **always** | UserTask 실행마다 (~1ms), **항상** |
| **Standby** | Still incrementing (mode-independent) | 계속 증가 (모드 무관) |
| **Source** | XM firmware, incremented in `XM_TotalData_Snapshot()` | XM 펌웨어, 스냅샷 함수에서 증가 |
| **Purpose** | Ground truth for XM task execution count | XM 태스크 실행 횟수의 기준 값 |

### ③ `SEQ_ID` (PhAI packet header, uint16)

| | EN | KR |
|---|---|---|
| **Clock** | XM MCU (CDC transmit) | XM MCU (CDC 전송) |
| **Increments** | Every CDC packet sent (wraps at 65535→0) | CDC 패킷 전송마다 (65535→0 순환) |
| **Source** | PhAI packet builder, before COBS encoding | PhAI 패킷 빌더, COBS 인코딩 전 |
| **Purpose** | Detect USB CDC packet loss between XM↔PC | XM↔PC 간 USB CDC 패킷 유실 감지 |

---

## 2. Cross-Comparison Table / 교차 비교 표

Each pair of counters reveals a specific problem.
각 카운터 쌍의 비교는 특정 문제를 드러냅니다.

### 2.1 `xm_loop_count` diff analysis (Primary)

| diff | Meaning (EN) | 의미 (KR) | Severity |
|------|-------------|-----------|----------|
| **1** | Normal — XM executed on time | 정상 — XM이 정시 실행됨 | OK (green) |
| **0** | Should never happen (always increments) | 발생 불가 (항상 증가) | Bug |
| **≥2** | XM task was blocked (missed cycles) | XM 태스크가 블로킹됨 (사이클 누락) | Warning (yellow) |

### 2.2 `h10AssistModeLoopCnt` diff analysis (Assist mode only)

| diff | Meaning (EN) | 의미 (KR) | Severity |
|------|-------------|-----------|----------|
| **1** | Normal — fresh CM data every cycle | 정상 — 매 사이클 최신 CM 데이터 | OK (green) |
| **0** | **DUP** — same CM data read twice (stale) | **DUP** — 동일 CM 데이터를 두 번 읽음 | Warning (yellow) |
| **≥2** | **GAP** — CM data missed (1-miss, 2-miss, ...) | **GAP** — CM 데이터 누락 | Error (red) |
| N/A | **Standby** — counter reset to 0 (mode=0) | **대기 모드** — 카운터 0 초기화 (mode=0) | Info (gray) |
| **< 0** (large jump) | **Mode transition** — Assist→Standby (reset to 0) or Standby→Assist (restart from 0) | **모드 전환** — Assist→Standby (0 초기화) 또는 Standby→Assist (0부터 재시작) | Info (gray) |

> **Important**: 
> - In Standby mode (`h10Mode == 0`), this counter is reset to 0 and stays at 0.
> - On Assist→Standby transition, a large negative diff occurs (e.g., 30000 → 0). This is NOT a bug.
> - On Standby→Assist transition, counter restarts from 0. Diff from last Standby value (0) will be 1.
> - Studio must check `h10Mode` before classifying: diff=0 in Standby is expected, not DUP.
>
> **중요**:
> - 대기 모드(`h10Mode == 0`)에서는 카운터가 0으로 초기화되어 유지됩니다.
> - Assist→Standby 전환 시 큰 음수 diff 발생 (예: 30000 → 0). 이는 버그가 아닙니다.
> - Standby→Assist 전환 시 0부터 다시 시작합니다.
> - Studio는 분류 전에 반드시 `h10Mode`를 확인해야 합니다: 대기 모드에서 diff=0은 DUP이 아닙니다.

### 2.3 `SEQ_ID` diff analysis

| diff | Meaning (EN) | 의미 (KR) | Severity |
|------|-------------|-----------|----------|
| **1** | Normal — PC received every packet | 정상 — PC가 모든 패킷 수신 | OK (green) |
| **≥2** | **CDC loss** — packets dropped between XM and PC | **CDC 유실** — XM↔PC 사이 패킷 누락 | Error (red) |
| **0** | Duplicate packet (should not happen) | 중복 패킷 (발생하면 안 됨) | Bug |

---

## 3. Combined Diagnosis / 통합 진단

### Diagnosis Matrix / 진단 매트릭스

| xm_loop diff | h10_loop diff | SEQ_ID diff | Diagnosis (EN) | 진단 (KR) |
|---|---|---|---|---|
| 1 | 1 | 1 | **Perfect** — all systems nominal | **완벽** — 모든 시스템 정상 |
| 1 | 0 | 1 | **CM stale** — XM ran, but CM data didn't update | **CM stale** — XM은 실행됐으나 CM 데이터 미갱신 |
| 1 | ≥2 | 1 | **CM gap** — XM ran, but CM missed sending PDO | **CM gap** — XM은 실행됐으나 CM이 PDO 전송 누락 |
| ≥2 | ≥2 | 1 | **XM blocked** — UserTask delayed, CM data also missed | **XM 블로킹** — UserTask 지연, CM 데이터도 누락 |
| 1 | 1 | ≥2 | **CDC loss** — data was fresh but PC didn't receive | **CDC 유실** — 데이터는 최신이나 PC 수신 실패 |
| ≥2 | any | ≥2 | **XM blocked + CDC loss** — compound issue | **XM 블로킹 + CDC 유실** — 복합 문제 |

### Sync Rate Calculation / 동기화율 계산

```
sync_rate = count(h10_loop_diff == 1) / total_samples × 100%
```

| sync_rate | Quality (EN) | 품질 (KR) | Color |
|---|---|---|---|
| ≥ 99% | Excellent | 우수 | Green |
| 95~99% | Good | 양호 | Light green |
| 90~95% | Fair | 보통 | Yellow |
| < 90% | Poor | 불량 | Red |

> Only calculate when `h10Mode != 0` (Assist mode active).
> `h10Mode == 0` (대기 모드)에서는 계산하지 않음.

### Mode Transition Handling (CRITICAL) / 모드 전환 처리 (필수)

Raw diff values (negative jumps, large numbers) must **NEVER** be shown to the user.
Studio must filter mode transitions internally and only show clean results.

raw diff 값 (음수 점프, 큰 숫자)은 **절대 사용자에게 노출하면 안 됩니다.**
Studio는 모드 전환을 내부적으로 필터링하고 깨끗한 결과만 표시해야 합니다.

```typescript
// Reference implementation / 참조 구현
function classifyH10Diff(
    prev_h10_loop: number, curr_h10_loop: number, h10Mode: number
): 'normal' | 'dup' | 'gap' | 'skip' {

    // 1. Standby mode — not counting, skip entirely
    if (h10Mode === 0) return 'skip';

    const diff = curr_h10_loop - prev_h10_loop;

    // 2. Mode transition — large jump or negative diff
    //    (Assist→Standby: e.g. 30000→0, Standby→Assist: restart from 0)
    //    Do NOT count as DUP or GAP — just reset baseline
    if (diff < 0 || diff > 100) return 'skip';

    // 3. Normal classification
    if (diff === 1) return 'normal';
    if (diff === 0) return 'dup';
    return 'gap';  // diff >= 2
}

// Usage: only count 'normal', 'dup', 'gap' for statistics
// 'skip' samples are excluded from sync_rate, DUP count, GAP count
```

**User sees / 사용자에게 보이는 것:**
- Sync Rate: **97.2%** (mode transition samples excluded)
- DUP: **3** / GAP: **12** (clean numbers, no negatives)
- Mode badge: **ASSIST** or **STANDBY**

**User does NOT see / 사용자에게 보이지 않는 것:**
- Raw counter values (xm_loop_count, h10AssistModeLoopCnt)
- Negative diff values
- Mode transition transient samples

---

## 4. Recommended Studio UI / Studio UI 권장

### 4.1 Real-time Status Bar / 실시간 상태 바

```
┌─────────────────────────────────────────────────────────┐
│  Mode: ASSIST        │  Sync Rate: 97.2%     │  CDC: OK    │
│  DUP: 3  GAP: 12    │  XM Miss: 0           │  CDC Loss: 0│
│  ● CAN Ch1: OK      │  ● CAN Ch2: OK        │             │
└─────────────────────────────────────────────────────────┘
```

### 4.2 Data Quality Indicators / 데이터 품질 지표

| Indicator | Source | Display |
|---|---|---|
| **Mode** | `h10Mode` (offset 78) | Badge: ASSIST (green) / STANDBY (gray) |
| **Sync Rate %** | `h10AssistModeLoopCnt` diff (Assist only, skip transitions) | Large number + color badge |
| **DUP Count** | `h10_loop_diff == 0` count (Assist only, skip transitions) | Counter, yellow if > 0 |
| **GAP Count** | `h10_loop_diff >= 2` count (Assist only, skip transitions) | Counter, red if > 1% |
| **XM Miss** | `xm_loop_diff >= 2` count | Counter, yellow if > 0 |
| **CDC Loss** | `SEQ_ID_diff >= 2` count | Counter, red if > 0 |

### 4.3 Mode-Aware Display / 모드 인식 표시

```
if (h10Mode == 0) {
    // Standby — show "STANDBY" badge, disable sync rate calculation
    // h10AssistModeLoopCnt diff is meaningless
    // Only xm_loop_count and SEQ_ID are valid
} else {
    // Assist — full data quality analysis
    // All three counters are valid
}
```

---

## 5. Migration Note (v2.4 → v2.5) / 마이그레이션 안내

| Field | v2.4 | v2.5 |
|---|---|---|
| offset 0 (uint32) | `timestamp_ms` (SysTick uptime) | `xm_loop_count` (UserTask counter) |

**Why the change / 변경 이유:**
- `timestamp_ms` was SysTick-based — it shows *when* the snapshot was taken, but cannot detect if UserTask was blocked (SysTick keeps running even during blocking)
- `xm_loop_count` shows *how many times* UserTask actually executed — if the task was blocked for 2ms, the count still increments by 1, revealing the blocking when cross-compared with `h10AssistModeLoopCnt`

- `timestamp_ms`는 SysTick 기반이라 스냅샷 *시점*은 알 수 있지만, UserTask 블로킹 여부를 감지할 수 없습니다 (블로킹 중에도 SysTick은 계속 증가)
- `xm_loop_count`는 UserTask *실행 횟수*를 보여줍니다 — 태스크가 2ms 블로킹되어도 카운트는 1만 증가하므로, `h10AssistModeLoopCnt`와 교차 비교 시 블로킹을 드러냅니다

**Action required / 필요 작업:**
1. Update TypeScript data map import (auto-generated `xm_total_data_map.ts` v2.5)
2. Replace any `timestamp_ms` references with `xm_loop_count`
3. Remove any `Date`/`ms` formatting — it's now a counter, not a timestamp
