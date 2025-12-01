// va2gfn.c
// build:
//   gcc -O2 -Wall -Wextra -std=gnu11 va2gfn.c -o va2gfn \
//     $(pkg-config --cflags libvmi glib-2.0) \
//     $(pkg-config --libs   libvmi glib-2.0) \
//     -Wl,--no-as-needed

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <libvmi/libvmi.h>

static int parse_u64(const char *s, uint64_t *out){
    char *end = NULL;
    uint64_t v = strtoull(s, &end, 0);
    if(!s || *s=='\0' || (end && *end!='\0')) return -1;
    *out = v; return 0;
}

int main(int argc, char **argv){
    if(argc < 4){
        fprintf(stderr,"usage: %s <vm-name> <pid> <va>\n", argv[0]);
        return 1;
    }
    const char *vm = argv[1];
    uint64_t pid=0, va=0;
    if(parse_u64(argv[2], &pid) || parse_u64(argv[3], &va)){
        fprintf(stderr,"bad pid/va\n"); return 1;
    }

    vmi_instance_t vmi = NULL;
    if(VMI_FAILURE == vmi_init_complete(&vmi, (void*)vm, VMI_INIT_DOMAINNAME,
                                        NULL, VMI_CONFIG_GLOBAL_FILE_ENTRY,
                                        NULL, NULL)){
        fprintf(stderr,"vmi_init_complete failed\n"); return 1;
    }

    addr_t dtb = 0;
    if(VMI_SUCCESS != vmi_pid_to_dtb(vmi, (vmi_pid_t)pid, &dtb)){
        fprintf(stderr,"vmi_pid_to_dtb failed for pid=%" PRIu64 "\n", pid);
        vmi_destroy(vmi); return 1;
    }

    addr_t pa = 0;
    if(VMI_SUCCESS != vmi_pagetable_lookup(vmi, dtb, (addr_t)va, &pa)){
        fprintf(stderr,"pagetable_lookup failed (unmapped VA or wrong DTB)\n");
        vmi_destroy(vmi); return 1;
    }

    uint64_t gfn = (uint64_t)(pa >> 12);
    uint64_t off = (uint64_t)(pa & 0xfffULL);

    printf("pid=%" PRIu64 " dtb=0x%llx va=0x%llx -> pa=0x%llx (gfn=0x%llx, off=0x%03llx)\n",
           pid,
           (unsigned long long)dtb,
           (unsigned long long)va,
           (unsigned long long)pa,
           (unsigned long long)gfn,
           (unsigned long long)off);

    vmi_destroy(vmi);
    return 0;
}
