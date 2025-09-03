#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
verify_component_bases_vmi.py
Self-contained LibVMI verifier for decoded component-base candidates.
"""
import argparse, json, sys, struct
from typing import Dict, Tuple
from libvmi import Libvmi
from find_module_base import find_module_base
from vmi_helpers import read_ptr, wait_present
from ist_helpers import IST

def parse_sections(vmi, pid, base) -> Dict[str, Tuple[int,int,int,int]]:
    # minimal PE section reader via VMI
    mz_buf, _ = vmi.read_va(base, pid, 2)
    mz = int.from_bytes(bytes(mz_buf), "little")
    if (mz & 0xFFFF) != 0x5A4D:
        raise RuntimeError("MZ not found")
    e_lfanew_buf, _ = vmi.read_va(base+0x3C, pid, 4)
    e_lfanew = int.from_bytes(bytes(e_lfanew_buf), "little")
    nt = base + e_lfanew
    sig_buf, _ = vmi.read_va(nt, pid, 4)
    sig = int.from_bytes(bytes(sig_buf), "little")
    if sig != 0x00004550:
        raise RuntimeError("PE signature not found")
    filehdr = nt + 4
    nsec_buf, _ = vmi.read_va(filehdr + 2, pid, 2)
    nsec = int.from_bytes(bytes(nsec_buf), "little")
    optsz_buf, _ = vmi.read_va(filehdr + 16, pid, 2)
    optsz = int.from_bytes(bytes(optsz_buf), "little")
    shdr = filehdr + 20 + optsz
    out = {}
    for i in range(nsec):
        buf, n = vmi.read_va(shdr + i*40, pid, 40)
        if n != 40:
            raise RuntimeError("read_va failed at section header")
        hdr = bytes(buf)
        name    = hdr[0:8].rstrip(b"\x00").decode(errors="ignore") or f"sec_{i}"
        vsize   = struct.unpack_from("<I", hdr, 8)[0]
        rva     = struct.unpack_from("<I", hdr, 12)[0]
        rawsize = struct.unpack_from("<I", hdr, 16)[0]
        va_start= base + rva
        out[name] = (rva, vsize, rawsize, va_start)
    return out

def in_range(x, start, size): return start <= x < start + size
def uniq_by(items, key):
    seen=set(); out=[]
    for it in items:
        k=key(it)
        if k in seen: continue
        seen.add(k); out.append(it)
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--decoded-json", required=True)
    ap.add_argument("--domain", required=True)
    ap.add_argument("--pid", type=int, required=True)
    ap.add_argument("--module", required=True)
    ap.add_argument("--ist", required=True)

    ap.add_argument("--text-va",  type=lambda x:int(x,16))
    ap.add_argument("--text-size",type=lambda x:int(x,16))
    ap.add_argument("--rdata-va", type=lambda x:int(x,16))
    ap.add_argument("--rdata-size",type=lambda x:int(x,16))

    ap.add_argument("--modes", nargs="*", default=["xor"])
    ap.add_argument("--max-candidates", type=int, default=16)
    ap.add_argument("--out-json", default="verified_component_bases.json")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    decoded = json.load(open(args.decoded_json))

    ist = IST(args.ist)
    with Libvmi(args.domain) as vmi:
        base = find_module_base(args.domain, args.pid, args.module, args.ist)
        if args.verbose: print(f"[i] module {args.module} base = 0x{base:016x}")

        # Resolve .text/.rdata ranges
        if None in (args.text_va, args.text_size, args.rdata_va, args.rdata_size):
            secs = parse_sections(vmi, args.pid, base)
            txt = secs.get(".text") or secs.get("text")
            rdt = secs.get(".rdata") or secs.get("rdata") or secs.get("_RDATA")
            if not txt or not rdt: 
                raise RuntimeError("Cannot find .text/.rdata; pass --text-va/--text-size/--rdata-va/--rdata-size explicitly.")
            text_va, text_sz   = txt[3], txt[1]
            rdata_va, rdata_sz = rdt[3], rdt[1]
        else:
            text_va, text_sz   = args.text_va, args.text_size
            rdata_va, rdata_sz = args.rdata_va, args.rdata_size

        if args.verbose:
            print(f"[i] .text  = 0x{text_va:016x} .. +0x{text_sz:x}")
            print(f"[i] .rdata = 0x{rdata_va:016x} .. +0x{rdata_sz:x}")

        items = [r for r in decoded.get("results", []) if r.get("mode") in args.modes and "decoded_canonical" in r]
        items = uniq_by(items, key=lambda r: r["decoded_canonical"])[:args.max_candidates]
        if args.verbose: print(f"[i] candidates after filter = {len(items)}")
        if not items: 
            print("[-] No candidates after filtering."); sys.exit(1)

        passed=[]
        for r in items:
            base_ptr = r["decoded_canonical"]
            print(f"[*] base {base_ptr:#018x}  (slot {r['slot_va']:#018x} +{r['step']:#x}, mode={r['mode']})")

            try:
                wait_present(vmi, args.pid, base_ptr)
                vptr = read_ptr(vmi, args.pid, base_ptr)
            except Exception as e:
                print("    [FAIL] cannot read [base]:", e); continue
            ok_vptr = in_range(vptr, rdata_va, rdata_sz)
            print(f"    [vptr] {vptr:#018x}  in .rdata? {ok_vptr}")
            if not ok_vptr:
                print("    [FAIL] vptr not in .rdata"); continue

            try:
                vt0 = read_ptr(vmi, args.pid, vptr+0x0)
                vt1 = read_ptr(vmi, args.pid, vptr+0x8)
            except Exception as e:
                print("    [FAIL] cannot read vtable entries:", e); continue
            ok0 = in_range(vt0, text_va, text_sz)
            ok1 = in_range(vt1, text_va, text_sz)
            print(f"    [vt[0]] {vt0:#018x}  in .text? {ok0}")
            print(f"    [vt[1]] {vt1:#018x}  in .text? {ok1}")
            if ok0 or ok1:
                print("    [PASS] Valid C++ object base")
                passed.append({"base":base_ptr,"slot_va":r["slot_va"],"step":r["step"],"mode":r["mode"],
                               "vptr":vptr,"vt0":vt0,"vt1":vt1})
            else:
                print("    [FAIL] vtable entries not in .text")

        json.dump({"passed":passed}, open(args.out_json,"w"), indent=2)
        print(f"[+] Wrote PASS results -> {args.out_json}")
        if not passed: print("[!] No candidates passed. Consider relaxing filters.")
if __name__=="__main__": main()
