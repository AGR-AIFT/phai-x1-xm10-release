/**
 ******************************************************************************
 * @file    debug_monitor.c
 * @author  HyundoKim
 * @brief   [고급] 시스템 디버깅 및 성능 모니터링
 * @details
 * 임베디드 시스템의 실시간 디버깅 기법을 다루는 예제입니다.
 *
 * [학습 목표]
 *   - 제어 루프 실행 시간 측정 및 통계 수집
 *   - USB CDC를 활용한 시스템 Health 대시보드
 *   - 진단 LED 패턴으로 시스템 상태 시각화
 *   - Watchdog 스타일 데이터 신선도(Staleness) 모니터링
 *
 * [기능 요약]
 *   1. 루프 실행 시간 측정 (Min/Max/Avg, 오버런 감지)
 *   2. 1초 주기 시스템 Health 대시보드 (USB CDC 출력)
 *   3. 진단 LED 패턴 (Heartbeat / CM 연결 / 버튼 피드백)
 *   4. CM 데이터 Staleness 감지 (5초 무변화 경고)
 *   5. 버튼 진단 (통계 리셋 / 상세 모드 / 시스템 덤프)
 *
 * [임베디드 디버깅 모범 사례]
 *   - printf 디버깅은 타이밍을 왜곡할 수 있으므로, 데이터를 모아서
 *     1초 단위로 출력하는 것이 실시간 시스템에 적합합니다.
 *   - LED 패턴은 USB 연결 없이도 시스템 상태를 파악할 수 있는
 *     가장 원시적이고 신뢰성 높은 디버깅 수단입니다.
 *   - 실행 시간 측정 시 XM_GetTick()은 ms 해상도이므로,
 *     1ms 루프에서는 0 또는 1 중 하나의 값만 관측됩니다.
 *     us 정밀도가 필요하면 DWT Cycle Counter를 사용해야 합니다.
 *
 * @see     docs/api-reference/system.md
 * @version 1.0
 * @date    Mar 09, 2026
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "xm_api.h"
#include <stdio.h>

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

/** 루프 오버런 임계값 (ms) — XM10 User_Loop 주기는 1ms */
#define LOOP_PERIOD_MS          (1U)

/** Health 대시보드 출력 주기 (ms) */
#define HEALTH_REPORT_INTERVAL  (1000U)

/** 통계 갱신 주기 (ms) — 1초마다 평균 계산 후 리셋 */
#define STATS_UPDATE_INTERVAL   (1000U)

/** CM 데이터 신선도 타임아웃 (ms) — 5초간 변화 없으면 경고 */
#define STALE_DATA_TIMEOUT_MS   (5000U)

/** 데이터 변화 판별 임계값 (도) — 부동소수 비교를 위한 epsilon */
#define ANGLE_CHANGE_EPSILON    (0.001f)

/** USB 출력 버퍼 크기 */
#define USB_BUF_SIZE            (256U)

/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** 루프 실행 시간 통계 구조체 */
typedef struct {
    uint32_t min_ms;            /**< 최소 실행 시간 (ms) */
    uint32_t max_ms;            /**< 최대 실행 시간 (ms) */
    uint32_t sum_ms;            /**< 누적 합계 (평균 계산용) */
    uint32_t sample_count;      /**< 샘플 수 (1초 구간 내) */
    float    avg_ms;            /**< 평균 실행 시간 (ms, 소수점 포함) */
} LoopStats_t;

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

static XmTsmHandle_t s_tsm;

/* 루프 실행 시간 측정 */
static LoopStats_t s_stats;
static uint32_t    s_loop_count    = 0;     /**< 전체 루프 실행 횟수 */
static uint32_t    s_overrun_count = 0;     /**< 오버런 발생 횟수 */

/* 타이머 */
static uint32_t s_last_stats_time  = 0;     /**< 통계 갱신 타이머 */
static uint32_t s_last_health_time = 0;     /**< Health 출력 타이머 */

/* CM 데이터 Staleness 감지 */
static float    s_prev_left_hip_angle = 0.0f;  /**< 이전 좌측 고관절 각도 */
static uint32_t s_last_data_change_time = 0;   /**< 마지막 데이터 변화 시각 */
static bool     s_is_data_stale = false;       /**< 데이터 신선도 플래그 */

/* 모드 플래그 */
static bool s_verbose_mode = false;         /**< 상세 출력 모드 (BTN2 토글) */

/* USB 출력 버퍼 */
static char s_usb_buf[USB_BUF_SIZE];

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void Run_Monitor(void);
static void _MeasureLoopTime(uint32_t elapsed_ms);
static void _UpdateStats(uint32_t now);
static void _PrintHealthDashboard(uint32_t now);
static void _UpdateDiagnosticLeds(void);
static void _CheckDataStaleness(uint32_t now);
static void _HandleButtons(void);
static void _ResetStats(void);
static void _PrintSystemDump(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

void User_Setup(void)
{
    /* 단일 상태 TSM — 디버그 모니터는 항상 동작해야 함 */
    s_tsm = XM_TSM_Create(XM_STATE_USER_START);
    XmStateConfig_t conf = { .id = XM_STATE_USER_START, .on_loop = Run_Monitor };
    XM_TSM_AddState(s_tsm, &conf);

    /* 통계 초기화 */
    _ResetStats();

    /* 초기 시각 설정 */
    uint32_t now = XM_GetTick();
    s_last_stats_time      = now;
    s_last_health_time     = now;
    s_last_data_change_time = now;

    /* 시작 LED: Heartbeat (시스템 정상) */
    XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);

    XM_SendUsbDebugMessage("[DEBUG] Monitor started. BTN1=Reset BTN2=Verbose BTN3(Long)=Dump\r\n");
}

void User_Loop(void)
{
    /*
     * [실행 시간 측정 전략]
     * XM_GetTick()은 ms 해상도이므로 1ms 루프에서는 정밀도가 제한됩니다.
     * 대부분의 루프에서 0ms로 측정되며, 가끔 1ms가 관측됩니다.
     * 이 제약을 이해하고 사용하는 것이 중요합니다.
     *
     * 더 정밀한 측정이 필요하면 DWT->CYCCNT (CPU 사이클 카운터)를
     * 사용해야 하지만, 이 예제에서는 XM API만 활용합니다.
     */
    uint32_t start_tick = XM_GetTick();

    XM_TSM_Run(s_tsm);

    uint32_t end_tick = XM_GetTick();
    uint32_t elapsed = end_tick - start_tick;

    _MeasureLoopTime(elapsed);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief 메인 모니터링 루프 — TSM 상태 콜백
 *
 * 모든 모니터링 기능을 순차적으로 실행합니다.
 * 각 기능은 자체 타이머로 실행 주기를 관리합니다.
 */
static void Run_Monitor(void)
{
    uint32_t now = XM_GetTick();

    /* 1초마다 통계 갱신 */
    _UpdateStats(now);

    /* 1초마다 Health 대시보드 출력 */
    _PrintHealthDashboard(now);

    /* CM 데이터 신선도 감시 */
    _CheckDataStaleness(now);

    /* 진단 LED 업데이트 */
    _UpdateDiagnosticLeds();

    /* 버튼 입력 처리 */
    _HandleButtons();
}

/**
 * @brief 루프 실행 시간 측정 및 오버런 감지
 * @param elapsed_ms 이번 루프의 실행 시간 (ms)
 *
 * [설계 의도]
 * ms 해상도의 한계로 인해 대부분 0ms로 측정됩니다.
 * 하지만 오버런(>1ms) 감지는 시스템 이상 징후를 포착하는 데 유용합니다.
 */
static void _MeasureLoopTime(uint32_t elapsed_ms)
{
    s_loop_count++;

    /* 통계 누적 */
    if (elapsed_ms < s_stats.min_ms) {
        s_stats.min_ms = elapsed_ms;
    }
    if (elapsed_ms > s_stats.max_ms) {
        s_stats.max_ms = elapsed_ms;
    }
    s_stats.sum_ms += elapsed_ms;
    s_stats.sample_count++;

    /* 오버런 감지: 루프 주기(1ms)를 초과하면 카운트 */
    if (elapsed_ms > LOOP_PERIOD_MS) {
        s_overrun_count++;
    }
}

/**
 * @brief 1초 주기 통계 갱신
 * @param now 현재 시각 (ms)
 *
 * 평균을 계산하고 구간 통계를 리셋합니다.
 * 평균값은 s_stats.avg_ms에 보존되어 Health 출력에 사용됩니다.
 */
static void _UpdateStats(uint32_t now)
{
    if ((now - s_last_stats_time) < STATS_UPDATE_INTERVAL) {
        return;
    }
    s_last_stats_time = now;

    /* 평균 계산 (0 나눗셈 방어) */
    if (s_stats.sample_count > 0) {
        s_stats.avg_ms = (float)s_stats.sum_ms / (float)s_stats.sample_count;
    } else {
        s_stats.avg_ms = 0.0f;
    }

    /* 구간 통계 리셋 (min/max/avg는 보존, 누적값만 리셋) */
    s_stats.sum_ms = 0;
    s_stats.sample_count = 0;
}

/**
 * @brief 시스템 Health 대시보드 — USB CDC 출력
 * @param now 현재 시각 (ms)
 *
 * [출력 형식]
 * 고정폭 정렬로 터미널에서 가독성을 확보합니다.
 * sprintf를 사용하므로 실시간성에 영향을 줄 수 있으나,
 * 1초 주기이므로 허용 가능한 수준입니다.
 */
static void _PrintHealthDashboard(uint32_t now)
{
    if ((now - s_last_health_time) < HEALTH_REPORT_INTERVAL) {
        return;
    }
    s_last_health_time = now;

    /* Uptime 계산 (시:분:초) */
    uint32_t uptime_sec = now / 1000U;
    uint32_t hours   = uptime_sec / 3600U;
    uint32_t minutes = (uptime_sec % 3600U) / 60U;
    uint32_t seconds = uptime_sec % 60U;

    /* Line 1: 기본 상태 */
    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[HEALTH] Uptime: %02lu:%02lu:%02lu | Loops: %lu | Overrun: %lu\r\n",
             (unsigned long)hours, (unsigned long)minutes, (unsigned long)seconds,
             (unsigned long)s_loop_count, (unsigned long)s_overrun_count);
    XM_SendUsbDebugMessage(s_usb_buf);

    /* Line 2: 루프 타이밍 */
    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[HEALTH] Loop Time -- Min: %lums  Max: %lums  Avg: %.1fms\r\n",
             (unsigned long)s_stats.min_ms, (unsigned long)s_stats.max_ms,
             s_stats.avg_ms);
    XM_SendUsbDebugMessage(s_usb_buf);

    /* Line 3: CM 연결 + 데이터 상태 */
    const char *cm_status = XM_IsCmConnected() ? "Connected" : "Disconnected";
    const char *data_status = s_is_data_stale ? "STALE" : "Fresh";
    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[HEALTH] CM: %-12s | Data: %-5s | LH Angle: %.2f deg\r\n",
             cm_status, data_status, XM.status.h10.leftHipAngle);
    XM_SendUsbDebugMessage(s_usb_buf);

    /* Line 4: ADC + 모드 */
    uint32_t adc1_mv = XM_AnalogReadMillivolts(XM_EXT_ADC_1);
    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[HEALTH] ADC1: %lumV | Verbose: %s\r\n",
             (unsigned long)adc1_mv, s_verbose_mode ? "ON" : "OFF");
    XM_SendUsbDebugMessage(s_usb_buf);

    /* 상세 모드: 추가 센서 데이터 출력 */
    if (s_verbose_mode) {
        snprintf(s_usb_buf, USB_BUF_SIZE,
                 "[DETAIL] LH: %.2f deg  RH: %.2f deg\r\n",
                 XM.status.h10.leftHipAngle,
                 XM.status.h10.rightHipAngle);
        XM_SendUsbDebugMessage(s_usb_buf);

        snprintf(s_usb_buf, USB_BUF_SIZE,
                 "[DETAIL] L Torque: %.3f Nm  R Torque: %.3f Nm\r\n",
                 XM.status.h10.leftHipTorque,
                 XM.status.h10.rightHipTorque);
        XM_SendUsbDebugMessage(s_usb_buf);

        snprintf(s_usb_buf, USB_BUF_SIZE,
                 "[DETAIL] L Motor: %.2f deg  R Motor: %.2f deg\r\n",
                 XM.status.h10.leftHipMotorAngle,
                 XM.status.h10.rightHipMotorAngle);
        XM_SendUsbDebugMessage(s_usb_buf);

        /* ADC 전체 채널 */
        snprintf(s_usb_buf, USB_BUF_SIZE,
                 "[DETAIL] ADC — CH1: %lumV  CH2: %lumV  CH3: %lumV  CH4: %lumV\r\n",
                 (unsigned long)XM_AnalogReadMillivolts(XM_EXT_ADC_1),
                 (unsigned long)XM_AnalogReadMillivolts(XM_EXT_ADC_2),
                 (unsigned long)XM_AnalogReadMillivolts(XM_EXT_ADC_3),
                 (unsigned long)XM_AnalogReadMillivolts(XM_EXT_ADC_4));
        XM_SendUsbDebugMessage(s_usb_buf);
    }

    /* 구분선 */
    XM_SendUsbDebugMessage("----------------------------------------------\r\n");
}

/**
 * @brief 진단 LED 패턴 업데이트
 *
 * [LED 매핑]
 * - LED 1: 시스템 상태 — 정상=Heartbeat, 오버런/STALE=빠른 Blink
 * - LED 2: CM 연결 상태 — 연결=ON, 미연결=OFF
 * - LED 3: 버튼 입력 피드백 — 아무 버튼이나 누르면 ONESHOT
 */
static void _UpdateDiagnosticLeds(void)
{
    /* LED 1: 시스템 상태 표시 */
    if ((s_overrun_count > 0) || s_is_data_stale) {
        /* 이상 상태: 빠른 Blink (200ms) */
        XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
    } else {
        /* 정상: 느린 Heartbeat (1초) */
        XM_SetLedEffect(XM_LED_1, XM_LED_HEARTBEAT, 1000);
    }

    /* LED 2: CM 연결 상태 */
    if (XM_IsCmConnected()) {
        XM_SetLedState(XM_LED_2, XM_ON);
    } else {
        XM_SetLedState(XM_LED_2, XM_OFF);
    }

    /* LED 3: 버튼 피드백 — 아무 버튼이나 누르면 깜빡 */
    if ((XM_GetButtonEvent(XM_BTN_1) != XM_BTN_NONE) ||
        (XM_GetButtonEvent(XM_BTN_2) != XM_BTN_NONE) ||
        (XM_GetButtonEvent(XM_BTN_3) != XM_BTN_NONE)) {
        XM_SetLedEffect(XM_LED_3, XM_LED_ONESHOT, 100);
    }
}

/**
 * @brief CM 데이터 Staleness(신선도) 감시
 * @param now 현재 시각 (ms)
 *
 * [Watchdog 패턴]
 * leftHipAngle 값이 5초간 변화하지 않으면 데이터가
 * "오래된(stale)" 것으로 판단합니다.
 *
 * [주의사항]
 * 부동소수점 비교 시 epsilon을 사용하여
 * 미세한 노이즈로 인한 오탐을 방지합니다.
 */
static void _CheckDataStaleness(uint32_t now)
{
    float current_angle = XM.status.h10.leftHipAngle;
    float delta = current_angle - s_prev_left_hip_angle;

    /* 절대값 비교 (fabsf 대신 수동 계산 — math.h 의존 제거) */
    if (delta < 0.0f) {
        delta = -delta;
    }

    if (delta > ANGLE_CHANGE_EPSILON) {
        /* 데이터 변화 감지 → 타이머 리셋 */
        s_last_data_change_time = now;
        s_prev_left_hip_angle = current_angle;

        if (s_is_data_stale) {
            s_is_data_stale = false;
            XM_SendUsbDebugMessage("[WATCH] Data refresh detected — STALE cleared.\r\n");
        }
    } else {
        /* 변화 없음 → 타임아웃 확인 */
        if ((!s_is_data_stale) && ((now - s_last_data_change_time) >= STALE_DATA_TIMEOUT_MS)) {
            s_is_data_stale = true;
            XM_SendUsbDebugMessage("[WATCH] WARNING: STALE DATA — No angle change for 5s!\r\n");
        }
    }
}

/**
 * @brief 버튼 입력 진단 처리
 *
 * [버튼 매핑]
 * - BTN1 클릭: 통계 리셋 (Min/Max/Avg/Overrun 초기화)
 * - BTN2 클릭: 상세 모드 토글 (추가 센서 데이터 출력 ON/OFF)
 * - BTN3 롱프레스: 전체 시스템 정보 덤프 (1회성)
 */
static void _HandleButtons(void)
{
    /* BTN1: 통계 리셋 */
    if (XM_GetButtonEvent(XM_BTN_1) == XM_BTN_CLICK) {
        _ResetStats();
        s_loop_count = 0;
        s_overrun_count = 0;
        s_is_data_stale = false;
        s_last_data_change_time = XM_GetTick();
        XM_SendUsbDebugMessage("[BTN1] Stats reset complete.\r\n");
    }

    /* BTN2: 상세 모드 토글 */
    if (XM_GetButtonEvent(XM_BTN_2) == XM_BTN_CLICK) {
        s_verbose_mode = !s_verbose_mode;
        snprintf(s_usb_buf, USB_BUF_SIZE,
                 "[BTN2] Verbose mode: %s\r\n", s_verbose_mode ? "ON" : "OFF");
        XM_SendUsbDebugMessage(s_usb_buf);
    }

    /* BTN3 롱프레스: 시스템 정보 덤프 */
    if (XM_GetButtonEvent(XM_BTN_3) == XM_BTN_LONG_PRESS) {
        _PrintSystemDump();
    }
}

/**
 * @brief 통계 구조체 초기화
 *
 * min_ms를 UINT32_MAX로 설정하여 첫 번째 측정값이
 * 항상 최소값으로 기록되도록 합니다.
 */
static void _ResetStats(void)
{
    s_stats.min_ms       = UINT32_MAX;
    s_stats.max_ms       = 0;
    s_stats.sum_ms       = 0;
    s_stats.sample_count = 0;
    s_stats.avg_ms       = 0.0f;
}

/**
 * @brief 전체 시스템 정보 덤프 (1회성 출력)
 *
 * BTN3 롱프레스 시 호출됩니다.
 * 시스템의 현재 상태를 종합적으로 출력합니다.
 */
static void _PrintSystemDump(void)
{
    XM_SendUsbDebugMessage("========== SYSTEM DUMP ==========\r\n");

    /* 기본 정보 */
    uint32_t now = XM_GetTick();
    uint32_t uptime_sec = now / 1000U;
    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[DUMP] Uptime: %lu sec (%lu ms raw)\r\n",
             (unsigned long)uptime_sec, (unsigned long)now);
    XM_SendUsbDebugMessage(s_usb_buf);

    /* 루프 통계 */
    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[DUMP] Total Loops: %lu | Overruns: %lu\r\n",
             (unsigned long)s_loop_count, (unsigned long)s_overrun_count);
    XM_SendUsbDebugMessage(s_usb_buf);

    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[DUMP] Loop Time — Min: %lums  Max: %lums  Avg: %.1fms\r\n",
             (unsigned long)s_stats.min_ms, (unsigned long)s_stats.max_ms,
             s_stats.avg_ms);
    XM_SendUsbDebugMessage(s_usb_buf);

    /* CM 상태 */
    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[DUMP] CM Connected: %s | Data Stale: %s\r\n",
             XM_IsCmConnected() ? "Yes" : "No",
             s_is_data_stale ? "Yes" : "No");
    XM_SendUsbDebugMessage(s_usb_buf);

    /* 로봇 데이터 */
    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[DUMP] LH Angle: %.3f deg | RH Angle: %.3f deg\r\n",
             XM.status.h10.leftHipAngle, XM.status.h10.rightHipAngle);
    XM_SendUsbDebugMessage(s_usb_buf);

    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[DUMP] LH Torque: %.3f Nm | RH Torque: %.3f Nm\r\n",
             XM.status.h10.leftHipTorque, XM.status.h10.rightHipTorque);
    XM_SendUsbDebugMessage(s_usb_buf);

    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[DUMP] L Motor: %.3f deg | R Motor: %.3f deg\r\n",
             XM.status.h10.leftHipMotorAngle, XM.status.h10.rightHipMotorAngle);
    XM_SendUsbDebugMessage(s_usb_buf);

    /* ADC 전체 채널 */
    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[DUMP] ADC — CH1: %lumV  CH2: %lumV  CH3: %lumV  CH4: %lumV\r\n",
             (unsigned long)XM_AnalogReadMillivolts(XM_EXT_ADC_1),
             (unsigned long)XM_AnalogReadMillivolts(XM_EXT_ADC_2),
             (unsigned long)XM_AnalogReadMillivolts(XM_EXT_ADC_3),
             (unsigned long)XM_AnalogReadMillivolts(XM_EXT_ADC_4));
    XM_SendUsbDebugMessage(s_usb_buf);

    /* 모드 상태 */
    snprintf(s_usb_buf, USB_BUF_SIZE,
             "[DUMP] Verbose: %s\r\n", s_verbose_mode ? "ON" : "OFF");
    XM_SendUsbDebugMessage(s_usb_buf);

    XM_SendUsbDebugMessage("========== END DUMP =============\r\n");
}
