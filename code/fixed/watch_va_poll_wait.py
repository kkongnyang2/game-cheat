#!/usr/bin/env python3
import sys, time, signal
from datetime import datetime
from libvmi import Libvmi

def hexdump(b, width=16):
    out=[]
    for i in range(0, len(b), width):
        chunk=b[i:i+width]
        hexs=' '.join(f'{x:02x}' for x in chunk)
        text=''.join(chr(x) if 32<=x<127 else '.' for x in chunk)
        out.append(f'{i:08x}  {hexs:<{width*3}}  {text}')
    return '\n'.join(out)

def try_decode(b):
    for enc in ('utf-16le','utf-8','cp1252'):
        try:
            return enc, b.decode(enc, errors='strict')
        except Exception:
            pass
    return None, None

if len(sys.argv) < 5:
    print("Usage: sudo ~/vmi-venv/bin/python3 watch_va_poll_wait.py <domain> <pid> <vaddr_hex> <length> [interval=0.05]")
    sys.exit(1)

dom   = sys.argv[1]
pid   = int(sys.argv[2])
vaddr = int(sys.argv[3], 16)
length= int(sys.argv[4])
interval = float(sys.argv[5]) if len(sys.argv)>5 else 0.05

vmi = Libvmi(dom)

stop = {'f': False}
def bye(sig, frm): stop['f'] = True
signal.signal(signal.SIGINT, bye)
signal.signal(signal.SIGTERM, bye)

# 1) 메모리에 올라올 때까지 대기 (읽기 성공할 때까지 루프)
print(f"[.] Waiting for page to become resident... (pid={pid}, va=0x{vaddr:x})")
buf = None
while not stop['f']:
    try:
        data, n = vmi.read_va(vaddr, pid, length)
        buf = bytes(data)
        break
    except Exception:
        time.sleep(0.05)

if stop['f']:
    vmi.destroy(); sys.exit(0)

enc, text = try_decode(buf)
now = datetime.now().strftime('%H:%M:%S.%f')[:-3]
print(f"[{now}] INIT len={len(buf)}")
if enc and text is not None:
    print(f"  decode({enc}): {repr(text)}")
print(hexdump(buf))

# 2) 변경 감시
last = buf
while not stop['f']:
    try:
        data, n = vmi.read_va(vaddr, pid, length)
        cur = bytes(data)
        if cur != last:
            now = datetime.now().strftime('%H:%M:%S.%f')[:-3]
            enc, text = try_decode(cur)
            print(f"[{now}] CHANGE len={len(cur)}")
            if enc and text is not None:
                print(f"  decode({enc}): {repr(text)}")
            print(hexdump(cur))
            last = cur
        time.sleep(interval)
    except Exception:
        # 일시적으로 다시 페이징 아웃되면 잠깐 기다렸다 재시도
        time.sleep(0.1)

vmi.destroy()
