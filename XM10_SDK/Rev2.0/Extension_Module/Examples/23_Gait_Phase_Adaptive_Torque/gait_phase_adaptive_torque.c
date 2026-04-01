/**
 ******************************************************************************
 * @file    gait_phase_adaptive_torque.c
 * @author  HyundoKim
 * @brief   [고급] 보행 위상 동기 적응 토크 프로파일 제어
 * @details
 * 보행 주기(gaitCycle, 0~100%)에 동기화된 구간별 정현파 토크 프로파일을 생성합니다.
 * 시간 기반이 아닌 보행 위상 기반이므로 보행 속도 변화에 자연스럽게 적응합니다.
 *
 * [제어 이론 — 보행 위상 동기 토크 프로파일]
 * 보행 주기를 4구간으로 분할하여 구간별 정현파 토크를 생성합니다:
 *   φ = gaitCycle / 100.0f                  (정규화 위상, 0~1)
 *
 *   0~30%  초기 입각기 (Initial Contact~Loading): τ = 0
 *   30~60% 중간·말기 입각기 (Mid~Terminal Stance): τ = A·sin(π·(φ-0.3)/0.3) [신전 보조]
 *   60~80% 전기·초기 유각기 (Pre~Initial Swing):  τ = -A·sin(π·(φ-0.6)/0.2) [굴곡 보조]
 *   80~100% 말기 유각기 (Terminal Swing):         τ = 0 (fade out)
 *
 * [⚠️ Body Data 전제조건 — 필수]
 * footContact, forwardVelocity는 H10 CM 실시간 보행 분석(1kHz) 출력입니다.
 * _EstimateGaitCycle()이 footContact 기반으로 보행 위상(0~100%)을 추정합니다.
 * XM_SendUserBodyData()로 신체 데이터를 H10에 전달하지 않으면:
 *   - footContact 항상 0 → 보행 위상 추정 불가
 *   - forwardVelocity 부정확 → 정지/보행 판정 오류
 * 이 예제는 Body Data 없이 정상 동작하지 않습니다.
 * 대안: IMU Hub 등 외부 센서로 독립 보행 위상 계측 가능
 *
 * [논문 레퍼런스]
 * - Quinlivan, B.T. et al. (2017) "Assistance magnitude versus metabolic cost reductions
 *   for a tethered multiarticular soft exosuit" Science Robotics, 2(2), eaah4416.
 * - Zhang, J. et al. (2017) "Human-in-the-loop optimization of exoskeleton assistance
 *   during walking" Science, 356(6344), 1280-1284.
 *
 * [버튼 조작]
 * - BTN1: 토크 진폭 단계 증가 (0.5→1.0→1.5→2.0→2.5→3.0 Nm 래핑)
 * - BTN2: 프로파일 위상 오프셋 조절 (-10%→0%→+10% 3단계 순환)
 * - BTN3: 좌우 대칭/비대칭 모드 전환 (대칭↔우측 강화↔좌측 강화)
 *
 * @see     docs/api-reference/02-h10-control-n-data.md
 * @version 1.0
 * @date    Mar 10, 2026
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include <math.h>

/* ===================================================================
 * Gait Cycle Estimator — Heel Strike Detection + Linear Interpolation
 *
 * [⚠️ Body Data 필수]
 * footContact는 H10의 실시간 동작 분석 알고리즘에 의해 추정됩니다.
 * XM_SendUserBodyData()로 인체 정보(체중, 키, 다리 길이)를 반드시 설정하세요.
 * 미설정 시 footContact가 항상 0 → 보행 위상 추정 불가.
 *
 * [알고리즘]
 * 1. forwardVelocity < 임계값 → 정지 (gaitCycle = 0)
 * 2. leftFootContact rising edge → Heel Strike → stride 주기 갱신
 * 3. Heel Strike 간 경과 시간으로 선형 보간 (0~100%)
 *
 * [정확도] ±2-3% (steady-state walking, Body Data 설정 시)
 * =================================================================== */
#define GAIT_EST_VELOCITY_THRESHOLD  0.1f   /* m/s — 이하이면 정지 판정 */
#define GAIT_EST_MIN_STRIDE_MS       400U   /* 최소 stride 주기 (잡음 방지) */
#define GAIT_EST_MAX_STRIDE_MS       3000U  /* 최대 stride 주기 (타임아웃) */

static uint8_t _EstimateGaitCycle(void)
{
    static bool     s_prev_left_contact = false;
    static uint32_t s_last_hs_tick      = 0;
    static uint32_t s_stride_period_ms  = 1000;  /* 초기값 ~1Hz 보행 가정 */
    static bool     s_has_first_hs      = false;

    /* 정지 판정 */
    if (XM.status.h10.forwardVelocity < GAIT_EST_VELOCITY_THRESHOLD) {
        s_has_first_hs = false;
        return 0;
    }

    uint32_t now = XM_GetTick();
    bool left_contact = XM.status.h10.isLeftFootContact;

    /* Heel Strike 감지: left foot contact rising edge */
    if (left_contact && !s_prev_left_contact) {
        uint32_t elapsed = now - s_last_hs_tick;
        if (s_has_first_hs && elapsed >= GAIT_EST_MIN_STRIDE_MS && elapsed <= GAIT_EST_MAX_STRIDE_MS) {
            s_stride_period_ms = elapsed;
        }
        s_last_hs_tick = now;
        s_has_first_hs = true;
    }
    s_prev_left_contact = left_contact;

    /* 선형 보간 */
    if (!s_has_first_hs || s_stride_period_ms == 0) {
        return 0;
    }

    uint32_t since_hs = now - s_last_hs_tick;
    uint32_t pct = (since_hs * 100U) / s_stride_period_ms;
    return (pct > 100U) ? 100U : (uint8_t)pct;
}

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* --- 토크 진폭 설정 --- */
#define TORQUE_AMP_DEFAULT  1.0f    /* 기본 토크 진폭 (Nm) */
#define TORQUE_AMP_STEP     0.5f    /* BTN1 증가 단위 */
#define TORQUE_AMP_MAX      3.0f    /* 최대 진폭 (Nm) */
#define TORQUE_AMP_MIN      0.5f    /* 최소 진폭 (Nm) */
#define MAX_TORQUE_NM       3.0f    /* 포화 한계 (Nm, 보행 토크는 보수적으로 설정) */

/* --- 보행 속도 안전 임계값 --- */
#define MIN_FWD_VELOCITY    0.1f    /* 이 속도 미만이면 정지 상태로 판단, 토크=0 */

/* --- 위상 오프셋 프리셋 (% 단위, gaitCycle에 적용) --- */
#define PHASE_OFFSET_COUNT  3U
static const float k_phase_offsets[PHASE_OFFSET_COUNT] = {-0.10f, 0.0f, 0.10f};

/* --- 제어 주기 --- */
#define CONTROL_DT          0.001f
#define USB_DEBUG_PERIOD_MS 500U

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** @brief 좌우 보조 모드 */
typedef enum {
    ASSIST_MODE_SYMMETRIC = 0,  /* 좌우 동일 토크 */
    ASSIST_MODE_RIGHT_DOM,      /* 우측 강화 (좌측 50%) */
    ASSIST_MODE_LEFT_DOM,       /* 좌측 강화 (우측 50%) */
} AssistMode_t;

/** @brief USB 스트리밍 구조체 */
typedef struct {
    float gait_phase;   /* 정규화 보행 위상 (0~1) */
    float torque_rh;    /* 우측 출력 토크 (Nm) */
    float torque_lh;    /* 좌측 출력 토크 (Nm) */
    float fwd_vel;      /* 전진 속도 (m/s) */
} GaitStreamData_t;

/**
 *-----------------------------------------------------------
 * PUBLIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */

/**
 *-----------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *-----------------------------------------------------------
 */

static XmTsmHandle_t    s_tsm;
static float            s_torque_amp        = TORQUE_AMP_DEFAULT;
static uint8_t          s_phase_offset_idx  = 1U;    /* 기본값: 오프셋 0% */
static AssistMode_t     s_assist_mode       = ASSIST_MODE_SYMMETRIC;
static float            s_torque_rh         = 0.0f;
static float            s_torque_lh         = 0.0f;
static uint32_t         s_usb_debug_timer   = 0U;
static GaitStreamData_t s_stream_data;

/**
 *-----------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

static void  Off_Loop(void);
static void  Standby_Loop(void);
static void  Active_Entry(void);
static void  Active_Loop(void);
static void  Active_Exit(void);

static float _ComputeGaitTorque(float phi, float amplitude);
static void  _HandleButtonInput(void);
static void  _UpdateUsbDebug(void);
static void  _UpdateStreamData(void);
static float _ClampFloat(float val, float min_val, float max_val);
static void  _SetupBodyData(void);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

void User_Setup(void)
{
    /* ⚠️ Body Data 필수 설정
     * footContact/forwardVelocity 정확도를 위해 반드시 호출
     * 미설정 시 보행 위상 추정 불가, 이 예제는 정상 동작하지 않습니다 */
    _SetupBodyData();

    s_tsm = XM_TSM_Create(XM_STATE_OFF);

    XmStateConfig_t off_conf = { .id = XM_STATE_OFF, .on_loop = Off_Loop };
    XM_TSM_AddState(s_tsm, &off_conf);

    XmStateConfig_t sb_conf  = { .id = XM_STATE_STANDBY, .on_loop = Standby_Loop };
    XM_TSM_AddState(s_tsm, &sb_conf);

    XmStateConfig_t act_conf = {
        .id       = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"Gait Phase\",\"unit\":\"-\"},"
        "{\"name\":\"RH Torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"LH Torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"Fwd Velocity\",\"unit\":\"m/s\"}]");
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
 * STATIC FUNCTIONS
 *-----------------------------------------------------------
 */

static void Off_Loop(void)
{
    if (XM_IsCmConnected()) {
        XM_SendUsbDebugMessage("[GAIT] CM 연결됨 -> STANDBY\r\n");
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

static void Standby_Loop(void)
{
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[GAIT] ASSIST 감지 -> ACTIVE\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

static void Active_Entry(void)
{
    XM_SetControlMode(XM_CTRL_TORQUE);
    s_torque_rh = 0.0f;
    s_torque_lh = 0.0f;
    s_usb_debug_timer = XM_GetTick();

    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[GAIT] ACTIVE — 보행 위상 토크 시작\r\n");
}

/**
 * @brief ACTIVE 루프 — 보행 위상 동기 토크 생성 (1ms)
 * @details
 * 1. 전진 속도 확인 (정지 중이면 토크=0 안전 처리)
 * 2. gaitCycle → 정규화 위상 φ 변환
 * 3. 위상 오프셋 적용
 * 4. 구간별 정현파 토크 계산
 * 5. 좌우 모드별 토크 분배
 * 6. 전송
 */
static void Active_Loop(void)
{
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_SendUsbDebugMessage("[GAIT] ASSIST 해제 -> STANDBY\r\n");
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    _HandleButtonInput();

    /* 전진 속도 안전 확인 — 정지 중에는 토크 출력 금지 */
    float fwd_vel = XM.status.h10.forwardVelocity;
    if (fwd_vel < MIN_FWD_VELOCITY) {
        XM_SetAssistTorqueRH(0.0f);
        XM_SetAssistTorqueLH(0.0f);
        s_torque_rh = 0.0f;
        s_torque_lh = 0.0f;
        _UpdateStreamData();
        return;
    }

    /* footContact 기반 보행 위상 추정 (0~100) → 정규화 위상 φ (0~1), 위상 오프셋 적용 */
    float phi_raw  = (float)_EstimateGaitCycle() / 100.0f;
    float offset   = k_phase_offsets[s_phase_offset_idx];
    float phi      = phi_raw + offset;

    /* 위상 범위 [0,1) 정규화 */
    if (phi < 0.0f) { phi += 1.0f; }
    if (phi >= 1.0f) { phi -= 1.0f; }

    /* 구간별 토크 계산 */
    float tau_base = _ComputeGaitTorque(phi, s_torque_amp);

    /* 좌우 모드별 분배 */
    switch (s_assist_mode) {
        case ASSIST_MODE_SYMMETRIC:
            s_torque_rh = tau_base;
            s_torque_lh = tau_base;
            break;
        case ASSIST_MODE_RIGHT_DOM:     /* 우측 강화, 좌측 50% */
            s_torque_rh = tau_base;
            s_torque_lh = tau_base * 0.5f;
            break;
        case ASSIST_MODE_LEFT_DOM:      /* 좌측 강화, 우측 50% */
            s_torque_rh = tau_base * 0.5f;
            s_torque_lh = tau_base;
            break;
        default:
            s_torque_rh = 0.0f;
            s_torque_lh = 0.0f;
            break;
    }

    /* 포화 후 전송 */
    s_torque_rh = _ClampFloat(s_torque_rh, -MAX_TORQUE_NM, MAX_TORQUE_NM);
    s_torque_lh = _ClampFloat(s_torque_lh, -MAX_TORQUE_NM, MAX_TORQUE_NM);
    XM_SetAssistTorqueRH(s_torque_rh);
    XM_SetAssistTorqueLH(s_torque_lh);

    /* LED2: 입각기 표시 (footContact) */
    XM_SetLedState(XM_LED_2, XM.status.h10.isRightFootContact ? XM_ON : XM_OFF);
    XM_SetLedState(XM_LED_3, XM.status.h10.isLeftFootContact  ? XM_ON : XM_OFF);

    _UpdateUsbDebug();
    _UpdateStreamData();
}

static void Active_Exit(void)
{
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
    XM_SetControlMode(XM_CTRL_MONITOR);
    s_torque_rh = 0.0f;
    s_torque_lh = 0.0f;

    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[GAIT] 제어 종료 — 토크 해제\r\n");
}

/**
 * @brief 보행 위상 φ에 따른 구간별 정현파 토크 계산
 * @param phi       정규화 위상 (0~1)
 * @param amplitude 토크 진폭 (Nm)
 * @return 출력 토크 (Nm)
 * @details
 * 구간 정의 (Quinlivan et al. 2017 기반):
 *   0~30%  초기 입각기: τ = 0 (체중 이동 단계, 개입 최소화)
 *   30~60% 입각 중~말기: τ = +A·sin(π·(φ-0.3)/0.3)  (신전 보조, 추진력 증가)
 *   60~80% 전·초기 유각기: τ = -A·sin(π·(φ-0.6)/0.2) (굴곡 보조, 유각 개시 지원)
 *   80~100% 말기 유각기: τ = 0 (착지 준비, 부드러운 fade-out)
 */
static float _ComputeGaitTorque(float phi, float amplitude)
{
    if (phi < 0.30f) {
        return 0.0f;   /* 초기 입각기 — 개입 없음 */
    }
    if (phi < 0.60f) {
        /* 입각 중·말기 — 정현파 신전 보조 */
        return amplitude * sinf((float)M_PI * (phi - 0.30f) / 0.30f);
    }
    if (phi < 0.80f) {
        /* 전·초기 유각기 — 정현파 굴곡 보조 (부호 반전) */
        return -amplitude * sinf((float)M_PI * (phi - 0.60f) / 0.20f);
    }
    return 0.0f;   /* 말기 유각기 — 착지 준비, 토크 제거 */
}

static void _HandleButtonInput(void)
{
    /* BTN1: 토크 진폭 단계 증가 (래핑) */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        s_torque_amp += TORQUE_AMP_STEP;
        if (s_torque_amp > TORQUE_AMP_MAX) { s_torque_amp = TORQUE_AMP_MIN; }
        XM_SendUsbDebugMessage("[GAIT] BTN1: 진폭 변경\r\n");
    }

    /* BTN2: 위상 오프셋 순환 (-10% → 0% → +10%) */
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_phase_offset_idx = (s_phase_offset_idx + 1U) % PHASE_OFFSET_COUNT;
        XM_SendUsbDebugMessage("[GAIT] BTN2: 위상 오프셋 변경\r\n");
    }

    /* BTN3: 좌우 보조 모드 순환 */
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_CLICK) {
        s_assist_mode = (AssistMode_t)(((uint8_t)s_assist_mode + 1U) % 3U);
        XM_SendUsbDebugMessage("[GAIT] BTN3: 보조 모드 전환\r\n");
    }
}

static void _UpdateUsbDebug(void)
{
    uint32_t now = XM_GetTick();
    if (now - s_usb_debug_timer < USB_DEBUG_PERIOD_MS) { return; }
    s_usb_debug_timer = now;

    float phi = (float)_EstimateGaitCycle() / 100.0f;
    char buf[96];
    snprintf(buf, sizeof(buf),
             "GAIT | φ:%.2f A:%.1fNm τRH:%.2f τLH:%.2f v:%.2fm/s\r\n",
             phi, s_torque_amp, s_torque_rh, s_torque_lh,
             XM.status.h10.forwardVelocity);
    XM_SendUsbDebugMessage(buf);
}

static void _UpdateStreamData(void)
{
    s_stream_data.gait_phase = (float)_EstimateGaitCycle() / 100.0f;
    s_stream_data.torque_rh  = s_torque_rh;
    s_stream_data.torque_lh  = s_torque_lh;
    s_stream_data.fwd_vel    = XM.status.h10.forwardVelocity;
    XM_SendUsbDataWithId(&s_stream_data, sizeof(s_stream_data), 0xF0);
}

static float _ClampFloat(float val, float min_val, float max_val)
{
    if (val < min_val) { return min_val; }
    if (val > max_val) { return max_val; }
    return val;
}

static void _SetupBodyData(void)
{
    /* H10 보행 분석 정확도를 위한 신체 데이터 — 실측값 우선 사용 권장 */
    uint32_t body_data[8] = {
        70000U,  /* 체중 (g): 70 kg */
        1750U,   /* 신장 (mm): 175 cm */
        450U,    /* 우측 대퇴 길이 (mm) */
        450U,    /* 좌측 대퇴 길이 (mm) */
        420U,    /* 우측 하퇴 길이 (mm) */
        420U,    /* 좌측 하퇴 길이 (mm) */
        60U,     /* 우측 발목 높이 (mm) */
        60U,     /* 좌측 발목 높이 (mm) */
    };
    XM_SendUserBodyData(body_data);
}
