#!/bin/bash
# jobthreads_check.sh <out dir> <install root> [race frames (4200)] [modes (1 2 4 8)] -- the w-jobthreads bit-exactness check: builds
# core/tools/jobthreads_hash (the product objects of core/build.sh) and runs a 12-rider National, a Supercross, a four-wheel (UTV)
# Powercross, an ATV National and a super-buggy Powercross and two fuzzed-pad races (National, UTV Powercross: crashes and resets) (JT_EVENTS: a subset of "nat sx px atv pxb fzn fzp") once per RX_JOB_THREADS mode, the whole arena hashed every 600 race frames. Passes when every mode's hash lines equal mode
# 1's (the worker jobs one after another on the game thread) for every event. Then mode 0 (the in-place workers of before) against the
# largest mode: the per-frame rider / bike poses and the game LCG (JT_POSE) must be the same in every frame. Its arena is not compared
# (the in-place workers leave their frames in the game thread's dead stack, which the particle templates and the job records copy:
# core/plan/QUEUE.md w-jobthreads).
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out dir}; ROOT=${2:?install root}; FR=${3:-4200}; MODES=${4:-1 2 4 8}
mkdir -p "$O"; source "$C/tools/product_link.sh"
rx_product_objs
rx_link_driver "$O/jobthreads_hash" "$C"/tools/jobthreads_hash.c
ev() { case $1 in nat) echo "National 10 NAT_Concord 50 0 2 3 Laps 12 95";; sx) echo "Supercross 8 SX_Kingston 74 0 2 3 Laps 12 95";;
                  px) echo "Powercross 16 PX_Laguna 58 0 22 3 Laps 12 95";; atv) echo "National 10 NAT_Bedford 48 0 1 3 Laps 12 95";;
                  pxb) echo "Powercross 16 PX_Laguna 58 0 19 3 Laps 12 95";; fzn) echo "National 10 NAT_Concord 50 0 2 3 Laps 12 95";;
                  fzp) echo "Powercross 16 PX_Laguna 58 0 22 3 Laps 12 95";; esac; }
key() { case $1 in fzn) echo fuzz:7;; fzp) echo fuzz:11;; *) echo 17;; esac; }   # fzn / fzp: stop_audit_run's fuzzed pad (crashes, resets)
fail=0
for e in ${JT_EVENTS:-nat sx px atv pxb fzn fzp}; do
  last=1
  for m in $MODES; do   # the runs of an event side by side (they do not share anything)
    (RX_JOB_THREADS=$m JT_HASH=600 "$O/jobthreads_hash" "$ROOT" $(ev $e) - 3000 "$FR" $(key $e) 2>/dev/null | grep '^HASH' > "$O/$e.m$m.hash" || true) &
    last=$m
  done
  (RX_JOB_THREADS=0 JT_POSE=1 "$O/jobthreads_hash" "$ROOT" $(ev $e) - 3000 "$FR" $(key $e) 2>/dev/null | grep '^G ' > "$O/$e.p0" || true) &
  (RX_JOB_THREADS=$last JT_POSE=1 "$O/jobthreads_hash" "$ROOT" $(ev $e) - 3000 "$FR" $(key $e) 2>/dev/null | grep '^G ' > "$O/$e.p$last" || true) &
  wait
  [ -s "$O/$e.m1.hash" ] || { echo "$e: no hash lines (mode 1)"; fail=1; continue; }
  for m in $MODES; do
    if cmp -s "$O/$e.m1.hash" "$O/$e.m$m.hash"; then echo "$e: mode $m equals mode 1 ($(wc -l < "$O/$e.m1.hash" | tr -d ' ') hashes)"
    else echo "$e: mode $m DIFFERS from mode 1"; fail=1; fi
  done
  if [ -s "$O/$e.p0" ] && cmp -s "$O/$e.p0" "$O/$e.p$last"; then echo "$e: poses / LCG of mode 0 and mode $last equal in $(wc -l < "$O/$e.p0" | tr -d ' ') frames"
  else echo "$e: poses / LCG of mode 0 and mode $last DIFFER"; fail=1; fi
done
exit $fail
