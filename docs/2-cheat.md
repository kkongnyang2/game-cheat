## 핵 종류를 공부해보자

### 목표: 시나리오 탐색
작성자: kkongnyang2 작성일: 2025-07-08

---

### > 가능한 시나리오

A. 프로세스 메모리 인젝션

B. 컴퓨터 비전 기반 aimbot
캡쳐카드 + YOLOv8 -> Δ마우스 주입

C. 에뮬레이트
Cronus Zen, XIM Apex -> 패드 VID/PID 위장

D. 네트워크 단
시퀀스 넘버 + AES-CTR + HMAC 로 방지함.

요즘 핵은 메모리에 넣는 internal 치트는 줄어들고, external 치트가 늘어났다.
내부 치트 : DLL 인젝션, 메모리 값 패치, 커널 드라이버
외부 치트 : (1) 하드웨어 HID - Cronus Zen,XIM,Strike Pack
           (2) DMA/PCle 카드 - 두 대 PC로 메모리 snoop, ESP
           (3) AI 비전 봇 - 화면 캡쳐 후 Y