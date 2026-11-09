#!/bin/bash
# build_cli.sh <out dir> -- incremental build of the reflex_boot CLI (world runtime + native/game ports) into <out dir>/reflex_boot.
# Each agent / checkout uses its own <out dir> (objects are not shared).
# RX_BUILD_JOBS bounds compiler concurrency (default 2); failed compiles fail the build.
set -e
set -o pipefail
JOBS=${RX_BUILD_JOBS:-2}
case "$JOBS" in ''|*[!0-9]*|0) echo "RX_BUILD_JOBS must be a positive integer" >&2; exit 2;; esac
wait_batch() { for p in "${pids[@]}"; do wait "$p"; done; pids=(); }
limit_jobs() { if [ "${#pids[@]}" -ge "$JOBS" ]; then wait_batch; fi; }
O=${1:?out dir}; mkdir -p "$O/obj"
cd "$(dirname "$0")/../../drafts/world_runtime"
CS=../../../_re/.venv/lib/python3.14/site-packages/capstone
FLAGS=(-std=c++17 -O3 -mcpu=apple-m1 -Wall -Wextra -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 -I. -I../../game -I$CS/include -I../../../core/include)
eval "$(grep "^SRC=" build.sh)"
# objects built with other flags are stale (e.g. -fwrapv added 2026-10-18): start over
STAMP="$O/obj/.flags"; FL="${FLAGS[*]}"
if [ ! -f "$STAMP" ] || [ "$(cat "$STAMP")" != "$FL" ]; then rm -f "$O"/obj/*.o; echo "$FL" > "$STAMP"; fi
# REFLEX_PORT_FILES="math.cpp bike*.cpp": only these files of native/game/ports (others' work in progress stays out of the build)
if [ -n "$REFLEX_PORT_FILES" ]; then
  KEEP=(); for f in "${SRC[@]}"; do case "$f" in ../../game/ports/*) ;; *) KEEP+=("$f");; esac; done
  for g in $REFLEX_PORT_FILES; do for f in ../../game/ports/$g; do [ -f "$f" ] && KEEP+=("$f"); done; done
  SRC=("${KEEP[@]}")
fi
OBJS=()
pids=()
for f in boot_main.cpp "${SRC[@]}"; do
  o="$O/obj/$(echo "$f" | tr '/.' '__').o"; OBJS+=("$o")
  dep=$(ls -t "$f" *.h *.inc f80/*.h ../../game/*.h ../../game/ports/*.h 2>/dev/null | sed -n 1p)
  if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ "$dep" -nt "$o" ]; then clang++ "${FLAGS[@]}" -c -o "$o" "$f" 2>&1 | { grep -E "error|warning: unused (variable|function)" || true; } & pids+=($!); limit_jobs; fi
done
# OPEN REFLEX core (REFLEX/core): its C game code compiled with RX_ORACLE + the oracle adapters, so verify.sh can check every core
# function against the original (core/README.md). REFLEX_NO_CORE=1 leaves it out.
if [ -z "$REFLEX_NO_CORE" ]; then
  CORE=../../../core
  CFLAGS=(-std=c11 -O2 -mcpu=apple-m1 -Wall -Wextra -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 -DRX_ORACLE -I$CORE/include -I$CORE/src -I$CORE/game)
  # RX_CORE_FILES="phys_* bike_*": only those core/game/*.c and core/oracle/*.cpp (others' work in progress stays out)
  # w-parity5: the four-wheel families' finite dependency boundaries are compiled in (RX_ORACLE_FOURWHEEL: their files' product
  # sides, oracle_unclosed.cpp's seams left out) with the product's per-translation-unit defines (core/tools/native_tu_flags.tsv, e.g.
  # bike_sim.c's RX_FOURWHEEL_REST_FRAME_OBSERVERS), so the UTV / Buggy / TrophyTruck compositions run On / Verify in the original's
  # world. REFLEX_NO_FOURWHEEL=1 builds the old seams.
  [ -z "$REFLEX_NO_FOURWHEEL" ] && CFLAGS+=(-DRX_ORACLE_FOURWHEEL) && FWFLAGS=(-DRX_ORACLE_FOURWHEEL)
  pushd $CORE > /dev/null; source tools/native_tu_flags.sh; popd > /dev/null
  [ -n "$REFLEX_NO_FOURWHEEL" ] && rx_native_tu_flags() { RX_NATIVE_TU_FLAGS=(); }
  sel() { [ -z "$RX_CORE_FILES" ] && return 0; local g; set -f; for g in $RX_CORE_FILES; do case "$(basename "$1")" in $g) set +f; return 0;; esac; done; set +f; return 1; }
  for f in $CORE/src/rx_x86.c $CORE/src/rx_fn.c $CORE/src/rx_d3d.c $CORE/src/fmt_*.c $CORE/game/*.c; do
    [ -f "$f" ] || continue; case "$f" in */game/*) sel "$f" || continue;; esac
    # product-only files that refuse the oracle build on purpose (#error under RX_ORACLE): highlight_progress.c (needs a Steam provider)
    grep -q '^#error' "$f" && grep -B1 '^#error' "$f" | grep -q 'RX_ORACLE' && continue
    o="$O/obj/core_$(basename "$f" .c).o"; OBJS+=("$o")
    dep=$(ls -t "$f" $CORE/src/*.h $CORE/src/*.inc $CORE/game/*.h $CORE/tools/native_tu_flags.tsv 2>/dev/null | sed -n 1p)
    rx_native_tu_flags "${f#$CORE/}"
    if [ ! -f "$o" ] || [ "$dep" -nt "$o" ]; then clang "${CFLAGS[@]}" "${RX_NATIVE_TU_FLAGS[@]}" -c -o "$o" "$f" 2>&1 | { grep -E "error|warning" || true; } & pids+=($!); limit_jobs; fi
  done
  for f in $CORE/src/rx_x87.cpp $CORE/src/f80/f80.cpp $CORE/oracle/*.cpp; do
    case "$f" in */oracle/*) case "$(basename "$f")" in oracle.cpp) ;; *) sel "$f" || continue;; esac;; esac
    o="$O/obj/core_cpp_$(basename "$f" .cpp).o"; OBJS+=("$o")
    dep=$(ls -t "$f" $CORE/oracle/*.h $CORE/game/*.h $CORE/src/*.h $CORE/tools/*.inc ../../game/*.h 2>/dev/null | sed -n 1p)
    if [ ! -f "$o" ] || [ "$dep" -nt "$o" ]; then clang++ "${FLAGS[@]}" "${FWFLAGS[@]}" -I$CORE/src -I$CORE/game -c -o "$o" "$f" 2>&1 | { grep -E "error|warning: unused (variable|function)" || true; } & pids+=($!); limit_jobs; fi
  done
fi
wait_batch
clang++ -arch arm64 -o "$O/reflex_boot" "${OBJS[@]}" -Llib -lcapstone.5 -lz -Wl,-rpath,$PWD/lib && echo "built $O/reflex_boot"
