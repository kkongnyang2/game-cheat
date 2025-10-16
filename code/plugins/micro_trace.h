#pragma once
#include "plugins.h"
#include <vector>
#include <string>
#include <cstdint>
#include <cstdlib>

class micro_trace final : public plugin
{
public:
    micro_trace(drakvuf_t drakvuf, output_format_t output);

private:
    drakvuf_t        drakvuf_;

    // plugin virtuals
    bool start_impl();
    bool stop_impl()  override;

    // traps
    static event_response_t memaccess_cb(drakvuf_t d, drakvuf_trap_info_t* info);
    static event_response_t step_observer_cb(drakvuf_t d, drakvuf_trap_info_t* info, void* user);

    // helpers
    void log_json(const char* kind, const char* fmt, ...) const;
    bool parse_env();
    void add_gfn_watch(uint64_t gfn);
    bool match_seed(uint64_t gfn, uint32_t off) const;
    void begin_trace(uint32_t vcpu);
    void end_trace(drakvuf_t d, uint64_t ts_end);
    static uint32_t page_off(addr_t pa) { return (uint32_t)(pa & 0xfffULL); }

    // config (env)
    uint32_t  pid_          = 0;                 // optional
    std::vector<uint64_t> gfns_;                 // watch 대상 GFN들
    uint64_t  seed_gfn_     = 0;                 // “관심 시드” GFN (필수/권장)
    uint32_t  seed_off_     = 0xffffffffu;       // 페이지 내 오프셋 (옵션; ANY = 0xffffffff)
    uint32_t  step_cap_     = 20000;             // 최대 스텝 수
    uint32_t  duration_ms_  = 0;                 // (옵션) 시간 제한: 미사용/보류

    // runtime
    struct trace_item { uint64_t ts; uint64_t rip; };
    std::vector<trace_item> trace_;
    std::vector<drakvuf_trap_t*> traps_;
    bool      tracing_      = false;
    uint32_t  tracing_vcpu_ = 0;
    uint32_t  stepped_      = 0;

    // misc
    output_format_t output_;
};
