#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
verify_component_bases_vmi_v3.py
--------------------------------
Fast, non-blocking LibVMI verifier for "component base" candidates.

Key points
- No infinite waiting: we never spin on non-present pages.
- Two input modes:
  (A) decoded JSON from decoder (uses encoded_qword values inside)
  (B) *LIVE* slot offsets: read encoded QWORDs from process .data at runtime
- Tries multiple decode variants (xor / ror-xor / xor-ror, + optional +/- module_base/slot_va, + optional >>shift)
- As soon as a variant yields [base] ∈ .rdata and vt[0|1] ∈ .text → PASS

Usage examples
--------------
(A) From decoded JSON (quick):
python3 verify_component_bases_vmi_v3.py \
  --decoded-json /home/kkongnyang2/decoded_component_bases.json \
  --domain win10-seabios --pid 12164 \
  --module Overwatch.exe --ist /root/symbols/ntkrnlmp.json \
  --text-va 0x7ff73cdd1000 --text-size 0x2000000 \
  --rdata-va 0x7ff73f96b000 --rdata-size 0x916112 \
  --verbose

(B) LIVE (no JSON): read encoded QWORDs at runtime from .data slots
python3 verify_component_bases_vmi_v3.py \
  --live-slot-offs 0x3603ed0 0x3603f30 \
  --steps 0 8 24 \
  --domain win10-seabios --pid 12164 \
  --module Overwatch.exe --ist /root/symbols/ntkrnlmp.json \
  --text-va 0x7ff73cdd1000 --text-size 0x2000000 \
  --rdata-va 0x7ff73f96b000 --rdata-size 0x916112 \
  --verbose

"""

import argparse, json, sys, struct
from typing import Dict, Tuple, List
from libvmi import Libvmi
from find_module_base import find_module_base
from vmi_helpers import read_ptr
from ist_helpers import IST

MASK64 = (1<<64)-1
MASK48 = (1<<48)-1

def parse_sections(vmi, pid, base) -> Dict[str, Tuple[int,int,int,int]]:
    # Minimal PE section reader via VMI
    def rd(va, sz):
        buf, n = vmi.read_va(va, pid, sz)
        if n != sz: raise RuntimeError("read_va failed")
        return bytes(buf)
    mz = int.from_bytes(rd(base,2), "little")
    if (mz & 0xFFFF) != 0x5A4D: raise RuntimeError("MZ not found")
    e_lfanew = int.from_bytes(rd(base+0x3C,4), "little")
    nt = base + e_lfanew
    sig = int.from_bytes(rd(nt,4), "little")
    if sig != 0x00004550: raise RuntimeError("PE signature not found")
    filehdr = nt + 4
    nsec  = int.from_bytes(rd(filehdr+2,2), "little")
    optsz = int.from_bytes(rd(filehdr+16,2), "little")
    shdr  = filehdr + 20 + optsz
    out = {}
    for i in range(nsec):
        hdr = rd(shdr + i*40, 40)
        name    = hdr[0:8].rstrip(b"\x00").decode(errors="ignore") or f"sec_{i}"
        vsize   = struct.unpack_from("<I", hdr, 8)[0]
        rva     = struct.unpack_from("<I", hdr, 12)[0]
        rawsize = struct.unpack_from("<I", hdr, 16)[0]
        va_start= base + rva
        out[name] = (rva, vsize, rawsize, va_start)
    return out

def in_range(x, start, size): return start <= x < start + size
def ror64(x,r): r%=64; return ((x>>r)|((x&((1<<r)-1))<<(64-r))) & MASK64
def canon48(x, sign_extend=False):
    x &= MASK48
    if sign_extend and ((x>>47)&1):
        x |= (~MASK48) & MASK64
    return x

def try_read_ptr(vmi, pid, va):
    try:
        return read_ptr(vmi, pid, va)
    except Exception:
        return None

def decode_variants(enc, key, rots, slot_va, module_base, try_add_base, try_add_slot, shifts):
    def emit(tag, raw):
        yield (f"{tag}", raw)
        yield (f"{tag}|canon", canon48(raw))
        if try_add_base:
            yield (f"{tag}|+mod", (raw + module_base) & MASK64)
            yield (f"{tag}|-mod", (raw - module_base) & MASK64)
            yield (f"{tag}|canon+mod", (canon48(raw) + module_base) & MASK64)
            yield (f"{tag}|canon-mod", (canon48(raw) - module_base) & MASK64)
        if try_add_slot:
            yield (f"{tag}|+slot", (raw + slot_va) & MASK64)
            yield (f"{tag}|-slot", (raw - slot_va) & MASK64)
            yield (f"{tag}|canon+slot", (canon48(raw) + slot_va) & MASK64)
            yield (f"{tag}|canon-slot", (canon48(raw) - slot_va) & MASK64)

    # Base ops
    for t in emit("xor", enc ^ key): yield t
    for r in rots:
        for t in emit(f"ror_then_xor:{r}", ror64(enc, r) ^ key): yield t
    x = enc ^ key
    for r in rots:
        for t in emit(f"xor_then_ror:{r}", ror64(x, r)): yield t

    # Pre-shift variants
    for sh in shifts:
        if sh<=0: continue
        for t in emit(f"shr{sh}_xor", (enc>>sh) ^ key): yield t
        for r in rots:
            for t in emit(f"shr{sh}_ror_then_xor:{r}", ror64(enc>>sh, r) ^ key): yield t

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--decoded-json", help="decoded_component_bases.json (optional if --live-slot-offs used)")
    ap.add_argument("--live-slot-offs", nargs="*", type=lambda x:int(x,16), help="module-relative slot offsets (hex)")
    ap.add_argument("--steps", nargs="*", type=int, default=[0,8,0x10,0x18], help="intra-slot QWORD steps (for live mode)")
    ap.add_argument("--domain", required=True)
    ap.add_argument("--pid", type=int, required=True)
    ap.add_argument("--module", required=True)
    ap.add_argument("--ist", required=True)
    ap.add_argument("--text-va",  type=lambda x:int(x,16))
    ap.add_argument("--text-size",type=lambda x:int(x,16))
    ap.add_argument("--rdata-va", type=lambda x:int(x,16))
    ap.add_argument("--rdata-size",type=lambda x:int(x,16))
    ap.add_argument("--key", type=lambda x:int(x,16), default=0xEE222BB7EF934A1D)
    ap.add_argument("--rots", nargs="*", type=int, default=[8,13,16,24,31])
    ap.add_argument("--try-add-base", action="store_true")
    ap.add_argument("--try-add-slot", action="store_true")
    ap.add_argument("--try-shifts", nargs="*", type=int, default=[0,4])
    ap.add_argument("--max-candidates", type=int, default=24)
    ap.add_argument("--max-variants-per-item", type=int, default=128)
    ap.add_argument("--out-json", default="verified_component_bases.json")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    ist = IST(args.ist)
    with Libvmi(args.domain) as vmi:
        module_base = find_module_base(args.domain, args.pid, args.module, args.ist)

        # Resolve sections
        if None in (args.text_va, args.text_size, args.rdata_va, args.rdata_size):
            secs = parse_sections(vmi, args.pid, module_base)
            txt = secs.get(".text") or secs.get("text")
            rdt = secs.get(".rdata") or secs.get("rdata") or secs.get("_RDATA")
            if not txt or not rdt: raise RuntimeError("Cannot find .text/.rdata; pass ranges explicitly.")
            text_va, text_sz   = txt[3], txt[1]
            rdata_va, rdata_sz = rdt[3], rdt[1]
        else:
            text_va, text_sz   = args.text_va, args.text_size
            rdata_va, rdata_sz = args.rdata_va, args.rdata_size

        if args.verbose:
            print(f"[i] module base = {module_base:#018x}")
            print(f"[i] .text  = {text_va:#018x} .. +0x{text_sz:x}")
            print(f"[i] .rdata = {rdata_va:#018x} .. +0x{rdata_sz:x}")

        # Build candidate list: (slot_va, step, enc)
        items = []

        if args.live_slot_offs:
            # Live read encoded QWORDs from process memory
            for off in args.live_slot_offs:
                slot_va = module_base + off
                for step in args.steps:
                    ptr = slot_va + step
                    val = try_read_ptr(vmi, args.pid, ptr)
                    if val is None: 
                        if args.verbose: print(f"[ ] live read fail at {ptr:#018x}")
                        continue
                    items.append((slot_va, step, val))
        elif args.decoded_json:
            decoded = json.load(open(args.decoded_json))
            for r in decoded.get("results", []):
                slot_va = r.get("slot_va"); step = r.get("step"); enc = r.get("encoded_qword")
                if slot_va is None or step is None or enc is None: continue
                items.append((slot_va, step, enc))
        else:
            print("[-] Provide either --decoded-json or --live-slot-offs"); sys.exit(1)

        # Dedup and cap
        seen=set(); uniq=[]
        for t in items:
            if t in seen: continue
            seen.add(t); uniq.append(t)
        items = uniq[:args.max_candidates]
        if args.verbose: print(f"[i] candidates (slot,step,enc) = {len(items)}")

        passed=[]
        for slot_va, step, enc in items:
            print(f"[*] slot {slot_va:#018x} +{step:#x}  enc={enc:#018x}")
            tried = 0
            for mode, base in decode_variants(enc, args.key, args.rots, slot_va, module_base, args.try_add_base, args.try_add_slot, args.try_shifts):
                tried += 1
                if tried > args.max_variants_per_item:
                    if args.verbose: print("    [CUT] max variants reached for this item"); break

                vptr = try_read_ptr(vmi, args.pid, base)
                if vptr is None: 
                    continue
                if not in_range(vptr, rdata_va, rdata_sz):
                    continue
                vt0 = try_read_ptr(vmi, args.pid, vptr+0x0)
                vt1 = try_read_ptr(vmi, args.pid, vptr+0x8)
                if vt0 is None and vt1 is None: 
                    continue
                ok0 = vt0 is not None and in_range(vt0, text_va, text_sz)
                ok1 = vt1 is not None and in_range(vt1, text_va, text_sz)
                if ok0 or ok1:
                    print(f"    [PASS] {mode} -> base {base:#018x}  vptr {vptr:#018x}")
                    passed.append({
                        "slot_va": slot_va, "step": step, "mode": mode,
                        "encoded_qword": enc, "base": base,
                        "vptr": vptr, "vt0": vt0, "vt1": vt1
                    })
                    break  # stop on first pass for this item

        json.dump({"passed":passed}, open(args.out_json,"w"), indent=2)
        print(f"[+] Wrote PASS results -> {args.out_json}")
        if not passed:
            print("[!] No candidates passed. Try enabling --try-add-base/--try-add-slot or widen --rots/--try-shifts, or use LIVE mode.")
if __name__=="__main__": main()
