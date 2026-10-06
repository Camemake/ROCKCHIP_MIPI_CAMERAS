#!/bin/sh
set -e
ROOT=/opt/aura-build
export PATH="$ROOT/bin:$ROOT/gcc/bin:$PATH"
cd "$ROOT/imx675"
make -C "$ROOT/linux-6.1.141" ARCH=arm64 CC=gcc HOSTCC=gcc M="$ROOT/imx675" modules
"$ROOT/gcc/bin/gcc" -O2 -static -o "$ROOT/imx675/livecap" "$ROOT/imx675/livecap.c" -lm
echo BUILT
ls -l "$ROOT/imx675/imx675.ko" "$ROOT/imx675/livecap"
