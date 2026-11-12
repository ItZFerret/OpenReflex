#!/bin/sh
# tests/untamed_convert/meshes/run.sh <scratch dir> [--levels a,b] [--no-threads]
# Builds the meshes part (../build.sh) and runs the S3 mesh stage port for the 12 levels from the disc against the Python reference's
# work/<Level>/meshes (see Program.cs). Nothing is written outside <scratch>.
# Inputs (environment):
#   UX_DISC_ISO     the MX vs ATV Untamed disc image          UX_DISC_FOLDER  the extracted disc (the folder holding Database and AI)
#   UX_PY_ROOT      the reference converter, read only
#   UX_PYTHON       its Python, run with -I -B: writes the digests of the
#                   ConvertModel variants and of the damaged-file checks (ref_meshes.py -> <scratch>/ref/models.json, neg.json, neg/;
#                   nothing is written to the reference).
#                   Without it the digests must already be there, or that test is skipped (result INCOMPLETE).
#   UX_REFS=1       write the reference digests again
# Exit 0: every test ran and passed.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); S=${1:?scratch dir}; shift
PY=${UX_PYTHON:-}
export UX_PY_ROOT=${UX_PY_ROOT:-}
sh "$HERE/../build.sh" "$S" meshes > /dev/null
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) DN=${RX_DOTNET:-"C:/Program Files/dotnet/dotnet.exe"} ;;
*) DN=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting/DotNetSdk/dotnet ;;
esac
mkdir -p "$S/ref"
if [ -x "$PY" ] || command -v "$PY" > /dev/null 2>&1; then
  if [ -n "${UX_REFS:-}" ] || [ ! -e "$S/ref/models.json" ] || [ ! -e "$S/ref/neg.json" ]; then "$PY" -I -B "$HERE/ref_meshes.py" "$S/ref/models.json" "$UX_PY_ROOT" > "$S/ref/models.log"; fi
else
  echo "no reference Python ($PY): using the digests already in $S/ref"
fi
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$DN" "$S/bin/meshes.dll" --scratch "$S" --ref "$S/ref" "$@"
