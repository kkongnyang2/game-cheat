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

vif = [ 'bridge=xenbr0,model=e1000,script=vif-bridge,mac=52:54:00:12:34:56' ]

disk = [
  'format=qcow2,vdev=hda,access=rw,target=/var/lib/xen/images/win10.qcow2'
]

pci = [
  '0000:01:00.0,permissive=1'
]

복제로 변경

sudo apt install -y \
  build-essential cmake pkg-config libtool check
sudo apt install -y \
  libglib2.0-dev libjson-c-dev libxen-dev xenstore-utils
sudo apt install -y \
  flex bison git python3-venv python3-pip


git clone https://github.com/libvmi/libvmi.git
cd libvmi
mkdir build && cd build

cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DENABLE_XEN=ON -DENABLE_KVM=OFF
make -j"$(nproc)"
sudo make install
sudo ldconfig

설치가 끝나면 /usr/local/bin/ 아래에 vmi-* 예제가 깔림


kkongnyang2@acer:~$ sudo tee /etc/libvmi.conf >/dev/null <<'EOF'
win10-seabios {
    ostype = "Windows";
    # 도메인 이름으로 찾게 할지, domid로 박을지 중 택1
    domain = "win10-seabios";
    # 또는: domid = <xl list로 본 숫자>;

    # 아래 rekall_profile은 5단계에서 만들 JSON 경로로 바꿔 넣습니다.
    rekall_profile = "/root/symbols/ntkrnlmp.json";
}
EOF
kkongnyang2@acer:~$ sudo vmi-dump-memory win10-seabios --memory 1 --pid 4 >/dev/null 2>&1 || true
kkongnyang2@acer:~$ sudo vmi-win-guid name win10-seabios
Windows Kernel found @ 0x2c00000
	Version: 64-bit Windows 10
	PE GUID: f71f414a1046000
	PDB GUID: 89284d0ca6acc8274b9a44bd5af9290b1
	Kernel filename: ntkrnlmp.pdb
	Multi-processor without PAE
	Signature: 17744.
	Machine: 34404.
	# of sections: 33.
	# of symbols: 0.
	Timestamp: 4146020682.
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

kkongnyang2@acer:~$ python3 -m venv ~/vol3-venv
kkongnyang2@acer:~$ source ~/vol3-venv/bin/activate
(vol3-venv) kkongnyang2@acer:~$ pip install --upgrade pip
(vol3-venv) kkongnyang2@acer:~$ pip install volatility3

$ python3 -m volatility3.framework.symbols.windows.pdbconv \
  -p ntkrnlmp.pdb \
  -g 89284d0ca6acc8274b9a44bd5af9290b1 \
  -o ntkrnlmp.json
deactivate

kkongnyang2@acer:~$ sudo install -d -m 755 /root/symbols
[sudo] password for kkongnyang2: 
kkongnyang2@acer:~$ sudo cp -i ~/ntkrnlmp.json /root/symobls/ntkrnlmp.json
cp: cannot create regular file '/root/symobls/ntkrnlmp.json': No such file or directory
kkongnyang2@acer:~$ sudo cp -i ~/ntkrnlmp.json /root/symbols/ntkrnlmp.json
kkongnyang2@acer:~$ sudo chown root:root /root/symbols/ntkrnlmp.json
kkongnyang2@acer:~$ sudo chmod 644 /root/symbols/ntkrnlmp.json

kkongnyang2@acer:~$ sudo nano /etc/libvmi.conf
win10-seabios {
    volatility_ist = "/root/symbols/ntkrnlmp.json";
}

kkongnyang2@acer:~$ sudo vmi-process-list win10-seabios
Process listing for VM win10-seabios (id=31)
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
[  372] svchost.exe (struct addr:ffffd184ac711300)
[ 1064] svchost.exe (struct addr:ffffd184ac77e300)
[ 1076] svchost.exe (struct addr:ffffd184ac784300)
[ 1160] svchost.exe (struct addr:ffffd184ac7c12c0)
[ 1228] svchost.exe (struct addr:ffffd184ac7c5300)
[ 1296] svchost.exe (struct addr:ffffd184ac7c4080)
[ 1328] svchost.exe (struct addr:ffffd184ac8262c0)
[ 1336] svchost.exe (struct addr:ffffd184ac8240c0)
[ 1356] svchost.exe (struct addr:ffffd184ac825080)
[ 1408] svchost.exe (struct addr:ffffd184ac827080)
[ 1428] svchost.exe (struct addr:ffffd184ac829300)
[ 1476] MemCompression (struct addr:ffffd184ac878080)
[ 1540] svchost.exe (struct addr:ffffd184ac8a5240)
[ 1584] svchost.exe (struct addr:ffffd184ac8eb2c0)
[ 1620] svchost.exe (struct addr:ffffd184ac94a280)
[ 1632] svchost.exe (struct addr:ffffd184ac94c2c0)
[ 1644] svchost.exe (struct addr:ffffd184ac94e2c0)
[ 1748] svchost.exe (struct addr:ffffd184ac9be2c0)
[ 1780] svchost.exe (struct addr:ffffd184ac9e1240)
[ 1900] svchost.exe (struct addr:ffffd184acaaf300)
[ 1908] svchost.exe (struct addr:ffffd184aca78080)
[ 1924] svchost.exe (struct addr:ffffd184acab3080)
[ 1108] svchost.exe (struct addr:ffffd184acaeb240)
[ 1440] svchost.exe (struct addr:ffffd184acb27240)
[ 2120] svchost.exe (struct addr:ffffd184acb7b300)
[ 2128] spoolsv.exe (struct addr:ffffd184acb730c0)
[ 2200] svchost.exe (struct addr:ffffd184acbe22c0)
[ 2268] svchost.exe (struct addr:ffffd184acc0a300)
[ 2500] svchost.exe (struct addr:ffffd184acccf240)
[ 2508] svchost.exe (struct addr:ffffd184accd2300)
[ 2516] svchost.exe (struct addr:ffffd184accd72c0)
[ 2536] svchost.exe (struct addr:ffffd184accd90c0)
[ 2604] svchost.exe (struct addr:ffffd184accab240)
[ 2616] svchost.exe (struct addr:ffffd184accae2c0)
[ 2636] svchost.exe (struct addr:ffffd184accb9280)
[ 2672] svchost.exe (struct addr:ffffd184acd51240)
[ 2680] MsMpEng.exe (struct addr:ffffd184acd53280)
[ 2764] svchost.exe (struct addr:ffffd184acdf5240)
[ 2828] svchost.exe (struct addr:ffffd184ace0a2c0)
[ 2928] svchost.exe (struct addr:ffffd184ace4d240)
[ 3548] NisSrv.exe (struct addr:ffffd184ad2b9300)
[ 3700] svchost.exe (struct addr:ffffd184ad4262c0)
[ 3944] MicrosoftEdgeU (struct addr:ffffd184ad49c080)
[ 3960] svchost.exe (struct addr:ffffd184ad49e080)
[ 4060] svchost.exe (struct addr:ffffd184ad4f9280)
[ 3216] svchost.exe (struct addr:ffffd184ad506240)
[ 3252] svchost.exe (struct addr:ffffd184ad5f32c0)
[ 4324] svchost.exe (struct addr:ffffd184ad674300)
[ 5236] svchost.exe (struct addr:ffffd184ad7d9080)
[ 5244] svchost.exe (struct addr:ffffd184adeed340)
[ 5304] svchost.exe (struct addr:ffffd184add0f080)
[ 5520] svchost.exe (struct addr:ffffd184add0d080)
[ 6264] svchost.exe (struct addr:ffffd184adca3080)
[ 6328] SearchIndexer. (struct addr:ffffd184ade9c240)
[ 6816] svchost.exe (struct addr:ffffd184ae2f4340)
[ 6856] svchost.exe (struct addr:ffffd184ae30b340)
[ 6972] svchost.exe (struct addr:ffffd184ae26d080)
[ 7036] svchost.exe (struct addr:ffffd184ae2eb240)
[ 4868] svchost.exe (struct addr:ffffd184ae5662c0)
[ 7316] SecurityHealth (struct addr:ffffd184ae673280)
[ 7720] svchost.exe (struct addr:ffffd184aeea30c0)
[ 8444] msedge.exe (struct addr:ffffd184ae6240c0)
[ 8452] msedge.exe (struct addr:ffffd184aed4b080)
[ 8460] msedge.exe (struct addr:ffffd184aed63080)
[ 8768] msedge.exe (struct addr:ffffd184aeecc080)
[ 5836] msedge.exe (struct addr:ffffd184aee76080)
[ 3316] svchost.exe (struct addr:ffffd184ae2750c0)
[ 5624] svchost.exe (struct addr:ffffd184ae9f3080)
[ 8020] SgrmBroker.exe (struct addr:ffffd184af165080)
[ 5868] svchost.exe (struct addr:ffffd184af169080)
[ 8704] csrss.exe (struct addr:ffffd184aed8f080)
[ 3220] winlogon.exe (struct addr:ffffd184af02a080)
[ 8608] LogonUI.exe (struct addr:ffffd184af079300)
[ 3616] dwm.exe (struct addr:ffffd184adca9300)
[ 3264] fontdrvhost.ex (struct addr:ffffd184ae25a300)
[ 2808] svchost.exe (struct addr:ffffd184ae6d0080)
[ 6132] sihost.exe (struct addr:ffffd184ad9f1080)
[ 6304] svchost.exe (struct addr:ffffd184ad1b9340)
[ 3736] svchost.exe (struct addr:ffffd184ae664080)
[ 4396] taskhostw.exe (struct addr:ffffd184adef0080)
[ 4696] userinit.exe (struct addr:ffffd184ae058080)
[ 2584] explorer.exe (struct addr:ffffd184ae65c080)
[ 8320] svchost.exe (struct addr:ffffd184ad7a2080)
[ 6204] StartMenuExper (struct addr:ffffd184ae659080)
[ 7620] RuntimeBroker. (struct addr:ffffd184ad425080)
[ 6032] SearchApp.exe (struct addr:ffffd184aece9340)
[ 6660] RuntimeBroker. (struct addr:ffffd184ab92a080)
[ 7764] ctfmon.exe (struct addr:ffffd184ae2e2080)
[ 8012] SkypeApp.exe (struct addr:ffffd184ae5cf080)
[ 7908] SkypeBackgroun (struct addr:ffffd184ae671080)
[ 4764] LockApp.exe (struct addr:ffffd184adf3e340)
[ 5744] RuntimeBroker. (struct addr:ffffd184aefd5340)
[ 3300] RuntimeBroker. (struct addr:ffffd184ae654080)
[ 4284] RuntimeBroker. (struct addr:ffffd184ac82f080)
[ 3884] SecurityHealth (struct addr:ffffd184acf54080)
[ 9060] OneDrive.exe (struct addr:ffffd184ad634300)
[ 7884] TextInputHost. (struct addr:ffffd184aef68300)
[ 6100] dllhost.exe (struct addr:ffffd184ae49e080)
[ 7968] ApplicationFra (struct addr:ffffd184ae37e080)
[ 7452] msedge.exe (struct addr:ffffd184aecd2080)
[ 9140] msedge.exe (struct addr:ffffd184aed83080)
[ 8828] svchost.exe (struct addr:ffffd184add0e080)
[ 7380] NVDisplay.Cont (struct addr:ffffd184ad9ef080)
[ 9056] dbInstaller.ex (struct addr:ffffd184aed9a080)
[ 2712] NVDisplay.Cont (struct addr:ffffd184ac3cb100)
[ 7784] dllhost.exe (struct addr:ffffd184aedab080)
[ 8576] msedge.exe (struct addr:ffffd184aea9a080)
[ 2740] msedge.exe (struct addr:ffffd184aeaa7080)
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

sudo mkdir -p /root/dumps
sudo vmi-dump-memory win10-seabios /root/dumps/win10-seabios.mem
$ sudo rm /root/dumps/win10-seabios.mem

auto select와 optimus 차이
지금까지는 optimus는 백라이트 밝기 조절이 안되는거밖에 없음. 둘다 잘 부팅됨

