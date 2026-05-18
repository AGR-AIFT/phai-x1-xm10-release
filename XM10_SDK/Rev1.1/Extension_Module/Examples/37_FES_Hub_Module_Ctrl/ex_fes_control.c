/**
 ******************************************************************************
 * @file    ex_fes_control.c
 * @author  HyundoKim
 * @brief   [Example] FES Hub ES-vector Control via CAN-FD DOP V3
 * @version 1.0
 * @date    2026-04-09
 *
 * @details
 * XM10에서 FES Hub를 ES-vector SDO 명령으로 제어하는 예제.
 * user_app.c에서 이 파일의 함수를 호출하거나,
 * 이 파일 내용을 user_app.c에 복사하여 사용.
 *
 * ============================================================
 *  시나리오 (TSM 3-State)
 * ============================================================
 *
 *  [OFF] ──(FES Hub 연결)──> [STANDBY] ──(BTN1 클릭)──> [ACTIVE]
 *    ^                          ^                          │
 *    │                          │                          │
 *    └──(연결 해제)─────────────┴──(BTN1 클릭, 자극 정지)──┘
 *
 *  OFF:
 *    - LED1 느린 깜빡임 (1초)
 *    - FES Hub PnP 연결 대기
 *    - 연결되면 STANDBY로 자동 전이
 *
 *  STANDBY:
 *    - LED1 점등 (Solid)
 *    - FES Hub 연결 상태 (OPERATIONAL)
 *    - 이 시점에서 FES Hub는 Calibration 완료 후 CH1_READY_CH2_READY 상태
 *    - [BTN1 클릭]:
 *        1) Master Command SDO (0x6310) cmd=1 → 가상 ISI EXT7 set
 *           - FSM: CH1_READY → CH1_STIM (다음 1ms tick)
 *        2) ACTIVE 상태로 전이
 *        3) Active_Loop에서 10ms 경과 후 ES-vector SDO (0x6300) 전송
 *           - amplitude: 20mA, duty: 0.020, frequency: 50Hz, burst: 무한
 *           - STIMULATING 상태에서 수신되므로 즉시 적용 (비블로킹)
 *
 *  ACTIVE:
 *    - LED1 빠른 깜빡임 (200ms)
 *    - CH1 자극 중 — PID가 target_amplitude_mA(20mA) 추종
 *    - TPDO 10ms 주기로 XM.status.fes_hub 자동 갱신 (core_process)
 *    - [BTN2 클릭]: setpoint mid-update
 *        - ES-vector SDO (0x6300) 전송 → amplitude 20→40mA
 *        - PID가 즉시 40mA 추종 시작 (KHJ FW 동작)
 *    - [BTN1 클릭]: 자극 정지
 *        - Master Command SDO (0x6310) cmd=1 → EXT7 다시 set
 *        - FSM: CH1_STIM → CH1_READY (토글)
 *        - STANDBY로 전이
 *
 * ============================================================
 *  사용 방법
 * ============================================================
 *
 *  방법 1: user_app.c에 복사
 *    - 이 파일의 내용을 user_app.c에 통째로 복사
 *    - Control_Setup/User_Loop가 그대로 동작
 *
 *  방법 2: user_app.c에서 호출
 *    - user_app.c의 Control_Setup()에서 Ex_FES_Setup() 호출
 *    - user_app.c의 Control_Loop()에서 Ex_FES_Loop() 호출
 *    - CMakeLists.txt에 이 파일 추가 필요
 *
 * ============================================================
 *  XM → FES Hub 통신 흐름
 * ============================================================
 *
 *  1) XM 전원 ON → system_startup → FesHub_Drv_Init()
 *     → PnP Master에 FES Hub(0x0C, Node ID v3.0) Slave 등록
 *
 *  2) FES Hub 전원 ON → Boot-up (0x70C) → PnP Heartbeat 교환
 *     → Pre-Op SM 자동 실행: TPDO Mapping SDO → NMT START
 *     → FES Hub OPERATIONAL → FesHub_Drv_IsConnected() == true
 *
 *  3) FES Hub TPDO (0x18C, 37B = Legacy 16B + KHJ 21B, 10ms 주기) 자동 수신
 *     → core_process 1ms polling → XM.status.fes_hub 자동 갱신
 *     (KHJ telemetry: fsm_state, isi_packed, ch_target_amplitude_mA,
 *      ch_impedance, ch_pulse_cnt, ch_voltage_diff_V 포함)
 *
 *  4) XM → FES Hub 명령 (비주기, SDO)
 *     - FesHub_Drv_SendESVector(&esv)  → SDO 0x6300:00 (6B atomic)
 *     - FesHub_Drv_SendMasterCommand(cmd) → SDO 0x6310:00 (1B)
 *
 *  5) Master heartbeat timeout (5초) → FES Hub _OnMasterOffline()
 *     → target_amplitude_mA = 0 → PID가 0mA 추종 (안전 정지)
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include "fes_hub_drv.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/** @brief ES-vector 기본 파라미터 (PPT: FES HUB - Protocols.pptx) */
#define FES_DEFAULT_AMPLITUDE_MA    20      /* 전류 진폭 20mA */
#define FES_DEFAULT_DUTY_X1000      20      /* 듀티비 0.020 (= 20 x 0.001) */
#define FES_DEFAULT_FREQUENCY_HZ    50      /* 펄스 주파수 50Hz */
#define FES_DEFAULT_BURST_MS        65535   /* 무한 burst */

/** @brief Mid-update 파라미터 */
#define FES_UPDATE_AMPLITUDE_MA     40      /* 전류 진폭 40mA (변경) */

/** @brief TOGGLE → ES Vector 전송 대기 시간 (ms) */
#define FES_STIM_TRANSITION_MS      10

/**
 *-----------------------------------------------------------
 * STATIC VARIABLES
 *-----------------------------------------------------------
 */

static XmTsmHandle_t s_user_tsm;
static uint32_t      s_stim_start_tick;     /* TOGGLE 전송 시각 기록 */
static bool          s_pending_es_vector;   /* ES Vector 전송 대기 플래그 */

/**
 *-----------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

static void Off_Entry(void);
static void Off_Loop(void);

static void Standby_Entry(void);
static void Standby_Loop(void);

static void Active_Entry(void);
static void Active_Loop(void);
static void Active_Exit(void);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

void Control_Setup(void)
{
    s_user_tsm = XM_TSM_Create(XM_STATE_OFF);

    XmStateConfig_t off_conf = {
        .id = XM_STATE_OFF,
        .on_entry = Off_Entry,
        .on_loop  = Off_Loop
    };
    XM_TSM_AddState(s_user_tsm, &off_conf);

    XmStateConfig_t sb_conf = {
        .id = XM_STATE_STANDBY,
        .on_entry = Standby_Entry,
        .on_loop  = Standby_Loop
    };
    XM_TSM_AddState(s_user_tsm, &sb_conf);

    XmStateConfig_t act_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit,
    };
    XM_TSM_AddState(s_user_tsm, &act_conf);
}

void Control_Loop(void)
{
    XM_TSM_Run(s_user_tsm);
}

/**
 *-----------------------------------------------------------
 * STATIC FUNCTIONS — OFF State
 *-----------------------------------------------------------
 */

static void Off_Entry(void)
{
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 1000);
}

static void Off_Loop(void)
{
    if (FesHub_Drv_IsConnected()) {
        XM_TSM_TransitionTo(s_user_tsm, XM_STATE_STANDBY);
    }
}

/**
 *-----------------------------------------------------------
 * STATIC FUNCTIONS — STANDBY State
 *-----------------------------------------------------------
 */

static void Standby_Entry(void)
{
    XM_SetLedEffect(XM_LED_1, XM_LED_SOLID, 0);
}

static void Standby_Loop(void)
{
    if (!FesHub_Drv_IsConnected()) {
        XM_TSM_TransitionTo(s_user_tsm, XM_STATE_OFF);
        return;
    }

    /* BTN1 클릭 → Master toggle CH1 → ACTIVE (ES-vector는 전이 후 전송) */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {

        /* TOGGLE 전송 → FES FSM READY→STIM 전이 시작 */
        FesHub_Drv_SendMasterCommand(FESHUB_MCMD_TOGGLE_CH1);

        /* ES Vector는 STIMULATING 전이 완료 후 전송 (Active_Loop에서 처리) */
        s_stim_start_tick  = XM_GetTick();
        s_pending_es_vector = true;

        XM_TSM_TransitionTo(s_user_tsm, XM_STATE_ACTIVE);
    }
}

/**
 *-----------------------------------------------------------
 * STATIC FUNCTIONS — ACTIVE State
 *-----------------------------------------------------------
 */

static void Active_Entry(void)
{
    XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
}

static void Active_Loop(void)
{
    if (!FesHub_Drv_IsConnected()) {
        s_pending_es_vector = false;
        XM_TSM_TransitionTo(s_user_tsm, XM_STATE_OFF);
        return;
    }

    /* TOGGLE 후 FES FSM 전이 대기 완료 → ES Vector 전송 */
    if (s_pending_es_vector &&
        (XM_GetTick() - s_stim_start_tick) >= FES_STIM_TRANSITION_MS) {

        FesHub_ESVector_t esv = {
            .ch_select     = 1,                         /* CH1 only */
            .amplitude_mA  = FES_DEFAULT_AMPLITUDE_MA,  /* 20mA */
            .duty_x1000    = FES_DEFAULT_DUTY_X1000,    /* 0.020 */
            .frequency_Hz  = FES_DEFAULT_FREQUENCY_HZ,  /* 50Hz */
            .burst_ms      = FES_DEFAULT_BURST_MS,      /* 무한 */
        };
        FesHub_Drv_SendESVector(&esv);
        s_pending_es_vector = false;
    }

    /* BTN2 클릭 → amplitude mid-update (20mA → 40mA) */
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        FesHub_ESVector_t esv_update = {
            .ch_select     = 1,
            .amplitude_mA  = FES_UPDATE_AMPLITUDE_MA,   /* 40mA */
            .duty_x1000    = FES_DEFAULT_DUTY_X1000,
            .frequency_Hz  = FES_DEFAULT_FREQUENCY_HZ,
            .burst_ms      = FES_DEFAULT_BURST_MS,
        };
        FesHub_Drv_SendESVector(&esv_update);
    }

    /* BTN1 클릭 → CH1 자극 정지 → STANDBY */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        FesHub_Drv_SendMasterCommand(FESHUB_MCMD_TOGGLE_CH1);
        XM_TSM_TransitionTo(s_user_tsm, XM_STATE_STANDBY);
    }
}

static void Active_Exit(void)
{
    /* LED 복원은 다음 state의 Entry에서 처리 */
}
