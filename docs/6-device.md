## VM 환경에서 디바이스들을 정돈하자

### 목표: 장치들 이해
작성자: kkongnyang2 작성일: 2025-07-17

---

### > 오버워치 그래픽 이해

[호스트 Linux] ──(VFIO/PCIe 패스스루)──► [게스트 Windows]
                                               │
                                               │  공식 NVIDIA 드라이버
                                               │  DX11 Feature Level OK
                                               │
                                Overwatch 2 런처 + 안티치트

컴퓨터에는 gpu 장치가 있고, 윈도우에 내장된 드라이버와 api 도구로 그래픽 관련 처리를 한다. cpu가 아닌 gpu로 그래픽을 처리하는 걸 가속이라고 부른다. 또한 cpu와의 전용 고속 버스를 PCIe라고 부르는데, 이를 가지는 장치는 gpu, 사운드카드 등이 있다.

가상머신 환경이라면 장치를 에뮬레이션 하지만, 오버워치 2에서는 gpu 장치와 드라이버의 ID를 명확히 확인하며 directX 11만 받아들이기 때문에 게스트에 패스스루 해야한다. 리눅스 커널이 제공하는 패스스루용 프레임워크인 VFIO-PCI를 이용해 PCIe 장치들을 안전하게 넘길 수 있다.

* 벤더 ID, DX Feature Level 11_0 이상 & 서명된 WDDM 2.x 조건도 검사
* Hyper-V/VBS OFF 해야지 못알아챔
* VFIO-PCI는 같은 IOMMU 그룹에 묶인 장치는 통째로 넘겨야 함
* 다른 게임은 에뮬레이션인 virtio-gpu(VirGL)만으로 충분하기도 함

┌────────────────────────────────────────────────────────────┐
│ 6. 애플리케이션                                              │
│    • Overwatch 2 │ Battle.net 런처                         │
└────────────────────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────┐
│ 5. 그래픽 API                                              │
│    ✔ Direct3D 11 (DirectX 11)                             │
└────────────────────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────┐
│ 4. OS 드라이버                                            │
│    ✔ Windows WDDM 2.x 드라이버 (NVIDIA / AMD / Intel)     │
└────────────────────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────┐
│ 3. 버스                                                   │
│    ✔ PCIe 각 장치는 *Vendor ID:Device ID* 쌍으로 식별        │
└────────────────────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────┐
│ 2. 하드웨어                                                 │
│    ✔ 물리 GPU (dGPU / iGPU) 전용 연산 유닛으로 하드웨어 가속       │
└────────────────────────────────────────────────────────────┘
┌────────────────────────────────────────────────────────────┐
│ 1. 가상화/패스스루                                            │
│    • 호스트: IOMMU에 묶여있는 GPU를 VFIO-PCI 모듈로 패스스루       │
└────────────────────────────────────────────────────────────┘


### > 기본 그래픽 처리 흐름

|용어|뜻|포인트|
|---|---|---|
|렌더링(Rendering)|앱이 데이터를 자신의 이미지 픽셀/버퍼로 그리는 과정. CPU 또는 GPU 사용|“내 창 안의 내용” 생성.|
|합성(Compositing)|여러 윈도우 버퍼(배경, 창, 패널 등)를 하나의 최종 화면 이미지로 조립|화면에 실제로 뭐가 보일지 결정|
|프레임버퍼(Frame Buffer)|디스플레이로 보낼 픽셀 메모리. 최종 합성 결과가 여기에 놓임|디스플레이 컨트롤러가 읽어감|
|백 버퍼/프런트 버퍼|그릴 때 쓰는 임시 버퍼 vs 현재 화면 표시 중인 버퍼|깜빡임 방지(더블 버퍼링)|
|오버레이(Overlay Plane)|합성 없이 특정 버퍼를 직접 화면 특정 영역에 하드웨어적으로 얹는 기능|비디오 재생 최적화 등|

렌더링? 위치 데이터를 transform과 projection하여 화면에 그래픽을 그림
명령 생성: 응용 프로그램
명령 해석: 렌더링 API + GPU 드라이버
실제 연산: 하드웨어 GPU

렌더링 API? 화면에 3D/2D 그래픽을 그리는 일을 도와주는 약속된 함수 집합 도구. drawTriagnles()같은 명령으로 실제로 삼각형을 그림. 정점 데이터를 GPU로 전송, 조명 계산, 픽셀에 색상 결정, GPU에 필요한 메모리 할당, 프레임버퍼 관리.

GPU 드라이버? 렌더링 API 명령을 GPU 하드웨어 명령으로 번역

cpu는 명령과 데이터를 준비하고, GPU가 대량 픽셀 연산을 수행한 뒤 결과를 메모리 상의 프레임버퍼에 쓴다. 디스플레이 하드웨어가 그 프레임버퍼를 직접 읽어 모니터에 띄운다.

프레임버퍼는 자체 VRAM(video random access memory)라는 메모리에 있기도 하고, 그냥 DRAM의 일부를 프레임버퍼 용도로 사용하기도 한다. 어쨌거나 cpu가 그 픽셀을 복사하려하지 않고 주소나 레지스터만 업데이트하며 처리한다.

깜빡임 방지를 위해 두세개의 버퍼를 사용한다.

그리고 여러 개의 프로그램 창을 띄우면 다음과 같이 합성을 통해 진행된다. 본인 창이 차치하고 있는 영역의 크기를 알려주면 그에 맞게 각 렌더링 API들이 데이터를 만들어 합성기에 전달하여 하나의 최종 화면을 만드는 것이다.


GPU 드라이버? 렌더링 API 명령을 GPU 하드웨어 명령으로 번역

┌────────────────────────────────────────────────────────┐
│                      애플리케이션들                     │
│  App A  App B  App C ...                                │
│   │      │      │                                       │
│   │렌더링 API 사용 (OpenGL, Vulkan, Direct3D, Skia 등) │
│   ▼      ▼      ▼                                       │
│ [윈도우 버퍼 A] [윈도우 버퍼 B] [윈도우 버퍼 C]         │
└────┬────────────┬────────────┬──────────────────────────┘
     │            │            │
     ▼            ▼            ▼
    버퍼 핸들 전달: X11 Pixmap, Wayland wl_buffer, Windows DWM Surface
            ┌───────────────────────────┐
            │      컴포지터(합성기)        │
            │  - 윈도우 스택/좌표           │
            └─────────┬─────────────────┘
                      ▼  
                  [GPU 연산]
                      │
                      ▼
                   [백 버퍼]
                      │
                      ▼
                 디스플레이 컨트롤러 → 모니터


이때 특수하게 비디오 같은 프로그램은 합성이 아닌 오버레이 방식을 사용하여 overlay plane으로 덮는다. 너무 자주 바뀌어 합성이 힘드니 직접 디스플레이 컨트롤러의 오버레이 플레인에 말을 거는 것이다.

               ┌───────────────────────┐
               │ Display Controller    │  ← 디스플레이 하드웨어
               │ (scanout engine)      │
               │                       │
               │   Primary  Plane  ──────┐  데스크톱 합성된 전체화면(RGB)
               │   Overlay  Plane  ──┐   │  비디오 YUV 버퍼 (HW 변환)
               │   Cursor   Plane ─┐ │   │  하드웨어 커서
               └────────────────┬──┴─┴───┘
                                │
                              HDMI/eDP/DP → 모니터


### > VM 디스플레이

그럼 VM은 어떤 방식으로 진행하냐?

┌─────────────────────────────────────────────┐
│               게스트 OS                      │
│      앱 → 그래픽 API → GPU 드라이버             │─────┐
│       → 가상/패스스루 장치에 커맨드 전달           │     │
└─────────────────────────────────────────────      │
              ├─ virtio-gpu (가상)                   └─ vfio-pci (패스스루)
  ┌────────────────────────────────────────┐        │
  │               호스트 OS                 │         │
  │  ┌──────────────────────────────────┐  │        │
  │  │            QEMU 프로세스           │  │        │ 
  │  │   ├─ gtk ( -display gtk )        │  │        │
  │  │   └─ sdl ( -display sdl )        │  │        │
  │  │       → 그래픽 API(gl=on)          │  │        │
  │  └──────────────────────────────────┘  │        │
  │           → GPU 드라이버                 │        │
  └────────────────────────────────────────┘        │ 
                   하드웨어                          하드웨어


전체 패스스루 방식이면 (VFIO-PCI)
CPU도 그대로 사용(일부 trap만 따로 처리), RAM도 할당된 영역만큼 그대로 사용, GPU도 드라이버까지 설치해서 직접 사용.

분할 패스스루 방식이면 (GVT-g)
GPU를 넘겨줘서 연산은 하되 최종 프레임버퍼는 호스트가 합성.

게스트 Windows
 └▶ DirectX 호출
     └▶ GVT-g 드라이버
         └▶ QEMU + GVT-g 커널 모듈
             └▶ 실제 GPU의 일부 자원을 할당
                 └▶ GPU가 연산 → 결과는 호스트 메모리에 복사
                     └▶ 호스트 GTK 창으로 합성 출력


가상화 방식이면 (Virtio-gpu)
가짜 GPU를 만들어 호스트가 수신해와 렌더링 연산과 출력을 담당

게스트 OS
 └▶ 렌더링 API 호출
     └▶ 게스트용 가상 GPU 드라이버 (virtio-gpu)
         └▶ QEMU 가상 장치가 데이터 수신
             └▶ 호스트가 그걸 해석해서 렌더링


전통 VGA 방식이면
GPU 사용 없이 오직 그 위에서 장치를 구현해서 진짜 가상으로 연산 처리

ramfb(RAM Framebuffer)는 게스트 os가 메모리에 픽셀 데이터를 쓰면 호스트가 그 메모리를 읽어와 화면에 그대로 표시하는 매커니즘(렌더링x). UEFI(OVMF) 부팅 중에 사용.


### > qemu-kvm 옵션의 이해

빌드 때는 어떤 타깃 아키텍처를 만들지, 어떤 GUI/디스플레이 백엔드/가속 라이브러리를 포함할지 결정

런타임 때는 VM 하드웨어 토폴로지, 가속(KVM), CPU 패스스루, 메모리, 펌웨어(OVMF), 디스크/네트워크/USB/그래픽 장치, VFIO 패스스루(vGPU 포함), 디버깅/관리(QMP,monitor) 등을 설정.

./configure 옵션

| 카테고리 | 대표 옵션 | 설명 |
|--------|---------|------|
|타깃 아키텍처|--target-list=x86_64-softmmu|필요한 에뮬레이션 대상만 빌드|
|설치 경로|--prefix=/usr/local|시스템 기본 패키지와 충돌 피하려면 별도 prefix|
|디버그|--enable-debug|문제 추적 유용|
|KVM 지원|--enable-kvm|보통 자동 감지되나 하는게 좋음|
|그래픽 백엔드|--enable-gtk,--enable-sdl,--enable-opengl|GUI창(gtk),레거시/대안(sdl),GL렌더링(virgl 등과 엮임)|
|오디오|--audio-drv-list=pa|오디오 백엔드 선택(필요 없으면 제거)|
|SPICE/remote|--enable-spice,--enable-usb-redir|원격 그래픽 및 USB 리다이렉션|
|VFIO 관련|거의 없음|시스템 라이브러리에 의존. 커널쪽 VFIO 모듈이 더 중요|
|스토리지|--enable-libiscsi|네트워크 스토리지 프로토콜 필요 시|
|LTO/최적화|--enable-lto|최적화 관련|


gtk? 가장 일반적인 선택. 창 크기 조절, 메뉴 제어, 마우스 포인터 통합 등등.
sdl? 경량 옵션. 창 크기 고정, 메뉴 없음, 마우스 포인터 중첩 불가. 키오스크 같은 고정 미니멀 환경

gl=on(opengl)? virtio-gpu를 사용할때 호스트로 전달받은 게스트 렌더링을 처리하기 위한 api.


런타입 골격
일반 KVM 가속 + UEFI(OVMF) + Virtio 디스크 + GTK 창 을 제일 많이 사용한다.
qemu-system-x86_64 \
  -enable-kvm \
  -machine q35,accel=kvm \
  -cpu host \
  -smp 4 \
  -m 4G \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd \
  -drive if=pflash,format=raw,file=$HOME/OVMF_VARS.fd \
  -drive file=vm.qcow2,if=virtio,format=qcow2,cache=none \
  -netdev user,id=n0,hostfwd=tcp::2222-:22 \
  -device virtio-net-pci,netdev=n0 \
  -display gtk,gl=on \
  -monitor stdio \
  -qmp unix:/tmp/vm-qmp.sock,server,nowait


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

NVMe? SSD 전용 고속 통신 규격 프로토콜. PCIe 고속버스와 함께 SSD를 보조.


### > 분할 패스스루 실행 - GVT-g(vCPU)

step 1. 전제 확인
```
$ grep GVT /boot/config-$(uname -r)     # 커널에 i915 GVT 코드 존재 확인
CONFIG_DRM_I915_GVT_KVMGT=m
CONFIG_DRM_I915_GVT=y
```

step 2. 커널 매개변수
```
$ sudo nano /etc/default/grub
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

$ sudo update-grub && sudo reboot
```

step 3. 모듈 준비
```
$ sudo modprobe kvmgt mdev vfio-iommu-type1 vfio-pci
$ sudo dmesg | grep -i gvt              # 로그 확인
[    0.000000] Command line: BOOT_IMAGE=/boot/vmlinuz-6.8.0-60-generic root=UUID=f20a3184-37e9-471b-93ed-2832064a0619 ro quiet splash intel_iommu=on i915.enable_gvt=1 kvm.ignore_msrs=1 iommu=pt vt.handoff=7
[    0.045888] Kernel command line: BOOT_IMAGE=/boot/vmlinuz-6.8.0-60-generic root=UUID=f20a3184-37e9-471b-93ed-2832064a0619 ro quiet splash intel_iommu=on i915.enable_gvt=1 kvm.ignore_msrs=1 iommu=pt vt.handoff=7
[  462.136478] i915 0000:00:02.0: Direct firmware load for i915/gvt/vid_0x8086_did_0x9b41_rid_0x02.golden_hw_state failed with error -2
$ ls /sys/bus/pci/devices/0000:00:02.0/mdev_supported_types     # 지원 타입 확인
i915-GVTg_V5_4  i915-GVTg_V5_8
$ lsmod | grep kvmgt                    # 모듈 확인
kvmgt                 421888  0
mdev                   24576  1 kvmgt
vfio                   69632  2 kvmgt,vfio_iommu_type1
kvm                  1413120  2 kvmgt,kvm_intel
i915                 4300800  24 kvmgt
```

step 4. vGPU 생성

/sys/bus/pci/devices/0000:00:02.0/
└─ mdev_supported_types/
   └─ i915-GVTg_V5_4/
      ├─ create      ←  UUID 쓰면 ➜ 새 디바이스 생성
      ├─ remove      ←  UUID 쓰면 ➜ 해당 디바이스 삭제
      └─ instances   ←  이미 존재하는 UUID 목록 (읽기 전용)

```
$ UUID=$(uuidgen)           # UUID 생성
$ echo $UUID | sudo tee \
 /sys/bus/pci/devices/0000:00:02.0/mdev_supported_types/i915-GVTg_V5_4/create
```
참고로 컴퓨터 끄면 UUID 날라가니 매번 새로 만들어줘야 함.

step 5. qemu 빌드

예전 꺼는 gtk 옵션을 안해놔서 다시 재빌드
```
$ cd qemu && git checkout v8.2.0    # 8.2.0 버전 읽기 모드로 전환한것. 다시 실시간 버전으로 바꾸고 싶다면 git switch - 
$ rm -rf build                # 했던 빌드 디렉토리 삭제
$ mkdir build && cd build
$ ../configure --target-list=x86_64-softmmu \
            --enable-kvm --enable-debug \
            --enable-opengl --enable-gtk \
            --disable-werror
$ ninja -j$(nproc)
$ ninja install
```

step 6. qemu 부팅

펌웨어 seaBIOS 부팅법
```
$ sudo qemu-system-x86_64 \
  -machine q35,accel=kvm -cpu host,kvm=on -smp 4 -m 4G \
  -vga none \
  -device vfio-pci,sysfsdev=/sys/bus/mdev/devices/$UUID,display=on,x-igd-opregion=on \
  -drive file=~/win10_120g.qcow2,if=virtio,format=qcow2,cache=none \
  -monitor stdio \
  -qmp unix:/tmp/win10-qmp.sock,server,nowait \
  -display gtk,gl=on
```

### > 번외

qemu에서 펌웨어 OVMF(UEFI) 부팅법
```
$ sudo apt install ovmf
$ cp /usr/share/OVMF/OVMF_VARS.fd  ~/OVMF_VARS_WIN10.fd   # 얜 홈에 꺼내놔야 함
$ sudo qemu-system-x86_64 \
  -enable-kvm \
  -machine q35,accel=kvm -cpu host,kvm=on -smp 4 -m 4G \
  -vga none \
  -device vfio-pci-nohotplug,sysfsdev=/sys/bus/mdev/devices/$UUID,display=on,x-igd-opregion=on,ramfb=on \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd \
  -drive if=pflash,format=raw,file=$HOME/OVMF_VARS_WIN10.fd \
  -drive file=~/win10_120g.qcow2,if=virtio,format=qcow2,cache=none \
  -monitor stdio \
  -qmp unix:/tmp/win10-qmp.sock,server,nowait \
  -display gtk,gl=on
```
실패함

qemu에서 풀 패스스루 실행법 - GVT-d
```
qemu 기본 부팅에 라인 하나 추가
-device vfio-pci,host=01:00.0,x-vga=on,kvm=on
```
이게 더 간편하고 좋긴 하지만 10세대 노트북에서는 지원 안함.

잠깐 컴퓨터 용량 비우기
```
$ apt-mark showmanual     # apt를 통해 깐 패키지만 보여줌
$ sudo apt remove qemu-system-arm
$ snap list               # snap을 통해 깐 앱 보여줌
$ sudo snap remove rpi-imager
```

### > 게스트 측 보안 제거

안티치트가 Hyper-V 커널 레이러를 감지하면 바로 블루스크린.

step 1. 설정 › 앱 › 선택적 기능
```
    Hyper-V
    Windows Hypervisor Platform
    Virtual Machine Platform
    Windows Sandbox
    Windows Subsystem for Linux
    → 전부 체크 해제(제거) ▸ 재부팅
```

step 2. 설정
```
    core isolation → Memory Integrity OFF
    Optional: bcdedit /set hypervisorlaunchtype off
```









