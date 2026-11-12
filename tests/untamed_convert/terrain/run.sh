#!/bin/sh
# tests/untamed_convert/terrain/run.sh <scratch dir> [terrain.dll arguments, e.g. --levels norton,fairview --jobs 4 --strict]
# Builds the terrain part (../build.sh) and runs the terrain stage for the 12 levels against the Python reference (UX_PY_ROOT,
# untamed, read only): work/<Level>/level.json is the input, work/<Level>/terrain/ the expected output. Inputs (environment):
#   UX_DISC_ISO / UX_DISC_FOLDER   the MX vs ATV Untamed disc image / the extracted disc (the folder holding Database and AI)
#   UX_PY_ROOT                     the reference converter
#   UX_PYTHON                      its Python (run with -I -B): writes the reference
#                                  PNG files of the PNG writer test into <scratch>/ref/png (ref_png.py); without it that test uses the
#                                  files already there, or is skipped
#   UX_REFS=1                      write the reference PNG files again
# Nothing is written outside <scratch>. Exit 0: every comparison passed (--strict: the documented deviations fail too).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); S=${1:?scratch dir}; shift
PY=${UX_PYTHON:-}
export UX_PY_ROOT=${UX_PY_ROOT:-}
sh "$HERE/../build.sh" "$S" terrain > /dev/null
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) DN=${RX_DOTNET:-"C:/Program Files/dotnet/dotnet.exe"} ;;
*) DN=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting/DotNetSdk/dotnet ;;
esac
mkdir -p "$S/ref"
if [ -x "$PY" ] || command -v "$PY" > /dev/null 2>&1; then
  if [ -n "${UX_REFS:-}" ] || [ ! -d "$S/ref/png" ]; then "$PY" -I -B "$HERE/ref_png.py" "$S/ref" > "$S/ref/png.log"; fi
fi
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$DN" "$S/bin/terrain.dll" --scratch "$S" --ref "$S/ref" "$@"
