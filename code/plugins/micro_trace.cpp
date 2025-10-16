#include "micro_trace.h"
#include <cstdio>
#include <cinttypes>
#include <cstring>
#include <algorithm>

// 이 분기(libdrakvuf)에 맞춘 심볼/필드 사용:
// - memaccess 트랩은 trap->memaccess.{gfn,access,type}
// - info->trap_pa, info->vcpu, info->regs 가 유효
// - PRE/POST 및 VMI_MEMACCESS_W 상수 존재
// - 싱글스텝 유지/종료는 event_response_t 비트: VMI_EVENT_RESPONSE_TOGGLE_SINGLESTEP
// - 우리가 추가한 API: drakvuf_register_step_observer / drakvuf_unregister_step_observer

// --------- 유틸 ----------

[[maybe_unused]] static inline uint64_t now_ns()
{
    // drakvuf_trap_info_t.timestamp 가 us 기준이면 그대로 써도 되고,
    // 여기서는 별도 필요시 g_get_real_time() 등을 쓸 수 있으나 info->timestamp를 주로 사용
    return 0;
}

// REUSE env 이름들: 기존 WRITEWATCH_* 와 새 MICROTRACE_* 둘 다 받도록 함
static const char* getenv2(const char* a, const char* b)
{
    const char* v = std::getenv(a);
    if (v && *v) return v;
    v = std::getenv(b);
    return (v && *v) ? v : nullptr;
}

static bool parse_hex_u64(const char* s, uint64_t& out)
{
    if (!s) return false;
    char* end = nullptr;
    uint64_t v = strtoull(s, &end, 16);
    if (end==s) return false;
    out = v;
    return true;
}

static void split_tokens(const char* s, char sep, std::vector<std::string>& out)
{
    if (!s) return;
    const char* p = s;
    while (*p)
    {
        while (*p==sep || *p==' ' || *p=='\t') ++p;
        if (!*p) break;
        const char* q = p;
        while (*q && *q!=sep) ++q;
        out.emplace_back(p, q-p);
        p = q;
        if (*p==sep) ++p;
    }
}

// ------------------------

micro_trace::micro_trace(drakvuf_t drakvuf, output_format_t output)
    : drakvuf_(drakvuf), output_(output)
{
    // ...
}

void micro_trace::log_json(const char* kind, const char* fmt, ...) const
{
    va_list ap;
    va_start(ap, fmt);
    if (output_ == OUTPUT_JSON)
    {
        // 간단 JSON 라인: {"micro_trace":"kind", "msg":"..."}
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, ap);
        printf("{\"micro_trace\":\"%s\",\"msg\":\"%s\"}\n", kind, buf);
    }
    else
    {
        vprintf(fmt, ap);
        printf("\n");
    }
    va_end(ap);
}

bool micro_trace::parse_env()
{
    // PID (옵션)
    if (const char* s = getenv2("MICROTRACE_PID","WRITEWATCH_PID"))
    {
        pid_ = (uint32_t)strtoul(s, nullptr, 0);
    }

    // STEP CAP
    if (const char* s = getenv2("MICROTRACE_STEP_MAX","WRITEWATCH_STEP_COUNT"))
    {
        step_cap_ = (uint32_t)strtoul(s, nullptr, 0);
        if (step_cap_ == 0) step_cap_ = 20000;
    }

    // DURATION (미사용/보류: 코어에서 타이머 훅 없으면 직접 체크 필요)
    if (const char* s = getenv2("MICROTRACE_DURATION_MS","WRITEWATCH_DURATION_MS"))
    {
        duration_ms_ = (uint32_t)strtoul(s, nullptr, 0);
    }

    // 시드: GFN / 오프셋
    if (const char* s = getenv2("MICROTRACE_SEED_GFN", "WRITEWATCH_SEED_GFN"))
    {
        uint64_t gfn = 0;
        if (parse_hex_u64(s, gfn)) seed_gfn_ = gfn;
    }
    if (const char* s = getenv2("MICROTRACE_SEED_OFF", "WRITEWATCH_SEED_OFF"))
    {
        seed_off_ = (uint32_t)strtoul(s, nullptr, 0);
    }

    // 감시할 GFN 목록 (콤마 구분, 범위 "A-B" 허용)
    if (const char* s = getenv2("MICROTRACE_PFNS", "WRITEWATCH_PFNS"))
    {
        std::vector<std::string> toks;
        split_tokens(s, ',', toks);
        for (auto& t: toks)
        {
            // 공백 제거
            std::string x = t;
            x.erase(std::remove_if(x.begin(), x.end(), ::isspace), x.end());
            if (x.empty()) continue;

            // 범위?
            auto dash = x.find('-');
            if (dash == std::string::npos)
            {
                uint64_t v = 0;
                // 0x.. or decimal 모두 허용
                if (x.size()>2 && (x[0]=='0' && (x[1]=='x' || x[1]=='X')))
                    v = strtoull(x.c_str(), nullptr, 16);
                else
                    v = strtoull(x.c_str(), nullptr, 0);
                if (v) gfns_.push_back(v);
            }
            else
            {
                std::string a = x.substr(0, dash);
                std::string b = x.substr(dash+1);
                uint64_t va=0, vb=0;
                va = (a.size()>2 && a[0]=='0' && (a[1]=='x' || a[1]=='X')) ? strtoull(a.c_str(), nullptr, 16) : strtoull(a.c_str(), nullptr, 0);
                vb = (b.size()>2 && b[0]=='0' && (b[1]=='x' || b[1]=='X')) ? strtoull(b.c_str(), nullptr, 16) : strtoull(b.c_str(), nullptr, 0);
                if (va && vb && vb>=va)
                {
                    for (uint64_t g=va; g<=vb; ++g) gfns_.push_back(g);
                }
            }
        }
    }

    // 최소 한 개는 있어야 trap 가능. seed_gfn_만 있어도 동작
    if (gfns_.empty() && seed_gfn_) gfns_.push_back(seed_gfn_);
    if (gfns_.empty())
    {
        log_json("error", "No GFNs given. Set MICROTRACE_PFNS or MICROTRACE_SEED_GFN.");
        return false;
    }
    return true;
}

bool micro_trace::start_impl()
{
    if (!parse_env()) return false;

    log_json("start", "micro_trace start (pid=%u, gfns=%zu, seed_gfn=0x%llx, seed_off=%s, step_cap=%u)",
        pid_, gfns_.size(),
        (unsigned long long)seed_gfn_,
        (seed_off_==0xffffffffu ? "ANY" : "set"),
        step_cap_);

    // 각 GFN에 PRE-W trap 설치
    for (auto g: gfns_) add_gfn_watch(g);

    return true;
}

bool micro_trace::stop_impl()
{
    // 트랩 해제
    for (auto t: traps_)
    {
        if (t) {
            drakvuf_remove_trap(drakvuf_, t, nullptr);
            g_free(t);
        }
    }
    traps_.clear();

    // 스텝 옵저버 해제
    if (tracing_) {
        drakvuf_unregister_step_observer(drakvuf_, tracing_vcpu_);
        tracing_ = false;
    }
    return true;
}

void micro_trace::add_gfn_watch(uint64_t gfn)
{
    drakvuf_trap_t* t = (drakvuf_trap_t*)g_malloc0(sizeof(drakvuf_trap_t));
    t->cb = &micro_trace::memaccess_cb;
    t->data = this;
    t->memaccess.gfn    = gfn;
    t->memaccess.type   = PRE;
    t->memaccess.access = VMI_MEMACCESS_W; // write
    if (!drakvuf_add_trap(drakvuf_, t))
    {
        log_json("warn", "failed to add memaccess trap @gfn=%llu", (unsigned long long)gfn);
        g_free(t);
        return;
    }
    traps_.push_back(t);
}

bool micro_trace::match_seed(uint64_t gfn, uint32_t off) const
{
    if (!seed_gfn_) return false;
    if (gfn != seed_gfn_) return false;
    if (seed_off_ == 0xffffffffu) return true; // ANY offset in the page
    return off == seed_off_;
}

void micro_trace::begin_trace(uint32_t vcpu)
{
    tracing_ = true;
    tracing_vcpu_ = vcpu;
    stepped_ = 0;
    trace_.clear();

    // 옵저버 등록
    drakvuf_register_step_observer(drakvuf_, tracing_vcpu_, &micro_trace::step_observer_cb, this);
}

void micro_trace::end_trace(drakvuf_t d, uint64_t ts_end)
{
    // JSON 덤프 (간단한 한 블록)
    if (output_ == OUTPUT_JSON)
    {
        printf("{\"micro_trace\":\"trace\",\"count\":%zu,\"vcpu\":%u,\"seed_gfn\":%llu,\"seed_off\":%s,\"items\":[",
               trace_.size(), tracing_vcpu_,
               (unsigned long long)seed_gfn_,
               (seed_off_==0xffffffffu?"null":"\"set\""));
        for (size_t i=0;i<trace_.size();++i)
        {
            if (i) printf(",");
            printf("{\"rip\":%llu,\"ts\":%llu}",
                   (unsigned long long)trace_[i].rip,
                   (unsigned long long)trace_[i].ts);
        }
        printf("]}\n");
    }
    else
    {
        log_json("trace", "count=%zu vcpu=%u", trace_.size(), tracing_vcpu_);
        for (auto& it : trace_) {
            printf("  rip=0x%llx ts=%llu\n",
                   (unsigned long long)it.rip,
                   (unsigned long long)it.ts);
        }
    }

    // 옵저버 해제
    drakvuf_unregister_step_observer(d, tracing_vcpu_);
    tracing_ = false;
    trace_.clear();
    stepped_ = 0;
}

// static
event_response_t micro_trace::step_observer_cb(drakvuf_t d, drakvuf_trap_info_t* info, void* user)
{
    auto self = (micro_trace*)user;
    if (!self->tracing_) return 0;
    if (info->vcpu != self->tracing_vcpu_) return 0;

    // RIP 수집
    uint64_t rip = info->regs ? info->regs->rip : 0;
    self->trace_.push_back({ static_cast<uint64_t>(info->timestamp), rip });
    self->stepped_++;

    // step cap 넘으면 종료
    if (self->stepped_ >= self->step_cap_)
    {
        // 여기서 0을 리턴하면 코어가 singlestep OFF
        // 종료 마무리는 다음 write에서 end_trace 하거나,
        // 여기서 바로 마감해도 된다. (이번엔 보수적으로 여기선 OFF만 시키고 마감은 write때)
        return 0;
    }

    // 계속 한 스텝 더
    return VMI_EVENT_RESPONSE_TOGGLE_SINGLESTEP;
}

// static
event_response_t micro_trace::memaccess_cb(drakvuf_t d, drakvuf_trap_info_t* info)
{
    auto self = (micro_trace*)info->trap->data;

    // PID 필터(옵션): 이 분기에서는 info->proc_data / attached_proc_data 둘 중 OS별로 다름
    // 안전하게 둘 다 검사
    vmi_pid_t pid_a = info->proc_data.pid;
    vmi_pid_t pid_b = info->attached_proc_data.pid;
    if (self->pid_ && pid_a != (vmi_pid_t)self->pid_ && pid_b != (vmi_pid_t)self->pid_)
        return 0;

    uint64_t gfn = (info->trap_pa >> 12);
    uint32_t off = page_off(info->trap_pa);

    // 트레이스 미시작: 시드 첫 write면 시작
    if (!self->tracing_ && self->match_seed(gfn, off))
    {
        self->log_json("hit", "seed first write @ gfn=%llu off=0x%x vcpu=%u rip=0x%llx",
                       (unsigned long long)gfn, off, info->vcpu,
                       (unsigned long long)(info->regs?info->regs->rip:0));

        self->begin_trace(info->vcpu);

        // 다음 인스트럭션부터 싱글스텝
        return VMI_EVENT_RESPONSE_TOGGLE_SINGLESTEP;
    }

    // 트레이스 중: 동일 시드에 대한 "다음 write"면 종료
    if (self->tracing_ && self->match_seed(gfn, off))
    {
        self->log_json("hit", "seed next write -> end trace (steps=%u)", self->stepped_);
        self->end_trace(d, info->timestamp);
        // 스텝은 옵저버가 0을 리턴하는 순간 OFF. 방금 종료한 뒤엔 더 이상 keep 스텝 필요 없음.
        return 0;
    }

    // 그 외 write: 무시
    return 0;
}
