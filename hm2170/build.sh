#!/bin/sh
set -e
ROOT=/opt/aura-build
export PATH="$ROOT/bin:$ROOT/gcc/bin:$PATH"
cd "$ROOT/hm2170"
make -C "$ROOT/linux-6.1.141" ARCH=arm64 CC=gcc HOSTCC=gcc M="$ROOT/hm2170" modules
"$ROOT/gcc/bin/gcc" -O2 -static -o "$ROOT/hm2170/livecap" "$ROOT/hm2170/livecap.c" -lm
echo BUILT
ls -l "$ROOT/hm2170/hm2170.ko" "$ROOT/hm2170/livecap"
