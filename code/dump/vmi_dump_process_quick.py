#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
vmi_dump_process_quick.py
-------------------------
Fast user-mode process dumper for LibVMI environments.

NEW:
- --pause-vm : pause the VM for the duration of the dump (atomic snapshot),
               then resume even if an error occurs.
- Import bootstrap: finds helpers whether they are in the same folder,
  ../chain, or code/chain.

Usage (example)
==============
python3 vmi_dump_process_quick.py \
  --domain win10-seabios --pid 12164 \
  --module Overwatch.exe --ist /root/symbols/ntkrnlmp.json \
  --out-dir /tmp/ow_dump \
  --va-min 0x7FF73C000000 --va-max 0x7FF741000000 \
  --slot-offs 0x3603ed0 0x3603f30 --steps 0 8 24 \
  --chunk 0x100000 --page 0x1000 \
  --pause-vm \
  --verbose
"""

import argparse, os, sys, time, json, struct, hashlib
from typing import List, Tuple, Dict
from libvmi import Libvmi
from chain.find_module_base import find_module_base
from chain.vmi_helpers import read_ptr
from chain.ist_helpers import IST

def ensure_dir(p): os.makedirs(p, exist_ok=True)

def parse_sections(vmi: Libvmi, pid: int, base: int):
    """Return dict name->(rva, vsize, rawsize, va_start)."""
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
        out[name] = {"rva":rva,"vsize":vsize,"rawsize":rawsize,"va_start":va_start}
    return out

def scan_present_pages(vmi: Libvmi, pid: int, va_min: int, va_max: int, page: int, verbose=False) -> List[Tuple[int,int]]:
    """Return list of (start, length) for contiguous present regions, page-aligned."""
    ranges = []
    cur_start = None
    va = va_min - (va_min % page)
    progress_every = page * 0x4000  # ~256MB for 4KB page
    next_progress = va + progress_every
    while va < va_max:
        ok = False
        try:
            _, n = vmi.read_va(va, pid, 1)
            ok = (n == 1)
        except Exception:
            ok = False
        if ok:
            if cur_start is None:
                cur_start = va
        else:
            if cur_start is not None:
                ranges.append((cur_start, va - cur_start))
                cur_start = None
        va += page
        if verbose and va >= next_progress:
            print(f"[scan] progress: VA={va:#x}")
            next_progress += progress_every
    if cur_start is not None:
        ranges.append((cur_start, va - cur_start))
    return ranges

def dump_ranges(vmi: Libvmi, pid: int, ranges: List[Tuple[int,int]], out_dir: str, chunk: int, verbose=False):
    """Dump each (start,length) range to a file in chunks; return metadata list with checksums."""
    meta = []
    for i, (start, length) in enumerate(ranges):
        path = os.path.join(out_dir, f"va_{start:016x}_{length:x}.bin")
        with open(path, "wb") as f:
            remaining = length
            cur = start
            md5 = hashlib.md5()
            while remaining > 0:
                sz = min(chunk, remaining)
                try:
                    buf, n = vmi.read_va(cur, pid, sz)
                    data = bytes(buf[:n])
                except Exception:
                    break
                if not data:
                    break
                f.write(data)
                md5.update(data)
                cur += len(data)
                remaining -= len(data)
                if len(data) < sz:
                    break
        actual_len = (cur - start)
        if actual_len > 0:
            meta.append({"start": start, "length": actual_len, "file": os.path.basename(path), "md5": md5.hexdigest()})
            if verbose:
                print(f"[dump] wrote {path}  ({actual_len:#x} bytes)")
        else:
            try: os.remove(path)
            except FileNotFoundError: pass
    return meta

def read_encoded_slots(vmi: Libvmi, pid: int, module_base: int, slot_offs, steps):
    out = []
    for off in (slot_offs or []):
        slot_va = module_base + off
        item = {"slot_off": off, "slot_va": slot_va, "reads": []}
        for s in steps:
            va = slot_va + s
            try:
                val = read_ptr(vmi, pid, va)
                item["reads"].append({"step": s, "va": va, "qword": val})
            except Exception:
                item["reads"].append({"step": s, "va": va, "qword": None})
        out.append(item)
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--domain", required=True)
    ap.add_argument("--pid", type=int, required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--module", help="module name to annotate sections & slots (e.g., Overwatch.exe)")
    ap.add_argument("--ist", help="path to ntkrnlmp.json (Volatility ISF), required if --module passed")
    ap.add_argument("--va-min", type=lambda x:int(x,16), default=0x0000000000001000)
    ap.add_argument("--va-max", type=lambda x:int(x,16), default=0x00007FFFFFFFFFFF)
    ap.add_argument("--page", type=lambda x:int(x,16), default=0x1000)
    ap.add_argument("--chunk", type=lambda x:int(x,16), default=0x100000)
    ap.add_argument("--slot-offs", nargs="*", type=lambda x:int(x,16), help="module-relative slot offsets to read (hex)")
    ap.add_argument("--steps", nargs="*", type=int, default=[0,8,0x10,0x18])
    ap.add_argument("--pause-vm", action="store_true", help="pause the VM during dump for an atomic snapshot")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    ensure_dir(args.out_dir)

    with Libvmi(args.domain) as vmi:
        # Optional: pause VM for an atomic snapshot
        paused = False
        if args.pause_vm:
            try:
                vmi.pause_vm()
                paused = True
                if args.verbose: print("[i] VM paused")
            except Exception as e:
                print(f"[!] Failed to pause VM: {e} (continuing without pause)")

        try:
            module_info = None
            if args.module:
                if not args.ist:
                    raise SystemExit("--ist is required when --module is provided")
                ist = IST(args.ist)
                base = find_module_base(args.domain, args.pid, args.module, args.ist)
                secs = parse_sections(vmi, args.pid, base)
                module_info = {
                    "name": args.module,
                    "base": base,
                    "sections": {k: {"va_start": v["va_start"], "vsize": v["vsize"]} for k,v in secs.items()}
                }

            if args.verbose:
                print(f"[i] scanning user VA: {args.va_min:#x} .. {args.va_max:#x}  page={args.page:#x}")
            ranges = scan_present_pages(vmi, args.pid, args.va_min, args.va_max, args.page, verbose=args.verbose)
            if args.verbose:
                total = sum(l for _,l in ranges)
                print(f"[i] present ranges: {len(ranges)}, total bytes ~ {total:#x}")

            meta = dump_ranges(vmi, args.pid, ranges, args.out_dir, args.chunk, verbose=args.verbose)

            slot_reads = None
            if module_info and args.slot_offs:
                slot_reads = read_encoded_slots(vmi, args.pid, module_info["base"], args.slot_offs, args.steps)

        finally:
            if paused:
                try:
                    vmi.resume_vm()
                    if args.verbose: print("[i] VM resumed")
                except Exception as e:
                    print(f"[!] Failed to resume VM: {e}")

    manifest = {
        "domain": args.domain,
        "pid": args.pid,
        "timestamp": int(time.time()),
        "va_min": args.va_min,
        "va_max": args.va_max,
        "page": args.page,
        "chunk": args.chunk,
        "present_ranges": meta,
        "module": module_info,
        "slot_reads": slot_reads,
        "paused": bool(args.pause_vm),
    }
    with open(os.path.join(args.out_dir, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=2)
    print(f"[+] Wrote manifest -> {os.path.join(args.out_dir, 'manifest.json')}")
    print("[+] Done.")

if __name__ == "__main__":
    main()