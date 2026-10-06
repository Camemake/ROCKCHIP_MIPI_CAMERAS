#!/bin/sh
set -e
ROOT=/opt/aura-build
export PATH="$ROOT/bin:$ROOT/gcc/bin:$PATH"
cd "$ROOT/imx586"
make -C "$ROOT/linux-6.1.141" ARCH=arm64 CC=gcc HOSTCC=gcc M="$ROOT/imx586" modules
"$ROOT/gcc/bin/gcc" -O2 -static -o "$ROOT/imx586/livecap" "$ROOT/imx586/livecap.c" -lm
echo BUILT
ls -l "$ROOT/imx586/imx586.ko" "$ROOT/imx586/livecap"
