## IGPU 없을때 DGPU 풀 패스스루 vm을 만들자

작성자: kkongnyang2 작성일: 2025-08-02

---

igpu 없이 dgpu 하나여도, 호스트에서 떼고 게스트로 넘기는 법

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
혹은 ovmf로 부팅해도 백라이트 문제가 해결된다

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


> ⚠️ 실패. 호스트를 못봄 + 백라이트 문제로 게스트 화면도 안보임. 노트북 교체