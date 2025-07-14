## 메모리 정보를 알아보자

### 목표: 메모리 공부
작성자: kkongnyang2 작성일: 2025-07-06

---
### > 전체 메모리 보기

step 1. 실시간 사용량
```
~$ top
top - 16:45:55 up 38 min,  1 user,  load average: 0.89, 1.03, 0.93
Tasks: 284 total,   1 running, 283 sleeping,   0 stopped,   0 zombie
%Cpu(s):  3.5 us,  1.0 sy,  0.0 ni, 95.5 id,  0.0 wa,  0.0 hi,  0.0 si,  0.0 st
MiB Mem :   7742.6 total,   1928.0 free,   2904.1 used,   2910.5 buff/cache
MiB Swap:   2048.0 total,   2048.0 free,      0.0 used.   4062.3 avail Mem 

    PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND  
   3547 kkongny+  20   0 3008464 477472 118420 S  19.3   6.0   4:23.30 Isolate+ 
   1169 kkongny+  20   0 5301912 289408 135616 S   5.6   3.7   3:01.53 gnome-s+ 
   3448 kkongny+  20   0  367208  63532  51328 S   2.3   0.8   0:58.19 Utility+ 
   2589 kkongny+  20   0   11.8g 521776 242904 S   2.0   6.6   6:08.27 firefox  
   5196 kkongny+  20   0  594156  56948  43620 S   1.7   0.7   0:00.42 gnome-t+ 
    963 kkongny+   9 -11 1792844  27632  22000 S   1.0   0.3   0:19.37 pulseau+ 
    586 root      20   0    2816   1792   1792 S   0.7   0.0   0:01.57 acpid    
    427 root     -51   0       0      0      0 S   0.3   0.0   0:00.11 irq/173+ 
    517 systemd+  20   0   14836   6784   6016 S   0.3   0.1   0:02.96 systemd+ 
   4743 kkongny+  20   0 1161.6g 182960 128680 S   0.3   2.3   0:12.15 code     
   4900 kkongny+  20   0 1161.5g 183948  79488 S   0.3   2.3   0:40.57 code     
   5231 kkongny+  20   0   14532   4480   3584 R   0.3   0.1   0:00.04 top      
      1 root      20   0  166960  11584   8128 S   0.0   0.1   0:01.10 systemd  
      2 root      20   0       0      0      0 S   0.0   0.0   0:00.00 kthreadd 
      3 root      20   0       0      0      0 S   0.0   0.0   0:00.00 pool_wo+ 
      4 root       0 -20       0      0      0 I   0.0   0.0   0:00.00 kworker+ 
      5 root       0 -20       0      0      0 I   0.0   0.0   0:00.00 kworker+ 
```
step 2. 아예 프로그램처럼 보여주기
```
~$ htop
~$ F2로 숨길항목 조정
~$ F6 정렬을 MEM% 로
~$ F4 필터로 /usr/bin/qemu 특정 프로세스 계열만 확인 가능
~$ q로 끄기
```
step 3. 스냅샷/스크립트
```
~$ ps -eo pid,ppid,user,comm,rss,vsz --sort=-rss | head     #rss(실메모리) vsz(가상)
    PID    PPID USER     COMMAND           RSS    VSZ
   2589    1169 kkongny+ firefox         594364 12629332
   4884    4749 kkongny+ code            563836 1219649316
   3547    2589 kkongny+ Isolated Web Co 550156 3038512
   3455    2589 kkongny+ Isolated Web Co 347484 2922224
   1169     950 kkongny+ gnome-shell     297092 5567980
   1539     950 kkongny+ snap-store      209600 1045388
   4900    4743 kkongny+ code            187248 1217878856
   3946    2589 kkongny+ Isolated Web Co 187084 2635356
   4743     950 kkongny+ code            183576 1218076356
```
step 4. 전공자용 정확한 합계
```
~$ smem -r
```

### > 프로세스 레벨로 메모리 확인

step 1. 프로세스 정보 개요
```
~$ pmap -x 2589
2589:   /snap/firefox/6436/usr/lib/firefox/firefox
Address           Kbytes     RSS   Dirty Mode  Mapping
000001fa00500000    1024     684     684 rw---   [ anon ]
000002d1ed600000    1024     292     292 rw---   [ anon ]
000002f3c6200000    1024     144     144 rw---   [ anon ]
0000050269000000    1024    1024    1024 rw---   [ anon ]
0000050c95900000    1024      64      64 rw---   [ anon ]
0000056dbae00000    1024    1024    1024 rw---   [ anon ]
00000693c5200000    1024    1024    1024 rw---   [ anon ]
000006b766100000    1024     236     236 rw---   [ anon ]
```
* address: 매핑의 시작 가상주소
* kbytes: 매핑 전체 크기. 1024KB/4KB = 256 페이지
* RSS: 지금 실제 물리램에 있는 크기. Kbytes보다 작음
* Dirty: 해당 매핑에서 수정된 양. [anon] 익명이면 전부 원본없는 수정이기 때문에 RSS와 양이 똑같음. 그러나 디스크에서 ro만 한거면 0. 언제든 버려도 됨. 디스크에서 rw한거면 수정될때만 증가함.
* Mapping: 파일 경로

step 2. 원본
```
~$ cat /proc/2589/maps
1fa00500000-1fa00600000 rw-p 00000000 00:00 0                            [anon:js-gc-heap]
2d1ed600000-2d1ed700000 rw-p 00000000 00:00 0                            [anon:js-gc-heap]
2f3c6200000-2f3c6300000 rw-p 00000000 00:00 0                            [anon:js-gc-heap]
50269000000-50269100000 rw-p 00000000 00:00 0                            [anon:js-gc-heap]
```
가상주소 범위 별 자세한 정보. 권한, 파일 안에서의 오프셋, 디스크 major:minor, inode 번호, 출처 순이다.
이건 파일이 아니라 딱히 그런 정보가 없다.

step 3. 통계
```
~$ cat /proc/2589/smaps_rollup
1fa00500000-7ffc125cf000 ---p 00000000 00:00 0                           [rollup]
Rss:              745608 kB
Pss:              492933 kB
Pss_Dirty:        389358 kB
Pss_Anon:         377596 kB
Pss_File:         103506 kB
Pss_Shmem:         11830 kB
Shared_Clean:     302352 kB
Shared_Dirty:       9888 kB
Private_Clean:     48448 kB
Private_Dirty:    384920 kB
Referenced:       745608 kB
Anonymous:        377596 kB
KSM:                   0 kB
LazyFree:              0 kB
AnonHugePages:         0 kB
ShmemPmdMapped:        0 kB
FilePmdMapped:         0 kB
Shared_Hugetlb:        0 kB
Private_Hugetlb:       0 kB
Swap:                  0 kB
SwapPss:               0 kB
Locked:                0 kB
```
step 4. gdb
```
~$ sudo gdb -p 2589
(gdb) x/32xb 0x00001fa00500000      # 총 32개. 16진수 표현. 하나당 1B 단위.
0x1fa00500000:	0x00	0x00	0x00	0x00	0x00	0x00	0x00	0x00
0x1fa00500008:	0x00	0xc0	0xe2	0xd6	0xee	0x7f	0x00	0x00
0x1fa00500010:	0x01	0xff	0x00	0x00	0x00	0x00	0x00	0x00
0x1fa00500018:	0x00	0x00	0xb0	0x0a	0xf8	0x07	0x00	0x00
```
x = 메모리 확인 명령
x / [개수][형식][단위] 주소
이 의미를 알기 위해서는 (1) 리틀엔디안 재배열 (2) 그 값이 정수·포인터·길이·비트플래그 중 무엇인지
```
(gdb) x/4gx 0x1fa00500000        # 총 4개. 하나당 giant word(8B) 단위. 16진수 표현.
0x1fa00500000: 0x0000000000000000    ← 워드 0
0x1fa00500008: 0x00007feed6e2c000    ← 워드 1
0x1fa00500010: 0x000000000000ff01    ← 워드 2
0x1fa00500018: 0x00007f80ab000000    ← 워드 3
```
워드 1·3 은 48 bit 유저 공간 포인터처럼 생긴 값(0x0000 7f**…**) → 메모리 어딘가를 가리키는 주소일 가능성.
워드 2 (0xff01)는 길이+플래그 비트류 메타데이터 패턴과 비슷.

### > 커널 레벨 물리 메모리

step 1. 커널 모듈 목록
```
~$ sudo cat /proc/modules
ccm 20480 6 - Live 0xffffffffc1408000
rfcomm 102400 4 - Live 0xffffffffc1be7000
snd_ctl_led 24576 0 - Live 0xffffffffc145d000
ledtrig_audio 12288 1 snd_ctl_led, Live 0xffffffffc1453000
snd_soc_skl_hda_dsp 24576 7 - Live 0xffffffffc1445000
snd_soc_hdac_hdmi 45056 1 snd_soc_skl_hda_dsp, Live 0xffffffffc1434000
```
step 2. 슬랩 캐시(inode, task_struct 같은 고정 크기 객체마다 전용 캐시)
```
~$ sudo cat /proc/slabinfo | head
[sudo] password for kkongnyang2: 
slabinfo - version: 2.1
# name            <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab> : tunables <limit> <batchcount> <sharedfactor> : slabdata <active_slabs> <num_slabs> <sharedavail>
kvm_async_pf           0      0    136   30    1 : tunables    0    0    0 : slabdata      0      0      0
kvm_vcpu               0      0   9152    3    8 : tunables    0    0    0 : slabdata      0      0      0
kvm_mmu_page_header      0      0    184   22    1 : tunables    0    0    0 : slabdata      0      0      0
x86_emulator           0      0   2656   12    8 : tunables    0    0    0 : slabdata      0      0      0
i915_dependency      256    256    128   32    1 : tunables    0    0    0 : slabdata      8      8      0
execute_cb             0      0     64   64    1 : tunables    0    0    0 : slabdata      0      0      0
i915_request         530    552    704   23    4 : tunables    0    0    0 : slabdata     24     24      0
drm_i915_gem_object    753    896   1152   28    8 : tunables    0    0    0 : slabdata     32     32      0
```
step 3. vmalloc 영역 (kmalloc은 물리 연속 할당, vmalloc은 가상 연속 할당)
```
~$ sudo cat /proc/vmallocinfo
0xffffbcba80000000-0xffffbcba80005000   20480 irq_init_percpu_irqstack+0x114/0x1b0 vmap
0xffffbcba80005000-0xffffbcba80007000    8192 x86_acpi_os_ioremap+0xe/0x20 phys=0x0000000084189000 ioremap
0xffffbcba80007000-0xffffbcba80009000    8192 x86_acpi_os_ioremap+0xe/0x20 phys=0x0000000084292000 ioremap
```

step 4. 물리 메모리 맵
```
~$ sudo cat /proc/iomem
00000000-00000fff : Reserved
00001000-0009efff : System RAM
0009f000-000fffff : Reserved
  000a0000-000bffff : PCI Bus 0000:00
  000e0000-000fffff : PCI Bus 0000:00
    000f0000-000fffff : System ROM
00100000-793fafff : System RAM
  54200000-557fffff : Kernel code
  55800000-5659ffff : Kernel rodata
  56600000-56a54eff : Kernel data
  56f62000-573fffff : Kernel bss
793fb000-79415fff : Reserved
79416000-7ade8fff : System RAM
7ade9000-7ade9fff : Reserved
7adea000-7f239fff : System RAM
7f23a000-84063fff : Reserved
84064000-84193fff : ACPI Tables
84194000-84293fff : ACPI Non-volatile Storage
  84293298-84293397 : USBC000:00
84294000-8554dfff : Reserved
8554e000-8554efff : System RAM
8554f000-897fffff : Reserved
  87800000-897fffff : Graphics Stolen Memory
89800000-dfffffff : PCI Bus 0000:00
  8c000000-a20fffff : PCI Bus 0000:01
    8c000000-a20fffff : PCI Bus 0000:02
      8c000000-a1efffff : PCI Bus 0000:04
      a1f00000-a1ffffff : PCI Bus 0000:39
        a1f00000-a1f0ffff : 0000:39:00.0
          a1f00000-a1f0ffff : xhci-hcd
      a2000000-a20fffff : PCI Bus 0000:03
        a2000000-a203ffff : 0000:03:00.0
          a2000000-a203ffff : thunderbolt
        a2040000-a2040fff : 0000:03:00.0
  a2200000-a22fffff : PCI Bus 0000:3a
    a2200000-a2203fff : 0000:3a:00.0
      a2200000-a2203fff : nvme
e0000000-efffffff : PCI ECAM 0000 [bus 00-ff]
  e0000000-efffffff : pnp 00:07
fc800000-fe7fffff : PCI Bus 0000:00
  fd000000-fd69ffff : pnp 00:05
  fd6a0000-fd6affff : INT34BB:00
    fd6a0000-fd6affff : INT34BB:00 INT34BB:00
  fd6b0000-fd6cffff : pnp 00:05
  fd6d0000-fd6dffff : INT34BB:00
    fd6d0000-fd6dffff : INT34BB:00 INT34BB:00
  fd6e0000-fd6effff : INT34BB:00
    fd6e0000-fd6effff : INT34BB:00 INT34BB:00
  fd6f0000-fdffffff : pnp 00:05
  fe000000-fe010fff : Reserved
    fe010000-fe010fff : 0000:00:1f.5
      fe010000-fe010fff : 0000:00:1f.5 0000:00:1f.5
  fe038000-fe038fff : pnp 00:09
  fe200000-fe7fffff : pnp 00:05
fec00000-fec00fff : Reserved
  fec00000-fec003ff : IOAPIC 0
fed00000-fed03fff : Reserved
  fed00000-fed003ff : HPET 0
    fed00000-fed003ff : PNP0103:00
fed10000-fed17fff : pnp 00:07
fed18000-fed18fff : pnp 00:07
fed19000-fed19fff : pnp 00:07
fed20000-fed3ffff : pnp 00:07
fed40000-fed44fff : MSFT0101:00
  fed40000-fed44fff : MSFT0101:00
fed45000-fed8ffff : pnp 00:07
fed90000-fed90fff : dmar0
fed91000-fed91fff : dmar1
fee00000-fee00fff : Reserved
ff000000-ffffffff : pnp 00:05
100000000-2747fffff : System RAM
274800000-277ffffff : RAM buffer
4000000000-7fffffffff : PCI Bus 0000:00
  4000000000-400fffffff : 0000:00:02.0
  4010000000-4010000fff : 0000:00:15.0
    4010000000-40100001ff : lpss_dev
      4010000000-40100001ff : i2c_designware.0 lpss_dev
    4010000200-40100002ff : lpss_priv
    4010000800-4010000fff : idma64.0
      4010000800-4010000fff : idma64.0 idma64.0
  6000000000-6021ffffff : PCI Bus 0000:01
    6000000000-6021ffffff : PCI Bus 0000:02
      6000000000-6021ffffff : PCI Bus 0000:04
  6022000000-6022ffffff : 0000:00:02.0
  6023000000-60230fffff : 0000:00:1f.3
    6023000000-60230fffff : Audio DSP
  6023100000-602310ffff : 0000:00:14.0
    6023100000-602310ffff : xhci-hcd
  6023110000-6023117fff : 0000:00:04.0
    6023110000-6023117fff : proc_thermal
  6023118000-602311bfff : 0000:00:1f.3
    6023118000-602311bfff : Audio DSP
  602311c000-602311ffff : 0000:00:14.3
    602311c000-602311ffff : iwlwifi
  6023120000-6023121fff : 0000:00:14.2
  6023122000-60231220ff : 0000:00:1f.4
  6023123000-6023123fff : 0000:00:16.0
    6023123000-6023123fff : mei_me
  6023125000-6023125fff : 0000:00:14.2
  6023126000-6023126fff : 0000:00:12.0
    6023126000-6023126fff : Intel PCH thermal driver
```

### > 물리 덤프

RAM 그 자체를 파일로 보관

A. Crash dump (패닉 시 자동 vmcore)
```
~$ sudo apt install linux-crashdump          # kdump-tools + makedumpfile
~$ sudo systemctl enable --now kdump         # 서비스 활성화
~$ sudo kdump-config show                    # crashkernel=X 확인. 없으면 /etc/default/grub.d/kdump-tools.cfg 추가해야함
~$ echo 1 | sudo tee /proc/sys/kernel/sysrq
~$ echo c | sudo tee /proc/sysrq-trigger     # VM에서만! 즉시 패닉
재부팅 하면 /var/crash/$(date +%Y%)/vmcore 가 있음.
~$ sudo apt install crash
~$ sudo crash /usr/lib/debug/boot/vmlinux-$(uname -r) /var/crash/*/vmcore
crash> bt          # 패닉 스택
crash> ps          # 전 프로세스 상태
```

B. Live dump (LiME으로 메모리 구조 연구)
```
sudo apt install build-essential linux-headers-$(uname -r)
git clone https://github.com/504ensicsLabs/LiME.git
cd LiME/src
make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
# LiME은 format=raw|lime|padded 및 net=<host:port>(SSH 대신 UDP 스트림) 지원. 
sudo insmod lime.ko "path=/tmp/memdump.lime format=lime"
ls -lh /tmp/memdump.lime    # 전체 RAM 크기만큼
git clone https://github.com/volatilityfoundation/volatility3
python3 -m pip install -r volatility3/requirements.txt
python3 volatility3/vol.py -f /tmp/memdump.lime windows.info     # 예시 플러그인
```

C. /proc/kcore (커널 전체를 gdb로)
```
sudo sysctl -w kernel.kptr_restrict=0   # 주소 마스킹 해제. 일반인에게도 커널 주소를 그대로 노출
sudo gdb /usr/lib/debug/vmlinux-$(uname -r) /proc/kcore
(gdb) info mem
(gdb) x/16gx 0xffffffffff600000        # vdso 예시
```
시스템 RAM + I/O 공간을 ELF64로 노출하며 크기가 RAM 크기와 동일

D. /dev/mem (물리 주소 직접)
최신 커널은 config_strict_devmem=y로 1MB 이후 영역 접근을 차단한다. 따라서 부트시 iomem=relaxed를 grub 커널 라인에 추가하거나, 커스텀 커널에서 strict_devmem=n으로 빌드해야한다. 
```
sudo dd if=/dev/mem of=efi_fw.bin bs=1M skip=0 count=1
hexdump -C efi_fw.bin | head
```

E. 가상머신 메모리 덤프
```
sudo virsh dump --memory-only --live vm1 /var/tmp/vm1.dump      # 게스트 중단 없이
혹은
(qemu) dump-guest-memory /tmp/vm1.dump                            # QEMU monitor
sudo crash vmlinux-guest /var/tmp/vm1.dump
```

주의사항.
kernel.randomize_va_space, kptr_restrict, dmesg_restrict를 풀면 편하나, 반드시 실험용에서만.
프로세스가 실행중인 메모리를 읽으면 멈춤(HALT)은 없지만, 쓰기는 데이터 손상 위험

kerenl.randomize_va_space
2 = 풀 랜덤화
1 = 보수적
0 = ASLR off

kernel.kptr_restrict
2 = root 포함 전원 마스킹
1 = 일반 사용자 마스킹
0 = 아무나 풀 주소 보여주기

kernel.demsg_restrict
1 = root/CAP_SYSLOG만
0 = 모두 허용          # 커널 로그에 메모리 주소나 토큰이 찍힐 수 있음