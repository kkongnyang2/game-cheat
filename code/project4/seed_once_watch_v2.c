// seed_once_watch_v2.c — once watch + pre-write snapshot (minimal diff)
// - 메모리 읽기: vmi_read_va(vmi, va, pid, count, buf, &got)
// - VLA 초기화 제거(고정 크기 배열 사용)
// - 권한 out_access 복구는 하지 않음 (vmi_destroy가 정리)

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <inttypes.h>

#include <libvmi/libvmi.h>
#include <libvmi/events.h>
#include <libvmi/x86.h>   // 일부 버전에서 레지스터 enum이 여기 존재

// ---- libvmi register enum compatibility shim ----
#ifndef VMI_X86_RIP
  #define VMI_X86_RIP    RIP
  #define VMI_X86_RSP    RSP
  #define VMI_X86_RBP    RBP
  #define VMI_X86_RFLAGS RFLAGS
  #define VMI_X86_RAX    RAX
  #define VMI_X86_RBX    RBX
  #define VMI_X86_RCX    RCX
  #define VMI_X86_RDX    RDX
  #define VMI_X86_RSI    RSI
  #define VMI_X86_RDI    RDI
  #define VMI_X86_R8     R8
  #define VMI_X86_R9     R9
  #define VMI_X86_R10    R10
  #define VMI_X86_R11    R11
  #define VMI_X86_R12    R12
  #define VMI_X86_R13    R13
  #define VMI_X86_R14    R14
  #define VMI_X86_R15    R15
  #define VMI_X86_CR3    CR3
#endif
// -------------------------------------------------

typedef struct {
    char     domain[128];
    uint64_t pid;
    uint64_t va_lo, va_hi;
    uint64_t watch_timeout_ms;

    vmi_instance_t vmi;
    addr_t  dtb;

    int      triggered;
} Ctx;

static Ctx G;
static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int sig){ (void)sig; g_stop=1; }

static uint64_t now_us(void){
    struct timespec ts; clock_gettime(CLOCK_REALTIME,&ts);
    return (uint64_t)ts.tv_sec*1000000ULL + (uint64_t)ts.tv_nsec/1000ULL;
}

static bool va_to_gfn(vmi_instance_t vmi, addr_t dtb, addr_t va, addr_t* out_gfn){
    addr_t pa=0; if(VMI_FAILURE==vmi_pagetable_lookup(vmi, dtb, va, &pa)) return false;
    *out_gfn = pa>>12; return true;
}

// --- [수정] 안전한 VA 읽기: 네 헤더 시그니처에 맞춰 PID 기반 vmi_read_va 사용 ---
static size_t safe_read_va(vmi_instance_t vmi, vmi_pid_t pid, addr_t va, void* buf, size_t len){
    size_t got = 0;
    if (VMI_SUCCESS == vmi_read_va(vmi, va, pid, len, buf, &got))
        return got;
    return 0;
}

static void hex_dump(const uint8_t* b, size_t n, char* out, size_t outsz){
    size_t pos=0;
    for(size_t i=0;i<n;i++){
        if(pos+2 >= outsz) break;
        pos += (size_t)snprintf(out+pos, outsz-pos, "%02x", b[i]);
    }
    if(pos<outsz) out[pos]='\0';
}

// --- [추가] 첫 트랩 직전 스냅샷 수집/출력 ---
static void collect_and_print_snapshot(vmi_instance_t vmi, vmi_event_t* event){
    uint64_t ts = now_us();
    const uint32_t vcpu = event->vcpu_id;
    const addr_t gla   = event->mem_event.gla;
    const addr_t gfn   = event->mem_event.gfn;

    // 레지스터 스냅샷
    uint64_t rip=0,rsp=0,rbp=0,rflags=0;
    uint64_t rax=0,rbx=0,rcx=0,rdx=0,rsi=0,rdi=0;
    uint64_t r8=0,r9=0,r10=0,r11=0,r12=0,r13=0,r14=0,r15=0;
    uint64_t cr3=0;

    vmi_get_vcpureg(vmi, &rip,   VMI_X86_RIP,   vcpu);
    vmi_get_vcpureg(vmi, &rsp,   VMI_X86_RSP,   vcpu);
    vmi_get_vcpureg(vmi, &rbp,   VMI_X86_RBP,   vcpu);
    vmi_get_vcpureg(vmi, &rflags,VMI_X86_RFLAGS,vcpu);

    vmi_get_vcpureg(vmi, &rax, VMI_X86_RAX, vcpu);
    vmi_get_vcpureg(vmi, &rbx, VMI_X86_RBX, vcpu);
    vmi_get_vcpureg(vmi, &rcx, VMI_X86_RCX, vcpu);
    vmi_get_vcpureg(vmi, &rdx, VMI_X86_RDX, vcpu);
    vmi_get_vcpureg(vmi, &rsi, VMI_X86_RSI, vcpu);
    vmi_get_vcpureg(vmi, &rdi, VMI_X86_RDI, vcpu);
    vmi_get_vcpureg(vmi, &r8,  VMI_X86_R8,  vcpu);
    vmi_get_vcpureg(vmi, &r9,  VMI_X86_R9,  vcpu);
    vmi_get_vcpureg(vmi, &r10, VMI_X86_R10, vcpu);
    vmi_get_vcpureg(vmi, &r11, VMI_X86_R11, vcpu);
    vmi_get_vcpureg(vmi, &r12, VMI_X86_R12, vcpu);
    vmi_get_vcpureg(vmi, &r13, VMI_X86_R13, vcpu);
    vmi_get_vcpureg(vmi, &r14, VMI_X86_R14, vcpu);
    vmi_get_vcpureg(vmi, &r15, VMI_X86_R15, vcpu);
    vmi_get_vcpureg(vmi, &cr3, VMI_X86_CR3, vcpu);

    // RIP 부근 명령어 바이트 (최대 16B)
    uint8_t rip_bytes[16]; memset(rip_bytes, 0, sizeof(rip_bytes));
    size_t rip_n = 0;
    if(rip) rip_n = safe_read_va(vmi, (vmi_pid_t)G.pid, (addr_t)rip, rip_bytes, sizeof(rip_bytes));

    // 대상 주소 주변 (pre:16B, post:16B) — VLA 금지, 고정 크기 사용
    enum { PREN = 16, POSTN = 16 };
    uint8_t pre[PREN];  memset(pre,  0, sizeof(pre));
    uint8_t post[POSTN]; memset(post, 0, sizeof(post));
    size_t pre_got=0, post_got=0;
    if(gla && gla>=PREN) pre_got  = safe_read_va(vmi, (vmi_pid_t)G.pid, gla-PREN, pre,  sizeof(pre));
    if(gla)              post_got = safe_read_va(vmi, (vmi_pid_t)G.pid, gla,      post, sizeof(post));

    // 스택 상단 64B
    uint8_t stk[64]; memset(stk, 0, sizeof(stk));
    size_t stk_got=0;
    if(rsp) stk_got = safe_read_va(vmi, (vmi_pid_t)G.pid, (addr_t)rsp, stk, sizeof(stk));

    // JSON 한 줄
    fprintf(stderr, "{");
    fprintf(stderr, "\"evt\":\"seed_write_pre\",\"ts_us\":%" PRIu64 ",\"vcpu\":%u,", ts, vcpu);
    fprintf(stderr, "\"pid\":%llu,", (unsigned long long)G.pid);

    fprintf(stderr, "\"gla\":\"0x%llx\",\"gfn\":\"0x%llx\",",
            (unsigned long long)gla, (unsigned long long)gfn);
    fprintf(stderr, "\"dtb\":\"0x%llx\",\"cr3\":\"0x%llx\",",
            (unsigned long long)G.dtb, (unsigned long long)cr3);

    fprintf(stderr, "\"rip\":\"0x%llx\",\"rsp\":\"0x%llx\",\"rbp\":\"0x%llx\",\"rflags\":\"0x%llx\",",
            (unsigned long long)rip,(unsigned long long)rsp,(unsigned long long)rbp,(unsigned long long)rflags);

    fprintf(stderr, "\"gpr\":{");
    fprintf(stderr, "\"rax\":\"0x%llx\",\"rbx\":\"0x%llx\",\"rcx\":\"0x%llx\",\"rdx\":\"0x%llx\",",
            (unsigned long long)rax,(unsigned long long)rbx,(unsigned long long)rcx,(unsigned long long)rdx);
    fprintf(stderr, "\"rsi\":\"0x%llx\",\"rdi\":\"0x%llx\",",
            (unsigned long long)rsi,(unsigned long long)rdi);
    fprintf(stderr, "\"r8\":\"0x%llx\",\"r9\":\"0x%llx\",\"r10\":\"0x%llx\",\"r11\":\"0x%llx\",",
            (unsigned long long)r8,(unsigned long long)r9,(unsigned long long)r10,(unsigned long long)r11);
    fprintf(stderr, "\"r12\":\"0x%llx\",\"r13\":\"0x%llx\",\"r14\":\"0x%llx\",\"r15\":\"0x%llx\"",
            (unsigned long long)r12,(unsigned long long)r13,(unsigned long long)r14,(unsigned long long)r15);
    fprintf(stderr, "},");

    { char h[256]; hex_dump(rip_bytes, rip_n, h, sizeof(h));
      fprintf(stderr, "\"rip_bytes_len\":%zu,\"rip_bytes\":\"%s\",", rip_n, h); }

    { char h[256]; hex_dump(pre, pre_got, h, sizeof(h));
      fprintf(stderr, "\"pre_len\":%zu,\"pre_bytes\":\"%s\",", pre_got, h); }

    { char h[512]; hex_dump(post, post_got, h, sizeof(h));
      fprintf(stderr, "\"post_len\":%zu,\"post_bytes\":\"%s\",", post_got, h); }

    { char h[1024]; hex_dump(stk, stk_got, h, sizeof(h));
      fprintf(stderr, "\"stack_len\":%zu,\"stack_bytes\":\"%s\"", stk_got, h); }

    fprintf(stderr, "}\n");
}

static event_response_t on_seed_write(vmi_instance_t vmi, vmi_event_t *event){
    const addr_t gla = event->mem_event.gla;
    if(!(gla >= G.va_lo && gla < G.va_hi)) return 0;

    if(!G.triggered){
        G.triggered = 1;

        vmi_set_mem_event(vmi, event->mem_event.gfn, VMI_MEMACCESS_N, 0);

        fprintf(stderr,
            "[W] WRITE trap @0x%llx (vcpu=%u, gfn=0x%llx)\n",
            (unsigned long long)gla, event->vcpu_id,
            (unsigned long long)event->mem_event.gfn);

        collect_and_print_snapshot(vmi, event);

        g_stop = 1; // out_access 복구는 하지 않음
    }
    return 0;
}

static int arm_write_watch(vmi_instance_t vmi, vmi_event_t* ev_out){
    memset(ev_out, 0, sizeof(*ev_out));
    ev_out->version  = VMI_EVENTS_VERSION;
    ev_out->type     = VMI_EVENT_MEMORY;
    ev_out->callback = on_seed_write;
    ev_out->mem_event.in_access = VMI_MEMACCESS_W;

    uint64_t page_lo = G.va_lo & ~0xFFFULL;
    uint64_t page_hi = (G.va_hi + 0xFFFULL) & ~0xFFFULL;

    size_t pages=0, ok=0;
    for(uint64_t va=page_lo; va<page_hi; va+=0x1000ULL){
        addr_t gfn=0;
        if(!va_to_gfn(vmi, G.dtb, (addr_t)va, &gfn)){
            fprintf(stderr,"[!] VA->GFN fail for 0x%016llx\n",(unsigned long long)va);
            pages++; continue;
        }
        ev_out->mem_event.gfn = gfn;
        if(VMI_FAILURE==vmi_register_event(vmi, ev_out)){
            fprintf(stderr,"[!] register MEMORY gfn=0x%llx fail\n",(unsigned long long)gfn);
        }else ok++;
        pages++;
    }

    fprintf(stderr,"[*] Armed W-trap: pages=%zu registered=%zu range=[0x%016llx .. 0x%016llx)\n",
            pages, ok, (unsigned long long)page_lo, (unsigned long long)page_hi);
    return ok>0 ? 0 : -1;
}

static void usage(const char* p){
    fprintf(stderr,
        "Usage: sudo %s <domain> <pid> <va_start-va_end> [watch_timeout_ms]\n",
        p);
}

int main(int argc, char** argv){
    if(argc<4){ usage(argv[0]); return 1; }
    memset(&G, 0, sizeof(G));

    snprintf(G.domain,sizeof(G.domain),"%s",argv[1]);
    G.pid = strtoull(argv[2],NULL,10);

    char* dash = strchr(argv[3],'-'); if(!dash){ fprintf(stderr,"[!] invalid range\n"); return 1; }
    *dash=0; G.va_lo = strtoull(argv[3],NULL,0); G.va_hi = strtoull(dash+1,NULL,0);
    if(G.va_lo>=G.va_hi){ fprintf(stderr,"[!] invalid range values\n"); return 1; }

    G.watch_timeout_ms = (argc>=5)? strtoull(argv[4],NULL,0) : 5000;

    fprintf(stderr,"=== seed_once_watch_v2 ===\n");
    fprintf(stderr,"[*] domain=%s pid=%llu range=[0x%llx..0x%llx) timeout=%llu ms\n",
            G.domain,
            (unsigned long long)G.pid,
            (unsigned long long)G.va_lo, (unsigned long long)G.va_hi,
            (unsigned long long)G.watch_timeout_ms);

    signal(SIGINT,on_sigint);

    if(VMI_FAILURE==vmi_init_complete(&G.vmi, G.domain,
                                      VMI_INIT_DOMAINNAME | VMI_INIT_EVENTS,
                                      NULL, VMI_CONFIG_GLOBAL_FILE_ENTRY,
                                      NULL,NULL)){
        fprintf(stderr,"[!] vmi_init_complete failed\n"); return 1;
    }
    fprintf(stderr,"[*] libvmi initialized\n");

    // PID → DTB
    addr_t dtb=0;
    if(VMI_SUCCESS==vmi_pid_to_dtb(G.vmi,(vmi_pid_t)G.pid,&dtb)){
        G.dtb = dtb;
        fprintf(stderr,"[+] DTB resolved via vmi_pid_to_dtb: 0x%llx\n",(unsigned long long)G.dtb);
    }else{
        fprintf(stderr,"[!] vmi_pid_to_dtb failed for pid=%llu\n",(unsigned long long)G.pid);
        vmi_destroy(G.vmi); return 1;
    }

    // write-watch 등록
    vmi_event_t write_ev;
    if(arm_write_watch(G.vmi, &write_ev)!=0){
        vmi_destroy(G.vmi); return 1;
    }

    // 이벤트 루프
    uint64_t t0 = now_us();
    while(!g_stop){
        if(VMI_FAILURE==vmi_events_listen(G.vmi, 500)){
            fprintf(stderr,"[!] events_listen failed\n"); break;
        }
        uint64_t elapsed_ms = (now_us() - t0)/1000ULL;
        if(elapsed_ms >= G.watch_timeout_ms){
            fprintf(stderr,"[*] Watch timeout (%llu ms)\n",(unsigned long long)elapsed_ms);
            break;
        }
    }

    vmi_clear_event(G.vmi, &write_ev, NULL);
    vmi_destroy(G.vmi);
    fprintf(stderr,"[*] Done. %s\n", G.triggered ? "(seed write trapped)" : "(no write)");
    return 0;
}
