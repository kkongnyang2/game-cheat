### 단계 1 - write 감시

CE로 찾아낸 hp 관련 시드에 write를 잡아내기

에러: 버전 이슈로 vmi_init_complete 인자 호환
해결: grep -n "vmi_init_complete(" /usr/local/include/libvmi/libvmi.h -n9
status_t vmi_init_complete(
    vmi_instance_t *vmi,
    const void *domain,
    uint64_t init_flags,
    vmi_init_data_t *init_data,
    vmi_config_t config_mode,
    void *config,
    vmi_init_error_t *error);
이대로 수정

gcc -O2 -Wall -Wextra -std=gnu11 seed_once_watch.c -o seed_once_watch \
  $(pkg-config --cflags libvmi glib-2.0) \
  $(pkg-config --libs   libvmi glib-2.0) \
  -Wl,--no-as-needed

sudo vmi-process-list win10 | grep -i overwatch

sudo ./seed_once_watch win10 10472 0x233B65D8000-0x233B65D8FFF 5000
=== seed_once_write_watch (PID-only, page-scoped) ===
[*] domain=win10 pid=10472 range=[0x233b65d8000..0x233b65d8fff) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x202452000
[*] Armed write-watch: pages=1 registered=1 range=[0x00000233b65d8000 .. 0x00000233b65d9000)
[*] Watch timeout (5005 ms)
[*] Done. (no write)

sudo ./seed_once_watch win10 10472 0x233B65D8000-0x233B65D8FFF 5000
=== seed_once_write_watch (PID-only, page-scoped) ===
[*] domain=win10 pid=10472 range=[0x233b65d8000..0x233b65d8fff) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x202452000
[*] Armed write-watch: pages=1 registered=1 range=[0x00000233b65d8000 .. 0x00000233b65d9000)
[W] WRITE @0x233b65d8a78 vcpu=6 (seed)
[*] TRACE start (gfn=0x16d4bc view=0)
[*] Done. (seed write detected)

### 단계 2 - 해당 상태 캡쳐

write trap이 들어왔을 때 레지스터 값들 캡처

에러: 메모리 읽기를 추가해야 하는데 버전 이슈로 vmi_read_va 인자 호환 안됨
해결: grep -n "vmi_read_va(" /usr/local/include/libvmi/libvmi.h -n9
1407:status_t vmi_read_va(
1408-    vmi_instance_t vmi,
1409-    addr_t vaddr,
1410-    vmi_pid_t pid,
1411-    size_t count,
1412-    void *buf,
1413-    size_t *bytes_read
1414-) NOEXCEPT;
이대로 수정

에러: 스냅샷을 캡쳐하는데 필요한 애들이 내 버전에선 다른 곳에 정의되어 있음
해결: #include <libvmi/x86.h>   // 일부 버전에서 레지스터 enum이 여기 존재
// ---- libvmi register enum compatibility shim ----
#ifndef VMI_X86_RIP
  #define VMI_X86_RIP    RIP
  #define VMI_X86_RSP    RSP

에러: write가 타이밍 이슈로 권한이 재조정되기 전에 재트랩되며 다음 write가 VMI_ERROR: Caught a memory event that had no handler registered in LibVMI @ GFN 0x16d4bc (0x16d4bca78), access: 6 가 뜨고 게스트 멈춤
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

에러: 버전 이슈로 vmi_set_mem_event 인자 호환 안됨
해결: status_t vmi_set_mem_event(
    vmi_instance_t vmi,
    addr_t gfn,
    vmi_mem_access_t access,
    uint16_t vmm_pagetable_id) NOEXCEPT;
이대로 수정

gcc -O2 -Wall -Wextra -std=gnu11 seed_once_watch_v2.c -o seed_once_watch_v2   $(pkg-config --cflags libvmi glib-2.0)   $(pkg-config --libs   libvmi glib-2.0)   -Wl,--no-as-needed

sudo ./seed_once_watch_v2 win10 12904 0x1A08E5A8000-0x1A08E5A8FFF 5000
=== seed_once_watch_v2 ===
[*] domain=win10 pid=12904 range=[0x1a08e5a8000..0x1a08e5a8fff) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x216a32000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001a08e5a8000 .. 0x000001a08e5a9000)
[W] WRITE trap @0x1a08e5a8a78 (vcpu=7, gfn=0x162062)
{"evt":"seed_write_pre","ts_us":1760082248056611,"vcpu":7,"pid":12904,"gla":"0x1a08e5a8a78","gfn":"0x162062","dtb":"0x216a32000","cr3":"0x216a32000","rip":"0x7ff7f89cbc1f","rsp":"0x79958ff270","rbp":"0x79958ff390","rflags":"0x10206","gpr":{"rax":"0x1a0c46c4601","rbx":"0x1a08e5a8a78","rcx":"0xf7","rdx":"0x1a08e5a8a78","rsi":"0x1a0c46c46a8","rdi":"0x8","r8":"0x79958ff310","r9":"0x79958ff2d0","r10":"0x79958ff2d8","r11":"0x79958ff2f8","r12":"0x19ed5600dc0","r13":"0x19ed5604428","r14":"0x19ed5603e20","r15":"0x0"},"rip_bytes_len":16,"rip_bytes":"890a4989104883c428c3498b08488b44","pre_len":16,"pre_bytes":"e1296a3982060000808a5a8ea0010000","post_len":16,"post_bytes":"fa00000000000000484e27b99e010000","stack_len":64,"stack_bytes":"06000000000000001000b7909e010000a0e63ff0a00100001000b7909e0100000400000000000000a4c29cf8f77f000050000000000000007000000000000000"}
[*] Done. (seed write trapped)

### 단계 3 - rip 명령어 디스어셈블리

해당 값을 변하게 한 명령어인 rip 해석 및 정보

에러: 내 캡스톤 버전에는 seed_once_watch_v3.c:155:62: error: ‘X86_INS_REP_STOSD’ undeclared (first use in this function); did you mean ‘X86_INS_STOSD’? 155 | case X86_INS_REP_STOSB: case X86_INS_REP_STOSW: case X86_INS_REP_STOSD: case X86_INS_REP_STOSQ: | ^~~~~~~~~~~~~~~~~ | X86_INS_STOSD 사용하는 용어가 다름.
해결: 수정

sudo apt-get install -y libcapstone-dev

gcc -O2 -Wall -Wextra -std=gnu11 seed_once_watch_v3.c -o seed_once_watch_v3 \
  $(pkg-config --cflags libvmi glib-2.0) \
  $(pkg-config --libs   libvmi glib-2.0) \
  -lcapstone -Wl,--no-as-needed

sudo ./seed_once_watch_v3_1 win10 12904 0x19EAC718000-0x19EAC718FFF 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=12904 range=[0x19eac718000..0x19eac718fff) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x216a32000
[*] Armed W-trap: pages=1 registered=1 range=[0x0000019eac718000 .. 0x0000019eac719000)
[W] WRITE trap @0x19eac718a78 (vcpu=6, gfn=0x3f3592)
{"evt":"seed_write_pre","ts_us":1760099851372240,"vcpu":6,"pid":12904,"gla":"0x19eac718a78","gfn":"0x3f3592","dtb":"0x216a32000","cr3":"0x216a32000","rip":"0x7ff7f89cbc1f","rsp":"0x79957ff1f0","rbp":"0x79957ff310","rflags":"0x10206","module":{"base":"0x7ff7f6960000","rip_off":"0x206bc1f"},"gpr":{"rax":"0x1a0f6160101","rbx":"0x19eac718a78","rcx":"0xef","rdx":"0x19eac718a78","rsi":"0x1a0f61601f8","rdi":"0x8","r8":"0x79957ff290","r9":"0x79957ff250","r10":"0x79957ff258","r11":"0x79957ff278","r12":"0x19ed5600dc0","r13":"0x19ed5604428","r14":"0x19ed5603e20","r15":"0x0"},"disasm":{"rip":"0x7ff7f89cbc1f","mn":"mov","op":"dword ptr [rdx], ecx","mem_write":true,"wkind":"store","ea_calc":"0x19eac718a78","ea_match":true},"rip_bytes_len":16,"rip_bytes":"890a4989104883c428c3498b08488b44","pre_len":16,"pre_bytes":"e129c6b17a060000808a71ac9e010000","post_len":16,"post_bytes":"fa0000","stack_len":64,"stack_bytes":"78f37f9579000000b07d7af8f77f0000d84260d59e01000005cb9ff8f77f0000e84260d59e010000a4c29cf8f77f0000e01860d59e01000042b99ef8f77f0000"}
[*] Done. (seed write trapped)

이때 모듈 베이스는 overwatch.exe 즉 0x7FF7F6960000이고 이 안에도 ordinal 여러개와 NVSDK_NGX_D3D11_AllocateParameters 등이 있는데 해당 rip은 ordinal 마지막 즈음에 속해있다. 그리고 overwatch.exe 다음에 나오는 모듈은 ntdll.dll 즉 0x7FF9BBEB0000이다.

### 단계 5 - 두번 write

두번 write 잡는 코드


### 단계 6

두번 write, in_access를 rwx다 해서 w 없으면 콜백에서 무효처리, 깔끔한 clear


### 단계 7

한번 write, in_access는 w만, 깔끔한 clear


* 여기서 작성한 코드는 project4에 저장되어 있음.