#!/bin/bash
# build_plugin_windows.sh [<out dir>] -- reflexworld.dll, the Windows x64 libreflexworld (MinGW-w64; notes: _re/notes/client-windows.md).
#
# The same sources as native/drafts/world_runtime/build.sh's plugin (plugin_api.cpp crt_extract.cpp lzxd.c + build.sh's SRC: the
# world runtime, native/game/port.cpp and every native/game/ports/*.cpp present now), compiled in parallel and incrementally into
# <out dir>/obj, linked against the STATIC MinGW capstone 5.0.9 / zlib 1.3.2 of scripts/windows/build_deps_windows.sh, the C / C++
# runtimes and winpthreads linked statically: the DLL imports only KERNEL32 and the Universal CRT (api-ms-win-crt-*, part of
# Windows 10+; Homebrew's MinGW targets UCRT); dbghelp is loaded at run time by the crash handler. The JIT is off: jit.cpp builds its interpreter-only stub off AArch64 macOS. The portability shim
# scripts/windows/win_shim.h is force-included and scripts/windows/win_io.c (binary-mode open) linked in (no source is edited).
#
# Writes only into <out dir> (default REFLEX/builds/windows/plugin): never native/drafts/world_runtime, never the Unity project.
#
#   <out>/reflexworld.dll          the plugin (debug info stripped; .gnu_debuglink -> reflexworld.dll.debug)
#   <out>/reflexworld.dll.debug    its DWARF, -g1: functions + lines (addr2line on the crash report's offsets)
#   <out>/exports.txt              the exported names, sorted
#   <out>/reflex_boot.exe          build.sh's diagnostics CLI for Windows (the verify worlds' hashes on a Windows machine)
#
# Environment:
#   REFLEX_WIN_DEPS=<dir>          capstone / zlib (<dir>/include, <dir>/lib); default <out>/deps (build_deps_windows.sh <cs> <z> <out>)
#   REFLEX_PORT_FILES="a.cpp b*.cpp"  only these files of native/game/ports (as native/game/tools/build_cli.sh)
#   REFLEX_PORT_EXCLUDE="x.cpp"    leave these port files out (a file in progress that does not compile yet)
#   JOBS=<n>                        parallel compiles (default 8)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT="${1:-$ROOT/builds/windows/plugin}"; mkdir -p "$OUT/obj"; OUT="$(cd "$OUT" && pwd)"
DEPS="${REFLEX_WIN_DEPS:-$OUT/deps}"; [ -d "$DEPS" ] && DEPS="$(cd "$DEPS" && pwd)"
CXX=x86_64-w64-mingw32-g++; CC=x86_64-w64-mingw32-gcc; OBJCOPY=x86_64-w64-mingw32-objcopy; OBJDUMP=x86_64-w64-mingw32-objdump
command -v $CXX >/dev/null || { echo "$CXX not installed (Homebrew mingw-w64)"; exit 2; }
for f in include/capstone/capstone.h include/zlib.h lib/libcapstone.a lib/libz.a; do
	[ -f "$DEPS/$f" ] || { echo "missing $DEPS/$f: build the deps first (scripts/windows/build_deps_windows.sh <capstone src> <zlib src> $OUT)"; exit 2; }
done
SHIM="$ROOT/scripts/windows/win_shim.h"
cd "$ROOT/native/drafts/world_runtime"
O="$OUT/obj"
# build.sh's flags minus the Apple ones (-mcpu=apple-m1 -arch arm64); x86-64 baseline (SSE2: no FMA, no x87 for float / double);
# -g1 (functions + line tables: addr2line on a crash offset) instead of -g, whose full DWARF is ~525 MB here
FLAGS=(-std=c++17 -O3 -g1 -Wall -Wextra -Wno-unused-parameter -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -I. -I../../game -I"$DEPS/include"
       -include "$SHIM" -D_WIN32_WINNT=0x0602)
# a change of compiler or flags rebuilds everything
stamp="$($CXX --version | head -1) ${FLAGS[*]}"
[ "$(cat "$O/flags.txt" 2>/dev/null)" = "$stamp" ] || { rm -f "$O"/*.o; echo "$stamp" >"$O/flags.txt"; }
eval "$(grep "^SRC=" build.sh)"
if [ -n "${REFLEX_PORT_FILES:-}" ] || [ -n "${REFLEX_PORT_EXCLUDE:-}" ]; then
	KEEP=(); PORTS=()
	for f in "${SRC[@]}"; do case "$f" in ../../game/ports/*) PORTS+=("$f");; *) KEEP+=("$f");; esac; done
	if [ -n "${REFLEX_PORT_FILES:-}" ]; then
		PORTS=(); for g in $REFLEX_PORT_FILES; do for f in ../../game/ports/$g; do [ -f "$f" ] && PORTS+=("$f"); done; done
	fi
	for f in "${PORTS[@]}"; do
		skip=0; for g in ${REFLEX_PORT_EXCLUDE:-}; do case "$(basename "$f")" in $g) skip=1;; esac; done
		[ $skip = 1 ] && echo "left out: $f" || KEEP+=("$f")
	done
	SRC=("${KEEP[@]}")
fi
dep=$(ls -t *.h *.inc f80/*.h ../../game/*.h ../../game/*.inc ../../game/ports/*.h "$SHIM" 2>/dev/null | head -1 || true)
objs=(); cli=(); : > "$OUT/failed.txt"
for f in plugin_api.cpp crt_extract.cpp boot_main.cpp "${SRC[@]}"; do
	o=$O/$(echo "$f" | tr '/.' '__').o
	case "$f" in plugin_api.cpp|crt_extract.cpp) objs+=("$o");; boot_main.cpp) cli+=("$o");; *) objs+=("$o"); cli+=("$o");; esac
	if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ "$dep" -nt "$o" ]; then
		while [ "$(jobs -r | wc -l)" -ge "${JOBS:-8}" ]; do sleep 0.2; done
		( $CXX "${FLAGS[@]}" -c -o "$o.tmp" "$f" >"$o.log" 2>&1 && mv -f "$o.tmp" "$o" || { rm -f "$o.tmp"; echo "$f" >>"$OUT/failed.txt"; } ) &
	fi
done
wait
if [ -s "$OUT/failed.txt" ]; then
	while read -r f; do o=$O/$(echo "$f" | tr '/.' '__').o; echo "FAILED $f"; grep -E "error" "$o.log" | head -5 | sed 's/^/  /'; done <"$OUT/failed.txt"
	echo "compile failed ($(wc -l <"$OUT/failed.txt" | tr -d ' ') files; logs: $O/*.o.log). A port in progress: REFLEX_PORT_EXCLUDE=\"<file>\""
	exit 1
fi
[ -f "$O/lzxd.o" ] && [ "$O/lzxd.o" -nt lib/libmspack/lzxd.c ] || $CC -O2 -c -o "$O/lzxd.o" lib/libmspack/lzxd.c -Ilib/libmspack
# win_io.c: open() in binary mode (the CRT's default is text mode), linked into the DLL and the CLI
WIO="$ROOT/scripts/windows/win_io.c"
[ -f "$O/win_io.o" ] && [ "$O/win_io.o" -nt "$WIO" ] || $CC -O2 -Wall -c -o "$O/win_io.o" "$WIO"
objs+=("$O/win_io.o"); cli+=("$O/win_io.o")
# Only the REFLEX_API (__declspec(dllexport)) functions are exported (as -fvisibility=hidden on macOS; the dylib also exports
# lzxd.c's five lzxd_* functions, which nothing outside the plugin calls). -static: libstdc++, libgcc and winpthreads inside the DLL.
$CXX -shared -o "$OUT/reflexworld.dll.tmp" "${objs[@]}" "$O/lzxd.o" -L"$DEPS/lib" -lcapstone -lz \
	-static -static-libgcc -static-libstdc++ -Wl,--disable-auto-import \
	-Wl,--out-implib,"$OUT/libreflexworld.dll.a" 2>"$OUT/link.txt" \
	|| { grep -oE "undefined reference to \`[^']+'" "$OUT/link.txt" | sort -u | head -20; tail -5 "$OUT/link.txt"; echo "LINK FAILED ($OUT/link.txt)"; exit 1; }
$OBJCOPY --only-keep-debug "$OUT/reflexworld.dll.tmp" "$OUT/reflexworld.dll.debug"
$OBJCOPY --strip-debug --add-gnu-debuglink="$OUT/reflexworld.dll.debug" "$OUT/reflexworld.dll.tmp"
mv -f "$OUT/reflexworld.dll.tmp" "$OUT/reflexworld.dll"
# reflex_boot.exe: build.sh's diagnostics CLI (boot_main.cpp + SRC), for the verify worlds' state / memory hashes on Windows
$CXX -o "$OUT/reflex_boot.exe.tmp" "${cli[@]}" -L"$DEPS/lib" -lcapstone -lz -static -static-libgcc -static-libstdc++ 2>"$OUT/link-cli.txt" \
	|| { tail -5 "$OUT/link-cli.txt"; echo "CLI LINK FAILED ($OUT/link-cli.txt)"; exit 1; }
x86_64-w64-mingw32-strip --strip-debug "$OUT/reflex_boot.exe.tmp"; mv -f "$OUT/reflex_boot.exe.tmp" "$OUT/reflex_boot.exe"
# the export table, and the DLLs it imports (must be Windows system DLLs only)
$OBJDUMP -p "$OUT/reflexworld.dll" | awk '/\[Ordinal\/Name Pointer\] Table/{t=1;next} t&&/^[[:space:]]*\[/{print $NF;next} t&&/^[[:space:]]*$/{t=0}' | sort >"$OUT/exports.txt"
imports=$($OBJDUMP -p "$OUT/reflexworld.dll" | sed -n 's/^[[:space:]]*DLL Name: //p' | sort -u | tr '\n' ' ')
echo "built $OUT/reflex_boot.exe ($(du -h "$OUT/reflex_boot.exe" | cut -f1 | tr -d ' '))"
echo "built $OUT/reflexworld.dll ($(du -h "$OUT/reflexworld.dll" | cut -f1 | tr -d ' '), $(wc -l <"$OUT/exports.txt" | tr -d ' ') exports, ${#objs[@]} sources; imports: $imports)"
