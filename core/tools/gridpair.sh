#!/bin/bash
# gridpair.sh <out dir> <side: oracle|product|both> <frames> [VAR=value ...] -- the paired ride (w-gridpair): the shipped product's core
# (core/tools/bike_parity_probe.c, built by gridpair_build.sh) and the original (a baton boot of the same event in the oracle CLI, from
# process start) under the same scripted pad (RX_WHIP_MODE=turn|whip and the RX_TURN_* / RX_WHIP_* knobs of
# core/oracle/bike_whip_reverse_probe.cpp, which bike_parity_probe.c mirrors), each writing core/tools/gridpair_line.inc's per-frame
# line; then gridpair_cmp.py <out>/pair-product.txt <out>/pair-oracle.txt.
#   env  RB         the oracle CLI (default builds/gridpair/cli/reflex_boot)
#        PB         the product probe (default builds/gridpair/probe/bike_parity_probe)
#        ACCEPT_AT  the oracle's loading-page accept frame (REFLEX_ACCEPT_AT; default 30 = the boot's own: its grid draw gives the local
#                   StartZone 5, the product's; 1 = the player accepts at once: zone 2)
#        RESEED=1   both sides reseed both generators every frame after the countdown entry (PROBE_RESEED / RX_PAIR_RESEED, test hooks)
#        CLOCK_AT   frame:word for the product's PROBE_CLOCK_AT (the engine clock set to the oracle's at the matching frame; a test hook)
#        PORTS      extra oracle ports (e.g. rx_bike,rx_phys: the native code inside the original's world)
#        SPEC       the event (default the product's reference event: National NAT_Concord, class 2, 3 laps, 12 riders, AI 95)
# <frames>: the product's frames from the session start; the oracle runs <frames> - 40 after its event start (its own loading frames
# are inside start_event).
set -e
R=$(cd "$(dirname "$0")/../.." && pwd); O=$(mkdir -p "${1:?out dir}" && cd "$1" && pwd); side=${2:?side}; n=${3:?frames}; shift 3
RB=${RB:-$R/builds/gridpair/cli/reflex_boot}; PB=${PB:-$R/builds/gridpair/probe/bike_parity_probe}
G="$R/game files/Archive"; S=${REFLEX_TESTDATA:-$R/testdata}
SPEC=${SPEC:-National,10,NAT_Concord,50,0,2,Laps,3,0}
mode=turn; for kv in "$@"; do case $kv in RX_WHIP_MODE=*) mode=${kv#RX_WHIP_MODE=};; esac; done
if [ "$side" = oracle ] || [ "$side" = both ]; then
  rm -rf "$O/save"; mkdir -p "$O/save"
  env REFLEX_BOOT_JIT=1 REFLEX_BOOT_SCHED=baton REFLEX_EVENT=1 REFLEX_EVENT_SPEC="$SPEC" REFLEX_RIDERS=12 REFLEX_AI=95 \
    REFLEX_ACCEPT_AT=${ACCEPT_AT:-30} RX_WHIP_AFTER_COUNTDOWN=1 ${RESEED:+RX_PAIR_RESEED=1} REFLEX_AFTER=$(( n > 41 ? n - 40 : 1 )) REFLEX_PORT=rx_PairFrameTrace,rx_PairGridTrace,rx_WhipReversePadHook${PORTS:+,$PORTS} \
    RX_PAIR_TRACE="$O/pair-oracle.txt" RX_WHIP_MODE=$mode RX_WHIP_QUANT=60 "$@" \
    nice perl -e 'alarm shift; exec @ARGV' 7200 "$RB" "$G" "$S/crt" "$O/save" 6000 "$O/log-oracle.txt" > "$O/stdout-oracle.txt" 2> "$O/stderr-oracle.txt" &
  op=$!
fi
if [ "$side" = product ] || [ "$side" = both ]; then
  env PROBE_MODE=$mode PROBE_PAIR="$O/pair-product.txt" ${RESEED:+PROBE_RESEED=1} ${CLOCK_AT:+PROBE_CLOCK_AT=$CLOCK_AT} RX_WHIP_QUANT=60 "$@" nice "$PB" "$G" "$n" "$O/trace-product.txt" > "$O/stdout-product.txt" 2> "$O/stderr-product.txt"
  echo "product: $(tail -1 "$O/stdout-product.txt")"
fi
if [ -n "$op" ]; then while kill -0 $op 2>/dev/null; do sleep 20; done; wait $op || echo "oracle exit $?"; echo "oracle: $(grep -E 'EVENT STARTED|EVENT FAILED|STOPPED' "$O/stdout-oracle.txt" | head -2)"; fi
[ -s "$O/pair-product.txt" ] && [ -s "$O/pair-oracle.txt" ] && python3 "$R/core/tools/gridpair_cmp.py" "$O/pair-product.txt" "$O/pair-oracle.txt" | tee "$O/cmp.txt"
exit 0
