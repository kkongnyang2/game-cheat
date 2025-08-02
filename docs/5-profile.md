## 윈도우 프로파일을 만들자

작성자: kkongnyang2 작성일: 2025-07-17

---

# libvmi 설치
git clone https://github.com/libvmi/libvmi.git
cd libvmi
mkdir build
cd build
cmake .. -DENABLE_KVM=ON -DENABLE_KVM_LEGACY=ON -DENABLE_XEN=OFF -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install && sudo ldconfig    # 설치와 라이브러리 갱신

# conf 파일
sudo nano /etc/libvmi.conf
win10 {
    driver = "kvm";
    ostype = "Windows";
    # 아래 항목은 나중에 JSON 프로파일 만들 때 채워도 OK
    # win_pdbase  = 0x18;
    # win_pid     = 0x84;
    # win_tasks   = 0x88;
}

# 전용 가상환경 만들기
sudo apt install -y python3-venv
python3 -m venv ~/vol3env
source ~/vol3env/bin/activate   # 터미널마다 작업 시 재실행

* 시스템 파이썬(/usr/lib/python)과 섞여 버전, 의존성이 충돌할까봐

# volatility3 설치
pip install --upgrade pip        # apt는 우분투 패키지, pip은 파이썬 패키지
pip install volatility3          # 최신 안정판 2.26.0이 설치됨
vol -h            # 도움말이 뜨면 정상

# PDB GUID 알아오기
echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope        # 권한 열어두기
sudo vmi-win-guid name win10
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

sudo vmi-win-guid name win10 는 메모리에 적재된 ntoskrnl.exe 의 PE 헤더를 스캔해서 “PDB GUID + Age” 값을 출력만 해 주는 도구


# PDB 받고 JSON 변환
python3 -m volatility3.framework.symbols.windows.pdbconv \
  -p ntkrnlmp.pdb \                                 # 파일 이름
  -g 89284d0ca6acc8274b9a44bd5af9290b1 \            # GUID+AGE 값
  -o ntkrnlmp-19045.json                            # 결과물

인터넷이 연결돼 있으면 pdbconv가 MS 서버에서 PDB를 내려받아 바로 JSON으로 변환해 준다.

# pyvmi 설치