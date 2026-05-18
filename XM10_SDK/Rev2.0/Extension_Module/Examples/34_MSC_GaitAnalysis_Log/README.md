# Ex.34 — MSC GaitAnalysis Log (H10 보행 데이터 → USB → Python → MATLAB 파이프라인)

> 🎯 **학습 목표**:
> - H10 SUIT 보행 분석 데이터를 USB 메모리에 **고속 바이너리 로깅** (FAT32 32KB cluster).
> - Python Decoder 로 CSV 변환 + MATLAB 후처리 자동 워크플로우.
> - `XM_SetH10AssistExistingMode` 로 보조 ON/OFF 비교 데이터 수집.
>
> ⏱️ 권장 시간: 40분 | 🔧 난이도: ⭐⭐
> 🧰 사전 예제: [Ex.10c MSC Advanced Log](../10c_MSC_Advanced_Log/) + [Ex.17 FSM Gait Intent](../17_FSM_Gait_Intent/) | 📚 관련 docs: [USB Data Logging](../../docs/api-reference/06-usb-data-logging.md)

> ⚠️ **USB 메모리 포맷** — FAT32 + 32 KB cluster 필수 (exFAT 미지원). 4 GB 이상 USB 권장.

---

본 예제는 H10 SUIT 웨어러블 로봇의 동작분석 데이터를 XM10에서 USB 메모리에 저장하는 **데이터 수집 파이프라인**의 XM10 측 구현입니다. 저장된 바이너리를 Python 디코더로 변환하면, MATLAB에서 바로 보행 분석을 수행할 수 있습니다.

## 🎯 학습 목표 (Objective)

* H10 CM에서 PDO로 수신한 **29채널 동작분석 데이터**를 USB 메모리에 저장하는 방법을 학습합니다.
* `XM_SetH10AssistExistingMode(true)`로 **H10 Existing Mode**를 활성화하여, XM10은 제어 없이 모니터링만 수행합니다.
* Self-describing metadata를 통해 **Python 디코더가 자동으로 CSV 컬럼을 생성**하는 구조를 이해합니다.
* CSV 컬럼명이 **MATLAB 변수명과 1:1 매칭**되도록 설계하는 패턴을 학습합니다.
* `__attribute__((packed))` 구조체와 `_Static_assert`로 **바이너리 레이아웃을 보장**하는 방법을 이해합니다.

## 📋 데이터 파이프라인 (Data Pipeline)

```
H10 CM ──PDO──▶ XM10 ──USB-MSC──▶ .bin 파일
                                      │
                     Python xm10_gait_tool.py
                                      │
                                      ▼
                               decoded_output.csv
                                      │
                     MATLAB load_decoded_data()
                                      │
                          ┌───────────┴───────────┐
                          ▼                       ▼
                   GaitAnalysis_RT         GaitAnalysis_PP
                   (실시간 분석)           (후처리 분석)
```

## 📊 저장 데이터 (29 Channels, 108 Bytes)

### MATLAB RT 필수 (10ch)

| # | 필드명 | 타입 | 단위 | 설명 |
|---|--------|------|------|------|
| 1 | `count` | uint32 | - | CM assist loop counter (시간 기준) |
| 2 | `theta_trunk_improved` | float | deg | 골반/체간 각도 |
| 3 | `thigh_angle_lh` | float | deg | 좌측 대퇴 절대각 |
| 4 | `thigh_angle_rh` | float | deg | 우측 대퇴 절대각 |
| 5 | `LeftHipTorque` | float | Nm | 좌측 관절 토크 (FW에서 변환 완료) |
| 6 | `RightHipTorque` | float | Nm | 우측 관절 토크 (FW에서 변환 완료) |
| 7 | `velX` | float | m/s | 전방 보행 속도 |
| 8 | `LeftFootContact` | uint8 | 0/1 | 좌측 지면 접촉 |
| 9 | `RightFootContact` | uint8 | 0/1 | 우측 지면 접촉 |
| 10 | `post_processing_cnt` | uint16 | - | 동작분석 구간 카운터 (0~30000) |

### Extended (19ch)

| 구분 | 필드 |
|------|------|
| **관절 각도 (6ch)** | hip L/R, knee L/R, motor encoder L/R |
| **IMU 가속도 (6ch)** | acc X/Y/Z × 좌/우 고관절 (global frame) |
| **IMU 자이로 (6ch)** | gyr X/Y/Z × 좌/우 고관절 (global frame) |
| **FSM 상태 (1ch)** | CM 현재 FSM state |

## ⚙️ 동작 원리 (How it Works)

### H10 Existing Mode

```
XM_SetH10AssistExistingMode(true);
```

* XM10이 H10에 연결되면 **기본적으로 H10 보조 알고리즘이 비활성화**됩니다.
* 이 예제는 데이터 수집만 목적이므로, `true`로 설정하여 H10의 기존 보조 알고리즘을 그대로 유지합니다.
* **피험자가 보조 토크를 받으며 정상 보행하는 동안 XM10이 데이터만 수집합니다.**

### 토크 변환 (중요)

```c
s_log.LeftHipTorque = h10->leftHipTorque * TORQUE_SCALE;  // A → Nm
// TORQUE_SCALE = Kt(0.085) × GearRatio(18.75) = 1.59375
```

* XM API의 `leftHipTorque`/`rightHipTorque`는 **모터 전류(A)**입니다.
* 이 예제에서 FW 측에서 Nm 변환 후 저장하므로, **MATLAB에서 추가 변환이 불필요**합니다.

### 상태 전이

```
STANDBY ──(BTN1)──▶ LOGGING ──(BTN2)──▶ STANDBY
                       │                    ▲
                       └──(ERROR 감지)──────┘
```

## 📊 LED 피드백 표

| LED | 상태 | 의미 |
|-----|------|------|
| LED1 OFF | STANDBY | 대기 중 |
| LED1 BLINK | LOGGING | 로깅 진행 중 |
| LED2 BLINK | WARNING | 버퍼 90%+ (쓰기 지연) |
| LED2 SOLID | WARNING | 디스크 잔여 50MB 미만 |
| LED3 SOLID | ERROR | 로깅 강제 중단, STANDBY로 자동 복귀 |
| LED3 HEARTBEAT | - | USB 메모리 미연결 |

## 🚀 실행 방법 (How to Use)

### 데이터 수집

1. USB 메모리를 XM10에 연결하고, H10과 XM10을 CAN으로 연결합니다.
2. 코드를 업로드합니다.
3. H10이 Assist 모드로 진입하면 XM10이 PDO 데이터를 수신합니다.
4. **BTN1 클릭:** 로깅 시작 → LED1 깜빡임
5. 피험자가 보행을 수행합니다.
6. **BTN2 클릭:** 로깅 정지 → 모든 LED 꺼짐
7. USB 메모리에서 `/LOGS/GA_000/` 폴더를 확인합니다.

### Python 디코딩

```bash
# GUI 모드 (더블클릭으로 실행 가능)
python xm10_gait_tool.py

# CLI 모드
python xm10_gait_tool.py D:\LOGS --no-gui

# 단일 세션 Quick Plot
python xm10_gait_tool.py D:\LOGS\GA_000 --plot
```

### MATLAB 분석

```matlab
% 1. CSV 로드 → workspace 변수 자동 생성
load_decoded_data('path/to/GA_000/decoded_output.csv');

% 2. 실시간 분석 (count, thigh_angle, velX 등 바로 사용 가능)
run('GaitAnalysis_RT.m');

% 3. 후처리 분석 (보행주기 추출, 101pt 리샘플링, 통계)
run('GaitAnalysis_PP.m');
```

## 💡 직접 해보기 (Things to Try)

* `ROLLING_SIZE_MB`를 5로 줄여 파일 분할 동작을 확인해보세요.
* `XM_InsertUsbLogMarker()`로 보행 시작/종료 마커를 삽입하고, `events.csv`에서 확인해보세요.
* Extended 채널의 IMU 데이터로 보행 대칭성 분석을 시도해보세요.
* `XM_SetH10AssistExistingMode(false)`로 설정하면 H10 보조가 꺼집니다. **주의: 피험자가 보조 없이 걷게 됩니다.**
* Quick Plot에서 `Kinematics`, `Kinetics`, `IMU Acc` 그룹을 선택하여 좌/우 비교 그래프를 확인해보세요.

## 📁 관련 파일

| 파일 | 위치 | 역할 |
|------|------|------|
| `xm10_gait_tool.py` | `GaitAnalysis_Rulebase/Python_Decoder/` | 디코딩 + GUI + Quick Plot |
| `GaitAnalysis_RT.m` | `GaitAnalysis_Rulebase/MATLAB/` | 실시간 보행 분석 |
| `GaitAnalysis_PP.m` | `GaitAnalysis_Rulebase/MATLAB/` | 후처리 보행 분석 |
| `load_decoded_data.m` | `GaitAnalysis_Rulebase/MATLAB/data/` | CSV → MATLAB 변수 로드 |

---

## ⚠️ 흔한 실수

| 증상 | 원인 | 해결 |
|------|------|------|
| USB 인식 안 됨 | exFAT 또는 NTFS 포맷 | FAT32 + 32 KB cluster 로 재포맷 |
| `.bin` 파일 크기 0 | `XM_SetUsbLogSource` 미호출 | Setup 에서 등록 확인 |
| 로깅 중 데이터 누락 | USB 메모리 속도 (USB 2.0 < 30 MB/s) | 빠른 USB 3.0 호환 메모리 사용 |
| Python Decoder 가 .bin 못 읽음 | 포맷 버전 불일치 | `xm10_gait_tool.py` 최신 버전 |
| MATLAB load 시 timestamp 점프 | RTC 시간 미설정 | `XM_RTC_SetDateTime` 호출 |
| 좌·우 데이터 동기 안 맞음 | 별도 로깅 (의도된 경우) | Python Decoder 가 자동 정렬 |
| Assist OFF 모드인데 토크 출력됨 | `XM_SetH10AssistExistingMode(false)` 호출 누락 | 피험자 안전 위해 사전 확인 |
| 파일 크기가 너무 큼 | 매 cycle 풀 데이터 (1 kHz × 200 B = 200 KB/s) | 다운샘플 10 → 100 Hz |
