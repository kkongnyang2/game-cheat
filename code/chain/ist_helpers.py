#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import json, gzip, lzma
from typing import Optional, Tuple

def _open_any(path):
    with open(path, 'rb') as f:
        sig = f.read(6)
    if sig.startswith(b'\xFD7zXZ\x00'):
        return lzma.open(path, 'rt', encoding='utf-8')
    if sig.startswith(b'\x1F\x8B'):
        return gzip.open(path, 'rt', encoding='utf-8')
    return open(path, 'rt', encoding='utf-8')

class IST:
    def __init__(self, path: str):
        with _open_any(path) as fp:
            self.j = json.load(fp)
        # Vol3 ISF는 'user_types'가 표준. (구버전/타 생성기엔 'types'일 수 있음)
        self._types = self.j.get("user_types") or self.j.get("types")
        if not self._types:
            raise ValueError(f"Unexpected ISF schema: top keys={list(self.j.keys())[:6]}")

    def off(self, typ: str, field: str) -> int:
        return int(self._types[typ]["fields"][field]["offset"])

def read_unicode_string(vmi, pid: int, us_va: int, ist: IST) -> str:
    """
    Read UNICODE_STRING at us_va (VA in target pid) and return Python str.
    UNICODE_STRING { USHORT Length; USHORT MaximumLength; PWSTR Buffer; }
    """
    off_len   = ist.off("_UNICODE_STRING", "Length")
    off_buf   = ist.off("_UNICODE_STRING", "Buffer")
    # Length (bytes)
    b = vmi.read_va(us_va + off_len, pid, 2)[0]
    length = int.from_bytes(bytes(b), "little", signed=False)
    # Buffer
    buf_ptr = int.from_bytes(bytes(vmi.read_va(us_va + off_buf, pid, 8)[0]), "little")
    if length == 0 or buf_ptr == 0:
        return ""
    raw = bytes(vmi.read_va(buf_ptr, pid, length)[0])
    try:
        return raw.decode("utf-16le", errors="ignore")
    except Exception:
        return ""
