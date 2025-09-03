#!/usr/bin/env python3
# 사용:
#   sudo python3 vmi_list_sections.py <domain> <pid> <module_name> <ist_json>

import sys, struct
from libvmi import Libvmi
from chain.vmi_helpers import read_u32
from chain.find_module_base import find_module_base

def parse_sections(vmi, pid, base):
    # 반환: [ {name,beg_va,rva,vsize,rawsize,rawptr,chars,perm}, ... ] (RVA 오름차순)
    mz = read_u32(vmi, pid, base) & 0xFFFF
    if mz != 0x5A4D:
        raise RuntimeError("MZ not found at module base")
    e_lfanew = read_u32(vmi, pid, base + 0x3C)
    nt = base + e_lfanew
    sig = read_u32(vmi, pid, nt)
    if sig != 0x00004550:
        raise RuntimeError("PE signature not found")

    filehdr = nt + 4
    nsec   = read_u32(vmi, pid, filehdr + 2) & 0xFFFF
    optsz  = read_u32(vmi, pid, filehdr + 16) & 0xFFFF
    shdr   = filehdr + 20 + optsz

    out = []
    for i in range(nsec):
        buf, n = vmi.read_va(shdr + i*40, pid, 40)
        if n != 40:
            raise RuntimeError(f"read_va failed at section header #{i}")
        hdr = bytes(buf)
        name    = hdr[0:8].rstrip(b"\x00").decode(errors="ignore") or f"sec_{i}"
        vsize   = struct.unpack_from("<I", hdr, 8)[0]     # VirtualSize
        rva     = struct.unpack_from("<I", hdr, 12)[0]    # VirtualAddress
        rawsize = struct.unpack_from("<I", hdr, 16)[0]    # SizeOfRawData
        rawptr  = struct.unpack_from("<I", hdr, 20)[0]    # PointerToRawData
        chars   = struct.unpack_from("<I", hdr, 36)[0]    # Characteristics
        perm = "".join([
            "R" if (chars & 0x40000000) else "-",
            "W" if (chars & 0x80000000) else "-",
            "X" if (chars & 0x20000000) else "-",
        ])
        out.append({
            "name": name,
            "rva": rva,
            "beg_va": base + rva,
            "vsize": vsize,
            "rawsize": rawsize,
            "rawptr": rawptr,
            "chars": chars,
            "perm": perm
        })
    out.sort(key=lambda e: e["rva"])
    return out

def main():
    if len(sys.argv) != 5:
        print("Usage: vmi_list_sections.py <domain> <pid> <module_name> <ist_json>")
        sys.exit(1)

    dom, pid, mod, ist_json = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
    with Libvmi(dom) as vmi:
        base = find_module_base(dom, pid, mod, ist_json)
        print(f"[module] {mod}  base=0x{base:016x}\n")
        rows = parse_sections(vmi, pid, base)
        print("Name      RVA       VA(start)           VSize      RawSize   RawPtr  Perm  Chars")
        print("--------- --------- -------------------- ---------- --------- ------- ----- ----------")
        for e in rows:
            print(f"{e['name']:<9}  0x{e['rva']:08x}  0x{e['beg_va']:016x}  0x{e['vsize']:08x}  "
                  f"0x{e['rawsize']:08x}  0x{e['rawptr']:06x}  {e['perm']:<3}   0x{e['chars']:08x}")

if __name__ == "__main__":
    main()
