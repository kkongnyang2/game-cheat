## KVMI 방식으로 vmi를 구현해보자

작성자: kkongnyang2 작성일: 2025-08-12

---

### > 사전 준비

bios 설정
```
VT-x, VT-d 켜기
Secure boot 끄기(서명 문제 회피에 유리. 바로 안꺼지면 바이오스 비번 설정한 후 끈 후 비번 다시 없애면 됨)
vmd controller 끄기(advanced-main에서 ctrl+s 눌러 히든옵션 띄우면 됨)
display mode : optimus
```
* optimus = igpu 직결, dgpu 도움
* dgpu only = dgpu 직결
* auto select = 위 두개 중에 자동선택

커널과 하드웨어 호환 미리 살피기
```
# === 시스템 점검 패키지(없으면) ===
sudo apt -y install ethtool mokutil || true

echo "===== CPU ====="
lscpu | egrep 'Model name|Vendor ID|Architecture|CPU\\(s\\)|Thread|Core|Socket'
echo; echo "===== CPU flags (vmx/ept 확인) ====="
grep -m1 '^flags' /proc/cpuinfo | tr ' ' '\n' | egrep -x 'vmx|svm|ept|npt' | sort -u

echo; echo "===== IOMMU/DMAR (VT-d) ====="
dmesg | egrep -i 'DMAR|IOMMU|VT-d' | head -n 50

echo; echo "===== 부팅/시큐어부트 ====="
[ -d /sys/firmware/efi ] && echo "Boot: UEFI" || echo "Boot: Legacy"
mokutil --sb-state 2>/dev/null || echo "mokutil not available"

echo; echo "===== 커널/OS ====="
uname -a
grep -E 'PRETTY_NAME|VERSION=' /etc/os-release

echo; echo "===== 메인보드/BIOS ====="
sudo dmidecode -t baseboard -t bios | egrep -i 'Vendor:|Product Name:|Version:|Release Date:' || true

echo; echo "===== GPU ====="
lspci -nnk | grep -A3 -Ei 'vga|3d|display'

echo; echo "===== NIC(이더넷/무선) ====="
lspci -nnk | grep -A3 -Ei 'ethernet|network'

echo; echo "===== 인터페이스별 드라이버 ====="
for i in $(ls /sys/class/net | grep -v lo); do
  echo "== $i =="; sudo ethtool -i "$i" 2>/dev/null || true
done

===== CPU =====
Architecture:                         x86_64
Vendor ID:                            GenuineIntel
Model name:                           Intel(R) Core(TM) i5-14500HX
Thread(s) per core:                   2
Core(s) per socket:                   14
Socket(s):                            1

===== CPU flags (vmx/ept 확인) =====
ept
vmx

===== IOMMU/DMAR (VT-d) =====
dmesg: read kernel buffer failed: Operation not permitted

===== 부팅/시큐어부트 =====
Boot: UEFI
SecureBoot disabled

===== 커널/OS =====
Linux acer 6.14.0-27-generic #27~24.04.1-Ubuntu SMP PREEMPT_DYNAMIC Tue Jul 22 17:38:49 UTC 2 x86_64 x86_64 x86_64 GNU/Linux
PRETTY_NAME="Ubuntu 24.04.3 LTS"
VERSION="24.04.3 LTS (Noble Numbat)"

===== 메인보드/BIOS =====
	Vendor: Insyde Corp.
	Version: V1.14
	Release Date: 09/19/2024
	Product Name: EQE_RTX
	Version: V1.14

===== GPU =====
00:02.0 VGA compatible controller [0300]: Intel Corporation Raptor Lake-S UHD Graphics [8086:a788] (rev 04)
	Subsystem: Acer Incorporated [ALI] Raptor Lake-S UHD Graphics [1025:1731]
	Kernel driver in use: i915
	Kernel modules: i915, xe
--
01:00.0 VGA compatible controller [0300]: NVIDIA Corporation AD107M [GeForce RTX 4060 Max-Q / Mobile] [10de:28e0] (rev a1)
	Subsystem: Acer Incorporated [ALI] AD107M [GeForce RTX 4060 Max-Q / Mobile] [1025:1731]
	Kernel driver in use: nvidia
	Kernel modules: nvidiafb, nouveau, nvidia_drm, nvidia

===== NIC(이더넷/무선) =====
00:14.3 Network controller [0280]: Intel Corporation Raptor Lake-S PCH CNVi WiFi [8086:7a70] (rev 11)
	Subsystem: Rivet Networks Device [1a56:1672]
	Kernel driver in use: iwlwifi
	Kernel modules: iwlwifi
00:15.0 Serial bus controller [0c80]: Intel Corporation Raptor Lake Serial IO I2C Host Controller #0 [8086:7a4c] (rev 11)
--
44:00.0 Ethernet controller [0200]: Realtek Semiconductor Co., Ltd. Killer E3000 2.5GbE Controller [10ec:3000] (rev 06)
	Subsystem: Acer Incorporated [ALI] Killer E3000 2.5GbE Controller [1025:1731]
	Kernel driver in use: r8169
	Kernel modules: r8169

===== 인터페이스별 드라이버 =====
== enp68s0 ==
driver: r8169
version: 6.14.0-27-generic
firmware-version: rtl8125b-2_0.0.2 07/13/20
expansion-rom-version: 
bus-info: 0000:44:00.0
supports-statistics: yes
supports-test: no
supports-eeprom-access: no
supports-register-dump: yes
supports-priv-flags: no
== wlp0s20f3 ==
driver: iwlwifi
version: 6.14.0-27-generic
firmware-version: 89.202a2f7b.0 so-a0-gf-a0-89.uc
expansion-rom-version: 
bus-info: 0000:00:14.3
supports-statistics: yes
supports-test: no
supports-eeprom-access: no
supports-register-dump: no
supports-priv-flags: no
```
* kvm-vmi에서 완벽하게 구성해놓은 v7 기준으로는 5.4 커널이 빌드될텐데 내 네트워크 칩셋이 너무 최신이라 드라이버 미존재. dkms 백포트 빌드를 해야 하는데 무선 랜선은 복잡함. 따라서 유선랜선 Realtek Killer E3000 (10ec:3000)을 위한 r8125 드라이버를 미리 준비. 

iommu 키기
```
$ sudo dmesg | grep -i -e iommu -e dmar
뭐라고 뜨지만 IOMMU enable은 없음
$ sudo sed -i 's/^\(GRUB_CMDLINE_LINUX_DEFAULT="[^"]*\)"/\1 intel_iommu=on iommu=pt"/' /etc/default/grub
$ sudo update-grub
$ sudo reboot
$ sudo dmesg | grep -i -e iommu -e dmar
아까랑 다르게 IOMMU enable 있음
```

vfio 모듈 자동 로드
```
$ lsmod | grep vfio
$ echo -e "vfio\nvfio_pci\nvfio_iommu_type1\nvfio_virqfd" | sudo tee /etc/modules-load.d/vfio.conf
$ sudo update-initramfs -u
$ sudo reboot
$ lsmod | grep vfio
vfio_pci               16384  0
vfio_pci_core          94208  1 vfio_pci
irqbypass              12288  2 vfio_pci_core,kvm
vfio_iommu_type1       49152  0
vfio                   69632  3 vfio_pci_core,vfio_iommu_type1,vfio_pci
iommufd               118784  1 vfio
```
* 만약 자동이 아닌 수동으로 킬거면 매번 sudo modprobe vfio vfio_pci vfio_iommu_type1 vfio_virqfd

r8125 드라이버 다운받아놓기
```
mkdir -p ~/drivers/r8125
https://www.realtek.com/Download/List?cate_id=584 들어가 2.5G Ethernet LINUX driver r8125 (r8125-9.016.00.tar.bz2) 다운. 위 폴더에 옮겨놓기
```

### > kvm-vmi v7

kvm-vmi? vmi를 실행할 수 있도록 커널(kvm), QEMU(qemu), libkvmi, libvmi 각 레포 버전을 맞춰 모아둔 레포. 기존에 있는 qemu/kvm은 기능도 적고 호환 문제가 터지니 이 레포에 있는 일반 generic 커널이 아닌 kvmi+ 이 추가된 kvm 커널을 만들고 qemu를 설치한다.

필수 패키지 설치
```
sudo apt update
sudo apt install -y build-essential git cmake pkg-config \
  libvirt-daemon-system libvirt-clients qemu-utils virt-manager ovmf bridge-utils \
  libglib2.0-dev libpixman-1-dev zlib1g-dev libspice-server-dev \
  check libjson-c-dev libtool autoconf automake bison flex \
  libelf-dev libssl-dev dwarves bc fakeroot
sudo apt install -y \
  build-essential git fakeroot bc flex bison libelf-dev libssl-dev \
  libncurses-dev dwarves xz-utils
sudo apt install -y \
  libvirt-daemon-system libvirt-clients virt-manager \
  ovmf swtpm virtiofsd \
  bridge-utils dnsmasq
sudo usermod -aG libvirt $USER
```

kvm-vmi 소스 가져오기
```
git clone --recursive https://github.com/KVM-VMI/kvm-vmi.git
```

kvm(커널) 설정
```
cd kvm-vmi/kvm

# 산출물 초기화
make clean
# config 완전 초기화
make mrproper

# x86_64 기본값에서 시작
make x86_64_defconfig

# 우분투 호환 스위치(가이드 기재)
./scripts/config --enable PREEMPT
./scripts/config --disable NET_VENDOR_NETRONOME

# 기본/부팅 필수
./scripts/config --enable BLK_DEV_INITRD
./scripts/config --enable DEVTMPFS --enable DEVTMPFS_MOUNT
./scripts/config --enable PCI --enable PCIEPORTBUS --enable PCI_MSI
./scripts/config --set-str CONFIG_LOCALVERSION -kvmi

# 스토리지 (루트: NVMe)
./scripts/config --enable NVME_CORE
./scripts/config --enable BLK_DEV_NVME
# SATA 장치가 있으면:
./scripts/config --module SATA_AHCI
# x86에 보통 불필요
./scripts/config --disable AHCI_PLATFORM
# VMD는 BIOS에서 꺼놨으니 불필요
./scripts/config --disable VMD

# 파일시스템
./scripts/config --enable EXT4_FS
./scripts/config --enable CRYPTO_CRC32C   # ext4/jbd2 체크섬 가속

# KVMI
./scripts/config --module KVM
./scripts/config --module KVM_INTEL
./scripts/config --enable KVM_INTROSPECTION
./scripts/config --disable TRANSPARENT_HUGEPAGE

# UEFI(선택이지만 켜도 무해)
./scripts/config --enable EFI_PARTITION
./scripts/config --enable EFI_STUB

# 불필요/문제 소지 드라이버 제거
./scripts/config --disable NET_VENDOR_NETRONOME
./scripts/config --disable NFP
# r8169은 r8125로 대체 예정이므로 끄면 깔끔
./scripts/config --disable R8169
# 오디오는 필요시 모듈로
./scripts/config --module SND_HDA_INTEL

# 변경 반영
make olddefconfig
```
* ssd가 인식되지 않고 busybox로 떨어져 빌트인으로 구성하였다.

빌드(.deb 생성) 및 설치
```
# CPU 코어수만큼 병렬 빌드
make -j"$(nproc)" bindeb-pkg WERROR=0 \
  KCFLAGS="-Wno-error=use-after-free" \
  HOSTCFLAGS="-Wno-error=use-after-free"

# 빌드 산출물은 부모 디렉터리에 생성됨
cd ..
ls -1 *.deb
linux-headers-5.4.24-kvmi+_5.4.24-kvmi+-1_amd64.deb
linux-image-5.4.24-kvmi+_5.4.24-kvmi+-1_amd64.deb
linux-libc-dev_5.4.24-kvmi+-1_amd64.deb

# 설치
sudo dpkg -i linux-image-*-kvmi+_*.deb linux-headers-*-kvmi+_*.deb
sudo update-grub
```

initramfs란? 부트로더가 커널과 함께 메모리로 올리는 작은 루트 파일시스템. cpio 아카이브이고, 보통 gzip/lz4/zstd로 압축되어 있다. 안에는 초기 사용자 공간 busybox, 저장장치/파일시스템 드라이버 모듈, udev 규칙, 암호화(LUKS)/RAID/LVM 스크립트 등이 들어 있다. 커널이 먼저 이 initramfs를 루트로 마운트해 필요한 모듈을 로드하고 실제 디스크의 진짜 루트(/)를 찾은 뒤 본 부팅을 이어간다.

initramfs 확인
```
# 확인
file /boot/initrd.img-5.4.24-kvmi+    # zstd는 예전 커널에서 못읽음 gzip compressed data 여야 함. 혹은 아예 ASCII cpio archive로 압축 미적용 initramfs 여도 괜찮

# 만약 그렇지 않다면
# 설정이 켜져 있는지 확인
grep -E 'CONFIG_BLK_DEV_INITRD|CONFIG_RD_GZIP|CONFIG_RD_LZ4|CONFIG_RD_ZSTD' /boot/config-5.4.24-kvmi+
CONFIG_BLK_DEV_INITRD=y
CONFIG_RD_GZIP=y
CONFIG_RD_LZ4=y

# 이것마저 꺼져 있으면
./scripts/config --enable BLK_DEV_INITRD --enable RD_GZIP --enable RD_LZ4 --enable RD_XZ --enable RD_LZO
로 재빌드

# initramfs-tools 압축 설정을 gzip으로
sudo sed -i 's/^COMPRESS=.*/COMPRESS=gzip/' /etc/initramfs-tools/initramfs.conf

# 5.4.24-kvmi+ 용 새 initramfs 생성(-c 새로 생성)
sudo update-initramfs -c -k 5.4.24-kvmi+

# 다시 확인
file /boot/initrd.img-5.4.24-kvmi+

sudo update-grub
reboot
```

텍스트 모드로 고정
```
# 현재 5.4로 부팅한 TTY에서
sudo systemctl set-default multi-user.target   # 다음 부팅부터 텍스트 모드
sudo systemctl disable --now gdm3              # 그래픽 로그인 매니저 중지
# 나중에 GUI로 되돌릴 땐
sudo systemctl set-default graphical.target && sudo systemctl enable --now gdm3
```
* 5.4 커널이 Raptor Lake iGPU(i915)를 다룰 수 없어 gui 모드는 무리이다.

준비해둔 r8125 설치
```
cd ~/drivers/r8125
tar xf r8125-*.tar.*
cd r8125-*/
sudo ./autorun.sh || { sudo make clean && sudo make && sudo make install; }

echo "blacklist r8169" | sudo tee /etc/modprobe.d/blacklist-r8169.conf
sudo update-initramfs -u -k 5.4.24-kvmi+
sudo reboot
```

부팅 후 확인
```
uname -r                                   # 5.4.24-kvmi+ 인지
lspci -nnk -d 10ec:3000                    # Kernel driver in use: r8125
sudo ethtool -i $(ip -o -4 route show default | awk '{print $5}')
```

> ⚠️ 텍스트 모드가 너무 불편해서 포기