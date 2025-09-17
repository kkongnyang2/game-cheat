## VMI의 구조를 이해해보자

작성자: kkongnyang2 작성일: 2025-08-02

---
### > 하이퍼바이저 선택

|       하이퍼바이저       | VMI 친화도 | 장점                                          | 단점                           | 대표 툴/프로젝트                   |
| :----------------: | :-----: | :------------------------------------------ | :--------------------------- | :-------------------------- |
|       **Xen**      |  ★★★★★  | LibVMI 원조급, altp2m 기반 메모리 이벤트, DRAKVUF 등 풍부 | 설정/운영 복잡, 최신 HW 지원/호환성 이슈    | **DRAKVUF**, LibVMI         |
|    **KVM/QEMU**    |  ★★★★☆  | 리눅스 기본, libvirt 연동 쉽고 널리 사용                 | 이벤트 트랩 구현 시 Xen보다 손이 더 감     | LibVMI, PyVMI               |
|   **VMware ESXi**  |  ★★★★☆  | 공식 introspection API, 안정적                   | 상용/폐쇄적, 비용, 자유도 제한           | VMware VMI SDK (Thin agent) |
|     **Hyper-V**    |  ★★☆☆☆  | Windows 친화, 커널 레벨 접근 가능                     | 공개 도구 적음, 구현 난이도             | 자체 개발 필요                    |
|   **VirtualBox**   |  ★☆☆☆☆  | 셋업 쉬움                                       | VMI용 API 사실상 없음, hacky 작업 필요 | 직접 /proc/…/mem 접근 등 “꼼수”    |
| **Bareflank/ACRN** |  ★★☆☆☆  | 경량/오픈소스, 연구 목적 좋음                           | 문서/커뮤니티 작음, 모든 기능 직접 구현      | 커스텀 하이퍼바이저 개발               |

메모리 이벤트 트랩이면 Xen + LibVMI (DRAKVUF 구조 참고)
libvirt 없이 Xen 자체 툴(xl) 사용. altp2m로 페이지 권한 바꿔 트랩 잡기 수월.

이미 우분투에 kvm 환경이라면 KVM/QEMU + LibVMI
libvirt로 편하게 관리 하고, 문서 많은 libvmi/pyvmi로.

* kvm(kernel based virtual machine) 리눅스 커널 모듈. VT-x를 사용해서 게스트 cpu 실행을 커널 레벨에서 가속해줌. qemu는 가상머신 하이퍼바이저.

윈도우 환경에서 hyper-v로 작업하면 바로 오버워치2 안티치트에 탐색


### vmi 툴

VMI란? 이미 실행 중인 VM 내부 상태(메모리, 레지스터, 파일시스템 등)를 바깥(하이퍼바이저)에서 들여다보거나 조작하는 기술. 게스트 밖에서 이뤄지므로 안티치트 입장에서는 아무것도 알 수 없다.

┌─────────────────────────┐
│  호스트 OS / 하이퍼바이저│  ← LibVMI·DRAKVUF 같은 VMI 라이브러리
│  (VMI 대상 메모리 맵)   │
├─────────▽──────────────┤
│      게스트 VM          │  ← Windows 10 + 게임 + 안티치트
│  ┌───────────────┐      │
│  │  게임 프로세스 │      │
│  └───────────────┘      │
└─────────────────────────┘

VMI 프로그램 구성

┌───────────────────────────────┐
│         Python 레벨           │
│  ┌────────┐   ┌──────────┐    │
│  │ PyVMI  │   │  r2vmi   │    │
│  └──┬─────┘   └─────┬────┘    │
│     │               │         │
├─────▼───────────────▼─────────┤
│          C 라이브러리         │
│  ┌────────┐   ┌──────────┐    │
│  │ libvmi │   │ libkvmi  │    │
│  └──┬─────┘   └─────┬────┘    │
├─────▼─────────────────────────┤
│        하이퍼바이저 층        │
│  ┌────────────┐  (옵션)       │
│  │ QEMU(KVM)  │───KVMI 패치   │
│  └────────────┘               │
├───────────────────────────────┤
│         Linux 커널(KVM)       │
│      (옵션: kvmi-intro patch) │
└───────────────────────────────┘

### 필요한 게스트 정보

[필요정보]
  ├─ 식별: (domain name | domid | qemu pid)
  ├─ 주소변환: DTB (CR3)
  ├─ 해석: 프로파일(json) - 심볼 주소와 구조체 필드 오프셋
  └─ ostype, KASLR 여부, 권한
 
domid, 메모리 맵, DTB를 libvirt API로 자동으로 가져올 수 있음. 직접 qemu로 실행한 vm는 /proc/<pid>/mem 등을 직접 매핑해야 함

(1) VM 식별을 위한 domain name

domain name : libvirt에서 vm을 구분하는 문자열 이름. 사용자가 직접 정함
domain ID : libvirt가 실행 중인 vm에 부여하는 일시적 런타임 ID. virsh domid <name> 로 확인.
qemu PID : qemu 프로세스의 pid. ps aux | grep qemu-system-x86_64 같은 걸로 확인

(2) 주소 변환을 위한 DTB (Directory Table Base)

게스트 가상 주소(VA)를 물리 주소(PA)로 바꿀 때 필요한 페이지 테이블 루트 물리 주소. x86-64에서 CR3 레지스터가 가리킨다.
libvmi가 자동으로 찾아주기도 하지만, 실패하면 직접 지정
리눅스는 task_struct->mm->pgd에서 뽑고, 윈도우면 EPROCESS.DirectoryTableBase에서 뽑는다.

(3) 의미 있는 해석을 위한 프로파일(json 형식)

심볼 주소와 커널 구조체 필드 오프셋 정보.
리눅스면 System.map(공개 심볼 주소) + vmlinux를 바탕으로 dwarf2json(vilatility) 툴을 이용해 만든다.
윈도우면 PDB(program database)를 바탕으로 volatility/rekall/drakpdb 툴을 이용해 만든다.

(4) 그밖에도 KASLR(커널 주소 랜덤화. libvmi가 DTB를 찾기 어렵게 함) 사용 여부(linux_kaslr=true), 권한(root), /etc/libvmi.conf 접근성 들이 필요하다.

### vmi 시나리오

(1) KVM(리눅스)에서 Legacy 방식 (LibVMI + QEMU GDB/패치)

흐름/구조

호스트의 LibVMI가 QEMU의 옛 메모리 접근 경로(GDB stub 또는 QEMU fast-memaccess 패치)로 게스트 RAM을 읽고 씀. 하이퍼바이저 레벨 이벤트(실행/쓰기 트랩)는 사실상 없음. LibVMI를 빌드할 때 -DENABLE_KVM_LEGACY 옵션으로 켬. 

가능한 기능

물리/가상 메모리 읽기·쓰기, 레지스터 접근, 일시정지/재개 등 기본 VMI. 하지만 메모리 이벤트(실행/읽기/쓰기 훅) 는 미지원(또는 매우 제한적) → 실시간 훅 기반 분석/방어엔 부적합. 

호환/현실성(2025)

KVM이 돌아가기만 하면(대부분의 최신 Intel/AMD) 작동 가능. 다만 GDB stub/패치 경로는 느리고 취약하며, 최신 QEMU/커널과의 궁합 이슈가 잦음(프로젝트에서도 “과거 방식”으로 분류). 초보자 권장 X. 

(2) KVMI 커널 경로 (KVM-VMI: 커널·QEMU 패치 + libkvmi)

흐름/구조

KVM에 KVMi(=KVMI) 서브시스템을 추가한 커널(CONFIG_KVM_INTROSPECTION, kvmi-v7 등) + KVMI 대응 QEMU로 게스트를 올림 → 호스트의 libkvmi/LibVMI가 공식적인 VMI API(chardev)를 통해 페이지 접근 제어, 이벤트 통지, vCPU 상태 조회 등을 수행. 

가능한 기능

VM 일시정지·재개, vCPU GPR/MSR 조회, EPT(같은 HW 백킹) 기반 페이지 접근비트 조작, 접근/실행 fault 이벤트 수신, 하이퍼콜/MSR 변경 감시 등 능동 VMI. 

호환/현실성(2025)

메인라인이 아니라 별도 브랜치(커널/ QEMU) 를 빌드해야 해서 유지보수 부담 큼. 예제·릴리스는 존재하지만(최신 릴리스 2023) 최신 배포판 커널과의 버전 핀ning이 필수. EPT 전제를 명시(문서가 “예: Intel의 EPT”라 서술)하므로 AMD NPT는 지원/기능 제약을 확인 필요. 초보자 난이도 높음. 

(3) Xen + LibVMI (가장 표준적인 VMI 경로)

흐름/구조

Xen 하이퍼바이저 위에 Windows/Linux 게스트 실행 → Dom0(또는 별도 도메인)의 LibVMI가 Xen의 VMI/메모리 이벤트(vm_event, mem_access, altp2m) 를 이용해 게스트 메모리/레지스터 접근 + 하드웨어 수준 트랩을 수행. 

가능한 기능

메모리 읽기·쓰기, vCPU 레지스터 접근 + 실행/읽기/쓰기 이벤트 훅, 싱글스텝, altp2m(대체 EPT) 스왑 등 풍부한 이벤트 기반 VMI. LibVMI 측 문서도 메모리 이벤트는 현재 Xen에서만 지원이라 명시. 

호환/현실성(2025)

최신 Intel VT-x/EPT 환경에서 매우 성숙. Xen은 ARM 2-stage paging도 지원(기능 차이 존재), 일반적으론 Intel/EPT에서 가장 안정적. AMD NPT에서도 기본 메모리 접근은 가능하나, 고급 이벤트/altp2m 기반 기능은 Intel EPT 중심 생태계. 실무/연구 커뮤니티와 자료가 가장 풍부. 

(4) Xen + DRAKVUF 엔진 + DRAKVUF Sandbox

흐름/구조

③의 Xen/VMI 기반 위에 DRAKVUF 엔진을 올려 에이전트 없는 트레이싱(syscall, 파일/레지스트리, 프로세스, 메모리 덤프 등)을 수행.

DRAKVUF Sandbox(drakkvuf-sandbox) 는 그 엔진을 자동화/웹UI/큐/스냅샷 템플릿로 감싼 자동 분석 파이프라인. 설치도 “Xen과 DRAKVUF 엔진을 먼저 준비”가 1단계. 

가능한 기능

대량 자동 샌드박싱(시료 업로드→실행→행위 수집→리포트), 프로세스/시스템콜/파일·네트워크·스냅샷/스크린샷 수집 등. 반면, 임의 주소 즉시 패치 같은 미세 조작은 기본 워크플로가 아님(엔진/LibVMI API로 구현은 가능). 

호환/현실성(2025)

하드웨어 요구: Intel VT-x + EPT 필수(AMD 미지원) 를 명시. 최신 Intel CPU라면 대체로 OK. 샌드박스 문서도 CPU 확인 방법(vmxept 플래그)과 소스 빌드 권장을 안내. 구축 난이도는 ③보다 한 단계 높음(엔진+웹 스택+스냅샷 템플릿).