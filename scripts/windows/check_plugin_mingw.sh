#!/bin/bash
# Windows x64 FEASIBILITY check of libreflexworld (_re/notes/client-platform.md, "Windows"): compiles every source of build.sh with
# MinGW-w64 (x86_64-w64-mingw32-g++, the toolchain of Open Unleashed's native/cmake/windows-mingw.cmake), the JIT off (jit.cpp builds
# its interpreter-only stub off AArch64 macOS), then tries the DLL link and lists what is unresolved. Never installs anything.
#
#   scripts/windows/check_plugin_mingw.sh [out dir]      (default: a temp dir)
#
# win_shim.h (force-included) stands in for the POSIX calls MinGW lacks (mkdir with a mode, pread / pwrite) and the standard
# headers some sources only get transitively on macOS. zlib.h comes from the macOS SDK (header only) because MinGW has none here:
# the link then needs MinGW builds of zlib and capstone (not vendored: their sources would have to be downloaded).
set -uo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
W="$ROOT/native/drafts/world_runtime"
OUT="${1:-$(mktemp -d)}"; mkdir -p "$OUT/obj" "$OUT/err" "$OUT/zinc"
command -v x86_64-w64-mingw32-g++ >/dev/null || { echo "x86_64-w64-mingw32-g++ not installed (Homebrew mingw-w64)"; exit 2; }
SDK="$(xcrun --show-sdk-path)"; cp "$SDK/usr/include/zlib.h" "$SDK/usr/include/zconf.h" "$OUT/zinc/"
CS="$ROOT/_re/.venv/lib/python3.14/site-packages/capstone/include"
cd "$W"
FLAGS=(-std=c++17 -O2 -fno-fast-math -ffp-contract=off -I. -I../../game -I"$CS" -I"$OUT/zinc" -include "$ROOT/scripts/windows/win_shim.h")
eval "$(grep "^SRC=" build.sh)"
: > "$OUT/status.txt"
for f in plugin_api.cpp crt_extract.cpp "${SRC[@]}"; do
	b=$(echo "$f" | tr '/.' '__')
	while [ "$(jobs -r | wc -l)" -ge 8 ]; do sleep 0.2; done
	(x86_64-w64-mingw32-g++ "${FLAGS[@]}" -c -o "$OUT/obj/$b.o" "$f" >"$OUT/err/$b.txt" 2>&1; echo "$? $f" >>"$OUT/status.txt") &
done
wait
x86_64-w64-mingw32-gcc -O2 -c -o "$OUT/obj/lzxd.o" lib/libmspack/lzxd.c -Ilib/libmspack >"$OUT/err/lzxd.txt" 2>&1; echo "$? lzxd.c" >>"$OUT/status.txt"
echo "compiled: $(grep -c '^0 ' "$OUT/status.txt") ok, $(grep -vc '^0 ' "$OUT/status.txt") failed"
grep -v '^0 ' "$OUT/status.txt" | sed 's/^/  FAILED /'
x86_64-w64-mingw32-g++ -shared -o "$OUT/reflexworld.dll" "$OUT"/obj/*.o -static-libgcc -static-libstdc++ -Wl,-Bstatic -lpthread -Wl,-Bdynamic 2>"$OUT/link.txt" \
	&& echo "linked: $OUT/reflexworld.dll" \
	|| { echo "link: unresolved symbols:"; grep -oE "undefined reference to \`[^']+'" "$OUT/link.txt" | sort -u | sed "s/undefined reference to /  /"; }
echo "output: $OUT"
