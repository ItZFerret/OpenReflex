#!/bin/sh
# tests/untamed_convert/level/run.sh <scratch dir> [level ...]
# Builds the level part (../build.sh: the foundation + UxLevel.cs) and runs the level stage (UxLevel = ux_level.py) for the 12 levels
# (or the ones named) against the Python reference converter, headless.
# Inputs (environment):
#   UX_DISC_FOLDER  the extracted MX vs ATV Untamed disc (the folder holding Database and AI)   and / or
#   UX_DISC_ISO     the disc image. With both, the folder is the source and two levels are built again from the image and compared
#                   (UX_DISC_ALSO names another second source).
#   UX_PY_ROOT      the reference converter, read only
#   UX_PYTHON       its Python, run with -I -B. It writes the references into
#                   <scratch>/pyref and nowhere else: ref_level.py (what the current ux_level.py writes: level.json, level_fr.json),
#                   ref_numerics.py (unrounded numbers and number-rule vectors). They are written again when a reference tool is newer
#                   than them or with UX_REFS=1; without Python the references already in <scratch>/pyref are used.
# Compared per level: level.json and level_fr.json with the current ux_level.py output and with the stored work/<Level>/ files, as
# documents and byte for byte (Program.cs names the environment fields that cannot be equal); the unrounded numerics.
# Exit 0: everything ran and passed (RESULT level: PASS). 3: something was skipped (an input is missing).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); S=${1:?scratch dir}; shift
PY=${UX_PYTHON:-}
export UX_PY_ROOT=${UX_PY_ROOT:-}
sh "$HERE/../build.sh" "$S" level > /dev/null
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) DN=${RX_DOTNET:-"C:/Program Files/dotnet/dotnet.exe"} ;;
*) DN=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting/DotNetSdk/dotnet ;;
esac
REF="$S/pyref"; mkdir -p "$REF"
if [ -x "$PY" ] || command -v "$PY" > /dev/null 2>&1; then
  stale()   # stale <reference file>: missing, UX_REFS set, or older than a tool the reference comes from
  {
    [ -n "${UX_REFS:-}" ] || [ ! -e "$1" ] && return 0
    for t in ux_level.py ux_common.py uxscene.py uxmesh.py uxmat.py uxtex.py uxterrain.py uxpkg.py; do
      [ "$UX_PY_ROOT/tools/$t" -nt "$1" ] && return 0
    done
    return 1
  }
  if stale "$REF/Lincoln_STADIUM/level_fr.json" || stale "$REF/rigid-drop/Lincoln_STADIUM/level.json"; then
    echo "writing the reference: ux_level.py on the 12 levels (several minutes)"
    "$PY" -I -B "$HERE/ref_level.py" "$REF" > "$REF/level.log"
  fi
  if stale "$REF/numerics/Lincoln_STADIUM.json" || stale "$REF/numerics/fixed.json"; then
    echo "writing the reference numerics (several minutes)"
    UX_REFS=1 "$PY" -I -B "$HERE/ref_numerics.py" "$REF" > "$REF/numerics.log"
  fi
else
  echo "no reference Python ($PY): using the references already in $REF"
fi
ALSO=${UX_DISC_ALSO:-}
if [ -z "$ALSO" ] && [ -n "${UX_DISC_FOLDER:-}" ] && [ -n "${UX_DISC_ISO:-}" ]; then ALSO=$UX_DISC_ISO; fi
if [ -n "$ALSO" ]; then set -- "$@" --also "$ALSO"; fi
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$DN" "$S/bin/level.dll" "$@" --scratch "$S" --ref "$REF"
