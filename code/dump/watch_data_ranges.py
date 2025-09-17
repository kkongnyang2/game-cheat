#!/usr/bin/env python3
import argparse, os, sys, time, subprocess, select
from pathlib import Path
from collections import defaultdict, Counter
from datetime import datetime, timezone

# Optional dependency: python-libvmi for VA->PFN 변환
try:
    from libvmi import Libvmi
except Exception:
    Libvmi = None

PAGE = 0x1000
SEG_BEGIN = 0x0000020000000000
SEG_END   = 0x0000030000000000

def get_domid_from_name(name: str) -> str:
    try:
        out = subprocess.check_output(["xl", "domid", name], text=True).strip()
        if out.isdigit():
            return out
    except Exception as e:
        print("[warn] xl domid failed:", e)
    return None

def dump_data_section(py_exe: str, dumper_path: Path, domain: str, pid: int, module: str, ist_json: str,
                      outdir: Path, cap_hex: str=None, use_module: str=None, project_root: str=None) -> Path:
    """Run vmi_dump_section (.py or -m) to dump .data. Returns path to the created .bin"""
    outdir.mkdir(parents=True, exist_ok=True)
    cwd = os.getcwd()
    os.chdir(outdir)
    try:
        env = os.environ.copy()
        if project_root:
            pr = os.path.expanduser(project_root)
            env["PYTHONPATH"] = pr + os.pathsep + env.get("PYTHONPATH", "")
        if use_module:
            cmd = [py_exe, "-m", use_module, domain, str(pid), module, ist_json, ".data"]
        else:
            cmd = [py_exe, str(dumper_path), domain, str(pid), module, ist_json, ".data"]
        if cap_hex:
            cmd += ["--cap", cap_hex]
        print("[dump] running:", " ".join(cmd))
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, env=env)
        print(p.stdout)
        if p.returncode != 0:
            print(p.stderr)
            raise SystemExit(f"dump failed with code {p.returncode}")
        candidates = sorted(outdir.glob(f"{module}_.data_*_*.bin"), key=lambda p: p.stat().st_mtime, reverse=True)
        if not candidates:
            raise SystemExit("could not find .data dump output")
        return candidates[0]
    finally:
        os.chdir(cwd)

def _segments_from_bucket(values, gap_pages):
    """Split sorted values into subsegments whenever gap > gap_pages pages"""
    if not values:
        return []
    values = sorted(values)
    segs = [[values[0], values[0]]]
    last = values[0]
    max_gap = gap_pages * PAGE
    for v in values[1:]:
        if v - last > max_gap:
            segs.append([v, v])
        else:
            segs[-1][1] = v
        last = v
    return segs

def cluster_ranges(values, topk=2, pad=0x10000, gran_log2=22, gap_pages=64):
    """
    Bucket by addr>>gran_log2 (default 2^22 = 4MB windows), keep top-k buckets by count.
    For each bucket, optionally split into subsegments using gap_pages (default 64 pages = 256KB).
    Build [min..max] per segment, 4KB-align, pad (default 64KB), merge overlaps, return ranges.
    """
    if not values:
        return []
    buckets = defaultdict(list)
    for v in values:
        buckets[v >> gran_log2].append(v)
    counts = Counter({k: len(v) for k,v in buckets.items()})
    topkeys = [k for k,_ in counts.most_common(topk)]
    ranges = []
    for k in topkeys:
        lst = sorted(buckets[k])
        if gap_pages and gap_pages > 0:
            segs = _segments_from_bucket(lst, gap_pages)
        else:
            segs = [[lst[0], lst[-1]]]
        for (lo_val, hi_val) in segs:
            lo = (lo_val // PAGE) * PAGE
            hi = ((hi_val + PAGE - 1) // PAGE) * PAGE
            lo = max(0, lo - pad)
            hi = hi + pad
            ranges.append([lo, hi])
    ranges.sort()
    merged = []
    for lo, hi in ranges:
        if not merged or lo > merged[-1][1]:
            merged.append([lo, hi])
        else:
            merged[-1][1] = max(merged[-1][1], hi)
    return merged

def unique(seq):
    seen = set()
    out = []
    for x in seq:
        if x not in seen:
            seen.add(x)
            out.append(x)
    return out

def build_anchor_ranges(heap_vals, anchors_per_cycle=16, pad_pages=1):
    """Pick unique heap addresses and build tiny page-aligned windows around them."""
    if not heap_vals:
        return []
    uniq = unique(sorted(heap_vals))
    step = max(1, len(uniq) // max(1, anchors_per_cycle))
    picks = [uniq[i] for i in range(0, min(len(uniq), anchors_per_cycle*step), step)]
    ranges = []
    for v in picks[:anchors_per_cycle]:
        page = (v // PAGE) * PAGE
        start = max(0, page - pad_pages*PAGE)
        end = page + (1 + pad_pages)*PAGE
        ranges.append([start, end])
    return ranges

def parse_seed(spec: str):
    """
    Parse seed like '0x27b6dba1234' or '0x27b6dba1234@2' (pad pages after '@').
    Returns (addr_int, pad_pages_int).
    """
    s = spec.strip()
    if '@' in s:
        addr_s, pad_s = s.split('@', 1)
        pad = int(pad_s, 0)
    else:
        addr_s, pad = s, None
    addr = int(addr_s, 16) if addr_s.lower().startswith('0x') else int(addr_s, 10)
    return addr, pad

def build_seed_ranges(seed_specs, default_pad_pages=1):
    ranges = []
    for spec in seed_specs or []:
        addr, pad = parse_seed(spec)
        if pad is None:
            pad = default_pad_pages
        page = (addr // PAGE) * PAGE
        start = max(0, page - pad*PAGE)
        end = page + (1 + pad)*PAGE
        ranges.append([start, end])
    return ranges

# ------------------- PFN 전환 보조 -------------------

def _find_va_to_pa_api(vmi):
    candidates = [
        "v2p", "virt_to_phys", "convert_virt_to_phys", "va_to_pa",
        "virtual_to_physical", "convert_virt_addr_to_phys"
    ]
    for name in candidates:
        fn = getattr(vmi, name, None)
        if callable(fn):
            return fn
    return None

# --- VA→PFN: python-libvmi v3.7.x 확정 경로 ---
PAGE = 0x1000

def va_to_pfn(vmi, pid: int, va: int):
    """
    v3.7.x python-libvmi:
      - 유저 VA:  translate_uv2p(va, pid)  ← pid는 32bit 취급
      - 커널 VA:  translate_kv2p(va)
    """
    try:
        if va >= 0xFFFF000000000000:
            pa = vmi.translate_kv2p(int(va))
        else:
            # 두 번째 인자는 PID (32bit)
            pa = vmi.translate_uv2p(int(va), int(pid) & 0xffffffff)
    except TypeError:
        # 혹시 시그니처가 (pid, va)로 빌드된 경우 대비
        try:
            pa = vmi.translate_uv2p(int(pid) & 0xffffffff, int(va))
        except Exception:
            pa = None
    except Exception:
        pa = None

    if pa:
        return int(pa) >> 12
    return None


def vas_to_pfns(vmi, pid: int, lo: int, hi: int, max_pfns: int = 4096):
    """[lo,hi) VA 범위를 PFN 집합으로 변환. 실패한 페이지는 건너뜀."""
    pfns, seen = [], set()
    page_lo = (lo // PAGE) * PAGE
    page_hi = ((hi + PAGE - 1) // PAGE) * PAGE
    for va in range(page_lo, page_hi, PAGE):
        pfn = va_to_pfn(vmi, pid, va)
        if pfn is not None and pfn not in seen:
            seen.add(pfn)
            pfns.append(pfn)
            if len(pfns) >= max_pfns:
                break
    return pfns


def coalesce_pfns_to_ranges(pfns):
    """연속 PFN을 하나의 [start,end] (inclusive) 구간으로 묶음."""
    if not pfns:
        return []
    pfns = sorted(pfns)
    ranges = []
    s = e = pfns[0]
    for p in pfns[1:]:
        if p == e + 1:
            e = p
        else:
            ranges.append((s, e))
            s = e = p
    ranges.append((s, e))
    return ranges

# ------------------- DRAKVUF 호출 -------------------

def run_writewatch_va(drakvuf: str, ist_json: str, domid: str, pid: int, start: int, end: int, duration: int, logdir: Path,
                      max_events: int = 5000, max_bytes: int = 8_000_000):
    """구버전: pid_va_writewatch (VA 감시)."""
    env = os.environ.copy()
    env["WRITEWATCH_PID"] = str(pid)
    env["WRITEWATCH_RANGE"] = f"0x{start:x}-0x{end:x}"
    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    logdir.mkdir(parents=True, exist_ok=True)
    log = logdir / f"writewatch_{pid}_{start:016x}-{end:016x}_{ts}.jsonl"
    cmd = [drakvuf, "-r", ist_json, "-d", domid, "-a", "pid_va_writewatch", "-o", "json"]
    print("[writewatch/VA] START", " ".join(cmd), "→", log, f"(max_events={max_events}, max_bytes={max_bytes})")
    return _run_and_log(cmd, env, log, duration, max_events, max_bytes)

def run_writewatch_pfn(drakvuf: str, ist_json: str, domid: str, pfn_list, duration: int, logdir: Path,
                       max_events: int = 5000, max_bytes: int = 8_000_000):
    """신규: pfn_writewatch (PFN 감시). pfn_list: int들의 리스트."""
    env = os.environ.copy()
    if not pfn_list:
        raise SystemExit("no PFNs to watch")
    if len(pfn_list) > 8192:
        print(f"[warn] PFN list too long ({len(pfn_list)}); truncating to 8192")
        pfn_list = pfn_list[:8192]
    env["WRITEWATCH_PFNS"] = ",".join(f"0x{p:x}" for p in pfn_list)
    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    logdir.mkdir(parents=True, exist_ok=True)
    log = logdir / f"pfnwatch_{len(pfn_list)}pfns_{ts}.jsonl"
    cmd = [drakvuf, "-r", ist_json, "-d", domid, "-a", "pfn_writewatch", "-o", "json"]
    print("[writewatch/PFN] START", " ".join(cmd), "→", log, f"(PFNs={len(pfn_list)}, max_events={max_events}, max_bytes={max_bytes})")
    return _run_and_log(cmd, env, log, duration, max_events, max_bytes)

def _run_and_log(cmd, env, log, duration, max_events, max_bytes):
    with open(log, "w") as f:
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env, text=True, bufsize=1)
        events = 0
        size = 0
        start_t = time.time()
        try:
            while True:
                rlist, _, _ = select.select([p.stdout], [], [], 0.5)
                if rlist:
                    line = p.stdout.readline()
                    if not line:
                        if p.poll() is not None:
                            break
                        continue
                    f.write(line)
                    size += len(line)
                    if line.startswith("{"):
                        events += 1
                    if events >= max_events or size >= max_bytes:
                        print(f"[writewatch] storm guard hit (events={events}, bytes={size}); terminating")
                        p.terminate()
                        break
                if time.time() - start_t >= duration:
                    print("[writewatch] timeout reached; terminating")
                    p.terminate()
                    break
                if p.poll() is not None:
                    break
            try:
                p.wait(timeout=3)
            except subprocess.TimeoutExpired:
                p.kill()
        except KeyboardInterrupt:
            p.terminate()
            try:
                p.wait(timeout=2)
            except subprocess.TimeoutExpired:
                p.kill()
    print(f"[writewatch] END events≈{events}, bytes={size}, file={log}")
    return log, events

def main():
    ap = argparse.ArgumentParser(description="Dump .data repeatedly → derive heap ranges → run writewatch (cluster/anchor/seed)")
    ap.add_argument("--domain", required=True, help="Xen domain name or domid")
    ap.add_argument("--pid", type=int, required=True, help="Target PID")
    ap.add_argument("--module", required=True, help="Module (e.g., Overwatch.exe)")
    ap.add_argument("--ist-json", required=True, help="Kernel IST JSON (ntkrnlmp.json)")
    ap.add_argument("--python", default="python3", help="Python interpreter to run the dumper")
    ap.add_argument("--dumper", default=None, help="Filesystem path to vmi_dump_section.py (script mode)")
    ap.add_argument("--dumper-module", dest="dumper_module", default=None, help="Dotted module path for -m (e.g., dump.vmi_dump_section)")
    ap.add_argument("--project-root", dest="project_root", default=None, help="Add this dir to PYTHONPATH when running the dumper")
    ap.add_argument("--drakvuf", default="drakvuf", help="Path to drakvuf")
    ap.add_argument("--mode", choices=["cluster","anchor","seed"], default="cluster", help="Range generator mode")
    ap.add_argument("--watch", choices=["pfn","va"], default="pfn", help="Which writewatch plugin to use (default PFN)")
    ap.add_argument("--topk", type=int, default=2, help="Cluster: Top-K buckets to monitor per cycle")
    ap.add_argument("--gran-log2", type=int, default=22, help="Cluster: bucket granularity log2 bytes (default 22 = 4MB)")
    ap.add_argument("--gap-pages", type=int, default=64, help="Cluster: split segments when gap > this many pages")
    ap.add_argument("--pad", type=lambda x:int(x,0), default=0x10000, help="Cluster: padding bytes per side (default 64KB)")
    ap.add_argument("--anchors-per-cycle", type=int, default=16, help="Anchor: how many anchors per cycle")
    ap.add_argument("--anchor-pad-pages", type=int, default=1, help="Anchor: +/- pages around anchor")
    ap.add_argument("--seed-addr", action="append", default=[], help="Seed addresses (repeat). Format: 0xADDR or 0xADDR@PADPAGES")
    ap.add_argument("--seed-default-pad-pages", type=int, default=1, help="Default pad pages for seeds")
    ap.add_argument("--duration", type=int, default=10, help="Seconds to monitor each range")
    ap.add_argument("--interval", type=int, default=5, help="Seconds to wait before next cycle")
    ap.add_argument("--max-events", type=int, default=5000, help="Storm guard: stop range if event count exceeds this")
    ap.add_argument("--max-bytes", type=int, default=8_000_000, help="Storm guard: stop range if log bytes exceed this")
    ap.add_argument("--outdir", default="./out", help="Output directory for dumps/logs")
    ap.add_argument("--cap", default=None, help="Forwarded --cap for the dumper (hex string)")
    ap.add_argument("--max-pfns", type=int, default=4096, help="PFN mode: per range, cap number of PFNs to arm")
    args = ap.parse_args()

    outdir = Path(args.outdir)
    dumps = outdir / "dumps"
    logs = outdir / "logs"
    dumps.mkdir(parents=True, exist_ok=True)
    logs.mkdir(parents=True, exist_ok=True)

    domid = get_domid_from_name(args.domain) or args.domain

    # Prepare libvmi if PFN mode
    vmi = None
    if args.watch == "pfn":
        if Libvmi is None:
            raise SystemExit("python-libvmi not available but PFN watch requested")
        try:
            vmi = Libvmi(args.domain)
        except Exception as e:
            raise SystemExit(f"Libvmi init failed for domain '{args.domain}': {e}")

    cycle = 0
    while True:
        cycle += 1
        print(f"\n=== CYCLE {cycle} ===")

        if args.mode in ("cluster","anchor"):
            bin_path = dump_data_section(args.python, Path(args.dumper) if args.dumper else Path("vmi_dump_section.py"),
                                        args.domain, args.pid, args.module, args.ist_json,
                                        dumps, args.cap, use_module=args.dumper_module, project_root=args.project_root)
            print("[info] data dump:", bin_path)
            buf = Path(bin_path).read_bytes()
            heap_vals = []
            for off in range(0, len(buf) - 8 + 1, 8):
                val = int.from_bytes(buf[off:off+8], "little")
                if SEG_BEGIN <= val < SEG_END:
                    heap_vals.append(val)
            print(f"[info] heap-like candidates: {len(heap_vals)}")
            if not heap_vals and args.mode != "seed":
                print("[warn] no heap-like values found; sleeping")
                time.sleep(args.interval)
                continue
        else:
            heap_vals = []

        if args.mode == "cluster":
            ranges = cluster_ranges(heap_vals, topk=args.topk, pad=args.pad, gran_log2=args.gran_log2, gap_pages=args.gap_pages)
        elif args.mode == "anchor":
            ranges = build_anchor_ranges(heap_vals, anchors_per_cycle=args.anchors_per_cycle, pad_pages=args.anchor_pad_pages)
        else:
            ranges = build_seed_ranges(args.seed_addr, default_pad_pages=args.seed_default_pad_pages)

        if not ranges:
            print("[warn] no ranges to monitor; sleeping")
            time.sleep(args.interval)
            continue

        for i,(lo,hi) in enumerate(ranges, 1):
            size = hi - lo
            if size < 1024*1024:
                print(f"[range {i}/{len(ranges)}] VA 0x{lo:x}-0x{hi:x} (size={size//1024} KB)")
            else:
                print(f"[range {i}/{len(ranges)}] VA 0x{lo:x}-0x{hi:x} (size={size/(1024*1024):.2f} MB)")

        for (lo,hi) in ranges:
            if args.watch == "va":
                log, ev = run_writewatch_va(args.drakvuf, args.ist_json, domid, args.pid, lo, hi, args.duration, logs,
                                            max_events=args.max_events, max_bytes=args.max_bytes)
                print("[saved]", log, f"(events≈{ev})")
            else:
                pfns = vas_to_pfns(vmi, args.pid, lo, hi, max_pfns=args.max_pfns)
                if not pfns:
                    print(f"[warn] VA window 0x{lo:x}-0x{hi:x} produced no PFNs; skipping")
                    continue
                print(f"[info] VA→PFN: {len(pfns)} PFNs from window 0x{lo:x}-0x{hi:x}")
                log, ev = run_writewatch_pfn(args.drakvuf, args.ist_json, domid, pfns, args.duration, logs,
                                             max_events=args.max_events, max_bytes=args.max_bytes)
                print("[saved]", log, f"(events≈{ev})")

        print(f"[sleep] {args.interval}s ...")
        time.sleep(args.interval)

if __name__ == "__main__":
    main()
