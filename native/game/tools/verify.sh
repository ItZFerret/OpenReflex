#!/bin/bash
# verify.sh <reflex_boot> <REFLEX_PORT filter> [quick] -- native/game acceptance for a set of ports:
#  1. Verify mode on four worlds driven by the deterministic random rider (REFLEX_DRIVE): every call of every selected port is run
#     natively and originally on the same state and compared: must print "mismatches 0" for every port that was called.
#  2. On mode (the natives alone) against no ports: the bike state hash and the whole-memory hash after the run must be identical.
# Worlds: the scratchpad's boot/worlds (VERIFY_WORLDS=<dir> instead): baton worlds as the plugin builds them since 2026-10-18
# (notes/loading.md section 9). Unity's default world settings (ambient classes 0x3ff, particles). Output: one line per check; exit 1 on any mismatch / difference.
B=${1:?reflex_boot}; P=${2:?filter}; Q=$3
# core (rx_*) filters: the On-mode memory hash leaves out the JobPool ring of dead job records (boot_main.cpp REFLEX_MEMHASH=nojobs)
MH=1; case "$P" in rx_*) MH=nojobs;; esac
S=${REFLEX_TESTDATA:-$(cd "$(dirname "$0")/../../.." && pwd)/testdata}   # test inputs: boot/worlds, crt, events (see testdata/README.md)
G="/Users/payton/Documents/Development/REFLEX/game files/Archive"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
run() {  # world frames drive extra-env...
  local w=$1 n=$2 d=$3; shift 3
  mkdir -p "$T/save"
  env REFLEX_AMBIENT_OFF=1 REFLEX_AMBIENT_KEEP=3ff REFLEX_PARTICLES=1 REFLEX_JIT=1 REFLEX_DRIVE=$d REFLEX_MEMHASH=$MH REFLEX_WORLD_TEST=${VERIFY_WORLDS:-$S/boot/worlds}/$w REFLEX_AFTER=$n "$@" \
    perl -e 'alarm shift; exec @ARGV' 1800 "$B" "$G" $S/crt "$T/save" $n "$T/log.txt" 2>"$T/err.txt"
}
if [ -n "$Q" ]; then WORLDS="National_NAT_Concord:1200:1"; else WORLDS="National_NAT_Concord:1800:1 National_NAT_Concord_ai11:1500:2 FreeRide_FR_Alpine:1500:3 SX_Springfield_ai11:1500:4 Terracross_TRX_Verde_ai11:1500:5"; fi
bad=0
for wf in $WORLDS; do
  IFS=: read w n d <<< "$wf"
  out=$(run $w $n $d REFLEX_PORT="$P" REFLEX_PORT_VERIFY=1)
  if echo "$out" | grep -q "WORLD STOPPED"; then echo "$w verify: STOPPED $(echo "$out" | grep STOPPED)"; bad=1; continue; fi
  ports=$(echo "$out" | grep "^port " | awk '$6 > 0'); called=$(echo "$ports" | grep -c .); mism=$(echo "$ports" | awk '$10 != 0' | grep -c .)
  echo "$w verify: $called ports called, $mism with mismatches"
  [ "$mism" -gt 0 ] && { echo "$ports" | awk '$10 != 0' | cut -c1-400; head -5 "$T/err.txt"; bad=1; }
  ref=$(run $w $n $d | grep -E "state hash|memory hash" | sed 's/regs.*//')
  nat=$(run $w $n $d REFLEX_PORT="$P" | grep -E "state hash|memory hash" | sed 's/regs.*//')
  if [ "$ref" == "$nat" ] && [ -n "$ref" ]; then echo "$w on: identical ($(echo "$ref" | tr '\n' ' ' | cut -c1-120))"; else echo "$w on: DIFFERENT"; echo " ref: $ref" | tr '\n' ' '; echo; echo " nat: $nat" | tr '\n' ' '; echo; bad=1; fi
done
exit $bad
