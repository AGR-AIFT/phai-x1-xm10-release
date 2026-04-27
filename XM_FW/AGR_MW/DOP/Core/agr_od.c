/**
 ******************************************************************************
 * @file    agr_od.c
 * @author  HyundoKim
 * @brief   AGR Object Dictionary - Core Implementation
 * @version 3.0
 * @date    Feb 25, 2026
 *
 * @details
 * DOP Context 독립적인 순수 OD 조작 함수 구현입니다.
 * 모든 함수는 AGR_OD_Table_t 또는 AGR_OD_Entry_t를 직접 받습니다.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "agr_od.h"
#include <string.h>

/*============================================================
 * OD LOOKUP
 *============================================================*/

const AGR_OD_Entry_t* AGR_OD_FindEntry(const AGR_OD_Table_t* od,
                                       uint16_t index)
{
    if (od == NULL || od->entries == NULL) {
        return NULL;
    }

    for (uint16_t i = 0; i < od->entry_count; i++) {
        if (od->entries[i].index == index) {
            return &od->entries[i];
        }
    }

    return NULL;
}

const AGR_OD_Entry_t* AGR_OD_FindEntryEx(const AGR_OD_Table_t* od,
                                         uint16_t index,
                                         uint8_t subindex)
{
    if (od == NULL || od->entries == NULL) {
        return NULL;
    }

    for (uint16_t i = 0; i < od->entry_count; i++) {
        if (od->entries[i].index == index &&
            od->entries[i].subindex == subindex) {
            return &od->entries[i];
        }
    }

    return NULL;
}

/*============================================================
 * OD VALUE ACCESS
 *============================================================*/

int AGR_OD_ReadValue(const AGR_OD_Entry_t* entry,
                     void* out_buf,
                     uint8_t buf_len)
{
    if (entry == NULL || out_buf == NULL) {
        return -1;
    }

    if (entry->data_ptr == NULL) {
        return -2;
    }

    if (entry->access == AGR_ACCESS_WO) {
        return -3;
    }

    uint8_t copy_len = (buf_len < entry->size) ? buf_len : entry->size;
    memcpy(out_buf, entry->data_ptr, copy_len);

    return copy_len;
}

int AGR_OD_WriteValue(const AGR_OD_Entry_t* entry,
                      const void* in_buf,
                      uint8_t in_len)
{
    if (entry == NULL || in_buf == NULL) {
        return -1;
    }

    if (entry->data_ptr == NULL) {
        return -2;
    }

    if (entry->access == AGR_ACCESS_RO) {
        return -3;
    }

    if (in_len > entry->size) {
        return -4;
    }

    memcpy(entry->data_ptr, in_buf, in_len);

    if (entry->on_write != NULL) {
        entry->on_write();
    }

    return 0;
}
