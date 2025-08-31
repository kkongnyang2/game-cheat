#!/usr/bin/env python3
# vmi_dump_process.py
# Usage: sudo python3 vmi_dump_process.py <domain> <pid> <ist_json> <outdir>
import os, sys, json
from libvmi import Libvmi
from ist_helpers import IST
from vmi_helpers import read_u64, read_u32, read_bytes, read_ptr

PAGE = 0x1000

def vpn_to_va(vpn):  # x64
    return vpn * PAGE

def protection_to_str(p):
    # Windows _MMVAD_SHORT->u.VadFlags.Protection encoding (대략 매핑)
    m = {
        0x01: "NOACCESS", 0x02: "READONLY", 0x04: "READWRITE", 0x08: "WRITECOPY",
        0x10: "EXECUTE",  0x20: "EXECUTE_READ", 0x40: "EXECUTE_READWRITE", 0x80: "EXECUTE_WRITECOPY"
    }
    return m.get(p & 0xFF, hex(p))

def walk_vad_tree(vmi, pid, ist, eprocess):
    # 오프셋들 (IST JSON 기준 필드 이름은 환경에 따라 다를 수 있음)
    off_vadroot  = ist.off("EPROCESS", "VadRoot")              # RTL_AVL_TREE or MM_AVL_TABLE
    off_root_ptr = off_vadroot + ist.off("RTL_AVL_TREE", "Root") if ist.has_type("RTL_AVL_TREE") else off_vadroot

    root = read_ptr(vmi, pid, eprocess + off_root_ptr)
    if not root:
        return []

    # MMVAD_SHORT 내부: 균형 트리 노드(Left/Right)와 필드들
    off_left   = ist.off("MMVAD_SHORT", "LeftChild") if ist.has_field("MMVAD_SHORT","LeftChild") else ist.off("MMVAD_SHORT","VadNode.Left")
    off_right  = ist.off("MMVAD_SHORT", "RightChild") if ist.has_field("MMVAD_SHORT","RightChild") else ist.off("MMVAD_SHORT","VadNode.Right")
    off_svpn   = ist.off("MMVAD_SHORT", "StartingVpn")
    off_evpn   = ist.off("MMVAD_SHORT", "EndingVpn")
    # VadFlags.Protection가 bitfield인 경우가 많아 서브오프셋/마스크가 필요할 수 있음
    off_vadflags = ist.off("MMVAD_SHORT", "u.VadFlags") if ist.has_field("MMVAD_SHORT","u.VadFlags") else None
    off_prot     = ist.bitoff("MMVAD_SHORT",
