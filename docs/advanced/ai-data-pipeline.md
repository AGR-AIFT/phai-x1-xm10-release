# AI 학습 데이터 파이프라인

XM10 보드에서 모은 외골격 데이터를 PyTorch / scikit-learn 같은 학습 프레임워크로 가져가는 흐름을 한 페이지에 정리했어요. AI 모델 학습이 목표라면 이 페이지를 흐름표로 두고 작업하세요.

---

## 큰 그림

```
[보드 동작]                [수집]                  [변환]                    [학습]
  ↓                          ↓                      ↓                        ↓
KIT H10 착용  →  USB 메모리 기록     →  Python 디코더    →  PyTorch DataLoader
              또는 PhAI Studio 실시간     CSV / npy 출력         또는 sklearn
                  스트리밍
```

세 가지 길이 있어요. 목적에 따라 골라가세요.

| 길 | 적합한 상황 | 데이터량 | 지연 |
|----|-----------|---------|------|
| A. USB 메모리 로깅 | 오프라인 학습 (대부분) | 시간 단위 | — |
| B. PhAI Studio 스트리밍 | 실시간 시각화 + 짧은 세션 | 분 단위 | < 100 ms |
| C. 보드 내 추론 | 학습된 모델을 보드로 배포 | — | 1 ms |

---

## 길 A — USB 메모리 로깅 (가장 일반적)

### 1. 보드 측 — 데이터 기록

`examples/10c_MSC_Advanced_Log/` 또는 `examples/34_MSC_GaitAnalysis_Log/` 패턴을 사용해요.

핵심 호출 3 가지:

```c
// 1. 로깅 소스 설정 (Total Data 자동 / 커스텀 구조체 수동)
XM_SetUsbLogSource(USB_LOG_SOURCE_TOTAL_DATA);

// 2. 매 루프에서 데이터 기록
XM_WriteUsbLogData(&my_data, sizeof(my_data));

// 3. 파일 롤링 (장시간 세션용, 10 분마다 새 파일)
XM_GetUsbLogStatus(&status);
if (status.elapsed_sec > 600) XM_NewUsbLogFile();
```

자세한 API: [docs/api-reference/06-usb-data-logging.md](../api-reference/06-usb-data-logging.md).

### 2. 파일 회수

USB 메모리를 PC 에 꽂으면 `.bin` 또는 `.csv` 파일이 보여요. 권장은 `.bin` (디코더가 처리).

### 3. PythonDecoder 로 변환

`PythonDecoder/` 폴더의 USB MSC 디코더가 `.bin` → `.csv` 또는 `.npy` 로 변환해줘요.

```bash
cd PythonDecoder
python usb_msc_decoder.py --input my_log.bin --output my_log.csv
```

산출물:
- `.csv` — 사람이 보기 좋음, pandas 로 바로 읽기
- `.npy` — PyTorch / numpy 빠른 로드
- `.mat` — MATLAB 사용자용

### 4. 학습 프레임워크에서 로드

#### PyTorch 예시

```python
import torch
from torch.utils.data import Dataset, DataLoader
import numpy as np

class XM10Dataset(Dataset):
    def __init__(self, npy_path):
        self.data = np.load(npy_path)  # shape (N, features)

    def __len__(self):
        return len(self.data) - 100  # 100-step window

    def __getitem__(self, idx):
        x = self.data[idx : idx + 100]
        y = self.data[idx + 100, 0]  # 다음 스텝의 첫 채널 예측
        return torch.tensor(x, dtype=torch.float32), torch.tensor(y, dtype=torch.float32)

ds = XM10Dataset("my_log.npy")
loader = DataLoader(ds, batch_size=64, shuffle=True)
```

#### scikit-learn 예시 (간단 분류)

```python
import pandas as pd
from sklearn.ensemble import RandomForestClassifier

df = pd.read_csv("my_log.csv")
X = df[["imu_ax", "imu_ay", "imu_az", "knee_angle"]].values
y = df["gait_phase"].values

clf = RandomForestClassifier(n_estimators=100)
clf.fit(X, y)
```

---

## 길 B — PhAI Studio 실시간 스트리밍

짧은 세션 + 시각 검증이 목적이면 이쪽이 빨라요.

### 1. 보드 측 — 커스텀 채널 등록

```c
void User_Setup(void) {
    XM_SetUsbCustomMeta(0xF0, "my_signal", "v");  // 채널 0xF0, 단위 V
}

void User_Loop(void) {
    float my_value = read_my_sensor();
    XM_SendUsbDataWithId(0xF0, &my_value, sizeof(my_value));
}
```

### 2. PhAI Studio 측 — 실시간 그래프 + 녹화

PhAI Studio 에서 USB 연결 → 채널 `0xF0` 선택 → 그래프 보면서 동시에 녹화 버튼.

녹화된 데이터는 PhAI Studio 의 export 기능으로 `.csv` 출력 가능. 이후 길 A 의 PyTorch / sklearn 단계로 동일하게 진행.

자세한 PhAI Studio 사용법은 별도 자료.

---

## 길 C — 보드 내 추론 (학습된 모델 배포)

길 A 로 학습한 모델을 보드에 다시 올려서 1 ms 루프 안에서 추론.

### 1. 모델 경량화

PyTorch / TensorFlow 모델을 STM32 가 다룰 수 있도록 변환.

| 도구 | 변환 결과 | 적합한 모델 |
|------|----------|-----------|
| [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers) | `.tflite` | 작은 CNN, MLP |
| [STM32Cube.AI](https://www.st.com/en/embedded-software/x-cube-ai.html) | C 코드 자동 생성 | TF/PyTorch/ONNX 모두 |
| 직접 구현 | 사람이 C 로 forward 작성 | 매우 작은 NN (3-layer 이하) |

### 2. 보드에서 추론

`Ex.16 TinyAI Sensor Fusion` 이 직접 구현 (3-layer NN) 예시. `Ex.36 OnDevice Kinesthetic Learning` 은 온디바이스 학습까지 다룹니다.

핵심 패턴:

```c
void User_Loop(void) {
    float input[3] = { imu_ax, imu_ay, imu_az };
    float output[5];

    my_nn_forward(input, output);  // 사용자 정의 추론 함수
    int gait_phase = argmax(output, 5);

    if (gait_phase == STANCE_PHASE) {
        XM_SetAssistTorqueRH(assist_table[gait_phase]);
    }
}
```

추론 시간 측정은 `Ex.18 Debug Monitor` 의 루프 프로파일링 패턴 사용 → 1 ms 안에 끝나는지 확인.

---

## 자주 막히는 부분

- **로그 파일이 너무 큼** — 1 ms × 1 시간 = 360 만 샘플. 다 필요 없으면 `XM_WriteUsbLogData` 호출 빈도를 낮추거나 (예: 10 ms 마다 = 100 Hz), 관심 필드만 골라 작은 구조체로 저장.
- **CSV 가 너무 느림** — 큰 데이터셋은 `.npy` 또는 `.parquet` 로. CSV 는 사람 확인용으로만.
- **타임스탬프가 안 맞아요** — `Ex.10b` 패턴의 수동 타임스탬프 사용. 자동 모드는 파일 단위만 기록.
- **NaN / Inf 값** — 보드 측에서 `assert(isfinite(value))` 추가. 학습 직전에 `np.isfinite()` 로 필터링.
- **클래스 불균형** — Stance/Swing 같은 보행 phase 는 7:3 정도로 비균등. `class_weight` 옵션 또는 SMOTE 사용.
- **보드에서 추론이 1 ms 를 넘김** — 모델 양자화 (int8) 또는 layer 수 감소. STM32H7 의 FPU 활용 확인.

---

## 다음

- `Ex.10c MSC Advanced Log` 로 로깅 패턴 익히기
- `Ex.34 MSC GaitAnalysis Log` 로 보행 분석 데이터 직접 수집
- `Ex.16 TinyAI Sensor Fusion` → `Ex.36 OnDevice Kinesthetic Learning` 으로 보드 내 추론 학습
- 학습 모델을 보드에 올린 뒤에는 `Ex.33 Kinesthetic Teaching` 처럼 전문가 시연 → 모델 학습 → 재생 의 전체 사이클 시도

PhAI Studio + Python 후처리에 대한 더 자세한 내용이 필요하면 GitHub Issues 에 요청해주세요.
