#!/usr/bin/env python3
"""
patch_cubemx_overrides.py — CubeMX Code Generation 후 덮어쓰인 파일 자동 패치

[배경]
CubeMX는 Middlewares/ 폴더 내 파일을 Code Generation 시 통째로 덮어쓴다.
lwipopts.h(USER CODE 보호)에서 #undef로 override하지만,
cc.h의 #ifndef guard가 없으면 include 순서에 따라 rand() → newlib assert → abort 발생.

[사용법]
STM32CubeIDE: Project → Properties → C/C++ Build → Settings → Build Steps
  → Pre-build steps: python ${workspace_loc:/${ProjName}}/../tools/patch_cubemx_overrides.py

또는 CLI:
  python tools/patch_cubemx_overrides.py
"""

import io
import re
import sys
from pathlib import Path

# 빌드 콘솔 인코딩(cp949 등)이 유니코드 출력을 못 받아 크래시→빌드차단 되는 것 방지
# (size_report.py / regen_guard.py 와 동일 관례).
if sys.stdout.encoding != "utf-8":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

# 이 스크립트는 Extension_Module/tools/build/ 에 위치
# .parent(build) → .parent(tools) → .parent(Extension_Module) = 모듈 루트
MODULE_ROOT = Path(__file__).resolve().parent.parent.parent

# ============================================================
# 패치 목록: (대상 파일, 패치 함수)
# CubeMX가 덮어쓰는 파일에 대한 패치를 여기에 추가
# ============================================================

def patch_cc_h_lwip_rand(file_path: Path) -> bool:
    """cc.h: LWIP_RAND 정의에 #ifndef guard 추가

    CubeMX가 guard 없이 생성:
        #define LWIP_RAND() ((u32_t)rand())

    패치 후:
        #ifndef LWIP_RAND
        #define LWIP_RAND() ((u32_t)rand())
        #endif

    이 guard가 있어야 lwipopts.h의 #undef + lwip_port_rand() override가 유효함.
    """
    if not file_path.exists():
        return False

    content = file_path.read_text(encoding='utf-8')

    # 이미 guard가 있으면 스킵
    if '#ifndef LWIP_RAND' in content:
        return False

    # guard 없는 패턴 → guard 추가
    pattern = r'(/\* Define random number generator function \*/\n)#define LWIP_RAND\(\) \(\(u32_t\)rand\(\)\)'
    replacement = r'\1#ifndef LWIP_RAND\n#define LWIP_RAND() ((u32_t)rand())\n#endif'

    new_content, count = re.subn(pattern, replacement, content)
    if count == 0:
        return False

    file_path.write_text(new_content, encoding='utf-8')
    return True


# ------------------------------------------------------------
# main.c — CubeMX 가 매 code-gen 마다 재생성하며 USER CODE 밖 수기 수정을 삭제.
# regen_guard.py 가 감지하는 두 항목(부트마크 체인·UserTask DTCM 배치)을 외과적으로
# 재주입한다. git checkout(뭉툭) 대신 이 방식을 쓰는 이유: 같은 regen 에 섞인 '의도한'
# .ioc 변경(신규 페리페럴 init 등)을 되돌리지 않고 잃어버린 수기 수정만 복원하기 위함.
# 안정 앵커 = CubeMX 스켈레톤 고정 호출(MPU_Config/SCB_*/HAL_Init/*Config/osKernel*).
# ------------------------------------------------------------

# USER CODE 밖이라 regen 이 지우는 부트마크만 재주입 (USER CODE 안 0x1003/0x1501/0x2002/
# 0x4002 는 생존하므로 대상 아님). (mark_id, 앵커 정규식, 'after'|'before')
_BOOT_MARKS = [
    ("0x1101u", r"^[ \t]*MPU_Config\(\);[ \t]*$",              "after"),
    ("0x1201u", r"^[ \t]*SCB_EnableICache\(\);[ \t]*$",        "after"),
    ("0x1202u", r"^[ \t]*SCB_EnableDCache\(\);[ \t]*$",        "after"),
    ("0x1301u", r"^[ \t]*HAL_Init\(\);[ \t]*$",                "after"),
    ("0x1401u", r"^[ \t]*SystemClock_Config\(\);[ \t]*$",      "after"),
    ("0x1402u", r"^[ \t]*PeriphCommonClock_Config\(\);[ \t]*$", "after"),
    ("0x2001u", r"^[ \t]*/\* USER CODE BEGIN 2 \*/[ \t]*$",    "before"),
    ("0x3001u", r"^[ \t]*osKernelInitialize\(\);[ \t]*$",      "after"),
    ("0x4001u", r"^[ \t]*StartupTaskHandle = osThreadNew\(StartStartupTask, NULL, &StartupTask_attributes\);[ \t]*$", "after"),
    ("0x4FFFu", r"^[ \t]*osKernelStart\(\);[ \t]*$",           "before"),
]


def patch_main_userstack_dtcm(file_path: Path) -> bool:
    """main.c: UserTaskBuffer 를 DTCM(.dtcm_data)에 배치하는 __attribute__ 재주입.

    CubeMX 재생성: `uint32_t UserTaskBuffer[ 8192 ];`
    패치 후:       `uint32_t UserTaskBuffer[ 8192 ] __attribute__((section(".dtcm_data"), aligned(8)));`
    """
    if not file_path.exists():
        return False
    content = file_path.read_text(encoding="utf-8")

    if "UserTaskBuffer[ 8192 ] __attribute__" in content:
        return False   # 이미 배치 속성 있음

    new_content, count = re.subn(
        r"uint32_t UserTaskBuffer\[ 8192 \];",
        'uint32_t UserTaskBuffer[ 8192 ] __attribute__((section(".dtcm_data"), aligned(8)));',
        content, count=1)
    if count == 0:
        return False
    file_path.write_text(new_content, encoding="utf-8")
    return True


def patch_main_boot_marks(file_path: Path) -> bool:
    """main.c: USER CODE 밖에서 regen 에 지워진 부트 진행 마크 체인 재주입 (멱등)."""
    if not file_path.exists():
        return False
    content = file_path.read_text(encoding="utf-8")
    changed = False

    for mark_id, anchor_re, mode in _BOOT_MARKS:
        if f"XM_BOOT_DIAG_MARK({mark_id})" in content:
            continue   # 이미 존재 (USER CODE 안이든 이전 주입이든) → 스킵
        mark_line = f"  XM_BOOT_DIAG_MARK({mark_id});"
        if mode == "after":
            repl = r"\g<0>" + "\n" + mark_line
        else:  # before
            repl = mark_line + "\n" + r"\g<0>"
        new_content, count = re.subn(anchor_re, repl, content, count=1, flags=re.M)
        if count == 0:
            print(f"  [WARN]    main.c: 부트마크 {mark_id} 앵커 미발견 — "
                  f"regen_guard 가 차단할 것 (수동 확인 필요)", file=sys.stderr)
            continue
        content = new_content
        changed = True

    if changed:
        file_path.write_text(content, encoding="utf-8")
    return changed


# ------------------------------------------------------------
# ethernetif.c — ETH RTOS 객체 static 배치(2026-07-08, heap 경쟁 제거). CubeMX 가 매
# code-gen 마다 (a) static 백킹스토어 선언 블록과 (b) 세마포어 생성부(osSemaphoreNew
# static attr)를 되돌린다. task attr 사용부는 USER CODE(OS_THREAD_NEW)라 생존하므로,
# 선언이 사라지면 그 사용부가 undeclared 로 빌드가 깨진다. 아래 3훅을 재주입한다.
# (진단 카운터 g_diag_eth_*/g_eth_rx_packet_count 는 비-빌드결정적이라 대상 제외.)
# ------------------------------------------------------------
_ETH_STATIC_DECLS = (
    "\n"
    "/* [2026-07-08] ETH RTOS 객체 static 저장 — FreeRTOS heap(60KB DTCM) 경쟁 제거.\n"
    " * 095e730 이 힙을 100→60KB 로 줄이고 malloc-fail 훅을 무한정지로 바꾼 뒤, ETH bring-up\n"
    " * 의 동적 할당(세마포어×2 + ethernetif_input 스레드)이 실패하면 low_level_init 이\n"
    " * RTL8201F_Init/HAL_ETH_Start_IT 전에 얼어붙어 \"링크 UP·ARP 무응답\"이 될 수 있다.\n"
    " * 이 객체들은 전부 CPU-only RTOS 구조체(DMA 미접근)라 링커 .bss(RAM_D1) 배치 안전 —\n"
    " * DMA 디스크립터/RX풀만 RAM_D2 유지(불변). configSUPPORT_STATIC_ALLOCATION=1 전제. */\n"
    "static StaticSemaphore_t s_rxPktSemCb;\n"
    "static StaticSemaphore_t s_txPktSemCb;\n"
    "static StaticTask_t      s_ethIfTaskCb;\n"
    "static uint64_t          s_ethIfTaskStack[INTERFACE_THREAD_STACK_SIZE / sizeof(uint64_t)];\n"
)


def patch_ethernetif_eth_static_alloc(file_path: Path) -> bool:
    if not file_path.exists():
        return False
    content = file_path.read_text(encoding="utf-8")
    changed = False

    # (a) static 백킹스토어 선언 블록 — TxPktSemaphore 선언 라인 뒤 (앵커=CubeMX 고정)
    if "static StaticTask_t      s_ethIfTaskCb;" not in content:
        content, n = re.subn(
            r"(^osSemaphoreId_t TxPktSemaphore = NULL;[^\n]*\n)",
            lambda m: m.group(1) + _ETH_STATIC_DECLS,
            content, count=1, flags=re.M)
        if n == 0:
            print("  [WARN]    ethernetif.c: static 선언 앵커 미발견 — regen_guard 차단 예정",
                  file=sys.stderr)
        else:
            changed = True

    # (b) Rx 세마포어 static attr 복원 (regen 이 osSemaphoreNew(1,0,NULL) 로 되돌림)
    if "rxSemAttr" not in content:
        content, n = re.subn(
            r"^([ \t]*)RxPktSemaphore = osSemaphoreNew\(1, 0, NULL\);[ \t]*$",
            r"\1osSemaphoreAttr_t rxSemAttr = { .cb_mem = &s_rxPktSemCb, .cb_size = sizeof(s_rxPktSemCb) };\n"
            r"\1RxPktSemaphore = osSemaphoreNew(1, 0, &rxSemAttr);",
            content, count=1, flags=re.M)
        if n:
            changed = True

    # (c) Tx 세마포어 static attr 복원
    if "txSemAttr" not in content:
        content, n = re.subn(
            r"^([ \t]*)TxPktSemaphore = osSemaphoreNew\(1, 0, NULL\);[ \t]*$",
            r"\1osSemaphoreAttr_t txSemAttr = { .cb_mem = &s_txPktSemCb, .cb_size = sizeof(s_txPktSemCb) };\n"
            r"\1TxPktSemaphore = osSemaphoreNew(1, 0, &txSemAttr);",
            content, count=1, flags=re.M)
        if n:
            changed = True

    if changed:
        file_path.write_text(content, encoding="utf-8")
    return changed


# ------------------------------------------------------------
# stm32h7xx_it.c — Cortex fault 핸들러 naked dispatcher 재주입. CubeMX 는 매 code-gen 마다
# HardFault/MemManage/BusFault/UsageFault 를 stock while(1) 스텁으로 되돌린다 (2026-06-24
# 509ceb3 실사고 — fault-dump 인프라 무력화). 4개를 HardFault_CaptureAndReset() 로 분기하는
# naked dispatcher 로 복원 → .noinit 덤프 + D-Cache clean + warm reset 유지. 대상 함수
# 정의(hardfault_dump.c)와 include(hardfault_dump.h)는 사내 코드라 상존.
# ------------------------------------------------------------
_IT_FAULT_EXC = [
    ("HardFault",  "3", "HF_EXC_HARDFAULT"),
    ("MemManage",  "4", "HF_EXC_MEMMANAGE"),
    ("BusFault",   "5", "HF_EXC_BUSFAULT"),
    ("UsageFault", "6", "HF_EXC_USAGEFAULT"),
]

_IT_FAULT_NOTE = (
    '/* [XM] fault dispatcher → HardFault_CaptureAndReset() (.noinit 덤프 + warm reset).\n'
    ' *      CubeMX "Generate Code" 시 stock while(1) 스텁 회귀 → tools/build/patch_cubemx_overrides.py\n'
    ' *      (pre-build) 가 자동 복원. 2026-06-24 509ceb3 스텁 회귀 사고 재발 방지 / Rev1.1 정합. */\n'
)


def _it_naked_fn(name: str, num: str, comment: str) -> str:
    return (
        "__attribute__((naked, noreturn))\n"
        "void " + name + "_Handler(void)\n"
        "{\n"
        "    __asm volatile (\n"
        '        "tst   lr, #4                       \\n"\n'
        '        "ite   eq                           \\n"\n'
        '        "mrseq r0, msp                      \\n"\n'
        '        "mrsne r0, psp                      \\n"\n'
        '        "mov   r1, lr                       \\n"\n'
        '        "mov   r2, #' + num + '                       \\n"  /* ' + comment + ' */\n'
        '        "b     HardFault_CaptureAndReset    \\n"\n'
        "    );\n"
        "}\n"
    )


def patch_it_fault_handlers(file_path: Path) -> bool:
    """stm32h7xx_it.c: Cortex fault 핸들러 4개를 naked dispatcher 로 복원 (멱등).

    CubeMX 생성 스텁:  void HardFault_Handler(void) { ... while(1){...} }
    패치 후:           __attribute__((naked, noreturn)) void HardFault_Handler(void)
                       { __asm volatile( ... "b HardFault_CaptureAndReset\\n" ); }
    """
    if not file_path.exists():
        return False
    content = file_path.read_text(encoding="utf-8")
    # 이미 복원됨 (naked HardFault 존재) → 스킵
    if re.search(r"__attribute__\(\(naked, noreturn\)\)\s*\nvoid HardFault_Handler", content):
        return False
    changed = False
    for name, num, comment in _IT_FAULT_EXC:
        pattern = r"void " + name + r"_Handler\(void\)\s*\n\{.*?\n\}\n"
        repl = (_IT_FAULT_NOTE if name == "HardFault" else "") + _it_naked_fn(name, num, comment)
        new_content, n = re.subn(pattern, lambda m, r=repl: r, content, count=1, flags=re.S)
        if n == 0:
            print(f"  [WARN]    stm32h7xx_it.c: {name}_Handler 스텁 앵커 미발견 — 수동 확인",
                  file=sys.stderr)
            continue
        content = new_content
        changed = True
    if changed:
        file_path.write_text(content, encoding="utf-8")
    return changed


# ============================================================
# 패치 레지스트리
# ============================================================
PATCHES = [
    (
        MODULE_ROOT / "Middlewares" / "Third_Party" / "LwIP" / "system" / "arch" / "cc.h",
        patch_cc_h_lwip_rand,
        "cc.h: LWIP_RAND #ifndef guard",
    ),
    (
        MODULE_ROOT / "Core" / "Src" / "main.c",
        patch_main_userstack_dtcm,
        "main.c: UserTask 스택 .dtcm_data 배치",
    ),
    (
        MODULE_ROOT / "Core" / "Src" / "main.c",
        patch_main_boot_marks,
        "main.c: 부트 진행 마크(XM_BOOT_DIAG_MARK) 체인",
    ),
    (
        MODULE_ROOT / "LWIP" / "Target" / "ethernetif.c",
        patch_ethernetif_eth_static_alloc,
        "ethernetif.c: ETH RTOS 객체 static 배치",
    ),
    (
        MODULE_ROOT / "Core" / "Src" / "stm32h7xx_it.c",
        patch_it_fault_handlers,
        "stm32h7xx_it.c: fault 핸들러 naked dispatcher (regen 자동복원)",
    ),
    # 향후 CubeMX override 패치 추가 시 여기에 tuple 추가
    # (Path, patch_func, description),
]


def main():
    patched = 0
    skipped = 0
    errors = 0

    print("[patch_cubemx_overrides] Checking CubeMX-managed files...")

    for file_path, patch_func, desc in PATCHES:
        try:
            if patch_func(file_path):
                print(f"  [PATCHED] {desc}")
                patched += 1
            else:
                print(f"  [OK]      {desc}")
                skipped += 1
        except Exception as e:
            print(f"  [ERROR]   {desc}: {e}", file=sys.stderr)
            errors += 1

    print(f"[patch_cubemx_overrides] Done: {patched} patched, {skipped} ok, {errors} errors")
    return 1 if errors > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
