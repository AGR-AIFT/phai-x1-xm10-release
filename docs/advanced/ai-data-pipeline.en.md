# AI Training Data Pipeline

This page maps the end-to-end flow from collecting exoskeleton data on the XM10 board to loading it into a training framework such as PyTorch or scikit-learn. Use it as a quick-reference map while you work.

> USB memory (MSC) file logging was removed in v2.5.0. Data capture now uses **USB-CDC real-time streaming**, and Path A below is the standard route. On-board storage (SD card) is planned for a future HW revision.

---

## Big Picture

```
[Board operation]         [Collection]                  [Conversion]          [Training]
       ↓                       ↓                             ↓                    ↓
KIT H10 worn  →  USB-CDC real-time streaming   →   CSV / npy output  →  PyTorch DataLoader
                 (PhAI Studio or                                          or sklearn
                  xm10 tool → .xmlog → export)
```

There are two paths. Choose based on your goal.

| Path | Best for | Data volume | Latency |
|------|----------|-------------|---------|
| A. USB-CDC streaming capture | Offline training (most cases) | Minutes to hours | < 100 ms |
| B. On-device inference | Deploying a trained model back to the board | — | 1 ms |

---

## Path A — USB-CDC Streaming Capture (Standard)

### 1. Board Side — Streaming Custom Channels

Use the `examples/09_CDC_Stream/` pattern. Stream the data you want to train on over a custom channel in real time.

```c
void Control_Setup(void) {
    // Channel metadata (JSON): args are module_id, json_str
    XM_SetUsbCustomMeta(0xF0, "[{\"name\":\"hip_L\",\"unit\":\"deg\"},{\"name\":\"hip_R\",\"unit\":\"deg\"}]");
}

void Control_Loop(void) {
    float sample[2] = { hip_L, hip_R };
    XM_SendUsbDataWithId(sample, sizeof(sample), 0xF0);  // arg order: (data, len, id) — every loop
}
```

### 2. PC Side — Receive Stream + Save CSV

Two options:

- **PhAI Studio** — connect via USB → select channel `0xF0` → watch the live graph and click the record button → export to `.csv`. Simplest.
- **xm10 tool** (in the repo's `PythonDecoder/`) — saves the raw bytes as-is to `.xmlog` and lets you pull CSV out later. Since you can re-export old recordings even after changing channel names/layout, it's better suited for managing a training dataset. Build it as an executable and it works on PCs without Python too ([guide](../getting-started/04-pc-data-tool.en.md)).

```bash
python PythonDecoder/xm10.py recv --cli --port COM6 --log         # receive to console + save .xmlog (Ctrl+C to stop)
python PythonDecoder/xm10.py export data/cdc_<timestamp>.xmlog --csv out/   # export per-channel CSV
```

`out/` gets `..._user_0xF0.csv` (your channel) and `..._total_0x20.csv` (the 197 channels the board always sends — joint angle/torque, IMU, GRF). Both files carry `pc_time_us` (PC receive time) and `seq_id` (the board's send sequence, shared across all channels) columns up front so you can align the time axis.

### 3. Loading into a Training Framework

#### PyTorch Example

```python
import pandas as pd
import numpy as np
import torch
from torch.utils.data import Dataset, DataLoader

class XM10Dataset(Dataset):
    def __init__(self, csv_path):
        df = pd.read_csv(csv_path)
        self.data = df.select_dtypes("number").values.astype("float32")  # shape (N, features)

    def __len__(self):
        return len(self.data) - 100  # 100-step window

    def __getitem__(self, idx):
        x = self.data[idx : idx + 100]
        y = self.data[idx + 100, 0]  # predict the first channel of the next step
        return torch.tensor(x), torch.tensor(y, dtype=torch.float32)

ds = XM10Dataset("cdc_phai_20260224_120000.csv")
loader = DataLoader(ds, batch_size=64, shuffle=True)
```

> For large datasets, convert the CSV once with `np.save()` to `.npy` for much faster loading.

#### scikit-learn Example (Simple Classification)

```python
import pandas as pd
from sklearn.ensemble import RandomForestClassifier

df = pd.read_csv("cdc_phai_20260224_120000.csv")
X = df[["imu_ax", "imu_ay", "imu_az", "knee_angle"]].values
y = df["gait_phase"].values

clf = RandomForestClassifier(n_estimators=100)
clf.fit(X, y)
```

---

## Path B — On-Device Inference (Deploying a Trained Model)

Take a model trained via Path A and deploy it back to the board to run inference inside the 1 ms control loop.

### 1. Model Compression

Convert a PyTorch or TensorFlow model into a format the STM32 can handle.

| Tool | Output | Suitable models |
|------|--------|-----------------|
| [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers) | `.tflite` | Small CNN, MLP |
| [STM32Cube.AI](https://www.st.com/en/embedded-software/x-cube-ai.html) | Auto-generated C code | TF / PyTorch / ONNX |
| Manual implementation | Hand-written C forward pass | Very small NN (≤ 3 layers) |

### 2. Running Inference on the Board

`Ex.16 TinyAI Sensor Fusion` demonstrates manual implementation (3-layer NN). `Ex.36 OnDevice Kinesthetic Learning` goes further and covers on-device learning as well.

Core pattern:

```c
void Control_Loop(void) {
    float input[3] = { imu_ax, imu_ay, imu_az };
    float output[5];

    my_nn_forward(input, output);  // user-defined inference function
    int gait_phase = argmax(output, 5);

    if (gait_phase == STANCE_PHASE) {
        XM_SetAssistTorqueRH(assist_table[gait_phase]);
    }
}
```

Measure inference time using the loop profiling pattern from `Ex.18 Debug Monitor` to confirm it completes within 1 ms.

---

## Common Pitfalls

- **CSV is too large / slow** — For large datasets, convert to `.npy` or `.parquet` for loading. Keep CSV for human inspection only.
- **Packet loss while streaming** — Run `python PythonDecoder/xm10.py soak --port COM6 --minutes 10` to check for dropped frames. If you captured via `.xmlog`, the missing ranges (GAP) show up right in the `export` summary. If there's loss, reduce the data volume or trim channels.
- **NaN / Inf values** — Add `assert(isfinite(value))` on the board side. Filter with `np.isfinite()` just before training.
- **Class imbalance** — Gait phases like Stance/Swing are naturally imbalanced (roughly 7:3). Use the `class_weight` option or SMOTE.
- **On-board inference exceeds 1 ms** — Apply model quantization (int8) or reduce the number of layers. Verify that the STM32H7's FPU is being utilized.

---

## Next Steps

- Practice the custom-channel streaming pattern with `Ex.09 CDC Stream`
- Progress from `Ex.16 TinyAI Sensor Fusion` to `Ex.36 OnDevice Kinesthetic Learning` for on-device inference
- After deploying a trained model to the board, try the full expert demonstration → model training → playback cycle as shown in `Ex.33 Kinesthetic Teaching`

If you need more detail on PhAI Studio or Python post-processing, open a request on GitHub Issues.
