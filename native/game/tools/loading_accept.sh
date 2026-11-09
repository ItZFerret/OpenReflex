#!/bin/bash
# loading_accept.sh <reflex_boot> <REFLEX_PORT filter> [event: 1 (Concord, default) | alpine] [frames (1500)] [drive (1)]
# Acceptance for loading ports (native/game notes/loading.md): boots the event twice under baton scheduling (REFLEX_BOOT_SCHED=baton,
# the conversion's reference schedule): the original code, then with the ports installed for the whole boot; then
#  1. world_compare.py on the two canonical dumps (layout-free: same named heap nodes with the same contents) -> VERDICT,
#     plus the page-level diff of the world images (informational: identical when the ports keep the allocation order);
#  2. races both worlds <frames> frames with the deterministic REFLEX_DRIVE rider (the verify settings), natives off, and compares the
#     address-free race state (race_compare.py) and, when the layouts agree, the bike state / whole-memory hashes.
# VERIFY=1: also a third boot with the ports in Verify mode (REFLEX_PORT_VERIFY=1, per-call Verify inside the boot, notes/loading.md):
#  every verified call that differs is listed (port, calls, mismatches, unverifiable calls, the first difference); the Verify boot keeps
#  the original's results, so its world image must equal the original's byte for byte (a check of the boot's host-state undo).
# Outputs under $OUT (default: a fresh temporary folder, removed at the end; set OUT to keep the worlds).
B=${1:?reflex_boot}; P=${2:?filter}; E=${3:-1}; N=${4:-1500}; D=${5:-1}
S=${REFLEX_TESTDATA:-$(cd "$(dirname "$0")/../../.." && pwd)/testdata}   # test inputs: boot/worlds, crt, events (see testdata/README.md)
G="/Users/payton/Documents/Development/REFLEX/game files/Archive"
H=$(cd "$(dirname "$0")" && pwd)
if [ -z "$OUT" ]; then OUT=$(mktemp -d); trap 'rm -rf "${OUT:?}"' EXIT; fi
boot() {  # name extra-env...
  local n=$1; shift; mkdir -p "$OUT/$n/save"
  env REFLEX_BOOT_JIT=1 REFLEX_BOOT_SCHED=baton REFLEX_EVENT=$E REFLEX_WORLD_OUT="$OUT/$n/c" REFLEX_WORLD_CANON= "$@" \
    perl -e 'alarm 1800; exec @ARGV' "$B" "$G" $S/crt "$OUT/$n/save" 6000 "$OUT/$n/log.txt" > "$OUT/$n/out.txt" 2>&1
  if grep -q "written after" "$OUT/$n/out.txt"; then echo "boot $n: event started ($(grep -c '^port .* calls *[1-9]' "$OUT/$n/out.txt") ports called)"
  else echo "boot $n: FAILED $(grep -E 'STOPPED|FAILED' "$OUT/$n/out.txt" | head -1 | cut -c1-300)"; return 1; fi
}
race() {  # name extra-env...
  local n=$1; shift; mkdir -p "$OUT/$n/rsave"
  env REFLEX_AMBIENT_OFF=1 REFLEX_AMBIENT_KEEP=3ff REFLEX_PARTICLES=1 REFLEX_JIT=1 REFLEX_DRIVE=$D REFLEX_MEMHASH=1 REFLEX_WORLD_TEST="$OUT/$n/c" \
    REFLEX_RACE_STATE="$OUT/$n/race.txt" "$@" perl -e 'alarm 1800; exec @ARGV' "$B" "$G" $S/crt "$OUT/$n/rsave" $N "$OUT/$n/rlog.txt" 2>&1 | grep -E "state hash|memory hash|STOPPED" | sed 's/regs.*//'
}
boot ref & boot port REFLEX_PORT="$P" &
[ -n "$VERIFY" ] && boot ver REFLEX_PORT="$P" REFLEX_PORT_VERIFY=1 &
wait
if [ -n "$VERIFY" ] && [ -f "$OUT/ver/out.txt" ]; then
  v=$(grep "^port " "$OUT/ver/out.txt" | awk '$6 > 0')
  echo "verify: $(echo "$v" | grep -c .) ports called, $(echo "$v" | awk '$10 != 0' | grep -c .) with mismatches, $(echo "$v" | grep -c unverifiable) with unverifiable calls"
  echo "$v" | awk '$10 != 0' | cut -c1-400
  cmp -s "$OUT/ref/c-world.bin" "$OUT/ver/c-world.bin" && echo "verify boot world: identical to the original's" || echo "verify boot world: DIFFERENT from the original's (the host-state undo missed something: tell the infra agent)"
fi
[ -f "$OUT/ref/c-world.bin" ] && [ -f "$OUT/port/c-world.bin" ] || exit 1
python3 "$H/world_compare.py" "$OUT/ref/c-canon.bin" "$OUT/port/c-canon.bin" --examples 2 | head -40
python3 "$H/world_compare.py" --images "$OUT/ref/c-world.bin" "$OUT/port/c-world.bin"
race ref > "$OUT/ref/race.out" & race port > "$OUT/port/race.out" & wait
python3 "$H/race_compare.py" "$OUT/ref/race.txt" "$OUT/port/race.txt"
if diff -q "$OUT/ref/race.out" "$OUT/port/race.out" > /dev/null; then echo "race hashes: identical ($(tr '\n' ' ' < "$OUT/ref/race.out" | cut -c1-140))"
else echo "race hashes: different"; cat "$OUT/ref/race.out" "$OUT/port/race.out"; fi
