/**
 ******************************************************************************
 * @file    xm_api_logging.h
 * @author  HyundoKim
 * @brief   XM10 USB-CDC 구조체 로깅 API
 * @details
 * 직접 정의한 구조체를 등록해 두고 보내면, PC 가 각 필드의 이름 · 단위 · 타입을 함께 받아 데이터를 풉니다.
 * 구조체마다 '토큰'(짧은 이름) 하나를 정하고, 아래 네 매크로를 그 토큰으로 씁니다(전체 예제는 예제 44 참고).
 *
 *   #include "xm_api.h"
 *   #include "xm_api_logging_raw.h"   // 매크로가 부르는 함수 원형 — 매크로를 쓰는 .c 파일은 꼭 include 하세요
 *
 *   typedef struct {
 *       float    hip_angle;
 *       int16_t  emg[8];
 *       uint32_t mode;
 *   } MyLog_t;                                          // 24 B — 크기는 4의 배수
 *
 *   XM_LOG_DECLARE(MyLog, MyLog_t);                     // 토큰 MyLog ↔ 구조체 MyLog_t
 *
 *   XM_LOG_FIELDS(MyLog) = {
 *       XM_FIELD(MyLog, hip_angle, "hip_angle", "deg", 1.0f),
 *       XM_FIELD_ARRAY(MyLog, emg, "emg", "mV", 1.0f),
 *       XM_FIELD(MyLog, mode, "mode", "", 1.0f),
 *   };
 *
 *   void Control_Setup(void)
 *   {
 *       if (!XM_LOG_REGISTER(MyLog, 0xF0, 1000)) {
 *           // 등록 실패는 LED 로 알립니다 — Control_Setup() 에서는 USB 가 아직 준비되지 않아 메시지가 보통 나가지 않습니다
 *           XM_SetLedEffect(XM_LED_1, XM_LED_BLINK, 200);
 *       }
 *   }
 *
 *   void Control_Loop(void)
 *   {
 *       MyLog_t s = { 0 };
 *       (void)XM_LOG_SEND(MyLog, &s);                   // 등록한 주기보다 일찍 부르면 보내지 않고 false
 *   }
 *
 * 네 매크로(DECLARE · FIELDS · REGISTER · SEND)는 같은 .c 파일 안에서 씁니다. 매크로는 C 파일 전용입니다.
 * 틀린 사용(지원하지 않는 멤버 타입, 4의 배수가 아닌 크기, 다른 구조체 포인터 등)은 컴파일러가 그 줄을 가리킵니다.
 *
 * 구조체를 하나라도 등록하면 등록한 Module ID 는 이 구조체 전용이 되고(XM_SendUsbDataWithId() 로 보내면 false),
 * XM_SendUsbDataWithId() · XM_SendUsbDebugMessage() 로 보내는 양에 한도가 생깁니다 — 짧은 시간에 너무 많이 보내거나,
 * 한 번에 보내기에 너무 길거나 크면 false 입니다(너무 긴 · 큰 것은 기다려도 나가지 않습니다 — 줄이거나 시간 간격을 두고 나눠 보내세요).
 *
 * 호스트 프로파일(XM_USB_SetHostProfile) · 채널 메타(XM_SetUsbCustomMeta) · XM_Log_SetMinStallMs 는 Control_Setup() 에서
 * 첫 XM_LOG_REGISTER 보다 먼저 부르세요. 구조체를 하나라도 등록하면 프로파일은 더 바꿀 수 없습니다(호출이 무시됩니다).
 *
 * 시리얼 터미널용 프로파일(XM_USB_HOST_TERMINAL)이어도 등록한 구조체의 데이터와 필드 정보(XM_Log_EnableLinkHealth() 를 켰다면 링크 상태도)는
 * 그대로 나가 터미널 화면에 깨진 글자로 섞여 보일 수 있습니다. 텍스트만 볼 때는 구조체를 등록하지 마세요.
 *
 * @copyright Copyright (c) 2026 Angel Robotics Co., Ltd. All rights reserved.
 ******************************************************************************
 */

#pragma once

#ifndef XM_API_XM_API_LOGGING_H_
#define XM_API_XM_API_LOGGING_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 *-----------------------------------------------------------
 * PUBLIC DEFINITIONS AND MACROS
 *-----------------------------------------------------------
 */

#define XM_LOG_MAX_STRUCT_SIZE   (1008U)  /**< 등록할 수 있는 구조체 크기 상한(바이트) */
#define XM_LOG_MAX_FIELDS        (90U)    /**< 구조체 하나에 적을 수 있는 필드 수 상한 */
#define XM_LOG_MAX_ARRAY_LEN     (255U)   /**< 배열 필드 하나의 원소 수 상한 */
#define XM_LOG_MAX_TOKEN_LEN     (11U)    /**< 토큰 길이 상한(글자) — 토큰이 PC 에 보이는 구조체 이름이 됩니다 */
#define XM_LOG_SLOT_NONE         (0xFFU)  /**< 등록되지 않은 토큰의 슬롯 값 */

/**
 *-----------------------------------------------------------
 * PUBLIC ENUMERATIONS AND TYPES
 *-----------------------------------------------------------
 */

/** @brief 필드 타입 — XM_FIELD · XM_FIELD_ARRAY 가 멤버의 C 타입에서 정합니다(직접 적지 않습니다) */
typedef enum {
    XM_FIELD_F32 = 0, XM_FIELD_F64, XM_FIELD_I8,  XM_FIELD_U8,
    XM_FIELD_I16,     XM_FIELD_U16, XM_FIELD_I32, XM_FIELD_U32,
    XM_FIELD_I64,     XM_FIELD_U64, XM_FIELD_BOOL
} XM_FieldType_e;

/** @brief 필드 설명 한 줄 — XM_FIELD · XM_FIELD_ARRAY 로 만듭니다 */
typedef struct {
    const char*    name;       /**< 필드 이름 (ASCII 영문 · 숫자 · 기호 15자까지) */
    const char*    unit;       /**< 단위 (ASCII 영문 · 숫자 · 기호 7자까지, 없으면 "". °C · µV 같은 글자는 쓸 수 없습니다) */
    uint8_t        type;       /**< 원소 타입 — XM_FieldType_e 값. 컴파일 옵션과 상관없이 1 바이트입니다 */
    uint16_t       offset;     /**< 구조체 안 위치(바이트) */
    uint8_t        array_len;  /**< 원소 개수 (스칼라 = 1) */
    float          scale;      /**< 물리값 = 저장값 × scale */
} XM_FieldDesc_t;

/** @brief 로깅 관측 카운터 — XM_Log_GetStats() 로 받습니다. 모두 부팅 뒤 누적값이고, 따로 적지 않은 칸은 구조체를 등록하지 않아도 셉니다 */
typedef struct {
    uint32_t rate_gate_suppressed_calls;      /**< XM_LOG_SEND 를 등록한 주기보다 자주 불러 보내지 않은 횟수 (등록한 구조체가 있을 때만 생깁니다) */
    uint32_t owner_violation_count;           /**< XM_LOG_REGISTER · XM_LOG_SEND · 이 헤더의 설정 함수를 Control_Setup() · Control_Loop() 밖(다른 태스크 · 인터럽트)에서
                                                   부른 횟수 (구조체를 등록한 뒤라면 XM_SetUsbCustomMeta() 도 셉니다) */
    uint32_t profile_change_blocked_count;    /**< 등록 뒤 XM_USB_SetHostProfile() 를 불러 무시된 횟수 */
    uint32_t admission_reject_count;          /**< XM_SendUsbDataWithId() · XM_SendUsbDebugMessage() 등이 송신량 한도(한 번에 보내기에 너무 큰 것 포함)로 거절된 횟수 (구조체를 등록했을 때만 셉니다. 인자가 함수의 최대 길이를 넘어 거절된 호출은 세지 않습니다) */
    uint32_t reserved_id_reject_count;        /**< 시스템이 쓰는 모듈 ID 나 등록한 구조체의 모듈 ID 로 보내려다 거절된 횟수 + XM_SetUsbStreamModuleId() 에 시스템이 쓰는 ID 를 주어 무시된 횟수
                                                   (등록한 구조체의 ID 는 정할 때가 아니라 그 ID 로 보낼 때마다 셉니다) */
    uint32_t seal_reject_count;               /**< Control_Setup() 이 끝난 뒤 XM_LOG_REGISTER 를 부른 횟수 (구조체를 등록한 뒤라면 XM_SetUsbCustomMeta() 도 셉니다) */
    uint32_t admission_attempt_calls;         /**< 위 송신이 송신량 한도 검사를 받은 횟수(허용 + 거절, 구조체를 등록했을 때만 셉니다) */
    uint32_t x20_same_tick_suppressed_calls;  /**< 제어 루프가 같은 1 ms 안에 다시 돌아 Total Data 송신을 건너뛴 횟수 (구조체를 등록했을 때만 셉니다) */
} XM_LogStats_t;

/* ---------------------------------------------------------------------------
 * 아래 이름 끝이 '_' 인 매크로는 다른 매크로가 쓰는 보조 정의입니다 — 직접 쓰지 마세요.
 * --------------------------------------------------------------------------- */

/* 컴파일 시간 검사 — 조건이 거짓이면 msg 를 내고 컴파일을 멈춘다. 식 안에서 쓸 수 있고 값은 0 이다 */
#define XM_LOG_REQUIRE_(cond, msg) \
    (sizeof(struct { _Static_assert((cond), msg); int xm_log_ok_; }) * 0U)

#define XM_FIELD_TYPE_NONE_  (0xFFU)

/* 멤버의 C 타입 → 필드 타입. int32_t 가 long 인 컴파일러(ARM)에서도 int · unsigned 를 받도록 안쪽에서 한 번 더 고른다 */
#define XM_TYPE_TAG_(x) _Generic((x),                                              \
    float:   XM_FIELD_F32,  double:   XM_FIELD_F64,                                \
    int8_t:  XM_FIELD_I8,   uint8_t:  XM_FIELD_U8,                                 \
    int16_t: XM_FIELD_I16,  uint16_t: XM_FIELD_U16,                                \
    int32_t: XM_FIELD_I32,  uint32_t: XM_FIELD_U32,                                \
    int64_t: XM_FIELD_I64,  uint64_t: XM_FIELD_U64,                                \
    _Bool:   XM_FIELD_BOOL,                                                        \
    default: _Generic((x), int: XM_FIELD_I32, unsigned int: XM_FIELD_U32,          \
                           default: XM_FIELD_TYPE_NONE_))

/**
 * @brief 멤버 식의 필드 타입. 지원: float · double · bool · int · unsigned · int8_t ~ uint64_t.
 *        열거형 멤버는 크기가 같은 정수 타입으로 받습니다.
 *        그 밖의 타입(char · 포인터 · 구조체 · 배열 등)은 컴파일 오류입니다.
 */
#define XM_TYPE_TAG(x)                                                             \
    ((uint8_t)(XM_TYPE_TAG_(x) +                                                   \
        XM_LOG_REQUIRE_(XM_TYPE_TAG_(x) != XM_FIELD_TYPE_NONE_,                    \
            "XM_LOG: unsupported member type - use float, double, bool, int, unsigned or int8_t..uint64_t (arrays: XM_FIELD_ARRAY)")))

#ifndef __cplusplus
_Static_assert(sizeof(int) == 4U, "XM_LOG: int must be 32-bit");
#endif

#define XM_LOG_MEMBER_(tok, member)   (((XM_LogType_##tok*)0)->member)
#define XM_LOG_ARRLEN_(tok, member)   (sizeof(XM_LOG_MEMBER_(tok, member)) / sizeof(XM_LOG_MEMBER_(tok, member)[0]))
#define XM_LOG_IS_ARRAY_(x)           (!__builtin_types_compatible_p(__typeof__(x), __typeof__(&(x)[0])))
#define XM_LOG_FIELD_COUNT_(tok)      (sizeof(xm_log_fields_##tok) / sizeof(xm_log_fields_##tok[0]))

/* ---------------------------------------------------------------------------
 * 필드 설명 — XM_LOG_FIELDS(tok) = { ... }; 의 중괄호 안에 씁니다.
 * 중괄호 안에는 같은 토큰의 XM_FIELD · XM_FIELD_ARRAY 만 쓰세요(컴파일러가 이 규칙은 검사하지 못합니다).
 * --------------------------------------------------------------------------- */

/**
 * @brief 스칼라 멤버 한 줄. 타입과 위치는 멤버에서 정해집니다.
 * @param tok    XM_LOG_DECLARE 의 토큰
 * @param member 구조체 멤버 이름
 * @param nm     PC 에 보일 필드 이름 (ASCII 영문 · 숫자 · 기호 15자까지 — 한글 · ° · µ 같은 글자가 있으면 등록이 false)
 * @param un     단위 (ASCII 영문 · 숫자 · 기호 7자까지, 없으면 "". 예: "deg" · "degC" · "uV")
 * @param sc     물리값 = 저장값 × sc (보통 1.0f)
 */
#define XM_FIELD(tok, member, nm, un, sc)                                          \
    { .name = (nm), .unit = (un),                                                  \
      .type = XM_TYPE_TAG(XM_LOG_MEMBER_(tok, member)),                            \
      .offset = (uint16_t)offsetof(XM_LogType_##tok, member),                      \
      .array_len = 1U, .scale = (sc) }

/**
 * @brief 배열 멤버 한 줄. 원소 타입과 개수(1 ~ 255)는 배열에서 정해집니다.
 *        포인터 멤버나 256 개 이상인 배열은 컴파일 오류입니다. 인자는 XM_FIELD 와 같습니다.
 */
#define XM_FIELD_ARRAY(tok, member, nm, un, sc)                                    \
    { .name = (nm), .unit = (un),                                                  \
      .type = XM_TYPE_TAG(XM_LOG_MEMBER_(tok, member)[0]),                         \
      .offset = (uint16_t)(offsetof(XM_LogType_##tok, member) +                    \
          XM_LOG_REQUIRE_(XM_LOG_IS_ARRAY_(XM_LOG_MEMBER_(tok, member)),           \
              "XM_LOG: XM_FIELD_ARRAY needs an array member, not a pointer")),     \
      .array_len = (uint8_t)(XM_LOG_ARRLEN_(tok, member) +                         \
          XM_LOG_REQUIRE_(XM_LOG_ARRLEN_(tok, member) >= 1U &&                     \
                          XM_LOG_ARRLEN_(tok, member) <= XM_LOG_MAX_ARRAY_LEN,     \
              "XM_LOG: XM_FIELD_ARRAY length must be 1..255")),                    \
      .scale = (sc) }

/* ---------------------------------------------------------------------------
 * 구조체 로깅 매크로 4개
 * --------------------------------------------------------------------------- */

/**
 * @brief 토큰과 구조체 타입을 묶습니다. .c 파일 위쪽에 한 번 씁니다.
 * @details 토큰은 영문 · 숫자 · _ 로 11자까지 씁니다. 한글 같은 글자가 있으면 컴파일은 되지만 XM_LOG_REGISTER 가 false 입니다.
 *          구조체 크기가 4의 배수가 아니거나 1,008 B 를 넘거나, 토큰이 11자를 넘으면 컴파일 오류입니다.
 *          크기를 맞출 때는 끝에 uint8_t 배열을 더하세요.
 */
#define XM_LOG_DECLARE(tok, Type)                                                  \
    typedef Type XM_LogType_##tok;                                                 \
    static uint8_t xm_log_slot_id_##tok __attribute__((unused)) = XM_LOG_SLOT_NONE; \
    _Static_assert(sizeof(Type) % 4U == 0U,                                        \
        "XM_LOG: sizeof(" #Type ") must be a multiple of 4 - add padding");        \
    _Static_assert(sizeof(Type) <= XM_LOG_MAX_STRUCT_SIZE,                         \
        "XM_LOG: " #Type " exceeds XM_LOG_MAX_STRUCT_SIZE");                       \
    _Static_assert(sizeof(#tok) <= XM_LOG_MAX_TOKEN_LEN + 1U,                      \
        "XM_LOG: token " #tok " longer than 11 chars")

/** @brief 필드 설명 배열을 정의합니다. 뒤에 = { XM_FIELD(...), ... }; 를 씁니다 */
#define XM_LOG_FIELDS(tok)                                                         \
    static const XM_FieldDesc_t xm_log_fields_##tok[] __attribute__((unused))

/**
 * @brief 구조체를 등록합니다. Control_Setup() 안에서 부릅니다.
 * @param tok             XM_LOG_DECLARE 의 토큰 (XM_LOG_FIELDS 가 앞에 있어야 합니다)
 * @param module_id       이 구조체를 보낼 모듈 ID — 0xF0 ~ 0xFE
 * @param nominal_rate_hz 보낼 주기(Hz) — XM_LOG_SEND 를 더 자주 불러도 이보다 자주 나가지 않습니다.
 *                        간격을 1 ms 단위로 올려 잡아서 1,000 의 약수가 아니면 덜 자주 나갑니다(예: 300 Hz → 250 Hz,
 *                        600 Hz → 500 Hz). 1,000 Hz 보다 자주는 나가지 않습니다
 * @return true: 등록됨. false: 이 토큰을 이미 등록함, 모듈 ID 가 범위 밖이거나 이미 쓰임, 등록이 4개를 넘음,
 *         nominal_rate_hz 가 0, 필드 이름이 겹치거나 두 필드가 구조체의 같은 바이트를 가리킴,
 *         필드 이름 · 단위가 너무 길거나, 토큰 · 필드 이름 · 단위에 ASCII 가 아닌 글자(한글 등)가 있음,
 *         scale 이 0 이거나 숫자가 아님(무한대 · NaN),
 *         보낼 양이 USB 로 보낼 수 있는 양을 넘음(먼저 등록한 구조체와 합쳐서 셉니다),
 *         먼저 등록한 구조체와 합친 양이 PC 프로그램이 잠시 멈춰도 담아 둘 수 있는 양을 넘음
 *         (XM_Log_SetMinStallMs() 참고 — 그 목표 시간은 첫 XM_LOG_REGISTER 보다 먼저만 바꿀 수 있습니다),
 *         Control_Setup() 밖(Control_Setup() 이 끝난 뒤 · 다른 태스크 · 인터럽트)에서 부름.
 *         false 면 등록 상태는 바뀌지 않으니 Control_Setup() 안에서 고쳐 다시 부를 수 있습니다.
 *         false 여도 이미 등록된 같은 토큰은 그대로 보낼 수 있습니다.
 * @note 필드는 1 ~ 90 개입니다. 0 개이거나 90 개를 넘으면 컴파일 오류입니다.
 * @note 등록한 모듈 ID 는 XM_LOG_SEND 전용입니다 — 같은 ID 로 XM_SendUsbDataWithId() 를 부르면 false 입니다.
 */
#define XM_LOG_REGISTER(tok, module_id, nominal_rate_hz)                           \
    XM_Log_RegisterSchemaRaw(&xm_log_slot_id_##tok, (module_id), #tok,             \
        (uint16_t)sizeof(XM_LogType_##tok), (nominal_rate_hz), xm_log_fields_##tok, \
        (uint8_t)(XM_LOG_FIELD_COUNT_(tok) +                                       \
            XM_LOG_REQUIRE_(XM_LOG_FIELD_COUNT_(tok) >= 1U &&                      \
                            XM_LOG_FIELD_COUNT_(tok) <= XM_LOG_MAX_FIELDS,         \
                "XM_LOG: field count must be 1..90")))

/**
 * @brief 등록한 구조체 하나를 보냅니다. Control_Loop() 안에서 부릅니다.
 * @param tok XM_LOG_DECLARE 의 토큰
 * @param ptr 그 구조체의 포인터 — 다른 타입의 포인터는 컴파일 오류입니다
 * @return true: 보냄. false: 등록 안 됨(XM_LOG_REGISTER 가 false 였던 토큰 포함), ptr 가 NULL, 등록 주기보다 일찍 부름,
 *         USB 장치가 준비되지 않음, 송신 버퍼가 가득 참, Control_Setup() · Control_Loop() 밖(다른 태스크 · 인터럽트)에서 부름
 * @note 송신 버퍼가 가득 차 false 가 나와도 다음 송신은 등록한 주기가 지난 뒤에 나갑니다(바로 다시 불러도 보내지 않습니다).
 */
#define XM_LOG_SEND(tok, ptr)                                                      \
    XM_Log_SendRaw(xm_log_slot_id_##tok,                                           \
        _Generic((ptr), XM_LogType_##tok*: (const void*)(ptr),                     \
                        const XM_LogType_##tok*: (const void*)(ptr)),              \
        (uint16_t)sizeof(XM_LogType_##tok))

/**
 *------------------------------------------------------------
 * PUBLIC FUNCTION PROTOTYPES
 *------------------------------------------------------------
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 링크 상태 정보(송신 실패 수 · 버퍼 사용률 등)를 1초마다 보낼지 정합니다. 기본은 보내지 않습니다.
 *        구조체를 하나 이상 등록했을 때만 보냅니다 — 등록하지 않으면 켜도 보내지 않습니다.
 * @param enable true: 보냄, false: 보내지 않음
 * @return true: 설정됨. false: Control_Setup() 밖에서 부름
 */
bool XM_Log_EnableLinkHealth(bool enable);

/**
 * @brief USB 송신 예산 가운데 이미 정해진 몫(바이트/초) — 등록한 구조체들의 몫과 보드가 미리 잡아 둔 몫의 합입니다.
 *        미리 잡아 둔 몫: 등록하지 않은 데이터 · 디버그 텍스트, 구조체 정보 · 링크 상태 전송, 그리고 PhAI Studio 프로파일(기본)이면
 *        Total Data. 구조체를 하나도 등록하지 않았으면 미리 잡아 둔 몫만 나옵니다. 진단용입니다
 */
uint32_t XM_Log_GetAdmittedWireBps(void);

/**
 * @brief 관측 카운터를 복사합니다. Control_Loop() 에서 부릅니다.
 *        칸마다 따로 읽으므로 여러 칸이 정확히 같은 순간의 값은 아닐 수 있습니다.
 * @param out 받을 곳 — NULL 이면 아무것도 하지 않습니다
 */
void XM_Log_GetStats(XM_LogStats_t* out);

/**
 * @brief PC 프로그램이 잠시 멈춰도 보드가 데이터를 빠뜨리지 않고 담아 두는 목표 시간(ms)을 정합니다. 기본 50 ms.
 * @details 이 목표를 지킬 수 없을 만큼 많이 보내는 구조체의 등록은 false 가 됩니다.
 *          50 ms 는 아직 보드에서 재 보지 않은 초기값이라 보장이 아니라 목표입니다.
 * @param ms 목표 시간(ms)
 * @return true: 바뀜. false: Control_Setup() 밖에서 부름, 또는 등록에 성공한 구조체가 이미 있음
 */
bool XM_Log_SetMinStallMs(uint16_t ms);

/** @brief 지금 쓰는 목표 시간(ms). 진단용입니다 */
uint16_t XM_Log_GetMinStallMs(void);

#ifdef __cplusplus
}
#endif

#endif /* XM_API_XM_API_LOGGING_H_ */
