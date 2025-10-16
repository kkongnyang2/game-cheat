### libvmi로 마이크로트레이스

Libvmi에서 게스트 특정 프로세스 특정 시드에 어떻게 데이터가 적히는지 써지기까지의 포인터 복호화를 포함한 근원 과정을 찾아낼 수 있는 방법.

### 단계 1

write하면 디버깅 메시지

sudo vmi-process-list win10 | grep -i overwatch

gcc -O2 -Wall -Wextra -std=gnu11 seed_once_watch.c -o seed_once_watch \
  $(pkg-config --cflags libvmi glib-2.0) \
  $(pkg-config --libs   libvmi glib-2.0) \
  -Wl,--no-as-needed

kkongnyang2@acer:~/project4$ sudo ./seed_once_watch win10 10472 0x233B65D8000-0x233B65D8FFF 5000
=== seed_once_write_watch (PID-only, page-scoped) ===
[*] domain=win10 pid=10472 range=[0x233b65d8000..0x233b65d8fff) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x202452000
[*] Armed write-watch: pages=1 registered=1 range=[0x00000233b65d8000 .. 0x00000233b65d9000)
[*] Watch timeout (5005 ms)
[*] Done. (no write)
kkongnyang2@acer:~/project4$ sudo ./seed_once_watch win10 10472 0x233B65D8000-0x233B65D8FFF 5000
=== seed_once_write_watch (PID-only, page-scoped) ===
[*] domain=win10 pid=10472 range=[0x233b65d8000..0x233b65d8fff) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x202452000
[*] Armed write-watch: pages=1 registered=1 range=[0x00000233b65d8000 .. 0x00000233b65d9000)
[W] WRITE @0x233b65d8a78 vcpu=6 (seed)
[*] TRACE start (gfn=0x16d4bc view=0)
[*] Done. (seed write detected)

### 단계 2

현 상태 캡쳐

문제: 스냅샷을 캡쳐하는데 필요한 애들이 내 버전에선 다른 곳에 정의되어 있음
해결: #include <libvmi/x86.h> // 일부 버전에서 레지스터 enum이 여기 존재 // ---- libvmi register enum compatibility shim ---- #ifndef VMI_X86_RIP #define VMI_X86_RIP RIP #define VMI_X86_RSP RSP

문제: write가 타이밍 이슈로 권한이 재조정되기 전에 재트랩되며 다음 write가 VMI_ERROR: Caught a memory event that had no handler registered in LibVMI @ GFN 0x16d4bc (0x16d4bca78), access: 6 가 뜨고 게스트 멈춤
해결: 콜백 부분을 vmi_set_mem_event로 권한 재조정이 아닌 즉시 트랩 풀어버리기
static event_response_t on_seed_write(vmi_instance_t vmi, vmi_event_t *event){
    const addr_t gla = event->mem_event.gla;
    if(!(gla >= G.va_lo && gla < G.va_hi)) return 0;

    if(!G.triggered){
        G.triggered = 1;

        // 1) 먼저 이 GFN의 W-트랩을 즉시 해제 (EPT 권한 복구를 즉시 적용)
        vmi_set_mem_event(vmi, event->mem_event.gfn, VMI_MEMACCESS_N, 0);

        // 2) 디버깅 메시지 & 스냅샷
        fprintf(stderr,
            "[W] WRITE trap @0x%llx (vcpu=%u, gfn=0x%llx)\n",
            (unsigned long long)gla, event->vcpu_id,
            (unsigned long long)event->mem_event.gfn);

        collect_and_print_snapshot(vmi, event);

        // 3) 종료 신호 — 메인루프에서 vmi_clear_event/vmi_destroy
        g_stop = 1;
    }
    return 0; // 별도 SET_ACCESS 반환값 필요 없는 버전
}

kkongnyang2@acer:~/project4$ gcc -O2 -Wall -Wextra -std=gnu11 seed_once_watch_v2.c -o seed_once_watch_v2   $(pkg-config --cflags libvmi glib-2.0)   $(pkg-config --libs   libvmi glib-2.0)   -Wl,--no-as-needed
kkongnyang2@acer:~/project4$ sudo vmi-process-list win10 | grep -i overwatch
[12904] Overwatch.exe (struct addr:ffffc3030022a300)
kkongnyang2@acer:~/project4$ sudo ./seed_once_watch_v2 win10 12904 0x1A08E5A8000-0x1A08E5A8FFF 5000
=== seed_once_watch_v2 ===
[*] domain=win10 pid=12904 range=[0x1a08e5a8000..0x1a08e5a8fff) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x216a32000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001a08e5a8000 .. 0x000001a08e5a9000)
[W] WRITE trap @0x1a08e5a8a78 (vcpu=7, gfn=0x162062)
{"evt":"seed_write_pre","ts_us":1760082248056611,"vcpu":7,"pid":12904,"gla":"0x1a08e5a8a78","gfn":"0x162062","dtb":"0x216a32000","cr3":"0x216a32000","rip":"0x7ff7f89cbc1f","rsp":"0x79958ff270","rbp":"0x79958ff390","rflags":"0x10206","gpr":{"rax":"0x1a0c46c4601","rbx":"0x1a08e5a8a78","rcx":"0xf7","rdx":"0x1a08e5a8a78","rsi":"0x1a0c46c46a8","rdi":"0x8","r8":"0x79958ff310","r9":"0x79958ff2d0","r10":"0x79958ff2d8","r11":"0x79958ff2f8","r12":"0x19ed5600dc0","r13":"0x19ed5604428","r14":"0x19ed5603e20","r15":"0x0"},"rip_bytes_len":16,"rip_bytes":"890a4989104883c428c3498b08488b44","pre_len":16,"pre_bytes":"e1296a3982060000808a5a8ea0010000","post_len":16,"post_bytes":"fa00000000000000484e27b99e010000","stack_len":64,"stack_bytes":"06000000000000001000b7909e010000a0e63ff0a00100001000b7909e0100000400000000000000a4c29cf8f77f000050000000000000007000000000000000"}
[*] Done. (seed write trapped)

### 단계 3

무엇이 썼냐 (명령어 디스어셈블)

sudo apt-get install -y libcapstone-dev

gcc -O2 -Wall -Wextra -std=gnu11 seed_once_watch_v3.c -o seed_once_watch_v3 \
  $(pkg-config --cflags libvmi glib-2.0) \
  $(pkg-config --libs   libvmi glib-2.0) \
  -lcapstone -Wl,--no-as-needed

문제: 내 캡스톤 버전에는 seed_once_watch_v3.c:155:62: error: ‘X86_INS_REP_STOSD’ undeclared (first use in this function); did you mean ‘X86_INS_STOSD’? 155 | case X86_INS_REP_STOSB: case X86_INS_REP_STOSW: case X86_INS_REP_STOSD: case X86_INS_REP_STOSQ: | ^~~~~~~~~~~~~~~~~ | X86_INS_STOSD 사용하는 용어가 다름.
해결: 수정

kkongnyang2@acer:~/project4$ sudo ./seed_once_watch_v3 win10 12904 0x1A08E5A8000-0x1A08E5A8FFF 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=12904 range=[0x1a08e5a8000..0x1a08e5a8fff) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x216a32000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001a08e5a8000 .. 0x000001a08e5a9000)
[W] WRITE trap @0x1a08e5a8a78 (vcpu=1, gfn=0x162062)
{"evt":"seed_write_pre","ts_us":1760085770384536,"vcpu":1,"pid":12904,"gla":"0x1a08e5a8a78","gfn":"0x162062","dtb":"0x216a32000","cr3":"0x216a32000","rip":"0x7ff7f89cbc1f","rsp":"0x79959ff490","rbp":"0x79959ff5b0","rflags":"0x10206","gpr":{"rax":"0x1a0c46c4601","rbx":"0x1a08e5a8a78","rcx":"0xf8","rdx":"0x1a08e5a8a78","rsi":"0x1a0c46c46a8","rdi":"0x8","r8":"0x79959ff530","r9":"0x79959ff4f0","r10":"0x79959ff4f8","r11":"0x79959ff518","r12":"0x19ed5600dc0","r13":"0x19ed5604428","r14":"0x19ed5603e20","r15":"0x0"},"disasm":{"rip":"0x7ff7f89cbc1f","mn":"mov","op":"dword ptr [rdx], ecx","mem_write":true,"wkind":"store","ea_calc":"0x1a08e5a8a78","ea_match":true},"rip_bytes_len":16,"rip_bytes":"890a4989104883c428c3498b08488b44","pre_len":16,"pre_bytes":"e1296a3982060000808a5a8ea0010000","post_len":16,"post_bytes":"fa00000000000000484e27b99e010000","stack_len":64,"stack_bytes":"18f69f9579000000b07d7af8f77f0000d84260d59e01000005cb9ff8f77f0000e84260d59e010000a4c29cf8f77f0000e01860d59e01000042b99ef8f77f0000"}
[*] Done. (seed write trapped)


현재 RIP=0x7ff7f89cbc1f의 명령어는 mov dword ptr [rdx], ecx.
→ 즉, ECX(32비트) 값을 [RDX] 주소에 저장하는 직접 저장(store) 명령이야(복사/스트링 아님).

Capstone이 해석한 EA(유효주소) = [RDX], 우리가 트랩한 GLA와 완전히 일치(ea_match:true).
즉, RDX가 가리키는 그 주소가 이번 HP 슬롯.

피연산자 값
"rcx":"0xf8",  "rdx":"0x1a08e5a8a78",
"rbx":"0x1a08e5a8a78" (동일 주소를 RBX도 가리킴)
이 순간 ECX=0xF8 → 저장될 32비트 값은 0x000000F8 (리틀엔디안으로 메모리에 f8 00 00 00).
RDX=0x…a78 → 쓰기 대상 주소.
흥미롭게도 RBX도 같은 주소를 들고 있음(컴파일러가 두 레지스터에 같은 포인터를 보존한 상황으로 보임).
EA는 RDX 기반이므로 근원 포인터는 RDX가 맞고, RBX는 보조/캐시일 가능성.

만약 RIP 디스어셈블이 store이다 -> mov [base + index*scale + disp], src
EA = base + index*scale + disp 를 계산해 GLA와 일치하는지 확인하여 어느 레지스터가 포인터인지 확정. src가 뭔지를 보고 값의 근원 후보 좁히기
만약 RIP 디스어셈블이 memcpy이다 -> RSI/RCX/RDI (원본/카운트/목적지). 원본이 어디서 왔느냐 찾기


### 단계 3-1

해당 모듈 베이스와 오프셋 추가

kkongnyang2@acer:~/project4$ sudo ./seed_once_watch_v3_1 win10 12904 0x19EAC718000-0x19EAC718FFF 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=12904 range=[0x19eac718000..0x19eac718fff) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x216a32000
[*] Armed W-trap: pages=1 registered=1 range=[0x0000019eac718000 .. 0x0000019eac719000)
[W] WRITE trap @0x19eac718a78 (vcpu=6, gfn=0x3f3592)
{"evt":"seed_write_pre","ts_us":1760099851372240,"vcpu":6,"pid":12904,"gla":"0x19eac718a78","gfn":"0x3f3592","dtb":"0x216a32000","cr3":"0x216a32000","rip":"0x7ff7f89cbc1f","rsp":"0x79957ff1f0","rbp":"0x79957ff310","rflags":"0x10206","module":{"base":"0x7ff7f6960000","rip_off":"0x206bc1f"},"gpr":{"rax":"0x1a0f6160101","rbx":"0x19eac718a78","rcx":"0xef","rdx":"0x19eac718a78","rsi":"0x1a0f61601f8","rdi":"0x8","r8":"0x79957ff290","r9":"0x79957ff250","r10":"0x79957ff258","r11":"0x79957ff278","r12":"0x19ed5600dc0","r13":"0x19ed5604428","r14":"0x19ed5603e20","r15":"0x0"},"disasm":{"rip":"0x7ff7f89cbc1f","mn":"mov","op":"dword ptr [rdx], ecx","mem_write":true,"wkind":"store","ea_calc":"0x19eac718a78","ea_match":true},"rip_bytes_len":16,"rip_bytes":"890a4989104883c428c3498b08488b44","pre_len":16,"pre_bytes":"e129c6b17a060000808a71ac9e010000","post_len":16,"post_bytes":"fa0000","stack_len":64,"stack_bytes":"78f37f9579000000b07d7af8f77f0000d84260d59e01000005cb9ff8f77f0000e84260d59e010000a4c29cf8f77f0000e01860d59e01000042b99ef8f77f0000"}
[*] Done. (seed write trapped)

이때 모듈 베이스는 overwatch.exe 즉 0x7FF7F6960000이고 이 안에서도 ordinal 여러개와 NVSDK_NGX_D3D11_AllocateParameters 등이 있는데 해당 rip은 ordinal 마지막 즈음에 속해있다. 그리고 overwatch.exe 다음에 나오는 모듈은 ntdll.dll 즉 0x7FF9BBEB0000이다.

### 단계 4

rdx와 ecx가 어디서 등장한지

gcc -O2 -Wall -Wextra -std=gnu11 seed_once_watch_v4.c -o seed_once_watch_v4 \
  $(pkg-config --cflags libvmi glib-2.0) \
  $(pkg-config --libs   libvmi glib-2.0) \
  -lcapstone -Wl,--no-as-needed


        첫번째          두번째              세번째             네번째            다섯번째
rip:    cbc1f          cbc1f              cbc1f            cbc1f
start:  cb91f          cb9af              cac1f            cac1f
ctx:  cba42-cba71    cba42-cba71        cadab-cae74      cadab-cae74
rcx:    cb927          cba4a              null             null
rdx:    cb922          cba2b              null             null
변경점               마지막 정의로 바꾸기   윈도우 범위넓히기      rip직전을열심히읽기    역방향


kkongnyang2@acer:~/project4$ sudo ./seed_once_watch_v4 win10 3520  0x26F19038A78-0x26F19038A79 5000
=== seed_once_watch_v4_3 ===
[*] domain=win10 pid=3520 range=[0x26f19038a78..0x26f19038a79) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x1f5ba8000
[*] Armed W-trap: pages=1 registered=1 range=[0x0000026f19038000 .. 0x0000026f19039000)
[W] WRITE trap @0x26f19038a78 (vcpu=7, gfn=0x7e29c)
{"evt":"seed_write_pre","ts_us":1760582387458360,"vcpu":7,"pid":3520,"gla":"0x26f19038a78","gfn":"0x7e29c","dtb":"0x1f5ba8000","cr3":"0x1f5ba8000","rip":"0x7ff74c264fef","rsp":"0xcd059feeb0","rbp":"0xcd059fefd0","rflags":"0x10206","module":{"base":"0x7ff74a1f0000","rip_off":"0x2074fef"},"gpr":{"rax":"0x26f388ef701","rbx":"0x26f19038a78","rcx":"0xe6","rdx":"0x26f19038a78","rsi":"0x26f388ef7b8","rdi":"0x8","r8":"0xcd059fef50","r9":"0xcd059fef10","r10":"0xcd059fef18","r11":"0xcd059fef38","r12":"0x26d69570dc0","r13":"0x26d69574460","r14":"0x26d69573e50","r15":"0x0"},"disasm":{"rip":"0x7ff74c264fef","mn":"mov","op":"dword ptr [rdx], ecx","mem_write":true,"wkind":"store","ea_calc":"0x26f19038a78","ea_match":true},"rip_bytes_len":16,"rip_bytes":"890a4989104883c428c3498b08488b44","pre_len":16,"pre_bytes":"e1290e64bc090000808a03196f020000","post_len":16,"post_bytes":"fa00000000000000b0b5cf4d6d020000","stack_len":64,"stack_bytes":"0600000000000000100003206d020000b03fa7256f020000100003206d02000004000000000000007456264cf77f000050000000000000007000000000000000","origin":{"rcx":{"addr":"0x7ff74c264e1a","mn":"mov","op":"ecx, dword ptr [rbx + 8]","kind":"mov"},"rdx":{"addr":"0x7ff74c264dfb","mn":"mov","op":"edx, 0xc","kind":"mov"}},"backtrace":{"start":"0x7ff74c264cef","ctx":[{"a":"0x7ff74c264daa","mn":"mov","op":"rbx, qword ptr [rax]"},{"a":"0x7ff74c264dad","mn":"call","op":"0x7ff74c1efae0"},{"a":"0x7ff74c264db2","mn":"mov","op":"qword ptr [rbx], rax"},{"a":"0x7ff74c264db5","mn":"mov","op":"al, 1"},{"a":"0x7ff74c264db7","mn":"mov","op":"rbx, qword ptr [rsp + 0x30]"},{"a":"0x7ff74c264dbc","mn":"add","op":"rsp, 0x20"},{"a":"0x7ff74c264dc0","mn":"pop","op":"rdi"},{"a":"0x7ff74c264dc1","mn":"ret","op":""},{"a":"0x7ff74c264dc2","mn":"mov","op":"rax, qword ptr [rsp + 0x50]"},{"a":"0x7ff74c264dc7","mn":"mov","op":"edx, 8"},{"a":"0x7ff74c264dcc","mn":"mov","op":"rcx, qword ptr [r10]"},{"a":"0x7ff74c264dcf","mn":"mov","op":"rbx, qword ptr [r9]"},{"a":"0x7ff74c264dd2","mn":"mov","op":"rdi, qword ptr [rax]"},{"a":"0x7ff74c264dd5","mn":"lea","op":"r8d, [rdx - 4]"},{"a":"0x7ff74c264dd9","mn":"call","op":"0x7ff74c1efbb0"},{"a":"0x7ff74c264dde","mn":"movsd","op":"xmm0, qword ptr [rbx]"},{"a":"0x7ff74c264de2","mn":"movsd","op":"qword ptr [rax], xmm0"},{"a":"0x7ff74c264de6","mn":"mov","op":"qword ptr [rdi], rax"},{"a":"0x7ff74c264de9","mn":"mov","op":"al, 1"},{"a":"0x7ff74c264deb","mn":"mov","op":"rbx, qword ptr [rsp + 0x30]"},{"a":"0x7ff74c264df0","mn":"add","op":"rsp, 0x20"},{"a":"0x7ff74c264df4","mn":"pop","op":"rdi"},{"a":"0x7ff74c264df5","mn":"ret","op":""},{"a":"0x7ff74c264df6","mn":"mov","op":"rax, qword ptr [rsp + 0x50]"},{"a":"0x7ff74c264dfb","mn":"mov","op":"edx, 0xc"},{"a":"0x7ff74c264e00","mn":"mov","op":"rcx, qword ptr [r10]"},{"a":"0x7ff74c264e03","mn":"mov","op":"rbx, qword ptr [r9]"},{"a":"0x7ff74c264e06","mn":"mov","op":"rdi, qword ptr [rax]"},{"a":"0x7ff74c264e09","mn":"lea","op":"r8d, [rdx - 8]"},{"a":"0x7ff74c264e0d","mn":"call","op":"0x7ff74c1efbb0"},{"a":"0x7ff74c264e12","mn":"movsd","op":"xmm0, qword ptr [rbx]"},{"a":"0x7ff74c264e16","mn":"movsd","op":"qword ptr [rax], xmm0"},{"a":"0x7ff74c264e1a","mn":"mov","op":"ecx, dword ptr [rbx + 8]"},{"a":"0x7ff74c264e1d","mn":"mov","op":"dword ptr [rax + 8], ecx"},{"a":"0x7ff74c264e20","mn":"mov","op":"qword ptr [rdi], rax"},{"a":"0x7ff74c264e23","mn":"mov","op":"al, 1"},{"a":"0x7ff74c264e25","mn":"mov","op":"rbx, qword ptr [rsp + 0x30]"},{"a":"0x7ff74c264e2a","mn":"add","op":"rsp, 0x20"},{"a":"0x7ff74c264e2e","mn":"pop","op":"rdi"},{"a":"0x7ff74c264e2f","mn":"ret","op":""},{"a":"0x7ff74c264e30","mn":"mov","op":"rbx, qword ptr [rsp + 0x30]"},{"a":"0x7ff74c264e35","mn":"xor","op":"al, al"},{"a":"0x7ff74c264e37","mn":"add","op":"rsp, 0x20"},{"a":"0x7ff74c264e3b","mn":"pop","op":"rdi"},{"a":"0x7ff74c264e3c","mn":"ret","op":""},{"a":"0x7ff74c264e3d","mn":"nop","op":"dword ptr [rax]"},{"a":"0x7ff74c264e40","mn":"sub","op":"byte ptr [rdi + rax + 2], cl"},{"a":"0x7ff74c264e44","mn":"pop","op":"rax"}]},"_":"_"}
[*] Done. (seed write trapped)


###

~/project-mem$ make clean
~/project-mem$ make
~/project-mem$ sudo ./libvmi_heap_write_tracer win10 3284 0x2355EE65000-0x2355EE65999 5000 20000 3000 trace.jsonl
=== libvmi_heap_write_tracer (PID-only, page-scoped) ===
[*] domain=win10 pid=3284 range=[0x000002355ee65000 .. 0x000002355ee65999) watch_timeout_ms=5000 max_steps=20000 max_trace_ms=3000 out=trace.jsonl
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x2e6662000
[+] SINGLESTEP handler registered: cb=0x5bd68a48cdb0
[*] Armed write-watch: pages=1 registered=1 range=[0x000002355ee65000 .. 0x000002355ee66000)
[W] WRITE @0x2355ee65078 vcpu=0 (seed)
[*] TRACE start t0=1759338247448419 us (vcpu=0, gfn=0x82ed view=0)
[*] single-step ENABLED on vcpu=0 (mask=0x1)
[!] re-arm W trap failed gfn=0x82ed view=0
[*] TRACE stop by limit: steps=20000, elapsed_ms=457
[*] single-step DISABLED on vcpu=0
VMI_ERROR: process_singlestep error: no singlestep handler is registered in LibVMI
[*] Done. Output: trace.jsonl
결과는 다음과 같고, trace.jsonl도 첨부해줄게.


### 자주 발생하는 문제

libvmi에 
Capstone엔 X86_REG_RFLAGS가 없고 X86_REG_EFLAGS가 있음

status_t vmi_init_complete(
    vmi_instance_t *vmi,
    const void *domain,
    uint64_t init_flags,
    vmi_init_data_t *init_data,
    vmi_config_t config_mode,
    void *config,
    vmi_init_error_t *error);
kkongnyang2@acer:~/project4$ grep -n "vmi_read_va(" /usr/local/include/libvmi/libvmi.h -n9
1407:status_t vmi_read_va(
1408-    vmi_instance_t vmi,
1409-    addr_t vaddr,
1410-    vmi_pid_t pid,
1411-    size_t count,
1412-    void *buf,
1413-    size_t *bytes_read
1414-) NOEXCEPT;

kkongnyang2@acer:~/project4$ grep -n "vmi_read("    /usr/local/include/libvmi/libvmi.h -n9
1244:status_t vmi_read(
1245-    vmi_instance_t vmi,
1246-    const access_context_t *ctx,
1247-    size_t count,
1248-    void *buf,
1249-    size_t *bytes_read) NOEXCEPT;
status_t vmi_set_mem_event(
    vmi_instance_t vmi,
    addr_t gfn,
    vmi_mem_access_t access,
    uint16_t vmm_pagetable_id) NOEXCEPT;


프로파일에서 PsActiveProcessHead를 offset으로 찾으려다 실패.
PsActiveProcessHead는 커널 심볼(주소)이지 구조체 오프셋이 아니어서 vmi_giet_offset("PsActiveProcessHead")로는 못 가져오는 프로파일이 많음.
vmi_translate_ksym2v("PsActiveProcessHead")로 심볼 주소를 해석해야 함.



kkongnyang2@acer:~$ sudo vmi-win-guid name win10
Windows Kernel found @ 0x2c00000
	Version: 64-bit Windows 10
	PE GUID: 39b665671046000
	PDB GUID: f388d1c459c2dc9e81f891d0760a56cd1
	Kernel filename: ntkrnlmp.pdb
	Multi-processor without PAE
	Signature: 17744.
	Machine: 34404.
	# of sections: 33.
	# of symbols: 0.
	Timestamp: 968254823.
	Characteristics: 34.
	Optional header size: 240.
	Optional header type: 0x20b
	Section 1: .rdata
	Section 2: .pdata
	Section 3: .idata
	Section 4: .edata
	Section 5: PROTDATA
