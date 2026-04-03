#!/usr/bin/env python3
"""
fw_packager.py - AGR_BOOT V2 Firmware Packager

Combines a raw app.bin with a 1KB AGR_FwInfo_t header to produce a packaged binary.

Location: Extension_Module/tools/ (App FW build pipeline)
    This tool operates on App FW build output, NOT on the bootloader.
    Bootloader-specific tools (ftp_upload.py, gen_boot_config.py) remain in
    Extension_Module_BootLoader/tools/.

Usage:
    # Explicit version
    python fw_packager.py <app.bin> -v 1.2.0.0 -m XM

    # Read version from version.h (recommended for Post-Build)
    python fw_packager.py <app.bin> --version-from-header ../Core/Inc/version.h -m XM

Output naming:
    Default: <MODULE>_<MAJOR>_<MINOR>_<PATCH>_<DEBUG>.bin
    Example: XM10_1_2_0_0.bin
    Override with -o <path>

Module name mapping (for output filename):
    XM -> XM10, CM -> CM, MD -> MD, SM -> SM

The packaged binary is what the bootloader expects in the Active Slot:
  - Offset 0x000: AGR_FwInfo_t (64 bytes) + zero-padding (960 bytes) = 1KB
  - Offset 0x400: Application binary (VTOR at this address)

Header fields:
  - signature:       'AGRBOOT\\x01'
  - fw_size:         app.bin file size
  - fw_crc32:        CRC-32 of app.bin
  - fw_start_offset: 0x400 (fixed)
  - version:         major.minor.patch.debug (each 0-255)
  - build_timestamp: Unix epoch (auto or --timestamp)
  - target_module:   --module (XM=0x10, CM=0x20, MD=0x30, SM=0x40)
  - hw_rev_min/max:  --hw-rev-min / --hw-rev-max
  - flags:           0x00 (reserved)
  - reserved:        32 bytes of zeros

Copyright (c) Angel Robotics Inc.
"""
from __future__ import annotations

import argparse
import os
import re
import struct
import sys
import time
import zlib
from pathlib import Path


# AGR_FwInfo_t layout (64 bytes, packed)
FW_INFO_FORMAT = "<8sIIIBBBBIBBBB32s"
FW_INFO_SIZE = 64
HEADER_PAD_SIZE = 1024  # 1KB total header
FW_START_OFFSET = 0x400
MAX_FW_SIZE = 767 * 1024  # 767KB

SIGNATURE = b'AGRBOOT\x01'

MODULE_MAP = {
    'XM': 0x10,
    'CM': 0x20,
    'MD': 0x30,
    'SM': 0x40,
}

MODULE_FILENAME_MAP = {
    'XM': 'XM10',
    'CM': 'CM',
    'MD': 'MD',
    'SM': 'SM',
}

HWREV_MAP = {
    '1.1': 0x11,
    '2.0': 0x20,
    '0x11': 0x11,
    '0x20': 0x20,
}


def parse_version(ver_str: str) -> tuple[int, int, int, int]:
    """Parse version string 'major.minor.patch[.debug]' -> (maj, min, pat, dbg)"""
    parts = ver_str.split('.')
    if len(parts) < 3:
        raise ValueError(f"Version must be major.minor.patch[.debug], got: {ver_str}")
    major = int(parts[0])
    minor = int(parts[1])
    patch = int(parts[2])
    debug = int(parts[3]) if len(parts) > 3 else 0
    for name, v in [("major", major), ("minor", minor),
                    ("patch", patch), ("debug", debug)]:
        if not (0 <= v <= 255):
            raise ValueError(f"Version {name} must be 0-255, got: {v}")
    return (major, minor, patch, debug)


def parse_version_from_header(header_path: str) -> tuple[int, int, int, int]:
    """Parse FW_VER_MAJOR/MINOR/PATCH/DEBUG from a C version.h file."""
    path = Path(header_path)
    if not path.exists():
        raise FileNotFoundError(f"version.h not found: {path}")

    text = path.read_text(encoding='utf-8')

    def extract(name: str) -> int:
        m = re.search(rf'#define\s+{name}\s+(\d+)', text)
        if not m:
            raise ValueError(f"{name} not found in {path}")
        val = int(m.group(1))
        if not (0 <= val <= 255):
            raise ValueError(f"{name}={val} out of range (0-255) in {path}")
        return val

    major = extract('FW_VER_MAJOR')
    minor = extract('FW_VER_MINOR')
    patch = extract('FW_VER_PATCH')
    debug = extract('FW_VER_DEBUG')
    return (major, minor, patch, debug)


def parse_hw_rev(rev_str: str) -> int:
    """Parse hw_rev string ('1.1', '2.0', '0x11', '0x20') -> uint8"""
    if rev_str in HWREV_MAP:
        return HWREV_MAP[rev_str]
    try:
        val = int(rev_str, 0)
        if 0 <= val <= 255:
            return val
        raise ValueError
    except ValueError:
        raise ValueError(f"Invalid hw_rev: {rev_str}. Use: 1.1, 2.0, 0x11, 0x20, or 0-255")


def compute_crc32(data: bytes) -> int:
    """Compute CRC-32 (IEEE 802.3) matching AGR_Boot_CRC32()."""
    return zlib.crc32(data) & 0xFFFFFFFF


def generate_output_name(module: str, ver: tuple[int, int, int, int],
                         input_dir: Path) -> Path:
    """Generate output filename: <MODULE>_<MAJ>_<MIN>_<PAT>_<DBG>.bin"""
    module_name = MODULE_FILENAME_MAP.get(module, module)
    filename = f"{module_name}_{ver[0]}_{ver[1]}_{ver[2]}_{ver[3]}.bin"
    return input_dir / filename


def main():
    parser = argparse.ArgumentParser(
        description="AGR_BOOT V2 Firmware Packager - creates packaged binary from app.bin")
    parser.add_argument('input', help="Input app.bin file path")
    parser.add_argument('-o', '--output', default=None,
                        help="Output path (default: <MODULE>_<MAJ>_<MIN>_<PAT>_<DBG>.bin)")
    parser.add_argument('-v', '--version', default=None,
                        help="FW version: major.minor.patch[.debug]")
    parser.add_argument('--version-from-header', default=None, metavar='PATH',
                        help="Read version from version.h (path to the .h file)")
    parser.add_argument('-m', '--module', default='XM', choices=MODULE_MAP.keys(),
                        help="Target module (default: XM)")
    parser.add_argument('--hw-rev-min', default='1.1',
                        help="Minimum HW revision (default: 1.1)")
    parser.add_argument('--hw-rev-max', default='2.0',
                        help="Maximum HW revision (default: 2.0)")
    parser.add_argument('--timestamp', default=None, type=int,
                        help="Build timestamp (Unix epoch, default: now)")
    parser.add_argument('--dry-run', action='store_true',
                        help="Print header info without writing")

    args = parser.parse_args()

    # Version source: --version-from-header (priority) or -v (fallback)
    if args.version_from_header:
        ver = parse_version_from_header(args.version_from_header)
        print(f"  Version source: {args.version_from_header}")
    elif args.version:
        ver = parse_version(args.version)
        print(f"  Version source: CLI argument")
    else:
        parser.error("Either -v/--version or --version-from-header is required")
        return  # unreachable

    input_path = Path(args.input)
    if not input_path.exists():
        print(f"ERROR: Input file not found: {input_path}", file=sys.stderr)
        sys.exit(1)

    app_data = input_path.read_bytes()
    raw_size = len(app_data)

    if raw_size == 0:
        print("ERROR: Input file is empty", file=sys.stderr)
        sys.exit(1)

    # If .bin starts with AGR_BOOT FwInfo signature, strip the 1KB header.
    # Why: When linker script has FW_HEADER at 0x08040000, objcopy -O binary
    #      includes the FwInfo in the .bin. fw_packager prepends its own header,
    #      causing double-header → App code shifted by 1KB → crash on boot.
    if app_data[:8] == SIGNATURE:
        print(f"  Detected embedded FwInfo header - stripping first {HEADER_PAD_SIZE} bytes")
        app_data = app_data[HEADER_PAD_SIZE:]
        raw_size = len(app_data)

    if raw_size > MAX_FW_SIZE:
        print(f"ERROR: FW size ({raw_size} bytes) exceeds max ({MAX_FW_SIZE} bytes)",
              file=sys.stderr)
        sys.exit(1)

    pad_needed = (4 - (raw_size % 4)) % 4
    if pad_needed > 0:
        app_data = app_data + b'\xFF' * pad_needed
        print(f"  4-byte align:   {raw_size} -> {len(app_data)} (+{pad_needed}B, padded with 0xFF)")
    fw_size = len(app_data)

    hw_rev_min = parse_hw_rev(args.hw_rev_min)
    hw_rev_max = parse_hw_rev(args.hw_rev_max)
    target_module = MODULE_MAP[args.module]
    timestamp = args.timestamp if args.timestamp else int(time.time())
    fw_crc32 = compute_crc32(app_data)

    fw_info = struct.pack(
        FW_INFO_FORMAT,
        SIGNATURE,              # signature[8]
        fw_size,                # fw_size
        fw_crc32,               # fw_crc32
        FW_START_OFFSET,        # fw_start_offset
        ver[0], ver[1], ver[2], ver[3],  # ver_major/minor/patch/debug
        timestamp,              # build_timestamp
        target_module,          # target_module
        hw_rev_min,             # hw_rev_min
        hw_rev_max,             # hw_rev_max
        0,                      # flags
        b'\x00' * 32,          # reserved[32]
    )

    assert len(fw_info) == FW_INFO_SIZE, \
        f"FW info struct size mismatch: {len(fw_info)} != {FW_INFO_SIZE}"

    # 1KB header = 64B AGR_FwInfo_t + 960B zero padding
    header = fw_info + b'\x00' * (HEADER_PAD_SIZE - FW_INFO_SIZE)
    packaged = header + app_data

    # Print summary
    print("=" * 60)
    print("  AGR_BOOT V2 Firmware Packager")
    print("=" * 60)
    print(f"  Input:          {input_path}")
    print(f"  FW Size:        {fw_size} bytes ({fw_size / 1024:.1f} KB)")
    print(f"  FW CRC-32:      0x{fw_crc32:08X}")
    print(f"  Version:        {ver[0]}.{ver[1]}.{ver[2]}.{ver[3]}")
    print(f"  Module:         {args.module} (0x{target_module:02X})")
    print(f"  HW Rev Min:     0x{hw_rev_min:02X}")
    print(f"  HW Rev Max:     0x{hw_rev_max:02X}")
    print(f"  Timestamp:      {timestamp} "
          f"({time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(timestamp))} UTC)")
    print(f"  Header Size:    {HEADER_PAD_SIZE} bytes")
    print(f"  Total Package:  {len(packaged)} bytes ({len(packaged) / 1024:.1f} KB)")
    print("=" * 60)

    if args.dry_run:
        print("  [DRY RUN] No file written.")
        return

    if args.output:
        output_path = Path(args.output)
    else:
        output_path = generate_output_name(args.module, ver, input_path.parent)

    output_path.write_bytes(packaged)
    print(f"  Output:         {output_path}")
    print(f"  Filename:       {output_path.name}")
    print("  Done.")


if __name__ == "__main__":
    main()
