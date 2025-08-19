성공비결.

(1) auto select와 optimus 차이
지금까지는 optimus는 백라이트 밝기 조절이 안되는거밖에 없음. 둘다 잘 부팅됨

(2) secure boot 끄기
계속 서명 문제를 일으킴

(3) vmd controller 끄기
계속 ssd 인식 문제를 일으킴

(4) 24.04 버전 사용
구버전은 최신 하드웨어들을 잘 못받는 커널임

(5) 화면 이슈. grub 커맨드
echo 'GRUB_CMDLINE_XEN="iommu=1 iommu=verbose dom0_mem=4096M,max:4096M"' | sudo tee /etc/default/grub.d/xen.cfg
echo 'GRUB_CMDLINE_LINUX_XEN_REPLACE="xen-pciback.hide=(01:00.0)(01:00.1) modprobe.blacklist=nvidia,nvidia_drm,nvidia_modeset,nouveau"' \
| sudo tee -a /etc/default/grub.d/xen.cfg
kvm을 하든 xen을 하든 grub에서 nvidia gpu를 잘 숨기는 게 관건인듯.

(6) ovmf대신 seabios로 vm 생성
seabios win10 하니까 vm 만들어짐

(7) libvmi conf 파일 형식
$ sudo nano /etc/libvmi.conf
win10-seabios {
    volatility_ist = "/root/symbols/ntkrnlmp.json";
}
하니까 연결됨

이거 이용하면 다른 방식도 성공할 수 있을걸로 생각.
