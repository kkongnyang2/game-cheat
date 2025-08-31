#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gm_finder_vmi.py
사용:
  sudo python3 gm_finder_vmi.py <domain> <pid> FakeGame.exe <ist_json>
"""
import sys, struct
from libvmi import Libvmi
from ist_helpers import IST                         # :contentReference[oaicite:8]{index=8}
from vmi_helpers import read_ptr, read_u64, read_u32, read_bytes  # :contentReference[oaicite:9]{index=9}
from walk_chain import read_module_base             # :contentReference[oaicite:10]{index=10}

def u64(x): return x & 0xFFFFFFFFFFFFFFFF
def rol64(x, r): r &= 63; x &= 0xFFFFFFFFFFFFFFFF; return u64(((x << r) | (x >> (64-r))))
def ror64(x, r): r &= 63; x &= 0xFFFFFFFFFFFFFFFF; return u64(((x >> r) | (x << (64-r))))

# 섹션 내부 오프셋 (FakeGame.cpp와 동일)
INSEC_KEYBLOCK      = 0x0100
INSEC_SEED_A        = 0x0200
INSEC_SEED_B        = 0x0208
INSEC_FINALMASK_SRC = 0x0300
INSEC_PTR_BLOCK     = 0x0400
INSEC_GAMEMANAGER   = 0x0800
TABLE_QWORDS        = 20
CONST_XOR   = 0x0F74216354A20866
CONST_FINAL = 0x14737E1555DD8CD7

def pe_find_section(vmi, pid, base_va, name: bytes) -> int:
    """모듈 베이스에서 PE 헤더를 읽어 'name' 섹션의 RVA 반환"""
    # DOS
    mz = read_u32(vmi, pid, base_va) & 0xFFFF
    if mz != 0x5A4D: raise RuntimeError("MZ not found")
    e_lfanew = read_u32(vmi, pid, base_va + 0x3C)
    nt = base_va + e_lfanew
    # NT
    sig = read_u32(vmi, pid, nt)
    if sig != 0x00004550: raise RuntimeError("PE signature not found")
    filehdr = nt + 4
    num_sections = read_u32(vmi, pid, filehdr + 2) & 0xFFFF
    opt_size     = read_u32(vmi, pid, filehdr + 16) & 0xFFFF
    sect = filehdr + 20 + opt_size  # 첫 섹션 헤더
    for i in range(num_sections):
        hdr = bytes(vmi.read_va(sect + i*40, pid, 40)[0])
        sect_name = hdr[0:8].rstrip(b"\x00")
        virt_addr = struct.unpack_from("<I", hdr, 12)[0]  # VirtualAddress
        if sect_name == name:
            return virt_addr
    raise RuntimeError(f"section {name!r} not found")

def gm_index(X):
    acc = X[0]^X[1]^X[2]^X[3]
    acc ^= X[7]
    return acc & 0x0B  # 0..11

def main():
    if len(sys.argv) != 5:
        print("Usage: gm_finder_vmi.py <domain> <pid> <module_name> <ist_json>")
        sys.exit(1)
    dom, pid, modname, ist_path = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
    ist = IST(ist_path)  # IST는 여기선 섹션 파싱엔 안 쓰지만, read_module_base가 PEB·Ldr 탐색에 사용 :contentReference[oaicite:11]{index=11}
    with Libvmi(dom) as vmi:
        base = read_module_base(vmi, pid, modname, ist)  # PEB->Ldr 탐색 경로 재사용 :contentReference[oaicite:12]{index=12}
        sec_rva = pe_find_section(vmi, pid, base, b".gmblk")
        sec = base + sec_rva

        # Seeds
        seedA = read_u64(vmi, pid, sec + INSEC_SEED_A)
        seedB = read_u64(vmi, pid, sec + INSEC_SEED_B)
        GMKey1 = rol64(seedA ^ 0x1111222233334444, 17) ^ ror64(seedB ^ 0x9999AAAABBBBCCCC, 11)
        GMKey2 = ror64(seedA ^ 0x5555666677778888, 29) ^ rol64(seedB ^ 0xDDDDCCCCBBBBAAAA, 7)

        v2 = sec + INSEC_KEYBLOCK
        v3 = v2 + 0x8
        v5 = rol64(seedA + seedB, 7) ^ GMKey1
        GMXorKey = u64(v5 ^ (~v3) ^ CONST_XOR ^ GMKey2)

        # Table decode
        X = [ u64(read_u64(vmi, pid, v3 + 8*i) ^ GMXorKey) for i in range(TABLE_QWORDS) ]
        idx = gm_index(X)
        candidate = X[idx + 4]

        # Final step: *(candidate + 0x30) == (gm ^ FinalMask)
        engm = read_u64(vmi, pid, candidate + 0x30)
        FinalMask = u64(read_u64(vmi, pid, sec + INSEC_FINALMASK_SRC) - CONST_FINAL)
        gm_addr = u64(engm ^ FinalMask)

        # Verify
        head  = read_u64(vmi, pid, gm_addr + 0x20)
        count = read_u32(vmi, pid, gm_addr + 0x2C)
        uid   = read_u64(vmi, pid, gm_addr + 0x2E8)
        ok = (head != 0) and (0 < count < 1000000) and (uid != 0)

        print(f"[finder] base      = 0x{base:016x}")
        print(f"[finder] .gmblk RVA= 0x{sec_rva:08x}  VA=0x{sec:016x}")
        print(f"[finder] seedA/B   = {hex(seedA)} {hex(seedB)}")
        print(f"[finder] GMKey1/2  = {hex(GMKey1)} {hex(GMKey2)}")
        print(f"[finder] GMXorKey  = {hex(GMXorKey)}  v3=0x{v3:016x}  idx={idx}")
        print(f"[finder] candidate = 0x{candidate:016x}")
        print(f"[finder] gm_addr   = 0x{gm_addr:016x}")
        print(f"[finder] verify    = {ok}  head=0x{head:016x}  count={count}  uid={hex(uid)}")

if __name__ == "__main__":
    main()
