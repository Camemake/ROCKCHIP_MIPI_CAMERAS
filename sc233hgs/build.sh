#!/bin/sh
set -e
ROOT=/opt/aura-build
export PATH="$ROOT/bin:$ROOT/gcc/bin:$PATH"
cd "$ROOT/sc233hgs"
make -C "$ROOT/linux-6.1.141" ARCH=arm64 CC=gcc HOSTCC=gcc M="$ROOT/sc233hgs" modules
"$ROOT/gcc/bin/gcc" -O2 -static -o "$ROOT/sc233hgs/livecap" "$ROOT/sc233hgs/livecap.c" -lm
echo BUILT
ls -l "$ROOT/sc233hgs/sc233hgs.ko" "$ROOT/sc233hgs/livecap"
