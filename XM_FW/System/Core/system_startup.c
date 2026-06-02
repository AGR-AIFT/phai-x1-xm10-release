/**
 ******************************************************************************
 * @file    system_startup.c
 * @author  HyundoKim
 * @brief   시스템 초기화 및 부팅 시퀀스 관리
 * @version 0.1
 * @date    Oct 14, 2025
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "system_startup.h"

#include "stm32h7xx_hal.h"

#include "module.h"

#include "data_object_dictionaries.h"
#include "agr_dop_config.h"          /* AGR_CAN_ID_SYNC (0x080) */

// 모든 IOIF 헤더 포함
#include "ioif_agrb_fdcan.h"
#include "ioif_agrb_gpio.h"
#include "ioif_agrb_uart.h"
#include "ioif_agrb_adc.h"
#include "ioif_agrb_fs.h"
#include "ioif_agrb_tim.h"

// 생성할 시스템 서비스 헤더 포함
#include "pnp_task.h"           /* ✅ V4.0: PnP Task (PnPManager 대체) */
#include "canfd_rx_handler.h"
#include "usb_mode_handler.h"
#include "uart_rx_handler.h"
#include "data_logger.h"
#include "cdc_handler.h"
#include "led_manager.h"
#include "button_manager.h"
#include "external_io.h"

#include "imu_hub_drv.h"
#include "emg_hub_drv.h"
#include "fes_hub_drv.h"
#include "mti-630.h"

// RTOS 및 HAL 핸들 참조
// FreeRTOS 및 CMSIS-OS 관련 헤더
#include "cmsis_os2.h"
#include "main.h" // HAL 핸들(hfdcan1) 및 RTOS 태스크 핸들을 extern으로 참조

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PRIVATE ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * PULBIC (GLOBAL) VARIABLES
 *-----------------------------------------------------------
 */

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern DMA_HandleTypeDef hdma_adc1;
extern DMA_HandleTypeDef hdma_adc2;

extern FDCAN_HandleTypeDef hfdcan1;
extern FDCAN_HandleTypeDef hfdcan2;

extern I2C_HandleTypeDef hi2c1;

extern TIM_HandleTypeDef htim2;

// ✅ [Global Export] TIM2 IOIF ID (external_io.c에서 사용)
IOIF_TIMx_t g_tim2_id = IOIF_TIM_INVALID_ID;

extern UART_HandleTypeDef huart7;
extern UART_HandleTypeDef huart8;
extern DMA_HandleTypeDef hdma_uart7_rx;
extern DMA_HandleTypeDef hdma_uart8_rx;

extern osThreadId_t UserTaskHandle;
extern osThreadId_t StartupTaskHandle;

// On-board LEDs
IOIF_GPIOx_t  g_gpio_func_led1_id = IOIF_GPIO_NOT_INITIALIZED; // PB8
IOIF_GPIOx_t  g_gpio_func_led2_id = IOIF_GPIO_NOT_INITIALIZED; // PB9
IOIF_GPIOx_t  g_gpio_func_led3_id = IOIF_GPIO_NOT_INITIALIZED; // PB10

// On-board Buttons
IOIF_GPIOx_t  g_gpio_func_btn1_id = IOIF_GPIO_NOT_INITIALIZED; // PC11
IOIF_GPIOx_t  g_gpio_func_btn2_id = IOIF_GPIO_NOT_INITIALIZED; // PC12
IOIF_GPIOx_t  g_gpio_func_btn3_id = IOIF_GPIO_NOT_INITIALIZED; // PC13

/* 수동으로 관리할 UART4 핸들 */
UART_HandleTypeDef huart4_manual;
DMA_HandleTypeDef hdma_uart4_tx_manual;
DMA_HandleTypeDef hdma_uart4_rx_manual;

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *------------------------------------------------------------
 */

/* --- 핸들(ID) 저장을 위한 static 변수 --- */
// USB
static IOIF_ADCx_t   s_adc1_id = IOIF_ADC_NOT_INITIALIZED; // (hadc1: CC핀 + Ext A0/A1)
static IOIF_GPIOx_t  s_gpio_usb_pwr_id = IOIF_GPIO_NOT_INITIALIZED;
static IOIF_GPIOx_t  s_gpio_usb_vbus_id = IOIF_GPIO_NOT_INITIALIZED;
static IOIF_GPIOx_t  s_gpio_usb_ufp_id = IOIF_GPIO_NOT_INITIALIZED;
// USB 제어 태스크에 주입할 설정 구조체
static TaskUSBControlTask_Init_t s_usbControlInitStruct;

// CAN
static IOIF_FDCANx_t s_fdcan1_id = IOIF_FDCAN_INVALID_ID;  /* CM Bus (DOP V1) */
static IOIF_FDCANx_t s_fdcan2_id = IOIF_FDCAN_INVALID_ID;  /* Sensor Hub Bus (DOP V2) */
/* UART */
// static IOIF_UARTx_t  s_uart4_id = IOIF_UART_ID_NOT_ALLOCATED; // [Deprecated] IMU용 (external_io.c로 이동)
static IOIF_UARTx_t  s_uart7_id = IOIF_UART_ID_NOT_ALLOCATED;
static IOIF_UARTx_t  s_uart8_id = IOIF_UART_ID_NOT_ALLOCATED;

// On-board LEDs
static IOIF_GPIOx_t  s_gpio_pwr_led_id = IOIF_GPIO_NOT_INITIALIZED; // PC6
static IOIF_GPIOx_t  s_gpio_rgb_r_id = IOIF_GPIO_NOT_INITIALIZED;   // PC7
static IOIF_GPIOx_t  s_gpio_rgb_g_id = IOIF_GPIO_NOT_INITIALIZED;   // PC8
static IOIF_GPIOx_t  s_gpio_rgb_b_id = IOIF_GPIO_NOT_INITIALIZED;   // PC9

// External IO 핸들
static IOIF_GPIOx_t s_dio_ids[EXT_DIO_COUNT]; // 8개
static IOIF_ADCx_t  s_adc2_id = IOIF_ADC_NOT_INITIALIZED; // (hadc2: Ext A2/A3)
static IOIF_GPIOx_t s_gpio_ext_pwr_en_id = IOIF_GPIO_NOT_INITIALIZED; // PE4: EXT_PWR_EN

// Sensor Hub Modules Power Enable
static IOIF_GPIOx_t s_gpio_pwr_emg_id = IOIF_GPIO_NOT_INITIALIZED; // PG4
static IOIF_GPIOx_t s_gpio_pwr_fes_id = IOIF_GPIO_NOT_INITIALIZED; // PG6
static IOIF_GPIOx_t s_gpio_pwr_imu_id = IOIF_GPIO_NOT_INITIALIZED; // PG8
static IOIF_GPIOx_t s_gpio_pwr_hmmg_id = IOIF_GPIO_NOT_INITIALIZED; // PG10
static IOIF_GPIOx_t s_gpio_pwr_left_grf_id = IOIF_GPIO_NOT_INITIALIZED; // PG12
static IOIF_GPIOx_t s_gpio_pwr_right_grf_id = IOIF_GPIO_NOT_INITIALIZED; // PG14

/**
 *------------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

static void _InitIoInterfaces(void);
static void _InitSystemServices(void);

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief XM10 시스템의 모든 기반 서비스를 초기화하고 시작합니다.
 * @details main() 함수에서 RTOS 스케줄러가 시작되기 전에 단 한 번만 호출되어야 합니다.
 */
void System_Startup(void)
{
    // 1. 하위 드라이버(IOIF) 계층을 초기화합니다.
    //    이 단계에서 FDCAN 하드웨어가 준비되고, s_fdcan1_id가 발급됩니다.
    _InitIoInterfaces();

    // 2. 시스템 서비스 계층을 초기화합니다.
    //    이 단계에서 IOIF 드라이버에 콜백을 등록하고, 백그라운드 태스크를 생성합니다.
    _InitSystemServices();
}

/* ===== CiA 301 SYNC 전송 (Ch1: XM↔CM) =====
 * UserTask에서 호출: _FetchAllInputs() 직후, Read↔SYNC 동기화.
 * HAL 직접 호출 (IOIF Tx Mutex 우회) — UserTask는 단일 Tx 경로이므로 경합 없음.
 */
static uint8_t s_sync_counter_ch1 = 0;

void System_SendSync_Ch1(void)
{
    FDCAN_TxHeaderTypeDef txHeader = {
        .Identifier          = AGR_CAN_ID_SYNC,
        .IdType              = FDCAN_STANDARD_ID,
        .TxFrameType         = FDCAN_DATA_FRAME,
        .DataLength          = FDCAN_DLC_BYTES_1,
        .ErrorStateIndicator = FDCAN_ESI_ACTIVE,
        .BitRateSwitch       = FDCAN_BRS_OFF,
        .FDFormat            = FDCAN_CLASSIC_CAN,
        .TxEventFifoControl  = FDCAN_NO_TX_EVENTS,
        .MessageMarker       = 0
    };
    uint8_t data[1] = { ++s_sync_counter_ch1 };
    HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHeader, data);
}

/**
 * @brief 초기화된 FDCAN1의 IOIF 핸들(ID)을 반환합니다.
 * @details 다른 시스템 모듈(예: canfd_rx_handler)이 IOIF 드라이버에 접근하기 위해 사용합니다.
 * @return FDCAN1의 IOIF_FDCANx_t 핸들.
 */
IOIF_FDCANx_t System_GetFDCAN1_Id(void)
{
    return s_fdcan1_id;
}

/**
 * @brief 초기화된 FDCAN2의 IOIF 핸들(ID)을 반환합니다.
 * @details Sensor Hub Bus (DOP V2) 접근용.
 * @return FDCAN2의 IOIF_FDCANx_t 핸들.
 */
IOIF_FDCANx_t System_GetFDCAN2_Id(void)
{
    return s_fdcan2_id;
}

IOIF_GPIOx_t System_GetExtPwrEnGpioId(void)
{
    return s_gpio_ext_pwr_en_id;
}

/**
 * @brief FDCAN1 채널을 통해 CAN 메시지를 전송하는 래퍼 함수.
 * @param can_id CAN ID (11-bit 또는 29-bit)
 * @param data 전송할 데이터 포인터
 * @param len 데이터 길이 (0~64 bytes)
 * @return 0=성공, <0=에러
 * @note AGR_TxFunc_t 타입과 호환됩니다.
 * 
 * [Week 8] Tx FIFO 체크 추가 (CAN Bus Off 방지)
 */
/** @brief Tx 통계 - 추적할 상위 CAN ID 수 */
#define FDCAN_TX_STATS_TOP_COUNT    4

/** @brief Tx 통계 - Burst 감지 구간 (ms) */
#define FDCAN_TX_STATS_BURST_MS     100

/** @brief Tx 통계 - FIFO 초기 여유 공간 */
#define FDCAN_TX_FIFO_INITIAL_FREE  32

/**
 * @brief [Debug] Tx 통계 구조체
 */
typedef struct {
    uint32_t total_calls;        /**< 총 호출 횟수 */
    uint32_t fifo_full_count;    /**< FIFO Full 발생 횟수 */
    uint32_t hal_error_count;    /**< HAL 기타 에러 횟수 (Bus Off 등) */
    uint32_t success_count;      /**< 전송 성공 횟수 */
    uint32_t last_can_id;        /**< 마지막 전송 CAN ID */
    uint32_t last_hal_error;     /**< 마지막 HAL 에러 코드 */
    uint32_t fifo_free_level;    /**< 현재 FIFO 여유 공간 */
    uint32_t min_fifo_level;     /**< 최소 FIFO 여유 (최대 부하 추적) */

    struct {
        uint32_t can_id;
        uint32_t count;
    } top_tx[FDCAN_TX_STATS_TOP_COUNT];
    
    /* 100ms 동안의 전송 횟수 (Burst 감지) */
    uint32_t burst_count;
    uint32_t last_reset_ms;
} Fdcan_TxStats_t;

static Fdcan_TxStats_t s_fdcan1_tx_stats = {0};

/**
 * @brief CAN ID별 전송 횟수 업데이트
 */
static void _UpdateTxStats(Fdcan_TxStats_t* stats, uint32_t can_id)
{
    for (int i = 0; i < FDCAN_TX_STATS_TOP_COUNT; i++) {
        if (stats->top_tx[i].can_id == can_id) {
            stats->top_tx[i].count++;
            return;
        }
    }
    for (int i = 0; i < FDCAN_TX_STATS_TOP_COUNT; i++) {
        if (stats->top_tx[i].count == 0) {
            stats->top_tx[i].can_id = can_id;
            stats->top_tx[i].count = 1;
            return;
        }
    }
}

int System_Fdcan1_Transmit(uint32_t can_id, const uint8_t* data, uint8_t len)
{
    /* ✅ [Debug] Tx 통계 업데이트 */
    s_fdcan1_tx_stats.total_calls++;
    s_fdcan1_tx_stats.last_can_id = can_id;
    
    /* FIFO 여유 공간 확인 (통계용, 전송은 막지 않음) */
    uint32_t free_level = IOIF_FDCAN_GetTxFifoFreeLevel(s_fdcan1_id);
    s_fdcan1_tx_stats.fifo_free_level = free_level;
    
    /* 최소 여유 공간 추적 (최대 부하 감지) */
    if (s_fdcan1_tx_stats.min_fifo_level == 0) {
        s_fdcan1_tx_stats.min_fifo_level = FDCAN_TX_FIFO_INITIAL_FREE;
    }
    if (free_level < s_fdcan1_tx_stats.min_fifo_level) {
        s_fdcan1_tx_stats.min_fifo_level = free_level;
    }
    
    /* CAN ID별 전송 횟수 업데이트 */
    _UpdateTxStats(&s_fdcan1_tx_stats, can_id);
    
    /* 100ms 동안 Burst 감지 */
    uint32_t current_ms = IOIF_TIM_GetTick();
    if (current_ms - s_fdcan1_tx_stats.last_reset_ms >= FDCAN_TX_STATS_BURST_MS) {
        s_fdcan1_tx_stats.burst_count = 0;
        s_fdcan1_tx_stats.last_reset_ms = current_ms;
    }
    s_fdcan1_tx_stats.burst_count++;
    
    /* Tx 전송 (HAL이 내부에서 FIFO Full 체크함) */
    AGRBStatusDef result = IOIF_FDCAN_Transmit(s_fdcan1_id, can_id, (uint8_t*)data, len);
    
    if (result == AGRBStatus_OK) {
        s_fdcan1_tx_stats.success_count++;
        return 0;
    } else {
        /* 에러 분류 */
        if (free_level == 0) {
            /* FIFO Full (최초 체크 값 사용) */
            s_fdcan1_tx_stats.fifo_full_count++;
        } else {
            /* 기타 HAL 에러 (Bus Off, Error Passive 등) */
            s_fdcan1_tx_stats.hal_error_count++;
            s_fdcan1_tx_stats.last_hal_error = (uint32_t)result;
        }
        return -1;
    }
}

/**
 * @brief [Debug] FDCAN2 Tx 통계 구조체
 */
static Fdcan_TxStats_t s_fdcan2_tx_stats = {0};


/**
 * @brief FDCAN2 채널을 통해 CAN 메시지를 전송하는 래퍼 함수.
 * @details Sensor Hub Bus (DOP V2) 전용. AGR_TxFunc_t 타입과 호환.
 */
int System_Fdcan2_Transmit(uint32_t can_id, const uint8_t* data, uint8_t len)
{
    s_fdcan2_tx_stats.total_calls++;
    s_fdcan2_tx_stats.last_can_id = can_id;

    uint32_t free_level = IOIF_FDCAN_GetTxFifoFreeLevel(s_fdcan2_id);
    s_fdcan2_tx_stats.fifo_free_level = free_level;

    if (s_fdcan2_tx_stats.min_fifo_level == 0) {
        s_fdcan2_tx_stats.min_fifo_level = FDCAN_TX_FIFO_INITIAL_FREE;
    }
    if (free_level < s_fdcan2_tx_stats.min_fifo_level) {
        s_fdcan2_tx_stats.min_fifo_level = free_level;
    }

    _UpdateTxStats(&s_fdcan2_tx_stats, can_id);

    uint32_t current_ms = IOIF_TIM_GetTick();
    if (current_ms - s_fdcan2_tx_stats.last_reset_ms >= FDCAN_TX_STATS_BURST_MS) {
        s_fdcan2_tx_stats.burst_count = 0;
        s_fdcan2_tx_stats.last_reset_ms = current_ms;
    }
    s_fdcan2_tx_stats.burst_count++;

    AGRBStatusDef result = IOIF_FDCAN_Transmit(s_fdcan2_id, can_id, (uint8_t*)data, len);

    if (result == AGRBStatus_OK) {
        s_fdcan2_tx_stats.success_count++;
        return 0;
    } else {
        if (free_level == 0) {
            s_fdcan2_tx_stats.fifo_full_count++;
        } else {
            s_fdcan2_tx_stats.hal_error_count++;
            s_fdcan2_tx_stats.last_hal_error = (uint32_t)result;
        }
        return -1;
    }
}

/**
 * @brief [RTOS 태스크] "강한(strong)" 정의의 StartupTask 구현부.
 * @details main.c에서 생성된 __weak StartStartupTask를 덮어씁니다.
 * 시스템 초기화를 총괄하고, 완료되면 다른 태스크를 깨운 뒤 자신을 삭제합니다.
 */
void StartStartupTask(void *argument)
{
    // 1. 모든 드라이버, 서비스, IOIF를 초기화하는 메인 함수 호출
    System_Startup();

    // 2. 스케줄러를 중지하여 다른 태스크들을 원자적(atomic)으로 재개
    vTaskSuspendAll();

    // 3. main.c에서 생성된 다른 태스크들을 재개(Resume)
    //    (main.c의 `RTOS_THREADS` 영역 초기에 Task 생성 후, kernel 시작 전 osThreadSuspend(UserTaskHandle) 등이 추가되어야 함)
    if (UserTaskHandle != NULL) {
        osThreadResume(UserTaskHandle);
    }
    // (다른 Application Task가 있다면 여기서 마저 재개)

    // 4. 스케줄러를 다시 시작합니다.
    xTaskResumeAll();

    // 5. StartupTask는 임무를 완수했으므로 스스로를 삭제합니다.
    vTaskDelete(NULL);
}

/**
 *------------------------------------------------------------
 * STATIC FUNCTIONS
 *------------------------------------------------------------
 */

/**
 * @brief IOIF 계층을 통해 보드의 모든 주변 장치 드라이버를 초기화합니다.
 * @details 이 함수는 RTOS에 의존하지 않으며, 순수 하드웨어 드라이버만 초기화합니다.
 */
static void _InitIoInterfaces(void)
{
    // --- On-board Button IOIF 초기화 ---
    // Active-low button: released=HIGH, pressed=LOW.
    IOIF_GPIO_Initialize_t button_config = {
      .mode = IOIF_GPIO_Mode_Input,
      .pull = IOIF_GPIO_PullUp,
    };

    // Function Buttons (PC11, PC12, PC13)
    IOIF_GPIO_INITIALIZE(g_gpio_func_btn1_id, FUNC_BTN_1_GPIO_Port, FUNC_BTN_1_Pin, IOIF_GPIO_Mode_Input);
    IOIF_GPIO_REINITIALIZE(g_gpio_func_btn1_id, &button_config);
    
    IOIF_GPIO_INITIALIZE(g_gpio_func_btn2_id, FUNC_BTN_2_GPIO_Port, FUNC_BTN_2_Pin, IOIF_GPIO_Mode_Input);
    IOIF_GPIO_REINITIALIZE(g_gpio_func_btn2_id, &button_config);
    
    IOIF_GPIO_INITIALIZE(g_gpio_func_btn3_id, FUNC_BTN_3_GPIO_Port, FUNC_BTN_3_Pin, IOIF_GPIO_Mode_Input);
    IOIF_GPIO_REINITIALIZE(g_gpio_func_btn3_id, &button_config);

    // Power LED (PC6)
    IOIF_GPIO_INITIALIZE(s_gpio_pwr_led_id, POWER_ON_LED_GPIO_Port, POWER_ON_LED_Pin, IOIF_GPIO_Mode_Output);
    
    // RGB LED (PC7, PC8, PC9)
    IOIF_GPIO_INITIALIZE(s_gpio_rgb_r_id, CM_LED_R_GPIO_Port, CM_LED_R_Pin, IOIF_GPIO_Mode_Output);
    IOIF_GPIO_INITIALIZE(s_gpio_rgb_g_id, CM_LED_G_GPIO_Port, CM_LED_G_Pin, IOIF_GPIO_Mode_Output);
    IOIF_GPIO_INITIALIZE(s_gpio_rgb_b_id, CM_LED_B_GPIO_Port, CM_LED_B_Pin, IOIF_GPIO_Mode_Output);

    // Function LEDs (PB8, PB9, PB10)
    IOIF_GPIO_INITIALIZE(g_gpio_func_led1_id, FUNC_LED_1_GPIO_Port, FUNC_LED_1_Pin, IOIF_GPIO_Mode_Output);
    IOIF_GPIO_INITIALIZE(g_gpio_func_led2_id, FUNC_LED_2_GPIO_Port, FUNC_LED_2_Pin, IOIF_GPIO_Mode_Output);
    IOIF_GPIO_INITIALIZE(g_gpio_func_led3_id, FUNC_LED_3_GPIO_Port, FUNC_LED_3_Pin, IOIF_GPIO_Mode_Output);

    // FDCAN1 IOIF 드라이버 초기화
    // 1. main.c에서 초기화된 HAL 핸들(&hfdcan1)을 IOIF 계층에 '할당(ASSIGN)'하여 ID(s_fdcan1_id)를 발급받습니다.
    if (IOIF_FDCAN_ASSIGN(s_fdcan1_id, &hfdcan1) == AGRBStatus_OK) {
        /*
         * [FDCAN1 Hardware Filter - Rev1.1: V1(CM) + V2(Sensor Hub) 공존]
         *
         * Rev1.1 HW에서는 FDCAN1 단일 버스로 CM과 Sensor Hub 모두 통신합니다.
         * Rev2.0에서는 FDCAN1=CM(V1), FDCAN2=Sensor Hub(V2)로 분리됩니다.
         *
         * [수신 대상 - DOP V1 (CM)]
         * - CM → XM SDO: 0x212 (0x200 + CM(1)<<4 + XM(2))
         * - CM → XM PDO: 0x312 (0x300 + CM(1)<<4 + XM(2))
         *
         * [수신 대상 - DOP V2 (Sensor Hub, Node 0x08~0x0F)]
         * - Heartbeat/Boot-up: 0x708~0x70F
         * - SDO Response: 0x588~0x58F
         * - TPDO1: 0x188~0x18F
         * - TPDO2: 0x288~0x28F
         *
         * [차단 대상 - Hardware Level]
         * - MD (Node 0x07): XM10과 직접 통신 안함
         */
        FDCAN_FilterTypeDef filter_config;

        /* Filter 0: DOP V1 CM Messages (0x212, 0x312) - Dual Exact Match */
        filter_config = (FDCAN_FilterTypeDef){
            .IdType = FDCAN_STANDARD_ID,
            .FilterIndex = 0,
            .FilterType = FDCAN_FILTER_DUAL,
            .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
            .FilterID1 = 0x212,  /* CM → XM SDO */
            .FilterID2 = 0x312,  /* CM → XM PDO */
        };
        IOIF_FDCAN_ConfigFilter(s_fdcan1_id, &filter_config);

        /* Filter 1: DOP V2 Heartbeat/Boot-up (0x708 ~ 0x70F) */
        filter_config = (FDCAN_FilterTypeDef){
            .IdType = FDCAN_STANDARD_ID,
            .FilterIndex = 1,
            .FilterType = FDCAN_FILTER_RANGE,
            .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
            .FilterID1 = 0x708,  /* Node 8 (IMU Hub) */
            .FilterID2 = 0x70F,  /* Node 15 (확장 예비) */
        };
        IOIF_FDCAN_ConfigFilter(s_fdcan1_id, &filter_config);

        /* Filter 2: DOP V2 SDO Response (0x588 ~ 0x58F) */
        filter_config = (FDCAN_FilterTypeDef){
            .IdType = FDCAN_STANDARD_ID,
            .FilterIndex = 2,
            .FilterType = FDCAN_FILTER_RANGE,
            .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
            .FilterID1 = 0x588,
            .FilterID2 = 0x58F,
        };
        IOIF_FDCAN_ConfigFilter(s_fdcan1_id, &filter_config);

        /* Filter 3: DOP V2 TPDO1 (0x188 ~ 0x18F) */
        filter_config = (FDCAN_FilterTypeDef){
            .IdType = FDCAN_STANDARD_ID,
            .FilterIndex = 3,
            .FilterType = FDCAN_FILTER_RANGE,
            .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
            .FilterID1 = 0x188,
            .FilterID2 = 0x18F,
        };
        IOIF_FDCAN_ConfigFilter(s_fdcan1_id, &filter_config);

        /* Filter 4: DOP V2 TPDO2 (0x288 ~ 0x28F) */
        filter_config = (FDCAN_FilterTypeDef){
            .IdType = FDCAN_STANDARD_ID,
            .FilterIndex = 4,
            .FilterType = FDCAN_FILTER_RANGE,
            .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
            .FilterID1 = 0x288,
            .FilterID2 = 0x28F,
        };
        IOIF_FDCAN_ConfigFilter(s_fdcan1_id, &filter_config);
    }

#if 0  /* ===== Rev2.0 전용: FDCAN2 Sensor Hub Bus (Rev1.1에서는 미사용) ===== */
    if (IOIF_FDCAN_ASSIGN(s_fdcan2_id, &hfdcan2) == AGRBStatus_OK) {
        /*
         * [FDCAN2 Hardware Filter - DOP V2 전용 (Sensor Hub Bus)]
         *
         * Rev2.0 HW에서는 센서 모듈(IMU Hub, EMG Hub, FES Hub 등)이
         * FDCAN2 전용 버스로 분리됩니다.
         *
         * [필터 전략]
         * - Filter 0: Heartbeat/Boot-up Range (0x708~0x70F)
         * - Filter 1: SDO Response Range (0x588~0x58F)
         * - Filter 2: TPDO1 Range (0x188~0x18F)
         * - Filter 3: TPDO2 Range (0x288~0x28F)
         * - Filter 4: NMT Broadcast (0x000) + SYNC (0x080)
         */
        FDCAN_FilterTypeDef filter_config;

        /* Filter 0: Heartbeat/Boot-up (0x708 ~ 0x70F) */
        filter_config = (FDCAN_FilterTypeDef){
            .IdType = FDCAN_STANDARD_ID,
            .FilterIndex = 0,
            .FilterType = FDCAN_FILTER_RANGE,
            .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
            .FilterID1 = 0x708,
            .FilterID2 = 0x70F,
        };
        IOIF_FDCAN_ConfigFilter(s_fdcan2_id, &filter_config);

        /* Filter 1: SDO Response (0x588 ~ 0x58F) */
        filter_config = (FDCAN_FilterTypeDef){
            .IdType = FDCAN_STANDARD_ID,
            .FilterIndex = 1,
            .FilterType = FDCAN_FILTER_RANGE,
            .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
            .FilterID1 = 0x588,
            .FilterID2 = 0x58F,
        };
        IOIF_FDCAN_ConfigFilter(s_fdcan2_id, &filter_config);

        /* Filter 2: TPDO1 (0x188 ~ 0x18F) */
        filter_config = (FDCAN_FilterTypeDef){
            .IdType = FDCAN_STANDARD_ID,
            .FilterIndex = 2,
            .FilterType = FDCAN_FILTER_RANGE,
            .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
            .FilterID1 = 0x188,
            .FilterID2 = 0x18F,
        };
        IOIF_FDCAN_ConfigFilter(s_fdcan2_id, &filter_config);

        /* Filter 3: TPDO2 (0x288 ~ 0x28F) */
        filter_config = (FDCAN_FilterTypeDef){
            .IdType = FDCAN_STANDARD_ID,
            .FilterIndex = 3,
            .FilterType = FDCAN_FILTER_RANGE,
            .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
            .FilterID1 = 0x288,
            .FilterID2 = 0x28F,
        };
        IOIF_FDCAN_ConfigFilter(s_fdcan2_id, &filter_config);

        /* Filter 4: NMT Broadcast (0x000) + SYNC (0x080) */
        filter_config = (FDCAN_FilterTypeDef){
            .IdType = FDCAN_STANDARD_ID,
            .FilterIndex = 4,
            .FilterType = FDCAN_FILTER_DUAL,
            .FilterConfig = FDCAN_FILTER_TO_RXFIFO0,
            .FilterID1 = 0x000,
            .FilterID2 = 0x080,
        };
        IOIF_FDCAN_ConfigFilter(s_fdcan2_id, &filter_config);
    }
#endif  /* Rev2.0 FDCAN2 */

    /* [중요] FDCAN START는 RX 콜백 등록 후에 수행 (StartupTask에서) */

    // --- USB 제어용 IOIF 초기화 (main.c에서 이동) ---
    // ADC1 (CC핀 2개 + 외부핀 2개) 그룹을 ID 1개로 할당
    IOIF_ADC_INITIALIZE(s_adc1_id, &hadc1);
    IOIF_GPIO_INITIALIZE(s_gpio_usb_pwr_id, USB_PWR_ON_GPIO_Port, USB_PWR_ON_Pin, IOIF_GPIO_Mode_Output);
    IOIF_GPIO_INITIALIZE(s_gpio_usb_ufp_id, USB_OTG_UFP_ID_GPIO_Port, USB_OTG_UFP_ID_Pin, IOIF_GPIO_Mode_Output);
    IOIF_GPIO_INITIALIZE(s_gpio_usb_vbus_id, USB_OTG_FS_VBUS_GPIO_Port, USB_OTG_FS_VBUS_Pin, IOIF_GPIO_Mode_Input);

    // GPIO Re-Initialize (DFP 모드)
    IOIF_GPIO_Initialize_t ufp_config = {
      .mode = IOIF_GPIO_Mode_Output,
      .pull = IOIF_GPIO_Floating,
      .init_state = false //초기 DFP 모드
    };
    IOIF_GPIO_REINITIALIZE(s_gpio_usb_ufp_id, &ufp_config);

    // --- UART IOIF 초기화 (main.c에서 이동) ---
    // FSR용 UART7, UART8 설정 (Idle Event 모드)
    IOIF_UART_Config_t fsr_config = {
		  .baudrate = IOIF_UART_Baudrate_921600,
          .rxMode = IOIF_UART_MODE_IDLE_EVENT,
		  .bounce_buffer_size = 512,
		  .rx_event_callback = NULL, 
    };
    IOIF_UART_AssignInstance(&s_uart7_id, &huart7, &fsr_config);
    IOIF_UART_AssignInstance(&s_uart8_id, &huart8, &fsr_config);

    // --- External DIO 핀 IOIF 초기화 (PF3 ~ PF10) ---
    IOIF_GPIO_INITIALIZE(s_dio_ids[EXT_DIO_1], EXT_GPIO_1_GPIO_Port, EXT_GPIO_1_Pin, IOIF_GPIO_Mode_Input);
    IOIF_GPIO_INITIALIZE(s_dio_ids[EXT_DIO_2], EXT_GPIO_2_GPIO_Port, EXT_GPIO_2_Pin, IOIF_GPIO_Mode_Input);
    IOIF_GPIO_INITIALIZE(s_dio_ids[EXT_DIO_3], EXT_GPIO_3_GPIO_Port, EXT_GPIO_3_Pin, IOIF_GPIO_Mode_Input);
    IOIF_GPIO_INITIALIZE(s_dio_ids[EXT_DIO_4], EXT_GPIO_4_GPIO_Port, EXT_GPIO_4_Pin, IOIF_GPIO_Mode_Input);
    IOIF_GPIO_INITIALIZE(s_dio_ids[EXT_DIO_5], EXT_GPIO_5_GPIO_Port, EXT_GPIO_5_Pin, IOIF_GPIO_Mode_Input);
    IOIF_GPIO_INITIALIZE(s_dio_ids[EXT_DIO_6], EXT_GPIO_6_GPIO_Port, EXT_GPIO_6_Pin, IOIF_GPIO_Mode_Input);
    IOIF_GPIO_INITIALIZE(s_dio_ids[EXT_DIO_7], EXT_GPIO_7_GPIO_Port, EXT_GPIO_7_Pin, IOIF_GPIO_Mode_Input);
    IOIF_GPIO_INITIALIZE(s_dio_ids[EXT_DIO_8], EXT_GPIO_8_GPIO_Port, EXT_GPIO_8_Pin, IOIF_GPIO_Mode_Input);
    
    // --- External ADC 핀 IOIF 초기화 (PA0, PA0_C, PA1, PA1_C) ---
    // ADC2 (외부핀 2개) 그룹을 ID 1개로 할당
    IOIF_ADC_INITIALIZE(s_adc2_id, &hadc2);

    // --- TIM2 IOIF 초기화 (ADC2/ADC3 트리거용, 10kHz) ---
    // ✅ CubeMX에서 생성된 TIM2를 IOIF에 할당 (external_io.c에서 사용)
    IOIF_TIM_AssignInstance(&g_tim2_id, &htim2);

    // --- Sensor Hub Module Power Enable 핀 IOIF 초기화 (PG4, PG6, PG8, PG10, PG12, PG14) ---
    IOIF_GPIO_INITIALIZE(s_gpio_pwr_emg_id, EMG_PWR_EN_GPIO_Port, EMG_PWR_EN_Pin, IOIF_GPIO_Mode_Output);
	IOIF_GPIO_INITIALIZE(s_gpio_pwr_fes_id, FES_PWR_EN_GPIO_Port, FES_PWR_EN_Pin, IOIF_GPIO_Mode_Output);
	IOIF_GPIO_INITIALIZE(s_gpio_pwr_imu_id, IMU_PWR_EN_GPIO_Port, IMU_PWR_EN_Pin, IOIF_GPIO_Mode_Output);
	IOIF_GPIO_INITIALIZE(s_gpio_pwr_hmmg_id, HMMG_PWR_EN_GPIO_Port, HMMG_PWR_EN_Pin, IOIF_GPIO_Mode_Output);
	IOIF_GPIO_INITIALIZE(s_gpio_pwr_left_grf_id, L_GRF_PWR_EN_GPIO_Port, L_GRF_PWR_EN_Pin, IOIF_GPIO_Mode_Output);
	IOIF_GPIO_INITIALIZE(s_gpio_pwr_right_grf_id, R_GRF_PWR_EN_GPIO_Port, R_GRF_PWR_EN_Pin, IOIF_GPIO_Mode_Output);

    // --- Extension Port Power Enable (PE4) ---
    IOIF_GPIO_INITIALIZE(s_gpio_ext_pwr_en_id, EXT_PWR_EN_GPIO_Port, EXT_PWR_EN_Pin, IOIF_GPIO_Mode_Output);
}

/**
 * @brief (RTOS)시스템의 안정적인 동작을 위해 백그라운드에서 실행될 모든
 * 서비스 모듈(태스크 포함)을 초기화하고 생성합니다.
 * IOIF로 생성된 메커니즘의 내용을 채우는 정책을 결정하는 공간
 */
static void _InitSystemServices(void)
{
    // --- LED 매니저 서비스 초기화 ---
	LedManager_InitLinkStatusLeds(s_gpio_rgb_r_id, s_gpio_rgb_g_id, s_gpio_rgb_b_id);
    LedManager_InitUserLeds(g_gpio_func_led1_id, g_gpio_func_led2_id, g_gpio_func_led3_id);

    // --- Button 매니저 서비스 초기화 ---
    ButtonManager_Init(g_gpio_func_btn1_id, g_gpio_func_btn2_id, g_gpio_func_btn3_id);

    /* ===== FDCAN Rx 핸들러 초기화 (통합 Architecture) =====
     * [V5.0] FDCAN1(CM) + FDCAN2(Sensor Hub) 듀얼 채널 지원
     * - FDCAN1: DOP V1 (CM) 메시지 수신
     * - FDCAN2: DOP V2 (Sensor Hub) 메시지 수신
     * - 동일한 _ClassifyAndRoute() 로직으로 V1/V2 자동 판별
     */
    FDCANRxHandler_Init();

    /* FDCAN 시작 — RX 콜백 등록 후 수행 */
    IOIF_FDCAN_START(s_fdcan1_id);   /* Rev1.1: V1(CM) + V2(Sensor Hub) 공존 */
#if 0  /* Rev2.0: FDCAN2 Sensor Hub Bus 별도 시작 */
    IOIF_FDCAN_START(s_fdcan2_id);
#endif

    /* ===== PnP Task 초기화 (Master PnP + V1 CM + V2 Device 통합 관리) =====
     *
     * Rev1.1: FDCAN1 단일 버스 — sensor_tx와 cm_tx 모두 FDCAN1 사용
     * Rev2.0: sensor_tx=FDCAN2, cm_tx=FDCAN1로 분리 예정
     *
     * @note Device Driver Init()보다 먼저 호출해야 합니다.
     */
    PnP_Task_Init(System_Fdcan1_Transmit, System_Fdcan1_Transmit, IOIF_TIM_GetTick);

    /* ===== DOP V2 Device Driver 초기화 =====
     * Rev1.1: FDCAN1 단일 버스 (Sensor Hub와 CM 공존)
     * Rev2.0: FDCAN2 전용 버스로 분리 예정
     */
    AGR_PnP_Master_t* master_pnp = PnP_Task_GetMaster();
    if (master_pnp != NULL) {
        ImuHub_Drv_Init(System_Fdcan1_Transmit, master_pnp);
        EmgHub_Drv_Init(System_Fdcan1_Transmit, master_pnp);
        FesHub_Drv_Init(System_Fdcan1_Transmit, master_pnp);
    }

    /* 향후 확장: FSR, GRF Device 추가 */
    // GrfHub_Drv_Init(System_Fdcan1_Transmit, master_pnp);

    // --- USB 제어 서비스 초기화 (main.c에서 이동) ---
    // 1. USB 제어 태스크에 전달할 설정 구조체(의존성)를 채웁니다.
    s_usbControlInitStruct.cc_id = s_adc1_id;
    s_usbControlInitStruct.enable_id = s_gpio_usb_pwr_id;
    s_usbControlInitStruct.vbus_id = s_gpio_usb_vbus_id;
    s_usbControlInitStruct.ufp_id = s_gpio_usb_ufp_id;
    s_usbControlInitStruct.enable_host_mode = true;
    s_usbControlInitStruct.enable_device_mode = true;

    // 2. USB 제어 모듈을 초기화합니다. (이 함수가 내부적으로 태스크를 생성)
    USBControl_Init(&s_usbControlInitStruct);

    // --- UART 수신 핸들러 초기화 (main.c에서 이동) ---
    // (이 함수가 내부적으로 Rx 태스크를 생성)
    UartRxHandler_Init(s_uart7_id, s_uart8_id);

    // DataLogger가 사용하기 전에 FileSystem 모듈(Mutex 등)을 먼저 초기화합니다.
    // 이 호출은 링커가 ioif_agrb_fs.c를 포함하도록 보장합니다.
    ioif_filesystem_init(IOIF_FileSystem_DeviceType_Auto);

    // --- USB MSC Data Logger 서비스 초기화 ---
    // (이 함수가 내부적으로 저순위 로깅 태스크를 생성)
    DataLogger_Init();

    // --- USB CDC Data Streaming 서비스 초기화 ---
    CdcStream_Init();

    // --- TIM2 시작 (ADC2/ADC3 트리거용, 10kHz) ---
    // ✅ External IO 초기화 **전에** TIM2를 시작해야 ADC2가 정상 동작!
    // ✅ ADC2는 TIM2 트리거에 의존하므로 타이머 먼저 시작 필수
    IOIF_TIM_StartBase(g_tim2_id);

    // --- External IO 서비스 초기화 (ID 주입) ---
    // External IO 모듈에는 adc1과 adc2의 ID를 모두 주입
    ExternalIO_Init(s_dio_ids, s_adc1_id, s_adc2_id);

    // --- Power On LED 켜기 ---
    // 모든 초기화가 완료되었으므로 Power LED(PC6)를 켬
    IOIF_GPIO_SET(s_gpio_pwr_led_id);

    // --- Sensor Module Power Enable ---
    IOIF_GPIO_SET(s_gpio_pwr_emg_id); // PG4
    IOIF_GPIO_SET(s_gpio_pwr_fes_id); // PG6
    IOIF_GPIO_SET(s_gpio_pwr_imu_id); // PG8
    IOIF_GPIO_SET(s_gpio_pwr_hmmg_id); // PG10
    IOIF_GPIO_SET(s_gpio_pwr_left_grf_id); // PG12
    IOIF_GPIO_SET(s_gpio_pwr_right_grf_id); // PG14
}
