#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
프로세스 PID에서 모듈 베이스(DllBase/ImageBase)를 구한다.
편의 래퍼(get_process_by_pid)가 없어도 동작하도록
- PsActiveProcessHead를 따라 EPROCESS를 직접 찾고
- EPROCESS.Peb -> PEB.Ldr -> LDR 리스트에서 모듈 이름을 대조한다.
사용:
  sudo .../python3 find_module_base.py <domain> <pid> <module_name> <ist_json>
예:
  sudo .../python3 find_module_base.py win10-seabios 4692 Notepad.exe /root/symbols/ntkrnlmp.json
"""
import sys
from libvmi import Libvmi
from chain.ist_helpers import IST, read_unicode_string
from chain.vmi_helpers import read_ptr

def get_offset(vmi, ist, key, fallback_type=None, fallback_field=None):
    """libvmi의 windows 오프셋(win_tasks/win_pid/win_peb 등) 우선, 없으면 IST 타입/필드로 폴백"""
    try:
        fn = getattr(vmi, "get_offset", None)
        if callable(fn):
            off = fn(key)
            if off is not None and off != 0:
                return int(off)
    except Exception:
        pass
    if fallback_type and fallback_field:
        return ist.off(fallback_type, fallback_field)
    raise RuntimeError(f"offset '{key}' not available and no fallback provided")

def ksym_va(vmi, name: str) -> int:
    """커널 심볼의 VA: translate_ksym2v / get_kernel_symbol / read_addr_ksym 등 환경별로 시도"""
    # 1) translate_ksym2v
    fn = getattr(vmi, "translate_ksym2v", None)
    if callable(fn):
        try:
            va = fn(name)
            if va: return int(va)
        except Exception:
            pass
    # 2) get_kernel_symbol (환경에 따라 존재)
    fn = getattr(vmi, "get_kernel_symbol", None)
    if callable(fn):
        try:
            va = fn(name)
            if va: return int(va)
        except Exception:
            pass
    # 3) read_addr_ksym: 이건 'Flink 값'을 읽어 올 수 있으므로 헤드 VA가 필요할 땐 부적절
    #    여기서는 사용하지 않음. (필요 시 adapt)
    raise RuntimeError(f"cannot resolve kernel symbol VA: {name}")

def walk_process_list_and_find_eproc(vmi, pid_target: int, tasks_off: int, pid_off: int) -> int:
    """
    PsActiveProcessHead(=LIST_ENTRY head)의 VA를 얻어, ActiveProcessLinks(Flink)를 따라가며
    EPROCESS를 찾아 반환한다.
    """
    head = ksym_va(vmi, "PsActiveProcessHead")  # LIST_ENTRY head 자체의 VA
    cur = read_ptr(vmi, 0, head)                # head->Flink
    while cur and cur != head:
        eproc = cur - tasks_off
        upid = read_ptr(vmi, 0, eproc + pid_off)  # UniqueProcessId (포인터 크기)
        if upid == pid_target:
            return eproc
        cur = read_ptr(vmi, 0, cur)  # 다음 리스트 엔트리(Flink)
    raise RuntimeError(f"EPROCESS not found for pid {pid_target}")

def find_module_base(dom: str, pid: int, modname: str, ist_path: str) -> int:
    ist = IST(ist_path)
    with Libvmi(dom) as vmi:
        # EPROCESS 관련 오프셋 확보
        tasks_off = get_offset(vmi, ist, "win_tasks",        "_EPROCESS", "ActiveProcessLinks")
        pid_off   = get_offset(vmi, ist, "win_pid",          "_EPROCESS", "UniqueProcessId")
        peb_off   = get_offset(vmi, ist, "win_peb",          "_EPROCESS", "Peb")

        # 대상 EPROCESS 구하기
        eproc = walk_process_list_and_find_eproc(vmi, pid, tasks_off, pid_off)

        # PEB -> Ldr
        peb = read_ptr(vmi, 0, eproc + peb_off)
        if peb == 0:
            raise RuntimeError("target process has no PEB (system process?)")
        ldr = read_ptr(vmi, pid, peb + ist.off("_PEB", "Ldr"))

        # 어떤 리스트를 사용할지 결정 (InLoadOrder or InMemoryOrder)
        list_va = ldr + ist.off("_PEB_LDR_DATA", "InLoadOrderModuleList")
        link_field = "InLoadOrderLinks"
        # 첫 엔트리
        flink = read_ptr(vmi, pid, list_va)
        head  = list_va

        while flink and flink != head:
            entry = flink - ist.off("_LDR_DATA_TABLE_ENTRY", link_field)
            dllbase = read_ptr(vmi, pid, entry + ist.off("_LDR_DATA_TABLE_ENTRY", "DllBase"))
            us_va   = entry + ist.off("_LDR_DATA_TABLE_ENTRY", "BaseDllName")
            name    = read_unicode_string(vmi, pid, us_va, ist)
            if name.lower() == modname.lower():
                return dllbase
            flink = read_ptr(vmi, pid, entry + ist.off("_LDR_DATA_TABLE_ENTRY", link_field))
    raise RuntimeError("module not found")

def main():
    if len(sys.argv) < 5:
        print("Usage: find_module_base.py <domain> <pid> <module_name> <ist_json>")
        sys.exit(1)
    dom, pid, modname, ist_path = sys.argv[1], int(sys.argv[2]), sys.argv[3], sys.argv[4]
    try:
        base = find_module_base(dom, pid, modname, ist_path)
        print(f"0x{base:016x}")
    except Exception as e:
        print("[-]", e)
        sys.exit(2)

if __name__ == "__main__":
    main()
