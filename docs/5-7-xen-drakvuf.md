## xen drakvuf sandbox 방식으로 vmi를 구현해보자

작성자: kkongnyang2 작성일: 2025-08-08

---

### Xen 설치

Xen? 타입1 하이퍼바이저.

깔면 grub 화면에서 윈도우 / 우분투 / 우분투 on xen 세개 중에 선택가능.
윈도우 : UEFI ➜ Windows 부트 로더
우분투 : UEFI ➜ GRUB ➜ vmlinuz-...
우분투 on xen : UEFI ➜ GRUB ➜ xen-4.19.gz ➜ vmlinuz-...

즉 xen hypervisor 위에서 우분투를 올린 세번째 형태가 생긴 것임
부트로더(Grub) → Xen hypervisor → Dom0(리눅스 커널) → DomU(게스트 VM)

참고로 type 1 하이퍼바이저인데 Dom0 DomU 구분이 왜 있냐 하면, 그래도 하드웨어를 다루는 주 os는 있어야 하기 때문

빌드 도구 설치
```
$ sudo apt update
$ sudo apt-get install wget git build-essential make gcc clang bcc \
    libcurl4-openssl-dev libpci-dev zlib1g-dev bridge-utils
$ sudo apt install flex bison python3-dev libncurses-dev \
     libssl-dev libelf-dev libglib2.0-dev libpixman-1-dev \
     pkg-config gettext uuid-dev iasl libyajl-dev ninja-build meson
```

Xen 4.19.2
```
$ wget https://downloads.xenproject.org/release/xen/4.19.2/xen-4.19.2.tar.gz
$ tar xf xen-4.19.2.tar.gz
$ cd ~/xen-4.19.2
$ make distclean                      # 이전 빌드 산출물 삭제
$ ./configure --disable-docs          # 문서 없는 설정
$ make -j$(nproc) world               # 병렬 빌드
$ sudo make install
$ ls -lh /boot/xen-4.19*.gz         # 하이퍼바이저 이미지 확인
-rw-r--r-- 1 root root 1.2M  8월  5 14:54 /boot/xen-4.19.2.gz
lrwxrwxrwx 1 root root   13  8월  5 15:00 /boot/xen-4.19.gz -> xen-4.19.2.gz
```

### Xen 문제해결

라이브러리 위치 conf
```
$ sudo xl info | head                 # 헤드 info. xen으로 부팅된건지 체크하는거임
아직 일반 우분투이기에 cannot connect 는 맞지만, 라이브러리 오류까지 뜸
$ sudo find /usr/local -name 'libxl*.so*' -o -name 'libxen*so*'     # xen 라이브러리 위치 찾기
/usr/local/lib/libxencall.so.1.3
/usr/local/lib/libxlutil.so.4.19
(생략)
원래 /usr/lib 이 정상이지만 /usr/local/lib 에 설치된 것 확인
echo /usr/local/lib | sudo tee /etc/ld.so.conf.d/local.conf     # 위치 등록
$ sudo ldconfig                       # 캐시 재생성
$ sudo update-grub                    # grub 설정 갱신
```

secure boot 끄기
```
전원 껐다 키면 grub 메뉴에 xen ubuntu가 추가된 것 확인.
근데 Secure Boot(펌웨어 UEFI 의 보안 부팅)이 xen을 막아버려 부팅 실패
f2로 uefi 세팅창 들어가서 superviosr password를 4108로 설정해주고 secure boot를 끈다.
```

igpu만 사용
```
이번에는 부팅하며 하얀 글씨들이 쭉 뜨지만 검은 화면이 떠버림
nvidia 모듈이 xen과 호환이 안되며 생기는 문제. 어차피 dgpu는 패스스루할 거기에 igpu로만 부팅하자
uefi 들어가서 디스플레이를 optimus/dpgu only/auto select 중 auto select(mux) 선택.
$ sudo nano /etc/default/grub.d/xen.cfg
GRUB_CMDLINE_XEN_DEFAULT="dom0_mem=16384M,max:16384M dom0_max_vcpus=10 iommu=1 driver-dma-ops=1 pciback.hide=(0000:01:00.0)(0000:01:00.1)"
# max를 붙이면 ballon 기능이 꺼져 Dom0 메모리 고정
# 부팅 직후 xen 하이퍼바이저에게 VT-d를 활성화하라고 지시
# Dom0 커널이 소프트 바운스 버퍼 대신 표준 dma_ops + IOMMU 경로를 쓰도록 강제
# xen 시작 단계에서 해당 PCI 함수를 xen-pciback이 선점해 Dom0 커널이 못 잡게 만듦.
sudo update-grub                    # 커널 이미지 다시 만들기
sudo apt purge 'nvidia*'            # nvidia 관련된거 제거 및 블랙리스트
echo "blacklist nouveau" | sudo tee /etc/modprobe.d/blacklist-nouveau.conf    # nouveau도 블랙리스트
echo xen_pciback | sudo tee -a /etc/initramfs-tools/modules   # xen-pciback을 initramfs에 포함
sudo update-initramfs -u            # 디스크 이미지 다시 만들기
```

PCI 주소 확인
```
(0000:01:00.0)(0000:01:00.1)이 맞는지 혹시 몰라 확인
$ lspci -nn | grep -i -e vga -e audio
0000:00:02.0 VGA compatible controller [0300]: Intel Corporation Device [8086:a788] (rev 04)
0000:00:1f.3 Multimedia audio controller [0401]: Intel Corporation Device [8086:7a50] (rev 11)
0000:01:00.0 VGA compatible controller [0300]: NVIDIA Corporation Device [10de:28e0] (rev a1)
0000:01:00.1 Audio device [0403]: NVIDIA Corporation Device [10de:22be] (rev a1)
$ for g in /sys/kernel/iommu_groups/*; do
  echo "Group $(basename $g):"; ls -l $g/devices; echo; done
Group 15:
합계 0
lrwxrwxrwx 1 root root 0  8월  5 21:49 0000:01:00.0 -> ../../../../devices/pci0000:00/0000:00:01.0/0000:01:00.0
lrwxrwxrwx 1 root root 0  8월  5 21:49 0000:01:00.1 -> ../../../../devices/pci0000:00/0000:00:01.0/0000:01:00.1
```

wayland 대신 xorg로 변경
```
로그인 화면까지는 떴지만 충돌이 난건지 멈춰버림
$ sudo journalctl -k -b -1 -n 300 --no-pager | less   # 직전 부팅 커널 로그 마지막 300줄
kernel: BUG: Bad page state in process gdm-wayland-ses  pfn:103249
$ sudoedit /etc/gdm3/custom.conf
WaylandEnable=false
DefaultSession=gnome-xorg.desktop
$ sudo systemctl restart gdm3
```

sof 오디오 모듈 끄기
```
이제 들어가지지만 뭘 틀어도 실행이 계속 로딩중임
$ sudo journalctl -k -b -1 -n 300 --no-pager | less   # 직전 부팅 커널 로그 마지막 300줄
SOF 오디오(snd_sof_*)·i915 표준 초기화 메시지만 흘러가다 멈춤
소문난 에이서 12~14세대 노트북 이슈. SOF DSP가 펌웨어 로드에서 수 십 초 블로킹·GDM 타임아웃 유발
# sof 오디오 모듈 끄기
$ echo 'options snd_intel_dspcfg dlg_support=0' | sudo tee /etc/modprobe.d/disable-sof.conf
# sof 계열 모듈 전부 블랙리스트
$ echo "blacklist snd_sof_pci_intel_tgl
blacklist snd_sof_intel_hda_common
blacklist snd_sof_pci
blacklist snd_sof
" | sudo tee /etc/modprobe.d/blacklist-sof.conf
# HDA 모드만 강제
$ echo 'options snd_intel_dspcfg dsp_driver=1' | sudo tee /etc/modprobe.d/force-hda.conf
$ sudo update-initramfs -u
```


### Xen 부팅

잘 들어가지니 들어가 설정을 해주겠다

장치 modprobe
```
$ grep -i "Xen version" /var/log/dmesg
[    0.983837] kernel: Xen version: 4.19.2 (preserve-AD)
$ sudo modprobe xenfs xen_privcmd xen_evtchn xen_gntdev xen_gntalloc
$ ls -ld /proc/xen
dr-xr-xr-x 2 root root 0  8월  6 01:19 /proc/xen
$ ls -l /dev/xen/privcmd
crw------- 1 root root 10, 119  8월  6 01:23 /dev/xen/privcmd
$ lsmod | grep -E 'xen(fs|_evtchn|_privcmd)'
xenfs                  12288  0
xen_privcmd            32768  1 xenfs
$ sudo xl info | head
host                   : kkongnyang2-Predator-PHN16-72
release                : 6.8.0-65-generic
version                : #68~22.04.1-Ubuntu SMP PREEMPT_DYNAMIC Tue Jul 15 18:06:34 UTC 2
machine                : x86_64
nr_cpus                : 20
max_cpu_id             : 19
nr_nodes               : 1
cores_per_socket       : 10
threads_per_core       : 2
cpu_mhz                : 2803.200
```

데몬 경로 변경
```
일반적으로 /usr가 정상이지만 /usr/local에 설치됐었음.
그래서 xenstored 소켓을 통해 도메인 정보를 가져와야 하는데 데몬이 없어 xl list가 그대로 블록됨
$ sudo ln -s /usr/local/sbin/xenstored /usr/sbin/xenstored
$ sudo systemctl daemon-reload
$ sudo systemctl start xenstored
$ sudo xl list
Name                                        ID   Mem VCPUs	State	Time(s)
Domain-0                                     0 16384    10     r-----     298.0
```
wget https://download.qemu.org/qemu-8.2.2.tar.xz
tar xf qemu-8.2.2.tar.xz
cd qemu-8.2.2
./configure --target-list=x86_64-softmmu,i386-softmmu \
            --enable-xen              \
            --enable-system           \
            --disable-docs
make -j"$(nproc)"
sudo make install         # /usr/local/bin/qemu-system-x86_64 등
sudo ldconfig
sudo ln -sf /usr/local/bin/qemu-system-x86_64 /usr/local/lib/xen/bin/
sudo ln -sf /usr/local/bin/qemu-system-i386   /usr/local/lib/xen/bin/


### libvmi + DRAKVUF 설치

libvmi? vmi를 위한 c 라이브러리 엔진
drakvuf? 추적, 덤프하는 C 프로그램
drakvuf-sandbox? 총합 자동화 패키지

libvmi 라이브러리 설치
```
sudo apt install libjson-c-dev doxygen cmake
git clone https://github.com/libvmi/libvmi
cd libvmi
mkdir build && cd build
rm -rf *
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_XEN=ON  -DENABLE_KVM=OFF \
  -DENABLE_JSONC=ON
make -j$(nproc)
sudo make install
sudo ldconfig
```

DRAKVUF 프로그램 설치
```
sudo apt install clang-15 lld-15 llvm-15 llvm-15-dev
sudo ln -s /usr/bin/lld-15 /usr/bin/ld.lld
sudo ln -sf /usr/bin/llvm-ar-15 /usr/bin/llvm-ar
git clone https://github.com/tklengyel/drakvuf
cd drakvuf
meson setup build --native-file llvm.ini
ninja -C build
sudo ninja -C build install
sudo ldconfig
which drakvuf
/usr/local/bin/drakvuf
drakvuf --version
```

### drakvuf-sandbox 사용

root로 쉘 접속
```
sudo passwd root        # root용 비밀번호 설정
신규 비밀번호 : 4108
su                      # root로 접속
exit                    # 종료 명령어
```

실행
```
apt install iptables tcpdump dnsmasq qemu-utils bridge-utils libmagic1 redis-server python3.10-venv
python3 -m venv /opt/venv
source /opt/venv/bin/activate
pip install --upgrade pip
pip install drakvuf-sandbox
```

vm 생성
```
윈도우 이미지 설치를 위해선 두 개의 iso 파일과 빈 하드디스크 하나가 필요하다.

win10_22H2_English_x64.iso : 부팅 가능한 DVD 역할. vm이 이걸로 첫 부팅하며 윈도우 setup 실행.
virtio-win.iso : 드라이버 CD 역할. setup이 이 viostor.inf와 NetKVM.inf 를 읽어 virtio 가상 HDD를 인식하도록 함
win10.qcow2 : 빈 하드디스크. setup이 여기 파티션을 만들고 윈도우 파일을 복사하여 재부팅이 끝나면 윈도우가 들어 있는 디스크가 됨.

https://www.microsoft.com/software-download/windows10 들어가 영어(미국) 64비트 다운로드. Win10_22H2_English_x64v1.iso
https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/ 들어가 stable-virtio/virtio-win-0.1.271.iso 다운

sudo apt install qemu-system-x86 ovmf
drakrun install \
    "/home/kkongnyang2/다운로드/Win10_22H2_English_x64v1.iso" \
    --memory 12288 --vcpus 8
```


> ⚠️ 실패. vm이 안만들어짐. ovmf 호환 문제였음. 다음엔 seabios로 하면 해결될듯?