/**
 ******************************************************************************
 * @file    agr_dop_coe_master.h
 * @author  HyundoKim
 * @brief   AGR DOP CoE Master Transport - EtherCAT Master (SOEM) 연동
 * @version 1.0
 * @date    Feb 26, 2026
 *
 * @details
 * CoE (CAN over EtherCAT) Master Transport 계층.
 * SOEM 라이브러리와 연동하여 EtherCAT Master에서 Slave OD에 접근합니다.
 *
 * [SOEM 연동 방식 — Dependency Injection]
 * SOEM ec_SDOread/write, ec_send/receive_processdata 등을 함수 포인터로 주입합니다.
 * AGR_DOP Core는 SOEM에 대한 직접 의존성이 없습니다.
 *
 * [CAN-FD Master와의 차이]
 * - CAN-FD: AGR_CANFD_SendSDO → CAN 프레임 전송
 * - CoE:    AGR_COE_Master_SDORead/Write → SOEM 함수 포인터 호출
 * - CAN-FD: AGR_CANFD_SendTxPDO → CAN 프레임 전송
 * - CoE:    AGR_COE_Master_ExchangePDO → SOEM IOmap + ec_send/receive
 *
 * [PDO 방향 주의 (Master 관점)]
 * - Slave TxPDO (Slave→Master) → Master가 읽기 → DecodeTxPDO
 * - Slave RxPDO (Master→Slave) → Master가 쓰기 → EncodeRxPDO
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef AGR_DOP_COE_MASTER_H
#define AGR_DOP_COE_MASTER_H

#include "agr_dop_types.h"
#include "agr_dop_config.h"

#if AGR_DOP_TRANSPORT_COE

#include "Core/agr_od.h"
#include "Core/agr_sdo_protocol.h"
#include "Core/agr_pdo_engine.h"

/**
 *-----------------------------------------------------------
 * MASTER-SPECIFIC TYPES (agr_dop_types.h 무변경)
 *-----------------------------------------------------------
 */

/**
 * @brief SOEM ec_SDOread 래핑 함수 포인터
 * @param slave    Slave 번호 (SOEM slave index)
 * @param index    Object Index
 * @param subindex Sub-Index
 * @param data     읽은 데이터 (출력)
 * @param size     데이터 크기 (입출력: 입력=버퍼크기, 출력=읽은크기)
 * @return >0=성공 (work counter), <=0=에러
 */
typedef int (*AGR_COE_MasterSDOReadFunc_t)(
    uint16_t slave, uint16_t index, uint8_t subindex,
    void* data, int* size);

/**
 * @brief SOEM ec_SDOwrite 래핑 함수 포인터
 * @param slave    Slave 번호
 * @param index    Object Index
 * @param subindex Sub-Index
 * @param data     쓸 데이터
 * @param size     데이터 크기
 * @return >0=성공 (work counter), <=0=에러
 */
typedef int (*AGR_COE_MasterSDOWriteFunc_t)(
    uint16_t slave, uint16_t index, uint8_t subindex,
    const void* data, int size);

/**
 * @brief SOEM ec_send/receive_processdata 래핑 함수 포인터
 * @param timeout_us PDO 교환 타임아웃 (microseconds)
 * @return Work Counter (expected_wkc 와 비교하여 검증)
 */
typedef int (*AGR_COE_MasterPDOExchangeFunc_t)(int timeout_us);

/**
 * @brief CoE Master Transport Context (AGR_DOP_Ctx_t 확장)
 *
 * @details
 * Master는 여러 Slave와 통신하므로, 각 Slave별 AGR_DOP_Ctx_t와 함께
 * SOEM 관련 함수 포인터와 IOmap 정보를 관리합니다.
 */
typedef struct {
    AGR_DOP_Ctx_t*                    dop_ctx;       /**< DOP Core Context */
    AGR_COE_MasterSDOReadFunc_t       sdo_read;      /**< SOEM SDO Read 함수 */
    AGR_COE_MasterSDOWriteFunc_t      sdo_write;     /**< SOEM SDO Write 함수 */
    AGR_COE_MasterPDOExchangeFunc_t   pdo_exchange;  /**< SOEM PDO Exchange 함수 */
    uint8_t*                          iomap;          /**< SOEM IOmap 포인터 */
    uint32_t                          iomap_size;     /**< IOmap 전체 크기 (bytes) */
} AGR_COE_MasterCtx_t;

/**
 *-----------------------------------------------------------
 * INITIALIZATION
 *-----------------------------------------------------------
 */

/**
 * @brief CoE Master Transport 초기화
 * @param mctx      Master Context
 * @param ctx       DOP Core Context
 * @param od        Object Dictionary 테이블
 * @param node_id   Master Node ID
 * @param sdo_read  SOEM SDO Read 함수 (Dependency Injection)
 * @param sdo_write SOEM SDO Write 함수 (Dependency Injection)
 * @return 0=성공, <0=에러
 */
int AGR_COE_Master_Init(AGR_COE_MasterCtx_t* mctx,
                        AGR_DOP_Ctx_t* ctx,
                        const AGR_OD_Table_t* od,
                        uint8_t node_id,
                        AGR_COE_MasterSDOReadFunc_t sdo_read,
                        AGR_COE_MasterSDOWriteFunc_t sdo_write);

/**
 * @brief CoE Master Transport 리셋
 * @param mctx Master Context
 */
void AGR_COE_Master_Reset(AGR_COE_MasterCtx_t* mctx);

/**
 *-----------------------------------------------------------
 * IOmap CONNECTION (SOEM ec_config_map 이후 호출)
 *-----------------------------------------------------------
 */

/**
 * @brief IOmap 연결 (SOEM ec_config_map() 완료 후 호출)
 * @param mctx Master Context
 * @param iomap SOEM IOmap 포인터
 * @param size  IOmap 전체 크기
 */
void AGR_COE_Master_SetIOmap(AGR_COE_MasterCtx_t* mctx,
                             uint8_t* iomap,
                             uint32_t size);

/**
 * @brief PDO Exchange 함수 설정
 * @param mctx         Master Context
 * @param pdo_exchange SOEM PDO Exchange 함수 (Dependency Injection)
 */
void AGR_COE_Master_SetPDOExchange(AGR_COE_MasterCtx_t* mctx,
                                   AGR_COE_MasterPDOExchangeFunc_t pdo_exchange);

/**
 *-----------------------------------------------------------
 * SDO TRANSPORT (SOEM 래핑, Non-RT에서 호출)
 *-----------------------------------------------------------
 */

/**
 * @brief SDO Read (SOEM ec_SDOread 래핑)
 * @param mctx     Master Context
 * @param slave    Slave 번호 (SOEM index)
 * @param index    Object Index
 * @param subindex Sub-Index
 * @param data     읽은 데이터 (출력)
 * @param size     데이터 크기 (입출력)
 * @return 0=성공, <0=에러
 *
 * @note Non-RT (PREOP/SAFEOP 단계에서 호출). RT Thread에서 사용 금지.
 */
int AGR_COE_Master_SDORead(AGR_COE_MasterCtx_t* mctx,
                           uint16_t slave,
                           uint16_t index,
                           uint8_t subindex,
                           void* data,
                           int* size);

/**
 * @brief SDO Write (SOEM ec_SDOwrite 래핑)
 * @param mctx     Master Context
 * @param slave    Slave 번호 (SOEM index)
 * @param index    Object Index
 * @param subindex Sub-Index
 * @param data     쓸 데이터
 * @param size     데이터 크기
 * @return 0=성공, <0=에러
 *
 * @note Non-RT (PREOP/SAFEOP 단계에서 호출). RT Thread에서 사용 금지.
 */
int AGR_COE_Master_SDOWrite(AGR_COE_MasterCtx_t* mctx,
                            uint16_t slave,
                            uint16_t index,
                            uint8_t subindex,
                            const void* data,
                            int size);

/**
 *-----------------------------------------------------------
 * PDO TRANSPORT (IOmap 기반, Slave PDO와 동일 Core 사용)
 *-----------------------------------------------------------
 */

/**
 * @brief Slave TxPDO 디코딩 (IOmap → Master OD)
 * @param ctx          Slave별 DOP Context (Slave의 OD 구조를 미러링)
 * @param iomap_region 해당 Slave의 IOmap Input 영역 포인터
 * @param size         데이터 크기
 * @return 0=성공, <0=에러
 *
 * @details
 * Slave가 보낸 TxPDO를 Master가 읽습니다 (Slave→Master 방향).
 * IOmap의 Input 영역에서 데이터를 언패킹하여 Master의 로컬 OD에 저장합니다.
 */
int AGR_COE_Master_DecodeTxPDO(AGR_DOP_Ctx_t* ctx,
                               const uint8_t* iomap_region,
                               uint8_t size);

/**
 * @brief Slave RxPDO 인코딩 (Master OD → IOmap)
 * @param ctx          Slave별 DOP Context
 * @param iomap_region 해당 Slave의 IOmap Output 영역 포인터
 * @param size         인코딩된 바이트 수 (출력)
 * @return 0=성공, <0=에러
 *
 * @details
 * Master가 Slave에게 보낼 RxPDO를 작성합니다 (Master→Slave 방향).
 * Master의 로컬 OD에서 데이터를 패킹하여 IOmap의 Output 영역에 씁니다.
 */
int AGR_COE_Master_EncodeRxPDO(AGR_DOP_Ctx_t* ctx,
                               uint8_t* iomap_region,
                               uint8_t* size);

/**
 *-----------------------------------------------------------
 * PDO EXCHANGE (전체 Slave 일괄, RT Thread에서 호출)
 *-----------------------------------------------------------
 */

/**
 * @brief 전체 Slave PDO 교환 (SOEM ec_send/receive_processdata 래핑)
 * @param mctx       Master Context
 * @param timeout_us PDO 교환 타임아웃 (microseconds)
 * @return Work Counter (expected_wkc와 비교하여 통신 상태 판단)
 *
 * @details
 * RT Thread에서 매 사이클(1ms) 호출합니다.
 * 1. SOEM pdo_exchange() 호출 (IOmap 기반 DMA 전송)
 * 2. 반환된 Work Counter로 통신 상태 판단
 *
 * WKC == expected_wkc → 정상
 * WKC < expected_wkc  → 일부 Slave 미응답
 * WKC == 0            → 전체 통신 실패
 */
int AGR_COE_Master_ExchangePDO(AGR_COE_MasterCtx_t* mctx,
                               int timeout_us);

#endif /* AGR_DOP_TRANSPORT_COE */

#endif /* AGR_DOP_COE_MASTER_H */
