import sys
import re
import os
import io

# Windows 콘솔 cp949 인코딩 문제 방지 — UTF-8 강제
if sys.stdout.encoding != 'utf-8':
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

def parse_hex(val_str):
    try:
        return int(val_str, 16)
    except:
        return 0

def format_size(size_bytes):
    """바이트를 KB 단위로 포맷 (항상 KB — 가독성 우선)"""
    if size_bytes == 0:
        return f"{'0':>7s} KB"
    val = size_bytes / 1024
    if val == int(val):
        return f"{int(val):>7d} KB"
    else:
        return f"{val:>7.2f} KB"

def make_bar(pct, width=25):
    """사용률 기반 프로그레스 바 생성 (25칸)"""
    filled = int(round(pct / 100.0 * width))
    filled = min(filled, width)
    empty = width - filled

    # 사용률 구간별 문자
    if pct >= 95:
        fill_char = '█'
        tag = ' !!FULL'
    elif pct >= 80:
        fill_char = '█'
        tag = ' !WARN'
    elif pct >= 60:
        fill_char = '▓'
        tag = ''
    elif pct > 0:
        fill_char = '▓'
        tag = ''
    else:
        fill_char = '▓'
        tag = ''

    bar = fill_char * filled + '░' * empty
    return f"[{bar}]{tag}"

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

        # ── 1. Memory Configuration 파싱 ─────────────────────────
        mem_block = re.search(
            r'Memory Configuration\s+Name\s+Origin\s+Length\s+Attributes\s+(.*?)\n\n',
            content, re.DOTALL)
        if not mem_block:
            print("[Error] Memory Configuration block not found.")
            return

        regions = {}
        region_order = []
        for line in mem_block.group(1).strip().split('\n'):
            parts = re.split(r'\s+', line.strip())
            if len(parts) >= 3:
                name = parts[0]
                if name == "*default*":
                    continue
                origin = parse_hex(parts[1])
                length = parse_hex(parts[2])
                regions[name] = {'origin': origin, 'length': length, 'used': 0}
                region_order.append(name)

        # ── 2. Output Section 파싱 (최상위 섹션만) ──────────────
        #
        # GCC linker map 형식:
        #   Output Section (열 0 시작):
        #     .text           0x08000000   0x2f494
        #     ._user_heap_stack\n                0x24030a48   0x4000
        #   Input Section (공백 들여쓰기):
        #      .text.main     0x08001234   0x100  main.c.obj
        #
        # Output Section만 파싱하면 이중 계산을 방지합니다.
        # LMA(load address)가 있으면 FLASH 사용량에도 추가합니다.

        start_idx = content.find("Linker script and memory map")
        if start_idx < 0:
            print("[Error] 'Linker script and memory map' not found.")
            return
        map_text = content[start_idx:]

        # ELF 메타데이터 섹션 (MCU에 로드되지 않음)
        NON_LOADABLE = {'.ARM.attributes', '.comment', '.stab', '.stabstr'}

        # NOLOAD 섹션: VMA는 RAM에 잡히지만 FLASH에 LMA 복사본 없음
        # .bss는 NOLOAD이므로 'load address'가 표시되어도 실제 FLASH 사용 0
        NOLOAD_SECTIONS = {'.bss', '.tbss',
                           '.RAM_D2_data', '.RAM_D3_data',
                           '.lwip_sec', '.PSRAM_data'}

        # Pattern A: 한 줄에 이름+주소+크기 (+ 선택적 LMA)
        #   .text           0x080002a0    0x2f494
        #   .data           0x24000000    0x258 load address 0x08030438
        pat_single = re.compile(
            r'^(\.[a-zA-Z_]\S*)\s+'
            r'(0x[0-9a-fA-F]+)\s+'
            r'(0x[0-9a-fA-F]+)'
            r'(?:\s+load address\s+(0x[0-9a-fA-F]+))?',
            re.MULTILINE)

        # Pattern B: 이름이 길어서 두 줄에 걸침
        #   ._user_heap_stack
        #                   0x24030a48    0x4000
        pat_twoline = re.compile(
            r'^(\.[a-zA-Z_]\S*)\s*\n'
            r'\s+(0x[0-9a-fA-F]+)\s+'
            r'(0x[0-9a-fA-F]+)',
            re.MULTILINE)

        output_sections = []  # (name, vma, size, lma_or_None)

        for m in pat_single.finditer(map_text):
            name = m.group(1)
            vma = parse_hex(m.group(2))
            size = parse_hex(m.group(3))
            lma = parse_hex(m.group(4)) if m.group(4) else None
            output_sections.append((name, vma, size, lma))

        for m in pat_twoline.finditer(map_text):
            name = m.group(1)
            vma = parse_hex(m.group(2))
            size = parse_hex(m.group(3))
            output_sections.append((name, vma, size, None))

        # 중복 제거 (two-line이 single-line과 겹칠 수 있음): (name, vma) 기준
        seen = set()
        unique_sections = []
        for sec in output_sections:
            key = (sec[0], sec[1])
            if key not in seen:
                seen.add(key)
                unique_sections.append(sec)

        def find_region(addr):
            """주소가 속하는 메모리 영역 이름 반환"""
            for rname in region_order:
                info = regions[rname]
                if info['origin'] <= addr < (info['origin'] + info['length']):
                    return rname
            return None

        for sec_name, vma, size, lma in unique_sections:
            if size == 0:
                continue
            # 디버그/메타데이터 섹션 필터링
            if sec_name.startswith('.debug') or sec_name in NON_LOADABLE:
                continue

            # VMA 기준으로 RAM 사용량 합산
            rgn = find_region(vma)
            if rgn:
                regions[rgn]['used'] += size

            # LMA가 있으면 초기화 데이터의 FLASH 복사본 크기 추가
            # (VMA는 RAM, LMA는 FLASH를 가리킴)
            # 단, NOLOAD 섹션(.bss 등)은 제외 — 실제 FLASH 복사본 없음
            if lma is not None and sec_name not in NOLOAD_SECTIONS:
                flash_rgn = find_region(lma)
                if flash_rgn and flash_rgn != rgn:
                    regions[flash_rgn]['used'] += size

        # ── 3. 결과 출력 ────────────────────────────────────────
        LINE_W = 100
        print()
        print(" MCU FINAL MEMORY REPORT ".center(LINE_W, "="))
        print(f" Target: {os.path.basename(elf_path)}")
        print("-" * LINE_W)
        print(f" {'REGION':<12} | {'TOTAL':>10} | {'USED':>10} | {'USAGE':>8} | {'FREE':>10} | MEMORY MAP")
        print("-" * LINE_W)

        warn_regions = []

        for name in region_order:
            info = regions[name]
            used = info['used']
            total = info['length']
            free_bytes = total - used
            pct = (used / total) * 100 if total > 0 else 0

            total_str = format_size(total)
            used_str = format_size(used)
            free_str = format_size(free_bytes)
            bar = make_bar(pct)

            print(f" {name:<12} | {total_str} | {used_str} | {pct:>6.2f} % | {free_str} | {bar}")

            # 경고/위험 영역 수집
            if pct >= 95:
                warn_regions.append((name, pct, 'CRITICAL'))
            elif pct >= 80:
                warn_regions.append((name, pct, 'WARNING'))

        print("=" * LINE_W)

        # ── 4. 경고 메시지 ────────────────────────────────────
        if warn_regions:
            print()
            for rname, rpct, severity in warn_regions:
                if severity == 'CRITICAL':
                    print(f" [!!] {rname}: {rpct:.1f}% USED — CRITICAL! Overflow risk.")
                else:
                    print(f" [**] {rname}: {rpct:.1f}% USED — Consider optimization.")
            print()
        else:
            print()

    except Exception as e:
        print(f"[Error] {e}")

if __name__ == "__main__":
    main()
