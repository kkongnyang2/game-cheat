## VMI를 실행해보자

### 목표: VMI 구현
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


### > VMI를 위해 필요한 게스트 정보

[VMI 필요정보]
  ├─ 식별: (domain name | domid | qemu pid)
  ├─ 주소변환: DTB (CR3)
  ├─ 해석: 프로파일(json) - 심볼 주소와 구조체 필드 오프셋
  ├─ ostype, KASLR 여부, 권한
  └─ (libvirt 쓰면 대부분 자동 처리 쉬움)


libvirt로 vm을 만들어야 하는 이유?
가장 쉬운 길이기 때문. libvmi가 libvirt API로 domid, 메모리 맵, DTB를 자동으로 가져옴. 직접 qemu로 실행한 vm는 qemu pid를 써서 /proc/<pid>/mem 등을 직접 매핑해야 함

(1) VM 식별을 위한 domain name
libvirt 사용 시 domain name만 넣으면 libvmi가 domain ID나 qemu PID 알아서

* domain name : libvirt에서 vm을 구분하는 문자열 이름. 사용자가 직접 정함
* domain ID : libvirt가 실행 중인 vm에 부여하는 일시적 런타임 ID. virsh domid <name> 로 확인.
* qemu PID : qemu 프로세스의 pid. ps aux | grep qemu-system-x86_64 같은 걸로 확인

(2) 주소 변환을 위한 DTB (Directory Table Base)
게스트 가상 주소(VA)를 물리 주소(PA)로 바꿀 때 필요한 페이지 테이블 루트 물리리 주소. x86-64에서 CR3 레지스터가 가리킨다.
libvmi가 자동으로 찾아주기도 하지만, 실패하면 직접 지정
리눅스는 task_struct->mm->pgd에서 뽑고, 윈도우면 EPROCESS.DirectoryTableBase에서 뽑는다.

(3) 의미 있는 해석을 위한 프로파일(json 형식)
심볼 주소와 커널 구조체 필드 오프셋 정보.
리눅스면 System.map(공개 심볼 주소) + vmlinux를 바탕으로 dwarf2json(vilatility) 툴을 이용해 만든다.
윈도우면 PDB(program database)를 바탕으로 volatility/rekall/drakpdb 툴을 이용해 만든다.

(4) 그밖에도 KASLR(커널 주소 랜덤화. libvmi가 DTB를 찾기 어렵게 함) 사용 여부(linux_kaslr=true), 권한(root), /etc/libvmi.conf 접근성 들이 필요하다.

### > Part 0. 가상화 지원 확인

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

### > Part 1. libvirt로 VM 생성

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

iso 다운로드
```
https://www.microsoft.com/software-download/windows10 들어가 영어(미국) 64비트 다운로드. Win10_22H2_English_x64v1.iso
https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/ 들어가 stable-virtio/virtio-win-0.1.271.iso 다운
```

vm 만들기
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

원격 ssh 활성화
```
$ sudo apt install openssh-server
$ sudo systemctl start ssh
$ sudo ufw allow 22
```

타 노트북에서 vm 작동시키기
```
$ ssh -V        # openSSH 클라이언트 설치 여부 확인
OpenSSH_for_Windows_9.5p1, LibreSSL 3.8.2
$ ssh kkongnyang2@172.30.1.69
비밀번호 4108 입력
$ virsh list --all
$ virsh start win10
$ virsh shutdown win10
$ virsh destroy win10     # 강제 파워내리기
$ exit                    # ssh 연결 종료
```

### > Part 2. VM 튜닝

GPU 패스스루
IOMMU 켜고 VFIO 바인딩 + hostdev 추가를 해야 실제 GPU가 넘어간다
참고로 장치는 에뮬레이션(진짜 하드웨어인척 흉내), 파라버추얼(가짜 장치 인터페이스 virtio 제공), 패스스루(하드웨어 건내주기) 세가지 방식이 있다

grub에서 IOMMU 전체 켜기
```
$ sudo nano /etc/default/grub
원본
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash"
수정
# iommu=pt 패스스루 장치만 통과 video=efifb:off 호스트가 gpu 기본 프레임버퍼 잡는 것 방지
GRUB_CMDLINE_LINUX_DEFAULT="quiet splash amd_iommu=on iommu=pt video-efifb:off"
$ sudo update-grub
$ reboot
```

PCI 주소와 ID 확인
```
$ lspci -nn | grep -i -e vga -e audio   # gpu에 audio 장치가 딸려 있는 경우가 많기 때문에 같이 ID 같이 찾음
01:00.0 VGA compatible controller [0300]: NVIDIA Corporation AD107M [GeForce RTX 4060 Max-Q / Mobile] [10de:28e0] (rev a1)
01:00.1 Audio device [0403]: NVIDIA Corporation Device [10de:22be] (rev a1)
05:00.5 Multimedia controller [0480]: Advanced Micro Devices, Inc. [AMD] ACP/ACP3X/ACP6x Audio Coprocessor [1022:15e2] (rev 60)
05:00.6 Audio device [0403]: Advanced Micro Devices, Inc. [AMD] Family 17h/19h HD Audio Controller [1022:15e3]
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
Group 14:
합계 0
lrwxrwxrwx 1 root root 0  7월 26 10:11 0000:01:00.0 -> ../../../../devices/pci0000:00/0000:00:01.1/0000:01:00.0
lrwxrwxrwx 1 root root 0  7월 26 10:11 0000:01:00.1 -> ../../../../devices/pci0000:00/0000:00:01.1/0000:01:00.1
```
따라서 둘은 14번 그룹이 맞고 이거 두개만 넘기면 됨

VM xml 튜닝
```
$ virt-manager
하드웨어 추가 - PCI 호스트 장치 - 0000:01:00.0과 0000:01:00.1 추가
xml 확인해서 <devices>에
  <devices>
    <hostdev mode="subsystem" type="pci" managed="yes">
      <source>
        <address domain="0x0000" bus="0x01" slot="0x00" function="0x0"/>
      </source>
      <address type="pci" domain="0x0000" bus="0x05" slot="0x00" function="0x0"/>
    </hostdev>
    <hostdev mode="subsystem" type="pci" managed="yes">
      <source>
        <address domain="0x0000" bus="0x01" slot="0x00" function="0x1"/>
      </source>
      <address type="pci" domain="0x0000" bus="0x06" slot="0x00" function="0x0"/>
    </hostdev>
  </devices>
이 내용 추가됐는지 확인
```
* 장치 떼기/붙이기의 관리를 managed=yes로 libvirt에게 맡기는 것

hook 스크립트 작성
```
$ sudo mkdir -p /etc/libvirt/hooks    # 디렉토리 확인
$ sudo nano /etc/libvirt/hooks/qemu   # qemu 파일 생성
#!/usr/bin/env bash
# /etc/libvirt/hooks/qemu
# Args: $1 = VM name, $2 = hook-name, $3 = sub-operation, $4 = extra
set -euo pipefail

VM="$1"
HOOK="$2"
PHASE="$3"

GPU="0000:01:00.0"
AUD="0000:01:00.1"
HOSTDRV="nvidia"      # 또는 amd

DM_LIST=("gdm" "gdm3" "sddm" "lightdm")

stop_dm() {
  for dm in "${DM_LIST[@]}"; do
    if systemctl is-active --quiet "$dm"; then
      systemctl stop "$dm"
      export ACTIVE_DM="$dm"
      break
    fi
  done
}

start_dm() {
  if [[ -n "${ACTIVE_DM-}" ]]; then
    systemctl start "$ACTIVE_DM"
  fi
}

unbind_vtconsoles() {
  for c in /sys/class/vtconsole/*; do
    [[ -e "$c/bind" ]] && echo 0 > "$c/bind" || true
  done
  echo efi-framebuffer.0   > /sys/bus/platform/drivers/efi-framebuffer/unbind   2>/dev/null || true
  echo simple-framebuffer.0 > /sys/bus/platform/drivers/simple-framebuffer/unbind 2>/dev/null || true
}

unload_host_driver() {
  case "$HOSTDRV" in
    nvidia)
      modprobe -r nvidia_drm nvidia_modeset nvidia_uvm nvidia 2>/dev/null || true
      ;;
    amdgpu)
      modprobe -r amdgpu 2>/dev/null || true
      ;;
  esac
}

reload_host_driver() {
  case "$HOSTDRV" in
    nvidia)
      modprobe nvidia nvidia_uvm nvidia_modeset nvidia_drm 2>/dev/null || true
      ;;
    amdgpu)
      modprobe amdgpu 2>/dev/null || true
      ;;
  esac
}

case "${HOOK}/${PHASE}" in
  # VM이 시작되기 직전
  prepare/begin)
    # 1) GUI/DM 내려서 GPU 점유 해제
    stop_dm
    # 2) vtconsole/프레임버퍼 분리(있다면)
    unbind_vtconsoles
    # 3) 호스트 GPU 드라이버 언로드
    unload_host_driver
    # 4) vfio 관련 모듈 로드 (안 돼 있으면)
    modprobe vfio vfio-pci vfio_iommu_type1
    ;;

  # VM 종료 직후 (장치 reattach 완료 후)
  release/end)
    # 1) VFIO에서 빠진 GPU를 원래 드라이버에 재바인딩( managed=yes면 libvirt가 이미 함 )
    #    → 여기선 보통 필요 없음. 혹시 안 됐다면 수동 bind 코드 추가.
    # 2) 호스트 드라이버 다시 로드
    reload_host_driver
    # 3) DM 다시 올리기
    start_dm
    ;;
esac

exit 0
$ sudo chmod +x /etc/libvirt/hooks/qemu     # 저장 후 실행권한
```
* hook 스크립트란 vm이 시작/종료되기 직전 직후에 libvirt가 자동으로 실행해주는 스크립트
* #!는 이 파일을 어떤 인터프리터로 실행할지임


```
systemctl stop gdm
modprobe -r nvidia
sudo modprobe vfio-pci
sudo modprobe vfio_iommu_type1
echo 10de 28e0 | sudo tee /sys/bus/pci/drivers/vfio-pci/new_id
echo 10de 22be | sudo tee /sys/bus/pci/drivers/vfio-pci/new_id
```
* 저 ID 장치를 지원 목록 new_id에 추가하라
* 지원 목록 추가일뿐, 실제 unbind/bind는 libvirt yml 에서 managed 로 넘겨주면 알아서 해줌

libvirt에서 추가
```
편집 - 가상 머신 정보 - i아이콘 - 하드웨어 추가 - PCI 호스트 장치 - 01:00.0과 01:00.1 선택
```

```
사용자 편집란 체크
하드웨어 추가 - 사용자 정의 저장소 - 다운로드에 있는 virtio-win-0.1.271.iso 선택 - 장치유형 CDROM, 버스유형 SATA로
그럼 이제 사타 CDROM 두개가 잘보임
CD나 DVD로 부팅 뜨면 바로 엔터
```

libvirt xml 튜닝
* cpu passthrough 키고 memballon 끌거임
```
$ virsh edit win10
1번 방식 선택
원본
    <memballoon model='virtio'>
      <address type='pci' domain='0x0000' bus='0x04' slot='0x00' function='0x0'/>
    </memballoon>
수정
    <memballoon model='none'/>
```



게스트에서 nvidia 드라이버 설치



### > libvmi

libvmi 설치
```
git clone https://github.com/libvmi/libvmi.git
cd libvmi
sudo apt install -y build-essential autoconf automake libtool pkg-config \
                    libglib2.0-dev libvirt-dev libjson-c-dev \
                    flex bison check
./autogen.sh
./configure --enable-kvm --disable-xen --enable-json
make -j$(nproc)
sudo make install
sudo ldconfig  # 새로 설치된 라이브러리 경로 갱신
make examples
# 예: ./examples/vmi-process-list <config_key>
```

/etc/libvmi.conf 작성
```
<keyname> {
    ostype = "Linux";               # 대상 OS 타입 (Linux 또는 Windows 등)
    domain = "<libvirt 도메인 이름>"; # virt-manager/virsh에서 쓰는 VM 이름
    # 또는 domid = <정수>;           # virsh domid로 확인한 숫자 ID

    # 주소 공간 변환 관련
    sysmap = "/path/to/System.map-6.8.0-31-generic";  # 커널 심볼 파일
    rekall_profile = "/path/to/linux-6.8.0-31.json";  # (선택) Volatility/rekall JSON 프로파일

    # DTB 지정 가능 (일반적으로 자동 탐지 시 생략)
    # dtb = 0x00000000f1234000;

    # Linux KASLR 사용시
    linux_kaslr = true;

    # (윈도우의 경우) win_...
    # ostype = "Windows";
    # win_profile = "/path/to/windows_profile.json";

    # 기타 옵션
    # kvm = 1;   # KVM 백엔드 사용 (신규 libvmi는 domain 지정만으로 자동)
    # shm_file = "/dev/shm/somefile"; # 공유 메모리 파일 (특수 상황)
}
```
권한 설정
```
sudo chown root:root /etc/libvmi.conf
sudo chmod 644 /etc/libvmi.conf
```
테스트
```
virsh list    # ub24-test 같은 VM이 running 상태
```

### > 프로파일 만들기

의미 있는 해석을 위해 게스트 os의 심볼과 구조체 정보를 가져오는 역할. PDB라는 데이터베이스를 받아와 이를 이용해 프로파일을 만든다. 사용하는 툴은 Volatiliy/Rekall/drakpdb 등. 이걸 LibVMI에 넣어줘야 잘 작동한다

         [PDB / DWARF]  <-- 디버그/타입 정보 (메모리 안 X)
                |
        (parse & convert)
                |
   +------------------------------+
   |  Volatility / Rekall / drakpdb |
   +------------------------------+
                |
           [JSON Profile]
                |
          /etc/libvmi.conf
                |
             LibVMI
         (live memory access)


* DRAKVUF sandbox라는 전체 자동화 툴이 있음. 근데 intel cpu + 우분투 22.04 + xen 전용임.

윈도우 커널(ntoskrnl.exe)의 필드 번호와 정확히 맞는 PDB를 microsoft symbol server에서 받아야 구조체 오프셋이 일치한다. winver으로 확인할 것.
C:\Windows\System32\ntoskrnl.exe 같은 커널 바이너리를 호스트로 가져옴.
PDB GUID/AGE를 추출하거나 symchk, dbghelp.dll을 이용해서 Microsoft Symbol Server에서 해당 PDB를 다운로드.
pdbparse, rekall/tools/windows/pefile.py, drakvuf/scripts/pdb2json.py 등으로 변환하여 JSON/rekall 프로파일 생성.
(버전이 맞으면 DRAKVUF, LibVMI 커뮤니티에서 이미 만들어둔 프로파일을 가져오는 방법도 있음)
/etc/libvmi.conf에 win_profile = "/path/to/windows-<build>-<arch>.json" 같은 식으로 지정.

참고로 volatility3은 PDB를 바로 쓰기도 해서, 거기서 심볼/오프셋을 확인하고 libvmi config에 필요한 주소만 일부 사용하는 혼합 방식도 가능. rekall은 자체 json 프로파일 형식을 사용.


### > pyvmi