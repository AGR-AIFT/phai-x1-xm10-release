# Ex.40 — EMG Proportional Assist (외부 ADC EMG → 비례 보조 토크)

> 🎯 **학습 목표**:
> - 외부 아날로그 **EMG 4채널**을 GPIO ADC로 읽고, envelope(근활성도)를 추출.
> - envelope를 **비례 보조 토크**로 변환 → 좌/우 고관절에 인가.
> - **캘리브레이션(BTN1/BTN2)** · **control_ON 게이트** · **PhAI Studio 실시간 스트리밍**까지 한 흐름.
>
> ⏱️ 권장 시간: 60분 | 🔧 난이도: ⭐⭐⭐⭐ | 🟢 **Rev 2.0 전용** (외부 전원 5V 스위치 사용)
> 🧰 사전 예제: [Ex.05a DIO→ADC](../05a_Ext_IO_DIO_to_ADC/) · [Ex.12 Active Assist](../12_Active_Assist_Mode/)
> 📚 관련 docs: [외부 IO](../../docs/api-reference/04-external-io.md) · [H10 Control](../../docs/api-reference/02-h10-control-n-data.md) · ⚠️ [**착용 안전 1-페이지**](../../docs/safety/wearable-safety.md)

---

## 1️⃣ 목표 — 이 예제로 무엇이 동작하나

H10 슈트가 ASSIST 모드일 때, 사용자의 근전도(EMG) 신호 크기에 비례해 고관절 보조 토크를 만듭니다. 이 예제는 **EMG → 토크의 가장 단순한 베이스라인(선형 비례)** 이며, "어떤 의도를 어떻게 읽고 얼마나 도울지"는 여러분이 설계할 영역입니다(§6).

| 상태 (TSM) | 동작 |
|---|---|
| OFF | CM(H10) 연결 대기 |
| STANDBY | 슈트 ASSIST 모드 진입 대기 |
| ACTIVE | EMG 처리 + (control_ON일 때) 비례 토크 인가 |

신호 흐름(채널별 독립):
```
raw ADC 전압 → bias 제거 → 1차 LPF(80Hz) → 전파 정류 → envelope LPF(5Hz)
            → deadband → 정규화 → 비례 게인 → 포화(clamp)
```

> 🟢 **Rev 2.0 전용**: 이 예제는 EMG 센서 구동용 5V를 확장포트로 내보내기 위해 `XM_SetExtPowerVoltage(XM_EXT_PWR_5V)`를 사용합니다(Rev 1.1에는 없는 API). 신호 입력 자체는 Rev 무관하게 항상 0~3.3 V입니다.

---

## 2️⃣ 사전 지식 — 시작 전 알아둘 것

- **EMG 센서 매핑** — `PF3=ADC5, PF4=ADC6, PF5=ADC7, PF6=ADC8`. `torque_input_pair`로 사용할 쌍 선택(0=PF3/PF4→R/L, 1=PF5/PF6→R/L).
- **외부 전원 5V** — EMG 센서 구동용. `XM_SetExtPowerVoltage(XM_EXT_PWR_5V)`로 확장포트 공급 전압을 5V로. **단, MCU로 들어오는 신호는 0~3.3 V를 넘기면 안 됩니다.**
- **DIO→ADC 전환** — `XM_SwitchDioToAdc(XM_EXT_DIO_1..4)`로 DIO 핀을 ADC 입력으로. ([Ex.05a](../05a_Ext_IO_DIO_to_ADC/))
- **Envelope** — raw EMG는 ±진동 신호. 정류+저역통과로 "근활성도 크기"를 부드럽게 뽑은 것이 envelope.
- **부호 규약** — `XM_SetAssistTorqueRH/LH(Nm)`: **양수(+) = Flexion(굴곡) 보조, 음수(−) = Extension(신전) 보조.** ([api-ref](../../docs/api-reference/02-h10-control-n-data.md))
- **`control_ON` 게이트** — `0`이면 토크 0(관찰만), `1`이면 비례 토크 인가. **벤치 검증 전에는 0으로 유지.**
- **토크 한계** — 기본 최대 출력 **1.25 Nm**(`EMG_MAX_TORQUE_NM 2.5 × emg_assist_scale 0.5`). 그 위로 XM10 내부 **±10 Nm 하드 클램프**(변경 불가). 자세한 한계·비상정지는 [착용 안전 1-페이지](../../docs/safety/wearable-safety.md).

---

## 3️⃣ 핵심 코드 — 무엇이 어디서 일어나나

```c
#define EMG_RAW_LPF_CUTOFF_HZ   80.0f   // 1차 LPF (EMG 대역 보존)
#define EMG_ENV_LPF_CUTOFF_HZ    5.0f   // envelope 평활도 (낮을수록 부드럽고 느림)
#define EMG_ENVELOPE_DEADBAND_V  0.020f // 이완 시 잔여 노이즈 무시
#define EMG_MAX_TORQUE_NM        2.5f   // 비례 매핑 상한
float    emg_assist_scale = 0.5f;       // 출력 스케일(0~1) → 기본 최대 1.25 Nm

/* ① envelope → 토크 (가장 단순한 선형 비례 + 포화) */
static float _EnvelopeToTorque(int ch, float envelope_v) {
    float active_v = envelope_v - EMG_ENVELOPE_DEADBAND_V;
    if (active_v <= 0.0f) return 0.0f;                       // 이완 구간 = 0
    float span = s_full_scale_v[ch] - EMG_ENVELOPE_DEADBAND_V;
    float normalized = active_v / span;                     // 0~1
    float scaled_max = EMG_MAX_TORQUE_NM * emg_assist_scale;
    return _ClampFloat(normalized * scaled_max, 0.0f, scaled_max);  // 포화
}

/* ② ACTIVE 루프: 샘플 → 캘리브 → 처리 → (control_ON일 때만) 토크 */
static void Active_Loop(void) {
    _SampleAdcChannels();
    _UpdateCalibration();          // BTN1=이완 bias, BTN2=수축 full-scale, BTN3=리셋
    _ProcessEmgSignals();          // bias→LPF→정류→envelope→토크
    float rh = 0, lh = 0;
    _SelectTorquePair(&rh, &lh);
    if (control_ON == 1U) { XM_SetAssistTorqueRH(rh); XM_SetAssistTorqueLH(lh); }
    else                  { XM_SetAssistTorqueRH(0);  XM_SetAssistTorqueLH(0);  }
    _UpdateStreamData(rh, lh);     // ③ PhAI Studio 0xF0 6채널 스트림
}
```

전체 코드: [`emg_proportional_assist.c`](emg_proportional_assist.c)

> 🧒 핵심: ①의 envelope→토크 매핑이 "지능"의 자리입니다. 지금은 선형 비례 한 줄 — 여기를 여러분이 바꿉니다(§6).

---

## 4️⃣ 실험 — 벤치 우선 절차 (필독)

> ⚠️ **착용 전 반드시 거치대(벤치)에서 검증.** 보조력은 사람 다리에 직접 들어갑니다.

1. **HW**: EMG 센서를 확장포트에 연결(전원 5V, 신호 PF3~6). KIT H10 ↔ XM10, 거치 상태.
2. **빌드 + 플래시 + CM 연결 + 슈트 ASSIST** → ACTIVE 진입(LED1 점멸).
3. **`torque_input_pair` 선택** (Live Expressions). 기본 0(PF3/PF4).
4. **이완 캘리브** — 근육 이완 → **BTN1** → 3초 정지(LED1 빠른 점멸). bias 측정.
5. **수축 캘리브** — 대표 수축 → **BTN2** → 3초 유지(LED2 빠른 점멸). full-scale 측정. *(BTN3 = 기본값 리셋)*
6. **관찰** — `emg_pf*_raw_v / _envelope_v / _torque_nm`을 Live Expressions 또는 **PhAI Studio**(USB Connect → 0xF0 채널: Raw/Env/Tau RH·LH)로 확인. 이완 시 envelope≈0, 토크≈0인지 확인.
7. **방향·크기 검증** — `control_ON`은 아직 0. 토크 값(관찰만)이 의도한 방향/크기인지 거치 상태에서 확인.
8. **활성화** — 모든 확인 후에만 `control_ON = 1`. 작은 `emg_assist_scale`(예: 0.2)부터 천천히.

> 🔧 변형: `EMG_ENV_LPF_CUTOFF_HZ`(envelope 반응성), `EMG_ENVELOPE_DEADBAND_V`(노이즈 제거), `emg_assist_scale`(출력 크기)을 Live Expressions/매크로로 조정하며 응답 비교.

---

## 5️⃣ 🏆 설계 공간 — 여기가 여러분(대회)의 영역

이 예제는 **배관(ADC·필터·캘리브·안전·스트리밍)과 가장 단순한 베이스라인**만 제공합니다. 진짜 알고리즘은 직접 설계하세요:

- **A. envelope 응답 튜닝** — LPF 컷오프로 부드러움 vs 지연 trade-off 탐색.
- **B. 노이즈 거절** — 이완 envelope 측정 후 deadband 조정.
- **C. 매핑 변경** — `_EnvelopeToTorque()`를 선형 대신 임계·제곱·구간별 등으로. (최종 포화는 유지)
- **D. 토크 프로파일** — `_SelectTorquePair()` 뒤에 ramp/slew-rate 제한 추가로 급변 방지.
- **E. 상위 조건 결합** — EMG 활성도 + FSR 접지/보행 위상 검출을 결합해 **의도한 보행 구간에서만** 보조. ← 대회 핵심 난이도.

> 📝 "어떤 근육에서 어떤 의도를 읽고, 언제, 얼마의 보조를 줄지"가 평가 대상입니다. 이 파일은 출발선일 뿐입니다.

---

## 6️⃣ 다음 단계

- 보행 의도 FSM: [Ex.17 FSM Gait Intent](../17_FSM_Gait_Intent/)
- AI 센서 융합: [Ex.16 TinyAI](../16_TinyAI_Sensor_Fusion/)
- 데이터 수집·학습 파이프라인: [AI 데이터 파이프라인](../../docs/advanced/ai-data-pipeline.md)
- 토크 직접 제어 베이스: [Ex.12 Active Assist](../12_Active_Assist_Mode/) · [Ex.14 PD Realtime](../14_PD_Realtime_Control/)

---

## ⚠️ 안전 — 제거하면 안 되는 코드

알고리즘을 바꿔도 **다음은 그대로 둡니다** (소스 상단 주석 §9 *"Areas Students Should Not Modify"* 기준):

1. **`_EnvelopeToTorque()` 최종 포화(clamp)** — 출력 상한 보장.
2. **캘리브레이션 중 zero-torque** — 캘리브 동안 토크 0.
3. **모드 종료 시 zero-torque** — `Active_Exit`에서 토크 0 + `XM_SetControlMode(XM_CTRL_MONITOR)` 복귀.
4. **`XM_SetControlMode` 게이트** — `XM_CTRL_TORQUE` 진입 / `MONITOR` 복귀.

추가로 [착용 안전 1-페이지](../../docs/safety/wearable-safety.md)의 비상정지 경로(슈트 STANDBY)·정지 담당 대기를 반드시 숙지하세요.

---

## ❓ 흔한 실수

| 증상 | 원인 | 해결 |
|---|---|---|
| 토크가 항상 0 | `control_ON = 0` (기본값) | 벤치 검증 후 `control_ON = 1` |
| 토크가 항상 0 (control_ON=1인데도) | `XM_SetControlMode(XM_CTRL_TORQUE)` 미진입 / 슈트 ASSIST 아님 | ACTIVE 상태·슈트 ASSIST 확인 |
| 버튼이 안 먹음 | `XM_IO_Update()` 미호출 | `Control_Loop` 끝에서 매번 호출(이 예제엔 포함됨) |
| 이완해도 토크가 남음 | deadband 부족 / bias 캘리브 안 함 | BTN1 이완 캘리브 + `EMG_ENVELOPE_DEADBAND_V` 상향 |
| 좌우 반대로 보조 | `torque_input_pair`/센서 좌우 뒤바뀜 | 거치 상태에서 한쪽씩 확인 후 매핑 정정 |
| 신호가 깨짐/포화 | EMG 출력이 3.3 V 초과 | 센서 출력 범위 확인(0~3.3 V) |
| PhAI Studio에 데이터 없음 | Connect 안 함 | 포트 선택 → **Connect**, 0xF0 채널 선택 |

막혔다면 → [docs/troubleshooting.md](../../docs/troubleshooting.md)
