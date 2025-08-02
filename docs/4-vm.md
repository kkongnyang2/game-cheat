## qemu-kvm을 실행해보자

### 목표: VM 생성
작성자: kkongnyang2 작성일: 2025-07-24

---

노트북 교체로 빠르게 처음부터.(24.04)

### > 하이퍼바이저 선택

|       하이퍼바이저       | VMI 친화도 | 장점                                          | 단점                           | 대표 툴/프로젝트                   |
| :----------------: | :-----: | :------------------------------------------ | :--------------------------- | :-------------------------- |
|       **Xen**      |  ★★★★★  | LibVMI 원조급, altp2m 기반 메모리 이벤트, DRAKVUF 등 풍부 | 설정/운영 복잡, 최신 HW 지원/호환성 이슈    | **DRAKVUF**, LibVMI         |
|    **KVM/QEMU**    |  ★★★★☆  | 리눅스 기본, libvirt 연동 쉽고 널리 사용                 | 이벤트 트랩 구현 시 Xen보다 손이 더 감     | LibVMI, PyVMI               |
|   **VMware ESXi**  |  ★★★★☆  | 공식 introspection API, 안정적                   | 상용/폐쇄적, 비용, 자유도 제한           | VMware VMI SDK (Thin agent) |
|     **Hyper-V**    |  ★★☆☆☆  | Windows 친화, 커널 레벨 접근 가능                     | 공개 도구 적음, 구현 난이도             | 자체 개발 필요                    |
|   **VirtualBox**   |  ★☆☆☆☆  | 셋업 쉬움                                       | VMI용 API 사실상 없음, hacky 작업 필요 | 직접 /proc/…/mem 접근 등 “꼼수”    |
| **Bareflank/ACRN** |  ★★☆☆☆  | 경량/오픈소스, 연구 목적 좋음                           | 문서/커뮤니티 작음, 모든 기능 직접 구현      | 커스텀 하이퍼바이저 개발               |

메모리 이벤트 트랩이면
Xen + LibVMI (DRAKVUF 구조 참고)
libvirt 없이 Xen 자체 툴(xl) 사용. altp2m로 페이지 권한 바꿔 트랩 잡기 수월.

이미 우분투에 kvm 환경이라면
KVM/QEMU + LibVMI
libvirt로 편하게 관리 하고, 문서 많은 libvmi/pyvmi로.

* kvm(kernel based virtual machine) 리눅스 커널 모듈. VT-x를 사용해서 게스트 cpu 실행을 커널 레벨에서 가속해줌. qemu는 가상머신 하이퍼바이저.

윈도우 환경에서 hyper-v로 작업하면 바로 오버워치2 안티치트에 탐색


### > 가상화 지원 확인

cpu 가상화 지원 여부
```
$ egrep -c '(vmx|svm)' /proc/cpuinfo    # 0이 아니어야 함. UEFI에서 Intel VT-x/AMD-V 활성화 필요
16            # 논리 cpu 16개 모두가 AMD-V 플래그를 갖고 있다는 뜻
```
장치 가상화 지원 여부
```
$ virt-host-validate    # 커널에 관련 지원이 있는지 확인
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
IOMMU 존재 여부
```
$ sudo dmesg | grep -i -e iommu -e dmar
[sudo] kkongnyang2 암호: 
[    0.416612] iommu: Default domain type: Translated
[    0.416612] iommu: DMA domain TLB invalidation policy: lazy mode
[    0.548245] pci 0000:00:00.2: AMD-Vi: IOMMU performance counters supported
[    0.548333] pci 0000:00:00.0: Adding to iommu group 0
...
[    0.549089] pci 0000:06:00.4: Adding to iommu group 26
[    0.555040] perf/amd_iommu: Detected AMD IOMMU #0 (2 banks, 4 counters/bank).
```

### > libvirt로 VM 생성

[사용자]
   ├─ virt-manager (GUI)
   ├─ virsh        (CLI)
   └─ virt-install, virt-viewer 등

          ↓ (libvirt API 호출)

[libvirt 데몬/드라이버들]
   ├─ QEMU/KVM 드라이버
   ├─ LXC, Xen, VMware 등 다른 드라이버
   └─ 스토리지/네트워크 관리 모듈

          ↓

[실제 하이퍼바이저 & 커널 기능]
   ├─ qemu-system-xxx + KVM
   ├─ tap/bridge/ovn 등 네트워크
   └─ LVM/iscsi/nbd 등 스토리지


필수 패키지 설치
```
$ sudo apt update
$ sudo apt install -y qemu-kvm \
                    libvirt-daemon-system libvirt-clients virt-manager \  # vm 관리 매니저
                    bridge-utils build-essential pkg-config git \
                    autoconf automake libtool flex bison check \    # 빌드 툴
                    libglib2.0-dev libvirt-dev libjson-c-dev \      # libvmi 빌드에 필요한 dev
                    python3 python3-pip python3-dev
```

확인 및 유저 추가
```
$ lsmod | grep kvm        # KVM 모듈 올라와있는지 확인
kvm_amd               245760  0
kvm                  1425408  1 kvm_amd
irqbypass              12288  1 kvm
ccp                   155648  1 kvm_amd
$ sudo usermod -aG libvirt,kvm $USER    # 현재 사용자를 libvirt, kvm 그룹에 추가해 root 없이 접근
                                        # 나중에 해제하려면 sudo gpasswd -d <USER> <GROUP>
$ reboot
```


윈도우 이미지 설치를 위해선 두 개의 iso 파일과 빈 하드디스크 하나가 필요하다.

win10_22H2_English_x64.iso : 부팅 가능한 DVD 역할. vm이 이걸로 첫 부팅하며 윈도우 setup 실행.
virtio-win.iso : 드라이버 CD 역할. setup이 이 viostor.inf와 NetKVM.inf 를 읽어 virtio 가상 HDD를 인식하도록 함
win10.qcow2 : 빈 하드디스크. setup이 여기 파티션을 만들고 윈도우 파일을 복사하여 재부팅이 끝나면 윈도우가 들어 있는 디스크가 됨.

iso 다운로드
```
https://www.microsoft.com/software-download/windows10 들어가 영어(미국) 64비트 다운로드. Win10_22H2_English_x64v1.iso
https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/ 들어가 stable-virtio/virtio-win-0.1.271.iso 다운
```

vm 생성
```
$ virt-manager
새 vm 만들기
로컬 설치 선택 - 로컬 찾기 - 다운로드에 있는 Win10_22H2_English_x64v1.iso 선택. 윈도우 10.
메모리 12000MB, cpu 8개
저장소 활성화. 디스크 이미지 120.0GB. 이름 win10 (위치 /var/lib/libvirt/images/win10.qcow2)
vm 실행
윈도우 설치
계정 입력 나오면 shift+f10 누르고 start ms-cxh:localonly. kkongnyang2에 암호 4108
잘작동하면 vm 종료
cdrom 삭제
```

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