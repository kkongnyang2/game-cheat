### 오버워치 데이터 메모리 구조


Entity = (index, genenration)로 구성된 식별자

Component
아키타입 청크 모델은 청크 블록에 특정 아키타입의 컴포넌트들을 모아놓는다. 엔티티의 슬롯[index]에 아키타입, 청크, row를 기록해서 매핑 위치를 바로 알 수 있다. 구성이 바뀌면 다른 청크로 이동한다.
PosX: [  5,  7, 12, -3, ...]
PosY: [  1,  0,  5,  2, ...]
PosZ: [  0,  0,  0,  0, ...]


스파스 셋 모델은 컴포넌트마다 고유의 저장소가 있다. 엔티티의 슬롯[index]에는 gen/alive의 생존 정보만 기록한다. 서로 간의 매핑은 sparse 테이블로 정보를 적어둔다.
HP:   [100, 90, 45, 12, ...]

PosX: [  5,  7, 12, -3, ...]

따라서 주소 구조는

[World/Registry] ─→ [Archetype 목록] ─→ [해당 Archetype의 Chunk] ─→ row(k)
                                                         └→ SoA 배열들 (hp[k], posX[k], …)

[World/Registry] ─→ [Component T의 저장소] ─→ sparse(entity.index) = dense k ─→ SoA 배열 T[k]


### 게임 중 과정

매치 준비
풀/아키타입 청크 준비. 내부 배열 보존으로 재할당 방지.
서버 엔티티 ID와 클라이언트 Entity 핸들 매핑 테이블 보관

유저2 입장
Entity e2 = createEntity();         // {index=42, gen=5}
addComponent<name>(e2, "kkongnyang2");
addComponent<Transform>(e2, {x:0,y:0,z:0, ...});
addComponent<Health>(e2, {hp:100, max:100});
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
addComponent<Health>(e2b, {hp:100});


### 내가 찾아야 할 정보

일단 전체 런타임 메모리 구조를 어느정도 파악.
헤드인지, 몸샷인지, 미스인지를 판별하는 변수를 찾아서 화면에 perfect/good/miss 띄어주기.