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

import re
import sys
from pathlib import Path

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


# ============================================================
# 패치 레지스트리
# ============================================================
PATCHES = [
    (
        MODULE_ROOT / "Middlewares" / "Third_Party" / "LwIP" / "system" / "arch" / "cc.h",
        patch_cc_h_lwip_rand,
        "cc.h: LWIP_RAND #ifndef guard",
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
