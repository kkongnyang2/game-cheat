// seed_once_watch_v3.c — once watch + pre-write snapshot + disasm (Capstone)
// + 모듈 라벨링(PE32+ 전용): SizeOfImage 없이 base/offset만 계산
// - 콜백에서 해당 GFN 즉시 해제: vmi_set_mem_event(..., VMI_MEMACCESS_N, 0)
// - Capstone REP 판별은 prefix(0xF3/0xF2)

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

#include <capstone/capstone.h>

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

// --- 메모리 읽기: pid 기반 vmi_read_va (네 환경 시그니처)
static size_t safe_read_va(vmi_instance_t vmi, vmi_pid_t pid, addr_t va, void* buf, size_t len){
    size_t got = 0;
    if (VMI_SUCCESS == vmi_read_va(vmi, va, pid, len, buf, &got)) return got;
    return 0;
}

static bool read_u16(vmi_instance_t vmi, vmi_pid_t pid, addr_t va, uint16_t* out){
    uint16_t t=0; return safe_read_va(vmi,pid,va,&t,sizeof(t))==sizeof(t) ? (*out=t, true):false;
}
static bool read_u32(vmi_instance_t vmi, vmi_pid_t pid, addr_t va, uint32_t* out){
    uint32_t t=0; return safe_read_va(vmi,pid,va,&t,sizeof(t))==sizeof(t) ? (*out=t, true):false;
}

static void hex_dump(const uint8_t* b, size_t n, char* out, size_t outsz){
    size_t pos=0;
    for(size_t i=0;i<n;i++){
        if(pos+2 >= outsz) break;
        pos += (size_t)snprintf(out+pos, outsz-pos, "%02x", b[i]);
    }
    if(pos<outsz) out[pos]='\0';
}

// --- Capstone: EA 재계산용 레지스터 묶음
typedef struct {
    uint64_t rax,rbx,rcx,rdx,rsi,rdi;
    uint64_t r8,r9,r10,r11,r12,r13,r14,r15;
    uint64_t rip,rsp,rbp;
} Regs;

static uint64_t cap_get_reg64(const Regs* R, x86_reg reg){
    switch(reg){
        case X86_REG_RAX: return R->rax;
        case X86_REG_RBX: return R->rbx;
        case X86_REG_RCX: return R->rcx;
        case X86_REG_RDX: return R->rdx;
        case X86_REG_RSI: return R->rsi;
        case X86_REG_RDI: return R->rdi;
        case X86_REG_RBP: return R->rbp;
        case X86_REG_RSP: return R->rsp;
        case X86_REG_R8:  return R->r8;
        case X86_REG_R9:  return R->r9;
        case X86_REG_R10: return R->r10;
        case X86_REG_R11: return R->r11;
        case X86_REG_R12: return R->r12;
        case X86_REG_R13: return R->r13;
        case X86_REG_R14: return R->r14;
        case X86_REG_R15: return R->r15;
        case X86_REG_RIP: return R->rip;
        default: return 0;
    }
}

// EA = base + index*scale + disp (+ RIP상대인 경우 RIP)
static uint64_t compute_ea_from_cs(const cs_x86_op* op, const Regs* R, uint64_t rip){
    int64_t disp = op->mem.disp;
    uint64_t base = cap_get_reg64(R, op->mem.base);
    uint64_t idx  = cap_get_reg64(R, op->mem.index);
    uint32_t sc   = op->mem.scale ? op->mem.scale : 1;
    if(op->mem.base == X86_REG_RIP) base = rip; // RIP-relative
    return (uint64_t)(base + idx * (uint64_t)sc + (uint64_t)disp);
}

// prefix에서 REP/REPNE 체크
static bool has_rep_prefix(const cs_detail* D){
    if(!D) return false;
    for(int i=0;i<8;i++){
        if(D->x86.prefix[i] == 0xF3 || D->x86.prefix[i] == 0xF2) return true;
    }
    return false;
}

// --- 디스어셈블 + 쓰기 판별 + EA 검증 (한 개 명령만)
static void disasm_and_classify(uint64_t rip, const uint8_t* bytes, size_t n,
                                const Regs* R, uint64_t gla){
    csh h = 0;
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &h) != CS_ERR_OK) return;
    cs_option(h, CS_OPT_DETAIL, CS_OPT_ON);

    cs_insn *insn = NULL;
    size_t cnt = cs_disasm(h, bytes, n, rip, 1, &insn);
    if (cnt == 0) { cs_close(&h); return; }

    const cs_insn* I = &insn[0];
    const cs_detail* D = I->detail;

    const char* wkind = "unknown";
    bool mem_write = false;
    uint64_t ea_calc = 0;
    bool ea_match = false;

    // string move/store: MOVS*, STOS* (+ optional REP prefix)
    switch(I->id){
        case X86_INS_MOVSB: case X86_INS_MOVSW: case X86_INS_MOVSD: case X86_INS_MOVSQ:
            wkind = has_rep_prefix(D) ? "string-move(rep)" : "string-move";
            mem_write = true; break;
        case X86_INS_STOSB: case X86_INS_STOSW: case X86_INS_STOSD: case X86_INS_STOSQ:
            wkind = has_rep_prefix(D) ? "string-store(rep)" : "string-store";
            mem_write = true; break;
        default: break;
    }

    // 일반 store 판정: 피연산자 중 mem && access WRITE
    if (!mem_write && D){
        for (int i=0; i<D->x86.op_count; i++){
            const cs_x86_op* op = &D->x86.operands[i];
            if (op->type == X86_OP_MEM && (op->access & CS_AC_WRITE)){
                mem_write = true;
                if (strcmp(wkind, "unknown") == 0) wkind = "store";
                ea_calc = compute_ea_from_cs(op, R, R->rip);
                ea_match = (ea_calc == gla);
                break;
            }
        }
    }

    fprintf(stderr, "\"disasm\":{");
    fprintf(stderr, "\"rip\":\"0x%llx\",\"mn\":\"%s\",\"op\":\"%s\",\"mem_write\":%s,\"wkind\":\"%s\"",
            (unsigned long long)I->address, I->mnemonic, I->op_str,
            mem_write ? "true":"false", wkind);
    if(ea_calc){
        fprintf(stderr, ",\"ea_calc\":\"0x%llx\",\"ea_match\":%s",
            (unsigned long long)ea_calc, ea_match ? "true":"false");
    }
    fprintf(stderr, "},");
    cs_free(insn, cnt);
    cs_close(&h);
}

/* ========== 모듈 베이스 탐색(PE32+ 전용, SizeOfImage 불요) ========== */

typedef struct {
    uint64_t base;
    bool     ok;
} ModuleInfo;

static bool pe64_is_base(vmi_instance_t vmi, vmi_pid_t pid, addr_t base){
    // DOS header: 'MZ'
    uint16_t mz=0; if(!read_u16(vmi,pid,base,&mz) || mz!=0x5A4D) return false;
    // e_lfanew
    uint32_t e_lfanew=0; if(!read_u32(vmi,pid,base+0x3C,&e_lfanew)) return false;
    // NT headers: 'PE\0\0'
    uint32_t pe=0; if(!read_u32(vmi,pid,base+e_lfanew,&pe) || pe!=0x00004550) return false;
    // OptionalHeader.Magic == 0x20B (PE32+)
    uint16_t magic=0; if(!read_u16(vmi,pid,base+e_lfanew+0x18,&magic)) return false;
    return (magic == 0x20B);
}

// 더 넓은 PE 스캔: 64KB 스텝 2GB + 4KB 스텝 1GB
static ModuleInfo find_module_base_by_rip(vmi_instance_t vmi, vmi_pid_t pid, uint64_t rip){
    ModuleInfo M; memset(&M,0,sizeof(M));

    // 1) 64KB 경계 하향 스캔 — 2GB
    uint64_t cand = rip & ~0xFFFFULL;
    for (int i=0; i< (1<<15); i++, cand -= 0x10000ULL){ // 32768 * 64KB = 2GB
        if (pe64_is_base(vmi,pid,cand)){
            M.base = cand; M.ok = true; return M;
        }
    }

    // 2) 보조: 4KB 페이지 하향 스캔 — 1GB
    cand = rip & ~0xFFFULL;
    for (int i=0; i< (1<<18); i++, cand -= 0x1000ULL){ // 262144 * 4KB = 1GB
        if (pe64_is_base(vmi,pid,cand)){
            M.base = cand; M.ok = true; return M;
        }
    }

    return M; // 실패 시 ok=false
}

/* =================================================================== */

static void collect_and_print_snapshot(vmi_instance_t vmi, vmi_event_t* event){
    uint64_t ts = now_us();
    const uint32_t vcpu = event->vcpu_id;
    const addr_t gla   = event->mem_event.gla;
    const addr_t gfn   = event->mem_event.gfn;

    // 레지스터 스냅샷
    Regs R = {0};
    uint64_t rflags=0, cr3=0;

    vmi_get_vcpureg(vmi, &R.rip,   VMI_X86_RIP,   vcpu);
    vmi_get_vcpureg(vmi, &R.rsp,   VMI_X86_RSP,   vcpu);
    vmi_get_vcpureg(vmi, &R.rbp,   VMI_X86_RBP,   vcpu);
    vmi_get_vcpureg(vmi, &rflags,  VMI_X86_RFLAGS,vcpu);

    vmi_get_vcpureg(vmi, &R.rax, VMI_X86_RAX, vcpu);
    vmi_get_vcpureg(vmi, &R.rbx, VMI_X86_RBX, vcpu);
    vmi_get_vcpureg(vmi, &R.rcx, VMI_X86_RCX, vcpu);
    vmi_get_vcpureg(vmi, &R.rdx, VMI_X86_RDX, vcpu);
    vmi_get_vcpureg(vmi, &R.rsi, VMI_X86_RSI, vcpu);
    vmi_get_vcpureg(vmi, &R.rdi, VMI_X86_RDI, vcpu);
    vmi_get_vcpureg(vmi, &R.r8,  VMI_X86_R8,  vcpu);
    vmi_get_vcpureg(vmi, &R.r9,  VMI_X86_R9,  vcpu);
    vmi_get_vcpureg(vmi, &R.r10, VMI_X86_R10, vcpu);
    vmi_get_vcpureg(vmi, &R.r11, VMI_X86_R11, vcpu);
    vmi_get_vcpureg(vmi, &R.r12, VMI_X86_R12, vcpu);
    vmi_get_vcpureg(vmi, &R.r13, VMI_X86_R13, vcpu);
    vmi_get_vcpureg(vmi, &R.r14, VMI_X86_R14, vcpu);
    vmi_get_vcpureg(vmi, &R.r15, VMI_X86_R15, vcpu);
    vmi_get_vcpureg(vmi, &cr3,   VMI_X86_CR3, vcpu);

    // RIP 바이트
    uint8_t rip_bytes[16]; memset(rip_bytes, 0, sizeof(rip_bytes));
    size_t rip_n = 0;
    if(R.rip) rip_n = safe_read_va(vmi, (vmi_pid_t)G.pid, (addr_t)R.rip, rip_bytes, sizeof(rip_bytes));

    // 모듈 베이스 (PE32+ 스캔, SizeOfImage 불요)
    ModuleInfo MI = {0};
    if(R.rip) MI = find_module_base_by_rip(vmi, (vmi_pid_t)G.pid, R.rip);

    // 대상 주소 주변 (pre:16, post:16)
    enum { PREN=16, POSTN=16 };
    uint8_t pre[PREN];  memset(pre,  0, sizeof(pre));
    uint8_t post[POSTN]; memset(post, 0, sizeof(post));
    size_t pre_got=0, post_got=0;
    if(gla && gla>=PREN) pre_got  = safe_read_va(vmi, (vmi_pid_t)G.pid, gla-PREN, pre,  sizeof(pre));
    if(gla)              post_got = safe_read_va(vmi, (vmi_pid_t)G.pid, gla,      post, sizeof(post));

    // 스택 64B
    uint8_t stk[64]; memset(stk, 0, sizeof(stk));
    size_t stk_got = 0;
    if(R.rsp) stk_got = safe_read_va(vmi, (vmi_pid_t)G.pid, (addr_t)R.rsp, stk, sizeof(stk));

    // JSON 한 줄
    fprintf(stderr, "{");
    fprintf(stderr, "\"evt\":\"seed_write_pre\",\"ts_us\":%" PRIu64 ",\"vcpu\":%u,", ts, vcpu);
    fprintf(stderr, "\"pid\":%llu,", (unsigned long long)G.pid);

    fprintf(stderr, "\"gla\":\"0x%llx\",\"gfn\":\"0x%llx\",",
            (unsigned long long)gla, (unsigned long long)gfn);
    fprintf(stderr, "\"dtb\":\"0x%llx\",\"cr3\":\"0x%llx\",",
            (unsigned long long)G.dtb, (unsigned long long)cr3);

    fprintf(stderr, "\"rip\":\"0x%llx\",\"rsp\":\"0x%llx\",\"rbp\":\"0x%llx\",\"rflags\":\"0x%llx\",",
            (unsigned long long)R.rip,(unsigned long long)R.rsp,(unsigned long long)R.rbp,(unsigned long long)rflags);

    // 모듈(base/offset)
    fprintf(stderr, "\"module\":{");
    if(MI.ok){
        uint64_t off = R.rip - MI.base;
        fprintf(stderr, "\"base\":\"0x%llx\",\"rip_off\":\"0x%llx\"",
                (unsigned long long)MI.base, (unsigned long long)off);
    }else{
        fprintf(stderr, "\"base\":null");
    }
    fprintf(stderr, "},");

    // GPR
    fprintf(stderr, "\"gpr\":{");
    fprintf(stderr, "\"rax\":\"0x%llx\",\"rbx\":\"0x%llx\",\"rcx\":\"0x%llx\",\"rdx\":\"0x%llx\",",
            (unsigned long long)R.rax,(unsigned long long)R.rbx,(unsigned long long)R.rcx,(unsigned long long)R.rdx);
    fprintf(stderr, "\"rsi\":\"0x%llx\",\"rdi\":\"0x%llx\",",
            (unsigned long long)R.rsi,(unsigned long long)R.rdi);
    fprintf(stderr, "\"r8\":\"0x%llx\",\"r9\":\"0x%llx\",\"r10\":\"0x%llx\",\"r11\":\"0x%llx\",",
            (unsigned long long)R.r8,(unsigned long long)R.r9,(unsigned long long)R.r10,(unsigned long long)R.r11);
    fprintf(stderr, "\"r12\":\"0x%llx\",\"r13\":\"0x%llx\",\"r14\":\"0x%llx\",\"r15\":\"0x%llx\"",
            (unsigned long long)R.r12,(unsigned long long)R.r13,(unsigned long long)R.r14,(unsigned long long)R.r15);
    fprintf(stderr, "},");

    // 디스어셈블 & 분류 & EA검증
    if(rip_n > 0){
        disasm_and_classify(R.rip, rip_bytes, rip_n, &R, gla);
    }

    { char h[256]; hex_dump(rip_bytes, rip_n, h, sizeof(h));
      fprintf(stderr, "\"rip_bytes_len\":%zu,\"rip_bytes\":\"%s\",", rip_n, h); }

    { char h[256]; hex_dump(pre, pre_got, h, sizeof(h));
      fprintf(stderr, "\"pre_len\":%zu,\"pre_bytes\":\"%s\",", pre_got, h); }

    { char h[512]; hex_dump(post, post_got, h, sizeof(post_got ? post : 0));
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

        // ★ 같은 GFN W-트랩 즉시 해제(프리즈 방지)
        vmi_set_mem_event(vmi, event->mem_event.gfn, VMI_MEMACCESS_N, 0);

        fprintf(stderr,
            "[W] WRITE trap @0x%llx (vcpu=%u, gfn=0x%llx)\n",
            (unsigned long long)gla, event->vcpu_id,
            (unsigned long long)event->mem_event.gfn);

        collect_and_print_snapshot(vmi, event);

        g_stop = 1;
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
        "Usage: sudo %s <domain> <pid> <va_start-va_end> [watch_timeout_ms]\n", p);
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

    fprintf(stderr,"=== seed_once_watch_v3 ===\n");
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

    /* === freeze-safe cleanup: relax -> drain -> resume === */
    vmi_pause_vm(G.vmi);

    /* 잡아둔 범위(페이지 단위) 전체를 권한 N으로 복구 */
    uint64_t page_lo = G.va_lo & ~0xfffULL;
    uint64_t page_hi = (G.va_hi + 0xfffULL) & ~0xfffULL;
    for (uint64_t va = page_lo; va < page_hi; va += 0x1000ULL) {
        addr_t gfn = 0;
        if (va_to_gfn(G.vmi, G.dtb, (addr_t)va, &gfn)) {
            vmi_set_mem_event(G.vmi, gfn, VMI_MEMACCESS_N, 0);
        }
    }

    /* 늦게 도착한 이벤트 드레인 */
    vmi_events_listen(G.vmi, 0);
    vmi_events_listen(G.vmi, 0);

    vmi_resume_vm(G.vmi);
    /* === end freeze-safe cleanup === */

    vmi_clear_event(G.vmi, &write_ev, NULL);
    vmi_destroy(G.vmi);
    fprintf(stderr,"[*] Done. %s\n", G.triggered ? "(seed write trapped)" : "(no write)");
    return 0;
}
