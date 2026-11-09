#!/bin/bash
# build.sh [out dir (build)] -- the OPEN REFLEX core library librxcore.dylib (product build: flat arena, no emulator) and its tests.
# Same flags as the oracle build in native/game/tools/build_cli.sh, without RX_ORACLE. Windows x64: core/tools/build_windows.sh (MinGW-w64 cross build).
# RX_BUILD_JOBS bounds compiler concurrency (default 2), including clean builds.
set -e
JOBS=${RX_BUILD_JOBS:-2}
case "$JOBS" in ''|*[!0-9]*|0) echo "RX_BUILD_JOBS must be a positive integer" >&2; exit 2;; esac
wait_batch() { for p in "${pids[@]}"; do wait "$p"; done; pids=(); }
limit_jobs() { if [ "${#pids[@]}" -ge "$JOBS" ]; then wait_batch; fi; }
cd "$(dirname "$0")"; source tools/native_tu_flags.sh; O=${1:-build}; mkdir -p "$O/obj"
# one core function per original address (rx_fn aborts at start otherwise)
dup=$(grep -ho "RX_FUNCTION(0x[0-9a-fA-F]*" game/*.c 2>/dev/null | sed 's/.*(0x//' | tr 'A-F' 'a-f' | sort | uniq -d)
[ -n "$dup" ] && { for a in $dup; do echo "duplicate RX_FUNCTION 0x$a: $(grep -li "RX_FUNCTION(0x$a" game/*.c | tr '\n' ' ')"; done; exit 1; }
CFLAGS=(-std=c11 -O2 -mcpu=apple-m1 -Wall -Wextra -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 -fvisibility=hidden -fPIC -Iinclude -Isrc -Igame)
if [ -n "${RX_BUILD_EXTRA_CFLAGS:-}" ]; then CFLAGS+=($RX_BUILD_EXTRA_CFLAGS); fi   # w-vtlog: e.g. "-DRX_FN_TRACE -g -fno-omit-frame-pointer" for a traced build in its own out dir (tools/vtrace_build.sh); unset in every normal build
OBJS=(); pids=()
mkdir -p "$O/gen"; python3 tools/gen_fnsig.py "$O/gen/rx_fnsig.c" --strict   # the product's integer-signature table of the registered functions (src/rx_mem.c)
for f in src/*.c game/*.c api/*.c "$O/gen/rx_fnsig.c"; do
  [ -f "$f" ] || continue
  o="$O/obj/$(echo "$f" | tr '/.' '__').o"; OBJS+=("$o")
  dep=$(ls -t "$f" build.sh tools/native_tu_flags.sh tools/native_tu_flags.tsv include/*.h src/*.h src/*.inc data/*.inc game/*.h api/*.h api/*.inc 2>/dev/null | head -1)
  if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ "$dep" -nt "$o" ]; then rx_native_tu_flags "$f"; clang "${CFLAGS[@]}" "${RX_NATIVE_TU_FLAGS[@]}" -c -o "$o" "$f" & pids+=($!); limit_jobs; fi
done
CXXFLAGS=(-std=c++17 -O2 -mcpu=apple-m1 -Wall -Wextra -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 -fvisibility=hidden -fPIC -Iinclude -Isrc)
for f in src/*.cpp src/f80/*.cpp; do
  [ -f "$f" ] || continue
  o="$O/obj/$(echo "$f" | tr '/.' '__').o"; OBJS+=("$o")
  if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ build.sh -nt "$o" ]; then clang++ "${CXXFLAGS[@]}" -c -o "$o" "$f" & pids+=($!); limit_jobs; fi
done
wait_batch
LIB_TMP="$O/.librxcore.$$.dylib"
trap 'rm -f "${LIB_TMP:?}"' EXIT
clang++ -arch arm64 -dynamiclib -install_name "$O/librxcore.dylib" -o "$LIB_TMP" "${OBJS[@]}" -lz
mv -f "$LIB_TMP" "$O/librxcore.dylib"
printf '%s\n' "${OBJS[@]}" > "$O/objs.list"   # the link set (tools/product_link.sh links test drivers against exactly these; obj/ may hold stale objects)
echo "built $O/librxcore.dylib"
