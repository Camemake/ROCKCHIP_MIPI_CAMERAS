#!/bin/sh
set -e
ROOT=/opt/aura-build
export PATH="$ROOT/bin:$ROOT/gcc/bin:$PATH"
cd "$ROOT/gc4023"
make -C "$ROOT/linux-6.1.141" ARCH=arm64 CC=gcc HOSTCC=gcc M="$ROOT/gc4023" modules
"$ROOT/gcc/bin/gcc" -O2 -static -o "$ROOT/gc4023/livecap" "$ROOT/gc4023/livecap.c" -lm
echo BUILT
ls -l "$ROOT/gc4023/gc4023.ko" "$ROOT/gc4023/livecap"
