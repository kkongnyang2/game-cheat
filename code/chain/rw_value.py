#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
체인을 따라간 '최종 주소'에서 값을 읽거나 쓰기.

읽기:
  sudo .../python3 rw_value.py <domain> <pid> <module> <ist_json> "<chain>" --read [--type u32|u64|utf16 --n 32]

쓰기:
  sudo .../python3 rw_value.py <domain> <pid> <module> <ist_json> "<chain>" --write --type u32 --value 1337
  sudo .../python3 rw_value.py <domain> <pid> <module> <ist_json> "<chain>" --write --type utf16 --text "HELLO"

체인 표기법은 walk_chain.py와 동일: "+0xOFF * +0xOFF * ..."
"""
import sys
from libvmi import Libvmi
from ist_helpers import IST
from vmi_helpers import read_ptr, read_u32, read_u64, write_u32, write_u64, write_utf16, wait_present
from walk_chain import read_module_base

def resolve_addr(vmi, pid:int, module:str, ist:IST, chain:str) -> int:
    addr = read_module_base(vmi, pid, module, ist)
    for t in chain.split():
        if t == '*':
            wait_present(vmi, pid, addr)
            addr = read_ptr(vmi, pid, addr)
        elif t.startswith('+'):
            addr += int(t[1:], 0)
        else:
            raise RuntimeError(f"bad token: {t}")
    return addr

def main():
    if len(sys.argv) < 7:
        print(__doc__)
        sys.exit(1)

    dom, pid, module, ist_path, chain = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4], sys.argv[5]
    mode = sys.argv[6]
    ist = IST(ist_path)

    with Libvmi(dom) as vmi:
        addr = resolve_addr(vmi, pid, module, ist, chain)
        print(f"[+] final address: 0x{addr:016x}")

        if mode == "--read":
            vtype = sys.argv[7] if len(sys.argv)>7 else "u32"
            if vtype == "--type":
                vtype = sys.argv[8] if len(sys.argv)>8 else "u32"
            if vtype == "u32":
                wait_present(vmi, pid, addr)
                val = read_u32(vmi, pid, addr)
                print(f"u32 = {val} (0x{val:08x})")
            elif vtype == "u64":
                wait_present(vmi, pid, addr)
                val = read_u64(vmi, pid, addr)
                print(f"u64 = {val} (0x{val:016x})")
            elif vtype == "utf16":
                n = 64
                if "--n" in sys.argv:
                    n = int(sys.argv[sys.argv.index("--n")+1])
                wait_present(vmi, pid, addr)
                b = bytes(vmi.read_va(addr, pid, n)[0])
                s = b.decode("utf-16le", errors="ignore").split("\x00")[0]
                print(f"utf16 = {repr(s)}")
            else:
                raise RuntimeError("unknown type")
        elif mode == "--write":
            # --type <u32|u64|utf16>  + value/text
            if "--type" not in sys.argv:
                raise RuntimeError("--type required")
            vtype = sys.argv[sys.argv.index("--type")+1]
            if vtype in ("u32","u64"):
                if "--value" not in sys.argv:
                    raise RuntimeError("--value required for integer types")
                val = int(sys.argv[sys.argv.index("--value")+1], 0)
                wait_present(vmi, pid, addr)
                if vtype == "u32": write_u32(vmi, pid, addr, val)
                else: write_u64(vmi, pid, addr, val)
                print(f"[OK] wrote {vtype} {val} at 0x{addr:016x}")
            elif vtype == "utf16":
                if "--text" not in sys.argv:
                    raise RuntimeError("--text required for utf16")
                text = sys.argv[sys.argv.index("--text")+1]
                wait_present(vmi, pid, addr)
                write_utf16(vmi, pid, addr, text)
                print(f"[OK] wrote utf16 '{text}' at 0x{addr:016x}")
            else:
                raise RuntimeError("unknown type")
        else:
            print(__doc__)
            sys.exit(1)

if __name__ == "__main__":
    main()
