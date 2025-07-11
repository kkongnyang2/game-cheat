## 오버워치 구조를 알아보자

### 목표: 게임 구조 탐색
작성자: kkongnyang2 작성일: 2025-07-06

---

오버워치는 엔진, 설치, 툴체인으로 이루어져 있다.


### > 설치 파이프라인

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
```
i. Battle.net 앱에서 '설치' 버튼을 누르면 TACT 프로토콜로 버전, 해시 목록을 받아옴
ii. 클라이언트는 CDN에서 컴파일된 실행 파일과 CASC 조각을 내려받음
iii. 받은 조각은 Overwatch/data/casc 아래에 저장되고, 인덱스, 저널(*.idx,shmem) 파일에 위치가 기록됨
iv. 실행 시 런처가 무결성 검사(해시->블록 확인) 후 실행.
```

그리고 이조차 오버워치 1과 2에 차이를 보인다.

| 항목                         | Overwatch (2016)                                  | Overwatch 2 (2022\~)                                                                                                                         |
| -------------------------- | ------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------- |
| **엔진 코어**                  | Team 4 독자 ECS 엔진 v1.0                             | **v1.5 – 동일 코드베이스를 확장**<br>· 멀티스레드 렌더 파이프라인<br>· 물리·파티클 최대치 상향<br>· **동적 조명·PBR 재질** 추가 |
| **맵/자산 규모**                | 평균 100 × 100 m, 6v6 전제                            | **PvE용 3\~5배 대형 맵**, 스크립트형 이벤트, 동적 기상 효과                                                                                                     |
| **DataBuild 파이프라인**        | 203 타입, 분산 빌드 30 노드/4 h  | **≈250 타입**(+Perk·Cinematic 등), 일부 GPU 전용 빌더-DLL 신설·CUDA 활용                                                                                  |
| **Generator(SDLang → 코드)** | 1,700여 구조체                                        | **Hero Perks/Progression 등 신규 스키마** 자동생성                                                                                                     |
| **패키징·배포**                 | CASC v2 + TACT, SHA-1 암호화                         | **CASC v3** (AES-CTR 128 bit) · 증분 스트리밍 최적화                                                                     |
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
```
for every tick N:
    cmds = gatherCommandsFromClients(N)      // 없으면 N-1 입력 재사용
    runAllECSSystems(cmds)                   // 결정론 시뮬레이션
    delta = makeSnapshotDelta(prev, current)
    sendSnapshotToEachClient(delta, ack=N)   // 완성된 스냅샷을 UDP로 전송
```

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
| ① **Battle.net HTTP API**<br>(로그인·OAuth·매치메이커·패치 메타)         | HTTPS - TLS 1.3                                           | TLS―브라우저와 동일.<br>클라에 **Pinning 없음** → `SSLKEYLOGFILE` + Wireshark로 복호화 가능 (윈도우 Schannel 기반) | <br>1. 환경변수 `SSLKEYLOGFILE=%USERPROFILE%\tls.keys` 설정<br>2. Wireshark ▸ TLS ▸ (Pre-)Master-Secret log 경로 지정<br>3. 필터 `tcp.port==443 && tls.handshake.extensions_server_name contains "battle.net"` |
| ② **TACT/CDN 다운로드**<br>(CASC \*.data, \*.idx, root/encoding) | 순수 HTTP GET                                               | **평문**<br>(블록 자체는 BLTE 압축·AES 가능)                                                           | <br>– Wireshark `http.host contains "level3.blizzard.com"`<br>– `tactmon`, `curl -O` 로 재현<br>– 이후 로컬 파일을 **CascView / TACTLib**로 분석                                                                |
| ③ **게임플레이 UDP 스트림**<br>(Command ↔ Snapshot)                  | 전용 UDP (기본 63 Hz)<br>포트 **5060, 5062, 6250, 12000-64000** | **AES-CTR 전구간 암호화**<br>키는 런타임 메모리 안(OW2 = Key-Ring) → 외부에서 복호화 불가                           | <br>– Wireshark `udp.port >= 12000 && udp.port <= 64000`<br>– “Follow → UDP Stream”으로 **패킷 길이·RTT·손실** 통계만 확인<br>– Payload 해석은 사실상 불가능 (키 추출·메모리 후킹 필요)                                            |
| ④ **클라이언트 로그·로컬 상태**                                         | `Overwatch/Logs/*.log` & CASC `data/shmem`, `.idx`        | 평문                                                                                          | 텍스트 뷰어·010 Editor                                                                                                                                                                                  | 

```
1) TLS 키 로깅
setx SSLKEYLOGFILE "%USERPROFILE%\tls.keys"

1) 관리자 권한으로 Wireshark 실행
    - Capture filter: host 137.*.***.*** or udp
    - TLS ‣ (Pre-)Master-Secret log: %USERPROFILE%\tls.keys

 2) Overwatch2 실행 → 로그인 → 한 판 플레이

 3) Wireshark
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


### > 클라우드 스트리밍 서비스

DTLS-UDP 채널에 컨트롤 패킷만 실려 간다. SRTP 채널은 반대로 서버가 인코딩한 영상, 오디오 스트림을 내려보낸다.