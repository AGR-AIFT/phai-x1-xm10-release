import sys
import re
import os

def parse_val(val_str):
    try:
        return int(val_str, 16) if val_str.startswith('0x') else int(val_str)
    except:
        return 0

def main():
    if len(sys.argv) < 2 or not sys.argv[1]:
        return

    elf_path = sys.argv[1]
    map_path = os.path.splitext(elf_path)[0] + ".map"

    if not os.path.exists(map_path):
        print(f"[Error] Map file not found: {map_path}")
        return

    try:
        with open(map_path, 'r', encoding='utf-8', errors='ignore') as f:
            content = f.read()

        # 1. 'Memory Configuration' 섹션의 영역 이름들만 정확히 추출
        mem_block = re.search(r'Memory Configuration\s+Name\s+Origin\s+Length\s+Attributes\s+(.*?)\n\n', content, re.DOTALL)
        if not mem_block:
            print("[Error] Memory Configuration block not found.")
            return

        regions = {}
        target_names = [] # 실제 공식 영역 이름들 저장
        for line in mem_block.group(1).strip().split('\n'):
            parts = re.split(r'\s+', line.strip())
            if len(parts) >= 3:
                name = parts[0]
                if name == "*default*": continue
                origin = parse_val(parts[1])
                length = parse_val(parts[2])
                regions[name] = {'origin': origin, 'length': length, 'used': 0}
                target_names.append(name)

        # 2. 섹션 할당량 파싱 (공식 영역에 해당하는 것만 합산)
        # Linker script and memory map 이후의 섹션 정보를 읽음
        start_idx = content.find("Linker script and memory map")
        section_pattern = r'^[ ](\.[a-zA-Z0-9_\.]+)\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)'
        sections = re.findall(section_pattern, content[start_idx:], re.MULTILINE)

        for sec_name, addr_str, size_str in sections:
            addr = parse_val(addr_str)
            size = parse_val(size_str)
            if size == 0 or sec_name.startswith('.debug'): 
                continue

            for name in target_names:
                info = regions[name]
                if info['origin'] <= addr < (info['origin'] + info['length']):
                    info['used'] += size
                    break

        # 3. 결과 출력 (깔끔한 요약 테이블)
        print("\n" + " MCU FINAL MEMORY REPORT ".center(70, "="))
        print(f" Target: {os.path.basename(elf_path)}")
        print("-" * 70)
        print(f" {'REGION NAME':<18} | {'USED (KB)':>12} | {'TOTAL (KB)':>12} | {'USAGE (%)':>10}")
        print("-" * 70)

        for name in sorted(target_names):
            info = regions[name]
            used_kb = info['used'] / 1024
            total_kb = info['length'] / 1024
            pct = (info['used'] / info['length']) * 100 if info['length'] > 0 else 0
            print(f" {name:<18} | {used_kb:>9.2f} KB | {total_kb:>9.0f} KB | {pct:>8.2f} %")

        print("=" * 70 + "\n")

    except Exception as e:
        print(f"[Error] {e}")

if __name__ == "__main__":
    main()