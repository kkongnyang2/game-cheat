## 디스플레이 공부

작성자: kkongnyang2 작성일: 2025-08-02

---

### 오버워치 디스플레이 이해

[호스트 Linux] ──(VFIO/PCIe 패스스루)──► [게스트 Windows]
                                               │
                                               │  공식 NVIDIA 드라이버
                                               │  DX11 Feature Level OK
                                               │
                                Overwatch 2 런처 + 안티치트

가상머신 환경이라면 장치를 에뮬레이션하지만, 오버워치 2에서는 gpu 장치와 드라이버의 ID를 명확히 확인하며 directX 11만 받아들이기 때문에 게스트에 패스스루 해야한다. 리눅스 커널이 제공하는 패스스루용 프레임워크인 VFIO-PCI를 이용해 PCIe 장치들을 안전하게 넘길 수 있다.

* 벤더 ID, DX Feature Level 11_0 이상 & 서명된 WDDM 2.x 조건도 검사
* Hyper-V/VBS OFF 해야지 못알아챔
* VFIO-PCI는 같은 IOMMU 그룹에 묶인 장치는 통째로 넘겨야 함
* 다른 게임은 에뮬레이션인 virtio-gpu(VirGL)만으로 충분하기도 함


### VM 디스플레이

그럼 VM은 디스플레이를 어떻게 하냐? 장치를 얼마나 넘겨주냐에 따라 다르다.
장치는 에뮬레이션(진짜 하드웨어인척 흉내), 파라버추얼(가짜 장치 인터페이스 virtio 제공), 패스스루(하드웨어 건내주기) 세가지 방식이 있다

전체 패스스루 방식이면 (VFIO-PCI)
CPU도 그대로 사용(일부 trap만 따로 처리), RAM도 할당된 영역만큼 그대로 사용, GPU도 드라이버까지 설치해서 직접 사용. IOMMU 켜고 hostdev 추가 + VFIO 바인딩을 해야 실제 GPU가 넘어간다

분할 패스스루 방식이면 (GVT-g)
GPU를 일부 넘겨줘서 연산은 하되 최종 프레임버퍼는 호스트가 합성.

반가상화 파라버추얼 방식이면 (Virtio-gpu)
가짜 GPU를 만들어 호스트가 수신해와 렌더링 연산과 출력을 담당

VGA 에뮬레이션 방식이면
GPU 사용 없이 오직 그 위에서 장치를 구현해서 진짜 가상으로 연산 처리

ramfb(RAM Framebuffer)는 게스트 os가 메모리에 픽셀 데이터를 쓰면 호스트가 그 메모리를 읽어와 화면에 그대로 표시하는 매커니즘(렌더링x). UEFI(OVMF) 부팅 중에 사용.


### 기본 그래픽 처리 흐름

┌────────────────────────────────────────────────────────┐
│                      애플리케이션들                     │
│  App A  App B  App C ...                                │
│   │      │      │                                       │
│   │렌더링 API 사용 (OpenGL, Vulkan, Direct3D, Skia 등) │
│   ▼      ▼      ▼                                       │
│ [윈도우 버퍼 A] [윈도우 버퍼 B] [윈도우 버퍼 C]         │
└────┬────────────┬────────────┬──────────────────────────┘
     │            │            │
     ▼            ▼            ▼
    버퍼 핸들 전달: X11 Pixmap, Wayland wl_buffer, Windows DWM Surface
            ┌───────────────────────────┐
            │      컴포지터(합성기)        │
            │  - 윈도우 스택/좌표           │
            └─────────┬─────────────────┘
                      ▼  
                  [GPU 연산]
                      │
                      ▼
                   [백 버퍼]
                      │
                      ▼
                 디스플레이 컨트롤러 → 모니터


렌더링? 위치 데이터를 transform과 projection하여 화면에 그래픽을 그림
명령 생성: 응용 프로그램
명령 해석: 렌더링 API + GPU 드라이버
실제 연산: 하드웨어 GPU

렌더링 API? 화면에 3D/2D 그래픽을 그리는 일을 도와주는 약속된 함수 집합 도구. drawTriagnles()같은 명령으로 실제로 삼각형을 그림. 정점 데이터를 GPU로 전송, 조명 계산, 픽셀에 색상 결정, GPU에 필요한 메모리 할당, 프레임버퍼 관리.

GPU 드라이버? 렌더링 API 명령을 GPU 하드웨어 명령으로 번역

합성? 여러 윈도우 버퍼(배경, 창, 패널 등)를 하나의 최종 화면 이미지로 조립하여 화면에 실제로 뭐가 보일지 결정

프레임버퍼? 디스플레이로 보낼 최종 합성 픽셀 메모리. 디스플레이 컨트롤러가 읽어간다. gpu 전용 VRAM(video random access memory)에 있기도 하고, DRAM의 일부를 프레임버퍼 용도로 사용하기도 한다. 어쨌거나 cpu가 그 픽셀을 복사하려하지 않고 주소나 레지스터만 업데이트하며 처리한다.

백 버퍼/프런트 버퍼? 그릴 때 쓰는 임시 버퍼 vs 현재 화면 표시 중인 버퍼로 깜빡임을 방지

한줄 요약하면 렌더링을 위해 응용 프로그램이 만든 명령을 cpu가 준비하고, GPU가 대량 픽셀 연산을 수행한 뒤 결과를 메모리 상의 프레임버퍼에 쓴다. 디스플레이 하드웨어가 그 프레임버퍼를 직접 읽어 모니터에 띄운다.

오버레이(Overlay Plane)? 합성 없이 특정 버퍼를 직접 화면 특정 영역에 하드웨어적으로 얹는 기능. 비디오 같은 프로그램은 너무 자주 바뀌어 합성이 힘드니 직접 디스플레이 컨트롤러의 오버레이 플레인에 말을 거는 것이다.


               ┌───────────────────────┐
               │ Display Controller    │  ← 디스플레이 하드웨어
               │ (scanout engine)      │
               │                       │
               │   Primary  Plane  ──────┐  데스크톱 합성된 전체화면(RGB)
               │   Overlay  Plane  ──┐   │  비디오 YUV 버퍼 (HW 변환)
               │   Cursor   Plane ─┐ │   │  하드웨어 커서
               └────────────────┬──┴─┴───┘
                                │
                              HDMI/eDP/DP → 모니터



### DMA 장치 이해

DMA? Direct Memory Access
CPU를 거치지 않고 디바이스(SSD, GPU, NIC)가 주체가 되어 주 메모리로 직접 읽고 쓰기. cpu가 아닌 gpu로 그래픽을 처리하는 걸 가속이라고 부른다. 또한 cpu와의 전용 고속 버스를 PCIe라고 부르는데, 이를 가지는 장치는 gpu, 사운드카드 등이 있다.

IOMMU? I/O MMU
디바이스 전용 MMU.

VT-d? Intel Virtualization Technology for Directed I/O
intel CPU 칩셋에 내장된 IOMMU 구현 브랜드명. AMD는 AMD-VI라 표기. ACPI DMAR 테이블에 DRHD(하드웨어 유닛)·RMRR(예약 메모리)·ATSR 등을 기록. 리눅스 intel_iommu (옵션 intel_iommu=on), Windows vtd.sys으로 작동.

ACPI? Advanced configuration & Power Interface
UEFI->OS로 건네주는 하드웨어, IRQ, 전원 관리 설명서. 테이블 형식으로 되어 있다.
RSDP(루트) → XSDT/RSDT가 여러 하위 테이블 주소를 나열.
• 대표 테이블: FADT(전원), MADT(APIC·CPU), DSDT(AML 코드), DMAR(IOMMU) 등

DMAR? Direct Memory Access Remapping
ACPI 속 Intel VT-d(IOMMU)를 초기화하기 위한 전용 테이블 이름. DMAR 엔진이 어디에 몇 개가 달려 있으며 어떤 PCI 장치를 맡는가.
• DRHD(DMA Remapping Hardware Unit)은 IOMMU 레지스터 베이스, 관리 버스.
• RMRR(Resserved Memory Region Reporting)은 BIOS, iGPU, USB 레거시 등 특정 장치 부팅 중 DMA 해야 하는 예약 메모리.
• ATSR(Address-Translation Service Remap)