#!/bin/sh
# tests/untamed_convert/collision/run.sh <scratch dir> [levels|qhull|synthetic|all] [--levels a,b] [--no-threads]
# Builds the collision part (../build.sh) and runs the S5 collision stage port (UxCollision) for the 12 levels from the disc against the
# Python reference's work/<Level>/collision-t9 and collision (see Program.cs). Nothing is written outside <scratch>.
# Inputs (environment):
#   UX_DISC_ISO     the MX vs ATV Untamed disc image          UX_DISC_FOLDER  the extracted disc (the folder holding Database and AI)
#   UX_PY_ROOT      the reference converter, read only: its work/<Level>/ stage outputs, its
#                   templates/ (the template slots' collis names) and its tools
#   UX_PYTHON       its Python, run with -I -B: ref_collision.py writes
#                   <scratch>/ref/qhull2d.json, synthetic.json and levels/<Level>.<variant>.json (scipy hull orders, helper values,
#                   synthetic sources, the current tool's manifests for other options; it imports the reference tools and writes
#                   nothing to the reference). Without it the files must already be there, or those tests are skipped (INCOMPLETE).
#   UX_REFS=1       write the reference values again
# No Reflex library is needed (the stage reads only the disc and the work store).
# Exit 0: every test ran and passed.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); S=${1:?scratch dir}; shift
PY=${UX_PYTHON:-}
export UX_PY_ROOT=${UX_PY_ROOT:-}
sh "$HERE/../build.sh" "$S" collision > /dev/null
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) DN=${RX_DOTNET:-"C:/Program Files/dotnet/dotnet.exe"} ;;
*) DN=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting/DotNetSdk/dotnet ;;
esac
mkdir -p "$S/ref"
if [ -x "$PY" ] || command -v "$PY" > /dev/null 2>&1; then
  if [ -n "${UX_REFS:-}" ] || [ ! -e "$S/ref/qhull2d.json" ] || [ ! -e "$S/ref/synthetic.json" ] || [ ! -d "$S/ref/levels" ]; then
    "$PY" -I -B "$HERE/ref_collision.py" "$S/ref" "$UX_PY_ROOT" > "$S/ref/collision.log"
  fi
else
  echo "no reference Python ($PY): using the values already in $S/ref"
fi
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$DN" "$S/bin/collision.dll" --scratch "$S" --ref "$S/ref" "$@"
