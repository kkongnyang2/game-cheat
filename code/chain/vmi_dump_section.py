#!/usr/bin/env python3
# 사용:
#   sudo python3 vmi_dump_section.py <domain> <pid> <module_name> <ist_json> <section_name> [--cap 0x2000000]
# 예: sudo python3 vmi_dump_section.py win10-seabios 11280 abc.exe /root/symbols/ntkrnlmp.json .rdata

import sys, json, struct, datetime
from libvmi import Libvmi, libvmi
from vmi_helpers import read_u32
from find_module_base import find_module_base

PAGE = 0x1000

def read_sections(vmi, pid, base):
    # name -> (rva, vsize, rawsize)
    mz = read_u32(vmi, pid, base) & 0xFFFF
    if mz != 0x5A4D: raise RuntimeError("MZ not found")
    e_lfanew = read_u32(vmi, pid, base + 0x3C)
    nt = base + e_lfanew
    sig = read_u32(vmi, pid, nt)
    if sig != 0x00004550: raise RuntimeError("PE signature not found")
    filehdr = nt + 4
    nsec = read_u32(vmi, pid, filehdr + 2) & 0xFFFF
    optsz = read_u32(vmi, pid, filehdr + 16) & 0xFFFF
    shdr = filehdr + 20 + optsz

    out = {}
    for i in range(nsec):
        buf, n = vmi.read_va(shdr + i*40, pid, 40)
        if n != 40: raise RuntimeError(f"read_va failed at sec#{i}")
        hdr = bytes(buf)
        name    = hdr[0:8].rstrip(b"\x00")
        vsize   = struct.unpack_from("<I", hdr, 8)[0]
        rva     = struct.unpack_from("<I", hdr, 12)[0]
        rawsize = struct.unpack_from("<I", hdr, 16)[0]
        out[name] = (rva, vsize, rawsize)
    return out

def read_bytes_sparse(vmi, pid, start, size):
    data = bytearray(size)
    off = 0
    while off < size:
        want = min(PAGE - ((start + off) & (PAGE-1)), size - off)
        try:
            buf, n = vmi.read_va(start + off, pid, want)
            if n > 0:
                data[off:off+n] = bytes(buf[:n])
        except libvmi.LibvmiError:
            pass
        off += want
    return bytes(data)

def main():
    if len(sys.argv) < 6:
        print("Usage: vmi_dump_section.py <domain> <pid> <module_name> <ist_json> <section_name> [--cap 0xSIZE]")
        sys.exit(1)
    dom, pid, mod, ist_json, secname = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4], sys.argv[5]
    cap = None
    if len(sys.argv) >= 8 and sys.argv[6] == "--cap":
        cap = int(sys.argv[7], 16)

    with Libvmi(dom) as vmi:
        base = find_module_base(dom, pid, mod, ist_json)
        secs = read_sections(vmi, pid, base)

        key = secname.encode() if isinstance(secname, str) else secname
        if key not in secs:
            print(f"[error] section not found: {secname}")
            print("[hint] 먼저 vmi_list_sections.py로 섹션 이름을 확인하세요.")
            sys.exit(2)

        rva, vsize, rawsize = secs[key]
        size = max(0, min(vsize, rawsize) or vsize)
        if cap: size = min(size, cap)

        va = base + rva
        blob = read_bytes_sparse(vmi, pid, va, size)

        out = f"{mod}_{secname}_{hex(va)}_{hex(size)}.bin"
        with open(out, "wb") as f:
            f.write(blob)
        meta = {
            "dump_type": "section",
            "module": mod,
            "section": secname,
            "base_va": hex(base),
            "rva": hex(rva),
            "va_start": hex(va),
            "size": hex(size),
            "arch": "x64",
            "endian": "little",
            "timestamp_utc": datetime.datetime.utcnow().isoformat()+"Z"
        }
        with open(out + ".json", "w") as f:
            json.dump(meta, f, indent=2)

        print(f"[dumped] {out}  (size={len(blob)}/{size})")

if __name__ == "__main__":
    main()
