## VMI를 실행해보자

작성자: kkongnyang2 작성일: 2025-08-02

---
### vmi 툴

VMI란? 이미 실행 중인 VM 내부 상태(메모리, 레지스터, 파일시스템 등)를 바깥(하이퍼바이저)에서 들여다보거나 조작하는 기술. 게스트 밖에서 이뤄지므로 안티치트 입장에서는 아무것도 알 수 없다.

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


### 필요한 게스트 정보

[필요정보]
  ├─ 식별: (domain name | domid | qemu pid)
  ├─ 주소변환: DTB (CR3)
  ├─ 해석: 프로파일(json) - 심볼 주소와 구조체 필드 오프셋
  └─ ostype, KASLR 여부, 권한
 
domid, 메모리 맵, DTB를 libvirt API로 자동으로 가져올 수 있음. 직접 qemu로 실행한 vm는 /proc/<pid>/mem 등을 직접 매핑해야 함

(1) VM 식별을 위한 domain name

domain name : libvirt에서 vm을 구분하는 문자열 이름. 사용자가 직접 정함
domain ID : libvirt가 실행 중인 vm에 부여하는 일시적 런타임 ID. virsh domid <name> 로 확인.
qemu PID : qemu 프로세스의 pid. ps aux | grep qemu-system-x86_64 같은 걸로 확인

(2) 주소 변환을 위한 DTB (Directory Table Base)
게스트 가상 주소(VA)를 물리 주소(PA)로 바꿀 때 필요한 페이지 테이블 루트 물리 주소. x86-64에서 CR3 레지스터가 가리킨다.
libvmi가 자동으로 찾아주기도 하지만, 실패하면 직접 지정
리눅스는 task_struct->mm->pgd에서 뽑고, 윈도우면 EPROCESS.DirectoryTableBase에서 뽑는다.

(3) 의미 있는 해석을 위한 프로파일(json 형식)
심볼 주소와 커널 구조체 필드 오프셋 정보.
리눅스면 System.map(공개 심볼 주소) + vmlinux를 바탕으로 dwarf2json(vilatility) 툴을 이용해 만든다.
윈도우면 PDB(program database)를 바탕으로 volatility/rekall/drakpdb 툴을 이용해 만든다.

(4) 그밖에도 KASLR(커널 주소 랜덤화. libvmi가 DTB를 찾기 어렵게 함) 사용 여부(linux_kaslr=true), 권한(root), /etc/libvmi.conf 접근성 들이 필요하다.


### libvmi 라이브러리 설치

libvmi = vmi를 위한 c 라이브러리

libvmi 설치
```
git clone https://github.com/libvmi/libvmi.git
cd libvmi
mkdir build
cd build
cmake .. -DENABLE_KVM=ON -DENABLE_KVM_LEGACY=ON -DENABLE_XEN=OFF -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install && sudo ldconfig    # 설치와 라이브러리 갱신
```

conf 파일 생성
```
sudo nano /etc/libvmi.conf
win10 {
    driver = "kvm";
    ostype = "Windows";
    # 아래 항목은 나중에 JSON 프로파일 만들 때 채워도 OK
}
```

### volatility3 툴 설치

volatility3 = 프로파일 만들기 전용 툴

전용 가상환경 만들기
```
sudo apt install -y python3-venv
python3 -m venv ~/vol3env       # 가상환경 생성
source ~/vol3env/bin/activate   # 현재 쉘의 path가 vol3env 아래를 바라보도록 하여 가상환경 진입(활성화) 터미널마다
deactivate                      # 가상환경 종료
python3 -m venv --clear ~/vol3env   # vol3env 전체를 지움
```
* 시스템 파이썬(/usr/lib/python)과 섞여 버전, 의존성이 충돌할까봐

volatility3 설치
```
pip install --upgrade pip        # apt는 우분투 패키지, pip은 파이썬 패키지
pip install volatility3          # 최신 안정판 2.26.0이 설치됨
vol -h            # 도움말이 뜨면 정상
```

### GUID 알아오기

GUID = 커널 바이너리 지문

vmi-win-guid
```
$ echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope        # 권한 열어두기
$ sudo vmi-win-guid name win10
Windows Kernel found @ 0x2400000
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
```
* sudo vmi-win-guid name win10 는 메모리에 적재된 ntoskrnl.exe 의 PE 헤더를 스캔해서 “PDB GUID + Age” 값을 출력만 해 주는 도구

### JSON 프로파일 만들기

패스스루 같은건 GUID에 영향이 없기에 한번만 만들어 놓으면 된다.

앞에서 찾은 GUID에 맞는 PDB 받고 JSON 변환
```
$ python3 -m volatility3.framework.symbols.windows.pdbconv \
  -p ntkrnlmp.pdb \                                 # 파일 이름
  -g 89284d0ca6acc8274b9a44bd5af9290b1 \            # GUID+AGE 값
  -o ntkrnlmp-19045.json                            # 결과물
```
* 인터넷이 연결돼 있으면 pdbconv가 MS 서버에서 PDB를 내려받아 바로 JSON으로 변환해 준다.
* DRAKVUF sandbox라는 전체 자동화 툴이 있음. 근데 intel cpu + 우분투 22.04 + xen 전용임.

conf 파일에 추가
```
$ sudo nano /etc/libvmi.conf
win10 {
    driver = "kvm";
    ostype = "Windows";
    json = "/home/kkongnyang2/ntkrnlmp-19045.json"
}
```

참고로 JSON은 “EPROCESS -> DirectoryTableBase 오프셋 = 0x28C” 같은 구조체·심볼 상대 오프셋만 담고, 절대 주소는 런타임 계산에 맡긴다. Base Address, CR3/DTB 값, 물리 주소 매핑 전부 런타임에 결정되기 때문이다.

### pyvmi 바인딩

pyvmi = vmi를 위한 python 확장 모듈

pyvmi 설치
```
# 빌드 도구와 의존성 설치
$ sudo apt install build-essential python3-dev libffi-dev \
                 libglib2.0-dev libjson-c-dev pkg-config git
$ python -m pip install --upgrade pip setuptools wheel pycparser cffi
# 소스판 설치(pip으로 만들어지는 격리를 막아 vol3env에 설치된 패키지 참조 가능)
$ python -m pip install --no-build-isolation "git+https://github.com/libvmi/python.git#egg=libvmi"
```

python 코드
```
$ mkdir -p ~/scripts
$ nano ~/scripts/vm_readmem.py
#!/usr/bin/env python3
from libvmi import Libvmi
import sys, struct

VM_NAME   = "win10"          # libvirt 이름
PROC_NAME = "notepad.exe"    # 찾을 프로세스
TARGET_VA = 0x7ffe0000       # 예: SharedUserData

vmi = Libvmi(VM_NAME)        # utils 없이도 attach OK

# ---- JSON에서 필드 offset 가져오기 ----
off_eproc_links = vmi.get_offset("WinEPROCESS", "ActiveProcessLinks")
off_eproc_name  = vmi.get_offset("WinEPROCESS", "ImageFileName")
off_eproc_pid   = vmi.get_offset("WinEPROCESS", "UniqueProcessId")
off_eproc_dtb   = vmi.get_offset("WinEPROCESS", "DirectoryTableBase")

# ---- PsActiveProcessHead → EPROCESS 리스트 순회 ----
head = vmi.translate_ksym2v("PsActiveProcessHead")
cur  = vmi.read_addr_va(head, 0, 0)         # LIST_ENTRY->Flink

found = False
while True:
    eproc = cur - off_eproc_links           # LIST_ENTRY 에서 구조체 기준 주소 계산
    name  = vmi.read_str_va(eproc + off_eproc_name, 15, 0).decode(errors="ignore")
    if name.lower() == PROC_NAME:
        pid = vmi.read_32_va(eproc + off_eproc_pid, 0)
        dtb = vmi.read_64_va(eproc + off_eproc_dtb, 0)
        found = True
        break
    cur = vmi.read_addr_va(cur, 0, 0)       # 다음 노드
    if cur == head:
        break                               # 리스트 한 바퀴

if not found:
    vmi.destroy()
    sys.exit(f"{PROC_NAME} not found")

# ---- 원하는 가상 주소 읽기 ----
phys = vmi.virtual_to_physical(TARGET_VA, dtb)
data = vmi.read(phys, 32)

print(f"[+] {PROC_NAME} PID={pid}  DTB={hex(dtb)}")
print(f"[+] {hex(TARGET_VA)} -> {data.hex()}")
vmi.destroy()
$ chmod +x ~/scripts/vm_readmem.py
$ sudo ~/scripts/vm_readmem.py
```

1. LibVMI가 VM에 attach → VCPU0 CR3 = 0x1aa000 ➜ 커널 DTB 확보
2. JSON에서  'EPROCESS.DirectoryTableBase' 오프셋 = 0x28C 확인
3. vmi_read_addr( KernelEPROCESS + 0x28C ) → 0x23e000 (System 프로세스 DTB)
4. vmi_translate_kv2p(0xfffff600...)  ⟹ 물리 주소 계산 성공
