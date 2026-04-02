/**
 ******************************************************************************
 * @file    ondevice_kinesthetic_learning.c
 * @author  HyundoKim
 * @brief   [시연용] On-Device Kinesthetic Learning — MCU 위 실시간 NN 학습
 * @details
 * 사람이 로봇을 잡고 움직인 궤적을 MCU에서 Tiny Neural Network로 학습한 뒤,
 * 학습된 NN 출력 + Multi-Layer 제어로 정밀하게 재현합니다.
 *
 * [전체 흐름]
 *   IDLE → [BTN1 click]      → TEACH (투명모드, 궤적기록)
 *        → [BTN1 click]      → LEARN (NN 학습, ~100ms)
 *        → 자동               → REPLAY (NN + 다층 제어로 재현)
 *        → [BTN1 click]      → IDLE (종료)
 *   [BTN1 long]: REPLAY 중 NN 가중치를 NV Flash에 저장
 *   [BTN2 click]: IDLE에서 마지막 학습(RAM 또는 Flash)으로 즉시 REPLAY
 *   [BTN3 click]: 리셋 (버퍼 클리어, NN 초기화)
 *
 * [NN 사양]
 *   Architecture: 3 input (phase, sin(2πφ), cos(2πφ)) → 16 hidden (ReLU) → 2 output (θ_R, θ_L)
 *   Parameters:   3×16 + 16 + 16×2 + 2 = 114개 (456B)
 *   Training:     SGD with LR decay, MSE loss
 *   Inference:    < 2μs @ 480MHz
 *
 * [제어 — Multi-Layer + LQR 최적 게인]
 *   TEACH:  Gravity Comp (적응 MGL 추정) → 투명 느낌
 *   REPLAY: Gravity Comp + DOB + LQR-PD + NN Feedforward → 정밀 재현
 *
 * @see     Ex.35 multilayer_transparent_ctrl.c (다층 제어)
 * @version 3.0
 * @date    Apr 03, 2026
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include <math.h>
#include <string.h>

/**
 *-----------------------------------------------------------
 * DEFINITIONS
 *-----------------------------------------------------------
 */

/* --- 교시 기록 --- */
#define MAX_TEACH_POINTS        2000    /* 최대 교시 샘플 수 (100Hz × 20s) */
#define TEACH_DOWNSAMPLE        10      /* 1kHz → 100Hz */

/* --- NN 아키텍처 (Phase Encoding: phase, sin, cos) --- */
#define NN_INPUT                3       /* phase, sin(2πφ), cos(2πφ) */
#define NN_HIDDEN               16
#define NN_OUTPUT               2       /* θ_R, θ_L */
#define NN_TOTAL_PARAMS         114     /* 3×16 + 16 + 16×2 + 2 */

/* --- NN 학습 --- */
#define NN_LR_INIT              0.001f
#define NN_LR_END               0.0001f
#define NN_EPOCHS               5000    /* batch 줄인 만큼 epoch 보상 (5초) */
#define NN_BATCH_SIZE           20      /* 1tick에 20샘플만 — PnP heartbeat 보장 */

/* --- 물리 상수 (거치대 시연용) ---
 * NOTE: MGL_EFF_INIT는 초기값, TEACH 중 적응 추정으로 갱신됨
 * 마찰보상은 SAM10(MD)이 시스템식별 기반으로 이미 수행 — 이중보상 금지 */
#define G_ACC                   9.81f
#define LINK_MASS_KG            0.184f
#define LINK_LENGTH_M           0.2845f
#define L_COM_M                 (LINK_LENGTH_M * 0.5f)
#define MGL_EFF_INIT            (LINK_MASS_KG * G_ACC * L_COM_M)  /* ≈ 0.257 Nm */
#define MGL_ADAPT_RATE          0.001f  /* 적응 추정 학습률 */

/* --- 구동기 사양 (Ex.35 기반) --- */
#define GEAR_RATIO              18.75f
#define KT_MOTOR_NM_PER_A      0.085f
#define KT_JOINT_NM_PER_A      (KT_MOTOR_NM_PER_A * GEAR_RATIO)

/* --- DOB Q-filter (Ex.35 기반) --- */
#define DOB_CUTOFF_HZ           5.0f
#define TWO_PI                  6.28318530f
#define CONTROL_DT              0.001f
#define DOB_ALPHA_Q             (1.0f - (TWO_PI * DOB_CUTOFF_HZ * CONTROL_DT))

/* --- LQR 최적 게인 유도 ---
 * 중력보상 후 잔여 동역학: J_total·θ̈ = τ  (선형화)
 *
 * J_total = J_link + J_motor_reflected
 *   J_link  = m·L_com² = 0.184 × 0.14225² ≈ 0.003724 kg·m²
 *   J_motor ≈ 33e-7 kg·m² (typical 45mm BLDC)
 *   J_motor_reflected = J_motor × GR² = 33e-7 × 18.75² ≈ 0.00116 kg·m²
 *   J_total ≈ 0.003724 + 0.00116 ≈ 0.00488 kg·m²
 *
 * 설계 목표: ω_n ≈ 15 rad/s (반응시간 ~0.2s), ζ ≈ 0.8 (약간 과감쇠)
 *   Kp = J_total · ω_n² = 0.00488 × 225 = 1.098 Nm/rad → 0.019 Nm/deg
 *   Kd = 2·ζ·J_total·ω_n = 2 × 0.8 × 0.00488 × 15 = 0.117 Nm·s/rad → 0.00204 Nm·s/deg */
#define J_MOTOR_REFLECTED       (33e-7f * GEAR_RATIO * GEAR_RATIO)  /* kg·m² */
#define J_LINK                  (LINK_MASS_KG * L_COM_M * L_COM_M)  /* kg·m² */
#define J_TOTAL                 (J_LINK + J_MOTOR_REFLECTED)         /* ≈ 0.00488 kg·m² */
#define LQR_WN                  15.0f   /* 자연 주파수 (rad/s) */
#define LQR_ZETA                0.8f    /* 댐핑비 */
#define KP_LQR                  (J_TOTAL * LQR_WN * LQR_WN * 0.017453292f)  /* Nm/deg */
#define KD_LQR                  (2.0f * LQR_ZETA * J_TOTAL * LQR_WN * 0.017453292f) /* Nm·s/deg */

/* --- NN Feedforward --- */
#define FF_GAIN                 0.0f    /* 0=비활성, 안정화 확인 후 0.3~0.5로 올리기 */

/* --- Alpha-Beta 속도 추정기 --- */
#define AB_ALPHA                0.2f
#define AB_BETA                 0.01f

/* --- 안전 --- */
#define MAX_TORQUE_NM           8.0f

/* --- NV Flash 저장 --- */
#define NV_MAGIC                0xCA5E0A01
#define NV_OFFSET_MAGIC         0
#define NV_OFFSET_NN            4
#define NV_OFFSET_MGL           (NV_OFFSET_NN + sizeof(TinyNN_t))
#define NV_OFFSET_COUNT         (NV_OFFSET_MGL + sizeof(float))

/* --- 유틸 --- */
#define DEG_TO_RAD(d)           ((d) * 0.017453292f)

/**
 *-----------------------------------------------------------
 * TYPES
 *-----------------------------------------------------------
 */

typedef struct {
    float theta_r;
    float theta_l;
} TeachPoint_t;

typedef enum {
    PHASE_IDLE     = 0,
    PHASE_TEACH    = 1,
    PHASE_LEARN    = 2,
    PHASE_REPLAY   = 3
} LearnPhase_t;

typedef struct {
    float w1[NN_HIDDEN][NN_INPUT];
    float b1[NN_HIDDEN];
    float w2[NN_OUTPUT][NN_HIDDEN];
    float b2[NN_OUTPUT];
} TinyNN_t;

/**
 *-----------------------------------------------------------
 * PUBLIC DEBUG VARIABLES (디버거/CDC에서 관찰 가능)
 *-----------------------------------------------------------
 */

/** @brief 외부에서 현재 상태를 확인할 수 있는 디버그 변수들 */
volatile uint8_t  g_kl_phase       = 0;  /* 0=IDLE, 1=TEACH, 2=LEARN, 3=REPLAY */
volatile uint32_t g_kl_teach_count = 0;  /* 교시 샘플 수 */
volatile float    g_kl_loss        = 0.0f; /* 마지막 학습 loss */
volatile uint32_t g_kl_epoch       = 0;  /* 현재/마지막 epoch */
volatile float    g_kl_mgl_eff     = 0.0f; /* 적응 추정 MGL */

/**
 *-----------------------------------------------------------
 * STATIC VARIABLES
 *-----------------------------------------------------------
 */

static XmTsmHandle_t s_tsm;
static LearnPhase_t  s_phase = PHASE_IDLE;

/* 교시 버퍼 */
static TeachPoint_t  s_teach_buf[MAX_TEACH_POINTS];
static uint32_t      s_teach_count = 0;
static uint32_t      s_rec_downcnt = 0;

/* 교시 범위 (NN 출력 클램핑용) */
static float         s_teach_min_r = 0.0f, s_teach_max_r = 0.0f;
static float         s_teach_min_l = 0.0f, s_teach_max_l = 0.0f;

/* NN */
static TinyNN_t      s_nn;
static float         s_hidden_out[NN_HIDDEN];
static float         s_hidden_grad[NN_HIDDEN];
static float         s_last_loss   = 0.0f;
static bool          s_nn_trained  = false;

/* 리플레이 */
static uint32_t      s_replay_idx  = 0;
static uint32_t      s_replay_tick = 0;
static int8_t        s_replay_dir  = 1;
static float         s_prev_target_r = 0.0f;  /* NN feedforward용 이전 목표 */
static float         s_prev_target_l = 0.0f;

/* Alpha-Beta */
static float         s_ab_pos_r = 0.0f, s_ab_vel_r = 0.0f;
static float         s_ab_pos_l = 0.0f, s_ab_vel_l = 0.0f;
static bool          s_ab_init  = false;

/* DOB */
static float         s_d_hat_r = 0.0f;
static float         s_d_hat_l = 0.0f;

/* 적응 중력보상 */
static float         s_mgl_eff = MGL_EFF_INIT;

/* 학습 진행 (백그라운드 태스크와 공유 → volatile) */
static volatile uint32_t s_learn_epoch = 0;
static volatile float    s_learn_loss  = 0.0f;
static volatile bool     s_learn_done  = false;
static XmBgTaskHandle_t  s_train_handle = NULL;

/**
 *-----------------------------------------------------------
 * STATIC PROTOTYPES
 *-----------------------------------------------------------
 */

static void Off_Loop(void);
static void Standby_Loop(void);
static void Active_Entry(void);
static void Active_Loop(void);
static void Active_Exit(void);

static void _Idle_Loop(void);
static void _Teach_Loop(void);
static void _Learn_Loop(void);
static void _Replay_Loop(void);

static void  _NN_Init(void);
static void  _NN_Forward(float phase, float* out_r, float* out_l);
static float _NN_TrainEpoch(uint32_t current_epoch);
static float _ReLU(float x);

static float _ComputeGravity(float angle_rad);
static float _UpdateDob(float d_hat_prev, float tau_meas, float tau_model);
static void  _ApplyTransparent(void);

static void  _HandleButtons(void);
static void  _UpdateLeds(void);
static float _Clamp(float v, float lo, float hi);

static bool  _NV_Save(void);
static bool  _NV_Load(void);

/* Simple pseudo-random */
static uint32_t s_rng_state = 12345;
static uint32_t _Rand(void) {
    s_rng_state = s_rng_state * 1103515245 + 12345;
    return (s_rng_state >> 16) & 0x7FFF;
}

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

void User_Setup(void)
{
    s_tsm = XM_TSM_Create(XM_STATE_OFF);

    XmStateConfig_t off_conf = { .id = XM_STATE_OFF, .on_loop = Off_Loop };
    XM_TSM_AddState(s_tsm, &off_conf);

    XmStateConfig_t sb_conf = { .id = XM_STATE_STANDBY, .on_loop = Standby_Loop };
    XM_TSM_AddState(s_tsm, &sb_conf);

    XmStateConfig_t act_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    XM_SetControlMode(XM_CTRL_MONITOR);
}

void User_Loop(void)
{
    if (!XM_IsCmConnected()) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_OFF);
    }
    XM_TSM_Run(s_tsm);
}

/**
 *-----------------------------------------------------------
 * TSM CALLBACKS
 *-----------------------------------------------------------
 */

static void Off_Loop(void)
{
    if (XM_IsCmConnected()) {
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

static void Active_Entry(void)
{
    XM_SetControlMode(XM_CTRL_TORQUE);
    s_phase = PHASE_IDLE;
    s_teach_count = 0;
    s_nn_trained = false;
    s_mgl_eff = MGL_EFF_INIT;
    _NN_Init();

    /* Flash에서 이전 학습 결과 복원 시도 */
    if (_NV_Load()) {
        s_nn_trained = true;
    }

    s_ab_pos_r = XM.status.h10.rightHipMotorAngle;
    s_ab_vel_r = 0.0f;
    s_ab_pos_l = XM.status.h10.leftHipMotorAngle;
    s_ab_vel_l = 0.0f;
    s_ab_init  = false;
    s_d_hat_r  = 0.0f;
    s_d_hat_l  = 0.0f;

    _UpdateLeds();
}

static uint32_t s_mode_lost_tick = 0;
#define MODE_LOST_TIMEOUT_MS    500  /* 500ms 연속 비정상이어야 종료 */

static void Active_Loop(void)
{
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        if (s_mode_lost_tick == 0) {
            s_mode_lost_tick = XM_GetTick();
        } else if ((XM_GetTick() - s_mode_lost_tick) >= MODE_LOST_TIMEOUT_MS) {
            s_mode_lost_tick = 0;
            XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
            return;
        }
        return;  /* 타임아웃 전까지는 루프 유지 (토크 출력 계속) */
    } else {
        s_mode_lost_tick = 0;  /* ASSIST 복귀 → 타이머 리셋 */
    }

    _HandleButtons();

    switch (s_phase) {
    case PHASE_IDLE:    _Idle_Loop();   break;
    case PHASE_TEACH:   _Teach_Loop();  break;
    case PHASE_LEARN:   _Learn_Loop();  break;
    case PHASE_REPLAY:  _Replay_Loop(); break;
    }

    /* 디버그 변수 동기화 */
    g_kl_phase       = (uint8_t)s_phase;
    g_kl_teach_count = s_teach_count;
    g_kl_loss        = s_last_loss;
    g_kl_epoch       = s_learn_epoch;
    g_kl_mgl_eff     = s_mgl_eff;
}

static void Active_Exit(void)
{
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
    XM_SetControlMode(XM_CTRL_MONITOR);
    s_phase = PHASE_IDLE;
    s_d_hat_r = 0.0f;
    s_d_hat_l = 0.0f;
    s_ab_init = false;

    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);
}

/**
 *-----------------------------------------------------------
 * PHASE HANDLERS
 *-----------------------------------------------------------
 */

static void _Idle_Loop(void)
{
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
}

/** @brief TEACH — 적응 중력보상 투명 모드 + 궤적 기록 */
static void _Teach_Loop(void)
{
    _ApplyTransparent();

    if (++s_rec_downcnt >= TEACH_DOWNSAMPLE) {
        s_rec_downcnt = 0;
        if (s_teach_count < MAX_TEACH_POINTS) {
            s_teach_buf[s_teach_count].theta_r = XM.status.h10.rightHipMotorAngle;
            s_teach_buf[s_teach_count].theta_l = XM.status.h10.leftHipMotorAngle;
            s_teach_count++;
        }
        if (s_teach_count >= MAX_TEACH_POINTS) {
            s_phase = PHASE_LEARN;
            s_learn_epoch = 0;
            /* s_learn_tick 제거 — 백그라운드 학습에서 불필요 */
            _NN_Init();
            _UpdateLeds();
        }
    }
}

/** @brief 백그라운드 태스크에서 실행되는 학습 함수 */
static void _BgTrainFunc(void* arg)
{
    (void)arg;
    for (uint32_t epoch = 0; epoch < NN_EPOCHS; epoch++) {
        s_learn_loss = _NN_TrainEpoch(epoch);
        s_learn_epoch = epoch + 1;
    }
    s_learn_done = true;
}

/** @brief LEARN — 백그라운드 학습 + 투명 제어 유지 */
static void _Learn_Loop(void)
{
    _ApplyTransparent();

    if (s_teach_count < 10) {
        s_phase = PHASE_IDLE;
        _UpdateLeds();
        return;
    }

    /* 백그라운드 학습 태스크 시작 (한 번만) */
    if (s_train_handle == NULL) {
        s_learn_epoch = 0;
        s_learn_loss  = 0.0f;
        s_learn_done  = false;
        _NN_Init();
        s_train_handle = XM_BgTask_Create("NN_Train", _BgTrainFunc, NULL, 2048);
    }

    /* 학습 완료 대기 */
    if (s_learn_done) {
        s_train_handle = NULL;
        s_nn_trained = true;
        s_last_loss  = s_learn_loss;

        /* REPLAY 전환 */
        s_phase = PHASE_REPLAY;
        s_replay_idx = 0;
        s_replay_dir = 1;
        s_replay_tick = XM_GetTick();
        s_d_hat_r = 0.0f;
        s_d_hat_l = 0.0f;
        s_ab_pos_r = XM.status.h10.rightHipMotorAngle;
        s_ab_vel_r = 0.0f;
        s_ab_pos_l = XM.status.h10.leftHipMotorAngle;
        s_ab_vel_l = 0.0f;
        s_ab_init  = false;
        s_prev_target_r = XM.status.h10.rightHipMotorAngle;
        s_prev_target_l = XM.status.h10.leftHipMotorAngle;
        _UpdateLeds();
    }
}

/** @brief REPLAY — NN + Gravity + DOB + LQR-PD + NN Feedforward */
static void _Replay_Loop(void)
{
    if (s_teach_count == 0) {
        s_phase = PHASE_IDLE;
        _UpdateLeds();
        return;
    }

    /* === 1. 센서 === */
    float meas_r = XM.status.h10.rightHipMotorAngle;
    float meas_l = XM.status.h10.leftHipMotorAngle;
    float ang_r_rad = DEG_TO_RAD(meas_r);
    float ang_l_rad = DEG_TO_RAD(meas_l);

    /* === 2. Alpha-Beta 속도 추정 === */
    float vel_r_dps, vel_l_dps;

    if (!s_ab_init) {
        s_ab_pos_r = meas_r;  s_ab_pos_l = meas_l;
        s_ab_vel_r = 0.0f;    s_ab_vel_l = 0.0f;
        s_ab_init  = true;
        vel_r_dps  = 0.0f;    vel_l_dps  = 0.0f;
    } else {
        float pred_r = s_ab_pos_r + s_ab_vel_r * CONTROL_DT;
        float pred_l = s_ab_pos_l + s_ab_vel_l * CONTROL_DT;
        float res_r = meas_r - pred_r;
        float res_l = meas_l - pred_l;
        s_ab_pos_r = pred_r + AB_ALPHA * res_r;
        s_ab_vel_r += (AB_BETA / CONTROL_DT) * res_r;
        s_ab_pos_l = pred_l + AB_ALPHA * res_l;
        s_ab_vel_l += (AB_BETA / CONTROL_DT) * res_l;
        vel_r_dps = s_ab_vel_r;
        vel_l_dps = s_ab_vel_l;
    }

    /* === 3. Ping-pong 재생 인덱스 === */
    if ((XM_GetTick() - s_replay_tick) >= 10) {
        s_replay_tick = XM_GetTick();
        int8_t prev_dir = s_replay_dir;

        int32_t next_idx = (int32_t)s_replay_idx + s_replay_dir;
        if (next_idx >= (int32_t)s_teach_count) {
            s_replay_dir = -1;
            next_idx = (int32_t)s_teach_count - 2;
            if (next_idx < 0) next_idx = 0;
        } else if (next_idx < 0) {
            s_replay_dir = 1;
            next_idx = 1;
            if (next_idx >= (int32_t)s_teach_count) next_idx = 0;
        }
        s_replay_idx = (uint32_t)next_idx;

        if (prev_dir != s_replay_dir) {
            s_ab_vel_r = 0.0f;
            s_ab_vel_l = 0.0f;
        }
    }

    /* === 4. NN 추론 → 목표 각도 (교시 범위로 클램핑) === */
    float phase = (float)s_replay_idx / (float)(s_teach_count > 1 ? s_teach_count - 1 : 1);
    float target_r, target_l;
    _NN_Forward(phase, &target_r, &target_l);
    target_r = _Clamp(target_r, s_teach_min_r, s_teach_max_r);
    target_l = _Clamp(target_l, s_teach_min_l, s_teach_max_l);

    /* === 5. Layer 1 — Gravity Compensation (적응 MGL) === */
    float tau_grav_r = _ComputeGravity(ang_r_rad);
    float tau_grav_l = _ComputeGravity(ang_l_rad);

    /* === 6. LQR-PD 추종 === */
    float err_r = target_r - meas_r;
    float err_l = target_l - meas_l;
    float tau_pd_r = KP_LQR * err_r - KD_LQR * vel_r_dps;
    float tau_pd_l = KP_LQR * err_l - KD_LQR * vel_l_dps;

    /* === 7. 합산: Gravity + PD (DOB/FF 비활성 — 안정화 후 활성화) === */
    float total_r = _Clamp(tau_grav_r + tau_pd_r, -MAX_TORQUE_NM, MAX_TORQUE_NM);
    float total_l = _Clamp(tau_grav_l + tau_pd_l, -MAX_TORQUE_NM, MAX_TORQUE_NM);

    XM_SetAssistTorqueRH(total_r);
    XM_SetAssistTorqueLH(total_l);
}

/**
 *-----------------------------------------------------------
 * NEURAL NETWORK (Phase Encoding: phase, sin(2πφ), cos(2πφ))
 *-----------------------------------------------------------
 */

static void _NN_Init(void)
{
    s_rng_state = XM_GetTick();
    float scale1 = 1.0f / sqrtf((float)NN_INPUT);
    float scale2 = 1.0f / sqrtf((float)NN_HIDDEN);

    for (int i = 0; i < NN_HIDDEN; i++) {
        for (int j = 0; j < NN_INPUT; j++) {
            float r = ((float)(_Rand() % 1000) / 500.0f) - 1.0f;
            s_nn.w1[i][j] = r * scale1;
        }
        s_nn.b1[i] = 0.0f;
    }
    for (int i = 0; i < NN_OUTPUT; i++) {
        for (int j = 0; j < NN_HIDDEN; j++) {
            float r = ((float)(_Rand() % 1000) / 500.0f) - 1.0f;
            s_nn.w2[i][j] = r * scale2;
        }
        s_nn.b2[i] = 0.0f;
    }
}

/** @brief Forward: phase → (sin, cos encoding) → hidden(ReLU) → output(linear) */
static void _NN_Forward(float phase, float* out_r, float* out_l)
{
    float input[NN_INPUT] = {
        phase,
        sinf(TWO_PI * phase),
        cosf(TWO_PI * phase)
    };

    for (int i = 0; i < NN_HIDDEN; i++) {
        float z = s_nn.b1[i];
        for (int j = 0; j < NN_INPUT; j++) {
            z += s_nn.w1[i][j] * input[j];
        }
        s_hidden_out[i] = _ReLU(z);
    }

    float o0 = s_nn.b2[0], o1 = s_nn.b2[1];
    for (int j = 0; j < NN_HIDDEN; j++) {
        o0 += s_nn.w2[0][j] * s_hidden_out[j];
        o1 += s_nn.w2[1][j] * s_hidden_out[j];
    }
    *out_r = o0;
    *out_l = o1;
}

static float _NN_TrainEpoch(uint32_t current_epoch)
{
    float progress = (float)current_epoch / (float)(NN_EPOCHS > 1 ? NN_EPOCHS - 1 : 1);
    float lr = NN_LR_INIT + (NN_LR_END - NN_LR_INIT) * progress;

    float total_loss = 0.0f;
    uint32_t n_samples = (s_teach_count < NN_BATCH_SIZE) ? s_teach_count : NN_BATCH_SIZE;

    for (uint32_t s = 0; s < n_samples; s++) {
        uint32_t idx = _Rand() % s_teach_count;
        float phase = (float)idx / (float)(s_teach_count > 1 ? s_teach_count - 1 : 1);

        float input[NN_INPUT] = {
            phase,
            sinf(TWO_PI * phase),
            cosf(TWO_PI * phase)
        };

        float label_r = s_teach_buf[idx].theta_r;
        float label_l = s_teach_buf[idx].theta_l;

        /* Forward */
        float pred_r, pred_l;
        _NN_Forward(phase, &pred_r, &pred_l);

        float e_r = pred_r - label_r;
        float e_l = pred_l - label_l;
        total_loss += (e_r * e_r + e_l * e_l) * 0.5f;

        /* Backward: hidden gradient 먼저 (W2 업데이트 전) */
        float dL_do[NN_OUTPUT] = { e_r, e_l };

        for (int j = 0; j < NN_HIDDEN; j++) {
            float dL_dh = 0.0f;
            for (int i = 0; i < NN_OUTPUT; i++) {
                dL_dh += dL_do[i] * s_nn.w2[i][j];
            }
            s_hidden_grad[j] = dL_dh * ((s_hidden_out[j] > 0.0f) ? 1.0f : 0.0f);
        }

        /* W2 업데이트 */
        for (int i = 0; i < NN_OUTPUT; i++) {
            for (int j = 0; j < NN_HIDDEN; j++) {
                s_nn.w2[i][j] -= lr * dL_do[i] * s_hidden_out[j];
            }
            s_nn.b2[i] -= lr * dL_do[i];
        }

        /* W1 업데이트 */
        for (int j = 0; j < NN_HIDDEN; j++) {
            for (int k = 0; k < NN_INPUT; k++) {
                s_nn.w1[j][k] -= lr * s_hidden_grad[j] * input[k];
            }
            s_nn.b1[j] -= lr * s_hidden_grad[j];
        }
    }

    return total_loss / (float)n_samples;
}

static float _ReLU(float x) { return (x > 0.0f) ? x : 0.0f; }

/**
 *-----------------------------------------------------------
 * CONTROL (Ex.35 기반 + 적응 중력보상)
 *-----------------------------------------------------------
 */

static float _ComputeGravity(float angle_rad)
{
    return s_mgl_eff * sinf(angle_rad);
}

static float _UpdateDob(float d_hat_prev, float tau_meas, float tau_model)
{
    return DOB_ALPHA_Q * d_hat_prev + (1.0f - DOB_ALPHA_Q) * (tau_meas - tau_model);
}

/** @brief 투명 모드 + 적응 MGL 추정 */
static void _ApplyTransparent(void)
{
    float ang_r_rad = DEG_TO_RAD(XM.status.h10.rightHipMotorAngle);
    float ang_l_rad = DEG_TO_RAD(XM.status.h10.leftHipMotorAngle);

    /* 적응 MGL 추정: 토크 잔차로 MGL_EFF를 온라인 보정
     * residual = τ_measured - MGL_est·sin(θ)
     * MGL_est += adapt_rate × residual × sin(θ) */
    if (s_phase == PHASE_TEACH) {
        float sin_r = sinf(ang_r_rad);
        float sin_l = sinf(ang_l_rad);
        float tau_r = KT_JOINT_NM_PER_A * XM.status.h10.rightHipTorque;
        float tau_l = KT_JOINT_NM_PER_A * XM.status.h10.leftHipTorque;
        float res_r = tau_r - s_mgl_eff * sin_r;
        float res_l = tau_l - s_mgl_eff * sin_l;
        float adapt = MGL_ADAPT_RATE * (res_r * sin_r + res_l * sin_l) * 0.5f;
        s_mgl_eff = _Clamp(s_mgl_eff + adapt, 0.01f, 2.0f);
    }

    float tau_r = _ComputeGravity(ang_r_rad);
    float tau_l = _ComputeGravity(ang_l_rad);

    XM_SetAssistTorqueRH(_Clamp(tau_r, -MAX_TORQUE_NM, MAX_TORQUE_NM));
    XM_SetAssistTorqueLH(_Clamp(tau_l, -MAX_TORQUE_NM, MAX_TORQUE_NM));
}

/**
 *-----------------------------------------------------------
 * NV FLASH PERSISTENCE
 *-----------------------------------------------------------
 */

static bool _NV_Save(void)
{
    uint32_t magic = NV_MAGIC;
    if (XM_UserNV_Erase() != 0) return false;
    if (XM_UserNV_Write(NV_OFFSET_MAGIC, &magic, sizeof(magic)) != 0) return false;
    if (XM_UserNV_Write(NV_OFFSET_NN, &s_nn, sizeof(TinyNN_t)) != 0) return false;
    if (XM_UserNV_Write(NV_OFFSET_MGL, &s_mgl_eff, sizeof(float)) != 0) return false;
    if (XM_UserNV_Write(NV_OFFSET_COUNT, &s_teach_count, sizeof(uint32_t)) != 0) return false;
    return true;
}

static bool _NV_Load(void)
{
    uint32_t magic = 0;
    if (XM_UserNV_Read(NV_OFFSET_MAGIC, &magic, sizeof(magic)) != 0) return false;
    if (magic != NV_MAGIC) return false;
    if (XM_UserNV_Read(NV_OFFSET_NN, &s_nn, sizeof(TinyNN_t)) != 0) return false;
    if (XM_UserNV_Read(NV_OFFSET_MGL, &s_mgl_eff, sizeof(float)) != 0) return false;
    if (XM_UserNV_Read(NV_OFFSET_COUNT, &s_teach_count, sizeof(uint32_t)) != 0) return false;
    return true;
}

/**
 *-----------------------------------------------------------
 * BUTTON / LED
 *-----------------------------------------------------------
 */

static void _HandleButtons(void)
{
    XmBtnEvent_t btn1 = XM_GetButtonEvent(XM_BTN_1);
    XmBtnEvent_t btn2 = XM_GetButtonEvent(XM_BTN_2);
    XmBtnEvent_t btn3 = XM_GetButtonEvent(XM_BTN_3);

    if (btn1 == XM_BTN_CLICK) {
        switch (s_phase) {
        case PHASE_IDLE:
            s_teach_count = 0;
            s_rec_downcnt = 0;
            s_mgl_eff = MGL_EFF_INIT;
            s_phase = PHASE_TEACH;
            _UpdateLeds();
            break;
        case PHASE_TEACH:
            if (s_teach_count >= 10) {
                /* 교시 범위 계산 (NN 출력 클램핑용) */
                s_teach_min_r = s_teach_buf[0].theta_r;
                s_teach_max_r = s_teach_buf[0].theta_r;
                s_teach_min_l = s_teach_buf[0].theta_l;
                s_teach_max_l = s_teach_buf[0].theta_l;
                for (uint32_t i = 1; i < s_teach_count; i++) {
                    if (s_teach_buf[i].theta_r < s_teach_min_r) s_teach_min_r = s_teach_buf[i].theta_r;
                    if (s_teach_buf[i].theta_r > s_teach_max_r) s_teach_max_r = s_teach_buf[i].theta_r;
                    if (s_teach_buf[i].theta_l < s_teach_min_l) s_teach_min_l = s_teach_buf[i].theta_l;
                    if (s_teach_buf[i].theta_l > s_teach_max_l) s_teach_max_l = s_teach_buf[i].theta_l;
                }
                /* 약간의 마진 추가 (5도) */
                s_teach_min_r -= 5.0f; s_teach_max_r += 5.0f;
                s_teach_min_l -= 5.0f; s_teach_max_l += 5.0f;

                s_phase = PHASE_LEARN;
                s_learn_epoch = 0;
                /* s_learn_tick 제거 — 백그라운드 학습에서 불필요 */
                _NN_Init();
                _UpdateLeds();
            }
            break;
        case PHASE_REPLAY:
            s_phase = PHASE_IDLE;
            XM_SetAssistTorqueRH(0.0f);
            XM_SetAssistTorqueLH(0.0f);
            _UpdateLeds();
            break;
        default:
            break;
        }
    }

    /* BTN1 Long Press: REPLAY 중 NN 가중치를 Flash에 저장 */
    if (btn1 == XM_BTN_LONG_PRESS && s_phase == PHASE_REPLAY && s_nn_trained) {
        if (_NV_Save()) {
            XM_SetLedEffect(XM_LED_2, XM_LED_ONESHOT, 500);
        }
    }

    /* BTN2: IDLE에서 이전 학습으로 즉시 REPLAY */
    if (btn2 == XM_BTN_CLICK) {
        if (s_phase == PHASE_IDLE && s_nn_trained && s_teach_count > 0) {
            s_phase = PHASE_REPLAY;
            s_replay_idx = 0;
            s_replay_dir = 1;
            s_replay_tick = XM_GetTick();
            s_d_hat_r = 0.0f;
            s_d_hat_l = 0.0f;
            s_ab_pos_r = XM.status.h10.rightHipMotorAngle;
            s_ab_vel_r = 0.0f;
            s_ab_pos_l = XM.status.h10.leftHipMotorAngle;
            s_ab_vel_l = 0.0f;
            s_ab_init  = false;
            float phase0 = 0.0f;
            _NN_Forward(phase0, &s_prev_target_r, &s_prev_target_l);
            _UpdateLeds();
        }
    }

    /* BTN3: 전체 리셋 */
    if (btn3 == XM_BTN_CLICK) {
        s_phase = PHASE_IDLE;
        s_teach_count = 0;
        s_nn_trained = false;
        s_mgl_eff = MGL_EFF_INIT;
        _NN_Init();
        XM_SetAssistTorqueRH(0.0f);
        XM_SetAssistTorqueLH(0.0f);
        s_d_hat_r = 0.0f;
        s_d_hat_l = 0.0f;
        _UpdateLeds();
    }
}

static void _UpdateLeds(void)
{
    switch (s_phase) {
    case PHASE_IDLE:
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_SetLedState(XM_LED_2, XM_OFF);
        XM_SetLedState(XM_LED_3, XM_OFF);
        break;
    case PHASE_TEACH:
        XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
        XM_SetLedState(XM_LED_2, XM_OFF);
        XM_SetLedState(XM_LED_3, XM_OFF);
        break;
    case PHASE_LEARN:
        XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 100);
        XM_SetLedState(XM_LED_2, XM_ON);
        XM_SetLedState(XM_LED_3, XM_OFF);
        break;
    case PHASE_REPLAY:
        XM_SetLedState(XM_LED_1, XM_ON);
        XM_SetLedState(XM_LED_2, XM_OFF);
        XM_SetLedState(XM_LED_3, XM_ON);
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
