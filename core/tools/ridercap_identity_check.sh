#!/bin/bash
# ridercap_identity_check.sh <out dir> <install root> [git ref (main)] [events (nat sx px nat8)] -- w-rc-core: a race of 12 or fewer riders
# is the original's byte for byte after the rider cap (core/plan/RIDER_CAP.md "Done"). Builds the product core of this checkout and of
# <git ref> (its core/ exported with git archive), links core/tools/jobthreads_hash.c against each, runs the same events with both
# (RX_JOB_THREADS=1, the whole arena hashed every 600 of 4200 race frames: image (with the rider cap's side storage at 0x0e000000 ..),
# heap, DLL, stack) and compares the HASH lines. Passes when every event's lines are equal.
#   nat  = National NAT_Concord, 12 riders      sx = Supercross SX_Kingston, 12 riders
#   px   = Powercross PX_Laguna UTV, 12 riders  nat8 = National NAT_Concord, 8 riders
# Two runs at a time; RX_NICE=<n> runs everything at that nice level (a shared machine); RX_BUILD_JOBS bounds the builds.
set -e
C=$(cd "$(dirname "$0")/.." && pwd); ROOT=$(cd "$C/.." && pwd)
O=${1:?out dir}; G=${2:?install root}; REF=${3:-main}; EVENTS=${4:-nat sx px nat8}
mkdir -p "$O"; O=$(cd "$O" && pwd)
export RX_BUILD_JOBS=${RX_BUILD_JOBS:-2}
N=(); [ -n "${RX_NICE:-}" ] && N=(nice -n "$RX_NICE")
# the reference core
rm -rf "$O/ref"; mkdir -p "$O/ref"
git -C "$ROOT" archive "$REF" core | tar -x -C "$O/ref"
build() {   # <core dir> <exe>
  local c=$1 exe=$2
  (cd "$c" && "${N[@]}" bash build.sh > "$O/$(basename "$exe").build.log" 2>&1) || { echo "build failed: $c (log $O/$(basename "$exe").build.log)"; exit 1; }
  local objs=() l
  while IFS= read -r l; do case $l in /*) objs+=("$l");; *) objs+=("$c/$l");; esac; done < "$c/build/objs.list"
  clang -std=c11 -O2 -mcpu=apple-m1 -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 \
    -I"$c/include" -I"$c/src" -I"$c/game" -I"$c/api" -c -o "$exe.o" "$c/tools/jobthreads_hash.c"
  clang++ -arch arm64 -o "$exe" "$exe.o" "${objs[@]}" -lz
}
build "$O/ref/core" "$O/jth_ref"
build "$C" "$O/jth_cap"
ev() { case $1 in nat) echo "National 10 NAT_Concord 50 0 2 3 Laps 12 95";; sx) echo "Supercross 8 SX_Kingston 74 0 2 3 Laps 12 95";;
                  px) echo "Powercross 16 PX_Laguna 58 0 22 3 Laps 12 95";; nat8) echo "National 10 NAT_Concord 50 0 2 3 Laps 8 95";; esac; }
run() { RX_JOB_THREADS=1 JT_HASH=600 "${N[@]}" "$1" "$G" $(ev "$2") - 3000 4200 17 2>/dev/null | grep '^HASH' > "$3" || true; }
fail=0
for e in $EVENTS; do
  run "$O/jth_ref" "$e" "$O/$e.ref" & run "$O/jth_cap" "$e" "$O/$e.cap" & wait
  if [ ! -s "$O/$e.ref" ]; then echo "$e: no hash lines (reference)"; fail=1
  elif cmp -s "$O/$e.ref" "$O/$e.cap"; then echo "$e: identical ($(wc -l < "$O/$e.ref" | tr -d ' ') hashes)"
  else echo "$e: DIFFERENT"; diff "$O/$e.ref" "$O/$e.cap" | head -4; fail=1; fi
done
[ $fail = 0 ] && echo "PASS: 12-rider races identical to $REF" || { echo "FAIL"; exit 1; }
