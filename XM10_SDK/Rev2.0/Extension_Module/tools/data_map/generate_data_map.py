#!/usr/bin/env python3
"""
XM10 Total Data Packet — Code Generator

YAML SSOT → C header + TypeScript type definitions.

Usage:
    python generate_data_map.py xm_total_data.yaml \
        --c-out ../../Extension_Module/XM_FW/System/Comm/USB/xm_total_data_packet.h \
        --ts-out generated/xm_total_data_map.ts

Dependencies:
    pip install pyyaml
"""
import sys
import os
import argparse
from datetime import datetime

try:
    import yaml
except ImportError:
    print("ERROR: pyyaml not installed. Run: pip install pyyaml", file=sys.stderr)
    sys.exit(1)


# =============================================================================
# Type system
# =============================================================================
TYPE_INFO = {
    "uint8":   {"c": "uint8_t",  "ts": "'uint8'",   "size": 1},
    "int8":    {"c": "int8_t",   "ts": "'int8'",    "size": 1},
    "uint16":  {"c": "uint16_t", "ts": "'uint16'",  "size": 2},
    "int16":   {"c": "int16_t",  "ts": "'int16'",   "size": 2},
    "uint32":  {"c": "uint32_t", "ts": "'uint32'",  "size": 4},
    "int32":   {"c": "int32_t",  "ts": "'int32'",   "size": 4},
    "float32": {"c": "float",    "ts": "'float32'", "size": 4},
}


def load_yaml(path):
    with open(path, "r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def get_type_size(type_name, custom_types):
    if type_name in TYPE_INFO:
        return TYPE_INFO[type_name]["size"]
    if type_name in custom_types:
        return custom_types[type_name]["size"]
    raise ValueError(f"Unknown type: {type_name}")


def get_c_type(type_name, custom_types):
    if type_name in TYPE_INFO:
        return TYPE_INFO[type_name]["c"]
    return None  # Custom type handled separately


def calc_field_size(field, custom_types):
    """Calculate total byte size of a field (including count)."""
    count = field.get("count", 1)
    type_name = field.get("type", "")
    return get_type_size(type_name, custom_types) * count


def calc_group_size(group, custom_types):
    """Calculate total byte size of a group."""
    if group.get("reserved"):
        return group["size"]
    total = 0
    for field in group.get("fields", []):
        total += calc_field_size(field, custom_types)
    return total


# =============================================================================
# Flatten fields for TypeScript (expand complex types and arrays)
# =============================================================================
def flatten_fields(groups, custom_types):
    """Flatten all fields into a list of (offset, name, type, scale, scaleFormula, unit, group, count)."""
    result = []
    offset = 0

    for group in groups:
        group_name = group["name"]

        if group.get("reserved"):
            offset += group["size"]
            continue

        group_formula = group.get("scale_formula", "none")

        for field in group.get("fields", []):
            fname = field.get("name", "")
            ftype = field.get("type", "")
            fcount = field.get("count", 1)
            fscale = field.get("scale", 1)
            funit = field.get("unit", "")
            fdesc = field.get("desc", "")

            # Check if it's a custom type (struct of struct)
            if ftype in custom_types:
                ct = custom_types[ftype]
                ct_size = ct["size"]
                for i in range(fcount):
                    for sf in ct["fields"]:
                        sf_name = sf["name"]
                        sf_type = sf["type"]
                        sf_count = sf.get("count", 1)
                        sf_scale = sf.get("scale", 1)
                        sf_unit = sf.get("unit", "")
                        sf_formula = group_formula if sf_scale != 1 else "none"
                        # Use group's scale_formula for sub-fields with scale
                        if sf_scale != 1:
                            sf_formula = group_formula

                        full_name = f"{fname}[{i}].{sf_name}"
                        result.append({
                            "offset": offset,
                            "name": full_name,
                            "type": sf_type,
                            "scale": sf_scale,
                            "scaleFormula": sf_formula,
                            "unit": sf_unit,
                            "group": group_name,
                            "count": sf_count,
                        })
                        offset += TYPE_INFO[sf_type]["size"] * sf_count
            else:
                formula = "none"
                if fscale != 1:
                    formula = group_formula

                result.append({
                    "offset": offset,
                    "name": fname,
                    "type": ftype,
                    "scale": fscale,
                    "scaleFormula": formula,
                    "unit": funit,
                    "group": group_name,
                    "count": fcount,
                })
                offset += TYPE_INFO[ftype]["size"] * fcount

    return result, offset


# =============================================================================
# C Header Generator
# =============================================================================
def generate_c_header(data, custom_types, total_size):
    packet_name = data["packet_name"]
    module_id = data["module_id"]
    version = data["version"]
    groups = data["groups"]

    lines = []
    lines.append(f"/* AUTO-GENERATED from xm_total_data.yaml v{version} — DO NOT EDIT MANUALLY */")
    lines.append(f"/* Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} */")
    lines.append("")
    lines.append(f"#ifndef XM_TOTAL_DATA_PACKET_H")
    lines.append(f"#define XM_TOTAL_DATA_PACKET_H")
    lines.append("")
    lines.append("#include <stdint.h>")
    lines.append("#include <stdbool.h>")
    lines.append("")

    # Custom types first
    for tname, tdef in custom_types.items():
        c_tname = f"{tname}"
        lines.append(f"/* Complex type: {tdef.get('desc', '')} */")
        lines.append(f"typedef struct __attribute__((packed)) {{")
        for sf in tdef["fields"]:
            c_type = TYPE_INFO[sf["type"]]["c"]
            count = sf.get("count", 1)
            arr = f"[{count}]" if count > 1 else ""
            scale_comment = f", scale: {sf['scale']}" if sf.get("scale", 1) != 1 else ""
            unit_comment = f", unit: {sf.get('unit', '')}" if sf.get("unit") else ""
            lines.append(f"    {c_type:10s} {sf['name']}{arr};{' ' * max(1, 20 - len(sf['name']) - len(arr))}/* {sf.get('desc', '')}{scale_comment}{unit_comment} */")
        lines.append(f"}} {c_tname};")
        lines.append("")

    # Main packet struct
    lines.append("#pragma pack(push, 1)")
    lines.append(f"typedef struct {{")

    offset = 0
    channel_count = 0

    for group in groups:
        gname = group["name"]
        gsize = calc_group_size(group, custom_types)
        lines.append(f"    /* === {gname} ({gsize}B) === */")

        if group.get("reserved"):
            lines.append(f"    uint8_t  rsv_{gname.lower().replace('reserved_', '')}[{gsize}];{' ' * max(1, 10)}/* offset: {offset}, {group.get('desc', '')} */")
            offset += gsize
            lines.append("")
            continue

        for field in group.get("fields", []):
            fname = field.get("name", "")
            ftype = field.get("type", "")
            fcount = field.get("count", 1)
            fscale = field.get("scale", 1)
            funit = field.get("unit", "")

            if ftype in custom_types:
                # Custom type field
                ct = custom_types[ftype]
                arr = f"[{fcount}]" if fcount > 1 else ""
                lines.append(f"    {ftype:22s} {fname}{arr};{' ' * max(1, 15 - len(fname) - len(arr))}/* offset: {offset} */")
                field_size = ct["size"] * fcount
                offset += field_size
                # Count sub-channels
                for _ in range(fcount):
                    for sf in ct["fields"]:
                        channel_count += sf.get("count", 1)
            else:
                c_type = TYPE_INFO[ftype]["c"]
                arr = f"[{fcount}]" if fcount > 1 else ""
                scale_str = f", scale: {fscale}" if fscale != 1 else ""
                unit_str = f", unit: {funit}" if funit else ""

                c_name = fname
                lines.append(f"    {c_type:10s} {c_name}{arr};{' ' * max(1, 25 - len(c_name) - len(arr))}/* offset: {offset}{scale_str}{unit_str} */")
                field_size = TYPE_INFO[ftype]["size"] * fcount
                offset += field_size
                channel_count += fcount

        lines.append("")

    lines.append(f"}} {packet_name}_t;")
    lines.append("#pragma pack(pop)")
    lines.append("")

    # Static assert
    lines.append(f"_Static_assert(sizeof({packet_name}_t) == {total_size},")
    lines.append(f'    "{packet_name}_t size mismatch — update YAML and regenerate");')
    lines.append("")

    # Defines
    lines.append(f"#define XM_TOTAL_DATA_PAYLOAD_SIZE  sizeof({packet_name}_t)")
    mid_hex = f"0x{module_id:02X}" if isinstance(module_id, int) else str(module_id)
    lines.append(f"#define XM_TOTAL_DATA_MODULE_ID     {mid_hex}")
    lines.append(f"#define XM_TOTAL_DATA_NUM_CHANNELS  {channel_count}   /* excluding reserved */")
    lines.append("")
    lines.append(f"#endif /* XM_TOTAL_DATA_PACKET_H */")
    lines.append("")

    return "\n".join(lines)


# =============================================================================
# TypeScript Generator
# =============================================================================
def generate_ts(data, flat_fields, total_size):
    version = data["version"]
    module_id = data["module_id"]

    lines = []
    lines.append(f"/* AUTO-GENERATED from xm_total_data.yaml v{version} — DO NOT EDIT MANUALLY */")
    lines.append(f"/* Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')} */")
    lines.append("")
    lines.append("export interface ChannelDef {")
    lines.append("  offset: number;")
    lines.append("  type: 'uint8' | 'int8' | 'uint16' | 'int16' | 'uint32' | 'int32' | 'float32';")
    lines.append("  scale: number;")
    lines.append("  scaleFormula: 'none' | 'divide' | 'multiply_divide';")
    lines.append("  unit: string;")
    lines.append("  name: string;")
    lines.append("  group: string;")
    lines.append("  count?: number;  // array size (default 1)")
    lines.append("}")
    lines.append("")

    # Type size map
    lines.append("export const TYPE_SIZE: Record<string, number> = {")
    for tname, tinfo in TYPE_INFO.items():
        lines.append(f"  '{tname}': {tinfo['size']},")
    lines.append("};")
    lines.append("")

    lines.append("export const TOTAL_DATA_MAP: ChannelDef[] = [")

    for f in flat_fields:
        count_str = ""
        if f["count"] > 1:
            count_str = f", count: {f['count']}"

        scale_val = f["scale"]
        if isinstance(scale_val, float):
            scale_str = f"{scale_val}"
        else:
            scale_str = str(scale_val)

        lines.append(
            f"  {{ offset: {f['offset']:3d}, type: {TYPE_INFO[f['type']]['ts']:10s}, "
            f"scale: {scale_str:>8s}, scaleFormula: '{f['scaleFormula']}', "
            f"unit: '{f['unit']}', name: '{f['name']}', group: '{f['group']}'"
            f"{count_str} }},"
        )

    lines.append("];")
    lines.append("")
    lines.append(f"export const TOTAL_PACKET_SIZE = {total_size};")
    lines.append(f"export const DATA_MAP_VERSION = '{version}';")
    mid_hex = f"0x{module_id:02X}" if isinstance(module_id, int) else str(module_id)
    lines.append(f"export const MODULE_ID_TOTAL = {mid_hex};")
    lines.append(f"export const MODULE_ID_USER_META = 0xEF;")
    lines.append(f"export const MODULE_ID_USER_CUSTOM_START = 0xF0;")
    lines.append(f"export const MODULE_ID_USER_CUSTOM_END = 0xFE;")
    lines.append("")

    return "\n".join(lines)


# =============================================================================
# Main
# =============================================================================
def main():
    parser = argparse.ArgumentParser(description="Generate C header and TS types from YAML Data Map")
    parser.add_argument("yaml_file", help="Input YAML file (SSOT)")
    parser.add_argument("--c-out", help="Output C header path")
    parser.add_argument("--ts-out", help="Output TypeScript path")
    parser.add_argument("--dry-run", action="store_true", help="Print to stdout, don't write files")
    args = parser.parse_args()

    data = load_yaml(args.yaml_file)
    groups = data.get("groups", [])
    custom_types = data.get("types", {})

    # Calculate total size
    total_size = 0
    for g in groups:
        total_size += calc_group_size(g, custom_types)

    print(f"[INFO] Packet: {data['packet_name']}, Total: {total_size}B, Module ID: {data['module_id']}")

    # Flatten for TS
    flat_fields, flat_size = flatten_fields(groups, custom_types)
    assert flat_size == total_size, f"Flatten size mismatch: {flat_size} != {total_size}"

    # Generate C header
    c_content = generate_c_header(data, custom_types, total_size)
    if args.dry_run or not args.c_out:
        if args.dry_run:
            print("=== C HEADER ===")
            print(c_content)
    else:
        os.makedirs(os.path.dirname(os.path.abspath(args.c_out)), exist_ok=True)
        with open(args.c_out, "w", encoding="utf-8") as f:
            f.write(c_content)
        print(f"[OK] C header: {args.c_out}")

    # Generate TypeScript
    ts_content = generate_ts(data, flat_fields, total_size)
    if args.dry_run or not args.ts_out:
        if args.dry_run:
            print("\n=== TYPESCRIPT ===")
            print(ts_content)
    else:
        os.makedirs(os.path.dirname(os.path.abspath(args.ts_out)), exist_ok=True)
        with open(args.ts_out, "w", encoding="utf-8") as f:
            f.write(ts_content)
        print(f"[OK] TypeScript: {args.ts_out}")

    print(f"[INFO] Channels: {len(flat_fields)} entries (flat), {total_size}B total")


if __name__ == "__main__":
    main()
