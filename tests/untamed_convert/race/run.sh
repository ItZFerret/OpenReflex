#!/bin/sh
# tests/untamed_convert/race/run.sh <scratch dir> [numbers|levels|variants|lappacing|startpack|all] [--levels norton,foothills ...]
# Builds the "race" part (../build.sh) and runs the S8 stage (UxRace, UxTdf07, UxLapPacing, UxStartPack) against the Python reference,
# headless. Inputs (environment; a test whose input is missing is reported as skipped and the result is INCOMPLETE):
#   UX_DISC_ISO     the MX vs ATV Untamed disc image          UX_DISC_FOLDER  the extracted disc (the folder holding Database and AI)
#   UX_REFLEX       an original MX vs ATV Reflex install      UX_CLIENT_LIB   the Open Reflex client's "game files" folder
#   UX_PY_ROOT      the reference converter, read only
#   UX_PYTHON       its Python, run with -I -B. It writes the reference outputs into
#                   <scratch>/ref when they are missing (ref_num.py: the number rules; ref_race.py: the stage as the current builds run
#                   it, the option variants, lap pacing, the start-pack report: about 5 minutes). Nothing is written outside <scratch>.
#   UX_REFS=1       write the reference outputs again
# Exit 0: every test ran and passed.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); S=${1:?scratch dir}; shift
WHAT=${1:-all}; [ $# -gt 0 ] && shift
PY=${UX_PYTHON:-}
export UX_PY_ROOT=${UX_PY_ROOT:-}
sh "$HERE/../build.sh" "$S" race > /dev/null
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) DN=${RX_DOTNET:-"C:/Program Files/dotnet/dotnet.exe"} ;;
*) DN=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting/DotNetSdk/dotnet ;;
esac
mkdir -p "$S/ref"
if [ -x "$PY" ] || command -v "$PY" > /dev/null 2>&1; then
  ref() { [ -n "${UX_REFS:-}" ] || [ ! -e "$1" ]; }
  if ref "$S/ref/num.json"; then "$PY" -I -B "$HERE/ref_num.py" "$S/ref/num.json" > "$S/ref/ref_num.log"; fi
  if ref "$S/ref/work/Lincoln_STADIUM/race.json" || ref "$S/ref/variants.json" || ref "$S/ref/startpack/Lincoln_STADIUM.json"; then
    "$PY" -I -B "$HERE/ref_race.py" "$S/ref" > "$S/ref/ref_race.log"
  fi
else
  echo "no reference Python ($PY): using the reference outputs already in $S/ref"
fi
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$DN" "$S/bin/race.dll" "$WHAT" --scratch "$S" --ref "$S/ref" "$@"
