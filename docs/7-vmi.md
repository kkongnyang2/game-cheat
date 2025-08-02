## VMI를 실행해보자

### 목표: VMI 구현
작성자: kkongnyang2 작성일: 2025-07-24

---

### > vmi 툴

VMI란? 이미 실행 중인 VM 내부 상태(메모리, 레지스터, 파일시스템 등)를 바깥(하이퍼바이저)에서 들여다보거나 조작하는 기술.경
게스트 밖에서 이뤄지므로 안티치트 입장에서는 아무것도 알 수 없다.

┌─────────────────────────┐
│  호스트 OS / 하이퍼바이저│  ← LibVMI·DRAKVUF 같은 VMI 라이브러리
│  (VMI 대상 메모리 맵)   │
├─────────▽──────────────┤
│      게스트 VM          │  ← Windows 10 + 게임 + 안티치트
│  ┌───────────────┐      │
│  │  게임 프로세스 │      │
│  └───────────────┘      │
└─────────────────────────┘


VMI 프로그램 구성

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


### > 호스트 측(폴링용)

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


git clone https://github.com/libvmi/python.git pyvmi
cd pyvmi
python3 -m venv .venv && source .venv/bin/activate
pip install --upgrade pip cffi future
python setup.py install


### > 게스트 측

battle.net 다운 후 설치, 오버워치 설치
cheat engine window 10 7.5 인터넷에서 다운 후 설치

### > VMI를 위해 필요한 게스트 정보

[VMI 필요정보]
  ├─ 식별: (domain name | domid | qemu pid)
  ├─ 주소변환: DTB (CR3)
  ├─ 해석: 프로파일(json) - 심볼 주소와 구조체 필드 오프셋
  ├─ ostype, KASLR 여부, 권한
  └─ (libvirt 쓰면 대부분 자동 처리 쉬움)


libvirt로 vm을 만들어야 하는 이유?
가장 쉬운 길이기 때문. libvmi가 libvirt API로 domid, 메모리 맵, DTB를 자동으로 가져옴. 직접 qemu로 실행한 vm는 qemu pid를 써서 /proc/<pid>/mem 등을 직접 매핑해야 함

(1) VM 식별을 위한 domain name
libvirt 사용 시 domain name만 넣으면 libvmi가 domain ID나 qemu PID 알아서

* domain name : libvirt에서 vm을 구분하는 문자열 이름. 사용자가 직접 정함
* domain ID : libvirt가 실행 중인 vm에 부여하는 일시적 런타임 ID. virsh domid <name> 로 확인.
* qemu PID : qemu 프로세스의 pid. ps aux | grep qemu-system-x86_64 같은 걸로 확인

(2) 주소 변환을 위한 DTB (Directory Table Base)
게스트 가상 주소(VA)를 물리 주소(PA)로 바꿀 때 필요한 페이지 테이블 루트 물리리 주소. x86-64에서 CR3 레지스터가 가리킨다.
libvmi가 자동으로 찾아주기도 하지만, 실패하면 직접 지정
리눅스는 task_struct->mm->pgd에서 뽑고, 윈도우면 EPROCESS.DirectoryTableBase에서 뽑는다.

(3) 의미 있는 해석을 위한 프로파일(json 형식)
심볼 주소와 커널 구조체 필드 오프셋 정보.
리눅스면 System.map(공개 심볼 주소) + vmlinux를 바탕으로 dwarf2json(vilatility) 툴을 이용해 만든다.
윈도우면 PDB(program database)를 바탕으로 volatility/rekall/drakpdb 툴을 이용해 만든다.

(4) 그밖에도 KASLR(커널 주소 랜덤화. libvmi가 DTB를 찾기 어렵게 함) 사용 여부(linux_kaslr=true), 권한(root), /etc/libvmi.conf 접근성 들이 필요하다.


### > libvmi



/etc/libvmi.conf 작성
```
<keyname> {
    ostype = "Linux";               # 대상 OS 타입 (Linux 또는 Windows 등)
    domain = "<libvirt 도메인 이름>"; # virt-manager/virsh에서 쓰는 VM 이름
    # 또는 domid = <정수>;           # virsh domid로 확인한 숫자 ID

    # 주소 공간 변환 관련
    sysmap = "/path/to/System.map-6.8.0-31-generic";  # 커널 심볼 파일
    rekall_profile = "/path/to/linux-6.8.0-31.json";  # (선택) Volatility/rekall JSON 프로파일

    # DTB 지정 가능 (일반적으로 자동 탐지 시 생략)
    # dtb = 0x00000000f1234000;

    # Linux KASLR 사용시
    linux_kaslr = true;

    # (윈도우의 경우) win_...
    # ostype = "Windows";
    # win_profile = "/path/to/windows_profile.json";

    # 기타 옵션
    # kvm = 1;   # KVM 백엔드 사용 (신규 libvmi는 domain 지정만으로 자동)
    # shm_file = "/dev/shm/somefile"; # 공유 메모리 파일 (특수 상황)
}
```
권한 설정
```
sudo chown root:root /etc/libvmi.conf
sudo chmod 644 /etc/libvmi.conf
```
테스트
```
virsh list    # ub24-test 같은 VM이 running 상태
```

### > 프로파일 만들기

의미 있는 해석을 위해 게스트 os의 심볼과 구조체 정보를 가져오는 역할. PDB라는 데이터베이스를 받아와 이를 이용해 프로파일을 만든다. 사용하는 툴은 Volatiliy/Rekall/drakpdb 등. 이걸 LibVMI에 넣어줘야 잘 작동한다

         [PDB / DWARF]  <-- 디버그/타입 정보 (메모리 안 X)
                |
        (parse & convert)
                |
   +------------------------------+
   |  Volatility / Rekall / drakpdb |
   +------------------------------+
                |
           [JSON Profile]
                |
          /etc/libvmi.conf
                |
             LibVMI
         (live memory access)


* DRAKVUF sandbox라는 전체 자동화 툴이 있음. 근데 intel cpu + 우분투 22.04 + xen 전용임.

윈도우 커널(ntoskrnl.exe)의 필드 번호와 정확히 맞는 PDB를 microsoft symbol server에서 받아야 구조체 오프셋이 일치한다. winver으로 확인할 것.
C:\Windows\System32\ntoskrnl.exe 같은 커널 바이너리를 호스트로 가져옴.
PDB GUID/AGE를 추출하거나 symchk, dbghelp.dll을 이용해서 Microsoft Symbol Server에서 해당 PDB를 다운로드.
pdbparse, rekall/tools/windows/pefile.py, drakvuf/scripts/pdb2json.py 등으로 변환하여 JSON/rekall 프로파일 생성.
(버전이 맞으면 DRAKVUF, LibVMI 커뮤니티에서 이미 만들어둔 프로파일을 가져오는 방법도 있음)
/etc/libvmi.conf에 win_profile = "/path/to/windows-<build>-<arch>.json" 같은 식으로 지정.

참고로 volatility3은 PDB를 바로 쓰기도 해서, 거기서 심볼/오프셋을 확인하고 libvmi config에 필요한 주소만 일부 사용하는 혼합 방식도 가능. rekall은 자체 json 프로파일 형식을 사용.


### > pyvmi