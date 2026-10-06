#!/bin/sh
# Install the IMX586 on a Luckfox Aura: overlay, module, then the viewer.
# Power-cycle off the previous camera first. Do not unbind rkcif.
set -e
DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)

if lsmod | grep -Eq '^(gc4023|hm2170|ox05b1s|imx908|sc450ai|imx675) '; then
	echo "Another camera is still loaded. Power-cycle the board, connect the IMX586,"
	echo "and run this again. Do not unbind rkcif: that hangs this kernel."
	exit 1
fi

mount -t configfs none /sys/kernel/config 2>/dev/null || true
devmem 0x2000083c 32 0x00f80000
# CSI1 IO0 stays an input so the module crystal can run.
echo 129 > /sys/class/gpio/export 2>/dev/null || true
echo in > /sys/class/gpio/gpio129/direction 2>/dev/null || true
devmem 0x201c8080 32 0x00f00000
echo 137 > /sys/class/gpio/unexport 2>/dev/null || true

if [ ! -d /proc/device-tree/i2c@21120000/imx586cm@1a ]; then
	dtc -@ -I dts -O dtb -o /tmp/imx586.dtbo "$DIR/imx586-overlay.dts"
	mkdir -p /sys/kernel/config/device-tree/overlays/imx586
	python3 - "$DIR" << 'PY'
import os
data = open("/tmp/imx586.dtbo", "rb").read()
base = "/sys/kernel/config/device-tree/overlays/imx586"
fd = os.open(base + "/dtbo", os.O_WRONLY)
os.write(fd, data)
os.close(fd)
fd = os.open(base + "/status", os.O_WRONLY)
os.write(fd, b"1\n")
os.close(fd)
print("overlay", open(base + "/status").read().strip())
PY
fi

if ! lsmod | grep -q '^imx586'; then
	insmod "$DIR/imx586.ko" link_mhz=450
fi
if ! lsmod | grep -q '^phy1clk'; then
	if [ -f "$DIR/phy1clk.ko" ]; then
		insmod "$DIR/phy1clk.ko"
	else
		insmod /opt/aura-build/imx908/phy1clk.ko
	fi
fi

if ! grep -q imx586 /sys/class/video4linux/*/name 2>/dev/null; then
	echo "IMX586 probed but did not join the media graph."
	echo "CSI1 was already running. Power-cycle the board and run this again."
	echo "Do not unbind rkcif: that hangs this kernel."
	dmesg | grep -i imx586 | tail -n 15
	exit 1
fi
exec python3 "$DIR/live.py"
