#!/bin/sh
# The 12-rider engine field, before / after (w-aiengine). Needs a built core (core/build.sh) and `clang`; the original-engine events come from the
# audio export (_re/formats/out/audio, REFLEX_AUDIO_EXPORT) and the install ("game files/Archive"). Everything goes to <scratch>; the review WAVs to <wav dir>.
#   sh field_batch.sh <scratch dir> <wav dir>
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../../.." && pwd); S=${1:?scratch}; W=${2:?wav dir}
U=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting
mkdir -p "$S" "$W"
clang -std=c11 -O1 -I "$ROOT/core/include" "$HERE/aitrace.c" -o "$S/aitrace"
sh "$HERE/engine_logic/build.sh" "$S/el" > /dev/null
python3 "$HERE/engine_events.py" "$S"
cd "$ROOT"
for pad in hold lap:5; do
  t=$(echo $pad | tr -d ':')
  [ -f "$S/trace_$t.csv" ] || "$S/aitrace" core/build/librxcore.dylib "game files/Archive" 1800 $pad > "$S/trace_$t.csv" 2> "$S/trace_$t.err"
  FIELD_BUDGET=0 "$U/NetCoreRuntime/dotnet" exec "$S/el/engine_logic.dll" "$S/trace_$t.csv" "$S/events.csv" "$S/states.csv" "game files/Archive" > "$S/cmds_${t}_before.tsv"
  "$U/NetCoreRuntime/dotnet" exec "$S/el/engine_logic.dll" "$S/trace_$t.csv" "$S/events.csv" "$S/states.csv" "game files/Archive" > "$S/cmds_${t}_after.tsv"
done
cd "$HERE"
for tag in before after; do
  /usr/bin/python3 field_run.py "$S/trace_hold.csv" "$S/cmds_hold_$tag.tsv" "$W" field__original-engine --tag $tag --wav
  for pk in "HRC Honda 250 (Loud)" "HRC Honda 450"; do
    n=$(echo "$pk" | tr -d '()' | tr ' ' '_')
    /usr/bin/python3 field_run.py "$S/trace_hold.csv" "$S/cmds_hold_$tag.tsv" "$W" "field__$n" --tag $tag --pack "$pk" "$ROOT/Unity/Reflex/Assets/StreamingAssets/EngineSounds" --wav
  done
  /usr/bin/python3 field_run.py "$S/trace_lap5.csv" "$S/cmds_lap5_$tag.tsv" "$W" field__original-engine-lap5 --tag $tag
done
