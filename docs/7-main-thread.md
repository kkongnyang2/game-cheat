### >

github 오픈소스 - LibVMI과 PyVMI 예제. Python 바인딩으로 프로세스 리스트와 메모리 읽기 데모.
https://github.com/maxking/pyvmi_example?utm_source=chatgpt.com

github 오픈소스 - r2vmi와 IntroVirt. LibVMI + Radare2 플러그인/KVM 전용 확장과 VMI 프레임워크
https://github.com/Wenzel/r2vmi?utm_source=chatgpt.com
https://github.com/IntroVirt/IntroVirt?utm_source=chatgpt.com

github 오픈소스 - pixelbot
https://github.com/AliShazly/pixelbot?utm_source=chatgpt.com


치트 소스
P. Karkallis, J. Blasco, G. Suarez-Tangil, and S. Pastrana. Detecting video-game injectors exchanged in game
cheating communities. In Computer Security – ESORICS 2021: 26th European Symposium on Research
in Computer Security, Darmstadt, Germany, October 4–8, 2021, Proceedings, Part I, page 305–324, Berlin,
Heidelberg, 2021. Springer-Verlag.


### > 게스트 측 보안 제거

안티치트가 Hyper-V 커널 레이러를 감지하면 바로 블루스크린.

step 1. 설정 › 앱 › 선택적 기능
```
    Hyper-V
    Windows Hypervisor Platform
    Virtual Machine Platform
    Windows Sandbox
    Windows Subsystem for Linux
    → 전부 체크 해제(제거) ▸ 재부팅
```

step 2. 설정
```
    core isolation → Memory Integrity OFF
    Optional: bcdedit /set hypervisorlaunchtype off
```