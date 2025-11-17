## 오버워치 메모리를 덤프해보자

작성자: kkongnyang2 작성일: 2025-08-30

---
### 섹션별 덤프파일 만들기

오버워치 프로세스 이미지를 덤프시키고 디스어셈블리로 풀어보자.
구조 및 난독화를 살펴보고 자동 AoB로 루틴 위치 찾기 + 복호화로 실주소 산출
gamemanager - entity_rawlist - parentpool - componentparent - componentbase

```
$ sudo vmi-process-list win10-seabios | grep -i overwatch
[11280] Overwatch.exe (struct addr:ffffd304d63ed080)

$ cd ~/game-cheat/code
$ sudo /home/kkongnyang2/vmi-venv/bin/python3 -m dump.vmi_list_sections \
  win10 11280 Overwatch.exe /root/symbols/ntkrnlmp.json
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

$ sudo /home/kkongnyang2/vmi-venv/bin/python3 -m dump.vmi_dump_section \
  win10 11280 Overwatch.exe /root/symbols/ntkrnlmp.json .text
VMI_WARNING: Invalid offset name in windows_get_offset (win_peb).
[info] module=Overwatch.exe base=0x00007ff73cdd0000
[dumped] Overwatch.exe_.text_0x7ff73cdd1000_0x2000000.bin  ok_pages=2965 missing=5227 size=33554432/33554432

$ sudo /home/kkongnyang2/vmi-venv/bin/python3 -m dump.vmi_dump_section \
  win10 11280 Overwatch.exe /root/symbols/ntkrnlmp.json .rdata
VMI_WARNING: Invalid offset name in windows_get_offset (win_peb).
[info] module=Overwatch.exe base=0x00007ff73cdd0000
[dumped] Overwatch.exe_.rdata_0x7ff73f96b000_0x916112.bin  ok_pages=1027 missing=1300 size=9527570/9527570

$ sudo /home/kkongnyang2/vmi-venv/bin/python3 -m dump.vmi_dump_section \
  win10 11280 Overwatch.exe /root/symbols/ntkrnlmp.json .data
VMI_WARNING: Invalid offset name in windows_get_offset (win_peb).
[info] module=Overwatch.exe base=0x00007ff73cdd0000
[dumped] Overwatch.exe_.data_0x7ff740282000_0x2ef800.bin  ok_pages=452 missing=299 size=3078144/3078144

$ sudo /home/kkongnyang2/vmi-venv/bin/python3 -m chain.walk_chain \
  win10 11280 Overwatch.exe /root/symbols/ntkrnlmp.json "+0x03603ED0" 64
[+] base=Overwatch.exe@0x00007ff73cdd0000
[+] final address = 0x00007ff7403d3ed0
00000000  62 8c d8 ad aa 4c 47 f0 e2 82 dc ad aa 4c 47 f0   b....LG......LG.
00000010  8a 80 ae ea 41 cf 47 f0 22 17 15 2b 49 4f 47 f0   ....A.G."..+IOG.
00000020  62 b7 a9 ea 41 cf 47 f0 22 a7 a9 ea 41 cf 47 f0   b...A.G."...A.G.
00000030  32 b4 a9 ea 41 cf 47 f0 a2 82 af ea 41 cf 47 f0   2...A.G.....A.G.
```


### 전체 덤프파일 생성

모듈 이미지 뿐 아니라 heap 영역까지 포함

```
$ cd ~/game-cheat/code
$ sudo /home/kkongnyang2/vmi-venv/bin/python3 -m dump.vmi_dump_process_quick \
  --domain win10 \
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
```

[ Module Image ]

  .text   0x00007FF627195000 ──────────────────────────────────────────────┐
           │  함수 본문/분기/점프테이블 일부                                │
  .rdata  0x00007FF629CFB000 ──┬─► vtable(함수포인터 테이블) ──► .text     │
           │                   └─ (XOR key: 0x00007FF627195000)             │
  .data   0x00007FF62A612000 ──┬─► 전역/싱글톤 포인터(일부 XOR)             │
           │                   └─► [component manager] → (heap 배열)  ← 찾는 중
  heap    0x000002EF00006000 ──┬─► 객체/컨테이너/테이블(이번에 A/B/C 확장) │
           │                    └─► [component* 배열] (연속 포인터)         │
           └────────────────────────────────────────────────────────────────┘

Deobf: decoded = raw ^ 0x00007FF627195000  (코드/테이블용 확인완료)

vftable = 가상 함수 주소 테이블. 상수 데이터를 적어놓는 rdata 섹션에 존재한다.
객체 메모리(구조체)의 보통 첫 필드에 vftable 포인터를 적어놓는다. 여기서 가상 함수 주소를 찾아서 돌아온다.
가상 함수란? base* p가 가르키는 실제 타입이 뭐냐에 따라 다른 동작을 런타임에 고르는 메커니즘.
쉽게 말해 마우스 좌클을 눌렀냐, 우클을 눌렀냐에 따라 다른 분기로 가게 되잖아.
이 케이스들을 미리 가상 함수로 만들어서 주소 테이블에 적어놓으면 객체 메모리에서 vftable을 뒤져서 사용하는 것.
그냥 구조체에 분기 포인터들을 여러 개 박아두면 되는거 아냐? 용량이 커지잖아.


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



* 툴로 사용할 예정
* 여기서 작성한 코드는 dump에 저장되어 있음.