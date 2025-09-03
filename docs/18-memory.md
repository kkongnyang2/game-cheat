## 오버워치 메모리를 뜯어보자

작성자: kkongnyang2 작성일: 2025-08-30

---
### CE poiner scan 실패

우리의 최종 목표는 component base를 찾는 것. 그러나 단순 서칭하기엔 매 매칭마다, 런타임마다 달라져. 논리적인 체인 구조를 찾아야 한다.
PID: 12164
모듈 이미지 베이스: 0x00007ff6134e0000
오버워치에 CE를 붙이고 체력값 225부터 피 닳면 decrased value로 체력 component 후보군 다섯개 주소를 찾음.
2448997EDE8, 2449682E418 등등 모듈 이미지 베이스보다 앞인 힙 할당 영역임을 알 수 있음
그렇지만 pointer scan for this address로 체인을 찾으려 해도 아무것도 안뜸
왜? 코드에 포인터 부분이 난독화 및 xor로 가러져있기 때문. 상용 게임은 포인터가 평문 값으로 저장되어 있지 않고 매번 엔티티+타입+키로 계산해서 얻는다.
해결: 메모리를 전체 덤프시켜서 구조 및 난독화를 살펴보고 자동 AoB로 루틴 위치 찾기 + 복호화로 실주소 산출


따라서 이를 찾으려면 직접 gamemanager - entity_rawlist - parentpool - componentparent - componentbase 순으로 찾아가야해. 중간중간에 암호화되어 있는 부분은 풀고. 이 암호화를 풀기 위해서는 내가 만든 암호화 과정을 역으로 복호화해야해. 이때 나는 암호화를 어떻게 했을까? 우선 키 두개를 준비했어. 어디에? 모르겠어. 실행 exe에 있을지 아니면 뭐 ncrypt.dll 같은 곳에 넣어놨을지 예상이 안가. 이 키를 찾았다고 했을 때 그냥 임의로 서로 간에 빼고 더하고 나누고 해서 xor키를 만들도록 했어. 그리고 이를 이용해 table에서 주소를 뽑도록 했지. 그 주소가 gamemanager 베이스야. 그럼 이때 키는 어디서 난거고 table은 어디 있는거지?

오버워치 프로세스 메모리 전부를 덤프시키고 디스어셈블리로 풀어보자.


내가 보내준 섹션별 덤프 파일만으로는 메모리 구조 판정이 불가능해? 아니면 어느정도 메모리 구조 판정을 했기에 vptr이 존재할 것으로 예상한거야? 네가 덤프파일을 보고 예측한 구조를 알려줘(component base 찾기 관련된). 그리고 

네가 만들어준 판별 코드는 덤프 파일 시점 기준이야, 아니면 현 시점 기준이야? 덤프 기준이면 할당이 확실할 때 덤프를 해서 다시 보내줄게. 그런데 힙 쪽은 덤프를 못하나? 아니면 내가 할당 진행을 안해서 힙이 아예 안뜬건가. 섹션이 저번에 이렇게 나왔었잖아.
Name      RVA       VA(start)           VSize      RawSize   RawPtr  Perm  Chars
--------- --------- -------------------- ---------- --------- ------- ----- ----------
.text      0x00001000  0x00007ff73cdd1000  0x02b994fc  0x02b99600  0x000400  R-X   0x60000020
.rdata     0x02b9b000  0x00007ff73f96b000  0x00916112  0x00916200  0x2b99a00  R--   0x40000040
.data      0x034b2000  0x00007ff740282000  0x00c12a54  0x002ef800  0x34afc00  RW-   0xc0000040
.pdata     0x040c5000  0x00007ff740e95000  0x0021e058  0x0021e200  0x379f400  R--   0x40000040
_RDATA     0x042e4000  0x00007ff7410b4000  0x00030be0  0x00030c00  0x39bd600  R--   0x40000040
.rodata    0x04315000  0x00007ff7410e5000  0x00001260  0x00001400  0x39ee200  R--   0x40000040
.rsrc      0x04317000  0x00007ff7410e7000  0x00011350  0x00011400  0x39ef600  R--   0x40000040
.reloc     0x04329000  0x00007ff7410f9000  0x0009f35c  0x0009f400  0x3a00a00  R--   0x42000040

### 덤프파일 만들기

$ sudo vmi-process-list win10-seabios | grep -i overwatch
[11280] Overwatch.exe (struct addr:ffffd304d63ed080)

$ nano ~/game-cheat/code/chain/vmi_dump_sections.py

$ sudo /home/kkongnyang2/vmi-venv/bin/python3 ~/game-cheat/code/chain/vmi_dump_sections.py win10-seabios 11280 Overwatch.exe /root/symbols/ntkrnlmp.json .text
VMI_WARNING: Invalid offset name in windows_get_offset (win_peb).
[info] module=Overwatch.exe base=0x00007ff73cdd0000
/home/kkongnyang2/game-cheat/code/chain/vmi_dump_sections.py:76: DeprecationWarning: datetime.datetime.utcnow() is deprecated and scheduled for removal in a future version. Use timezone-aware objects to represent datetimes in UTC: datetime.datetime.now(datetime.UTC).
  "timestamp_utc": datetime.datetime.utcnow().isoformat()+"Z",
[dumped] Overwatch.exe_.text_0x7ff73cdd1000_0x2000000.bin  ok_pages=2965 missing=5227 size=33554432/33554432

$ sudo /home/kkongnyang2/vmi-venv/bin/python3 ~/game-cheat/code/chain/vmi_dump_sections.py win10-seabios 11280 Overwatch.exe /root/symbols/ntkrnlmp.json .rdata
VMI_WARNING: Invalid offset name in windows_get_offset (win_peb).
[info] module=Overwatch.exe base=0x00007ff73cdd0000
/home/kkongnyang2/game-cheat/code/chain/vmi_dump_sections.py:76: DeprecationWarning: datetime.datetime.utcnow() is deprecated and scheduled for removal in a future version. Use timezone-aware objects to represent datetimes in UTC: datetime.datetime.now(datetime.UTC).
  "timestamp_utc": datetime.datetime.utcnow().isoformat()+"Z",
[dumped] Overwatch.exe_.rdata_0x7ff73f96b000_0x916112.bin  ok_pages=1027 missing=1300 size=9527570/9527570

$ nano ~/game-cheat/code/chain/vmi_dump_section.py
$ nano ~/game-cheat/code/chain/vmi_list_sections.py

$ sudo /home/kkongnyang2/vmi-venv/bin/python3 \
  ~/game-cheat/code/chain/vmi_list_sections.py \
  win10-seabios 11280 Overwatch.exe /root/symbols/ntkrnlmp.json
VMI_WARNING: Invalid offset name in windows_get_offset (win_peb).
[module] Overwatch.exe  base=0x00007ff73cdd0000

Name      RVA       VA(start)           VSize      RawSize   RawPtr  Perm  Chars
--------- --------- -------------------- ---------- --------- ------- ----- ----------
.text      0x00001000  0x00007ff73cdd1000  0x02b994fc  0x02b99600  0x000400  R-X   0x60000020
.rdata     0x02b9b000  0x00007ff73f96b000  0x00916112  0x00916200  0x2b99a00  R--   0x40000040
.data      0x034b2000  0x00007ff740282000  0x00c12a54  0x002ef800  0x34afc00  RW-   0xc0000040
.pdata     0x040c5000  0x00007ff740e95000  0x0021e058  0x0021e200  0x379f400  R--   0x40000040
_RDATA     0x042e4000  0x00007ff7410b4000  0x00030be0  0x00030c00  0x39bd600  R--   0x40000040
.rodata    0x04315000  0x00007ff7410e5000  0x00001260  0x00001400  0x39ee200  R--   0x40000040
.rsrc      0x04317000  0x00007ff7410e7000  0x00011350  0x00011400  0x39ef600  R--   0x40000040
.reloc     0x04329000  0x00007ff7410f9000  0x0009f35c  0x0009f400  0x3a00a00  R--   0x42000040

$ sudo /home/kkongnyang2/vmi-venv/bin/python3 ~/game-cheat/code/chain/vmi_dump_sections.py win10-seabios 11280 Overwatch.exe /root/symbols/ntkrnlmp.json .data
VMI_WARNING: Invalid offset name in windows_get_offset (win_peb).
[info] module=Overwatch.exe base=0x00007ff73cdd0000
/home/kkongnyang2/game-cheat/code/chain/vmi_dump_sections.py:76: DeprecationWarning: datetime.datetime.utcnow() is deprecated and scheduled for removal in a future version. Use timezone-aware objects to represent datetimes in UTC: datetime.datetime.now(datetime.UTC).
  "timestamp_utc": datetime.datetime.utcnow().isoformat()+"Z",
[dumped] Overwatch.exe_.data_0x7ff740282000_0x2ef800.bin  ok_pages=452 missing=299 size=3078144/3078144


$ sudo /home/kkongnyang2/vmi-venv/bin/python3 ~/game-cheat/code/chain/walk_chain.py   win10-seabios 11280 Overwatch.exe /root/symbols/ntkrnlmp.json "+0x03603ED0" 64
[+] base=Overwatch.exe@0x00007ff73cdd0000
[+] final address = 0x00007ff7403d3ed0
00000000  62 8c d8 ad aa 4c 47 f0 e2 82 dc ad aa 4c 47 f0   b....LG......LG.
00000010  8a 80 ae ea 41 cf 47 f0 22 17 15 2b 49 4f 47 f0   ....A.G."..+IOG.
00000020  62 b7 a9 ea 41 cf 47 f0 22 a7 a9 ea 41 cf 47 f0   b...A.G."...A.G.
00000030  32 b4 a9 ea 41 cf 47 f0 a2 82 af ea 41 cf 47 f0   2...A.G.....A.G.

$ sudo /home/kkongnyang2/vmi-venv/bin/python3 ~/game-cheat/code/chain/rw_value.py   win10-seabios 11280 Overwatch.exe /root/symbols/ntkrnlmp.json "+0x03603ED0" --read --type u64
[+] final address: 0x00007ff7403d3ed0
u64 = 17313891588393962594 (0xf0474caaadd88c62)
$ sudo /home/kkongnyang2/vmi-venv/bin/python3 ~/game-cheat/code/chain/rw_value.py   win10-seabios 11280 Overwatch.exe /root/symbols/ntkrnlmp.json "+0x036FA6E8" --read --type u64
[+] final address: 0x00007ff7404ca6e8
u64 = 139367985281041 (0x00007ec123417811)
$ sudo /home/kkongnyang2/vmi-venv/bin/python3 ~/game-cheat/code/chain/rw_value.py   win10-seabios 11280 Overwatch.exe /root/symbols/ntkrnlmp.json "+0x03603F30" --read --type u64
[+] final address: 0x00007ff7403d3f30
u64 = 17313891589021701922 (0xf0474caad3431722)

### 

kkongnyang2@acer:~$ nano ~/game-cheat/code/chain/scan_component_slots.py
kkongnyang2@acer:~$ nano ~/game-cheat/code/chain/decode_component_slots.py
kkongnyang2@acer:~$ sudo /home/kkongnyang2/vmi-venv/bin/python3 ~/game-cheat/code/chain/scan_component_slots.py \                                    
  --text /home/kkongnyang2/abc.exe_.text_0x7ff73cdd1000_0x2000000.bin \
  --data /home/kkongnyang2/abc.exe_.data_0x7ff740282000_0x2ef800.bin \
  --out-json /home/kkongnyang2/component_slot_candidates.json
[sudo] password for kkongnyang2: 
[i] .text VA = 0x7ff73cdd1000, size = 0x2000000
[i] .data VA = 0x7ff740282000, size = 0x2ef800
[i] module base (heuristic) = 0x7ff73cdd0000
[i] FNV-1a(32) prime hits = 628 (scanning around first 628)
[i] Top .data targets near hashed-dispatch sites:
   1) slot_va 0x7ff7403d3ed0  off 0x3603ed0  refs=14
       ref @ .text 0x7ff73d48acc7
       ref @ .text 0x7ff73d48acc7
       ref @ .text 0x7ff73d48acc7
       ref @ .text 0x7ff73e9d8acd
       ref @ .text 0x7ff73e9d8acd
       ref @ .text 0x7ff73e9d8acd
   2) slot_va 0x7ff7404ca6e8  off 0x36fa6e8  refs=12
       ref @ .text 0x7ff73d48ac0f
       ref @ .text 0x7ff73d48ac0f
       ref @ .text 0x7ff73d8ba2d6
       ref @ .text 0x7ff73da48d1f
       ref @ .text 0x7ff73da48d1f
       ref @ .text 0x7ff73da48d1f
   3) slot_va 0x7ff7403d3f30  off 0x3603f30  refs=8
       ref @ .text 0x7ff73e9d8af9
       ref @ .text 0x7ff73e9d8af9
       ref @ .text 0x7ff73e9d8af9
       ref @ .text 0x7ff73e9d8af9
       ref @ .text 0x7ff73e9d8af9
       ref @ .text 0x7ff73e9d8af9
   4) slot_va 0x7ff7402a50c0  off 0x34d50c0  refs=4
       ref @ .text 0x7ff73d8ba405
       ref @ .text 0x7ff73d8ba44b
       ref @ .text 0x7ff73d8ba405
       ref @ .text 0x7ff73d8ba44b
   5) slot_va 0x7ff7403cc118  off 0x35fc118  refs=1
       ref @ .text 0x7ff73e71c7ac
[+] Wrote candidates JSON -> /home/kkongnyang2/component_slot_candidates.json
kkongnyang2@acer:~$ sudo /home/kkongnyang2/vmi-venv/bin/python3 ~/game-cheat/code/chain/decode_component_slots.py \                                           
  --data /home/kkongnyang2/abc.exe_.data_0x7ff740282000_0x2ef800.bin \
  --json /home/kkongnyang2/component_slot_candidates.json \
  --top 6 \                                                
  --out-json /home/kkongnyang2/decoded_component_bases.json
[i] Using module base = 0x7ff73cdd0000
[i] .data VA = 0x7ff740282000, size = 0x2ef800
[i] XOR key = 0xee222bb7ef934a1d
[i] Steps = [0, 8, 16, 24]
[+] Wrote decoded candidates -> /home/kkongnyang2/decoded_component_bases.json
[!] No plausible decoded pointers found in range. Try adjusting --key, --steps, or pointer bounds.
kkongnyang2@acer:~$ sudo nano ~/decoded_component_bases.json
kkongnyang2@acer:~$ sudo nano ~/component_slot_candidates.json


이제 모든 from에 레포 이름을 포함시켜라(ex chain.ist_helpers) 같은 경로에 있다고 해도 마찬가지이다
그리고 모든 레포 앞에 __init__.py라는 빈 파일을 넣어주면 절대경로를 서로 잘 인식한다.
실행은 ~/game-cheat/code에 들어가서 $ sudo /home/kkongnyang2/vmi-venv/bin/python3 -m  dump.vmi_list_sections   win10-seabios 4888 Overwatch.exe /root/symbols/ntkrnlmp.json

$ sudo /home/kkongnyang2/vmi-venv/bin/python3 -m  dump.vmi_list_sections   win10-seabios 4888 Overwatch.exe /root/symbols/ntkrnlmp.json
  --domain win10-seabios \
  --pid 4888 \
  --module Overwatch.exe --ist /root/symbols/ntkrnlmp.json \
  --out-dir /home/kkongnyang2/ow_full_dump \
  --va-min 0x10000 --va-max 0x00007FFFFFFFFFFF \
  --slot-offs 0x3603ed0 0x3603f30 --steps 0 8 24 \
  --chunk 0x100000 --page 0x1000 \
  --verbose

[dump] wrote /home/kkongnyang2/ow_full_dump/va_00007ffaf08a0000_1000.bin  (0x1000 bytes)
[dump] wrote /home/kkongnyang2/ow_full_dump/va_00007ffaf08b0000_167000.bin  (0x167000 bytes)
[dump] wrote /home/kkongnyang2/ow_full_dump/va_00007ffaf0a19000_6000.bin  (0x6000 bytes)
[dump] wrote /home/kkongnyang2/ow_full_dump/va_00007ffaf0a21000_87000.bin  (0x87000 bytes)
[i] VM resumed
[+] Wrote manifest -> /home/kkongnyang2/ow_full_dump/manifest.json
[+] Done.


### 모듈 종류

CE - memory viewer - 상단 메뉴 view - Enumerate DLLs and Symbols

게임 자체
- Overwatch.exe: 게임 메인 실행 파일(게임 루프 시작, 초기화와 모듈 로딩)
- Overwatch_loader.dll: 게임 전용 로더/부트스트랩 DLL(자원과 엔진 초기화 분리)
- bink2w64.dll: Bink 2 동영상 코덱(인트로와 컷신 재생)
- nvngx_dlss.dll: DLSS(업스케일/프레임 생성)용 NGX 게임 측 스텁
- vivoxsdk.dll: Vivox 보이스 채팅 SDK(인게임 음성 통신)

nvidia 드라이버
- _nvngx.dll: nvidia ngx 런타임(드라이버 측 DLSS/Reflex 연동)
- nvapi64.dll: nvidia 전용 gpu/디스플레이 제어 api
- nvcuda64.dll: CUDA 드라이버 api
- nvldumdx.dll: D3D11 UMD 로더(사용자 모드 드라이브 진입점)
- nvwgf2umx.dll: D3D11 사용자 모드 드라이버 본체
- nvdxgdmal64.dll: DGX/DMA 경로 관련 nvidia 구성요소
- nvgpucomp64.dll: nvidia 셰이더/파이프라인 컴파일 구성요소
- nvmessagebus.dll: nvidia 내부 메시지

directX/그래픽
- d3d11.dll: Direct3D 11 렌더링 API(OW2 기본 렌더러)
- dxgi.dll: 스왑체인/어댑터/표면 관리
- dxcore.dll: DXCore(어댑터 열거 및 선택)
- d3d12.dll: D3D12 런타임
- D3DSCache.dll: D3D 셰이더 캐시 매니저
- directxdatabasehelper.dll: 드라이버/셰이더 데이터베이스 헬퍼
- dcomp.dll: DirectComposition(합성,UI 효과)
- dwmapi.dll: DWM(창 합성) 인터페이스
- Microsoft.Internal.WarPal.dll: WARP(소프트웨어 래스터라이저) 관련 구성요소

입력
- Xinput1_4.dll: 게임패드(엑스박스 패드) 입력
- HID.DLL: HID(마우스/키보드/패드) 저수준 입력 API
- IMM32.DLL/MSCTF.dll: 키보드/IME(텍스트 서비스 프레임워크)
- inputhost.dll/textinputframwork.dll: 현대 입력 스택(터치/가상키보드)

UI
- oreMessaging.dll / CoreUIComponents.dll / Windows.UI.dll: 현대 UI/메시지 펌프 구성요소.
- uxtheme.dll / USER32.dll / GDI32.dll / gdi32full.dll / shcore.dll: 윈도우 UI/그림(스케일링 포함)

보안
- amsi.dll: AMSI(안티멀웨어 스캔 인터페이스, 스크립트/컨텐츠 스캔 훅)
- wldp.dll: Windows Lockdown Policy(AppX/서명 정책)
- WINTRUST.dll/ CRYPT43.dll/ cryptnet.dll: 서명 검증/인증서/CRL
- bcrypt.dll / bcryptPrimitives.dll: 해시/대칭키 등 CNG 원시 연산.
- ncrypt.dll / ncryptsslp.dll: 키 스토리지/SSL 제공자 연계.
- MpOav.dll: Microsoft Defender 실시간 검사(파일/메모리 접근 시 후킹).

오디오, 네트워킹, 보이스, 장치, 윈도우 런타임 모듈은 생략


$ sudo chown -R kkongnyang2:kkongnyang2 ~/ow_full_dump
$ tar -cvf f955.tar need
$ tar -cvf heap.tar need
$ cd ~/game-cheat/code
$ sudo /home/kkongnyang2/vmi-venv/bin/python3 -m  dump.slice_sections    ~/ow_full_dump/manifest.json
[.text] packed 842 chunks -> /home/kkongnyang2/ow_full_dump/text.tar
[.rdata] packed 274 chunks -> /home/kkongnyang2/ow_full_dump/rdata.tar
[.data] packed 270 chunks -> /home/kkongnyang2/ow_full_dump/data.tar
[done]
$ tar -cvf heap2.tar need/new

0x00007FF627195000