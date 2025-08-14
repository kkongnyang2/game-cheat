## igpu 분할 패스스루 vm을 만들자

작성자: kkongnyang2 작성일: 2025-08-01

---

### 가상화 지원 확인

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


### 커널 설정

전제 확인
```
$ grep GVT /boot/config-$(uname -r)     # 커널에 i915 GVT 코드 존재 확인
CONFIG_DRM_I915_GVT_KVMGT=m
CONFIG_DRM_I915_GVT=y
```

커널 설정
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

### 모듈 및 vGPU 준비

모듈 키기
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

vGPU 생성

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
* 참고로 컴퓨터 끄면 UUID 날라가니 매번 새로 만들어줘야 함.

### qemu-kvm 옵션의 이해

만약 git으로 qemu 설치하고 싶으면
```
git clone https://gitlab.com/qemu-project/qemu.git
cd qemu
mkdir build
cd build
../configure --target-list=x86_64-softmmu --enable-kvm --disable-werror
ninja -j$(nproc)
ninja install
```

빌드 때는 어떤 타깃 아키텍처를 만들지, 어떤 GUI/디스플레이 백엔드/가속 라이브러리를 포함할지 결정

런타임 때는 VM 하드웨어 토폴로지, 가속(KVM), CPU 패스스루, 메모리, 펌웨어(OVMF), 디스크/네트워크/USB/그래픽 장치, VFIO 패스스루(vGPU 포함), 디버깅/관리(QMP,monitor) 등을 설정.

../configure 옵션

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


### qemu 부팅

qemu 빌드
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

seaBIOS 부팅
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

> ⚠️ 성공은 했다. 그렇지만 igpu로 게임이 원활하게 작동되지 않아 노트북 교체