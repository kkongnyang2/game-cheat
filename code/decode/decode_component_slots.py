
#!/usr/bin/env python3
"""
decode_component_slots.py  (v2)
-------------------------------
Given candidate .data slots, read QWORDs and attempt to decode them into plausible pointers.

New in v2:
- Tries multiple decode modes:
    1) xor                 : dec = enc ^ key
    2) ror_then_xor:<r>    : dec = ROR64(enc, r) ^ key
    3) xor_then_ror:<r>    : dec = ROR64(enc ^ key, r)
- Canonicalization step (on by default): mask to 48-bit and/or sign-extend
  so that top bits don't disqualify valid user-space pointers.
- More verbose output if --verbose is set.

Inputs:
  - --data PATH_TO_DATA_DUMP
  - Either:
      (A) --json CANDIDATE_JSON  (produced by scan_component_slots.py)
          (optionally restrict with --top N)
      or
      (B) --slot-offs HEX_OFFS... + --module-base HEX
  - --key XOR_KEY (hex), default: 0xEE222BB7EF934A1D
  - --rots ROTATIONS (ints), default: 8 13 16 24 31
  - --steps 0 8 16 24 (within-slot QWORD steps)
  - --mask48 / --no-mask48  (default: mask48 on)
  - --out-json decoded_component_bases.json

Usage Example (JSON from scanner):
  python3 decode_component_slots.py \
    --data /home/kkongnyang2/abc.exe_.data_0x7ff740282000_0x2ef800.bin \
    --json /home/kkongnyang2/component_slot_candidates.json \
    --top 6 \
    --verbose
"""

import argparse, os, re, struct, json

def parse_va_size_from_name(path):
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

def read_qword_from_data(data_bytes, data_va, data_size, target_va):
    off = map_va_to_off(target_va, data_va, data_size)
    if off is None or off+8 > len(data_bytes):
        raise ValueError(f"Target VA {hex(target_va)} not inside .data")
    return struct.unpack_from("<Q", data_bytes, off)[0]

def ror64(x, r):
    r %= 64
    return ((x >> r) | ((x & ((1<<r)-1)) << (64-r))) & ((1<<64)-1)

def canonicalize_48b(ptr, do_mask=True, sign_extend=False):
    """Return a 64-bit canonical pointer from a possibly non-canonical 64-bit value.
       - do_mask: keep only low 48 bits
       - sign_extend: propagate bit 47 into high bits (rare for user-mode; off by default)"""
    if do_mask:
        ptr48 = ptr & ((1<<48)-1)
        if sign_extend:
            if (ptr48 >> 47) & 1:
                ptr64 = ptr48 | (~((1<<48)-1) & ((1<<64)-1))
            else:
                ptr64 = ptr48
            return ptr64
        else:
            return ptr48
    return ptr

def looks_like_ptr(p, lo, hi):
    return lo <= p <= hi

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", required=True, help=".data dump path")
    ap.add_argument("--json", help="candidates JSON produced by scanner")
    ap.add_argument("--top", type=int, default=12, help="use only first N candidates from JSON")
    ap.add_argument("--slot-offs", nargs="*", type=lambda x:int(x,16), help="module-relative offsets (hex)")
    ap.add_argument("--module-base", type=lambda x:int(x,16), help="module base VA (hex)")
    ap.add_argument("--key", type=lambda x:int(x,16), default=0xEE222BB7EF934A1D, help="XOR key (hex)")
    ap.add_argument("--steps", nargs="*", type=int, default=[0,8,0x10,0x18], help="intra-slot QWORD steps to probe")
    ap.add_argument("--rots", nargs="*", type=int, default=[8,13,16,24,31], help="rotations to try")
    ap.add_argument("--ptr-min", type=lambda x:int(x,16), default=0x0000000000100000, help="pointer min VA (hex)")
    ap.add_argument("--ptr-max", type=lambda x:int(x,16), default=0x00007FFFFFFFFFFF, help="pointer max VA (hex)")
    ap.add_argument("--mask48", dest="mask48", action="store_true", default=True, help="mask to 48-bit (default ON)")
    ap.add_argument("--no-mask48", dest="mask48", action="store_false", help="disable 48-bit masking")
    ap.add_argument("--sign-extend", action="store_true", help="sign-extend bit47 after masking (off by default)")
    ap.add_argument("--verbose", action="store_true", help="print more details")
    ap.add_argument("--out-json", default="decoded_component_bases.json", help="output JSON path")
    args = ap.parse_args()

    with open(args.data, "rb") as f: data_bytes = f.read()
    data_va, data_size = parse_va_size_from_name(args.data)
    if data_va is None or data_size is None:
        raise SystemExit("[-] Could not infer .data VA/size from filename; provide a correctly named file.")

    # Determine slot list
    slot_vas = []

    if args.json:
        with open(args.json, "r") as f:
            cand = json.load(f)
        module_base = cand.get("module_base")
        if args.module_base is not None:
            module_base = args.module_base
        if module_base is None:
            raise SystemExit("[-] module_base missing; pass --module-base or in JSON.")
        for item in cand.get("candidates", [])[:args.top]:
            off = item["module_off"]
            slot_vas.append(module_base + off)
    else:
        if not args.slot_offs or args.module_base is None:
            raise SystemExit("[-] Provide either --json (and optionally --top) OR --slot-offs plus --module-base.")
        module_base = args.module_base
        for off in args.slot_offs:
            slot_vas.append(module_base + off)

    print(f"[i] Using module base = {hex(module_base)}")
    print(f"[i] .data VA = {hex(data_va)}, size = {hex(data_size)}")
    print(f"[i] XOR key = {hex(args.key)}")
    print(f"[i] Steps = {args.steps}, Rotations = {args.rots}")
    print(f"[i] Canonicalization: mask48={'ON' if args.mask48 else 'OFF'}, sign_extend={'ON' if args.sign_extend else 'OFF'}")

    decoded = []
    def try_record(mode, slot_va, step, enc, dec_raw):
        # Canonicalize
        dec_canon = canonicalize_48b(dec_raw, do_mask=args.mask48, sign_extend=args.sign_extend)
        ok = looks_like_ptr(dec_canon, args.ptr_min, args.ptr_max)
        if ok:
            decoded.append({
                "slot_va": slot_va,
                "step": step,
                "mode": mode,
                "encoded_qword": enc,
                "decoded_raw": dec_raw,
                "decoded_canonical": dec_canon
            })
            print(f"[+] {mode:>14s} | slot {hex(slot_va)} +{step:#x} -> enc {hex(enc)}  raw {hex(dec_raw)}  canon {hex(dec_canon)}  (OK)")
        elif args.verbose:
            print(f"[ ] {mode:>14s} | slot {hex(slot_va)} +{step:#x} -> enc {hex(enc)}  raw {hex(dec_raw)} (not in range)")

    for slot_va in slot_vas:
        for step in args.steps:
            tgt = slot_va + step
            try:
                enc = read_qword_from_data(data_bytes, data_va, data_size, tgt)
            except Exception as e:
                if args.verbose:
                    print(f"[!] read fail at {hex(tgt)}: {e}")
                continue

            # Mode 1: XOR only
            try_record("xor", slot_va, step, enc, enc ^ args.key)

            # Mode 2: ROR(enc,r) ^ key
            for r in args.rots:
                try_record(f"ror_then_xor:{r}", slot_va, step, enc, ror64(enc, r) ^ args.key)

            # Mode 3: ROR(enc ^ key, r)
            x = enc ^ args.key
            for r in args.rots:
                try_record(f"xor_then_ror:{r}", slot_va, step, enc, ror64(x, r))

    out = {
        "module_base": module_base,
        "data_va": data_va,
        "data_size": data_size,
        "key": args.key,
        "results": decoded
    }
    with open(args.out_json, "w") as f:
        json.dump(out, f, indent=2)
    print(f"[+] Wrote decoded candidates -> {args.out_json}")
    if not decoded:
        print("[!] No plausible decoded pointers found. Try different --key/--rots or disable masking to inspect raw.")
        print("[!] Tip: use --verbose to see all attempts.")
if __name__ == "__main__":
    main()