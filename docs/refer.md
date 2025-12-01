### esp

네트워크 복제: 서버가 AOI(관심영역) 안의 엔티티 상태(위치, 체력 등)를 내려줌 → 클라 ECS에 반영.

가시성 선별(클라):
뷰 프러스텀 컬링: 카메라 시야 밖이면 제외
포털/PVS/룸 시스템: 같은 공간만 남김(있는 게임)
오클루전 컬링(Hi-Z/쿼리): 벽 등으로 가려진 건 제외
LOD/거리/팀 규칙으로 추가 제외

드로우 리스트 생성: 위 필터를 통과한 것만 드로우콜 생성 → GPU 렌더링
→ 보통 **가려진 적은 ‘논리 데이터는 업데이트되지만, 메인 패스엔 드로우콜 자체가 없음’**이 정석.

따라서 component에 있는 모든 적의 위치 정보를 바탕으로 내가 직접 렌더링을 해줘야함.


### 트리거봇

아이디어: 로컬 클라이언트가 가진 적 좌표/본을 읽고, 내 시야 벡터·FOV와의 각도/거리가 임계치 이하면 발사.



### 내가 찾아야 할 정보

일단 전체 런타임 메모리 구조를 어느정도 파악.
헤드인지, 몸샷인지, 미스인지를 판별하는 변수를 찾아서 화면에 perfect/good/miss 띄어주기.

| 패턴                    | 구조 예시                                                              | 특징 & 힌트                                                                  |
| --------------------- | ------------------------------------------------------------------ | ------------------------------------------------------------------------ |
| **① Boolean 플래그**     | `c++ uint8_t bCrosshairOnEnemy; // 0/1`                            | \* 가장 단순·보편<br>\* 값이 0↔1만 변하므로 CE에서는 **1 Byte, Exact value**(0/1) 필터가 유효 |
| **② Enum/Bit-field**  | `c++ uint8_t CrosshairState; // 0 none, 1 enemy, 2 ally, 3 object` | \* 팀識別·NPC 구분이 필요한 게임<br>\* 스캔 시 0·1·2처럼 **소수의 정수**가 반복                  |
| **③ Target 포인터 + 거리** | `c++ struct HitInfo { void* pTarget; float fDist; }`               | \* TPS/FPS에서 ‘상대 객체’ 정보 재사용<br>\* `pTarget!=NULL` 여부만 봐도 조준 여부 판단 가능     |


###

애쉬 250
바스 350(주황)
캐서디 275
에코 225(파랑)
프레야 225
겐지 250
한조 250


###

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
