/**
 ******************************************************************************
 * @file    multilayer_transparent_ctrl.c
 * @author  HyundoKim
 * @brief   [시연용] 다층 제어 아키텍처: Transparent / Wall / Guide 모드 전환
 * @details
 * 3개의 제어 레이어를 조합하여 하나의 예제에서 극적으로 다른 물리적 느낌을
 * 실시간 전환합니다. 거치대 시연에 최적화.
 *
 * [제어 아키텍처]
 *   Layer 1 — Gravity Compensation (마찰보상은 SAM10이 수행)
 *     τ_grav = m·g·L_com · sin(θ)   (단순 진자 모델)
 *
 *   Layer 2 — Disturbance Observer (잔류 외란 보상, TRANSPARENT 전용)
 *     d_hat[k] = α_q·d_hat[k-1] + (1-α_q)·(τ_meas - τ_gravity)
 *
 *   Layer 3a — Impedance (WALL 전용)
 *     τ_imp = K·(θ_eq - θ) - B·θ̇   (LPF 속도 사용)
 *
 *   Layer 3b — Computed Torque + Virtual Target (GUIDE 전용)
 *     τ = J·θ̈_des + gravity,  θ̈_des = ωn²·e + 2ζωn·ė
 *
 * [3가지 모드]
 *   TRANSPARENT (BTN1): Layer1 + Layer2(DOB 토글) → 로봇이 사라진 느낌
 *   WALL        (BTN2): Layer1 + Layer3a → 벽처럼 안 움직임
 *   GUIDE       (BTN3): Layer1 + Layer3b → 밀 때 자유, 놓으면 부드럽게 정위치 복귀
 *
 * @see     Ohnishi (1996), Hogan (1985), Slotine & Li (1991)
 * @see     Ex.20 impedance_control.c, Ex.21 gravity_compensation.c,
 *          Ex.31 friction_comp_dob.c
 * @version 1.0
 * @date    Apr 02, 2026
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include <math.h>

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* --- 물리 상수 --- */
#define G_ACC                   9.81f

/* --- 링크 물성치 (CAD 실측, 거치대 시연용) ---
 * 거치대 기준: 골반 베이스 고정, 다리 링크만 구동
 * 질량 = 0.184 kg (좌우 동일, CAD 실측)
 * CoM 거리 = 126.4 mm (관절축→질량중심, CAD CoM X좌표)
 * 관절축 관성: J = Iyy(CoM) + m·d² = 0.001195 + 0.184×0.1264² ≈ 0.00414 kgm²
 * MGL_EFF = 0.184 × 9.81 × 0.1264 ≈ 0.228 Nm
 * NOTE: 마찰보상은 SAM10(MD)이 시스템식별 기반으로 이미 수행 — 이중보상 금지 */
#define LINK_MASS_KG            0.184f
#define LINK_LENGTH_M           0.2845f
#define L_COM_M                 0.1264f     /* CAD 실측 CoM 거리 (m) */
#define MGL_EFF                 (LINK_MASS_KG * G_ACC * L_COM_M)  /* ≈ 0.228 Nm */

/* --- 구동기 사양 ---
 * SAM10: 감속비 18.75:1, Kt=0.085 Nm/A
 * 관절 토크 = Kt × 감속비 × 전류 ≈ 1.594 × i [Nm]
 * 역구동성 < 0.3 Nm, 최대 18.3 Nm, 정격 10 Nm, 85 RPM */
#define GEAR_RATIO              18.75f
#define KT_MOTOR_NM_PER_A      0.085f
#define KT_JOINT_NM_PER_A      (KT_MOTOR_NM_PER_A * GEAR_RATIO)  /* ≈ 1.594 Nm/A */

/* --- 공통 제어 상수 --- */
#define PI_VALUE                3.14159265f
#define CONTROL_DT              0.001f      /* User_Loop = 1ms (1kHz) */

/* --- Alpha-Beta Tracker (전 모드 공통, LPF 대체) ---
 * 모델 의존성 제거: A21=0, B2=0 → 순수 측정 기반 속도 추정
 * 등속 가정: θ̈ ≈ 0 (predict), 측정 혁신으로 보정 (update)
 * α = L1·dt = 0.0225 → 위치 추적 (노이즈 필터링)
 * β = L2·dt = 0.248  → 속도 추정 (~40Hz 대역) */
#define KF_L1                   22.49f              /* θ 보정 게인 */
#define KF_L2                   248.00f             /* θ̇ 보정 게인 */

/* --- DOB Q-filter (Layer 2, TRANSPARENT 전용) ---
 * 공칭 모델 = 중력보상만 (마찰은 SAM10이 처리) */
#define DOB_CUTOFF_HZ           5.0f
#define DOB_ALPHA_Q             (1.0f - (2.0f * PI_VALUE * DOB_CUTOFF_HZ * CONTROL_DT))

/* --- WALL Impedance (Layer 3a) ---
 * K = 0.2 Nm/deg, B = 0.02 Nm·s/deg (과감쇠)
 * Kalman 속도 사용 → LPF 때보다 깨끗한 댐핑 */
#define IMP_K_WALL              0.2f        /* Nm/deg */
#define IMP_B_WALL              0.02f       /* Nm·s/deg */

/* --- GUIDE: LQR + Kalman + Virtual Target (Layer 3b) ---
 * LQR (Q=diag(500,1), R=1, 연속시간 Riccati 해):
 *   K = [22.0417, 1.0874] (rad, rad/s)
 *   폐루프: ωn=22.5 rad/s (3.58Hz), ζ=1.000 (정확한 임계감쇠)
 *   1° 오차 → 0.385 Nm (역구동성 즉시 극복)
 *
 * Virtual Target: 등속 궤적으로 평형점 복귀 */

/* --- Bilateral Coupling (Layer 3c, 양방향 텔레오퍼레이션) ---
 * R = Master, L = Slave (또는 반대)
 * 커플링: τ_R = Kc·(θ_L - θ_R) + Bc·(θ̇_L - θ̇_R)
 *         τ_L = Kc·(θ_R - θ_L) + Bc·(θ̇_R - θ̇_L)
 * → 한쪽을 움직이면 반대쪽이 따라오고, 반대쪽을 잡으면 이쪽에서 저항 느낌
 * Kc: 위치 커플링 강성,  Bc: 속도 커플링 감쇠 */
#define BILATERAL_KC            0.15f       /* Nm/deg — 위치 커플링 */
#define BILATERAL_BC            0.01f       /* Nm·s/deg — 속도 커플링 */

/* --- 안전 한계 --- */
#define MAX_TORQUE_NM           8.0f        /* 정격 10Nm 대비 보수적 (무부하 급동작 방지) */

/* --- 고관절 ROM (기구학 안전) --- */
#define ROM_FLEXION_DEG         120.0f      /* 최대 굴곡 (deg) */
#define ROM_EXTENSION_DEG       (-25.0f)    /* 최대 신전 (deg, 음수) */
#define ROM_MARGIN_DEG          10.0f       /* ROM 경계 감쇠 시작 여유 (deg) */

/* --- 각도 변환 --- */
#define DEG_TO_RAD(d)           ((d) * 0.017453292f)

/* --- USB 디버그 주기 --- */

/* --- Homing 설정 (Ex.12 패턴) --- */
#define HOMING_TARGET_DEG10     0       /* 홈 위치 (deg×10) — 0도 */
#define HOMING_SPEED_DPS        150     /* 호밍 속도 (deg/s) */
#define HOMING_ACCEL_S0         4       /* 초기 가속도 */
#define HOMING_ACCEL_SD         4       /* 감속도 */
#define HOMING_DELAY_MS         50      /* 안정화 지연 (ms) */

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** @brief Homing 절차 상태 */
typedef enum {
    HOMING_ENTRY,
    HOMING_SET_IMPEDANCE,
    HOMING_START_MOTION,
    HOMING_WAIT_DONE,
    HOMING_FINALIZE_DELAY,
    HOMING_FINALIZE_CLEANUP,
    HOMING_COMPLETE
} HomingState_t;

/** @brief 제어 모드 열거형 */
typedef enum {
    MODE_ZERO_IMPEDANCE     = 0,  /* Gravity + DOB → 임피던스 0 렌더링 */
    MODE_VIRTUAL_WALL       = 1,  /* Gravity + Impedance → 가상 강성 벽 */
    MODE_BILATERAL_COUPLING = 2,  /* R↔L 양방향 힘 반사 텔레오퍼레이션 */
    NUM_MODES               = 3
} ControlModeSelect_t;


/**
 *-----------------------------------------------------------
 * LIVE EXPRESSION DEBUG (전역 — IDE에서 실시간 관찰용)
 *-----------------------------------------------------------
 */
volatile struct {
    uint8_t  tsm_state;         /* 0=OFF, 1=STANDBY, 2=ACTIVE */
    uint8_t  homing_state;      /* HomingState_t (0~6) */
    uint8_t  homing_done;       /* 0/1 */
    uint8_t  ctrl_mode;         /* 0=TRANS, 1=WALL, 2=GUIDE */
    uint8_t  dob_active;        /* 0/1 */
    uint8_t  h10_mode;          /* 0=STANDBY, 1=ASSIST */
    uint8_t  cm_connected;      /* 0/1 */
    uint8_t  ctrl_mode_set;     /* 0=MONITOR, 1=TORQUE */
    float    ang_r;             /* 우측 모터 각도 (deg) */
    float    ang_l;             /* 좌측 모터 각도 (deg) */
    float    tau_total_r;       /* 우측 총 토크 (Nm) */
    float    tau_total_l;       /* 좌측 총 토크 (Nm) */
    uint32_t loop_cnt;          /* 루프 카운터 (증가 확인용) */
    uint8_t  ctrl_step;         /* _RunControl 체크포인트 (1~8) */
} g_ml_dbg;

/**
 *-----------------------------------------------------------
 * STATIC VARIABLES
 *-----------------------------------------------------------
 */

/* --- TSM --- */
static XmTsmHandle_t s_tsm;

/* --- 제어 모드 --- */
static ControlModeSelect_t s_mode = MODE_ZERO_IMPEDANCE;

/* --- DOB 상태 (Layer 2) --- */
static float s_d_hat_r    = 0.0f;
static float s_d_hat_l    = 0.0f;
static bool  s_dob_active = false;  /* 초기 OFF — BTN1 long press 토글 */

/* --- Impedance 평형 위치 (Layer 3) --- */
static float s_eq_r_deg   = 0.0f;
static float s_eq_l_deg   = 0.0f;


/* --- Kalman Filter 상태 (LPF 대체) --- */
static float s_kf_theta_r = 0.0f;      /* θ̂ 추정 (rad) */
static float s_kf_omega_r = 0.0f;      /* θ̂̇ 추정 (rad/s) */
static float s_kf_theta_l = 0.0f;
static float s_kf_omega_l = 0.0f;
static bool  s_kf_initialized = false;

/* --- 제어 출력 (디버그/스트리밍용) --- */
static float s_tau_grav_r  = 0.0f;
static float s_tau_dob_r   = 0.0f;
static float s_tau_imp_r   = 0.0f;
static float s_tau_total_r = 0.0f;
static float s_tau_total_l = 0.0f;

/* --- Homing --- */
static HomingState_t s_homing_state = HOMING_COMPLETE;
static uint32_t      s_homing_timer = 0;
static bool          s_homing_done  = false;


/**
 *-----------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

static void Off_Loop(void);
static void Standby_Loop(void);
static void Active_Entry(void);
static void Active_Loop(void);
static void Active_Exit(void);

static void  _RunHoming(void);
static float _ComputeGravity(float angle_rad);
static float _UpdateDob(float d_hat_prev, float tau_meas, float tau_model);
static float _ComputeImpedance(float angle_deg, float vel_dps,
                               float eq_deg, float kp, float bd);
static void  _RunControl(void);
static void  _HandleButtons(void);
static void  _UpdateLeds(void);
static float _Clamp(float v, float lo, float hi);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

void User_Setup(void)
{
    s_tsm = XM_TSM_Create(XM_STATE_OFF);

    XmStateConfig_t off_conf = {
        .id      = XM_STATE_OFF,
        .on_loop = Off_Loop
    };
    XM_TSM_AddState(s_tsm, &off_conf);

    XmStateConfig_t sb_conf = {
        .id      = XM_STATE_STANDBY,
        .on_loop = Standby_Loop
    };
    XM_TSM_AddState(s_tsm, &sb_conf);

    XmStateConfig_t act_conf = {
        .id       = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    /* USB 스트리밍: 7채널 (0xF0) — 마찰보상은 SAM10이 수행하므로 제외 */
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"Gravity\",\"unit\":\"Nm\"},"
        "{\"name\":\"DOB\",\"unit\":\"Nm\"},"
        "{\"name\":\"Impedance\",\"unit\":\"Nm\"},"
        "{\"name\":\"Total\",\"unit\":\"Nm\"},"
        "{\"name\":\"Angle\",\"unit\":\"deg\"},"
        "{\"name\":\"Velocity\",\"unit\":\"deg/s\"},"
        "{\"name\":\"Mode\",\"unit\":\"-\"}]");

    XM_SetControlMode(XM_CTRL_MONITOR);
}

void User_Loop(void)
{
    if (!XM_IsCmConnected()) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_OFF);
    }
    XM_TSM_Run(s_tsm);

    /* --- Live Expression 갱신 --- */
    g_ml_dbg.cm_connected  = XM_IsCmConnected() ? 1 : 0;
    g_ml_dbg.h10_mode      = (uint8_t)XM.status.h10.h10Mode;
    g_ml_dbg.homing_state  = (uint8_t)s_homing_state;
    g_ml_dbg.homing_done   = s_homing_done ? 1 : 0;
    g_ml_dbg.ctrl_mode     = (uint8_t)s_mode;
    g_ml_dbg.dob_active    = s_dob_active ? 1 : 0;
    g_ml_dbg.ang_r         = XM.status.h10.rightHipMotorAngle;
    g_ml_dbg.ang_l         = XM.status.h10.leftHipMotorAngle;
    g_ml_dbg.tau_total_r   = s_tau_total_r;
    g_ml_dbg.tau_total_l   = s_tau_total_l;
    g_ml_dbg.loop_cnt++;
}

/**
 *-----------------------------------------------------------
 * TSM CALLBACKS
 *-----------------------------------------------------------
 */

static void Off_Loop(void)
{
    g_ml_dbg.tsm_state = 0;
    if (XM_IsCmConnected()) {
        /* USB 디버그 제거 (크래시 원인) */
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

static void Standby_Loop(void)
{
    g_ml_dbg.tsm_state = 1;
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

static void Active_Entry(void)
{
    g_ml_dbg.tsm_state = 2;
    g_ml_dbg.ctrl_mode_set = 1;
    XM_SetControlMode(XM_CTRL_TORQUE);

    /* === Homing 절차 시작 (Ex.12 패턴) === */
    s_homing_state = HOMING_ENTRY;
    s_homing_done  = false;

    /* 초기 모드: TRANSPARENT */
    s_mode = MODE_ZERO_IMPEDANCE;

    /* DOB 초기화 */
    s_d_hat_r = 0.0f;
    s_d_hat_l = 0.0f;

    /* Impedance 평형 = 현재 각도 */
    s_eq_r_deg = XM.status.h10.rightHipMotorAngle;
    s_eq_l_deg = XM.status.h10.leftHipMotorAngle;

    /* Kalman Filter 초기화 */
    s_kf_theta_r = DEG_TO_RAD(s_eq_r_deg);
    s_kf_theta_l = DEG_TO_RAD(s_eq_l_deg);
    s_kf_omega_r = 0.0f;
    s_kf_omega_l = 0.0f;
    s_kf_initialized = true;

    /* 제어 변수 초기화 */
    s_tau_grav_r  = 0.0f;
    s_tau_dob_r   = 0.0f;
    s_tau_imp_r   = 0.0f;
    s_tau_total_r = 0.0f;


    _UpdateLeds();
}

static void Active_Loop(void)
{
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    /* 호밍 완료 전에는 호밍 FSM만 실행 */
    if (!s_homing_done) {
        _RunHoming();
        return;
    }

    /* === 크래시 추적: 단계별 활성화 ===
     * Step 1: 아래 전부 주석 → loop_cnt 계속 증가하면 제어 코드가 원인
     * Step 2: _HandleButtons만 활성화
     * Step 3: _RunControl도 활성화 */
    _HandleButtons();
    _RunControl();
}

static void Active_Exit(void)
{
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);

    /* 호밍 중 종료 시 P-Vector 취소 + I-Vector 해제 */
    if (!s_homing_done) {
        XM_SendPVectorReset(SYS_NODE_ID_RH);
        XM_SendPVectorReset(SYS_NODE_ID_LH);
    }
    IVector_t zeroImp = { .epsilon = 0, .kp = 0, .kd = 0, .lambda = 0, .duration = 50 };
    XM_SendIVector(SYS_NODE_ID_RH, &zeroImp);
    XM_SendIVector(SYS_NODE_ID_LH, &zeroImp);

    XM_SetControlMode(XM_CTRL_MONITOR);

    s_d_hat_r    = 0.0f;
    s_d_hat_l    = 0.0f;
    s_homing_done = false;
    s_kf_initialized = false;

    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

}

/**
 *-----------------------------------------------------------
 * CONTROL CORE
 *-----------------------------------------------------------
 */

/**
 * @brief Homing FSM — 0도로 이동 후 임피던스 해제 (Ex.12 패턴)
 */
static void _RunHoming(void)
{
    switch (s_homing_state) {
    case HOMING_ENTRY:
        /* SAM10 임피던스 Kp/Kd 최대값 설정 */
        XM_SendIVectorKpKdMax(SYS_NODE_ID_RH, 6, 1);
        XM_SendIVectorKpKdMax(SYS_NODE_ID_LH, 6, 1);
        s_homing_state = HOMING_SET_IMPEDANCE;
        break;

    case HOMING_SET_IMPEDANCE: {
        /* 호밍용 강한 임피던스 설정 */
        IVector_t stiff = { .epsilon = 0, .kp = 80, .kd = 1, .lambda = 0, .duration = 50 };
        XM_SendIVector(SYS_NODE_ID_RH, &stiff);
        XM_SendIVector(SYS_NODE_ID_LH, &stiff);
        s_homing_state = HOMING_START_MOTION;
        break;
    }

    case HOMING_START_MOTION: {
        /* 현재 위치 → 0도로 P-Vector 궤적 전송 */
        int16_t cur_r = (int16_t)roundf(XM.status.h10.rightHipMotorAngle * 10.0f);
        int16_t cur_l = (int16_t)roundf(XM.status.h10.leftHipMotorAngle * 10.0f);
        int16_t dist_r = abs(HOMING_TARGET_DEG10 - cur_r);
        int16_t dist_l = abs(HOMING_TARGET_DEG10 - cur_l);
        uint16_t dur_r = (uint16_t)(((float)dist_r / (float)HOMING_SPEED_DPS) * 1000.0f);
        uint16_t dur_l = (uint16_t)(((float)dist_l / (float)HOMING_SPEED_DPS) * 1000.0f);
        if (dur_r < 100) dur_r = 100;  /* 최소 100ms */
        if (dur_l < 100) dur_l = 100;

        PVector_t pv_r = { .yd = HOMING_TARGET_DEG10, .L = dur_r, .s0 = HOMING_ACCEL_S0, .sd = HOMING_ACCEL_SD };
        PVector_t pv_l = { .yd = HOMING_TARGET_DEG10, .L = dur_l, .s0 = HOMING_ACCEL_S0, .sd = HOMING_ACCEL_SD };
        XM_SendPVector(SYS_NODE_ID_RH, &pv_r);
        XM_SendPVector(SYS_NODE_ID_LH, &pv_l);
        XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
        XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);
        s_homing_state = HOMING_WAIT_DONE;
        break;
    }

    case HOMING_WAIT_DONE:
        if (XM.status.h10.isPVectorRHDone && XM.status.h10.isPVectorLHDone) {
            XM_ClearPVectorDoneFlag(SYS_NODE_ID_RH);
            XM_ClearPVectorDoneFlag(SYS_NODE_ID_LH);
            s_homing_timer = XM_GetTick();
            s_homing_state = HOMING_FINALIZE_DELAY;
        }
        break;

    case HOMING_FINALIZE_DELAY:
        if ((XM_GetTick() - s_homing_timer) >= HOMING_DELAY_MS) {
            /* 임피던스 해제 → 관절 자유 */
            IVector_t zero = { .epsilon = 0, .kp = 0, .kd = 0, .lambda = 0, .duration = 50 };
            XM_SendIVector(SYS_NODE_ID_RH, &zero);
            XM_SendIVector(SYS_NODE_ID_LH, &zero);
            s_homing_state = HOMING_FINALIZE_CLEANUP;
        }
        break;

    case HOMING_FINALIZE_CLEANUP:
        /* 호밍 완료 → 평형점 캡처 + Kalman 재초기화 */
        s_eq_r_deg = XM.status.h10.rightHipMotorAngle;
        s_eq_l_deg = XM.status.h10.leftHipMotorAngle;
        s_kf_theta_r = DEG_TO_RAD(s_eq_r_deg);
        s_kf_theta_l = DEG_TO_RAD(s_eq_l_deg);
        s_kf_omega_r = 0.0f;
        s_kf_omega_l = 0.0f;
        s_homing_done = true;
        s_homing_state = HOMING_COMPLETE;
        break;

    case HOMING_COMPLETE:
        break;
    }
}

static void _RunControl(void)
{
    g_ml_dbg.ctrl_step = 1;
    /* === 1. 센서 데이터 획득 === */
    float ang_r_deg = XM.status.h10.rightHipMotorAngle;
    float ang_l_deg = XM.status.h10.leftHipMotorAngle;
    float ang_r_rad = DEG_TO_RAD(ang_r_deg);
    float ang_l_rad = DEG_TO_RAD(ang_l_deg);

    float cur_r_a = XM.status.h10.rightHipTorque;  /* 실제 전류 (A) */
    float cur_l_a = XM.status.h10.leftHipTorque;

    g_ml_dbg.ctrl_step = 2;
    /* === 2. Alpha-Beta Tracker — 모델프리 속도 추정 ===
     * Predict: 등속 가정 (θ̈ ≈ 0)
     * Update:  측정 혁신으로 위치+속도 보정 */
    {
        /* --- Right --- */
        float theta_pred_r = s_kf_theta_r + s_kf_omega_r * CONTROL_DT;
        float omega_pred_r = s_kf_omega_r;  /* 등속 가정 */
        float y_err_r = ang_r_rad - theta_pred_r;
        s_kf_theta_r = theta_pred_r + KF_L1 * CONTROL_DT * y_err_r;
        s_kf_omega_r = omega_pred_r + KF_L2 * CONTROL_DT * y_err_r;

        /* --- Left --- */
        float theta_pred_l = s_kf_theta_l + s_kf_omega_l * CONTROL_DT;
        float omega_pred_l = s_kf_omega_l;
        float y_err_l = ang_l_rad - theta_pred_l;
        s_kf_theta_l = theta_pred_l + KF_L1 * CONTROL_DT * y_err_l;
        s_kf_omega_l = omega_pred_l + KF_L2 * CONTROL_DT * y_err_l;
    }

    /* Kalman 추정 속도 (rad/s → deg/s 변환) */
    float vel_r_dps = s_kf_omega_r * (180.0f / PI_VALUE);
    float vel_l_dps = s_kf_omega_l * (180.0f / PI_VALUE);

    g_ml_dbg.ctrl_step = 3;
    /* === 3. Layer 1 — Gravity Compensation (마찰보상은 SAM10이 수행) === */
    float tau_grav_r = _ComputeGravity(ang_r_rad);
    float tau_grav_l = _ComputeGravity(ang_l_rad);

    g_ml_dbg.ctrl_step = 4;
    /* === 4. Layer 2 — DOB (TRANSPARENT 모드에서만) === */
    float tau_dob_r = 0.0f;
    float tau_dob_l = 0.0f;

    if (s_mode == MODE_ZERO_IMPEDANCE && s_dob_active) {
        /* DOB 공칭모델 = 중력만 (마찰은 SAM10이 이미 보상) */
        float tau_model_r = tau_grav_r;
        float tau_model_l = tau_grav_l;
        float tau_meas_r  = KT_JOINT_NM_PER_A * cur_r_a;
        float tau_meas_l  = KT_JOINT_NM_PER_A * cur_l_a;

        s_d_hat_r = _UpdateDob(s_d_hat_r, tau_meas_r, tau_model_r);
        s_d_hat_l = _UpdateDob(s_d_hat_l, tau_meas_l, tau_model_l);

        tau_dob_r = s_d_hat_r;
        tau_dob_l = s_d_hat_l;
    } else if (s_mode != MODE_ZERO_IMPEDANCE || !s_dob_active) {
        /* DOB 비활성 시 추정치 리셋 */
        s_d_hat_r = 0.0f;
        s_d_hat_l = 0.0f;
    }

    g_ml_dbg.ctrl_step = 5;
    /* === 5. Layer 3 — Impedance (WALL / GUIDE 모드에서만) === */
    float tau_imp_r = 0.0f;
    float tau_imp_l = 0.0f;

    if (s_mode == MODE_VIRTUAL_WALL) {
        tau_imp_r = _ComputeImpedance(ang_r_deg, vel_r_dps,
                                      s_eq_r_deg, IMP_K_WALL, IMP_B_WALL);
        tau_imp_l = _ComputeImpedance(ang_l_deg, vel_l_dps,
                                      s_eq_l_deg, IMP_K_WALL, IMP_B_WALL);
    } else if (s_mode == MODE_BILATERAL_COUPLING) {
        /* --- Bilateral Coupling: R ↔ L 양방향 힘 반사 ---
         * τ_R = Kc·(θ_L - θ_R) + Bc·(θ̇_L - θ̇_R)  → R을 L 쪽으로 당김
         * τ_L = Kc·(θ_R - θ_L) + Bc·(θ̇_R - θ̇_L)  → L을 R 쪽으로 당김
         * 한쪽을 움직이면 반대쪽이 따라옴 (작용-반작용) */
        float pos_diff_deg = ang_l_deg - ang_r_deg;     /* L-R */
        float vel_diff_dps = vel_l_dps - vel_r_dps;     /* L̇-Ṙ */

        tau_imp_r = BILATERAL_KC * pos_diff_deg + BILATERAL_BC * vel_diff_dps;  /* +: R→L방향 */
        tau_imp_l = -tau_imp_r;                                                  /* 반작용 */
    }

    g_ml_dbg.ctrl_step = 6;
    /* === 6. 레이어 합산 + 포화 ===
     * ZERO_IMP:  gravity ± DOB
     * V_WALL:    gravity + K·err - B·vel
     * BILATERAL: gravity + Kc·(θ_other - θ_self) */
    float total_r = tau_grav_r + tau_dob_r + tau_imp_r;
    float total_l = tau_grav_l + tau_dob_l + tau_imp_l;

    total_r = _Clamp(total_r, -MAX_TORQUE_NM, MAX_TORQUE_NM);
    total_l = _Clamp(total_l, -MAX_TORQUE_NM, MAX_TORQUE_NM);

    XM_SetAssistTorqueRH(total_r);
    XM_SetAssistTorqueLH(total_l);

    g_ml_dbg.ctrl_step = 7;
    /* === 7. 디버그용 저장 === */
    s_tau_grav_r  = tau_grav_r;
    s_tau_dob_r   = tau_dob_r;
    s_tau_imp_r   = tau_imp_r;
    s_tau_total_r = total_r;
    s_tau_total_l = total_l;
}

/**
 *-----------------------------------------------------------
 * LAYER FUNCTIONS
 *-----------------------------------------------------------
 */

/** @brief Layer 1a — 중력 보상 토크 */
static float _ComputeGravity(float angle_rad)
{
    return MGL_EFF * sinf(angle_rad);
}

/** @brief Layer 2 — DOB Q-filter 업데이트 */
static float _UpdateDob(float d_hat_prev, float tau_meas, float tau_model)
{
    float residual = tau_meas - tau_model;
    return DOB_ALPHA_Q * d_hat_prev + (1.0f - DOB_ALPHA_Q) * residual;
}

/** @brief Layer 3a — WALL 임피던스 토크 (스프링 + 댐퍼) */
static float _ComputeImpedance(float angle_deg, float vel_dps,
                               float eq_deg, float kp, float bd)
{
    float pos_error = eq_deg - angle_deg;
    return kp * pos_error - bd * vel_dps;
}

/**
 *-----------------------------------------------------------
 * BUTTON / LED / USB
 *-----------------------------------------------------------
 */

static void _HandleButtons(void)
{
    XmBtnEvent_t btn1 = XM_GetButtonEvent(XM_BTN_1);
    XmBtnEvent_t btn2 = XM_GetButtonEvent(XM_BTN_2);
    XmBtnEvent_t btn3 = XM_GetButtonEvent(XM_BTN_3);

    if (btn1 == XM_BTN_CLICK) {
        s_mode = MODE_ZERO_IMPEDANCE;
        s_dob_active = false;
        s_d_hat_r = 0.0f;
        s_d_hat_l = 0.0f;
        _UpdateLeds();
    }
    if (btn1 == XM_BTN_LONG_PRESS && s_mode == MODE_ZERO_IMPEDANCE) {
        s_dob_active = !s_dob_active;
        s_d_hat_r = 0.0f;
        s_d_hat_l = 0.0f;
        _UpdateLeds();
    }
    if (btn2 == XM_BTN_CLICK) {
        s_mode = MODE_VIRTUAL_WALL;
        s_dob_active = false;      /* 모드 전환 시 DOB 자동 해제 */
        s_d_hat_r = 0.0f;
        s_d_hat_l = 0.0f;
        s_eq_r_deg = XM.status.h10.rightHipMotorAngle;
        s_eq_l_deg = XM.status.h10.leftHipMotorAngle;
        _UpdateLeds();
    }
    if (btn3 == XM_BTN_CLICK) {
        s_mode = MODE_BILATERAL_COUPLING;
        s_dob_active = false;
        s_d_hat_r = 0.0f;
        s_d_hat_l = 0.0f;
        _UpdateLeds();
    }
}

static void _UpdateLeds(void)
{
    /* LED1: TRANSPARENT(heartbeat) / WALL(solid) / GUIDE(blink) */
    switch (s_mode) {
    case MODE_ZERO_IMPEDANCE:
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_SetLedState(XM_LED_2, XM_OFF);
        XM_SetLedState(XM_LED_3, s_dob_active ? XM_ON : XM_OFF); /* LED3 = DOB 상태 */
        break;
    case MODE_VIRTUAL_WALL:
        XM_SetLedState(XM_LED_1, XM_ON);
        XM_SetLedState(XM_LED_2, XM_ON);
        XM_SetLedState(XM_LED_3, XM_OFF);
        break;
    case MODE_BILATERAL_COUPLING:
        XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);  /* 빠른 점멸 = 양방향 */
        XM_SetLedState(XM_LED_2, XM_ON);
        XM_SetLedState(XM_LED_3, XM_ON);
        break;
    default:
        break;
    }
}

/**
 *-----------------------------------------------------------
 * UTILITIES
 *-----------------------------------------------------------
 */

static float _Clamp(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

