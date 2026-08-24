/**
 * @file  boot_ftp_trigger.c
 * @author Angel Robotics
 * @brief  XM Boot FTP Trigger — USB CDC-based FTP command handler
 * @version 1.0.0
 * @date  2026-03-24
 * @details
 *   XM-specific boot trigger implementing PhAI V2.2 FTP protocol over USB CDC:
 *   - COBS decode/encode
 *   - CRC16-CCITT validation
 *   - QUERY_INFO response builder (BL ver, App ver, hw_rev, target_module)
 *   - ENTER_BOOTLOADER dispatch (calls AGR_Boot_RequestUpdate from agr_boot_core)
 *
 * @copyright Copyright (c) Angel Robotics Inc.
 */

#include "boot_ftp_trigger.h"
#include "agr_boot_core.h"
#include "agr_boot_types.h"
#include "stm32h7xx_hal.h"
#include <string.h>

/* *---------------------------------------------------------------------
 * PhAI V2.2 FTP PROTOCOL CONSTANTS (self-contained, no BL header dependency)
 *---------------------------------------------------------------------*/

#define APP_FTP_SOF             (0xAAU)
#define APP_FTP_MODULE_CMD      (0xB0U)
#define APP_FTP_MODULE_RESP     (0xB2U)
#define APP_FTP_CMD_QUERY_INFO  (0x01U)
#define APP_FTP_CMD_ENTER_BL    (0x02U)
#define APP_FTP_RESP_ACK        (0x80U)
#define APP_FTP_HEADER_SIZE     (6U)   /* SOF(1)+LEN(1)+SEQ(2)+MODID(1)+STATUS(1) */
#define APP_FTP_CRC16_SIZE      (2U)
#define APP_FTP_MIN_DECODED     (12U)  /* 6B header + 4B payload + 2B CRC16 */

/* *---------------------------------------------------------------------
 * STATIC STATE
 *---------------------------------------------------------------------*/

static Boot_FTP_TxFunc_t s_tx_func = NULL;
static uint16_t s_resp_seq = 0;

/* *---------------------------------------------------------------------
 * PRIVATE: CRC16-CCITT for FTP packet validation
 *
 * Polynomial 0x1021, init 0xFFFF. Must match BL's agr_boot_serial.c.
 *---------------------------------------------------------------------*/

static uint16_t _CRC16(const uint8_t* data, uint32_t len)
{
    uint16_t crc = 0xFFFFU;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8U; j++) {
            if (crc & 0x8000U) {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            } else {
                crc = (uint16_t)(crc << 1);
            }
        }
    }
    return crc;
}

/* *---------------------------------------------------------------------
 * PRIVATE: CRC-32 (IEEE 802.3) for Boot Config validation
 * Must produce same results as BL's agr_boot_crc.c.
 *---------------------------------------------------------------------*/

static const uint32_t s_crc32_table[256] = {
    0x00000000U, 0x77073096U, 0xEE0E612CU, 0x990951BAU,
    0x076DC419U, 0x706AF48FU, 0xE963A535U, 0x9E6495A3U,
    0x0EDB8832U, 0x79DCB8A4U, 0xE0D5E91EU, 0x97D2D988U,
    0x09B64C2BU, 0x7EB17CBDU, 0xE7B82D07U, 0x90BF1D91U,
    0x1DB71064U, 0x6AB020F2U, 0xF3B97148U, 0x84BE41DEU,
    0x1ADAD47DU, 0x6DDDE4EBU, 0xF4D4B551U, 0x83D385C7U,
    0x136C9856U, 0x646BA8C0U, 0xFD62F97AU, 0x8A65C9ECU,
    0x14015C4FU, 0x63066CD9U, 0xFA0F3D63U, 0x8D080DF5U,
    0x3B6E20C8U, 0x4C69105EU, 0xD56041E4U, 0xA2677172U,
    0x3C03E4D1U, 0x4B04D447U, 0xD20D85FDU, 0xA50AB56BU,
    0x35B5A8FAU, 0x42B2986CU, 0xDBBBC9D6U, 0xACBCF940U,
    0x32D86CE3U, 0x45DF5C75U, 0xDCD60DCFU, 0xABD13D59U,
    0x26D930ACU, 0x51DE003AU, 0xC8D75180U, 0xBFD06116U,
    0x21B4F4B5U, 0x56B3C423U, 0xCFBA9599U, 0xB8BDA50FU,
    0x2802B89EU, 0x5F058808U, 0xC60CD9B2U, 0xB10BE924U,
    0x2F6F7C87U, 0x58684C11U, 0xC1611DABU, 0xB6662D3DU,
    0x76DC4190U, 0x01DB7106U, 0x98D220BCU, 0xEFD5102AU,
    0x71B18589U, 0x06B6B51FU, 0x9FBFE4A5U, 0xE8B8D433U,
    0x7807C9A2U, 0x0F00F934U, 0x9609A88EU, 0xE10E9818U,
    0x7F6A0DBBU, 0x086D3D2DU, 0x91646C97U, 0xE6635C01U,
    0x6B6B51F4U, 0x1C6C6162U, 0x856530D8U, 0xF262004EU,
    0x6C0695EDU, 0x1B01A57BU, 0x8208F4C1U, 0xF50FC457U,
    0x65B0D9C6U, 0x12B7E950U, 0x8BBEB8EAU, 0xFCB9887CU,
    0x62DD1DDFU, 0x15DA2D49U, 0x8CD37CF3U, 0xFBD44C65U,
    0x4DB26158U, 0x3AB551CEU, 0xA3BC0074U, 0xD4BB30E2U,
    0x4ADFA541U, 0x3DD895D7U, 0xA4D1C46DU, 0xD3D6F4FBU,
    0x4369E96AU, 0x346ED9FCU, 0xAD678846U, 0xDA60B8D0U,
    0x44042D73U, 0x33031DE5U, 0xAA0A4C5FU, 0xDD0D7CC9U,
    0x5005713CU, 0x270241AAU, 0xBE0B1010U, 0xC90C2086U,
    0x5768B525U, 0x206F85B3U, 0xB966D409U, 0xCE61E49FU,
    0x5EDEF90EU, 0x29D9C998U, 0xB0D09822U, 0xC7D7A8B4U,
    0x59B33D17U, 0x2EB40D81U, 0xB7BD5C3BU, 0xC0BA6CADU,
    0xEDB88320U, 0x9ABFB3B6U, 0x03B6E20CU, 0x74B1D29AU,
    0xEAD54739U, 0x9DD277AFU, 0x04DB2615U, 0x73DC1683U,
    0xE3630B12U, 0x94643B84U, 0x0D6D6A3EU, 0x7A6A5AA8U,
    0xE40ECF0BU, 0x9309FF9DU, 0x0A00AE27U, 0x7D079EB1U,
    0xF00F9344U, 0x8708A3D2U, 0x1E01F268U, 0x6906C2FEU,
    0xF762575DU, 0x806567CBU, 0x196C3671U, 0x6E6B06E7U,
    0xFED41B76U, 0x89D32BE0U, 0x10DA7A5AU, 0x67DD4ACCU,
    0xF9B9DF6FU, 0x8EBEEFF9U, 0x17B7BE43U, 0x60B08ED5U,
    0xD6D6A3E8U, 0xA1D1937EU, 0x38D8C2C4U, 0x4FDFF252U,
    0xD1BB67F1U, 0xA6BC5767U, 0x3FB506DDU, 0x48B2364BU,
    0xD80D2BDAU, 0xAF0A1B4CU, 0x36034AF6U, 0x41047A60U,
    0xDF60EFC3U, 0xA867DF55U, 0x316E8EEFU, 0x4669BE79U,
    0xCB61B38CU, 0xBC66831AU, 0x256FD2A0U, 0x5268E236U,
    0xCC0C7795U, 0xBB0B4703U, 0x220216B9U, 0x5505262FU,
    0xC5BA3BBEU, 0xB2BD0B28U, 0x2BB45A92U, 0x5CB36A04U,
    0xC2D7FFA7U, 0xB5D0CF31U, 0x2CD99E8BU, 0x5BDEAE1DU,
    0x9B64C2B0U, 0xEC63F226U, 0x756AA39CU, 0x026D930AU,
    0x9C0906A9U, 0xEB0E363FU, 0x72076785U, 0x05005713U,
    0x95BF4A82U, 0xE2B87A14U, 0x7BB12BAEU, 0x0CB61B38U,
    0x92D28E9BU, 0xE5D5BE0DU, 0x7CDCEFB7U, 0x0BDBDF21U,
    0x86D3D2D4U, 0xF1D4E242U, 0x68DDB3F8U, 0x1FDA836EU,
    0x81BE16CDU, 0xF6B9265BU, 0x6FB077E1U, 0x18B74777U,
    0x88085AE6U, 0xFF0F6A70U, 0x66063BCAU, 0x11010B5CU,
    0x8F659EFFU, 0xF862AE69U, 0x616BFFD3U, 0x166CCF45U,
    0xA00AE278U, 0xD70DD2EEU, 0x4E048354U, 0x3903B3C2U,
    0xA7672661U, 0xD06016F7U, 0x4969474DU, 0x3E6E77DBU,
    0xAED16A4AU, 0xD9D65ADCU, 0x40DF0B66U, 0x37D83BF0U,
    0xA9BCAE53U, 0xDEBB9EC5U, 0x47B2CF7FU, 0x30B5FFE9U,
    0xBDBDF21CU, 0xCABAC28AU, 0x53B39330U, 0x24B4A3A6U,
    0xBAD03605U, 0xCDD70693U, 0x54DE5729U, 0x23D967BFU,
    0xB3667A2EU, 0xC4614AB8U, 0x5D681B02U, 0x2A6F2B94U,
    0xB40BBE37U, 0xC30C8EA1U, 0x5A05DF1BU, 0x2D02EF8DU,
};

static uint32_t _CRC32(const uint8_t* data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFU;
    for (uint32_t i = 0; i < len; i++) {
        uint32_t idx = (crc ^ (uint32_t)data[i]) & 0xFFU;
        crc = (crc >> 8) ^ s_crc32_table[idx];
    }
    return crc ^ 0xFFFFFFFFU;
}

/* *---------------------------------------------------------------------
 * PRIVATE: Minimal COBS decoder for single-frame input
 *---------------------------------------------------------------------*/

static uint32_t _COBSDecode(const uint8_t* encoded, uint32_t enc_len,
                             uint8_t* decoded, uint32_t dec_max)
{
    uint32_t dec_idx = 0;
    uint32_t enc_idx = 0;

    while (enc_idx < enc_len) {
        uint8_t code = encoded[enc_idx++];
        if (code == 0U) {
            return 0;
        }

        for (uint8_t i = 1; i < code; i++) {
            if (enc_idx >= enc_len || dec_idx >= dec_max) {
                return 0;
            }
            decoded[dec_idx++] = encoded[enc_idx++];
        }

        if (code < 0xFFU && enc_idx < enc_len) {
            if (dec_idx >= dec_max) {
                return 0;
            }
            decoded[dec_idx++] = 0x00U;
        }
    }

    return dec_idx;
}

/* *---------------------------------------------------------------------
 * PRIVATE: COBS encoder for FTP response transmission
 *---------------------------------------------------------------------*/

static uint32_t _COBSEncode(const uint8_t* src, uint32_t src_len,
                             uint8_t* dst, uint32_t dst_max)
{
    uint32_t si = 0;
    uint32_t di = 1;
    uint32_t code_idx = 0;
    uint8_t  code = 1;

    if (dst_max < 2U) { return 0; }

    while (si < src_len) {
        if (src[si] == 0x00U) {
            if (di >= dst_max) { return 0; }
            dst[code_idx] = code;
            code_idx = di++;
            code = 1;
            si++;
        } else {
            if (di >= dst_max) { return 0; }
            dst[di++] = src[si++];
            code++;
            if (code == 0xFFU) {
                dst[code_idx] = code;
                code_idx = di++;
                code = 1;
            }
        }
    }

    if (di >= dst_max) { return 0; }
    dst[code_idx] = code;

    return di;
}

/* *---------------------------------------------------------------------
 * PRIVATE: Boot Config reader for QUERY_INFO response
 *---------------------------------------------------------------------*/

static bool _ReadBootConfig(AGR_BootConfig_t* out)
{
    memcpy(out, (const void*)AGR_BOOT_CONFIG_BASE_ADDR, sizeof(AGR_BootConfig_t));

    if (out->magic != AGR_BOOT_CONFIG_MAGIC) {
        return false;
    }

    uint32_t cfg_data_size = (uint32_t)((uintptr_t)&out->config_crc - (uintptr_t)out);
    uint32_t computed = _CRC32((const uint8_t*)out, cfg_data_size);
    return (computed == out->config_crc);
}

/* *---------------------------------------------------------------------
 * PRIVATE: QUERY_INFO response builder
 *
 * Builds a PhAI V2.2 FTP ACK packet with device/firmware information,
 * COBS-encodes it, and sends via the stored TX callback.
 *
 * Response payload (16 bytes):
 *   [0-3]  bl_ver         = Boot Config bl_ver (packed uint32_t)
 *   [4]    hw_rev         = Boot Config hw_rev
 *   [5]    target_module  = AGR_BOOT_MODULE_XM (0x10)
 *   [6]    ftp_state      = 0xFF (AGR_FTP_STATE_APP_MODE)
 *   [7-10] active_ver     = Current App ver (packed uint32_t)
 *   [11-14] active_size   = Current App binary size
 *   [15]   boot_cfg_status = 0x00(valid,NONE), 0x01(valid,PENDING),
 *                            0x02(invalid), 0xFF(read fail)
 *---------------------------------------------------------------------*/

static void _SendQueryInfoResponse(void)
{
    if (s_tx_func == NULL) { return; }

    uint8_t resp_payload[16];
    memset(resp_payload, 0, sizeof(resp_payload));

    /* Read Boot Config for BL version + hw_rev + status */
    AGR_BootConfig_t boot_cfg;
    bool cfg_valid = _ReadBootConfig(&boot_cfg);

    uint32_t bl_ver = 0;
    uint8_t hw_rev = AGR_BOOT_HWREV_UNKNOWN;
    uint8_t boot_cfg_status;

    if (cfg_valid) {
        bl_ver = AGR_BOOT_PACK_VERSION(boot_cfg.bl_ver_major,
                                        boot_cfg.bl_ver_minor,
                                        boot_cfg.bl_ver_patch, 0);
        hw_rev = boot_cfg.hw_rev;

        if (boot_cfg.update_state == (uint8_t)AGR_BOOT_STATE_PENDING_CONFIRM) {
            boot_cfg_status = 0x01U;
        } else {
            boot_cfg_status = 0x00U;
        }
    } else {
        boot_cfg_status = 0x02U;
    }

    memcpy(&resp_payload[0], &bl_ver, 4);
    resp_payload[4] = hw_rev;
    resp_payload[5] = (uint8_t)AGR_BOOT_MODULE_XM;
    resp_payload[6] = AGR_FTP_STATE_APP_MODE;

    /* Read Active FW info for App version + size */
    const AGR_FwInfo_t* fw_info = (const AGR_FwInfo_t*)AGR_BOOT_ACTIVE_BASE_ADDR;
    uint32_t active_ver = 0;
    uint32_t active_size = 0;

    if (AGR_Boot_IsSignatureValid(fw_info)) {
        active_ver = AGR_BOOT_PACK_VERSION(fw_info->ver_major,
                                            fw_info->ver_minor,
                                            fw_info->ver_patch,
                                            fw_info->ver_debug);
        active_size = fw_info->fw_size;
    }

    memcpy(&resp_payload[7], &active_ver, 4);
    memcpy(&resp_payload[11], &active_size, 4);
    resp_payload[15] = boot_cfg_status;

    /* Build PhAI V2.2 FTP response packet:
     * [SOF][LEN][SEQ_LO][SEQ_HI][MODID][STATUS][RESP_CODE][CMD_ECHO][PAYLOAD...][CRC16] */
    uint32_t total_resp_payload = 2U + sizeof(resp_payload);  /* resp_code + cmd_echo + data */
    uint32_t len_units = (total_resp_payload + 3U) / 4U;

    uint8_t raw_pkt[APP_FTP_HEADER_SIZE + 20 + APP_FTP_CRC16_SIZE];
    uint32_t idx = 0;

    raw_pkt[idx++] = APP_FTP_SOF;
    raw_pkt[idx++] = (uint8_t)len_units;
    raw_pkt[idx++] = (uint8_t)(s_resp_seq & 0xFFU);
    raw_pkt[idx++] = (uint8_t)((s_resp_seq >> 8) & 0xFFU);
    raw_pkt[idx++] = APP_FTP_MODULE_RESP;
    raw_pkt[idx++] = 0x00U;

    raw_pkt[idx++] = APP_FTP_RESP_ACK;
    raw_pkt[idx++] = APP_FTP_CMD_QUERY_INFO;
    memcpy(&raw_pkt[idx], resp_payload, sizeof(resp_payload));
    idx += sizeof(resp_payload);

    /* Pad to 4-byte aligned payload */
    uint32_t padded = len_units * 4U;
    while ((idx - APP_FTP_HEADER_SIZE) < padded) {
        raw_pkt[idx++] = 0x00U;
    }

    /* CRC16 over entire packet (excluding CRC bytes themselves) */
    uint16_t crc = _CRC16(raw_pkt, idx);
    raw_pkt[idx++] = (uint8_t)(crc & 0xFFU);
    raw_pkt[idx++] = (uint8_t)((crc >> 8) & 0xFFU);

    s_resp_seq++;

    /* COBS encode + 0x00 delimiter.
     * COBS overhead = ceil(N/254) + 1. Integer ceiling: (N + 253) / 254.
     * Why +3: +1 for COBS leading code byte, +1 for ceil overhead, +1 for 0x00 delimiter. */
    uint8_t cobs_buf[sizeof(raw_pkt) + ((sizeof(raw_pkt) + 253U) / 254U) + 2U];
    uint32_t cobs_len = _COBSEncode(raw_pkt, idx, cobs_buf, sizeof(cobs_buf) - 1U);
    if (cobs_len == 0U) { return; }

    cobs_buf[cobs_len] = 0x00U;
    cobs_len++;

    (void)s_tx_func(cobs_buf, cobs_len);
}

/* *---------------------------------------------------------------------
 * PUBLIC API
 *---------------------------------------------------------------------*/

void Boot_FTP_Init(Boot_FTP_TxFunc_t tx_func)
{
    s_tx_func = tx_func;
    s_resp_seq = 0;
}

void Boot_FTP_CheckForEnterBL(const uint8_t* data, uint32_t len)
{
    /*
     * Why: Transparent CDC hook — scans every USB Rx for FTP commands.
     *      Handles QUERY_INFO (0x01) and ENTER_BOOTLOADER (0x02).
     * What: Find 0x00 delimiter -> COBS decode -> check PhAI header -> validate CRC16
     *       -> dispatch QUERY_INFO (respond) or ENTER_BOOTLOADER (reset).
     * Impact: Normal PhAI traffic unaffected. QUERY_INFO enables PhAI Studio
     *         device auto-detection. ENTER_BOOTLOADER triggers BL transition.
     */
    if (data == NULL || len < 4U) {
        return;
    }

    /* Find COBS delimiter (0x00) in received buffer */
    uint32_t delim_idx = len;
    for (uint32_t i = 0; i < len; i++) {
        if (data[i] == 0x00U) {
            delim_idx = i;
            break;
        }
    }

    if (delim_idx == 0U || delim_idx >= len) {
        return;
    }

    /* COBS decode the frame (before delimiter) */
    uint8_t decoded[24];
    uint32_t dec_len = _COBSDecode(data, delim_idx, decoded, sizeof(decoded));
    if (dec_len < APP_FTP_MIN_DECODED) {
        return;
    }

    /* Check PhAI V2.2 header: SOF + Module ID */
    if (decoded[0] != APP_FTP_SOF) {
        return;
    }
    if (decoded[4] != APP_FTP_MODULE_CMD) {
        return;
    }

    /* Validate CRC16 (prevents accidental trigger from random data) */
    uint32_t crc_offset = dec_len - 2U;
    uint16_t rx_crc = (uint16_t)decoded[crc_offset]
                    | ((uint16_t)decoded[crc_offset + 1U] << 8);
    uint16_t calc_crc = _CRC16(decoded, crc_offset);
    if (rx_crc != calc_crc) {
        return;
    }

    /* Dispatch by FTP command code (payload byte[0] at decoded[6]) */
    uint8_t cmd = decoded[6];

    if (cmd == APP_FTP_CMD_QUERY_INFO) {
        _SendQueryInfoResponse();
        return;
    }

    if (cmd == APP_FTP_CMD_ENTER_BL) {
        AGR_Boot_RequestUpdate();
        /* Never returns */
    }
}
