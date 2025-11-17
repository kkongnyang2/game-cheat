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
BDF_AUDIO="0000:01:00.1"

DRVDIR=$(ls -d /sys/bus/pci/drivers/pciback /sys/bus/pci/drivers/xen-pciback 2>/dev/null | head -n1 || true)
if [[ -z "${DRVDIR:-}" ]]; then
  echo "pciback 드라이버 디렉터리를 찾지 못했습니다."; exit 1
fi

if [[ -e "/sys/bus/pci/devices/$BDF_AUDIO/driver/unbind" ]]; then
  say "$BDF_AUDIO 장치를 현재 드라이버에서 분리(unbind)합니다."
  echo "$BDF_AUDIO" | sudo tee "/sys/bus/pci/devices/$BDF_AUDIO/driver/unbind" >/dev/null
fi
say "$BDF_AUDIO 장치를 pciback에 바인드합니다. ($DRVDIR/bind)"
echo "$BDF_AUDIO" | sudo tee "$DRVDIR/bind" >/dev/null

# --------- 3) 게스트 부팅 ----------
CFG="/etc/xen/win10.cfg"
if [[ ! -f "$CFG" ]]; then
  echo "Xen cfg 파일이 없습니다: $CFG"; exit 1
fi

say "윈도우 VM을 켭니다: $CFG"
run sudo xl create "$CFG"

say "켜는 중입니다. 미리 키보드와 마우스를 연결해주세요."
sleep 40

# --------- 4) USB 식별 (lsusb 파싱) ----------
say "lsusb 로 장치를 식별합니다."
run lsusb

ID_MOUSE="3554:f58a"   # Compx VXE NordicMouse 1K Dongle
ID_KEYBD="1a2c:4c5e"   # China Resource Semico USB Keyboard

parse_bus_dev() {
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
  read -r -p "엔터를 누르면 창이 닫힙니다..." _ 
  exit 1
fi

read MOUSE_BUS MOUSE_DEV < <(echo "$MOUSE_LINE" | parse_bus_dev)
read KEYBD_BUS KEYBD_DEV < <(echo "$KEYBD_LINE" | parse_bus_dev)

say "감지된 장치:"
echo "  - Mouse : Bus ${MOUSE_BUS} Device ${MOUSE_DEV} ($ID_MOUSE)"
echo "  - Keyboard: Bus ${KEYBD_BUS} Device ${KEYBD_DEV} ($ID_KEYBD)"

# --------- 5) USB 컨트롤러/디바이스 부착 ----------
# USB3만 시도 (폴백 제거)
say "USB 컨트롤러 부착 (USB3만 시도)"
run sudo xl usbctrl-attach win10 type=auto version=3 ports=15

# 포트 매핑: 마우스 port=1, 키보드 port=2
say "USB 디바이스를 컨트롤러에 부착합니다."
run sudo xl usbdev-attach win10 hostbus="${MOUSE_BUS}" hostaddr="${MOUSE_DEV}" controller=0 port=1
run sudo xl usbdev-attach win10 hostbus="${KEYBD_BUS}" hostaddr="${KEYBD_DEV}" controller=0 port=2
sleep 10

# --------- 6) vCPU 설정 & 핀ning ----------
# 6-1) dom0 (id=0) 고정: vcpu 8개, 12-19 코어에 핀
say "dom0(vmid=0) vCPU 개수 및 핀 설정"
run sudo xl vcpu-set 0 8
run sudo xl vcpu-pin 0 all 12-19

# 6-2) win10의 domain id 가져오기
WIN_ID=$(sudo xl list | awk '$1=="win10"{print $2; exit}')
if [[ -z "${WIN_ID:-}" ]]; then
  echo "xl list 에서 win10 도메인 ID를 찾지 못했습니다. 수동 확인이 필요합니다."
else
  say "win10 도메인 ID: $WIN_ID"
  # 숫자는 고정, id만 인자로
  run sudo xl vcpu-pin "$WIN_ID" 0 0
  run sudo xl vcpu-pin "$WIN_ID" 1 2
  run sudo xl vcpu-pin "$WIN_ID" 2 4
  run sudo xl vcpu-pin "$WIN_ID" 3 6
  run sudo xl vcpu-pin "$WIN_ID" 4 8
  run sudo xl vcpu-pin "$WIN_ID" 5 10
fi

# --------- 7) 완료 ----------
say "설정이 완료되었습니다."
read -r -p "엔터를 누르면 창이 닫힙니다..." _
exit 0
