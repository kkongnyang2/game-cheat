
#!/usr/bin/env python3
"""
scan_component_slots.py
-----------------------
Find high-confidence global slot candidates in .data that are referenced
near hashed-dispatch code in .text (FNV-1a hotspots).

Inputs (dump files placed on disk):
  - --text PATH_TO_TEXT_DUMP
  - --data PATH_TO_DATA_DUMP
  - Optional: --text-va, --data-va, --data-size to override auto-detected VA/size
    (If filenames are of the form ..._0x<VASTART>_0x<SIZE>.bin the script extracts VA/SIZE automatically)

Outputs:
  - Prints top .data VA candidates and their module-relative offsets (VA - MODULE_BASE)
  - Writes JSON to <out_json>, default: ./component_slot_candidates.json

Usage Example:
  python3 scan_component_slots.py \
    --text /home/kkongnyang2/abc.exe_.text_0x7ff73cdd1000_0x2000000.bin \
    --data /home/kkongnyang2/abc.exe_.data_0x7ff740282000_0x2ef800.bin \
    --out-json /home/kkongnyang2/component_slot_candidates.json

Notes:
  - We detect FNV-1a 32-bit prime (0x01000193, little-endian) in .text, then scan ±160 bytes
    around each hit for RIP-relative MOV loads (48 8B xx disp32). Targets landing in .data are counted.
  - The most-referenced .data addresses are "global slots" that often hold pointers/handles (possibly encoded)
    to ECS managers/tables, which you can later decode at runtime.
"""

import argparse, os, re, struct, json
from collections import Counter, defaultdict

FNV32_LE = struct.pack("<I", 0x01000193)

def parse_va_size_from_name(path):
    # Expect pattern ..._0x<hexva>_0x<hexsize>.bin
    m = re.search(r"_0x([0-9a-fA-F]+)_0x([0-9a-fA-F]+)\.bin$", os.path.basename(path))
    if not m:
        return None, None
    va   = int(m.group(1), 16)
    size = int(m.group(2), 16)
    return va, size

def map_va_to_off(va, region_base, region_size):
    if va < region_base or va >= region_base + region_size:
        return None
    return va - region_base

def find_all(hay, needle):
    res=[]; i=0
    while True:
        j = hay.find(needle, i)
        if j<0: break
        res.append(j); i=j+1
    return res

def rip_rel_loads_around(text, text_va, pos, radius=160):
    """
    Collect RIP-relative MOV loads within [pos-radius, pos+radius].
    Recognizes encodings: 48 8B 0x{05,0D,15,1D,25,2D,35,3D} <disp32>
    Returns list of (insn_off, target_va, modrm_byte).
    """
    res=[]
    start=max(0, pos-radius); end=min(len(text)-7, pos+radius)
    i=start
    while i<end:
        if i+7 <= len(text):
            if text[i]==0x48 and text[i+1]==0x8B and text[i+2] in (0x05,0x0D,0x15,0x1D,0x25,0x2D,0x35,0x3D):
                disp = struct.unpack_from("<i", text, i+3)[0]
                rip  = text_va + i + 7
                tgt  = (rip + disp) & 0xFFFFFFFFFFFFFFFF
                res.append((i, tgt, text[i+2]))
                i += 7
                continue
        i += 1
    return res

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--text", required=True, help=".text dump path")
    ap.add_argument("--data", required=True, help=".data dump path")
    ap.add_argument("--text-va", type=lambda x:int(x,16), help="optional override VA for .text (hex, e.g. 0x7ff73cdd1000)")
    ap.add_argument("--data-va", type=lambda x:int(x,16), help="optional override VA for .data (hex)")
    ap.add_argument("--data-size", type=lambda x:int(x,16), help="optional override size for .data (hex)")
    ap.add_argument("--module-base", type=lambda x:int(x,16), default=None,
                    help="Optional module base VA (hex). If omitted, inferred as nearest 64KB-down from text_va.")
    ap.add_argument("--out-json", default="component_slot_candidates.json", help="output JSON path")
    ap.add_argument("--limit-fnv-sites", type=int, default=2000, help="cap scanning around first N FNV hits for speed")
    args = ap.parse_args()

    with open(args.text, "rb") as f: text = f.read()
    with open(args.data, "rb") as f: data = f.read()

    text_va, _ = parse_va_size_from_name(args.text)
    data_va, data_size = parse_va_size_from_name(args.data)

    if args.text_va is not None:  text_va  = args.text_va
    if args.data_va is not None:  data_va  = args.data_va
    if args.data_size is not None: data_size = args.data_size

    if text_va is None:
        raise SystemExit("[-] Could not infer .text VA from filename. Provide --text-va.")
    if data_va is None or data_size is None:
        raise SystemExit("[-] Could not infer .data VA/size from filename. Provide --data-va and --data-size.")

    if args.module_base is not None:
        mod_base = args.module_base
    else:
        # Heuristic: module base is 64KB-align down from text_va - 0x1000
        mod_base = (text_va - 0x1000) & ~0xFFFF

    print(f"[i] .text VA = {hex(text_va)}, size = {hex(len(text))}")
    print(f"[i] .data VA = {hex(data_va)}, size = {hex(data_size)}")
    print(f"[i] module base (heuristic) = {hex(mod_base)}")

    fnv_sites = find_all(text, FNV32_LE)
    print(f"[i] FNV-1a(32) prime hits = {len(fnv_sites)} (scanning around first {min(len(fnv_sites), args.limit_fnv_sites)})")

    from collections import Counter, defaultdict
    counter = Counter()
    details = defaultdict(list)

    for off in fnv_sites[:args.limit_fnv_sites]:
        for insn_off, tgt_va, modrm in rip_rel_loads_around(text, text_va, off, radius=160):
            # Keep only targets that land inside .data
            if data_va <= tgt_va < data_va + data_size:
                counter[tgt_va] += 1
                details[tgt_va].append(text_va + insn_off)

    top = counter.most_common(16)
    print("[i] Top .data targets near hashed-dispatch sites:")
    results = []
    for idx, (addr, cnt) in enumerate(top, 1):
        off_in_mod = addr - mod_base
        refs = details[addr][:6]
        print(f"  {idx:2d}) slot_va {hex(addr)}  off {hex(off_in_mod)}  refs={cnt}")
        for r in refs:
            print(f"       ref @ .text {hex(r)}")
        results.append({
            "slot_va": addr,
            "module_off": off_in_mod,
            "ref_count": cnt,
            "sample_refs": refs
        })

    out = {
        "text_va": text_va,
        "data_va": data_va,
        "data_size": data_size,
        "module_base": mod_base,
        "candidates": results
    }
    with open(args.out_json, "w") as f:
        json.dump(out, f, indent=2)
    print(f"[+] Wrote candidates JSON -> {args.out_json}")

if __name__ == "__main__":
    main()
