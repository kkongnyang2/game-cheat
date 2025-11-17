## drakvuf로 마이크로트레이스를 시도해보자

작성자: kkongnyang2 작성일: 2025-08-27

---
### 만들 코드

CE로 값을 변화시켜가며 확실한 시드 10개 확보
해당 페이지에 pfn writewatch를 건 상태에서 값을 변화시켜 이벤트가 찍히는지 확인
store 부터 다음 store까지의 명령어 정보를 다 마이크로 트레이스
게임 exe가 명령내린 rip만 필터링
mov [RAX+disp], reg/imm 이게 store 명령어인데 RAX(베이스) 계산하는 블록의 시작 주소가 복호화 루틴 후보
.rdata의 16바이트 셔플 마스크/곱셈 상수 XREF로 교차확인, 참조 함수 목록이랑 대조해 복호화→주소계산→store 패턴찾기
복호화 함수 진입/리턴을 잡는 커스텀 플러그인 추가

### 코어 수정

재빌드
```
cd ~/drakvuf/src/libdrakvuf
code .
libdrakvuf.h의 313번째 줄(cplusplus 안에 drakvuf_trap_info 구조체 정의 후)에
// 파일 상단 근처 적절한 곳에:
typedef event_response_t (*drakvuf_step_observer_t)(drakvuf_t, drakvuf_trap_info_t*);
// 등록/해제 API (간단 버전: 등록만 제공해도 충분)
void drakvuf_register_step_observer(drakvuf_t, drakvuf_step_observer_t cb);
private.h의 258번째 줄에(struck drakvuf 내부)
GSList* step_observers;  // list of drakvuf_step_observer_t
vmi.c 143번째 줄에
/* === 추가: 옵저버 브로드캐스트 === */
static inline event_response_t notify_step_observers(drakvuf_t d, drakvuf_trap_info_t* info)
{
    event_response_t resp = 0;
    for (GSList* l = d->step_observers; l; l = l->next)
    {
        drakvuf_step_observer_t cb = (drakvuf_step_observer_t)l->data;
        if (cb) resp |= cb(d, info);
    }
    return resp;
}

/* === 추가: 공개 등록 API 구현 === */
void drakvuf_register_step_observer(drakvuf_t d, drakvuf_step_observer_t cb)
{
    if (!d || !cb) return;
    d->step_observers = g_slist_prepend(d->step_observers, (gpointer)cb);
}

/* === 추가: 반복 싱글스텝용 콜백 ===
   _post_mem_cb 가 “계속 스텝”이 필요하다고 판단하면
   step_event[v]->callback 을 이 함수로 전환한다. */
static event_response_t step_pump_cb(vmi_instance_t vmi, vmi_event_t* event)
{
    (void)vmi;
    drakvuf_t drakvuf = (drakvuf_t)event->data;

    drakvuf_trap_info_t info = {
        .trap       = NULL,
        .trap_pa    = 0,
        .regs       = event->x86_regs,
        .vcpu       = event->vcpu_id,
        .timestamp  = g_get_real_time(),
    };

    /* 가능하면 현재 프로세스 컨텍스트 채움(필요 없으면 빈 값이어도 ok) */
    memset(&info.proc_data, 0, sizeof(info.proc_data));
    memset(&info.attached_proc_data, 0, sizeof(info.attached_proc_data));

    event_response_t rsp = notify_step_observers(drakvuf, &info);

    /* altp2m view 복구 */
    uint16_t view = drakvuf->altp2m_idx;
    if (drakvuf->enable_cr3_based_interception && !drakvuf->vcpu_monitor[event->vcpu_id])
        view = drakvuf->altp2m_idrx;
    event->slat_id = view;

    if (rsp & VMI_EVENT_RESPONSE_TOGGLE_SINGLESTEP)
    {
        /* 한 스텝 더 진행 */
        drakvuf->step_event[event->vcpu_id]->callback = step_pump_cb;
        drakvuf->step_event[event->vcpu_id]->data     = drakvuf;
        return rsp | VMI_EVENT_RESPONSE_SLAT_ID;
    }
    else
    {
        /* 스텝 종료: 리셋 콜백으로 전환 + 싱글스텝 끔 */
        drakvuf->step_event[event->vcpu_id]->callback = vmi_reset_trap;
        drakvuf->step_event[event->vcpu_id]->data     = drakvuf;
        return rsp | VMI_EVENT_RESPONSE_TOGGLE_SINGLESTEP | VMI_EVENT_RESPONSE_SLAT_ID;
    }
}
347줄에 _post_mem_cb 갈아끼기
/* ====== 기존 코드 ====== */
/* Here we are in singlestep mode already and this is a singlstep cb */
static event_response_t _post_mem_cb(drakvuf_t drakvuf, vmi_event_t* event)
{
    event_response_t rsp = 0;
    struct memcb_pass* pass = (struct memcb_pass*)event->data;

    flush_vmi(drakvuf);

    struct wrapper* s = (struct wrapper*)g_hash_table_lookup(drakvuf->memaccess_lookup_gfn, GSIZE_TO_POINTER(pass->gfn));
    if (!s)
    {
        PRINT_DEBUG("Post mem cb @ 0x%lx has been cleared\n", pass->gfn);
        goto done;
    }

    PRINT_DEBUG("Post mem cb @ 0x%lx vCPU %u altp2m %u\n", pass->pa, event->vcpu_id, event->slat_id);

    vmi_pid_t pid = (drakvuf->os == VMI_OS_WINDOWS) ? pass->attached_proc_data.pid : pass->proc_data.pid;
    if (!drakvuf_is_ignored_process(drakvuf, pid))
    {
        drakvuf->in_callback = 1;
        GSList* loop = s->traps;
        while (loop)
        {
            drakvuf_trap_t* trap = (drakvuf_trap_t*)loop->data;

            if (trap->cb && trap->memaccess.type == POST &&
                (trap->memaccess.access & pass->access))
            {
                drakvuf_trap_info_t trap_info =
                {
                    .trap       = trap,
                    .trap_pa    = pass->pa,
                    .regs       = event->x86_regs,
                    .vcpu       = event->vcpu_id,
                    .timestamp  = g_get_real_time(),
                };
                copy_proc_data_from_priv(&trap_info.proc_data,          &pass->proc_data);
                copy_proc_data_from_priv(&trap_info.attached_proc_data, &pass->attached_proc_data);

                rsp |= trap->cb(drakvuf, &trap_info);
            }

            loop = loop->next;
        }
        drakvuf->in_callback = 0;
    }

    process_free_requests(drakvuf);

    if ( pass->traps && !refresh_shadow_copy(drakvuf->vmi, pass) )
    {
        fprintf(stderr, "Failed to refresh shadow copy\n");
        drakvuf->interrupted = 1;
        goto done;
    }

    /* === 추가: store 직후 첫 스텝에서 옵저버에게 RIP 송신 === */
    {
        drakvuf_trap_info_t info_for_step = {
            .trap       = NULL,
            .trap_pa    = pass->pa,
            .regs       = event->x86_regs,
            .vcpu       = event->vcpu_id,
            .timestamp  = g_get_real_time(),
        };
        copy_proc_data_from_priv(&info_for_step.proc_data,          &pass->proc_data);
        copy_proc_data_from_priv(&info_for_step.attached_proc_data, &pass->attached_proc_data);

        rsp |= notify_step_observers(drakvuf, &info_for_step);
    }

done:
    free_proc_data_priv_2(&pass->proc_data, &pass->attached_proc_data);
    g_slice_free(struct memcb_pass, pass);

    uint16_t view = drakvuf->altp2m_idx;
    if (drakvuf->enable_cr3_based_interception && !drakvuf->vcpu_monitor[event->vcpu_id])
        view = drakvuf->altp2m_idrx;
    event->slat_id = view;

    /* === 변경: 옵저버가 “계속 스텝”을 원하면 step_pump_cb로 전환 === */
    if (rsp & VMI_EVENT_RESPONSE_TOGGLE_SINGLESTEP)
    {
        drakvuf->step_event[event->vcpu_id]->callback = step_pump_cb;
        drakvuf->step_event[event->vcpu_id]->data     = drakvuf;
        return rsp | VMI_EVENT_RESPONSE_SLAT_ID;
    }
    else
    {
        drakvuf->step_event[event->vcpu_id]->callback = vmi_reset_trap;
        drakvuf->step_event[event->vcpu_id]->data     = drakvuf;
        return rsp | VMI_EVENT_RESPONSE_TOGGLE_SINGLESTEP | VMI_EVENT_RESPONSE_SLAT_ID;
    }
}
그리고 아래에 있던 
/*
 * This function gets called from the singlestep event
 * after an int3 or a read event happens.
 */
static event_response_t vmi_reset_trap(vmi_instance_t vmi, vmi_event_t* event)
{
    UNUSED(vmi);
    drakvuf_t drakvuf = (drakvuf_t)event->data;
    uint16_t view = drakvuf->altp2m_idx;

    if (drakvuf->enable_cr3_based_interception && !drakvuf->vcpu_monitor[event->vcpu_id])
        view = drakvuf->altp2m_idrx;

    PRINT_DEBUG("reset trap on vCPU %u, switching altp2m %u->%u\n", event->vcpu_id, event->slat_id, view);
    event->slat_id = view;

    return VMI_EVENT_RESPONSE_TOGGLE_SINGLESTEP | // Turn off singlestep
        VMI_EVENT_RESPONSE_SLAT_ID;
}
이걸 지금 추가한 애들 위로 올려주기
$ cd ~/drakvuf
$ meson setup build --reconfigure
$ ninja -C build
$ sudo ninja -C build install
$ sudo ldconfig
```

### micro_trace

커스텀 플러그인 제작
```
$ cd ~/drakvuf/src/plugins
$ mkdir micro_trace
$ nano micro_trace/micro_trace.h
$ nano micro_trace/micro_trace.cpp
$ nano meson.build
plugin_sources = [
  # (기존들)
  'micro_trace/micro_trace.cpp',
  'micro_trace/micro_trace.h',
]
$ nano plugins.h
플러그인 리스트 drakvuf_plugin_t 의 마지막 __DRAKVUF_PLUGIN_LIST_MAX 앞줄에
    PLUGIN_MICRO_TRACE,
이름 리스트 static const char* drakvuf_plugin_names[] 에
    [PLUGIN_MICRO_TRACE] = "micro_trace",
os 지원 리스트 drakvuf_plugin_os_support 에
    [PLUGIN_MICRO_TRACE] = { [VMI_OS_WINDOWS] = 1, [VMI_OS_LINUX] = 1 },
$ nano plugins.cpp
#include "micro_trace/micro_trace.h"
drakvuf_plugins::start 안의 큰 switch에 case 추가
case PLUGIN_MICRO_TRACE:
{
    this->plugins[plugin_id] = std::make_unique<micro_trace>(this->drakvuf, this->output);
    break;
}
```

재빌드
```
$ cd ~/drakvuf
$ meson setup build --reconfigure
$ ninja -C build
$ sudo ninja -C build install
$ sudo ldconfig
```

### 실행

```
$ sudo vmi-process-list win10 | grep -i overwatch
$ sudo env \
  MICROTRACE_PID=0 \
  MICROTRACE_VA_RANGES="0x2E9FCDCD000-0x2E9FCDCF000" \
  MICROTRACE_STEP_MAX=100000 \
  MICROTRACE_MODE=both \
  MICROTRACE_DURATION_MS=10000 \
  MICROTRACE_PEEK=16 \
  MICROTRACE_MAXCHG=8 \
  drakvuf -r /root/symbols/ntkrnlmp.json -d 2 -a micro_trace -o json \
  | tee /home/kkongnyang2/watch_dump/logs/microtrace_wide.jsonl
```
결과가 계속 안나오고 코어 수정하기가 너무 힘들어서 포기


* 여기서 작성한 코드는 plugins에 저장되어 있음.