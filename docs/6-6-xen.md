## xen 방식으로 vmi를 구현해보자

작성자: kkongnyang2 작성일: 2025-08-08

---

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


sudo apt install -y xen-system-amd64 ovmf qemu-system-x86

echo 'GRUB_CMDLINE_XEN="iommu=1 iommu=verbose dom0_mem=4096M,max:4096M"' | sudo tee /etc/default/grub.d/xen.cfg

echo 'GRUB_CMDLINE_LINUX_XEN_REPLACE="xen-pciback.hide=(01:00.0)(01:00.1) modprobe.blacklist=nvidia,nvidia_drm,nvidia_modeset,nouveau"' \
| sudo tee -a /etc/default/grub.d/xen.cfg

sudo update-grub

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

~$ sudo xl list
[sudo] password for kkongnyang2: 
Name                                        ID   Mem VCPUs	State	Time(s)
Domain-0                                     0  4096    20     r-----     415.7

 cat /sys/hypervisor/type
xen

kkongnyang2@acer:~$ lsmod | grep xen_pciback || sudo modprobe xen_pciback
kkongnyang2@acer:~$ lspci -nnk | grep -A3 -Ei 'vga|3d|display|Audio'
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



kkongnyang2@acer:~$ DRVDIR=$(ls -d /sys/bus/pci/drivers/pciback /sys/bus/pci/drivers/xen-pciback 2>/dev/null | head -n1)
kkongnyang2@acer:~$ echo "$DRVDIR"
/sys/bus/pci/drivers/pciback
kkongnyang2@acer:~$ echo 0000:01:00.1 | sudo tee /sys/bus/pci/devices/0000:01:00.1/driver/unbind
0000:01:00.1
kkongnyang2@acer:~$ echo "10de 22be" | sudo tee "$DRVDIR/new_id"
10de 22be
tee: /sys/bus/pci/drivers/pciback/new_id: File exists
kkongnyang2@acer:~$ echo 0000:01:00.1 | sudo tee "$DRVDIR/bind"
0000:01:00.1
kkongnyang2@acer:~$ lspci -nnk | grep -A3 -Ei 'vga|3d|display|Audio'
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
kkongnyang2@acer:~$ sudo xl pci-assignable-list
0000:01:00.0 
0000:01:00.1 

https://www.microsoft.com/ko-kr/software-download/windows10 들어가 영어(미국) 64비트 다운로드.

kkongnyang2@acer:~$ sudo install -d -o root -g root -m 755 /var/lib/xen/images
kkongnyang2@acer:~$ sudo install -d -o root -g root -m 755 /var/lib/xen/iso

kkongnyang2@acer:~$ sudo qemu-img create -f qcow2 /var/lib/xen/images/win10.qcow2 60G
Formatting '/var/lib/xen/images/win10.qcow2', fmt=qcow2 cluster_size=65536 extended_l2=off compression_type=zlib size=64424509440 lazy_refcounts=off refcount_bits=16
kkongnyang2@acer:~$ sudo mv ~/Downloads/Win10_22H2_English_x64v1.iso /var/lib/xen/iso/Win10.iso

kkongnyang2@acer:~$ sudo chown root:root /var/lib/xen/iso/Win10.iso /var/lib/xen/images/win10.qcow2
kkongnyang2@acer:~$ sudo chmod 644 /var/lib/xen/iso/Win10.iso
kkongnyang2@acer:~$ sudo chmod 644 /var/lib/xen/images/win10.qcow2
kkongnyang2@acer:~$ sudo chmod 755 /var/lib/xen /var/lib/xen/iso /var/lib/xen/images

$ sudo nano /etc/xen/win10-seabios.cfg
name = "win10-seabios"
type = "hvm"
memory = 4096
vcpus  = 2

bios  = "seabios"
boot  = "dc"

# DM 경로 고정 (둘 중 실제 존재하는 경로 하나만)
device_model_version  = "qemu-xen"
device_model_override = "/usr/libexec/xen-qemu-system-i386"
# device_model_override = "/usr/libexec/xen-qemu-system-i386"

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

$ sudo xl -vvv create /etc/xen/win10-seabios.cfg
$ sudo apt install -y tigervnc-viewer
$ vncviewer 127.0.0.1:5900			# 혹은 sudo xl vncviwer win10-seabios
									# 포트 = 5900 + vncdisplay


$ sudo xl list
Name                                        ID   Mem VCPUs	State	Time(s)
Domain-0                                     0  4096    20     r-----    6300.0
win10-seabios                               22  4096     1     r-----       4.3



sudo nano /etc/xen/win10-seabios-poweron.cfg      
name = "win10-seabios"
type = "hvm"
memory = 4096
vcpus  = 2

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

vif = []

disk = [
  'format=qcow2,vdev=hda,access=rw,target=/var/lib/xen/images/win10.qcow2'
]


sudo xl create /etc/xen/win10-seabios-poweron.cfg



$ ip -br link
lo               UNKNOWN        00:00:00:00:00:00 <LOOPBACK,UP,LOWER_UP> 
enp68s0          UP             38:a7:46:0f:3b:be <BROADCAST,MULTICAST,UP,LOWER_UP> 
wlp0s20f3        UP             2c:7b:a0:8d:53:ba <BROADCAST,MULTICAST,UP,LOWER_UP> 

$ sudo nmcli connection add type bridge ifname xenbr0 con-name xenbr0
Connection 'xenbr0' (73ed640e-e780-434e-ac81-454bbb63a13f) successfully added.

sudo nmcli con mod xenbr0 ipv4.method auto ipv6.method ignore bridge.stp no bridge.forward-delay 0

$ sudo nmcli connection add type bridge-slave ifname enp68s0 master xenbr0
Connection 'bridge-slave-enp68s0' (3b3f8c28-d977-4851-9e61-f94cdb2d323e) successfully added.

$ sudo nmcli connection down "$(nmcli -t -f NAME,TYPE c show --active | awk -F: '$2=="ethernet"{print $1; exit}')" 2>/dev/null || true

kkongnyang2@acer:~$ sudo nmcli device disconnect enp68s0
Device 'enp68s0' successfully disconnected.

kkongnyang2@acer:~$ sudo nmcli con up bridge-slave-enp68s0
Connection successfully activated (D-Bus active path: /org/freedesktop/NetworkManager/ActiveConnection/18)

kkongnyang2@acer:~$ sudo nmcli con up xenbr0
Connection successfully activated (master waiting for slaves) (D-Bus active path: /org/freedesktop/NetworkManager/ActiveConnection/19)

kkongnyang2@acer:~$ nmcli device status
DEVICE             TYPE      STATE                                  CONNECTION           
xenbr0             bridge    connected                              xenbr0               
wlp0s20f3          wifi      connected                              KT_GiGA_5G_1D39      
enp68s0            ethernet  connected                              bridge-slave-enp68s0 
xenbr-nat          bridge    connecting (getting IP configuration)  xenbr-na             
lo                 loopback  connected (externally)                 lo                   
p2p-dev-wlp0s20f3  wifi-p2p  disconnected                           --                   


$ sudo nano /etc/xen/win10-seabios-poweron.cfg
vif = [ 'bridge=xenbr0,model=e1000,script=vif-bridge,mac=52:54:00:12:34:56' ]

$ sudo xl create /etc/xen/win10-seabios-poweron.cfg
$ vncviewer 127.0.0.1:5900






auto select와 optimus 차이
지금까지는 optimus는 백라이트 밝기 조절이 안되는거밖에 없음. 둘다 잘 부팅됨

