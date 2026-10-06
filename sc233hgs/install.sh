#!/bin/sh
# Install the SC233HGS on a Luckfox Aura: overlay, module, then the viewer.
# Power-cycle off the previous camera first. Do not unbind rkcif.
set -e
DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)

if lsmod | grep -q '^gc4023' || lsmod | grep -q '^hm2170' || lsmod | grep -q '^ox05b1s' || lsmod | grep -q '^imx908' || lsmod | grep -q '^imx675' || lsmod | grep -q '^imx586' || lsmod | grep -q '^sc450ai'; then
	echo "Another camera is still loaded. Power-cycle the board, connect the SC233HGS,"
	echo "and run this again. Do not unbind rkcif: that hangs this kernel."
	exit 1
fi

devmem 0x2000083c 32 0x00f80000
echo 129 > /sys/class/gpio/export 2>/dev/null || true
echo in > /sys/class/gpio/gpio129/direction 2>/dev/null || true
devmem 0x201c8080 32 0x00f00000
echo 137 > /sys/class/gpio/unexport 2>/dev/null || true

if [ ! -d /proc/device-tree/i2c@21120000/sc233hgs@30 ]; then
	dtc -@ -I dts -O dtb -o /tmp/sc233hgs.dtbo "$DIR/sc233hgs-overlay.dts"
	mkdir -p /sys/kernel/config/device-tree/overlays/sc233hgs
	python3 - << 'PY'
import os
data = open("/tmp/sc233hgs.dtbo", "rb").read()
base = "/sys/kernel/config/device-tree/overlays/sc233hgs"
fd = os.open(base + "/dtbo", os.O_WRONLY)
os.write(fd, data)
os.close(fd)
fd = os.open(base + "/status", os.O_WRONLY)
os.write(fd, b"1\n")
os.close(fd)
print("overlay", open(base + "/status").read().strip())
PY
fi

if ! lsmod | grep -q '^sc233hgs'; then
	insmod "$DIR/sc233hgs.ko" link_mhz=270
fi
if ! lsmod | grep -q '^phy1clk'; then
	for phy in "$DIR/phy1clk.ko" /opt/aura-build/imx908/phy1clk.ko /opt/aura-build/ox05b1s/phy1clk.ko /opt/aura-build/hm2170/phy1clk.ko /opt/aura-build/phy1clk.ko; do
		if [ -f "$phy" ]; then
			insmod "$phy"
			break
		fi
	done
fi

if ! grep -q sc233hgs /sys/class/video4linux/*/name 2>/dev/null; then
	echo "SC233HGS probed but did not join the media graph."
	echo "CSI1 was already running. Power-cycle the board and run this again."
	echo "Do not unbind rkcif: that hangs this kernel."
	dmesg | grep -i sc233hgs | tail -n 15
	exit 1
fi
exec python3 "$DIR/live.py"
