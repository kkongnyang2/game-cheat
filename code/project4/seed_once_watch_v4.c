// seed_once_watch_v4_3.c
// once watch + pre-write snapshot + disasm(Capstone) + EA검증
// v4.3: RIP 직전 "연속 바이트"를 역방향으로 확보(backward fill) → 근접(last-writer) 추적 우선
//       실패 시 4KB 윈도우 + 제어흐름 경계 이후 Fallback
//
// 빌드:
//   gcc -O2 -Wall -Wextra -std=gnu11 seed_once_watch_v4_3.c -o seed_once_watch_v4_3 \
//     $(pkg-config --cflags libvmi glib-2.0) \
//     $(pkg-config --libs   libvmi glib-2.0) \
//     -lcapstone -Wl,--no-as-needed
//
// 실행:
//   sudo ./seed_once_watch_v4_3 <domain> <pid> 0xVA_START-0xVA_END [timeout_ms]

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
#include <libvmi/x86.h>   // 일부 버전에선 레지스터 enum이 여기 있음

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

    int triggered;
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

// --- pid 기반 vmi_read_va (네 환경 시그니처와 일치)
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

/* ========================= Capstone helpers ========================= */

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

static uint64_t compute_ea_from_cs(const cs_x86_op* op, const Regs* R, uint64_t rip){
    int64_t disp = op->mem.disp;
    uint64_t base = cap_get_reg64(R, op->mem.base);
    uint64_t idx  = cap_get_reg64(R, op->mem.index);
    uint32_t sc   = op->mem.scale ? op->mem.scale : 1;
    if(op->mem.base == X86_REG_RIP) base = rip; // RIP-relative
    return (uint64_t)(base + idx * (uint64_t)sc + (uint64_t)disp);
}

static bool has_rep_prefix(const cs_detail* D){
    if(!D) return false;
    for(int i=0;i<8;i++){
        if(D->x86.prefix[i] == 0xF3 || D->x86.prefix[i] == 0xF2) return true;
    }
    return false;
}

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

    switch(I->id){
        case X86_INS_MOVSB: case X86_INS_MOVSW: case X86_INS_MOVSD: case X86_INS_MOVSQ:
            wkind = has_rep_prefix(D) ? "string-move(rep)" : "string-move";
            mem_write = true; break;
        case X86_INS_STOSB: case X86_INS_STOSW: case X86_INS_STOSD: case X86_INS_STOSQ:
            wkind = has_rep_prefix(D) ? "string-store(rep)" : "string-store";
            mem_write = true; break;
        default: break;
    }

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

/* ===================== 모듈 base/offset (PE32+, SizeOfImage 없음) ===================== */

typedef struct {
    uint64_t base;
    bool     ok;
} ModuleInfo;

static bool pe64_is_base(vmi_instance_t vmi, vmi_pid_t pid, addr_t base){
    uint16_t mz=0; if(!read_u16(vmi,pid,base,&mz) || mz!=0x5A4D) return false;       // 'MZ'
    uint32_t e_lfanew=0; if(!read_u32(vmi,pid,base+0x3C,&e_lfanew)) return false;
    uint32_t pe=0; if(!read_u32(vmi,pid,base+e_lfanew,&pe) || pe!=0x00004550) return false; // 'PE\0\0'
    uint16_t magic=0; if(!read_u16(vmi,pid,base+e_lfanew+0x18,&magic)) return false; // OptionalHeader.Magic
    return (magic == 0x20B); // PE32+
}

// 64KB 스텝 2GB + 4KB 스텝 1GB 역스캔
static ModuleInfo find_module_base_by_rip(vmi_instance_t vmi, vmi_pid_t pid, uint64_t rip){
    ModuleInfo M; memset(&M,0,sizeof(M));

    uint64_t cand = rip & ~0xFFFFULL;
    for (int i=0; i< (1<<15); i++, cand -= 0x10000ULL){
        if (pe64_is_base(vmi,pid,cand)){ M.base=cand; M.ok=true; return M; }
    }
    cand = rip & ~0xFFFULL;
    for (int i=0; i< (1<<18); i++, cand -= 0x1000ULL){
        if (pe64_is_base(vmi,pid,cand)){ M.base=cand; M.ok=true; return M; }
    }
    return M;
}

/* ========================= v4.3: 연속 확보기반 last-writer ========================= */

typedef struct {
    uint64_t addr;
    char     mn[32];
    char     op[64];
    char     kind[20]; // "mov/lea/alu/pop/movx/call/other"
    bool     valid;
} RegOrigin;

static bool writes_reg(const cs_insn* I, x86_reg target){
    const cs_detail* D = I->detail;
    if(!D) return false;
    for (int i=0;i<D->x86.op_count;i++){
        const cs_x86_op* op = &D->x86.operands[i];
        if(op->type == X86_OP_REG && (op->access & CS_AC_WRITE)){
            if (op->reg == target) return true;
            // 32비트 쓰기도 상위 32비트 클리어 → RCX/RDX에 대한 정의로 간주
            if (target==X86_REG_RCX && op->reg==X86_REG_ECX) return true;
            if (target==X86_REG_RDX && op->reg==X86_REG_EDX) return true;
        }
    }
    return false;
}

static const char* classify_kind(const cs_insn* I){
    switch(I->id){
        case X86_INS_MOV:    return "mov";
        case X86_INS_LEA:    return "lea";
        case X86_INS_ADD: case X86_INS_SUB: case X86_INS_XOR:
        case X86_INS_OR:  case X86_INS_AND: case X86_INS_SHL:
        case X86_INS_SHR: case X86_INS_SAR: case X86_INS_IMUL:
        case X86_INS_INC: case X86_INS_DEC: return "alu";
        case X86_INS_POP:    return "pop";
        case X86_INS_MOVZX: case X86_INS_MOVSX: return "movx";
        case X86_INS_CALL:   return "call";
        default:             return "other";
    }
}

// RIP을 끝점으로, 뒤에서 앞으로 "연속 바이트"를 최대 near_bytes만큼 확보
static size_t read_contiguous_before_rip(vmi_instance_t vmi, vmi_pid_t pid,
                                         uint64_t rip, uint8_t* out, size_t near_bytes)
{
    size_t filled = 0;
    uint64_t end = rip; // [start, end) 영역에서 end가 rip
    while (filled < near_bytes) {
        uint64_t need = near_bytes - filled;
        uint64_t cur_end = end;
        uint64_t page_lo = (cur_end - 1) & ~0xFFFULL;
        uint64_t cur_start = (cur_end > need) ? (cur_end - need) : 0;
        if ((cur_start & ~0xFFFULL) != page_lo) cur_start = page_lo; // 한 번에 한 페이지만

        size_t chunk = (size_t)(cur_end - cur_start);
        if (chunk == 0) break;

        uint8_t tmp[4096];
        size_t got = safe_read_va(vmi, pid, cur_start, tmp, chunk);
        if (got == 0) break; // 연속성 깨짐 → 중단

        // out의 끝쪽부터 채우기: out[(near_bytes - filled - got) .. (near_bytes - filled - 1)]
        memcpy(out + (near_bytes - filled - got), tmp + (chunk - got), got);
        filled += got;
        end = cur_start; // 다음 라운드에 그 이전 페이지 시도
    }
    return filled; // 실제 확보된 연속 길이
}

// 확보한 "RIP 직전 연속 바이트"에서 마지막 정의 찾기
static bool analyze_near_last_defs(vmi_instance_t vmi, vmi_pid_t pid, uint64_t rip,
                                   RegOrigin* o_rcx, RegOrigin* o_rdx,
                                   size_t near_bytes /*예: 768*/)
{
    uint8_t buf[4096]; if (near_bytes > sizeof(buf)) near_bytes = sizeof(buf);
    size_t got = read_contiguous_before_rip(vmi, pid, rip, buf, near_bytes);
    if (got == 0) return false;

    uint64_t start = rip - got;

    csh h=0; if (cs_open(CS_ARCH_X86, CS_MODE_64, &h) != CS_ERR_OK) return false;
    cs_option(h, CS_OPT_DETAIL, CS_OPT_ON);

    cs_insn* insn = NULL;
    size_t cnt = cs_disasm(h, buf, got, start, 0, &insn);
    if (!cnt) { cs_close(&h); return false; }

    RegOrigin rcx={0}, rdx={0};
    for (size_t i=0; i<cnt; i++){
        const cs_insn* I = &insn[i];
        if (I->address >= rip) break;
        if (writes_reg(I, X86_REG_RCX)){
            rcx.addr=I->address;
            snprintf(rcx.mn,sizeof(rcx.mn),"%s",I->mnemonic);
            snprintf(rcx.op,sizeof(rcx.op),"%s",I->op_str);
            snprintf(rcx.kind,sizeof(rcx.kind),"%s", classify_kind(I));
            rcx.valid=true;
        }
        if (writes_reg(I, X86_REG_RDX)){
            rdx.addr=I->address;
            snprintf(rdx.mn,sizeof(rdx.mn),"%s",I->mnemonic);
            snprintf(rdx.op,sizeof(rdx.op),"%s",I->op_str);
            snprintf(rdx.kind,sizeof(rdx.kind),"%s", classify_kind(I));
            rdx.valid=true;
        }
    }

    cs_free(insn, cnt);
    cs_close(&h);

    if (o_rcx) *o_rcx = rcx;
    if (o_rdx) *o_rdx = rdx;
    return (rcx.valid || rdx.valid);
}

// Fallback: 4KB 윈도우 + 최근 제어흐름 경계(RET/JMP/Jcc/CALL) 이후만 분석
static void analyze_backtrace(uint64_t rip, vmi_instance_t vmi, vmi_pid_t pid,
                              RegOrigin* out_rcx, RegOrigin* out_rdx,
                              uint64_t* bt_start, size_t* bt_count, int max_ctx){
    const size_t PRE_WINDOW = 4096;
    uint8_t buf[PRE_WINDOW+32]; memset(buf,0,sizeof(buf));
    addr_t start = (rip > PRE_WINDOW)? (rip - PRE_WINDOW) : 0;
    size_t got = safe_read_va(vmi, pid, start, buf, sizeof(buf));
    if(got == 0){ if(bt_start) *bt_start=start; if(bt_count) *bt_count=0; return; }

    csh h=0; if(cs_open(CS_ARCH_X86, CS_MODE_64, &h)!=CS_ERR_OK){ if(bt_start) *bt_start=start; if(bt_count) *bt_count=0; return; }
    cs_option(h, CS_OPT_DETAIL, CS_OPT_ON);

    cs_insn* insn=NULL;
    size_t cnt = cs_disasm(h, buf, got, start, 0, &insn);

    size_t ctx_total = 0, last_idx_before_rip = 0;
    for(size_t i=0;i<cnt;i++){
        if(insn[i].address >= rip) break;
        last_idx_before_rip = i;
        ctx_total++;
    }

    ssize_t begin = 0;
    for(ssize_t i=(ssize_t)last_idx_before_rip; i>=0; --i){
        const cs_insn* I = &insn[i];
        switch(I->id){
            case X86_INS_RET: case X86_INS_RETF:
            case X86_INS_JMP:
            #ifdef X86_INS_JMPQ
            case X86_INS_JMPQ:
            #endif
            case X86_INS_JE: case X86_INS_JNE: case X86_INS_JA:
            case X86_INS_JAE: case X86_INS_JB: case X86_INS_JBE:
            case X86_INS_JG: case X86_INS_JGE: case X86_INS_JL: case X86_INS_JLE:
            #ifdef X86_INS_LOOP
            case X86_INS_LOOP:
            #endif
            #ifdef X86_INS_LOOPE
            case X86_INS_LOOPE:
            #endif
            #ifdef X86_INS_LOOPNE
            case X86_INS_LOOPNE:
            #endif
            #ifdef X86_INS_JRCXZ
            case X86_INS_JRCXZ:
            #endif
            case X86_INS_CALL:
                begin = i + 1;
                goto found_boundary;
            default: break;
        }
    }
found_boundary:;

    RegOrigin rcx={0}, rdx={0};
    for(size_t i=(size_t)begin;i<=last_idx_before_rip;i++){
        const cs_insn* I = &insn[i];
        if(writes_reg(I, X86_REG_RCX)){
            rcx.addr=I->address;
            snprintf(rcx.mn,sizeof(rcx.mn),"%s",I->mnemonic);
            snprintf(rcx.op,sizeof(rcx.op),"%s",I->op_str);
            snprintf(rcx.kind,sizeof(rcx.kind),"%s", classify_kind(I));
            rcx.valid=true;
        }
        if(writes_reg(I, X86_REG_RDX)){
            rdx.addr=I->address;
            snprintf(rdx.mn,sizeof(rdx.mn),"%s",I->mnemonic);
            snprintf(rdx.op,sizeof(rdx.op),"%s",I->op_str);
            snprintf(rdx.kind,sizeof(rdx.kind),"%s", classify_kind(I));
            rdx.valid=true;
        }
    }

    if(out_rcx) *out_rcx = rcx;
    if(out_rdx) *out_rdx = rdx;
    if(bt_start) *bt_start = start;
    if(bt_count) *bt_count = (ctx_total > (size_t)max_ctx ? (size_t)max_ctx : ctx_total);

    cs_free(insn, cnt);
    cs_close(&h);
}

/* ========================= 메인 수집/출력 ========================= */

static void collect_and_print_snapshot(vmi_instance_t vmi, vmi_event_t* event){
    uint64_t ts = now_us();
    const uint32_t vcpu = event->vcpu_id;
    const addr_t gla   = event->mem_event.gla;
    const addr_t gfn   = event->mem_event.gfn;

    // 레지스터
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

    // RIP bytes
    uint8_t rip_bytes[16]; memset(rip_bytes, 0, sizeof(rip_bytes));
    size_t rip_n = 0;
    if(R.rip) rip_n = safe_read_va(vmi, (vmi_pid_t)G.pid, (addr_t)R.rip, rip_bytes, sizeof(rip_bytes));

    // 모듈 base/offset
    ModuleInfo MI = {0};
    if(R.rip) MI = find_module_base_by_rip(vmi, (vmi_pid_t)G.pid, R.rip);

    // pre/post/stack
    enum { PREN=16, POSTN=16 };
    uint8_t pre[PREN];  memset(pre,  0, sizeof(pre));
    uint8_t post[POSTN]; memset(post, 0, sizeof(post));
    size_t pre_got=0, post_got=0;
    if(gla && gla>=PREN) pre_got  = safe_read_va(vmi, (vmi_pid_t)G.pid, gla-PREN, pre,  sizeof(pre));
    if(gla)              post_got = safe_read_va(vmi, (vmi_pid_t)G.pid, gla,      post, sizeof(post));

    uint8_t stk[64]; memset(stk, 0, sizeof(stk));
    size_t stk_got = 0;
    if(R.rsp) stk_got = safe_read_va(vmi, (vmi_pid_t)G.pid, (addr_t)R.rsp, stk, sizeof(stk));

    // v4.3: 우선 RIP 직전 "연속 바이트"에서 마지막 정의 추적
    RegOrigin o_rcx={0}, o_rdx={0};
    bool ok_near = analyze_near_last_defs(vmi, (vmi_pid_t)G.pid, R.rip, &o_rcx, &o_rdx, 768);

    // backtrace 출력용 컨텍스트 준비
    uint64_t bt_start=0; size_t bt_ctxN=0;
    if (!ok_near) {
        // 실패 시 Fallback로 보조 추적 + backtrace
        const int PRINT_CTX = 48;
        analyze_backtrace(R.rip, vmi, (vmi_pid_t)G.pid, &o_rcx, &o_rdx, &bt_start, &bt_ctxN, PRINT_CTX);
    } else {
        // 근접 추적이 성공한 경우, backtrace는 단순히 RIP-768 기준으로 출력
        bt_start = (R.rip > 768)? (R.rip - 768) : 0;
        bt_ctxN  = 48;
    }

    // JSON
    fprintf(stderr, "{");
    fprintf(stderr, "\"evt\":\"seed_write_pre\",\"ts_us\":%" PRIu64 ",\"vcpu\":%u,", ts, vcpu);
    fprintf(stderr, "\"pid\":%llu,", (unsigned long long)G.pid);

    fprintf(stderr, "\"gla\":\"0x%llx\",\"gfn\":\"0x%llx\",",
            (unsigned long long)gla, (unsigned long long)gfn);
    fprintf(stderr, "\"dtb\":\"0x%llx\",\"cr3\":\"0x%llx\",",
            (unsigned long long)G.dtb, (unsigned long long)cr3);

    fprintf(stderr, "\"rip\":\"0x%llx\",\"rsp\":\"0x%llx\",\"rbp\":\"0x%llx\",\"rflags\":\"0x%llx\",",
            (unsigned long long)R.rip,(unsigned long long)R.rsp,(unsigned long long)R.rbp,(unsigned long long)rflags);

    // module base/offset
    fprintf(stderr, "\"module\":{");
    if(MI.ok){
        uint64_t off = R.rip - MI.base;
        fprintf(stderr, "\"base\":\"0x%llx\",\"rip_off\":\"0x%llx\"",
                (unsigned long long)MI.base, (unsigned long long)off);
    }else{
        fprintf(stderr, "\"base\":null");
    }
    fprintf(stderr, "},");

    // GPRs
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

    // 현재 RIP 명령 요약
    if(rip_n > 0){
        disasm_and_classify(R.rip, rip_bytes, rip_n, &R, gla);
    }

    // RIP bytes / 주변 메모리 / 스택
    { char h[256]; hex_dump(rip_bytes, rip_n, h, sizeof(h));
      fprintf(stderr, "\"rip_bytes_len\":%zu,\"rip_bytes\":\"%s\",", rip_n, h); }
    { char h[256]; hex_dump(pre, pre_got, h, sizeof(h));
      fprintf(stderr, "\"pre_len\":%zu,\"pre_bytes\":\"%s\",", pre_got, h); }
    { char h[512]; hex_dump(post, post_got, h, sizeof(h));
      fprintf(stderr, "\"post_len\":%zu,\"post_bytes\":\"%s\",", post_got, h); }
    { char h[1024]; hex_dump(stk, stk_got, h, sizeof(h));
      fprintf(stderr, "\"stack_len\":%zu,\"stack_bytes\":\"%s\",", stk_got, h); }

    // origin (마지막 정의)
    fprintf(stderr, "\"origin\":{");
    fprintf(stderr, "\"rcx\":");
    if(o_rcx.valid){
        fprintf(stderr, "{\"addr\":\"0x%llx\",\"mn\":\"%s\",\"op\":\"%s\",\"kind\":\"%s\"}",
                (unsigned long long)o_rcx.addr, o_rcx.mn, o_rcx.op, o_rcx.kind);
    }else{
        fprintf(stderr, "null");
    }
    fprintf(stderr, ",\"rdx\":");
    if(o_rdx.valid){
        fprintf(stderr, "{\"addr\":\"0x%llx\",\"mn\":\"%s\",\"op\":\"%s\",\"kind\":\"%s\"}",
                (unsigned long long)o_rdx.addr, o_rdx.mn, o_rdx.op, o_rdx.kind);
    }else{
        fprintf(stderr, "null");
    }
    fprintf(stderr, "},"); // origin

    // backtrace context (최근 N개만)
    {
        const size_t PRE_WINDOW2 = 768; // 보기용: 근접 위주
        uint8_t b2[PRE_WINDOW2+32]; memset(b2,0,sizeof(b2));
        addr_t st = (R.rip > PRE_WINDOW2)? (R.rip - PRE_WINDOW2) : 0;
        size_t g2 = safe_read_va(vmi, (vmi_pid_t)G.pid, st, b2, sizeof(b2));
        fprintf(stderr, "\"backtrace\":{\"start\":\"0x%llx\",\"ctx\":[", (unsigned long long)st);
        if(g2){
            csh hh=0; if(cs_open(CS_ARCH_X86, CS_MODE_64, &hh)==CS_ERR_OK){
                cs_option(hh, CS_OPT_DETAIL, CS_OPT_OFF);
                cs_insn* ii=NULL; size_t cc=cs_disasm(hh, b2, g2, st, 0, &ii);
                size_t valid_count=0;
                for(size_t k=0;k<cc;k++){ if(ii[k].address < R.rip) valid_count++; else break; }
                size_t keep = (bt_ctxN ? bt_ctxN : 48);
                size_t start_idx = (valid_count>keep)? (valid_count-keep) : 0;
                for(size_t k=start_idx; k<valid_count; k++){
                    fprintf(stderr, "{\"a\":\"0x%llx\",\"mn\":\"%s\",\"op\":\"%s\"}%s",
                        (unsigned long long)ii[k].address, ii[k].mnemonic, ii[k].op_str,
                        (k+1<valid_count)? ",":"");
                }
                cs_free(ii,cc); cs_close(&hh);
            }
        }
        fprintf(stderr, "]},");
    }

    // JSON 마무리
    fprintf(stderr, "\"_\":\"_\"}\n");
}

static event_response_t on_seed_write(vmi_instance_t vmi, vmi_event_t *event){
    const addr_t gla = event->mem_event.gla;
    if(!(gla >= G.va_lo && gla < G.va_hi)) return 0;

    if(!G.triggered){
        G.triggered = 1;

        // 같은 GFN의 W-트랩 즉시 해제 (프리즈/재진입 방지)
        vmi_set_mem_event(vmi, event->mem_event.gfn, VMI_MEMACCESS_N, 0);

        fprintf(stderr,"[W] WRITE trap @0x%llx (vcpu=%u, gfn=0x%llx)\n",
                (unsigned long long)gla, event->vcpu_id, (unsigned long long)event->mem_event.gfn);

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
    fprintf(stderr,"Usage: sudo %s <domain> <pid> <va_start-va_end> [watch_timeout_ms]\n", p);
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

    fprintf(stderr,"=== seed_once_watch_v4_3 ===\n");
    fprintf(stderr,"[*] domain=%s pid=%llu range=[0x%llx..0x%llx) timeout=%llu ms\n",
            G.domain, (unsigned long long)G.pid,
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
    if(VMI_SUCCESS==vmi_pid_to_dtb(G.vmi,(vmi_pid_t)G.pid,&G.dtb)){
        fprintf(stderr,"[+] DTB resolved via vmi_pid_to_dtb: 0x%llx\n",(unsigned long long)G.dtb);
    }else{
        fprintf(stderr,"[!] vmi_pid_to_dtb failed for pid=%llu\n",(unsigned long long)G.pid);
        vmi_destroy(G.vmi); return 1;
    }

    // write-watch 등록
    vmi_event_t write_ev;
    if(arm_write_watch(G.vmi, &write_ev)!=0){ vmi_destroy(G.vmi); return 1; }

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
