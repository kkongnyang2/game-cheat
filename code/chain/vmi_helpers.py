#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import time
from typing import Tuple

def read_ptr(vmi, pid: int, va: int) -> int:
    return int.from_bytes(bytes(vmi.read_va(va, pid, 8)[0]), "little")

def read_u32(vmi, pid: int, va: int) -> int:
    return int.from_bytes(bytes(vmi.read_va(va, pid, 4)[0]), "little", signed=False)

def read_u64(vmi, pid: int, va: int) -> int:
    return int.from_bytes(bytes(vmi.read_va(va, pid, 8)[0]), "little", signed=False)

def _write(vmi, pid: int, va: int, data: bytes) -> None:
    """python-libvmi 빌드별 write_va 시그니처 차이를 흡수"""
    try:
        # 대부분의 빌드: (va, pid, data)
        vmi.write_va(va, pid, data)
    except TypeError:
        # 일부 빌드: (va, pid, data, length)
        vmi.write_va(va, pid, data, len(data))

def write_u32(vmi, pid: int, va: int, val: int) -> None:
    b = (val & 0xffffffff).to_bytes(4, "little")
    _write(vmi, pid, va, b)

def write_u64(vmi, pid: int, va: int, val: int) -> None:
    b = (val & 0xffffffffffffffff).to_bytes(8, "little")
    _write(vmi, pid, va, b)

def write_utf16(vmi, pid: int, va: int, s: str) -> None:
    b = s.encode("utf-16le")
    _write(vmi, pid, va, b)

def wait_present(vmi, pid: int, va: int, timeout: float = 5.0) -> bool:
    """
    Best-effort: 해당 VA가 메모리에 올라올 때까지 1바이트 읽기를 시도.
    """
    t0 = time.time()
    while True:
        try:
            vmi.read_va(va, pid, 1)
            return True
        except Exception:
            if time.time() - t0 > timeout:
                return False
            time.sleep(0.05)
