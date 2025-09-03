뭘 만들고 싶냐?

위치핵.

화면에 테투리 렌더링.

그리고 체력바 표시.

에임을 따라가는 원리는 무엇인가?
음..
렌더링 기준 상대의 헤드가 내 중앙으로 가게끔 마우스 입력값?
그걸 어떻게? 헤드가 올때까지 입력값 주는것. 위치 편차를 줄이는 방향으로 0으로 만들기.
정도는? 최대한 사람 흉내.

학습을 배워서 써먹을수 있을까? 꿀값말고 실제로 선수들 에이밍을.

아니면 에임이 맞으면 파란색으로 변해서 그때 쏘게하는 것도 재밌을거 같음.

비슷한 범위면 노란색. 이건 기계가 나머지를 조정해서 쏘도록.

너무 자동화말고 내가 통제하는 느낌을 주도록.

히스토리는 어떻게 하는거지? 시간 차를 조작해 판정이 넓어지게.

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


### > 게스트 측 보안 제거

안티치트가 Hyper-V 커널 레이러를 감지하면 바로 블루스크린.

step 1. 설정 › 앱 › 선택적 기능
```
    Hyper-V
    Windows Hypervisor Platform
    Virtual Machine Platform
    Windows Sandbox
    Windows Subsystem for Linux
    → 전부 체크 해제(제거) ▸ 재부팅
```

step 2. 설정
```
    core isolation → Memory Integrity OFF
    Optional: bcdedit /set hypervisorlaunchtype off
```


안티치트 논문 - VMI 자체를 게스트 쪽에서 감지하는 휴리스틱 제안
https://www.researchgate.net/publication/345646485_Who_watches_the_watcher_Detecting_hypervisor_introspection_from_unprivileged_guests



https://drakvuf-sandbox.readthedocs.io/en/latest/usage/getting_started.html?utm_source=chatgpt.com

Bug noted on 2025-04-24: Xen refers to old Tianocore OVMF version that refers to broken subhook submodule URL. If you’re affected by this bug, you’ll be asked for Git credentials while running make -j4 dist-tools and build process will be interrupted.

To overcome this issue, we changed the OVMF version to edk2-stable202408.01 by applying this patch to Config.mk:

- OVMF_UPSTREAM_URL ?= https://xenbits.xen.org/git-http/ovmf.git
- OVMF_UPSTREAM_REVISION ?= ba91d0292e593df8528b66f99c1b0b14fadc8e16
+ OVMF_UPSTREAM_URL ?= https://github.com/tianocore/edk2.git
+ OVMF_UPSTREAM_REVISION ?= 4dfdca63a93497203f197ec98ba20e2327e4afe4



kkongnyang2@acer:~$ sudo /home/kkongnyang2/vmi-venv/bin/python3 ~/game-cheat/code/chain/decode_component_slots.py \
>   --data /home/kkongnyang2/abc.exe_.data_0x7ff740282000_0x2ef800.bin \
  --slot-offs 0x3603ed0 0x36fa6e8 0x3603f30 \
  --module-base 0x7ff73cdd0000 \
  --out-json /home/kkongnyang2/decoded_component_bases.json
[i] Using module base = 0x7ff73cdd0000
[i] .data VA = 0x7ff740282000, size = 0x2ef800
[i] XOR key = 0xee222bb7ef934a1d
[i] Steps = [0, 8, 16, 24]
[+] Wrote decoded candidates -> /home/kkongnyang2/decoded_component_bases.json
[!] No plausible decoded pointers found in range. Try adjusting --key, --steps, or pointer bounds.
kkongnyang2@acer:~$ sudo nano ~/decoded_component_bases.json
kkongnyang2@acer:~$ sudo /home/kkongnyang2/vmi-venv/bin/python3 ~/game-cheat/code/chain/decode_component_slots.py   --data /home/kkongnyang2/abc.exe_.data_0x7ff740282000_0x2ef800.bin   --json /home/kkongnyang2/component_slot_candidates.json   --top 6   --verbose
[i] Using module base = 0x7ff73cdd0000
[i] .data VA = 0x7ff740282000, size = 0x2ef800
[i] XOR key = 0xee222bb7ef934a1d
[i] Steps = [0, 8, 16, 24], Rotations = [8, 13, 16, 24, 31]
[i] Canonicalization: mask48=ON, sign_extend=OFF
[+]            xor | slot 0x7ff7403d3ed0 +0x0 -> enc 0xf0474caaadd88c62  raw 0x1e65671d424bc67f  canon 0x671d424bc67f  (OK)
[+] ror_then_xor:8 | slot 0x7ff7403d3ed0 +0x0 -> enc 0xf0474caaadd88c62  raw 0x8cd26cfb453e9291  canon 0x6cfb453e9291  (OK)
[ ] ror_then_xor:13 | slot 0x7ff7403d3ed0 +0x0 -> enc 0xf0474caaadd88c62  raw 0x8d35a98d8ac624d9 (not in range)
[ ] ror_then_xor:16 | slot 0x7ff7403d3ed0 +0x0 -> enc 0xf0474caaadd88c62  raw 0x6240dbf0a339e7c5 (not in range)
[+] ror_then_xor:24 | slot 0x7ff7403d3ed0 +0x0 -> enc 0xf0474caaadd88c62  raw 0x36ae4947a8dfe0b0  canon 0x4947a8dfe0b0  (OK)
[+] ror_then_xor:31 | slot 0x7ff7403d3ed0 +0x0 -> enc 0xf0474caaadd88c62  raw 0xb59333720f1dd348  canon 0x33720f1dd348  (OK)
[+] xor_then_ror:8 | slot 0x7ff7403d3ed0 +0x0 -> enc 0xf0474caaadd88c62  raw 0x7f1e65671d424bc6  canon 0x65671d424bc6  (OK)
[ ] xor_then_ror:13 | slot 0x7ff7403d3ed0 +0x0 -> enc 0xf0474caaadd88c62  raw 0x33f8f32b38ea125e (not in range)
[+] xor_then_ror:16 | slot 0x7ff7403d3ed0 +0x0 -> enc 0xf0474caaadd88c62  raw 0xc67f1e65671d424b  canon 0x1e65671d424b  (OK)
[+] xor_then_ror:24 | slot 0x7ff7403d3ed0 +0x0 -> enc 0xf0474caaadd88c62  raw 0x4bc67f1e65671d42  canon 0x7f1e65671d42  (OK)
[ ] xor_then_ror:31 | slot 0x7ff7403d3ed0 +0x0 -> enc 0xf0474caaadd88c62  raw 0x84978cfe3ccace3a (not in range)
[+]            xor | slot 0x7ff7403d3ed0 +0x8 -> enc 0xf0474caaaddc82e2  raw 0x1e65671d424fc8ff  canon 0x671d424fc8ff  (OK)
[+] ror_then_xor:8 | slot 0x7ff7403d3ed0 +0x8 -> enc 0xf0474caaaddc82e2  raw 0xcd26cfb453e969f  canon 0x6cfb453e969f  (OK)
[ ] ror_then_xor:13 | slot 0x7ff7403d3ed0 +0x8 -> enc 0xf0474caaaddc82e2  raw 0xf935a98d8ac624f9 (not in range)
[ ] ror_then_xor:16 | slot 0x7ff7403d3ed0 +0x8 -> enc 0xf0474caaaddc82e2  raw 0x6cc0dbf0a339e7c1 (not in range)
[ ] ror_then_xor:24 | slot 0x7ff7403d3ed0 +0x8 -> enc 0xf0474caaaddc82e2  raw 0x32a0c947a8dfe0b0 (not in range)
[+] ror_then_xor:31 | slot 0x7ff7403d3ed0 +0x8 -> enc 0xf0474caaaddc82e2  raw 0xb59b2e720f1dd348  canon 0x2e720f1dd348  (OK)
[+] xor_then_ror:8 | slot 0x7ff7403d3ed0 +0x8 -> enc 0xf0474caaaddc82e2  raw 0xff1e65671d424fc8  canon 0x65671d424fc8  (OK)
[ ] xor_then_ror:13 | slot 0x7ff7403d3ed0 +0x8 -> enc 0xf0474caaaddc82e2  raw 0x47f8f32b38ea127e (not in range)
[+] xor_then_ror:16 | slot 0x7ff7403d3ed0 +0x8 -> enc 0xf0474caaaddc82e2  raw 0xc8ff1e65671d424f  canon 0x1e65671d424f  (OK)
[ ] xor_then_ror:24 | slot 0x7ff7403d3ed0 +0x8 -> enc 0xf0474caaaddc82e2  raw 0x4fc8ff1e65671d42 (not in range)
[ ] xor_then_ror:31 | slot 0x7ff7403d3ed0 +0x8 -> enc 0xf0474caaaddc82e2  raw 0x849f91fe3ccace3a (not in range)
[ ]            xor | slot 0x7ff7403d3ed0 +0x10 -> enc 0xf047cf41eaae808a  raw 0x1e65e4f6053dca97 (not in range)
[+] ror_then_xor:8 | slot 0x7ff7403d3ed0 +0x10 -> enc 0xf047cf41eaae808a  raw 0x64d26c78ae79e49d  canon 0x6c78ae79e49d  (OK)
[ ] ror_then_xor:13 | slot 0x7ff7403d3ed0 +0x10 -> enc 0xf047cf41eaae808a  raw 0xea75a989959c1f69 (not in range)
[ ] ror_then_xor:16 | slot 0x7ff7403d3ed0 +0x10 -> enc 0xf047cf41eaae808a  raw 0x6ea8dbf020d2a0b3 (not in range)
[ ] ror_then_xor:24 | slot 0x7ff7403d3ed0 +0x10 -> enc 0xf047cf41eaae808a  raw 0x40a2a147a85c0bf7 (not in range)
[+] ror_then_xor:31 | slot 0x7ff7403d3ed0 +0x10 -> enc 0xf047cf41eaae808a  raw 0x3b7f2aa20f1cd49e  canon 0x2aa20f1cd49e  (OK)
[+] xor_then_ror:8 | slot 0x7ff7403d3ed0 +0x10 -> enc 0xf047cf41eaae808a  raw 0x971e65e4f6053dca  canon 0x65e4f6053dca  (OK)
[ ] xor_then_ror:13 | slot 0x7ff7403d3ed0 +0x10 -> enc 0xf047cf41eaae808a  raw 0x54b8f32f27b029ee (not in range)
[+] xor_then_ror:16 | slot 0x7ff7403d3ed0 +0x10 -> enc 0xf047cf41eaae808a  raw 0xca971e65e4f6053d  canon 0x1e65e4f6053d  (OK)
[ ] xor_then_ror:24 | slot 0x7ff7403d3ed0 +0x10 -> enc 0xf047cf41eaae808a  raw 0x3dca971e65e4f605 (not in range)
[ ] xor_then_ror:31 | slot 0x7ff7403d3ed0 +0x10 -> enc 0xf047cf41eaae808a  raw 0xa7b952e3ccbc9ec (not in range)
[+]            xor | slot 0x7ff7403d3ed0 +0x18 -> enc 0xf0474f492b151722  raw 0x1e6564fec4865d3f  canon 0x64fec4865d3f  (OK)
[+] ror_then_xor:8 | slot 0x7ff7403d3ed0 +0x18 -> enc 0xf0474f492b151722  raw 0xccd26cf8a6b85f0a  canon 0x6cf8a6b85f0a  (OK)
[ ] ror_then_xor:13 | slot 0x7ff7403d3ed0 +0x18 -> enc 0xf0474f492b151722  raw 0x5735a98d95da12b5 (not in range)
[ ] ror_then_xor:16 | slot 0x7ff7403d3ed0 +0x18 -> enc 0xf0474f492b151722  raw 0xf900dbf0a0da6108 (not in range)
[+] ror_then_xor:24 | slot 0x7ff7403d3ed0 +0x18 -> enc 0xf0474f492b151722  raw 0xfb350947a8dc0336  canon 0x947a8dc0336  (OK)
[+] ror_then_xor:31 | slot 0x7ff7403d3ed0 +0x18 -> enc 0xf0474f492b151722  raw 0xb80805f20f1dd48f  canon 0x5f20f1dd48f  (OK)
[+] xor_then_ror:8 | slot 0x7ff7403d3ed0 +0x18 -> enc 0xf0474f492b151722  raw 0x3f1e6564fec4865d  canon 0x6564fec4865d  (OK)
[ ] xor_then_ror:13 | slot 0x7ff7403d3ed0 +0x18 -> enc 0xf0474f492b151722  raw 0xe9f8f32b27f62432 (not in range)
[+] xor_then_ror:16 | slot 0x7ff7403d3ed0 +0x18 -> enc 0xf0474f492b151722  raw 0x5d3f1e6564fec486  canon 0x1e6564fec486  (OK)
[+] xor_then_ror:24 | slot 0x7ff7403d3ed0 +0x18 -> enc 0xf0474f492b151722  raw 0x865d3f1e6564fec4  canon 0x3f1e6564fec4  (OK)
[ ] xor_then_ror:31 | slot 0x7ff7403d3ed0 +0x18 -> enc 0xf0474f492b151722  raw 0x890cba7e3ccac9fd (not in range)
[+]            xor | slot 0x7ff7404ca6e8 +0x0 -> enc 0x7ec123417811  raw 0xee225576ccd2320c  canon 0x5576ccd2320c  (OK)
[+] ror_then_xor:8 | slot 0x7ff7404ca6e8 +0x0 -> enc 0x7ec123417811  raw 0xff222bc92eb00b65  canon 0x2bc92eb00b65  (OK)
[+] ror_then_xor:13 | slot 0x7ff7404ca6e8 +0x0 -> enc 0x7ec123417811  raw 0x2eaa2bb4199a5016  canon 0x2bb4199a5016  (OK)
[+] ror_then_xor:16 | slot 0x7ff7404ca6e8 +0x0 -> enc 0x7ec123417811  raw 0x96332bb79152695c  canon 0x2bb79152695c  (OK)
[+] ror_then_xor:24 | slot 0x7ff7404ca6e8 +0x0 -> enc 0x7ec123417811  raw 0xaf5a3ab7efed8b3e  canon 0x3ab7efed8b3e  (OK)
[ ] ror_then_xor:31 | slot 0x7ff7404ca6e8 +0x0 -> enc 0x7ec123417811  raw 0xa8a0db95ef93b79f (not in range)
[+] xor_then_ror:8 | slot 0x7ff7404ca6e8 +0x0 -> enc 0x7ec123417811  raw 0xcee225576ccd232  canon 0x225576ccd232  (OK)
[+] xor_then_ror:13 | slot 0x7ff7404ca6e8 +0x0 -> enc 0x7ec123417811  raw 0x90677112abb66691  canon 0x7112abb66691  (OK)
[ ] xor_then_ror:16 | slot 0x7ff7404ca6e8 +0x0 -> enc 0x7ec123417811  raw 0x320cee225576ccd2 (not in range)
[+] xor_then_ror:24 | slot 0x7ff7404ca6e8 +0x0 -> enc 0x7ec123417811  raw 0xd2320cee225576cc  canon 0xcee225576cc  (OK)
[+] xor_then_ror:31 | slot 0x7ff7404ca6e8 +0x0 -> enc 0x7ec123417811  raw 0x99a46419dc44aaed  canon 0x6419dc44aaed  (OK)
[+]            xor | slot 0x7ff7404ca6e8 +0x8 -> enc 0x9875  raw 0xee222bb7ef93d268  canon 0x2bb7ef93d268  (OK)
[+] ror_then_xor:8 | slot 0x7ff7404ca6e8 +0x8 -> enc 0x9875  raw 0x9b222bb7ef934a85  canon 0x2bb7ef934a85  (OK)
[+] ror_then_xor:13 | slot 0x7ff7404ca6e8 +0x8 -> enc 0x9875  raw 0x2d8a2bb7ef934a19  canon 0x2bb7ef934a19  (OK)
[+] ror_then_xor:16 | slot 0x7ff7404ca6e8 +0x8 -> enc 0x9875  raw 0x76572bb7ef934a1d  canon 0x2bb7ef934a1d  (OK)
[+] ror_then_xor:24 | slot 0x7ff7404ca6e8 +0x8 -> enc 0x9875  raw 0xeeba5eb7ef934a1d  canon 0x5eb7ef934a1d  (OK)
[+] ror_then_xor:31 | slot 0x7ff7404ca6e8 +0x8 -> enc 0x9875  raw 0xee231b5def934a1d  canon 0x1b5def934a1d  (OK)
[+] xor_then_ror:8 | slot 0x7ff7404ca6e8 +0x8 -> enc 0x9875  raw 0x68ee222bb7ef93d2  canon 0x222bb7ef93d2  (OK)
[+] xor_then_ror:13 | slot 0x7ff7404ca6e8 +0x8 -> enc 0x9875  raw 0x934771115dbf7c9e  canon 0x71115dbf7c9e  (OK)
[ ] xor_then_ror:16 | slot 0x7ff7404ca6e8 +0x8 -> enc 0x9875  raw 0xd268ee222bb7ef93 (not in range)
[+] xor_then_ror:24 | slot 0x7ff7404ca6e8 +0x8 -> enc 0x9875  raw 0x93d268ee222bb7ef  canon 0x68ee222bb7ef  (OK)
[ ] xor_then_ror:31 | slot 0x7ff7404ca6e8 +0x8 -> enc 0x9875  raw 0xdf27a4d1dc44576f (not in range)
[+]            xor | slot 0x7ff7404ca6e8 +0x10 -> enc 0x0  raw 0xee222bb7ef934a1d  canon 0x2bb7ef934a1d  (OK)
[+] ror_then_xor:8 | slot 0x7ff7404ca6e8 +0x10 -> enc 0x0  raw 0xee222bb7ef934a1d  canon 0x2bb7ef934a1d  (OK)
[+] ror_then_xor:13 | slot 0x7ff7404ca6e8 +0x10 -> enc 0x0  raw 0xee222bb7ef934a1d  canon 0x2bb7ef934a1d  (OK)
[+] ror_then_xor:16 | slot 0x7ff7404ca6e8 +0x10 -> enc 0x0  raw 0xee222bb7ef934a1d  canon 0x2bb7ef934a1d  (OK)
[+] ror_then_xor:24 | slot 0x7ff7404ca6e8 +0x10 -> enc 0x0  raw 0xee222bb7ef934a1d  canon 0x2bb7ef934a1d  (OK)
[+] ror_then_xor:31 | slot 0x7ff7404ca6e8 +0x10 -> enc 0x0  raw 0xee222bb7ef934a1d  canon 0x2bb7ef934a1d  (OK)
[+] xor_then_ror:8 | slot 0x7ff7404ca6e8 +0x10 -> enc 0x0  raw 0x1dee222bb7ef934a  canon 0x222bb7ef934a  (OK)
[+] xor_then_ror:13 | slot 0x7ff7404ca6e8 +0x10 -> enc 0x0  raw 0x50ef71115dbf7c9a  canon 0x71115dbf7c9a  (OK)
[ ] xor_then_ror:16 | slot 0x7ff7404ca6e8 +0x10 -> enc 0x0  raw 0x4a1dee222bb7ef93 (not in range)
[+] xor_then_ror:24 | slot 0x7ff7404ca6e8 +0x10 -> enc 0x0  raw 0x934a1dee222bb7ef  canon 0x1dee222bb7ef  (OK)
[ ] xor_then_ror:31 | slot 0x7ff7404ca6e8 +0x10 -> enc 0x0  raw 0xdf26943bdc44576f (not in range)
[+]            xor | slot 0x7ff7404ca6e8 +0x18 -> enc 0x2e00000005  raw 0xee222b99ef934a18  canon 0x2b99ef934a18  (OK)
[+] ror_then_xor:8 | slot 0x7ff7404ca6e8 +0x18 -> enc 0x2e00000005  raw 0xeb222bb7c1934a1d  canon 0x2bb7c1934a1d  (OK)
[+] ror_then_xor:13 | slot 0x7ff7404ca6e8 +0x18 -> enc 0x2e00000005  raw 0xee0a2bb7eee34a1d  canon 0x2bb7eee34a1d  (OK)
[+] ror_then_xor:16 | slot 0x7ff7404ca6e8 +0x18 -> enc 0x2e00000005  raw 0xee272bb7efbd4a1d  canon 0x2bb7efbd4a1d  (OK)
[+] ror_then_xor:24 | slot 0x7ff7404ca6e8 +0x18 -> enc 0x2e00000005  raw 0xee222eb7ef93641d  canon 0x2eb7ef93641d  (OK)
[+] ror_then_xor:31 | slot 0x7ff7404ca6e8 +0x18 -> enc 0x2e00000005  raw 0xee222bbdef934a41  canon 0x2bbdef934a41  (OK)
[+] xor_then_ror:8 | slot 0x7ff7404ca6e8 +0x18 -> enc 0x2e00000005  raw 0x18ee222b99ef934a  canon 0x222b99ef934a  (OK)
[+] xor_then_ror:13 | slot 0x7ff7404ca6e8 +0x18 -> enc 0x2e00000005  raw 0x50c771115ccf7c9a  canon 0x71115ccf7c9a  (OK)
[ ] xor_then_ror:16 | slot 0x7ff7404ca6e8 +0x18 -> enc 0x2e00000005  raw 0x4a18ee222b99ef93 (not in range)
[+] xor_then_ror:24 | slot 0x7ff7404ca6e8 +0x18 -> enc 0x2e00000005  raw 0x934a18ee222b99ef  canon 0x18ee222b99ef  (OK)
[ ] xor_then_ror:31 | slot 0x7ff7404ca6e8 +0x18 -> enc 0x2e00000005  raw 0xdf269431dc445733 (not in range)
[+]            xor | slot 0x7ff7403d3f30 +0x0 -> enc 0xf0474caad3431722  raw 0x1e65671d3cd05d3f  canon 0x671d3cd05d3f  (OK)
[+] ror_then_xor:8 | slot 0x7ff7403d3f30 +0x0 -> enc 0xf0474caad3431722  raw 0xccd26cfb4540090a  canon 0x6cfb4540090a  (OK)
[ ] ror_then_xor:13 | slot 0x7ff7403d3f30 +0x0 -> enc 0xf0474caad3431722  raw 0x5735a98d8ac5d005 (not in range)
[ ] ror_then_xor:16 | slot 0x7ff7403d3f30 +0x0 -> enc 0xf0474caad3431722  raw 0xf900dbf0a339995e (not in range)
[+] ror_then_xor:24 | slot 0x7ff7403d3f30 +0x0 -> enc 0xf0474caad3431722  raw 0xad350947a8dfe0ce  canon 0x947a8dfe0ce  (OK)
[+] ror_then_xor:31 | slot 0x7ff7403d3f30 +0x0 -> enc 0xf0474caad3431722  raw 0x48a405f20f1dd348  canon 0x5f20f1dd348  (OK)
[+] xor_then_ror:8 | slot 0x7ff7403d3f30 +0x0 -> enc 0xf0474caad3431722  raw 0x3f1e65671d3cd05d  canon 0x65671d3cd05d  (OK)
[ ] xor_then_ror:13 | slot 0x7ff7403d3f30 +0x0 -> enc 0xf0474caad3431722  raw 0xe9f8f32b38e9e682 (not in range)
[+] xor_then_ror:16 | slot 0x7ff7403d3f30 +0x0 -> enc 0xf0474caad3431722  raw 0x5d3f1e65671d3cd0  canon 0x1e65671d3cd0  (OK)
[+] xor_then_ror:24 | slot 0x7ff7403d3f30 +0x0 -> enc 0xf0474caad3431722  raw 0xd05d3f1e65671d3c  canon 0x3f1e65671d3c  (OK)
[ ] xor_then_ror:31 | slot 0x7ff7403d3f30 +0x0 -> enc 0xf0474caad3431722  raw 0x79a0ba7e3ccace3a (not in range)
[ ]            xor | slot 0x7ff7403d3f30 +0x8 -> enc 0xf047cf41eaaabcf2  raw 0x1e65e4f60539f6ef (not in range)
[+] ror_then_xor:8 | slot 0x7ff7403d3f30 +0x8 -> enc 0xf047cf41eaaabcf2  raw 0x1cd26c78ae79e0a1  canon 0x6c78ae79e0a1  (OK)
[ ] ror_then_xor:13 | slot 0x7ff7403d3f30 +0x8 -> enc 0xf047cf41eaaabcf2  raw 0x9b5a989959c1f48 (not in range)
[ ] ror_then_xor:16 | slot 0x7ff7403d3f30 +0x8 -> enc 0xf047cf41eaaabcf2  raw 0x52d0dbf020d2a0b7 (not in range)
[ ] ror_then_xor:24 | slot 0x7ff7403d3f30 +0x8 -> enc 0xf047cf41eaaabcf2  raw 0x449ed947a85c0bf7 (not in range)
[+] ror_then_xor:31 | slot 0x7ff7403d3f30 +0x8 -> enc 0xf047cf41eaaabcf2  raw 0x3b7752520f1cd49e  canon 0x52520f1cd49e  (OK)
[+] xor_then_ror:8 | slot 0x7ff7403d3f30 +0x8 -> enc 0xf047cf41eaaabcf2  raw 0xef1e65e4f60539f6  canon 0x65e4f60539f6  (OK)
[ ] xor_then_ror:13 | slot 0x7ff7403d3f30 +0x8 -> enc 0xf047cf41eaaabcf2  raw 0xb778f32f27b029cf (not in range)
[+] xor_then_ror:16 | slot 0x7ff7403d3f30 +0x8 -> enc 0xf047cf41eaaabcf2  raw 0xf6ef1e65e4f60539  canon 0x1e65e4f60539  (OK)
[ ] xor_then_ror:24 | slot 0x7ff7403d3f30 +0x8 -> enc 0xf047cf41eaaabcf2  raw 0x39f6ef1e65e4f605 (not in range)
[ ] xor_then_ror:31 | slot 0x7ff7403d3f30 +0x8 -> enc 0xf047cf41eaaabcf2  raw 0xa73edde3ccbc9ec (not in range)
[ ]            xor | slot 0x7ff7403d3f30 +0x10 -> enc 0xf047cf41eaaabdf2  raw 0x1e65e4f60539f7ef (not in range)
[+] ror_then_xor:8 | slot 0x7ff7403d3f30 +0x10 -> enc 0xf047cf41eaaabdf2  raw 0x1cd26c78ae79e0a0  canon 0x6c78ae79e0a0  (OK)
[ ] ror_then_xor:13 | slot 0x7ff7403d3f30 +0x10 -> enc 0xf047cf41eaaabdf2  raw 0x1b5a989959c1f48 (not in range)
[ ] ror_then_xor:16 | slot 0x7ff7403d3f30 +0x10 -> enc 0xf047cf41eaaabdf2  raw 0x53d0dbf020d2a0b7 (not in range)
[ ] ror_then_xor:24 | slot 0x7ff7403d3f30 +0x10 -> enc 0xf047cf41eaaabdf2  raw 0x449fd947a85c0bf7 (not in range)
[+] ror_then_xor:31 | slot 0x7ff7403d3f30 +0x10 -> enc 0xf047cf41eaaabdf2  raw 0x3b7750520f1cd49e  canon 0x50520f1cd49e  (OK)
[+] xor_then_ror:8 | slot 0x7ff7403d3f30 +0x10 -> enc 0xf047cf41eaaabdf2  raw 0xef1e65e4f60539f7  canon 0x65e4f60539f7  (OK)
[ ] xor_then_ror:13 | slot 0x7ff7403d3f30 +0x10 -> enc 0xf047cf41eaaabdf2  raw 0xbf78f32f27b029cf (not in range)
[+] xor_then_ror:16 | slot 0x7ff7403d3f30 +0x10 -> enc 0xf047cf41eaaabdf2  raw 0xf7ef1e65e4f60539  canon 0x1e65e4f60539  (OK)
[ ] xor_then_ror:24 | slot 0x7ff7403d3f30 +0x10 -> enc 0xf047cf41eaaabdf2  raw 0x39f7ef1e65e4f605 (not in range)
[ ] xor_then_ror:31 | slot 0x7ff7403d3f30 +0x10 -> enc 0xf047cf41eaaabdf2  raw 0xa73efde3ccbc9ec (not in range)
[ ]            xor | slot 0x7ff7403d3f30 +0x18 -> enc 0xf047cf41eaa01c72  raw 0x1e65e4f60533566f (not in range)
[+] ror_then_xor:8 | slot 0x7ff7403d3f30 +0x18 -> enc 0xf047cf41eaa01c72  raw 0x9cd26c78ae79ea01  canon 0x6c78ae79ea01  (OK)
[ ] ror_then_xor:13 | slot 0x7ff7403d3f30 +0x18 -> enc 0xf047cf41eaa01c72  raw 0xdb5a989959c1f1d (not in range)
[ ] ror_then_xor:16 | slot 0x7ff7403d3f30 +0x18 -> enc 0xf047cf41eaa01c72  raw 0xf250dbf020d2a0bd (not in range)
[+] ror_then_xor:24 | slot 0x7ff7403d3f30 +0x18 -> enc 0xf047cf41eaa01c72  raw 0x4e3e5947a85c0bf7  canon 0x5947a85c0bf7  (OK)
[+] ror_then_xor:31 | slot 0x7ff7403d3f30 +0x18 -> enc 0xf047cf41eaa01c72  raw 0x3b6213520f1cd49e  canon 0x13520f1cd49e  (OK)
[+] xor_then_ror:8 | slot 0x7ff7403d3f30 +0x18 -> enc 0xf047cf41eaa01c72  raw 0x6f1e65e4f6053356  canon 0x65e4f6053356  (OK)
[ ] xor_then_ror:13 | slot 0x7ff7403d3f30 +0x18 -> enc 0xf047cf41eaa01c72  raw 0xb378f32f27b0299a (not in range)
[+] xor_then_ror:16 | slot 0x7ff7403d3f30 +0x18 -> enc 0xf047cf41eaa01c72  raw 0x566f1e65e4f60533  canon 0x1e65e4f60533  (OK)
[+] xor_then_ror:24 | slot 0x7ff7403d3f30 +0x18 -> enc 0xf047cf41eaa01c72  raw 0x33566f1e65e4f605  canon 0x6f1e65e4f605  (OK)
[ ] xor_then_ror:31 | slot 0x7ff7403d3f30 +0x18 -> enc 0xf047cf41eaa01c72  raw 0xa66acde3ccbc9ec (not in range)
[+]            xor | slot 0x7ff7402a50c0 +0x0 -> enc 0x7ff73facba10  raw 0xee225440d03ff00d  canon 0x5440d03ff00d  (OK)
[+] ror_then_xor:8 | slot 0x7ff7402a50c0 +0x0 -> enc 0x7ff73facba10  raw 0xfe222bc818ace6a7  canon 0x2bc818ace6a7  (OK)
[+] ror_then_xor:13 | slot 0x7ff7402a50c0 +0x0 -> enc 0x7ff73facba10  raw 0x3ea22bb4102ab778  canon 0x2bb4102ab778  (OK)
[+] ror_then_xor:16 | slot 0x7ff7402a50c0 +0x0 -> enc 0x7ff73facba10  raw 0x54322bb7906475b1  canon 0x2bb7906475b1  (OK)
[+] ror_then_xor:24 | slot 0x7ff7402a50c0 +0x0 -> enc 0x7ff73facba10  raw 0x42983bb7efecbd22  canon 0x3bb7efecbd22  (OK)
[+] ror_then_xor:31 | slot 0x7ff7402a50c0 +0x0 -> enc 0x7ff73facba10  raw 0x917b5f97ef93b5f3  canon 0x5f97ef93b5f3  (OK)
[+] xor_then_ror:8 | slot 0x7ff7402a50c0 +0x0 -> enc 0x7ff73facba10  raw 0xdee225440d03ff0  canon 0x225440d03ff0  (OK)
[+] xor_then_ror:13 | slot 0x7ff7402a50c0 +0x0 -> enc 0x7ff73facba10  raw 0x806f7112a20681ff  canon 0x7112a20681ff  (OK)
[ ] xor_then_ror:16 | slot 0x7ff7402a50c0 +0x0 -> enc 0x7ff73facba10  raw 0xf00dee225440d03f (not in range)
[+] xor_then_ror:24 | slot 0x7ff7402a50c0 +0x0 -> enc 0x7ff73facba10  raw 0x3ff00dee225440d0  canon 0xdee225440d0  (OK)
[ ] xor_then_ror:31 | slot 0x7ff7402a50c0 +0x0 -> enc 0x7ff73facba10  raw 0xa07fe01bdc44a881 (not in range)
[ ]            xor | slot 0x7ff7402a50c0 +0x8 -> enc 0xa245fde21e2f656d  raw 0x4c67d655f1bc2f70 (not in range)
[+] ror_then_xor:8 | slot 0x7ff7402a50c0 +0x8 -> enc 0xa245fde21e2f656d  raw 0x83806e4a0d8d6578  canon 0x6e4a0d8d6578  (OK)
[+] ror_then_xor:13 | slot 0x7ff7402a50c0 +0x8 -> enc 0xa245fde21e2f656d  raw 0xc54f39980083bb66  canon 0x39980083bb66  (OK)
[ ] ror_then_xor:16 | slot 0x7ff7402a50c0 +0x8 -> enc 0xa245fde21e2f656d  raw 0x8b4f89f212715432 (not in range)
[+] ror_then_xor:24 | slot 0x7ff7402a50c0 +0x8 -> enc 0xa245fde21e2f656d  raw 0xc1474615aa6ea803  canon 0x4615aa6ea803  (OK)
[ ] ror_then_xor:31 | slot 0x7ff7402a50c0 +0x8 -> enc 0xa245fde21e2f656d  raw 0xd27ce16cab18b1d9 (not in range)
[+] xor_then_ror:8 | slot 0x7ff7402a50c0 +0x8 -> enc 0xa245fde21e2f656d  raw 0x704c67d655f1bc2f  canon 0x67d655f1bc2f  (OK)
[+] xor_then_ror:13 | slot 0x7ff7402a50c0 +0x8 -> enc 0xa245fde21e2f656d  raw 0x7b82633eb2af8de1  canon 0x633eb2af8de1  (OK)
[+] xor_then_ror:16 | slot 0x7ff7402a50c0 +0x8 -> enc 0xa245fde21e2f656d  raw 0x2f704c67d655f1bc  canon 0x4c67d655f1bc  (OK)
[+] xor_then_ror:24 | slot 0x7ff7402a50c0 +0x8 -> enc 0xa245fde21e2f656d  raw 0xbc2f704c67d655f1  canon 0x704c67d655f1  (OK)
[+] xor_then_ror:31 | slot 0x7ff7402a50c0 +0x8 -> enc 0xa245fde21e2f656d  raw 0xe3785ee098cfacab  canon 0x5ee098cfacab  (OK)
[+]            xor | slot 0x7ff7402a50c0 +0x10 -> enc 0x4874585001000027  raw 0xa65673e7ee934a3a  canon 0x73e7ee934a3a  (OK)
[+] ror_then_xor:8 | slot 0x7ff7402a50c0 +0x10 -> enc 0x4874585001000027  raw 0xc96a5fefbf924a1d  canon 0x5fefbf924a1d  (OK)
[+] ror_then_xor:13 | slot 0x7ff7402a50c0 +0x10 -> enc 0x4874585001000027  raw 0xef1868152d13421d  canon 0x68152d13421d  (OK)
[+] ror_then_xor:16 | slot 0x7ff7402a50c0 +0x10 -> enc 0x4874585001000027  raw 0xee0563c3b7c34b1d  canon 0x63c3b7c34b1d  (OK)
[+] ror_then_xor:24 | slot 0x7ff7402a50c0 +0x10 -> enc 0x4874585001000027  raw 0xee220cff9bcb1a1c  canon 0xcff9bcb1a1c  (OK)
[+] ror_then_xor:31 | slot 0x7ff7402a50c0 +0x10 -> enc 0x4874585001000027  raw 0xec222bf97f7bfabd  canon 0x2bf97f7bfabd  (OK)
[+] xor_then_ror:8 | slot 0x7ff7402a50c0 +0x10 -> enc 0x4874585001000027  raw 0x3aa65673e7ee934a  canon 0x5673e7ee934a  (OK)
[+] xor_then_ror:13 | slot 0x7ff7402a50c0 +0x10 -> enc 0x4874585001000027  raw 0x51d532b39f3f749a  canon 0x32b39f3f749a  (OK)
[ ] xor_then_ror:16 | slot 0x7ff7402a50c0 +0x10 -> enc 0x4874585001000027  raw 0x4a3aa65673e7ee93 (not in range)
[+] xor_then_ror:24 | slot 0x7ff7402a50c0 +0x10 -> enc 0x4874585001000027  raw 0x934a3aa65673e7ee  canon 0x3aa65673e7ee  (OK)
[ ] xor_then_ror:31 | slot 0x7ff7402a50c0 +0x10 -> enc 0x4874585001000027  raw 0xdd2694754cace7cf (not in range)
[+]            xor | slot 0x7ff7402a50c0 +0x18 -> enc 0x7b4d0808ef65a0a1  raw 0x956f23bf00f6eabc  canon 0x23bf00f6eabc  (OK)
[+] ror_then_xor:8 | slot 0x7ff7402a50c0 +0x18 -> enc 0x7b4d0808ef65a0a1  raw 0x4f5966bfe77c2fbd  canon 0x66bfe77c2fbd  (OK)
[ ] ror_then_xor:13 | slot 0x7ff7402a50c0 +0x18 -> enc 0x7b4d0808ef65a0a1  raw 0xeb29f1dfafd43130 (not in range)
[+] ror_then_xor:16 | slot 0x7ff7402a50c0 +0x18 -> enc 0x7b4d0808ef65a0a1  raw 0x4e8350fae79ba578  canon 0x50fae79ba578  (OK)
[ ] ror_then_xor:24 | slot 0x7ff7402a50c0 +0x18 -> enc 0x7b4d0808ef65a0a1  raw 0x8b828acca29b42f2 (not in range)
[+] ror_then_xor:31 | slot 0x7ff7402a50c0 +0x18 -> enc 0x7b4d0808ef65a0a1  raw 0x30e96af519095a0c  canon 0x6af519095a0c  (OK)
[+] xor_then_ror:8 | slot 0x7ff7402a50c0 +0x18 -> enc 0x7b4d0808ef65a0a1  raw 0xbc956f23bf00f6ea  canon 0x6f23bf00f6ea  (OK)
[ ] xor_then_ror:13 | slot 0x7ff7402a50c0 +0x18 -> enc 0x7b4d0808ef65a0a1  raw 0x55e4ab791df807b7 (not in range)
[ ] xor_then_ror:16 | slot 0x7ff7402a50c0 +0x18 -> enc 0x7b4d0808ef65a0a1  raw 0xeabc956f23bf00f6 (not in range)
[ ] xor_then_ror:24 | slot 0x7ff7402a50c0 +0x18 -> enc 0x7b4d0808ef65a0a1  raw 0xf6eabc956f23bf00 (not in range)
[ ] xor_then_ror:31 | slot 0x7ff7402a50c0 +0x18 -> enc 0x7b4d0808ef65a0a1  raw 0x1edd5792ade477e (not in range)
[+]            xor | slot 0x7ff7403cc118 +0x0 -> enc 0x21e11a70081  raw 0xee2229a9fe344a9c  canon 0x29a9fe344a9c  (OK)
[+] ror_then_xor:8 | slot 0x7ff7403cc118 +0x0 -> enc 0x21e11a70081  raw 0x6f222bb5f182ed1d  canon 0x2bb5f182ed1d  (OK)
[+] ror_then_xor:13 | slot 0x7ff7403cc118 +0x0 -> enc 0x21e11a70081  raw 0xea2a2bb7ff63c725  canon 0x2bb7ff63c725  (OK)
[+] ror_then_xor:16 | slot 0x7ff7403cc118 +0x0 -> enc 0x21e11a70081  raw 0xeea32bb7ed8d5bba  canon 0x2bb7ed8d5bba  (OK)
[ ] ror_then_xor:24 | slot 0x7ff7403cc118 +0x0 -> enc 0x21e11a70081  raw 0x4922aab7ef91540c (not in range)
[+] ror_then_xor:31 | slot 0x7ff7403cc118 +0x0 -> enc 0x21e11a70081  raw 0xcd6c2ab5ef934e21  canon 0x2ab5ef934e21  (OK)
[+] xor_then_ror:8 | slot 0x7ff7403cc118 +0x0 -> enc 0x21e11a70081  raw 0x9cee2229a9fe344a  canon 0x2229a9fe344a  (OK)
[+] xor_then_ror:13 | slot 0x7ff7403cc118 +0x0 -> enc 0x21e11a70081  raw 0x54e771114d4ff1a2  canon 0x71114d4ff1a2  (OK)
[ ] xor_then_ror:16 | slot 0x7ff7403cc118 +0x0 -> enc 0x21e11a70081  raw 0x4a9cee2229a9fe34 (not in range)
[ ] xor_then_ror:24 | slot 0x7ff7403cc118 +0x0 -> enc 0x21e11a70081  raw 0x344a9cee2229a9fe (not in range)
[ ] xor_then_ror:31 | slot 0x7ff7403cc118 +0x0 -> enc 0x21e11a70081  raw 0xfc689539dc445353 (not in range)
[+]            xor | slot 0x7ff7403cc118 +0x8 -> enc 0x79e6650a55f6b082  raw 0x97c44ebdba65fa9f  canon 0x4ebdba65fa9f  (OK)
[ ] ror_then_xor:8 | slot 0x7ff7403cc118 +0x8 -> enc 0x79e6650a55f6b082  raw 0x6c5bcdd2e5c6bcad (not in range)
[ ] ror_then_xor:13 | slot 0x7ff7403cc118 +0x8 -> enc 0x79e6650a55f6b082  raw 0x6a31e484c7c1e5a8 (not in range)
[+] ror_then_xor:16 | slot 0x7ff7403cc118 +0x8 -> enc 0x79e6650a55f6b082  raw 0x5ea052518a991feb  canon 0x52518a991feb  (OK)
[ ] ror_then_xor:24 | slot 0x7ff7403cc118 +0x8 -> enc 0x79e6650a55f6b082  raw 0x1892a9ce09f64048 (not in range)
[+] ror_then_xor:31 | slot 0x7ff7403cc118 +0x8 -> enc 0x79e6650a55f6b082  raw 0x45cf4ab31c5f8009  canon 0x4ab31c5f8009  (OK)
[ ] xor_then_ror:8 | slot 0x7ff7403cc118 +0x8 -> enc 0x79e6650a55f6b082  raw 0x9f97c44ebdba65fa (not in range)
[ ] xor_then_ror:13 | slot 0x7ff7403cc118 +0x8 -> enc 0x79e6650a55f6b082  raw 0xd4fcbe2275edd32f (not in range)
[ ] xor_then_ror:16 | slot 0x7ff7403cc118 +0x8 -> enc 0x79e6650a55f6b082  raw 0xfa9f97c44ebdba65 (not in range)
[ ] xor_then_ror:24 | slot 0x7ff7403cc118 +0x8 -> enc 0x79e6650a55f6b082  raw 0x65fa9f97c44ebdba (not in range)
[ ] xor_then_ror:31 | slot 0x7ff7403cc118 +0x8 -> enc 0x79e6650a55f6b082  raw 0x74cbf53f2f889d7b (not in range)
[+]            xor | slot 0x7ff7403cc118 +0x10 -> enc 0x666669440000000c  raw 0x884442f3ef934a11  canon 0x42f3ef934a11  (OK)
[+] ror_then_xor:8 | slot 0x7ff7403cc118 +0x10 -> enc 0x666669440000000c  raw 0xe2444ddeab934a1d  canon 0x4ddeab934a1d  (OK)
[+] ror_then_xor:13 | slot 0x7ff7403cc118 +0x10 -> enc 0x666669440000000c  raw 0xee411884a5b34a1d  canon 0x1884a5b34a1d  (OK)
[+] ror_then_xor:16 | slot 0x7ff7403cc118 +0x10 -> enc 0x666669440000000c  raw 0xee2e4dd186d74a1d  canon 0x4dd186d74a1d  (OK)
[+] ror_then_xor:24 | slot 0x7ff7403cc118 +0x10 -> enc 0x666669440000000c  raw 0xee2227d189fa0e1d  canon 0x27d189fa0e1d  (OK)
[+] ror_then_xor:31 | slot 0x7ff7403cc118 +0x10 -> enc 0x666669440000000c  raw 0xee222baf235f9895  canon 0x2baf235f9895  (OK)
[+] xor_then_ror:8 | slot 0x7ff7403cc118 +0x10 -> enc 0x666669440000000c  raw 0x11884442f3ef934a  canon 0x4442f3ef934a  (OK)
[+] xor_then_ror:13 | slot 0x7ff7403cc118 +0x10 -> enc 0x666669440000000c  raw 0x508c4222179f7c9a  canon 0x4222179f7c9a  (OK)
[ ] xor_then_ror:16 | slot 0x7ff7403cc118 +0x10 -> enc 0x666669440000000c  raw 0x4a11884442f3ef93 (not in range)
[+] xor_then_ror:24 | slot 0x7ff7403cc118 +0x10 -> enc 0x666669440000000c  raw 0x934a11884442f3ef  canon 0x11884442f3ef  (OK)
[ ] xor_then_ror:31 | slot 0x7ff7403cc118 +0x10 -> enc 0x666669440000000c  raw 0xdf269423108885e7 (not in range)
[+]            xor | slot 0x7ff7403cc118 +0x18 -> enc 0x726f6c6f43657375  raw 0x9c4d47d8acf63968  canon 0x47d8acf63968  (OK)
[+] ror_then_xor:8 | slot 0x7ff7403cc118 +0x18 -> enc 0x726f6c6f43657375  raw 0x9b5044db80d02f6e  canon 0x44db80d02f6e  (OK)
[ ] ror_then_xor:13 | slot 0x7ff7403cc118 +0x18 -> enc 0x726f6c6f43657375  raw 0x7589b8cc8ce95136 (not in range)
[+] ror_then_xor:16 | slot 0x7ff7403cc118 +0x18 -> enc 0x726f6c6f43657375  raw 0x9d5759d883fc0978  canon 0x59d883fc0978  (OK)
[+] ror_then_xor:24 | slot 0x7ff7403cc118 +0x18 -> enc 0x726f6c6f43657375  raw 0x8b515ec580ff255e  canon 0x5ec580ff255e  (OK)
[ ] ror_then_xor:31 | slot 0x7ff7403cc118 +0x18 -> enc 0x726f6c6f43657375  raw 0x68e8cd5d0b4d92c3 (not in range)
[+] xor_then_ror:8 | slot 0x7ff7403cc118 +0x18 -> enc 0x726f6c6f43657375  raw 0x689c4d47d8acf639  canon 0x4d47d8acf639  (OK)
[ ] xor_then_ror:13 | slot 0x7ff7403cc118 +0x18 -> enc 0x726f6c6f43657375  raw 0xcb44e26a3ec567b1 (not in range)
[ ] xor_then_ror:16 | slot 0x7ff7403cc118 +0x18 -> enc 0x726f6c6f43657375  raw 0x39689c4d47d8acf6 (not in range)
[+] xor_then_ror:24 | slot 0x7ff7403cc118 +0x18 -> enc 0x726f6c6f43657375  raw 0xf639689c4d47d8ac  canon 0x689c4d47d8ac  (OK)
[+] xor_then_ror:31 | slot 0x7ff7403cc118 +0x18 -> enc 0x726f6c6f43657375  raw 0x59ec72d1389a8fb1  canon 0x72d1389a8fb1  (OK)
[+] Wrote decoded candidates -> decoded_component_bases.json
kkongnyang2@acer:~$ 

