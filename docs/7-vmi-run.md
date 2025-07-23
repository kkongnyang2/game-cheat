## VMI를 실행해보자

### 목표: VMI 구현
작성자: kkongnyang2 작성일: 2025-07-10

---

### >

sudo apt install libvirt-daemon-system libvirt-clients
sudo adduser $USER libvirt
sudo apt install virtinst
virt-install \
  --name win10vmi \
  --memory 4096 \
  --vcpus 4 \
  --disk path=~/win10_120g.qcow2,format=qcow2,bus=virtio \
  --os-variant win10 \
  --import \
  --noautoconsole
virsh edit win10vmi

Select an editor.  To change later, run 'select-editor'.
  1. /bin/nano        <---- easiest
  2. /usr/bin/vim.tiny
  3. /usr/bin/code
  4. /bin/ed

Choose 1-4 [1]: 
Domain 'win10vmi' XML configuration not changed.
virsh list --all
 Id   Name       State
--------------------------
 1    win10vmi   running



sudo qemu-system-x86_64 \
  -machine q35,accel=kvm -cpu host,kvm=on -smp 4 -m 4G \
  -vga none \
  -device vfio-pci,sysfsdev=/sys/bus/mdev/devices/$UUID,display=on,x-igd-opregion=on \
  -drive file=~/win10_120g.qcow2,if=virtio,format=qcow2,cache=none \
  -monitor stdio \
  -qmp unix:/tmp/win10-qmp.sock,server,nowait \
  -display gtk,gl=on




