#pragma once
#include <plugins/plugins.h>
#include <drakvuf.h>
#include <glib.h>
#include <stdint.h>
#include <unordered_map>
#include <vector>

extern "C" {
#include <libvmi/libvmi.h>      // ★ 추가/복구
}

class pfn_writewatch : public plugin {
public:
    pfn_writewatch(drakvuf_t drakvuf, output_format_t output);
    ~pfn_writewatch();
    bool stop_impl() override;

private:
    static event_response_t on_mem(drakvuf_t drakvuf, drakvuf_trap_info_t* info);
    void add_pfn(uint64_t pfn);
    void remove_all();

    static uint64_t now_ns();
    void log_event_diff(uint64_t pfn, const uint8_t* curr, size_t len, drakvuf_trap_info_t* info);

    drakvuf_t        drakvuf_;
    vmi_instance_t   vmi_ = nullptr;        // ★ 복구
    GSList*          traps_ = nullptr;

    std::unordered_map<uint64_t, std::vector<uint8_t>> snap_;

    bool     do_diff_  = true;
    bool     do_peek_  = true;
    uint32_t max_changes_ = 16;
    uint32_t peek_len_    = 16;

    uint64_t start_ns_   = 0;
    uint64_t max_events_ = 0;
    uint64_t events_     = 0;
    uint64_t timeout_s_  = 0;
};
