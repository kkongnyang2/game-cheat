// main.c — LibVMI heap write tracer (Xen, Windows, PID-only, page-scoped)
// Flow:
//   seed WRITE -> memaccess=N (해당 페이지 알람 OFF, write 통과)
//   -> 메인루프에서 SS ON (해당 vCPU)
//   -> on_single_step 첫 진입에 memaccess=W (같은 페이지 재트랩)
//   -> 같은 페이지의 다음 WRITE에서 SS OFF + 종료
//
// 주의:
//   - 콜백 안에서 SS 토글 X (반환값/직접토글 모두 금지).
//   - 메인루프에서 vmi_toggle_single_step_vcpu 사용 (ss_ev.vcpus/enable 먼저 설정).
//   - altp2m view id는 런타임(dlsym)으로 가져오고, 없으면 0 사용.

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <dlfcn.h>

#include <libvmi/libvmi.h>
#include <libvmi/events.h>
#include <libvmi/x86.h>

#ifdef __has_include
#  if __has_include(<capstone/capstone.h>)
#    include <capstone/capstone.h>
#    define HAVE_CAPSTONE 1
#  endif
#endif

#ifndef HAVE_CAPSTONE
#  warning "Capstone not found at build time. Falling back to raw bytes (no disassembly)."
#endif

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int sig){ (void)sig; g_stop=1; }

static uint64_t now_us(void){
    struct timespec ts; clock_gettime(CLOCK_REALTIME,&ts);
    return (uint64_t)ts.tv_sec*1000000ULL + ts.tv_nsec/1000ULL;
}

typedef enum { ST_IDLE=0, ST_ARMED=1, ST_ACTIVE=2 } trace_stage_t;

typedef struct {
    char     domain[128];
    uint64_t pid;
    uint64_t va_lo, va_hi;
    uint64_t watch_timeout_ms;
    uint64_t max_steps;
    uint64_t max_trace_ms;
    char     out_path[256];

    vmi_instance_t vmi;
    addr_t   dtb;

    // 상태
    bool           tracing;
    trace_stage_t  stage;
    uint64_t       t0_us;
    uint64_t       step_count;

    // 시드 write 페이지 기준 종료
    uint64_t       first_page;   // (gla>>12)
    addr_t         seed_gfn;
    uint16_t       seed_viewid;

    // 콜백 밖 SS 토글 펜딩
    int            pending_ss;    // -1=OFF, +1=ON, 0=없음
    uint32_t       pending_vcpu;

    FILE*    out;
} tracer_t;

static tracer_t G;
static vmi_event_t write_ev, ss_ev;

#ifdef HAVE_CAPSTONE
static csh cs_handle; static bool cs_ready=false;
static void cs_init_once(void){
    if(cs_ready) return;
    if(cs_open(CS_ARCH_X86, CS_MODE_64, &cs_handle)==CS_ERR_OK){
        cs_option(cs_handle, CS_OPT_DETAIL, CS_OPT_OFF);
        cs_ready = true;
    }
}
static void disas_and_write(uint64_t rip, const uint8_t* bytes, size_t len, FILE* out){
    if(!cs_ready) cs_init_once();
    if(!cs_ready || len==0){ fprintf(out, "\"asm\":\"\""); return; }
    cs_insn *insn; size_t n = cs_disasm(cs_handle, bytes, len, rip, 1, &insn);
    if(n>0){
        fprintf(out, "\"asm\":\"%s %s\"", insn[0].mnemonic, insn[0].op_str);
        cs_free(insn, n);
    }else{
        fprintf(out, "\"asm\":\"\"");
    }
}
#else
static void disas_and_write(uint64_t rip, const uint8_t* bytes, size_t len, FILE* out){
    (void)rip; (void)bytes; (void)len; fprintf(out, "\"asm\":\"\"");
}
#endif

static bool va_to_gfn(vmi_instance_t vmi, addr_t dtb, addr_t va, addr_t* out_gfn){
    addr_t pa=0; if(VMI_FAILURE==vmi_pagetable_lookup(vmi, dtb, va, &pa)) return false;
    *out_gfn = pa>>12; return true;
}

#ifndef VMI_EVENT_SINGLESTEP
#define VMI_EVENT_SINGLESTEP VMI_EVENT_X86_SINGLESTEP
#endif

// view id 런타임 획득 (없으면 0)
typedef uint16_t (*get_viewid_fn_t)(vmi_instance_t);
static uint16_t get_current_viewid(vmi_instance_t vmi){
    static get_viewid_fn_t fn = NULL;
    static int inited = 0;
    if(!inited){
        fn = (get_viewid_fn_t)dlsym(RTLD_DEFAULT, "vmi_get_vmm_pagetable_id");
        inited = 1;
    }
    if(fn) return fn(vmi);
    return 0;
}

// ── 콜백 forward ──
static event_response_t on_single_step(vmi_instance_t vmi, vmi_event_t *event);

// ── WRITE 콜백 ──
static event_response_t on_heap_write(vmi_instance_t vmi, vmi_event_t *event){
    const addr_t gla    = event->mem_event.gla;
    const uint32_t vcpu = event->vcpu_id;
    const uint64_t page = (uint64_t)(gla >> 12);

    // 범위 밖: 방해하지 않음
    if(!(gla >= G.va_lo && gla < G.va_hi)) return 0;

    if(!G.tracing || G.stage==ST_IDLE){
        // 시드 WRITE 트리거
        G.tracing    = true;
        G.stage      = ST_ARMED;
        G.t0_us      = now_us();
        G.step_count = 0;

        G.first_page = page;
        G.seed_gfn   = event->mem_event.gfn;
        G.seed_viewid= get_current_viewid(vmi); // 현재 altp2m view

        fprintf(stderr,
            "[W] WRITE @0x%llx vcpu=%u (seed)\n"
            "[*] TRACE start t0=%llu us (vcpu=%u, gfn=0x%llx view=%u)\n",
            (unsigned long long)gla, vcpu,
            (unsigned long long)G.t0_us, vcpu,
            (unsigned long long)G.seed_gfn, (unsigned)G.seed_viewid);

        // EPT 스위치: 이 페이지 알람 OFF → write 통과
        if (VMI_FAILURE == vmi_set_mem_event(vmi, G.seed_gfn, VMI_MEMACCESS_N, G.seed_viewid)) {
            fprintf(stderr,"[!] vmi_set_mem_event(N) failed gfn=0x%llx view=%u\n",
                    (unsigned long long)G.seed_gfn, (unsigned)G.seed_viewid);
        }

        // SS ON을 콜백 밖에서 요청
        G.pending_ss   = +1;
        G.pending_vcpu = vcpu;
        return 0;
    }

    if(G.stage==ST_ARMED){
        // SS 첫 진입 전 추가 write는 무시
        return 0;
    }

    // ST_ACTIVE: 같은 페이지의 다음 WRITE면 종료
    if (page == G.first_page){
        fprintf(stderr, "[W] WRITE @0x%llx vcpu=%u (same page → stop)\n",
                (unsigned long long)gla, vcpu);
        G.pending_ss   = -1;        // SS OFF 요청(콜백 밖)
        G.pending_vcpu = vcpu;
        G.tracing = false;
        G.stage   = ST_IDLE;
        g_stop    = 1;
        return 0;
    }

    return 0;
}

// ── 싱글스텝 콜백 ──
static event_response_t on_single_step(vmi_instance_t vmi, vmi_event_t *event){
    x86_registers_t *r = event->x86_regs;
    addr_t rip = r->rip;
    uint8_t buf[15]={0};
    size_t got = vmi_read_va(vmi, rip, G.dtb, sizeof(buf), buf, NULL);
    uint64_t ts = now_us();

    if(G.stage==ST_ARMED){
        // SS 첫 진입: 시드 페이지에 W-트랩 재장착
        if(G.seed_gfn){
            if(VMI_FAILURE == vmi_set_mem_event(vmi, G.seed_gfn, VMI_MEMACCESS_W, G.seed_viewid)){
                fprintf(stderr,"[!] re-arm W trap failed gfn=0x%llx view=%u\n",
                        (unsigned long long)G.seed_gfn, (unsigned)G.seed_viewid);
            }
        }
        G.stage = ST_ACTIVE;
    }

    // trace.jsonl 기록
    fprintf(G.out, "{");
    fprintf(G.out, "\"ts_us\":%llu,\"vcpu\":%u,",
            (unsigned long long)ts, event->vcpu_id);
    fprintf(G.out, "\"rip\":\"0x%016llx\",",
            (unsigned long long)rip);
    fprintf(G.out, "\"bytes\":\"");
    for(size_t i=0;i<got;i++) fprintf(G.out,"%02X", buf[i]);
    fprintf(G.out, "\",");
    disas_and_write((uint64_t)rip, buf, got, G.out);
    fprintf(G.out, "}\n");

    G.step_count++;
    uint64_t elapsed = ts - G.t0_us;
    if(G.step_count >= G.max_steps || elapsed/1000ULL >= G.max_trace_ms){
        fprintf(stderr, "[*] TRACE stop by limit: steps=%llu, elapsed_ms=%llu\n",
                (unsigned long long)G.step_count, (unsigned long long)(elapsed/1000ULL));
        G.pending_ss   = -1;            // SS OFF 요청
        G.pending_vcpu = event->vcpu_id;
        G.tracing = false;
        G.stage   = ST_IDLE;
        g_stop    = 1;
        return 0;
    }
    return 0;
}

// ── write-watch 장착 (gfn 단위) ──
static bool arm_write_watch(vmi_instance_t vmi){
    memset(&write_ev, 0, sizeof(write_ev));
    write_ev.version  = VMI_EVENTS_VERSION;
    write_ev.type     = VMI_EVENT_MEMORY;
    write_ev.callback = on_heap_write;
    write_ev.mem_event.in_access = VMI_MEMACCESS_W;

    uint64_t page_lo = G.va_lo & ~0xFFFULL;
    uint64_t page_hi = (G.va_hi + 0xFFFULL) & ~0xFFFULL;
    size_t pages=0, ok=0;

    for(uint64_t va=page_lo; va<page_hi; va+=0x1000ULL){
        addr_t gfn=0;
        if(!va_to_gfn(vmi, G.dtb, va, &gfn)){
            fprintf(stderr,"[!] VA->GFN fail for 0x%016llx\n",(unsigned long long)va);
            pages++; continue;
        }
        write_ev.mem_event.gfn = gfn;
        if(VMI_FAILURE==vmi_register_event(vmi, &write_ev)){
            fprintf(stderr,"[!] register MEMORY gfn=0x%llx fail\n",(unsigned long long)gfn);
        }else ok++;
        pages++;
    }

    fprintf(stderr,"[*] Armed write-watch: pages=%zu registered=%zu range=[0x%016llx .. 0x%016llx)\n",
            pages, ok, (unsigned long long)page_lo, (unsigned long long)page_hi);
    return ok>0;
}

static void usage(const char* p){
    fprintf(stderr,"Usage: sudo %s <domain> <pid> <va_start-va_end> [watch_timeout_ms] [max_steps] [max_trace_ms] [out.jsonl]\n", p);
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
    G.max_steps        = (argc>=6)? strtoull(argv[5],NULL,0) : 20000;
    G.max_trace_ms     = (argc>=7)? strtoull(argv[6],NULL,0) : 3000;
    snprintf(G.out_path,sizeof(G.out_path), "%s", (argc>=8)? argv[7] : "trace.jsonl");

    fprintf(stderr,"=== libvmi_heap_write_tracer (PID-only, page-scoped) ===\n");
    fprintf(stderr,"[*] domain=%s pid=%llu range=[0x%016llx .. 0x%016llx) watch_timeout_ms=%llu max_steps=%llu max_trace_ms=%llu out=%s\n",
            G.domain,(unsigned long long)G.pid,(unsigned long long)G.va_lo,(unsigned long long)G.va_hi,
            (unsigned long long)G.watch_timeout_ms,(unsigned long long)G.max_steps,(unsigned long long)G.max_trace_ms,G.out_path);

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

    // SINGLESTEP 핸들러 등록(1회)
    memset(&ss_ev,0,sizeof(ss_ev));
    ss_ev.version  = VMI_EVENTS_VERSION;
    ss_ev.type     = VMI_EVENT_SINGLESTEP;
    ss_ev.callback = on_single_step;
    ss_ev.ss_event.enable = 0;
    ss_ev.ss_event.vcpus  = 0;
    if(VMI_FAILURE==vmi_register_event(G.vmi,&ss_ev)){
        fprintf(stderr,"[!] vmi_register_event(SINGLESTEP) failed\n");
        vmi_destroy(G.vmi); return 1;
    }
    fprintf(stderr,"[+] SINGLESTEP handler registered: cb=%p\n",(void*)ss_ev.callback);

    // 출력 파일
    G.out = fopen(G.out_path,"w");
    if(!G.out){ fprintf(stderr,"[!] open %s failed: %s\n",G.out_path,strerror(errno)); vmi_destroy(G.vmi); return 1; }
    setvbuf(G.out,NULL,_IOFBF,1<<20); // 1MB 버퍼

    // 상태 초기화
    G.tracing=false; G.stage=ST_IDLE;
    G.pending_ss=0;  G.pending_vcpu=0;

    // write-watch 장착
    if(!arm_write_watch(G.vmi)){ fclose(G.out); vmi_destroy(G.vmi); return 1; }

    // 이벤트 루프
    uint64_t start_us = now_us();
    while(!g_stop){
        if(VMI_FAILURE==vmi_events_listen(G.vmi,500)){
            fprintf(stderr,"[!] events_listen failed\n"); break;
        }

        // 콜백 밖에서 SS 토글 처리
        if (G.pending_ss != 0){
            uint32_t v = G.pending_vcpu;
            if (G.pending_ss > 0){
                // ON: ss_ev의 마스크/enable 세팅 후 토글
                ss_ev.ss_event.vcpus  = (1ULL << v);
                ss_ev.ss_event.enable = 1;
                if (VMI_FAILURE == vmi_toggle_single_step_vcpu(G.vmi, &ss_ev, v, 1)) {
                    fprintf(stderr,"[!] vmi_toggle_single_step_vcpu(ON) failed on vcpu=%u\n", v);
                } else {
                    fprintf(stderr,"[*] single-step ENABLED on vcpu=%u (mask=0x%llx)\n",
                            v, (unsigned long long)ss_ev.ss_event.vcpus);
                }
                // 재개 보장
                vmi_resume_vm(G.vmi);
            }else{
                // OFF
                ss_ev.ss_event.vcpus  = (1ULL << v);
                ss_ev.ss_event.enable = 0;
                if (VMI_FAILURE == vmi_toggle_single_step_vcpu(G.vmi, &ss_ev, v, 0)) {
                    fprintf(stderr,"[!] vmi_toggle_single_step_vcpu(OFF) failed on vcpu=%u\n", v);
                } else {
                    fprintf(stderr,"[*] single-step DISABLED on vcpu=%u\n", v);
                }
                vmi_resume_vm(G.vmi);
            }
            G.pending_ss = 0;
        }

        if(!G.tracing){
            uint64_t elapsed_ms=(now_us()-start_us)/1000ULL;
            if(elapsed_ms>=G.watch_timeout_ms){
                fprintf(stderr,"[*] Watch timeout (%llu ms)\n",(unsigned long long)elapsed_ms);
                break;
            }
        }
    }

    fclose(G.out);
    vmi_destroy(G.vmi);
#ifdef HAVE_CAPSTONE
    if(cs_ready) cs_close(&cs_handle);
#endif
    fprintf(stderr,"[*] Done. Output: %s\n", G.out_path);
    return 0;
}
