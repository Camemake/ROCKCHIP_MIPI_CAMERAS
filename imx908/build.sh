#!/bin/sh
set -e
ROOT=/opt/aura-build
export PATH="$ROOT/bin:$ROOT/gcc/bin:$PATH"
cd "$ROOT/imx908"
make -C "$ROOT/linux-6.1.141" ARCH=arm64 CC=gcc HOSTCC=gcc M="$ROOT/imx908" modules
"$ROOT/gcc/bin/gcc" -O2 -static -o "$ROOT/imx908/livecap" "$ROOT/imx908/livecap.c" -lm
echo BUILT
ls -l "$ROOT/imx908/imx908.ko" "$ROOT/imx908/livecap"
