kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 2032 0x1C59E905468-0x1C59E905469 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=2032 range=[0x1c59e905468..0x1c59e905469) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x126f6a000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001c59e905000 .. 0x000001c59e906000)
[W] WRITE trap @0x1c59e905468 (vcpu=1, gfn=0x760aa)
{"evt":"seed_write_pre","ts_us":1764242870350459,"vcpu":1,"pid":2032,"gla":"0x1c59e905468","gfn":"0x760aa","dtb":"0x126f6a000","cr3":"0x126f6a000","rip":"0x7ff7de2acdad","rsp":"0x7e300fe4f0","rbp":"0x7e300fe600","rflags":"0x10202","module":{"base":"0x7ff7dcac0000","rip_off":"0x17ecdad"},"gpr":{"rax":"0x43540000","rbx":"0x1c59e905468","rcx":"0x1c59e905468","rdx":"0xfffffff7","rsi":"0x1c59e905418","rdi":"0x7e300fe5e0","r8":"0x7ff7de2afe73","r9":"0x3","r10":"0x7ff7dcac0000","r11":"0x31d","r12":"0x0","r13":"0x1c5babcb5c0","r14":"0x0","r15":"0x1c5babcb428"},"disasm":{"rip":"0x7ff7de2acdad","mn":"mov","op":"qword ptr [rbx], rax","mem_write":true,"wkind":"store","ea_calc":"0x1c59e905468","ea_match":true},"rip_bytes_len":16,"rip_bytes":"488903488b47084889430833c0488907","pre_len":16,"pre_bytes":"00000000000000000000000000000000","post_len":16,"post_bytes":"00c058","stack_len":64,"stack_bytes":"6854909ec50100007c0c44ddf77f0000200000002d000000c0acbcbac5010000c0acbcbac5010000a15aaadef77f0000e0e50f307e000000c34f80ddf77f0000"}
[*] Done. (seed write trapped)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 2032 0x1C5A5E1ECB4-0x1C5A5E1ECB5 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=2032 range=[0x1c5a5e1ecb4..0x1c5a5e1ecb5) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x126f6a000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001c5a5e1e000 .. 0x000001c5a5e1f000)
[W] WRITE trap @0x1c5a5e1ecb4 (vcpu=2, gfn=0x1bfff6)
{"evt":"seed_write_pre","ts_us":1764242974101942,"vcpu":2,"pid":2032,"gla":"0x1c5a5e1ecb4","gfn":"0x1bfff6","dtb":"0x126f6a000","cr3":"0x126f6a000","rip":"0x7ff7deb31a3d","rsp":"0x7e300fea90","rbp":"0x7e300febd0","rflags":"0x10203","module":{"base":"0x7ff7dcac0000","rip_off":"0x2071a3d"},"gpr":{"rax":"0x14","rbx":"0x14000200000006","rcx":"0x1c455740dc8","rdx":"0x6","rsi":"0x1c4db858a90","rdi":"0x1c5a5e1eca0","r8":"0x56c4","r9":"0x1c433263428","r10":"0x1c433263400","r11":"0x7e300feb20","r12":"0x1c4048adfc0","r13":"0x1c5198cca40","r14":"0x0","r15":"0x457a6baf26328f71"},"disasm":{"rip":"0x7ff7deb31a3d","mn":"movss","op":"dword ptr [rax + rdi], xmm6","mem_write":true,"wkind":"store","ea_calc":"0x1c5a5e1ecb4","ea_match":true},"rip_bytes_len":16,"rip_bytes":"f30f113438e8a97c6ffe488bc841b101","pre_len":16,"pre_bytes":"00000100053000000000008000007a43","post_len":16,"post_bytes":"000067","stack_len":64,"stack_bytes":"908a85dbc4010000d0eb0f307e000000908a85dbc401000000ffffff00000000d602803c00000000000000000000000001ffffff00000000736030ddf77f0000"}
[*] Done. (seed write trapped)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 2032 0x1C5A5FD3474-0x1C5A5FD3475 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=2032 range=[0x1c5a5fd3474..0x1c5a5fd3475) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x126f6a000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001c5a5fd3000 .. 0x000001c5a5fd4000)
[W] WRITE trap @0x1c5a5fd3474 (vcpu=1, gfn=0x1ce2a2)
{"evt":"seed_write_pre","ts_us":1764243055590257,"vcpu":1,"pid":2032,"gla":"0x1c5a5fd3474","gfn":"0x1ce2a2","dtb":"0x126f6a000","cr3":"0x126f6a000","rip":"0x7ff7deb3a7bb","rsp":"0x7e300feb50","rbp":"0x6","rflags":"0x10206","module":{"base":"0x7ff7dcac0000","rip_off":"0x207a7bb"},"gpr":{"rax":"0x4360b165","rbx":"0x7149aed2681","rcx":"0x1c5a5fd3460","rdx":"0x14","rsi":"0x1c5a5fd3460","rdi":"0x14","r8":"0x1c526bb49a0","r9":"0x7e300fec00","r10":"0x7e300febf8","r11":"0x7e300febe8","r12":"0x0","r13":"0x1c455740dc8","r14":"0x7e300fed68","r15":"0x0"},"disasm":{"rip":"0x7ff7deb3a7bb","mn":"mov","op":"dword ptr [rdx + rcx], eax","mem_write":true,"wkind":"store","ea_calc":"0x1c5a5fd3474","ea_match":true},"rip_bytes_len":16,"rip_bytes":"89040ab0014883c428c3488b44245048","pre_len":16,"pre_bytes":"00000100053000002b0000c000007a43","post_len":16,"post_bytes":"004061","stack_len":64,"stack_bytes":"c45600000000800d251b00ddf77f00000600000000000000d1b0b3def77f0000008049bcc50100008eccb3def77f00006034fda5c5010000c0de49bcc5010000"}
[*] Done. (seed write trapped)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 2032 0x1C5B9D9DA48-0x1C5B9D9DA49 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=2032 range=[0x1c5b9d9da48..0x1c5b9d9da49) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x126f6a000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001c5b9d9d000 .. 0x000001c5b9d9e000)
[W] WRITE trap @0x1c5b9d9da48 (vcpu=5, gfn=0x1b9f98)
{"evt":"seed_write_pre","ts_us":1764243252935568,"vcpu":5,"pid":2032,"gla":"0x1c5b9d9da48","gfn":"0x1b9f98","dtb":"0x126f6a000","cr3":"0x126f6a000","rip":"0x7ff7deb3a7bb","rsp":"0x7e300fe9c0","rbp":"0x8","rflags":"0x10202","module":{"base":"0x7ff7dcac0000","rip_off":"0x207a7bb"},"gpr":{"rax":"0xe3","rbx":"0x7149aed2441","rcx":"0x1c5b9d9d9b0","rdx":"0x98","rsi":"0x1c5b9d9d9b0","rdi":"0x98","r8":"0x1c526bb4910","r9":"0x7e300fea70","r10":"0x7e300fea68","r11":"0x7e300fea58","r12":"0x0","r13":"0x1c455740dc8","r14":"0x7e300febd8","r15":"0x0"},"disasm":{"rip":"0x7ff7deb3a7bb","mn":"mov","op":"dword ptr [rdx + rcx], eax","mem_write":true,"wkind":"store","ea_calc":"0x1c5b9d9da48","ea_match":true},"rip_bytes_len":16,"rip_bytes":"89040ab0014883c428c3488b44245048","pre_len":16,"pre_bytes":"000000000000000080dc84dbc4010000","post_len":16,"post_bytes":"e70000","stack_len":64,"stack_bytes":"c45600000000800d251b00ddf77f00000800000000000000d1b0b3def77f0000000083b5c50100008eccb3def77f0000b0d9d9b9c5010000c03c83b5c5010000"}
[*] Done. (seed write trapped)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 2032 0x1C5BB9D1F38-0x1C5BB9D1F39 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=2032 range=[0x1c5bb9d1f38..0x1c5bb9d1f39) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x126f6a000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001c5bb9d1000 .. 0x000001c5bb9d2000)
[*] Watch timeout (5000 ms)
VMI_ERROR: Caught a memory event that had no handler registered in LibVMI @ GFN 0x1a64fb (0x1a64fbf30), access: 6
[*] Done. (no write)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 12056 0x172651557FC-0x172651557FD 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=12056 range=[0x172651557fc..0x172651557fd) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x88ca4000
[*] Armed W-trap: pages=1 registered=1 range=[0x0000017265155000 .. 0x0000017265156000)
[*] Watch timeout (5000 ms)
VMI_ERROR: Caught a memory event that had no handler registered in LibVMI @ GFN 0x72cef (0x72cef81d), access: 6
[*] Done. (no write)


kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 3252 0x1A2169C8A78-0x1A2169C8A79 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=3252 range=[0x1a2169c8a78..0x1a2169c8a79) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x68537000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001a2169c8000 .. 0x000001a2169c9000)
[W] WRITE trap @0x1a2169c8a78 (vcpu=2, gfn=0x7f7bb)
{"evt":"seed_write_pre","ts_us":1764249262137103,"vcpu":2,"pid":3252,"gla":"0x1a2169c8a78","gfn":"0x7f7bb","dtb":"0x68537000","cr3":"0x68537000","rip":"0x7ff621c780cf","rsp":"0x8ddf3ff3a0","rbp":"0x8ddf3ff4c0","rflags":"0x10206","module":{"base":"0x7ff61fb90000","rip_off":"0x20e80cf"},"gpr":{"rax":"0x1a216340101","rbx":"0x1a2169c8a78","rcx":"0x71","rdx":"0x1a2169c8a78","rsi":"0x1a2163401f8","rdi":"0x8","r8":"0x8ddf3ff440","r9":"0x8ddf3ff400","r10":"0x8ddf3ff408","r11":"0x8ddf3ff428","r12":"0x1a2110f0dc0","r13":"0x1a2110f4460","r14":"0x1a2110f3e50","r15":"0x0"},"disasm":{"rip":"0x7ff621c780cf","mn":"mov","op":"dword ptr [rdx], ecx","mem_write":true,"wkind":"store","ea_calc":"0x1a2169c8a78","ea_match":true},"rip_bytes_len":16,"rip_bytes":"890a4989104883c428c3498b08488b44","pre_len":16,"pre_bytes":"e129725a88060000808a9c16a2010000","post_len":16,"post_bytes":"750000","stack_len":64,"stack_bytes":"3050b022f67f000047b2c621f67f0000000000000000000040c86816a201000010000000010000005487c721f67f0000f0f53fdf8d000000e06cb022f67f0000"}
[*] Done. (seed write trapped)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 3252 0x1A2169C9730-0x1A2169C9734 5000
[sudo] password for kkongnyang2: 
=== seed_once_watch_v3 ===
[*] domain=win10 pid=3252 range=[0x1a2169c9730..0x1a2169c9734) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x68537000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001a2169c9000 .. 0x000001a2169ca000)
[W] WRITE trap @0x1a2169c9730 (vcpu=4, gfn=0xa1fca)
{"evt":"seed_write_pre","ts_us":1764250298851818,"vcpu":4,"pid":3252,"gla":"0x1a2169c9730","gfn":"0xa1fca","dtb":"0x68537000","cr3":"0x68537000","rip":"0x7ff621c780cf","rsp":"0x8ddf5fefe0","rbp":"0x8ddf5ff100","rflags":"0x10206","module":{"base":"0x7ff61fb90000","rip_off":"0x20e80cf"},"gpr":{"rax":"0x1a216340101","rbx":"0x1a2169c9730","rcx":"0xb2","rdx":"0x1a2169c9730","rsi":"0x1a2163401f8","rdi":"0x8","r8":"0x8ddf5ff080","r9":"0x8ddf5ff040","r10":"0x8ddf5ff048","r11":"0x8ddf5ff068","r12":"0x1a2110f0dc0","r13":"0x1a2110f4460","r14":"0x1a2110f3e50","r15":"0x0"},"disasm":{"rip":"0x7ff621c780cf","mn":"mov","op":"dword ptr [rdx], ecx","mem_write":true,"wkind":"store","ea_calc":"0x1a2169c9730","ea_match":true},"rip_bytes_len":16,"rip_bytes":"890a4989104883c428c3498b08488b44","pre_len":16,"pre_bytes":"c15c725a8806000038979c16a2010000","post_len":16,"post_bytes":"d00000","stack_len":64,"stack_bytes":"388a9c16a2010000388a9c16a20100005102fc8788060000e128725a88060000503e0f11a20100005487c721f67f0000e0e05c42a201000068e05c42a2010000"}
[*] Done. (seed write trapped)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 3252 0x1A2169CAB00-0x1A2169CAB08 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=3252 range=[0x1a2169cab00..0x1a2169cab08) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x68537000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001a2169ca000 .. 0x000001a2169cb000)
[W] WRITE trap @0x1a2169cab00 (vcpu=2, gfn=0x1edc9)
{"evt":"seed_write_pre","ts_us":1764250445984545,"vcpu":2,"pid":3252,"gla":"0x1a2169cab00","gfn":"0x1edc9","dtb":"0x68537000","cr3":"0x68537000","rip":"0x7ff621c780a6","rsp":"0x8ddf3ff2b0","rbp":"0x8ddf3ff3d0","rflags":"0x10206","module":{"base":"0x7ff61fb90000","rip_off":"0x20e80a6"},"gpr":{"rax":"0x8ddf3ff358","rbx":"0x1a2169cab00","rcx":"0x43658445","rdx":"0x1a2169cab00","rsi":"0x1a37a0e98e4","rdi":"0x6","r8":"0x8ddf3ff318","r9":"0x8ddf3ff310","r10":"0x8ddf3ff318","r11":"0x8ddf3ff338","r12":"0x1a2110f0dc0","r13":"0x1a2110f4460","r14":"0x1a2110f3e50","r15":"0x0"},"disasm":{"rip":"0x7ff621c780a6","mn":"mov","op":"dword ptr [rdx], ecx","mem_write":true,"wkind":"store","ea_calc":"0x1a2169cab00","ea_match":true},"rip_bytes_len":16,"rip_bytes":"890a488b08b0014889114883c428c349","pre_len":16,"pre_bytes":"01ac725a8806000008ab9c16a2010000","post_len":16,"post_bytes":"60f365","stack_len":64,"stack_bytes":"3050b022f67f000047b2c621f67f0000000000000000000040c86816a201000010000000010000005487c721f67f000000f53fdf8d000000485fb022f67f0000"}
[*] Done. (seed write trapped)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 3252 0x1A2169D7B88-0x1A2169D7B8C 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=3252 range=[0x1a2169d7b88..0x1a2169d7b8c) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x68537000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001a2169d7000 .. 0x000001a2169d8000)
[W] WRITE trap @0x1a2169d7b88 (vcpu=4, gfn=0x69ece)
{"evt":"seed_write_pre","ts_us":1764250554417095,"vcpu":4,"pid":3252,"gla":"0x1a2169d7b88","gfn":"0x69ece","dtb":"0x68537000","cr3":"0x68537000","rip":"0x7ff621c780cf","rsp":"0x8ddf5fefd0","rbp":"0x8ddf5ff0f0","rflags":"0x10206","module":{"base":"0x7ff61fb90000","rip_off":"0x20e80cf"},"gpr":{"rax":"0x1a216340101","rbx":"0x1a2169d7b88","rcx":"0xde","rdx":"0x1a2169d7b88","rsi":"0x1a2163401f8","rdi":"0x8","r8":"0x8ddf5ff070","r9":"0x8ddf5ff030","r10":"0x8ddf5ff038","r11":"0x8ddf5ff058","r12":"0x1a2110f0dc0","r13":"0x1a2110f4460","r14":"0x1a2110f3e50","r15":"0x0"},"disasm":{"rip":"0x7ff621c780cf","mn":"mov","op":"dword ptr [rdx], ecx","mem_write":true,"wkind":"store","ea_calc":"0x1a2169d7b88","ea_match":true},"rip_bytes_len":16,"rip_bytes":"890a4989104883c428c3498b08488b44","pre_len":16,"pre_bytes":"21ee755a88060000907b9d16a2010000","post_len":16,"post_bytes":"e30000","stack_len":64,"stack_bytes":"388a9c16a2010000388a9c16a2010000d102d0eb8a060000e128725a88060000503e0f11a20100005487c721f67f0000e0cf1c47a201000068cf1c47a2010000"}
[*] Done. (seed write trapped)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 3252 0x1A2169D92f8-0x1A2169D9300 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=3252 range=[0x1a2169d92f8..0x1a2169d9300) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x68537000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001a2169d9000 .. 0x000001a2169da000)
[W] WRITE trap @0x1a2169d92f8 (vcpu=0, gfn=0xceccc)
{"evt":"seed_write_pre","ts_us":1764250702140211,"vcpu":0,"pid":3252,"gla":"0x1a2169d92f8","gfn":"0xceccc","dtb":"0x68537000","cr3":"0x68537000","rip":"0x7ff621c780a6","rsp":"0x8ddf3ff4a0","rbp":"0x8ddf3ff5c0","rflags":"0x10202","module":{"base":"0x7ff61fb90000","rip_off":"0x20e80a6"},"gpr":{"rax":"0x8ddf3ff548","rbx":"0x1a2169d92f8","rcx":"0x436e6486","rdx":"0x1a2169d92f8","rsi":"0x1a37a0e98e4","rdi":"0x6","r8":"0x8ddf3ff508","r9":"0x8ddf3ff500","r10":"0x8ddf3ff508","r11":"0x8ddf3ff528","r12":"0x1a2110f0dc0","r13":"0x1a2110f4460","r14":"0x1a2110f3e50","r15":"0x0"},"disasm":{"rip":"0x7ff621c780a6","mn":"mov","op":"dword ptr [rdx], ecx","mem_write":true,"wkind":"store","ea_calc":"0x1a2169d92f8","ea_match":true},"rip_bytes_len":16,"rip_bytes":"890a488b08b0014889114883c428c349","pre_len":16,"pre_bytes":"e14b765a8806000000939d16a2010000","post_len":16,"post_bytes":"63be6e","stack_len":64,"stack_bytes":"3050b022f67f000047b2c621f67f0000000000000000000040c86816a201000010000000010000005487c721f67f0000f0f63fdf8d000000485fb022f67f0000"}
[*] Done. (seed write trapped)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 3252 0x1A216A5DA30-0x1A216A5DA38 5000
[sudo] password for kkongnyang2: 
=== seed_once_watch_v3 ===
[*] domain=win10 pid=3252 range=[0x1a216a5da30..0x1a216a5da38) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x68537000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001a216a5d000 .. 0x000001a216a5e000)
[W] WRITE trap @0x1a216a5da30 (vcpu=2, gfn=0x1ea1d6)
{"evt":"seed_write_pre","ts_us":1764251631207680,"vcpu":2,"pid":3252,"gla":"0x1a216a5da30","gfn":"0x1ea1d6","dtb":"0x68537000","cr3":"0x68537000","rip":"0x7ff621c780a6","rsp":"0x8ddf3ff3e0","rbp":"0x8ddf3ff560","rflags":"0x10206","module":{"base":"0x7ff61fb90000","rip_off":"0x20e80a6"},"gpr":{"rax":"0x8ddf3ff488","rbx":"0x1a216a5da30","rcx":"0x435685f6","rdx":"0x1a216a5da30","rsi":"0x1a37a0e98e4","rdi":"0x6","r8":"0x8ddf3ff448","r9":"0x8ddf3ff440","r10":"0x8ddf3ff448","r11":"0x8ddf3ff468","r12":"0x8ddf3ff560","r13":"0x1a2169d8f18","r14":"0x5d0","r15":"0x8ddf3ff5a8"},"disasm":{"rip":"0x7ff621c780a6","mn":"mov","op":"dword ptr [rdx], ecx","mem_write":true,"wkind":"store","ea_calc":"0x1a216a5da30","ea_match":true},"rip_bytes_len":16,"rip_bytes":"890a488b08b0014889114883c428c349","pre_len":16,"pre_bytes":"c168975a8806000038daa516a2010000","post_len":16,"post_bytes":"00c058","stack_len":64,"stack_bytes":"70008e4fa2010000408c3f83a3010000080000000000000016c07021f67f000000008e4fa20100005487c721f67f000000000000000000000800000000000000"}
[*] Done. (seed write trapped)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 3252 0x1A216CE1F38-0x1A216CE1F40 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=3252 range=[0x1a216ce1f38..0x1a216ce1f40) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x68537000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001a216ce1000 .. 0x000001a216ce2000)
[*] Watch timeout (5000 ms)
VMI_ERROR: Caught a memory event that had no handler registered in LibVMI @ GFN 0x194184 (0x194184f30), access: 6
[*] Done. (no write)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 10484 0x22DD649DAD0-0x22DD649DAD8 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=10484 range=[0x22dd649dad0..0x22dd649dad8) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x74fbb000
[*] Armed W-trap: pages=1 registered=1 range=[0x0000022dd649d000 .. 0x0000022dd649e000)
[*] Watch timeout (5000 ms)
VMI_ERROR: Caught a memory event that had no handler registered in LibVMI @ GFN 0x80fb3 (0x80fb3ac8), access: 6
[*] Done. (no write)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 11636 0x28DEBFEE650-0x28DEBFEE658 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=11636 range=[0x28debfee650..0x28debfee658) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x91bcb000
[*] Armed W-trap: pages=1 registered=1 range=[0x0000028debfee000 .. 0x0000028debfef000)
[W] WRITE trap @0x28debfee650 (vcpu=1, gfn=0xe875)
{"evt":"seed_write_pre","ts_us":1764253352777476,"vcpu":1,"pid":11636,"gla":"0x28debfee650","gfn":"0xe875","dtb":"0x91bcb000","cr3":"0x91bcb000","rip":"0x7ff6ae13cdad","rsp":"0xeacdffe610","rbp":"0xeacdffe700","rflags":"0x10206","module":{"base":"0x7ff6ac950000","rip_off":"0x17ecdad"},"gpr":{"rax":"0x43624000","rbx":"0x28debfee650","rcx":"0x28debfee650","rdx":"0xfffffff7","rsi":"0x28debfee600","rdi":"0xeacdffe700","r8":"0x7ff6ae13fe73","r9":"0x3","r10":"0x7ff6ac950000","r11":"0x31c","r12":"0x0","r13":"0x28de6db3b00","r14":"0x0","r15":"0x28de6db39c8"},"disasm":{"rip":"0x7ff6ae13cdad","mn":"mov","op":"qword ptr [rbx], rax","mem_write":true,"wkind":"store","ea_calc":"0x28debfee650","ea_match":true},"rip_bytes_len":16,"rip_bytes":"488903488b47084889430833c0488907","pre_len":16,"pre_bytes":"00000000000000000000000000000000","post_len":16,"post_bytes":"000067","stack_len":64,"stack_bytes":"50e6feeb8d0200007c0c2dadf67f000013000000200000000032dbe68d0200000032dbe68d020000a15a93aef67f000000e7ffcdea000000c34f69adf67f0000"}
[*] Done. (seed write trapped)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 11636 0x28DFDB00174-0x28DFDB00175 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=11636 range=[0x28dfdb00174..0x28dfdb00175) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x91bcb000
[*] Armed W-trap: pages=1 registered=1 range=[0x0000028dfdb00000 .. 0x0000028dfdb01000)
[*] Watch timeout (5000 ms)
VMI_ERROR: Caught a memory event that had no handler registered in LibVMI @ GFN 0xa05f9 (0xa05f9f84), access: 6
[*] Done. (no write)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 2768 0x1D6F9218F84-0x1D6F9218F88 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=2768 range=[0x1d6f9218f84..0x1d6f9218f88) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x5c8fb000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001d6f9218000 .. 0x000001d6f9219000)
[W] WRITE trap @0x1d6f9218f84 (vcpu=4, gfn=0x72dbc)
{"evt":"seed_write_pre","ts_us":1764261617035029,"vcpu":4,"pid":2768,"gla":"0x1d6f9218f84","gfn":"0x72dbc","dtb":"0x5c8fb000","cr3":"0x5c8fb000","rip":"0x7ff6c867a7bb","rsp":"0x62c19ae7d0","rbp":"0x6","rflags":"0x10202","module":{"base":"0x7ff6c6600000","rip_off":"0x207a7bb"},"gpr":{"rax":"0x436bc000","rbx":"0x75910524c41","rcx":"0x1d6f9218f70","rdx":"0x14","rsi":"0x1d6f9218f70","rdi":"0x14","r8":"0x1d644149310","r9":"0x62c19ae880","r10":"0x62c19ae878","r11":"0x62c19ae868","r12":"0x0","r13":"0x1d58d160dc8","r14":"0x62c19ae9e8","r15":"0x0"},"disasm":{"rip":"0x7ff6c867a7bb","mn":"mov","op":"dword ptr [rdx + rcx], eax","mem_write":true,"wkind":"store","ea_calc":"0x1d6f9218f84","ea_match":true},"rip_bytes_len":16,"rip_bytes":"89040ab0014883c428c3488b44245048","pre_len":16,"pre_bytes":"00000200045500002b0000c000007a43","post_len":16,"post_bytes":"008070","stack_len":64,"stack_bytes":"c45600000000800d251bb4c6f67f00000600000000000000d1b067c8f67f000000806002d70100008ecc67c8f67f0000708f21f9d601000040c06002d7010000"}
[*] Done. (seed write trapped)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 2768 0x1D6FC9E46A8-0x1D6FC9E46AC 5000
[sudo] password for kkongnyang2: 
=== seed_once_watch_v3 ===
[*] domain=win10 pid=2768 range=[0x1d6fc9e46a8..0x1d6fc9e46ac) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x5c8fb000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001d6fc9e4000 .. 0x000001d6fc9e5000)
[W] WRITE trap @0x1d6fc9e46a8 (vcpu=2, gfn=0x1c20c2)
{"evt":"seed_write_pre","ts_us":1764262938183981,"vcpu":2,"pid":2768,"gla":"0x1d6fc9e46a8","gfn":"0x1c20c2","dtb":"0x5c8fb000","cr3":"0x5c8fb000","rip":"0x7ff6c867a7bb","rsp":"0x62c19ae880","rbp":"0x8","rflags":"0x10202","module":{"base":"0x7ff6c6600000","rip_off":"0x207a7bb"},"gpr":{"rax":"0xd4","rbx":"0x759105249f1","rcx":"0x1d6fc9e4610","rdx":"0x98","rsi":"0x1d6fc9e4610","rdi":"0x98","r8":"0x1d64414927c","r9":"0x62c19ae930","r10":"0x62c19ae928","r11":"0x62c19ae918","r12":"0x0","r13":"0x1d58d160dc8","r14":"0x62c19aea98","r15":"0x0"},"disasm":{"rip":"0x7ff6c867a7bb","mn":"mov","op":"dword ptr [rdx + rcx], eax","mem_write":true,"wkind":"store","ea_calc":"0x1d6fc9e46a8","ea_match":true},"rip_bytes_len":16,"rip_bytes":"89040ab0014883c428c3488b44245048","pre_len":16,"pre_bytes":"0000000000000000a0403619d6010000","post_len":16,"post_bytes":"d90000","stack_len":64,"stack_bytes":"c45600000000800d251bb4c6f67f00000800000000000000d1b067c8f67f0000008051f6d60100008ecc67c8f67f000010469efcd6010000e0ca51f6d6010000"}
[*] Done. (seed write trapped)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 2768 0x1D6FB74D6F8-0x1D6FB74D700 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=2768 range=[0x1d6fb74d6f8..0x1d6fb74d700) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x5c8fb000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001d6fb74d000 .. 0x000001d6fb74e000)
[W] WRITE trap @0x1d6fb74d6f8 (vcpu=5, gfn=0xa9b76)
{"evt":"seed_write_pre","ts_us":1764263096303797,"vcpu":5,"pid":2768,"gla":"0x1d6fb74d6f8","gfn":"0xa9b76","dtb":"0x5c8fb000","cr3":"0x5c8fb000","rip":"0x7ff6c7df250b","rsp":"0x62c19ae900","rbp":"0x62c19ae9c9","rflags":"0x10286","module":{"base":"0x7ff6c6600000","rip_off":"0x17f250b"},"gpr":{"rax":"0x43710000","rbx":"0x1d6fb74d6f8","rcx":"0xfffffff7","rdx":"0x3","rsi":"0x62c19aea90","rdi":"0x62c19ae998","r8":"0x28","r9":"0x20","r10":"0x1d6fb74d184","r11":"0x5df7bcf134f45550","r12":"0x2000","r13":"0x1d68b16ab50","r14":"0xd8000000000612f","r15":"0xd"},"disasm":{"rip":"0x7ff6c7df250b","mn":"mov","op":"qword ptr [rbx], rax","mem_write":true,"wkind":"store","ea_calc":"0x1d6fb74d6f8","ea_match":true},"rip_bytes_len":16,"rip_bytes":"4889038b4708894308488b5c24304883","pre_len":16,"pre_bytes":"00000000000000000000000000000000","post_len":16,"post_bytes":"000076","stack_len":64,"stack_bytes":"f8d674fbd60100000000000002000000700100000000000090b85dc8f67f0000f0d674fbd6010000eecddec7f67f0000f8d674fbd601000000e99ac162000000"}
[*] Done. (seed write trapped)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v3 win10 2768 0x1D6F92198E4-0x1D6F92198E8 5000
=== seed_once_watch_v3 ===
[*] domain=win10 pid=2768 range=[0x1d6f92198e4..0x1d6f92198e8) timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x5c8fb000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001d6f9219000 .. 0x000001d6f921a000)
[*] Watch timeout (5000 ms)
VMI_ERROR: Caught a memory event that had no handler registered in LibVMI @ GFN 0x716e4 (0x716e4951), access: 6
[*] Done. (no write)


18개

공통 부분 / 개별 부분
첫번째: 7fc 468 a78(4) 730(4) b00 b88(4) 2f8 f38 ad0 650 a30 / d04 664 8b8 cd8(4) 6a8(4) 280 120
두번째: 7fc 468 f38 ad0 650 a78(4) 730(4) b00 b88(4) 2f8 a30 / 3a4 d04 288 680 a48(4) 418(4) e80
세번째: 468 a78(4) 730(4) b00 b88(4) 2f8 a30 f38 ad0 650 7fc / 1f8(4) 6f8 080 8e4 244 7b8(4) 8a0
닷번째: 468 f38 ad0 650 a78(4) 730(4) b00 b88(4) 2f8 a30 7fc / 480 530 a48(4) f84 8e4 6f8 6a8(4)
엿번째: 7fc b88 a78(4) 730(4) b00 b88(4) 2f8 a30 / 520 f84 8e4 ff0 708 c80 d28 570 1f8(4) bc8(4)

트랩성공한거: 468 a78(4) 730(4) b00 b88(4) 2f8 a30 650 / f84 cb4 474 a48(4) 6a8(4) 6f8
트랩실패한거: f38 7fc ad0 / 174 8e4

성공한거 rip정보
207a7bb mov dword ptr [rdx + rcx], eax  474 a48 f84 6a8

207cc50부터 트레이스를 걸었더니 call207a710까지는 잘하는데 ja 안가고 jmp rcx에서 207a83f로 넘어가버림. 그리고 ret해서 207cc8e와서는 jne가고 ret해서 207cd86으로. 그리고 또 ret해서 20ff2ae로.
ret 세번하고 20FF2AE로 감.
20ff1af시작 call2b54070 call207ce70 jne20ff302 call20fe9d0 call2b54070 call2072ca0 call207b180 call2130a60 call2b54070 call207cd00 여기에 속함 call2072cao call207c410 

jmp rcx에서는 0207a742 0207a763 0207a784 0207a7a5 0207a7c5 0207a7fd 0207a81c 0207a83f 0207a86b
이 중 0207a7a5를 타고 가면 보인다.

"rax":"0x4360b165","rcx":"0x1c5a5fd3460","rdx":"0x14"
"rax":"0xe3","rcx":"0x1c5b9d9d9b0","rdx":"0x98"
"rax":"0x436bc000","rcx":"0x1d6f9218f70","rdx":"0x14"
"rax":"0xd4","rcx":"0x1d6fc9e4610","rdx":"0x98

20e80cf mov dword ptr [rdx], ecx  a78 730 b88
2119ad1. jne감. call 20fa6e0 이 안에서 핸들러 20b5e30 선택. 끝나면 다시 2119ad1.

"rcx":"0x71","rdx":"0x1a2169c8a78"
"rcx":"0xb2","rdx":"0x1a2169c9730"
"rcx":"0xde","rdx":"0x1a2169d7b88"

20e80a6 mov dword ptr [rdx], ecx  b00 2f8 a30

"rcx":"0x43658445","rdx":"0x1a2169cab00"
"rcx":"0x436e6486","rdx":"0x1a2169d92f8"
"rcx":"0x435685f6","rdx":"0x1a216a5da30"

17ecdad mov qword ptr [rbx], rax  468 650

"rax":"0x43540000","rbx":"0x1c59e905468"
"rax":"0x43624000","rbx":"0x28debfee650"

2071a3d movss dword ptr [rax + rdi], xmm6  cb4

"rax":"0x14","rdi":"0x1c5a5e1eca0"

17f250b mov qword ptr [rbx], rax  6f8

"rax":"0x43710000","rbx":"0x1d6fb74d6f8



소전

2f8 888 0c0 7fc bb8 820 a78(4) 730(4) b00 b88(4) 2f8 a30 894 1f4 480 d28 a48(4) 418(4)

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v5 win10 10028 0x1D7F05052F8 5000
=== seed_once_watch_v5 (two-hit) ===
[*] domain=win10 pid=10028 va=0x1d7f05052f8 timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x10eaf4000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001d7f0505000 .. 0x000001d7f0506000)
[W] WRITE trap #1 @0x1d7f05052f8 (vcpu=4, gfn=0x1586ff)
{"evt":"seed_write_pre","ts_us":1764354028294492,"vcpu":4,"pid":10028,"gla":"0x1d7f05052f8","gfn":"0x1586ff","dtb":"0x10eaf4000","cr3":"0x10eaf4000","rip":"0x7ff72678cdad","rsp":"0x28196fdef0","rbp":"0x28196fe000","rflags":"0x10202","module":{"base":"0x7ff724fa0000","rip_off":"0x17ecdad"},"gpr":{"rax":"0x43494000","rbx":"0x1d7f05052f8","rcx":"0x1d7f05052f8","rdx":"0xfffffff7","rsi":"0x1d7f05052a8","rdi":"0x28196fdfe0","r8":"0x7ff72678fe73","r9":"0x3","r10":"0x7ff724fa0000","r11":"0x286","r12":"0x0","r13":"0x1d803d4f640","r14":"0x0","r15":"0x1d803d4f4a8"},"disasm":{"rip":"0x7ff72678cdad","mn":"mov","op":"qword ptr [rbx], rax","mem_write":true,"wkind":"store","ea_calc":"0x1d7f05052f8","ea_match":true},"rip_bytes_len":16,"rip_bytes":"488903488b47084889430833c0488907","pre_len":16,"pre_bytes":"00000000000000000000000000000000","post_len":16,"post_bytes":"00004e","stack_len":64,"stack_bytes":"f85250f0d70100007c0c9225f77f00000807a775c933c99740edd403d801000040edd403d8010000a15af826f77f0000e0df6f1928000000c34fce25f77f0000"}
[W] WRITE trap #2 @0x1d7f05052f8 (vcpu=4, gfn=0x1586ff)
{"evt":"seed_write_pre","ts_us":1764354028306136,"vcpu":4,"pid":10028,"gla":"0x1d7f05052f8","gfn":"0x1586ff","dtb":"0x10eaf4000","cr3":"0x10eaf4000","rip":"0x7ff72678cdad","rsp":"0x28196fdef0","rbp":"0x28196fe000","rflags":"0x10202","module":{"base":"0x7ff724fa0000","rip_off":"0x17ecdad"},"gpr":{"rax":"0x43494000","rbx":"0x1d7f05052f8","rcx":"0x1d7f05052f8","rdx":"0xfffffff7","rsi":"0x1d7f05052a8","rdi":"0x28196fdfe0","r8":"0x7ff72678fe73","r9":"0x3","r10":"0x7ff724fa0000","r11":"0x286","r12":"0x0","r13":"0x1d803d4f640","r14":"0x0","r15":"0x1d803d4f4a8"},"disasm":{"rip":"0x7ff72678cdad","mn":"mov","op":"qword ptr [rbx], rax","mem_write":true,"wkind":"store","ea_calc":"0x1d7f05052f8","ea_match":true},"rip_bytes_len":16,"rip_bytes":"488903488b47084889430833c0488907","pre_len":16,"pre_bytes":"00000000000000000000000000000000","post_len":16,"post_bytes":"00004e","stack_len":64,"stack_bytes":"f85250f0d70100007c0c9225f77f00000807a775c933c99740edd403d801000040edd403d8010000a15af826f77f0000e0df6f1928000000c34fce25f77f0000"}
[*] Done. seed write trapped (hits=2)

17ecdad mov qword ptr [rbx], rax 2f8
"rax":"0x43494000","rbx":"0x1d7f05052f8"

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v5 win10 10028 0x1D848f25a48 5000
=== seed_once_watch_v5 (two-hit) ===
[*] domain=win10 pid=10028 va=0x1d848f25a48 timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x10eaf4000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001d848f25000 .. 0x000001d848f26000)
[W] WRITE trap #1 @0x1d848f25a48 (vcpu=1, gfn=0x11d8b2)
{"evt":"seed_write_pre","ts_us":1764354213613088,"vcpu":1,"pid":10028,"gla":"0x1d848f25a48","gfn":"0x11d8b2","dtb":"0x10eaf4000","cr3":"0x10eaf4000","rip":"0x7ff72706caaf","rsp":"0x28196fe260","rbp":"0x28196fe2d0","rflags":"0x10202","module":{"base":"0x7ff724fa0000","rip_off":"0x20ccaaf"},"gpr":{"rax":"0xca","rbx":"0x98001000000008","rcx":"0x1d844639d28","rdx":"0x0","rsi":"0x1d848f259b0","rdi":"0x1d6932d0dc8","r8":"0x98","r9":"0x28196fe350","r10":"0x7ff72706ca86","r11":"0x28196fe338","r12":"0x0","r13":"0x1d7cfd2ab40","r14":"0x0","r15":"0x1d78f02ce20"},"disasm":{"rip":"0x7ff72706caaf","mn":"mov","op":"dword ptr [r8 + rsi], eax","mem_write":true,"wkind":"store","ea_calc":"0x1d848f25a48","ea_match":true},"rip_bytes_len":16,"rip_bytes":"41890430e934010000498b38488d4d20","pre_len":16,"pre_bytes":"0000000000000000c063f444d7010000","post_len":16,"post_bytes":"ce0000","stack_len":64,"stack_bytes":"051b0600010010000000000000000000a1b5873c5e07000000000000000000000000000000000000000000000000000000000000000000000000000000000000"}
[W] WRITE trap #2 @0x1d848f25a48 (vcpu=1, gfn=0x11d8b2)
{"evt":"seed_write_pre","ts_us":1764354213619045,"vcpu":1,"pid":10028,"gla":"0x1d848f25a48","gfn":"0x11d8b2","dtb":"0x10eaf4000","cr3":"0x10eaf4000","rip":"0x7ff72706caaf","rsp":"0x28196fe260","rbp":"0x28196fe2d0","rflags":"0x10202","module":{"base":"0x7ff724fa0000","rip_off":"0x20ccaaf"},"gpr":{"rax":"0xca","rbx":"0x98001000000008","rcx":"0x1d844639d28","rdx":"0x0","rsi":"0x1d848f259b0","rdi":"0x1d6932d0dc8","r8":"0x98","r9":"0x28196fe350","r10":"0x7ff72706ca86","r11":"0x28196fe338","r12":"0x0","r13":"0x1d7cfd2ab40","r14":"0x0","r15":"0x1d78f02ce20"},"disasm":{"rip":"0x7ff72706caaf","mn":"mov","op":"dword ptr [r8 + rsi], eax","mem_write":true,"wkind":"store","ea_calc":"0x1d848f25a48","ea_match":true},"rip_bytes_len":16,"rip_bytes":"41890430e934010000498b38488d4d20","pre_len":16,"pre_bytes":"0000000000000000c063f444d7010000","post_len":16,"post_bytes":"ce0000","stack_len":64,"stack_bytes":"051b0600010010000000000000000000a1b5873c5e07000000000000000000000000000000000000000000000000000000000000000000000000000000000000"}
[*] Done. seed write trapped (hits=2)

20ccaaf mov dword ptr [r8 + rsi], eax a48
"r8":"0x98" "rsi":"0x1d848f259b0" "rax":"0xca"

kkongnyang2@acer:~/game-cheat/code/project4$ sudo ./seed_once_watch_v5 win10 10028 0x1D83EFBE894 5000
=== seed_once_watch_v5 (two-hit) ===
[*] domain=win10 pid=10028 va=0x1d83efbe894 timeout=5000 ms
[*] libvmi initialized
[+] DTB resolved via vmi_pid_to_dtb: 0x10eaf4000
[*] Armed W-trap: pages=1 registered=1 range=[0x000001d83efbe000 .. 0x000001d83efbf000)
[*] Watch timeout (5000 ms)
VMI_ERROR: Caught a memory event that had no handler registered in LibVMI @ GFN 0x123912 (0x1239121a8), access: 6
[*] Done. no write (hits=0)
