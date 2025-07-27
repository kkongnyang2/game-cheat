## 핵 종류를 알아보자

### 목표: 시나리오 탐색
작성자: kkongnyang2 작성일: 2025-07-10

---

### > 가능한 시나리오

요즘 핵은 메모리에 넣는 internal 치트는 줄어들고, external 치트가 늘어났다.
내부 치트 : (1) 메모리 인젝션 (2) 커널 드라이버
외부 치트 : (1) 장치 이용 (2) 가상머신 이용

* AES-CTR 암호화로 네트워크 단에서 가로채는건 거의 불가능. 평문으로 적힌 메모리 순간 변조해야 함.

### > 메모리 인젝션

스냅샷이 AES-CTR로 암호화된다해도 네트워크 전송 중의 변조를 막아줄 뿐, 결국 클라이언트 메모리 안에서는 다시 평문이 된다. 따라서 그걸 읽어와 상대 데이터를 파악하고 그걸 바탕으로 본인 전송 command 패킷을 조작해서 보내면 된다.

오버워치는 이를 막기 위해 다음과 같은 조치를 취한다

서버가 각 클라이언트 FOV로 레이캐스팅해 보이는 오브젝트만 스냅샷에 포함해서 보낸다. 벽 뒤 자표, 히트박스, 쿨타임 등은 애초에 내려오지 않는다. 또한 클라이언트 측에도 안티치트를 넣어놓는다. warden 안티치트는 wow 시절부터 사용된 블리자드 내부 엔진으로, 서명 기반 프로세스 메모리 스캔 방식이다. Eidolon은 최근 패치에 일시 도입하여 조정 중으로, 코드 흐름 난독화 및 안티탐퍼 방식이다. 커널 레벨 드라이버까지는 도입하지 않았지만 battle.net 런처 권한으로 HW 브레이크, DMA 접근을 감시한다. 정적 해시뿐 아니라 런타임 무결성 검사, heap Re-randomization을 주기적으로 돌려 패턴 치트를 어렵게 만든다. 서버 측 Defense matrix 백엔드를 통해 시즌 12 전후로 AI 분석 + 행동 통계 파이프라인을 강화해 50만 계정 일괄 밴, 치터와 파티한 동료 4만 계정도 추가 제재 하였다. 콘솔의 XIM-류 마우스 변환기도 실시간 탐지한다. TLS 터널 내부에서도 시퀀스 넘버 + AES-CTR + HMAC을 써서 중간 삽입, 리플레이를 봉쇄한다. 서버는 물리 한계 검증을 매 틱마다 수행해 수치가 비정상이면 바로 취소한다.

### > 장치 이용 핵 시나리오

1. 게임 실행
2. PCle 버스 마스터로 원하는 물리주소 읽기
3. 캡처카드로 게임 화면 정보 읽기
4. 케이블을 통해 두번째 PC로 데이터 전송
5. ESP/에임봇 앱이 좌표, 체력 등 파싱해서 해석
6. 오버레이나 별도 모니터에 월핵 표시
7. KMBox, Cronus Zen 등이 패드/키보드 HID 신호로 변환하여 전송

### > 메모리 가져오기 - DMA 카드

Direct Memory Access 카드. PCle 슬롯에 꽂는 작은 FPGA 보드이다. 원래는 디버깅이나 포렌식 용도로 시스템 RAM을 고속으로 읽거나 쓸 때 쓰이는데, 치터들은 이 능력을 이용해 CPU, OS, 안티치트가 모르는 사이에 게임 프로세스의 메모리를 그대로 훑어본다.

안티치트의 대응 ->
게임 쪽에는 소프트웨어나 드라이버조차 설치되지 않으므로 전통적 프로세스 검사가 통하지 않는다. 이를 대응하기 위해 PCle 슬롯을 덤프하여 수상한 VID를 block한다. 또는 IOMMU를 켜서 메모리 맵 접근하려면 DMA Remapping 통과를 하도록 한다. 또는 서버 행동을 분석하거나, PCI config 공간의 ROM 해시를 비교해 펌웨어를 검사한다.

### > 화면 정보 가져오기 - 캡처 카드

캡처 카드가 뭐냐?
화면을 카메라 피드처럼 뽑아오기. 원래는 PC의 HDMI 출력을 받아 USB 3.0 등으로 두번째 PC에 보내고, OBS 같은 스트리머 앱이 인코딩, 송출한다.
이를 이용해 AI 에임봇에서는 치트 PC에 보내 YOLO-v8/ViT 등으로 적 실루엣, 머리 위치를 추적한다. 그래서 결과 좌표를 Cronus Zen, KMBox 같은 입력 장치로 변환해 역주입 하는 것이다.

### > 화면 정보 파싱하기 - YOLO-v8 / ViT

적 실루엣, 머리 위치를 추적한다.

### > 값 주입하기 - HID Cronus Zen / XIM

HID가 뭐냐?
Human Interface Device. 입력이면 다 여기로 통일. 키보드 마우스 게임패드 터치패드 전부.
운영체제는 VID(제조사)/PID(제품) 값만 보고 자동으로 범용 드라이버를 붙이므로, Cronus Zen, KMBox 같은 장치가 "나는 평범한 Xbox 패드다"라고만 선언하면 그대로 동작한다. 안티치트가 소프트웨어적으로 구분하기 어려운 이유가 바로 이 HID 위장성이다.

종류는 Cronus zen, XIM, Strike pack 등이 있다.

Cronus zen이 뭐냐?
컨트롤러에 매크로를 심는다. USB 동글 형태로, 한쪽은 패드, 다른 쪽은 콘솔에 연결한다. 그래서 입력을 읽어 올린 뒤, 내부 GPC 스크립트 로직을 수행하여 수정된 입력을 다시 패드콘솔로 전달한다. OS엔 순수 HID로 보이므로 드라이브나 레지스트리 흔적이 없다. 2025년부터 OW2는 비인가 VID/PID 감지 시 PC 풀 강제 매칭을 시작했다.

XIM이 뭐냐? 각 게임의 스틱 마우스 곡선을 분석해 가장 자연스러운 에임을 제공한다.

KMBox는 DMA, AI 세트의 로봇 팔이다. 작은 FPGA/MCU 보드에 2개의 USB(게이밍 PC와 치트 PC)로 구성되어 있다. 치트 PC가 에임 각도 등을 계산하여 KMBox로 명령을 전송하면 물리 HID 신호(정교한 마우스 델타, 키 입력)로 바꿔 게이밍 PC에 주입한다. 오즘은 USB가 아니라 LAN 포트로 하기도 한다.


### > 가상머신 이용 핵 시나리오

VMI (가상버신 인트로스펙션) 치트라고 불린다.

하이버바이저가 게스트 VM 안의 메모리를 직접 엿보고 게임 정보를 빼내거나 조작하는 방식이다. 치트 코드가 게임 프로세스 안, 윈도우 커널 안에 전혀 존재하지 않는다는 점을 이용해 안티치트를 우회한다.

host 물리 머신에 KVM/XEN/Hyper-V 등 하이퍼바이저가 존재하고, LibVMI나 altp2m API를 통해 게스트 RAM 페이지를 매핑한다. 또한 VMI Cheat 프로세스를 돌려 월핵과 trigger-bot 등을 작동시킨다. Guest 가상 머신에는 일반 윈도우를 설치하고 게임이 돌아간다. 따라서 VMI 라이브러리가 SLAT 테이블을 따라가서 원하는 게스트 가상주소를 호스트 물리주소로 한산하여 메모리를 덤프시키면 된다. 그래서 필요한 값을 읽거나 써서 치트를 실행한다.

방식은 폴링과 Page-guard 방식이 있다. 폴링은 초당 60x 메모리 블록 읽어 Radar / Wallhack 정보를 획득하는 것이다. page-guard 방식은 특정 좌표 값이 바뀌면 트리거봇이 바로 총을 발사하는 것이다. 그리고 OVerlay 스레드가 OpenGL 윈도우에 월핵 스켈레톤을 렌더링하고, 입력 스레드는 KMBox를 통해 마우스 Δ 값을 전송한다.

클라우드형으로 치트 서비스도 가능하다. AWS m6a 인스턴스에서도 성공 사례 보고.


### > VMI 치트의 안티치트

안티치트의 대응 -> 가상화 자체를 블로킹한다. VM 비트, Hyper-V 제어 레지스터 검사하여 못하게 막는다. 혹은 TPM-based Measurement로 VM 무결성을 검증하거나, 서명되지 않은 Hyper-V 루트파티션을 거부한다. 아니면 EPT/PT 리맵 호출 빈도, LibVMI I/O 패턴을 커널 드라이버에서 카운트하여 임계값 초과시 세션을 차단한다. 또, 호스트 AWS, Azure 네트워크 대역에서 생성된 의심 다중 세션을 자동 추적해 정지시킨다.


### > 핵심 참고 자료

VIC 논문 - VMI 기반 월핵과 트리거봇을 구현해 탐지 방지 성능을 측정한다.
https://arxiv.org/pdf/2502.12322


### > 연구용 최소 재현 환경

VMI 치트
Host OS : ubuntu 24.04 (5.15+커널,KVM)
하이퍼바이저 : qemu-kvm + intel VT-d/AMD-V (IOMMU on)
GPU 패스스루 : NVIDIA RTX 20xx 이상 또는 AMD RDNA2 이상
VMI 라이브러리 : LibVMI 0.14 + PyVMI, altp2m(Xen) 옵션 필요시 Xen 4.18
HID 브리지 : KMBox(USB)나 Zen류나 uinput+virtio-input
작동 방식 : 폴링 read_pa() 반복은 구현이 간단하고 프레임 영향이 적다. page-guard는 트리거봇에는 이상적이지만 VM 프리즈 위험이 있다.
ESP 표시 : host virtio-gpu 2nd head + PyOpenGL 창
Trigger : host uinput 디바이스에 BTN_LEFT down/up 쓰기
지연 측정 : Guest PresentMon FPS 로그 + Host time.time() 비교

안티치트
가상화 탐지 : CPUID bit, TSC 슬립, ACPI DMAR 테이블 변경 탐지
EPT 호출 패턴 : KVM trace로 EPT violation 빈도 측정하여 ML로 분류
GPU 패스스루 : PCI config vendor != 0x10DE & passthru flag
LibVMI 호출 : 자체 API, LibVMI symbol 제거 후 비교

### > 설치 모듈

LibVMI - VMI의 표준 C 라이브러리.
언어는 C로, Xen, KVM/QEMU, VMwart, VirtualBox를 지원한다.
VMI에서 가장 널리 쓰이는 중간 계층 AIP이다.

PyVMI - LibVMI의 파이썬 바인딩
CFFI로 LibVMI API를 그대로 노출한 thin wrapper. 파이썬 바인딩이란 c라이브러리를 python 객체와 함수로 감깐 모듈이다.

r2vmi - vmi 디버거 플러그인
하이퍼바이저 레벨 디버깅 도구. radare2 콘솔에서 r2 vmi://win10:PID 식으로 vm 내부 프로세스를 분석 가능.

altp2m - alternate second-level page tables, 즉 다중 EPT.
Xen에서 사용하는 기술

### > 메모리 위치 찾기

치트 개발자는 먼저 치트에 필요한 구조체 및 정보가 메모리 어디에 저장되어 있는지를 식별해야 한다. 이러한 오프셋을 찾기 위한 최초 분석 단계는 다음과 같은 도구를 통해 수행된다.

디스어셈블러 : IDA, Ghidra
메모리 스캐너 : Cheat Engine
덤퍼
게임 해킹 포럼 : MPGH, UnknwonCheats

치트 개발 도구 - CheatEngine과 ReClass
CheatEngine은 최근 DBVM이라는 Type-1 하이퍼바이저를 도입하였다. 부팅 시 os와 함께 USB로 로드해야한다. 하지만 이는 치트와 게임이 같은 os에서 작동해야 함을 의미한다. Type-2 하이퍼바이저가 훨씬 좋은 구조이다.

### > 위치 찾으면

step 1. 메모리 읽기
LibVMI는 게스트 VM의 페이지 테이블(SLAT:Second level address table)에 접근한다. 이는 EPT (Extended page table) 또는 NPT (Nested Page Table)을 통해 구현된다. 따라가서 호스트 물리 주소를 찾아내고, 그 페이지를 읽어온다.
step 2. 메모리 쓰기
읽기와 마찬가지로 위치를 찾아 갚을 덮어쓴다
step 3. 페이지 가드 예외
특정 메모리 페이지의 gfn(guest frame number) 비트를 바꿔 페이지 폴트를 유도하고, 콜백을 등록한다. 값이 바뀔때만 콜백이 불려 효율적이며, 트리거봇처럼 이벤트 기반 치트에 최적.
step 4. 입력 에뮬레이션
qemu machine protocol은 소켓 기반 json rpc. 호스트가 가짜 마우스 이동과 클릭을 보내면 게스트는 진짜 HID 이벤트로 인식해 안티치트가 구분 불가.
step 5. 오버레이
DirectX/OpenGL 훅을 쓸 필요도 없이 호스트 OS에서 투명 창을 올려놓는다.

### > 치트 스레드 구현

메인 스레드 : 게임에서 정보를 읽어온다
UI 스레드 : 화면에 필요한 정보를 렌더링한다.

메인스레드는 두 가지 버전으로 나뉜다. 일정 주기로 메모리를 읽는 방식과, 페이지 가드 예외를 활용하여 이벤트 기반으로 콜백을 실행하는 방식이다.
대부분의 치트 구현에서 전자를 사용하고, 포트리스2의 트리거봇에서만 후자를 추가로 제공하였다.

알고리즘 1 - 폴링
```
gameValues ← Array[PlayerObject, PlayerObject, ...]
uiThread ← Thread(gameValues)             ▶ 배열 참조를 UI 스레드에 전달
uiThread.start()                          ▶ UI 스레드 시작

while gameIsRunning do
    UpdateGameValues(gameValues)         ▶ 메모리 값 업데이트
    Sleep(TimeInMilliseconds)            ▶ 일정 시간 대기
end while
```

알고리즘 2 - 페이지 가드
```
procedure CALLBACKA(newAddressValue)
    JUMP()
end procedure

procedure CALLBACKB(newAddressValue)
    STARTSHOOTING()
end procedure

procedure MAIN
    addressList ← Array[int, int]
    registerException(addressList[0], CallbackA)
    registerException(addressList[1], CallbackB)
    ListenForExceptionEvents()
end procedure
```

### > 월핵

(1) 게임 메모리의 특정 오프셋을 읽어서 게임 세계 내의 플레이어 3D 좌표를 알아내기.
(2) world space로 각 항목들을 배치하기.
(3) 4x4 View matrix를 통해 view space로 변환.
(4) Projection matrix를 통해 2D 스크린 좌표계로 변환하기.

이걸 World-to-Screen 변환이라고 부른다.

Or_X − Axis.x  Or_Y − Axis.x  Or_Z − Axis.x  T.x
Or_X − Axis.y  Or_Y − Axis.y  Or_Z − Axis.y  T.y
Or_X − Axis.z  Or_Y − Axis.z  Or_Z − Axis.z  T.z
0               0                 0           1
새로운 공간에서의 x축 방향 y축 방향     z축 방향   객체의 위치

### > 트리거봇

(1) 로컬 플레이어가 적 아바타를 조준하고 있는지 식별
(2) 언제 사격을 시작하거나 멈춰야 하는지를 로컬 플레이어에게 전달

식별은 둘 중 하나의 방식을 사용한다. 게임 메모리에서 조준선의 결과를 저장하는 주소를 찾아 읽기, 또는 적 아바타의 스크린 위치와 조준선의 위치를 비교하기.

사격 결정은 둘 중 하나의 방식을 사용한다. 게임 프로세스를 스캔하여 사격 상태를 제어하는 메모리 주소를 찾아 원할 때 write, 또는 QMP 같은 원격 인터페이스를 사용해 입력 장치를 에뮬레이션 전달.