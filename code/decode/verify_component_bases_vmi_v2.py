#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
verify_component_bases_vmi_v2.py
--------------------------------
Robust LibVMI-based verifier that recomputes multiple decode formulas on the fly
and tests each candidate until one passes the vtable check.

Why this exists
---------------
If XOR-only decoding yields non-present addresses (VMI_FAILURE), the encoding
may additionally depend on slot address or module base, or require rotation
ordering differences. This tool tries a richer search space automatically.

Inputs
------
- --decoded-json  decoded_component_bases.json (from decode_component_slots.py v2)
- --domain/--pid/--module/--ist  (LibVMI + helpers)
- --text-va/--text-size and --rdata-va/--rdata-size (optional; else auto from PE)
- --key  (default 0xEE222BB7EF934A1D)
- --rots "8 13 16 24 31" (default)
- --try-add-base/--try-add-slot  (toggle trying +/- module_base or slot_va)
- --try-shifts "0 4" (try >>n before/after XOR; default 0 and 4)
- --max-candidates N (default 24)
- --modes  (subset of base decode ops to consider; default: xor ror_then_xor:8,13,16,24,31 xor_then_ror:8,13,16,24,31)
- Outputs verified_component_bases.json with the first passing variant for each item.

PASS criteria
-------------
[base] ∈ .rdata  AND  [vtable+0|+8] ∈ .text

"""
import argparse, json, sys, struct, itertools
from typing import Dict, Tuple, List
from libvmi import Libvmi
from find_module_base import find_module_base
from vmi_helpers import read_ptr, wait_present
from ist_helpers import IST

def parse_sections(vmi, pid, base) -> Dict[str, Tuple[int,int,int,int]]:
    mz_buf, _ = vmi.read_va(base, pid, 2)
    mz = int.from_bytes(bytes(mz_buf), "little")
    if (mz & 0xFFFF) != 0x5A4D: raise RuntimeError("MZ not found")
    e_lfanew = int.from_bytes(bytes(vmi.read_va(base+0x3C, pid, 4)[0]), "little")
    nt = base + e_lfanew
    sig = int.from_bytes(bytes(vmi.read_va(nt, pid, 4)[0]), "little")
    if sig != 0x00004550: raise RuntimeError("PE signature not found")
    filehdr = nt + 4
    nsec    = int.from_bytes(bytes(vmi.read_va(filehdr + 2, pid, 2)[0]), "little")
    optsz   = int.from_bytes(bytes(vmi.read_va(filehdr + 16, pid, 2)[0]), "little")
    shdr    = filehdr + 20 + optsz
    out = {}
    for i in range(nsec):
        buf, n = vmi.read_va(shdr + i*40, pid, 40)
        if n != 40: raise RuntimeError("read_va failed at section header")
        hdr = bytes(buf)
        name    = hdr[0:8].rstrip(b"\x00").decode(errors="ignore") or f"sec_{i}"
        vsize   = struct.unpack_from("<I", hdr, 8)[0]
        rva     = struct.unpack_from("<I", hdr, 12)[0]
        rawsize = struct.unpack_from("<I", hdr, 16)[0]
        va_start= base + rva
        out[name] = (rva, vsize, rawsize, va_start)
    return out

def ror64(x,r): r%=64; return ((x>>r)|((x&((1<<r)-1))<<(64-r))) & ((1<<64)-1)
MASK48 = (1<<48)-1

def canon48(x, sign_extend=False):
    x = x & MASK48
    if sign_extend and ((x>>47)&1):  # usually false for user-mode
        x |= (~MASK48) & ((1<<64)-1)
    return x

def in_range(x, start, size): return start <= x < start + size
def uniq_by(items, key):
    seen=set(); out=[]
    for it in items:
        k=key(it)
        if k in seen: continue
        seen.add(k); out.append(it)
    return out

def make_decode_variants(enc, key, rots, slot_va, module_base, try_add_base, try_add_slot, try_shifts):
    """Yield (mode_str, base_candidate) tuples."""
    def variants_from_raw(tag, raw):
        # canonical and raw, with +/- module_base or slot_va
        xs = [(f"{tag}", raw), (f"{tag}|canon", canon48(raw))]
        if try_add_base:
            xs += [(f"{tag}|+mod", (raw + module_base) & ((1<<64)-1)),
                   (f"{tag}|-mod", (raw - module_base) & ((1<<64)-1)),
                   (f"{tag}|canon+mod", (canon48(raw) + module_base) & ((1<<64)-1)),
                   (f"{tag}|canon-mod", (canon48(raw) - module_base) & ((1<<64)-1))]
        if try_add_slot:
            xs += [(f"{tag}|+slot", (raw + slot_va) & ((1<<64)-1)),
                   (f"{tag}|-slot", (raw - slot_va) & ((1<<64)-1)),
                   (f"{tag}|canon+slot", (canon48(raw) + slot_va) & ((1<<64)-1)),
                   (f"{tag}|canon-slot", (canon48(raw) - slot_va) & ((1<<64)-1))]
        return xs

    # base ops
    yield from variants_from_raw("xor", enc ^ key)
    for r in rots:
        yield from variants_from_raw(f"ror_then_xor:{r}", ror64(enc, r) ^ key)
    x = enc ^ key
    for r in rots:
        yield from variants_from_raw(f"xor_then_ror:{r}", ror64(x, r))

    # shift-before combos (rare but quick to try)
    for sh in try_shifts:
        if sh<=0: continue
        yield from variants_from_raw(f"shr{sh}_xor", (enc>>sh) ^ key)
        for r in rots:
            yield from variants_from_raw(f"shr{sh}_ror_then_xor:{r}", ror64(enc>>sh, r) ^ key)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--decoded-json", required=True)
    ap.add_argument("--domain", required=True)
    ap.add_argument("--pid", type=int, required=True)
    ap.add_argument("--module", required=True)
    ap.add_argument("--ist", required=True)
    ap.add_argument("--key", type=lambda x:int(x,16), default=0xEE222BB7EF934A1D)
    ap.add_argument("--rots", nargs="*", type=int, default=[8,13,16,24,31])
    ap.add_argument("--try-add-base", action="store_true", help="also try +/- module_base to decode result")
    ap.add_argument("--try-add-slot", action="store_true", help="also try +/- slot_va to decode result")
    ap.add_argument("--try-shifts", nargs="*", type=int, default=[0,4], help="try >>n before XOR (0 means disabled)")
    ap.add_argument("--text-va",  type=lambda x:int(x,16))
    ap.add_argument("--text-size",type=lambda x:int(x,16))
    ap.add_argument("--rdata-va", type=lambda x:int(x,16))
    ap.add_argument("--rdata-size",type=lambda x:int(x,16))
    ap.add_argument("--max-candidates", type=int, default=24)
    ap.add_argument("--out-json", default="verified_component_bases.json")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    decoded = json.load(open(args.decoded_json))

    ist = IST(args.ist)
    with Libvmi(args.domain) as vmi:
        module_base = find_module_base(args.domain, args.pid, args.module, args.ist)
        if args.verbose: print(f"[i] module {args.module} base = 0x{module_base:016x}")

        # Resolve .text/.rdata ranges
        if None in (args.text_va, args.text_size, args.rdata_va, args.rdata_size):
            secs = parse_sections(vmi, args.pid, module_base)
            txt = secs.get(".text") or secs.get("text")
            rdt = secs.get(".rdata") or secs.get("rdata") or secs.get("_RDATA")
            if not txt or not rdt:
                raise RuntimeError("Cannot find .text/.rdata; pass ranges explicitly.")
            text_va, text_sz   = txt[3], txt[1]
            rdata_va, rdata_sz = rdt[3], rdt[1]
        else:
            text_va, text_sz   = args.text_va, args.text_size
            rdata_va, rdata_sz = args.rdata_va, args.rdata_size

        if args.verbose:
            print(f"[i] .text  = 0x{text_va:016x} .. +0x{text_sz:x}")
            print(f"[i] .rdata = 0x{rdata_va:016x} .. +0x{rdata_sz:x}")

        # Prepare candidate list (need encoded_qword to recompute variants)
        items = []
        for r in decoded.get("results", []):
            slot_va = r.get("slot_va")
            step    = r.get("step")
            enc     = r.get("encoded_qword")
            if slot_va is None or step is None or enc is None: 
                continue
            items.append((slot_va, step, enc))
        # Dedup by (slot_va,step,enc) and limit
        seen=set(); uniq=[]
        for t in items:
            if t in seen: continue
            seen.add(t); uniq.append(t)
        items = uniq[:args.max_candidates]
        if args.verbose: print(f"[i] candidates (slot,step,enc) = {len(items)}")

        passed=[]
        for slot_va, step, enc in items:
            print(f"[*] slot {slot_va:#018x} +{step:#x}  enc={enc:#018x}")
            # Try many variants; stop at the first PASS
            success=False
            for mode, base in make_decode_variants(enc, args.key, args.rots, slot_va, module_base, args.try_add_base, args.try_add_slot, args.try_shifts):
                try:
                    wait_present(vmi, args.pid, base)
                    vptr = read_ptr(vmi, args.pid, base)
                except Exception:
                    continue
                if not in_range(vptr, rdata_va, rdata_sz):
                    continue
                try:
                    vt0 = read_ptr(vmi, args.pid, vptr+0x0)
                    vt1 = read_ptr(vmi, args.pid, vptr+0x8)
                except Exception:
                    continue
                if in_range(vt0, text_va, text_sz) or in_range(vt1, text_va, text_sz):
                    print(f"    [PASS] {mode} -> base {base:#018x}  vptr {vptr:#018x}")
                    passed.append({
                        "slot_va": slot_va, "step": step, "mode": mode,
                        "encoded_qword": enc, "base": base,
                        "vptr": vptr, "vt0": vt0, "vt1": vt1
                    })
                    success=True
                    break
            if not success and args.verbose:
                print("    [MISS] no variant passed for this (slot,step).")

        json.dump({"passed":passed}, open(args.out_json,"w"), indent=2)
        print(f"[+] Wrote PASS results -> {args.out_json}")
        if not passed:
            print("[!] No candidates passed. Try enabling --try-add-base/--try-add-slot or widen --try-shifts.")
if __name__=="__main__": main()
