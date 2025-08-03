## GPU 풀 패스스루를 실행해보자

작성자: kkongnyang2 작성일: 2025-08-02

---


### 기본 그래픽 처리 흐름

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


렌더링? 위치 데이터를 transform과 projection하여 화면에 그래픽을 그림
명령 생성: 응용 프로그램
명령 해석: 렌더링 API + GPU 드라이버
실제 연산: 하드웨어 GPU

렌더링 API? 화면에 3D/2D 그래픽을 그리는 일을 도와주는 약속된 함수 집합 도구. drawTriagnles()같은 명령으로 실제로 삼각형을 그림. 정점 데이터를 GPU로 전송, 조명 계산, 픽셀에 색상 결정, GPU에 필요한 메모리 할당, 프레임버퍼 관리.

합성? 여러 윈도우 버퍼(배경, 창, 패널 등)를 하나의 최종 화면 이미지로 조립하여 화면에 실제로 뭐가 보일지 결정

GPU 드라이버? 렌더링 API 명령을 GPU 하드웨어 명령으로 번역

프레임버퍼? 디스플레이로 보낼 최종 합성 픽셀 메모리. 디스플레이 컨트롤러가 읽어간다. gpu 전용 VRAM(video random access memory)에 있기도 하고, DRAM의 일부를 프레임버퍼 용도로 사용하기도 한다. 어쨌거나 cpu가 그 픽셀을 복사하려하지 않고 주소나 레지스터만 업데이트하며 처리한다.

백 버퍼/프런트 버퍼? 그릴 때 쓰는 임시 버퍼 vs 현재 화면 표시 중인 버퍼로 깜빡임을 방지

한줄 요약하면 렌더링을 위해 응용 프로그램이 만든 명령을 cpu가 준비하고, GPU가 대량 픽셀 연산을 수행한 뒤 결과를 메모리 상의 프레임버퍼에 쓴다. 디스플레이 하드웨어가 그 프레임버퍼를 직접 읽어 모니터에 띄운다.

오버레이(Overlay Plane)? 합성 없이 특정 버퍼를 직접 화면 특정 영역에 하드웨어적으로 얹는 기능. 비디오 같은 프로그램은 너무 자주 바뀌어 합성이 힘드니 직접 디스플레이 컨트롤러의 오버레이 플레인에 말을 거는 것이다.


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


### VM 디스플레이

그럼 VM은 어떤 방식으로 진행하냐? 장치를 얼마나 넘겨주냐에 따라 다르다

전체 패스스루 방식이면 (VFIO-PCI)
CPU도 그대로 사용(일부 trap만 따로 처리), RAM도 할당된 영역만큼 그대로 사용, GPU도 드라이버까지 설치해서 직접 사용.

분할 패스스루 방식이면 (GVT-g)
GPU를 넘겨줘서 연산은 하되 최종 프레임버퍼는 호스트가 합성.

가상화 방식이면 (Virtio-gpu)
가짜 GPU를 만들어 호스트가 수신해와 렌더링 연산과 출력을 담당

전통 VGA 방식이면
GPU 사용 없이 오직 그 위에서 장치를 구현해서 진짜 가상으로 연산 처리

ramfb(RAM Framebuffer)는 게스트 os가 메모리에 픽셀 데이터를 쓰면 호스트가 그 메모리를 읽어와 화면에 그대로 표시하는 매커니즘(렌더링x). UEFI(OVMF) 부팅 중에 사용.


### 오버워치 디스플레이 이해

[호스트 Linux] ──(VFIO/PCIe 패스스루)──► [게스트 Windows]
                                               │
                                               │  공식 NVIDIA 드라이버
                                               │  DX11 Feature Level OK
                                               │
                                Overwatch 2 런처 + 안티치트

가상머신 환경이라면 장치를 에뮬레이션 하지만, 오버워치 2에서는 gpu 장치와 드라이버의 ID를 명확히 확인하며 directX 11만 받아들이기 때문에 게스트에 패스스루 해야한다. 리눅스 커널이 제공하는 패스스루용 프레임워크인 VFIO-PCI를 이용해 PCIe 장치들을 안전하게 넘길 수 있다.

* 벤더 ID, DX Feature Level 11_0 이상 & 서명된 WDDM 2.x 조건도 검사
* Hyper-V/VBS OFF 해야지 못알아챔
* VFIO-PCI는 같은 IOMMU 그룹에 묶인 장치는 통째로 넘겨야 함
* 다른 게임은 에뮬레이션인 virtio-gpu(VirGL)만으로 충분하기도 함


### DMA 장치 이해

컴퓨터에는 gpu가 있고, 윈도우에 내장된 드라이버와 api 도구로 그래픽 관련 처리를 한다. cpu가 아닌 gpu로 그래픽을 처리하는 걸 가속이라고 부른다. 또한 cpu와의 전용 고속 버스를 PCIe라고 부르는데, 이를 가지는 장치는 gpu, 사운드카드 등이 있다.

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

### 풀 패스스루 개요

IOMMU 켜고 hostdev 추가 + VFIO 바인딩을 해야 실제 GPU가 넘어간다
참고로 장치는 에뮬레이션(진짜 하드웨어인척 흉내), 파라버추얼(가짜 장치 인터페이스 virtio 제공), 패스스루(하드웨어 건내주기) 세가지 방식이 있다

### 커널 설정

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

### 주소 및 그룹 정보 확인

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


### gpu 장치 제어

hostdev 장치 추가
```
$ virt-manager
하드웨어 추가 - PCI 호스트 장치 - 0000:01:00.0과 0000:01:00.1 추가
```
* 장치 떼기/붙이기의 관리를 managed=yes로 libvirt에게 맡기는 것

hook 스크립트 작성
```
$ sudo mkdir -p /etc/libvirt/hooks    # 디렉토리 확인
$ sudo nano /etc/libvirt/hooks/qemu   # qemu 파일 생성
#!/usr/bin/env bash
# /etc/libvirt/hooks/qemu
set -euo pipefail

VM="$1"
HOOK="$2"
PHASE="$3"

GPU="0000:01:00.0"
AUD="0000:01:00.1"
HOSTDRV="nvidia"

DM_LIST=("gdm3" "gdm" "sddm" "lightdm")
DM_STATE_FILE="/run/libvirt-active-dm"

log(){ echo "[$(date +'%F %T')] $*" >> /var/log/libvirt/qemu-hook.log; }

stop_dm() {
  for dm in "${DM_LIST[@]}"; do
    if systemctl is-active --quiet "$dm"; then
      systemctl stop "$dm"
      echo "$dm" > "$DM_STATE_FILE"
      break
    fi
  done
}

start_dm() {
  if [[ -f "$DM_STATE_FILE" ]]; then
    dm="$(cat "$DM_STATE_FILE")"
    systemctl start "$dm" || true
    rm -f "$DM_STATE_FILE"
  fi
}

unbind_vtconsoles() {
  for c in /sys/class/vtconsole/*; do
    [[ -e "$c/bind" ]] && echo 0 > "$c/bind" || true
  done
  echo efi-framebuffer.0        > /sys/bus/platform/drivers/efi-framebuffer/unbind        2>/dev/null || true
  echo simple-framebuffer.0    > /sys/bus/platform/drivers/simple-framebuffer/unbind     2>/dev/null || true
  echo simpledrm.0             > /sys/bus/platform/drivers/simpledrm/unbind              2>/dev/null || true
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
      modprobe nvidia 2>/dev/null || true
      modprobe nvidia_uvm 2>/dev/null || true
      modprobe nvidia_modeset 2>/dev/null || true
      modprobe nvidia_drm 2>/dev/null || true
      ;;
    amdgpu)
      modprobe amdgpu 2>/dev/null || true
      ;;
  esac
}

load_vfio() {
  for m in vfio vfio-pci vfio_iommu_type1; do
    modprobe -n "$m" &>/dev/null && modprobe "$m" || true
  done
}

case "${HOOK}/${PHASE}" in
  prepare/begin)
    log "[$VM] prepare/begin: stop DM, unbind fb, unload $HOSTDRV, load vfio"
    stop_dm
    unbind_vtconsoles
    unload_host_driver
    load_vfio
    ;;

  release/end|stopped/end)
    log "[$VM] release/stopped: reload $HOSTDRV, start DM"
    reload_host_driver
    start_dm
    ;;

  *)
    log "[$VM] other phase: $HOOK / $PHASE $4"
    ;;
esac

exit 0

$ sudo chmod +x /etc/libvirt/hooks/qemu     # 저장 후 실행권한
$ sudo mv /etc/libvirt/hooks/qemu ~/dd      # 잠깐 무효화할때
$ sudo mv ~/dd /etc/libvirt/hooks/qemu      # 다시 활성화
```
* hook 스크립트란 vm이 시작/종료되기 직전 직후에 libvirt가 자동으로 실행해주는 스크립트
* 이 안에서 vfio 모듈로 장치 건네주기/언바인딩/바인딩 코드를 짜주고 호스트의 gui/dm 도 내려준다
* #!는 이 파일을 어떤 인터프리터로 실행할지임

### vm에 정보 전달

rom 파일 추가
```
$ sudo mkdir -p /usr/share/vgabios      # 디렉토리 생성
$ echo 1 | sudo tee /sys/bus/pci/devices/0000:01:00.0/rom     # rom 읽기 허용
$ sudo dd if=/sys/bus/pci/devices/0000:01:00.0/rom of=/usr/share/vgabios/rtx4060.rom bs=1K
$ sudo chown root:root /usr/share/vgabios/rtx4060.rom
$ sudo chmod 644 /usr/share/vgabios/rtx4060.rom
$ echo 0 | sudo tee /sys/bus/pci/devices/0000:01:00.0/rom   # rom 잠금
$ virsh edit win10
gpu hostdev 안에 source 블록 다음에 두는 것이 관례
<rom file='/usr/share/vgabios/rtx4060.rom'/>
```

### > 원격

이렇게 패스스루를 시키면 호스트 화면이 없으므로 ssh 원격컴으로 vm을 키고 끄자

ssh 실행
```
# 내컴에서
$ sudo apt install openssh-server
$ sudo systemctl start ssh
$ sudo ufw allow 22
```

ssh 원격
```
# 원격컴에서
$ ssh kkongnyang2@172.30.1.69
비밀번호 4108 입력
$ virsh list --all        # vm 리스트보기
$ virsh start win10       # vm 키기
$ virsh shutdown win10    # vm 끄기
$ virsh destroy win10     # 강제 파워내리기
$ exit                    # ssh 연결 종료
```

### 해결

호스트 화면은 잘 꺼지지만 게스트 화면이 켜지질 않음. 로그를 살펴보자

hook 스크립트 로그: /var/log/libvirt/qemu-hook.log
VM별 QEMU 로그: /var/log/libvirt/qemu/win10.log
libvirt 데몬 로그: journalctl -u virtqemud -u libvirtd -f
커널/VFIO 메시지: dmesg | grep -i vfio
```
vfio-pci ... resetting … reset done
NVRM: GPU 0000:01:00.0 is already bound to vfio-pci.
```
정상 작동.

휴대폰 플래시로 비춰보니 화면이 보임. 패널 백라이트 문제임을 알 수 있음. 왜 패널 백라이트가 안켜지냐?
igpu가 없고 dgpu만 있는 모델은 프레임버퍼가 dpgu 전용 vram에만 있게 되고 패널 eDP 신호선 또한 dpgu display port 엔진에만 물려 있다. 또한 노트북에서는 주로 EC(Embedded Controller)가 밝기를 조절하는 컨트롤러인데, ROM에 EC 펌웨어가 탑재되어 있으며 ACPI라는 테이블을 통해 이걸 os로 넘겨주게 된다. 하지만 qemu 부팅에서는 EC 관련 ACPI를 생성하지 않기 때문에 백라이트를 킬 수 없는 것이다. 그러나 요즘 패널에는 DPCD 주소로 AUX 채널(DisplayPort 배선에 포함된 링크 제어 및 백라이트 조정 채널)을 조절할 수 있게 해놨기에 EC 없이도 조절이 가능하다.
geforce rtx 4060 game ready 드라이버 552.12 WHQL 계열부터 NVAPI를 통해 eDP AUX 경로(DPCD 레지스터 0x720/0x722/0x723)로 밝기 값을 쓰는 게 가능하다 적혀 있으므로 이걸 이용하자.

게스트 spice 원격화면 접속
```
# 원격컴에서
virt-manager.org/download 들어가서 virt-viewer 카테고리에서 win x64 msi 다운받기
$ virsh start win10
$ virsh domdisplay win10
spice://127.0.0.1:5900
다른 cmd $ ssh -N -L 5900:127.0.0.1:5900 kkongnyang2@172.30.1.69   # 내부 서비스 접속 가능한 명령어
C:\Program Files\VirtViewer v12\bin 들어가서 remote-viewer.exe 클릭
spice://127.0.0.1:5900 연결
```
* spice? 가상 머신 원격 접속 프로토콜


게스트에서 nvidia 드라이버 설치
```
geforce - geforce RTX 40 series (notebooks) - geforce RTX 4060 laptop GPU - window 10 64-bit - English(US) - geforce game ready driver
```

게스트에서 control 설치

libvirt xml 튜닝
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







