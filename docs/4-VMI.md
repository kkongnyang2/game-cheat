## VMI를 실행해보자

### 목표: VMI 구현
작성자: kkongnyang2 작성일: 2025-07-10

---

┌───────────────────────────────┐
│         Python 레벨           │
│  ┌────────┐   ┌──────────┐    │
│  │ PyVMI  │   │  r2vmi   │    │
│  └──┬─────┘   └─────┬────┘    │
│     │               │         │
├─────▼───────────────▼─────────┤
│          C 라이브러리         │
│  ┌────────┐   ┌──────────┐    │
│  │ libvmi │   │ libkvmi  │    │
│  └──┬─────┘   └─────┬────┘    │
├─────▼─────────────────────────┤
│        하이퍼바이저 층        │
│  ┌────────────┐  (옵션)       │
│  │ QEMU(KVM)  │───KVMI 패치   │
│  └────────────┘               │
├───────────────────────────────┤
│         Linux 커널(KVM)       │
│      (옵션: kvmi-intro patch) │
└───────────────────────────────┘

| 저장소                                 | 최근 주 브랜치                     | 빌드 도구                     | 핵심 출력물                         | 언제 필요한가?                                 |
| ----------------------------------- | ---------------------------- | ------------------------- | ------------------------------ | ---------------------------------------- |
| **github.com/libvmi/libvmi**        | `master` (Python 3, CMake)   | **CMake → make**          | `libvmi.so`, 헤더 & pkg-config   | *반드시* 필요. KVM-폴링만 써도 OK                  |
| **github.com/KVM-VMI/libkvmi**      | `kvmi-v6`, `kvmi-v7`         | **autotools**             | `libkvmi.so`, `<kvmi/kvmi.h>`  | Page-Guard, CR3 event 등 *이벤트 기반* 기능을 쓸 때 |
| **github.com/libvmi/python**        | `master`                     | `setup.py` (Cython)       | `pyvmi` 모듈                     | Python으로 스크립트 작성할 때                      |
| **github.com/Wenzel/r2vmi**         | `master`                     | `install.sh` 내부 `make`    | `libr2vmi.so` (radare2 plugin) | radare2에 *“디스어셈 + 메모리 보기”* 를 붙이고 싶을 때    |
| **github.com/qemu/qemu** (upstream) | `master` (6개월 단위 릴리스)        | **Meson/Ninja**           | `qemu-system-*.bin`            | 기본 KVM VM 실행. 최신 GLib·Python 3           |
| **github.com/KVM-VMI/qemu**         | `kvmi-v7` (또는 `kvmi-master`) | `configure`(옛) / Meson(신) | KVMI-확장 QEMU                   | libkvmi를 쓰려면 필수                          |
| **linux-kvm-kvmi 커널**               | `kvmi-5.15`, `kvmi-6.6` 등    | `make`                    | patched vmlinux, 모듈            | QEMU-KVMI의 이벤트를 실제로 받으려면 필수              |


PyVMI로 가상주소 덤프 테스트

import libvmi
vmi = libvmi.Libvmi("winvm")  # 게스트 도메인
buf = vmi.read_pa(0x1234000, 0x100)

r2vmi 로 프로세스 내부 디스어셈 확인

r2 vmi://winvm:explorer.exe

Xen 환경이라면 altp2m=1 부트 옵션으로 다중-EPT 활성화 후 LibVMI altp2m_enable() 호출해 페이지 가드 성능 측정.


### > 호스트 측 준비(폴링용)

sudo apt install -y build-essential git cmake autoconf automake \
                    libglib2.0-dev libjansson-dev libsqlite3-dev \
                    python3-dev python3-pip python3-venv \
                    libncurses5-dev libcapstone-dev \
                    libtool pkg-config
sudo apt install -y linux-headers-$(uname -r)           # kvm 헤더
sudo apt install -y libpixman-1-dev libjson-c-dev \
                    autotools-dev  ninja-build \
                    bison flex python2
sudo apt install -y libx11-dev libxext-dev libxrandr-dev \
                    libxinerama-dev libxcursor-dev \
                    libxi-dev libxxf86vm-dev libxkbcommon-dev \
                    libepoxy-dev libgl1-mesa-dev libgbm-dev \
                    libgtk-3-dev libsdl2-dev libslirp-dev

git clone https://gitlab.com/qemu-project/qemu.git
cd qemu
mkdir build
cd build
../configure --target-list=x86_64-softmmu --enable-kvm --disable-werror
ninja -j$(nproc)
ninja install

git clone https://github.com/libvmi/libvmi.git
cd libvmi
mkdir build
cd build
cmake .. -DENABLE_KVM=ON -DENABLE_XEN=OFF
make -j$(nproc)
sudo make install && sudo ldconfig

git clone https://github.com/libvmi/python.git pyvmi
cd pyvmi
python3 -m venv .venv && source .venv/bin/activate
pip install --upgrade pip cffi future
python setup.py install

### > 윈도우 이미지 설치

윈도우 이미지 설치를 위해선 두 개의 iso 파일과 빈 하드디스크 하나가 필요하다.

win10_22H2_English_x64.iso : 부팅 가능한 DVD 역할. vm이 이걸로 첫 부팅하며 윈도우 setup 실행.
virtio-win.iso : 드라이버 CD 역할. setup이 이 CD의 viostor.inf와 NetKVM.inf 를 읽어 virtio 가상 HDD를 인식하도록 함
win10.qcow2 : 빈 하드디스크. setup이 여기 파티션을 만들고 윈도우 파일을 복사하여 재부팅이 끝나면 윈도우가 들어 있는 디스크가 됨.

qemu-img create -f qcow2 ~/vmi-lab/win10.qcow2 60G
https://www.microsoft.com/ko-kr/software-download/windows10ISO?utm_source=chatgpt.com 들어가 영어(국제) 64비트 다운로드.
https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/ 들어가 stable-virtio/virtio-win-0.1.271.iso 다운
qemu-system-x86_64 \
  -machine q35,accel=kvm -cpu host -smp 4 -m 4G \
  -drive file=~/win10.qcow2,if=virtio,format=qcow2,cache=none \
  -cdrom ~/Downloads/Win10_22H2_EnglishInternational_x64v1.iso \
  -drive file=~/Downloads/virtio-win-0.1.271.iso,media=cdrom \
  -monitor stdio \
  -qmp unix:/tmp/win10-qmp.sock,server,nowait \
  -display sdl \
  -boot order=d         # 처음에만 hdd가 아닌 dvd로 부팅하기 위해. 다음에는 이거 없애야 정상적으로 켜짐.
setup 화면에서 드라이브 인식 못하니까 load driver 들가서 우리가 넣어놓은 것중 red hat virtio scsi controller(w10\viostor.inf) 드라이버 선택해 설치.
설치 완료되면 넣어뒀던 빈 하드디스크 인식하니 클릭.
그러면 알아서 윈도우 설치하고 들어가짐.
qemu 화면 나오는건 ctrl+alt+g 


### > 게스트 측 준비

cheat engine window 10 7.5 인터넷에서 다운
battle.net 설치


github 오픈소스 - LibVMI과 PyVMI 예제. Python 바인딩으로 프로세스 리스트와 메모리 읽기 데모.
https://github.com/maxking/pyvmi_example?utm_source=chatgpt.com

github 오픈소스 - r2vmi와 IntroVirt. LibVMI + Radare2 플러그인/KVM 전용 확장과 VMI 프레임워크
https://github.com/Wenzel/r2vmi?utm_source=chatgpt.com
https://github.com/IntroVirt/IntroVirt?utm_source=chatgpt.com

github 오픈소스 - pixelbot
https://github.com/AliShazly/pixelbot?utm_source=chatgpt.com


치트 소스
P. Karkallis, J. Blasco, G. Suarez-Tangil, and S. Pastrana. Detecting video-game injectors exchanged in game
cheating communities. In Computer Security – ESORICS 2021: 26th European Symposium on Research
in Computer Security, Darmstadt, Germany, October 4–8, 2021, Proceedings, Part I, page 305–324, Berlin,
Heidelberg, 2021. Springer-Verlag.