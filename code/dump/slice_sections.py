# slice_sections.py
# usage: python3 slice_sections.py /path/to/dump_dir/manifest.json
import json, os, tarfile, sys
def pack(sec_name, start, size, pranges, dump_dir):
    end = start + size
    out = os.path.join(dump_dir, f"{sec_name.strip('.')}.tar")
    with tarfile.open(out, "w") as tf:
        added=0
        for r in pranges:
            s,l = r["start"], r["length"]
            if s >= start and s < end:
                p = os.path.join(dump_dir, r["file"])
                if os.path.exists(p):
                    tf.add(p, arcname=os.path.join("need", r["file"]))
                    added += 1
    print(f"[{sec_name}] packed {added} chunks -> {out}")

if __name__ == "__main__":
    manp = sys.argv[1]
    dump_dir = os.path.dirname(os.path.abspath(manp))
    man = json.load(open(manp))
    secs = man["module"]["sections"]
    pr = man["present_ranges"]
    for sec in [".text",".rdata",".data"]:
        s = secs[sec]["va_start"]; z = secs[sec]["vsize"]
        pack(sec, s, z, pr, dump_dir)
    print("[done]")
