나-솔저

18개

공통 부분 / 개별 부분
첫번째: 7fc 468 a78(4) 730(4) b00 b88(4) 2f8 f38 ad0 650 a30 / d04 664 8b8 cd8(4) 6a8(4) 280 120
두번째: 7fc 468 f38 ad0 650 a78(4) 730(4) b00 b88(4) 2f8 a30 / 3a4 d04 288 680 a48(4) 418(4) e80
세번째: 468 a78(4) 730(4) b00 b88(4) 2f8 a30 f38 ad0 650 7fc / 1f8(4) 6f8 080 8e4 244 7b8(4) 8a0
닷번째: 468 f38 ad0 650 a78(4) 730(4) b00 b88(4) 2f8 a30 7fc / 480 530 a48(4) f84 8e4 6f8 6a8(4)
엿번째: 7fc b88 a78(4) 730(4) b00 b88(4) 2f8 a30 / 520 f84 8e4 ff0 708 c80 d28 570 1f8(4) bc8(4)

트랩성공한거: 468 a78(4) 730(4) b00 b88(4) 2f8 a30 650 / f84 cb4 474 a48(4) 6a8(4) 6f8
트랩실패한거: f38 7fc ad0 / 174 8e4

성공한거 rip정보
207a7bb mov dword ptr [rdx + rcx], eax  474 a48 f84 6a8

207cc50부터 트레이스를 걸었더니 call207a710까지는 잘하는데 ja 안가고 jmp rcx에서 207a83f로 넘어가버림. 그리고 ret해서 207cc8e와서는 jne가고 ret해서 207cd86으로. 그리고 또 ret해서 20ff2ae로.
ret 세번하고 20FF2AE로 감.
20ff1af시작 call2b54070 call207ce70 jne20ff302 call20fe9d0 call2b54070 call2072ca0 call207b180 call2130a60 call2b54070 call207cd00 여기에 속함 call2072cao call207c410 

jmp rcx에서는 0207a742 0207a763 0207a784 0207a7a5 0207a7c5 0207a7fd 0207a81c 0207a83f 0207a86b
이 중 0207a7a5를 타고 가면 보인다.

"rax":"0x4360b165","rcx":"0x1c5a5fd3460","rdx":"0x14"
"rax":"0xe3","rcx":"0x1c5b9d9d9b0","rdx":"0x98"
"rax":"0x436bc000","rcx":"0x1d6f9218f70","rdx":"0x14"
"rax":"0xd4","rcx":"0x1d6fc9e4610","rdx":"0x98

20e80cf mov dword ptr [rdx], ecx  a78 730 b88
2119ad1. jne감. call 20fa6e0 이 안에서 핸들러 20b5e30 선택. 끝나면 다시 2119ad1.

"rcx":"0x71","rdx":"0x1a2169c8a78"
"rcx":"0xb2","rdx":"0x1a2169c9730"
"rcx":"0xde","rdx":"0x1a2169d7b88"

20e80a6 mov dword ptr [rdx], ecx  b00 2f8 a30

"rcx":"0x43658445","rdx":"0x1a2169cab00"
"rcx":"0x436e6486","rdx":"0x1a2169d92f8"
"rcx":"0x435685f6","rdx":"0x1a216a5da30"

17ecdad mov qword ptr [rbx], rax  468 650

"rax":"0x43540000","rbx":"0x1c59e905468"
"rax":"0x43624000","rbx":"0x28debfee650"

2071a3d movss dword ptr [rax + rdi], xmm6  cb4

"rax":"0x14","rdi":"0x1c5a5e1eca0"

17f250b mov qword ptr [rbx], rax  6f8

"rax":"0x43710000","rbx":"0x1d6fb74d6f8


나-소전

18개

2f8 888 0c0 7fc bb8 820 a78(4) 730(4) b00 b88(4) 2f8 a30 894 1f4 480 d28 a48(4) 418(4)

17ecdad mov qword ptr [rbx], rax 2f8
"rax":"0x43494000","rbx":"0x1d7f05052f8"

20ccaaf mov dword ptr [r8 + rsi], eax a48
"r8":"0x98" "rsi":"0x1d848f259b0" "rax":"0xca"


상대-소전

2개

5D0 130
130 C00
1BB04112320 1BB19FF2F08

0x17ecdad f08


CE로 다른 주소 rip 찾음 2051c5d





일단 17ecdad

Overwatch.exe+200C517 - 48 8D 95 00010000     - lea rdx,[rbp+00000100]      # src
Overwatch.exe+200C51E - 48 8B CB              - mov rcx,rbx                 # dst
Overwatch.exe+200C521 - E8 6A087EFF           - call Overwatch.exe+17ECD90  # 이동 대입 함수ㄱㄱ
Overwatch.exe+17ECD90 - 48 89 5C 24 08        - mov [rsp+08],rbx
Overwatch.exe+17ECD95 - 57                    - push rdi
Overwatch.exe+17ECD96 - 48 83 EC 20           - sub rsp,20 { 32 }
Overwatch.exe+17ECD9A - 48 8B FA              - mov rdi,rdx                 # src
Overwatch.exe+17ECD9D - 48 8B D9              - mov rbx,rcx                 # dst
Overwatch.exe+17ECDA0 - 48 3B D1              - cmp rdx,rcx                 # src=dst 면 아무것도 안하고 복귀
Overwatch.exe+17ECDA3 - 74 22                 - je Overwatch.exe+17ECDC7
Overwatch.exe+17ECDA5 - E8 B64FD5FE           - call Overwatch.exe+541D60   # dst의 기존 참조 clear로 추정
Overwatch.exe+17ECDAA - 48 8B 07              - mov rax,[rdi]
Overwatch.exe+17ECDAD - 48 89 03              - mov [rbx],rax               # 8바이트 복사. 4889F2DD40 에 있던 0 4889F2E220 으로
Overwatch.exe+17ECDB0 - 48 8B 47 08           - mov rax,[rdi+08]
Overwatch.exe+17ECDB4 - 48 89 43 08           - mov [rbx+08],rax            # 다음 8바이트 복사. 4889F2DD48 에 있던 100000000 4889F2E228 으로
Overwatch.exe+17ECDB8 - 33 C0                 - xor eax,eax
Overwatch.exe+17ECDBA - 48 89 07              - mov [rdi],rax               # 원래 src에는 0으로
Overwatch.exe+17ECDBD - 89 47 08              - mov [rdi+08],eax
Overwatch.exe+17ECDC0 - C7 47 0C 02000000     - mov [rdi+0C],00000002 { 2 } # 원래 src+C 에는 2라는 타입/상태 필드 남겨놓기
Overwatch.exe+17ECDC7 - 48 8B C3              - mov rax,rbx                 # 반환값 dst
Overwatch.exe+17ECDCA - 48 8B 5C 24 30        - mov rbx,[rsp+30]
Overwatch.exe+17ECDCF - 48 83 C4 20           - add rsp,20 { 32 }
Overwatch.exe+17ECDD3 - 5F                    - pop rdi
Overwatch.exe+17ECDD4 - C3                    - ret 
Overwatch.exe+200C526 - EB 12                 - jmp Overwatch.exe+200C53A   # 여러 경로에서 얘로 갈텐데, 17ecdad를 거친 트레이스는 무조건 타입코드가 2가됨.
Overwatch.exe+200C53A - 45 33 C0              - xor r8d,r8d
Overwatch.exe+200C53D - 41 8B C0              - mov eax,r8d
Overwatch.exe+200C540 - 48 89 43 10           - mov [rbx+10],rax            # dst+0x10 을 0으로 초기화
Overwatch.exe+200C544 - 8B 8D 0C010000        - mov ecx,[rbp+0000010C]      # src+C 에서 타입/상태 코드를 읽어옴
Overwatch.exe+200C54A - 83 E9 05              - sub ecx,05 { 5 }
Overwatch.exe+200C54D - 74 35                 - je Overwatch.exe+200C584    # ==5면 200C584
Overwatch.exe+200C54F - 83 E9 06              - sub ecx,06 { 6 }
Overwatch.exe+200C552 - 74 30                 - je Overwatch.exe+200C584    # ==11면 200C584
Overwatch.exe+200C554 - 83 E9 01              - sub ecx,01 { 1 }
Overwatch.exe+200C557 - 74 05                 - je Overwatch.exe+200C55E
Overwatch.exe+200C559 - 83 F9 01              - cmp ecx,01 { 1 }
Overwatch.exe+200C55C - 75 5C                 - jne Overwatch.exe+200C5BA   # 다 아니면 200C5BA
Overwatch.exe+200C55E - 48 8B 9D 00010000     - mov rbx,[rbp+00000100]      # ==12나 13 이면 여기
Overwatch.exe+200C565 - 48 85 DB              - test rbx,rbx
Overwatch.exe+200C568 - 74 50                 - je Overwatch.exe+200C5BA    # src가 널이면 예외처리
Overwatch.exe+200C56A - F6 05 0F26DC01 10     - test byte ptr [Overwatch.exe+3DCEB80],10 { (20),16 }    # 글로벌 플래그의 0x10 비트가 켜져 있으면
Overwatch.exe+200C571 - 74 08                 - je Overwatch.exe+200C57B    # 꺼져 있으면 200C57B
Overwatch.exe+200C573 - 48 8B CB              - mov rcx,rbx
Overwatch.exe+200C576 - E8 454E53FE           - call Overwatch.exe+5413C0   # 켜져 있으면 후처리 함수 호출

값을 src에서 dst로 이동하고 원래 있던 곳에선 없애는 과정으로 보임. 그리고 타입에 따라 추가 작업도.

이때 트레이스에는 다 아니어서(2엿음) 200C5BA로 감
Overwatch.exe+200C5BA - mov ebx,[rbp+000005D8]      # 현재 상태/모드/카운터로 추정. 1
Overwatch.exe+200C5C0 - add rsi,02                  # 이번에 처리한 항목이 2바이트짜리라, 그만큼 건너뛰고 다음걸 보겠다. 1FDC9173BC0+02
Overwatch.exe+200C5C4 - mov rdi,[rsp+30]            # 얘는 버퍼 시작 지점. 1FDC9173BC0
Overwatch.exe+200C5C9 - jmp Overwatch.exe+2012766
Overwatch.exe+2012766 - cmp byte ptr [rsi],00       # 플래그 비교
Overwatch.exe+2012769 - lea rdx,[Overwatch.Ordinal8]
Overwatch.exe+2012770 - movss xmm6,[Overwatch.exe+2C94650]  # 7FF7A1D64650 = (float)0.00
Overwatch.exe+2012778 - mov r15,[rbp+00000098]      # 상위 매니저 포인터로 추정. 1FE254182A0
Overwatch.exe+201277F - mov r13,[rsp+48]            # 설명자나 현재 엔티티에 대한 핸들로 추정. 1FE2A625950
Overwatch.exe+2012784 - mov r14,[rbp+00000228]      # 리스트/배열 풀의 베이스 포인터로 추정. 1FDC916BA30
Overwatch.exe+201278B - mov eax,[rbp+00000084]      # 1
Overwatch.exe+2012791 - jne Overwatch.exe+200AEE5
Overwatch.exe+200AEE5 - cmp eax,000F4240            # 앞선 1과 100만비교. 루프가 너무 도는거 아닌지 보호용 카운터.
Overwatch.exe+200AEEA - jnl Overwatch.exe+201279E
Overwatch.exe+200AEF0 - inc eax                     # +1
Overwatch.exe+200AEF2 - mov [rbp+00000084],eax      # 다시 넣기
Overwatch.exe+200AEF8 - movzx eax,byte ptr [rsi]    # 현재 위치의 1바이트 읽기
Overwatch.exe+200AEFB - dec eax
Overwatch.exe+200AEFD - cmp eax,69                  # 범위 내인지 확인
Overwatch.exe+200AF00 - ja Overwatch.exe+20127F2    # 아니면 예외처리
Overwatch.exe+200AF06 - cdqe 
Overwatch.exe+200AF08 - mov ecx,[rdx+rax*4+020128A4]
Overwatch.exe+200AF0F - add rcx,rdx
Overwatch.exe+200AF12 - jmp rcx
Overwatch.exe+200B00F - lea eax,[rbx-01]



2051c5d 단은 다음과 같음.

Overwatch.exe+2051C3C - 8B CF                 - mov ecx,edi                         # 쓰기위해 비워놓기
Overwatch.exe+2051C3E - E8 FDFCFFFF           - call Overwatch.exe+2051940          # 아마 루프 반복 횟수를 rax로 받아와 rdi에 넣기 위한 함수일듯
Overwatch.exe+2051C43 - 8B F8                 - mov edi,eax
Overwatch.exe+2051C45 - 85 C0                 - test eax,eax
Overwatch.exe+2051C47 - 74 1F                 - je Overwatch.exe+2051C68            # 0이면 할일이 없다. 바로 끝으로 스킵.
Overwatch.exe+2051C49 - 0F1F 80 00000000      - nop dword ptr [rax+00000000]
Overwatch.exe+2051C50 - BA 08000000           - mov edx,00000008 { 8 }              # 루프 시작지점. 8비트 즉 1바이트씩 가지고 오겠다
Overwatch.exe+2051C55 - 48 8B CE              - mov rcx,rsi                         # 201FDF0CDD8
Overwatch.exe+2051C58 - E8 230659FE           - call Overwatch.exe+5E2280           # 8(필요비트수)과 201FDF0CDD8을 가지고 값을 받아오기 위해 출발

[ctx+0x18] = 0x348 = 840
[ctx+0x1C] = 0x2AE = 686

패턴이 너무 전형적이야:
total_bits = 840 (전체 비트 수)
consumed_bits = 686 (현재까지 읽은 비트 수)

**이 구조체는 “비트 단위로 읽는 스트림”**이고
+0x18, +0x1C 필드는 “전체 길이 vs 현재 위치” 같은 역할을 하는 중.
[ctx+8]에서 읽어오는 RAX는 그 비트스트림 안의 **64비트 청크(워드)**라고 보면 돼.
RAX=0000004000000000

현재 비트 위치
bit_index = [ctx+0x1C] = 686
bit_offset_in_word = bit_index & 0x3F = 686 % 64 = 0x2E = 46
이 워드에서 아직 읽을 수 있는 비트 수
64 - 46 = 18비트 남음
이번에 요청한 비트 수는 EDX = 8비트
rdi = min(18, 8) = 8
→ 이번 8비트는 한 워드 안에서 다 해결 가능
(다음 워드로 넘어갈 필요 없음)

내부 상태 업데이트(“8비트 소비했다” 2B6으로) 하고,
결과는 비트오프셋으로 쉬프트해서 뽑아내 AL=00에 담아서 리턴.

Overwatch.exe+2051C5D - 88 03                 - mov [rbx],al                        # 버퍼 20330FA5619 에 00 저장
Overwatch.exe+2051C5F - 48 8D 5B 01           - lea rbx,[rbx+01]                    # 주소 +1
Overwatch.exe+2051C63 - 83 C7 FF              - add edi,-01 { 255 }                 # 3이었는데 -1
Overwatch.exe+2051C66 - 75 E8                 - jne Overwatch.exe+2051C50           # 아직 0 안됐으니 루프ㄱ
Overwatch.exe+2051C68 - 8B 46 18              - mov eax,[rsi+18]                    # 그렇게 네번 돌려보내고 빠져나와서 201FDF0CDD8+18 에서 348 꺼내기
Overwatch.exe+2051C6B - 39 46 1C              - cmp [rsi+1C],eax                    # 전체 비트 수와 현재까지 읽은 비트 수 비교
Overwatch.exe+2051C6E - 0F96 C0               - setbe al                            # 작거나 같으면 rax=1
Overwatch.exe+2051C71 - E9 15010000           - jmp Overwatch.exe+2051D8B

서버에서 온 패킷에서 원하는 값을 뽑는 과정으로 보임

call을 타기 전과 후의 레지스터 비교

Overwatch.exe+2051C58 - call Overwatch.exe+5E2280
RAX=0000000000000001
RBX=0000020330FA5619
RCX=00000201FDF0CDD8
RDX=0000000000000008
RSI=00000201FDF0CDD8
RDI=0000000000000003
RBP=00007FF7A2CB3960
RSP=000000E3D3B2EA40
R8=0000000000000026
R9=00000201FDF0CDD8
R10=00000000000002A6
R11=0000000000000348
R12=0000020330FA55A0
R13=0000020330FA5540
R14=0000000000000000
R15=000000E3D3B2EC00
RIP=00007FF7A1121C58

Overwatch.exe+2051C5D - mov [rbx],al
20330FA5619 = (byte)00(0)
RAX=0000000000000000
RBX=0000020330FA5619
RCX=000000000000002E
RDX=000000000000000A
RSI=00000201FDF0CDD8
RDI=0000000000000003
RBP=00007FF7A2CB3960
RSP=000000E3D3B2EA40
R8=000000000000002E
R9=00000201FDF0CDD8
R10=00000000000002AE
R11=0000000000000348
R12=0000020330FA55A0
R13=0000020330FA5540
R14=0000000000000000
R15=000000E3D3B2EC00
RIP=00007FF7A1121C5D