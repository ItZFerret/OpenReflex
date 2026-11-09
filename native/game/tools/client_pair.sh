#!/bin/bash
# client_pair.sh <reflex_client> <out dir> profile <file> | world <prefix> <file> | build "<line>" [filter (all)] -- the plugin driven as the Unity client drives it (reflex_client,
# client_main.cpp), twice in parallel: the original code ("none") and with the ports ("port"), each on its own copy of a save
# ($CLIENT_SAVE, default the scratchpad's boot/client_save0: a Unity save folder with remote/savegame.bxml, stats/, steam_persona.txt).
#   kind profile <commands file>   a profile session (reflex_profile_open, the file's "profile ..." lines; ports for the session's boot)
#   kind world <world prefix> <commands file>   a race world (reflex_world_create, world_port <filter> on the "port" side, the file's lines)
#   kind build "<build line>"      a world build: "build <spec9> [riders ai]", "testdrive [owner id]", "series <spec9> <series> [vtype]"
# Compared: every answer (anscmp.py: the run folders' paths normalised), the save files, world images (build), canonical dumps (profile:
# the session's heap at the end). COV=1: REFLEX_CLIENT_COVERAGE=<out>/<side>/cov (boot.h; then tools/gap_functions.py on the .bin files).
# Command files: tools/client/*.txt (the client's own sequences: notes/loading.md section 9).
C=${1:?reflex_client}; O=${2:?out}; K=${3:?kind}; A=${4:?arg}; F=$5; P=${6:-all}
[ "$K" == world ] || P=${5:-all}
S=${REFLEX_TESTDATA:-$(cd "$(dirname "$0")/../../.." && pwd)/testdata}   # test inputs: boot/worlds, crt, events (see testdata/README.md)
G="/Users/payton/Documents/Development/REFLEX/game files/Archive"
H=$(cd "$(dirname "$0")" && pwd)
SAVE=${CLIENT_SAVE:-$S/boot/client_save0}
one() { local x=$1 mode=$2; rm -rf "${O:?}/$x"; mkdir -p $O/$x/ans; cp -R "$SAVE" $O/$x/save
  case $K in
    profile) { echo "boot_ports $P $mode"; cat "$A"; echo "profile canonical $O/$x/canon.bin"; echo "profile_close"; } > $O/$x/script.txt ;;
    world) { echo "world_open $A"; [ $mode == 1 ] && echo "world_port $P 1"; grep -v '^#' "$F"; echo "port_report"; echo "world_close"; } > $O/$x/script.txt ;;
    build) set -- $A; v=$1; shift; printf 'boot_ports %s %s\n%s %s %s\n' $P $mode $v $O/$x/c "$*" > $O/$x/script.txt ;;
  esac
  env REFLEX_JIT=1 REFLEX_CLIENT_ANSWERS=$O/$x/ans ${COV:+REFLEX_CLIENT_COVERAGE=$O/$x/cov} perl -e 'alarm 5000; exec @ARGV' "$C" "$G" $S/crt $O/$x/save $O/$x/script.txt > $O/$x/out.txt 2>&1
  echo "$x: exit $?, $(grep -c '^!' $O/$x/out.txt) errors"; }
one none 0 & one port 1 & wait
grep -h "^!" $O/none/out.txt $O/port/out.txt | sort | uniq -c | head -8
python3 "$H/anscmp.py" $O/none $O/port
(cd $O/none/save && find . -type f ! -name '*.log' | sort | xargs shasum) > $O/none/files.txt; (cd $O/port/save && find . -type f ! -name '*.log' | sort | xargs shasum) > $O/port/files.txt
diff -q $O/none/files.txt $O/port/files.txt > /dev/null && echo "save files: identical ($(wc -l < $O/none/files.txt | tr -d ' ') files)" || { echo "save files: DIFFERENT"; diff $O/none/files.txt $O/port/files.txt; }
if [ $K == profile ] && [ -f $O/none/canon.bin ] && [ -f $O/port/canon.bin ]; then python3 "$H/world_compare.py" $O/none/canon.bin $O/port/canon.bin --examples 2 | grep -E "VERDICT|named nodes:|  node |^\s+\+" | head -12; fi
if [ $K == build ]; then cmp -s $O/none/c-world.bin $O/port/c-world.bin && echo "world images: identical" || python3 "$H/world_compare.py" --images $O/none/c-world.bin $O/port/c-world.bin; fi
