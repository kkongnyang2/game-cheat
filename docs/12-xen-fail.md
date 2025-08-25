## xen 방식으로 vmi를 구현해보자

작성자: kkongnyang2 작성일: 2025-08-15

---

bios 설정
```
VT-x, VT-d 켜기
Secure boot 끄기(서명 문제 회피에 유리. 바로 안꺼지면 바이오스 비번 설정한 후 끈 후 비번 다시 없애면 됨)
vmd controller 끄기(advanced-main에서 ctrl+s 눌러 히든옵션 띄우면 됨)
display mode : optimus
```
* optimus = igpu 직결, dgpu 도움
* dgpu only = dgpu 직결
* auto select = 위 두개 중에 자동선택


24.04 설치시 드라이버 두개 다 미체크
```
$ lspci -nnk | grep -A3 -Ei 'vga|3d|display'
00:02.0 VGA compatible controller [0300]: Intel Corporation Raptor Lake-S UHD Graphics [8086:a788] (rev 04)
	Subsystem: Acer Incorporated [ALI] Raptor Lake-S UHD Graphics [1025:1731]
	Kernel driver in use: i915
	Kernel modules: i915, xe
--
01:00.0 VGA compatible controller [0300]: NVIDIA Corporation AD107M [GeForce RTX 4060 Max-Q / Mobile] [10de:28e0] (rev a1)
	Subsystem: Acer Incorporated [ALI] AD107M [GeForce RTX 4060 Max-Q / Mobile] [1025:1731]
	Kernel driver in use: nouveau
	Kernel modules: nvidiafb, nouveau
```

패키지 설치
```
$ sudo apt update
$ sudo apt install -y xen-system-amd64 ovmf qemu-system-x86
```

1차 시도
```
xen 부팅하면서 e
systemd.unit=multi-user.target module_blacklist=nvidia,nvidia_drm,nvidia_modeset nouveau.modeset=0 추가하고 f10
```
* 안켜짐

2차 시도
```
$ sudo nano /etc/gdm3/custom.conf
WaylandEnable=false
$ sudo systemctl restart gdm
아까처럼 부팅
```
* 안켜짐

3차 시도
```
$ sudo journalctl -k -b -1 -n 300 --no-pager | less
오디오 드라이버가 오랫동안 시간끌다 안들어간 로그 확인
$ echo 'options snd-intel-dspcfg dsp_driver=1' | sudo tee /etc/modprobe.d/alsa-base.conf
$ sudo update-grub
$ sudo update-initramfs -u
아까처럼 부팅
```
* 안켜짐

4차 시도
```
$ sudo journalctl -k -b -1 -n 300 --no-pager | less
내 igpu인 a788이 i915를 거부하는거 확인. xe로 교체하라함
$ echo "options xe force_probe=a788" | sudo tee /etc/modprobe.d/xe-force.conf
options xe force_probe=a788
$ echo "options i915 force_probe=!a788" | sudo tee /etc/modprobe.d/i915-noforce.conf
bash: !a788: event not found
$ echo 'options i915 force_probe=!a788' | sudo tee /etc/modprobe.d/i915-noforce.conf
$ echo -e "blacklist nvidia\nblacklist nvidia_drm\nblacklist nvidia_modeset\nblacklist nouveau" \
| sudo tee /etc/modprobe.d/blacklist-nvidia.conf
blacklist nvidia
blacklist nvidia_drm
blacklist nvidia_modeset
blacklist nouveau
$ sudo nano /etc/default/grub
i915.force_probe=!a788 xe.force_probe=a788
$ sudo tee /etc/modprobe.d/blacklist-audio.conf >/dev/null <<'EOF'
# SOF(PCI/Intel) 계열 전부 차단
blacklist snd_sof_pci
blacklist snd_sof
blacklist snd_sof_intel_hda_common
blacklist snd_sof_intel_hda
blacklist snd_sof_pci_intel_tgl
blacklist snd_sof_pci_intel_mtl
blacklist snd_sof_pci_intel_lnl
# HDA 인텔 드라이버/코덱 차단(완전 무음이 목적일 때)
blacklist snd_hda_intel
blacklist snd_hda_codec_hdmi
# (선택) 사운드와이어도 함께 차단
blacklist soundwire_intel
EOF
$ sudo update-grub
$ sudo update-initramfs -u
아까처럼 부팅
```
* 안켜짐

5차 시도
```
$ sudo journalctl -k -b -1 -n 300 --no-pager | less
교체한 xe 또한 내 igpu인 a788이 거부하는걸 확인
$ lspci -nn -d 8086: -k | sed -n '/VGA/,+2p'
00:02.0 VGA compatible controller [0300]: Intel Corporation Raptor Lake-S UHD Graphics [8086:a788] (rev 04)
	Subsystem: Acer Incorporated [ALI] Raptor Lake-S UHD Graphics [1025:1731]
	Kernel driver in use: xe
$ sudo rm -f /etc/modprobe.d/xe-force.conf /etc/modprobe.d/i915-noforce.conf
# i915를 강제 프로브 + PSR 끄기(보수)
$ echo 'options i915 force_probe=a788 enable_psr=0' | sudo tee /etc/modprobe.d/i915-force.conf
$ sudo nano /etc/default/grub
# GRUB_CMDLINE_LINUX_DEFAULT= 에 다음을 포함
#   intel_iommu=on,igfx_off
$ sudo update-grub
$ sudo update-initramfs -u
아까처럼 부팅
```
* 안켜짐

6차 시도
```
$ sudo journalctl -k -b -1 -n 300 --no-pager | less
백라이트 문제 확인
$ lspci -nn -d 8086: -k | sed -n '/VGA/,+2p'
00:02.0 VGA compatible controller [0300]: Intel Corporation Raptor Lake-S UHD Graphics [8086:a788] (rev 04)
	Subsystem: Acer Incorporated [ALI] Raptor Lake-S UHD Graphics [1025:1731]
	Kernel driver in use: i915
$ ls -1 /sys/class/backlight
nvidia_wmi_ec_backlight
$ sudo nano /etc/default/grub
acpi_backlight=native video.use_native_backlight=1 i915.enable_dpcd_backlight=1 i915.enable_psr=0
```
* 안켜짐


뭐가 문제냐? xen 하이퍼바이저에서는 보안/격리를 위해 메모리를 보수적으로 잡음. 베어메탈에서는 a788이 요청하는 dma api가 iommu에서 바로 수용되는데 xen 하이퍼바이저에서는 swiotlb를 통해 바운스하기에 a788가 요청하는 크기/연속성/정렬/일관성을 만족하지 못함.

> ⚠️ 실패. 화면이 안뜸. grub 설정 잘 하면 화면 해결될듯?