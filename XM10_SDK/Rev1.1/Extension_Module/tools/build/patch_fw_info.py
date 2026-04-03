#!/usr/bin/env python3
"""
patch_fw_info.py — Post-build: Patch AGR_FwInfo_t in ELF with correct fw_size & fw_crc32.

Usage (CMake post-build):
    python patch_fw_info.py <ELF_PATH> [--objcopy <OBJCOPY_PATH>]

What it does:
    1. Extracts full binary from ELF (objcopy -O binary)
    2. Computes fw_size and CRC-32 of app portion (offset 0x400+)
    3. Patches the .fw_header section in the ELF with correct values
    4. Regenerates .bin and .hex from patched ELF

Why:
    The linker embeds AGR_FwInfo_t at 0x08040000 with placeholder fw_size=0
    and fw_crc32=0. The BL requires fw_size > 0 and valid CRC to jump to App.
    This script fills in the correct values after linking, enabling SWD debug
    without needing FTP upload.

Copyright (c) Angel Robotics Inc.
"""

import argparse
import struct
import subprocess
import sys
import zlib
from pathlib import Path


# AGR_FwInfo_t field offsets (64-byte packed struct)
FWINFO_OFFSET_FW_SIZE  = 8   # uint32_t fw_size
FWINFO_OFFSET_FW_CRC32 = 12  # uint32_t fw_crc32
FWINFO_SIZE            = 64
FW_HEADER_SIZE         = 0x400  # 1KB header in flash layout


def find_objcopy():
    """Find arm-none-eabi-objcopy in PATH or STM32CubeIDE plugins."""
    import shutil
    objcopy = shutil.which("arm-none-eabi-objcopy")
    if objcopy:
        return objcopy

    # STM32CubeIDE bundled toolchain (Windows)
    from glob import glob
    patterns = [
        "C:/ST/STM32CubeIDE*/STM32CubeIDE/plugins/"
        "com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*.win32_*/tools/bin/"
        "arm-none-eabi-objcopy.exe"
    ]
    for pat in patterns:
        matches = sorted(glob(pat), reverse=True)
        if matches:
            return matches[0]

    return None


def main():
    parser = argparse.ArgumentParser(description="Patch AGR_FwInfo_t in ELF after build")
    parser.add_argument("elf", help="Path to ELF file (modified in-place)")
    parser.add_argument("--objcopy", default=None, help="Path to arm-none-eabi-objcopy")
    args = parser.parse_args()

    elf_path = Path(args.elf)
    if not elf_path.exists():
        print(f"ERROR: ELF not found: {elf_path}", file=sys.stderr)
        sys.exit(1)

    objcopy = args.objcopy or find_objcopy()
    if not objcopy:
        print("ERROR: arm-none-eabi-objcopy not found", file=sys.stderr)
        sys.exit(1)

    # --- Step 1: Extract full binary from ELF ---
    tmp_bin = elf_path.with_suffix(".tmp.bin")
    subprocess.run([objcopy, "-O", "binary", str(elf_path), str(tmp_bin)], check=True)
    full_data = bytearray(tmp_bin.read_bytes())

    if len(full_data) <= FW_HEADER_SIZE:
        print("ERROR: Binary too small — no app data after header", file=sys.stderr)
        tmp_bin.unlink(missing_ok=True)
        sys.exit(1)

    # --- Step 2: Compute fw_size and CRC-32 of app portion ---
    app_data = full_data[FW_HEADER_SIZE:]

    # Trim trailing 0xFF (erased flash padding from objcopy)
    while len(app_data) > 0 and app_data[-1] == 0xFF:
        app_data = app_data[:-1]

    fw_size = len(app_data)
    fw_crc32 = zlib.crc32(bytes(app_data)) & 0xFFFFFFFF

    # --- Step 3: Patch the fw_header in the binary ---
    struct.pack_into('<I', full_data, FWINFO_OFFSET_FW_SIZE, fw_size)
    struct.pack_into('<I', full_data, FWINFO_OFFSET_FW_CRC32, fw_crc32)

    # --- Step 4: Write patched .fw_header section and update ELF ---
    fw_header_bin = elf_path.with_suffix(".fw_header.bin")
    fw_header_bin.write_bytes(bytes(full_data[:FWINFO_SIZE]))

    subprocess.run([
        objcopy,
        "--update-section", f".fw_header={fw_header_bin}",
        str(elf_path)
    ], check=True)

    # --- Step 5: Regenerate .bin and .hex from patched ELF ---
    final_bin = elf_path.with_suffix(".bin")
    final_hex = elf_path.with_suffix(".hex")
    subprocess.run([objcopy, "-O", "binary", str(elf_path), str(final_bin)], check=True)
    subprocess.run([objcopy, "-O", "ihex", str(elf_path), str(final_hex)], check=True)

    # --- Step 6: Generate app-only .bin for fw_packager.py (no .fw_header) ---
    # fw_packager.py adds its own 1KB header, so input must NOT contain .fw_header.
    app_only_bin = elf_path.parent / (elf_path.stem + "_app.bin")
    subprocess.run([
        objcopy, "--remove-section=.fw_header",
        "-O", "binary", str(elf_path), str(app_only_bin)
    ], check=True)

    # --- Cleanup ---
    tmp_bin.unlink(missing_ok=True)
    fw_header_bin.unlink(missing_ok=True)

    print(f"  [fw_info] Patched: fw_size={fw_size} ({fw_size/1024:.1f}KB), "
          f"fw_crc32=0x{fw_crc32:08X}")
    print(f"  [fw_info] App-only binary: {app_only_bin.name}")


if __name__ == "__main__":
    main()
