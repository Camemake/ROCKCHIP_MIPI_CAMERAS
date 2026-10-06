#!/bin/sh
# Install the GC4023 on a Luckfox Aura: overlay, module, then the viewer.
set -e
DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)

devmem 0x2000083c 32 0x00f80000
# CSI1 IO0 stays an input so the module crystal can run.
echo 129 > /sys/class/gpio/export 2>/dev/null || true
echo in > /sys/class/gpio/gpio129/direction 2>/dev/null || true
devmem 0x201c8080 32 0x00f00000
echo 137 > /sys/class/gpio/unexport 2>/dev/null || true

if [ ! -d /proc/device-tree/i2c@21120000/gc4023@29 ]; then
	dtc -@ -I dts -O dtb -o /tmp/gc4023.dtbo "$DIR/gc4023-overlay.dts"
	mkdir -p /sys/kernel/config/device-tree/overlays/gc4023
	python3 - "$DIR" << 'PY'
import os, sys
data = open("/tmp/gc4023.dtbo", "rb").read()
base = "/sys/kernel/config/device-tree/overlays/gc4023"
fd = os.open(base + "/dtbo", os.O_WRONLY)
os.write(fd, data)
os.close(fd)
fd = os.open(base + "/status", os.O_WRONLY)
os.write(fd, b"1\n")
os.close(fd)
print("overlay", open(base + "/status").read().strip())
PY
fi

if ! lsmod | grep -q '^gc4023'; then
	insmod "$DIR/gc4023.ko" link_mhz=351
fi
if ! lsmod | grep -q '^phy1clk'; then
	insmod "$DIR/phy1clk.ko"
fi

if ! grep -q gc4023 /sys/class/video4linux/*/name 2>/dev/null; then
	echo "GC4023 probed but did not join the media graph."
	echo "CSI1 was already running. Power-cycle the board and run this again."
	echo "Do not unbind rkcif: that hangs this kernel."
	dmesg | grep -i gc4023 | tail -n 15
	exit 1
fi
exec python3 "$DIR/live.py"
