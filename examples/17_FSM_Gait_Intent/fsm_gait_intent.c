/**
 ******************************************************************************
 * @file    fsm_gait_intent.c
 * @author  HyundoKim
 * @brief   [고급] FSM 기반 보행 의도 인식 + Phase-Dependent 토크 보조
 * @details
 * 보행 주기(Gait Cycle)를 7단계 FSM으로 분해하고, 센서 데이터 기반으로
 * 각 단계 전환을 실시간 감지하여 단계별 최적의 보조 토크를 사전 적용합니다.
 *
 * [보행 주기 이론 (Gait Cycle)]
 *   정상 보행 1주기는 한 발의 착지(Heel Strike)부터 같은 발의 다음 착지까지입니다.
 *   - Stance Phase (지지기): ~60% — 발이 지면에 닿아 있는 구간
 *   - Swing Phase (유각기): ~40% — 발이 공중에 있는 구간
 *
 *   세부 7단계:
 *   [Stance] Loading → Mid-Stance → Terminal Stance → Pre-Swing
 *   [Swing]  Initial Swing → Mid-Swing → Terminal Swing → (착지→Loading)
 *
 * [각 단계의 생체역학적 의미]
 *   1. Loading Response: 체중 수용 — 충격 흡수, 무릎 약간 굴곡
 *   2. Mid-Stance: 단하지 지지 — 신체가 발 위를 통과, 에너지 최소
 *   3. Terminal Stance: 추진 준비 — 발뒤꿈치 들림, 전방 가속
 *   4. Pre-Swing: 스윙 준비 — 발끝 떨어짐 직전, 고관절 굴곡 시작
 *   5. Initial Swing: 다리 전진 — 발이 지면을 떠나 앞으로 이동
 *   6. Mid-Swing: 다리 전진 중간 — 무릎 최대 굴곡, 발 끌림 방지
 *   7. Terminal Swing: 착지 준비 — 무릎 신전, 다음 착지 대비 감속
 *
 * [보조 전략 (Phase-Dependent Assist)]
 *   - Loading Response: 약한 신전 보조 (0.5 Nm) — 무릎 안정화
 *   - Terminal Stance: 추진 보조 (+1.5 Nm) — 앞으로 밀기
 *   - Pre-Swing: 굴곡 보조 (-1.0 Nm) — 다리 들어올리기
 *   - 나머지 단계: 0 Nm (자유 운동, 자연스러운 움직임 보존)
 *   - 모든 토크에 LPF 적용하여 급격한 변화 방지 (스무딩)
 *
 * [안전]
 *   - 토크 포화: |torque| <= MAX_ASSIST_TORQUE (3.0 Nm)
 *   - STANDBY 복귀 시: 양쪽 토크 즉시 0 리셋
 *   - 발 접지 신호 소실 시: 현재 단계 유지 (튀는 전환 방지)
 *
 * [필수 — 사용자 신체 정보 설정]
 *   이 예제에서 사용하는 rightThighAngle, leftThighAngle, rightKneeAngle,
 *   leftKneeAngle, isRightFootContact, isLeftFootContact 등의 동작 분석 추정치는
 *   H10 CM 내부에서 IMU + 역기구학으로 계산됩니다.
 *   올바른 추정을 위해 반드시 User_Setup()에서 XM_SendUserBodyData()를 호출하여
 *   착용자의 신체 정보(몸무게, 키, 분절 길이 등)를 설정해야 합니다.
 *   신체 정보가 미설정이면 무릎 각도, 허벅지 각도, 발 접지 감지가 부정확합니다.
 *
 * @see     docs/api-reference/gait_intent.md
 * @version 1.0
 * @date    Mar 09, 2026
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"

#include <math.h>
#include <string.h>

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/* --- 보행 단계 전환 임계값 --- */
/* 허벅지 각도(deg): 양수=신전(뒤로), 음수=굴곡(앞으로) */
#define MIDSTANCE_ANGLE_THRESHOLD       (5.0f)   /* Loading→MidStance: 허벅지 신전 시작 */
#define TERMINAL_ANGLE_THRESHOLD        (-3.0f)  /* MidStance→Terminal: 허벅지 뒤로 넘어감 */

/* 무릎 각도(deg): 양수=굴곡 */
#define SWING_KNEE_THRESHOLD            (15.0f)  /* PreSwing→InitialSwing: 무릎 굴곡 시작 */
#define MID_SWING_KNEE_THRESHOLD        (40.0f)  /* InitialSwing→MidSwing: 무릎 깊은 굴곡 */
#define TERMINAL_SWING_KNEE_THRESHOLD   (20.0f)  /* MidSwing→TerminalSwing: 무릎 펴지기 시작 */

/* --- 보조 토크 설정 --- */
#define LOADING_ASSIST_NM               (0.5f)   /* Loading Response: 약한 신전 보조 (Nm) */
#define TERMINAL_STANCE_ASSIST_NM       (1.5f)   /* Terminal Stance: 추진 보조 (Nm) */
#define PRE_SWING_ASSIST_NM             (-1.0f)  /* Pre-Swing: 굴곡 보조 (Nm, 음수=굴곡) */
#define MAX_ASSIST_TORQUE               (3.0f)   /* 절대 최대 토크 포화값 (Nm) */
#define TORQUE_LPF_FACTOR               (0.01f)  /* LPF 스무딩 팩터 (작을수록 부드러움) */

/* --- USB 디버그 출력 주기 --- */
#define DEBUG_PRINT_PERIOD_MS           (200)    /* 200ms마다 CDC 출력 */

/* --- USB 로깅 폴더 --- */
#define LOG_FOLDER_NAME                 "GaitIntent"

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief 보행 단계 (한쪽 다리 기준, 양쪽 독립 구동)
 *
 * [Stance Phase — 지지기 (발이 지면에 접촉)]
 *   Loading Response → Mid-Stance → Terminal Stance → Pre-Swing
 *
 * [Swing Phase — 유각기 (발이 공중)]
 *   Initial Swing → Mid-Swing → Terminal Swing → (착지→Loading)
 */
typedef enum {
    GAIT_LOADING_RESPONSE,      /* 1. 체중 수용: 발 접지 시작 → 체중 이동 중 */
    GAIT_MID_STANCE,            /* 2. 단하지 지지: 직립 → 안정 구간 */
    GAIT_TERMINAL_STANCE,       /* 3. 추진 준비: 발뒤꿈치 들림 → 전방 가속 */
    GAIT_PRE_SWING,             /* 4. 스윙 준비: 발끝 떨어짐 직전 */
    GAIT_INITIAL_SWING,         /* 5. 스윙 시작: 다리 전방 이동 시작 */
    GAIT_MID_SWING,             /* 6. 스윙 중간: 무릎 최대 굴곡 */
    GAIT_TERMINAL_SWING,        /* 7. 착지 준비: 무릎 신전, 감속 */
    GAIT_PHASE_COUNT            /* 단계 수 (7) */
} GaitPhase_t;

/**
 * @brief 한쪽 다리의 보행 FSM 상태를 관리하는 구조체
 */
typedef struct {
    GaitPhase_t phase;              /* 현재 보행 단계 */
    float       target_torque_nm;   /* 목표 보조 토크 (Nm) */
    float       current_torque_nm;  /* LPF 적용 후 실제 출력 토크 (Nm) */
} GaitLegFsm_t;

/**
 * @brief USB 스트리밍용 데이터 구조체
 */
typedef struct __attribute__((packed)) {
    float rh_phase;             /* 오른쪽 다리 보행 단계 (0~6) */
    float lh_phase;             /* 왼쪽 다리 보행 단계 (0~6) */
    float rh_torque;            /* 오른쪽 출력 토크 (Nm) */
    float lh_torque;            /* 왼쪽 출력 토크 (Nm) */
    float thigh_angle_r;        /* 오른쪽 허벅지 각도 (deg) */
    float thigh_angle_l;        /* 왼쪽 허벅지 각도 (deg) */
} StreamData_t;

/**
 * @brief USB MSC 로깅용 데이터 구조체
 */
typedef struct __attribute__((packed)) {
    uint32_t tick_ms;               /* 타임스탬프 (ms) */
    uint8_t  rh_phase;              /* 오른쪽 보행 단계 */
    uint8_t  lh_phase;              /* 왼쪽 보행 단계 */
    float    rh_torque;             /* 오른쪽 출력 토크 */
    float    lh_torque;             /* 왼쪽 출력 토크 */
    float    thigh_angle_r;         /* 오른쪽 허벅지 각도 */
    float    thigh_angle_l;         /* 왼쪽 허벅지 각도 */
    float    knee_angle_r;          /* 오른쪽 무릎 각도 */
    float    knee_angle_l;          /* 왼쪽 무릎 각도 */
    float    pelvic_angle;          /* 골반 기울기 */
    bool     is_foot_contact_r;     /* 오른쪽 발 접지 */
    bool     is_foot_contact_l;     /* 왼쪽 발 접지 */
} LogData_t;

/**
 *-----------------------------------------------------------
 * PULBIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

/* --- TSM 핸들 --- */
static XmTsmHandle_t s_tsm;

/* --- 양쪽 다리 보행 FSM --- */
static GaitLegFsm_t s_gait_rh;     /* 오른쪽 다리 */
static GaitLegFsm_t s_gait_lh;     /* 왼쪽 다리 */

/* --- 타이머 --- */
static uint32_t s_debug_timer;

/* --- USB 데이터 --- */
static StreamData_t s_stream;
static LogData_t    s_log;

/* --- 로깅 상태 --- */
static bool s_is_logging;

/* --- 보행 단계 이름 (디버그 출력용) --- */
static const char* const s_phase_names[GAIT_PHASE_COUNT] = {
    "LOADING ",     /* 8자 고정폭으로 정렬 */
    "MIDSTANC",
    "TERMINAL",
    "PRESWING",
    "INI_SWNG",
    "MID_SWNG",
    "TRM_SWNG"
};

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

/* --- TSM 상태 함수 --- */
static void Off_Loop(void);

static void Standby_Entry(void);
static void Standby_Loop(void);

static void Active_Entry(void);
static void Active_Loop(void);
static void Active_Exit(void);

/* --- 보행 FSM 핵심 로직 --- */
static void _UpdateGaitPhase(GaitLegFsm_t *fsm, float thigh_angle_deg,
                             float knee_angle_deg, bool is_foot_contact);
static float _GetPhaseAssistTorque(GaitPhase_t phase);
static void _ApplyPhaseAssist(GaitLegFsm_t *fsm);
static float _ClampTorque(float torque_nm);

/* --- 디버그 / 데이터 --- */
static void _PrintDebugInfo(void);
static void _UpdateStreamData(void);
static void _UpdateLogData(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief 보행 의도 인식 예제를 초기화합니다.
 * @details 부팅 시 한 번 호출됩니다. TSM 생성, USB 스트리밍/로깅 소스를 등록합니다.
 */
void User_Setup(void)
{
    /*
     * [필수] 사용자 신체 정보 전송 — 동작 분석 추정치의 정확도에 직접 영향
     * bodyData[0] = 체중 (kg × 10, 예: 700 = 70.0kg)
     * bodyData[1] = 신장 (cm × 10, 예: 1750 = 175.0cm)
     * bodyData[2~7] = 분절 길이 등 (H10 프로토콜 참조)
     * → 미설정 시 무릎 각도, 허벅지 각도, 발 접지 감지 등이 부정확합니다.
     */
    uint32_t bodyData[8] = { 700, 1750, 0, 0, 0, 0, 0, 0 };  // 70kg, 175cm 예시
    XM_SendUserBodyData(bodyData);

    /* TSM 생성 (초기 상태: OFF) */
    s_tsm = XM_TSM_Create(XM_STATE_OFF);

    /* OFF 상태 등록 */
    XmStateConfig_t off_conf = {
        .id = XM_STATE_OFF,
        .on_loop = Off_Loop
    };
    XM_TSM_AddState(s_tsm, &off_conf);

    /* STANDBY 상태 등록 */
    XmStateConfig_t sb_conf = {
        .id = XM_STATE_STANDBY,
        .on_entry = Standby_Entry,
        .on_loop  = Standby_Loop
    };
    XM_TSM_AddState(s_tsm, &sb_conf);

    /* ACTIVE 상태 등록 */
    XmStateConfig_t act_conf = {
        .id = XM_STATE_ACTIVE,
        .on_entry = Active_Entry,
        .on_loop  = Active_Loop,
        .on_exit  = Active_Exit
    };
    XM_TSM_AddState(s_tsm, &act_conf);

    /* USB 스트리밍 소스 등록 */
    XM_SetUsbCustomMeta(0xF0,
        "[{\"name\":\"RH Phase\",\"unit\":\"phase\"},"
        "{\"name\":\"LH Phase\",\"unit\":\"phase\"},"
        "{\"name\":\"RH Torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"LH Torque\",\"unit\":\"Nm\"},"
        "{\"name\":\"Thigh Angle R\",\"unit\":\"deg\"},"
        "{\"name\":\"Thigh Angle L\",\"unit\":\"deg\"}]");

    /* USB MSC 로깅 소스 등록 */
    XM_SetUsbLogSource(&s_log, sizeof(LogData_t));
}

/**
 * @brief 매 제어 루프(1ms)마다 호출되는 메인 루프입니다.
 */
void User_Loop(void)
{
    /* CM 연결이 끊기면 안전을 위해 OFF로 강제 전환 */
    if (!XM_IsCmConnected()) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_OFF);
    }

    XM_TSM_Run(s_tsm);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/* ====================================================
 * State: OFF — CM 연결 대기
 * ==================================================== */
static void Off_Loop(void)
{
    if (XM_IsCmConnected()) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
    }
}

/* ====================================================
 * State: STANDBY — H10 ASSIST 모드 대기
 * ==================================================== */
static void Standby_Entry(void)
{
    /* LED 1: 대기 상태 표시 (심장박동) */
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1500);
    XM_SetLedState(XM_LED_2, XM_OFF);

    /* 안전: 토크 0으로 리셋 */
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
    XM_SetControlMode(XM_CTRL_MONITOR);

    /* 보행 FSM 초기화 */
    memset(&s_gait_rh, 0, sizeof(GaitLegFsm_t));
    memset(&s_gait_lh, 0, sizeof(GaitLegFsm_t));

    s_is_logging = false;
}

static void Standby_Loop(void)
{
    /* H10이 ASSIST 모드로 전환되면 ACTIVE로 진입 */
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

/* ====================================================
 * State: ACTIVE — 보행 의도 인식 + 토크 보조 실행
 * ==================================================== */
static void Active_Entry(void)
{
    /* 보행 FSM 초기화 — Loading Response에서 시작 */
    s_gait_rh.phase = GAIT_LOADING_RESPONSE;
    s_gait_rh.target_torque_nm = 0.0f;
    s_gait_rh.current_torque_nm = 0.0f;

    s_gait_lh.phase = GAIT_LOADING_RESPONSE;
    s_gait_lh.target_torque_nm = 0.0f;
    s_gait_lh.current_torque_nm = 0.0f;

    /* 타이머 초기화 */
    s_debug_timer = XM_GetTick();

    /* 토크 제어 모드 활성화 */
    XM_SetControlMode(XM_CTRL_TORQUE);

    /* LED: Stance/Swing 구분 표시 초기화 */
    XM_SetLedState(XM_LED_1, XM_ON);   /* LED 1: Stance 표시 */
    XM_SetLedState(XM_LED_2, XM_OFF);  /* LED 2: Swing 표시 */

    /* 로깅 상태 초기화 */
    s_is_logging = false;

    XM_SendUsbDebugMessage("[Gait] ACTIVE: 보행 의도 인식 시작\r\n");
}

static void Active_Loop(void)
{
    /* H10이 STANDBY로 돌아가면 복귀 */
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    /* ------------------------------------------------
     * BTN 1: USB MSC 로깅 시작/정지 토글
     * ------------------------------------------------ */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        if (!s_is_logging) {
            /* 로깅 시작 */
            if (XM_IsUsbLogReady()) {
                s_is_logging = XM_StartUsbDataLog(
                    LOG_FOLDER_NAME,
                    "tick_ms(u32), rh_phase(u8), lh_phase(u8),"
                    "rh_torque(f32), lh_torque(f32),"
                    "thigh_angle_r(f32), thigh_angle_l(f32),"
                    "knee_angle_r(f32), knee_angle_l(f32),"
                    "pelvic_angle(f32),"
                    "is_foot_contact_r(bool), is_foot_contact_l(bool)\n"
                );
                if (s_is_logging) {
                    XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, 300);
                    XM_SendUsbDebugMessage("[Gait] 로깅 시작\r\n");
                }
            }
        } else {
            /* 로깅 정지 */
            XM_StopUsbDataLog();
            XM_SetLedState(XM_LED_3, XM_OFF);
            s_is_logging = false;
            XM_SendUsbDebugMessage("[Gait] 로깅 정지\r\n");
        }
    }

    /* ------------------------------------------------
     * [매 1ms] 보행 단계 FSM 갱신 (양쪽 독립)
     * ------------------------------------------------
     * 오른쪽 다리와 왼쪽 다리는 보행 시 약 50% 위상차를 가짐.
     * 각각 독립적인 FSM으로 관리하여 비대칭 보행에도 대응.
     */
    _UpdateGaitPhase(&s_gait_rh,
                     XM.status.h10.rightThighAngle,
                     XM.status.h10.rightKneeAngle,
                     XM.status.h10.isRightFootContact);

    _UpdateGaitPhase(&s_gait_lh,
                     XM.status.h10.leftThighAngle,
                     XM.status.h10.leftKneeAngle,
                     XM.status.h10.isLeftFootContact);

    /* ------------------------------------------------
     * [매 1ms] Phase-Dependent 보조 토크 적용
     * ------------------------------------------------ */
    _ApplyPhaseAssist(&s_gait_rh);
    _ApplyPhaseAssist(&s_gait_lh);

    /* 토크 출력 (어시스트 레벨 반영) */
    float level_scale = (float)XM.status.h10.h10AssistLevel / 9.0f;
    XM_SetAssistTorqueRH(s_gait_rh.current_torque_nm * level_scale);
    XM_SetAssistTorqueLH(s_gait_lh.current_torque_nm * level_scale);

    /* ------------------------------------------------
     * LED 표시: Stance/Swing 구분 (오른쪽 다리 기준)
     * LED 1 = Stance (지지기), LED 2 = Swing (유각기)
     * ------------------------------------------------ */
    bool is_stance = (s_gait_rh.phase <= GAIT_PRE_SWING);
    XM_SetLedState(XM_LED_1, is_stance ? XM_ON : XM_OFF);
    XM_SetLedState(XM_LED_2, is_stance ? XM_OFF : XM_ON);

    /* ------------------------------------------------
     * USB 스트리밍 데이터 갱신
     * ------------------------------------------------ */
    _UpdateStreamData();

    /* ------------------------------------------------
     * USB MSC 로깅 데이터 갱신
     * ------------------------------------------------ */
    if (s_is_logging) {
        _UpdateLogData();
    }

    /* ------------------------------------------------
     * [매 200ms] USB CDC 디버그 메시지
     * ------------------------------------------------ */
    uint32_t now = XM_GetTick();
    if (now - s_debug_timer >= DEBUG_PRINT_PERIOD_MS) {
        s_debug_timer = now;
        _PrintDebugInfo();
    }
}

static void Active_Exit(void)
{
    /* 안전: 토크 즉시 0 리셋 */
    XM_SetAssistTorqueRH(0.0f);
    XM_SetAssistTorqueLH(0.0f);
    XM_SetControlMode(XM_CTRL_MONITOR);

    /* 로깅 중이면 정지 */
    if (s_is_logging) {
        XM_StopUsbDataLog();
        s_is_logging = false;
    }

    /* LED 끄기 */
    XM_SetLedState(XM_LED_1, XM_OFF);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[Gait] ACTIVE 종료 — 토크 리셋 완료\r\n");
}

/* ====================================================
 * 보행 단계 FSM 갱신
 * ====================================================
 *
 * [전환 조건 요약]
 *
 *  Loading ──(thigh > MIDSTANCE && footContact)──→ MidStance
 *  MidStance ──(thigh < TERMINAL)──→ Terminal
 *  Terminal ──(!footContact)──→ PreSwing
 *  PreSwing ──(knee > SWING_KNEE)──→ InitialSwing
 *  InitialSwing ──(knee > MID_SWING_KNEE)──→ MidSwing
 *  MidSwing ──(knee < TERMINAL_SWING_KNEE)──→ TerminalSwing
 *  TerminalSwing ──(footContact)──→ Loading
 *
 * [설계 원칙]
 * - 각 전환은 단방향(순환): 뒤로 돌아가지 않음
 * - 센서 잡음으로 인한 오탐 방지: 각 전환 조건에 물리적 의미 부여
 * - 발 접지 신호가 핵심 anchor: Stance↔Swing 전환의 기준점
 */
static void _UpdateGaitPhase(GaitLegFsm_t *fsm, float thigh_angle_deg,
                             float knee_angle_deg, bool is_foot_contact)
{
    switch (fsm->phase) {
        /* ---- Stance Phase (지지기) ---- */

        case GAIT_LOADING_RESPONSE:
            /*
             * 체중 수용 → 단하지 지지 전환
             * 조건: 허벅지가 신전 방향으로 넘어감 + 발이 여전히 접지 중
             * 의미: 체중이 완전히 이 다리로 이동 완료
             */
            if (thigh_angle_deg > MIDSTANCE_ANGLE_THRESHOLD && is_foot_contact) {
                fsm->phase = GAIT_MID_STANCE;
            }
            break;

        case GAIT_MID_STANCE:
            /*
             * 단하지 지지 → 추진 준비 전환
             * 조건: 허벅지가 뒤로 넘어감 (신체가 발 앞으로 이동)
             * 의미: 추진(propulsion) 구간 시작
             */
            if (thigh_angle_deg < TERMINAL_ANGLE_THRESHOLD) {
                fsm->phase = GAIT_TERMINAL_STANCE;
            }
            break;

        case GAIT_TERMINAL_STANCE:
            /*
             * 추진 → 스윙 준비 전환
             * 조건: 발 접지 해제 (발끝이 지면에서 떨어짐)
             * 의미: Toe-Off 이벤트 감지
             */
            if (!is_foot_contact) {
                fsm->phase = GAIT_PRE_SWING;
            }
            break;

        case GAIT_PRE_SWING:
            /*
             * 스윙 준비 → 스윙 시작 전환
             * 조건: 무릎이 충분히 굴곡 (다리가 들려올라감)
             * 의미: 유각기(Swing Phase) 본격 시작
             */
            if (knee_angle_deg > SWING_KNEE_THRESHOLD) {
                fsm->phase = GAIT_INITIAL_SWING;
            }
            break;

        /* ---- Swing Phase (유각기) ---- */

        case GAIT_INITIAL_SWING:
            /*
             * 스윙 시작 → 스윙 중간 전환
             * 조건: 무릎 최대 굴곡 구간 진입
             * 의미: 발 끌림(foot drag) 방지를 위한 최대 무릎 굴곡
             */
            if (knee_angle_deg > MID_SWING_KNEE_THRESHOLD) {
                fsm->phase = GAIT_MID_SWING;
            }
            break;

        case GAIT_MID_SWING:
            /*
             * 스윙 중간 → 착지 준비 전환
             * 조건: 무릎이 펴지기 시작 (감속 구간)
             * 의미: 다음 착지를 위한 무릎 신전 시작
             */
            if (knee_angle_deg < TERMINAL_SWING_KNEE_THRESHOLD) {
                fsm->phase = GAIT_TERMINAL_SWING;
            }
            break;

        case GAIT_TERMINAL_SWING:
            /*
             * 착지 준비 → 체중 수용 (순환 완료)
             * 조건: 발 접지 감지 (Heel Strike)
             * 의미: 새로운 보행 주기 시작
             */
            if (is_foot_contact) {
                fsm->phase = GAIT_LOADING_RESPONSE;
            }
            break;

        default:
            /* 방어적 처리: 알 수 없는 상태 → Loading으로 복귀 */
            fsm->phase = GAIT_LOADING_RESPONSE;
            break;
    }
}

/**
 * @brief 보행 단계별 목표 보조 토크를 반환합니다.
 * @param phase 현재 보행 단계
 * @return 해당 단계의 목표 토크 (Nm)
 *
 * [보조 전략 근거]
 * - Loading: 체중 수용 시 무릎 안정화 → 약한 신전 보조
 * - Terminal Stance: 추진력 보강 → 전방 밀기 보조
 * - Pre-Swing: 다리 들어올리기 보조 → 굴곡 보조
 * - 나머지: 자연스러운 움직임 보존 → 0 Nm (과도한 보조는 근위축 유발)
 */
static float _GetPhaseAssistTorque(GaitPhase_t phase)
{
    switch (phase) {
        case GAIT_LOADING_RESPONSE:
            return LOADING_ASSIST_NM;
        case GAIT_TERMINAL_STANCE:
            return TERMINAL_STANCE_ASSIST_NM;
        case GAIT_PRE_SWING:
            return PRE_SWING_ASSIST_NM;
        default:
            return 0.0f;
    }
}

/**
 * @brief 단계별 보조 토크를 LPF로 스무딩하여 적용합니다.
 * @param fsm 한쪽 다리의 보행 FSM 포인터
 *
 * [LPF 수식]
 * current = factor * target + (1 - factor) * current
 * factor = 0.01 → 시상수 약 100ms (1ms 루프 기준)
 * → 토크 급변 방지, 착용자가 부드러운 전환을 체감
 */
static void _ApplyPhaseAssist(GaitLegFsm_t *fsm)
{
    /* 현재 단계의 목표 토크 결정 */
    fsm->target_torque_nm = _GetPhaseAssistTorque(fsm->phase);

    /* LPF 스무딩 */
    fsm->current_torque_nm = TORQUE_LPF_FACTOR * fsm->target_torque_nm
                           + (1.0f - TORQUE_LPF_FACTOR) * fsm->current_torque_nm;

    /* 토크 포화 (안전) */
    fsm->current_torque_nm = _ClampTorque(fsm->current_torque_nm);
}

/**
 * @brief 토크 포화 함수 — 절대값이 MAX_ASSIST_TORQUE를 넘지 않도록 제한
 * @param torque_nm 입력 토크 (Nm)
 * @return 포화 적용된 토크 (Nm)
 */
static float _ClampTorque(float torque_nm)
{
    if (torque_nm > MAX_ASSIST_TORQUE) {
        return MAX_ASSIST_TORQUE;
    }
    if (torque_nm < -MAX_ASSIST_TORQUE) {
        return -MAX_ASSIST_TORQUE;
    }
    return torque_nm;
}

/* ====================================================
 * USB 스트리밍 데이터 갱신
 * ==================================================== */
static void _UpdateStreamData(void)
{
    s_stream.rh_phase     = (float)s_gait_rh.phase;
    s_stream.lh_phase     = (float)s_gait_lh.phase;
    s_stream.rh_torque    = s_gait_rh.current_torque_nm;
    s_stream.lh_torque    = s_gait_lh.current_torque_nm;
    s_stream.thigh_angle_r = XM.status.h10.rightThighAngle;
    s_stream.thigh_angle_l = XM.status.h10.leftThighAngle;
    XM_SendUsbDataWithId(&s_stream, sizeof(s_stream), 0xF0);
}

/* ====================================================
 * USB MSC 로깅 데이터 갱신
 * ==================================================== */
static void _UpdateLogData(void)
{
    s_log.tick_ms          = XM_GetTick();
    s_log.rh_phase         = (uint8_t)s_gait_rh.phase;
    s_log.lh_phase         = (uint8_t)s_gait_lh.phase;
    s_log.rh_torque        = s_gait_rh.current_torque_nm;
    s_log.lh_torque        = s_gait_lh.current_torque_nm;
    s_log.thigh_angle_r    = XM.status.h10.rightThighAngle;
    s_log.thigh_angle_l    = XM.status.h10.leftThighAngle;
    s_log.knee_angle_r     = XM.status.h10.rightKneeAngle;
    s_log.knee_angle_l     = XM.status.h10.leftKneeAngle;
    s_log.pelvic_angle     = XM.status.h10.pelvicAngle;
    s_log.is_foot_contact_r = XM.status.h10.isRightFootContact;
    s_log.is_foot_contact_l = XM.status.h10.isLeftFootContact;
}

/* ====================================================
 * USB CDC 디버그 메시지 출력 (200ms 주기)
 * ==================================================== */
static void _PrintDebugInfo(void)
{
    char msg[128];

    /* 보행 단계 이름 안전 참조 */
    const char* rh_name = "UNKNOWN ";
    const char* lh_name = "UNKNOWN ";
    if (s_gait_rh.phase < GAIT_PHASE_COUNT) {
        rh_name = s_phase_names[s_gait_rh.phase];
    }
    if (s_gait_lh.phase < GAIT_PHASE_COUNT) {
        lh_name = s_phase_names[s_gait_lh.phase];
    }

    /* 형식: "Gait | RH:%s LH:%s Tau_R:%.2f Tau_L:%.2f\r\n" */
    snprintf(msg, sizeof(msg),
             "Gait | RH:%s LH:%s Tau_R:%.2f Tau_L:%.2f\r\n",
             rh_name,
             lh_name,
             s_gait_rh.current_torque_nm,
             s_gait_lh.current_torque_nm);

    XM_SendUsbDebugMessage(msg);
}
