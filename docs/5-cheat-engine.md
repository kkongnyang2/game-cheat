## cheat engine으로 각 항목 주소를 찾자

### 목표: 메모리 내용의 이해
작성자: kkongnyang2 작성일: 2025-07-14

---

### > CE tutorial 1~5

주소 탐색
```
newscan 누르기
창에 initial 값 입력 후 firstscan 누르기
값 변화시키기
창에 second 값 입력 후 nextscan 누르기
목록 추려지면 클릭해서 아래 작업바에 내리기
거기서 값 조작
```
코드 무효화
```
what writes 들어가서 디스어셈블러 보기. 해당 줄 코드를 replace with code that does nothing으로 바꾸면 체력값을 쓰는 코드가 무효화돼서 작동 안한다.
```

### > 빌드와 런 이해하기

빌드 = 소스 파일을 실행 파일로 컴파일. 단 한번.
런 = 그 실행 파일이 메모리에 올라와서 프로세스로 돌아감.

빌드는 소스 파일의 모든 함수를 어셈블리어로 만들고 .text와 .data 등 섹션을 나눠 저장한다. 어차피 전부 가상주소긴 해도 함수 간의 오프셋은 완벽하게 고정된다. 그렇기에 press_button() 함수에서 hp_edit() 함수로, struct player 데이터로, 이 사이에는 정해진 거리가 있어 그렇게 호출이 가능하다. 그걸 오프셋이라 부른다.

```
# foo() 안에서 bar() 호출 (x86-64 PIE)
call  [rip + 0x34]   ; ← 0x34는 bar 까지의 거리

# 만약 진짜 외부에 있으면?(라이브러리 등)
# 외부 심볼 bar() 호출
call  QWORD PTR [rip + .got.plt.bar]  ; GOT 슬롯에 patch된 절대주소를 읽음
```
```
0x000055f4_9c8a0000  ── [텍스트]  ← PIE 베이스 (ASLR로 매 실행마다 변동)
                     ├─ [rodata]
                     ├─ [data]     ← g_player
                     ├─ [bss]
                     ├─ (heap)     ← malloc() 구조체들 … ↑ 성장
0x00007f3d_40000000  ── [libc.so]  ← 공유 라이브러리 베이스
                     ├─ 기타 .so …
                     ├─ (익명 mmap/JIT 영역)
0x00007ffe_fffe0000  ── [stack]    ← 스레드마다 새로, 위에서 ↓ 성장
```

우리가 이걸 런하면 새로 순차적으로 코드들이 생기는 게 아니라 이 빌드 실행 파일 안에서 위치만 옮겨가며 각 줄을 cpu로 작동시키는 것이다. 모든 행동은 이미 완벽하게 규율로 지정되어 있다. 하드웨어 인터럽트가 커널 단에서 작동되면 그게 어느 주소로 향하고 그 다음은 어디로 정해져 있는지의 문제이다. 물론 인터럽트에 의해 새로 할당되는 데이터들도 있긴 하다. 이것들은 힙, 스택들로 쌓인다.

즉, 빌드는 완벽히 고정된 퍼즐, 런은 베이스 주소를 기반으로 그 퍼즐 전체를 그대로 올리고, 사용자 인터럽트에 의해 추가 데이터가 더 할당되는 것.

```
┌─[빌드 단계]─────────────────────────────────────────┐
│ ① 모든 코드·전역·상수 → 한 퍼즐처럼 “세그먼트 + 오프셋” 고정 │
│    (.text / .rodata / .data / .bss)                          │
└──────────────────────────────────────────────────────────────┘
         ↓ execve()
┌─[런타임: 프로세스 시작]──────────────────────────────┐
│ ② 커널·ld.so가 베이스 주소(ASLR)만 정해 mmap       │
│    ──> “퍼즐” 전체를 한 번에 가상공간에 올림        │
└────────────────────────────────────────────────────┘
         ↓ 게임 진행
┌─[런타임: 동적 동작]────────────────────────────────┐
│ ③ 사용자 입력·네트워크 이벤트 등 → 힙/스택/mmap에 │
│    새 객체·버퍼를 **필요할 때마다** 추가           │
└────────────────────────────────────────────────────┘
```

참고로 모든 프로세스들은 메모리 뒷부분에 커널을 공통으로 가지고 있다.

### > CE tutorial 6~9

게임 진행 도중 새 플레이어의 hp와 ammo를 어떻게 할당할까?

struct player *p = malloc(sizeof *p);   // (1) rax ← 새 힙 주소
g_local_player   = p;                   // (2) 코드가 전역 변수에 저장

이런 구조를 통해 할당한다. 따라서 우리는 플레이어 마다 값이 적힌 주소를 일일히 찾아다닐게 아니라, 저 할당하고 포인터하는 (베이스 주소 기준으로)고정된 진원지 자체를 찾아야 한다.

```
[game.bin+0x1BCF30]   → 0x0000006002A000   (전역 포인터, 오프셋 고정)
                                    │
                                    ├─ hp  = *(+0x00)
                                    ├─ ammo= *(+0x04)
                                    └─ pos  = *(+0x08) …
```

우리는 저 값이 담긴 주소를 누가 access 하는지 진원지를 찾을 것이다. 튜토리얼에서 change value라는 버튼을 눌러서 값을 할당하고 쓰기 명령을 내렸다 했을 때 이를 따라가보자.

먼저 용어 정리.
베이스 주소는 런할때 정해지는 영점 조절 같은 것이다. game.bin
포인터는 값 부분에 수치를 입력하는게 아니라 주소를 입력하는 4바이트(64비트 os면 8바이트)다. 0x0000006002A000라는 주소값을 적어준다.
오프셋은 고정된 퍼즐에서의 거리를 뜻한다. +뒤에 적힌 걸 의미한다.

만약 빌드 때 이미 위치가 정해진 정적 데이터면 포인터를 쓸 필요도 없이 거리로 바로 읽으면 된다.


하향식 (값에서부터 체인 따라가기)
```
Find out what writes to this address -> 쓰기 타입 명령만
Find out what accesses this address -> 모든 타입 명령(참조 등등)

# 값이 담긴 주소 탐색
# 15C4508. what writes
# 체력값 16F를 15C44F0 + 18 위치에 적으라는 코드 발견
"Tutorial-x86_64.exe"+4910A:
10004910A - 89 46 18  - mov [rsi+18],eax <<
RAX=000000000000016F
RSI=00000000015C44F0

# 15C44F0를 포인팅 하는 주소 탐색
# 1642E30. what access
# 1642E30 위치에 있던 값 15C44F0을 불러오는 코드 발견
"Tutorial-x86_64.exe"+490C8:
1000490C8 - 48 8B 36  - mov rsi,[rsi] <<
RAX=0000000001642E30
RSI=00000000015C44F0

# 1642E30을 포인팅 하는 주소 탐색
# 162A838. what access
# 162A820 + 18 위치에 있던 값 1642E30을 불러오는 코드 발견
"Tutorial-x86_64.exe"+4907C:
10004907C - 48 8B 76 18  - mov rsi,[rsi+18] <<
RAX=000000000162A820
RSI=0000000001642E30

# 162A820을 포인팅 하는 주소 탐색
# 162A5B0. what access
# 162A5A0 + 10 위치에 있던 값 162A820을 불러오는 코드 발견
"Tutorial-x86_64.exe"+49034:
100049034 - 48 8B 76 10  - mov rsi,[rsi+10] <<
RAX=000000000162A5A0
RSI=000000000162A820

# 162A5A0을 포인팅 하는 주소 탐색
# "tutorial-x86_64.exe"+34ECA0.

10034ECA0 -> 162A5A0 + 10 -> 162A820 + 18 -> 1642E30 -> 15C44F0 + 18 -> 16F

# change address 누르고 tutorial-x86_64.exe 부터 체인 연결해주면 됨
```
![image](Capture.PNG)

코드 인젝션
```
디스어셈블리에서 ctrl+A 눌러 어셈블러 창 열기. code injection을 클릭하고 원하는 부분의 내용 수정
```

상향식 (메모리 전수 스캔)
```
pointer scan for this address 자동 스캐너 이용.
값 주소를 찾은 후 우클 누르고 해당 스캐너 클릭. max level은 보통 5로 , max offset은 0x3FF 정도로 설정. .ptr 파일에 후보 리스트가 저장되면 게임 재실행 후 pointer scan 누르고 rescan memory 하면 정말 static인 후보만 남는다. 그것들을 작업바로 옮긴다.
```

### > 오버워치 그래픽 이해

[호스트 Linux] ──(VFIO/PCIe 패스스루)──► [게스트 Windows]
  │                                            │
  │   vfio-pci 바인딩                           │  공식 NVIDIA 드라이버
  │                                            │  DX11 Feature Level OK
  └─> Hyper-V/VBS OFF  ◄────────────┐          │
                                    │          │
                    Overwatch 2 런처 + 안티치트 │
                    (벤더 ID·DX11·WDDM 검사)   │

컴퓨터에는 물리 gpu 장치가 있고, 일반적으로 윈도우에 내장된 WDDM 드라이버를 이용해 directX 11 이라는 api로 그래픽 관련 소통한다. cpu가 아닌 전용 gpu로 그래픽을 처리하는 걸 가속이라고 부른다. 또한 cpu와의 전용 고속 버스를 PCIe라고 부르는데, 이를 가지는 장치는 gpu, 사운드카드 등이 있다.

만약 가상머신 환경이라면 가상 장치를 에뮬레이션 하지만, 오버워치 2에서는 gpu 장치와 드라이버의 ID를 명확히 확인하며 directX 11만 받아들이기 때문에 게스트에 패스스루 해야한다. 리눅스 커널이 제공하는 패스스루용 프레임워크인 VFIO-PCI를 이용해 PCIe 장치들을 안전하게 넘길 수 있다.

* DX Feature Level 11_0 이상 & 서명된 WDDM 2.x 조건도 검사
* “게임 ↔ Direct3D 11 ↔ WDDM 드라이버 ↔ PCIe GPU” 체인
* VFIO-PCI는 같은 IOMMU 그룹에 묶인 장치는 통째로 넘겨야 함
* 다른 게임은 에뮬레이션인 virtio-gpu(VirGL)만으로 충분하기도 함

┌────────────────────────────────────────────────────────────┐
│ 6. 애플리케이션 계층                                      │
│    • Overwatch 2 │ Battle.net 런처                         │
│      ↳ 실행 시 “GPU 벤더 ID + DirectX Feature Level 11_0   │
│        + 서명된 WDDM 2.x 드라이버” 여부를 점검            │
└────────────────────────────────────────────────────────────┘
            ▲ Direct3D 11 호출
┌────────────────────────────────────────────────────────────┐
│ 5. 그래픽 API 계층                                        │
│    ✔ Direct3D 11 (DirectX 11)                             │
│      🔧 OpenGL·Vulkan 등도 존재하지만 OW2는 미지원        │
└────────────────────────────────────────────────────────────┘
            ▲ API 호출을 드라이버가 수신
┌────────────────────────────────────────────────────────────┐
│ 4. OS 드라이버 계층                                       │
│    ✔ Windows WDDM 2.x 드라이버 (NVIDIA / AMD / Intel)     │
│      🔧 드라이버가 GPU의 기능(셰이더, 메모리 등)을 매핑    │
└────────────────────────────────────────────────────────────┘
            ▲ I/O 요청이 버스를 타고 이동
┌────────────────────────────────────────────────────────────┐
│ 3. 버스 계층                                              │
│    ✔ PCI Express (PCIe)                                   │
│      🔧 ‘PCI’는 병렬 규격, 오늘날 GPU는 모두 PCIe         │
│      🔧 각 장치는 *Vendor ID:Device ID* 쌍으로 식별        │
└────────────────────────────────────────────────────────────┘
            ▲ 전기적 신호
┌────────────────────────────────────────────────────────────┐
│ 2. 하드웨어 장치 계층                                     │
│    ✔ 물리 GPU (dGPU / iGPU)                               │
│      ↳ 전용 연산 유닛으로 **하드웨어 가속** 수행          │
└────────────────────────────────────────────────────────────┘
            ▲ (가상화 환경이라면) 장치 소유권 이전
┌────────────────────────────────────────────────────────────┐
│ 1. 가상화/패스스루 계층                                   │
│    • 리눅스 호스트: VFIO-PCI 모듈이 GPU를 “묶음”           │
│      🔧 IOMMU(-vtd/-vi)가 메모리 접근을 게스트로 격리     │
│    • 게스트 Windows: 실 GPU처럼 인식 → 드라이버 설치 OK   │
│    • 가상 GPU(virtio-gpu)만 사용할 경우 DirectX 11 미지원 │
└────────────────────────────────────────────────────────────┘


### > DMA 장치 이해

패스스루를 하기 위해 DMA 장치들을 이해해보자.

DMA? Direct Memory Access
CPU를 거치지 않고 디바이스(SSD, GPU, NIC)가 주 메모리로 직접 읽고 쓰기. 장치가 고속 버스(PCIe)를 통해 DMA 트랜잭션을 발행하면 메모리 컨트롤러가 전송 처리.

IOMMU? I/O MMU
디바이스 전용 MMU.

VT-d? Intel Virtualization Technology for Directed I/O
intel CPU 칩셋에 내장된 IOMMU 구현 브랜드명. AMD는 AMD-VI라 표기. ACPI DMAR 테이블에 DRHD(하드웨어 유닛)·RMRR(예약 메모리)·ATSR 등을 기록. 리눅스 intel_iommu (옵션 intel_iommu=on), Windows vtd.sys으로 작동.

ACPI? Advanced configuration & Power Interface
UEFI->OS로 건네주는 하드웨어, IRQ, 전원 관리 설명서. 테이블 형식으로 되어 있다.
RSDP(루트) → XSDT/RSDT가 여러 하위 테이블 주소를 나열.
• 대표 테이블: FADT(전원), MADT(APIC·CPU), DSDT(AML 코드), DMAR(IOMMU) 등

DMAR? Direct Memory Access Remapping
ACPI 속 Intel VT-d(IOMMU)를 초기화하기 위한 전용 테이블 이름. DMAR 엔진이 어디에 몇 개가 달려 있으며 어떤 PCI 장치를 맡는가.
• DRHD(DMA Remapping Hardware Unit)은 IOMMU 레지스터 베이스, 관리 버스.
• RMRR(Resserved Memory Region Reporting)은 BIOS, iGPU, USB 레거시 등 특정 장치 부팅 중 DMA 해야 하는 예약 메모리.
• ATSR(Address-Translation Service Remap)

```
# gpu 확인
~$ lspci | grep -i vga
00:02.0 VGA compatible controller: Intel Corporation CometLake-U GT2 [UHD Graphics] (rev 02)
# IOMMU 활성 여부
~$ sudo dmesg | grep -e IOMMU -e DMAR
# DMAR 테이블은 intel 펌웨어가 제공했다
[    0.023609] ACPI: DMAR 0x0000000084136000 0000A8 (v01 INTEL  EDK2     00000002      01000013)
# DMAR 테이블 위치
[    0.023660] ACPI: Reserving DMAR table memory at [mem 0x84136000-0x841360a7]
# IOMMU가 39비트 물리 주소(512GB)까지 번역 가능
[    0.116940] DMAR: Host address width 39
# DRHD(DMA-Remapping Hardware Unit) 두개가 존재
# flags 0x0 = 모든 버스 포함, flags 0x1 = 스코프 안 PCI 디바이스만
[    0.116942] DMAR: DRHD base: 0x000000fed90000 flags: 0x0
[    0.116957] DMAR: dmar0: reg_base_addr fed90000 ver 1:0 cap 1c0000c40660462 ecap 19e2ff0505e
[    0.116962] DMAR: DRHD base: 0x000000fed91000 flags: 0x1
[    0.116968] DMAR: dmar1: reg_base_addr fed91000 ver 1:0 cap d2008c40660462 ecap f050da
# RMRR(Reserved Memory Region Reporting)로 항상 열어두는 영역
[    0.116971] DMAR: RMRR base: 0x00000084cb2000 end: 0x00000084efbfff
[    0.116976] DMAR: RMRR base: 0x00000087000000 end: 0x000000897fffff
# IRQ remapping 기능 켜짐. Queued invlidation(TLB flush)까지 활성
[    0.116979] DMAR-IR: IOAPIC id 2 under DRHD base  0xfed91000 IOMMU 1
[    0.116982] DMAR-IR: HPET id 0 under DRHD base 0xfed91000
[    0.116984] DMAR-IR: Queued invalidation will be enabled to support x2apic and Intr-remapping.
[    0.119247] DMAR-IR: Enabled IRQ remapping in x2apic mode
# 커널 퀀크가 iGPU(00:02.0)을 IOMMU에서 강제 제외. 따라서 이 IGD를 VFIO-PCI로 통째로 패스스루 하는 경로는 막힘
[    0.389107] pci 0000:00:02.0: DMAR: Disabling IOMMU for graphics on this chipset
# ATSR(Address-Translation Service Remap)와 SATC(System Agent Translation Cache) 없음. 상관없음
[    0.513959] DMAR: No ATSR found
[    0.513962] DMAR: No SATC found
# 두 번째 DRHD도 queued invalidation 사용
[    0.513965] DMAR: dmar1: Using Queued invalidation
# 최종적으로 intel_iommu 모듈이 올라가면서 VT-d 활성 완료.
[    0.516031] DMAR: Intel(R) Virtualization Technology for Directed I/O
# GPU가 vfio-pci에 묶였는지
~$ lspci -nnk | grep -A3 VGA
# iGPU 벤더, 디바이스 ID: 8086:9b41, 드라이버: i915
00:02.0 VGA compatible controller [0300]: Intel Corporation CometLake-U GT2 [UHD Graphics] [8086:9b41] (rev 02)
	DeviceName: Onboard - Video
	Subsystem: Samsung Electronics Co Ltd CometLake-U GT2 [UHD Graphics] [144d:c18a]
	Kernel driver in use: i915
```
즉, IOMMU(VT-d) 자체는 활성. irq remap, qi 지원까지 갖춰져 있어 USB,NVMe 카드 등은 바로 VFIO 패스스루 가능. 하지만 IGD(내장 GPU)는 IOMMU에서 제외.
따라서 내장그래픽 장치를 쪼개서 쓰는 GVT-g(vGPU) 밖에 불가능. 풀 VFIO 패스스루(GVT-d)를 시도한다면 막힘.


### > 패스스루 실행

GVT-g(vCPU) 구성 방법

step 1. 커널 매개변수
```
~$ sudo nano /etc/default/grub
원본
GRUB_DEFAULT=0
GRUB_TIMEOUT_STYLE=hidden
GRUB_TIMEOUT=10
GRUB_DISTRIBUTOR=`lsb_release -i -s 2> /dev/null || echo Debian`
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash"
GRUB_CMDLINE_LINUX=""

수정 후
GRUB_DEFAULT=0
GRUB_TIMEOUT_STYLE=hidden
GRUB_TIMEOUT=10
GRUB_DISTRIBUTOR=`lsb_release -i -s 2> /dev/null || echo Debian`
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash intel_iommu=on i915.enable_gvt=1 kvm.ignore_msrs=1 iommu=pt"
GRUB_CMDLINE_LINUX=""

~$ sudo update-grub && sudo reboot
```
step 2. 모듈 준비
```
~$ sudo modprobe kvmgt mdev vfio-iommu-type1 vfio-pci
# 지원 타입 확인
~$ ls /sys/bus/pci/devices/0000:00:02.0/mdev_supported_types
# 예) i915-GVTg_V5_4  i915-GVTg_V5_8 …

# vGPU 1개 생성
UUID=$(uuidgen)
~$ echo $UUID | sudo tee \
 /sys/bus/pci/devices/0000:00:02.0/mdev_supported_types/i915-GVTg_V5_4/create
```
step 3. qemu 부팅
```
-device vfio-pci,sysfsdev=/sys/bus/mdev/devices/$UUID,\
      display=on,x-igd-opregion=on,ramfb=on
```

GVT-d(풀 VFIO 패스스루) 구성 방법

step 1. qemu 부팅
```
-device vfio-pci,host=01:00.0,x-vga=on,kvm=on
```
step 2. 게스트 설정
```
    core isolation → Memory Integrity OFF
    Optional: bcdedit /set hypervisorlaunchtype off
```
step 3. 배틀넷 설정
```
    Battle.net → 설정 → 게임 내 옵션 초기화
    윈도우 전원 옵션을 고성능으로 변경
```

### > 