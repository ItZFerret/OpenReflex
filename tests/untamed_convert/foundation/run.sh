#!/bin/sh
# tests/untamed_convert/foundation/run.sh <scratch dir> [s0|json|zlib|stock|readers|all]
# Builds the foundation part (../build.sh) and runs its tests against the Python reference converter (UX_PY_ROOT), headless.
# Inputs (environment; a test whose input is missing is reported as skipped and the result is INCOMPLETE):
#   UX_DISC_ISO     the MX vs ATV Untamed disc image          UX_DISC_FOLDER  the extracted disc (the folder holding Database and AI)
#   UX_REFLEX       an original MX vs ATV Reflex install      UX_CLIENT_LIB   the Open Reflex client's "game files" folder
#   UX_PY_ROOT      the reference converter, read only
#   UX_PYTHON       its Python; run with -I -B. Without it the reference digests must
#                   already be in <scratch>/ref (ref_zlib.py, ref_fortdodge.py, ref_readers.py, ref_pil.py write them; nothing is
#                   written outside <scratch>).
#   UX_REFS=1       write the reference digests again
# Exit 0: every test ran and passed.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); S=${1:?scratch dir}; WHAT=${2:-all}
PY=${UX_PYTHON:-}
export UX_PY_ROOT=${UX_PY_ROOT:-}
sh "$HERE/../build.sh" "$S" foundation > /dev/null
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*) DN=${RX_DOTNET:-"C:/Program Files/dotnet/dotnet.exe"} ;;
*) DN=/Applications/Unity/Hub/Editor/6000.6.1f1/Unity.app/Contents/Resources/Scripting/DotNetSdk/dotnet ;;
esac
mkdir -p "$S/ref"
if [ -x "$PY" ] || command -v "$PY" > /dev/null 2>&1; then
  ref() { [ -n "${UX_REFS:-}" ] || [ ! -e "$1" ]; }
  if ref "$S/ref/zlib.json" && [ -n "${UX_REFLEX:-}" ] && [ -n "${UX_DISC_FOLDER:-}" ]; then "$PY" -I -B "$HERE/ref_zlib.py" "$S/ref/zlib.json" "$UX_REFLEX" "$UX_DISC_FOLDER" > "$S/ref/zlib.log"; fi
  if ref "$S/ref/fortdodge.json"; then "$PY" -I -B "$HERE/ref_fortdodge.py" "$S/ref/fortdodge.json" > "$S/ref/fortdodge.log"; fi
  if ref "$S/ref/pil.json"; then "$PY" -I -B "$HERE/ref_pil.py" "$S/ref" > "$S/ref/pil.log"; fi
  if ref "$S/ref/readers/common.json" || ref "$S/ref/readers/Lincoln_STADIUM.json"; then "$PY" -I -B "$HERE/ref_readers.py" "$S/ref" > "$S/ref/readers.log"; fi
else
  echo "no reference Python ($PY): using the digests already in $S/ref"
fi
RC=0
DOTNET_CLI_TELEMETRY_OPTOUT=1 "$DN" "$S/bin/foundation.dll" "$WHAT" --scratch "$S" --ref "$S/ref" || RC=$?
# the C core's reader on the stored resources the stock test wrote (needs a C compiler: MSVC on Windows, cc elsewhere)
if [ "$WHAT" = all ] || [ "$WHAT" = stock ]; then
  if [ -f "$S/stored_blobs.bin" ]; then
    if out=$(sh "$HERE/core_stored_test.sh" "$S" 2>&1) && printf '%s\n' "$out" | grep -q ", 0 failed"; then echo "ok   $(printf '%s\n' "$out" | grep 'core stored resources')"
    else echo "FAIL core stored resources"; printf '%s\n' "$out" | tail -5 | sed 's/^/     /'; RC=1; fi
  else echo "SKIP core stored resources (no $S/stored_blobs.bin: the stock test needs UX_REFLEX)"; [ "$RC" = 0 ] && RC=3; fi
fi
exit $RC
