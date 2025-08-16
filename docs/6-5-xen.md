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


24.04 설치시 드라이버 두개 다 미체크
```
$ lspci -nnk | grep -A3 -Ei 'vga|3d|display'
00:02.0 VGA compatible controller [0300]: Intel Corporation Raptor Lake-S UHD Graphics [8086:a788] (rev 04)
	Subsystem: Acer Incorporated [ALI] Raptor Lake-S UHD Graphics [1025:1731]
	Kernel driver in use: i915
	Kernel modules: i915, xe
--
01:00.0 VGA compatible controller [0300]: NVIDIA Corporation AD107M [GeForce RTX 4060 Max-Q / Mobile] [10de:28e0] (rev a1)
	Subsystem: Acer Incorporated [ALI] AD107M [GeForce RTX 4060 Max-Q / Mobile] [1025:1731]
	Kernel driver in use: nouveau
	Kernel modules: nvidiafb, nouveau
```


화질 낮추고 프레임 낮춰서

```
# 안정/공식 경로: Xen 4.17
sudo apt install -y xen-system-amd64 ovmf qemu-system-x86
sudo apt install -y xen-system-amd64 xen-utils-4.17 ovmf qemu-utils bridge-utils
```
```
# Xen 하이퍼바이저 파라미터(=xen.gz에 전달)
echo 'GRUB_CMDLINE_XEN="iommu=1 iommu=verbose dom0_mem=4096M,max:4096M"' | sudo tee /etc/default/grub.d/xen.cfg

# dom0 리눅스 커널 파라미터(=xen 엔트리에서 리눅스에만 적용)
# dGPU를 부팅 초기에 pciback로 바인딩하여 dom0 드라이버가 못 잡게 함
echo 'GRUB_CMDLINE_LINUX_XEN_REPLACE="xen-pciback.hide=(01:00.0)(01:00.1) modprobe.blacklist=nvidia,nvidia_drm,nvidia_modeset,nouveau"' \
| sudo tee -a /etc/default/grub.d/xen.cfg

sudo update-grub
```
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
cpu_mhz                : 2803.203
kkongnyang2@acer:~$ grep -i "Xen version" /var/log/dmesg
[    0.755301] kernel: Xen version: 4.17.4-pre (preserve-AD)
kkongnyang2@acer:~$ ls -ld /proc/xen
drwxr-xr-x 2 root root 0 Aug 16 12:38 /proc/xen
kkongnyang2@acer:~$ sudo xl list
Name                                        ID   Mem VCPUs	State	Time(s)
Domain-0                                     0  4096    20     r-----     291.6

$ cat /sys/hypervisor/type
xen

$ sudo xl dmesg | grep -i iommu
(XEN) Command line: placeholder iommu=1 iommu=verbose dom0_mem=4096M,max:4096M no-real-mode edd=off
(XEN) [VT-D]drhd->address = fed90000 iommu->reg = ffff82c0011a6000
(XEN) [VT-D]drhd->address = fed91000 iommu->reg = ffff82c0011a8000
(XEN) Intel VT-d iommu 0 supported page sizes: 4kB, 2MB, 1GB
(XEN) Intel VT-d iommu 1 supported page sizes: 4kB, 2MB, 1GB
(XEN) [VT-D]iommu_enable_translation: iommu->reg = ffff82c0011a6000
(XEN) [VT-D]iommu_enable_translation: iommu->reg = ffff82c0011a8000


$ lspci -nnk | grep -A3 -Ei 'vga|3d|display'
00:02.0 VGA compatible controller [0300]: Intel Corporation Raptor Lake-S UHD Graphics [8086:a788] (rev 04)
	Subsystem: Acer Incorporated [ALI] Raptor Lake-S UHD Graphics [1025:1731]
	Kernel driver in use: i915
	Kernel modules: i915, xe
--
01:00.0 VGA compatible controller [0300]: NVIDIA Corporation AD107M [GeForce RTX 4060 Max-Q / Mobile] [10de:28e0] (rev a1)
	Subsystem: Acer Incorporated [ALI] AD107M [GeForce RTX 4060 Max-Q / Mobile] [1025:1731]
	Kernel modules: nvidiafb, nouveau
01:00.1 Audio device [0403]: NVIDIA Corporation Device [10de:22be] (rev a1)
```
```
$ sudo mkdir -p /xen/{iso,disks,roms}
$ sudo qemu-img create -f qcow2 /xen/disks/win11.qcow2 200G
Formatting '/xen/disks/win11.qcow2', fmt=qcow2 cluster_size=65536 extended_l2=off compression_type=zlib size=214748364800 lazy_refcounts=off refcount_bits=16


https://www.microsoft.com/ko-kr/software-download/windows11 들어가 영어(미국) 64비트 다운로드. Win11_24H2_English_x64.iso. 윈도우 설치를 위한 DVD 역할.

$ sudo mv ~/Downloads/Win11_24H2_English_x64.iso /xen/iso/win11.iso
$ sudo chown root:root /xen/iso/win11.iso
$ sudo chmod 664 /xen/iso/win11.iso
```
```
sudo nano /etc/xen/win11-install.cfg
name = "win11"
type = "hvm"
memory = 16384
vcpus = 16
bios = "ovmf"
device_model_version = "qemu-xen"
vif = []
disk = [
  'file:/xen/disks/win11.qcow2,hda,w',
  'file:/xen/iso/win11.iso,hdc:cdrom,ro'
]
boot = "d"
vga = "stdvga"
graphics = "vnc"
vnc = 1
vnclisten = "0.0.0.0"
vncdisplay = 10
viridian = 1
rdm = "strategy=host,policy=relaxed"
```
```
sudo xl create /etc/xen/win11-install.cfg
sudo xl create -c /etc/xen/win11-install.cfg
Parsing config from /etc/xen/win11-install.cfg
빠져나오려면 Ctrl + ]
sudo xl shutdown -w win11
sudo xl destroy win11
$ sudo xl list
Name                                        ID   Mem VCPUs	State	Time(s)
Domain-0                                     0  4096    20     r-----    1635.3
win11                                        1 16385     1     -b----      10.2

STATE가 r 또는 b면 떠 있는 것
```
```
sudo apt install -y tigervnc-viewer  # 또는 remmina
vncviewer 127.0.0.1:5910             # 포트 = 5900 + vncdisplay
```

설치가 끝나면 ISO 줄이고 정상 부팅 확인:

# cdrom 제거 후 부팅용 설정으로 바꾸기
sudo sed -i '/cdrom/d' /etc/xen/win11.cfg
sudo sed -i 's/boot = "dc"/boot = "c"/' /etc/xen/win11.cfg
sudo xl destroy win11 || true
sudo xl create -c /etc/xen/win11.cfg