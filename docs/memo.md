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