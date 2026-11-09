#!/bin/bash
# build_deps_windows.sh <capstone src> <zlib src> <out dir>
#
# Builds the two C libraries libreflexworld links for Windows x64 as STATIC MinGW libraries, out of tree, with their own CMake
# build systems only (nothing inside the source trees is executed by hand): capstone 5.0.9 (x86 only: the plugin opens
# CS_ARCH_X86 / CS_MODE_32 alone) and zlib 1.3.2. Result: <out>/deps/include/{capstone/*.h,zlib.h,zconf.h} and
# <out>/deps/lib/{libcapstone.a,libz.a}. The sources are the release archives named in _re/notes/client-windows.md (checksums there).
set -euo pipefail
CS_SRC="$(cd "${1:?usage: build_deps_windows.sh <capstone src> <zlib src> <out dir>}" && pwd)"
Z_SRC="$(cd "${2:?zlib src}" && pwd)"
mkdir -p "${3:?out dir}"; OUT="$(cd "$3" && pwd)"
TC="$(cd "$(dirname "$0")" && pwd)/mingw-x64.cmake"
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "x86_64-w64-mingw32-gcc not installed (Homebrew mingw-w64)"; exit 2; }
P="$OUT/deps"; mkdir -p "$P"
GEN=(); command -v ninja >/dev/null && GEN=(-G Ninja)

cmake -S "$CS_SRC" -B "$OUT/capstone-build" "${GEN[@]}" -DCMAKE_TOOLCHAIN_FILE="$TC" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$P" -DBUILD_SHARED_LIBS=OFF -DBUILD_STATIC_LIBS=ON \
  -DCAPSTONE_BUILD_TESTS=OFF -DCAPSTONE_BUILD_CSTOOL=OFF -DCAPSTONE_BUILD_CSTEST=OFF -DCAPSTONE_INSTALL=ON \
  -DCAPSTONE_ARCHITECTURE_DEFAULT=OFF -DCAPSTONE_X86_SUPPORT=ON >"$OUT/capstone-configure.log"
cmake --build "$OUT/capstone-build" -j8 >"$OUT/capstone-build.log"
cmake --install "$OUT/capstone-build" >"$OUT/capstone-install.log"

cmake -S "$Z_SRC" -B "$OUT/zlib-build" "${GEN[@]}" -DCMAKE_TOOLCHAIN_FILE="$TC" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$P" -DZLIB_BUILD_SHARED=OFF -DZLIB_BUILD_STATIC=ON -DZLIB_BUILD_TESTING=OFF -DZLIB_INSTALL=ON >"$OUT/zlib-configure.log"
cmake --build "$OUT/zlib-build" -j8 >"$OUT/zlib-build.log"
cmake --install "$OUT/zlib-build" >"$OUT/zlib-install.log"

# zlib's static archive is named libzs.a by its CMake on MinGW in some versions: give the linker one name.
[ -f "$P/lib/libz.a" ] || { z=$(ls "$P"/lib/libz*.a | head -1); cp "$z" "$P/lib/libz.a"; }
ls -l "$P/lib"/*.a
echo "deps: $P"
