## KVMI 방식으로 vmi를 구현해보자

작성자: kkongnyang2 작성일: 2025-08-10

---

### KVMI 소스

```
git clone https://github.com/KVM-VMI/kvm-vmi.git --recursive
cd kvm-vmi
git submodule update --init

# 각 서브모듈을 v12 브랜치로
cd kvm     && git checkout kvmi-v12 && cd ..
cd qemu    && git checkout kvmi-v12 && cd ..
```

### 커널

기본 6.14.0-27-generic 커널 (커널이미지+파일시스템)이 아닌 kvmi가 패치된 커널을 만들어줘야 한다. 5.15.0-rc2-kvmi+ 를 만들어주자.

kvmi 커널 이미지 만들기
```
$ sudo apt-get install -y bc fakeroot flex bison libelf-dev libssl-dev dwarves

$ cd kvm-vmi/kvm
$ cp /boot/config-$(uname -r) .config

# 서명키/리복키 제거, KVM+KVMI 켜기, 투명 Hugepage 끄기, 우분투 호환
$ ./scripts/config --disable SYSTEM_TRUSTED_KEYS
$ ./scripts/config --disable SYSTEM_REVOCATION_KEYS
$ ./scripts/config --module KVM
$ ./scripts/config --module KVM_INTEL
$ ./scripts/config --module KVM_AMD
$ ./scripts/config --enable KVM_INTROSPECTION
$ ./scripts/config --disable TRANSPARENT_HUGEPAGE
$ ./scripts/config --set-str CONFIG_LOCALVERSION -kvmi
$ ./scripts/config --enable PREEMPT

# 저장장치/USB 컨트롤러(루트 디스크 인식용) 확실히 켜두기
$ ./scripts/config --module VMD
$ ./scripts/config --module BLK_DEV_NVME
$ ./scripts/config --module SATA_AHCI
$ ./scripts/config --module USB_XHCI_HCD

# 서명 문제 해결
$ ./scripts/config --disable NET_VENDOR_NETRONOME    # 우분투 22.04용 메모, 24.04도 유사
$ ./scripts/config --disable MODULE_SIG
$ ./scripts/config --disable MODULE_SIG_ALL
$ ./scripts/config --undefine MODULE_SIG_KEY

# BTF/모듈 BTF/분할 BTF 끄기, DWARF5도 안전하게 끔 → DWARF4로
$ ./scripts/config --disable DEBUG_INFO_BTF
$ ./scripts/config --disable DEBUG_INFO_BTF_MODULES
$ ./scripts/config --disable PAHOLE_HAS_SPLIT_BTF || true
$ ./scripts/config --disable DEBUG_INFO_DWARF5 || true
$ ./scripts/config --enable  DEBUG_INFO_DWARF4

$ make olddefconfig

$ make -j"$(nproc)" bindeb-pkg WERROR=0 \
  KCFLAGS="-Wno-error=use-after-free" \
  HOSTCFLAGS="-Wno-error=use-after-free"

$ ls -1 ../linux-image-*.deb ../linux-headers-*.deb
../linux-headers-5.15.0-rc2-kvmi+_5.15.0-rc2-kvmi+-3_amd64.deb
../linux-headers-5.4.24-kvmi+_5.4.24-kvmi+-1_amd64.deb
../linux-image-5.15.0-rc2-kvmi+_5.15.0-rc2-kvmi+-3_amd64.deb
../linux-image-5.15.0-rc2-kvmi+-dbg_5.15.0-rc2-kvmi+-3_amd64.deb
../linux-image-5.4.24-kvmi+_5.4.24-kvmi+-1_amd64.deb
../linux-image-5.4.24-kvmi+-dbg_5.4.24-kvmi+-1_amd64.deb

$ sudo dpkg -i ../linux-image-*-kvmi+_*.deb

$ sudo update-grub
```

busybox 쉘까지만 들어가짐. 뭐가 문제냐? Intel VMD(=RST) 아래에 NVMe가 매달린 구성이라서, 커널이 그 VMD에 붙지 못해 루트 디스크를 못 보는 상황.

빌트인으로 구성
```
cd ~/kvm-vmi/kvm

# 기본 장치 트리/노드
./scripts/config --enable PCI
./scripts/config --enable PCIEPORTBUS
./scripts/config --enable PCI_MSI
./scripts/config --enable DEVTMPFS
./scripts/config --enable DEVTMPFS_MOUNT
./scripts/config --enable BLK_DEV_INITRD

# 저장장치(네 노트북에서 NVMe + AHCI + VMD 가능성 모두 커버)
./scripts/config --enable VMD
./scripts/config --enable NVME_CORE
./scripts/config --enable BLK_DEV_NVME
./scripts/config --enable SATA_AHCI
./scripts/config --enable AHCI_PLATFORM

# 루트 파일시스템에 맞춰 하나만 빌트인 (보통 ext4)
./scripts/config --enable EXT4_FS
# 만약 루트가 btrfs라면 위 대신
# ./scripts/config --enable BTRFS_FS

# LUKS/LVM이면 이것도 빌트인
./scripts/config --enable DM_MOD
./scripts/config --enable DM_CRYPT

# (선택) 모듈 압축 이슈 피하려면 아예 모듈 압축 끄기
./scripts/config --disable MODULE_COMPRESS

$ grep -E 'CONFIG_(VMD|NVME_CORE|BLK_DEV_NVME|DEVTMPFS|DEVTMPFS_MOUNT|EXT4_FS|BTRFS_FS)=' .config
CONFIG_VMD=y
CONFIG_DEVTMPFS=y
CONFIG_DEVTMPFS_MOUNT=y
CONFIG_NVME_CORE=y
CONFIG_BLK_DEV_NVME=y
CONFIG_EXT4_FS=y
CONFIG_BTRFS_FS=m

make olddefconfig

make -j"$(nproc)" bindeb-pkg WERROR=0 \
  KCFLAGS="-Wno-error=use-after-free" HOSTCFLAGS="-Wno-error=use-after-free"

sudo dpkg -i ../linux-image-*-kvmi+_*.deb
sudo update-grub
sudo reboot
```
* 이건 해결방법을 찾기 위해 해봤던 방법이고 별 의미 없을 것 같다.

VMD controller 끄기
```
부팅 중 F2로 바이오스 들어가서 advanced에서 main에서 ctrl+s 누르면 히든메뉴 나옴.
거기서 VMD controller를 끄면 NVMe가 직접 노출되고, 커널도 NVMe를 바로 보고 부팅된다.
```


### qemu 설치
```
sudo apt-get install -y libpixman-1-dev pkg-config zlib1g-dev \
  libglib2.0-dev dh-autoreconf libspice-server-dev

cd ~/kvm-vmi/qemu
git checkout kvmi-v12
./configure --target-list=x86_64-softmmu --enable-spice --prefix=/usr/local
make -j"$(nproc)"
sudo make install    # /usr/local/bin/qemu-system-x86_64
```

### libkvmi 설치
```
cd ~/kvm-vmi/libkvmi
./bootstrap && ./configure
make -j"$(nproc)"
sudo make install
sudo ldconfig
```

### vm 생성

iso 다운로드
```
https://www.microsoft.com/software-download/windows11 들어가 영어(미국) 64비트 다운로드. Win11_24H2_English_x64.iso. 윈도우 설치를 위한 DVD 역할.
```

vm 생성
```
$ virt-manager
새 vm 만들기
local install media - browse - downloads에 있는 Win11_24H2_English_x64.iso 선택. 윈도우 11.
메모리 16000MB, cpu 10개
저장소 활성화. 디스크 이미지 160.0GB. 이름 win11-2 (위치 /var/lib/libvirt/images/win11-2.qcow2)     # win11-2.qcow2 : 빈 하드디스크.
custom 선택
Overview에서 Q35, UEFI x86_64 : OVMF_CODE_4M.ms.fd 선택
CPUs에서 host-passthruogh 체크 확인
Add hardware - PCI Host Device - 0000:01:00:0과 0000:01:00:1 선택
완료 후 hdmi 꽂아놓고 설치
계정 입력 나오면 shift+f10 누르고 start ms-cxh:localonly. kkongnyang2에 암호 4108
인터넷이 연결되어 있으면 윈도우 드라이버 업데이트까지 설치 과정 중에 해줌
device manager 들어가 느낌표 없는지 확인(nvidia가 code43 안뜨는지)
```


### 실행
```
./hookguest-libkvmi /tmp/introspector
다른 쉘에서 virsh start win11-2
```

> ⚠️ 실패. v12는 아직 실험 단계라고 함. hookguest가 qemu/kvm의 kvmi abi와 서로 다른 버전이라 api 호출에서 뻗게 됨.