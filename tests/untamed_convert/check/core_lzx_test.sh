#!/bin/sh
# tests/untamed_convert/check/core_lzx_test.sh <scratch dir> -- the C core's package resource reader (core/src/fmt_lzx.c
# fmt_xmem_resource) on the resources the track builder writes (UxLzxEncoder: LZX chunks, stored chunks). Input:
# <scratch>/coreblobs/<short>.bin, written by "run.sh <scratch> build --core-blobs" (per compressed resource: this build's stored bytes
# and what the client's resource reader decodes them to), and _cases.bin (the 616 synthetic blobs). Builds core_stored_test.c with the core's own sources (MSVC cl through
# vcvars64.bat on Windows, cc elsewhere) and runs it on every file; exit 0 = every resource decodes to the same bytes in the C core.
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../../.." && pwd); S=${1:?scratch dir}
ls "$S"/coreblobs/*.bin > /dev/null 2>&1 || { echo "no $S/coreblobs/*.bin (run: run.sh <scratch> build --core-blobs)" >&2; exit 3; }
mkdir -p "$S/core_lzx"
SRC="$HERE/core_stored_test.c"
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*)
  VC=${RX_VCVARS:-"C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat"}
  cat > "$S/core_lzx/build.bat" <<BAT
@echo off
call "$(cygpath -w "$VC")" >nul
cd /d "$(cygpath -w "$S/core_lzx")"
cl /nologo /O2 /W3 /std:c11 /I"$(cygpath -w "$ROOT/core/src")" /I"$(cygpath -w "$ROOT/core/include")" /Fe:core_lzx_test.exe "$(cygpath -w "$SRC")" "$(cygpath -w "$ROOT/core/src/fmt_lzx.c")" "$(cygpath -w "$ROOT/core/src/fmt_common.c")"
BAT
  cmd //c "$(cygpath -w "$S/core_lzx/build.bat")" > "$S/core_lzx/build.log" 2>&1 || { cat "$S/core_lzx/build.log"; exit 1; }
  EXE="$S/core_lzx/core_lzx_test.exe"
  ;;
*)
  cc -std=c11 -O2 -I"$ROOT/core/src" -I"$ROOT/core/include" -o "$S/core_lzx/core_lzx_test" "$SRC" "$ROOT/core/src/fmt_lzx.c" "$ROOT/core/src/fmt_common.c"
  EXE="$S/core_lzx/core_lzx_test"
  ;;
esac
rc=0
for f in "$S"/coreblobs/*.bin; do
  printf '%-14s ' "$(basename "$f" .bin)"
  "$EXE" "$f" || rc=1
done
exit $rc
