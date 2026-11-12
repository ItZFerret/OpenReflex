#!/bin/sh
# tests/untamed_convert/assembly_parts/run.sh <scratch dir> [names|base|assets|closure|tuff|water|screens|ads|splitgate|dirtlook|all] [--levels norton,foothills ...]
# Builds the "assembly_parts" part (../build.sh) and runs the libraries the track builder calls (UxBase, UxBuildAssets, UxTuff, UxWater,
# UxScreens, UxAds, UxSplitGate, UxDirtLook, the pinned template names of UxStock) against the Python reference, headless.
# Inputs (environment; a missing input makes the result INCOMPLETE):
#   UX_DISC_ISO     the MX vs ATV Untamed disc image          UX_DISC_FOLDER  the extracted disc (the folder holding Database and AI)
#   UX_REFLEX       an original MX vs ATV Reflex install      UX_CLIENT_LIB   the Open Reflex client's "game files" folder
#   UX_PY_ROOT      the reference converter, read only
#   UX_PYTHON       its Python, run with -I -B. ref_parts.py writes the reference
#                   values (what the current Python returns in memory for the 12 levels, about 4.5 minutes, 0.25 GB) into <scratch>/ref when
#                   they are missing; it writes nothing outside <scratch>/ref.
#   UX_REFS=1       write the reference values again
# The work stores are <scratch>/work/<Level>, seeded with the reference's stage outputs (about 1.5 GB for the 12 levels).
# Exit 0: every test ran and passed.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); S=${1:?scratch dir}; shift
WHAT=${1:-all}; [ $# -gt 0 ] && shift
PY=${UX_PYTHON:-}
export UX_PY_ROOT=${UX_PY_ROOT:-}
sh "$HERE/../build.sh" "$S" assembly_parts > /dev/null
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) DN=${RX_DOTNET:-"C:/Program Files/dotnet/dotnet.exe"} ;;
*) DN=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting/DotNetSdk/dotnet ;;
esac
mkdir -p "$S/ref"
if [ -x "$PY" ] || command -v "$PY" > /dev/null 2>&1; then
  if [ -n "${UX_REFS:-}" ] || [ ! -e "$S/ref/done.txt" ]; then
    rm -f "$S/ref/done.txt"
    "$PY" -I -B "$HERE/ref_parts.py" "$S/ref" > "$S/ref/ref_parts.log"
  fi
else
  echo "no reference Python ($PY): using the reference values already in $S/ref"
fi
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$DN" "$S/bin/assembly_parts.dll" "$WHAT" --scratch "$S" --ref "$S/ref" "$@"
