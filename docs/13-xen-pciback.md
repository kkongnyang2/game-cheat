## xen 방식으로 vmi를 구현해보자

작성자: kkongnyang2 작성일: 2025-08-17

---

### 이번 세팅

bios 설정
```
VT-x, VT-d 켜기
Secure boot 끄기(서명 문제 회피에 유리. 바로 안꺼지면 바이오스 비번 설정한 후 끈 후 비번 다시 없애면 됨)
vmd controller 끄기(advanced-main에서 ctrl+s 눌러 히든옵션 띄우면 됨)
display mode : auto select
```
* optimus = igpu 직결, dgpu 도움
* dgpu only = dgpu 직결
* auto select = nvidia 드라이버가 mux를 조정해 위 두개 중에 자동선택

설치 시 드라이버 두개 다 미체크
```
$ lspci -nnk | grep -A3 -Ei 'vga|3d|display|Audio'
00:02.0 VGA compatible controller [0300]: Intel Corporation Raptor Lake-S UHD Graphics [8086:a788] (rev 04)
	Subsystem: Acer Incorporated [ALI] Raptor Lake-S UHD Graphics [1025:1731]
	Kernel driver in use: i915
	Kernel modules: i915, xe
--
00:1f.3 Multimedia audio controller [0401]: Intel Corporation Raptor Lake High Definition Audio Controller [8086:7a50] (rev 11)
	Subsystem: Acer Incorporated [ALI] Device [1025:1731]
	Kernel driver in use: sof-audio-pci-intel-tgl
	Kernel modules: snd_hda_intel, snd_soc_avs, snd_sof_pci_intel_tgl
00:1f.4 SMBus [0c05]: Intel Corporation Raptor Lake-S PCH SMBus Controller [8086:7a23] (rev 11)
	Subsystem: Acer Incorporated [ALI] Device [1025:1731]
--
01:00.0 VGA compatible controller [0300]: NVIDIA Corporation AD107M [GeForce RTX 4060 Max-Q / Mobile] [10de:28e0] (rev a1)
	Subsystem: Acer Incorporated [ALI] AD107M [GeForce RTX 4060 Max-Q / Mobile] [1025:1731]
	Kernel driver in use: nouveau
	Kernel modules: nvidiafb, nouveau
01:00.1 Audio device [0403]: NVIDIA Corporation Device [10de:22be] (rev a1)
	Subsystem: Acer Incorporated [ALI] Device [1025:1731]
	Kernel driver in use: snd_hda_intel
	Kernel modules: snd_hda_intel
```

### xen ubuntu 만들기

xen 설치 및 dpgu 하이딩
```
sudo apt install -y xen-system-amd64 ovmf qemu-system-x86

echo 'GRUB_CMDLINE_XEN="iommu=1 iommu=verbose dom0_mem=4096M,max:4096M"' | sudo tee /etc/default/grub.d/xen.cfg

echo 'GRUB_CMDLINE_LINUX_XEN_REPLACE="xen-pciback.hide=(01:00.0)(01:00.1) modprobe.blacklist=nvidia,nvidia_drm,nvidia_modeset,nouveau"' \
| sudo tee -a /etc/default/grub.d/xen.cfg

sudo update-grub
```

xen ubuntu로 들어가기
```
$ sudo xl info | head
host                   : acer
release                : 6.14.0-27-generic
version                : #27~24.04.1-Ubuntu SMP PREEMPT_DYNAMIC Tue Jul 22 17:38:49 UTC 2
machine                : x86_64
nr_cpus                : 20
max_cpu_id             : 19
nr_nodes               : 1
cores_per_socket       : 10
threads_per_core       : 2
cpu_mhz                : 2803.195

$ sudo xl list
Name                                        ID   Mem VCPUs	State	Time(s)
Domain-0                                     0  4096    20     r-----     415.7

$ cat /sys/hypervisor/type
xen
```

### pciback 활성화

pciback 모듈 활성화
```
$ lsmod | grep xen_pciback || sudo modprobe xen_pciback
$ lspci -nnk | grep -A3 -Ei 'vga|3d|display|Audio'
00:02.0 VGA compatible controller [0300]: Intel Corporation Raptor Lake-S UHD Graphics [8086:a788] (rev 04)
	Subsystem: Acer Incorporated [ALI] Raptor Lake-S UHD Graphics [1025:1731]
	Kernel driver in use: i915
	Kernel modules: i915, xe
--
00:1f.3 Multimedia audio controller [0401]: Intel Corporation Raptor Lake High Definition Audio Controller [8086:7a50] (rev 11)
	Subsystem: Acer Incorporated [ALI] Device [1025:1731]
	Kernel driver in use: sof-audio-pci-intel-tgl
	Kernel modules: snd_hda_intel, snd_soc_avs, snd_sof_pci_intel_tgl
00:1f.4 SMBus [0c05]: Intel Corporation Raptor Lake-S PCH SMBus Controller [8086:7a23] (rev 11)
	Subsystem: Acer Incorporated [ALI] Device [1025:1731]
--
01:00.0 VGA compatible controller [0300]: NVIDIA Corporation AD107M [GeForce RTX 4060 Max-Q / Mobile] [10de:28e0] (rev a1)
	Subsystem: Acer Incorporated [ALI] AD107M [GeForce RTX 4060 Max-Q / Mobile] [1025:1731]
	Kernel driver in use: pciback
	Kernel modules: nvidiafb, nouveau
01:00.1 Audio device [0403]: NVIDIA Corporation Device [10de:22be] (rev a1)
	Subsystem: Acer Incorporated [ALI] Device [1025:1731]
	Kernel driver in use: snd_hda_intel
	Kernel modules: snd_hda_intel
```
* 오디오 장치를 아직도 pciback이 아니다. 언바인드 후 바인드를 해주겠다.

오디오 언바인드 후 바인드
```
$ DRVDIR=$(ls -d /sys/bus/pci/drivers/pciback /sys/bus/pci/drivers/xen-pciback 2>/dev/null | head -n1)
$ echo "$DRVDIR"
/sys/bus/pci/drivers/pciback
$ echo 0000:01:00.1 | sudo tee /sys/bus/pci/devices/0000:01:00.1/driver/unbind
0000:01:00.1
$ echo "10de 22be" | sudo tee "$DRVDIR/new_id"
10de 22be
tee: /sys/bus/pci/drivers/pciback/new_id: File exists
$ echo 0000:01:00.1 | sudo tee "$DRVDIR/bind"
0000:01:00.1
$ lspci -nnk | grep -A3 -Ei 'vga|3d|display|Audio'
00:02.0 VGA compatible controller [0300]: Intel Corporation Raptor Lake-S UHD Graphics [8086:a788] (rev 04)
	Subsystem: Acer Incorporated [ALI] Raptor Lake-S UHD Graphics [1025:1731]
	Kernel driver in use: i915
	Kernel modules: i915, xe
--
00:1f.3 Multimedia audio controller [0401]: Intel Corporation Raptor Lake High Definition Audio Controller [8086:7a50] (rev 11)
	Subsystem: Acer Incorporated [ALI] Device [1025:1731]
	Kernel driver in use: sof-audio-pci-intel-tgl
	Kernel modules: snd_hda_intel, snd_soc_avs, snd_sof_pci_intel_tgl
00:1f.4 SMBus [0c05]: Intel Corporation Raptor Lake-S PCH SMBus Controller [8086:7a23] (rev 11)
	Subsystem: Acer Incorporated [ALI] Device [1025:1731]
--
01:00.0 VGA compatible controller [0300]: NVIDIA Corporation AD107M [GeForce RTX 4060 Max-Q / Mobile] [10de:28e0] (rev a1)
	Subsystem: Acer Incorporated [ALI] AD107M [GeForce RTX 4060 Max-Q / Mobile] [1025:1731]
	Kernel driver in use: pciback
	Kernel modules: nvidiafb, nouveau
01:00.1 Audio device [0403]: NVIDIA Corporation Device [10de:22be] (rev a1)
	Subsystem: Acer Incorporated [ALI] Device [1025:1731]
	Kernel driver in use: pciback
	Kernel modules: snd_hda_intel
$ sudo xl pci-assignable-list
0000:01:00.0 
0000:01:00.1 
```

### vm 생성

vm 윈도우10 생성
```
https://www.microsoft.com/ko-kr/software-download/windows10 들어가 영어(미국) 64비트 다운로드.

$ sudo install -d -o root -g root -m 755 /var/lib/xen/images
$ sudo install -d -o root -g root -m 755 /var/lib/xen/iso

$ sudo qemu-img create -f qcow2 /var/lib/xen/images/win10.qcow2 60G
Formatting '/var/lib/xen/images/win10.qcow2', fmt=qcow2 cluster_size=65536 extended_l2=off compression_type=zlib size=64424509440 lazy_refcounts=off refcount_bits=16
$ sudo mv ~/Downloads/Win10_22H2_English_x64v1.iso /var/lib/xen/iso/Win10.iso

$ sudo chown root:root /var/lib/xen/iso/Win10.iso /var/lib/xen/images/win10.qcow2
$ sudo chmod 644 /var/lib/xen/iso/Win10.iso
$ sudo chmod 644 /var/lib/xen/images/win10.qcow2
$ sudo chmod 755 /var/lib/xen /var/lib/xen/iso /var/lib/xen/images

$ sudo nano /etc/xen/win10-install.cfg
name = "win10"
type = "hvm"
memory = 4096
vcpus  = 2

bios  = "seabios"
boot  = "dc"

device_model_version  = "qemu-xen"
device_model_override = "/usr/libexec/xen-qemu-system-i386"

vga = "stdvga"
videoram = 32
graphics = "vnc"
vnc = 1
vncunused = 1
vnclisten = "127.0.0.1"

vif = []

disk = [
  'format=qcow2,vdev=hda,access=rw,target=/var/lib/xen/images/win10.qcow2',
  'format=raw,vdev=hdc,devtype=cdrom,access=ro,target=/var/lib/xen/iso/Win10.iso'
]

$ sudo xl -vvv create /etc/xen/win10-install.cfg
$ sudo xl list
Name                                        ID   Mem VCPUs	State	Time(s)
Domain-0                                     0  4096    20     r-----    6300.0
win10-seabios                               22  4096     1     r-----       4.3

$ sudo apt install -y tigervnc-viewer
$ vncviewer 127.0.0.1:5900			# 혹은 sudo xl vncviwer win10-seabios
									# 포트 = 5900 + vncdisplay
화면 보면서 윈도우 설치 완료
```

### 네트워크 연결

wifi 브릿지 만들기
```
$ ip -br link
lo               UNKNOWN        00:00:00:00:00:00 <LOOPBACK,UP,LOWER_UP> 
enp68s0          UP             38:a7:46:0f:3b:be <BROADCAST,MULTICAST,UP,LOWER_UP> 
wlp0s20f3        UP             2c:7b:a0:8d:53:ba <BROADCAST,MULTICAST,UP,LOWER_UP> 

$ sudo nmcli connection add type bridge ifname xenbr0 con-name xenbr0
Connection 'xenbr0' (73ed640e-e780-434e-ac81-454bbb63a13f) successfully added.

$ sudo nmcli con mod xenbr0 ipv4.method auto ipv6.method ignore bridge.stp no bridge.forward-delay 0

$ sudo nmcli connection add type bridge-slave ifname enp68s0 master xenbr0
Connection 'bridge-slave-enp68s0' (3b3f8c28-d977-4851-9e61-f94cdb2d323e) successfully added.

$ sudo nmcli connection down "$(nmcli -t -f NAME,TYPE c show --active | awk -F: '$2=="ethernet"{print $1; exit}')" 2>/dev/null || true

$ sudo nmcli device disconnect enp68s0
Device 'enp68s0' successfully disconnected.

$ sudo nmcli con up bridge-slave-enp68s0
Connection successfully activated (D-Bus active path: /org/freedesktop/NetworkManager/ActiveConnection/18)

$ sudo nmcli con up xenbr0
Connection successfully activated (master waiting for slaves) (D-Bus active path: /org/freedesktop/NetworkManager/ActiveConnection/19)

$ nmcli device status
DEVICE             TYPE      STATE                                  CONNECTION           
xenbr0             bridge    connected                              xenbr0               
wlp0s20f3          wifi      connected                              KT_GiGA_5G_1D39      
enp68s0            ethernet  connected                              bridge-slave-enp68s0          
lo                 loopback  connected (externally)                 lo                   
p2p-dev-wlp0s20f3  wifi-p2p  disconnected                           --                   
```

참고로 삭제하려면
```
sudo nmcli con down xenbr0
sudo nmcli con down bridge-slave-enp68s0
sudo nmcli con delete xenbr0
sudo nmcli con delete bridge-slave-enp68s0
```

wifi 추가
```
$ sudo cp /etc/xen/win10-install.cfg /etc/xen/win10-ing.cfg
$ sudo nano /etc/xen/win10-ing.cfg

boot  = "c" 로 변경

vif = [ 'bridge=xenbr0,model=e1000,script=vif-bridge,mac=52:54:00:12:34:56' ] 로 변경

disk = [
  'format=qcow2,vdev=hda,access=rw,target=/var/lib/xen/images/win10.qcow2'
]
로 변경

$ sudo xl create /etc/xen/win10-ing.cfg
$ vncviewer 127.0.0.1:5900
```

### dgpu 패스스루

dgpu 패스스루 추가
```
$ sudo nano /etc/xen/win10-ing.cfg 

pci = [
  '0000:01:00.0,permissive=1'
]
추가

$ sudo xl create /etc/xen/win10-ing.cfg
$ vncviewer 127.0.0.1:5900

hdmi에 외부 모니터 연결해서 뜨면 디스플레이 복제로 변경
```

### libvmi 설치

libvmi 설치
```
$ sudo apt install -y \
  build-essential cmake pkg-config libtool check
$ sudo apt install -y \
  libglib2.0-dev libjson-c-dev libxen-dev xenstore-utils
$ sudo apt install -y \
  flex bison git python3-venv python3-pip

$ git clone https://github.com/libvmi/libvmi.git
cd libvmi
mkdir build && cd build

$ cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DENABLE_XEN=ON -DENABLE_KVM=OFF
$ make -j"$(nproc)"
$ sudo make install
$ sudo ldconfig
```
* 설치가 끝나면 /usr/local/bin/ 아래에 vmi-* 예제가 깔림


vmi-win-guid
```
$ sudo vmi-win-guid name win10
Windows Kernel found @ 0x2800000
	Version: 64-bit Windows 10
	PE GUID: bca5a8dd1046000
	PDB GUID: b6121da15ddcf625c8c7273c0d85eb101
	Kernel filename: ntkrnlmp.pdb
	Multi-processor without PAE
	Signature: 17744.
	Machine: 34404.
	# of sections: 33.
	# of symbols: 0.
	Timestamp: 3164973277.
	Characteristics: 34.
	Optional header size: 240.
	Optional header type: 0x20b
	Section 1: .rdata
	Section 2: .pdata
	Section 3: .idata
	Section 4: .edata
	Section 5: PROTDATA
	Section 6: GFIDS
	Section 7: Pad1
	Section 8: .text
	Section 9: PAGE
	Section 10: PAGELK
	Section 11: POOLCODE
	Section 12: PAGEKD
	Section 13: PAGEVRFY
	Section 14: PAGEHDLS
	Section 15: PAGEBGFX
	Section 16: INITKDBG
	Section 17: TRACESUP
	Section 18: KVASCODE
	Section 19: RETPOL
	Section 20: MINIEX
	Section 21: INIT
	Section 22: Pad2
	Section 23: .data
	Section 24: ALMOSTRO
	Section 25: CACHEALI
	Section 26: PAGEDATA
	Section 27: PAGEVRFD
	Section 28: INITDATA
	Section 29: Pad3
	Section 30: CFGRO
	Section 31: Pad4
	Section 32: .rsrc
	Section 33: .reloc
```

vmi-dump-memory
```
$ sudo mkdir -p /root/dumps
$ sudo install -d -m 755 /root/dumps
$ sudo vmi-dump-memory win10 /root/dumps/mem.bin
$ sudo strings -el /root/dumps/mem.bin | grep -F "##4729193##"
##4729193##5
```

### volatility 프로파일 연결

volatility 설치해 프로파일 만들기
```
$ python3 -m venv ~/vol3-venv
$ source ~/vol3-venv/bin/activate
(vol3-venv) $ pip install --upgrade pip
(vol3-venv) $ pip install volatility3

(vol3-venv) $ python3 -m volatility3.framework.symbols.windows.pdbconv \
  -p ntkrnlmp.pdb \
  -g b6121da15ddcf625c8c7273c0d85eb101 \
  -o ntkrnlmp.json
deactivate

$ sudo install -d -m 755 /root/symbols
$ sudo cp -i ~/ntkrnlmp.json /root/symbols/ntkrnlmp.json
$ sudo chown root:root /root/symbols/ntkrnlmp.json
$ sudo chmod 644 /root/symbols/ntkrnlmp.json

$ sudo nano /etc/libvmi.conf
win10 {
    volatility_ist = "/root/symbols/ntkrnlmp.json";
}
```

이제 이게 있으면 프로세스 구조체와 심볼 오프셋이 있어 vmi-process-list를 실행할 수 있음

vmi-process-list
```
$ sudo vmi-process-list win10
Process listing for VM win10 (id=31)
[    4] System (struct addr:ffffd184a6891040)
[   92] Registry (struct addr:ffffd184a7a05040)
[  328] smss.exe (struct addr:ffffd184a7674040)
[  420] csrss.exe (struct addr:ffffd184ab7c5080)
[  496] wininit.exe (struct addr:ffffd184ab8f2080)
[  636] services.exe (struct addr:ffffd184ab949080)
[  652] lsass.exe (struct addr:ffffd184ab8ec080)
[  756] svchost.exe (struct addr:ffffd184ab9a9240)
[  764] fontdrvhost.ex (struct addr:ffffd184ab9aa300)
[  876] svchost.exe (struct addr:ffffd184ac64c2c0)
[  928] svchost.exe (struct addr:ffffd184ac653240)
[    8] svchost.exe (struct addr:ffffd184ac6ea2c0)
[  356] svchost.exe (struct addr:ffffd184ac6ec300)
[  368] svchost.exe (struct addr:ffffd184ac70e280)
...
[ 9084] msedge.exe (struct addr:ffffd184b04130c0)
[ 3088] msedge.exe (struct addr:ffffd184b04350c0)
[ 6472] notepad.exe (struct addr:ffffd184adb04080)
[ 3840] svchost.exe (struct addr:ffffd184ad7b3080)
[  568] svchost.exe (struct addr:ffffd184b04d6340)
[ 4744] SystemSettings (struct addr:ffffd184ad7a6080)
[ 7744] MoUsoCoreWorke (struct addr:ffffd184ad9f0080)
[ 7112] UserOOBEBroker (struct addr:ffffd184ac69f080)
[ 6216] msedge.exe (struct addr:ffffd184ad54f080)
[ 3772] msedge.exe (struct addr:ffffd184ae31e300)
[ 3580] smartscreen.ex (struct addr:ffffd184aecc8280)
[ 2032] FileCoAuth.exe (struct addr:ffffd184aee84240)
[ 4584] msedge.exe (struct addr:ffffd184ac39a0c0)
[ 3784] msedge.exe (struct addr:ffffd184ae4850c0)
[ 4152] msedge.exe (struct addr:ffffd184ac3d8300)
[ 2704] msedge.exe (struct addr:ffffd184ae5e80c0)
[ 3912] msedge.exe (struct addr:ffffd184b004b0c0)
[ 4204] svchost.exe (struct addr:ffffd184affdb0c0)
[ 3748] msedge.exe (struct addr:ffffd184b0dda0c0)
[ 7292] msedge.exe (struct addr:ffffd184b0deb0c0)
```

### 파이썬 바인딩

파이썬 바인딩 설치
```
# (섀도잉 방지) 홈의 C 소스 폴더 이름 변경
[ -d ~/libvmi ] && mv ~/libvmi ~/libvmi-src

# 파이썬 바인딩 설치 (libvmi/python 공식)
sudo apt -y install python3-pip python3-cffi python3-pkgconfig
$ python3 -m venv ~/vmi-venv
$ ~/vmi-venv/bin/pip install --upgrade pip
$ ~/vmi-venv/bin/pip install 'libvmi@git+https://github.com/libvmi/python@v3.7.1'
```

확인
```
$ ~/vmi-venv/bin/python3 - <<'PY'
from libvmi import Libvmi
import libvmi, inspect
print("OK:", Libvmi)
print("Loaded from:", getattr(libvmi, "__file__", None))
PY
OK: <class 'libvmi.libvmi.Libvmi'>
Loaded from: /home/kkongnyang2/vmi-venv/lib/python3.12/site-packages/libvmi/__init__.py
```

이제 이게 있으면 C API 말고 파이썬 함수로 바인딩해 사용할 수 있음

### 가상환경이란

왜 Volatility3는 venv(가상환경)로 설치했을까?

Volatility3는 의존성이 많고 자주 바뀌는 툴이야. 이걸 시스템 파이썬(우분투가 내부에서 쓰는 파이썬)에 섞어 설치하면 apt로 관리되는 패키지와 버전 충돌이 나거나, /usr/lib/python3/dist-packages 같은 시스템 디렉터리를 오염시켜서 다른 시스템 도구(apt, Ansible, cloud-init 등)가 망가질 수 있어.
그래서 독립된 venv에 넣어두면 그 venv 안에서만 pip 패키지가 깔림 → 충돌/오염 방지, 원하는 버전으로 자유롭게 설치/업데이트 가능, 실험 끝나면 폴더만 지우면 깨끗하게 삭제

즉 시스템 안정성 + 의존성 고립 때문에 Volatility3는 venv에 두는 게 안전한 “관례”야.


파이썬 “환경/설치”가 어떻게 나뉘는지?

우분투(24.04 기준)에는 보통 시스템 파이썬이 있어 (예: /usr/bin/python3).

여기에 파이썬 패키지가 깔리는 경로는 크게 네 종류야:

시스템(apt) 영역
/usr/lib/python3/dist-packages
apt로 설치되는 패키지가 들어감 (섞으면 위험)

시스템-전역 pip 영역
/usr/local/lib/python3.X/dist-packages
root로 pip install 하면 여기에 들어갈 수 있음 (권장 X)

사용자 pip 영역
~/.local/lib/python3.X/site-packages
pip install --user 로 깔리는 곳 (시스템과 분리되지만, 여전히 전역 PATH에 섞일 수 있음)

가상환경(venv)
~/vol3-venv/lib/python3.X/site-packages
해당 venv 안에서만 보이는 “독립 공간” → 가장 안전


### 관리모드(vnc 포함) 버전

```
$ sudo nano /etc/xen/win10-ing.cfg 
name = "win10"
type = "hvm"
memory = 8192
vcpus  = 4

bios  = "seabios"
boot  = "c"

device_model_version  = "qemu-xen"
device_model_override = "/usr/libexec/xen-qemu-system-i386"

vga = "stdvga"
videoram = 32
graphics = "vnc"
vnc = 1
vncunused = 1
vnclisten = "127.0.0.1"

vif = [ 'bridge=xenbr0,model=e1000,script=vif-bridge,mac=52:54:00:12:34:56' ]

disk = [
  'format=qcow2,vdev=hda,access=rw,target=/var/lib/xen/images/win10.qcow2'
]

pci = [
  '0000:01:00.0,permissive=1'
]

usb = 1
usbdevice = [ 'tablet' ]

$ sudo xl create /etc/xen/win10-ing.cfg
$ vncviewer 127.0.0.1:5900
```

### 실사용

용량 확충
```
$ sudo qemu-img resize -f qcow2 /var/lib/xen/images/win10.qcow2 200G
들어가서 그냥 추가된 140GB를 드라이버 D로 만들어줌
```

프로그램
```
배틀넷 설치하고 드라이버 D에 오버워치2 설치
Cheat engine 7.5 window 프로그램 다운
```

vnc 제거 버전
```
$ sudo nano /etc/xen/win10.cfg  
name = "win10"
type = "hvm"
memory = 8192
vcpus  = 4

bios  = "seabios"
boot  = "c"

device_model_version  = "qemu-xen"
device_model_override = "/usr/libexec/xen-qemu-system-i386"

vif = [ 'bridge=xenbr0,model=e1000,script=vif-bridge,mac=52:54:00:12:34:56' ]

disk = [
  'format=qcow2,vdev=hda,access=rw,target=/var/lib/xen/images/win10.qcow2'
]

pci = [
  '0000:01:00.0,permissive=1'
]

$ sudo xl create /etc/xen/win10.cfg
```

이벤트 잡아내기 기능 추가, dom0 메모리 고정
```
$ sudoedit /etc/default/grub.d/xen.cfg
GRUB_CMDLINE_XEN="iommu=1 iommu=verbose dom0_mem=16384M,max:16384M altp2m=1"
GRUB_CMDLINE_LINUX_XEN_REPLACE="xen-pciback.hide=(01:00.0)(01:00.1) modprobe.blacklist=nvidia,nvidia_drm,nvidia_modeset,nouveau"
$ sudo update-grub
재부팅
$ sudo nano /etc/xen/win10.cfg
altp2m = "external" 추가
```
* free_memory도 기존 게스트 메모리 확장/벌룬, EPT/NPT 테이블·altp2m 뷰 같은 하이퍼바이저 측 추가 할당에 쓰일 수 있는 풀. 어느정도 남겨두는게 좋다.

usb 개별로 attach
```
$ lsusb -t
$ lsusb
Bus 001 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub
Bus 001 Device 002: ID 046d:c54d Logitech, Inc. USB Receiver
Bus 001 Device 003: ID 04f2:b76f Chicony Electronics Co., Ltd ACER HD User Facing
Bus 001 Device 004: ID 8087:0033 Intel Corp. AX211 Bluetooth
Bus 001 Device 005: ID 3554:f58a Compx VXE NordicMouse 1K Dongle
Bus 001 Device 006: ID 046d:c31c Logitech, Inc. Keyboard K120
Bus 002 Device 001: ID 1d6b:0003 Linux Foundation 3.0 root hub
Bus 003 Device 001: ID 1d6b:0002 Linux Foundation 2.0 root hub
Bus 004 Device 001: ID 1d6b:0003 Linux Foundation 3.0 root hub
장치 5,6 확정

$ sudo xl usbctrl-attach win10 type=auto version=3 ports=15
$ sudo xl usbdev-attach  win10 hostbus=1 hostaddr=5 controller=0 port=1
$ sudo xl usbdev-attach  win10 hostbus=1 hostaddr=6 controller=0 port=2
```

vcpu 핀
```
$ lscpu -e
CPU NODE SOCKET CORE L1d:L1i:L2:L3 ONLINE
  0    0      0    0 0:0:0:0          yes
  1    0      0    0 0:0:0:0          yes
  2    0      0    1 4:4:1:0          yes
  3    0      0    1 4:4:1:0          yes
  4    0      0    2 8:8:2:0          yes
  5    0      0    2 8:8:2:0          yes
  6    0      0    3 12:12:3:0        yes
  7    0      0    3 12:12:3:0        yes
  8    0      0    4 16:16:4:0        yes
  9    0      0    4 16:16:4:0        yes
 10    0      0    5 20:20:5:0        yes
 11    0      0    5 20:20:5:0        yes
 12    0      0    6 24:24:6:0        yes
 13    0      0    7 25:25:6:0        yes
 14    0      0    8 26:26:6:0        yes
 15    0      0    9 27:27:6:0        yes
 16    0      0   10 28:28:7:0        yes
 17    0      0   11 29:29:7:0        yes
 18    0      0   12 30:30:7:0        yes
 19    0      0   13 31:31:7:0        yes

6P + 8E = 14코어 / 20스레드 구조야.
CORE 0~5 즉 cpu 0~11는 각 2개 CPU가 같은 L1/L2 인덱스를 공유
CORE 6~13 즉 cpu 12~19는 단독 CPU씩
전부 NODE 0 / SOCKET 0 / L3 0 → 단일 소켓·단일 NUMA (L3 공유)

$ sudo xl vcpu-set 0 8
$ sudo xl vcpu-pin 0 all 12-19

$ sudo xl vcpu-pin 2 0 0
$ sudo xl vcpu-pin 2 1 2
$ sudo xl vcpu-pin 2 2 4
$ sudo xl vcpu-pin 2 3 6
각 vcpu에 pcpu를 배정해줌
```

확인
```
$ sudo xl list
Name                                        ID   Mem VCPUs	State	Time(s)
Domain-0                                     0 16384     8     r-----     468.8
win10                                        2  8192     4     r-----      75.8

$ sudo xl info | egrep -i 'memory|free_memory'
total_memory           : 32472
free_memory            : 7529
sharing_freed_memory   : 0
sharing_used_memory    : 0

$ sudo xl vcpu-list
Name                                ID  VCPU   CPU State   Time(s) Affinity (Hard / Soft)
Domain-0                             0     0   14   r--      37.9  12-19 / all
Domain-0                             0     1   18   -b-      13.8  12-19 / all
Domain-0                             0     2   19   r--      33.7  12-19 / all
Domain-0                             0     3   17   -b-      29.8  12-19 / all
Domain-0                             0     4   12   -b-      41.6  12-19 / all
Domain-0                             0     5   16   -b-      24.1  12-19 / all
Domain-0                             0     6   13   -b-      36.5  12-19 / all
Domain-0                             0     7   15   -b-      14.6  12-19 / all
Domain-0                             0     8    -   --p      20.1  12-19 / all
Domain-0                             0     9    -   --p       4.3  12-19 / all
Domain-0                             0    10    -   --p      17.7  12-19 / all
Domain-0                             0    11    -   --p      12.6  12-19 / all
Domain-0                             0    12    -   --p      25.9  12-19 / all
Domain-0                             0    13    -   --p      24.5  12-19 / all
Domain-0                             0    14    -   --p      21.8  12-19 / all
Domain-0                             0    15    -   --p      25.1  12-19 / all
Domain-0                             0    16    -   --p      32.5  12-19 / all
Domain-0                             0    17    -   --p      28.9  12-19 / all
Domain-0                             0    18    -   --p      26.3  12-19 / all
Domain-0                             0    19    -   --p      27.1  12-19 / all
win10                                2     0    0   -b-      25.8  0 / all
win10                                2     1    2   -b-      24.7  2 / all
win10                                2     2    4   -b-      25.6  4 / all
win10                                2     3    6   -b-      25.9  6 / all
```

수정하려면
```
dom0 메모리는 여기서 설정
sudoedit /etc/default/grub.d/xen.cfg
sudo update-grub

게스트 메모리 및 vcpu 개수는 여기서 설정
sudo nano /etc/xen/win10.cfg  
```