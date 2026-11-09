#!/bin/bash
# Current native product core, Windows x64; no emulator/oracle sources.
# RX_BUILD_JOBS=1 core/tools/build_windows.sh <out> [Windows zlib prefix]
# Compiler: Homebrew LLVM clang for x86_64-w64-mingw32 against the MinGW-w64 headers and libraries of the cross GCC, linked by the
# cross GCC's driver (GNU ld). It is the Mac build's compiler: native TLS (no emulated TLS / winpthreads), and about 1.4x faster
# than GCC's code on the same race with bit-identical results (w-winclang, core/plan/QUEUE.md). RX_WIN_CC=gcc builds with the
# cross GCC instead (the previous toolchain, kept as a fallback); RX_WIN_LLVM=<dir> names another LLVM bin directory.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/build-windows}"; mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
DEPS="${2:-$ROOT/../builds/windows/plugin/deps}"
GCC=x86_64-w64-mingw32-gcc; GXX=x86_64-w64-mingw32-g++
COPY=x86_64-w64-mingw32-objcopy; DUMP=x86_64-w64-mingw32-objdump
JOBS=${RX_BUILD_JOBS:-1}
case "$JOBS" in ''|*[!0-9]*|0) echo 'RX_BUILD_JOBS must be positive' >&2;exit 2;; esac
for tool in "$GCC" "$GXX" "$COPY" "$DUMP" python3;do command -v "$tool" >/dev/null || { echo "missing $tool" >&2;exit 2; };done
TOOLCHAIN=${RX_WIN_CC:-clang}
case "$TOOLCHAIN" in
gcc) CC=$GCC; CXX=$GXX;;
clang)
 LLVM=${RX_WIN_LLVM:-$(brew --prefix llvm 2>/dev/null || echo /opt/homebrew/opt/llvm)/bin}
 [ -x "$LLVM/clang" ] && [ -x "$LLVM/clang++" ] || { echo "missing LLVM clang in $LLVM (brew install llvm, or RX_WIN_CC=gcc)" >&2;exit 2; }
 # the MinGW-w64 headers of the cross GCC (C, and its libstdc++ for the C++ files); clang's own intrinsic headers
 GCCLIB="$(dirname "$("$GCC" -print-libgcc-file-name)")"; GCCVER="$("$GCC" -dumpversion)"
 MINC="$(cd "$GCCLIB/../../../../x86_64-w64-mingw32/include" && pwd)"; RINC="$("$LLVM/clang" -print-resource-dir)/include"
 [ -f "$MINC/windows.h" ] && [ -d "$MINC/c++/$GCCVER" ] && [ -f "$RINC/emmintrin.h" ] || { echo "MinGW-w64 / clang headers not found ($MINC, $RINC)" >&2;exit 2; }
 # wrappers (gen_fnsig.py and the stamp take one executable)
 mkdir -p "$OUT/toolchain"
 printf '#!/bin/sh\nexec "%s" --target=x86_64-w64-mingw32 -nostdinc -isystem "%s" -isystem "%s" "$@"\n' "$LLVM/clang" "$RINC" "$MINC" >"$OUT/toolchain/cc"
 printf '#!/bin/sh\nexec "%s" --target=x86_64-w64-mingw32 -nostdinc -nostdinc++ -isystem "%s" -isystem "%s" -isystem "%s" -isystem "%s" -isystem "%s" "$@"\n' \
  "$LLVM/clang++" "$MINC/c++/$GCCVER" "$MINC/c++/$GCCVER/x86_64-w64-mingw32" "$MINC/c++/$GCCVER/backward" "$RINC" "$MINC" >"$OUT/toolchain/cxx"
 chmod +x "$OUT/toolchain/cc" "$OUT/toolchain/cxx"
 CC="$OUT/toolchain/cc"; CXX="$OUT/toolchain/cxx";;
*) echo "RX_WIN_CC must be clang or gcc" >&2;exit 2;;
esac
for file in include/zlib.h lib/libz.a;do [ -f "$DEPS/$file" ] || { echo "missing Windows dependency $DEPS/$file" >&2;exit 2; };done
DEPS="$(cd "$DEPS" && pwd)"
cd "$ROOT";source tools/native_tu_flags.sh;mkdir -p "$OUT/obj" "$OUT/gen"
RX_FNSIG_CC="$CC" python3 tools/gen_fnsig.py "$OUT/gen/rx_fnsig.c" --strict
FLAGS=(-fdata-sections -O2 -g1 -Wall -Wextra -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -D_WIN32_WINNT=0x0602 -Iinclude -Isrc -Igame -I"$DEPS/include")
# clang: no warning flood from the comment style the sources use; the same exactness flags as the Mac build (-ffp-contract=off,
# -fno-fast-math; clang's x86 SSE math has no excess precision)
[ "$TOOLCHAIN" = clang ] && FLAGS+=(-Wno-comment)
# The C++ files (the F80 library) need nothing from libstdc++ at run time: no exceptions, no RTTI, no thread-safe statics (w-winclean).
CXX_ONLY=(-fno-exceptions -fno-rtti -fno-threadsafe-statics)
# Imports that winpthreads (or similar) would bring in and the core never calls (process-injection look-alikes to antivirus
# heuristics); the build fails if any comes back (w-winclean).
FORBIDDEN_IMPORTS=(SuspendThread GetThreadContext SetThreadContext ResumeThread OpenProcess DuplicateHandle SetSystemTime SetProcessAffinityMask GetProcessTimes)
# Rename only core writable statics; runtime library sections keep their own data.
# .rxkeep and TLS are deliberately excluded from session reset.
compile(){
 local source=$1 object=$2 compiler=$3 standard=$4 extra=()
 rx_native_tu_flags "$source"
 [ "$compiler" = "$CXX" ] && extra=("${CXX_ONLY[@]}")
 if ! "$compiler" "$standard" "${FLAGS[@]}" ${extra[@]+"${extra[@]}"} ${RX_NATIVE_TU_FLAGS[@]+"${RX_NATIVE_TU_FLAGS[@]}"} -c "$source" -o "$object.raw" >"$object.log" 2>&1;then
  echo "FAILED $source ($object.log)" >&2;tail -30 "$object.log" >&2;return 1
 fi
 python3 tools/windows_core_sections.py "$object.raw" "$object"
 rm -f "$object.raw"
}
stamp="$TOOLCHAIN $($CC --version | head -1) $($CXX --version | head -1) ${FLAGS[*]} ${CXX_ONLY[*]} $(shasum "$ROOT/tools/native_tu_flags.tsv" | cut -d' ' -f1) $(shasum "$ROOT/tools/native_tu_flags.sh" | cut -d' ' -f1) $(shasum "$ROOT/tools/windows_core_sections.py" | cut -d' ' -f1)"
if [ "$(cat "$OUT/obj/flags.txt" 2>/dev/null || true)" != "$stamp" ];then
 rm -f "$OUT/obj/"*.o
 echo "$stamp" >"$OUT/obj/flags.txt"
fi
# One newest-header scan covers transitive hand-written headers and embedded data.
dep=$(python3 - <<'PYDEP'
import pathlib
files=[p for d in ('include','src','game','api','data') for p in pathlib.Path(d).rglob('*') if p.suffix in ('.h','.inc')]
print(max(files,key=lambda p:p.stat().st_mtime))
PYDEP
)
objects=();pids=();pid_count=0
wait_batch(){ local failed=0; if [ "$pid_count" -eq 0 ];then return 0;fi; for pid in "${pids[@]}";do wait "$pid" || failed=1;done;pids=();pid_count=0;[ "$failed" = 0 ]; }
# src/win: the pthread calls GCC's emulated TLS needs, without winpthreads (w-winclean); clang's TLS is native and needs none.
win_sources=(); [ "$TOOLCHAIN" = gcc ] && win_sources=(src/win/*.c)
for source in src/*.c ${win_sources[@]+"${win_sources[@]}"} game/*.c api/*.c "$OUT/gen/rx_fnsig.c" src/*.cpp src/f80/*.cpp;do
 [ -f "$source" ] || continue
 object="$OUT/obj/$(echo "$source" | tr '/.' '__').o";objects+=("$object")
 case "$source" in *.cpp) compiler=$CXX;standard=-std=c++17;;*)compiler=$CC;standard=-std=c11;;esac
 if [ ! -f "$object" ] || [ "$source" -nt "$object" ] || [ "$dep" -nt "$object" ];then
  compile "$source" "$object" "$compiler" "$standard" & pids+=($!);pid_count=$((pid_count+1))
 fi
 if [ "$pid_count" -ge "$JOBS" ];then wait_batch;fi
done
wait_batch
# Linked in a directory of its own under the final name (the DLL's export directory records the output name: it read
# "rxcore.dll.tmp" before), by the cross GCC driver (GNU ld, MinGW-w64 CRT and libgcc). Fixed image base, no timestamp (w-winclean).
rm -rf "$OUT/link"; mkdir -p "$OUT/link"
if ! "$GXX" -shared -o "$OUT/link/rxcore.dll" "${objects[@]}" "$DEPS/lib/libz.a" -static -static-libgcc -static-libstdc++ -Wl,--exclude-all-symbols -Wl,--out-implib,"$OUT/librxcore.dll.a" -Wl,-Map,"$OUT/rxcore.map" -Wl,--image-base=0x180000000 -Wl,--no-insert-timestamp >"$OUT/link.log" 2>&1;then
 echo "DLL link failed ($OUT/link.log)" >&2;tail -35 "$OUT/link.log" >&2;exit 1
fi
# No winpthreads member may be linked (the archive is libpthread.a / libwinpthread.a; its members are libwinpthread_la-*.o), and no
# emulated-TLS runtime (clang's TLS is native; GCC's would pull libgcc's emutls.o and winpthreads with it).
if grep -E '(libwinpthread|libpthread)[^ ]*\.a\(' "$OUT/rxcore.map" | head -5 | grep .;then echo 'winpthreads linked into rxcore.dll (see rxcore.map)' >&2;exit 1;fi
if [ "$TOOLCHAIN" = clang ] && grep -E 'emutls' "$OUT/rxcore.map" | head -3 | grep .;then echo 'emulated TLS linked into the clang rxcore.dll (see rxcore.map)' >&2;exit 1;fi
# The shipped DLL carries no debug info or symbol table (-g1 left ~16 MB of DWARF in numbered sections, which reads as packed to
# antivirus heuristics); the debug info goes to rxcore.dbg next to it, linked by .gnu_debuglink, for crash symbols (w-winclean).
"$COPY" --only-keep-debug "$OUT/link/rxcore.dll" "$OUT/rxcore.dbg"
"$COPY" --strip-all --add-gnu-debuglink="$OUT/rxcore.dbg" "$OUT/link/rxcore.dll"
# objcopy stamps the PE header with the current time; put back --no-insert-timestamp's zero (the COFF header's TimeDateStamp)
python3 - "$OUT/link/rxcore.dll" <<'PYTS'
import struct,sys
p=sys.argv[1];b=bytearray(open(p,'rb').read());pe=struct.unpack_from('<I',b,0x3c)[0]
assert b[pe:pe+4]==b'PE\0\0';struct.pack_into('<I',b,pe+8,0);open(p,'wb').write(b)
PYTS
mv "$OUT/link/rxcore.dll" "$OUT/rxcore.dll"; rmdir "$OUT/link"
"$DUMP" -p "$OUT/rxcore.dll" >"$OUT/pe.txt"
"$DUMP" -h "$OUT/rxcore.dll" >"$OUT/sections.txt"
awk '/DLL Name:/{print $3}' "$OUT/pe.txt" >"$OUT/imports.txt"
awk '/\[Ordinal\/Name Pointer\] Table/{on=1;next}on&&/^[[:space:]]*\[/{print $NF;next}on&&/^[[:space:]]*$/{on=0}' "$OUT/pe.txt" | sort >"$OUT/exports.txt"
# API dllexport remains explicit even with auto-export disabled.
grep -qx rx_session_create "$OUT/exports.txt"
grep -q '^Name .*rxcore\.dll$' "$OUT/pe.txt" || { echo 'rxcore.dll export directory does not name rxcore.dll' >&2;exit 1; }
grep -q '\.rxdata' "$OUT/sections.txt" # .rxbss is optional (MinGW may put zeros in .data$*)
if grep -Ei '(libgcc|libstdc|winpthread|zlib|reflexworld|capstone)' "$OUT/imports.txt";then echo 'unexpected non-system dependency' >&2;exit 1;fi
# every imported function, "DLL function" per line
awk '/DLL Name:/{d=$3;next} /vma:.*Member-Name/{on=1;next} on&&NF==0{on=0} on&&NF>=4{print d, $4}' "$OUT/pe.txt" | sort >"$OUT/import_functions.txt"
[ -s "$OUT/import_functions.txt" ] || { echo 'could not read the import functions (pe.txt format?)' >&2;exit 1; }
for name in "${FORBIDDEN_IMPORTS[@]}";do
 if awk -v n="$name" '$2==n{found=1}END{exit !found}' "$OUT/import_functions.txt";then echo "forbidden import $name in rxcore.dll (winpthreads or similar is back)" >&2;exit 1;fi
done
echo "built $OUT/rxcore.dll ($TOOLCHAIN, native x64; inspect exports.txt/imports.txt/import_functions.txt)"
