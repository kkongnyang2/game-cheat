// step-event-example.c (변수 출처만 CLI 인자로 대체 지원)
// 빌드 예: gcc -O2 -Wall -Wextra -std=gnu11 step-event-example.c -o step-event-example \
/* ... (원본 헤더/라이선스 주석 그대로) ... */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdio.h>
#include <inttypes.h>
#include <signal.h>

#include <libvmi/libvmi.h>
#include <libvmi/events.h>

reg_t cr3, rip;
vmi_pid_t pid;
vmi_event_t cr3_event;
vmi_event_t mm_event;

addr_t rip_pa;

int mm_enabled;

static int interrupted = 0;
static void close_handler(int sig)
{
    interrupted = sig;
}

void print_event(vmi_event_t *event)
{
    printf("\tPAGE ACCESS: %c%c%c for GFN %"PRIx64" (offset %06"PRIx64") gla %016"PRIx64" (vcpu %u)\n",
           (event->mem_event.out_access & VMI_MEMACCESS_R) ? 'r' : '-',
           (event->mem_event.out_access & VMI_MEMACCESS_W) ? 'w' : '-',
           (event->mem_event.out_access & VMI_MEMACCESS_X) ? 'x' : '-',
           event->mem_event.gfn,
           event->mem_event.offset,
           event->mem_event.gla,
           event->vcpu_id
          );
}

event_response_t step_callback(vmi_instance_t vmi, vmi_event_t *event)
{
    printf("Re-registering event\n");
    vmi_register_event(vmi, event);
    return 0;
}

event_response_t mm_callback(vmi_instance_t vmi, vmi_event_t *event)
{

    vmi_pid_t current_pid = -1;
    vmi_get_vcpureg(vmi, &cr3, CR3, 0);
    vmi_dtb_to_pid(vmi, cr3, &current_pid);

    reg_t rip_test;
    vmi_get_vcpureg(vmi, &rip_test, RIP, 0);

    printf("Memevent: {\n\tPID %u. RIP 0x%lx:\n", current_pid, rip_test);

    print_event(event);

    if ( current_pid == pid && event->mem_event.gla == rip) {
        printf("\tCought the original RIP executing again!");
        vmi_clear_event(vmi, event, NULL);
        interrupted = 1;
    } else {
        printf("\tEvent on same page, but not the same RIP");
        vmi_clear_event(vmi, event, NULL);

        /* These two calls are equivalent */
        //vmi_step_event(vmi, event, event->vcpu_id, 1, NULL);
        vmi_step_event(vmi, event, event->vcpu_id, 1, step_callback);
    }

    printf("\n}\n");
    return 0;
}

event_response_t cr3_callback(vmi_instance_t vmi, vmi_event_t *event)
{
    vmi_pid_t current_pid = -1;
    vmi_dtb_to_pid(vmi, event->reg_event.value, &current_pid);
    //printf("PID %i with CR3=%"PRIx64" executing on vcpu %u.\n", current_pid, event->reg_event.value, event->vcpu_id);

    if (current_pid == pid) {
        if (!mm_enabled) {
            mm_enabled = 1;
        //    printf(" -- Enabling mem-event\n");
            vmi_register_event(vmi, &mm_event);
        }
    } else {
        if (mm_enabled) {
            mm_enabled = 0;
        //    printf(" -- Disabling mem-event\n");
            vmi_clear_event(vmi, &mm_event, NULL);
        }
    }
    return 0;
}

int main (int argc, char **argv)
{
    vmi_instance_t vmi = {0};
    status_t status = VMI_SUCCESS;
    vmi_init_data_t *init_data = NULL;
    addr_t gfn;
    int retcode = 1;

    struct sigaction act;

    mm_enabled = 0;

    char *name = NULL;

    if (argc < 2) {
        fprintf(stderr,
            "Usage:\n"
            "  %s <vm> [<kvmi_socket>]                              (auto mode)\n"
            "  %s <vm> <pid> <dtb_hex> <gla_hex> <gfn_hex> <off_hex> (manual mode)\n",
            argv[0], argv[0]);
        return retcode;
    }

    // Arg 1 is the VM name.
    name = argv[1];

    // KVMi socket ?
    if (argc == 3) {
        char *path = argv[2];

        init_data = malloc(sizeof(vmi_init_data_t) + sizeof(vmi_init_data_entry_t));
        init_data->count = 1;
        init_data->entry[0].type = VMI_INIT_DATA_KVMI_SOCKET;
        init_data->entry[0].data = strdup(path);
    }

    /* for a clean exit */
    act.sa_handler = close_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGHUP,  &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    sigaction(SIGINT,  &act, NULL);
    sigaction(SIGALRM, &act, NULL);

    // Initialize the libvmi library.
    if (VMI_FAILURE ==
            vmi_init_complete(&vmi, (void*)name, VMI_INIT_DOMAINNAME | VMI_INIT_EVENTS,
                              init_data, VMI_CONFIG_GLOBAL_FILE_ENTRY, NULL, NULL)) {
        printf("Failed to init LibVMI library.\n");
        goto error_exit;
    }

    printf("LibVMI init succeeded!\n");

    vmi_pause_vm(vmi);

    SETUP_REG_EVENT(&cr3_event, CR3, VMI_REGACCESS_W, 0, cr3_callback);
    vmi_register_event(vmi, &cr3_event);

    // Setup a mem event for tracking memory at the target page
    // But don't install it; that will be done by the cr3 handler.

    if (argc >= 7) {
        // ==== 수동 주입 모드: 네가 제공한 값으로만 설정 ====
        // argv[2]=pid, argv[3]=dtb, argv[4]=gla, argv[5]=gfn, argv[6]=off
        pid = (vmi_pid_t)strtoull(argv[2], NULL, 0);
        cr3 = (reg_t)strtoull(argv[3], NULL, 0);   // DTB
        rip = (reg_t)strtoull(argv[4], NULL, 0);   // GLA
        addr_t gfn_cli = (addr_t)strtoull(argv[5], NULL, 0);
        addr_t off_cli = (addr_t)strtoull(argv[6], NULL, 0) & 0xfff;

        printf("Manual input: pid=%u dtb=0x%lx gla=0x%lx gfn=0x%lx off=0x%lx\n",
               pid, (unsigned long)cr3, (unsigned long)rip,
               (unsigned long)gfn_cli, (unsigned long)off_cli);

        // 원본과 동일하게 '다음 RIP'을 잡기 위해 -1 수행
        printf("Current value of RIP is 0x%lx\n", rip);
        rip -= 0x1;

        // 원본은 RIP을 물리로 변환해 gfn을 얻었지만,
        // 여기서는 gfn+off를 받아 rip_pa를 구성하고 gfn을 직접 사용
        rip_pa = (gfn_cli << 12) + off_cli;
        gfn = gfn_cli;

    } else {
        // ==== 기존 자동 모드(원본 코드 그대로) ====
        vmi_get_vcpureg(vmi, &rip, RIP, 0);
        vmi_get_vcpureg(vmi, &cr3, CR3, 0);

        printf("Current value of RIP is 0x%lx\n", rip);
        rip -= 0x1;

        vmi_dtb_to_pid(vmi, cr3, &pid);
        if (pid==4) {
            vmi_translate_kv2p(vmi, rip, &rip_pa);
        } else {
            vmi_translate_uv2p(vmi, rip, pid, &rip_pa);
        }

        gfn = rip_pa >> 12;
    }

    printf("Preparing memory event to catch next RIP 0x%lx, PA 0x%lx, page 0x%lx for PID %u\n",
           rip, rip_pa, gfn, pid);
    SETUP_MEM_EVENT(&mm_event, gfn, VMI_MEMACCESS_X, mm_callback, 0);

    vmi_resume_vm(vmi);

    while (!interrupted) {
        status = vmi_events_listen(vmi,500);
        if (status != VMI_SUCCESS) {
            printf("Error waiting for events, quitting...\n");
            interrupted = -1;
        }
    }
    printf("Finished with test.\n");

    retcode = 0;
error_exit:
    // cleanup any memory associated with the libvmi instance
    vmi_destroy(vmi);

    if (init_data) {
        free(init_data->entry[0].data);
        free(init_data);
    }

    return retcode;
}
