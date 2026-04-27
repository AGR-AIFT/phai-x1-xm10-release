/**
 ******************************************************************************
 * @file    agr_sdo_protocol.c
 * @author  HyundoKim
 * @brief   AGR SDO Protocol - Core Implementation (Transport-Agnostic)
 * @version 3.0
 * @date    Feb 25, 2026
 *
 * @details
 * CAN-ID / Transport에 의존하지 않는 순수 SDO 프로토콜 구현입니다.
 * 모든 함수는 AGR_OD_Table_t 또는 AGR_SDO_Msg_t를 직접 받습니다.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "agr_sdo_protocol.h"
#include <string.h>

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS
 *-----------------------------------------------------------
 */

/** @brief SDO 헤더 크기 (cs + index + subindex) */
#define SDO_HEADER_SIZE     4

/**
 *-----------------------------------------------------------
 * STATIC FUNCTION PROTOTYPES
 *-----------------------------------------------------------
 */

static int _ProcessSDOReadRequest(const AGR_OD_Table_t* od,
                                  const AGR_SDO_Msg_t* req,
                                  AGR_SDO_Msg_t* out_rsp);

static int _ProcessSDOWriteRequest(const AGR_OD_Table_t* od,
                                   const AGR_SDO_Msg_t* req,
                                   AGR_SDO_Msg_t* out_rsp);

/*============================================================
 * SDO MESSAGE CREATE
 *============================================================*/

void AGR_SDO_CreateReadReq(AGR_SDO_Msg_t* out_msg,
                           uint16_t index,
                           uint8_t subindex)
{
    if (out_msg == NULL) {
        return;
    }
    
    memset(out_msg, 0, sizeof(AGR_SDO_Msg_t));
    out_msg->cs = AGR_SDO_CS_UPLOAD_INIT_REQ;
    out_msg->index = index;
    out_msg->subindex = subindex;
    out_msg->data_len = 0;
}

void AGR_SDO_CreateWriteReq(AGR_SDO_Msg_t* out_msg,
                            uint16_t index,
                            uint8_t subindex,
                            const void* data,
                            uint8_t data_len)
{
    if (out_msg == NULL) {
        return;
    }
    
    memset(out_msg, 0, sizeof(AGR_SDO_Msg_t));
    out_msg->cs = AGR_SDO_CS_DOWNLOAD_INIT_REQ;
    out_msg->index = index;
    out_msg->subindex = subindex;
    
    if (data != NULL && data_len > 0) {
        uint8_t copy_len = (data_len > AGR_SDO_MAX_DATA_SIZE) ? AGR_SDO_MAX_DATA_SIZE : data_len;
        memcpy(out_msg->data, data, copy_len);
        out_msg->data_len = copy_len;
    }
}

void AGR_SDO_CreateAbortResponse(AGR_SDO_Msg_t* out_rsp,
                                 uint16_t index,
                                 uint8_t subindex,
                                 AGR_SDO_AbortCode_t abort_code)
{
    if (out_rsp == NULL) {
        return;
    }
    
    out_rsp->cs = AGR_SDO_CS_ABORT;
    out_rsp->index = index;
    out_rsp->subindex = subindex;
    
    out_rsp->data[0] = (uint8_t)(abort_code & 0xFF);
    out_rsp->data[1] = (uint8_t)((abort_code >> 8) & 0xFF);
    out_rsp->data[2] = (uint8_t)((abort_code >> 16) & 0xFF);
    out_rsp->data[3] = (uint8_t)((abort_code >> 24) & 0xFF);
    out_rsp->data_len = 4;
}

/*============================================================
 * SDO PROCESS (Slave-side)
 *============================================================*/

int AGR_SDO_ProcessRequest(const AGR_OD_Table_t* od,
                           const AGR_SDO_Msg_t* req,
                           AGR_SDO_Msg_t* out_rsp)
{
    if (od == NULL || req == NULL || out_rsp == NULL) {
        return -1;
    }
    
    switch (req->cs & 0xE0) {
        case 0x40:  /* Upload Initiate (Read) */
            return _ProcessSDOReadRequest(od, req, out_rsp);
            
        case 0x20:  /* Download Initiate (Write) */
            return _ProcessSDOWriteRequest(od, req, out_rsp);
            
        case 0x80:  /* Abort */
            return 0;
            
        default:
            AGR_SDO_CreateAbortResponse(out_rsp, req->index, req->subindex,
                                        AGR_SDO_ABORT_INVALID_CS);
            return -2;
    }
}

/*============================================================
 * SDO ENCODE / DECODE
 *============================================================*/

int AGR_SDO_Encode(const AGR_SDO_Msg_t* msg, uint8_t* out_buf)
{
    if (msg == NULL || out_buf == NULL) {
        return -1;
    }
    
    out_buf[0] = msg->cs;
    out_buf[1] = (uint8_t)(msg->index & 0xFF);
    out_buf[2] = (uint8_t)((msg->index >> 8) & 0xFF);
    out_buf[3] = msg->subindex;
    
    if (msg->data_len > 0) {
        memcpy(&out_buf[4], msg->data, msg->data_len);
    }
    
    /*
     * [CANopen 표준]
     * - Expedited Upload Response (cs & 0x02): DLC = 8 (고정)
     * - Non-Expedited Upload Response (cs == 0x41): DLC = 4 + data_len
     * - Download Response (cs & 0xE0 == 0x60): DLC = 8 (고정)
     * - Abort (cs == 0x80): DLC = 8 (고정)
     */
    if ((msg->cs & 0xE0) == 0x60 || msg->cs == 0x80) {
        return 8;  /* Download Response / Abort: 항상 8B */
    }
    if ((msg->cs & 0xE0) == 0x40) {
        if (msg->cs & 0x02) {
            return 8;  /* Expedited Upload Response: 8B 고정 */
        }
        return SDO_HEADER_SIZE + msg->data_len;  /* Non-Expedited: 가변 길이 */
    }

    return SDO_HEADER_SIZE + msg->data_len;
}

int AGR_SDO_Decode(const uint8_t* in_buf, uint8_t in_len, AGR_SDO_Msg_t* out_msg)
{
    if (in_buf == NULL || out_msg == NULL) {
        return -1;
    }
    
    if (in_len < SDO_HEADER_SIZE) {
        return -2;
    }
    
    memset(out_msg, 0, sizeof(AGR_SDO_Msg_t));
    
    out_msg->cs = in_buf[0];
    out_msg->index = (uint16_t)in_buf[1] | ((uint16_t)in_buf[2] << 8);
    out_msg->subindex = in_buf[3];
    
    /*
     * [CANopen SDO Upload Response 처리]
     * - Expedited (cs bit1 set):     data_len = 4 - n, where n = (cs >> 2) & 0x03
     * - Non-Expedited (cs bit1 clear): data_len = in_len - 4 (가변 길이)
     * - 다른 메시지 (Write/Abort 등): in_len - SDO_HEADER_SIZE
     */
    if ((out_msg->cs & 0xE0) == 0x40) {
        if (out_msg->cs & 0x02) {
            /* Expedited: n 비트에서 data_len 추출 */
            uint8_t n = (out_msg->cs >> 2) & 0x03;
            out_msg->data_len = 4 - n;
            if (out_msg->data_len > 4) {
                out_msg->data_len = 4;
            }
        } else {
            /* Non-Expedited: 가변 길이 (CAN-FD/Serial) */
            out_msg->data_len = in_len - SDO_HEADER_SIZE;
            if (out_msg->data_len > AGR_SDO_MAX_DATA_SIZE) {
                out_msg->data_len = AGR_SDO_MAX_DATA_SIZE;
            }
        }
    } else {
        out_msg->data_len = in_len - SDO_HEADER_SIZE;
        if (out_msg->data_len > AGR_SDO_MAX_DATA_SIZE) {
            out_msg->data_len = AGR_SDO_MAX_DATA_SIZE;
        }
    }
    
    if (out_msg->data_len > 0) {
        memcpy(out_msg->data, &in_buf[4], out_msg->data_len);
    }
    
    return 0;
}

/*============================================================
 * STATIC FUNCTIONS
 *============================================================*/

/**
 * @brief SDO Read 요청 처리 (CANopen Upload Initiate)
 */
static int _ProcessSDOReadRequest(const AGR_OD_Table_t* od,
                                  const AGR_SDO_Msg_t* req,
                                  AGR_SDO_Msg_t* out_rsp)
{
    const AGR_OD_Entry_t* entry = AGR_OD_FindEntryEx(od, req->index, req->subindex);
    
    if (entry == NULL) {
        AGR_SDO_CreateAbortResponse(out_rsp, req->index, req->subindex,
                                    AGR_SDO_ABORT_NOT_EXIST);
        return -1;
    }
    
    if (entry->access == AGR_ACCESS_WO) {
        AGR_SDO_CreateAbortResponse(out_rsp, req->index, req->subindex,
                                    AGR_SDO_ABORT_WRITE_ONLY);
        return -2;
    }
    
    out_rsp->index = req->index;
    out_rsp->subindex = req->subindex;
    
    int read_len = AGR_OD_ReadValue(entry, out_rsp->data, AGR_SDO_MAX_DATA_SIZE);
    if (read_len < 0) {
        AGR_SDO_CreateAbortResponse(out_rsp, req->index, req->subindex,
                                    AGR_SDO_ABORT_GENERAL);
        return -3;
    }
    
    out_rsp->data_len = (uint8_t)read_len;

    if (read_len <= 4) {
        /* Expedited Transfer (CiA 301: ≤4B, 단일 프레임) */
        uint8_t n = 4 - read_len;
        out_rsp->cs = AGR_SDO_CS_UPLOAD_EXP(n);
    } else {
        /* Non-Expedited Transfer (CiA 301: >4B, CAN-FD/Serial 단일 프레임) */
        out_rsp->cs = 0x41;  /* Upload Initiate Response, size indicated, NOT expedited */
    }

    return 0;
}

/**
 * @brief SDO Write 요청 처리 (CANopen Download Initiate)
 */
static int _ProcessSDOWriteRequest(const AGR_OD_Table_t* od,
                                   const AGR_SDO_Msg_t* req,
                                   AGR_SDO_Msg_t* out_rsp)
{
    const AGR_OD_Entry_t* entry = AGR_OD_FindEntryEx(od, req->index, req->subindex);
    
    if (entry == NULL) {
        AGR_SDO_CreateAbortResponse(out_rsp, req->index, req->subindex,
                                    AGR_SDO_ABORT_NOT_EXIST);
        return -1;
    }
    
    if (entry->access == AGR_ACCESS_RO) {
        AGR_SDO_CreateAbortResponse(out_rsp, req->index, req->subindex,
                                    AGR_SDO_ABORT_READ_ONLY);
        return -2;
    }
    
    int result = AGR_OD_WriteValue(entry, req->data, req->data_len);
    if (result < 0) {
        AGR_SDO_CreateAbortResponse(out_rsp, req->index, req->subindex,
                                    AGR_SDO_ABORT_GENERAL);
        return -3;
    }
    
    out_rsp->cs = AGR_SDO_CS_DOWNLOAD_INIT_RSP;
    out_rsp->index = req->index;
    out_rsp->subindex = req->subindex;
    out_rsp->data_len = 0;
    
    return 0;
}
