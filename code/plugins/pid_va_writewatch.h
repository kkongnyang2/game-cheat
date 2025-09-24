#pragma once
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "plugins.h"   // plugin base (C++)
extern "C" {
#include <libvmi/libvmi.h>  // C 헤더만 extern "C"로 감싸기
}
#include "drakvuf.h"  // C++ 헤더는 extern "C" 바깥

struct va_range_t { uint64_t lo, hi; }; // [lo, hi)

struct tracked_page_t {
    uint64_t        gva_page;  // page-aligned GVA
    uint64_t        gfn;       // current GFN
    drakvuf_trap_t* trap;      // MEMACCESS trap handle
};

class pid_va_writewatch : public plugin
{
public:
    // plugins.cpp의 switch-case에서 이 시그니처로 생성됨
    pid_va_writewatch(drakvuf_t drakvuf, output_format_t output);
    ~pid_va_writewatch() override;

protected:
    bool stop_impl() override;

private:
    bool init_all();

    // helpers
    bool resolve_dtb();                                 // pid -> dtb
    bool gva_to_gfn(uint64_t cr3, uint64_t gva, uint64_t& gfn);
    bool watch_page(uint64_t gva_page);
    void unwatch_page(uint64_t gva_page);
    bool ensure_reinsert(uint64_t gva_page, uint64_t cr3);

    // trap callback
    static event_response_t memaccess_cb(drakvuf_t, drakvuf_trap_info_t*);

private:
    drakvuf_t       drakvuf_;
    [[maybe_unused]] output_format_t output_; // 현재는 사용 안 하므로 경고 억제
    uint32_t        pid_ = 0;
    uint64_t        dtb_ = 0;
    std::vector<va_range_t>                      ranges_;
    std::unordered_map<uint64_t, tracked_page_t> pages_; // key: gva_page
};
