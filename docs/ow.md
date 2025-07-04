오버워치는 엔진, 파이프라인, 툴체인으로 이루어져 있다.


### >설치 파이프라인

파이프라인은 소스 asset에서 실행가능 asset으로 변환하거나, 캐싱, 분산 빌드, 패키징 등을 한다.

```
[Source Assets] ─▶ pull --sourcepull ─▶ SQLite ‘local asset DB’
                 ▲                             │
                 │ (Structured-Data Generator) │
                 │                             ▼
[Builder per Type] ◀─ DataBuild --compile ──▶ [Compiled Asset Store]
```

genrator: json 유사 선언(SDLang)으로 타입-안전 C++/C# 클래스, 에디터 리플렉션 코드 자동생성. 산출물 위치는 Generated/<TypeName>.h/.cpp로 추정.

builer: asset 타입별(메시, 머티리얼, 영웅 스크립트) DLL. 입력 SHA-1 해시와 CompilerID 조합으로 결과를 캐싱해, 이미 컴파일된 동일 입력은 전역 Compiled Asset Repository에서 그냥 당겨온다. 분산 빌드는 내부 빌드팜 30노드 기준 12h -> 4h 이다. 완성 자산을 기능 단위 Package로 묶고, 지역, 플랫폼별 암호화 규칙을 붙여 런처가 온-디맨드 스트리밍을 한다. 산출물 위치는 Compiled/<hash>/<guid>.bin로 추정.

```
1) Battle.net → /versions  → buildConfig, cdnConfig, keyRing
2) buildConfig → root, encoding, install, download 해시 목록 확인
3) cdnConfig  → 사용 가능한 CDN 도메인·아카이브 분할 규칙 확인
4) 필요한 .idx / .data 조각 GET
5) 인덱스 갱신 → 게임 실행 → shmem 업데이트
```
TACT 프로토콜이 ①~③을 담당하고, 실제 .data 조각 전송은 HTTPS GET으로 이뤄집니다.

```
Overwatch/
├─ Engine/                 # 런타임 코어
│   ├─ Core/               # 메모리, Job, Math, ECS Core
│   ├─ Graphics/           # DX12/Vulkan 렌더러
│   ├─ Physics/
│   ├─ Audio/
│   └─ Platform/
├─ Game/                   # ECS 시스템·컴포넌트·스킴
│   ├─ Heroes/
│   │   └─ Tracer/
│   ├─ Abilities/
│   └─ PvE/
├─ Network/                # 프레임 스냅샷, 롤백, 예측
├─ Tools/
│   ├─ TED/                # 월드·스크립트 에디터 GUI
│   ├─ DataBuild/          # 메인 파이프라인 실행파일
│   │   └─ Builders/       # *.Builder.dll per asset-type
│   ├─ Generators/         # Structured-Data codegen
│   ├─ BranchManager/
│   └─ PackageManager/
├─ CI/
│   └─ DataBuildCI/
├─ Content/                # ‘소스’ 자산(PSD, FBX, WAV…)
└─ BuildScripts/           # CMake/SCons + 퍼블리셔 스크립트
```

실제로 우리가 내려받는 파일은 실행 파일과 라이브러리,  CASC 데이터 아카이브, 인덱스, 런처 메타데이터가 다이다.

실행 파일: C/C++로 컴파일된 클라이언트, 런처 코드. Overwatch/Overwatch.exe
라이브러리: *.dll
CASC: Content Addressable Storage Container로, 블리자드가 MPQ를 대체해 사용하는 패키지 포맷으로, 파일 덩어리를 '콘텐츠 해시' 기준으로 저장, 스트리밍한다. 영웅 모델, 사운드, 맵 등 asset이 암호화된 채로 저장되어 있다. 설치기(Battle.net) <-> 게임 실행 시 동일 구조를 공유해 증분 패치가 빠르다. Overwatch/data/casc/data/…
인덱스: CASC 블록-해시와 실제 위치를 매핑한다. …/casc/indices/*.idx
런처 메타데이터: 버전, CDN 위치, 해시 목록 (TACT 프로토콜). *.build.info, *.product.db

설치나 업데이트 흐름은 다음과 같다.
i. Battle.net 앱에서 '설치' 버튼을 누르면 TACT 프로토콜로 버전, 해시 목록을 받아옴
ii. 클라이언트는 CDN에서 컴파일된 실행 파일과 CASC 조각을 내려받음
iii. 받은 조각은 Overwatch/data/casc 아래에 저장되고, 인덱스, 저널(*.idx,shmem) 파일에 위치가 기록됨
iv. 실행 시 런처가 무결성 검사(해시->블록 확인) 후 실행.


그리고 이조차 오버워치 1과 2에 차이를 보인다.
| 항목                         | Overwatch (2016)                                  | Overwatch 2 (2022\~)                                                                                                                         |
| -------------------------- | ------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| **엔진 코어**                  | Team 4 독자 ECS 엔진 v1.0                             | **v1.5 – 동일 코드베이스를 확장**<br>· 멀티스레드 렌더 파이프라인<br>· 물리·파티클 최대치 상향<br>· **동적 조명·PBR 재질** 추가 ([en.wikipedia.org][1], [overwatch.blizzard.com][2]) |
| **맵/자산 규모**                | 평균 100 × 100 m, 6v6 전제                            | **PvE용 3\~5배 대형 맵**, 스크립트형 이벤트, 동적 기상 효과                                                                                                     |
| **DataBuild 파이프라인**        | 203 타입, 분산 빌드 30 노드/4 h ([media.gdcvault.com][3]) | **≈250 타입**(+Perk·Cinematic 등), 일부 GPU 전용 빌더-DLL 신설·CUDA 활용                                                                                  |
| **Generator(SDLang → 코드)** | 1,700여 구조체                                        | **Hero Perks/Progression 등 신규 스키마** 자동생성                                                                                                     |
| **패키징·배포**                 | CASC v2 + TACT, SHA-1 암호화                         | **CASC v3** (AES-CTR 128 bit) · 증분 스트리밍 최적화 ([us.forums.blizzard.com][4])                                                                    |
| **클라이언트 규모(PC)**           | 설치 25 \~ 30 GB                                    | 시즌 15 기준 **≈52 GB**, 패치 시 증분 다운로드                                                                                                            |

### > 엔진

엔진은 게임이 실시간으로 진행되는 코드이다. ECS(Entitiy-Component-System) 기반 시뮬레이션 루프, 렌더러, 물리, 오디오, 플랫폼 래퍼 등으로 구성되어 있다.

이때 엔진의 핵심 특징은 ECS + Network-deterministic 시뮬레이션이라는 것이다. 모든 클라이언트, 서버가 틱 단위로 같은 계산을 재현하도록 설계되어 입력만 동기화하면 결과가 일치한다.


ECS? Entity/Component/System 세가지 축.
Entity: 고유 ID, 생명주기, 폭발 구역, 영웅 한명, 투사체 등 데이터나 논리가 없는 것들. 메모리에서 32-bit 핸들(인덱스+세대) 배열이다.
Component: 순수 데이터(POD)로 위치(transform), HP(health), 총알수(weaponstate). 메모리에서 SoA(Structure of Arrays)로 캐시 친화이다.
System: 매 틱마다 필요 Component 셋을 가진 Entity 스트림을 스캔해 로직을 실행한다. MovementSystem(transform+Physics), WeaponSystem(weaponstate+input) 등이다. 메모리에서 다중 스레드/Job 큐로 병렬 처리한다.

전통석인 상속 계층(OOP)과는 확실히 다른 분리로 코드 복잡도가 낮다.

네트워크 결정론? 모든 참가자가 같은 초기 상태에서 같은 시점별 입력 시퀀스만 공유하면, 각자의 PC에서 돌린 결과가 바이트 단위까지 동일하게 재현된다.

즉 클라이언트는 매 틱(63hz 즉 15.9ms) 입력값 수바이트를 꾸준히 보낸다. 63 Hz 'high-bandwidth' 서버가 이를 받는다. 서버는 모두의 Command[N]을 모아 권위 시뮬레이션을 돌린다. 그 결과를 delta 스냅샷으로 각 클라이언트에 배포한다. 클라이언트는 자신이 미리 돌린 로컬 예측과 비교해 필요하면 미세 롤백/보정만 수행한다.

따라서 자신의 움직임은 입력 시점에 돌아가고 다른 플레이어는 살짝 과거(약 50ms)를 재생하고 있는 것이다. 서버의 스냅샷을 기준으로 말하면 내 캐릭은 앞으로 1/2 RTT정도 앞서 달리고, 상대 캐릭터는 과거 1/2 RTT정도를 재생하는 것이다.

참고/ 결정론을 유지하기 위해 고정 틱 레이트, 부동소수점과 연산 순서 통제, 무작위 요소는 전역 시드 PRNG 사용, 컴포넌트 순서 고정, command에 Tick ID 포함.


클라이언트->서버 command 패킷 형식

| 필드             | 압축·비트수    | 설명                                      |
| -------------- | --------- | --------------------------------------- |
| `tickId`       | 16 bit    | 이 명령이 적용될 ECS 틱(63 Hz면 0.0159 s)        |
| `buttons`      | 16 bit    | LMB / Shift / Ability 1·2·Ult 등 ― 비트플래그 |
| `moveX, moveY` | 각각 8 bit  | WASD 축, -127\~127 정규화                   |
| `yaw, pitch`   | 각각 13 bit | 0-360°, 0-±89° 양자화                      |
| `heroPayload`  | 가변        | (예) 메이 얼음벽 높이, 자리야 충전량…                 |
| **중복 명령**      | Δ-압축      | 직전 3 틱 입력을 diff 형태로 추가해 패킷 손실 대비        |

실제 기준 11-15B 정도로 매우 작음
헤더에는 seq/ack 번호와 서버 스냅샷 ack(내가 N번째 결과까지 받았다)가 함께 들어 있다.

물리적 전송 계층 
프로토콜은 battle.net 실시간 스트림으로 UDP+자체 재전송/암호화 레이어.
주 사용 포트는 UDP 5060,5062,6250,3478,3479,12000~64000 과 TCP 1119,3724,6113.
보안은 세션 핸드셰이크 뒤 AES-CTR 키 링으로 외부 추출 차단.

서버 측 처리 흐름.
for every tick N:
    cmds = gatherCommandsFromClients(N)      // 없으면 N-1 입력 재사용
    runAllECSSystems(cmds)                   // 결정론 시뮬레이션
    delta = makeSnapshotDelta(prev, current)
    sendSnapshotToEachClient(delta, ack=N)   // 완성된 스냅샷을 UDP로 전송

서버->클라이언트 스냅샷 형식

100 ~ 200 바이트. 바이트 스트림 전체가 AES-CTR로 암호화돼 있다.
A층 헤더. 6-8B

| 필드            | 비트수 | 설명                              |
| ------------- | --- | ------------------------------- |
| `snapshotSeq` | 16  | 이번 스냅샷의 연속 번호                   |
| `baselineSeq` | 16  | “이 번호까지는 클라가 받았다고 확정”— 델타의 기준 틱 |
| `ackBits`     | 32  | 최근 32개 스냅샷 수신 확인 비트-필드          |
| `flags`       | 8   | 압축(LZ4), 전체스냅샷 여부 등             |

B층 entity 테이블. 5-40B
Create/Update/Delete 비트 마스크 + 엔티티 ID 가변길이 VLQ 
ID는 EntityIndex << 6 | GenerationBits(6) 형태를 7bit VLQ로 압축.

C층 Component Δ블록(entity별로 1~100B).

| 순서             | 내용                   | 인코딩 방식·비트수 예                                                                           |
| -------------- | -------------------- | -------------------------------------------------------------------------------------- |
| ① `updateMask` | 이 엔티티에서 바뀐 컴포넌트 비트필드 | 최대 16 비트                                                                               |
| ② 컴포넌트 인덱스     | 바뀐 것만 순차 기록          | 가변(Δ-VLQ)                                                                              |
| ③ **필드별 Δ**    | 값마다 개별 양자화           | 예) **위치** X/Y/Z → 16·16·14 bit<br>**Yaw/Pitch** → 13·9 bit<br>**HP** → 10 bit (0-1023) |

즉 한번에 표현하자면
[UDP]  <AES-CTR>  |Header|E-Tbl|ΔComp(Entity #17)|ΔComp(Entity #42)|…
                ▲ 압축(LZ4) ▲

클라이언트 수신->롤백/재적용
로컬 예측 결과와 비교해 차이가 나면 롤백.


### > API
api? 소프트웨어끼리 약속한 대화 규칙이다. 어떤 프로그램이 다른 기능을 호출하거나 제어할 수 있도록 이름, 입력, 반환, 동작이 문서화된 인터페이스.

오버워치에서 사용하는 api?

| 구역                                                           | 무엇을 캡처할 수 있나                                              | 암/복호화 가능성                                                                                   | 추천 도구·방법                                                                                                                                                                                           |                                                   |
| ------------------------------------------------------------ | --------------------------------------------------------- | ------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------- |
| ① **Battle.net HTTP API**<br>(로그인·OAuth·매치메이커·패치 메타)         | HTTPS - TLS 1.3                                           | TLS―브라우저와 동일.<br>클라에 **Pinning 없음** → `SSLKEYLOGFILE` + Wireshark로 복호화 가능 (윈도우 Schannel 기반) | <br>1. 환경변수 `SSLKEYLOGFILE=%USERPROFILE%\tls.keys` 설정<br>2. Wireshark ▸ TLS ▸ (Pre-)Master-Secret log 경로 지정<br>3. 필터 `tcp.port==443 && tls.handshake.extensions_server_name contains "battle.net"` | ([develop.battle.net][1])                         |
| ② **TACT/CDN 다운로드**<br>(CASC \*.data, \*.idx, root/encoding) | 순수 HTTP GET                                               | **평문**<br>(블록 자체는 BLTE 압축·AES 가능)                                                           | <br>– Wireshark `http.host contains "level3.blizzard.com"`<br>– `tactmon`, `curl -O` 로 재현<br>– 이후 로컬 파일을 **CascView / TACTLib**로 분석                                                                | ([wowdev.wiki][2])                                |
| ③ **게임플레이 UDP 스트림**<br>(Command ↔ Snapshot)                  | 전용 UDP (기본 63 Hz)<br>포트 **5060, 5062, 6250, 12000-64000** | **AES-CTR 전구간 암호화**<br>키는 런타임 메모리 안(OW2 = Key-Ring) → 외부에서 복호화 불가                           | <br>– Wireshark `udp.port >= 12000 && udp.port <= 64000`<br>– “Follow → UDP Stream”으로 **패킷 길이·RTT·손실** 통계만 확인<br>– Payload 해석은 사실상 불가능 (키 추출·메모리 후킹 필요)                                            | ([us.forums.blizzard.com][3], [blog.csdn.net][4]) |
| ④ **클라이언트 로그·로컬 상태**                                         | `Overwatch/Logs/*.log` & CASC `data/shmem`, `.idx`        | 평문                                                                                          | 텍스트 뷰어·010 Editor                                                                                                                                                                                  | 

```
1) TLS 키 로깅
setx SSLKEYLOGFILE "%USERPROFILE%\tls.keys"

2) 관리자 권한으로 Wireshark 실행
    - Capture filter: host 137.*.***.*** or udp
    - TLS ‣ (Pre-)Master-Secret log: %USERPROFILE%\tls.keys

 3) Overwatch2 실행 → 로그인 → 한 판 플레이

 4) Wireshark
    a) HTTP 스트림: tcp.port==443 && tls.app_data
       ­→ Follow TCP Stream → JSON Body 확인
    b) UDP 스트림: udp.port>=12000 && udp.port<=64000
      ­→ Statistics ▸ I/O Graph 로 틱 주기·손실률 확인
```

스냅샷 패킷 내부를 까볼 수 있나?

    OW1 – 일부 키가 공개돼 있어 CascView + OverTool 로 제한적 디코딩 가능.

    OW2 (CASC v3 + Key-Ring) – AES-CTR 키가 서버에서 동적 교환 → 외부 추출 난도 ↑.
    따라서 Wireshark로는 헤더 길이·Sequence/ACK 필드만 보이고,
    엔티티 Table·컴포넌트 Δ-블록은 전부 암호화된 바이트 스트림입니다.


### > 메모리핵 방지

스냅샷이 AES-CTR로 암호화된다해도 네트워크 전송 중의 변조를 막아줄 뿐, 결국 클라이언트 메모리 안에서는 다시 평문이 된다. 따라서 그걸 읽어와 상대 데이터를 파악하고 그걸 바탕으로 본인 전송 command 패킷을 조작해서 보내면 된다.

오버워치는 이를 막기 위해 다음과 같은 조치를 취한다

i. 서버가 각 클라이언트 FOV로 레이캐스팅해 보이는 오브젝트만 스냅샷에 포함해서 보낸다. 벽 뒤 자표, 히트박스, 쿨타임 등은 애초에 내려오지 않는다.
ii. 또한 클라이언트 측에도 안티치트를 넣어놓는다. warden 안티치트는 wow 시절부터 사용된 블리자드 내부 엔진으로, 서명 기반 프로세스 메모리 스캔 방식이다. Eidolon은 최근 패치에 일시 도입하여 조정 중으로, 코드 흐름 난독화 및 안티탐퍼 방식이다. 커널 레벨 드라이버까지는 도입하지 않았지만 battle.net 런처 권한으로 HW 브레이크, DMA 접근을 감시한다. 정적 해시뿐 아니라 런타임 무결성 검사, heap Re-randomization을 주기적으로 돌려 패턴 치트를 어렵게 만든다.
iii. 서버 측 Defense matrix 백엔드를 통해 시즌 12 전후로 AI 분석 + 행동 통계 파이프라인을 강화해 50만 계정 일괄 밴, 치터와 파티한 동료 4만 계정도 추가 제재 하였다. 콘솔의 XIM-류 마우스 변환기도 실시간 탐지한다.
iv. TLS 터널 내부에서도 시퀀스 넘버 + AES-CTR + HMAC을 써서 중간 삽입, 리플레이를 봉쇄한다.  (Statescript 세션 ‘security’ 항목) .
v. 서버는 물리 한계 검증을 매 틱마다 수행해 수치가 비정상이면 바로 취소한다.

### > 클라우드 스트리밍 서비스

DTLS-UDP 채널에 컨트롤 패킷만 실려 간다. SRTP 채널은 반대로 서버가 인코딩한 영상, 오디오 스트림을 내려보낸다.


### > 전체 메모리 보기

step 1. 실시간 사용량
```
~$ top
top - 16:45:55 up 38 min,  1 user,  load average: 0.89, 1.03, 0.93
Tasks: 284 total,   1 running, 283 sleeping,   0 stopped,   0 zombie
%Cpu(s):  3.5 us,  1.0 sy,  0.0 ni, 95.5 id,  0.0 wa,  0.0 hi,  0.0 si,  0.0 st
MiB Mem :   7742.6 total,   1928.0 free,   2904.1 used,   2910.5 buff/cache
MiB Swap:   2048.0 total,   2048.0 free,      0.0 used.   4062.3 avail Mem 

    PID USER      PR  NI    VIRT    RES    SHR S  %CPU  %MEM     TIME+ COMMAND  
   3547 kkongny+  20   0 3008464 477472 118420 S  19.3   6.0   4:23.30 Isolate+ 
   1169 kkongny+  20   0 5301912 289408 135616 S   5.6   3.7   3:01.53 gnome-s+ 
   3448 kkongny+  20   0  367208  63532  51328 S   2.3   0.8   0:58.19 Utility+ 
   2589 kkongny+  20   0   11.8g 521776 242904 S   2.0   6.6   6:08.27 firefox  
   5196 kkongny+  20   0  594156  56948  43620 S   1.7   0.7   0:00.42 gnome-t+ 
    963 kkongny+   9 -11 1792844  27632  22000 S   1.0   0.3   0:19.37 pulseau+ 
    586 root      20   0    2816   1792   1792 S   0.7   0.0   0:01.57 acpid    
    427 root     -51   0       0      0      0 S   0.3   0.0   0:00.11 irq/173+ 
    517 systemd+  20   0   14836   6784   6016 S   0.3   0.1   0:02.96 systemd+ 
   4743 kkongny+  20   0 1161.6g 182960 128680 S   0.3   2.3   0:12.15 code     
   4900 kkongny+  20   0 1161.5g 183948  79488 S   0.3   2.3   0:40.57 code     
   5231 kkongny+  20   0   14532   4480   3584 R   0.3   0.1   0:00.04 top      
      1 root      20   0  166960  11584   8128 S   0.0   0.1   0:01.10 systemd  
      2 root      20   0       0      0      0 S   0.0   0.0   0:00.00 kthreadd 
      3 root      20   0       0      0      0 S   0.0   0.0   0:00.00 pool_wo+ 
      4 root       0 -20       0      0      0 I   0.0   0.0   0:00.00 kworker+ 
      5 root       0 -20       0      0      0 I   0.0   0.0   0:00.00 kworker+ 
```
step 2. 아예 프로그램처럼 보여주기
```
~$ htop
~$ F2로 숨길항목 조정
~$ F6 정렬을 MEM% 로
~$ F4 필터로 /usr/bin/qemu 특정 프로세스 계열만 확인 가능
~$ q로 끄기
```
step 3. 스냅샷/스크립트
```
~$ ps -eo pid,ppid,user,comm,rss,vsz --sort=-rss | head     #rss(실메모리) vsz(가상)
    PID    PPID USER     COMMAND           RSS    VSZ
   2589    1169 kkongny+ firefox         594364 12629332
   4884    4749 kkongny+ code            563836 1219649316
   3547    2589 kkongny+ Isolated Web Co 550156 3038512
   3455    2589 kkongny+ Isolated Web Co 347484 2922224
   1169     950 kkongny+ gnome-shell     297092 5567980
   1539     950 kkongny+ snap-store      209600 1045388
   4900    4743 kkongny+ code            187248 1217878856
   3946    2589 kkongny+ Isolated Web Co 187084 2635356
   4743     950 kkongny+ code            183576 1218076356
```
step 4. 전공자용 정확한 합계
```
~$ smem -r
```

### > 프로세스 레벨로 메모리 확인

step 1. pmap -x 2589
```
~$ pmap -x 2589
2589:   /snap/firefox/6436/usr/lib/firefox/firefox
Address           Kbytes     RSS   Dirty Mode  Mapping
000001fa00500000    1024     684     684 rw---   [ anon ]
000002d1ed600000    1024     292     292 rw---   [ anon ]
000002f3c6200000    1024     144     144 rw---   [ anon ]
0000050269000000    1024    1024    1024 rw---   [ anon ]
0000050c95900000    1024      64      64 rw---   [ anon ]
0000056dbae00000    1024    1024    1024 rw---   [ anon ]
00000693c5200000    1024    1024    1024 rw---   [ anon ]
000006b766100000    1024     236     236 rw---   [ anon ]
```


### > 가능한 시나리오

메모리 단
A. 프로세스 메모리 인젝션


입력 단
B. 컴퓨터 비전 기반 aimbot
캡쳐카드 + YOLOv8 -> Δ마우스 주입

C. 에뮬레이트
Cronus Zen, XIM Apex -> 패드 VID/PID 위장

네트워크 단
시퀀스 넘버 + AES-CTR + HMAC 로 방지함.