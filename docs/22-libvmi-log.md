### singlestep-event-example

libvmi 기본예제에 속함.
모든 vcpu에 관찰을 키고 모든 명령어를 가져옴

```
sudo ./singlestep-event-example win10
Waiting for events...
Single-step event: VCPU:1  GFN 2ead GLA fffff8046a6adf9a
Single-step event: VCPU:0  GFN 2e34 GLA fffff8046a634d55
Single-step event: VCPU:2  GFN 2e33 GLA fffff8046a6332f4
Single-step event: VCPU:3  GFN 2e22 GLA fffff8046a62257e
Waiting for events...
Single-step event: VCPU:1  GFN 2ead GLA fffff8046a6adf9d
Single-step event: VCPU:0  GFN 2e34 GLA fffff8046a634d58
Single-step event: VCPU:2  GFN 2e33 GLA fffff8046a6332f9
Single-step event: VCPU:3  GFN 2e22 GLA fffff8046a622586
Waiting for events...
Single-step event: VCPU:1  GFN 2ead GLA fffff8046a6adfa3
Single-step event: VCPU:0  GFN 2e34 GLA fffff8046a634d60
Single-step event: VCPU:2  GFN 2e33 GLA fffff8046a6332ff
^CSingle-step event: VCPU:1  GFN 2ead GLA fffff8046a6adfa7
Single-step event: VCPU:0  GFN 2e34 GLA fffff8046a634d63
Single-step event: VCPU:2  GFN 2e33 GLA fffff8046a633303
Single-step event: VCPU:3  GFN 2e22 GLA fffff8046a622590
Singlestep stopped
Restarting singlestep
Waiting for events...
Waiting for events...
Single-step event: VCPU:0  GFN 3001 GLA fffff8046a801088
Single-step event: VCPU:3  GFN 2e22 GLA fffff8046a6225b0
Single-step event: VCPU:2  GFN 2e33 GLA fffff8046a633306
Single-step event: VCPU:1  GFN 209542 GLA fffff80470b105dd
Waiting for events...
Single-step event: VCPU:0  GFN 3001 GLA fffff8046a80108a
Single-step event: VCPU:3  GFN 2e22 GLA fffff8046a6225b5
Single-step event: VCPU:2  GFN 2e33 GLA fffff8046a63330a
Single-step event: VCPU:1  GFN 209542 GLA fffff80470b105e3
Waiting for events...
Single-step event: VCPU:3  GFN 2e22 GLA fffff8046a6225ba
Single-step event: VCPU:2  GFN 2e33 GLA fffff8046a63330f
Single-step event: VCPU:1  GFN 209542 GLA fffff80470b105eb
Single-step event: VCPU:0  GFN 3001 GLA fffff8046a80108b
Waiting for events...
Single-step event: VCPU:3  GFN 2e22 GLA fffff8046a6225c2
Single-step event: VCPU:1  GFN 209542 GLA fffff80470b105f2
Single-step event: VCPU:2  GFN 2e33 GLA fffff8046a633312
Single-step event: VCPU:0  GFN 3001 GLA fffff8046a801211
Finished with test.
Single-step event: VCPU:3  GFN 2e22 GLA fffff8046a6225ca
Single-step event: VCPU:1  GFN 209542 GLA fffff80470b105f8
Single-step event: VCPU:0  GFN 3001 GLA fffff8046a801212
Single-step event: VCPU:2  GFN 2e33 GLA fffff8046a633319
```

### step-event-example

libvmi 기본예제에 속함.
현재 페이지에 관찰을 켜서 현재의 rip이 다시 올때까지. vcpu 움직임도 관찰해서 현재 프로세스 dtb를 추적.

```
Memevent: {
	PID 11496. RIP 0xfffff80340a06273:
	PAGE ACCESS: --x for GFN 162f3e (offset 00074d) gla 00007ff7c6e3674d (vcpu 1)
	Event on same page, but not the same RIP
}
PID 3460 with CR3=203c61000 executing on vcpu 0.
 -- Disabling mem-event
Re-registering event
PID 3460 with CR3=203c61000 executing on vcpu 1.
PID 11496 with CR3=111a6f000 executing on vcpu 0.
 -- Enabling mem-event
PID 11496 with CR3=111a6f000 executing on vcpu 1.
Memevent: {
	PID 11496. RIP 0x7ff7c6e3674f:
	PAGE ACCESS: --x for GFN 162f3e (offset 00074f) gla 00007ff7c6e3674f (vcpu 0)
	Event on same page, but not the same RIP
}
Re-registering event
Memevent: {
	PID 11496. RIP 0x7ff7c6e36750:
	PAGE ACCESS: --x for GFN 162f3e (offset 000750) gla 00007ff7c6e36750 (vcpu 0)
	Event on same page, but not the same RIP
}
Re-registering event
```

### mem-event-example

libvmi 기본예제에 속함.
더 단순하게 현재 페이지 특정 트랩 관찰

```
LibVMI init succeeded!
Setting X memory event at RIP 0x7fff7df81710, GPA 0x1cc0c2710, GFN 0x1cc0c2
Waiting for events...
mem_cb: at 0x7fff7df81d60, on frame 0x1cc0c2, permissions: __X
mem_cb: at 0x7fff7df81d65, on frame 0x1cc0c2, permissions: __X
mem_cb: at 0x7fff7df81d6a, on frame 0x1cc0c2, permissions: __X
mem_cb: at 0x7fff7df81d6b, on frame 0x1cc0c2, permissions: __X
mem_cb: at 0x7fff7df81d6f, on frame 0x1cc0c2, permissions: __X
mem_cb: at 0x7fff7df81d72, on frame 0x1cc0c2, permissions: __X
mem_cb: at 0x7fff7df81d74, on frame 0x1cc0c2, permissions: __X
mem_cb: at 0x7fff7df81d77, on frame 0x1cc0c2, permissions: __X
mem_cb: at 0x7fff7df81d7a, on frame 0x1cc0c2, permissions: __X
mem_cb: at 0x7fff7df81d81, on frame 0x1cc0c2, permissions: __X
```

### singlestep-event.c

gcc -O2 -Wall -Wextra -std=gnu11 singlestep-event.c -o singlestep-event   $(pkg-config --cflags libvmi glib-2.0)   $(pkg-config --libs   libvmi glib-2.0)   -Wl,--no-as-needed

sudo ./singlestep-event win10


### va2gfn.c

gcc -O2 -Wall -Wextra -std=gnu11 va2gfn.c -o va2gfn   $(pkg-config --cflags libvmi glib-2.0)   $(pkg-config --libs   libvmi glib-2.0)   -Wl,--no-as-needed

sudo ./va2gfn win10 4320 0x7FF6EED21DE0
pid=4320 dtb=0xabfeb000 va=0x7ff6eed21de0 -> pa=0xb46e3de0 (gfn=0xb46e3, off=0xde0)

vm이름 pid va 순


### step-event.c

gcc -O2 -Wall -Wextra -std=gnu11 step-event.c -o step-event   $(pkg-config --cflags libvmi glib-2.0)   $(pkg-config --libs   libvmi glib-2.0)   -Wl,--no-as-needed

sudo ./step-event win10 4320 0xabfeb000 0x7ff6eed21de0 0xb46e3 0xde0 > ss.log 2>&1
vm이름 pid dtb va gfn offset 순


* 성공은 했지만 페이지를 하나밖에 못추적해서 다른 페이지로 넘어가면 관찰 불가 한계
* 여기서 작성한 코드는 step에 저장되어 있음.