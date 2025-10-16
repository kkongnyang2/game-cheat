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


### pid_va_writewatch

drakvuf/
└─ src/
   └─ plugins/
      ├─ meson.build              ← 여기에 한 줄 추가
      └─ pid_va_writewatch/
         ├─ pid_va_writewatch.cpp ← 새로 추가
         └─ pid_va_writewatch.h   ← 새로 추가

커스텀 플러그인 제작
```
$ cd ~/drakvuf/src/plugins
$ mkdir pid_va_writewatch
$ nano pid_va_writewatch/pid_va_writewatch.h
$ nano pid_va_writewatch/pid_va_writewatch.cpp
$ nano meson.build
plugin_sources = [
  # (기존들)
  'pid_va_writewatch/pid_va_writewatch.cpp',
  'pid_va_writewatch/pid_va_writewatch.h',
]
$ nano plugins.h
플러그인 리스트 drakvuf_plugin_t 의 마지막 __DRAKVUF_PLUGIN_LIST_MAX 앞줄에
    PLUGIN_PID_VA_WRITEWATCH,
이름 리스트 static const char* drakvuf_plugin_names[] 에
    [PLUGIN_PID_VA_WRITEWATCH] = "pid_va_writewatch",
os 지원 리스트 drakvuf_plugin_os_support 에
    [PLUGIN_PID_VA_WRITEWATCH] = { [VMI_OS_WINDOWS] = 1, [VMI_OS_LINUX] = 1 },
$ nano plugins.cpp
#include "pid_va_writewatch/pid_va_writewatch.h"
drakvuf_plugins::start 안의 큰 switch에 case 추가
case PLUGIN_PID_VA_WRITEWATCH:
{
    this->plugins[plugin_id] = std::make_unique<pid_va_writewatch>(this->drakvuf, this->output);
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

커맨드
```
sudo env WRITEWATCH_PID=4972 \
         WRITEWATCH_RANGE=0x0000020000000000-0x000002FFFFFFFFFF \
  drakvuf -r /root/symbols/ntkrnlmp.json -d 1 -a pid_va_writewatch
```

### pfn_writewatch

힙 할당된 va에 값이 변화하는게 CE에서 보임에도 여기서는 같은 seed의 va write를 못잡아옴. 일반 read에서는 읽어오는 걸 보면 프로세스가 아닌 다른 곳에서 해당 gpa에 write 하는 걸로 추정. 즉 결과 write는 읽을 수 있지만 실시간 망에는 포착되지 않는다는 뜻. va를 gpa로 번역한 후 write를 잡아오도록 고친다.

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

커맨드
```
export WRITEWATCH_PFN_RANGE=0x12abc-0x12ac0
export WRITEWATCH_SECONDS=5
drakvuf -r /root/symbols/ntkrnlmp.json -d <domid> -a pfn_writewatch -o json
```

### 상위 클러스터 감시

watch_data_ranges.py

실시간으로 .data 섹션을 덤프하여 heap 포인터로 추정되는 candidate를 전부 찾고 상위 클러스터 범위 다섯개를 추려 pfn_writewatch 플러그인으로 감시하는 코드
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
```
약간 범위가 빗나감. 난독화 때문에 .data에 있는 포인터들이 부정확 하거나, 힙에 포인터가 있는 거 같음.