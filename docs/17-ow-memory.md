## 오버워치 메모리를 탐색하자

작성자: kkongnyang2 작성일: 2025-08-30

---

### 게임에 자주 쓰이는 모델

아키타입 청크 모델

Entity = (index, genenration)로 구성된 식별자
죽고 리스폰되거나 픽을 변경하면 새 entity가 생기는 것이다.

slot[index] 테이블 = { 아키타입, 청크, row, 맞는 generation }

Component = 청크 블록에 특정 아키타입의 컴포넌트들을 모아놓는다.
PosX: [  5,  7, 12, -3, ...]
PosY: [  1,  0,  5,  2, ...]


매치 준비
풀/아키타입 청크 준비. 내부 배열 보존으로 재할당 방지.
서버 엔티티 ID와 클라이언트 Entity 핸들 매핑 테이블 보관

유저2 입장
Entity e2 = createEntity();         // {index=42, gen=5}
addComponent<name>(e2, "kkongnyang2");
addComponent<Transform>(e2, {x:0,y:0,z:0, ...});
// slots[e2.index].denseIndex = k; posX[k], posY[k], posZ[k], hp[k] 등 동일 k 위치에 각 컴포넌트가 정렬.

틱 업데이트
InputSystem: velX[k], velY[k] 수정
MovementSystem: posX[k] += velX[k]*dt
Combat/HealthSystem: 히트 확인 시 hp[k] -= dmg
RenderPrepSystem: 현재 pos를 상수 버퍼용 구조로 패킹하여 GPU 업로드(프레임 링비퍼라 주소 고정 기대x)

유저2가 사망
destroyEntity(e2);
// slots[e2.index].gen++  (예: 5 → 6)
// 따라서 {index, gen=5}는 매치가 안되니 즉시 무효

유저2 리스폰
Entity e2b = createEntity();    // {index=42, gen=6}  (같은 index라도 gen이 다름)
addComponent<Transform>(e2b, spawnPoint);


### 오버워치2 메모리 구조(언노운)

┌──────────────────────────────────────────────────────────────────────┐
│ Windows Process (게임 클라이언트)                                       │
│  PEB → ImageBase = [GS:0x60]+0x10  (memory.h)                        │
│                                                                      │
│  Globals (ImageBase+RVA)                                             │
│  ├─ ViewMatrixPtr_RVA          = 0x4149F68  (offset::matrix)         │
│  │    └─ ViewMatrix = *[ImageBase+0x4149F68] + 0x7E0                 │
│  │                        (offset::matrix_sub)                       │
│  ├─ RayTrace.force_fn_RVA      = 0x643270   (offset::ray_force)      │
│  ├─ RayTrace.cast_fn_RVA       = 0xF01140   (offset::ray_cast)       │
│  └─ DecryptKey_RVA             = 0x00B3D20  (offset::decrypt_key)    │
└──────────────────────────────────────────────────────────────────────┘


┌──────────────────────────────────────────────────────────────┐
│ GameManager (동적 주소, decrypt::GameManager()로 해제)          │
│  +0x020 entity_rawlist         (offset::entity_rawlist)      │
│  +0x02C entity_rawlist_count   (offset::entity_rawlist_count)│
│  +0x2E8 local_unique_id        (offset::local_unique_id)     │
└──────────────────────────────────────────────────────────────┘
* 모든 정보 집합 gamemanager

entity_rawlist 로부터 연결 리스트를 순회하며 ParentPool 배열을 모음:
   do {
       rawListArr = *(entity_rawlist + 0x08);         // 배열 시작 (function.cpp)
       for x in [0..entity_rawlist_count-1]:
           ParentPool[x] 유효 시 수집
   } while ( entity_rawlist = *(entity_rawlist + 0x18) )  // 다음 노드(next_rawlist)

┌───────────────────────────────────────────────┐
│ RawList Node                                  │
│  +0x08  ParentPool* array head                │
│  +0x18  next_rawlist (offset::next_rawlist)   │
│  ...                                          │
└───────────────────────────────────────────────┘
* 한 노드마다 parentpool 포인터들이 적당히 담겨 있음

┌───────────────────────────────────────────────┐
│ ParentPool                                    │
│  GetEntity()  = &this->Entity                 │
│  GetUniqueID()= *(GetEntity()+0x118)          │
│                    (offset::unique_id)        │
└───────────────────────────────────────────────┘
* parentpool은 entity를 찾는 역할

┌───────────────────────────────────────────────────────────────┐
│ ComponentParent (= Entity 내부 공통 componentbase찾는 테이블)     │
│  [decrypt::Parent(parent, Type)]로 컴포넌트 Base 계산            │
│   - 내부 비트필드/테이블(예: +0x60, +0xF0 등)을 혼합 사용             │
│   - CompKey1/2 를 이용한 XOR/롤 연산 (decrypt.cpp)                │
└───────────────────────────────────────────────────────────────┘
* 엔티티의 원하는 타입 컴포넌트 베이스를 찾을 수 있음

┌──────────────────────────────────────────────────────┐
│ ComponentBase (타입 공통 헤더)                        │
│  +0x10  type_tag == Type (IsValid() 체크에 사용)      │
│  ...                                                 │
└──────────────────────────────────────────────────────┘

[ MeshComponent ]   (TYPE_VELOCITY)
┌───────────────────────────────────────────────┐
│ ComponentBase                                 │
│  +0x050 velocity            (offset::velocity)│
│  +0x380 root_pos            (offset::root_pos)│
│  +0x810 bone_list           (offset::bone_list) → 
│        *(bone_list+0x20) = bone_base          │
│        bone[i] = *(bone_base + 0x30*i + 0x20) │
│  +0x7C0 rotate_struct       (offset::rotate_struct)
│  +0x8FC rotate_yaw(float)   (offset::rotate_yaw)   │
└───────────────────────────────────────────────┘
* 배열로 엔티티별 데이터가 쭉 적혀있음

[ TeamComponent ]   (TYPE_TEAMBIT)
┌───────────────────────────────────────────────┐
│ ComponentBase                                 │
│  +0x058 team bitfield     (offset::team)      │
│    - bit 0x17 → RED, 0x18 → BLUE, 0x1B → DM   │
└───────────────────────────────────────────────┘

[ ObjectComponent ] (TYPE_OBJECT)
┌───────────────────────────────────────────────┐
│ ComponentBase                                 │
│  +0x018 unique_id        (offset::ObUniqueID) │
│  +0x0E0 pos(Vector3)     (offset::ObPos)      │
└───────────────────────────────────────────────┘

[ HealthComponent ] (TYPE_HEALTH)
┌───────────────────────────────────────────────┐
│ ComponentBase                                 │
│  +0x0DC health (Vector2) (offset::health)     │
│  +0x21C armor  (Vector2) (offset::armor)      │
│  +0x35C barrier(Vector2) (offset::barrier)    │
│  +0x4A4 bubble flag (bit 0)                   │
└───────────────────────────────────────────────┘

[ HeroComponent ]   (TYPE_HERO)
┌───────────────────────────────────────────────┐
│ ComponentBase                                 │
│  +0x0F0 hero_id (offset::hero_id)             │
└───────────────────────────────────────────────┘

[ CommandComponent ](TYPE_CMD)
┌───────────────────────────────────────────────┐
│ ComponentBase                                 │
│  +0x1244 input bitfield  (offset::input)      │
│  +0x12A0 angle (Vector?) (offset::angle)      │
└───────────────────────────────────────────────┘

[ SkillComponent ]  (TYPE_SKILL 계열)
┌───────────────────────────────────────────────┐
│ ComponentBase                                 │
│  +0x1868 skill_rawlist (offset::skill_rawlist)│
│  (전역) ImageBase+0x22A2F00 = skill_struct    │
│                          (offset::skill_struct)│
└───────────────────────────────────────────────┘


ViewMatrix 계산 (function.cpp):
   ViewMatrix = *[ImageBase + 0x4149F68] + 0x7E0
                 (offset::matrix)          (offset::matrix_sub)

struct Matrix (struct.h):
   m11..m44 (4x4), WorldToScreen(), GetCameraVec() 등 유틸 포함

Globals
  game::RayComponent = decrypt::Ray(GameManager) 결과

InitRayStruct (game.cpp):
  ray_component = **(RayComponent - 0x48)
  ZeroMemory(rtIn, 0x1A0)
  ...(필수 필드 구성)...

RayCastIn (struct.h):
┌───────────────────────────────────────────────┐
│ +0x00 from (Vector3)                          │
│ +0x10 to   (Vector3)                          │
│ +0x58 ignore_level (uintptr_t)                │
│ +0x60 entity_list  (uintptr_t)                │
│ +0x68 entity_count (uintptr_t)                │
│ +0x70 padding[0x208]                          │
└───────────────────────────────────────────────┘

RayTrace 호출 (game.cpp):
  rayIn.ignore_level = 0x690000000000017
  // 엔티티 목록 준비
  (ImageBase+0x643270)( &rayIn.entity_list, &LocalEntity.common_parent_ )
  // 실제 캐스트
  ok = (ImageBase+0xF01140)( &rayIn, &rayOut, 0 ) == 0


1) GameManager 해제 → entity_rawlist/카운트 획득
2) 각 RawList 배열 순회 → ParentPool 수집 (IsValid() 확인)
3) current_entity = ParentPool[i]
   link_component = decrypt::Parent(current_entity->GetEntity(), TYPE_LINK)
   link_unique_id = *(link_component + 0xD4)         (offset::link_unqiue_id)
   common_parent = parent_poollist 중 UniqueID==link_unique_id 인 것
4) Entity(common_parent, current_entity) 구성 → Init()/IsValid()
5) current_entity->UniqueID == *(GameManager+0x2E8)  (local_unique_id)이면 LocalEntity
   아니면 game::Entities[]에 축적


### 흐름 요약

[Module(.exe)]
 ImageBase
   ├─ globals (RVA들)
   │    ├─ ViewMatrixPtr_RVA
   │    ├─ RayTrace.force_fn_RVA
   │    ├─ RayTrace.cast_fn_RVA
   │    └─ DecryptKey_RVA
   └─ ...

[GameManager]  (decrypt로 베이스 해제)
 +0x020 head_rawlist ─────────► [RawListNode #0] ─────────► [RawListNode #1] ──► ...
 +0x02C total_count

[RawListNode]
 +0x08 ParentPool* items[]  (배열)
 +0x18 next_rawlist

[ParentPool]  (컨테이너 레벨 핸들)
 ├─ meta: UniqueID 등
 └─ GetEntity() ─────────────► [Entity]
                               ├─ UniqueID (+0x118 등)
                               ├─ (내부 공통 Parent / component map seed)
                               └─ decrypt::Parent(Entity, Type)
                                     └─ [ComponentBase(Type)]
                                         └─ (Health/Mesh/Team/Rotate/Skill/…)



### 데이터 받기

hp 및 위치
서버 기준으로 내 로컬 플레이어는 어느정도 미리 입력값을 반영해 작동하는 거고, 서버에서 이제 제대로 된 델타값이 오면 이전 스냅샷에서 계산해 옳은 값들을 업데이트하고 반영해주고, 서버에서 한번씩 델타값이 아니라 풀 스냅샷을 보내 혹시라도 틀어졌을걸 방지해준다.
특정 DLL이 아니라 Overwatch.exe가 들고 있는 ECS 컨테이너(메모리)에 올라가 있다.

에셋
또한 \Data\Casc에는 실시간 데이터가 아닌 에셋(리소스) 정보들이 들어있어서 필요할 때마다 참조한다.
네트워크 계층(BLTE블록, 암호화 및 압축)에서 받아와 로컬 파일의 Data\Casc에 BLTE 블록(data.000..) + 오프셋 크기 정보인 .idx를 저장해두고,
파일명을 Ckey(컨텐츠 해시)로 매핑 후 Ekey(인코딩 해시)로 매핑, .idx에서 BTLE 블록과 오프셋 정보 찾음
해당 블록을 가져와 복호화 및 압축해제하여 평문으로 메모리에 전개

피격 표시
클라: 발사 → 입력(+시각/시드) 전송, 예측 히트마커 잠깐 표시
서버: 과거로 리와인드 → 레이캐스트 → 히트 확정 & 피해 계산 → Health 감소
서버→클라: 다음 틱에 피격자 Health 델타 + (선택) HitConfirm 이벤트 전송
클라: Health 반영, 예측 히트마커를 확정/취소, 이펙트/사운드 재생


### 모듈 종류

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
