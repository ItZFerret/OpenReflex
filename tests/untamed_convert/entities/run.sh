#!/bin/sh
# tests/untamed_convert/entities/run.sh <scratch dir> [<level>]
# Builds the entities part (../build.sh) and runs the stage for the 12 levels (or one) against the Python reference converter, headless.
# Inputs (environment; without the disc or a library the result is INCOMPLETE):
#   UX_DISC_ISO     the MX vs ATV Untamed disc image          UX_DISC_FOLDER  the extracted disc (the folder holding Database and AI)
#   UX_REFLEX       an original MX vs ATV Reflex install      UX_CLIENT_LIB   the Open Reflex client's "game files" folder
#   UX_PY_ROOT      the reference converter, read only: work/<Level>/ (the upstream stage outputs the
#                   test seeds its own work store with, and entities.json, entities/), build/<name>/assembly/entities.json,
#                   work/combined/commands.json
#   UX_PYTHON       its Python, run with -I -B: ref_entities.py writes the reference
#                   documents of the option sets no stage output covers into <scratch>/ref/variants (nothing outside <scratch>).
#                   Without it those documents must already be there, else their comparisons are skipped (INCOMPLETE).
#   UX_REFS=1       write the reference documents again
# Exit 0: every comparison ran and passed.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); S=${1:?scratch dir}; ONLY=${2:-}
PY=${UX_PYTHON:-}
export UX_PY_ROOT=${UX_PY_ROOT:-}
sh "$HERE/../build.sh" "$S" entities > /dev/null
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) DN=${RX_DOTNET:-"C:/Program Files/dotnet/dotnet.exe"} ;;
*) DN=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting/DotNetSdk/dotnet ;;
esac
mkdir -p "$S/ref"
if [ -x "$PY" ] || command -v "$PY" > /dev/null 2>&1; then
  if [ -n "${UX_REFS:-}" ] || [ ! -e "$S/ref/variants/.done" ]; then
    "$PY" -I -B "$HERE/ref_entities.py" "$S/ref/variants" "$UX_PY_ROOT" > "$S/ref/variants.log" && : > "$S/ref/variants/.done"
  fi
else
  echo "no reference Python ($PY): using the documents already in $S/ref/variants"
fi
if [ -n "$ONLY" ]; then set -- --only "$ONLY"; else set --; fi
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$DN" "$S/bin/entities.dll" --scratch "$S" --ref "$S/ref" "$@"
