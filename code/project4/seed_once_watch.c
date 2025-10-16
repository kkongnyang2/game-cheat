// seed_once_watch.c — LibVMI seed page once write watcher (PID-only, page-scoped)
// 목표: 지정 VA 범위의 페이지들에 대해 "첫 WRITE" 발생 시 메시지 출력 후 종료.
//  - 인자/출력 포맷은 네가 준 main.c의 스타일을 최대한 유지
//  - altp2m/싱글스텝/디스어셈블 등 고급 추적은 모두 제외 (최소 기능)
//
// Usage:
//   sudo ./seed_once_watch <domain> <pid> <va_lo-va_hi> [watch_timeout_ms]
//
// Build:
//   gcc -O2 -Wall -Wextra -std=gnu11 seed_once_watch.c -o seed_once_watch 
//     $(pkg-config --cflags libvmi glib-2.0) 
//     $(pkg-config --libs   libvmi glib-2.0) 
//     -Wl,--no-as-needed

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <inttypes.h>
#include <dlfcn.h>

#include <libvmi/libvmi.h>
#include <libvmi/events.h>

typedef struct {
    char     domain[128];
    uint64_t pid;
    uint64_t va_lo, va_hi;

    uint64_t watch_timeout_ms; // 시드 대기 타임아웃

    vmi_instance_t vmi;
    addr_t  dtb;

    // 시드 정보 기록용
    uint64_t first_page;   // (gla>>12)
    addr_t   seed_gfn;
    uint16_t seed_viewid;

    // 종료 트리거
    int      triggered;    // 0=미발생, 1=첫 write 감지
} once_t;

static once_t G;

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

// altp2m view id 런타임 획득(없으면 0)
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

static event_response_t on_seed_write(vmi_instance_t vmi, vmi_event_t *event){
    const addr_t gla    = event->mem_event.gla;
    const uint32_t vcpu = event->vcpu_id;
    const uint64_t page = (uint64_t)(gla >> 12);

    // 범위 밖이면 무시
    if(!(gla >= G.va_lo && gla < G.va_hi)) return 0;

    if(!G.triggered){
        G.triggered   = 1;
        G.first_page  = page;
        G.seed_gfn    = event->mem_event.gfn;
        G.seed_viewid = get_current_viewid(vmi);

        fprintf(stderr,
            "[W] WRITE @0x%llx vcpu=%u (seed)\n"
            "[*] TRACE start (gfn=0x%llx view=%u)\n",
            (unsigned long long)gla, vcpu,
            (unsigned long long)G.seed_gfn, G.seed_viewid);

        // 여기서는 write를 통과시키고(차단 안함) 메인루프에서 종료
        // out_access를 건드리지 않으면 기본적으로 통과
        g_stop = 1;
    }
    return 0; // VM 계속 진행
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

    fprintf(stderr,"[*] Armed write-watch: pages=%zu registered=%zu range=[0x%016llx .. 0x%016llx)\n",
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

    fprintf(stderr,"=== seed_once_write_watch (PID-only, page-scoped) ===\n");
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

    // write-watch 장착
    vmi_event_t write_ev;
    if(arm_write_watch(G.vmi, &write_ev)!=0){
        vmi_destroy(G.vmi); return 1;
    }

    // 이벤트 루프: 첫 write 감지 시 종료
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

    // 정리
    vmi_destroy(G.vmi);
    fprintf(stderr,"[*] Done. %s\n", G.triggered ? "(seed write detected)" : "(no write)");
    return 0;
}
