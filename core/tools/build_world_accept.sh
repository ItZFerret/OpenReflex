#!/bin/bash
# build_world_accept.sh <out dir> <orig dir> <level> <orig physworld> <orig terrain entity> [collision subscenes] [asset script]
# Acceptance of construction units 3 / 5 (core/notes/construction.md section 10) on one event: builds the physics world, the terrain,
# the collision entities and the grids' finalize with tools/build_world_test (twice: RX_BUILD_FILL 0 / 0xa5), then compares with the
# original's canonical dump + image of the same event (<orig dir>/c-canon.bin, c-world.bin, c-deform.bin: a baton boot with
# REFLEX_WORLD_OUT=<orig dir>/c REFLEX_WORLD_CANON=, section 1):
#  1. rx_canon_cmp.py from the physics world, the terrain entity and every collision entity (paired through the static objects);
#  2. the static objects aligned (positions, quaternions, property ids, shapes compared deeply, the broadphase boxes and cells);
#  3. the deformation surface's bits against c-deform.bin;
#  4. the PhysicsProperties registry (G: anchors on its globals).
# Example (reference event): build_world_accept.sh $S/rx_build2/acc $S/rx_build/orig NAT_Concord 162a2fd0 164702a0 "" core/tools/build_script_concord.txt
set -e
C=$(cd "$(dirname "$0")/.." && pwd); O=${1:?out}; R=${2:?orig dir}; L=${3:?level}; PW=${4:?orig physworld}; TE=${5:?orig terrain}
SUBS=${6:-}; SCRIPT=${7:-}
A="$C/../game files/Archive"
mkdir -p "$O"
"$C/tools/build_world_test.sh" "$O/bin" > /dev/null
printf 'physworld %s\nterrain %s\n' "$PW" "$TE" > "$O/orig.txt"
U="physworld,terrain,collision${SUBS:+:$SUBS},finalize"
"$O/bin/build_world_test" "$A" "$O/w" "$L" "$U" "$O/orig.txt" $SCRIPT | tee "$O/build.txt" | grep -v '^heap'
RX_BUILD_FILL=0xa5 "$O/bin/build_world_test" "$A" "$O/wF" "$L" "$U" "$O/orig.txt" $SCRIPT > /dev/null
W=$(grep '^physics world' "$O/build.txt" | awk '{print $3}'); T=$(grep '^terrain entity' "$O/build.txt" | awk '{print $3}')
python3 "$C/tools/build_world_cmp.py" statics "$R/c-world.bin" "$PW" "$O/w-world.bin" "$W" "$O/entities.txt"
python3 "$C/tools/build_world_cmp.py" deform "$O/w-world.bin" "$T" "$R/c-deform.bin"
cat "$O/w-anchors.txt" "$O/entities.txt" > "$O/anchors.txt"
python3 "$C/tools/rx_canon_cmp.py" --orig "$R/c-canon.bin" "$R/c-world.bin" --core "$O/w-canon.bin" "$O/w-world.bin" --undef "$O/wF-world.bin" \
    --show 40 @"$O/anchors.txt" > "$O/cmp.txt" || true
head -1 "$O/cmp.txt"; sed -n '/differing words by/,$p' "$O/cmp.txt" | head -60
python3 "$C/tools/rx_canon_cmp.py" --orig "$R/c-canon.bin" "$R/c-world.bin" --core "$O/w-canon.bin" "$O/w-world.bin" --undef "$O/wF-world.bin" \
    G:00da6adc G:00da6af4 G:00da6b18 G:00da6ad0 G:00da6ae8 G:00da6b00 G:00da6b20 G:00da6b38 | sed 's/^/physics properties: /' | head -3
