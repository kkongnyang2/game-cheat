## 오버워치 구조 레퍼런스를 살펴보자

작성자: kkongnyang2 작성일: 2025-08-27

---
### 일반적인 ECS 엔진

Entity = 플레이어 10명에게 부여하는 식별 ID
Component = 상태 데이터
System = 모든 로직 함수. 상태와 조건을 따져 상태를 변경시키는 시스템.


엔티티는 아키타입, 즉 같은 컴포넌트 스트림들을 가지는 애들로 분류된다.
아키타입 A = hp, position x
아키타입 B = barrier, position y


(.data) g_world ──► (heap) World
                      ├─ registry ─► (heap) EntityLoc[Max]   // entityId → (chunk, idx)
                      └─ pools     ─► Map<ArchetypeID, ArchetypePool*>

  (heap) ArchetypePool
    ├─ layout ─► (rdata/heap) ArchetypeLayout { CompLayout[] (offset,stride) }
    └─ chunks : vector<Chunk*> ─► (heap) Chunk { count, entity_dense[], data_base … }

주소 = chunk.data_base + layout.comp[id].offset + local_index * layout.comp[id].stride



* 최초의 루트 포인터 : g_world 포인터
* world : 런타임의 루트 컨텍스트. 게임 한 판을 대표하는 최상위 그릇.
* registry : 엔티티/컴포넌트/아키타입/청크를 인덱싱하는 O(1) 중앙 테이블
            EntityLoc[], FreeList, IDAllocator, ArchetypeTbale, LayoutTable
* EntityLoc : 레지스트리의 테이블 중 가장 핫함
            entityId → (archetype, chunk*, local_index)
* ArchetypePool: 실제 데이터 컨테이너 집합
* ArchetypeLayout: 이미 개발자들이 만들어놓은 레이아웃
                  id, comp_count, compLayout[], chunk_data_size
* CompLayout : id, offset, stride


찾아가는 방법은 따라서 두개.

g_world.registry.EntityLoc.chunk 도착해서 layout로 offset,stride 얻어서 주소 계산
g_world.pools.ArchetypePool.chunks 도착해서 entity_dense 보고 원하는 엔티티/컴포넌트 찾기. layout로 offset,stride 얻어서 주소 계산


게임 진행

1. match_start()시 청크들 할당
2. 10개의 엔티티 생성 - entityID 발급 및 아키타입 결정
3. 입력값들이 들어오고 진행이 되면 로컬에서 매 프레임 렌더링시스템, UI시스템, 스킬시스템 등을 돌림
4. 엔티티별로 chunk_base + comp_offset + local_index*stride로 찾아가 write
5. 매 프레임 진짜 스냅샷이 들어오면 보간해서 다시 write
6. 특정 엔티티가 아키타입을 변경하면 스왑 삭제, 청크 헤더 및 EntityLoc에 반영


### 오버워치2 메모리 구조 레퍼런스

```
namespace offset
{
	// PoolList
	constexpr auto entity = 0x8;
	constexpr auto unique_id = 0x118;
	constexpr auto local_unique_id = 0x2E8;
	constexpr auto link_unqiue_id = 0xD4;

	constexpr auto entity_rawlist = 0x20;
	constexpr auto entity_rawlist_count = 0x2C;
	constexpr auto next_rawlist = 0x18;
}
```

PoolHeader
 ├─ 0x18: next (다음 페이지)
 ├─ 0x20: rawlist (엔트리 배열 시작 주소)  ← 여기를 따라가면
 └─ 0x2C: count

rawlist:  [ Entry0 ][ Entry1 ][ Entry2 ] ... (count개)
              └─ +0x08 에 Parent(엔티티) 포인터 (구조체형일 때)


그렇게 찾은 Parent(즉 엔티티) 주소를 decrypt::Parent(parent, N)에 입력하면 ComponentBase 리턴

uint64_t decrypt::Parent(uint64_t Parent, uint8_t Type) //OK
{
	__try
	{
		if (IsValidPtr(Parent))
		{
			uint64_t v1 = Parent;
			uint64_t v2 = (uint64_t)1 << (uint64_t)(Type & 0x3F);
			uint64_t v3 = v2 - 1;
			uint64_t v4 = Type & 0x3F;
			uint64_t v5 = Type / 0x3F;
			uint64_t b1 = *(_QWORD*)(v1 + 0x60);
			uint64_t b2 = *(_QWORD*)(v1 + 8i64 * v5 + 0xF0);
			uint64_t b3 = (v2 & *(_QWORD*)(v1 + 8i64 * v5 + 0xF0)) >> v4;
			uint64_t b4 = (v3 & b2) - (((v3 & b2) >> 1) & 0x5555555555555555i64);
			uint64_t b5 = *(unsigned __int8*)(v5 + v1 + 272)
				+ ((72340172838076673i64
					* (((b4 & 0x3333333333333333i64)
						+ ((b4 >> 2) & 0x3333333333333333i64)
						+ (((b4 & 0x3333333333333333i64) + ((b4 >> 2) & 0x3333333333333333i64)) >> 4)) & 0xF0F0F0F0F0F0F0Fi64)) >> 56);
			uint64_t Key1 = CompKey1;
			uint64_t Key2 = CompKey2;
			uint64_t b6 = *(_QWORD*)(b1 + 8 * b5);
			uint64_t b7 = Key2 ^ ((unsigned int)b6 | ((unsigned int)b6 | b6 & 0xFFFFFFFF00000000ui64 ^ ((unsigned __int64)((unsigned int)b6 ^ ~(unsigned int)*(_QWORD*)(ImageBase + 0x3ACDCA0 + (Key1 & 0xFFF))) << 32)) & 0xFFFFFFFF00000000ui64 ^ ((unsigned __int64)((unsigned int)b6 ^ 0xBC6F2947) << 32));
			uint64_t b8 = (unsigned int)b7 | b7 & 0xFFFFFFFF00000000ui64 ^ ((unsigned __int64)(unsigned int)(b7
				+ __ROL4__(
					*(_QWORD*)(ImageBase + 0x3ACDCA0 + (Key1 >> 52)),
					1)) << 32);
			return -(int)b3 & ((unsigned int)b8 | b8 & 0xFFFFFFFF00000000ui64 ^ ((unsigned __int64)((unsigned int)b8 ^ (unsigned int)~(*(_QWORD*)(ImageBase + 0x3ACDCA0 + (Key1 & 0xFFF)) >> 32)) << 32));
		}
	}
	__except (1) {}
	return 0;
}

비트마스크/벡터 명령어(SSE)로 키를 섞어 결과 포인터를 복호화

참고로 N은 eComponentType

enum eComponentType
{
	TYPE_ANIMATE = 0x1,
	TYPE_RAY = 0x02,
	TYPE_VELOCITY = 0x04,
	TYPE_TEAMBIT = 0x1F,
	TYPE_ROTATE = 0x2D,
	TYPE_PLAYER = 0x31,
	TYPE_LINK = 0x32,
	TYPE_VISCHECK = 0x33,
	TYPE_SKILL = 0x35,
	TYPE_ROTATE2 = 0x37,
	TYPE_OBJECT = 0x38,
	TYPE_HEALTH = 0x39,
	TYPE_CMD = 0x41,
	TYPE_HEROID = 0x52,
	TYPE_OUTLINE = 0x5A,
};

찾은 ComponentBase에서 아래와 같은 오프셋을 추가하면 해당 데이터 항목

namespace offset
{
	// MeshComponent
	constexpr auto velocity = 0x50;
	constexpr auto root_pos = 0x380;
	constexpr auto bone_list = 0x810;

	// TeamComponent
	constexpr auto team = 0x58;

	// RotateComponent
	constexpr auto rotate_struct = 0x7C0;
	constexpr auto rotate_yaw = 0x8FC;

	// SkillComponent
	constexpr auto skill_rawlist = 0x1868;
	constexpr auto skill_struct = 0x22A2F00;

	// ObjectComponent
	constexpr auto ObUniqueID = 0x18;
	constexpr auto ObPos = 0xE0;
	
	// HealthComponent
	constexpr auto health = 0xDC;
	constexpr auto armor = 0x21C;
	constexpr auto barrier = 0x35C;

	// HeroComponent
	constexpr auto hero_id = 0xF0;
	
	// CommandComponent
	constexpr auto input = 0x1244;
	constexpr auto angle = 0x12A0;
}


### 그럼 PoolList는 어떻게 찾는데?

XorKey, Index, Table, GM 순으로 복호화해서.

uint64_t decrypt::GameManagerXorKey(uint64_t a1) //OK
{
	__try
	{
		__int64 v1; // rbx
		unsigned __int64 v2; // rdi
		unsigned __int64 v3; // r8
		unsigned __int64 v4; // rax
		unsigned __int64 v5; // rbx
		unsigned __int64 v6; // rcx
		unsigned __int64 v7; // rcx
		__m128i v8; // xmm1
		__m128i v9; // xmm2
		__m128i v10; // xmm0
		__m128i v11; // xmm1

		v1 = a1;
		v2 = ImageBase + 0x5D14D2;
		v3 = v2 + 0x8;
		v4 = 0i64;
		v5 = *(__int64*)(ImageBase + 0x3ACECC0
			+ 8 * (((unsigned __int8)v1 + 0x66) & 0x7F)
			+ (((unsigned __int64)(v1 + 0xF74216354A20866i64) >> 7) & 7)) ^ v2 ^ (v1 + 0xF74216354A20866i64);
		v6 = (v3 - v2 + 7) >> 3;
		if (v2 > v3)
			v6 = 0i64;
		if (v6 && v6 >= 4)
		{
			v7 = v6 & 0xFFFFFFFFFFFFFFFCui64;
			ZeroMemory(&v8, sizeof(v8));
			ZeroMemory(&v9, sizeof(v9));

			do
			{
				v4 += 4i64;
				v8 = _mm_xor_si128(v8, _mm_loadu_si128((const __m128i*)v2));
				v10 = _mm_loadu_si128((const __m128i*)(v2 + 16));
				v2 += 32i64;
				v9 = _mm_xor_si128(v9, v10);
			} while (v4 < v7);
			v11 = _mm_xor_si128(v8, v9);
			v5 ^= _mm_xor_si128(v11, _mm_srli_si128(v11, 8)).m128i_u64[0];
		}
		for (; v2 < v3; v2 += 8i64)
			v5 ^= *(_QWORD*)v2;

		return v5 ^ ~v3 ^ 0xF74216354A20866i64;
	}
	__except (1) {}
	return 0;
}

int decrypt::GameManagerIndex(uint64_t a1) //OK
{
	__try
	{
		__int64 v1; // rdi
		__int64 v2; // rbx
		unsigned __int64 v3; // rsi
		__int64 v4; // r10
		__int64 v5; // r11
		__int64 v6; // rax
		_QWORD* v7; // r9
		__int64 v8; // r10
		__int64 v9; // rax

		v1 = a1;
		v2 = ImageBase + 0x5D70D0;
		v3 = v2 + 0xB;
		v4 = 0xCC0BFEAA1FF95DA9i64;
		v5 = v1 + 24;
		v6 = 4i64;

		do
		{
			v7 = (_QWORD*)(v5 - 16);
			v8 = *(_QWORD*)(v5 + 8) ^ (v5 - 8) ^ (v5 - 16) ^ (v5 - 24) ^ *(_QWORD*)v5 ^ *(_QWORD*)(v5 - 24) ^ *(_QWORD*)(v5 - 8) ^ v5 ^ ~(v5 + 8) ^ v4;
			v5 += 0x28i64;
			v4 = *v7 ^ v8;
			--v6;
		} while (v6);

		for (; v2 + 64 < v3; LOBYTE(v4) = v9 ^ v4)
		{
			v9 = *(_QWORD*)v2 ^ ~v2;
			v2 += 8i64;
		}
		return ((unsigned __int8)v4 ^ 6) & 0xF;
	}
	__except (1) {}
	return 0;
}

uint64_t decrypt::Table() //OK
{
	__try
	{
		uint64_t Key1 = GMKey1;
		uint64_t Key2 = GMKey2;


		uint64_t qword_3AE3978 = *(uint64_t*)(ImageBase + 0x3AE3978);

		uint64_t v2 = (unsigned int)Key2 ^ (unsigned int)qword_3AE3978 | (Key2 ^ ((unsigned int)qword_3AE3978 | ((unsigned int)qword_3AE3978 | qword_3AE3978 & 0xFFFFFFFF00000000ui64 ^ ((unsigned __int64)(unsigned int)(557102392 - qword_3AE3978) << 32)) & 0xFFFFFFFF00000000ui64 ^ ((unsigned __int64)(unsigned int)(qword_3AE3978 - (*(_QWORD*)(((unsigned __int64)Key1 >> 52) + ImageBase + 61660320) >> 32)) << 32))) & 0xFFFFFFFF00000000ui64 ^ ((unsigned __int64)(((unsigned int)Key2 ^ (unsigned int)qword_3AE3978) + __ROL4__(*(_QWORD*)((Key1 & 0xFFF) + ImageBase + 61660320), 1)) << 32);
		uint64_t v3 = (unsigned int)v2 | v2 & 0xFFFFFFFF00000000ui64 ^ ((unsigned __int64)(unsigned int)(-229200722 - v2) << 32);
		uint64_t GMXorKey = decrypt::GameManagerXorKey(*(uint64_t*)(0xA0 + v3));

		uint64_t XorBuf[20];
		uint64_t v6;
		
		for (uint64_t i = 0i64; i < 20; ++i)
		{
			XorBuf[i] = *(_QWORD*)(v3 + 8 * i) ^ GMXorKey;
			switch ((unsigned __int64)(GMXorKey & 7))
			{
			case 0ui64:
				v6 = 0xA22115FFD20A1F26i64;
				break;
			case 1ui64:
				v6 = 0xC93978D9115078F0i64;
				break;
			case 2ui64:
				v6 = 0xDE83BAC0690CC8A2i64;
				break;
			case 3ui64:
				v6 = 0x57C00EA24B120A82i64;
				break;
			case 4ui64:
				v6 = 0xB2730B23F294D216i64;
				break;
			case 5ui64:
				v6 = 0x53F351AE73BE44D4i64;
				break;
			case 6ui64:
				v6 = 0x3B207AD9732C7F3Ei64;
				break;
			default:
				v6 = 0x1C84FF058EB23CBCi64;
				break;
			}
			GMXorKey += v6;
		}

		int Idx = GameManagerIndex((uint64_t)XorBuf);
		uint64_t result = XorBuf[Idx + 4];
		return result;
	}
	__except (1) {}
	return 0;
}

uint64_t decrypt::GameManager() //OK
{
	__try
	{
		uint64_t engm = *(uint64_t*)(Table() + 0x30);
		uint64_t v1 = *(uint64_t*)(ImageBase + 0x3ACE14A) - 0x14737E1555DD8CD7i64;

		if (engm != v1)
			return engm ^ v1;
	}
	__except (1) {}
	return 0;
}

uint64_t decrypt::Ray(uint64_t GameManager)
{
	__try
	{
		uint64_t Key1 = RayKey1;
		uint64_t Key2 = RayKey2;

		uint64_t  v3 = *(_QWORD*)(GameManager + 0x220);
		uint64_t  v4 = (unsigned int)v3 | v3 & 0xFFFFFFFF00000000ui64 ^ ((unsigned __int64)(unsigned int)(v3 + 0x184E9681) << 32);
		uint64_t  v5 = Key2 ^ ((unsigned int)v3 | v4 & 0xFFFFFFFF00000000ui64 ^ ((unsigned __int64)(unsigned int)(v4
			+ __ROL4__(
				*(_QWORD*)(ImageBase + 0x3ACDCA0 + ((unsigned __int64)Key1 >> 52)) >> 32,
				1)) << 32));
		return (unsigned int)v5 | ((unsigned int)v5 | v5 & 0xFFFFFFFF00000000ui64 ^ ((unsigned __int64)((unsigned int)v5 - (unsigned int)*(_QWORD*)(ImageBase + 0x3ACDCA0 + (Key1 & 0xFFF))) << 32)) & 0xFFFFFFFF00000000ui64 ^ ((unsigned __int64)(unsigned int)(2 * v5 + 994381642) << 32);
	}
	__except (1) {}
	return 0;
}

uint64_t decrypt::Outline(uint64_t Xor)
{
	__int64 v1; // rbx
	unsigned __int64 v2; // rdi
	unsigned __int64 v3; // r8
	unsigned __int64 v4; // rax
	__int64 v5; // rbx
	unsigned __int64 v6; // rcx
	unsigned __int64 v7; // rcx
	__m128i v8; // xmm1
	__m128i v9; // xmm2
	__m128i v10; // xmm0
	__m128i v11; // xmm1
	unsigned __int64 retaddr; // [rsp+28h] [rbp+0h]
	char v14; // [rsp+68h] [rbp+40h]

	v1 = Xor;
	v2 = ImageBase + 0x7AEC32;
	v3 = v2 + 0x8;
	v4 = 0i64;
	v5 = *(uint64_t*)(ImageBase + 0x3AA7180
		+ 8 * (((unsigned __int8)v1 - 78) & 0x7F)
		+ (((unsigned __int64)(v1 + 817210438226979250i64) >> 7) & 7)) ^ v2 ^ (v1 + 817210438226979250i64);
	v6 = (v3 - v2 + 7) >> 3;
	if (v2 > v3)
		v6 = 0i64;
	if (v6 && v6 >= 4)
	{
		v7 = v6 & 0xFFFFFFFFFFFFFFFCui64;
		ZeroMemory(&v8, sizeof(v8));
		ZeroMemory(&v9, sizeof(v9));

		do
		{
			v4 += 4i64;
			v8 = _mm_xor_si128(v8, _mm_loadu_si128((const __m128i*)v2));
			v10 = _mm_loadu_si128((const __m128i*)(v2 + 16));
			v2 += 32i64;
			v9 = _mm_xor_si128(v9, v10);
		} while (v4 < v7);
		v11 = _mm_xor_si128(v8, v9);
		auto a1 = _mm_xor_si128(v11, _mm_srli_si128(v11, 8));
		v5 ^= *(uint64_t*)&a1;
	}
	for (; v2 < v3; v2 += 8i64)
		v5 ^= *(uint64_t*)v2;

	return v5 ^ ~v3 ^ 0xB5750905542A9B2i64;
}

### 그럼 복호화 코드는 어떻게 찾는데?

실행 중에 계속 사용되는 복호화 코드는 보통 게임 EXE 혹은 보안 DLL의 .text에 있다.
그리고 키/상수/테이블 같이 자수 바뀌지 않는 상수와 셔플마스크, RVA 포인터들은 .rdata에 놓이는 경우가 많다.
세션이나 프레임별로 바뀌는 키는 힙이나 TLS 같은 런타임 저장소에 있다. 


	// Decryption
	constexpr auto decrypt_key = 0xB3D20;



### 시나리오

방법1. CE로 값 주소 찾거나 pfn_writewatch 힙 할당 잡기로 그쪽 관심영역 전부 덤프

데이터 힙 부분 열심히 덤프해봤자 산발적이고 의미도 없음.

방법2. CE로 what access 찾아보기

디버거 붙이는 순간 옵치 무결성 안티치트 발동해 꺼짐

방법3. 사용할 방법

CE로 값을 변화시켜가며 확실한 시드 10개 확보

혹은 힙에서 (pfn,off) 히트율과 주기성, 값 형태로 확률을 높인 시드 확보해도 됨

해당 페이지에 pfn writewatch를 건 상태에서 값을 변화시켜 이벤트가 찍히는지 확인

store 직전 5만개 명령어 정도를 마이크로 트레이스, 게임 exe가 명령내린 거만 필터링

mov [RAX+disp], reg/imm 이게 store 명령어인데 RAX(베이스) 계산하는 블록의 시작 주소가 복호화 루틴 후보

.rdata의 16바이트 셔플 마스크/곱셈 상수 XREF로 교차확인, 참조 함수 목록이랑 대조해 복호화→주소계산→store 패턴찾기

복호화 함수 진입/리턴을 잡는 커스텀 플러그인 추가

자동화 파이프라인으로 승격


즉 store 부터 다음 store까지의 명령어를 다 잡는다는거네. 그리고 적당히 뒷부분을 살펴서 원하는 복호화→주소계산→store 패턴을 찾는건가. 그럼 플러그인에 해당 기능을 넣어줘. 그리고 va를 시드로 줬을때 pfn으로 번역해서 사용하는 기능도, 그리고 화이트리스트 안의 rip만 수집하는 기능도 추가해줘.

$ sudo vmi-process-list win10 | grep -i overwatch
$ cd ~/game-cheat/code
sudo /home/kkongnyang2/vmi-venv/bin/python3 -m dump.watch_data_ranges \
  --domain win10 \
  --pid 11448 \
  --module Overwatch.exe \
  --ist-json /root/symbols/ntkrnlmp.json \
  --drakvuf /usr/local/bin/drakvuf \
  --mode seed \
  --watch pfn \
  --seed-addr 0x19D741D57FC@1 \
  --seed-default-pad-pages 1 \
  --duration 15 \
  --interval 5  \
  --ww-mode both \
  --peek-bytes 32 \
  --max-changes 16 \
  --max-events 5000 \
  --max-bytes 8000000 \
  --outdir /home/kkongnyang2/watch_dump

sudo /home/kkongnyang2/vmi-venv/bin/python3 -m dump.watch_data_ranges \
  --domain win10 \
  --pid 11448 \
  --module Overwatch.exe \
  --ist-json /root/symbols/ntkrnlmp.json \
  --drakvuf /usr/local/bin/drakvuf \
  --mode seed \
  --watch va \
  --seed-addr 0x19D741D57FC@1 \
  --duration 15 \
  --outdir /home/kkongnyang2/watch_dump


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