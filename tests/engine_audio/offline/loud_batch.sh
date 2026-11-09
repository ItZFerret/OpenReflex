#!/bin/sh
# w-packloud review: every pack as the local engine, before (w-enginepacks MixGain 0.30 x pack.ini gain, LandingPackTrim 0.35) and after
# (the measured loudness + peak control, LandingPackTrim 1), over the 12-rider field and the landing clanks (landing_mix.py), and the original
# engine for reference. Inputs: w-landclank's trace_swap.csv / wheels_hold.csv / cmds_swap.tsv (rider 2 as the local rider, a 450F).
#   sh loud_batch.sh <landclank scratch> <wav dir> <EngineSounds dir> [<extra pack dir>]
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); S=${1:?scratch}; W=${2:?wav dir}; D=${3:?EngineSounds}; X=${4:-}
cd "$HERE"; export PYTHONDONTWRITEBYTECODE=1
run() { /usr/bin/python3 -E landing_mix.py "$S/trace_swap.csv" "$S/wheels_hold.csv" "$S/cmds_swap.tsv" "$W" --start 2628 --local 2 --logic after "$@"; }
run --wav loud__original-engine.wav
for dir in "$D" $X; do
  ls "$dir" | while read -r pk; do
    [ -f "$dir/$pk/engine.scl" ] || continue
    n=$(echo "$pk" | tr -d '()' | tr ' ' '_')
    run --pack "$pk" "$dir" --pack-logic after --pack-trim 0.35 --wav "loud__${n}__before.wav"
    run --pack "$pk" "$dir" --pack-logic loud --pack-trim 1.0 --wav "loud__${n}__after.wav"
  done
done
