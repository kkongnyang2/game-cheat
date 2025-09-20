Entity = 플레이어 10명에게 부여하는 식별 ID
Component = 상태 데이터
System = 모든 로직 함수. 상태와 조건을 따져 상태를 변경시키는 시스템.


엔티티는 아키타입, 즉 같은 컴포넌트 스트림들을 가지는 애들로 분류된다.
아키타입 A = hp, position x
아키타입 B = barrier, position y

1. 이미 개발자들이 만들어놓은 레이아웃

struct CompLayout {
  ComponentID id;
  uint32_t    offset;   // chunk 메모리 블록 내 시작 바이트 오프셋
  uint16_t    stride;   // 요소 1개 크기(SoA면 타입 크기), 정렬 반영
};

struct ArchetypeLayout {
  ArchetypeID id;
  uint16_t    comp_count;
  CompLayout  comps[MAX_COMPS_PER_ARCHETYPE]; // ComponentID → (offset,stride)
  uint32_t    chunk_data_size;                // 한 청크의 전체 데이터 사이즈
};

2. match_start()시 청크들 할당

struct Chunk {
  ArchetypeLayout* layout;
  uint16_t count, capacity;
  EntityID entity_dense[MAX_PER_CHUNK];

  uint8_t* data_base; // 연속 메모리 블록(여러 컴포넌트 스트림이 여기에 SoA로 들어감)
  // addr = data_base + comp_offset + local_index * stride
};

이 chunk_base 힙 주소들 리턴해서 ?에 저장

3. 10개의 엔티티 생성

entityID 발급 및 아키타입 결정

struct EntityLoc {
  ArchetypeID archetype;
  Chunk*      chunk;       // 해당 엔티티가 들어있는 청크
  uint16_t    local_index; // 청크 내부 인덱스
};

EntityLoc g_entity_locs[MAX_ENTITIES]; // entityId 를 인덱스로 바로 접근

해당 정보는 전역 엔티티 레지스트리(EntityManager) 테이블에 저장

4. 진행

입력값들이 들어오고 진행이 되면 로컬에서 매 프레임 렌더링시스템, UI시스템, 스킬시스템 등을 돌림

엔티티별로 chunk_base + comp_offset + local_index*stride로 찾아가 write

매 프레임 진짜 스냅샷이 들어오면 보간해서 다시 write

5. 변경

특정 엔티티가 아키타입을 변경하면 해당 청크에서 마지막 엔티티 데이터를 그자리에 채워놓고 스왑 삭제, 청크 헤더 및 EntityLoc에 반영