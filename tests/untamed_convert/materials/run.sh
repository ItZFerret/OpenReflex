#!/bin/sh
# tests/untamed_convert/materials/run.sh <scratch dir> [pins|fmt|shaders|materials|fresh|variants|parallel|all] [program arguments, e.g. --levels norton,fairview]
# Builds the materials part (../build.sh) and runs stage S4 (UxMaterials, UxShaderProg) against the Python reference converter, headless.
# Inputs (environment; a test whose input is missing is reported as skipped and the result is INCOMPLETE):
#   UX_DISC_ISO     the MX vs ATV Untamed disc image          UX_DISC_FOLDER  the extracted disc (the folder holding Database and AI)
#   UX_REFLEX       an original MX vs ATV Reflex install      UX_CLIENT_LIB   the Open Reflex client's "game files" folder
#   UX_PY_ROOT      the reference converter, read only
#   UX_PYTHON       its Python, run with -I -B: ref_fmt.py writes Python's number
#                   formatting of 20,050 values into <scratch>/ref/fmt.json, ref_shaderprog.py the digest of
#                   ux_shaderprog.py over every shader into <scratch>/ref/shaderprog, ref_materials.py a fresh in-memory run of
#                   ux_materials.py into <scratch>/ref/materials-fresh, and with --variants its 16 selftest switch sets on
#                   UX_VARIANT_LEVELS (default Norton_STADIUM Fairview_STADIUM) into <scratch>/ref/materials-variants (nothing is
#                   written outside <scratch>). Without it the references must already be there.   UX_REFS=1  write them again
# When both UX_REFLEX and UX_CLIENT_LIB are set the stage is also run from the client's game files (Norton and Fairview).
# Exit 0: every test ran and passed.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); S=${1:?scratch dir}; WHAT=${2:-all}
[ $# -ge 2 ] && shift 2 || shift 1
PY=${UX_PYTHON:-}
export UX_PY_ROOT=${UX_PY_ROOT:-}
sh "$HERE/../build.sh" "$S" materials > /dev/null
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) DN=${RX_DOTNET:-"C:/Program Files/dotnet/dotnet.exe"} ;;
*) DN=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting/DotNetSdk/dotnet ;;
esac
mkdir -p "$S/ref"
if [ -x "$PY" ] || command -v "$PY" > /dev/null 2>&1; then
  ref() { [ -n "${UX_REFS:-}" ] || [ ! -e "$1" ]; }
  if [ "$WHAT" = all ] || [ "$WHAT" = fmt ]; then
    if ref "$S/ref/fmt.json"; then "$PY" -I -B "$HERE/ref_fmt.py" "$S/ref" > "$S/ref/fmt.log"; fi
  fi
  if [ "$WHAT" = all ] || [ "$WHAT" = shaders ]; then
    if ref "$S/ref/shaderprog/Lincoln_STADIUM.json"; then "$PY" -I -B "$HERE/ref_shaderprog.py" "$S/ref" "$UX_PY_ROOT" > "$S/ref/shaderprog.log"; fi
  fi
  if [ "$WHAT" = all ] || [ "$WHAT" = fresh ]; then
    if ref "$S/ref/materials-fresh/Lincoln_STADIUM/materials.json"; then "$PY" -I -B "$HERE/ref_materials.py" "$S/ref" "$UX_PY_ROOT" > "$S/ref/materials-fresh.log"; fi
  fi
  if [ "$WHAT" = all ] || [ "$WHAT" = variants ]; then    # the 16 switch sets of the reference's selftest on UX_VARIANT_LEVELS
    if ref "$S/ref/materials-variants/safe/Fairview_STADIUM/materials.json"; then
      "$PY" -I -B "$HERE/ref_materials.py" "$S/ref" "$UX_PY_ROOT" --variants ${UX_VARIANT_LEVELS:-Norton_STADIUM Fairview_STADIUM} > "$S/ref/materials-variants.log"
    fi
  fi
else
  echo "no reference Python ($PY): using the references already in $S/ref"
fi
RC=0
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$DN" "$S/bin/materials.dll" "$WHAT" --scratch "$S" --ref "$S/ref" "$@" || RC=$?
if [ "$WHAT" = all ] && [ -n "${UX_REFLEX:-}" ] && [ -n "${UX_CLIENT_LIB:-}" ]; then
  echo "== the same stage from the Open Reflex client's game files"
  DOTNET_CLI_TELEMETRY_OPTOUT=1 "$DN" "$S/bin/materials.dll" materials --scratch "$S/client" --reflex none --levels norton,fairview || RC=$?
fi
exit $RC
