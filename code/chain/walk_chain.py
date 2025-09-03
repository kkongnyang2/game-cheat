#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
모듈 베이스 + 체인(+오프셋/*역참조)을 따라 최종 주소를 계산하고 읽어온다.
사용:
  sudo .../python3 walk_chain.py <domain> <pid> <module> <ist_json> "<chain>" [read_len]
예:
  sudo .../python3 walk_chain.py win10-seabios 10220 Tutorial-x86_64.exe /root/symbols/ntkrnlmp.json \
      "+0x34ECA0 * +0x10 * +0x18 * +0x0 * +0x18" 64
"""
import sys
from libvmi import Libvmi
from chain.ist_helpers import IST, read_unicode_string
from chain.vmi_helpers import read_ptr, wait_present

def try_vmi_offset(vmi, key):
    fn = getattr(vmi, "get_offset", None)
    if callable(fn):
        try:
            val = fn(key)
            if val not in (None, 0):
                return int(val)
        except Exception:
            pass
    return None

def ksym_va(vmi, name: str) -> int:
    for api in ("translate_ksym2v", "get_kernel_symbol"):
        fn = getattr(vmi, api, None)
        if callable(fn):
            try:
                va = fn(name)
                if va:
                    return int(va)
            except Exception:
                pass
    raise RuntimeError(f"cannot resolve kernel symbol VA: {name}")

def walk_eproc(vmi, pid_target: int, tasks_off: int, pid_off: int) -> int:
    head = ksym_va(vmi, "PsActiveProcessHead")  # LIST_ENTRY head VA
    flink = read_ptr(vmi, 0, head)              # head->Flink
    while flink and flink != head:
        eproc = flink - tasks_off
        upid  = read_ptr(vmi, 0, eproc + pid_off)
        if upid == pid_target:
            return eproc
        flink = read_ptr(vmi, 0, flink)         # next Flink
    raise RuntimeError(f"EPROCESS not found for pid {pid_target}")

def read_module_base(vmi, pid:int, modname:str, ist:IST) -> int:
    # EPROCESS 오프셋 확보 (libvmi 키 있으면 사용, 없으면 IST 폴백)
    tasks_off = try_vmi_offset(vmi, "win_tasks") or ist.off("_EPROCESS","ActiveProcessLinks")
    pid_off   = try_vmi_offset(vmi, "win_pid")   or ist.off("_EPROCESS","UniqueProcessId")
    peb_off   = ist.off("_EPROCESS","Peb")  # 'win_peb'는 일부 빌드에서 없음

    eproc = walk_eproc(vmi, pid, tasks_off, pid_off)
    peb   = read_ptr(vmi, 0, eproc + peb_off)
    if not peb:
        raise RuntimeError("target process has no PEB")

    ldr = read_ptr(vmi, pid, peb + ist.off("_PEB","Ldr"))

    # InLoadOrder 우선, 필요시 InMemoryOrder로 폴백
    for list_name, link_field in (
        ("InLoadOrderModuleList", "InLoadOrderLinks"),
        ("InMemoryOrderModuleList", "InMemoryOrderLinks"),
    ):
        list_va = ldr + ist.off("_PEB_LDR_DATA", list_name)
        head = list_va
        flink = read_ptr(vmi, pid, list_va)
        while flink and flink != head:
            entry   = flink - ist.off("_LDR_DATA_TABLE_ENTRY", link_field)
            dllbase = read_ptr(vmi, pid, entry + ist.off("_LDR_DATA_TABLE_ENTRY","DllBase"))
            us_va   = entry + ist.off("_LDR_DATA_TABLE_ENTRY","BaseDllName")
            name    = read_unicode_string(vmi, pid, us_va, ist)
            if name.lower() == modname.lower():
                return dllbase
            flink = read_ptr(vmi, pid, entry + ist.off("_LDR_DATA_TABLE_ENTRY", link_field))
    raise RuntimeError("module not found")

def hexdump(b:bytes, width=16)->str:
    out=[]
    for i in range(0,len(b),width):
        chunk=b[i:i+width]
        hexs=' '.join(f'{x:02x}' for x in chunk)
        text=''.join(chr(x) if 32<=x<127 else '.' for x in chunk)
        out.append(f'{i:08x}  {hexs:<{width*3}}  {text}')
    return '\n'.join(out)

def main():
    if len(sys.argv)<6:
        print("Usage: walk_chain.py <domain> <pid> <module> <ist_json> \"<chain>\" [read_len]")
        sys.exit(1)
    dom, pid, module, ist_path, chain = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4], sys.argv[5]
    read_len = int(sys.argv[6]) if len(sys.argv)>6 else 32

    ist = IST(ist_path)
    with Libvmi(dom) as vmi:
        base = read_module_base(vmi, pid, module, ist)
        addr = base
        for t in chain.split():
            if t == '*':
                wait_present(vmi, pid, addr)
                addr = read_ptr(vmi, pid, addr)
            elif t.startswith('+'):
                addr += int(t[1:], 0)
            else:
                raise RuntimeError(f"bad token: {t}")
        print(f"[+] base={module}@0x{base:016x}")
        print(f"[+] final address = 0x{addr:016x}")
        wait_present(vmi, pid, addr)
        buf = bytes(vmi.read_va(addr, pid, read_len)[0])
        print(hexdump(buf))

if __name__ == "__main__":
    main()
