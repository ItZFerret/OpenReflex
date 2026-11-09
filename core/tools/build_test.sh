#!/bin/bash
# build_test.sh <out dir> -- builds core/tools/build_test (construction units + rx_canon) with the product flags (core/build.sh) from the
# core's infrastructure (src/*.c) and the construction files only (game/build_*.c, + the CRT ports game/crt_*.c they call; crt_math
# needs the x87 C++ files): other subsystems' files may be mid-edit.
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O"
CFLAGS=(-std=c11 -O2 -mcpu=apple-m1 -Wall -Wextra -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 -I"$C/include" -I"$C/src" -I"$C/game")
mkdir -p "$O/obj_bt"; rm -f "$O"/obj_bt/*.o
CRT=("$C"/game/crt_string.c "$C"/game/crt_runtime.c "$C"/game/crt_thread.c "$C"/game/crt_math.c)
for f in "$C"/src/*.c "$C"/game/build_*.c "${CRT[@]}" "$C"/tools/rx_canon.c "$C"/tools/build_test.c; do
  clang "${CFLAGS[@]}" -c -o "$O/obj_bt/$(basename "$f" .c).o" "$f"
done
CXXFLAGS=(-std=c++17 -O2 -mcpu=apple-m1 -Wall -Wextra -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 -I"$C/src")
for f in "$C"/src/rx_x87.cpp "$C"/src/f80/*.cpp; do clang++ "${CXXFLAGS[@]}" -c -o "$O/obj_bt/cpp_$(basename "$f" .cpp).o" "$f"; done
clang++ -arch arm64 -o "$O/build_test" "$O"/obj_bt/*.o -lz
echo "built $O/build_test"
