#!/bin/bash
# gridpair_stepbuild.sh <out dir> -- a scratch product probe whose bike_integrate_step (00431d60) calls bike_parity_probe.c's
# gridpair_step_hook at its entry (PROBE_STEP_TRACE / PROBE_STEP_SLOT: a per-step trace of one simulation in the oracle's
# rx_WhipStepTrace RX_WHIP_TRACE_WIDE format). The patched copy of game/bike_sim.c lives in <out dir> only; the product is not changed.
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=$(mkdir -p "${1:?out dir}" && cd "$1" && pwd)
cd "$C"   # native_tu_flags.sh reads tools/native_tu_flags.tsv relative to the core dir (w-parity4w: from elsewhere bike_sim.c lost its
          # -DRX_FOURWHEEL_REST_FRAME_OBSERVERS and every four-wheel rest test aborted)
source "$C/tools/product_link.sh"; source "$C/tools/native_tu_flags.sh"
rx_product_objs
python3 - "$C/game/bike_sim.c" "$O/bike_sim_hooked.c" <<'PY'
import sys
s = open(sys.argv[1]).read()
old = "void bike_integrate_step(rx_addr S, float dt) {\n"
assert old in s
s = s.replace(old, "void gridpair_step_hook(rx_addr S, float dt) __attribute__((weak));\n" + old + "    if (gridpair_step_hook) gridpair_step_hook(S, dt);\n", 1)
open(sys.argv[2], 'w').write('#line 1 "bike_sim.c"\n' + s)
PY
cd "$C"; rx_native_tu_flags game/bike_sim.c
clang -std=c11 -O2 -mcpu=apple-m1 -Wall -Wextra -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 -fvisibility=hidden -fPIC \
  -Iinclude -Isrc -Igame "${RX_NATIVE_TU_FLAGS[@]}" -c -o "$O/game_bike_sim_hooked.o" "$O/bike_sim_hooked.c"
OBJS=(); for o in "${RX_PRODUCT_OBJS[@]}"; do case $o in */game_bike_sim_c.o) OBJS+=("$O/game_bike_sim_hooked.o");; *) OBJS+=("$o");; esac; done
RX_PRODUCT_OBJS=("${OBJS[@]}")
rx_link_driver "$O/bike_parity_probe" "$C/tools/bike_parity_probe.c"
echo "built $O/bike_parity_probe (step hook)"
