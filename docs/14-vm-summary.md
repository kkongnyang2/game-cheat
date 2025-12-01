### 성공비결. debug

(1) auto select와 optimus 차이
지금까지는 optimus는 백라이트 밝기 조절이 안되는거밖에 없음. 둘다 잘 부팅됨

(2) secure boot 끄기
계속 서명 문제를 일으킴

(3) vmd controller 끄기
계속 ssd 인식 문제를 일으킴

(4) 24.04 버전 사용
구버전은 최신 하드웨어들을 잘 못받는 커널임

(5) 화면 이슈. grub 커맨드
```
echo 'GRUB_CMDLINE_XEN="iommu=1 iommu=verbose dom0_mem=4096M,max:4096M"' | sudo tee /etc/default/grub.d/xen.cfg
echo 'GRUB_CMDLINE_LINUX_XEN_REPLACE="xen-pciback.hide=(01:00.0)(01:00.1) modprobe.blacklist=nvidia,nvidia_drm,nvidia_modeset,nouveau"' \
| sudo tee -a /etc/default/grub.d/xen.cfg
```
kvm을 하든 xen을 하든 grub에서 nvidia gpu를 잘 숨기는 게 관건인듯.

(6) ovmf대신 seabios로 vm 생성
seabios win10 하니까 vm 만들어짐

(7) libvmi conf 파일 형식
```
$ sudo nano /etc/libvmi.conf
win10-seabios {
    volatility_ist = "/root/symbols/ntkrnlmp.json";
}
```
하니까 연결됨

이거 이용하면 다른 방식도 성공할 수 있을걸로 생각.



### 관리 모드 들어가기

ubuntu on xen hypervisor 들어가서 

관리모드
```
lsmod | grep xen_pciback || sudo modprobe xen_pciback 
DRVDIR=$(ls -d /sys/bus/pci/drivers/pciback /sys/bus/pci/drivers/xen-pciback 2>/dev/null | head -n1)
echo 0000:01:00.1 | sudo tee /sys/bus/pci/devices/0000:01:00.1/driver/unbind
echo 0000:01:00.1 | sudo tee "$DRVDIR/bind"
sudo xl pci-assignable-list
랜선 연결
sudo xl create /etc/xen/win10-ing.cfg
hdmi 연결
vncviewer 127.0.0.1:5900
```    

### 일반 모드 들어가기

일반모드
```
lsmod | grep xen_pciback || sudo modprobe xen_pciback 
DRVDIR=$(ls -d /sys/bus/pci/drivers/pciback /sys/bus/pci/drivers/xen-pciback 2>/dev/null | head -n1)
echo 0000:01:00.1 | sudo tee /sys/bus/pci/devices/0000:01:00.1/driver/unbind
echo 0000:01:00.1 | sudo tee "$DRVDIR/bind"
sudo xl pci-assignable-list
랜선 연결
sudo xl create /etc/xen/win10.cfg
hdmi 연결
lsusb
sudo xl usbctrl-attach win10 type=auto version=3 ports=15
sudo xl usbdev-attach  win10 hostbus=1 hostaddr=5 controller=0 port=1
sudo xl usbdev-attach  win10 hostbus=1 hostaddr=6 controller=0 port=2
```

### 일반 모드 스크립트로 만들어놓음

아이콘으로 만들어놓음
```
$ mkdir -p ~/bin
$ nano ~/bin/win10-monitor-setup.sh
$ chmod +x ~/bin/win10-monitor-setup.sh
$ mkdir -p ~/.local/share/applications
$ nano ~/.local/share/applications/win10-monitor.desktop
$ chmod +x ~/.local/share/applications/win10-monitor.desktop
$ cp ~/.local/share/applications/win10-monitor.desktop ~/Desktop/
$ chmod +x ~/Desktop/win10-monitor.desktop
바탕화면 아이콘 우클누르고 allow launching
```
vcpu만 가끔씩 조절해주기

### 윈도우 업데이트 하고나면

```
sudo vmi-win-guid name win10
PDB GUID: f388d1c459c2dc9e81f891d0760a56cd1
source ~/vol3-venv/bin/activate
python3 -m volatility3.framework.symbols.windows.pdbconv \
  -p ntkrnlmp.pdb \
  -g f388d1c459c2dc9e81f891d0760a56cd1 \
  -o ntkrnlmp.json   
deactivate
sudo cp -i ~/ntkrnlmp.json /root/symbols/ntkrnlmp.json
y
```