# Bilateral 4-FSR Gait Event Monitor Plan

## Goal

Create a Python CDC monitor that receives four FSR channels from XM10 firmware,
normalizes them through a simple calibration flow, detects bilateral gait events
from the combined four-sensor contact pattern, and visualizes the result in
real time.

Initial implementation file:

```text
cdc_bilateral_gait_event_monitor.py
```

Expected CDC payload:

```text
float[4] = LHeel, LToe, RHeel, RToe
```

The CDC packet reader should follow the existing PhAI V2.2 COBS path used by:

```text
cdc_4ch_live_monitor.py
```

The older one-foot fuzzy monitor is useful as a reference for GUI layout, FSR
filtering, threshold controls, and real-time plotting, but its one-foot
four-region fuzzy phase classifier should not be copied directly.

## Contact Bits

Each sample is converted into four binary contact bits:

```text
LH = Left heel contact
LT = Left toe contact
RH = Right heel contact
RT = Right toe contact
```

The four bits form a 16-state bilateral contact pattern:

```text
LH LT RH RT
```

These 16 patterns are treated as instantaneous states. Gait events are generated
from transitions between states.

## 16-State Mapping

| Bits | State label | Interpretation |
| --- | --- | --- |
| 0000 | NO_CONTACT | No FSR contact. Usually airborne, unloaded, or invalid for normal walking. |
| 0001 | R_TOE_ONLY | Right forefoot/push-off partial contact. |
| 0010 | R_HEEL_ONLY | Right initial contact/loading partial contact. |
| 0011 | R_FOOT_FLAT | Right single support. |
| 0100 | L_TOE_ONLY | Left forefoot/push-off partial contact. |
| 0101 | TOE_TOE | Toe-toe contact; unusual or transition. |
| 0110 | R_LOAD_L_PUSH | Right heel loading while left toe remains loaded. |
| 0111 | R_STANCE_L_PUSH | Double support, left push-off dominant. |
| 1000 | L_HEEL_ONLY | Left initial contact/loading partial contact. |
| 1001 | L_LOAD_R_PUSH | Left heel loading while right toe remains loaded. |
| 1010 | HEEL_HEEL | Heel-heel contact; unusual, broad stance, or transition. |
| 1011 | L_LOAD_R_STANCE | Double support, left loading dominant. |
| 1100 | L_FOOT_FLAT | Left single support. |
| 1101 | L_STANCE_R_PUSH | Double support, right push-off dominant. |
| 1110 | R_LOAD_L_STANCE | Double support, right loading dominant. |
| 1111 | DOUBLE_FULL | Full double contact. Becomes QUIET_STANDING if stable long enough. |

## Event Set

Keep the first version intentionally simple for student practice. The monitor
should expose only 6 high-level gait event labels. Lower-level sensor edges can
be computed internally for debugging, but they should not be the main event API.

### Internal Sensor Edges

The implementation may still detect direct contact-bit edges such as heel
contact on/off or toe contact on/off. These are useful for debugging and future
extensions, but they should be treated as internal signals.

### Public Gait Events

Initial version should support these 6 event labels:

```text
L_STEP_START     = left foot starts a step/contact transition
R_STEP_START     = right foot starts a step/contact transition
L_SUPPORT_START  = left-only support begins
R_SUPPORT_START  = right-only support begins
DOUBLE_SUPPORT   = both feet are in contact after a transition
STANDING         = stable full-contact standing is detected
```

Recommended first-pass mapping:

```text
L_STEP_START
  LH turns on while the right foot is already in contact.

R_STEP_START
  RH turns on while the left foot is already in contact.

L_SUPPORT_START
  right foot contact becomes false while left foot contact remains true.

R_SUPPORT_START
  left foot contact becomes false while right foot contact remains true.

DOUBLE_SUPPORT
  state transitions from single support to both-feet contact.

STANDING
  1111 remains stable longer than the standing dwell time.
```

No separate `STANDING_END`, `NO_CONTACT_START`, `HEEL_OFF`, or `TOE_OFF` event
is required in the first version. Those can be added later by students using the
same 16-state transition table.

## Quiet Standing

`1111` should not always be treated as normal double support. If all four
contacts remain on for a configurable dwell time, classify it as quiet standing.

Suggested default:

```text
standing_dwell_s = 0.7
```

Optional stability criterion:

```text
normalized load derivative remains below a small threshold
```

First implementation can start with dwell time only.

## Calibration And Normalization

Raw FSR magnitude can vary across sensors, shoes, users, and wiring. The monitor
should therefore provide a two-step calibration flow.

### Step 1: Zero / Off Calibration

User condition:

```text
Shoes worn, feet not touching the ground.
```

Action:

```text
Collect N samples and store per-channel off baseline.
```

Default:

```text
N = 500 samples, or about 1 second at 500 Hz
```

Stored values:

```text
off[ch] = median(raw[ch])
noise[ch] = robust noise estimate, for example MAD or percentile range
```

### Step 2: On Calibration

User condition:

```text
Shoes worn, standing with both feet on the ground.
```

Action:

```text
Collect N samples and store per-channel loaded level.
```

Default:

```text
N = 1000 samples, or about 2 seconds at 500 Hz
```

Stored values:

```text
on[ch] = median(raw[ch])
span[ch] = max(on[ch] - off[ch], minimum_span)
```

### Normalized Signal

Each sample is normalized per channel:

```text
norm[ch] = clamp((filtered_raw[ch] - off[ch]) / span[ch], 0.0, 1.5)
```

Values around `0.0` mean unloaded. Values around `1.0` mean normal loaded
standing pressure for that specific sensor.

### Contact Detection

Use hysteresis on normalized values:

```text
contact turns ON  when norm >= on_threshold
contact turns OFF when norm <= off_threshold
```

Suggested defaults:

```text
on_threshold  = 0.35
off_threshold = 0.20
```

This means each FSR is calibrated so that "off the ground" maps near 0 and
"standing loaded" maps near 1, then contact is detected from the normalized
range instead of raw voltage.

### Calibration Quality Checks

Warn the user if any channel has insufficient span:

```text
on[ch] - off[ch] < minimum_span
```

This usually means the sensor did not load during the on calibration, the sensor
is disconnected, or the channel mapping is wrong.

Recommended GUI status:

```text
LH OK / weak / invalid
LT OK / weak / invalid
RH OK / weak / invalid
RT OK / weak / invalid
```

## Processing Pipeline

```text
PhAI V2.2 COBS CDC packet
-> parse module 0xF0 float[4]
-> LPF per channel
-> calibration normalization
-> hysteresis contact bits
-> 16-state lookup
-> duration-aware quiet standing override
-> transition/event detector
-> GUI plots, state panels, event log, CSV recorder
```

## GUI Plan

Main views:

```text
4-channel normalized signal plot
raw/normalized toggle
per-channel contact indicators: LH, LT, RH, RT
current 4-bit pattern
current 16-state label
current high-level state: standing, double support, left single, right single, unusual
event timeline markers
recent event log
calibration controls
```

Calibration controls:

```text
Capture Zero/Off
Capture On/Loaded
Clear Calibration
Save Calibration JSON
Load Calibration JSON
```

Recommended plotting:

```text
normalized channels on y=0..1.5
horizontal lines for on/off thresholds
vertical markers for the 6 public gait events
```

## CSV Columns

```csv
time_s,seq_id,
LHeel_raw,LToe_raw,RHeel_raw,RToe_raw,
LHeel_norm,LToe_norm,RHeel_norm,RToe_norm,
LH,LT,RH,RT,
bits,state,high_level_state,event
```

If multiple events occur on one sample, join them with `|`.

For the first student-facing version, `event` should normally contain only one
of the 6 public event labels. Internal contact edges can be added later as
optional debug columns if needed.

## Implementation Notes

- Start from `cdc_4ch_live_monitor.py` for CDC parsing and serial threading.
- Reuse LPF and zeroing ideas from `cdc_fuzzy_gait_monitor_final.py`, but replace
  the one-foot fuzzy phase classifier with the bilateral 4-bit state mapping.
- Keep the public gait event set small. The 16-state table and internal sensor
  edges provide enough room for later student extensions.
- Keep thresholds configurable in the GUI.
- Save calibration as JSON next to the CDC scripts or in `PythonDecoder/CDC/data/`.
- Do not require hardware flashing from this tool.
