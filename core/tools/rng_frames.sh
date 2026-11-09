#!/bin/bash
# rng_frames.sh <out dir> -- builds core/tools/rng_frames (the product's random streams per frame, core/tools/rng_frames.c) with the product
# link shape (src/*.c, src/*.cpp, src/f80/*.cpp, every game file with its native_tu_flags.tsv define, api/*.c and the driver).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; mkdir -p "$O/eobj" "$O/gen"
CFLAGS=(-std=c11 -O2 -mcpu=apple-m1 -Wall -Wextra -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 -I"$C/include" -I"$C/src" -I"$C/game" -I"$C/api")
CXXFLAGS=(-std=c++17 -O2 -mcpu=apple-m1 -Wall -Wextra -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 -I"$C/include" -I"$C/src")
OBJS=()
for f in "$C"/src/*.cpp "$C"/src/f80/*.cpp; do
  [ -f "$f" ] || continue; o="$O/eobj/$(basename "$f").o"; OBJS+=("$o")
  [ -f "$o" ] && [ "$o" -nt "$f" ] || clang++ "${CXXFLAGS[@]}" -c -o "$o" "$f"
done
NEWEST=$(ls -t "$C"/include/*.h "$C"/src/*.h "$C"/game/*.h "$C"/api/*.h "$C"/api/*.inc 2>/dev/null | head -1)
python3 "$C/tools/gen_fnsig.py" "$O/gen/rx_fnsig.c"
cd "$C"; source tools/native_tu_flags.sh
SRCS=("$C"/src/*.c "$C"/game/*.c "$C"/api/*.c "$C"/tools/rng_frames.c "$O/gen/rx_fnsig.c")
pids=()
for f in "${SRCS[@]}"; do
  o="$O/eobj/$(basename "$f").o"; rx_native_tu_flags "game/$(basename "$f")"
  if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ "$NEWEST" -nt "$o" ]; then (clang "${CFLAGS[@]}" "${RX_NATIVE_TU_FLAGS[@]}" -c -o "$o" "$f" 2>"$o.err" || rm -f "$o") & pids+=($!); fi
  if [ "${#pids[@]}" -ge "${RX_BUILD_JOBS:-8}" ]; then wait "${pids[@]}"; pids=(); fi
done
wait
for f in "${SRCS[@]}"; do o="$O/eobj/$(basename "$f").o"; if [ -f "$o" ]; then OBJS+=("$o"); else echo "does not compile: $(basename "$f")"; head -5 "$o.err"; exit 1; fi; done
clang++ -arch arm64 -o "$O/rng_frames" "${OBJS[@]}" -lz
echo "built $O/rng_frames"
