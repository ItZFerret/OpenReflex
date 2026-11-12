#!/bin/sh
# tests/untamed_import/run.sh <scratch dir> <Untamed disc image> [<extracted Untamed disc folder>] [<another game's disc image>]
# Builds the MX vs ATV Untamed disc check (build.sh) and runs it outside Unity:
#   the disc image                 12 stadiums ready
#   the extracted folder           12 ready;  both together: 12 ready (each stadium once), 2 sources
#   another game's disc image      0 ready and the "not MX vs ATV Untamed (title ...)" warning (e.g. the Alive disc image)
#   a file that is no disc image   0 ready and the "Not an Xbox 360 game disc image" warning
#   a scratch Mods/Tracks          Untamed/norton-v2 and norton-v3: Norton reported as installed in Untamed/norton-v3
#   the table                      UntamedImporter.KnownTracks holds every row tools/formats/untamed_disc.py --csharp prints for the image
#                                  (python3, or PYTHON=...; skipped without one)
# The sources are only read (Program.cs checks every file's size and time). Exit 0: UNTAMED IMPORT TESTS PASS.
# This is the scan only (UntamedImporter.cs alone). The import itself (UntamedImporter.Import, with the converter): import/run.sh.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../.." && pwd)
S=${1:?scratch dir}; ISO=${2:?Untamed disc image}; DIR=${3:-}; OTHER=${4:-}
sh "$HERE/build.sh" "$S" > /dev/null
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) DN=${RX_DOTNET:-"C:/Program Files/dotnet/dotnet.exe"} ;;
*) DN=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting/DotNetSdk/dotnet ;;
esac
FAILS=0
check()   # check <name> <pattern the output must hold> <program arguments...>
{
  name=$1; want=$2; shift 2
  if out=$(DOTNET_CLI_TELEMETRY_OPTOUT=1 "$DN" "$S/bin/UntamedImport.dll" "$@" 2>&1) && printf '%s\n' "$out" | grep -q -- "$want"; then echo "ok    $name"
  else echo "FAIL  $name"; printf '%s\n' "$out" | sed 's/^/      /'; FAILS=$((FAILS + 1)); fi
}
check "disc image" "12 of 12 stadiums ready" --expect 12 "$ISO"
if [ -n "$DIR" ]; then
  check "extracted folder" "12 of 12 stadiums ready" --expect 12 "$DIR"
  check "image and folder" "12 of 12 stadiums ready, 2 sources" --expect 12 "$ISO" "$DIR"
fi
[ -n "$OTHER" ] && check "another game's disc" "not MX vs ATV Untamed (title" --expect 0 "$OTHER"
printf 'not a disc image\n' > "$S/not-a-disc.iso"
check "not a disc image" "Not an Xbox 360 game disc image" --expect 0 "$S/not-a-disc.iso"
rm -rf "$S/tracks"; mkdir -p "$S/tracks/Untamed/norton-v2" "$S/tracks/Untamed/norton-v3"
: > "$S/tracks/Untamed/norton-v2/Beta_Sx_Track_Slot_1.dx9.level"; : > "$S/tracks/Untamed/norton-v3/Beta_Sx_Track_Slot_1.dx9.level"
check "installed track" "installed: Untamed/norton-v3" --tracks "$S/tracks" --expect 12 "$ISO"
PY=${PYTHON:-python3}
if command -v "$PY" > /dev/null 2>&1 && "$PY" -c "" > /dev/null 2>&1; then
  missing=0
  "$PY" -I "$ROOT/tools/formats/untamed_disc.py" --csharp "$ISO" > "$S/rows.raw" || { echo "      untamed_disc.py failed (exit $?)"; missing=$((missing + 1)); }
  tr -d '\r' < "$S/rows.raw" | grep -v '^//' > "$S/rows.txt" || :
  while IFS= read -r row; do
    tr -d '\r' < "$ROOT/Unity/Reflex/Assets/Scripts/Content/UntamedImport/UntamedImporter.cs" | grep -qF -- "$row" || { echo "      not in KnownTracks: $row"; missing=$((missing + 1)); }
  done < "$S/rows.txt"
  if [ "$missing" -eq 0 ] && [ -s "$S/rows.txt" ]; then echo "ok    table = untamed_disc.py"; else echo "FAIL  table = untamed_disc.py"; FAILS=$((FAILS + 1)); fi
else echo "skip  table = untamed_disc.py (no $PY)"; fi
if [ "$FAILS" -eq 0 ]; then echo "UNTAMED IMPORT TESTS PASS"; else echo "UNTAMED IMPORT TESTS FAIL ($FAILS)"; exit 1; fi
