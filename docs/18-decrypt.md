난독화를 풀어야 할 부분

1. GameManager 포인터/키
목적: GameManager의 베이스를 얻어 entity_rawlist, total_count, local_unique_id에 접근
관련: decrypt::Table(), decrypt::GameManager*Key, DecryptKey 테이블
2. ComponentParent
목적: entity와 type을 조합해 그 엔티티의 해당 타입 컴포넌트 베이스를 얻음
관련: decrypt::Parent(entity, Type) (CompKey1/2 + 엔티티 내부 시드/비트필드 + 전역 키/테이블)
3. Ray
목적: InitRayStruct에 넣을 ray 컴포넌트/함수 포인터를 복호화
관련: decrypt::Ray (RayKey1/2), ray_force, ray_cast RVA
4. Outline
목적: 외곽선 플래그/색상 필드를 XOR 마스크로 풀고 씌움
관련: decrypt::Outline


### GameManager 베이스 얻기

decrypt::GameManagerXorKey()로 XOR 키를 얻음
decrypt::GameManagerIndex()로 인덱스를 뽑고 result를 반환
decrypt::Table()로 가려진 테이블 덩어리에서 유효 엔트리 하나를 골라 주소 후보를 내놓음
decrypt::GameManager()로 최종적으로 XOR 마스크를 한 번 더 벗겨 GameManager 베이스를 얻음