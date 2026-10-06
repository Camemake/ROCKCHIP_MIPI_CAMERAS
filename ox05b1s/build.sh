#!/bin/sh
set -e
ROOT=/opt/aura-build
export PATH="$ROOT/bin:$ROOT/gcc/bin:$PATH"
cd "$ROOT/ox05b1s"
make -C "$ROOT/linux-6.1.141" ARCH=arm64 CC=gcc HOSTCC=gcc M="$ROOT/ox05b1s" modules
"$ROOT/gcc/bin/gcc" -O2 -static -o "$ROOT/ox05b1s/livecap" "$ROOT/ox05b1s/livecap.c" -lm
echo BUILT
ls -l "$ROOT/ox05b1s/ox05b1s.ko" "$ROOT/ox05b1s/livecap"
