#!/bin/sh
# tests/untamed_convert/foundation/core_stored_test.sh <scratch dir> -- builds core_stored_test.c with the core's own fmt_lzx.c / fmt_common.c
# and runs it on <scratch>/stored_blobs.bin (written by "foundation stock" from an original install): proof that the C core loads the
# stored (uncompressed-chunk) resources the Untamed converter writes.
#   Windows (Git Bash): MSVC cl.exe through vcvars64.bat (RX_VCVARS overrides the path);  other systems: cc
set -eu
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/../../.." && pwd); S=${1:?scratch dir}
[ -f "$S/stored_blobs.bin" ] || { echo "no $S/stored_blobs.bin (run: foundation stock --reflex <original install>)" >&2; exit 3; }
mkdir -p "$S/core_stored"
case "$(uname -s)" in
MINGW*|MSYS*|CYGWIN*)
  VC=${RX_VCVARS:-"C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat"}
  cat > "$S/core_stored/build.bat" <<BAT
@echo off
call "$(cygpath -w "$VC")" >nul
cd /d "$(cygpath -w "$S/core_stored")"
cl /nologo /O2 /W3 /std:c11 /I"$(cygpath -w "$ROOT/core/src")" /I"$(cygpath -w "$ROOT/core/include")" /Fe:core_stored_test.exe "$(cygpath -w "$HERE/core_stored_test.c")" "$(cygpath -w "$ROOT/core/src/fmt_lzx.c")" "$(cygpath -w "$ROOT/core/src/fmt_common.c")"
BAT
  cmd //c "$(cygpath -w "$S/core_stored/build.bat")" || exit 1
  "$S/core_stored/core_stored_test.exe" "$S/stored_blobs.bin"
  ;;
*)
  cc -std=c11 -O2 -I"$ROOT/core/src" -I"$ROOT/core/include" -o "$S/core_stored/core_stored_test" "$HERE/core_stored_test.c" "$ROOT/core/src/fmt_lzx.c" "$ROOT/core/src/fmt_common.c"
  "$S/core_stored/core_stored_test" "$S/stored_blobs.bin"
  ;;
esac
