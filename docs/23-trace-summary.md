1. hp 값을 변화시켜가며 CE에서 값 주소 찾기
2. libvmi-capture를 통해 write에 관여하는 rip 및 그때의 레지스터들, 스택 캡쳐 
3. 스택에서 ret 주소를 찾아내 그때 CE memory view에서 흐름 살피기
94 f7 9e 02 f7 7f 00 00
99 e2 93 00 f7 7f 00 00
08 f1 2f 5d b0 00 00 00
0d 55 2f ff f6 7f 00 00
38 f2 2f 5d b0 00 00 00 
74 c8 b5 00 f7 7f 00 00
f0 f1 2f 5d b0 00 00 00
00 ad 9e 01 f7 7f 00 00
7ff700b5c874
4. 단의 첫 rip 주소에 break and trace를 걸어 트레이스 저장.
count : 300
dereference addresses
save stack snapshots
step over rep instructions
hardware breakpoint
5. 혹시 더 이전 흐름 로그도 보고 싶다면 libvmi-trace 이용.