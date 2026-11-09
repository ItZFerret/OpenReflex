#!/bin/bash
# rng_ab.sh <reflex_boot> <out dir> -- the per-frame random-stream check of the product's audio-boundary code (w-rng): the five reference
# worlds as verify.sh runs them (testdata/boot/worlds, the same drive / ambient settings), each three times with the RNG probe
# (core/oracle/rng_probe.cpp: the main thread's _holdrand, g_GameRandSeed / Count at every frame end):
#   A  the originals
#   B  the core's engine-emitter start line (game/engine_startline.c) replacing the original's (core/oracle/rng_boundary_probe.cpp replace)
#   S  the control: the start line dropped, as the product did before w-rng (skip)
#   V  per call: the core's start line against the original's emitters (verify), one summary line per function
# then core/tools/rng_cmp.py --abs --by-frame A/B (must be 0 frames differing) and A/S (differs once the emitters rev).
B=${1:?reflex_boot}; O=${2:?out dir}; mkdir -p "$O"
C=$(cd "$(dirname "$0")" && pwd)
S=${REFLEX_TESTDATA:-$(cd "$C/../.." && pwd)/testdata}
G="/Users/payton/Documents/Development/REFLEX/game files/Archive"
RB=rx_RngbUpdate,rx_RngbEnable,rx_RngbPreExited,rx_RngbAudioCommand
WORLDS="National_NAT_Concord:1800:1 National_NAT_Concord_ai11:1500:2 FreeRide_FR_Alpine:1500:3 SX_Springfield_ai11:1500:4 Terracross_TRX_Verde_ai11:1500:5"
run() {  # mode world frames drive
  local x=$1 w=$2 n=$3 d=$4 P=rx_RngProbeFrame E=""
  case $x in B) P=$P,$RB; E="RX_RNGB=replace";; S) P=$P,$RB; E="RX_RNGB=skip";; V) P=$RB; E="RX_RNGB=verify";; esac
  mkdir -p "$O/save_$x$w"
  env REFLEX_AMBIENT_OFF=1 REFLEX_AMBIENT_KEEP=3ff REFLEX_PARTICLES=1 REFLEX_JIT=1 REFLEX_DRIVE=$d REFLEX_MEMHASH=nojobs REFLEX_WORLD_TEST=$S/boot/worlds/$w \
    REFLEX_AFTER=$n REFLEX_PORT=$P RX_RNG_PROBE=$O/$x.$w.rng $E "$B" "$G" $S/crt "$O/save_$x$w" $n "$O/$x.$w.log" > "$O/$x.$w.out" 2>&1
}
for wf in $WORLDS; do IFS=: read w n d <<< "$wf"; for x in A B S V; do run $x $w $n $d & done; wait; done
bad=0
for wf in $WORLDS; do
  IFS=: read w n d <<< "$wf"
  ab=$(python3 "$C/rng_cmp.py" "$O/A.$w.rng" "$O/B.$w.rng" --abs --by-frame | tail -1); python3 "$C/rng_cmp.py" "$O/A.$w.rng" "$O/B.$w.rng" --abs --by-frame > /dev/null || bad=1
  as=$(python3 "$C/rng_cmp.py" "$O/A.$w.rng" "$O/S.$w.rng" --abs --by-frame | tail -1)
  echo "$w: A/B $ab | A/S $as"
  echo "   state hash A $(grep -o 'state hash [0-9a-f]*' "$O/A.$w.out" | awk '{print $3}') B $(grep -o 'state hash [0-9a-f]*' "$O/B.$w.out" | awk '{print $3}') S $(grep -o 'state hash [0-9a-f]*' "$O/S.$w.out" | awk '{print $3}')"
  grep -h "\[rngb\].*calls" "$O/V.$w.out" | sed 's/^/   /'
  grep -h "\[rngb\].*calls" "$O/V.$w.out" | grep -qv "mismatches 0" && bad=1
done
exit $bad
