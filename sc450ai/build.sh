#!/bin/sh
set -e
ROOT=/opt/aura-build
export PATH="$ROOT/bin:$ROOT/gcc/bin:$PATH"
cd "$ROOT/sc450ai"
make -C "$ROOT/linux-6.1.141" ARCH=arm64 CC=gcc HOSTCC=gcc M="$ROOT/sc450ai" modules
"$ROOT/gcc/bin/gcc" -O2 -static -o "$ROOT/sc450ai/livecap" "$ROOT/sc450ai/livecap.c" -lm
echo BUILT
ls -l "$ROOT/sc450ai/sc450ai.ko" "$ROOT/sc450ai/livecap"
