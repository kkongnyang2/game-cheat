## 실시간 할당 후보군을 관찰해보자

작성자: kkongnyang2 작성일: 2025-09-16

---
### xen 페이지 잡기

게스트 코드  ──(HP 갱신)──► [가상주소 GVA] → 페이지 테이블 → [게스트물리 GPA]
                                           │
                              하이퍼바이저의 2단 변환(EPT/NPT)
                                           ↓
             EPT 권한 비트(R/W/X)로 페이지를 의도적으로 막아둠
                                           ↓
                 접근 발생 → EPT violation → VM-exit(호스트로)
                                           ↓
   VM-exit 정보: 접근종류(R/W/X), GLA(선택적), GPA, RIP, CR3(DTB) 등
                                           ↓
  분석기(예: LibVMI/Drakvuf)가 RIP 주변 명령 디코드 → 쓰기 크기/대상 파악
                                           ↓
            (옵션) 일시 허용+단일 스텝 후 재보호(re-arm)로 계속 추적


실시간 vmi 환경에서 할당되는 페이지를 훅킹해 VM-exit 트랩으로 감시


### drakvuf 설치

drakvuf 설치
```
ubuntu on xen으로 들어가서
$ sudo apt install meson ninja-build clang llvm lld libglib2.0-dev libjson-c-dev
$ git clone --recursive https://github.com/tklengyel/drakvuf
$ cd drakvuf
$ meson setup build --native-file llvm.ini
$ ninja -C build
$ sudo ninja -C build install
$ sudo ldconfig
```

기능 확인
```
$ sudo xl list        # 몇번인지 확인
$ sudo drakvuf -r /root/symbols/ntkrnlmp.json -d 1 -a procmon
```

### drakvuf 커스텀 플러그인 제작

데이터를 서버에서 받아오면 ntdll.dll이 비어있는 주소에 힙 할당을 하고 그 주소를 리턴해준다. 그러면 overwatch.exe는 그걸 테이블에 저장해뒀다가 필요할때마다 찾아가서 read & write를 해준다. 

그럼 이 할당을 어떻게 잡아올 수 있냐?
윈도우 내부에서는 커널의 할당 함수들을 쫒는다. vmi 환경에서는 특정 구역들에 vm-exit 걸어놓아 거기에 write하려하면 무조건 예외처리로 확인된다. 이 방식이 drakvuf에 구현되어 있다.

drakvuf의 syscall + memaccess를 이용해 drakvuf 커스텀 플러그인으로 해주자.
1. syscalls 후킹: NtAllocateVirtualMemory, NtMapViewOfsection
2. 리턴 성공 시 *BaseAddress, *RegionSize 읽어서 [base,size) 구간 확보
3. VA 필터 : (base..base+size)가 0x 0000 0200 0000 0000 ~ 0x 0000 02FF FFFF FFFF와 교차하면 대상 처리
4. 페이지별 W-트랩 등록: 해당 구간을 4KB 페이지로 쪼개 memaccess 등록
5. memaccess 콜백: (pid, tid, rip, gva, gfn)로깅 -> 싱글스텝 1회 허용 -> 다시 W 금지
6. free/unmap 시 : 추적 테이블에서 트랩 해제

다만 처음에 이 방법으로 시도했다가, 힙 할당된 va에 값이 변화하는게 CE에서 보임에도 여기서는 va write를 못잡아옴. 일반 read에서는 읽어오는 걸 보면 프로세스가 아닌 커널 측에서 해당 gpa에 write 하는 걸로 추정. 즉 결과 write는 읽을 수 있지만 실시간 망에는 포착되지 않는다는 뜻. va를 gpa로 번역한 후 write를 잡아오도록 고친다.

drakvuf/
└─ src/
   └─ plugins/
      ├─ meson.build              ← 여기에 한 줄 추가
      └─ pfn_writewatch/
         ├─ pfn_writewatch.cpp ← 새로 추가
         └─ pfn_writewatch.h   ← 새로 추가

커스텀 플러그인 제작
```
$ cd ~/drakvuf/src/plugins
$ mkdir pfn_writewatch
$ nano pfn_writewatch/pfn_writewatch.h
$ nano pfn_writewatch/pfn_writewatch.cpp
$ nano meson.build
plugin_sources = [
  # (기존들)
  'pfn_writewatch/pfn_writewatch.cpp',
  'pfn_writewatch/pfn_writewatch.h',
]
$ nano plugins.h
플러그인 리스트 drakvuf_plugin_t 의 마지막 __DRAKVUF_PLUGIN_LIST_MAX 앞줄에
    PLUGIN_PFN_WRITEWATCH,
이름 리스트 static const char* drakvuf_plugin_names[] 에
    [PLUGIN_PFN_WRITEWATCH] = "pfn_writewatch",
os 지원 리스트 drakvuf_plugin_os_support 에
    [PLUGIN_PFN_WRITEWATCH] = { [VMI_OS_WINDOWS] = 1, [VMI_OS_LINUX] = 1 },
$ nano plugins.cpp
#include "pfn_writewatch/pfn_writewatch.h"
drakvuf_plugins::start 안의 큰 switch에 case 추가
case PLUGIN_PFN_WRITEWATCH:
{
    this->plugins[plugin_id] = std::make_unique<pfn_writewatch>(this->drakvuf, this->output);
    break;
}
```

재빌드
```
$ cd ~/drakvuf
$ meson setup build --reconfigure
$ ninja -C build
$ sudo ninja -C build install
$ sudo ldconfig
```

### watch_data_ranges

실시간으로 .data 섹션을 덤프하여 heap 포인터로 추정되는 candidate를 전부 찾고 상위 클러스터 범위 다섯개를 추려 아까 만든 drakvuf 플러그인으로 감시하는 코드
.data 덤프 + candidate 찾기 + 상위 클러스터 만들기 + pfn 번역 + writewatch + 내용 및 델타 바이터 확인

실행 커맨드
```
$ sudo vmi-process-list win10 | grep -i overwatch
$ cd ~/game-cheat/code
$ sudo /home/kkongnyang2/vmi-venv/bin/python3 -m dump.watch_data_ranges \
  --domain win10 --pid 2900 --module Overwatch.exe \
  --ist-json /root/symbols/ntkrnlmp.json \
  --python /home/kkongnyang2/vmi-venv/bin/python3 \
  --dumper-module dump.vmi_dump_section \
  --project-root ~/game-cheat/code \
  --drakvuf /usr/local/bin/drakvuf \
  --watch pfn \
  --mode cluster --topk 5 --gran-log2 22 --gap-pages 64 --pad 0x10000 \
  --duration 10 --interval 5 \
  --ww-mode both --peek-bytes 16 --max-changes 12 \
  --outdir /home/kkongnyang2/watch_dump --max-pfns 2048

=== CYCLE 1 ===
[dump] running: /home/kkongnyang2/vmi-venv/bin/python3 -m dump.vmi_dump_section win10 10068 Overwatch.exe /root/symbols/ntkrnlmp.json .data
[dumped] Overwatch.exe_.data_0x7ff7af882000_0x2ef800.bin  (size=3078144/3078144)
Invalid offset name in windows_get_offset (win_peb).

[info] data dump: /home/kkongnyang2/watch_dump/dumps/Overwatch.exe_.data_0x7ff7af882000_0x2ef800.bin
[info] heap-like candidates: 1097
[range 1/13] VA 0x1ffffff0000-0x20000091000 (size=644 KB)
[range 2/13] VA 0x20f19070000-0x20f190b2000 (size=264 KB)
[range 3/13] VA 0x20f191b0000-0x20f191d3000 (size=140 KB)
[range 4/13] VA 0x20f192b0000-0x20f1938d000 (size=884 KB)
[range 5/13] VA 0x20f20fd6000-0x20f21055000 (size=508 KB)
[range 6/13] VA 0x20f210b8000-0x20f210dc000 (size=144 KB)
[range 7/13] VA 0x20f21123000-0x20f21145000 (size=136 KB)
[range 8/13] VA 0x20f2117a000-0x20f211dc000 (size=392 KB)
[range 9/13] VA 0x20f211fd000-0x20f2121e000 (size=132 KB)
[range 10/13] VA 0x20f21248000-0x20f21269000 (size=132 KB)
[range 11/13] VA 0x20f2132f000-0x20f21350000 (size=132 KB)
[range 12/13] VA 0x20f213ca000-0x20f213f4000 (size=168 KB)
[range 13/13] VA 0x20f3b7f0000-0x20f3b81d000 (size=180 KB)
[warn] VA window 0x1ffffff0000-0x20000091000 produced no PFNs; skipping
[info] VA→PFN: 35 PFNs from window 0x20f19070000-0x20f190b2000
[writewatch/PFN] START /usr/local/bin/drakvuf -r /root/symbols/ntkrnlmp.json -d 4 -a pfn_writewatch -o json → /home/kkongnyang2/watch_dump/logs/pfnwatch_35pfns_20250916T035457Z.jsonl (PFNs=35, max_events=5000, max_bytes=8000000)
[writewatch] timeout reached; terminating
[writewatch] END events≈0, bytes=112, file=/home/kkongnyang2/watch_dump/logs/pfnwatch_35pfns_20250916T035457Z.jsonl
[saved] /home/kkongnyang2/watch_dump/logs/pfnwatch_35pfns_20250916T035457Z.jsonl (events≈0)
[info] VA→PFN: 20 PFNs from window 0x20f191b0000-0x20f191d3000
[writewatch/PFN] START /usr/local/bin/drakvuf -r /root/symbols/ntkrnlmp.json -d 4 -a pfn_writewatch -o json → /home/kkongnyang2/watch_dump/logs/pfnwatch_20pfns_20250916T035508Z.jsonl (PFNs=20, max_events=5000, max_bytes=8000000)
[writewatch] timeout reached; terminating
[writewatch] END events≈2779, bytes=302303, file=/home/kkongnyang2/watch_dump/logs/pfnwatch_20pfns_20250916T035508Z.jsonl
[saved] /home/kkongnyang2/watch_dump/logs/pfnwatch_20pfns_20250916T035508Z.jsonl (events≈2779)
[info] VA→PFN: 206 PFNs from window 0x20f192b0000-0x20f1938d000
[writewatch/PFN] START /usr/local/bin/drakvuf -r /root/symbols/ntkrnlmp.json -d 4 -a pfn_writewatch -o json → /home/kkongnyang2/watch_dump/logs/pfnwatch_206pfns_20250916T035518Z.jsonl (PFNs=206, max_events=5000, max_bytes=8000000)
[writewatch] storm guard hit (events=5000, bytes=545111); terminating
[writewatch] END events≈5000, bytes=545111, file=/home/kkongnyang2/watch_dump/logs/pfnwatch_206pfns_20250916T035518Z.jsonl
[saved] /home/kkongnyang2/watch_dump/logs/pfnwatch_206pfns_20250916T035518Z.jsonl (events≈5000)
[info] VA→PFN: 117 PFNs from window 0x20f20fd6000-0x20f21055000
[writewatch/PFN] START /usr/local/bin/drakvuf -r /root/symbols/ntkrnlmp.json -d 4 -a pfn_writewatch -o json → /home/kkongnyang2/watch_dump/logs/pfnwatch_117pfns_20250916T035518Z.jsonl (PFNs=117, max_events=5000, max_bytes=8000000)
[writewatch] storm guard hit (events=5000, bytes=545109); terminating
[writewatch] END events≈5000, bytes=545109, file=/home/kkongnyang2/watch_dump/logs/pfnwatch_117pfns_20250916T035518Z.jsonl
[saved] /home/kkongnyang2/watch_dump/logs/pfnwatch_117pfns_20250916T035518Z.jsonl (events≈5000)
[info] VA→PFN: 35 PFNs from window 0x20f210b8000-0x20f210dc000
[writewatch/PFN] START /usr/local/bin/drakvuf -r /root/symbols/ntkrnlmp.json -d 4 -a pfn_writewatch -o json → /home/kkongnyang2/watch_dump/logs/pfnwatch_35pfns_20250916T035519Z.jsonl (PFNs=35, max_events=5000, max_bytes=8000000)
[writewatch] storm guard hit (events=5000, bytes=544934); terminating
[writewatch] END events≈5000, bytes=544934, file=/home/kkongnyang2/watch_dump/logs/pfnwatch_35pfns_20250916T035519Z.jsonl
[saved] /home/kkongnyang2/watch_dump/logs/pfnwatch_35pfns_20250916T035519Z.jsonl (events≈5000)
[info] VA→PFN: 34 PFNs from window 0x20f21123000-0x20f21145000
[writewatch/PFN] START /usr/local/bin/drakvuf -r /root/symbols/ntkrnlmp.json -d 4 -a pfn_writewatch -o json → /home/kkongnyang2/watch_dump/logs/pfnwatch_34pfns_20250916T035519Z.jsonl (PFNs=34, max_events=5000, max_bytes=8000000)
[writewatch] storm guard hit (events=5000, bytes=543016); terminating
[writewatch] END events≈5000, bytes=543016, file=/home/kkongnyang2/watch_dump/logs/pfnwatch_34pfns_20250916T035519Z.jsonl
[saved] /home/kkongnyang2/watch_dump/logs/pfnwatch_34pfns_20250916T035519Z.jsonl (events≈5000)
[info] VA→PFN: 95 PFNs from window 0x20f2117a000-0x20f211dc000
[writewatch/PFN] START /usr/local/bin/drakvuf -r /root/symbols/ntkrnlmp.json -d 4 -a pfn_writewatch -o json → /home/kkongnyang2/watch_dump/logs/pfnwatch_95pfns_20250916T035521Z.jsonl (PFNs=95, max_events=5000, max_bytes=8000000)
[writewatch] storm guard hit (events=5000, bytes=541629); terminating
[writewatch] END events≈5000, bytes=541629, file=/home/kkongnyang2/watch_dump/logs/pfnwatch_95pfns_20250916T035521Z.jsonl
[saved] /home/kkongnyang2/watch_dump/logs/pfnwatch_95pfns_20250916T035521Z.jsonl (events≈5000)
[info] VA→PFN: 33 PFNs from window 0x20f211fd000-0x20f2121e000
[writewatch/PFN] START /usr/local/bin/drakvuf -r /root/symbols/ntkrnlmp.json -d 4 -a pfn_writewatch -o json → /home/kkongnyang2/watch_dump/logs/pfnwatch_33pfns_20250916T035521Z.jsonl (PFNs=33, max_events=5000, max_bytes=8000000)
[writewatch] timeout reached; terminating
[writewatch] END events≈0, bytes=112, file=/home/kkongnyang2/watch_dump/logs/pfnwatch_33pfns_20250916T035521Z.jsonl
[saved] /home/kkongnyang2/watch_dump/logs/pfnwatch_33pfns_20250916T035521Z.jsonl (events≈0)
[info] VA→PFN: 33 PFNs from window 0x20f21248000-0x20f21269000
[writewatch/PFN] START /usr/local/bin/drakvuf -r /root/symbols/ntkrnlmp.json -d 4 -a pfn_writewatch -o json → /home/kkongnyang2/watch_dump/logs/pfnwatch_33pfns_20250916T035532Z.jsonl (PFNs=33, max_events=5000, max_bytes=8000000)
[writewatch] timeout reached; terminating
[writewatch] END events≈0, bytes=112, file=/home/kkongnyang2/watch_dump/logs/pfnwatch_33pfns_20250916T035532Z.jsonl
[saved] /home/kkongnyang2/watch_dump/logs/pfnwatch_33pfns_20250916T035532Z.jsonl (events≈0)
[info] VA→PFN: 33 PFNs from window 0x20f2132f000-0x20f21350000
[writewatch/PFN] START /usr/local/bin/drakvuf -r /root/symbols/ntkrnlmp.json -d 4 -a pfn_writewatch -o json → /home/kkongnyang2/watch_dump/logs/pfnwatch_33pfns_20250916T035542Z.jsonl (PFNs=33, max_events=5000, max_bytes=8000000)
[writewatch] timeout reached; terminating
[writewatch] END events≈4957, bytes=540425, file=/home/kkongnyang2/watch_dump/logs/pfnwatch_33pfns_20250916T035542Z.jsonl
[saved] /home/kkongnyang2/watch_dump/logs/pfnwatch_33pfns_20250916T035542Z.jsonl (events≈4957)
[info] VA→PFN: 42 PFNs from window 0x20f213ca000-0x20f213f4000
[writewatch/PFN] START /usr/local/bin/drakvuf -r /root/symbols/ntkrnlmp.json -d 4 -a pfn_writewatch -o json → /home/kkongnyang2/watch_dump/logs/pfnwatch_42pfns_20250916T035552Z.jsonl (PFNs=42, max_events=5000, max_bytes=8000000)
[writewatch] storm guard hit (events=5000, bytes=545112); terminating
[writewatch] END events≈5000, bytes=545112, file=/home/kkongnyang2/watch_dump/logs/pfnwatch_42pfns_20250916T035552Z.jsonl
[saved] /home/kkongnyang2/watch_dump/logs/pfnwatch_42pfns_20250916T035552Z.jsonl (events≈5000)
[info] VA→PFN: 29 PFNs from window 0x20f3b7f0000-0x20f3b81d000
[writewatch/PFN] START /usr/local/bin/drakvuf -r /root/symbols/ntkrnlmp.json -d 4 -a pfn_writewatch -o json → /home/kkongnyang2/watch_dump/logs/pfnwatch_29pfns_20250916T035553Z.jsonl (PFNs=29, max_events=5000, max_bytes=8000000)
[writewatch] storm guard hit (events=5000, bytes=545076); terminating
[writewatch] END events≈5000, bytes=545076, file=/home/kkongnyang2/watch_dump/logs/pfnwatch_29pfns_20250916T035553Z.jsonl
[saved] /home/kkongnyang2/watch_dump/logs/pfnwatch_29pfns_20250916T035553Z.jsonl (events≈5000)
[sleep] 5s ...
```


### 잡기 실패

약간 범위가 빗나감. 난독화 때문에 .data에 있는 포인터들이 부정확 하거나, 힙에 포인터가 있는 거 같음.