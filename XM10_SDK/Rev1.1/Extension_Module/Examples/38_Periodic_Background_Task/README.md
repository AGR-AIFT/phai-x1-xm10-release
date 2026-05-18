# Ex.38 — Periodic Background Task + Mutex+Snapshot

> Control_Loop (1 kHz) 와 보조 task (100 Hz) 가 공유 데이터를 안전하게 주고받는
> 기본 패턴. 멀티 task 프로그래밍의 출발점.

## 학습 포인트

1. `XM_Task_CreatePeriodic()` 로 보조 task 생성
2. 공유 변수 보호 — 두 가지 원칙
   - **단일 워드** (float / int / bool, ≤ 32-bit) → `volatile` 만으로 OK
   - **멀티 워드** (배열, 구조체)               → `XM_Mutex_*` 필수
3. 1 kHz Writer (Control_Loop) 는 항상 `timeout = 0` — 실패 시 skip
4. Reader (보조 task) 도 `timeout = 0` 권장 — 실패 시 이전값 유지

## 시나리오

```
Control_Loop (1 kHz)  ──[ADC 샘플]──▶  s_adc_buf[10]  ──[평균]──▶  s_adc_avg
   (Writer, 1 kHz)                    (Mutex 보호)              (volatile float)
                                          ▲                          ▲
                                          │ Lock                     │ read
                                          │                          │
                                       AdcSummary (100 Hz Reader)    사용자 알고리즘
```

- `s_adc_buf[10]` — 10개 ADC 샘플 ring buffer (멀티 워드 → Mutex 보호)
- `s_adc_avg`     — 직전 평균 (단일 float → volatile 만으로 안전)
- 사용자 알고리즘은 `s_adc_avg` 만 읽으면 됨 — lock 불필요

## 회로 / 보드 셋업

- Ex.04 `Ext_IO_Basic` 와 동일.
- Analog 입력 1개만 필요 — `XM_EXT_ADC_1` 핀에 가변 전압 (0~3.3 V).

## 실행 / 검증

1. `periodic_bg_task.c` 를 `XM_Apps/Control_Task/control_task.c` 자리에 복붙.
2. 빌드 → 플래시.
3. 입력 전압을 바꾸면 `s_adc_avg` 가 100 Hz 로 갱신
   (디버거 Live View / Variable Watch 에서 확인).

## 흔한 실수

> "Mutex_Lock 이 가끔 실패하는데 괜찮나요?"
> → 정상 동작입니다. Writer 가 잡고 있을 때 Reader 가 timeout=0 으로 실패한 것.
> Reader 는 이전 평균값을 유지 → garbage 보다 duplicate 가 안전.

> "Control_Loop 안에서 timeout > 0 으로 lock 하면 안 되나요?"
> → 1 kHz 주기가 깨집니다. 항상 timeout = 0, 실패 시 skip.

## 다음 예제

- **Ex.39 Task_Lifecycle** — OneShot task 생성 → 완료 후 Delete 패턴
