#include "pfn_writewatch.h"

#include <algorithm>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef VMI_MEMACCESS_W
# define VMI_MEMACCESS_W ((vmi_mem_access_t)2)
#endif

static inline uint64_t to_pa(uint64_t pfn) { return pfn << 12; }

uint64_t pfn_writewatch::now_ns() {
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec*1000000000ull + (uint64_t)ts.tv_nsec;
}

pfn_writewatch::pfn_writewatch(drakvuf_t drakvuf, output_format_t /*output*/)
: plugin(), drakvuf_(drakvuf) {
    start_ns_ = now_ns();

    // VMI 핸들 확보 (이 버전의 정석 경로)
    vmi_ = drakvuf_lock_and_get_vmi(this->drakvuf_);

    // ---- guards ----
    if (const char* s = getenv("WRITEWATCH_MAX_EVENTS")) max_events_ = strtoull(s, nullptr, 10);
    if (const char* s = getenv("WRITEWATCH_SECONDS"))    timeout_s_  = strtoull(s, nullptr, 10);

    // ---- modes ----
    if (const char* s = getenv("WRITEWATCH_MODE")) {
        // diff | peek | both
        if (!g_ascii_strcasecmp(s, "diff")) { do_diff_ = true;  do_peek_ = false; }
        else if (!g_ascii_strcasecmp(s, "peek")) { do_diff_ = false; do_peek_ = true; }
        else { do_diff_ = true; do_peek_ = true; } // both/anything
    }
    if (const char* s = getenv("WRITEWATCH_MAX_CHANGES")) max_changes_ = (uint32_t)strtoul(s, nullptr, 10);
    if (const char* s = getenv("WRITEWATCH_PEEK"))        peek_len_    = (uint32_t)strtoul(s, nullptr, 10);
    if (peek_len_ > 256) peek_len_ = 256;

    // ---- PFN list ----
    if (const char* list = getenv("WRITEWATCH_PFNS")) {
        char* dup = g_strdup(list);
        char* save = nullptr;
        for (char* tok = strtok_r(dup, ",", &save); tok; tok = strtok_r(nullptr, ",", &save)) {
            while (*tok==' '||*tok=='\t') ++tok;
            if (tok[0]=='0' && (tok[1]=='x'||tok[1]=='X')) tok+=2;
            uint64_t pfn = strtoull(tok, nullptr, 16);
            add_pfn(pfn);
        }
        g_free(dup);
    }

    // ---- PFN range ----
    if (const char* range = getenv("WRITEWATCH_PFN_RANGE")) {
        char* dup = g_strdup(range);
        char* dash = strchr(dup, '-');
        if (dash) {
            *dash = 0;
            const char* s1 = dup;
            const char* s2 = dash+1;
            if (s1[0]=='0' && (s1[1]=='x'||s1[1]=='X')) s1+=2;
            if (s2[0]=='0' && (s2[1]=='x'||s2[1]=='X')) s2+=2;
            uint64_t a = strtoull(s1, nullptr, 16);
            uint64_t b = strtoull(s2, nullptr, 16);
            if (a > b) std::swap(a,b);
            // 안전 상한 (너무 길면 arm 비용 큼)
            const uint64_t LIMIT = 1ULL<<15; // 32768 PFNs
            uint64_t cnt = (b>=a)? (b-a+1):0;
            if (cnt > LIMIT) b = a + LIMIT - 1;
            for (uint64_t p=a; p<=b; ++p) add_pfn(p);
        }
        g_free(dup);
    }

    if (!traps_) {
        fprintf(stderr, "[pfn_writewatch] no PFNs (set WRITEWATCH_PFNS or WRITEWATCH_PFN_RANGE)\n");
        throw -1;
    }
    fprintf(stderr, "[pfn_writewatch] armed %u PFN(s) mode=%s peek=%u maxchg=%u\n",
            g_slist_length(traps_),
            do_diff_&&do_peek_?"both":(do_diff_?"diff":"peek"),
            peek_len_, max_changes_);
}

pfn_writewatch::~pfn_writewatch() {
    remove_all();
    if (vmi_) { drakvuf_release_vmi(drakvuf_); vmi_ = nullptr; }
}

bool pfn_writewatch::stop_impl() {
    remove_all();
    if (vmi_) { drakvuf_release_vmi(drakvuf_); vmi_ = nullptr; }
    return true;
}

void pfn_writewatch::add_pfn(uint64_t pfn) {
    auto* t = (drakvuf_trap_t*)g_malloc0(sizeof(drakvuf_trap_t));
    t->type = MEMACCESS;                 // 이 드라크뷰프 버전의 메모리 트랩 식별자
    t->cb   = &pfn_writewatch::on_mem;
    t->data = this;
    t->name = (char*)"pfn_writewatch";

    // PFN(EPT GFN)을 write-POST로 arm
    t->memaccess.gfn    = pfn;
    t->memaccess.access = VMI_MEMACCESS_W; // write
    t->memaccess.type   = POST;            // 쓴 뒤 상태를 관찰 (diff/peek용)

    if (!drakvuf_add_trap(drakvuf_, t)) {
        fprintf(stderr, "[pfn_writewatch] add_trap failed PFN=0x%llx\n", (unsigned long long)pfn);
        g_free(t);
        return;
    }
    traps_ = g_slist_prepend(traps_, t);
}

void pfn_writewatch::remove_all() {
    for (GSList* it=traps_; it; it=it->next) {
        drakvuf_trap_t* t = (drakvuf_trap_t*)it->data;
        drakvuf_remove_trap(drakvuf_, t, nullptr);
        g_free(t);
    }
    g_slist_free(traps_);
    traps_ = nullptr;
}

void pfn_writewatch::log_event_diff(uint64_t pfn, const uint8_t* curr, size_t len, drakvuf_trap_info_t* info) {
    auto& prev = snap_[pfn];
    if (prev.empty()) {
        prev.assign(curr, curr+len);
        return; // 첫 이벤트는 baseline만 저장
    }

    struct change { uint16_t off; uint8_t oldb, newb; };
    change diffs[256];
    unsigned n = 0, total = 0;

    for (unsigned i=0; i<len; ++i) {
        if (prev[i] != curr[i]) {
            if (n < max_changes_) diffs[n++] = {(uint16_t)i, prev[i], curr[i]};
            ++total;
        }
    }
    prev.assign(curr, curr+len);

    if (!n && !do_peek_) return;

    // 공통 메타
    printf("{\"ts\":%" PRIu64 ",\"vcpu\":%u,\"cr3\":\"0x%llx\",\"rip\":\"0x%llx\",\"pfn\":\"0x%llx\",\"nchanges\":%u",
           now_ns(), info->vcpu,
           (unsigned long long)info->regs->cr3,
           (unsigned long long)info->regs->rip,
           (unsigned long long)pfn, total);

    // diff 목록(최대 max_changes_)
    if (n) {
        printf(",\"changes\":[");
        for (unsigned i=0;i<n;i++) {
            printf("%s{\"off\":%u,\"old\":\"%02x\",\"new\":\"%02x\"}",
                   (i?",":""), diffs[i].off, diffs[i].oldb, diffs[i].newb);
        }
        printf("]");
    }

    // 주변 바이트 peek (옵션)
    if (do_peek_ && n) {
        printf(",\"blobs\":[");
        for (unsigned i=0;i<n;i++) {
            uint32_t off = diffs[i].off;
            uint32_t half = peek_len_/2;
            uint32_t s = (off>half)? (off-half):0;
            if (s + peek_len_ > len) s = (len>peek_len_)? (len-peek_len_):0;

            printf("%s{\"off\":%u,\"hex\":\"", (i?",":""), s);
            for (uint32_t k=0; k<peek_len_ && (s+k)<len; ++k) printf("%02x", curr[s+k]);
            printf("\"}");
        }
        printf("]");
    }

    printf("}\n");
    fflush(stdout);
}

event_response_t pfn_writewatch::on_mem(drakvuf_t /*drakvuf*/, drakvuf_trap_info_t* info) {
    auto* self = (pfn_writewatch*)info->trap->data;
    self->events_++;

    uint64_t pfn = info->trap->memaccess.gfn;
    uint8_t page[4096];
    const addr_t pa = (addr_t)to_pa(pfn);

    size_t br = 0;
    // libvmi: vmi_read_pa(vmi, paddr, count, buf, &bytes_read)
    if ( VMI_SUCCESS != vmi_read_pa(self->vmi_, pa, sizeof(page), page, &br) || br != sizeof(page) ) {
        // 읽기 실패 시 스킵 (가드만 적용)
    } else {
        if (self->do_diff_ || self->do_peek_)
            self->log_event_diff(pfn, page, sizeof(page), info);
    }

    // storm guard / timeout
    if (self->max_events_ && self->events_ >= self->max_events_) {
        fprintf(stderr, "[pfn_writewatch] event cap reached (%" PRIu64 ")\n", self->events_);
        drakvuf_interrupt(self->drakvuf_, 0);
        return VMI_EVENT_RESPONSE_NONE;
    }
    if (self->timeout_s_) {
        if (now_ns() - self->start_ns_ >= self->timeout_s_*1000000000ull) {
            fprintf(stderr, "[pfn_writewatch] timeout reached (%" PRIu64 " s)\n", self->timeout_s_);
            drakvuf_interrupt(self->drakvuf_, 0);
            return VMI_EVENT_RESPONSE_NONE;
        }
    }
    return 0;
}
