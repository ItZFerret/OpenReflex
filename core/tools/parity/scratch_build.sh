#!/bin/bash
# scratch_build.sh <out dir> <patch.py> -- w-parity (development only): a scratch product probe. patch.py <core/game dir> <out dir>/src
# writes patched copies of some core/game/*.c files (same basenames); they replace the product's objects in the link of
# core/tools/bike_parity_probe.c (+ core/tools/parity/parity_hook.c). The product (core/build) is not changed.
#   patch_pairlog.py: 008732d0 calls parity_pair_hook (the C / c lines of core/tools/parity_pair.inc)
#   patch_displog.py: that and every Collision_ContactDispatch 0085ddd0 call (D / d lines, PARITY_NEAR / PARITY_DEPTH0)
# The oracle side: core/oracle/parity_probe.cpp (rx_ParityCompositeLog / rx_ParityDispatchLog, RX_PARITY_LOG=<file>).
set -e
C=$(cd "$(dirname "$0")/../.." && pwd); O=$(mkdir -p "${1:?out}" && cd "$1" && pwd); P=$(cd "$(dirname "$2")" && pwd)/$(basename "$2")
cd "$C"; source tools/product_link.sh; source tools/native_tu_flags.sh
rx_product_objs
rm -rf "$O/src"; mkdir -p "$O/src"; python3 "$P" "$C/game" "$O/src"
for f in "$O"/src/*.c; do b=$(basename "$f" .c); rx_native_tu_flags "game/$b.c"
  clang -std=c11 -O2 -mcpu=apple-m1 -Wall -Wextra -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 -fvisibility=hidden -fPIC \
    -Iinclude -Isrc -Igame "${RX_NATIVE_TU_FLAGS[@]}" -c -o "$O/src/game_${b}_c.o" "$f"
done
OBJS=(); for o in "${RX_PRODUCT_OBJS[@]}"; do b=$(basename "$o"); if [ -f "$O/src/$b" ]; then OBJS+=("$O/src/$b"); else OBJS+=("$o"); fi; done
RX_PRODUCT_OBJS=("${OBJS[@]}")
rx_link_driver "$O/bike_parity_probe" "$C/tools/bike_parity_probe.c" "$C/tools/parity/parity_hook.c"
echo "built $O/bike_parity_probe"
