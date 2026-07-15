/*
******************************************************************************
**
**  File        : LinkerScript.ld
**
**  Author      : STM32CubeIDE
**
**  Abstract    : Linker script for STM32H7 series
**                2048Kbytes FLASH and 1056Kbytes RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used.
**
**  Target      : STMicroelectronics STM32
**
**  Distribution: The file is distributed as is, without any warranty
**                of any kind.
**
*****************************************************************************
** @attention
**
** Copyright (c) 2023 STMicroelectronics.
** All rights reserved.
**
** This software is licensed under terms that can be found in the LICENSE file
** in the root directory of this software component.
** If no LICENSE file comes with this software, it is provided AS-IS.
**
****************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack — Phase 2: DTCMRAM (Zero-Wait-State) */
_estack = ORIGIN(DTCMRAM) + LENGTH(DTCMRAM);  /* MSP stack in DTCMRAM top */
/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0x2000;      /* required amount of heap  */
_Min_Stack_Size = 0x2000; /* required amount of stack */

/* Specify the memory areas
 *
 * Flash Layout (AGR_BOOT V2):
 *   BL:          0x08000000 - 0x0803FFFF (256KB, Bank1 S0-S1)
 *   FW Header:   0x08040000 - 0x080403FF (1KB, AGR_FwInfo_t)
 *   App (here):  0x08040400 - 0x080FFFFF (767KB, Bank1 S2-S7 remainder)
 *   Backup:      0x08100000 - 0x081BFFFF (768KB, Bank2 S0-S5)
 *   Boot Config: 0x081C0000 - 0x081DFFFF (128KB, Bank2 S6)
 *   Reserved:    0x081E0000 - 0x081FFFFF (128KB, Bank2 S7)
 *
 * Note on memory permissions (GCC 13+ RWX warning fix):
 *   - DTCMRAM/RAM_D1/RAM_D2/RAM_D3/PSRAM: 'x' (execute) removed — data-only regions.
 *     No code is placed or executed from these regions; they hold .data, .bss, heap,
 *     stack, DMA buffers, and NOLOAD variables only.
 *   - ITCMRAM: 'x' retained — .RamFunc and .itcm_text sections contain executable
 *     hot code (ISR/DSP) copied from Flash at startup.
 *   - FLASH: 'x' retained — primary code execution region.
 *   Cortex-M7 MPU does not enforce NX from linker attributes; this is purely a
 *   linker metadata fix to suppress the -Wl,--warn-rwx-segments warning.
 */
MEMORY
{
  FW_HEADER (r)  : ORIGIN = 0x08040000, LENGTH = 1K    /* AGR_FwInfo_t (64B) + padding (960B) */
  FLASH (rx)     : ORIGIN = 0x08040400, LENGTH = 767K
  DTCMRAM (rw)   : ORIGIN = 0x20000000, LENGTH = 128K   /* MSP stack + FreeRTOS heap + .dtcm_data */
  RAM_D1 (rw)    : ORIGIN = 0x24000000, LENGTH = 512K   /* .data, .bss, user heap/stack */
  RAM_D2 (rw)    : ORIGIN = 0x30000000, LENGTH = 288K   /* ETH DMA descriptors, D2 NOLOAD data */
  RAM_D3 (rw)    : ORIGIN = 0x38000000, LENGTH = 64K    /* D3 NOLOAD data */
  ITCMRAM (xrw)  : ORIGIN = 0x00000000, LENGTH = 64K    /* .RamFunc + .itcm_text (executable) */
  PSRAM (rw)     : ORIGIN = 0x90000000, LENGTH = 8M     /* Rev2.0: APS6404L QSPI Memory-Mapped */
}

/* Define output sections */
SECTIONS
{
  /* AGR_BOOT V2: FW Info header at Active Slot base (0x08040000).
   * Placed BEFORE .isr_vector so SWD flash includes it in the same erase/write.
   * BL reads this to validate App before jumping. */
  .fw_header :
  {
    . = ALIGN(4);
    KEEP(*(.fw_header))
    . = ALIGN(4);
  } >FW_HEADER

  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >FLASH

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  /* Constant data goes into FLASH */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH

  .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */
    /* .RamFunc moved to .itcm_text section (Phase 2) */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >RAM_D1 AT> FLASH

  /* Phase 2: DTCMRAM section — FreeRTOS Heap + user zero-wait-state data */
  .dtcm_data (NOLOAD) :
  {
    . = ALIGN(8);
    *(.dtcm_data)      /* __attribute__((section(".dtcm_data"))) */
    *(.dtcm_data*)
    . = ALIGN(4);
  } > DTCMRAM

  /* Phase 2: ITCMRAM code section — ISR/DSP hot code (Flash에서 복사) */
  _sitcm = LOADADDR(.itcm_text);
  .itcm_text :
  {
    . = ALIGN(4);
    _sitcm_data = .;
    *(.RamFunc)        /* .RamFunc sections */
    *(.RamFunc*)       /* .RamFunc* sections */
    *(.itcm_text)      /* __attribute__((section(".itcm_text"))) */
    *(.itcm_text*)
    . = ALIGN(4);
    _eitcm_data = .;
  } > ITCMRAM AT> FLASH

  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss section */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM_D1

  /* HardFault dump persistent area — warm reset (NVIC_SystemReset / lockup) 후에도 보존.
   * NOLOAD + startup 이 _sbss~_ebss 만 0-clear 하므로 touch 되지 않음.
   * 사용: __attribute__((section(".noinit"))) volatile HardFaultDump_t s_fault_dump;
   * 주의: RAM_D1 은 D-Cache cacheable → handler 에서 쓴 뒤
   *       SCB_CleanDCache_by_Addr() 호출 필수 (미호출 시 reset 후 SRAM 엔 미반영). */
  .noinit (NOLOAD) :
  {
    . = ALIGN(8);
    _snoinit = .;
    KEEP(*(.noinit))
    KEEP(*(.noinit*))
    . = ALIGN(8);
    _enoinit = .;
  } >RAM_D1

  /* [???] D2, D3 ???? ???? ??? (???? .bss ????) */
  .RAM_D2_data (NOLOAD) :
  {
    . = ALIGN(4);
    *(.RAM_D2_data)
    *(.RAM_D2_data*)
    . = ALIGN(4);
  } > RAM_D2

  .RAM_D3_data (NOLOAD) :
  {
    . = ALIGN(4);
    *(.RAM_D3_data)
    *(.RAM_D3_data*)
    . = ALIGN(4);
  } > RAM_D3

  /* Rev2.0: ETH DMA descriptors and buffers (MUST be in D2 SRAM, Non-Cacheable)
   * Why: STM32H7 ETH DMA can only access D2 SRAM (0x30000000).
   *       Section names must exactly match ethernetif.c __attribute__((section())) */
  .lwip_sec (NOLOAD) :
  {
    . = ALIGN(4);
    *(.RxDescripSection)  /* ETH RX DMA Descriptors (ethernetif.c) */
    *(.TxDescripSection)  /* ETH TX DMA Descriptors (ethernetif.c) */

    . = ALIGN(4);
    *(.Rx_PoolSection)    /* ETH RX Buffer Pool (ethernetif.c) */

    . = ALIGN(4);
    *(.lwip_sec)
    *(.lwip_sec*)
    . = ALIGN(4);
  } > RAM_D2

  /* Rev2.0: PSRAM sections (NOLOAD - available after QSPI Memory-Mapped init) */
  .PSRAM_data (NOLOAD) :
  {
    . = ALIGN(4);
    *(.PSRAM_data)
    *(.PSRAM_data*)
    . = ALIGN(4);
  } > PSRAM

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM_D1

  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}


