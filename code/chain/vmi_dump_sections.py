# vmi_dump_sections.py
# 사용:
#   sudo python3 vmi_dump_sections.py <domain> <pid> <module_name> <ist_json> [.text .rdata .gmblk ...]

import sys, json, struct, datetime
from libvmi import Libvmi, libvmi
from vmi_helpers import read_u32
from find_module_base import find_module_base

PAGE = 0x1000

def pe_sections(vmi, pid, base):
    # return dict: name -> (rva, vsize, rawsize)
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
        vsize   = struct.unpack_from("<I", hdr, 8)[0]   # VirtualSize
        rva     = struct.unpack_from("<I", hdr, 12)[0]  # VirtualAddress
        rawsize = struct.unpack_from("<I", hdr, 16)[0]  # SizeOfRawData
        out[name] = (rva, vsize, rawsize)
    return out

def read_bytes_sparse(vmi, pid, start, size):
    """페이지 단위로 읽고, 실패 페이지는 0으로 채워서 크기 유지"""
    data = bytearray(size)
    ok_pages = 0; bad_pages = 0
    off = 0
    while off < size:
        want = min(PAGE - ( (start + off) & (PAGE-1) ), size - off)
        try:
            buf, n = vmi.read_va(start + off, pid, want)
            if n > 0:
                data[off:off+n] = bytes(buf[:n])
                if ( (start + off) & (PAGE-1) ) == 0 and n == PAGE:
                    ok_pages += 1
            else:
                bad_pages += 1
        except libvmi.LibvmiError:
            bad_pages += 1
        off += want
    return bytes(data), ok_pages, bad_pages

def dump_one(vmi, pid, mod, base, sec_name, rva, vsize, rawsize):
    va = base + rva
    # 과도한 읽기 방지: 섹션 가상 크기와 파일 원시 크기 중 작은 값 사용
    size = max(0, min(vsize, rawsize) or vsize)
    # 안전 상한
    size = min(size, 32*1024*1024)
    blob, okp, badp = read_bytes_sparse(vmi, pid, va, size)
    out = f"{mod}_{sec_name.decode() if isinstance(sec_name,bytes) else sec_name}_{hex(va)}_{hex(size)}.bin"
    with open(out, "wb") as f:
        f.write(blob)
    meta = {
        "dump_type": "section",
        "module": mod,
        "section": sec_name.decode() if isinstance(sec_name,bytes) else sec_name,
        "base_va": hex(base),
        "rva": hex(rva),
        "va_start": hex(va),
        "size": hex(size),
        "arch": "x64",
        "endian": "little",
        "timestamp_utc": datetime.datetime.utcnow().isoformat()+"Z",
        "pages_ok": okp, "pages_missing": badp
    }
    with open(out + ".json", "w") as f:
        json.dump(meta, f, indent=2)
    print(f"[dumped] {out}  ok_pages={okp} missing={badp} size={len(blob)}/{size}")

def main():
    if len(sys.argv) < 5:
        print("Usage: vmi_dump_sections.py <domain> <pid> <module_name> <ist_json> [section ...]")
        sys.exit(1)
    dom, pid, mod, ist_json = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
    wanted = [s.encode() if isinstance(s, str) else s for s in sys.argv[5:]]

    with Libvmi(dom) as vmi:
        base = find_module_base(dom, pid, mod, ist_json)
        print(f"[info] module={mod} base=0x{base:016x}")

        secs = pe_sections(vmi, pid, base)
        if not wanted:
            wanted = list(secs.keys())
        for s in wanted:
            if s not in secs:
                print("[skip] section not found:", s.decode() if isinstance(s,bytes) else s)
                continue
            rva, vsize, rawsize = secs[s]
            dump_one(vmi, pid, mod, base, s, rva, vsize, rawsize)

if __name__ == "__main__":
    main()
