## VM을 만들자

작성자: kkongnyang2 작성일: 2025-08-08

---

### > 하이퍼바이저 선택

|       하이퍼바이저       | VMI 친화도 | 장점                                          | 단점                           | 대표 툴/프로젝트                   |
| :----------------: | :-----: | :------------------------------------------ | :--------------------------- | :-------------------------- |
|       **Xen**      |  ★★★★★  | LibVMI 원조급, altp2m 기반 메모리 이벤트, DRAKVUF 등 풍부 | 설정/운영 복잡, 최신 HW 지원/호환성 이슈    | **DRAKVUF**, LibVMI         |
|    **KVM/QEMU**    |  ★★★★☆  | 리눅스 기본, libvirt 연동 쉽고 널리 사용                 | 이벤트 트랩 구현 시 Xen보다 손이 더 감     | LibVMI, PyVMI               |
|   **VMware ESXi**  |  ★★★★☆  | 공식 introspection API, 안정적                   | 상용/폐쇄적, 비용, 자유도 제한           | VMware VMI SDK (Thin agent) |
|     **Hyper-V**    |  ★★☆☆☆  | Windows 친화, 커널 레벨 접근 가능                     | 공개 도구 적음, 구현 난이도             | 자체 개발 필요                    |
|   **VirtualBox**   |  ★☆☆☆☆  | 셋업 쉬움                                       | VMI용 API 사실상 없음, hacky 작업 필요 | 직접 /proc/…/mem 접근 등 “꼼수”    |
| **Bareflank/ACRN** |  ★★☆☆☆  | 경량/오픈소스, 연구 목적 좋음                           | 문서/커뮤니티 작음, 모든 기능 직접 구현      | 커스텀 하이퍼바이저 개발               |

메모리 이벤트 트랩이면 Xen + LibVMI (DRAKVUF 구조 참고)
libvirt 없이 Xen 자체 툴(xl) 사용. altp2m로 페이지 권한 바꿔 트랩 잡기 수월.

이미 우분투에 kvm 환경이라면 KVM/QEMU + LibVMI
libvirt로 편하게 관리 하고, 문서 많은 libvmi/pyvmi로.

* kvm(kernel based virtual machine) 리눅스 커널 모듈. VT-x를 사용해서 게스트 cpu 실행을 커널 레벨에서 가속해줌. qemu는 가상머신 하이퍼바이저.

윈도우 환경에서 hyper-v로 작업하면 바로 오버워치2 안티치트에 탐색


### > 가상화 지원 확인

cpu 가상화 지원 여부
```
$ egrep -c '(vmx|svm)' /proc/cpuinfo    # 0이 아니어야 함. UEFI에서 Intel VT-x/AMD-V 활성화 필요
40            # 논리 cpu 40개 모두가 Intel VT-x 플래그를 갖고 있다는 뜻
```

장치 IOMMU 존재 여부
```
$ sudo dmesg | grep -i -e iommu -e dmar
[    0.005034] ACPI: DMAR 0x000000005BA13000 000088 (v01 ACRSYS ACRPRDCT 00000002 1025 00040000)
[    0.005073] ACPI: Reserving DMAR table memory at [mem 0x5ba13000-0x5ba13087]
[    0.103447] DMAR: Host address width 39
[    0.103448] DMAR: DRHD base: 0x000000fed90000 flags: 0x0
[    0.103453] DMAR: dmar0: reg_base_addr fed90000 ver 4:0 cap 1c0000c40660462 ecap 29a00f0505e
[    0.103455] DMAR: DRHD base: 0x000000fed91000 flags: 0x1
[    0.103458] DMAR: dmar1: reg_base_addr fed91000 ver 5:0 cap d2008c40660462 ecap f050da
[    0.103459] DMAR: RMRR base: 0x00000064000000 end: 0x000000687fffff
[    0.103461] DMAR-IR: IOAPIC id 2 under DRHD base  0xfed91000 IOMMU 1
[    0.103461] DMAR-IR: HPET id 0 under DRHD base 0xfed91000
[    0.103462] DMAR-IR: Queued invalidation will be enabled to support x2apic and Intr-remapping.
[    0.105022] DMAR-IR: Enabled IRQ remapping in x2apic mode
[    0.642545] pci 0000:00:02.0: DMAR: Skip IOMMU disabling for graphics
[    1.096406] iommu: Default domain type: Translated
[    1.096406] iommu: DMA domain TLB invalidation policy: lazy mode
[    1.141664] DMAR: No ATSR found
[    1.141665] DMAR: No SATC found
[    1.141666] DMAR: IOMMU feature fl1gp_support inconsistent
[    1.141666] DMAR: IOMMU feature pgsel_inv inconsistent
[    1.141667] DMAR: IOMMU feature nwfs inconsistent
[    1.141668] DMAR: IOMMU feature dit inconsistent
[    1.141669] DMAR: IOMMU feature sc_support inconsistent
[    1.141669] DMAR: IOMMU feature dev_iotlb_support inconsistent
[    1.141670] DMAR: dmar0: Using Queued invalidation
[    1.141673] DMAR: dmar1: Using Queued invalidation
[    1.141843] pci 0000:00:02.0: Adding to iommu group 0
[    1.142381] pci 0000:00:00.0: Adding to iommu group 1
[    1.142394] pci 0000:00:01.0: Adding to iommu group 2
[    1.142401] pci 0000:00:04.0: Adding to iommu group 3
[    1.142409] pci 0000:00:08.0: Adding to iommu group 4
[    1.142416] pci 0000:00:0a.0: Adding to iommu group 5
[    1.142424] pci 0000:00:0e.0: Adding to iommu group 6
[    1.142438] pci 0000:00:14.0: Adding to iommu group 7
[    1.142445] pci 0000:00:14.2: Adding to iommu group 7
[    1.142452] pci 0000:00:14.3: Adding to iommu group 8
[    1.142464] pci 0000:00:15.0: Adding to iommu group 9
[    1.142471] pci 0000:00:15.1: Adding to iommu group 9
[    1.142483] pci 0000:00:16.0: Adding to iommu group 10
[    1.142497] pci 0000:00:1b.0: Adding to iommu group 11
[    1.142506] pci 0000:00:1c.0: Adding to iommu group 12
[    1.142520] pci 0000:00:1e.0: Adding to iommu group 13
[    1.142527] pci 0000:00:1e.1: Adding to iommu group 13
[    1.142546] pci 0000:00:1f.0: Adding to iommu group 14
[    1.142553] pci 0000:00:1f.3: Adding to iommu group 14
[    1.142561] pci 0000:00:1f.4: Adding to iommu group 14
[    1.142569] pci 0000:00:1f.5: Adding to iommu group 14
[    1.142583] pci 0000:01:00.0: Adding to iommu group 15
[    1.142592] pci 0000:01:00.1: Adding to iommu group 15
[    1.142608] pci 0000:02:00.0: Adding to iommu group 16
[    1.142617] pci 0000:03:00.0: Adding to iommu group 17
[    1.142627] pci 0000:03:01.0: Adding to iommu group 18
[    1.142636] pci 0000:03:02.0: Adding to iommu group 19
[    1.142649] pci 0000:03:03.0: Adding to iommu group 20
[    1.142661] pci 0000:04:00.0: Adding to iommu group 21
[    1.142673] pci 0000:23:00.0: Adding to iommu group 22
[    1.142681] pci 0000:43:00.0: Adding to iommu group 23
[    1.145510] DMAR: Intel(R) Virtualization Technology for Directed I/O
[    1.564068] pci 10000:e0:01.0: Adding to iommu group 6
[    1.564492] pci 10000:e0:01.1: Adding to iommu group 6
[    1.564860] pci 10000:e1:00.0: Adding to iommu group 6
```

qemu 지원 가능 여부
```
$ virt-host-validate
  QEMU: Checking for hardware virtualization                                 : PASS
  QEMU: Checking if device /dev/kvm exists                                   : PASS
  QEMU: Checking if device /dev/kvm is accessible                            : PASS
  QEMU: Checking if device /dev/vhost-net exists                             : PASS
  QEMU: Checking if device /dev/net/tun exists                               : PASS
  QEMU: Checking for cgroup 'cpu' controller support                         : PASS
  QEMU: Checking for cgroup 'cpuacct' controller support                     : PASS
  QEMU: Checking for cgroup 'cpuset' controller support                      : PASS
  QEMU: Checking for cgroup 'memory' controller support                      : PASS
  QEMU: Checking for cgroup 'devices' controller support                     : WARN (Enable 'devices' in kernel Kconfig file or mount/enable cgroup controller in your system)
  QEMU: Checking for cgroup 'blkio' controller support                       : PASS
  QEMU: Checking for device assignment IOMMU support                         : PASS
  QEMU: Checking if IOMMU is enabled by kernel                               : PASS
  QEMU: Checking for secure guest support                                    : WARN (Unknown if this platform has Secure Guest support)
   LXC: Checking for Linux >= 2.6.26                                         : PASS
   LXC: Checking for namespace ipc                                           : PASS
   LXC: Checking for namespace mnt                                           : PASS
   LXC: Checking for namespace pid                                           : PASS
   LXC: Checking for namespace uts                                           : PASS
   LXC: Checking for namespace net                                           : PASS
   LXC: Checking for namespace user                                          : PASS
   LXC: Checking for cgroup 'cpu' controller support                         : PASS
   LXC: Checking for cgroup 'cpuacct' controller support                     : PASS
   LXC: Checking for cgroup 'cpuset' controller support                      : PASS
   LXC: Checking for cgroup 'memory' controller support                      : PASS
   LXC: Checking for cgroup 'devices' controller support                     : FAIL (Enable 'devices' in kernel Kconfig file or mount/enable cgroup controller in your system)
   LXC: Checking for cgroup 'freezer' controller support                     : FAIL (Enable 'freezer' in kernel Kconfig file or mount/enable cgroup controller in your system)
   LXC: Checking for cgroup 'blkio' controller support                       : PASS
   LXC: Checking if device /sys/fs/fuse/connections exists                   : PASS
```


### > 패키지 설치 및 모듈 확인

필수 패키지 설치
```
$ sudo apt update
$ sudo apt install -y qemu-kvm \
                    libvirt-daemon-system libvirt-clients virt-manager \  # vm 관리 매니저
                    bridge-utils build-essential pkg-config git ovmf \
                    autoconf automake libtool flex bison check \    # 빌드 툴
                    libglib2.0-dev libvirt-dev libjson-c-dev \      # libvmi 빌드에 필요한 dev
                    python3 python3-pip python3-dev
```

kvm 모듈 사용자 추가
```
$ lsmod | grep kvm        # KVM 모듈 확인
kvm_intel             487424  0
kvm                  1413120  1 kvm_intel
irqbypass              12288  1 kvm
$ sudo usermod -aG libvirt,kvm $USER    # 현재 사용자를 libvirt, kvm 그룹에 추가해 root 없이 접근
                                        # 나중에 해제하려면 sudo gpasswd -d <USER> <GROUP>
```

vfio 모듈 켜보기(존재 확인)
```
$ sudo modprobe vfio_pci      # 키기
$ lsmod | grep vfio
vfio_pci               16384  0
vfio_pci_core          90112  1 vfio_pci
vfio_iommu_type1       49152  0
vfio                   69632  3 vfio_pci_core,vfio_iommu_type1,vfio_pci
iommufd                98304  1 vfio
irqbypass              12288  2 vfio_pci_core,kvm
```

### dpgu 주소 및 그룹 정보

PCI 주소와 ID 확인
```
$ lspci -nn | grep -i -e vga -e audio   # gpu에 audio 장치가 딸려 있는 경우가 많기 때문에 같이 ID 같이 찾음
0000:00:02.0 VGA compatible controller [0300]: Intel Corporation Device [8086:a788] (rev 04)
0000:00:1f.3 Multimedia audio controller [0401]: Intel Corporation Device [8086:7a50] (rev 11)
0000:01:00.0 VGA compatible controller [0300]: NVIDIA Corporation Device [10de:28e0] (rev a1)
0000:01:00.1 Audio device [0403]: NVIDIA Corporation Device [10de:22be] (rev a1)
```
* PCI 주소? 버스 슬롯 어디에 꽂혀 있나
* 벤더:디바이스 ID? 제조사+모델 코드로 무슨 장치인가
VGA : PCI 주소 01:00.0에 ID 10de:28e0
Audio : PCI 주소 01:00.1에 ID 10de:22be
이게 그룹이 맞는지 iommu_groups을 확인해보자

Iommu_groups 확인
```
$ for g in /sys/kernel/iommu_groups/*; do
  echo "Group $(basename $g):"; ls -l $g/devices; echo; done
Group 15:
total 0
lrwxrwxrwx 1 root root 0  8월  8 16:22 0000:01:00.0 -> ../../../../devices/pci0000:00/0000:00:01.0/0000:01:00.0
lrwxrwxrwx 1 root root 0  8월  8 16:22 0000:01:00.1 -> ../../../../devices/pci0000:00/0000:00:01.0/0000:01:00.1
```
따라서 둘은 15번 그룹이 맞고 이거 두개만 넘기면 됨


### dgpu 분리

커널 파라미터로 vfio가 dpgu 선점
```
$ sudo nano /etc/default/grub
원본
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash"
수정
# iommu=pt 패스스루 장치만 통과 vfio-pci.ids 선점할 PCI ID 지정 
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash intel_iommu=on iommu=pt kvm.ignore__msrs=1 vfio-pci.ids=10de:28e0,10de:22be"
$ sudo update-grub
```

부팅시 vfio 모듈 자동 로드
```
$ sudo nano /etc/modules
vfio
vfio_pci
vfio_iommu_type1
```

VGA BAR 차단 및 해당 드라이버 블랙리스트
```
$ sudo nano /etc/modprobe.d/vfio.conf
options vfio-pci ids=10de:28e0,10de:22be disable_vga=1
blacklist nvidia nvidia_drm nouveau
$ sudo update-initramfs -u
```

재부팅 후 확인
```
$ sudo reboot
$ lspci -nnk -d 10de:28e0
0000:01:00.0 VGA compatible controller [0300]: NVIDIA Corporation Device [10de:28e0] (rev a1)
	Subsystem: Acer Incorporated [ALI] Device [1025:1731]
	Kernel driver in use: vfio-pci
	Kernel modules: nvidiafb, nouveau
```

### vm 생성

iso 다운로드
```
https://www.microsoft.com/software-download/windows10 들어가 영어(미국) 64비트 다운로드. Win10_22H2_English_x64v1.iso. 윈도우 설치를 위한 DVD 역할.
$ sudo mv ~/Downloads/Win10_22H2_English_x64v1.iso /var/lib/libvirt/images/
권한 libvirt-qemu:kkongnyang2
```

vm 생성
```
$ virt-manager
새 vm 만들기
local install media - browse - images에 있는 Win10_22H2_English_x64v1.iso 선택. 윈도우 10.
메모리 16000MB, cpu 10개
저장소 활성화. 디스크 이미지 160.0GB. 이름 win10 (위치 /var/lib/libvirt/images/win10.qcow2)     # win10.qcow2 : 빈 하드디스크.
custom 선택
Overview에서 Q35, UEFI x86_64 : OVMF_CODE_4M.fd 선택
CPUs에서 host-passthruogh 체크 확인
Add hardware - PCI Host Device - 0000:01:00:0과 0000:01:00:1 선택
Display Spice - Spice server, none(내 컴퓨터 안에서만 원격 접속)
완료 후 설치
```

혹시 virtio 드라이버 필요하면
```
https://fedorapeople.org/groups/virt/virtio-win/direct-downloads 들어가 stable - virtio-win-0.1.271.iso 다운로드. virtio 드라이버들 모음.
$ sudo mv ~/Downloads/virtio-win-0.1.271.iso /var/lib/libvirt/images/
권한 libvirt-qemu:kkongnyang2
$ virt-manager
add hardware - storage - images에 있는 virtio-win.iso, cdrom, sata
```

hostdev를 해주긴 했지만 아직 해당 nvidia 드라이버와 looking glass 엔진이 게스트에 없어서 가상 vga 에뮬레이션 = qxl 비디오(가상 gpu) + spice 원격 + virt-viewer로 창 출력 구조임.

윈도우 및 드라이버 설치
```
계정 입력 나오면 shift+f10 누르고 start ms-cxh:localonly. kkongnyang2에 암호 4108
윈도우 업데이트로 드라이버 알아서 설치
device manager 들어가 느낌표 없는지 확인
```