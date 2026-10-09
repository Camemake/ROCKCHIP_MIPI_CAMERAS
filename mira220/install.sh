#!/bin/sh
set -e
DIR=$(CDPATH= cd -- "$(dirname "$0")" && pwd)

if lsmod | grep -qE '^(gc4023|hm2170|ox05b1s|imx908|imx675|imx586|sc450ai|sc233hgs) '; then
	echo "Another camera is still loaded. Power-cycle the board, connect the MIRA220,"
	echo "and run this again. Do not unbind rkcif."
	exit 1
fi

if [ ! -d /proc/device-tree/i2c@21120000/mira220@54 ]; then
	dtc -@ -I dts -O dtb -o /tmp/mira220.dtbo "$DIR/mira220-overlay.dts"
	mkdir -p /sys/kernel/config/device-tree/overlays/mira220
	python3 - << 'PY'
import os
data = open("/tmp/mira220.dtbo", "rb").read()
base = "/sys/kernel/config/device-tree/overlays/mira220"
fd = os.open(base + "/dtbo", os.O_WRONLY)
os.write(fd, data)
os.close(fd)
fd = os.open(base + "/status", os.O_WRONLY)
os.write(fd, b"1\n")
os.close(fd)
print("overlay", open(base + "/status").read().strip())
PY
fi

if ! lsmod | grep -q '^mira220'; then
	insmod "$DIR/mira220.ko"
fi

if ! grep -q mira220 /sys/class/video4linux/*/name 2>/dev/null; then
	echo "MIRA220 probed but did not join the media graph."
	echo "Power-cycle the board and run this again. Do not unbind rkcif."
	dmesg | grep -i mira220 | tail -n 15
	exit 1
fi

echo "MIRA220 is on the graph. CIF RAW10 + AWB: python3 $DIR/isp/raw10_cif.py"
echo "RKAIQ IQ is $DIR/isp/mira220_camevision-mira220_default.json (hardware ISP not linked)."
