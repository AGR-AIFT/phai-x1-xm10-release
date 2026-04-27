/**
 ******************************************************************************
 * @file    pnp_task.c
 * @author  HyundoKim
 * @brief   PnP Task 구현 - DOP V2 Master PnP + Device 통합 관리
 * @version 2.0.0
 * @date    2026-02-11
 *
 * @details
 * [V2.0 변경사항 - Link Layer 완전 제거]
 * - AS-IS: V1 Legacy Links (LinkModule_t, grf_xm_link, xsens_imu_xm_link)
 * - TO-BE: 모든 Device를 직접 호출 (동일 패턴)
 *
 * [PnP Task 주기 작업] (100ms)
 * ┌─────────────────────────────────────────────────────────┐
 * │ 1. AGR_PnP_Master_RunPeriodic()                        │
 * │    - Master Heartbeat 전송 (1초마다 1회)                │
 * │    - 모든 Slave Heartbeat Timeout 체크                  │
 * │                                                         │
 * │ 2. Protocol Devices RunPeriodic()                       │
 * │    - CM_Drv_RunPeriodic: DOP V1 Heartbeat + Timeout    │
 * │    - ImuHub_Drv_RunPeriodic: DOP V2 Pre-Op SM          │
 * │                                                         │
 * │ 3. Sensor Devices RunPeriodic() (Auto-Sense)            │
 * │    - MarvelDex_RunPeriodic: UART 데이터 타임아웃 체크    │
 * │    - XsensMTi_RunPeriodic: UART 데이터 타임아웃 체크     │
 * └─────────────────────────────────────────────────────────┘
 *
 * @copyright Copyright (c) 2025 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "pnp_task.h"

/* System Config */
#include "module.h"

/* Node IDs */
#include "agr_dop_node_id.h"

/* Protocol Device Drivers */
#include "cm_drv.h"
#include "imu_hub_drv.h"
#include "emg_hub_drv.h"
#include "fes_hub_drv.h"

/* Sensor Device Drivers (Auto-Sense) */
#include "mdaf-25-6850.h"
#include "mti-630.h"

/* USB CDC */
#include "cdc_handler.h"

/* FreeRTOS */
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */


/**
 *-----------------------------------------------------------
 * STATIC (PRIVATE) VARIABLES
 *-----------------------------------------------------------
 */

/* ===== DOP V2: Master PnP 인스턴스 (XM10 노드 단위 1개) ===== */
static AGR_PnP_Master_t s_master_pnp;
static bool s_master_pnp_initialized = false;

/* ===== Task ===== */
static osThreadId_t s_pnp_task_handle = NULL;

static const osThreadAttr_t s_pnp_task_attr = {
    .name = "PnP_Task",
    .stack_size = TASK_STACK_PNP_MANAGER,
    .priority = (osPriority_t)TASK_PRIO_PNP_MANAGER,
};

/**
 *-----------------------------------------------------------
 * STATIC (PRIVATE) FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

static void _PnP_TaskEntry(void* argument);

/**
 *-----------------------------------------------------------
 * PUBLIC FUNCTIONS
 *-----------------------------------------------------------
 */

/**
 * @brief PnP Task 초기화 (듀얼 CAN 채널)
 * @param sensor_tx_func  FDCAN2 전송 함수 (Sensor Hub Bus, V2 Master Heartbeat)
 * @param cm_tx_func      FDCAN1 전송 함수 (CM Bus, V1 통신)
 * @param get_tick         Tick 함수
 */
void PnP_Task_Init(AGR_PnP_TxFunc_t sensor_tx_func,
                    AGR_PnP_TxFunc_t cm_tx_func,
                    AGR_PnP_GetTickFunc_t get_tick)
{
    if (sensor_tx_func == NULL || cm_tx_func == NULL || get_tick == NULL) {
        return;
    }

    /* ===== 1. DOP V2: Master PnP 초기화 (FDCAN2 — Sensor Hub Bus) =====
     * Master Heartbeat(0x702)가 FDCAN2로 전송되어 Sensor Hub가 수신합니다.
     */
    if (!s_master_pnp_initialized) {
        int result = AGR_PnP_Master_Init(&s_master_pnp,
                                          AGR_NODE_ID_XM,
                                          sensor_tx_func,
                                          get_tick);
        if (result >= 0) {
            s_master_pnp_initialized = true;
        }
    }

    /* ===== 2. Protocol Device 초기화 (FDCAN1 — CM Bus) ===== */
    CM_Init(cm_tx_func, SYS_NODE_ID_XM, SYS_NODE_ID_CM);

    /* ===== 3. Sensor Device Auto-Sense 초기화 (Multi-Instance) ===== */
    for (uint8_t ch = 0; ch < MARVELDEX_MAX_INSTANCES; ch++) {
        MarvelDex_StateInit(ch);
    }
    XsensMTi_StateInit(0);  /* XM: XSENS ch=0 고정 */

    /* ===== 4. PnP Task 생성 (100ms 주기) ===== */
    s_pnp_task_handle = osThreadNew(_PnP_TaskEntry, NULL, &s_pnp_task_attr);
}

/**
 * @brief Master PnP 인스턴스 반환
 */
AGR_PnP_Master_t* PnP_Task_GetMaster(void)
{
    return s_master_pnp_initialized ? &s_master_pnp : NULL;
}

/**
 *-----------------------------------------------------------
 * STATIC (PRIVATE) FUNCTIONS
 *-----------------------------------------------------------
 */

/**
 * @brief PnP Task Entry Point (100ms 주기)
 *
 * @details
 * 모든 Device를 직접 호출합니다 (LinkModule_t 인터페이스 제거).
 *
 * [실행 순서]
 * 1. V2 Master PnP RunPeriodic (Heartbeat + Timeout)
 * 2. Protocol Devices RunPeriodic (CM, ImuHub)
 * 3. Sensor Devices RunPeriodic (GRF, XSENS - Auto-Sense 타임아웃)
 */
static void _PnP_TaskEntry(void* argument)
{
    (void)argument;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(TASK_PERIOD_MS_PNP_MANAGER);

    for (;;) {
        vTaskDelayUntil(&xLastWakeTime, xPeriod);

        /* ===== 1. V2 Master PnP RunPeriodic ===== */
        if (s_master_pnp_initialized) {
            AGR_PnP_Master_RunPeriodic(&s_master_pnp);
        }

        /* ===== 2. Protocol Devices RunPeriodic ===== */
        CM_Drv_RunPeriodic();
        ImuHub_Drv_RunPeriodic();
        EmgHub_Drv_RunPeriodic();
        FesHub_Drv_RunPeriodic();

        /* ===== 3. Sensor Devices RunPeriodic (Auto-Sense, Multi-Instance) =====
         * 데이터 타임아웃 체크: 500ms 동안 데이터 없으면 STOPPED 전환
         * Protocol PnP와 동일한 패턴으로 PnP Task에서 통합 관리
         */
        for (uint8_t ch = 0; ch < MARVELDEX_MAX_INSTANCES; ch++) {
            MarvelDex_RunPeriodic(ch);
        }
        XsensMTi_RunPeriodic(0);  /* XM: XSENS ch=0 고정 */

        /* ===== 4. USB CDC: DTR=0 미지원 호스트 fallback ===== */
        CdcStream_CheckForDisconnect();
    }
}
