sudo passwd root        # root용 비밀번호 설정
신규 비밀번호 : 4108
su                      # root로 접속
exit                    # 종료 명령어

# 최신 패키지로 갱신
sudo apt update && sudo apt full-upgrade -y
# 헤더·툴체인·유틸리티 설치 (Ubuntu 22.04)
sudo apt install -y gcc make git \
  libyajl-dev uuid-dev libpci-dev iasl \
  texinfo texlive-fonts-extra python3-twisted

  # 1) 공통 툴체인
sudo apt update
sudo apt install build-essential clang meson ninja-build cmake \
                 autoconf automake libtool autoconf-archive \
                 pkg-config bison flex gettext python3-dev python3-pip -y

# 2) Xen/QEMU 의존성
sudo apt install libglib2.0-dev libpixman-1-dev libaio-dev \
                 libzstd-dev liblzma-dev libbz2-dev libssl-dev libyajl-dev \
                 libpci-dev libncurses5-dev uuid-dev libnl-3-dev \
                 libnl-route-3-dev iasl -y

# 3) LibVMI + DRAKVUF
sudo apt install libjansson-dev libjson-c-dev libcapstone-dev \
                 libfuse-dev libmagic-dev cabextract kpartx nasm llvm -y

# 4) drakrun / Sandbox 런타임
sudo apt install bridge-utils dnsmasq qemu-utils genisoimage tcpdump \
                 libmagic1 iptables redis-server nodejs npm python3-venv -y

# 작업 디렉터리
mkdir -p ~/src && cd ~/src
# 4.20 릴리스 태그 체크아웃
git clone -b RELEASE-4.20.0 https://xenbits.xenproject.org/git-http/xen.git
cd xen
# systemd 서비스 파일 생성 활성화
./configure --enable-systemd

# 전체 빌드 (하이퍼바이저 + 툴체인 + 문서)
make -j$(nproc) world          #  ≈ 30 – 40 분 소요
* make world는 내부적으로 make dist + make tools 등을 한꺼번에 수행합니다. Xen 4.20부터는 QEMU-Xen 서브트리 빌드도 자동 포함되므로 별도 make qemu-xen-dir 타깃은 없습니다

# 루트 권한으로 설치
sudo make install
sudo ldconfig                  # 새 라이브러리 캐시 반영

# 1-b) 소스 디렉터리 삭제
rm -rf ~/src/xen

# 1-c) /usr/local 아래에 설치된 Xen/QEMU-Xen 파일 제거
sudo rm -rf /usr/local/{bin,lib,libexec,include,share}/xen*
sudo rm -rf /usr/local/{bin,libexec}/qemu-system-* /usr/local/share/qemu

# 1-d) 모듈·헤더 흔적 제거(있다면)
sudo rm -rf /lib/modules/*/xen*  /usr/local/lib/modules

sudo systemctl disable --now dnsmasq.service
sudo apt install -y python3-tomli python3-setuptools python3-wheel python3-venv

mkdir -p ~/src && cd ~/src
git clone -b RELEASE-4.20.0 https://xenbits.xenproject.org/git-http/xen.git
cd xen
make distclean
./configure --enable-systemd --disable-docs 
make -j$(nproc) world
sudo make install
sudo ldconfig 


sudo mkdir -p /etc/default/grub.d
cat | sudo tee /etc/default/grub.d/xen.cfg >/dev/null <<'EOF'
# Xen 부트 파라미터
GRUB_CMDLINE_XEN_DEFAULT="dom0_mem=8192M,max:8192M"
EOF

# grub.cfg 재생성
sudo update-grub


secure boot 끄기
```
전원 껐다 키면 grub 메뉴에 xen ubuntu가 추가된 것 확인.
근데 Secure Boot(펌웨어 UEFI 의 보안 부팅)이 xen을 막아버려 부팅 실패
f2로 uefi 세팅창 들어가서 superviosr password를 4108로 설정해주고 secure boot를 끈다.
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



xl info | grep -E 'release|virt_caps'

xen_major : 4 / xen_minor : 20 → 빌드 성공

virt_caps 에 hvm 이 보이면 HVM(EPT/NPT) 지원 여부 확인 가능

uname -r            # generic 이면 일반 Ubuntu 커널
dmesg | grep -i xen  # 출력이 0 줄이면 Xen 안 올라감
ls /dev/xen          # 디렉터리 자체가 없을 가능성 큼

uname -r 예시

    5.15.0-...-generic → 일반 커널

    5.15.0-...-xen-dom0 (또는 xen-*) → Xen dom0 커널



ls /usr/local/lib/xen/bin/qemu-system-i386
ldd /usr/local/lib/xen/bin/qemu-system-i386 | grep not
xl create -n vm.cfg | grep device_model