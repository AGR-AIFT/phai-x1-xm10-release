#!/usr/bin/env python3
"""
Parse STM32CubeIDE .cproject XML and generate CMakeLists.txt for CLI builds.

Generates:
  1. CMakeLists.txt   — full CMake build configuration
  2. compile_flags.txt — clangd fallback (legacy, optional)

The .cproject remains the Single Source of Truth. This script is a translator
that runs post-CubeMX code generation.

Usage:
    python cproject_to_cmake.py <ProjDirPath>
    python cproject_to_cmake.py <ProjDirPath> --config Release
    python cproject_to_cmake.py <ProjDirPath> --compile-flags   # also emit compile_flags.txt

Shared across all Angel Robotics STM32 modules (XM, CM, MD, SM).
"""

import argparse
import fnmatch
import os
import re
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


# ==============================================================================
# Data Structures
# ==============================================================================

@dataclass
class SourceEntry:
    """A source folder with its exclusion patterns."""
    name: str                      # e.g. "Core", "XM_FW"
    excludes: list[str] = field(default_factory=list)  # relative to name


@dataclass
class BuildConfig:
    """All build information extracted from .cproject."""
    project_name: str = ""
    mcu_model: str = ""            # e.g. STM32H743XIHx
    cpu: str = "cortex-m7"
    fpu: str = "fpv5-d16"
    float_abi: str = "hard"
    c_standard: str = "gnu11"
    linker_script: str = ""        # absolute path to .ld
    includes: list[str] = field(default_factory=list)
    defines: list[str] = field(default_factory=list)
    source_entries: list[SourceEntry] = field(default_factory=list)
    sources_c: list[str] = field(default_factory=list)
    sources_asm: list[str] = field(default_factory=list)
    # Linker: libraries and paths
    libraries: list[str] = field(default_factory=list)      # -l names (e.g. "XM_Lib")
    library_paths: list[str] = field(default_factory=list)   # -L absolute paths
    printf_float: bool = False     # -u _printf_float
    scanf_float: bool = False      # -u _scanf_float


# ==============================================================================
# .cproject XML Parsing
# ==============================================================================

def resolve_path(value: str, proj_dir: str) -> str:
    """Resolve Eclipse path variables to absolute path."""
    value = value.strip().strip('"')
    m = re.match(r'\$\{workspace_loc:/\$\{ProjName\}/(.+)\}', value)
    if m:
        return os.path.normpath(os.path.join(proj_dir, m.group(1).replace("\\", "/")))
    m = re.match(r'\$\{workspace_loc:/([^}]+)/(.+)\}', value)
    if m:
        return os.path.normpath(os.path.join(proj_dir, m.group(2).replace("\\", "/")))
    if value.startswith("../") or value.startswith("./"):
        resolved = os.path.normpath(os.path.join(proj_dir, value))
        if not os.path.isdir(resolved) and value.startswith("../"):
            alt = os.path.normpath(os.path.join(proj_dir, value[3:]))
            if os.path.isdir(alt):
                return alt
        return resolved
    if os.path.isabs(value):
        return os.path.normpath(value)
    return os.path.normpath(os.path.join(proj_dir, value))


def find_debug_config(root: ET.Element) -> Optional[ET.Element]:
    """Find the Debug cconfiguration element."""
    for c in root.findall(".//cconfiguration"):
        cid = c.get("id", "").lower()
        cname = c.get("name", "")
        if "debug" in cid or "Debug" in cname:
            return c
    configs = root.findall(".//cconfiguration")
    return configs[0] if configs else None


def extract_build_config(cproject_path: str, proj_dir: str) -> BuildConfig:
    """Extract all build information from .cproject XML."""
    tree = ET.parse(cproject_path)
    root = tree.getroot()
    cfg = BuildConfig()
    config = find_debug_config(root)
    if config is None:
        print("Warning: No cconfiguration found in .cproject", file=sys.stderr)
        return cfg

    seen_inc: set[str] = set()
    seen_def: set[str] = set()

    # --- MCU model ---
    for opt in config.iter("option"):
        opt_name = opt.get("name", "")
        if opt_name == "MCU" or "mcu" in opt.get("id", "").lower():
            val = opt.get("value", "")
            if val:
                cfg.mcu_model = val
                break

    # --- Compiler options ---
    for tool in config.iter("tool"):
        tool_id = tool.get("id", "")
        if "assembler" in tool_id or "c.linker" in tool_id:
            continue
        if "c.compiler" not in tool_id and "cpp.compiler" not in tool_id:
            continue

        for opt in tool.findall("option"):
            opt_id = opt.get("id", "")
            if "includepaths" in opt_id:
                for lv in opt.findall("listOptionValue"):
                    val = lv.get("value", "")
                    if not val:
                        continue
                    resolved = resolve_path(val, proj_dir)
                    if resolved and resolved not in seen_inc:
                        seen_inc.add(resolved)
                        cfg.includes.append(resolved)
            elif "definedsymbols" in opt_id:
                for lv in opt.findall("listOptionValue"):
                    val = lv.get("value", "")
                    if val and val not in seen_def:
                        seen_def.add(val)
                        cfg.defines.append(val)

    # --- Linker options (script, libraries, library paths) ---
    for tool in config.iter("tool"):
        tool_id = tool.get("id", "")
        if "c.linker" not in tool_id:
            continue
        for opt in tool.findall("option"):
            opt_id = opt.get("id", "")
            opt_name = opt.get("name", "")
            # Linker script (-T)
            if "script" in opt_id.lower() or "script" in opt_name.lower():
                val = opt.get("value", "")
                if val:
                    cfg.linker_script = resolve_path(val, proj_dir)
            # Libraries (-l)
            elif "option.libraries" in opt_id and "directories" not in opt_id:
                for lv in opt.findall("listOptionValue"):
                    val = lv.get("value", "").strip().strip('"')
                    if val and val not in cfg.libraries:
                        cfg.libraries.append(val)
            # Library search paths (-L)
            elif "option.directories" in opt_id:
                for lv in opt.findall("listOptionValue"):
                    val = lv.get("value", "")
                    if not val:
                        continue
                    resolved = resolve_path(val, proj_dir)
                    if resolved and resolved not in cfg.library_paths:
                        cfg.library_paths.append(resolved)
        break  # Only first linker tool (Debug config)

    # --- Printf/Scanf float options ---
    for opt in config.iter("option"):
        opt_super = opt.get("superClass", "")
        val = opt.get("value", "")
        if opt_super.endswith(".option.nanoprintffloat") and val == "true":
            cfg.printf_float = True
        elif opt_super.endswith(".option.nanoscanffloat") and val == "true":
            cfg.scanf_float = True

    # --- Source entries (folders + per-folder exclusions) ---
    for se in config.iter("sourceEntries"):
        for entry in se.findall("entry"):
            if entry.get("kind", "") != "sourcePath":
                continue
            name = entry.get("name", "")
            if not name:
                continue
            excludes: list[str] = []
            excluding = entry.get("excluding", "")
            if excluding:
                for pat in excluding.split("|"):
                    pat = pat.strip()
                    if pat:
                        excludes.append(pat)
            cfg.source_entries.append(SourceEntry(name=name, excludes=excludes))
        break  # Only first sourceEntries block (Debug config)

    # --- Project name from build artifact ---
    cfg.project_name = os.path.basename(proj_dir)
    for elem in config.iter("configuration"):
        art_name = elem.get("artifactName", "")
        if art_name and art_name != "${ProjName}":
            cfg.project_name = art_name
            break

    # --- FPU settings ---
    # Eclipse stores as enumerated values like:
    #   com.st.stm32cube.ide.mcu.gnu.managedbuild.option.fpu.value.fpv5-d16
    #   com.st.stm32cube.ide.mcu.gnu.managedbuild.option.floatabi.value.hard
    # Must match specifically to avoid false hits (e.g. nanoprintffloat → "true")
    for opt in config.iter("option"):
        opt_super = opt.get("superClass", "")
        val = opt.get("value", "")
        if not val:
            continue
        if opt_super.endswith(".option.fpu"):
            # Extract "fpv5-d16" from "...option.fpu.value.fpv5-d16"
            cfg.fpu = val.split(".value.")[-1] if ".value." in val else val
        elif opt_super.endswith(".option.floatabi"):
            # Extract "hard" from "...option.floatabi.value.hard"
            cfg.float_abi = val.split(".value.")[-1] if ".value." in val else val

    return cfg


# ==============================================================================
# Source File Discovery
# ==============================================================================

# Directories to always skip (build artifacts, version control, tools)
SKIP_DIRS = {
    "Debug", "Release", "build", ".git", ".svn", "__pycache__",
    "tools", ".settings", ".vscode", ".cursor",
    "ioif_legacy",  # V1→V2 migration: legacy IOIF sources replaced by IOIF/Src
}


def is_excluded(rel_path_in_entry: str, excludes: list[str]) -> bool:
    """Check if a path (relative to source entry root) matches any exclusion pattern."""
    rel_posix = rel_path_in_entry.replace("\\", "/")
    for pattern in excludes:
        pat = pattern.replace("\\", "/").rstrip("/")
        # Exact prefix match (directory exclusion)
        if rel_posix == pat or rel_posix.startswith(pat + "/"):
            return True
        # Exact file match
        if rel_posix == pat:
            return True
        # Filename glob match
        if fnmatch.fnmatch(rel_posix, pat):
            return True
    return False


def discover_sources(proj_dir: str, source_entries: list['SourceEntry']) -> tuple[list[str], list[str]]:
    """Find all .c and .s files within declared source entries, respecting exclusions.

    If source_entries is empty, falls back to scanning all directories
    (for projects without explicit sourceEntries in .cproject).
    """
    c_files: list[str] = []
    asm_files: list[str] = []

    if not source_entries:
        # Fallback: scan entire project tree
        source_entries = [SourceEntry(name=".")]

    for se in source_entries:
        entry_dir = os.path.join(proj_dir, se.name) if se.name != "." else proj_dir
        if not os.path.isdir(entry_dir):
            print(f"  Warning: source entry '{se.name}' not found, skipping", file=sys.stderr)
            continue

        for dirpath, dirnames, filenames in os.walk(entry_dir):
            # Skip build/VCS directories
            dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]

            rel_dir = os.path.relpath(dirpath, entry_dir).replace("\\", "/")
            if rel_dir == ".":
                rel_dir = ""

            for fname in filenames:
                # Path relative to the source entry root (for exclusion matching)
                rel_in_entry = f"{rel_dir}/{fname}" if rel_dir else fname

                if is_excluded(rel_in_entry, se.excludes):
                    continue

                # Path relative to project root (for CMakeLists.txt)
                if se.name != ".":
                    rel_proj = f"{se.name}/{rel_in_entry}"
                else:
                    rel_proj = rel_in_entry

                ext = os.path.splitext(fname)[1].lower()
                if ext == ".c":
                    c_files.append(rel_proj)
                elif ext == ".s":
                    asm_files.append(rel_proj)

    c_files.sort()
    asm_files.sort()
    return c_files, asm_files


# ==============================================================================
# CMakeLists.txt Generation
# ==============================================================================

def make_relative(abs_path: str, proj_dir: str) -> str:
    """Convert absolute path to ${CMAKE_CURRENT_SOURCE_DIR}/... form."""
    try:
        rel = os.path.relpath(abs_path, proj_dir).replace("\\", "/")
        if rel.startswith(".."):
            return abs_path.replace("\\", "/")
        return "${CMAKE_CURRENT_SOURCE_DIR}/" + rel
    except ValueError:
        return abs_path.replace("\\", "/")


def generate_cmakelists(cfg: BuildConfig, proj_dir: str) -> str:
    """Generate CMakeLists.txt content from BuildConfig."""
    lines: list[str] = []

    # --- Header ---
    lines.append("# ==============================================================================")
    lines.append(f"# CMakeLists.txt for {cfg.project_name}")
    lines.append("# Auto-generated by cproject_to_cmake.py from .cproject")
    lines.append("# DO NOT EDIT — re-run the script after CubeMX code generation")
    lines.append("# ==============================================================================")
    lines.append("")
    lines.append("cmake_minimum_required(VERSION 3.22)")
    lines.append("")

    # --- Toolchain ---
    toolchain_candidates = [
        "tools/stm32_gcc_toolchain.cmake",
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/stm32_gcc_toolchain.cmake",
    ]
    lines.append("# Toolchain file (set via -DCMAKE_TOOLCHAIN_FILE or auto-detect)")
    lines.append("if(NOT CMAKE_TOOLCHAIN_FILE)")
    lines.append("    # Search common locations")
    lines.append("    foreach(_tc")
    lines.append('        "${CMAKE_CURRENT_SOURCE_DIR}/tools/stm32_gcc_toolchain.cmake"')
    lines.append('        "${CMAKE_CURRENT_SOURCE_DIR}/../tools/stm32_gcc_toolchain.cmake"')
    lines.append("    )")
    lines.append("        if(EXISTS ${_tc})")
    lines.append("            set(CMAKE_TOOLCHAIN_FILE ${_tc})")
    lines.append("            break()")
    lines.append("        endif()")
    lines.append("    endforeach()")
    lines.append("endif()")
    lines.append("")

    # --- Project ---
    lines.append(f'project({cfg.project_name} C ASM)')
    lines.append("")

    # --- MCU flags ---
    lines.append("# --- MCU Configuration ---")
    lines.append(f'set(MCU_FLAGS "-mcpu={cfg.cpu} -mthumb -mfpu={cfg.fpu} -mfloat-abi={cfg.float_abi}")')
    lines.append(f'set(CMAKE_C_STANDARD 11)')
    lines.append(f'set(CMAKE_C_STANDARD_REQUIRED ON)')
    lines.append(f'set(CMAKE_C_EXTENSIONS ON)  # gnu11')
    lines.append('set(CMAKE_EXPORT_COMPILE_COMMANDS ON)')
    lines.append("")
    lines.append("# Apply MCU flags")
    lines.append('set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MCU_FLAGS}")')
    lines.append('set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} ${MCU_FLAGS}")')
    lines.append("")

    # --- Linker script ---
    if cfg.linker_script:
        ld_rel = make_relative(cfg.linker_script, proj_dir)
        lines.append(f'set(LINKER_SCRIPT "{ld_rel}")')
        lines.append('set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${LINKER_SCRIPT}")')
    lines.append("")

    # --- Source files ---
    lines.append("# --- Source Files ---")
    lines.append("set(SOURCES")
    for src in cfg.sources_c:
        lines.append(f'    "{src}"' if " " in src else f"    {src}")
    lines.append(")")
    lines.append("")

    if cfg.sources_asm:
        lines.append("set(ASM_SOURCES")
        for src in cfg.sources_asm:
            lines.append(f'    "{src}"' if " " in src else f"    {src}")
        lines.append(")")
        lines.append("")

    # --- Target ---
    elf_name = cfg.project_name
    if cfg.sources_asm:
        lines.append(f'add_executable(${{PROJECT_NAME}} ${{SOURCES}} ${{ASM_SOURCES}})')
    else:
        lines.append(f'add_executable(${{PROJECT_NAME}} ${{SOURCES}})')
    lines.append("")

    # --- Include paths ---
    lines.append("# --- Include Paths ---")
    lines.append("target_include_directories(${PROJECT_NAME} PRIVATE")
    for inc in cfg.includes:
        inc_rel = make_relative(inc, proj_dir)
        lines.append(f'    "{inc_rel}"' if " " in inc_rel else f"    {inc_rel}")
    lines.append(")")
    lines.append("")

    # --- Defines ---
    lines.append("# --- Preprocessor Defines ---")
    lines.append("target_compile_definitions(${PROJECT_NAME} PRIVATE")
    for d in cfg.defines:
        # Skip DEBUG — handled by CMAKE_C_FLAGS_DEBUG
        if d == "DEBUG":
            lines.append(f"    $<$<CONFIG:Debug>:{d}>")
        else:
            lines.append(f"    {d}")
    lines.append(")")
    lines.append("")

    # --- Libraries (-l, -L, printf/scanf float) ---
    if cfg.libraries or cfg.printf_float or cfg.scanf_float:
        lines.append("# --- Libraries ---")

        # Library search paths
        for lib_path in cfg.library_paths:
            lp_rel = make_relative(lib_path, proj_dir)
            lines.append(f"target_link_directories(${{PROJECT_NAME}} PRIVATE")
            lines.append(f'    "{lp_rel}"' if " " in lp_rel else f"    {lp_rel}")
            lines.append(")")
        if cfg.library_paths:
            lines.append("")

        # Libraries and float options
        lines.append("target_link_libraries(${PROJECT_NAME} PRIVATE")
        for lib in cfg.libraries:
            lines.append(f"    {lib}")
        if cfg.printf_float:
            lines.append('    -u _printf_float')
        if cfg.scanf_float:
            lines.append('    -u _scanf_float')
        lines.append(")")
        lines.append("")

    # --- Post-build: .bin and .hex ---
    lines.append("# --- Post-build: generate .bin and .hex ---")
    lines.append("add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD")
    lines.append('    COMMAND ${CMAKE_OBJCOPY} -O ihex   "$<TARGET_FILE:${PROJECT_NAME}>" "${PROJECT_NAME}.hex"')
    lines.append('    COMMAND ${CMAKE_OBJCOPY} -O binary "$<TARGET_FILE:${PROJECT_NAME}>" "${PROJECT_NAME}.bin"')
    lines.append('    COMMAND ${CMAKE_SIZE} "$<TARGET_FILE:${PROJECT_NAME}>"')
    lines.append('    COMMENT "Generating .hex and .bin, printing size"')
    lines.append(")")
    lines.append("")

    return "\n".join(lines)


# ==============================================================================
# compile_flags.txt Generation (legacy, for clangd fallback)
# ==============================================================================

def generate_compile_flags(cfg: BuildConfig, proj_dir: str) -> str:
    """Generate compile_flags.txt content."""
    lines = [
        "-xc",
        f"-std={cfg.c_standard}",
        "--target=arm-none-eabi",
    ]
    for d in cfg.defines:
        lines.append(f"-D{d}")
    for inc in cfg.includes:
        lines.append(f"-I{inc}")
    return "\n".join(lines) + "\n"


# ==============================================================================
# Main
# ==============================================================================

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate CMakeLists.txt (and optionally compile_flags.txt) from STM32CubeIDE .cproject"
    )
    parser.add_argument("proj_dir", help="Project directory (where .cproject lives)")
    parser.add_argument("--output", "-o", help="Output directory for CMakeLists.txt (default: proj_dir)")
    parser.add_argument("--config", default="Debug", choices=["Debug", "Release"],
                        help="Build configuration to extract (default: Debug)")
    parser.add_argument("--compile-flags", action="store_true",
                        help="Also generate compile_flags.txt at workspace root")
    args = parser.parse_args()

    proj_dir = os.path.abspath(args.proj_dir)
    cproject_path = os.path.join(proj_dir, ".cproject")

    if not os.path.isfile(cproject_path):
        print(f"Error: .cproject not found at {cproject_path}", file=sys.stderr)
        return 1

    # --- Parse .cproject ---
    print(f"Parsing {cproject_path} ...")
    cfg = extract_build_config(cproject_path, proj_dir)

    # --- Auto-detect linker script if not found in .cproject ---
    if not cfg.linker_script or not os.path.isfile(cfg.linker_script):
        for f in Path(proj_dir).glob("*_FLASH.ld"):
            cfg.linker_script = str(f)
            print(f"  Auto-detected linker script: {f.name}")
            break

    # --- Discover source files ---
    n_excl = sum(len(se.excludes) for se in cfg.source_entries)
    print(f"Scanning {len(cfg.source_entries)} source entries ({n_excl} exclusion patterns) ...")
    cfg.sources_c, cfg.sources_asm = discover_sources(proj_dir, cfg.source_entries)
    print(f"  Found {len(cfg.sources_c)} C files, {len(cfg.sources_asm)} ASM files")
    if cfg.libraries:
        print(f"  Libraries: {', '.join('-l' + lib for lib in cfg.libraries)}")
        print(f"  Library paths: {', '.join(cfg.library_paths)}")
    if cfg.printf_float:
        print(f"  Printf float: enabled (-u _printf_float)")
    if cfg.scanf_float:
        print(f"  Scanf float: enabled (-u _scanf_float)")

    # --- Generate CMakeLists.txt ---
    out_dir = args.output or proj_dir
    cmake_path = os.path.join(out_dir, "CMakeLists.txt")

    cmake_content = generate_cmakelists(cfg, proj_dir)
    os.makedirs(out_dir, exist_ok=True)
    with open(cmake_path, "w", encoding="utf-8") as f:
        f.write(cmake_content)
    print(f"Generated {cmake_path}")

    # --- Generate compile_flags.txt (optional) ---
    if args.compile_flags:
        workspace_root = os.path.dirname(proj_dir)
        cf_path = os.path.join(workspace_root, "compile_flags.txt")
        cf_content = generate_compile_flags(cfg, proj_dir)
        with open(cf_path, "w", encoding="utf-8") as f:
            f.write(cf_content)
        print(f"Generated {cf_path} ({len(cfg.includes)} includes, {len(cfg.defines)} defines)")

    # --- Summary ---
    print(f"\nBuild commands:")
    print(f"  cmake -B build -DCMAKE_BUILD_TYPE=Debug .")
    print(f"  cmake --build build -j$(nproc)")
    print(f"\nclangd: compile_commands.json will be at build/compile_commands.json")
    print(f"  Symlink or set clangd --compile-commands-dir=build")

    return 0


if __name__ == "__main__":
    sys.exit(main())
