#!/bin/sh
# tests/untamed_convert/textures/run.sh <scratch dir> [stage|threads|texmax|generated|rgba|all]
# Builds the "textures" part (../build.sh) and runs the stage test against the Python reference converter (UX_PY_ROOT), headless.
# Inputs (environment; a test whose input is missing is reported as skipped and the result is INCOMPLETE):
#   UX_DISC_ISO     the MX vs ATV Untamed disc image          UX_DISC_FOLDER  the extracted disc (the folder holding Database and AI)
#   UX_REFLEX       an original MX vs ATV Reflex install      UX_CLIENT_LIB   the Open Reflex client's "game files" folder
#   UX_PY_ROOT      the reference converter, read only: work/<Level>/level.json seeds the work store,
#                   work/<Level>/textures/ is what the stage is compared with
#   UX_PYTHON       its Python, run with -I -B: ref_textures.py writes the digests of
#                   the parts the stage folders do not cover (--tex-max, generated textures, the PC decoder) into <scratch>/ref with the
#                   current Python code (nothing is written outside <scratch>). Without it the digests must already be there.
#   UX_REFS=1       write the reference digests again
# Outputs go to <scratch>/work/<Level>/textures (and work-*/ for the variants). Exit 0: every test ran and passed.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); S=${1:?scratch dir}; WHAT=${2:-all}
PY=${UX_PYTHON:-}
export UX_PY_ROOT=${UX_PY_ROOT:-}
sh "$HERE/../build.sh" "$S" textures > /dev/null
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) DN=${RX_DOTNET:-"C:/Program Files/dotnet/dotnet.exe"} ;;
*) DN=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting/DotNetSdk/dotnet ;;
esac
mkdir -p "$S/ref"
if [ -x "$PY" ] || command -v "$PY" > /dev/null 2>&1; then
  if [ -n "${UX_REFS:-}" ] || [ ! -e "$S/ref/grey.json" ]; then "$PY" -I -B "$HERE/ref_textures.py" "$S/ref" > "$S/ref/textures.log"; fi
else
  echo "no reference Python ($PY): using the digests already in $S/ref"
fi
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$DN" "$S/bin/textures.dll" "$WHAT" --scratch "$S" --ref "$S/ref"
