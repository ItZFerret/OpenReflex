#!/bin/bash
# build_plugin_parallel.sh <out dir> -- libreflexworld.dylib with build.sh's flags and sources, compiled in parallel and incrementally
# (objects in <out dir>/obj): ~40 s from clean on 10 cores (build.sh compiles every source serially, three times: ~10 min for its plugin, ~14 min in all here). It writes only into
# <out dir> (never native/drafts/world_runtime, never the Unity project). Not a replacement for build.sh / verify_install.sh.
set -euo pipefail
OUT="${1:?usage: build_plugin_parallel.sh <out dir>}"; mkdir -p "$OUT/obj"; OUT="$(cd "$OUT" && pwd)"
cd "$(dirname "$0")/../native/drafts/world_runtime"
O="$OUT/obj"
CS=../../../_re/.venv/lib/python3.14/site-packages/capstone
FLAGS=(-std=c++17 -O3 -mcpu=apple-m1 -Wall -Wextra -fno-fast-math -ffp-contract=off -fwrapv -fno-strict-aliasing -arch arm64 -I. -I../../game -I$CS/include -fvisibility=hidden)
eval "$(grep "^SRC=" build.sh)"
dep=$(ls -t *.h *.inc f80/*.h ../../game/*.h 2>/dev/null | head -1)
objs=(); fail=0
for f in plugin_api.cpp crt_extract.cpp "${SRC[@]}"; do
  o=$O/$(echo "$f" | tr '/.' '__').o; objs+=("$o")
  if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ "$dep" -nt "$o" ]; then
    while [ "$(jobs -r | wc -l)" -ge "${JOBS:-8}" ]; do sleep 0.2; done
    (clang++ "${FLAGS[@]}" -c -o "$o" "$f" 2>&1 | grep -E " error" || true) &
  fi
done
wait
[ -f "$O/lzxd.o" ] || clang -O2 -arch arm64 -c -o "$O/lzxd.o" lib/libmspack/lzxd.c -Ilib/libmspack
clang++ -arch arm64 -dynamiclib -install_name @rpath/libreflexworld.dylib -o "$OUT/libreflexworld.dylib" "${objs[@]}" "$O/lzxd.o" \
  -Llib -lcapstone.5 -lz -Wl,-rpath,@loader_path -Wl,-rpath,@loader_path/lib && echo "built $OUT/libreflexworld.dylib"
