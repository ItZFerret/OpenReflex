#!/bin/bash
# input_sweep.sh <out dir> -- links core/tools/input_sweep.c against the product objects of core/build (run core/build.sh first).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"
clang -std=c11 -O2 -mcpu=apple-m1 -Wall -Wextra -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 -I"$C/include" -I"$C/src" -I"$C/game" -I"$C/api" -c -o "$O/input_sweep.o" "$C/tools/input_sweep.c"
clang++ -arch arm64 -o "$O/input_sweep" "$O/input_sweep.o" "$C"/build/obj/*.o -lz
echo "built $O/input_sweep"
