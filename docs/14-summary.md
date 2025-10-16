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



### 관리

앞으로 ubuntu on xen hypervisor 들어가서 

관리모드
```
lsmod | grep xen_pciback || sudo modprobe xen_pciback 
DRVDIR=$(ls -d /sys/bus/pci/drivers/pciback /sys/bus/pci/drivers/xen-pciback 2>/dev/null | head -n1)
echo 0000:01:00.1 | sudo tee /sys/bus/pci/devices/0000:01:00.1/driver/unbind
echo 0000:01:00.1 | sudo tee "$DRVDIR/bind"
sudo xl pci-assignable-list
랜선 연결
sudo xl create /etc/xen/win10-all.cfg
hdmi 연결
vncviewer 127.0.0.1:5900
```

실전모드
```
lsmod | grep xen_pciback || sudo modprobe xen_pciback 
DRVDIR=$(ls -d /sys/bus/pci/drivers/pciback /sys/bus/pci/drivers/xen-pciback 2>/dev/null | head -n1)
echo 0000:01:00.1 | sudo tee /sys/bus/pci/devices/0000:01:00.1/driver/unbind
echo 0000:01:00.1 | sudo tee "$DRVDIR/bind"
sudo xl pci-assignable-list
랜선 연결
sudo xl create /etc/xen/win10-monitor.cfg
hdmi 연결
lsusb
sudo xl usbctrl-attach win10 type=auto version=3 ports=15
sudo xl usbdev-attach  win10 hostbus=1 hostaddr=5 controller=0 port=1
sudo xl usbdev-attach  win10 hostbus=1 hostaddr=6 controller=0 port=2
```

아이콘으로 만들어놓음
```
kkongnyang2@acer:~$ mkdir -p ~/bin
kkongnyang2@acer:~$ nano ~/bin/win10-monitor-setup.sh
#!/usr/bin/env bash
set -euo pipefail

# --------- Helper: pretty print ----------
say() { printf "\n\033[1;36m[ 안내 ]\033[0m %s\n" "$*"; }
run() { echo -e "\033[1;33m$ $*\033[0m"; eval "$@"; }

# --------- 0) 사용자 안내 ----------
say "랜선을 연결해주세요."
sleep 5

say "모니터를 켜고 케이블을 연결해주세요."
sleep 5

# --------- 1) xen-pciback 모듈 준비 ----------
if ! lsmod | grep -qE '^xen_pciback\s'; then
  say "xen_pciback 모듈을 적재합니다."
  run sudo modprobe xen_pciback
else
  say "xen_pciback 모듈이 이미 적재되어 있습니다."
fi

# --------- 2) GPU 오디오 함수(0000:01:00.1) pciback로 재바인딩 ----------
# (필요 시 장치 주소를 바꿔주세요)
BDF_AUDIO="0000:01:00.1"

# 드라이버 디렉터리 탐색
DRVDIR=$(ls -d /sys/bus/pci/drivers/pciback /sys/bus/pci/drivers/xen-pciback 2>/dev/null | head -n1 || true)
if [[ -z "${DRVDIR:-}" ]]; then
  echo "pciback 드라이버 디렉터리를 찾지 못했습니다."; exit 1
fi

# 바인딩 변경
if [[ -e "/sys/bus/pci/devices/$BDF_AUDIO/driver/unbind" ]]; then
  say "$BDF_AUDIO 장치를 현재 드라이버에서 분리(unbind)합니다."
  echo "$BDF_AUDIO" | sudo tee "/sys/bus/pci/devices/$BDF_AUDIO/driver/unbind" >/dev/null
fi
say "$BDF_AUDIO 장치를 pciback에 바인드합니다. ($DRVDIR/bind)"
echo "$BDF_AUDIO" | sudo tee "$DRVDIR/bind" >/dev/null

# 확인
say "지금 할당 가능한 PCI 리스트를 확인합니다."
run sudo xl pci-assignable-list || true

# --------- 3) 게스트 부팅 ----------
CFG="/etc/xen/win10-monitor.cfg"
if [[ ! -f "$CFG" ]]; then
  echo "Xen cfg 파일이 없습니다: $CFG"; exit 1
fi

say "윈도우 VM을 켭니다: $CFG"
run sudo xl create "$CFG"

say "켜는 중입니다. 미리 키보드와 마우스를 연결해주세요."
sleep 15

# --------- 4) USB 식별 (lsusb 파싱) ----------
say "lsusb 로 장치를 식별합니다."
run lsusb

# 대상 장치 ID (필요하면 아래 두 줄만 수정)
ID_MOUSE="3554:f58a"   # Compx VXE NordicMouse 1K Dongle
ID_KEYBD="1a2c:4c5e"   # China Resource Semico USB Keyboard

parse_bus_dev() {
  # stdin: lsusb 한 줄
  # stdout: "<bus> <dev>" (숫자)
  awk '{b=$2; d=$4; gsub(":","",d); printf("%d %d\n", b, d); exit}'
}

LSUSB_OUT="$(lsusb)"

MOUSE_LINE=$(grep -i "$ID_MOUSE" <<<"$LSUSB_OUT" || true)
KEYBD_LINE=$(grep -i "$ID_KEYBD" <<<"$LSUSB_OUT" || true)

if [[ -z "$MOUSE_LINE" || -z "$KEYBD_LINE" ]]; then
  echo "필요한 USB 장치를 찾지 못했습니다.
- 마우스($ID_MOUSE): ${MOUSE_LINE:-없음}
- 키보드($ID_KEYBD): ${KEYBD_LINE:-없음}
장치를 뺐다 다시 꽂은 뒤 다시 실행해보세요."
  exit 1
fi

read MOUSE_BUS MOUSE_DEV < <(echo "$MOUSE_LINE" | parse_bus_dev)
read KEYBD_BUS KEYBD_DEV < <(echo "$KEYBD_LINE" | parse_bus_dev)

say "감지된 장치:"
echo "  - Mouse : Bus ${MOUSE_BUS} Device ${MOUSE_DEV} ($ID_MOUSE)"
echo "  - Keyboard: Bus ${KEYBD_BUS} Device ${KEYBD_DEV} ($ID_KEYBD)"

# --------- 5) USB 컨트롤러/디바이스 부착 ----------
# 먼저 USB3 시도 (type=auto version=3), 실패 시 version=2로 폴백
say "USB 컨트롤러 부착 (우선 USB3 시도, 실패 시 USB2로 폴백)"
if ! sudo xl usbctrl-attach win10 type=auto version=3 ports=15; then
  echo "USB3 부착 실패 → USB2로 재시도"
  run sudo xl usbctrl-attach win10 type=auto version=2 ports=15
fi

# 포트 매핑: 마우스 port=1, 키보드 port=2
say "USB 디바이스를 컨트롤러에 부착합니다."
run sudo xl usbdev-attach win10 hostbus="${MOUSE_BUS}" hostaddr="${MOUSE_DEV}" controller=0 port=1
run sudo xl usbdev-attach win10 hostbus="${KEYBD_BUS}" hostaddr="${KEYBD_DEV}" controller=0 port=2

# --------- 6) 완료 ----------
say "설정이 완료되었습니다."
sleep 1
exit 0
kkongnyang2@acer:~$ chmod +x ~/bin/win10-monitor-setup.sh
kkongnyang2@acer:~$ mkdir -p ~/.local/share/applications
kkongnyang2@acer:~$ nano ~/.local/share/applications/win10-monitor.desktop
# 저장 경로: ~/.local/share/applications/win10-monitor.desktop
[Desktop Entry]
Type=Application
Name=Win10 모니터 부팅 & USB 세팅
Comment=랜선/모니터 안내 후 Xen VM 부팅 및 USB 자동 연결
Exec=gnome-terminal -- bash -lc '~/bin/win10-monitor-setup.sh'
Icon=computer
Terminal=false
Categories=Utility;
kkongnyang2@acer:~$ chmod +x ~/.local/share/applications/win10-monitor.desktop
kkongnyang2@acer:~$ cp ~/.local/share/applications/win10-monitor.desktop ~/Desktop/
kkongnyang2@acer:~$ chmod +x ~/Desktop/win10-monitor.desktop
바탕화면 아이콘 우클누르고 allow launching
```

업데이트 하고나면
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