#include "pid_va_writewatch.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "libdrakvuf/libdrakvuf.h" // drakvuf_lock_and_get_vmi, drakvuf_release_vmi

// --- range 파서 유틸 ---
static bool parse_range_tok(const char* s, uint64_t& lo, uint64_t& hi)
{
    char* end=nullptr;
    std::string tok(s);
    auto pos = tok.find('-');
    if (pos==std::string::npos) return false;
    lo = strtoull(tok.substr(0,pos).c_str(), &end, 0);
    if (*end) return false;
    hi = strtoull(tok.substr(pos+1).c_str(), &end, 0);
    if (*end || hi<=lo) return false;
    lo &= ~0xFFFULL; hi = (hi + 0xFFFULL) & ~0xFFFULL;
    return true;
}

static std::vector<va_range_t> get_ranges_from_env()
{
    std::vector<va_range_t> out;
    const char* e = getenv("WRITEWATCH_RANGE"); // "0xA-0xB,0xC-0xD"
    if (!e || !*e) return out;
    std::string s(e);
    size_t p=0;
    while (p < s.size()) {
        size_t q = s.find(',', p); if (q==std::string::npos) q = s.size();
        uint64_t lo=0, hi=0;
        if (parse_range_tok(s.substr(p, q-p).c_str(), lo, hi)) out.push_back({lo, hi});
        p = q + 1;
    }
    return out;
}

// --- 클래스 구현 ---
pid_va_writewatch::pid_va_writewatch(drakvuf_t drakvuf, output_format_t output)
: plugin(), drakvuf_(drakvuf), output_(output)
{
    const char* epid = getenv("WRITEWATCH_PID");
    if (!epid || !*epid) {
        fprintf(stderr, "[writewatch] set WRITEWATCH_PID & WRITEWATCH_RANGE\n");
        return;
    }
    pid_ = (uint32_t)strtoul(epid, nullptr, 0);
    ranges_ = get_ranges_from_env();
    if (ranges_.empty()) {
        fprintf(stderr, "[writewatch] invalid WRITEWATCH_RANGE\n");
        return;
    }
    if (!init_all()) {
        fprintf(stderr, "[writewatch] init failed\n");
    } else {
        printf("[writewatch] PID=%u ranges=%zu armed\n", pid_, ranges_.size());
        fflush(stdout);
    }
}

pid_va_writewatch::~pid_va_writewatch() { stop_impl(); }

bool pid_va_writewatch::stop_impl()
{
    for (auto& kv : pages_) {
        if (kv.second.trap) {
            drakvuf_remove_trap(drakvuf_, kv.second.trap, nullptr);
            g_free(kv.second.trap);
        }
    }
    pages_.clear();
    return true;
}

bool pid_va_writewatch::resolve_dtb()
{
    auto vmi = drakvuf_lock_and_get_vmi(drakvuf_);
    bool ok = (VMI_SUCCESS == vmi_pid_to_dtb(vmi, (vmi_pid_t)pid_, &dtb_));
    drakvuf_release_vmi(drakvuf_);
    return ok;
}

bool pid_va_writewatch::gva_to_gfn(uint64_t cr3, uint64_t gva, uint64_t& gfn)
{
    addr_t pa=0;
    auto vmi = drakvuf_lock_and_get_vmi(drakvuf_);
    bool ok = (VMI_SUCCESS == vmi_pagetable_lookup(vmi, cr3, gva, &pa));
    drakvuf_release_vmi(drakvuf_);
    if (!ok) return false;
    gfn = (uint64_t)(pa >> 12);
    return true;
}

bool pid_va_writewatch::watch_page(uint64_t gva_page)
{
    if (pages_.count(gva_page)) return true;
    uint64_t gfn=0;
    if (!gva_to_gfn(dtb_, gva_page, gfn)) return false;

    auto* t = (drakvuf_trap_t*)g_malloc0(sizeof(drakvuf_trap_t));
    t->type = MEMACCESS;
    t->name = "pid_va_writewatch";
    t->cb   = &pid_va_writewatch::memaccess_cb;
    t->memaccess.gfn    = gfn;
    t->memaccess.access = VMI_MEMACCESS_W;
    t->data = this;

    if (!drakvuf_add_trap(drakvuf_, t)) { g_free(t); return false; }
    pages_.emplace(gva_page, tracked_page_t{gva_page, gfn, t});
    return true;
}

void pid_va_writewatch::unwatch_page(uint64_t gva_page)
{
    auto it = pages_.find(gva_page);
    if (it == pages_.end()) return;
    if (it->second.trap) {
        drakvuf_remove_trap(drakvuf_, it->second.trap, nullptr);
        g_free(it->second.trap);
    }
    pages_.erase(it);
}

bool pid_va_writewatch::ensure_reinsert(uint64_t gva_page, uint64_t cr3)
{
    auto it = pages_.find(gva_page);
    if (it == pages_.end()) return false;
    uint64_t new_gfn=0;
    if (!gva_to_gfn(cr3, gva_page, new_gfn)) return false;
    if (new_gfn == it->second.gfn) return true;

    drakvuf_remove_trap(drakvuf_, it->second.trap, nullptr);
    g_free(it->second.trap);

    auto* t = (drakvuf_trap_t*)g_malloc0(sizeof(drakvuf_trap_t));
    t->type = MEMACCESS;
    t->name = "pid_va_writewatch";
    t->cb   = &pid_va_writewatch::memaccess_cb;
    t->memaccess.gfn    = new_gfn;
    t->memaccess.access = VMI_MEMACCESS_W;
    t->data = this;

    if (!drakvuf_add_trap(drakvuf_, t)) return false;
    it->second.gfn  = new_gfn;
    it->second.trap = t;
    return true;
}

bool pid_va_writewatch::init_all()
{
    if (!resolve_dtb()) return false;
    for (auto& r : ranges_) {
        for (uint64_t gva = r.lo; gva < r.hi; gva += 0x1000ULL)
            (void)watch_page(gva);
    }
    return true;
}

event_response_t pid_va_writewatch::memaccess_cb(drakvuf_t, drakvuf_trap_info_t* info)
{
    auto* self = static_cast<pid_va_writewatch*>(info->trap->data);
    if (!self) return VMI_EVENT_RESPONSE_NONE;
    if ((uint32_t)info->proc_data.pid != self->pid_) return VMI_EVENT_RESPONSE_NONE;

    // trap 포인터로 우리가 등록한 GVA 페이지를 찾는다(O(n)이나 초기에 충분)
    uint64_t gva_page = 0;
    for (const auto& kv : self->pages_) {
        if (kv.second.trap == info->trap) { gva_page = kv.first; break; }
    }
    const uint64_t rip = info->regs->rip;

    printf("[WRITEWATCH] PID:%u TID:%u RIP:0x%llx GVA:0x%llx\n",
           (unsigned)self->pid_, (unsigned)info->proc_data.tid,
           (unsigned long long)rip, (unsigned long long)gva_page);
    fflush(stdout);

    if (gva_page) self->ensure_reinsert(gva_page, info->regs->cr3);
    return VMI_EVENT_RESPONSE_NONE;
}
