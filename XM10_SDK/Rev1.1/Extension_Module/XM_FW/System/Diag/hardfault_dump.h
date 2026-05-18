/**
 ******************************************************************************
 * @file    hardfault_dump.h
 * @author  HyundoKim
 * @brief   HardFault/MemManage/BusFault/UsageFault 덤프 — warm-reset 생존
 *
 * @details
 * Step B 재설계 체크리스트 (project_step_b_failed_attempts.md) 중 #1.
 *
 * 목적: 크래시 시 ARM Cortex-M7 fault context 를 SRAM `.noinit` 영역에
 *   캡처하고 `NVIC_SystemReset()` 으로 재부팅 → 부팅 직후 구조체를 읽어
 *   USB 마운트 뒤 파일로 영속화. 다음 크래시의 원인을 추측 없이 분석.
 *
 * 생존 경로: NVIC_SystemReset / Lockup 은 SRAM 유지. POR/BOR/NRST 는 소실.
 *   → 파워 사이클 로 날아가는 데이터는 부팅 직후 파일로 덤프해서 보관.
 *
 * D-Cache: RAM_D1 은 cacheable 이므로 capture 후
 *   `SCB_CleanDCache_by_Addr()` + `__DSB()` 반드시 호출 (미호출 시
 *   reset 후 SRAM 에 실제 반영 안 됨 → dump 가 0 으로 보임).
 *
 * 사용법:
 *   1. main() 페리페럴 초기화 직전 → `HardFault_CheckPending(&out)` 호출
 *      → pending 이면 RAM 에 복사만 해두고 magic clear
 *   2. USB 마운트 완료 후 pending 사본을 `/hardfault_<ts>.txt` 로 기록
 *
 * NOTE: 이 모듈은 fault handler 컨텍스트에서만 호출되는 함수를 포함.
 *   handler 내에서 printf / FatFs / HAL delay 등 **절대 금지** — 재귀 fault
 *   또는 lockup 위험.
 ******************************************************************************
 */

#ifndef HARDFAULT_DUMP_H_
#define HARDFAULT_DUMP_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 덤프 유효성 magic — 64-bit 이중 검증 (POR 후 random 값 false positive 회피) */
#define HF_DUMP_MAGIC_A          0xDEADFA17U
#define HF_DUMP_MAGIC_B          0x5A5AC0DEU

/** Exception type 식별 */
typedef enum {
    HF_EXC_NONE        = 0,
    HF_EXC_HARDFAULT   = 3,
    HF_EXC_MEMMANAGE   = 4,
    HF_EXC_BUSFAULT    = 5,
    HF_EXC_USAGEFAULT  = 6,
    HF_EXC_NMI         = 2,
} HF_ExcType_t;

/** 덤프 구조체 — `.noinit` 영역에 배치, sizeof 반드시 8-byte align 유지 */
typedef struct {
    uint32_t     magic_a;        /* HF_DUMP_MAGIC_A */
    uint32_t     magic_b;        /* HF_DUMP_MAGIC_B */
    uint32_t     checksum;       /* magic~task_name 전 필드 XOR (validity 추가 검증) */
    uint32_t     exc_type;       /* HF_ExcType_t */

    /* 스택에서 읽은 ABI frame (8 words) */
    uint32_t     r0;
    uint32_t     r1;
    uint32_t     r2;
    uint32_t     r3;
    uint32_t     r12;
    uint32_t     lr;             /* stacked LR (return address before exception) */
    uint32_t     pc;             /* stacked PC (fault 발생 명령어 주소) */
    uint32_t     xpsr;

    /* ARM Cortex-M fault status */
    uint32_t     cfsr;           /* SCB->CFSR (MMSR | BFSR | UFSR) */
    uint32_t     hfsr;           /* SCB->HFSR */
    uint32_t     mmfar;          /* SCB->MMFAR (MMARVALID 일 때만 유효) */
    uint32_t     bfar;           /* SCB->BFAR  (BFARVALID 일 때만 유효) */

    /* 컨텍스트 정보 */
    uint32_t     exc_return;     /* EXC_RETURN (LR at entry: MSP/PSP, FP 여부) */
    uint32_t     msp;            /* Main Stack Pointer at entry */
    uint32_t     psp;            /* Process Stack Pointer at entry */
    uint32_t     ipsr;           /* Interrupt Program Status (활성 exception #) */

    /* FreeRTOS task name (thread mode 일 때만) — 15 char + NUL */
    char         task_name[16];

    uint32_t     uptime_ms;      /* uwTick snapshot at capture (참고용) */
    uint32_t     reserved[7];    /* 128B cache-line align 보정 및 향후 확장 */
} HardFaultDump_t;

/* 구조체 크기 고정 (sizeof 변하면 컴파일 에러 → layout shift 감지) */
_Static_assert(sizeof(HardFaultDump_t) == 128, "HardFaultDump_t layout drift");


/* ─────────────────────────────────────────────────────────────
 * Public API
 * ───────────────────────────────────────────────────────────── */

/**
 * @brief  부팅 직후 pending fault dump 가 있는지 확인하고 사본 반환.
 * @param  out [out] pending 덤프 사본 (magic 일치 시에만 유효)
 * @retval true  pending 존재 → `.noinit` magic clear 완료, caller 가 파일 기록 책임
 * @retval false pending 없음 (정상 boot / POR 직후 / 이미 소비됨)
 * @note   main() 초기, 페리페럴 초기화 이전에 호출 권장.
 */
bool HardFault_CheckPending(HardFaultDump_t *out);

/**
 * @brief  부팅 시 `.noinit` 덤프를 읽어 모듈 내부 static buffer 로 이관.
 *         `.noinit` 의 magic 은 clear → 다음 reset loop 에서 false positive 방지.
 *         이후 USB mount 완료 시 `HardFault_TakeBootDump()` 로 소비.
 * @retval true  pending 덤프가 있어서 보관 완료
 * @retval false pending 없음 (정상 부팅)
 * @note   main() 의 SysInit 단계, peripheral init 전에 1회만 호출.
 */
bool HardFault_LoadBootDump(void);

/**
 * @brief  `HardFault_LoadBootDump()` 로 보관된 덤프가 있는지 확인.
 * @retval true 보관된 덤프 존재 (아직 소비 안 됨)
 */
bool HardFault_HasBootDump(void);

/**
 * @brief  보관된 덤프를 호출자에게 이관 + 내부 slot 은 clear.
 *         USB mount 완료 후 파일 기록 직전에 호출.
 * @param  out [out] 덤프 사본
 * @retval true 성공 (*out 에 유효 덤프 복사됨)
 * @retval false 보관된 덤프 없음
 */
bool HardFault_TakeBootDump(HardFaultDump_t *out);

/**
 * @brief  덤프 구조체를 사람이 읽는 텍스트로 포맷.
 * @param  dump [in]  유효한 덤프 구조체
 * @param  buf  [out] 출력 버퍼 (권장 ≥ 1024B)
 * @param  bufsize 버퍼 크기
 * @retval 실제 기록된 바이트 수 (NUL 제외), buf 부족 시 truncate
 */
uint32_t HardFault_FormatReport(const HardFaultDump_t *dump,
                                char *buf, uint32_t bufsize);

/**
 * @brief  Fault handler 내부 사용 — naked dispatcher 가 호출.
 *         유저 코드가 직접 부르면 안 됨.
 * @param  stacked_sp [in] 스택 포인터 (r0 인자로 넘어온 MSP/PSP)
 * @param  exc_return_val [in] EXC_RETURN (LR at entry)
 * @param  exc_type [in] HF_ExcType_t
 */
void HardFault_CaptureAndReset(uint32_t *stacked_sp,
                               uint32_t exc_return_val,
                               uint32_t exc_type);

#ifdef __cplusplus
}
#endif

#endif /* HARDFAULT_DUMP_H_ */
