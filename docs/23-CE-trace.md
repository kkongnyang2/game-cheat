CE에서 그냥 처리할 수 있는 걸 알게 됨.

1. hp 값을 변화시켜가며 CE에서 값 주소 찾기
2. libvmi-watch에서 작성한 코드를 통해 write에 관여하는 rip 및 그때의 레지스터들, 스택 캡쳐 
3. CE memory view에서 그 rip 주소에 break and trace를 걸어 트레이스 저장.

count : 300
dereference addresses
save stack snapshots
step over rep instructions
hardware breakpoint


* 해당 트레이스는 trace에 저장되어 있음.