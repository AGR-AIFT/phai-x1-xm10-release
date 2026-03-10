/**
 ******************************************************************************
 * @file    tinyai_sensor_fusion.c
 * @author  HyundoKim
 * @brief   [고급] Tiny AI: 자세 분류 (H10 자세 데이터 + IMU Hub 센서 퓨전)
 * @details
 * H10 로봇 또는 IMU Hub Module에서 자세 데이터를 획득하고,
 * 3층 퍼셉트론(Tiny Neural Network)으로 실시간 자세를 분류하는 예제입니다.
 *
 * [두 가지 동작 모드]
 *   컴파일 시 #define으로 모드를 선택합니다.
 *
 *   Mode 1 — USE_H10_PRECOMPUTED (기본값, 추가 HW 불필요)
 *     H10 내부에서 이미 센서 퓨전(가속도+자이로 → 자세 추정)이 완료된
 *     SagittalPitch/FrontalRoll 값을 직접 NN 입력으로 사용합니다.
 *     상보 필터가 필요 없으며, H10 연결만으로 동작합니다.
 *     시연 포인트: "로봇이 제공하는 전처리 데이터 + MCU-side NN 분류"
 *
 *   Mode 2 — USE_IMU_HUB_FUSION (IMU Hub Module 필요)
 *     IMU Hub Module의 body-frame 원시 가속도/자이로 데이터에
 *     상보 필터(Complementary Filter)를 적용하여 자세를 추정합니다.
 *     시연 포인트: "raw IMU에서 실제 센서 퓨전 + NN 분류"
 *
 * [왜 H10의 Global-frame 가속도에 상보 필터를 쓰면 안 되는가?]
 *   H10이 제공하는 leftHipImuGlobalAccX/Y/Z는 이미 좌표 회전(rotation)이
 *   적용된 글로벌 프레임 값입니다. 따라서 기울기에 관계없이 acc_z ≈ 9.81 m/s²,
 *   acc_x ≈ 0, acc_y ≈ 0이 되므로 atan2로 Pitch/Roll을 추정하면 항상 ~0°가
 *   나옵니다. 상보 필터에는 반드시 body-frame 가속도가 필요합니다.
 *
 * [Part A — 상보 필터 (Complementary Filter, Mode 2에서만 사용)]
 *   가속도계는 고주파 노이즈에 취약하지만 장기적으로 중력 방향을 정확히 측정합니다.
 *   자이로스코프는 단기간 정확하지만 적분 시 드리프트가 누적됩니다.
 *   상보 필터는 주파수 도메인에서 두 센서의 장점을 결합합니다:
 *     - 고주파 영역: 자이로 (빠른 자세 변화 추적)
 *     - 저주파 영역: 가속도계 (드리프트 보정)
 *     - pitch = ALPHA * (pitch + gyro*dt) + (1-ALPHA) * pitch_acc
 *     - ALPHA=0.98, dt=0.001s → 차단 주파수 fc ≈ 1/(2*pi*0.001/0.02) ≈ 3.2Hz
 *
 * [Part B — Tiny Neural Network 자세 분류 (두 모드 공통)]
 *   구조: 4 → 8 → 4 → 3 (입력 → 은닉1 → 은닉2 → 출력)
 *   입력: [pitch, roll, pitch_rate, roll_rate]
 *   출력: UPRIGHT(직립), FORWARD_LEAN(전방경사), BACKWARD_LEAN(후방경사)
 *   활성화: ReLU(은닉층), argmax(출력층 — Softmax 대신 경량화)
 *
 *   [Tiny ML 워크플로우]
 *   1. Python(TensorFlow/PyTorch)에서 모델 학습
 *   2. 가중치를 C 배열로 export (float32 또는 int8 양자화)
 *   3. MCU에서 순전파(forward pass)만 수행 — 역전파 불필요
 *   4. 메모리 제약: 4→8→4→3 = 총 67개 파라미터 x 4B = 268B (충분)
 *   5. 연산 제약: 행렬 곱 67회 x 20Hz = 1,340 ops/s (Cortex-M7에서 무시 가능)
 *   6. 양자화(Quantization): float32 → int8로 변환 시 메모리 75% 절감 가능
 *
 * [안전]
 *   분류 결과는 모니터링 전용이며 제어에 직접 영향을 주지 않습니다.
 *   XM_CTRL_MONITOR 모드를 유지합니다.
 *
 * @see     docs/api-reference/tinyai.md
 * @version 2.0
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

/*
 * [모드 선택] 아래 중 하나만 활성화하세요.
 * H10_PRECOMPUTED: H10이 이미 계산한 자세 데이터 사용 (기본값, 추가 HW 불필요)
 * IMU_HUB_FUSION:  IMU Hub Module의 raw 데이터에 상보 필터 적용 (IMU Hub 필요)
 */
#define USE_H10_PRECOMPUTED
// #define USE_IMU_HUB_FUSION

/* 모드 중복 선택 방어 */
#if defined(USE_H10_PRECOMPUTED) && defined(USE_IMU_HUB_FUSION)
    #error "USE_H10_PRECOMPUTED와 USE_IMU_HUB_FUSION 중 하나만 선택하세요."
#endif
#if !defined(USE_H10_PRECOMPUTED) && !defined(USE_IMU_HUB_FUSION)
    #error "USE_H10_PRECOMPUTED 또는 USE_IMU_HUB_FUSION 중 하나를 정의하세요."
#endif

/* --- 상보 필터 파라미터 (IMU Hub 모드에서만 사용) --- */
#define CF_ALPHA                (0.98f)     /* 상보 필터 계수 (0.95~0.99 권장) */
#define CF_DT                   (0.001f)    /* 제어 루프 주기 (1ms = 1kHz) */
#define RAD_TO_DEG              (57.2957795f)  /* radian → degree 변환 계수 */

/* --- IMU Hub 센서 인덱스 --- */
#define IMU_HUB_SENSOR_IDX      (0)         /* 사용할 IMU Hub 센서 포트 (0번) */

/* --- Tiny NN 구조 파라미터 --- */
#define NN_INPUT_SIZE           (4)         /* 입력: pitch, roll, pitch_rate, roll_rate */
#define NN_HIDDEN1_SIZE         (8)         /* 은닉층 1 뉴런 수 */
#define NN_HIDDEN2_SIZE         (4)         /* 은닉층 2 뉴런 수 */
#define NN_OUTPUT_SIZE          (3)         /* 출력 클래스 수 */

/* --- 추론 주기 --- */
#define INFERENCE_PERIOD_MS     (50)        /* 50ms마다 추론 (20Hz) — 자세 변화는 느림 */

/* --- USB 디버그 출력 주기 --- */
#define DEBUG_PRINT_PERIOD_MS   (500)       /* 500ms마다 CDC 출력 */

/* --- 자세 분류 클래스 --- */
#define POSTURE_UPRIGHT         (0)         /* 직립 자세 */
#define POSTURE_FORWARD_LEAN    (1)         /* 전방 경사 */
#define POSTURE_BACKWARD_LEAN   (2)         /* 후방 경사 */

/* --- LED 효과 주기 --- */
#define LED_HEARTBEAT_PERIOD_MS (1500)      /* 직립: 느린 심장박동 */
#define LED_BLINK_SLOW_MS       (500)       /* 전방 경사: 느린 깜빡임 */
#define LED_BLINK_FAST_MS       (200)       /* 후방 경사: 빠른 깜빡임 */

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/**
 * @brief 추론 결과를 담는 구조체
 */
typedef struct {
    uint8_t class_id;           /* 분류 결과 (0=직립, 1=전방, 2=후방) */
    float   confidence;         /* 최대 출력값 (0.0~1.0 범위가 아닌 raw score) */
    float   output[NN_OUTPUT_SIZE]; /* 출력층 raw 값 */
} InferenceResult_t;

/**
 * @brief USB 스트리밍용 데이터 구조체
 */
typedef struct __attribute__((packed)) {
    float pitch_deg;            /* 추정된 Pitch (deg) */
    float roll_deg;             /* 추정된 Roll (deg) */
    float pitch_rate_dps;       /* Pitch 각속도 (deg/s) */
    float class_id;             /* 분류 결과 (float으로 전송) */
    float confidence;           /* 신뢰도 */
} StreamData_t;

/**
 *-----------------------------------------------------------
 * PUBLIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */


/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

/* --- TSM 핸들 --- */
static XmTsmHandle_t s_tsm;

/* --- 자세 추정 값 (두 모드 공통으로 사용) --- */
static float s_pitch_deg;          /* 추정/수신된 Pitch 각도 (deg) */
static float s_roll_deg;           /* 추정/수신된 Roll 각도 (deg) */
static float s_pitch_rate_dps;     /* Pitch 각속도 (deg/s) */
static float s_roll_rate_dps;      /* Roll 각속도 (deg/s) */

#ifdef USE_IMU_HUB_FUSION
/* --- 상보 필터 초기화 플래그 (IMU Hub 모드 전용) --- */
static bool s_cf_initialized;
#endif

/* --- 추론 결과 --- */
static InferenceResult_t s_result;

/* --- 타이머 --- */
static uint32_t s_inference_timer;
static uint32_t s_debug_timer;

/* --- USB 스트리밍 데이터 --- */
static StreamData_t s_stream;

/* --- 이전 LED 상태 (불필요한 재설정 방지) --- */
static uint8_t s_prev_class_id = 255;

/*
 * ============================================================
 * Tiny Neural Network 가중치 (사전 학습된 값)
 * ============================================================
 *
 * [Python 학습 → C 배열 export 워크플로우]
 * 1. Python에서 학습 데이터 수집 (IMU pitch/roll/rate 로깅)
 * 2. TensorFlow/PyTorch로 모델 학습:
 *    model = Sequential([
 *        Dense(8, activation='relu', input_shape=(4,)),
 *        Dense(4, activation='relu'),
 *        Dense(3)  # logits, softmax는 argmax로 대체
 *    ])
 * 3. 학습된 가중치를 C 헤더로 export:
 *    for layer in model.layers:
 *        w, b = layer.get_weights()
 *        print(f"const float w[] = {{{', '.join(map(str, w.flatten()))}}};")
 *
 * 아래 값은 시뮬레이션용 하드코딩 가중치입니다.
 * 실제 사용 시 Python에서 학습한 가중치로 교체하세요.
 */

/* 은닉층 1 가중치: [4 x 8] = 32개 */
static const float s_w1[NN_INPUT_SIZE * NN_HIDDEN1_SIZE] = {
     0.45f, -0.32f,  0.18f,  0.67f, -0.21f,  0.53f, -0.44f,  0.11f,
    -0.38f,  0.72f, -0.15f,  0.29f,  0.61f, -0.47f,  0.33f, -0.56f,
     0.22f, -0.19f,  0.84f, -0.41f,  0.16f,  0.58f, -0.27f,  0.43f,
    -0.51f,  0.36f, -0.63f,  0.25f, -0.34f,  0.71f,  0.48f, -0.17f
};

/* 은닉층 1 바이어스: [8] */
static const float s_b1[NN_HIDDEN1_SIZE] = {
     0.10f, -0.05f,  0.08f, -0.12f,  0.03f,  0.15f, -0.07f,  0.02f
};

/* 은닉층 2 가중치: [8 x 4] = 32개 */
static const float s_w2[NN_HIDDEN1_SIZE * NN_HIDDEN2_SIZE] = {
     0.39f, -0.28f,  0.52f, -0.17f,
    -0.43f,  0.61f, -0.35f,  0.24f,
     0.31f, -0.54f,  0.46f, -0.22f,
    -0.37f,  0.48f, -0.19f,  0.63f,
     0.26f, -0.41f,  0.57f, -0.33f,
    -0.15f,  0.38f, -0.52f,  0.44f,
     0.49f, -0.23f,  0.36f, -0.58f,
    -0.31f,  0.55f, -0.42f,  0.27f
};

/* 은닉층 2 바이어스: [4] */
static const float s_b2[NN_HIDDEN2_SIZE] = {
     0.06f, -0.03f,  0.09f, -0.04f
};

/* 출력층 가중치: [4 x 3] = 12개 */
static const float s_w3[NN_HIDDEN2_SIZE * NN_OUTPUT_SIZE] = {
     0.74f, -0.58f, -0.31f,
    -0.42f,  0.65f, -0.27f,
     0.33f, -0.49f,  0.62f,
    -0.21f,  0.38f,  0.56f
};

/* 출력층 바이어스: [3] */
static const float s_b3[NN_OUTPUT_SIZE] = {
     0.15f, -0.08f, -0.05f
};

/* 자세 이름 문자열 (디버그 출력용) */
static const char* const s_posture_names[NN_OUTPUT_SIZE] = {
    "UPRIGHT",
    "FWD_LEAN",
    "BWD_LEAN"
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

/* --- 자세 데이터 갱신 --- */
#ifdef USE_H10_PRECOMPUTED
static void _UpdatePostureFromH10(void);
#endif

#ifdef USE_IMU_HUB_FUSION
static void _UpdatePostureFromImuHub(void);
#endif

/* --- Tiny Neural Network --- */
static void _RunInference(void);
static float _ReLU(float x);

/* --- LED / 디버그 출력 --- */
static void _UpdateLedByPosture(uint8_t class_id);
static void _PrintDebugInfo(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief Tiny AI 센서 퓨전 예제를 초기화합니다.
 * @details 부팅 시 한 번 호출됩니다. TSM을 생성하고 USB 스트리밍 소스를 등록합니다.
 */
void User_Setup(void)
{
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
        "[{\"name\":\"Pitch\",\"unit\":\"deg\"},"
        "{\"name\":\"Roll\",\"unit\":\"deg\"},"
        "{\"name\":\"Pitch Rate\",\"unit\":\"deg/s\"},"
        "{\"name\":\"Class ID\",\"unit\":\"-\"},"
        "{\"name\":\"Confidence\",\"unit\":\"-\"}]");
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
 * LED 1 Heartbeat으로 대기 상태 표시
 * ==================================================== */
static void Standby_Entry(void)
{
    /* LED 1: 대기 상태 표시 (느린 심장박동) */
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 2000);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    /* 자세 추정 값 초기화 */
    s_pitch_deg      = 0.0f;
    s_roll_deg       = 0.0f;
    s_pitch_rate_dps = 0.0f;
    s_roll_rate_dps  = 0.0f;

#ifdef USE_IMU_HUB_FUSION
    s_cf_initialized = false;
#endif

    s_prev_class_id = 255;

    XM_SetControlMode(XM_CTRL_MONITOR);
}

static void Standby_Loop(void)
{
    /* H10이 ASSIST 모드로 전환되면 ACTIVE로 진입 */
    if (XM.status.h10.h10Mode == XM_H10_MODE_ASSIST) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_ACTIVE);
    }
}

/* ====================================================
 * State: ACTIVE — 자세 데이터 갱신 + AI 추론 실행
 * 매 1ms: 자세 데이터 갱신 (H10 수신 또는 상보 필터)
 * 매 50ms: NN 추론 + LED 갱신
 * 매 500ms: USB CDC 디버그 출력
 * ==================================================== */
static void Active_Entry(void)
{
    /* 자세 추정 값 초기화 */
    s_pitch_deg      = 0.0f;
    s_roll_deg       = 0.0f;
    s_pitch_rate_dps = 0.0f;
    s_roll_rate_dps  = 0.0f;

#ifdef USE_IMU_HUB_FUSION
    s_cf_initialized = false;
#endif

    /* 추론 결과 초기화 */
    s_result.class_id = POSTURE_UPRIGHT;
    s_result.confidence = 0.0f;

    /* 타이머 초기화 */
    s_inference_timer = XM_GetTick();
    s_debug_timer = XM_GetTick();

    /* 모니터링 전용 — 토크 출력 없음 */
    XM_SetControlMode(XM_CTRL_MONITOR);

    /* LED 초기 상태 */
    s_prev_class_id = 255;

#ifdef USE_H10_PRECOMPUTED
    XM_SendUsbDebugMessage("[TinyAI] ACTIVE: H10 자세 데이터 + NN 분류 시작\r\n");
#else
    XM_SendUsbDebugMessage("[TinyAI] ACTIVE: IMU Hub 센서 퓨전 + NN 분류 시작\r\n");
#endif
}

static void Active_Loop(void)
{
    /* H10이 STANDBY로 돌아가면 STANDBY로 복귀 */
    if (XM.status.h10.h10Mode != XM_H10_MODE_ASSIST) {
        XM_TSM_TransitionTo(s_tsm, XM_STATE_STANDBY);
        return;
    }

    /* ------------------------------------------------
     * [매 1ms] 자세 데이터 갱신
     * ------------------------------------------------ */
#ifdef USE_H10_PRECOMPUTED
    /* H10 내부에서 이미 센서 퓨전 완료된 자세 데이터 직접 사용 */
    _UpdatePostureFromH10();
#endif

#ifdef USE_IMU_HUB_FUSION
    /* IMU Hub의 body-frame raw 데이터에 상보 필터 적용 */
    _UpdatePostureFromImuHub();
#endif

    /* ------------------------------------------------
     * [매 50ms] Tiny NN 추론 — 자세 변화가 느리므로 20Hz 충분
     * ------------------------------------------------
     * 매 루프(1ms)마다 추론하면 불필요한 연산 낭비.
     * 인체 자세 변화 대역폭은 ~5Hz이므로 20Hz 샘플링으로 충분.
     */
    uint32_t now = XM_GetTick();
    if (now - s_inference_timer >= INFERENCE_PERIOD_MS) {
        s_inference_timer = now;

        _RunInference();
        _UpdateLedByPosture(s_result.class_id);
    }

    /* ------------------------------------------------
     * USB 스트리밍 데이터 갱신 (매 1ms 주기)
     * ------------------------------------------------ */
    s_stream.pitch_deg      = s_pitch_deg;
    s_stream.roll_deg       = s_roll_deg;
    s_stream.pitch_rate_dps = s_pitch_rate_dps;
    s_stream.class_id       = (float)s_result.class_id;
    s_stream.confidence     = s_result.confidence;
    XM_SendUsbDataWithId(&s_stream, sizeof(s_stream), 0xF0);

    /* ------------------------------------------------
     * [매 500ms] USB CDC 디버그 메시지 출력
     * ------------------------------------------------ */
    if (now - s_debug_timer >= DEBUG_PRINT_PERIOD_MS) {
        s_debug_timer = now;
        _PrintDebugInfo();
    }
}

static void Active_Exit(void)
{
    /* LED 모두 끄기 */
    XM_SetLedState(XM_LED_1, XM_OFF);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    XM_SendUsbDebugMessage("[TinyAI] ACTIVE 종료\r\n");
}

/* ====================================================
 * Mode 1: H10 전처리 자세 데이터 사용
 * ====================================================
 * H10 내부에서 이미 센서 퓨전(가속도+자이로 → 자세 추정)이 완료된 값을
 * 그대로 가져옵니다. 추가 필터링이 필요 없어 간단하고 정확합니다.
 *
 * - SagittalPitch: 시상면(앞뒤) 기울기 (deg)
 * - FrontalRoll:   관상면(좌우) 기울기 (deg)
 * - GlobalGyrY:    글로벌 프레임 Pitch 각속도 (deg/s)
 * - GlobalGyrX:    글로벌 프레임 Roll 각속도 (deg/s)
 */
#ifdef USE_H10_PRECOMPUTED
static void _UpdatePostureFromH10(void)
{
    /* H10 내부에서 이미 센서 퓨전(가속도+자이로 → 자세 추정)이 완료된 값 */
    s_pitch_deg      = XM.status.h10.leftHipImuSagittalPitch;
    s_roll_deg       = XM.status.h10.leftHipImuFrontalRoll;
    s_pitch_rate_dps = XM.status.h10.leftHipImuGlobalGyrY;
    s_roll_rate_dps  = XM.status.h10.leftHipImuGlobalGyrX;
}
#endif

/* ====================================================
 * Mode 2: IMU Hub 상보 필터 (Complementary Filter)
 * ====================================================
 *
 * [이론적 배경]
 * 가속도계 기반 자세 추정 (body-frame 가속도, 단위: g):
 *   pitch_acc = atan2(acc_x, sqrt(acc_y^2 + acc_z^2)) * RAD_TO_DEG
 *   roll_acc  = atan2(acc_y, sqrt(acc_x^2 + acc_z^2)) * RAD_TO_DEG
 *   → 정적 상태에서 정확, 진동/충격 시 노이즈 큼
 *
 * 자이로 기반 자세 추정:
 *   pitch_gyro += gyro_y * dt
 *   → 단기간 정확, 장기간 드리프트 누적 (바이어스 x 시간)
 *
 * 상보 필터 (1차 High-Pass + Low-Pass 결합):
 *   pitch = ALPHA * (pitch + gyro_y * dt) + (1 - ALPHA) * pitch_acc
 *   ALPHA = 0.98 → 자이로 비중 98%, 가속도계 보정 2%
 *   → 차단 주파수: fc = 1 / (2 * pi * dt / (1 - ALPHA))
 *                      ≈ 1 / (2 * pi * 0.001 / 0.02) ≈ 3.2 Hz
 */
#ifdef USE_IMU_HUB_FUSION
static void _UpdatePostureFromImuHub(void)
{
    /* IMU Hub 연결 확인 — 미연결 시 갱신 생략 */
    if (!XM.status.imu_hub.is_connected) {
        return;
    }

    /* 센서 0번 포트의 body-frame 원시 데이터 읽기 */
    float acc_x = XM.status.imu_hub.sensor[IMU_HUB_SENSOR_IDX].acc_x;  /* (g) */
    float acc_y = XM.status.imu_hub.sensor[IMU_HUB_SENSOR_IDX].acc_y;  /* (g) */
    float acc_z = XM.status.imu_hub.sensor[IMU_HUB_SENSOR_IDX].acc_z;  /* (g) */
    float gyro_x = XM.status.imu_hub.sensor[IMU_HUB_SENSOR_IDX].gyr_x; /* Roll 축 (deg/s) */
    float gyro_y = XM.status.imu_hub.sensor[IMU_HUB_SENSOR_IDX].gyr_y; /* Pitch 축 (deg/s) */

    /* 자이로 각속도 저장 (NN 추론 입력으로 사용) */
    s_pitch_rate_dps = gyro_y;
    s_roll_rate_dps  = gyro_x;

    /* 가속도계 기반 Pitch/Roll 계산 (body-frame, 단위: g) */
    float acc_yz = sqrtf(acc_y * acc_y + acc_z * acc_z);
    float acc_xz = sqrtf(acc_x * acc_x + acc_z * acc_z);

    /* 0 나눗셈 방어 — 가속도 합이 거의 0이면 갱신 생략 */
    if (acc_yz < 0.01f && acc_xz < 0.01f) {
        return;
    }

    float pitch_acc = atan2f(acc_x, acc_yz) * RAD_TO_DEG;
    float roll_acc  = atan2f(acc_y, acc_xz) * RAD_TO_DEG;

    /* 최초 실행 시: 가속도계 값으로 초기화 (드리프트 없는 시작점) */
    if (!s_cf_initialized) {
        s_pitch_deg = pitch_acc;
        s_roll_deg  = roll_acc;
        s_cf_initialized = true;
        return;
    }

    /* 상보 필터 적용 — 1ms 주기 (CF_DT = 0.001s) */
    s_pitch_deg = CF_ALPHA * (s_pitch_deg + gyro_y * CF_DT)
                + (1.0f - CF_ALPHA) * pitch_acc;
    s_roll_deg  = CF_ALPHA * (s_roll_deg + gyro_x * CF_DT)
                + (1.0f - CF_ALPHA) * roll_acc;
}
#endif

/* ====================================================
 * Tiny Neural Network 추론
 * ====================================================
 *
 * [순전파(Forward Pass) 과정]
 * Layer 1: h1 = ReLU(W1 * input + b1)   [4->8]
 * Layer 2: h2 = ReLU(W2 * h1 + b2)      [8->4]
 * Layer 3: out = W3 * h2 + b3            [4->3] (활성화 없음, argmax로 분류)
 *
 * [ReLU 활성화 함수]
 * f(x) = max(0, x)
 * -> 음수 영역을 0으로 클리핑하여 비선형성 도입
 * -> 계산량 최소 (비교 1회), MCU에 최적
 *
 * [Softmax 대신 argmax 사용 이유]
 * Softmax: exp() 연산 필요 -> MCU에서 비용 큼 + 오버플로우 위험
 * argmax: 최대값 인덱스만 찾으면 분류 결과 동일
 * 신뢰도는 최대 출력값을 직접 사용 (확률 해석 불필요)
 */
static void _RunInference(void)
{
    /* 입력 벡터 구성 (두 모드 공통 — s_pitch/roll 변수에 이미 값이 준비됨) */
    float input[NN_INPUT_SIZE] = {
        s_pitch_deg,
        s_roll_deg,
        s_pitch_rate_dps,
        s_roll_rate_dps
    };

    /* --- Layer 1: 입력(4) -> 은닉1(8) + ReLU --- */
    float h1[NN_HIDDEN1_SIZE];
    for (int i = 0; i < NN_HIDDEN1_SIZE; i++) {
        float sum = s_b1[i];
        for (int j = 0; j < NN_INPUT_SIZE; j++) {
            sum += input[j] * s_w1[j * NN_HIDDEN1_SIZE + i];
        }
        h1[i] = _ReLU(sum);
    }

    /* --- Layer 2: 은닉1(8) -> 은닉2(4) + ReLU --- */
    float h2[NN_HIDDEN2_SIZE];
    for (int i = 0; i < NN_HIDDEN2_SIZE; i++) {
        float sum = s_b2[i];
        for (int j = 0; j < NN_HIDDEN1_SIZE; j++) {
            sum += h1[j] * s_w2[j * NN_HIDDEN2_SIZE + i];
        }
        h2[i] = _ReLU(sum);
    }

    /* --- Layer 3: 은닉2(4) -> 출력(3) (활성화 없음) --- */
    float output[NN_OUTPUT_SIZE];
    for (int i = 0; i < NN_OUTPUT_SIZE; i++) {
        float sum = s_b3[i];
        for (int j = 0; j < NN_HIDDEN2_SIZE; j++) {
            sum += h2[j] * s_w3[j * NN_OUTPUT_SIZE + i];
        }
        output[i] = sum;
    }

    /* --- argmax: 최대 출력값의 인덱스 = 분류 결과 --- */
    uint8_t best_class = 0;
    float best_score = output[0];
    for (int i = 1; i < NN_OUTPUT_SIZE; i++) {
        if (output[i] > best_score) {
            best_score = output[i];
            best_class = (uint8_t)i;
        }
    }

    /* 결과 저장 */
    s_result.class_id = best_class;
    s_result.confidence = best_score;
    for (int i = 0; i < NN_OUTPUT_SIZE; i++) {
        s_result.output[i] = output[i];
    }
}

/**
 * @brief ReLU 활성화 함수
 * @param x 입력값
 * @return max(0, x)
 */
static float _ReLU(float x)
{
    return (x > 0.0f) ? x : 0.0f;
}

/* ====================================================
 * LED 자세 표시
 * ====================================================
 * LED 1: 직립 (UPRIGHT) — Heartbeat (안정)
 * LED 2: 전방 경사 (FORWARD_LEAN) — Blink 느림
 * LED 3: 후방 경사 (BACKWARD_LEAN) — Blink 빠름
 */
static void _UpdateLedByPosture(uint8_t class_id)
{
    /* 이전과 동일하면 재설정 불필요 */
    if (class_id == s_prev_class_id) {
        return;
    }
    s_prev_class_id = class_id;

    /* 모든 LED 끄기 */
    XM_SetLedState(XM_LED_1, XM_OFF);
    XM_SetLedState(XM_LED_2, XM_OFF);
    XM_SetLedState(XM_LED_3, XM_OFF);

    /* 해당 자세의 LED만 켜기 */
    switch (class_id) {
        case POSTURE_UPRIGHT:
            XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, LED_HEARTBEAT_PERIOD_MS);
            break;
        case POSTURE_FORWARD_LEAN:
            XM_SetLedEffect(XM_LED_2, XM_LED_BLINK, LED_BLINK_SLOW_MS);
            break;
        case POSTURE_BACKWARD_LEAN:
            XM_SetLedEffect(XM_LED_3, XM_LED_BLINK, LED_BLINK_FAST_MS);
            break;
        default:
            /* 알 수 없는 클래스 — 모든 LED 끔 (방어적 처리) */
            break;
    }
}

/**
 * @brief USB CDC 디버그 메시지 출력 (500ms 주기)
 */
static void _PrintDebugInfo(void)
{
    char msg[128];

    /* 자세 분류 이름 안전 참조 */
    const char* posture_name = "UNKNOWN";
    if (s_result.class_id < NN_OUTPUT_SIZE) {
        posture_name = s_posture_names[s_result.class_id];
    }

    /* 형식: "AI | P:%.1f R:%.1f Class:%s Conf:%.1f%%\r\n" */
    snprintf(msg, sizeof(msg),
             "AI | P:%.1f R:%.1f Class:%s Conf:%.1f%%\r\n",
             s_pitch_deg,
             s_roll_deg,
             posture_name,
             s_result.confidence * 100.0f);

    XM_SendUsbDebugMessage(msg);
}
