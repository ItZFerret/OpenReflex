#!/bin/bash
# champ_verify.sh <reflex_boot> <out dir> [race|wp|cases|all (all)] -- verification of the championship race activity classes
# (core/game/race_champ.c, core/oracle/race_champ*.cpp). No reference world holds a championship activity (the career data of the PC game
# has no "Championship" series), so the classes are stimulated in the oracle (core/oracle/race_champ_probe.cpp):
#   build   an arcade event (National NAT_Concord / WaypointRace FR_Gibson) whose activity-name -> type map (FUN_005964b0) answers the
#           championship type: the original game builds, starts and runs that class (RX_CHAMP_TYPE, RX_CHAMP_FROM). Twice, in baton boots:
#           the original code, then the ports On (the factory 0055ef80 and every championship port): world_compare.py of the canonical dumps.
#   frames  the original's world image raced with the ports in Verify (REFLEX_PORT_VERIFY=1): every call of the per-frame virtuals and
#           the stunt handlers, core against original, memory and registers; then the On-mode hash comparison (verify.sh's).
#   cases   core/oracle/race_champ_cases.cpp: the rest of the virtuals (destructors, teardown, restart, camera placements, takeover,
#           listener templates, intro classes) called on a snapshot of the live activity (RX_CASES=race|wp), Verify.
# The types: 34 / 35 / 36 / 41 = NationalChampPractice / Heat / MainEvent / SupercrossChampLCQ (race classes), 48 / 49 =
# WaypointRaceChampPractice / MainEvent. The intro classes (31, 33) stop in the original's own scenario code (see the QUEUE entry).
set -e
B=${1:?reflex_boot}; O=${2:?out dir}; K=${3:-all}
R=$(cd "$(dirname "$0")/../.." && pwd); S=${REFLEX_TESTDATA:-$R/testdata}; G="$R/game files/Archive"
mkdir -p "$O/worlds"
PORTS=$(grep -o "RX_PORT(0x[0-9a-f]*" "$R/core/oracle/race_champ.cpp" | sed 's/RX_PORT(0x/00/' | tr '\n' ',' | sed 's/,$//')
build() {  # name spec type from extra-port
  local n=$1 spec=$2 t=$3 from=$4 pre=$5 mode=$6 d="$O/$1"
  mkdir -p "$d/save"
  local filter="rx_ChampTypeHook,$pre"; [ "$mode" == port ] && filter="$PORTS,0055ef80,$filter"
  env REFLEX_BOOT_JIT=1 REFLEX_BOOT_SCHED=baton REFLEX_EVENT=1 REFLEX_EVENT_SPEC="$spec" REFLEX_RIDERS=4 REFLEX_AI=95 REFLEX_WORLD_OUT="$d/c" REFLEX_WORLD_CANON= \
    REFLEX_PORT="$filter" RX_CHAMP_TYPE=$t RX_CHAMP_FROM=$from perl -e 'alarm 1800; exec @ARGV' "$B" "$G" $S/crt "$d/save" 6000 "$d/log.txt" > "$d/out.txt" 2>&1
  grep -q "written after" "$d/out.txt" || { echo "build $n: FAILED $(grep -E 'STOPPED|FAILED' "$d/out.txt" | head -1 | cut -c1-200)"; return 1; }
}
frames() {  # world frames
  local w=$1 n=$2 t=$(mktemp -d)
  local out=$(env REFLEX_AMBIENT_OFF=1 REFLEX_AMBIENT_KEEP=3ff REFLEX_PARTICLES=1 REFLEX_JIT=1 REFLEX_DRIVE=1 REFLEX_MEMHASH=nojobs REFLEX_WORLD_TEST="$O/worlds/$w" REFLEX_AFTER=$n \
    REFLEX_PORT="$PORTS" REFLEX_PORT_VERIFY=1 perl -e 'alarm shift; exec @ARGV' 1800 "$B" "$G" $S/crt "$t/save" $n "$t/log.txt" 2>&1)
  echo "$out" | grep -E "STOPPED" | head -2
  echo "$out" | grep "^port rx_Champ\|^port rx_Wp" | awk '$6>0{printf "  %s calls %s mismatches %s\n",$2,$6,$10}'
  for m in "" "$PORTS"; do :; done
  local ref=$(env REFLEX_AMBIENT_OFF=1 REFLEX_AMBIENT_KEEP=3ff REFLEX_PARTICLES=1 REFLEX_JIT=1 REFLEX_DRIVE=1 REFLEX_MEMHASH=nojobs REFLEX_WORLD_TEST="$O/worlds/$w" REFLEX_AFTER=$n perl -e 'alarm shift; exec @ARGV' 1800 "$B" "$G" $S/crt "$t/save" $n "$t/log.txt" 2>/dev/null | grep -E "state hash|memory hash" | sed 's/regs.*//')
  local nat=$(env REFLEX_AMBIENT_OFF=1 REFLEX_AMBIENT_KEEP=3ff REFLEX_PARTICLES=1 REFLEX_JIT=1 REFLEX_DRIVE=1 REFLEX_MEMHASH=nojobs REFLEX_WORLD_TEST="$O/worlds/$w" REFLEX_AFTER=$n REFLEX_PORT="$PORTS" perl -e 'alarm shift; exec @ARGV' 1800 "$B" "$G" $S/crt "$t/save" $n "$t/log.txt" 2>/dev/null | grep -E "state hash|memory hash" | sed 's/regs.*//')
  [ "$ref" == "$nat" ] && [ -n "$ref" ] && echo "  $w on: identical" || echo "  $w on: DIFFERENT"
  rm -rf "$t"
}
cases() {  # world kind
  local w=$1 kind=$2 t=$(mktemp -d)
  env REFLEX_AMBIENT_OFF=1 REFLEX_AMBIENT_KEEP=3ff REFLEX_PARTICLES=1 REFLEX_JIT=1 REFLEX_DRIVE=1 REFLEX_MEMHASH=nojobs REFLEX_WORLD_TEST="$O/worlds/$w" REFLEX_AFTER=1600 \
    REFLEX_PORT="$PORTS,rx_ChampCasesHook" REFLEX_PORT_VERIFY=1 RX_CASES=$kind RX_CASES_FRAME=1500 perl -e 'alarm shift; exec @ARGV' 1800 "$B" "$G" $S/crt "$t/save" 1600 "$t/log.txt" > "$t/out.txt" 2> "$t/err.txt" || true
  grep -E "cases\] [0-9]+ cases done" "$t/err.txt"
  grep -E "STOPPED" "$t/out.txt" | head -2
  grep "^port rx_Champ\|^port rx_Wp\|^port rx_Activity\|^port rx_RaceStub\|^port rx_Waypoint\|^port rx_Player" "$t/out.txt" | awk '$6>0{n++; if ($10>0) bad++} END {printf "  %d ports called, %d with mismatches\n", n, bad}'
  rm -rf "$t"
}
if [ "$K" == race ] || [ "$K" == all ]; then
  for t in 34 35 36 41; do
    build ref$t "National,10,NAT_Concord,50,0,3,Laps,2,0" $t 0 rx_ChampVf5Pre ref; build port$t "National,10,NAT_Concord,50,0,3,Laps,2,0" $t 0 rx_ChampVf5Pre port
    echo "race type $t build: $(python3 "$R/native/game/tools/world_compare.py" "$O/ref$t/c-canon.bin" "$O/port$t/c-canon.bin" | grep VERDICT)"
    for f in world deform deform-model; do cp "$O/ref$t/c-$f.bin" "$O/worlds/Champ_t$t-$f.bin"; done
    frames Champ_t$t 2500
  done
  cases Champ_t36 race
fi
if [ "$K" == wp ] || [ "$K" == all ]; then
  for t in 48 49; do
    build wref$t "WaypointRace,14,FR_Gibson,47,0,3,Laps,1,6" $t 6 rx_ChampVf5WpPre ref; build wport$t "WaypointRace,14,FR_Gibson,47,0,3,Laps,1,6" $t 6 rx_ChampVf5WpPre port
    echo "waypoint type $t build: $(python3 "$R/native/game/tools/world_compare.py" "$O/wref$t/c-canon.bin" "$O/wport$t/c-canon.bin" | grep VERDICT)"
    for f in world deform deform-model; do cp "$O/wref$t/c-$f.bin" "$O/worlds/ChampWp_t$t-$f.bin"; done
    frames ChampWp_t$t 3000
  done
  cases ChampWp_t49 wp
fi
