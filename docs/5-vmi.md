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