# API Reference: KIT H10 Control & Data Interface

> 📌 **After reading this page** you will be able to read the H10 state via `XM.status` and send torque / PI Vector commands using `XM_Set*`.
> ⏱️ Estimated reading time: 30 minutes
> 🧰 Prerequisites: IPO model (Input → Process → Output 1 ms cycle) + [Ex.11~14](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/11_Passive_Mode/)
> 🎯 Key objects: `XM.status` (read) / `XM.command` (staging commands) / `XM_SetControlMode` / `XM_SetAssistTorque` / `XM_SendPVector`

One of the core values of `XM10` is controlling the `KIT H10` exoskeleton with algorithms you design yourself. This API provides everything you need to check the connection status to KIT H10, receive the robot's live state data, and send control commands — such as `PIF-Vectors` and `Aux inputs` — to drive the robot's motion.
This is the detailed reference for the **robot data and control API** defined in `xm_api_data.h`.
The XM10 firmware implements a **Facade pattern** so users can read robot state and issue commands through an intuitive global object (`XM`) without worrying about the underlying communication protocols (CAN-FD, UART).

---

## 📌 Operating Principle

The XM10 control system follows a strict **IPO (Input-Process-Output)** model, executed precisely on a 1 ms (1 kHz) cycle by the internal **`core_process`** engine.

### The IPO Cycle (1 ms Loop)

1.  **Input (Data Gathering):**

      * At the start of each loop, the system collects the latest data from all connected hardware — the `H10`, `GRF Module`, `IMU Module`, and others.
      * The collected data is converted to physical units (degrees, Nm, etc.) and written into the **`XM.status`** struct.
      * Users are guaranteed to always see **the most recent data snapshot** at this stage.

2.  **Process (User Loop):**

      * The user-written `Control_Loop()` (or TSM Loop) executes.
      * Users read `XM.status` to evaluate the current state and run their control algorithm.
      * Computed control commands (torque, etc.) are staged into the **`XM.command`** struct via **`XM_Set...`** functions.

3.  **Output (Command Flushing):**

      * After the user loop finishes, the system checks whether anything in `XM.command` has changed.
      * When in torque control mode (`XM_CTRL_TORQUE`), updated commands are dispatched to the actual hardware (CAN Bus).

4.	**Data Logging (MSC) or Streaming (CDC):**

      * After Input, Process, and Output are complete, data logging or streaming is performed.
      * If a USB memory device is connected, user-defined data is saved to it every 1 ms.
      * If XM10 is connected to a PC via USB and the string `AGRB MON START` is sent over the serial port, user-defined data is forwarded to the terminal every 1 ms. Send `AGRB MON STOP` to halt streaming.

> **Note:** Users never need to call receive or flush functions manually. Simply read data and set commands.
> **Note:** No complex logic is required to save data. Just define the data struct you want to log and call the data-transmission API function.

-----

## 🛠 Data Structures

All data is accessed through `XM`, the global instance of type **`XmRobot_t`**.

### `XmControlMode_t`

Defines the control mode.

```c
typedef enum {
    XM_CTRL_MONITOR = 0,  // No control commands sent (Safety)
    XM_CTRL_TORQUE  = 1   // Control commands sent (Active)
} XmControlMode_t;
```

### `XmH10Mode_t`

The current operating state of the H10 suit.

```c
typedef enum {
    XM_H10_MODE_STANDBY = 0,  // Standby
    XM_H10_MODE_ASSIST  = 1,  // Assist force active
} XmH10Mode_t;
```

### `PVector_t`

Position trajectory command vector.

```c
typedef struct {
    int16_t  yd; // Desired Position (unit: deg, scaled by 100)
    uint16_t L;  // Trajectory Duration (ms)
    uint8_t  s0; // Acceleration Profile (deg/s^2)
    uint8_t  sd; // Deceleration Profile (deg/s^2)
} PVector_t;
```

### `IVector_t`

Impedance control parameter vector.

```c
typedef struct {
    uint8_t  epsilon; 	// Half width of Corridor (deg, scaled by 10)
    uint8_t  kp;      	// Virtual Spring Magnitude (%)
    uint8_t  kd;      	// Virtual Damper Magnitude (%)
    uint8_t  lambda;  	// Impedance Ratio (scaled by 100)
    uint16_t duration;	// Transition Duration (ms)
} IVector_t;
```

### `FVector_t`

Force trajectory command vector for H10.

```c
typedef struct {
    uint16_t modeIdx; 	// Torque Profile Index, Tp : 0.1 ~ 10
    int16_t  tauMax;  	// Max Torque (A, scaled by 100)
    uint16_t delay;   	// Initial Delay (ms)
	uint16_t zero;		// Dummy data for reset
} FVector_t;
```

### `XmH10Data_t`

Core data received from the H10 wearable robot body. Access path: `XM.status.h10`.
The actual fields received may be revised in future firmware versions.

```c
typedef struct {
    bool  is_connected;      // Connection status

    // --- Info & State ---
    uint32_t h10AssistModeLoopCnt;  // H10 assist mode loop counter (counts from assist mode start)
    uint32_t h10PostProcessingCnt;  // GaitAnalysis post-processing sample count (0~30000)
    XmH10Mode_t h10Mode;    // H10 operating mode (Assist(1) <-> Standby(0))
    uint8_t h10AssistLevel; // H10 assist level (0~10)
    uint8_t h10FSMcurrentState; // H10 current FSM state
    bool isPVectorRHDone;   // RH P-Vector completion flag
    bool isPVectorLHDone;   // LH P-Vector completion flag
    bool h10IsNeutralPosSet; // H10 neutral position set status

    // --- Kinematics Data ---
    float leftHipAngle;     // Left hip joint angle (degrees)
    float rightHipAngle;    // Right hip joint angle
    float leftThighAngle;   // Left thigh absolute angle (degrees)
    float rightThighAngle;  // Right thigh absolute angle
    float leftKneeAngle;    // Left knee angle (estimated)
    float rightKneeAngle;   // Right knee angle (estimated)
    float pelvicAngle;      // Pelvic tilt angle

    // --- Gait Data ---
    bool isLeftFootContact;  // Left foot contact state
    bool isRightFootContact; // Right foot contact state
    float forwardVelocity;  // Forward walking velocity (m/s)

    // --- Motor Data ---
    // [Note] leftHipTorque / rightHipTorque contain motor current (A), despite the field name.
    //        Joint torque conversion: τ_joint[Nm] ≈ Kt(0.085) × gear(18.75) × hipTorque[A] ≈ 1.594 × hipTorque
    float leftHipTorque;      // Left motor current (A) — unit is A, not Nm
    float rightHipTorque;     // Right motor current (A) — unit is A, not Nm
    float leftHipMotorAngle;  // Left motor encoder angle (degrees)
    float rightHipMotorAngle; // Right motor encoder angle

    // --- IMU Data (detailed inertial sensor information) ---
    // Orientation
    float leftHipImuFrontalRoll;    // Left hip IMU Frontal Roll angle (degrees)
    float rightHipImuFrontalRoll;
    float leftHipImuSagittalPitch;  // Left hip IMU Sagittal Pitch angle (degrees)
    float rightHipImuSagittalPitch;
    float leftHipImuTransverseYaw;  // Left hip IMU Transverse Yaw angle (degrees)
    float rightHipImuTransverseYaw;
    
    // Global Acceleration (m/s^2)
    float leftHipImuGlobalAccX;	// Left hip IMU global acceleration
    float leftHipImuGlobalAccY;
    float leftHipImuGlobalAccZ;
    float rightHipImuGlobalAccX;
    float rightHipImuGlobalAccY;
    float rightHipImuGlobalAccZ;

    // Global Gyroscope (deg/s)
    float leftHipImuGlobalGyrX; // Left hip IMU global gyroscope
    float leftHipImuGlobalGyrY;
    float leftHipImuGlobalGyrZ;
    float rightHipImuGlobalGyrX;
    float rightHipImuGlobalGyrY;
    float rightHipImuGlobalGyrZ;
} XmH10Data_t;
```

Bold fields are currently being received. Additional fields may be added or modified in future releases.

| Field Name | Type | Unit | Description |
| :--- | :--- | :--- | :--- |
| **`is_connected`** | `bool` | - | H10 communication link status (`true`: connected) |
| **`h10AssistModeLoopCnt`** | `uint32_t` | - | H10 assist mode loop counter |
| **`h10Mode`** | `XmH10Mode_t` | - | Current H10 operating mode (`STANDBY` / `ASSIST`) |
| **`h10AssistLevel`** | `uint8_t` | 0\~10 | H10 assist-strength dial. Recommended: multiply assist torque by `h10AssistLevel/10.0` to reflect the dial (level 0 → torque 0). See `XM_SetAssistTorque`. |
| **`isPVectorRHDone`** | `bool` | - | Flag indicating the right-hip (RH) P-Vector has completed |
| **`isPVectorLHDone`** | `bool` | - | Flag indicating the left-hip (LH) P-Vector has completed |
| **`leftHipAngle`** | `float` | deg | Left hip joint angle (Extension \< 0 \< Flexion) |
| **`rightHipAngle`** | `float` | deg | Right hip joint angle |
| **`leftThighAngle`** | `float` | deg | Left thigh absolute angle (relative to vertical) |
| **`rightThighAngle`** | `float` | deg | Right thigh absolute angle |
| **`leftKneeAngle`** | `float` | deg | Left knee angle (estimated) |
| **`rightKneeAngle`** | `float` | deg | Right knee angle (estimated) |
| **`pelvicAngle`** | `float` | deg | Pelvic lateral tilt |
| **`isLeftFootContact`** | `bool` | - | Left foot contact state (`true`: on ground) |
| **`isRightFootContact`** | `bool` | - | Right foot contact state |
| **`forwardVelocity`** | `float` | m/s | Forward velocity (estimated) |
| **`leftHipTorque`** | `float` | A | Left motor current — unit is **A** despite the field name (joint torque ≈ ×1.594 Nm) |
| **`rightHipTorque`** | `float` | A | Right motor current — unit is **A** despite the field name (joint torque ≈ ×1.594 Nm) |
| **`leftHipMotorAngle`** | `float` | deg | Left motor encoder angle (feedback) |
| **`rightHipMotorAngle`** | `float` | deg | Right motor encoder angle (feedback) |
| `leftHipImuFrontalRoll` | `float` | deg | Left hip IMU Frontal Roll angle |
| `rightHipImuFrontalRoll` | `float` | deg | Right hip IMU Frontal Roll angle |
| `leftHipImuSagittalPitch` | `float` | deg | Left hip IMU Sagittal Pitch angle |
| `rightHipImuSagittalPitch` | `float` | deg | Right hip IMU Sagittal Pitch angle |
| **`leftHipImuGlobalAccX`** | `float` | m/s^2 | Left hip IMU global acceleration X |
| **`leftHipImuGlobalAccY`** | `float` | m/s^2 | Left hip IMU global acceleration Y |
| **`leftHipImuGlobalAccZ`** | `float` | m/s^2 | Left hip IMU global acceleration Z |
| **`rightHipImuGlobalAccX`** | `float` | m/s^2 | Right hip IMU global acceleration X |
| **`rightHipImuGlobalAccY`** | `float` | m/s^2 | Right hip IMU global acceleration Y |
| **`rightHipImuGlobalAccZ`** | `float` | m/s^2 | Right hip IMU global acceleration Z |
| **`leftHipImuGlobalGyrX`** | `float` | deg/s | Left hip IMU global gyroscope X |
| **`leftHipImuGlobalGyrY`** | `float` | deg/s | Left hip IMU global gyroscope Y |
| **`leftHipImuGlobalGyrZ`** | `float` | deg/s | Left hip IMU global gyroscope Z |
| **`rightHipImuGlobalGyrX`** | `float` | deg/s | Right hip IMU global gyroscope X |
| **`rightHipImuGlobalGyrY`** | `float` | deg/s | Right hip IMU global gyroscope Y |
| **`rightHipImuGlobalGyrZ`** | `float` | deg/s | Right hip IMU global gyroscope Z |

### `XM_GRF_SPACE_e`

Connection ID for the left/right GRF Module.

```c
typedef enum {
    XM_SPACE_LEFT = 1,
    XM_SPACE_RIGHT,
    XM_SPACE_UNKNOWN,
} XM_GRF_SPACE_e;
```

### `XmGrfData_t`

Data from the GRF Module.

```c
typedef struct {
    bool     is_left_grf_connected;  // Left GRF module connection status
    bool     is_right_grf_connected; // Right GRF module connection status

    // --- Left Foot Data ---
    // Data from packets where sensorSpace == LEFT(1)
    uint32_t leftLastUpdateTick;    // Timestamp of last received data (ms)
    XM_GRF_SPACE_e leftSensorSpace; // 1=left foot, 2=right foot
    uint8_t leftRollingIndex;       // Packet sequence 0-199
    uint8_t leftSensorData[XM_GRF_CHANNEL_SIZE]; // 14-channel values (0~255 raw)
    uint8_t leftBatteryLevel;       // Battery level (0~100)
    uint8_t leftStatusFlags;        // Status flags
    
    // --- Right Foot Data ---
    // Data from packets where sensorSpace == RIGHT(2)
    uint32_t rightLastUpdateTick;
    XM_GRF_SPACE_e  rightSensorSpace;   // 1=left foot, 2=right foot
    uint8_t  rightRollingIndex;         // Packet sequence 0-199
    uint8_t  rightSensorData[XM_GRF_CHANNEL_SIZE]; // (0~255 raw)
    uint8_t  rightBatteryLevel;
    uint8_t  rightStatusFlags;
} XmGrfData_t;
```

Bold fields are currently being received.

| Field Name | Type | Unit | Description |
| :--- | :--- | :--- | :--- |
| **`is_left_grf_connected`** | `bool` | - | Left GRF module connection status |
| **`is_right_grf_connected`** | `bool` | - | Right GRF module connection status |
| **`leftLastUpdateTick`** | `uint32_t` | ms | Timestamp of last left-foot data received |
| **`leftSensorSpace`** | `XM_GRF_SPACE_e` | - | 1=left foot, 2=right foot |
| **`leftRollingIndex`** | `uint8_t` | - | Left-foot packet sequence 0-199 |
| **`leftSensorData[14]`** | `uint8_t` | - | Left-foot 14-channel values (0~255 raw) |
| **`leftBatteryLevel`** | `uint8_t` | - | Left-foot battery level (0~100) |
| **`leftStatusFlags`** | `uint8_t` | - | Left-foot status flags |
| **`rightLastUpdateTick`** | `uint32_t` | ms | Timestamp of last right-foot data received |
| **`rightSensorSpace`** | `XM_GRF_SPACE_e` | - | 1=left foot, 2=right foot |
| **`rightRollingIndex`** | `uint8_t` | - | Right-foot packet sequence 0-199 |
| **`rightSensorData[14]`** | `uint8_t` | - | Right-foot 14-channel values (0~255 raw) |
| **`rightBatteryLevel`** | `uint8_t` | - | Right-foot battery level (0~100) |
| **`rightStatusFlags`** | `uint8_t` | - | Right-foot status flags |

### `XmExtImuData_t`

Data from the XSENS IMU (MTi-630). You must call `XM_AttachXsensMTi630()` (and `XM_ConfigureXsensMTi630()` if needed) before using this data, and the hardware must be physically connected.
On Rev 1.1, attaching this module repurposes the `XM_EXT_ADC_1` / `XM_EXT_ADC_3` pins as UART. Rev 2.0 uses a dedicated USART2 port and does not occupy ADC pins. See §3.5 of the [external IO documentation](04-external-io.md) for details.

```c
typedef struct {
    bool  is_connected; // XSENS IMU module connection status
    uint32_t lastUpdateTick; // Timestamp of last received data (ms)

    // --- 1. Orientation (Quaternion) ---
    float q_w, q_x, q_y, q_z;

    // --- 2. Calibrated Acceleration (m/s^2) ---
    float acc_x, acc_y, acc_z;

    // --- 3. Calibrated Gyroscope (deg/s or rad/s) ---
    float gyr_x, gyr_y, gyr_z;
} XmExtImuData_t;
```

Bold fields are currently being received.

| Field Name | Type | Unit | Description |
| :--- | :--- | :--- | :--- |
| **`is_connected`** | `bool` | - | XSENS IMU module connection status |
| **`lastUpdateTick`** | `uint32_t` | ms | Timestamp of last received data |
| **`q_w`** | `float` | - | Orientation (Quaternion) w |
| **`q_x`** | `float` | - | Orientation (Quaternion) x |
| **`q_y`** | `float` | - | Orientation (Quaternion) y |
| **`q_z`** | `float` | - | Orientation (Quaternion) z |
| **`acc_x`** | `float` | m/s^2  | Calibrated acceleration x |
| **`acc_y`** | `float` | m/s^2  | Calibrated acceleration y |
| **`acc_z`** | `float` | m/s^2 | Calibrated acceleration z |
| **`gyr_x`** | `float` | deg/s | Calibrated gyroscope x |
| **`gyr_y`** | `float` | deg/s | Calibrated gyroscope y |
| **`gyr_z`** | `float` | deg/s | Calibrated gyroscope z |

### `XmInput_t`

Aggregate robot state struct. Access it through `XM.status`.

```c
typedef struct {
    XmH10Data_t     h10;      // H10 robot body data
    XmGrfData_t     grf;      // GRF foot pressure sensor data
    XmExtImuData_t  ext_imu;  // External UART IMU (Xsens MTi-630)
    XmImuHubData_t  imu_hub;  // IMU Hub sensor (CAN-FD)
    XmEmgHubData_t  emg_hub;  // EMG Hub sensor (CAN-FD)
    XmFesHubData_t  fes_hub;  // FES Hub stimulation feedback (CAN-FD)
} XmInput_t;
```

### `XmOutput_t`

Robot control command struct.

```c
typedef struct {
    XmControlMode_t control_mode; // Current control mode

    float assist_torque_rh;
    float assist_torque_lh;
    
    /* Dirty Flags (managed internally by helper functions — users do not need to touch these) */
    struct {
        uint8_t torque_rh_updated : 1;
        uint8_t torque_lh_updated : 1;
    } _dirty_flags;
} XmOutput_t;
```

### `XmRobot_t` (Global Instance `XM`)

```c
typedef struct {
    XmInput_t  status;  // [Read] Sensor data (H10, GRF, IMU)
    XmOutput_t command; // [Write] Control commands (use helper functions)
} XmRobot_t;
```

  * **Key field access examples:**
      * `XM.status.h10.leftHipAngle`: Left hip joint angle (degrees)
      * `XM.status.h10.rightHipTorque`: Right motor current (A) — unit is A, not Nm
      * `XM.status.grf.leftSensorData`: Left FSR sensor array
      * `XM.status.ext_imu.acc_z`: External IMU vertical acceleration
      * ...

-----

## 📚 Functions

Before starting your algorithm, always confirm that `XM10` has a stable communication link with the `KIT H10` Control Module (CM).

### `XM_IsCmConnected()`

Checks whether the communication link with the **Control Module (CM)** is `Operational`.
The connection between CM and XM10 is managed by an internal `Plug and Play` background task. The link is considered active **from the moment the first PDO data packet is received**.

**Syntax**
```c
bool XM_IsCmConnected(void);
```

**Returns**
- `true`: The link is active and operational.
- `false`: The link is not yet established or has been lost.

**Example**
```c
#include "xm_api.h"

void Off_loop(void) {
    // Transition to Standby once the CM link is confirmed.
    if (XM_IsCmConnected()) {
        XM_TSM_TransitionTo(s_mainTaskHandle, XM_STATE_STANDBY);
    }
}
```

---

### `XM_GetXMNmtState()`

```c
CM_NmtState_t XM_GetXMNmtState(void);
```

| Item | Details |
|------|------|
| **Description** | Returns the current DOP V3 PnP (NMT) state for the CM link. |
| **Return value** | `CM_NmtState_t` enum — current NMT state |
| **Call location** | `Control_Loop()` |

**NMT state values:**

| State | Value | Description |
|------|-----|------|
| `CM_NMT_INITIALISING` | 0 | Booting (boot-up message not yet received) |
| `CM_NMT_PRE_OPERATIONAL` | 1 | SDO communication available; PDO inactive |
| `CM_NMT_OPERATIONAL` | 2 | All communication active (normal state) |
| `CM_NMT_STOPPED` | 3 | Communication halted |

> **Note:** `XM_IsCmConnected()` internally checks `XM_GetXMNmtState() == CM_NMT_OPERATIONAL`.
> Use this function directly when you need finer-grained branching based on NMT state.

---

### `XM_SetControlMode`

Sets the control authority mode for the robot. This is a safety-critical function.
The default mode is monitoring mode, which does not send real-time control commands to H10.
To enable real-time torque control, call `XM_SetControlMode` with `XM_CTRL_TORQUE`.

**Syntax**
```c
void XM_SetControlMode(XmControlMode_t mode);
```

**Parameters**
  * `mode`: The mode to set.
	  * `XM_CTRL_MONITOR` (0): **Monitoring mode.** No control commands are sent. (Default, safe)
	  * `XM_CTRL_TORQUE` (1): **Torque control mode.** Staged torque commands are sent to the motors.

**Safety Logic**
  * Whenever the mode changes (e.g., Monitor → Torque), **all torque commands are immediately reset to 0.0 internally**. This prevents sudden jerk at the moment control begins.

**Example**
```c
// Enable torque control mode when entering the algorithm
void Active_Entry(void) {
	XM_SetControlMode(XM_CTRL_TORQUE);
}

// Return safely to monitoring mode when exiting the algorithm
void Active_Exit(void) {
	XM_SetControlMode(XM_CTRL_MONITOR);
}
```

---

To use H10's built-in assist mode as-is, call `XM_SetH10AssistExistingMode` with `true`.

### `XM_SetH10AssistExistingMode()`

Pass `true` (1) to enable H10's built-in assist algorithm; pass `false` (0) to disable it.
By default, the built-in assist algorithm is disabled when XM10 connects to H10.

**Syntax**
```c
void XM_SetH10AssistExistingMode(bool isSet);
```

**Returns**
- `true`: H10's built-in assist algorithm is enabled.
- `false`: H10's built-in assist algorithm is disabled.

**Example**
```c
#include "xm_api.h"

void Off_Entry(void) {
    XM_SetH10AssistExistingMode(true);
}
```

---

## Data Outputs (Control Inputs)

`P-Vector`, `I-Vector`, and `F-Vector` (PIF-Vectors), together with various other control commands, let you precisely design the motion of `KIT H10`.
**For a detailed explanation of pre-defined control techniques such as PIF-Vectors, refer to `angel Robotics-Control Algorithm` (coming soon).**

### `XM_SendPVector()`

Sends a **position-based trajectory** (`P-Vector`) that commands the joint to move to a target position with a smooth trajectory over a specified duration.
**An impedance control parameter (`I-Vector`) must be configured beforehand.**
**When a `P-Vector` is sent, the motor driver generates a 5th-order polynomial position trajectory.**

<figure markdown="span">
  ![P-Vector based position trajectory](https://github.com/user-attachments/assets/ebd67c3d-2b5d-4453-b081-c20d8750204d){ width="90%" }
  <figcaption>▲ Figure 1. Example of position trajectory generation using P-Vector</figcaption>
</figure>


**Syntax**
```c
void XM_SendPVector(SystemNodeID_t nodeId, const PVector_t* pVector);
```

**Parameters**
- `nodeId`: The joint to command (`SYS_NODE_ID_RH` or `SYS_NODE_ID_LH`).
- `pVector`: Pointer to a `PVector_t` struct containing the target position (`yd`), travel duration (`L`), initial acceleration (`s0`), and final deceleration (`sd`).

**Returns**
None.

**Key members of the `pVector` struct:**

| Member | Description | Unit | Type |
| :--- | :--- | :--- | :--- |
| `yd` | Target position | degree, scaled by 100 | int16_t |
| `L` | Travel duration | ms | uint16_t |
| `s0`| Initial acceleration | deg/s^2 | uint8_t |
| `sd` | Final deceleration | deg/s^2 | uint8_t |

**Example**
```c
static void UpdatePassiveMode(void)
{
    // Read the current angle from the XM.status.h10 snapshot.
    int16_t currentAngleRH = (int16_t)round(XM.status.h10.rightHipMotorAngle * 10.0f);
    int16_t currentAngleLH = (int16_t)round(XM.status.h10.leftHipMotorAngle * 10.0f);

    switch (s_passiveState) {
        case PASSIVE_STATE_SET_IMPEDANCE: {
            // Set impedance (stiffness) for position control
            IVector_t stiffImpedance = { .epsilon = 0, .kp = 80, .kd = 1, .lambda = 0, .duration = 50 };
            XM_SendIVector(SYS_NODE_ID_RH, &stiffImpedance);
            XM_SendIVector(SYS_NODE_ID_LH, &stiffImpedance);
            s_passiveState = PASSIVE_STATE_START_MOTION;
            break;
        }
        case PASSIVE_STATE_START_MOTION: {
            // Send P-Vector to move to the first target angle
            int16_t targetAngle = JOINT_ANGLE_MAX_ANGLE_INT16;

            // Calculate the required duration to reach the target angle
            int16_t angleToMoveRH = abs(targetAngle - currentAngleRH);
            int16_t angleToMoveLH = abs(targetAngle - currentAngleLH);
            uint16_t durationRH = (uint16_t)(((float)angleToMoveRH / (float)PM_SPEED_RH) * 1000.0f);
            uint16_t durationLH = (uint16_t)(((float)angleToMoveLH / (float)PM_SPEED_LH) * 1000.0f);

            PVector_t pVecRH = { .yd = targetAngle, .L = durationRH, .s0 = PM_ACCEL_S0_RH, .sd = PM_ACCEL_SD_RH };
            PVector_t pVecLH = { .yd = targetAngle, .L = durationLH, .s0 = PM_ACCEL_S0_LH, .sd = PM_ACCEL_SD_LH };
            XM_SendPVector(SYS_NODE_ID_RH, &pVecRH);
            XM_SendPVector(SYS_NODE_ID_LH, &pVecLH);

            s_passiveState = PASSIVE_STATE_MOVING_TO_MIN;
            break;
        }
        // Handle subsequent states
        ...
```

### `XM_SendIVector()`

Sends **impedance control parameters** (`I-Vector`) to configure the robot joint to behave like a virtual spring or damper.
You must carefully tune the maximum values of `kp` and `kd` against the **actuator's maximum torque of 10 Nm** and the overall system behavior. (See `XM_SendIVectorKpKdMax()`.)
**The actuator's maximum current is 14 A. The motor driver's internal impedance control clamps its output at 10 A (saturation).**

<figure markdown="span">
  ![I-Vector and P-Vector simulation](https://github.com/user-attachments/assets/abd3a1e3-55cd-4f33-b103-52c22d88a4a2){ width="90%" }
  <figcaption>▲ Figure 2. Simulation example of position-based control using I-Vector (red) and P-Vector (blue)</figcaption>
</figure>


**Syntax**
```c
void XM_SendIVector(SystemNodeID_t nodeId, const IVector_t* iVector);
```

**Parameters**
- `nodeId`: The joint to command.
- `iVector`: Pointer to an `IVector_t` struct containing impedance parameters (stiffness `kP`, damping `kD`, etc.).

**Returns**
None.

**Key members of the `iVector` struct:**

| Member | Description | Unit | Type |
| :--- | :--- | :--- | :--- |
| `epsilon` | Half-width of the impedance corridor | degree, scaled by 10 | uint8_t |
| `kp` | Virtual spring magnitude | % | uint8_t |
| `kd`| Virtual damper magnitude | % | uint8_t |
| `lambda` | Impedance ratio | ratio, scaled by 100 | uint8_t |
| `duration`| Transition duration | ms | uint16_t |

**Example**
```c
static void EnterStandbyMode(void)
{
    // Release impedance parameters
    IVector_t stiffImpedance = { .epsilon = 0, .kp = 0, .kd = 0, .lambda = 0, .duration = 50 };
    XM_SendIVector(SYS_NODE_ID_RH, &stiffImpedance);
    XM_SendIVector(SYS_NODE_ID_LH, &stiffImpedance);
    XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
    XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);
}
```

### `XM_SendFVector()`

Sends a **force-based trajectory** (`F-Vector`) that commands the motor driver to generate a **predefined torque trajectory over a specified duration**.

<figure markdown="span">
  ![F-Vector based force trajectory 1](https://github.com/user-attachments/assets/a39ffb45-f10f-4e61-a1c7-b0f235dbc0c7){ width="90%" }
  ![F-Vector based force trajectory 2](https://github.com/user-attachments/assets/3560f9a4-d9fa-407e-b9cd-eb24faae42c9){ width="90%" }
  <figcaption>▲ Figure 3. Example of force trajectory generation using F-Vector</figcaption>
</figure>

**Syntax**
```c
void XM_SendFVector(SystemNodeID_t nodeId, const FVector_t* fVector);
```

**Parameters**
- `nodeId`: The joint to command.
- `fVector`: Pointer to an `FVector_t` struct containing the target torque (`tauMax`), mode (`modeIdx`), and other parameters.

**Returns**
None.

**Key members of the `fVector` struct:**

| Member | Description | Unit | Type |
| :--- | :--- | :--- | :--- |
| `modeIdx` | Torque profile index | Index (0.1 ~ 10) | uint16_t |
| `tauMax` | Maximum torque | A, scaled by 100	 | int16_t |
| `delay`| Initial delay | ms | uint16_t |
| `zero` | Dummy data for reset | - | uint16_t |

**Example**
```c
(coming soon)
```

### `XM_SendPVectorReset()`

Immediately cancels the currently executing or pending `P-Vector` command. Use this to abort the current motion in an emergency or when the intended trajectory changes.

**Syntax**
```c
void XM_SendPVectorReset(SystemNodeID_t nodeId);
```

**Parameters**
- `nodeId`: The joint whose P-Vector should be reset (`SYS_NODE_ID_RH` or `SYS_NODE_ID_LH`).

**Returns**
None.

**Example**
```c
static void ManageModeTransition(void)
{
    // Get the current mode
    XmH10Mode_t currentSuitMode  = XM.status.h10.h10Mode;

    switch (s_modeTransitionState) {
        case MODE_TRANSITION_IDLE:
            // Check whether a mode change has been detected.
            if (currentSuitMode != s_previousSuitMode) {
                
                // Transition from Passive Mode to Standby Mode:
                // Begin the safe-stop sequence for Passive Mode, which was using P-Vectors.
                if (s_previousSuitMode == XM_H10_MODE_ASSIST && currentSuitMode == XM_H10_MODE_STANDBY) {
                    XM_SendPVectorReset(SYS_NODE_ID_RH);   // Cancel P-Vector trajectory generation
                    XM_SendPVectorReset(SYS_NODE_ID_LH);
                    s_modeTransitionTimer = XM_GetTick();  // Start the reset delay timer
                    s_modeTransitionState = MODE_TRANSITION_STOP_PENDING; // Advance to next state
                }
            }
            break;
        // Handle subsequent states
        ...
```

### `XM_ClearPVectorDoneFlag()`

After you detect that `isPVectorRHDone` or `isPVectorLHDone` has become `true` via `XM.status.h10`, you must call this function manually to acknowledge the event to XM10. Without this call, the flag remains `true` and the same completion event will be processed repeatedly.
Calling this function resets the corresponding `isPVectorRHDone` or `isPVectorLHDone` flag (via `XM.status.h10`) to `false`.

**Syntax**
```c
void XM_ClearPVectorDoneFlag(SystemNodeID_t nodeId);
```

**Parameters**
- `nodeId`: The joint whose completion flag should be cleared (`SYS_NODE_ID_RH` or `SYS_NODE_ID_LH`).

**Example**
```c
static void EnterStandbyMode(void)
{
    // Release impedance parameters
    IVector_t stiffImpedance = { .epsilon = 0, .kp = 0, .kd = 0, .lambda = 0, .duration = 50 };
    XM_SendIVector(SYS_NODE_ID_RH, &stiffImpedance);
    XM_SendIVector(SYS_NODE_ID_LH, &stiffImpedance);
    XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
    XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);
}
```

### `XM_SendIVectorKpKdMax()`

Sets the **maximum Kp (stiffness) and Kd (damping) values** used in the impedance control for a given joint. These maximums serve as the 100% reference when `kp` and `kd` are specified as percentages in `XM_SendIVector`.

**Syntax**
```c
void XM_SendIVectorKpKdMax(SystemNodeID_t nodeId, const float kpMax, const float kdMax);
```

**Parameters**
- `nodeId`: The joint to configure (`SYS_NODE_ID_RH` or `SYS_NODE_ID_LH`).
- `kpMax`: The maximum Kp (virtual spring stiffness) applied when `XM_SendIVector`'s `kp` parameter is 100%.
- `kdMax`: The maximum Kd (virtual damper stiffness) applied when `XM_SendIVector`'s `kd` parameter is 100%.

**Example**
```c
static void InitHoming(void)
{
    static uint32_t homingTimer = 0;
    // --- Homing state machine ---
    switch (s_homingState) {
        case HOMING_ENTRY:
            XM_SendIVectorKpKdMax(SYS_NODE_ID_RH, 6, 1);
            XM_SendIVectorKpKdMax(SYS_NODE_ID_LH, 6, 1);
            s_homingState = HOMING_SET_IMPEDANCE;
            break;
        // Handle subsequent states
        ...
```

---

### `Set...` Routines and Parameters

Functions prefixed with `Set` activate various built-in control assistance routines in `KIT H10` or adjust their parameters in real time. This lets users modify the robot's motion characteristics at a high level without implementing low-level control logic themselves.

#### 1. Angle and Velocity Limits

Constrains robot motion to a physically safe range.

**Syntax**
```c
// Enable / disable the angle limit routine
void XM_SetDegreeLimitRoutine(SystemNodeID_t nodeId, bool isSet);

// Set the angle limit range
void XM_SetDegreeLimit(SystemNodeID_t nodeId, float upperLimit, float lowerLimit);

// Enable / disable the velocity limit routine
void XM_SetVelocityLimitRoutine(SystemNodeID_t nodeId, bool isSet);

// Set the velocity limit range
void XM_SetVelocityLimit(SystemNodeID_t nodeId, float upperLimit, float lowerLimit);
```

**Parameters**
- `nodeId`: The joint to control (`SYS_NODE_ID_RH` or `SYS_NODE_ID_LH`).
- `isSet`: Pass `true` to enable the routine; pass `false` to disable it.
- `upperLimit`: Upper bound of the range (unit: degrees or deg/s).
- `lowerLimit`: Lower bound of the range (unit: degrees or deg/s).

**Example**
```c
// Enable the angle limit for the right leg
// and set the range of motion to -30 to 30 degrees.
XM_SetDegreeLimitRoutine(SYS_NODE_ID_RH, true);
XM_SetDegreeLimit(SYS_NODE_ID_RH, 30.0f, -30.0f);

// Limit the left leg's maximum speed to 100 deg/s.
XM_SetVelocityLimitRoutine(SYS_NODE_ID_LH, true);
XM_SetVelocityLimit(SYS_NODE_ID_LH, 100.0f, -100.0f);
```

---

#### 2. Disturbance Observer (DOB)

An advanced built-in control routine in `KIT H10` that estimates and compensates for user-applied forces and unexpected external disturbances, producing smoother and more stable motion.
**To use DOB, the `KIT H10` actuators must have undergone System Identification for DOB, and the identification data must be stored in the motor driver. (System Identification for DOB has not been performed on the current `KIT H10`; this will change in a future revision.)**
**For details on KIT H10's DOB, refer to `angel Robotics-Control Algorithm` (coming soon).**

**Syntax**
```c
void XM_SetDOBRoutine(SystemNodeID_t nodeId, bool isSet);
```

**Parameters**
- `nodeId`: The joint to control (`SYS_NODE_ID_RH` or `SYS_NODE_ID_LH`).
- `isSet`: Pass `true` to enable the DOB routine; pass `false` to disable it.

**Example**
```c
XM_SetDOBRoutine(SYS_NODE_ID_RH, true);
```

---

#### 3. Compensation Gain

Adjusts the strength of KIT H10's built-in gravity/velocity compensation modes.
**For details on KIT H10's compensation, refer to `angel Robotics-Compensation` (coming soon).**

**Syntax**
```c
// General compensation gain (gravity compensation, etc.)
void XM_SetNormalCompGain(SystemNodeID_t nodeId, uint8_t gain);

// Resistive compensation gain (resistance training mode)
void XM_SetResistiveCompGain(SystemNodeID_t nodeId, float gain);
```

**Parameters**
- `nodeId`: The joint to control (`SYS_NODE_ID_RH` or `SYS_NODE_ID_LH`).
- `gain`: The gain value to set. The range and effect depend on the compensation mode.

**Example**
```c
// Apply strong resistance to the right leg in resistance training mode.
float strongResistance = 0.8f;
XM_SetResistiveCompGain(SYS_NODE_ID_RH, strongResistance);
```

---

### `XM_SendUserBodyData()`

Sends the wearer's body parameters (weight, height, segment lengths, etc.) to `KIT H10`. KIT H10 uses this information for real-time motion analysis and returns more accurate, personalized gait data and motion dynamics data to XM10. **For details on KIT H10's real-time motion analysis, refer to `GaitAnalysis` (coming soon).**
**You must measure and supply the body parameters manually before sending them to KIT H10.**

**Body-parameter-dependent fields in `RxData_t`:**

| PDO field | Description | Unit | Type |
| :--- | :--- | :--- | :-- |
| `leftKneeAngle` | **Estimated** left knee angle | degree | float |
| `rightKneeAngle` | **Estimated** right knee angle | degree | float |
| `isLeftFootContact`| Left foot contact state | - | bool |
| `isRightFootContact` | Right foot contact state | - | bool |
| `forwardVelocity`| **Estimated** forward velocity | m/s | float |
...

**Syntax**
```c
void XM_SendUserBodyData(const uint32_t bodyData[8]);
```

**Parameters**

`bodyData` — array of 8 `uint32_t` body parameters:

| Index | Description | Unit | Type |
| :--- | :--- | :--- | :-- |
| `0` | Wearer weight | g | uint32_t |
| `1` | Wearer height | mm | uint32_t |
| `2`| Right thigh segment length | mm | uint32_t |
| `3` | Left thigh segment length | mm | uint32_t |
| `4`| Right shank segment length | mm | uint32_t |
| `5` | Left shank segment length | mm | uint32_t |
| `6`| Right ankle segment length | mm | uint32_t |
| `7` | Left shank segment length | mm | uint32_t |

**Example**
```c
// Fill in body parameters and send them to the CM.
bodyData[0] = 73000; // 73 kg
bodyData[1] = 1800;  // 180 cm
bodyData[2] = 500;   // 0.5 m
bodyData[3] = 495;   // 0.495 m
bodyData[4] = 440;   // 0.440 m
bodyData[5] = 435;   // 0.435 m
bodyData[6] = 60;    // 0.06 m
bodyData[7] = 59;    // 0.059 m
XM_SendUserBodyData(&bodyData[0]);
```

---

## Real-Time Control

These are the core functions used to apply torque in real time within the **1 ms** control loop.
Reading data is done by accessing the struct fields directly, but **all control outputs must be issued through the helper functions below.** These functions manage **dirty flags** internally to ensure only changed values are transmitted efficiently.

### `XM_SetAssistTorque`

Sets the assist torque for both legs simultaneously.
The actual transmission (`Output`) is handled internally by the `Core Process` following the IPO model.
Users only need to compute the desired torque and set it through this function.

**Syntax**
```c
void XM_SetAssistTorque(float rh, float lh);
```

**Parameters**
  * `rh`: Right hip joint assist torque (**Unit: Nm**)
  * `lh`: Left hip joint assist torque (**Unit: Nm**)

> ⚠️ The argument order is **`(right rh, left lh)`**. Swapping them sends assistance to the wrong leg; when in doubt, set one side at a time with `XM_SetAssistTorqueRH()` / `XM_SetAssistTorqueLH()`.

**Returns**: None

**Sign · Unit · Limit (must read)**
  * **Sign convention**: **positive(+) = Flexion assist, negative(−) = Extension assist** (matches the angle convention `Extension < 0 < Flexion`).
  * **Unit chain**: the input is joint torque [Nm]. Internally it is converted to motor current — `τ_joint[Nm] = Kt(0.085) × Gear(18.75) × I[A] ≈ 1.594 × I[A]`. Apply this conversion when comparing with the feedback `XM.status.h10.*HipTorque` (unit **A**).
  * **Hard limit (±10 Nm)**: the input torque is **clamped to ±10 Nm** inside the XM10 library (`CM_StageAuxTorque`, `libXM_Lib.a`) before it goes out on CAN. This is internal code, not the example, so users cannot change it; larger inputs saturate at ±10 Nm. (Motor-driver side: 14A max / 10A impedance saturation.)
  * **Assist level**: to reflect the suit assist-strength dial (`XM.status.h10.h10AssistLevel`, 0~10), multiply the torque by `h10AssistLevel / 10.0f` — at level 0 the output is 0.

**Operating Principle**
  * Stores the values in the `XM.command` struct and sets the `torque_updated` dirty flag.
  * The actual transmission occurs at the end of the current control cycle (`_FlushAllOutputs`).

**Example**
```c
void Active_Loop(void) {
	// P-control: generate torque proportional to the current angle
	float cmd_R = XM.status.h10.rightHipAngle * 0.5f;
	float cmd_L = XM.status.h10.leftHipAngle  * 0.5f;
	
	// Set torque commands for both legs
	XM_SetAssistTorque(cmd_R, cmd_L);
}
```

-----

### `XM_SetAssistTorqueRH` / `XM_SetAssistTorqueLH`

Sets the torque for a single leg independently. The opposite leg's torque value remains unchanged.

**Syntax**
```c
void XM_SetAssistTorqueRH(float rh);
void XM_SetAssistTorqueLH(float lh);
```

**Parameters**
  * `rh` / `lh`: Assist torque for the respective joint (**Unit: Nm**)

**Example**
```c
// Set the right leg to 5.0 Nm (left leg retains its previous value)
XM_SetAssistTorqueRH(5.0f);
```

---

## Utilities

### `XM_GetTick()`

Returns the elapsed time since system boot in milliseconds (ms). This is essential for time-based control: measuring the duration of an action, running logic for a fixed period, and so on.

**Syntax**
```c
uint32_t XM_GetTick(void);
```

**Returns**
- `uint32_t`: Elapsed time since boot (ms). This value rolls over to 0 approximately every 49.7 days.

**Example**
```c
static void ManageModeTransition(void)
{
    XmH10Mode_t currentSuitMode = XM.status.h10.h10Mode;

    switch (s_modeTransitionState) {
        case MODE_TRANSITION_IDLE:
            // Check whether a mode change has been detected.
            if (currentSuitMode != s_previousSuitMode) {
                
                // Transition from Active-Assist Mode to Standby Mode:
                // [CASE 1] Begin the safe-stop sequence for AA Mode, which was using P-Vectors during homing.
                if (s_previousSuitMode == XM_H10_MODE_ASSIST && currentSuitMode == XM_H10_MODE_STANDBY 
                    && s_aaGlobalState == AA_STATE_HOMING) {
                    XM_SendPVectorReset(SYS_NODE_ID_RH);   // Cancel P-Vector trajectory generation
                    XM_SendPVectorReset(SYS_NODE_ID_LH);
                    s_modeTransitionTimer = XM_GetTick();  // Start the reset delay timer
                    s_modeTransitionState = MODE_TRANSITION_STOP_PENDING; // Advance to next state
                }
        // Handle subsequent states
        ...
```

---

## Utility API

### `XM_CaptureLoopCountBase()`

```c
void XM_CaptureLoopCountBase(void);
```

Captures a baseline for the H10 Assist Loop Counter. Stores the value of `h10AssistModeLoopCnt` at the time of the call as the reference point (0). Call this at the start of a data logging session so that the saved count always begins from 0.

### `XM_GetRelativeLoopCount()`

```c
uint32_t XM_GetRelativeLoopCount(void);
```

Returns the loop count relative to the captured baseline. If `XM_CaptureLoopCountBase()` has not been called, returns the absolute value.

**Usage example:**
```c
void Active_Entry(void) {
    XM_CaptureLoopCountBase();  // Capture baseline
}

void Active_Loop(void) {
    uint32_t elapsed = XM_GetRelativeLoopCount();  // Relative count starting from 0
    myData.loopCnt = elapsed;
}
```

---

## Related Examples

| Example | Difficulty | Control Method |
|------|--------|----------|
| [08_CDC_Sensor_Print](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/08_CDC_Sensor_Print/) | Beginner | Reading sensor data (XM.status) |
| [11_Passive_Mode](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/11_Passive_Mode/) | Advanced | P-Vector + I-Vector trajectory control |
| [12_Active_Assist_Mode](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/12_Active_Assist_Mode/) | Advanced | Real-time torque control (SetAssistTorque) |
| [13_Resistive_Mode](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/13_Resistive_Mode/) | Intermediate | Compensation gain setting (SetResistiveCompGain) |
| [14_PD_Realtime_Control](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/14_PD_Realtime_Control/) | Intermediate | PD torque control |
| [15_Inverted_Pendulum_Control](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/15_Inverted_Pendulum_Control/) | Advanced | Model-based gravity compensation + PD |
| [17_FSM_Gait_Intent](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/17_FSM_Gait_Intent/) | Advanced | Gait-phase-based torque assist |

---

## ⚠️ Common Mistakes

| Symptom | Cause | Fix |
|------|------|------|
| All `XM.status.h10.*` fields are 0 | KIT H10 not connected, or CAN-FD HIGH/LOW pins swapped | Check the pinmap in [01-hardware-setup.md](../getting-started/01-hardware-setup.md) Figure 1 |
| `XM.status.h10.is_connected` is `false` | CAN-FD cable loose, or H10 body power is off | Verify KIT H10 24 V input and fully seat the connector |
| `SetAssistTorque` is called but torque remains 0 | `XM_SetControlMode(XM_CTRL_TORQUE)` was never called | Set the mode once on entering the active state |
| Torque commands are sent but H10 does not move | KIT H10 firmware < v2.3.0 (incompatible with XM v2.0.0 and later) | Update using the [kit-h10-firmware/](../kit-h10-firmware/) guide |
| Estimated data such as knee angle and forward velocity are always 0 | `XM_SendUserBodyData()` was never called (prerequisite for body-data-dependent fields) | See the Body Data instructions in [examples/README.md](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/README.md#part-5) |
| IPO cycle misalignment / missed ticks | Blocking call inside `Control_Loop` (e.g., `osDelay`) | Use `XM_GetTick()` with a non-blocking pattern ([Ex.08](https://github.com/AGR-EXO/Extension_Module/tree/Develop/examples/08_CDC_Sensor_Print/)) |
| Writing directly to `XM.command` has no effect | `XM.command` is a staging area — only `XM_Set*` functions set the dirty flag | Always use setter functions such as `XM_SetAssistTorque` |
