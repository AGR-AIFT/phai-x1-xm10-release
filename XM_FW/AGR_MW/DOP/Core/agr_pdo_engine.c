/**
 ******************************************************************************
 * @file    agr_pdo_engine.c
 * @author  HyundoKim
 * @brief   AGR PDO Engine - Core Implementation (Transport-Agnostic)
 * @version 3.0
 * @date    Feb 25, 2026
 *
 * @details
 * DOP Context에 의존하지 않는 순수 PDO 엔진 구현입니다.
 * Mapping / Encode / Decode / Inhibit 로직만 포함하며,
 * CAN-ID, tx_func 등 transport 코드는 일절 포함하지 않습니다.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#include "agr_pdo_engine.h"
#include <string.h>

/**
 *-----------------------------------------------------------
 * PRIVATE DEFINITIONS
 *-----------------------------------------------------------
 */

/** @brief SDO PDO Mapping Entry 크기 (4B: BitLength + SubIndex + Index LE) */
#define PDO_MAP_SDO_ENTRY_SIZE  4

/*============================================================
 * PDO MAPPING
 *============================================================*/

void AGR_PDO_ClearMap(AGR_PDO_MapTable_t* map)
{
    if (map == NULL) {
        return;
    }
    map->count = 0;
}

int AGR_PDO_AddMap(AGR_PDO_MapTable_t* map,
                   const AGR_OD_Table_t* od,
                   uint16_t index,
                   uint8_t subindex)
{
    if (map == NULL || od == NULL) {
        return -1;
    }

    if (map->count >= AGR_PDO_MAP_MAX_ENTRIES) {
        return -1;
    }

    const AGR_OD_Entry_t* entry = AGR_OD_FindEntryEx(od, index, subindex);
    if (entry == NULL) {
        return -2;
    }

    /* 중복 확인 */
    for (uint8_t i = 0; i < map->count; i++) {
        if (map->items[i].od_index == index &&
            map->items[i].od_subindex == subindex) {
            return 0;
        }
    }

    map->items[map->count].od_index    = index;
    map->items[map->count].od_subindex = subindex;
    map->items[map->count].bit_length  = (uint8_t)(entry->size * 8u);
    map->count++;

    return 0;
}

int AGR_PDO_ApplyMapFromSDO(AGR_PDO_MapTable_t* map,
                            const AGR_OD_Table_t* od,
                            const uint8_t* data,
                            uint8_t data_len)
{
    if (map == NULL || od == NULL || data == NULL) {
        return -1;
    }

    if (data_len < 1) {
        return -2;
    }

    AGR_PDO_ClearMap(map);

    /*
     * [CANopen 표준 PDO Mapping Format - 4B]
     * data[0]: Number of mapped objects (1B)
     * Per entry (4B each):
     *   Byte 0: BitLength  (bits)
     *   Byte 1: SubIndex
     *   Byte 2: Index Low  (Little Endian)
     *   Byte 3: Index High (Little Endian)
     */
    uint8_t num_objects = data[0];
    int added = 0;

    for (uint8_t i = 1;
         (i + (PDO_MAP_SDO_ENTRY_SIZE - 1)) < data_len;
         i += PDO_MAP_SDO_ENTRY_SIZE)
    {
        uint8_t  bit_len  = data[i];
        uint8_t  subindex = data[i + 1];
        uint16_t index    = (uint16_t)data[i + 2] |
                            ((uint16_t)data[i + 3] << 8);

        const AGR_OD_Entry_t* entry = AGR_OD_FindEntryEx(od, index, subindex);
        if (entry == NULL) {
            continue;
        }

        if (map->count >= AGR_PDO_MAP_MAX_ENTRIES) {
            break;
        }

        map->items[map->count].od_index    = index;
        map->items[map->count].od_subindex = subindex;
        map->items[map->count].bit_length  = (bit_len != 0)
                                             ? bit_len
                                             : (uint8_t)(entry->size * 8u);
        map->count++;
        added++;

        if (added >= num_objects) {
            break;
        }
    }

    return added;
}

/*============================================================
 * PDO ENCODE / DECODE
 *============================================================*/

int AGR_PDO_Encode(const AGR_PDO_MapTable_t* map,
                   const AGR_OD_Table_t* od,
                   uint8_t* out_buf,
                   uint8_t buf_size)
{
    if (map == NULL || od == NULL || out_buf == NULL) {
        return -1;
    }

    uint8_t offset = 0;

    for (uint8_t i = 0; i < map->count; i++) {
        const AGR_PDO_MapItem_t* item = &map->items[i];
        const AGR_OD_Entry_t* entry = AGR_OD_FindEntryEx(od,
                                                         item->od_index,
                                                         item->od_subindex);

        if (entry == NULL || entry->data_ptr == NULL) {
            continue;
        }

        if (offset + entry->size > buf_size) {
            break;
        }

        memcpy(&out_buf[offset], entry->data_ptr, entry->size);
        offset += entry->size;
    }

    return offset;
}

int AGR_PDO_Decode(const AGR_PDO_MapTable_t* map,
                   const AGR_OD_Table_t* od,
                   const uint8_t* in_buf,
                   uint8_t in_len)
{
    if (map == NULL || od == NULL || in_buf == NULL) {
        return -1;
    }

    uint8_t offset = 0;

    for (uint8_t i = 0; i < map->count; i++) {
        const AGR_PDO_MapItem_t* item = &map->items[i];
        const AGR_OD_Entry_t* entry = AGR_OD_FindEntryEx(od,
                                                         item->od_index,
                                                         item->od_subindex);

        uint8_t field_size = item->bit_length / 8;

        if (entry == NULL || entry->data_ptr == NULL) {
            /* OD에 해당 항목이 없어도 PDO 프레임 내 데이터는 존재하므로
             * offset을 mapping의 bit_length만큼 전진시켜야 후속 필드 정렬 유지 */
            offset += field_size;
            continue;
        }

        if (offset + entry->size > in_len) {
            break;
        }

        if (entry->access != AGR_ACCESS_RO) {
            memcpy(entry->data_ptr, &in_buf[offset], entry->size);

            if (entry->on_write != NULL) {
                entry->on_write();
            }
        }

        offset += entry->size;
    }

    return offset;
}

/*============================================================
 * PDO INHIBIT TIME
 *============================================================*/

void AGR_PDO_SetInhibitTime(AGR_PDO_Inhibit_t* inhibit,
                            uint32_t inhibit_time_us)
{
    if (inhibit == NULL) {
        return;
    }

    inhibit->inhibit_enabled  = true;
    inhibit->inhibit_time_us  = inhibit_time_us;
    inhibit->last_tx_tick_us  = 0;
}

void AGR_PDO_DisableInhibit(AGR_PDO_Inhibit_t* inhibit)
{
    if (inhibit == NULL) {
        return;
    }

    inhibit->inhibit_enabled = false;
}

bool AGR_PDO_CanSend(const AGR_PDO_Inhibit_t* inhibit,
                     uint32_t current_time_us)
{
    if (inhibit == NULL) {
        return false;
    }

    if (!inhibit->inhibit_enabled) {
        return true;
    }

    if (inhibit->last_tx_tick_us == 0) {
        return true;
    }

    uint32_t elapsed_us = current_time_us - inhibit->last_tx_tick_us;
    return (elapsed_us >= inhibit->inhibit_time_us);
}

void AGR_PDO_MarkSent(AGR_PDO_Inhibit_t* inhibit,
                      uint32_t current_time_us)
{
    if (inhibit == NULL) {
        return;
    }

    inhibit->last_tx_tick_us = current_time_us;
}
